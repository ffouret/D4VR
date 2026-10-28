"""Pure bounded supplied-fact OUTER ReceiptCopy/control/APP relation.

No files, original APIs, processes, modules or GPU calls are accessed.  The
private CPU auditor is invoked once for each primary ORIGINAL OUTER byte span;
its entire result and prototype flags are preserved.  There is no child raw
byte digest, reserialization attestation or old bind-analyzer invocation.
"""
from __future__ import annotations

from copy import deepcopy
import hashlib
import json
import re

from private_translator_control_validate import audit_private_source_wire
import private_translator_control_validate as private

n = private.n
MAX_INPUT = 524288
MAX_OUTPUT = 4194304
MAX_NODES = 16384
MAX_DEPTH = 18
MAX_CALLS = 256
MAX_EDGES = 512
MAX_WIRE = 16384
MAX_COPIES = 4
U32 = (1 << 32) - 1
CLAIMS = private.CLAIMS + (
    'all_original_arguments_captured', 'current_descriptor_bytes_known',
    'readback_bytes_origin_known', 'generic_hidden_sret_pointer_mapping_known',
    'physical_device_mapping_known', 'source_memory_attested',
    'all_source_calls_observed', 'exclusive_thread_attribution_proven',
)
FALSE_CLAIMS = CLAIMS
CONTROL_PREFIX = 'D4VR_TypedCopy'
CONTROL_ARGS = {
 'D4VR_TypedCopyBeginV1': 'abi_version nonce_pointer nonce_bytes nonce_text enabled run_output_pointer',
 'D4VR_TypedCopyExpectV1': 'run_token role admission_sequence',
 'D4VR_TypedCopySealV1': 'run_token sequence owner_alias_output_pointer bytes_output_pointer',
 'D4VR_TypedCopyReceiptCopyV1': 'run_token owner_alias output_pointer capacity written_pointer required_pointer',
 'D4VR_TypedCopyReceiptRetainV1': 'run_token owner_alias new_owner_output_pointer',
 'D4VR_TypedCopyReceiptDropV1': 'run_token owner_alias',
}
CONTROL_OUTPUTS = {
 'D4VR_TypedCopyBeginV1': 'run_token', 'D4VR_TypedCopyExpectV1': '',
 'D4VR_TypedCopySealV1': 'owner_alias bytes',
 'D4VR_TypedCopyReceiptCopyV1': 'written required',
 'D4VR_TypedCopyReceiptRetainV1': 'new_owner_alias',
 'D4VR_TypedCopyReceiptDropV1': '',
}
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
 'Close': 'command recording_epoch', 'ExecuteCommandLists': 'queue commands',
 'Signal': 'queue fence value', 'SetEventOnCompletion': 'fence value event',
 'WaitForSingleObject': 'event timeout_ms', 'GetCompletedValue': 'fence',
 'Map': 'resource subresource range', 'Unmap': 'resource subresource written_range',
 'Release': 'object_kind object',
 'GetCopyableFootprints': 'device description first_subresource num_subresources base_offset layout_output_pointer num_rows_output_pointer row_bytes_output_pointer total_bytes_output_pointer',
}
RETURNS = {k: 'HRESULT' for k in CREATES}
RETURNS.update({k: 'HRESULT' for k in ('Reset', 'Close', 'Signal', 'SetEventOnCompletion', 'Map')})
RETURNS.update({k: 'void' for k in ('CreateShaderResourceView', 'CopyDescriptorsSimple',
 'SetDescriptorHeaps', 'SetGraphicsRootDescriptorTable', 'ExecuteCommandLists', 'Unmap', 'GetCopyableFootprints')})
RETURNS.update(GetDescriptorHandleIncrementSize='uint32', GetCompletedValue='uint64',
 WaitForSingleObject='uint32', Release='uint32',
 GetCPUDescriptorHandleForHeapStart='CPU_HANDLE', GetGPUDescriptorHandleForHeapStart='GPU_HANDLE')
OBJECT_KEYS = 'device resource_A resource_B cpu_heap visible_heap sampler_heap allocator command queue fence readback'
CONTEXT_KEYS = ('ordinal name source_sequence command recording_epoch selected_heap selected_index '
 'object_calls getter_calls typed_calls reset_call bind_call root0_call close_call seal_call copy_call '
 'execute_call signal_call event_call wait_call completed_call map_call unmap_call readback')
TEXTURE = dict(dimension=3, width=8, height=8, depth_or_array_size=1, mip_levels=1, format=28, sample_count=1, flags=0)
TYPED = ('CreateShaderResourceView', 'CopyDescriptorsSimple')


def _complete_budget(value, cap=MAX_OUTPUT):
    size = 0
    try:
        for chunk in json.JSONEncoder(ensure_ascii=True, allow_nan=False,
                                     separators=(',', ':')).iterencode(value):
            size += len(chunk.encode('ascii'))
            n.require(size <= cap, 'Complete report byte budget; no truncation')
    except (TypeError, UnicodeError, RecursionError) as error:
        raise ValueError('Bounded integer JSON report') from error
    return size


def _parse(data):
    if type(data) in (bytes, str):
        try:
            raw = data if type(data) is bytes else data.encode('utf8', 'strict')
            n.require(len(raw) <= MAX_INPUT, '512KiB original UTF8 input')
            data = json.loads(raw.decode('utf8', 'strict'), object_pairs_hook=n._pairs,
                              parse_constant=lambda _: n.require(False, 'Nonfinite JSON'))
        except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
            raise ValueError('Bounded strict UTF8 JSON') from error
    count = 0
    def detached(x, depth=0, key=''):
        nonlocal count
        count += 1
        n.require(count <= MAX_NODES and depth <= MAX_DEPTH, 'Outer node/depth budget')
        if x is None or type(x) is bool:
            return x
        if type(x) is int:
            n.require(-(1 << 31) <= x <= n.MAX_U64, 'Bounded exact JSON integer')
            return x
        if type(x) is str:
            try: size = len(x.encode('utf8', 'strict'))
            except UnicodeError as error: raise ValueError('Strict UTF8 string') from error
            cap = 32832 if key in ('before_hex', 'after_hex') else MAX_WIRE if key == 'outer_wire_text' else 2048 if key == 'path' else 256
            n.require(size <= cap, 'Field string byte budget')
            return x
        if type(x) is list:
            n.require(len(x) <= MAX_EDGES, 'Array count budget')
            return [detached(v, depth + 1) for v in x]
        if type(x) is dict:
            n.require(len(x) <= 64 and all(type(k) is str and len(k) <= 128 for k in x), 'Bounded exact keys')
            return {k: detached(v, depth + 1, k) for k, v in x.items()}
        raise ValueError('Only exact integer JSON types')
    try:
        value = detached(data)
        size = 0
        for chunk in json.JSONEncoder(ensure_ascii=False, allow_nan=False, separators=(',', ':')).iterencode(value):
            size += len(chunk.encode('utf8'))
            n.require(size <= MAX_INPUT, '512KiB entire detached input')
    except (UnicodeError, RecursionError, RuntimeError, TypeError) as error:
        raise ValueError('Bounded acyclic UTF8 JSON') from error
    return value


def _ref(value):
    n.obj(value, 'raw generation', 'APP occurrence reference')
    n.positive(value['raw'], 'APP raw interface'); n.positive(value['generation'], 'APP generation')
    return value['raw'], value['generation']


def _callref(value, calls, api=None, completed=True):
    n.obj(value, 'ordinal entry return', 'Original CallRef')
    n.positive(value['ordinal'], 'CallRef ordinal'); n.positive(value['entry'], 'CallRef ENTRY')
    n.uint(value['return'], 'CallRef RETURN')
    n.require(value['ordinal'] in calls, 'Original CallRef exists')
    c = calls[value['ordinal']]
    n.exact(value, dict(ordinal=c['ordinal'], entry=c['entry'], **{'return': c['return']}), 'Exact original CallRef')
    if completed: n.positive(c['return'], 'Original completed')
    if api: n.exact(c['api'], api, 'Original API')
    return c


