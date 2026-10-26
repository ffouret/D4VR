#!/usr/bin/env python3
"""Replay held-depth artifact checks and reject altered evidence without a GPU.

All subprocess execution is replaced with a fail-closed recorded-output adapter.
Only disposable copies of an already completed four-case capture are altered.
"""
from __future__ import annotations
import argparse
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
import held_depth_validate
import probe
import vulkan_trace


def _json_update(path, change):
    value = json.loads(path.read_text())
    change(value)
    path.write_text(json.dumps(value)+'\n')


def _depth_meta(directory):
    return next(path for path in sorted(directory.glob('depth-copy-*.json'))
                if json.loads(path.read_text()).get('request') == 3)


def _trace_update(directory, change):
    path = next(directory.glob('vulkan-*.jsonl'))
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    rows = change(rows)
    path.write_text(''.join(json.dumps(row)+'\n' for row in rows))


def _flip_packet(observation):
    packet = bytearray.fromhex(observation['packet_abi_bytes_hex'])
    packet[36] ^= 1  # Matrix value outside the separately asserted translation.
    observation['packet_abi_bytes_hex'] = packet.hex()


def _mutate(name, directory):
    if name == 'private_depth_pixel':
        path = _depth_meta(directory).with_suffix('.bin')
        raw = bytearray(path.read_bytes());raw[0] ^= 1;path.write_bytes(raw)
    elif name == 'clear_only_depth':
        path = directory/'held-depth-frame-16.pixels'
        path.write_bytes(bytes.fromhex('0000803f')*(path.stat().st_size//4))
    elif name == 'expected_packet':
        _json_update(_depth_meta(directory), lambda value: _flip_packet(value['expected_held_packet']))
    elif name == 'submit_packet':
        _json_update(_depth_meta(directory), lambda value: _flip_packet(value['copy_submit_camera']))
    elif name == 'recording_eye':
        _json_update(_depth_meta(directory), lambda value: value.update(recording_eye=1))
    elif name == 'capture_serial_zero':
        _json_update(_depth_meta(directory), lambda value: value.update(capture_submission=0))
    elif name == 'depth_before_color':
        def earlier(value):
            value['capture_submission'] = 1
            value['copy_submit_camera']['first_submission'] = 1
            value['copy_submit_camera']['last_submission'] = 1
        _json_update(_depth_meta(directory), earlier)
    elif name == 'refused_request_export':
        _json_update(_depth_meta(directory), lambda value: value.update(request=2))
    elif name == 'missing_active_refusal':
        _trace_update(directory, lambda rows: [row for row in rows if not (row.get('event') == 'held_depth_request_rejected' and row.get('request') == 2)])
    elif name == 'late_acceptance_of_refused_id':
        def late(rows):
            next(row for row in rows if row.get('event') == 'depth_copy_armed')['request'] = 2
            return rows
        _trace_update(directory, late)
    elif name == 'color_consumer_pixel':
        path = next(directory.glob('pose-pair-*-801-1.pixels'))
        raw = bytearray(path.read_bytes());raw[0] ^= 1;path.write_bytes(raw)
    elif name == 'invented_common_time':
        _json_update(_depth_meta(directory), lambda value: value.update(common_scene_time_known=True))
    elif name == 'producer_fence_count':
        path = next(path for path in directory.glob('depth-copy-status-*.json') if json.loads(path.read_text())['final'])
        _json_update(path, lambda value: value.update(private_fences=3))
    elif name == 'missing_validation_layer':
        path = directory/'run.log'
        path.write_text(path.read_text().replace('Insert instance layer "VK_LAYER_KHRONOS_validation"', 'removed layer-load receipt'))
    elif name == 'validation_error':
        path = directory/'run.log'
        path.write_text(path.read_text()+'\nSYNC-HAZARD-ALTERED-EVIDENCE\n')
    elif name == 'original_copy_scope':
        path = directory/'run.log'
        lines = path.read_text().splitlines()
        for index, line in enumerate(lines):
            if line.startswith('{"presentation_layer_fixture_passed"'):
                value = json.loads(line);value['original_copy_stage_mask'] = 1 << 12
                lines[index] = json.dumps(value, separators=(',', ':'))
        path.write_text('\n'.join(lines)+'\n')
    else:
        raise ValueError('Unknown alteration '+name)


def replay(source: Path, output: Path):
    source = source.resolve()
    proof = json.loads((source/'proof.json').read_text())
    required = {'pose-hold-control-native', 'held-depth-native',
                'pose-hold-control-runtime', 'held-depth-runtime'}
    if {case['case'] for case in proof['cases']} != required or proof.get('validation_errors') != 0:
        raise ValueError('A completed healthy native/Steam Runtime four-case proof is required')
    current_validator = Path(held_depth_validate.__file__).resolve()
    if probe.digest(current_validator) != proof['source_sha256']['tools/held_depth_validate.py']:
        raise ValueError('Replay must use the exact validator source recorded by the capture')
    if output.exists():
        raise ValueError('Use a new private replay output directory')
    output.mkdir(mode=0o700, parents=True)
    digest = probe.digest
    runs = []

    def once(alteration=None):
        with tempfile.TemporaryDirectory(prefix='held-depth-cpu-', dir=output) as temporary:
            root = Path(temporary)
            a = SimpleNamespace(output=root/'checked', unit=source/'recorded-unit',
                                forwarding=source/'recorded-forwarding', compiler=source/'recorded-compiler',
                                validator=source/'recorded-spirv-validator', fixture=source/'recorded-fixture',
                                layer=source/'recorded-layer', validation_layer=source/'recorded-validation-layer',
                                runtime=source/'recorded-runtime')
            calls = []
            altered = False

            def recorded(command, **_):
                nonlocal altered
                calls.append(command)
                if str(a.unit) in command:
                    log = (source/'unit.log').read_text()
                elif str(a.forwarding) in command:
                    log = (source/'forwarding.log').read_text()
                elif str(a.compiler) in command:
                    target = Path(command[command.index('-o')+1])
                    shutil.copy2(source/target.name, target)
                    log = ''
                elif str(a.validator) in command:
                    log = ''
                elif str(a.fixture) in command:
                    destination = Path(command[-1])
                    if destination.name not in required:
                        raise ValueError('Unrecorded fixture case denied')
                    shutil.copytree(source/destination.name, destination, dirs_exist_ok=True)
                    control_target = alteration in ('clear_only_depth', 'missing_validation_layer', 'validation_error', 'original_copy_scope')
                    target_case = 'pose-hold-control-native' if control_target else 'held-depth-native'
                    if alteration and destination.name == target_case:
                        _mutate(alteration, destination)
                        altered = True
                    log = (destination/'run.log').read_text()
                else:
                    raise ValueError('Unrecorded subprocess denied: '+repr(command))
                return subprocess.CompletedProcess(command, 0, stdout=log, stderr='')

            recorded_digests = {a.unit: proof['unit_sha256'], a.forwarding: proof['forwarding_sha256'],
                                a.fixture: proof['fixture_sha256'],
                                a.layer/vulkan_trace.LIBRARY: proof['layer_sha256']}

            def recorded_digest(path):
                path = Path(path)
                return recorded_digests[path] if path in recorded_digests else digest(path)

            def deny_process(*_, **__):
                raise AssertionError('Actual process execution is forbidden during artifact replay')

            captured = io.StringIO()
            try:
                with patch.object(held_depth_validate.subprocess, 'run', recorded), \
                     patch.object(subprocess, 'Popen', deny_process), \
                     patch.object(held_depth_validate.probe, 'digest', recorded_digest), \
                     redirect_stdout(captured):
                    result = held_depth_validate.validate(a)
            except (ValueError, KeyError) as error:
                if not alteration or not altered:
                    raise ValueError('Healthy artifact replay failed before the intended alteration: '+str(error)) from error
                return {'alteration': alteration, 'rejected': True, 'reason': str(error),
                        'recorded_commands_replayed': len(calls), 'actual_subprocesses': 0}
            if alteration:
                raise ValueError('Altered evidence was accepted: '+alteration)
            if len(result['cases']) != 4:
                raise ValueError('Healthy replay omitted a captured case')
            return {'alteration': None, 'healthy_replay_passed': True, 'cases': 4,
                    'recorded_commands_replayed': len(calls), 'actual_subprocesses': 0}

    runs.append(once())
    alterations = ('private_depth_pixel', 'clear_only_depth', 'expected_packet', 'submit_packet',
                   'recording_eye', 'capture_serial_zero', 'depth_before_color',
                   'refused_request_export', 'missing_active_refusal', 'late_acceptance_of_refused_id',
                   'color_consumer_pixel', 'invented_common_time', 'producer_fence_count',
                   'missing_validation_layer', 'validation_error', 'original_copy_scope')
    for alteration in alterations:
        runs.append(once(alteration))
        print(alteration+': rejected', flush=True)
    result = {'schema_version': 1, 'mode': 'cpu_recorded_artifact_replay',
              'source_proof_sha256': digest(source/'proof.json'),
              'validator_sha256': digest(current_validator), 'replay_tool_sha256': digest(Path(__file__)),
              'healthy_four_case_replay_passed': True, 'alterations_rejected': len(alterations),
              'actual_subprocesses': 0, 'gpu_operations': 0, 'source_artifacts_modified': False,
              'cases': runs}
    probe.write_json(output/'proof.json', result)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    try:
        print(json.dumps(replay(**vars(parser.parse_args())), indent=2))
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(str(error))
