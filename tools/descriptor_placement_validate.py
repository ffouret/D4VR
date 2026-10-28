#!/usr/bin/env python3
"""Owned descriptor write/copy control; no game resource admission.

The pixel reference uses owned texture formulas and rational pixel centers.
Placement analysis sees only the saved query/binding facts, never groundtruth
CPU copies. A write at query return does not prove contents at draw/execution.
"""
from __future__ import annotations
import argparse
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess

import sampled_writer_validate as base

WIDTH, HEIGHT, TEXELS = 2560, 1440, 8
BYTES = WIDTH * HEIGHT * 4
CASES = {
    'direct_A_clamp': (0, 0),
    'direct_B_clamp': (1, 0),
    'scratch_B_clamp': (1, 0),
    'identical_hook_A_clamp': (0, 0),
    'identical_hook_overwrite_B_clamp': (1, 0),
    'direct_B_repeat': (1, 1),
}
SOURCES = ('native/CMakeLists.txt', 'native/tests/descriptor_placement_gpu.cpp',
           'native/tests/sampled_writer_gpu.cpp', 'native/tests/sampled_writer_hash.hpp',
           'tools/descriptor_write_placement.py', 'tests/test_descriptor_write_placement.py',
           'tools/descriptor_placement_validate.py', 'tests/test_descriptor_placement_validate.py',
           'tools/sampled_writer_validate.py')


def equal(value, expected, label):
    """Strict JSON shape, including bool/int distinction."""
    if type(value) is not type(expected):
        raise ValueError('Owned declaration type changed: ' + label)
    if isinstance(expected, dict):
        if set(value) != set(expected):
            raise ValueError('Owned declaration keys changed: ' + label)
        for key in expected:
            equal(value[key], expected[key], label+'.'+key)
    elif isinstance(expected, list):
        if len(value) != len(expected):
            raise ValueError('Owned declaration length changed: ' + label)
        for i, item in enumerate(expected):
            equal(value[i], item, label+'.'+str(i))
    elif value != expected:
        raise ValueError('Owned declaration changed: ' + label)


def texture(index):
    if type(index) is not int or index not in (0, 1):
        raise ValueError('Unknown owned texture')
    out = bytearray()
    for y in range(8):
        for x in range(8):
            out.extend((17+13*x, 29+19*y, 41+7*x+11*y, 255) if index == 0 else
                       (233-11*x, 211-17*y, 197-5*x-9*y, 255))
    return bytes(out)


def reference_rows(texture_index, sampler_index):
    """Original stages, full positive viewport, UBO shift (+1,-1) texels.

    Pixel center x=(2*x+1)/(2*width); original vertex flips texture Y.
    These integer formulas do not read a fixture-produced reference image.
    """
    if type(sampler_index) is not int or sampler_index not in (0, 1):
        raise ValueError('Unknown owned sampler')
    data = texture(texture_index)
    cache = {}
    for y in range(HEIGHT):
        ty = ((2*HEIGHT-2*y-1)*8)//(2*HEIGHT)-1
        ty = ty % 8 if sampler_index else min(7, max(0, ty))
        if ty not in cache:
            row = bytearray()
            for x in range(WIDTH):
                tx = ((2*x+1)*8)//(2*WIDTH)+1
                tx = tx % 8 if sampler_index else min(7, max(0, tx))
                r, g, b, a = data[(ty*8+tx)*4:(ty*8+tx)*4+4]
                row.extend((b, g, r, a))
            cache[ty] = bytes(row)
        yield cache[ty]