def _cr(c): return dict(ordinal=c['ordinal'], entry=c['entry'], **{'return': c['return']})
def _out(c): return c['result']['output']
def _val(c): return c['result']['value']
def _before(a, b): n.require(a['return'] and a['return'] < b['entry'], 'Original ENTRY/RETURN order')
def _expect(c, args): n.exact(c['args'], args, 'Actual arguments retained by value')
def _null(c): n.exact(_out(c), None, 'Original HRESULT has no value output')
def _add(a, b):
    n.uint(a, 'CheckedU64 left'); n.uint(b, 'CheckedU64 right')
    n.require(a <= n.MAX_U64 - b, 'CheckedU64 addition')
    return a + b


def _separate(p, width, q, other_width, message):
    n.positive(p, message + ' left'); n.positive(q, message + ' right')
    # Compute BOTH ends before comparisons: short-circuit OR must never
    # hide overflow of the second original output object.
    end = _add(p, width); other_end = _add(q, other_width)
    n.require(end <= q or other_end <= p, message)


def _argument_types(api, a):
    for key in ('recording_epoch', 'gpu_handle', 'event', 'value', 'initial_value', 'initial_pipeline', 'base_offset'):
        if key in a: n.uint(a[key], 'Original U64 ' + key)
    for key in ('heap_type', 'count', 'node_mask', 'type', 'flags', 'parameter', 'timeout_ms', 'subresource', 'first_subresource', 'num_subresources'):
        if key in a: n.uint(a[key], 'Original UINT/enum ' + key, U32)
    for key in ('device', 'heap', 'resource', 'allocator', 'command', 'queue', 'fence'):
        if key in a: _ref(a[key])
    if api in ('CreateCommittedResource', 'GetCopyableFootprints'):
        d = a['description']; n.obj(d, 'dimension width height depth_or_array_size mip_levels format sample_count flags', 'Retained original resource description')
        for key, v in d.items(): n.uint(v, 'Original resource ' + key, n.MAX_U64 if key == 'width' else U32)
    elif api in ('CreateDescriptorHeap', 'CreateCommandQueue'):
        n.obj(a['description'], 'heap_type count flags node_mask' if api == 'CreateDescriptorHeap' else 'type priority flags node_mask', 'Original creation description')
        for key, v in a['description'].items():
            if key == 'priority': n.require(type(v) is int and -(1 << 31) <= v < (1 << 31), 'Signed priority')
            else: n.uint(v, 'Original description UINT', U32)
    elif api == 'CreateShaderResourceView':
        n.obj(a['description'], 'format view_dimension component_mapping most_detailed_mip mip_levels plane_slice min_lod_clamp_bits', 'Exact retained SRV bits')
        for v in a['description'].values(): n.uint(v, 'Original SRV UINT bits', U32)
    for side in ('source', 'destination'):
        if side in a:
            s = a[side]; n.obj(s, 'heap index cpu_handle', 'Original typed handle')
            _ref(s['heap']); n.uint(s['index'], 'Original index', U32); n.uint(s['cpu_handle'], 'Original CPUhandle')
    for key in ('heaps', 'commands'):
        if key in a:
            n.require(type(a[key]) is list and 1 <= len(a[key]) <= (2 if key == 'heaps' else 1), 'Finite original list')
            for r in a[key]: _ref(r)
    for key in ('range', 'written_range'):
        if key in a:
            if a[key] is None: continue
            n.require(type(a[key]) is list and len(a[key]) == 2, 'Original range')
            for v in a[key]: n.uint(v, 'Range scalar')
            n.require(a[key][0] <= a[key][1], 'Range order')
    if api == 'Release': n.require(a['object_kind'] in set(CREATES.values()), 'Original object class')
    if api == 'GetCopyableFootprints':
        fields = [('layout_output_pointer', 32), ('num_rows_output_pointer', 4), ('row_bytes_output_pointer', 8), ('total_bytes_output_pointer', 8)]
        for i, (key, width) in enumerate(fields):
            n.positive(a[key], 'Owned footprint output pointer')
            for other, other_width in fields[:i]: _separate(a[key], width, a[other], other_width, 'Owned footprint output objects disjoint')


def _control_types(api, a, result=None):
    n.obj(a, CONTROL_ARGS[api] + ' outputs_before', 'Exact original control arguments')
    for k, v in a.items():
        if k not in ('nonce_text', 'outputs_before'):
            n.uint(v, 'Control original scalar', U32 if k in ('abi_version', 'nonce_bytes', 'enabled', 'role', 'capacity') else n.MAX_U64)
    n.obj(a['outputs_before'], CONTROL_OUTPUTS[api], 'Caller scalar output values BEFORE')
    for k, v in a['outputs_before'].items(): n.uint(v, 'BEFORE scalar output', U32 if k in ('bytes', 'written', 'required') else n.MAX_U64)
    if api == 'D4VR_TypedCopyBeginV1': n.nonce(a['nonce_text'])
    if result is not None:
        n.obj(result, 'status outputs_after', 'Original control status/output')
        n.uint(result['status'], 'Actual control statusU32', 6)
        n.obj(result['outputs_after'], CONTROL_OUTPUTS[api], 'Caller scalar output values AFTER')
        for k, v in result['outputs_after'].items(): n.uint(v, 'AFTER scalar output', U32 if k in ('bytes', 'written', 'required') else n.MAX_U64)


def _journal(value):
    n.obj(value, 'clock_domain scope events', 'APP original/control journal')
    n.exact(value['clock_domain'], 'independent_APP_original_transition', 'Separate APP clock')
    n.exact(value['scope'], 'declared_owned_two_case_original_and_control_subset', 'Declared original subset')
    rows = value['events']; n.require(type(rows) is list and 0 < len(rows) <= MAX_EDGES, 'Bounded APP edges')
    calls = {}; pending = set(); last_transition = last_id = 0
    for r in rows:
        n.obj(r, 'ordinal transition api edge case_ordinal return_kind args original_returned result', 'Exact original edge')
        n.positive(r['ordinal'], 'Original ordinal'); n.positive(r['transition'], 'APP transition')
        n.require(r['transition'] > last_transition, 'Increasing unique original transition'); last_transition = r['transition']
        n.uint(r['case_ordinal'], 'Case ordinal', 2)
        api = r['api']; n.require(api in ARGS or api in CONTROL_ARGS, 'Closed original API set')
        control = api in CONTROL_ARGS
        n.exact(r['return_kind'], 'control_status_u32' if control else RETURNS[api], 'Actual return domain')
        if control: _control_types(api, r['args'])
        else:
            n.obj(r['args'], ARGS[api], 'Original retained arguments'); _argument_types(api, r['args'])
        if r['edge'] == 'before':
            n.require(r['ordinal'] > last_id and r['ordinal'] not in calls, 'Unique increasing original ENTRYordinal'); last_id = r['ordinal']
            n.exact(r['original_returned'], False, 'Original pending BEFORE'); n.exact(r['result'], None, 'No result before original')
            calls[last_id] = dict(ordinal=last_id, entry=r['transition'], **{'return': 0}, api=api, case=r['case_ordinal'], kind=r['return_kind'], args=r['args'], result=None)
            pending.add(last_id)
        else:
            n.exact(r['edge'], 'after', 'Original edgekind'); n.require(r['ordinal'] in pending, 'One exact completion')
            c = calls[r['ordinal']]
            for a, b in (('api', 'api'), ('case_ordinal', 'case'), ('return_kind', 'kind'), ('args', 'args')): n.exact(r[a], c[b], 'BEFORE frozen by value')
            n.exact(r['original_returned'], True, 'Original returned'); result = r['result']
            if control: _control_types(api, c['args'], result)
            elif c['kind'] == 'void': n.exact(result, None, 'Void original resultnull')
            elif c['kind'] == 'HRESULT':
                n.obj(result, 'hresult output', 'Original HRESULT/output')
                n.require(type(result['hresult']) is int and -(1 << 31) <= result['hresult'] < (1 << 31), 'Signed original HRESULT')
                if result['hresult'] >= 0:
                    if api in CREATES: _ref(result['output'])
                    elif api == 'Map':
                        n.obj(result['output'], 'pointer', 'Successful Map output'); n.positive(result['output']['pointer'], 'Map pointer')
                    else: n.exact(result['output'], None, 'No original value output')
            else:
                n.obj(result, 'value', 'Original scalar projection'); n.uint(result['value'], 'Actual scalar', U32 if c['kind'] == 'uint32' else n.MAX_U64)
            c['return'] = r['transition']; c['result'] = result; pending.remove(c['ordinal'])
    n.require(len(calls) <= MAX_CALLS, 'Bounded original occurrences')
    return calls, pending


