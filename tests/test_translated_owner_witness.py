import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import translated_owner_witness as model
import test_query_scope_validate as owned


def facts(slot=0):
    rows = owned.rows(slot)
    queries = [r for r in rows if r['event'] == 'get_descriptor']
    for sequence, q in enumerate(queries, 1):
        q.update(device=2001, device_generation=1, output_pointer=6000 + 16 * sequence)
        q['phase_before'] = dict(status='known', case_ordinal=8, api_phase=sequence)
        q['phase_after'] = q['phase_before'].copy()
    for field in ('calls_by_case', 'returns_by_case', 'events_by_case'):
        rows[-1][field] = [0] * 8 + [2]
    claim = dict(scope_slot=slot, host_pid=100, process_start_cookie=200, dso_epoch=slot + 1)
    reports = []
    for sequence in (1, 2):
        reports.append(dict(schema_version=1, purpose='owned_claimed_process_correlation',
                            run_id=owned.RUN, gate_sequence=sequence,
                            process_correlation_known=True,
                            selected_process=dict(host_pid=100, process_start_cookie=200),
                            claim_identities=[claim.copy()], input_facts_attested=False,
                            Windows_PID_used_for_host_identity=False,
                            Unix_device_owner_known=False,
                            PE_to_Unix_handle_mapping_known=False,
                            global_query_absence_proven=False))
        reports[-1].update({key: False for key in model.gates.FALSE_FACTS})
        reports[-1].update(cmdline_and_backing_maps_are_loaded_memory_attestation=False,
            reasons=[], inventoried_processes=[dict(host_pid=100,
                process_start_cookie=200, candidate=True, reasons=[],
                module_roles=list(model.gates.ROLES), cmdline_sha256='a' * 64,
                maps_sha256='b' * 64, map_lines=5)])
    witness = dict(schema_version=1, purpose='owned_PE_original_query_witnesses',
                   run_id=owned.RUN, windows_pid=999, pe_instance=91000,
                   pe_physical=93000, pe_device=92001, queries=[])
    for sequence, q in enumerate(queries, 1):
        witness['queries'].append(dict(sequence=sequence,
            phase=dict(case_ordinal=8, api_phase=sequence), return_kind='void',
            original_returned=True, args=dict(device=92001,
                sampler=q['typed_input']['sampler'], data_size=q['data_size'],
                output_pointer=q['output_pointer'], descriptor_type=0,
                info_sType=1000316004, info_pnext=None), output_hex=q['output_hex'],
            scratch_span_retained_until_after_queries=True))
    return rows, reports, witness


def original_inputs(witness):
    outputs = dict(instance=witness['pe_instance'], physical_device=witness['pe_physical'],
                   device=witness['pe_device'])
    interop = dict(requested=True, query_interface_hresult=0,
                   get_vulkan_handles_hresult=0, outputs=outputs)
    ready = [dict(protocol_version=1, run_id=owned.RUN, gate_sequence=sequence,
                  status='ready', windows_pid=witness['windows_pid'], device_interface=8888,
                  setup_fences_consumed=True, readback_fences_consumed=sequence == 2,
                  modules=[dict(role=role, path='Z:\\probe\\' + role) for role in
                           model.gates.READY_ROLES], interop=copy.deepcopy(interop))
             for sequence in (1, 2)]
    activation = dict(schema_version=1, run_id=owned.RUN,
        IID='39da4e09-bd1c-4198-9fae-86bbe3be41fd',
        handle_domain='PE_facing_vkd3d_contract', PE_to_Unix_handle_mapping_known=False,
        query_interface_hresult=0, get_vulkan_handles_hresult=0, outputs=outputs,
        extensions_count_hresult=0, extensions_values_hresult=0, device_features_hresult=0,
        enabled_extensions=['VK_EXT_descriptor_buffer'],
        enabled_feature_chain_types=[1000316002], descriptorBuffer=True,
        sampler_descriptor_size=4, create_sampler_result=0,
        sampler_PE=witness['queries'][0]['args']['sampler'], activated=True)
    events = dict(schema_version=1, run_id=owned.RUN, events=[])
    # One sampler is held across both calls. Returned opaque bytes may differ.
    for q in witness['queries']:
        q['args']['sampler'] = activation['sampler_PE']
        for edge in ('before', 'after'):
            row = dict(sequence=q['sequence'], api='vkGetDescriptorEXT', run_id=owned.RUN,
                       phase=q['phase'].copy(), args=q['args'].copy(), return_kind='void',
                       edge=edge, original_returned=edge == 'after', result=None)
            if edge == 'after':
                row['output_hex'] = q['output_hex']
            events['events'].append(row)
    return ready, activation, events


