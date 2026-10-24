from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import camera_capture
import spirv_probe


def message(sample, row, values="1.0, 0.0, 0.0, 1.0", marker=camera_capture.MARKER):
    return f"info: Shader: abcd: Instance {sample:010d}, ID ({marker}, {sample}, {row}): {values}\n"


class CameraLogTests(unittest.TestCase):
    def parse(self, text):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "gpu.log"
            log.write_text(text, encoding="utf-8")
            return camera_capture.parse(log)

    def test_four_rows_form_one_typed_sample(self):
        result = self.parse(''.join(message(0, row) for row in range(4)))
        self.assertEqual(result['complete_matrix_samples'], 1)
        self.assertEqual(result['samples'][0]['shader_hash'], '000000000000abcd')
        self.assertFalse(result['scene_camera_proven'])
        self.assertFalse(result['native_stereo_proven'])

    def test_partial_and_other_debug_messages_are_not_camera_samples(self):
        result = self.parse(message(0, 0) + message(0, 1, marker=7))
        self.assertEqual(result['complete_matrix_samples'], 0)
        self.assertEqual(result['incomplete_or_duplicate_groups'], 1)

    def test_duplicate_rows_cannot_mix_invocations(self):
        result = self.parse(''.join(message(0, row) for row in [0, 1, 2, 3, 0]))
        self.assertEqual(result['complete_matrix_samples'], 0)
        self.assertEqual(result['rejected_rows'], 1)

    def test_device_restart_separates_reused_sample_ids(self):
        text = ''.join(message(0, row) for row in range(4))
        text += 'Enabling shader debug ring of size: 16777216.\n'
        text += ''.join(message(0, row) for row in range(4))
        self.assertEqual(self.parse(text)['complete_matrix_samples'], 2)

    def test_nonfinite_rows_are_rejected(self):
        result = self.parse(message(0, 0, 'nan, 0, 0, 1'))
        self.assertEqual(result['rejected_rows'], 1)
        self.assertEqual(result['complete_matrix_samples'], 0)

    def test_sample_budget_is_enforced_when_parsing(self):
        self.assertEqual(self.parse(message(1024, 0))['rejected_rows'], 1)

    def test_paired_capture_requires_all_eight_rows_from_one_invocation(self):
        text = ''.join(message(2, row, marker=camera_capture.PAIRED_MARKER) for row in range(8))
        result = self.parse(text)
        self.assertEqual(result['paired_matrix_samples'], 1)
        self.assertEqual(len(result['samples'][0]['input_transform_rows']), 4)
        self.assertEqual(self.parse(text.rsplit('\n', 2)[0] + '\n')['complete_matrix_samples'], 0)

    def test_legacy_and_paired_markers_cannot_complete_each_other(self):
        text = ''.join(message(0, row) for row in range(4))
        text += ''.join(message(0, row, marker=camera_capture.PAIRED_MARKER) for row in range(4, 8))
        result = self.parse(text)
        self.assertEqual(result['complete_matrix_samples'], 1)
        self.assertEqual(result['paired_matrix_samples'], 0)

    def test_perspective_shape_has_fov_without_claiming_scene_identity(self):
        matrix = [[2, 0, .001, 0], [0, 4, -.001, 0], [0, 0, -1.1, -.11], [0, 0, -1, 0]]
        result = camera_capture.describe_projection(matrix)
        self.assertEqual(result['shape'], 'right_handed_perspective')
        self.assertEqual(result['aspect_ratio'], 2)
        self.assertAlmostEqual(result['near_far_if_depth_zero_to_one'][0], .1)
        matrix[0][1] = .2
        self.assertEqual(camera_capture.describe_projection(matrix)['shape'], 'unclassified')

    def test_spirv_rejects_truncated_or_zero_length_instructions(self):
        header = bytes.fromhex('0302230700030100000000000200000000000000')
        for data in [header[:8], header + bytes(4)]:
            with self.assertRaises(ValueError):
                spirv_probe.Module(data)


if __name__ == '__main__':
    unittest.main()
