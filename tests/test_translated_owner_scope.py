"""CPU-only preflight for the new two-case owner fixture.

The Fraction oracle imports no authored fixture/auditor pixel routine. Native
helper tests compile ONLY D4VR_OWNER_GATE_CPU_TEST: no Windows/Vulkan includes,
Wine, D3D12, Vulkan loading, device, command, shader or submission is executed.
"""
from fractions import Fraction
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'native/tests/translated_owner_scope.cpp'
CASES=('direct_A_clamp','after_bind_B_clamp')
WIDTH,HEIGHT=256,144
NONCE='0123456789abcdef0123456789abcdef'
FROZEN={
 'native/tests/translated_descriptor_copy.cpp':'d0b8c1d15e143dd866a294cbceefd9f3d2b8dfb768a325de0cce43745ddd0cf4',
 'native/tests/translated_descriptor_copy.hlsl':'804d0572f7a845f932f552b909617274b35f797fada095a39355ec3ff0471684',
 'native/tests/sampled_writer_hash.hpp':'f6b59c3568c23d6f3198a8d952fbdcf8a4da85db945d311a0b369d15ae266f9e',
}

def reference(case):
    """Exact owned point/clamp BGRA for pixel centers and finite 1/8 offsets."""
    if case not in CASES:raise ValueError('Exact two-case oracle name')
    ix=[min(7,max(0,int((Fraction(2*x+1,2*WIDTH)+Fraction(1,8))*8//1))) for x in range(WIDTH)]
    iy=[min(7,max(0,int((Fraction(2*y+1,2*HEIGHT)-Fraction(1,8))*8//1))) for y in range(HEIGHT)]
    rows={}
    for y in set(iy):
        row=bytearray()
        for x in ix:
            rgba=(17+13*x,29+19*y,41+7*x+11*y,255) if case==CASES[0] else (233-11*x,211-17*y,197-5*x-9*y,83+7*x+11*y)
            row.extend((rgba[2],rgba[1],rgba[0],rgba[3]))
        rows[y]=bytes(row)
    return b''.join(rows[y] for y in iy)

def compare_readback(case,data):
    if type(data) is not bytes or data!=reference(case):raise ValueError('Exact complete two-case BGRA mismatch')
    return {'case':case,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}

def write_oracle(output):
    output=Path(output).absolute()
    if output.exists() or any(p.is_symlink() for p in [output,*output.parents]):raise ValueError('Fresh non-symlink private oracle directory')
    output.mkdir()
    rows=[]
    for case in CASES:
        data=reference(case);p=output/(case+'.bgra');p.write_bytes(data);rows.append(compare_readback(case,data))
    proof={'schema_version':1,'purpose':'prephysical_independent_two_case_Fraction_BGRA_reference','width':256,'height':144,'DXGI_format':87,'UV':'SV_Position.xy/(256,144)+(1/8,-1/8); no Y flip','sampling':'point_clamp_LOD0','cases':rows,'physical_execution':False,'imports_author_pixel_functions':False,'source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (output/'reference.json').write_text(json.dumps(proof,indent=2)+'\n')
    return proof

class OracleTests(unittest.TestCase):
    def test_owned_two_complete_readbacks_and_independent_integer_derivation(self):
        for case in CASES:
            data=reference(case);self.assertEqual(len(data),147456)
            # Separate integer reduction of exact rational centers. Every target
            # pixel is compared, including clamp on top/bottom/right borders.
            independent=bytearray()
            for y in range(144):
                sy=min(7,max(0,(2*y+1-36)//36))
                for x in range(256):
                    sx=min(7,max(0,(2*x+1+64)//64))
                    if case==CASES[0]:independent.extend((41+7*sx+11*sy,29+19*sy,17+13*sx,255))
                    else:independent.extend((197-5*sx-9*sy,211-17*sy,233-11*sx,83+7*sx+11*sy))
            self.assertEqual(data,bytes(independent))
    def test_wrong_resource_copy_and_each_channel_corruption_refused(self):
        a,b=[reference(c) for c in CASES]
        self.assertNotEqual(a,b)
        with self.assertRaises(ValueError):compare_readback(CASES[1],a)
        for case in CASES:
            for channel in range(4):
                bad=bytearray(reference(case));bad[(77*256+113)*4+channel]^=1
                with self.assertRaises(ValueError):compare_readback(case,bytes(bad))
    def test_exact_extent_and_case_not_silently_generalized(self):
        for data in (reference(CASES[0])[:-1],reference(CASES[0])+b'\0',bytearray(reference(CASES[0]))):
            with self.assertRaises(ValueError):compare_readback(CASES[0],data)
        with self.assertRaises(ValueError):reference('simple_A_clamp')
    def test_frozen_borrowed_sources_and_safe_CPU_compile_boundary(self):
        for rel,sha in FROZEN.items():self.assertEqual(hashlib.sha256((ROOT/rel).read_bytes()).hexdigest(),sha)
        source=SOURCE.read_text();self.assertIn('#define wmain frozen_translated_descriptor_copy_main',source)
        prefix=source.split('#include "translated_descriptor_copy.cpp"')[0]
        self.assertIn('#ifndef D4VR_OWNER_GATE_CPU_TEST',prefix)

class NativeGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which('c++') or shutil.which('g++') or shutil.which('clang++');cls.run_prefix=[]
        if compiler:command=[compiler]
        elif Path('/run/host/usr/bin/flatpak-spawn').is_file():
            command=['/run/host/usr/bin/flatpak-spawn','--host','/usr/bin/g++'];cls.run_prefix=command[:-1]
        else:raise unittest.SkipTest('No CPU C++17 compiler; no PE/Wine/GPU fallback')
        # The host compiler needs a shared workspace directory, rather than a
        # Flatpak-private /tmp. Other platforms use their ordinary temporary dir.
        parent=ROOT.parent/'work' if cls.run_prefix else None
        if parent is not None:parent.mkdir(exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(prefix='owner-gate-cpu-',dir=parent)
        cls.binary=Path(cls.temp.name)/('gate-cpu.exe' if __import__('os').name=='nt' else 'gate-cpu')
        built=subprocess.run(command+['-std=c++17','-O2','-DNDEBUG','-Wall','-Wextra','-Werror','-DD4VR_OWNER_GATE_CPU_TEST',str(SOURCE),'-o',str(cls.binary)],capture_output=True,text=True,timeout=60)
        if built.returncode:raise AssertionError('CPU helper compilation failed: '+built.stdout+built.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def native(self,*args,body=''):
        return subprocess.run(self.run_prefix+[str(self.binary),*args],input=body,capture_output=True,text=True,timeout=5)
    def reply(self,seq=1,**kw):
        row=dict(protocol_version=1,run_id=NONCE,gate_sequence=seq,status='go');row.update(kw);return json.dumps(row)+'\n'
    def test_portable_standalone_CPU_self_test(self):
        r=self.native('self-test');self.assertEqual(r.returncode,0);proof=json.loads(r.stdout);self.assertTrue(proof['owner_scope_CPU_passed']);self.assertGreaterEqual(proof['assertions'],12)
    def test_exact_GO_and_key_order_whitespace(self):
        for seq in (1,2):
            r=self.native('parse',NONCE,str(seq),body=self.reply(seq));self.assertEqual((r.returncode,r.stdout),(0,'go\n'))
        body=' { "status":"go", "gate_sequence":1,"run_id":"'+NONCE+'","protocol_version":1 }\r\n'
        self.assertEqual(self.native('parse',NONCE,'1',body=body).returncode,0)
    def test_nonce_sequence_and_version_refused(self):
        for kw in ({'run_id':'b'*32},{'run_id':'a'*31},{'run_id':'A'*32},{'gate_sequence':2},{'protocol_version':2}):
            self.assertNotEqual(self.native('parse',NONCE,'1',body=self.reply(**kw)).returncode,0)
    def test_malformed_duplicate_unknown_and_oversized_inputs_refused(self):
        good=self.reply();bad=[good[:-2],good+'{}',good.replace('"status": "go"','"status": "go", "status": "go"'),good.replace('"protocol_version": 1','"protocol_version": 01'),good.replace('"protocol_version": 1','"protocol_version": 1.0'),good.replace('"protocol_version": 1','"protocol_version": true'),good.replace('"status": "go"','"status": "go", "command": "run"'),good.replace('"status": "go"','"status": "g\\u006f"'),' '*257,good.replace('"status": "go"','"status": "unknown"')]
        for body in bad:self.assertNotEqual(self.native('parse',NONCE,'1',body=body).returncode,0,body)
    def test_refuse_is_recognized_and_wait_does_not_run_cases(self):
        r=self.native('parse',NONCE,'1',body=self.reply(status='refuse'));self.assertEqual((r.returncode,r.stdout),(0,'refuse\n'))
        for scenario in ('refuse','stale','reused'):
            r=self.native('wait',scenario);self.assertEqual(r.returncode,0);x=json.loads(r.stdout);self.assertTrue(x['failed']);self.assertEqual(x['pauses'],0)
    def test_bounded_timeout_and_delayed_GO(self):
        timeout=json.loads(self.native('wait','timeout').stdout);self.assertTrue(timeout['failed']);self.assertEqual(timeout['elapsed_ms'],30000)
        delayed=json.loads(self.native('wait','delayed').stdout);self.assertFalse(delayed['failed']);self.assertEqual(delayed['reads'],4);self.assertEqual(delayed['elapsed_ms'],15000)
        late=json.loads(self.native('wait','late').stdout);self.assertTrue(late['failed']);self.assertEqual(late['reads'],3)
    def test_failed_original_creation_cannot_own_poisoned_cleanup_output(self):
        bad=json.loads(self.native('cleanup','failure').stdout);self.assertEqual(bad['retained_handle'],0);self.assertFalse(bad['cleanup_owns_handle'])
        okay=json.loads(self.native('cleanup','success').stdout);self.assertEqual(okay['retained_handle'],0xdeadbeef);self.assertTrue(okay['cleanup_owns_handle'])
    def test_native_activation_headers_ignore_other_payloads(self):
        for scenario,n in (('enabled',1),('unknown_headers',3),('limit',128)):
            r=self.native('activation',scenario);self.assertEqual(r.returncode,0);x=json.loads(r.stdout);self.assertFalse(x['failed']);self.assertEqual(x['headers'],n);self.assertEqual(x['selected_payload_reads'],1)
    def test_native_activation_cycle_duplicate_limits_and_disabled_refused(self):
        for scenario in ('cycle','duplicate','missing','disabled','bad_bool','no_extension','duplicate_extension','overflow','qi_failed','getter_failed','null_handles','count_overflow','count_changed','wrong_root','size_zero','size_overflow'):
            r=self.native('activation',scenario);self.assertEqual(r.returncode,0);self.assertTrue(json.loads(r.stdout)['failed'],scenario)

if __name__=='__main__':
    if len(__import__('sys').argv)>1 and __import__('sys').argv[1]=='--write-oracle':
        parser=argparse.ArgumentParser();parser.add_argument('--write-oracle',required=True);a=parser.parse_args();print(json.dumps(write_oracle(a.write_oracle),indent=2))
    else:unittest.main()
