"""Closed SSA data routes from Uniform rows to vertex outputs.

This records possible arithmetic data dependencies, not execution, temporal
semantics, sensitivity, fragment effects, or a missing-geometry diagnosis.
Input components and phi/select alternatives are conservatively combined.
"""
import hashlib
from spirv_probe import Module
from shader_write_contract import Policy, assess, VALUES

# Reviewed value forms: trailing operands are IDs, except the separately
# decoded composite/extended/phi instructions. Never interpret a literal as
# an ID because its numeric value happens to match a result ID.
ID_VALUES={op for op in VALUES if 109<=op<=115 or op==124 or 126<=op<=148 or 164<=op<=200}
STRUCTURAL={0,2,3,4,5,6,7,8,10,11,14,15,16,17,19,20,21,22,23,24,28,30,32,33,41,42,43,44,46,48,49,50,51,54,56,59,71,72,247,248,249,253,317,330}
DATA={12,61,62,65,66,79,80,81,82,83,245,250}|ID_VALUES


def analyze(data, policy):
    m=Module(data);effects=assess(data,policy.glsl)
    if not effects['static_non_output_external_writes_excluded']:
        raise ValueError('Dataflow requires the reviewed non-output-write policy')
    if effects['execution_models']!=[0] or effects['functions']!=1:
        raise ValueError('Dataflow requires a single original vertex function')
    if effects['core_writes_by_storage'].keys()-{'Output'}:
        raise ValueError('Dataflow does not model local/private memory stores')
    d={};member_d={};constants={};variables={};variable_types={};types={};imports={};aliases={};graph={};seeds={};stores=[];conditions=[]
    for op,a in m.instructions:
        if op not in STRUCTURAL|DATA:raise ValueError('Unmodeled dataflow opcode '+str(op))
        if op==71:d.setdefault(a[0],{})[a[1]]=a[2:]
        elif op==72:member_d.setdefault((a[0],a[1]),{})[a[2]]=a[3:]
        elif op==43 and len(a)==3:constants[a[1]]=a[2]
        elif op==59:
            if len(a)!=3 or a[2] not in (1,2,3,9):raise ValueError('Unmodeled variable/initializer')
            variables[a[1]]=a[2];variable_types[a[1]]=a[0];aliases[a[1]]=(a[1],())
        elif 19<=op<=39:types[a[0]]=(op,a[1:])
        elif op==11:
            from spirv_probe import literal
            imports[a[0]]=literal(a[1:])
    def definition(identifier,refs=(),source=()):
        if identifier in graph:raise ValueError('Duplicate data definition')
        graph[identifier]=set(refs);seeds[identifier]=set(source)
    for op,a in m.instructions:
        if op in (41,42,43,44,46,48,49,50,51):definition(a[1])
        elif op in (65,66):
            if a[2] not in aliases:raise ValueError('Unresolved pointer origin')
            root,path=aliases[a[2]]
            if any(x not in constants for x in a[3:]):raise ValueError('Dynamic pointer path')
            aliases[a[1]]=(root,path+tuple(constants[x] for x in a[3:]))
        elif op==83:
            if a[2] in aliases:aliases[a[1]]=aliases[a[2]]
            else:definition(a[1],[a[2]])
        elif op==61:
            if len(a)!=3 or a[2] not in aliases:raise ValueError('Unmodeled load/alias')
            root,path=aliases[a[2]];storage=variables[root];source=[]
            if storage==2:
                attrs=d.get(root,{})
                if len(path)<2 or path[0]!=0 or attrs.get(34) is None or attrs.get(33) is None:
                    raise ValueError('Unresolved Uniform descriptor/row')
                source=[(attrs[34][0],attrs[33][0],path[1])]
            elif storage not in (1,9):raise ValueError('Unmodeled memory read')
            definition(a[1],source=source)
        elif op==12:
            effect=policy.glsl.classify(imports.get(a[2],''),a[3],a[4:])
            if effect['effect']!='none':raise ValueError('Unmodeled extended value effect')
            definition(a[1],a[4:])
        elif op==81:definition(a[1],[a[2]]) # remaining operands are literal indices
        elif op in (79,82):definition(a[1],a[2:4])
        elif op==80:definition(a[1],a[2:])
        elif op==245:definition(a[1],a[2::2]) # labels are not value operands
        elif op in ID_VALUES:definition(a[1],a[2:])
        elif op==62:
            if a[0] not in aliases:raise ValueError('Unresolved output pointer')
            root,path=aliases[a[0]]
            if variables[root]!=3:raise ValueError('Unmodeled store')
            attrs=d.get(root,{})
            if 30 not in attrs and 11 not in attrs and path:
                ptr=types.get(variable_types[root]);struct=ptr[1][1] if ptr and ptr[0]==32 else None
                if types.get(struct,(0,[]))[0]==30:
                    attrs=member_d.get((struct,path[0]),{});path=path[1:]
            if (30 in attrs)==(11 in attrs):raise ValueError('Unresolved direct output interface')
            stores.append((attrs,path,a[1]))
        elif op==250:conditions.append(a[0])
    def sources(start):
        todo=[start];seen=set();result=set()
        while todo:
            i=todo.pop()
            if i in seen:continue
            if len(seen)>=250000:raise ValueError('Dependency walk budget')
            seen.add(i)
            if i not in graph:raise ValueError('Unresolved data value')
            result.update(seeds[i]);todo.extend(graph[i]-seen)
        return [{'set':s,'binding':b,'row':r} for s,b,r in sorted(result)]
    outputs=[]
    for attrs,path,value in stores:
        interface={'location':attrs[30][0]} if 30 in attrs else {'builtin':attrs[11][0]}
        outputs.append({'interface':interface,'declared_component':attrs.get(31,[0])[0],
                        'access_path':list(path),'uniform_rows':sources(value)})
    return {'schema_version':1,'shader_sha256':hashlib.sha256(data).hexdigest(),
        'policy':'closed_uniform_ssa_routes_v1','scope':'all original value paths, including unreachable alternatives',
        'outputs':outputs,'branch_condition_uniform_rows':[sources(i) for i in conditions],
        'pure_ssa_data_routes_resolved':True,'control_dependencies_attributed_to_outputs':False,
        'componentwise_sensitivity_proven':False,'execution_proven':False,
        'temporal_semantics_proven':False,'fragment_effects_proven':False,
        'coverage_cause_proven':False,'module_validation_performed':False}


def validated(data, core, glsl, validator):
    policy=Policy(core,glsl);check=policy.validate(data,validator)
    if not check['validated_non_output_external_writes_excluded']:
        raise ValueError('Invalid or unreviewed dataflow shader')
    result=analyze(data,policy)
    result.update(module_validation_performed=True,grammar_sha256=policy.core.sha256,
                  glsl_grammar_sha256=policy.glsl.sha256,validator_sha256=check['validator_sha256'])
    return result
