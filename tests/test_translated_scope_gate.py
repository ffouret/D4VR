import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools import translated_scope_gate as gate

NONCE='0123456789abcdef0123456789abcdef'
PID=1234
COOKIE=56789
UID=1000


def packet(value):return (json.dumps(value,separators=(',',':'))+'\n').encode()


def ready(sequence=1):
    return dict(protocol_version=1,run_id=NONCE,gate_sequence=sequence,status='ready',windows_pid=4321,device_interface=987654321,
                setup_fences_consumed=True,readback_fences_consumed=sequence==2,
                modules=[dict(role=r,path='Z:\\owned\\'+r+'.dll') for r in gate.READY_ROLES],
                interop=dict(requested=False,query_interface_hresult=None,get_vulkan_handles_hresult=None,outputs=None))


def claim(slot=0,pid=PID,cookie=COOKIE):return f'D4VR_QUERY_SCOPE_2 {NONCE} {slot} {pid} {cookie} {slot+1}\n'.encode()


def stat_data(pid=PID,cookie=COOKIE):
    return (str(pid)+' (owned name) (with parens) S '+'0 '*18+str(cookie)+' '+'0 '*30+'\n').encode()


def policy(sequence=1):
    return dict(run_id=NONCE,gate_sequence=sequence,uid=UID,pe_argument='/owned/fixture.dll',modules=[
        dict(role=r,map_path='/owned/'+r+'.dll',read_path='/read/pinned/'+r+'.dll',ready_path=None if r=='observer' else 'Z:\\owned\\'+r+'.dll',sha256=str(i+1)*64,size=100+i,device_major=8,device_minor=1,inode=100+i)
        for i,r in enumerate(gate.ROLES)])


def pins(p):return [{k:m[k] for k in ('role','sha256','size','device_major','device_minor','inode')} for m in p['modules']]


def maps_data(p=None):
    p=p or policy();return ''.join(f'{0x1000+i*0x2000:x}-{0x2000+i*0x2000:x} r-xp 00000000 08:01 {m["inode"]} {m["map_path"]}\n' for i,m in enumerate(p['modules'])).encode()


def snapshot(pid=PID,cookie=COOKIE,p=None):
    p=p or policy();cmd=packet_cmd(p['pe_argument'],NONCE);maps=maps_data(p)
    return dict(uid=UID,stat_before=stat_data(pid,cookie),stat_after=stat_data(pid,cookie),cmdline_before=cmd,cmdline_after=cmd,maps_before=maps,maps_after=maps)


def packet_cmd(path,nonce):return b'wine\0'+path.encode()+b'\0--run-id\0'+nonce.encode()+b'\0'


def correlation(sequence=1,claims=None,snapshots=None,p=None,**kw):
    p=p or policy(sequence);claims=claims or {0:claim()};snapshots=snapshots if snapshots is not None else {PID:snapshot(p=p)}
    return gate.correlate_processes(packet(ready(sequence)),claims,kw.get('after',claims),snapshots,p,kw.get('beforepins',pins(p)),kw.get('afterpins',pins(p)))


class Clock:
    def __init__(self):self.value=0.;self.sleeps=0
    def __call__(self):return self.value
    def sleep(self,n):self.value+=n;self.sleeps+=1


