"""Validate finite original depth reads on GPU, directly and through the layer."""
import argparse
import json
import struct
import subprocess
from pathlib import Path

import paired_capture
import probe


def wsi(a):
    """Real WSI, known depth clears, and exact original color with probe on/off."""
    root = Path(__file__).resolve().parents[1]
    shaders = []
    for ext in ('vert', 'frag'):
        target = (a.output/('wsi.'+ext+'.spv')).resolve()
        subprocess.run(paired_capture.host_command()+[str(a.compiler.resolve()), '-V',
                       '--target-env', 'vulkan1.2', str(root/('native/tests/present_eye.'+ext)),
                       '-o', str(target)], capture_output=True, text=True, check=True, timeout=20)
        shaders.append(str(target))
    cases = []
    for runtime in (False, True) if a.runtime else (False,):
        original = None
        for enabled in (False, True):
            name = 'wsi-'+('read-' if enabled else 'baseline-')+('runtime' if runtime else 'native')
            directory = (a.output/name).resolve();directory.mkdir(mode=0o700)
            command = paired_capture.host_command()+['/usr/bin/env']
            for key in ('VK_LAYER_PATH', 'D4VR_TRACE_XR_BINDING', 'D4VR_COOPERATIVE_XR_DIAGNOSTIC',
                        'D4VR_LAYER_REPLAY_POC', 'D4VR_FRAME_TRACE', 'D4VR_PAIR_LEFT_FILE',
                        'D4VR_PAIR_RIGHT_FILE', 'D4VR_EYE_STATE_FILE', 'D4VR_SEQUENTIAL_CAPTURE_DIR',
                        'D4VR_POSE_CAPTURE_DIAGNOSTIC', 'D4VR_DIAGNOSTIC_VIEW_HOLD',
                        'D4VR_GPU_CAMERA_UPDATES', 'D4VR_GPU_CAPTURE_DRY_RUN'):
                command += ['-u', key]
            command += ['VK_INSTANCE_LAYERS=VK_LAYER_D4VR_trace:VK_LAYER_KHRONOS_validation',
                        'VK_ADD_LAYER_PATH='+str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),
                        'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                        'VK_LAYER_VALIDATE_SYNC=1', 'VK_LOADER_DEBUG=layer',
                        'D4VR_TRACE_DIR='+str(directory), 'D4VR_DEPTH_LAYER_FIXTURE=1',
                        'D4VR_DEPTH_READBACK_DIAGNOSTIC='+('1' if enabled else '0')]
            if runtime:command += [str(a.runtime.resolve()), '--batch', '--']
            command += [str(a.wsi_fixture.resolve()), *shaders, str(directory)]
            p = subprocess.run(command, capture_output=True, text=True, timeout=40)
            log = p.stdout+p.stderr;(directory/'run.log').write_text(log)
            if p.returncode or any(v in log for v in ('VUID-', 'Validation Error', 'SYNC-HAZARD-')):
                raise ValueError(name+': inspect private WSI log')
            for layer in ('VK_LAYER_D4VR_trace', 'VK_LAYER_KHRONOS_validation'):
                if 'Insert instance layer "'+layer+'"' not in log or 'Inserted device layer "'+layer+'"' not in log:
                    raise ValueError('WSI layer load not verified')
            rows = [json.loads(line) for path in directory.glob('vulkan-*.jsonl')
                    for line in path.read_text().splitlines()]
            if any(r.get('event') in ('depth_error', 'depth_init_refused', 'depth_submit_refused') for r in rows):
                raise ValueError('WSI depth driver refused/faulted')
            destroy = [r for r in rows if r.get('event') == 'device_destroy']
            if (len(destroy) != 1 or destroy[0]['present_calls'] != 64 or destroy[0]['submissions'] != 64
                    or destroy[0]['submitted_command_buffers'] != 64 or destroy[0]['commands']['draw'] != 128):
                raise ValueError('Original WSI commands duplicated/lost')
            pixels = {name: (directory/name).read_bytes() for name in
                      ('mono-before.pixels', 'left-reference.pixels', 'right-reference.pixels', 'mono-after.pixels')}
            if not all(len(raw) == 256*256*4 for raw in pixels.values()) or len(set(pixels['mono-before.pixels'])) < 2:
                raise ValueError('WSI original images missing/empty')
            exports = sorted([(json.loads(path.read_text()), path) for path in directory.glob('depth-*.json')],
                             key=lambda item: item[0]['copy'])
            if enabled:
                if pixels != original or len(exports) != 4:
                    raise ValueError('Depth read changed original WSI colors or lost samples')
                if [d['request'] for d, _ in exports] != [1, 1, 2, 2]:
                    raise ValueError('WSI original request identity lost')
                if len({d['image_generation'] for d, _ in exports}) != 1 or len({d['recording'] for d, _ in exports}) != 4:
                    raise ValueError('WSI generation provenance invalid')
                for i, (d, path) in enumerate(exports):
                    raw = path.with_suffix('.bin').read_bytes();words = struct.unpack('=65536I', raw)
                    expected = (i % 2)/4
                    if (d['format'] != 129 or d['width'] != 256 or d['height'] != 256
                            or d['rendering_ordinal'] != 1 or d['depth_load_op'] != 1
                            or d['submit_camera']['status'] != 'unknown' or d['eye'] != 2
                            or d['raw_complete'] is not True or d['whole_scene_verified'] is not False
                            or d['common_scene_time_known'] is not False or d['xr_pose_known'] is not False
                            or any(word & 0xff000000 or abs(word/16777215-expected) >= 1e-7 for word in words)):
                        raise ValueError('WSI depth clear/reference/claim invalid')
            else:
                original = pixels
                if exports:raise ValueError('Disabled depth mode exported data')
            cases.append({'environment': name, 'present_calls': 64, 'depth_exports': len(exports),
                          'depth_bytes': sum(path.with_suffix('.bin').stat().st_size for _, path in exports),
                          'exact_original_color_bytes': sum(len(raw) for raw in pixels.values()) if enabled else 0,
                          'depth_clear_only_reference': True, 'validation_errors': 0})
    return cases


