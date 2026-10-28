"""Closed recognition of two original vertex depth suffixes.

This describes the original expression, including its nonlinear depth step.
It neither admits a shader to a bundle nor certifies changed projection units,
execution, coverage, or a pass role. IDs are diagnostics within this snapshot.
Only validated() also validates the same immutable bytes with the pinned Policy.
"""
import hashlib

import camera_dataflow
from shader_write_contract import Policy, TYPED
from spirv_probe import Module, literal

MAX_BYTES = 16 * 1024 * 1024
MAX_INSTRUCTIONS = 250000
BIAS_SCALE_BITS = 0x3dcccccd
BIAS_OFFSET_BITS = 0x3ea8f5c3


def _snapshot(data):
    if not isinstance(data, (bytes, bytearray, memoryview)) or len(data) > MAX_BYTES:
        raise ValueError('Suffix shader byte budget/type')
    return bytes(data)


def _analyze(data, policy):
    module = Module(data)
    if len(module.instructions) > MAX_INSTRUCTIONS:
        raise ValueError('Suffix shader instruction budget')
    routes = camera_dataflow.analyze(data, policy)
    entries = [a for op, a in module.instructions if op == 15]
    functions = [a for op, a in module.instructions if op == 54]
    if len(entries) != 1 or entries[0][0] != 0 or len(functions) != 1 or functions[0][1] != entries[0][1]:
        raise ValueError('Suffix requires one original vertex entry/function')
    defs, types, decor, member_decor, imports, variables = {}, {}, {}, {}, {}, {}
    stores = []
    branch_target = None
    label_count = 0
    for op, a in module.instructions:
        if op in (57, 169, 245, 246, 247, 250, 251, 254):
            raise ValueError('Suffix does not model calls or alternative control flow')
        expected_label = branch_target is not None
        if expected_label and op not in (8, 317):
            if op != 248 or a != [branch_target]:
                raise ValueError('Suffix requires a linear block sequence')
            branch_target = None
        if op == 248:
            if label_count and not expected_label:
                raise ValueError('Unreachable/alternative suffix block')
            label_count += 1
        if op == 249:
            branch_target = a[0]
        if op in TYPED:
            if len(a) < 2 or a[1] in defs:
                raise ValueError('Duplicate/malformed suffix value')
            defs[a[1]] = (op, a)
        if 19 <= op <= 39:
            if a[0] in types:
                raise ValueError('Duplicate suffix type')
            types[a[0]] = (op, a[1:])
        elif op == 71:
            attrs = decor.setdefault(a[0], {})
            if a[1] in attrs or a[1] == 21:
                raise ValueError('Duplicate/volatile suffix decoration')
            attrs[a[1]] = a[2:]
        elif op == 72:
            attrs = member_decor.setdefault((a[0], a[1]), {})
            if a[2] in attrs or a[2] == 21:
                raise ValueError('Duplicate/volatile suffix member decoration')
            attrs[a[2]] = a[3:]
        elif op == 11:
            imports[a[0]] = literal(a[1:])
        elif op == 59:
            variables[a[1]] = a
        elif op == 62:
            if len(a) != 2:
                raise ValueError('Decorated suffix store')
            stores.append(a)
    if branch_target is not None:
        raise ValueError('Unresolved suffix branch')
    if sum(op == 253 for op, _ in module.instructions) != 1:
        raise ValueError('Suffix requires one final return')

    def definition(i, op=None):
        found = defs.get(i)
        if found is None or (op is not None and found[0] != op):
            raise ValueError('Unsupported suffix value form')
        return found[1]

    def scalar(i):
        return types.get(definition(i)[0]) == (22, [32])

    def vector(i):
        t = types.get(definition(i)[0])
        return bool(t and t[0] == 23 and t[1][1:] == [4] and types.get(t[1][0]) == (22, [32]))

    def integer(i):
        a = definition(i, 43)
        if len(a) != 3 or types.get(a[0]) not in ((21, [32, 0]), (21, [32, 1])) or a[2] >= 0x80000000:
            raise ValueError('Suffix requires a constant nonnegative index')
        return a[2]

    def pointer(i, seen=None):
        seen = set() if seen is None else seen
        if i in seen or len(seen) >= 32:
            raise ValueError('Suffix pointer cycle/budget')
        if i in variables:
            return i, ()
        op, a = defs.get(i, (None, []))
        if op not in (65, 66):
            raise ValueError('Suffix requires direct constant pointer paths')
        root, path = pointer(a[2], seen | {i})
        return root, path + tuple(integer(k) for k in a[3:])

    positions = []
    for i, a in variables.items():
        if a[2] != 3:
            continue
        ptr = types.get(a[0])
        if not ptr or ptr[0] != 32 or ptr[1][0] != 3:
            raise ValueError('Unresolved suffix output type')
        if decor.get(i, {}).get(11) == [0]:
            positions.append((i, (), ptr[1][1]))
        struct = types.get(ptr[1][1])
        if struct and struct[0] == 30:
            for index, t in enumerate(struct[1]):
                if member_decor.get((ptr[1][1], index), {}).get(11) == [0]:
                    positions.append((i, (index,), t))
    if len(positions) != 1:
        raise ValueError('Expected one declared Position output')
    position, prefix, position_type = positions[0]
    pt = types.get(position_type)
    if not pt or pt[0] != 23 or pt[1][1:] != [4] or types.get(pt[1][0]) != (22, [32]):
        raise ValueError('Position must be float32 vec4')
    components = {}
    vector_store = None
    for dst, value in stores:
        root, path = pointer(dst)
        if root != position:
            continue
        if path == prefix:
            a = definition(value, 80)
            if components or vector_store is not None or a[0] != position_type or len(a) != 6:
                raise ValueError('Extra/noncanonical Position writes')
            vector_store = value
            components = dict(enumerate(a[2:]))
        elif len(path) == len(prefix) + 1 and path[:len(prefix)] == prefix and 0 <= path[-1] < 4:
            if path[-1] in components or vector_store is not None:
                raise ValueError('Extra Position component writes')
            components[path[-1]] = value
        else:
            raise ValueError('Unmodeled write to Position container')
    if set(components) != {0, 1, 2, 3} or not all(scalar(i) for i in components.values()):
        raise ValueError('Position requires exactly four scalar writes')
    final = [components[i] for i in range(4)]

    targets = [i for i in variables if decor.get(i, {}).get(34) == [3] and decor.get(i, {}).get(33) == [0]]
    if len(targets) != 1:
        raise ValueError('Expected unique set3 binding0 projection UBO')
    target = targets[0]
    var = variables[target]
    ptr = types.get(var[0])
    if var[2] != 2 or not ptr or ptr[0] != 32 or ptr[1][0] != 2:
        raise ValueError('Projection descriptor must be Uniform')
    struct_id = ptr[1][1]
    struct = types.get(struct_id)
    if not struct or struct[0] != 30 or len(struct[1]) != 1 or decor.get(struct_id, {}).get(2) != [] or member_decor.get((struct_id, 0), {}).get(35) != [0]:
        raise ValueError('Projection UBO requires one zero-offset Block array')
    array_id = struct[1][0]
    array = types.get(array_id)
    if not array or array[0] != 28 or decor.get(array_id, {}).get(6) != [16] or integer(array[1][1]) < 76:
        raise ValueError('Projection UBO row layout/extent')
    vector_type = array[1][0]
    if vector_type != position_type:
        raise ValueError('Projection UBO requires the Position float4 type')

    def row_load(i, row, component=None):
        a = definition(i, 61)
        if len(a) != 3:
            raise ValueError('Decorated projection load')
        chain = definition(a[2])
        if defs[a[2]][0] not in (65, 66) or chain[2] != target:
            raise ValueError('Projection load must directly address its UBO')
        expected = (0, row) if component is None else (0, row, component)
        if pointer(a[2]) != (target, expected):
            raise ValueError('Projection row/component mismatch')
        if component is None and a[0] != vector_type:
            raise ValueError('Projection row must be float4')
        if component is not None and not scalar(i):
            raise ValueError('Projection scalar must be float32')

    def extended(i, instruction, count):
        a = definition(i, 12)
        if len(a) != 4 + count or imports.get(a[2]) != 'GLSL.std.450' or a[3] != instruction or not scalar(i):
            raise ValueError('Unsupported suffix extended expression')
        return a[4:]

    def coefficient(i, row, component):
        a = definition(i, 81)
        if len(a) != 4 or a[3] != component or not scalar(i):
            raise ValueError('Projection coefficient extraction')
        row_load(a[2], row)

    def dot(i, row):
        op, a = defs.get(i, (None, []))
        if not scalar(i):
            raise ValueError('Projection dot must be float32')
        if op == 148:
            if len(a) != 4 or not vector(a[3]):
                raise ValueError('Expected direct float4 OpDot')
            row_load(a[2], row)
            return 'op_dot', a[3]
        inputs = [None] * 4
        current = i
        for c in (3, 2, 1):
            coeff, inputs[c], current = extended(current, 50, 3)
            coefficient(coeff, row, c)
        a = definition(current, 133)
        if len(a) != 4:
            raise ValueError('Expected initial projection multiply')
        coefficient(a[2], row, 0)
        inputs[0] = a[3]
        if not all(scalar(v) for v in inputs):
            raise ValueError('Projection inputs must be float32')
        return 'ordered_fmul_fma', tuple(inputs)

    def fconstant(i, bits):
        if defs.get(i, (None, []))[0] != 43:
            return False
        a = definition(i, 43)
        return len(a) == 3 and scalar(i) and a[2] == bits

    def bias(i):
        a = definition(i, 129)
        if len(a) != 4 or not scalar(i) or not fconstant(a[3], BIAS_OFFSET_BITS):
            raise ValueError('Expected exact affine bias offset')
        mul = definition(a[2], 133)
        if len(mul) != 4 or not scalar(a[2]):
            raise ValueError('Expected separate affine bias multiply')
        if fconstant(mul[3], BIAS_SCALE_BITS):
            value, scale_operand = mul[2], 1
        elif fconstant(mul[2], BIAS_SCALE_BITS):
            value, scale_operand = mul[3], 0
        else:
            raise ValueError('Expected exact affine bias scale')
        if defs.get(value, (None, []))[0] == 81:
            extract = definition(value, 81)
            if len(extract) != 4 or extract[3] != 0 or not scalar(value):
                raise ValueError('Expected bias row51.x')
            row_load(extract[2], 51)
        else:
            row_load(value, 51, 0)
        return {'set': 3, 'binding': 0, 'row': 51, 'component': 0,
                'scale_float32_bits': BIAS_SCALE_BITS, 'offset_float32_bits': BIAS_OFFSET_BITS,
                'operation_order': 'separate_fmul_then_fadd', 'scale_operand_index': scale_operand,
                'value_id_diagnostic_only': i}

    z = final[2]
    bias_contract = None
    if defs[z][0] == 12:
        qz, qw = extended(z, 79, 2)
        if qw != final[3]:
            raise ValueError('NMin must use the unchanged projected W')
        kind = 'min_z_w'
    elif defs[z][0] == 129:
        a = definition(z, 129)
        if len(a) != 4:
            raise ValueError('Expected depth bias addition')
        candidates = [(a[2], a[3]), (a[3], a[2])]
        found = []
        for possible_z, quotient in candidates:
            d = defs.get(quotient)
            if d and d[0] == 136 and len(d[1]) == 4 and scalar(quotient):
                if d[1][3] != final[3]:
                    raise ValueError('Depth bias must divide by unchanged projected W')
                found.append((possible_z, bias(d[1][2])))
        if len(found) != 1:
            raise ValueError('Expected exactly one affine-bias-over-W suffix')
        qz, bias_contract = found[0]
        kind = 'affine_bias_over_w'
    else:
        raise ValueError('Unrecognized final depth suffix')
    original = [final[0], final[1], qz, final[3]]
    signatures = [dot(v, row) for v, row in zip(original, range(72, 76))]
    if any(s != signatures[0] for s in signatures[1:]):
        raise ValueError('Projection rows must multiply the same float4 input')
    # These are the four original source row reads, not another projection
    # already hidden in their common input. This also rejects unused extra
    # projection reads rather than silently enlarging a later rewrite's scope.
    row_reads = {row: 0 for row in range(72, 76)}
    for i, (op, a) in defs.items():
        if op == 61:
            root, path = pointer(a[2])
            if root == target and len(path) >= 2 and path[0] == 0 and path[1] in row_reads:
                row_load(i, path[1])
                row_reads[path[1]] += 1
    if any(count != 1 for count in row_reads.values()):
        raise ValueError('Expected exactly four original projection row loads')
    pending = [signatures[0][1]] if signatures[0][0] == 'op_dot' else list(signatures[0][1])
    seen = set()
    while pending:
        i = pending.pop()
        if i in seen:
            continue
        if len(seen) >= MAX_INSTRUCTIONS:
            raise ValueError('Projection input walk budget')
        seen.add(i)
        op, a = defs.get(i, (None, []))
        if op == 61:
            root, path = pointer(a[2])
            if root == target and len(path) >= 2 and path[0] == 0 and path[1] in row_reads:
                raise ValueError('Projection input depends on the rewritten projection rows')
        elif op == 12:
            pending.extend(a[4:])
        elif op in (41, 42, 43, 44, 46, 48, 49, 50, 51):
            continue
        elif op in (81, 83):
            pending.append(a[2])
        elif op in (79, 82):
            pending.extend(a[2:4])
        elif op == 80 or op in camera_dataflow.ID_VALUES:
            pending.extend(a[2:])
        else:
            raise ValueError('Unresolved original projection input')
    uniform_rows = {tuple(sorted(row.items())) for output in routes['outputs']
                    if output['interface'] == {'builtin': 0} for row in output['uniform_rows']}
    return {'schema_version': 1, 'policy': 'closed_projection_depth_suffix_v1',
            'shader_sha256': hashlib.sha256(data).hexdigest(), 'suffix_kind': kind,
            'projection': {'set': 3, 'binding': 0, 'rows': [72, 73, 74, 75],
                           'byte_offset': 1152, 'byte_size': 64, 'dot_form': signatures[0][0]},
            'bias': bias_contract, 'min_instruction': 'GLSL.std.450.NMin' if kind == 'min_z_w' else None,
            'position_ids_diagnostic_only': {'variable': position, 'access_prefix': list(prefix),
                                           'vector_store_value': vector_store, 'final_components': final,
                                           'pre_suffix_components': original,
                                           'shared_dot_input': signatures[0][1]},
            'position_uniform_rows': [dict(row) for row in sorted(uniform_rows)],
            'static_suffix_contract_matched': True, 'pure_ssa_data_routes_resolved': True,
            'module_validation_performed': False, 'projection_rewrite_allowed': False,
            'metric_scale_compatibility_proven': False, 'runtime_execution_proven': False,
            'coverage_cause_proven': False, 'replay_allowed': False, 'game_stereo_submitted': False}


def analyze(data, policy):
    """Recognize original syntax only; ValueError refuses unmodeled forms."""
    data = _snapshot(data)
    try:
        return _analyze(data, policy)
    except (IndexError, KeyError, TypeError, RecursionError) as error:
        raise ValueError('Malformed or unresolved suffix module') from error


def validated(data, core, glsl, validator):
    """Validate and analyze one byte snapshot; never issue rewrite permission."""
    data = _snapshot(data)
    policy = Policy(core, glsl)
    check = policy.validate(data, validator)
    if not check['validated_non_output_external_writes_excluded']:
        raise ValueError('Invalid or unreviewed projection suffix shader')
    result = analyze(data, policy)
    result.update(module_validation_performed=True, grammar_sha256=policy.core.sha256,
                  glsl_grammar_sha256=policy.glsl.sha256, validator_sha256=check['validator_sha256'])
    return result
