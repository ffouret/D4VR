"""Owned instruction fixtures; actual compiler/validator controls run separately."""
import struct
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import projection_depth_suffix as suffix
from glsl_effects import GLSL450
from spirv_probe import Module, string_words
import test_glsl_effects as extended


def fixture(kind='min', expanded=False, split=False, member=False):
    instructions = []
    next_id = 1

    def result(op, operands, typed=None):
        nonlocal next_id
        i = next_id
        next_id += 1
        instructions.append((op, [i, *operands] if typed is None else [typed, i, *operands]))
        return i

    instructions.append((17, [1]))
    glsl = result(11, string_words('GLSL.std.450'))
    instructions.append((14, [0, 1]))
    # Declarations are reordered below into standard SPIR-V sections.
    void = result(19, [])
    fun = result(33, [void])
    float_ = result(22, [32])
    vec = result(23, [float_, 4])
    uint = result(21, [32, 0])
    constants = {k: result(43, [k], uint) for k in (0, 1, 2, 3, 51, 72, 73, 74, 75, 113)}
    scale = result(43, [suffix.BIAS_SCALE_BITS], float_)
    offset = result(43, [suffix.BIAS_OFFSET_BITS], float_)
    array = result(28, [vec, constants[113]])
    block = result(30, [array])
    uniform_ptr = result(32, [2, block])
    row_ptr = result(32, [2, vec])
    input_ptr = result(32, [1, vec])
    output_struct = result(30, [vec]) if member else None
    output_ptr = result(32, [3, output_struct if member else vec])
    output_vec_ptr = result(32, [3, vec]) if member else output_ptr
    output_scalar_ptr = result(32, [3, float_])
    ubo = result(59, [2], uniform_ptr)
    input_ = result(59, [1], input_ptr)
    output = result(59, [3], output_ptr)
    decorations = [(71, [array, 6, 16]), (71, [block, 2]), (72, [block, 0, 35, 0]),
                   (71, [ubo, 34, 3]), (71, [ubo, 33, 0]), (71, [input_, 30, 0])]
    decorations += [(72, [output_struct, 0, 11, 0]), (71, [output_struct, 2])] if member else [(71, [output, 11, 0])]
    declaration_end = len(instructions)
    main = result(54, [0, fun], void)
    label = result(248, [])
    value = result(61, [input_], vec)
    inputs = [result(81, [value, c], float_) for c in range(4)] if expanded else []
    dots, row_pointers, row_loads = [], [], []
    for row in range(72, 76):
        ptr = result(65, [ubo, constants[0], constants[row]], row_ptr)
        load = result(61, [ptr], vec)
        row_pointers.append(ptr)
        row_loads.append(load)
        if expanded:
            coeff = [result(81, [load, c], float_) for c in range(4)]
            d = result(133, [coeff[0], inputs[0]], float_)
            for c in range(1, 4):
                d = result(12, [glsl, 50, coeff[c], inputs[c], d], float_)
        else:
            d = result(148, [load, value], float_)
        dots.append(d)
    bias_row = bias_value = multiply = bias = quotient = None
    if kind == 'min':
        z = result(12, [glsl, 79, dots[2], dots[3]], float_)
    else:
        ptr = result(65, [ubo, constants[0], constants[51]], row_ptr)
        bias_row = result(61, [ptr], vec)
        bias_value = result(81, [bias_row, 0], float_)
        multiply = result(133, [bias_value, scale], float_)
        bias = result(129, [multiply, offset], float_)
        quotient = result(136, [bias, dots[3]], float_)
        z = result(129, [quotient, dots[2]], float_)
    final = [dots[0], dots[1], z, dots[3]]
    if split:
        for c, v in enumerate(final):
            indices = [constants[0], constants[c]] if member else [constants[c]]
            ptr = result(65, [output, *indices], output_scalar_ptr)
            instructions.append((62, [ptr, v]))
        construct = None
    else:
        construct = result(80, final, vec)
        dst = result(65, [output, constants[0]], output_vec_ptr) if member else output
        instructions.append((62, [dst, construct]))
    instructions.extend([(253, []), (56, [])])
    prefix = instructions[:3]
    ordered = prefix + [(15, [0, main, *string_words('main'), input_, output])] + decorations
    ordered += instructions[3:declaration_end] + instructions[declaration_end:]
    m = Module(struct.pack('<5I', 0x07230203, 0x10300, 0, next_id, 0))
    m.instructions = ordered
    return m, dict(glsl=glsl, void=void, float=float_, vec=vec, fun=fun, main=main,
                   label=label, constants=constants, scale=scale, offset=offset, array=array,
                   block=block, ubo=ubo, input=input_, output=output, value=value,
                   dots=dots, final=final, z=z, row_pointers=row_pointers, row_loads=row_loads,
                   bias_row=bias_row, bias_value=bias_value, multiply=multiply,
                   bias=bias, quotient=quotient, construct=construct, inputs=inputs)


