import math,struct,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import camera_block as block

def record(sample=0):
    words=[[0x3f800000,0x80000000,0x7fc12345,1] for _ in range(block.ROWS)]
    lines=[f'Shader: abcd: Instance {sample:010d}, ID ({block.MARKER}, {sample}, {row}): '+', '.join('#'+format(x,'x') for x in values) for row,values in enumerate(words)]
    return words,lines

class LosslessCameraBlock(unittest.TestCase):
    def parse(self,lines):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'private.log';p.write_text('\n'.join(lines)+'\n');return block.read_records(p)

    def test_exact_words_including_nonfinite_subnormal_and_signed_zero(self):
        words,lines=record();r=self.parse(lines)
        self.assertEqual(r['complete_records'],1);self.assertEqual(r['records'][0]['rows_bits'],words)
        self.assertFalse(r['frame_or_pose_identity_known'])

    def test_incomplete_and_duplicate_records_are_not_completed_from_other_samples(self):
        _,a=record();_,b=record(1)
        for lines in [a[:-1],a+[a[0]],a[:-1]+b[-1:]]:
            self.assertEqual(self.parse(lines)['complete_records'],0)

    def test_decimal_payload_and_out_of_budget_sample_are_refused(self):
        _,lines=record();lines[0]=lines[0].replace('#3f800000','1.0')
        r=self.parse(lines);self.assertEqual(r['complete_records'],0);self.assertEqual(r['rejected_rows'],1)
        _,lines=record(64);self.assertEqual(self.parse(lines)['complete_records'],0)

    def test_ring_generations_keep_samples_separate(self):
        words,a=record();_,b=record();r=self.parse(a+['Enabling shader debug ring of size: 123']+b)
        self.assertEqual(r['complete_records'],2);self.assertNotEqual(r['records'][0]['segment'],r['records'][1]['segment'])

class CameraMatrixRelations(unittest.TestCase):
    def packet(self):return {'rows_bits':[[0]*4 for _ in range(block.ROWS)]}
    def put(self,packet,row,m):
        words=struct.unpack('<16I',struct.pack('<16f',*m))
        packet['rows_bits'][row:row+4]=[list(words[i:i+4]) for i in range(0,16,4)]
    def test_inverse_candidates_use_both_products_and_keep_ambiguity(self):
        p=self.packet();a,b,jx,jy=1.784022,3.171595,.0004,-.0003;c,d=-80/79.7,-80*.3/79.7
        projection=[a,0,jx,0,0,b,jy,0,0,0,c,d,0,0,-1,0]
        inverse=[1/a,0,0,jx/a,0,1/b,0,jy/b,0,0,0,-1,0,0,1/d,c/d]
        self.put(p,72,projection);self.put(p,80,inverse);self.put(p,88,[inverse[c*4+r] for r in range(4) for c in range(4)])
        near_but_wrong=inverse.copy();near_but_wrong[3]=near_but_wrong[7]=0;self.put(p,96,near_but_wrong)
        candidates=block.relations(p)['inverse_candidates'];self.assertEqual({(x['first_row'],x['transpose']) for x in candidates},{(80,False),(88,True)})
        self.assertTrue(all(x['max_identity_residual']<2e-5 for x in candidates))
        self.assertFalse(block.relations(p)['inverse_semantic_role_proven'])
    def test_nonfinite_primary_cannot_prove_a_relation(self):
        p=self.packet();p['rows_bits'][72][0]=0x7fc12345
        self.assertFalse(block.relations(p)['projection_finite'])
