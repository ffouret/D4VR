"""Repository-owned, closed SPIR-V fixtures. No private shaders or GPU needed."""
from collections import Counter
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import sampled_uv_probe as uv
from spirv_probe import Module, string_words


def fixture():
    """An owned sampler control, not bytes copied from a private module."""
    next_id = 1
    declarations, body = [], []

    def allocate():
        nonlocal next_id
        i = next_id;next_id += 1;return i

    def result(op, operands, type_=None, function=False):
        i = allocate();a = [i, *operands] if type_ is None else [type_, i, *operands]
        (body if function else declarations).append((op, a));return i

    string = allocate()
    void = result(19, []);fn = result(33, [void]);uint = result(21, [32, 0])
    float_ = result(22, [32]);vec2 = result(23, [float_, 2])
    vec3 = result(23, [float_, 3]);vec4 = result(23, [float_, 4])
    root = result(30, [uint]*7);image = result(25, [float_, 1, 0, 0, 0, 1, 0])
    sampler = result(26, []);images = result(29, [image]);samplers = result(29, [sampler])
    sampled = result(27, [image])
    specs = [(9, root), (0, images), (0, samplers), (1, vec4), (1, vec3),
             (1, vec2), (3, vec4), (0, image), (9, uint), (0, sampler),
             (1, float_), (3, float_)]
    pointers = {spec: result(32, list(spec)) for spec in specs}
    constants = {k: result(43, [k], uint) for k in (0, 1, 2, 3, 6)}
    result(43, [0], float_)
    pc = result(59, [9], pointers[(9, root)])
    image_var = result(59, [0], pointers[(0, images)])
    sampler_var = result(59, [0], pointers[(0, samplers)])
    fragcoord = result(59, [1], pointers[(1, vec4)])
    color = result(59, [1], pointers[(1, vec3)])
    varyings = [result(59, [1], pointers[(1, vec2)]) for _ in range(4)]
    output = result(59, [3], pointers[(3, vec4)])
    main = result(54, [0, fn], void, function=True)
    result(248, [], function=True);label = allocate();body.extend([(249, [label]), (248, [label])])
    indices, loaded = [], []
    for variable, word, type_ in ((image_var, 2, image), (sampler_var, 6, sampler)):
        index_ptr = result(65, [pc, constants[word]], pointers[(9, uint)], True)
        index = result(61, [index_ptr], uint, True);indices.append(index_ptr)
        ptr = result(65, [variable, index], pointers[(0, type_)], True)
        loaded.append(result(61, [ptr], type_, True))
    xy = []
    for k in (0, 1):
        ptr = result(65, [varyings[0], constants[k]], pointers[(1, float_)], True)
        xy.append(result(61, [ptr], float_, True))
    pair = result(86, loaded, sampled, True);coordinate = result(80, xy, vec2, True)
    sample = result(87, [pair, coordinate, 0], vec4, True)
    extracts = [result(81, [sample, k], float_, True) for k in range(4)]
    for k, v in enumerate(extracts):
        ptr = result(65, [output, constants[k]], pointers[(3, float_)], True)
        body.append((62, [ptr, v]))
    body.extend([(253, []), (56, [])])
    decorators = [(71, [root, 2]), (71, [image_var, 34, 1]), (71, [image_var, 33, 1]),
                  (71, [sampler_var, 34, 0]), (71, [sampler_var, 33, 0]),
                  (71, [fragcoord, 11, 15]), (71, [color, 30, 1]), (71, [output, 30, 0])]
    for i, location, component in zip(varyings, (2, 2, 3, 3), (0, 2, 0, 2)):
        decorators.append((71, [i, 30, location]))
        if component:decorators.append((71, [i, 31, component]))
    decorators += [(72, [root, k, 35, 4*k]) for k in range(7)]
    names = [(main, 'main'), (root, 'OwnedWords'), (pc, 'ownedWords'),
             (fragcoord, 'position'), (color, 'color'), *zip(varyings, ('uv', 'uv1', 'uv2', 'uv3')),
             (output, 'result')]
    prefix = [(17, [1]), (17, [5302]), (17, [5347]),
              (10, string_words('SPV_EXT_descriptor_indexing')),
              (10, string_words('SPV_KHR_physical_storage_buffer')), (14, [5348, 1]),
              (15, [4, main, *string_words('main'), fragcoord, color, *varyings, output]),
              (16, [main, 7]), (7, [string, *string_words('owned UV fixture')])]
    prefix += [(5, [i, *string_words(name)]) for i, name in names]
    m = Module(struct.pack('<5I', 0x07230203, 0x10300, 0, next_id, 0))
    m.instructions = prefix + decorators + declarations + body
    return m, dict(float=float_, uint=uint, constants=constants, pc=pc, image=image_var,
                   sampler=sampler_var, varyings=varyings, output=output, indices=indices,
                   coordinate=coordinate, sample=sample, fragcoord=fragcoord,
                   input_float_pointer=pointers[(1, float_)])


