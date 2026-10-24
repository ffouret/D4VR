#!/usr/bin/env python3
"""Check before/after geometry telemetry, allowing for six-decimal logging."""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path

import gpu_messages

MARKER = 0x44344745
EPSILON = 0.500001e-6  # VKD3D logger rounds float payloads to six decimals.


def evaluate(record: dict) -> dict:
    before, after, projection, status = record['rows']
    matched, view_w, eye_shift, reserved = status
    sx, sy, depth_a, depth_b = projection
    errors = []
    if matched not in (0, 1) or abs(eye_shift - .1) > EPSILON or reserved != 0:
        errors.append('invalid_status_contract')
    if any(abs(after[i] - before[i]) > 2 * EPSILON for i in (1, 2, 3)):
        errors.append('unexpected_y_z_or_w_change')
    dx = after[0] - before[0]
    expected_dx = 0.0
    if matched == 1:
        if abs(sx - 1.784022) > 1.1e-5 or abs(sy - 3.171595) > 1.1e-5:
            errors.append('unexpected_target_projection')
        expected_dx = -sx * eye_shift * view_w
        # Propagate each rounded payload's uncertainty through the product.
        dx_tolerance = EPSILON * (2 + abs(eye_shift * view_w) + abs(sx * view_w) + abs(sx * eye_shift)) + 1e-10
        if abs(dx - expected_dx) > dx_tolerance:
            errors.append('displacement_does_not_match_eye_translation')
        if abs(depth_b) <= EPSILON:
            errors.append('unusable_projection_depth')
        else:
            recovered_w = (before[2] + depth_a * before[3]) / depth_b
            # Cancellation at distant vertices amplifies the logged rounding.
            uncertainty = (EPSILON * (1 + abs(depth_a) + abs(before[3]) + abs(view_w))
                           + EPSILON ** 2) / (abs(depth_b) - EPSILON) + EPSILON
            if abs(recovered_w - view_w) > uncertainty + 1e-10:
                errors.append('homogeneous_coordinate_mismatch')
    elif matched == 0:
        if abs(dx) > 2 * EPSILON or view_w != 0:
            errors.append('non_target_vertex_changed')
    inside = before[3] > 0 and all(abs(before[i]) <= before[3] for i in (0, 1)) and 0 <= before[2] <= before[3]
    return {**{k: record[k] for k in ('segment', 'shader_hash', 'sample_id')},
            'clip_before': before, 'clip_after': after, 'projection_parameters': projection,
            'projection_matched': matched == 1, 'view_w': view_w, 'eye_shift_view_units': eye_shift,
            'clip_x_delta': dx, 'expected_clip_x_delta': expected_dx,
            'ndc_x_delta': dx / before[3] if abs(before[3]) > 1e-9 else None,
            'inside_original_clip_volume': inside, 'consistent': not errors, 'errors': errors}


def parse(log: Path) -> dict:
    messages = gpu_messages.read_records(log, {MARKER: 4})
    samples = [evaluate(r) for r in messages['records']]
    displaced = [s for s in samples if s['consistent'] and s['projection_matched'] and abs(s['clip_x_delta']) > 2 * EPSILON]
    return {'schema_version': 1, 'complete_geometry_samples': len(samples),
            'target_projection_samples': sum(s['projection_matched'] for s in samples),
            'verified_displaced_samples': len(displaced),
            'displaced_samples_inside_original_clip_volume': sum(s['inside_original_clip_volume'] for s in displaced),
            'verified_unchanged_non_target_samples': sum(s['consistent'] and not s['projection_matched'] for s in samples),
            'inconsistent_samples': sum(not s['consistent'] for s in samples),
            'incomplete_or_duplicate_groups': messages['incomplete_or_duplicate_groups'],
            'rejected_rows': messages['rejected_rows'],
            'verified_displacements_by_shader': dict(Counter(s['shader_hash'] for s in displaced)),
            'sample_budget_reached': any(s['sample_id'] == 1023 for s in samples),
            'numeric_geometry_shift_confirmed': bool(displaced),
            'main_pass_identity_proven': False, 'native_stereo_proven': False, 'samples': samples}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    report = parse(args.log)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'samples'}, indent=2))


if __name__ == '__main__':
    main()
