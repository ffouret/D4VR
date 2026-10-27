#!/usr/bin/env python3
"""Bounded readonly audit of per-process, per-DSO original-query journals.

This does not execute or control a process, read descriptor memory, or infer
coverage of an unjoined renderer, process, device, or loader namespace.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat

MAX_U64=(1<<64)-1
MAX_BYTES=1024*1024
FALSE_FACETS=('resource_generation_known','selected_slot_placement_known',
              'current_contents_at_draw_known','sampled_resource_GPU_read_proven',
              'exclusive_phase_attribution_proven')
IDENTITY=('run_id','scope_slot','host_pid','process_start_cookie','dso_epoch')


def require(condition,message):
    if not condition:raise ValueError(message)


def exact(a,b,label):
    require(type(a) is type(b),'Type mismatch: '+label)
    if isinstance(b,dict):
        require(set(a)==set(b),'Object keys: '+label)
        for k in b:exact(a[k],b[k],label+'.'+k)
    elif isinstance(b,list):
        require(len(a)==len(b),'List length: '+label)
        for i,(x,y) in enumerate(zip(a,b)):exact(x,y,label+'.'+str(i))
    else:require(a==b,'Value mismatch: '+label)


def obj(value,fields,label):
    require(type(value) is dict and set(value)==set(fields.split()),'Exact object keys: '+label)


def uint(value,label,maximum=MAX_U64):
    require(type(value) is int and 0<=value<=maximum,'Bounded unsigned integer: '+label)


def boolean(value,label):require(type(value) is bool,'Boolean: '+label)


def digest(data):return hashlib.sha256(data).hexdigest()


def sha(value,label):require(type(value) is str and re.fullmatch('[0-9a-f]{64}',value) is not None,'Explicit lowercase SHA256: '+label)


def nonce(value):require(type(value) is str and re.fullmatch('[0-9a-f]{32}',value) is not None,'Parent-pinned128-bit nonce')


def pairs(items):
    result={}
    for k,v in items:require(k not in result,'Duplicate JSON key');result[k]=v
    return result


def json_integer(text):
    require(len(text.lstrip('-'))<=20,'JSON integer digit bound');return int(text)


def loads(data):
    require(type(data) is bytes and len(data)<=MAX_BYTES,'Bounded JSON input')
    try:value=json.loads(data,object_pairs_hook=pairs,parse_int=json_integer,parse_constant=lambda _:require(False,'Nonfinite JSON'))
    except (json.JSONDecodeError,UnicodeError,RecursionError) as e:raise ValueError('Malformed JSON') from e
    pending=[(value,0)];nodes=0
    while pending:
        v,d=pending.pop();nodes+=1;require(nodes<=32768 and d<=16,'JSON structural bound')
        if type(v) is dict:pending.extend((x,d+1) for x in v.values())
        elif type(v) is list:pending.extend((x,d+1) for x in v)
        elif type(v) is float:require(math.isfinite(v),'Nonfinite JSON numeric value')
    return value


def safe_path(value):
    require(isinstance(value,(str,Path)),'Path string');p=Path(value)
    require('..' not in p.parts,'Parent traversal refused');p=p.absolute()
    for c in [p,*p.parents]:require(not c.is_symlink(),'Symlink path refused')
    return p.resolve()


def read_file(path,limit=MAX_BYTES):
    p=safe_path(path)
    flags=os.O_RDONLY|getattr(os,'O_NOFOLLOW',0)|getattr(os,'O_NONBLOCK',0)|getattr(os,'O_CLOEXEC',0)|getattr(os,'O_BINARY',0)
    fd=os.open(p,flags)
    try:
        info=os.fstat(fd);require(stat.S_ISREG(info.st_mode) and info.st_size<=limit,'Bounded regular file descriptor')
        data=bytearray()
        while len(data)<=limit:
            part=os.read(fd,min(65536,limit+1-len(data)))
            if not part:break
            data.extend(part)
        require(len(data)<=limit,'Input grew beyond bound');return bytes(data)
    finally:os.close(fd)


def identity(value):
    nonce(value['run_id']);uint(value['scope_slot'],'Scope slot',7)
    uint(value['host_pid'],'Host PID',(1<<32)-1);require(value['host_pid']>0,'Nonzero host PID')
    uint(value['process_start_cookie'],'Process start cookie');require(value['process_start_cookie']>0,'Nonzero process start cookie')
    exact(value['dso_epoch'],value['scope_slot']+1,'Persistent claim epoch label')


def phase(value):
    obj(value,'status case_ordinal api_phase','Phase')
    require(value['status'] in ('known','missing','invalid','io_error'),'Finite phase status')
    uint(value['case_ordinal'],'Phase case',8);uint(value['api_phase'],'Phase API',6)
    if value['status']!='known':exact(value['case_ordinal'],0,'Unknown phase case');exact(value['api_phase'],0,'Unknown phase API')


def false_facets(row):
    for k in FALSE_FACETS:exact(row[k],False,'Unsupported stronger claim '+k)


def query_payload(q):
    """Validate only captured original arguments/results, never memory contents."""
    for k in ('ordinal','operation_ordinal','return_ordinal','device','data_size','output_pointer','device_generation','parent_instance_generation'):uint(q[k],'Query '+k)
    for k in ('descriptor_type','info_sType'):uint(q[k],'Query '+k,(1<<32)-1)
    for k in ('info_present','chain_present','original_returned','phase_unchanged','output_captured','device_lifetime_known'):boolean(q[k],'Query '+k)
    require(0<q['ordinal']<=q['operation_ordinal'],'Original query entry indices')
    exact(q['return_kind'],'void','Original query return kind')
    if q['original_returned']:require(q['return_ordinal']>q['operation_ordinal'],'Original return follows entry')
    else:exact(q['return_ordinal'],0,'Pending original has no return ordinal');require(not q['output_captured'],'Pending output cannot be captured')
    if q['device_lifetime_known']:require(q['device']>0 and q['device_generation']>0 and q['parent_instance_generation']>0,'Known original device lifetime')
    else:exact(q['device_generation'],0,'Unknown device generation');exact(q['parent_instance_generation'],0,'Unknown parent generation')
    phase(q['phase_before']);phase(q['phase_after']);b,a=q['phase_before'],q['phase_after']
    require(b['status']=='known' and 1<=b['case_ordinal']<=8,'Retained selected case only')
    same=b['status']==a['status']=='known' and b['case_ordinal']==a['case_ordinal'] and b['api_phase']==a['api_phase']
    exact(q['phase_unchanged'],same,'Observed phase equality')
    if not q['original_returned']:exact(a,dict(status='missing',case_ordinal=0,api_phase=0),'Pending original has no AFTER observation')
    x=q['typed_input'];obj(x,'known kind reason raw image_view sampler image_layout address range format','Typed input')
    boolean(x['known'],'Typed input known')
    for k in ('raw','image_view','sampler','address','range'):uint(x[k],'Input '+k)
    for k in ('image_layout','format'):uint(x[k],'Input '+k,(1<<32)-1)
    kind={0:'sampler',1:'image_view',2:'image_view',3:'image_view',10:'image_view',4:'buffer_address',5:'buffer_address',6:'buffer_address',7:'buffer_address',1000150000:'acceleration_address'}.get(q['descriptor_type'])
    require(x['kind'] in ('unknown','sampler','image_view','buffer_address','acceleration_address'),'Selected input union')
    if x['known']:
        exact(x['reason'],'none','Known input reason')
        require(q['info_present'] and q['info_sType']==1000316004 and not q['chain_present'],'Known original info header')
        exact(x['kind'],kind,'Selected original descriptor union')
        if kind=='sampler':require(x['raw']==x['sampler'] and not any(x[k] for k in ('image_view','image_layout','address','range','format')),'Only selected sampler value')
        elif kind=='image_view':
            require(x['raw']==x['image_view'] and not any(x[k] for k in ('address','range','format')),'Only selected image values')
            if q['descriptor_type']!=1:exact(x['sampler'],0,'Ignored sampler union value')
        else:
            require(not any(x[k] for k in ('raw','image_view','sampler','image_layout')),'Only selected address values')
            if kind=='acceleration_address':require(not x['range'] and not x['format'],'Acceleration scalar only')
            if q['descriptor_type'] in (6,7):exact(x['format'],0,'Ignored uniform/storage address format')
    else:
        reason=x['reason'];require(reason in ('not_captured','null_info','unknown_info_type','unknown_chain','null_typed_input','unknown_address_chain','unsupported_type'),'Finite unknown input reason')
        require(not any(x[k] for k in ('raw','image_view','sampler','image_layout','address','range','format')),'No unknown typed values')
        if reason=='null_info':require(not q['info_present'] and not q['info_sType'] and not q['descriptor_type'] and not q['chain_present'] and x['kind']=='unknown','Null original info')
        elif reason=='unknown_info_type':require(q['info_present'] and q['info_sType']!=1000316004 and x['kind']=='unknown','Unknown structure type')
        elif reason=='unknown_chain':require(q['info_present'] and q['info_sType']==1000316004 and q['chain_present'] and x['kind']=='unknown','Refused info chain')
        elif reason!='not_captured':
            require(q['info_present'] and q['info_sType']==1000316004 and not q['chain_present'],'Exact info header for unknown union')
            if reason=='unsupported_type':require(kind is None and x['kind']=='unknown','Unsupported descriptor type')
            elif reason=='unknown_address_chain':require(kind==x['kind']=='buffer_address','Selected address chain gap')
            else:require(kind==x['kind'] and kind in ('sampler','image_view','buffer_address'),'Missing selected input pointer')
    if q['output_captured']:
        require(q['original_returned'] and q['output_pointer'] and 0<q['data_size']<=256,'Bounded output after exact original return')
        require(type(q['output_hex']) is str and re.fullmatch('[0-9a-f]{%d}'%(q['data_size']*2),q['output_hex']) is not None,'Full bounded returned payload')
    else:exact(q['output_hex'],'','No prefix for unavailable output')
    return dict(case=b['case_ordinal'],phase=b['api_phase'],same=same,returned=q['original_returned'])

COMMON='schema_version event '+' '.join(IDENTITY)+' '+' '.join(FALSE_FACETS)
LIFE='kind operation_ordinal return_ordinal original_returned return_kind result instance_generation device_generation parent_instance_generation raw_instance raw_device raw_physical output_pointer chain_present application count_only input_capacity returned_count physical_devices'
QUERY='operation_ordinal return_ordinal device_generation parent_instance_generation device_lifetime_known ordinal device descriptor_type info_sType info_present chain_present data_size output_pointer original_returned return_kind typed_input phase_before phase_after phase_unchanged output_captured output_hex'
COUNTERS='calls_seen original_returns events_retained dropped_events skipped_bootstrap_queries unknown_phase_queries phase_unavailable phase_changed output_unavailable typed_input_unknown active_instances active_devices lifecycle_calls_seen lifecycle_retained lifecycle_dropped transition_count'


def audit_lifetimes(lives,queries,footer):
    """Reconstruct only exact recorded callback lifetimes, by transition order."""
    steps={};instances={};devices={};events=[];gap=False;last_gen={'instance':0,'device':0}
    def step(n,kind,row):
        require(n>0 and n<=footer['transition_count'] and n not in steps,'Unique bounded entry/return transition');steps[n]=kind;events.append((n,kind,row))
    last_entry=0
    for row in lives:
        obj(row,COMMON+' '+LIFE,'Lifecycle row');kind=row['kind']
        require(kind in ('create_instance','destroy_instance','create_device','destroy_device','enumerate_physical_devices'),'Lifecycle kind')
        for k in ('operation_ordinal','return_ordinal','instance_generation','device_generation','parent_instance_generation','raw_instance','raw_device','raw_physical','output_pointer'):uint(row[k],'Lifecycle '+k)
        for k in ('input_capacity','returned_count'):uint(row[k],'Lifecycle '+k,(1<<32)-1)
        for k in ('original_returned','chain_present','count_only'):boolean(row[k],'Lifecycle '+k)
        require(row['operation_ordinal']>last_entry,'Lifecycle entry order');last_entry=row['operation_ordinal'];step(last_entry,'entry',row)
        void=kind.startswith('destroy_');exact(row['return_kind'],'void' if void else 'VkResult','Original lifecycle signature')
        if row['original_returned']:
            require(row['return_ordinal']>last_entry,'Lifecycle return follows entry');step(row['return_ordinal'],'return',row)
            if void:exact(row['result'],None,'Void destroy has no result')
            else:require(type(row['result']) is int and -(1<<31)<=row['result']<(1<<31),'Original VkResult')
        else:exact(row['return_ordinal'],0,'Pending return ordinal');exact(row['result'],None,'Pending has no result')
        app=row['application'];obj(app,'captured api_version name name_truncated engine_name engine_name_truncated','Original application declaration');boolean(app['captured'],'Application captured');uint(app['api_version'],'Declared API',(1<<32)-1)
        for k in ('name','engine_name'):require(type(app[k]) is str and len(app[k].encode('utf-8'))<=64 and '\x00' not in app[k],'Bounded declared application label')
        for k in ('name','engine_name'):
            boolean(app[k+'_truncated'],'Application label truncation')
            if app[k+'_truncated']:exact(len(app[k].encode('utf-8')),64,'Full retained label prefix')
        if not app['captured']:exact(app,dict(captured=False,api_version=0,name='',name_truncated=False,engine_name='',engine_name_truncated=False),'Absent application declaration')
        if kind!='create_instance':exact(app,dict(captured=False,api_version=0,name='',name_truncated=False,engine_name='',engine_name_truncated=False),'Ignored application declaration')
        physicals=row['physical_devices'];require(type(physicals) is list and len(physicals)<=32,'Bounded returned physical devices')
        for raw in physicals:uint(raw,'Physical handle');require(raw>0,'Nonzero returned physical handle')
        require(len(set(physicals))==len(physicals),'Duplicate returned physical devices')
        if kind!='enumerate_physical_devices':
            exact(row['count_only'],False,'Unused count_only');exact(row['input_capacity'],0,'Unused capacity');exact(row['returned_count'],0,'Unused count');exact(physicals,[],'Unused physical values')
        elif not row['original_returned'] or row['result'] not in (0,5):exact(row['returned_count'],0,'No failed/pending physical result');exact(physicals,[],'No failed/pending physical values')
        elif row['count_only']:exact(row['input_capacity'],0,'Count-only original input');exact(row['output_pointer'],0,'Count-only no values pointer');exact(physicals,[],'Count-only no returned handles')
        else:
            require(row['output_pointer']>0,'Original values output pointer')
            exact(len(physicals),min(row['input_capacity'],row['returned_count'],32),'Exact retained returned values')
            if row['returned_count']>row['input_capacity'] or row['returned_count']>32:gap=True
        zeros={'create_instance':('device_generation','parent_instance_generation','raw_device','raw_physical'),'destroy_instance':('device_generation','parent_instance_generation','raw_device','raw_physical','output_pointer'),'create_device':('instance_generation','raw_instance'),'destroy_device':('instance_generation','raw_instance','raw_physical','output_pointer'),'enumerate_physical_devices':('device_generation','parent_instance_generation','raw_device','raw_physical')}[kind]
        for k in zeros:exact(row[k],0,'Unused lifecycle '+k)
        if kind.startswith('destroy_') or kind=='enumerate_physical_devices':exact(row['chain_present'],False,'Unused lifecycle chain declaration')
        if kind.startswith('create_'):
            entity='instance' if kind=='create_instance' else 'device';g=row[entity+'_generation'];require(g==last_gen[entity]+1,'Monotonic creation generation including failures');last_gen[entity]=g
            raw=row['raw_'+entity]
            if not row['original_returned'] or row['result']!=0:exact(raw,0,'Failed/pending creation cannot publish a handle')
            else:require(raw>0 and row['output_pointer']>0,'Successful original creation output')
        if kind=='create_device':require(row['raw_physical']>0,'Original physical device')
    for q in queries:
        step(q['operation_ordinal'],'query_entry',q)
        if q['original_returned']:step(q['return_ordinal'],'query_return',q)
    live_i={};live_d={};pending_i=set();pending_d=set();physical_owner={};query_bound={}
    for n,edge,row in sorted(events,key=lambda e:e[0]):
        kind=row.get('kind')
        if edge=='entry':
            if kind=='create_instance':continue
            if kind=='create_device':
                parent=row['parent_instance_generation']
                if not parent:gap=True;continue
                require(parent in live_i and parent not in pending_i,'CreateDevice original parent alive')
                require((parent,row['raw_physical']) in physical_owner and physical_owner[parent,row['raw_physical']]<n,'Exact prior returned enumeration ownership')
            elif kind in ('destroy_instance','enumerate_physical_devices'):
                g=row['instance_generation']
                if not g:gap=True;continue
                require(g in live_i and live_i[g]['raw_instance']==row['raw_instance'] and g not in pending_i,'Exact live original instance')
                if kind=='destroy_instance':
                    require(not any(d['parent_instance_generation']==g for d in live_d.values()),'Parent destroy after all original devices destroyed');pending_i.add(g)
            elif kind=='destroy_device':
                g=row['device_generation']
                if not g:gap=True;continue
                require(g in live_d and g not in pending_d and live_d[g]['raw_device']==row['raw_device'] and live_d[g]['parent_instance_generation']==row['parent_instance_generation'],'Exact live original device destroy')
                require(not any(gen==g for gen in query_bound.values()),'Destroy cannot overlap pending original query');pending_d.add(g)
        elif edge=='return':
            if kind=='create_instance' and row['result']==0:
                g=row['instance_generation'];require(all(i['raw_instance']!=row['raw_instance'] for i in live_i.values()),'Concurrent reuse of raw instance');live_i[g]=row;instances[g]=dict(row,create_return=n,destroy_entry=0,destroy_return=0)
            elif kind=='create_device' and row['result']==0:
                g=row['device_generation'];parent=row['parent_instance_generation']
                if not parent:gap=True
                else:require(parent in live_i and parent not in pending_i,'Parent remains alive through device creation')
                require(all(d['raw_device']!=row['raw_device'] for d in live_d.values()),'Concurrent reuse of raw device');live_d[g]=row;devices[g]=dict(row,create_return=n,destroy_entry=0,destroy_return=0)
            elif kind=='destroy_instance' and row['instance_generation']:
                g=row['instance_generation'];require(g in pending_i,'Exact destroy entry before return');instances[g].update(destroy_entry=row['operation_ordinal'],destroy_return=n);pending_i.remove(g);del live_i[g]
                physical_owner={k:v for k,v in physical_owner.items() if k[0]!=g}
            elif kind=='destroy_device' and row['device_generation']:
                g=row['device_generation'];require(g in pending_d,'Exact device destroy entry before return');devices[g].update(destroy_entry=row['operation_ordinal'],destroy_return=n);pending_d.remove(g);del live_d[g]
            elif kind=='enumerate_physical_devices' and row['instance_generation']:
                parent=row['instance_generation'];require(parent in live_i and parent not in pending_i,'Enumeration finishes in original parent lifetime')
                for physical in row['physical_devices']:
                    require(not any(p==physical and g!=parent for g,p in physical_owner),'Ambiguous physical parent across live instances');physical_owner[parent,physical]=n
        elif edge=='query_entry':
            if not row['device_lifetime_known']:gap=True;continue
            g=row['device_generation'];p=row['parent_instance_generation']
            require(g in live_d and g not in pending_d and live_d[g]['raw_device']==row['device'] and live_d[g]['parent_instance_generation']==p and p in live_i and p not in pending_i,'Query bound to exact live device/parent generation');query_bound[row['operation_ordinal']]=g
        else:
            if row['device_lifetime_known']:
                require(query_bound.pop(row['operation_ordinal'],None)==row['device_generation'],'Query return matches entry generation')
    if not footer['lifecycle_dropped']:
        if not any(footer[k] for k in ('faulted','transport_overflow','transport_chain_mismatch')):
            exact(footer['active_instances'],len(live_i),'Original active instance count');exact(footer['active_devices'],len(live_d),'Original active device count')
        else:require(footer['active_instances']<=len(live_i) and footer['active_devices']<=len(live_d),'Faulted route counts cannot invent live original objects')
        exact(footer['transition_count'],len(lives)+sum(r['original_returned'] for r in lives)+footer['calls_seen']+footer['original_returns'],'All callback transition accounting')
    if gap:require(footer['faulted'],'Untracked/missing lifetime requires explicit fault')
    return dict(instances=instances,devices=devices,lifetime_gap=gap,pending_lifecycle=sum(not x['original_returned'] for x in lives),pending_queries=len(query_bound))


def audit_journal(data,expected):
    require(type(data) is bytes and 0<len(data)<=MAX_BYTES and data.endswith(b'\n'),'Bounded newline-complete journal')
    lines=data.splitlines();require(2<=len(lines)<=290 and all(lines),'Header,256lifecycle,32queries,footer bound')
    rows=[loads(line) for line in lines];h,f=rows[0],rows[-1]
    obj(h,COMMON+' scope_claimed build_id_known build_id build_id_is_binary_attestation scope_slots global_event_limit global_events_per_case_limit lifecycle_limit scope enabled phase_source transport_scope route_capacity limits','Scope header')
    obj(f,COMMON+' '+COUNTERS+' faulted transport_overflow transport_chain_mismatch coverage_complete calls_by_case returns_by_case events_by_case dropped_by_case','Scope footer')
    identity(expected);identity(h)
    for row in rows:
        exact(row['schema_version'],2,'Scope schema2');false_facets(row)
        for k in IDENTITY:exact(row[k],expected[k],'Pinned scope '+k)
    exact(h['event'],'observer_header','Header kind');exact(f['event'],'observer_footer','Footer kind')
    for k in ('scope_claimed','build_id_known','enabled'):boolean(h[k],k)
    exact(h['scope_claimed'],True,'Persistent claim required');exact(h['build_id_is_binary_attestation'],False,'Build label is not loaded binary attestation')
    exact(h['scope'],'owned_translated_original_get_descriptor','Narrow scope');exact(h['phase_source'],'file','Phase source');exact(h['transport_scope'],'owned_single_loader_chain','Loader transport');exact(h['route_capacity'],32,'Route cap')
    for k,n in dict(scope_slots=8,global_event_limit=256,global_events_per_case_limit=32,lifecycle_limit=256).items():exact(h[k],n,'Scope bound '+k)
    exact(h['limits'],dict(events=32,events_per_case=4,output_bytes=256,phase_file_bytes=256),'Per-scope bounded payload')
    if h['build_id_known']:require(type(h['build_id']) is str and re.fullmatch('[0-9a-f]{16,64}',h['build_id']) is not None,'Declared build ID')
    else:exact(h['build_id'],'unknown','Unknown build declaration')
    exact(h['build_id'],expected['build_id'],'Pinned build declaration')
    for k in COUNTERS.split():uint(f[k],'Footer '+k)
    for k in ('faulted','transport_overflow','transport_chain_mismatch','coverage_complete'):boolean(f[k],k)
    for k in ('calls_by_case','returns_by_case','events_by_case','dropped_by_case'):
        require(type(f[k]) is list and len(f[k])==9,'Nine per-case counters')
        for n in f[k]:uint(n,k)
    lives=[];queries=[];query_started=False;last=last_operation=0;hist=[0]*9;changed=unavailable=unknown=output_missing=0
    for r in rows[1:-1]:
        if r['event']=='lifecycle':require(not query_started,'Lifecycle section before query section');lives.append(r)
        else:
            exact(r['event'],'get_descriptor','Recorded row kind');query_started=True;obj(r,COMMON+' '+QUERY,'Original query row');x=query_payload(r);queries.append(r)
            require(last<r['ordinal']<=f['calls_seen'],'Query entry counter order');last=r['ordinal'];hist[x['case']]+=1
            require(r['operation_ordinal']>last_operation,'Shared query entry transition order');last_operation=r['operation_ordinal']
            if x['returned']:
                unknown+=not r['typed_input']['known'];output_missing+=not r['output_captured']
                if not x['same']:
                    if r['phase_after']['status']=='known':changed+=1
                    else:unavailable+=1
    require(len(lives)<=256 and len(queries)<=32,'Scope storage limits')
    exact(f['lifecycle_retained'],len(lives),'Lifecycle retention');exact(f['lifecycle_calls_seen'],len(lives)+f['lifecycle_dropped'],'Lifecycle quota accounting')
    exact(f['events_retained'],len(queries),'Query retention');exact(f['events_by_case'],hist,'Per-case retained queries')
    exact(f['dropped_events'],sum(f['dropped_by_case']),'Dropped query total');exact(f['skipped_bootstrap_queries'],f['calls_by_case'][0],'Bootstrap count')
    exact(hist[0],0,'No bootstrap payload');exact(f['dropped_by_case'][0],0,'Bootstrap consumes no payload quota')
    require(sum(f['calls_by_case'])+f['unknown_phase_queries']<=f['calls_seen'],'Unattributed original calls')
    require(sum(f['returns_by_case'])<=f['original_returns']<=f['calls_seen'],'Original query forwarding totals')
    exact(f['output_unavailable'],output_missing,'Returned unavailable outputs');exact(f['typed_input_unknown'],unknown,'Returned unknown typed inputs')
    require(changed<=f['phase_changed'] and unavailable<=f['phase_unavailable'],'Returned phase uncertainty')
    require(f['phase_changed']<=f['original_returns'] and f['phase_unavailable']<=f['original_returns'],'Phase counters only describe original returns')
    for i in range(9):
        require(hist[i]<=4,'Four records per case per scope');require(f['returns_by_case'][i]<=f['calls_by_case'][i],'Original per-case returns')
        if i:require(hist[i]+f['dropped_by_case'][i]<=f['calls_by_case'][i],'Case quota accounting')
    state=audit_lifetimes(lives,queries,f)
    complete=(h['enabled'] and not any(f[k] for k in ('faulted','dropped_events','unknown_phase_queries','phase_unavailable','phase_changed','active_instances','active_devices','transport_overflow','transport_chain_mismatch','lifecycle_dropped')) and f['calls_seen']==f['original_returns'] and not state['pending_lifecycle'])
    exact(f['coverage_complete'],complete,'Declared finite scope closure')
    if complete:
        exact(f['calls_by_case'],f['returns_by_case'],'Every original call returned');exact(f['calls_seen'],sum(f['calls_by_case']),'No unattributed entries')
        for i in range(1,9):exact(hist[i],f['calls_by_case'][i],'No lost selected-case queries')
    finite=complete and h['build_id_known'] and not state['lifetime_gap']
    return dict(identity={k:h[k] for k in IDENTITY},build_id=h['build_id'],declared_complete=complete,finite_scope_complete=finite,enabled=h['enabled'],lifecycle_records=len(lives),query_records=len(queries),bootstrap_queries=f['skipped_bootstrap_queries'],unattributed_queries=f['unknown_phase_queries'],query_payload_unavailable=output_missing,**state,queries=queries,header=h,footer=f,**{k:False for k in FALSE_FACETS})


def parse_claim(data,run_id,slot):
    require(type(data) is bytes and len(data)<=256,'Bounded persistent claim')
    match=re.fullmatch(rb'D4VR_QUERY_SCOPE_2 ([0-9a-f]{32}) ([0-7]) ([1-9][0-9]{0,9}) ([1-9][0-9]{0,19}) ([1-8])\n',data)
    require(match is not None,'Exact persistent claim framing')
    values=match.groups();r=dict(run_id=values[0].decode(),scope_slot=int(values[1]),host_pid=int(values[2]),process_start_cookie=int(values[3]),dso_epoch=int(values[4]))
    identity(r);exact(r['run_id'],run_id,'Claim nonce');exact(r['scope_slot'],slot,'Claim filename slot');return r


def owner_join(manifest,scopes):
    """Positive original-query witnesses, never first/last scope heuristics."""
    mappings=dict(application_to_layer_physical_mapping_known=False,application_to_layer_create_output_slot_mapping_known=False)
    if manifest is None:return dict(owner_joined=False,reason='owner_proof_absent',devices=[],**mappings)
    require(type(manifest) is dict,'Owned original query manifest')
    exact(manifest.get('purpose'),'owned_loader_query_owner','Owner proof purpose')
    exact(manifest.get('query_scope_loader_passed'),True,'Original owned query fixture completion')
    nonce(manifest['run_id']);uint(manifest['host_pid'],'Owner host PID',(1<<32)-1);uint(manifest['process_start_cookie'],'Owner process cookie')
    exact(manifest['process_identity_unchanged_before_after'],True,'Original process identity stable')
    require(type(manifest['devices']) is list and len(manifest['devices'])==2 and type(manifest['queries']) is list and len(manifest['queries'])==2,'Two simultaneous original owner devices')
    exact(manifest['counts'],dict(instances=1,devices=2,samplers=2,original_queries=2,shaders=0,GPU_commands=0,submissions=0,images=0,buffers=0),'Shader-free owned query counts')
    exact(manifest['game_rendered'],False,'No game owner identity');exact(manifest['validation']['errors'],0,'Owned fixture validation')
    for k in ('resource_generation_from_observer_claimed','descriptor_placement_claimed','sampled_GPU_read_claimed'):exact(manifest[k],False,'Unsupported owner claim')
    for k in ('instance','physical_device'):uint(manifest[k],'Owner '+k);require(manifest[k]>0,'Original owner handles')
    t=manifest['teardown'];exact(t['devices_destroyed'],2,'Original device teardown');exact(t['samplers_destroyed'],2,'Original sampler teardown');exact(t['instance_destroyed'],True,'Original instance teardown');exact(t['no_work_submitted'],True,'No work submitted')
    joined=[];unjoined=[];seen=set()
    for index,(d,w) in enumerate(zip(manifest['devices'],manifest['queries']),1):
        for k in ('device','sampler'):uint(d[k],'Original owner '+k);require(d[k]>0,'Nonzero original owner handle')
        exact(d['fixture_device_generation'],index,'Fixture generation is its own namespace');require(d['device'] not in seen and d['device']>0,'Two distinct simultaneous original devices');seen.add(d['device'])
        args=w['args'];exact(args['device'],d['device'],'Original query device');exact(args['sampler'],d['sampler'],'Original sampler')
        exact(args['instance'],manifest['instance'],'Original parent instance');exact(args['physical_device'],manifest['physical_device'],'Original physical owner')
        exact(args['descriptor_type'],0,'Original sampler query');exact(args['info_sType'],1000316004,'Original query structure');exact(args['info_pnext'],None,'Original query chain')
        exact(args['phase'],dict(case_ordinal=index,api_phase=1),'Original case phase');exact(w['original_returned'],True,'Original query returned');exact(w['return_kind'],'void','Void original query')
        uint(args['data_size'],'Owner descriptor size',256);uint(args['output_pointer'],'Owner output pointer');require(args['data_size']>0 and args['output_pointer']>0,'Bounded original output')
        require(type(w['output_hex']) is str and re.fullmatch('[0-9a-f]{%d}'%(args['data_size']*2),w['output_hex']) is not None,'Full original returned descriptor')
        matches=[]
        for scope in scopes:
            if scope.get('status')!='valid':continue
            sid=scope['identity']
            if sid['run_id']!=manifest['run_id'] or sid['host_pid']!=manifest['host_pid'] or sid['process_start_cookie']!=manifest['process_start_cookie']:continue
            for q in scope['queries']:
                if (q['device']==d['device'] and q['descriptor_type']==0 and q['data_size']==args['data_size'] and q['output_pointer']==args['output_pointer'] and q['original_returned'] and q['output_captured'] and q['output_hex']==w['output_hex'] and q['typed_input']['known'] and q['typed_input']['kind']=='sampler' and q['typed_input']['sampler']==d['sampler'] and q['phase_before']==dict(status='known',**args['phase']) and q['phase_unchanged']):matches.append((scope,q))
        if len(matches)!=1:unjoined.append(dict(fixture_device_generation=index,reason='query_witness_absent' if not matches else 'query_witness_ambiguous'));continue
        scope,q=matches[0];g=q['device_generation'];p=q['parent_instance_generation'];device=scope['devices'].get(g);instance=scope['instances'].get(p)
        # Application and layer VkPhysicalDevice handles and creation output
        # slots can belong to different loader representations. Only the exact
        # logical query witness crosses this boundary; layer parent ownership
        # is independently checked against its own returned enumeration.
        if not q['device_lifetime_known'] or not device or not instance or instance['raw_instance']!=manifest['instance'] or not device['destroy_return'] or not instance['destroy_return']:
            unjoined.append(dict(fixture_device_generation=index,reason='exact_original_lifetime_unjoined'));continue
        joined.append(dict(fixture_device_generation=index,scope_slot=scope['identity']['scope_slot'],host_pid=scope['identity']['host_pid'],process_start_cookie=scope['identity']['process_start_cookie'],dso_epoch=scope['identity']['dso_epoch'],observer_device_generation=g,observer_parent_instance_generation=p,query_operation_ordinal=q['operation_ordinal'],application_raw_physical=manifest['physical_device'],layer_raw_physical=device['raw_physical'],layer_create_output_pointer=device['output_pointer'],original_query_witness_matched=True,exact_teardown_matched=True,finite_scope_complete=scope['finite_scope_complete'],**mappings))
    return dict(owner_joined=len(joined)==2 and not unjoined,reason='unique_original_query_and_lifetime_join' if len(joined)==2 and not unjoined else 'one_or_more_owners_unjoined',devices=joined,unjoined=unjoined,fixture_generation_equal_observer_generation_assumed=False,host_PID_equal_Windows_PID_assumed=False,**mappings)


def audit_owner_events(manifest,data):
    require(type(data) is bytes and len(data)<=MAX_BYTES and data.endswith(b'\n'),'Bounded original owner journal')
    rows=[loads(line) for line in data.splitlines()];require(len(rows)==20,'Ten owned original calls with both edges')
    expected_apis=['vkCreateInstance','vkCreateDevice','vkCreateDevice','vkGetDescriptorEXT','vkGetDescriptorEXT','vkDestroySampler','vkDestroyDevice','vkDestroySampler','vkDestroyDevice','vkDestroyInstance']
    for index,api in enumerate(expected_apis):
        b,a=rows[index*2:index*2+2]
        for r in (b,a):obj(r,'schema_version run_id host_pid process_start_cookie ordinal api edge return_kind result original_returned returned_handle args','Original owner edge')
        for k in ('schema_version','run_id','host_pid','process_start_cookie','ordinal','api','return_kind','args'):exact(a[k],b[k],'Original owner unchanged '+k)
        exact(b['schema_version'],1,'Original owner schema');exact(b['ordinal'],index+1,'Original owner ordinal');exact(b['api'],api,'Original owner API order')
        for k in ('run_id','host_pid','process_start_cookie'):exact(b[k],manifest[k],'Original owner '+k)
        exact(b['edge'],'before','Original BEFORE');exact(a['edge'],'after','Original AFTER');exact(b['original_returned'],False,'BEFORE not returned');exact(a['original_returned'],True,'Exact original returned')
        exact(b['result'],None,'No invented BEFORE result');exact(b['returned_handle'],None,'No invented BEFORE output')
        if api in ('vkCreateInstance','vkCreateDevice'):
            exact(a['return_kind'],'VkResult','Original creation result kind');exact(a['result'],0,'Original creation success')
            wanted=manifest['instance'] if api=='vkCreateInstance' else manifest['devices'][index-1]['device'];exact(a['returned_handle'],wanted,'Exact original returned handle')
        else:exact(a['return_kind'],'void','Void original signature');exact(a['result'],None,'Void has no VkResult');exact(a['returned_handle'],None,'Void has no handle return')
        if api=='vkCreateInstance':exact(b['args'],dict(api_version=manifest['instance_api_version'],extensions=['VK_EXT_debug_utils'],layers=['VK_LAYER_KHRONOS_validation']),'Original instance arguments')
        elif api=='vkCreateDevice':
            args=b['args'];obj(args,'instance physical_device fixture_device_generation queue_family queue_count descriptorBuffer_enabled bufferDeviceAddress_enabled descriptorBufferCaptureReplay_enabled descriptorBufferImageLayoutIgnored_enabled descriptorBufferPushDescriptors_enabled','Original device arguments')
            exact(args['instance'],manifest['instance'],'Original instance');exact(args['physical_device'],manifest['physical_device'],'Original physical');exact(args['fixture_device_generation'],index,'Independent fixture generation');uint(args['queue_family'],'Original queue family',255);exact(args['queue_count'],1,'Original queue count')
            for k in ('descriptorBuffer_enabled','bufferDeviceAddress_enabled','descriptorBufferCaptureReplay_enabled','descriptorBufferImageLayoutIgnored_enabled','descriptorBufferPushDescriptors_enabled'):exact(args[k],k=='descriptorBuffer_enabled','Explicit original enabled feature')
        elif api=='vkGetDescriptorEXT':
            q=manifest['queries'][index-3];exact(q['original_ordinal'],index+1,'Original query join ordinal');exact(b['args'],q['args'],'Original query arguments frozen before/after')
        elif api in ('vkDestroySampler','vkDestroyDevice'):
            d=manifest['devices'][(index-5)//2]
            exact(b['args'],dict(device=d['device'],sampler=d['sampler']) if api=='vkDestroySampler' else dict(device=d['device'],fixture_device_generation=d['fixture_device_generation']),'Original exact destruction arguments')
        else:exact(b['args'],dict(instance=manifest['instance']),'Original instance destruction')
    return dict(original_calls=10,both_original_edges_verified=True,original_void_results_are_null=True)


def analyze(journals,claims,run_id,build_id,owner_manifest=None,overflow=False):
    """Pure bounded entrypoint. Values are caller-supplied snapshots, not hooks."""
    nonce(run_id);require(type(claims) is dict and type(journals) is dict and 1<=len(claims)<=8,'Bounded claim map')
    require(set(journals)<=set(claims),'Journal without persistent claim')
    require(sum(len(v) for v in journals.values())<=8*MAX_BYTES,'Global journal byte budget');boolean(overflow,'Overflow marker')
    require(type(build_id) is str and (build_id=='unknown' or re.fullmatch('[0-9a-f]{16,64}',build_id) is not None),'Expected declared build ID')
    scopes=[];identities=set();process_epochs={}
    for slot,data in sorted(claims.items()):
        uint(slot,'Claim slot',7);expected=parse_claim(data,run_id,slot);key=tuple(expected[k] for k in IDENTITY)
        require(key not in identities,'Scope identity collision');identities.add(key)
        pc=(expected['host_pid'],expected['process_start_cookie']);process_epochs.setdefault(pc,[]).append(expected['dso_epoch'])
        expected['build_id']=build_id
        if slot not in journals:scopes.append(dict(status='missing_journal',identity={k:expected[k] for k in IDENTITY},finite_scope_complete=False));continue
        try:result=audit_journal(journals[slot],expected)
        except ValueError as e:scopes.append(dict(status='invalid_or_partial_journal',identity={k:expected[k] for k in IDENTITY},finite_scope_complete=False,reason=str(e)));continue
        result['status']='valid';scopes.append(result)
    records=sum(s.get('query_records',0) for s in scopes);require(records<=256,'Global payload record cap')
    for case in range(1,9):require(sum(s['footer']['events_by_case'][case] for s in scopes if s['status']=='valid')<=32,'Global per-case payload cap')
    if owner_manifest is not None:exact(owner_manifest['run_id'],run_id,'Owner proof nonce matches observed run')
    joined=owner_join(owner_manifest,scopes);valid=all(s['status']=='valid' for s in scopes)
    all_complete=valid and not overflow and all(s['finite_scope_complete'] for s in scopes)
    return dict(schema_version=2,run_id=run_id,scope_audit_valid=valid,claimed_scopes=len(scopes),retained_query_records=records,scopes=scopes,all_saved_scopes_complete=all_complete,claim_pool_fully_occupied=len(claims)==8,overflow_marker_present=overflow,owner=joined,joined_owner_finite_coverage=all_complete and joined['owner_joined'],unclaimed_process_and_namespace_coverage_known=False,global_query_absence_proven=False,any_API_query_absence_proven=False,slot_order_is_runtime_chronology=False,build_id_is_binary_attestation=False,**{k:False for k in FALSE_FACETS})


def file_inventory(directory,allowed,cap=MAX_BYTES):
    directory=safe_path(directory);require(directory.is_dir(),'Existing evidence directory');result={}
    count=0
    for p in directory.iterdir():
        count+=1;require(count<=len(allowed),'Bounded evidence directory')
        require(p.name in allowed,'Unexpected evidence path');result[p.name]=digest(read_file(p,cap))
    return result


def artifact(value):
    obj(value,'path bytes sha256','Pinned module');sha(value['sha256'],'Module');uint(value['bytes'],'Module size',64*MAX_BYTES);require(value['bytes']>0,'Nonempty module')
    data=read_file(value['path'],64*MAX_BYTES);exact(len(data),value['bytes'],'Pinned module size');exact(digest(data),value['sha256'],'Pinned module bytes')


def audit_directory(directory,pins,owner_manifest=None):
    obj(pins,'schema_version run_id build_id observer_module source_root source_sha256 journal_directory files owner_directory owner_files','External scope pins')
    exact(pins['schema_version'],2,'Pins schema');nonce(pins['run_id']);exact(str(safe_path(directory)),str(safe_path(pins['journal_directory'])),'Pinned journal directory')
    directory=safe_path(directory)
    if os.name=='posix':require(stat.S_IMODE(directory.stat().st_mode)==0o700 and directory.stat().st_uid==os.geteuid(),'Parent-owned private0700 journal directory')
    artifact(pins['observer_module']);root=safe_path(pins['source_root']);require(type(pins['source_sha256']) is dict and 1<=len(pins['source_sha256'])<=32,'Bounded source pins')
    for name,value in pins['source_sha256'].items():
        require(type(name) is str and not Path(name).is_absolute() and '..' not in Path(name).parts,'Relative source pin');sha(value,'Source');p=safe_path(root/name);require(root in p.parents,'Source path escape');exact(digest(read_file(p)),value,'Source SHA')
    allowed={f'slot-{slot}.{suffix}' for slot in range(8) for suffix in ('claim','jsonl')}|{'overflow'}
    require(type(pins['files']) is dict and 1<=len(pins['files'])<=17,'Finite scope inventory pins')
    for name,value in pins['files'].items():require(name in allowed,'Finite pinned slot filename');sha(value,'Captured file')
    before=file_inventory(directory,allowed);exact(before,pins['files'],'Exact externally pinned scope inventory')
    directory=safe_path(directory);claims={};journals={};overflow=False
    for name in before:
        if name=='overflow':exact(read_file(directory/name,256),b'D4VR_QUERY_SCOPE_OVERFLOW_2\n','Persistent overflow marker');overflow=True;continue
        match=re.fullmatch(r'slot-([0-7])\.(claim|jsonl)',name);slot=int(match.group(1));data=read_file(directory/name,256 if match.group(2)=='claim' else MAX_BYTES)
        (claims if match.group(2)=='claim' else journals)[slot]=data
    owner_files={}
    if pins['owner_directory'] is None:exact(pins['owner_files'],{},'No owner file pins');require(owner_manifest is None,'No unpinned owner evidence')
    else:
        owner_root=safe_path(pins['owner_directory']);require(type(pins['owner_files']) is dict and 1<=len(pins['owner_files'])<=16,'Bounded original owner inventory')
        for name,value in pins['owner_files'].items():require(type(name) is str and re.fullmatch('[A-Za-z0-9_.-]{1,120}',name) is not None and name not in ('.','..'),'Owner child basename');sha(value,'Owner file')
        owner_files=file_inventory(owner_root,set(pins['owner_files']));exact(owner_files,pins['owner_files'],'Pinned original owner files')
        manifest=loads(read_file(owner_root/'manifest.json'))
        if owner_manifest is not None:exact(owner_manifest,manifest,'Explicit owner snapshot')
        owner_manifest=manifest
        original_file=manifest['original_events_file'];require(original_file in owner_files and original_file=='original-events.jsonl','Pinned original owner journal')
        audit_owner_events(manifest,read_file(owner_root/original_file))
        validation=manifest['validation'];exact(validation['file'],'validation.log','Original validation file');require(validation['file'] in owner_files,'Pinned original validation');exact(validation['debug_messenger'],True,'Original validation callback');exact(validation['includes_teardown'],True,'Original validation includes teardown')
        log=read_file(owner_root/validation['file']).decode('utf-8',errors='replace');require('VUID-' not in log and re.search(r'validation\s+error',log,re.I) is None,'Original owner validation errors')
        for q in manifest['queries']:
            require(q['output_file'] in owner_files,'Pinned original returned footprint');data=read_file(owner_root/q['output_file'],256);exact(digest(data),q['output_sha256'],'Original returned bytes hash');exact(data.hex(),q['output_hex'],'Original returned bytes')
    result=analyze(journals,claims,pins['run_id'],pins['build_id'],owner_manifest,overflow)
    artifact(pins['observer_module']);exact(file_inventory(directory,allowed),before,'Scope files unchanged during audit')
    for name,value in pins['source_sha256'].items():exact(digest(read_file(root/name)),value,'Source unchanged during audit')
    if pins['owner_directory'] is not None:exact(file_inventory(pins['owner_directory'],set(pins['owner_files'])),owner_files,'Original owner files unchanged')
    result.update(scope_file_sha256=before,owner_file_sha256=owner_files,observer_module_sha256=pins['observer_module']['sha256'],external_source_sha256=pins['source_sha256'],post_capture_file_pins_are_pre_execution_inputs=False)
    return result


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--directory',required=True);p.add_argument('--pins',required=True);p.add_argument('--pins-sha256',required=True);p.add_argument('--output',required=True);a=p.parse_args(argv)
    sha(a.pins_sha256,'External pins file');data=read_file(a.pins);exact(digest(data),a.pins_sha256,'External pins authority');pins=loads(data)
    out=safe_path(a.output);protected=[safe_path(a.directory),safe_path(pins['source_root'])]
    if pins['owner_directory'] is not None:protected.append(safe_path(pins['owner_directory']))
    require(not out.exists() and out.parent.is_dir() and all(out!=r and r not in out.parents for r in protected),'Fresh audit output outside protected evidence/source')
    result=audit_directory(a.directory,pins);result['external_pins_sha256']=a.pins_sha256
    with out.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2,sort_keys=True);stream.write('\n')
    print(json.dumps(dict(scope_audit_valid=result['scope_audit_valid'],claimed_scopes=result['claimed_scopes'],owner_joined=result['owner']['owner_joined'],global_query_absence_proven=False)))
    return 0 if result['scope_audit_valid'] else 1


if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,TypeError,KeyError,OSError) as error:raise SystemExit('REFUSED: '+str(error)) from error
