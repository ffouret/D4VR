#!/usr/bin/env python3
"""Prepare one private fixed-monoscopic experiment; never launch or install it.

Copy an unchanged certified 5377-entry baseline and add exactly the two explicit
post-depth candidates. The result has a targeted admission, not a new aggregate
GPU certificate. Original game files and the baseline are read-only inputs.
"""
from __future__ import annotations
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import probe
from camera_capture import validate_bundle
from post_depth_candidate import verify_certificate, verify_helper_certificate
from projection_depth_suffix import validated
from shader_write_contract import Policy, assess
from stereo_helper import verify_projection_only
from spirv_probe import build

POLICY = 'fixed_monoscopic_no_xr'
BASE_ENTRY_COUNT = 5377
KINDS = {'bias':'affine_bias_over_w', 'clamp':'min_z_w'}
NAME = re.compile(r'[0-9a-f]{16}\.spv')
HASH = re.compile(r'[0-9a-f]{64}')
UNPINNED = object()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require_hash(value):
    if not isinstance(value,str) or not HASH.fullmatch(value):
        raise ValueError('A complete lowercase SHA256 fingerprint is required')
    return value


def read(path, expected=UNPINNED):
    if expected is not UNPINNED:
        require_hash(expected)
    if path.is_symlink() or not path.is_file():
        raise ValueError('Admission requires a regular, non-symlink input: '+str(path))
    data = path.read_bytes()
    if expected is not UNPINNED and digest(data) != expected:
        raise ValueError('Admission input fingerprint changed: '+str(path))
    return data


def write(path, data):
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    with path.open('xb') as stream:
        stream.write(data)
    path.chmod(0o600)


def json_bytes(value):
    return (json.dumps(value, indent=2)+'\n').encode()


