#!/usr/bin/env python3
"""Launch a process-local Vulkan trace without changing the game's files."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

import probe
import shader_capture

LAYER = 'VK_LAYER_D4VR_trace'
LIBRARY = 'libd4vr_vulkan_trace.so'
MANIFEST = 'VkLayer_D4VR_trace.json'


def read_trace(path: Path) -> dict:
    if path.is_symlink() or path.stat().st_size > 8 * 1024 * 1024:
        raise ValueError('Invalid or oversized trace file')
    rows = [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
    if len(rows) > 512 or any(row.get('schema_version') != 1 for row in rows):
        raise ValueError('Unsupported trace schema or record budget exceeded')
    devices = {}
    instances = []
    eye_errors = []
    pair_errors = []
    for row in rows:
        event = row.get('event')
        if event == 'instance':
            instances.append(row)
        elif event == 'device':
            if row['device'] in devices:
                raise ValueError('Duplicate device identifier')
            devices[row['device']] = dict(row, last_total_commands=None, last_present_call=0, clean_destroy=False)
        elif event == 'eye_control_error':
            eye_errors.append(row)
        elif event == 'paired_pipeline_error':
            pair_errors.append(row)
        elif event in ('submit', 'submit_error', 'present', 'device_destroy', 'xr_binding', 'eye_control', 'eye_control_pipeline', 'paired_pipelines'):
            device = devices.get(row['device'])
            if device is None:
                raise ValueError('Command event precedes device creation')
            if event == 'present':
                device['last_total_commands'] = row['total_commands']
                device['last_present_call'] = row['present_call']
            elif event == 'xr_binding':
                device['xr_binding_probe'] = row['binding']
            elif event in ('eye_control','eye_control_pipeline'):
                device['eye_control'] = row['control']
            elif event == 'paired_pipelines':
                device['paired_pipelines'] = row['diagnostic']
            elif event == 'device_destroy':
                device.update(last_total_commands=row['commands'], clean_destroy=True,
                              last_present_call=row['present_calls'], submissions=row['submissions'])
                if 'eye_control' in row:
                    device['eye_control'] = row['eye_control']
                if 'paired_pipelines' in row:
                    device['paired_pipelines'] = row['paired_pipelines']
    return {'file': path.name, 'records': len(rows), 'instances': instances,
            'devices': list(devices.values()), 'record_budget_reached': len(rows) == 512,
            'game_stereo_submitted': False,'eye_control_errors':eye_errors,'paired_pipeline_errors':pair_errors}


def report(directory: Path) -> dict:
    files = sorted(directory.glob('vulkan-*.jsonl'))
    if len(files) > 128:
        raise ValueError('Too many trace processes')
    return {'schema_version': 1, 'process_traces': [read_trace(p) for p in files],
            'trace_loaded': bool(files), 'game_stereo_submitted': False,
            'counts_are_api_commands_not_indirect_gpu_draws': True}


def compare_requirements(trace: dict, requirements: dict) -> dict:
    if requirements.get('binding') != 'XR_KHR_vulkan_enable':
        raise ValueError('Expected legacy Vulkan binding requirements')

    def major_minor(value):
        if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', value):
            raise ValueError('Invalid API version')
        return tuple(int(part) for part in value.split('.')[:2])

    low, high = (major_minor(requirements[name]) for name in ('minimum_api', 'maximum_tested_api'))
    required_instance = set(requirements['required_instance_extensions'])
    required_device = set(requirements['required_device_extensions'])
    processes = []
    for process in trace['process_traces']:
        if not process['devices']:
            continue
        # Do not merge names from separate instances. The initial trace format
        # does not associate each device with one of these instance records.
        instances = [{'requested_api': instance['requested_api'],
                      'within_reported_api_range': low <= major_minor(instance['requested_api']) <= high,
                      'missing_literal_extension_names': sorted(required_instance - set(instance['extensions']))}
                     for instance in process['instances']]
        devices = [{'device': device['device'], 'gpu': device['gpu'],
                    'missing_literal_extension_names': sorted(required_device - set(device['extensions']))}
                   for device in process['devices']]
        processes.append({'file': process['file'], 'instances': instances, 'devices': devices})
    return {'schema_version': 1, 'binding': requirements['binding'],
            'minimum_api': requirements['minimum_api'], 'maximum_tested_api': requirements['maximum_tested_api'],
            'processes': processes, 'instance_device_association_known': False,
            'game_device_binding_proven': False,
            'interpretation': 'Literal extension-name comparison only; newer API versions need separate evaluation. GPU identity, device creation requirements and queue synchronization remain untested.'}


def stage(layer_directory: Path, proof: Path, output: Path) -> dict:
    """Freeze a layer only after its real-GPU/load tests have been recorded."""
    evidence = json.loads(proof.read_text(encoding='utf-8'))
    binary = layer_directory / LIBRARY
    required = ('observer_pixels_unchanged', 'geometry_parallax_unchanged',
                'loaded_trace_verified', 'steam_runtime_load_passed', 'forwarding_tests_passed')
    if any(evidence.get(key) is not True for key in required):
        raise ValueError('Complete GPU, runtime and forwarding evidence is required')
    if evidence.get('layer_sha256') != probe.digest(binary):
        raise ValueError('Layer differs from tested build')
    manifest = json.loads((layer_directory / MANIFEST).read_text(encoding='utf-8'))
    if manifest['layer']['name'] != LAYER or manifest['layer']['library_path'] != './' + LIBRARY:
        raise ValueError('Unexpected layer manifest')
    if output.exists():
        raise ValueError('The staging directory must be new')
    output.mkdir(parents=True, mode=0o700)
    for name in (LIBRARY, MANIFEST):
        shutil.copyfile(layer_directory / name, output / name)
    result = {'schema_version': 1, 'purpose': 'vulkan_trace_diagnostic',
              'layer_sha256': probe.digest(output / LIBRARY), 'manifest_sha256': probe.digest(output / MANIFEST),
              'validation': evidence}
    probe.write_json(output / 'trace-bundle.json', result)
    return result


def run(game: Path, launcher: Path, bundle: Path, runtime_bundle: Path, output: Path,
        xr_binding: bool = False, xr_loader: Path | None = None, xr_runtime: Path | None = None,
        frame_trace: bool = False) -> int:
    if sys.platform != 'linux':
        raise ValueError('This Vulkan diagnostic launcher currently requires Linux')
    staged = json.loads((bundle / 'trace-bundle.json').read_text(encoding='utf-8'))
    if staged.get('purpose') != 'vulkan_trace_diagnostic':
        raise ValueError('Unexpected trace bundle')
    if frame_trace and staged['validation'].get('frame_trace_fixture_passed') is not True:
        raise ValueError('Validate dynamic-rendering metadata and pixel preservation before this test')
    if xr_binding:
        if staged['validation'].get('borrowed_binding_fixture_passed') is not True:
            raise ValueError('Validate the borrowed-device fixture before this test')
        if not xr_loader or not xr_loader.is_file() or not xr_runtime or not xr_runtime.is_file():
            raise ValueError('Provide the tested native XR loader and SteamVR manifest')
        if probe.digest(xr_loader) != staged['validation'].get('xr_loader_sha256'):
            raise ValueError('XR loader differs from the fixture validation')
    for name, field in ((LIBRARY, 'layer_sha256'), (MANIFEST, 'manifest_sha256')):
        if probe.digest(bundle / name) != staged[field]:
            raise ValueError('Staged trace changed after validation')
    # Reuse only the reviewed runtime pin. No shader override from that bundle
    # is enabled: this trace observes the original game geometry.
    runtime = json.loads((runtime_bundle / 'bundle.json').read_text(encoding='utf-8'))
    if probe.digest(Path(runtime['runtime_module'])) != runtime['runtime_module_sha256']:
        raise ValueError('VKD3D runtime changed')
    game, launcher, bundle, output = game.resolve(), launcher.resolve(), bundle.resolve(), output.resolve()
    expected = probe.profile()
    if runtime['game_version'] != expected['game_version']:
        raise ValueError('Runtime bundle belongs to another game version')
    audit = probe.check_target(game, expected)
    if any((game / name).exists() for name in (probe.BACKUP, probe.STATE, probe.CONFIG)):
        raise ValueError('Restore the Streamline probe before tracing')
    if shader_capture.launcher_processes():
        raise ValueError('Close Diablo IV and Battle.net before this isolated launch')
    if output == game or game in output.parents or output.exists():
        raise ValueError('The output directory must be new and outside the game')
    source = launcher.read_text(encoding='utf-8')
    pattern = r'^export PROTONPATH="GE-Proton"$'
    if len(re.findall(pattern, source, re.M)) != 1:
        raise ValueError('Expected the reviewed Lutris launcher')
    source = re.sub(pattern, lambda _: 'export PROTONPATH=' + shlex.quote(runtime['proton_path']), source, flags=re.M)
    output.mkdir(parents=True, mode=0o700)
    private_launcher = output / 'battle-net-trace.sh'
    private_launcher.write_text(source, encoding='utf-8'); private_launcher.chmod(0o700)
    variables = {'VK_ADD_LAYER_PATH': str(bundle), 'VK_INSTANCE_LAYERS': LAYER,
                 'D4VR_TRACE_DIR': str(output), 'VKD3D_DEBUG': 'info',
                 'VKD3D_LOG_FILE': probe.windows_path(output / 'vkd3d.log')}
    if xr_binding:
        variables.update(D4VR_TRACE_XR_BINDING='1', D4VR_XR_LOADER=str(xr_loader.resolve()), XR_RUNTIME_JSON=str(xr_runtime.resolve()))
    if frame_trace:
        variables['D4VR_FRAME_TRACE'] = '1'
    capture = {'schema_version': 1, 'created_utc': datetime.now(timezone.utc).isoformat(),
               'phase': 'launching', 'variables': variables, 'game_files_changed': False,
               'game_audit_before': audit, 'trace_layer_sha256': staged['layer_sha256'],
               'shader_overrides_enabled': False, 'game_stereo_submitted': False}
    capture['temporary_xr_binding_probe_requested'] = xr_binding
    capture['frame_trace_requested'] = frame_trace
    probe.write_json(output / 'capture.json', capture)
    bridge = Path('/run/host/usr/bin/flatpak-spawn')
    command = ([str(bridge), '--host', '/usr/bin/env'] if bridge.exists() else ['/usr/bin/env'])
    for name in ('VKD3D_SHADER_OVERRIDE', 'VKD3D_SHADER_DUMP_PATH', 'VKD3D_SHADER_DEBUG_RING_SIZE_LOG2',
                 'D4VR_TRACE_XR_BINDING', 'D4VR_XR_LOADER', 'D4VR_FRAME_TRACE', 'D4VR_EYE_STATE_FILE', 'D4VR_PAIR_LEFT_FILE', 'D4VR_PAIR_RIGHT_FILE'):
        command += ['-u', name]
    command += [key + '=' + value for key, value in variables.items()] + ['/usr/bin/bash', str(private_launcher)]
    print('Battle.net : diagnostic Vulkan isolé ; entrer en partie sur le moniteur, puis fermer le jeu et Battle.net.', flush=True)
    with (output / 'launcher-private.log').open('wb') as stream:
        os.chmod(stream.name, 0o600)
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    summary = report(output)
    probe.write_json(output / 'trace-report.json', summary)
    capture.update(phase='launcher_exited', launcher_exit_code=result.returncode,
                   game_audit_after=probe.check_target(game, expected), trace_loaded=summary['trace_loaded'],
                   remaining_processes=shader_capture.launcher_processes())
    probe.write_json(output / 'capture.json', capture)
    print(json.dumps({'launcher_exit_code': result.returncode, 'trace_loaded': summary['trace_loaded'],
                      'trace_processes': len(summary['process_traces']), 'game_stereo_submitted': False}), flush=True)
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    prepare = sub.add_parser('stage')
    for name in ('layer-dir', 'proof', 'output'):
        prepare.add_argument('--' + name, required=True, type=Path)
    launch = sub.add_parser('run')
    for name in ('game-dir', 'launcher', 'bundle', 'runtime-bundle', 'output'):
        launch.add_argument('--' + name, required=True, type=Path)
    launch.add_argument('--xr-binding', action='store_true')
    launch.add_argument('--xr-loader', type=Path)
    launch.add_argument('--xr-runtime', type=Path)
    launch.add_argument('--frame-trace', action='store_true')
    summarize = sub.add_parser('report')
    summarize.add_argument('directory', type=Path)
    summarize.add_argument('--output', required=True, type=Path)
    compare = sub.add_parser('compare-requirements')
    for name in ('trace-directory', 'requirements', 'output'):
        compare.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    if args.command == 'stage':
        print(json.dumps(stage(args.layer_dir, args.proof, args.output), indent=2)); return 0
    if args.command == 'run':
        return run(args.game_dir, args.launcher, args.bundle, args.runtime_bundle, args.output,
                   args.xr_binding, args.xr_loader, args.xr_runtime, args.frame_trace)
    if args.command == 'compare-requirements':
        requirements = json.loads(args.requirements.read_text(encoding='utf-8'))
        result = compare_requirements(report(args.trace_directory), requirements)
        probe.write_json(args.output, result); print(json.dumps(result, indent=2)); return 0
    result = report(args.directory); probe.write_json(args.output, result)
    print(json.dumps(result, indent=2)); return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print(f'D4VR Vulkan : {error}', file=sys.stderr)
        raise SystemExit(2)