def _hex(text, size):
    n.require(type(text) is str and len(text) == size * 2 and re.fullmatch('[0-9a-f]*', text) is not None, 'Exact lowerhex owned byte span')
    return bytes.fromhex(text)


def _controls(value, calls):
    controls = [c for c in calls.values() if c['api'] in CONTROL_ARGS]
    if value['mode'] == 'off':
        n.exact(value['control_binding'], None, 'CompiledOFF missing controls')
        n.exact(controls, [], 'No fabricated call to missing OFF export')
        n.exact(value['source_receipts'], [], 'OFF has no source receipt'); n.exact(value['copy_buffers'], [], 'OFF has no Copy buffer')
        return dict(ready=False, run_token=0, enabled=False, aliases={}, copies={}, seals={}, reasons=['compiled_OFF_source_unavailable'])
    binding = value['control_binding']; n.obj(binding, 'module_role exports begin_call expect_calls retains drops', 'ON controls binding')
    n.exact(binding['module_role'], 'instrumented_d3d12core', 'Selected control module role')
    exports = binding['exports']; n.require(type(exports) is list and len(exports) == 6, 'Six original symbols')
    names = set()
    for e in exports:
        n.obj(e, 'name address', 'Exact resolved symbol'); n.require(e['name'] in CONTROL_ARGS and e['name'] not in names, 'Unique named original export'); names.add(e['name']); n.positive(e['address'], 'Actual function pointer')
    n.exact(names, set(CONTROL_ARGS), 'Complete controls')
    begin = _callref(binding['begin_call'], calls, 'D4VR_TypedCopyBeginV1', completed=False)
    n.exact([c['ordinal'] for c in controls if c['api'] == begin['api']], [begin['ordinal']], 'One original Begin')
    a = begin['args']; n.exact(a['abi_version'], 1, 'Control ABI1'); n.exact(a['nonce_bytes'], 32, 'Nonce32'); n.exact(a['nonce_text'], value['run_id'], 'Original nonce bytes')
    for k in ('nonce_pointer', 'run_output_pointer'): n.positive(a[k], 'Owned Begin pointer')
    n.uint(a['enabled'], 'Actual enabled', 1)
    _separate(a['nonce_pointer'], 32, a['run_output_pointer'], 8, 'Begin initializes output before nonce check; owned spans disjoint')
    expect_refs = binding['expect_calls']; n.require(type(expect_refs) is list and len(expect_refs) == 5, 'Five actual Expect controls')
    expects = [_callref(r, calls, 'D4VR_TypedCopyExpectV1', completed=False) for r in expect_refs]
    n.exact([c['ordinal'] for c in controls if c['api'] == 'D4VR_TypedCopyExpectV1'], [c['ordinal'] for c in expects], 'No omitted Expect')
    for key, api in (('retains', 'D4VR_TypedCopyReceiptRetainV1'), ('drops', 'D4VR_TypedCopyReceiptDropV1')):
        n.require(type(binding[key]) is list and len(binding[key]) <= MAX_CALLS, 'Bounded owner controls')
        n.exact([c['ordinal'] for c in controls if c['api'] == api], [_callref(r, calls, api, completed=False)['ordinal'] for r in binding[key]], 'All ownership calls retained')
    copies = [c for c in controls if c['api'] == 'D4VR_TypedCopyReceiptCopyV1']
    n.require(len(copies) <= MAX_COPIES, 'All Copy calls, includingNULL/negative, count against4')
    observations = value['copy_buffers']; n.require(type(observations) is list and len(observations) <= MAX_COPIES, 'Owned Copy snapshots')
    buffers = {}; spans = []; identities = set()
    for b in observations:
        n.obj(b, 'call buffer_id buffer_generation base_pointer allocation_bytes output_offset capacity before_hex after_hex retained_until_final_snapshot', 'Owned Copy BEFORE/AFTER')
        c = _callref(b['call'], calls, 'D4VR_TypedCopyReceiptCopyV1', completed=False)
        n.require(c['ordinal'] not in buffers and c['args']['output_pointer'] != 0, 'Exactly one real NONNULL Copy span')
        for k in ('buffer_id', 'buffer_generation', 'base_pointer'): n.positive(b[k], 'Caller-owned buffer identity/span')
        ident = b['buffer_id'], b['buffer_generation']; n.require(ident not in identities, 'Unique held buffer occurrence'); identities.add(ident)
        n.exact(b['output_offset'], 16, 'Owned16Bprefix'); n.uint(b['capacity'], 'Original capacity', MAX_WIRE)
        n.exact(c['args']['capacity'], b['capacity'], 'Actual Copy capacity'); n.exact(b['allocation_bytes'], b['capacity'] + 32, 'Full guarded allocation')
        n.exact(c['args']['output_pointer'], _add(b['base_pointer'], 16), 'Exact owned output span')
        end = _add(b['base_pointer'], b['allocation_bytes'])
        n.require(all(end <= lo or hi <= b['base_pointer'] for lo, hi in spans), 'Retained owned buffers do not overlap'); spans.append((b['base_pointer'], end))
        before = _hex(b['before_hex'], b['allocation_bytes']); after = _hex(b['after_hex'], b['allocation_bytes'])
        n.exact(before, b'\xc3' * 16 + b'\xa5' * b['capacity'] + b'\x3c' * 16, 'Original initialized owned buffer')
        n.exact(b['retained_until_final_snapshot'], True, 'Held original output storage declaration')
        p = c['args']['written_pointer']; q = c['args']['required_pointer']
        n.positive(p, 'Owned written output'); n.positive(q, 'Owned required output')
        pe = _add(p, 4); qe = _add(q, 4)
        n.require((pe <= q or qe <= p) and (pe <= b['base_pointer'] or end <= p) and (qe <= b['base_pointer'] or end <= q), 'Output span and scalar slots disjoint')
        buffers[c['ordinal']] = (b, before, after)
    n.exact(set(buffers), {c['ordinal'] for c in copies if c['args']['output_pointer']}, 'Each NONNULL Copy has real guarded observation; NULL has none')
    run = 0; enabled = False; ready = True; aliases = {}; payloads = {}; ever_aliases = set(); seals = {}; copy_results = {}; last_return = 0; last_seq = 0; last_admission = 0; last_alias = 0; roles = set()
    for c in controls:
        if c['return'] == 0:
            ready = False
            continue
        # The finite owner protocol is serial; never choose an entry order for
        # overlapping state transitions whose actual linearization is unknown.
        n.require(c['entry'] > last_return, 'Serial control ENTRY/RETURN, no ambiguous overlap'); last_return = c['return']
        a = c['args']; result = c['result']; status = result['status']; out = result['outputs_after']; api = c['api']
        if api == 'D4VR_TypedCopyBeginV1':
            if status == 0:
                n.positive(out['run_token'], 'Original run token'); run = out['run_token']; enabled = bool(a['enabled'])
            else:
                n.exact(out['run_token'], 0, 'Failed Begin outputzero'); ready = False
            continue
        valid_run = run != 0 and a['run_token'] == run
        if api == 'D4VR_TypedCopyExpectV1':
            n.positive(a['admission_sequence'], 'Role sequence'); n.require(1 <= a['role'] <= 5, 'Finite source role')
            if status == 0:
                n.require(valid_run and enabled and a['role'] not in roles and a['admission_sequence'] > last_admission, 'One-shot role exactrun/order')
                roles.add(a['role']); last_admission = a['admission_sequence']
            else: ready = False
        elif api == 'D4VR_TypedCopySealV1':
            n.positive(a['sequence'], 'Requested source sequence')
            for k in ('owner_alias_output_pointer', 'bytes_output_pointer'): n.positive(a[k], 'Owned Seal output')
            _separate(a['owner_alias_output_pointer'], 8, a['bytes_output_pointer'], 4, 'Original Seal scalar outputs disjoint')
            if status == 0:
                n.require(valid_run and a['sequence'] > last_seq and len(payloads) < 4 and len(aliases) < 32, 'Actual successful Seal finite state')
                alias = out['owner_alias']; size = out['bytes']; n.positive(alias, 'Original newowner'); n.require(alias not in ever_aliases and alias > last_alias, 'Monotone original owner scalar'); last_alias = alias; n.require(0 < size <= MAX_WIRE, 'Exact original payloadsize')
                payload = dict(sequence=a['sequence'], size=size, seal_call=c['ordinal'])
                aliases[alias] = payload; ever_aliases.add(alias); payloads[a['sequence']] = payload; last_seq = a['sequence']; seals[c['ordinal']] = payload
            else:
                n.exact(out, dict(owner_alias=0, bytes=0), 'Failed Seal outputszero'); ready = False
        elif api == 'D4VR_TypedCopyReceiptRetainV1':
            n.positive(a['new_owner_output_pointer'], 'Owned Retain output'); _add(a['new_owner_output_pointer'], 8)
            owner = aliases.get(a['owner_alias']) if valid_run else None
            if status == 0:
                new = out['new_owner_alias']; n.positive(new, 'Distinct new ownership alias')
                n.require(owner is not None and new not in ever_aliases and len(aliases) < 32 and sum(v is owner for v in aliases.values()) < 8, 'Retain same sharedpayload withinquota')
                n.require(new > last_alias, 'Monotone Retain ownership alias'); last_alias = new
                aliases[new] = owner; ever_aliases.add(new)
            else:
                n.exact(out['new_owner_alias'], 0, 'Failed Retain outputzero')
                if owner is None: n.exact(status, 1, 'Badrun/alias Retain INVALID')
                ready = False
        elif api == 'D4VR_TypedCopyReceiptDropV1':
            owner = aliases.get(a['owner_alias']) if valid_run else None
            if status == 0:
                n.require(owner is not None, 'Drop actual live same-run owner'); del aliases[a['owner_alias']]
                if not any(v is owner for v in aliases.values()): payloads.pop(owner['sequence'])
            else:
                if owner is None: n.exact(status, 1, 'Staleowner Drop INVALID')
                ready = False
        else:
            cap = a['capacity']; n.uint(cap, 'Owned Copy capacity', MAX_WIRE)
            for k in ('written_pointer', 'required_pointer'): n.positive(a[k], 'Owned Copy scalar slot')
            _separate(a['written_pointer'], 4, a['required_pointer'], 4, 'Scalar output spans checked and disjoint')
            owner = aliases.get(a['owner_alias']) if valid_run else None
            raw = None
            if owner is None:
                n.exact(status, 1, 'Badrun/alias Copy INVALID'); n.exact(out, dict(written=0, required=0), 'Badrun/alias initializes0/0')
            elif status != 3:  # Exceptions are explicit unavailable, not a byte success.
                n.exact(out['required'], owner['size'], 'Actual live payload required size')
                expected = 5 if cap < owner['size'] else 1 if a['output_pointer'] == 0 else 0
                n.exact(status, expected, 'Original cap-before-null Copy result')
                n.exact(out['written'], owner['size'] if status == 0 else 0, 'Actual original written')
            if c['ordinal'] in buffers:
                b, before, after = buffers[c['ordinal']]
                if status == 0:
                    end = 16 + out['written']; n.require(out['written'] <= cap, 'Written fits actualspan')
                    n.exact(before[:16], after[:16], 'Prefix guard unchanged'); n.exact(before[end:], after[end:], 'All bytes outside written unchanged')
                    raw = after[16:end]
                elif status in (1, 5): n.exact(after, before, 'Negative Copy output allocation unchanged')
            elif status == 0: raise ValueError('NULL Copy cannot witness a payload')
            copy_results[c['ordinal']] = dict(call=_cr(c), sequence=owner['sequence'] if owner else None, owner_alias=a['owner_alias'], raw=raw, status=status)
            if status != 0: ready = False
    return dict(ready=ready and enabled, run_token=run, enabled=enabled, aliases=aliases, copies=copy_results, seals=seals,
                expect_calls=[_cr(c) for c in expects], reasons=[] if ready and enabled else ['control_or_source_unavailable'])


