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


def fsr_owned(frame=0, draw=0):
    value = owned(frame, draw)
    value.update(pipeline_flags=audit.FSR_ATTACHMENT, attachment_fsr_feature_enabled=True,
                 rate_attachment_present=False, static_rate_pnext_present=False, dynamic_rate_declared=False)
    return value


def facet_state(original=None, ordinal=10):
    original = original or fsr_owned()
    value = state(original, ordinal)
    flags = original.get('pipeline_flags', 0)
    mode = original['mode']
    dynamic = [] if mode == 'static' else list(audit.COUNT_DYNAMIC if mode == 'with_count' else audit.REGULAR_DYNAMIC)
    masks = dict.fromkeys(audit.REASON_GROUPS, 0)
    masks['fragment_shading_rate'] = audit.REASON_BITS['fragment_shading_rate_unobserved']
    if flags & audit.FSR_ATTACHMENT:
        masks['fragment_shading_rate'] |= audit.REASON_BITS['rendering_fsr_attachment']
    value.update(schema_version=2, pipeline_flags=flags, pipeline_unknown_flags=0,
                 pipeline_declarations_complete=not bool(flags),
                 pipeline_declarations_complete_scope='legacy_header_admission_summary',
                 pipeline_recognized_rendering_flags=flags & audit.FSR_ATTACHMENT,
                 pipeline_facet_known=dict.fromkeys(audit.FACETS, True),
                 pipeline_facet_reason_masks=masks,
                 pipeline_facet_reasons={group: [name for name in audit.REASONS if mask & audit.REASON_BITS[name]]
                                        for group, mask in masks.items()},
                 fragment_shading_rate_known=False, effective_write_coverage_known=False, conventional_operand_scope=True,
                 pipeline_creation={'own_parts': 15, 'parts': 15, 'pre_raster_owners': 1, 'fragment_owners': 1,
                    'legacy_flags': flags, 'flag_source': 'legacy', 'inherited_rendering_flags': 0,
                    'inherited_unknown_flags': 0, 'structural_known': True, 'chain_types': [1000044002],
                    'chain_truncated': False, 'chain_duplicate': False, 'unknown_chain_types': [],
                    'unknown_chain_count': 0, 'fsr_static_declared': False, 'fsr_static_declared_scope': 'own_parts'},
                 raster_declarations={'owner_generation': 15, 'dynamic_state_types': dynamic,
                    'scope': 'pre_raster_owner', 'dynamic_info_observed': mode != 'static',
                    'dynamic_info_absent': mode == 'static', 'raster_info_observed': True,
                    'raster_info_absent': False, 'viewport_info_observed': mode != 'with_count',
                    'viewport_info_absent': False,
                    'dynamic_state_count': len(dynamic), 'dynamic_truncated': False,
                    'rejected_dynamic_types': [], 'rejected_dynamic_count': 0,
                    'dynamic_chain_types': [], 'dynamic_chain_truncated': False,
                    'raster_chain_types': [], 'raster_chain_truncated': False,
                    'viewport_chain_types': [], 'viewport_chain_truncated': False,
                    'viewport_mode': mode, 'scissor_mode': mode,
                    'viewport_count': 0 if mode == 'with_count' else 1,
                    'scissor_count': 0 if mode == 'with_count' else 1,
                    'fsr_dynamic_declared': False})
    for key in ('viewport', 'scissor'):
        value[key]['effective_known_scope'] = 'single_conventional_operand'
    return value


def set_reason(value, group, *names):
    mask = sum(audit.REASON_BITS[name] for name in names)
    value['pipeline_facet_reason_masks'][group] = mask
    value['pipeline_facet_reasons'][group] = [name for name in audit.REASONS if mask & audit.REASON_BITS[name]]
    if group in audit.FACETS:
        value['pipeline_facet_known'][group] = mask == 0


def unknown_bound_push(value):
    push = value['push']
    push.update(layout_known=False, bound_layout_generation=0, compatible_word_mask=0,
                bound_layout={'generation': 0, 'raw_handle': 0, 'flags': 0, 'ranges_complete': False,
                              'ranges': [], 'chain_types': []},
                image_index_known=False, image_index=None, sampler_index_known=False, sampler_index=None)
    for word in push['words']: word['compatible'] = False