def original_negative_controls(data):
    """Type-correct variants; the separate private CPU proof validates them."""
    m = Module(data);closed = uv.closed_original(data, uv.digest(data))
    defs = {a[1]: (op, a) for op, a in m.instructions if op in uv.TYPED}
    uint = next(a[0] for op, a in m.instructions if op == 21)
    constants = {a[2]: a[1] for op, a in m.instructions if op == 43 and a[0] == uint}
    loads = [(op, a) for op, a in m.instructions if op == 61]
    uv_variable = defs[loads[4][1][2]][1][2]
    other_uv = next(a[0] for op, a in m.instructions if op == 71 and a[1:] == [31, 2]
                    and any(o == 71 and x == [a[0], 30, 2] for o, x in m.instructions))
    variants = []

    def changed(name, edit):
        value = Module(data);edit(value);variants.append((name, value.binary()))

    def chain_index(v, n, index):
        target = loads[n][1][2]
        next(a for op, a in v.instructions if op == 65 and a[1] == target)[-1] = constants[index]

    changed('image_index_wrong_word', lambda v: chain_index(v, 0, 1))
    changed('sampler_index_wrong_word', lambda v: chain_index(v, 2, 3))

    def other_varying(v):
        for op, a in v.instructions:
            if op == 65 and a[2] == uv_variable:a[2] = other_uv
    changed('wrong_coordinate_varying', other_varying)
    changed('coordinate_components_swapped', lambda v: next(a for op, a in v.instructions if op == 80).__setitem__(slice(2, 4), list(reversed(next(a for op, a in v.instructions if op == 80)[2:]))))
    changed('sample_bias_operand_added', lambda v: next(a for op, a in v.instructions if op == 87).__setitem__(slice(4, None), [1, closed['zero_float_id']]))
    changed('output_red_uses_sample_green', lambda v: next(a for op, a in v.instructions if op == 62).__setitem__(1, closed['original_store_values'][1]))
    changed('descriptor_binding_changed', lambda v: next(a for op, a in v.instructions if op == 71 and a[1:] == [33, 1]).__setitem__(2, 2))
    changed('output_location_changed', lambda v: next(a for op, a in v.instructions if op == 71 and a[1:] == [30, 0]).__setitem__(2, 1))
    changed('additional_nop', lambda v: v.instructions.insert(-2, (0, [])))
    changed('additional_output_store', lambda v: v.instructions.insert(-2, (62, [closed['store_destinations'][3], closed['original_store_values'][3]])))
    changed('extra_unused_capability', lambda v: v.instructions.insert(3, (17, [11])))

    def extra_read(v):
        pointer_type = next(a[0] for op, a in v.instructions if op == 32 and a[1:] == [1, closed['float_type_id']])
        fragcoord = next(a[0] for op, a in v.instructions if op == 71 and a[1:] == [11, 15])
        pointer, value = v.allocate(), v.allocate();k = next(k for k, (op, _) in enumerate(v.instructions) if op == 87)
        v.instructions[k:k] = [(65, [pointer_type, pointer, fragcoord, constants[0]]), (61, [closed['float_type_id'], value, pointer])]
    changed('extra_original_input_read', extra_read)
    return variants


