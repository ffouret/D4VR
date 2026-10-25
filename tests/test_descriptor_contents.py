import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import descriptor_contents


def fixture():
    """Invented bytes only. This never contains a driver/game encoding."""
    defs=[{'id':i,'snapshot_id':i,'definition_complete':True,'flags':16,'bindings':[]} for i in (1,2,3)]
    ubo={'id':4,'snapshot_id':5,'definition_complete':True,'flags':16,'size_known':True,'size_bytes':8,
         'bindings':[{'binding':b,'type':6,'count':1,'flags':0,'stages':33,'offset_known':True,
                      'offset_bytes':b*4,'immutable_samplers_unresolved':False} for b in (0,1)]}
    old=copy.deepcopy(ubo);old['snapshot_id']=4
    for b in old['bindings']:b['offset_known']=False
    defs += [old,ubo]
    def layout(snapshot,last):
        return {'id':9,'snapshot_id':snapshot,'flags':0,'definition_complete':True,'set_count':4,
                'sets':[{'snapshot_id':i,'null_slot':False} for i in (1,2,3,last)]}
    def memory(buffer_id,size):
        return {'buffer_id':buffer_id,'buffer_size':size,'status':3,'allocation_live':True,
                'properties_known':True,'property_flags':6,'binding_complete':True,
                'allocation_chain_complete':True,'buffer_payload_fully_mapped':True,'mapping_complete':True,'mapped':True}
    receipts=[];payload=bytearray(48);offsets=[]
    for i in range(6):
        blob=bytes([i+1])*4;o=(i//2)*16+(i%2)*4;payload[o:o+4]=blob;offsets.append(o)
        receipts.append({'id':i+1,'type':6,'kind':1,'encoding_hex':blob.hex(),'range_bytes':32,
            'input_chain_complete':True,'null_descriptor':False,'input_pointer_null':False,'range_within_observed_buffer':True,
            'buffer':{'buffer_id':i+1,'size':32,'status':1,'creation_chain_complete':True,'address_offset':0}})
    receipts.append({'id':7,'type':2,'range_bytes':0,'encoding_hex':'feedface'})
    def event(kind,**kwargs):
        return {'kind':kind,'observed_event_index':0,'truncated':False,**kwargs}
    bound={'status':1,'buffer_id':20,'creation_chain_complete':True,'binding_usage':4194304,
           'buffer_usage':4194304,'address_offset':0,'remaining_bytes':48}
    events=[event('descriptor_buffers',count=1,bindings=[bound]),
            event('descriptor_offsets',bind_point=0,first_set=3,count=1,indices=[0],offsets=[0],layout_snapshot_id=11),
            event('descriptor_offsets',bind_point=1,first_set=3,count=1,indices=[0],offsets=[32],layout_snapshot_id=11),
            event('draw',pipeline_id=10),event('dispatch',pipeline_id=10),
            event('classic_descriptors',bind_point=0,first_set=3,count=1),event('draw',pipeline_id=10)]
    for i,e in enumerate(events):e['observed_event_index']=i
    data={'truncated':False,'memory_observations_truncated':False,'descriptor_query_catalog_truncated':False,
          'descriptor_queries_observed':7,'descriptor_query_receipts':receipts,'descriptor_set_layouts':defs,
          'pipeline_layouts':[layout(10,4),layout(11,5)],'pipelines':[{'id':10,'layout_snapshot_id':10,'missing_libraries':False,
             'pipeline_state':{'declarations_complete':True,'linked_libraries':False,'is_library':False}}],
          'buffer_memory':[memory(i,32) for i in range(1,7)]+[memory(20,48)],
          'submissions':[{'result':0,'complete':True,'commands':[{'missing':False,'truncated':False,'resource_events':events}]}]}
    oracle={'schema_version':1,'queries':[{k:r[k] for k in ('type','range_bytes','encoding_hex')} for r in receipts],
            'owned_storage':{'scope':'owned_synthetic_fixture','coherent_exclusive_immutable_storage':True,'completed_submissions':8,
                'uniform_descriptor_size':4,'offset_alignment':4,'set_stride':16,'encoding_hex':payload.hex(),
                'after_encoding_hex':payload.hex(),'query_write_offsets':offsets}}
    return data,oracle,events


class OwnedDescriptors(unittest.TestCase):
    def test_gpu_copy_is_used_only_after_entire_descriptor_and_uniform_comparison(self):
        data,oracle,_=fixture();payload=oracle['owned_storage']['encoding_hex']
        copies=[{'encoding_hex':payload,'oracle_hex':payload}]+[{'encoding_hex':'01'*32,'oracle_hex':'01'*32} for _ in range(6)]
        oracle['gpu_readback']={'scope':'after_first_completed_fixture_submission','guard_bytes_verified':224,'source_usage_unchanged':True,'copies':copies}
        result=descriptor_contents.resolve_owned_fixture(data,oracle)
        self.assertTrue(result['gpu_storage_readback_verified']);self.assertEqual(result['owned_uniform_payload_bytes'],192)
        copies[0]['encoding_hex']='00'*48
        with self.assertRaisesRegex(ValueError,'GPU descriptor readback differs'):descriptor_contents.resolve_owned_fixture(data,oracle)
        copies[0]['encoding_hex']=payload;copies[1]['encoding_hex']='02'*32
        with self.assertRaisesRegex(ValueError,'GPU uniform payload differs'):descriptor_contents.resolve_owned_fixture(data,oracle)

    def test_resolves_exact_sources_with_independent_compute_and_layout_query_revisions(self):
        data,oracle,_=fixture();r=descriptor_contents.resolve_owned_fixture(data,oracle)
        self.assertEqual(r['event_status_counts'],{'resolved_owned_sources':2,'unresolved_binding_state':1})
        self.assertEqual([[s['buffer_id'] for s in e['sources']] for e in r['events']],[[1,2],[5,6],[]])
        self.assertFalse(r['game_capture_supported']);self.assertFalse(r['dynamic_shader_accesses_proven'])
        self.assertNotIn('encoding_hex',str(r))

    def test_valid_wrong_compute_offset_selects_different_exact_pair(self):
        data,oracle,events=fixture();events[2]['offsets']=[0]
        r=descriptor_contents.resolve_owned_fixture(data,oracle)
        self.assertEqual([s['buffer_id'] for s in r['events'][1]['sources']],[1,2])

    def test_binding_address_offset_is_added_before_set_and_binding_offsets(self):
        data,oracle,events=fixture();events[0]['bindings'][0].update(address_offset=16,remaining_bytes=32)
        r=descriptor_contents.resolve_owned_fixture(data,oracle)
        self.assertEqual([s['buffer_id'] for s in r['events'][0]['sources']],[3,4])
        self.assertEqual(r['events'][1]['status'],'storage_out_of_bounds')

    def test_rebinding_buffers_invalidates_offsets_at_both_bind_points(self):
        data,oracle,events=fixture();events.insert(3,copy.deepcopy(events[0]))
        r=descriptor_contents.resolve_owned_fixture(data,oracle)
        self.assertEqual(r['event_status_counts'],{'unresolved_binding_state':3})

    def test_classic_binding_only_invalidates_its_bind_point(self):
        data,oracle,events=fixture();events.insert(3,copy.deepcopy(events[5]))
        r=descriptor_contents.resolve_owned_fixture(data,oracle)
        self.assertEqual([e['status'] for e in r['events']],['unresolved_binding_state','resolved_owned_sources','unresolved_binding_state'])

    def test_offset_bounds_alignment_unknown_bytes_and_binding_index_fail_closed(self):
        for offset,index,status in [(48,0,'storage_out_of_bounds'),(1,0,'unresolved_binding_state'),
                                    (8,0,'unknown_encoding'),(0,1,'unresolved_binding_state')]:
            with self.subTest(offset=offset,index=index):
                data,oracle,events=fixture();events[1].update(offsets=[offset],indices=[index])
                r=descriptor_contents.resolve_owned_fixture(data,oracle)
                self.assertEqual(r['events'][0]['status'],status);self.assertEqual(r['events'][0]['sources'],[])

    def test_equal_encodings_for_different_sources_are_ambiguous(self):
        data,oracle,_=fixture();r=data['descriptor_query_receipts'];r[1]['encoding_hex']=r[0]['encoding_hex']
        oracle['queries'][1]['encoding_hex']=r[0]['encoding_hex'];storage=oracle['owned_storage']
        blob=bytearray.fromhex(storage['encoding_hex']);blob[4:8]=bytes.fromhex(r[0]['encoding_hex'])
        storage['encoding_hex']=storage['after_encoding_hex']=blob.hex()
        result=descriptor_contents.resolve_owned_fixture(data,oracle)
        self.assertEqual(result['events'][0]['status'],'ambiguous_encoding');self.assertEqual(result['events'][0]['sources'],[])

    def test_missing_layout_unknown_offset_and_other_generations_are_not_guessed(self):
        for change in ('missing','offset','generation','gpl','partial','unknown_graphics'):
            data,oracle,events=fixture()
            if change=='missing':events[1]['layout_snapshot_id']=100
            elif change=='offset':data['descriptor_set_layouts'][-1]['bindings'][0]['offset_known']=False
            elif change=='generation':data['pipeline_layouts'][0]['id']=99
            elif change=='gpl':data['pipelines'][0]['pipeline_state']={'linked_libraries':True,'is_library':False}
            elif change=='unknown_graphics':data['pipelines'][0]['pipeline_state']=None
            else:events[1]['first_set']=2
            result=descriptor_contents.resolve_owned_fixture(data,oracle)
            self.assertNotEqual(result['events'][0]['status'],'resolved_owned_sources',change)

    def test_released_noncoherent_or_unknown_source_memory_is_unresolved(self):
        for key,value in [('allocation_live',False),('property_flags',2),('binding_complete',False),('mapped',False)]:
            data,oracle,_=fixture();data['buffer_memory'][0][key]=value
            result=descriptor_contents.resolve_owned_fixture(data,oracle)
            self.assertEqual(result['events'][0]['status'],'unresolved_source');self.assertEqual(result['events'][0]['sources'],[])

    def test_requires_closed_fixture_contract_stable_bytes_and_complete_catalog(self):
        for change in ('scope','writers','completed','changed','corrupt','catalog','storage','two_submissions','truncated'):
            data,oracle,_=fixture();s=oracle['owned_storage']
            if change=='scope':s['scope']='game'
            elif change=='writers':s['coherent_exclusive_immutable_storage']=False
            elif change=='completed':s['completed_submissions']=0
            elif change=='changed':s['after_encoding_hex']='00'*48
            elif change=='corrupt':s['encoding_hex']=s['after_encoding_hex']='00'*48
            elif change=='catalog':data['descriptor_query_catalog_truncated']=True
            elif change=='storage':data['buffer_memory'][-1]['mapped']=False
            elif change=='two_submissions':data['submissions']*=2
            else:data['truncated']=True
            with self.subTest(change=change),self.assertRaises(ValueError):descriptor_contents.resolve_owned_fixture(data,oracle)


if __name__=='__main__':unittest.main()
