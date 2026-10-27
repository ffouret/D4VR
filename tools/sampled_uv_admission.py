#!/usr/bin/env python3
"""Dedicated one-fragment UV admission; prepare privately, never launch/install.

The unchanged post-depth baseline keeps its own admission. Exactly two store
VALUE operands in one closed fragment content identity may change; this is not
an all-stores-preserved or combined game GPU certificate.
"""
from __future__ import annotations
import argparse
from copy import deepcopy
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

import post_depth_admission as base
import probe
import sampled_uv_probe as uv
import sampled_uv_validate as pixels
from shader_write_contract import Policy

POLICY = 'fixed_monoscopic_no_xr'
PURPOSE = 'targeted_sampled_uv_fixed_monoscopic_diagnostic'
BASE_ENTRY_COUNT = 5379
MAX_FILE = 64*1024*1024
MAX_BASE_FILES = 8192
MAX_BASE_BYTES = 512*1024*1024
NAME = re.compile(r'[0-9a-f]{16}\.spv')
HASH = re.compile(r'[0-9a-f]{64}')
INPUTS = ('baseline_bundle', 'baseline_admission', 'inventory', 'original', 'candidate',
          'candidate_report', 'certificate', 'independent_certificate', 'validator', 'grammar', 'glsl_grammar')
UV_DEPENDENCIES = ('tools/spirv_probe.py', 'tools/shader_write_contract.py', 'tools/shader_effects.py',
                   'tools/glsl_effects.py', 'tools/shader_effects_validate.py')
DEPENDENCIES = (*UV_DEPENDENCIES, *pixels.SOURCES, 'tools/post_depth_admission.py', 'tools/camera_capture.py', 'tools/probe.py')
# Reviewed PUBLIC code changes only. These pin the aggregate comparator repair,
# never a private shader/content identity. All six other physical inputs match.
COMPARATOR_LEDGER = {
 'tools/sampled_uv_validate.py': {'physical_before':'4e4e13aedfcd3a5eae4bd8b6249e34ffac9baadfe43ea4f6947cd11f703b6f20',
                                  'final':'9673219b158d5aacfa62f3c5912a5f8ccc95d50387868a770bcc19196071b2e5'},
 'tests/test_sampled_uv_validate.py': {'physical_before':'a0959244bb2cfc55a798413be5c0fd173ad61feedeedc274e339d38b1c321f55',
                                       'final':'bc2f8b131c84595b7408e65e177242f7a0ce6096044257fcd17f5fd3f1f03d59'}}
LIMITS = [
 'Exactly one fixed monoscopic diagnostic; no XR, stereo A/B or depth variant.',
 'All uses of the original fragment CONTENT may change, not one numeric pipeline identity.',
 'Exactly two original R/G Store VALUE operands change; older all-stores-preserved certificates do not apply.',
 'The unchanged baseline and owned UV controls are separate evidence; the combined game bundle has no GPU certificate.',
 'Finite interior codes1..254 only; codes0/255 unavailable, nonfinite GPU behavior uncertified.',
 'Output declarations alone do not prove attachment binding, downstream transforms, last writer, coverage, or numeric decoding.',
 'No exact original/candidate game pixel causality across successive or separate sessions.']


def digest(data):return hashlib.sha256(data).hexdigest()
def require_hash(value):
    if not isinstance(value,str) or not HASH.fullmatch(value):raise ValueError('Explicit complete lowercase SHA256 required')
    return value

def safe_path(path):
    path=Path(path).absolute()
    if any(p.is_symlink() for p in (path,*path.parents)):raise ValueError('Symlink path refused: '+str(path))
    # Canonicalize traversal only after rejecting symlinks on the lexical path.
    return path.resolve()

