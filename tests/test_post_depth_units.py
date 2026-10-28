"""Independent exact-arithmetic oracles for postprojection depth suffixes.

These tests execute no shader, constructor, driver or game. They establish why
source clip units matter and define owned reference cases; the helper's compiled
control flow and packet admission require their separate SPIR-V/GPU controls.
"""
from fractions import Fraction as Q
import math
import struct
import unittest


def matvec(matrix, vector):
    return tuple(sum(a*b for a, b in zip(row, vector)) for row in matrix)


def inverse(matrix):
    # Generic exact elimination, independent of the helper's closed-form
    # sparse perspective reconstruction.
    rows = [[Q(value) for value in row]+[Q(i == j) for j in range(4)]
            for i, row in enumerate(matrix)]
    for column in range(4):
        pivot = next(i for i in range(column, 4) if rows[i][column])
        rows[column], rows[pivot] = rows[pivot], rows[column]
        divisor = rows[column][column]
        rows[column] = [value/divisor for value in rows[column]]
        for i in range(4):
            if i == column:
                continue
            factor = rows[i][column]
            rows[i] = [a-factor*b for a, b in zip(rows[i], rows[column])]
    return tuple(tuple(row[4:]) for row in rows)


def divide_clip(clip, scale):
    return tuple(value/scale for value in clip)


def projection(fx=Q(2), fy=Q(3), jitter_x=Q(1, 97), jitter_y=Q(-1, 101),
               a=Q(-800, 797), b=Q(-240, 797)):
    return ((fx, 0, jitter_x, 0), (0, fy, jitter_y, 0),
            (0, 0, a, b), (0, 0, -1, 0))


def metric_projection(optics, source, scale):
    # Independent target reference: optical X/Y, source A and scaled source B.
    return (optics[0], optics[1], (0, 0, source[2][2], source[2][3]*scale),
            (0, 0, -1, 0))


def metric_eye(scale, yaw=False, translation=(Q(0), Q(0), Q(0))):
    # A rational proper rotation avoids a trigonometric/rounding oracle.
    cosine, sine = (Q(3, 5), Q(4, 5)) if yaw else (Q(1), Q(0))
    x, y, z = translation
    return ((scale*cosine, 0, scale*sine, x), (0, scale, 0, y),
            (-scale*sine, 0, scale*cosine, z), (0, 0, 0, 1))


def source_unit_eye(eye, scale):
    return tuple(tuple(value/scale if row < 3 else value for value in values)
                 for row, values in enumerate(eye))


def bias_suffix(clip, k):
    x, y, z, w = clip
    return (x, y, z+k/w, w)


def upper_suffix(clip):
    x, y, z, w = clip
    # Finite real-arithmetic NMin oracle only, not NaN/signed-zero semantics.
    return (x, y, min(z, w), w)


def ndc(clip):
    return tuple(value/clip[3] for value in clip[:3])


def finite_reference_bias(clip, k, minimum_positive_w=1e-6):
    """Explicit reference-domain guard, not a claim of a new GPU-helper guard."""
    if not all(math.isfinite(float(value)) for value in (*clip, k)):
        raise ValueError('Nonfinite reference input')
    if clip[3] < minimum_positive_w:
        raise ValueError('Reference W is outside its declared positive domain')
    result = bias_suffix(clip, k)
    if not all(math.isfinite(float(value)) for value in (*result, *ndc(result))):
        raise ValueError('Nonfinite reference output')
    return result


