"""Bounded, lossless camera-UBO neighborhood observation; no camera edits.

The helper uses the existing pinned VKD3D diagnostic ring, with hexadecimal
payloads rather than rounded decimal floats. Records have no frame/pose time.
"""
from pathlib import Path
import json,math,re,struct
import gpu_messages

ROWS=113
MARKER=0x4434424c
STRIDE=4096
SAMPLES=64


def helper_source():
    parameters=', '.join('vec4 c'+str(i) for i in range(ROWS))
    values=', '.join('c'+str(i) for i in range(ROWS))
    return '''#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(constant_id=0) const uint64_t shader_id=0ul;
layout(constant_id=1) const uint64_t control_address=0ul;
layout(constant_id=2) const uint64_t ring_address=0ul;
layout(constant_id=3) const uint ring_words=1u;
layout(buffer_reference,std430,buffer_reference_align=4) coherent buffer Counter {uint allocated;uint invocations;};
layout(buffer_reference,std430,buffer_reference_align=4) coherent buffer Ring {uint words[];};
void d4vr_probe(uint vertex_id, '''+parameters+'''){
    if(vertex_id!=0u || control_address==0ul || ring_address==0ul || ring_words<2048u || (ring_words&(ring_words-1u))!=0u)return;
    // Numerically selected perspective family only, not a proof of pass role.
    if(!(abs(c72.x-1.784022)<=0.00001 && abs(c73.y-3.171595)<=0.00001))return;
    Counter counters=Counter(control_address);
    if(atomicAdd(counters.invocations,0u)>=262144u)return;
    uint invocation=atomicAdd(counters.invocations,1u);
    if((invocation&4095u)!=0u || invocation>=262144u)return;
    uint sample_id=invocation>>12;
    uint start=atomicAdd(counters.allocated,1356u);
    uint mask=ring_words-1u;
    uint64_t identity=shader_id+uint64_t(vertex_id);
    Ring output_ring=Ring(ring_address);
    vec4 rows[113]=vec4[113]('''+values+''');
    for(uint row=0u;row<113u;++row){
        uint p=start+row*12u;
        output_ring.words[(p+1u)&mask]=uint(identity);
        output_ring.words[(p+2u)&mask]=uint(identity>>32);
        output_ring.words[(p+3u)&mask]=sample_id;
        output_ring.words[(p+4u)&mask]=0x4434424cu;
        output_ring.words[(p+5u)&mask]=sample_id;
        output_ring.words[(p+6u)&mask]=row;
        output_ring.words[(p+7u)&mask]=0u; // hexadecimal words, never decimal floats
        uvec4 bits=floatBitsToUint(rows[row]);
        for(uint k=0u;k<4u;++k)output_ring.words[(p+8u+k)&mask]=bits[k];
        memoryBarrierBuffer();output_ring.words[p&mask]=0xdeadca7cu;memoryBarrierBuffer();
    }
}
void main(){d4vr_probe(uint(gl_VertexIndex),'''+', '.join('vec4(0)' for _ in range(ROWS))+''');gl_Position=vec4(0);}
'''


def read_records(log: Path):
    if log.stat().st_size>64*1024*1024:raise ValueError('Camera block log exceeds 64 MiB')
    groups={};segment=0;rejected=0
    with log.open(encoding='utf8',errors='replace') as stream:
        for line in stream:
            if 'Enabling shader debug ring of size:' in line:segment+=1
            m=gpu_messages.MESSAGE.search(line)
            if not m or int(m[3])!=MARKER:continue
            shader,instance,_,sample,row,text=m.groups();sample=int(sample);row=int(row)
            tokens=[x.strip() for x in text.split(',')]
            if int(instance)!=sample or sample>=SAMPLES or row>=ROWS or len(tokens)!=4 or any(not re.fullmatch(r'#[0-9a-fA-F]{1,8}',x) for x in tokens):rejected+=1;continue
            values=[int(x[1:],16) for x in tokens];key=(segment,shader.lower().zfill(16),sample)
            if key not in groups and len(groups)>=1024:raise ValueError('Camera block group budget')
            g=groups.setdefault(key,{})
            if row in g:g['duplicate']=True;rejected+=1
            else:g[row]=values
    records=[]
    for (segment,shader,sample),g in sorted(groups.items()):
        if set(g)==set(range(ROWS)):records.append({'segment':segment,'shader_hash':shader,'sample_id':sample,'rows_bits':[g[i] for i in range(ROWS)]})
    return {'schema_version':1,'rows_per_record':ROWS,'complete_records':len(records),'incomplete_or_duplicate_groups':len(groups)-len(records),'rejected_rows':rejected,'records':records,'frame_or_pose_identity_known':False,'game_rendered_in_vr':False}


def matrix(rows,start,transpose=False):
    words=[x for r in rows[start:start+4] for x in r]
    if len(words)!=16:return None
    m=struct.unpack('<16f',struct.pack('<16I',*words))
    if not all(math.isfinite(x) for x in m):return None
    return [m[c*4+r] if transpose else m[r*4+c] for r in range(4) for c in range(4)]


def product_error(a,b):
    return max(abs(sum(a[r*4+k]*b[k*4+c] for k in range(4))-(1 if r==c else 0)) for r in range(4) for c in range(4))


def relations(record,tolerance=2e-5):
    if not math.isfinite(tolerance) or not 0<tolerance<=.001:raise ValueError('Camera inverse tolerance')
    p=matrix(record['rows_bits'],72)
    if p is None:return {'projection_finite':False,'inverse_candidates':[]}
    candidates=[]
    for row in range(ROWS-3):
        if not(row+4<=72 or row>=76):continue
        for transpose in (False,True):
            q=matrix(record['rows_bits'],row,transpose)
            if q is None:continue
            error=max(product_error(p,q),product_error(q,p))
            if error<tolerance:candidates.append({'first_row':row,'transpose':transpose,'max_identity_residual':error})
    return {'projection_finite':True,'projection_first_row':72,'inverse_candidates':candidates,'tolerance':tolerance,'inverse_semantic_role_proven':False,'pass_or_scene_role_proven':False}
