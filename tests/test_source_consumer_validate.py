"""Structural receipt falsification controls, with no executable or GPU use."""
from copy import deepcopy
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import source_consumer_validate as audit


def report():
    target=dict(zip(audit.COPY_FIELDS,(99,1,2,3,4)))
    result={'observation_only':True,'observer_session':99,'gpu_commands_inserted':0,
            'application_calls_rejected':0,'serialization_truncated':False,
            'consumer_observation_enabled':True,
            'candidate_images':[],
            'live_event_nodes':1,'snapshot_event_references':1,
            'limits':{'live_events':8192,'snapshot_references':32768,'reports':8},
            'consumer_limits':{'events_per_report':512,'effective_events_per_report':256,
                               'effective_prefix_events_per_report':256,'occurrences_per_report':8,
                               'descriptor_queries':64,'descriptor_queries_per_report':16,
                               'descriptor_bytes_per_query':256,'color_attachments':8,
                               'swapchains':16,'swapchain_images':64},
            'descriptor_image_query_catalog':[],
            'descriptor_query_catalog_scope':'live_bounded_cache_at_report',
            'descriptor_query_nodes':0,'descriptor_query_payload_bytes':0,
            'descriptor_query_calls':0,'descriptor_query_evicted':0,
            'descriptor_query_deduplicated':0,'descriptor_query_unknown':0,'late_descriptor_callbacks':0,
            'reports':[{'target_copy':target,'target_submission':10,'target_batch':0,'target_command':0,
                        'consumer_window':{'target_copy':deepcopy(target),'closed':False,'truncated':False,
                                           'descriptor_selected':False,'gpu_read_proven':False,
                                           'placement_known':False,'gpu_execution_order_known':False,
                                           'presentation_receipt_resource_bridge_known':False,
                                           'separate_stencil_and_depth_resolve_complete':False,'event_count':0,
                                           'observed_occurrences':0,'occurrences':[],
                                           'descriptor_image_queries':[]}}]}
    result.update({key:False for key in audit.NO_INFERENCE})
    return result


def query():
    return {'query':1,'image_use':{'observer_session':99,'image':1,'view':7,
                                  'aspects':2,'base_mip':0,'mip_count':1,
                                  'base_layer':0,'layer_count':1,'image_known':True,
                                  'view_known':True,'metadata_complete':True},
            'type':2,'layout':5,'input_chain_complete':True,'output_size':4,
            'complete':True,'bytes_complete':True,'bytes_hex':'01020304',
            'bytes_fnv1a64':audit.fnv1a(bytes.fromhex('01020304')),
            'before_observed_submission':0,'after_observed_submission':0,
            'descriptor_selected':False,'gpu_read_proven':False,'placement_known':False,'sampler_state_known':False}