class SourceClipUnitsTests(unittest.TestCase):
    scales = (Q(1, 20), Q(1, 10), Q(1), Q(3))

    def test_pure_scale_restores_all_original_clip_components_exactly(self):
        source = projection()
        for scale in self.scales:
            target = metric_projection(source, source, scale)
            eye = metric_eye(scale)
            for point in ((Q(1, 4), Q(-1, 8), Q(-2), Q(1)),
                          (Q(-2), Q(3), Q(-20), Q(2))):
                with self.subTest(scale=scale, point=point):
                    original = matvec(source, point)
                    metric = matvec(target, matvec(eye, point))
                    self.assertEqual(metric, tuple(scale*x for x in original))
                    restored = divide_clip(metric, scale)
                    self.assertEqual(restored, original)
                    self.assertEqual(bias_suffix(restored, Q(33, 100)),
                                     bias_suffix(original, Q(33, 100)))

    def test_naive_metric_suffix_has_inverse_square_scale_error(self):
        clip = (Q(1, 4), Q(1, 8), Q(3, 2), Q(2))
        k = Q(33, 100)
        for scale in self.scales:
            with self.subTest(scale=scale):
                metric = tuple(scale*x for x in clip)
                naive = ndc(bias_suffix(metric, k))[2]
                wanted = ndc(bias_suffix(clip, k))[2]
                self.assertEqual(naive, clip[2]/clip[3]+k/(scale**2*clip[3]**2))
                self.assertEqual(naive-wanted, k/clip[3]**2*(1/scale**2-1))
                self.assertEqual(ndc(bias_suffix(divide_clip(metric, scale), k))[2], wanted)
                if scale != 1:
                    self.assertNotEqual(naive, wanted)
                # Zero bias cannot distinguish old/new source clip units.
                self.assertEqual(ndc(bias_suffix(metric, 0)), ndc(bias_suffix(clip, 0)))

    def test_reference_optics_divided_by_scale_equals_source_unit_model(self):
        source = projection()
        optics = projection(fx=Q(5, 4), fy=Q(7, 5), jitter_x=Q(-1, 11),
                            jitter_y=Q(1, 13))
        for scale in self.scales:
            for translated in (False, True):
                translation = (Q(1, 32), Q(-1, 64), Q(1, 20)) if translated else (0, 0, 0)
                eye = metric_eye(scale, yaw=True, translation=translation)
                metric_p = metric_projection(optics, source, scale)
                reference_p = tuple(tuple(value/scale for value in row) for row in metric_p)
                source_p = metric_projection(optics, source, Q(1))
                source_eye = source_unit_eye(eye, scale)
                for point in ((Q(1), Q(1, 2), Q(-2), Q(1)),
                              (Q(-1), Q(-1, 2), Q(-6), Q(2))):
                    with self.subTest(scale=scale, translated=translated, point=point):
                        metric_clip = matvec(metric_p, matvec(eye, point))
                        reference = matvec(reference_p, matvec(eye, point))
                        independent = matvec(source_p, matvec(source_eye, point))
                        self.assertEqual(divide_clip(metric_clip, scale), reference)
                        self.assertEqual(reference, independent)
                        for suffix in (lambda c: bias_suffix(c, Q(7, 100)), upper_suffix):
                            self.assertEqual(suffix(divide_clip(metric_clip, scale)),
                                             suffix(independent))

    def test_uniform_rescaling_is_required_not_a_z_only_correction(self):
        clip = (Q(1, 4), Q(-1, 2), Q(3, 2), Q(2))
        scale, k = Q(1, 10), Q(33, 100)
        metric = tuple(scale*x for x in clip)
        z_only = (metric[0], metric[1], metric[2]/scale, metric[3])
        self.assertNotEqual(ndc(bias_suffix(z_only, k)), ndc(bias_suffix(clip, k)))
        self.assertEqual(ndc(bias_suffix(divide_clip(metric, scale), k)),
                         ndc(bias_suffix(clip, k)))

    def test_zero_w_projection_columns_are_valid_linear_transform_inputs(self):
        source, scale = projection(), Q(1, 10)
        source_inverse = inverse(source)
        eye = metric_eye(scale, yaw=True, translation=(Q(1, 32), Q(0), Q(1, 20)))
        target = metric_projection(projection(fx=Q(5, 4)), source, scale)
        basis = [tuple(Q(i == j) for i in range(4)) for j in range(4)]
        source_columns = [matvec(source, point) for point in basis]
        self.assertTrue(any(column[3] == 0 for column in source_columns))
        transformed = []
        for point, column in zip(basis, source_columns):
            recovered = matvec(source_inverse, column)
            self.assertEqual(recovered, point)
            transformed.append(divide_clip(matvec(target, matvec(eye, recovered)), scale))
        point = (Q(1, 4), Q(-1, 2), Q(-2), Q(1))
        recombined = tuple(sum(transformed[column][row]*point[column] for column in range(4))
                           for row in range(4))
        reference = divide_clip(matvec(target, matvec(eye, point)), scale)
        self.assertEqual(recombined, reference)
        self.assertEqual(bias_suffix(recombined, Q(33, 100)),
                         bias_suffix(reference, Q(33, 100)))

    def test_nmin_is_homogeneous_for_positive_scale_and_is_no_lower_clamp(self):
        for z, w in ((Q(1), Q(2)), (Q(3), Q(2)), (Q(2), Q(2)),
                     (Q(-1), Q(2)), (Q(-3), Q(-2))):
            clip = (Q(1), Q(-1), z, w)
            for scale in self.scales:
                with self.subTest(z=z, w=w, scale=scale):
                    scaled = tuple(scale*x for x in clip)
                    self.assertEqual(upper_suffix(scaled),
                                     tuple(scale*x for x in upper_suffix(clip)))
                    self.assertEqual(ndc(upper_suffix(scaled)), ndc(upper_suffix(clip)))
            if w > 0:
                self.assertEqual(ndc(upper_suffix(clip))[2], min(z/w, 1))
        self.assertLess(ndc(upper_suffix((0, 0, Q(-1), Q(2))))[2], 0)
        # Positivity matters; negative scale reverses min/max ordering.
        clip = (Q(0), Q(0), Q(3), Q(2))
        self.assertNotEqual(upper_suffix(tuple(-x for x in clip)),
                            tuple(-x for x in upper_suffix(clip)))

    def test_nmin_alone_cannot_certify_the_new_units_mode(self):
        clip = (Q(1, 4), Q(1, 8), Q(3), Q(2))
        scale = Q(1, 10)
        metric = tuple(scale*x for x in clip)
        self.assertEqual(ndc(upper_suffix(metric)), ndc(upper_suffix(divide_clip(metric, scale))))
        self.assertNotEqual(ndc(bias_suffix(metric, Q(1, 4))),
                            ndc(bias_suffix(divide_clip(metric, scale), Q(1, 4))))

    def test_post_depth_suffix_breaks_final_position_unprojection(self):
        source = projection(a=Q(-10, 9), b=Q(-10, 9))
        for kind, point in (('bias', (Q(1, 2), Q(1, 4), Q(-2), Q(1))),
                            ('nmin', (Q(1, 2), Q(1, 4), Q(-20), Q(1)))):
            clip = matvec(source, point)
            post = bias_suffix(clip, Q(33, 100)) if kind == 'bias' else upper_suffix(clip)
            recovered_w = (post[2]+source[2][2]*post[3])/source[2][3]
            self.assertNotEqual(recovered_w, point[3])
            # A requested X translation multiplies this wrong homogeneous W.
            self.assertNotEqual(Q(1, 2)*recovered_w, Q(1, 2)*point[3])

    def test_small_positive_w_amplifies_bias_even_after_correct_unit_restore(self):
        k = Q(33, 100)
        for w in (Q(1, 1000), Q(1, 1000000)):
            clip = (Q(0), Q(0), Q(0), w)
            contribution = ndc(bias_suffix(clip, k))[2]
            self.assertEqual(contribution, k/w**2)
            self.assertGreaterEqual(contribution, 330000)
        # The new mode restores units; it does not regularize the original
        # reciprocal-W expression or add a GPU input-domain check.
        for scale in self.scales:
            clip = (Q(0), Q(0), Q(0), Q(1, 1000000))
            restored = divide_clip(tuple(scale*x for x in clip), scale)
            self.assertEqual(bias_suffix(restored, k), bias_suffix(clip, k))

    def test_reference_refuses_zero_near_zero_negative_and_nonfinite_w(self):
        for w in (0., -1., 1e-12, math.inf, -math.inf, math.nan):
            with self.subTest(w=w), self.assertRaises(ValueError):
                finite_reference_bias((0., 0., .5, w), .33)
        for index in range(4):
            for bad in (math.inf, -math.inf, math.nan):
                clip = [0., 0., .5, 1.]
                clip[index] = bad
                with self.subTest(index=index, bad=bad), self.assertRaises(ValueError):
                    finite_reference_bias(clip, .33)
        self.assertEqual(finite_reference_bias((0., 0., .5, 1.), .25),
                         (0., 0., .75, 1.))

    def test_binary32_overflow_is_not_cured_by_homogeneous_unit_conversion(self):
        # Exact arithmetic is not itself a float32-domain guarantee.
        value = bias_suffix((0., 0., 0., 1e-40), .33)[2]
        self.assertTrue(math.isfinite(value))
        self.assertGreater(value, 3.4028234663852886e38)
        with self.assertRaises(OverflowError):
            struct.pack('<f', value)


if __name__ == '__main__':
    unittest.main()
