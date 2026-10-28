"""Independent causal inputs for the supplied CPU bind relation.

Only explicit --native-wire invokes the new CPU mock fixture. Discovery and
all mutation tests are pure Python: no files, process, driver or old pixels.
"""
from __future__ import annotations
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import typed_copy_bind_validate as model
import typed_copy_getter_validate as getters
import test_typed_copy_source_validate as old_seeds

RUN='abcdef0123456789abcdef0123456789'
CLAIMS=('input_facts_attested','source_runtime_relation_known','public_device_create_result_known',
'current_contents_at_draw_known','unchanged_since_copy_proven','sampled_resource_GPU_read_proven',
'renderer_owner_known','backing_known','global_API_absence_proven','bind_runtime_proven',
'generic_PE_to_Unix_resource_mapping','exclusive_phase_attribution_proven','old_process_or_backing_gate_repaired',
'loaded_binary_source_equivalence_proven')
def ref(raw,generation):return dict(raw=raw,generation=generation)
def sha(b):return hashlib.sha256(b).hexdigest()
def text(v):return json.dumps(v,separators=(',',':'),sort_keys=True)
def source_wires():
    # Independent, finite SOURCE timeline. Source token/gen namespaces are not APP.
    w=old_seeds.wire();w.update(schema_version=2,purpose='owned_translator_original_typed_COM_getter_history',
      source_runtime_relation_known=False,same_run_bind_provenance_known=False,getter_to_bind_relation_known=False)
    s=w['native_wire']['snapshot'];s['at_transition']=27;w['native_wire']['producer']['clock']=27
    w['source'].update(source_calls=12,source_returns=12,observer_events=3)
    desc=copy.deepcopy(s['events'][0]['before']['description'])
    s['events'][2].update(api='CreateShaderResourceView',before=dict(device=ref(71,1),resource=ref(101,1),
      destination=copy.deepcopy(s['selected_destination']),description=desc),
      phase_before=dict(case_ordinal=1,api_phase=1),phase_after=dict(case_ordinal=1,api_phase=1))
    for i,(event,row) in enumerate(zip(s['events'],w['source']['mutations'])):
        event.update(entry=21+2*i,**{'return':22+2*i});row.update(entry=event['entry'],**{'return':event['return']})
    rows=[]
    for ident,kind,entry,end,obj,pointer,value in (
      (6,'increment',11,12,ref(71,1),None,32),(7,'cpu_start',13,14,ref(81,1),101,901),
      (8,'cpu_start',16,17,ref(82,2),102,1601),(9,'gpu_start',19,20,ref(82,2),103,5001)):
        inc=kind=='increment'
        rows.append(dict(id=ident,epoch=7,kind=kind,entry=entry,**{'return':end},object_kind='device' if inc else 'heap',
          source_object=obj,parent_device=ref(71,1),argument_heap_type=0 if inc else None,
          caller_output_pointer=None if inc else pointer,return_kind='UINT32' if inc else 'structure_pointer',
          returned_pointer=None if inc else pointer,value_known=True,value=value,
          lifetime_valid_through_return=True,arithmetic_eligible=True,reason='none'))
    w['getters']=dict(calls=4,returns=4,ignored_unselected=0,ignored_faulted=0,dropped=0,
      publication_policy='latest_eligible_RETURN_before_publication',
      eligibility_scope='original_RETURN_operand_only_not_heap_span_or_bind',records=rows)
    w['publications']=[dict(id=i,kind='observer_heap_facts_publication',original_API=False,transition=t,epoch=7,
      heap=ref(raw,gen),device=ref(71,1),heap_count=count,cpu_getter_id=cpu,increment_getter_id=6,
      span_arithmetic_valid=True,heap_facts_called=True,published=True)
      for i,t,raw,gen,count,cpu in ((1,15,81,1,2,7),(2,18,82,2,1,8))]
    w['cutoff']=dict(kind='observer_capture_cutoff',original_API=False,transition=27,epoch=7)
    two=copy.deepcopy(w);two['sequence']=two['native_wire']['sequence']=2;two['source'].update(source_calls=16,source_returns=16,observer_events=4,case_ordinal=2)
    ts=two['native_wire']['snapshot'];ts['at_transition']=36;two['native_wire']['producer']['clock']=36;two['cutoff']['transition']=36
    for j in range(3):
        event=copy.deepcopy(ts['events'][j]);event.update(call_id=4+j,entry=28+2*j,**{'return':29+2*j})
        event['phase_before']['case_ordinal']=event['phase_after']['case_ordinal']=2;ts['events'].append(event)
    ts['events'].append(dict(call_id=7,entry=34,**{'return':35},api='CopyDescriptorsSimple',return_kind='void',
      original_returned=True,result=None,phase_before=dict(case_ordinal=2,api_phase=2),phase_after=dict(case_ordinal=2,api_phase=2),
      before=dict(device=ref(71,1),count=1,heap_type=0,source=old_seeds.slot((81,1),1,933),destination=old_seeds.slot((82,2),0,1601))))
    two['source']['mutations']=[dict(call_id=e['call_id'],variant=0,entry=e['entry'],**{'return':e['return']}) for e in ts['events']]
    return [w,two]


