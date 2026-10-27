"""Independent CPU controls for the additive selected original-call relation.

The old test module supplies synthetic protocol inputs only. Expected decisions,
clock mutations and the reference strict-result comparison are defined here.
No journal or old analyzer is repaired, filtered or rewritten for admission.
"""
import copy
import json
import unittest
from unittest.mock import patch

from tools import closed_call_relation_validate as relation
from tools import original_call_owner_validate as strict
from test_original_call_owner_validate import BUILD, RUN, claim, pe_receipt, rows, stream, packet


def inputs(values=None, *, other_scope=None):
    journals = {0: stream(rows() if values is None else values)}
    claims = {0: claim()}
    if other_scope is not None:
        journals[1] = stream(other_scope)
        claims[1] = claim(1, 3333, 4444)
    return dict(journals=journals, claims=claims, PE_data=packet(pe_receipt()),
                run_id=RUN, build_id=BUILD, overflow=False)


def lifecycle(values, kind):
    return next(r for r in values if r.get('kind') == kind)


def foreign_open(values=None, *, raw_instance=51):
    """Add one completed creation AFTER the whole closed selected occurrence."""
    values = rows() if values is None else copy.deepcopy(values)
    extra = copy.deepcopy(lifecycle(values, 'create_instance'))
    extra.update(instance_generation=2, raw_instance=raw_instance,
                 operation_ordinal=values[-1]['transition_count'] + 1,
                 return_ordinal=values[-1]['transition_count'] + 2, output_pointer=9900)
    # Payload labels are arbitrary and never identify a cache/process/role.
    extra['application'] = dict(captured=True, api_version=4194304, name='arbitrary application',
                                 name_truncated=False, engine_name='arbitrary engine',
                                 engine_name_truncated=False)
    first_owner = next(i for i, row in enumerate(values) if row['event'] == 'original_call_owner')
    values.insert(first_owner, extra)
    values[-1].update(lifecycle_calls_seen=values[-1]['lifecycle_calls_seen'] + 1,
                      lifecycle_retained=values[-1]['lifecycle_retained'] + 1,
                      transition_count=values[-1]['transition_count'] + 2,
                      active_instances=values[-1]['active_instances'] + 1, coverage_complete=False)
    return values


def foreign_only_scope():
    original = rows(1, 3333, 4444)
    header, create, footer = copy.deepcopy(original[0]), copy.deepcopy(original[1]), copy.deepcopy(original[-1])
    create.update(raw_instance=999, output_pointer=8000)
    footer.update(lifecycle_calls_seen=1, lifecycle_retained=1, transition_count=2,
                  owner_calls_seen=0, owner_original_returns=0, owner_records_retained=0,
                  sampler_occurrences=0, name_records=0, calls_seen=0, original_returns=0,
                  events_retained=0, active_instances=1, coverage_complete=False)
    for name in ('calls_by_case', 'returns_by_case', 'events_by_case', 'dropped_by_case'):
        footer[name] = [0] * 9
    return [header, create, footer]


def remove_closed_teardown(values, kind):
    """Delete a real destroy pair and compact the common clock coherently."""
    values = copy.deepcopy(values)
    removed = lifecycle(values, kind)
    start = removed['operation_ordinal']
    values.remove(removed)
    for r in values[1:-1]:
        for name in ('operation_ordinal', 'return_ordinal'):
            if r[name] > start:
                r[name] -= 2
    f = values[-1]
    f['transition_count'] -= 2
    if kind == 'destroy_sampler':
        f['owner_calls_seen'] -= 1
        f['owner_original_returns'] -= 1
        f['owner_records_retained'] -= 1
        f['active_sampler_occurrences'] += 1
    else:
        f['lifecycle_calls_seen'] -= 1
        f['lifecycle_retained'] -= 1
        f['active_devices' if kind == 'destroy_device' else 'active_instances'] += 1
    f['coverage_complete'] = False
    return values


