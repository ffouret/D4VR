#!/usr/bin/env python3
"""Read-only owned translated D3D12 descriptor-copy auditor.

No GPU, Wine, subprocess, descriptor-memory read or game adapter. The pixel
reference is independent Fraction arithmetic fixed before physical execution.
D3D12 groundtruth and finite Vulkan query observations remain distinct inputs.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import re
import struct

WIDTH,HEIGHT=256,144
RAW_BYTES=WIDTH*HEIGHT*4
CASES={
    'direct_A_clamp':(0,0,'direct'),
    'direct_B_clamp':(1,0,'direct'),
    'simple_A_clamp':(0,0,'simple'),
    'simple_B_clamp':(1,0,'simple'),
    'ranges_A_clamp':(0,0,'ranges'),
    'ranges_B_clamp':(1,0,'ranges'),
    'after_bind_B_clamp':(1,0,'after_bind'),
    'direct_B_repeat':(1,1,'direct'),
}
FALSE_FACETS=('resource_generation_known','selected_slot_placement_known',
              'current_contents_at_draw_known','sampled_resource_GPU_read_proven',
              'exclusive_phase_attribution_proven')
ARTIFACT_ROLES=('fixture','vertex_shader','pixel_shader','observer','d3d12','d3d12core','dxgi')
MODULE_ROLES=('fixture','d3d12','d3d12core','dxgi')
MAX_U64=(1<<64)-1


def require(value,message):
    if not value:raise ValueError(message)


def exact(a,b,label):
    require(type(a) is type(b),'Type mismatch: '+label)
    if type(b) is dict:
        require(set(a)==set(b),'Object keys: '+label)
        for key in b:exact(a[key],b[key],label+'.'+key)
    elif type(b) is list:
        require(len(a)==len(b),'Array length: '+label)
        for n,(x,y) in enumerate(zip(a,b)):exact(x,y,label+'.'+str(n))
    else:require(a==b,'Value mismatch: '+label)


def obj(value,keys,label):
    require(type(value) is dict and set(value)==set(keys.split()),'Exact keys required: '+label)


def uint(value,label,limit=MAX_U64):
    require(type(value) is int and 0<=value<=limit,'Unsigned bounded integer: '+label)


def sha(value,label='SHA256'):
    require(type(value) is str and re.fullmatch('[0-9a-f]{64}',value) is not None,'Explicit lowercase SHA256: '+label)


def digest(data):return hashlib.sha256(data).hexdigest()


def nonce(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{32}',value) is not None,'Parent-pinned 128-bit run nonce')


def pairs(items):
    result={}
    for key,value in items:
        require(key not in result,'Duplicate JSON key');result[key]=value
    return result


def loads(data):
    try:return json.loads(data,object_pairs_hook=pairs,parse_constant=lambda _:require(False,'Nonfinite JSON'))
    except (json.JSONDecodeError,UnicodeDecodeError,RecursionError) as exc:raise ValueError('Malformed bounded JSON') from exc


def safe_path(value):
    require(type(value) in (str,Path) or isinstance(value,Path),'Filesystem path')
    p=Path(value)
    require('..' not in p.parts,'Parent traversal is not admitted')
    absolute=p.absolute()
    for part in [*absolute.parents,absolute]:require(not part.is_symlink(),'Symlink path is not admitted')
    return absolute.resolve()


def read_regular(path,cap=2*1024*1024):
    p=safe_path(path);require(p.is_file() and p.stat().st_size<=cap,'Bounded regular file required')
    with p.open('rb') as stream:data=stream.read(cap+1)
    require(len(data)<=cap,'File grew beyond input bound');return data


def file_sha(path,cap=64*1024*1024):return digest(read_regular(path,cap))


def child(folder,name):
    require(type(name) is str and re.fullmatch('[A-Za-z0-9_.-]{1,120}',name) is not None and name not in ('.','..'),'Finite child basename')
    return safe_path(Path(folder)/name)


def read_json(path,cap=2*1024*1024):return loads(read_regular(path,cap))


def artifact(value,label):
    obj(value,'path bytes sha256',label);sha(value['sha256'],label);uint(value['bytes'],label,64*1024*1024)
    require(value['bytes']>0,'Empty pinned artifact')
    data=read_regular(value['path'],64*1024*1024)
    require(len(data)==value['bytes'] and digest(data)==value['sha256'],'Pinned artifact changed: '+label)
    return str(safe_path(value['path']))


def verify_pins(pins):
    obj(pins,'schema_version source_root source_sha256 artifacts capture_root capture_sha256 runs', 'parent pins')
    exact(pins['schema_version'],1,'pins schema')
    require(type(pins['source_sha256']) is dict and 1<=len(pins['source_sha256'])<=32,'Bounded explicit source pins')
    root=safe_path(pins['source_root']);require(root.is_dir(),'Source root')
    for relative,expected in pins['source_sha256'].items():
        require(type(relative) is str and not Path(relative).is_absolute() and '..' not in Path(relative).parts,'Relative source pin')
        sha(expected,'source');path=safe_path(root/relative)
        require(root in path.parents and file_sha(path)==expected,'Pinned source changed')
    obj(pins['artifacts'],' '.join(ARTIFACT_ROLES),'artifacts')
    canonical={role:artifact(pins['artifacts'][role],role) for role in ARTIFACT_ROLES}
    require(len(set(canonical.values()))==len(canonical),'Artifact-role path substitution')
    obj(pins['runs'],'off on','runs');ids=[]
    for mode,run in pins['runs'].items():
        obj(run,'run_id reported_module_paths reported_shader_paths reported_phase_file',mode);nonce(run['run_id']);ids.append(run['run_id'])
        obj(run['reported_module_paths'],' '.join(MODULE_ROLES),'reported module paths')
        for p in run['reported_module_paths'].values():
            require(type(p) is str and 0<len(p)<=1024 and '\x00' not in p,'Explicit exact reported Windows module path')
        obj(run['reported_shader_paths'],'vertex_shader pixel_shader','reported shader paths')
        for p in [*run['reported_shader_paths'].values(),run['reported_phase_file']]:
            require(type(p) is str and 0<len(p)<=32768 and '\x00' not in p,'Exact reported argument path')
    require(len(set(ids))==2,'Off/on log nonce substitution')
    capture=safe_path(pins['capture_root']);require(capture.is_dir(),'Expected capture root')
    require(type(pins['capture_sha256']) is dict and 39<=len(pins['capture_sha256'])<=96,'Explicit finite post-capture file pins')
    for key,value in pins['capture_sha256'].items():bounded_relative(key);sha(value,'capture')
    exact(capture_files(capture),pins['capture_sha256'],'Entire post-capture evidence file ledger')
    return canonical


def texture_bytes(index):
    require(type(index) is int and index in (0,1),'Owned texture index')
    out=bytearray()
    for y in range(8):
        for x in range(8):
            out.extend((17+13*x,29+19*y,41+7*x+11*y,255) if index==0 else
                       (233-11*x,211-17*y,197-5*x-9*y,83+7*x+11*y))
    return bytes(out)


def texel(pixel,extent,shift,sampler):
    centre=Fraction(2*pixel+1,2*extent)
    position=(centre+shift)*8
    index=position.numerator//position.denominator
    return index%8 if sampler else min(7,max(0,index))


@lru_cache(maxsize=8)
def row_bank(image,sampler,width=WIDTH):
    source=texture_bytes(image);xs=[texel(x,width,Fraction(1,8),sampler) for x in range(width)]
    rows=[]
    for y in range(8):
        out=bytearray()
        for x in xs:
            r,g,b,a=source[(y*8+x)*4:(y*8+x)*4+4];out.extend((b,g,r,a))
        rows.append(bytes(out))
    return tuple(rows)


def reference_rows(name,width=WIDTH,height=HEIGHT):
    require(name in CASES,'Fixed owned case required')
    image,sampler,_=CASES[name];bank=row_bank(image,sampler,width)
    for y in range(height):yield bank[texel(y,height,Fraction(-1,8),sampler)]


def compare_pixels(raw,name,width=WIDTH,height=HEIGHT):
    require(type(raw) in (bytes,bytearray) and len(raw)==width*height*4,'Exact tight BGRA readout size')
    mismatches=[0,0,0,0];ref=hashlib.sha256();cache={}
    for y,want in enumerate(reference_rows(name,width,height)):
        ref.update(want);got=bytes(raw[y*width*4:(y+1)*width*4])
        if got==want:continue
        key=got,want
        if key not in cache:cache[key]=[sum(got[i]!=want[i] for i in range(c,len(want),4)) for c in range(4)]
        mismatches=[a+b for a,b in zip(mismatches,cache[key])]
    return dict(mismatched_bytes=sum(mismatches),mismatched_BA_bytes=mismatches[0]+mismatches[3],
                mismatches_BGRA=mismatches,reference_sha256=ref.hexdigest(),raw_sha256=digest(raw))


def audit_modules(rows,pins,mode):
    require(type(rows) is list and len(rows)==len(MODULE_ROLES),'Exact loaded module attestation table')
    seen=set()
    for row in rows:
        obj(row,'role reported_before reported_after host_realpath_before host_realpath_after bytes_before bytes_after sha256_before sha256_after mapping_verified','module attestation')
        role=row['role'];require(role in MODULE_ROLES and role not in seen,'Module role collision');seen.add(role)
        expected=pins['artifacts'][role]
        for phase in ('before','after'):
            exact(row['reported_'+phase],pins['runs'][mode]['reported_module_paths'][role],'Actual reported module path')
            exact(row['host_realpath_'+phase],str(safe_path(expected['path'])),'Explicit parent host mapping')
            exact(row['bytes_'+phase],expected['bytes'],'Actual module byte size')
            exact(row['sha256_'+phase],expected['sha256'],'Actual module SHA256')
        exact(row['mapping_verified'],True,'Parent explicit mapping attestation')
    return dict(actual_reported_paths_and_parent_file_hashes_matched=True,
                mapped_loaded_image_bytes_independently_attested=False,
                binary_source_equivalence_proven=False,old_game_session_modules_attested=False)


def _phase(value):
    obj(value,'status case_ordinal api_phase','phase')
    require(value['status'] in ('known','missing','invalid','io_error'),'Phase status')
    uint(value['case_ordinal'],'phase case',8);uint(value['api_phase'],'phase API',6)
    if value['status']!='known':
        exact(value['case_ordinal'],0,'unknown phase case');exact(value['api_phase'],0,'unknown phase API')


def _false_facets(row):
    for key in FALSE_FACETS:exact(row[key],False,'unsupported stronger claim '+key)


def audit_query_log(data,run_id,*,allow_core=False):
    """Audit the finite original-query recorder, never a universal absence claim.

    An incomplete/faulted stream is parsed honestly, but cannot support a local
    no-query statement. Equal file labels do not establish exclusive attribution.
    """
    if allow_core:require(type(run_id) is str and re.fullmatch('[0-9a-f]{16,64}',run_id) is not None,'CPU core run label')
    else:nonce(run_id)
    require(type(data) is bytes and len(data)<=2*1024*1024 and data.endswith(b'\n'),'Bounded complete JSONL framing')
    lines=data.splitlines();require(2<=len(lines)<=258 and all(lines),'Bounded JSONL row count')
    rows=[loads(line) for line in lines];header,footer=rows[0],rows[-1];queries=rows[1:-1]
    common='schema_version event run_id '+' '.join(FALSE_FACETS)
    obj(header,common+' scope enabled phase_source limits transport_scope route_capacity','observer header')
    obj(footer,common+' calls_seen original_returns events_retained dropped_events skipped_bootstrap_queries unknown_phase_queries phase_unavailable phase_changed output_unavailable typed_input_unknown faulted active_instances active_devices calls_by_case returns_by_case events_by_case dropped_by_case coverage_complete transport_overflow transport_chain_mismatch','observer footer')
    for row in rows:
        exact(row['schema_version'],1,'observer schema');exact(row['run_id'],run_id,'observer run nonce');_false_facets(row)
    exact(header['event'],'observer_header','header event');exact(footer['event'],'observer_footer','footer event')
    exact(header['scope'],'owned_translated_original_get_descriptor','observer scope')
    exact(header['phase_source'],'file','phase protocol')
    core=header['transport_scope']=='core_CPU_only'
    require(not core or allow_core,'Production requires actual bounded loader transport')
    exact(header['transport_scope'],'core_CPU_only' if core else 'owned_single_loader_chain','Bounded transport scope')
    exact(header['route_capacity'],0 if core else 32,'Bounded transport routes')
    require(type(header['enabled']) is bool,'Observer enabled boolean')
    exact(header['limits'],dict(events=256,events_per_case=32,output_bytes=256,phase_file_bytes=256),'observer bounds')
    for key in ('faulted','coverage_complete','transport_overflow','transport_chain_mismatch'):require(type(footer[key]) is bool,'Footer boolean')
    counters=('calls_seen','original_returns','events_retained','dropped_events','skipped_bootstrap_queries',
              'unknown_phase_queries','phase_unavailable','phase_changed','output_unavailable','typed_input_unknown','active_instances','active_devices')
    for key in counters:uint(footer[key],'footer '+key)
    for key in ('calls_by_case','returns_by_case','events_by_case','dropped_by_case'):
        require(type(footer[key]) is list and len(footer[key])==9,'Nine per-case counters')
        for count in footer[key]:uint(count,key)
    exact(footer['events_retained'],len(queries),'retained row count')
    exact(footer['dropped_events'],sum(footer['dropped_by_case']),'drop count')
    exact(footer['skipped_bootstrap_queries'],footer['calls_by_case'][0],'bootstrap counter')
    exact(footer['events_by_case'][0],0,'bootstrap payloads excluded')
    exact(footer['dropped_by_case'][0],0,'bootstrap cannot consume/drop payload budget')
    require(sum(footer['calls_by_case'])+footer['unknown_phase_queries']<=footer['calls_seen'],'Unattributed call totals')
    require(sum(footer['returns_by_case'])<=footer['original_returns']<=footer['calls_seen'],'Original forwarding totals')
    hist=[0]*9;stable=[[0]*7 for _ in range(9)];last=0;unavailable=typed_unknown=changed=phase_unavailable=0
    for query in queries:
        obj(query,common+' ordinal device descriptor_type info_sType info_present chain_present data_size output_pointer original_returned return_kind typed_input phase_before phase_after phase_unchanged output_captured output_hex','query row')
        exact(query['event'],'get_descriptor','query event');exact(query['return_kind'],'void','original void call')
        for key in ('ordinal','device','data_size','output_pointer'):uint(query[key],'query '+key)
        for key in ('descriptor_type','info_sType'):uint(query[key],key,(1<<32)-1)
        require(last<query['ordinal']<=footer['calls_seen'],'Original entry ordinal order');last=query['ordinal']
        for key in ('info_present','chain_present','original_returned','phase_unchanged','output_captured'):
            require(type(query[key]) is bool,'Query boolean')
        _phase(query['phase_before']);_phase(query['phase_after']);before,after=query['phase_before'],query['phase_after']
        require(before['status']=='known' and 1<=before['case_ordinal']<=8,'Retained query must have a finite selected case at entry')
        same=before['status']==after['status']=='known' and before['case_ordinal']==after['case_ordinal'] and before['api_phase']==after['api_phase']
        exact(query['phase_unchanged'],same,'Phase equality is observed only')
        case=before['case_ordinal'];hist[case]+=1
        if query['original_returned']:
            if same:stable[case][before['api_phase']]+=1
            elif after['status']=='known':changed+=1
            else:phase_unavailable+=1
        inp=query['typed_input'];obj(inp,'known kind reason raw image_view sampler image_layout address range format','typed input')
        require(type(inp['known']) is bool,'Typed input known boolean')
        for key in ('raw','image_view','sampler','address','range'):uint(inp[key],'input '+key)
        for key in ('image_layout','format'):uint(inp[key],'input '+key,(1<<32)-1)
        require(inp['kind'] in ('unknown','sampler','image_view','buffer_address','acceleration_address'),'Selected union kind')
        if inp['known']:
            exact(inp['reason'],'none','Known input reason')
            require(query['info_present'] and query['info_sType']==1000316004 and not query['chain_present'],'Known typed GetDescriptor info header')
            kind={0:'sampler',1:'image_view',2:'image_view',3:'image_view',10:'image_view',
                  4:'buffer_address',5:'buffer_address',6:'buffer_address',7:'buffer_address',1000150000:'acceleration_address'}.get(query['descriptor_type'])
            exact(inp['kind'],kind,'Selected descriptor union')
            if kind=='sampler':
                require(inp['raw']==inp['sampler'] and not any(inp[k] for k in ('image_view','image_layout','address','range','format')),'Sampler-only union')
            elif kind=='image_view':
                require(inp['raw']==inp['image_view'] and not any(inp[k] for k in ('address','range','format')),'Image-only union')
                if query['descriptor_type']!=1:exact(inp['sampler'],0,'Unused sampler union field')
            else:
                require(not any(inp[k] for k in ('raw','image_view','sampler','image_layout')),'Address-only union')
                if kind=='acceleration_address':require(not inp['range'] and not inp['format'],'Acceleration-only value')
        else:
            require(inp['reason'] in ('not_captured','null_info','unknown_info_type','unknown_chain','null_typed_input','unknown_address_chain','unsupported_type'),'Bounded unknown input reason')
            require(not any(inp[k] for k in ('raw','image_view','sampler','image_layout','address','range','format')),'Unknown union payload must be absent')
            reason=inp['reason'];kind={0:'sampler',1:'image_view',2:'image_view',3:'image_view',10:'image_view',4:'buffer_address',5:'buffer_address',6:'buffer_address',7:'buffer_address',1000150000:'acceleration_address'}.get(query['descriptor_type'])
            if reason=='null_info':
                require(not query['info_present'] and not query['info_sType'] and not query['descriptor_type'] and not query['chain_present'] and inp['kind']=='unknown','Null original info has no header/union')
            elif reason=='unknown_info_type':require(query['info_present'] and query['info_sType']!=1000316004 and inp['kind']=='unknown','Unknown original structure type')
            elif reason=='unknown_chain':require(query['info_present'] and query['info_sType']==1000316004 and query['chain_present'] and inp['kind']=='unknown','Refused original chain')
            elif reason!='not_captured':
                require(query['info_present'] and query['info_sType']==1000316004 and not query['chain_present'],'Unknown selected input still requires exact info header')
                if reason=='unsupported_type':require(kind is None and inp['kind']=='unknown','Unsupported descriptor type')
                elif reason=='unknown_address_chain':require(kind==inp['kind']=='buffer_address','Only selected address structure can have address-chain gap')
                else:require(kind==inp['kind'] and kind in ('sampler','image_view','buffer_address'),'Missing selected typed pointer')
        if query['output_captured']:
            require(query['original_returned'] and query['output_pointer'] and 0<query['data_size']<=256,'Capture after original return only')
            require(type(query['output_hex']) is str and re.fullmatch('[0-9a-f]{%d}'%(query['data_size']*2),query['output_hex']) is not None,'Full bounded returned payload')
        else:exact(query['output_hex'],'','Unavailable output has no prefix bytes')
        if query['original_returned']:
            unavailable+=not query['output_captured'];typed_unknown+=not inp['known']
    exact(footer['events_by_case'],hist,'Per-case retained rows')
    exact(footer['output_unavailable'],unavailable,'Unavailable output count')
    exact(footer['typed_input_unknown'],typed_unknown,'Typed-unknown count')
    require(changed<=footer['phase_changed'] and phase_unavailable<=footer['phase_unavailable'],'Phase uncertainty counters')
    for case in range(9):
        require(footer['returns_by_case'][case]<=footer['calls_by_case'][case],'Per-case returns')
        require(hist[case]<=32,'Per-case payload cap')
        if case:require(hist[case]+footer['dropped_by_case'][case]<=footer['calls_by_case'][case],'Per-case event/drop accounting')
    complete=(header['enabled'] and not footer['faulted'] and not footer['dropped_events'] and not footer['unknown_phase_queries']
              and not footer['phase_unavailable'] and not footer['phase_changed'] and footer['calls_seen']==footer['original_returns']
              and not footer['active_instances'] and not footer['active_devices'] and not footer['transport_overflow'] and not footer['transport_chain_mismatch'])
    exact(footer['coverage_complete'],complete,'Finite recorder closure')
    if complete:
        require(all(q['original_returned'] for q in queries),'Incomplete retained return')
        exact(footer['calls_by_case'],footer['returns_by_case'],'Every original returned')
        require(footer['calls_seen']==sum(footer['calls_by_case']),'Complete bounded call accounting')
        for case in range(1,9):exact(hist[case],footer['calls_by_case'][case],'No unlogged selected queries')
    return dict(coverage_complete=complete,enabled=header['enabled'],query_records=len(queries),transport_scope=header['transport_scope'],
                calls_seen=footer['calls_seen'],bootstrap_queries_counted_without_payload=footer['skipped_bootstrap_queries'],
                stable_observed_phase_counts=stable[1:],bounded_phase_absence_usable=complete,
                unknown_phase_queries=footer['unknown_phase_queries'],phase_changed=footer['phase_changed'],
                typed_input_unknown=typed_unknown,output_unavailable=unavailable,
                exclusive_phase_attribution_proven=False,current_contents_at_draw_known=False,
                selected_slot_placement_known=False,sampled_resource_GPU_read_proven=False,
                no_runtime_query_ever_proven=False,old_game_session_coverage_inferred=False)

METHODS={'direct':'direct_create','simple':'copy_simple','ranges':'copy_ranges','after_bind':'copy_after_bind'}
PHASES={str(i):v for i,v in enumerate(('control_and_binds','typed_create_srv','copy_simple','copy_ranges','copy_after_bind','execute_and_wait','readback'))}
MODULE_NAMES={'fixture':'fixture','d3d12':'d3d12.dll','d3d12core':'d3d12core.dll','dxgi':'dxgi.dll'}
ALL_MODULE_NAMES={'fixture','d3d12.dll','d3d12core.dll','dxgi.dll','winevulkan.dll','vulkan-1.dll'}


def bounded_relative(value):
    require(type(value) is str and 0<len(value)<=240 and not Path(value).is_absolute() and '\\' not in value,'Relative evidence path')
    require(all(re.fullmatch('[A-Za-z0-9_.-]{1,120}',p) and p not in ('.','..') for p in value.split('/')),'Evidence path traversal')
    return value


def capture_files(root):
    root=safe_path(root);require(root.is_dir(),'Capture directory')
    files={}
    for p in root.rglob('*'):
        require(not p.is_symlink(),'Capture symlink redirection')
        require(p.is_file() or p.is_dir(),'Capture special file')
        if p.is_file():
            require(len(files)<96,'Capture file count bound')
            relative=p.relative_to(root).as_posix();bounded_relative(relative)
            files[relative]=file_sha(p,8*1024*1024)
    return files


def audit_snapshots(rows,pins,mode):
    require(type(rows) is list and len(rows)==4,'Four original module observation points')
    table={}
    for point,row in zip(('before_device','before_cases','after_cases','after_fixture_release'),rows):
        obj(row,'point modules','module point');exact(row['point'],point,'module observation order')
        require(type(row['modules']) is list and len(row['modules'])==6,'Complete fixed module-name query list')
        modules={}
        for module in row['modules']:
            obj(module,'name loaded module path','module snapshot');name=module['name']
            require(name in ALL_MODULE_NAMES and name not in modules,'Module name collision')
            require(type(module['loaded']) is bool,'Module loaded boolean');uint(module['module'],'HMODULE')
            if module['loaded']:
                require(module['module']>0 and type(module['path']) is str and 0<len(module['path'])<=32768 and '\x00' not in module['path'],'Actual loaded module path')
            else:exact(module['module'],0,'Absent module handle');exact(module['path'],None,'Absent module path')
            modules[name]=module
        for role,name in MODULE_NAMES.items():
            if point=='before_device' and role=='d3d12core':continue
            if role=='dxgi' and point not in ('before_cases','after_cases'):continue
            require(modules[name]['loaded'],'Required actual process module')
            exact(modules[name]['path'],pins['runs'][mode]['reported_module_paths'][role],'Pinned reported module path')
        table[point]=modules
    for name in ALL_MODULE_NAMES:
        exact(table['before_cases'][name],table['after_cases'][name],'Module substitution during cases')
    return table


def audit_resources(resources,heaps):
    require(type(resources) is list and len(resources)==6,'Six owned resources')
    require(type(heaps) is list and len(heaps)==4,'Four original descriptor heaps')
    rr={};hh={};raws=set()
    for row in resources:
        obj(row,'role raw generation heap_type initial_state description','resource')
        role=row['role'];require(role in ('texture_A','texture_B','upload_A','upload_B','render_target','readback') and role not in rr,'Resource role collision')
        uint(row['raw'],'resource raw');require(row['raw'] and row['raw'] not in raws,'Resource handle collision');raws.add(row['raw'])
        exact(row['generation'],1,'Original resource lifetime')
        d=row['description'];obj(d,'dimension alignment width height depth_or_array_size mip_levels format sample_count sample_quality layout flags','resource description')
        for k,v in d.items():uint(v,'resource '+k)
        common=dict(alignment=0,depth_or_array_size=1,mip_levels=1,sample_count=1,sample_quality=0)
        for k,v in common.items():exact(d[k],v,'resource '+k)
        if role.startswith('texture_') or role=='render_target':
            target=role=='render_target';want=dict(dimension=3,width=WIDTH if target else 8,height=HEIGHT if target else 8,format=87 if target else 28,layout=0,flags=1 if target else 0)
            exact(row['heap_type'],1,'Default texture heap');exact(row['initial_state'],2048 if target else 1024,'Initial texture state')
        else:
            read=role=='readback';want=dict(dimension=1,width=RAW_BYTES if read else d['width'],height=1,format=0,layout=1,flags=0)
            # An 8x8 RGBA8 footprint needs seven 256-byte pitches plus 32 useful
            # bytes in its final row. pTotalBytes need not retain final padding.
            if not read:require(7*256+8*4<=d['width']<=16384,'Bounded owned upload footprint')
            exact(row['heap_type'],3 if read else 2,'Host heap type');exact(row['initial_state'],1024 if read else 2755,'Initial host resource state')
        for k,v in want.items():exact(d[k],v,'resource '+role+'.'+k)
        rr[role]=row
    cpu_spans=[];gpu_spans=[]
    for row in heaps:
        obj(row,'role raw generation type count flags cpu_start gpu_start increment','descriptor heap')
        role=row['role'];require(role in ('cpu_srv','visible_srv','visible_sampler','rtv') and role not in hh,'Heap role collision')
        uint(row['raw'],'heap raw');require(row['raw'] and row['raw'] not in raws,'Heap/resource handle collision');raws.add(row['raw'])
        exact(row['generation'],1,'Original heap lifetime')
        typ,count,visible={'cpu_srv':(0,2,False),'visible_srv':(0,1,True),'visible_sampler':(1,2,True),'rtv':(2,1,False)}[role]
        exact(row['type'],typ,'Heap type');exact(row['count'],count,'Heap count');exact(row['flags'],int(visible),'Heap visibility')
        uint(row['increment'],'Descriptor stride',1<<20);require(row['increment']>0,'Positive descriptor increment')
        for start,spans in ((row['cpu_start'],cpu_spans),(row['gpu_start'],gpu_spans)):
            if start is None:require(not visible and spans is gpu_spans,'CPU-only heap GPU handle');continue
            uint(start,'Descriptor start');require(start and start<=MAX_U64-count*row['increment'],'Descriptor handle overflow')
            end=start+count*row['increment'];require(all(end<=a or start>=b for a,b in spans),'Overlapping owned descriptor heaps');spans.append((start,end))
        require((row['gpu_start'] is not None)==visible,'Shader visibility / GPU handle')
        hh[role]=row
    exact(hh['cpu_srv']['increment'],hh['visible_srv']['increment'],'CBV/SRV/UAV increment')
    return rr,hh


def audit_events(value,run_id,resources,heaps):
    obj(value,'schema_version run_id owned_fixture before_after_original_D3D12_calls events','D3D journal')
    exact(value['schema_version'],1,'Journal schema');exact(value['run_id'],run_id,'Journal nonce')
    exact(value['owned_fixture'],True,'Owned calls');exact(value['before_after_original_D3D12_calls'],True,'Original call edge recording')
    rows=value['events'];require(type(rows) is list and 2<=len(rows)<=768 and len(rows)%2==0,'Bounded paired API journal')
    calls=[];last_case=0
    for i in range(0,len(rows),2):
        b,a=rows[i:i+2]
        for r in (b,a):obj(r,'ordinal run_id case_ordinal api_phase api edge returns returned hresult value args','Original API edge')
        for k in ('ordinal','run_id','case_ordinal','api_phase','api','returns','args'):exact(a[k],b[k],'Unchanged original call '+k)
        exact(b['ordinal'],i//2+1,'Global original call ordinal');exact(b['run_id'],run_id,'Call nonce')
        uint(b['case_ordinal'],'Case ordinal',8);uint(b['api_phase'],'API phase',6)
        require(b['case_ordinal']>=last_case,'Case generation/order');last_case=b['case_ordinal']
        exact(b['edge'],'before','BEFORE edge');exact(a['edge'],'after','AFTER edge')
        exact(b['returned'],False,'Before call not returned');exact(a['returned'],True,'Original returned')
        exact(b['hresult'],None,'No invented before HRESULT');exact(b['value'],None,'No invented before value');exact(a['value'],None,'No invented return value')
        require(b['returns'] in ('void','HRESULT'),'Original return kind')
        if b['returns']=='void':exact(a['hresult'],None,'Void D3D12 call has no HRESULT')
        else:require(type(a['hresult']) is int and 0<=a['hresult']<1<<31,'Original successful HRESULT')
        require(type(b['api']) is str and type(b['args']) is dict,'Typed original API arguments');calls.append(b)
    by_case=[[c for c in calls if c['case_ordinal']==i] for i in range(9)]
    rr,hh=resources,heaps;device=None;queue=None;command=None
    def expect(c,api,phase,args,kind='void'):
        exact(c['api'],api,'Original API order');exact(c['api_phase'],phase,'Original phase');exact(c['returns'],kind,'Original return signature');exact(c['args'],args,'Original typed arguments '+api)
    def create(c,image,dest,slot,role):
        nonlocal device
        a=c['args'];device=a.get('device') if device is None else device;uint(device,'Original device');require(device>0,'Original device')
        expect(c,'CreateShaderResourceView',1,dict(device=device,resource=rr['texture_'+('B' if image else 'A')]['raw'],resource_generation=1,heap=dest['raw'],heap_generation=1,slot=slot,cpu_handle=dest['cpu_start']+slot*dest['increment'],role=role,description=dict(format=28,dimension=4,component_mapping=5768,most_detailed_mip=0,mip_levels=1,plane_slice=0,min_lod_clamp=0)))
    def copy(c,image,ranges=False,late=False):
        args=dict(device=device,heap_type=0,source_heap=hh['cpu_srv']['raw'],source_generation=1,source_slot=image,destination_heap=hh['visible_srv']['raw'],destination_generation=1,destination_slot=0)
        dst=hh['visible_srv']['cpu_start'];src=hh['cpu_srv']['cpu_start']+image*hh['cpu_srv']['increment']
        if ranges:args.update(destination_starts=[dst],destination_sizes=[1],source_starts=[src],source_sizes=[1])
        else:args.update(count=1,destination_cpu_handle=dst,source_cpu_handle=src)
        expect(c,'CopyDescriptors' if ranges else 'CopyDescriptorsSimple',3 if ranges else 4 if late else 2,args)
    def submission(part,value):
        nonlocal queue,command
        require(len(part) in (2,3),'Execute/Signal/optional completion wait')
        ex=part[0]['args'];obj(ex,'queue command_lists','Submit arguments')
        uint(ex['queue'],'Original queue');require(ex['queue']>0,'Original queue');require(type(ex['command_lists']) is list and len(ex['command_lists'])==1,'One original command list')
        uint(ex['command_lists'][0],'Original command list');require(ex['command_lists'][0]>0,'Original command list')
        if queue is None:queue,command=ex['queue'],ex['command_lists'][0]
        expect(part[0],'ExecuteCommandLists',5,dict(queue=queue,command_lists=[command]))
        expect(part[1],'Signal',5,dict(fence_value=value),'HRESULT')
        if len(part)==3:expect(part[2],'SetEventOnCompletion',5,dict(fence_value=value),'HRESULT')
    bootstrap=by_case[0];seen={}
    for c in bootstrap:
        api=c['api'];seen.setdefault(api,[]).append(c)
    required={'D3D12CreateDevice':1,'CheckFeatureSupportShaderModel':1,'CheckFeatureSupportFormat':2,'CreateCommandQueue':1,'CreateCommandAllocator':1,'CreateCommandList':1,'CreateFence':1,'CreateCommittedResource':6,'CreateDescriptorHeap':4,'CreateShaderResourceView':2,'CreateSampler':2,'CreateRenderTargetView':1,'CreateRootSignature':1,'CreateGraphicsPipelineState':1,'CloseUploadCommandList':1,'ExecuteCommandLists':1,'Signal':1}
    require(set(seen) in (set(required),set(required)|{'SetEventOnCompletion'}),'Unknown/bootstrap missing API')
    for k,n in required.items():exact(len(seen[k]),n,'Bootstrap call count '+k)
    if 'SetEventOnCompletion' in seen:exact(len(seen['SetEventOnCompletion']),1,'Bootstrap completion count')
    order=['D3D12CreateDevice','CheckFeatureSupportShaderModel','CheckFeatureSupportFormat','CheckFeatureSupportFormat','CreateCommandQueue','CreateCommandAllocator','CreateCommandList','CreateFence']+['CreateCommittedResource']*6+['CreateDescriptorHeap']*4+['CreateShaderResourceView']*2+['CreateSampler']*2+['CreateRenderTargetView','CreateRootSignature','CreateGraphicsPipelineState','CloseUploadCommandList','ExecuteCommandLists','Signal']
    if 'SetEventOnCompletion' in seen:order.append('SetEventOnCompletion')
    exact([c['api'] for c in bootstrap],order,'Bootstrap creation/use order')
    for c in seen['CreateCommittedResource']:
        a=c['args'];role=a.get('role');require(role in rr,'Original resource creation role');device=a.get('device') if device is None else device
        r=rr[role];expect(c,'CreateCommittedResource',0,dict(device=device,role=role,heap_type=r['heap_type'],heap_flags=0,creation_node_mask=1,visible_node_mask=1,initial_state=r['initial_state'],description=r['description']),'HRESULT')
    exact(sorted(c['args']['role'] for c in seen['CreateCommittedResource']),sorted(rr),'All resource creations')
    for c in seen['CreateDescriptorHeap']:
        role=c['args'].get('role');require(role in hh,'Original heap creation role');r=hh[role]
        expect(c,'CreateDescriptorHeap',0,dict(device=device,role=role,type=r['type'],count=r['count'],flags=r['flags'],node_mask=0),'HRESULT')
    exact(sorted(c['args']['role'] for c in seen['CreateDescriptorHeap']),sorted(hh),'All heap creations')
    create(seen['CreateShaderResourceView'][0],0,hh['cpu_srv'],0,'cpu_source_A');create(seen['CreateShaderResourceView'][1],1,hh['cpu_srv'],1,'cpu_source_B')
    static={'D3D12CreateDevice':dict(adapter=None,minimum_feature_level=45056),'CheckFeatureSupportShaderModel':dict(requested=96),'CreateCommandQueue':dict(type=0,priority=0,flags=0,node_mask=0),'CreateCommandAllocator':dict(type=0),'CreateCommandList':dict(node_mask=0,type=0,initial_pipeline=None),'CreateFence':dict(initial_value=0,flags=0),'CreateRootSignature':dict(node_mask=0,version=1,parameters=3),'CreateGraphicsPipelineState':dict(format=87,sample_count=1,blend_enable=False,logic_op_enable=False,color_write_mask=15),'CloseUploadCommandList':{}}
    for k,a in static.items():expect(seen[k][0],k,0,a,'HRESULT')
    for c,f,required_bits in zip(seen['CheckFeatureSupportFormat'],(28,87),(0x220,0x4020)):expect(c,'CheckFeatureSupportFormat',0,dict(format=f,required_support1=required_bits),'HRESULT')
    for i,c in enumerate(seen['CreateSampler']):expect(c,'CreateSampler',0,dict(heap=hh['visible_sampler']['raw'],generation=1,slot=i,cpu_handle=hh['visible_sampler']['cpu_start']+i*hh['visible_sampler']['increment'],filter=0,address_mode=1 if i else 3,min_lod=0,max_lod=0))
    expect(seen['CreateRenderTargetView'][0],'CreateRenderTargetView',0,dict(resource=rr['render_target']['raw'],cpu_handle=hh['rtv']['cpu_start'],description=None))
    start=next(i for i,c in enumerate(bootstrap) if c['api']=='ExecuteCommandLists');submission(bootstrap[start:],1)
    for index,(name,(image,sampler,method)) in enumerate(CASES.items(),1):
        cs=by_case[index];require(11<=len(cs)<=13,'Bounded complete per-case command journal')
        first=cs[0]
        if method in ('direct','after_bind'):create(first,0 if method=='after_bind' else image,hh['visible_srv'],0,'selected_visible_slot')
        else:copy(first,image,method=='ranges')
        for c,api in zip(cs[1:3],('ResetAllocator','ResetCommandList')):expect(c,api,0,{},'HRESULT')
        expect(cs[3],'SetDescriptorHeaps',0,dict(heaps=[hh['visible_srv']['raw'],hh['visible_sampler']['raw']],generations=[1,1]))
        for i,slot,heap in ((0,0,hh['visible_srv']),(1,sampler,hh['visible_sampler'])):
            expect(cs[4+i],'SetGraphicsRootDescriptorTable',0,dict(parameter=i,heap=heap['raw'],generation=1,slot=slot,gpu_handle=heap['gpu_start']+slot*heap['increment']))
        expect(cs[6],'DrawInstanced',0,dict(vertex_count=3,instance_count=1,first_vertex=0,first_instance=0))
        expect(cs[7],'CloseCommandList',0,{},'HRESULT');end=8
        if method=='after_bind':copy(cs[8],1,late=True);end+=1
        submission(cs[end:-1],index+1)
        expect(cs[-1],'MapReadback',6,dict(resource=rr['readback']['raw'],range=[0,RAW_BYTES]),'HRESULT')
    return dict(original_calls=len(calls),original_edge_rows=len(rows),cases=8,typed_copy_cases=5,after_bind_copy_after_Close_before_Execute=True,original_API_groundtruth_supplied_by_owned_fixture=True,live_application_registry_completeness_attested=False)


def audit_fixture(folder,pins,mode):
    folder=safe_path(folder);m=read_json(child(folder,'manifest.json'))
    obj(m,'schema_version translated_descriptor_copy_passed run_id owned_fixture game_rendered width height target_DXGI_format texture_DXGI_format raw_bytes_per_case readback_row_pitch readback_total_bytes features UV raster shaders resources heaps module_snapshots events_file events_sha256 api_phase case_phase_file phase_is_CPU_label_not_exclusive_GPU_or_translator_causality cases D3D12_void_calls_have_no_HRESULT game_resource_or_GPU_identity_claimed','fixture manifest')
    for k,v in dict(schema_version=1,translated_descriptor_copy_passed=True,run_id=pins['runs'][mode]['run_id'],owned_fixture=True,game_rendered=False,width=WIDTH,height=HEIGHT,target_DXGI_format=87,texture_DXGI_format=28,raw_bytes_per_case=RAW_BYTES,readback_row_pitch=WIDTH*4,readback_total_bytes=RAW_BYTES,api_phase=PHASES,case_phase_file=pins['runs'][mode]['reported_phase_file'],phase_is_CPU_label_not_exclusive_GPU_or_translator_causality=True,D3D12_void_calls_have_no_HRESULT=True,game_resource_or_GPU_identity_claimed=False).items():exact(m[k],v,'manifest '+k)
    exact(m['UV'],dict(source='SV_Position.xy',divisor=[WIDTH,HEIGHT],offset=[.125,-.125],flip_y=False,LOD=0),'Independent pixel coordinates')
    exact(m['raster'],dict(viewport=[0,0,WIDTH,HEIGHT,0,1],scissor=[0,0,WIDTH,HEIGHT],blend_enable=False,logic_op_enable=False,color_write_mask=15,sample_count=1),'Owned raster contract')
    f=m['features'];obj(f,'shader_model_requested shader_model_supported format_support','Required original feature queries')
    exact(f['shader_model_requested'],96,'SM6 request');uint(f['shader_model_supported'],'Supported SM',0xff);require(f['shader_model_supported']>=96,'SM6 prerequisite')
    require(type(f['format_support']) is list and len(f['format_support'])==2,'Two format queries')
    for row,fmt,bits in zip(f['format_support'],(28,87),(0x220,0x4020)):
        obj(row,'DXGI_format support1 support2 required_support1','Format support')
        exact(row['DXGI_format'],fmt,'Queried DXGI format');exact(row['required_support1'],bits,'Required exact format bits')
        uint(row['support1'],'Format support1',(1<<32)-1);uint(row['support2'],'Format support2',(1<<32)-1)
        require(row['support1']&bits==bits,'Queried format capability unsupported')
    require(type(m['shaders']) is list and len(m['shaders'])==2,'Exact two pinned shader modules')
    for row,role,stage,entry in zip(m['shaders'],('vertex_shader','pixel_shader'),('vertex','pixel'),('VSMain','PSMain')):
        obj(row,'stage path entry sha256 bytes','Shader module');a=pins['artifacts'][role]
        exact(row,dict(stage=stage,path=pins['runs'][mode]['reported_shader_paths'][role],entry=entry,sha256=a['sha256'],bytes=a['bytes']),'Exact original owned shader bytes and arguments')
    rr,hh=audit_resources(m['resources'],m['heaps']);audit_snapshots(m['module_snapshots'],pins,mode)
    exact(m['events_file'],'d3d12-events.json','No log redirect');data=read_regular(child(folder,m['events_file']));sha(m['events_sha256'],'events');exact(digest(data),m['events_sha256'],'Journal integrity')
    journal=audit_events(loads(data),m['run_id'],rr,hh)
    require(type(m['cases']) is list and len(m['cases'])==8,'Eight finite cases');results={};raws={}
    for index,(row,(name,(image,sampler,method))) in enumerate(zip(m['cases'],CASES.items()),1):
        obj(row,'name case_ordinal texture_index sampler_index method raw_file bytes sha256 constants_file constants_sha256 producer_fence_value producer_completed','Case')
        for k,v in dict(name=name,case_ordinal=index,texture_index=image,sampler_index=sampler,method=METHODS[method],raw_file=name+'.bgra',bytes=RAW_BYTES,constants_file=name+'.constants.bin',producer_fence_value=index+1,producer_completed=True).items():exact(row[k],v,'Case '+k)
        constants=read_regular(child(folder,row['constants_file']),8);exact(constants,struct.pack('<ff',.125,-.125),'Exact original constants');sha(row['constants_sha256']);exact(digest(constants),row['constants_sha256'],'Constant hash')
        raw=read_regular(child(folder,row['raw_file']),RAW_BYTES);sha(row['sha256']);exact(digest(raw),row['sha256'],'Case raw hash');r=compare_pixels(raw,name)
        require(r['mismatched_bytes']==0,'Independent BGRA pixel oracle rejected '+name);results[name]=r;raws[name]=raw
    exact({p.name for p in folder.iterdir()}, {'manifest.json','d3d12-events.json'}|{name+s for name in CASES for s in ('.bgra','.constants.bin')},'No extra or missing fixture outputs')
    for image in ('A','B'):
        for method in ('simple','ranges'):exact(raws[method+'_'+image+'_clamp'],raws['direct_'+image+'_clamp'],'Original copied descriptor pixels')
    exact(raws['after_bind_B_clamp'],raws['direct_B_clamp'],'Late copy visible to owned submitted draw')
    different=sum(a!=b for a,b in zip(raws['direct_A_clamp'],raws['direct_B_clamp']))
    ba_different=sum(a!=b for i,(a,b) in enumerate(zip(raws['direct_A_clamp'],raws['direct_B_clamp'])) if i%4 in (0,3))
    require(different>0 and ba_different>0 and raws['direct_B_repeat']!=raws['direct_B_clamp'],'Wrong image and sampler discrimination')
    return dict(cases=results,readbacks=8,readback_bytes=8*RAW_BYTES,original_D3D12=journal,A_B_different_bytes=different,A_B_different_BA_bytes=ba_different,copy_methods_match_direct=True,late_copy_matches_B=True,repeat_differs_from_clamp=True),raws


def audit_validation(root,value):
    obj(value,'log_files activation_marker sentinel_count non_sentinel_errors','Parent validation report')
    exact(value['activation_marker'],'Validation layers seem to be loaded correctly.','Actual vk_debug activation sentinel marker')
    require(type(value['log_files']) is list and 1<=len(value['log_files'])<=4 and len(set(value['log_files']))==len(value['log_files']),'Finite distinct validation logs')
    uint(value['sentinel_count'],'Expected sentinel count',16);require(value['sentinel_count']>0,'Expected intentional validation activation sentinel')
    text='\n'.join(read_regular(safe_path(root/bounded_relative(p)),2*1024*1024).decode('utf-8',errors='replace') for p in value['log_files'])
    require(value['activation_marker'] in text,'Validation activation marker missing')
    sentinel='VUID-VkDebugUtilsMessengerCallbackDataEXT-pNext-pNext'
    ids=re.findall(r'VUID-[A-Za-z0-9_]+-[A-Za-z0-9_]+-[A-Za-z0-9_]+',text)
    require(text.count(value['activation_marker'])==value['sentinel_count'],'Exact validation activation self-test marker count')
    require(all(i==sentinel for i in ids),'Unexpected validation VUID')
    bad=[line for line in text.splitlines() if (re.search(r'validation(?:\s+|[-_])error',line,re.I) or ('vkd3d_debug_messenger_callback:' in line and re.search(r'err:',line,re.I))) and sentinel not in line]
    require(not bad,'Unexpected non-sentinel validation error line');exact(value['non_sentinel_errors'],0,'No other recorded validation errors')
    return dict(activation_self_test_markers=value['sentinel_count'],literal_intentional_sentinel_VUID_occurrences_in_log=ids.count(sentinel),non_sentinel_validation_errors=0,activation_marker_seen=True,all_runtime_diagnostic_events_absent=False)


def audit_capture(root,pins):
    root=safe_path(root);exact(str(root),str(safe_path(pins['capture_root'])),'Expected capture root')
    verify_pins(pins);before=capture_files(root)
    parent=read_json(child(root,'parent-run.json'));obj(parent,'schema_version owned_fixture game_rendered runs','Parent original run receipt')
    exact(parent['schema_version'],1,'Parent schema');exact(parent['owned_fixture'],True,'Owned scope');exact(parent['game_rendered'],False,'No game execution')
    obj(parent['runs'],'off on','Parent two original runs')
    results={};raws={};queries={}
    for mode in ('off','on'):
        r=parent['runs'][mode];obj(r,'run_id fixture_exit_code modules validation','Parent run')
        exact(r['run_id'],pins['runs'][mode]['run_id'],'Parent run nonce');exact(r['fixture_exit_code'],0,'Original successful process exit')
        modules=audit_modules(r['modules'],pins,mode);validation=audit_validation(root,r['validation'])
        results[mode],raws[mode]=audit_fixture(child(root,mode),pins,mode)
        results[mode].update(module_attestation=modules,validation=validation)
        queries[mode]=audit_query_log(read_regular(child(root,mode+'-query.jsonl')),r['run_id'])
        exact(queries[mode]['enabled'],mode=='on','Observer off/on mode')
        if mode=='off':exact(queries[mode]['calls_seen'],0,'Disabled observer has no retained runtime-query facts');exact(queries[mode]['query_records'],0,'Disabled observer payloads')
    for name in CASES:exact(raws['off'][name],raws['on'][name],'Observer off/on original pixels')
    phase_counts=[]
    for i,(name,(_,_,method)) in enumerate(CASES.items()):
        phase={'direct':1,'simple':2,'ranges':3,'after_bind':4}[method]
        count=queries['on']['stable_observed_phase_counts'][i][phase]
        phase_counts.append(dict(name=name,observed_phase=phase,stable_query_count=count,
                                 no_query_with_these_finite_labels_observed=queries['on']['coverage_complete'] and count==0,
                                 exclusive_API_or_thread_attribution_proven=False))
    verify_pins(pins);exact(capture_files(root),before,'All captured files unchanged during audit')
    return dict(schema_version=1,translated_descriptor_copy_audit_passed=True,owned_fixture=True,game_rendered=False,
                runs=results,observer=queries,readbacks=16,readback_bytes=16*RAW_BYTES,off_on_equal_pairs=8,off_on_compared_bytes_once=8*RAW_BYTES,
                bounded_query_window_complete=queries['on']['coverage_complete'],case_phase_observations=phase_counts,
                capture_file_count=len(before),capture_sha256=before,external_source_sha256=pins['source_sha256'],
                pixel_reference='independent Fraction pixel-centres; no Y flip; integer nearest clamp/repeat; exact BGRA8',
                parent_post_capture_pins_are_pre_execution_inputs=False,finite_phase_labels_exclusive_attribution=False,
                current_contents_at_draw_known=False,selected_slot_placement_known=False,sampled_resource_GPU_read_proven=False,
                no_runtime_query_ever_proven=False,binary_source_equivalence_proven=False,old_game_session_coverage_inferred=False)


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',required=True);parser.add_argument('--pins',required=True);parser.add_argument('--pins-sha256',required=True);parser.add_argument('--output',required=True)
    a=parser.parse_args(argv);sha(a.pins_sha256,'external pins');data=read_regular(a.pins);exact(digest(data),a.pins_sha256,'External pins file authority')
    pins=loads(data);out=safe_path(a.output);root=safe_path(a.capture);source=safe_path(pins['source_root'])
    require(not out.exists() and out.parent.is_dir() and out!=root and root not in out.parents and out!=source and source not in out.parents,'Fresh audit output outside capture/source')
    result=audit_capture(root,pins);result['external_pins_sha256']=a.pins_sha256
    with out.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2,sort_keys=True);stream.write('\n')
    print(json.dumps(dict(translated_descriptor_copy_audit_passed=True,readbacks=16,readback_bytes=16*RAW_BYTES)))
    return 0


if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,TypeError,KeyError,OSError) as error:raise SystemExit('REFUSED: '+str(error)) from error