def prepare(a):
    root = Path(__file__).resolve().parents[1]
    if a.policy != POLICY or a.game_version != probe.profile()['game_version']:
        raise ValueError('Explicit fixed-monoscopic policy and current game version required')
    output = a.output.resolve()
    protected = [a.baseline.resolve(), a.original_directory.resolve(),
                 a.bias_candidate.resolve(), a.clamp_candidate.resolve()]
    if output.exists() or a.output.is_symlink() or any(output == p or p in output.parents for p in protected):
        raise ValueError('Use a new output outside all baseline/original/candidate inputs')
    if any((p/'Diablo IV.exe').exists() or (p/'Battle.net.exe').exists() for p in [output,*output.parents]):
        raise ValueError('Admission output must be outside the game and launcher directories')
    baseline_bytes = read(a.baseline/'bundle.json')
    baseline = validate_bundle(a.baseline)
    if baseline != json.loads(baseline_bytes) or baseline.get('game_version') != a.game_version:
        raise ValueError('Baseline manifest/version changed during validation')
    entries = baseline.get('entries', [])
    names = [e['file'] for e in entries]
    if len(entries) != BASE_ENTRY_COUNT or len(set(names)) != len(names) or any(not NAME.fullmatch(n) for n in names):
        raise ValueError('Require the exact distinct 5377-entry baseline')
    inventory_bytes = read(a.inventory)
    identities = {}
    for entry in json.loads(inventory_bytes)['shaders']:
        name = entry['file']
        if not NAME.fullmatch(name):
            continue
        if name in identities:
            raise ValueError('Ambiguous original inventory filename')
        identities[name] = require_hash(entry['sha256'])
    for entry in entries:
        if identities.get(entry['file']) != entry.get('original_sha256'):
            raise ValueError('Baseline original differs from the pinned inventory')
        read(a.original_directory/entry['file'], entry['original_sha256'])
        read(a.baseline/'overrides'/entry['file'], entry['override_sha256'])
    gpu_bytes, helper_certificate_bytes = read(a.certificate), read(a.helper_certificate)
    gpu, helper_certificate = json.loads(gpu_bytes), json.loads(helper_certificate_bytes)
    core, glsl = read(a.grammar), read(a.glsl_grammar)
    policy = Policy(core, glsl)
    targets = []
    target_payloads = {}
    evidence = {'evidence/baseline.bundle.json':baseline_bytes,
                'evidence/original-inventory.json':inventory_bytes,
                'evidence/gpu-certificate.json':gpu_bytes,
                'evidence/helper-certificate.json':helper_certificate_bytes}
    for label, directory in [('bias',a.bias_candidate), ('clamp',a.clamp_candidate)]:
        receipt_bytes = read(directory/'candidate.json')
        receipt = json.loads(receipt_bytes)
        if any(receipt.get(k) is not False for k in ('game_bundle_authorized','game_executed','xr_submission','coverage_repaired')):
            raise ValueError('Candidate must retain its unexecuted/no-XR/no-coverage status')
        if receipt.get('constructor_source_sha256') != probe.digest(root/'tools/post_depth_candidate.py'):
            raise ValueError('Candidate constructor source certificate changed')
        if receipt.get('certificate_sha256') != digest(gpu_bytes) or receipt.get('helper_certificate_sha256') != digest(helper_certificate_bytes):
            raise ValueError('Candidate certificate identity mismatch')
        if any(not isinstance(receipt.get(k),str) or not re.fullmatch(r'[0-9a-f]{64}',receipt[k])
               for k in ('original_sha256','helper_sha256','candidate_sha256')):
            raise ValueError('Candidate snapshot fingerprints are required')
        source = read(directory/'original.snapshot.spv', receipt.get('original_sha256'))
        helper = read(directory/'helper.snapshot.spv', receipt.get('helper_sha256'))
        candidate = read(directory/'candidate.spv', receipt.get('candidate_sha256'))
        matches = [name for name, value in identities.items() if value == digest(source)]
        if len(matches) != 1 or matches[0] in names or any(t['file'] == matches[0] for t in targets):
            raise ValueError('Each target must identify one new distinct original inventory module')
        name = matches[0]
        if read(a.original_directory/name, digest(source)) != source:
            raise ValueError('Original snapshot differs from the pinned original directory')
        verify_helper_certificate(helper_certificate, digest(helper), root)
        certificate_check = verify_certificate(gpu, directory/'helper.snapshot.spv', root, helper_digest=digest(helper))
        helper_contract = verify_projection_only(helper)
        contract = validated(source, core, glsl, a.tools/'spirv-val')
        if (contract.get('suffix_kind') != KINDS[label]
                or receipt.get('closed_original_contract',{}).get('shader_sha256') != digest(source)
                or receipt.get('closed_original_contract',{}).get('suffix_kind') != KINDS[label]
                or receipt.get('original_stores_and_suffix_arithmetic_preserved_before_link') is not True
                or receipt.get('projection_application') != 'canonical_input_loads'
                or receipt.get('clip_output_units') != 'source_VIEW_clip_units_after_metric_conversion'):
            raise ValueError('Candidate closed suffix/projection contract mismatch')
        before, after = assess(source, policy.glsl), policy.validate(candidate, a.tools/'spirv-val','vulkan1.2')
        if (not after.get('validated_non_output_external_writes_excluded')
                or after.get('core_writes_by_storage',{}).get('Output',0) != before.get('core_writes_by_storage',{}).get('Output',0)):
            raise ValueError('Candidate effect/output contract changed')
        # Effects and an unchanged store count also admit an uninstrumented
        # original. Reproduce the exact closed constructor, CPU tools only.
        with tempfile.TemporaryDirectory(prefix='post-depth-admission-') as temporary:
            directory_snapshot = Path(temporary)
            source_snapshot, helper_snapshot = directory_snapshot/'original.spv', directory_snapshot/'helper.spv'
            write(source_snapshot,source)
            write(helper_snapshot,helper)
            rebuilt = directory_snapshot/'rebuilt.spv'
            build(source_snapshot,helper_snapshot,rebuilt,a.tools,3,0,72,stereo=True,
                  helper_no_telemetry=True,projection_only_helper=True,projection_input=True)
            if read(rebuilt) != candidate:
                raise ValueError('Candidate differs from deterministic snapshot reconstruction')
        targets.append({'file':name,'suffix_kind':KINDS[label], 'original_sha256':digest(source),
                        'override_sha256':digest(candidate),'helper_sha256':digest(helper),
                        'candidate_receipt_sha256':digest(receipt_bytes),
                        'closed_suffix_revalidated':True,'write_contract_revalidated':True,
                        'owned_gpu_certificate':certificate_check,
                        'helper_abi_verified':helper_contract.get('projection_only_abi_verified') is True})
        target_payloads[name] = candidate
        evidence['evidence/'+label+'/candidate.json'] = receipt_bytes
        evidence['evidence/'+label+'/original.snapshot.spv'] = source
        evidence['evidence/'+label+'/helper.snapshot.spv'] = helper
    output.mkdir(mode=0o700, parents=True)
    copied = deepcopy(entries)
    for entry in entries:
        data = read(a.baseline/'overrides'/entry['file'], entry['override_sha256'])
        write(output/'overrides'/entry['file'],data)
    for target in targets:
        write(output/'overrides'/target['file'],target_payloads[target['file']])
        copied.append({k:target[k] for k in ('file','original_sha256','override_sha256')})
    for name, data in evidence.items():
        write(output/name,data)
    # Transport fields retain their existing meanings. Evidence for the 5377
    # baseline is isolated; none of its GPU certificate becomes a 5379 claim.
    transport = ('game_version','proton_path','runtime_module','runtime_module_sha256',
                 'helper_sha256','uniform_descriptor_set','uniform_bindings','matrix_byte_offsets',
                 'eye_state_abi','eye_state_specialization_id','helper_no_telemetry',
                 'projection_only_helper','stereo_telemetry','projection_application','source_depth_packet_version')
    manifest = {key:deepcopy(baseline[key]) for key in transport if key in baseline}
    manifest.update(schema_version=2, purpose='targeted_post_depth_fixed_monoscopic_diagnostic',
                    policy=POLICY, entries=copied, gpu_fixture_passed=False,
                    combined_bundle_gpu_validated=False, targeted_fixed_monoscopic_admitted=True,
                    heterogeneous_target_helpers=targets,
                    baseline_evidence={'manifest_sha256':digest(baseline_bytes),'entry_count':BASE_ENTRY_COUNT,
                                       'helper_sha256':baseline.get('helper_sha256'),
                                       'gpu_fixture_passed':baseline['gpu_fixture_passed'],
                                       'scope':'unchanged_baseline_entries_only',
                                       'manifest_snapshot':'evidence/baseline.bundle.json'},
                    game_executed=False,xr_submission=False,coverage_repaired=False,
                    whole_scene_verified=False,common_scene_time_known=False)
    bundle_bytes = json_bytes(manifest)
    write(output/'bundle.json',bundle_bytes)
    admission = {'schema_version':1,'policy':POLICY,'game_version':a.game_version,
                 'targeted_fixed_monoscopic_admitted':True,'game_executed':False,
                 'xr_submission':False,'coverage_repaired':False,
                 'combined_bundle_gpu_validated':False,'bundle_sha256':digest(bundle_bytes),
                 'baseline_entries':BASE_ENTRY_COUNT,'added_candidates':2,'total_entries':len(copied),
                 'baseline_manifest_sha256':digest(baseline_bytes),
                 'evidence_sha256':{name:digest(data) for name,data in evidence.items()},
                 'admission_source_sha256':probe.digest(Path(__file__)),
                 'limits':['One fixed monoscopic diagnostic, no XR or stereo A/B claim.',
                           'Reference/wide/return observations are successive; old-session comparisons are intersession.',
                           'The unchanged baseline and owned suffix controls are separate evidence, not a GPU certificate for all5379 entries.',
                           'No pixel-exact causal comparison, scene-completeness or hole-cause claim.']}
    write(output/'admission.json',json_bytes(admission))
    validate_admission(output,game_version=a.game_version)
    return admission