def candidate_negative_controls(original, candidate):
    """SPIR-V-valid mutations which must fail deterministic reconstruction."""
    closed = uv.closed_original(original, uv.digest(original));variants = []
    original_module = Module(original)
    loads = [a for op, a in original_module.instructions if op == 61]
    uint = next(a[0] for op, a in original_module.instructions if op == 21)
    integers = {a[2]: a[1] for op, a in original_module.instructions if op == 43 and a[0] == uint}

    def changed(name, edit):
        m = Module(candidate);edit(m);variants.append((name, m.binary()))

    for component in range(4):
        def store_value(m, k=component):
            stores = [a for op, a in m.instructions if op == 62]
            stores[k][1] = closed['original_store_values'][k if k < 2 else 0]
        changed('wrong_store_component_'+str(component), store_value)
    for load, word in ((0, 1), (2, 3)):
        def index(m, n=load, k=word):
            pointer = loads[n][2]
            next(a for op, a in m.instructions if op == 65 and a[1] == pointer)[-1] = integers[k]
        changed('changed_original_index_load_'+str(load), index)
    def swap_stores(m):
        stores = [a for op, a in m.instructions if op == 62]
        stores[0][1], stores[1][1] = stores[1][1], stores[0][1]
    changed('diagnostic_uv_stores_swapped', swap_stores)
    for value, new_value in ((-1., -2.), (253., 252.)):
        def constant(m, old=value, new=new_value):
            bits = struct.unpack('<I', struct.pack('<f', old))[0]
            a = next(a for op, a in m.instructions if op == 43 and a[0] == closed['float_type_id'] and a[2] == bits)
            a[2] = struct.unpack('<I', struct.pack('<f', new))[0]
        changed('encoding_constant_changed_'+str(value), constant)
    changed('diagnostic_taps_wrong_coordinate_component', lambda m: next(a for op, a in m.instructions if op == 81 and a[2:] == [closed['sample_coordinate_id'], 0]).__setitem__(3, 1))
    changed('original_sample_coordinate_swapped', lambda m: next(a for op, a in m.instructions if op == 80).__setitem__(slice(2, 4), list(reversed(next(a for op, a in m.instructions if op == 80)[2:]))))
    changed('extra_store_after_original_stores', lambda m: m.instructions.insert(-2, (62, [closed['store_destinations'][2], closed['original_store_values'][2]])))
    changed('candidate_extra_nop', lambda m: m.instructions.insert(-2, (0, [])))
    changed('original_sample_bias_added', lambda m: next(a for op, a in m.instructions if op == 87).__setitem__(slice(4, None), [1, closed['zero_float_id']]))
    return variants


