"""Exercise GPU camera updates and real loader hooks using owned geometry only."""
import argparse
import json
from pathlib import Path
import subprocess
import eye_control
import paired_capture
import present_validate
import probe
import vulkan_trace


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700,parents=True)
    validation={'VK_INSTANCE_LAYERS':'VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH':str(a.validation_layer.resolve()),'VK_LAYER_ENABLES':'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC':'1','VK_LOADER_DEBUG':'layer'}
    def run(name,fixture,args=(),variables=None,runtime=False,directory=None):
        directory=directory or a.output/name
        directory.mkdir(mode=0o700,exist_ok=True)
        command=paired_capture.host_command()+['/usr/bin/env']
        for key in ('VK_LAYER_PATH','VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','D4VR_TRACE_XR_BINDING','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_SEQUENTIAL_CAPTURE_DIR','D4VR_GPU_CAPTURE_DRY_RUN','D4VR_GPU_CAMERA_UPDATES','D4VR_XR_LOADER','D4VR_TRACE_DIR'):
            command+=['-u',key]
        variables=variables or {};command += [k+'='+v for k,v in variables.items()]
        if runtime:command += [str(a.runtime.resolve()),'--batch','--']
        command += [str(fixture.resolve()),*map(str,args)]
        p=subprocess.run(command,capture_output=True,text=True,timeout=40);log=p.stdout+p.stderr
        path=directory/'run.log';path.write_text(log);path.chmod(0o600)
        if p.returncode or any(s in log for s in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private validation log')
        if variables and not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation load not verified')
        print(name+': passed',flush=True);return json.loads(p.stdout.strip().splitlines()[-1]) if name!='layer-forwarding' else None
    ownership=run('portable-ownership',a.ownership_fixture)
    run('layer-forwarding',a.forwarding_fixture)
    cases=[]
    for runtime in (False,True) if a.runtime else (False,):
        name='gpu-steam-runtime' if runtime else 'gpu-native'
        report=run(name,a.fixture,[a.vertex.resolve(),a.fragment.resolve()],validation,runtime)
        if not (report.get('eye_updates_gpu_passed') and report.get('graphics_queues')==5 and report.get('poses_rendered')==9 and report.get('exact_reference_bytes')==5898240 and report.get('distinct_images')==18 and report.get('busy_reader_deferred') and report.get('fixed_addresses_and_pipelines')):raise ValueError('Camera updates did not prove geometry and bounded ownership')
        cases.append({'case':name,**report})
    legacy=present_validate.validate(argparse.Namespace(output=a.output/'legacy-wsi',fixture=a.layer_fixture,compiler=a.compiler,layer=a.layer,validation_layer=a.validation_layer,runtime=a.runtime))
    shaders=[(a.output/'legacy-wsi'/('present.'+ext+'.spv')).resolve() for ext in ('vert','frag')]
    def centers(path):
        data=path.read_bytes();width=256
        if len(data)!=width*width*4:raise ValueError('Unexpected owned WSI extent')
        out=[]
        for channel in (1,2):
            xs=[i//4%width for i in range(0,len(data),4) if data[i+channel]>200]
            if not xs:raise ValueError('Owned geometry disappeared')
            out.append(sum(xs)/len(xs))
        return out
    hooks=[]
    for runtime in (False,True) if a.runtime else (False,):
        name='camera-wsi-steam-runtime' if runtime else 'camera-wsi-native';directory=(a.output/name).resolve();directory.mkdir(mode=0o700)
        variables=dict(validation);variables.update(VK_INSTANCE_LAYERS=vulkan_trace.LAYER+':VK_LAYER_KHRONOS_validation',VK_ADD_LAYER_PATH=str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),D4VR_TRACE_DIR=str(directory),D4VR_SEQUENTIAL_CAPTURE_DIR=str(directory),D4VR_GPU_CAMERA_UPDATES='1')
        for eye,side in enumerate(('left','right')):
            path=directory/(side+'.bin');path.write_bytes(eye_control.encode(eye,separation=.128));path.chmod(0o600);variables['D4VR_PAIR_'+side.upper()+'_FILE']=str(path)
        run(name,a.layer_fixture,[*shaders,directory],variables,runtime,directory)
        rows=[json.loads(line) for path in directory.glob('vulkan-*.jsonl') for line in path.read_text().splitlines()]
        queued=[r['diagnostic']['packet_sequence'] for r in rows if r.get('event')=='camera_update_queued'];destroy=[r for r in rows if r.get('event')=='device_destroy']
        if queued!=[2,3,4] or len(destroy)!=1 or any(r.get('event') in ('camera_update_fatal','camera_request_rejected','presentation_error','paired_pipeline_error') for r in rows):raise ValueError('Real loader camera hook failed')
        stats=destroy[0].get('camera_updates',{})
        if stats.get('updates')!=3 or stats.get('registered_queues')!=5 or stats.get('marker_backpressure')!=0:raise ValueError('Hook queues/updates not covered')
        base=centers(directory/'left-reference.pixels');plus=centers(directory/'camera-plus.pixels');minus=centers(directory/'camera-minus.pixels');right=centers(directory/'right-reference.pixels');restored=centers(directory/'camera-restored-right.pixels')
        expected=[256*.5*1.784022*.125/z for z in (2,4)]
        deltas={'+translation':[p-b for p,b in zip(plus,base)],'-translation':[m-b for m,b in zip(minus,base)],'restore_right':[r-p for r,p in zip(restored,right)]}
        for label,values in deltas.items():
            sign=1 if label=='+translation' else -1
            if any(abs(v-sign*e)>.75 for v,e in zip(values,expected)):raise ValueError('Updated shader geometry does not match requested translation: '+label)
        before=(directory/'mono-before.pixels').read_bytes()
        if before!=(directory/'mono-after.pixels').read_bytes():raise ValueError('Restored original pixels changed')
        if destroy[0].get('submissions')!=320 or destroy[0].get('commands',{}).get('draw')!=128:raise ValueError('Game fixture draws/submissions duplicated')
        hooks.append({'case':name,'updates':3,'packet_sequences':queued,'graphics_queues':5,'original_draws':128,'original_submissions':320,'original_restored_equal_bytes':len(before),'centroid_shifts_pixels':deltas,'expected_absolute_shift_pixels':expected,'stats':stats,'validation_errors':0})
    legacy['forwarding_tests_passed']=True;probe.write_json(a.layer/'sequential-validation.json',legacy)
    proof={'schema_version':1,'synthetic_scene':True,'diablo_rendered':False,'actual_steamvr_submission':False,'xr_pose_applied':False,'khronos_synchronization_validation_loaded':True,'validation_errors':0,'fixture_sha256':probe.digest(a.fixture),'layer_sha256':probe.digest(a.layer/vulkan_trace.LIBRARY),'ownership':ownership,'gpu_cases':cases,'real_wsi_hooks':hooks,'legacy_wsi':legacy}
    probe.write_json(a.output/'proof.json',proof);return proof

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','ownership-fixture','forwarding-fixture','layer-fixture','vertex','fragment','compiler','layer','validation-layer','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
