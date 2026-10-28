#!/usr/bin/env python3
"""Owned X11/WSI held-camera geometric depth and successive color controls.

Runs only when explicitly invoked. The fixture owns all host completion waits;
the COPY producer preserves original COPY/BLIT signal scopes and a null fence.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import struct
import subprocess
import eye_control
import paired_capture
import probe
import vulkan_trace


def marked_vertex(path: Path) -> None:
    raw = path.read_bytes()
    words = list(struct.unpack('<'+'I'*(len(raw)//4), raw))
    marker = b'D4VR_STEREO_ABI_1\0'
    marker += b'\0'*(-len(marker) % 4)
    instruction = [((len(marker)//4+1) << 16) | 330,
                   *struct.unpack('<'+'I'*(len(marker)//4), marker)]
    index = 5
    while index < len(words) and words[index] & 65535 not in (71, 72, 19, 20, 21, 22):
        index += words[index] >> 16
    words[index:index] = instruction
    path.write_bytes(struct.pack('<'+'I'*len(words), *words))


def geometric_reference(depth: bytes, color: bytes, width: int, height: int) -> dict:
    """Independent analytic z and coverage check for the two owned triangles."""
    size = width*height*4
    if len(depth) != size or len(color) != size:
        raise ValueError('Geometric reference dimensions differ')
    depths = struct.unpack('<'+'f'*(width*height), depth)
    counts = {'green': 0, 'red': 0, 'background': 0}
    sums = {'green': 0, 'red': 0}
    expected = {'green': 1.001001-.1001001/2,
                'red': 1.001001-.1001001/4, 'background': 1.0}
    for i, z in enumerate(depths):
        pixel = color[i*4:i*4+4]
        kind = 'green' if pixel == b'\0\xff\0\xff' else 'red' if pixel == b'\0\0\xff\xff' else 'background' if pixel == b'\0\0\0\xff' else None
        if kind is None or not math.isfinite(z) or abs(z-expected[kind]) > 4e-7:
            raise ValueError('D32 depth does not match independently known geometry and color coverage')
        counts[kind] += 1
        if kind != 'background':
            sums[kind] += i % width
    if min(counts.values()) <= 100:
        raise ValueError('Triangle depth was replaced by an empty or clear-only image')
    return {'pixels': counts,
            'centers_x': {k: sums[k]/counts[k] for k in sums},
            'analytic_depth': expected, 'maximum_allowed_absolute_z_error': 4e-7}


def packet_bytes(observation: dict, sequence: int) -> bytes:
    if (observation.get('status'), observation.get('packet_sequence'), observation.get('packet_eye')) != ('uniform', sequence, 0):
        raise ValueError('Uniform held eye-zero packet missing')
    raw = bytes.fromhex(observation['packet_abi_bytes_hex'])
    if len(raw) != eye_control.FORMAT.size:
        raise ValueError('Held packet ABI size differs')
    values = eye_control.FORMAT.unpack(raw)
    if values[0] != 0x44345652 or values[2] != sequence or values[4] != 0 or abs(values[11]-(-.125 if sequence == 2 else -.25)) > 1e-7:
        raise ValueError('Held diagnostic translation or identity changed')
    return raw


def validate(a):
    if a.output.exists():
        raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700, parents=True)
    root = Path(__file__).resolve().parents[1]
    host = paired_capture.host_command()

    def run(command, log, timeout=60):
        result = subprocess.run(host+command, capture_output=True, text=True, timeout=timeout)
        output = result.stdout+result.stderr
        log.write_text(output)
        if result.returncode or any(marker in output for marker in ('VUID-', 'Validation Error', 'SYNC-HAZARD-')):
            raise ValueError(str(log.parent)+': inspect '+log.name)
        return output

    unit_output = run([str(a.unit.resolve())], a.output/'unit.log')
    unit = json.loads(unit_output.strip())
    if not unit.get('depth_copy_ownership_passed'):
        raise ValueError('Portable COPY ownership controls missing')
    run([str(a.forwarding.resolve())], a.output/'forwarding.log')
    shaders = []
    for ext in ('vert', 'frag'):
        path = (a.output/('present.'+ext+'.spv')).resolve()
        run([str(a.compiler.resolve()), '-V', '--target-env', 'vulkan1.3',
             str(root/('native/tests/present_eye.'+ext)), '-o', str(path)],
            a.output/('shader-'+ext+'.log'))
        if ext == 'vert':
            marked_vertex(path)
        if a.validator:
            run([str(a.validator.resolve()), '--target-env', 'vulkan1.3', str(path)],
                a.output/('spirv-'+ext+'.log'))
        shaders.append(str(path))

    cases = []
    for runtime in (False, True) if a.runtime else (False,):
        baseline = None
        for enabled in (False, True):
            name = ('held-depth' if enabled else 'pose-hold-control')+('-runtime' if runtime else '-native')
            directory = (a.output/name).resolve()
            directory.mkdir(mode=0o700)
            variables = {
                'VK_INSTANCE_LAYERS': vulkan_trace.LAYER+':VK_LAYER_KHRONOS_validation',
                'VK_ADD_LAYER_PATH': str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),
                'VK_LAYER_ENABLES': 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                'VK_LAYER_VALIDATE_SYNC': '1', 'VK_LOADER_DEBUG': 'layer',
                'D4VR_TRACE_DIR': str(directory), 'D4VR_SEQUENTIAL_CAPTURE_DIR': str(directory),
                'D4VR_GPU_CAMERA_UPDATES': '1', 'D4VR_POSE_CAPTURE_DIAGNOSTIC': '1',
                'D4VR_DIAGNOSTIC_VIEW_HOLD': '1', 'D4VR_POSE_FIXTURE_MONOSCOPIC': '1',
                'D4VR_HELD_DEPTH_FIXTURE': '1',
                'D4VR_HELD_DEPTH_COLOR_DIAGNOSTIC': '1' if enabled else '0',
                'D4VR_DEPTH_COPY_READBACK_DIAGNOSTIC': '1' if enabled else '0'}
            for eye, side in enumerate(('left', 'right')):
                path = directory/(side+'.bin')
                path.write_bytes(eye_control.encode(eye, separation=.128))
                path.chmod(0o600)
                variables['D4VR_PAIR_'+side.upper()+'_FILE'] = str(path)
            command = ['/usr/bin/env']
            for key in ('VK_LAYER_PATH', 'VK_INSTANCE_LAYERS', 'VK_ADD_LAYER_PATH',
                        'D4VR_TRACE_XR_BINDING', 'D4VR_COOPERATIVE_XR_DIAGNOSTIC',
                        'D4VR_LAYER_REPLAY_POC', 'D4VR_FRAME_TRACE', 'D4VR_PAIR_LEFT_FILE',
                        'D4VR_PAIR_RIGHT_FILE', 'D4VR_EYE_STATE_FILE', 'D4VR_SEQUENTIAL_CAPTURE_DIR',
                        'D4VR_POSE_CAPTURE_DIAGNOSTIC', 'D4VR_DIAGNOSTIC_VIEW_HOLD',
                        'D4VR_GPU_CAMERA_UPDATES', 'D4VR_GPU_CAPTURE_DRY_RUN',
                        'D4VR_DEPTH_AUDIT', 'D4VR_DEPTH_READBACK_DIAGNOSTIC',
                        'D4VR_DEPTH_COPY_READBACK_DIAGNOSTIC', 'D4VR_DEPTH_SOURCE_HISTORY','D4VR_SOURCE_HISTORY_WIDTH','D4VR_SOURCE_HISTORY_HEIGHT','D4VR_SOURCE_HISTORY_FORMAT','D4VR_HELD_DEPTH_COLOR_DIAGNOSTIC',
                        'D4VR_HELD_DEPTH_FIXTURE', 'D4VR_DEPTH_TRANSFER_FIXTURE',
                        'D4VR_DEPTH_COPY_FIXTURE', 'D4VR_DEPTH_LAYER_FIXTURE', 'D4VR_DEPTH_AUDIT_FIXTURE',
                        'D4VR_POSE_FIXTURE_SHORT', 'D4VR_POSE_FIXTURE_FOREIGN',
                        'D4VR_POSE_FIXTURE_CANCEL', 'D4VR_POSE_FIXTURE_CALIBRATED',
                        'D4VR_POSE_FIXTURE_MONOSCOPIC', 'D4VR_METRIC_SOURCE_DEPTH_SCALE'):
                command += ['-u', key]
            command += [key+'='+value for key, value in variables.items()]
            if runtime:
                command += [str(a.runtime.resolve()), '--batch', '--']
            command += [str(a.fixture.resolve()), *shaders, str(directory)]
            probe.write_json(directory/'command.json', host+command)
            log = run(command, directory/'run.log')
            for layer in (vulkan_trace.LAYER, 'VK_LAYER_KHRONOS_validation'):
                if 'Insert instance layer "'+layer+'"' not in log or 'Inserted device layer "'+layer+'"' not in log:
                    raise ValueError('Requested real-loader layer was not loaded')
            summaries = [json.loads(line) for line in log.splitlines() if line.startswith('{"presentation_layer_fixture_passed"')]
            if len(summaries) != 1:
                raise ValueError('Owned WSI fixture result missing')
            summary = summaries[0]
            if (summary.get('frames'), summary.get('held_depth_geometry'), summary.get('original_depth_copies'), summary.get('original_null_fence_submits'), summary.get('original_copy_stage_mask'), summary.get('original_blit_stage_mask'), summary.get('application_completion'), summary.get('depth_color_common_instant_claimed')) != (64, True, 64, 64, 1 << 32, 1 << 34, 'device_wait_idle_after_present', False):
                raise ValueError('Original COPY/BLIT/fence application controls differ')
            rows = [json.loads(line) for path in directory.glob('vulkan-*.jsonl') for line in path.read_text().splitlines()]
            errors = [r for r in rows if r.get('event') in ('pose_capture_error', 'presentation_error', 'camera_update_fatal', 'paired_pipeline_error', 'depth_copy_error', 'depth_copy_init_skipped', 'held_depth_initialization_refused', 'held_depth_export_error')]
            if errors:
                raise ValueError(name+': layer initialization or runtime error')
            destroy = [r for r in rows if r.get('event') == 'device_destroy']
            if len(destroy) != 1 or (destroy[0]['present_calls'], destroy[0]['submissions'], destroy[0]['submitted_command_buffers'], destroy[0]['commands']['draw']) != (64, 320, 64, 128):
                raise ValueError('Original application commands duplicated or lost')
            state = destroy[0]['pose_capture']
            pool = destroy[0]['pose_producer']
            if any(state[k] for k in ('active', 'consumer_pending', 'fault', 'fixed_view_held')) or not state['stopped'] or state['pairs'] != 2 or pool['outstanding_leases'] or pool['leases_issued'] != pool['retire_submissions']:
                raise ValueError('Held color stop or consumer teardown incomplete')

            pairs = {}
            pair_proofs = []
            for path in sorted(directory.glob('pose-pair-*.json')):
                pair = json.loads(path.read_text())
                request = pair['request']
                if request not in (1, 2) or request in pairs:
                    raise ValueError('Unexpected diagnostic color request')
                if pair['mode'] != 'successive_diagnostic' or any(pair[k] for k in ('actual_xr_pose_source', 'xr_submission', 'whole_scene_verified', 'common_scene_time_known')):
                    raise ValueError('Color claims a common scene time or XR source')
                raw = path.with_suffix('.pixels').read_bytes()
                size = pair['width']*pair['height']*4
                if len(raw) != size*2 or raw[:size] != raw[size:]:
                    raise ValueError('Held diagnostic pair is not monoscopic')
                for eye, receipt in enumerate(pair['eyes']):
                    camera = receipt['camera_provenance']
                    observed = camera['copy_submit']
                    draw = camera['selected_draw_interval']
                    if receipt['eye'] != eye or not camera['same_diagnostic_packet'] or observed['status'] != 'uniform' or draw['status'] != 'uniform' or observed['packet_abi_bytes_hex'] != draw['packet_abi_bytes_hex']:
                        raise ValueError('Selected color draw packet is not uniform and exact')
                    if raw[eye*size:(eye+1)*size] != (directory/('source-frame-'+str(receipt['present']-1)+'.pixels')).read_bytes():
                        raise ValueError('Actual color consumer differs from original application color')
                packet_bytes(pair['eyes'][0]['camera_provenance']['copy_submit'], request+1)
                if pair['eyes'][1]['present'] <= pair['eyes'][0]['present'] or pair['eyes'][1]['capture_sequence'] <= pair['eyes'][0]['capture_sequence']:
                    raise ValueError('Successive color source ordering missing')
                pairs[request] = pair
                pair_proofs.append({'request': request, 'source_presents': [e['present'] for e in pair['eyes']], 'source_capture_sequences': [e['capture_sequence'] for e in pair['eyes']], 'camera_eye_zero': pair['eyes'][0]['camera_provenance']['copy_submit']})
            if sorted(pairs) != [1, 2]:
                raise ValueError('Two successive held color pairs required')

            pixels = {path.name: path.read_bytes() for path in directory.glob('*.pixels') if path.name.startswith(('source-frame-', 'held-depth-frame-'))}
            if len(pixels) != 128:
                raise ValueError('Original per-frame color/depth references missing')
            if baseline is None:
                baseline = pixels
            elif baseline != pixels:
                raise ValueError('Combined depth diagnostic changed original color or geometric depth')
            geometry = {}
            for depth_frame, color_frame, request in ((16, 24, 1), (48, 56, 2)):
                width, height = pairs[request]['width'], pairs[request]['height']
                depth = pixels['held-depth-frame-'+str(depth_frame)+'.pixels']
                color = pixels['source-frame-'+str(color_frame)+'.pixels']
                geometry[request] = geometric_reference(depth, color, width, height)
                if pixels['source-frame-'+str(depth_frame)+'.pixels'] != color:
                    raise ValueError('Geometry view changed between the separate depth/color observations')
            shifts = {key: geometry[2]['centers_x'][key]-geometry[1]['centers_x'][key] for key in ('green', 'red')}
            if shifts['red'] >= 0 or not 1.7 < shifts['green']/shifts['red'] < 2.3:
                raise ValueError('The two diagnostic packets did not move geometry with its optical depth')

            exports = []
            for path in directory.glob('depth-copy-*.bin'):
                meta = json.loads(path.with_suffix('.json').read_text())
                request = meta['request']
                if request not in (3, 5):
                    raise ValueError('A refused held-depth request was accepted later')
                pose_request = 1 if request == 3 else 2
                frame = 16 if request == 3 else 48
                pair = pairs[pose_request]
                packet = packet_bytes(pair['eyes'][0]['camera_provenance']['copy_submit'], pose_request+1)
                expected = packet_bytes(meta['expected_held_packet'], pose_request+1)
                observed = packet_bytes(meta['copy_submit_camera'], pose_request+1)
                if packet != expected or packet != observed or meta['recording_eye'] != 0 or meta['capture_submission'] <= 0 or meta['copy_submit_camera']['first_submission'] != meta['capture_submission'] or meta['copy_submit_camera']['last_submission'] != meta['capture_submission']:
                    raise ValueError('Depth recording and successful submit do not retain the exact held packet')
                if meta['capture_submission'] <= max(e['camera_provenance']['copy_submit']['last_submission'] for e in pair['eyes']):
                    raise ValueError('Depth observation did not follow its successive color source')
                if not meta['raw_complete'] or meta['epoch'] != 1 or meta['format'] != 126 or meta['original_aspects'] != 2 or path.read_bytes() != pixels['held-depth-frame-'+str(frame)+'.pixels']:
                    raise ValueError('Private D32 pixels differ from the independent original COPY destination')
                if any(meta[k] for k in ('whole_scene_verified', 'initialized_pixels_verified', 'common_scene_time_known', 'xr_pose_known')):
                    raise ValueError('Depth claims unsupported scene closure or shared time')
                exports.append({'request': request, 'reference_frame': frame, 'successive_color_reference_frame': 24 if request == 3 else 56, 'submission': meta['submission'], 'capture_submission': meta['capture_submission'], 'recording': meta['recording'], 'recording_eye': meta['recording_eye'], 'packet_sequence': pose_request+1, 'exact_original_depth_bytes': path.stat().st_size})
            rejected = [r for r in rows if r.get('event') == 'held_depth_request_rejected']
            reports = [json.loads(path.read_text()) for path in directory.glob('depth-copy-status-*.json')]
            if enabled:
                if sorted(e['request'] for e in exports) != [3, 5] or [r['request'] for r in rejected] != [1, 2, 4, 6]:
                    raise ValueError('Held-phase admission or explicit request refusal differs')
                expected_reasons = {1: 'held_depth_consumer_uninitialized', 2: 'held_depth_color_active', 4: 'held_depth_color_active', 6: 'held_depth_terminal_or_fault'}
                if any(r['reason'] != expected_reasons[r['request']] for r in rejected):
                    raise ValueError('Request refusal did not occur in the intended actual phase')
                armed = [r for r in rows if r.get('event') == 'depth_copy_armed']
                if [r['request'] for r in armed] != [3, 5] or any(not r['held_depth_phase'] for r in armed):
                    raise ValueError('Depth was armed outside a held phase')
                finals = [r for r in reports if r['final']]
                if len(finals) != 1:
                    raise ValueError('Final bounded COPY producer report missing')
                final = finals[0]
                if (final['recorded'], final['exported'], final['private_fences'], final['invalidations'], final['held_phase_requests_seen'], final['held_phase_rejected'], final['held_depth_mode'], final['halted']) != (2, 2, 2, 2, 6, 4, True, True):
                    raise ValueError('Final held-depth bounded ownership counters differ')
                if any(final[k] for k in ('app_fence_bypass', 'refused', 'allocation_failures', 'original_failures', 'gpu_waits_or_extra_queue_submissions')):
                    raise ValueError('Producer changed completion behavior or failed')
            elif exports or reports or rejected:
                raise ValueError('Disabled combined diagnostic emitted depth artifacts')
            cases.append({'case': name, 'combined_mode_enabled': enabled, 'actual_x11_wsi': True,
                          'original_controls': summary, 'original_reference_bytes': sum(map(len, pixels.values())),
                          'all_original_depth_color_bytes_unchanged': enabled,
                          'color_pairs': pair_proofs, 'geometric_depth': geometry,
                          'optical_translation_pixels': shifts, 'depth_exports': exports,
                          'explicitly_rejected_requests': [{'request': r['request'], 'reason': r['reason']} for r in rejected],
                          'validation_errors': 0})
            print(name+': passed', flush=True)

    sources = ['native/tests/present_layer_vulkan.cpp', 'native/tests/held_depth_fixture.hpp',
               'native/tests/present_eye.vert', 'native/tests/present_eye.frag',
               'native/core/held_depth_mode.hpp', 'native/vulkan/trace_layer.cpp',
               'native/vulkan/depth_copy_readback.hpp', 'native/vulkan/depth_copy_readback.cpp',
               'native/tests/depth_copy_readback_tests.cpp', 'tools/held_depth_validate.py']
    proof = {'schema_version': 1, 'owned_geometry': True, 'actual_wsi_loader': True,
             'actual_xr_pose_source': False, 'xr_submission': False, 'game_rendered': False,
             'whole_scene_verified': False, 'common_scene_time_known': False,
             'depth_color_observations': 'successive_with_identical_held_packet',
             'khronos_synchronization_validation_loaded': True, 'validation_errors': 0,
             'unit': unit, 'cases': cases,
             'layer_sha256': probe.digest(a.layer/vulkan_trace.LIBRARY),
             'fixture_sha256': probe.digest(a.fixture), 'unit_sha256': probe.digest(a.unit),
             'forwarding_sha256': probe.digest(a.forwarding),
             'source_sha256': {path: probe.digest(root/path) for path in sources}}
    probe.write_json(a.output/'proof.json', proof)
    return proof


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('fixture', 'compiler', 'layer', 'validation-layer', 'unit', 'forwarding', 'output'):
        p.add_argument('--'+key, type=Path, required=True)
    p.add_argument('--runtime', type=Path)
    p.add_argument('--validator', type=Path)
    try:
        print(json.dumps(validate(p.parse_args()), indent=2))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
