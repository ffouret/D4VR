#!/usr/bin/env python3
"""Run a bounded, create-only stereo pipeline diagnostic without changing game binaries."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import subprocess
import sys
import time

import camera_capture
import eye_control
import gpu_messages
import probe
import shader_capture
import vulkan_trace

OWNED_SCRIPT = r'''
import json,os,pathlib,signal,sys
marker=sys.argv[1];expected=json.loads(sys.argv[2]);found=[]
wanted={'diablo iv.exe','diablo iv launcher.exe','battle.net.exe','battle.net launcher.exe','agent.exe'}
for p in pathlib.Path('/proc').glob('[0-9]*'):
 try:
  names={a.decode(errors='replace').replace('\\','/').rsplit('/',1)[-1].casefold() for a in (p/'cmdline').read_bytes().split(b'\0')} & wanted
  if not names:continue
  env=(p/'environ').read_bytes().split(b'\0')
  if ('D4VR_TRACE_DIR='+marker).encode() not in env:continue
  start=(p/'stat').read_text().rsplit(') ',1)[1].split()[19]
  row={'pid':int(p.name),'start':start,'names':sorted(names)}
  if expected.get(p.name)==start:
   fd=os.pidfd_open(int(p.name))
   try:
    if (p/'stat').read_text().rsplit(') ',1)[1].split()[19]==start:
     signal.pidfd_send_signal(fd,signal.SIGTERM)
   finally:os.close(fd)
  found.append(row)
 except (OSError,ValueError,IndexError):pass
print(json.dumps(found))
'''


def host_command() -> list[str]:
    bridge=Path('/run/host/usr/bin/flatpak-spawn')
    return [str(bridge),'--host'] if bridge.exists() else []


def owned_processes(output: Path, stop: list[dict] | None = None) -> list[dict]:
    # Match both the private environment marker and the process start time.
    # Never terminate a global Wine server or an unrelated launcher session.
    expected={str(p['pid']):p['start'] for p in stop or []}
    result=subprocess.run(host_command()+['python3','-c',OWNED_SCRIPT,str(output.resolve()),json.dumps(expected)],
                          capture_output=True,text=True,timeout=15,check=True)
    return json.loads(result.stdout)


def validate_layer(directory: Path) -> dict:
    manifest=json.loads((directory/'trace-bundle.json').read_text(encoding='utf-8'))
    proof=manifest['validation']
    required=('paired_pipeline_fixture_passed','forwarding_tests_passed','observer_pixels_unchanged',
              'geometry_parallax_unchanged','loaded_trace_verified','steam_runtime_load_passed','frame_trace_fixture_passed')
    if (manifest.get('purpose')!='vulkan_trace_diagnostic' or proof.get('paired_pipeline_mode')!='create_only'
            or any(proof.get(k) is not True for k in required) or proof.get('layer_sha256')!=manifest['layer_sha256']):
        raise ValueError('A validated create-only paired pipeline layer is required')
    for name,key in ((vulkan_trace.LIBRARY,'layer_sha256'),(vulkan_trace.MANIFEST,'manifest_sha256')):
        if probe.digest(directory/name)!=manifest[key]:raise ValueError('Paired layer changed since GPU validation')
    layer=json.loads((directory/vulkan_trace.MANIFEST).read_text())['layer']
    if layer['name']!=vulkan_trace.LAYER or layer['library_path']!='./'+vulkan_trace.LIBRARY:
        raise ValueError('Unexpected layer dispatch manifest')
    return manifest


def launcher_source(source: str,proton: str,game: Path,output: Path,launch_game: bool) -> str:
    pattern=r'^export PROTONPATH="GE-Proton"$'
    if len(re.findall(pattern,source,re.M))!=1:raise ValueError('Expected the reviewed Lutris runtime pin')
    prefixes=re.findall(r'^export WINEPREFIX=(.+)$',source,re.M)
    if len(prefixes)!=1 or len(shlex.split(prefixes[0]))!=1:
        raise ValueError('An explicit Lutris Wine prefix is required')
    if (Path(shlex.split(prefixes[0])[0])/'drive_c').resolve()!=game.parents[1].resolve():
        raise ValueError('The Lutris Wine prefix does not contain this game')
    source=re.sub(pattern,lambda _: 'export PROTONPATH='+shlex.quote(proton),source,flags=re.M)
    lines=source.splitlines();commands=[i for i,line in enumerate(lines) if line.strip() and not line.lstrip().startswith('#')]
    last=commands[-1];command=shlex.split(lines[last])
    expected=['/usr/bin/umu-run',str(game.parent/'Battle.net'/'Battle.net Launcher.exe')]
    if command!=expected:raise ValueError('Expected the reviewed Battle.net launch command')
    if launch_game:
        # Lutris targets Battle.net.exe for --exec, rather than its bootstrap.
        command[1]=str(Path(command[1]).with_name('Battle.net.exe'))
        lines[last]=shlex.join(command+['--exec=launch Fen'])
    # These assignments belong only to the private launcher copy.
    cache=str(output/'vendor-cache')
    for name in ('__GL_SHADER_DISK_CACHE_PATH','DXVK_STATE_CACHE_PATH'):
        pattern=r'^export '+name+r'=.*$'
        lines=[re.sub(pattern,lambda _: 'export '+name+'='+shlex.quote(cache),line) for line in lines]
    return '\n'.join(lines)+'\n'


def prepare(game: Path,launcher: Path,shader_bundle: Path,layer_bundle: Path,output: Path,launch_game: bool=False) -> dict:
    if sys.platform!='linux':raise ValueError('The diagnostic game launcher currently requires Linux')
    layer=validate_layer(layer_bundle);bundle=camera_capture.validate_bundle(shader_bundle);expected=probe.profile()
    if (bundle.get('purpose')!='eye_transform_diagnostic' or bundle.get('eye_state_abi')!=1
            or bundle.get('eye_state_specialization_id')!=4 or bundle.get('stereo_telemetry') is not True
            or bundle.get('gpu_validation',{}).get('stereo_gpu_fixture_passed') is not True
            or bundle['game_version']!=expected['game_version']):
        raise ValueError('A matching validated stereo-transform shader bundle is required')
    game,output=game.resolve(),output.resolve()
    if output==game or game in output.parents or output.exists():raise ValueError('Use a new private directory outside the game')
    source=launcher_source(launcher.read_text(encoding='utf-8'),bundle['proton_path'],game,output,launch_game)
    audit=probe.check_target(game,expected)
    if shader_capture.launcher_processes():raise ValueError('Close Diablo and Battle.net before the isolated diagnostic')
    output.mkdir(parents=True,mode=0o700)
    capture=shader_capture.prepare(game,launcher,output,expected)
    for eye,name in enumerate(('left','right')):
        path=output/(name+'.bin');path.write_bytes(eye_control.encode(eye));path.chmod(0o600)
    (output/'vendor-cache').mkdir(mode=0o700)
    private_layer=output/'layer';private_layer.mkdir(mode=0o700)
    for name,key in ((vulkan_trace.LIBRARY,'layer_sha256'),(vulkan_trace.MANIFEST,'manifest_sha256')):
        shutil.copyfile(layer_bundle/name,private_layer/name)
        if probe.digest(private_layer/name)!=layer[key]:raise ValueError('Layer changed while copying the validated build')
    path=output/'battle-net-pairs.sh';path.write_text(source,encoding='utf-8');path.chmod(0o700)
    variables={'VK_INSTANCE_LAYERS':vulkan_trace.LAYER,'VK_ADD_LAYER_PATH':str(private_layer),
        'D4VR_TRACE_DIR':str(output),'D4VR_PAIR_LEFT_FILE':str(output/'left.bin'),'D4VR_PAIR_RIGHT_FILE':str(output/'right.bin'),
        'D4VR_FRAME_TRACE':'1','VKD3D_SHADER_OVERRIDE':probe.windows_path(shader_bundle.resolve()/'overrides'),
        'VKD3D_SHADER_CACHE_PATH':capture['variables']['VKD3D_SHADER_CACHE_PATH'],'VKD3D_SHADER_DEBUG_RING_SIZE_LOG2':'24',
        'VKD3D_SHADER_DEBUG':'info','VKD3D_DEBUG':'info','VKD3D_LOG_FILE':probe.windows_path(output/'pairs.log')}
    request=output/'frame.request';request.write_text('1\n');request.chmod(0o600)
    capture.update(phase='prepared',purpose='paired_pipeline_create_only_diagnostic',variables=variables,
        game_audit_before=audit,layer_sha256=layer['layer_sha256'],shader_bundle=str(shader_bundle.resolve()),
        shader_bundle_sha256=probe.digest(shader_bundle/'bundle.json'),override_count=len(bundle['entries']),
        private_launcher=str(path),launch_game_requested=launch_game,frame_trace_requested=True,
        original_eye_specialization_overridden=False,private_pipelines_drawn=False,game_stereo_submitted=False)
    probe.write_json(output/'capture.json',capture);return capture


def command(capture: dict) -> list[str]:
    result=host_command()+['/usr/bin/env']
    for key in ('VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','VK_LAYER_PATH','VKD3D_SHADER_DUMP_PATH','D4VR_EYE_STATE_FILE',
                'D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_FRAME_TRACE','D4VR_TRACE_XR_BINDING','D4VR_XR_LOADER',
                'VK_LOADER_DEBUG','VK_LAYER_ENABLES','VK_LAYER_VALIDATE_SYNC','D4VR_SEQUENTIAL_CAPTURE_DIR','D4VR_GPU_CAPTURE_DRY_RUN','D4VR_GPU_CAMERA_UPDATES',
                'VKD3D_SHADER_OVERRIDE','VKD3D_SHADER_DEBUG','D4VR_POSE_CAPTURE_DIAGNOSTIC','D4VR_COOPERATIVE_XR_DIAGNOSTIC',
                'D4VR_DIAGNOSTIC_VIEW_HOLD','D4VR_LAYER_REPLAY_POC','D4VR_DEPTH_READBACK_DIAGNOSTIC','D4VR_DEPTH_AUDIT',
                'D4VR_DEPTH_AUDIT_FIXTURE','D4VR_DEPTH_LAYER_FIXTURE','D4VR_DEPTH_TRANSFER_FIXTURE',
                'D4VR_DEPTH_SOURCE_HISTORY','D4VR_SOURCE_CONSUMER_HISTORY','D4VR_SOURCE_HISTORY_WIDTH','D4VR_SOURCE_HISTORY_HEIGHT','D4VR_SOURCE_HISTORY_FORMAT','D4VR_HELD_DEPTH_COLOR_DIAGNOSTIC', 'D4VR_DEPTH_COPY_READBACK_DIAGNOSTIC','D4VR_DEPTH_COPY_FIXTURE'):
        result+=['-u',key]
    return result+[k+'='+v for k,v in capture['variables'].items()]+['/usr/bin/bash',capture['private_launcher']]


def inactive_telemetry(path: Path) -> dict:
    data=gpu_messages.read_records(path,{0x44345354:4});samples=data['records']
    good=sum(r['rows'][0]==r['rows'][1] and r['rows'][3]==[0.,0.,0.,1.] for r in samples)
    return {'complete_samples':len(samples),'unchanged_inactive_samples':good,
            'inconsistent_samples':len(samples)-good,'incomplete_or_duplicate_groups':data['incomplete_or_duplicate_groups'],
            'rejected_rows':data['rejected_rows'],'game_images_submitted_to_steamvr':0}


def session_report(output: Path) -> dict:
    trace=vulkan_trace.report(output)
    observations=json.loads((output/'process-observations.json').read_text())['observations']
    pids=sorted({p['pid'] for row in observations for p in row['processes'] if 'diablo iv.exe' in p['names']})
    game_traces=[t for t in trace['process_traces'] if t['file'] in {f'vulkan-{pid}.jsonl' for pid in pids}]
    devices=[d for t in game_traces for d in t['devices']]
    diagnostics=[d['paired_pipelines'] for d in devices if 'paired_pipelines' in d]
    return {'schema_version':1,'game_process_observed':bool(pids),'game_process_ids':pids,
        'any_layer_trace_loaded':trace['trace_loaded'],'game_device_trace_found':bool(devices),
        'game_pair_creation_confirmed':any(d['executable_pairs']>0 for d in diagnostics),
        'game_device_diagnostics':diagnostics,'all_game_devices_cleanly_destroyed':bool(devices) and all(d['clean_destroy'] for d in devices),
        'snapshot_counts_may_be_partial':any(not d['clean_destroy'] for d in devices),
        'private_pipelines_drawn':False,'game_stereo_submitted':False,'interpretation':'Only traces associated with an observed Diablo IV.exe process can confirm game device/pair creation.'}


def run(game: Path,launcher: Path,shader_bundle: Path,layer_bundle: Path,output: Path,seconds: int=180,launch_game: bool=False) -> int:
    if not 30<=seconds<=900:raise ValueError('Use a bounded diagnostic of 30 to 900 seconds')
    capture=prepare(game,launcher,shader_bundle,layer_bundle,output,launch_game);output=output.resolve()
    capture.update(phase='launching',maximum_seconds=seconds);probe.write_json(output/'capture.json',capture)
    started=time.monotonic();observations=[];process=None;timed_out=False
    print('Diagnostic de création seule : appels originaux conservés, aucune image VR. Fermeture ciblée à la fin.',flush=True)
    try:
        with (output/'launcher-private.log').open('xb') as stream:
            os.chmod(stream.name,0o600);process=subprocess.Popen(command(capture),stdout=stream,stderr=subprocess.STDOUT)
            while True:
                elapsed=time.monotonic()-started
                observations.append({'elapsed_seconds':round(elapsed,2),'processes':owned_processes(output)})
                probe.write_json(output/'process-observations.json',{'observations':observations})
                if process.poll() is not None:break
                if elapsed>=seconds:timed_out=True;break
                try:process.wait(timeout=min(2,seconds-elapsed))
                except subprocess.TimeoutExpired:pass
    finally:
        # Only processes bearing this new session marker AND the observed
        # start time receive SIGTERM. No kill-all, no Wine-server shutdown.
        owned=owned_processes(output)
        if owned:owned_processes(output,owned)
        deadline=time.monotonic()+10
        while owned and time.monotonic()<deadline:
            time.sleep(.5);owned=owned_processes(output)
            if owned:owned_processes(output,owned)
        if process is not None:
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10)
        capture.update(phase='launcher_exited',launcher_exit_code=process.returncode if process else None,
            time_limit_reached=timed_out,game_audit_after=probe.check_target(game,probe.profile()),
            remaining_owned_processes=owned_processes(output),remaining_launcher_processes=shader_capture.launcher_processes())
        probe.write_json(output/'capture.json',capture)
    summary=vulkan_trace.report(output);probe.write_json(output/'trace-report.json',summary)
    session=session_report(output);probe.write_json(output/'paired-session-report.json',session)
    if (output/'pairs.log').exists():probe.write_json(output/'inactive-telemetry.json',inactive_telemetry(output/'pairs.log'))
    print(json.dumps({'phase':capture['phase'],'any_layer_trace_loaded':summary['trace_loaded'],
                      'game_process_observed':session['game_process_observed'],'game_pair_creation_confirmed':session['game_pair_creation_confirmed'],'time_limit_reached':timed_out,
                      'remaining_owned_processes':len(capture['remaining_owned_processes']),'game_stereo_submitted':False}),flush=True)
    if capture['remaining_owned_processes']:return 2
    return 0 if timed_out else capture['launcher_exit_code'] or 0


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=('prepare','run'))
    for name in ('game-dir','launcher','shader-bundle','layer-bundle','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--launch-game',action='store_true',help='Ask Battle.net to launch Fen using the Lutris launch argument')
    parser.add_argument('--seconds',type=int,default=180);args=parser.parse_args()
    params=(args.game_dir,args.launcher,args.shader_bundle,args.layer_bundle,args.output)
    if args.action=='prepare':prepare(*params,launch_game=args.launch_game);print('Private diagnostic prepared; no process started.');return 0
    return run(*params,seconds=args.seconds,launch_game=args.launch_game)


if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        print(f'D4VR paires : {error}',file=sys.stderr);raise SystemExit(2)
