#!/usr/bin/env python3
"""Independent finite-UV pixel control; private modules remain outside the repo.

The original sample coordinates are never changed. The diagnostic replaces R/G
only; B/A must still match the original sampled output. This validates owned
resources, not a game's descriptors, last writer, coverage or XR submission.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess

import sampled_writer_validate as base

# Fixed BEFORE execution. This empirical finite-case budget is not a universal
# Vulkan interpolation guarantee. UNORM conversion permits floor OR ceil.
CODE_ARITHMETIC_BUDGET = 0.005
PIPELINE = dict(blend_enable=False, logic_op_enable=False, color_write_mask=15,
                sample_count=1, load_op='LOAD', store_op='STORE',
                rasterizer_discard=False, cull_mode=0, fragment_shading_rate=False)
NAMES = tuple(base.CASES) + ('negative_height', 'finite_overflow')
GROUPS = ('original', 'diagnostic', 'buffer-original', 'buffer-diagnostic')
SOURCES = ('native/tests/sampled_writer_gpu.cpp', 'native/tests/sampled_uv_gpu.cpp',
           'native/tests/sampled_writer_hash.hpp', 'tools/sampled_uv_probe.py',
           'tests/test_sampled_uv_probe.py', 'tools/sampled_uv_validate.py',
           'tests/test_sampled_uv_validate.py', 'tools/sampled_writer_validate.py')


def equal(a, b, label):
    base.check_equal(a, b, label)


def state(name):
    if name in base.CASES:
        return base.written_state(name)
    template, pc, ubo = base.written_state('B_clamp_offset' if name == 'negative_height' else 'B_repeat_offset')
    template['intended_reference'] = name
    if name == 'negative_height':
        template['viewport'] = [0, base.HEIGHT, base.WIDTH, -base.HEIGHT]
    elif name == 'finite_overflow':
        template.update(shift_texels=[24, -24], ubo_row0_xy=[3, -3])
        data = bytearray(ubo); struct.pack_into('<2f', data, 0, 3, -3); ubo = bytes(data)
    else:
        raise ValueError('Unknown fixed control')
    return template, pc, ubo


def occupied(x, y, viewport, scissor):
    vx, vy, vw, vh = viewport
    sx, sy, sw, sh = scissor
    return vx <= x < vx+vw and min(vy, vy+vh) <= y < max(vy, vy+vh) and sx <= x < sx+sw and sy <= y < sy+sh


def uv_at(x, y, viewport, shift):
    vx, vy, vw, vh = viewport
    return ((x+.5-vx)/vw + shift[0]/8,
            1-(y+.5-vy)/vh + shift[1]/8)


def encode_bounds(value):
    if not math.isfinite(value):
        raise ValueError('Nonfinite GPU behavior is not certified')
    if value < -1:
        return 0, 0
    if value > 2:
        return 255, 255
    code = 1 + 253*((value+1)/3)
    return max(1, math.floor(code-CODE_ARITHMETIC_BUDGET)), min(254, math.ceil(code+CODE_ARITHMETIC_BUDGET))


def decode_interval(code):
    if type(code) is not int or not 1 <= code <= 254:
        raise ValueError('Sentinel/noninteger code has no numeric UV interpretation')
    lo = -1 + 3*(code-2-CODE_ARITHMETIC_BUDGET)/253
    hi = -1 + 3*(code+CODE_ARITHMETIC_BUDGET)/253
    return max(-1, lo), min(2, hi)


def original_rows(name):
    s, _, _ = state(name)
    viewport, scissor = s['viewport'], s['scissor']; vx, vy, vw, vh = viewport
    sx, sy, sw, sh = scissor; shift = s['shift_texels']; sampler = s['sampler_index']
    texture = base.texture_bytes(s['texture_index'])
    sentinel = bytes((7, 5, 3, 255))*base.WIDTH; cache = {}
    def index(n):
        return n % 8 if sampler else min(7, max(0, n))
    for y in range(base.HEIGHT):
        if not (min(vy,vy+vh) <= y < max(vy,vy+vh) and sy <= y < sy+sh):
            yield sentinel; continue
        # Signed integer rational from the pixel center, not shader output.
        ty = ((2*vh-2*(y-vy)-1)*8)//(2*vh) + shift[1]
        ty = index(ty)
        if ty not in cache:
            row = bytearray(sentinel)
            for x in range(max(0,vx,sx), min(base.WIDTH,vx+vw,sx+sw)):
                tx = index(((2*(x-vx)+1)*8)//(2*vw)+shift[0])
                r,g,b,a = texture[(ty*8+tx)*4:(ty*8+tx+1)*4]
                row[x*4:x*4+4] = bytes((b,g,r,a))
            cache[ty] = bytes(row)
        yield cache[ty]


def audit_pixels(original, diagnostic, name):
    equal(len(original), base.BYTE_COUNT, 'original bytes'); equal(len(diagnostic), base.BYTE_COUNT, 'diagnostic bytes')
    s, _, _ = state(name); viewport,scissor,shift = s['viewport'],s['scissor'],s['shift_texels']
    expected = hashlib.sha256(); bad_original = bad_ba = bad_uv = 0; outside = encoded = 0
    max_code_error = 0.0
    # R's independent code bounds depend only on x in these affine controls.
    r_bounds = [encode_bounds(uv_at(x,0,viewport,shift)[0]) for x in range(base.WIDTH)]
    for y, row in enumerate(original_rows(name)):
        expected.update(row); start=y*base.WIDTH*4
        actual=original[start:start+len(row)]; diag=diagnostic[start:start+len(row)]
        bad_original += sum(a != b for a,b in zip(actual,row)) if actual != row else 0
        if diag[0::4] != actual[0::4]: bad_ba += sum(a!=b for a,b in zip(diag[0::4],actual[0::4]))
        if diag[3::4] != actual[3::4]: bad_ba += sum(a!=b for a,b in zip(diag[3::4],actual[3::4]))
        vlo,vhi=encode_bounds(uv_at(0,y,viewport,shift)[1])
        for x in range(base.WIDTH):
            i=x*4
            if not occupied(x,y,viewport,scissor):
                outside+=1; bad_uv += diag[i+1:i+3] != row[i+1:i+3]; continue
            encoded+=1; ulo,uhi=r_bounds[x]; r,g=diag[i+2],diag[i+1]
            bad_uv += not (ulo<=r<=uhi and vlo<=g<=vhi)
            if ulo not in (0,255):
                ideal=1+253*((uv_at(x,y,viewport,shift)[0]+1)/3);max_code_error=max(max_code_error,abs(r-ideal))
    if bad_original or bad_ba or bad_uv:
        raise ValueError(f'Pixel control failed {name}: original={bad_original}, B/A={bad_ba}, encoded/outside={bad_uv}')
    return dict(name=name,original_sha256=base.digest(original),diagnostic_sha256=base.digest(diagnostic),
                reference_sha256=expected.hexdigest(),original_exact_bytes=base.BYTE_COUNT,
                preserved_ba_exact_bytes=base.BYTE_COUNT//2,encoded_pixels=encoded,
                unchanged_load_pixels=outside,maximum_observed_r_code_error=max_code_error)


def validate_placement(p):
    equal(p['owned_exact_slots'],True,'owned placement')
    alignment=p['alignment']
    if type(alignment) is not int or alignment<=0 or alignment>1024*1024 or alignment&(alignment-1): raise ValueError('Descriptor alignment')
    for key in ('offsets','sizes','binding_offsets'):
        values=p[key]
        if not isinstance(values,list) or len(values)!=4 or any(type(v) is not int or not 0<=v<=1024*1024 for v in values):raise ValueError('Descriptor nonnegative bounded integer placement')
    for i in range(4):
        if p['offsets'][i]%alignment or (i and p['offsets'][i]<p['offsets'][i-1]+p['sizes'][i-1]):raise ValueError('Descriptor layout overlap')
    if p['binding_offsets'][2]!=0:raise ValueError('Empty set has no queried binding')
    total=p['offsets'][3]+p['sizes'][3]
    if not 0<total<=1024*1024:raise ValueError('Descriptor total bound')
    for i,count,key in ((0,2,'sampler_descriptor_size'),(1,2,'image_descriptor_size'),(3,1,'uniform_descriptor_size')):
        stride=p[key]
        if type(stride) is not int or not 0<stride<=1024*1024 or p['binding_offsets'][i]+count*stride>p['sizes'][i]:raise ValueError('Descriptor footprint')
    return total


def audit_directory(directory, modules):
    directory=Path(directory); data={}; manifests={}; counts=[]
    for group in GROUPS:
        folder=directory/group; m=base.decode(base.read(folder/'manifest.json'))
        diagnostic='diagnostic' in group; db=group.startswith('buffer-')
        equal(m['schema_version'],1,'schema');equal(m['owned_fixture'],True,'owned');equal(m['game_rendered'],False,'game')
        equal(m['diagnostic_uv'],diagnostic,'diagnostic');equal(m['descriptor_buffer'],db,'backend')
        equal([m['width'],m['height'],m['format']],[2560,1440,44],'target')
        equal(m['pipeline'],PIPELINE,'pipeline')
        expected_features={key:True for key in base.FEATURES};expected_features.update(descriptorBuffer=db,synchronization2=db)
        equal(m['features'],expected_features,'enabled features')
        selected=[modules[0],modules[2 if diagnostic else 1]]
        equal(m['modules'],selected,'modules')
        equal(m['validation']['synchronization_validation'],True,'sync validation');equal(m['validation']['errors'],0,'validation')
        log=base.read(folder/'validation.log').decode()
        if any(x in log for x in base.ERROR_MARKERS): raise ValueError('Validation error marker')
        if db:
            base.read(folder/'descriptor-buffer.bin',validate_placement(m['descriptor_placement']))
        names=('B_repeat_offset',) if db else NAMES
        equal([c['name'] for c in m['cases']],list(names),'cases');equal(m['submissions'],len(names)+1,'submissions')
        for case in m['cases']:
            name=case['name'];s,pc,ubo=state(name)
            for key,value in s.items():equal(case[key],value,'state '+key)
            for key,value in (('pc',pc),('ubo',ubo)):
                equal(base.read(folder/case[key+'_file']),value,key+' data');equal(case[key+'_sha256'],base.digest(value),key+' digest')
            raw=base.read(folder/case['raw_file'],base.BYTE_COUNT);equal(case['sha256'],base.digest(raw),'pixels hash');equal(case['bytes'],len(raw),'bytes')
            data[group,name]=raw
        manifests[group]=base.digest(base.read(folder/'manifest.json'));counts.append(m['validation']['warnings'])
    results=[]
    for backend, names in (('',NAMES),('buffer-',('B_repeat_offset',))):
        for name in names:results.append(dict(backend='descriptor_buffer' if backend else 'descriptor_sets',**audit_pixels(data[backend+'original',name],data[backend+'diagnostic',name],name)))
    # Changes in image/sampler must affect live sampled B; coordinates stay same.
    for wrong,intended in (('wrong_image_word','B_clamp_offset'),('wrong_sampler_word','B_repeat_offset')):
        a,b=data['diagnostic',wrong],data['diagnostic',intended]
        if a[0::4]==b[0::4] or a[1::4]!=b[1::4] or a[2::4]!=b[2::4]:raise ValueError('Sample/coordinate causal witness absent')
    if data['diagnostic','wrong_ubo_component'][2::4]==data['diagnostic','B_clamp_offset'][2::4]:raise ValueError('Wrong UBO component witness absent')
    return dict(owned_uv_control_passed=True,cases=results,manifest_sha256=manifests,
                total_readback_bytes=24*base.BYTE_COUNT,validation_errors=0,validation_warnings=counts,
                game_rendered=False,game_pixel_cause_proven=False,xr_submission=False,
                nonfinite_gpu_behavior_certified=False,universal_interpolation_bound_claimed=False)


def paired_hashes(audit):
    return [[c['original_sha256'], c['diagnostic_sha256']] for c in audit['cases']]


def run(args):
    from sampled_uv_probe import verify_candidate
    root=Path(__file__).resolve().parents[1]; expected=[args.vertex_sha256,args.fragment_sha256,base.digest(base.read(args.candidate))]
    paths=[args.vertex,args.fragment,args.candidate];modules=[base.verify_module(path,sha,stage) for path,sha,stage in zip(paths,expected,(1,16,16))]
    certificate=verify_candidate(base.read(args.fragment),base.read(args.candidate),args.fragment_sha256)
    if args.output.exists():raise ValueError('Require fresh output')
    args.output.mkdir(parents=True,mode=0o700);validator_hash=base.digest(base.read(args.validator.resolve()));fixture_hash=base.digest(base.read(args.fixture));frozen={s:base.digest(base.read(root/s)) for s in SOURCES}
    def stable():
        equal(base.digest(base.read(args.validator.resolve())),validator_hash,'frozen validator');equal(base.digest(base.read(args.fixture)),fixture_hash,'frozen fixture');equal({s:base.digest(base.read(root/s)) for s in SOURCES},frozen,'frozen sources')
        for path,sha in zip(paths,expected):equal(base.digest(base.read(path)),sha,'frozen module')
    base.write_json(args.output/'private-inputs.json',dict(modules=modules,constructor=certificate,source_sha256=frozen,fixture_sha256=fixture_hash,code_arithmetic_budget=CODE_ARITHMETIC_BUDGET,validator_sha256=validator_hash))
    for index,path in enumerate(paths):
        stable();command=base.host_command()+[str(args.validator.resolve()),"--target-env","vulkan1.3",str(path.resolve())];result=subprocess.run(command,capture_output=True,text=True,timeout=30)
        (args.output/("module-"+str(index)+"-validation.log")).write_text(result.stdout+result.stderr)
        if result.returncode:raise ValueError("SPIR-V preparation refused before GPU")
        stable()
    audits=[]
    for env,runtime in (('native',None),('steam-runtime',args.runtime)):
        stable();command=['/usr/bin/env'];clear={'VK_LAYER_PATH','VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','LD_PRELOAD'}|{k for k in os.environ if k.startswith('D4VR_')}
        for key in sorted(clear):command+=['-u',key]
        command+=['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(args.validation_layer.resolve()),'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer']
        if runtime:command+=[str(runtime.resolve()),'--batch','--']
        command+=[str(args.fixture.resolve()),'--vertex',modules[0]['path'],'--fragment',modules[1]['path'],'--candidate',modules[2]['path'],'--output',str((args.output/env).resolve())]
        command=base.host_command()+command;base.write_json(args.output/(env+'-command.json'),command)
        result=subprocess.run(command,capture_output=True,text=True,timeout=180);log=result.stdout+result.stderr;(args.output/(env+'.log')).write_text(log)
        if result.returncode or any(x in log for x in base.ERROR_MARKERS):raise ValueError('GPU control failed; inspect '+env+'.log')
        equal(base.decode(result.stdout.strip()),dict(sampled_uv_gpu_passed=True,groups=4,readbacks=24,validation_errors=0),'completion')
        stable();audit=audit_directory(args.output/env,modules);base.write_json(args.output/(env+'-audit.json'),audit);audits.append(dict(environment=env,**audit))
    stable()
    equal(paired_hashes(audits[0]),paired_hashes(audits[1]),'native/runtime bytes')
    proof=dict(schema_version=1,owned_uv_control_passed=True,source_sha256=frozen,fixture_sha256=fixture_hash,
               environments=audits,physical_readbacks=48,total_readback_bytes=48*base.BYTE_COUNT,
               validation_errors=0,native_runtime_bytes_equal=True,game_bundle_authorized=False,
               game_rendered=False,coverage_repaired=False,xr_submission=False,nonfinite_gpu_behavior_certified=False)
    base.write_json(args.output/'proof.json',proof);return proof


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fixture',type=Path,required=True)
    for key in ('vertex','fragment','candidate','validation-layer','runtime','output','validator'):p.add_argument('--'+key,type=Path,required=True)
    for key in ('vertex','fragment'):p.add_argument('--'+key+'-sha256',required=True)
    a=p.parse_args();result=run(a);print(json.dumps({k:v for k,v in result.items() if k not in ('source_sha256','environments')},indent=2))
if __name__=='__main__':main()
