"""Exercise actual loader interception; compare private MRTs with GPU references."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

TEXELS=256*256
PLANE=4*TEXELS
SLOT=5*PLANE+TEXELS

def defined(data: bytes,resolution: int=256) -> bytes:
    texels=resolution*resolution;plane=4*texels;slot=5*plane+texels
    if len(data)!=slot:
        raise ValueError('Readback slot size mismatch')
    # The high byte of a D24 depth copy is undefined. Compare all other bytes.
    return data[:4*plane]+bytes(x for k,x in enumerate(data[4*plane:5*plane]) if k%4!=3)+data[5*plane:]

def compare(a: bytes,b: bytes,resolution: int=256) -> dict:
    aa,bb=defined(a,resolution),defined(b,resolution)
    return {'defined_bytes':len(aa),'different_bytes':sum(x!=y for x,y in zip(aa,bb)),'sha256':hashlib.sha256(aa).hexdigest()}

def inspect(directory: Path,positive: bool,unsupported: bool=False,resolution: int=256) -> dict:
    texels=resolution*resolution;slot=21*texels
    app=(directory/'application.bin').read_bytes()
    if len(app)!=5*slot:
        raise ValueError('Application did not produce all original/reference slots')
    original=compare(app[:slot],app[4*slot:],resolution)
    witness=compare(app[slot:2*slot],app[4*slot:],resolution)
    if original['different_bytes'] or witness['different_bytes']:
        raise ValueError('Original rendering or following state restoration changed')
    reports=[json.loads(p.read_text()) for p in sorted(directory.glob('poc-command-*.json'))]
    result={'original':original,'following_original_scope':witness,'layer_reports':reports}
    if positive:
        report=json.loads((directory/'poc-command-1.json').read_text())
        if (report.get('diablo_rendered') is not False or report.get('synthetic_scene') is not True
                or report.get('width')!=resolution or report.get('height')!=resolution
                or report.get('replayed_scopes')!=2 or report.get('replayed_draws')!=8 or report.get('forwarded_draws')!=4
                or report.get('rejected_scopes')!=0 or report.get('images_copied') is not True):
            raise ValueError('Layer did not report the actual two replayed original scopes')
        private=(directory/'poc-command-1.bin').read_bytes()
        if len(private)!=2*slot:
            raise ValueError('Private stereo readback incomplete')
        left=compare(private[:slot],app[2*slot:3*slot],resolution);right=compare(private[slot:],app[3*slot:4*slot],resolution)
        if left['different_bytes'] or right['different_bytes']:
            raise ValueError('Layer-produced eyes differ from independently recorded GPU references')
        pixels=sum(private[k*4:k*4+4]!=private[slot+k*4:slot+k*4+4] for k in range(texels))
        foreground=[sum(private[eye*slot+k*4+3]!=0 for k in range(texels)) for eye in range(2)]
        if pixels<100 or min(foreground)<100:
            raise ValueError('Stereo images empty or identical')
        result.update(left=left,right=right,different_eye_color_pixels=pixels,foreground_pixels=foreground)
    else:
        if list(directory.glob('poc-command-*.bin')) or any(r.get('replayed_scopes') for r in reports):
            raise ValueError('Negative control unexpectedly admitted private draws')
        if unsupported and (not reports or reports[0].get('forwarded_draws')!=4 or reports[0].get('rejected_scopes')!=2):
            raise ValueError('Unsupported command control did not exercise the recorder')
    return result

def run(args) -> dict:
    if args.output.exists():
        raise ValueError('Use a new private output directory')
    args.output.mkdir(parents=True,mode=0o700)
    layer=args.layer.resolve();validation=args.validation_layer.resolve();client=args.client.resolve()
    if json.loads((layer/'VkLayer_D4VR_replay_poc.json').read_text())['layer']['name']!='VK_LAYER_D4VR_replay_poc':
        raise ValueError('Expected the separate POC layer, not a staged game diagnostic')
    cases=[('positive',True,[],False),('unsupported-command',True,['--unsupported-command'],False),
           ('unrecognized-shader',True,['--unrecognized-shader'],False),('disabled',False,[],False),
           ('wrong-client',True,['--wrong-client'],False),('xr-loader-unavailable',True,[],False)]
    if args.runtime:
        cases.append(('steam-runtime',True,[],True))
    if args.mock_loader:
        if not args.mock_loader.is_file():
            raise ValueError('Missing isolated XR mock library')
        cases.extend([('xr-mock-session-failure',True,[],False),('xr-mock-preflight',True,[],False),('xr-mock-frames',True,[],False)])
        if args.runtime:
            cases.append(('xr-mock-steam-runtime',True,[],True))
    results=[]
    for name,enabled,options,runtime in cases:
        directory=args.output/name;directory.mkdir(mode=0o700)
        bridge=Path('/run/host/usr/bin/flatpak-spawn')
        command=([str(bridge),'--host','/usr/bin/env'] if bridge.exists() else ['/usr/bin/env'])
        for key in ('VK_INSTANCE_LAYERS','VK_LAYER_PATH','VK_ADD_LAYER_PATH','VK_LAYER_ENABLES','VK_LAYER_VALIDATE_SYNC','VK_LOADER_DEBUG',
                    'D4VR_TRACE_DIR','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_TRACE_XR_BINDING','D4VR_LAYER_REPLAY_POC','D4VR_POC_OUTPUT','D4VR_POC_XR_SECONDS','D4VR_XR_LOADER','D4VR_MOCK_XR_FAIL_SESSION','D4VR_POC_EXPLICIT_LAYERS','D4VR_POC_RESOLUTION'):
            command+=['-u',key]
        command+=['VK_ADD_LAYER_PATH='+str(layer)+':'+str(validation),
                  'D4VR_POC_EXPLICIT_LAYERS=1','D4VR_POC_RESOLUTION='+str(args.resolution),
                  'VK_LOADER_DEBUG=layer','VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1',
                  'D4VR_TRACE_DIR='+str(directory.resolve()),'D4VR_POC_OUTPUT='+str(directory.resolve()),'D4VR_LAYER_REPLAY_POC='+('1' if enabled else '0')]
        if name=='xr-loader-unavailable':
            command+=['D4VR_POC_XR_SECONDS=1','D4VR_XR_LOADER='+str((directory/'not-an-openxr-loader.so').resolve())]
        if name.startswith('xr-mock-'):
            command+=['D4VR_POC_XR_SECONDS='+('0' if name=='xr-mock-preflight' else '1'),'D4VR_XR_LOADER='+str(args.mock_loader.resolve())]
            if name=='xr-mock-session-failure':
                command+=['D4VR_MOCK_XR_FAIL_SESSION=1']
        if runtime:
            command+=[str(args.runtime.resolve()),'--batch','--']
        command+=[str(client),str(directory.resolve()),*options]
        log=directory/'process.log'
        with log.open('xb') as stream:
            os.chmod(log,0o600);p=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,timeout=60)
        text=log.read_text(errors='replace')
        if p.returncode or any(token in text for token in ('VUID-','Validation Error','SYNC-HAZARD-')):
            raise ValueError(name+': Vulkan execution or validation failed; inspect private process.log')
        for loaded in ('VK_LAYER_D4VR_replay_poc','VK_LAYER_KHRONOS_validation'):
            if f'Insert instance layer "{loaded}"' not in text or f'Inserted device layer "{loaded}"' not in text:
                raise ValueError(name+': did not verify instance and device layer loading')
        result=inspect(directory,name in ('positive','steam-runtime','xr-loader-unavailable') or name.startswith('xr-mock-'),name=='unsupported-command',args.resolution)
        if name=='xr-loader-unavailable':
            xr=json.loads((directory/'poc-xr.json').read_text())
            if xr.get('success') is not False or xr.get('replayed_xr_frames')!=0 or xr.get('eye_packet_updates')!=0 or xr.get('error')!='OpenXR loader unavailable':
                raise ValueError('Unavailable XR loader was not reported without beginning private replay')
            result['xr_loader_failure_preserved_originals']=True
            result['xr_report']=xr
        if name.startswith('xr-mock-'):
            xr=json.loads((directory/'poc-xr.json').read_text())
            if xr.get('runtime')!='D4VR mock XR (no SteamVR)' or 'D4VR mock XR queue reentry completed' not in text:
                raise ValueError('Mock did not exercise reentrant queue retrieval')
            failure=name=='xr-mock-session-failure';preflight=name=='xr-mock-preflight';frames=0 if failure or preflight else 2
            if (xr.get('success') is not (not failure) or xr.get('replayed_xr_frames')!=frames or xr.get('eye_packet_updates')!=frames
                    or xr.get('tracked_frames')!=frames or xr.get('preflight_only') is not preflight
                    or xr.get('required_extensions_enabled') is not True or xr.get('runtime_uuid_matches') is not True):
                raise ValueError('Mock XR frame/preflight/failure contract not verified')
            if failure and not xr.get('error','').startswith('xrCreateSession borrowed device:'):
                raise ValueError('Reentrant session failure did not reach the expected error')
            if not failure and xr.get('error'):
                raise ValueError('Mock XR success path reported an error')
            if frames and (text.count('D4VR mock XR projection frame completed')!=2 or xr.get('head_translation_meters',0)<.02):
                raise ValueError('Two distinct mocked head poses were not submitted')
            if frames and 'D4VR mock XR asynchronous runtime work submitted' not in text:
                raise ValueError('Mock did not enqueue runtime work after the private replay fence')
            records=[json.loads(line) for path in directory.glob('vulkan-*.jsonl') for line in path.read_text().splitlines()]
            destroyed=[r for r in records if r.get('event')=='device_destroy']
            if (len(destroyed)!=1 or destroyed[0].get('poc_application_recordings_submitted')!=2
                    or destroyed[0].get('poc_application_draws_submitted')!=10 or destroyed[0].get('commands',{}).get('draw')!=10):
                raise ValueError('Private mocked XR replay changed original submission/draw counts')
            if destroyed[0].get('submissions')!=(3 if frames else 2):
                raise ValueError('Runtime/application submission separation was not exercised')
            result.update(mock_runtime=True,actual_steamvr_submission=False,xr_report=xr,total_observed_device_submissions=destroyed[0]['submissions'],application_recordings_submitted=2)
        result.update(name=name,returncode=p.returncode,validation_errors=0)
        results.append(result)
    proof={'schema_version':1,'actual_loader_interception':True,'synthetic_scene':True,'diablo_rendered':False,'steamvr_submission':False,
           'render_extent':[args.resolution,args.resolution],
           'layer_sha256':hashlib.sha256((layer/'libd4vr_layer_replay_poc.so').read_bytes()).hexdigest(),
           'client_sha256':hashlib.sha256(client.read_bytes()).hexdigest(),'cases':results}
    (args.output/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    return proof

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--layer',type=Path,required=True);p.add_argument('--client',type=Path,required=True)
    p.add_argument('--validation-layer',type=Path,required=True);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--mock-loader',type=Path,help='Isolated XR mock; uses real GPU images but never a headset/compositor')
    p.add_argument('--resolution',type=int,choices=(256,1024),default=256)
    a=p.parse_args();print(json.dumps(run(a),indent=2))
