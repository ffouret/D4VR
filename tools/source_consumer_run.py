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
    require_raw = bool(getattr(a, 'require_writer_raw', False))
    require_facets = bool(getattr(a, 'require_writer_facets', False)) or require_raw
    require_state = bool(getattr(a, 'require_writer_state', False)) or require_facets
    frozen_sources = {}
    frozen_binaries = {}
    if require_state:
        if not a.require_writer_prefix or not a.require_blit_stages or not a.writer_state_unit:
            raise ValueError('Writer-state WSI controls require saved prefixes, lifecycle stages and their CPU integration unit')
        freeze = ['native/tests/present_layer_vulkan.cpp', 'native/tests/vulkan_fixture.hpp',
                  'native/tests/sampled_writer_fixture.hpp', 'native/tests/source_consumer_fixture.hpp',
                  'native/tests/held_depth_fixture.hpp', 'native/tests/present_eye.vert', 'native/tests/present_eye.frag',
                  'native/vulkan/trace_layer.cpp', 'native/vulkan/source_history.cpp', 'native/vulkan/source_history.hpp',
                  'native/vulkan/sampled_writer_state.hpp', 'native/tests/source_writer_state_tests.cpp',
                  'native/tests/sampled_writer_state_tests.cpp', 'native/tests/sampled_writer_forwarding.hpp',
                  'native/core/captured_writer_prefix.hpp', 'native/core/blit_provenance.hpp',
                  'native/vulkan/present_capture.hpp', 'native/vulkan/present_capture.cpp',
                  'tools/source_consumer_run.py', 'tools/captured_writer_validate.py',
                  'tools/blit_provenance_validate.py', 'tools/sampled_writer_state_validate.py',
                  'tests/test_sampled_writer_state_validate.py']
        frozen_sources = {path: probe.digest(root/path) for path in freeze}
        frozen_binaries = {str(path.resolve()): probe.digest(path) for path in
                           (a.fixture, a.layer/vulkan_trace.LIBRARY, a.unit, a.forwarding, a.writer_unit, a.writer_state_unit)}
        probe.write_json(a.output/'before-execution-sha256.json', {'sources': frozen_sources, 'binaries': frozen_binaries})

    def check_freeze():
        if any(probe.digest(root/path) != expected for path, expected in frozen_sources.items()):
            raise ValueError('Writer-state source changed after the execution inputs were frozen')
        if any(probe.digest(Path(path)) != expected for path, expected in frozen_binaries.items()):
            raise ValueError('Writer-state executable changed after its execution input was frozen')

    def run(command, log):
        check_freeze()
        result = subprocess.run(host+command, capture_output=True, text=True, timeout=60)
        check_freeze()
        output = result.stdout+result.stderr
        log.write_text(output)
        if result.returncode or any(marker in output for marker in ('VUID-', 'Validation Error', 'SYNC-HAZARD-')):
            raise ValueError(str(log.parent)+': inspect '+log.name)
        return output

    unit = json.loads(run([str(a.unit.resolve())], a.output/'unit.log').strip())
    if unit.get('source_history_observation_passed') is not True:
        raise ValueError('Bounded CPU source history controls missing')
    run([str(a.forwarding.resolve())], a.output/'forwarding.log')
    writer_unit = None
    if a.require_writer_prefix:
        if not a.writer_unit:
            raise ValueError('Captured writer WSI controls require their CPU unit executable')
        writer_unit = json.loads(run([str(a.writer_unit.resolve())], a.output/'writer-unit.log').strip())
        if writer_unit.get('captured_writer_prefix_passed') is not True:
            raise ValueError('Bounded captured writer CPU controls missing')
    state_unit = None
    if require_state:
        state_unit = json.loads(run([str(a.writer_state_unit.resolve())], a.output/'writer-state-unit.log').strip())
        if state_unit.get('source_writer_state_passed') is not True or state_unit.get('gpu_dispatch_waits_or_rejections') != 0:
            raise ValueError('Read-only CPU per-writer state integration controls missing')
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
        baseline_state_controls = None
        for enabled in (False, True):
            prefix = 'writer-raw' if require_raw else 'writer-facets' if require_facets else 'writer-state' if require_state else 'consumer'
            name = prefix+('-on' if enabled else '-off')+('-runtime' if runtime else '-native')
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
                         'D4VR_SOURCE_CONSUMER_HISTORY': '1' if enabled or require_state else '0',
                         'D4VR_SOURCE_CONSUMER_FIXTURE': '1',
                         'D4VR_SOURCE_HISTORY_WIDTH': '256', 'D4VR_SOURCE_HISTORY_HEIGHT': '256',
                         'D4VR_SOURCE_HISTORY_FORMAT': '126'}
            if require_state:
                variables['D4VR_SAMPLED_WRITER_FIXTURE'] = '1'
                variables['D4VR_SAMPLED_WRITER_STATE'] = '1' if enabled else '0'
            if require_facets:
                variables['D4VR_SAMPLED_WRITER_FSR_FIXTURE'] = '1'
            if require_raw:
                variables['D4VR_SAMPLED_WRITER_RAW_FIXTURE'] = '1'
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
                        'D4VR_SAMPLED_WRITER_FIXTURE', 'D4VR_SAMPLED_WRITER_STATE',
                        'D4VR_SAMPLED_WRITER_FSR_FIXTURE',
                        'D4VR_SAMPLED_WRITER_RAW_FIXTURE',
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
                blit = audit_directory(directory, require_bridge=enabled or require_state, require_stages=True)
            writer = None
            if a.require_writer_prefix:
                from captured_writer_validate import audit_directory
                writer = audit_directory(directory, require_available=enabled or require_state)
            state = None
            if require_state:
                from sampled_writer_state_validate import audit_directory
                state = audit_directory(directory, observer_enabled=enabled, require_facets=require_facets, **({'require_raw': True} if require_raw else {}))
                summary = data['summary']
                if (summary.get('sampled_writer_state_fixture') is not True
                        or summary.get('original_writer_draw_states') != 128
                        or summary.get('fragment_push_constants_read_by_shader') is not False
                        or summary.get('writer_dynamic_count_enabled') != state['dynamic_count_enabled']
                        or summary.get('writer_maintenance6_enabled') != state['maintenance6_enabled']):
                    raise ValueError('Original fixture feature/unused-fragment controls differ from their saved draw arguments')
                if require_facets and (summary.get('sampled_writer_fsr_fixture') is not True
                        or summary.get('writer_attachment_fsr_enabled') is not True
                        or summary.get('writer_pipeline_fsr_flag') != 0x200000
                        or any(summary.get(key) is not False for key in ('writer_rate_attachment_present',
                                   'writer_static_rate_pnext_present', 'writer_dynamic_rate_declared'))):
                    raise ValueError('Required flag-only FSR application configuration was not executed exactly')
                if require_raw and (summary.get('sampled_writer_raw_fixture') is not True
                        or summary.get('depth_clip_feature_enabled') is not True
                        or summary.get('depth_clip_chain_type') != 1000102001
                        or summary.get('depth_clip_state_flags') != 0
                        or summary.get('depth_clip_enable_payload') is not True):
                    raise ValueError('Required feature-enabled fixed DepthClip application configuration was not executed exactly')
                controls = (state['dynamic_count_enabled'], state['maintenance6_enabled'], state['original_setter_aliases'])
                if baseline_state_controls is None:
                    baseline_state_controls = controls
                elif baseline_state_controls != controls:
                    raise ValueError('Original writer-state fixture features or setter aliases changed between observer off/on')
            cases.append({'case': name, 'observer_enabled': enabled, 'actual_x11_wsi': True,
                          'original_controls': data['summary'], 'history_copy_matches': history,
                          'delivered_blit_lifecycle_audit': blit, 'captured_writer_audit': writer,
                          'exact_original_reference_bytes': sum(map(len, data['pixels'].values())),
                          'all_original_bytes_unchanged': enabled, 'validation_errors': 0})
            if require_state:
                cases[-1]['sampled_writer_state_audit'] = state
            print(name+': passed', flush=True)
    sources = ['native/tests/present_layer_vulkan.cpp', 'native/tests/held_depth_fixture.hpp',
               'native/tests/present_eye.vert', 'native/tests/present_eye.frag',
               'native/vulkan/trace_layer.cpp', 'native/vulkan/source_history.hpp',
               'native/vulkan/source_history.cpp', 'native/core/source_history_id.hpp',
               'native/tests/source_history_tests.cpp', 'native/vulkan/frame_trace.hpp',
               'native/vulkan/depth_copy_readback.hpp', 'native/vulkan/depth_copy_readback.cpp',
               'tools/source_history_validate.py', 'tools/held_depth_validate.py',
               'native/tests/source_consumer_fixture.hpp',
               'tools/source_consumer_run.py', 'tools/blit_provenance_validate.py', 'tools/captured_writer_validate.py',
               'native/core/captured_writer_prefix.hpp', 'native/tests/captured_writer_tests.cpp', 'native/tests/vulkan_trace_tests.cpp',
               'native/core/blit_provenance.hpp', 'native/vulkan/present_capture.hpp',
               'native/vulkan/present_capture.cpp', 'native/vulkan/capture_projection.cpp',
               'native/xr/projection_source.hpp', 'native/tests/present_stream_tests.cpp']
    proof = {'schema_version': 1, 'owned_geometry': True, 'actual_wsi_loader': True,
             'observation_only': True, 'gpu_execution_order_known': False,
             'extra_gpu_commands_or_submissions_from_observer': 0, 'game_rendered': False,
             'xr_submission': False, 'common_scene_time_known': False, 'whole_scene_verified': False,
             'validation_errors': 0, 'unit': unit, 'captured_writer_unit': writer_unit, 'cases': cases,
             'layer_sha256': probe.digest(a.layer/vulkan_trace.LIBRARY),
             'fixture_sha256': probe.digest(a.fixture), 'unit_sha256': probe.digest(a.unit),
             'forwarding_sha256': probe.digest(a.forwarding),
             'source_sha256': {path: probe.digest(root/path) for path in sources}}
    if require_state:
        check_freeze()
        proof.update(sampled_writer_state_observer=True, sampled_writer_state_unit=state_unit,
                     sampled_writer_state_source_sha256=frozen_sources,
                     before_execution_binary_sha256=frozen_binaries,
                     only_new_observer_varies=True, descriptor_placement_known=False,
                     sampled_resource_known=False, coverage_known=False)
        if require_facets:
            produced_version = 3 if any(c['sampled_writer_state_audit'].get('newly_produced_schema3_draws', 0) for c in cases) else 2
            if require_raw and produced_version != 3:
                raise ValueError('No actual schema3 draw state was delivered by the raw-argument control')
            proof.update(sampled_writer_facets_required=True, writer_snapshot_schema_version=produced_version,
                         minimum_required_writer_snapshot_schema_version=3 if require_raw else 2,
                         original_application_fsr_flag=0x200000, original_attachment_fsr_feature_enabled=True,
                         original_rate_attachment_present=False, original_static_rate_pnext_present=False,
                         original_dynamic_rate_declared=False, fragment_shading_rate_known=False)
        if require_raw:
            proof.update(sampled_writer_raw_arguments_required=True, original_depth_clip_feature_enabled=True,
                         original_depth_clip_chain_type=1000102001, original_depth_clip_state_flags=0,
                         original_depth_clip_enable_payload=True, observed_depth_clip_enable_known=False,
                         raw_setter_arguments_prove_effective_coverage=False)
    probe.write_json(a.output/'proof.json', proof)
    return proof


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('fixture', 'compiler', 'layer', 'validation-layer', 'unit', 'forwarding', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--writer-unit', type=Path, help='Portable captured-writer CPU controls')
    parser.add_argument('--require-writer-prefix', action='store_true', help='Audit saved source prefixes against owned render scopes')
    parser.add_argument('--require-blit-stages', action='store_true', help='Audit saved lifecycle values against original app arguments/pixels')
    parser.add_argument('--require-writer-state', action='store_true', help='Opt-in state observer off/on; same original state calls and consumer history in both controls')
    parser.add_argument('--require-writer-facets', action='store_true', help='Require schema2 independent facets under an identical, feature-enabled FSR-attachment pipeline flag off/on control; unsupported feature is not a pass')
    parser.add_argument('--require-writer-raw', action='store_true', help='Require schema3 raw setter provenance and identical original feature-enabled DepthClip declaration off/on; no clipping-payload or coverage inference')
    parser.add_argument('--writer-state-unit', type=Path, help='Portable SourceHistory per-writer state integration controls')
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--validator', type=Path)
    try:
        print(json.dumps(validate(parser.parse_args()), indent=2))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
