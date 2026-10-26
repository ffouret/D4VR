#!/usr/bin/env python3
"""Prepare a VKD3D matrix observer or an explicit geometry displacement test.

This builds a separate SPIR-V override. It never writes game/NVIDIA libraries.
The result must pass spirv-val and a GPU fixture before any game launch.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def string_words(value: str) -> list[int]:
    data = value.encode("utf-8") + b"\0"
    data += bytes((-len(data)) % 4)
    return list(struct.unpack("<" + str(len(data) // 4) + "I", data))


def literal(words) -> str:
    return struct.pack("<" + str(len(words)) + "I", *words).split(b"\0", 1)[0].decode("utf-8")


class Module:
    def __init__(self, data: bytes):
        if len(data) < 20 or len(data) % 4 or data[:4] != b"\x03\x02\x23\x07":
            raise ValueError("Expected little-endian SPIR-V")
        words = list(struct.unpack("<" + str(len(data) // 4) + "I", data))
        self.header = words[:5]
        self.instructions = []
        index = 5
        while index < len(words):
            count, opcode = words[index] >> 16, words[index] & 0xFFFF
            if count < 1 or index + count > len(words):
                raise ValueError("Invalid SPIR-V instruction extent")
            self.instructions.append((opcode, words[index + 1:index + count]))
            index += count

    def allocate(self):
        identifier = self.header[3]
        self.header[3] += 1
        return identifier

    def binary(self):
        words = self.header.copy()
        for opcode, arguments in self.instructions:
            words += [((len(arguments) + 1) << 16) | opcode, *arguments]
        return struct.pack("<" + str(len(words)) + "I", *words)

    def add_capability(self, capability):
        if (17, [capability]) not in self.instructions:
            index = next(i for i, (op, _) in enumerate(self.instructions) if op != 17)
            self.instructions.insert(index, (17, [capability]))

    def decorate(self, instructions):
        index = next(i for i, (op, _) in enumerate(self.instructions) if 19 <= op <= 39)
        self.instructions[index:index] = instructions


def helper_library(data: bytes, geometry: bool = False, stereo: bool = False, camera_block_rows: int = 0) -> bytes:
    module = Module(data)
    names = {args[0]: literal(args[1:]) for op, args in module.instructions if op == 5}
    targets = [identifier for identifier, name in names.items() if name.startswith("d4vr_stereo(" if stereo else "d4vr_geometry(" if geometry else "d4vr_probe(")]
    if len(targets) != 1:
        raise ValueError("Expected one D4VR helper compiled without inlining")
    module.instructions = [(op, args) for op, args in module.instructions if op not in (15, 16, 331)]
    module.add_capability(5)  # Linkage, intermediate library only.
    module.decorate([(71, [targets[0], 41, *string_words("D4VRBlockProbe_v1" if camera_block_rows else "D4VRStereo_v1" if stereo else "D4VRGeometry_v1" if geometry else "D4VRProbe_v1"), 0])])
    return module.binary()


def instrument(data: bytes, descriptor_set: int, binding: int, first_row: int, secondary_binding: int = 1, secondary_row: int = 56, geometry: bool = False, stereo: bool = False, projection_only_helper: bool = False, projection_input: bool = False, camera_block_rows: int = 0, fixed_partner_projection: bool = False) -> bytes:
    if fixed_partner_projection and not projection_input:raise ValueError('Fixed partner projection requires early projection input')
    if camera_block_rows and (not 4<=camera_block_rows<=128 or geometry or stereo or projection_only_helper or projection_input):raise ValueError('Camera block observer mode/row budget')
    if projection_only_helper and not stereo:raise ValueError('Projection-only helper requires stereo mode')
    if projection_input and not projection_only_helper:raise ValueError('Projection input requires the projection-only helper')
    geometry = geometry or stereo
    module = Module(data)
    entries = [args for op, args in module.instructions if op == 15]
    if len(entries) != 1 or entries[0][0] != 0:
        raise ValueError("Only a single vertex entry point is supported")
    main = entries[0][1]
    decorations = {}
    member_offsets = {}
    position_members = set()
    types = {}
    variables = {}
    constants = {}
    for op, args in module.instructions:
        if op == 71:
            decorations.setdefault(args[0], {})[args[1]] = args[2:]
            if args[1] == 1 and args[2] in ((0, 1, 2, 3, 4) if stereo else (0, 1, 2, 3)):
                raise ValueError("Original shader uses reserved debug-ring specialization IDs")
        elif op == 72 and args[2] == 35:
            member_offsets[(args[0], args[1])] = args[3]
        elif op == 72 and args[2:] == [11, 0]:
            position_members.add((args[0], args[1]))
        elif 19 <= op <= 39:
            types[args[0]] = (op, args[1:])
        elif op == 59:
            variables[args[1]] = (args[0], args[2])
        elif op == 43:
            constants[args[1]] = (args[0], args[2:])
    def uniform_source(selected_set, selected_binding, row):
        targets = [identifier for identifier in variables if decorations.get(identifier, {}).get(34) == [selected_set]
                   and decorations.get(identifier, {}).get(33) == [selected_binding]]
        if len(targets) != 1:
            raise ValueError("Expected one uniform buffer at the selected descriptor set/binding")
        target = targets[0]
        pointer_type, storage = variables[target]
        if storage != 2 or types[pointer_type][0] != 32:
            raise ValueError("Selected descriptor is not a uniform buffer")
        struct_type = types[pointer_type][1][1]
        struct_op, members = types[struct_type]
        if struct_op != 30 or len(members) != 1 or member_offsets.get((struct_type, 0)) != 0:
            raise ValueError("Expected one array at byte offset zero in the uniform block")
        array_type = members[0]
        array_op, array = types[array_type]
        if array_op != 28 or decorations.get(array_type, {}).get(6) != [16]:
            raise ValueError("Expected a fixed float4 array with a 16-byte stride")
        vector_type, array_length_id = array
        vector_op, vector_args = types[vector_type]
        if vector_op != 23 or vector_args[1] != 4 or types[vector_args[0]] != (22, [32]):
            raise ValueError("Expected four 32-bit floats per vector")
        length = constants.get(array_length_id, (0, []))[1]
        if len(length) != 1 or row < 0 or row + 4 > length[0]:
            raise ValueError("Matrix extent outside the original uniform buffer")
        return target, vector_type

    target, vector_type = uniform_source(descriptor_set, binding, first_row)
    if fixed_partner_projection:uniform_source(descriptor_set,binding,80)
    if camera_block_rows:
        uniform_source(descriptor_set,binding,first_row+camera_block_rows-4)
    elif not projection_only_helper:
        second_target, second_vector_type = uniform_source(descriptor_set, secondary_binding, secondary_row)
        if vector_type != second_vector_type:
            raise ValueError("Both observed buffers must use the same float4 type")
    declarations = []

    def type_id(opcode, arguments):
        for identifier, declaration in types.items():
            if declaration == (opcode, arguments):
                return identifier
        identifier = module.allocate()
        types[identifier] = (opcode, arguments)
        declarations.append((opcode, [identifier, *arguments]))
        return identifier

    def constant(value):
        for identifier, definition in constants.items():
            if definition == (uint, [value]):
                return identifier
        identifier = module.allocate()
        constants[identifier] = (uint, [value])
        declarations.append((43, [uint, identifier, value]))
        return identifier

    uint = type_id(21, [32, 0])
    void = type_id(19, [])
    uniform_vector_pointer = type_id(32, [2, vector_type])
    local_uint_pointer = type_id(32, [7, uint])
    local_vector_pointer = type_id(32, [7, vector_type])
    vector_arguments = camera_block_rows or (9 if geometry else 8)
    function_type = type_id(33, [void, local_uint_pointer, *([local_vector_pointer] * vector_arguments)])
    position_output = None
    position_member = None
    if geometry:
        positions = [identifier for identifier in variables if decorations.get(identifier, {}).get(11) == [0]]
        members = [(identifier, index) for identifier, (pointer, storage) in variables.items() if storage == 3
                   for struct, index in position_members if types[pointer] == (32, [3, struct])]
        if len(positions) + len(members) != 1:
            raise ValueError("Expected one Position output")
        if positions:
            position_output = positions[0]
            if types[variables[position_output][0]] != (32, [3, vector_type]):
                raise ValueError("Expected a float4 Position output")
        else:
            position_output, position_member = members[0]
            struct = types[variables[position_output][0]][1][1]
            if types[struct][1][position_member] != vector_type:
                raise ValueError("Expected a float4 Position member")
        output_vector_pointer = type_id(32, [3, vector_type])
    vertex_inputs = [identifier for identifier in variables if decorations.get(identifier, {}).get(11) == [42]]
    added_decorations = []
    if len(vertex_inputs) > 1:
        raise ValueError("Ambiguous VertexIndex builtin")
    if vertex_inputs:
        vertex = vertex_inputs[0]
        vertex_pointer = variables[vertex][0]
        vertex_type = types[vertex_pointer][1][1]
        if types[vertex_type] not in ((21, [32, 0]), (21, [32, 1])):
            raise ValueError("Unsupported VertexIndex type")
    else:
        vertex_type = uint
        input_pointer = type_id(32, [1, uint])
        vertex = module.allocate()
        declarations.append((59, [input_pointer, vertex, 1]))
        added_decorations.append((71, [vertex, 11, 42]))
        entries[0].append(vertex)
    zero = constant(0)
    if projection_only_helper:
        unused_vector=module.allocate()
        declarations.append((46,[vector_type,unused_vector]))  # Defined zero, never a guessed matrix.
    position_index = constant(position_member) if position_member is not None else None
    row_indices = [(target, constant(first_row + i)) for i in range(camera_block_rows or 4)]
    if not projection_only_helper and not camera_block_rows:
        row_indices += [(second_target, constant(secondary_row + i)) for i in range(4)]
    imported = module.allocate()
    parameters = [module.allocate() for _ in range(vector_arguments + 1)]
    added_decorations.append((71, [imported, 41, *string_words("D4VRBlockProbe_v1" if camera_block_rows else "D4VRStereo_v1" if stereo else "D4VRGeometry_v1" if geometry else "D4VRProbe_v1"), 1]))
    import_body = [(54, [void, imported, 0, function_type])]
    import_body += [(55, [local_uint_pointer if i == 0 else local_vector_pointer, identifier])
                    for i, identifier in enumerate(parameters)]
    import_body.append((56, []))
    locals_ = [module.allocate() for _ in range(vector_arguments + 1)]
    local_declarations = [(59, [local_uint_pointer if i == 0 else local_vector_pointer, identifier, 7])
                          for i, identifier in enumerate(locals_)]

    def call_body():
        code = []
        value = module.allocate()
        code.append((61, [vertex_type, value, vertex]))
        if vertex_type != uint:
            converted = module.allocate()
            code.append((124, [uint, converted, value]))
            value = converted
        code.append((62, [locals_[0], value]))
        for i, (buffer, row) in enumerate(row_indices):
            pointer, value = module.allocate(), module.allocate()
            code.append((65, [uniform_vector_pointer, pointer, buffer, zero, row]))
            code.append((61, [vector_type, value, pointer]))
            code.append((62, [locals_[i + 1], value]))
        if projection_only_helper:
            code += [(62,[local,unused_vector]) for local in locals_[5:9]]
        if geometry:
            position_pointer = position_output
            if position_index is not None:
                position_pointer = module.allocate()
                code.append((65, [output_vector_pointer, position_pointer, position_output, position_index]))
            value = module.allocate()
            code += [(61, [vector_type, value, position_pointer]), (62, [locals_[-1], value])]
        code.append((57, [void, module.allocate(), imported, *locals_]))
        if geometry:
            value = module.allocate()
            code += [(61, [vector_type, value, locals_[-1]]), (62, [position_pointer, value])]
        return code

    if projection_input:
        if fixed_partner_projection:
            from fixed_partner_projection import rewrite
        else:
            from projection_input import rewrite
        output=rewrite(module,main,target,first_row,vector_type,types,constants,locals_,local_declarations,
            imported,void,uint,zero,unused_vector,uniform_vector_pointer,type_id,constant)
        module.instructions=output
        module.add_capability(5);module.decorate(added_decorations)
        first_function=next(i for i,(op,_) in enumerate(module.instructions) if op==54)
        module.instructions[first_function:first_function]=declarations+import_body
        return module.binary()

    output = []
    function = None
    locals_inserted = False
    calls = 0
    if camera_block_rows:bool_type=type_id(20,[])
    for op, args in module.instructions:
        if op == 54:
            function = args[1]
        if function == main and op == 253:
            if camera_block_rows:
                # Do not load the neighborhood for every vertex only to have
                # the helper reject it. The added selection rejoins before
                # the original return, including inside original branches.
                value,condition,chosen,merge=[module.allocate() for _ in range(4)]
                output.append((61,[vertex_type,value,vertex]))
                if vertex_type!=uint:
                    converted=module.allocate();output.append((124,[uint,converted,value]));value=converted
                output.extend([(170,[bool_type,condition,value,zero]),(247,[merge,0]),(250,[condition,chosen,merge]),(248,[chosen])])
                output.extend(call_body());output.extend([(249,[merge]),(248,[merge])])
            else:output.extend(call_body())
            calls += 1
        output.append((op, args))
        if function == main and op == 248 and not locals_inserted:
            output.extend(local_declarations)
            locals_inserted = True
        if op == 56:
            function = None
    if not locals_inserted or not calls:
        raise ValueError("No normal return in the selected vertex entry point")
    module.instructions = output
    module.add_capability(5)
    module.decorate(added_decorations)
    first_function = next(i for i, (op, _) in enumerate(module.instructions) if op == 54)
    module.instructions[first_function:first_function] = declarations + import_body
    return module.binary()


def build(original: Path, helper: Path, output: Path, tools: Path, descriptor_set: int, binding: int, first_row: int, secondary_binding: int = 1, secondary_row: int = 56, geometry: bool = False, stereo: bool = False, helper_no_telemetry: bool = False, projection_only_helper: bool = False, projection_input: bool = False, camera_block_rows: int = 0, fixed_partner_projection: bool = False):
    if projection_input and not (projection_only_helper and helper_no_telemetry):raise ValueError('Projection input requires a validated projection-only telemetry-free helper')
    if projection_only_helper and not (stereo and helper_no_telemetry):
        raise ValueError('Projection-only helper requires stereo and telemetry-free validation')
    if helper_no_telemetry:
        if not stereo:raise ValueError('Telemetry-free helper requires stereo mode')
    helper_data=helper.read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    scratch = output.parent / "intermediate" / output.stem
    scratch.mkdir(parents=True, exist_ok=True)
    library = scratch / "helper.spv"
    caller = scratch / "caller.spv"
    linked = scratch / "linked.spv"
    if helper_no_telemetry:
        from stereo_helper import verify,verify_projection_only
        with tempfile.TemporaryDirectory(prefix='helper-validation-',dir=scratch) as directory:
            snapshot=Path(directory)/'helper.spv';snapshot.write_bytes(helper_data)
            subprocess.run([str(tools / 'spirv-val'), '--target-env', 'vulkan1.2', str(snapshot)], check=True, capture_output=True)
        (verify_projection_only if projection_only_helper else verify)(helper_data)
    library.write_bytes(helper_library(helper_data, geometry, stereo,camera_block_rows))
    caller.write_bytes(instrument(original.read_bytes(), descriptor_set, binding, first_row, secondary_binding, secondary_row, geometry, stereo, projection_only_helper,projection_input,camera_block_rows,fixed_partner_projection))
    subprocess.run([str(tools / "spirv-link"), "--target-env", "spv1.3", str(caller), str(library), "-o", str(linked)], check=True, capture_output=True)
    subprocess.run([str(tools / "spirv-opt"), "--eliminate-dead-functions", "--eliminate-dead-variables", str(linked), "-o", str(output)], check=True, capture_output=True)
    if stereo:
        module = Module(output.read_bytes())
        index = next(i for i, (op, _) in enumerate(module.instructions) if op in (71, 72) or 19 <= op <= 39)
        module.instructions.insert(index, (330, string_words("D4VR_STEREO_ABI_1")))
        if helper_no_telemetry:
            from stereo_helper import MARKER
            module.instructions.insert(index+1, (330, string_words(MARKER)))
        if projection_only_helper:
            from stereo_helper import PROJECTION_MARKER
            module.instructions.insert(index+2, (330, string_words(PROJECTION_MARKER)))
        if projection_input:module.instructions.insert(index+3,(330,string_words('D4VR_PROJECTION_INPUT_1')))
        if fixed_partner_projection:module.instructions.insert(index+4,(330,string_words('D4VR_FIXED_PARTNER_PROJECTION_1')))
        output.write_bytes(module.binary())
    subprocess.run([str(tools / "spirv-val"), "--target-env", "vulkan1.2", str(output)], check=True, capture_output=True)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--tools", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--set", type=int, default=3, dest="descriptor_set")
    parser.add_argument("--binding", type=int, default=0)
    parser.add_argument("--first-row", type=int, default=72)
    parser.add_argument("--secondary-binding", type=int, default=1)
    parser.add_argument("--secondary-row", type=int, default=56)
    parser.add_argument("--geometry", action="store_true", help="Apply the diagnostic vertex displacement helper")
    parser.add_argument("--stereo", action="store_true", help="Apply the per-eye transform helper controlled by specialization ID 4")
    parser.add_argument("--helper-no-telemetry", action="store_true", help="Require a validated telemetry-free stereo helper; does not audit the original shader")
    parser.add_argument("--projection-only-helper", action="store_true", help="Omit added model-view buffer reads after proving that the helper's four object parameters are unused")
    parser.add_argument('--projection-input',action='store_true',help='Transform canonical projection loads before all original vertex computations; diagnostic only')
    parser.add_argument('--camera-block-rows',type=int,default=0,help='Read a bounded camera neighborhood through the matching block-observer helper')
    parser.add_argument('--fixed-partner-projection',action='store_true',help='Owned diagnostic: one fixed transform for source projections 72 and 80; no temporal XR history')
    args = parser.parse_args()
    print(build(args.original, args.helper, args.output, args.tools, args.descriptor_set, args.binding, args.first_row, args.secondary_binding, args.secondary_row, args.geometry, args.stereo, args.helper_no_telemetry, args.projection_only_helper,args.projection_input,args.camera_block_rows,args.fixed_partner_projection))


if __name__ == "__main__":
    main()