def seed():
    source=source_wires()
    receipt=[dict(sequence=i+1,receipt_token=900+i,wire_text=text(w),wire_sha256=sha(text(w).encode())) for i,w in enumerate(source)]
    events=[];clock=1000;ordinal=100;case=0
    def call(api,args,kind,result):
        nonlocal clock,ordinal
        ordinal+=1;clock+=1;entry=clock
        for edge,returned,output in (('before',False,None),('after',True,result)):
            if returned:clock+=1
            events.append(dict(ordinal=ordinal,transition=clock,api=api,edge=edge,case_ordinal=case,
              return_kind=kind,args=copy.deepcopy(args),original_returned=returned,result=copy.deepcopy(output)))
        return dict(ordinal=ordinal,entry=entry,**{'return':clock})
    def hr(api,args,output=None):return call(api,args,'HRESULT',dict(hresult=0,output=output))
    dev,a,b,cpu,visible,allocator,cmd,queue,fence,rb=[ref(r,g) for r,g in ((71,41),(101,51),(102,52),(81,61),(82,62),(91,71),(92,81),(93,91),(94,101),(95,111))]
    texture=dict(dimension=3,width=8,height=8,depth_or_array_size=1,mip_levels=1,format=28,sample_count=1,flags=0)
    objects=dict(device=hr('D3D12CreateDevice',{},dev),
      resource_A=hr('CreateCommittedResource',dict(device=dev,description=texture),a),
      resource_B=hr('CreateCommittedResource',dict(device=dev,description=texture),b),
      cpu_heap=hr('CreateDescriptorHeap',dict(device=dev,description=dict(heap_type=0,count=2,flags=0,node_mask=0)),cpu),
      visible_heap=hr('CreateDescriptorHeap',dict(device=dev,description=dict(heap_type=0,count=1,flags=1,node_mask=0)),visible),sampler_heap=None)
    objects['allocator']=hr('CreateCommandAllocator',dict(device=dev,type=0),allocator)
    objects['command']=hr('CreateCommandList',dict(device=dev,allocator=allocator,node_mask=0,type=0,initial_pipeline=0),cmd)
    objects['queue']=hr('CreateCommandQueue',dict(device=dev,description=dict(type=0,priority=0,flags=0,node_mask=0)),queue)
    objects['fence']=hr('CreateFence',dict(device=dev,initial_value=0,flags=0),fence)
    objects['readback']=hr('CreateCommittedResource',dict(device=dev,description=dict(dimension=1,width=32,height=1,depth_or_array_size=1,mip_levels=1,format=0,sample_count=1,flags=0)),rb)
    hr('Close',dict(command=cmd,recording_epoch=0))
    gc=dict(increment=call('GetDescriptorHandleIncrementSize',dict(device=dev,heap_type=0),'uint32',dict(value=32)),
      cpu_heap=call('GetCPUDescriptorHandleForHeapStart',dict(heap=cpu),'CPU_HANDLE',dict(value=901)),
      visible_cpu=call('GetCPUDescriptorHandleForHeapStart',dict(heap=visible),'CPU_HANDLE',dict(value=1601)),
      visible_gpu=call('GetGPUDescriptorHandleForHeapStart',dict(heap=visible),'GPU_HANDLE',dict(value=5001)))
    contexts=[]
    for case in (1,2):
        reset=hr('Reset',dict(command=cmd,recording_epoch=case,allocator=allocator,initial_pipeline=0))
        typed=[]
        for resource,h,index,handle in ((a,cpu,0,901),(b,cpu,1,933),(a,visible,0,1601)):
            typed.append(call('CreateShaderResourceView',dict(device=dev,resource=resource,
              destination=dict(heap=h,index=index,cpu_handle=handle),description=dict(format=28,view_dimension=4,component_mapping=5768,most_detailed_mip=0,mip_levels=1,plane_slice=0,min_lod_clamp_bits=0)),'void',None))
        bind=call('SetDescriptorHeaps',dict(command=cmd,recording_epoch=case,heaps=[visible]),'void',None)
        root=call('SetGraphicsRootDescriptorTable',dict(command=cmd,recording_epoch=case,parameter=0,gpu_handle=5001),'void',None)
        close=hr('Close',dict(command=cmd,recording_epoch=case))
        if case==2:
            typed.append(call('CopyDescriptorsSimple',dict(device=dev,count=1,heap_type=0,
              source=dict(heap=cpu,index=1,cpu_handle=933),destination=dict(heap=visible,index=0,cpu_handle=1601)),'void',None))
        rc=receipt[case-1]
        seal=call('SealSourceReceipt',dict(sequence=case),'control',dict(sequence=case,receipt_token=rc['receipt_token'],wire_sha256=rc['wire_sha256'],wire_bytes=len(rc['wire_text'].encode())))
        execute=call('ExecuteCommandLists',dict(queue=queue,commands=[cmd]),'void',None)
        signal=hr('Signal',dict(queue=queue,fence=fence,value=case))
        event=hr('SetEventOnCompletion',dict(fence=fence,value=case,event=7001))
        wait=call('WaitForSingleObject',dict(event=7001,timeout_ms=(1<<32)-1),'uint32',dict(value=0))
        completed=call('GetCompletedValue',dict(fence=fence),'uint64',dict(value=case))
        mapped=hr('Map',dict(resource=rb,subresource=0,range=[0,32]),dict(pointer=8000+case))
        data=bytes([case])*32
        contexts.append(dict(ordinal=case,name='direct_A_clamp' if case==1 else 'after_bind_B_clamp',source_sequence=case,
          command=cmd,recording_epoch=case,selected_heap=visible,selected_index=0,
          object_calls=copy.deepcopy(objects),getter_calls=copy.deepcopy(gc),typed_calls=typed,
          reset_call=reset,bind_call=bind,root0_call=root,close_call=close,seal_call=seal,execute_call=execute,
          signal_call=signal,event_call=event,wait_call=wait,completed_call=completed,map_call=mapped,
          readback=dict(kind='CPU_mock_not_GPU_pixels',bytes_hex=data.hex(),sha256=sha(data))))
    case=0
    for kind,obj in (('resource',a),('resource',b),('heap',cpu),('heap',visible),('allocator',allocator),('command',cmd),('queue',queue),('fence',fence),('resource',rb),('device',dev)):
        call('Release',dict(object_kind=kind,object=obj),'uint32',dict(value=0))
    result=dict(schema_version=1,purpose='supplied_original_typed_copy_bind_CPU_inputs',run_id=RUN,
      source_receipts=receipt,app_journal=dict(clock_domain='independent_app_original_transition',scope='declared_owned_CPU_mock_subset',events=events),cases=contexts,
      claims={key:False for key in CLAIMS})
    # The owned journal is a by-value JSON protocol, not Python alias state.
    return json.loads(text(result))


