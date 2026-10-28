#!/usr/bin/env python3
"""Construct one explicit, local post-depth candidate after owned GPU controls.

This never changes the general camera candidate selector, a game bundle, a
launcher or the game. Its result is not a replay or game-execution permission.
"""
from __future__ import annotations
import argparse
import json
import hashlib
import subprocess
import math
from collections import Counter
from pathlib import Path

import probe
from projection_depth_suffix import validated
from shader_write_contract import Policy, assess
from spirv_probe import Module, instrument, build
from stereo_helper import verify_projection_only

CERTIFICATE_SOURCES = (
    'native/tests/post_depth.vert', 'native/tests/post_depth.frag',
    'native/tests/post_depth_gpu_checks.hpp', 'native/tests/gpu_probe_vulkan.cpp',
    'tools/post_depth_validate.py', 'tools/projection_depth_suffix.py',
    'tools/projection_input.py', 'tools/spirv_probe.py', 'tools/stereo_helper.py',
)


def measured_error(value, limit):
    return (type(value) in (int, float) and math.isfinite(value)
            and 0 <= value < limit)


def counter_at_least(value, minimum):
    return type(value) is int and value >= minimum


def verify_certificate(certificate, helper, root, *, helper_digest=None):
    helper_digest = probe.digest(helper) if helper_digest is None else helper_digest
    if (certificate.get('gpu_validation_performed') is not True
            or certificate.get('game_admission') is not False
            or certificate.get('helper_sha256') != helper_digest
            or certificate.get('helper_sha256') == certificate.get('naive_helper_sha256')):
        raise ValueError('Post-depth candidate requires its distinct corrected-helper GPU certificate')
    hashes = certificate.get('source_sha256', {})
    if any(hashes.get(name) != probe.digest(root/name) for name in CERTIFICATE_SOURCES):
        raise ValueError('Post-depth certificate source mismatch')
    for environment in ('native', 'steam-runtime'):
        cases = [c for c in certificate.get('gpu_cases', []) if c.get('environment') == environment]
        keys = {(c.get('suffix'), c.get('control')) for c in cases}
        if len(cases) != 5 or keys != {('bias', 'reference'), ('bias', 'naive'), ('bias', 'late'),
                                      ('clamp', 'reference'), ('clamp', 'late')}:
            raise ValueError('Post-depth GPU environments or controls are incomplete')
        for case in cases:
            p = case.get('proof', {})
            if (case.get('validation_errors') != 0 or case.get('khronos_instance_and_device_loaded') is not True
                    or p.get('post_depth_gpu_passed') is not True or p.get('game_admission') is not False
                    or p.get('suffix') != case['suffix'] or p.get('control') != case['control']
                    or p.get('independent_reference_cases') != 36
                    or p.get('synthetic_scene') is not True or p.get('game_rendered') is not False
                    or not counter_at_least(p.get('nonempty_reference_cases'), 18)
                    or not counter_at_least(p.get('d32_compared_texels'), 10001)):
                raise ValueError('Post-depth GPU execution evidence is incomplete')
            if case['control'] == 'reference':
                if (any(p.get(k) != 0 for k in ('reference_failures', 'coverage_failure_cases',
                                               'color_failure_cases', 'depth_failure_cases'))
                        or not measured_error(p.get('maximum_depth_error'), 2e-6)
                        or p.get('exact_inert_rgba_d32_cases') != 5
                        or p.get('legacy_optical_reference_cases') != 1):
                    raise ValueError('Corrected post-depth reference or inert controls failed')
                if case['suffix'] == 'clamp' and (p.get('clamp_crossing_cases') != 6
                                                 or not counter_at_least(p.get('clamped_reference_texels'), 601)):
                    raise ValueError('Actually rendered clamp-depth witnesses missing')
            elif case['control'] == 'naive':
                if (not counter_at_least(p.get('depth_only_bias_witnesses'), 4)
                        or p.get('naive_prediction_cases') != 36
                        or not measured_error(p.get('maximum_naive_prediction_error'), 2e-6)):
                    raise ValueError('Naive reciprocal-W depth negative control missing')
            elif not (p.get('reference_failures', 0) > 0 and
                      (p.get('color_failure_cases', 0) > 0 or p.get('coverage_failure_cases', 0) > 0)):
                raise ValueError('Late-position owned-output negative control missing')
    return {'environments': ['native', 'steam-runtime'], 'actual_gpu_cases': 10,
            'game_admission': False, 'game_bundle_authorized': False}


