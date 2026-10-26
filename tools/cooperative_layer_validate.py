#!/usr/bin/env python3
"""Real loader/WSI/Capture/ProjectionSession integration; simulated XR only."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import eye_control
import paired_capture
import probe
import vulkan_trace


def validate(a):
    if a.output.exists():raise ValueError('Use a new private directory')
    a.output.mkdir(mode=0o700,parents=True)
    root=Path(__file__).resolve().parents[1]
    shaders=[]
    for ext in ('vert','frag'):
        path=(a.output/('present.'+ext+'.spv')).resolve()
        p=subprocess.run(paired_capture.host_command()+[str(a.compiler.resolve()),'-V','--target-env','vulkan1.2',str(root/('native/tests/present_eye.'+ext)),'-o',str(path)],capture_output=True,text=True,timeout=40)
        if p.returncode:raise ValueError(p.stderr[-500:])
        if ext=='vert':
            raw=path.read_bytes();words=list(struct.unpack('<'+'I'*(len(raw)//4),raw));marker=b'D4VR_STEREO_ABI_1\0';marker+=b'\0'*(-len(marker)%4);index=5
            while index<len(words) and words[index]&65535 not in (71,72,19,20,21,22):index+=words[index]>>16
            words[index:index]=[((len(marker)//4+1)<<16)|330,*struct.unpack('<'+'I'*(len(marker)//4),marker)];path.write_bytes(struct.pack('<'+'I'*len(words),*words))
        shaders.append(path)
    cases=[]
    controls={'normal':{},'timeout':{'D4VR_MOCK_XR_TIMEOUT':'1'},'stop-source':{'D4VR_MOCK_XR_STOP_SOURCE':'1'},'stop-file':{'D4VR_POSE_FIXTURE_CANCEL':'1'},'short':{'D4VR_POSE_FIXTURE_SHORT':'1'},'foreign':{'D4VR_POSE_FIXTURE_FOREIGN':'1'},'denied-thread':{'D4VR_MOCK_XR_DENY_THREAD':'1'},'denied-queue':{'D4VR_MOCK_XR_OTHER_QUEUE':'1'}}
    if a.only:controls={a.only:controls[a.only]}
    for runtime in (False,True) if a.runtime else (False,):
        for control,extra in controls.items():
            name=control+('-steam-runtime' if runtime else '-native');directory=(a.output/name).resolve();directory.mkdir(mode=0o700)
            variables={'VK_INSTANCE_LAYERS':vulkan_trace.LAYER+':VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH':str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),'VK_LAYER_ENABLES':'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC':'1','VK_LOADER_DEBUG':'layer','D4VR_TRACE_DIR':str(directory),'D4VR_SEQUENTIAL_CAPTURE_DIR':str(directory),'D4VR_GPU_CAMERA_UPDATES':'1','D4VR_COOPERATIVE_XR_DIAGNOSTIC':'1','D4VR_XR_LOADER':str(a.mock.resolve()),'D4VR_MOCK_XR_LAYER_OUTPUT':str(directory),'D4VR_MOCK_XR_CHECK_UPLOAD':'1',**extra}
            for eye,side in enumerate(('left','right')):
                path=directory/(side+'.bin');path.write_bytes(eye_control.encode(eye,separation=.128));path.chmod(0o600);variables['D4VR_PAIR_'+side.upper()+'_FILE']=str(path)
            command=paired_capture.host_command()+['/usr/bin/env']
            unset=('VK_LAYER_PATH','VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','D4VR_TRACE_XR_BINDING','D4VR_LAYER_REPLAY_POC','D4VR_FRAME_TRACE','D4VR_EYE_STATE_FILE','D4VR_POSE_CAPTURE_DIAGNOSTIC','D4VR_GPU_CAPTURE_DRY_RUN','D4VR_POSE_FIXTURE_SHORT','D4VR_POSE_FIXTURE_FOREIGN','D4VR_POSE_FIXTURE_CANCEL','D4VR_MOCK_XR_TIMEOUT','D4VR_MOCK_XR_STOP_SOURCE','D4VR_MOCK_XR_DENY_THREAD','D4VR_MOCK_XR_OTHER_QUEUE','D4VR_MOCK_XR_EXTERNAL_REFERENCE','D4VR_MOCK_XR_FAIL_SESSION','D4VR_MOCK_XR_FAIL_ACQUIRE','D4VR_MOCK_XR_UNTRACKED','D4VR_MOCK_XR_HIDDEN','D4VR_MOCK_XR_STOP','D4VR_MOCK_XR_STOP_IMAGES','D4VR_MOCK_XR_SPACE_CHANGE')
            for key in unset:command+=['-u',key]
            command += [k+'='+v for k,v in variables.items()]
            if runtime:command += [str(a.runtime.resolve()),'--batch','--']
            command += [str(a.fixture.resolve()),*map(str,shaders),str(directory)]
            p=subprocess.run(command,capture_output=True,text=True,timeout=45);log=p.stdout+p.stderr;(directory/'run.log').write_text(log)
            if p.returncode or any(x in log for x in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError(name+': inspect private run.log')
            if not ('Insert instance layer "VK_LAYER_KHRONOS_validation"' in log and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in log):raise ValueError('Validation load not proven')
            rows=[json.loads(line) for path in directory.glob('vulkan-*.jsonl') for line in path.read_text().splitlines()]
            errors=[r for r in rows if r.get('event') in ('presentation_error','camera_update_fatal','paired_pipeline_error','cooperative_xr_error')]
            if errors:raise ValueError(name+': layer error '+str(errors))
            destroy=[r for r in rows if r.get('event')=='device_destroy']
            if len(destroy)!=1:raise ValueError('Missing teardown')
            state=destroy[0].get('cooperative_xr',{});pool=destroy[0].get('pose_producer',{});cameraStats=destroy[0]['camera_updates'];xr=state.get('xr') or {}
            denied=control.startswith('denied-')
            if state.get('active') is not False or state.get('stopped') is not True or state.get('fault') is not denied or state.get('queue_access_denied')!=int(denied) or state.get('finishes')!=state.get('requests'):raise ValueError(name+': lifecycle/guard '+str(state))
            if pool.get('outstanding_leases')!=0 or pool.get('slots',5)>4 or pool.get('reserved_bytes',2**40)>256*1024*1024 or pool.get('leases_issued')!=pool.get('retire_submissions'):raise ValueError('Unretired lease '+str(pool))
            frames=8 if control in ('short','stop-file') else 64
            if destroy[0]['commands']['draw']!=frames*2:raise ValueError('Original draws repeated')
            if 'queue reentry completed' not in log or 'Submit/Submit2 reentry completed' not in log:raise ValueError('Public Vulkan reentry absent')
            if cameraStats['registered_queues']!=5 or cameraStats.get('write_holds')!=0 or cameraStats.get('inhibited_boundaries',0)<2:raise ValueError('Runtime write inhibition not observed '+str(cameraStats))
            if frames==64 and (directory/'source-frame-0.pixels').read_bytes()!=(directory/'source-frame-63.pixels').read_bytes():raise ValueError('Original pipeline pixels not restored after XR stop')
            pairs=[];recorded=[];exact=0
            for path in sorted(directory.glob('xr-pair-*.json')):
                d=json.loads(path.read_text());request=d['request'];centers={};recorded.append(request)
                if d['mode']!='successive_diagnostic' or d['whole_scene_verified'] is not False or d['common_scene_time_known'] is not False or d['display_time']!=request*10000000 or d['packet_sequence']!=request+1:raise ValueError('Provenance promotion')
                fields=d['diagnostic_sample'].split();f32=lambda value:struct.unpack('<f',struct.pack('<f',value))[0]
                expected=[0,0,0,1,0,0,0,-f32(.8),f32(.8),-f32(.8),f32(.8)]
                if fields[0]!='D4VR_DIAGNOSTIC_POSE_1' or list(map(int,fields[1:5]))!=[d['session'],request,d['display_time'],15]:raise ValueError('Original runtime identity/flags changed')
                for eye in (0,1):
                    view=list(map(float,fields[5+eye*11:16+eye*11]));expected[4]=f32(f32((request-1)*.025)+f32(.032 if eye else -.032))
                    if view!=expected:raise ValueError('Original runtime view/FOV changed')
                identity=[1.0 if i//4==i%4 else 0.0 for i in range(16)]
                if list(map(float,fields[27:43]))!=identity or list(map(float,fields[43:]))!=[.1,100.0]:raise ValueError('Original anchor/clip changed')
                delivered=[(directory/f'xr-request-{request}-eye-{eye}.pixels').exists() for eye in (0,1)]
                if delivered==[False,False] and control in ('short','stop-file'):continue
                if delivered!=[True,True]:raise ValueError('Recorded receipt without complete runtime submission')
                for eye,item in enumerate(d['eyes']):
                    camera=item['camera_provenance'];copy=camera['copy_submit'];draw=camera['selected_draw_interval'];raw=(directory/f'xr-request-{request}-eye-{eye}.pixels').read_bytes();reference=(directory/f'source-frame-{item["present"]-1}.pixels').read_bytes()
                    if raw!=reference:raise ValueError('XR pixels differ from original application frame')
                    if copy['status']!='uniform' or copy['packet_abi_bytes_hex']!=draw.get('packet_abi_bytes_hex') or copy['packet_sequence']!=request+1 or copy['packet_eye']!=eye:raise ValueError('Original camera packet mismatch')
                    values=eye_control.FORMAT.unpack(bytes.fromhex(copy['packet_abi_bytes_hex']));expected=(.032 if eye==0 else -.032)-(request-1)*.025
                    if abs(values[11]-expected)>1e-6:raise ValueError('XR pose not applied to view')
                    exact+=len(raw)
                    for channel in (1,2):
                        xs=[i//4%256 for i in range(0,len(raw),4) if raw[i+channel]>200]
                        if not xs:raise ValueError('Empty geometry')
                        centers[(eye,channel)]=sum(xs)/len(xs)
                disparities=[centers[(0,c)]-centers[(1,c)] for c in (1,2)]
                if disparities[1]<=0 or not 1.7<disparities[0]/disparities[1]<2.3:raise ValueError('Depth-dependent disparity missing')
                if d['eyes'][1]['present']<=d['eyes'][0]['present']:raise ValueError('Fabricated simultaneous scene')
                pairs.append({'request':request,'packet_sequence':d['packet_sequence'],'source_presents':[i['present'] for i in d['eyes']],'disparity_pixels':disparities})
            if control in ('normal','timeout','foreign') and destroy[0]['submissions']!=frames*(6 if control=='foreign' else 5)+7:raise ValueError('Public original/runtime submit count changed')
            if control in ('normal','timeout','foreign') and (len(pairs)!=2 or state['pairs']!=2 or xr['replayed_xr_frames']!=2):raise ValueError(name+': missing full XR cycle '+str(state))
            if control in ('stop-source','denied-thread','denied-queue') and pairs:raise ValueError('Cancelled source submitted')
            if control=='timeout' and xr['swapchain_wait_timeouts']!=8:raise ValueError('Timeout cycle not exercised')
            if control=='foreign' and state['deferred_queue_leases']<1:raise ValueError('Foreign producer not exercised')
            cases.append({'case':name,'frames':frames,'original_draws':frames*2,'original_pixels_restored':frames==64,'pairs':pairs,'recorded_receipts':recorded,'exact_xr_original_pixel_bytes':exact,'coordinator':state,'producer':pool,'camera_updates':cameraStats,'validation_errors':0});print(name+': passed',flush=True)
    proof={'schema_version':1,'synthetic_scene':True,'actual_wsi_loader':True,'actual_capture_producer':True,'actual_projection_session':True,'xr_runtime_simulated':True,'diablo_rendered':False,'actual_steamvr_submission':False,'whole_scene_verified':False,'common_scene_time_known':False,'validation_errors':0,'layer_sha256':probe.digest(a.layer/vulkan_trace.LIBRARY),'fixture_sha256':probe.digest(a.fixture),'mock_sha256':probe.digest(a.mock),'cases':cases}
    probe.write_json(a.output/'proof.json',proof);return proof

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','compiler','layer','validation-layer','mock','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path);p.add_argument('--only')
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
