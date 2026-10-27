"""Bounded supplied-original CPU bind/history relation, never runtime evidence.

wire_text is retained byte-for-byte; audit_getter_wire is called once per wire.
No process, file, driver, shader, mapped memory or pixel oracle is accessed.
"""
from __future__ import annotations
import hashlib
import json
import re
import typed_copy_getter_validate as getters

n = getters.native
MAX_INPUT = 256 * 1024
MAX_OUTPUT = 1024 * 1024
MAX_NODES = 8192
MAX_DEPTH = 16
MAX_EDGES = 384
FALSE_CLAIMS = (
    'input_facts_attested', 'source_runtime_relation_known',
    'public_device_create_result_known', 'current_contents_at_draw_known',
    'unchanged_since_copy_proven', 'sampled_resource_GPU_read_proven',
    'renderer_owner_known', 'backing_known', 'global_API_absence_proven',
    'bind_runtime_proven', 'generic_PE_to_Unix_resource_mapping',
    'exclusive_phase_attribution_proven', 'old_process_or_backing_gate_repaired',
    'loaded_binary_source_equivalence_proven',
)
CREATES = {'D3D12CreateDevice': 'device', 'CreateCommittedResource': 'resource',
           'CreateDescriptorHeap': 'heap', 'CreateCommandAllocator': 'allocator',
           'CreateCommandList': 'command', 'CreateCommandQueue': 'queue', 'CreateFence': 'fence'}
ARGS = {
 'D3D12CreateDevice': '', 'CreateCommittedResource': 'device description',
 'CreateDescriptorHeap': 'device description', 'CreateCommandAllocator': 'device type',
 'CreateCommandList': 'device allocator node_mask type initial_pipeline',
 'CreateCommandQueue': 'device description', 'CreateFence': 'device initial_value flags',
 'GetDescriptorHandleIncrementSize': 'device heap_type',
 'GetCPUDescriptorHandleForHeapStart': 'heap', 'GetGPUDescriptorHandleForHeapStart': 'heap',
 'CreateShaderResourceView': 'device resource destination description',
 'CopyDescriptorsSimple': 'device count heap_type source destination',
 'Reset': 'command recording_epoch allocator initial_pipeline',
 'SetDescriptorHeaps': 'command recording_epoch heaps',
 'SetGraphicsRootDescriptorTable': 'command recording_epoch parameter gpu_handle',
 'Close': 'command recording_epoch', 'SealSourceReceipt': 'sequence',
 'ExecuteCommandLists': 'queue commands', 'Signal': 'queue fence value',
 'SetEventOnCompletion': 'fence value event', 'WaitForSingleObject': 'event timeout_ms',
 'GetCompletedValue': 'fence', 'Map': 'resource subresource range',
 'Release': 'object_kind object',
}
RETURNS = {k: 'HRESULT' for k in CREATES}
RETURNS.update({k: 'HRESULT' for k in ('Reset', 'Close', 'Signal', 'SetEventOnCompletion', 'Map')})
RETURNS.update({k: 'void' for k in ('CreateShaderResourceView', 'CopyDescriptorsSimple',
                                  'SetDescriptorHeaps', 'SetGraphicsRootDescriptorTable', 'ExecuteCommandLists')})
RETURNS.update(GetDescriptorHandleIncrementSize='uint32', GetCompletedValue='uint64',
               WaitForSingleObject='uint32', Release='uint32',
               GetCPUDescriptorHandleForHeapStart='CPU_HANDLE', GetGPUDescriptorHandleForHeapStart='GPU_HANDLE',
               SealSourceReceipt='control')
OBJECT_KEYS = 'device resource_A resource_B cpu_heap visible_heap sampler_heap allocator command queue fence readback'
CONTEXT_KEYS = ('ordinal name source_sequence command recording_epoch selected_heap selected_index '
                'object_calls getter_calls typed_calls reset_call bind_call root0_call close_call seal_call '
                'execute_call signal_call event_call wait_call completed_call map_call readback')
TEXTURE = dict(dimension=3, width=8, height=8, depth_or_array_size=1, mip_levels=1, format=28, sample_count=1, flags=0)
READBACK = dict(dimension=1, width=32, height=1, depth_or_array_size=1, mip_levels=1, format=0, sample_count=1, flags=0)


def _bounded_encoded(value, cap):
    size = 0
    try:
        for part in json.JSONEncoder(separators=(',', ':'), ensure_ascii=True, allow_nan=False).iterencode(value):
            size += len(part.encode('ascii'))
            n.require(size <= cap, 'Complete serialized budget, no truncation')
    except (TypeError, UnicodeError, RecursionError) as error:
        raise ValueError('Bounded integer JSON only') from error


