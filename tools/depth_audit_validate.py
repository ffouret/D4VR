"""Real WSI control: observe incompatible original calls without changing them."""
import argparse
import json
import subprocess
from pathlib import Path
import paired_capture
import probe


def validate(a):
    if a.output.exists():raise ValueError('Use a new private directory')
    a.output.mkdir(mode=0o700, parents=True)
    def run(command, timeout=30):
        return subprocess.run(paired_capture.host_command()+command, capture_output=True,
                              text=True, check=True, timeout=timeout).stdout
    unit = json.loads(run([str(a.unit.resolve())]))
    if unit.get('depth_audit_tests_passed') is not True:raise ValueError('CPU controls absent')
    run([str(a.forwarding.resolve())])
    transfer_unit = json.loads(run([str(a.transfer_unit.resolve())])) if getattr(a,"transfers",False) else None
    if transfer_unit and not transfer_unit.get("depth_transfer_tests_passed"):raise ValueError("Transfer CPU controls absent")
    transfers = bool(transfer_unit)
    root = Path(__file__).resolve().parents[1];shaders=[]
    for ext in ('vert', 'frag'):
        path = (a.output/('present.'+ext+'.spv')).resolve()
        run([str(a.compiler.resolve()), '-V', '--target-env', 'vulkan1.2',
             str(root/('native/tests/present_eye.'+ext)), '-o', str(path)])
        shaders.append(str(path))
    cases=[]
    for runtime in (False, True) if a.runtime else (False,):
        original=None
        original_depth=None
        for enabled in (False, True):
            name=('audit-' if enabled else 'baseline-')+('runtime' if runtime else 'native')
            directory=(a.output/name).resolve();directory.mkdir(mode=0o700)
            command=paired_capture.host_command()+['/usr/bin/env']
            for key in ('VK_LAYER_PATH','D4VR_TRACE_XR_BINDING','D4VR_COOPERATIVE_XR_DIAGNOSTIC',
                        'D4VR_LAYER_REPLAY_POC','D4VR_FRAME_TRACE','D4VR_PAIR_LEFT_FILE',
                        'D4VR_PAIR_RIGHT_FILE','D4VR_EYE_STATE_FILE','D4VR_SEQUENTIAL_CAPTURE_DIR',
                        'D4VR_POSE_CAPTURE_DIAGNOSTIC','D4VR_DIAGNOSTIC_VIEW_HOLD',
                        'D4VR_GPU_CAMERA_UPDATES','D4VR_GPU_CAPTURE_DRY_RUN','D4VR_DEPTH_TRANSFER_FIXTURE'):
                command+=['-u',key]
            command += ['VK_INSTANCE_LAYERS=VK_LAYER_D4VR_trace:VK_LAYER_KHRONOS_validation',
                        'VK_ADD_LAYER_PATH='+str(a.layer.resolve())+':'+str(a.validation_layer.resolve()),
                        'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
                        'VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer','D4VR_TRACE_DIR='+str(directory),
                        'D4VR_DEPTH_LAYER_FIXTURE=1','D4VR_DEPTH_AUDIT_FIXTURE=1',
                        'D4VR_DEPTH_AUDIT='+('1' if enabled else '0'),
                        # Conflicting option must never activate the reader in
                        # audit mode or reject the valid original early signal.
                        'D4VR_DEPTH_READBACK_DIAGNOSTIC='+('1' if enabled else '0')]
            if transfers:command += ['D4VR_DEPTH_TRANSFER_FIXTURE=1']
            if runtime:command += [str(a.runtime.resolve()),'--batch','--']
            command += [str(a.fixture.resolve()),*shaders,str(directory)]
            p=subprocess.run(command,capture_output=True,text=True,timeout=40)
            log=p.stdout+p.stderr;(directory/'run.log').write_text(log)
            if p.returncode or any(v in log for v in ('VUID-','Validation Error','SYNC-HAZARD-')):
                raise ValueError(name+': inspect private log')
            for layer in ('VK_LAYER_D4VR_trace','VK_LAYER_KHRONOS_validation'):
                if 'Insert instance layer "'+layer+'"' not in log or 'Inserted device layer "'+layer+'"' not in log:
                    raise ValueError('Audit/validation layer load not verified')
            rows=[json.loads(line) for path in directory.glob('vulkan-*.jsonl')
                  for line in path.read_text().splitlines()]
            if any(r.get('event') in ('depth_error','depth_init_refused','depth_submit_refused','depth_export','depth_audit_unavailable') for r in rows):
                raise ValueError('Observer rejected calls or activated GPU readback')
            end=[r for r in rows if r.get('event')=='device_destroy']
            if (len(end)!=1 or end[0]['present_calls']!=64 or end[0]['submissions']!=(67 if transfers else 65)
                    or end[0]['submitted_command_buffers']!=(66 if transfers else 64) or end[0]['commands']['draw']!=128):
                raise ValueError('Original commands duplicated/lost')
            pixels={n:(directory/n).read_bytes() for n in
                    ('mono-before.pixels','left-reference.pixels','right-reference.pixels','mono-after.pixels')}
            if any(len(raw)!=256*256*4 for raw in pixels.values()) or len(set(pixels['mono-before.pixels']))<2:
                raise ValueError('Original reference missing/empty')
            if transfers:
                depth=b''.join((directory/('transfer-original-'+str(i)+'.pixels')).read_bytes() for i in (1,2))
                if depth!=bytes.fromhex('0000803e')*384:raise ValueError('Original known D32 depth changed')
                if enabled and depth!=original_depth:raise ValueError('Audit changed original transfer pixels')
                if not enabled:original_depth=depth
            reports=sorted([json.loads(path.read_text()) for path in directory.glob('depth-audit-*.json')],key=lambda d:d['report_sequence'])
            if enabled:
                if pixels!=original or not 3<=len(reports)<=32:raise ValueError('Observer changed image or report budget')
                if not all(any(r['external_request']==i and not r['final'] for r in reports) for i in (1,2)):
                    raise ValueError('On-request reports absent before teardown')
                for r in reports:
                    d=r['audit']
                    if (r['other_injection_modes_configured'] or r['frame_trace_enabled'] or not d['observation_only']
                            or d['gpu_commands_inserted']!=0 or d['application_calls_rejected']!=0 or d['faulted']
                            or d['scope_contract_observed_compatible'] or d['registry_overflow'] or d['scope_overflow']
                            or d['histogram_overflow'] or d['input_truncated']):
                        raise ValueError('Unsafe/incomplete audit claim')
                d=reports[-1]['audit']
                if (d['submissions'],d['legacy_calls'],d['submit2_calls'],d['successful_calls'],d['failed_calls'],
                    d['unsupported_attempts'],d['successful_unsupported']) != ((67,64,3,67,0,3,3) if transfers else (65,64,1,65,0,1,1)):
                    raise ValueError('Original unsupported call not observed/forwarded exactly')
                if d['submit2_signal_masks']!=([{'value':512,'count':1},{'value':21474836480,'count':2}] if transfers else [{'value':512,'count':1}]) or d['legacy_wait_masks']!=[{'value':65536,'count':64}]:
                    raise ValueError('Original semaphore masks altered')
                if transfers:
                    tr=d['original_transfers']
                    if (tr['api_calls']!=[1,1,6,1,65,1] or tr['depth_aspect_regions']!=7
                            or tr['input_truncated'] or tr['overflow'] or tr['unknown_source_calls'] or tr['unknown_command_calls']
                            or tr['recordings_with_multiple_successful_submissions']!=1
                            or tr['repeated_successful_recording_submissions']!=1 or tr['repeated_submit_attempts']!=1
                            or len(tr['rows'])!=7 or sum(row['successful_submit_observations'] for row in tr['rows'])!=14
                            or sum(row['repeat_success_observations'] for row in tr['rows'])!=7
                            or any(row['description']['source_format']!=126 or row['description']['command_flags']!=0
                                   or row['description']['source_layout']!=1 or row['last_submitted'][2]==0 for row in tr['rows'])):
                        raise ValueError('Original transfer/reuse metadata incorrect')
                scopes=d['scopes']
                if (len(scopes)!=4 or sum(s['successful_submit_observations'] for s in scopes)!=64
                        or any(s['description']['rendering_candidate']!=1 or s['description']['image_format']!=129
                               or s['description']['width']!=256 or s['description']['layout']!=1 for s in scopes)):
                    raise ValueError('Candidate/command provenance missing')
                # Generation/recording fields identify CPU observations, not
                # synthetic frame times or GPU completion.
                if any(s['last_submitted'][0:2]!=s['last_recorded'][0:2] or not s['last_submission'] for s in scopes):
                    raise ValueError('Submission receipts changed resource identity')
            else:
                original=pixels
                if reports:raise ValueError('Disabled observer emitted reports')
            cases.append({'environment':name,'reports':len(reports),'present_calls':64,
                          'original_calls':67 if transfers else 65,'unchanged_color_bytes':sum(map(len,pixels.values())) if enabled else 0,
                          'original_early_signal_succeeded':True,'gpu_depth_reads':0,'validation_errors':0,
                          'original_clear_depth_bytes_checked':1536 if transfers else 0,
                          'original_reused_recording_verified':transfers,
                          'on_request_reports_before_teardown':enabled,
                          'scope_contract_incompatible_observed':enabled})
    sources=['native/vulkan/depth_audit.hpp','native/vulkan/depth_audit.cpp','native/vulkan/depth_readback.hpp',
             'native/vulkan/depth_readback.cpp','native/vulkan/trace_layer.cpp','native/tests/depth_audit_tests.cpp',
             'native/tests/present_layer_vulkan.cpp','native/tests/vulkan_trace_tests.cpp','tools/depth_audit_validate.py']
    result={'schema_version':1,'owned_scene':True,'actual_wsi':True,'game_rendered':False,'xr_submission':False,
            'unit':unit,'cases':cases,'validation_errors':0,'layer_sha256':probe.digest(a.layer/'libd4vr_vulkan_trace.so'),
            'fixture_sha256':probe.digest(a.fixture),'unit_sha256':probe.digest(a.unit),
            'forwarding_sha256':probe.digest(a.forwarding),'transfer_unit':transfer_unit,'source_sha256':{p:probe.digest(root/p) for p in sources}}
    if transfers:
        result['transfer_unit_sha256']=probe.digest(a.transfer_unit)
        for name in ['native/vulkan/depth_transfer_audit.hpp','native/vulkan/depth_transfer_audit.cpp','native/tests/depth_transfer_audit_tests.cpp','native/tests/depth_transfer_fixture.hpp','tools/layer_poc_codegen.py']:
            result['source_sha256'][name]=probe.digest(root/name)
    probe.write_json(a.output/'proof.json',result);return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('output','fixture','unit','forwarding','compiler','layer','validation-layer'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    p.add_argument('--transfers',action='store_true')
    p.add_argument('--transfer-unit',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
