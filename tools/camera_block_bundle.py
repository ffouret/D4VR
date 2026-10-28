"""Prepare a private, observation-only block probe from previously observed shaders."""
import json,os,shutil,subprocess,time
from pathlib import Path
import camera_block,camera_capture,paired_capture,probe,shader_capture
from spirv_probe import build,Module


def prepare_bundle(baseline,shaders,observed,controls,tools,output):
    if output.exists():raise ValueError('Use a new camera-block bundle directory')
    source=camera_capture.validate_bundle(baseline);certificate=json.loads((controls/'proof.json').read_text());helper=controls/'helper.spv';root=Path(__file__).resolve().parents[1]
    expected={x:probe.digest(root/x) for x in ['tools/spirv_probe.py','tools/camera_block.py','native/tests/camera_block_gpu_checks.hpp','native/tests/gpu_probe_vulkan.cpp']}
    if certificate.get('source_sha256')!=expected or certificate.get('helper_sha256')!=probe.digest(helper):raise ValueError('Camera block source/helper certificate changed')
    if certificate.get('rows')!=113 or certificate.get('maximum_records')!=64 or certificate.get('short_valid_ubo_refused') is not True:raise ValueError('Camera block bounds unverified')
    cases=certificate.get('gpu_cases',[])
    if len(cases)!=2 or {c.get('environment') for c in cases}!={'native','steam-runtime'}:raise ValueError('Camera block requires native and runtime GPU controls')
    for c in cases:
        p=c.get('proof',{})
        if c.get('validation_errors')!=0 or not c.get('synchronization_validation_loaded') or p.get('camera_block_gpu_passed') is not True or p.get('exact_source_ubo_bytes')!=5424 or p.get('raw_float_bits_preserved') is not True or p.get('unchanged_rendered_bytes',0)<=0:raise ValueError('Camera block GPU certificate incomplete')
    evidence=json.loads(observed.read_text());selected={s['shader_hash'] for s in evidence['samples'] if abs(s['matrix_rows'][0][0]-1.784022)<1e-5 and abs(s['matrix_rows'][1][1]-3.171595)<1e-5}
    entries=[e for e in source['entries'] if Path(e['file']).stem in selected]
    if not entries or len(entries)!=len(selected):raise ValueError('Previously observed shader identities unavailable')
    output.mkdir(parents=True,mode=0o700);overrides=output/'overrides';overrides.mkdir();result=[]
    for e in entries:
        original=shaders/e['file']
        if probe.digest(original)!=e['original_sha256']:raise ValueError('Original observed shader changed')
        target=overrides/e['file'];build(original,helper,target,tools,3,0,0,camera_block_rows=113)
        caller=Module((overrides/'intermediate'/target.stem/'caller.spv').read_bytes());before=Module(original.read_bytes());old=[a for op,a in before.instructions if op==62];variables={a[1]:a[2] for op,a in caller.instructions if op==59}
        if any((62,a) not in caller.instructions for a in old) or any(op==62 and a not in old and variables.get(a[0])!=7 for op,a in caller.instructions):raise ValueError('Camera block altered original source stores')
        result.append({'file':e['file'],'original_sha256':e['original_sha256'],'override_sha256':probe.digest(target)})
    shutil.rmtree(overrides/'intermediate')
    manifest={'schema_version':2,'purpose':'read_only_camera_block_diagnostic','game_version':source['game_version'],'proton_path':source['proton_path'],'runtime_module':source['runtime_module'],'runtime_module_sha256':source['runtime_module_sha256'],'helper_sha256':probe.digest(helper),'gpu_fixture_passed':True,'gpu_certificate':certificate,'observed_matrix_evidence_sha256':probe.digest(observed),'uniform_descriptor_set':3,'uniform_binding':0,'first_row':0,'rows':113,'sample_stride_qualified_invocations':4096,'maximum_records':64,'maximum_payload_bytes':64*113*16,'lossless_hex_words':True,'camera_or_position_modified':False,'whole_scene_covered':False,'entries':result}
    probe.write_json(output/'bundle.json',manifest);return manifest


def prepare_game(game,launcher,bundle,output):
    game,launcher,bundle,output=[p.resolve() for p in (game,launcher,bundle,output)]
    manifest=camera_capture.validate_bundle(bundle)
    if manifest.get('purpose')!='read_only_camera_block_diagnostic' or manifest.get('game_version')!=probe.profile()['game_version']:raise ValueError('Use the matching certified block-observer bundle')
    if output.exists():raise ValueError('Use a new isolated capture directory')
    source=paired_capture.launcher_source(launcher.read_text(),manifest['proton_path'],game,output,True);audit=probe.check_target(game,probe.profile())
    capture=shader_capture.prepare(game,launcher,output,probe.profile());(output/'vendor-cache').mkdir(mode=0o700)
    script=output/'battle-net-block.sh';script.write_text(source);script.chmod(0o700)
    variables={'D4VR_TRACE_DIR':str(output),'VKD3D_SHADER_OVERRIDE':probe.windows_path(bundle/'overrides'),'VKD3D_SHADER_CACHE_PATH':capture['variables']['VKD3D_SHADER_CACHE_PATH'],'VKD3D_SHADER_DEBUG':'info','VKD3D_SHADER_DEBUG_RING_SIZE_LOG2':'24','VKD3D_DEBUG':'info','VKD3D_LOG_FILE':probe.windows_path(output/'camera-block.log')}
    capture.update(purpose='read_only_camera_block_diagnostic',variables=variables,private_launcher=str(script),game_audit_before=audit,shader_bundle_sha256=probe.digest(bundle/'bundle.json'),override_count=len(manifest['entries']),camera_or_position_modified=False,game_stereo_submitted=False)
    probe.write_json(output/'capture.json',capture);return capture


def launch(output):
    output=output.resolve()
    capture=json.loads((output/'capture.json').read_text())
    if capture.get('phase')!='prepared' or capture.get('purpose')!='read_only_camera_block_diagnostic' or shader_capture.launcher_processes():raise ValueError('Block capture is not ready or another game/launcher is active')
    command=paired_capture.host_command()+['/usr/bin/env']
    keys={'VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','VK_LAYER_PATH','VKD3D_SHADER_DUMP_PATH','VK_LOADER_DEBUG','VK_LAYER_ENABLES','VK_LAYER_VALIDATE_SYNC'}|{k for k in os.environ if k.startswith('D4VR_')}
    for k in sorted(keys):command+=['-u',k]
    command+=[k+'='+v for k,v in capture['variables'].items()]+['/usr/bin/bash',capture['private_launcher']]
    capture['phase']='running';probe.write_json(output/'capture.json',capture)
    with (output/'launcher-private.log').open('xb') as stream:
        os.chmod(stream.name,0o600);p=subprocess.Popen(command,stdout=stream,stderr=subprocess.STDOUT)
        while p.poll() is None:
            probe.write_json(output/'owned-current.json',{'processes':paired_capture.owned_processes(output)})
            try:p.wait(timeout=2)
            except subprocess.TimeoutExpired:pass
    capture.update(phase='launcher_exited',launcher_exit_code=p.returncode,game_audit_after=probe.check_target(Path(capture['game_directory']),probe.profile()))
    probe.write_json(output/'capture.json',capture);return capture
