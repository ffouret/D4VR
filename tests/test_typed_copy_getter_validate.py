"""Causal CPU tests for immutable original getter and publication provenance."""
from __future__ import annotations
import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import typed_copy_getter_validate as audit
import typed_copy_source_validate as old_source
import test_typed_copy_source_validate as old_tests


def ref(raw,generation):return dict(raw=raw,generation=generation)


def wire():
    # Authored finite CPU timeline: five creates; CPU#1, CPU#2, INC#3;
    # publication, visible CPU, publication, GPU; three mutations; cutoff.
    w=old_tests.wire()
    w.update(schema_version=2,purpose=audit.PURPOSE,source_runtime_relation_known=False,
             same_run_bind_provenance_known=False,getter_to_bind_relation_known=False)
    w['source'].update(source_calls=13,source_returns=13,observer_events=3)
    p=w['native_wire']['producer'];p['clock']=29
    s=w['native_wire']['snapshot'];s['at_transition']=29
    for i,event in enumerate(s['events']):event.update(entry=23+2*i,**{'return':24+2*i})
    for target,event in zip(w['source']['mutations'],s['events']):target.update(entry=event['entry'],**{'return':event['return']})
    rows=[]
    def getter(ident,kind,entry,returned,obj,pointer,value):
        inc=kind=='increment'
        rows.append(dict(id=ident,epoch=7,kind=kind,entry=entry,**{'return':returned},
                         object_kind='device' if inc else 'heap',source_object=obj,parent_device=ref(71,1),
                         argument_heap_type=0 if inc else None,caller_output_pointer=None if inc else pointer,
                         return_kind='UINT32' if inc else 'structure_pointer',returned_pointer=None if inc else pointer,
                         value_known=True,value=value,lifetime_valid_through_return=True,arithmetic_eligible=True,reason='none'))
    getter(6,'cpu_start',11,12,ref(81,1),101,901)
    getter(7,'cpu_start',13,14,ref(81,1),102,901)
    getter(8,'increment',15,16,ref(71,1),None,32)
    getter(9,'cpu_start',18,19,ref(82,2),103,1601)
    getter(10,'gpu_start',21,22,ref(82,2),104,5001)
    w['getters']=dict(calls=5,returns=5,ignored_unselected=0,ignored_faulted=0,dropped=0,
                      publication_policy='latest_eligible_RETURN_before_publication',
                      eligibility_scope='original_RETURN_operand_only_not_heap_span_or_bind',records=rows)
    w['publications']=[dict(id=1,kind='observer_heap_facts_publication',original_API=False,transition=17,epoch=7,
                            heap=ref(81,1),device=ref(71,1),heap_count=2,cpu_getter_id=7,increment_getter_id=8,
                            span_arithmetic_valid=True,heap_facts_called=True,published=True),
                       dict(id=2,kind='observer_heap_facts_publication',original_API=False,transition=20,epoch=7,
                            heap=ref(82,2),device=ref(71,1),heap_count=1,cpu_getter_id=9,increment_getter_id=8,
                            span_arithmetic_valid=True,heap_facts_called=True,published=True)]
    w['cutoff']=dict(kind='observer_capture_cutoff',original_API=False,transition=29,epoch=7)
    return w


def partial(w):
    w['native_wire']['producer'].update(faulted=True,gap=True)
    w['native_wire']['snapshot'].update(complete=False,gap=True)
    return w


def later_equal():
    w=wire();r=copy.deepcopy(w['getters']['records'][0]);r.update(id=14,entry=30,**{'return':31})
    w['sequence']=w['native_wire']['sequence']=2
    w['getters']['records'].append(r);w['getters'].update(calls=6,returns=6)
    w['source'].update(source_calls=14,source_returns=14,observer_events=4)
    w['native_wire']['producer']['clock']=32;w['native_wire']['snapshot']['at_transition']=32
    w['cutoff']['transition']=32
    return w


