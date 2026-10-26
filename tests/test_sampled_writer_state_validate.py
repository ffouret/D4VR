"""Independent CPU-only falsifications of immutable selected-draw state."""
from copy import deepcopy
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import sampled_writer_state_validate as audit
from test_captured_writer_validate import fixture as prefix_fixture


def owned(frame=0, draw=0, count=True, maintenance=True):
    floats = (-.35, 0, 2, .3, 0, 1, 0, 1) if draw == 0 else (.6, .3, 4, .4, 1, 0, 0, 1)
    raw = struct.pack('<8f', *floats)
    mode = 'static' if draw == 0 else 'with_count' if count and frame % 2 else 'dynamic'
    return {'schema_version': 1, 'frame': frame, 'draw_index': draw, 'command_handle': 101,
            'pipeline_handle': 102+draw, 'bound_layout_handle': 104, 'push_layout_handle': 105,
            'layout_ranges': [[17, 0, 32]], 'mode': mode,
            'viewport_command': 'pipeline_static' if mode == 'static' else 'vkCmdSetViewport' if mode == 'dynamic'
                                else 'vkCmdSetViewportWithCount'+('EXT' if frame % 4 == 3 else ''),
            'scissor_command': 'pipeline_static' if mode == 'static' else 'vkCmdSetScissor' if mode == 'dynamic'
                               else 'vkCmdSetScissorWithCount'+('EXT' if frame % 4 == 3 else ''),
            'push_command': 'vkCmdPushConstants2KHR' if maintenance and frame % 2 else 'vkCmdPushConstants',
            'fragment_push_hex': raw.hex(),
            'push_updates': [{'stage_flags': 17, 'offset': offset, 'size': size, 'hex': raw[offset:offset+size].hex()}
                             for offset, size in ((0, 32), (8, 4), (24, 4))],
            'viewport_bits': list(struct.unpack('<6I', struct.pack('<6f', 0, 0, 256, 256, 0, 1))),
            'scissor_offset': [0, 0], 'scissor_extent': [256, 256],
            'viewport_count': 1, 'scissor_count': 1,
            'draw_arguments': {'count': 3, 'instance_count': 1, 'first': 0, 'first_instance': 0},
            'dynamic_count_enabled': count, 'maintenance6_enabled': maintenance,
            'fragment_shader_reads_push_constants': False, 'push_values_identify_sampled_resource': False}


def state(original=None, ordinal=10):
    original = original or owned()
    words = []
    for index in range(8):
        update_index = 1 if index == 2 else 2 if index == 6 else 0
        update = original['push_updates'][update_index]
        words.append({'word': index, 'known': True, 'compatible': True, 'ordinal': ordinal-3+update_index,
                      'stage_flags': update['stage_flags'], 'offset': update['offset'], 'size': update['size'],
                      'layout': {'generation': 12, 'raw_handle': 105, 'flags': 0, 'ranges_complete': True,
                                 'ranges': [{'stage_flags': 17, 'offset': 0, 'size': 32}], 'chain_types': []}})
    mode = original['mode']
    def rectangle(viewport):
        setter = ordinal-(5 if viewport else 4) if mode != 'static' else 0
        value = {'mode': mode, 'value_known': True, 'effective_known': True, 'count_known': True, 'count': 1,
                 'declared_first': 0, 'declared_count': 1, 'setter_ordinal': setter,
                 'count_ordinal': setter if mode == 'with_count' else 0, 'epoch': 2,
                 'pipeline_generation': 15, 'last_first': 0, 'last_count': 1 if setter else 0,
                 'last_ordinal': setter, 'conventional_valid': True}
        if viewport:
            value.update(bits=original['viewport_bits'], finite=True)
        else:
            value.update(offset=original['scissor_offset'], extent=original['scissor_extent'])
        return value
    raw = bytes.fromhex(original['fragment_push_hex'])
    result = {'schema_version': 1, 'available': True, 'ordinal': ordinal, 'pipeline_generation': 15,
              'pipeline_flags': 0, 'pipeline_unknown_flags': 0, 'pipeline_declarations_complete': True,
              'overflow': False, 'push': {'raw_hex': raw.hex(), 'byte_known_mask': 0xffffffff,
                'compatible_word_mask': 255, 'host_little_endian': True, 'bound_layout_generation': 11,
                'bound_layout': {'generation': 11, 'raw_handle': 104, 'flags': 0, 'ranges_complete': True,
                                 'ranges': [{'stage_flags': 17, 'offset': 0, 'size': 32}], 'chain_types': []},
                'layout_known': True, 'image_index_known': True, 'image_index': int.from_bytes(raw[8:12], 'little'),
                'sampler_index_known': True, 'sampler_index': int.from_bytes(raw[24:28], 'little'), 'words': words},
              'viewport': rectangle(True), 'scissor': rectangle(False),
              'arguments': {'kind': 'direct', 'command_kind': 0, 'count': 3, 'instance_count': 1,
                            'first': 0, 'first_instance': 0, 'vertex_offset': 0}}
    result.update({key: False for key in ('effective_raster_state_known', 'descriptor_placement_known',
                                         'sampled_resource_known', 'coverage_known')})
    return result


