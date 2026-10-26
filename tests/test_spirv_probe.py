from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import spirv_probe

FIXTURE = Path(__file__).resolve().parents[1] / 'native/tests/probe_fixture.spv.hex'


class ObserverInstrumentationTests(unittest.TestCase):
    def setUp(self):
        self.data = bytes.fromhex(FIXTURE.read_text(encoding='ascii'))
        self.original = spirv_probe.Module(self.data)

    def test_original_stores_preserved_and_only_function_locals_added(self):
        observed = spirv_probe.Module(spirv_probe.instrument(self.data, 3, 0, 72))
        old_stores = [a for op, a in self.original.instructions if op == 62]
        new_stores = [a for op, a in observed.instructions if op == 62]
        for store in old_stores:
            self.assertIn(store, new_stores)
        new_variables = {a[1]: a[2] for op, a in observed.instructions
                         if op == 59 and a[1] >= self.original.header[3]}
        for store in new_stores:
            if store not in old_stores:
                self.assertEqual(new_variables[store[0]], 7)  # Function storage.
        imports = [a for op, a in observed.instructions if op == 71 and a[1] == 41]
        self.assertEqual(len(imports), 1)
        function = imports[0][0]
        calls = [a for op, a in observed.instructions if op == 57 and a[2] == function]
        self.assertEqual(len(calls), 1)
        self.assertEqual(len(calls[0][3:]), 9)  # vertex ID and eight float4 rows.

    def test_each_matrix_extent_is_checked(self):
        for first, second in [(110, 56), (72, 78), (-1, 56)]:
            with self.assertRaisesRegex(ValueError, 'extent'):
                spirv_probe.instrument(self.data, 3, 0, first, 1, second)

    def test_geometry_mode_adds_position_parameter_and_explicit_output_store(self):
        observed = spirv_probe.Module(spirv_probe.instrument(self.data, 3, 0, 72, geometry=True))
        imports = [a for op, a in observed.instructions if op == 71 and a[1] == 41]
        self.assertIn('D4VRGeometry_v1', spirv_probe.literal(imports[0][2:-1]))
        calls = [a for op, a in observed.instructions if op == 57 and a[2] == imports[0][0]]
        self.assertEqual(len(calls[0][3:]), 10)
        for op, args in self.original.instructions:
            if op == 62:
                self.assertIn((op, args), observed.instructions)
        position_stores = [args for op, args in observed.instructions if op == 62 and args[0] >= self.original.header[3]]
        pointers = {a[1]: a[0] for op, a in observed.instructions if op == 65}
        types = {a[0]: a[1:] for op, a in observed.instructions if op == 32}
        self.assertEqual(sum(types[pointers[a[0]]][0] == 3 for a in position_stores if a[0] in pointers), 1)

    def test_descriptor_binding_must_exist(self):
        with self.assertRaisesRegex(ValueError, 'one uniform buffer'):
            spirv_probe.instrument(self.data, 3, 9, 72)

    def test_existing_reserved_specialization_id_is_rejected(self):
        module = spirv_probe.Module(self.data)
        changed = False
        for op, args in module.instructions:
            if op == 71 and args[1:] == [1, 16]:
                args[2] = 0
                changed = True
        self.assertTrue(changed)
        with self.assertRaisesRegex(ValueError, 'reserved'):
            spirv_probe.instrument(module.binary(), 3, 0, 72)

    def test_nonvertex_entrypoint_is_rejected(self):
        module = spirv_probe.Module(self.data)
        for op, args in module.instructions:
            if op == 15:
                args[0] = 4
        with self.assertRaisesRegex(ValueError, 'vertex entry'):
            spirv_probe.instrument(module.binary(), 3, 0, 72)

    def test_stereo_mode_uses_its_own_abi_and_position_output(self):
        module = spirv_probe.Module(spirv_probe.instrument(self.data,3,0,72,stereo=True))
        imports = [a for op,a in module.instructions if op == 71 and a[1] == 41]
        self.assertEqual(spirv_probe.literal(imports[0][2:-1]),'D4VRStereo_v1')
        calls = [a for op,a in module.instructions if op == 57 and a[2] == imports[0][0]]
        self.assertEqual(len(calls[0][3:]),10)

    def test_eye_address_constant_collision_is_rejected_only_in_stereo_mode(self):
        module = spirv_probe.Module(self.data)
        for op,args in module.instructions:
            if op == 71 and args[1:] == [1,16]: args[2] = 4
        spirv_probe.instrument(module.binary(),3,0,72,geometry=True)
        with self.assertRaisesRegex(ValueError,'reserved'):
            spirv_probe.instrument(module.binary(),3,0,72,stereo=True)

    def test_projection_input_preserves_original_outputs_and_only_writes_function_locals(self):
        data=spirv_probe.instrument(self.data,3,0,72,stereo=True,projection_only_helper=True,projection_input=True)
        observed=spirv_probe.Module(data);old_stores=[a for op,a in self.original.instructions if op==62]
        variables={a[1]:a[2] for op,a in observed.instructions if op==59}
        for store in old_stores:self.assertIn((62,store),observed.instructions)
        for op,a in observed.instructions:
            if op==62 and a not in old_stores:self.assertEqual(variables[a[0]],7)
        function=next(a[0] for op,a in observed.instructions if op==71 and a[1]==41)
        self.assertEqual(sum(op==57 and a[2]==function for op,a in observed.instructions),4)
        # No late output write was added; original varying/Position computations
        # retain their IDs and all consume the invocation's transformed rows.
        self.assertEqual(sum(op==62 and a[0] not in variables for op,a in observed.instructions),
                         sum(op==62 and a[0] not in variables for op,a in self.original.instructions))

    def test_projection_input_is_explicit_and_requires_projection_only_mode(self):
        with self.assertRaisesRegex(ValueError,'requires'):
            spirv_probe.instrument(self.data,3,0,72,stereo=True,projection_input=True)

    def test_projection_input_does_not_guess_dynamic_camera_loads(self):
        module=spirv_probe.Module(self.data)
        camera=next(a[0] for op,a in module.instructions if op==71 and a[1:]==[33,0])
        vertex=next(a[0] for op,a in module.instructions if op==71 and a[1:]==[11,42])
        vertex_value=next(a[1] for op,a in module.instructions if op==61 and a[2]==vertex)
        for op,a in module.instructions:
            if op==65 and len(a)==5 and a[2]==camera:
                # A nonconstant index is outside the admitted static path.
                a[-1]=vertex_value;break
        with self.assertRaisesRegex(ValueError,'cannot bound'):
            spirv_probe.instrument(module.binary(),3,0,72,stereo=True,projection_only_helper=True,projection_input=True)

    def test_camera_block_mode_adds_no_original_buffer_or_output_stores(self):
        data=spirv_probe.instrument(self.data,3,0,0,camera_block_rows=113);m=spirv_probe.Module(data)
        old=[a for op,a in self.original.instructions if op==62];variables={a[1]:a[2] for op,a in m.instructions if op==59}
        for a in old:self.assertIn((62,a),m.instructions)
        for op,a in m.instructions:
            if op==62 and a not in old:self.assertEqual(variables[a[0]],7)
        imported=next(a[0] for op,a in m.instructions if op==71 and a[1]==41)
        self.assertEqual(spirv_probe.literal(next(a[2:-1] for op,a in m.instructions if op==71 and a[0]==imported)),'D4VRBlockProbe_v1')
        self.assertEqual(len(next(a[3:] for op,a in m.instructions if op==57 and a[2]==imported)),114)

    def test_camera_block_extent_and_mode_budget_are_closed(self):
        for rows,start,stereo in [(129,0,False),(113,1,False),(113,0,True)]:
            with self.assertRaises(ValueError):spirv_probe.instrument(self.data,3,0,start,stereo=stereo,camera_block_rows=rows)


if __name__ == '__main__':
    unittest.main()
