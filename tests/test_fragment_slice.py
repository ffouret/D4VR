import struct,sys,unittest
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from spirv_probe import Module
import fragment_slice


class FragmentSlices(unittest.TestCase):
    def setUp(self):
        self.m=Module(struct.pack('<5I',0x07230203,0x10300,0,100,0))
        self.m.instructions=[(15,[4,1,0]),(71,[10,30,4]),(71,[11,30,0]),(59,[1,10,1]),(59,[2,11,3]),(61,[3,30,10]),(62,[11,30])]
        typed={x:{'operands':[{'kind':'IdResultType'}]} for x in (59,61,43,81,88)}
        self.policy=SimpleNamespace(core=SimpleNamespace(ops=typed))
    def run_slice(self):return fragment_slice.analyze(self.m.binary(),self.policy)
    def test_selected_input_route_is_reported_without_temporal_claim(self):
        r=self.run_slice();self.assertEqual(r['slices'][0]['input_interfaces'],[{'kind':'location','value':4,'declared_component':0}]);self.assertFalse(r['temporal_semantics_proven'])
    def test_unvisited_texture_does_not_expand_the_claimed_slice(self):
        self.m.instructions.insert(-1,(88,[3,40,60,61,2,62]));self.assertEqual(len(self.run_slice()['slices']),1)
        self.m.instructions[-1]=(62,[11,40])
        with self.assertRaisesRegex(ValueError,'slice value 88'):self.run_slice()
    def test_literal_index_does_not_become_an_input_value(self):
        self.m.instructions[-1:]=[(43,[3,40,123]),(81,[3,41,40,30]),(62,[11,41])]
        self.assertEqual(self.run_slice()['slices'][0]['input_interfaces'],[])
