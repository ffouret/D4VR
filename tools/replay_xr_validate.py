"""Bounded real SteamVR test of a frozen scope captured by the Vulkan layer.

Requires an available headset. This is a synthetic scene, never a Diablo run.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
from collections import Counter
from pathlib import Path
import subprocess
from layer_poc_validate import inspect

def run(a) -> dict:
    if not 1<=a.seconds<=30 or a.output.exists():
        raise ValueError('Use a new private output directory and 1..30 seconds')
    for path in (a.client,a.loader,a.runtime_json,a.layer/'libd4vr_layer_replay_poc.so',a.validation_layer/'VkLayer_khronos_validation.json'):
        if not path.is_file():
            raise ValueError('Missing built dependency: '+str(path))
    a.output.mkdir(parents=True,mode=0o700)
    bridge=Path('/run/host/usr/bin/flatpak-spawn')
    command=([str(bridge),'--host','/usr/bin/env'] if bridge.exists() else ['/usr/bin/env'])
    for key in ('VK_INSTANCE_LAYERS','VK_LAYER_PATH','VK_ADD_LAYER_PATH','VK_LAYER_ENABLES','VK_LAYER_VALIDATE_SYNC','VK_LOADER_DEBUG',
                'D4VR_TRACE_DIR','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_TRACE_XR_BINDING','D4VR_LAYER_REPLAY_POC','D4VR_POC_OUTPUT','D4VR_POC_XR_SECONDS','D4VR_XR_LOADER','XR_RUNTIME_JSON','D4VR_MOCK_XR_FAIL_SESSION','D4VR_POC_EXPLICIT_LAYERS','D4VR_POC_RESOLUTION'):
        command+=['-u',key]
    command+=['VK_ADD_LAYER_PATH='+str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),
              'D4VR_POC_EXPLICIT_LAYERS=1','D4VR_POC_RESOLUTION='+str(a.resolution),
              'VK_LOADER_DEBUG=layer','VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1',
              'D4VR_LAYER_REPLAY_POC=1','D4VR_POC_OUTPUT='+str(a.output.resolve()),'D4VR_TRACE_DIR='+str(a.output.resolve()),
              'D4VR_POC_XR_SECONDS='+str(0 if a.preflight else a.seconds),'D4VR_XR_LOADER='+str(a.loader.resolve()),'XR_RUNTIME_JSON='+str(a.runtime_json.resolve()),
              '/usr/bin/timeout','--signal=TERM','--kill-after=5s',str(a.seconds+25)+'s']
    if a.runtime:
        command+=[str(a.runtime.resolve()),'--batch','--']
    command+=[str(a.client.resolve()),str(a.output.resolve())]
    log=a.output/'process.log'
    with log.open('xb') as stream:
        os.chmod(log,0o600);p=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,timeout=a.seconds+35)
    text=log.read_text(errors='replace')
    # Preserve failed attempts as observations, never as a clean proof.
    counts=Counter()
    for line in text.splitlines():
        match=re.search(r'\[ (VUID-[^ ]+|SYNC-HAZARD-[^ ]+) \]',line)
        if match:counts[match[1]]+=1
    report=json.loads((a.output/'poc-xr.json').read_text()) if (a.output/'poc-xr.json').is_file() else None
    attempt={'schema_version':1,'synthetic_scene':True,'diablo_rendered':False,'client_returncode':p.returncode,
             'render_extent':[a.resolution,a.resolution],'validation_message_counts':dict(counts),'xr':report,
             'actual_steamvr_submission_observed':bool(report and 'steamvr' in report.get('runtime','').lower() and report.get('replayed_xr_frames',0)>0),
             'clean_validation_proof':False}
    (a.output/'attempt.json').write_text(json.dumps(attempt,indent=2)+'\n')
    if p.returncode:
        raise ValueError(f'XR POC process failed (returncode={p.returncode}) or timed out; inspect its private process.log')
    if any(s in text for s in ('VUID-','Validation Error','SYNC-HAZARD-')):
        raise ValueError('Vulkan validation reported an error in the XR path')
    for name in ('VK_LAYER_D4VR_replay_poc','VK_LAYER_KHRONOS_validation'):
        if f'Insert instance layer "{name}"' not in text or f'Inserted device layer "{name}"' not in text:
            raise ValueError('Instance/device layer loading was not verified')
    report=json.loads((a.output/'poc-xr.json').read_text())
    if (report.get('success') is not True or 'steamvr' not in report.get('runtime','').lower()
            or report.get('diablo_rendered') is not False or report.get('synthetic_scene') is not True
            or report.get('eye_packet_updates')!=report.get('replayed_xr_frames')
            or report.get('application_submissions_repeated')!=0 or report.get('borrowed_vulkan_objects_destroyed') is not False):
        raise ValueError('No verified replay-produced SteamVR frames; inspect private poc-xr.json')
    if a.preflight:
        if report.get('preflight_only') is not True or report.get('session_and_swapchains_created') is not True or report.get('replayed_xr_frames')!=0:
            raise ValueError('XR session/swapchain preflight did not complete without frames')
    elif report.get('preflight_only') is not False or report.get('replayed_xr_frames',0)<1 or report.get('tracked_frames',0)<1:
        raise ValueError('XR tracked frames were not submitted')
    original=inspect(a.output,True,resolution=a.resolution)
    records=[json.loads(line) for path in a.output.glob('vulkan-*.jsonl') for line in path.read_text().splitlines()]
    destroyed=[r for r in records if r.get('event')=='device_destroy']
    if (len(destroyed)!=1 or destroyed[0].get('poc_application_recordings_submitted')!=2
            or destroyed[0].get('poc_application_draws_submitted')!=10 or destroyed[0].get('commands',{}).get('draw')!=10):
        raise ValueError('Original application submission/draw counts changed')
    proof={'schema_version':1,'synthetic_scene':True,'diablo_rendered':False,'actual_steamvr_submission':not a.preflight,'xr':report,
           'application_recordings_submitted':2,'original_application_draws':10,'total_observed_device_submissions':destroyed[0]['submissions'],
           'render_extent':[a.resolution,a.resolution],'original_rendering':original,'validation_errors':0,
           'layer_sha256':hashlib.sha256((a.layer/'libd4vr_layer_replay_poc.so').read_bytes()).hexdigest(),
           'client_sha256':hashlib.sha256(a.client.read_bytes()).hexdigest()}
    (a.output/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    return proof

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('client','layer','validation-layer','loader','runtime-json','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--runtime',type=Path);p.add_argument('--seconds',type=int,default=10)
    p.add_argument('--preflight',action='store_true',help='Create session/swapchains only; never begin a session or submit XR frames')
    p.add_argument('--resolution',type=int,choices=(256,1024),default=1024)
    print(json.dumps(run(p.parse_args()),indent=2))