def compare_pixels(raw, texture_index, sampler_index):
    if len(raw) != BYTES:
        raise ValueError('Wrong owned pixel byte count')
    mismatched_bytes = mismatched_BA = 0
    digest = hashlib.sha256()
    cache = {}
    for y, row in enumerate(reference_rows(texture_index, sampler_index)):
        digest.update(row)
        actual = bytes(raw[y*WIDTH*4:(y+1)*WIDTH*4])
        if actual != row:
            key = actual, row
            if key not in cache:
                cache[key] = (sum(a != b for a, b in zip(actual, row)),
                              sum(actual[i] != row[i] for i in range(len(row)) if i % 4 in (0, 3)))
            a, b = cache[key]
            mismatched_bytes += a
            mismatched_BA += b
    return {'mismatched_bytes': mismatched_bytes, 'mismatched_BA_bytes': mismatched_BA,
            'reference_sha256': digest.hexdigest()}


def read_child(folder, name, size=None):
    if type(name) is not str or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name):
        raise ValueError('Expected single regular child file')
    path = Path(folder)/name
    if path.is_symlink() or not path.is_file():
        raise ValueError('Require regular child')
    limit = 131072 if size is None else size
    with path.open('rb') as source:
        raw = source.read(limit+1)
    if len(raw) > limit or (size is not None and len(raw) != size):
        raise ValueError('Child byte budget differs')
    return raw


def json_child(folder, name, limit=131072):
    if type(name) is not str or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name):
        raise ValueError('Expected single regular JSON child')
    path = Path(folder)/name
    if path.is_symlink() or not path.is_file() or path.stat().st_size > limit:
        raise ValueError('Bounded regular JSON required')
    raw = read_child(folder, name)
    if len(raw) > limit:
        raise ValueError('JSON grew beyond bound')
    return base.decode(raw)


