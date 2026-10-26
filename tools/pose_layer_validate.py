"""Real WSI/loader CaptureProjection integration; external diagnostic poses only."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import eye_control
import paired_capture
import present_validate
import probe
import vulkan_trace


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700,parents=True)
    def run(command):
        p=subprocess.run(paired_capture.host_command()+command,capture_output=True,text=True,timeout=40)
        if p.returncode:raise ValueError('Fixture build/unit failed: '+p.stderr[-500:])
        return p.stdout
    portable=json.loads(run([str(a.unit.resolve())]).strip());run([str(a.forwarding.resolve())])
    root=Path(__file__).resolve().parents[1];shaders=[]
    for ext in ('vert','frag'):
        path=a.output/('present.'+ext+'.spv');run([str(a.compiler.resolve()),'-V','--target-env','vulkan1.2',str(root/('native/tests/'+('present_fragment_eye.' if getattr(a,'fragment_packet_check',False) else 'present_eye.')+ext)),'-o',str(path.resolve())])
        if ext=='vert' or (getattr(a,'fragment_packet_check',False) and getattr(a,'fragment_marker',True)):
            words=list(struct.unpack('<'+'I'*(len(path.read_bytes())//4),path.read_bytes()));marker=b'D4VR_STEREO_ABI_1\0';marker+=b'\0'*(-len(marker)%4);instruction=[((len(marker)//4+1)<<16)|330,*struct.unpack('<'+'I'*(len(marker)//4),marker)];index=5
            while index<len(words) and words[index]&65535 not in (71,72,19,20,21,22):index+=words[index]>>16
            words[index:index]=instruction;path.write_bytes(struct.pack('<'+'I'*len(words),*words))
        if ext=='frag' and getattr(a,'fragment_packet_check',False) and getattr(a,'fragment_marker',True):
            words=list(struct.unpack('<'+'I'*(len(path.read_bytes())//4),path.read_bytes()));marker=b'D4VR_FRAGMENT_PROJECTION_INPUT_1\0';marker+=b'\0'*(-len(marker)%4);instruction=[((len(marker)//4+1)<<16)|330,*struct.unpack('<'+'I'*(len(marker)//4),marker)];index=5
            while index<len(words) and words[index]&65535 not in (71,72,19,20,21,22):index+=words[index]>>16
            words[index:index]=instruction;path.write_bytes(struct.pack('<'+'I'*len(words),*words))
        shaders.append(path.resolve())
    cases=[]
    for runtime in (False,True) if a.runtime else (False,):
        for control in getattr(a,'controls',('normal','short','foreign','cancel')):
            short=control in ('short','cancel');name=('' if control=='normal' else control+'-')+('steam-runtime' if runtime else 'native');directory=(a.output/name).resolve();directory.mkdir(mode=0o700)
            variables={'VK_INSTANCE_LAYERS':vulkan_trace.LAYER+':VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH':str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),'VK_LAYER_ENABLES':'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC':'1','VK_LOADER_DEBUG':'layer','D4VR_TRACE_DIR':str(directory),'D4VR_SEQUENTIAL_CAPTURE_DIR':str(directory),'D4VR_GPU_CAMERA_UPDATES':'1','D4VR_POSE_CAPTURE_DIAGNOSTIC':'1'}
            if getattr(a,'hold_view',False):
                variables['D4VR_POSE_FIXTURE_MONOSCOPIC']='1'
                if not getattr(a,'disable_hold',False):variables['D4VR_DIAGNOSTIC_VIEW_HOLD']='1'
            if getattr(a,'calibrated',False):variables['D4VR_POSE_FIXTURE_CALIBRATED']='1'
            if short:variables['D4VR_POSE_FIXTURE_SHORT']='1'
            if control=='foreign':variables['D4VR_POSE_FIXTURE_FOREIGN']='1'
            if control=='cancel':variables['D4VR_POSE_FIXTURE_CANCEL']='1'
            for eye,side in enumerate(('left','right')):
                file=directory/(side+'.bin');file.write_bytes(eye_control.encode(eye,separation=.128));file.chmod(0o600);variables['D4VR_PAIR_'+side.upper()+'_FILE']=str(file)
            command=paired_capture.host_command()+['/usr/bin/env']
            for key in ('VK_LAYER_PATH','VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','D4VR_TRACE_XR_BINDING','D4VR_LAYER_REPLAY_POC','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_PAIR_LEFT_FILE','D4VR_PAIR_RIGHT_FILE','D4VR_SEQUENTIAL_CAPTURE_DIR','D4VR_GPU_CAPTURE_DRY_RUN','D4VR_GPU_CAMERA_UPDATES','D4VR_XR_LOADER','D4VR_TRACE_DIR','D4VR_HELD_DEPTH_COLOR_DIAGNOSTIC','D4VR_POSE_CAPTURE_DIAGNOSTIC','D4VR_POSE_FIXTURE_SHORT','D4VR_POSE_FIXTURE_FOREIGN','D4VR_POSE_FIXTURE_CANCEL','D4VR_POSE_FIXTURE_CALIBRATED','D4VR_METRIC_SOURCE_DEPTH_SCALE','D4VR_DIAGNOSTIC_VIEW_HOLD','D4VR_POSE_FIXTURE_MONOSCOPIC'):command+=['-u',key]
            command+=[k+'='+v for k,v in variables.items()]
            if runtime:command+=[str(a.runtime.resolve()),'--batch','--']
            command+=[str(a.fixture.resolve()),*map(str,shaders),str(directory)]
            p=subprocess.run(command,capture_output=True,text=True,timeout=40);log=p.stdout+p.stderr;(directory/'run.log').write_text(log)
            if p.returncode or any(x in log for x in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private validation log')
            if not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation load not verified')
            rows=[json.loads(line) for path in directory.glob('vulkan-*.jsonl') for line in path.read_text().splitlines()]
            errors=[r for r in rows if r.get('event') in ('pose_capture_error','presentation_error','camera_update_fatal','paired_pipeline_error')]
            if errors:raise ValueError(name+': layer error')
            if not any(r.get('event')=='pose_capture_request_rejected' for r in rows):raise ValueError('Malformed external sample not refused')
            requests=[r for r in rows if r.get('event')=='pose_capture_request'];expected=1 if short else 2
            if [r['request'] for r in requests]!=list(range(1,expected+1)):raise ValueError('Request duplicated/lost')
            revisions=[r['diagnostic']['packet_sequence'] for r in rows if r.get('event')=='camera_update_queued']
            if revisions!=list(range(2,expected+2)):raise ValueError('Competing camera file changed owned revisions')
            frames=8 if short else 64;destroy=[r for r in rows if r.get('event')=='device_destroy']
            submissions=frames*(6 if control=='foreign' else 5)
            if len(destroy)!=1 or destroy[0].get('submissions')!=submissions or destroy[0].get('commands',{}).get('draw')!=frames*2:raise ValueError('Application commands duplicated')
            state=destroy[0].get('pose_capture',{});pool=destroy[0].get('pose_producer',{})
            if state.get('active') is not False or state.get('consumer_pending') is not False or state.get('fault') is not False or pool.get('outstanding_leases')!=0 or pool.get('slots',5)>4 or pool.get('leases_issued')!=pool.get('retire_submissions'):raise ValueError('Lease/consumer teardown incomplete')
            if control=='foreign' and state.get('deferred_queue_leases',0)<1:raise ValueError('Foreign-queue lease not retained and retired on its producer queue')
            if control=='cancel' and (state.get('stopped') is not True or state.get('pairs')!=0):raise ValueError('Terminal stop did not cancel the partial source')
            pairs=[];exact=0;centers_by_request={}
            for path in sorted(directory.glob('pose-pair-*.json')):
                d=json.loads(path.read_text());raw=path.with_suffix('.pixels').read_bytes();size=d['width']*d['height']*4
                if len(raw)!=size*2 or d.get('mode')!='successive_diagnostic' or d.get('actual_xr_pose_source') is not False or d.get('xr_submission') is not False or d.get('whole_scene_verified') is not False or d.get('common_scene_time_known') is not False:raise ValueError('Diagnostic claim/size invalid')
                if d['diagnostic_sample']!=(directory/('pose-input-'+str(d['request'])+'.txt')).read_text():raise ValueError('Original sample retagged')
                centers={}
                for eye,item in enumerate(d['eyes']):
                    camera=item['camera_provenance'];copy=camera['copy_submit'];draw=camera['selected_draw_interval']
                    if item['eye']!=eye or copy['status']!='uniform' or draw['status']!='uniform' or copy['packet_abi_bytes_hex']!=draw['packet_abi_bytes_hex'] or copy['packet_sequence']!=d['request']+1 or copy['packet_eye']!=eye or not camera['same_diagnostic_packet']:raise ValueError('Uniform original packet absent')
                    values=eye_control.FORMAT.unpack(bytes.fromhex(copy['packet_abi_bytes_hex']));expected_x=(-((.125 if d['request']==1 else .25)) if getattr(a,'hold_view',False) else ((.064 if eye==0 else -.064)-(0 if d['request']==1 else .125)))*(.1 if getattr(a,'calibrated',False) else 1)
                    if getattr(a,'calibrated',False) and (values[1]!=2 or values[3]!=5 or abs(values[-1]-.1)>1e-6):raise ValueError('Metric source-depth policy lost')
                    if abs(values[11]-expected_x)>1e-6:raise ValueError('Requested game-view translation was not queued')
                    image=raw[eye*size:(eye+1)*size];reference=(directory/('source-frame-'+str(item['present']-1)+'.pixels')).read_bytes()
                    if image!=reference:raise ValueError('Actual GPU consumer differs from original rendered pixels')
                    if getattr(a,'fragment_packet_check',False) and any(image[i]>0 for i in range(0,len(image),4)):raise ValueError('Fragment packet differs from vertex packet')
                    exact+=size
                    for channel in (1,2):
                        xs=[i//4%d['width'] for i in range(0,len(image),4) if image[i+channel]>200]
                        if not xs:raise ValueError('Empty scene accepted')
                        centers[(eye,channel)]=sum(xs)/len(xs)
                if d['eyes'][1]['present']<=d['eyes'][0]['present'] or d['eyes'][1]['capture_sequence']<=d['eyes'][0]['capture_sequence']:raise ValueError('Invented common scene instant')
                disparities=[centers[(0,c)]-centers[(1,c)] for c in (1,2)]
                if getattr(a,'hold_view',False):
                    if any(v!=0 for v in disparities):raise ValueError('Held view was not monoscopic')
                elif disparities[1]<=0 or not 1.7<disparities[0]/disparities[1]<2.3:raise ValueError('Depth-dependent geometry disparity absent')
                centers_by_request[d['request']]=centers
                if getattr(a,'hold_view',False) and d['request']==2:
                    receipt=d.get('preceding_fixed_view');
                    if not receipt or receipt['camera']['packet_sequence']!=2 or receipt['same_requested_mapping'] or receipt['observed_present_intervals']<2 or receipt['longest_last_present']<=receipt['longest_first_present']:raise ValueError('Prior held mapping/provenance missing')
                pairs.append({'request':d['request'],'packet_sequence':d['packet_sequence'],'source_presents':[e['present'] for e in d['eyes']],'source_capture_sequences':[e['capture_sequence'] for e in d['eyes']],'disparities_pixels':disparities,'exact_original_pixel_bytes':size*2,'preceding_fixed_view':d.get('preceding_fixed_view')})
            if not short and (len(pairs)!=2 or (directory/'source-frame-0.pixels').read_bytes()!=(directory/'source-frame-63.pixels').read_bytes()):raise ValueError('Pairs absent or original pixels not restored')
            if getattr(a,'hold_view',False) and not short:
                first=next(directory.glob('pose-pair-*-801-1.pixels')).read_bytes();first=first[:len(first)//2]
                for frame in (16,30,35):
                    if (directory/('source-frame-'+str(frame)+'.pixels')).read_bytes()!=first:raise ValueError('View not held between capture requests')
                if state.get('fixed_view_held') is not False or state.get('stopped') is not True:raise ValueError('Stop failed to release held view')
            shifts=[]
            if not short:
                for eye in (0,1):
                    delta=[centers_by_request[2][(eye,c)]-centers_by_request[1][(eye,c)] for c in (1,2)]
                    if delta[1]>=0 or not 1.7<delta[0]/delta[1]<2.3:raise ValueError('Second diagnostic pose did not move depth-dependent geometry')
                    shifts.append(delta)
            cases.append({'case':name,'pairs':pairs,'frames':frames,'original_submissions':submissions,'original_draws':frames*2,'exact_original_pixel_bytes':exact,'view_translation_pixels_by_eye':shifts,'registered_graphics_queues':destroy[0]['camera_updates']['registered_queues'],'foreign_acquisition_fence_waited':control=='foreign','driver':state,'producer':pool,'validation_errors':0});print(name+': passed',flush=True)
    legacy=None
    if not getattr(a,'skip_legacy',False):
        legacy=present_validate.validate(argparse.Namespace(output=a.output/'legacy-wsi',fixture=a.fixture,compiler=a.compiler,layer=a.layer,validation_layer=a.validation_layer,runtime=a.runtime));legacy['forwarding_tests_passed']=True;probe.write_json(a.layer/'sequential-validation.json',legacy)
    proof={'schema_version':1,'synthetic_scene':True,'actual_wsi_loader':True,'actual_capture_producer':True,'actual_gpu_image_consumer':True,'actual_xr_pose_source':False,'diablo_rendered':False,'actual_steamvr_submission':False,'whole_scene_verified':False,'common_scene_time_known':False,'khronos_synchronization_validation_loaded':True,'validation_errors':0,'portable':portable,'fragment_packet_check':getattr(a,'fragment_packet_check',False),'layer_sha256':probe.digest(a.layer/vulkan_trace.LIBRARY),'fixture_sha256':probe.digest(a.fixture),'cases':cases,'legacy_wsi':legacy}
    probe.write_json(a.output/'proof.json',proof);return proof


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','compiler','layer','validation-layer','unit','forwarding','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    p.add_argument('--calibrated',action='store_true')
    p.add_argument('--hold-view',action='store_true')
    p.add_argument('--fragment-packet-check',action='store_true')
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