def call_edges(v,callref):return [x for x in v['app_journal']['events'] if x['ordinal']==callref['ordinal']]
def source_change(v,index,change):
    receipt=v['source_receipts'][index];w=json.loads(receipt['wire_text']);change(w)
    receipt['wire_text']=text(w);receipt['wire_sha256']=sha(receipt['wire_text'].encode())
    for edge in call_edges(v,v['cases'][index]['seal_call']):
        if edge['edge']=='after':edge['result'].update(wire_sha256=receipt['wire_sha256'],wire_bytes=len(receipt['wire_text'].encode()))

def alter_args(v,ref_,fn):
    for event in call_edges(v,ref_):fn(event['args'])

def alter_result(v,ref_,fn):
    for event in call_edges(v,ref_):
        if event['edge']=='after':fn(event['result'])


def layout(v, rows):
    """Retain APP data, replacing only its independent clock/CallRef namespace."""
    calls={};next_ordinal=100
    for index,row in enumerate(rows):
        old=row['ordinal']
        if row['edge']=='before':
            next_ordinal+=1;calls[old]={'ordinal':next_ordinal,'entry':1001+index,'return':0}
        else:calls[old]['return']=1001+index
    for row in rows:row['ordinal']=calls[row['ordinal']]['ordinal']
    for index,row in enumerate(rows):row['transition']=1001+index
    def update(x):
        if isinstance(x,dict):
            if set(x)=={'ordinal','entry','return'}:x.update(calls[x['ordinal']])
            else:
                for value in x.values():update(value)
        elif isinstance(x,list):
            for value in x:update(value)
    update(v['cases']);v['app_journal']['events']=rows


