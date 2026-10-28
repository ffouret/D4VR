import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import shader_inventory


def spv(*instructions):
    words = [0x07230203, 0x00010600, 0, 100, 0]
    for opcode, values in instructions:
        words.extend([(len(values) + 1) << 16 | opcode, *values])
    return struct.pack('<' + str(len(words)) + 'I', *words)


class ShaderTests(unittest.TestCase):
    def test_directx_stage_and_shader_model(self):
        part = b'DXIL' + struct.pack('<III', 8, (1 << 16) | 0x66, 2)
        data = b'DXBC' + bytes(20) + struct.pack('<III', 36 + len(part), 1, 36) + part
        result = shader_inventory.directx(data)
        self.assertEqual(result['stages'], ['vertex'])
        self.assertEqual(result['programs'][0]['shader_model'], '6.6')

    def test_directx_rejects_part_outside_container(self):
        data = b'DXBC' + bytes(20) + struct.pack('<III', 36, 1, 1024)
        with self.assertRaisesRegex(ValueError, 'offset'):
            shader_inventory.directx(data)

    def test_signature_distinguishes_position_from_system_value(self):
        data = struct.pack('<II6I', 1, 8, 0, 40, 0, 0, 3, 2) + bytes([7, 7, 0, 0]) + bytes(4) + b'POSITION\0'
        result = shader_inventory.signature(data)
        self.assertEqual(result[0]['semantic'], 'POSITION')
        self.assertEqual(result[0]['system_value'], 0)
        self.assertEqual(result[0]['mask'], 7)

    def test_signature_rejects_name_outside_part(self):
        data = struct.pack('<II6I', 1, 8, 0, 4096, 0, 0, 3, 2) + bytes(8)
        with self.assertRaisesRegex(ValueError, 'name'):
            shader_inventory.signature(data)

    def test_spirv_entry_and_declared_binding(self):
        data = spv((15, [0, 1, 0x6E69616D, 0]), (71, [20, 33, 4]), (71, [20, 34, 2]), (59, [10, 20, 2]))
        result = shader_inventory.spirv(data)
        self.assertEqual(result['stages'], ['vertex'])
        self.assertEqual(result['entries'][0]['name'], 'main')
        self.assertEqual(result['declared_resources'][0]['binding'], 4)
        self.assertEqual(result['declared_resources'][0]['descriptor_set'], 2)

    def test_spirv_rejects_zero_length_instruction(self):
        data = spv() + bytes(4)
        with self.assertRaisesRegex(ValueError, 'instruction'):
            shader_inventory.spirv(data)

    def test_spirv_rejects_truncated_instruction(self):
        data = spv((15, [0, 1, 0x6E69616D, 0]))[:-4]
        with self.assertRaisesRegex(ValueError, 'instruction'):
            shader_inventory.spirv(data)


if __name__ == '__main__':
    unittest.main()
