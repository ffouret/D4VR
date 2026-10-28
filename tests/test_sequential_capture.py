import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import sequential_capture as capture


def sample(directory,eye):
    path=directory/f'image-99-1-{eye+1}.json'
    d={'schema_version':1,'eye':eye,'sequence':eye+1,'present':10+eye,'width':2,'height':1,'format':44,'bytes':8,
       'single_eye_interval':True,'selected_eye_mask':1<<eye,'selected_draw_commands':1,'scope':'original_presentation_blit',
       'same_instant_pair':False,'game_stereo_submitted':False}
    path.write_text(json.dumps(d));path.with_suffix('.pixels').write_bytes(bytes([eye,0,255,255])*2);return path,d


class SequentialImages(unittest.TestCase):
    def test_pair_keeps_sequential_scope_and_writes_real_pixel_previews(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp);sample(directory,0);sample(directory,1);result=capture.pair(directory,False)
            self.assertEqual(result['different_pixel_bytes'],2);self.assertFalse(result['same_instant_pair']);self.assertFalse(result['game_stereo_submitted'])
            self.assertTrue((directory/'pair.png').read_bytes().startswith(b'\x89PNG'))

    def test_mixed_unmarked_wrong_eye_bounds_and_short_pixels_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            path,d=sample(Path(tmp),0)
            for key,value in [('selected_eye_mask',3),('single_eye_interval',False),('selected_draw_commands',0),('eye',True),('width',4097),('same_instant_pair',True),('scope','unknown')]:
                bad={**d,key:value};path.write_text(json.dumps(bad))
                with self.subTest(key=key),self.assertRaises(ValueError):capture.read_image(path)
            path.write_text(json.dumps(d));path.with_suffix('.pixels').write_bytes(b'\0')
            with self.assertRaises(ValueError):capture.read_image(path)

    def test_process_identity_order_and_actual_game_observation_are_required(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp);sample(directory,0);path,d=sample(directory,1)
            path.rename(directory/'image-100-1-2.json');path.with_suffix('.pixels').rename(directory/'image-100-1-2.pixels')
            with self.assertRaises(ValueError):capture.pair(directory,False)
            (directory/'image-100-1-2.json').rename(path);(directory/'image-100-1-2.pixels').rename(path.with_suffix('.pixels'))
            d['present']=1;path.write_text(json.dumps(d))
            with self.assertRaises(ValueError):capture.pair(directory,False)
            d['present']=11;path.write_text(json.dumps(d));(directory/'process-observations.json').write_text(json.dumps({'observations':[{'processes':[{'pid':100,'names':['diablo iv.exe']}]}]}))
            with self.assertRaises(ValueError):capture.pair(directory,True)

    def test_bgra_and_packed_unorm_channel_conversion(self):
        self.assertEqual(capture.rgba(bytes([0,128,255,255]),44),bytes([255,128,0,255]))
        self.assertEqual(capture.rgba(struct.pack('<I',1023|(512<<10)|(3<<30)),64),bytes([255,128,0,255]))
        self.assertEqual(capture.rgba(struct.pack('<I',(1023<<20)|(512<<10)|(3<<30)),58),bytes([255,128,0,255]))

    def test_repeated_eyes_preserve_first_pair_and_require_ordered_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp);first=[sample(directory,i) for i in (0,1)]
            for eye,(p,d) in enumerate(first):
                dest=directory/f'image-99-1-{eye+3}.json';row={**d,'sequence':eye+3,'present':12+eye}
                dest.write_text(json.dumps(row));dest.with_suffix('.pixels').write_bytes(p.with_suffix('.pixels').read_bytes())
            result=capture.pair(directory,False)
            self.assertEqual(result['images'][0]['metadata'],str(first[0][0]));self.assertTrue(result['repeated_eye_selections_verified']);self.assertFalse(result['stationary_scene_proven'])
            self.assertEqual(len(result['repeat_images']),2);self.assertTrue((directory/'repeat-right.png').exists())
            p=directory/'image-99-1-3.json';d=json.loads(p.read_text());d['sequence']=9;p.write_text(json.dumps(d))
            with self.assertRaises(ValueError):capture.pair(directory,False)

    def test_incomplete_repeat_does_not_silently_overwrite_first_eye(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp);p,d=sample(directory,0);sample(directory,1)
            extra=directory/'image-99-1-3.json';extra.write_text(json.dumps({**d,'sequence':3,'present':12}));extra.with_suffix('.pixels').write_bytes(p.with_suffix('.pixels').read_bytes())
            with self.assertRaises(ValueError):capture.pair(directory,False)

    def test_presentation_png_is_opaque_when_terrain_alpha_is_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp);p,_=sample(directory,0);q,_=sample(directory,1)
            original=bytes([33,66,99,0,10,20,30,95])
            for x in (p,q):x.with_suffix('.pixels').write_bytes(original)
            result=capture.pair(directory,False);data=(directory/'left.png').read_bytes();offset=8;compressed=b''
            while offset<len(data):
                length=struct.unpack_from('>I',data,offset)[0];kind=data[offset+4:offset+8]
                if kind==b'IDAT':compressed+=data[offset+8:offset+8+length]
                offset+=length+12
            self.assertEqual(zlib.decompress(compressed),bytes([0,99,66,33,255,30,20,10,255]))
            self.assertEqual(p.with_suffix('.pixels').read_bytes(),original)
            self.assertIn('opaque',result['preview_alpha_policy'])


if __name__=='__main__':unittest.main()
