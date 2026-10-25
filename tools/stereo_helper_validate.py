#!/usr/bin/env python3
"""Build and validate original helper controls; never execute game shaders."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import probe
from spirv_probe import Module,build,literal
from stereo_helper import verify,MARKER
from shader_effects import Grammar,analyze
from glsl_effects import GLSL450
from shader_effects_validate import GRAMMAR_SHA256,GLSL_GRAMMAR_SHA256


def validate(compiler: Path,tools: Path,grammar_path: Path,glsl_path: Path,output: Path) -> dict:
    if probe.digest(grammar_path)!=GRAMMAR_SHA256 or probe.digest(glsl_path)!=GLSL_GRAMMAR_SHA256:
        raise ValueError('Controls require the pinned Khronos grammars')
    grammar=Grammar(grammar_path.read_bytes());glsl=GLSL450(glsl_path.read_bytes())
    output.mkdir(parents=True,mode=0o700,exist_ok=False)
    repo=Path(__file__).resolve().parents[1];sources={};helpers={};cases=[]
    def compile_source(source,stem,defines=()):
        path=output/(stem+'.spv');src=repo/source;sources[source]=probe.digest(src)
        subprocess.run([str(compiler),'-V','--target-env','vulkan1.1','-Od',*defines,str(src),'-o',str(path)],check=True,capture_output=True,timeout=30)
        subprocess.run([str(tools/'spirv-val'),'--target-env','vulkan1.2',str(path)],check=True,capture_output=True,timeout=30)
        return path
    for name,defines in [('diagnostic',()),('no-telemetry',('-DD4VR_DIAGNOSTIC_TELEMETRY=0',))]:
        path=compile_source('native/probe_gpu/stereo_transform.vert','helper-'+name,defines)
        result=analyze(path.read_bytes(),grammar,glsl)
        if name=='no-telemetry':
            contract=verify(path.read_bytes())
            if result['non_atomic_external_pointer_write_instructions'] or result['atomic_instructions'] or result['barrier_instructions']:
                raise ValueError('Telemetry-free helper retains diagnostic memory operations')
        else:
            contract=None
            if (result['explicit_external_pointer_write_instructions']!=9 or result['atomic_instructions']!={'OpAtomicIAdd':3}
                    or result['barrier_instructions']!={'OpMemoryBarrier':2}):
                raise ValueError('Diagnostic helper positive census changed')
            try:verify(path.read_bytes())
            except ValueError:pass
            else:raise ValueError('Diagnostic helper falsely accepted as telemetry-free')
        helpers[name]=path
        cases.append({'case':name,'spirv_val_passed':True,'helper_contract':contract,'census':result})
    for stem,source,writes in [('probe','native/tests/probe_fixture.vert',0),
            ('mrt','native/tests/stereo_mrt.vert',0),('mrt-vertex','native/tests/stereo_mrt_vertex.vert',0),
            ('source-write','native/tests/stereo_helper_write.vert',1)]:
        original=compile_source(source,stem)
        for name,helper in helpers.items():
            path=output/(stem+'-'+name+'.spv')
            build(original,helper,path,tools,3,0,72,stereo=True,helper_no_telemetry=name=='no-telemetry')
            module=Module(path.read_bytes());markers=[literal(a) for op,a in module.instructions if op==330]
            result=analyze(path.read_bytes(),grammar,glsl)
            expected=writes+(9 if name=='diagnostic' else 0)
            if result['non_atomic_external_pointer_write_instructions']!=expected or result['unresolved_pointer_write_instructions']:
                raise ValueError('Source/helper writes were dropped or introduced')
            if (MARKER in markers)!=(name=='no-telemetry') or 'D4VR_STEREO_ABI_1' not in markers:
                raise ValueError('Helper provenance marker missing or added to diagnostic output')
            if result['external_memory_writes_excluded'] or result['replay_allowed']:
                raise ValueError('Helper marker falsely authorized a transformed module')
            cases.append({'case':stem+'-'+name,'spirv_val_passed':True,'helper_marker_present':MARKER in markers,'census':result})
    # This is a valid compiler output with one extra harmless OpNop. Its unknown
    # opcode must be rejected, rather than implicitly expanding the helper policy.
    changed=Module(helpers['no-telemetry'].read_bytes())
    returned=next(i for i,(op,_) in enumerate(changed.instructions) if op==253)
    changed.instructions.insert(returned,(0,[]));extra=output/'unreviewed-opcode.spv';extra.write_bytes(changed.binary())
    subprocess.run([str(tools/'spirv-val'),'--target-env','vulkan1.2',str(extra)],check=True,capture_output=True,timeout=30)
    try:verify(extra.read_bytes())
    except ValueError:pass
    else:raise ValueError('Unreviewed valid helper opcode accepted')
    try:build(original,helpers['diagnostic'],output/'must-not-build.spv',tools,3,0,72,stereo=True,helper_no_telemetry=True)
    except ValueError:pass
    else:raise ValueError('Builder mislabeled the diagnostic helper')
    return {'schema_version':1,'scope':'original standalone helper and transformed synthetic shaders only',
            'cases':cases,'validated_unknown_opcode_rejected':True,'diagnostic_helper_opt_in_rejected':True,
            'original_shader_write_preserved':True,'grammar_sha256':grammar.sha256,'glsl_grammar_sha256':glsl.sha256,
            'source_sha256':{**sources,**{p:probe.digest(repo/p) for p in ('tools/stereo_helper.py','tools/stereo_helper_validate.py','tools/spirv_probe.py')}},
            'gpu_execution_performed_by_this_tool':False,'replay_allowed':False,'game_stereo_submitted':False}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('compiler','tools','grammar','glsl-grammar','output-directory'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();result=validate(a.compiler.resolve(),a.tools.resolve(),a.grammar,a.glsl_grammar,a.output_directory.resolve())
    probe.write_json(a.output_directory/'proof.json',result)
    print(json.dumps({'validated_control_modules':len(result['cases']),'negative_controls_rejected':True}))


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        print(f'D4VR helper validation: {error}');raise SystemExit(2)
