import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from camera_bundle import select_candidates

def candidate(name,projection=(0,0,1152),object_matrix=None):
    def matrix(values):
        s,r,b=values;return {'buffer':{'space':s,'register':r},'byte_offset':b}
    return {'file':name,'final_clip_matrix':matrix(projection) if projection else None,
            'input_matrix_chain':[matrix(object_matrix)] if object_matrix else []}

class BundleSelection(unittest.TestCase):
    def test_projection_only_keeps_vertices_without_a_separate_object_matrix(self):
        rows=[candidate('separate-object',(0,0,1152),(0,1,896)),candidate('no-object-binding'),
              candidate('other-input-chain',(0,0,1152),(0,0,544))]
        self.assertEqual([x['file'] for x in select_candidates({'shaders':rows})],['separate-object'])
        self.assertEqual([x['file'] for x in select_candidates({'shaders':rows},True)],
                         ['separate-object','no-object-binding','other-input-chain'])

    def test_projection_only_still_requires_the_exact_observed_projection_binding(self):
        rows=[candidate('none',None),candidate('wrong-set',(1,0,1152)),candidate('wrong-binding',(0,1,1152)),
              candidate('wrong-offset',(0,0,1088)),candidate('known')]
        self.assertEqual([x['file'] for x in select_candidates({'shaders':rows},True)],['known'])
