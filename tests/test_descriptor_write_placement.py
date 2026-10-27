import copy
import gc
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import descriptor_write_placement as d


def snapshot(ordinal=4):
    """Independent owned facts with nonzero map, bind and descriptor offsets."""
    return dict(schema_version=1,domain='owned_fixture',observer=77,
        query=dict(ordinal=ordinal,returned=True,type='sampled_image',data_size=32,
                   output_pointer=0x10040,output_hex='a1'*32,info_complete=True,
                   input=dict(kind='image_view',raw=400,generation=7,live_before=True,live_at_return=True)),
        epochs={k:[9,9] for k in ('memory','buffer','layout','selection')},
        complete={k:True for k in ('allocations','maps','buffers','layouts','properties')},
        properties=dict(alignment=64,max_resource_range=4096,max_sampler_range=4096,
                        robust_buffer_access=False,pnext_complete=True,
                        sizes=dict(sampler=16,sampled_image=32,uniform_buffer=32,storage_buffer=32,
                                   uniform_texel_buffer=16,storage_texel_buffer=16,robust_uniform_buffer=48,
                                   robust_storage_buffer=64,robust_uniform_texel_buffer=24,robust_storage_texel_buffer=24)),
        allocations=[dict(raw=100,generation=1,size=8192,live=True,host_visible=True,host_coherent=True,external=False)],
        maps=[dict(allocation=dict(raw=100,generation=1),generation=3,offset=1024,size=4096,pointer=0x10000,live=True)],
        buffers=[dict(raw=200,generation=2,size=2048,usage=0x620000,flags=0,live=True,sparse=False,external=False,
                      binding=dict(allocation=dict(raw=100,generation=1),offset=512,result=0,complete=True),
                      address=dict(value=0x100000,known=True))],
        layouts=[dict(raw=300,generation=4,revision=2,size=128,flags=16,complete=True,pnext_complete=True,
                      bindings=[dict(binding=1,type='sampled_image',count=2,offset=32,offset_known=True,stride=32,
                                     mutable_types=[],immutable_samplers=False,flags=0)])],
        slots=[dict(set=1,layout=dict(raw=300,generation=4),layout_revision=2,binding=1,index=1,type='sampled_image',
                    buffer=dict(raw=200,generation=2),buffer_index=0,bound_address=0x100100,set_offset=256,
                    state_epoch=9,bind_ordinal=1,offset_ordinal=2,known=True)])


def relocate_query(s,buffer_byte):
    # Written independently from the model: allocation starts at bind byte512;
    # the map starts at allocation byte1024, so pointer=base+512+byte-1024.
    s['query']['output_pointer']=0x10000+512+buffer_byte-1024