def _parse(data):
    if type(data) in (bytes, str):
        try:
            raw = data if type(data) is bytes else data.encode('utf8', errors='strict')
            n.require(len(raw) <= MAX_INPUT, 'Outer input byte budget')
            data = json.loads(raw.decode('utf8', errors='strict'), object_pairs_hook=n._pairs,
                              parse_constant=lambda _: n.require(False, 'Nonfinite JSON'))
        except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
            raise ValueError('Bounded UTF8 JSON') from error
    nodes = 0
    def copy(x, depth=0, key=''):
        nonlocal nodes
        nodes += 1
        n.require(nodes <= MAX_NODES and depth <= MAX_DEPTH, 'Outer node/depth budget')
        if x is None or type(x) is bool:
            return x
        if type(x) is int:
            n.require(-(1 << 31) <= x <= n.MAX_U64, 'Bounded integer')
            return x
        if type(x) is str:
            try: length = len(x.encode('utf8', errors='strict'))
            except UnicodeError as error: raise ValueError('UTF8 string') from error
            n.require(length <= (16384 if key == 'wire_text' else 256), 'Bounded string')
            return x
        if type(x) is list:
            n.require(len(x) <= MAX_EDGES, 'Array budget')
            return [copy(v, depth + 1) for v in x]
        if type(x) is dict:
            n.require(len(x) <= 64 and all(type(k) is str and len(k) <= 128 for k in x), 'Bounded keys')
            return {k: copy(v, depth + 1, k) for k, v in x.items()}
        raise ValueError('Exact integer JSON types only')
    try: value = copy(data)
    except (RecursionError, RuntimeError) as error: raise ValueError('Bounded acyclic JSON') from error
    _bounded_encoded(value, MAX_INPUT)
    return value


def _ref(value):
    n.obj(value, 'raw generation', 'APP occurrence reference')
    n.positive(value['raw'], 'APP raw token'); n.positive(value['generation'], 'APP generation')
    return value['raw'], value['generation']


def _callref(value, calls, api=None):
    n.obj(value, 'ordinal entry return', 'APP CallRef')
    for x in value.values(): n.positive(x, 'APP CallRef scalar')
    n.require(value['ordinal'] in calls, 'Original call exists')
    c = calls[value['ordinal']]
    n.exact(value, dict(ordinal=c['ordinal'], entry=c['entry'], **{'return': c['return']}), 'Exact saved CallRef')
    if api: n.exact(c['api'], api, 'Original method')
    return c


def _output(c):
    return c['result']['output']


def _value(c):
    return c['result']['value']


def _argument_types(api, a):
    # Validate every retained call, including future calls outside a case cutoff.
    for key in ('recording_epoch', 'gpu_handle', 'event', 'sequence', 'value', 'initial_value', 'initial_pipeline'):
        if key in a: n.uint(a[key], 'Original ' + key)
    for key in ('heap_type', 'count', 'node_mask', 'type', 'flags', 'parameter', 'timeout_ms', 'subresource'):
        if key in a: n.uint(a[key], 'Original enum/UINT32 ' + key, (1 << 32) - 1)
    if api == 'CreateCommittedResource':
        d = a['description']; n.obj(d, 'dimension width height depth_or_array_size mip_levels format sample_count flags', 'Original retained resource description')
        for key, v in d.items(): n.uint(v, 'Original resource ' + key, n.MAX_U64 if key == 'width' else (1 << 32) - 1)
    elif api in ('CreateDescriptorHeap', 'CreateCommandQueue'):
        keys = 'heap_type count flags node_mask' if api == 'CreateDescriptorHeap' else 'type priority flags node_mask'
        n.obj(a['description'], keys, 'Original retained creation description')
        for key, v in a['description'].items():
            if key == 'priority': n.require(type(v) is int and -(1 << 31) <= v < (1 << 31), 'Original queue priority')
            else: n.uint(v, 'Original creation enum/UINT ' + key, (1 << 32) - 1)
    elif api == 'CreateShaderResourceView':
        n.obj(a['description'], 'format view_dimension component_mapping most_detailed_mip mip_levels plane_slice min_lod_clamp_bits', 'Original explicit SRV description')
        for key, v in a['description'].items(): n.uint(v, 'Original SRV bits ' + key, (1 << 32) - 1)
    elif api == 'Map':
        n.require(type(a['range']) is list and len(a['range']) == 2, 'Original Map range')
        for v in a['range']: n.uint(v, 'Original Map range value')
        n.require(a['range'][0] <= a['range'][1], 'Original Map range order')
    elif api == 'Release':
        n.require(a['object_kind'] in set(CREATES.values()), 'Declared original object class')

