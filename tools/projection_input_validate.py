"""Validate early projection loads and an owned dependent-fragment control.

Without --gpu-fixture this only compiles/validates construction and refusals.
GPU references use owned geometry, never the game or a headset.
"""
import argparse,json,subprocess
from pathlib import Path
import paired_capture,probe
from spirv_probe import Module,build


def validate(a):
    if a.output.exists():raise ValueError('Use a new output directory')
    a.output.mkdir(parents=True,mode=0o700);root=Path(__file__).resolve().parents[1]
    def run(command):return subprocess.run(command,check=True,capture_output=True,text=True,timeout=60)
    def compile_shader(source,target):
        run([str(a.compiler),'-V','--target-env','vulkan1.1',str(source),'-o',str(target)])
        run([str(a.tools/'spirv-val'),'--target-env','vulkan1.2',str(target)])
    vert=a.output/'dependent.vert.spv';frag=a.output/'dependent.frag.spv'
    compile_shader(root/'native/tests/projection_input.vert',vert);compile_shader(root/'native/tests/projection_input.frag',frag)
    def transform(src,target,early=True):return build(src,a.helper,target,a.tools,3,0,72,stereo=True,helper_no_telemetry=True,projection_only_helper=True,projection_input=early)
    early=a.output/'dependent.early.spv';late=a.output/'dependent.late.spv';transform(vert,early);transform(vert,late,False)
    source=(root/'native/tests/projection_input.vert').read_text();refusals=[]
    for name,text,reason in [
        ('dynamic-row',source.replace('camera.rows[72]','camera.rows[72+gl_VertexIndex]'),'cannot bound'),
        ('dynamic-component',source.replace('projected=p.xy/p.w;','projected=p.xy/p.w+camera.rows[72][gl_VertexIndex];'),'load shape'),
        ('function',source.replace('void main(){','float originalFunction(){return camera.rows[72].x;}\nvoid main(){').replace('projected=p.xy/p.w;','projected=p.xy/p.w+originalFunction();'),'single original')]:
        src=a.output/(name+'.vert');binary=a.output/(name+'.spv');src.write_text(text);compile_shader(src,binary)
        try:transform(binary,a.output/(name+'-must-fail.spv'))
        except ValueError as e:
            if reason not in str(e):raise
            refusals.append(name)
        else:raise ValueError(name+' was not refused')
    # A valid Uniform-pointer copy remains bounded and is admitted; a valid
    # volatile projection load is not silently converted to a local load.
    original=Module(vert.read_bytes());camera=next(x[0] for op,x in original.instructions if op==71 and x[1:]==[33,0])
    pointer=next(x for op,x in original.instructions if op==65 and x[2]==camera)
    copied=Module(vert.read_bytes());alias=copied.allocate();rows=[]
    for op,x in copied.instructions:
        rows.append((op,x))
        if op==65 and x[1]==pointer[1]:rows.append((83,[x[0],alias,x[1]]))
        elif op==61 and x[2]==pointer[1]:x[2]=alias
    copied.instructions=rows;copy=a.output/'pointer-copy.spv';copy.write_bytes(copied.binary());run([str(a.tools/'spirv-val'),'--target-env','vulkan1.2',str(copy)]);transform(copy,a.output/'pointer-copy-early.spv')
    volatile=Module(vert.read_bytes())
    for op,x in volatile.instructions:
        if op==61 and x[2]==pointer[1]:x.append(1);break
    file=a.output/'volatile.spv';file.write_bytes(volatile.binary());run([str(a.tools/'spirv-val'),'--target-env','vulkan1.2',str(file)])
    try:transform(file,a.output/'volatile-must-fail.spv')
    except ValueError as e:
        if 'volatile' not in str(e):raise
        refusals.append('volatile')
    else:raise ValueError('Volatile projection load was rewritten')
    cases=[]
    if a.gpu_fixture:
        if not a.validation_layer:raise ValueError('GPU controls require the validation layer')
        for runtime in (False,True) if a.runtime else (False,):
            prefix=paired_capture.host_command()+['/usr/bin/env','VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(a.validation_layer),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer']
            if runtime:prefix+=[str(a.runtime),'--batch','--']
            environment='steam-runtime' if runtime else 'native'
            for name,shader,mode,negative in [('dependent',early,'--depth-calibration',False),('late-negative',late,'--depth-calibration',True)]:
                p=subprocess.run(prefix+[str(a.gpu_fixture),str(vert),str(shader),str(frag),mode],capture_output=True,text=True,timeout=60);log=p.stdout+p.stderr;(a.output/(name+'-'+environment+'.log')).write_text(log)
                if any(x in log for x in ['VUID-','Validation Error','SYNC-HAZARD-']):raise ValueError('Inspect GPU synchronization log')
                if 'Insert instance layer "VK_LAYER_KHRONOS_validation"' not in log or 'Inserted device layer "VK_LAYER_KHRONOS_validation"' not in log:raise ValueError('Validation load unconfirmed')
                if negative:
                    if p.returncode!=1 or 'Calibrated color differs from independent render' not in log:raise ValueError('Late Position control did not expose the varying mismatch')
                    cases.append({'environment':environment,'case':name,'expected_failure_observed':True,'validation_errors':0})
                else:
                    if p.returncode:raise ValueError('Early projection GPU reference failed: '+p.stderr[-300:])
                    proof=json.loads(next(x for x in p.stdout.splitlines() if x.startswith('{"calibration_gpu_passed"')))
                    cases.append({'environment':environment,'case':name,'proof':proof,'validation_errors':0})
    result={'schema_version':1,'synthetic_scene':True,'game_rendered':False,'actual_steamvr_submission':False,'source_shader_stores_preserved':True,'source_uniform_writes_added':0,'helper_calls_per_invocation':4,'construction_refusals':refusals,'uniform_pointer_copy_validated':True,'gpu_cases':cases,'original_sha256':probe.digest(vert),'early_sha256':probe.digest(early),'late_sha256':probe.digest(late),'helper_sha256':probe.digest(a.helper),'source_sha256':{name:probe.digest(root/name) for name in ['tools/projection_input.py','tools/spirv_probe.py','native/tests/projection_input.vert','native/tests/projection_input.frag']}}
    probe.write_json(a.output/'proof.json',result);return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['compiler','tools','helper','output']:p.add_argument('--'+name,type=Path,required=True)
    for name in ['gpu-fixture','validation-layer','runtime']:p.add_argument('--'+name,type=Path)
    a=p.parse_args()
    for k,v in vars(a).items():
        if isinstance(v,Path):setattr(a,k,v.resolve())
    try:print(json.dumps(validate(a),indent=2))
    except (ValueError,OSError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