class ModelTests(unittest.TestCase):
    def assert_unknown(self,s):
        r=d.analyze(s);self.assertEqual(r['status'],'unknown');self.assertFalse(r[d.SCOPE])
        for key in d.FALSE_FACETS:self.assertIs(r[key],False)
        return r
    def test_nonzero_offsets_exact_relation(self):
        r=d.analyze(snapshot());self.assertTrue(r[d.SCOPE]);self.assertTrue(r['typed_write_seen'])
        f=r['slots'][0]['relation']
        self.assertEqual((f['allocation_byte'],f['buffer_byte'],f['expected_buffer_byte']),(1088,576,576))
        self.assertEqual((f['bind_memory_offset'],f['bound_address_displacement'],f['set_offset'],f['binding_offset'],f['descriptor_stride']),(512,256,256,32,32))
        self.assertEqual(f['map_generation'],3)
        for key in d.FALSE_FACETS:self.assertIs(r[key],False)
    def test_json_text_and_input_mutation_are_detached(self):
        s=snapshot();a=d.analyze(s);self.assertEqual(a,d.analyze(json.dumps(s)))
        s['maps'][0]['generation']=99;s['query']['input']['raw']=999
        self.assertEqual(a['slots'][0]['relation']['map_generation'],3)
        self.assertEqual(a['typed_input']['raw'],400)
    def test_twins_same_bytes_never_change_typed_identity(self):
        a=snapshot();b=copy.deepcopy(a);b['query']['input']['raw']=401
        ra,rb=d.analyze(a),d.analyze(b)
        self.assertTrue(ra[d.SCOPE] and rb[d.SCOPE]);self.assertEqual(ra['output_hex'],rb['output_hex'])
        self.assertNotEqual(ra['typed_input'],rb['typed_input'])
    def test_sampler_uses_its_own_size_and_type(self):
        s=snapshot();s['query'].update(type='sampler',data_size=16,output_hex='b2'*16)
        s['query']['input']['kind']='sampler';s['slots'][0].update(type='sampler',set=0)
        s['layouts'][0]['bindings'][0].update(type='sampler',stride=16)
        relocate_query(s,560)
        r=d.analyze(s);self.assertTrue(r[d.SCOPE]);self.assertEqual(r['slots'][0]['relation']['descriptor_size'],16)
    def test_two_selected_logical_slots_with_same_span_are_ambiguous(self):
        s=snapshot();other=copy.deepcopy(s['slots'][0]);other['set']=2;s['slots'].append(other)
        r=self.assert_unknown(s);self.assertIn('ambiguous_selected_slots',r['reasons'])
    def test_other_slot_scratch_and_partial_not_selected(self):
        for ptr in (0x10060,0x90000,0x10ff0,0x11000):
            with self.subTest(pointer=ptr):
                s=snapshot();s['query']['output_pointer']=ptr;self.assert_unknown(s)
        for size in (0,16,33,257):
            s=snapshot();s['query']['data_size']=size;s['query']['output_hex']=None
            self.assertFalse(self.assert_unknown(s)['typed_write_seen'])
    def test_overlapping_map_alias_even_partial_refused(self):
        for ptr,size in ((0x10000,4096),(0x10050,16)):
            s=snapshot();m=copy.deepcopy(s['maps'][0]);m['generation']=4;m['pointer']=ptr;m['size']=size
            s['maps'].append(m);self.assert_unknown(s)
    def test_buffer_alias_ignores_usage_but_not_disjoint_binding(self):
        for usage in (0,0x620000):
            s=snapshot();alias=copy.deepcopy(s['buffers'][0]);alias.update(raw=201,usage=usage)
            alias['address']=dict(value=0,known=False);s['buffers'].append(alias);self.assert_unknown(s)
        s=snapshot();alias=copy.deepcopy(s['buffers'][0]);alias.update(raw=201,size=256,usage=0)
        alias['binding']['offset']=4096;alias['address']=dict(value=0,known=False);s['buffers'].append(alias)
        self.assertTrue(d.analyze(s)[d.SCOPE])
    def test_device_address_alias_refused(self):
        s=snapshot();alias=copy.deepcopy(s['buffers'][0]);alias['raw']=201;alias['binding']['offset']=4096
        s['buffers'].append(alias);self.assert_unknown(s)
    def test_failed_binding_and_incomplete_alias(self):
        s=snapshot();s['buffers'][0]['binding']['result']=-2;self.assert_unknown(s)
        s=snapshot();a=copy.deepcopy(s['buffers'][0]);a['raw']=201;a['binding']['complete']=False
        a['address']['known']=False;s['buffers'].append(a);self.assert_unknown(s)
        for complete in (False,True):
            s=snapshot();a=copy.deepcopy(s['buffers'][0]);a['raw']=201
            a['binding'].update(result=-2,complete=complete);a['address']['known']=False
            s['buffers'].append(a);self.assert_unknown(s)
    def test_concurrent_live_handle_generations_refused_dead_history_allowed(self):
        for key in ('allocations','buffers','layouts'):
            s=snapshot();old=copy.deepcopy(s[key][0]);old['generation']+=1;s[key].append(old)
            self.assert_unknown(s)
            if key!='layouts':
                old['live']=False;self.assertTrue(d.analyze(s)[d.SCOPE])
        s=snapshot();m=copy.deepcopy(s['maps'][0]);m.update(generation=4,pointer=0x20000)
        s['maps'].append(m);self.assert_unknown(s)
        m['live']=False;self.assertTrue(d.analyze(s)[d.SCOPE])
    def test_query_return_epoch_change_unmap_remap_rebind_or_reset(self):
        for name in ('memory','buffer','layout','selection'):
            s=snapshot();s['epochs'][name][1]=10;self.assert_unknown(s)
        for returned in (False,):
            s=snapshot();s['query']['returned']=returned;r=self.assert_unknown(s)
            self.assertEqual(r['epochs'],s['epochs']);self.assertEqual(r['query_ordinal'],4)
        s=snapshot();s['maps'][0]['live']=False;self.assert_unknown(s)
    def test_handle_generation_and_layout_revision_do_not_retag(self):
        mutations=[lambda s:s['allocations'][0].update(generation=2),lambda s:s['buffers'][0].update(generation=3),
                   lambda s:s['layouts'][0].update(generation=5),lambda s:s['layouts'][0].update(revision=3),
                   lambda s:s['query']['input'].update(live_at_return=False)]
        for mutate in mutations:
            s=snapshot();mutate(s);self.assert_unknown(s)
    def test_selection_must_exist_before_return_not_future_draw(self):
        for key in ('bind_ordinal','offset_ordinal'):
            for ordinal in (0,4,5):
                s=snapshot();s['slots'][0][key]=ordinal;self.assert_unknown(s)
        s=snapshot();s['slots'][0]['state_epoch']=8;self.assert_unknown(s)
        s=snapshot();s['slots'][0]['known']=False;self.assert_unknown(s)
    def test_mutable_stride_is_maximum_not_written_type_size(self):
        s=snapshot();b=s['layouts'][0]['bindings'][0]
        b.update(type='mutable',mutable_types=['sampled_image','storage_buffer'],stride=64)
        s['properties']['robust_buffer_access']=True;s['layouts'][0]['size']=192
        relocate_query(s,608)
        r=d.analyze(s);self.assertTrue(r[d.SCOPE]);self.assertEqual(r['slots'][0]['relation']['descriptor_stride'],64)
        relocate_query(s,576);self.assert_unknown(s)
        b['stride']=32;self.assert_unknown(s)
    def test_mutable_incomplete_duplicate_or_unknown_types(self):
        for types in ([],['sampled_image','sampled_image'],['sampled_image','unknown'],['sampled_image','mutable']):
            s=snapshot();s['layouts'][0]['bindings'][0].update(type='mutable',mutable_types=types);self.assert_unknown(s)
        s=snapshot();s['layouts'][0]['bindings'][0].update(type='mutable',mutable_types=['sampled_image','uniform_buffer'])
        s['properties']['robust_buffer_access']=None;self.assert_unknown(s)
    def test_robust_property_changes_max_stride_and_missing_refuses(self):
        s=snapshot();b=s['layouts'][0]['bindings'][0];b.update(type='mutable',mutable_types=['sampled_image','uniform_buffer'],stride=48)
        s['properties']['robust_buffer_access']=True;s['layouts'][0]['size']=160;relocate_query(s,592)
        self.assertTrue(d.analyze(s)[d.SCOPE])
        s['properties']['sizes']['robust_uniform_buffer']=None;self.assert_unknown(s)
    def test_unknown_chains_immutable_samplers_and_unsupported_types(self):
        changes=[lambda s:s['properties'].update(pnext_complete=False),lambda s:s['layouts'][0].update(pnext_complete=False),
                 lambda s:s['layouts'][0]['bindings'][0].update(immutable_samplers=True),lambda s:s['layouts'][0]['bindings'][0].update(flags=1),
                 lambda s:s['buffers'][0].update(sparse=True),lambda s:s['buffers'][0].update(external=True),
                 lambda s:s['allocations'][0].update(external=True),lambda s:s['query'].update(type='uniform_buffer'),
                 lambda s:s['query'].update(info_complete=False)]
        for change in changes:
            s=snapshot();change(s);self.assert_unknown(s)
        for k in snapshot()['complete']:
            s=snapshot();s['complete'][k]=False;self.assert_unknown(s)
    def test_address_arithmetic_overflow_each_formula(self):
        mutations=[lambda s:s['query'].update(output_pointer=d.MAX_U64-15),
                   lambda s:s['maps'][0].update(offset=d.MAX_U64-8),
                   lambda s:s['buffers'][0]['binding'].update(offset=d.MAX_U64-8),
                   lambda s:s['buffers'][0]['address'].update(value=d.MAX_U64-8),
                   lambda s:s['slots'][0].update(set_offset=d.MAX_U64-8),
                   lambda s:s['layouts'][0]['bindings'][0].update(offset=d.MAX_U64-8),
                   lambda s:s['layouts'][0]['bindings'][0].update(count=d.MAX_U64)]
        for mutate in mutations:
            s=snapshot();mutate(s);self.assert_unknown(s)
    def test_range_alignment_layout_and_index_bounds(self):
        changes=[lambda s:s['slots'][0].update(index=2),lambda s:s['slots'][0].update(set_offset=257),
                 lambda s:s['slots'][0].update(bound_address=0x100101),lambda s:s['properties'].update(max_resource_range=300),
                 lambda s:s['layouts'][0].update(size=63),lambda s:s['maps'][0].update(offset=8190)]
        for change in changes:
            s=snapshot();change(s);self.assert_unknown(s)
    def test_silent_copy_overwrite_identical_snapshot_honest_false_facets(self):
        # Different possible CPU ground truths; neither is supplied to analyze.
        direct_A=snapshot();hidden_copy_B=copy.deepcopy(direct_A)
        self.assertEqual(d.analyze(direct_A),d.analyze(hidden_copy_B))
        self.assertTrue(d.analyze(direct_A)[d.SCOPE])
        for k in d.FALSE_FACETS:self.assertFalse(d.analyze(hidden_copy_B)[k])
    def test_strict_json_types_keys_duplicates_and_budgets(self):
        bad=[lambda s:s.update(extra=1),lambda s:s.update(schema_version=True),lambda s:s['query'].update(output_pointer=True),
             lambda s:s['properties'].update(alignment=64.0),lambda s:s['query'].update(output_hex='zz'),
             lambda s:s['query'].update(output_pointer=-1),lambda s:s.update(domain='arbitrary_live_application')]
        for change in bad:
            s=snapshot();change(s)
            with self.assertRaises(ValueError):d.analyze(s)
        text=json.dumps(snapshot()).replace('"schema_version": 1','"schema_version": 1,"schema_version": 1')
        with self.assertRaises(ValueError):d.analyze(text)
        with self.assertRaises(ValueError):d.analyze(' '*16385)
        s=snapshot();s['allocations']*=9
        with self.assertRaises(ValueError):d.analyze(s)
    def test_no_process_or_memory_reader_needed(self):
        with patch('subprocess.Popen',side_effect=AssertionError('No process')):
            self.assertTrue(d.analyze(snapshot())[d.SCOPE])


