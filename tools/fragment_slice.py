"""Closed backward slices of selected fragment output values.

Only the visited pure arithmetic is modeled. Other outputs, branch effects,
textures, history consumers and runtime execution remain outside this proof.
"""
import hashlib
from spirv_probe import Module,literal
from shader_write_contract import Policy
from camera_dataflow import ID_VALUES

# Extra pure numerical forms needed by the fragment's output packing. The
# pinned grammar must confirm exactly these names and IdRef counts.
EXTRA={2:('RoundEven',1),9:('Ceil',1)}


def analyze(data,policy,location=0):
    if len(data)>16*1024*1024 or not isinstance(location,int) or not 0<=location<32:raise ValueError('Fragment slice input budget')
    m=Module(data)
    if len(m.instructions)>250000:raise ValueError('Fragment instruction budget')
    entries=[a[0] for op,a in m.instructions if op==15]
    if entries!=[4]:raise ValueError('Expected one fragment entry')
    defs={};decor={};variables={};constants={};imports={};stores=[]
    for op,a in m.instructions:
        grammar=policy.core.ops.get(op,{});operands=grammar.get('operands',[])
        if operands and operands[0]['kind']=='IdResultType':defs[a[1]]=(op,a)
        if op==71:decor.setdefault(a[0],{})[a[1]]=a[2:]
        elif op==59:variables[a[1]]=a[2]
        elif op==43 and len(a)==3:constants[a[1]]=a[2]
        elif op==11:imports[a[0]]=literal(a[1:])
        elif op==62:stores.append(a)
    def pointer(i,seen=None):
        seen=set() if seen is None else seen
        if i in seen or len(seen)>64:raise ValueError('Pointer slice cycle/budget')
        if i in variables:return i,()
        op,a=defs.get(i,(None,[]))
        if op==83:return pointer(a[2],seen|{i})
        if op in (65,66):
            root,path=pointer(a[2],seen|{i})
            if any(x not in constants for x in a[3:]):raise ValueError('Dynamic slice pointer')
            return root,path+tuple(constants[x] for x in a[3:])
        raise ValueError('Unresolved slice pointer')
    selected=[]
    for a in stores:
        root,path=pointer(a[0])
        if variables[root]==3 and decor.get(root,{}).get(30)==[location]:selected.append((path,a[1]))
    if not selected:raise ValueError('No selected fragment output stores')
    results=[]
    for path,start in selected:
        todo=[start];seen=set();inputs=set();uniforms=set();ops=set()
        while todo:
            i=todo.pop()
            if i in seen:continue
            if len(seen)>10000:raise ValueError('Fragment value slice budget')
            seen.add(i);op,a=defs.get(i,(None,[]));refs=[];ops.add(op)
            if op in (41,42,43,44,46,48,49,50,51):continue
            if op==61:
                if len(a)!=3:raise ValueError('Decorated slice load')
                root,p=pointer(a[2]);d=decor.get(root,{})
                if variables[root]==1:
                    if (30 in d)==(11 in d):raise ValueError('Unknown input interface')
                    inputs.add(('location' if 30 in d else 'builtin',d.get(30,d.get(11))[0],d.get(31,[0])[0]))
                elif variables[root]==2 and len(p)>=2 and p[0]==0 and 33 in d and 34 in d:uniforms.add((d[34][0],d[33][0],p[1]))
                else:raise ValueError('Unmodeled slice memory')
            elif op==12:
                if imports.get(a[2])!='GLSL.std.450':raise ValueError('Unknown slice instruction set')
                if a[3] in EXTRA:
                    name,count=EXTRA[a[3]]
                    if len(a[4:])!=count:raise ValueError('Invalid extra pure slice signature')
                elif policy.glsl.classify(imports[a[2]],a[3],a[4:])['effect']!='none':raise ValueError('Unreviewed slice GLSL operation')
                refs=a[4:]
            elif op==81:refs=[a[2]]
            elif op in (79,82):refs=a[2:4]
            elif op==80 or op in ID_VALUES:refs=a[2:]
            elif op==83:refs=[a[2]]
            elif op==245:refs=a[2::2]
            else:raise ValueError('Unmodeled fragment slice value '+str(op))
            todo.extend(refs)
        results.append({'output_access_path':list(path),'value_instructions':len(seen),
            'input_interfaces':[{'kind':k,'value':v,'declared_component':c} for k,v,c in sorted(inputs)],
            'uniform_rows':[{'set':s,'binding':b,'row':r} for s,b,r in sorted(uniforms)],
            'value_opcodes':sorted(ops)})
    return {'schema_version':1,'shader_sha256':hashlib.sha256(data).hexdigest(),'output_location':location,'slices':results,
        'scope':'selected output values only; components and phi/select alternatives conservatively combined',
        'module_validation_performed':False,'runtime_execution_proven':False,'control_effects_proven':False,
        'temporal_semantics_proven':False,'coverage_cause_proven':False}


def validated(data,core,glsl,validator,location=0):
    import subprocess,tempfile,json
    from pathlib import Path
    policy=Policy(core,glsl)
    grammar=json.loads(glsl);rows={r['opcode']:r for r in grammar['instructions']}
    for op,(name,n) in EXTRA.items():
        if rows[op]['opname']!=name or len(rows[op]['operands'])!=n or any(o.get('kind')!='IdRef' or 'quantifier' in o for o in rows[op]['operands']):raise ValueError('Extra GLSL signature differs')
    with tempfile.TemporaryDirectory(prefix='fragment-slice-') as d:
        p=Path(d)/'snapshot.spv';p.write_bytes(data);subprocess.run([str(validator),'--target-env','vulkan1.3',str(p)],check=True,capture_output=True,timeout=30)
    result=analyze(data,policy,location);result.update(module_validation_performed=True,grammar_sha256=policy.core.sha256,glsl_grammar_sha256=policy.glsl.sha256);return result
