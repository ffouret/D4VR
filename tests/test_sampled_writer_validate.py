import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import sampled_writer_validate as audit


def module(model):
    # Mock instruction stream for CPU argument/stage checks only. Never a GPU shader.
    words = [0x07230203, 0x00010300, 0, 2, 0, (5 << 16) | 15,
             model, 1, int.from_bytes(b'main', 'little'), 0]
    return struct.pack('<' + 'I'*len(words), *words)


def manifest(modules):
    result = {'schema_version': 1, 'width': audit.WIDTH, 'height': audit.HEIGHT,
              'format': 44, 'target_format': 'B8G8R8A8_UNORM', 'modules': modules,
              'owned_fixture': True, 'game_rendered': False, 'shader_modified': False,
              'counts': {'submissions': 10, 'draws': 9, 'image_uploads': 2, 'target_initializations': 9, 'target_readbacks': 9},
              'texture_state': {'width': 8, 'height': 8, 'format': 37, 'mip_levels': 1, 'files': ['texture-A.rgba', 'texture-B.rgba']},
              'render_area': list(audit.FULL), 'dynamic_state': {'viewport': True, 'scissor': True},
              'pipeline_state': deepcopy(audit.PIPELINE),
              'descriptor_state': deepcopy(audit.DESCRIPTORS),
              'sampler_state': deepcopy(audit.SAMPLERS),
              'features': {name: True for name in audit.FEATURES},
              'validation': {'debug_messenger': True, 'synchronization_validation': True,
                             'errors': 0, 'warnings': 0, 'log_file': 'validation.log'}, 'cases': []}
    for name in audit.CASES:
        state, pc, ubo = audit.written_state(name)
        result['cases'].append({'name': name, 'raw_file': name+'.bgra',
                                'pc_file': name+'.pc.bin', 'ubo_file': name+'.ubo.bin',
                                'bytes': audit.BYTE_COUNT, 'pc_sha256': audit.digest(pc), 'ubo_sha256': audit.digest(ubo),
                                'sha256': hashlib.sha256(b''.join(audit.reference_rows(name))).hexdigest(),
                                **state})
    return result


def populate(directory, document):
    directory.mkdir(mode=0o700)
    for index, letter in enumerate(('A', 'B')):
        (directory/('texture-'+letter+'.rgba')).write_bytes(audit.texture_bytes(index))
    for name in audit.CASES:
        _, pc, ubo = audit.written_state(name)
        (directory/(name+'.bgra')).write_bytes(b''.join(audit.reference_rows(name)))
        (directory/(name+'.pc.bin')).write_bytes(pc)
        (directory/(name+'.ubo.bin')).write_bytes(ubo)
    (directory/'validation.log').write_text('')
    (directory/'manifest.json').write_text(json.dumps(document))


class SampledWriterAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.base = Path(cls.temp.name)
        cls.vertex, cls.fragment = cls.base/'private-a.spv', cls.base/'private-b.spv'
        cls.vertex.write_bytes(module(0)); cls.fragment.write_bytes(module(4))
        cls.modules = [audit.verify_module(cls.vertex, audit.digest(module(0)), 1),
                       audit.verify_module(cls.fragment, audit.digest(module(4)), 16)]
        cls.original = manifest(cls.modules)
        cls.directory = cls.base/'completed'
        populate(cls.directory, cls.original)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        (self.directory/'manifest.json').write_text(json.dumps(self.original))

    def changed(self, mutate):
        value = deepcopy(self.original)
        mutate(value)
        (self.directory/'manifest.json').write_text(json.dumps(value))
        with self.assertRaises((ValueError, KeyError, TypeError)):
            audit.audit_directory(self.directory, self.modules)

    def test_exact_owned_pixels_and_true_negative_writes(self):
        result = audit.audit_directory(self.directory, self.modules)
        self.assertEqual(result['compared_bytes'], 9*14745600)
        cases = {c['name']: c for c in result['cases']}
        expected = {'wrong_image_word': 3686400, 'wrong_ubo_component': 3628800,
                    'wrong_sampler_word': 864000}
        for name, count in expected.items():
            self.assertEqual(cases[name]['actual_state_mismatches']['pixels'], 0)
            self.assertEqual(cases[name]['intended_reference_mismatches']['pixels'], count)
            self.assertGreater(cases[name]['intended_reference_mismatches']['bytes'], count)
        self.assertEqual([x['pixels'] for x in result['coverage_witnesses']], [2764800, 864000])
        self.assertFalse(result['gpu_execution_independently_certified_by_audit'])

    def test_original_independent_cpu_predictions(self):
        expected = {'A_clamp_zero': '6e68d41408a25e7c726a18bd3b21bea401b736f74e19c609fda49fde78b52e10',
                    'B_clamp_zero': '21e294781a7cdf2e60721aa7b65b4f9991f2c41d2f92c91aa2896865f2eeefd9',
                    'B_clamp_offset': 'adc159963d8f05525f079a12e990e14cd7686f7cb063dbc3ca7b25cf60a2f4c6',
                    'B_repeat_offset': '44f528092e3b8afae2cd713e97bc6abb0cb355e4e572e00ca8dd72cb1fd18f48',
                    'B_clamp_offset_center_scissor_LOAD': 'e0114fbd71a171c05f22f8b31a18de185940960c77ef08b3357d05f94b133248',
                    'B_clamp_offset_center_viewport_LOAD': '98bf9c9eeece32b4493503f6178ff8410246baf4fe174ed3cb78dbfd98270309'}
        for name, digest in expected.items():
            result = hashlib.sha256()
            for row in audit.reference_rows(name):
                result.update(row)
            self.assertEqual(result.hexdigest(), digest)
        first = next(audit.reference_rows('A_clamp_zero'))[:4]
        self.assertEqual(first, bytes([118, 162, 17, 255]))  # BGRA, not RGBA; y is flipped.

    def test_no_fabricated_case_or_traversal(self):
        self.changed(lambda d: d['cases'].pop())
        self.changed(lambda d: d['cases'].__setitem__(8, deepcopy(d['cases'][0])))
        self.changed(lambda d: d['cases'][0].update(raw_file='../private-a.spv'))
        self.changed(lambda d: d['cases'][0].update(pc_file='../private-a.spv'))

    def test_validation_and_feature_rejections(self):
        for key in ('debug_messenger', 'synchronization_validation'):
            self.changed(lambda d, key=key: d['validation'].update({key: False}))
        self.changed(lambda d: d['validation'].update(errors=1))
        self.changed(lambda d: d['validation'].update(log_file='../validation.log'))
        for key in audit.FEATURES:
            self.changed(lambda d, key=key: d['features'].update({key: False}))
        self.changed(lambda d: d['features'].update(runtimeDescriptorArray=1))

    def test_state_and_target_retagging_rejected(self):
        self.changed(lambda d: d.update(format=37, target_format='R8G8B8A8_UNORM'))
        self.changed(lambda d: d['pipeline_state'].update(load_op='DONT_CARE'))
        self.changed(lambda d: d['pipeline_state'].update(color_write_mask=7))
        self.changed(lambda d: d['sampler_state'][1].update(address='clamp_to_edge'))
        self.changed(lambda d: d['descriptor_state']['ubo'].update(binding=0))
        self.changed(lambda d: d['cases'][2].update(shift_texels=[0, 0]))
        self.changed(lambda d: d['cases'][8].update(negative_control=False))
        self.changed(lambda d: d['cases'][0].update(pc_sha256='0'*64))
        self.changed(lambda d: d['cases'][0].update(ubo_sha256='0'*64))
        self.changed(lambda d: d.update(game_rendered=True))
        self.changed(lambda d: d['counts'].update(draws=8))
        self.changed(lambda d: d['texture_state'].update(mip_levels=2))

    def test_manifest_cannot_hide_validation_log_error(self):
        path=self.directory/'validation.log'
        try:
            path.write_text('SYNC-HAZARD-WRITE_AFTER_WRITE')
            with self.assertRaises(ValueError):audit.audit_directory(self.directory,self.modules)
        finally:path.write_text('')

    def test_raw_pc_ubo_texture_and_pixel_falsification(self):
        for filename in ('wrong_image_word.pc.bin', 'wrong_ubo_component.ubo.bin', 'texture-B.rgba', 'A_clamp_zero.bgra'):
            path = self.directory/filename
            original = path.read_bytes()
            try:
                changed = bytearray(original); changed[0] ^= 1; path.write_bytes(changed)
                with self.assertRaises(ValueError):
                    audit.audit_directory(self.directory, self.modules)
            finally:
                path.write_bytes(original)

    def test_rehashed_wrong_pixels_do_not_become_reference(self):
        path = self.directory/'A_clamp_zero.bgra'; original = path.read_bytes()
        try:
            changed = bytearray(original); changed[0], changed[2] = changed[2], changed[0]
            path.write_bytes(changed)
            self.changed(lambda d: d['cases'][0].update(sha256=audit.digest(changed)))
        finally:
            path.write_bytes(original)

    def test_gpu_writes_cannot_invent_correct_negative_output(self):
        path = self.directory/'wrong_image_word.bgra'; original = path.read_bytes()
        try:
            forged = (self.directory/'B_clamp_offset.bgra').read_bytes(); path.write_bytes(forged)
            self.changed(lambda d: next(c for c in d['cases'] if c['name']=='wrong_image_word').update(sha256=audit.digest(forged)))
        finally:
            path.write_bytes(original)

    def test_exact_private_module_identity_and_stage(self):
        with self.assertRaises(ValueError):
            audit.verify_module(self.vertex, '0'*64, 1)
        with self.assertRaises(ValueError):
            audit.verify_module(self.vertex, audit.digest(module(0)), 16)
        self.changed(lambda d: d['modules'][0].update(sha256='0'*64))
        self.changed(lambda d: d['modules'][1].update(fnv='1'))
        self.changed(lambda d: d['modules'][0].update(path=str(self.fragment)))
        malformed = self.base/'malformed.spv'; malformed.write_bytes(module(0)+b'\0\0\0\0')
        with self.assertRaises(ValueError):
            audit.verify_module(malformed, audit.digest(malformed.read_bytes()), 1)

    def test_duplicate_fields_nonfinite_and_symlink_rejected(self):
        with self.assertRaises(ValueError): audit.decode('{"x":0,"x":1}')
        with self.assertRaises(ValueError): audit.decode('{"x":NaN}')
        path = self.directory/'texture-A.rgba'; original = path.read_bytes(); target=self.base/'texture-copy';target.write_bytes(original)
        try:
            path.unlink();path.symlink_to(target)
            with self.assertRaises(ValueError):audit.audit_directory(self.directory,self.modules)
        finally:
            path.unlink();path.write_bytes(original)

    def test_mocked_runner_native_runtime_private_redaction(self):
        output = self.base/'mock-run'
        fixture = self.base/'mock-fixture';fixture.write_text('Mock CPU executable, never run')
        args = argparse.Namespace(vertex=self.vertex, fragment=self.fragment,
                                  vertex_sha256=self.modules[0]['sha256'], fragment_sha256=self.modules[1]['sha256'],
                                  fixture=fixture, validation_layer=self.base, runtime=self.base/'mock-runtime',
                                  output=output, validator=None)
        calls=[]
        def mock_run(command, **kwargs):
            calls.append(command)
            directory=Path(command[command.index('--output')+1]);populate(directory,self.original)
            return subprocess.CompletedProcess(command,0,json.dumps({'sampled_writer_gpu_passed':True,'cases':9}), 'Mock CPU only\n')
        with patch.object(audit,'host_command',return_value=[]),patch.object(audit.subprocess,'run',side_effect=mock_run):
            proof=audit.run_controls(args)
        self.assertEqual(len(calls),2);self.assertIn('--batch',calls[1]);self.assertEqual(proof['physical_cases'],18)
        self.assertEqual(proof['total_compared_bytes'],265420800)
        public=(output/'proof.json').read_text()
        for secret in (str(self.vertex),str(self.fragment),self.modules[0]['sha256'],self.modules[1]['sha256'],self.modules[0]['fnv']):
            self.assertNotIn(secret,public)
        self.assertTrue((output/'private-execution.json').is_file())
        with patch.object(audit.subprocess,'run') as run:
            with self.assertRaises(ValueError):audit.run_controls(args)
            run.assert_not_called()

    def test_mocked_runner_refuses_fixture_mutation_after_freeze(self):
        fixture=self.base/'mock-mutating-fixture';fixture.write_bytes(b'original CPU mock')
        args=argparse.Namespace(vertex=self.vertex,fragment=self.fragment,
                               vertex_sha256=self.modules[0]['sha256'],fragment_sha256=self.modules[1]['sha256'],
                               fixture=fixture,validation_layer=self.base,runtime=self.base/'runtime',
                               output=self.base/'mock-mutated',validator=None)
        def mutate(command,**kwargs):
            fixture.write_bytes(b'changed after freeze')
            return subprocess.CompletedProcess(command,0,json.dumps({'sampled_writer_gpu_passed':True,'cases':9}),'')
        with patch.object(audit,'host_command',return_value=[]),patch.object(audit.subprocess,'run',side_effect=mutate):
            with self.assertRaisesRegex(ValueError,'after pre-execution freeze'):audit.run_controls(args)
        self.assertFalse((args.output/'proof.json').exists())

    def test_mocked_runner_validation_failure_is_not_success(self):
        args=argparse.Namespace(vertex=self.vertex,fragment=self.fragment,
                               vertex_sha256=self.modules[0]['sha256'],fragment_sha256=self.modules[1]['sha256'],
                               fixture=self.vertex,validation_layer=self.base,runtime=self.base/'runtime',
                               output=self.base/'mock-invalid',validator=None)
        result=subprocess.CompletedProcess([],0,'{}','SYNC-HAZARD-WRITE_AFTER_WRITE')
        with patch.object(audit,'host_command',return_value=[]),patch.object(audit.subprocess,'run',return_value=result):
            with self.assertRaises(ValueError):audit.run_controls(args)
        self.assertFalse((args.output/'proof.json').exists())


if __name__=='__main__':
    unittest.main()
