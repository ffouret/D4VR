#!/usr/bin/env python3
"""Construct and optionally render owned post-projection depth suffix controls.

Without --gpu-fixture, only compilation, SPIR-V construction and validation
run. GPU execution requires an explicit fixture and Khronos layer directory.
These synthetic finite-input shaders provide no game admission certificate.
"""
from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

import paired_capture
import probe
import projection_depth_suffix
from spirv_probe import Module, build, instrument, literal


def output_stores(module: Module) -> list[tuple[int, ...]]:
    roots = {x[1]: x[1] for op, x in module.instructions if op == 59 and x[2] == 3}
    stores = []
    for op, x in module.instructions:
        if op in (65, 66) and x[2] in roots:
            roots[x[1]] = roots[x[2]]
        elif op == 83 and x[2] in roots:
            roots[x[1]] = roots[x[2]]
        elif op == 62 and x[0] in roots:
            stores.append(tuple(x))
    return stores


def normalize_owned_clamp(path: Path) -> dict:
    """Use the exact NMin suffix operator, only in this owned fixture module.

    No NaN-equivalence assertion is made. All fixture/reference inputs are
    finite; this is not a general SPIR-V or GLSL extended-instruction rewrite.
    """
    before = probe.digest(path)
    module = Module(path.read_bytes())
    imports = {x[0] for op, x in module.instructions if op == 11 and literal(x[1:]) == 'GLSL.std.450'}
    matches = [x for op, x in module.instructions if op == 12 and x[2] in imports and x[3] == 37]
    if len(matches) != 1:
        raise ValueError('Owned clamp shader must contain exactly one GLSL FMin37')
    matches[0][3] = 79  # GLSL.std.450 NMin, the original operator under study.
    path.write_bytes(module.binary())
    return {'scope': 'owned_clamp_fixture_only', 'from': 'GLSL.std.450 FMin37',
            'to': 'GLSL.std.450 NMin79', 'instructions_changed': 1,
            'finite_inputs_only': True, 'general_nan_equivalence_claimed': False,
            'compiled_sha256': before, 'normalized_sha256': probe.digest(path)}


def check_gpu_proof(proof: dict, suffix: str, control: str) -> None:
    if (proof.get('post_depth_gpu_passed') is not True or proof.get('suffix') != suffix
            or proof.get('control') != control or proof.get('independent_reference_cases') != 36
            or proof.get('synthetic_scene') is not True or proof.get('game_rendered') is not False
            or proof.get('game_admission') is not False or proof.get('d32_compared_texels', 0) <= 10000
            or proof.get('nonempty_reference_cases', 0) < 18):
        raise ValueError('Incomplete post-depth GPU proof')
    if control == 'reference':
        if any(proof.get(k) != 0 for k in ('reference_failures', 'coverage_failure_cases', 'color_failure_cases', 'depth_failure_cases')):
            raise ValueError('Corrected helper differs from its independent reference')
        if suffix == 'clamp' and proof.get('clamp_crossing_cases') != 6:
            raise ValueError('Clamp far-plane crossing reference missing')
        if suffix == 'clamp' and proof.get('clamped_reference_texels', 0) <= 600:
            raise ValueError('Clamp did not produce covered D32 texels at one')
        if (proof.get('exact_inert_rgba_d32_cases') != 5 or proof.get('legacy_optical_reference_cases') != 1
                or proof.get('maximum_legacy_depth_error', 1) >= 2e-6):
            raise ValueError('Off, malformed, gate mismatch or legacy optical controls missing')
    elif control == 'naive':
        if (suffix != 'bias' or proof.get('depth_only_bias_witnesses', 0) < 4
                or proof.get('naive_prediction_cases') != 36
                or proof.get('maximum_naive_prediction_error', 1) >= 2e-6):
            raise ValueError('Naive helper inverse-scale-squared depth control missing')
    elif proof.get('reference_failures', 0) <= 0 or not (
            proof.get('coverage_failure_cases', 0) > 0 or proof.get('color_failure_cases', 0) > 0):
        raise ValueError('Late Position original-output control missing')


