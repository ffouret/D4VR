from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from spirv_probe import Module,instrument
FIXTURE=Path(__file__).resolve().parents[1]/'native/tests/probe_fixture.spv.hex'
class ProjectionInstrumentation(unittest.TestCase):
    def test_no_added_access_to_secondary_buffer_and_original_stores_preserved(self):
        source=Module(bytes.fromhex(FIXTURE.read_text()));secondary=[]
        for op,a in source.instructions:
            if op==71 and a[1:]==[33,1]:secondary.append(a[0])
        self.assertEqual(len(secondary),1)
        # Move the original descriptor to a different set; source accesses stay
        # intact, while the injection's old secondary source no longer exists.
        for op,a in source.instructions:
            if op==71 and a[0]==secondary[0] and a[1]==34:a[2]=2
        with self.assertRaises(ValueError):instrument(source.binary(),3,0,72,stereo=True)
        out=Module(instrument(source.binary(),3,0,72,stereo=True,projection_only_helper=True))
        original_chains=[a for o,a in source.instructions if o==65 and a[2]==secondary[0]]
        self.assertEqual([a for o,a in out.instructions if o==65 and a[2]==secondary[0]],original_chains)
        for o,a in source.instructions:
            if o==62:self.assertIn((o,a),out.instructions)
        self.assertEqual(sum(o==46 for o,a in out.instructions),1)
    def test_primary_matrix_extent_and_stereo_mode_stay_required(self):
        data=bytes.fromhex(FIXTURE.read_text())
        with self.assertRaisesRegex(ValueError,'requires stereo'):instrument(data,3,0,72,projection_only_helper=True)
        with self.assertRaisesRegex(ValueError,'extent'):instrument(data,3,0,110,stereo=True,projection_only_helper=True)