def validate_owned(args):
    """CPU-only integration mode for CI with existing SPIRV-Tools/grammars."""
    output = Path(args.output)
    if output.exists() or output.is_symlink():raise ValueError('Use a fresh owned CPU output')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    inputs = output/'inputs';inputs.mkdir(mode=0o700)
    original = fixture()[0].binary();source = inputs/'owned-original.spv'
    source.write_bytes(original);source.chmod(0o600)
    candidate_args = SimpleNamespace(original=source, original_sha256=uv.digest(original),
        validator=args.validator, grammar=args.grammar, glsl_grammar=args.glsl_grammar,
        target_env='vulkan1.2', output=output/'candidate')
    report = uv.prepare(candidate_args);candidate = (output/'candidate/candidate.spv').read_bytes()
    policy = uv.Policy(uv.read_regular(args.grammar, 2*1024*1024), uv.read_regular(args.glsl_grammar, 128*1024))
    negatives = []
    for kind, controls in [('original', original_negative_controls(original)),
                           ('candidate', candidate_negative_controls(original, candidate))]:
        for name, data in controls:
            checked = policy.validate(data, args.validator, 'vulkan1.2')
            if not checked['module_validation_passed']:raise ValueError('Owned negative is not valid SPIR-V: '+name)
            try:
                if kind == 'original':uv.closed_original(data, uv.digest(data))
                else:uv.verify_candidate(original, data, uv.digest(original))
            except ValueError:pass
            else:raise ValueError('Owned valid negative accepted: '+name)
            path = output/(kind+'-'+name+'.spv');path.write_bytes(data);path.chmod(0o600)
            negatives.append({'kind': kind, 'name': name, 'sha256': uv.digest(data),
                              'module_validation_passed': True, 'closed_contract_refused': True})
    proof = {'schema_version': 1, 'sampled_uv_owned_CPU_passed': True,
             'owned_generated_fixture_only': True, 'positive_modules_validated': 2,
             'valid_original_negatives_refused': sum(x['kind'] == 'original' for x in negatives),
             'valid_candidate_negatives_refused': sum(x['kind'] == 'candidate' for x in negatives),
             'negative_controls': negatives, 'original_sha256': uv.digest(original),
             'candidate_sha256': uv.digest(candidate), 'candidate_report_sha256': uv.digest((output/'candidate/candidate.json').read_bytes()),
             'constructor_source_sha256': uv.digest(Path(uv.__file__).read_bytes()),
             'test_source_sha256': uv.digest(Path(__file__).read_bytes()),
             'validator_sha256': report['original_validation']['validator_sha256'],
             'GPU_calls': 0, 'game_execution': False, 'bundle_admission': False}
    path = output/'proof.json';path.write_text(json.dumps(proof, indent=2)+'\n');path.chmod(0o600)
    return proof


