"""Bounded caller-owned ready/go and claimed-process correlation, never a device bridge.

Only explicit scope-2 claim PIDs may be read. No process enumeration, environment
read, mapped-memory access, command execution or PE/Unix handle conversion exists.
The pure model does not attest its supplied facts or prove a security boundary.
"""
from __future__ import annotations
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import stat
import time

READY_LIMIT=16384
GO_LIMIT=256
CLAIM_LIMIT=256
STAT_LIMIT=4096
CMDLINE_LIMIT=65536
MAPS_LIMIT=1048576
MAPS_LINES=8192
MAX_PID=0xffffffff
MAX_U64=(1<<64)-1
MODULE_LIMIT=64*1024*1024
ROLES=('fixture','d3d12','d3d12core','dxgi','observer')
READY_ROLES=ROLES[:-1]
FALSE_FACTS=('Windows_PID_used_for_host_identity','Unix_device_owner_known',
             'PE_to_Unix_handle_mapping_known','application_to_layer_physical_mapping_known',
             'application_to_layer_create_output_slot_mapping_known',
             'selected_slot_placement_known','current_contents_at_draw_known',
             'sampled_resource_GPU_read_proven','global_query_absence_proven')


def require(value,message):
    if not value:raise ValueError(message)


def uint(value,label,maximum=MAX_U64,minimum=0):
    require(type(value) is int and minimum<=value<=maximum,'Integer bound: '+label)
    return value


def exact_keys(value,keys,label):
    require(type(value) is dict and set(value)==set(keys),'Exact object: '+label)


