#!/usr/bin/env python3
"""Validate a bounded CPU SourceHistory observer against owned X11/WSI work.

The four executions compare the same held geometric depth/color application
with history observation disabled/enabled, natively and in Steam Runtime.
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


def common_artifacts(directory, log):
    summaries = [json.loads(line) for line in log.splitlines()
                 if line.startswith('{"presentation_layer_fixture_passed"')]
    if len(summaries) != 1:
        raise ValueError('Owned WSI application summary missing')
    summary = summaries[0]
    if (summary.get('frames'), summary.get('source_history_fixture'),
        summary.get('original_depth_copies'), summary.get('original_late_source_clears'),
        summary.get('original_twin_clears'), summary.get('twin_lifetimes'),
        summary.get('original_null_fence_submits'), summary.get('original_copy_stage_mask'),
        summary.get('original_blit_stage_mask'), summary.get('history_gpu_execution_order_inferred')) != (64, True, 64, 64, 64, 2, 64, 1 << 32, 1 << 34, False):
        raise ValueError('Original fixture COPY/clear/lifetime/scope controls differ')
    for layer in (vulkan_trace.LAYER, 'VK_LAYER_KHRONOS_validation'):
        if 'Insert instance layer "'+layer+'"' not in log or 'Inserted device layer "'+layer+'"' not in log:
            raise ValueError('Real-loader layer insertion was not verified')
    rows = [json.loads(line) for path in directory.glob('vulkan-*.jsonl')
            for line in path.read_text().splitlines()]
    errors = {'pose_capture_error', 'presentation_error', 'camera_update_fatal',
              'paired_pipeline_error', 'depth_copy_error', 'depth_copy_init_skipped',
              'held_depth_initialization_refused', 'held_depth_export_error',
              'source_history_error', 'source_history_init_refused', 'source_history_unavailable'}
    if any(row.get('event') in errors for row in rows):
        raise ValueError('Layer initialization/runtime error')
    finals = [row for row in rows if row.get('event') == 'device_destroy']
    if len(finals) != 1 or (finals[0]['present_calls'], finals[0]['submissions'],
                           finals[0]['submitted_command_buffers'], finals[0]['commands']['draw']) != (64, 320, 64, 128):
        raise ValueError('Observer changed original application submissions or draws')
    state, pool = finals[0]['pose_capture'], finals[0]['pose_producer']
    if state['pairs'] != 2 or not state['stopped'] or any(state[k] for k in ('active', 'consumer_pending', 'fault', 'fixed_view_held')) or pool['outstanding_leases'] or pool['leases_issued'] != pool['retire_submissions']:
        raise ValueError('Held color teardown incomplete')
    fixture = json.loads((directory/'source-history-fixture.json').read_text())
    if fixture['source_image'] in (fixture['destination_image'], fixture['twin_life_0'], fixture['twin_life_1']) or not fixture['twin_life_0'] or not fixture['twin_life_1'] or fixture['twin_recreated_at_frame'] != 32 or fixture['gpu_execution_order_inferred']:
        raise ValueError('Known source/twin application lifetimes missing')
    pixels = {path.name: path.read_bytes() for path in directory.glob('*.pixels')
              if path.name.startswith(('source-frame-', 'held-depth-frame-'))}
    if len(pixels) != 128:
        raise ValueError('Independent original per-frame references missing')
    pairs = {}
    for path in directory.glob('pose-pair-*.json'):
        pair = json.loads(path.read_text())
        request = pair['request']
        if request in pairs or request not in (1, 2) or pair['mode'] != 'successive_diagnostic' or any(pair[k] for k in ('actual_xr_pose_source', 'xr_submission', 'whole_scene_verified', 'common_scene_time_known')):
            raise ValueError('Successive diagnostic color identity/claims differ')
        if pair['diagnostic_sample'] != (directory/('pose-input-'+str(request)+'.txt')).read_text():
            raise ValueError('External diagnostic sample was retagged')
        raw = path.with_suffix('.pixels').read_bytes()
        size = pair['width']*pair['height']*4
        if len(raw) != size*2 or raw[:size] != raw[size:]:
            raise ValueError('Held color consumer is not monoscopic')
        for eye, receipt in enumerate(pair['eyes']):
            camera = receipt['camera_provenance']
            if receipt['eye'] != eye or not camera['same_diagnostic_packet'] or camera['copy_submit']['status'] != 'uniform' or camera['selected_draw_interval']['status'] != 'uniform' or camera['copy_submit']['packet_abi_bytes_hex'] != camera['selected_draw_interval']['packet_abi_bytes_hex']:
                raise ValueError('Selected draw/copy packet differs')
            if raw[eye*size:(eye+1)*size] != pixels['source-frame-'+str(receipt['present']-1)+'.pixels']:
                raise ValueError('Color consumer differs from independent original color')
        held_depth_validate.packet_bytes(pair['eyes'][0]['camera_provenance']['copy_submit'], request+1)
        if pair['eyes'][1]['present'] <= pair['eyes'][0]['present']:
            raise ValueError('Color source ordering is not successive')
        pairs[request] = pair
    if sorted(pairs) != [1, 2]:
        raise ValueError('Two held color pairs required')
    geometry = {}
    for frame, color_frame, request in ((16, 24, 1), (48, 56, 2)):
        geometry[request] = held_depth_validate.geometric_reference(
            pixels['held-depth-frame-'+str(frame)+'.pixels'],
            pixels['source-frame-'+str(color_frame)+'.pixels'],
            pairs[request]['width'], pairs[request]['height'])
    exports = {}
    for path in directory.glob('depth-copy-*.bin'):
        meta = json.loads(path.with_suffix('.json').read_text())
        request = meta['request']
        if request not in (3, 5) or request in exports:
            raise ValueError('Refused depth request exported or export duplicated')
        pose_request, frame = (1, 16) if request == 3 else (2, 48)
        expected = held_depth_validate.packet_bytes(pairs[pose_request]['eyes'][0]['camera_provenance']['copy_submit'], pose_request+1)
        if held_depth_validate.packet_bytes(meta['expected_held_packet'], pose_request+1) != expected or held_depth_validate.packet_bytes(meta['copy_submit_camera'], pose_request+1) != expected or meta['recording_eye'] != 0 or meta['capture_submission'] <= max(e['camera_provenance']['copy_submit']['last_submission'] for e in pairs[pose_request]['eyes']):
            raise ValueError('Depth lost its exact preceding held packet')
        if not meta['raw_complete'] or meta['format'] != 126 or meta['epoch'] != 1 or path.read_bytes() != pixels['held-depth-frame-'+str(frame)+'.pixels'] or any(meta[k] for k in ('whole_scene_verified', 'initialized_pixels_verified', 'common_scene_time_known', 'xr_pose_known')):
            raise ValueError('Independent original depth pixels or claim limits differ')
        exports[request] = meta
    if sorted(exports) != [3, 5]:
        raise ValueError('Two successful held-depth exports required')
    rejected = [row for row in rows if row.get('event') == 'held_depth_request_rejected']
    if [(row['request'], row['reason']) for row in rejected] != [(1, 'held_depth_consumer_uninitialized'), (2, 'held_depth_color_active'), (4, 'held_depth_color_active'), (6, 'held_depth_terminal_or_fault')]:
        raise ValueError('Held-phase explicit refusals changed')
    return {'summary': summary, 'fixture': fixture, 'pixels': pixels,
            'exports': exports, 'geometry': geometry, 'rows': rows}


def check_history(directory, data, enabled):
    wrappers = [json.loads(path.read_text()) for path in directory.glob('source-history-*.json')
                if path.name != 'source-history-fixture.json']
    references = [meta.get('source_history') for meta in data['exports'].values()]
    if not enabled:
        if wrappers or any(ref is not None for ref in references):
            raise ValueError('Disabled history observer emitted reports or copy references')
        return []
    if not wrappers or any(not isinstance(ref, dict) for ref in references):
        raise ValueError('Enabled observer lacks aggregate or exact COPY references')
    wrappers.sort(key=lambda row: row['report_sequence'])
    if len(wrappers) > 16 or [row['external_request'] for row in wrappers if not row['final']] != [1, 2, 3, 4] or sum(bool(row['final']) for row in wrappers) != 1 or any(row['frame_trace_enabled'] for row in wrappers):
        raise ValueError('Bounded on-demand CPU reports or frame-trace isolation differ')
    aggregates = [row['history'] for row in wrappers]
    image_ids = set()
    report_by_ref = {}
    shader_receipts = {}
    for stage, suffix in ((1, 'vert'), (16, 'frag')):
        raw = (directory.parent/('present.'+suffix+'.spv')).read_bytes()
        fnv = 14695981039346656037
        for value in raw:
            fnv = ((fnv ^ value)*1099511628211) & ((1 << 64)-1)
        shader_receipts[stage] = (fnv, len(raw))
    for aggregate in aggregates:
        if aggregate.get('observation_only') is not True or aggregate.get('gpu_execution_order_known') is not False or aggregate.get('last_gpu_writer_known') is not False:
            raise ValueError('CPU history inferred GPU execution order')
        if any(aggregate[key] for key in ('gpu_commands_inserted', 'application_calls_rejected', 'faulted', 'serialization_truncated', 'overflow', 'failed_cpu_returns', 'duplicate_after_calls')):
            raise ValueError('Observer inserted/rejected work or lost bounded observations')
        limits = aggregate['limits']
        if aggregate['live_event_nodes'] > limits['live_events'] or aggregate['snapshot_event_references'] > limits['snapshot_references'] or len(aggregate['reports']) > limits['reports']:
            raise ValueError('CPU history exceeded its declared memory/reference/report bounds')
        if aggregate['candidate_images_scope'] != 'live_registry_at_report' or aggregate['candidate_images_truncated'] or len(aggregate['candidate_images']) > 64:
            raise ValueError('Candidate lifetime catalog was truncated or unbounded')
        image_ids.update(row['id'] for row in aggregate['candidate_images']
                         if row['format'] == 126 and (row['width'], row['height']) == (data['fixture']['width'], data['fixture']['height']))
        for report in aggregate['reports']:
            if report['gpu_execution_order_known'] or report['last_gpu_writer_known'] or report['catalog_truncated']:
                raise ValueError('Source report inferred execution order or exceeded its bounded catalog')
            event_count = sum(len(command['events']) for occurrence in report['occurrences'] for command in occurrence['commands'])
            if len(report['occurrences']) > limits['occurrences_per_report'] or event_count > limits['events_per_report']:
                raise ValueError('Source report exceeded its captured occurrence/event tail bounds')
            if not {'image_memory_aliasing_not_observed', 'all_write_routes_not_observed'}.issubset(report['incomplete']):
                raise ValueError('History omitted its known alias/write-route limitations')
            ref = report['target_copy']
            key = tuple(ref[k] for k in ('observer_session', 'source_image', 'destination_image', 'recording', 'ordinal'))
            report_by_ref[key] = report
    if len(image_ids) < 4:
        raise ValueError('Source/destination and two distinct same-sized twin lifetimes were conflated')
    source_ids, destination_ids, result = set(), set(), []
    for request, meta in sorted(data['exports'].items()):
        ref = meta['source_history']
        key = tuple(ref[k] for k in ('observer_session', 'source_image', 'destination_image', 'recording', 'ordinal'))
        if not all(isinstance(value, int) and value > 0 for value in key) or ref['source_image'] == ref['destination_image']:
            raise ValueError('COPY history reference is unknown or aliases its destination')
        if key not in report_by_ref:
            raise ValueError('Export COPY reference has no exact history report')
        report = report_by_ref[key]
        images = {image['id']: image for image in report['images']}
        if any(image not in images or not images[image]['known'] or images[image]['format'] != 126 or (images[image]['width'], images[image]['height']) != (data['fixture']['width'], data['fixture']['height']) for image in (ref['source_image'], ref['destination_image'])):
            raise ValueError('COPY source/destination exact lifetime metadata is missing')
        source_ids.add(ref['source_image']);destination_ids.add(ref['destination_image'])
        targets = [(occurrence, command) for occurrence in report['occurrences']
                   for command in occurrence['commands']
                   if occurrence['submission'] == report['target_submission']
                   and command['recording'] == ref['recording'] and command['is_target_recording']]
        if len(targets) != 1:
            raise ValueError('COPY history target recording/submission is ambiguous')
        occurrence, command = targets[0]
        if not occurrence['complete'] or occurrence['result'] != 0 or occurrence['capture_submission'] != meta['capture_submission'] or command['recording_eye'] != 0:
            raise ValueError('COPY history occurrence lacks exact successful application submission')
        semaphores = occurrence['semaphores']
        waits = [item for item in semaphores if not item['signal']]
        signals = [item for item in semaphores if item['signal']]
        if len(waits) != 1 or len(signals) != 1 or waits[0]['stage_mask'] != 1 << 34 or signals[0]['stage_mask'] != (1 << 32 | 1 << 34) or any(not item['lifetime_known'] or not item['value_known'] or item['value'] != 0 or item['device_index'] != 0 or item['batch'] != report['target_batch'] for item in semaphores) or occurrence['family'] != meta['family'] or occurrence['index'] != 0:
            raise ValueError('Observer changed or retagged original queue/semaphore values and COPY/BLIT scopes')
        events = command['events']
        ordinals = [event['ordinal'] for event in events]
        if ordinals != sorted(set(ordinals)) or any(ordinal > ref['ordinal'] for ordinal in ordinals):
            raise ValueError('A later command contaminated the target COPY prefix')
        if any(event['image'] != ref['source_image'] for event in events):
            raise ValueError('A same-sized twin or unrelated image contaminated source history')
        copies = [event for event in events if event['kind'] == 'copy_image' and event['ordinal'] == ref['ordinal']]
        if len(copies) != 1 or copies[0]['destination_image'] != ref['destination_image']:
            raise ValueError('History reference does not identify the exact original COPY')
        begins = [event for event in events if event['kind'] == 'begin_rendering']
        draws = [event for event in events if event['kind'] == 'draw']
        if len(begins) != 1 or len(draws) != 2 or any(event['kind'] == 'clear_depth_stencil' for event in events):
            raise ValueError('Source prefix omitted geometric draws or included the late explicit clear')
        begin = begins[0]
        if not begin['has_clear'] or begin['clear_depth_bits'] != 0x3f800000 or begin['load_op'] != 1 or begin['store_op'] != 0 or begin['area'] != [0, 0, data['fixture']['width'], data['fixture']['height']]:
            raise ValueError('Original geometric attachment clear/area was retagged')
        if any(draw['recording_eye'] != 0 or not draw['original_pipeline_id'] or not draw['selected_pipeline_id'] or not draw['pipeline_substituted'] for draw in draws):
            raise ValueError('Source draw pipeline identity/substitution is unknown')
        catalog = {pipeline['id']: pipeline for pipeline in report['catalog']}
        for draw in draws:
            pipeline = catalog.get(draw['original_pipeline_id'])
            if not pipeline or not pipeline['original_metadata_only'] or pipeline['missing_libraries'] or pipeline['metadata_truncated'] or draw['selected_pipeline_metadata_known']:
                raise ValueError('Original shader catalog was attributed to the substituted pipeline')
            shaders = {shader['stage']: shader for shader in pipeline['shaders']}
            if any(stage not in shaders or not shaders[stage]['known'] or (shaders[stage]['fnv'], shaders[stage]['bytes']) != receipt for stage, receipt in shader_receipts.items()):
                raise ValueError('Recorded original shader fingerprint differs from the owned source SPIR-V')
        prior_late = [event for prior in report['occurrences']
                      if prior['submission'] < report['target_submission']
                      for prior_command in prior['commands'] for event in prior_command['events']
                      if event['kind'] == 'clear_depth_stencil' and event['image'] == ref['source_image'] and event['clear_depth_bits'] == 0x3f200000]
        if not prior_late:
            raise ValueError('Known late source clear was not observed in earlier application recordings')
        if any(event['kind'] == 'clear_depth_stencil' and event['clear_depth_bits'] == 0x3f400000 for prior in report['occurrences'] for prior_command in prior['commands'] for event in prior_command['events']):
            raise ValueError('The known twin clear was relabeled as source-image history')
        result.append({'request': request, 'target_copy': ref,
                       'target_submission': report['target_submission'],
                       'capture_submission': occurrence['capture_submission'],
                       'original_semaphore_scopes_exact': True,
                       'target_prefix_events': len(events), 'geometric_draws': len(draws),
                       'prior_late_source_clears_observed': len(prior_late),
                       'history_prefix_missing': report['history_prefix_missing'],
                       'target_command_gaps': command['gaps'],
                       'original_shader_fingerprints_exact': True,
                       'substituted_pipeline_metadata_known': False,
                       'incomplete': report.get('incomplete', [])})
    if len(source_ids) != 1 or len(destination_ids) != 1:
        raise ValueError('Stable geometric source/destination lifetimes changed identity')
    return result


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
            name = ('history-on' if enabled else 'history-off')+('-runtime' if runtime else '-native')
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
                         'D4VR_DEPTH_SOURCE_HISTORY': '1' if enabled else '0',
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
            history = check_history(directory, data, enabled)
            cases.append({'case': name, 'observer_enabled': enabled, 'actual_x11_wsi': True,
                          'original_controls': data['summary'], 'history_copy_matches': history,
                          'exact_original_reference_bytes': sum(map(len, data['pixels'].values())),
                          'all_original_bytes_unchanged': enabled, 'validation_errors': 0})
            print(name+': passed', flush=True)
    sources = ['native/tests/present_layer_vulkan.cpp', 'native/tests/held_depth_fixture.hpp',
               'native/tests/present_eye.vert', 'native/tests/present_eye.frag',
               'native/vulkan/trace_layer.cpp', 'native/vulkan/source_history.hpp',
               'native/vulkan/source_history.cpp', 'native/core/source_history_id.hpp',
               'native/tests/source_history_tests.cpp', 'native/vulkan/frame_trace.hpp',
               'native/vulkan/depth_copy_readback.hpp', 'native/vulkan/depth_copy_readback.cpp',
               'tools/source_history_validate.py', 'tools/held_depth_validate.py']
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
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--validator', type=Path)
    try:
        print(json.dumps(validate(parser.parse_args()), indent=2))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
