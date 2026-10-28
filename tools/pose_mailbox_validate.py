"""OpenXR-request mailbox, deferred source lifecycle and real GPU packet controls."""
import argparse
import json
from pathlib import Path
import subprocess
import paired_capture
import probe


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700,parents=True)
    unit=subprocess.run(paired_capture.host_command()+[str(a.unit.resolve())],capture_output=True,text=True,timeout=15)
    if unit.returncode:raise ValueError('Portable pose mailbox test failed')
    portable=json.loads(unit.stdout.strip());(a.output/'portable.log').write_text(unit.stdout+unit.stderr)
    # mode, control, frames, XR requests, queued GPU packets, acquisitions
    cases=[('deferred','',2,2,4,4),('deferred-gpu','',2,2,4,4),('deferred-timeout','TIMEOUT',2,2,4,4),('deferred-placement','',2,2,4,4),
           ('deferred-unavailable','',0,2,0,0),('deferred-stale','',0,2,4,0),('deferred-abandon','',0,1,0,0),
           ('deferred-abandon-gpu','',0,1,2,2),('deferred-stop','STOP_SOURCE',0,1,0,0),('deferred-stop-gpu','STOP',0,1,2,2),
           ('deferred-space-change','SPACE_CHANGE',0,1,0,0),('deferred-throw','',0,1,0,0),('deferred-incomplete','',0,0,0,0)]
    runs=[(case,False) for case in cases]
    if a.runtime:runs.append((cases[1],True))
    results=[]
    for (mode,control,frames,requests,packets,acquires),runtime in runs:
        name=mode+('-steam-runtime' if runtime else '');command=paired_capture.host_command()+['/usr/bin/env']
        for key in ('VK_INSTANCE_LAYERS','VK_LAYER_PATH','VK_ADD_LAYER_PATH','D4VR_XR_LOADER','D4VR_MOCK_XR_FAIL_SESSION','D4VR_MOCK_XR_CHECK_UPLOAD','D4VR_MOCK_XR_FLIP','D4VR_LAYER_REPLAY_POC','D4VR_TRACE_DIR','D4VR_MOCK_XR_TIMEOUT','D4VR_MOCK_XR_STOP_IMAGES','D4VR_MOCK_XR_STOP','D4VR_MOCK_XR_STOP_SOURCE','D4VR_MOCK_XR_SPACE_CHANGE','D4VR_MOCK_XR_HIDDEN','D4VR_MOCK_XR_UNTRACKED','D4VR_MOCK_XR_FAIL_ACQUIRE'):
            command+=['-u',key]
        command+=['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(a.validation_layer.resolve()),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer','D4VR_XR_LOADER='+str(a.mock_loader.resolve()),'D4VR_MOCK_XR_CHECK_UPLOAD=1']
        if control:command+=['D4VR_MOCK_XR_'+control+'=1']
        if runtime:command+=[str(a.runtime.resolve()),'--batch','--']
        command+=[str(a.fixture.resolve()),mode];p=subprocess.run(command,capture_output=True,text=True,timeout=20);log=p.stdout+p.stderr;path=a.output/(name+'.log');path.write_text(log);path.chmod(0o600)
        if p.returncode or any(x in log for x in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private validation log')
        if not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation layer load not verified')
        report=json.loads(p.stdout.strip());xr=report['xr'];failure=mode in ('deferred-throw','deferred-incomplete')
        if (xr.get('projection_requests')!=requests or xr.get('deferred_requests')!=requests or xr.get('replayed_xr_frames')!=frames
            or report.get('requests_finished')!=requests or report.get('pose_packets_gpu_verified')!=packets or report.get('pose_packet_bytes_verified')!=packets*176
            or report.get('presentations')!=frames or xr.get('success')!=(frames>0) or bool(xr.get('error'))!=failure
            or report.get('actual_steamvr_submission') is not False or report.get('pose_driven_geometry_verified') is not False):raise ValueError(name+': incorrect request/packet lifecycle')
        if xr.get('incoherent_pairs_rejected')!=(2 if mode=='deferred-stale' else 0):raise ValueError('Strict source coherence was changed')
        if xr.get('unavailable_pairs')!=(2 if mode=='deferred-unavailable' else 0):raise ValueError('Unavailable source was not skipped')
        if log.count('D4VR mock XR image pixels verified')!=frames:raise ValueError('XR patterned pixels not independently verified')
        if mode!='deferred-incomplete' and f'D4VR mock XR image lifecycle: {acquires} acquires, {acquires} releases' not in log:raise ValueError('Deferred source acquired too soon or leaked XR images')
        if mode=='deferred-gpu' and report.get('producer_pending_polls',0)<6:raise ValueError('Producer GPU fence was not kept pending')
        if mode=='deferred-abandon-gpu' and report.get('held_consumer_drained_before_finish') is not True:raise ValueError('Finish ran before draining the held consumer')
        if mode=='deferred-stop-gpu' and (report.get('consumer_pending_polls',0)<3 or 'D4VR mock XR session stopped' not in log):raise ValueError('Stopping did not drain the pending consumer')
        if mode=='deferred-timeout' and xr.get('swapchain_wait_timeouts')!=8:raise ValueError('Deferred source did not preserve XR acquisitions on timeout')
        if mode=='deferred-incomplete' and 'callbacks must be complete' not in xr['error']:raise ValueError('Incomplete deferred callback contract accepted')
        results.append({'case':name,'result':report,'exact_pattern_pixel_bytes':frames*2*64*48*4,'swapchain_acquires':acquires,'swapchain_releases':acquires});print(name+': passed',flush=True)
    proof={'schema_version':1,'synthetic_scene':True,'mock_runtime':True,'diablo_rendered':False,'actual_steamvr_submission':False,'pose_driven_geometry_verified':False,'khronos_synchronization_validation_loaded':True,'validation_errors':0,'portable':portable,'fixture_sha256':probe.digest(a.fixture),'mock_loader_sha256':probe.digest(a.mock_loader),'cases':results}
    probe.write_json(a.output/'proof.json',proof);return proof

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('unit','fixture','mock-loader','validation-layer','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