def _journal(value):
    n.obj(value, 'clock_domain scope events', 'New mixed APP original/control journal')
    n.exact(value['clock_domain'], 'independent_app_original_transition', 'APP clock domain')
    n.exact(value['scope'], 'declared_owned_CPU_mock_subset', 'Declared subset, no global absence')
    rows = value['events']; n.require(type(rows) is list and 0 < len(rows) <= MAX_EDGES, '384 APP edges')
    calls = {}; pending = set(); previous = 0; ordinal = 0
    for r in rows:
        n.obj(r, 'ordinal transition api edge case_ordinal return_kind args original_returned result', 'Original/control edge')
        n.positive(r['ordinal'], 'Call ordinal'); n.positive(r['transition'], 'APP transition')
        n.require(r['transition'] > previous, 'Unique increasing APP edge clock'); previous = r['transition']
        n.uint(r['case_ordinal'], 'APP case', 2)
        n.require(r['api'] in ARGS, 'Declared original/control method')
        n.exact(r['return_kind'], RETURNS[r['api']], 'Original return kind')
        n.obj(r['args'], ARGS[r['api']], 'Frozen original retained arguments'); _argument_types(r['api'], r['args'])
        if r['edge'] == 'before':
            n.require(r['ordinal'] > ordinal and r['ordinal'] not in calls, 'Call IDs monotone at ENTRY'); ordinal = r['ordinal']
            n.exact(r['original_returned'], False, 'No return before original'); n.exact(r['result'], None, 'No result before original')
            calls[ordinal] = dict(ordinal=ordinal, entry=r['transition'], **{'return': 0}, api=r['api'], case=r['case_ordinal'], kind=r['return_kind'], args=r['args'], result=None)
            pending.add(ordinal)
        else:
            n.exact(r['edge'], 'after', 'Original edge kind')
            n.require(r['ordinal'] in pending, 'Exact one original completion'); c = calls[r['ordinal']]
            for a, b in (('api', 'api'), ('case_ordinal', 'case'), ('return_kind', 'kind'), ('args', 'args')): n.exact(r[a], c[b], 'Frozen BEFORE preserved')
            n.exact(r['original_returned'], True, 'Original returned')
            result = r['result']; kind = c['kind']
            if kind == 'void': n.exact(result, None, 'Void original is null, not HRESULT')
            elif kind == 'HRESULT':
                n.obj(result, 'hresult output', 'Original HRESULT/output')
                n.require(type(result['hresult']) is int and -(1 << 31) <= result['hresult'] < (1 << 31), 'Signed original HRESULT')
                n.exact(result['hresult'], 0, 'First finite successful APP profile')
            elif kind == 'control':
                n.obj(result, 'sequence receipt_token wire_sha256 wire_bytes', 'Explicit observer seal output, no HRESULT')
                n.positive(result['sequence'], 'Observer sequence'); n.positive(result['receipt_token'], 'Observer receipt token')
                n.sha(result['wire_sha256']); n.uint(result['wire_bytes'], 'Observer original byte length', 16384)
            else:
                n.obj(result, 'value', 'Original scalar projection')
                n.uint(result['value'], 'Original scalar value', (1 << 32) - 1 if kind == 'uint32' else n.MAX_U64)
            c['return'] = r['transition']; c['result'] = result; pending.remove(c['ordinal'])
    n.require(not pending and len(calls) <= 192, 'Closed bounded APP occurrences')
    return calls


def _objects(calls):
    objects = {}; generations = {}; last_raw = {}
    for c in calls.values():
        if c['api'] in CREATES:
            kind = CREATES[c['api']]; r = _output(c); raw, generation = _ref(r)
            n.require(generation > generations.get(kind, 0), 'APP generations monotone at original CREATE ENTRY'); generations[kind] = generation
            key = kind, raw, generation
            n.require(key not in objects, 'Unique APP object occurrence')
            previous = last_raw.get((kind, raw))
            if previous:
                n.require(previous['release_return'] and previous['release_return'] < c['entry'], 'No raw reuse across pending lifetime')
            o = dict(kind=kind, ref=r, create=c, release_entry=0, release_return=0)
            objects[key] = o; last_raw[kind, raw] = o
        elif c['api'] == 'Release':
            a = c['args']; raw, generation = _ref(a['object']); key = a['object_kind'], raw, generation
            n.require(key in objects, 'Release exact known occurrence'); o = objects[key]
            n.require(not o['release_entry'] and o['create']['return'] < c['entry'], 'Unique original Release after create')
            n.exact(_value(c), 0, 'Declared owned final Release only')
            o['release_entry'] = c['entry']; o['release_return'] = c['return']
    def alive(ref, kind, entry, returned=None):
        raw, generation = _ref(ref); key = kind, raw, generation
        n.require(key in objects, 'Original object kind/generation exists')
        o = objects[key]; end = entry if returned is None else returned
        n.require(o['create']['return'] < entry and (not o['release_entry'] or end < o['release_entry']), 'Original object live through use')
        return o
    for c in calls.values():
        a = c['args']; api = c['api']
        for field, kind in (('device', 'device'), ('command', 'command'), ('allocator', 'allocator'), ('queue', 'queue'), ('fence', 'fence'), ('heap', 'heap'), ('resource', 'resource')):
            if field in a: alive(a[field], kind, c['entry'], c['return'])
        if api == 'SetDescriptorHeaps':
            n.require(type(a['heaps']) is list and 1 <= len(a['heaps']) <= 2, 'Finite original heap list')
            for ref in a['heaps']: alive(ref, 'heap', c['entry'], c['return'])
        if api == 'ExecuteCommandLists':
            n.require(type(a['commands']) is list and len(a['commands']) == 1, 'One original command list')
            alive(a['commands'][0], 'command', c['entry'], c['return'])
        if api in ('CreateShaderResourceView', 'CopyDescriptorsSimple'):
            for side in ('source', 'destination'):
                if side in a:
                    slot = a[side]; n.obj(slot, 'heap index cpu_handle', 'Original slot arguments')
                    alive(slot['heap'], 'heap', c['entry'], c['return']); n.uint(slot['index'], 'Original index', (1 << 32) - 1); n.uint(slot['cpu_handle'], 'Original handle')
    return objects, alive