def audit_observations(observation, properties):
    """Join model inputs to the original arguments saved by the owned caller.

    The other helper resources are an explicit bootstrap, not an intercepted
    application's exhaustive catalogue. No groundtruth/pixel argument enters
    this function or the model.
    """
    import descriptor_write_placement as placement
    for key, expected in [('schema_version', 1), ('snapshot_protocol', 'owned_fixture_query_return_v1'),
            ('setup_fact_scope', 'owned_fixture_bootstrap_not_live_observer'),
            ('ordinals', 'case_local_CPU_calls_not_global_GPU_ids')]:
        equal(observation[key], expected, key)
    events, setup, snapshots = observation['events'], observation['setup_events'], observation['snapshots']
    if type(events) is not list or len(events) != 5 or type(setup) is not list or len(setup) != 8:
        raise ValueError('Unexpected bounded original event window')
    equal([e['kind'] for e in events], ['bind_descriptor_buffers', 'set_descriptor_offsets',
          'set_descriptor_offsets', 'get_descriptor', 'get_descriptor'], 'event kinds')
    equal([e['ordinal'] for e in events], [1, 2, 3, 4, 5], 'case-local CPU ordinals')
    equal([e['kind'] for e in setup], ['layout_query']*3+['buffer_create', 'memory_allocate',
          'buffer_bind_memory', 'memory_map', 'buffer_address'], 'owned setup kinds')
    layouts = {e['set']: e for e in setup[:3]}
    equal(sorted(layouts), [0, 1, 3], 'nonempty sets only')
    for e, first, sets in ((events[1], 0, [0, 1]), (events[2], 3, [3])):
        equal(e['first_set'], first, 'first_set')
        equal(e['buffer_indices'], [0]*len(sets), 'descriptor buffer index')
        equal(e['offsets'], [layouts[n]['set_offset'] for n in sets], 'queried set offsets')
    if len(events[0]['bindings']) != 1 or len({e['command'] for e in events[:3]}) != 1:
        raise ValueError('Ambiguous owned command/binding')
    equal(events[1]['pipeline_layout'], events[2]['pipeline_layout'], 'original pipeline layout for both offsets')
    if type(events[1]['pipeline_layout']) is not int or not 0 < events[1]['pipeline_layout'] < 1 << 64:
        raise ValueError('Invalid original pipeline layout')
    create, allocation, bind, mapping, address = setup[3:]
    for e in (create, allocation, bind, mapping):
        equal(e['result'], 0, 'original setup result')
    equal(create['flags'], 0, 'buffer flags')
    equal(create['sharing_mode'], 0, 'exclusive buffer')
    equal(create['pnext'], None, 'buffer chain')
    equal(mapping['flags'], 0, 'original classic map flags')
    equal(allocation['allocate_flags'], 2, 'original BDA allocation flags')
    equal(bind['buffer'], create['buffer'], 'bound buffer')
    equal(bind['memory'], allocation['memory'], 'bound allocation')
    equal(bind['buffer_generation'], create['generation'], 'bound buffer generation')
    equal(bind['memory_generation'], allocation['generation'], 'bound allocation generation')
    equal(mapping['memory'], allocation['memory'], 'mapped allocation')
    equal(mapping['memory_generation'], allocation['generation'], 'mapped allocation generation')
    equal(address['buffer'], create['buffer'], 'addressed buffer')
    equal(address['buffer_generation'], create['generation'], 'addressed buffer generation')
    equal(events[0]['bindings'][0]['address'], address['address'], 'original descriptor address')
    equal(events[0]['bindings'][0]['usage'], create['usage'] & ~0x00020000, 'bind usage')
    if type(snapshots) is not list or len(snapshots) != 2:
        raise ValueError('Two immediate query-return snapshots required')
    registry = placement.Registry()
    ticket = registry.reserve(1)
    outcomes = []
    for index, (snapshot, event) in enumerate(zip(snapshots, events[3:])):
        query = snapshot['query']
        token, vk_type, size_key = ('sampled_image', 2, 'image_size') if index == 0 else ('sampler', 0, 'sampler_size')
        for key, expected in [('ordinal', event['ordinal']), ('returned', True), ('type', token),
                ('data_size', properties[size_key]), ('output_pointer', event['output_pointer']),
                ('output_hex', event['returned_hex']), ('info_complete', True)]:
            equal(query[key], expected, 'query.'+key)
        equal(event['type'], vk_type, 'original VkDescriptorType')
        equal(event['data_size'], query['data_size'], 'original descriptor size')
        equal(event['returned'], True, 'original query returned')
        equal(event['pnext'], None, 'original query chain')
        for key in ('kind', 'raw', 'generation'):
            equal(query['input'][key], event['input'][key], 'typed input.'+key)
        if index == 0:
            equal(event['input']['image_layout'], 5, 'shader read layout')
        for key in ('live_before', 'live_at_return'):
            equal(query['input'][key], True, 'owned input lifetime.'+key)
        equal(snapshot['properties']['alignment'], properties['alignment'], 'descriptor alignment')
        equal(snapshot['properties']['sizes']['sampled_image'], properties['image_size'], 'image descriptor size')
        equal(snapshot['properties']['sizes']['sampler'], properties['sampler_size'], 'sampler descriptor size')
        equal(snapshot['properties']['sizes']['uniform_buffer'], properties['uniform_size'], 'uniform descriptor size')
        traced = next((b for b in snapshot['buffers'] if b['raw'] == create['buffer']), None)
        alloc = next((a for a in snapshot['allocations'] if a['raw'] == allocation['memory']), None)
        mapped = next((m for m in snapshot['maps'] if m['allocation']['raw'] == allocation['memory']), None)
        if traced is None or alloc is None or mapped is None:
            raise ValueError('Traced actual allocation/buffer/map is missing')
        for key in ('size', 'usage', 'flags'):
            equal(traced[key], create[key], 'actual buffer.'+key)
        equal(traced['generation'], create['generation'], 'actual buffer generation')
        equal(traced['binding']['allocation'], dict(raw=allocation['memory'], generation=allocation['generation']), 'actual allocation lifetime')
        equal(traced['binding']['offset'], bind['offset'], 'actual memory binding offset')
        equal(traced['binding']['result'], bind['result'], 'actual memory binding result')
        equal(traced['address'], dict(value=address['address'], known=True), 'actual queried device address')
        equal(alloc['size'], allocation['size'], 'actual allocation size')
        equal(alloc['generation'], allocation['generation'], 'actual allocation generation')
        equal(alloc['host_visible'], bool(allocation['memory_property_flags'] & 2), 'actual host visible allocation')
        equal(alloc['host_coherent'], bool(allocation['memory_property_flags'] & 4), 'actual host coherent allocation')
        for key in ('pointer', 'offset', 'size'):
            equal(mapped[key], mapping[key], 'actual mapping.'+key)
        if len(snapshot['slots']) != 3 or len(snapshot['layouts']) != 3:
            raise ValueError('Owned selection/bootstrap count differs')
        for slot in snapshot['slots']:
            declaration = layouts[slot['set']]
            equal(slot['index'], 0, 'selected shader array index')
            equal(slot['buffer_index'], 0, 'selected binding index')
            equal(slot['buffer'], dict(raw=create['buffer'], generation=create['generation']), 'selected buffer lifetime')
            equal(slot['layout'], dict(raw=declaration['layout'], generation=declaration['layout_generation']), 'selected layout lifetime')
            equal(slot['bound_address'], address['address'], 'bound address')
            equal(slot['set_offset'], declaration['set_offset'], 'selected set offset')
            equal(slot['bind_ordinal'], 1, 'bind before query')
            equal(slot['offset_ordinal'], 3 if slot['set'] == 3 else 2, 'offset before query')
        for layout in snapshot['layouts']:
            original = next((e for e in layouts.values() if e['layout'] == layout['raw']), None)
            if original is None or len(layout['bindings']) != 1:
                raise ValueError('Layout not in actual original query catalogue')
            equal(layout['size'], original['size'], 'queried layout size')
            equal(layout['flags'], original['create_flags'], 'original layout flags')
            equal(layout['generation'], original['layout_generation'], 'original layout generation')
            equal(layout['bindings'][0]['binding'], original['binding'], 'original binding number')
            equal(layout['bindings'][0]['count'], original['count'], 'original binding count')
            equal(layout['bindings'][0]['type'], {0: 'sampler', 2: 'sampled_image', 6: 'uniform_buffer'}[original['type']], 'original descriptor type')
            equal(layout['bindings'][0]['offset'], original['binding_offset'], 'queried binding offset')
        result = placement.analyze(snapshot)
        for key, expected in placement.FALSE_FACETS.items():
            equal(result[key], expected, 'model never promotes '+key)
        equal(result['owned_snapshot_only'], True, 'model domain')
        equal(result['input_facts_attested'], False, 'model does not attest its caller')
        if not ticket.record(snapshot):
            raise ValueError('Owned selected query did not fit bounded ticket')
        outcomes.append(result)
    receipt = ticket.receipt().to_dict()
    equal(receipt['complete'], True, 'owned ticket complete')
    if len(receipt['writes']) != 2:
        raise ValueError('Selected ticket lost a query')
    return dict(queries=outcomes, receipt=receipt, live_layer_observer=False)