def _objects(calls):
    objects = {}; generations = {}; last_raw = {}
    for c in calls.values():
        if c['api'] in CREATES:
            kind = CREATES[c['api']]; r = _out(c); raw, generation = _ref(r)
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
            n.require(not o['release_entry'] and o['create']['return'] < c['entry'], 'Release of live original occurrence')
            # Source prototype marks releasing at ENTRY; even nonfinal Release
            # makes later selected eligibility unknown. This is not a COM
            # destruction claim or a rewrite of the original ULONG.
            o['release_entry'] = c['entry']
            if _val(c) == 0: o['release_return'] = c['return']
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
            states[_ref(_out(c))] = dict(epoch=0, state='recording')
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



def _footprint(context, calls, dev, readback_create):
    r = context['readback']
    n.obj(r, 'kind relative_file bytes width height DXGI_format footprint buffer_resource_width sha256', 'Detached readback file metadata')
    for k, expected in (('kind', 'owned_readback_file_metadata'), ('bytes', 147456), ('width', 256), ('height', 144), ('DXGI_format', 87)):
        n.exact(r[k], expected, 'Owned readback ' + k)
    n.require(type(r['relative_file']) is str and re.fullmatch('[A-Za-z0-9_-]+\\.bgra', r['relative_file']), 'Finite single relative rawfilename')
    n.sha(r['sha256']); n.uint(r['buffer_resource_width'], 'Original readback buffer Width', 1048576)
    f = r['footprint']; n.obj(f, 'call outputs_before outputs_after', 'Original footprint output observation')
    c = _callref(f['call'], calls, 'GetCopyableFootprints')
    target = dict(dimension=3, width=256, height=144, depth_or_array_size=1, mip_levels=1, format=87, sample_count=1, flags=1)
    a = c['args']; n.exact(a['device'], dev, 'Original footprint device')
    # The target resource flag is retained as an original declaration; format,
    # dimensions and sampling must be this exact closed footprint profile.
    for k, v in target.items():
        if k != 'flags': n.exact(a['description'][k], v, 'Original target footprint ' + k)
    for k, v in (('first_subresource', 0), ('num_subresources', 1), ('base_offset', 0)): n.exact(a[k], v, 'Single exact original footprint')
    for label in ('outputs_before', 'outputs_after'):
        out = f[label]; n.obj(out, 'layout num_rows row_bytes total_bytes', 'Owned original footprint output values')
        n.obj(out['layout'], 'offset format width height depth row_pitch', 'Original placed footprint')
        for k, v in out['layout'].items(): n.uint(v, 'Footprint scalar', n.MAX_U64 if k == 'offset' else U32)
        n.uint(out['num_rows'], 'Footprint UINT rows', U32)
        n.uint(out['row_bytes'], 'Footprint UINT64 rowbytes'); n.uint(out['total_bytes'], 'Footprint UINT64 total')
    out = f['outputs_after']; layout = out['layout']
    for k, v in (('offset', 0), ('format', 87), ('width', 256), ('height', 144), ('depth', 1)): n.exact(layout[k], v, 'Actual finite footprint')
    n.exact(out['num_rows'], 144, 'Actual original numrows'); n.exact(out['row_bytes'], 1024, 'Actual useful rowbytes')
    pitch = layout['row_pitch']; n.require(pitch >= 1024 and pitch % 256 == 0, 'Actual aligned rowpitch')
    n.require(pitch <= n.MAX_U64 // 143, 'Checked footprint multiply')
    minimum = _add(_add(layout['offset'], 143 * pitch), out['row_bytes'])
    n.require(minimum <= out['total_bytes'] <= r['buffer_resource_width'] <= 1048576, 'Exact useful footprint fits actual buffer; total need not be256multiple')
    d = dict(dimension=1, width=r['buffer_resource_width'], height=1, depth_or_array_size=1, mip_levels=1, format=0, sample_count=1, flags=0)
    _expect(readback_create, dict(device=dev, description=d)); _before(c, readback_create)
    return dict(call=_cr(c), minimum=minimum, width=r['buffer_resource_width'])


def _case_app(context, calls, alive, previous):
    n.obj(context, CONTEXT_KEYS, 'Exact case context')
    ordinal = context['ordinal']; n.require(type(ordinal) is int and ordinal in (1, 2), 'Two original cases')
    n.exact(context['name'], ('direct_A_clamp', 'after_bind_B_clamp')[ordinal - 1], 'Schedule name, never a pixel oracle')
    n.exact(context['source_sequence'], ordinal, 'Intended source cutoff1/2')
    n.exact(context['selected_index'], 0, 'Finite elementzero')
    n.obj(context['object_calls'], OBJECT_KEYS, 'APP original object CallRefs')
    ap = {}; creations = {}
    expected = dict(device='D3D12CreateDevice', resource_A='CreateCommittedResource', resource_B='CreateCommittedResource', cpu_heap='CreateDescriptorHeap', visible_heap='CreateDescriptorHeap', sampler_heap='CreateDescriptorHeap', allocator='CreateCommandAllocator', command='CreateCommandList', queue='CreateCommandQueue', fence='CreateFence', readback='CreateCommittedResource')
    for role, ref in context['object_calls'].items():
        if role == 'sampler_heap' and ref is None: continue
        c = _callref(ref, calls, expected[role]); ap[role] = _out(c); creations[role] = c
    dev = ap['device']; _expect(creations['device'], {})
    for role in ('resource_A', 'resource_B'): _expect(creations[role], dict(device=dev, description=TEXTURE))
    for role, d in (('cpu_heap', dict(heap_type=0, count=2, flags=0, node_mask=0)), ('visible_heap', dict(heap_type=0, count=1, flags=1, node_mask=0)), ('sampler_heap', dict(heap_type=1, count=2, flags=1, node_mask=0))):
        if role in ap: _expect(creations[role], dict(device=dev, description=d))
    _expect(creations['allocator'], dict(device=dev, type=0))
    _expect(creations['command'], dict(device=dev, allocator=ap['allocator'], node_mask=0, type=0, initial_pipeline=0))
    _expect(creations['queue'], dict(device=dev, description=dict(type=0, priority=0, flags=0, node_mask=0)))
    _expect(creations['fence'], dict(device=dev, initial_value=0, flags=0))
    footprint = _footprint(context, calls, dev, creations['readback'])
    n.exact(context['command'], ap['command'], 'Exact command occurrence'); n.exact(context['selected_heap'], ap['visible_heap'], 'Original selected SRVheap')
    n.positive(context['recording_epoch'], 'Actual recording epoch')
    cs = {}
    for key, api in (('reset_call', 'Reset'), ('bind_call', 'SetDescriptorHeaps'), ('root0_call', 'SetGraphicsRootDescriptorTable'), ('close_call', 'Close'), ('execute_call', 'ExecuteCommandLists'), ('signal_call', 'Signal'), ('event_call', 'SetEventOnCompletion'), ('wait_call', 'WaitForSingleObject'), ('completed_call', 'GetCompletedValue'), ('map_call', 'Map'), ('unmap_call', 'Unmap')):
        c = _callref(context[key], calls, api); n.exact(c['case'], ordinal, 'Original call belongs to this case'); cs[key] = c
    for key, api in (('seal_call', 'D4VR_TypedCopySealV1'), ('copy_call', 'D4VR_TypedCopyReceiptCopyV1')):
        if context[key] is not None:
            cs[key] = _callref(context[key], calls, api, completed=False); n.exact(cs[key]['case'], ordinal, 'Control belongs to case')
    ca = dict(command=ap['command'], recording_epoch=context['recording_epoch'])
    _expect(cs['reset_call'], dict(**ca, allocator=ap['allocator'], initial_pipeline=0)); _null(cs['reset_call'])
    heaps = [ap['visible_heap']] + ([ap['sampler_heap']] if 'sampler_heap' in ap else [])
    _expect(cs['bind_call'], dict(**ca, heaps=heaps)); _expect(cs['close_call'], ca); _null(cs['close_call'])
    _expect(cs['execute_call'], dict(queue=ap['queue'], commands=[ap['command']]))
    signal = cs['signal_call']; value = signal['args']['value']; n.positive(value, 'Actual Signal value')
    _expect(signal, dict(queue=ap['queue'], fence=ap['fence'], value=value)); _null(signal)
    event = cs['event_call']['args']['event']; n.positive(event, 'Original event')
    _expect(cs['event_call'], dict(fence=ap['fence'], value=value, event=event)); _null(cs['event_call'])
    _expect(cs['wait_call'], dict(event=event, timeout_ms=cs['wait_call']['args']['timeout_ms'])); n.exact(_val(cs['wait_call']), 0, 'WAIT_OBJECT_0')
    _expect(cs['completed_call'], dict(fence=ap['fence'])); n.require(value <= _val(cs['completed_call']) < n.MAX_U64, 'Completed actual value, not deviceRemoved')
    ma = cs['map_call']['args']; n.exact(ma['resource'], ap['readback'], 'Mapped actual readback'); n.exact(ma['subresource'], 0, 'Subresourcezero')
    if ma['range'] is not None: n.require(ma['range'][0] == 0 and footprint['minimum'] <= ma['range'][1] <= footprint['width'], 'Original CPU read range includes useful pixels')
    _expect(cs['unmap_call'], dict(resource=ap['readback'], subresource=0, written_range=[0, 0]))
    order = ['reset_call', 'bind_call', 'root0_call', 'close_call', 'execute_call', 'signal_call', 'event_call', 'wait_call', 'completed_call', 'map_call', 'unmap_call']
    for a, b in zip(order, order[1:]): _before(cs[a], cs[b])
    def last_command(api, cutoff, parameter=None):
        rows = [c for c in calls.values() if c['api'] == api and c['args'].get('command') == ap['command'] and c['return'] < cutoff and (parameter is None or c['args'].get('parameter') == parameter)]
        n.require(rows, 'Original recording operation exists'); return max(rows, key=lambda c: c['return'])
    for api, key, cut, parameter in (('Reset', 'reset_call', cs['execute_call']['entry'], None), ('Close', 'close_call', cs['execute_call']['entry'], None), ('SetDescriptorHeaps', 'bind_call', cs['close_call']['entry'], None), ('SetGraphicsRootDescriptorTable', 'root0_call', cs['close_call']['entry'], 0)):
        n.exact(last_command(api, cut, parameter)['ordinal'], cs[key]['ordinal'], 'Exact last original recording operation')
    regs = [c for c in calls.values() if c['api'] == 'SetEventOnCompletion' and c['args']['event'] == event and c['return'] < cs['wait_call']['entry']]
    n.require(regs and regs[-1]['ordinal'] == cs['event_call']['ordinal'], 'Wait exact last registration')
    executions = [c['ordinal'] for c in calls.values() if c['api'] == 'ExecuteCommandLists' and ap['command'] in c['args']['commands'] and cs['reset_call']['entry'] < c['entry'] < cs['map_call']['return']]
    n.exact(executions, [cs['execute_call']['ordinal']], 'One exact Execute for retained recording')
    signals = [c for c in calls.values() if c['api'] == 'Signal' and c['args']['fence'] == ap['fence'] and c['entry'] < cs['map_call']['return']]
    n.require(signals and signals[-1]['ordinal'] == signal['ordinal'], 'No omitted Signal before Map')
    if previous:
        n.exact(context['object_calls'], previous['object_calls'], 'Same original object occurrences')
        n.exact(context['getter_calls'], previous['getter_calls'], 'Same actual cached fourgetter CallRefs')
        n.exact(context['readback']['footprint'], previous['readback']['footprint'], 'One original setupfootprint byvalue')
        _before(_callref(previous['unmap_call'], calls), cs['reset_call'])
    n.obj(context['getter_calls'], 'increment cpu_heap visible_cpu visible_gpu', 'Four cached getters')
    gs = {}
    for role, api, args in (('increment', 'GetDescriptorHandleIncrementSize', dict(device=dev, heap_type=0)), ('cpu_heap', 'GetCPUDescriptorHandleForHeapStart', dict(heap=ap['cpu_heap'])), ('visible_cpu', 'GetCPUDescriptorHandleForHeapStart', dict(heap=ap['visible_heap'])), ('visible_gpu', 'GetGPUDescriptorHandleForHeapStart', dict(heap=ap['visible_heap']))):
        c = _callref(context['getter_calls'][role], calls, api); _expect(c, args); _before(c, cs['bind_call'])
        candidates = [x for x in calls.values() if x['api'] == api and x['args'] == args and x['return'] < cs['bind_call']['entry']]
        n.require(len(candidates) == 1, 'Unique actual cached getter, equalvalue not identity'); gs[role] = c
    n.require(len({c['ordinal'] for c in gs.values()}) == 4, 'Four distinct getter originals')
    stride, cpu, visible, gpu = [_val(gs[k]) for k in ('increment', 'cpu_heap', 'visible_cpu', 'visible_gpu')]
    for v in (stride, cpu, visible, gpu): n.positive(v, 'Observed eligible scalar')
    _add(cpu, 2 * stride); _add(visible, stride); _add(gpu, stride)
    _expect(cs['root0_call'], dict(**ca, parameter=0, gpu_handle=gpu))
    for h in ('cpu_heap', 'visible_heap'): alive(ap[h], 'heap', cs['bind_call']['entry'], cs['execute_call']['return'])
    n.require(type(context['typed_calls']) is list and len(context['typed_calls']) == (3 if ordinal == 1 else 4), 'Original schedules3then4')
    typed = [_callref(r, calls) for r in context['typed_calls']]
    n.require(all(c['api'] == 'CreateShaderResourceView' for c in typed[:3]), 'Three actual SRV originals')
    if ordinal == 2: n.exact(typed[3]['api'], 'CopyDescriptorsSimple', 'Actual lateCopySimple')
    for c in typed: n.exact(c['case'], ordinal, 'Typed original case')
    for a, b in zip(typed, typed[1:]): _before(a, b)
    _before(cs['reset_call'], typed[0]); _before(typed[2], cs['bind_call'])
    # Enforce the finite actual workload separately from the mathematical child.
    expected_resources = [ap['resource_A'], ap['resource_B'], ap['resource_A']]
    expected_slots = [(ap['cpu_heap'], 0, cpu), (ap['cpu_heap'], 1, _add(cpu, stride)), (ap['visible_heap'], 0, visible)]
    for c, resource, slot in zip(typed[:3], expected_resources, expected_slots):
        a = c['args']; n.exact(a['device'], dev, 'Original typed device'); n.exact(a['resource'], resource, 'Declared finite original resource')
        n.exact(a['destination'], dict(heap=slot[0], index=slot[1], cpu_handle=slot[2]), 'Exact original slot arithmetic')
    if ordinal == 2:
        _expect(typed[3], dict(device=dev, count=1, heap_type=0, source=dict(heap=ap['cpu_heap'], index=1, cpu_handle=_add(cpu, stride)), destination=dict(heap=ap['visible_heap'], index=0, cpu_handle=visible)))
        _before(cs['close_call'], typed[3])
    for c in typed:
        _before(gs['increment'], c)
        for side in ('source', 'destination'):
            if side in c['args']:
                h = c['args'][side]['heap']; role = 'cpu_heap' if h == ap['cpu_heap'] else 'visible_cpu' if h == ap['visible_heap'] else None
                n.require(role is not None, 'Typed selected heap'); _before(gs[role], c)
    all_typed = [c['ordinal'] for c in calls.values() if c['api'] in TYPED and c['case'] == ordinal and c['entry'] < cs['execute_call']['entry']]
    n.exact(all_typed, [c['ordinal'] for c in typed], 'No omitted actual original typed call')
    for key in ('seal_call', 'copy_call'):
        if key in cs:
            if cs[key]['return']:
                _before(cs['close_call'], cs[key]); _before(typed[-1], cs[key]); _before(cs[key], cs['execute_call'])
    if 'seal_call' in cs and 'copy_call' in cs and cs['seal_call']['return'] and cs['copy_call']['return']: _before(cs['seal_call'], cs['copy_call'])
    return dict(context=context, ap=ap, creations=creations, calls=cs, getters=gs, typed=typed, visible=visible)


def _source_join(app, outer, audit, calls, all_app_typed):
    wire = outer['getter_wire']; ga = audit['getter_audit']; context = app['context']; cs = app['calls']; ap = app['ap']; dev = ap['device']
    snapshot = wire['native_wire']['snapshot']; n.require(snapshot is not None, 'Actual known source snapshot')
    source_map = {}; mappings = []; mapped_edges = []
    def mapped(source, original):
        mapped_edges.extend(((source['entry'], original['entry']), (source['return'], original['return'])))
    for role, kind in (('device', 'device'), ('resource_A', 'resource'), ('resource_B', 'resource'), ('cpu_heap', 'heap'), ('visible_heap', 'heap')):
        raw, _ = _ref(ap[role]); pool = [snapshot['device']] if kind == 'device' else snapshot['resources'] if kind == 'resource' else snapshot['heaps']
        matches = [o for o in pool if o['raw'] == raw]; n.require(len(matches) == 1, 'Unique same-run original source creation')
        o = matches[0]; sr = dict(raw=o['raw'], generation=o['generation']); source_map[(kind,) + _ref(sr)] = ap[role]
        n.require(o['create_return'] and o['create_return'] < wire['cutoff']['transition'] and (not o['release_entry'] or wire['cutoff']['transition'] < o['release_entry']), 'Source occurrence live at sealed cutoff')
        creation = app['creations'][role]
        # A healthy SOURCE receipt cannot omit ANY selected Release ENTRY
        # before this original Seal, including a nonfinal ULONG return.
        for released in calls.values():
            if released['api'] == 'Release' and released['args']['object_kind'] == kind and released['args']['object'] == ap[role]:
                n.require(released['entry'] > cs['seal_call']['return'], 'Selected original Release before sealed cutoff is unavailable')
        desc = None if kind == 'device' else o['description'] if kind == 'resource' else {k:o[k] for k in ('heap_type', 'count', 'flags', 'node_mask')}
        if desc is not None: n.exact(desc, creation['args']['description'], 'Original retained description')
        candidates = [c['ordinal'] for c in calls.values() if CREATES.get(c['api']) == kind and c['return'] < cs['seal_call']['entry'] and _out(c)['raw'] == raw and (kind == 'device' or (c['args']['device'] == dev and c['args']['description'] == desc))]
        n.exact(candidates, [creation['ordinal']], 'Unique historical APP creation candidate; rawreuse not numericlabel choice')
        if kind != 'device': mapped(dict(entry=o['create_entry'], **{'return':o['create_return']}), creation)
        mappings.append(dict(role=role, app=ap[role], source=sr))
    def translate(ref, kind):
        key = (kind,) + _ref(ref); n.require(key in source_map, 'Exact source/APP mapping'); return source_map[key]
    heap = next(o for o in snapshot['heaps'] if translate(dict(raw=o['raw'], generation=o['generation']), 'heap') == ap['visible_heap'])
    n.exact(snapshot['selected_destination'], dict(heap=dict(raw=heap['raw'], generation=heap['generation']), index=0, cpu_handle=app['visible']), 'Original selected source slotzero')
    records = wire['getters']['records']; n.require(len(records) == 4, 'Four unique source getter occurrences')
    links = {}
    for role, kind, obj in (('increment','increment',ap['device']), ('cpu_heap','cpu_start',ap['cpu_heap']), ('visible_cpu','cpu_start',ap['visible_heap']), ('visible_gpu','gpu_start',ap['visible_heap'])):
        matches = [g for g in records if g['kind'] == kind and translate(g['source_object'], 'device' if kind == 'increment' else 'heap') == obj]
        n.require(len(matches) == 1, 'Unique original source getter, no equalvalue heuristic'); g = matches[0]
        n.require(g['value_known'] and g['lifetime_valid_through_return'] and g['arithmetic_eligible'] and g['return'] < wire['cutoff']['transition'], 'Eligible original getter')
        n.exact(g['value'], _val(app['getters'][role]), 'Exact returned scalar, no hiddenoutputpointer equality')
        n.exact(translate(g['parent_device'], 'device'), dev, 'Original getter parent')
        mapped(g, app['getters'][role]); links[role] = dict(app=_cr(app['getters'][role]), source_getter_id=g['id'])
    n.require(len(wire['publications']) == 2, 'Actual two heap publications')
    for role in ('cpu_heap', 'visible_heap'):
        pubs = [p for p in wire['publications'] if translate(p['heap'], 'heap') == ap[role]]; n.require(len(pubs) == 1, 'Unique original publication'); p = pubs[0]
        n.require(p['heap_facts_called'] and p['published'] and p['span_arithmetic_valid'], 'Eligible SOURCE observer publication')
        n.exact(p['cpu_getter_id'], links['cpu_heap' if role == 'cpu_heap' else 'visible_cpu']['source_getter_id'], 'Exact saved CPU getter')
        n.exact(p['increment_getter_id'], links['increment']['source_getter_id'], 'Exact saved increment')
    source_events = snapshot['events']
    n.exact(len(source_events), len(all_app_typed), 'All source typed occurrences join complete original prefix')
    typed_links = []
    for event, original in zip(source_events, all_app_typed):
        b = deepcopy(event['before']); b['device'] = translate(b['device'], 'device')
        if 'resource' in b: b['resource'] = translate(b['resource'], 'resource')
        for side in ('source', 'destination'):
            if side in b: b[side]['heap'] = translate(b[side]['heap'], 'heap')
        n.exact(event['api'], original['api'], 'Same original API'); n.exact(b, original['args'], 'Original typed BEFORE byvalue')
        n.exact(event['phase_before']['case_ordinal'], original['case'], 'Same finite window, not exclusive thread')
        n.require(event['return'] and event['return'] < wire['cutoff']['transition'], 'Original typed return before sealed cutoff')
        mapped(event, original); typed_links.append(dict(app=_cr(original), source_call_id=event['call_id']))
    ordered = sorted(mapped_edges)
    n.require(len({x[0] for x in ordered}) == len(ordered) and all(a[1] < b[1] for a,b in zip(ordered,ordered[1:])), 'Exact selected ENTRY/RETURN order embedding, never clock equality')
    selected = ga['native_model_replay']['history']['selected_version']['origin']['resource']
    return dict(occurrence_map=mappings, getter_links=links, typed_links=typed_links, historical_origin_app_resource=translate(selected, 'resource'))


def _analyze(data):
    """Analyze two supplied original cases; no I/O or execution/attestation."""
    value = _parse(data)
    n.obj(value, 'schema_version purpose run_id mode facts_origin control_binding source_receipts copy_buffers app_journal cases claims', 'New OUTER/control/APP inputs')
    n.exact(value['schema_version'], 1, 'Envelope schema1'); n.exact(value['purpose'], 'supplied_private_outer_control_APP_inputs', 'New exact-outer purpose'); n.nonce(value['run_id'])
    n.require(value['mode'] in ('off', 'on'), 'Explicit compiled mode'); n.require(value['facts_origin'] in ('supplied_CPU_mock', 'supplied_owned_PE_original'), 'Declared supplied origin, never a positive predicate')
    n.obj(value['claims'], ' '.join(CLAIMS), 'All stronger claims explicitfalse')
    for k in CLAIMS: n.exact(value['claims'][k], False, k)
    n.require(type(value['source_receipts']) is list and len(value['source_receipts']) <= 2, 'At mosttwo original primarypayloads')
    n.require(type(value['cases']) is list and len(value['cases']) == 2, 'Exactly two actual contexts')
    calls, pending = _journal(value['app_journal'])
    controls = _controls(value, calls)
    receipt_reports = []; by_sequence = {}; seen_copies = set()
    for r in value['source_receipts']:
        n.obj(r, 'source_sequence seal_call primary_copy_call repeat_copy_calls owner_alias outer_wire_text outer_sha256 outer_bytes', 'Original OUTER receipt')
        seq = r['source_sequence']; n.require(type(seq) is int and seq in (1, 2) and seq not in by_sequence, 'Distinct intended cutoff1/2')
        seal = _callref(r['seal_call'], calls, 'D4VR_TypedCopySealV1'); copy = _callref(r['primary_copy_call'], calls, 'D4VR_TypedCopyReceiptCopyV1')
        _before(seal, copy); n.require(seal['ordinal'] in controls['seals'], 'Actual successful original Seal')
        p = controls['seals'][seal['ordinal']]; n.exact(seq, p['sequence'], 'Original sealed sequence')
        n.positive(r['owner_alias'], 'Actual copied ownership alias'); n.exact(copy['args']['owner_alias'], r['owner_alias'], 'Original owner used by Copy')
        n.require(type(r['repeat_copy_calls']) is list and len(r['repeat_copy_calls']) <= 2, 'At mosttwo explicit rechecks')
        all_copies = [copy] + [_callref(x, calls, 'D4VR_TypedCopyReceiptCopyV1') for x in r['repeat_copy_calls']]
        raw = None
        for c in all_copies:
            n.require(c['ordinal'] not in seen_copies, 'Each original Copy selected once'); seen_copies.add(c['ordinal'])
            co = controls['copies'].get(c['ordinal']); n.require(co is not None and co['status'] == 0 and co['raw'] is not None, 'Actual successful NONNULL original copy')
            n.exact(co['sequence'], seq, 'Same live payload during actual Copy'); n.exact(len(co['raw']), p['size'], 'Exact copiedsize')
            if raw is None: raw = co['raw']
            else: n.exact(co['raw'], raw, 'Original repeated Copy bytes unchanged')
        n.exact(r['outer_bytes'], len(raw), 'Original exact written bytes'); n.sha(r['outer_sha256']); n.exact(hashlib.sha256(raw).hexdigest(), r['outer_sha256'], 'SHA of original written OUTER bytes')
        n.require(type(r['outer_wire_text']) is str, 'Original OUTER UTF8text')
        try: n.exact(r['outer_wire_text'].encode('utf8', 'strict'), raw, 'EXACT UTF8 written bytes, no stripping/canonicalization')
        except UnicodeError as error: raise ValueError('Original strict UTF8 outer') from error
        outer = n.parse(raw, MAX_WIRE)
        audit = audit_private_source_wire(raw)  # ONCE. Whole result, unchanged.
        n.exact(audit['run_id'], value['run_id'], 'Original source/APP same supplied nonce'); n.exact(audit['sequence'], seq, 'Original source/control sequence')
        entry = dict(source_sequence=seq, outer_sha256=r['outer_sha256'], outer_bytes=len(raw), original=r, private_control_audit=audit)
        by_sequence[seq] = (r, outer, audit); receipt_reports.append(entry)
    n.exact(seen_copies, {k for k,v in controls['copies'].items() if v['status'] == 0}, 'No unreferenced successful Copy payload')
    if set(by_sequence) == {1, 2}:
        a, b = by_sequence[1][1]['getter_wire'], by_sequence[2][1]['getter_wire']
        n.exact(a['native_wire']['producer']['epoch'], b['native_wire']['producer']['epoch'], 'One genuine SOURCEepoch')
        if by_sequence[1][2]['private_source_history_known'] and by_sequence[2][2]['private_source_history_known']:
            n.exact(a['getters'], b['getters'], 'Same immutable four getter occurrences'); n.exact(a['publications'], b['publications'], 'Same original publications')
        x, y = a['native_wire']['snapshot'], b['native_wire']['snapshot']
        if x is not None and y is not None:
            for k in ('device', 'resources', 'heaps'): n.exact(x[k], y[k], 'Immutable source creations acrosscutoffs')
            n.exact(y['events'][:len(x['events'])], x['events'], 'Old typed observations immutable prefix')
    # A failed or pending actual APP call is explicit unavailable, never an
    # invented successful handle/lifetime. Structural edge checks still run.
    app_unavailable = bool(pending) or any(c['kind'] == 'HRESULT' and c['result']['hresult'] < 0 for c in calls.values() if c['return'])
    applications = []
    if not app_unavailable:
        objects, alive = _objects(calls); _recordings(calls)
        fence_values = {}
        for c in calls.values():
            if c['api'] == 'Signal':
                key = _ref(c['args']['fence']); v = c['args']['value']; n.positive(v, 'Original positive Signal')
                obj = alive(c['args']['fence'], 'fence', c['entry'], c['return'])
                n.exact(obj['create']['args']['initial_value'], 0, 'Original fenceinitialzero'); n.exact(obj['create']['args']['flags'], 0, 'Original fenceflagszero')
                n.exact(obj['create']['args']['device'], alive(c['args']['queue'], 'queue', c['entry'], c['return'])['create']['args']['device'], 'Queue/fence same originaldevice')
                n.require(v > fence_values.get(key, 0), 'All actual Signals strictlyincrease fromzero'); fence_values[key] = v
            elif c['api'] == 'GetCompletedValue': n.require(_val(c) < n.MAX_U64, 'No deviceRemoved completed sentinel')
        for i, context in enumerate(value['cases']):
            n.exact(context['ordinal'], i + 1, 'Ordered contexts')
            applications.append(_case_app(context, calls, alive, value['cases'][i-1] if i else None))
    else:
        for i, context in enumerate(value['cases']):
            n.obj(context, CONTEXT_KEYS, 'Negative context retains exact framing'); n.exact(context['ordinal'], i+1, 'Ordered negative contexts')
    reports = []
    for i, context in enumerate(value['cases'], 1):
        reason = []; details = {}; child = by_sequence.get(i, (None,None,None))[2]
        known = not app_unavailable and child is not None and child['private_source_history_known'] and controls['ready']
        if value['mode'] == 'off':
            n.exact(context['seal_call'], None, 'OFF no inventedSeal'); n.exact(context['copy_call'], None, 'OFF no inventedCopy'); reason.append('compiled_OFF_source_unavailable')
        if app_unavailable: reason.append('original_APP_call_failed_or_pending')
        if child is None or not child['private_source_history_known']: reason.append('supplied_private_source_history_unavailable')
        if not controls['ready']: reason.append('control_or_source_unavailable')
        if known:
            r, outer, audit = by_sequence[i]; app = applications[i-1]
            n.exact(context['seal_call'], r['seal_call'], 'Same original Seal context'); n.exact(context['copy_call'], r['primary_copy_call'], 'Same original Copy context')
            seal = app['calls']['seal_call']; n.exact(seal['args']['sequence'], i, 'Original sourcecutoff')
            _before(seal, app['calls']['copy_call']); _before(app['calls']['copy_call'], app['calls']['execute_call'])
            last_creation = None
            begin_call = _callref(value['control_binding']['begin_call'], calls, 'D4VR_TypedCopyBeginV1')
            for role_index, role in enumerate(('device','resource_A','resource_B','cpu_heap','visible_heap'),1):
                expect = _callref(controls['expect_calls'][role_index-1], calls, 'D4VR_TypedCopyExpectV1')
                n.exact(expect['args']['role'], role_index, 'Exact selected role'); _before(begin_call, expect); _before(expect, app['creations'][role])
                if last_creation is not None: _before(last_creation, expect)
                last_creation = app['creations'][role]
            prefix = [c for c in calls.values() if c['api'] in TYPED and c['entry'] < seal['entry']]
            details = _source_join(app, outer, audit, calls, prefix)
        reports.append(dict(ordinal=i,name=context['name'],supplied_outer_copy_and_APP_argument_relation_known=known,
            reasons=list(dict.fromkeys(reason)),context=context,private_control_audit=child,
            occurrence_map=details.get('occurrence_map',[]),getter_links=details.get('getter_links',{}),
            typed_links=details.get('typed_links',[]),historical_origin_app_resource=details.get('historical_origin_app_resource'),
            readback_digest_recomputed_here=False,readback_bytes_origin_known=False))
    n.exact([r['source_sequence'] for r in value['source_receipts']], sorted(by_sequence), 'Primary receipts in sourcecutoff order')
    replay = dict(run_token=controls['run_token'],enabled=controls['enabled'],ready=controls['ready'],
        live_owners=[dict(owner_alias=k,source_sequence=v['sequence']) for k,v in sorted(controls['aliases'].items())],
        copies=[dict(call=v['call'],source_sequence=v['sequence'],owner_alias=v['owner_alias'],status=v['status'],original_outer_bytes=0 if v['raw'] is None else len(v['raw'])) for v in controls['copies'].values()])
    result = dict(schema_version=1,purpose='supplied_private_outer_control_APP_relation_audit',run_id=value['run_id'],mode=value['mode'],facts_origin=value['facts_origin'],
        supplied_outer_copy_and_APP_argument_relation_known=all(r['supplied_outer_copy_and_APP_argument_relation_known'] for r in reports),
        reasons=list(dict.fromkeys(x for r in reports for x in r['reasons'])),cases=reports,
        source_receipts=receipt_reports,app_journal=value['app_journal'],copy_buffers=value['copy_buffers'],control_replay=replay,
        readback_digest_recomputed_here=False,claims={k:False for k in CLAIMS})
    _complete_budget(result, MAX_OUTPUT)
    return result


def analyze(data):
    """Strict pure bounded API; malformed/contradictory facts raise ValueError."""
    try:
        return _analyze(data)
    except (KeyError, IndexError, StopIteration, TypeError, OverflowError, RecursionError, UnicodeError) as error:
        raise ValueError('Malformed bounded original-fact envelope') from error
