#!/usr/bin/env python3
"""Run owned X11/WSI controls for the CPU source-consumer observer.

Keeps the existing held depth/color observer, application descriptor features,
queries and GPU workload identical in every control. Only the new consumer
observer changes. The separate read-only source_consumer_validate auditor must
check the resulting reports; this runner alone is not that certificate.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import eye_control
import held_depth_validate
import paired_capture
import probe
import vulkan_trace
from source_history_validate import common_artifacts

def validate(a):
    if a.output.exists():
        raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700, parents=True)
    root = Path(__file__).resolve().parents[1]
    host = paired_capture.host_command()

    def run(command, log):
        result = subprocess.run(host+command, capture_output=True, text=True, timeout=60)
        output = result.stdout+result.stderr
        log.write_text(output)
        if result.returncode or any(marker in output for marker in ('VUID-', 'Validation Error', 'SYNC-HAZARD-')):
            raise ValueError(str(log.parent)+': inspect '+log.name)
        return output

    unit = json.loads(run([str(a.unit.resolve())], a.output/'unit.log').strip())
    if unit.get('source_history_observation_passed') is not True:
        raise ValueError('Bounded CPU source history controls missing')
    run([str(a.forwarding.resolve())], a.output/'forwarding.log')
    shaders = []
    for ext in ('vert', 'frag'):
        path = (a.output/('present.'+ext+'.spv')).resolve()
        run([str(a.compiler.resolve()), '-V', '--target-env', 'vulkan1.3',
             str(root/('native/tests/present_eye.'+ext)), '-o', str(path)], a.output/('shader-'+ext+'.log'))
        if ext == 'vert':
            held_depth_validate.marked_vertex(path)
        if a.validator:
            run([str(a.validator.resolve()), '--target-env', 'vulkan1.3', str(path)], a.output/('spirv-'+ext+'.log'))
        shaders.append(str(path))
    cases = []
    for runtime in (False, True) if a.runtime else (False,):
        baseline = None
        for enabled in (False, True):
            name = ('consumer-on' if enabled else 'consumer-off')+('-runtime' if runtime else '-native')
            directory = (a.output/name).resolve();directory.mkdir(mode=0o700)
            variables = {'VK_INSTANCE_LAYERS': vulkan_trace.LAYER+':VK_LAYER_KHRONOS_validation',
                         'VK_ADD_LAYER_PATH': str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),
                         'VK_LAYER_ENABLES': 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                         'VK_LAYER_VALIDATE_SYNC': '1', 'VK_LOADER_DEBUG': 'layer',
                         'D4VR_TRACE_DIR': str(directory), 'D4VR_SEQUENTIAL_CAPTURE_DIR': str(directory),
                         'D4VR_GPU_CAMERA_UPDATES': '1', 'D4VR_POSE_CAPTURE_DIAGNOSTIC': '1',
                         'D4VR_DIAGNOSTIC_VIEW_HOLD': '1', 'D4VR_POSE_FIXTURE_MONOSCOPIC': '1',
                         'D4VR_HELD_DEPTH_FIXTURE': '1', 'D4VR_SOURCE_HISTORY_FIXTURE': '1',
                         'D4VR_HELD_DEPTH_COLOR_DIAGNOSTIC': '1', 'D4VR_DEPTH_COPY_READBACK_DIAGNOSTIC': '1',
                         'D4VR_DEPTH_SOURCE_HISTORY': '1',
                         'D4VR_SOURCE_CONSUMER_HISTORY': '1' if enabled else '0',
                         'D4VR_SOURCE_CONSUMER_FIXTURE': '1',
                         'D4VR_SOURCE_HISTORY_WIDTH': '256', 'D4VR_SOURCE_HISTORY_HEIGHT': '256',
                         'D4VR_SOURCE_HISTORY_FORMAT': '126'}
            for eye, side in enumerate(('left', 'right')):
                path = directory/(side+'.bin');path.write_bytes(eye_control.encode(eye, separation=.128));path.chmod(0o600)
                variables['D4VR_PAIR_'+side.upper()+'_FILE'] = str(path)
            command = ['/usr/bin/env']
            for key in ('VK_LAYER_PATH', 'VK_INSTANCE_LAYERS', 'VK_ADD_LAYER_PATH',
                        'D4VR_TRACE_XR_BINDING', 'D4VR_COOPERATIVE_XR_DIAGNOSTIC', 'D4VR_LAYER_REPLAY_POC',
                        'D4VR_FRAME_TRACE', 'D4VR_EYE_STATE_FILE', 'D4VR_PAIR_LEFT_FILE', 'D4VR_PAIR_RIGHT_FILE',
                        'D4VR_SEQUENTIAL_CAPTURE_DIR', 'D4VR_GPU_CAPTURE_DRY_RUN', 'D4VR_GPU_CAMERA_UPDATES',
                        'D4VR_POSE_CAPTURE_DIAGNOSTIC', 'D4VR_DIAGNOSTIC_VIEW_HOLD',
                        'D4VR_DEPTH_AUDIT', 'D4VR_DEPTH_READBACK_DIAGNOSTIC', 'D4VR_DEPTH_COPY_READBACK_DIAGNOSTIC',
                        'D4VR_HELD_DEPTH_COLOR_DIAGNOSTIC', 'D4VR_HELD_DEPTH_FIXTURE',
                        'D4VR_SOURCE_HISTORY_FIXTURE', 'D4VR_DEPTH_SOURCE_HISTORY',
                        'D4VR_SOURCE_CONSUMER_HISTORY','D4VR_SOURCE_CONSUMER_FIXTURE',
                        'D4VR_SOURCE_HISTORY_WIDTH', 'D4VR_SOURCE_HISTORY_HEIGHT', 'D4VR_SOURCE_HISTORY_FORMAT',
                        'D4VR_DEPTH_TRANSFER_FIXTURE', 'D4VR_DEPTH_COPY_FIXTURE',
                        'D4VR_DEPTH_LAYER_FIXTURE', 'D4VR_DEPTH_AUDIT_FIXTURE',
                        'D4VR_POSE_FIXTURE_SHORT', 'D4VR_POSE_FIXTURE_FOREIGN', 'D4VR_POSE_FIXTURE_CANCEL',
                        'D4VR_POSE_FIXTURE_CALIBRATED', 'D4VR_POSE_FIXTURE_MONOSCOPIC', 'D4VR_METRIC_SOURCE_DEPTH_SCALE'):
                command += ['-u', key]
            command += [key+'='+value for key, value in variables.items()]
            if runtime:
                command += [str(a.runtime.resolve()), '--batch', '--']
            command += [str(a.fixture.resolve()), *shaders, str(directory)]
            probe.write_json(directory/'command.json', host+command)
            data = common_artifacts(directory, run(command, directory/'run.log'))
            if baseline is None:
                baseline = data['pixels']
            elif baseline != data['pixels']:
                raise ValueError('CPU history observation changed original color or geometric depth bytes')
            history = [] # Independent source_consumer_validate checks exact COPY refs and expanded context afterwards.
            blit = None
            if a.require_blit_stages:
                from blit_provenance_validate import audit_directory
                blit = audit_directory(directory, require_bridge=enabled, require_stages=True)
            cases.append({'case': name, 'observer_enabled': enabled, 'actual_x11_wsi': True,
                          'original_controls': data['summary'], 'history_copy_matches': history,
                          'delivered_blit_lifecycle_audit': blit,
                          'exact_original_reference_bytes': sum(map(len, data['pixels'].values())),
                          'all_original_bytes_unchanged': enabled, 'validation_errors': 0})
            print(name+': passed', flush=True)
    sources = ['native/tests/present_layer_vulkan.cpp', 'native/tests/held_depth_fixture.hpp',
               'native/tests/present_eye.vert', 'native/tests/present_eye.frag',
               'native/vulkan/trace_layer.cpp', 'native/vulkan/source_history.hpp',
               'native/vulkan/source_history.cpp', 'native/core/source_history_id.hpp',
               'native/tests/source_history_tests.cpp', 'native/vulkan/frame_trace.hpp',
               'native/vulkan/depth_copy_readback.hpp', 'native/vulkan/depth_copy_readback.cpp',
               'tools/source_history_validate.py', 'tools/held_depth_validate.py',
               'native/tests/source_consumer_fixture.hpp',
               'tools/source_consumer_run.py', 'tools/blit_provenance_validate.py', 'native/tests/vulkan_trace_tests.cpp',
               'native/core/blit_provenance.hpp', 'native/vulkan/present_capture.hpp',
               'native/vulkan/present_capture.cpp', 'native/vulkan/capture_projection.cpp',
               'native/xr/projection_source.hpp', 'native/tests/present_stream_tests.cpp']
    proof = {'schema_version': 1, 'owned_geometry': True, 'actual_wsi_loader': True,
             'observation_only': True, 'gpu_execution_order_known': False,
             'extra_gpu_commands_or_submissions_from_observer': 0, 'game_rendered': False,
             'xr_submission': False, 'common_scene_time_known': False, 'whole_scene_verified': False,
             'validation_errors': 0, 'unit': unit, 'cases': cases,
             'layer_sha256': probe.digest(a.layer/vulkan_trace.LIBRARY),
             'fixture_sha256': probe.digest(a.fixture), 'unit_sha256': probe.digest(a.unit),
             'forwarding_sha256': probe.digest(a.forwarding),
             'source_sha256': {path: probe.digest(root/path) for path in sources}}
    probe.write_json(a.output/'proof.json', proof)
    return proof


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('fixture', 'compiler', 'layer', 'validation-layer', 'unit', 'forwarding', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--require-blit-stages', action='store_true', help='Audit saved lifecycle values against original app arguments/pixels')
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--validator', type=Path)
    try:
        print(json.dumps(validate(parser.parse_args()), indent=2))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