def validate_admission(directory, *, game_version, policy=POLICY):
    """Read-only integrity gate for the explicitly prepared private launcher."""
    if policy != POLICY:
        raise ValueError('Unsupported targeted admission policy')
    admission = json.loads(read(directory/'admission.json'))
    if (admission.get('policy') != POLICY or admission.get('game_version') != game_version
            or admission.get('targeted_fixed_monoscopic_admitted') is not True
            or any(admission.get(k) is not False for k in ('game_executed','xr_submission','coverage_repaired','combined_bundle_gpu_validated'))
            or admission.get('baseline_entries') != BASE_ENTRY_COUNT or admission.get('added_candidates') != 2
            or admission.get('total_entries') != BASE_ENTRY_COUNT+2
            or admission.get('admission_source_sha256') != probe.digest(Path(__file__))):
        raise ValueError('Targeted admission scope/version/count/source mismatch')
    manifest = json.loads(read(directory/'bundle.json',admission['bundle_sha256']))
    if (manifest.get('policy') != POLICY or manifest.get('game_version') != game_version
            or manifest.get('purpose') != 'targeted_post_depth_fixed_monoscopic_diagnostic'
            or manifest.get('gpu_fixture_passed') is not False
            or manifest.get('targeted_fixed_monoscopic_admitted') is not True
            or any(manifest.get(k) is not False for k in ('game_executed','xr_submission','coverage_repaired'))
            or manifest.get('combined_bundle_gpu_validated') is not False
            or len(manifest.get('entries',[])) != BASE_ENTRY_COUNT+2
            or len(manifest.get('heterogeneous_target_helpers',[])) != 2):
        raise ValueError('Prepared bundle is not the explicitly targeted diagnostic')
    required_evidence = {'evidence/baseline.bundle.json','evidence/original-inventory.json',
                         'evidence/gpu-certificate.json','evidence/helper-certificate.json'}
    required_evidence.update('evidence/'+label+'/'+name for label in KINDS
                             for name in ('candidate.json','original.snapshot.spv','helper.snapshot.spv'))
    if set(admission.get('evidence_sha256',{})) != required_evidence:
        raise ValueError('Prepared admission evidence is incomplete')
    for name, expected in admission['evidence_sha256'].items():
        if Path(name).is_absolute() or '..' in Path(name).parts:
            raise ValueError('Invalid evidence snapshot path')
        read(directory/name,expected)
    baseline = json.loads(read(directory/'evidence/baseline.bundle.json',admission['baseline_manifest_sha256']))
    if (manifest['entries'][:BASE_ENTRY_COUNT] != baseline.get('entries')
            or manifest.get('helper_sha256') != baseline.get('helper_sha256')
            or manifest.get('baseline_evidence',{}).get('manifest_sha256') != admission['baseline_manifest_sha256']):
        raise ValueError('Prepared baseline lineage changed')
    require_hash(manifest['helper_sha256'])
    inventory = json.loads(read(directory/'evidence/original-inventory.json'))
    identities = {e['file']:require_hash(e['sha256']) for e in inventory['shaders'] if NAME.fullmatch(e['file'])}
    gpu_bytes, helper_certificate_bytes = read(directory/'evidence/gpu-certificate.json'), read(directory/'evidence/helper-certificate.json')
    gpu, helper_certificate = json.loads(gpu_bytes), json.loads(helper_certificate_bytes)
    root = Path(__file__).resolve().parents[1]
    for index,(label,kind) in enumerate(KINDS.items()):
        target = manifest['heterogeneous_target_helpers'][index]
        entry = manifest['entries'][BASE_ENTRY_COUNT+index]
        receipt_bytes = read(directory/'evidence'/label/'candidate.json',target.get('candidate_receipt_sha256'))
        receipt = json.loads(receipt_bytes)
        source = read(directory/'evidence'/label/'original.snapshot.spv',receipt.get('original_sha256'))
        helper = read(directory/'evidence'/label/'helper.snapshot.spv',receipt.get('helper_sha256'))
        if (target.get('suffix_kind') != kind or target.get('file') != entry['file']
                or any(target.get(k) != entry[k] for k in ('original_sha256','override_sha256'))
                or target.get('helper_sha256') != digest(helper)
                or receipt.get('candidate_sha256') != entry['override_sha256']
                or receipt.get('original_sha256') != entry['original_sha256']
                or identities.get(entry['file']) != digest(source)
                or receipt.get('certificate_sha256') != digest(gpu_bytes)
                or receipt.get('helper_certificate_sha256') != digest(helper_certificate_bytes)
                or receipt.get('constructor_source_sha256') != probe.digest(root/'tools/post_depth_candidate.py')
                or any(receipt.get(k) is not False for k in ('game_bundle_authorized','game_executed','xr_submission','coverage_repaired'))):
            raise ValueError('Prepared target/receipt/entry/helper correspondence changed')
        verify_helper_certificate(helper_certificate,digest(helper),root)
        verify_certificate(gpu,directory/'evidence'/label/'helper.snapshot.spv',root,helper_digest=digest(helper))
    names = [e['file'] for e in manifest['entries']]
    if len(set(names)) != len(names) or any(not NAME.fullmatch(name) for name in names):
        raise ValueError('Prepared override identity mismatch')
    for entry in manifest['entries']:
        require_hash(entry['original_sha256'])
        if identities.get(entry['file']) != entry['original_sha256']:
            raise ValueError('Prepared original inventory correspondence changed')
        read(directory/'overrides'/entry['file'],entry['override_sha256'])
    actual = {p.name for p in (directory/'overrides').iterdir()}
    if actual != set(names):
        raise ValueError('Prepared override directory contains unlisted files')
    if probe.digest(Path(manifest['runtime_module'])) != manifest['runtime_module_sha256']:
        raise ValueError('Pinned runtime changed after targeted admission')
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('baseline','bias-candidate','clamp-candidate','original-directory','inventory',
                 'certificate','helper-certificate','tools','grammar','glsl-grammar','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--policy',choices=[POLICY],required=True)
    parser.add_argument('--game-version',required=True)
    print(json.dumps(prepare(parser.parse_args()),indent=2))


if __name__=='__main__':
    try:
        main()
    except (OSError,ValueError,KeyError,TypeError,subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
