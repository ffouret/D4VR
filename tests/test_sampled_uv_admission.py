"""Portable admission orchestration/refusal controls; no private data or process.

Owned SPIR-V reconstruction is real. Legacy baseline admission, full validator
and GPU certificates are mocked; these tests never certify rendered output.
"""
from copy import deepcopy
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import sampled_uv_admission as a
import sampled_uv_probe as uv
import test_sampled_uv_probe as owned


class SampledUVAdmission(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.directory=Path(temp.name);self.root=self.directory/'root';self.inputs=self.directory/'inputs';self.inputs.mkdir()
        for name in {*a.DEPENDENCIES,'tools/sampled_uv_admission.py'}:
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(('synthetic source '+name).encode())
        self.args=SimpleNamespace(baseline=self.inputs/'baseline',original_directory=self.inputs/'originals',
            inventory=self.inputs/'inventory.json',original=self.inputs/'originals/0000000000000003.spv',
            candidate=self.inputs/'candidate/candidate.spv',candidate_report=self.inputs/'candidate/candidate.json',
            certificate=self.inputs/'gpu/proof.json',independent_certificate=self.inputs/'review/proof.json',
            validator=self.inputs/'validator',grammar=self.inputs/'grammar.json',glsl_grammar=self.inputs/'glsl.json',
            output=self.directory/'output',policy=a.POLICY,game_version='owned-test-version')
        self.original=owned.fixture()[0].binary();self.candidate,self.report=uv.construct_bytes(self.original,a.digest(self.original))
        vertex=owned.fixture()[0];vertex.header[3]=5;vertex.instructions=[(17,[1]),(14,[0,1]),(15,[0,1,*owned.string_words('main')]),(19,[2]),(33,[3,2]),(54,[2,1,0,3]),(248,[4]),(253,[]),(56,[])];self.vertex=vertex.binary();self.base_payloads={f'{i:016x}.spv':('unchanged baseline override'+str(i)).encode() for i in (1,2)}
        self.baseline={'schema_version':2,'game_version':self.args.game_version,'purpose':'prior dedicated diagnostic','policy':a.POLICY,
            'runtime_module':str(self.inputs/'runtime'),'runtime_module_sha256':a.digest(b'runtime'),
            'helper_sha256':a.digest(b'base helper'),'gpu_fixture_passed':False,'combined_bundle_gpu_validated':False,
            'targeted_fixed_monoscopic_admitted':True,'heterogeneous_target_helpers':[{'original':'kept'}],
            'entries':[dict(file=n,original_sha256=a.digest(('base original'+n).encode()),override_sha256=a.digest(b)) for n,b in self.base_payloads.items()]}
        self.save(self.args.baseline/'bundle.json',self.baseline)
        self.save(self.args.baseline/'admission.json',{'old_admission':'copied exactly','old_gpu_scope':'unchanged base only'})
        self.write(self.args.baseline/'evidence/legacy.json',b'{ "spacing": "unchanged", "old": true }\n')
        for n,b in self.base_payloads.items():self.write(self.args.baseline/'overrides'/n,b)
        self.write(self.args.original,self.original);self.write(self.args.original_directory/'0000000000000004.spv',self.vertex)
        self.write(self.args.candidate,self.candidate);self.write(self.args.validator,b'owned fake validator');self.write(self.inputs/'runtime',b'runtime')
        self.write(self.args.grammar,b'owned grammar');self.write(self.args.glsl_grammar,b'owned glsl grammar')
        self.save(self.args.inventory,{'shaders':[{'file':'0000000000000003.spv','sha256':a.digest(self.original)},
            {'file':'0000000000000004.spv','sha256':a.digest(self.vertex)}]})
        self.report=uv.verify_candidate(self.original,self.candidate,a.digest(self.original))
        self.report.update(module_validation_performed=True,module_validation_passed=True,
            original_validation=self.validation(self.original),candidate_validation=self.validation(self.candidate),
            constructor_source_sha256=a.fingerprint(self.root/'tools/sampled_uv_probe.py'),dependency_source_sha256=a.source_hashes(self.root,a.UV_DEPENDENCIES))
        self.save(self.args.candidate_report,self.report)
        final=a.source_hashes(self.root,a.pixels.SOURCES);before=deepcopy(final)
        for name in a.COMPARATOR_LEDGER:before[name]=a.digest(('reviewed old '+name).encode())
        self.ledger={name:{'physical_before':before[name],'final':final[name]} for name in a.COMPARATOR_LEDGER}
        self.physical={'modules':[a.module_identity(data,stage) for data,stage,model in ((self.vertex,1,0),(self.original,16,4),(self.candidate,16,4))],
            'constructor':uv.verify_candidate(self.original,self.candidate,a.digest(self.original)),'source_sha256':before,
            'fixture_sha256':a.digest(b'owned GPU binary'),'validator_sha256':a.digest(self.args.validator.read_bytes()),'code_arithmetic_budget':a.pixels.CODE_ARITHMETIC_BUDGET}
        self.save(self.args.certificate.parent/'private-inputs.json',self.physical)
        self.audits={}
        for env in ('native','steam-runtime'):
            rows=[]
            for backend,name in [('descriptor_sets',n) for n in a.pixels.NAMES]+[('descriptor_buffer','B_repeat_offset')]:
                count=a.pixels.base.WIDTH*a.pixels.base.HEIGHT//(4 if 'center_' in name else 1);sha=a.digest(('pixels original '+backend+name).encode())
                rows.append(dict(backend=backend,name=name,original_sha256=sha,diagnostic_sha256=a.digest(('pixels diag '+backend+name).encode()),reference_sha256=sha,
                    original_exact_bytes=a.pixels.base.BYTE_COUNT,preserved_ba_exact_bytes=a.pixels.base.BYTE_COUNT//2,encoded_pixels=count,
                    unchanged_load_pixels=a.pixels.base.WIDTH*a.pixels.base.HEIGHT-count,maximum_observed_r_code_error=.5))
            self.audits[env]=dict(owned_uv_control_passed=True,cases=rows,manifest_sha256={g:a.digest(('manifest '+g).encode()) for g in a.pixels.GROUPS},
                total_readback_bytes=24*a.pixels.base.BYTE_COUNT,validation_errors=0,game_rendered=False,game_pixel_cause_proven=False,xr_submission=False,
                nonfinite_gpu_behavior_certified=False,universal_interpolation_bound_claimed=False)
            self.save(self.args.certificate.parent/(env+'-audit.json'),self.audits[env])
        self.cert=dict(schema_version=1,owned_uv_control_passed=True,physical_readbacks=48,total_readback_bytes=48*a.pixels.base.BYTE_COUNT,validation_errors=0,native_runtime_bytes_equal=True,
            game_bundle_authorized=False,game_rendered=False,coverage_repaired=False,xr_submission=False,nonfinite_gpu_behavior_certified=False,
            read_only_final_audits_equal_initial_saved_audits=True,finalization_GPU_calls=0,finalization_processes=0,
            physical_before_source_sha256=before,final_source_sha256=final,post_physical_changes=self.ledger,fixture_sha256=self.physical['fixture_sha256'],
            physical_private_inputs_sha256=a.fingerprint(self.args.certificate.parent/'private-inputs.json'),
            original_per_environment_audits_sha256={e:a.fingerprint(self.args.certificate.parent/(e+'-audit.json')) for e in self.audits},
            environments=[dict(environment=e,**self.audits[e]) for e in self.audits])
        self.save(self.args.certificate,self.cert)
        pairs=[]
        for env in self.audits:
            for c in self.audits[env]['cases']:
                pairs.append(dict(environment=env,backend=c['backend'],name=c['name'],original_sha256=c['original_sha256'],diagnostic_sha256=c['diagnostic_sha256'],
                    original_exact_bytes=c['original_exact_bytes'],BA_exact_bytes=c['preserved_ba_exact_bytes'],UV_or_overflow_pixels=c['encoded_pixels'],unchanged_LOAD_pixels=c['unchanged_load_pixels'],maximum_observed_code_error=.5))
        self.independent=dict(schema_version=1,status='PASS_read_only_independent_CPU_review_of_parent_GPU_outputs',physical_proof_sha256=a.digest(self.args.certificate.read_bytes()),
            reviewer_source_sha256=a.digest(b'independent reviewer'),source_sha256=final,physical_before_source_sha256=before,post_physical_changes=self.ledger,
            physical_readbacks=48,readback_bytes=48*a.pixels.base.BYTE_COUNT,validation_errors=0,native_runtime_raw_bytes_equal=True,
            GPU_calls_by_reviewer=0,external_processes_forbidden=True,game_executed=False,game_admission=False,XR_submission=False,coverage_repaired=False,
            hardware_transactions_proven=False,finite_only=True,nonfinite_certified=False,opaque_descriptor_bytes_not_decoded=True,
            UNORM_rule='floor_or_ceil_with_fixed0.005code_empirical_budget',manifest_sha256=[self.audits[e]['manifest_sha256'] for e in self.audits],
            paired_results=pairs,falsifications_refused=['alpha','blue','uv','original'])
        self.save(self.args.independent_certificate,self.independent)
        self.repin()
        def old_validate(directory,*,game_version,policy):
            self.assertEqual(game_version,self.args.game_version);self.assertEqual(policy,a.POLICY)
            result=a.decode((directory/'bundle.json').read_bytes())
            for e in result['entries']:a.read(directory/'overrides'/e['file'],e['override_sha256'])
            return result
        patches=[patch.object(a,'__file__',str(self.root/'tools/sampled_uv_admission.py')),patch.object(a,'BASE_ENTRY_COUNT',2),
            patch.object(a,'COMPARATOR_LEDGER',deepcopy(self.ledger)),patch.object(a.probe,'profile',return_value={'game_version':self.args.game_version}),
            patch.object(a.base,'validate_admission',side_effect=old_validate),patch.object(a,'Policy',return_value=SimpleNamespace(validate=lambda data,*_:self.validation(data))),
            patch('subprocess.Popen',side_effect=AssertionError('Admission units forbid every external process')),
            patch('subprocess.run',side_effect=AssertionError('Admission units forbid every external process'))]
        for p in patches:p.start();self.addCleanup(p.stop)
    def write(self,p,data):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    def save(self,p,obj):self.write(p,a.encoded(obj))
    def repin(self):
        for name in a.INPUTS:
            p=self.args.baseline/('bundle.json' if name=='baseline_bundle' else 'admission.json') if name.startswith('baseline_') else getattr(self.args,name)
            setattr(self.args,name+'_sha256',a.digest(p.read_bytes()))
    def validation(self,data):return dict(module_validation_passed=True,validated_non_output_external_writes_excluded=True,core_writes_by_storage={'Output':4},extended_writes_by_storage={},sha256=a.digest(data))
    def prepare(self):return a.prepare(self.args)
    def validate(self):return a.validate_admission(self.args.output,game_version=self.args.game_version)
    def mutate_certificate(self,fn):
        cert=deepcopy(self.cert);fn(cert);self.save(self.args.certificate,cert)
        independent=deepcopy(self.independent);independent['physical_proof_sha256']=a.fingerprint(self.args.certificate);self.save(self.args.independent_certificate,independent);self.repin()

    def test_preflight_is_read_only_and_uses_real_owned_reconstruction(self):
        pre=a.check_inputs(self.args);self.assertFalse(self.args.output.exists());self.assertEqual(pre['target']['file'],'0000000000000003.spv')
        self.assertEqual(pre['reconstruction']['structural_preservation']['changed_store_value_operands'],2)
        self.assertFalse(pre['reconstruction']['structural_preservation']['all_original_stores_preserved'])
    def test_exact_copy_one_target_separate_evidence_and_permissions(self):
        before=a.tree(self.args.baseline);receipt=self.prepare();manifest=self.validate()
        self.assertEqual(manifest['entries'][:2],self.baseline['entries']);self.assertEqual(len(manifest['entries']),3)
        self.assertEqual(a.tree(self.args.output/'baseline'),before);self.assertEqual(a.tree(self.args.baseline),before)
        self.assertEqual((self.args.output/'evidence/legacy.json').read_bytes(),(self.args.baseline/'evidence/legacy.json').read_bytes())
        self.assertEqual((self.args.output/'baseline/admission.json').read_bytes(),(self.args.baseline/'admission.json').read_bytes())
        for n,b in self.base_payloads.items():self.assertEqual((self.args.output/'overrides'/n).read_bytes(),b)
        self.assertEqual((self.args.output/'overrides/0000000000000003.spv').read_bytes(),self.candidate)
        self.assertEqual(receipt['total_entries'],3);self.assertEqual(receipt['added_candidates'],1)
        self.assertEqual(manifest['sampled_uv_target']['scope'],'all_uses_of_original_fragment_content')
        for key in ('gpu_fixture_passed','combined_bundle_gpu_validated','game_bundle_gpu_validated','game_executed','xr_submission','coverage_repaired'):self.assertIs(manifest[key],False)
        if os.name=='posix':
            for p in [self.args.output,*self.args.output.rglob('*')]:self.assertEqual(stat.S_IMODE(p.stat().st_mode),0o700 if p.is_dir() else 0o600)
    def test_all_explicit_hashes_reject_none_bool_partial_or_stale(self):
        for name in a.INPUTS:
            attr=name+'_sha256';old=getattr(self.args,attr)
            for bad in (None,False,'','A'*64,'a'*63,'0'*64):
                setattr(self.args,attr,bad)
                with self.subTest(name=name,bad=bad),self.assertRaises(ValueError):a.check_inputs(self.args)
                self.assertFalse(self.args.output.exists())
            setattr(self.args,attr,old)
    def test_policy_version_and_protected_or_existing_outputs(self):
        for key,bad in (('policy','stereo_ab'),('game_version','other')):
            old=getattr(self.args,key);setattr(self.args,key,bad)
            with self.assertRaises(ValueError):self.prepare()
            setattr(self.args,key,old)
        for p in (self.args.baseline,self.args.baseline/'new',self.args.original_directory/'new',self.args.candidate.parent/'new',self.root/'new'):
            self.args.output=p
            with self.assertRaises(ValueError):self.prepare()
        game=self.directory/'game';game.mkdir();(game/'Diablo IV.exe').write_bytes(b'owned game marker');self.args.output=game/'new'
        with self.assertRaises(ValueError):self.prepare()
    def test_parent_traversal_cannot_bypass_output_protection(self):
        for protected in (self.root,self.args.baseline,self.args.original_directory,self.args.candidate.parent):
            self.args.output=protected.parent/protected.name/'..'/protected.name/'new'
            with self.subTest(protected=protected),self.assertRaises(ValueError):self.prepare()
            self.assertFalse((protected/'new').exists())

    def test_symlink_input_parent_output_and_base_member_refused(self):
        link=self.directory/'link'
        try:link.symlink_to(self.inputs,target_is_directory=True)
        except (OSError,NotImplementedError):self.skipTest('Symlinks unsupported')
        old=self.args.original;self.args.original=link/'originals'/old.name
        with self.assertRaises(ValueError):self.prepare()
        self.args.original=old;self.args.output=link/'new'
        with self.assertRaises(ValueError):self.prepare()
        self.args.output=self.directory/'output';(self.args.baseline/'evidence/symlink').symlink_to(self.args.original)
        with self.assertRaises(ValueError):self.prepare()
    def test_duplicate_inventory_target_collision_and_original_directory_change(self):
        inv=a.decode(self.args.inventory.read_bytes());inv['shaders'].append(deepcopy(inv['shaders'][0]));self.save(self.args.inventory,inv);self.repin()
        with self.assertRaisesRegex(ValueError,'Duplicate'):self.prepare()
        inv['shaders'].pop();self.save(self.args.inventory,inv);self.repin()
        self.write(self.args.original,b'changed original')
        with self.assertRaises(ValueError):self.prepare()
        self.write(self.args.original,self.original)
        baseline=deepcopy(self.baseline);baseline['entries'][0]['file']='0000000000000003.spv';self.write(self.args.baseline/'overrides'/baseline['entries'][0]['file'],self.base_payloads['0000000000000001.spv']);self.save(self.args.baseline/'bundle.json',baseline);self.repin()
        with self.assertRaisesRegex(ValueError,'collides'):self.prepare()
    def test_original_retagged_as_candidate_and_valid_but_wrong_candidate_refused(self):
        for data in (self.original,owned.candidate_negative_controls(self.original,self.candidate)[0][1]):
            self.write(self.args.candidate,data);report=deepcopy(self.report);report['candidate_sha256']=a.digest(data);self.save(self.args.candidate_report,report);self.repin()
            with self.assertRaisesRegex(ValueError,'reconstruction'):self.prepare()
    def test_report_legacy_allstores_game_or_stale_dependency_claims_refused(self):
        changes=[lambda r:r['structural_preservation'].update(all_original_stores_preserved=True),lambda r:r.update(game_bundle_authorized=True),
            lambda r:r.update(game_admission=True),lambda r:r.update(GPU_validation_performed=True),lambda r:r.update(constructor_source_sha256='0'*64),
            lambda r:r['dependency_source_sha256'].pop(next(iter(r['dependency_source_sha256']))),lambda r:r['candidate_validation'].update(module_validation_passed=False)]
        for fn in changes:
            report=deepcopy(self.report);fn(report);self.save(self.args.candidate_report,report);self.repin()
            with self.assertRaises(ValueError):self.prepare()
    def test_physical_ledger_only_reviewed_comparator_change_allowed(self):
        changes=[lambda c:c['physical_before_source_sha256'].update({'native/tests/sampled_uv_gpu.cpp':'0'*64}),
            lambda c:c['final_source_sha256'].pop('tools/sampled_uv_probe.py'),lambda c:c['post_physical_changes'].clear(),
            lambda c:c['post_physical_changes']['tools/sampled_uv_validate.py'].update(physical_before='0'*64),
            lambda c:c.update(finalization_GPU_calls=1),lambda c:c.update(finalization_processes=1),lambda c:c.update(read_only_final_audits_equal_initial_saved_audits=False)]
        for fn in changes:
            self.mutate_certificate(fn)
            with self.assertRaises(ValueError):self.prepare()
    def test_gpu_errors_missing_environment_cases_backend_and_counts_refused(self):
        for fn in (lambda c:c.update(validation_errors=1),lambda c:c.update(physical_readbacks=44),lambda c:c['environments'].pop(),
                   lambda c:c.update(game_bundle_authorized=True),lambda c:c.update(nonfinite_gpu_behavior_certified=True),
                   lambda c:c['original_per_environment_audits_sha256'].pop('native')):
            self.mutate_certificate(fn)
            with self.assertRaises((ValueError,KeyError)):self.prepare()
    def test_independent_crosslink_pixels_scope_and_missing_falsifications_refused(self):
        changes=[lambda c:c.update(physical_proof_sha256='0'*64),lambda c:c.update(game_admission=True),lambda c:c.update(validation_errors=1),
            lambda c:c.update(source_sha256={}),lambda c:c['paired_results'].pop(),lambda c:c['paired_results'][0].update(BA_exact_bytes=0),
            lambda c:c['paired_results'][0].update(original_sha256='0'*64),lambda c:c.update(falsifications_refused=[])]
        for fn in changes:
            c=deepcopy(self.independent);fn(c);self.save(self.args.independent_certificate,c);self.repin()
            with self.assertRaises(ValueError):self.prepare()
    def test_failed_fresh_module_validation_refused_before_output(self):
        with patch.object(a,'Policy',return_value=SimpleNamespace(validate=lambda *_:dict(module_validation_passed=False))):
            with self.assertRaisesRegex(ValueError,'full validation'):self.prepare()
        self.assertFalse(self.args.output.exists())
    def test_source_or_base_payload_mutation_refused(self):
        for p in (self.root/'tools/sampled_uv_probe.py',self.root/'tools/shader_effects.py',self.root/'native/tests/sampled_uv_gpu.cpp',self.args.baseline/'overrides/0000000000000001.spv'):
            old=p.read_bytes();p.write_bytes(b'changed')
            with self.assertRaises(ValueError):self.prepare()
            p.write_bytes(old)
    def test_launch_rechecks_all_inputs_payloads_copies_and_unlisted_files(self):
        self.prepare()
        for p in (self.args.candidate,self.args.original,self.args.output/'overrides/0000000000000003.spv',
            self.args.output/'overrides/0000000000000001.spv',self.args.output/'baseline/evidence/legacy.json',self.args.baseline/'evidence/legacy.json',self.args.output/'evidence/uv/gpu-certificate.json'):
            old=p.read_bytes();p.write_bytes(b'changed')
            with self.assertRaises(ValueError):self.validate()
            p.write_bytes(old)
        extra=self.args.output/'overrides/unlisted.spv';extra.write_bytes(b'not admitted')
        with self.assertRaises(ValueError):self.validate()
    def test_launch_rejects_rehashed_manifest_transport_or_scope_promotion(self):
        self.prepare();receipt=a.decode((self.args.output/'admission.json').read_bytes());manifest=a.decode((self.args.output/'bundle.json').read_bytes())
        changes=[lambda m:m.update(policy='stereo_ab'),lambda m:m.update(gpu_fixture_passed=True),lambda m:m.update(combined_bundle_gpu_validated=True),
            lambda m:m.update(game_bundle_gpu_validated=True),lambda m:m['sampled_uv_target'].update(scope='one_old_pipeline'),lambda m:m.update(runtime_module='other'),
            lambda m:m['entries'][0].update(override_sha256='0'*64)]
        for fn in changes:
            m=deepcopy(manifest);fn(m);self.save(self.args.output/'bundle.json',m);r=deepcopy(receipt);r['bundle_sha256']=a.fingerprint(self.args.output/'bundle.json');self.save(self.args.output/'admission.json',r)
            with self.assertRaises(ValueError):self.validate()
    def test_launch_requires_all_pins_evidence_and_current_admission_source(self):
        self.prepare();path=self.args.output/'admission.json';original=a.decode(path.read_bytes())
        for fn in (lambda r:r.update(bundle_sha256=None),lambda r:r['input_records']['original'].update(sha256=None),
            lambda r:r['evidence_sha256'].pop('evidence/uv/native-audit.json'),lambda r:r['sampled_uv_target'].update(all_original_stores_preserved=True),
            lambda r:r.update(admission_source_sha256='0'*64),lambda r:r.update(dependency_source_sha256={})): 
            r=deepcopy(original);fn(r);self.save(path,r)
            with self.assertRaises(ValueError):self.validate()
        self.save(path,original);source=self.root/'tools/sampled_uv_admission.py';source.write_bytes(b'changed admission')
        with self.assertRaises(ValueError):self.validate()
    def test_generic_bundle_validator_remains_refusing(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError,'validated GPU'):a.base.validate_bundle(self.args.output)
    def test_source_change_during_copy_cannot_be_retagged_as_new_admission(self):
        original_write=a.write;changed=[]
        def racing_write(path,data):
            original_write(path,data)
            if not changed:
                changed.append(True);(self.root/'tools/sampled_uv_admission.py').write_bytes(b'changed after preflight')
        with patch.object(a,'write',side_effect=racing_write):
            with self.assertRaisesRegex(ValueError,'source changed during copying'):self.prepare()
        self.assertFalse((self.args.output/'admission.json').exists())

    def test_physical_module_bytes_stage_entry_and_fnv_are_checked(self):
        modules=deepcopy(self.physical['modules']);modules[0]['fnv']='0'
        with self.assertRaises(ValueError):a.verify_module_triplet(modules,self.vertex,self.original,self.candidate)
        with self.assertRaises(ValueError):a.module_identity(self.original,1)
        vertex=uv.Module(self.vertex);next(args for op,args in vertex.instructions if op==15)[0]=4
        with self.assertRaises(ValueError):a.module_identity(vertex.binary(),1)

    def test_json_duplicate_fields_and_bool_counts_are_not_numeric_pins(self):
        with self.assertRaises(ValueError):a.decode(b'{"same":1,"same":2}')
        self.mutate_certificate(lambda c:c.update(finalization_GPU_calls=False))
        with self.assertRaises(ValueError):self.prepare()

if __name__=='__main__':unittest.main()
