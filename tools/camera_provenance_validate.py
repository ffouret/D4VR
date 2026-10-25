"""Validate owned camera evidence through delayed GPU leases and real WSI hooks."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import eye_control
import paired_capture
import present_validate
import probe
import vulkan_trace


def expected_packet(eye,sequence):
    values=list(eye_control.FORMAT.unpack(eye_control.encode(eye,sequence,.128)))
    values[11]=(.064 if eye==0 else -.064)+(.125 if sequence==2 else -.125 if sequence==3 else 0)
    return eye_control.FORMAT.pack(*values)


def inspect(camera,eye,sequence,submission):
    if (camera.get('same_diagnostic_packet') is not True or camera.get('whole_scene_verified') is not False
        or camera.get('xr_pose_known') is not False or camera.get('common_scene_time_known') is not False):raise ValueError('Invented scene or XR provenance')
    for scope in ('copy_submit','selected_draw_interval'):
        sample=camera.get(scope,{})
        if (sample.get('status')!='uniform' or sample.get('packet_eye')!=eye or sample.get('packet_sequence')!=sequence
            or sample.get('first_submission')!=submission or sample.get('last_submission')!=submission
            or bytes.fromhex(sample.get('packet_abi_bytes_hex',''))!=expected_packet(eye,sequence)):raise ValueError('Camera packet bytes, eye, revision or submission identity changed')


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700,parents=True)
    validation={'VK_INSTANCE_LAYERS':'VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH':str(a.validation_layer.resolve()),'VK_LAYER_ENABLES':'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC':'1','VK_LOADER_DEBUG':'layer'}
    def run(name,fixture,args=(),variables=None,runtime=False,directory=None):
        directory=directory or a.output/name;directory.mkdir(mode=0o700,exist_ok=True)
        command=paired_capture.host_command()+['/usr/bin/env']
        for key in ('VK_LAYER_PATH','VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','D4VR_TRACE_XR_BINDING','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_SEQUENTIAL_CAPTURE_DIR','D4VR_GPU_CAPTURE_DRY_RUN','D4VR_GPU_CAMERA_UPDATES','D4VR_XR_LOADER','D4VR_TRACE_DIR'):
            command+=['-u',key]
        variables=variables or {};command += [k+'='+v for k,v in variables.items()]
        if runtime:command += [str(a.runtime.resolve()),'--batch','--']
        command += [str(fixture.resolve()),*map(str,args)]
        result=subprocess.run(command,capture_output=True,text=True,timeout=40);log=result.stdout+result.stderr
        path=directory/'run.log';path.write_text(log);path.chmod(0o600)
        if result.returncode or any(s in log for s in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private validation log')
        if variables and not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation load not verified')
        print(name+': passed',flush=True);return result.stdout
    portable=json.loads(run('portable-camera',a.camera_fixture).strip().splitlines()[-1]);run('portable-leases',a.ownership_fixture);run('layer-forwarding',a.forwarding_fixture)
    gpu=[]
    for runtime in (False,True) if a.runtime else (False,):
        name='delayed-gpu-steam-runtime' if runtime else 'delayed-gpu-native';report=json.loads(run(name,a.fixture,variables=validation,runtime=runtime).strip().splitlines()[-1])
        if report.get('camera_packet_copies_verified')!=37 or report.get('delayed_camera_not_retagged') is not True:raise ValueError('Delayed packet evidence missing')
        gpu.append({'case':name,**report})
    legacy=present_validate.validate(argparse.Namespace(output=a.output/'legacy-wsi',fixture=a.layer_fixture,compiler=a.compiler,layer=a.layer,validation_layer=a.validation_layer,runtime=a.runtime))
    immutable=[];negative_controls=0
    for directory in sorted((a.output/'legacy-wsi').iterdir()):
        if directory.is_dir() and directory.name!='disabled':
            count=0
            for path in directory.glob('image-*.json'):
                d=json.loads(path.read_text());inspect(d.get('camera_provenance',{}),d['eye'],1,d['present']);count+=1
                if not negative_controls:
                    # Mutate actual emitted evidence: the aggregate checker
                    # must refuse missing/mixed, retagged and invented XR data.
                    for field,value in (('status','mixed'),('packet_sequence',2),('packet_abi_bytes_hex','00'*176),('first_submission',d['present']+1)):
                        invalid=copy.deepcopy(d['camera_provenance']);invalid['copy_submit'][field]=value
                        try:inspect(invalid,d['eye'],1,d['present'])
                        except ValueError:negative_controls+=1
                        else:raise ValueError('Corrupted camera evidence accepted by validator')
                    invalid=copy.deepcopy(d['camera_provenance']);invalid['xr_pose_known']=True
                    try:inspect(invalid,d['eye'],1,d['present'])
                    except ValueError:negative_controls+=1
                    else:raise ValueError('Invented XR provenance accepted by validator')
            if count!=4:raise ValueError('Immutable camera metadata absent')
            immutable.append({'case':directory.name,'frames':count,'packet_sequence':1})
    shaders=[(a.output/'legacy-wsi'/('present.'+ext+'.spv')).resolve() for ext in ('vert','frag')]
    hooks=[]
    for runtime in (False,True) if a.runtime else (False,):
        for gpu_mode in (False,True):
            name=('gpu-' if gpu_mode else 'cpu-')+('mutable-wsi-steam-runtime' if runtime else 'mutable-wsi-native');directory=(a.output/name).resolve();directory.mkdir(mode=0o700)
            variables=dict(validation);variables.update(VK_INSTANCE_LAYERS=vulkan_trace.LAYER+':VK_LAYER_KHRONOS_validation',VK_ADD_LAYER_PATH=str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),D4VR_TRACE_DIR=str(directory),D4VR_SEQUENTIAL_CAPTURE_DIR=str(directory),D4VR_GPU_CAMERA_UPDATES='1')
            if gpu_mode:variables['D4VR_GPU_CAPTURE_DRY_RUN']='1'
            for eye,side in enumerate(('left','right')):
                path=directory/(side+'.bin');path.write_bytes(eye_control.encode(eye,separation=.128));path.chmod(0o600);variables['D4VR_PAIR_'+side.upper()+'_FILE']=str(path)
            run(name,a.layer_fixture,[*shaders,directory],variables,runtime,directory)
            rows=[json.loads(line) for path in directory.glob('vulkan-*.jsonl') for line in path.read_text().splitlines()]
            errors=[r for r in rows if r.get('event') in ('camera_update_fatal','camera_request_rejected','presentation_error','paired_pipeline_error')]
            if errors:raise ValueError('Camera layer error')
            revisions=[r['diagnostic']['packet_sequence'] for r in rows if r.get('event')=='camera_update_queued']
            if revisions!=[2,3,4]:raise ValueError('Mutable camera updates not queued')
            checked=0;packet_revisions=set()
            if gpu_mode:
                if list(directory.glob('image-*.pixels')):raise ValueError('GPU producer exported CPU pixels')
                leases=[r for r in rows if r.get('event')=='gpu_capture_lease']
                if not 48<=len(leases)<=56:raise ValueError('GPU lease stream incomplete')
                for r in leases:
                    frame=r['source_present']-1;eye=0 if 4<=frame<20 or 36<=frame<52 else 1 if 20<=frame<36 or 52<=frame<60 else 2
                    revision=1 if frame<=12 else 2 if frame<=40 else 3 if frame<=52 else 4
                    if r['eye']!=eye or not r.get('retirement_submitted') or r.get('xr_pose_known') is not False:raise ValueError('Lease eye/retirement invalid')
                    inspect(r.get('camera_provenance',{}),eye,revision,frame*5+1);packet_revisions.add(revision);checked+=1
            else:
                for path in directory.glob('image-*.json'):
                    d=json.loads(path.read_text());revision=d['sequence'];inspect(d.get('camera_provenance',{}),d['eye'],revision,(d['present']-1)*5+1);packet_revisions.add(revision);checked+=1
                if checked!=4:raise ValueError('Mutable CPU metadata absent')
            if packet_revisions!={1,2,3,4}:raise ValueError('A camera revision was lost')
            if (directory/'mono-before.pixels').read_bytes()!=(directory/'mono-after.pixels').read_bytes():raise ValueError('Original restored pixels changed')
            destroy=[r for r in rows if r.get('event')=='device_destroy']
            if len(destroy)!=1 or destroy[0].get('submissions')!=320 or destroy[0].get('commands',{}).get('draw')!=128:raise ValueError('Application rendering duplicated')
            hooks.append({'case':name,'frames_with_exact_camera_bytes':checked,'packet_sequences':sorted(packet_revisions),'original_submissions':320,'original_draws':128,'validation_errors':0,'gpu_mode':gpu_mode})
    legacy['forwarding_tests_passed']=True;probe.write_json(a.layer/'sequential-validation.json',legacy)
    if negative_controls!=5:raise ValueError('Camera evidence corruption controls missing')
    proof={'schema_version':1,'synthetic_scene':True,'diablo_rendered':False,'actual_steamvr_submission':False,'actual_xr_pose_source':False,'whole_scene_verified':False,'khronos_synchronization_validation_loaded':True,'validation_errors':0,'corrupted_evidence_controls_refused':negative_controls,'camera_ownership':portable,'delayed_gpu_cases':gpu,'immutable_wsi_evidence':immutable,'mutable_wsi_evidence':hooks,'legacy_wsi':legacy,'layer_sha256':probe.digest(a.layer/vulkan_trace.LIBRARY),'gpu_fixture_sha256':probe.digest(a.fixture)}
    probe.write_json(a.output/'proof.json',proof);return proof

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','camera-fixture','ownership-fixture','forwarding-fixture','layer-fixture','compiler','layer','validation-layer','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