def nonce(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{32}',value) is not None,'Fresh hexadecimal run nonce')
    return value


def text(value,label,limit=4096):
    require(type(value) is str and 0<len(value.encode('utf-8',errors='strict'))<=limit and '\x00' not in value,'Bounded text: '+label)
    require(not any(ord(c)<32 for c in value),'Control character: '+label)
    return value


def sha(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{64}',value) is not None,'SHA256')
    return value


def digest(data):return hashlib.sha256(data).hexdigest()


def _pairs(items):
    out={}
    for k,v in items:
        require(k not in out,'Duplicate JSON key');out[k]=v
    return out


def _integer(value):
    require(len(value.lstrip('-'))<=20,'JSON integer digit bound');return int(value)


def loads(data,limit):
    require(type(data) is bytes and len(data)<=limit and data.endswith(b'\n'),'Bounded newline JSON packet')
    try:value=json.loads(data,object_pairs_hook=_pairs,parse_int=_integer,parse_float=lambda _:require(False,'Float refused'),parse_constant=lambda _:require(False,'Nonfinite refused'))
    except (UnicodeError,json.JSONDecodeError,RecursionError) as e:raise ValueError('Malformed JSON') from e
    pending=[(value,0)];nodes=0
    while pending:
        v,depth=pending.pop();nodes+=1;require(nodes<=512 and depth<=8,'JSON structural bound')
        if type(v) is dict:pending.extend((x,depth+1) for x in v.values())
        elif type(v) is list:pending.extend((x,depth+1) for x in v)
    return value


def parse_ready(data,run_id,sequence):
    nonce(run_id);uint(sequence,'gate sequence',2,1)
    value=loads(data,READY_LIMIT)
    exact_keys(value,('protocol_version','run_id','gate_sequence','status','windows_pid','device_interface','setup_fences_consumed','readback_fences_consumed','modules','interop'),'ready')
    require(type(value['protocol_version']) is int and value['protocol_version']==1,'Protocol version')
    require(value['run_id']==run_id and type(value['gate_sequence']) is int and value['gate_sequence']==sequence,'Ready nonce/sequence')
    require(type(value['status']) is str and value['status']=='ready','Ready status');uint(value['windows_pid'],'Windows diagnostic PID',MAX_PID,1);uint(value['device_interface'],'PE diagnostic interface',minimum=1)
    require(value['setup_fences_consumed'] is True,'Setup fence consumed')
    require(value['readback_fences_consumed'] is (sequence==2),'Readback fence phase')
    require(type(value['modules']) is list and len(value['modules'])==4,'Four original PE module paths')
    seen=set()
    for m in value['modules']:
        exact_keys(m,('role','path'),'ready module');require(type(m['role']) is str and m['role'] in READY_ROLES and m['role'] not in seen,'Unique ready module role');seen.add(m['role']);text(m['path'],'PE module path',2048)
    interop=value['interop'];exact_keys(interop,('requested','query_interface_hresult','get_vulkan_handles_hresult','outputs'),'interop diagnostic')
    require(type(interop['requested']) is bool,'Interop requested bool')
    qi,get=interop['query_interface_hresult'],interop['get_vulkan_handles_hresult']
    for h in (qi,get):
        require(h is None or (type(h) is int and -(1<<31)<=h<(1<<31)),'Signed HRESULT or null')
    if not interop['requested']:require(qi is None and get is None and interop['outputs'] is None,'Unrequested getter is unavailable')
    elif qi is None or qi<0:require(get is None and interop['outputs'] is None,'Failed/unreturned QI has no getter outputs')
    elif get is None or get<0:require(interop['outputs'] is None,'Failed/unreturned getter outputs unavailable')
    else:
        exact_keys(interop['outputs'],('instance','physical_device','device'),'PE getter outputs')
        for k,v in interop['outputs'].items():uint(v,'PE '+k)
    return value


def encode_go(run_id,sequence,allow):
    nonce(run_id);uint(sequence,'gate sequence',2,1);require(type(allow) is bool,'Go decision bool')
    return (json.dumps(dict(protocol_version=1,run_id=run_id,gate_sequence=sequence,status='go' if allow else 'refuse'),separators=(',',':'))+'\n').encode()


def parse_go(data,run_id,sequence):
    nonce(run_id);uint(sequence,'gate sequence',2,1);v=loads(data,GO_LIMIT)
    exact_keys(v,('protocol_version','run_id','gate_sequence','status'),'go')
    require(type(v['protocol_version']) is int and v['protocol_version']==1 and v['run_id']==run_id and type(v['gate_sequence']) is int and v['gate_sequence']==sequence,'GO version/nonce/sequence')
    require(type(v['status']) is str and v['status'] in ('go','refuse'),'Decision contains no command');return v


def parse_claims(claims,run_id):
    nonce(run_id);require(type(claims) is dict and 1<=len(claims)<=8,'One to eight explicit claims')
    result=[]
    for slot,data in sorted(claims.items()):
        uint(slot,'claim slot',7);require(type(data) is bytes and len(data)<=CLAIM_LIMIT,'Bounded claim')
        m=re.fullmatch(rb'D4VR_QUERY_SCOPE_2 ([0-9a-f]{32}) ([0-7]) ([1-9][0-9]{0,9}) ([1-9][0-9]{0,19}) ([1-8])\n',data)
        require(m is not None,'Exact scope-2 claim')
        n,s,p,c,e=m.groups();n=n.decode();s,p,c,e=map(int,(s,p,c,e))
        require(n==run_id and s==slot and e==slot+1,'Claim nonce/slot/epoch')
        uint(p,'claim PID',MAX_PID,1);uint(c,'claim cookie',minimum=1)
        result.append(dict(scope_slot=slot,host_pid=p,process_start_cookie=c,dso_epoch=e))
    cookies={}
    for c in result:
        pid=c['host_pid'];cookie=c['process_start_cookie'];require(pid not in cookies or cookies[pid]==cookie,'One live cookie per claimed PID');cookies[pid]=cookie
    return result


def parse_stat(data,pid):
    uint(pid,'stat PID',MAX_PID,1);require(type(data) is bytes and 0<len(data)<=STAT_LIMIT and data.endswith(b'\n'),'Bounded stat')
    start=data.find(b' (');end=data.rfind(b')')
    require(start>0 and end>start+1 and data[end+1:end+2]==b' ','Stat comm framing')
    require(data[:start]==str(pid).encode(),'Stat exact PID')
    fields=data[end+2:].split();require(len(fields)>=20 and len(fields[0])==1,'Stat fields')
    token=fields[19];require(re.fullmatch(rb'[1-9][0-9]{0,19}',token) is not None,'Stat starttime field22');return uint(int(token),'stat cookie',minimum=1)


def parse_cmdline(data):
    require(type(data) is bytes and 0<len(data)<=CMDLINE_LIMIT and data.endswith(b'\0'),'Bounded NUL command line')
    pieces=data[:-1].split(b'\0');require(len(pieces)<=512 and all(len(x)<=8192 for x in pieces),'Command line argument bounds')
    try:return [x.decode('utf-8',errors='strict') for x in pieces]
    except UnicodeError as e:raise ValueError('Command line encoding unavailable') from e


def parse_maps(data):
    require(type(data) is bytes and 0<len(data)<=MAPS_LIMIT and data.endswith(b'\n'),'Bounded complete maps')
    lines=data.splitlines();require(len(lines)<=MAPS_LINES,'Maps line cap');rows=[];previous=0
    pattern=rb'([0-9a-f]{1,16})-([0-9a-f]{1,16}) ([r-][w-][x-][ps]) ([0-9a-f]{1,16}) ([0-9a-f]{1,8}):([0-9a-f]{1,8}) ([0-9]{1,20})(?: +(.*))?'
    for line in lines:
        m=re.fullmatch(pattern,line);require(m is not None,'Exact maps fields');lo,hi,perms,offset,major,minor,inode,path=m.groups()
        lo,hi,offset,major,minor=(int(x,16) for x in (lo,hi,offset,major,minor));inode=uint(int(inode),'maps inode')
        require(previous<=lo<hi<=MAX_U64,'Maps address ordering');previous=hi
        try:path=(path or b'').decode('utf-8',errors='strict')
        except UnicodeError as e:raise ValueError('Maps path encoding unavailable') from e
        require('\x00' not in path and len(path.encode())<=4096,'Bounded maps path')
        rows.append(dict(start=lo,end=hi,permissions=perms.decode(),offset=offset,device_major=major,device_minor=minor,inode=inode,path=path))
    return rows


def _posix_path(value,label):
    text(value,label);p=PurePosixPath(value);require(p.is_absolute() and not value.startswith('//') and '..' not in p.parts and str(p)==value,'Exact declared namespace path: '+label);return value


def validate_policy(policy):
    exact_keys(policy,('run_id','gate_sequence','uid','pe_argument','modules'),'process policy')
    nonce(policy['run_id']);uint(policy['gate_sequence'],'policy sequence',2,1);uint(policy['uid'],'owned UID',MAX_PID);text(policy['pe_argument'],'exact PE argument')
    require(type(policy['modules']) is list and len(policy['modules'])==5,'Five pinned module roles');seen=set();paths=set()
    for m in policy['modules']:
        exact_keys(m,('role','map_path','read_path','ready_path','sha256','size','device_major','device_minor','inode'),'module pin')
        require(type(m['role']) is str and m['role'] in ROLES and m['role'] not in seen,'Unique module role');seen.add(m['role'])
        for key in ('map_path','read_path'):_posix_path(m[key],key)
        require(m['map_path'] not in paths,'Unique exact map path');paths.add(m['map_path'])
        if m['role']=='observer':require(m['ready_path'] is None,'Observer is not a PE module')
        else:text(m['ready_path'],'ready path',2048)
        sha(m['sha256']);uint(m['size'],'module file size',MODULE_LIMIT,1);uint(m['inode'],'module inode',minimum=1)
        uint(m['device_major'],'module device major',MAX_PID);uint(m['device_minor'],'module device minor',MAX_PID)
    return policy


def _pins(pins,modules):
    require(type(pins) is list and len(pins)==5,'Five before/after file fingerprints');expected={m['role']:m for m in modules};seen=set()
    for p in pins:
        exact_keys(p,('role','sha256','size','device_major','device_minor','inode'),'file fingerprint')
        role=p['role'];require(role in expected and role not in seen,'Unique fingerprint role');seen.add(role)
        for key in p:
            require(type(p[key]) is type(expected[role][key]) and p[key]==expected[role][key],'Exact module fingerprint: '+key)


def correlate_processes(ready_bytes,claims_before,claims_after,snapshots,policy,file_pins_before,file_pins_after):
    validate_policy(policy);run_id=policy['run_id'];sequence=policy['gate_sequence'];ready=parse_ready(ready_bytes,run_id,sequence)
    claims=parse_claims(claims_before,run_id);after=parse_claims(claims_after,run_id)
    reasons=[]
    if claims!=after:reasons.append('claim_set_changed_during_inventory')
    try:_pins(file_pins_before,policy['modules']);_pins(file_pins_after,policy['modules'])
    except ValueError:reasons.append('module_file_fingerprint_changed_or_unavailable')
    ready_paths={m['role']:m['path'] for m in ready['modules']}
    if any(ready_paths[m['role']]!=m['ready_path'] for m in policy['modules'] if m['role']!='observer'):reasons.append('ready_module_paths_do_not_match_predeclared_policy')
    expected_pids={c['host_pid']:c['process_start_cookie'] for c in claims}
    require(type(snapshots) is dict and all(type(p) is int and p in expected_pids for p in snapshots),'Only claimed PID snapshots')
    inventories=[];candidates=[];gap=False
    for pid,cookie in sorted(expected_pids.items()):
        row=dict(host_pid=pid,process_start_cookie=cookie,candidate=False,reasons=[],module_roles=[])
        try:
            require(pid in snapshots,'missing_process_snapshot');v=snapshots[pid]
            exact_keys(v,('uid','stat_before','stat_after','cmdline_before','cmdline_after','maps_before','maps_after'),'process snapshot')
            require(type(v['uid']) is int and v['uid']==policy['uid'],'owned_user_mismatch')
            require(parse_stat(v['stat_before'],pid)==cookie and parse_stat(v['stat_after'],pid)==cookie,'PID_cookie_changed_or_reused')
            require(v['cmdline_before']==v['cmdline_after'],'command_line_changed');require(v['maps_before']==v['maps_after'],'maps_changed_during_inventory')
            args=parse_cmdline(v['cmdline_before']);maps=parse_maps(v['maps_before'])
            row.update(cmdline_sha256=digest(v['cmdline_before']),maps_sha256=digest(v['maps_before']),map_lines=len(maps))
            flags=[n for n,a in enumerate(args) if a=='--run-id']
            command=(args.count(policy['pe_argument'])==1 and len(flags)==1 and flags[0]+1<len(args) and args[flags[0]+1]==run_id and args.count(run_id)==1)
            if not command:row['reasons'].append('exact_PE_argument_and_nonce_not_unique')
            for m in policy['modules']:
                hits=[x for x in maps if x['path']==m['map_path']]
                matched=bool(hits) and all(all(x[k]==m[k] for k in ('device_major','device_minor','inode')) for x in hits)
                if matched:row['module_roles'].append(m['role'])
                else:row['reasons'].append('required_module_mapping_missing_deleted_or_mismatched:'+m['role'])
            if (command and len(row['module_roles'])!=5) or (len(row['module_roles'])==5 and not command):gap=True
            row['candidate']=command and len(row['module_roles'])==5
            if row['candidate']:candidates.append(dict(host_pid=pid,process_start_cookie=cookie))
        except (ValueError,TypeError,UnicodeError) as e:
            gap=True;row['reasons'].append(str(e))
        inventories.append(row)
    if gap:reasons.append('claimed_process_inventory_gap')
    if len(candidates)!=1:reasons.append('unique_owned_process_not_established')
    known=not reasons and len(candidates)==1
    return dict(schema_version=1,purpose='owned_claimed_process_correlation',run_id=run_id,gate_sequence=sequence,
                process_correlation_known=known,selected_process=candidates[0] if known else None,
                claim_identities=claims,inventoried_processes=inventories,reasons=reasons,input_facts_attested=False,
                cmdline_and_backing_maps_are_loaded_memory_attestation=False,
                **{key:False for key in FALSE_FACTS})


def _regular_read(path,limit,owned_uid=None):
    flags=os.O_RDONLY|getattr(os,'O_NONBLOCK',0)|getattr(os,'O_NOFOLLOW',0)|getattr(os,'O_CLOEXEC',0)|getattr(os,'O_BINARY',0)
    require(not Path(path).is_symlink(),'No symlink file');fd=os.open(path,flags)
    try:
        info=os.fstat(fd);require(stat.S_ISREG(info.st_mode) and info.st_size<=limit,'Bounded regular file')
        if owned_uid is not None:require(info.st_uid==owned_uid,'Owned user file')
        data=bytearray()
        while len(data)<=limit:
            chunk=os.read(fd,min(65536,limit+1-len(data)))
            if not chunk:break
            data.extend(chunk)
        require(len(data)<=limit,'File grew beyond bound');return bytes(data),info
    finally:os.close(fd)


def _directory(path):
    p=Path(path);require(p.is_absolute() and '..' not in p.parts,'Canonical absolute directory')
    for q in [p,*p.parents]:require(not q.is_symlink(),'No symlink path components')
    info=p.stat();require(stat.S_ISDIR(info.st_mode),'Existing directory')
    if os.name=='posix':require(stat.S_IMODE(info.st_mode)==0o700 and info.st_uid==os.geteuid(),'Owned0700 directory')
    return p


def read_gate_file(directory,name,limit):
    require(re.fullmatch(r'(ready|go)-[12]\.json',name) is not None,'Fixed gate filename')
    return _regular_read(_directory(directory)/name,limit,os.geteuid() if os.name=='posix' else None)[0]


def publish_packet(directory,name,data):
    require(re.fullmatch(r'(ready|go)-[12]\.json',name) is not None,'Fixed gate filename')
    limit=READY_LIMIT if name.startswith('ready') else GO_LIMIT
    require(type(data) is bytes and len(data)<=limit,'Bounded published packet')
    header=loads(data,limit);require(type(header) is dict,'Packet object');run=header.get('run_id');sequence=int(name[-6])
    (parse_ready if name.startswith('ready') else parse_go)(data,run,sequence)
    directory=_directory(directory);target=directory/name;temporary=directory/('.'+name+'.tmp')
    fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|getattr(os,'O_NOFOLLOW',0)|getattr(os,'O_CLOEXEC',0)|getattr(os,'O_BINARY',0),0o600)
    try:
        offset=0
        while offset<len(data):
            n=os.write(fd,data[offset:]);require(n>0,'Packet write');offset+=n
        os.fsync(fd)
    finally:os.close(fd)
    try:os.link(temporary,target,follow_symlinks=False) # atomic create-only, never replace
    finally:temporary.unlink()


def wait_packet(directory,name,run_id,sequence,*,timeout=30.0,clock=time.monotonic,sleep=time.sleep,reader=read_gate_file):
    require(type(timeout) in (int,float) and not isinstance(timeout,bool) and math.isfinite(timeout) and 0<=timeout<=30,'Bounded CPU timeout')
    require(name in ('ready-'+str(sequence)+'.json','go-'+str(sequence)+'.json'),'Exact gate filename/sequence')
    parser=parse_ready if name.startswith('ready-') else parse_go
    limit=READY_LIMIT if name.startswith('ready-') else GO_LIMIT
    started=clock();require(math.isfinite(started),'Finite monotonic clock');deadline=started+timeout;previous=started
    for _ in range(602):
        now=clock();require(math.isfinite(now) and now>=previous,'Monotonic clock');previous=now
        if now>deadline:raise TimeoutError('Ready/go deadline expired')
        try:data=reader(directory,name,limit)
        except FileNotFoundError:
            if now>=deadline:raise TimeoutError('Ready/go deadline expired')
            sleep(min(.05,deadline-now));continue
        finished=clock();require(math.isfinite(finished) and finished>=now,'Monotonic read completion')
        if finished>deadline:raise TimeoutError('Ready/go read exceeded deadline')
        return parser(data,run_id,sequence)
    raise TimeoutError('Clock failed to advance within bounded polls')


def read_claims(directory,run_id):
    directory=_directory(directory);nonce(run_id);claims={}
    if (directory/'overflow').exists() or (directory/'overflow').is_symlink():raise ValueError('Scope claim overflow marker')
    for slot in range(8):
        try:claims[slot]=_regular_read(directory/('slot-'+str(slot)+'.claim'),CLAIM_LIMIT,os.geteuid() if os.name=='posix' else None)[0]
        except FileNotFoundError:pass
    parse_claims(claims,run_id);return claims


def read_proc(pid,name,limit):
    require(os.name=='posix','Unix proc reader unavailable on this platform')
    uint(pid,'explicit claimed PID',MAX_PID,1)
    allowed={'stat':STAT_LIMIT,'cmdline':CMDLINE_LIMIT,'maps':MAPS_LIMIT}
    require(name in allowed and limit==allowed[name],'Only bounded stat/cmdline/maps')
    data,info=_regular_read('/proc/'+str(pid)+'/'+name,limit)
    return dict(data=data,uid=info.st_uid)


def collect_claimed_processes(claims,run_id,*,reader=read_proc):
    identities=parse_claims(claims,run_id);pids={c['host_pid'] for c in identities};snapshots={}
    for pid in sorted(pids):
        try:
            snapshot={};uids=set()
            for key,name,limit in (('stat_before','stat',STAT_LIMIT),('cmdline_before','cmdline',CMDLINE_LIMIT),('maps_before','maps',MAPS_LIMIT),('cmdline_after','cmdline',CMDLINE_LIMIT),('maps_after','maps',MAPS_LIMIT),('stat_after','stat',STAT_LIMIT)):
                result=reader(pid,name,limit);exact_keys(result,('data','uid'),'bounded proc reader');require(type(result['data']) is bytes and len(result['data'])<=limit,'Proc callback byte bound');uint(result['uid'],'proc file UID',MAX_PID)
                snapshot[key]=result['data'];uids.add(result['uid'])
            require(len(uids)==1,'Proc file owners changed');snapshot['uid']=next(iter(uids));snapshots[pid]=snapshot
        except (OSError,ValueError):pass # absent snapshot is an explicit model gap
    return snapshots


def fingerprint_module(module):
    require(hasattr(os,'major') and hasattr(os,'minor'),'Unix backing-file device IDs unavailable')
    path=Path(module['read_path']);require(path.is_absolute(),'Explicit module read path')
    for q in [path,*path.parents]:require(not q.is_symlink(),'No symlink module path')
    data,info=_regular_read(path,MODULE_LIMIT)
    return dict(role=module['role'],sha256=digest(data),size=len(data),device_major=os.major(info.st_dev),device_minor=os.minor(info.st_dev),inode=info.st_ino)


def validate_report(report,run_id,sequence):
    keys=('schema_version','purpose','run_id','gate_sequence','process_correlation_known','selected_process','claim_identities','inventoried_processes','reasons','input_facts_attested','cmdline_and_backing_maps_are_loaded_memory_attestation',*FALSE_FACTS)
    exact_keys(report,keys,'process correlation report')
    require(type(report['schema_version']) is int and report['schema_version']==1 and report['purpose']=='owned_claimed_process_correlation' and report['run_id']==run_id and type(report['gate_sequence']) is int and report['gate_sequence']==sequence,'Report schema/nonce/sequence')
    require(type(report['process_correlation_known']) is bool and report['input_facts_attested'] is False and report['cmdline_and_backing_maps_are_loaded_memory_attestation'] is False,'Report fact scope')
    require(all(report[k] is False for k in FALSE_FACTS),'No device/mapping promotion')
    claims=report['claim_identities'];require(type(claims) is list and 1<=len(claims)<=8,'Report claim cap');slots=set();tuples=set()
    for c in claims:
        exact_keys(c,('scope_slot','host_pid','process_start_cookie','dso_epoch'),'report claim')
        uint(c['scope_slot'],'report slot',7);uint(c['host_pid'],'report PID',MAX_PID,1);uint(c['process_start_cookie'],'report cookie',minimum=1)
        require(c['scope_slot'] not in slots and type(c['dso_epoch']) is int and c['dso_epoch']==c['scope_slot']+1,'Report immutable epoch');slots.add(c['scope_slot']);tuples.add((c['host_pid'],c['process_start_cookie']))
    require(type(report['reasons']) is list and len(report['reasons'])<=16,'Report reason cap')
    for reason in report['reasons']:text(reason,'report reason',512)
    rows=report['inventoried_processes'];require(type(rows) is list and len(rows)<=8,'Report inventory cap');inventory=set()
    for row in rows:
        require(type(row) is dict and set(row) in (set(('host_pid','process_start_cookie','candidate','reasons','module_roles')),set(('host_pid','process_start_cookie','candidate','reasons','module_roles','cmdline_sha256','maps_sha256','map_lines'))),'Report inventory shape')
        uint(row['host_pid'],'inventory PID',MAX_PID,1);uint(row['process_start_cookie'],'inventory cookie',minimum=1)
        pair=(row['host_pid'],row['process_start_cookie']);require(pair in tuples and pair not in inventory,'Only exact claimed inventory');inventory.add(pair)
        require(type(row['candidate']) is bool and type(row['module_roles']) is list and len(row['module_roles'])<=5 and all(type(x) is str and x in ROLES for x in row['module_roles']) and len(set(row['module_roles']))==len(row['module_roles']),'Bounded inventory roles')
        require(type(row['reasons']) is list and len(row['reasons'])<=8,'Inventory reasons')
        for reason in row['reasons']:text(reason,'inventory reason',512)
        if 'cmdline_sha256' in row:
            sha(row['cmdline_sha256']);sha(row['maps_sha256']);uint(row['map_lines'],'map line count',MAPS_LINES,1)
            require(row['map_lines']>=len(row['module_roles']),'Mappings cover observed roles')
        if row['candidate']:
            require('cmdline_sha256' in row and len(row['module_roles'])==5 and row['reasons']==[],'Candidate requires complete parsed hashes and five maps')
    selected=report['selected_process']
    if report['process_correlation_known']:
        exact_keys(selected,('host_pid','process_start_cookie'),'selected process')
        uint(selected['host_pid'],'selected PID',MAX_PID,1);uint(selected['process_start_cookie'],'selected cookie',minimum=1)
        require((selected['host_pid'],selected['process_start_cookie']) in tuples and inventory==tuples and report['reasons']==[],'Unique complete owned process')
        require(all('cmdline_sha256' in row for row in rows),'Known process requires complete inventory for every claimed PID')
        for row in rows:
            if not row['candidate']:
                missing=set(ROLES)-set(row['module_roles'])
                allowed={'exact_PE_argument_and_nonce_not_unique'}|{'required_module_mapping_missing_deleted_or_mismatched:'+role for role in missing}
                require(len(row['module_roles'])<5 and set(row['reasons'])==allowed and len(row['reasons'])==len(allowed),'No incomplete or partial candidate hidden as helper')
        candidates=[r for r in rows if r['candidate']]
        require(len(candidates)==1 and all(candidates[0][k]==selected[k] for k in selected) and len(candidates[0]['module_roles'])==5 and candidates[0]['reasons']==[],'Selected candidate proof')
    else:require(selected is None,'Unknown process has no selected tuple')
    return report


def audit_gate_pair(first,second,final_claims,run_id):
    nonce(run_id);validate_report(first,run_id,1);validate_report(second,run_id,2)
    final=parse_claims(final_claims,run_id);reasons=[]
    if not first['process_correlation_known'] or not second['process_correlation_known']:reasons.append('process_unknown_at_one_or_both_gates')
    if first['selected_process']!=second['selected_process']:reasons.append('process_tuple_changed_between_gates')
    old={c['scope_slot']:c for c in first['claim_identities']};new={c['scope_slot']:c for c in second['claim_identities']}
    if not all(new.get(k)==v for k,v in old.items()):reasons.append('claim_replaced_or_removed_between_gates')
    if final!=second['claim_identities']:reasons.append('later_uninventoried_claim_or_missing_final_claim')
    return dict(schema_version=1,purpose='owned_process_gate_pair',run_id=run_id,process_correlation_at_both_gates=not reasons,
                selected_process=second['selected_process'] if not reasons else None,reasons=reasons,input_facts_attested=False,**{k:False for k in FALSE_FACTS})


class GateSession:
    """Finite parent coordination; reports remain caller-owned, not security claims."""
    def __init__(self,run_id,*,timeout=30.0,clock=time.monotonic):
        require(type(timeout) in (int,float) and not isinstance(timeout,bool) and math.isfinite(timeout) and 0<timeout<=30,'Session deadline bound')
        self.timeout=timeout;self.clock=clock;self.deadline=None
        self.run_id=nonce(run_id);self.next_sequence=1;self.ready=None;self.first_ready=None;self.first_report=None;self.closed=False

    def accept_ready(self,data):
        require(not self.closed and self.ready is None and self.next_sequence<=2,'Gate not reused')
        value=parse_ready(data,self.run_id,self.next_sequence)
        if self.first_ready is not None:
            for key in ('windows_pid','device_interface','modules'):require(value[key]==self.first_ready[key],'Stable ready diagnostic: '+key)
        now=self.clock();require(math.isfinite(now),'Finite gate clock');self.deadline=now+self.timeout
        self.ready=json.loads(json.dumps(value))
        return value

    def release(self,report=None,*,allow=True):
        require(type(allow) is bool and not self.closed and self.ready is not None,'One ready per decision')
        if allow:
            now=self.clock();require(math.isfinite(now),'Finite release clock')
            if now>self.deadline:raise TimeoutError('Gate inventory/release deadline expired')
            validate_report(report,self.run_id,self.next_sequence)
            require(report['process_correlation_known'] is True,'Fresh bounded process correlation required')
            require(all(report.get(k) is False for k in FALSE_FACTS),'No device/mapping promotion')
            if self.first_report is not None:
                require(report['selected_process']==self.first_report['selected_process'],'Same process tuple at both gates')
                old={c['scope_slot']:c for c in self.first_report['claim_identities']};new={c['scope_slot']:c for c in report['claim_identities']}
                require(all(new.get(k)==v for k,v in old.items()),'Claims not removed/replaced between gates')
                inventoried={(x['host_pid'],x['process_start_cookie']) for x in report['inventoried_processes']}
                require(all((c['host_pid'],c['process_start_cookie']) in inventoried for c in new.values()),'Every new claim inventoried')
            if self.first_report is None:self.first_report=json.loads(json.dumps(report,allow_nan=False));self.first_ready=json.loads(json.dumps(self.ready))
        packet=encode_go(self.run_id,self.next_sequence,allow);self.ready=None;self.next_sequence+=1
        if not allow or self.next_sequence>2:self.closed=True
        return packet
