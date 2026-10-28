"""Bounded memory-effect model for explicitly audited GLSL.std.450 operations.

This classifies instruction effects; it is not a SPIR-V validator or a shader
replay permission. Unknown operations/sets must remain blocking.
"""
import hashlib
import json

# opcode: (normative name, IdRef operand count, written pointer operand or None)
# Arithmetic return values are pure with respect to memory, not promised to be
# bit-identical across devices. Modf/Frexp have an additional pointer write.
SIGNATURES = {
    4:('FAbs',1,None),8:('Floor',1,None),10:('Fract',1,None),
    13:('Sin',1,None),14:('Cos',1,None),29:('Exp2',1,None),
    30:('Log2',1,None),31:('Sqrt',1,None),32:('InverseSqrt',1,None),
    35:('Modf',2,1),36:('ModfStruct',1,None),50:('Fma',3,None),
    51:('Frexp',2,1),52:('FrexpStruct',1,None),
    79:('NMin',2,None),80:('NMax',2,None),81:('NClamp',3,None),
}


class GLSL450:
    def __init__(self,data: bytes):
        if len(data)>128*1024:raise ValueError('Oversized GLSL grammar')
        parsed=json.loads(data)
        if not isinstance(parsed,dict) or parsed.get('version')!=100 or not isinstance(parsed.get('instructions'),list) or len(parsed['instructions'])>512:
            raise ValueError('Expected bounded GLSL.std.450 grammar')
        rows={}
        for row in parsed['instructions']:
            if not isinstance(row,dict):raise ValueError('Invalid GLSL grammar row')
            op=row.get('opcode')
            if type(op) is not int or not 0<=op<=65535 or op in rows:
                raise ValueError('Invalid or duplicate GLSL grammar opcode')
            rows[op]=row
        for op,(name,count,_) in SIGNATURES.items():
            row=rows.get(op,{})
            operands=row.get('operands',[])
            if row.get('opname')!=name or not isinstance(operands,list) or len(operands)!=count or any(not isinstance(o,dict) or o.get('kind')!='IdRef' or 'quantifier' in o for o in operands):
                raise ValueError('GLSL grammar disagrees with audited memory-effect signature')
        self.sha256=hashlib.sha256(data).hexdigest()

    def classify(self,instruction_set: str,opcode: int,operands: list[int]) -> dict:
        if instruction_set!='GLSL.std.450' or opcode not in SIGNATURES:
            return {'effect':'unknown','name':None,'written_pointer':None}
        name,count,writes=SIGNATURES[opcode]
        if len(operands)!=count or any(type(i) is not int or i<=0 for i in operands):
            raise ValueError('Malformed audited GLSL instruction operands')
        return {'effect':'none' if writes is None else 'pointer_write','name':name,
                'written_pointer':None if writes is None else operands[writes]}
