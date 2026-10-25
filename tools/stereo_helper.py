"""Bounded structural contract for the standalone telemetry-free helper.

Run spirv-val first. This contract applies only to the supplied helper, before
linking; it cannot authorize an original or transformed game shader for replay.
New compiler opcodes fail closed until their effects are reviewed explicitly.
"""
from __future__ import annotations
import hashlib

# Explicit memory-effect policy, not a range of presumed harmless opcodes.
STRUCTURAL = {3,4,5,6,10,11,14,15,17,19,20,21,22,23,28,30,32,33,39,
              41,42,43,44,50,52,54,55,56,57,59,61,62,65,71,72,
              245,247,248,249,250,253}
VALUE_OPS = {80,120,124,127,129,131,133,136,148,167,170,171,172,
             176,178,186,188,190,199}
TYPED = {12,41,42,43,44,50,52,54,55,57,59,61,65,245} | VALUE_OPS
MARKER = 'D4VR_HELPER_NO_TELEMETRY_1'


def verify(data: bytes) -> dict:
    from spirv_probe import Module, literal
    if len(data)>128*1024:
        raise ValueError('Stereo helper exceeds byte budget')
    module=Module(data)
    if len(module.instructions)>8192:
        raise ValueError('Stereo helper exceeds instruction budget')
    pointers={};result_types={};imports={};spec_ids={};integers={};specs={};functions=[]
    for op,args in module.instructions:
        if op not in STRUCTURAL | VALUE_OPS | {12}:
            raise ValueError(f'Unreviewed stereo helper opcode {op}')
        if op in TYPED:
            if len(args)<2 or args[1] in result_types:
                raise ValueError('Invalid helper typed result')
            result_types[args[1]]=args[0]
        if op==32:
            if len(args)!=3:raise ValueError('Invalid helper pointer type')
            pointers[args[0]]=args[1]
        elif op==21:integers[args[0]]=args[1:]
        elif op==11:
            if len(args)<2 or args[0] in imports:raise ValueError('Invalid helper import')
            imports[args[0]]=literal(args[1:])
        elif op==71 and len(args)>=2 and args[1]==1:
            if len(args)!=3 or args[0] in spec_ids:raise ValueError('Invalid helper specialization decoration')
            spec_ids[args[0]]=args[2]
        elif op==50:specs[args[1]]=(args[0],args[2:])
        elif op==54:functions.append(args[1])
        elif op==52:
            # The compiler folds only eye_state_address != uint64(0).
            if len(args)!=5 or args[2]!=171:
                raise ValueError('Unreviewed helper specialization operation')
        elif op==17 and args not in ([1],[11],[5347]):
            raise ValueError('Unreviewed helper capability')
        elif op==10 and literal(args)!='SPV_KHR_physical_storage_buffer':
            raise ValueError('Unreviewed helper extension')
    if list(spec_ids.values())!=[4] or set(specs)!=set(spec_ids):
        raise ValueError('Telemetry-free helper requires only specialization ID 4')
    spec_type,default=specs[next(iter(spec_ids))]
    if integers.get(spec_type)!=[64,0] or default!=[0,0]:
        raise ValueError('Eye address must be an unsigned 64-bit zero default')
    if not imports or any(name!='GLSL.std.450' for name in imports.values()):
        raise ValueError('Unreviewed helper extended instruction set')
    if len(functions)!=2 or len(set(functions))!=2:
        raise ValueError('Expected the standalone helper and its compiler entry point')
    writes=0
    for op,args in module.instructions:
        if op==120 and pointers.get(args[0])!=5349:
            raise ValueError('Only physical address conversion is reviewed')
        if op in VALUE_OPS-{120} and args[0] in pointers:
            raise ValueError('Unreviewed pointer-valued helper operation')
        if op==14 and args!=[5348,1]:raise ValueError('Unexpected helper memory model')
        if op==12 and (len(args)!=5 or imports.get(args[2])!='GLSL.std.450' or args[3]!=4):
            raise ValueError('Only the result-only GLSL FAbs helper instruction is reviewed')
        if op==57 and (len(args)<3 or args[2] not in functions):
            raise ValueError('Unresolved helper call')
        if op==59 and (len(args)<3 or args[2] not in (1,3,7)):
            raise ValueError('Unreviewed helper variable storage')
        if op==62:
            if not args or pointers.get(result_types.get(args[0])) not in (3,7):
                raise ValueError('Helper store is external or has an unresolved pointer')
            writes+=1
    return {'schema_version':1,'scope':'standalone helper before linking only',
            'helper_sha256':hashlib.sha256(data).hexdigest(),
            'policy':'stereo_helper_no_telemetry_v1','instruction_count':len(module.instructions),
            'local_or_output_store_instructions':writes,'specialization_ids':[4],
            'telemetry_instructions_absent':True,
            'transformed_module_external_writes_excluded':False,'replay_allowed':False}