def _recordings(calls):
    actions = []
    for c in calls.values(): actions.extend(((c['entry'], False, c), (c['return'], True, c)))
    states = {}; busy = {}
    for _, after, c in sorted(actions, key=lambda x: x[0]):
        api = c['api']; a = c['args']
        commands = a['commands'] if api == 'ExecuteCommandLists' else [a['command']] if api in ('Reset', 'Close', 'SetDescriptorHeaps', 'SetGraphicsRootDescriptorTable') else []
        for command in commands:
            key = _ref(command)
            if after:
                n.exact(busy.pop(key, None), c['ordinal'], 'Exact serial command operation RETURN')
            else:
                n.require(key not in busy, 'No overlapping original operations on one command occurrence')
                busy[key] = c['ordinal']
        if api == 'CreateCommandList' and after:
            states[_ref(_output(c))] = dict(epoch=0, state='recording')
        elif api in ('Reset', 'Close', 'SetDescriptorHeaps', 'SetGraphicsRootDescriptorTable'):
            key = _ref(a['command']); n.require(key in states, 'Command created before recording operation'); s = states[key]
            n.uint(a['recording_epoch'], 'APP recording epoch')
            if api == 'Reset':
                if not after:
                    n.require(s['state'] == 'closed' and a['recording_epoch'] == s['epoch'] + 1, 'Original Reset only closed prior epoch')
                    s['state'] = 'reset_pending'
                else:
                    n.exact(s['state'], 'reset_pending', 'Exact Reset RETURN'); s.update(epoch=a['recording_epoch'], state='recording')
            elif api == 'Close':
                n.exact(a['recording_epoch'], s['epoch'], 'Close exact epoch')
                n.exact(s['state'], 'close_pending' if after else 'recording', 'Original Close state')
                s['state'] = 'closed' if after else 'close_pending'
            else:
                n.exact(a['recording_epoch'], s['epoch'], 'Bind/root exact epoch'); n.exact(s['state'], 'recording', 'Bind/root in open recording')
        elif api == 'ExecuteCommandLists' and not after:
            for command in a['commands']: n.exact(states[_ref(command)]['state'], 'closed', 'Execute closed original command')


def _expect(c, args): n.exact(c['args'], args, 'Original arguments by value')
def _null_output(c): n.exact(_output(c), None, 'Original HRESULT has no value output')
def _before(a, b): n.require(a['return'] < b['entry'], 'Original/control occurrence order')


