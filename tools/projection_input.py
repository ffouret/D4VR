"""Closed, invocation-local rewrite of canonical projection uniform loads.

The original uniform buffer is never written. All admitted original projection
loads use the same four transformed rows, before any original computation.
This is not an audit of source shader effects or upstream engine culling.
"""


def rewrite(module,main,target,first_row,vector_type,types,constants,locals_,local_declarations,
            imported,void,uint,zero,unused_vector,uniform_vector_pointer,type_id,constant):
    if [a[1] for op,a in module.instructions if op==54]!=[main]:
        raise ValueError('Projection input requires a single original entry function')
    scalar=types[vector_type][1][0]
    scalar_pointer=type_id(32,[7,scalar])
    aliases={target:()};replacements={};replaced=0
    for index,(op,a) in enumerate(module.instructions):
        if op in (65,66) and a[2] in aliases:
            aliases[a[1]]=aliases[a[2]]+tuple(constants.get(k,(0,['dynamic']))[1][0] for k in a[3:])
        elif op==83 and a[2] in aliases:aliases[a[1]]=aliases[a[2]]
        # Uniform pointer arithmetic/select/phi cannot introduce an untracked
        # alias, even in a branch we expect not to execute.
        if op not in (32,59,65,66,83) and len(a)>=2 and types.get(a[0],(0,[]))[0]==32:
            if types[a[0]][1][0]==2:raise ValueError('Unreviewed uniform pointer producer')
        if op!=61 or a[2] not in aliases:continue
        path=aliases[a[2]]
        if len(path)<2 or path[0]!=0 or not isinstance(path[1],int):
            raise ValueError('Projection input cannot bound a camera-buffer load')
        if not first_row<=path[1]<first_row+4:continue
        row=path[1]-first_row
        if len(a)!=3:raise ValueError('Projection input does not rewrite decorated/volatile loads')
        if len(path)==2 and a[0]==vector_type:
            replacements[index]=[(61,[a[0],a[1],locals_[row+1]])]
        elif len(path)==3 and isinstance(path[2],int) and 0<=path[2]<4 and a[0]==scalar:
            ptr=module.allocate();replacements[index]=[(65,[scalar_pointer,ptr,locals_[row+1],constant(path[2])]),(61,[scalar,a[1],ptr])]
        else:raise ValueError('Projection input load shape unavailable')
        replaced+=1
    if not replaced:raise ValueError('No original projection loads to transform')

    def entry():
        code=[(62,[locals_[0],zero])];rows=[]
        for r in range(4):
            ptr,value=module.allocate(),module.allocate();rows.append(value)
            code.extend([(65,[uniform_vector_pointer,ptr,target,zero,constant(first_row+r)]),
                         (61,[vector_type,value,ptr]),(62,[locals_[r+1],value])])
        code.extend((62,[v,unused_vector]) for v in locals_[5:9]);columns=[]
        # Applying the already-validated clip helper to each SOURCE matrix
        # column yields targetProjection*eyeFromGameView, without a new helper
        # memory policy, guessed matrix or repeated original source command.
        for col in range(4):
            parts=[]
            for row in rows:
                x=module.allocate();parts.append(x);code.append((81,[scalar,x,row,col]))
            before=module.allocate();after=module.allocate();columns.append(after)
            code.extend([(80,[vector_type,before,*parts]),(62,[locals_[-1],before]),
                         (57,[void,module.allocate(),imported,*locals_]),(61,[vector_type,after,locals_[-1]])])
        for row in range(4):
            parts=[]
            for col in columns:
                x=module.allocate();parts.append(x);code.append((81,[scalar,x,col,row]))
            value=module.allocate();code.extend([(80,[vector_type,value,*parts]),(62,[locals_[row+1],value])])
        return code

    output=[];in_main=False;entry_prefix=False;inserted=False
    for index,(op,a) in enumerate(module.instructions):
        if op==54:in_main=a[1]==main
        if in_main and op==248 and not inserted:
            output.append((op,a));output.extend(local_declarations);entry_prefix=True;continue
        if entry_prefix and op not in (59,8,317):
            output.extend(entry());entry_prefix=False;inserted=True
        output.extend(replacements.get(index,[(op,a)]))
        if op==56:in_main=False
    if not inserted:raise ValueError('Original entry block unavailable')
    return output
