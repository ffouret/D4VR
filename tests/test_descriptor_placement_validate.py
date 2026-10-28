import argparse
import copy
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import descriptor_placement_validate as d
import sampled_writer_validate as old
from test_descriptor_write_placement import snapshot


def observation():
    """CPU protocol fixture, not Vulkan arguments or a physical certificate."""
    image = snapshot()
    image['query'].update(output_pointer=0x10120)
    original = image['layouts'][0]
    layouts, slots, setup = [], [], []
    for number, raw, kind, count, offset, stride, set_offset, size in (
            (0, 301, 'sampler', 2, 16, 16, 512, 64),
            (1, 300, 'sampled_image', 2, 32, 32, 768, 128),
            (3, 302, 'uniform_buffer', 1, 0, 32, 1024, 64)):
        layout = copy.deepcopy(original)
        layout.update(raw=raw, size=size)
        layout['bindings'][0].update(binding=0 if number == 0 else 1, type=kind,
                                      count=count, offset=offset, stride=stride)
        layouts.append(layout)
        slot = copy.deepcopy(image['slots'][0])
        slot.update(set=number, binding=0 if number == 0 else 1, index=0, type=kind,
                    layout=dict(raw=raw, generation=4), bound_address=0x100000,
                    set_offset=set_offset, offset_ordinal=3 if number == 3 else 2)
        slots.append(slot)
        setup.append(dict(kind='layout_query', set=number, layout=raw, layout_generation=4,
                          create_flags=16, binding=0 if number == 0 else 1,
                          type={'sampler': 0, 'sampled_image': 2, 'uniform_buffer': 6}[kind],
                          count=count, size=size, binding_offset=offset, set_offset=set_offset))
    image['layouts'], image['slots'] = layouts, slots
    sampler = copy.deepcopy(image)
    sampler['query'].update(ordinal=5, type='sampler', data_size=16,
                             output_pointer=0x10010, output_hex='b2'*16)
    sampler['query']['input'].update(kind='sampler', raw=500)
    setup += [dict(kind='buffer_create', buffer=200, generation=2, size=2048, usage=0x620000,
                   flags=0, sharing_mode=0, pnext=None, result=0),
              dict(kind='memory_allocate', memory=100, generation=1, size=8192,
                   memory_type_index=0, memory_property_flags=6, allocate_flags=2, result=0),
              dict(kind='buffer_bind_memory', buffer=200, buffer_generation=2, memory=100,
                   memory_generation=1, offset=512, result=0),
              dict(kind='memory_map', memory=100, memory_generation=1, offset=1024,
                   size=4096, flags=0, pointer=0x10000, result=0),
              dict(kind='buffer_address', buffer=200, buffer_generation=2, address=0x100000)]
    events = [dict(kind='bind_descriptor_buffers', ordinal=1, command=800,
                   bindings=[dict(address=0x100000, usage=0x600000)]),
              dict(kind='set_descriptor_offsets', ordinal=2, command=800,
                   pipeline_layout=900, first_set=0, buffer_indices=[0, 0], offsets=[512, 768]),
              dict(kind='set_descriptor_offsets', ordinal=3, command=800,
                   pipeline_layout=900, first_set=3, buffer_indices=[0], offsets=[1024])]
    for s, vk_type in ((image, 2), (sampler, 0)):
        q = s['query']
        inp = {k: q['input'][k] for k in ('kind', 'raw', 'generation')}
        if vk_type == 2:
            inp.update(image_layout=5, sampler=0)
        events.append(dict(kind='get_descriptor', ordinal=q['ordinal'], type=vk_type,
                           data_size=q['data_size'], output_pointer=q['output_pointer'],
                           returned=True, pnext=None, input=inp, returned_hex=q['output_hex']))
    return dict(schema_version=1, snapshot_protocol='owned_fixture_query_return_v1',
                setup_fact_scope='owned_fixture_bootstrap_not_live_observer',
                ordinals='case_local_CPU_calls_not_global_GPU_ids', setup_events=setup,
                events=events, snapshots=[image, sampler])