class SampledUVProbe(unittest.TestCase):
    def setUp(self):
        self.module, self.ids = fixture();self.original = self.module.binary();self.sha = uv.digest(self.original)

    def test_owned_closed_original_and_deterministic_candidate(self):
        contract = uv.closed_original(self.original, self.sha)
        self.assertEqual((contract['load_count'], contract['image_sample_count'], contract['output_store_count']), (6, 1, 4))
        candidate, report = uv.construct_bytes(self.original, self.sha)
        self.assertEqual(uv.construct_bytes(self.original, self.sha), (candidate, report))
        self.assertEqual(self.original, self.module.binary())
        self.assertTrue(uv.verify_candidate(self.original, candidate, self.sha)['deterministic_reconstruction_verified'])
        self.assertFalse(report['module_validation_performed'])

    def test_exactly_two_original_store_values_are_the_only_changed_instructions(self):
        candidate, report = uv.construct_bytes(self.original, self.sha);new = Module(candidate)
        original = Counter((op, tuple(a)) for op, a in self.module.instructions)
        difference = original - Counter((op, tuple(a)) for op, a in new.instructions)
        stores = [tuple(a) for op, a in self.module.instructions if op == 62]
        self.assertEqual(difference, Counter((62, a) for a in stores[:2]))
        self.assertTrue(all(item in new.instructions for item in self.module.instructions if item[0] in (15, 16, 61, 87)))
        self.assertEqual([a for op, a in new.instructions if op == 62][2:], [list(a) for a in stores[2:]])
        p = report['structural_preservation'];self.assertFalse(p['all_original_stores_preserved'])
        self.assertEqual((p['added_declaration_instructions'], p['added_value_instructions']), (7, 24))
        self.assertEqual([sum(op == o for op, _ in new.instructions) for o in (61, 62, 87)], [6, 4, 1])

    def test_diagnostic_taps_real_sample_coordinate_ssa(self):
        candidate, report = uv.construct_bytes(self.original, self.sha)
        original_ids = {a[1] for op, a in self.module.instructions if op in uv.TYPED}
        taps = [a for op, a in Module(candidate).instructions if op == 81 and a[1] not in original_ids]
        self.assertEqual([a[2:] for a in taps], [[report['closed_original_contract']['sample_coordinate_id'], 0], [report['closed_original_contract']['sample_coordinate_id'], 1]])

    def test_original_sha_is_required_and_cannot_be_replaced_by_shape(self):
        for expected in ('', '0'*64, self.sha.upper(), self.sha[:-1], None):
            with self.subTest(expected=expected), self.assertRaises(ValueError):uv.construct_bytes(self.original, expected)
        changed = bytearray(self.original);changed[-1] ^= 1
        with self.assertRaises(ValueError):uv.construct_bytes(changed, self.sha)

    def test_type_correct_closed_original_negatives_rejected_even_with_their_own_hash(self):
        variants = original_negative_controls(self.original);self.assertEqual(len(variants), 12)
        for name, data in variants:
            with self.subTest(name=name), self.assertRaises(ValueError):uv.closed_original(data, uv.digest(data))

    def test_any_candidate_tampering_fails_exact_reconstruction(self):
        candidate, _ = uv.construct_bytes(self.original, self.sha)
        for op in (15, 61, 62, 87, 129, 136, 169, 190):
            m = Module(candidate);a = next(a for o, a in m.instructions if o == op);a[-1] ^= 1
            with self.subTest(op=op), self.assertRaises(ValueError):uv.verify_candidate(self.original, m.binary(), self.sha)
        with self.assertRaises(ValueError):uv.verify_candidate(self.original, self.original, self.sha)

    def test_valid_candidate_negative_controls_refused(self):
        candidate, _ = uv.construct_bytes(self.original, self.sha)
        controls = candidate_negative_controls(self.original, candidate);self.assertEqual(len(controls), 14)
        for name, data in controls:
            with self.subTest(name=name), self.assertRaises(ValueError):uv.verify_candidate(self.original, data, self.sha)

    def test_malformed_and_oversized_originals_do_not_reach_construction(self):
        values = [b'', b'x'*20, self.original[:-1], self.original*(uv.MAX_BYTES//len(self.original)+1)]
        m = Module(self.original);m.header[1] = 0x10400;values.append(m.binary())
        m = Module(self.original);m.header[3] = 1;values.append(m.binary())
        m = Module(self.original);next(a for op, a in m.instructions if op == 22).clear();values.append(m.binary())
        for data in values:
            with self.subTest(size=len(data)), self.assertRaises(ValueError):uv.closed_original(data, uv.digest(data))

    def test_report_names_finite_only_and_no_admission_or_hardware_read_claim(self):
        _, report = uv.construct_bytes(self.original, self.sha)
        for key in ('GPU_validation_performed', 'game_bundle_authorized', 'game_admission', 'game_executed',
                    'xr_submission', 'coverage_repaired', 'sampled_resource_identified', 'physical_original_image_reads_proven'):
            self.assertIs(report[key], False)
        self.assertEqual(report['encoding']['UNORM_conversion'], 'floor_or_ceil_not_round_nearest_only')
        self.assertTrue(report['encoding']['finite_GPU_cases_only'])
        self.assertTrue(report['encoding']['nonfinite_GPU_behavior_uncertified'])

    def test_cpu_float32_encoding_endpoints_ranges_and_unordered_inputs(self):
        candidate, _ = uv.construct_bytes(self.original, self.sha);m = Module(candidate)
        defs = {a[1]: (op, a) for op, a in m.instructions if op in uv.TYPED}
        stores = [a for op, a in m.instructions if op == 62]
        def f32(x):return struct.unpack('<f', struct.pack('<f', x))[0]
        def evaluate(i, coordinate):
            if i == self.ids['coordinate']:return coordinate
            op, a = defs[i]
            if op == 43:return struct.unpack('<f', struct.pack('<I', a[2]))[0]
            if op == 81:return evaluate(a[2], coordinate)[a[3]]
            args = [evaluate(k, coordinate) for k in a[2:]]
            if op == 129:return f32(args[0]+args[1])
            if op == 133:return f32(args[0]*args[1])
            if op == 136:return f32(args[0]/args[1])
            if op == 190:return args[0] >= args[1]
            if op == 188:return args[0] <= args[1]
            if op == 186:return args[0] > args[1]
            if op == 167:return args[0] and args[1]
            if op == 169:return args[1] if args[0] else args[2]
            raise AssertionError(op)
        for x in (-1., -.5, -0., 0., .125, 1., 2.):
            actual = evaluate(stores[0][1], (x, .5));expected = (1+253*((x+1)/3))/255
            self.assertLess(abs(actual-expected), 2e-7)
        for x, expected in ((-2., 0.), (3., 1.), (math.nan, 0.), (math.inf, 1.), (-math.inf, 0.)):
            self.assertEqual(evaluate(stores[0][1], (x, 0.)), expected)  # CPU model only.

    def test_prepare_uses_immutable_snapshots_and_private_fresh_files(self):
        original_bytes = self.original
        class FakePolicy:
            def __init__(self, *_):self.calls = []
            def validate(self, data, *_):
                self.calls.append(data)
                if len(self.calls) == 1:source.write_bytes(b'changed after the snapshot')
                return {'validated_non_output_external_writes_excluded': True,
                        'core_writes_by_storage': {'Output': 4}, 'extended_writes_by_storage': {},
                        'module_validation_passed': True}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);inputs = root/'inputs';inputs.mkdir();source = inputs/'source.spv';source.write_bytes(original_bytes)
            grammar = root/'core.json';grammar.write_bytes(b'{}');glsl = root/'glsl.json';glsl.write_bytes(b'{}')
            output = root/'candidate';args = SimpleNamespace(original=source, original_sha256=self.sha,
                grammar=grammar, glsl_grammar=glsl, validator=root/'unused', target_env='vulkan1.2', output=output)
            with patch.object(uv, 'Policy', FakePolicy):report = uv.prepare(args)
            self.assertEqual((output/'original.snapshot.spv').read_bytes(), original_bytes)
            self.assertEqual((output/'candidate.spv').read_bytes(), uv.construct_bytes(original_bytes, self.sha)[0])
            self.assertEqual(json.loads((output/'candidate.json').read_text()), report)
            self.assertTrue(report['module_validation_performed']);self.assertFalse(report['GPU_validation_performed'])
            if sys.platform != 'win32':
                self.assertEqual(output.stat().st_mode & 0o777, 0o700)
                for file in output.iterdir():self.assertEqual(file.stat().st_mode & 0o777, 0o600)

    def test_symlink_original_refused_without_reading_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);target = root/'input';target.write_bytes(self.original)
            link = root/'link'
            try:link.symlink_to(target)
            except (NotImplementedError, OSError):self.skipTest('Symlink unavailable')
            with self.assertRaises(ValueError):uv.read_regular(link)

    def test_read_is_bounded_even_if_input_grows_after_stat(self):
        class Growing(io.BytesIO):
            requested = []
            def read(self, size=-1):
                self.requested.append(size);return super().read(size)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'small';path.write_bytes(b'small')
            stream = Growing(b'x'*(uv.MAX_BYTES+100))
            with patch.object(Path, 'open', return_value=stream), self.assertRaises(ValueError):uv.read_regular(path)
            self.assertEqual(stream.requested, [uv.MAX_BYTES+1])


if __name__ == '__main__':
    if '--validate-owned' in sys.argv:
        parser = argparse.ArgumentParser(description='Validate owned CPU SPIR-V controls only; no private inputs or GPU')
        parser.add_argument('--validate-owned', action='store_true')
        for name in ('validator', 'grammar', 'glsl-grammar', 'output'):
            parser.add_argument('--'+name, type=Path, required=True)
        result = validate_owned(parser.parse_args())
        print(json.dumps({key: result[key] for key in ('sampled_uv_owned_CPU_passed', 'positive_modules_validated',
            'valid_original_negatives_refused', 'valid_candidate_negatives_refused', 'GPU_calls')}))
    else:
        unittest.main()