class ScopeGateTests(unittest.TestCase):
    def setUp(self):
        self.blockers=[patch('subprocess.run',side_effect=AssertionError('Subprocess forbidden')),patch('subprocess.Popen',side_effect=AssertionError('Subprocess forbidden'))]
        for p in self.blockers:p.start();self.addCleanup(p.stop)

    def test_exact_ready_and_go(self):
        for seq in (1,2):
            self.assertEqual(gate.parse_ready(packet(ready(seq)),NONCE,seq),ready(seq))
            for allow in (False,True):self.assertEqual(gate.parse_go(gate.encode_go(NONCE,seq,allow),NONCE,seq)['status'],'go' if allow else 'refuse')

    def test_packet_nonce_sequence_type_and_commands(self):
        for key,value in [('run_id','f'*32),('gate_sequence',2),('protocol_version',True),('windows_pid',True),('device_interface',-1),('status','run command'),('setup_fences_consumed',False),('readback_fences_consumed',True)]:
            v=ready();v[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):gate.parse_ready(packet(v),NONCE,1)
        v=ready();v['command']='echo bad'
        with self.assertRaises(ValueError):gate.parse_ready(packet(v),NONCE,1)
        for body in (b'{"protocol_version":1,"protocol_version":1}\n',b'{}',b'x'*16385+b'\n',b'{"x":NaN}\n',b'{"x":1.0}\n'):
            with self.assertRaises(ValueError):gate.parse_ready(body,NONCE,1)
        with self.assertRaises(ValueError):gate.parse_go(gate.encode_go(NONCE,1,True),NONCE,2)

    def test_failed_interop_outputs_stay_unavailable(self):
        for result in (-2147467262,-1,None):
            v=ready();v['interop'].update(requested=True,query_interface_hresult=result)
            self.assertIsNone(gate.parse_ready(packet(v),NONCE,1)['interop']['outputs'])
            v['interop']['outputs']=dict(instance=1,physical_device=2,device=3)
            with self.assertRaises(ValueError):gate.parse_ready(packet(v),NONCE,1)
        v=ready();v['interop'].update(requested=True,query_interface_hresult=0,get_vulkan_handles_hresult=0,outputs=dict(instance=10,physical_device=20,device=30))
        self.assertEqual(gate.parse_ready(packet(v),NONCE,1)['interop']['outputs']['device'],30)
        # These diagnostic fields never enter the host tuple/device-owner result.
        p=policy();out=gate.correlate_processes(packet(v),{0:claim()},{0:claim()},{PID:snapshot()},p,pins(p),pins(p))
        self.assertTrue(out['process_correlation_known']);self.assertFalse(out['Unix_device_owner_known'])

    def test_ready_modules_exact_bounded(self):
        for change in ('duplicate','oversize','missing','extra'):
            v=ready()
            if change=='duplicate':v['modules'][1]=v['modules'][0]
            elif change=='oversize':v['modules'][0]['path']='a'*2049
            elif change=='missing':v['modules'].pop()
            else:v['modules'][0]['extra']=1
            with self.subTest(change=change),self.assertRaises(ValueError):gate.parse_ready(packet(v),NONCE,1)

    def test_claim_bounds_nonce_and_generations(self):
        self.assertEqual(gate.parse_claims({0:claim()},NONCE)[0]['host_pid'],PID)
        for claims in ({},{8:claim(8)},{0:claim().replace(NONCE.encode(),b'f'*32)}, {0:claim(),1:claim(1,cookie=COOKIE+1)}, {False:claim()}, {0:b'x'*257}):
            with self.subTest(claims=claims),self.assertRaises(ValueError):gate.parse_claims(claims,NONCE)

    def test_stat_exact_cookie_and_complex_comm(self):
        self.assertEqual(gate.parse_stat(stat_data(),PID),COOKIE)
        for data in (stat_data(pid=PID+1),b'1234 (bad) S 0\n',stat_data()[:-1],b'x'*4097):
            with self.assertRaises(ValueError):gate.parse_stat(data,PID)

    def test_cmdline_nul_not_shell(self):
        self.assertEqual(gate.parse_cmdline(b'a\0$();`not executed`\0'),['a','$();`not executed`'])
        for value in (b'a',b'',b'a'*65537,b'\xff\0',b'\0'*513):
            with self.assertRaises(ValueError):gate.parse_cmdline(value)

    def test_maps_exact_fields_paths_and_limits(self):
        rows=gate.parse_maps(maps_data());self.assertEqual(len(rows),5);self.assertEqual(rows[0]['inode'],100)
        for data in (maps_data()[:-1],maps_data()+maps_data(),b'bad fields\n',b'x'*(gate.MAPS_LIMIT+1)):
            with self.assertRaises(ValueError):gate.parse_maps(data)
        self.assertTrue(gate.parse_maps(b'1000-2000 rw-p 00000000 00:00 0\n'))

    def test_positive_correlation_no_windows_numeric_join(self):
        out=correlation();self.assertTrue(out['process_correlation_known']);self.assertEqual(out['selected_process'],dict(host_pid=PID,process_start_cookie=COOKIE))
        self.assertNotEqual(ready()['windows_pid'],PID)
        self.assertTrue(all(out[k] is False for k in gate.FALSE_FACTS));gate.validate_report(out,NONCE,1)

    def test_pid_reuse_owner_and_permission_gaps(self):
        for key,value in [('stat_after',stat_data(cookie=COOKIE+1)),('stat_before',stat_data(cookie=COOKIE+1)),('uid',UID+1)]:
            v=snapshot();v[key]=value;out=correlation(snapshots={PID:v});self.assertFalse(out['process_correlation_known']);self.assertIn('claimed_process_inventory_gap',out['reasons'])
        self.assertFalse(correlation(snapshots={})['process_correlation_known'])
        with self.assertRaises(ValueError):correlation(snapshots={PID:snapshot(),99:snapshot(99)})

    def test_maps_missing_deleted_basename_wrong_inode_namespace(self):
        for value in (maps_data().replace(b'/owned/fixture.dll',b'/other/fixture.dll'),maps_data().replace(b'/owned/fixture.dll',b'/owned/fixture.dll (deleted)'),maps_data().replace(b' 100 ',b' 999 '),maps_data().replace(b'/owned/fixture.dll',b'/alias/fixture.dll')):
            v=snapshot();v['maps_before']=v['maps_after']=value
            self.assertFalse(correlation(snapshots={PID:v})['process_correlation_known'])

    def test_cmdline_mutation_absent_nonce_and_ambiguity(self):
        for cmd in (packet_cmd('/other/fixture.dll',NONCE),packet_cmd('/owned/fixture.dll','f'*32),packet_cmd('/owned/fixture.dll',NONCE)+b'/owned/fixture.dll\0'):
            v=snapshot();v['cmdline_before']=v['cmdline_after']=cmd;self.assertFalse(correlation(snapshots={PID:v})['process_correlation_known'])
        v=snapshot();v['cmdline_after']+=b'extra\0';self.assertFalse(correlation(snapshots={PID:v})['process_correlation_known'])
        v=snapshot();v['maps_after']=v['maps_before'].replace(b'r-xp',b'rw-p');self.assertFalse(correlation(snapshots={PID:v})['process_correlation_known'])

    def test_two_candidates_or_partial_second_candidate_block_unique(self):
        claims={0:claim(),1:claim(1,pid=2222,cookie=3333)};second=snapshot(2222,3333)
        self.assertFalse(correlation(claims=claims,snapshots={PID:snapshot(),2222:second})['process_correlation_known'])
        second['maps_before']=second['maps_after']=b'1000-2000 r-xp 00000000 08:01 100 /owned/fixture.dll\n'
        self.assertFalse(correlation(claims=claims,snapshots={PID:snapshot(),2222:second})['process_correlation_known'])

    def test_known_helper_can_be_excluded_without_scan(self):
        claims={0:claim(),1:claim(1,pid=2222,cookie=3333)};helper=snapshot(2222,3333)
        helper['cmdline_before']=helper['cmdline_after']=b'wineserver\0'
        helper['maps_before']=helper['maps_after']=b'1000-2000 r-xp 00000000 08:01 104 /owned/observer.dll\n'
        out=correlation(claims=claims,snapshots={PID:snapshot(),2222:helper});self.assertTrue(out['process_correlation_known'])

    def test_fingerprints_and_ready_path_crosschecks(self):
        p=policy();changed=pins(p);changed[0]['sha256']='f'*64
        self.assertFalse(correlation(afterpins=changed)['process_correlation_known'])
        changed=pins(p);changed[0]['inode']+=1;self.assertFalse(correlation(beforepins=changed)['process_correlation_known'])
        v=ready();v['modules'][0]['path']='Z:\\other\\fixture.dll'
        out=gate.correlate_processes(packet(v),{0:claim()},{0:claim()},{PID:snapshot()},p,pins(p),pins(p));self.assertFalse(out['process_correlation_known'])

    def test_claim_set_changes_during_inventory(self):
        self.assertFalse(correlation(after={0:claim(),1:claim(1)})['process_correlation_known'])
        with self.assertRaises(ValueError):gate.correlate_processes(packet(ready()),{0:claim()},{},{PID:snapshot()},policy(),pins(policy()),pins(policy()))

    def test_only_claimed_proc_files_read_and_deduplicated(self):
        calls=[];v=snapshot()
        def reader(pid,name,limit):
            calls.append((pid,name,limit));return dict(data=v[name+'_before'],uid=UID)
        result=gate.collect_claimed_processes({0:claim(),1:claim(1)},NONCE,reader=reader)
        self.assertEqual(list(result),[PID]);self.assertEqual(len(calls),6);self.assertEqual({x[0] for x in calls},{PID});self.assertEqual({x[1] for x in calls},{'stat','cmdline','maps'})
        def denied(*_):raise PermissionError('owned proc inaccessible')
        self.assertEqual(gate.collect_claimed_processes({0:claim()},NONCE,reader=denied),{})

    def test_proc_reader_overlimit_callback_is_gap(self):
        def oversized(pid,name,limit):return dict(data=b'x'*(limit+1),uid=UID)
        self.assertEqual(gate.collect_claimed_processes({0:claim()},NONCE,reader=oversized),{})
        with self.assertRaises(ValueError):gate.read_proc(PID,'mem',1)
        with self.assertRaises(ValueError):gate.read_proc(PID,'environ',65536)

    def test_gate_session_two_sequences_and_immutable_first_receipt(self):
        session=gate.GateSession(NONCE);r1=correlation();copy_r1=copy.deepcopy(r1);read=ready();returned=session.accept_ready(packet(read));returned['windows_pid']=0
        self.assertEqual(gate.parse_go(session.release(r1),NONCE,1)['status'],'go')
        r1['selected_process']['host_pid']=999
        session.accept_ready(packet(ready(2)));r2=correlation(2)
        self.assertEqual(gate.parse_go(session.release(r2),NONCE,2)['status'],'go')
        self.assertTrue(gate.audit_gate_pair(copy_r1,r2,{0:claim()},NONCE)['process_correlation_at_both_gates'])
        with self.assertRaises(ValueError):session.accept_ready(packet(ready(2)))

    def test_session_refusal_reuse_and_unknown_report(self):
        session=gate.GateSession(NONCE);session.accept_ready(packet(ready()));self.assertEqual(gate.parse_go(session.release(allow=False),NONCE,1)['status'],'refuse')
        with self.assertRaises(ValueError):session.accept_ready(packet(ready(2)))
        session=gate.GateSession(NONCE);session.accept_ready(packet(ready()))
        with self.assertRaises(ValueError):session.release(correlation(snapshots={}))
        with self.assertRaises(ValueError):session.accept_ready(packet(ready()))

    def test_changed_process_and_late_claims_remain_unknown(self):
        first=correlation();second=correlation(2);late={0:claim(),1:claim(1)}
        self.assertFalse(gate.audit_gate_pair(first,second,late,NONCE)['process_correlation_at_both_gates'])
        claims={0:claim(pid=2222,cookie=3333)};new=correlation(2,claims=claims,snapshots={2222:snapshot(2222,3333)})
        self.assertFalse(gate.audit_gate_pair(first,new,claims,NONCE)['process_correlation_at_both_gates'])
        s=gate.GateSession(NONCE);s.accept_ready(packet(ready()));s.release(first);s.accept_ready(packet(ready(2)))
        with self.assertRaises(ValueError):s.release(new)

    def test_report_stronger_facet_tamper_refused(self):
        for key in gate.FALSE_FACTS:
            v=correlation();v[key]=True
            with self.subTest(key=key),self.assertRaises(ValueError):gate.validate_report(v,NONCE,1)
        v=correlation();v['selected_process']['host_pid']=True
        with self.assertRaises(ValueError):gate.validate_report(v,NONCE,1)

    def test_candidate_requires_parsed_hashes_and_at_least_five_map_lines(self):
        for key in ('cmdline_sha256','maps_sha256','map_lines'):
            v=correlation();del v['inventoried_processes'][0][key]
            with self.subTest(key=key),self.assertRaises(ValueError):gate.validate_report(v,NONCE,1)
        for lines in (0,1,4):
            v=correlation();v['inventoried_processes'][0]['map_lines']=lines
            with self.subTest(lines=lines),self.assertRaises(ValueError):gate.validate_report(v,NONCE,1)
        v=correlation();v['inventoried_processes'][0]['reasons']=['gap']
        with self.assertRaises(ValueError):gate.validate_report(v,NONCE,1)

    def test_known_report_cannot_hide_failed_second_claimed_inventory(self):
        v=correlation();v['claim_identities'].append(dict(scope_slot=1,host_pid=2222,process_start_cookie=3333,dso_epoch=2))
        v['inventoried_processes'].append(dict(host_pid=2222,process_start_cookie=3333,candidate=False,reasons=['missing_process_snapshot'],module_roles=[]))
        with self.assertRaises(ValueError):gate.validate_report(v,NONCE,1)
        row=v['inventoried_processes'][-1];row.update(cmdline_sha256='a'*64,maps_sha256='b'*64,map_lines=1)
        with self.assertRaises(ValueError):gate.validate_report(v,NONCE,1)

    def test_timeout_frozen_clock_rollback_and_late_read(self):
        clock=Clock()
        def absent(*_):raise FileNotFoundError()
        with self.assertRaises(TimeoutError):gate.wait_packet('/unused','ready-1.json',NONCE,1,timeout=.1,clock=clock,sleep=clock.sleep,reader=absent)
        self.assertLessEqual(clock.sleeps,3)
        with self.assertRaises(TimeoutError):gate.wait_packet('/unused','ready-1.json',NONCE,1,timeout=30,clock=lambda:0.,sleep=lambda _:None,reader=absent)
        clock=Clock()
        def late(*_):clock.value=31;return packet(ready())
        with self.assertRaises(TimeoutError):gate.wait_packet('/unused','ready-1.json',NONCE,1,clock=clock,reader=late)
        values=iter((10.,9.))
        with self.assertRaises(ValueError):gate.wait_packet('/unused','ready-1.json',NONCE,1,clock=lambda:next(values),reader=absent)
        clock=Clock();s=gate.GateSession(NONCE,clock=clock);s.accept_ready(packet(ready()));clock.value=31
        with self.assertRaises(TimeoutError):s.release(correlation())

    def test_wait_valid_and_malformed_reply_no_retry(self):
        clock=Clock();out=gate.wait_packet('/unused','ready-1.json',NONCE,1,clock=clock,reader=lambda *_:packet(ready()))
        self.assertEqual(out['run_id'],NONCE)
        calls=[]
        def bad(*_):calls.append(1);return b'{}\n'
        with self.assertRaises(ValueError):gate.wait_packet('/unused','go-1.json',NONCE,1,clock=clock,reader=bad)
        self.assertEqual(len(calls),1)
        with self.assertRaises(ValueError):gate.wait_packet('/unused','../../mem',NONCE,1,reader=bad)

    def test_atomic_create_only_regular_files_symlinks_and_size(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)/'gate';directory.mkdir(mode=0o700)
            gate.publish_packet(directory,'ready-1.json',packet(ready()))
            self.assertEqual(gate.read_gate_file(directory,'ready-1.json',gate.READY_LIMIT),packet(ready()))
            with self.assertRaises(FileExistsError):gate.publish_packet(directory,'ready-1.json',packet(ready()))
            self.assertEqual(gate.read_gate_file(directory,'ready-1.json',gate.READY_LIMIT),packet(ready()))
            if os.name!='nt':
                (directory/'go-1.json').symlink_to(directory/'ready-1.json')
                with self.assertRaises(ValueError):gate.read_gate_file(directory,'go-1.json',gate.GO_LIMIT)
            else:
                with patch.object(Path,'is_symlink',return_value=True),self.assertRaises(ValueError):gate.read_gate_file(directory,'go-1.json',gate.GO_LIMIT)
            with self.assertRaises(ValueError):gate.publish_packet(directory,'go-2.json',b'garbage')
            (directory/'go-2.json').write_bytes(b'x'*257)
            with self.assertRaises(ValueError):gate.read_gate_file(directory,'go-2.json',gate.GO_LIMIT)
            if os.name=='posix':
                directory.chmod(0o755)
                with self.assertRaises(ValueError):gate.read_gate_file(directory,'ready-1.json',gate.READY_LIMIT)

    def test_regular_only_rejects_directory_and_fifo_without_wait(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)/'gate';directory.mkdir(mode=0o700);(directory/'ready-1.json').mkdir()
            # Windows may reject the directory at os.open before the regular-file guard.
            with self.assertRaises((ValueError,OSError)):gate.read_gate_file(directory,'ready-1.json',gate.READY_LIMIT)
            if os.name=='posix':
                os.mkfifo(directory/'go-1.json')
                with self.assertRaises(ValueError):gate.read_gate_file(directory,'go-1.json',gate.GO_LIMIT)

    def test_maps_line_limit_and_callback_owner_change(self):
        data=b''.join(f'{i*8192:x}-{i*8192+4096:x} rw-p 00000000 00:00 0\n'.encode() for i in range(8193))
        self.assertLess(len(data),gate.MAPS_LIMIT)
        with self.assertRaises(ValueError):gate.parse_maps(data)
        calls=[];v=snapshot()
        def changing(pid,name,limit):
            calls.append(name);return dict(data=v[name+'_before'],uid=UID if len(calls)<4 else UID+1)
        self.assertEqual(gate.collect_claimed_processes({0:claim()},NONCE,reader=changing),{})

    def test_claim_reader_fixed_names_no_directory_or_proc_scan(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)/'scope';directory.mkdir(mode=0o700);(directory/'slot-0.claim').write_bytes(claim())
            with patch('os.listdir',side_effect=AssertionError('No global listing')),patch.object(Path,'iterdir',side_effect=AssertionError('No listing')):
                self.assertEqual(gate.read_claims(directory,NONCE),{0:claim()})
            (directory/'overflow').write_bytes(b'marker')
            with self.assertRaises(ValueError):gate.read_claims(directory,NONCE)

    def test_fingerprint_owned_file_no_mapped_memory(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'owned.dll';path.write_bytes(b'owned file bytes');module=dict(read_path=str(path),role='fixture')
            if not hasattr(os,'major'):
                with self.assertRaises(ValueError):gate.fingerprint_module(module)
                return
            pin=gate.fingerprint_module(module);self.assertEqual(pin['sha256'],hashlib.sha256(path.read_bytes()).hexdigest());self.assertEqual(pin['size'],16)
            alias=Path(tmp)/'alias';alias.symlink_to(path)
            with self.assertRaises(ValueError):gate.fingerprint_module(dict(read_path=str(alias),role='fixture'))


if __name__=='__main__':unittest.main()