def _case(context, receipt, wire, audit, calls, objects, alive, previous_context):
    n.obj(context, CONTEXT_KEYS, 'Case context')
    ordinal = context['ordinal']; n.uint(ordinal, 'Case ordinal', 2); n.require(ordinal in (1, 2), 'Two cases')
    n.exact(context['name'], ('direct_A_clamp', 'after_bind_B_clamp')[ordinal - 1], 'Schedule name, not pixel oracle')
    n.exact(context['source_sequence'], receipt['sequence'], 'Source receipt sequence')
    n.exact(context['selected_index'], 0, 'Finite slot zero')
    n.obj(context['object_calls'], OBJECT_KEYS, 'Immutable APP creation CallRefs')
    ap = {}
    expected_api = dict(device='D3D12CreateDevice', resource_A='CreateCommittedResource', resource_B='CreateCommittedResource', cpu_heap='CreateDescriptorHeap', visible_heap='CreateDescriptorHeap', sampler_heap='CreateDescriptorHeap', allocator='CreateCommandAllocator', command='CreateCommandList', queue='CreateCommandQueue', fence='CreateFence', readback='CreateCommittedResource')
    creations = {}
    for role, ref in context['object_calls'].items():
        if role == 'sampler_heap' and ref is None: continue
        c = _callref(ref, calls, expected_api[role]); ap[role] = _output(c); creations[role] = c
    dev = ap['device']; _expect(creations['device'], {})
    for role in ('resource_A', 'resource_B'): _expect(creations[role], dict(device=dev, description=TEXTURE))
    for role, desc in (('cpu_heap', dict(heap_type=0, count=2, flags=0, node_mask=0)), ('visible_heap', dict(heap_type=0, count=1, flags=1, node_mask=0)), ('sampler_heap', dict(heap_type=1, count=2, flags=1, node_mask=0))):
        if role in ap: _expect(creations[role], dict(device=dev, description=desc))
    _expect(creations['allocator'], dict(device=dev, type=0))
    _expect(creations['command'], dict(device=dev, allocator=ap['allocator'], node_mask=0, type=0, initial_pipeline=0))
    _expect(creations['queue'], dict(device=dev, description=dict(type=0, priority=0, flags=0, node_mask=0)))
    _expect(creations['fence'], dict(device=dev, initial_value=0, flags=0))
    _expect(creations['readback'], dict(device=dev, description=READBACK))
    n.exact(context['command'], ap['command'], 'Exact command occurrence')
    n.exact(context['selected_heap'], ap['visible_heap'], 'Actual visible heap occurrence')
    n.positive(context['recording_epoch'], 'Recording epoch')
    cs = {}
    for key, api in (('reset_call', 'Reset'), ('bind_call', 'SetDescriptorHeaps'), ('root0_call', 'SetGraphicsRootDescriptorTable'), ('close_call', 'Close'), ('seal_call', 'SealSourceReceipt'), ('execute_call', 'ExecuteCommandLists'), ('signal_call', 'Signal'), ('event_call', 'SetEventOnCompletion'), ('wait_call', 'WaitForSingleObject'), ('completed_call', 'GetCompletedValue'), ('map_call', 'Map')):
        cs[key] = _callref(context[key], calls, api); n.exact(cs[key]['case'], ordinal, 'Call belongs to this supplied case')
    command_args = dict(command=ap['command'], recording_epoch=context['recording_epoch'])
    _expect(cs['reset_call'], dict(**command_args, allocator=ap['allocator'], initial_pipeline=0)); _null_output(cs['reset_call'])
    heaps = [ap['visible_heap']] + ([ap['sampler_heap']] if 'sampler_heap' in ap else [])
    _expect(cs['bind_call'], dict(**command_args, heaps=heaps))
    _expect(cs['close_call'], command_args); _null_output(cs['close_call'])
    _expect(cs['seal_call'], dict(sequence=receipt['sequence']))
    n.exact(cs['seal_call']['result'], dict(sequence=receipt['sequence'], receipt_token=receipt['receipt_token'], wire_sha256=receipt['wire_sha256'], wire_bytes=len(receipt['wire_text'].encode('utf8'))), 'Exact sealed receipt output inside APP control interval')
    _expect(cs['execute_call'], dict(queue=ap['queue'], commands=[ap['command']]))
    signal = cs['signal_call']; n.obj(signal['args'], 'queue fence value', 'Signal arguments')
    value = signal['args']['value']; n.positive(value, 'Original fence value')
    _expect(signal, dict(queue=ap['queue'], fence=ap['fence'], value=value)); _null_output(signal)
    event = cs['event_call']['args']['event']; n.positive(event, 'Original event handle')
    _expect(cs['event_call'], dict(fence=ap['fence'], value=value, event=event)); _null_output(cs['event_call'])
    timeout = cs['wait_call']['args']['timeout_ms']; n.uint(timeout, 'Original timeout', (1 << 32) - 1)
    _expect(cs['wait_call'], dict(event=event, timeout_ms=timeout)); n.exact(_value(cs['wait_call']), 0, 'WAIT_OBJECT_0')
    _expect(cs['completed_call'], dict(fence=ap['fence']))
    completed = _value(cs['completed_call']); n.require(value <= completed < n.MAX_U64, 'Completed value, no device-removed sentinel')
    _expect(cs['map_call'], dict(resource=ap['readback'], subresource=0, range=[0, 32]))
    n.obj(_output(cs['map_call']), 'pointer', 'Defined successful Map output'); n.positive(_output(cs['map_call'])['pointer'], 'Original Map pointer')
    n.obj(context['readback'], 'kind bytes_hex sha256', 'Supplied ordinary CPU checksum')
    n.exact(context['readback']['kind'], 'CPU_mock_not_GPU_pixels', 'No pixel/readback proof')
    text = context['readback']['bytes_hex']; n.require(type(text) is str and re.fullmatch('[0-9a-f]{64}', text), 'Exactly32 supplied CPU bytes')
    n.sha(context['readback']['sha256']); n.exact(hashlib.sha256(bytes.fromhex(text)).hexdigest(), context['readback']['sha256'], 'Supplied checksum self-consistency only')
    order = ['reset_call', 'bind_call', 'root0_call', 'close_call', 'seal_call', 'execute_call', 'signal_call', 'event_call', 'wait_call', 'completed_call', 'map_call']
    for a, b in zip(order, order[1:]): _before(cs[a], cs[b])
    # Reconstruct the selected original recording at this case's own cutoff.
    # Future Reset/getters after this Execute cannot retag this context.
    def last_command(api, before, parameter=None):
        rows = [x for x in calls.values() if x['api'] == api and x['args'].get('command') == ap['command'] and x['return'] < before and (parameter is None or x['args'].get('parameter') == parameter)]
        n.require(rows, 'Recorded original command operation')
        return max(rows, key=lambda x: x['return'])
    n.exact(last_command('Reset', cs['execute_call']['entry'])['ordinal'], cs['reset_call']['ordinal'], 'No intervening Reset replaces the saved recording')
    n.exact(last_command('Close', cs['execute_call']['entry'])['ordinal'], cs['close_call']['ordinal'], 'Execute refers to this exact successful Close')
    n.exact(last_command('SetDescriptorHeaps', cs['close_call']['entry'])['ordinal'], cs['bind_call']['ordinal'], 'Actual last heap bind in this recording')
    n.exact(last_command('SetGraphicsRootDescriptorTable', cs['close_call']['entry'], 0)['ordinal'], cs['root0_call']['ordinal'], 'Actual last root0 argument in this recording')
    registrations = [x for x in calls.values() if x['api'] == 'SetEventOnCompletion' and x['args']['event'] == event and x['return'] < cs['wait_call']['entry']]
    n.require(registrations and max(registrations, key=lambda x: x['return'])['ordinal'] == cs['event_call']['ordinal'], 'Wait joins this exact event registration')
    executions = [x for x in calls.values() if x['api'] == 'ExecuteCommandLists' and ap['command'] in x['args']['commands'] and cs['reset_call']['entry'] < x['entry'] < cs['map_call']['return']]
    n.exact([x['ordinal'] for x in executions], [cs['execute_call']['ordinal']], 'One exact original producer Execute in this retained case')
    signals = [x for x in calls.values() if x['api'] == 'Signal' and x['args']['fence'] == ap['fence'] and x['entry'] < cs['map_call']['return']]
    n.require(signals and signals[-1]['ordinal'] == signal['ordinal'], 'No omitted Signal before this Map context')
    if previous_context:
        n.exact(context['object_calls'], previous_context['object_calls'], 'Same actual objects across cases')
        n.exact(context['getter_calls'], previous_context['getter_calls'], 'Same four cached original getter CallRefs')
        prev_map = _callref(previous_context['map_call'], calls); _before(prev_map, cs['reset_call'])
        prev_signal = _callref(previous_context['signal_call'], calls); n.require(value > prev_signal['args']['value'], 'Same fence Signal values strictly increase')
    n.obj(context['getter_calls'], 'increment cpu_heap visible_cpu visible_gpu', 'Cached original getter CallRefs')
    original_getters = {}
    for role, api, args in (('increment', 'GetDescriptorHandleIncrementSize', dict(device=dev, heap_type=0)), ('cpu_heap', 'GetCPUDescriptorHandleForHeapStart', dict(heap=ap['cpu_heap'])), ('visible_cpu', 'GetCPUDescriptorHandleForHeapStart', dict(heap=ap['visible_heap'])), ('visible_gpu', 'GetGPUDescriptorHandleForHeapStart', dict(heap=ap['visible_heap']))):
        c = _callref(context['getter_calls'][role], calls, api); _expect(c, args); _before(c, cs['bind_call'])
        candidates = [x for x in calls.values() if x['api'] == api and x['args'] == args and x['return'] < cs['bind_call']['entry']]
        n.require(len(candidates) == 1, 'Unique original APP getter before this use, no proximity/equal-value choice')
        original_getters[role] = c
    n.require(len({x['ordinal'] for x in original_getters.values()}) == 4, 'Four distinct selected APP getters')
    stride = _value(original_getters['increment']); cpu = _value(original_getters['cpu_heap']); visible = _value(original_getters['visible_cpu']); gpu = _value(original_getters['visible_gpu'])
    n.positive(stride, 'Observed SRV increment'); n.positive(cpu, 'Observed CPU base'); n.positive(visible, 'Observed visible CPU base'); n.positive(gpu, 'Observed visible GPU base')
    n.require(cpu <= n.MAX_U64 - 2 * stride and visible <= n.MAX_U64 - stride and gpu <= n.MAX_U64 - stride, 'Checked CPU/GPU declared heap spans')
    _expect(cs['root0_call'], dict(**command_args, parameter=0, gpu_handle=gpu))
    for role in ('cpu_heap', 'visible_heap'):
        alive(ap[role], 'heap', cs['bind_call']['entry'], cs['execute_call']['return'])
    n.require(type(context['typed_calls']) is list and len(context['typed_calls']) == (3 if ordinal == 1 else 4), 'Two supplied schedules3then4new originals')
    typed = [_callref(r, calls) for r in context['typed_calls']]
    n.require(all(c['api'] == 'CreateShaderResourceView' for c in typed[:3]), 'Three genuine SRV calls')
    if ordinal == 2: n.exact(typed[3]['api'], 'CopyDescriptorsSimple', 'One late original CopySimple')
    n.require(all(c['case'] == ordinal for c in typed), 'Typed calls belong to case')
    for a, b in zip(typed, typed[1:]): _before(a, b)
    _before(cs['reset_call'], typed[0]); _before(typed[2], cs['bind_call'])
    for original in typed:
        _before(original_getters['increment'], original)
        for side in ('source', 'destination'):
            if side in original['args']:
                heap = original['args'][side]['heap']
                role = 'cpu_heap' if heap == ap['cpu_heap'] else 'visible_cpu' if heap == ap['visible_heap'] else None
                n.require(role is not None, 'Typed original uses one selected retained heap')
                _before(original_getters[role], original)
    if ordinal == 2: _before(cs['close_call'], typed[3])
    _before(typed[-1], cs['seal_call'])
    all_typed = [c['ordinal'] for c in calls.values() if c['api'] in ('CreateShaderResourceView', 'CopyDescriptorsSimple') and c['case'] == ordinal and c['entry'] < cs['execute_call']['entry']]
    n.exact(all_typed, [c['ordinal'] for c in typed], 'No omitted original typed call before Execute')
    # A valid unavailable source receipt remains negative, with exact full legacy result.
    if not audit['prototype_source_history_known']:
        return dict(ordinal=ordinal, supplied_argument_relation_known=False, reasons=['source_history_unavailable'], getter_audit=audit, context=context, readback_digest_self_consistent=True, readback_bytes_origin_known=False)
    snapshot = wire['native_wire']['snapshot']; n.require(snapshot is not None, 'Known source snapshot')
    source_map = {}; mappings = []; mapped_edges = []
    def mapped_call(source, app):
        mapped_edges.extend(((source['entry'], app['entry']), (source['return'], app['return'])))
    for role, kind in (('device', 'device'), ('resource_A', 'resource'), ('resource_B', 'resource'), ('cpu_heap', 'heap'), ('visible_heap', 'heap')):
        raw, _ = _ref(ap[role]); pool = [snapshot['device']] if kind == 'device' else snapshot['resources'] if kind == 'resource' else snapshot['heaps']
        matches = [o for o in pool if o['raw'] == raw]; n.require(len(matches) == 1, 'Unique same-run source/raw creation occurrence')
        o = matches[0]; sr = dict(raw=o['raw'], generation=o['generation']); source_map[(kind,) + _ref(sr)] = ap[role]
        n.require(o['create_return'] and o['create_return'] < wire['cutoff']['transition'] and (not o['release_entry'] or wire['cutoff']['transition'] < o['release_entry']), 'Source occurrence live at frozen cutoff')
        if kind == 'resource': n.exact(o['description'], creations[role]['args']['description'], 'Original resource description by value')
        if kind == 'heap':
            n.exact({k: o[k] for k in ('heap_type', 'count', 'flags', 'node_mask')}, creations[role]['args']['description'], 'Original heap description by value')
        # Separate clocks/generation labels cannot disambiguate reused raw
        # values. An earlier closed APP occurrence is still a candidate for
        # this supplied SOURCE creation. Future creates after this Seal are
        # outside this retained case's domain and cannot retag its mapping.
        candidates = []
        for candidate in calls.values():
            if CREATES.get(candidate['api']) != kind or candidate['return'] >= cs['seal_call']['entry']:
                continue
            if _output(candidate)['raw'] != raw:
                continue
            if kind != 'device':
                if candidate['args']['device'] != dev:
                    continue
                description = (o['description'] if kind == 'resource' else
                               {k: o[k] for k in ('heap_type', 'count', 'flags', 'node_mask')})
                if candidate['args']['description'] != description:
                    continue
            candidates.append(candidate['ordinal'])
        n.exact(candidates, [creations[role]['ordinal']], 'Unique APP original creation candidate, no raw-reuse/generation choice')
        if kind != 'device':
            # Resource/heap original occurrences are mapped by retained args.
            # The internal source device factory is not a public factory call.
            mapped_call(dict(entry=o['create_entry'], **{'return': o['create_return']}), creations[role])
        mappings.append(dict(role=role, app=ap[role], source=sr))
    def translate_ref(ref, kind):
        key = (kind,) + _ref(ref); n.require(key in source_map, 'Source reference maps to exact APP occurrence'); return source_map[key]
    def translate_before(event):
        b = json.loads(json.dumps(event['before']))
        b['device'] = translate_ref(b['device'], 'device')
        if 'resource' in b: b['resource'] = translate_ref(b['resource'], 'resource')
        for side in ('source', 'destination'):
            if side in b: b[side]['heap'] = translate_ref(b[side]['heap'], 'heap')
        return b
    selected_heap = next(o for o in snapshot['heaps'] if translate_ref(dict(raw=o['raw'], generation=o['generation']), 'heap') == ap['visible_heap'])
    n.exact(snapshot['selected_destination'], dict(heap=dict(raw=selected_heap['raw'], generation=selected_heap['generation']), index=0, cpu_handle=visible), 'Exact frozen selected source visible slot0/CPU base')
    records = wire['getters']['records']; n.require(len(records) == 4, 'Four unique selected SOURCE getter occurrences')
    getter_links = {}
    for role, kind, obj in (('increment', 'increment', ap['device']), ('cpu_heap', 'cpu_start', ap['cpu_heap']), ('visible_cpu', 'cpu_start', ap['visible_heap']), ('visible_gpu', 'gpu_start', ap['visible_heap'])):
        matches = [g for g in records if g['kind'] == kind and translate_ref(g['source_object'], 'device' if kind == 'increment' else 'heap') == obj]
        n.require(len(matches) == 1, 'Unique SOURCE getter, repeated equal values remain ambiguous'); g = matches[0]
        n.require(g['value_known'] and g['lifetime_valid_through_return'] and g['arithmetic_eligible'] and g['return'] < wire['cutoff']['transition'], 'Eligible original getter before frozen cutoff')
        n.exact(g['value'], _value(original_getters[role]), 'Same original scalar projection across declared domains')
        n.exact(translate_ref(g['parent_device'], 'device'), dev, 'Original source parent device')
        mapped_call(g, original_getters[role])
        getter_links[role] = dict(app=context['getter_calls'][role], source_getter_id=g['id'])
    n.require(len(wire['publications']) == 2, 'Exact CPU publication attempts')
    for role in ('cpu_heap', 'visible_heap'):
        matches = [p for p in wire['publications'] if translate_ref(p['heap'], 'heap') == ap[role]]; n.require(len(matches) == 1, 'Exact source heap publication'); pub = matches[0]
        n.require(pub['heap_facts_called'] and pub['published'] and pub['span_arithmetic_valid'], 'Actual successful observer publication')
        n.exact(pub['cpu_getter_id'], getter_links['cpu_heap' if role == 'cpu_heap' else 'visible_cpu']['source_getter_id'], 'Exact publication CPU getter ID')
        n.exact(pub['increment_getter_id'], getter_links['increment']['source_getter_id'], 'Exact publication increment getter ID')
    source_events = [e for e in snapshot['events'] if e['phase_before']['case_ordinal'] == ordinal]
    n.require(len(source_events) == len(typed), 'Exact source/app typed original count in window')
    typed_links = []
    for event, original in zip(source_events, typed):
        n.exact(event['api'], original['api'], 'Same supplied original method'); n.exact(translate_before(event), original['args'], 'Original typed arguments by value')
        n.require(event['return'] and event['return'] < wire['cutoff']['transition'], 'Original typed RETURN before seal cutoff')
        mapped_call(event, original)
        typed_links.append(dict(app=dict(ordinal=original['ordinal'], entry=original['entry'], **{'return': original['return']}), source_call_id=event['call_id']))
    ordered = sorted(mapped_edges)
    n.require(len({x[0] for x in ordered}) == len(ordered) and all(a[1] < b[1] for a, b in zip(ordered, ordered[1:])), 'Mapped original ENTRY/RETURN order embedding, never clock equality')
    selected = audit['native_model_replay']['history']['selected_version']['origin']['resource']
    return dict(ordinal=ordinal, supplied_argument_relation_known=True, reasons=[], getter_audit=audit, context=context, occurrence_map=mappings, getter_links=getter_links, typed_links=typed_links, historical_origin_app_resource=translate_ref(selected, 'resource'), readback_digest_self_consistent=True, readback_bytes_origin_known=False)


