#!/usr/bin/env python3
"""Run or independently audit an owned sampled-writer pixel control.

The read-only audit reconstructs every BGRA byte from owned texture uploads,
actual push constants, uniform bytes and explicit raster state. Private module
paths and content identities are retained only in private execution metadata.
No game image, resource identity, cause, headset or XR result is established.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess

WIDTH, HEIGHT, TEXELS = 2560, 1440, 8
BYTE_COUNT = WIDTH * HEIGHT * 4
FULL = [0, 0, WIDTH, HEIGHT]
CENTER = [640, 360, 1280, 720]
SENTINEL = (3, 5, 7, 255)
CASES = {
    'A_clamp_zero': (0, 0, (0, 0), FULL, FULL, None),
    'B_clamp_zero': (1, 0, (0, 0), FULL, FULL, None),
    'B_clamp_offset': (1, 0, (1, -1), FULL, FULL, None),
    'B_repeat_offset': (1, 1, (1, -1), FULL, FULL, None),
    'B_clamp_offset_center_scissor_LOAD': (1, 0, (1, -1), CENTER, FULL, None),
    'B_clamp_offset_center_viewport_LOAD': (1, 0, (1, -1), FULL, CENTER, None),
    'wrong_image_word': (0, 0, (1, -1), FULL, FULL, 'B_clamp_offset'),
    'wrong_ubo_component': (1, 0, (0, 0), FULL, FULL, 'B_clamp_offset'),
    'wrong_sampler_word': (1, 0, (1, -1), FULL, FULL, 'B_repeat_offset'),
}
PIPELINE = {'topology': 'triangle_list', 'vertex_count': 3, 'instance_count': 1,
            'first_vertex': 0, 'first_instance': 0, 'cull_mode': 'none',
            'depth_test': False, 'depth_write': False, 'stencil_test': False,
            'blend': False, 'color_write_mask': 15, 'rasterizer_discard': False,
            'sample_count': 1, 'load_op': 'LOAD', 'store_op': 'STORE',
            'sentinel_rgba8': list(SENTINEL)}
DESCRIPTORS = {'sampled_images': {'set': 1, 'binding': 1, 'count': 2, 'type': 'sampled_image'},
               'samplers': {'set': 0, 'binding': 0, 'count': 2, 'type': 'sampler'},
               'ubo': {'set': 3, 'binding': 1, 'bytes': 1280, 'type': 'uniform_buffer'},
               'push_constant_bytes': 28, 'push_constant_stages': [1, 16]}
SAMPLERS = [{'index': index, 'address': address, 'min_filter': 'nearest',
             'mag_filter': 'nearest', 'mipmap_mode': 'nearest', 'min_lod': 0,
             'max_lod': 0, 'anisotropy': False, 'compare': False}
            for index, address in enumerate(('clamp_to_edge', 'repeat'))]
FEATURES = ('dynamicRendering', 'bufferDeviceAddress', 'shaderDrawParameters',
            'runtimeDescriptorArray', 'shaderSampledImageArrayDynamicIndexing')
ERROR_MARKERS = ('VUID-', 'Validation Error', 'SYNC-HAZARD-')


def decode(data):
    def pairs(items):
        out = {}
        for key, value in items:
            if key in out:
                raise ValueError('Duplicate JSON field: ' + key)
            out[key] = value
        return out
    def reject(value):
        raise ValueError('Nonfinite JSON number: ' + value)
    return json.loads(data, object_pairs_hook=pairs, parse_constant=reject)


def read(path, size=None):
    path = Path(path)
    if path.is_symlink() or not path.is_file():
        raise ValueError('Require a regular input file: ' + str(path))
    if size is not None and path.stat().st_size != size:
        raise ValueError('Unexpected byte count: ' + path.name)
    return path.read_bytes()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def check_equal(actual, expected, context):
    # JSON booleans must not impersonate integer settings.
    if type(expected) is bool:
        okay = type(actual) is bool and actual is expected
    elif type(expected) is int:
        okay = (type(actual) is int or (type(actual) is float and math.isfinite(actual))) and actual == expected
    elif isinstance(expected, dict):
        okay = isinstance(actual, dict) and set(actual) == set(expected)
        if okay:
            for key in expected:
                check_equal(actual[key], expected[key], context + '.' + key)
    elif isinstance(expected, (list, tuple)):
        okay = isinstance(actual, list) and len(actual) == len(expected)
        if okay:
            for index, value in enumerate(expected):
                check_equal(actual[index], value, context + '[' + str(index) + ']')
    else:
        okay = actual == expected
    if not okay:
        raise ValueError('Owned control metadata changed: ' + context)


def texture_bytes(index):
    values = bytearray()
    for y in range(TEXELS):
        for x in range(TEXELS):
            rgba = ((17 + 13*x, 29 + 19*y, 41 + 7*x + 11*y, 255) if index == 0
                    else (233 - 11*x, 211 - 17*y, 197 - 5*x - 9*y, 255))
            values.extend(rgba)
    return bytes(values)


def written_state(name):
    texture, sampler, shift, scissor, viewport, intended = CASES[name]
    pc = [0] * 7
    pc[2], pc[6] = texture, sampler
    if name == 'wrong_image_word':
        pc[1] = 1
    if name == 'wrong_sampler_word':
        pc[5] = 1
    ubo = bytearray(1280)
    struct.pack_into('<2f', ubo, 0, shift[0]/8, shift[1]/8)
    zw = (1/8, -1/8) if name == 'wrong_ubo_component' else (0, 0)
    struct.pack_into('<2f', ubo, 8, *zw)
    return {'texture_index': texture, 'sampler_index': sampler, 'shift_texels': list(shift),
            'ubo_row0_xy': [shift[0]/8, shift[1]/8], 'ubo_row0_zw': list(zw),
            'scissor': list(scissor), 'viewport': list(viewport), 'push_constants': pc,
            'negative_control': intended is not None,
            'intended_reference': intended or name}, struct.pack('<7I', *pc), bytes(ubo)


def reference_rows(name):
    """Independent integer pixel-center oracle; no fixture-produced reference."""
    texture, sampler, shift, scissor, viewport, _ = CASES[name]
    txdata = texture_bytes(texture)
    vx, vy, vw, vh = viewport
    sx, sy, sw, sh = scissor
    sentinel = bytes((SENTINEL[2], SENTINEL[1], SENTINEL[0], SENTINEL[3])) * WIDTH
    cache = {}
    for y in range(HEIGHT):
        if not (vy <= y < vy+vh and sy <= y < sy+sh):
            yield sentinel
            continue
        ty = ((2*vh - 2*(y-vy) - 1)*TEXELS)//(2*vh) + shift[1]
        ty = ty % TEXELS if sampler else min(TEXELS-1, max(0, ty))
        if ty not in cache:
            row = bytearray(sentinel)
            for x in range(max(0, vx, sx), min(WIDTH, vx+vw, sx+sw)):
                tx = ((2*(x-vx)+1)*TEXELS)//(2*vw) + shift[0]
                tx = tx % TEXELS if sampler else min(TEXELS-1, max(0, tx))
                i = (ty*TEXELS + tx)*4
                r, g, b, a = txdata[i:i+4]
                row[x*4:x*4+4] = bytes((b, g, r, a))
            cache[ty] = bytes(row)
        yield cache[ty]


def compare_rows(data, name):
    byte_mismatches = pixel_mismatches = 0
    expected_hash = hashlib.sha256()
    mismatch_cache = {}
    for y, row in enumerate(reference_rows(name)):
        expected_hash.update(row)
        actual = data[y*WIDTH*4:(y+1)*WIDTH*4]
        if actual != row:
            key = (actual, row)
            mismatch = mismatch_cache.get(key)
            if mismatch is None:
                mismatch = (sum(a != b for a, b in zip(actual, row)),
                            sum(actual[i:i+4] != row[i:i+4] for i in range(0, len(row), 4)))
                if len(mismatch_cache) < 64:
                    mismatch_cache[key] = mismatch
            byte_mismatches += mismatch[0]
            pixel_mismatches += mismatch[1]
    return {'bytes': byte_mismatches, 'pixels': pixel_mismatches,
            'reference_sha256': expected_hash.hexdigest()}


def verify_module(path, expected_sha256, stage):
    if not re.fullmatch('[0-9a-f]{64}', expected_sha256):
        raise ValueError('Require a private expected SHA256 for each module')
    data = read(path)
    if digest(data) != expected_sha256 or len(data) < 20 or len(data) % 4:
        raise ValueError('Private module bytes differ from pinned input')
    words = struct.unpack('<' + 'I'*(len(data)//4), data)
    if words[0] != 0x07230203:
        raise ValueError('Private module is not SPIR-V')
    models = []
    offset = 5
    while offset < len(words):
        count, opcode = words[offset] >> 16, words[offset] & 65535
        if not count or offset + count > len(words):
            raise ValueError('Malformed private SPIR-V instruction')
        if opcode == 15:
            if count < 4:
                raise ValueError('Malformed private entry point')
            raw_name = struct.pack('<' + 'I'*(count-3), *words[offset+3:offset+count])
            models.append((words[offset+1], raw_name.split(b'\0', 1)[0].decode('utf-8')))
        offset += count
    model = {1: 0, 16: 4}[stage]
    if models != [(model, 'main')]:
        raise ValueError('Private module has unexpected entry point/stage')
    fnv = 14695981039346656037
    for byte in data:
        fnv = ((fnv ^ byte)*1099511628211) & ((1 << 64)-1)
    return {'stage': stage, 'execution_model': model, 'bytes': len(data),
            'sha256': expected_sha256, 'fnv': str(fnv), 'entry': 'main',
            'path': str(Path(path).resolve())}


def audit_directory(directory, modules):
    """Read-only artifact audit. It never launches a subprocess or GPU call."""
    directory = Path(directory)
    manifest = decode(read(directory/'manifest.json'))
    for key, expected in [('schema_version', 1), ('width', WIDTH), ('height', HEIGHT),
                          ('format', 44), ('target_format', 'B8G8R8A8_UNORM')]:
        check_equal(manifest[key], expected, key)
    for key, expected in [('owned_fixture', True), ('game_rendered', False), ('shader_modified', False)]:
        check_equal(manifest[key], expected, key)
    check_equal(manifest['counts'], {'submissions': 10, 'draws': 9, 'image_uploads': 2,
                                   'target_initializations': 9, 'target_readbacks': 9}, 'counts')
    check_equal(manifest['texture_state'], {'width': 8, 'height': 8, 'format': 37, 'mip_levels': 1,
                                         'files': ['texture-A.rgba', 'texture-B.rgba']}, 'texture_state')
    check_equal(manifest['render_area'], FULL, 'render_area')
    check_equal(manifest['dynamic_state'], {'viewport': True, 'scissor': True}, 'dynamic_state')
    check_equal(manifest['modules'], modules, 'modules')
    check_equal(manifest['pipeline_state'], PIPELINE, 'pipeline_state')
    check_equal(manifest['descriptor_state'], DESCRIPTORS, 'descriptor_state')
    check_equal(manifest['sampler_state'], SAMPLERS, 'sampler_state')
    for feature in FEATURES:
        check_equal(manifest['features'][feature], True, feature)
    for optional in ('descriptorBindingVariableDescriptorCount', 'descriptorBindingPartiallyBound',
                     'shaderSampledImageArrayNonUniformIndexing'):
        if optional in manifest['features']:
            check_equal(manifest['features'][optional], False, optional)
    validation = manifest['validation']
    for key in ('debug_messenger', 'synchronization_validation'):
        check_equal(validation[key], True, 'validation.' + key)
    check_equal(validation['errors'], 0, 'validation.errors')
    if validation['log_file'] != 'validation.log':
        raise ValueError('Validation log path changed')
    validation_log = read(directory/'validation.log').decode('utf-8')
    if any(marker in validation_log for marker in ERROR_MARKERS):
        raise ValueError('Khronos validation error hidden by manifest counts')
    warnings = validation['warnings']
    if type(warnings) is not int or warnings < 0:
        raise ValueError('Invalid validation warning count')
    for index, letter in enumerate(('A', 'B')):
        if read(directory/('texture-' + letter + '.rgba'), 256) != texture_bytes(index):
            raise ValueError('Owned texture upload differs from independent pattern')
    cases = manifest['cases']
    if not isinstance(cases, list) or len(cases) != len(CASES) or {c['name'] for c in cases} != set(CASES):
        raise ValueError('Require all nine distinct physical control outputs')
    result = []
    for case in cases:
        name = case['name']
        state, pc, ubo = written_state(name)
        for key, expected in state.items():
            check_equal(case[key], expected, name + '.' + key)
        for key, filename, count, expected in [('pc_file', name+'.pc.bin', 28, pc),
                                               ('ubo_file', name+'.ubo.bin', 1280, ubo)]:
            if case[key] != filename:
                raise ValueError('Actual written control file changed: ' + name + '.' + key)
            raw = read(directory/filename, count)
            hash_key = 'pc_sha256' if key == 'pc_file' else 'ubo_sha256'
            if raw != expected or case[hash_key] != digest(raw):
                raise ValueError('Actual written control bytes/hash changed: ' + name + '.' + key)
        if case['raw_file'] != name+'.bgra' or case['bytes'] != BYTE_COUNT:
            raise ValueError('Owned output byte range changed')
        data = read(directory/case['raw_file'], BYTE_COUNT)
        if digest(data) != case['sha256']:
            raise ValueError('Owned output content hash changed')
        actual = compare_rows(data, name)
        if actual['bytes'] or actual['pixels']:
            raise ValueError(name + ': GPU bytes differ from actual-state independent CPU reference')
        intended = compare_rows(data, state['intended_reference'])
        if state['negative_control']:
            expected_pixels = {'wrong_image_word': 3686400, 'wrong_ubo_component': 3628800,
                               'wrong_sampler_word': 864000}[name]
            expected_data = b''.join(reference_rows(name))
            expected_difference = compare_rows(expected_data, state['intended_reference'])
            if intended != expected_difference or intended['pixels'] != expected_pixels or not intended['bytes']:
                raise ValueError('Negative control did not have its independently predicted mismatch')
        result.append({'name': name, 'bytes': BYTE_COUNT, 'observed_sha256': digest(data),
                       'reference_sha256': actual['reference_sha256'],
                       'actual_state_mismatches': actual, 'intended_reference': state['intended_reference'],
                       'intended_reference_mismatches': intended, 'negative_control': state['negative_control']})
    # Coverage witnesses must also differ from each other and the full target.
    pairs = [('B_clamp_offset_center_scissor_LOAD', 'B_clamp_offset'),
             ('B_clamp_offset_center_viewport_LOAD', 'B_clamp_offset_center_scissor_LOAD')]
    coverage = []
    for actual_name, intended_name in pairs:
        data = read(directory/(actual_name+'.bgra'), BYTE_COUNT)
        mismatch = compare_rows(data, intended_name)
        expected_pixels = 2764800 if actual_name.endswith('scissor_LOAD') else 864000
        if mismatch['pixels'] != expected_pixels or not mismatch['bytes']:
            raise ValueError('Owned coverage/LOAD control is not independently discriminating')
        coverage.append({'actual': actual_name, 'reference': intended_name, **mismatch})
    return {'read_only_audit': True, 'cases': result, 'coverage_witnesses': coverage,
            'compared_bytes': BYTE_COUNT*len(CASES), 'validation_errors': 0,
            'validation_warnings': warnings, 'module_files_verified': True,
            'gpu_execution_independently_certified_by_audit': False}


def private_modules(args):
    return [verify_module(args.vertex, args.vertex_sha256, 1),
            verify_module(args.fragment, args.fragment_sha256, 16)]


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')
    path.chmod(0o600)


def host_command():
    launcher = Path('/run/host/usr/bin/flatpak-spawn')
    return [str(launcher), '--host'] if launcher.is_file() else []


def run_controls(args):
    modules = private_modules(args)
    if args.output.exists():
        raise ValueError('Require a new private output directory')
    args.output.mkdir(mode=0o700, parents=True)
    host = host_command()
    root = Path(__file__).resolve().parents[1]
    sources = ('native/tests/sampled_writer_gpu.cpp', 'native/tests/sampled_writer_hash.hpp',
               'tools/sampled_writer_validate.py', 'tests/test_sampled_writer_validate.py')
    frozen_sources = {p: digest(read(root/p)) for p in sources}
    frozen_fixture = digest(read(args.fixture))
    def check_frozen():
        if digest(read(args.fixture)) != frozen_fixture or {p: digest(read(root/p)) for p in sources} != frozen_sources:
            raise ValueError('Fixture/source changed after pre-execution freeze')
    write_json(args.output/'private-inputs.json', {'modules': modules, 'fixture': str(args.fixture.resolve()),
                                               'fixture_sha256': frozen_fixture, 'source_sha256': frozen_sources})
    audits, private_runs = [], []
    def launch(command, logfile, timeout=120):
        result = subprocess.run(host + command, capture_output=True, text=True, timeout=timeout)
        text = result.stdout + result.stderr
        logfile.write_text(text)
        logfile.chmod(0o600)
        if result.returncode or any(marker in text for marker in ERROR_MARKERS):
            raise ValueError('Owned control failed; inspect private ' + logfile.name)
        return result.stdout
    if args.validator:
        for name, module in zip(('vertex', 'fragment'), modules):
            launch([str(args.validator.resolve()), '--target-env', 'vulkan1.3', module['path']],
                   args.output/(name+'-validation.log'))
    for name, runtime in [('native', None), ('steam-runtime', args.runtime)]:
        output = (args.output/name).resolve()
        command = ['/usr/bin/env']
        clear = {'VK_LAYER_PATH', 'VK_INSTANCE_LAYERS', 'VK_ADD_LAYER_PATH', 'LD_PRELOAD'}
        clear.update(key for key in os.environ if key.startswith('D4VR_'))
        for key in sorted(clear):
            command += ['-u', key]
        command += ['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation',
                    'VK_ADD_LAYER_PATH=' + str(args.validation_layer.resolve()),
                    'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                    'VK_LAYER_VALIDATE_SYNC=1', 'VK_LOADER_DEBUG=layer']
        if runtime:
            command += [str(runtime.resolve()), '--batch', '--']
        command += [str(args.fixture.resolve()), '--vertex', modules[0]['path'],
                    '--fragment', modules[1]['path'], '--output', str(output)]
        check_frozen()
        write_json(args.output/(name+'-command.json'), host + command)
        stdout = launch(command, args.output/(name+'.log'))
        check_frozen()
        summary = decode(stdout.strip())
        if summary.get('sampled_writer_gpu_passed') is not True or summary.get('cases') != 9:
            raise ValueError('Owned fixture did not complete all nine physical outputs')
        if private_modules(args) != modules:
            raise ValueError('Private module input changed during control')
        audit = audit_directory(output, modules)
        audits.append({'environment': name, **audit})
        private_runs.append({'environment': name, 'command': host + command, 'summary': summary,
                             'manifest_sha256': digest(read(output/'manifest.json'))})
    first = {x['name']: x['observed_sha256'] for x in audits[0]['cases']}
    second = {x['name']: x['observed_sha256'] for x in audits[1]['cases']}
    if first != second:
        raise ValueError('Native and Steam Runtime owned outputs differ')
    check_frozen()
    proof = {'schema_version': 1, 'owned_sampled_writer_control_passed': True,
             'native_and_steam_runtime_bytes_equal': True,
             'unchanged_private_module_bytes_verified': True,
             'physical_cases': 18, 'positive_cases_per_environment': 6,
             'negative_cases_per_environment': 3, 'total_compared_bytes': BYTE_COUNT*18,
             'validation_errors': 0, 'environments': audits,
             'fixture_sha256': frozen_fixture,
             'source_sha256': frozen_sources,
             'game_rendered': False, 'game_sampled_resource_identified': False,
             'game_viewport_scissor_known': False, 'game_pixel_cause_proven': False,
             'xr_submission': False, 'actual_game_subprocesses': 0}
    write_json(args.output/'private-execution.json', {'schema_version': 1, 'modules': modules, 'runs': private_runs})
    write_json(args.output/'proof.json', proof)
    return proof


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    for mode in ('run', 'audit'):
        p = sub.add_parser(mode)
        for key in ('vertex', 'fragment'):
            p.add_argument('--'+key, type=Path, required=True)
            p.add_argument('--'+key+'-sha256', required=True)
        if mode == 'run':
            for key in ('fixture', 'validation-layer', 'runtime', 'output'):
                p.add_argument('--'+key, type=Path, required=True)
            p.add_argument('--validator', type=Path)
        else:
            p.add_argument('--directory', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        result = run_controls(args) if args.mode == 'run' else audit_directory(args.directory, private_modules(args))
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, KeyError, TypeError, struct.error, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))


if __name__ == '__main__':
    main()