def manifest_prefix():
    """Finite metadata only; no image/readout is fabricated by this helper."""
    return dict(schema_version=1, descriptor_placement_gpu_passed=True, owned_fixture=True,
        game_rendered=False, width=2560, height=1440, format=44, modules=[],
        features={k: True for k in ('dynamicRendering', 'bufferDeviceAddress', 'shaderDrawParameters',
             'runtimeDescriptorArray', 'shaderSampledImageArrayDynamicIndexing', 'descriptorBuffer', 'synchronization2')},
        pipeline=dict(blend_enable=False, logic_op_enable=False, color_write_mask=15, sample_count=1,
             load_op='LOAD', store_op='STORE', rasterizer_discard=False, cull_mode=0, fragment_shading_rate=False),
        raster=dict(viewport=[0, 0, 2560, 1440, 0, 1], scissor=[0, 0, 2560, 1440], render_area=[0, 0, 2560, 1440]),
        textures=[dict(index=i, image=10+i, view=20+i, generation=1, format=37, extent=[8, 8], mips=1, layers=1) for i in range(2)],
        samplers=[dict(index=i, raw=30+i, generation=1, min_filter='nearest', mag_filter='nearest',
             min_lod=0, max_lod=0, address='repeat' if i else 'clamp_to_edge') for i in range(2)],
        counts=dict(submissions=7, draws=6, readbacks=6, image_uploads=2, case_window_queries=12),
        descriptor_properties=dict(alignment=64, sampler_size=16, image_size=32, uniform_size=32),
        validation=dict(debug_messenger=True, synchronization_validation=True, counters_include_teardown=True,
             errors=0, warnings=0, log_file='validation.log'),
        cases=[dict(name=n, bytes=14745600) for n in ('direct_A_clamp', 'direct_B_clamp', 'scratch_B_clamp',
             'identical_hook_A_clamp', 'identical_hook_overwrite_B_clamp', 'direct_B_repeat')])


def preparation_prefix():
    queries = []
    for index, kind, size in ((0, 'image_view', 32), (0, 'sampler', 16),
                              (1, 'image_view', 32), (1, 'sampler', 16), (0, 'uniform_address', 32)):
        inp = dict(kind=kind)
        if kind == 'uniform_address':
            inp.update(address=0x100000, range=1280, format=0)
        else:
            inp.update(raw=(20 if kind == 'image_view' else 30)+index, generation=1)
            if kind == 'image_view':
                inp.update(image_layout=5, sampler=0)
        queries.append(dict(kind='get_descriptor', ordinal=0, returned=True, pnext=None,
            type={'image_view': 2, 'sampler': 0, 'uniform_address': 6}[kind], data_size=size,
            output_pointer=0x90000+len(queries)*512, returned_hex='aa'*size, input=inp))
    return dict(schema_version=1, not_model_input=True, queries_outside_case_windows=queries)


