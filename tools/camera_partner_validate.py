"""Owned two-projection references and closed SSA route/refusal controls.

No game bundle or temporal XR history is authorized by this diagnostic.
"""
import argparse,json,subprocess
from collections import Counter
from pathlib import Path
import camera_dataflow,paired_capture,probe
from spirv_probe import Module,build


def validate(a):
    if a.output.exists():raise ValueError('Use a new output directory')
    a.output.mkdir(parents=True,mode=0o700);root=Path(__file__).resolve().parents[1]
    def run(cmd):return subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=60)
    def compile(src,dst):run([str(a.compiler),'-V','--target-env','vulkan1.1','-Os',str(src),'-o',str(dst)])
    original=a.output/'source.vert.spv';fragment=a.output/'source.frag.spv'
    compile(root/'native/tests/fixed_partner.vert',original);compile(root/'native/tests/fixed_partner.frag',fragment)
    def transform(src,dst,partner=True):return build(src,a.helper,dst,a.tools,3,0,72,stereo=True,helper_no_telemetry=True,projection_only_helper=True,projection_input=True,fixed_partner_projection=partner)
    paired=a.output/'paired.spv';single=a.output/'primary-only.spv';transform(original,paired);transform(original,single,False)
    before=Module(original.read_bytes());caller=Module((a.output/'intermediate/paired/caller.spv').read_bytes())
    old=Counter(tuple(x) for op,x in before.instructions if op==62);after=Counter(tuple(x) for op,x in caller.instructions if op==62);variables={x[1]:x[2] for op,x in caller.instructions if op==59}
    if old-after:raise ValueError('Original store removed')
    if any(variables.get(x[0])!=7 for x in after-old):raise ValueError('Fixed partner added a non-Function store')
    # The source inverse rows must retain their original access/load sequence.
    def paths(m):
        d={};c={};aliases={};loads=[]
        for op,x in m.instructions:
            if op==71:d.setdefault(x[0],{})[x[1]]=x[2:]
            elif op==43 and len(x)==3:c[x[1]]=x[2]
        for i,v in d.items():
            if v.get(34)==[3] and v.get(33)==[0]:aliases[i]=()
        for op,x in m.instructions:
            if op in (65,66) and x[2] in aliases:aliases[x[1]]=aliases[x[2]]+tuple(c.get(i) for i in x[3:])
            elif op==83 and x[2] in aliases:aliases[x[1]]=aliases[x[2]]
            elif op==61 and x[2] in aliases and len(aliases[x[2]])>=2 and aliases[x[2]][1] in (42,43,44,45):loads.append(x)
        return loads
    if not paths(before) or paths(before)!=paths(caller):raise ValueError('Source inverse loads changed')
    routes=camera_dataflow.validated(original.read_bytes(),a.grammar.read_bytes(),a.glsl_grammar.read_bytes(),a.tools/'spirv-val')
    refusals=[]
    def refused(name,module,text):
        file=a.output/(name+'.spv');file.write_bytes(module.binary());run([str(a.tools/'spirv-val'),'--target-env','vulkan1.2',str(file)])
        try:transform(file,a.output/(name+'-must-fail.spv'))
        except ValueError as e:
            if text not in str(e):raise
            refusals.append(name)
        else:raise ValueError(name+' accepted')
    short=Module(original.read_bytes())
    for op,x in short.instructions:
        if op==43 and x[2:]==[113]:x[2]=83
    refused('short-partner-extent',short,'extent')
    decorations={}
    for op,x in before.instructions:
        if op==71:decorations.setdefault(x[0],{})[x[1]]=x[2:]
    target=next(i for i,d in decorations.items() if d.get(34)==[3] and d.get(33)==[0]);constants={x[1]:x[2] for op,x in before.instructions if op==43 and len(x)==3}
    pointer=next(x[1] for op,x in before.instructions if op==65 and x[2]==target and constants.get(x[4])==80)
    volatile=Module(original.read_bytes())
    for op,x in volatile.instructions:
        if op==61 and x[2]==pointer:x.append(1);break
    refused('volatile-partner-load',volatile,'volatile')
    # Valid dynamic paths are rejected by both the constructor and graph model.
    dynamic_src=a.output/'dynamic.vert';dynamic_src.write_text((root/'native/tests/fixed_partner.vert').read_text().replace('camera.rows[80]','camera.rows[80+gl_VertexIndex]'));dynamic=a.output/'dynamic.spv';compile(dynamic_src,dynamic)
    refused('dynamic-partner-path',Module(dynamic.read_bytes()),'bound')
    try:camera_dataflow.validated(dynamic.read_bytes(),a.grammar.read_bytes(),a.glsl_grammar.read_bytes(),a.tools/'spirv-val')
    except ValueError as e:
        if 'Dynamic' not in str(e):raise
    else:raise ValueError('Dynamic route silently accepted')
    cases=[]
    if a.gpu_fixture:
        if not a.validation_layer:raise ValueError('GPU checks require synchronization validation')
        for runtime in (False,True) if a.runtime else (False,):
            prefix=paired_capture.host_command()+['/usr/bin/env','VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(a.validation_layer),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer']
            if runtime:prefix+=[str(a.runtime),'--batch','--']
            environment='steam-runtime' if runtime else 'native'
            for name,shader,negative in [('paired',paired,False),('primary-only-negative',single,True)]:
                p=subprocess.run(prefix+[str(a.gpu_fixture),str(original),str(shader),str(fragment),'--fixed-partner'],capture_output=True,text=True,timeout=60);log=p.stdout+p.stderr;(a.output/(name+'-'+environment+'.log')).write_text(log)
                if any(s in log for s in ['VUID-','Validation Error','SYNC-HAZARD-']):raise ValueError('Inspect fixed partner GPU validation log')
                if 'Insert instance layer "VK_LAYER_KHRONOS_validation"' not in log or 'Inserted device layer "VK_LAYER_KHRONOS_validation"' not in log:raise ValueError('Synchronization validation load unconfirmed')
                if negative:
                    if p.returncode!=1 or 'Fixed partner color differs from independent render' not in log:raise ValueError('Primary-only control did not expose partner mismatch')
                    cases.append({'environment':environment,'case':name,'expected_failure_observed':True,'validation_errors':0})
                else:
                    if p.returncode:raise ValueError('Fixed partner GPU check failed; inspect private log')
                    proof=json.loads(next(x for x in p.stdout.splitlines() if x.startswith('{"fixed_partner_gpu_passed"')));cases.append({'environment':environment,'case':name,'proof':proof,'validation_errors':0})
    result={'schema_version':1,'synthetic_scene':True,'game_rendered':False,'game_bundle_authorized':False,'temporal_xr_history_proven':False,'whole_scene_verified':False,'gpu_cases':cases,'construction_refusals':refusals,'dynamic_ssa_path_refused':True,'original_store_count':sum(old.values()),'original_store_count_retained':sum(old.values()),'added_non_function_stores':0,'source_inverse_loads_unchanged':True,'helper_calls_per_invocation':8,'owned_original_routes':routes,'helper_sha256':probe.digest(a.helper),'source_sha256':{x:probe.digest(root/x) for x in ['tools/spirv_probe.py','tools/fixed_partner_projection.py','tools/camera_dataflow.py','native/tests/fixed_partner.vert','native/tests/fixed_partner.frag','native/tests/fixed_partner_gpu_checks.hpp','native/tests/gpu_probe_vulkan.cpp']}}
    probe.write_json(a.output/'proof.json',result);return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for k in ['compiler','tools','helper','grammar','glsl-grammar','output']:p.add_argument('--'+k,type=Path,required=True)
    for k in ['gpu-fixture','validation-layer','runtime']:p.add_argument('--'+k,type=Path)
    a=p.parse_args()
    for k,v in vars(a).items():
        if isinstance(v,Path):setattr(a,k,v.resolve())
    try:print(json.dumps(validate(a),indent=2))
    except (ValueError,OSError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
