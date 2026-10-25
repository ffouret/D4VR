"""Validate bounded GPU capture leases, portable ownership and real WSI hooks."""
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
    def run(name,fixture,args=(),variables=None,runtime=False):
        directory=a.output/name;directory.mkdir(mode=0o700)
        command=paired_capture.host_command()+['/usr/bin/env']
        for key in ('VK_LAYER_PATH','VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','D4VR_TRACE_XR_BINDING','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_SEQUENTIAL_CAPTURE_DIR','D4VR_GPU_CAPTURE_DRY_RUN','D4VR_XR_LOADER','D4VR_TRACE_DIR'):
            command+=['-u',key]
        variables=variables or {}
        command += [k+'='+v for k,v in variables.items()]
        if runtime:command += [str(a.runtime.resolve()),'--batch','--']
        command += [str(fixture.resolve()),*map(str,args)]
        p=subprocess.run(command,capture_output=True,text=True,timeout=40);log=p.stdout+p.stderr
        path=directory/'run.log';path.write_text(log);path.chmod(0o600)
        if p.returncode or any(s in log for s in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private validation log')
        if 'VK_INSTANCE_LAYERS' in variables and not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation load not verified')
        print(name+': passed',flush=True);return p.stdout
    validation={'VK_INSTANCE_LAYERS':'VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH':str(a.validation_layer.resolve()),'VK_LAYER_ENABLES':'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC':'1','VK_LOADER_DEBUG':'layer'}
    run('portable-ownership',a.ownership_fixture);run('layer-forwarding',a.forwarding_fixture)
    baseline=json.loads(run('legacy-cpu',a.baseline_fixture,variables=validation).strip().splitlines()[-1])
    gpu=[]
    for runtime in (False,True) if a.runtime else (False,):
        name='gpu-steam-runtime' if runtime else 'gpu-native';report=json.loads(run(name,a.fixture,variables=validation,runtime=runtime).strip().splitlines()[-1])
        if (report.get('gpu_capture_fixture_passed') is not True or report.get('leased_frames')!=37 or report.get('slots')!=4 or report.get('allocations')!=4 or report.get('reuses')!=37 or report.get('backpressure')!=3
            or any(report.get(k)!=0 for k in ('producer_readback_buffers','producer_maps','producer_readback_commands'))):raise ValueError('GPU fixture did not prove bounded ownership and no CPU readback')
        gpu.append({'case':name,**report})
    legacy=present_validate.validate(argparse.Namespace(output=a.output/'legacy-wsi',fixture=a.layer_fixture,compiler=a.compiler,layer=a.layer,validation_layer=a.validation_layer,runtime=a.runtime))
    shaders=[(a.output/'legacy-wsi'/('present.'+ext+'.spv')).resolve() for ext in ('vert','frag')]
    wsi=[]
    for runtime in (False,True) if a.runtime else (False,):
        name='gpu-wsi-steam-runtime' if runtime else 'gpu-wsi-native';directory=(a.output/name).resolve()
        variables=dict(validation);variables.update(VK_INSTANCE_LAYERS=vulkan_trace.LAYER+':VK_LAYER_KHRONOS_validation',VK_ADD_LAYER_PATH=str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),D4VR_TRACE_DIR=str(directory),D4VR_SEQUENTIAL_CAPTURE_DIR=str(directory),D4VR_GPU_CAPTURE_DRY_RUN='1')
        # The directory is made by run(); packet files can live beside it.
        for eye,side in enumerate(('left','right')):
            path=(a.output/(name+'-'+side+'.bin')).resolve();path.write_bytes(eye_control.encode(eye,separation=.128));path.chmod(0o600);variables['D4VR_PAIR_'+side.upper()+'_FILE']=str(path)
        run(name,a.layer_fixture,[*shaders,directory],variables,runtime)
        if list(directory.glob('image-*')):raise ValueError('GPU-only hook exported a CPU image')
        rows=[json.loads(line) for path in directory.glob('vulkan-*.jsonl') for line in path.read_text().splitlines()]
        leases=[r for r in rows if r.get('event')=='gpu_capture_lease'];destroy=[r for r in rows if r.get('event')=='device_destroy']
        if not leases or len(destroy)!=1 or any(r.get('event') in ('presentation_error','paired_pipeline_error') for r in rows):raise ValueError('Hook did not complete GPU lease lifecycle')
        stats=destroy[0].get('gpu_capture',{})
        if not (len(leases)>=48 and len(leases)<=56 and stats.get('leases_issued')==len(leases) and stats.get('retire_submissions')==len(leases) and stats.get('outstanding_leases')==0
                and 1<=stats.get('slots',0)<=4 and stats.get('allocations')<=4 and stats.get('reuses',0)>=48):raise ValueError('Real WSI hook did not reuse bounded images')
        for row in leases:
            frame=row['source_present']-1;expected=0 if 4<=frame<20 or 36<=frame<52 else 1 if 20<=frame<36 or 52<=frame<60 else 2
            if (row.get('eye')!=expected or row.get('selected_mask')!=1<<expected or row.get('selected_draws')!=2 or row.get('retirement_submitted') is not True
                or row.get('xr_pose_known') is not False or row.get('game_images_read_by_consumer') is not False or row.get('xr_submission') is not False):raise ValueError('Hook image provenance/retirement invalid')
        before=(directory/'mono-before.pixels').read_bytes();after=(directory/'mono-after.pixels').read_bytes()
        if before!=after:raise ValueError('GPU stream changed restored original pixels')
        if destroy[0].get('submissions')!=64 or destroy[0].get('commands',{}).get('draw')!=128:raise ValueError('Original application rendering was repeated')
        wsi.append({'case':name,'frames':64,'original_source_equal_bytes':len(before),'leases':len(leases),'eyes':sorted(set(r['eye'] for r in leases)),'stats':stats,'validation_errors':0,'original_application_submissions':64,'original_draw_commands':128})
    legacy['forwarding_tests_passed']=True;probe.write_json(a.layer/'sequential-validation.json',legacy)
    proof={'schema_version':1,'synthetic_scene':True,'diablo_rendered':False,'actual_steamvr_submission':False,'validation_errors':0,'khronos_synchronization_validation_loaded':True,'fixture_sha256':probe.digest(a.fixture),'layer_sha256':probe.digest(a.layer/vulkan_trace.LIBRARY),'portable_ownership_passed':True,'layer_forwarding_passed':True,'gpu_cases':gpu,'legacy_cpu_case':baseline,'legacy_wsi':legacy,'gpu_wsi_dry_run':wsi}
    probe.write_json(a.output/'proof.json',proof);return proof

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','ownership-fixture','baseline-fixture','forwarding-fixture','layer-fixture','compiler','layer','validation-layer','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