def audit_copy_integrity(name, truth, observation, earlier):
    # The vector used for the hidden memcpy was last rewritten by the earlier
    # scratch-B query. Equal typed inputs need not yield canonical opaque bytes.
    # This checks the owned CPU copy operand, never decodes a resource identity.
    if name == 'identical_hook_overwrite_B_clamp':
        expected_image = earlier['scratch_B_clamp']['events'][3]['returned_hex']
    else:
        expected_image = observation['events'][3]['returned_hex']
    equal(truth['slot0_image_hex_at_submit'], expected_image, name+'.actual image bytes before submit')
    equal(truth['slot0_sampler_hex_at_submit'], observation['events'][4]['returned_hex'],
          name+'.actual sampler bytes before submit')


def audit_directory(directory, modules):
    directory = Path(directory)
    manifest = json_child(directory, 'manifest.json')
    for key, expected in [('schema_version', 1), ('descriptor_placement_gpu_passed', True),
                          ('owned_fixture', True), ('game_rendered', False),
                          ('width', WIDTH), ('height', HEIGHT), ('format', 44), ('modules', modules)]:
        equal(manifest[key], expected, key)
    equal(manifest['features'], {key: True for key in (*base.FEATURES, 'descriptorBuffer', 'synchronization2')}, 'features')
    equal(manifest['pipeline'], {'blend_enable': False, 'logic_op_enable': False,
        'color_write_mask': 15, 'sample_count': 1, 'load_op': 'LOAD', 'store_op': 'STORE',
        'rasterizer_discard': False, 'cull_mode': 0, 'fragment_shading_rate': False}, 'pipeline')
    equal(manifest['raster'], dict(viewport=[0, 0, WIDTH, HEIGHT, 0, 1],
          scissor=[0, 0, WIDTH, HEIGHT], render_area=[0, 0, WIDTH, HEIGHT]), 'raster')
    textures, samplers = manifest['textures'], manifest['samplers']
    if len(textures) != 2 or len(samplers) != 2:
        raise ValueError('Owned texture/sampler table changed')
    for index, resource in enumerate(textures):
        for key, expected in [('index', index), ('generation', 1), ('format', 37),
                              ('extent', [8, 8]), ('mips', 1), ('layers', 1)]:
            equal(resource[key], expected, 'texture.'+key)
    for index, resource in enumerate(samplers):
        for key, expected in [('index', index), ('generation', 1), ('min_filter', 'nearest'),
                              ('mag_filter', 'nearest'), ('min_lod', 0), ('max_lod', 0),
                              ('address', 'repeat' if index else 'clamp_to_edge')]:
            equal(resource[key], expected, 'sampler.'+key)
    for values in ([x['image'] for x in textures], [x['view'] for x in textures], [x['raw'] for x in samplers]):
        if any(type(x) is not int or not 0 < x < 1 << 64 for x in values) or len(set(values)) != 2:
            raise ValueError('Owned resource lifetime table is ambiguous')
    equal(manifest['counts'], dict(submissions=7, draws=6, readbacks=6, image_uploads=2,
                                   case_window_queries=12), 'counts')
    for key in ('alignment', 'sampler_size', 'image_size', 'uniform_size'):
        value = manifest['descriptor_properties'][key]
        if type(value) is not int or not 1 <= value <= 4096:
            raise ValueError('Descriptor property outside reviewed finite bounds')
    validation = manifest['validation']
    equal(validation['log_file'], 'validation.log', 'original validation log filename')
    for key in ('debug_messenger', 'synchronization_validation', 'counters_include_teardown'):
        equal(validation[key], True, 'validation.'+key)
    equal(validation['errors'], 0, 'validation.errors')
    if type(validation['warnings']) is not int or validation['warnings'] < 0:
        raise ValueError('Invalid warning count')
    log = read_child(directory, validation['log_file']).decode('utf-8')
    if any(x in log for x in base.ERROR_MARKERS):
        raise ValueError('Khronos error retained in owned log')
    for index, name in enumerate(('texture-A.rgba', 'texture-B.rgba')):
        equal(read_child(directory, name, 256), texture(index), name)
    original_setup = json_child(directory, 'setup.json', 65536)
    equal(original_setup['schema_version'], 1, 'setup schema')
    equal(original_setup['owned_fixture'], True, 'setup domain')
    equal(original_setup['setup_fact_scope'], 'owned_fixture_bootstrap_not_live_observer', 'setup scope')
    preparation = json_child(directory, 'descriptor-preparation.json', 16384)
    equal(preparation['schema_version'], 1, 'preparation schema')
    equal(preparation['not_model_input'], True, 'preparation excluded from model')
    prepared_queries = preparation['queries_outside_case_windows']
    if type(prepared_queries) is not list or len(prepared_queries) != 5:
        raise ValueError('Expected five outside-window descriptor queries')
    for i, event in enumerate(prepared_queries):
        vk_type, size_key = (2, 'image_size') if i in (0, 2) else (0, 'sampler_size') if i in (1, 3) else (6, 'uniform_size')
        for key, expected in [('kind', 'get_descriptor'), ('ordinal', 0), ('returned', True),
                ('pnext', None), ('type', vk_type), ('data_size', manifest['descriptor_properties'][size_key])]:
            equal(event[key], expected, 'outside-window typed query.'+key)
        if type(event['output_pointer']) is not int or not 0 < event['output_pointer'] < 1 << 64:
            raise ValueError('Missing original descriptor output span')
        raw_hex = event['returned_hex']
        if type(raw_hex) is not str or len(raw_hex) != event['data_size']*2 or not re.fullmatch('[0-9a-f]+', raw_hex):
            raise ValueError('Outside-window returned bytes differ')
        inp = event['input']
        if vk_type in (0, 2):
            index = i//2
            resource = textures[index] if vk_type == 2 else samplers[index]
            for key, expected in [('kind', 'image_view' if vk_type == 2 else 'sampler'),
                                  ('raw', resource['view'] if vk_type == 2 else resource['raw']),
                                  ('generation', resource['generation'])]:
                equal(inp[key], expected, 'prepared resource input.'+key)
            if vk_type == 2:
                equal(inp['image_layout'], 5, 'prepared image layout')
                equal(inp['sampler'], 0, 'unused image-info sampler field')
        else:
            equal(inp['kind'], 'uniform_address', 'uniform typed input')
            equal(inp['range'], 1280, 'finite uniform range')
            equal(inp['format'], 0, 'uniform buffer format')
            if type(inp['address']) is not int or not 0 < inp['address'] < 1 << 64:
                raise ValueError('Unknown owned uniform device address')
    cases = manifest['cases']
    if type(cases) is not list or len(cases) != len(CASES) or [c['name'] for c in cases] != list(CASES):
        raise ValueError('Wrong case order/count')
    results, observables, observed_objects, raw_images = [], {}, {}, {}
    for row in cases:
        name = row['name']
        equal(row['bytes'], BYTES, name+'.bytes')
        values = {}
        for kind, size in (('raw', BYTES), ('pc', 28), ('ubo', 1280), ('observations', None), ('groundtruth', None)):
            suffix = {'raw': '.bgra', 'pc': '.pc.bin', 'ubo': '.ubo.bin',
                      'observations': '.observations.json', 'groundtruth': '.groundtruth.json'}[kind]
            equal(row[kind+'_file'], name+suffix, name+'.'+kind+' filename')
            raw = read_child(directory, row[kind+'_file'], size)
            equal(base.digest(raw), row['sha256' if kind == 'raw' else kind+'_sha256'], name+'.'+kind+'hash')
            values[kind] = raw
        equal(values['pc'], struct.pack('<7I', *([0]*7)), name+'.push')
        uniform = bytearray(1280)
        struct.pack_into('<2f', uniform, 0, .125, -.125)
        equal(values['ubo'], bytes(uniform), name+'.uniform')
        # Only this function's groundtruth/pixel side sees the owned CPU-copy
        # action. Placement analysis below receives the observations alone.
        truth = json_child(directory, row['groundtruth_file'], 16384)
        expected_texture, expected_sampler = CASES[name]
        for key, expected in [('schema_version', 1), ('not_model_input', True), ('name', name),
                ('selected_texture_index_at_submit', expected_texture),
                ('selected_sampler_index_at_submit', expected_sampler),
                ('image_query_to_scratch_then_CPU_copy', name == 'scratch_B_clamp'),
                ('hidden_image_overwrite_after_queries_and_bind', name == 'identical_hook_overwrite_B_clamp'),
                ('overwrite_source_prepared_outside_observation_window', name == 'identical_hook_overwrite_B_clamp')]:
            equal(truth[key], expected, name+'.truth.'+key)
        for key in ('previous_GPU_work_finished', 'host_coherent', 'HOST_WRITE_to_DESCRIPTOR_BUFFER_READ_barrier',
                    'original_submit_success', 'producer_fence_completed'):
            equal(truth[key], True, name+'.truth.'+key)
        observation = json_child(directory, row['observations_file'], 65536)
        equal(observation['setup_events'], original_setup['events'], name+'.original setup linkage')
        placement = audit_observations(observation, manifest['descriptor_properties'])
        expected_query_image = 0 if name == 'identical_hook_overwrite_B_clamp' else expected_texture
        equal(observation['events'][3]['input']['raw'], textures[expected_query_image]['view'], name+'.actual image query')
        equal(observation['events'][4]['input']['raw'], samplers[expected_sampler]['raw'], name+'.actual sampler query')
        equal(observation['events'][3]['input']['generation'], textures[expected_query_image]['generation'], name+'.image query lifetime')
        equal(observation['events'][4]['input']['generation'], samplers[expected_sampler]['generation'], name+'.sampler query lifetime')
        equal(observation['events'][3]['input']['sampler'], 0, name+'.unused image-info sampler')
        audit_copy_integrity(name, truth, observation, observed_objects)
        equal(placement['queries'][0]['typed_write_to_selected_slot_at_query_return'],
              name != 'scratch_B_clamp', name+'.image write placement at return')
        equal(placement['queries'][1]['typed_write_to_selected_slot_at_query_return'],
              True, name+'.sampler write placement at return')
        pixels = compare_pixels(values['raw'], expected_texture, expected_sampler)
        if pixels['mismatched_bytes']:
            raise ValueError('Owned independent RGBA mismatch: '+name)
        observables[name] = values['observations']
        observed_objects[name] = observation
        raw_images[name] = values['raw']
        results.append(dict(name=name, raw_sha256=row['sha256'], placement=placement, **pixels))
    equal(observables['identical_hook_A_clamp'], observables['identical_hook_overwrite_B_clamp'],
          'identical selected CPU hook windows')
    first, second = raw_images['identical_hook_A_clamp'], raw_images['identical_hook_overwrite_B_clamp']
    different_BA = sum(a != b for channel in (0, 3) for a, b in zip(first[channel::4], second[channel::4]))
    if not different_BA:
        raise ValueError('Silent copy witness failed to distinguish B/A')
    equal(raw_images['direct_B_clamp'], raw_images['scratch_B_clamp'], 'scratch copy equivalent actual pixels')
    equal(raw_images['direct_B_clamp'], second, 'hidden copy equivalent actual pixels')
    if raw_images['direct_B_repeat'] == raw_images['direct_B_clamp']:
        raise ValueError('Sampler witness did not discriminate')
    return dict(cases=results, readbacks=6, raw_bytes=6*BYTES, validation_errors=0,
                validation_warnings=validation['warnings'], identical_selected_hook_windows=True,
                identical_window_different_BA_bytes=different_BA,
                hidden_or_copied_current_resource_proven=False, game_sampled_resource_identified=False)


