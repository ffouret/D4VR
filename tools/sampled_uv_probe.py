#!/usr/bin/env python3
"""Construct one closed, private UV-color candidate; never admit or execute it.

The caller pins the original SHA256. Recognition additionally closes the entire
original declaration/body shape. The two changed R/G store values are explicit;
this is NOT the older all-original-stores-preserved contract. Original B/A,
reads, sample operands, interfaces and control flow remain unchanged.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

from shader_write_contract import Policy, TYPED
from spirv_probe import Module, literal

CONSTRUCTOR = 'sampled_uv_rg_ba_v1'
MAX_BYTES = 16384
MAX_INSTRUCTIONS = 512
HASH = re.compile(r'[0-9a-f]{64}')
ENCODING = {
    'version': 1, 'range': [-1, 2], 'interior_codes': [1, 254],
    'unavailable_codes': [0, 255], 'interior_code_span': 253,
    'formula': '(1 + 253 * ((t + 1) / 3)) / 255',
    'operation_order': ['FAdd(t,1)', 'FDiv(previous,3)', 'FMul(previous,253)',
                        'FAdd(1,previous)', 'FDiv(previous,255)'],
    'selection': 'ordered(-1<=t && t<=2) ? formula : (ordered(t>2) ? 1 : 0)',
    'UNORM_conversion': 'floor_or_ceil_not_round_nearest_only',
    'finite_GPU_cases_only': True, 'nonfinite_GPU_behavior_uncertified': True,
    'numeric_decoding_requires_output_and_downstream_facets': True,
}
COUNTS = Counter({17: 3, 10: 2, 14: 1, 15: 1, 16: 1, 7: 1, 5: 10,
                  71: 14, 72: 7, 19: 1, 33: 1, 21: 1, 30: 1, 32: 12,
                  59: 10, 22: 1, 25: 1, 29: 2, 26: 1, 23: 3, 43: 6,
                  27: 1, 54: 1, 248: 2, 249: 1, 65: 10, 61: 6,
                  86: 1, 80: 1, 87: 1, 81: 4, 62: 4, 253: 1, 56: 1})


def digest(data):
    return hashlib.sha256(data).hexdigest()


def snapshot(data):
    if not isinstance(data, (bytes, bytearray, memoryview)) or len(data) > MAX_BYTES:
        raise ValueError('UV shader snapshot type/byte bound')
    return bytes(data)


def _closed_original(data, expected_sha256):
    """Static closed recognition only; prepare() validates these same bytes too."""
    data = snapshot(data)
    if not isinstance(expected_sha256, str) or not HASH.fullmatch(expected_sha256):
        raise ValueError('An explicit complete lowercase original SHA256 is required')
    if digest(data) != expected_sha256:
        raise ValueError('Original UV shader SHA256 changed')
    m = Module(data)
    if m.header[1] != 0x10300 or m.header[4] or len(m.instructions) > MAX_INSTRUCTIONS:
        raise ValueError('UV original version/schema/instruction bound')
    if Counter(op for op, _ in m.instructions) != COUNTS:
        raise ValueError('Original is outside the closed UV declaration/body census')
    types, defs, variables, decor, members = {}, {}, {}, {}, {}
    result_ids = set()
    for op, a in m.instructions:
        rid = None
        if 19 <= op <= 39:
            rid = a[0];types[rid] = (op, a[1:])
        elif op in TYPED:
            if len(a) < 2:
                raise ValueError('Malformed UV original result')
            rid = a[1];defs[rid] = (op, a)
        elif op in (7, 248):
            rid = a[0]
        if rid is not None:
            if rid <= 0 or rid >= m.header[3] or rid in result_ids:
                raise ValueError('Duplicate/out-of-bound UV original ID')
            result_ids.add(rid)
        if op == 59:
            variables[a[1]] = a
        elif op == 71:
            if len(a) < 2 or a[1] in decor.setdefault(a[0], {}):
                raise ValueError('Duplicate/malformed UV decoration')
            decor[a[0]][a[1]] = a[2:]
        elif op == 72:
            key = tuple(a[:3])
            if len(a) < 3 or key in members:
                raise ValueError('Duplicate/malformed UV member decoration')
            members[key] = a[3:]

    def one(values, message):
        values = list(values)
        if len(values) != 1:
            raise ValueError(message)
        return values[0]

    def type_(op, operands):
        return one((i for i, value in types.items() if value == (op, operands)),
                   'Original UV type is missing/ambiguous')

    void = type_(19, []);uint = type_(21, [32, 0]);float_ = type_(22, [32])
    function_type = type_(33, [void])
    vec2 = type_(23, [float_, 2]);vec3 = type_(23, [float_, 3]);vec4 = type_(23, [float_, 4])
    root = type_(30, [uint] * 7)
    image = type_(25, [float_, 1, 0, 0, 0, 1, 0]);sampler = type_(26, [])
    images = type_(29, [image]);samplers = type_(29, [sampler]);sampled = type_(27, [image])
    pointer_specs = [(9, root), (0, images), (0, samplers), (1, vec4),
                     (1, vec3), (1, vec2), (3, vec4), (0, image),
                     (9, uint), (0, sampler), (1, float_), (3, float_)]
    pointers = {spec: type_(32, list(spec)) for spec in pointer_specs}
    expected_type_ids = {void, uint, float_, function_type, vec2, vec3, vec4,
                         root, image, sampler, images, samplers, sampled, *pointers.values()}
    if set(types) != expected_type_ids:
        raise ValueError('Additional UV original type declaration')

    def variable(pointer, storage, attributes):
        return one((i for i, a in variables.items() if a == [pointer, i, storage]
                    and decor.get(i, {}) == attributes), 'UV original variable/interface mismatch')

    pc = variable(pointers[(9, root)], 9, {})
    image_var = variable(pointers[(0, images)], 0, {34: [1], 33: [1]})
    sampler_var = variable(pointers[(0, samplers)], 0, {34: [0], 33: [0]})
    fragcoord = variable(pointers[(1, vec4)], 1, {11: [15]})
    color = variable(pointers[(1, vec3)], 1, {30: [1]})
    uv = [variable(pointers[(1, vec2)], 1, attrs) for attrs in
          ({30: [2]}, {30: [2], 31: [2]}, {30: [3]}, {30: [3], 31: [2]})]
    output = variable(pointers[(3, vec4)], 3, {30: [0]})
    if set(variables) != {pc, image_var, sampler_var, fragcoord, color, *uv, output}:
        raise ValueError('Extra UV original variable')
    expected_decor = {root: {2: []}, image_var: {34: [1], 33: [1]},
                      sampler_var: {34: [0], 33: [0]}, fragcoord: {11: [15]},
                      color: {30: [1]}, output: {30: [0]},
                      **{i: attrs for i, attrs in zip(uv, ({30: [2]}, {30: [2], 31: [2]},
                                                          {30: [3]}, {30: [3], 31: [2]}))}}
    if decor != expected_decor or members != {(root, k, 35): [4*k] for k in range(7)}:
        raise ValueError('Original UV decorations/layout changed')
    constants = {(a[0], a[2]): i for i, (op, a) in defs.items() if op == 43 and len(a) == 3}
    if len(constants) != 6 or set(constants) != {(uint, k) for k in (0, 1, 2, 3, 6)} | {(float_, 0)}:
        raise ValueError('Original UV literal set changed')
    ints = {k: constants[(uint, k)] for k in (0, 1, 2, 3, 6)}
    entries = [a for op, a in m.instructions if op == 15]
    entry = entries[0]
    # The closed original has main encoded in two words, seven interface IDs.
    if (len(entry) != 11 or entry[0] != 4 or literal(entry[2:4]) != 'main'
            or entry[4:] != [fragcoord, color, *uv, output]):
        raise ValueError('Expected the closed main fragment entry/interface')
    main = entry[1]
    if ([a for op, a in m.instructions if op == 17] != [[1], [5302], [5347]]
            or [literal(a) for op, a in m.instructions if op == 10] !=
                ['SPV_EXT_descriptor_indexing', 'SPV_KHR_physical_storage_buffer']
            or [a for op, a in m.instructions if op == 14] != [[5348, 1]]
            or [a for op, a in m.instructions if op == 16] != [[main, 7]]):
        raise ValueError('Original UV capability/extension/execution contract changed')

    def definition(i, op, operands=None):
        found = defs.get(i)
        if not found or found[0] != op or (operands is not None and found[1] != operands):
            raise ValueError('Original UV dataflow route changed')
        return found[1]

    sample_op, sample = one(((op, a) for op, a in m.instructions if op == 87), 'Unique UV sample required')
    if len(sample) != 5 or sample[0] != vec4 or sample[4] != 0:
        raise ValueError('UV sample image operands/type changed')
    sample_id, sampled_id, coord_id = sample[1:4]
    pair = definition(sampled_id, 86)
    if len(pair) != 4 or pair[0] != sampled:
        raise ValueError('Original UV sampled-image expression changed')
    image_load = definition(pair[2], 61);sampler_load = definition(pair[3], 61)
    if len(image_load) != 3 or image_load[0] != image or len(sampler_load) != 3 or sampler_load[0] != sampler:
        raise ValueError('Original UV image/sampler load changed')
    image_chain = definition(image_load[2], 65);sampler_chain = definition(sampler_load[2], 65)
    if (len(image_chain) != 4 or image_chain[0] != pointers[(0, image)] or image_chain[2] != image_var
            or len(sampler_chain) != 4 or sampler_chain[0] != pointers[(0, sampler)] or sampler_chain[2] != sampler_var):
        raise ValueError('Original UV descriptor array path changed')
    indices = []
    for chain, word in ((image_chain, 2), (sampler_chain, 6)):
        load = definition(chain[3], 61)
        if len(load) != 3 or load[0] != uint:
            raise ValueError('Original UV index must be loaded uint32')
        access = definition(load[2], 65)
        definition(load[2], 65, [pointers[(9, uint)], load[2], pc, ints[word]])
        indices.append((access, load))
    coord = definition(coord_id, 80)
    if len(coord) != 4 or coord[0] != vec2:
        raise ValueError('Original UV coordinate construction changed')
    xy = []
    for component, value in enumerate(coord[2:]):
        load = definition(value, 61)
        if len(load) != 3 or load[0] != float_:
            raise ValueError('Original UV coordinate scalar load changed')
        access = definition(load[2], 65, [pointers[(1, float_)], load[2], uv[0], ints[component]])
        xy.append((access, load))
    stores = []
    for component, (op, store) in enumerate((x for x in m.instructions if x[0] == 62)):
        if len(store) != 2:
            raise ValueError('Original UV store operands changed')
        pointer, value = store
        access = definition(pointer, 65, [pointers[(3, float_)], pointer, output, ints[component]])
        extract = definition(value, 81, [float_, value, sample_id, component])
        stores.append((access, extract, store))
    body_start = next(k for k, (op, _) in enumerate(m.instructions) if op == 54)
    body = m.instructions[body_start:]
    if len(body) < 4 or body[1][0] != 248 or body[2][0] != 249 or body[3] != (248, body[2][1]):
        raise ValueError('Original UV linear entry blocks changed')
    expected_body = [(54, [void, main, 0, function_type]), body[1], body[2], body[3],
                     (65, indices[0][0]), (61, indices[0][1]), (65, image_chain), (61, image_load),
                     (65, indices[1][0]), (61, indices[1][1]), (65, sampler_chain), (61, sampler_load),
                     (65, xy[0][0]), (61, xy[0][1]), (65, xy[1][0]), (61, xy[1][1]),
                     (86, pair), (80, coord), (sample_op, sample)]
    expected_body += [(81, row[1]) for row in stores]
    for access, _, store in stores:
        expected_body += [(65, access), (62, store)]
    expected_body += [(253, []), (56, [])]
    if body != expected_body:
        raise ValueError('Original UV instruction order or closed linear body changed')
    return {'schema_version': 1, 'contract': 'closed_linear_sample_pc2_pc6_location2_rgba_v1',
            'original_sha256': digest(data), 'original_bytes': len(data),
            'instruction_count': len(m.instructions), 'entry': 'main', 'execution_model': 4,
            'load_count': 6, 'image_sample_count': 1, 'output_store_count': 4,
            'sample_coordinate_id': coord_id, 'sample_result_id': sample_id,
            'float_type_id': float_, 'zero_float_id': constants[(float_, 0)],
            'store_destinations': [row[2][0] for row in stores],
            'original_store_values': [row[2][1] for row in stores],
            'module_validation_performed': False, 'runtime_execution_proven': False}


def closed_original(data, expected_sha256):
    """Bounded static recognition, with no module-validity or execution claim."""
    try:
        return _closed_original(data, expected_sha256)
    except (IndexError, KeyError, TypeError, UnicodeError, struct.error) as error:
        raise ValueError('Malformed closed UV original declaration/body') from error


def construct_bytes(original, expected_sha256):
    original = snapshot(original)
    closed = closed_original(original, expected_sha256)
    m = Module(original);declarations = [];values = []
    float_ = closed['float_type_id'];zero = closed['zero_float_id']
    bool_ = m.allocate();declarations.append((20, [bool_]))
    constants = {}
    for value in (-1., 1., 2., 3., 253., 255.):
        i = m.allocate();bits = struct.unpack('<I', struct.pack('<f', value))[0]
        declarations.append((43, [float_, i, bits]));constants[value] = i

    def value(op, type_, operands):
        i = m.allocate();values.append((op, [type_, i, *operands]));return i

    xy = [value(81, float_, [closed['sample_coordinate_id'], k]) for k in (0, 1)]
    encoded = []
    for t in xy:
        plus = value(129, float_, [t, constants[1.]])
        fraction = value(136, float_, [plus, constants[3.]])
        span = value(133, float_, [fraction, constants[253.]])
        shifted = value(129, float_, [constants[1.], span])
        normalized = value(136, float_, [shifted, constants[255.]])
        ge = value(190, bool_, [t, constants[-1.]])
        le = value(188, bool_, [t, constants[2.]])
        inside = value(167, bool_, [ge, le])
        high = value(186, bool_, [t, constants[2.]])
        outside = value(169, float_, [high, constants[1.], zero])
        encoded.append(value(169, float_, [inside, normalized, outside]))
    result = []
    for op, args in m.instructions:
        if op == 54:
            result.extend(declarations)
        if op == 62 and args[0] in closed['store_destinations'][:2]:
            component = closed['store_destinations'].index(args[0]);args = [args[0], encoded[component]]
        result.append((op, args))
        if op == 87:
            result.extend(values)
    m.instructions = result;candidate = m.binary()
    if len(candidate) > MAX_BYTES or len(m.instructions) > MAX_INSTRUCTIONS:
        raise ValueError('UV candidate exceeded the fixed module budget')
    preservation = {'changed_store_components': [0, 1], 'unchanged_store_components': [2, 3],
                    'changed_store_value_operands': 2, 'output_store_count': 4,
                    'original_loads_unchanged': 6, 'original_image_samples_unchanged': 1,
                    'original_sample_coordinate_and_indices_unchanged': True,
                    'all_other_original_instructions_in_order_unchanged': True,
                    'original_interfaces_and_BA_stores_unchanged': True,
                    'all_original_stores_preserved': False,
                    'added_declaration_instructions': len(declarations),
                    'added_value_instructions': len(values),
                    'new_loads_stores_image_operations_or_control_flow': 0}
    report = {'schema_version': 1, 'constructor': CONSTRUCTOR,
              'original_sha256': digest(original), 'candidate_sha256': digest(candidate),
              'original_bytes': len(original), 'candidate_bytes': len(candidate),
              'encoding': json.loads(json.dumps(ENCODING)), 'closed_original_contract': closed,
              'structural_preservation': preservation, 'module_validation_performed': False,
              'GPU_validation_performed': False, 'game_bundle_authorized': False,
              'game_admission': False, 'game_executed': False, 'xr_submission': False,
              'coverage_repaired': False, 'sampled_resource_identified': False,
              'physical_original_image_reads_proven': False,
              'limits': ['B/A retain an original sample-result dependency; hardware transactions are not proven.',
                         'Only two original store VALUE operands change; older all-stores-preserved certificates do not apply.',
                         'UNORM conversion may choose floor or ceil; independent owned interval oracles are required.',
                         'Finite GPU controls only: NaN/Inf behavior is not certified without separate float controls.',
                         'Codes0/255 are not numeric UV measurements; output and downstream facets gate any decoding.',
                         'A candidate and static write exclusion are not game-bundle admission or a GPU certificate.']}
    return candidate, report


def verify_candidate(original, candidate, expected_sha256):
    """Rebuild deterministically; do not trust a candidate-supplied census."""
    expected, report = construct_bytes(original, expected_sha256)
    if snapshot(candidate) != expected:
        raise ValueError('Candidate differs from the exact closed two-store reconstruction')
    report['deterministic_reconstruction_verified'] = True
    return report


def read_regular(path, maximum=MAX_BYTES):
    path = Path(path)
    if type(maximum) is not int or maximum < 0 or path.is_symlink() or not path.is_file() or path.stat().st_size > maximum:
        raise ValueError('UV constructor requires a bounded regular input file')
    # Bound the read itself as well as the earlier stat, even if the file grows.
    with path.open('rb') as stream:
        data = stream.read(maximum+1)
    if len(data) > maximum:
        raise ValueError('UV constructor input grew beyond its byte bound')
    return data


def prepare(args):
    original = read_regular(args.original)
    candidate, _ = construct_bytes(original, args.original_sha256)
    report = verify_candidate(original, candidate, args.original_sha256)
    grammar = read_regular(args.grammar, 2*1024*1024)
    glsl = read_regular(args.glsl_grammar, 128*1024)
    policy = Policy(grammar, glsl)
    original_validation = policy.validate(original, args.validator, args.target_env)
    candidate_validation = policy.validate(candidate, args.validator, args.target_env)
    if not all(v['validated_non_output_external_writes_excluded'] for v in (original_validation, candidate_validation)):
        raise ValueError('UV original/candidate module validation or write exclusion failed')
    if any(v['core_writes_by_storage'] != {'Output': 4} or v['extended_writes_by_storage']
           for v in (original_validation, candidate_validation)):
        raise ValueError('UV original/candidate write census changed')
    output = Path(args.output)
    protected = [Path(args.original).resolve().parent]
    if (output.exists() or output.is_symlink() or any(p == output.resolve() or p in output.resolve().parents for p in protected)
            or any((p/'Diablo IV.exe').exists() or (p/'Battle.net.exe').exists() for p in [output.resolve(), *output.resolve().parents])):
        raise ValueError('Use a fresh private output outside original/game/launcher directories')
    report.update(module_validation_performed=True,
                  module_validation_passed=True,
                  original_validation=original_validation, candidate_validation=candidate_validation,
                  constructor_source_sha256=digest(Path(__file__).read_bytes()))
    root = Path(__file__).resolve().parents[1]
    report['dependency_source_sha256'] = {name: digest((root/name).read_bytes()) for name in (
        'tools/spirv_probe.py', 'tools/shader_write_contract.py', 'tools/shader_effects.py',
        'tools/glsl_effects.py', 'tools/shader_effects_validate.py')}
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    for name, data in [('original.snapshot.spv', original), ('candidate.spv', candidate),
                       ('candidate.json', (json.dumps(report, indent=2)+'\n').encode())]:
        with (output/name).open('xb') as stream:
            stream.write(data)
        (output/name).chmod(0o600)
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('original', 'validator', 'grammar', 'glsl-grammar', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--original-sha256', required=True)
    p.add_argument('--target-env', choices=('vulkan1.2', 'vulkan1.3'), default='vulkan1.2')
    result = prepare(p.parse_args())
    print(json.dumps({key: result[key] for key in ('constructor', 'original_sha256', 'candidate_sha256',
        'candidate_bytes', 'module_validation_performed', 'GPU_validation_performed', 'game_bundle_authorized')}))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit('UV candidate construction refused: '+str(error))