def analyze(data):
    """Audit exactly two bounded supplied CPU cases; no I/O or execution."""
    value = _parse(data)
    n.obj(value, 'schema_version purpose run_id source_receipts app_journal cases claims', 'New joint input')
    n.exact(value['schema_version'], 1, 'Joint schema'); n.exact(value['purpose'], 'supplied_original_typed_copy_bind_CPU_inputs', 'Distinct CPU input purpose'); n.nonce(value['run_id'])
    n.obj(value['claims'], ' '.join(FALSE_CLAIMS), 'Unknown stronger facets')
    for k in FALSE_CLAIMS: n.exact(value['claims'][k], False, k)
    n.require(type(value['source_receipts']) is list and len(value['source_receipts']) == 2 and type(value['cases']) is list and len(value['cases']) == 2, 'Exactly two wires/cases')
    parsed = []; audits = []; tokens = set()
    for i, receipt in enumerate(value['source_receipts'], 1):
        n.obj(receipt, 'sequence receipt_token wire_text wire_sha256', 'Exact source receipt bytes')
        n.exact(receipt['sequence'], i, 'Source sequence1/2'); n.positive(receipt['receipt_token'], 'Opaque receipt token')
        n.require(receipt['receipt_token'] not in tokens, 'Distinct run-bound receipt tokens'); tokens.add(receipt['receipt_token'])
        text = receipt['wire_text']; n.require(type(text) is str, 'Original wire UTF8 text'); raw = text.encode('utf8', errors='strict'); n.require(len(raw) <= 16384, 'Each complete original source wire')
        n.sha(receipt['wire_sha256']); n.exact(hashlib.sha256(raw).hexdigest(), receipt['wire_sha256'], 'EXACT UTF8 digest, not re-JSON')
        wire = n.parse(raw, 16384)  # Child4096nodes/depth24, independent of outer budget.
        audit = getters.audit_getter_wire(raw)  # Exactly once per wire. Preserve whole result.
        n.exact(audit['run_id'], value['run_id'], 'Source/app same supplied run'); n.exact(audit['sequence'], i, 'Source receipt sequence')
        parsed.append(wire); audits.append(audit)
    n.exact(parsed[0]['native_wire']['producer']['epoch'], parsed[1]['native_wire']['producer']['epoch'], 'One genuine source epoch, no reset/re-registration')
    if all(a['prototype_source_history_known'] for a in audits):
        n.exact(parsed[0]['getters'], parsed[1]['getters'], 'Same four immutable cached SOURCE getter occurrences across cutoffs')
        n.exact(parsed[0]['publications'], parsed[1]['publications'], 'Same exact SOURCE publication attempts and getter IDs across cutoffs')
    if parsed[0]['native_wire']['snapshot'] is not None and parsed[1]['native_wire']['snapshot'] is not None:
        a = parsed[0]['native_wire']['snapshot']; b = parsed[1]['native_wire']['snapshot']
        for key in ('device', 'resources', 'heaps'): n.exact(a[key], b[key], 'Same immutable source creations at two cutoffs')
        n.exact(b['events'][:len(a['events'])], a['events'], 'Old typed occurrences are immutable prefix')
    calls = _journal(value['app_journal']); objects, alive = _objects(calls); _recordings(calls)
    fence_values = {}
    for c in calls.values():
        if c['api'] == 'Signal':
            key = _ref(c['args']['fence']); val = c['args']['value']; n.positive(val, 'Original Signal value')
            n.require(val > fence_values.get(key, 0), 'Original fence Signals strictly increase'); fence_values[key] = val
        elif c['api'] == 'GetCompletedValue': n.require(_value(c) < n.MAX_U64, 'No completed device-removed sentinel')
    reports = []
    for i in range(2):
        n.exact(value['cases'][i]['ordinal'], i + 1, 'Case order')
        reports.append(_case(value['cases'][i], value['source_receipts'][i], parsed[i], audits[i], calls, objects, alive, value['cases'][i - 1] if i else None))
    report = dict(schema_version=1, purpose='supplied_original_typed_copy_bind_history_audit', run_id=value['run_id'], prototype_only=True, vendor_built=False, runtime_executed=False, supplied_argument_relation_known=all(x['supplied_argument_relation_known'] for x in reports), cases=reports, app_journal=value['app_journal'], source_wire_sha256=[r['wire_sha256'] for r in value['source_receipts']], readback_bytes_origin_known=False, **{k: False for k in FALSE_CLAIMS})
    _bounded_encoded(report, MAX_OUTPUT)
    return report


audit_pair = analyze