def insert_call(v, anchor, api, args, kind='void', result=None, after=False, case=None):
    rows=v['app_journal']['events'];target=call_edges(v,anchor)
    position=rows.index(target[-1] if after else target[0])+(1 if after else 0)
    ordinal=max(x['ordinal'] for x in rows)+1000
    case=target[0]['case_ordinal'] if case is None else case
    pair=[dict(ordinal=ordinal,transition=0,api=api,edge=edge,case_ordinal=case,
      return_kind=kind,args=copy.deepcopy(args),original_returned=returned,result=copy.deepcopy(output))
      for edge,returned,output in (('before',False,None),('after',True,result))]
    layout(v,rows[:position]+pair+rows[position:])


def move_call(v, moving, anchor, after=False):
    rows=v['app_journal']['events'];pair=call_edges(v,moving)
    rest=[row for row in rows if row not in pair]
    target=call_edges(v,anchor);position=rest.index(target[-1] if after else target[0])+(1 if after else 0)
    layout(v,rest[:position]+pair+rest[position:])


def add_equal_SOURCE_getter(w,kind):
    rows=w['getters']['records'];original=next(g for g in rows if g['kind']==kind and (kind=='gpu_start' or g['source_object']['raw']==81))
    row=copy.deepcopy(original);row.update(id=10,entry=21,**{'return':22},caller_output_pointer=999,returned_pointer=999)
    rows.append(row);w['getters']['calls']+=1;w['getters']['returns']+=1
    w['source']['source_calls']+=1;w['source']['source_returns']+=1
    for event in w['native_wire']['snapshot']['events']:event['entry']+=2;event['return']+=2
    for event in w['source']['mutations']:event['entry']+=2;event['return']+=2
    w['native_wire']['snapshot']['at_transition']+=2;w['native_wire']['producer']['clock']+=2;w['cutoff']['transition']+=2


