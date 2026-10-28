#!/usr/bin/env python3
"""Compile, validate and audit original memory-effect controls without executing them."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import probe
from shader_effects import Grammar, analyze
from glsl_effects import GLSL450

GRAMMAR_COMMIT='fbf86836a9b1966586faf7be7bb8a6440e156d62'
GRAMMAR_SHA256='dd2a7c48d3131c9d0c78ad669ab4765048e6fd63e1fe277a0858f5458febdc68'
GLSL_GRAMMAR_SHA256='5d72e48247569f9b77a17a849db624350abc9d26f7f72b153bbcf15c37238239'


def validate(compiler: Path, validator: Path, grammar_path: Path, output: Path, host_bridge: Path | None = None,
             glsl_path: Path | None = None, assembler: Path | None = None) -> dict:
    data=grammar_path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=GRAMMAR_SHA256: raise ValueError('Fixture requires the pinned Khronos grammar')
    grammar=Grammar(data);repo=Path(__file__).resolve().parents[1]
    if bool(glsl_path)!=bool(assembler):raise ValueError('GLSL controls require the grammar and assembler together')
    glsl=None
    if glsl_path:
        extended=glsl_path.read_bytes()
        if hashlib.sha256(extended).hexdigest()!=GLSL_GRAMMAR_SHA256:raise ValueError('Fixture requires the pinned GLSL grammar')
        glsl=GLSL450(extended)
    output.mkdir(parents=True,mode=0o700,exist_ok=False)
    prefix=[str(host_bridge.resolve()),'--host'] if host_bridge else []
    cases=[]
    def expect(ok):
        if not ok: raise ValueError('Unexpected static census for control '+name)
    for name,source,define in [
            ('sampled-fragment','native/tests/shader_effects_fixture.frag',None),
            ('input-attachment','native/tests/shader_effects_fixture.frag','D4VR_EFFECT_INPUT_ATTACHMENT'),
            ('buffer-image-atomic-writes','native/tests/shader_effects_fixture.comp',None),
            ('physical-pointer-write','native/tests/shader_effects_fixture.comp','D4VR_EFFECT_PHYSICAL'),
            ('stereo-helper-telemetry','native/probe_gpu/stereo_transform.vert',None)]:
        path=output/(name+'.spv');src=repo/source
        command=prefix+[str(compiler.resolve()),'-V','--target-env','vulkan1.2']
        if define: command+=['-D'+define+'=1']
        command += [str(src),'-o',str(path)]
        built=subprocess.run(command,capture_output=True,text=True,timeout=30)
        if built.returncode: raise ValueError('Control compile failed: '+built.stdout+built.stderr)
        checked=subprocess.run(prefix+[str(validator.resolve()),'--target-env','vulkan1.2',str(path)],capture_output=True,text=True,timeout=30)
        if checked.returncode: raise ValueError('Control validation failed: '+checked.stdout+checked.stderr)
        result=analyze(path.read_bytes(),grammar,glsl)
        if result['unknown_opcodes'] or result['unmodeled_resource_instructions']:
            raise ValueError('Control used unexpected unmodeled instruction')
        if result['external_memory_writes_excluded'] or result['replay_allowed']:
            raise ValueError('Static census falsely authorized replay')
        if name=='sampled-fragment':
            expect(result['image_write_instructions']==0 and not result['atomic_instructions'])
            expect(result['explicit_external_pointer_write_instructions']==0 and result['declared_direct_output_locations']==[0])
        elif name=='input-attachment':
            expect(result['input_attachment_type_declarations']==1 and result['image_instructions'].get('OpImageRead')==1)
        elif name=='buffer-image-atomic-writes':
            expect(result['explicit_external_pointer_write_instructions']>=1 and result['image_write_instructions']==1)
            expect(result['atomic_instructions'].get('OpAtomicIAdd')==1)
        elif name=='physical-pointer-write':
            expect(result['physical_storage_pointer_type_declarations']>=1)
            expect(result['pointer_write_instructions_by_storage'].get('PhysicalStorageBuffer',0)>=1)
        else:
            expect(result['pointer_write_instructions_by_storage'].get('PhysicalStorageBuffer',0)>=1)
            expect(result['atomic_instructions'].get('OpAtomicIAdd',0)>=1)
        cases.append({'case':name,'spirv_val_passed':True,'source_sha256':probe.digest(src),**result})
    if glsl:
        for name,source in [('glsl-pure','native/tests/glsl_pure_effects.spvasm'),
                ('glsl-external-pointer-writes','native/tests/glsl_pointer_effects.spvasm'),
                ('glsl-local-pointer-writes','native/tests/glsl_local_effects.spvasm'),
                ('unmodeled-debug-printf','native/tests/glsl_unknown_effects.frag')]:
            src=repo/source;path=output/(name+'.spv')
            command=prefix+([str(assembler.resolve()),'--target-env','vulkan1.2',str(src),'-o',str(path)] if src.suffix=='.spvasm'
                else [str(compiler.resolve()),'-V','--target-env','vulkan1.2',str(src),'-o',str(path)])
            built=subprocess.run(command,capture_output=True,text=True,timeout=30)
            if built.returncode:raise ValueError('Control build failed: '+built.stdout+built.stderr)
            checked=subprocess.run(prefix+[str(validator.resolve()),'--target-env','vulkan1.2',str(path)],capture_output=True,text=True,timeout=30)
            if checked.returncode:raise ValueError('Control validation failed: '+checked.stdout+checked.stderr)
            result=analyze(path.read_bytes(),grammar,glsl)
            expect(not result['external_memory_writes_excluded'] and not result['replay_allowed'])
            if name=='glsl-pure':
                expect(result['extended_instruction_effects_audited'] and len(result['extended_instruction_effects'])==15)
                expect(not result['extended_pointer_write_instructions_by_storage'])
            elif name=='glsl-external-pointer-writes':
                expect(result['extended_instruction_effects_audited'] and result['explicit_external_pointer_write_instructions']==0)
                expect(result['extended_pointer_write_instructions_by_storage']=={'StorageBuffer':2})
                expect(result['non_atomic_external_pointer_write_instructions']==2)
            elif name=='glsl-local-pointer-writes':
                expect(result['extended_instruction_effects_audited'] and result['extended_external_pointer_write_instructions']==0)
                expect(result['extended_pointer_write_instructions_by_storage']=={'Function':1,'Private':1})
            else:expect(not result['extended_instruction_effects_audited'] and result['unmodeled_extended_instruction_count']>0)
            cases.append({'case':name,'spirv_val_passed':True,'source_sha256':probe.digest(src),**result})
    # Includes the helper's telemetry even when its specialization defaults disable it.
    return {'schema_version':1,'scope':'original synthetic shader modules only; compiled and validated, never GPU executed',
            'grammar_commit':GRAMMAR_COMMIT,'grammar_sha256':grammar.sha256,'cases':cases,
            'glsl_grammar_sha256':glsl.sha256 if glsl else None,
            'source_sha256':{p:probe.digest(repo/p) for p in ('tools/shader_effects.py','tools/shader_effects_validate.py','tools/glsl_effects.py')},
            'gpu_shader_execution_performed':False,'game_stereo_submitted':False}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler',type=Path,required=True);p.add_argument('--validator',type=Path,required=True)
    p.add_argument('--grammar',type=Path,required=True);p.add_argument('--output-directory',type=Path,required=True)
    p.add_argument('--host-bridge',type=Path)
    p.add_argument('--glsl-grammar',type=Path);p.add_argument('--assembler',type=Path);args=p.parse_args()
    result=validate(args.compiler,args.validator,args.grammar,args.output_directory.resolve(),args.host_bridge,args.glsl_grammar,args.assembler)
    probe.write_json(args.output_directory/'proof.json',result)
    print(json.dumps({'synthetic_controls_passed':len(result['cases']),'gpu_shader_execution_performed':False}))


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,AssertionError,subprocess.SubprocessError) as error:
        print(f'D4VR shader audit validation: {error}');raise SystemExit(2)
