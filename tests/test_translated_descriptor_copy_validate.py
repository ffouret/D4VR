"""Portable synthetic controls; no GPU, Wine or external commands."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC=importlib.util.spec_from_file_location('translated_copy',Path(__file__).resolve().parents[1]/'tools/translated_descriptor_copy_validate.py')
v=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(v)


def encoded(x):return (json.dumps(x,separators=(',',':'))+'\n').encode()


def integer_pixels(image,sampler):
    # Separately derived integer coordinates, no calls to the production oracle.
    # floor(8*((p+.5)/extent +/- 1/8)) = floor((8p+4)/extent) +/-1.
    out=bytearray()
    for y in range(144):
        iy=(8*y+4)//144-1;iy=iy%8 if sampler else max(0,min(7,iy))
        for x in range(256):
            ix=(8*x+4)//256+1;ix=ix%8 if sampler else max(0,min(7,ix))
            rgba=(17+13*ix,29+19*iy,41+7*ix+11*iy,255) if image==0 else (233-11*ix,211-17*iy,197-5*ix-9*iy,83+7*ix+11*iy)
            out.extend((rgba[2],rgba[1],rgba[0],rgba[3]))
    return bytes(out)


def query_log(run,enabled=True,queries=None,**changes):
    common=dict(schema_version=1,run_id=run,**{k:False for k in v.FALSE_FACETS})
    header=dict(common,event='observer_header',scope='owned_translated_original_get_descriptor',enabled=enabled,phase_source='file',limits=dict(events=256,events_per_case=32,output_bytes=256,phase_file_bytes=256),transport_scope='owned_single_loader_chain',route_capacity=32)
    queries=[] if queries is None else queries
    hist=[0]*9
    for q in queries:hist[q['phase_before']['case_ordinal']]+=1
    footer=dict(common,event='observer_footer',calls_seen=len(queries),original_returns=len(queries),events_retained=len(queries),dropped_events=0,skipped_bootstrap_queries=0,unknown_phase_queries=0,phase_unavailable=0,phase_changed=0,output_unavailable=0,typed_input_unknown=0,faulted=False,active_instances=0,active_devices=0,calls_by_case=hist,returns_by_case=hist.copy(),events_by_case=hist.copy(),dropped_by_case=[0]*9,coverage_complete=enabled,transport_overflow=False,transport_chain_mismatch=False)
    footer.update(changes)
    return b''.join(encoded(row) for row in [header,*queries,footer])


def query(run,ordinal=1,case=3,phase=2):
    return dict(schema_version=1,event='get_descriptor',run_id=run,**{k:False for k in v.FALSE_FACETS},ordinal=ordinal,device=123,descriptor_type=2,info_sType=1000316004,info_present=True,chain_present=False,data_size=4,output_pointer=987,original_returned=True,return_kind='void',typed_input=dict(known=True,kind='image_view',reason='none',raw=888,image_view=888,sampler=0,image_layout=5,address=0,range=0,format=0),phase_before=dict(status='known',case_ordinal=case,api_phase=phase),phase_after=dict(status='known',case_ordinal=case,api_phase=phase),phase_unchanged=True,output_captured=True,output_hex='01020304')


def synthetic(base):
    root=base/'capture';root.mkdir();source=base/'source';source.mkdir();(source/'owned.cpp').write_text('// synthetic fixture only\n')
    artifacts={}
    for i,role in enumerate(v.ARTIFACT_ROLES):
        path=base/(role+'.bin');data=b'DXBC'+bytes([i])*64 if role.endswith('shader') else (role+' synthetic').encode();path.write_bytes(data)
        artifacts[role]=dict(path=str(path),bytes=len(data),sha256=v.digest(data))
    pins=dict(schema_version=1,source_root=str(source),source_sha256={'owned.cpp':v.file_sha(source/'owned.cpp')},artifacts=artifacts,capture_root=str(root),capture_sha256={},runs={})
    parent=dict(schema_version=1,owned_fixture=True,game_rendered=False,runs={})
    desc=lambda dim,w,h,fmt,layout,flags:dict(dimension=dim,alignment=0,width=w,height=h,depth_or_array_size=1,mip_levels=1,format=fmt,sample_count=1,sample_quality=0,layout=layout,flags=flags)
    resources=[]
    for i,role in enumerate(('texture_A','upload_A','texture_B','upload_B','render_target','readback')):
        if role.startswith('texture'):heap,state,d=1,1024,desc(3,8,8,28,0,0)
        elif role.startswith('upload'):heap,state,d=2,2755,desc(1,2048,1,0,1,0)
        elif role=='render_target':heap,state,d=1,2048,desc(3,256,144,87,0,1)
        else:heap,state,d=3,1024,desc(1,147456,1,0,1,0)
        resources.append(dict(role=role,raw=1000+i,generation=1,heap_type=heap,initial_state=state,description=d))
    heaps=[]
    for i,(role,t,n,visible) in enumerate((('cpu_srv',0,2,False),('visible_srv',0,1,True),('visible_sampler',1,2,True),('rtv',2,1,False))):heaps.append(dict(role=role,raw=2000+i,generation=1,type=t,count=n,flags=int(visible),cpu_start=10000+i*1000,gpu_start=100000+i*1000 if visible else None,increment=32))
    rr={r['role']:r for r in resources};hh={r['role']:r for r in heaps};device=1234
    for mode in ('off','on'):
        run=('1' if mode=='off' else '2')*32;folder=root/mode;folder.mkdir()
        pr=dict(run_id=run,reported_module_paths={r:'Z:\\owned\\'+r+'.dll' for r in v.MODULE_ROLES},reported_shader_paths={r:'Z:\\owned\\'+r+'.dxil' for r in ('vertex_shader','pixel_shader')},reported_phase_file='Z:\\owned\\'+mode+'-phase.txt');pins['runs'][mode]=pr
        events=[];case=0
        def call(api,args,phase=0,kind='void'):
            ordinal=len(events)//2+1
            for edge in ('before','after'):events.append(dict(ordinal=ordinal,run_id=run,case_ordinal=case,api_phase=phase,api=api,edge=edge,returns=kind,returned=edge=='after',hresult=0 if edge=='after' and kind=='HRESULT' else None,value=None,args=copy.deepcopy(args)))
        def submit(fence):
            call('ExecuteCommandLists',dict(queue=777,command_lists=[888]),5);call('Signal',dict(fence_value=fence),5,'HRESULT')
        def create(image,heap,slot,role):call('CreateShaderResourceView',dict(device=device,resource=rr['texture_B' if image else 'texture_A']['raw'],resource_generation=1,heap=heap['raw'],heap_generation=1,slot=slot,cpu_handle=heap['cpu_start']+slot*heap['increment'],role=role,description=dict(format=28,dimension=4,component_mapping=5768,most_detailed_mip=0,mip_levels=1,plane_slice=0,min_lod_clamp=0)),1)
        def cp(image,ranges=False,late=False):
            args=dict(device=device,heap_type=0,source_heap=hh['cpu_srv']['raw'],source_generation=1,source_slot=image,destination_heap=hh['visible_srv']['raw'],destination_generation=1,destination_slot=0)
            dst=hh['visible_srv']['cpu_start'];src=hh['cpu_srv']['cpu_start']+image*32
            if ranges:args.update(destination_starts=[dst],destination_sizes=[1],source_starts=[src],source_sizes=[1])
            else:args.update(count=1,destination_cpu_handle=dst,source_cpu_handle=src)
            call('CopyDescriptors' if ranges else 'CopyDescriptorsSimple',args,3 if ranges else 4 if late else 2)
        for api,args in [('D3D12CreateDevice',dict(adapter=None,minimum_feature_level=45056)),('CheckFeatureSupportShaderModel',dict(requested=96)),('CheckFeatureSupportFormat',dict(format=28,required_support1=0x220)),('CheckFeatureSupportFormat',dict(format=87,required_support1=0x4020)),('CreateCommandQueue',dict(type=0,priority=0,flags=0,node_mask=0)),('CreateCommandAllocator',dict(type=0)),('CreateCommandList',dict(node_mask=0,type=0,initial_pipeline=None)),('CreateFence',dict(initial_value=0,flags=0))]:call(api,args,kind='HRESULT')
        for r in resources:call('CreateCommittedResource',dict(device=device,role=r['role'],heap_type=r['heap_type'],heap_flags=0,creation_node_mask=1,visible_node_mask=1,initial_state=r['initial_state'],description=r['description']),kind='HRESULT')
        for r in heaps:call('CreateDescriptorHeap',dict(device=device,role=r['role'],type=r['type'],count=r['count'],flags=r['flags'],node_mask=0),kind='HRESULT')
        create(0,hh['cpu_srv'],0,'cpu_source_A');create(1,hh['cpu_srv'],1,'cpu_source_B')
        for i in (0,1):call('CreateSampler',dict(heap=hh['visible_sampler']['raw'],generation=1,slot=i,cpu_handle=hh['visible_sampler']['cpu_start']+i*32,filter=0,address_mode=1 if i else 3,min_lod=0,max_lod=0))
        call('CreateRenderTargetView',dict(resource=rr['render_target']['raw'],cpu_handle=hh['rtv']['cpu_start'],description=None))
        for api,args in [('CreateRootSignature',dict(node_mask=0,version=1,parameters=3)),('CreateGraphicsPipelineState',dict(format=87,sample_count=1,blend_enable=False,logic_op_enable=False,color_write_mask=15)),('CloseUploadCommandList',{})]:call(api,args,kind='HRESULT')
        submit(1);case_rows=[]
        for case,(name,(image,sampler,method)) in enumerate(v.CASES.items(),1):
            if method in ('direct','after_bind'):create(0 if method=='after_bind' else image,hh['visible_srv'],0,'selected_visible_slot')
            else:cp(image,ranges=method=='ranges')
            call('ResetAllocator',{},kind='HRESULT');call('ResetCommandList',{},kind='HRESULT')
            call('SetDescriptorHeaps',dict(heaps=[hh['visible_srv']['raw'],hh['visible_sampler']['raw']],generations=[1,1]))
            for param,slot,heap in ((0,0,hh['visible_srv']),(1,sampler,hh['visible_sampler'])):call('SetGraphicsRootDescriptorTable',dict(parameter=param,heap=heap['raw'],generation=1,slot=slot,gpu_handle=heap['gpu_start']+slot*32))
            call('DrawInstanced',dict(vertex_count=3,instance_count=1,first_vertex=0,first_instance=0));call('CloseCommandList',{},kind='HRESULT')
            if method=='after_bind':cp(1,late=True)
            submit(case+1);call('MapReadback',dict(resource=rr['readback']['raw'],range=[0,147456]),6,'HRESULT')
            raw=integer_pixels(image,sampler);(folder/(name+'.bgra')).write_bytes(raw);constants=struct.pack('<ff',.125,-.125);(folder/(name+'.constants.bin')).write_bytes(constants)
            case_rows.append(dict(name=name,case_ordinal=case,texture_index=image,sampler_index=sampler,method={'direct':'direct_create','simple':'copy_simple','ranges':'copy_ranges','after_bind':'copy_after_bind'}[method],raw_file=name+'.bgra',bytes=147456,sha256=v.digest(raw),constants_file=name+'.constants.bin',constants_sha256=v.digest(constants),producer_fence_value=case+1,producer_completed=True))
        journal=dict(schema_version=1,run_id=run,owned_fixture=True,before_after_original_D3D12_calls=True,events=events);data=encoded(journal);(folder/'d3d12-events.json').write_bytes(data)
        module_snapshots=[]
        for point in ('before_device','before_cases','after_cases','after_fixture_release'):
            modules=[]
            for i,name in enumerate(('fixture','d3d12.dll','d3d12core.dll','dxgi.dll','winevulkan.dll','vulkan-1.dll')):
                role={'fixture':'fixture','d3d12.dll':'d3d12','d3d12core.dll':'d3d12core','dxgi.dll':'dxgi'}.get(name);loaded=role is not None and not(point=='before_device' and role in ('d3d12core','dxgi'))
                modules.append(dict(name=name,loaded=loaded,module=3000+i if loaded else 0,path=pr['reported_module_paths'][role] if loaded else None))
            module_snapshots.append(dict(point=point,modules=modules))
        manifest=dict(schema_version=1,translated_descriptor_copy_passed=True,run_id=run,owned_fixture=True,game_rendered=False,width=256,height=144,target_DXGI_format=87,texture_DXGI_format=28,raw_bytes_per_case=147456,readback_row_pitch=1024,readback_total_bytes=147456,features=dict(shader_model_requested=96,shader_model_supported=96,format_support=[dict(DXGI_format=f,support1=bits,support2=0,required_support1=bits) for f,bits in ((28,0x220),(87,0x4020))]),UV=dict(source='SV_Position.xy',divisor=[256,144],offset=[.125,-.125],flip_y=False,LOD=0),raster=dict(viewport=[0,0,256,144,0,1],scissor=[0,0,256,144],blend_enable=False,logic_op_enable=False,color_write_mask=15,sample_count=1),shaders=[dict(stage=stage,path=pr['reported_shader_paths'][role],entry=entry,sha256=artifacts[role]['sha256'],bytes=artifacts[role]['bytes']) for role,stage,entry in (('vertex_shader','vertex','VSMain'),('pixel_shader','pixel','PSMain'))],resources=copy.deepcopy(resources),heaps=copy.deepcopy(heaps),module_snapshots=module_snapshots,events_file='d3d12-events.json',events_sha256=v.digest(data),api_phase=copy.deepcopy(v.PHASES),case_phase_file=pr['reported_phase_file'],phase_is_CPU_label_not_exclusive_GPU_or_translator_causality=True,cases=case_rows,D3D12_void_calls_have_no_HRESULT=True,game_resource_or_GPU_identity_claimed=False)
        (folder/'manifest.json').write_bytes(encoded(manifest));(root/(mode+'-query.jsonl')).write_bytes(query_log(run,mode=='on'))
        marker='Validation layers seem to be loaded correctly.';(root/(mode+'.log')).write_text(marker+'\n')
        modules=[]
        for role in v.MODULE_ROLES:
            a=artifacts[role];r=dict(role=role,mapping_verified=True)
            for suffix in ('before','after'):r.update({'reported_'+suffix:pr['reported_module_paths'][role],'host_realpath_'+suffix:a['path'],'bytes_'+suffix:a['bytes'],'sha256_'+suffix:a['sha256']})
            modules.append(r)
        parent['runs'][mode]=dict(run_id=run,fixture_exit_code=0,modules=modules,validation=dict(log_files=[mode+'.log'],activation_marker=marker,sentinel_count=1,non_sentinel_errors=0))
    (root/'parent-run.json').write_bytes(encoded(parent));pins['capture_sha256']=v.capture_files(root)
    return root,pins


class Validation(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.base=Path(self.temp.name).resolve();self.root,self.pins=synthetic(self.base)
        self.blockers=[patch.object(subprocess,k,side_effect=AssertionError('External process forbidden')) for k in ('Popen','run','call','check_call','check_output')]
        for p in self.blockers:p.start();self.addCleanup(p.stop)
    def repin(self):self.pins['capture_sha256']=v.capture_files(self.root)
    def manifest(self,mode='on'):return json.loads((self.root/mode/'manifest.json').read_bytes())
    def save_manifest(self,m,mode='on'):(self.root/mode/'manifest.json').write_bytes(encoded(m));self.repin()
    def journal(self):return json.loads((self.root/'on/d3d12-events.json').read_bytes())
    def save_journal(self,j):
        data=encoded(j);(self.root/'on/d3d12-events.json').write_bytes(data);m=self.manifest();m['events_sha256']=v.digest(data);self.save_manifest(m)
    def reject(self):
        with self.assertRaises((ValueError,TypeError,KeyError)):v.audit_capture(self.root,self.pins)
    def test_complete_independent_integer_reference(self):
        r=v.audit_capture(self.root,self.pins);self.assertEqual(r['readbacks'],16);self.assertEqual(r['readback_bytes'],2359296)
        self.assertFalse(r['current_contents_at_draw_known']);self.assertTrue(r['observer']['on']['bounded_phase_absence_usable']);self.assertFalse(r['observer']['off']['bounded_phase_absence_usable'])
    def test_four_real_channel_corruptions_even_rehashed(self):
        m=self.manifest();path=self.root/'on/direct_A_clamp.bgra';original=path.read_bytes()
        for channel in range(4):
            bad=bytearray(original);bad[4*345+channel]^=1;path.write_bytes(bad);m['cases'][0]['sha256']=v.digest(bad);self.save_manifest(m);self.reject()
        path.write_bytes(original)
    def test_wrong_image_and_sampler_oracles(self):
        self.assertGreater(v.compare_pixels(integer_pixels(0,0),'direct_B_clamp')['mismatched_BA_bytes'],0)
        self.assertGreater(v.compare_pixels(integer_pixels(1,0),'direct_B_repeat')['mismatched_bytes'],0)
    def test_y_flip_rejected(self):
        raw=integer_pixels(0,0);flipped=b''.join(raw[y*1024:(y+1)*1024] for y in reversed(range(144)));self.assertGreater(v.compare_pixels(flipped,'direct_A_clamp')['mismatched_bytes'],0)
    def test_rehashed_metadata_format(self):m=self.manifest();m['target_DXGI_format']=44;self.save_manifest(m);self.reject()
    def test_fence_false(self):m=self.manifest();m['cases'][6]['producer_completed']=False;self.save_manifest(m);self.reject()
    def test_wrong_constants_rehashed(self):
        p=self.root/'on/direct_B_clamp.constants.bin';p.write_bytes(struct.pack('<ff',0,0));m=self.manifest();m['cases'][1]['constants_sha256']=v.file_sha(p);self.save_manifest(m);self.reject()
    def test_generation_retag(self):m=self.manifest();m['heaps'][1]['generation']=2;self.save_manifest(m);self.reject()
    def test_heap_alias(self):m=self.manifest();m['heaps'][1]['cpu_start']=m['heaps'][0]['cpu_start'];self.save_manifest(m);self.reject()
    def test_heap_end_overflow(self):m=self.manifest();m['heaps'][1]['cpu_start']=v.MAX_U64;self.save_manifest(m);self.reject()
    def test_upload_total_excludes_final_row_padding(self):
        for total in (1824,1823):
            j=self.journal()
            for row in j['events']:
                if row['api']=='CreateCommittedResource' and row['args']['role'] in ('upload_A','upload_B'):row['args']['description']['width']=total
            self.save_journal(j);m=self.manifest()
            for resource in m['resources']:
                if resource['role'] in ('upload_A','upload_B'):resource['description']['width']=total
            self.save_manifest(m)
            if total==1824:self.assertEqual(v.audit_capture(self.root,self.pins)['readbacks'],16)
            else:self.reject()
    def test_module_path_substitution(self):m=self.manifest();m['module_snapshots'][1]['modules'][2]['path']='Z:\\imposter.dll';self.save_manifest(m);self.reject()
    def test_shader_hash_substitution(self):m=self.manifest();m['shaders'][1]['sha256']='0'*64;self.save_manifest(m);self.reject()
    def test_DXGI_artifact_is_required_and_immutable(self):
        a=self.pins['artifacts'].pop('dxgi');self.reject();self.pins['artifacts']['dxgi']=a
        Path(a['path']).write_bytes(b'imposter DXGI');self.reject()
    def test_DXGI_process_path_and_parent_mapping_required(self):
        m=self.manifest();m['module_snapshots'][1]['modules'][3]['path']='Z:\\imposter\\dxgi.dll';self.save_manifest(m);self.reject()
    def test_DXGI_cannot_be_missing_during_cases(self):
        m=self.manifest();m['module_snapshots'][1]['modules'][3].update(loaded=False,module=0,path=None);self.save_manifest(m);self.reject()
    def test_DXGI_file_attestation_cannot_be_another_module(self):
        p=self.root/'parent-run.json';j=json.loads(p.read_bytes());dxgi=next(r for r in j['runs']['on']['modules'] if r['role']=='dxgi');dxgi['sha256_after']=self.pins['artifacts']['d3d12core']['sha256'];p.write_bytes(encoded(j));self.repin();self.reject()
    def test_parent_host_mapping_substitution(self):
        p=self.root/'parent-run.json';j=json.loads(p.read_bytes());j['runs']['on']['modules'][1]['host_realpath_after']=self.pins['artifacts']['fixture']['path'];p.write_bytes(encoded(j));self.repin();self.reject()
    def test_external_pin_None(self):self.pins['artifacts']['observer']['sha256']=None;self.reject()
    def test_nonce_log_substitution(self):
        (self.root/'on-query.jsonl').write_bytes(query_log('1'*32));self.repin();self.reject()
    def test_raw_log_path_redirection(self):m=self.manifest();m['events_file']='../off/d3d12-events.json';self.save_manifest(m);self.reject()
    def test_symlink(self):
        p=self.root/'on/direct_A_clamp.bgra';data=p.read_bytes();p.unlink();outside=self.base/'outside';outside.write_bytes(data)
        try:p.symlink_to(outside)
        except OSError:self.skipTest('Symlink unavailable')
        self.reject()
    def test_source_substitution(self):(self.base/'source/owned.cpp').write_text('changed');self.reject()
    def test_unpinned_extra_file(self):(self.root/'intruder').write_text('untrusted');self.reject()
    def test_capture_path_parent_traversal(self):self.pins['capture_root']=str(self.base/'other'/'..'/'capture');self.reject()
    def test_duplicate_JSON_and_nonfinite(self):
        for data in (b'{"a":1,"a":2}',b'{"a":NaN}'):
            with self.assertRaises(ValueError):v.loads(data)
    def test_original_void_fake_HRESULT(self):
        j=self.journal();r=next(r for r in j['events'] if r['api']=='CopyDescriptorsSimple' and r['edge']=='after');r['hresult']=0;self.save_journal(j);self.reject()
    def test_copy_source_handle_substitution(self):
        j=self.journal()
        for r in j['events']:
            if r['case_ordinal']==4 and r['api']=='CopyDescriptorsSimple':r['args']['source_slot']=0
        self.save_journal(j);self.reject()
    def test_copy_generation_substitution(self):
        j=self.journal()
        for r in j['events']:
            if r['case_ordinal']==5 and r['api']=='CopyDescriptors':r['args']['source_generation']=2
        self.save_journal(j);self.reject()
    def test_late_copy_before_Close_rejected(self):
        j=self.journal();rows=j['events'];a=next(i for i,r in enumerate(rows) if r['case_ordinal']==7 and r['api']=='CloseCommandList');rows[a:a+4]=rows[a+2:a+4]+rows[a:a+2]
        for i,r in enumerate(rows):r['ordinal']=i//2+1
        self.save_journal(j);self.reject()
    def test_missing_edge(self):j=self.journal();j['events'].pop();self.save_journal(j);self.reject()
    def test_validation_error_VUID(self):
        (self.root/'on.log').write_text('Validation layers seem to be loaded correctly.\nVUID-vkCmdDraw-None-12345\n');self.repin();self.reject()
    def test_valid_query_and_union_forgeries(self):
        run='2'*32;q=query(run);result=v.audit_query_log(query_log(run,queries=[q]),run);self.assertEqual(result['query_records'],1)
        for key,value in [('sampler',77),('address',999)]:
            bad=copy.deepcopy(q);bad['typed_input'][key]=value
            with self.assertRaises(ValueError):v.audit_query_log(query_log(run,queries=[bad]),run)
    def test_query_stronger_claim_and_output_prefix(self):
        run='2'*32
        for mutate in (lambda q:q.update(current_contents_at_draw_known=True),lambda q:q.update(output_hex='01'),lambda q:q.update(info_sType=1)):
            q=query(run);mutate(q)
            with self.assertRaises(ValueError):v.audit_query_log(query_log(run,queries=[q]),run)
    def test_unknown_input_reason_matches_observed_header(self):
        run='2'*32;q=query(run);q['typed_input'].update(known=False,kind='unknown',reason='unknown_chain',raw=0,image_view=0,image_layout=0);q['chain_present']=True
        data=query_log(run,queries=[q],typed_input_unknown=1);self.assertEqual(v.audit_query_log(data,run)['typed_input_unknown'],1)
        q['chain_present']=False
        with self.assertRaises(ValueError):v.audit_query_log(query_log(run,queries=[q],typed_input_unknown=1),run)
    def test_phase_change_cannot_certify_closure(self):
        run='2'*32;q=query(run);q['phase_after']['api_phase']=5;q['phase_unchanged']=False
        with self.assertRaises(ValueError):v.audit_query_log(query_log(run,queries=[q]),run)
        result=v.audit_query_log(query_log(run,queries=[q],phase_changed=1,coverage_complete=False),run);self.assertFalse(result['bounded_phase_absence_usable'])
    def test_pending_BEFORE_is_not_an_observed_unavailable_AFTER(self):
        run='2'*32;q=query(run);q.update(original_returned=False,phase_after=dict(status='missing',case_ordinal=0,api_phase=0),phase_unchanged=False,output_captured=False,output_hex='')
        result=v.audit_query_log(query_log(run,queries=[q],original_returns=0,returns_by_case=[0]*9,phase_unavailable=0,coverage_complete=False),run)
        self.assertFalse(result['coverage_complete']);self.assertFalse(result['bounded_phase_absence_usable']);self.assertEqual(result['query_records'],1)
    def test_overflow_or_fault_cannot_claim_complete(self):
        run='2'*32
        for changes in (dict(faulted=True),dict(active_devices=1),dict(transport_overflow=True),dict(transport_chain_mismatch=True)):
            with self.assertRaises(ValueError):v.audit_query_log(query_log(run,**changes),run)
    def test_truncated_log(self):
        with self.assertRaises(ValueError):v.audit_query_log(query_log('2'*32)[:-1],'2'*32)
    def test_incomplete_window_preserves_pixel_control_without_absence(self):
        p=self.root/'on-query.jsonl';p.write_bytes(query_log('2'*32,faulted=True,coverage_complete=False));self.repin()
        r=v.audit_capture(self.root,self.pins);self.assertEqual(r['readbacks'],16);self.assertFalse(r['bounded_query_window_complete'])
        self.assertTrue(all(not x['no_query_with_these_finite_labels_observed'] for x in r['case_phase_observations']))
    def test_query_budget_and_bootstrap_payload(self):
        run='2'*32
        with self.assertRaises(ValueError):v.audit_query_log(query_log(run,queries=[query(run,i+1) for i in range(33)]),run)
        with self.assertRaises(ValueError):v.audit_query_log(query_log(run,queries=[query(run,case=0)]),run)
    def test_core_samples_never_attest_loader_transport(self):
        run='2'*32;rows=[json.loads(x) for x in query_log(run).splitlines()];rows[0].update(transport_scope='core_CPU_only',route_capacity=0);data=b''.join(encoded(r) for r in rows)
        with self.assertRaises(ValueError):v.audit_query_log(data,run)
        self.assertEqual(v.audit_query_log(data,run,allow_core=True)['transport_scope'],'core_CPU_only')
    def test_non_sentinel_error_callback(self):
        (self.root/'on.log').write_text('Validation layers seem to be loaded correctly.\n123:err:vkd3d_debug_messenger_callback: bad callback\n');self.repin();self.reject()
    def test_cli_fresh_output_and_external_pins_hash(self):
        p=self.base/'pins.json';p.write_bytes(encoded(self.pins));out=self.base/'audit.json'
        with patch('builtins.print'):self.assertEqual(v.main(['--capture',str(self.root),'--pins',str(p),'--pins-sha256',v.file_sha(p),'--output',str(out)]),0)
        with self.assertRaises(ValueError):v.main(['--capture',str(self.root),'--pins',str(p),'--pins-sha256',v.file_sha(p),'--output',str(out)])
        with self.assertRaises(ValueError):v.main(['--capture',str(self.root),'--pins',str(p),'--pins-sha256','0'*64,'--output',str(self.base/'bad.json')])


if __name__=='__main__':unittest.main()
