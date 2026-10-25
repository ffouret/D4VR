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

def defined(data: bytes) -> bytes:
    if len(data)!=SLOT:
        raise ValueError('Readback slot size mismatch')
    # The high byte of a D24 depth copy is undefined. Compare all other bytes.
    return data[:4*PLANE]+bytes(x for k,x in enumerate(data[4*PLANE:5*PLANE]) if k%4!=3)+data[5*PLANE:]

def compare(a: bytes,b: bytes) -> dict:
    aa,bb=defined(a),defined(b)
    return {'defined_bytes':len(aa),'different_bytes':sum(x!=y for x,y in zip(aa,bb)),'sha256':hashlib.sha256(aa).hexdigest()}

def inspect(directory: Path,positive: bool,unsupported: bool=False) -> dict:
    app=(directory/'application.bin').read_bytes()
    if len(app)!=5*SLOT:
        raise ValueError('Application did not produce all original/reference slots')
    original=compare(app[:SLOT],app[4*SLOT:])
    witness=compare(app[SLOT:2*SLOT],app[4*SLOT:])
    if original['different_bytes'] or witness['different_bytes']:
        raise ValueError('Original rendering or following state restoration changed')
    reports=[json.loads(p.read_text()) for p in sorted(directory.glob('poc-command-*.json'))]
    result={'original':original,'following_original_scope':witness,'layer_reports':reports}
    if positive:
        report=json.loads((directory/'poc-command-1.json').read_text())
        if (report.get('diablo_rendered') is not False or report.get('synthetic_scene') is not True
                or report.get('replayed_scopes')!=2 or report.get('replayed_draws')!=8 or report.get('forwarded_draws')!=4
                or report.get('rejected_scopes')!=0 or report.get('images_copied') is not True):
            raise ValueError('Layer did not report the actual two replayed original scopes')
        private=(directory/'poc-command-1.bin').read_bytes()
        if len(private)!=2*SLOT:
            raise ValueError('Private stereo readback incomplete')
        left=compare(private[:SLOT],app[2*SLOT:3*SLOT]);right=compare(private[SLOT:],app[3*SLOT:4*SLOT])
        if left['different_bytes'] or right['different_bytes']:
            raise ValueError('Layer-produced eyes differ from independently recorded GPU references')
        pixels=sum(private[k*4:k*4+4]!=private[SLOT+k*4:SLOT+k*4+4] for k in range(TEXELS))
        foreground=[sum(private[eye*SLOT+k*4+3]!=0 for k in range(TEXELS)) for eye in range(2)]
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
           ('wrong-client',True,['--wrong-client'],False)]
    if args.runtime:
        cases.append(('steam-runtime',True,[],True))
    results=[]
    for name,enabled,options,runtime in cases:
        directory=args.output/name;directory.mkdir(mode=0o700)
        bridge=Path('/run/host/usr/bin/flatpak-spawn')
        command=([str(bridge),'--host','/usr/bin/env'] if bridge.exists() else ['/usr/bin/env'])
        for key in ('VK_INSTANCE_LAYERS','VK_LAYER_PATH','VK_ADD_LAYER_PATH','VK_LAYER_ENABLES','VK_LAYER_VALIDATE_SYNC','VK_LOADER_DEBUG',
                    'D4VR_TRACE_DIR','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_TRACE_XR_BINDING','D4VR_LAYER_REPLAY_POC','D4VR_POC_OUTPUT'):
            command+=['-u',key]
        command+=['VK_ADD_LAYER_PATH='+str(layer)+':'+str(validation),
                  'VK_INSTANCE_LAYERS=VK_LAYER_D4VR_replay_poc:VK_LAYER_KHRONOS_validation',
                  'VK_LOADER_DEBUG=layer','VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1',
                  'D4VR_TRACE_DIR='+str(directory.resolve()),'D4VR_POC_OUTPUT='+str(directory.resolve()),'D4VR_LAYER_REPLAY_POC='+('1' if enabled else '0')]
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
        result=inspect(directory,name in ('positive','steam-runtime'),name=='unsupported-command')
        result.update(name=name,returncode=p.returncode,validation_errors=0)
        results.append(result)
    proof={'schema_version':1,'actual_loader_interception':True,'synthetic_scene':True,'diablo_rendered':False,'steamvr_submission':False,
           'layer_sha256':hashlib.sha256((layer/'libd4vr_layer_replay_poc.so').read_bytes()).hexdigest(),
           'client_sha256':hashlib.sha256(client.read_bytes()).hexdigest(),'cases':results}
    (args.output/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    return proof

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--layer',type=Path,required=True);p.add_argument('--client',type=Path,required=True)
    p.add_argument('--validation-layer',type=Path,required=True);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();print(json.dumps(run(a),indent=2))
