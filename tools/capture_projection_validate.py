"""Actual reusable Capture -> explicit successive diagnostic -> mock XR geometry."""
import argparse
import json
from pathlib import Path
import subprocess
import paired_capture
import probe


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700,parents=True)
    modes=['normal','held-producer','held-consumer','timeout','foreign-queue','strict','expire','wrong-packet','abandon']
    runs=[(mode,False) for mode in modes]
    if a.runtime:runs.extend([('normal',True),('held-consumer',True)])
    results=[]
    for mode,runtime in runs:
        name=mode+('-steam-runtime' if runtime else '')
        command=paired_capture.host_command()+['/usr/bin/env']
        for key in ('VK_INSTANCE_LAYERS','VK_LAYER_PATH','VK_ADD_LAYER_PATH','D4VR_XR_LOADER','D4VR_MOCK_XR_FAIL_SESSION','D4VR_MOCK_XR_CHECK_UPLOAD','D4VR_MOCK_XR_FLIP','D4VR_LAYER_REPLAY_POC','D4VR_TRACE_DIR','D4VR_MOCK_XR_TIMEOUT','D4VR_MOCK_XR_STOP_IMAGES','D4VR_MOCK_XR_STOP','D4VR_MOCK_XR_STOP_SOURCE','D4VR_MOCK_XR_SPACE_CHANGE','D4VR_MOCK_XR_HIDDEN','D4VR_MOCK_XR_UNTRACKED','D4VR_MOCK_XR_FAIL_ACQUIRE','D4VR_MOCK_XR_EXTERNAL_REFERENCE'):
            command+=['-u',key]
        command+=['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(a.validation_layer.resolve()),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer','D4VR_XR_LOADER='+str(a.mock_loader.resolve()),'D4VR_MOCK_XR_CHECK_UPLOAD=1','D4VR_MOCK_XR_EXTERNAL_REFERENCE=1']
        if mode=='timeout':command+=['D4VR_MOCK_XR_TIMEOUT=1']
        if runtime:command+=[str(a.runtime.resolve()),'--batch','--']
        command+=[str(a.fixture.resolve()),str(a.vertex.resolve()),str(a.fragment.resolve()),mode]
        p=subprocess.run(command,capture_output=True,text=True,timeout=15);log=p.stdout+p.stderr
        path=a.output/(name+'.log');path.write_text(log);path.chmod(0o600)
        if p.returncode or any(x in log for x in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private validation log')
        if not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation layer load not verified')
        report=json.loads(p.stdout[p.stdout.index('{'):]);xr=report['xr']
        normal=mode in ('normal','held-producer','held-consumer','timeout','foreign-queue')
        expected=2 if normal else 0
        if (report.get('presentations')!=expected or xr.get('replayed_xr_frames')!=expected or xr.get('error')
            or report.get('actual_capture_producer') is not True or report.get('pose_driven_geometry_verified') is not True
            or report.get('retired_leases')!=report.get('selected_leases')+report.get('rejected_leases')
            or report.get('slots',5)>4 or report.get('diablo_rendered') is not False or report.get('actual_steamvr_submission') is not False
            or report.get('whole_scene_verified') is not False or report.get('common_scene_time_known') is not False):raise ValueError(name+': source/lease/claim mismatch')
        if log.count('D4VR mock XR independent geometry pixels verified')!=expected:raise ValueError('XR geometry pixel reference missing')
        if f'D4VR mock XR image lifecycle: {expected*2} acquires, {expected*2} releases' not in log:raise ValueError('Invalid source acquired/leaked XR images')
        if normal and (report.get('independent_geometry_bytes')!=262144 or report.get('distinct_images')!=4 or report.get('selected_leases')!=4):raise ValueError('Geometry not controlled by both poses/eyes')
        if mode=='strict' and (xr.get('presentation_mode')!='coherent' or xr.get('incoherent_pairs_rejected')!=2):raise ValueError('Strict coherence guard weakened')
        if mode!='strict' and xr.get('presentation_mode')!='successive_diagnostic':raise ValueError('Successive diagnostic not explicit')
        if mode=='held-producer' and report.get('held_producer_polls',0)<3:raise ValueError('Producer fence not held')
        if mode=='held-consumer' and report.get('held_consumer_polls',0)<6:raise ValueError('Consumer fence not held')
        if mode=='timeout' and xr.get('swapchain_wait_timeouts')!=8:raise ValueError('Swapchain timeout acquisition lost')
        if mode in ('expire','wrong-packet') and xr.get('unavailable_pairs')!=2:raise ValueError('Incomplete source not canceled')
        if mode=='wrong-packet' and report.get('rejected_leases')!=2:raise ValueError('Mixed camera evidence not retired')
        if mode=='abandon' and (report.get('selected_leases')!=1 or report.get('retired_leases')!=1):raise ValueError('Abandoned producer lease not retired')
        results.append({'case':name,'result':report,'exact_xr_geometry_bytes':expected*2*128*128*4,'validation_errors':0});print(name+': passed',flush=True)
    proof={'schema_version':1,'synthetic_scene':True,'mock_runtime':True,'actual_capture_producer':True,'pose_driven_geometry_verified':True,'diablo_rendered':False,'actual_steamvr_submission':False,'whole_scene_verified':False,'common_scene_time_known':False,'khronos_synchronization_validation_loaded':True,'validation_errors':0,'fixture_sha256':probe.digest(a.fixture),'mock_loader_sha256':probe.digest(a.mock_loader),'vertex_sha256':probe.digest(a.vertex),'fragment_sha256':probe.digest(a.fragment),'cases':results}
    probe.write_json(a.output/'proof.json',proof);return proof


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','mock-loader','vertex','fragment','validation-layer','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
