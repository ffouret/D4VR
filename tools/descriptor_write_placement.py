#!/usr/bin/env python3
"""Bounded CPU model of a typed GetDescriptor write at its return, only.

analyze(snapshot) consumes explicit caller-owned facts, not a live observer.
It never reads an address or descriptor memory. Positive results describe only
that moment's unique pointer/allocation/buffer/selected-slot relation. Ordinary
copies and later stores are invisible; current-content and GPU facets are false.

Snapshot v1 has exact keys: schema_version, domain, observer, query, epochs, complete,
properties, allocations, maps, buffers, layouts, slots. Each snapshot is ONE
returned query; fixtures keep hidden writes/pixels outside this schema. See the
independent test snapshot for a complete small example. Generations and complete
flags are supplied protocol facts; this model cannot attest their truth.
"""
from __future__ import annotations
from dataclasses import dataclass
import hashlib
import json
import threading
import weakref

MAX_U64 = (1 << 64)-1
MAX_BYTES = 16384
MAX_WRITES = 8
MAX_SLOTS = 3
MAX_TICKETS = 4
MAX_OUTPUT = 256
RESOURCE_USAGE = 0x00400000
SAMPLER_USAGE = 0x00200000
ADDRESS_USAGE = 0x00020000
LAYOUT_DESCRIPTOR_BUFFER = 0x10
SIZES = ('sampler', 'sampled_image', 'uniform_buffer', 'storage_buffer',
         'uniform_texel_buffer', 'storage_texel_buffer', 'robust_uniform_buffer',
         'robust_storage_buffer', 'robust_uniform_texel_buffer', 'robust_storage_texel_buffer')
BINDING_TYPES = ('sampler', 'sampled_image', 'uniform_buffer', 'storage_buffer',
                 'uniform_texel_buffer', 'storage_texel_buffer', 'mutable', 'unknown')
SCOPE = 'typed_write_to_selected_slot_at_query_return'
FALSE_FACETS = dict(current_contents_at_draw_known=False,
                    descriptor_unchanged_until_execution=False,
                    descriptor_dynamically_accessed=False,
                    sampled_resource_GPU_read_proven=False)


def _fail(message):
    raise ValueError(message)


def _object(value, keys, label):
    if type(value) is not dict or set(value) != set(keys.split()):
        _fail('Exact object keys required: '+label)


def _uint(value, label, nullable=False):
    if nullable and value is None:
        return
    if type(value) is not int or not 0 <= value <= MAX_U64:
        _fail('Unsigned 64-bit integer required: '+label)


def _bool(value, label, nullable=False):
    if nullable and value is None:
        return
    if type(value) is not bool:
        _fail('Boolean required: '+label)


def _ref(value, label):
    _object(value, 'raw generation', label)
    for v in value.values():
        _uint(v, label)


def _list(value, cap, label):
    if type(value) is not list or len(value) > cap:
        _fail('Bounded array required: '+label)