class ProjectionDepthSuffix(unittest.TestCase):
    def setUp(self):
        self.policy = SimpleNamespace(glsl=GLSL450(extended.extended_grammar()))

    def analyze(self, m):
        return suffix.analyze(m.binary(), self.policy)

    def typed(self, m, i):
        return next(a for op, a in m.instructions if op in suffix.TYPED and a[1] == i)

    def test_two_suffixes_and_two_dot_forms_preserve_original_final_ids(self):
        for kind in ('min', 'bias'):
            for expanded in (False, True):
                with self.subTest(kind=kind, expanded=expanded):
                    m, i = fixture(kind, expanded)
                    original = m.binary()
                    r = self.analyze(m)
                    self.assertEqual(m.binary(), original)
                    self.assertEqual(r['suffix_kind'], 'min_z_w' if kind == 'min' else 'affine_bias_over_w')
                    self.assertEqual(r['position_ids_diagnostic_only']['pre_suffix_components'], i['dots'])
                    self.assertEqual(r['position_ids_diagnostic_only']['final_components'], i['final'])
                    self.assertFalse(r['module_validation_performed'])
                    self.assertFalse(r['projection_rewrite_allowed'])
                    self.assertFalse(r['metric_scale_compatibility_proven'])
                    self.assertFalse(r['runtime_execution_proven'])
                    self.assertFalse(r['coverage_cause_proven'])

    def test_direct_and_member_position_component_stores(self):
        for split in (False, True):
            for member in (False, True):
                m, _ = fixture('bias', split=split, member=member)
                self.assertEqual(self.analyze(m)['bias']['row'], 51)

    def test_wrong_w_and_other_nonlinear_x_are_refused(self):
        for kind in ('min', 'bias'):
            m, i = fixture(kind)
            self.typed(m, i['z'] if kind == 'min' else i['quotient'])[-1] = i['dots'][0]
            with self.assertRaises(ValueError):
                self.analyze(m)
        m, i = fixture()
        self.typed(m, i['construct'])[2] = i['z']
        with self.assertRaises(ValueError):
            self.analyze(m)

    def test_rows_order_and_shared_vector_are_required(self):
        m, i = fixture()
        self.typed(m, i['row_pointers'][2])[-1] = i['constants'][73]
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, i = fixture()
        self.typed(m, i['dots'][2])[-1] = i['row_loads'][0]
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, i = fixture(expanded=True)
        self.typed(m, i['dots'][2])[-2] = i['inputs'][0]
        with self.assertRaises(ValueError):
            self.analyze(m)

    def test_projection_cannot_already_be_hidden_in_input_or_have_extra_reads(self):
        m, i = fixture()
        for dot in i['dots']:
            self.typed(m, dot)[-1] = i['row_loads'][0]
        with self.assertRaisesRegex(ValueError, 'input depends'):
            self.analyze(m)
        m, i = fixture()
        m.instructions.insert(-2, (61, [i['vec'], 1000, i['row_pointers'][0]]))
        with self.assertRaisesRegex(ValueError, 'exactly four'):
            self.analyze(m)

    def test_bias_bits_row_component_and_rounding_form_are_closed(self):
        for target, value in (('scale', suffix.BIAS_SCALE_BITS + 1), ('offset', suffix.BIAS_OFFSET_BITS - 1)):
            m, i = fixture('bias')
            self.typed(m, i[target])[-1] = value
            with self.assertRaises(ValueError):
                self.analyze(m)
        m, i = fixture('bias')
        self.typed(m, i['bias_value'])[-1] = 1
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, i = fixture('bias')
        ptr = self.typed(m, i['bias_row'])[2]
        self.typed(m, ptr)[-1] = i['constants'][72]
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, i = fixture('bias')
        m.instructions = [(12, [i['float'], i['bias'], i['glsl'], 50, i['bias_value'], i['scale'], i['offset']])
                          if op == 129 and a[1] == i['bias'] else (op, a) for op, a in m.instructions]
        with self.assertRaises(ValueError):
            self.analyze(m)  # Fma would change the original rounding contract.

    def test_bias_scale_multiply_operand_order_is_reported_without_rewriting(self):
        m, i = fixture('bias')
        mul = self.typed(m, i['multiply'])
        mul[2], mul[3] = mul[3], mul[2]
        before = m.binary()
        self.assertEqual(self.analyze(m)['bias']['scale_operand_index'], 0)
        self.assertEqual(before, m.binary())

    def test_dynamic_volatile_and_unknown_forms_are_refused(self):
        m, i = fixture()
        self.typed(m, i['row_pointers'][0])[-1] = i['value']
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, i = fixture()
        self.typed(m, i['row_loads'][0]).append(1)
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, i = fixture()
        m.instructions.insert(4, (71, [i['ubo'], 21]))
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, _ = fixture()
        m.instructions.insert(-2, (65000, []))
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, i = fixture()
        self.typed(m, i['z'])[3] = 37  # FMin is outside existing GLSL450 Policy.
        with self.assertRaises(ValueError):
            self.analyze(m)

    def test_extra_or_incomplete_position_writes_are_refused(self):
        m, _ = fixture()
        store = next(a for op, a in m.instructions if op == 62)
        m.instructions.insert(-2, (62, store.copy()))
        with self.assertRaises(ValueError):
            self.analyze(m)
        m, _ = fixture(split=True)
        m.instructions = [(op, a) for index, (op, a) in enumerate(m.instructions)
                          if index != next(j for j, (o, _) in enumerate(m.instructions) if o == 62)]
        with self.assertRaises(ValueError):
            self.analyze(m)

    def test_multiple_entries_functions_and_control_alternatives_are_refused(self):
        for additional in ('entry', 'function', 'branch'):
            m, i = fixture()
            if additional == 'entry':
                m.instructions.insert(4, (15, [0, i['main'], *string_words('other'), i['input'], i['output']]))
            elif additional == 'function':
                m.instructions.extend([(54, [i['void'], 1000, 0, i['fun']]), (248, [1001]), (253, []), (56, [])])
            else:
                m.instructions.insert(-2, (250, [i['value'], i['label'], i['label']]))
            with self.subTest(additional=additional), self.assertRaises(ValueError):
                self.analyze(m)

    def test_forward_linear_blocks_are_allowed_but_unreachable_blocks_are_refused(self):
        m, i = fixture()
        label = m.instructions.index((248, [i['label']]))
        m.instructions[label + 1:label + 1] = [(249, [1000]), (248, [1000])]
        self.assertTrue(self.analyze(m)['static_suffix_contract_matched'])
        m, _ = fixture()
        m.instructions[-2:-2] = [(253, []), (248, [1000])]
        with self.assertRaises(ValueError):
            self.analyze(m)

    def test_even_pure_select_is_outside_the_linear_closed_contract(self):
        m, i = fixture()
        function = next(j for j, (op, _) in enumerate(m.instructions) if op == 54)
        m.instructions[function:function] = [(20, [1000]), (41, [1000, 1001])]
        m.instructions.insert(-2, (169, [i['vec'], 1002, 1001, i['value'], i['value']]))
        with self.assertRaisesRegex(ValueError, 'alternative control'):
            self.analyze(m)

    def test_local_variables_and_external_stores_are_refused(self):
        for storage in (7, 6, 12):
            m, i = fixture()
            m.instructions.insert(-2, (32, [1000, storage, i['float']]))
            m.instructions.insert(-2, (59, [1000, 1001, storage]))
            m.instructions.insert(-2, (62, [1001, i['dots'][0]]))
            with self.subTest(storage=storage), self.assertRaises(ValueError):
                self.analyze(m)

    def test_ubo_layout_and_duplicate_descriptors_are_refused(self):
        for form in ('stride', 'extent', 'offset', 'duplicate'):
            m, i = fixture()
            if form == 'stride':
                next(a for op, a in m.instructions if op == 71 and a[:2] == [i['array'], 6])[-1] = 32
            elif form == 'extent':
                self.typed(m, i['constants'][113])[-1] = 75
            elif form == 'offset':
                next(a for op, a in m.instructions if op == 72 and a[:3] == [i['block'], 0, 35])[-1] = 16
            else:
                var = self.typed(m, i['ubo']).copy()
                var[1] = 1000
                m.instructions.insert(4, (71, [1000, 34, 3]))
                m.instructions.insert(4, (71, [1000, 33, 0]))
                m.instructions.insert(-2, (59, var))
            with self.subTest(form=form), self.assertRaises(ValueError):
                self.analyze(m)

    def test_malformed_and_oversized_snapshots_fail_closed(self):
        for data in (b'', b'abc', b'x' * (suffix.MAX_BYTES + 1), 'not bytes'):
            with self.assertRaises(ValueError):
                suffix.analyze(data, self.policy)

    def test_validation_uses_one_immutable_snapshot_and_retains_denials(self):
        m, _ = fixture()
        source = bytearray(m.binary())
        expected = bytes(source)
        checked = []

        def validate(data, validator):
            checked.append(data)
            source[:] = b'changed by caller'
            return {'validated_non_output_external_writes_excluded': True, 'validator_sha256': 'test-validator'}

        policy = SimpleNamespace(glsl=self.policy.glsl, core=SimpleNamespace(sha256='test-core'), validate=validate)
        policy.glsl.sha256 = 'test-glsl'
        with patch.object(suffix, 'Policy', return_value=policy):
            r = suffix.validated(source, b'core', b'glsl', Path('unused'))
        self.assertEqual(checked, [expected])
        self.assertTrue(r['module_validation_performed'])
        self.assertFalse(r['projection_rewrite_allowed'])
        policy.validate = lambda *_: {'validated_non_output_external_writes_excluded': False}
        with patch.object(suffix, 'Policy', return_value=policy), self.assertRaises(ValueError):
            suffix.validated(expected, b'core', b'glsl', Path('unused'))
