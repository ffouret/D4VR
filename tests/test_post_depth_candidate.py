"""Synthetic certificate/orchestration controls; no compiler, driver or process.

The successful mocked construction verifies snapshots and permission scope only.
It is not SPIR-V validation, a GPU certificate or a game-shader construction.
"""
from copy import deepcopy
from hashlib import sha256
import json
import math
from pathlib import Path
import stat
import struct
import sys
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import post_depth_candidate as candidate
from spirv_probe import Module


HELPER_SOURCES = ('native/probe_gpu/stereo_transform.vert', 'tools/stereo_helper.py',
                  'tools/stereo_helper_validate.py', 'tools/spirv_probe.py')


class PostDepthCandidate(unittest.TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory(prefix='post-depth-certificate-unit-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.root = self.directory/'repo'
        for name in set(candidate.CERTIFICATE_SOURCES) | set(HELPER_SOURCES) | {'tools/post_depth_candidate.py'}:
            path = self.root/name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(('synthetic source '+name).encode())
        self.helper = self.directory/'helper.spv'
        self.helper_bytes = b'synthetic helper snapshot, never parsed or executed'
        self.helper.write_bytes(self.helper_bytes)
        self.digest = sha256(self.helper_bytes).hexdigest()
        self.cert = self.certificate()
        self.helper_cert = {
            'source_sha256': {name:candidate.probe.digest(self.root/name) for name in HELPER_SOURCES},
            'cases': [{'case':'source-clip-units', 'spirv_val_passed':True,
                       'helper_contract': {'helper_sha256':self.digest,
                                           'telemetry_instructions_absent':True}}],
            'validated_unknown_opcode_rejected':True,
            'diagnostic_helper_opt_in_rejected':True,
        }

    def certificate(self):
        cases = []
        for environment in ('native', 'steam-runtime'):
            for suffix, control in (('bias','reference'), ('bias','naive'), ('bias','late'),
                                    ('clamp','reference'), ('clamp','late')):
                proof = dict(post_depth_gpu_passed=True, suffix=suffix, control=control,
                             synthetic_scene=True, game_rendered=False, game_admission=False,
                             game_bundle_authorized=False,
                             independent_reference_cases=36, d32_compared_texels=20000,
                             nonempty_reference_cases=20, reference_failures=0,
                             coverage_failure_cases=0, color_failure_cases=0,
                             depth_failure_cases=0, maximum_depth_error=1e-7,
                             exact_inert_rgba_d32_cases=5, legacy_optical_reference_cases=1,
                             maximum_legacy_depth_error=1e-7,
                             minimum_final_reference_w=1.2, maximum_final_reference_w=230.,
                             clamp_crossing_cases=6 if suffix=='clamp' else 0,
                             clamped_reference_texels=1000 if suffix=='clamp' else 0,
                             depth_only_bias_witnesses=4, naive_prediction_cases=36,
                             maximum_naive_prediction_error=1e-7)
                if control != 'reference':
                    proof.update(reference_failures=4, color_failure_cases=2,
                                 depth_failure_cases=2, maximum_depth_error=.1)
                cases.append(dict(environment=environment, suffix=suffix, control=control,
                                  validation_errors=0, khronos_instance_and_device_loaded=True,
                                  proof=proof))
        return dict(gpu_validation_performed=True, synthetic_scene=True, game_rendered=False,
                    game_admission=False, game_bundle_authorized=False,
                    helper_sha256=self.digest, naive_helper_sha256='a'*64,
                    source_sha256={name:candidate.probe.digest(self.root/name)
                                   for name in candidate.CERTIFICATE_SOURCES}, gpu_cases=cases)

    def verify(self, certificate=None):
        return candidate.verify_certificate(self.cert if certificate is None else certificate,
                                            self.helper, self.root)

    def one(self, certificate, suffix='bias', control='reference', environment='native'):
        return next(c for c in certificate['gpu_cases'] if
                    (c['environment'],c['suffix'],c['control'])==(environment,suffix,control))

    def rejected(self, change):
        certificate = deepcopy(self.cert)
        change(certificate)
        with self.assertRaises(ValueError):
            self.verify(certificate)

    def test_complete_synthetic_certificate_preserves_no_game_permission(self):
        result = self.verify()
        self.assertEqual(result['actual_gpu_cases'], 10)
        self.assertFalse(result['game_admission'])
        self.assertFalse(result['game_bundle_authorized'])

    def test_missing_environment_controls_and_duplicate_cases_are_refused(self):
        for environment in ('native','steam-runtime'):
            self.rejected(lambda c: c.update(gpu_cases=[r for r in c['gpu_cases'] if r['environment']!=environment]))
        for control in ('naive','late','reference'):
            self.rejected(lambda c: c['gpu_cases'].remove(self.one(c, control=control)))
        self.rejected(lambda c: c['gpu_cases'].append(deepcopy(c['gpu_cases'][0])))

    def test_static_only_game_permission_wrong_helper_and_source_hashes_are_refused(self):
        for key, value in (('gpu_validation_performed',False), ('game_admission',True),
                           ('helper_sha256','b'*64), ('naive_helper_sha256',self.digest)):
            self.rejected(lambda c: c.update({key:value}))
        self.rejected(lambda c: c['source_sha256'].pop(candidate.CERTIFICATE_SOURCES[0]))
        (self.root/candidate.CERTIFICATE_SOURCES[0]).write_bytes(b'changed source')
        with self.assertRaisesRegex(ValueError,'source mismatch'):
            self.verify()

    def test_inner_suffix_control_and_owned_scope_cannot_be_retagged(self):
        for key, value in (('suffix','clamp'), ('control','naive'),
                           ('synthetic_scene',False), ('game_rendered',True)):
            self.rejected(lambda c: self.one(c)['proof'].update({key:value}))

    def test_validation_loading_and_nonempty_depth_evidence_are_required(self):
        for key, value in (('validation_errors',1), ('khronos_instance_and_device_loaded',False)):
            self.rejected(lambda c: self.one(c).update({key:value}))
        for key, value in (('post_depth_gpu_passed',False), ('independent_reference_cases',35),
                           ('d32_compared_texels',10000), ('nonempty_reference_cases',17)):
            self.rejected(lambda c: self.one(c)['proof'].update({key:value}))

    def test_reference_inert_legacy_and_exact_rendered_clamp_witnesses_required(self):
        for key, value in (('reference_failures',1), ('coverage_failure_cases',1),
                           ('color_failure_cases',1), ('depth_failure_cases',1),
                           ('maximum_depth_error',2e-6), ('exact_inert_rgba_d32_cases',4),
                           ('legacy_optical_reference_cases',0)):
            self.rejected(lambda c: self.one(c)['proof'].update({key:value}))
        for key, value in (('clamp_crossing_cases',5), ('clamped_reference_texels',600)):
            self.rejected(lambda c: self.one(c,'clamp')['proof'].update({key:value}))

    def test_naive_inverse_square_prediction_and_late_mismatch_are_required(self):
        for key, value in (('depth_only_bias_witnesses',3), ('naive_prediction_cases',35),
                           ('maximum_naive_prediction_error',2e-6)):
            self.rejected(lambda c: self.one(c,control='naive')['proof'].update({key:value}))
        self.rejected(lambda c: self.one(c,control='late')['proof'].update(reference_failures=0))
        self.rejected(lambda c: self.one(c,control='late')['proof'].update(color_failure_cases=0,coverage_failure_cases=0))

    def test_nonfinite_or_negative_error_metrics_are_refused(self):
        for control, key in (('reference','maximum_depth_error'), ('naive','maximum_naive_prediction_error')):
            for value in (math.nan, math.inf, -math.inf, -1e-7, False, None):
                with self.subTest(control=control, value=value):
                    self.rejected(lambda c: self.one(c,control=control)['proof'].update({key:value}))

    def test_observation_counters_must_be_integers(self):
        for suffix, control, key, value in (
                ('bias','reference','nonempty_reference_cases',18.),
                ('bias','reference','d32_compared_texels',20000.),
                ('clamp','reference','clamped_reference_texels',1000.),
                ('bias','naive','depth_only_bias_witnesses',4.)):
            with self.subTest(key=key):
                self.rejected(lambda c: self.one(c,suffix,control)['proof'].update({key:value}))

    def test_standalone_helper_certificate_cannot_be_missing_stale_or_diagnostic(self):
        candidate.verify_helper_certificate(self.helper_cert,self.digest,self.root)
        mutations = [lambda c: c.update(cases=[]),
                     lambda c: c['cases'].append(deepcopy(c['cases'][0])),
                     lambda c: c['cases'][0].update(spirv_val_passed=False),
                     lambda c: c['cases'][0]['helper_contract'].update(helper_sha256='0'*64),
                     lambda c: c['cases'][0]['helper_contract'].update(telemetry_instructions_absent=False),
                     lambda c: c.update(validated_unknown_opcode_rejected=False),
                     lambda c: c.update(diagnostic_helper_opt_in_rejected=False),
                     lambda c: c['source_sha256'].update({HELPER_SOURCES[0]:'0'*64})]
        for mutation in mutations:
            certificate = deepcopy(self.helper_cert)
            mutation(certificate)
            with self.assertRaises(ValueError):
                candidate.verify_helper_certificate(certificate,self.digest,self.root)

    def construction_args(self):
        # Instruction framing only; validation/construction is deliberately
        # mocked below. This module never reaches a compiler or driver.
        module = Module(struct.pack('<5I',0x07230203,0x10300,0,100,0))
        module.instructions = [(129,[1,20,2,3]), (133,[1,21,2,3]),
                               (136,[1,22,20,21]), (148,[1,23,2,3]),
                               (12,[1,24,5,50,2,3,20]), (62,[10,24])]
        self.source = module.binary()
        original = self.directory/'original.spv'
        original.write_bytes(self.source)
        certificate = self.directory/'gpu.json'
        certificate.write_text(json.dumps(self.cert))
        helper_certificate = self.directory/'helper.json'
        helper_certificate.write_text(json.dumps(self.helper_cert))
        grammar, glsl = self.directory/'grammar.json', self.directory/'glsl.json'
        grammar.write_bytes(b'fake core grammar'); glsl.write_bytes(b'fake glsl grammar')
        return SimpleNamespace(output=self.directory/'new-candidate', original=original,
                               helper=self.helper, certificate=certificate,
                               helper_certificate=helper_certificate, grammar=grammar,
                               glsl_grammar=glsl, tools=self.directory/'tools')

    def run_mocked_construction(self, args, *, mutate_inputs=False, drop_opcode=None,
                                validated_effects=True, final_output_count=1):
        def original_contract(data,*_):
            self.assertEqual(data,self.source)
            if mutate_inputs:
                args.original.write_bytes(b'changed original path')
                args.helper.write_bytes(b'changed helper path')
                args.certificate.write_bytes(b'changed certificate path')
                args.helper_certificate.write_bytes(b'changed helper certificate path')
            return dict(static_suffix_contract_matched=True, projection_rewrite_allowed=False,
                        shader_sha256=sha256(data).hexdigest())

        def instrumented(data,*_,**__):
            self.assertEqual(data,self.source)
            if drop_opcode is None:
                return data
            module=Module(data)
            module.instructions=[(op,a) for op,a in module.instructions if op!=drop_opcode]
            return module.binary()

        def build(source_snapshot, helper_snapshot, transformed,*_,**__):
            self.assertEqual(source_snapshot.read_bytes(),self.source)
            self.assertEqual(helper_snapshot.read_bytes(),self.helper_bytes)
            transformed.write_bytes(b'synthetic final bytes, never validated or executed')

        policy = SimpleNamespace(glsl=object(), validate=lambda *_:
            dict(core_writes_by_storage={'Output':final_output_count},
                 validated_non_output_external_writes_excluded=validated_effects))
        def no_process(*_,**__):
            raise AssertionError('Actual subprocess execution forbidden in unit controls')
        with patch.object(candidate,'__file__',str(self.root/'tools/post_depth_candidate.py')), \
             patch.object(candidate,'validated',side_effect=original_contract), \
             patch.object(candidate,'verify_projection_only',side_effect=lambda data:
                 dict(helper_sha256=sha256(data).hexdigest(), replay_allowed=False)), \
             patch.object(candidate,'instrument',side_effect=instrumented), \
             patch.object(candidate,'build',side_effect=build) as builder, \
             patch.object(candidate,'Policy',return_value=policy), \
             patch.object(candidate,'assess',return_value={'core_writes_by_storage':{'Output':1}}), \
             patch.object(candidate.subprocess,'run',side_effect=no_process), \
             patch.object(candidate.subprocess,'Popen',side_effect=no_process):
            result=candidate.construct(args)
            self.assertEqual(builder.call_count,1)
            return result

    def test_construction_uses_captured_snapshots_and_never_grants_game_permission(self):
        args = self.construction_args()
        result = self.run_mocked_construction(args,mutate_inputs=True)
        self.assertEqual(result['original_sha256'],sha256(self.source).hexdigest())
        self.assertEqual(result['helper_sha256'],self.digest)
        self.assertEqual(result['helper_contract']['helper_sha256'],self.digest)
        self.assertEqual(result['certificate_sha256'],sha256(json.dumps(self.cert).encode()).hexdigest())
        self.assertEqual(result['helper_certificate_sha256'],sha256(json.dumps(self.helper_cert).encode()).hexdigest())
        for key in ('game_bundle_authorized','game_executed','xr_submission','coverage_repaired'):
            self.assertIs(result[key],False)
        self.assertEqual(stat.S_IMODE(args.output.stat().st_mode),0o700)
        self.assertEqual(stat.S_IMODE((args.output/'candidate.json').stat().st_mode),0o600)
        self.assertTrue((args.output/'original.snapshot.spv').read_bytes()==self.source)
        self.assertTrue((args.output/'helper.snapshot.spv').read_bytes()==self.helper_bytes)

    def test_construction_refuses_dropped_source_store_or_unreviewed_final_effects(self):
        for kind in ('drop_store','drop_division','drop_extended','effect','count'):
            with self.subTest(kind=kind):
                args = self.construction_args()
                args.output=self.directory/('candidate-'+kind)
                with self.assertRaises(ValueError):
                    self.run_mocked_construction(args,drop_opcode={'drop_store':62,'drop_division':136,'drop_extended':12}.get(kind),
                        validated_effects=kind!='effect',final_output_count=2 if kind=='count' else 1)
                self.assertFalse((args.output/'candidate.json').exists())

    def test_existing_output_is_rejected_before_inputs_are_read(self):
        args = self.construction_args()
        args.output.mkdir()
        args.original.unlink()
        with self.assertRaisesRegex(ValueError,'new local candidate'):
            candidate.construct(args)


if __name__=='__main__':
    unittest.main()
