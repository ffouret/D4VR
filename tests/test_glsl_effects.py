import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import shader_effects as effects
from glsl_effects import GLSL450
from spirv_probe import string_words
import test_shader_effects as base
from test_shader_effects import grammar_data,binary


def extended_grammar():
    signatures={4:('FAbs',1),8:('Floor',1),10:('Fract',1),13:('Sin',1),14:('Cos',1),
        29:('Exp2',1),30:('Log2',1),31:('Sqrt',1),32:('InverseSqrt',1),35:('Modf',2),36:('ModfStruct',1),
        50:('Fma',3),51:('Frexp',2),52:('FrexpStruct',1),79:('NMin',2),80:('NMax',2),81:('NClamp',3),34:('MatrixInverse',1)}
    return json.dumps({'version':100,'instructions':[{'opcode':op,'opname':name,
        'operands':[{'kind':'IdRef'} for _ in range(count)]} for op,(name,count) in signatures.items()]}).encode()


class ExtendedEffects(unittest.TestCase):
    def setUp(self):
        self.core=effects.Grammar(grammar_data());self.glsl=GLSL450(extended_grammar())

    def analyze(self,operations,source='GLSL.std.450'):
        return effects.analyze(binary([(11,[30,*string_words(source)]),*operations]),self.core,self.glsl)

    def test_pure_math_and_struct_results_do_not_write_through_operands(self):
        ops=[]
        for k,(op,count) in enumerate([(4,1),(8,1),(10,1),(13,1),(14,1),(29,1),(30,1),(31,1),(32,1),
                                      (36,1),(50,3),(52,1),(79,2),(80,2),(81,3)]):
            ops.append((12,[1,k+40,30,op,*([1]*count)]))
        r=self.analyze(ops)
        self.assertTrue(r['extended_instruction_effects_audited'])
        self.assertEqual(r['unmodeled_extended_instruction_count'],0)
        self.assertEqual(r['extended_pointer_write_instructions_by_storage'],{})
        self.assertTrue(all(e['memory_effect']=='none' for e in r['extended_instruction_effects']))
        self.assertFalse(r['external_memory_writes_excluded']);self.assertFalse(r['module_validation_performed'])

    def test_pointer_forms_detect_writes_even_without_core_stores(self):
        r=self.analyze([(12,[1,40,30,35,1,12]),(12,[1,41,30,51,1,13])])
        self.assertEqual(r['explicit_external_pointer_write_instructions'],0)
        self.assertEqual(r['extended_external_pointer_write_instructions'],2)
        self.assertEqual(r['non_atomic_external_pointer_write_instructions'],2)
        self.assertEqual(r['extended_pointer_write_instructions_by_storage'],{'PhysicalStorageBuffer':1,'StorageBuffer':1})
        self.assertTrue(r['extended_instruction_effects_audited'])
        self.assertFalse(r['external_memory_writes_excluded']);self.assertFalse(r['replay_allowed'])

    def test_output_function_and_private_writes_remain_separate_from_external(self):
        r=self.analyze([(32,[6,6,1]),(59,[6,14,6]),(12,[1,40,30,35,1,10]),
                        (12,[1,41,30,51,1,11]),(12,[1,42,30,51,1,14])])
        self.assertEqual(r['extended_pointer_write_instructions_by_storage'],{'Function':1,'Output':1,'Private':1})
        self.assertEqual(r['extended_external_pointer_write_instructions'],0)
        self.assertTrue(r['extended_instruction_effects_audited'])

    def test_unknown_destination_blocks_extended_audit(self):
        r=self.analyze([(12,[1,40,30,35,1,99])])
        self.assertEqual(r['extended_unresolved_pointer_write_instructions'],1)
        self.assertFalse(r['extended_instruction_effects_audited'])

    def test_unknown_sets_missing_imports_and_unreviewed_opcodes_do_not_gain_audit(self):
        for name in ('GLSL.std.450.fake','NonSemantic.DebugPrintf','OpenCL.std'):
            r=self.analyze([(12,[1,40,30,13,1])],name)
            self.assertFalse(r['extended_instruction_effects_audited']);self.assertEqual(r['unmodeled_extended_instruction_count'],1)
        r=self.analyze([(12,[1,40,99,13,1]),(12,[1,41,30,34,1])])
        self.assertEqual(r['unmodeled_extended_instruction_count'],2)
        self.assertFalse(r['extended_instruction_effects_audited'])

    def test_wrong_arities_invalid_ids_and_duplicate_imports_are_rejected(self):
        for operands in ([1,40,30,13],[1,40,30,13,1,2],[1,40,30,35,1],[1,40,30,35,1,0]):
            with self.assertRaises(ValueError):self.analyze([(12,operands)])
        with self.assertRaisesRegex(ValueError,'Duplicate instruction-set'):
            self.analyze([(11,[30,*string_words('other')])])

    def test_known_but_unmodeled_core_extension_forms_are_blocking(self):
        data=json.loads(grammar_data())
        data['instructions'].append({'opcode':4433,'opname':'OpExtInstWithForwardRefsKHR','class':'Extension',
            'operands':[{'kind':'IdResultType'},{'kind':'IdResult'}]})
        core=effects.Grammar(json.dumps(data).encode())
        r=effects.analyze(binary([(11,[30,*string_words('GLSL.std.450')]),(4433,[1,40,30,13,1])]),core,self.glsl)
        self.assertEqual(r['unmodeled_extended_instruction_forms'],{'OpExtInstWithForwardRefsKHR':1})
        self.assertEqual(r['unmodeled_extended_instruction_count'],1)
        self.assertFalse(r['extended_instruction_effects_audited'])

    def test_altered_grammar_signature_cannot_change_effect_policy(self):
        for kind in ('name','arity','operand_kind','optional','duplicate','version'):
            data=json.loads(extended_grammar());row=next(r for r in data['instructions'] if r['opcode']==35)
            if kind=='name':row['opname']='FAbs'
            if kind=='arity':row['operands'].pop()
            if kind=='operand_kind':row['operands'][1]['kind']='LiteralInteger'
            if kind=='optional':row['operands'][1]['quantifier']='?'
            if kind=='duplicate':data['instructions'].append(row.copy())
            if kind=='version':data['version']=101
            with self.assertRaises(ValueError):GLSL450(json.dumps(data).encode())
        with self.assertRaises(ValueError):GLSL450(b' '*(128*1024+1))
        for malformed in (b'[]',b'{}',b'{"version":100,"instructions":[null]}'):
            with self.assertRaises(ValueError):GLSL450(malformed)

    def test_optional_policy_preserves_legacy_census_and_never_authorizes_candidates(self):
        data=binary([(11,[30,*string_words('GLSL.std.450')]),(12,[1,40,30,13,1])])
        r=effects.analyze(data,self.core)
        self.assertFalse(r['extended_instruction_effects_audited']);self.assertIsNone(r['glsl_grammar_sha256'])
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);frame,catalog=base.ShaderEffects().frame_inputs(directory)
            r=effects.audit_frame(frame,[0],directory,catalog,self.core,self.glsl)
            self.assertEqual(r['candidate_modules_with_unresolved_extended_effects'],0)
            self.assertFalse(r['module_identity_with_game_proven']);self.assertFalse(r['external_memory_writes_excluded']);self.assertFalse(r['replay_allowed'])
