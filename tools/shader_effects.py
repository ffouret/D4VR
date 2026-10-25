#!/usr/bin/env python3
"""Inventory static SPIR-V memory instructions in private frame shader candidates.

This is a census, not an effect proof, descriptor resolver or replay authorization.
The Khronos grammar is an explicit external input, identified by its SHA-256.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import frame_trace
import probe
import shader_inventory
from spirv_probe import Module, literal
from glsl_effects import GLSL450

MAX_SHADER = 16 * 1024 * 1024
LOCAL_OR_OUTPUT = {'Function','Private','Output'}
MEMORY_MODELED = {59,60,61,62,63,64,65,66,67,68,69,70,401,402,403}
STAGES = {1:'vertex',2:'hull',4:'domain',8:'geometry',16:'pixel',32:'compute'}


class Grammar:
    def __init__(self, data: bytes):
        if len(data) > 2 * 1024 * 1024:
            raise ValueError('Oversized grammar')
        parsed = json.loads(data)
        if parsed.get('magic_number') != '0x07230203' or len(parsed['instructions']) > 8192:
            raise ValueError('Expected bounded Khronos SPIR-V core grammar')
        self.sha256 = hashlib.sha256(data).hexdigest()
        self.ops = {}
        for row in parsed['instructions']:
            opcode = row['opcode']
            if type(opcode) is not int or not 0 <= opcode <= 65535 or opcode in self.ops:
                raise ValueError('Invalid or duplicate grammar opcode')
            self.ops[opcode] = row
        self.storage = {e['value']:e['enumerant'] for k in parsed['operand_kinds']
                        if k['kind']=='StorageClass' for e in k['enumerants']}
        # These numeric instructions are interpreted below, not merely labeled.
        for opcode,name in {32:'OpTypePointer',59:'OpVariable',61:'OpLoad',62:'OpStore',
                            63:'OpCopyMemory',64:'OpCopyMemorySized',99:'OpImageWrite'}.items():
            if self.ops.get(opcode,{}).get('opname') != name:
                raise ValueError('Grammar disagrees with core memory opcode')


def analyze(data: bytes, grammar: Grammar, glsl: GLSL450 | None = None) -> dict:
    if len(data) > MAX_SHADER:
        raise ValueError('Oversized shader')
    module = Module(data)
    if len(module.instructions) > 250000:
        raise ValueError('Shader instruction budget exceeded')
    types, result_types, imports, counts, unknown = {}, {}, {}, Counter(), Counter()
    locations, variables = {}, []
    for opcode,args in module.instructions:
        row = grammar.ops.get(opcode)
        if row is None:
            unknown[str(opcode)] += 1
            continue
        counts[row['opname']] += 1
        operands = row.get('operands',[])
        if operands and operands[0]['kind']=='IdResultType':
            if len(args)<2 or len(operands)<2 or operands[1]['kind']!='IdResult':
                raise ValueError('Truncated or unsupported typed result')
            if args[1] in result_types:
                raise ValueError('Duplicate typed result ID')
            result_types[args[1]] = args[0]
        if opcode==32:
            if len(args)!=3: raise ValueError('Invalid pointer type')
            types[args[0]] = args[1]
        elif opcode==11:
            if len(args)<2: raise ValueError('Invalid instruction-set import')
            if args[0] in imports: raise ValueError('Duplicate instruction-set import ID')
            imports[args[0]] = literal(args[1:])
        elif opcode==71 and len(args)==3 and args[1]==30:
            locations[args[0]] = args[2]
        elif opcode==59:
            if len(args)<3: raise ValueError('Invalid variable')
            variables.append(args)
    reads, writes, atomics, image_ops, barriers = Counter(),Counter(),Counter(),Counter(),Counter()
    extended, unmodeled = Counter(),Counter()
    extended_effects,extended_writes,extended_forms = Counter(),Counter(),Counter()
    def storage(pointer):
        return grammar.storage.get(types.get(result_types.get(pointer)),'unknown')
    for opcode,args in module.instructions:
        row = grammar.ops.get(opcode,{})
        name,kind = row.get('opname',str(opcode)),row.get('class')
        if opcode==61:
            if len(args)<3: raise ValueError('Invalid load')
            reads[storage(args[2])] += 1
        elif opcode in (62,63,64):
            if len(args)<(3 if opcode==64 else 2): raise ValueError('Invalid store or copy')
            writes[storage(args[0])] += 1
            if opcode in (63,64): reads[storage(args[1])] += 1
        elif opcode==59 and len(args)>3:
            # An initializer is also a write; do not only count OpStore.
            writes[storage(args[1])] += 1
        if kind=='Atomic': atomics[name] += 1
        if kind=='Image': image_ops[name] += 1
        if kind=='Barrier': barriers[name] += 1
        if kind=='Memory' and opcode not in MEMORY_MODELED: unmodeled[name] += 1
        if kind in ('Reserved','Pipe','Device-Side_Enqueue','Tensor','Graph'): unmodeled[name] += 1
        if kind=='Extension' and opcode not in (10,11,12):
            # For example OpExtInstWithForwardRefsKHR is a distinct core form,
            # not an OpExtInst that can silently inherit this policy's audit.
            extended_forms[name] += 1;unmodeled[name] += 1
        if opcode==12:
            if len(args)<4: raise ValueError('Invalid extended instruction')
            instruction_set=imports.get(args[2],'unknown')
            extended[(instruction_set,args[3])] += 1
            effect=glsl.classify(instruction_set,args[3],args[4:]) if glsl else {'effect':'unknown','name':None,'written_pointer':None}
            extended_effects[(instruction_set,args[3],effect['name'],effect['effect'])] += 1
            if effect['written_pointer'] is not None:extended_writes[storage(effect['written_pointer'])] += 1
    info = shader_inventory.spirv(data)
    return {
        'schema_version':1,'scope':'static census of all module functions, including unreachable code',
        'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'grammar_sha256':grammar.sha256,
        'stages':info['stages'],'instruction_count':len(module.instructions),
        'pointer_read_instructions_by_storage':dict(sorted(reads.items())),
        'pointer_write_instructions_by_storage':dict(sorted(writes.items())),
        'explicit_external_pointer_write_instructions':sum(n for s,n in writes.items() if s not in LOCAL_OR_OUTPUT and s!='unknown'),
        'unresolved_pointer_write_instructions':writes['unknown'],
        'image_instructions':dict(sorted(image_ops.items())),
        'image_write_instructions':counts['OpImageWrite'],
        'atomic_instructions':dict(sorted(atomics.items())),
        'barrier_instructions':dict(sorted(barriers.items())),
        'input_attachment_type_declarations':sum(op==25 and len(a)>2 and a[2]==6 for op,a in module.instructions),
        'attachment_read_instructions':sum(counts[n] for n in ('OpColorAttachmentReadEXT','OpDepthAttachmentReadEXT','OpStencilAttachmentReadEXT')),
        'declared_direct_output_locations':sorted({locations[a[1]] for a in variables if a[2]==3 and a[1] in locations}),
        'output_interface_fully_resolved':False,
        'physical_storage_pointer_type_declarations':sum(grammar.storage.get(s)=='PhysicalStorageBuffer' for s in types.values()),
        'extended_instructions':[{'set':s,'instruction':i,'count':n} for (s,i),n in sorted(extended.items())],
        'glsl_grammar_sha256':glsl.sha256 if glsl else None,
        'extended_instruction_policy':'glsl450_memory_v1' if glsl else None,
        'extended_instruction_effects':[{'set':s,'instruction':op,'name':name,'memory_effect':effect,'count':n}
            for (s,op,name,effect),n in sorted(extended_effects.items())],
        'unmodeled_extended_instruction_forms':dict(sorted(extended_forms.items())),
        'unmodeled_extended_instruction_count':sum(extended_forms.values())+sum(n for (_,_,_,effect),n in extended_effects.items() if effect=='unknown'),
        'extended_pointer_write_instructions_by_storage':dict(sorted(extended_writes.items())),
        'extended_external_pointer_write_instructions':sum(n for s,n in extended_writes.items() if s not in LOCAL_OR_OUTPUT and s!='unknown'),
        'extended_unresolved_pointer_write_instructions':extended_writes['unknown'],
        'non_atomic_external_pointer_write_instructions':sum(n for s,n in (writes+extended_writes).items() if s not in LOCAL_OR_OUTPUT and s!='unknown'),
        'extended_instruction_effects_audited':glsl is not None and not extended_forms and not extended_writes['unknown'] and all(e!='unknown' for _,_,_,e in extended_effects),
        'unmodeled_resource_instructions':dict(sorted(unmodeled.items())),
        'unknown_opcodes':dict(sorted(unknown.items())),
        'declared_descriptor_resources':info['declared_resources'],
        'module_validation_performed':False,'bound_descriptor_images_resolved':False,
        'fixed_function_blend_and_masks_known':False,'external_memory_writes_excluded':False,
        'replay_allowed':False,'game_stereo_submitted':False}


def audit_frame(data: dict, scopes: list[int], directory: Path, catalog_path: Path, grammar: Grammar, glsl: GLSL450 | None = None) -> dict:
    summary = frame_trace.summarize(data)
    if (not scopes or len(scopes)>64 or len(set(scopes))!=len(scopes)
            or any(type(i) is not int or not 0<=i<len(summary['passes']) for i in scopes)):
        raise ValueError('Select 1 to 64 distinct rendering scopes')
    correlation = frame_trace.correlate(data,None,identifier_catalog=catalog_path)
    matches = {('code',s['fnv1a64'],s['code_bytes']) if 'fnv1a64' in s else ('identifier',s['module_identifier']):s['candidates']
               for s in correlation['shaders']}
    catalog = json.loads(catalog_path.read_text())
    rows = {r['catalog_id']:r for r in catalog['shaders']}
    if len(rows)!=len(catalog['shaders']): raise ValueError('Duplicate shader catalog IDs')
    pipelines = {p['id']:p for p in data['pipelines']}
    selected = sorted({p for i in scopes for p in summary['passes'][i]['pipeline_ids']})
    if len(selected)>256: raise ValueError('Selected pipeline budget exceeded')
    modules,stages,total = {},[],0
    for pid in selected:
        pipeline = pipelines.get(pid)
        if pipeline is None:
            stages.append({'pipeline_id':pid,'stage':None,'candidate_ids':[],'reason':'unknown_pipeline'})
            continue
        if pipeline.get('missing_libraries'):
            stages.append({'pipeline_id':pid,'stage':None,'candidate_ids':[],'reason':'missing_libraries'})
        if not pipeline['shaders']:
            stages.append({'pipeline_id':pid,'stage':None,'candidate_ids':[],'reason':'no_shader_stages'})
        for s in pipeline['shaders']:
            if len(stages)>=4096: raise ValueError('Selected shader stage budget exceeded')
            key = ('code',s['fnv1a64'],s['code_bytes']) if s['known'] else ('identifier',s.get('module_identifier',''))
            candidates = matches.get(key,[])
            ids = [c['catalog_id'] for c in candidates]
            stages.append({'pipeline_id':pid,'stage':s['stage'],'association':key[0],
                           'candidate_ids':ids,'byte_identity_with_game_proven':False})
            for name in ids:
                if name in modules: continue
                if len(modules)>=512: raise ValueError('Candidate shader budget exceeded')
                path = directory/(name+'.spv')
                if path.is_symlink() or path.stat().st_size>MAX_SHADER:
                    raise ValueError('Invalid shader candidate file')
                binary = path.read_bytes();total += len(binary)
                if total>256*1024*1024: raise ValueError('Candidate byte budget exceeded')
                row = rows[name]
                if len(binary)!=row['code_bytes'] or frame_trace.fnv1a64(binary)!=row['fnv1a64']:
                    raise ValueError('Candidate differs from identifier catalog')
                modules[name] = analyze(binary,grammar,glsl)
                modules[name]['catalog_fingerprint_verified'] = True
            for name in ids:
                if STAGES.get(s['stage']) not in modules[name]['stages']:
                    raise ValueError('Candidate execution model disagrees with stage')
    return {'schema_version':1,'scope':'static census of candidate shader modules associated with recorded passes',
            'requested_scopes':scopes,'pipeline_count':len(selected),'stage_associations':stages,
            'stages_without_candidate':sum(not s['candidate_ids'] for s in stages),
            'stages_with_multiple_candidates':sum(len(s['candidate_ids'])>1 for s in stages),
            'candidate_modules':modules,'candidate_module_count':len(modules),'candidate_bytes':total,
            'metadata_incomplete':summary['truncated'] or bool(summary['missing_command_buffers'] or summary['failed_or_incomplete_submissions']),
            'grammar_sha256':grammar.sha256,'module_identity_with_game_proven':False,
            'glsl_grammar_sha256':glsl.sha256 if glsl else None,
            'candidate_modules_with_unresolved_extended_effects':sum(not m['extended_instruction_effects_audited'] for m in modules.values()),
            'external_memory_writes_excluded':False,'fixed_function_blend_and_masks_known':False,
            'bound_descriptor_images_resolved':False,'replay_allowed':False,'game_stereo_submitted':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('frame',type=Path);parser.add_argument('--scopes',nargs='+',type=int,required=True)
    parser.add_argument('--shader-directory',type=Path,required=True)
    parser.add_argument('--identifier-catalog',type=Path,required=True)
    parser.add_argument('--grammar',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--glsl-grammar',type=Path)
    args=parser.parse_args()
    if args.grammar.is_symlink() or args.grammar.stat().st_size>2*1024*1024: raise ValueError('Invalid grammar file')
    if args.glsl_grammar and (args.glsl_grammar.is_symlink() or args.glsl_grammar.stat().st_size>128*1024):raise ValueError('Invalid GLSL grammar file')
    glsl=GLSL450(args.glsl_grammar.read_bytes()) if args.glsl_grammar else None
    result=audit_frame(frame_trace.read_frame(args.frame),args.scopes,args.shader_directory,
                       args.identifier_catalog,Grammar(args.grammar.read_bytes()),glsl)
    probe.write_json(args.output,result)
    args.output.chmod(0o600)
    print(json.dumps({k:v for k,v in result.items() if k not in ('candidate_modules','stage_associations')},indent=2))


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TypeError,IndexError) as error:
        print(f'D4VR shader effects: {error}');raise SystemExit(2)
