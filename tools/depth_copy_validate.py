"""Verify bounded original-COPY depth readback on owned geometry and real WSI."""
import argparse
import json
import subprocess
from pathlib import Path
import paired_capture
import probe


def validate(a):
    if a.output.exists():
        raise ValueError('Use a new private output directory')
    a.output.mkdir(parents=True, mode=0o700)
    root = Path(__file__).resolve().parents[1]
    host = paired_capture.host_command()

    def run(command, directory, timeout=45):
        p = subprocess.run(host+command, capture_output=True, text=True, timeout=timeout)
        log = p.stdout+p.stderr
        (directory/'run.log').write_text(log)
        if p.returncode or any(x in log for x in ('VUID-', 'Validation Error', 'SYNC-HAZARD-')):
            raise ValueError(str(directory)+': inspect private log')
        return log

    unit = json.loads(run([str(a.unit.resolve())], a.output))
    if not unit.get('depth_copy_ownership_passed'):
        raise ValueError('CPU ownership controls missing')
    run([str(a.forwarding.resolve())], a.output)
    shaders = {}
    for name in ('depth_copy', 'present_eye'):
        shaders[name] = []
        for ext in ('vert', 'frag'):
            path = (a.output/(name+'.'+ext+'.spv')).resolve()
            run([str(a.compiler.resolve()), '-V', '--target-env', 'vulkan1.3',
                 str(root/('native/tests/'+name+'.'+ext)), '-o', str(path)], a.output)
            shaders[name].append(str(path))
    cases = []
    for runtime in (False, True) if a.runtime else (False,):
        for wsi in (False, True):
            baseline = None
            modes = ('disabled', 'layer') if wsi else ('disabled', 'direct', 'loader-disabled', 'layer')
            for mode in modes:
                enabled = mode in ('direct', 'layer')
                layered = wsi or mode in ('loader-disabled', 'layer')
                name = ('wsi-' if wsi else 'geometry-')+mode+('-runtime' if runtime else '-native')
                directory = (a.output/name).resolve()
                directory.mkdir(mode=0o700)
                command = ['/usr/bin/env']
                # No inherited diagnostic may inject unrelated GPU operations.
                for key in ('VK_LAYER_PATH', 'VK_INSTANCE_LAYERS', 'VK_ADD_LAYER_PATH',
                            'D4VR_TRACE_XR_BINDING', 'D4VR_COOPERATIVE_XR_DIAGNOSTIC',
                            'D4VR_LAYER_REPLAY_POC', 'D4VR_FRAME_TRACE', 'D4VR_PAIR_LEFT_FILE',
                            'D4VR_PAIR_RIGHT_FILE', 'D4VR_EYE_STATE_FILE', 'D4VR_SEQUENTIAL_CAPTURE_DIR',
                            'D4VR_POSE_CAPTURE_DIAGNOSTIC', 'D4VR_DIAGNOSTIC_VIEW_HOLD',
                            'D4VR_GPU_CAMERA_UPDATES', 'D4VR_GPU_CAPTURE_DRY_RUN',
                            'D4VR_DEPTH_AUDIT', 'D4VR_DEPTH_READBACK_DIAGNOSTIC',
                            'D4VR_DEPTH_TRANSFER_FIXTURE', 'D4VR_DEPTH_COPY_FIXTURE',
                            'D4VR_DEPTH_LAYER_FIXTURE', 'D4VR_DEPTH_AUDIT_FIXTURE',
                            'D4VR_HELD_DEPTH_COLOR_DIAGNOSTIC', 'D4VR_DEPTH_COPY_READBACK_DIAGNOSTIC'):
                    command += ['-u', key]
                command += ['VK_INSTANCE_LAYERS='+('VK_LAYER_D4VR_trace:' if layered else '')+'VK_LAYER_KHRONOS_validation',
                            'VK_ADD_LAYER_PATH='+str(a.validation_layer.resolve())+(':'+str(a.layer.resolve()) if layered else ''),
                            'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                            'VK_LAYER_VALIDATE_SYNC=1', 'VK_LOADER_DEBUG=layer',
                            'D4VR_TRACE_DIR='+str(directory),
                            'D4VR_DEPTH_COPY_READBACK_DIAGNOSTIC='+('1' if mode == 'layer' else '0')]
                if wsi:
                    command += ['D4VR_DEPTH_LAYER_FIXTURE=1', 'D4VR_DEPTH_AUDIT_FIXTURE=1',
                                'D4VR_DEPTH_TRANSFER_FIXTURE=1', 'D4VR_DEPTH_COPY_FIXTURE=1']
                if runtime:
                    command += [str(a.runtime.resolve()), '--batch', '--']
                command += [str((a.wsi_fixture if wsi else a.fixture).resolve()),
                            *shaders['present_eye' if wsi else 'depth_copy'], str(directory)]
                if not wsi:
                    command += ['disabled' if mode == 'loader-disabled' else mode]
                (directory/'command.json').write_text(json.dumps(command, indent=2))
                log = run(command, directory)
                for layer in ('VK_LAYER_KHRONOS_validation',)+(('VK_LAYER_D4VR_trace',) if layered else ()):
                    if 'Insert instance layer "'+layer+'"' not in log or 'Inserted device layer "'+layer+'"' not in log:
                        raise ValueError('Requested layer was not loaded')
                if wsi:
                    pixels = {p.name:p.read_bytes() for p in directory.glob('*.pixels')}
                    if len(pixels) != 6 or sum(map(len, pixels.values())) != 1050112:
                        raise ValueError('WSI references missing')
                    if any(pixels['transfer-original-'+str(i)+'.pixels'] != bytes.fromhex('0000803e')*192 for i in (1, 2)):
                        raise ValueError('Original clear depth changed')
                    rows = [json.loads(line) for p in directory.glob('vulkan-*.jsonl') for line in p.read_text().splitlines()]
                    end = [r for r in rows if r.get('event') == 'device_destroy']
                    if len(end) != 1 or (end[0]['present_calls'], end[0]['submissions'], end[0]['submitted_command_buffers'], end[0]['commands']['draw']) != (64, 67, 66, 128):
                        raise ValueError('WSI original commands duplicated/lost')
                else:
                    result = [json.loads(line) for line in log.splitlines() if line.startswith('{"depth_copy_gpu_passed"')]
                    if len(result) != 1 or not result[0]['depth_copy_gpu_passed'] or result[0]['original_submit_calls'] != 13 or result[0]['geometry_pixels'] != 3080 or result[0].get('latency_annotations_unchanged') != 6:
                        raise ValueError('Geometry/future gate controls incomplete')
                    pixels = {p.name:p.read_bytes() for p in directory.iterdir() if p.suffix in ('.depth', '.color', '.stencil')}
                    if sum(map(len, pixels.values())) != 104448:
                        raise ValueError('Geometry references missing')
                    if mode == 'direct':
                        for i in range(3):
                            if (directory/('case-'+str(i)+'.private')).read_bytes() != pixels['case-'+str(i)+'.depth']:
                                raise ValueError('Direct producer differs from original pixels')
                if baseline is None:
                    baseline = pixels
                elif baseline != pixels:
                    raise ValueError('Readback changed original depth/color/stencil')
                private = []
                for p in directory.glob('depth-copy-*.bin'):
                    meta = json.loads(p.with_suffix('.json').read_text())
                    expected = bytes.fromhex('0000803e')*64 if wsi else pixels['case-'+str(meta['request']-1)+'.depth']
                    if p.read_bytes() != expected or not meta['raw_complete'] or meta['epoch'] != (2 if wsi or meta['request'] < 3 else 1):
                        raise ValueError('Layer pixels/epoch wrong')
                    if any(meta[k] for k in ('whole_scene_verified', 'initialized_pixels_verified', 'common_scene_time_known', 'xr_pose_known')):
                        raise ValueError('Unsupported scene/pose claim')
                    private.append(meta)
                reports = [json.loads(p.read_text()) for p in directory.glob('depth-copy-status-*.json')]
                if mode == 'layer':
                    finals = [v for v in reports if v['final']]
                    if len(private) != (1 if wsi else 3) or len(finals) != 1:
                        raise ValueError('Exports/final report missing')
                    v = finals[0]
                    if (v['recorded'], v['exported'], v['private_fences'], v['invalidations'], v['app_fence_bypass']) != ((1, 1, 2, 2, 0) if wsi else (4, 3, 5, 6, 1)):
                        raise ValueError('Bounded producer counts incorrect')
                    if any(v[k] for k in ('halted', 'refused', 'allocation_failures', 'original_failures', 'gpu_waits_or_extra_queue_submissions')):
                        raise ValueError('Producer failed or changed queue work')
                    if not wsi:
                        for request, count in ((102, 0), (112, 1), (121, 2)):
                            if not any(v['external_request'] == request and v['exported'] == count and not v['final'] for v in reports):
                                raise ValueError('Pending/stale frame exported')
                elif reports or private:
                    raise ValueError('Disabled producer emitted exports')
                cases.append({'environment':name, 'actual_wsi':wsi, 'producer_enabled':enabled,
                              'unchanged_original_bytes':sum(map(len, pixels.values())) if enabled else 0,
                              'private_depth_bytes':sum((directory/('case-'+str(i)+'.private')).stat().st_size for i in range(3)) if mode == 'direct' else sum(p.stat().st_size for p in directory.glob('depth-copy-*.bin')),
                              'validation_errors':0, 'layer_exports':len(private)})
    sources = ['native/vulkan/depth_copy_readback.hpp', 'native/vulkan/depth_copy_readback.cpp',
               'native/vulkan/trace_layer.cpp', 'native/tests/depth_copy_readback_tests.cpp',
               'native/tests/depth_copy_vulkan.cpp', 'native/tests/depth_copy.vert', 'native/tests/depth_copy.frag',
               'native/tests/depth_transfer_fixture.hpp', 'native/tests/present_layer_vulkan.cpp',
               'native/tests/vulkan_trace_tests.cpp', 'tools/depth_copy_validate.py']
    proof = {'schema_version':1, 'owned_scene':True, 'game_rendered':False, 'xr_submission':False,
             'cases':cases, 'unit':unit, 'validation_errors':0,
             'layer_sha256':probe.digest(a.layer/'libd4vr_vulkan_trace.so'),
             'fixture_sha256':probe.digest(a.fixture), 'wsi_fixture_sha256':probe.digest(a.wsi_fixture),
             'unit_sha256':probe.digest(a.unit), 'forwarding_sha256':probe.digest(a.forwarding),
             'source_sha256':{p:probe.digest(root/p) for p in sources}}
    probe.write_json(a.output/'proof.json', proof)
    return proof


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('output', 'fixture', 'wsi-fixture', 'unit', 'forwarding', 'compiler', 'layer', 'validation-layer'):
        p.add_argument('--'+key, type=Path, required=True)
    p.add_argument('--runtime', type=Path)
    try:
        print(json.dumps(validate(p.parse_args()), indent=2))
    except (OSError, ValueError, subprocess.SubprocessError) as e:
        raise SystemExit(str(e))