def validate(a: argparse.Namespace) -> dict:
    if a.output.exists():
        raise ValueError('Use a new output directory')
    if a.gpu_fixture and not a.validation_layer:
        raise ValueError('GPU controls require --validation-layer')
    if not a.gpu_fixture and (a.validation_layer or a.runtime):
        raise ValueError('--validation-layer and --runtime require --gpu-fixture')
    if probe.digest(a.helper) == probe.digest(a.naive_helper):
        raise ValueError('Corrected and naive helper binaries must differ')
    a.output.mkdir(parents=True, mode=0o700)
    root = Path(__file__).resolve().parents[1]
    commands = []

    def run(command: list[str]) -> subprocess.CompletedProcess:
        commands.append(command)
        return subprocess.run(command, check=True, capture_output=True, text=True, timeout=60)

    def spirv_validate(path: Path) -> None:
        run([str(a.tools/'spirv-val'), '--target-env', 'vulkan1.2', str(path)])

    def compile_shader(source: Path, target: Path, *defines: str) -> None:
        run([str(a.compiler), '-V', '--target-env', 'vulkan1.1', '-Os', *defines, str(source), '-o', str(target)])
        spirv_validate(target)

    fragment = a.output/'post-depth.frag.spv'
    compile_shader(root/'native/tests/post_depth.frag', fragment)
    modules = {}
    suffix_contracts = {}
    constructions = []
    clamp_normalization = None
    for suffix in ('bias', 'clamp'):
        original = a.output/(suffix+'.original.spv')
        compile_shader(root/'native/tests/post_depth.vert', original, '-DD4VR_POST_DEPTH_BIAS='+str(int(suffix == 'bias')))
        if suffix == 'clamp':
            clamp_normalization = normalize_owned_clamp(original)
            spirv_validate(original)
        source = Module(original.read_bytes())
        suffix_contracts[suffix] = projection_depth_suffix.validated(
            original.read_bytes(), a.grammar.read_bytes(), a.glsl_grammar.read_bytes(), a.tools/'spirv-val')
        expected_kind = 'affine_bias_over_w' if suffix == 'bias' else 'min_z_w'
        if suffix_contracts[suffix]['suffix_kind'] != expected_kind:
            raise ValueError('Owned GPU original does not match its closed suffix contract')
        stores = output_stores(source)
        if not stores:
            raise ValueError('Owned shader has no original output stores')
        caller = Module(instrument(original.read_bytes(), 3, 0, 72, stereo=True,
                                   projection_only_helper=True, projection_input=True))
        if output_stores(caller) != stores:
            raise ValueError('Early projection construction changed an original output store')
        variants = {'original': original}
        for control, helper, early in [('reference', a.helper, True), ('late', a.helper, False)]+(
                [('naive', a.naive_helper, True)] if suffix == 'bias' else []):
            target = a.output/(suffix+'.'+control+'.spv')
            build(original, helper, target, a.tools, 3, 0, 72, stereo=True,
                  helper_no_telemetry=True, projection_only_helper=True, projection_input=early)
            variants[control] = target
            constructions.append({'suffix': suffix, 'control': control, 'projection_input': early,
                                  'helper_sha256': probe.digest(helper), 'sha256': probe.digest(target),
                                  'spirv_validated': True})
        modules[suffix] = variants

    gpu_cases = []
    gpu_commands = []
    if a.gpu_fixture:
        for runtime in ((False, True) if a.runtime else (False,)):
            environment = 'steam-runtime' if runtime else 'native'
            prefix = paired_capture.host_command()+['/usr/bin/env',
                'VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation',
                'VK_ADD_LAYER_PATH='+str(a.validation_layer),
                'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                'VK_LAYER_VALIDATE_SYNC=1', 'VK_LOADER_DEBUG=layer']
            if runtime:
                prefix += [str(a.runtime), '--batch', '--']
            for suffix, variants in modules.items():
                for control in ('reference', 'naive', 'late') if suffix == 'bias' else ('reference', 'late'):
                    command = prefix+[str(a.gpu_fixture), str(variants['original']), str(variants[control]),
                                      str(fragment), '--post-depth', suffix, '--post-depth-control', control]
                    gpu_commands.append(command)
                    process = subprocess.run(command, capture_output=True, text=True, timeout=a.gpu_timeout)
                    log = process.stdout+process.stderr
                    log_path = a.output/(suffix+'-'+control+'-'+environment+'.log')
                    log_path.write_text(log)
                    if any(x in log for x in ('VUID-', 'Validation Error', 'SYNC-HAZARD-')):
                        raise ValueError('Inspect post-depth Vulkan validation log: '+str(log_path))
                    if ('Insert instance layer "VK_LAYER_KHRONOS_validation"' not in log
                            or 'Inserted device layer "VK_LAYER_KHRONOS_validation"' not in log):
                        raise ValueError('Khronos instance/device validation load unconfirmed: '+str(log_path))
                    if process.returncode:
                        raise ValueError('Post-depth fixture failed: '+str(log_path)+' '+process.stderr[-500:])
                    lines = [x for x in process.stdout.splitlines() if x.startswith('{"post_depth_gpu_passed"')]
                    if len(lines) != 1:
                        raise ValueError('Post-depth fixture result missing or ambiguous')
                    proof = json.loads(lines[0])
                    check_gpu_proof(proof, suffix, control)
                    gpu_cases.append({'environment': environment, 'suffix': suffix, 'control': control,
                                      'validation_errors': 0, 'khronos_instance_and_device_loaded': True,
                                      'log_sha256': probe.digest(log_path), 'proof': proof})
    sources = ['native/tests/post_depth.vert', 'native/tests/post_depth.frag',
               'native/tests/post_depth_gpu_checks.hpp', 'native/tests/gpu_probe_vulkan.cpp',
               'native/core/pose_request.hpp', 'native/core/eye_state.hpp',
               'tools/post_depth_validate.py', 'tools/projection_input.py', 'tools/spirv_probe.py',
               'tools/stereo_helper.py', 'tools/projection_depth_suffix.py',
               'tools/camera_dataflow.py', 'tools/shader_write_contract.py']
    result = {'schema_version': 1, 'scope': 'owned finite-input post-projection suffix controls',
              'synthetic_scene': True, 'game_rendered': False, 'game_admission': False,
              'game_bundle_authorized': False,
              'actual_steamvr_submission': False, 'packet_abi_bytes': 176,
              'reference': 'uninstrumented original suffix with Ptarget/scale and eyeFromGameView*model',
              'depth_measurement': 'D32_SFLOAT on common alpha-covered pixels; coverage and color measured separately',
              'fragment_discard': False, 'original_output_stores_preserved_by_early_constructor': True,
              'extra_projected_varying_is_owned_control': True,
              'original_game_shaders_have_this_varying_claimed': False,
              'inert_packet_controls': ['disabled', 'invalid_magic', 'invalid_version', 'projection_gate_mismatch', 'invalid_source_scale'],
              'helper_calls_per_early_invocation': 4, 'source_uniform_writes_added': 0,
              'clamp_fixture_normalization': clamp_normalization,
              'helper_sha256': probe.digest(a.helper), 'naive_helper_sha256': probe.digest(a.naive_helper),
              'fixture_sha256': probe.digest(a.gpu_fixture) if a.gpu_fixture else None,
              'module_sha256': {suffix: {role: probe.digest(path) for role, path in variants.items()}
                                for suffix, variants in modules.items()},
              'fragment_sha256': probe.digest(fragment), 'constructions': constructions,
              'suffix_contracts': suffix_contracts,
              'source_sha256': {name: probe.digest(root/name) for name in sources},
              'direct_cpu_commands': commands, 'gpu_commands': gpu_commands,
              'actual_gpu_subprocesses': len(gpu_commands), 'gpu_cases': gpu_cases,
              'gpu_validation_performed': bool(gpu_cases),
              'native_and_runtime_controls_passed': len(gpu_cases) == 10 and {
                  x['environment'] for x in gpu_cases} == {'native', 'steam-runtime'}}
    probe.write_json(a.output/'proof.json', result)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('compiler', 'tools', 'helper', 'naive-helper', 'grammar', 'glsl-grammar', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    for name in ('gpu-fixture', 'validation-layer', 'runtime'):
        parser.add_argument('--'+name, type=Path)
    parser.add_argument('--gpu-timeout', type=int, default=120)
    args = parser.parse_args()
    for key, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, key, value.resolve())
    try:
        print(json.dumps(validate(args), indent=2))
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
