"""Apply one explicitly fixed diagnostic transform to two source projections.

The second source matrix is kept distinct and supplies its own depth and input
coordinates. This has no previous XR pose/history and is not a temporal VR mode.
"""


def rewrite(module,main,target,first_row,vector_type,types,constants,locals_,local_declarations,
            imported,void,uint,zero,unused_vector,uniform_vector_pointer,type_id,constant):
    if first_row!=72:raise ValueError('Fixed partner diagnostic requires rows 72 and 80')
    if [a[1] for op,a in module.instructions if op==54]!=[main]:
        raise ValueError('Fixed partner diagnostic requires one original function')
    scalar=types[vector_type][1][0];scalar_pointer=type_id(32,[7,scalar])
    aliases={target:()};replacements={};counts=[0,0];outputs=[]
    for group in range(2):
        rows=[module.allocate() for _ in range(4)];outputs.append(rows)
        local_declarations.extend((59,[type_id(32,[7,vector_type]),p,7]) for p in rows)
    for index,(op,a) in enumerate(module.instructions):
        if op in (65,66) and a[2] in aliases:
            aliases[a[1]]=aliases[a[2]]+tuple(constants.get(k,(0,['dynamic']))[1][0] for k in a[3:])
        elif op==83 and a[2] in aliases:aliases[a[1]]=aliases[a[2]]
        if op not in (32,59,65,66,83) and len(a)>=2 and types.get(a[0],(0,[]))[0]==32 and types[a[0]][1][0]==2:
            raise ValueError('Unreviewed uniform pointer producer')
        if op!=61 or a[2] not in aliases:continue
        path=aliases[a[2]]
        if len(path)<2 or path[0]!=0 or not isinstance(path[1],int):raise ValueError('Cannot bound fixed partner camera load')
        group=0 if 72<=path[1]<76 else 1 if 80<=path[1]<84 else None
        if group is None:continue
        row=path[1]-(72 if group==0 else 80);ptr=outputs[group][row]
        if len(a)!=3:raise ValueError('Fixed partner refuses volatile/decorated loads')
        if len(path)==2 and a[0]==vector_type:replacements[index]=[(61,[a[0],a[1],ptr])]
        elif len(path)==3 and isinstance(path[2],int) and 0<=path[2]<4 and a[0]==scalar:
            p=module.allocate();replacements[index]=[(65,[scalar_pointer,p,ptr,constant(path[2])]),(61,[scalar,a[1],p])]
        else:raise ValueError('Fixed partner load shape unavailable')
        counts[group]+=1
    if not all(counts):raise ValueError('Both original projection windows must be read')
    def entry():
        code=[(62,[locals_[0],zero])]
        code.extend((62,[v,unused_vector]) for v in locals_[5:9])
        for group,start in enumerate((72,80)):
            rows=[]
            for r in range(4):
                p,v=module.allocate(),module.allocate();rows.append(v)
                code.extend([(65,[uniform_vector_pointer,p,target,zero,constant(start+r)]),
                             (61,[vector_type,v,p]),(62,[locals_[r+1],v])])
            columns=[]
            for col in range(4):
                parts=[]
                for row in rows:
                    v=module.allocate();parts.append(v);code.append((81,[scalar,v,row,col]))
                before,after=module.allocate(),module.allocate();columns.append(after)
                code.extend([(80,[vector_type,before,*parts]),(62,[locals_[-1],before]),
                    (57,[void,module.allocate(),imported,*locals_]),(61,[vector_type,after,locals_[-1]])])
            for row in range(4):
                parts=[]
                for col in columns:
                    v=module.allocate();parts.append(v);code.append((81,[scalar,v,col,row]))
                v=module.allocate();code.extend([(80,[vector_type,v,*parts]),(62,[outputs[group][row],v])])
        return code
    output=[];inside=False;prefix=False;inserted=False
    for index,(op,a) in enumerate(module.instructions):
        if op==54:inside=a[1]==main
        if inside and op==248 and not inserted:
            output.append((op,a));output.extend(local_declarations);prefix=True;continue
        if prefix and op not in (59,8,317):output.extend(entry());prefix=False;inserted=True
        output.extend(replacements.get(index,[(op,a)]))
        if op==56:inside=False
    if not inserted:raise ValueError('Original entry block unavailable')
    return output