def read(path, expected, maximum=MAX_FILE):
    require_hash(expected);path=safe_path(path)
    if not path.is_file() or path.stat().st_size>maximum:raise ValueError('Bounded regular admission input required: '+str(path))
    with path.open('rb') as f:data=f.read(maximum+1)
    if len(data)>maximum or digest(data)!=expected:raise ValueError('Admission input fingerprint/size changed: '+str(path))
    return data

def fingerprint(path):
    path=safe_path(path)
    if not path.is_file() or path.stat().st_size>MAX_FILE:raise ValueError('Bounded regular file required')
    with path.open('rb') as f:data=f.read(MAX_FILE+1)
    if len(data)>MAX_FILE:raise ValueError('Input grew beyond bound')
    return digest(data)

def decode(data):return pixels.base.decode(data)
def encoded(value):return (json.dumps(value,indent=2,allow_nan=False)+'\n').encode()
def equal(actual,expected,why):
    if type(actual) is not type(expected):raise ValueError(why)
    if isinstance(expected,dict):
        if actual.keys()!=expected.keys():raise ValueError(why)
        for key in expected:equal(actual[key],expected[key],why)
    elif isinstance(expected,(list,tuple)):
        if len(actual)!=len(expected):raise ValueError(why)
        for a,b in zip(actual,expected):equal(a,b,why)
    elif actual!=expected:raise ValueError(why)

def source_hashes(root,names):return {p:fingerprint(root/p) for p in dict.fromkeys(names)}

def inventory(data):
    result={}
    rows=decode(data)['shaders']
    if not isinstance(rows,list) or len(rows)>65536:raise ValueError('Inventory bound changed')
    for item in rows:
        name=item['file']
        if not isinstance(name,str):raise ValueError('Malformed inventory identity')
        if not NAME.fullmatch(name):continue
        if name in result:raise ValueError('Duplicate original inventory identity')
        result[name]=require_hash(item['sha256'])
    return result

def unique_name(identities,sha):
    matches=[n for n,h in identities.items() if h==sha]
    if len(matches)!=1:raise ValueError('Original must join exactly one inventory module')
    return matches[0]

def module_identity(data,stage):
    if stage not in (1,16):raise ValueError('Unsupported UV module stage')
    module=uv.Module(data);entries=[args for op,args in module.instructions if op==15]
    model=0 if stage==1 else 4
    if len(entries)!=1 or len(entries[0])<3 or entries[0][0]!=model or uv.literal(entries[0][2:])!='main':
        raise ValueError('Physical module bytes disagree with their exact stage/main entry')
    fnv=14695981039346656037
    for byte in data:fnv=((fnv^byte)*1099511628211)&((1<<64)-1)
    return {'stage':stage,'execution_model':model,'entry':'main','sha256':digest(data),'bytes':len(data),'fnv':str(fnv)}

def verify_module_triplet(modules,vertex,original,candidate):
    for declared,data,stage in zip(modules,(vertex,original,candidate),(1,16,16)):
        expected=module_identity(data,stage)
        equal({key:declared.get(key) for key in expected},expected,'Physical module content/FNV/stage correspondence changed')


def verify_candidate(original,candidate,report,validator,core,glsl,root):
    rebuilt=uv.verify_candidate(original,candidate,digest(original))
    for key,value in rebuilt.items():
        if key!='module_validation_performed':equal(report.get(key),value,'Candidate closed reconstruction/report mismatch: '+key)
    if report.get('module_validation_performed') is not True or report.get('module_validation_passed') is not True:
        raise ValueError('Candidate needs complete validated immutable snapshots')
    equal(report.get('constructor_source_sha256'),fingerprint(root/'tools/sampled_uv_probe.py'),'Stale UV constructor')
    equal(report.get('dependency_source_sha256'),source_hashes(root,UV_DEPENDENCIES),'Stale UV dependency')
    policy=Policy(core,glsl)
    for name,data in (('original',original),('candidate',candidate)):
        checked=policy.validate(data,validator,'vulkan1.2')
        if (checked.get('module_validation_passed') is not True or checked.get('validated_non_output_external_writes_excluded') is not True
                or checked.get('core_writes_by_storage')!={'Output':4} or checked.get('extended_writes_by_storage')!={}):
            raise ValueError('UV full validation/write exclusion refused')
        equal(report.get(name+'_validation'),checked,'Stale UV full validation receipt')
    return rebuilt


