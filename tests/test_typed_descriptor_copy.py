"""Independent causal inputs for the finite typed D3D12 history model.

These are synthetic original API arguments, never opaque descriptor contents or
an attestation of a real application. Expected resource A/B identities and
transition orders below are written independently of the model's replay.
"""
import copy
import gc
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import typed_descriptor_copy as d


NONCE = '438471ad23b94c929c9199a840435067'
U64 = (1 << 64) - 1
FALSE_FACTS = (
    'input_facts_attested', 'opaque_descriptor_decoded',
    'actual_source_contents_at_copy_proven', 'current_contents_at_draw_known',
    'unchanged_since_copy_proven', 'sampled_resource_GPU_read_proven',
    'generic_PE_to_Unix_resource_mapping', 'renderer_owner_known',
    'exclusive_phase_attribution_proven', 'global_API_absence_proven',
    'old_process_or_backing_gate_repaired',
)


def ref(raw, generation):
    return dict(raw=raw, generation=generation)


def srv_description():
    return dict(format=28, view_dimension=4, component_mapping=5768,
                most_detailed_mip=0, mip_levels=1, plane_slice=0,
                min_lod_clamp_bits=0)


def resource_description():
    return dict(dimension=3, width=8, height=8, depth_or_array_size=1,
                mip_levels=1, format=28, sample_count=1, flags=0)


def lifetime(raw, generation, entry):
    return dict(raw=raw, generation=generation, create_entry=entry,
                create_return=entry + 1, create_hresult=0,
                release_entry=0, release_return=0)


def slot(role='cpu', index=0):
    # CPU descriptor handles are opaque arithmetic tokens, not addresses read.
    return dict(heap=ref(201 if role == 'cpu' else 202, 1 if role == 'cpu' else 2),
                index=index, cpu_handle=(1000 if role == 'cpu' else 2000) + 32 * index)


def event(api, call_id, entry, before, returned=True, return_at=None):
    phase = dict(case_ordinal=1, api_phase=1)
    return dict(call_id=call_id, entry=entry,
                **{'return': (entry + 1 if return_at is None else return_at) if returned else 0},
                api=api, return_kind='void', original_returned=returned,
                result=None, phase_before=phase,
                phase_after=copy.deepcopy(phase) if returned else None,
                before=before)


def create_srv(call_id, entry, resource=0, destination=None, **kwargs):
    return event('CreateShaderResourceView', call_id, entry,
                 dict(device=ref(11, 1), resource=ref(101 + resource, 1 + resource),
                      destination=slot('cpu', resource) if destination is None else destination,
                      description=srv_description()), **kwargs)


def simple_copy(call_id=3, entry=15, source=None, destination=None, **kwargs):
    return event('CopyDescriptorsSimple', call_id, entry,
                 dict(device=ref(11, 1), heap_type=0, count=1,
                      source=slot() if source is None else source,
                      destination=slot('visible') if destination is None else destination), **kwargs)


def ranges_copy(source_sizes=None, destination_sizes=None):
    return event('CopyDescriptors', 3, 15,
                 dict(device=ref(11, 1), heap_type=0,
                      source_range_count=1, source_starts=[slot()],
                      source_sizes=source_sizes,
                      destination_range_count=1, destination_starts=[slot('visible')],
                      destination_sizes=destination_sizes))


def snapshot():
    """Two equal-description resources, three slots, independent raw identities."""
    resources = []
    for index in range(2):
        r = lifetime(101 + index, 1 + index, 3 + 2 * index)
        r.update(device=ref(11, 1), description=resource_description())
        resources.append(r)
    heaps = []
    for index, role in enumerate(('cpu', 'visible')):
        h = lifetime(201 + index, 1 + index, 7 + 2 * index)
        h.update(device=ref(11, 1), role=role, heap_type=0,
                 count=2 if index == 0 else 1, flags=index, node_mask=0,
                 cpu_start=1000 if index == 0 else 2000, increment=32)
        heaps.append(h)
    return dict(schema_version=1, purpose='owned_typed_D3D12_descriptor_history',
                run_id=NONCE, domain='owned_two_texture_three_SRV_slots',
                at_transition=16, complete=True, gap=False,
                device=lifetime(11, 1, 1), resources=resources, heaps=heaps,
                events=[create_srv(1, 11), create_srv(2, 13, 1), simple_copy()],
                selected_destination=slot('visible'))


