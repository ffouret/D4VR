"""Private admission integrity controls with synthetic certificates, CPU only.

Compiler/validator operations are mocked and all subprocesses are forbidden.
These units certify orchestration and refusals, not SPIR-V or rendered output.
"""
from copy import deepcopy
import json
import os
from pathlib import Path
import stat
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import post_depth_admission as admission
import test_post_depth_candidate as certificates


class PostDepthAdmission(unittest.TestCase):
    def setUp(self):
        self.fixture = certificates.PostDepthCandidate()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.directory, self.root = self.fixture.directory, self.fixture.root
        self.source_file = self.root/'tools/post_depth_admission.py'
        self.source_file.write_bytes(b'synthetic admission source')
        self.sources = {'0000000000000001.spv':b'baseline original',
                        '0000000000000002.spv':b'bias original',
                        '0000000000000003.spv':b'clamp original'}
        self.candidates = {b'bias original':b'correctly rebuilt bias',
                           b'clamp original':b'correctly rebuilt clamp'}
        self.runtime = self.directory/'runtime.dll'
        self.runtime.write_bytes(b'pinned fake runtime')
        self.args = SimpleNamespace(
            baseline=self.directory/'baseline',
            bias_candidate=self.directory/'bias',clamp_candidate=self.directory/'clamp',
            original_directory=self.directory/'originals',inventory=self.directory/'inventory.json',
            certificate=self.directory/'gpu.json',helper_certificate=self.directory/'helper.json',
            tools=self.directory/'tools',grammar=self.directory/'core.json',
            glsl_grammar=self.directory/'glsl.json',policy=admission.POLICY,
            game_version='test-v1',output=self.directory/'output')
        self.baseline = dict(schema_version=2,game_version='test-v1',gpu_fixture_passed=True,
            runtime_module=str(self.runtime),runtime_module_sha256=admission.digest(self.runtime.read_bytes()),
            helper_sha256='b'*64,purpose='baseline diagnostic',
            entries=[dict(file='0000000000000001.spv',
                          original_sha256=admission.digest(self.sources['0000000000000001.spv']),
                          override_sha256=admission.digest(b'baseline override'),baseline_marker='kept')])
        self.reset_inputs()
        self.rebuild_result = None
        self.build_calls = []
        self.policy_result = dict(core_writes_by_storage={'Output':1},
                                  validated_non_output_external_writes_excluded=True)
        def closed(data,*_):
            kind = 'affine_bias_over_w' if data==self.sources['0000000000000002.spv'] else 'min_z_w'
            return dict(suffix_kind=kind,shader_sha256=admission.digest(data))
        def rebuild(source,helper,output,tools,*args,**kwargs):
            self.assertEqual(helper.read_bytes(),self.fixture.helper_bytes)
            self.assertEqual(args,(3,0,72))
            self.assertEqual(kwargs,dict(stereo=True,helper_no_telemetry=True,
                                        projection_only_helper=True,projection_input=True))
            self.build_calls.append(source.read_bytes())
            output.write_bytes(self.rebuild_result if self.rebuild_result is not None
                               else self.candidates[source.read_bytes()])
        def no_process(*_,**__):
            raise AssertionError('Actual process execution forbidden in admission units')
        mocks = [patch.object(admission,'__file__',str(self.source_file)),
                 patch.object(admission,'BASE_ENTRY_COUNT',1),
                 patch.object(admission.probe,'profile',return_value={'game_version':'test-v1'}),
                 patch.object(admission,'Policy',return_value=SimpleNamespace(
                     glsl=object(),validate=lambda *_:deepcopy(self.policy_result))),
                 patch.object(admission,'assess',return_value={'core_writes_by_storage':{'Output':1}}),
                 patch.object(admission,'validated',side_effect=closed),
                 patch.object(admission,'verify_projection_only',return_value={'projection_only_abi_verified':True}),
                 patch.object(admission,'build',side_effect=rebuild),
                 patch.object(admission.subprocess,'run',side_effect=no_process),
                 patch.object(admission.subprocess,'Popen',side_effect=no_process)]
        for mocked in mocks:
            mocked.start()
            self.addCleanup(mocked.stop)

    def save(self,path,value):
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(admission.json_bytes(value))

    def reset_inputs(self):
        a=self.args
        self.save(a.baseline/'bundle.json',self.baseline)
        (a.baseline/'overrides').mkdir(exist_ok=True)
        (a.baseline/'overrides'/'0000000000000001.spv').write_bytes(b'baseline override')
        a.original_directory.mkdir(exist_ok=True)
        for name,data in self.sources.items():
            (a.original_directory/name).write_bytes(data)
        self.save(a.inventory,{'shaders':[{'file':name,'sha256':admission.digest(data)}
                                         for name,data in self.sources.items()]})
        self.save(a.certificate,self.fixture.cert)
        self.save(a.helper_certificate,self.fixture.helper_cert)
        a.grammar.write_bytes(b'fake grammar'); a.glsl_grammar.write_bytes(b'fake glsl grammar')
        for index,(label,kind) in enumerate(admission.KINDS.items(),2):
            directory=getattr(a,label+'_candidate')
            directory.mkdir(exist_ok=True)
            source=self.sources[f'{index:016x}.spv']
            candidate=self.candidates[source]
            (directory/'original.snapshot.spv').write_bytes(source)
            (directory/'helper.snapshot.spv').write_bytes(self.fixture.helper_bytes)
            (directory/'candidate.spv').write_bytes(candidate)
            self.save(directory/'candidate.json',dict(
                original_sha256=admission.digest(source),helper_sha256=self.fixture.digest,
                candidate_sha256=admission.digest(candidate),
                certificate_sha256=admission.digest(a.certificate.read_bytes()),
                helper_certificate_sha256=admission.digest(a.helper_certificate.read_bytes()),
                constructor_source_sha256=admission.probe.digest(self.root/'tools/post_depth_candidate.py'),
                closed_original_contract={'shader_sha256':admission.digest(source),'suffix_kind':kind},
                original_stores_and_suffix_arithmetic_preserved_before_link=True,
                projection_application='canonical_input_loads',
                clip_output_units='source_VIEW_clip_units_after_metric_conversion',
                game_bundle_authorized=False,game_executed=False,xr_submission=False,coverage_repaired=False))

    def receipt(self,label='bias'):
        return json.loads((getattr(self.args,label+'_candidate')/'candidate.json').read_bytes())

    def save_receipt(self,receipt,label='bias'):
        self.save(getattr(self.args,label+'_candidate')/'candidate.json',receipt)

    def fresh_output(self,index):
        self.args.output=self.directory/('output-'+str(index))

    def prepare(self):
        return admission.prepare(self.args)

    def verify(self):
        return admission.validate_admission(self.args.output,game_version='test-v1')

    def rehash_manifest(self,manifest):
        self.save(self.args.output/'bundle.json',manifest)
        receipt=json.loads((self.args.output/'admission.json').read_bytes())
        receipt['bundle_sha256']=admission.digest((self.args.output/'bundle.json').read_bytes())
        self.save(self.args.output/'admission.json',receipt)

    def test_exact_copy_two_rebuilt_candidates_separate_evidence_and_permissions(self):
        result=self.prepare()
        manifest=self.verify()
        self.assertEqual(self.build_calls,[b'bias original',b'clamp original'])
        self.assertEqual(manifest['entries'][:1],self.baseline['entries'])
        self.assertEqual(manifest['helper_sha256'],'b'*64)
        self.assertEqual([t['helper_sha256'] for t in manifest['heterogeneous_target_helpers']],
                         [self.fixture.digest]*2)
        self.assertIs(manifest['gpu_fixture_passed'],False)
        self.assertIs(manifest['combined_bundle_gpu_validated'],False)
        self.assertIs(manifest['baseline_evidence']['gpu_fixture_passed'],True)
        self.assertEqual(result['total_entries'],3)
        self.assertEqual(result['policy'],'fixed_monoscopic_no_xr')
        self.assertEqual(len(result['evidence_sha256']),10)
        for key in ('game_executed','xr_submission','coverage_repaired'):
            self.assertIs(result[key],False)
        self.assertEqual((self.args.baseline/'overrides'/'0000000000000001.spv').read_bytes(),b'baseline override')
        self.assertEqual(json.loads((self.args.baseline/'bundle.json').read_bytes()),self.baseline)
        for name,data in self.sources.items():
            self.assertEqual((self.args.original_directory/name).read_bytes(),data)
        if os.name=='posix':
            self.assertEqual(stat.S_IMODE(self.args.output.stat().st_mode),0o700)
            for path in self.args.output.rglob('*'):
                self.assertEqual(stat.S_IMODE(path.stat().st_mode),0o700 if path.is_dir() else 0o600)

    def test_policy_version_existing_and_protected_output_refusals(self):
        for key,value in (('policy','stereo_ab'),('game_version','other-version')):
            old=getattr(self.args,key); setattr(self.args,key,value)
            with self.assertRaises(ValueError): self.prepare()
            setattr(self.args,key,old)
        for output in (self.args.baseline,self.args.baseline/'new',
                       self.args.original_directory/'new',self.args.bias_candidate/'new',self.args.clamp_candidate/'new'):
            self.args.output=output
            with self.assertRaises(ValueError): self.prepare()
        game=self.directory/'game'; game.mkdir(); (game/'Diablo IV.exe').write_bytes(b'fake')
        self.args.output=game/'new'
        with self.assertRaises(ValueError): self.prepare()

    def test_missing_or_malformed_expected_hash_never_becomes_unpinned_read(self):
        for value in (None,False,'','a'*63,'A'*64,0):
            with self.subTest(value=value), self.assertRaises(ValueError):
                admission.read(self.runtime,value)

    def test_original_inventory_and_unchanged_baseline_payloads_are_required(self):
        for index,kind in enumerate(('original','override','inventory','duplicate')):
            self.reset_inputs(); self.fresh_output(index)
            if kind=='original':
                (self.args.original_directory/'0000000000000001.spv').write_bytes(b'changed')
            elif kind=='override':
                (self.args.baseline/'overrides'/'0000000000000001.spv').write_bytes(b'changed')
            else:
                inventory=json.loads(self.args.inventory.read_bytes())
                if kind=='inventory': inventory['shaders'][0]['sha256']=None
                else: inventory['shaders'].append(deepcopy(inventory['shaders'][0]))
                self.save(self.args.inventory,inventory)
            with self.subTest(kind=kind),self.assertRaises(ValueError): self.prepare()
            self.assertFalse(self.args.output.exists())

    def test_candidate_identity_flags_suffix_and_source_refusals(self):
        changes=[('candidate_sha256',None),('helper_sha256',None),('original_sha256',None),
                 ('certificate_sha256','0'*64),('helper_certificate_sha256','0'*64),
                 ('constructor_source_sha256','0'*64),('game_executed',True),
                 ('game_bundle_authorized',True),('xr_submission',True),('coverage_repaired',True),
                 ('projection_application','late_position')]
        for index,(key,value) in enumerate(changes):
            self.reset_inputs(); self.fresh_output(index)
            receipt=self.receipt(); receipt[key]=value; self.save_receipt(receipt)
            with self.subTest(key=key),self.assertRaises(ValueError): self.prepare()
            self.assertFalse(self.args.output.exists())
        self.reset_inputs(); self.fresh_output('suffix')
        receipt=self.receipt(); receipt['closed_original_contract']['suffix_kind']='min_z_w'; self.save_receipt(receipt)
        with self.assertRaises(ValueError): self.prepare()
        self.reset_inputs(); self.fresh_output('source')
        (self.args.original_directory/'0000000000000002.spv').write_bytes(b'changed')
        with self.assertRaises(ValueError): self.prepare()

    def test_uninstrumented_original_with_retagged_receipt_is_refused(self):
        path=self.args.bias_candidate/'candidate.spv'
        path.write_bytes(self.sources['0000000000000002.spv'])
        receipt=self.receipt(); receipt['candidate_sha256']=admission.digest(path.read_bytes()); self.save_receipt(receipt)
        with self.assertRaisesRegex(ValueError,'deterministic snapshot reconstruction'): self.prepare()
        self.assertFalse(self.args.output.exists())

    def test_reconstruction_mismatch_or_new_external_writes_fail_before_output(self):
        self.rebuild_result=b'different reconstruction'
        with self.assertRaisesRegex(ValueError,'deterministic snapshot reconstruction'): self.prepare()
        self.assertFalse(self.args.output.exists())
        self.rebuild_result=None
        for key,value in (('validated_non_output_external_writes_excluded',False),
                           ('core_writes_by_storage',{'Output':2})):
            old=self.policy_result[key]; self.policy_result[key]=value
            with self.assertRaisesRegex(ValueError,'effect/output contract'): self.prepare()
            self.policy_result[key]=old

    def test_missing_gpu_environment_controls_and_static_only_are_refused(self):
        for index,kind in enumerate(('environment','naive','late','static')):
            self.reset_inputs(); self.fresh_output(index)
            certificate=deepcopy(self.fixture.cert)
            if kind=='environment':
                certificate['gpu_cases']=[c for c in certificate['gpu_cases'] if c['environment']=='native']
            elif kind=='static': certificate['gpu_validation_performed']=False
            else: certificate['gpu_cases']=[c for c in certificate['gpu_cases'] if c['control']!=kind]
            self.save(self.args.certificate,certificate)
            for label in admission.KINDS:
                receipt=self.receipt(label); receipt['certificate_sha256']=admission.digest(self.args.certificate.read_bytes())
                self.save_receipt(receipt,label)
            with self.subTest(kind=kind),self.assertRaises(ValueError): self.prepare()

    def test_launch_checks_every_payload_and_refuses_unlisted_override(self):
        self.prepare()
        path=self.args.output/'overrides'/'0000000000000002.spv'
        original=path.read_bytes(); path.write_bytes(b'changed')
        with self.assertRaises(ValueError): self.verify()
        path.write_bytes(original)
        (self.args.output/'overrides'/'extra.spv').write_bytes(b'not listed')
        with self.assertRaisesRegex(ValueError,'unlisted'): self.verify()

    def test_launch_requires_all_pinned_hashes_and_all_evidence(self):
        self.prepare()
        path=self.args.output/'admission.json'; original=json.loads(path.read_bytes())
        for key in ('bundle_sha256','baseline_manifest_sha256'):
            receipt=deepcopy(original); receipt[key]=None; self.save(path,receipt)
            with self.subTest(key=key),self.assertRaises(ValueError): self.verify()
        for key in original['evidence_sha256']:
            receipt=deepcopy(original); receipt['evidence_sha256'][key]=None; self.save(path,receipt)
            with self.subTest(key=key),self.assertRaises(ValueError): self.verify()
        receipt=deepcopy(original); receipt['evidence_sha256'].pop('evidence/gpu-certificate.json'); self.save(path,receipt)
        with self.assertRaisesRegex(ValueError,'incomplete'): self.verify()
        self.save(path,original)
        manifest=json.loads((self.args.output/'bundle.json').read_bytes())
        manifest['entries'][1]['override_sha256']=None
        manifest['heterogeneous_target_helpers'][0]['override_sha256']=None
        self.rehash_manifest(manifest)
        with self.assertRaises(ValueError): self.verify()

    def test_launch_crosslinks_targets_receipts_entries_and_helpers(self):
        self.prepare()
        original=json.loads((self.args.output/'bundle.json').read_bytes())
        for key,value in (('file','0000000000000003.spv'),('helper_sha256','0'*64),
                           ('candidate_receipt_sha256',None),('suffix_kind','min_z_w'),
                           ('override_sha256','0'*64),('original_sha256','0'*64)):
            manifest=deepcopy(original); manifest['heterogeneous_target_helpers'][0][key]=value
            self.rehash_manifest(manifest)
            with self.subTest(key=key),self.assertRaises(ValueError): self.verify()

    def test_launch_revalidates_current_gpu_helper_and_constructor_sources(self):
        self.prepare()
        names=(certificates.candidate.CERTIFICATE_SOURCES[0],
               certificates.HELPER_SOURCES[0],'tools/post_depth_candidate.py')
        for name in names:
            path=self.root/name; data=path.read_bytes(); path.write_bytes(b'changed source')
            with self.subTest(name=name),self.assertRaises(ValueError): self.verify()
            path.write_bytes(data)
        self.assertEqual(len(self.verify()['entries']),3)

    def test_launch_rejects_aggregate_gpu_or_stereo_permission_and_runtime_change(self):
        self.prepare()
        original=json.loads((self.args.output/'bundle.json').read_bytes())
        for key,value in (('gpu_fixture_passed',True),('combined_bundle_gpu_validated',True),
                           ('xr_submission',True),('policy','stereo_ab')):
            manifest=deepcopy(original); manifest[key]=value; self.rehash_manifest(manifest)
            with self.subTest(key=key),self.assertRaises(ValueError): self.verify()
        self.rehash_manifest(original)
        self.runtime.write_bytes(b'changed runtime')
        with self.assertRaisesRegex(ValueError,'runtime changed'): self.verify()


if __name__=='__main__':
    unittest.main()