def _encode(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            _fail('Duplicate JSON key: '+key)
        result[key] = value
    return result


def _copy(value):
    # Reject oversize/deep inputs before serialization can allocate without bound.
    nodes = [0]
    def walk(v, depth=0):
        nodes[0] += 1
        if nodes[0] > 4096 or depth > 12:
            _fail('Snapshot node/depth budget')
        if v is None or type(v) is bool:
            return v
        if type(v) is int:
            if not -(1<<31)<=v<=MAX_U64:_fail('Integer outside bounded JSON range')
            return v
        if type(v) is str and len(v) <= 1024:
            return v
        if type(v) is list and len(v) <= 64:
            return [walk(x, depth+1) for x in v]
        if type(v) is dict and len(v) <= 32 and all(type(k) is str and len(k) <= 64 for k in v):
            return {k:walk(x, depth+1) for k,x in v.items()}
        _fail('Unsupported or unbounded JSON value')
    if type(value) is str:
        if len(value) > MAX_BYTES:
            _fail('Snapshot byte budget')
        try:
            value = json.loads(value, object_pairs_hook=_pairs,
                               parse_constant=lambda _: _fail('Nonfinite JSON constant'))
        except (json.JSONDecodeError, RecursionError) as exc:
            raise ValueError('Malformed bounded JSON') from exc
    result = walk(value)
    if len(_encode(result).encode('utf-8')) > MAX_BYTES:
        _fail('Snapshot byte budget')
    return result


def _validate(s):
    _object(s, 'schema_version domain observer query epochs complete properties allocations maps buffers layouts slots', 'snapshot')
    if type(s['schema_version']) is not int or s['schema_version'] != 1:
        _fail('Unsupported schema')
    if s['domain']!='owned_fixture':_fail('Only caller-owned fixture snapshots are admitted')
    _uint(s['observer'], 'observer')
    q = s['query']
    _object(q, 'ordinal returned type data_size output_pointer output_hex info_complete input', 'query')
    for key in ('ordinal', 'data_size', 'output_pointer'):
        _uint(q[key], 'query.'+key, True)
    for key in ('returned', 'info_complete'):
        _bool(q[key], 'query.'+key)
    if q['type'] not in BINDING_TYPES:
        _fail('Unknown descriptor type token')
    inp = q['input']
    _object(inp, 'kind raw generation live_before live_at_return', 'typed input')
    if inp['kind'] not in ('image_view', 'sampler', 'buffer', 'unknown'):
        _fail('Unknown typed input kind')
    _uint(inp['raw'], 'input.raw');_uint(inp['generation'], 'input.generation')
    _bool(inp['live_before'], 'input.live_before');_bool(inp['live_at_return'], 'input.live_at_return')
    if q['output_hex'] is not None:
        h = q['output_hex']
        if type(h) is not str or len(h)>MAX_OUTPUT*2 or len(h)%2 or any(c not in '0123456789abcdef' for c in h):
            _fail('Invalid bounded output hex')
        if q['data_size'] is None or len(h)//2 != q['data_size']:
            _fail('Output hex/data size mismatch')
    _object(s['epochs'], 'memory buffer layout selection', 'epochs')
    for pair in s['epochs'].values():
        _list(pair, 2, 'epoch pair')
        if len(pair)!=2:_fail('Before/return epoch pair required')
        for n in pair:_uint(n, 'epoch', True)
    _object(s['complete'], 'allocations maps buffers layouts properties', 'complete')
    for b in s['complete'].values():_bool(b, 'completeness')
    p=s['properties']
    _object(p, 'alignment max_resource_range max_sampler_range robust_buffer_access pnext_complete sizes', 'properties')
    for key in ('alignment','max_resource_range','max_sampler_range'):_uint(p[key], 'property.'+key, True)
    _bool(p['robust_buffer_access'], 'robust feature', True);_bool(p['pnext_complete'], 'property chain')
    _object(p['sizes'], ' '.join(SIZES), 'property sizes')
    for n in p['sizes'].values():_uint(n, 'descriptor size', True)
    for key in ('allocations','maps','buffers'):_list(s[key], 8, key)
    for a in s['allocations']:
        _object(a,'raw generation size live host_visible host_coherent external','allocation')
        for key in ('raw','generation','size'):_uint(a[key],key)
        for key in ('live','host_visible','host_coherent','external'):_bool(a[key],key)
    for m in s['maps']:
        _object(m,'allocation generation offset size pointer live','map')
        _ref(m['allocation'],'map allocation')
        for key in ('generation','offset','size','pointer'):_uint(m[key],key)
        _bool(m['live'],'map live')
    for b in s['buffers']:
        _object(b,'raw generation size usage flags live sparse external binding address','buffer')
        for key in ('raw','generation','size','usage','flags'):_uint(b[key],key)
        for key in ('live','sparse','external'):_bool(b[key],key)
        _object(b['binding'],'allocation offset result complete','buffer binding');_ref(b['binding']['allocation'],'bound allocation')
        _uint(b['binding']['offset'],'bind offset');_bool(b['binding']['complete'],'binding complete')
        if type(b['binding']['result']) is not int or not -(1<<31)<=b['binding']['result']<(1<<31):_fail('Invalid bind VkResult')
        _object(b['address'],'value known','device address');_uint(b['address']['value'],'address',True);_bool(b['address']['known'],'address known')
    _list(s['layouts'],3,'layouts')
    for l in s['layouts']:
        _object(l,'raw generation revision size flags complete pnext_complete bindings','layout')
        for key in ('raw','generation','revision','size','flags'):_uint(l[key],key)
        for key in ('complete','pnext_complete'):_bool(l[key],key)
        _list(l['bindings'],32,'bindings')
        for b in l['bindings']:
            _object(b,'binding type count offset offset_known stride mutable_types immutable_samplers flags','layout binding')
            for key in ('binding','count','offset','flags'):_uint(b[key],key)
            _uint(b['stride'],'stride',True)
            for key in ('offset_known','immutable_samplers'):_bool(b[key],key)
            if b['type'] not in BINDING_TYPES:_fail('Invalid binding type')
            _list(b['mutable_types'],8,'mutable types')
            if any(type(t) is not str or t not in BINDING_TYPES for t in b['mutable_types']):_fail('Invalid mutable type token')
    _list(s['slots'],MAX_SLOTS,'selected slots')
    for slot in s['slots']:
        _object(slot,'set layout layout_revision binding index type buffer buffer_index bound_address set_offset state_epoch bind_ordinal offset_ordinal known','selected slot')
        _ref(slot['layout'],'selected layout');_ref(slot['buffer'],'selected buffer')
        for key in ('set','layout_revision','binding','index','buffer_index','set_offset','state_epoch','bind_ordinal','offset_ordinal'):_uint(slot[key],key)
        _uint(slot['bound_address'],'bound address',True);_bool(slot['known'],'selected state known')
        if slot['type'] not in BINDING_TYPES:_fail('Invalid slot type')


def _add(a,b):
    if a is None or b is None or a>MAX_U64-b:
        raise ValueError('arithmetic_overflow')
    return a+b


def _mul(a,b):
    if a and b>MAX_U64//a:
        raise ValueError('arithmetic_overflow')
    return a*b


def _span(start,size):
    if not start or not size:
        raise ValueError('empty_or_unknown_span')
    return start,_add(start,size)


def _refkey(x):return (x['raw'],x['generation'])
def _live_ref(x):return bool(x['raw'] and x['generation'])
def _intersects(a,b):return a[0]<b[1] and b[0]<a[1]


def _size(p,t):
    if t not in SIZES or t.startswith('robust_'):
        raise ValueError('unsupported_descriptor_type')
    if t in ('uniform_buffer','storage_buffer','uniform_texel_buffer','storage_texel_buffer'):
        if p['robust_buffer_access'] is None:raise ValueError('unknown_robust_feature')
        if p['robust_buffer_access']:t='robust_'+t
    n=p['sizes'][t]
    if not n or n>MAX_OUTPUT:raise ValueError('unknown_descriptor_size')
    return n


def _binding_stride(p,b):
    if b['flags'] or b['immutable_samplers'] or not b['offset_known']:
        raise ValueError('unsupported_binding')
    if b['type']=='mutable':
        types=b['mutable_types']
        if not types or len(set(types))!=len(types) or any(t in ('mutable','unknown') for t in types):
            raise ValueError('incomplete_mutable_types')
        stride=max(_size(p,t) for t in types)
    else:
        if b['mutable_types']:raise ValueError('unexpected_mutable_types')
        stride=_size(p,b['type'])
    if b['stride']!=stride:raise ValueError('stride_mismatch')
    if not b['count']:raise ValueError('empty_binding')
    return stride


def _slot_key(slot):
    return (slot['set'],*_refkey(slot['layout']),slot['layout_revision'],slot['binding'],slot['index'],slot['type'])


def analyze(snapshot):
    """Return a fresh JSON value, or ValueError for malformed/unbounded schema.

    Missing/unsupported/inconsistent facts return unknown with bounded reasons.
    Complete registries and epoch observations are supplied by the owned caller;
    a positive relation does not attest this input or a real application's state.
    """
    s=_copy(snapshot);_validate(s);q=s['query'];p=s['properties'];reasons=[]
    def reason(r):
        if r not in reasons:reasons.append(r)
    typed=(q['returned'] and q['info_complete'] and bool(q['ordinal']) and
           q['type'] in ('sampled_image','sampler') and
           q['input']['kind']==('image_view' if q['type']=='sampled_image' else 'sampler') and
           _live_ref(q['input']) and q['input']['live_before'] and q['input']['live_at_return'])
    if not typed:reason('typed_input_unknown')
    if not s['observer']:reason('observer_unknown')
    for key,complete in s['complete'].items():
        if not complete:reason('incomplete_'+key)
    for key,pair in s['epochs'].items():
        if not pair[0] or pair[0]!=pair[1]:reason('changed_or_unknown_'+key+'_epoch')
    if not p['pnext_complete']:reason('unknown_property_chain')
    alignment=p['alignment']
    if not alignment or alignment&(alignment-1):reason('unknown_alignment')
    try:
        size=_size(p,q['type'])
        if q['data_size']!=size:reason('descriptor_size_mismatch')
    except ValueError as exc:reason(str(exc));size=None
    if q['data_size'] is None or not 0<q['data_size']<=MAX_OUTPUT:reason('output_size_budget')
    typed=bool(typed and size and q['data_size']==size and s['complete']['properties'] and p['pnext_complete'])
    # Duplicate lifetime identities are ambiguous even when all values agree.
    for key in ('allocations','buffers','layouts'):
        ids=[_refkey(x) for x in s[key]]
        if len(ids)!=len(set(ids)):reason('duplicate_'+key)
        # Layouts are active definitions in this owned snapshot, not a history.
        live_raw=[x['raw'] for x in s[key] if key=='layouts' or x['live']]
        if len(live_raw)!=len(set(live_raw)):reason('ambiguous_live_'+key)
    mapids=[(*_refkey(m['allocation']),m['generation']) for m in s['maps']]
    if len(mapids)!=len(set(mapids)):reason('duplicate_maps')
    live_mapped_allocations=[_refkey(m['allocation']) for m in s['maps'] if m['live']]
    if len(live_mapped_allocations)!=len(set(live_mapped_allocations)):reason('multiple_live_maps_for_allocation')
    # A failed attempt is not evidence that a previous binding never existed.
    # Without an explicit successful binding it cannot exclude an allocation alias.
    if any(b['live'] and (b['binding']['result']!=0 or not b['binding']['complete']) for b in s['buffers']):
        reason('live_buffer_binding_unknown')
    keys=[_slot_key(slot) for slot in s['slots']]
    if len(keys)!=len(set(keys)):reason('duplicate_selected_slots')
    mapping=None;allocation=None;allocation_byte=None;mapped=False
    try:
        output_span=_span(q['output_pointer'],q['data_size'])
        overlaps=[]
        for m in s['maps']:
            if m['live']:
                span=_span(m['pointer'],m['size'])
                if _intersects(span,output_span):overlaps.append((m,span))
        if len(overlaps)!=1:raise ValueError('unmapped_or_ambiguous_output')
        mapping,span=overlaps[0]
        if not (span[0]<=output_span[0] and output_span[1]<=span[1]):raise ValueError('partial_mapped_output')
        aa=[a for a in s['allocations'] if _refkey(a)==_refkey(mapping['allocation']) and a['live']]
        if len(aa)!=1 or not _live_ref(aa[0]) or not mapping['generation']:raise ValueError('unknown_allocation_or_map_generation')
        allocation=aa[0]
        if not allocation['host_visible'] or allocation['external']:raise ValueError('unsupported_allocation')
        if _add(mapping['offset'],mapping['size'])>allocation['size']:raise ValueError('mapping_outside_allocation')
        allocation_byte=_add(mapping['offset'],q['output_pointer']-mapping['pointer'])
        if _add(allocation_byte,q['data_size'])>allocation['size']:raise ValueError('output_outside_allocation')
        mapped=True
    except (ValueError,TypeError) as exc:reason(str(exc) if isinstance(exc,ValueError) else 'output_span_unknown')
    base_reasons=list(reasons);results=[]
    for slot in s['slots']:
        local=list(base_reasons);facts={}
        def require(condition,why):
            if not condition:raise ValueError(why)
        try:
            require(not base_reasons,'prerequisite_unknown')
            require(slot['known'] and slot['state_epoch']==s['epochs']['selection'][1], 'selection_unknown_or_stale')
            require(slot['set']<32 and slot['buffer_index']<32,'selection_index_budget')
            require(0<slot['bind_ordinal']<q['ordinal'] and 0<slot['offset_ordinal']<q['ordinal'],'selection_not_known_at_query_return')
            require(slot['type']==q['type'],'different_descriptor_type')
            layouts=[l for l in s['layouts'] if _refkey(l)==_refkey(slot['layout'])]
            require(len(layouts)==1 and _live_ref(slot['layout']),'layout_lifetime_unknown')
            l=layouts[0]
            require(l['revision']==slot['layout_revision'] and l['revision']>0,'layout_revision_mismatch')
            require(l['complete'] and l['pnext_complete'] and l['flags']==LAYOUT_DESCRIPTOR_BUFFER,'unsupported_layout')
            bindings=[b for b in l['bindings'] if b['binding']==slot['binding']]
            require(len({b['binding'] for b in l['bindings']})==len(l['bindings']),'duplicate_bindings')
            require(len(bindings)==1,'binding_unknown');b=bindings[0];stride=_binding_stride(p,b)
            require(b['type']==q['type'] or b['type']=='mutable' and q['type'] in b['mutable_types'],'typed_binding_mismatch')
            require(slot['index']<b['count'],'array_index_outside_binding')
            require(_add(b['offset'],_mul(b['count'],stride))<=l['size'],'binding_outside_layout')
            selected=[buf for buf in s['buffers'] if _refkey(buf)==_refkey(slot['buffer']) and buf['live']]
            require(len(selected)==1 and _live_ref(slot['buffer']),'buffer_lifetime_unknown');buf=selected[0];bind=buf['binding']
            require(bind['result']==0,'failed_bind')
            require(bind['complete'] and not buf['sparse'] and not buf['external'] and not buf['flags'],'unsupported_buffer_binding')
            require(_refkey(bind['allocation'])==_refkey(allocation),'different_allocation')
            require(_add(bind['offset'],buf['size'])<=allocation['size'],'buffer_outside_allocation')
            # Any overlapping live bound buffer is an alias, regardless of usage.
            aliases=[]
            for other in s['buffers']:
                ob=other['binding']
                if other['live'] and ob['result']==0 and _refkey(ob['allocation'])==_refkey(allocation):
                    require(ob['complete'],'incomplete_alias_binding')
                    span=(ob['offset'],_add(ob['offset'],other['size']))
                    if _intersects(span,(allocation_byte,_add(allocation_byte,size))):aliases.append(other)
            require(len(aliases)==1 and _refkey(aliases[0])==_refkey(buf),'overlapping_buffer_alias')
            need=SAMPLER_USAGE if q['type']=='sampler' else RESOURCE_USAGE
            require(buf['usage']&need and buf['usage']&ADDRESS_USAGE,'buffer_usage_mismatch')
            address=buf['address']['value']
            require(buf['address']['known'] and address and slot['bound_address'] is not None,'device_address_unknown')
            end=_add(address,buf['size'])
            require(address<=slot['bound_address']<end,'bound_address_outside_buffer')
            # Address aliases cannot resolve one selected binding uniquely either.
            for other in s['buffers']:
                if other is not buf and other['live'] and other['address']['known'] and other['address']['value']:
                    require(not _intersects((address,end),(other['address']['value'],_add(other['address']['value'],other['size']))),'overlapping_device_address')
            require(slot['bound_address']%alignment==0 and slot['set_offset']%alignment==0,'descriptor_offset_alignment')
            max_range=p['max_sampler_range'] if q['type']=='sampler' else p['max_resource_range']
            require(max_range and _add(slot['set_offset'],l['size'])<=max_range,'descriptor_range_limit')
            displacement=slot['bound_address']-address
            set_byte=_add(displacement,slot['set_offset'])
            require(_add(set_byte,l['size'])<=buf['size'],'set_outside_buffer')
            expected=_add(_add(set_byte,b['offset']),_mul(slot['index'],stride))
            actual=allocation_byte-bind['offset']
            require(actual>=0 and _add(actual,size)<=buf['size'],'output_outside_buffer')
            require(actual==expected,'different_slot_address')
            facts=dict(allocation={'raw':allocation['raw'],'generation':allocation['generation']},
                       map_generation=mapping['generation'],allocation_byte=allocation_byte,
                       buffer={'raw':buf['raw'],'generation':buf['generation']},bind_memory_offset=bind['offset'],
                       buffer_byte=actual,buffer_device_address=address,bound_descriptor_address=slot['bound_address'],
                       bound_address_displacement=displacement,set_offset=slot['set_offset'],
                       binding_offset=b['offset'],descriptor_stride=stride,descriptor_size=size,
                       expected_buffer_byte=expected,output_pointer=q['output_pointer'])
        except ValueError as exc:
            if str(exc) not in local:local.append(str(exc))
        results.append(dict(slot=list(_slot_key(slot)),typed_write_to_selected_slot_at_query_return=not local,
                            reasons=local,relation=facts))
    if sum(r[SCOPE] for r in results)>1:
        reason('ambiguous_selected_slots')
        for r in results:
            if r[SCOPE]:
                r[SCOPE]=False;r['reasons'].append('ambiguous_selected_slots')
    placed=any(r[SCOPE] for r in results)
    if not s['slots']:reason('no_selected_slots')
    return dict(schema_version=1,scope=SCOPE,owned_snapshot_only=True,input_facts_attested=False,
                observer=s['observer'],query_ordinal=q['ordinal'],query_id=[s['observer'],q['ordinal']],
                typed_input=dict(q['input']),descriptor_type=q['type'],data_size=q['data_size'],output_hex=q['output_hex'],
                snapshot_sha256=hashlib.sha256(_encode(s).encode()).hexdigest(),epochs=s['epochs'],
                typed_write_seen=typed,mapped_output_span_known_at_query_return=mapped and not base_reasons,
                typed_write_to_selected_slot_at_query_return=placed,status='placed' if placed else 'unknown',
                reasons=reasons,slots=results,**FALSE_FACETS)


class _Lease:
    def __init__(self,sequence,ticket_id):
        self.sequence=sequence;self.ticket_id=ticket_id;self.records=[];self.slots=set()
        self.sealed=False;self.unavailable=None;self.dropped=0;self.payload_bytes=0;self.json=None;self.retained_writes=0


def _receipt_value(lease,records=None,slots=None):
    return dict(schema_version=1,scope=SCOPE,owned_snapshot_only=True,input_facts_attested=False,
                ticket_id=lease.ticket_id,sequence=lease.sequence,
                complete=lease.unavailable is None,unavailable_reason=lease.unavailable,
                dropped_writes=lease.dropped,writes=lease.records if records is None else records,
                selected_slot_count=len(lease.slots if slots is None else slots),**FALSE_FACETS)


@dataclass(frozen=True)
class Receipt:
    """Immutable JSON value plus a shared quota pin; to_dict returns a copy."""
    _json: str
    _lease: object
    def to_dict(self):return json.loads(self._json)
    def report(self):return self._json
    def __copy__(self):return self
    def __deepcopy__(self,memo):return self


class Ticket:
    def __init__(self,registry,lease):self._registry=registry;self._lease=lease
    def record(self,snapshot):
        result=analyze(snapshot);lease=self._lease
        with self._registry._lock:
            if lease.sealed or lease.unavailable is not None:return False
            why=None
            new_slots=lease.slots|{tuple(x['slot']) for x in result['slots']}
            if len(lease.records)>=MAX_WRITES:why='write_limit'
            elif len(new_slots)>MAX_SLOTS:why='slot_limit'
            elif lease.records and (not result['query_ordinal'] or not lease.records[-1]['query_ordinal'] or result['query_ordinal']<=lease.records[-1]['query_ordinal']):why='query_order'
            elif lease.records and result['observer']!=lease.records[0]['observer']:why='observer_mismatch'
            records=lease.records+[result]
            if why is None:
                # Reserve bounded room for the worst refusal header and counter.
                candidate=_receipt_value(lease,records,new_slots);candidate['unavailable_reason']='serialization_limit';candidate['dropped_writes']=MAX_U64
                if len(_encode(candidate).encode())>MAX_BYTES:why='serialization_limit'
            if why:
                lease.unavailable=why;lease.dropped=min(MAX_U64,lease.dropped+1)
                lease.payload_bytes=len(_encode(_receipt_value(lease)).encode());return False
            lease.records=records;lease.slots=new_slots;lease.retained_writes=len(records)
            lease.payload_bytes=len(_encode(_receipt_value(lease)).encode());return True
    def receipt(self):
        lease=self._lease
        with self._registry._lock:
            if not lease.sealed:
                lease.json=_encode(_receipt_value(lease));lease.payload_bytes=len(lease.json.encode());lease.sealed=True
                lease.records=[];lease.slots=set()
            return Receipt(lease.json,lease)


class Registry:
    """Four live shared payloads, including every externally retained receipt.

    Receipting seals a ticket. Copies share the payload/quota pin; no mutable
    current-state lookup, old receipt replacement, silent eviction or bootstrapping.
    """
    def __init__(self):
        self._lock=threading.RLock();self._live=weakref.WeakValueDictionary();self._next=0;self._refused=0
    def reserve(self,sequence):
        _uint(sequence,'capture sequence')
        if not sequence:_fail('Nonzero sequence required')
        with self._lock:
            if len(self._live)>=MAX_TICKETS or any(v.sequence==sequence for v in self._live.values()) or self._next==MAX_U64:
                self._refused=min(MAX_U64,self._refused+1);return None
            self._next+=1;lease=_Lease(sequence,self._next);self._live[self._next]=lease
            lease.payload_bytes=len(_encode(_receipt_value(lease)).encode())
            return Ticket(self,lease)
    def stats(self):
        with self._lock:
            values=list(self._live.values())
            return dict(live_tickets=len(values),payload_bytes=sum(x.payload_bytes for x in values),
                        retained_writes=sum(x.retained_writes for x in values),refused_reservations=self._refused,
                        limits=dict(tickets=MAX_TICKETS,slots_per_ticket=MAX_SLOTS,writes_per_ticket=MAX_WRITES,bytes_per_ticket=MAX_BYTES))
