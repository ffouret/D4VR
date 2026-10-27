"""Pure CPU snapshots and adversaries. No loader, device or subprocess."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
SPEC=importlib.util.spec_from_file_location('scope',Path(__file__).resolve().parents[1]/'tools/query_scope_validate.py');v=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(v)
RUN='1'*32;BUILD='a'*64

def encoded(x):return (json.dumps(x,separators=(',',':'))+'\n').encode()
def common(slot=0,pid=100,cookie=200):return dict(schema_version=2,run_id=RUN,scope_slot=slot,host_pid=pid,process_start_cookie=cookie,dso_epoch=slot+1,**{k:False for k in v.FALSE_FACETS})
def life(kind,op,ret,slot=0,**kw):
 r=dict(common(slot),event='lifecycle',kind=kind,operation_ordinal=op,return_ordinal=ret,original_returned=ret!=0,return_kind='void' if kind.startswith('destroy') else 'VkResult',result=None if kind.startswith('destroy') or not ret else 0,instance_generation=0,device_generation=0,parent_instance_generation=0,raw_instance=0,raw_device=0,raw_physical=0,output_pointer=0,chain_present=False,application=dict(captured=False,api_version=0,name='',name_truncated=False,engine_name='',engine_name_truncated=False),count_only=False,input_capacity=0,returned_count=0,physical_devices=[]);r.update(kw);return r

def query(index,op,ret,slot=0):
 return dict(common(slot),event='get_descriptor',ordinal=index,operation_ordinal=op,return_ordinal=ret,device=2000+index,device_generation=index,parent_instance_generation=1,device_lifetime_known=True,descriptor_type=0,info_sType=1000316004,info_present=True,chain_present=False,data_size=4,output_pointer=6000+index,original_returned=ret!=0,return_kind='void',typed_input=dict(known=True,kind='sampler',reason='none',raw=5000+index,image_view=0,sampler=5000+index,image_layout=0,address=0,range=0,format=0),phase_before=dict(status='known',case_ordinal=index,api_phase=1),phase_after=dict(status='known',case_ordinal=index,api_phase=1) if ret else dict(status='missing',case_ordinal=0,api_phase=0),phase_unchanged=bool(ret),output_captured=bool(ret),output_hex=('01020304' if index==1 else '05060708') if ret else '')

def rows(slot=0):
 c=common(slot);h=dict(c,event='observer_header',scope_claimed=True,build_id_known=True,build_id=BUILD,build_id_is_binary_attestation=False,scope_slots=8,global_event_limit=256,global_events_per_case_limit=32,lifecycle_limit=256,scope='owned_translated_original_get_descriptor',enabled=True,phase_source='file',transport_scope='owned_single_loader_chain',route_capacity=32,limits=dict(events=32,events_per_case=4,output_bytes=256,phase_file_bytes=256))
 lives=[life('create_instance',1,2,slot,instance_generation=1,raw_instance=1000,output_pointer=4000),life('enumerate_physical_devices',3,4,slot,instance_generation=1,raw_instance=1000,output_pointer=4001,input_capacity=1,returned_count=1,physical_devices=[3000]),life('create_device',5,6,slot,device_generation=1,parent_instance_generation=1,raw_device=2001,raw_physical=3000,output_pointer=4002),life('create_device',7,8,slot,device_generation=2,parent_instance_generation=1,raw_device=2002,raw_physical=3000,output_pointer=4003),life('destroy_device',13,14,slot,device_generation=1,parent_instance_generation=1,raw_device=2001),life('destroy_device',15,16,slot,device_generation=2,parent_instance_generation=1,raw_device=2002),life('destroy_instance',17,18,slot,instance_generation=1,raw_instance=1000)]
 qs=[query(1,9,10,slot),query(2,11,12,slot)];hist=[0,1,1,0,0,0,0,0,0]
 f=dict(c,event='observer_footer',lifecycle_calls_seen=7,lifecycle_retained=7,lifecycle_dropped=0,transition_count=18,calls_seen=2,original_returns=2,events_retained=2,dropped_events=0,skipped_bootstrap_queries=0,unknown_phase_queries=0,phase_unavailable=0,phase_changed=0,output_unavailable=0,typed_input_unknown=0,faulted=False,active_instances=0,active_devices=0,calls_by_case=hist,returns_by_case=hist.copy(),events_by_case=hist.copy(),dropped_by_case=[0]*9,transport_overflow=False,transport_chain_mismatch=False,coverage_complete=True)
 return [h,*lives,*qs,f]

def journal(rs):return b''.join(encoded(r) for r in rs)
def expected(rs):return {k:rs[0][k] for k in v.IDENTITY}|dict(build_id=rs[0]['build_id'])
def claim(slot=0,pid=100,cookie=200):return f'D4VR_QUERY_SCOPE_2 {RUN} {slot} {pid} {cookie} {slot+1}\n'.encode()
def owner():
 r=dict(purpose='owned_loader_query_owner',query_scope_loader_passed=True,run_id=RUN,host_pid=100,process_start_cookie=200,process_identity_unchanged_before_after=True,instance=1000,physical_device=3000,devices=[],queries=[],counts=dict(instances=1,devices=2,samplers=2,original_queries=2,shaders=0,GPU_commands=0,submissions=0,images=0,buffers=0),game_rendered=False,validation=dict(errors=0),resource_generation_from_observer_claimed=False,descriptor_placement_claimed=False,sampled_GPU_read_claimed=False,teardown=dict(devices_destroyed=2,samplers_destroyed=2,instance_destroyed=True,no_work_submitted=True))
 for i in (1,2):
  q=query(i,0,1);r['devices'].append(dict(fixture_device_generation=i,device=2000+i,sampler=5000+i))
  r['queries'].append(dict(original_returned=True,return_kind='void',args=dict(device=2000+i,sampler=5000+i,instance=1000,physical_device=3000,descriptor_type=0,info_sType=1000316004,info_pnext=None,phase=dict(case_ordinal=i,api_phase=1),data_size=4,output_pointer=6000+i),output_hex=q['output_hex']))
 return r

def original_events(m):
 rows=[]
 def call(api,args,handle=None):
  index=len(rows)//2+1;kind='VkResult' if api in ('vkCreateInstance','vkCreateDevice') else 'void'
  for edge in ('before','after'):rows.append(dict(schema_version=1,run_id=m['run_id'],host_pid=m['host_pid'],process_start_cookie=m['process_start_cookie'],ordinal=index,api=api,edge=edge,return_kind=kind,result=0 if edge=='after' and kind=='VkResult' else None,original_returned=edge=='after',returned_handle=handle if edge=='after' else None,args=copy.deepcopy(args)))
 m['instance_api_version']=4206592;call('vkCreateInstance',dict(api_version=4206592,extensions=['VK_EXT_debug_utils'],layers=['VK_LAYER_KHRONOS_validation']),m['instance'])
 for i,d in enumerate(m['devices'],1):call('vkCreateDevice',dict(instance=m['instance'],physical_device=m['physical_device'],fixture_device_generation=i,queue_family=0,queue_count=1,descriptorBuffer_enabled=True,bufferDeviceAddress_enabled=False,descriptorBufferCaptureReplay_enabled=False,descriptorBufferImageLayoutIgnored_enabled=False,descriptorBufferPushDescriptors_enabled=False),d['device'])
 for i,q in enumerate(m['queries'],4):q['original_ordinal']=i;call('vkGetDescriptorEXT',q['args'])
 for d in m['devices']:call('vkDestroySampler',dict(device=d['device'],sampler=d['sampler']));call('vkDestroyDevice',dict(device=d['device'],fixture_device_generation=d['fixture_device_generation']))
 call('vkDestroyInstance',dict(instance=m['instance']));return b''.join(encoded(r) for r in rows)

class Scope(unittest.TestCase):
 def setUp(self):
  self.rs=rows();self.original=journal(self.rs)
  for k in ('Popen','run','call','check_call','check_output'):
   p=patch.object(subprocess,k,side_effect=AssertionError('No subprocess'));p.start();self.addCleanup(p.stop)
 def audit(self):return v.audit_journal(journal(self.rs),expected(self.rs))
 def reject(self):
  with self.assertRaises((ValueError,KeyError,TypeError)):self.audit()
 def test_valid_lifecycle_query_teardown(self):
  r=self.audit();self.assertTrue(r['finite_scope_complete']);self.assertEqual(len(r['devices']),2);self.assertEqual(r['query_records'],2)
 def test_global_absence_is_always_unknown(self):
  r=v.analyze({0:self.original},{0:claim()},RUN,BUILD,owner());self.assertTrue(r['owner']['owner_joined']);self.assertTrue(r['joined_owner_finite_coverage']);self.assertFalse(r['global_query_absence_proven']);self.assertFalse(r['any_API_query_absence_proven'])
  self.assertFalse(r['owner']['application_to_layer_physical_mapping_known']);self.assertFalse(r['owner']['application_to_layer_create_output_slot_mapping_known'])
  for d in r['owner']['devices']:
   self.assertEqual(d['application_raw_physical'],d['layer_raw_physical']);self.assertFalse(d['application_to_layer_physical_mapping_known']);self.assertFalse(d['application_to_layer_create_output_slot_mapping_known'])
 def test_loader_physical_and_creation_slots_are_distinct_domains(self):
  o=owner();o['physical_device']=9999
  for q in o['queries']:q['args']['physical_device']=9999
  self.rs[3]['output_pointer']=777777;self.rs[4]['output_pointer']=888888
  r=v.analyze({0:journal(self.rs)},{0:claim()},RUN,BUILD,o);self.assertTrue(r['owner']['owner_joined']);self.assertTrue(r['joined_owner_finite_coverage'])
  for d in r['owner']['devices']:
   self.assertEqual(d['application_raw_physical'],9999);self.assertEqual(d['layer_raw_physical'],3000);self.assertFalse(d['application_to_layer_physical_mapping_known']);self.assertFalse(d['application_to_layer_create_output_slot_mapping_known'])
 def test_cross_loader_unknown_does_not_weaken_logical_query_witness(self):
  for field in ('device','sampler','output_pointer'):
   o=owner();o['physical_device']=9999
   for q in o['queries']:q['args']['physical_device']=9999
   o['queries'][0]['args'][field]=123456
   if field in ('device','sampler'):o['devices'][0][field]=123456
   r=v.analyze({0:self.original},{0:claim()},RUN,BUILD,o);self.assertFalse(r['owner']['owner_joined'])
 def test_missing_owner_blocks_join(self):self.assertFalse(v.analyze({0:self.original},{0:claim()},RUN,BUILD)['owner']['owner_joined'])
 def test_owner_cookie_reuse_refused(self):o=owner();o['process_start_cookie']=201;self.assertFalse(v.analyze({0:self.original},{0:claim()},RUN,BUILD,o)['owner']['owner_joined'])
 def test_owner_wrong_output_refused(self):o=owner();o['queries'][0]['output_hex']='00000000';self.assertFalse(v.analyze({0:self.original},{0:claim()},RUN,BUILD,o)['owner']['owner_joined'])
 def test_namespace_ambiguous_witness_refuses_owner(self):
  other=rows(1);r=v.analyze({0:self.original,1:journal(other)},{0:claim(),1:claim(1)},RUN,BUILD,owner());self.assertFalse(r['owner']['owner_joined'])
 def test_separate_PID_reload_scopes(self):
  other=rows(1)
  for r in other:r['host_pid']=101;r['process_start_cookie']=202
  result=v.analyze({0:self.original,1:journal(other)},{0:claim(),1:claim(1,101,202)},RUN,BUILD,owner());self.assertEqual(result['claimed_scopes'],2);self.assertTrue(result['owner']['owner_joined'])
 def test_bad_nonce(self):self.rs[4]['run_id']='2'*32;self.reject()
 def test_bad_scope_epoch(self):self.rs[4]['dso_epoch']=9;self.reject()
 def test_bad_PID_or_cookie(self):self.rs[4]['host_pid']=101;self.reject()
 def test_claim_filename_collision(self):
  with self.assertRaises(ValueError):v.analyze({0:self.original},{0:claim(1)},RUN,BUILD)
 def test_claim_without_journal_incomplete(self):r=v.analyze({}, {0:claim()},RUN,BUILD,owner());self.assertFalse(r['all_saved_scopes_complete']);self.assertFalse(r['owner']['owner_joined'])
 def test_partial_header_and_missing_footer(self):r=v.analyze({0:encoded(self.rs[0])},{0:claim()},RUN,BUILD,owner());self.assertFalse(r['scope_audit_valid']);self.assertFalse(r['owner']['owner_joined'])
 def test_truncated_json(self):
  with self.assertRaises(ValueError):v.audit_journal(self.original[:-1],expected(self.rs))
 def test_duplicate_transition_collision(self):self.rs[8]['operation_ordinal']=7;self.reject()
 def test_reverse_original_returns_valid(self):
  self.rs[8]['return_ordinal']=12;self.rs[9]['operation_ordinal']=10;self.rs[9]['return_ordinal']=11;self.assertTrue(self.audit()['finite_scope_complete'])
 def test_pending_query_cannot_close(self):
  self.rs[8].update(return_ordinal=0,original_returned=False,phase_after=dict(status='missing',case_ordinal=0,api_phase=0),phase_unchanged=False,output_captured=False,output_hex='');self.rs[-1].update(original_returns=1,returns_by_case=[0,0,1,0,0,0,0,0,0],transition_count=17,coverage_complete=False)
  # Remove device/parent teardown: those originals have not happened yet.
  self.rs=[r for r in self.rs if r.get('kind') not in ('destroy_device','destroy_instance')];self.rs[-1].update(lifecycle_calls_seen=4,lifecycle_retained=4,transition_count=11,active_instances=1,active_devices=2)
  # Query2 entry/return occupy10/11 while query1 is still pending at9.
  self.rs[-2].update(operation_ordinal=10,return_ordinal=11)
  r=self.audit();self.assertFalse(r['finite_scope_complete']);self.assertEqual(r['pending_queries'],1)
 def test_failed_create_not_a_live_lifetime(self):self.rs[3].update(result=-2,raw_device=0);self.reject()
 def test_failed_create_can_be_recorded_without_publishing_lifetime(self):
  x=life('create_instance',19,20,instance_generation=2,raw_instance=0,output_pointer=4004,result=-2);self.rs.insert(8,x);self.rs[-1].update(lifecycle_calls_seen=8,lifecycle_retained=8,transition_count=20);self.assertTrue(self.audit()['finite_scope_complete'])
 def test_pending_creation_not_live(self):self.rs[1].update(original_returned=False,return_ordinal=0,result=None,raw_instance=0);self.reject()
 def test_unenumerated_parent_refused(self):self.rs[2]['physical_devices']=[3001];self.reject()
 def test_count_only_not_an_ownership_mapping(self):self.rs[2].update(count_only=True,input_capacity=0,output_pointer=0,physical_devices=[]);self.reject()
 def test_device_generation_twins_distinct(self):self.rs[9]['device_generation']=1;self.reject()
 def test_concurrent_raw_device_alias(self):self.rs[4]['raw_device']=2001;self.reject()
 def test_destroy_wrong_generation(self):self.rs[5]['device_generation']=2;self.reject()
 def test_parent_destroy_while_device_live(self):self.rs[7].update(operation_ordinal=12,return_ordinal=19);self.reject()
 def test_query_return_after_destroy_entry(self):self.rs[8]['return_ordinal']=14;self.rs[5]['return_ordinal']=10;self.reject()
 def test_raw_handle_reuse_with_new_generation(self):
  # A completed third generation reuses device1's raw handle after exact return.
  self.rs[7].update(operation_ordinal=21,return_ordinal=22)
  self.rs.insert(7,life('create_device',17,18,device_generation=3,parent_instance_generation=1,raw_device=2001,raw_physical=3000,output_pointer=4005));self.rs.insert(8,life('destroy_device',19,20,device_generation=3,parent_instance_generation=1,raw_device=2001));self.rs[-1].update(lifecycle_calls_seen=9,lifecycle_retained=9,transition_count=22)
  self.assertTrue(self.audit()['finite_scope_complete'])
 def test_ignored_input_union_zero(self):self.rs[8]['typed_input']['image_view']=77;self.reject()
 def test_output_prefix_not_a_full_return(self):self.rs[8]['output_hex']='01';self.reject()
 def test_byte_quota(self):
  with self.assertRaises(ValueError):v.audit_journal(b'x'*(v.MAX_BYTES+1),expected(self.rs))
 def test_false_resource_facets(self):self.rs[8]['current_contents_at_draw_known']=True;self.reject()
 def test_build_unknown_is_not_finite_coverage(self):self.rs[0].update(build_id='unknown',build_id_known=False);r=self.audit();self.assertTrue(r['declared_complete']);self.assertFalse(r['finite_scope_complete'])
 def test_overflow_marker_blocks_owner_coverage(self):r=v.analyze({0:self.original},{0:claim()},RUN,BUILD,owner(),True);self.assertTrue(r['owner']['owner_joined']);self.assertFalse(r['joined_owner_finite_coverage'])
 def test_eight_claims_never_evicted(self):
  claims={i:claim(i) for i in range(8)};journals={i:journal(rows(i)) for i in range(8)};r=v.analyze(journals,claims,RUN,BUILD);self.assertTrue(r['claim_pool_fully_occupied']);self.assertEqual(r['retained_query_records'],16)
  with self.assertRaises(ValueError):v.analyze(journals,claims|{8:claim()},RUN,BUILD)
 def test_full32records_and_four_per_case(self):
  h=self.rs[0];lives=[life('create_instance',1,2,instance_generation=1,raw_instance=1000,output_pointer=4000),life('enumerate_physical_devices',3,4,instance_generation=1,raw_instance=1000,output_pointer=4001,input_capacity=1,returned_count=1,physical_devices=[3000]),life('create_device',5,6,device_generation=1,parent_instance_generation=1,raw_device=2001,raw_physical=3000,output_pointer=4002),life('destroy_device',71,72,device_generation=1,parent_instance_generation=1,raw_device=2001),life('destroy_instance',73,74,instance_generation=1,raw_instance=1000)]
  qs=[]
  for i in range(32):
   q=query(1,7+2*i,8+2*i);q['ordinal']=i+1;q['phase_before']['case_ordinal']=q['phase_after']['case_ordinal']=i//4+1;qs.append(q)
  f=copy.deepcopy(self.rs[-1]);hist=[0]+[4]*8;f.update(lifecycle_calls_seen=5,lifecycle_retained=5,transition_count=74,calls_seen=32,original_returns=32,events_retained=32,calls_by_case=hist,returns_by_case=hist.copy(),events_by_case=hist.copy())
  self.rs=[h,*lives,*qs,f];self.assertEqual(self.audit()['query_records'],32)
  qs[4]['phase_before']['case_ordinal']=qs[4]['phase_after']['case_ordinal']=1;hist[1]=5;hist[2]=3;f['returns_by_case']=hist.copy();f['events_by_case']=hist.copy();self.reject()
 def test_original_query_counter_and_transition_cannot_swap(self):
  self.rs[8]['operation_ordinal']=11;self.rs[9]['operation_ordinal']=9;self.reject()
 def test_query_and_case_counter_cap(self):self.rs[-1]['events_by_case'][1]=5;self.reject()
 def test_lifecycle_gap_count(self):self.rs[-1]['lifecycle_dropped']=1;self.reject()
 def test_footer_active_routes_not_closed(self):self.rs[-1]['active_devices']=1;self.reject()
 def test_pinned_directory_and_fresh_cli(self):
  with tempfile.TemporaryDirectory() as tmp:
   base=Path(tmp).resolve();d=base/'journals';d.mkdir(mode=0o700);(d/'slot-0.claim').write_bytes(claim());(d/'slot-0.jsonl').write_bytes(self.original);src=base/'src';src.mkdir();(src/'observer.cpp').write_text('synthetic source');mod=base/'observer.so';mod.write_bytes(b'synthetic ELF')
   pins=dict(schema_version=2,run_id=RUN,build_id=BUILD,observer_module=dict(path=str(mod),bytes=mod.stat().st_size,sha256=v.digest(mod.read_bytes())),source_root=str(src),source_sha256={'observer.cpp':v.digest((src/'observer.cpp').read_bytes())},journal_directory=str(d),files={p.name:v.digest(p.read_bytes()) for p in d.iterdir()},owner_directory=None,owner_files={})
   r=v.audit_directory(d,pins);self.assertTrue(r['scope_audit_valid']);self.assertFalse(r['owner']['owner_joined'])
   pp=base/'pins.json';pp.write_bytes(encoded(pins));out=base/'audit.json'
   with patch('builtins.print'):self.assertEqual(v.main(['--directory',str(d),'--pins',str(pp),'--pins-sha256',v.digest(pp.read_bytes()),'--output',str(out)]),0)
   with self.assertRaises(ValueError):v.main(['--directory',str(d),'--pins',str(pp),'--pins-sha256',v.digest(pp.read_bytes()),'--output',str(out)])
   (d/'slot-1.jsonl').write_text('unclaimed');
   with self.assertRaises(ValueError):v.audit_directory(d,pins)
 def test_original_owner_edges_and_void_results(self):
  m=owner();data=original_events(m);self.assertEqual(v.audit_owner_events(m,data)['original_calls'],10)
  rs=[json.loads(line) for line in data.splitlines()];rs[7]['result']=0
  with self.assertRaises(ValueError):v.audit_owner_events(m,b''.join(encoded(r) for r in rs))
 def test_directory_positive_owner_and_tampered_originals(self):
  with tempfile.TemporaryDirectory() as tmp:
   base=Path(tmp).resolve();d=base/'journals';d.mkdir(mode=0o700);(d/'slot-0.claim').write_bytes(claim());(d/'slot-0.jsonl').write_bytes(self.original)
   src=base/'src';src.mkdir();(src/'source').write_bytes(b'owned');module=base/'module';module.write_bytes(b'ELFmock')
   root=base/'owner';root.mkdir();m=owner();m['original_events_file']='original-events.jsonl';data=original_events(m);(root/m['original_events_file']).write_bytes(data)
   m['validation']=dict(debug_messenger=True,errors=0,warnings=0,includes_teardown=True,file='validation.log');(root/'validation.log').write_bytes(b'')
   for i,q in enumerate(m['queries'],1):
    q['output_file']=f'device-{i}.descriptor.bin';b=bytes.fromhex(q['output_hex']);q['output_sha256']=v.digest(b);(root/q['output_file']).write_bytes(b)
   (root/'manifest.json').write_bytes(encoded(m))
   pins=dict(schema_version=2,run_id=RUN,build_id=BUILD,observer_module=dict(path=str(module),bytes=module.stat().st_size,sha256=v.digest(module.read_bytes())),source_root=str(src),source_sha256={'source':v.digest(b'owned')},journal_directory=str(d),files={p.name:v.digest(p.read_bytes()) for p in d.iterdir()},owner_directory=str(root),owner_files={p.name:v.digest(p.read_bytes()) for p in root.iterdir()})
   r=v.audit_directory(d,pins);self.assertTrue(r['owner']['owner_joined']);self.assertFalse(r['global_query_absence_proven'])
   rs=[json.loads(line) for line in data.splitlines()];rs[7]['args']['data_size']=5;(root/m['original_events_file']).write_bytes(b''.join(encoded(r) for r in rs));pins['owner_files'][m['original_events_file']]=v.digest((root/m['original_events_file']).read_bytes())
   with self.assertRaises(ValueError):v.audit_directory(d,pins)
 def test_FIFO_and_symlink_refused_without_payload_read(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp).resolve();real=root/'real';real.write_bytes(b'private')
   link=root/'link'
   try:link.symlink_to(real)
   except OSError:pass
   else:
    with patch.object(os,'read',side_effect=AssertionError('No payload read')):
     with self.assertRaises((ValueError,OSError)):v.read_file(link)
   if hasattr(os,'mkfifo'):
    fifo=root/'fifo';os.mkfifo(fifo)
    with patch.object(os,'read',side_effect=AssertionError('No FIFO read')):
     with self.assertRaises((ValueError,OSError)):v.read_file(fifo)
 def test_JSON_duplicate_and_integer_bounds(self):
  for data in (b'{"a":1,"a":2}',b'{"a":NaN}',b'{"a":1e999}',b'{"a":99999999999999999999999999999}'):
   with self.assertRaises(ValueError):v.loads(data)

if __name__=='__main__':unittest.main()
