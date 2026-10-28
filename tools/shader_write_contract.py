#!/usr/bin/env python3
"""Bounded full-module exclusion of writes outside invocation-local/stage output storage.

This is one static precondition, not a pass replay permission. The CLI validates
an immutable byte snapshot with spirv-val before issuing a validated conclusion.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from spirv_probe import Module,literal
from glsl_effects import GLSL450
from shader_effects import Grammar,analyze,MAX_SHADER
from shader_effects_validate import GRAMMAR_SHA256,GLSL_GRAMMAR_SHA256

# Reviewed operations are explicit. Grammar classes/ranges never grant coverage.
STRUCTURAL={
 0:'OpNop',2:'OpSourceContinued',3:'OpSource',4:'OpSourceExtension',5:'OpName',6:'OpMemberName',7:'OpString',8:'OpLine',
 10:'OpExtension',11:'OpExtInstImport',12:'OpExtInst',14:'OpMemoryModel',15:'OpEntryPoint',16:'OpExecutionMode',17:'OpCapability',
 19:'OpTypeVoid',20:'OpTypeBool',21:'OpTypeInt',22:'OpTypeFloat',23:'OpTypeVector',24:'OpTypeMatrix',25:'OpTypeImage',
 26:'OpTypeSampler',27:'OpTypeSampledImage',28:'OpTypeArray',29:'OpTypeRuntimeArray',30:'OpTypeStruct',32:'OpTypePointer',33:'OpTypeFunction',39:'OpTypeForwardPointer',
 41:'OpConstantTrue',42:'OpConstantFalse',43:'OpConstant',44:'OpConstantComposite',46:'OpConstantNull',
 48:'OpSpecConstantTrue',49:'OpSpecConstantFalse',50:'OpSpecConstant',51:'OpSpecConstantComposite',52:'OpSpecConstantOp',
 54:'OpFunction',55:'OpFunctionParameter',56:'OpFunctionEnd',57:'OpFunctionCall',59:'OpVariable',61:'OpLoad',62:'OpStore',
 63:'OpCopyMemory',64:'OpCopyMemorySized',65:'OpAccessChain',66:'OpInBoundsAccessChain',67:'OpPtrAccessChain',68:'OpArrayLength',70:'OpInBoundsPtrAccessChain',
 71:'OpDecorate',72:'OpMemberDecorate',
 245:'OpPhi',246:'OpLoopMerge',247:'OpSelectionMerge',248:'OpLabel',249:'OpBranch',250:'OpBranchConditional',251:'OpSwitch',
 252:'OpKill',253:'OpReturn',254:'OpReturnValue',255:'OpUnreachable',317:'OpNoLine',330:'OpModuleProcessed',5380:'OpDemoteToHelperInvocation',
}
VALUES={
 79:'OpVectorShuffle',80:'OpCompositeConstruct',81:'OpCompositeExtract',82:'OpCompositeInsert',83:'OpCopyObject',
 86:'OpSampledImage',87:'OpImageSampleImplicitLod',88:'OpImageSampleExplicitLod',95:'OpImageFetch',98:'OpImageRead',100:'OpImage',
 109:'OpConvertFToU',110:'OpConvertFToS',111:'OpConvertSToF',112:'OpConvertUToF',113:'OpUConvert',114:'OpSConvert',115:'OpFConvert',
 120:'OpConvertUToPtr',124:'OpBitcast',126:'OpSNegate',127:'OpFNegate',128:'OpIAdd',129:'OpFAdd',130:'OpISub',131:'OpFSub',132:'OpIMul',133:'OpFMul',
 134:'OpUDiv',135:'OpSDiv',136:'OpFDiv',137:'OpUMod',138:'OpSRem',139:'OpSMod',140:'OpFRem',141:'OpFMod',
 142:'OpVectorTimesScalar',143:'OpMatrixTimesScalar',144:'OpVectorTimesMatrix',145:'OpMatrixTimesVector',146:'OpMatrixTimesMatrix',147:'OpOuterProduct',148:'OpDot',
 164:'OpLogicalEqual',165:'OpLogicalNotEqual',166:'OpLogicalOr',167:'OpLogicalAnd',168:'OpLogicalNot',169:'OpSelect',
 170:'OpIEqual',171:'OpINotEqual',172:'OpUGreaterThan',173:'OpSGreaterThan',174:'OpUGreaterThanEqual',175:'OpSGreaterThanEqual',
 176:'OpULessThan',177:'OpSLessThan',178:'OpULessThanEqual',179:'OpSLessThanEqual',180:'OpFOrdEqual',181:'OpFUnordEqual',
 182:'OpFOrdNotEqual',183:'OpFUnordNotEqual',184:'OpFOrdLessThan',185:'OpFUnordLessThan',186:'OpFOrdGreaterThan',187:'OpFUnordGreaterThan',
 188:'OpFOrdLessThanEqual',189:'OpFUnordLessThanEqual',190:'OpFOrdGreaterThanEqual',191:'OpFUnordGreaterThanEqual',
 194:'OpShiftRightLogical',195:'OpShiftRightArithmetic',196:'OpShiftLeftLogical',197:'OpBitwiseOr',198:'OpBitwiseXor',199:'OpBitwiseAnd',200:'OpNot',
 207:'OpDPdx',208:'OpDPdy',209:'OpFwidth',210:'OpDPdxFine',211:'OpDPdyFine',212:'OpFwidthFine',213:'OpDPdxCoarse',214:'OpDPdyCoarse',215:'OpFwidthCoarse',
}
OPS=STRUCTURAL|VALUES
TYPED=set(VALUES)|{12,41,42,43,44,46,48,49,50,51,52,54,55,57,59,61,65,66,67,68,70,245}
SPEC_VALUES={79,80,81,82,83,113,114,124,126,128,130,132,134,135,137,138,139,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,194,195,196,197,198,199,200}
POINTER_VALUES={55,57,59,61,65,66,67,70,83,120,169,245}
LOCAL_OUTPUT={3,6,7}
STORAGE={0:'UniformConstant',1:'Input',2:'Uniform',3:'Output',6:'Private',7:'Function',9:'PushConstant',12:'StorageBuffer',5349:'PhysicalStorageBuffer'}
CAPABILITIES={1,11,40,50,51,5302,5347,5379}
EXTENSIONS={'SPV_KHR_physical_storage_buffer','SPV_EXT_descriptor_indexing','SPV_EXT_demote_to_helper_invocation','SPV_KHR_storage_buffer_storage_class'}
DECORATIONS={0,1,2,3,4,5,6,7,11,13,14,16,17,18,19,20,21,23,24,25,30,31,32,33,34,35,42,43,5355,5356}


def assess(data: bytes,glsl: GLSL450) -> dict:
    """Static analysis only. A true result requires independent module validation."""
    if len(data)>MAX_SHADER:raise ValueError('Shader byte budget exceeded')
    module=Module(data)
    if len(module.instructions)>250000:raise ValueError('Shader instruction budget exceeded')
    pointers={};types={};imports={};functions=set();reasons=Counter();stores=Counter();extended=Counter();stages=[]
    def block(reason):reasons[reason]+=1
    for op,a in module.instructions:
        if op not in OPS:block('unreviewed_opcode:'+str(op));continue
        if op in TYPED:
            if len(a)<2 or a[1] in types:raise ValueError('Malformed typed result')
            types[a[1]]=a[0]
        if op==32:
            if len(a)!=3:raise ValueError('Malformed pointer type')
            pointers[a[0]]=a[1]
            if a[1] not in STORAGE:block('unreviewed_pointer_storage:'+str(a[1]))
        elif op==11:
            if len(a)<2 or a[0] in imports:raise ValueError('Malformed extended import')
            imports[a[0]]=literal(a[1:])
            if imports[a[0]]!='GLSL.std.450':block('unreviewed_extended_set')
        elif op==54:functions.add(a[1])
        elif op==15:
            if len(a)<3:raise ValueError('Malformed entry point')
            stages.append(a[0])
        elif op==17 and (len(a)!=1 or a[0] not in CAPABILITIES):block('unreviewed_capability')
        elif op==10 and literal(a) not in EXTENSIONS:block('unreviewed_extension')
        elif op==14 and a not in ([0,1],[5348,1]):block('unreviewed_memory_model')
        elif op==16 and (len(a)!=2 or a[1]!=7):block('unreviewed_execution_mode')
        elif op==71 and (len(a)<2 or a[1] not in DECORATIONS):block('unreviewed_decoration')
        elif op==72 and (len(a)<3 or a[2] not in DECORATIONS):block('unreviewed_member_decoration')
        elif op==52 and (len(a)<3 or a[2] not in SPEC_VALUES):block('unreviewed_specialization_operation')
    if len(stages)!=1 or stages[0] not in (0,4):block('unsupported_entry_points')
    def storage(pointer):return pointers.get(types.get(pointer))
    def write(pointer,counts):
        s=storage(pointer);counts[STORAGE.get(s,'unknown')]+=1
        if s not in LOCAL_OUTPUT:block('non_output_external_or_unresolved_write')
    for op,a in module.instructions:
        if op not in OPS:continue
        if op in TYPED and a[0] in pointers and op not in POINTER_VALUES:
            block('unreviewed_pointer_result')
        if op==120 and pointers.get(a[0])!=5349:block('unreviewed_address_conversion')
        if op in (65,66,67,70):
            if len(a)<3 or pointers.get(a[0])!=storage(a[2]):block('unresolved_pointer_derivation')
        if op==57 and (len(a)<3 or a[2] not in functions):block('unresolved_call')
        if op in (62,63,64):
            if len(a)<(3 if op==64 else 2):raise ValueError('Malformed write or copy')
            write(a[0],stores)
        if op==59:
            if len(a)<3:raise ValueError('Malformed variable')
            if pointers.get(a[0])!=a[2]:block('variable_storage_mismatch')
            if len(a)>3:write(a[1],stores)
        if op==12:
            if len(a)<4:raise ValueError('Malformed extended instruction')
            effect=glsl.classify(imports.get(a[2],'unknown'),a[3],a[4:])
            if effect['effect']=='unknown':block('unreviewed_extended_instruction')
            if effect['written_pointer'] is not None:write(effect['written_pointer'],extended)
    return {'schema_version':1,'policy':'shader_non_output_writes_v1','sha256':hashlib.sha256(data).hexdigest(),
        'scope':'all module functions; excludes invocation-local memory and stage outputs from external writes',
        'bytes':len(data),'instructions':len(module.instructions),'functions':len(functions),'execution_models':stages,
        'core_writes_by_storage':dict(sorted(stores.items())),'extended_writes_by_storage':dict(sorted(extended.items())),
        'blocking_reasons':dict(sorted(reasons.items())),'static_non_output_external_writes_excluded':not reasons,
        'module_validation_performed':False,'validated_non_output_external_writes_excluded':False,
        'descriptor_and_attachment_aliases_resolved':False,'fixed_state_and_dependencies_audited':False,
        'replay_allowed':False,'game_stereo_submitted':False}


class Policy:
    def __init__(self,core: bytes,glsl: bytes):
        if hashlib.sha256(core).hexdigest()!=GRAMMAR_SHA256 or hashlib.sha256(glsl).hexdigest()!=GLSL_GRAMMAR_SHA256:
            raise ValueError('Write contract requires the pinned Khronos grammars')
        self.core=Grammar(core);self.glsl=GLSL450(glsl)
        for op,name in OPS.items():
            row=self.core.ops.get(op,{})
            operands=row.get('operands',[])
            typed=bool(operands and operands[0]['kind']=='IdResultType')
            if row.get('opname')!=name or typed!=(op in TYPED):raise ValueError('Core grammar disagrees with write policy')

    def validate(self,data: bytes,validator: Path,target_env: str='vulkan1.3') -> dict:
        if target_env not in ('vulkan1.1','vulkan1.2','vulkan1.3'):raise ValueError('Unsupported validation environment')
        result=assess(data,self.glsl)
        # Never validate a path and subsequently re-read it. The validator and
        # analysis consume the same immutable byte snapshot.
        with tempfile.TemporaryDirectory(prefix='d4vr-write-') as name:
            path=Path(name)/'snapshot.spv';path.write_bytes(data);path.chmod(0o600)
            checked=subprocess.run([str(validator.resolve()),'--target-env',target_env,str(path)],capture_output=True,text=True,timeout=30)
        result.update(grammar_sha256=self.core.sha256,glsl_grammar_sha256=self.glsl.sha256,
                      policy_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                      glsl_model_source_sha256=hashlib.sha256(Path(__file__).with_name('glsl_effects.py').read_bytes()).hexdigest(),
                      validator_sha256=hashlib.sha256(validator.read_bytes()).hexdigest(),target_env=target_env,
                      module_validation_performed=True,module_validation_passed=checked.returncode==0)
        if checked.returncode:
            result['validation_error']=(checked.stdout+checked.stderr)[:2048]
        result['validated_non_output_external_writes_excluded']=checked.returncode==0 and result['static_non_output_external_writes_excluded']
        result['static_census']=analyze(data,self.core,self.glsl)
        return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('shader',type=Path)
    for name in ('validator','grammar','glsl-grammar','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--target-env',default='vulkan1.3',choices=('vulkan1.1','vulkan1.2','vulkan1.3'));a=p.parse_args()
    if a.shader.is_symlink() or a.shader.stat().st_size>MAX_SHADER:raise ValueError('Invalid shader input')
    if a.grammar.stat().st_size>2*1024*1024 or a.glsl_grammar.stat().st_size>128*1024:raise ValueError('Oversized grammar')
    result=Policy(a.grammar.read_bytes(),a.glsl_grammar.read_bytes()).validate(a.shader.read_bytes(),a.validator,a.target_env)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');a.output.chmod(0o600)
    print(json.dumps({k:result[k] for k in ('module_validation_passed','validated_non_output_external_writes_excluded','blocking_reasons','replay_allowed')}))


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,IndexError,TypeError,subprocess.SubprocessError) as error:
        print(f'D4VR write contract: {error}');raise SystemExit(2)
