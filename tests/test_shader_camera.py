from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import shader_camera

FIXTURE = Path(__file__).resolve().parents[1] / "native/tests/camera_fixture.ll"


class CameraTraceTests(unittest.TestCase):
    def test_compiled_original_hlsl_recovers_layout(self):
        result = shader_camera.analyze_ir(FIXTURE.read_text())
        matrix = result["final_clip_matrix"]
        self.assertEqual(matrix["buffer"]["register"], 3)
        self.assertEqual(matrix["buffer"]["space"], 2)
        self.assertEqual(matrix["byte_offset"], 112)
        self.assertEqual(matrix["legacy_vector_registers"], [7, 8, 9, 10])
        self.assertFalse(result["live_camera_proven"])

    def test_position_signature_id_is_not_assumed_zero(self):
        text = FIXTURE.read_text()
        text = text.replace('i32 0, !"SV_Position"', 'i32 7, !"SV_Position"')
        text = text.replace('storeOutput.f32(i32 5, i32 0,', 'storeOutput.f32(i32 5, i32 7,')
        result = shader_camera.analyze_ir(text)
        self.assertEqual(result["position_signature_id"], 7)
        self.assertIsNotNone(result["final_clip_matrix"])

    def test_noncontiguous_rows_are_rejected(self):
        text, count = re.subn(r'(%dx.types.Handle %\d+, i32 )10\)', r'\g<1>12)', FIXTURE.read_text())
        self.assertEqual(count, 1)
        self.assertIsNone(shader_camera.analyze_ir(text)["final_clip_matrix"])

    def test_dynamic_vector_index_is_not_guessed(self):
        text, count = re.subn(r'(%dx.types.Handle %\d+, i32 )10\)', r'\g<1>%dynamic)', FIXTURE.read_text())
        self.assertEqual(count, 1)
        self.assertIsNone(shader_camera.analyze_ir(text)["final_clip_matrix"])

    def test_upstream_affine_transform_preserves_homogeneous_one(self):
        chain = shader_camera.analyze_ir(FIXTURE.read_text())["input_matrix_chain"]
        self.assertEqual(len(chain), 1)
        self.assertEqual(chain[0]["buffer"]["register"], 1)
        self.assertEqual(chain[0]["byte_offset"], 32)
        self.assertEqual(chain[0]["input_components"][-1], "1")

    def test_multiple_position_stores_are_not_combined_across_branches(self):
        text = FIXTURE.read_text()
        store = next(line for line in text.splitlines() if 'call void @dx.op.storeOutput.f32' in line)
        result = shader_camera.analyze_ir(text + '\n' + store)
        self.assertTrue(result["multiple_stores_per_component"])
        self.assertIsNone(result["final_clip_matrix"])

    def test_missing_position_metadata_does_not_guess_outputs(self):
        text = re.sub(r'^!\d+ = !\{i32 \d+, !"SV_Position".*$', '', FIXTURE.read_text(), flags=re.M)
        self.assertFalse(shader_camera.analyze_ir(text)["position_signature_resolved"])


if __name__ == '__main__':
    unittest.main()
