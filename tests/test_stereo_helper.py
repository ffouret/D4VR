from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from spirv_probe import Module,string_words,build
from stereo_helper import verify


class StereoHelperTests(unittest.TestCase):
    def module(self,extra=()):
        # Structural unit input only; real compiler outputs are separately
        # validated with spirv-val before this policy is used by the builder.
        m=Module(struct.pack('<5I',0x07230203,0x10300,0,100,0))
        m.instructions=[(17,[1]),(14,[5348,1]),(11,[1,*string_words('GLSL.std.450')]),
            (21,[2,64,0]),(21,[3,32,0]),(32,[4,7,3]),(32,[5,5349,3]),
            (71,[6,1,4]),(50,[2,6,0,0]),(54,[7,8,0,9]),(54,[7,10,0,9]),
            (59,[4,11,7]),(62,[11,12]),*extra]
        return m

    def test_helper_scope_does_not_grant_transformed_shader_permission(self):
        result=verify(self.module().binary())
        self.assertTrue(result['telemetry_instructions_absent'])
        self.assertEqual(result['specialization_ids'],[4])
        self.assertFalse(result['transformed_module_external_writes_excluded'])
        self.assertFalse(result['replay_allowed'])

    def test_unknown_memory_and_synchronization_opcodes_are_rejected(self):
        for op in (0,63,64,99,224,225,234,4433,6248,65535):
            with self.subTest(op=op),self.assertRaisesRegex(ValueError,'Unreviewed'):
                verify(self.module([(op,[])]).binary())

    def test_external_and_unresolved_stores_and_initializers_are_rejected(self):
        for extra in ([(55,[5,13]),(62,[13,12])],[(62,[90,12])],[(59,[4,13,6,12])]):
            with self.subTest(extra=extra),self.assertRaises(ValueError):verify(self.module(extra).binary())

    def test_telemetry_specializations_and_wrong_eye_width_are_rejected(self):
        for extra in ([(71,[20,1,1]),(50,[2,20,0,0])],[(71,[6,1,4])],[(52,[3,13,128,6,6])]):
            with self.subTest(extra=extra),self.assertRaises(ValueError):verify(self.module(extra).binary())
        m=self.module();m.instructions[3]=(21,[2,32,0])
        with self.assertRaisesRegex(ValueError,'64-bit'):verify(m.binary())

    def test_extended_pointer_writes_and_unknown_imports_are_rejected(self):
        for extra in ([(12,[3,13,1,35,12,11])],[(11,[14,*string_words('NonSemantic.DebugPrintf')])],
                      [(12,[3,13,1,999,12])]):
            with self.subTest(extra=extra),self.assertRaises(ValueError):verify(self.module(extra).binary())
        verify(self.module([(12,[3,13,1,4,12])]).binary())

    def test_extra_functions_capabilities_and_pointer_casts_are_rejected(self):
        for extra in ([(54,[7,14,0,9])],[(57,[7,14,90])],[(17,[4442])],
                      [(124,[4,14,6])],[(120,[4,14,6])]):
            with self.subTest(extra=extra),self.assertRaises(ValueError):verify(self.module(extra).binary())

    def test_byte_budget_and_bad_binary_are_rejected(self):
        for data in (b'',bytes(128*1024+4)):
            with self.assertRaises(ValueError):verify(data)

    def test_build_flag_requires_stereo_before_accessing_files(self):
        with tempfile.TemporaryDirectory() as root:
            p=Path(root)/'missing'
            with self.assertRaisesRegex(ValueError,'requires stereo'):
                build(p,p,p,p,3,0,72,helper_no_telemetry=True)

class ProjectionOnlyHelperTests(unittest.TestCase):
    def module(self,extra=()):
        m=StereoHelperTests().module()
        m.header[3]=200
        m.instructions=[*m.instructions[:-2],(19,[7]),(22,[21,32]),(23,[22,21,4]),(32,[23,7,22]),
            (5,[10,*string_words('d4vr_stereo(')]),
            *[(55,[4 if k==0 else 23,100+k]) for k in range(10)],*extra,(253,[]),(56,[])]
        return m
    def test_unused_object_arguments_are_proven_separately(self):
        from stereo_helper import verify_projection_only
        r=verify_projection_only(self.module().binary())
        self.assertTrue(r['projection_only_abi_verified']);self.assertEqual(r['unused_object_parameter_indices'],[5,6,7,8])
        self.assertFalse(r['replay_allowed'])
    def test_direct_reads_and_pointer_escape_are_rejected(self):
        from stereo_helper import verify_projection_only
        for extra in ([(61,[22,50,105])],[(57,[7,50,8,106])],[(62,[107,50])]):
            with self.subTest(extra=extra),self.assertRaisesRegex(ValueError,'Object-matrix parameter'):
                verify_projection_only(self.module(extra).binary())
    def test_wrong_parameter_count_or_type_is_rejected(self):
        from stereo_helper import verify_projection_only
        for kind in ('count','uint','vector'):
            m=self.module()
            if kind=='count':m.instructions.remove((55,[23,108]))
            if kind=='uint':next(a for o,a in m.instructions if o==55 and a[1]==100)[0]=23
            if kind=='vector':next(a for o,a in m.instructions if o==55 and a[1]==101)[0]=4
            with self.subTest(kind=kind),self.assertRaises(ValueError):verify_projection_only(m.binary())
    def test_projection_mode_requires_both_opt_ins_before_reading_files(self):
        for stereo,quiet in ((False,False),(True,False),(False,True)):
            with self.subTest(stereo=stereo,quiet=quiet),self.assertRaisesRegex(ValueError,'requires stereo and telemetry-free'):
                p=Path('missing');build(p,p,p,p,3,0,72,stereo=stereo,helper_no_telemetry=quiet,projection_only_helper=True)