def verify_helper_certificate(certificate, digest, root):
    names = ('native/probe_gpu/stereo_transform.vert', 'tools/stereo_helper.py',
             'tools/stereo_helper_validate.py', 'tools/spirv_probe.py')
    if any(certificate.get('source_sha256', {}).get(name) != probe.digest(root/name) for name in names):
        raise ValueError('Standalone helper source certificate mismatch')
    modes = [case for case in certificate.get('cases', []) if case.get('case') == 'source-clip-units']
    if (len(modes) != 1 or modes[0].get('spirv_val_passed') is not True
            or modes[0].get('helper_contract', {}).get('helper_sha256') != digest
            or modes[0].get('helper_contract', {}).get('telemetry_instructions_absent') is not True
            or certificate.get('validated_unknown_opcode_rejected') is not True
            or certificate.get('diagnostic_helper_opt_in_rejected') is not True):
        raise ValueError('Source-unit helper construction controls missing')


def construct(a):
    root = Path(__file__).resolve().parents[1]
    if a.output.exists():
        raise ValueError('Use a new local candidate directory')
    certificate_bytes = a.certificate.read_bytes()
    certificate = json.loads(certificate_bytes)
    source = a.original.read_bytes()
    helper_bytes = a.helper.read_bytes()
    helper_digest = hashlib.sha256(helper_bytes).hexdigest()
    helper_certificate_bytes = a.helper_certificate.read_bytes()
    verify_helper_certificate(json.loads(helper_certificate_bytes), helper_digest, root)
    checks = verify_certificate(certificate, a.helper, root, helper_digest=helper_digest)
    contract = validated(source, a.grammar.read_bytes(), a.glsl_grammar.read_bytes(), a.tools/'spirv-val')
    helper_contract = verify_projection_only(helper_bytes)
    # Before linking/ID renumbering, original output stores and suffix expression
    # instructions keep their exact operands. Only projection UBO reads move to
    # invocation-local transformed rows. The general selector stays unchanged.
    original = Module(source)
    caller = Module(instrument(source, 3, 0, 72, stereo=True, projection_only_helper=True,
                               projection_input=True))
    for op in (62, 129, 133, 136, 148):
        old = [tuple(x) for o, x in original.instructions if o == op]
        new = [tuple(x) for o, x in caller.instructions if o == op]
        if Counter(old) - Counter(new):
            raise ValueError('Early constructor changed an original store/arithmetic instruction')
    old_extended = [tuple(x) for o, x in original.instructions if o == 12]
    new_extended = [tuple(x) for o, x in caller.instructions if o == 12]
    if Counter(old_extended) - Counter(new_extended):
        raise ValueError('Early constructor changed an original extended instruction')
    a.output.mkdir(mode=0o700, parents=True)
    source_snapshot = a.output/'original.snapshot.spv'
    helper_snapshot = a.output/'helper.snapshot.spv'
    source_snapshot.write_bytes(source)
    helper_snapshot.write_bytes(helper_bytes)
    source_snapshot.chmod(0o600)
    helper_snapshot.chmod(0o600)
    transformed = a.output/'candidate.spv'
    build(source_snapshot, helper_snapshot, transformed, a.tools, 3, 0, 72, stereo=True,
          helper_no_telemetry=True, projection_only_helper=True, projection_input=True)
    transformed.chmod(0o600)
    policy = Policy(a.grammar.read_bytes(), a.glsl_grammar.read_bytes())
    before = assess(source, policy.glsl)
    after = policy.validate(transformed.read_bytes(), a.tools/'spirv-val', 'vulkan1.2')
    if after.get('core_writes_by_storage', {}).get('Output', 0) != before['core_writes_by_storage'].get('Output', 0):
        raise ValueError('Transformed candidate changed the original output-store count')
    if not after.get('validated_non_output_external_writes_excluded'):
        raise ValueError('Transformed candidate has an unreviewed write/effect')
    result = {'schema_version': 1, 'original_sha256': hashlib.sha256(source).hexdigest(),
              'helper_sha256': helper_digest, 'candidate_sha256': probe.digest(transformed),
              'certificate_sha256': hashlib.sha256(certificate_bytes).hexdigest(),
              'helper_certificate_sha256': hashlib.sha256(helper_certificate_bytes).hexdigest(),
              'closed_original_contract': contract,
              'helper_contract': helper_contract, 'owned_gpu_certificate': checks,
              'original_stores_and_suffix_arithmetic_preserved_before_link': True,
              'transformed_write_contract': after, 'projection_application': 'canonical_input_loads',
              'clip_output_units': 'source_VIEW_clip_units_after_metric_conversion',
              'game_bundle_authorized': False, 'game_executed': False, 'xr_submission': False,
              'coverage_repaired': False,
              'constructor_source_sha256': probe.digest(root/'tools/post_depth_candidate.py'),
              'limits': ['Finite owned GPU inputs do not certify all original shader inputs.',
                         'The original reciprocal-W singularity remains.',
                         'This candidate is not installed or selected by the game bundle builder.',
                         'A matching static expression does not establish a pass role or hole causality.']}
    probe.write_json(a.output/'candidate.json', result)
    (a.output/'candidate.json').chmod(0o600)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('original', 'helper', 'certificate', 'helper-certificate', 'grammar', 'glsl-grammar', 'tools', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    try:
        print(json.dumps(construct(parser.parse_args()), indent=2))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