def validate(a):
    if a.output.exists():
        raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700, parents=True)
    unit = subprocess.run(paired_capture.host_command()+[str(a.unit.resolve())],
                          capture_output=True, text=True, check=True, timeout=20)
    portable = json.loads(unit.stdout)
    if portable.get('depth_ownership_passed') is not True:
        raise ValueError('Ownership controls missing')
    results = []
    for runtime in (False, True) if a.runtime else (False,):
        for layer in (False, True):
            name = ('layer-' if layer else 'direct-')+('runtime' if runtime else 'native')
            directory = (a.output/name).resolve()
            directory.mkdir(mode=0o700)
            layers = 'VK_LAYER_KHRONOS_validation'
            if layer:
                layers = 'VK_LAYER_D4VR_trace:'+layers
            variables = {
                'VK_INSTANCE_LAYERS': layers,
                'VK_ADD_LAYER_PATH': str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),
                'VK_LAYER_ENABLES': 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                'VK_LAYER_VALIDATE_SYNC': '1', 'VK_LOADER_DEBUG': 'layer',
            }
            if layer:
                variables.update(D4VR_DEPTH_READBACK_DIAGNOSTIC='1', D4VR_TRACE_DIR=str(directory))
            command = paired_capture.host_command()+['/usr/bin/env']
            # Clear inherited diagnostics before running a controlled fixture.
            for key in ('VK_LAYER_PATH', 'VK_INSTANCE_LAYERS', 'VK_ADD_LAYER_PATH',
                        'D4VR_TRACE_DIR', 'D4VR_DEPTH_READBACK_DIAGNOSTIC',
                        'D4VR_TRACE_XR_BINDING', 'D4VR_COOPERATIVE_XR_DIAGNOSTIC',
                        'D4VR_LAYER_REPLAY_POC', 'D4VR_FRAME_TRACE',
                        'D4VR_PAIR_LEFT_FILE', 'D4VR_PAIR_RIGHT_FILE', 'D4VR_EYE_STATE_FILE',
                        'D4VR_SEQUENTIAL_CAPTURE_DIR', 'D4VR_POSE_CAPTURE_DIAGNOSTIC',
                        'D4VR_DIAGNOSTIC_VIEW_HOLD'):
                command += ['-u', key]
            command += [key+'='+value for key, value in variables.items()]
            if runtime:
                command += [str(a.runtime.resolve()), '--batch', '--']
            command += [str(a.fixture.resolve())]
            if layer:
                command += [str(directory)]
            p = subprocess.run(command, capture_output=True, text=True, timeout=40)
            log = p.stdout+p.stderr
            (directory/'run.log').write_text(log)
            if p.returncode or any(v in log for v in ('VUID-', 'Validation Error', 'SYNC-HAZARD-')):
                raise ValueError(name+': inspect private GPU log')
            for tag in ('Insert instance layer "VK_LAYER_KHRONOS_validation"',
                        'Inserted device layer "VK_LAYER_KHRONOS_validation"'):
                if tag not in log:
                    raise ValueError('Validation layer load not verified')
            proofs = [json.loads(line) for line in p.stdout.splitlines() if line.startswith('{')]
            if len(proofs) != 1 or proofs[0].get('depth_gpu_passed') is not True:
                raise ValueError('GPU reference controls absent')
            proof = proofs[0]
            if (proof.get('cases') != 6 or proof.get('exact_depth_bytes') != 73728
                    or proof.get('held_producer_fences') != 2 or proof.get('cross_queue_overwrites') != 2):
                raise ValueError('Incomplete GPU controls')
            exports = []
            if layer:
                for tag in ('Insert instance layer "VK_LAYER_D4VR_trace"',
                            'Inserted device layer "VK_LAYER_D4VR_trace"'):
                    if tag not in log:
                        raise ValueError('Readback layer load not verified')
                rows = [json.loads(line) for path in directory.glob('vulkan-*.jsonl')
                        for line in path.read_text().splitlines()]
                if any(r.get('event') in ('depth_error', 'depth_init_refused') for r in rows):
                    raise ValueError('Depth layer error')
                refusals = [r for r in rows if r.get('event') == 'depth_submit_refused']
                if [r.get('reason') for r in refusals] != ['device_wide_stage_scopes', 'sparse_outside_contract']:
                    raise ValueError('Pre-forwarding refusals not verified')
                for path in directory.glob('depth-*.json'):
                    d = json.loads(path.read_text())
                    raw = path.with_suffix('.bin').read_bytes()
                    words = struct.unpack('=3072I', raw)
                    if (d['width'], d['height'], d['rendering_ordinal']) != (64, 48, 1):
                        raise ValueError('Unexpected source attachment/scope')
                    if d.get('depth_load_op') != 1 or d.get('clear_depth_bits') != 0x3f800000:
                        raise ValueError('Original clear/load provenance missing')
                    if (d['raw_complete'] is not True or d['whole_scene_verified'] is not False
                            or d['common_scene_time_known'] is not False or d['xr_pose_known'] is not False
                            or d['eye'] != 2 or d['submit_camera']['status'] != 'unknown'):
                        raise ValueError('Unsupported scene/pose claim')
                    for y in range(48):
                        for x in range(64):
                            q = (2 if y >= 24 else 0)+(1 if x >= 32 else 0)
                            word = words[y*64+x]
                            if d['format'] == 129:
                                if word & 0xff000000 or abs(word/16777215-q/4) >= 1e-7:
                                    raise ValueError('D24 source/padding mismatch')
                            elif d['format'] != 126 or word != struct.unpack('=I', struct.pack('=f', q/4))[0]:
                                raise ValueError('D32 source mismatch')
                    exports.append({k: d[k] for k in ('request', 'copy', 'recording', 'rendering_ordinal',
                                                       'submission', 'image_generation', 'view_generation', 'format')})
                exports.sort(key=lambda d: d['request'])
                if ([d['request'] for d in exports] != list(range(1, 7))
                        or len({d['image_generation'] for d in exports}) != 6
                        or len({d['recording'] for d in exports}) != 6):
                    raise ValueError('Generation/request provenance lost')
            results.append({'environment': name, 'gpu': proof, 'layer_exports': exports,
                            'validation_errors': 0, 'pre_forwarding_refusals_verified': layer})
    root = Path(__file__).resolve().parents[1]
    sources = ['native/vulkan/depth_readback.hpp', 'native/vulkan/depth_readback.cpp',
               'native/vulkan/trace_layer.cpp', 'native/tests/depth_readback_tests.cpp',
               'native/tests/depth_readback_vulkan.cpp', 'tools/depth_readback_validate.py']
    wsi_cases = wsi(a)
    sources += ['native/tests/present_layer_vulkan.cpp']
    result = {'schema_version': 1, 'synthetic_attachments': True, 'game_rendered': False,
              'actual_wsi': True, 'xr_submission': False, 'portable': portable, 'wsi_cases': wsi_cases,
              'cases': results, 'validation_errors': 0,
              'fixture_sha256': probe.digest(a.fixture), 'unit_sha256': probe.digest(a.unit),
              'wsi_fixture_sha256': probe.digest(a.wsi_fixture),
              'layer_sha256': probe.digest(a.layer/'libd4vr_vulkan_trace.so'),
              'source_sha256': {p: probe.digest(root/p) for p in sources}}
    probe.write_json(a.output/'proof.json', result)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('fixture', 'unit', 'layer', 'validation-layer', 'output', 'wsi-fixture', 'compiler'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--runtime', type=Path)
    try:
        print(json.dumps(validate(parser.parse_args()), indent=2))
    except (OSError, ValueError, subprocess.SubprocessError) as e:
        raise SystemExit(str(e))
