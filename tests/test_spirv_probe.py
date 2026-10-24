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


if __name__ == '__main__':
    unittest.main()
