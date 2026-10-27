"""Independent causal CPU inputs for the new private SOURCE control receipt.

Default discovery has no subprocess, filesystem or driver interaction. Real
new native wires are handled separately by the parent's explicit CPU bridge.
"""
from __future__ import annotations
import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import private_translator_control_validate as model
import typed_copy_getter_validate as getters
import test_typed_copy_bind_validate as independent_source

CLAIMS=('input_facts_attested','opaque_descriptor_decoded','actual_source_contents_at_copy_proven',
'current_contents_at_draw_known','unchanged_since_copy_proven','sampled_resource_GPU_read_proven',
'generic_PE_to_Unix_resource_mapping','renderer_owner_known','exclusive_phase_attribution_proven',
'global_API_absence_proven','old_process_or_backing_gate_repaired','public_device_create_result_known',
'source_runtime_relation_known','same_run_bind_provenance_known','getter_to_bind_relation_known',
'backing_known','loaded_binary_source_equivalence_proven')

def seed():
    return dict(schema_version=1,purpose='owned_private_translator_source_control_CPU_prototype',
      prototype_only=True,vendor_built=False,runtime_executed=False,
      admission=dict(selected_reserved=5,selected_committed=5,unselected=0,rejected=0,
                     pending_admissions=0,pending_originals=0,faulted=False),
      getter_wire=independent_source.source_wires()[0],claims={key:False for key in CLAIMS})

def partial(v):
    p=v['getter_wire']['native_wire'];p['producer'].update(faulted=True,gap=True)
    p['snapshot'].update(complete=False,gap=True)
    v['admission']['faulted']=True
    return v

def batch():
    return dict(schema_version=1,purpose='private_translator_control_CPU_samples',samples=[
      dict(name='new_private_A',expected_history_known=True,wire=seed()),
      dict(name='diagnostic_fault',expected_history_known=False,wire=partial(seed()))])

