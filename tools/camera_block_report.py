"""Numerical partners and conservative static reads of observed camera blocks.

Neither a numerical relation nor a static load proves a pass's semantic role,
execution order, frame identity, or responsibility for missing geometry.
"""
import math
from camera_block import ROWS, matrix, product_error, relations
from spirv_probe import Module


def partners(record, tolerance=2e-5):
    if not math.isfinite(tolerance) or not 0<tolerance<=.001:
        raise ValueError('Camera partner tolerance')
    matrices={i:matrix(record['rows_bits'],i) for i in range(ROWS-3)}
    p=matrices[72]; affine=[]; perspectives=[]; pairs=[]
    for i,q in matrices.items():
        if q is None:continue
        if max(abs(q[12]),abs(q[13]),abs(q[14]),abs(q[15]-1))<1e-6:
            error=max(abs(sum(q[a*4+k]*q[c*4+k] for k in range(3))-(a==c))
                      for a in range(3) for c in range(3))
            # Skip trivial identity windows: retain rigid transforms with a
            # nontrivial translation, without assuming their coordinate space.
            if error<1e-5 and sum(abs(q[k]) for k in (3,7,11))>1:
                affine.append((i,q))
        off=[k for k in range(16) if k not in (0,2,5,6,10,11,14)]
        if p is not None and q[0]>0 and q[5]>0 and max(abs(q[k]) for k in off)<1e-7 and abs(q[14]+1)<1e-7:
            perspectives.append({'first_row':i,
                'different_components_from_primary':[k for k in range(16) if q[k]!=p[k]],
                'maximum_absolute_difference':max(abs(x-y) for x,y in zip(q,p))})
    for i,a in affine:
        for j,b in affine:
            if j<=i:continue
            error=max(product_error(a,b),product_error(b,a))
            if error<tolerance:pairs.append({'first_rows':[i,j],'max_identity_residual':error})
    return {'projection_inverse':relations(record,tolerance),
            'affine_inverse_pairs':pairs,'perspective_windows':perspectives,
            'tolerance':tolerance,'semantic_roles_proven':False,
            'frame_or_pose_identity_known':False,'coverage_cause_proven':False}


def static_reads(data, descriptor_set=3, binding=0):
    """Count directly resolvable loads; unresolved aliases remain explicit.

    This is an inventory of instructions, not an execution/dataflow proof.
    Unknown Uniform pointer producers conservatively make completeness false,
    including producers for other buffers. Do not infer absence from a partial
    inventory. The selected buffer's layout must be independently validated.
    """
    m=Module(data);decorations={};constants={};types={};aliases={};rows=[]
    for op,a in m.instructions:
        if op==71:decorations.setdefault(a[0],{})[a[1]]=a[2:]
        elif op==43 and len(a)==3:constants[a[1]]=a[2]
        elif 19<=op<=39:types[a[0]]=(op,a[1:])
    targets=[i for i,d in decorations.items() if d.get(34)==[descriptor_set] and d.get(33)==[binding]]
    if len(targets)!=1:raise ValueError('Expected one camera uniform descriptor')
    aliases[targets[0]]=();unknown_loads=0;unknown_producers=0
    for op,a in m.instructions:
        if op in (65,66) and a[2] in aliases:
            aliases[a[1]]=aliases[a[2]]+tuple(constants.get(k) for k in a[3:])
        elif op==83 and a[2] in aliases:aliases[a[1]]=aliases[a[2]]
        if op not in (32,59,65,66,83) and len(a)>=2 and types.get(a[0],(0,[]))[0]==32 and types[a[0]][1][0]==2:
            unknown_producers+=1
        if op==61 and a[2] in aliases:
            path=aliases[a[2]]
            if len(path)>=2 and path[0]==0 and isinstance(path[1],int) and 0<=path[1]<ROWS:
                rows.append(path[1])
            else:unknown_loads+=1
    return {'statically_loaded_rows':sorted(set(rows)),
            'resolved_load_instructions':len(rows),'unresolved_camera_loads':unknown_loads,
            'unreviewed_uniform_pointer_producers':unknown_producers,
            'direct_read_inventory_complete':unknown_loads==0 and unknown_producers==0,
            'runtime_branch_execution_proven':False,'output_dependency_proven':False}
