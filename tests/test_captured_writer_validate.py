"""Falsify selected CPU writer-prefix values without a GPU or subprocess."""
from copy import deepcopy
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import captured_writer_validate as audit
from test_blit_provenance_validate import staged_fixture


def event(ordinal, kind, image=11, destination=0, pipeline=0):
    return {'ordinal': ordinal, 'kind': kind, 'image': image, 'destination_image': destination,
            'original_pipeline_id': pipeline, 'selected_pipeline_id': 91 if pipeline else 0,
            'pipeline_substituted': bool(pipeline), 'selected_pipeline_metadata_known': False,
            'selected_pipeline_lifetime_known': False, 'actual_gpu_write_proven': False}


def prefix():
    declaration = event(2, 'declared_color_attachment')
    declaration.update(view=31, area=[0, 0, 8, 8], load_op=1, store_op=0,
                       attachment_index=0, associated_image=13, associated_view=33,
                       null_attachment=False, declared_attachment_use=True)
    draw = event(3, 'draw', image=13, pipeline=81)
    draw.update(rendering_scope_ordinal=2, draw_color_images=[11], draw_color_views=[31],
                declared_color_membership_only=True)
    second = deepcopy(draw); second['ordinal'] = 4
    command = {'recording': 21, 'batch': 0, 'command': 0,
               'known': True, 'truncated': False, 'events': [declaration, draw, second]}
    value = {'schema_version': 1, 'observation_only': True, 'capture_sequence': 8,
             'target_blit': dict(zip(audit.REF_FIELDS, (99, 11, 12, 21, 7))),
             'available': True, 'frozen': True, 'unavailable_reason': 'none',
             'ambiguous': False, 'matching_original_occurrences': 1,
             'target_submission': 70, 'target_batch': 0, 'target_command': 0,
             'cutoff_ordinal': 7, 'cutoff_exclusive': True,
             'original_submit_complete': True, 'original_submit_result': 0,
             'missing_before_admission': True, 'prefix_missing': False, 'truncated': False,
             'event_count': 3,
             'retention': 'immutable_cpu_values', 'event_nodes_held': 0,
             'snapshot_event_references_held': 0,
             'limits': {'events_per_prefix': 64, 'occurrences_per_prefix': 4, 'serialized_bytes': 131072},
             'admission_start': {'recording': 20, 'ordinal': 7, 'before_observed_submission': 69},
             'occurrences': [{'submission': 70, 'capture_submission': 40, 'truncated': False,
                              'commands': [command]}],
             'cpu_completions': [{'submission': 70, 'complete': True, 'result': 0}],
             'images': [{'id': 11, 'known': True}, {'id': 12, 'known': True}, {'id': 13, 'known': True}],
             'catalog': [{'id': 81, 'original_metadata_only': True, 'missing_libraries': False,
                          'metadata_truncated': False,
                          'shaders': [{'stage': stage, 'fnv': fnv, 'bytes': 8, 'known': True,
                                       'identifier': '', 'identifier_truncated': False}
                                      for stage, fnv in ((1, 123), (16, 456))]}]}
    value.update({key: False for key in audit.NO_INFERENCE})
    return value


def fixture():
    receipt = staged_fixture()[0][0]
    receipt['captured_writer_prefix'] = prefix()
    return receipt


