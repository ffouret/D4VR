#!/usr/bin/env python3
"""Validate write-effect exclusion against original positive/negative SPIR-V controls."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import probe
from spirv_probe import Module
from shader_write_contract import Policy
from stereo_helper_validate import validate as helper_controls


def validate(compiler: Path,tools: Path,core: Path,glsl: Path,output: Path) -> dict:
    output.mkdir(parents=True,mode=0o700,exist_ok=False)
    policy=Policy(core.read_bytes(),glsl.read_bytes());repo=Path(__file__).resolve().parents[1]
    helper_controls(compiler,tools,core,glsl,output/'helpers')
    cases=[]
    def check(name,path,expected,required_reason=None,validation=True):
        result=policy.validate(path.read_bytes(),tools/'spirv-val')
        if result['validated_non_output_external_writes_excluded'] is not expected or result['module_validation_passed'] is not validation:
            raise ValueError('Unexpected write contract result for '+name+': '+str(result['blocking_reasons']))
        if required_reason and required_reason not in result['blocking_reasons']:raise ValueError('Expected block missing for '+name)
        if result['replay_allowed']:raise ValueError('Write contract authorized replay')
        cases.append({'case':name,'expected_validated_write_exclusion':expected,**result})
    for name in ('diagnostic','no-telemetry'):
        check('helper-'+name,output/'helpers'/('helper-'+name+'.spv'),name=='no-telemetry')
        for stem in ('probe','mrt','mrt-vertex','source-write'):
            check(stem+'-'+name,output/'helpers'/(stem+'-'+name+'.spv'),name=='no-telemetry' and stem!='source-write')
    for stem in ('probe','mrt','mrt-vertex','source-write'):
        check('original-'+stem,output/'helpers'/(stem+'.spv'),stem!='source-write')
    sources={}
    def built(name,source,defines=()):
        src=repo/source;path=output/(name+'.spv');sources[source]=probe.digest(src)
        if src.suffix=='.spvasm':cmd=[str(tools/'spirv-as'),'--target-env','vulkan1.2',str(src),'-o',str(path)]
        else:cmd=[str(compiler),'-V','--target-env','vulkan1.2',*defines,str(src),'-o',str(path)]
        subprocess.run(cmd,capture_output=True,check=True,timeout=30)
        return path
    for name,source,expected,reason,defines in [
        ('glsl-pure','native/tests/glsl_pure_effects.spvasm',True,None,()),
        ('glsl-external-writes','native/tests/glsl_pointer_effects.spvasm',False,'non_output_external_or_unresolved_write',()),
        ('glsl-local-writes','native/tests/glsl_local_effects.spvasm',True,None,()),
        ('sampled-fragment','native/tests/shader_effects_fixture.frag',True,None,()),
        ('input-attachment','native/tests/shader_effects_fixture.frag',True,None,('-DD4VR_EFFECT_INPUT_ATTACHMENT=1',)),
        ('physical-write','native/tests/shader_effects_fixture.comp',False,'non_output_external_or_unresolved_write',('-DD4VR_EFFECT_PHYSICAL=1',)),
        ('buffer-image-atomic-write','native/tests/shader_effects_fixture.comp',False,'non_output_external_or_unresolved_write',()),
        ('debug-printf','native/tests/glsl_unknown_effects.frag',False,'unreviewed_extended_set',()),
        ('uncalled-external-copy','native/tests/shader_write_copy.spvasm',False,'non_output_external_or_unresolved_write',())]:
        check(name,built(name,source,defines),expected,reason)
    text=(repo/'native/tests/shader_write_copy.spvasm').read_text()
    for name,replacement,expected,reason in [
        ('uncalled-local-copy','OpCopyMemory %local %pointer',True,None),
        ('uncalled-atomic','%sum = OpAtomicIAdd %uint %pointer %one %zero %one',False,'unreviewed_opcode:234'),
        ('uncalled-barrier','OpMemoryBarrier %one %semantics',False,'unreviewed_opcode:225')]:
        src=output/(name+'.spvasm');src.write_text(text.replace('OpCopyMemory %pointer %local',replacement));path=output/(name+'.spv')
        subprocess.run([str(tools/'spirv-as'),'--target-env','vulkan1.2',str(src),'-o',str(path)],capture_output=True,check=True,timeout=30)
        check(name,path,expected,reason)
    # Static opcode coverage cannot replace validation: a vertex built-in is
    # invalid in a fragment entry point even though every instruction is reviewed.
    invalid=Module((output/'helpers/mrt-vertex-no-telemetry.spv').read_bytes())
    next(a for o,a in invalid.instructions if o==15)[0]=4
    path=output/'invalid-stage.spv';path.write_bytes(invalid.binary())
    check('invalid-stage-covered-opcodes',path,False,validation=False)
    if not cases[-1]['static_non_output_external_writes_excluded']:raise ValueError('Invalid-stage control no longer isolates validation')
    # The general contract deliberately admits a reviewed OpNop rejected by the
    # smaller helper-only ABI contract. Coverage is explicit in each policy.
    check('reviewed-nop-helper',output/'helpers/unreviewed-opcode.spv',True)
    return {'schema_version':1,'scope':'static original synthetic controls; never GPU executed',
        'cases':cases,'source_sha256':{**sources,**{p:probe.digest(repo/p) for p in ('tools/shader_write_contract.py','tools/shader_write_validate.py')}},
        'controls':len(cases),'validated_write_exclusion_controls':sum(c['validated_non_output_external_writes_excluded'] for c in cases),
        'all_replay_permissions_false':True,'gpu_execution_performed':False,'game_stereo_submitted':False}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('compiler','tools','grammar','glsl-grammar','output-directory'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();r=validate(a.compiler.resolve(),a.tools.resolve(),a.grammar,a.glsl_grammar,a.output_directory.resolve())
    probe.write_json(a.output_directory/'proof.json',r)
    print(json.dumps({k:r[k] for k in ('controls','validated_write_exclusion_controls','all_replay_permissions_false')}))


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,IndexError,subprocess.SubprocessError) as error:
        print(f'D4VR write controls: {error}');raise SystemExit(2)
