import math
import unittest
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from unittest.mock import patch
import sampled_uv_validate as uv

class UVOracleTests(unittest.TestCase):
    def test_environment_hash_comparison_uses_json_arrays(self):
        a={'cases':[{'original_sha256':'a','diagnostic_sha256':'b'}]}
        values=uv.paired_hashes(a);uv.equal(values,[['a','b']],'paired')
        with self.assertRaises(ValueError):uv.equal(values,[['a','c']],'paired')
    def test_pixel_centers_and_signed_viewport(self):
        a=uv.uv_at(0,0,[0,0,2560,1440],[0,0]);b=uv.uv_at(0,0,[0,1440,2560,-1440],[0,0])
        self.assertAlmostEqual(a[0],1/5120);self.assertAlmostEqual(a[1],1-1/2880);self.assertAlmostEqual(b[1],1/2880)
    def test_original_old_oracles_exact(self):
        for name in uv.base.CASES:
            self.assertEqual(list(uv.original_rows(name)),list(uv.base.reference_rows(name)),name)
    def test_finite_overflow_is_sentinel(self):
        s,_,_=uv.state('finite_overflow');u,v=uv.uv_at(0,0,s['viewport'],s['shift_texels'])
        self.assertEqual(uv.encode_bounds(u),(255,255));self.assertEqual(uv.encode_bounds(v),(0,0))
    def test_decode_conservative_floor_ceil(self):
        for value in (-1,-.125,0,.125,1,1.125,2):
            lo,hi=uv.encode_bounds(value)
            for code in range(lo,hi+1):
                a,b=uv.decode_interval(code);self.assertLessEqual(a,value);self.assertGreaterEqual(b,value)
        for value in (math.nan,math.inf,-math.inf):
            with self.assertRaises(ValueError):uv.encode_bounds(value)
        for code in (0,255,True,1.0):
            with self.assertRaises(ValueError):uv.decode_interval(code)
    def test_load_coverage_is_separate(self):
        self.assertFalse(uv.occupied(0,0,[640,360,1280,720],[0,0,2560,1440]))
        self.assertTrue(uv.occupied(0,0,[0,1440,2560,-1440],[0,0,2560,1440]))
    def test_wrong_ubo_keeps_actual_xy_zero(self):
        s,_,ubo=uv.state('wrong_ubo_component')
        self.assertEqual(s['ubo_row0_xy'],[0,0]);self.assertEqual(s['ubo_row0_zw'],[.125,-.125]);self.assertEqual(len(ubo),1280)
class DescriptorPlacementTests(unittest.TestCase):
    def placement(self):
        return dict(owned_exact_slots=True,alignment=16,offsets=[0,32,64,64],sizes=[32,32,0,16],binding_offsets=[0,0,0,0],sampler_descriptor_size=16,image_descriptor_size=16,uniform_descriptor_size=16)
    def test_finite_slots(self):self.assertEqual(uv.validate_placement(self.placement()),80)
    def test_negative_and_boolean_values_refused(self):
        for key in ('offsets','sizes','binding_offsets'):
            for value in (-1,True,1.0,1024*1024+1):
                p=self.placement();p[key][0]=value
                with self.assertRaises(ValueError):uv.validate_placement(p)
    def test_empty_binding_and_stride_and_total(self):
        changes=[('binding_offsets',[0,0,1,0]),('uniform_descriptor_size',True),('image_descriptor_size',-1),('sampler_descriptor_size',0),('offsets',[0,32,64,1024*1024])]
        for key,value in changes:
            p=self.placement();p[key]=value
            with self.assertRaises(ValueError):uv.validate_placement(p)

class UVPixelAuditTests(unittest.TestCase):
    def setUp(self):
        self.state=dict(viewport=[0,0,8,8],scissor=[0,0,8,8],shift_texels=[1,-1],sampler_index=1,texture_index=1)
        self.patches=[patch.object(uv.base,'WIDTH',8),patch.object(uv.base,'HEIGHT',8),patch.object(uv.base,'BYTE_COUNT',256),patch.object(uv,'state',return_value=(self.state,b'',b''))]
        for p in self.patches:p.start()
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
    def pixels(self):
        original=b''.join(uv.original_rows('test'));diagnostic=bytearray(original)
        for y in range(8):
            for x in range(8):
                if not uv.occupied(x,y,self.state['viewport'],self.state['scissor']):continue
                u,v=uv.uv_at(x,y,self.state['viewport'],self.state['shift_texels'])
                diagnostic[(y*8+x)*4+2]=uv.encode_bounds(u)[0]
                diagnostic[(y*8+x)*4+1]=uv.encode_bounds(v)[1]
        return original,diagnostic
    def test_finite_owned_pixels_and_alternate_unorm_rounding(self):
        a,b=self.pixels();r=uv.audit_pixels(a,b,'test');self.assertEqual(r['encoded_pixels'],64)
    def test_removed_original_sample_rejected(self):
        a,b=self.pixels();b[0]^=1
        with self.assertRaisesRegex(ValueError,'B/A=1'):uv.audit_pixels(a,b,'test')
    def test_wrong_original_rejected(self):
        a,b=self.pixels();a=bytes([a[0]^1])+a[1:]
        with self.assertRaisesRegex(ValueError,'original=1'):uv.audit_pixels(a,b,'test')
    def test_swapped_coordinate_rejected(self):
        a,b=self.pixels();b[1::4],b[2::4]=b[2::4],b[1::4]
        with self.assertRaises(ValueError):uv.audit_pixels(a,b,'test')
    def test_changed_sampling_uv_witness_rejected(self):
        a,b=self.pixels();b[0::4]=bytes([0])*64
        with self.assertRaises(ValueError):uv.audit_pixels(a,b,'test')
    def test_load_pixels_cannot_be_replaced_by_gradient(self):
        self.state['scissor']=[2,2,4,4];a,b=self.pixels()
        r=uv.audit_pixels(a,b,'test');self.assertEqual(r['unchanged_load_pixels'],48)
        b[2]=100
        with self.assertRaises(ValueError):uv.audit_pixels(a,b,'test')
    def test_nonfinite_not_given_numeric_interpretation(self):
        with self.assertRaises(ValueError):uv.encode_bounds(float('nan'))

if __name__=='__main__':unittest.main()
