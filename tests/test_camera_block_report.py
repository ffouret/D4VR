import struct,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import camera_block_report as report
from spirv_probe import Module


class CameraPartners(unittest.TestCase):
    def test_unaligned_affine_inverse_and_separate_projection_variant(self):
        p={'rows_bits':[[0]*4 for _ in range(113)]}
        def put(row,m):
            w=struct.unpack('<16I',struct.pack('<16f',*m));p['rows_bits'][row:row+4]=[list(w[i:i+4]) for i in range(0,16,4)]
        put(5,[0,-1,0,2,1,0,0,3,0,0,1,4,0,0,0,1])
        put(13,[0,1,0,-3,-1,0,0,2,0,0,1,-4,0,0,0,1])
        a=[2,0,.001,0,0,3,-.001,0,0,0,-1.01,-.3,0,0,-1,0]
        put(72,a);b=a.copy();b[2]=b[6]=0;put(76,b)
        r=report.partners(p)
        self.assertEqual(r['affine_inverse_pairs'],[{'first_rows':[5,13],'max_identity_residual':0}])
        self.assertEqual(next(x for x in r['perspective_windows'] if x['first_row']==76)['different_components_from_primary'],[2,6])
        self.assertFalse(r['semantic_roles_proven']);self.assertFalse(r['coverage_cause_proven'])

    def test_nonfinite_and_invalid_tolerance_do_not_infer_projection(self):
        p={'rows_bits':[[0]*4 for _ in range(113)]};p['rows_bits'][72][0]=0x7fc12345
        self.assertEqual(report.partners(p)['perspective_windows'],[])
        for bad in (float('nan'),0,.1):
            with self.assertRaises(ValueError):report.partners(p,bad)

    def module(self):
        # Minimal instruction inventory only; GPU controls separately require
        # spirv-val. This fixture checks alias accounting, not SPIR-V validity.
        m=Module(struct.pack('<5I',0x07230203,0x10300,0,100,0))
        m.instructions=[(71,[10,34,3]),(71,[10,33,0]),(32,[11,2,12]),
            (43,[20,21,0]),(43,[20,22,80]),(59,[11,10,2]),
            (65,[11,30,10,21,22]),(83,[11,31,30]),(61,[12,32,31])]
        return m

    def test_static_copy_alias_and_dynamic_paths_are_distinct(self):
        m=self.module();r=report.static_reads(m.binary())
        self.assertEqual(r['statically_loaded_rows'],[80]);self.assertTrue(r['direct_read_inventory_complete'])
        m.instructions.extend([(65,[11,40,10,21,99]),(61,[12,41,40])])
        r=report.static_reads(m.binary());self.assertEqual(r['unresolved_camera_loads'],1)
        self.assertFalse(r['direct_read_inventory_complete'])

    def test_unknown_uniform_pointer_producer_prevents_absence_claim(self):
        m=self.module();m.instructions.append((245,[11,50,30,60,31,61]))
        r=report.static_reads(m.binary());self.assertEqual(r['unreviewed_uniform_pointer_producers'],1)
        self.assertFalse(r['direct_read_inventory_complete']);self.assertFalse(r['output_dependency_proven'])