def launch_command(args, modules, environment):
    if environment not in ('native', 'steam-runtime'):
        raise ValueError('Unknown environment')
    command = ['/usr/bin/env']
    clear = {'VK_LAYER_PATH', 'VK_INSTANCE_LAYERS', 'VK_ADD_LAYER_PATH', 'LD_PRELOAD'}
    clear.update(k for k in os.environ if k.startswith('D4VR_'))
    for key in sorted(clear):
        command += ['-u', key]
    command += ['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation',
                'VK_ADD_LAYER_PATH='+str(args.validation_layer.resolve()),
                'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                'VK_LAYER_VALIDATE_SYNC=1', 'VK_LOADER_DEBUG=layer']
    if environment == 'steam-runtime':
        command += [str(args.runtime.resolve()), '--batch', '--']
    command += [str(args.fixture.resolve()), '--vertex', modules[0]['path'],
                '--fragment', modules[1]['path'], '--output', str((args.output/environment).resolve())]
    return base.host_command()+command


def run(args):
    root = Path(__file__).resolve().parents[1]
    if args.output.exists() or args.output.is_symlink() or any(p.is_symlink() for p in args.output.parents):
        raise ValueError('Require fresh regular output directory')
    modules = base.private_modules(args)
    frozen = {p: base.digest(base.read(root/p)) for p in SOURCES}
    fixture = base.digest(base.read(args.fixture))
    validator = base.digest(base.read(args.validator))
    def stable():
        equal({p: base.digest(base.read(root/p)) for p in SOURCES}, frozen, 'frozen sources')
        equal(base.digest(base.read(args.fixture)), fixture, 'fixture')
        equal(base.digest(base.read(args.validator)), validator, 'validator')
        equal(base.private_modules(args), modules, 'private originals')
    stable()
    args.output.mkdir(mode=0o700, parents=True)
    base.write_json(args.output/'private-inputs.json', dict(modules=modules, source_sha256=frozen,
                    fixture_sha256=fixture, validator_sha256=validator,
                    scope='owned_snapshots_only_no_live_layer_observer'))
    for index, module in enumerate(modules):
        stable()
        command = base.host_command()+[str(args.validator.resolve()), '--target-env', 'vulkan1.3', module['path']]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        (args.output/('module-'+str(index)+'-validation.log')).write_text(result.stdout+result.stderr)
        if result.returncode:
            raise ValueError('SPIR-V preparation failed before GPU')
        stable()
    audits = []
    for environment in ('native', 'steam-runtime'):
        stable()
        command = launch_command(args, modules, environment)
        base.write_json(args.output/(environment+'-command.json'), command)
        result = subprocess.run(command, capture_output=True, text=True, timeout=300)
        log = result.stdout+result.stderr
        (args.output/(environment+'.log')).write_text(log)
        if result.returncode or any(x in log for x in base.ERROR_MARKERS):
            raise ValueError('Physical control failed; preserve '+environment+' log')
        equal(base.decode(result.stdout.strip()), dict(descriptor_placement_gpu_passed=True,
              cases=6, readbacks=6, validation_errors=0), 'physical completion')
        stable()
        audit = audit_directory(args.output/environment, modules)
        base.write_json(args.output/(environment+'-audit.json'), audit)
        audits.append(dict(environment=environment, **audit))
    stable()
    equal([x['raw_sha256'] for x in audits[0]['cases']], [x['raw_sha256'] for x in audits[1]['cases']],
          'native/runtime original bytes')
    proof = dict(schema_version=1, owned_descriptor_placement_control_passed=True,
        physical_source_sha256=frozen, final_source_sha256=frozen, fixture_sha256=fixture,
        validator_sha256=validator, environments=audits, physical_readbacks=12,
        total_compared_RGBA_bytes=12*BYTES, validation_errors=0,
        native_runtime_bytes_equal=True, live_layer_observer_implemented=False,
        descriptor_current_contents_or_GPU_read_proven=False, game_rendered=False,
        game_sampled_resource_identified=False, coverage_repaired=False, xr_submission=False)
    base.write_json(args.output/'proof.json', proof)
    return proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('fixture', 'vertex', 'fragment', 'validator', 'validation-layer', 'runtime', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    for key in ('vertex', 'fragment'):
        parser.add_argument('--'+key+'-sha256', required=True)
    result = run(parser.parse_args())
    import json
    print(json.dumps({k: v for k, v in result.items() if k != 'environments'}, indent=2))


if __name__ == '__main__':
    main()
