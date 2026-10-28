from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import geometry_report


def record(sample=0, depth=2., matched=True, constant_ndc_shift=False, change_depth=False):
    sx, sy, a, b = 1.784022, 3.171595, -1.00150225, -.300451
    if not matched:
        sx, sy = .75, .8
    before = [sx * .4 - .000439 * depth, sy * .1 + .000347 * depth, -a * depth + b, depth]
    after = before.copy()
    if matched:
        after[0] -= sx * .1
    if constant_ndc_shift:
        after[0] = before[0] - .01 * before[3]
    if change_depth:
        after[2] += .1
    rows = [before, after, [sx, sy, a, b], [float(matched), float(matched), .1, 0.]]
    return ''.join(f"Shader: abcd: Instance {sample}, ID ({geometry_report.MARKER}, {sample}, {row}): "
                   + ', '.join(f'{v:.6f}' for v in values) + '\n' for row, values in enumerate(rows))


class GeometryReportTests(unittest.TestCase):
    def parse(self, text):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory) / 'gpu.log'
            p.write_text(text)
            return geometry_report.parse(p)

    def test_measured_disparity_depends_on_depth(self):
        report = self.parse(record(depth=2) + record(sample=1, depth=4))
        self.assertEqual(report['verified_displaced_samples'], 2)
        self.assertAlmostEqual(report['samples'][0]['ndc_x_delta'] / report['samples'][1]['ndc_x_delta'], 2, places=5)
        self.assertTrue(report['numeric_geometry_shift_confirmed'])
        self.assertFalse(report['native_stereo_proven'])
        self.assertFalse(report['main_pass_identity_proven'])

    def test_far_vertex_rounding_is_not_treated_as_failed_geometry(self):
        report = self.parse(record(depth=199.9))
        self.assertEqual(report['inconsistent_samples'], 0)
        self.assertEqual(report['verified_displaced_samples'], 1)

    def test_constant_screen_shift_is_rejected_as_inconsistent(self):
        report = self.parse(record(constant_ndc_shift=True))
        self.assertEqual(report['inconsistent_samples'], 1)
        self.assertFalse(report['numeric_geometry_shift_confirmed'])

    def test_non_target_vertices_are_verified_unchanged(self):
        report = self.parse(record(matched=False))
        self.assertEqual(report['verified_unchanged_non_target_samples'], 1)
        self.assertFalse(report['numeric_geometry_shift_confirmed'])

    def test_position_change_without_matching_projection_is_rejected(self):
        report = self.parse(record(matched=False, constant_ndc_shift=True))
        self.assertEqual(report['inconsistent_samples'], 1)

    def test_partial_or_duplicate_telemetry_never_proves_a_displacement(self):
        lines = record().splitlines(keepends=True)
        for text in (''.join(lines[:3]), ''.join(lines + lines[:1])):
            result = self.parse(text)
            self.assertEqual(result['complete_geometry_samples'], 0)
            self.assertFalse(result['numeric_geometry_shift_confirmed'])

    def test_changed_depth_is_rejected(self):
        report = self.parse(record(change_depth=True))
        self.assertEqual(report['inconsistent_samples'], 1)
        self.assertIn('unexpected_y_z_or_w_change', report['samples'][0]['errors'])

    def test_bad_status_is_rejected(self):
        report = self.parse(record().replace('1.000000, 1.000000, 0.100000, 0.000000',
                                            '2.000000, 1.000000, 0.100000, 0.000000'))
        self.assertEqual(report['inconsistent_samples'], 1)


if __name__ == '__main__':
    unittest.main()