class GetterAuditTests(unittest.TestCase):
    def setUp(self):
        p=patch.object(subprocess,'Popen',side_effect=AssertionError('Pure audit starts no process'))
        p.start();self.addCleanup(p.stop)
    def denied(self,w):
        try:r=audit.audit_getter_wire(w)
        except ValueError:return
        self.assertFalse(r['prototype_source_history_known'])
    def test_exact_delayed_prerequisite_ID(self):
        r=audit.audit_getter_wire(wire())
        self.assertTrue(r['prototype_source_history_known'])
        self.assertEqual(r['publications'][0]['cpu_getter_id'],7)
        self.assertEqual(r['getter_summary']['retained_getters'],5)
        self.assertEqual(r['native_model_replay']['history']['selected_version']['origin']['resource'],ref(101,1))
    def test_equal_value_other_original_is_not_the_prerequisite(self):
        w=wire();w['publications'][0]['cpu_getter_id']=6;self.denied(w)
    def test_later_equal_original_does_not_rewrite_publication(self):
        w=later_equal();r=audit.audit_getter_wire(w)
        self.assertTrue(r['prototype_source_history_known']);self.assertEqual(r['publications'][0]['cpu_getter_id'],7)
        w['publications'][0]['cpu_getter_id']=14;self.denied(w)
    def test_v1_unmodified_and_not_retagged_as_v2(self):
        old=old_tests.wire();before=old_source.audit_source_wire(old)
        self.assertTrue(before['prototype_source_history_known'])
        with self.assertRaises(ValueError):audit.audit_getter_wire(old)
        self.assertEqual(before,old_source.audit_source_wire(old))
        with self.assertRaises(ValueError):old_source.audit_source_wire(wire())
    def test_historical_operands_do_not_promote_bind_or_runtime(self):
        r=audit.audit_getter_wire(wire())
        for key in audit.FALSE_FACTS+('source_runtime_relation_known','same_run_bind_provenance_known','getter_to_bind_relation_known','public_device_create_result_known'):
            self.assertIs(r[key],False)
            w=wire();w[key]=True;self.denied(w)
    def test_publication_is_observer_not_original_API(self):
        for where in ('publications','cutoff'):
            w=wire();o=w[where][0] if where=='publications' else w[where];o['original_API']=True;self.denied(w)
    def test_all_clock_occurrences_distinct(self):
        for at in (1,2,7,8,11,12,13,14,15,16,18,19,21,22,23,24,25,26,27,28,29):
            w=wire();w['publications'][0]['transition']=at;self.denied(w)
    def test_factory_cannot_use_a_getter_or_publication_time(self):
        for at in (11,12,16,17,19,20):
            w=wire();w['source'].update(factory_calls=1,factory_returns=1,source_calls=14,source_returns=14)
            w['source']['factory_records']=[dict(entry=at,**{'return':at+1},hresult=0,raw_internal_output=999,outcome='outer_factory_only_unknown')]
            self.denied(w)
    def test_getter_epoch_id_type_and_bool_strict(self):
        for key,value in (('epoch',8),('epoch',True),('id',True),('id',99),('id',-1),('entry',True),('return',True)):
            w=wire();w['getters']['records'][0][key]=value;self.denied(w)
    def test_distinct_equal_calls_do_not_collapse_or_reorder(self):
        w=wire();w['getters']['records'][1]['id']=6;self.denied(w)
        w=wire();w['getters']['records'][0],w['getters']['records'][1]=w['getters']['records'][1],w['getters']['records'][0];self.denied(w)
    def test_token_ID_rank_counts_real_source_BEGIN_not_getter_index(self):
        w=wire()
        for row in w['getters']['records']:row['id']-=5
        for pub in w['publications']:
            pub['cpu_getter_id']-=5;pub['increment_getter_id']-=5
        self.denied(w)
    def test_original_pointer_return_ABI_and_unknown_payload(self):
        for change in (dict(returned_pointer=0),dict(returned_pointer=102),dict(caller_output_pointer=0),dict(return_kind='UINT32')):
            w=wire();w['getters']['records'][0].update(change);self.denied(w)
    def test_increment_enum32_and_UINT32_return_are_different(self):
        for key,value in (('argument_heap_type',True),('argument_heap_type',1),('argument_heap_type',1<<32),('value',1<<32),('caller_output_pointer',0),('returned_pointer',0),('return_kind','structure_pointer')):
            w=wire();w['getters']['records'][2][key]=value;self.denied(w)
    def test_CPU_GPU_method_and_saved_generation(self):
        for key,value in (('kind','gpu_start'),('object_kind','device'),('source_object',ref(82,2)),('parent_device',ref(72,1)),('source_object',ref(81,99))):
            w=wire();w['getters']['records'][1][key]=value;self.denied(w)
    def test_missing_CPU_or_increment_never_invents_native_heapFacts(self):
        for index in (1,2,3):
            w=wire();w['getters']['records'].pop(index);w['getters']['calls']-=1;w['getters']['returns']-=1;self.denied(w)
        w=wire();w['publications'].pop();self.denied(w)
    def test_missing_GPU_remains_history_only(self):
        w=wire();w['getters']['records'].pop();w['getters']['calls']-=1;w['getters']['returns']-=1
        w['source'].update(source_calls=12,source_returns=12)
        for event,row in zip(w['native_wire']['snapshot']['events'],w['source']['mutations']):
            event['entry']-=2;event['return']-=2;row['entry']-=2;row['return']-=2
        w['native_wire']['producer']['clock']=27;w['native_wire']['snapshot']['at_transition']=27;w['cutoff']['transition']=27
        r=audit.audit_getter_wire(w);self.assertTrue(r['prototype_source_history_known'])
        self.assertFalse(r['same_run_bind_provenance_known'])
        self.assertEqual(r['getter_summary']['retained_getters'],4)
    def test_zeros_must_not_be_claimed_eligible_or_unknown(self):
        w=partial(wire());row=w['getters']['records'][-1];row.update(value=0,arithmetic_eligible=False,reason='zero_value')
        r=audit.audit_getter_wire(w);self.assertFalse(r['prototype_source_history_known'])
        self.assertIs(r['getters']['records'][-1]['value_known'],True)
        row['arithmetic_eligible']=True;self.denied(w)
    def test_null_and_mismatch_never_read_another_payload(self):
        for returned in (0,555):
            w=partial(wire());r=w['getters']['records'][-1];r.update(returned_pointer=returned,value_known=False,value=None,arithmetic_eligible=False,reason='output_pointer_mismatch')
            self.assertFalse(audit.audit_getter_wire(w)['prototype_source_history_known'])
            r['value']=5001;self.denied(w)
    def test_pending_fields_no_invented_RETURN(self):
        w=partial(wire());r=w['getters']['records'][-1];r.update(**{'return':0},returned_pointer=None,value_known=False,value=None,lifetime_valid_through_return=False,arithmetic_eligible=False,reason='pending')
        w['getters']['returns']-=1;w['source']['source_returns']-=1;w['source']['pending']=1
        self.assertFalse(audit.audit_getter_wire(w)['prototype_source_history_known'])
        r['value_known']=True;r['value']=0;self.denied(w)
    def test_release_during_getter_preserves_observed_historical_value(self):
        w=later_equal();r=w['getters']['records'][-1]
        r.update(**{'return':33},lifetime_valid_through_return=False,arithmetic_eligible=False,reason='lifetime_changed')
        w['native_wire']['snapshot']['heaps'][0].update(release_entry=31,release_return=32)
        w['source'].update(source_calls=15,source_returns=15)
        w['native_wire']['producer']['clock']=34;w['native_wire']['snapshot']['at_transition']=34;w['cutoff']['transition']=34;partial(w)
        r2=audit.audit_getter_wire(w);self.assertTrue(r2['getters']['records'][-1]['value_known'])
        self.assertEqual(r2['getters']['records'][-1]['value'],901)
        r['lifetime_valid_through_return']=True;self.denied(w)
    def test_later_gap_does_not_retag_earlier_operand(self):
        w=partial(wire());r=audit.audit_getter_wire(w)
        self.assertTrue(r['getters']['records'][0]['arithmetic_eligible'])
        self.assertFalse(r['prototype_source_history_known'])
    def test_counters_observer_marks_and_ignored_calls(self):
        for section,key,value in (('source','observer_events',0),('source','observer_events',True),('source','source_calls',100000),('getters','calls',True),('getters','returns',0),('getters','ignored_unselected',1),('getters','dropped',1)):
            w=wire();w[section][key]=value;self.denied(w)
    def test_retained_getters_cannot_be_hidden_in_observer_marks(self):
        w=wire();w['source'].update(source_calls=10,source_returns=10,observer_events=9)
        self.denied(w)
    def test_first_seal_cannot_invent_older_observer_cutoffs(self):
        w=wire();w['source']['observer_events']=5;w['native_wire']['producer']['clock']=31
        w['native_wire']['snapshot']['at_transition']=31;w['cutoff']['transition']=31
        self.denied(w)
    def test_publications_cannot_be_reconstructed_after_typed_mutations(self):
        w=wire();w['publications'][0]['transition']=30;w['publications'][1]['transition']=31
        w['source']['observer_events']=6;w['cutoff']['transition']=32
        w['native_wire']['producer']['clock']=32;w['native_wire']['snapshot']['at_transition']=32
        self.denied(w)
    def test_full_outer_budget_UTF8_duplicates(self):
        text=json.dumps(wire())
        for data in (text.encode('utf16'),' '*16385,text.replace('"schema_version": 2','"schema_version":2,"schema_version":2',1)):
            with self.assertRaises(ValueError):audit.audit_getter_wire(data)
    def test_publication_span_guard_and_no_invented_return(self):
        for key,value in (('heap_count',True),('heap_count',3),('span_arithmetic_valid',False),('heap_facts_called',False),('published',1),('cpu_getter_id',True),('increment_getter_id',6)):
            w=wire();w['publications'][0][key]=value;self.denied(w)
    def test_pub_epoch_parent_heap_and_cutoff_exact(self):
        for key,value in (('epoch',8),('heap',ref(82,2)),('device',ref(72,1)),('id',2)):
            w=wire();w['publications'][0][key]=value;self.denied(w)
        w=wire();w['cutoff']['transition']=28;self.denied(w)
    def test_frozen_output_and_nested_model_remain_exact(self):
        w=wire();r=audit.audit_getter_wire(w);frozen=copy.deepcopy(r)
        self.assertEqual(r['native_model_replay'],audit.native.audit_wire(w['native_wire']))
        w['getters']['records'][0]['value']=999;w['publications'][0]['cpu_getter_id']=6
        self.assertEqual(r,frozen)
    def test_eighth_ninth_retained_quota(self):
        w=wire();w['getters']['records']*=2;self.denied(w)
    def test_silent_overwrite_observational_twins(self):
        a,b=wire(),copy.deepcopy(wire());self.assertEqual(audit.audit_getter_wire(a),audit.audit_getter_wire(b))
        self.assertFalse(audit.audit_getter_wire(a)['current_contents_at_draw_known'])

def bridge(flag, path):
    executable = Path(path).resolve(strict=True)
    result = subprocess.run([str(executable), '--emit-json'], check=True, capture_output=True, timeout=60)
    if result.stderr:
        raise ValueError('Unexpected native CPU stderr: ' + result.stderr.decode('utf8', 'replace')[:1024])
    if flag == '--native-wire':
        report = audit.audit_native_samples(result.stdout)
    else:
        report = audit.audit_integration_samples(result.stdout)
    print(json.dumps(report, sort_keys=True))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] in ('--native-wire', '--integration-wire'):
        bridge(sys.argv[1], sys.argv[2])
    else:
        unittest.main()