def unknown_rectangles(value):
    for name in ('viewport', 'scissor'):
        value[name].update(mode='unknown', value_known=False, count_known=False, effective_known=False)


def clip_owned(frame=0, draw=0):
    value = fsr_owned(frame, draw)
    value.update(depth_clip_feature_enabled=True, depth_clip_chain_type=audit.DEPTH_CLIP_TYPE,
                 depth_clip_state_flags=0, depth_clip_enable_payload=True)
    return value


def raw_state(original=None, ordinal=10):
    original = original or clip_owned()
    value = facet_state(original, ordinal)
    value.update(schema_version=3, depth_clip_value_known=False, effective_clipping_known=False,
                 clipping_reasons=['depth_clip_unobserved'], clipping_reason_mask=audit.REASON_BITS['depth_clip_unobserved'])
    clip = 'depth_clip_chain_type' in original
    value['raster_declarations'].update(depth_clip_declared=clip,
        reviewed_depth_clip_nodes=[{'chain_index': 0, 'flags_known': True, 'flags': 0}] if clip else [],
        raster_chain_types=[audit.DEPTH_CLIP_TYPE] if clip else [])
    banks = {}
    for name in ('viewport', 'scissor'):
        interpreted = value[name];dynamic = original['mode'] != 'static'
        origin = interpreted['setter_ordinal'] if dynamic else 0
        form = ('with_count' if original['mode'] == 'with_count' else 'regular') if dynamic else 'unknown'
        first = 0 if form == 'regular' else None
        bank = {'scope': 'index_zero_argument_snapshot', 'value_captured': dynamic, 'origin_ordinal': origin,
                'capture_epoch': interpreted['epoch'] if dynamic else 0, 'recording_epoch': interpreted['epoch'],
                'recording_epoch_matches': dynamic, 'recording_bank_known': dynamic, 'form': form,
                'first': first, 'count': 1 if dynamic else 0,
                'last_observed_call': {'ordinal': origin, 'form': form, 'first': first, 'count': 1 if dynamic else 0,
                                       'payload_captured_for_this_call': dynamic}, 'effective_operand_claimed': False}
        for field in (('bits',) if name == 'viewport' else ('offset', 'extent')):
            bank[field] = deepcopy(interpreted[field]) if dynamic else None
        banks[name] = bank
    value['raw_setter_arguments'] = banks
    return value


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

    def test_schema2_fsr_flag_keeps_three_independent_facets_and_separate_rate_gap(self):
        for original in (fsr_owned(), fsr_owned(0, 1), fsr_owned(1, 1)):
            value = facet_state(original)
            result = self.check(value, original)
            self.assertEqual(result['writer_state_schema_version'], 2)
            self.assertTrue(all(result['pipeline_facet_known'].values()))
            self.assertEqual(result['pipeline_recognized_rendering_flags'], audit.FSR_ATTACHMENT)
            self.assertFalse(result['fragment_shading_rate_known'])
            self.assertFalse(value['pipeline_declarations_complete'])
        original = owned()
        self.check(facet_state(original), original)

    def test_schema2_reason_names_masks_and_known_booleans_cannot_contradict(self):
        changes = (lambda v: v['pipeline_facet_known'].update(push_layout=False),
                   lambda v: v['pipeline_facet_reason_masks'].update(push_layout=1 << 40),
                   lambda v: v['pipeline_facet_reason_masks'].update(push_layout=1),
                   lambda v: v['pipeline_facet_reasons'].update(push_layout=['unknown_push_layout']),
                   lambda v: v['pipeline_facet_reasons']['fragment_shading_rate'].reverse(),
                   lambda v: v['pipeline_facet_known'].update(sampled_resource=True),
                   lambda v: v['pipeline_creation'].update(structural_known=False),
                   lambda v: v.update(pipeline_declarations_complete=True))
        for change in changes:
            value = facet_state();change(value)
            with self.subTest(change=change), self.assertRaises(ValueError): self.check(value, fsr_owned())

    def test_schema2_recognized_fsr_flags_do_not_erase_other_flags_or_structural_gaps(self):
        value = facet_state();value['pipeline_flags'] |= 1 << 63;value['pipeline_unknown_flags'] = 1 << 63
        value['pipeline_creation']['flag_source'] = 'flags2'
        value['pipeline_creation']['chain_types'].append(1000470005)
        value['pipeline_creation']['structural_known'] = False
        for group in ('structure', *audit.FACETS): set_reason(value, group, 'unknown_pipeline_flags')
        unknown_bound_push(value);unknown_rectangles(value)
        audit.audit_state(value)
        for change in (lambda v: v.update(pipeline_unknown_flags=0),
                       lambda v: v.update(pipeline_recognized_rendering_flags=audit.FSR_ATTACHMENT | (1 << 63)),
                       lambda v: set_reason(v, 'push_layout'),
                       lambda v: v['pipeline_facet_known'].update(push_layout=True)):
            changed = deepcopy(value);change(changed)
            with self.assertRaises(ValueError): audit.audit_state(changed)

    def test_schema2_push_can_remain_known_when_an_unreviewed_raster_chain_is_explicit(self):
        value = facet_state();value['raster_declarations']['raster_chain_types'] = [123456]
        for group in ('viewport_declaration', 'scissor_declaration'): set_reason(value, group, 'unknown_raster_chain')
        unknown_rectangles(value)
        result = audit.audit_state(value)
        self.assertTrue(result['pipeline_facet_known']['push_layout'])
        self.assertFalse(result['pipeline_facet_known']['viewport_declaration'])
        changed = deepcopy(value);set_reason(changed, 'viewport_declaration')
        with self.assertRaises(ValueError): audit.audit_state(changed)

    def test_schema2_unknown_push_signature_does_not_erase_observed_conventional_rectangles(self):
        value = facet_state();set_reason(value, 'push_layout', 'unknown_push_layout');unknown_bound_push(value)
        result = audit.audit_state(value)
        self.assertFalse(result['pipeline_facet_known']['push_layout'])
        self.assertTrue(result['pipeline_facet_known']['viewport_declaration'])
        value['push'].update(image_index_known=True, image_index=7)
        with self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema2_chain_dynamic_bounds_unknown_types_flags2_and_gpl_owner_falsifications(self):
        changes = (lambda v: v['pipeline_creation'].update(chain_types=[1000044002]*33),
                   lambda v: v['pipeline_creation'].update(chain_types=[1000044002]*2),
                   lambda v: v['pipeline_creation'].update(chain_types=[123456], unknown_chain_types=[], unknown_chain_count=0),
                   lambda v: v['pipeline_creation'].update(flag_source='flags2'),
                   lambda v: v['pipeline_creation'].update(pre_raster_owners=2),
                   lambda v: v['pipeline_creation'].update(unknown_chain_types=[1]*9),
                   lambda v: v['raster_declarations'].update(dynamic_state_types=[0]*65),
                   lambda v: v['raster_declarations'].update(dynamic_state_count=65),
                   lambda v: v['raster_declarations'].update(rejected_dynamic_types=[123], rejected_dynamic_count=1),
                   lambda v: v['raster_declarations'].update(viewport_mode='unknown'))
        for change in changes:
            value = facet_state();change(value)
            with self.subTest(change=change), self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema2_fsr_static_and_dynamic_declarations_never_establish_rate_values(self):
        value = facet_state();value['pipeline_creation']['chain_types'].append(audit.FSR_STATIC_TYPE)
        value['pipeline_creation']['fsr_static_declared'] = True
        set_reason(value, 'fragment_shading_rate', 'fragment_shading_rate_unobserved', 'rendering_fsr_attachment', 'fsr_static_declaration')
        audit.audit_state(value)
        changed = deepcopy(value);changed['pipeline_creation']['fsr_static_declared_scope'] = 'all_linked_payloads'
        with self.assertRaises(ValueError): audit.audit_state(changed)
        original = fsr_owned(0, 1);value = facet_state(original)
        value['raster_declarations']['dynamic_state_types'].append(audit.FSR_DYNAMIC_TYPE)
        value['raster_declarations'].update(dynamic_state_count=3, fsr_dynamic_declared=True)
        set_reason(value, 'fragment_shading_rate', 'fragment_shading_rate_unobserved', 'rendering_fsr_attachment', 'fsr_dynamic_declaration')
        audit.audit_state(value)
        for key in ('fragment_shading_rate_known', 'effective_write_coverage_known'):
            changed = deepcopy(value);changed[key] = True
            with self.assertRaises(ValueError): audit.audit_state(changed)
        set_reason(value, 'fragment_shading_rate', 'rendering_fsr_attachment', 'fsr_dynamic_declaration')
        with self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema1_old_receipt_cannot_be_reinterpreted_as_newer_facet_proof(self):
        old = state();self.check(old)
        upgraded = facet_state();upgraded['schema_version'] = 1
        with self.assertRaises(ValueError): audit.audit_state(upgraded)
        old['pipeline_flags'] = old['pipeline_unknown_flags'] = audit.FSR_ATTACHMENT
        old['pipeline_declarations_complete'] = False
        with self.assertRaises(ValueError): audit.audit_state(old)
        unknown_bound_push(old);unknown_rectangles(old)
        audit.audit_state(old)
        old['schema_version'] = 2
        with self.assertRaises((KeyError, ValueError)): audit.audit_state(old)

    def test_schema2_conventional_scope_and_original_fsr_feature_configuration_are_explicit(self):
        for change in (lambda v: v.update(conventional_operand_scope=False),
                       lambda v: v.update(pipeline_declarations_complete_scope='whole_pipeline'),
                       lambda v: v['viewport'].update(effective_known_scope='coverage')):
            value = facet_state();change(value)
            with self.assertRaises(ValueError): audit.audit_state(value)
        for change in (lambda v: v.update(attachment_fsr_feature_enabled=False),
                       lambda v: v.update(rate_attachment_present=True),
                       lambda v: v.update(pipeline_flags=audit.FSR_ATTACHMENT | 1),
                       lambda v: v.update(dynamic_rate_declared=True)):
            original = fsr_owned();change(original)
            with self.assertRaises(ValueError): audit.golden(original)

    def test_schema2_structure_absence_is_not_inferred_from_empty_diagnostic_arrays(self):
        value = facet_state()
        for change in (lambda r: r.update(dynamic_info_observed=True),
                       lambda r: r.update(raster_info_absent=True),
                       lambda r: r.update(viewport_info_absent=True),
                       lambda r: r.update(raster_info_observed=False),
                       lambda r: r.update(viewport_info_observed=False),
                       lambda r: r.update(owner_generation=99),
                       lambda r: r.update(scope='link_only_ignored_pointer')):
            changed = deepcopy(value);change(changed['raster_declarations'])
            with self.assertRaises(ValueError): self.check(changed, fsr_owned())
        original = fsr_owned(1, 1);value = facet_state(original)
        self.check(value, original)
        value['raster_declarations']['viewport_info_absent'] = True
        with self.assertRaises(ValueError): self.check(value, original)

    def test_schema2_selected_prefix_requires_new_state_without_retagging_old_receipts(self):
        receipt, controls = delivered(1)
        for index, draw in enumerate(receipt['captured_writer_prefix']['occurrences'][0]['commands'][0]['events'][1:]):
            original = fsr_owned(1, index);controls[(1, index)] = original
            draw['writer_state'] = facet_state(original, draw['ordinal'])
        result = audit.audit_receipt(receipt, controls, require_facets=True)
        self.assertEqual(result['draw_states_compared'], 2)
        self.assertTrue(all(row['writer_state_schema_version'] == 2 for row in result['states']))
        old, old_controls = delivered(1)
        with self.assertRaises(ValueError): audit.audit_receipt(old, old_controls, require_facets=True)

    def test_generic_known_push_needs_aligned_complete_write_and_declared_stage_coverage(self):
        for offset, size, stages in ((7, 28, 17), (8, 1, 17), (8, 0, 17),
                                     (8, audit.UINT32, 17), (8, 4, 49)):
            value = facet_state();value['push']['words'][2].update(offset=offset, size=size, stage_flags=stages)
            with self.subTest(offset=offset, size=size, stages=stages), self.assertRaises(ValueError):
                audit.audit_state(value)
        for ranges in ([], [{'stage_flags': 1, 'offset': 0, 'size': 32}],
                       [{'stage_flags': 17, 'offset': 12, 'size': 20}]):
            value = facet_state()
            value['push']['bound_layout']['ranges'] = deepcopy(ranges)
            for word in value['push']['words']: word['layout']['ranges'] = deepcopy(ranges)
            with self.subTest(ranges=ranges), self.assertRaises(ValueError): audit.audit_state(value)
        audit.audit_state(facet_state())

    def test_generic_conventional_validity_is_recomputed_from_exact_operand_values(self):
        changes = (lambda v: v['viewport'].update(bits=[0, 0, 0x7fc12345, 0x43800000, 0, 0x3f800000], finite=False),
                   lambda v: v['viewport']['bits'].__setitem__(2, 0xc3800000),
                   lambda v: v['viewport']['bits'].__setitem__(3, 0x80000000),
                   lambda v: v['scissor'].update(offset=[-1, 0]),
                   lambda v: v['scissor'].update(offset=[0x7fffffff, 0], extent=[1, 256]))
        for change in changes:
            value = facet_state();change(value)
            with self.subTest(change=change), self.assertRaises(ValueError): audit.audit_state(value)
            field = 'viewport' if not value['viewport']['finite'] or value['viewport']['bits'] != facet_state()['viewport']['bits'] else 'scissor'
            value[field].update(conventional_valid=False, effective_known=False)
            audit.audit_state(value)
        value = facet_state();value['viewport']['bits'][3] = 0xc3800000
        audit.audit_state(value)  # Negative viewport height remains conventional.
        value = facet_state()
        value['viewport'].update(value_known=False, effective_known=False, bits=[0]*6,
                                 finite=False, conventional_valid=False)
        audit.audit_state(value)  # Unknown placeholders are not decoded as known operands.

    def test_unobserved_headers_cannot_retain_interpreted_arrays_or_unexplained_subchains(self):
        for info in ('dynamic', 'raster', 'viewport'):
            value = facet_state(fsr_owned(0, 1));raster = value['raster_declarations']
            raster[info+'_chain_types'] = [123456]
            for group in ('viewport_declaration', 'scissor_declaration'):
                set_reason(value, group, 'unknown_'+info+'_chain')
            unknown_rectangles(value);audit.audit_state(value)
            raster[info+'_info_observed'] = False
            with self.subTest(info=info), self.assertRaises(ValueError): audit.audit_state(value)
        value = facet_state(fsr_owned(0, 1));raster = value['raster_declarations']
        raster.update(dynamic_info_observed=False, dynamic_state_types=[123456], dynamic_state_count=1,
                      rejected_dynamic_types=[123456], rejected_dynamic_count=1)
        for group in ('viewport_declaration', 'scissor_declaration'):
            set_reason(value, group, 'invalid_dynamic_info', 'unsupported_dynamic_state')
        unknown_rectangles(value)
        with self.assertRaises(ValueError): audit.audit_state(value)

    def test_refused_raw_header_diagnostics_keep_explicit_invalid_reasons_and_no_interpretation(self):
        for info, invalid in (('dynamic', 'invalid_dynamic_info'), ('raster', 'invalid_raster_state'),
                              ('viewport', 'invalid_viewport_state')):
            value = facet_state();raster = value['raster_declarations']
            raster.update({info+'_info_observed': False, info+'_info_absent': False,
                           info+'_chain_types': [123456]})
            if info == 'dynamic': raster['dynamic_state_count'] = 4
            for group in ('viewport_declaration', 'scissor_declaration'):
                set_reason(value, group, invalid, 'unknown_'+info+'_chain')
            unknown_rectangles(value);audit.audit_state(value)
            raster[info+'_info_absent'] = True
            with self.subTest(info=info), self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema3_exact_raw_arguments_and_reviewed_depthclip_keep_effects_unknown(self):
        for original in (clip_owned(), clip_owned(0, 1), clip_owned(1, 1), fsr_owned(1, 1)):
            value = raw_state(original)
            result = audit.audit_state(value, original=original)
            self.assertEqual(result['writer_state_schema_version'], 3)
            self.assertTrue(all(result['pipeline_facet_known'].values()))
            self.assertFalse(value['depth_clip_value_known'])
            self.assertFalse(value['effective_clipping_known'])

    def test_schema3_depthclip_flags_unknown_siblings_duplicates_and_retained_positions(self):
        value = raw_state(clip_owned(1, 1))
        value['raster_declarations']['reviewed_depth_clip_nodes'][0]['flags'] = 1
        for group in ('viewport_declaration', 'scissor_declaration'): set_reason(value, group, 'depth_clip_flags')
        unknown_rectangles(value);audit.audit_state(value)
        for change in (lambda v: set_reason(v, 'viewport_declaration'),
                       lambda v: v['raster_declarations']['reviewed_depth_clip_nodes'][0].update(chain_index=1),
                       lambda v: v['raster_declarations']['reviewed_depth_clip_nodes'][0].update(flags_known=False),
                       lambda v: v.update(depth_clip_value_known=True),
                       lambda v: v.update(effective_clipping_known=True),
                       lambda v: v.update(clipping_reasons=[])):
            changed = deepcopy(value);change(changed)
            with self.subTest(change=change), self.assertRaises(ValueError): audit.audit_state(changed)
        value = raw_state(clip_owned(1, 1));value['raster_declarations']['raster_chain_types'].append(123456)
        for group in ('viewport_declaration', 'scissor_declaration'): set_reason(value, group, 'unknown_raster_chain')
        unknown_rectangles(value);audit.audit_state(value)
        value['raster_declarations']['raster_chain_types'][1] = audit.DEPTH_CLIP_TYPE
        value['raster_declarations']['reviewed_depth_clip_nodes'] = [
            {'chain_index': index, 'flags_known': False, 'flags': None} for index in (0, 1)]
        audit.audit_state(value)
        value['raster_declarations']['reviewed_depth_clip_nodes'][0].update(flags_known=True, flags=0)
        with self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema3_raw_capture_origin_form_epoch_and_interpretation_cannot_be_retagged(self):
        changes = (lambda r: r.update(origin_ordinal=10), lambda r: r.update(origin_ordinal=4),
                   lambda r: r.update(capture_epoch=1), lambda r: r.update(recording_epoch=3),
                   lambda r: r.update(recording_epoch_matches=False), lambda r: r.update(recording_bank_known=False),
                   lambda r: r.update(form='regular'), lambda r: r.update(first=0), lambda r: r.update(count=2),
                   lambda r: r.update(value_captured=False), lambda r: r['bits'].__setitem__(0, 1),
                   lambda r: r['last_observed_call'].update(ordinal=9),
                   lambda r: r['last_observed_call'].update(payload_captured_for_this_call=False),
                   lambda r: r.update(effective_operand_claimed=True))
        for change in changes:
            value = raw_state(clip_owned(1, 1));change(value['raw_setter_arguments']['viewport'])
            with self.subTest(change=change), self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema3_historical_raw_inputs_survive_unknown_mode_without_becoming_current(self):
        value = raw_state(clip_owned(1, 1));value['raster_declarations']['raster_chain_types'].append(123456)
        for group in ('viewport_declaration', 'scissor_declaration'): set_reason(value, group, 'unknown_raster_chain')
        unknown_rectangles(value)
        for name, raw in value['raw_setter_arguments'].items():
            raw.update(recording_epoch=3, recording_epoch_matches=False, recording_bank_known=False)
            value[name]['epoch'] = 3
        audit.audit_state(value)
        changed = deepcopy(value);changed['raw_setter_arguments']['viewport']['recording_bank_known'] = True
        with self.assertRaises(ValueError): audit.audit_state(changed)
        changed = deepcopy(value);changed['viewport'].update(value_known=True, effective_known=True)
        with self.assertRaises(ValueError): audit.audit_state(changed)

    def test_schema3_last_nonzero_index_or_regular_zero_count_call_is_not_the_candidate(self):
        for first, count in ((3, 2), (0, 0)):
            value = raw_state(clip_owned(0, 1));raw = value['raw_setter_arguments']['viewport']
            raw['last_observed_call'].update(ordinal=9, first=first, count=count, payload_captured_for_this_call=False)
            value['viewport'].update(last_ordinal=9, last_first=first, last_count=count)
            audit.audit_state(value)
            raw['last_observed_call']['payload_captured_for_this_call'] = True
            with self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema3_unavailable_attempt_uses_null_payload_and_same_original_call(self):
        for form, first, count in (('regular', 0, 2), ('with_count', None, 0)):
            value = raw_state(clip_owned(1, 1));raw = value['raw_setter_arguments']['viewport']
            raw.update(value_captured=False, recording_bank_known=False, origin_ordinal=8, form=form, first=first, count=count, bits=None)
            raw['last_observed_call'].update(ordinal=8, form=form, first=first, count=count, payload_captured_for_this_call=False)
            value['viewport'].update(value_known=False, effective_known=False, setter_ordinal=8,
                                     declared_count=count, last_ordinal=8, last_first=0, last_count=count)
            audit.audit_state(value)
            raw['bits'] = [0]*6
            with self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema3_new_semantics_require_new_receipts_and_explicit_owned_feature_control(self):
        for version in (1, 2):
            value = raw_state();value['schema_version'] = version
            with self.assertRaises(ValueError): audit.audit_state(value)
        for change in (lambda v: v.update(depth_clip_feature_enabled=False),
                       lambda v: v.update(depth_clip_state_flags=1), lambda v: v.update(depth_clip_enable_payload=False),
                       lambda v: v.update(depth_clip_chain_type=123456)):
            original = clip_owned();change(original)
            with self.assertRaises(ValueError): audit.golden(original)
        receipt, controls = delivered(1)
        with self.assertRaises(ValueError): audit.audit_receipt(receipt, controls, require_raw=True)

    def test_schema3_ignored_depthclip_flags_cannot_be_promoted_after_raster_refusal(self):
        for reason in ('raster_flags', 'rasterizer_discard'):
            value = raw_state(clip_owned(1, 1))
            node = value['raster_declarations']['reviewed_depth_clip_nodes'][0]
            node.update(flags_known=False, flags=None)
            for group in ('viewport_declaration', 'scissor_declaration'):
                set_reason(value, group, reason, 'unknown_raster_chain')
            unknown_rectangles(value);audit.audit_state(value)
            node.update(flags_known=True, flags=0)
            with self.subTest(reason=reason), self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema3_absent_candidate_and_latest_candidate_changing_call_cannot_conflict(self):
        for form, first, count in (('regular', 0, 1), ('with_count', None, 0)):
            value = raw_state();raw = value['raw_setter_arguments']['viewport']
            raw['last_observed_call'].update(ordinal=8, form=form, first=first, count=count)
            value['viewport'].update(last_ordinal=8, last_first=0, last_count=count)
            with self.subTest(form=form), self.assertRaises(ValueError): audit.audit_state(value)
        value = raw_state();raw = value['raw_setter_arguments']['viewport']
        raw['last_observed_call'].update(ordinal=8, form='regular', first=3, count=2)
        value['viewport'].update(last_ordinal=8, last_first=3, last_count=2)
        audit.audit_state(value)  # First>0 was observed without reading an array.

    def test_schema3_raw_nan_signed_zero_and_extreme_scissor_are_exact_uninterpreted_arguments(self):
        value = raw_state(clip_owned(1, 1));raster = value['raster_declarations']
        raster['raster_chain_types'].append(123456)
        for group in ('viewport_declaration', 'scissor_declaration'): set_reason(value, group, 'unknown_raster_chain')
        unknown_rectangles(value)
        value['raw_setter_arguments']['viewport']['bits'] = [0x7fc12345, 0x80000000, 0, 0xc3800000, 0, 0x3f800000]
        value['raw_setter_arguments']['scissor'].update(offset=[-(1 << 31), (1 << 31)-1], extent=[audit.UINT32, 0])
        audit.audit_state(value)
        for name in ('viewport', 'scissor'):
            changed = deepcopy(value);changed[name].update(value_known=True, effective_known=True)
            with self.subTest(name=name), self.assertRaises(ValueError): audit.audit_state(changed)

    def test_schema3_selected_prefix_requires_its_own_original_raw_and_depthclip_controls(self):
        receipt, controls = delivered(1)
        for index, draw in enumerate(receipt['captured_writer_prefix']['occurrences'][0]['commands'][0]['events'][1:]):
            original = clip_owned(1, index);controls[(1, index)] = original
            draw['writer_state'] = raw_state(original, draw['ordinal'])
        result = audit.audit_receipt(receipt, controls, require_raw=True)
        self.assertEqual(result['draw_states_compared'], 2)
        self.assertEqual(sum(item['raw_argument_banks_audited'] for item in result['states']), 4)
        self.assertEqual(sum(item['raw_captured_payload_banks'] for item in result['states']), 2)
        altered = deepcopy(controls);altered[(1, 0)] = fsr_owned(1, 0)
        with self.assertRaises(ValueError): audit.audit_receipt(receipt, altered, require_raw=True)

    def test_schema3_future_raw_capture_epoch_cannot_hide_behind_an_unknown_mode(self):
        value = raw_state(clip_owned(1, 1));value['raster_declarations']['raster_chain_types'].append(123456)
        for group in ('viewport_declaration', 'scissor_declaration'): set_reason(value, group, 'unknown_raster_chain')
        unknown_rectangles(value)
        raw = value['raw_setter_arguments']['viewport']
        raw.update(capture_epoch=3, recording_epoch_matches=False, recording_bank_known=False)
        with self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema3_interpreted_counts_and_origins_match_their_immutable_declaration_or_setter(self):
        cases = ((clip_owned(), lambda v: v['raster_declarations'].update(viewport_count=2)),
                 (clip_owned(0, 1), lambda v: v['raster_declarations'].update(viewport_count=2)),
                 (clip_owned(), lambda v: v['viewport'].update(declared_count=7)),
                 (clip_owned(), lambda v: v['viewport'].update(setter_ordinal=1)),
                 (clip_owned(0, 1), lambda v: v['viewport'].update(count_ordinal=1)),
                 (clip_owned(1, 1), lambda v: v['viewport'].update(count_ordinal=110)),
                 (clip_owned(1, 1), lambda v: v['viewport'].update(count_ordinal=0)),
                 (clip_owned(1, 1), lambda v: v['viewport'].update(count_ordinal=4)),
                 (clip_owned(1, 1), lambda v: v['viewport'].update(declared_first=99)))
        for original, change in cases:
            value = raw_state(original);change(value)
            with self.subTest(mode=original['mode'], change=change), self.assertRaises(ValueError): audit.audit_state(value)

    def test_schema3_viewport_and_scissor_setters_cannot_share_a_command_occurrence(self):
        value = raw_state(clip_owned(1, 1))
        vp, sc = value['raw_setter_arguments']['viewport'], value['raw_setter_arguments']['scissor']
        sc['origin_ordinal'] = sc['last_observed_call']['ordinal'] = vp['origin_ordinal']
        value['scissor'].update(setter_ordinal=vp['origin_ordinal'], count_ordinal=vp['origin_ordinal'], last_ordinal=vp['origin_ordinal'])
        with self.assertRaises(ValueError): audit.audit_state(value)


if __name__ == '__main__':
    unittest.main()
