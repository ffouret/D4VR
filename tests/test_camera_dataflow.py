import struct,sys,unittest
from types import SimpleNamespace
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import camera_dataflow as flow
from spirv_probe import Module


class DataRoutes(unittest.TestCase):
    def module(self):
        m=Module(struct.pack('<5I',0x07230203,0x10300,0,200,0))
        m.instructions=[(71,[10,34,3]),(71,[10,33,0]),(71,[11,30,4]),
            (59,[1,10,2]),(59,[2,11,3]),(43,[3,20,0]),(43,[3,21,80]),
            (65,[1,30,10,20,21]),(61,[4,31,30]),(62,[11,31])]
        return m
    def analyze(self,m):
        # These are instruction-graph unit fixtures, not SPIR-V validation
        # evidence. The separate controls validate actual compiler outputs.
        effects={'static_non_output_external_writes_excluded':True,'execution_models':[0],
                 'functions':1,'core_writes_by_storage':{'Output':1}}
        with patch.object(flow,'assess',return_value=effects):return flow.analyze(m.binary(),SimpleNamespace(glsl=None))
    def test_direct_and_copied_uniform_paths(self):
        m=self.module();m.instructions[-1:]=[(83,[1,32,30]),(61,[4,33,32]),(62,[11,33])]
        r=self.analyze(m);self.assertEqual(r['outputs'][0]['uniform_rows'],[{'set':3,'binding':0,'row':80}])
        self.assertFalse(r['temporal_semantics_proven']);self.assertFalse(r['execution_proven'])
    def test_composite_literal_collision_does_not_create_an_id_edge(self):
        m=self.module();m.instructions[-1:]=[(43,[3,40,123]),(81,[3,41,40,31]),(62,[11,41])]
        self.assertEqual(self.analyze(m)['outputs'][0]['uniform_rows'],[])
    def test_phi_label_is_not_a_value_dependency_and_cycles_terminate(self):
        m=self.module();m.instructions[-1:]=[(43,[3,40,123]),(245,[3,41,40,31,41,99]),(62,[11,41])]
        self.assertEqual(self.analyze(m)['outputs'][0]['uniform_rows'],[])
    def test_dynamic_path_and_unreviewed_operation_are_refused(self):
        m=self.module();m.instructions.insert(-1,(65,[1,50,10,20,31]))
        with self.assertRaisesRegex(ValueError,'Dynamic'):self.analyze(m)
        m=self.module();m.instructions.insert(-1,(9999,[]))
        with self.assertRaisesRegex(ValueError,'opcode'):self.analyze(m)
    def test_unresolved_output_and_memory_alias_are_refused(self):
        m=self.module();m.instructions=[x for x in m.instructions if x!=(71,[11,30,4])]
        with self.assertRaisesRegex(ValueError,'output interface'):self.analyze(m)
        m=self.module();m.instructions.insert(-1,(59,[1,60,7]))
        with self.assertRaisesRegex(ValueError,'variable'):self.analyze(m)
