import sys
from pathlib import Path
import unittest
import argparse,json,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from camera_bundle import select_candidates
import camera_bundle,probe

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

class ProjectionInputCertificate(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory();self.addCleanup(self.directory.cleanup)
        self.path=Path(self.directory.name);self.helper=self.path/'helper.spv';self.helper.write_bytes(b'owned test helper identity')
        self.certificate=self.path/'proof.json';self.certificate.write_text('{}')
        self.args=argparse.Namespace(projection_input=True,projection_only_helper=True,stereo=True,
            source_depth_calibration=True,projection_input_certificate=self.certificate,helper=self.helper)

    def test_input_mode_does_not_accept_a_nonmetric_bundle(self):
        self.args.source_depth_calibration=False
        with self.assertRaisesRegex(ValueError,'source-depth'):camera_bundle.build(self.args)

    def test_missing_dependent_fragment_certificate_refuses_before_construction(self):
        self.args.projection_input_certificate=None
        with self.assertRaisesRegex(ValueError,'dependent-fragment'):camera_bundle.build(self.args)

    def test_other_helper_identity_is_refused(self):
        with self.assertRaisesRegex(ValueError,'helper certificate mismatch'):camera_bundle.build(self.args)

    def test_other_source_identity_is_refused(self):
        self.certificate.write_text(json.dumps({'helper_sha256':probe.digest(self.helper),'source_sha256':{}}))
        with self.assertRaisesRegex(ValueError,'source certificate mismatch'):camera_bundle.build(self.args)

    def test_static_certificate_cannot_authorize_a_game_bundle(self):
        root=Path(__file__).resolve().parents[1]
        hashes={name:probe.digest(root/name) for name in ['tools/projection_input.py','tools/spirv_probe.py','native/tests/projection_input.vert','native/tests/projection_input.frag']}
        self.certificate.write_text(json.dumps({'helper_sha256':probe.digest(self.helper),'source_sha256':hashes,
            'construction_refusals':['dynamic-row','dynamic-component','function','volatile'],'gpu_cases':[]}))
        with self.assertRaisesRegex(ValueError,'dependent-fragment controls missing'):camera_bundle.build(self.args)
        value=json.loads(self.certificate.read_text());value['gpu_cases']=[{'environment':'native','case':'dependent',
            'proof':{'calibration_gpu_passed':True,'independent_reference_cases':48,'maximum_reference_depth_error':0},'validation_errors':1},
            {'environment':'native','case':'late-negative','expected_failure_observed':True,'validation_errors':0}]
        self.certificate.write_text(json.dumps(value))
        with self.assertRaisesRegex(ValueError,'dependent-fragment controls missing'):camera_bundle.build(self.args)