class PixelOracle(unittest.TestCase):
    def setUp(self):
        self.process = patch.object(d.subprocess, 'run', side_effect=AssertionError('No process in unit tests'))
        self.process.start()
        self.addCleanup(self.process.stop)

    def test_texture_formulas_match_existing_owned_original_bytes(self):
        self.assertEqual(d.texture(0), old.texture_bytes(0))
        self.assertEqual(d.texture(1), old.texture_bytes(1))
        for invalid in (True, 0.0, -1, 2, '0'):
            with self.assertRaises(ValueError):
                d.texture(invalid)

    def test_integer_pixel_centers_and_flipped_Y_at_extremes(self):
        rows = list(d.reference_rows(0, 0))
        self.assertEqual(rows[0][:4], bytes((114, 143, 30, 255)))
        self.assertEqual(rows[-1][-4:], bytes((90, 29, 108, 255)))
        # This 320-pixel cell boundary lies between pixel centers.
        self.assertEqual(rows[0][319*4:320*4], bytes((114, 143, 30, 255)))
        self.assertEqual(rows[0][320*4:321*4], bytes((121, 143, 43, 255)))

    def test_entire_B_clamp_and_repeat_references_match_prior_independent_control(self):
        for sampler, name in ((0, 'B_clamp_offset'), (1, 'B_repeat_offset')):
            actual = hashlib.sha256(b''.join(d.reference_rows(1, sampler))).hexdigest()
            prior = hashlib.sha256(b''.join(old.reference_rows(name))).hexdigest()
            self.assertEqual(actual, prior)

    def test_image_and_sampler_countermodels_are_distinguishable(self):
        refs = [b''.join(d.reference_rows(t, s)) for t, s in ((0, 0), (1, 0), (1, 1))]
        self.assertEqual(len({hashlib.sha256(x).digest() for x in refs}), 3)
        for raw, t, s in ((refs[0], 1, 0), (refs[1], 1, 1)):
            result = d.compare_pixels(raw, t, s)
            self.assertGreater(result['mismatched_BA_bytes'], 0)

    def test_original_RGBA_exact_and_single_channel_falsifications(self):
        raw = bytearray(b''.join(d.reference_rows(1, 0)))
        self.assertEqual(d.compare_pixels(raw, 1, 0)['mismatched_bytes'], 0)
        for channel in range(4):
            changed = bytearray(raw)
            changed[100*4+channel] ^= 1
            result = d.compare_pixels(changed, 1, 0)
            self.assertEqual(result['mismatched_bytes'], 1)
            self.assertEqual(result['mismatched_BA_bytes'], int(channel in (0, 3)))
        with self.assertRaises(ValueError):
            d.compare_pixels(raw[:-1], 1, 0)

    def test_type_and_shape_falsifications_are_not_equal(self):
        for actual, expected in ((True, 1), (1.0, 1), ({'x': 1, 'extra': 2}, {'x': 1}),
                                 ([1, 2], [1]), ([1.0], [1])):
            with self.assertRaises(ValueError):
                d.equal(actual, expected, 'test')

    def test_child_paths_are_rejected_before_reading(self):
        with patch.object(d.base, 'read', side_effect=AssertionError('No read for invalid names')):
            for name in ('../secret', '/absolute', '..\\secret', 'C:secret', '.', '..', True):
                with self.assertRaises(ValueError):
                    d.read_child(Path('.'), name)

    def test_native_and_runtime_commands_clear_injections_and_resolve_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'alias').mkdir()
            args = argparse.Namespace(fixture=root/'fixture', validation_layer=root/'layer',
                                      runtime=root/'alias'/'..'/'runtime', output=root/'out')
            modules = [{'path': 'originalVS'}, {'path': 'originalFS'}]
            with patch.object(d.base, 'host_command', return_value=[]), patch.dict(d.os.environ, {'D4VR_OLD': '1'}):
                native = d.launch_command(args, modules, 'native')
                runtime = d.launch_command(args, modules, 'steam-runtime')
            self.assertIn('D4VR_OLD', native)
            self.assertIn('VK_LAYER_VALIDATE_SYNC=1', native)
            self.assertNotIn(str(args.runtime.resolve()), native)
            self.assertIn(str(args.runtime.resolve()), runtime)
            self.assertEqual(runtime.count('--batch'), 1)
            with self.assertRaises(ValueError):
                d.launch_command(args, modules, 'unknown')

    def test_original_argument_joins_and_unknown_epoch_are_independent(self):
        props = dict(alignment=64, image_size=32, sampler_size=16, uniform_size=32)
        original = observation()
        valid = d.audit_observations(original, props)
        self.assertTrue(all(x['typed_write_to_selected_slot_at_query_return'] for x in valid['queries']))
        self.assertTrue(valid['receipt']['complete'])
        for change in (
                lambda o: o['snapshots'][0]['query'].update(output_pointer=0x10121),
                lambda o: o['events'][3].update(returned_hex='cc'*32),
                lambda o: o['snapshots'][0]['buffers'][0].update(size=4096),
                lambda o: o['snapshots'][0]['layouts'][0].update(flags=17),
                lambda o: o['events'][1].update(buffer_indices=[1, 0]),
                lambda o: o['events'][2].update(pipeline_layout=901),
                lambda o: o['events'][3].update(ordinal=3),
                lambda o: o['setup_events'][5].update(buffer_generation=3),
                lambda o: o['setup_events'][5].update(memory_generation=2),
                lambda o: o['setup_events'][6].update(memory_generation=2),
                lambda o: o['setup_events'][7].update(buffer_generation=3),
                lambda o: o['setup_events'][6].update(flags=1),
                lambda o: o['setup_events'][4].update(allocate_flags=0),
                lambda o: o.update(setup_fact_scope='arbitrary_live_application'),
                lambda o: o['snapshots'][1]['slots'][1].update(index=1)):
            altered = copy.deepcopy(original)
            change(altered)
            with self.assertRaises(ValueError):
                d.audit_observations(altered, props)
        changed = copy.deepcopy(original)
        changed['snapshots'][0]['epochs']['memory'][1] += 1
        result = d.audit_observations(changed, props)
        self.assertFalse(result['queries'][0]['typed_write_to_selected_slot_at_query_return'])
        self.assertFalse(result['queries'][0]['sampled_resource_GPU_read_proven'])

    def test_validation_log_cannot_be_redirected_to_an_empty_file(self):
        manifest = manifest_prefix()
        manifest['validation']['log_file'] = 'empty.log'
        with patch.object(d, 'json_child', return_value=manifest), \
             patch.object(d, 'read_child', side_effect=AssertionError('Must reject redirection before reading')):
            with self.assertRaisesRegex(ValueError, 'original validation log filename'):
                d.audit_directory(Path('.'), [])

    def test_copy_integrity_uses_actual_latest_scratch_not_canonical_encoding(self):
        current, scratch = observation(), observation()
        scratch['events'][3]['returned_hex'] = 'c3'*32
        truth = dict(slot0_image_hex_at_submit='c3'*32,
                     slot0_sampler_hex_at_submit=current['events'][4]['returned_hex'])
        d.audit_copy_integrity('identical_hook_overwrite_B_clamp', truth, current,
                               {'scratch_B_clamp': scratch})
        truth['slot0_image_hex_at_submit'] = current['events'][3]['returned_hex']
        with self.assertRaises(ValueError):
            d.audit_copy_integrity('identical_hook_overwrite_B_clamp', truth, current,
                                   {'scratch_B_clamp': scratch})
        d.audit_copy_integrity('direct_A_clamp', truth, current, {})

    def test_case_readout_cannot_be_relabelled_from_an_equivalent_case(self):
        manifest = manifest_prefix()
        manifest['cases'][0]['raw_file'] = 'direct_B_clamp.bgra'
        def metadata(_folder, name, *_args):
            if name == 'manifest.json':
                return manifest
            if name == 'setup.json':
                return dict(schema_version=1, owned_fixture=True,
                    setup_fact_scope='owned_fixture_bootstrap_not_live_observer', events=[])
            if name == 'descriptor-preparation.json':
                return preparation_prefix()
            raise AssertionError('No case metadata should be read')
        def data(_folder, name, *_args):
            if name == 'validation.log':
                return b''
            if name in ('texture-A.rgba', 'texture-B.rgba'):
                return old.texture_bytes(int(name == 'texture-B.rgba'))
            raise AssertionError('Reject relabel before reading any pixels')
        with patch.object(d, 'json_child', side_effect=metadata), patch.object(d, 'read_child', side_effect=data):
            with self.assertRaisesRegex(ValueError, 'raw filename'):
                d.audit_directory(Path('.'), [])


if __name__ == '__main__':
    unittest.main()
