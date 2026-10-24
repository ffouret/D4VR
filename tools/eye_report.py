#!/usr/bin/env python3
"""Check a fixed lateral eye transform using bounded VKD3D GPU telemetry."""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import eye_control
import gpu_messages
import probe

MARKER = 0x44345354
EPSILON = .500001e-6  # Logger precision, not GPU arithmetic precision.


def evaluate(record: dict,state: dict) -> dict:
    before,after,projection,status = record['rows']; applied,eye,sequence,abi = status
    sx,sy,a,b = projection; errors = []
    if applied not in (0,1) or eye != state['eye'] or sequence != state['sequence'] or abi != 1:
        errors.append('eye_packet_status_mismatch')
    delta = after[0]-before[0]; expected = 0.; tolerance = 2*EPSILON
    if applied == 1:
        if not state['enabled'] or abs(sx-1.784022) > 1.1e-5 or abs(sy-3.171595) > 1.1e-5:
            errors.append('unexpected_target_projection_or_disabled_state')
        if abs(b) <= EPSILON:
            errors.append('unusable_source_depth_coefficients')
        else:
            w = (before[2]+a*before[3])/b
            w_error = (EPSILON*(1+abs(a)+abs(before[3])+abs(w))+EPSILON**2)/(abs(b)-EPSILON)
            tx = state['translation_view_units']; expected = sx*tx*w
            tolerance += abs(tx)*(abs(sx)*w_error+abs(w)*EPSILON+EPSILON*w_error)
            tolerance += 8*2**-23*(abs(before[0])+abs(after[0])+abs(expected)+1)
        if abs(delta-expected) > tolerance:
            errors.append('clip_translation_mismatch')
        for k in (1,2,3):
            if abs(after[k]-before[k]) > 2*EPSILON+8*2**-23*(abs(before[k])+abs(after[k])+1):
                errors.append('unexpected_y_z_or_w_change'); break
    elif any(abs(after[k]-before[k]) > 2*EPSILON for k in range(4)):
        errors.append('inactive_or_non_target_vertex_changed')
    inside = before[3] > 0 and all(abs(before[k]) <= before[3] for k in (0,1)) and 0 <= before[2] <= before[3]
    return {**{k:record[k] for k in ('segment','shader_hash','sample_id')},'clip_before':before,'clip_after':after,
            'projection_parameters':projection,'applied':applied == 1,'eye':eye,'sequence':sequence,
            'clip_x_delta':delta,'expected_clip_x_delta':expected,'comparison_tolerance':tolerance,
            'inside_original_clip_volume':inside,'consistent':not errors,'errors':errors}


def parse(log: Path,packet: Path) -> dict:
    state = eye_control.read(packet); messages = gpu_messages.read_records(log,{MARKER:4})
    samples = [evaluate(r,state) for r in messages['records']]
    displaced = [s for s in samples if s['consistent'] and s['applied'] and abs(s['clip_x_delta']) > s['comparison_tolerance']]
    return {'schema_version':1,'mode':'single_fixed_eye','eye_state':state,'complete_samples':len(samples),
            'verified_transformed_samples':len(displaced),'verified_transformed_inside_clip':sum(s['inside_original_clip_volume'] for s in displaced),
            'verified_unchanged_non_target_samples':sum(s['consistent'] and not s['applied'] for s in samples),
            'inconsistent_samples':sum(not s['consistent'] for s in samples),'rejected_rows':messages['rejected_rows'],
            'incomplete_or_duplicate_groups':messages['incomplete_or_duplicate_groups'],
            'verified_transformed_by_shader':dict(Counter(s['shader_hash'] for s in displaced)),
            'numeric_eye_transform_confirmed':bool(displaced),'stereo_views_of_same_game_frame_proven':False,
            'game_images_submitted_to_steamvr':0,'samples':samples}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('log',type=Path)
    parser.add_argument('--state',type=Path,required=True); parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args(); result = parse(args.log,args.state); probe.write_json(args.output,result)
    print(json.dumps({k:v for k,v in result.items() if k!='samples'},indent=2)); return 0


if __name__ == '__main__': raise SystemExit(main())
