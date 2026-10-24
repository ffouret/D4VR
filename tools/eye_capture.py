#!/usr/bin/env python3
"""Launch an explicit single-eye geometry diagnostic with a layer-owned packet."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

import camera_capture
import eye_control
import eye_report
import probe
import shader_capture
import vulkan_trace


def validate_layer(directory: Path) -> dict:
    manifest = json.loads((directory/'trace-bundle.json').read_text(encoding='utf-8'))
    proof = manifest['validation']
    if (manifest.get('purpose') != 'vulkan_trace_diagnostic' or proof.get('eye_control_fixture_passed') is not True
            or proof.get('eye_control_mode') != 'immutable_per_launch'):
        raise ValueError('A validated immutable eye-control layer is required')
    for name,key in ((vulkan_trace.LIBRARY,'layer_sha256'),(vulkan_trace.MANIFEST,'manifest_sha256')):
        if probe.digest(directory/name) != manifest[key]:
            raise ValueError('Eye-control layer changed since GPU validation')
    return manifest


def run(game: Path,launcher: Path,shader_bundle: Path,layer_bundle: Path,state_file: Path,output: Path) -> int:
    state = eye_control.read(state_file); layer = validate_layer(layer_bundle)
    bundle = camera_capture.validate_bundle(shader_bundle)
    if (bundle.get('purpose') != 'eye_transform_diagnostic' or bundle.get('eye_state_abi') != 1
            or bundle.get('eye_state_specialization_id') != 4 or bundle.get('stereo_telemetry') is not True
            or bundle['gpu_validation'].get('stereo_gpu_fixture_passed') is not True):
        raise ValueError('Provide the validated controlled eye-transform shader bundle')
    expected = probe.profile()
    if bundle['game_version'] != expected['game_version']:
        raise ValueError('Eye-transform bundle belongs to another game version')
    game,output = game.resolve(),output.resolve()
    if output == game or game in output.parents or output.exists():
        raise ValueError('Use a new private output directory outside the game')
    source = launcher.read_text(encoding='utf-8'); pattern = r'^export PROTONPATH="GE-Proton"$'
    if len(re.findall(pattern,source,re.M)) != 1:
        raise ValueError('Expected the reviewed Lutris launcher')
    source = re.sub(pattern,lambda _: 'export PROTONPATH='+shlex.quote(bundle['proton_path']),source,flags=re.M)
    # Target/process checks happen before creating the launch directory.
    audit = probe.check_target(game,expected)
    if shader_capture.launcher_processes():
        raise ValueError('Close Diablo IV and Battle.net before this isolated launch')
    output.mkdir(parents=True,mode=0o700)
    capture = shader_capture.prepare(game,launcher,output,expected)
    packet = output/'eye-state.bin'; packet.write_bytes(state_file.read_bytes()); packet.chmod(0o600)
    if eye_control.read(packet) != state:
        raise ValueError('Eye state changed during preparation')
    private_launcher = output/'battle-net-eye.sh'; private_launcher.write_text(source,encoding='utf-8'); private_launcher.chmod(0o700)
    variables = {'VK_INSTANCE_LAYERS':vulkan_trace.LAYER,'VK_ADD_LAYER_PATH':str(layer_bundle.resolve()),
        'D4VR_TRACE_DIR':str(output),'D4VR_EYE_STATE_FILE':str(packet),
        'VKD3D_SHADER_OVERRIDE':probe.windows_path(shader_bundle.resolve()/'overrides'),
        'VKD3D_SHADER_CACHE_PATH':capture['variables']['VKD3D_SHADER_CACHE_PATH'],
        'VKD3D_SHADER_DEBUG_RING_SIZE_LOG2':'24','VKD3D_SHADER_DEBUG':'info','VKD3D_DEBUG':'info',
        'VKD3D_LOG_FILE':probe.windows_path(output/'eye.log')}
    capture.update(phase='launching',purpose='single_fixed_eye_geometry_diagnostic',variables=variables,
        game_audit_before=audit,eye_state=state,eye_state_sha256=probe.digest(packet),
        layer_sha256=layer['layer_sha256'],shader_bundle=str(shader_bundle.resolve()),
        game_files_changed=False,game_stereo_submitted=False)
    probe.write_json(output/'capture.json',capture)
    bridge = Path('/run/host/usr/bin/flatpak-spawn')
    command = [str(bridge),'--host','/usr/bin/env'] if bridge.exists() else ['/usr/bin/env']
    for key in ('VKD3D_SHADER_DUMP_PATH','D4VR_TRACE_XR_BINDING','D4VR_XR_LOADER','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE'):
        command += ['-u',key]
    command += [key+'='+value for key,value in variables.items()]+['/usr/bin/bash',str(private_launcher)]
    print('Battle.net : test géométrique avec un œil fixe, sur le moniteur. Le casque reste posé.',flush=True)
    print('Un très léger décalage du décor est prévu. Entrer en partie puis attendre le relevé.',flush=True)
    with (output/'launcher-private.log').open('wb') as stream:
        os.chmod(stream.name,0o600); result = subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT)
    capture.update(phase='launcher_exited',launcher_exit_code=result.returncode,
        game_audit_after=probe.check_target(game,expected),remaining_processes=shader_capture.launcher_processes())
    if (output/'eye.log').exists():
        report = eye_report.parse(output/'eye.log',packet); probe.write_json(output/'eye-report.json',report)
        capture['numeric_eye_transform_confirmed'] = report['numeric_eye_transform_confirmed']
        capture['verified_transformed_samples'] = report['verified_transformed_samples']
    probe.write_json(output/'trace-report.json',vulkan_trace.report(output))
    probe.write_json(output/'capture.json',capture)
    print(json.dumps({k:capture[k] for k in ('phase','launcher_exit_code','game_stereo_submitted')},indent=2),flush=True)
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('game-dir','launcher','shader-bundle','layer-bundle','state','output'):
        parser.add_argument('--'+option,type=Path,required=True)
    args = parser.parse_args()
    return run(args.game_dir,args.launcher,args.shader_bundle,args.layer_bundle,args.state,args.output)


if __name__ == '__main__':
    try: raise SystemExit(main())
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        print(f'D4VR œil fixe : {error}',file=sys.stderr); raise SystemExit(2)
