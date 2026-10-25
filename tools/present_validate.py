#!/usr/bin/env python3
"""Verify sequential selection and capture through a real WSI loader path."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import os
import struct
import subprocess
import eye_control
import paired_capture
import probe
import sequential_capture
import vulkan_trace


def validate(args):
    if args.output.exists():raise ValueError('Use a new private validation directory')
    args.output.mkdir(parents=True,mode=0o700);root=Path(__file__).resolve().parents[1]
    shaders=[]
    for ext in ('vert','frag'):
        target=args.output/('present.'+ext+'.spv')
        subprocess.run(paired_capture.host_command()+[str(args.compiler.resolve()),'-V','--target-env','vulkan1.2',str(root/('native/tests/present_eye.'+ext)),'-o',str(target.resolve())],capture_output=True,check=True,timeout=30)
        if ext=='vert':
            raw=target.read_bytes();words=list(struct.unpack('<'+'I'*(len(raw)//4),raw));marker=b'D4VR_STEREO_ABI_1\0';marker+=b'\0'*(-len(marker)%4)
            instruction=[((len(marker)//4+1)<<16)|330,*struct.unpack('<'+'I'*(len(marker)//4),marker)];index=5
            while index<len(words) and words[index]&65535 not in (71,72,19,20,21,22):index+=words[index]>>16
            words[index:index]=instruction;target.write_bytes(struct.pack('<'+'I'*len(words),*words))
        shaders.append(target.resolve())
    cases=[('native',True,False),('disabled',False,False)]
    if args.runtime:cases.append(('steam-runtime',True,True))
    results=[]
    for name,enabled,in_runtime in cases:
        directory=(args.output/name).resolve();directory.mkdir(mode=0o700)
        variables={'VK_INSTANCE_LAYERS':vulkan_trace.LAYER+':VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH':str(args.layer.resolve())+':'+str(args.validation_layer.resolve()),
            'VK_LAYER_ENABLES':'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC':'1','VK_LOADER_DEBUG':'layer','D4VR_TRACE_DIR':str(directory)}
        if enabled:
            for eye,side in enumerate(('left','right')):
                file=directory/(side+'.bin');file.write_bytes(eye_control.encode(eye,separation=.128));file.chmod(0o600)
                variables['D4VR_PAIR_'+side.upper()+'_FILE']=str(file)
            variables['D4VR_SEQUENTIAL_CAPTURE_DIR']=str(directory)
        command=paired_capture.host_command()+['/usr/bin/env']
        for key in ('VK_LAYER_PATH','VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','D4VR_TRACE_XR_BINDING','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_SEQUENTIAL_CAPTURE_DIR'):command+=['-u',key]
        command+=[k+'='+v for k,v in variables.items()]
        if in_runtime:command+=[str(args.runtime.resolve()),'--batch','--']
        command+=[str(args.fixture.resolve()),*(str(p) for p in shaders),str(directory)]
        with (directory/'run.log').open('xb') as log:
            os.chmod(log.name,0o600);result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=40)
        log=(directory/'run.log').read_text(errors='replace')
        if result.returncode or any(x in log for x in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect the private validation log')
        if not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation layer load not verified')
        before=(directory/'mono-before.pixels').read_bytes();after=(directory/'mono-after.pixels').read_bytes()
        if before!=after:raise ValueError('Original image changed after restoring original pipelines')
        if enabled:
            pair=sequential_capture.pair(directory,require_game=False);centers={};reference_bytes=0
            if pair.get('repeated_eye_selections_verified') is not True:raise ValueError('Repeated left/right images missing')
            for metadata in directory.glob('image-*.json'):
                d,b=sequential_capture.read_image(metadata);reference=(directory/('left-reference.pixels' if d['eye']==0 else 'right-reference.pixels')).read_bytes()
                if b!=reference:raise ValueError('Captured pixels differ from original rendered reference')
                registry=d.get('pipeline_registry_at_export',{})
                if registry.get('maximum_live_nodes')!=8192 or registry.get('budget_rejections')!=0:raise ValueError('Pipeline counters missing from image export')
                reference_bytes+=len(b)
                for channel in (1,2):
                    xs=[i//4%d['width'] for i in range(0,len(b),4) if b[i+channel]>200]
                    if not xs:raise ValueError('Controlled geometry missing')
                    centers[(d['eye'],channel)]=sum(xs)/len(xs)
            disparities=[centers[(0,c)]-centers[(1,c)] for c in (1,2)]
            if disparities[1]<=0 or not 1.7<disparities[0]/disparities[1]<2.3:raise ValueError('Depth-dependent geometric disparity not verified')
            entry={'case':name,'reference_equal_bytes':reference_bytes,'original_restored_bytes':len(before),'disparities_pixels':disparities,'different_pixel_bytes':pair['different_pixel_bytes'],'repeated_capture_frames':4}
        else:
            if list(directory.glob('image-*.json')) or (directory/'left-reference.pixels').read_bytes()!=before or (directory/'right-reference.pixels').read_bytes()!=before:raise ValueError('Disabled capture changed pixels or wrote an image')
            entry={'case':name,'disabled_capture_verified':True,'original_restored_bytes':len(before)}
        trace=vulkan_trace.report(directory)
        if not trace['trace_loaded'] or any(not d['clean_destroy'] for t in trace['process_traces'] for d in t['devices']):raise ValueError('Loader integration/clean teardown not verified')
        if enabled:
            stats=[d['paired_pipelines'] for t in trace['process_traces'] for d in t['devices'] if 'paired_pipelines' in d]
            if not stats or any(s.get('mode')!='sequential_original_render' or s.get('maximum_live_nodes')!=8192 or s.get('maximum_creation_attempts')!=8192 or s.get('budget_rejections')!=0 for s in stats):raise ValueError('Explicit sequential pipeline quota not verified')
            entry['sequential_quota_verified']=True
        results.append(entry);print(name+': passed',flush=True)
    report={'purpose':'sequential_presentation_capture','schema_version':1,'layer_sha256':probe.digest(args.layer/vulkan_trace.LIBRARY),
        'wsi_loader_capture_passed':True,'original_pixels_restored':True,'synthetic_scene':True,'game_stereo_submitted':False,'cases':results}
    probe.write_json(args.output/'proof.json',report);return report


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','compiler','layer','validation-layer','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path);a=p.parse_args()
    try:print(json.dumps(validate(a),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(f'D4VR presentation validation: {e}')
