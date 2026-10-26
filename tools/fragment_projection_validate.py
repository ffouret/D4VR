"""Owned VS+fragment projection references; explicit stage/helper admission."""
import argparse,json,subprocess
from collections import Counter
from pathlib import Path
import paired_capture,probe
from spirv_probe import Module,build


def validate(a):
    if a.output.exists():raise ValueError('Use a new output directory')
    a.output.mkdir(parents=True,mode=0o700);root=Path(__file__).resolve().parents[1]
    def run(cmd):return subprocess.run(cmd,check=True,capture_output=True,text=True,timeout=60)
    def compile(src,dst,extra=()):run([str(a.compiler),'-V','--target-env','vulkan1.1',*extra,str(src),'-o',str(dst)])
    vert=a.output/'vertex.spv';frag=a.output/'fragment.spv';vout=a.output/'vertex.modified.spv';fout=a.output/'fragment.modified.spv'
    compile(root/'native/tests/fixed_partner.vert',vert,('-Os',));compile(root/'native/tests/fragment_projection.frag',frag,('-Os',))
    def transform(source,dst,fragment=False,helper=None):return build(source,helper or a.helper,dst,a.tools,3,0,72,stereo=True,helper_no_telemetry=True,projection_only_helper=True,projection_input=True,fixed_partner_projection=not fragment,fragment_projection_input=fragment)
    transform(vert,vout);transform(frag,fout,True)
    before=Module(frag.read_bytes());caller=Module((a.output/'intermediate/fragment.modified/caller.spv').read_bytes());old=Counter(tuple(x) for op,x in before.instructions if op==62);after=Counter(tuple(x) for op,x in caller.instructions if op==62);variables={x[1]:x[2] for op,x in caller.instructions if op==59}
    if old-after or any(variables.get(x[0])!=7 for x in after-old):raise ValueError('Fragment source stores changed or external store added')
    if any(op==71 and x[1:]==[11,42] for op,x in caller.instructions):raise ValueError('VertexIndex was added to fragment')
    # This remains a valid, memory-reviewed helper, but its vertex argument is
    # no longer unused. Fragment admission must not supply an invented ID.
    changed=a.output/'uses-vertex.vert';changed.write_text((root/'native/probe_gpu/stereo_transform.vert').read_text().replace('vec4 before = clip_position;','if(vertex_id != 0u) clip_position.x += 1.;\n    vec4 before = clip_position;'))
    changed_binary=a.output/'uses-vertex.spv';compile(changed,changed_binary,('-Od','-DD4VR_DIAGNOSTIC_TELEMETRY=0'));run([str(a.tools/'spirv-val'),'--target-env','vulkan1.2',str(changed_binary)])
    from stereo_helper import verify_projection_only
    verify_projection_only(changed_binary.read_bytes())
    try:transform(frag,a.output/'must-fail.spv',True,changed_binary)
    except ValueError as e:
        if 'vertex argument' not in str(e):raise
    else:raise ValueError('Fragment helper used vertex identity')
    try:transform(vert,a.output/'wrong-stage.spv',True)
    except ValueError as e:
        if 'entry point' not in str(e):raise
    else:raise ValueError('Fragment mode accepted vertex entry')
    slice_control=None
    if a.grammar and a.glsl_grammar:
        import fragment_slice
        def ssa(source):
            target=source.with_name(source.stem+'.ssa.spv');run([str(a.tools/'spirv-opt'),'--convert-local-access-chains','--eliminate-local-single-block','--eliminate-local-single-store','--ssa-rewrite',str(source),'-o',str(target)]);return target
        # A separate logical-address control permits local SSA promotion.
        # The GPU shaders retain the physical address model required to link
        # the helper. This normalized control is not a byte-identity claim.
        logical_text=(root/'native/tests/fragment_projection.frag').read_text().replace('#extension GL_EXT_buffer_reference : require\n','').replace('layout(buffer_reference,std430) readonly buffer AddressModel {float x;};\n','')
        logical=a.output/'slice-logical.frag';logical.write_text(logical_text);logical_binary=a.output/'slice-logical.spv';compile(logical,logical_binary,('-Os',))
        sliced=fragment_slice.validated(ssa(logical_binary).read_bytes(),a.grammar.read_bytes(),a.glsl_grammar.read_bytes(),a.tools/'spirv-val')
        texture=a.output/'texture.frag';texture.write_text(logical_text.replace('void main(){','layout(set=3,binding=4) uniform sampler2D sourceTexture;\nvoid main(){').replace('sourceWorld.x<0 ? .125:.875','texture(sourceTexture,vec2(.5)).x'))
        binary=a.output/'texture.spv';compile(texture,binary,('-Os',))
        try:fragment_slice.validated(ssa(binary).read_bytes(),a.grammar.read_bytes(),a.glsl_grammar.read_bytes(),a.tools/'spirv-val')
        except ValueError as e:
            if 'slice value 87' not in str(e) and 'slice value 88' not in str(e):raise
        else:raise ValueError('Texture slice silently treated as pure arithmetic')
        slice_control={'owned_slice':sliced,'separate_logical_address_control':True,'valid_texture_slice_refused':True}
    cases=[]
    if a.gpu_fixture:
        if not a.validation_layer:raise ValueError('GPU checks require synchronization validation')
        for runtime in (False,True) if a.runtime else (False,):
            prefix=paired_capture.host_command()+['/usr/bin/env','VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(a.validation_layer),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer']
            if runtime:prefix+=[str(a.runtime),'--batch','--']
            environment='steam-runtime' if runtime else 'native'
            for name,module,negative in [('both-stages',fout,False),('vertex-only-negative',frag,True)]:
                p=subprocess.run(prefix+[str(a.gpu_fixture),str(vert),str(vout),str(frag),'--fixed-partner','--fragment-projection',str(module)],capture_output=True,text=True,timeout=60);log=p.stdout+p.stderr;(a.output/(name+'-'+environment+'.log')).write_text(log)
                if any(x in log for x in ('VUID-','Validation Error','SYNC-HAZARD-')):raise ValueError('Inspect fragment projection GPU validation log')
                if 'Insert instance layer "VK_LAYER_KHRONOS_validation"' not in log or 'Inserted device layer "VK_LAYER_KHRONOS_validation"' not in log:raise ValueError('Synchronization validation load unconfirmed')
                if negative:
                    if p.returncode!=1 or 'Fixed partner color differs from independent render' not in log:raise ValueError('Vertex-only control did not expose fragment mismatch')
                    cases.append({'environment':environment,'case':name,'expected_failure_observed':True,'validation_errors':0})
                else:
                    if p.returncode:raise ValueError('Fragment projection references failed; inspect private log')
                    proof=json.loads(next(x for x in p.stdout.splitlines() if x.startswith('{"fixed_partner_gpu_passed"')));cases.append({'environment':environment,'case':name,'proof':proof,'validation_errors':0})
    result={'schema_version':1,'synthetic_scene':True,'game_rendered':False,'actual_steamvr_submission':False,'temporal_xr_history_proven':False,'gpu_cases':cases,'slice_controls':slice_control,'valid_helper_using_vertex_argument_refused':True,'wrong_stage_refused':True,'fragment_vertex_index_added':False,'fragment_original_stores_retained':sum(old.values()),'fragment_new_nonfunction_stores':0,'fragment_helper_calls_per_invocation':4,'helper_sha256':probe.digest(a.helper),'source_sha256':{x:probe.digest(root/x) for x in ['tools/spirv_probe.py','tools/stereo_helper.py','tools/projection_input.py','tools/fragment_slice.py','native/tests/fixed_partner.vert','native/tests/fragment_projection.frag','native/tests/fixed_partner_gpu_checks.hpp','native/tests/gpu_probe_vulkan.cpp']}}
    probe.write_json(a.output/'proof.json',result);return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['compiler','tools','helper','output']:p.add_argument('--'+name,type=Path,required=True)
    for name in ['gpu-fixture','validation-layer','runtime','grammar','glsl-grammar']:p.add_argument('--'+name,type=Path)
    a=p.parse_args()
    for k,v in vars(a).items():
        if isinstance(v,Path):setattr(a,k,v.resolve())
    try:print(json.dumps(validate(a),indent=2))
    except (ValueError,OSError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