def eight_mutations():
    s = snapshot()
    s['events'] = [create_srv(i + 1, 11 + 2 * i, i % 2,
                              destination=slot('visible')) for i in range(8)]
    s['at_transition'] = 26
    return s


class TypedHistoryTests(unittest.TestCase):
    def setUp(self):
        for target in ('subprocess.Popen', 'subprocess.run', 'os.system', 'os.popen'):
            p = patch(target, side_effect=AssertionError('No child process in CPU model tests'))
            p.start()
            self.addCleanup(p.stop)

    def false_facts(self, report):
        for key in FALSE_FACTS:
            self.assertIs(report[key], False, key)

    def known(self, s=None, resource=101, generation=1):
        r = d.analyze(snapshot() if s is None else s)
        self.assertIs(r['selected_history_known'], True, r['reasons'])
        self.assertEqual(r['selected_version']['origin']['resource'], ref(resource, generation))
        self.false_facts(r)
        return r

    def unknown(self, s):
        r = d.analyze(s)
        self.assertIs(r['selected_history_known'], False)
        self.assertIsNone(r['selected_version'])
        self.assertIs(r['original_typed_copy_call_returned'], False)
        self.assertTrue(r['reasons'])
        self.false_facts(r)
        return r

    def test_simple_source_A_exact_frozen_origin_and_void_return(self):
        r = self.known()
        v = r['selected_version']
        self.assertEqual((v['call_id'], v['entry'], v['return'], v['source_call_id']), (3, 15, 16, 1))
        self.assertEqual(v['slot'], slot('visible'))
        self.assertEqual(v['origin'], dict(resource=ref(101, 1), create_call_id=1,
                         create_entry=11, create_return=12, description=srv_description()))
        self.assertIs(r['original_typed_copy_call_returned'], True)
        self.assertIsNone(r['mutations'][0]['source_version_at_entry'])
        self.assertEqual(r['mutations'][2]['source_version_at_entry']['call_id'], 1)
        self.assertNotIn('original_succeeded', r)
        self.assertIsNone(r['mutations'][2]['before'].get('result'))

    def test_equal_description_twins_are_selected_by_exact_source_slot(self):
        s = snapshot()
        s['events'][2]['before']['source'] = slot('cpu', 1)
        r = self.known(s, 102, 2)
        self.assertEqual(r['selected_version']['source_call_id'], 2)
        self.assertEqual(s['resources'][0]['description'], s['resources'][1]['description'])

    def test_direct_selected_CreateSRV_is_history_without_copy_claim(self):
        s = snapshot()
        s['events'] = [create_srv(1, 11, destination=slot('visible'))]
        s['at_transition'] = 12
        r = self.known(s)
        self.assertIs(r['original_typed_copy_call_returned'], False)
        self.assertIsNone(r['selected_version']['source_call_id'])

    def test_null_and_explicit_unit_range_sizes_preserve_original_nullness(self):
        for a, b in ((None, None), ([1], None), (None, [1]), ([1], [1])):
            with self.subTest(source=a, destination=b):
                s = snapshot()
                s['events'][2] = ranges_copy(a, b)
                r = self.known(s)
                before = r['mutations'][2]['before']
                self.assertEqual(before['source_sizes'], a)
                self.assertEqual(before['destination_sizes'], b)

    def test_completed_copy_A_remains_A_after_later_source_B_write(self):
        s = snapshot()
        s['events'].append(create_srv(4, 17, 1, destination=slot()))
        s['at_transition'] = 18
        r = self.known(s)
        self.assertEqual(r['selected_version']['call_id'], 3)
        self.assertEqual(r['mutations'][2]['source_version_at_entry']['origin']['resource'], ref(101, 1))
        self.assertEqual(r['mutations'][3]['version_after']['origin']['resource'], ref(102, 2))

    def test_new_copy_after_later_B_write_has_new_B_origin(self):
        s = snapshot()
        s['events'] += [create_srv(4, 17, 1, destination=slot()), simple_copy(5, 19)]
        s['at_transition'] = 20
        r = self.known(s, 102, 2)
        self.assertEqual(r['selected_version']['call_id'], 5)
        self.assertEqual(r['selected_version']['source_call_id'], 4)
        self.assertEqual(r['mutations'][2]['version_after']['origin']['resource'], ref(101, 1))

    def test_before_values_and_returned_report_do_not_alias_input(self):
        s = snapshot()
        r = self.known(s)
        before = json.dumps(r, sort_keys=True)
        s['events'][0]['before']['description']['format'] = 99
        s['events'][2]['before']['source']['index'] = 1
        s['resources'][0]['raw'] = 999
        self.assertEqual(json.dumps(r, sort_keys=True), before)
        self.assertEqual(r['selected_version']['origin']['description']['format'], 28)

    def test_silent_overwrite_observational_twins_never_claim_current_or_GPU(self):
        observations = snapshot()
        # Groundtruth is intentionally outside the model's supplied API history.
        groundtruth = ('unobserved bytes still A', 'unobserved overwrite to B')
        a, b = (d.analyze(copy.deepcopy(observations)) for _ in groundtruth)
        self.assertEqual(a, b)
        self.assertTrue(a['selected_history_known'])
        self.false_facts(a)

    def test_pending_copy_has_no_after_version_or_fabricated_result(self):
        s = snapshot()
        s['events'][2] = simple_copy(returned=False)
        s['at_transition'] = 15
        r = self.unknown(s)
        m = r['mutations'][2]
        self.assertIsNone(m['version_after'])
        self.assertEqual(m['return'], 0)
        self.assertIs(m['original_returned'], False)

    def test_source_write_inside_copy_interval_and_reverse_after_are_unknown(self):
        for return_at in (18, 20):
            with self.subTest(copy_return=return_at):
                s = snapshot()
                s['events'][2]['return'] = return_at
                s['events'].append(create_srv(4, 16, 1, destination=slot(), return_at=19))
                s['at_transition'] = max(19, return_at)
                r = self.unknown(s)
                self.assertIn('overlapping_mutation_interval', r['reasons'])
                self.assertIsNone(r['mutations'][2]['version_after'])

    def test_overlapping_destination_writes_do_not_choose_last_after(self):
        s = snapshot()
        s['events'][2]['return'] = 18
        s['events'].append(simple_copy(4, 16, source=slot('cpu', 1), return_at=17))
        s['at_transition'] = 18
        r = self.unknown(s)
        self.assertIn('overlapping_mutation_interval', r['reasons'])
        self.assertTrue(all(m['version_after'] is None for m in r['mutations'][2:]))

    def test_disjoint_slot_write_inside_copy_does_not_invent_a_conflict(self):
        s = snapshot()
        s['events'][2]['return'] = 19
        s['events'].append(create_srv(4, 16, 1, return_at=18))
        s['at_transition'] = 19
        r = self.known(s)
        self.assertEqual(r['selected_version']['source_call_id'], 1)
        self.assertEqual(r['mutations'][3]['version_after']['origin']['resource'], ref(102, 2))

    def test_later_conflicting_call_is_not_repaired_by_return_order_or_list_order(self):
        s = snapshot()
        s['events'][2]['return'] = 19
        s['events'].append(create_srv(4, 16, 1, destination=slot(), return_at=18))
        s['at_transition'] = 19
        a = self.unknown(s)
        s['events'].reverse()
        self.assertEqual(a, self.unknown(s))

    def test_ranges_nonunit_mismatch_duplicate_and_unavailable_are_unknown(self):
        changes = (
            ('source_range_count', 0), ('source_range_count', 2),
            ('destination_range_count', 2), ('source_sizes', [0]),
            ('source_sizes', [2]), ('destination_sizes', [2]),
            ('source_sizes', []), ('source_starts', []),
            ('source_starts', [slot(), slot()]),
        )
        for key, value in changes:
            with self.subTest(field=key, value=value):
                s = snapshot()
                s['events'][2] = ranges_copy()
                s['events'][2]['before'][key] = value
                self.unknown(s)

    def test_wrong_copy_direction_slots_and_heap_type(self):
        mutations = (
            lambda a: a.update(source=slot('visible'), destination=slot()),
            lambda a: a.update(destination=slot('cpu', 1)),
            lambda a: a.update(source=slot('visible')),
            lambda a: a.update(heap_type=1),
            lambda a: a.update(count=0), lambda a: a.update(count=2),
            lambda a: a['source']['heap'].update(generation=99),
            lambda a: a['destination']['heap'].update(raw=999),
            lambda a: a['source'].update(cpu_handle=1001),
            lambda a: a['source'].update(index=2, cpu_handle=1064),
        )
        for index, change in enumerate(mutations):
            with self.subTest(index=index):
                s = snapshot()
                change(s['events'][2]['before'])
                self.unknown(s)

    def test_default_null_or_other_SRV_union_cannot_publish_origin(self):
        for key, value in (('resource', None), ('description', None)):
            s = snapshot()
            s['events'][0]['before'][key] = value
            self.unknown(s)
        for key, value in (('format', 50), ('view_dimension', 1), ('component_mapping', 0),
                           ('most_detailed_mip', 1), ('mip_levels', 2),
                           ('plane_slice', 1), ('min_lod_clamp_bits', 0x7fc00000)):
            with self.subTest(field=key):
                s = snapshot()
                s['events'][0]['before']['description'][key] = value
                self.unknown(s)

    def test_incomplete_gap_or_changed_phase_never_claims_complete_history(self):
        for key, value in (('complete', False), ('gap', True)):
            s = snapshot()
            s[key] = value
            self.unknown(s)
        for phase in (dict(case_ordinal=2, api_phase=1), dict(case_ordinal=1, api_phase=2)):
            s = snapshot()
            s['events'][2]['phase_after'] = phase
            self.unknown(s)
        s = snapshot()
        s['events'][2]['phase_before']['case_ordinal'] = 0
        s['events'][2]['phase_after']['case_ordinal'] = 0
        self.unknown(s)

    def test_original_void_result_null_is_required_not_HRESULT_zero(self):
        for value in (0, False, 'S_OK'):
            s = snapshot()
            s['events'][2]['result'] = value
            with self.assertRaises(ValueError):
                d.analyze(s)
        s = snapshot()
        s['events'][2]['return_kind'] = 'HRESULT'
        with self.assertRaises(ValueError):
            d.analyze(s)

    def test_strict_scalars_reject_bool_float_negative_and_U64_overflow(self):
        mutations = (
            lambda s: s.update(schema_version=True),
            lambda s: s.update(at_transition=True),
            lambda s: s.update(complete=1),
            lambda s: s['device'].update(raw=True),
            lambda s: s['resources'][0].update(generation=False),
            lambda s: s['heaps'][0].update(increment=True),
            lambda s: s['heaps'][0].update(increment=1 << 32),
            lambda s: s['events'][2]['before'].update(count=True),
            lambda s: s['events'][2]['before']['source'].update(cpu_handle=-1),
            lambda s: s['events'][2]['before']['source'].update(cpu_handle=1 << 64),
            lambda s: s['events'][2]['before'].update(heap_type=0.0),
            lambda s: s['events'][2]['phase_before'].update(api_phase=True),
        )
        for index, change in enumerate(mutations):
            with self.subTest(index=index):
                s = snapshot()
                change(s)
                with self.assertRaises(ValueError):
                    d.analyze(s)

    def test_exact_keys_unknown_API_duplicate_calls_and_common_clock_rejected(self):
        mutations = (
            lambda s: s.update(unobserved_current_bytes='aa'),
            lambda s: s['events'][2]['before'].update(source_version_at_entry={}),
            lambda s: s['events'][2].update(api='CopyDescriptorBytes'),
            lambda s: s['events'][2].update(call_id=1),
            lambda s: s['events'][2].update(entry=13),
            lambda s: s['events'][2].update(**{'return': 15}),
            lambda s: s['events'][2].update(original_returned=False),
            lambda s: s.update(at_transition=15),
        )
        for index, change in enumerate(mutations):
            with self.subTest(index=index):
                s = snapshot()
                change(s)
                with self.assertRaises(ValueError):
                    d.analyze(s)

    def test_checked_handle_arithmetic_and_unique_heap_spans(self):
        for base, increment in ((U64 - 15, 32), (1000, 0)):
            s = snapshot()
            s['heaps'][0].update(cpu_start=base, increment=increment)
            self.unknown(s)
        s = snapshot()
        s['heaps'][1]['cpu_start'] = 1032
        s['events'][2]['before']['destination']['cpu_handle'] = 1032
        s['selected_destination']['cpu_handle'] = 1032
        self.assertIn('ambiguous_overlapping_heap_handle_spans', self.unknown(s)['reasons'])

    def test_failed_and_pending_creates_publish_no_live_occurrence(self):
        for kind, index in (('device', None), ('resources', 0), ('heaps', 0), ('heaps', 1)):
            for pending in (False, True):
                with self.subTest(kind=kind, index=index, pending=pending):
                    s = snapshot()
                    item = s[kind] if index is None else s[kind][index]
                    item.update(raw=0, create_return=0 if pending else item['create_return'],
                                create_hresult=None if pending else -2147467259)
                    self.unknown(s)
        s = snapshot()
        s['resources'][0]['create_hresult'] = -1
        with self.assertRaises(ValueError):
            d.analyze(s)

    def test_release_during_copy_cannot_become_a_returned_version(self):
        for kind, index in (('device', None), ('resources', 0), ('heaps', 0), ('heaps', 1)):
            with self.subTest(kind=kind, index=index):
                s = snapshot()
                s['events'][2]['return'] = 18
                item = s[kind] if index is None else s[kind][index]
                item.update(release_entry=16, release_return=17)
                s['at_transition'] = 18
                self.unknown(s)

    def test_reused_resource_raw_handle_joins_generation_not_handle_number(self):
        s = snapshot()
        s['resources'][0].update(release_entry=13, release_return=14)
        s['resources'][1].update(raw=101, create_entry=17, create_return=18)
        s['events'] = [create_srv(1, 11), create_srv(2, 19, 1)]
        s['events'][1]['before'].update(resource=ref(101, 2), destination=slot())
        s['events'].append(simple_copy(3, 21))
        s['at_transition'] = 22
        self.known(s, 101, 2)
        s['events'][1]['before']['resource']['generation'] = 1
        self.unknown(s)

    def test_overlapping_raw_generations_or_duplicate_class_generations_unknown(self):
        for kind in ('resources', 'heaps'):
            s = snapshot()
            s[kind][1]['raw'] = s[kind][0]['raw']
            self.unknown(s)
            s = snapshot()
            s[kind][1]['generation'] = s[kind][0]['generation']
            self.unknown(s)

    def test_generation_assignment_follows_creation_order_within_each_class(self):
        for kind in ('resources', 'heaps'):
            with self.subTest(kind=kind):
                s = snapshot()
                # Both IDs remain individually valid and distinct; changing
                # their temporal assignment cannot manufacture a new history.
                s[kind][0]['generation'], s[kind][1]['generation'] = 2, 1
                self.unknown(s)

    def test_raw_reuse_cannot_begin_before_prior_Release_returns(self):
        s = snapshot()
        s['resources'][0].update(release_entry=13, release_return=18)
        s['resources'][1].update(raw=101, create_entry=15, create_return=16)
        s['events'] = [create_srv(1, 11), create_srv(2, 19, 1)]
        s['events'][1]['before'].update(resource=ref(101, 2), destination=slot())
        s['events'].append(simple_copy(3, 21))
        s['at_transition'] = 22
        r = self.unknown(s)
        self.assertIn('overlapping_raw_object_lifetimes', r['reasons'])

    def test_json_bytes_strings_duplicate_keys_floats_and_depth_are_bounded(self):
        s = snapshot()
        text = json.dumps(s)
        self.assertEqual(self.known(s), d.analyze(text))
        self.assertEqual(d.analyze(text), d.analyze(text.encode('utf8')))
        for raw in ('{"schema_version":1,"schema_version":1}', '{"n":NaN}',
                    'x' * 16385, '[' * 20 + '0' + ']' * 20):
            with self.subTest(raw=raw[:50]), self.assertRaises(ValueError):
                d.analyze(raw)

    def test_eight_record_budget_is_per_ticket_not_shared_global_eight(self):
        s = eight_mutations()
        self.known(s, 102, 2)
        reg = d.Registry()
        tickets = [reg.reserve(i + 1) for i in range(4)]
        receipts = []
        for t in tickets:
            self.assertTrue(t.record(s))
            receipts.append(t.receipt())
        stats = reg.stats()
        self.assertEqual((stats['live_tickets'], stats['sealed_tickets'], stats['retained_records']), (4, 4, 32))
        self.assertEqual(stats['limits'], dict(tickets=4, slots_per_ticket=3,
                         records_per_ticket=8, bytes_per_ticket=16384))
        self.assertLessEqual(stats['serialized_bytes'], 4 * 16384)
        self.assertGreater(stats['serialized_bytes'], 16384)
        for r in receipts:
            self.assertLessEqual(len(r.serialized().encode('ascii')), 16384)
        too_many = copy.deepcopy(s)
        too_many['events'].append(create_srv(9, 27, destination=slot('visible')))
        too_many['at_transition'] = 28
        with self.assertRaises(ValueError):
            d.analyze(too_many)

    def test_complete_wire_budget_includes_sequence_envelope_at_exact_boundary(self):
        s = snapshot()
        report = self.known(s)
        wire = json.dumps(dict(sequence=1, history=report), sort_keys=True,
                          separators=(',', ':'), ensure_ascii=True, allow_nan=False)
        cap = len(wire.encode('ascii'))
        self.assertLess(cap, 16384)
        self.assertLess(len(json.dumps(s).encode('ascii')), cap - 1)
        with patch.object(d, 'MAX_BYTES', cap):
            t = d.Registry().reserve(1)
            self.assertTrue(t.record(s))
            self.assertEqual(t.receipt().serialized(), wire)
        with patch.object(d, 'MAX_BYTES', cap - 1):
            # analyze's body still fits; only the complete retained envelope
            # crosses this deliberately reduced limit. No truncated receipt.
            self.assertEqual(d.analyze(s), report)
            reg = d.Registry()
            t = reg.reserve(1)
            self.assertFalse(t.record(s))
            self.assertIsNone(t.receipt())
            self.assertEqual(reg.stats()['serialized_bytes'], 0)
        self.assertTrue(t.record(s))
        self.assertEqual(t.receipt().serialized(), wire)

    def test_fourth_slot_identity_is_diagnostic_only_and_cannot_seal(self):
        s = snapshot()
        s['selected_destination'] = slot('visible', 1)
        r = self.unknown(s)
        self.assertIn('slot_identity_budget_exceeded', r['reasons'])
        reg = d.Registry()
        t = reg.reserve(1)
        self.assertFalse(t.record(s))
        self.assertIsNone(t.receipt())
        self.assertEqual(reg.stats()['retained_slot_occurrences'], 0)
        self.assertTrue(t.record(snapshot()))
        self.assertEqual(reg.stats()['retained_slot_occurrences'], 3)

    def test_selected_heap_release_or_recording_reset_cannot_retag_a_held_receipt(self):
        reg = d.Registry()
        t = reg.reserve(1)
        s = snapshot()
        self.assertTrue(t.record(s))
        receipt = t.receipt()
        wire = receipt.serialized()
        s['heaps'][1].update(release_entry=17, release_return=18)
        s['at_transition'] = 18
        self.unknown(s)
        self.assertEqual(receipt.serialized(), wire)
        # A new input recording without the original writes cannot inherit an
        # old batch's slot history, even with the same raw object numbers.
        fresh = snapshot()
        fresh['events'] = []
        self.unknown(fresh)
        self.assertEqual(receipt.serialized(), wire)

    def test_original_phase_values_and_occurrence_declarations_are_frozen(self):
        s = snapshot()
        s['events'][2]['phase_before']['api_phase'] = 2
        s['events'][2]['phase_after']['api_phase'] = 2
        r = self.known(s)
        self.assertEqual(r['mutations'][2]['phase_before'], dict(case_ordinal=1, api_phase=2))
        self.assertEqual(r['mutations'][2]['phase_after'], dict(case_ordinal=1, api_phase=2))
        for key in ('domain', 'at_transition', 'complete', 'gap', 'device', 'resources', 'heaps',
                    'selected_destination'):
            self.assertEqual(r[key], s[key])
        wire = json.dumps(r, sort_keys=True)
        s['events'][2]['phase_before']['case_ordinal'] = 2
        s['heaps'][0]['cpu_start'] = 9000
        s['device']['generation'] = 12
        self.assertEqual(json.dumps(r, sort_keys=True), wire)

    def test_registry_seal_once_detached_report_retry_and_fresh_registry(self):
        reg = d.Registry()
        t = reg.reserve(1)
        malformed = snapshot()
        malformed['events'][0]['result'] = 0
        self.assertFalse(t.record(malformed))
        self.assertIsNone(t.receipt())
        s = snapshot()
        self.assertTrue(t.record(s))
        receipt = t.receipt()
        wire = receipt.serialized()
        digest = receipt.sha256
        s['events'][2]['before']['source'] = slot('cpu', 1)
        self.assertFalse(t.record(s))
        altered = receipt.report()
        altered['history']['selected_version']['origin']['resource']['raw'] = 999
        self.assertEqual(receipt.serialized(), wire)
        self.assertEqual(receipt.sha256, digest)
        self.assertEqual(receipt.report()['history']['selected_version']['origin']['resource'], ref(101, 1))
        self.assertEqual(receipt.report()['sequence'], 1)
        # A new registry/batch does not rewrite an old reservation's history.
        other = d.Registry().reserve(1)
        self.assertTrue(other.record(s))
        self.assertEqual(other.receipt().report()['history']['selected_version']['origin']['resource'], ref(102, 2))
        self.assertEqual(receipt.serialized(), wire)

    def test_four_shared_tickets_copies_pin_quota_through_eighty_refusals(self):
        reg = d.Registry()
        tickets = [reg.reserve(i + 1) for i in range(4)]
        for ticket in tickets:
            self.assertTrue(ticket.record(snapshot()))
        receipts = [ticket.receipt() for ticket in tickets]
        original_wire = [r.serialized() for r in receipts]
        self.assertIs(copy.copy(tickets[0]), tickets[0])
        self.assertIs(copy.deepcopy(tickets[0]), tickets[0])
        c = copy.copy(receipts[0])
        dc = copy.deepcopy(receipts[0])
        self.assertIs(c, receipts[0])
        self.assertIs(dc, receipts[0])
        tickets.clear()
        del ticket
        gc.collect()
        for sequence in range(5, 85):
            self.assertIsNone(reg.reserve(sequence))
        self.assertEqual([r.serialized() for r in receipts], original_wire)
        receipts[0] = None
        gc.collect()
        self.assertIsNone(reg.reserve(85))
        del c, dc
        gc.collect()
        replacement = reg.reserve(85)
        self.assertIsNotNone(replacement)
        self.assertTrue(replacement.record(snapshot()))
        self.assertEqual(reg.stats()['live_tickets'], 4)
        self.assertEqual([r.serialized() for r in receipts[1:]], original_wire[1:])
        del replacement
        receipts.clear()
        gc.collect()
        self.assertEqual(reg.stats()['live_tickets'], 0)
        self.assertEqual(reg.stats()['serialized_bytes'], 0)

    def test_unsealed_reservations_also_pin_quota_and_sequences_are_strict(self):
        reg = d.Registry()
        tickets = [reg.reserve(i + 1) for i in range(4)]
        self.assertIsNone(reg.reserve(1))
        self.assertIsNone(reg.reserve(5))
        self.assertEqual(reg.stats()['sealed_tickets'], 0)
        for sequence in (0, True, -1, 1 << 64, 1.0):
            with self.subTest(sequence=sequence), self.assertRaises(ValueError):
                reg.reserve(sequence)
        tickets.clear()
        gc.collect()
        self.assertIsNotNone(reg.reserve(1))

    def test_no_IO_or_process_is_needed_to_analyze_and_seal(self):
        s = snapshot()
        with patch('builtins.open', side_effect=AssertionError('No file read')), \
             patch.object(Path, 'open', side_effect=AssertionError('No path read')):
            self.known(s)
            t = d.Registry().reserve(1)
            self.assertTrue(t.record(s))
            self.assertTrue(t.receipt().report()['history']['selected_history_known'])


if __name__ == '__main__':
    unittest.main()
