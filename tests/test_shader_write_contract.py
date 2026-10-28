from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from shader_write_contract import assess,Policy
from glsl_effects import GLSL450
from spirv_probe import Module,string_words
import test_glsl_effects as extended
import test_shader_effects as base


class ShaderWriteContract(unittest.TestCase):
    def setUp(self):self.glsl=GLSL450(extended.extended_grammar())
    def analyze(self,ops):return assess(base.binary(ops),self.glsl)
    def assertBlocked(self,ops,reason):
        r=self.analyze(ops);self.assertFalse(r['static_non_output_external_writes_excluded']);self.assertIn(reason,r['blocking_reasons'])

    def test_static_local_output_writes_cannot_grant_validated_or_replay_permission(self):
        r=self.analyze([(62,[10,1]),(62,[11,1]),(32,[6,6,1]),(59,[6,14,6,1])])
        self.assertTrue(r['static_non_output_external_writes_excluded'])
        self.assertEqual(r['core_writes_by_storage'],{'Output':1,'Function':1,'Private':1})
        self.assertFalse(r['module_validation_performed']);self.assertFalse(r['validated_non_output_external_writes_excluded']);self.assertFalse(r['replay_allowed'])

    def test_external_stores_copies_initializers_and_unknown_destinations_block(self):
        for op in ([(62,[12,1])],[(62,[13,1])],[(63,[12,11])],[(64,[12,11,1])],[(59,[4,20,12,1])],[(62,[99,1])]):
            with self.subTest(op=op):self.assertBlocked(op,'non_output_external_or_unresolved_write')
        self.assertTrue(self.analyze([(63,[11,12])])['static_non_output_external_writes_excluded'])

    def test_all_functions_count_even_unreachable_external_stores(self):
        self.assertBlocked([(54,[1,80,0,90]),(248,[81]),(62,[12,1]),(253,[]),(56,[])],
                          'non_output_external_or_unresolved_write')

    def test_known_extended_pointer_writes_must_be_local(self):
        prefix=[(11,[30,*string_words('GLSL.std.450')])]
        self.assertBlocked(prefix+[(12,[1,40,30,35,1,12])],'non_output_external_or_unresolved_write')
        r=self.analyze(prefix+[(12,[1,40,30,51,1,11])]);self.assertEqual(r['extended_writes_by_storage'],{'Function':1})
        self.assertTrue(r['static_non_output_external_writes_excluded'])

    def test_unknown_extended_sets_and_operations_block(self):
        for name,op,reason in [('NonSemantic.DebugPrintf',1,'unreviewed_extended_set'),('GLSL.std.450',34,'unreviewed_extended_instruction')]:
            self.assertBlocked([(11,[30,*string_words(name)]),(12,[1,40,30,op,1])],reason)
        self.assertBlocked([(12,[1,40,99,13,1])],'unreviewed_extended_instruction')

    def test_unknown_writes_atomics_barriers_and_extension_forms_block(self):
        for op in (99,224,225,227,228,234,4433,5398,6248,65000):
            with self.subTest(op=op):self.assertBlocked([(op,[])],'unreviewed_opcode:'+str(op))

    def test_pointer_address_casts_and_storage_derivation_are_conservative(self):
        self.assertBlocked([(120,[3,20,1])],'unreviewed_address_conversion')
        self.assertBlocked([(124,[3,20,1])],'unreviewed_pointer_result')
        self.assertBlocked([(65,[3,20,12,1])],'unresolved_pointer_derivation')
        self.assertBlocked([(32,[6,8,1])],'unreviewed_pointer_storage:8')
        self.assertBlocked([(59,[3,20,12])],'variable_storage_mismatch')
        self.assertTrue(self.analyze([(120,[5,20,1])])['static_non_output_external_writes_excluded'])

    def test_specialization_cannot_hide_unreviewed_operations(self):
        self.assertBlocked([(52,[1,20,99,1])],'unreviewed_specialization_operation')
        self.assertTrue(self.analyze([(52,[1,20,171,1,1])])['static_non_output_external_writes_excluded'])

    def test_unknown_caps_modes_annotations_and_calls_block(self):
        for ops,reason in [([(17,[5])],'unreviewed_capability'),([(14,[0,3])],'unreviewed_memory_model'),
            ([(10,string_words('SPV_unknown'))],'unreviewed_extension'),([(16,[90,11])],'unreviewed_execution_mode'),
            ([(71,[10,9999])],'unreviewed_decoration'),([(72,[1,0,9999])],'unreviewed_member_decoration'),
            ([(57,[1,20,99])],'unresolved_call')]:
            with self.subTest(reason=reason):self.assertBlocked(ops,reason)
        m=Module(base.binary());next(a for o,a in m.instructions if o==15)[0]=5
        self.assertIn('unsupported_entry_points',assess(m.binary(),self.glsl)['blocking_reasons'])

    def test_invalid_binary_grammar_and_arity_fail_closed(self):
        with self.assertRaises(ValueError):assess(b'',self.glsl)
        with self.assertRaises(ValueError):Policy(b'{}',b'{}')
        with self.assertRaises(ValueError):self.analyze([(11,[30,*string_words('GLSL.std.450')]),(12,[1,40,30,35,1])])
        with self.assertRaises(ValueError):self.analyze([(62,[12])])