class SourceConsumerAudit(unittest.TestCase):
    def window(self,data):
        return data['reports'][0]['consumer_window']

    def test_strict_json_refuses_duplicate_fields_and_nonfinite_numbers(self):
        for raw in ('{"a":1,"a":2}','{"n":NaN}','{"n":Infinity}'):
            with self.subTest(raw=raw),self.assertRaises(ValueError): audit.decode(raw)
        self.assertEqual(audit.decode('{"n":1}'),{'n':1})

    def test_default_absence_is_distinct_from_enabled_missing_window(self):
        data=report(); data['reports'][0].pop('consumer_window');data.pop('consumer_observation_enabled')
        self.assertEqual(audit.audit_history(data,enabled=False)['consumer_windows'],0)
        with self.assertRaises(ValueError): audit.audit_history(data,enabled=True)
        with self.assertRaises(ValueError): audit.audit_history(report(),enabled=False)

    def test_query_is_not_placement_selection_or_gpu_read(self):
        data=report(); self.window(data)['descriptor_image_queries']=[query()]
        self.assertEqual(audit.audit_history(data,enabled=True)['descriptor_query_references'],1)
        for field in ('descriptor_selected','gpu_read_proven','placement_known'):
            changed=deepcopy(data); self.window(changed)['descriptor_image_queries'][0][field]=True
            with self.subTest(field=field),self.assertRaises(ValueError): audit.audit_history(changed)

    def test_query_bytes_size_pending_and_lifetime_refusals(self):
        changes=[lambda q:q.update(bytes_hex='01 02 03 04'),lambda q:q.update(output_size=3),
                 lambda q:q.update(complete=False),lambda q:q.update(bytes_hex='00'*257,output_size=257),
                 lambda q:q['image_use'].update(observer_session=100),
                 lambda q:q['image_use'].update(image=3),lambda q:q['image_use'].update(view=0),
                 lambda q:q.update(type=6),lambda q:q.update(output_size=True)]
        for change in changes:
            data=report(); value=query(); change(value); self.window(data)['descriptor_image_queries']=[value]
            with self.subTest(change=change),self.assertRaises(ValueError): audit.audit_history(data)

    def test_partial_opaque_bytes_are_valid_without_promoting_completeness(self):
        data=report(); value=query(); value.update(bytes_complete=False,output_size=8,bytes_fnv1a64=0)
        value['input_chain_complete']=False
        value['image_use']['metadata_complete']=False
        self.window(data)['descriptor_image_queries']=[value]
        self.assertEqual(audit.audit_history(data)['descriptor_query_references'],1)

    def test_forward_window_cannot_change_target_or_cross_the_copy_prefix(self):
        data=report(); window=self.window(data)
        window['target_copy']['observer_session']=100
        with self.assertRaises(ValueError): audit.audit_history(data)
        data=report(); window=self.window(data); window['observed_occurrences']=1
        window['occurrences']=[{'submission':10,'complete':True,'result':0,
                                'commands':[{'recording':3,'batch':0,'command':0,
                                             'events':[{'ordinal':4,'kind':'blit_image'}]}]}]
        with self.assertRaises(ValueError): audit.audit_history(data)
        window['occurrences'][0]['commands'][0]['events'][0]['ordinal']=5
        window['event_count']=1
        self.assertEqual(audit.audit_history(data)['forward_event_references'],1)
        window['occurrences'][0]['submission']=9
        with self.assertRaises(ValueError): audit.audit_history(data)

    def test_bounded_queries_occurrences_and_events(self):
        for count in (9,True):
            data=report(); self.window(data)['observed_occurrences']=count
            with self.assertRaises(ValueError): audit.audit_history(data)
        data=report(); self.window(data)['descriptor_image_queries']=[query(),query()]
        with self.assertRaises(ValueError): audit.audit_history(data)
        data=report(); self.window(data)['observed_occurrences']=1
        self.window(data)['occurrences']=[{'submission':11,'complete':False,'result':1,
                                         'commands':[{'recording':3,'batch':0,'command':0,
                                                      'events':[{'ordinal':n+5} for n in range(513)]}]}]
        with self.assertRaises(ValueError): audit.audit_history(data)

    def test_gpu_scene_claims_and_nonzero_insertions_are_refused(self):
        for field in audit.NO_INFERENCE:
            data=report(); data[field]=True
            with self.subTest(field=field),self.assertRaises(ValueError): audit.audit_history(data)
        for field in ('gpu_commands_inserted','application_calls_rejected'):
            data=report(); data[field]=1
            with self.subTest(field=field),self.assertRaises(ValueError): audit.audit_history(data)

    def test_valid_shortened_serialization_envelope_stays_incomplete(self):
        data={'observer_session':99,'observation_only':True,'gpu_execution_order_known':False,
              'serialization_truncated':True,'reports':[]}
        self.assertTrue(audit.audit_history(data)['serialization_truncated'])
        data['reports']=[{}]
        with self.assertRaises(ValueError): audit.audit_history(data)

    def test_wsi_modes_keep_history_and_fixture_constant(self):
        base=['/usr/bin/env','D4VR_DEPTH_SOURCE_HISTORY=1','D4VR_SOURCE_CONSUMER_FIXTURE=1']
        flag='D4VR_SOURCE_CONSUMER_HISTORY'
        self.assertFalse(audit.command_mode(base+[flag+'=0'],flag))
        self.assertTrue(audit.command_mode(base+[flag+'=1'],flag))
        for command in (base,base+[flag+'=1',flag+'=0'],[flag+'=1'],
                        base+[flag+'=1','D4VR_FRAME_TRACE=1']):
            with self.subTest(command=command),self.assertRaises(ValueError): audit.command_mode(command,flag)

    def original_rows(self):
        labels=[('source',0),('destination',0),('twin0',0),('source',16),
                ('destination',16),('twin1',32),('source',48),('destination',48)]
        identity={'source':(101,201),'destination':(102,202),'twin0':(103,203),'twin1':(103,203)}
        return [dict(ordinal=n,label=label,frame=frame,image=identity[label][0],view=identity[label][1],
                     descriptor_type=2,layout=1,size=4,bytes_hex=f'{n:02x}'*4,
                     descriptor_bound=False,shader_read_verified=False)
                for n,(label,frame) in enumerate(labels,1)]

    def test_same_run_original_query_bytes_are_compared_without_a_raw_handle_join(self):
        rows=self.original_rows(); data=report()
        first=query(); first.update(layout=1,bytes_hex=rows[0]['bytes_hex'])
        second=deepcopy(first); second.update(query=2,bytes_hex=rows[1]['bytes_hex'])
        second['image_use'].update(image=2,view=8)
        self.window(data)['descriptor_image_queries']=[first,second]
        twin0=deepcopy(first);twin0.update(query=3,bytes_hex=rows[2]['bytes_hex']);twin0['image_use'].update(image=3,view=9)
        twin1=deepcopy(first);twin1.update(query=6,bytes_hex=rows[5]['bytes_hex']);twin1['image_use'].update(image=4,view=10)
        data.update(descriptor_image_query_catalog=[first,second,twin0,twin1],
                    descriptor_query_calls=8,descriptor_query_deduplicated=4,
                    candidate_images=[{'id':n} for n in (1,2,3,4)])
        result=audit.compare_queries(rows,[data],True)
        self.assertEqual(result['exact_unique_target_query_receipts'],2)
        self.assertEqual(result['application_query_calls'],8)
        # App raw handles are a different namespace from History lifetimes.
        self.assertNotEqual(rows[0]['image'],first['image_use']['image'])
        changed=deepcopy(data); self.window(changed)['descriptor_image_queries'][0]['bytes_hex']='00000000'
        with self.assertRaises(ValueError): audit.compare_queries(rows,[changed],True)
        changed=deepcopy(data); self.window(changed)['descriptor_image_queries'][0]['query']=3
        with self.assertRaises(ValueError): audit.compare_queries(rows,[changed],True)
        with self.assertRaises(ValueError): audit.compare_queries(rows,[data],False)

    def test_application_query_artifact_preserves_lifetimes_but_allows_new_opaque_encodings(self):
        import json
        with TemporaryDirectory() as temporary:
            where=Path(temporary); path=where/'consumer-descriptor-fixture.jsonl'
            rows=self.original_rows()
            # Repeated image/view queries may have different opaque bytes.
            path.write_text('\n'.join(json.dumps(row) for row in rows)+'\n')
            self.assertEqual(audit.owned_query_rows(where),rows)
            for mutate in (lambda r:r[0].update(descriptor_bound=True),
                           lambda r:r[3].update(view=999),lambda r:r[1].update(ordinal=1),
                           lambda r:r[0].update(bytes_hex='01 01 01 01')):
                changed=deepcopy(rows); mutate(changed)
                path.write_text('\n'.join(json.dumps(row) for row in changed)+'\n')
                with self.assertRaises(ValueError): audit.owned_query_rows(where)


if __name__=='__main__':
    unittest.main()
