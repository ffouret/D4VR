import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import eye_control
import eye_report


class EyePackets(unittest.TestCase):
    def test_binary_abi_and_eye_signs(self):
        for eye,sign in ((0,1),(1,-1)):
            packet = eye_control.encode(eye)
            self.assertEqual(len(packet),176)
            state = eye_control.decode(packet)
            self.assertAlmostEqual(state['translation_view_units'],sign*.032)
            self.assertEqual(state['eye'],eye)
            self.assertTrue(state['uses_current_game_projection'])
        self.assertFalse(eye_control.decode(eye_control.encode(1,enabled=False))['enabled'])

    def test_invalid_packets_and_parameters_rejected(self):
        for args in ((2,1,.064),(1,0,.064),(1,1,.3),(1,1,float('nan'))):
            with self.assertRaises(ValueError): eye_control.encode(*args)
        for index,value in ((0,0),(3,7),(6,1),(8,float('nan')),(12,.1)):
            values = list(eye_control.FORMAT.unpack(eye_control.encode(1)))
            values[index] = value
            with self.assertRaises(ValueError): eye_control.decode(eye_control.FORMAT.pack(*values))
        with self.assertRaises(ValueError): eye_control.decode(b'bad')

    def test_complete_typed_log_verifies_fixed_eye_delta(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); packet = root/'eye.bin';packet.write_bytes(eye_control.encode(1))
            before = [.1,.2,3.705557,4.]
            tx = eye_control.read(packet)['translation_view_units']
            after = [before[0]+1.784022*tx,*before[1:]]
            rows = [before,after,[1.784022,3.171595,-1.001502,-.300451],[1,1,1,1]]
            log = root/'eye.log'
            log.write_text(''.join(f'Shader: ab: Instance 0, ID ({eye_report.MARKER}, 0, {k}): '+', '.join(f'{v:.6f}' for v in row)+'\n' for k,row in enumerate(rows)))
            report = eye_report.parse(log,packet)
            self.assertEqual(report['verified_transformed_samples'],1)
            self.assertEqual(report['verified_transformed_inside_clip'],1)
            self.assertTrue(report['numeric_eye_transform_confirmed'])
            self.assertFalse(report['stereo_views_of_same_game_frame_proven'])
            self.assertEqual(report['game_images_submitted_to_steamvr'],0)
            log.write_text(log.read_text()+log.read_text().splitlines()[0]+'\n')
            self.assertFalse(eye_report.parse(log,packet)['numeric_eye_transform_confirmed'])

    def test_wrong_eye_sequence_or_displacement_cannot_confirm(self):
        state = eye_control.decode(eye_control.encode(1))
        good = {'segment':1,'shader_hash':'0'*16,'sample_id':0,'rows':[
            [.1,.2,3.705557,4.],[.0429113,.2,3.705557,4.],[1.784022,3.171595,-1.001502,-.300451],[1,1,1,1]]}
        self.assertTrue(eye_report.evaluate(good,state)['consistent'])
        for row,component,value in ((3,1,0),(3,2,2),(1,0,.157),(1,1,.3)):
            changed = json.loads(json.dumps(good)); changed['rows'][row][component] = value
            self.assertFalse(eye_report.evaluate(changed,state)['consistent'])

    def test_inactive_or_non_target_geometry_must_stay_unchanged(self):
        record = {'segment':1,'shader_hash':'0'*16,'sample_id':0,'rows':[
            [.1,.2,.5,1.],[.1,.2,.5,1.],[.75,.8,-1.,-.3],[0,1,1,1]]}
        state = eye_control.decode(eye_control.encode(1))
        self.assertTrue(eye_report.evaluate(record,state)['consistent'])
        record['rows'][1][0] += .01
        self.assertFalse(eye_report.evaluate(record,state)['consistent'])


if __name__ == '__main__': unittest.main()