class BindTests(unittest.TestCase):
    def setUp(self):
        p=patch.object(subprocess,'Popen',side_effect=AssertionError('Pure tests start no child'));p.start();self.addCleanup(p.stop)
    def denied(self,v):
        try:r=model.analyze(v)
        except ValueError:return
        self.assertFalse(r['supplied_argument_relation_known'])
    def test_positive_two_new_CPU_cases(self):
        r=model.analyze(seed());self.assertTrue(r['supplied_argument_relation_known'])
        for i,raw in enumerate((101,102)):
            self.assertEqual(r['cases'][i]['getter_audit']['native_model_replay']['history']['selected_version']['origin']['resource']['raw'],raw)
    def test_getter_result_preserved_exact_once_per_wire(self):
        v=seed();real=getters.audit_getter_wire
        with patch.object(getters,'audit_getter_wire',wraps=real) as probe:r=model.analyze(v)
        self.assertEqual(probe.call_count,2)
        for i,receipt in enumerate(v['source_receipts']):self.assertEqual(r['cases'][i]['getter_audit'],real(receipt['wire_text']))
    def test_wrong_nonce_sequence_token_digest(self):
        for key,val in (('run_id','a'*32),):
            v=seed();v[key]=val;self.denied(v)
        for key,val in (('sequence',3),('receipt_token',True),('wire_sha256','a'*64)):
            v=seed();v['source_receipts'][0][key]=val;self.denied(v)
    def test_wire_digest_uses_exact_bytes_not_reencoding(self):
        v=seed();v['source_receipts'][0]['wire_text']+=' ';self.denied(v)
    def test_source_pointer_is_hidden_not_APP_equality(self):
        v=seed()
        def mutate(w):
            for row in w['getters']['records']:
                if row['kind']!='increment':row['caller_output_pointer']+=900000;row['returned_pointer']+=900000
        source_change(v,0,mutate);source_change(v,1,mutate)
        self.assertTrue(model.analyze(v)['supplied_argument_relation_known'])
    def test_APP_clocks_offset_positive(self):
        v=seed()
        for event in v['app_journal']['events']:event['transition']+=7000
        def shift(x):
            if isinstance(x,dict):
                if set(x)=={'ordinal','entry','return'}:x['entry']+=7000;x['return']+=7000
                else:
                    for value in x.values():shift(value)
            elif isinstance(x,list):
                for y in x:shift(y)
        shift(v['cases']);self.assertTrue(model.analyze(v)['supplied_argument_relation_known'])
    def test_APP_generations_offset_positive(self):
        v=seed()
        def shift(x):
            if isinstance(x,dict):
                if set(x)=={'raw','generation'}:x['generation']+=9999
                else:
                    for y in x.values():shift(y)
            elif isinstance(x,list):
                for y in x:shift(y)
        shift(v['app_journal']);shift(v['cases']);self.assertTrue(model.analyze(v)['supplied_argument_relation_known'])
    def test_wrong_APP_generation_and_parent(self):
        v=seed();v['cases'][0]['selected_heap']['generation']=999;self.denied(v)
        v=seed();alter_args(v,v['cases'][0]['object_calls']['visible_heap'],lambda a:a['device'].update(generation=999));self.denied(v)
    def test_failed_pending_or_missing_original_create(self):
        v=seed();alter_result(v,v['cases'][0]['object_calls']['visible_heap'],lambda r:r.update(hresult=-1));self.denied(v)
        v=seed();ordinal=v['cases'][0]['object_calls']['visible_heap']['ordinal'];v['app_journal']['events']=[e for e in v['app_journal']['events'] if not(e['ordinal']==ordinal and e['edge']=='after')];self.denied(v)
    def test_wrong_getter_value_enum_or_CallRef(self):
        for which,value in (('increment',64),('cpu_heap',933),('visible_gpu',9001)):
            v=seed();alter_result(v,v['cases'][0]['getter_calls'][which],lambda r:r.update(value=value));self.denied(v)
        v=seed();alter_args(v,v['cases'][0]['getter_calls']['increment'],lambda a:a.update(heap_type=1));self.denied(v)
    def test_source_getter_and_publication_forgery(self):
        v=seed();source_change(v,0,lambda w:w['publications'][0].update(increment_getter_id=7));self.denied(v)
        v=seed();source_change(v,0,lambda w:w['getters']['records'][0]['source_object'].update(generation=99));self.denied(v)
    def test_source_original_typed_argument_mismatch(self):
        v=seed();alter_args(v,v['cases'][1]['typed_calls'][-1],lambda a:a['source'].update(cpu_handle=901,index=0));self.denied(v)
    def test_same_raw_wrong_recording_epoch_Close_root(self):
        for call in ('bind_call','root0_call','close_call','reset_call'):
            v=seed();alter_args(v,v['cases'][1][call],lambda a:a.update(recording_epoch=1));self.denied(v)
        v=seed();alter_result(v,v['cases'][0]['close_call'],lambda r:r.update(hresult=-1));self.denied(v)
    def test_root0_exact_heap_and_handle_checked(self):
        for delta in (1,32,(1<<64)-5001):
            v=seed();alter_args(v,v['cases'][0]['root0_call'],lambda a:a.update(gpu_handle=a['gpu_handle']+delta));self.denied(v)
        v=seed();alter_args(v,v['cases'][0]['root0_call'],lambda a:a.update(parameter=1));self.denied(v)
        v=seed();alter_args(v,v['cases'][0]['bind_call'],lambda a:a.update(heaps=[ref(81,61)]));self.denied(v)
    def test_void_original_never_invents_HRESULT(self):
        for call in ('bind_call','root0_call','execute_call'):
            v=seed()
            for e in call_edges(v,v['cases'][0][call]):
                if e['edge']=='after':e['result']={'hresult':0,'output':None}
            self.denied(v)
    def test_fence_initial_signal_completion_sentinel_and_queue(self):
        v=seed();alter_args(v,v['cases'][0]['object_calls']['fence'],lambda a:a.update(initial_value=999));self.denied(v)
        v=seed();alter_args(v,v['cases'][1]['signal_call'],lambda a:a.update(value=1));self.denied(v)
        for value in (0,(1<<64)-1,True):
            v=seed();alter_result(v,v['cases'][0]['completed_call'],lambda r:r.update(value=value));self.denied(v)
        v=seed();alter_args(v,v['cases'][0]['signal_call'],lambda a:a['queue'].update(generation=999));self.denied(v)
    def test_event_wait_map_and_readback_hash(self):
        v=seed();alter_args(v,v['cases'][0]['wait_call'],lambda a:a.update(event=999));self.denied(v)
        v=seed();alter_result(v,v['cases'][0]['wait_call'],lambda r:r.update(value=258));self.denied(v)
        v=seed();alter_result(v,v['cases'][0]['map_call'],lambda r:r.update(hresult=-1));self.denied(v)
        v=seed();v['cases'][0]['readback']['sha256']='a'*64;self.denied(v)
    def test_readback_consistency_is_not_bytes_origin_or_pixels(self):
        v=seed();data=b'x'*32;v['cases'][0]['readback'].update(bytes_hex=data.hex(),sha256=sha(data))
        r=model.analyze(v);self.assertTrue(r['supplied_argument_relation_known'])
        self.assertFalse(r['cases'][0]['readback_bytes_origin_known'])
    def test_case1_immutable_despite_future_Reset_and_80_churn(self):
        v=seed();r=model.analyze(v);old=copy.deepcopy(r)
        for _ in range(80):self.assertEqual(model.analyze(copy.deepcopy(v)),old)
        self.assertEqual(r,old)
    def test_all_stronger_claims_false_and_not_pinned_runtime(self):
        r=model.analyze(seed())
        for key in CLAIMS:
            self.assertIs(r[key],False)
            v=seed();v['claims'][key]=True;self.denied(v)
    def test_bool_U64_overflow_and_untrusted_source_fields(self):
        for value in (True,-1,1<<64):
            v=seed();v['cases'][0]['selected_index']=value;self.denied(v)
        v=seed();v['cases'][0]['source_generation']=1;self.denied(v)
    def test_outer_child_and_output_budget_framing(self):
        for v in (' '*262145,seed()):
            if isinstance(v,dict):v['source_receipts'][0]['wire_text']+=' '*16385
            self.denied(v)
    def test_silent_overwrite_observational_twins(self):
        a=seed();b=copy.deepcopy(a);ra=model.analyze(a);rb=model.analyze(b);self.assertEqual(ra,rb)
        self.assertFalse(ra['current_contents_at_draw_known']);self.assertFalse(ra['sampled_resource_GPU_read_proven'])


    def test_unreferenced_root0_and_heap_overrides(self):
        for api,which in (('SetGraphicsRootDescriptorTable','root0_call'),('SetDescriptorHeaps','bind_call')):
            v=seed();args=copy.deepcopy(call_edges(v,v['cases'][0][which])[0]['args'])
            if api=='SetGraphicsRootDescriptorTable':args['gpu_handle']+=1
            insert_call(v,v['cases'][0]['close_call'],api,args);self.denied(v)
    def test_unreferenced_Execute_and_Signal(self):
        for which in ('execute_call','signal_call'):
            v=seed();edge=call_edges(v,v['cases'][0][which])[0]
            result=None if which=='execute_call' else dict(hresult=0,output=None)
            insert_call(v,v['cases'][0][which],edge['api'],edge['args'],edge['return_kind'],result);self.denied(v)
    def test_extra_event_registration_is_not_selected_event(self):
        v=seed();edge=call_edges(v,v['cases'][0]['event_call'])[0]
        args=copy.deepcopy(edge['args']);args['value']=999
        insert_call(v,v['cases'][0]['wait_call'],edge['api'],args,'HRESULT',dict(hresult=0,output=None));self.denied(v)
    def test_coherent_late_copy_Seal_Execute_and_fence_order(self):
        for moving,anchor,after in (('late','close_call',False),('late','execute_call',True),('seal_call','execute_call',True),('fence','signal_call',True)):
            v=seed();c=v['cases'][1]
            ref_=c['typed_calls'][-1] if moving=='late' else c['object_calls']['fence'] if moving=='fence' else c[moving]
            move_call(v,ref_,c[anchor],after);self.denied(v)
    def test_reentrant_or_pending_selected_setter(self):
        v=seed();c=v['cases'][0];rows=v['app_journal']['events'];root=call_edges(v,c['root0_call'])
        rows.remove(root[1]);position=rows.index(call_edges(v,c['close_call'])[0])+1
        rows.insert(position,root[1]);layout(v,rows);self.denied(v)
        v=seed();c=v['cases'][0];rows=v['app_journal']['events'];root=call_edges(v,c['root0_call'])
        inner=copy.deepcopy(root);ident=max(e['ordinal'] for e in rows)+1000
        for edge in inner:edge['ordinal']=ident;edge['args']['gpu_handle']+=1
        position=rows.index(root[1]);layout(v,rows[:position]+inner+rows[position:]);self.denied(v)
    def test_release_before_use_and_raw_reuse_ambiguity(self):
        v=seed();c=v['cases'][0]
        r=next(e for e in v['app_journal']['events'] if e['edge']=='before' and e['api']=='Release' and e['args']['object']==c['selected_heap'])
        move_call(v,dict(ordinal=r['ordinal'],entry=r['transition'],**{'return':r['transition']+1}),c['bind_call']);self.denied(v)
        v=seed();c=v['cases'][0];a=call_edges(v,c['object_calls']['resource_A'])
        twin=copy.deepcopy(a);rid=max(e['ordinal'] for e in v['app_journal']['events'])+1000
        for e in twin:
            e['ordinal']=rid
            if e['edge']=='after':e['result']['output']['generation']=50
        release=[dict(ordinal=rid+1,transition=0,api='Release',edge=edge,case_ordinal=0,return_kind='uint32',args=dict(object_kind='resource',object=ref(101,50)),original_returned=done,result=dict(value=0) if done else None) for edge,done in (('before',False),('after',True))]
        rows=v['app_journal']['events'];at=rows.index(a[0]);layout(v,rows[:at]+twin+release+rows[at:]);self.denied(v)
    def test_equal_SOURCE_getters_remain_ambiguous(self):
        for kind in ('cpu_start','gpu_start'):
            v=seed()
            for i in range(2):
                source_change(v,i,lambda w:add_equal_SOURCE_getter(w,kind))
                self.assertTrue(getters.audit_getter_wire(v['source_receipts'][i]['wire_text'])['prototype_source_history_known'])
            self.denied(v)
    def test_equal_APP_getters_remain_ambiguous(self):
        v=seed();c=v['cases'][0];edge=call_edges(v,c['getter_calls']['visible_gpu'])[0]
        insert_call(v,c['reset_call'],edge['api'],edge['args'],'GPU_HANDLE',dict(value=5001),case=0);self.denied(v)
    def test_missing_final_APP_edge_and_future_malformed_args(self):
        v=seed();v['app_journal']['events'].pop();self.denied(v)
        v=seed();v['app_journal']['events'][-1]['args']['object']['raw']=True;self.denied(v)
    def test_UTF8_duplicate_keys_float_deep_and_report_budgets(self):
        for raw in (text(seed()).encode('utf16'),'{"schema_version":1,"schema_version":1}',text(seed()).replace('"selected_index":0','"selected_index":0.0',1)):
            self.denied(raw)
        v=seed();v['claims']['input_facts_attested']=[[[[[[[[[[[[[[[[[False]]]]]]]]]]]]]]]]];self.denied(v)
        with patch.object(model,'MAX_OUTPUT',128):self.denied(seed())


    def test_CPU_getter_and_increment_precede_first_typed_use(self):
        for role in ('cpu_heap','visible_cpu','increment'):
            v=seed();move_call(v,v['cases'][0]['getter_calls'][role],v['cases'][0]['typed_calls'][2],True)
            self.denied(v)
    def test_valid_wire_whitespace_is_preserved_and_digest_attested_only_as_input(self):
        v=seed()
        for index,receipt in enumerate(v['source_receipts']):
            receipt['wire_text']=' '+receipt['wire_text']+'\n';receipt['wire_sha256']=sha(receipt['wire_text'].encode())
            alter_result(v,v['cases'][index]['seal_call'],lambda r:r.update(wire_sha256=receipt['wire_sha256'],wire_bytes=len(receipt['wire_text'].encode())))
        r=model.analyze(v);self.assertTrue(r['supplied_argument_relation_known'])
        self.assertEqual(r['source_wire_sha256'],[x['wire_sha256'] for x in v['source_receipts']])
    def test_source_gap_is_diagnostic_and_full_legacy_result_retained(self):
        v=seed()
        def partial(w):
            w['native_wire']['producer'].update(faulted=True,gap=True)
            w['native_wire']['snapshot'].update(complete=False,gap=True)
        for index in range(2):source_change(v,index,partial)
        r=model.analyze(v);self.assertFalse(r['supplied_argument_relation_known'])
        for case,receipt in zip(r['cases'],v['source_receipts']):
            self.assertEqual(case['getter_audit'],getters.audit_getter_wire(receipt['wire_text']))
            self.assertFalse(case['getter_audit']['prototype_source_history_known'])
    def test_after_both_cutoffs_getter_is_not_retroactively_selected(self):
        v=seed();c=v['cases'][1];edge=call_edges(v,c['getter_calls']['visible_gpu'])[0]
        insert_call(v,c['map_call'],edge['api'],edge['args'],'GPU_HANDLE',dict(value=5002),after=True,case=0)
        self.assertTrue(model.analyze(v)['supplied_argument_relation_known'])
    def test_original_BEFORE_is_immutable_and_Return_exact_once(self):
        v=seed();e=call_edges(v,v['cases'][1]['typed_calls'][-1]);e[1]['args']['count']=2;self.denied(v)
        v=seed();e=call_edges(v,v['cases'][0]['execute_call']);e[1]['original_returned']=False;self.denied(v)
        v=seed();rows=v['app_journal']['events'];rows.insert(2,copy.deepcopy(rows[1]));self.denied(v)
    def test_U64_references_getters_queue_and_control_are_strict(self):
        for field in ('command','selected_heap'):
            v=seed();v['cases'][0][field]['raw']=True;self.denied(v)
        for value in (True,1<<64,-1):
            v=seed();alter_result(v,v['cases'][0]['getter_calls']['visible_gpu'],lambda r:r.update(value=value));self.denied(v)
        v=seed();alter_args(v,v['cases'][0]['execute_call'],lambda a:a['queue'].update(raw=True));self.denied(v)
        v=seed();alter_result(v,v['cases'][0]['seal_call'],lambda r:r.update(receipt_token=True));self.denied(v)
    def test_output_does_not_alias_input_or_previous_result(self):
        v=seed();r=model.analyze(v);original=copy.deepcopy(r)
        v['cases'][0]['readback']['bytes_hex']='00'*32;v['app_journal']['events'].clear()
        self.assertEqual(r,original)
        fresh=model.analyze(seed());fresh['cases'][0]['context']['readback']['bytes_hex']='ff'*32
        self.assertEqual(r,original)


    def test_source_APP_getter_order_embedding_is_not_clock_equality(self):
        v=seed();c=v['cases'][0]
        move_call(v,c['getter_calls']['visible_gpu'],c['getter_calls']['visible_cpu'])
        self.denied(v)
    def test_second_receipt_cannot_retag_first_original_getter(self):
        v=seed()
        def pointer_change(w):
            row=w['getters']['records'][1];row['caller_output_pointer']+=777;row['returned_pointer']+=777
        source_change(v,1,pointer_change)
        self.assertTrue(getters.audit_getter_wire(v['source_receipts'][1]['wire_text'])['prototype_source_history_known'])
        self.denied(v)
    def test_second_receipt_cannot_switch_source_epoch_or_old_typed_prefix(self):
        v=seed()
        def epoch_change(w):
            w['native_wire']['producer']['epoch']=8;w['cutoff']['epoch']=8
            for row in w['getters']['records']+w['publications']:row['epoch']=8
        source_change(v,1,epoch_change)
        self.assertTrue(getters.audit_getter_wire(v['source_receipts'][1]['wire_text'])['prototype_source_history_known'])
        self.denied(v)
        v=seed();source_change(v,1,lambda w:w['native_wire']['snapshot']['events'][0]['before']['resource'].update(raw=102,generation=2));self.denied(v)


