#!/usr/bin/env python3
"""Validate projection-only helper construction with absent secondary descriptors."""
from __future__ import annotations
import argparse,json,subprocess
from pathlib import Path
import probe
from spirv_probe import Module,build,literal
from stereo_helper import verify_projection_only,PROJECTION_MARKER
from stereo_helper_validate import validate as helper_controls
from shader_write_contract import Policy


def validate(compiler: Path,tools: Path,core: Path,glsl: Path,output: Path) -> dict:
    output.mkdir(parents=True,mode=0o700,exist_ok=False);repo=Path(__file__).resolve().parents[1]
    helper_controls(compiler,tools,core,glsl,output/'legacy')
    helper=output/'legacy/helper-no-telemetry.spv';contract=verify_projection_only(helper.read_bytes())
    policy=Policy(core.read_bytes(),glsl.read_bytes());cases=[]
    def compiled(src,path,defines=()):
        subprocess.run([str(compiler),'-V','--target-env','vulkan1.1','-Od',*defines,str(src),'-o',str(path)],capture_output=True,check=True,timeout=30)
        subprocess.run([str(tools/'spirv-val'),'--target-env','vulkan1.2',str(path)],capture_output=True,check=True,timeout=30)
    def bindings(module):
        sets={a[0]:a[2] for o,a in module.instructions if o==71 and a[1]==34}
        return {a[2]:a[0] for o,a in module.instructions if o==71 and a[1]==33 and sets.get(a[0])==3}
    for stem in ('mrt','mrt-vertex','single-mrt','single-mrt-vertex','source-write'):
        single=stem.startswith('single-');plain=stem.removeprefix('single-')
        if single:
            original=output/(stem+'.spv');src=repo/('native/tests/stereo_mrt'+('_vertex' if plain=='mrt-vertex' else '')+'.vert')
            compiled(src,original,('-DD4VR_SINGLE_CAMERA_BUFFER=1',))
            if set(bindings(Module(original.read_bytes())))!={0}:raise ValueError('Single-buffer shader still declares secondary descriptor')
            try:build(original,helper,output/(stem+'-strict-must-fail.spv'),tools,3,0,72,stereo=True,helper_no_telemetry=True)
            except ValueError:pass
            else:raise ValueError('Legacy construction silently bypassed its secondary requirement')
        else:original=output/'legacy'/(plain+'.spv')
        transformed=output/(stem+'-projection.spv')
        build(original,helper,transformed,tools,3,0,72,stereo=True,helper_no_telemetry=True,projection_only_helper=True)
        result=policy.validate(transformed.read_bytes(),tools/'spirv-val')
        if not result['module_validation_passed'] or result['validated_non_output_external_writes_excluded'] is (plain=='source-write'):
            raise ValueError('Projection-only shader validation/write control failed')
        markers=[literal(a) for o,a in Module(transformed.read_bytes()).instructions if o==330]
        if PROJECTION_MARKER not in markers:raise ValueError('Projection-only provenance missing')
        before=Module(original.read_bytes());caller=Module((output/'intermediate'/(stem+'-projection')/'caller.spv').read_bytes())
        if 1 in bindings(before):
            second=bindings(before)[1]
            if [a for o,a in before.instructions if o==65 and a[2]==second]!=[a for o,a in caller.instructions if o==65 and a[2]==second]:
                raise ValueError('Projection injection added secondary descriptor reads')
        cases.append({'case':stem,'secondary_descriptor_present':not single,'new_secondary_accesses':0,
            'spirv_val_passed':True,'validated_non_output_external_writes_excluded':result['validated_non_output_external_writes_excluded'],
            'source_ssbo_write_preserved':plain=='source-write','transformed_sha256':probe.digest(transformed),'replay_allowed':False})
    # A future helper using object0 remains a valid telemetry-free shader, but
    # cannot be linked in this mode. Test the actual builder, not only the checker.
    changed=output/'object-read.vert';changed.write_text((repo/'native/probe_gpu/stereo_transform.vert').read_text().replace(
        'vec4 before = clip_position;','vec4 before = clip_position + object0;'))
    changed_binary=output/'object-read.spv';compiled(changed,changed_binary,('-DD4VR_DIAGNOSTIC_TELEMETRY=0',))
    try:build(output/'single-mrt.spv',changed_binary,output/'object-read-must-not-build.spv',tools,3,0,72,
              stereo=True,helper_no_telemetry=True,projection_only_helper=True)
    except ValueError as error:
        if 'Object-matrix parameter' not in str(error):raise
    else:raise ValueError('Helper using object argument was accepted')
    return {'schema_version':1,'scope':'original synthetic construction controls only; no GPU execution',
        'helper_contract':contract,'projection_controls':cases,'valid_object_read_helper_rejected':True,
        'legacy_secondary_requirement_preserved':True,'single_buffer_originals_validated':2,
        'immutable_helper_snapshot_used_for_validation_and_link':True,
        'source_sha256':{p:probe.digest(repo/p) for p in ('tools/stereo_helper.py','tools/spirv_probe.py','tools/projection_helper_validate.py',
            'native/probe_gpu/stereo_transform.vert','native/tests/stereo_mrt.vert','native/tests/stereo_mrt_vertex.vert')},
        'replay_allowed':False,'game_stereo_submitted':False}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('compiler','tools','grammar','glsl-grammar','output-directory'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();r=validate(a.compiler.resolve(),a.tools.resolve(),a.grammar,a.glsl_grammar,a.output_directory.resolve())
    probe.write_json(a.output_directory/'proof.json',r)
    print(json.dumps({'projection_controls':len(r['projection_controls']),'object_read_helper_rejected':True}))


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,IndexError,subprocess.SubprocessError) as error:
        print(f'D4VR projection helper: {error}');raise SystemExit(2)
