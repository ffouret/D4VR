"""Cooperative XR lifecycle with actual Vulkan fences and caller GPU progress."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import paired_capture
import probe


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700,parents=True);results=[]
    # (fixture mode, runtime control, expected frames/requests/acquires)
    cases=[('pump','',2,2,4),('pump-timeout','TIMEOUT',2,2,4),
           ('pump-gpu','',2,2,4),('pump-stop','STOP',0,1,2),
           ('pump-stop-images','STOP_IMAGES',0,1,1),('pump-abandon-images','TIMEOUT',0,1,1),('pump-abandon-gpu','',0,1,2),
           ('pump-acquire-failure','FAIL_ACQUIRE',0,1,1),('throw-source','',0,1,0),
           ('pump-hidden','HIDDEN',0,0,0),('pump-untracked','UNTRACKED',0,0,0)]
    runs=[(case,False) for case in cases]
    if a.runtime:runs.append((cases[2],True))
    for (mode,control,frames,requests,acquires),runtime in runs:
        name=mode+('-steam-runtime' if runtime else '')
        command=paired_capture.host_command()+['/usr/bin/env']
        for key in ('VK_INSTANCE_LAYERS','VK_LAYER_PATH','VK_ADD_LAYER_PATH','D4VR_XR_LOADER','D4VR_MOCK_XR_FAIL_SESSION','D4VR_MOCK_XR_CHECK_UPLOAD','D4VR_MOCK_XR_FLIP','D4VR_LAYER_REPLAY_POC','D4VR_TRACE_DIR','D4VR_MOCK_XR_TIMEOUT','D4VR_MOCK_XR_STOP_IMAGES','D4VR_MOCK_XR_STOP','D4VR_MOCK_XR_HIDDEN','D4VR_MOCK_XR_UNTRACKED','D4VR_MOCK_XR_FAIL_ACQUIRE'):
            command+=['-u',key]
        command+=['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(a.validation_layer.resolve()),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer','D4VR_XR_LOADER='+str(a.mock_loader.resolve()),'D4VR_MOCK_XR_CHECK_UPLOAD=1']
        if control:command+=['D4VR_MOCK_XR_'+control+'=1']
        if control=='STOP_IMAGES':command+=['D4VR_MOCK_XR_TIMEOUT=1']
        if runtime:command+=[str(a.runtime.resolve()),'--batch','--']
        command+=[str(a.fixture.resolve()),mode]
        result=subprocess.run(command,capture_output=True,text=True,timeout=20)
        log=result.stdout+result.stderr;p=a.output/(name+'.log');p.write_text(log);p.chmod(0o600)
        if result.returncode or any(s in log for s in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private validation log')
        if not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation layer load not verified')
        report=json.loads(result.stdout.strip().splitlines()[-1]);failure=mode in ('throw-source','pump-acquire-failure')
        if (report.get('replayed_xr_frames')!=frames or report.get('projection_requests')!=requests
            or bool(report.get('error'))!=failure or report.get('success')!=(frames>0)
            or report.get('diablo_rendered') is not False or report.get('eye_packet_updates')!=(0 if mode=='throw-source' else requests)):
            raise ValueError(name+': unexpected frame lifecycle report')
        if failure and ('Injected source failure' if mode=='throw-source' else 'xrAcquireSwapchainImage') not in report['error']:raise ValueError('Wrong failure path')
        if log.count('D4VR mock XR image pixels verified')!=frames:raise ValueError('Projection pixels not independently verified')
        if f'D4VR mock XR image lifecycle: {acquires} acquires, {acquires} releases' not in log:raise ValueError('Acquired images leaked on timeout/stop/error/teardown')
        progress=re.search(r'D4VR caller progress: (\d+) turns, (\d+) GPU markers, (\d+) pending GPU turns, (\d+) presentations',log)
        if not progress or int(progress[2])!=requests or int(progress[4])!=frames:raise ValueError('Caller did not progress independently between XR advances')
        if mode=='pump-timeout' and report.get('swapchain_wait_timeouts')!=8:raise ValueError('Timed-out images were reused/acquired/released prematurely')
        if mode.startswith('pump-gpu') and (report.get('gpu_pending_polls',0)<6 or int(progress[3])<6):raise ValueError('Actual pending GPU fence not exercised')
        if mode=='pump-abandon-images' and report.get('swapchain_wait_timeouts')!=1:raise ValueError('Acquired but unavailable image teardown not tested')
        if mode=='pump-stop-images' and ('D4VR mock XR session stopped' not in log or report.get('swapchain_wait_timeouts')!=2):raise ValueError('Stop did not wait for the single acquired image')
        if mode=='pump-stop' and ('D4VR mock XR session stopped' not in log or report.get('gpu_pending_polls',0)<3):raise ValueError('Stop did not drain pending work before ending session')
        results.append({'case':name,'xr':report,'caller_turns':int(progress[1]),'caller_gpu_markers':int(progress[2]),'pending_gpu_turns':int(progress[3]),'exact_pixel_bytes_verified':frames*2*64*48*4,'swapchain_acquires':acquires,'swapchain_releases':acquires});print(name+': passed',flush=True)
    proof={'schema_version':1,'synthetic_scene':True,'diablo_rendered':False,'actual_steamvr_submission':False,'validation_errors':0,'khronos_synchronization_validation_loaded':True,'fixture_sha256':probe.digest(a.fixture),'mock_loader_sha256':probe.digest(a.mock_loader),'cases':results}
    probe.write_json(a.output/'proof.json',proof);return proof

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','mock-loader','validation-layer','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