def packet(value):
    return (json.dumps(value) + '\n').encode()


class TranslatedOwnerWitness(unittest.TestCase):
    def setUp(self):
        self.rows, self.reports, self.witness = facts()
        for name in ('Popen', 'run', 'call', 'check_call', 'check_output'):
            mock = patch.object(subprocess, name, side_effect=AssertionError('CPU model only'))
            mock.start()
            self.addCleanup(mock.stop)

    def analyze(self, journals=None, claims=None):
        return model.analyze(journals or {0: owned.journal(self.rows)},
                             claims or {0: owned.claim()}, owned.RUN, owned.BUILD,
                             self.reports, self.witness)

    def reject(self):
        with self.assertRaises((ValueError, TypeError, KeyError)):
            self.analyze()

    def test_positive_original_calls_cross_device_domains_without_numeric_equality(self):
        result = self.analyze()
        self.assertTrue(result['original_PE_query_device_witnessed'])
        self.assertTrue(result['finite_saved_scope_and_owner_complete'])
        self.assertEqual(result['joins'][0]['PE_device'], 92001)
        self.assertEqual(result['joins'][0]['Unix_device'], 2001)
        self.assertFalse(result['PE_device_equality_to_Unix_device_used'])
        self.assertFalse(result['windows_PID_compared_to_host_PID'])

    def test_equal_handles_do_not_change_proof_kind(self):
        self.witness['pe_device'] = 2001
        for q in self.witness['queries']:
            q['args']['device'] = 2001
        result = self.analyze()
        self.assertTrue(result['original_PE_query_device_witnessed'])
        self.assertFalse(result['general_PE_to_Unix_handle_conversion_proven'])

    def test_no_resource_current_content_or_absence_claim_even_positive(self):
        result = self.analyze()
        for key in model.FALSE_FACETS:
            self.assertIs(result[key], False, key)
        self.assertFalse(result['loader_thunk_source_contract_is_loaded_binary_attestation'])

    def test_identical_opaque_bytes_are_not_used_as_unique_identity(self):
        self.witness['queries'][1]['output_hex'] = '01020304'
        self.rows[-2]['output_hex'] = '01020304'
        self.assertTrue(self.analyze()['original_PE_query_device_witnessed'])
        self.witness['queries'][1]['args']['output_pointer'] += 128
        self.assertFalse(self.analyze()['original_PE_query_device_witnessed'])

    def test_each_original_typed_argument_or_payload_is_discriminant(self):
        for field, value in [('sampler', 555555), ('data_size', 3),
                             ('output_pointer', 777777)]:
            before = copy.deepcopy(self.witness)
            self.witness['queries'][0]['args'][field] = value
            if field == 'data_size':
                self.witness['queries'][0]['output_hex'] = '010203'
            self.assertFalse(self.analyze()['original_PE_query_device_witnessed'], field)
            self.witness = before
        self.witness['queries'][0]['output_hex'] = '00000000'
        self.assertFalse(self.analyze()['original_PE_query_device_witnessed'])

    def test_changed_phase_refuses_witness_without_changing_original_scope(self):
        self.witness['queries'][0]['phase']['api_phase'] = 3
        self.reject()

    def test_original_void_return_cannot_be_a_success_hresult(self):
        self.witness['queries'][0]['return_kind'] = 'HRESULT'
        self.reject()
        self.witness['queries'][0]['return_kind'] = 'void'
        self.witness['queries'][0]['hresult'] = 0
        self.reject()

    def test_pending_original_is_not_a_returned_query(self):
        self.witness['queries'][0]['original_returned'] = False
        self.reject()

    def test_overlapping_or_recycled_scratch_is_refused(self):
        for delta in (0, 1, 3):
            self.witness['queries'][1]['args']['output_pointer'] = self.witness['queries'][0]['args']['output_pointer'] + delta
            self.reject()

    def test_scratch_lifetime_and_pointer_overflow_refused(self):
        self.witness['queries'][0]['scratch_span_retained_until_after_queries'] = False
        self.reject()
        self.witness['queries'][0]['scratch_span_retained_until_after_queries'] = True
        self.witness['queries'][0]['args']['output_pointer'] = (1 << 64) - 2
        self.reject()

    def test_ambiguous_namespace_query_never_selects_first_or_last(self):
        other, _, _ = facts(1)
        for report in self.reports:
            report['claim_identities'].append(dict(scope_slot=1, host_pid=100,
                                                   process_start_cookie=200, dso_epoch=2))
        result = self.analyze({0: owned.journal(self.rows), 1: owned.journal(other)},
                              {0: owned.claim(), 1: owned.claim(1)})
        self.assertFalse(result['original_PE_query_device_witnessed'])
        self.assertTrue(all(j['reason'] == 'original_witness_ambiguous' for j in result['unjoined']))

    def test_identical_query_in_other_host_process_is_not_this_owner(self):
        for report in self.reports:
            report['selected_process']['host_pid'] = 101
            other = copy.deepcopy(report['inventoried_processes'][0])
            report['inventoried_processes'][0].update(candidate=False, module_roles=[],
                reasons=['exact_PE_argument_and_nonce_not_unique'] +
                    ['required_module_mapping_missing_deleted_or_mismatched:' + role
                     for role in model.gates.ROLES])
            other.update(host_pid=101, candidate=True, reasons=[])
            report['inventoried_processes'].append(other)
            report['claim_identities'].append(dict(scope_slot=1, host_pid=101,
                                                   process_start_cookie=200, dso_epoch=2))
        # The extra claimed process may have no journal, so finite closure also fails.
        result = self.analyze(claims={0: owned.claim(), 1: owned.claim(1, 101, 200)})
        self.assertFalse(result['original_PE_query_device_witnessed'])
        self.assertFalse(result['finite_saved_scope_and_owner_complete'])

    def test_bracketing_queries_must_join_same_live_unix_device(self):
        self.rows[-2]['device'] = 2002
        self.rows[-2]['device_generation'] = 2
        self.assertFalse(self.analyze()['original_PE_query_device_witnessed'])

    def test_missing_journal_cannot_be_reconstructed_from_current_status(self):
        result = model.analyze({}, {0: owned.claim()}, owned.RUN, owned.BUILD,
                               self.reports, self.witness)
        self.assertFalse(result['original_PE_query_device_witnessed'])

    def test_same_windows_pid_as_host_is_never_an_extra_witness(self):
        self.witness['windows_pid'] = 100
        self.witness['queries'][0]['args']['output_pointer'] += 128
        result = self.analyze()
        self.assertFalse(result['original_PE_query_device_witnessed'])
        self.assertFalse(result['windows_PID_compared_to_host_PID'])

    def test_wrong_host_cookie_between_gates_refused(self):
        self.reports[1]['selected_process']['process_start_cookie'] += 1
        self.reject()

    def test_boolean_schema_and_contradictory_inventory_refused(self):
        self.reports[0]['schema_version'] = True
        self.reject()
        self.reports[0]['schema_version'] = 1
        self.reports[0]['inventoried_processes'][0]['candidate'] = False
        self.reject()

    def test_missing_correlation_or_device_claim_in_process_report_refused(self):
        self.reports[0]['process_correlation_known'] = False
        self.reject()
        self.reports[0]['process_correlation_known'] = True
        self.reports[0]['Unix_device_owner_known'] = True
        self.reject()

    def test_nonce_and_gate_sequence_are_exact(self):
        for field, value in [('run_id', '9' * 32), ('gate_sequence', 2)]:
            previous = copy.deepcopy(self.reports)
            self.reports[0][field] = value
            self.reject()
            self.reports = previous
        self.witness['run_id'] = '9' * 32
        self.reject()

    def test_new_final_claim_must_be_inventoried_before_gate_two(self):
        with self.assertRaises(ValueError):
            self.analyze(claims={0: owned.claim(), 1: owned.claim(1, 101, 201)})

    def test_persistent_claim_change_between_gates_refused(self):
        self.reports[0]['claim_identities'][0]['host_pid'] += 1
        self.reject()

    def test_finite_limits_and_boolean_integers_refused(self):
        self.witness['queries'][0]['args']['data_size'] = True
        self.reject()
        self.witness['queries'][0]['args']['data_size'] = 257
        self.reject()

    def test_uninterpreted_extension_chains_and_unknown_fields_refused(self):
        self.witness['queries'][0]['args']['info_pnext'] = 123
        self.reject()
        self.witness['queries'][0]['args']['info_pnext'] = None
        self.witness['resource_generation_known'] = True
        self.reject()

    def test_original_ready_activation_and_void_edges_join_before_unix_audit(self):
        ready, activation, events = original_inputs(self.witness)
        result = model.audit_owned_inputs(self.witness, list(map(packet, ready)),
                                         packet(activation), packet(events))
        self.assertEqual(result['original_PE_query_edges_verified'], 4)
        self.assertTrue(result['ready_and_activation_outputs_joined'])
        self.assertFalse(result['PE_to_Unix_handle_mapping_known'])

    def test_changed_ready_PE_device_is_refused_even_with_positive_query_witnesses(self):
        ready, activation, events = original_inputs(self.witness)
        ready[1]['interop']['outputs']['device'] += 1
        with self.assertRaises(ValueError):
            model.audit_owned_inputs(self.witness, list(map(packet, ready)),
                                     packet(activation), packet(events))

    def test_real_activation_failure_or_wrong_sampler_property_refused(self):
        for key, value in [('device_features_hresult', -1), ('activated', False),
                           ('descriptorBuffer', False), ('sampler_descriptor_size', 8),
                           ('sampler_PE', 7777), ('create_sampler_result', -2)]:
            ready, activation, events = original_inputs(self.witness)
            activation[key] = value
            with self.assertRaises(ValueError, msg=key):
                model.audit_owned_inputs(self.witness, list(map(packet, ready)),
                                         packet(activation), packet(events))

    def test_unknown_or_duplicate_enabled_feature_and_extension_refused(self):
        for key, value in [('enabled_feature_chain_types', [12]),
                           ('enabled_feature_chain_types', [1000316002] * 2),
                           ('enabled_extensions', ['VK_EXT_debug_utils']),
                           ('enabled_extensions', ['VK_EXT_descriptor_buffer'] * 2)]:
            ready, activation, events = original_inputs(self.witness)
            activation[key] = value
            with self.assertRaises(ValueError):
                model.audit_owned_inputs(self.witness, list(map(packet, ready)),
                                         packet(activation), packet(events))

    def test_original_before_after_argument_mutation_and_result_invention_refused(self):
        for mutate in (lambda e: e['events'][1]['args'].update(device=55),
                       lambda e: e['events'][0].update(original_returned=True),
                       lambda e: e['events'][1].update(result=0),
                       lambda e: e['events'][0]['args'].update(descriptor_type=False),
                       lambda e: e['events'][0]['phase'].update(api_phase=True),
                       lambda e: e['events'][1].update(output_hex='00000000'),
                       lambda e: e['events'].pop()):
            ready, activation, events = original_inputs(self.witness)
            mutate(events)
            with self.assertRaises(ValueError):
                model.audit_owned_inputs(self.witness, list(map(packet, ready)),
                                         packet(activation), packet(events))


if __name__ == '__main__':
    unittest.main()