def bridge(path):
    exe=Path(path).resolve(strict=True)
    p=subprocess.run([str(exe),'--emit-json'],capture_output=True,check=True,timeout=60)
    if p.stderr:raise ValueError(p.stderr.decode('utf8','replace')[:1024])
    report=model.analyze(p.stdout)
    if report['supplied_argument_relation_known'] is not True or len(report['cases'])!=2:raise ValueError('Two new CPU cases must join')
    for index,case in enumerate(report['cases']):
        if case['historical_origin_app_resource']['raw']!=(101,102)[index]:raise ValueError('Independent A then B typed origin')
        if case['getter_audit']['prototype_source_history_known'] is not True:raise ValueError('Full frozen child audit')
        if case['readback_bytes_origin_known'] is not False:raise ValueError('CPU checksums are not pixel provenance')
    for key in CLAIMS:
        if report[key] is not False:raise ValueError('Stronger claim remains false')
    encoded=json.dumps(report,separators=(',',':'),ensure_ascii=True,allow_nan=False).encode('ascii')
    if len(encoded)>model.MAX_OUTPUT:raise ValueError('Complete report budget')
    print(json.dumps(dict(cpu_mock_only=True,cases=2,supplied_argument_relation_known=True,source_wires=2,
      original_APP_edges=len(report['app_journal']['events']),complete_report_bytes=len(encoded),stronger_claims=False),sort_keys=True))
if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--native-wire':bridge(sys.argv[2])
    else:unittest.main()