class ClosedCallRelationIndependentTests(unittest.TestCase):
    def setUp(self):
        # The analyzer is a pure saved-fact calculation; no new observation may
        # be introduced while deciding the local relation.
        for name in ('run', 'Popen', 'call', 'check_call', 'check_output'):
            blocker = patch('subprocess.' + name, side_effect=AssertionError('Child process forbidden'))
            blocker.start(); self.addCleanup(blocker.stop)
        for name in ('builtins.open', 'os.open', 'os.readlink', 'os.listdir', 'os.system', 'os.popen'):
            blocker = patch(name, side_effect=AssertionError('Runtime/file observation forbidden'))
            blocker.start(); self.addCleanup(blocker.stop)

    def evaluate(self, args, expected):
        before = copy.deepcopy(args)
        reference = strict.analyze(**args)
        report = relation.analyze_closed_call_relation(**args)
        self.assertEqual(args, before, 'No input retagging/mutation')
        self.assertEqual(report['legacy_strict_result'], reference)
        self.assertEqual(strict.analyze(**args), reference, 'Frozen strict decision unchanged')
        self.assertIs(report['closed_selected_original_call_relation_known'], expected, report)
        self.assertEqual(report['purpose'], 'closed_original_nonce_call_relation_audit')
        self.assertIs(type(report['schema_version']), int)
        self.assertEqual(report['schema_version'], 1)
        for key in strict.FALSE_FACTS:
            self.assertIs(report[key], False, key)
        return report

    def refuse(self, args):
        try:
            old = strict.analyze(**args)
        except (ValueError, KeyError, TypeError):
            with self.assertRaises((ValueError, KeyError, TypeError)):
                relation.analyze_closed_call_relation(**args)
        else:
            report = self.evaluate(args, False)
            self.assertEqual(report['legacy_strict_result'], old)

    def test_complete_original_target_is_positive_without_stronger_facts(self):
        report = self.evaluate(inputs(), True)
        self.assertTrue(report['legacy_strict_result']['original_call_owner_known'])
        self.assertIs(report['all_saved_scopes_lifecycle_closed'], True)

    def test_foreign_completed_open_instance_does_not_erase_closed_target(self):
        report = self.evaluate(inputs(foreign_open()), True)
        self.assertFalse(report['legacy_strict_result']['original_call_owner_known'])
        self.assertEqual(len(report['legacy_strict_result']['joins']), 2)
        self.assertIs(report['all_saved_scopes_lifecycle_closed'], False)
        self.assertFalse(report['legacy_strict_result']['all_saved_scopes_complete'])

    def test_foreign_saved_scope_is_retained_not_filtered(self):
        report = self.evaluate(inputs(other_scope=foreign_only_scope()), True)
        self.assertEqual(len(report['legacy_strict_result']['scopes']), 2)
        self.assertIs(report['all_saved_scopes_lifecycle_closed'], False)

    def test_labels_pid_order_and_phase_never_whitelist_foreign_instance(self):
        for label in ('', 'Wine', 'not Wine', 'Diablo IV', 'D4VR_CALL_OWNER_1', 'anything'):
            values = foreign_open()
            extra = [r for r in values if r.get('kind') == 'create_instance'][1]
            extra['application']['name'] = label
            extra['application']['engine_name'] = label[::-1]
            with self.subTest(label=label):
                self.evaluate(inputs(values), True)
        self.evaluate(inputs(other_scope=foreign_only_scope()), True)

    def test_same_raw_instance_after_completed_target_is_a_new_generation(self):
        report = self.evaluate(inputs(foreign_open(raw_instance=50)), True)
        self.assertFalse(report['all_saved_scopes_lifecycle_closed'])
        stale = foreign_open(raw_instance=50)
        extra = [r for r in stale if r.get('kind') == 'create_instance'][1]
        extra['instance_generation'] = 1
        self.refuse(inputs(stale))

    def test_reversed_unrelated_returns_are_replayed_by_original_clock(self):
        values = foreign_open()
        first = [r for r in values if r.get('kind') == 'create_instance'][1]
        first['return_ordinal'] = 26
        second = copy.deepcopy(first)
        second.update(instance_generation=3, raw_instance=52,
                      operation_ordinal=24, return_ordinal=25, output_pointer=9908)
        position = next(i for i, r in enumerate(values) if r['event'] == 'original_call_owner')
        values.insert(position, second)
        values[-1].update(lifecycle_calls_seen=7, lifecycle_retained=7, transition_count=26,
                          active_instances=2)
        report = self.evaluate(inputs(values), True)
        self.assertEqual({x['instance_generation'] for x in report['open_instances']}, {2, 3})

    def test_global_saved_lifecycle_closure_is_independent_of_selected_nonce_join(self):
        args = inputs()
        pe = pe_receipt()
        pe['events'][5]['output_hex'] = '00112233'
        args['PE_data'] = packet(pe)
        report = self.evaluate(args, False)
        self.assertTrue(report['legacy_strict_result']['all_saved_scopes_complete'])
        self.assertIs(report['all_saved_scopes_lifecycle_closed'], True)

    def test_future_foreign_calls_with_complete_returns_do_not_retag_target(self):
        values = foreign_open()
        extra = copy.deepcopy(lifecycle(values, 'destroy_instance'))
        extra.update(instance_generation=2, raw_instance=51,
                     operation_ordinal=25, return_ordinal=26)
        values.insert(next(i for i, r in enumerate(values) if r['event'] == 'original_call_owner'), extra)
        values[-1].update(lifecycle_calls_seen=7, lifecycle_retained=7, transition_count=26,
                          active_instances=0, coverage_complete=True)
        report = self.evaluate(inputs(values), True)
        self.assertIs(report['all_saved_scopes_lifecycle_closed'], True)
        self.assertEqual({j['Unix_parent_instance_generation'] for j in
                          report['legacy_strict_result']['joins']}, {1})

    def test_completed_sampler_reuse_requires_new_exact_generation(self):
        values = rows()
        first = copy.deepcopy(lifecycle(values, 'create_sampler'))
        retired = copy.deepcopy(lifecycle(values, 'destroy_sampler'))
        for row in values[1:-1]:
            if row['operation_ordinal'] >= 7:
                row['operation_ordinal'] += 4
                row['return_ordinal'] += 4
            if row['event'] in ('original_call_owner', 'get_descriptor'):
                row['sampler_generation'] = 2
        retired.update(operation_ordinal=9, return_ordinal=10, sampler_generation=1)
        position = next(i for i, r in enumerate(values) if r['event'] == 'original_call_owner')
        values[position:position] = [first, retired]
        values[-1].update(owner_calls_seen=6, owner_original_returns=6, owner_records_retained=6,
                          sampler_occurrences=2, transition_count=26)
        values = foreign_open(values)
        report = self.evaluate(inputs(values), True)
        self.assertEqual({j['Unix_sampler_generation'] for j in
                          report['legacy_strict_result']['joins']}, {2})
        lifecycle(values, 'object_name')['sampler_generation'] = 1
        self.refuse(inputs(values))

    def test_target_parent_open_refuses_even_when_sampler_and_device_are_closed(self):
        self.refuse(inputs(remove_closed_teardown(rows(), 'destroy_instance')))

    def test_target_device_or_sampler_open_refuses(self):
        for kind in ('destroy_device', 'destroy_sampler'):
            with self.subTest(kind=kind):
                self.refuse(inputs(remove_closed_teardown(rows(), kind)))

    def test_unrelated_active_device_is_not_admitted_by_local_relation(self):
        # Remove target references in a different scope; a live device there
        # remains outside this first deliberately limited relation contract.
        foreign = rows(1, 3333, 4444)
        foreign = foreign[:4] + [foreign[-1]]
        f = foreign[-1]
        f.update(lifecycle_calls_seen=3, lifecycle_retained=3, transition_count=6,
                 owner_calls_seen=0, owner_original_returns=0, owner_records_retained=0,
                 sampler_occurrences=0, name_records=0, calls_seen=0, original_returns=0,
                 events_retained=0, active_instances=1, active_devices=1,
                 coverage_complete=False)
        for name in ('calls_by_case', 'returns_by_case', 'events_by_case', 'dropped_by_case'):
            f[name] = [0] * 9
        self.refuse(inputs(other_scope=foreign))

    def test_foreign_pending_creation_or_destroy_is_not_a_completed_open_instance(self):
        for kind in ('create', 'destroy'):
            values = foreign_open()
            extra = [r for r in values if r.get('kind') == 'create_instance'][1]
            if kind == 'create':
                extra.update(original_returned=False, return_ordinal=0, result=None, raw_instance=0)
                extra['instance_debug_utils']['enabled_on_success'] = False
                values[-1]['transition_count'] -= 1
                values[-1]['active_instances'] = 0
            else:
                pending = copy.deepcopy(lifecycle(values, 'destroy_instance'))
                pending.update(instance_generation=2, raw_instance=51,
                               operation_ordinal=25, return_ordinal=0, original_returned=False)
                values.insert(next(i for i, r in enumerate(values) if r['event'] == 'original_call_owner'), pending)
                values[-1].update(lifecycle_calls_seen=7, lifecycle_retained=7, transition_count=25)
            with self.subTest(kind=kind):
                self.refuse(inputs(values))

    def test_pending_original_name_or_query_never_becomes_a_completed_relation(self):
        for event in ('original_call_owner', 'get_descriptor'):
            values = foreign_open()
            row = next(r for r in values if r['event'] == event and
                       (event != 'original_call_owner' or r['kind'] == 'object_name'))
            row.update(original_returned=False, return_ordinal=0,
                       phase_after=dict(status='missing', case_ordinal=0, api_phase=0), phase_unchanged=False)
            if event == 'original_call_owner':
                row['result'] = None
                values[-1]['owner_original_returns'] -= 1
                values[-1]['pending_owner_records'] += 1
            else:
                row.update(output_captured=False, output_hex='')
                values[-1]['original_returns'] -= 1
                values[-1]['returns_by_case'][8] -= 1
            values[-1]['transition_count'] -= 1
            self.refuse(inputs(values))

    def test_duplicate_nonce_across_two_otherwise_valid_scopes_is_ambiguous(self):
        self.refuse(inputs(other_scope=rows(1, 3333, 4444)))

    def test_original_return_order_not_serialization_order_controls_join(self):
        for mutation in ('marker_overlap_query', 'second_marker_before_first_query_return',
                         'device_destroy_overlap_sampler_destroy'):
            values = foreign_open()
            if mutation == 'marker_overlap_query':
                lifecycle(values, 'object_name')['return_ordinal'] = 12
            elif mutation == 'second_marker_before_first_query_return':
                names = [r for r in values if r.get('kind') == 'object_name']
                names[1].update(operation_ordinal=11, return_ordinal=12)
            else:
                lifecycle(values, 'destroy_device')['operation_ordinal'] = 18
            with self.subTest(mutation=mutation):
                self.refuse(inputs(values))

    def test_drops_unknown_phases_faults_and_route_gaps_are_not_waived(self):
        for key in ('owner_gap', 'faulted', 'transport_overflow', 'transport_chain_mismatch'):
            values = foreign_open(); values[-1][key] = True
            with self.subTest(key=key):
                self.refuse(inputs(values))
        for key in ('owner_records_dropped', 'lifecycle_dropped', 'dropped_events',
                    'owner_unknown_phase', 'unknown_phase_queries'):
            values = foreign_open(); values[-1][key] += 1
            with self.subTest(key=key):
                self.refuse(inputs(values))
        self.refuse(dict(inputs(foreign_open()), overflow=True))

    def test_shared_clock_holes_and_counters_cannot_hide_originals(self):
        for key in ('transition_count', 'calls_seen', 'original_returns',
                    'owner_calls_seen', 'owner_original_returns', 'lifecycle_calls_seen'):
            values = foreign_open(); values[-1][key] += 1
            with self.subTest(key=key):
                self.refuse(inputs(values))

    def test_all_claims_and_footer_are_mandatory(self):
        args = inputs(other_scope=foreign_only_scope())
        for mutation in ('missing_claim', 'missing_journal', 'missing_footer', 'missing_header'):
            bad = copy.deepcopy(args)
            if mutation == 'missing_claim':
                del bad['claims'][1]
            elif mutation == 'missing_journal':
                del bad['journals'][1]
            else:
                selected = foreign_only_scope()
                bad['journals'][1] = stream(selected[:-1] if mutation == 'missing_footer' else selected[1:])
            with self.subTest(mutation=mutation):
                self.refuse(bad)

    def test_unknown_build_wrong_claim_cookie_or_namespace_is_not_selected(self):
        for mutation in ('unknown_build', 'fork_cookie', 'wrong_slot', 'wrong_nonce'):
            values = foreign_open()
            if mutation == 'unknown_build':
                values[0].update(build_id_known=False, build_id='unknown')
            elif mutation == 'fork_cookie':
                lifecycle(values, 'object_name')['process_start_cookie'] += 1
            elif mutation == 'wrong_slot':
                values[0]['scope_slot'] = 1
            else:
                values[-1]['run_id'] = 'f' * 32
            with self.subTest(mutation=mutation):
                self.refuse(inputs(values))

    def test_scope2_off_is_never_relabelled_as_a_new_positive(self):
        values = foreign_open()
        for row in values:
            row['schema_version'] = 2
        self.refuse(inputs(values))
        values = foreign_open(); values[0]['enabled'] = False
        self.refuse(inputs(values))

    def test_stronger_true_input_facets_are_not_accepted(self):
        keys = ('resource_generation_known', 'selected_slot_placement_known',
                'current_contents_at_draw_known', 'sampled_resource_GPU_read_proven',
                'exclusive_phase_attribution_proven')
        for key in keys:
            values = foreign_open(); values[0][key] = True
            with self.subTest(key=key):
                self.refuse(inputs(values))

    def test_cross_domain_counts_and_handle_values_are_not_conversion_witnesses(self):
        values = foreign_open(); lifecycle(values, 'create_instance')['instance_debug_utils']['extension_count'] = 8
        args = inputs(values); pe = pe_receipt()
        pe['activation']['enabled_instance_extensions'].append('VK_KHR_surface')
        pe['activation']['instance_extension_count'] = pe['activation']['instance_extension_capacity'] = 2
        args['PE_data'] = packet(pe)
        report = self.evaluate(args, True)
        self.assertFalse(report['PE_to_Unix_handle_mapping_known'])
        self.assertNotEqual(report['legacy_strict_result']['joins'][0]['PE_device'],
                            report['legacy_strict_result']['joins'][0]['Unix_device'])

    def test_descriptor_bytes_may_differ_per_exact_original_query(self):
        values = foreign_open(); pe = pe_receipt()
        q2 = [r for r in values if r['event'] == 'get_descriptor'][1]
        q2['output_hex'] = pe['events'][9]['output_hex'] = '01234567'
        self.evaluate(dict(inputs(values), PE_data=packet(pe)), True)
        q2['output_hex'] = '41424344'
        self.refuse(dict(inputs(values), PE_data=packet(pe)))

    def test_boolean_integer_and_extra_key_spoofs_refuse(self):
        for key in ('transition_count', 'active_instances', 'owner_original_returns'):
            values = foreign_open(); values[-1][key] = True
            self.refuse(inputs(values))
        values = foreign_open(); values[-1]['foreign_instance_is_safe'] = True
        self.refuse(inputs(values))


if __name__ == '__main__':
    unittest.main()
