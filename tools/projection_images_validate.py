"""Real Vulkan image transport through an isolated mock XR session; no headset."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import paired_capture
import probe

def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700,parents=True);results=[]
    cases=['fresh','flip','stale','mixed-scene','wrong-eye','wrong-family','wrong-format','missing','stale-second','missing-first']
    runs=[(name,False) for name in cases]
    if a.runtime:runs.append(('fresh',True))
    for mode,runtime in runs:
        name='steam-runtime' if runtime else mode
        command=paired_capture.host_command()+['/usr/bin/env']
        for key in ('VK_INSTANCE_LAYERS','VK_LAYER_PATH','VK_ADD_LAYER_PATH','D4VR_XR_LOADER','D4VR_MOCK_XR_FAIL_SESSION','D4VR_MOCK_XR_CHECK_UPLOAD','D4VR_MOCK_XR_FLIP','D4VR_LAYER_REPLAY_POC','D4VR_TRACE_DIR'):
            command+=['-u',key]
        command+=['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(a.validation_layer.resolve()),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer','D4VR_XR_LOADER='+str(a.mock_loader.resolve()),'D4VR_MOCK_XR_CHECK_UPLOAD=1']
        if mode=='flip':command+=['D4VR_MOCK_XR_FLIP=1']
        if runtime:command+=[str(a.runtime.resolve()),'--batch','--']
        command+=[str(a.fixture.resolve()),mode]
        result=subprocess.run(command,capture_output=True,text=True,timeout=20)
        log=result.stdout+result.stderr;p=a.output/(name+'.log');p.write_text(log);p.chmod(0o600)
        if result.returncode or any(s in log for s in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private validation log')
        if not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation layer load not verified')
        report=json.loads(result.stdout.strip().splitlines()[-1]);expected=2 if mode in ('fresh','flip') else 1 if mode in ('stale-second','missing-first') else 0
        unavailable=2 if mode=='missing' else 1 if mode=='missing-first' else 0
        rejected=0 if mode in ('fresh','flip','missing','missing-first') else 1 if mode=='stale-second' else 2
        if (report.get('replayed_xr_frames')!=expected or report.get('projection_requests')!=2
            or report.get('incoherent_pairs_rejected')!=rejected or report.get('unavailable_pairs')!=unavailable
            or report.get('success')!=(expected>0) or report.get('error') or report.get('diablo_rendered') is not False):raise ValueError(name+': unexpected coherence report')
        if log.count('D4VR mock XR image pixels verified')!=expected:raise ValueError('XR image pixels not independently verified')
        lifecycle=f'D4VR mock XR image lifecycle: {2*expected} acquires, {2*expected} releases'
        if lifecycle not in log:raise ValueError('Invalid/absent pairs acquired an XR image or leaked one')
        if expected==2 and 'D4VR mock XR asynchronous runtime work submitted' not in log:raise ValueError('Runtime teardown control missing')
        results.append({'case':name,'xr':report,'exact_pixel_bytes_verified':expected*2*64*48*4,'swapchain_acquires':2*expected,'swapchain_releases':2*expected});print(name+': passed',flush=True)
    proof={'schema_version':1,'synthetic_scene':True,'diablo_rendered':False,'actual_steamvr_submission':False,'validation_errors':0,'khronos_synchronization_validation_loaded':True,'fixture_sha256':probe.digest(a.fixture),'mock_loader_sha256':probe.digest(a.mock_loader),'cases':results}
    probe.write_json(a.output/'proof.json',proof);return proof

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','mock-loader','validation-layer','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
