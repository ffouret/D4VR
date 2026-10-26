"""Falsify frozen BLIT receipts without a subprocess, Vulkan, or a game."""
from copy import deepcopy
import json
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import blit_provenance_validate as audit


def source_blit(status='producer_complete'):
    submitted = status in ('submitted', 'producer_complete')
    complete = status == 'producer_complete'
    return {'original': dict(zip(audit.REF_FIELDS, (99, 11, 12, 21, 7)), event_retained=True),
            'source_image_handle': 1001, 'destination_image_handle': 2001,
            'source_layout': 6, 'destination_layout': 7, 'filter': 1,
            'source_subresource': [1, 0, 0, 1], 'destination_subresource': [1, 0, 0, 1],
            'source_offsets': [[0, 0, 0], [8, 8, 1]], 'destination_offsets': [[0, 0, 0], [8, 8, 1]],
            'history_submission': 70 if submitted else 0,
            'capture_submission': 40 if submitted else 0,
            'identity_known': True, 'region_known': True,
            'original_submit_succeeded': submitted,
            'history_submission_known': submitted, 'capture_submission_known': submitted,
            'gpu_complete': complete,
            'bridge_known': complete, 'status': status}


def fixture():
    observed = source_blit()
    original = {key: deepcopy(observed[key]) for key in audit.ARGUMENT_FIELDS}
    original.update(frame=2, command=3001, original_submit_result=0)
    hook = {'sequence': 8, 'eye': 0, 'command': 3001, 'source_blit': source_blit('recorded')}
    submit = {'sequence': 8, 'source_blit': source_blit('submitted')}
    receipt = {'capture_sequence': 8, 'present': 3, 'eye': 0, 'lease': 17,
               'source_blit': observed,
               'camera_provenance': {'copy_submit': {'first_submission': 40, 'last_submission': 40}},
               'whole_scene_verified': False, 'xr_pose_known': False}
    return [receipt], [original], [hook], [submit]


def staged_fixture():
    data = fixture()
    data[0][0]['source_blit_stages'] = {
        'recorded_known': True, 'submitted_known': True,
        'recorded': deepcopy(data[2][0]['source_blit']),
        'submitted': deepcopy(data[3][0]['source_blit'])}
    return data


def absent_blit():
    value = source_blit('recorded')
    value.update(status='absent', identity_known=False, region_known=False,
                 source_image_handle=0, destination_image_handle=0,
                 source_layout=0, destination_layout=0, filter=0,
                 source_subresource=[0]*4, destination_subresource=[0]*4,
                 source_offsets=[[0]*3 for _ in range(2)],
                 destination_offsets=[[0]*3 for _ in range(2)])
    value['original'] = dict(dict.fromkeys(audit.REF_FIELDS, 0), event_retained=False)
    return value


def history(observed=None):
    observed = observed or source_blit()
    subresource = observed['source_subresource']
    destination = observed['destination_subresource']
    event = {'kind': 'blit_image', 'ordinal': observed['original']['ordinal'],
             'image': observed['original']['source_image'], 'destination_image': observed['original']['destination_image'],
             'layout': observed['source_layout'], 'destination_layout': observed['destination_layout'],
             'filter': observed['filter'], 'aspects': subresource[0],
             'base_mip': subresource[1], 'mip_count': 1, 'base_layer': subresource[2],
             'layer_count': subresource[3], 'destination_aspects': destination[0],
             'destination_base_mip': destination[1], 'destination_mip_count': 1,
             'destination_base_layer': destination[2], 'destination_layer_count': destination[3],
             'blit_source_offsets': deepcopy(observed['source_offsets']),
             'blit_destination_offsets': deepcopy(observed['destination_offsets'])}
    return [{'process_id': 123, 'device': 1, 'history': {'observer_session': 99,
             'gpu_execution_order_known': False,
             'reports': [{'occurrences': [{'submission': 70, 'capture_submission': 40,
                                          'complete': True, 'result': 0,
                                          'commands': [{'recording': 21, 'events': [event]}]}]}]}}]