def delivered(frame=0, enabled=True):
    receipt = prefix_fixture()
    receipt['source_present'] = frame+1
    prefix = receipt['captured_writer_prefix']
    prefix['target_blit']['ordinal'] = prefix['cutoff_ordinal'] = 30
    receipt['source_blit']['original']['ordinal'] = 30
    for name in ('recorded', 'submitted'):
        receipt['source_blit_stages'][name]['original']['ordinal'] = 30
    draws = [event for event in prefix['occurrences'][0]['commands'][0]['events'] if event['kind'] == 'draw']
    controls = {}
    for index, draw in enumerate(draws):
        draw['ordinal'] = 10+index*10
        original = owned(frame, index);controls[(frame, index)] = original
        if enabled:
            draw['writer_state'] = state(original, draw['ordinal'])
            draw['writer_state_original_pipeline_handle'] = original['pipeline_handle']
    return receipt, controls


class SampledWriterStateAudit(unittest.TestCase):
    def check(self, value=None, original=None, ordinal=10):
        with patch('subprocess.Popen', side_effect=AssertionError('Read-only CPU audit cannot execute a process')):
            return audit.audit_state(value or state(original), original=original or owned(), ordinal=ordinal)

    def test_exact_static_partial_push_values_and_distinct_compatible_layouts(self):
        result = self.check()
        self.assertEqual(result['known_push_bytes'], 32)
        self.assertEqual(result['compatible_push_words'], 8)
        self.assertEqual(result['viewport_mode'], 'static')
        self.assertFalse(result['sampled_resource_known'])

    def test_regular_and_core_ext_count_aliases_with_explicit_features(self):
        for frame in (0, 1, 3):
            original = owned(frame, 1)
            self.assertEqual(self.check(state(original), original)['viewport_mode'], original['mode'])
        for count, maintenance in ((False, False), (True, False), (False, True)):
            original = owned(1, 1, count, maintenance)
            self.check(state(original), original)

    def test_all_saved_draws_bind_exact_lease_source_and_original_pipeline(self):
        receipt, controls = delivered(3)
        result = audit.audit_receipt(receipt, controls)
        self.assertEqual(result['draw_states_compared'], 2)
        self.assertEqual(result['source_present'], 4)
        for change in (lambda r: r.update(source_present=5),
                       lambda r: r['captured_writer_prefix']['target_blit'].update(source_image=19),
                       lambda r: r['captured_writer_prefix']['occurrences'][0]['commands'][0]['events'][1].update(writer_state_original_pipeline_handle=999)):
            value = deepcopy(receipt);change(value)
            with self.assertRaises((ValueError, KeyError)): audit.audit_receipt(value, controls)

    def test_disabled_legacy_draw_event_format_remains_unchanged(self):
        receipt, controls = delivered(enabled=False)
        result = audit.audit_receipt(receipt, controls, observer_enabled=False)
        self.assertTrue(result['legacy_draw_events_unchanged'])
        self.assertEqual(result['draw_states_compared'], 0)
        draw = receipt['captured_writer_prefix']['occurrences'][0]['commands'][0]['events'][1]
        for key in ('writer_state', 'writer_state_original_pipeline_handle'):
            changed = deepcopy(receipt)
            changed['captured_writer_prefix']['occurrences'][0]['commands'][0]['events'][1][key] = {} if key == 'writer_state' else 1
            with self.assertRaises(ValueError): audit.audit_receipt(changed, controls, observer_enabled=False)

    def test_push_bytes_masks_decode_and_word_order_falsifications(self):
        changes = (lambda v: v['push'].update(raw_hex='00'*32),
                   lambda v: v['push'].update(byte_known_mask=0xfffffff0),
                   lambda v: v['push'].update(compatible_word_mask=251),
                   lambda v: v['push'].update(image_index=19),
                   lambda v: v['push'].update(sampler_index_known=False),
                   lambda v: v['push']['words'][2].update(word=1),
                   lambda v: v['push']['words'].pop(),
                   lambda v: v['push'].update(host_little_endian=False))
        for change in changes:
            value = state();change(value)
            with self.subTest(change=change), self.assertRaises(ValueError): self.check(value)

    def test_push_original_stage_offset_size_ordinal_and_layout_are_not_current_state(self):
        changes = (lambda w: w.update(ordinal=10), lambda w: w.update(ordinal=6),
                   lambda w: w.update(stage_flags=1), lambda w: w.update(offset=0),
                   lambda w: w.update(size=32), lambda w: w['layout'].update(raw_handle=104),
                   lambda w: w['layout'].update(generation=99),
                   lambda w: w['layout'].update(ranges_complete=False),
                   lambda w: w['layout']['ranges'][0].update(stage_flags=1),
                   lambda w: w['layout'].update(chain_types=[100]))
        for change in changes:
            value = state();change(value['push']['words'][2])
            with self.subTest(change=change), self.assertRaises(ValueError): self.check(value)
        value = state();value['push'].update(bound_layout_generation=12)
        with self.assertRaises(ValueError): self.check(value)
        for change in (lambda l: l.update(raw_handle=105), lambda l: l.update(ranges_complete=False),
                       lambda l: l['ranges'][0].update(stage_flags=16), lambda l: l.update(chain_types=[1])):
            value = state();change(value['push']['bound_layout'])
            with self.assertRaises(ValueError): self.check(value)

    def test_static_pipeline_cannot_claim_a_dynamic_setter_or_other_owner(self):
        for change in (lambda r: r.update(setter_ordinal=1), lambda r: r.update(count_ordinal=1),
                       lambda r: r.update(pipeline_generation=99), lambda r: r.update(mode='dynamic'),
                       lambda r: r.update(value_known=False), lambda r: r.update(effective_known=False)):
            value = state();change(value['viewport'])
            with self.assertRaises(ValueError): self.check(value)

    def test_dynamic_with_count_requires_its_own_original_count_and_index(self):
        original = owned(1, 1)
        for change in (lambda r: r.update(count_ordinal=0), lambda r: r.update(setter_ordinal=9),
                       lambda r: r.update(count=2), lambda r: r.update(count_known=False),
                       lambda r: r.update(declared_first=1), lambda r: r.update(last_ordinal=1),
                       lambda r: r.update(last_first=1), lambda r: r.update(last_count=2)):
            value = state(original);change(value['scissor'])
            with self.assertRaises(ValueError): self.check(value, original)

    def test_viewport_float_bits_and_scissor_signed_offsets_are_exact(self):
        changes = (lambda v: v['viewport']['bits'].__setitem__(0, 0x80000000),
                   lambda v: v['viewport']['bits'].__setitem__(3, 0xc3800000),
                   lambda v: v['viewport']['bits'].__setitem__(1, 0x7fc12345),
                   lambda v: v['viewport'].update(finite=False),
                   lambda v: v['scissor'].update(offset=[-1, 0]),
                   lambda v: v['scissor'].update(extent=[255, 256]))
        for change in changes:
            value = state();change(value)
            with self.assertRaises(ValueError): self.check(value)

    def test_unknown_diagnostic_keeps_zero_bytes_and_null_decode_without_resource_inference(self):
        value = state();push = value['push']
        push.update(raw_hex='00'*32, byte_known_mask=0, compatible_word_mask=0,
                    bound_layout_generation=0, layout_known=False,
                    bound_layout={'generation': 0, 'raw_handle': 0, 'flags': 0, 'ranges_complete': False,
                                  'ranges': [], 'chain_types': []},
                    image_index_known=False, image_index=None, sampler_index_known=False, sampler_index=None)
        for word in push['words']: word.update(known=False, compatible=False)
        result = audit.audit_state(value)
        self.assertEqual(result['known_push_bytes'], 0)
        push['raw_hex'] = '01'+'00'*31
        with self.assertRaises(ValueError): audit.audit_state(value)

    def test_draw_arguments_late_retagging_and_gpu_claims_are_rejected(self):
        for change in (lambda v: v.update(ordinal=11), lambda v: v['arguments'].update(kind='unknown'),
                       lambda v: v['arguments'].update(first=3), lambda v: v['arguments'].update(vertex_offset=-1),
                       lambda v: v.update(overflow=True), lambda v: v.update(available=False),
                       lambda v: v.update(pipeline_flags=1), lambda v: v.update(pipeline_unknown_flags=1),
                       lambda v: v.update(pipeline_declarations_complete=False)):
            value = state();change(value)
            with self.assertRaises(ValueError): self.check(value)
        for name in audit.NO_INFERENCE:
            value = state();value['push'][name] = True
            with self.subTest(name=name), self.assertRaises(ValueError): self.check(value)

    def test_original_controls_need_exact_partial_writes_aliases_and_unused_fragment_claim(self):
        for change in (lambda v: v['push_updates'][1].update(offset=12),
                       lambda v: v['push_updates'][0].update(stage_flags=16),
                       lambda v: v.update(push_command='vkCmdPushConstants2KHR', maintenance6_enabled=False),
                       lambda v: v.update(fragment_shader_reads_push_constants=True),
                       lambda v: v.update(push_values_identify_sampled_resource=True)):
            original = owned();change(original)
            with self.assertRaises(ValueError): audit.golden(original)


if __name__ == '__main__':
    unittest.main()
