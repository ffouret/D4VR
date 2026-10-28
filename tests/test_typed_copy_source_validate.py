"""Independent causal inputs for the NEW source prototype envelope.

Default discovery is pure CPU Python and starts no child. Explicit bridge CLIs
run only a caller-selected new CPU mock executable, never a driver or Wine.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import typed_copy_source_validate as audit

RUN = 'abcdef0123456789abcdef0123456789'


def ref(raw, generation):
    return dict(raw=raw, generation=generation)


def slot(heap, index, address):
    return dict(heap=ref(*heap), index=index, cpu_handle=address)


def snapshot():
    """Closed small world, authored here; no model output supplies the oracle."""
    dev = ref(71, 1)
    def occurrence(raw, generation, entry):
        return dict(raw=raw, generation=generation, create_entry=entry,
                    create_return=entry + 1, create_hresult=0,
                    release_entry=0, release_return=0)
    a, b = slot((81, 1), 0, 901), slot((81, 1), 1, 933)
    visible = slot((82, 2), 0, 1601)
    desc = dict(format=28, view_dimension=4, component_mapping=5768,
                most_detailed_mip=0, mip_levels=1, plane_slice=0, min_lod_clamp_bits=0)
    events = []
    for ident, resource, destination in ((1, ref(101, 1), a), (2, ref(102, 2), b)):
        events.append(dict(call_id=ident, entry=17 + 2 * ident, return_=18 + 2 * ident,
                           api='CreateShaderResourceView', return_kind='void', original_returned=True,
                           result=None, phase_before=dict(case_ordinal=1, api_phase=1),
                           phase_after=dict(case_ordinal=1, api_phase=1),
                           before=dict(device=copy.deepcopy(dev), resource=resource,
                                       destination=destination, description=copy.deepcopy(desc))))
    events.append(dict(call_id=3, entry=23, return_=24, api='CopyDescriptorsSimple',
                       return_kind='void', original_returned=True, result=None,
                       phase_before=dict(case_ordinal=1, api_phase=2),
                       phase_after=dict(case_ordinal=1, api_phase=2),
                       before=dict(device=copy.deepcopy(dev), count=1, heap_type=0,
                                   source=copy.deepcopy(a), destination=copy.deepcopy(visible))))
    for item in events:
        item['return'] = item.pop('return_')
    resource_desc = dict(dimension=3, width=8, height=8, depth_or_array_size=1,
                         mip_levels=1, format=28, sample_count=1, flags=0)
    return dict(schema_version=1, purpose='owned_typed_D3D12_descriptor_history',
                run_id=RUN, domain='owned_two_texture_three_SRV_slots',
                at_transition=24, complete=True, gap=False,
                device=occurrence(71, 1, 1),
                resources=[dict(**occurrence(101, 1, 3), device=copy.deepcopy(dev), description=copy.deepcopy(resource_desc)),
                           dict(**occurrence(102, 2, 5), device=copy.deepcopy(dev), description=copy.deepcopy(resource_desc))],
                heaps=[dict(**occurrence(81, 1, 7), device=copy.deepcopy(dev), role='cpu', heap_type=0,
                            count=2, flags=0, node_mask=0, cpu_start=901, increment=32),
                       dict(**occurrence(82, 2, 9), device=copy.deepcopy(dev), role='visible', heap_type=0,
                            count=1, flags=1, node_mask=0, cpu_start=1601, increment=32)],
                events=events, selected_destination=visible)


def wire():
    s = snapshot()
    inner = dict(schema_version=1, purpose='owned_original_typed_copy_transport',
                 sequence=1, run_id=RUN,
                 producer=dict(enabled=True, faulted=False, gap=False, dropped=0, clock=24,
                               epoch=7, nonfinal_release_returns=0, snapshot_available=True), snapshot=s,
                 **{key: False for key in audit.FALSE_FACTS})
    return dict(schema_version=1, purpose=audit.PURPOSE, prototype_only=True,
                vendor_built=False, runtime_executed=False, run_id=RUN, sequence=1,
                source=dict(device_origin='internal_new_initialization', public_outer_result_known=False,
                            factory_calls=0, factory_returns=0, existing_singleton_calls=0,
                            source_calls=11, source_returns=11, ignored_calls=0, pending=0, case_ordinal=1,
                            mutations=[dict(call_id=x['call_id'], variant=0, entry=x['entry'], **{'return': x['return']}) for x in s['events']],
                            factory_records=[]), native_wire=inner, public_device_create_result_known=False,
                **{key: False for key in audit.FALSE_FACTS})


def integration():
    cases = []
    for name in ('source_wrapped_srv_A', 'source_wrapped_simple_B', 'source_wrapped_ranges_NULL_A'):
        w = wire()
        if name.endswith('simple_B'):
            w['native_wire']['snapshot']['events'][-1]['before']['source'] = slot((81, 1), 1, 933)
        if name.endswith('ranges_NULL_A'):
            event = w['native_wire']['snapshot']['events'][-1]
            event['api'] = 'CopyDescriptors'
            event['phase_before']['api_phase'] = event['phase_after']['api_phase'] = 3
            event['before'] = dict(device=ref(71, 1), heap_type=0, source_range_count=1,
                                   source_starts=[slot((81, 1), 0, 901)], source_sizes=None,
                                   destination_range_count=1, destination_starts=[slot((82, 2), 0, 1601)],
                                   destination_sizes=None)
        cases.append(dict(name=name, expected_known=True,
                          expected_origin=ref(102, 2) if name.endswith('simple_B') else ref(101, 1), wire=w))
    return dict(purpose='real_C_ABI_source_wrapper_CPU_integration', cases=cases)


class SourceAuditTests(unittest.TestCase):
    def setUp(self):
        self.child = patch.object(subprocess, 'Popen', side_effect=AssertionError('No child during pure audit'))
        self.child.start()
        self.addCleanup(self.child.stop)

    def denied(self, value):
        try:
            result = audit.audit_source_wire(value)
        except ValueError:
            return
        self.assertFalse(result['prototype_source_history_known'])

    def test_positive_is_internal_CPU_replay_only(self):
        r = audit.audit_source_wire(wire())
        self.assertTrue(r['prototype_source_history_known'])
        self.assertEqual(r['native_model_replay']['history']['selected_version']['origin']['resource'], ref(101, 1))
        self.assertFalse(r['source_runtime_relation_known'])
        self.assertFalse(r['public_device_create_result_known'])
        self.assertFalse(r['installed_binary_source_equivalence_proven'])
        for name in audit.FALSE_FACTS:
            self.assertIs(r[name], False)

    def test_separate_purpose_no_old_wire_promotion(self):
        old = wire()['native_wire']
        old_audit = audit.native.audit_wire(old)
        self.assertTrue(old_audit['selected_history_known'])
        with self.assertRaises(ValueError):
            audit.audit_source_wire(old)
        self.assertEqual(audit.native.audit_wire(old), old_audit)

    def test_vendor_runtime_public_claims_forbidden(self):
        for key in ('prototype_only', 'vendor_built', 'runtime_executed', 'public_device_create_result_known'):
            w = wire(); w[key] = not w[key]
            self.denied(w)
        w = wire(); w['source']['public_outer_result_known'] = True
        self.denied(w)

    def test_all_stronger_facts_stay_false(self):
        for key in audit.FALSE_FACTS:
            for obj in ('root', 'native'):
                w = wire(); (w if obj == 'root' else w['native_wire'])[key] = True
                self.denied(w)

    def test_nonce_and_sequence_cross_domain(self):
        for key, value in (('run_id', 'a' * 32), ('sequence', 2), ('sequence', True)):
            w = wire(); w[key] = value; self.denied(w)

    def test_factory_success_not_new_device(self):
        w = wire(); w['source']['device_origin'] = 'unknown'
        self.denied(w)

    def test_outer_factory_diagnostic_has_no_device_authority(self):
        w = wire(); source = w['source']
        source.update(factory_calls=1, factory_returns=1, source_calls=12, source_returns=12)
        source['factory_records'] = [dict(entry=11, **{'return': 12}, hresult=0, raw_internal_output=999,
                                         outcome='outer_factory_only_unknown')]
        r = audit.audit_source_wire(w)
        self.assertTrue(r['prototype_source_history_known'])
        self.assertFalse(r['public_device_create_result_known'])
        self.assertEqual(r['source']['factory_records'][0]['raw_internal_output'], 999)

    def test_failed_factory_output_must_not_be_used(self):
        w = wire(); source = w['source']
        source.update(factory_calls=1, factory_returns=1, source_calls=12, source_returns=12)
        source['factory_records'] = [dict(entry=11, **{'return': 12}, hresult=-1, raw_internal_output=999,
                                         outcome='outer_factory_only_unknown')]
        self.denied(w)
        source['factory_records'][0]['raw_internal_output'] = 0
        self.assertTrue(audit.audit_source_wire(w)['prototype_source_history_known'])

    def test_factory_pending_no_promotion(self):
        w = wire(); w['source'].update(factory_calls=1, source_calls=12, pending=1)
        w['source']['factory_records'] = [dict(entry=11, **{'return': 0}, hresult=None,
                                              raw_internal_output=0, outcome='outer_factory_only_unknown')]
        self.denied(w)

    def test_factory_counter_cannot_retag_pending_as_returned(self):
        w = wire(); source = w['source']
        source.update(factory_calls=1, factory_returns=1, source_calls=12, source_returns=12)
        source['factory_records'] = [dict(entry=11, **{'return': 0}, hresult=None, raw_internal_output=0,
                                         outcome='outer_factory_only_unknown')]
        self.denied(w)

    def test_factory_clock_cannot_share_a_native_creation_or_release(self):
        for at in (1, 2, 3, 4, 7, 8, 19, 20):
            w = wire(); source = w['source']
            source.update(factory_calls=1, factory_returns=1, source_calls=12, source_returns=12)
            source['factory_records'] = [dict(entry=at, **{'return': at + 1}, hresult=0,
                                             raw_internal_output=999, outcome='outer_factory_only_unknown')]
            self.denied(w)

    def test_retained_pending_row_cannot_disappear_from_pending_counter(self):
        w = wire(); w['native_wire']['producer'].update(faulted=True, gap=True, snapshot_available=False)
        w['native_wire']['snapshot'] = None
        w['source']['mutations'][0]['return'] = 0
        self.denied(w)

    def test_source_case_cannot_retag_old_window(self):
        w = wire(); w['source']['case_ordinal'] = 8; self.denied(w)

    def test_mutations_exact_clock_identity_not_latest_lookup(self):
        for key, value in (('call_id', 99), ('entry', 15), ('return', 16)):
            w = wire(); w['source']['mutations'][0][key] = value; self.denied(w)

    def test_source_missing_or_reordered_mutation(self):
        w = wire(); w['source']['mutations'].pop(1); self.denied(w)
        w = wire(); w['source']['mutations'].reverse(); self.denied(w)

    def test_variant_api_shapes_exact(self):
        w = wire(); w['source']['mutations'][0]['variant'] = 3; self.denied(w)
        w = wire(); w['source']['mutations'][-1]['variant'] = 1; self.denied(w)
        for choice in (0, 3, 4, 5, 6, 7, 8, 9):
            w = wire(); w['source']['mutations'][-1]['variant'] = choice
            self.assertTrue(audit.audit_source_wire(w)['prototype_source_history_known'])

    def test_source_phase_is_not_future(self):
        w = wire()
        for key in ('phase_before', 'phase_after'):
            w['native_wire']['snapshot']['events'][0][key]['case_ordinal'] = 2
        self.denied(w)
        w = wire()
        for key in ('phase_before', 'phase_after'):
            w['native_wire']['snapshot']['events'][0][key]['api_phase'] = 2
        self.denied(w)

    def test_counter_bool_and_unsigned_overflow(self):
        for key in ('source_calls', 'pending', 'ignored_calls', 'case_ordinal'):
            for value in (True, -1, 1 << 64, 2.0):
                w = wire(); w['source'][key] = value; self.denied(w)

    def test_source_accounting_pending_and_missing_return(self):
        for key, value in (('pending', 1), ('source_calls', 12), ('source_returns', 12), ('factory_returns', 1)):
            w = wire(); w['source'][key] = value; self.denied(w)

    def test_healthy_call_counts_cannot_exceed_common_clock(self):
        w = wire(); w['source'].update(source_calls=100000, source_returns=100000, ignored_calls=99990)
        self.denied(w)

    def test_call_counts_cannot_omit_retained_native_creations(self):
        w = wire(); w['source'].update(source_calls=3, source_returns=3)
        self.denied(w)

    def test_distinct_call_families_cannot_overcount(self):
        w = wire(); w['source']['ignored_calls'] = 11; self.denied(w)
        w = wire(); w['source']['existing_singleton_calls'] = 11; self.denied(w)

    def test_duplicate_clock_after_and_pending_result(self):
        w = wire(); w['source']['mutations'][1]['return'] = w['source']['mutations'][0]['entry']; self.denied(w)
        w = wire(); w['native_wire']['snapshot']['events'][0]['result'] = 0; self.denied(w)
        w = wire(); w['native_wire']['snapshot']['events'][0]['return_kind'] = 'HRESULT'; self.denied(w)

    def test_selected_source_and_generation_must_exist(self):
        for key, value in (('raw', 999), ('generation', 99), ('generation', True)):
            w = wire(); w['native_wire']['snapshot']['events'][-1]['before']['source']['heap'][key] = value; self.denied(w)

    def test_NULL_range_sizes_preserved(self):
        data = integration()
        result = audit.audit_integration_samples(data)
        self.assertEqual(result['emitted_samples'], 3)
        original = data['cases'][-1]['wire']
        self.assertTrue(audit.audit_source_wire(original)['prototype_source_history_known'])
        original['native_wire']['snapshot']['events'][-1]['before']['source_sizes'] = [2]
        self.denied(original)

    def test_integration_distinct_purpose_case_and_expected_origin(self):
        for mutation in ('purpose', 'oldroot', 'origin', 'case'):
            data = integration()
            if mutation == 'purpose': data['purpose'] = 'typed_copy_transport_cpu_passed'
            if mutation == 'oldroot': data['samples'] = data.pop('cases')
            if mutation == 'origin': data['cases'][0]['expected_origin'] = ref(102, 2)
            if mutation == 'case': data['cases'][1]['name'] = 'source_wrapped_srv_A'
            with self.assertRaises(ValueError): audit.audit_integration_samples(data)

    def test_model_source_at_entry_not_later_source(self):
        w = wire(); s = w['native_wire']['snapshot']
        later = copy.deepcopy(s['events'][0]); later.update(call_id=4, entry=25, **{'return': 26})
        later['before']['resource'] = ref(102, 2); s['events'].append(later)
        w['source']['mutations'].append(dict(call_id=4, variant=0, entry=25, **{'return': 26}))
        w['source']['source_calls'] += 1; w['source']['source_returns'] += 1
        s['at_transition'] = w['native_wire']['producer']['clock'] = 26
        r = audit.audit_source_wire(w)
        self.assertEqual(r['native_model_replay']['history']['selected_version']['origin']['resource'], ref(101, 1))

    def test_report_detached_from_input_later_mutation(self):
        w = wire(); r = audit.audit_source_wire(w); frozen = copy.deepcopy(r)
        w['source']['mutations'][0]['variant'] = 9
        w['native_wire']['snapshot']['heaps'][0]['cpu_start'] = 0
        self.assertEqual(r, frozen)

    def test_observational_twins_cannot_claim_current_or_GPU_read(self):
        # Hidden descriptor-byte writes have no input field in this protocol.
        a, overwritten = wire(), copy.deepcopy(wire())
        self.assertEqual(audit.audit_source_wire(a), audit.audit_source_wire(overwritten))
        r = audit.audit_source_wire(a)
        for key in ('current_contents_at_draw_known', 'unchanged_since_copy_proven', 'sampled_resource_GPU_read_proven'):
            self.assertIs(r[key], False)

    def test_strict_UTF8_duplicates_and_full_outer_budget(self):
        text = json.dumps(wire())
        for data in (text.encode('utf16'), text.replace('"schema_version": 1', '"schema_version": 1,"schema_version": 1', 1),
                     ' ' * 16385, {'x': 'z' * 20000}):
            with self.assertRaises(ValueError): audit.audit_source_wire(data)

    def test_source_unknown_extra_fields_not_attested(self):
        w = wire(); w['source']['live_descriptor_bytes'] = '00'; self.denied(w)
        w = wire(); w['native_wire']['snapshot']['events'][0]['before']['app_role'] = 'A'; self.denied(w)

    def test_disabled_source_cannot_keep_enabled_history(self):
        w = wire(); w['native_wire']['producer'].update(enabled=False, snapshot_available=False)
        w['native_wire']['snapshot'] = None
        with self.assertRaises(ValueError): audit.audit_source_wire(w)

    def test_source_metadata_budgets(self):
        for key, count in (('mutations', 9), ('factory_records', 5)):
            w = wire()
            row = w['source']['mutations'][0] if key == 'mutations' else dict(entry=11, **{'return': 12}, hresult=0, raw_internal_output=0, outcome='outer_factory_only_unknown')
            w['source'][key] = [copy.deepcopy(row) for _ in range(count)]
            self.denied(w)


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