class BlitProvenanceAudit(unittest.TestCase):
    def check(self, data=None, wrappers=()):
        return audit.audit_receipts(*(data or fixture()), wrappers)

    def test_exact_successful_original_is_distinct_from_record_and_fence(self):
        result = self.check(wrappers=history())
        self.assertEqual(result['exact_original_blit_receipts'], 1)
        self.assertEqual(result['retained_journal_events_matched'], 1)
        self.assertFalse(result['gpu_execution_order_known'])
        self.assertFalse(result['descriptor_query_implies_read'])
        self.assertNotEqual(result['receipts'][0]['capture_submission'], result['receipts'][0]['history_submission'])

    def test_unretained_original_ref_keeps_exact_hook_bridge_without_journal_claim(self):
        data = fixture()
        data[0][0]['source_blit']['original']['event_retained'] = False
        data[2][0]['source_blit']['original']['event_retained'] = False
        data[3][0]['source_blit']['original']['event_retained'] = False
        result = self.check(data)
        self.assertEqual(result['hook_only_original_refs_matched'], 1)
        self.assertEqual(result['retained_journal_events_matched'], 0)
        with self.assertRaises(ValueError):
            self.check(data, history())

    def test_same_sized_twins_reverse_and_recording_ordinals_cannot_join(self):
        changes = [lambda o: o['original'].update(source_image=13),
                   lambda o: o['original'].update(destination_image=14),
                   lambda o: o['original'].update(recording=22),
                   lambda o: o['original'].update(ordinal=8),
                   lambda o: o['original'].update(observer_session=100),
                   lambda o: o.update(source_image_handle=1002),
                   lambda o: o.update(destination_image_handle=2002),
                   lambda o: o.update(source_image_handle=2001, destination_image_handle=1001),
                   lambda o: o['original'].update(source_image=12, destination_image=11)]
        for mutate in changes:
            with self.subTest(mutate=mutate):
                data = fixture()
                mutate(data[0][0]['source_blit'])
                with self.assertRaises(ValueError): self.check(data)

    def test_original_direction_is_valid_when_all_three_sources_agree(self):
        data = fixture()
        for observed in (data[0][0]['source_blit'], data[2][0]['source_blit'], data[3][0]['source_blit']):
            observed.update(source_image_handle=2001, destination_image_handle=1001)
            observed['original'].update(source_image=12, destination_image=11)
        data[1][0].update(source_image_handle=2001, destination_image_handle=1001)
        self.assertEqual(self.check(data)['exact_original_blit_receipts'], 1)

    def test_layout_filter_both_regions_ranges_and_offsets_are_frozen(self):
        changes = [lambda o: o.update(source_layout=1), lambda o: o.update(destination_layout=1),
                   lambda o: o.update(filter=0),
                   lambda o: o['source_subresource'].__setitem__(1, 1),
                   lambda o: o['destination_subresource'].__setitem__(2, 1),
                   lambda o: o['source_offsets'][1].__setitem__(0, 7),
                   lambda o: o['destination_offsets'][0].__setitem__(1, 1),
                   lambda o: o.update(source_offsets=[[0, 0, 0], [True, 8, 1]])]
        for mutate in changes:
            with self.subTest(mutate=mutate):
                data = fixture(); mutate(data[0][0]['source_blit'])
                with self.assertRaises(ValueError): self.check(data)

    def test_record_ref_missing_or_failed_original_submit_cannot_deliver(self):
        changes = [lambda d: d[3].clear(), lambda d: d[3][0]['source_blit'].update(original_submit_succeeded=False),
                   lambda d: d[1][0].update(original_submit_result=-4),
                   lambda d: d[3][0]['source_blit']['original'].update(recording=9999),
                   lambda d: d[0][0].update(source_blit=source_blit('recorded')),
                   lambda d: d[0][0].update(source_blit=source_blit('submitted')),
                   lambda d: d[2][0].update(source_blit=source_blit('producer_complete'))]
        for mutate in changes:
            with self.subTest(mutate=mutate):
                data = fixture(); mutate(data)
                with self.assertRaises(ValueError): self.check(data)

    def test_namespace_counters_cannot_replace_each_other_or_camera_counter(self):
        for mutate in (lambda d: d[0][0]['source_blit'].update(history_submission=40),
                       lambda d: d[0][0]['source_blit'].update(capture_submission=70),
                       lambda d: d[0][0]['camera_provenance']['copy_submit'].update(first_submission=70),
                       lambda d: d[0][0]['source_blit'].update(history_submission_known=True, history_submission=0),
                       lambda d: d[0][0]['source_blit'].update(capture_submission_known=False)):
            data = fixture(); mutate(data)
            with self.assertRaises(ValueError): self.check(data)
        # Equal numeric values are allowed when both independently observed
        # namespaces agree; inequality is not a substitute for provenance.
        data = fixture(); data[0][0]['source_blit']['history_submission'] = 40
        data[3][0]['source_blit']['history_submission'] = 40
        self.assertEqual(self.check(data)['exact_original_blit_receipts'], 1)

    def test_unknown_generic_capture_cannot_assert_history_identity_or_bridge(self):
        original = source_blit()
        original.update(identity_known=False, bridge_known=False)
        original['original'] = dict(dict.fromkeys(audit.REF_FIELDS, 0), event_retained=False)
        audit.source_blit(original)
        with self.assertRaises(ValueError): audit.source_blit(original, require_bridge=True)
        for mutate in (lambda o: o.update(bridge_known=True),
                       lambda o: o.update(identity_known=True),
                       lambda o: o['original'].update(event_retained=True),
                       lambda o: o.update(identity_known=True, source_image_handle=0)):
            value = deepcopy(original); mutate(value)
            with self.assertRaises(ValueError): audit.source_blit(value)

    def test_generic_off_compares_original_arguments_without_inventing_history(self):
        data = fixture()
        observed = data[0][0]['source_blit']
        observed.update(identity_known=False, bridge_known=False)
        observed['original'] = dict(dict.fromkeys(audit.REF_FIELDS, 0), event_retained=False)
        data[2].clear(); data[3].clear()
        result = audit.audit_receipts(*data, require_bridge=False)
        self.assertEqual(result['exact_original_blit_receipts'], 0)
        self.assertEqual(result['generic_completed_capture_receipts'], 1)
        self.assertEqual(result['hook_only_original_refs_matched'], 0)
        self.assertFalse(result['receipts'][0]['record_and_submit_snapshots_independently_available'])
        with self.assertRaises(ValueError): self.check(data)
        for mutate in (lambda d: d[0][0]['source_blit'].update(source_image_handle=1002),
                       lambda d: d[0][0]['source_blit']['original'].update(recording=21),
                       lambda d: d[0][0]['source_blit'].update(gpu_complete=False),
                       lambda d: d[1][0].update(original_submit_result=-4)):
            changed = deepcopy(data); mutate(changed)
            with self.assertRaises(ValueError): audit.audit_receipts(*changed, require_bridge=False)

    def test_delayed_delivery_uses_original_source_present_and_immutable_generation(self):
        data = fixture()
        data[0][0]['source_present'] = data[0][0].pop('present')
        data[0][0]['delivery_present'] = 99
        later = deepcopy(data[1][0]); later.update(frame=98, command=3001, source_image_handle=1002)
        data[1].append(later)
        self.assertEqual(self.check(data)['receipts'][0]['source_present'], 3)
        # Current handle lookup/pose retagging must not overwrite the old value.
        data[0][0]['source_blit']['source_image_handle'] = 1002
        with self.assertRaises(ValueError): self.check(data)

    def test_reset_reused_command_and_lease_cannot_retag_old_receipt(self):
        data = fixture()
        second = deepcopy(data[0][0]); second.update(capture_sequence=9, present=4)
        second['source_blit']['original'].update(recording=22, ordinal=1, source_image=13)
        second['source_blit'].update(source_image_handle=1002, history_submission=71, capture_submission=41)
        second['camera_provenance']['copy_submit'].update(first_submission=41, last_submission=41)
        original = deepcopy(data[1][0]); original.update(frame=3, source_image_handle=1002)
        hook = deepcopy(data[2][0]); hook.update(sequence=9)
        hook['source_blit']['original'].update(recording=22, ordinal=1, source_image=13)
        hook['source_blit']['source_image_handle'] = 1002
        submit = deepcopy(data[3][0]); submit.update(sequence=9); submit['source_blit'] = deepcopy(second['source_blit']); submit['source_blit'].update(status='submitted', gpu_complete=False, bridge_known=False)
        for rows, row in zip(data, (second, original, hook, submit)): rows.append(row)
        with self.assertRaises(ValueError): self.check(data)  # Old lease token.
        data[0][1]['lease'] = 18
        self.assertEqual(self.check(data)['exact_original_blit_receipts'], 2)
        data[0][0]['source_blit']['original'] = deepcopy(data[0][1]['source_blit']['original'])
        with self.assertRaises(ValueError): self.check(data)

    def test_retained_journal_cannot_replace_frozen_arguments_or_after_result(self):
        for mutate in (lambda w: w[0]['history']['reports'][0]['occurrences'][0].update(result=-4),
                       lambda w: w[0]['history']['reports'][0]['occurrences'][0].update(complete=False),
                       lambda w: w[0]['history']['reports'][0]['occurrences'][0].update(capture_submission=70),
                       lambda w: w[0]['history']['reports'][0]['occurrences'][0]['commands'][0]['events'][0].update(filter=0)):
            wrappers = history(); mutate(wrappers)
            with self.assertRaises(ValueError): self.check(wrappers=wrappers)

    def test_queries_scene_or_xr_claims_do_not_follow_a_blit(self):
        for key in audit.NO_INFERENCE:
            data = fixture(); data[0][0][key] = True
            with self.subTest(key=key), self.assertRaises(ValueError): self.check(data)

    def test_duplicate_json_nonfinite_boolean_counter_and_symlink_are_refused(self):
        for raw in ('{"a":1,"a":2}', '{"n":NaN}', '{"n":Infinity}'):
            with self.assertRaises(ValueError): audit.decode(raw)
        data = fixture(); data[0][0]['source_blit']['history_submission'] = True
        with self.assertRaises(ValueError): self.check(data)
        with TemporaryDirectory() as temporary:
            path = Path(temporary)/'data.json'; path.write_text('{}')
            link = Path(temporary)/'link.json'; link.symlink_to(path)
            with self.assertRaises(ValueError): audit.read(link)

    def test_reader_compares_exported_pixels_to_original_frozen_frame(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            receipts, originals, hooks, submissions = fixture()
            receipt = receipts[0]; receipt['sequence'] = receipt.pop('capture_sequence')
            rows = [dict(row, event='presentation_blit_recorded') for row in hooks]
            rows += [dict(row, event='presentation_blit_submitted') for row in submissions]
            (directory/'vulkan-123.jsonl').write_text('\n'.join(json.dumps(row) for row in rows)+'\n')
            (directory/'blit-fixture.jsonl').write_text('\n'.join(json.dumps(row) for row in originals)+'\n')
            (directory/'run.log').write_text('Owned fixture completed\n')
            (directory/'image-123-1-8.json').write_text(json.dumps(receipt))
            (directory/'image-123-1-8.pixels').write_bytes(b'old-pixels')
            (directory/'source-frame-2.pixels').write_bytes(b'old-pixels')
            result = audit.audit_directory(directory)
            self.assertEqual(result['original_pixel_bytes_compared'], 10)
            self.assertIn('blit-fixture.jsonl', result['input_sha256'])
            (directory/'source-frame-2.pixels').write_bytes(b'new-pixels')
            with self.assertRaises(ValueError): audit.audit_directory(directory)


class SavedBlitLifecycleAudit(unittest.TestCase):
    def check(self, data=None, *, require_bridge=True):
        # The evidence reader must never turn a CPU falsification into a
        # compiler, GPU process, game, or other executable invocation.
        with patch('subprocess.Popen', side_effect=AssertionError('No process in read-only audit')):
            return audit.audit_receipts(*(data or staged_fixture()),
                                        require_bridge=require_bridge, require_stages=True)

    def test_saved_values_match_separate_original_hooks_and_final_fence(self):
        result = self.check()
        self.assertEqual(result['delivered_lifecycle_snapshot_pairs'], 1)
        self.assertTrue(result['receipts'][0]['record_and_submit_snapshots_independently_available'])
        self.assertTrue(result['receipts'][0]['delivered_record_and_submit_snapshots_available'])
        self.assertFalse(result['gpu_execution_order_known'])

    def test_old_proof_mode_remains_explicit_and_staged_mode_requires_values(self):
        self.assertEqual(audit.audit_receipts(*fixture())['exact_original_blit_receipts'], 1)
        with self.assertRaises(ValueError): self.check(fixture())
        data = staged_fixture()
        data[0][0]['source_blit_stages']['submitted']['gpu_complete'] = True
        # Optional does not mean a present forged stage can be ignored.
        with self.assertRaises(ValueError): audit.audit_receipts(*data)

    def test_default_unknown_values_are_absent_and_cannot_be_later_reconstruction(self):
        final = absent_blit()
        empty = {'recorded_known': False, 'submitted_known': False,
                 'recorded': absent_blit(), 'submitted': absent_blit()}
        audit.source_blit_stages(empty, final)
        with self.assertRaises(ValueError): audit.source_blit_stages(empty, final, require_known=True)
        changes = [lambda s: s['recorded'].update(status='recorded'),
                   lambda s: s['recorded'].update(source_image_handle=1001),
                   lambda s: s['submitted']['original'].update(recording=21),
                   lambda s: s['submitted']['source_offsets'][1].__setitem__(0, 8),
                   lambda s: s['submitted']['source_subresource'].__setitem__(0, 1),
                   lambda s: s['recorded'].update(filter=1)]
        for mutate in changes:
            value = deepcopy(empty); mutate(value)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                audit.source_blit_stages(value, final)

    def test_saved_phase_flags_and_missing_fields_cannot_invent_success(self):
        changes = [lambda s: s.pop('recorded'),
                   lambda s: s.update(recorded_known=1),
                   lambda s: s.update(submitted_known=False),
                   lambda s: s.update(recorded_known=False, recorded=absent_blit()),
                   lambda s: s['submitted'].update(original_submit_succeeded=False),
                   lambda s: s['recorded'].update(status='submitted'),
                   lambda s: s['submitted'].update(status='producer_complete'),
                   lambda s: s['recorded'].update(history_submission=70),
                   lambda s: s['submitted'].update(gpu_complete=True),
                   lambda s: s['submitted'].update(bridge_known=True)]
        for mutate in changes:
            data = staged_fixture(); mutate(data[0][0]['source_blit_stages'])
            with self.subTest(mutate=mutate), self.assertRaises((ValueError, KeyError)):
                self.check(data)

    def test_each_saved_lifetime_endpoint_direction_and_geometry_is_frozen(self):
        changes = [lambda o: o['original'].update(observer_session=100),
                   lambda o: o['original'].update(source_image=13),
                   lambda o: o['original'].update(destination_image=14),
                   lambda o: o['original'].update(recording=22),
                   lambda o: o['original'].update(ordinal=8),
                   lambda o: o['original'].update(event_retained=False),
                   lambda o: o.update(source_image_handle=2001, destination_image_handle=1001),
                   lambda o: o.update(source_layout=1),
                   lambda o: o.update(destination_layout=1),
                   lambda o: o.update(filter=0),
                   lambda o: o['source_offsets'][1].__setitem__(0, 7),
                   lambda o: o['destination_offsets'][1].__setitem__(1, 7),
                   lambda o: o['source_subresource'].__setitem__(1, 1),
                   lambda o: o['destination_subresource'].__setitem__(2, 1)]
        for phase in ('recorded', 'submitted'):
            for mutate in changes:
                data = staged_fixture(); mutate(data[0][0]['source_blit_stages'][phase])
                with self.subTest(phase=phase, mutate=mutate), self.assertRaises(ValueError):
                    self.check(data)

    def test_saved_submission_namespaces_and_camera_counter_remain_distinct(self):
        for mutate in (lambda s: s.update(history_submission=40),
                       lambda s: s.update(capture_submission=70),
                       lambda s: s.update(history_submission_known=False, history_submission=0),
                       lambda s: s.update(capture_submission_known=False, capture_submission=0)):
            data = staged_fixture(); mutate(data[0][0]['source_blit_stages']['submitted'])
            with self.assertRaises(ValueError): self.check(data)
        data = staged_fixture()
        for value in (data[0][0]['source_blit'], data[3][0]['source_blit'],
                      data[0][0]['source_blit_stages']['submitted']):
            value['history_submission'] = 40
        self.assertEqual(self.check(data)['exact_original_blit_receipts'], 1)

    def test_more_than_32_unselected_hook_rows_do_not_replace_delivered_saved_values(self):
        # This models a capped-log evidence gap only. Native ownership tests
        # separately exercise real private-slot churn and fence lifetimes.
        data = staged_fixture()
        old_hook, old_submit = deepcopy(data[2][0]), deepcopy(data[3][0])
        data[2].clear(); data[3].clear()
        for sequence in range(100, 132):
            hook, submit = deepcopy(old_hook), deepcopy(old_submit)
            hook['sequence'] = submit['sequence'] = sequence
            data[2].append(hook); data[3].append(submit)
        # A subsequent selected capture has no spare global hook row.
        data[0][0]['delivery_present'] = 99
        result = self.check(data)
        row = result['receipts'][0]
        self.assertEqual(row['capture_sequence'], 8)
        self.assertEqual(row['source_present'], 3)
        self.assertTrue(row['delivered_record_and_submit_snapshots_available'])
        self.assertFalse(row['record_and_submit_snapshots_independently_available'])
        self.assertEqual(result['original_application_blit_calls'], 1)
        data[0][0]['source_blit_stages']['recorded']['original']['recording'] = 9999
        with self.assertRaises(ValueError): self.check(data)

    def test_missing_secondary_hooks_still_require_original_app_arguments(self):
        data = staged_fixture(); data[2].clear(); data[3].clear()
        self.assertEqual(self.check(data)['exact_original_blit_receipts'], 1)
        for mutate in (lambda d: d[1].clear(),
                       lambda d: d[1][0].update(original_submit_result=-4),
                       lambda d: d[1][0].update(source_image_handle=1002),
                       lambda d: d[0][0]['camera_provenance']['copy_submit'].update(last_submission=70)):
            changed = deepcopy(data); mutate(changed)
            with self.assertRaises(ValueError): self.check(changed)

    def test_available_secondary_hooks_are_compared_even_with_saved_values(self):
        for phase, index in (('recorded', 2), ('submitted', 3)):
            data = staged_fixture()
            data[index][0]['source_blit']['original']['recording'] = 9999
            with self.subTest(phase=phase), self.assertRaises(ValueError): self.check(data)

    def test_generic_history_unknown_keeps_real_lifecycle_without_promoting_identity(self):
        data = staged_fixture()
        values = (data[0][0]['source_blit'], data[0][0]['source_blit_stages']['recorded'],
                  data[0][0]['source_blit_stages']['submitted'])
        for value in values:
            value.update(identity_known=False, bridge_known=False,
                         history_submission=0, history_submission_known=False)
            value['original'] = dict(dict.fromkeys(audit.REF_FIELDS, 0), event_retained=False)
        data[2].clear(); data[3].clear()
        result = self.check(data, require_bridge=False)
        self.assertEqual(result['exact_original_blit_receipts'], 0)
        self.assertEqual(result['generic_completed_capture_receipts'], 1)
        self.assertEqual(result['delivered_lifecycle_snapshot_pairs'], 1)
        with self.assertRaises(ValueError): self.check(data)

    def test_saved_stage_scene_query_or_xr_claims_remain_refused(self):
        for phase in ('recorded', 'submitted'):
            for key in audit.NO_INFERENCE:
                data = staged_fixture(); data[0][0]['source_blit_stages'][phase][key] = True
                with self.subTest(phase=phase, key=key), self.assertRaises(ValueError): self.check(data)

    def test_directory_requires_saved_values_without_global_rows_and_keeps_pixel_check(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            receipts, originals, _, _ = staged_fixture()
            receipt = receipts[0]; receipt['sequence'] = receipt.pop('capture_sequence')
            (directory/'vulkan-123.jsonl').write_text('')
            (directory/'blit-fixture.jsonl').write_text('\n'.join(json.dumps(row) for row in originals)+'\n')
            (directory/'run.log').write_text('Owned fixture completed\n')
            (directory/'image-123-1-8.json').write_text(json.dumps(receipt))
            (directory/'image-123-1-8.pixels').write_bytes(b'old-pixels')
            (directory/'source-frame-2.pixels').write_bytes(b'old-pixels')
            result = audit.audit_directory(directory, require_stages=True)
            self.assertEqual(result['delivered_lifecycle_snapshot_pairs'], 1)
            self.assertEqual(result['original_pixel_bytes_compared'], 10)
            (directory/'source-frame-2.pixels').write_bytes(b'new-pixels')
            with self.assertRaises(ValueError): audit.audit_directory(directory, require_stages=True)


if __name__ == '__main__':
    unittest.main()