def verify_certificates(cert,independent,physical,audits,certificate_sha,original,candidate,root,validator_sha):
    for key,value in {'schema_version':1,'owned_uv_control_passed':True,'physical_readbacks':48,
            'total_readback_bytes':48*pixels.base.BYTE_COUNT,'validation_errors':0,'native_runtime_bytes_equal':True,
            'game_bundle_authorized':False,'game_rendered':False,'coverage_repaired':False,'xr_submission':False,
            'nonfinite_gpu_behavior_certified':False,'read_only_final_audits_equal_initial_saved_audits':True,
            'finalization_GPU_calls':0,'finalization_processes':0}.items():equal(cert.get(key),value,'GPU certificate scope/count/error: '+key)
    before,final=cert['physical_before_source_sha256'],cert['final_source_sha256']
    equal(final,source_hashes(root,pixels.SOURCES),'Stale final GPU source fingerprints')
    equal(set(before),set(final),'Physical source set differs')
    changed={k for k in before if before[k]!=final[k]}
    if changed!=set(COMPARATOR_LEDGER):raise ValueError('Unreviewed post-physical source change')
    equal(cert['post_physical_changes'],COMPARATOR_LEDGER,'Unreviewed comparator-only ledger')
    for name in before:
        require_hash(before[name]);require_hash(final[name])
        if name in COMPARATOR_LEDGER:equal({'physical_before':before[name],'final':final[name]},COMPARATOR_LEDGER[name],'Ledger endpoint mismatch')
    equal(physical['source_sha256'],before,'Physical/final source identities conflated')
    equal(physical['fixture_sha256'],cert['fixture_sha256'],'Fixture identity mismatch');require_hash(cert['fixture_sha256'])
    equal(physical['validator_sha256'],validator_sha,'Physical validator changed')
    equal(physical['code_arithmetic_budget'],pixels.CODE_ARITHMETIC_BUDGET,'UV arithmetic budget changed')
    equal(physical['constructor'],uv.verify_candidate(original,candidate,digest(original)),'Physical constructor/candidate identity changed')
    modules=physical['modules']
    if not isinstance(modules,list) or len(modules)!=3:raise ValueError('Physical module triplet missing')
    for m,stage,model in zip(modules,(1,16,16),(0,4,4)):
        equal([m['stage'],m['execution_model'],m['entry']],[stage,model,'main'],'Physical module stage changed');require_hash(m['sha256'])
    equal([modules[1]['sha256'],modules[1]['bytes'],modules[2]['sha256'],modules[2]['bytes']],
          [digest(original),len(original),digest(candidate),len(candidate)],'Physical original/candidate retagged')
    equal(set(audits),{'native','steam-runtime'},'Both original environment audits required')
    environments=cert['environments']
    if not isinstance(environments,list) or [e['environment'] for e in environments]!=['native','steam-runtime']:raise ValueError('GPU environment order/coverage changed')
    rows=[];names=[('descriptor_sets',n) for n in pixels.NAMES]+[('descriptor_buffer','B_repeat_offset')]
    for environment in environments:
        env=environment['environment'];equal({k:v for k,v in environment.items() if k!='environment'},audits[env],'Saved physical audit changed')
        a=audits[env]
        for key,value in {'owned_uv_control_passed':True,'total_readback_bytes':24*pixels.base.BYTE_COUNT,
             'validation_errors':0,'game_rendered':False,'game_pixel_cause_proven':False,'xr_submission':False,
             'nonfinite_gpu_behavior_certified':False,'universal_interpolation_bound_claimed':False}.items():equal(a.get(key),value,'Original GPU audit scope changed')
        if [(c['backend'],c['name']) for c in a['cases']]!=names:raise ValueError('Owned GPU cases/backend coverage incomplete')
        equal(set(a['manifest_sha256']),set(pixels.GROUPS),'GPU manifest set changed')
        for sha in a['manifest_sha256'].values():require_hash(sha)
        for c in a['cases']:
            for key in ('original_sha256','diagnostic_sha256','reference_sha256'):require_hash(c[key])
            equal(c['original_sha256'],c['reference_sha256'],'Original pixel oracle mismatch')
            count=pixels.base.WIDTH*pixels.base.HEIGHT//(4 if 'center_' in c['name'] else 1)
            equal([c['original_exact_bytes'],c['preserved_ba_exact_bytes'],c['encoded_pixels'],c['unchanged_load_pixels']],
                  [pixels.base.BYTE_COUNT,pixels.base.BYTE_COUNT//2,count,pixels.base.WIDTH*pixels.base.HEIGHT-count],'Pixel control counts changed')
            err=c['maximum_observed_r_code_error']
            if type(err) not in (int,float) or not math.isfinite(err) or not 0<=err<=1+pixels.CODE_ARITHMETIC_BUDGET:raise ValueError('UV numeric control failed')
            rows.append((env,c))
    equal([[c['original_sha256'],c['diagnostic_sha256']] for c in audits['native']['cases']],
          [[c['original_sha256'],c['diagnostic_sha256']] for c in audits['steam-runtime']['cases']],'Native/runtime physical hashes differ')
    for key,value in {'schema_version':1,'status':'PASS_read_only_independent_CPU_review_of_parent_GPU_outputs',
        'physical_proof_sha256':certificate_sha,'source_sha256':final,'physical_before_source_sha256':before,
        'post_physical_changes':cert['post_physical_changes'],'physical_readbacks':48,'readback_bytes':48*pixels.base.BYTE_COUNT,
        'validation_errors':0,'native_runtime_raw_bytes_equal':True,'GPU_calls_by_reviewer':0,'external_processes_forbidden':True,
        'game_executed':False,'game_admission':False,'XR_submission':False,'coverage_repaired':False,'hardware_transactions_proven':False,
        'finite_only':True,'nonfinite_certified':False,'opaque_descriptor_bytes_not_decoded':True}.items():equal(independent.get(key),value,'Independent GPU audit cross-link changed: '+key)
    require_hash(independent['reviewer_source_sha256'])
    equal(independent['UNORM_rule'],'floor_or_ceil_with_fixed0.005code_empirical_budget','Independent encoding scope changed')
    equal(independent['manifest_sha256'],[audits[e]['manifest_sha256'] for e in ('native','steam-runtime')],'Independent manifests differ')
    expected=[]
    for env,c in rows:expected.append(dict(environment=env,backend=c['backend'],name=c['name'],original_sha256=c['original_sha256'],diagnostic_sha256=c['diagnostic_sha256'],original_exact_bytes=c['original_exact_bytes'],BA_exact_bytes=c['preserved_ba_exact_bytes'],UV_or_overflow_pixels=c['encoded_pixels'],unchanged_LOAD_pixels=c['unchanged_load_pixels']))
    paired=independent['paired_results']
    if not isinstance(paired,list) or len(paired)!=24:raise ValueError('Independent pair coverage incomplete')
    for got,want in zip(paired,expected):
        equal({k:got.get(k) for k in want},want,'Independent pixels/cases differ')
        err=got['maximum_observed_code_error']
        if type(err) not in (int,float) or not math.isfinite(err) or not 0<=err<=1+pixels.CODE_ARITHMETIC_BUDGET:raise ValueError('Independent numeric audit failed')
    if not isinstance(independent['falsifications_refused'],list) or len(set(independent['falsifications_refused']))<4:raise ValueError('Independent falsification controls absent')
    return modules


def tree(directory):
    directory=safe_path(directory)
    if not directory.is_dir():raise ValueError('Baseline directory missing')
    result={};size=0
    for path in directory.rglob('*'):
        safe_path(path)
        if path.is_dir():continue
        sha=fingerprint(path);size+=path.stat().st_size
        if len(result)>=MAX_BASE_FILES or size>MAX_BASE_BYTES:raise ValueError('Baseline file/byte budget exceeded')
        result[path.relative_to(directory).as_posix()]=sha
    return result


def input_records(a):
    result={}
    for name in INPUTS:
        path=a.baseline/('bundle.json' if name=='baseline_bundle' else 'admission.json') if name.startswith('baseline_') else getattr(a,name)
        result[name]={'path':str(safe_path(path)),'sha256':require_hash(getattr(a,name+'_sha256'))}
    return result


def check_inputs(a):
    """Read-only preflight, including CPU SPIR-V validation; creates no bundle."""
    root=Path(__file__).resolve().parents[1]
    if a.policy!=POLICY or a.game_version!=probe.profile()['game_version']:raise ValueError('Current version and fixed monoscopic policy required')
    entry_source=fingerprint(Path(__file__));entry_dependencies=source_hashes(root,DEPENDENCIES)
    records=input_records(a);data={k:read(v['path'],v['sha256']) for k,v in records.items()}
    baseline=decode(data['baseline_bundle']);validated=base.validate_admission(safe_path(a.baseline),game_version=a.game_version,policy=POLICY)
    equal(validated,baseline,'Baseline changed during dedicated validation')
    entries=baseline['entries'];names=[e['file'] for e in entries]
    if len(entries)!=BASE_ENTRY_COUNT or len(set(names))!=len(names) or any(not NAME.fullmatch(n) for n in names):raise ValueError('Exact unchanged5379 baseline required')
    base_files=tree(a.baseline)
    if base_files.get('bundle.json')!=records['baseline_bundle']['sha256'] or base_files.get('admission.json')!=records['baseline_admission']['sha256']:raise ValueError('Baseline root fingerprints changed')
    identities=inventory(data['inventory']);name=unique_name(identities,records['original']['sha256'])
    if name in names:raise ValueError('UV target collides with an existing baseline entry')
    original_path=safe_path(a.original_directory)/name
    equal(read(original_path,records['original']['sha256']),data['original'],'Original directory and UV snapshot differ')
    report=decode(data['candidate_report'])
    rebuilt=verify_candidate(data['original'],data['candidate'],report,a.validator,data['grammar'],data['glsl_grammar'],root)
    cert=decode(data['certificate']);independent=decode(data['independent_certificate'])
    physical_path=Path(a.certificate).parent/'private-inputs.json';physical_bytes=read(physical_path,cert['physical_private_inputs_sha256']);physical=decode(physical_bytes)
    audit_bytes={env:read(Path(a.certificate).parent/(env+'-audit.json'),cert['original_per_environment_audits_sha256'][env]) for env in ('native','steam-runtime')}
    equal(set(cert['original_per_environment_audits_sha256']),set(audit_bytes),'Original audit ledger incomplete')
    modules=verify_certificates(cert,independent,physical,{e:decode(b) for e,b in audit_bytes.items()},records['certificate']['sha256'],data['original'],data['candidate'],root,records['validator']['sha256'])
    vertex_name=unique_name(identities,modules[0]['sha256']);vertex=read(safe_path(a.original_directory)/vertex_name,modules[0]['sha256'])
    equal(len(vertex),modules[0]['bytes'],'Original vertex identity size changed')
    verify_module_triplet(modules,vertex,data['original'],data['candidate'])
    evidence={
      'evidence/uv/original-inventory.json':data['inventory'],'evidence/uv/original.snapshot.spv':data['original'],
      'evidence/uv/vertex.snapshot.spv':vertex,'evidence/uv/candidate.spv':data['candidate'],
      'evidence/uv/candidate.json':data['candidate_report'],'evidence/uv/gpu-certificate.json':data['certificate'],
      'evidence/uv/independent-certificate.json':data['independent_certificate'],'evidence/uv/physical-private-inputs.json':physical_bytes,
      'evidence/uv/grammar.json':data['grammar'],'evidence/uv/glsl-grammar.json':data['glsl_grammar']}
    evidence.update({'evidence/uv/'+e+'-audit.json':b for e,b in audit_bytes.items()})
    target={'file':name,'original_sha256':digest(data['original']),'override_sha256':digest(data['candidate']),
            'candidate_report_sha256':digest(data['candidate_report']),'stage':16,'scope':'all_uses_of_original_fragment_content',
            'constructor':uv.CONSTRUCTOR,'all_original_stores_preserved':False,'changed_store_value_components':[0,1],'preserved_store_components':[2,3]}
    equal(fingerprint(Path(__file__)),entry_source,'Admission source changed during preflight')
    equal(source_hashes(root,DEPENDENCIES),entry_dependencies,'Admission dependencies changed during preflight')
    return dict(records=records,data=data,baseline=baseline,base_files=base_files,evidence=evidence,target=target,
                dependencies=entry_dependencies,admission_source_sha256=entry_source,reconstruction=rebuilt)


def manifest_for(baseline,target,bundle_sha,admission_sha):
    manifest=deepcopy(baseline);manifest.update(purpose=PURPOSE,policy=POLICY,gpu_fixture_passed=False,
        combined_bundle_gpu_validated=False,game_bundle_gpu_validated=False,targeted_sampled_uv_admitted=True,
        sampled_uv_target=target,baseline_admission={'directory':'baseline','bundle_sha256':bundle_sha,'admission_sha256':admission_sha,'entry_count':BASE_ENTRY_COUNT,'scope':'unchanged_baseline_with_original_admission'},
        game_executed=False,xr_submission=False,coverage_repaired=False,whole_scene_verified=False,common_scene_time_known=False)
    manifest['entries']=deepcopy(baseline['entries'])+[{k:target[k] for k in ('file','original_sha256','override_sha256')}]
    return manifest


def write(path,data):
    path.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
    with path.open('xb') as f:f.write(data)
    path.chmod(0o600)


def protect_output(a):
    output=safe_path(a.output);root=Path(__file__).resolve().parents[1]
    protected=[safe_path(p) for p in (a.baseline,a.original_directory,a.original.parent,a.candidate.parent,a.candidate_report.parent,a.certificate.parent,a.independent_certificate.parent,root)]
    if output.exists() or any(output==p or p in output.parents for p in protected):raise ValueError('Fresh output outside protected original/base/candidate/evidence/repository paths required')
    if any((p/'Diablo IV.exe').exists() or (p/'Battle.net.exe').exists() for p in (output,*output.parents)):raise ValueError('Output must remain outside game/launcher paths')
    return output


def prepare(a):
    output=protect_output(a);pre=check_inputs(a)
    # All semantic refusals precede publication. Every copied read is rehashed.
    output.mkdir(mode=0o700,parents=True,exist_ok=False)
    for name,sha in pre['base_files'].items():write(output/'baseline'/name,read(a.baseline/name,sha))
    for e in pre['baseline']['entries']:write(output/'overrides'/e['file'],read(a.baseline/'overrides'/e['file'],e['override_sha256']))
    for name,sha in pre['base_files'].items():
        if name.startswith('evidence/'):write(output/name,read(a.baseline/name,sha))
    for name,data in pre['evidence'].items():write(output/name,data)
    write(output/'overrides'/pre['target']['file'],pre['data']['candidate'])
    manifest=manifest_for(pre['baseline'],pre['target'],a.baseline_bundle_sha256,a.baseline_admission_sha256);body=encoded(manifest);write(output/'bundle.json',body)
    receipt={'schema_version':1,'policy':POLICY,'purpose':PURPOSE,'game_version':a.game_version,
        'targeted_sampled_uv_admitted':True,'baseline_entries':BASE_ENTRY_COUNT,'added_candidates':1,'total_entries':BASE_ENTRY_COUNT+1,
        'bundle_sha256':digest(body),'input_records':pre['records'],'baseline_files_sha256':pre['base_files'],
        'evidence_sha256':{k:digest(b) for k,b in pre['evidence'].items()},'sampled_uv_target':pre['target'],
        'admission_source_sha256':pre['admission_source_sha256'],'dependency_source_sha256':pre['dependencies'],
        'game_executed':False,'xr_submission':False,'coverage_repaired':False,'combined_bundle_gpu_validated':False,
        'game_bundle_gpu_validated':False,'limits':LIMITS}
    root=Path(__file__).resolve().parents[1]
    equal(fingerprint(Path(__file__)),pre['admission_source_sha256'],'Admission source changed during copying')
    equal(source_hashes(root,DEPENDENCIES),pre['dependencies'],'Admission dependencies changed during copying')
    for record in pre['records'].values():read(record['path'],record['sha256'])
    write(output/'admission.json',encoded(receipt));validate_admission(output,game_version=a.game_version,policy=a.policy)
    return receipt


def validate_admission(directory,*,game_version,policy=POLICY):
    """Revalidate pinned live inputs AND copied evidence; no selector extension."""
    directory=safe_path(directory);root=Path(__file__).resolve().parents[1]
    if policy!=POLICY or game_version!=probe.profile()['game_version']:raise ValueError('Unsupported UV admission policy/version')
    receipt=decode(read(directory/'admission.json',fingerprint(directory/'admission.json')))
    for key,value in {'schema_version':1,'policy':POLICY,'purpose':PURPOSE,'game_version':game_version,
        'targeted_sampled_uv_admitted':True,'baseline_entries':BASE_ENTRY_COUNT,'added_candidates':1,'total_entries':BASE_ENTRY_COUNT+1,
        'admission_source_sha256':fingerprint(Path(__file__)),'dependency_source_sha256':source_hashes(root,DEPENDENCIES),
        'game_executed':False,'xr_submission':False,'coverage_repaired':False,'combined_bundle_gpu_validated':False,'game_bundle_gpu_validated':False,'limits':LIMITS}.items():equal(receipt.get(key),value,'UV admission scope/source changed: '+key)
    records=receipt['input_records'];equal(set(records),set(INPUTS),'Missing explicitly pinned inputs')
    data={k:read(v['path'],v['sha256']) for k,v in records.items()}
    base_files=receipt['baseline_files_sha256']
    equal(tree(Path(records['baseline_bundle']['path']).parent),base_files,'Live baseline bytes/evidence changed')
    equal(tree(directory/'baseline'),base_files,'Copied baseline bytes/evidence changed')
    equal(read(directory/'baseline/bundle.json',records['baseline_bundle']['sha256']),data['baseline_bundle'],'Baseline manifest copy changed')
    equal(read(directory/'baseline/admission.json',records['baseline_admission']['sha256']),data['baseline_admission'],'Baseline admission copy changed')
    baseline=base.validate_admission(directory/'baseline',game_version=game_version,policy=POLICY)
    if len(baseline['entries'])!=BASE_ENTRY_COUNT:raise ValueError('Copied baseline count changed')
    required={'evidence/uv/'+n for n in ('original-inventory.json','original.snapshot.spv','vertex.snapshot.spv','candidate.spv','candidate.json','gpu-certificate.json','independent-certificate.json','physical-private-inputs.json','grammar.json','glsl-grammar.json','native-audit.json','steam-runtime-audit.json')}
    equal(set(receipt['evidence_sha256']),required,'Incomplete UV evidence')
    evidence={n:read(directory/n,sha) for n,sha in receipt['evidence_sha256'].items()}
    for key,name in (('inventory','original-inventory.json'),('original','original.snapshot.spv'),('candidate','candidate.spv'),('candidate_report','candidate.json'),('certificate','gpu-certificate.json'),('independent_certificate','independent-certificate.json'),('grammar','grammar.json'),('glsl_grammar','glsl-grammar.json')):equal(evidence['evidence/uv/'+name],data[key],'Copied UV evidence differs from explicitly pinned input')
    identities=inventory(data['inventory']);target=receipt['sampled_uv_target'];name=unique_name(identities,digest(data['original']))
    expected_target={'file':name,'original_sha256':digest(data['original']),'override_sha256':digest(data['candidate']),'candidate_report_sha256':digest(data['candidate_report']),'stage':16,'scope':'all_uses_of_original_fragment_content','constructor':uv.CONSTRUCTOR,'all_original_stores_preserved':False,'changed_store_value_components':[0,1],'preserved_store_components':[2,3]}
    equal(target,expected_target,'Target scope/identity retagged')
    if name in {e['file'] for e in baseline['entries']}:raise ValueError('Target baseline collision')
    verify_candidate(data['original'],data['candidate'],decode(data['candidate_report']),Path(records['validator']['path']),data['grammar'],data['glsl_grammar'],root)
    cert=decode(data['certificate']);physical_bytes=evidence['evidence/uv/physical-private-inputs.json'];equal(digest(physical_bytes),cert['physical_private_inputs_sha256'],'Copied physical inputs changed')
    audits={e:decode(evidence['evidence/uv/'+e+'-audit.json']) for e in ('native','steam-runtime')}
    for e in audits:equal(digest(evidence['evidence/uv/'+e+'-audit.json']),cert['original_per_environment_audits_sha256'][e],'Copied original environment audit changed')
    modules=verify_certificates(cert,decode(data['independent_certificate']),decode(physical_bytes),audits,records['certificate']['sha256'],data['original'],data['candidate'],root,records['validator']['sha256'])
    equal(digest(evidence['evidence/uv/vertex.snapshot.spv']),modules[0]['sha256'],'Copied context vertex changed');unique_name(identities,modules[0]['sha256'])
    verify_module_triplet(modules,evidence['evidence/uv/vertex.snapshot.spv'],data['original'],data['candidate'])
    manifest=decode(read(directory/'bundle.json',receipt['bundle_sha256']))
    equal(manifest,manifest_for(baseline,target,records['baseline_bundle']['sha256'],records['baseline_admission']['sha256']),'Prepared bundle transport/base/target changed')
    names=[]
    for e in manifest['entries']:
        if not NAME.fullmatch(e['file']):raise ValueError('Unsafe override name')
        require_hash(e['original_sha256']);read(directory/'overrides'/e['file'],e['override_sha256']);names.append(e['file'])
    equal({p.name for p in (directory/'overrides').iterdir()},set(names),'Unlisted override');equal(len(set(names)),BASE_ENTRY_COUNT+1,'Duplicate target')
    for name,sha in base_files.items():
        if name.startswith('evidence/'):read(directory/name,sha)
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('baseline','original-directory','inventory','original','candidate','candidate-report','certificate','independent-certificate','validator','grammar','glsl-grammar','output'):p.add_argument('--'+name,type=Path,required=True)
    for name in INPUTS:p.add_argument('--'+name.replace('_','-')+'-sha256',required=True)
    p.add_argument('--policy',choices=[POLICY],required=True);p.add_argument('--game-version',required=True)
    print(json.dumps(prepare(p.parse_args()),indent=2))
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TypeError,subprocess.SubprocessError) as error:raise SystemExit(str(error))