class TicketTests(unittest.TestCase):
    def test_retained_receipt_pins_four_ticket_quota(self):
        reg=d.Registry();held=[]
        for n in range(1,5):
            t=reg.reserve(n);self.assertTrue(t.record(snapshot()));held.append(t.receipt())
        del t;gc.collect();self.assertEqual(reg.stats()['live_tickets'],4)
        for n in range(5,85):self.assertIsNone(reg.reserve(n))
        self.assertEqual(reg.stats()['refused_reservations'],80)
        held.pop();gc.collect();self.assertIsNotNone(reg.reserve(85))
    def test_old_receipt_survives_eighty_recycled_tickets_reset_reuse(self):
        reg=d.Registry();t=reg.reserve(1);t.record(snapshot());old=t.receipt();saved=old.report();del t
        for n in range(2,82):
            t=reg.reserve(n);s=snapshot();s['maps'][0]['generation']=n;s['query']['input']['generation']=n
            self.assertTrue(t.record(s));new=t.receipt();self.assertEqual(old.report(),saved)
            del t,new
        gc.collect();self.assertEqual(reg.stats()['live_tickets'],1);self.assertEqual(old.report(),saved)
    def test_three_slots_eight_occurrences_no_dedup_and_ninth_refusal(self):
        reg=d.Registry();t=reg.reserve(1)
        for n in range(8):self.assertTrue(t.record(snapshot(n+4)))
        self.assertFalse(t.record(snapshot(12)));r=t.receipt().to_dict()
        self.assertEqual(len(r['writes']),8);self.assertFalse(r['complete']);self.assertEqual(r['unavailable_reason'],'write_limit')
        self.assertLessEqual(len(t.receipt().report().encode()),16384)
        self.assertFalse(t.record(snapshot(13)))
    def test_slot_limit_across_snapshots_and_duplicate_query(self):
        reg=d.Registry();t=reg.reserve(1)
        for n in range(3):
            s=snapshot(n+4);s['slots'][0]['index']=n;self.assertTrue(t.record(s))
        s=snapshot(7);s['slots'][0]['index']=3;self.assertFalse(t.record(s))
        self.assertEqual(t.receipt().to_dict()['unavailable_reason'],'slot_limit')
        t=reg.reserve(2);self.assertTrue(t.record(snapshot()));self.assertFalse(t.record(snapshot()))
        self.assertEqual(t.receipt().to_dict()['unavailable_reason'],'query_order')
    def test_receipt_dict_mutation_and_copy_do_not_retag_or_release_pin(self):
        reg=d.Registry();t=reg.reserve(1);t.record(snapshot());r=t.receipt();copy_r=copy.copy(r)
        value=r.to_dict();value['writes'][0]['typed_input']['raw']=999
        self.assertEqual(copy_r.to_dict()['writes'][0]['typed_input']['raw'],400)
        del t,r;gc.collect();self.assertEqual(reg.stats()['live_tickets'],1)
        self.assertFalse(copy_r.to_dict()['current_contents_at_draw_known'])
    def test_deepcopied_receipts_share_the_quota_pin(self):
        reg=d.Registry();held=[]
        for n in range(1,5):
            t=reg.reserve(n);t.record(snapshot());original=t.receipt()
            held.append(copy.deepcopy(original));self.assertIs(held[-1],original)
        del t,original;gc.collect()
        self.assertEqual(reg.stats()['live_tickets'],4);self.assertIsNone(reg.reserve(5))
        self.assertEqual(reg.stats()['retained_writes'],4)
        held.clear();gc.collect();self.assertEqual(reg.stats()['live_tickets'],0)
    def test_serialization_limit_refuses_without_losing_old_values(self):
        reg=d.Registry();t=reg.reserve(1)
        s=snapshot();s['properties']['sizes']['sampled_image']=256
        s['query'].update(data_size=256,output_hex='c3'*256)
        s['layouts'][0].update(size=544);s['layouts'][0]['bindings'][0].update(stride=256)
        relocate_query(s,800)
        s['slots']=[dict(copy.deepcopy(s['slots'][0]),set=n) for n in range(3)]
        accepted=0
        for n in range(8):
            s['query']['ordinal']=4+n
            if not t.record(s):break
            accepted+=1
        r=t.receipt().to_dict();self.assertEqual(r['unavailable_reason'],'serialization_limit')
        self.assertGreater(accepted,0);self.assertEqual(len(r['writes']),accepted)
        self.assertLessEqual(len(t.receipt().report().encode()),16384)


if __name__=='__main__':unittest.main()