class PrivateControlTests(unittest.TestCase):
    def setUp(self):
        p=patch.object(subprocess,'Popen',side_effect=AssertionError('No child in pure tests'));p.start();self.addCleanup(p.stop)
    def denied(self,v):
        try:r=model.audit_private_source_wire(v)
        except ValueError:return
        self.assertFalse(r['private_source_history_known'])
    def test_new_purpose_is_CPU_history_only(self):
        r=model.audit_private_source_wire(seed());self.assertTrue(r['private_source_history_known'])
        self.assertIs(r['prototype_only'],True);self.assertIs(r['vendor_built'],False);self.assertIs(r['runtime_executed'],False)
        self.assertEqual(r['getter_audit']['native_model_replay']['history']['selected_version']['origin']['resource'],dict(raw=101,generation=1))
        self.assertEqual(r['claims'],{key:False for key in CLAIMS})
    def test_frozen_getter_result_exact_and_called_once(self):
        v=seed();real=getters.audit_getter_wire
        with patch.object(getters,'audit_getter_wire',wraps=real) as probe:r=model.audit_private_source_wire(v)
        self.assertEqual(probe.call_count,1);self.assertEqual(r['getter_audit'],real(v['getter_wire']))
    def test_legacy_child_cannot_be_retagged_as_new_receipt(self):
        child=seed()['getter_wire'];old=getters.audit_getter_wire(child)
        with self.assertRaises(ValueError):model.audit_private_source_wire(child)
        self.assertEqual(getters.audit_getter_wire(child),old)
    def test_inner_fault_gap_propagates_outer_but_late_outer_fault_can_be_independent(self):
        v=partial(seed());v['admission']['faulted']=False
        with self.assertRaises(ValueError):model.audit_private_source_wire(v)
        v=seed();v['admission']['faulted']=True
        self.assertTrue(getters.audit_getter_wire(v['getter_wire'])['prototype_source_history_known'])
        r=model.audit_private_source_wire(v);self.assertFalse(r['private_source_history_known'])
        self.assertTrue(r['getter_audit']['prototype_source_history_known'])
    def test_seventeen_stronger_facts_refused(self):
        for name in CLAIMS:
            v=seed();v['claims'][name]=True;self.denied(v)
    def test_runtime_vendor_and_schema_bool_are_not_promoted(self):
        for name,value in (('schema_version',True),('schema_version',2),('prototype_only',False),('vendor_built',True),('runtime_executed',True)):
            v=seed();v[name]=value;self.denied(v)
    def test_healthy_admission_requires_five_selected_commits(self):
        for count in (0,4,6):
            v=seed();v['admission'].update(selected_reserved=count,selected_committed=count);self.denied(v)
    def test_selected_role_accounting_exact(self):
        for key,value in (('selected_reserved',4),('selected_committed',4),('rejected',1)):
            v=seed();v['admission'][key]=value;self.denied(v)
    def test_seal_pending_never_claims_saved_complete(self):
        for key in ('pending_admissions','pending_originals'):
            v=seed();v['admission'][key]=1;self.denied(v)
    def test_faulted_source_diagnostic_preserves_full_child(self):
        v=partial(seed());r=model.audit_private_source_wire(v)
        self.assertFalse(r['private_source_history_known']);self.assertEqual(r['getter_audit'],getters.audit_getter_wire(v['getter_wire']))
    def test_rejected_selected_fault_cannot_claim_positive(self):
        v=partial(seed());v['admission'].update(selected_committed=4,rejected=1)
        r=model.audit_private_source_wire(v);self.assertFalse(r['private_source_history_known'])
        v['admission']['faulted']=False;self.denied(v)
    def test_healthy_unselected_cannot_exceed_original_ignored_count(self):
        for count in (1,13,(1<<64)-1):
            v=seed();v['admission']['unselected']=count;self.denied(v)
    def test_observed_ignored_creation_count_positive_lower_bound(self):
        v=seed();w=v['getter_wire'];w['source'].update(source_calls=13,source_returns=13,ignored_calls=1)
        w['native_wire']['producer']['clock']=29;w['native_wire']['snapshot']['at_transition']=29;w['cutoff']['transition']=29
        v['admission']['unselected']=1
        self.assertTrue(getters.audit_getter_wire(w)['prototype_source_history_known'])
        self.assertTrue(model.audit_private_source_wire(v)['private_source_history_known'])
    def test_faulted_counts_not_silently_normalized_into_healthy(self):
        v=partial(seed());v['admission']['unselected']=100
        r=model.audit_private_source_wire(v);self.assertFalse(r['private_source_history_known']);self.assertEqual(r['admission']['unselected'],100)
    def test_exact_integer_counter_widths(self):
        for key in ('selected_reserved','selected_committed','unselected','rejected','pending_admissions','pending_originals'):
            for value in (True,-1,1<<64):
                v=seed();v['admission'][key]=value;self.denied(v)
        v=seed();v['admission']['faulted']=0;self.denied(v)
    def test_child_nonce_sequence_and_native_flags_are_validated(self):
        v=seed();v['getter_wire']['run_id']='f'*32;self.denied(v)
        v=seed();v['getter_wire']['sequence']=2;self.denied(v)
        v=seed();v['getter_wire']['getter_to_bind_relation_known']=True;self.denied(v)
    def test_detached_result_no_input_or_future_alias(self):
        v=seed();r=model.audit_private_source_wire(v);saved=copy.deepcopy(r)
        v['admission']['unselected']=55;v['getter_wire']['getters']['records'].clear()
        self.assertEqual(r,saved)
        for _ in range(80):self.assertEqual(model.audit_private_source_wire(seed()),saved)
    def test_outer_complete_bytes_and_UTF8_duplicate_float(self):
        raw=json.dumps(seed(),separators=(',',':'))
        for v in (raw.encode('utf16'),'{"schema_version":1,"schema_version":1}',raw.replace('"selected_reserved":5','"selected_reserved":5.0'),raw+' '*16385):self.denied(v)
    def test_whole_report_budget_refuses_without_truncation(self):
        with patch.object(model,'MAX_REPORT',128):self.denied(seed())
    def test_batch_exact_purpose_names_expectations_and_cap(self):
        r=model.audit_samples(batch());self.assertEqual((r['sample_count'],r['known_histories']),(2,1))
        for mutate in (lambda v:v.update(purpose='old_prototype_samples'),lambda v:v['samples'].append(copy.deepcopy(v['samples'][0])),lambda v:v['samples'][0].update(expected_history_known=False),lambda v:v.update(extra=True)):
            v=batch();mutate(v)
            with self.assertRaises(ValueError):model.audit_samples(v)
        v=batch();v['samples']=[dict(name=str(i),expected_history_known=True,wire=seed()) for i in range(65)]
        with self.assertRaises(ValueError):model.audit_samples(v)
    def test_batch_output_bound_keeps_full_getter_audit_or_refuses(self):
        with patch.object(model,'MAX_BATCH_REPORT',128):
            with self.assertRaises(ValueError):model.audit_samples(batch())

def bridge(mode,path):
    exe=Path(path).resolve(strict=True)
    completed=subprocess.run([str(exe),'--emit-json'],capture_output=True,check=True,timeout=60)
    if completed.stderr:raise ValueError(completed.stderr.decode('utf8','replace')[:1024])
    if len(completed.stdout)>model.MAX_BATCH:raise ValueError('Complete new CPU batch input bound')
    result=model.audit_samples(completed.stdout)
    wanted=(7,4) if mode=='--native-wire' else (2,2)
    if (result['sample_count'],result['known_histories'])!=wanted:raise ValueError('Independent finite sample schedule')
    for item in result['samples']:
        r=item['audit']
        if r['prototype_only'] is not True or r['vendor_built'] is not False or r['runtime_executed'] is not False:raise ValueError('CPU prototype only')
        if any(r['claims'][key] is not False for key in CLAIMS):raise ValueError('No stronger fact promotion')
        if r['private_source_history_known'] and not r['getter_audit']['prototype_source_history_known']:raise ValueError('Complete frozen child result')
    encoded=json.dumps(result,separators=(',',':'),ensure_ascii=True,allow_nan=False).encode('ascii')
    if len(encoded)>model.MAX_BATCH_REPORT:raise ValueError('Complete audit report bound')
    print(json.dumps(dict(cpu_mock_only=True,samples=result['sample_count'],known_histories=result['known_histories'],
      getter_results_preserved=True,complete_report_bytes=len(encoded),vendor_runtime_GPU_facts=False),sort_keys=True))

if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1] in ('--native-wire','--integration-wire'):bridge(sys.argv[1],sys.argv[2])
    else:unittest.main()