class CapturedWriterAudit(unittest.TestCase):
    def check(self, receipt=None, **kwargs):
        with patch('subprocess.Popen', side_effect=AssertionError('Read-only CPU evidence only')):
            return audit.audit_receipts([receipt or fixture()], **kwargs)

    def test_exact_selected_capture_prefix_keeps_separate_original_cpu_after(self):
        result = self.check()
        self.assertEqual(result['available_prefixes'], 1)
        self.assertEqual(result['event_references'], 3)
        row = result['receipts'][0]
        self.assertEqual(row['draw_declarations'], 2)
        self.assertTrue(row['unique_successful_original_cpu_after'])
        self.assertFalse(result['last_gpu_writer_known'])

    def test_target_namespace_twins_command_generation_and_cutoff_are_exact(self):
        for key in audit.REF_FIELDS:
            changed = fixture(); changed['captured_writer_prefix']['target_blit'][key] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): self.check(changed)
        for key in ('capture_sequence', 'target_submission', 'cutoff_ordinal'):
            changed = fixture(); changed['captured_writer_prefix'][key] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): self.check(changed)

    def test_exclusive_cutoff_and_target_command_position_reject_later_work(self):
        changes = [lambda p: p.update(cutoff_exclusive=False),
                   lambda p: p['occurrences'][0]['commands'][0]['events'][0].update(ordinal=7),
                   lambda p: p['occurrences'][0]['commands'][0].update(recording=22),
                   lambda p: p['occurrences'][0]['commands'][0].update(command=1),
                   lambda p: p['occurrences'][0].update(submission=71)]
        for mutate in changes:
            changed = fixture(); mutate(changed['captured_writer_prefix'])
            with self.subTest(mutate=mutate), self.assertRaises(ValueError): self.check(changed)

    def test_complete_original_submission_is_not_camera_or_current_history_counter(self):
        changes = [lambda p: p.update(target_submission=40),
                   lambda p: p['occurrences'][0].update(capture_submission=70),
                   lambda p: p.update(original_submit_complete=False),
                   lambda p: p.update(original_submit_result=-4),
                   lambda p: p['cpu_completions'][0].update(submission=69),
                   lambda p: p['cpu_completions'][0].update(result=-4)]
        for mutate in changes:
            changed = fixture(); mutate(changed['captured_writer_prefix'])
            with self.subTest(mutate=mutate), self.assertRaises(ValueError): self.check(changed)

    def test_before_body_has_no_later_after_mutation_and_prior_failed_returns_stay_separate(self):
        changed = fixture()
        changed['captured_writer_prefix']['occurrences'][0].update(complete=True, result=0)
        with self.assertRaises(ValueError): self.check(changed)
        changed = fixture(); p = changed['captured_writer_prefix']
        p['occurrences'].insert(0, {'submission': 69, 'capture_submission': 39, 'truncated': False, 'commands': []})
        p['cpu_completions'].insert(0, {'submission': 69, 'complete': True, 'result': -4})
        self.assertEqual(self.check(changed)['receipts'][0]['cpu_occurrences'], 2)
        p['cpu_completions'][0].update(complete=False)
        with self.assertRaises(ValueError): self.check(changed)

    def test_resubmission_ambiguity_cannot_reuse_first_body_as_unique_success(self):
        for mutate in (lambda p: p.update(matching_original_occurrences=2),
                       lambda p: p.update(ambiguous=True),
                       lambda p: p.update(ambiguous=True, matching_original_occurrences=2)):
            changed = fixture(); mutate(changed['captured_writer_prefix'])
            with self.assertRaises(ValueError): self.check(changed)
        changed = fixture(); changed['captured_writer_prefix'].update(ambiguous=True, matching_original_occurrences=2)
        row = audit.audit_prefix(changed['captured_writer_prefix'], original=changed['source_blit']['original'],
                                 capture_sequence=8, require_complete=False)
        self.assertFalse(row['unique_successful_original_cpu_after'])

    def test_missing_lifetime_history_and_admission_position_remain_explicit(self):
        for mutate in (lambda p: p.update(missing_before_admission=False),
                       lambda p: p.update(truncated=True),
                       lambda p: p['admission_start'].update(before_observed_submission=70),
                       lambda p: p['admission_start'].update(recording=21, ordinal=8)):
            changed = fixture(); mutate(changed['captured_writer_prefix'])
            with self.assertRaises(ValueError): self.check(changed)
        changed = fixture(); changed['captured_writer_prefix'].update(prefix_missing=True, truncated=True)
        self.assertTrue(self.check(changed)['receipts'][0]['prefix_missing'])

    def test_draw_source_membership_is_explicit_instead_of_depth_or_scope_proximity(self):
        for mutate in (lambda e: e.update(draw_color_images=[14]),
                       lambda e: e.update(draw_color_views=[]),
                       lambda e: e.update(rendering_scope_ordinal=3),
                       lambda e: e.update(declared_color_membership_only=False),
                       lambda e: e.update(draw_color_images=[11]*9, draw_color_views=[31]*9),
                       lambda e: e.pop('rendering_scope_ordinal')):
            changed = fixture(); mutate(changed['captured_writer_prefix']['occurrences'][0]['commands'][0]['events'][1])
            with self.subTest(mutate=mutate), self.assertRaises(ValueError): self.check(changed)

    def test_incoming_copy_and_blit_directions_are_not_outgoing_source_reads(self):
        for kind in ('copy_image', 'blit_image'):
            changed = fixture(); p = changed['captured_writer_prefix']
            incoming = event(5, kind, image=14, destination=11)
            p['occurrences'][0]['commands'][0]['events'].append(incoming); p['event_count'] += 1
            self.assertEqual(self.check(changed)['receipts'][0]['incoming_transfer_declarations'], 1)
            incoming.update(image=11, destination_image=14)
            with self.assertRaises(ValueError): self.check(changed)

    def test_unrelated_twins_and_missing_pipeline_catalog_cannot_be_target_writers(self):
        for mutate in (lambda p: p['occurrences'][0]['commands'][0]['events'][0].update(image=14),
                       lambda p: p.update(catalog=[]),
                       lambda p: p['catalog'][0].update(original_metadata_only=False),
                       lambda p: p['occurrences'][0]['commands'][0]['events'][1].update(selected_pipeline_metadata_known=True),
                       lambda p: p.update(images=[{'id': 12}])):
            changed = fixture(); mutate(changed['captured_writer_prefix'])
            with self.subTest(mutate=mutate), self.assertRaises(ValueError): self.check(changed)

    def test_bounds_counts_duplicate_occurrences_and_boolean_integers_are_refused(self):
        changes = [lambda p: p.update(event_count=4),
                   lambda p: p['limits'].update(events_per_prefix=65),
                   lambda p: p['limits'].update(occurrences_per_prefix=5),
                   lambda p: p['limits'].update(serialized_bytes=131073),
                   lambda p: p['limits'].update(serialized_bytes=100),
                   lambda p: p.update(schema_version=True),
                   lambda p: p.update(target_submission=True),
                   lambda p: p['occurrences'].append(deepcopy(p['occurrences'][0])),
                   lambda p: p['cpu_completions'].append(deepcopy(p['cpu_completions'][0]))]
        for mutate in changes:
            changed = fixture(); mutate(changed['captured_writer_prefix'])
            with self.subTest(mutate=mutate), self.assertRaises(ValueError): self.check(changed)

    def test_unavailable_explicit_binding_and_default_absence_are_not_retagged(self):
        changed = fixture(); actual = changed['captured_writer_prefix']['target_blit']
        changed['captured_writer_prefix'] = {'schema_version': 1, 'observation_only': True,
            'capture_sequence': 8, 'target_blit': actual, 'available': False,
            'frozen': False, 'unavailable_reason': 'ticket_limit',
            'gpu_execution_order_known': False, 'last_gpu_writer_known': False}
        self.assertFalse(self.check(changed, require_available=False)['receipts'][0]['available'])
        with self.assertRaises(ValueError): self.check(changed)
        changed['captured_writer_prefix'].update(capture_sequence=0, unavailable_reason='absent',
                                                 target_blit=dict.fromkeys(audit.REF_FIELDS, 0))
        row = self.check(changed, require_available=False)['receipts'][0]
        self.assertFalse(row['bound_to_capture'])
        changed['captured_writer_prefix']['capture_sequence'] = 9
        with self.assertRaises(ValueError): self.check(changed, require_available=False)

    def test_shortened_serialization_fault_stays_unknown_and_live_journal_retention_is_refused(self):
        changed = fixture()
        changed['captured_writer_prefix'] = {'schema_version': 1, 'available': False, 'frozen': False,
                                             'unavailable_reason': 'observer_fault'}
        row = self.check(changed, require_available=False)['receipts'][0]
        self.assertTrue(row['identity_unavailable_after_serialization_fault'])
        self.assertFalse(row['bound_to_capture'])
        with self.assertRaises(ValueError): self.check(changed)
        for mutate in (lambda p: p.update(retention='live_resource_lookup'),
                       lambda p: p.update(event_nodes_held=1),
                       lambda p: p.update(snapshot_event_references_held=1)):
            changed = fixture(); mutate(changed['captured_writer_prefix'])
            with self.assertRaises(ValueError): self.check(changed)

    def test_nested_gpu_read_scene_alias_or_initialization_claim_is_never_accepted(self):
        for key in audit.NO_INFERENCE:
            changed = fixture()
            changed['captured_writer_prefix']['occurrences'][0]['commands'][0]['events'][0][key] = True
            with self.subTest(key=key), self.assertRaises(ValueError): self.check(changed)

    def test_delivered_copy_is_not_reconstructed_after_mutation_or_reuse(self):
        changed = fixture(); old = deepcopy(changed)
        changed['captured_writer_prefix']['target_blit'].update(recording=22)
        changed['captured_writer_prefix']['catalog'][0]['shaders'][0].update(fnv=999)
        self.assertEqual(self.check(old)['available_prefixes'], 1)
        with self.assertRaises(ValueError): self.check(changed)

    def test_owned_original_draws_scope_attachment_and_shader_controls_are_independent(self):
        original = {'writer_pipeline_handle': 5001, 'writer_view_handle': 6001,
                    'writer_draw_calls': 2, 'writer_color_attachment_count': 1,
                    'writer_load_op': 1, 'writer_store_op': 0, 'writer_render_area': [0, 0, 8, 8]}
        shaders = {1: {'fnv': 123, 'bytes': 8}, 16: {'fnv': 456, 'bytes': 8}}
        self.assertEqual(audit.owned_writer(prefix(), original, shaders)['original_draw_calls_compared'], 2)
        changes = [lambda p: p['occurrences'][0]['commands'][0]['events'].pop(),
                   lambda p: p['occurrences'][0]['commands'][0]['events'][0].update(view=32),
                   lambda p: p['occurrences'][0]['commands'][0]['events'][0].update(area=[0, 0, 9, 8]),
                   lambda p: p['occurrences'][0]['commands'][0]['events'][0].update(load_op=0),
                   lambda p: p['occurrences'][0]['commands'][0]['events'][1].update(draw_color_images=[14]),
                   lambda p: p['catalog'][0]['shaders'][0].update(fnv=999),
                   lambda p: p['catalog'][0]['shaders'][1].update(bytes=9)]
        for mutate in changes:
            changed = prefix(); mutate(changed)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError): audit.owned_writer(changed, original, shaders)
        # Raw handles do not need to numerically equal CPU catalog/view labels.
        original.update(writer_pipeline_handle=9991, writer_view_handle=9992)
        self.assertFalse(audit.owned_writer(prefix(), original, shaders)['raw_pipeline_or_view_handle_used_as_catalog_label'])

    def test_owned_shader_hashes_are_read_from_exact_spirv_bytes_without_execution(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            raw = b'\x03\x02\x23\x07'+bytes(16)
            for suffix in ('vert', 'frag'):
                (directory/('present.'+suffix+'.spv')).write_bytes(raw)
            with patch('subprocess.Popen', side_effect=AssertionError('No compiler or GPU')):
                result = audit.shader_references(directory)
            self.assertEqual(result[1]['bytes'], 20)
            self.assertEqual(result[1], result[16])
            (directory/'present.vert.spv').write_bytes(b'not-spirv')
            with self.assertRaises(ValueError): audit.shader_references(directory)


if __name__ == '__main__':
    unittest.main()
