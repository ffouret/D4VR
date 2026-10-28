"""Compile/validate the lossless block observer; optional owned GPU controls."""
import argparse,json,subprocess
from pathlib import Path
import camera_block,paired_capture,probe
from spirv_probe import Module,build


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(parents=True,mode=0o700);root=Path(__file__).resolve().parents[1]
    def run(cmd):return subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=60)
    def compile(src,out):run([str(a.compiler),'-V','--target-env','vulkan1.1','-Od',str(src),'-o',str(out)])
    source=a.output/'block-helper.vert';source.write_text(camera_block.helper_source());helper=a.output/'helper.spv';compile(source,helper)
    original=a.output/'original.spv';fragment=a.output/'fragment.spv';compile(root/'native/tests/probe_fixture.vert',original);compile(root/'native/tests/probe_fixture.frag',fragment)
    patched=a.output/'block.spv';build(original,helper,patched,a.tools,3,0,0,camera_block_rows=113)
    before=Module(original.read_bytes());caller=Module((a.output/'intermediate/block/caller.spv').read_bytes());stores=[x for op,x in before.instructions if op==62];variables={x[1]:x[2] for op,x in caller.instructions if op==59}
    for x in stores:
        if (62,x) not in caller.instructions:raise ValueError('Original source store removed')
    for op,x in caller.instructions:
        if op==62 and x not in stores and variables.get(x[0])!=7:raise ValueError('Observer added an original-resource/output store')
    for binary in (original,helper,patched):run([str(a.tools/'spirv-val'),'--target-env','vulkan1.2',str(binary)])
    short=Module(original.read_bytes())
    for op,x in short.instructions:
        if op==43 and x[2:]==[113]:x[2]=112
    shortfile=a.output/'short.spv';shortfile.write_bytes(short.binary());run([str(a.tools/'spirv-val'),'--target-env','vulkan1.2',str(shortfile)])
    try:build(shortfile,helper,a.output/'must-fail.spv',a.tools,3,0,0,camera_block_rows=113)
    except ValueError as e:
        if 'extent' not in str(e):raise
    else:raise ValueError('Short original UBO admitted')
    cases=[]
    if a.gpu_fixture:
        if not a.validation_layer:raise ValueError('GPU controls require synchronization validation')
        for runtime in (False,True) if a.runtime else (False,):
            command=paired_capture.host_command()+['/usr/bin/env','VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(a.validation_layer),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer']
            if runtime:command+=[str(a.runtime),'--batch','--']
            p=subprocess.run(command+[str(a.gpu_fixture),str(original),str(patched),str(fragment),'--camera-block'],capture_output=True,text=True,timeout=60);log=p.stdout+p.stderr;name='steam-runtime' if runtime else 'native';(a.output/(name+'.log')).write_text(log)
            if p.returncode or any(x in log for x in ['VUID-','Validation Error','SYNC-HAZARD-']):raise ValueError('Camera block GPU/validation failed; inspect private log')
            if 'Insert instance layer "VK_LAYER_KHRONOS_validation"' not in log or 'Inserted device layer "VK_LAYER_KHRONOS_validation"' not in log:raise ValueError('Validation load unconfirmed')
            proof=json.loads(next(x for x in p.stdout.splitlines() if x.startswith('{"camera_block_gpu_passed"')));cases.append({'environment':name,'proof':proof,'synchronization_validation_loaded':True,'validation_errors':0})
    result={'schema_version':1,'synthetic_scene':True,'game_camera_block_captured':False,'actual_steamvr_submission':False,'rows':113,'words_per_record':1356,'maximum_records':64,'maximum_payload_bytes':64*113*16,'short_valid_ubo_refused':True,'original_source_stores_preserved':True,'original_resource_writes_added':0,'gpu_cases':cases,'helper_sha256':probe.digest(helper),'original_sha256':probe.digest(original),'patched_sha256':probe.digest(patched),'source_sha256':{x:probe.digest(root/x) for x in ['tools/spirv_probe.py','tools/camera_block.py','native/tests/camera_block_gpu_checks.hpp','native/tests/gpu_probe_vulkan.cpp']}}
    probe.write_json(a.output/'proof.json',result);return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for k in ['compiler','tools','output']:p.add_argument('--'+k,type=Path,required=True)
    for k in ['gpu-fixture','validation-layer','runtime']:p.add_argument('--'+k,type=Path)
    a=p.parse_args()
    for k,v in vars(a).items():
        if isinstance(v,Path):setattr(a,k,v.resolve())
    try:print(json.dumps(validate(a),indent=2))
    except (ValueError,OSError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
