"""Finite original D3D12 typed-SRV history, without descriptor-memory access.

analyze(snapshot) is a pure batch calculation over the exact v1 schema. The
caller supplies owned occurrence/lifetime facts and BEFORE values, not opaque
bytes. Only two Texture2D resources, a CPU two-slot heap and a visible one-slot
heap are admitted. Returned original void calls establish API HISTORY only.
Pending or overlapping reads/writes cannot establish a new typed lineage.

Registry reserves at most four shared tickets. Each ticket seals ONE report,
with <=3 slot identities, <=8 mutations and <=16384 serialized bytes INCLUDING
its sequence/envelope. Copies/deepcopies of Ticket and Receipt share the same
object and pin. No held receipt is evicted. These are per-ticket wire budgets,
not a total Python-heap bound. reserve/record rejection does not call an API.
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import re
import threading
import weakref

MAX_U64 = (1 << 64) - 1
MAX_U32 = (1 << 32) - 1
MAX_BYTES = 16384
MAX_RECORDS = 8
MAX_SLOTS = 3
MAX_TICKETS = 4
PURPOSE = 'owned_typed_D3D12_descriptor_history'
DOMAIN = 'owned_two_texture_three_SRV_slots'
FALSE_FACTS = ('input_facts_attested', 'opaque_descriptor_decoded',
               'actual_source_contents_at_copy_proven', 'current_contents_at_draw_known',
               'unchanged_since_copy_proven', 'sampled_resource_GPU_read_proven',
               'generic_PE_to_Unix_resource_mapping', 'renderer_owner_known',
               'exclusive_phase_attribution_proven', 'global_API_absence_proven',
               'old_process_or_backing_gate_repaired')
RESOURCE = dict(dimension=3, width=8, height=8, depth_or_array_size=1,
                mip_levels=1, format=28, sample_count=1, flags=0)
SRV = dict(format=28, view_dimension=4, component_mapping=5768,
           most_detailed_mip=0, mip_levels=1, plane_slice=0, min_lod_clamp_bits=0)
_LIFE = 'raw generation create_entry create_return create_hresult release_entry release_return'


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _object(value, keys, label):
    _require(type(value) is dict and set(value) == set(keys.split()), 'Exact keys: ' + label)


def _uint(value, label, maximum=MAX_U64):
    _require(type(value) is int and 0 <= value <= maximum, 'Unsigned integer: ' + label)


def _positive(value, label):
    _uint(value, label)
    _require(value > 0, 'Positive integer: ' + label)


def _bool(value, label):
    _require(type(value) is bool, 'Boolean: ' + label)


def _list(value, maximum, label):
    _require(type(value) is list and len(value) <= maximum, 'Bounded array: ' + label)


def _wire(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True, allow_nan=False)


def _pairs(items):
    result = {}
    for key, value in items:
        _require(key not in result, 'Duplicate JSON key')
        result[key] = value
    return result


def _freeze(value):
    if type(value) in (bytes, str):
        _require(len(value) <= MAX_BYTES, 'Snapshot byte budget')
        try:
            if type(value) is bytes:
                value = value.decode('utf-8')
            value = json.loads(value, object_pairs_hook=_pairs,
                               parse_constant=lambda _: _require(False, 'Nonfinite JSON'))
        except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
            raise ValueError('Malformed bounded JSON') from error
    nodes = 0
    def copy(v, depth=0):
        nonlocal nodes
        nodes += 1
        _require(nodes <= 4096 and depth <= 14, 'Snapshot node/depth budget')
        if v is None or type(v) is bool:
            return v
        if type(v) is int:
            _require(-(1 << 31) <= v <= MAX_U64, 'Bounded JSON integer')
            return v
        if type(v) is str:
            _require(len(v) <= 256, 'Bounded JSON string')
            return v
        if type(v) is list:
            _require(len(v) <= 16, 'Bounded JSON list')
            return [copy(x, depth + 1) for x in v]
        if type(v) is dict:
            _require(len(v) <= 32 and all(type(k) is str and len(k) <= 64 for k in v),
                     'Bounded JSON object')
            return {k: copy(x, depth + 1) for k, x in v.items()}
        raise ValueError('Only bounded exact JSON types are accepted')
    result = copy(value)
    _require(len(_wire(result).encode('ascii')) <= MAX_BYTES, 'Snapshot byte budget')
    return result


def _ref(value, label):
    _object(value, 'raw generation', label)
    for key in value:
        _uint(value[key], label + '.' + key)


def _key(value):
    return value['raw'], value['generation']


def _slot(value):
    _object(value, 'heap index cpu_handle', 'slot')
    _ref(value['heap'], 'heap reference')
    _uint(value['index'], 'slot index', MAX_U32)
    _uint(value['cpu_handle'], 'CPU handle')


def _slotkey(value):
    return *_key(value['heap']), value['index']


def _mentioned_slots(events, selected):
    slots = {_slotkey(selected)}
    for event in events:
        args = event['before']
        if event['api'] == 'CreateShaderResourceView':
            values = [args['destination']]
        elif event['api'] == 'CopyDescriptorsSimple':
            values = [args['source'], args['destination']]
        else:
            values = args['source_starts'] + args['destination_starts']
        slots.update(_slotkey(value) for value in values)
    return slots


def _phase(value):
    _object(value, 'case_ordinal api_phase', 'phase')
    _uint(value['case_ordinal'], 'case ordinal', 8)
    _uint(value['api_phase'], 'API phase', 6)


def _sum(a, b):
    _require(a <= MAX_U64 - b, 'Handle arithmetic overflow')
    return a + b


def _product(a, b):
    _require(not a or b <= MAX_U64 // a, 'Handle arithmetic overflow')
    return a * b


def _life(value, at, transitions):
    for key in ('raw', 'generation', 'create_entry', 'create_return', 'release_entry', 'release_return'):
        _uint(value[key], key)
    _positive(value['generation'], 'Occurrence generation')
    _positive(value['create_entry'], 'Create ENTRY')
    ret, hr = value['create_return'], value['create_hresult']
    if ret:
        _require(type(hr) is int and -(1 << 31) <= hr < (1 << 31), 'Original create HRESULT')
        _require(ret > value['create_entry'], 'Create RETURN after ENTRY')
        if hr >= 0:
            _positive(value['raw'], 'Successful original output')
        else:
            _require(value['raw'] == 0, 'No undefined failed output published')
    else:
        _require(hr is None and value['raw'] == 0, 'No pending create result/output')
    if value['release_entry']:
        _require(ret and hr == 0 and value['release_entry'] > ret, 'Release after successful create')
        if value['release_return']:
            _require(value['release_return'] > value['release_entry'], 'Release RETURN after ENTRY')
    else:
        _require(value['release_return'] == 0, 'No Release RETURN without ENTRY')
    for key in ('create_entry', 'create_return', 'release_entry', 'release_return'):
        n = value[key]
        if n:
            _require(n <= at, 'Transition beyond snapshot')
            _require(n not in transitions, 'Duplicate common transition')
            transitions.add(n)


def _validate(s):
    _object(s, 'schema_version purpose run_id domain at_transition complete gap device resources heaps events selected_destination', 'snapshot')
    _require(type(s['schema_version']) is int and s['schema_version'] == 1, 'Schema version')
    _require(s['purpose'] == PURPOSE and s['domain'] == DOMAIN, 'Owned finite purpose/domain')
    _require(type(s['run_id']) is str and re.fullmatch('[0-9a-f]{32}', s['run_id']), 'Run nonce')
    _positive(s['at_transition'], 'Snapshot transition')
    _bool(s['complete'], 'Complete owned-domain history'); _bool(s['gap'], 'Gap')
    _object(s['device'], _LIFE, 'device')
    transitions = set()
    _life(s['device'], s['at_transition'], transitions)
    for kind in ('resources', 'heaps'):
        _list(s[kind], 2, kind)
        _require(len(s[kind]) == 2, 'Exactly two ' + kind)
        for value in s[kind]:
            if kind == 'resources':
                _object(value, _LIFE + ' device description', 'resource')
                _object(value['description'], ' '.join(RESOURCE), 'resource description')
                for key, n in value['description'].items():
                    _uint(n, key, MAX_U32 if key != 'width' else MAX_U64)
            else:
                _object(value, _LIFE + ' device role heap_type count flags node_mask cpu_start increment', 'heap')
                _require(value['role'] in ('cpu', 'visible'), 'Owned heap role')
                for key in ('heap_type', 'count', 'flags', 'node_mask', 'increment'):
                    _uint(value[key], key, MAX_U32)
                _uint(value['cpu_start'], 'CPU start')
            _ref(value['device'], 'Object device')
            _life(value, s['at_transition'], transitions)
    _slot(s['selected_destination'])
    _list(s['events'], MAX_RECORDS, 'Mutation records')
    call_ids = set()
    for event in s['events']:
        _object(event, 'call_id entry return api return_kind original_returned result phase_before phase_after before', 'original mutation')
        for key in ('call_id', 'entry'):
            _positive(event[key], key)
        _require(event['call_id'] not in call_ids, 'Duplicate call ID')
        call_ids.add(event['call_id'])
        _uint(event['return'], 'Original RETURN')
        _bool(event['original_returned'], 'Original returned')
        _require(event['return_kind'] == 'void' and event['result'] is None, 'Original void result, never HRESULT')
        _phase(event['phase_before'])
        if event['original_returned']:
            _require(event['return'] > event['entry'], 'Original RETURN after ENTRY')
            _phase(event['phase_after'])
        else:
            _require(event['return'] == 0 and event['phase_after'] is None, 'No pending AFTER facts')
        for n in (event['entry'], event['return']):
            if n:
                _require(n <= s['at_transition'] and n not in transitions, 'Unique bounded common transition')
                transitions.add(n)
        args = event['before']
        if event['api'] == 'CreateShaderResourceView':
            _object(args, 'device resource destination description', 'CreateSRV BEFORE')
            if args['resource'] is not None: _ref(args['resource'], 'SRV resource')
            if args['description'] is not None:
                _object(args['description'], ' '.join(SRV), 'Explicit SRV description')
                for key, n in args['description'].items(): _uint(n, key, MAX_U32)
            _slot(args['destination'])
        elif event['api'] == 'CopyDescriptorsSimple':
            _object(args, 'device heap_type count source destination', 'Simple BEFORE')
            _uint(args['heap_type'], 'Descriptor heap type', MAX_U32)
            _uint(args['count'], 'Copy count', MAX_U32)
            _slot(args['source']); _slot(args['destination'])
        elif event['api'] == 'CopyDescriptors':
            _object(args, 'device heap_type source_range_count source_starts source_sizes destination_range_count destination_starts destination_sizes', 'Ranges BEFORE')
            _uint(args['heap_type'], 'Descriptor heap type', MAX_U32)
            for side in ('source', 'destination'):
                _uint(args[side + '_range_count'], 'Range count', MAX_U32)
                _list(args[side + '_starts'], MAX_SLOTS, 'Original range starts')
                for slot in args[side + '_starts']: _slot(slot)
                if args[side + '_sizes'] is not None:
                    _list(args[side + '_sizes'], MAX_SLOTS, 'Original range sizes')
                    for n in args[side + '_sizes']: _uint(n, 'Range size', MAX_U32)
        else:
            raise ValueError('Only the three closed original mutation APIs are framed')
        _ref(args['device'], 'Original device argument')
    return transitions


def _success(value):
    return bool(value['raw'] and value['create_return'] and value['create_hresult'] == 0)


def _live(value, entry, returned=None):
    end = entry if returned is None else returned
    return (_success(value) and value['create_return'] < entry and
            (not value['release_entry'] or end < value['release_entry']))


def _overlap(a, b):
    return a[0] < b[1] and b[0] < a[1]


def _occurrence_span(value):
    # Reuse is admitted only AFTER the old original Release returned. Its
    # ENTRY does not prove that the old occurrence ceased to exist already.
    return value['create_entry'], value['release_return'] or MAX_U64 + 1


def analyze(snapshot):
    """Return fresh JSON history. Malformed framing raises ValueError.

    Well-framed incomplete/unsupported/conflicting facts return unknown with
    reasons. Version IDs are original call IDs, not content hashes. Source
    historical versions are frozen at ENTRY and never resolved again at AFTER.
    """
    s = _freeze(snapshot)
    _validate(s)
    reasons = []
    def reason(text):
        if text not in reasons: reasons.append(text)
    if not s['complete']: reason('incomplete_owned_domain_history')
    if s['gap']: reason('owned_domain_gap')
    if len(_mentioned_slots(s['events'], s['selected_destination'])) > MAX_SLOTS:
        reason('slot_identity_budget_exceeded')
    device = s['device']
    if not _success(device): reason('device_creation_not_known_successful')
    resources = {_key(x): x for x in s['resources']}
    heaps = {_key(x): x for x in s['heaps']}
    for values in (s['resources'], s['heaps']):
        generations = [x['generation'] for x in sorted(values, key=lambda x: x['create_entry'])]
        if any(a >= b for a, b in zip(generations, generations[1:])):
            reason('nonmonotone_occurrence_generations')
        if len({_key(x) for x in values}) != len(values) or len({x['generation'] for x in values}) != len(values):
            reason('duplicate_occurrence_generation')
        for a in values:
            if a['device'] != {k: device[k] for k in ('raw', 'generation')}:
                reason('object_device_occurrence_mismatch')
            if _success(a) and not _live(device, a['create_entry'], a['release_return'] or a['release_entry'] or a['create_return']):
                reason('object_outside_device_lifetime')
            for b in values:
                if a is not b and _success(a) and _success(b) and a['raw'] == b['raw'] and _overlap(_occurrence_span(a), _occurrence_span(b)):
                    reason('overlapping_raw_object_lifetimes')
    for resource in s['resources']:
        if resource['description'] != RESOURCE: reason('unsupported_resource_description')
        if not _success(resource): reason('resource_creation_not_known_successful')
    if sorted(h['role'] for h in s['heaps']) != ['cpu', 'visible']:
        reason('finite_heap_roles_missing')
    if len({h['increment'] for h in s['heaps']}) != 1:
        reason('same_device_heap_type_increment_mismatch')
    for heap in s['heaps']:
        if not _success(heap): reason('heap_creation_not_known_successful')
        expected = (2, 0) if heap['role'] == 'cpu' else (1, 1)
        if (heap['heap_type'] != 0 or (heap['count'], heap['flags']) != expected or
                heap['node_mask'] != 0 or not heap['cpu_start'] or not heap['increment']):
            reason('unsupported_heap_declaration')
        try:
            _sum(heap['cpu_start'], _product(heap['count'], heap['increment']))
        except ValueError: reason('heap_handle_span_overflow')
    for a in s['heaps']:
        for b in s['heaps']:
            if a is b or not _success(a) or not _success(b): continue
            try:
                aa = (a['cpu_start'], _sum(a['cpu_start'], _product(a['count'], a['increment'])))
                bb = (b['cpu_start'], _sum(b['cpu_start'], _product(b['count'], b['increment'])))
                if _overlap(aa, bb) and _overlap(_occurrence_span(a), _occurrence_span(b)):
                    reason('ambiguous_overlapping_heap_handle_spans')
            except ValueError: pass
    ids = [x['call_id'] for x in sorted(s['events'], key=lambda x: x['entry'])]
    if any(a >= b for a, b in zip(ids, ids[1:])):
        reason('nonmonotone_original_call_ids')
    base_reasons = list(reasons)

    def resolve(slot, entry, returned=None):
        heap = heaps.get(_key(slot['heap']))
        _require(heap is not None and _live(heap, entry, returned), 'slot_heap_lifetime_unknown')
        _require(slot['index'] < heap['count'] and heap['increment'] > 0, 'slot_index_or_increment_unknown')
        expected = _sum(heap['cpu_start'], _product(slot['index'], heap['increment']))
        _require(expected == slot['cpu_handle'], 'CPU_handle_not_exact_slot')
        return _slotkey(slot), heap

    states, active, records = {}, {}, {}
    actions = []
    for event in s['events']:
        actions.append((event['entry'], False, event))
        if event['original_returned']: actions.append((event['return'], True, event))
    for _, after, event in sorted(actions, key=lambda x: x[0]):
        cid = event['call_id']
        if not after:
            args = event['before']
            r = dict(call_id=cid, entry=event['entry'], **{'return': event['return']}, api=event['api'],
                     original_returned=event['original_returned'], before=args,
                     phase_before=event['phase_before'], phase_after=event['phase_after'],
                     source_version_at_entry=None, version_after=None, reasons=list(base_reasons))
            records[cid] = r
            src = dst = None
            try:
                _require(args['device'] == {k: device[k] for k in ('raw', 'generation')} and
                         _live(device, event['entry']), 'original_device_lifetime_unknown')
                _require(event['phase_before']['case_ordinal'] > 0 and
                         (not event['original_returned'] or event['phase_after'] == event['phase_before']), 'original_phase_unknown_or_changed')
                if event['api'] == 'CreateShaderResourceView':
                    dst, _ = resolve(args['destination'], event['entry'])
                    resource = resources.get(_key(args['resource'])) if args['resource'] is not None else None
                    _require(resource is not None and _live(resource, event['entry']), 'SRV_resource_lifetime_unknown')
                    _require(args['description'] == SRV, 'unsupported_explicit_SRV_description')
                    origin = dict(resource=dict(args['resource']), create_call_id=cid,
                                  create_entry=event['entry'], create_return=event['return'], description=dict(args['description']))
                else:
                    _require(args['heap_type'] == 0, 'unsupported_copy_heap_type')
                    if event['api'] == 'CopyDescriptorsSimple':
                        _require(args['count'] == 1, 'only_unit_copy_admitted')
                        ss, dd = args['source'], args['destination']
                    else:
                        for side in ('source', 'destination'):
                            _require(args[side + '_range_count'] == 1 and len(args[side + '_starts']) == 1 and
                                     args[side + '_sizes'] in (None, [1]), 'only_unit_ranges_admitted')
                        ss, dd = args['source_starts'][0], args['destination_starts'][0]
                    src, sh = resolve(ss, event['entry'])
                    dst, dh = resolve(dd, event['entry'])
                    _require(sh['role'] == 'cpu' and dh['role'] == 'visible' and src != dst, 'closed_copy_direction_not_known')
                    source = states.get(src)
                    r['source_version_at_entry'] = source
                    _require(source is not None, 'source_typed_history_unknown_at_ENTRY')
                    resource = resources.get(_key(source['origin']['resource']))
                    _require(resource is not None and _live(resource, event['entry']), 'historical_origin_resource_lifetime_unknown')
                    origin = source['origin']
                touched = {x for x in (src, dst) if x is not None}
                for other in active.values():
                    # read/read does not collide; every mutation writes its dst.
                    if (other.get('unknown', False) or dst in other['touched'] or other['dst'] in touched):
                        if 'overlapping_mutation_interval' not in r['reasons']: r['reasons'].append('overlapping_mutation_interval')
                        if 'overlapping_mutation_interval' not in other['record']['reasons']: other['record']['reasons'].append('overlapping_mutation_interval')
                active[cid] = dict(record=r, src=src, dst=dst, touched=touched, origin=origin)
            except ValueError as error:
                r['reasons'].append(str(error))
                # An unadmitted original mutation can have touched the domain.
                # Without a complete unit interpretation no future latest
                # version may be reconstructed from the preceding history.
                for key in list(states): states[key] = None
                for other in active.values():
                    if 'possibly_overlapping_unadmitted_mutation' not in other['record']['reasons']:
                        other['record']['reasons'].append('possibly_overlapping_unadmitted_mutation')
                reason('unadmitted_original_mutation')
                active[cid] = dict(record=r, src=src, dst=dst, touched=set(states), origin=None, unknown=True)
        else:
            current = active.pop(cid)
            r, dst = current['record'], current['dst']
            # ENTRY history is already frozen above. Only validate the original
            # occurrence interval here; never consult a newer source version.
            try:
                _require(_live(device, event['entry'], event['return']), 'device_changed_before_original_RETURN')
                for slotkey in current['touched']:
                    heap = heaps.get(slotkey[:2])
                    _require(heap is not None and _live(heap, event['entry'], event['return']), 'heap_changed_before_original_RETURN')
                if current['origin'] is not None:
                    resource = resources.get(_key(current['origin']['resource']))
                    _require(resource is not None and _live(resource, event['entry'], event['return']), 'resource_changed_before_original_RETURN')
            except ValueError as error:
                r['reasons'].append(str(error))
            if not r['reasons'] and current['origin'] is not None:
                slot = event['before']['destination'] if event['api'] != 'CopyDescriptors' else event['before']['destination_starts'][0]
                version = dict(call_id=cid, entry=event['entry'], **{'return': event['return']}, slot=slot,
                               origin=current['origin'], source_call_id=(r['source_version_at_entry']['call_id'] if r['source_version_at_entry'] else None))
                r['version_after'] = version
                states[dst] = version
            elif dst is not None:
                states[dst] = None
    for current in active.values():
        current['record']['reasons'].append('original_mutation_pending')
        if current['dst'] is not None: states[current['dst']] = None
        reason('pending_original_mutation')
    for record in records.values():
        for text in record['reasons']: reason(text)
    selected = None
    try:
        key, heap = resolve(s['selected_destination'], s['at_transition'])
        _require(heap['role'] == 'visible', 'selected_destination_not_visible')
        selected = states.get(key)
        _require(selected is not None, 'selected_typed_history_unknown')
    except ValueError as error: reason(str(error))
    known = not reasons and selected is not None
    report = dict(schema_version=1, purpose='finite_original_typed_descriptor_history_audit', run_id=s['run_id'],
                domain=s['domain'], at_transition=s['at_transition'], complete=s['complete'], gap=s['gap'],
                device=s['device'], resources=s['resources'], heaps=s['heaps'],
                selected_destination=s['selected_destination'], selected_history_known=known,
                original_typed_copy_call_returned=bool(known and selected['source_call_id'] is not None),
                selected_version=selected if known else None,
                mutations=[records[e['call_id']] for e in sorted(s['events'], key=lambda x: x['entry'])],
                reasons=reasons, scope='last_observed_typed_origin_not_actual_descriptor_contents',
                **{key: False for key in FALSE_FACTS})
    _require(len(_wire(report).encode('ascii')) <= MAX_BYTES, 'Complete history report byte budget')
    return report


@dataclass(frozen=True)
class Receipt:
    """Immutable shared sealed payload. report() returns a detached JSON value."""
    _lease: object

    @property
    def sequence(self): return self._lease.sequence

    def serialized(self): return self._lease.payload

    def report(self): return json.loads(self.serialized())

    @property
    def sha256(self): return hashlib.sha256(self.serialized().encode('ascii')).hexdigest()

    def __copy__(self): return self

    def __deepcopy__(self, memo):
        memo[id(self)] = self
        return self


class _Lease:
    def __init__(self, sequence):
        self.sequence = sequence
        self.payload = None


class Ticket:
    """One reservation/one successful seal. Copies share this same ticket."""
    def __init__(self, registry, lease):
        self._registry = registry
        self._lease = lease

    def record(self, snapshot): return self._registry._record(self._lease, snapshot)

    def receipt(self): return None if self._lease.payload is None else Receipt(self._lease)

    def __copy__(self): return self

    def __deepcopy__(self, memo):
        memo[id(self)] = self
        return self


class Registry:
    """Four weakly registered shared reservations; retaining receipts pins slots.

    reserve returns None for a duplicate/live-sequence or full quota. Malformed
    sequence raises ValueError. record returns False for an already sealed,
    malformed or oversized snapshot and leaves the reservation unsealed. It may
    retry a corrected input, but never replace a successfully sealed report.
    """
    def __init__(self):
        self._lock = threading.RLock()
        self._leases = weakref.WeakValueDictionary()

    def reserve(self, sequence):
        _positive(sequence, 'Capture sequence')
        with self._lock:
            if sequence in self._leases or len(self._leases) >= MAX_TICKETS: return None
            lease = _Lease(sequence)
            self._leases[sequence] = lease
            return Ticket(self, lease)

    def _record(self, lease, snapshot):
        with self._lock:
            if lease.payload is not None: return False
            try:
                report = analyze(snapshot)
                _require(len(_mentioned_slots(report['mutations'], report['selected_destination'])) <= MAX_SLOTS,
                         'Retained slot identity budget')
                envelope = dict(sequence=lease.sequence, history=report)
                wire = _wire(envelope)
                _require(len(wire.encode('ascii')) <= MAX_BYTES, 'Retained full report byte budget')
            except (ValueError, TypeError, RecursionError):
                return False
            lease.payload = wire
            return True

    def stats(self):
        with self._lock:
            leases = list(self._leases.values())
            payloads = [x.payload for x in leases if x.payload is not None]
            histories = [json.loads(x)['history'] for x in payloads]
            return dict(live_tickets=len(leases), sealed_tickets=len(payloads),
                        retained_records=sum(len(h['mutations']) for h in histories),
                        retained_slot_occurrences=sum(len(_mentioned_slots(h['mutations'], h['selected_destination']))
                                                      for h in histories),
                        serialized_bytes=sum(len(x.encode('ascii')) for x in payloads),
                        limits=dict(tickets=MAX_TICKETS, slots_per_ticket=MAX_SLOTS,
                                    records_per_ticket=MAX_RECORDS, bytes_per_ticket=MAX_BYTES))
