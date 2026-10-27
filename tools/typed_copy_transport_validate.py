"""Read-only audit of NEW finite native typed-copy receipts and owned joins.

The historical model is imported unchanged. A receipt is caller-supplied owned
metadata, not an attestation of application inputs, descriptor bytes, GPU reads,
or a repair of any old owner/backing gate. No process or driver is called here.
"""
from __future__ import annotations

import hashlib
import json
import re

import typed_descriptor_copy as history_model
from translated_descriptor_copy_validate import compare_pixels

MAX_U64 = (1 << 64) - 1
MAX_WIRE = 16384
MODEL_SHA256 = '7987ec83978a2909d023e93d66dde66a473154fef5a1767df8b036c5964c1953'
FALSE_FACTS = (
    'input_facts_attested', 'opaque_descriptor_decoded',
    'actual_source_contents_at_copy_proven', 'current_contents_at_draw_known',
    'unchanged_since_copy_proven', 'sampled_resource_GPU_read_proven',
    'generic_PE_to_Unix_resource_mapping', 'renderer_owner_known',
    'exclusive_phase_attribution_proven', 'global_API_absence_proven',
    'old_process_or_backing_gate_repaired',
)
NAMES = ('direct_A_clamp', 'after_bind_B_clamp')
MUTATION_APIS = ('CreateShaderResourceView', 'CopyDescriptorsSimple', 'CopyDescriptors')


def require(value, message):
    if not value:
        raise ValueError(message)


def exact(actual, expected, message):
    require(type(actual) is type(expected), message + ': exact type')
    if type(expected) is dict:
        require(set(actual) == set(expected), message + ': exact keys')
        for key in expected:
            exact(actual[key], expected[key], message + '.' + key)
    elif type(expected) is list:
        require(len(actual) == len(expected), message + ': exact array size')
        for a, b in zip(actual, expected):
            exact(a, b, message)
    else:
        require(actual == expected, message)


def obj(value, keys, message):
    require(type(value) is dict and set(value) == set(keys.split()), message + ': exact keys')


def uint(value, message, maximum=MAX_U64):
    require(type(value) is int and 0 <= value <= maximum, message + ': unsigned integer')


def positive(value, message):
    uint(value, message)
    require(value > 0, message + ': positive integer')


def boolean(value, message):
    require(type(value) is bool, message + ': boolean')


def nonce(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{32}', value), 'Run nonce')


def sha(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{64}', value), 'SHA256')


def _pairs(items):
    out = {}
    for key, value in items:
        require(key not in out, 'Duplicate JSON key')
        out[key] = value
    return out


def _json(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True, allow_nan=False)


def parse(data, cap=MAX_WIRE):
    if type(data) in (bytes, str):
        require(len(data) <= cap, 'Complete wire byte budget')
        try:
            if type(data) is bytes:
                data = data.decode('utf8', errors='strict')
            data = json.loads(data, object_pairs_hook=_pairs,
                              parse_constant=lambda _: require(False, 'Nonfinite JSON'))
        except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
            raise ValueError('Malformed bounded JSON') from error
    nodes = 0
    def bounded_copy(value, depth=0):
        nonlocal nodes
        nodes += 1
        require(nodes <= max(4096, cap // 8) and depth <= 24, 'Bounded JSON node/depth count')
        if value is None or type(value) is bool:
            return value
        if type(value) is int:
            require(-(1 << 31) <= value <= MAX_U64, 'Bounded integer JSON')
            return value
        if type(value) is str:
            require(len(value) <= 4096, 'Bounded JSON string')
            return value
        if type(value) is list:
            require(len(value) <= 768, 'Bounded JSON array')
            return [bounded_copy(x, depth + 1) for x in value]
        if type(value) is dict:
            require(len(value) <= 64 and all(type(k) is str and len(k) <= 128 for k in value), 'Bounded exact JSON keys')
            return {k: bounded_copy(v, depth + 1) for k, v in value.items()}
        raise ValueError('Only exact finite integer JSON types')
    try:
        data = bounded_copy(data)
        wire = _json(data)
    except (TypeError, ValueError, RecursionError, RuntimeError) as error:
        raise ValueError('Only finite bounded JSON') from error
    require(len(wire.encode('ascii')) <= cap, 'Complete wire byte budget')
    return json.loads(wire)


def _false(value):
    for key in FALSE_FACTS:
        exact(value[key], False, key + ' must remain unknown')


def audit_wire(data):
    """Validate the original native envelope; never normalize an old log."""
    value = parse(data)
    obj(value, 'schema_version purpose sequence run_id producer snapshot ' + ' '.join(FALSE_FACTS), 'Native envelope')
    exact(value['schema_version'], 1, 'Envelope schema')
    exact(value['purpose'], 'owned_original_typed_copy_transport', 'Envelope purpose')
    positive(value['sequence'], 'Capture sequence')
    _false(value)
    p = value['producer']
    obj(p, 'enabled faulted gap dropped clock epoch nonfinal_release_returns snapshot_available', 'Native producer')
    for key in ('enabled', 'faulted', 'gap', 'snapshot_available'):
        boolean(p[key], key)
    for key in ('dropped', 'clock', 'epoch', 'nonfinal_release_returns'):
        uint(p[key], key)
    require(p['epoch'] > 0, 'Producer epoch known')
    require(not p['faulted'] or p['gap'], 'Fault implies an explicit gap')
    require(not (p['dropped'] or p['nonfinal_release_returns']) or (p['faulted'] and p['gap']), 'Drops/nonfinal Release cannot become complete')
    if value['run_id'] == '':
        require(p['faulted'] and p['gap'] and not p['snapshot_available'], 'Invalid-run diagnostic only')
    else:
        nonce(value['run_id'])
    history = None
    if p['snapshot_available']:
        require(p['enabled'] and type(value['snapshot']) is dict, 'Available native input')
        s = value['snapshot']
        history = history_model.analyze(s)
        exact(s['run_id'], value['run_id'], 'Snapshot run')
        exact(s['at_transition'], p['clock'], 'Frozen snapshot cutoff')
        exact(s['complete'], not (p['faulted'] or p['gap']), 'Complete producer snapshot')
        exact(s['gap'], p['faulted'] or p['gap'], 'Producer snapshot gap')
        _false(history)
    else:
        exact(value['snapshot'], None, 'Absent native snapshot')
    known = bool(history is not None and history['selected_history_known'])
    require(not known or (p['enabled'] and not p['faulted'] and not p['gap'] and p['dropped'] == 0 and p['nonfinal_release_returns'] == 0), 'Complete historical relation producer')
    return dict(schema_version=1, purpose='new_native_typed_copy_wire_audit',
                run_id=value['run_id'], sequence=value['sequence'],
                snapshot_available=p['snapshot_available'], producer=p,
                selected_history_known=known, history=history,
                canonical_native_envelope_sha256=hashlib.sha256(_json(value).encode('ascii')).hexdigest(),
                **{key: False for key in FALSE_FACTS})


def audit_native_samples(data):
    """Bridge the actual CPU executable's emitted JSON to the frozen model."""
    value = parse(data, 1024 * 1024)
    obj(value, 'typed_copy_transport_cpu_passed cases assertions original_calls samples GPU_or_Wine_calls', 'Native CPU result')
    exact(value['typed_copy_transport_cpu_passed'], True, 'Native CPU checks')
    exact(value['GPU_or_Wine_calls'], 0, 'Native CPU scope')
    for key in ('cases', 'assertions', 'original_calls'):
        positive(value[key], key)
    require(type(value['samples']) is list and 1 <= len(value['samples']) <= 64, 'Finite emitted native samples')
    names, audits = set(), []
    for sample in value['samples']:
        obj(sample, 'name expected_known expected_origin wire', 'Native sample')
        require(type(sample['name']) is str and len(sample['name']) <= 80 and sample['name'] not in names, 'Unique sample name')
        names.add(sample['name'])
        boolean(sample['expected_known'], 'Separately authored expected history')
        audit = audit_wire(sample['wire'])
        exact(audit['selected_history_known'], sample['expected_known'], 'Native/model historical expectation ' + sample['name'])
        if sample['expected_known']:
            obj(sample['expected_origin'], 'raw generation', 'Original expected resource')
            for key in ('raw', 'generation'):
                positive(sample['expected_origin'][key], 'Expected resource ' + key)
            exact(audit['history']['selected_version']['origin']['resource'], sample['expected_origin'], 'Native original expected resource')
        else:
            exact(sample['expected_origin'], None, 'Unknown sample origin')
        audits.append(dict(name=sample['name'], snapshot_available=audit['snapshot_available'],
                           selected_history_known=audit['selected_history_known']))
    return dict(native_cases=value['cases'], native_assertions=value['assertions'],
                original_mock_calls=value['original_calls'], emitted_samples=len(audits),
                snapshots_consumed_by_frozen_model=sum(x['snapshot_available'] for x in audits),
                known_histories=sum(x['selected_history_known'] for x in audits), samples=audits,
                frozen_model_sha256=MODEL_SHA256, **{key: False for key in FALSE_FACTS})


def audit_original_journal(data, run_id):
    value = parse(data, 1024 * 1024)
    obj(value, 'schema_version purpose run_id owned_fixture before_after_original_D3D12_calls transition_domain original_transition_domain events', 'New original journal')
    exact(value['schema_version'], 1, 'Original journal schema')
    exact(value['purpose'], 'owned_typed_copy_original_call_journal', 'New journal purpose')
    exact(value['run_id'], run_id, 'Original journal nonce')
    exact(value['owned_fixture'], True, 'Owned original journal')
    exact(value['before_after_original_D3D12_calls'], True, 'Original edges')
    exact(value['transition_domain'], 'Session_common_transition', 'Transport clock label')
    exact(value['original_transition_domain'], 'independent_original_call_transition', 'Original clock label')
    rows = value['events']
    require(type(rows) is list and 0 < len(rows) <= 768 and len(rows) % 2 == 0, 'Finite complete original edges')
    calls, transitions = {}, set()
    last_native = 0
    for index, row in enumerate(rows):
        obj(row, 'ordinal original_transition transition run_id case_ordinal api_phase api edge returns returned mutation_call_id transport_call_id object_generation transport_generation hresult value args', 'Original edge')
        for key in ('ordinal', 'original_transition', 'transition', 'mutation_call_id', 'transport_call_id', 'object_generation', 'transport_generation'):
            uint(row[key], 'Original ' + key)
        positive(row['ordinal'], 'Original call ID')
        exact(row['original_transition'], index + 1, 'Original common CPU clock')
        if row['transition']:
            require(row['transition'] not in transitions, 'Unique native transition')
            require(row['transition'] > last_native, 'Monotone original/native occurrence mapping')
            last_native = row['transition']
            transitions.add(row['transition'])
        exact(row['run_id'], run_id, 'Original edge nonce')
        uint(row['case_ordinal'], 'Original case', 2)
        uint(row['api_phase'], 'Original phase', 6)
        require(type(row['api']) is str and 0 < len(row['api']) <= 80, 'Original API')
        require(row['returns'] in ('void', 'HRESULT', 'uint64', 'uint32'), 'Original return framing')
        require(row['edge'] in ('before', 'after'), 'Original edge kind')
        exact(row['returned'], row['edge'] == 'after', 'Original returned flag')
        require(type(row['args']) is dict, 'Frozen original arguments')
        if row['edge'] == 'before':
            exact(row['hresult'], None, 'No HRESULT before original')
            exact(row['value'], None, 'No result before original')
        elif row['returns'] == 'void':
            exact(row['hresult'], None, 'Original void has no HRESULT')
            exact(row['value'], None, 'Original void has no value')
        elif row['returns'] == 'HRESULT':
            require(type(row['hresult']) is int and -(1 << 31) <= row['hresult'] < (1 << 31), 'Original HRESULT')
        else:
            exact(row['hresult'], None, 'Original integer has no HRESULT')
            require(type(row['value']) is dict and len(row['value']) == 1, 'Original integer value')
            uint(next(iter(row['value'].values())), 'Original integer result', (1 << 32) - 1 if row['returns'] == 'uint32' else MAX_U64)
        pair = calls.setdefault(row['ordinal'], {})
        require(row['edge'] not in pair, 'No duplicate original edge')
        pair[row['edge']] = row
    for ordinal, pair in calls.items():
        require(set(pair) == {'before', 'after'}, 'No pending original journal call')
        before, after = pair['before'], pair['after']
        for key in ('ordinal', 'run_id', 'case_ordinal', 'api_phase', 'api', 'returns',
                    'mutation_call_id', 'transport_call_id', 'object_generation', 'transport_generation', 'args'):
            exact(after[key], before[key], 'Original frozen edge ' + key)
        require(after['original_transition'] > before['original_transition'], 'Original AFTER order')
        require((not before['transition'] and not after['transition']) or after['transition'] > before['transition'] > 0, 'Native AFTER order')
    typed = [p for p in calls.values() if p['before']['api'] in MUTATION_APIS]
    for index, pair in enumerate(typed, 1):
        exact(pair['before']['mutation_call_id'], index, 'Independent original typed-call sequence')
    for pair in calls.values():
        if pair['before']['api'] not in MUTATION_APIS:
            exact(pair['before']['mutation_call_id'], 0, 'No original typed ID on other API')
            exact(pair['before']['transport_call_id'], 0, 'No transport typed ID on other API')
    return calls


def _ref(value, label):
    obj(value, 'raw generation', label)
    for key in value:
        positive(value[key], label + '.' + key)


def _call(calls, reference, api, case, kind):
    obj(reference, 'ordinal entry return transport_entry transport_return', 'Original call reference')
    for key in reference:
        uint(reference[key], 'Call reference ' + key)
    require(reference['ordinal'] in calls, 'Referenced original call exists')
    pair = calls[reference['ordinal']]
    b, a = pair['before'], pair['after']
    exact(reference, dict(ordinal=b['ordinal'], entry=b['original_transition'],
                         **{'return': a['original_transition']},
                         transport_entry=b['transition'], transport_return=a['transition']), 'Original occurrence reference')
    exact(b['api'], api, 'Original referenced API')
    exact(b['case_ordinal'], case, 'Original referenced case')
    exact(b['returns'], kind, 'Original referenced return kind')
    if kind == 'HRESULT':
        exact(a['hresult'], 0, 'Owned original HRESULT')
    return pair


def _creation(calls, api, raw, generation, *, transport=False):
    positive(raw, 'Original creation raw output')
    positive(generation, 'Original creation generation')
    found = []
    key = 'transport_generation' if transport else 'object_generation'
    for pair in calls.values():
        b, a = pair['before'], pair['after']
        if b['api'] == api and type(a['value']) is dict and a['value'].get('raw') == raw and a['value'].get(key) == generation:
            found.append(pair)
    require(len(found) == 1, 'Unique original object creation: ' + api)
    pair = found[0]
    exact(pair['after']['value']['raw'], raw, 'Exact original creation raw output')
    exact(pair['before']['returns'], 'HRESULT', 'Original creation return')
    exact(pair['after']['hresult'], 0, 'Original creation success')
    exact(pair['after']['value'][key], generation, 'Exact original output generation')
    exact(pair['before'][key], generation, 'Original create ENTRY generation')
    return pair


def _join_snapshot(snapshot, calls, cutoff):
    """Exact original occurrence joins, no data imported from a later epoch."""
    for kind, api in (('device', 'D3D12CreateDevice'), ('resources', 'CreateCommittedResource'), ('heaps', 'CreateDescriptorHeap')):
        values = [snapshot[kind]] if kind == 'device' else snapshot[kind]
        for occurrence in values:
            pair = _creation(calls, api, occurrence['raw'], occurrence['generation'], transport=True)
            b, a = pair['before'], pair['after']
            exact(occurrence['create_entry'], b['transition'], 'Original create BEFORE')
            exact(occurrence['create_return'], a['transition'], 'Original create AFTER')
            exact(occurrence['create_hresult'], a['hresult'], 'Original create HRESULT')
            require(a['transition'] <= cutoff, 'Object existed at snapshot cutoff')
            exact(occurrence['release_entry'], 0, 'No release in selected pre-Execute snapshot')
            exact(occurrence['release_return'], 0, 'No release RETURN in selected snapshot')
            released = [p for p in calls.values() if p['before']['api'] == 'Release' and
                        p['before']['args'].get('object') == {'raw': occurrence['raw'], 'generation': occurrence['generation']} and
                        0 < p['before']['transition'] <= cutoff]
            require(not released, 'No original Release ENTRY omitted from snapshot')
            if kind == 'resources':
                exact(b['args']['tracked_texture'], True, 'Original tracked texture')
                exact(b['args']['device'], snapshot['device']['raw'], 'Original resource scalar device')
                exact(b['args']['device_ref'], occurrence['device'], 'Original resource device')
                exact({k: b['args']['description'][k] for k in occurrence['description']}, occurrence['description'], 'Original resource description')
            if kind == 'heaps':
                exact(b['args']['tracked_srv_heap'], True, 'Original tracked SRV heap')
                exact(b['args']['device'], snapshot['device']['raw'], 'Original heap scalar device')
                exact(b['args']['device_ref'], occurrence['device'], 'Original heap device')
                for key, original in (('heap_type', 'type'), ('count', 'count'), ('flags', 'flags'), ('node_mask', 'node_mask')):
                    exact(b['args'][original], occurrence[key], 'Original heap ' + key)
                expected_role = 'cpu_srv' if occurrence['role'] == 'cpu' else 'visible_srv'
                exact(b['args']['role'], expected_role, 'Original heap role')
                def getter(name, field, expected):
                    matches = [p for p in calls.values() if p['before']['api'] == name and
                               p['before']['args'].get('heap') == occurrence['raw'] and
                               p['before']['args'].get('generation') == a['value']['object_generation'] and
                               p['after']['transition'] <= cutoff]
                    require(len(matches) == 1, 'Unique original heap getter')
                    require(a['transition'] < matches[0]['before']['transition'] and
                            matches[0]['after']['transition'] < min(e['entry'] for e in snapshot['events']), 'Original getter before first typed mutation')
                    exact(matches[0]['after']['value'], {field: expected}, 'Original heap returned fact')
                getter('GetCPUDescriptorHandleForHeapStart', 'cpu_start', occurrence['cpu_start'])
                strides = [p for p in calls.values() if p['before']['api'] == 'GetDescriptorHandleIncrementSize' and
                           p['before']['args'] == {'device': snapshot['device']['raw'], 'type': 0} and
                           a['transition'] < p['before']['transition'] < cutoff]
                require(strides, 'Actual original descriptor increment')
                require(all(p['after']['value'] == {'increment': occurrence['increment']} for p in strides), 'All relevant original increment values')
    # Membership comes from the independently recorded ORIGINAL API, never
    # from a producer label which could erase an unretained mutation.
    original_mutations = [p for p in calls.values() if p['before']['api'] in MUTATION_APIS and p['before']['transition'] <= cutoff]
    exact(len(original_mutations), len(snapshot['events']), 'No omitted original typed mutation')
    for event in snapshot['events']:
        found = [p for p in original_mutations if p['before']['transport_call_id'] == event['call_id']]
        require(len(found) == 1, 'Unique original typed call')
        b, a = found[0]['before'], found[0]['after']
        exact(event['entry'], b['transition'], 'Original typed ENTRY')
        exact(event['return'], a['transition'], 'Original typed RETURN')
        exact(event['api'], b['api'], 'Original typed API')
        exact(event['return_kind'], b['returns'], 'Original typed return kind')
        exact(event['original_returned'], True, 'Original typed call returned')
        exact(event['result'], None, 'Original typed void result')
        for key, row in (('phase_before', b), ('phase_after', a)):
            exact(event[key], dict(case_ordinal=row['case_ordinal'], api_phase=row['api_phase']), 'Original typed phase')
        args = dict(b['args'])
        if event['api'] == 'CreateShaderResourceView':
            require(args.pop('role') in ('cpu_source_A', 'cpu_source_B', 'selected_visible_slot'), 'Owned original SRV role')
        exact(event['before'], args, 'Frozen original typed arguments')


def _audit_case(wire, original_events, context, raw=None, *, require_history=None):
    """Join a NEW owned receipt to its original recording and readback calls."""
    w = parse(wire)
    transport = audit_wire(w)
    c = parse(context, 16384)
    obj(c, 'schema_version purpose run_id case_ordinal name transport_enabled transport_capture_succeeded transport_clock_domain journal_clock_domain snapshot_cutoff transport_epoch command selected_destination visible_heap selected_mutation_call_id selected_mutation_call reset_call bind_call root0_call close_call execute_call signal_call event_call wait_call completed_call readback_call producer current_contents_at_draw_known sampled_resource_GPU_read_proven renderer_owner_known', 'New case context')
    exact(c['schema_version'], 1, 'Case schema')
    exact(c['purpose'], 'owned_typed_copy_case_context', 'New case context purpose')
    nonce(c['run_id'])
    require(type(c['name']) is str and c['name'] in NAMES, 'Only two new owned cases')
    case = NAMES.index(c['name']) + 1
    exact(c['case_ordinal'], case, 'Case ordinal')
    exact(c['run_id'], w['run_id'], 'Receipt/case run')
    exact(w['sequence'], case, 'Receipt/case sequence')
    exact(c['transport_clock_domain'], 'Session_common_transition', 'Case transport clock label')
    exact(c['journal_clock_domain'], 'independent_original_call_transition', 'Case original clock label')
    exact(c['transport_enabled'], w['producer']['enabled'], 'Same capture mode')
    exact(c['transport_capture_succeeded'], True, 'Original receipt was retained')
    exact(c['transport_epoch'], w['producer']['epoch'], 'Capture epoch')
    exact(c['snapshot_cutoff'], w['producer']['clock'], 'Capture cutoff')
    for key in ('current_contents_at_draw_known', 'sampled_resource_GPU_read_proven', 'renderer_owner_known'):
        exact(c[key], False, 'Case limit ' + key)
    if require_history is None:
        require_history = c['transport_enabled']
    boolean(require_history, 'Required history mode')
    if require_history:
        require(transport['selected_history_known'], 'Selected typed history must be known')
    calls = audit_original_journal(original_events, c['run_id'])
    for pair in calls.values():
        for row in pair.values():
            if c['transport_enabled']:
                positive(row['transition'], 'Active transport occurrence clock')
            else:
                exact(row['transition'], 0, 'Disabled original has no transport clock')
                exact(row['transport_call_id'], 0, 'Disabled original has no transport mutation')
                exact(row['transport_generation'], 0, 'Disabled original has no transport generation')
    command = c['command']
    obj(command, 'raw generation epoch', 'Original command recording')
    for key in command:
        positive(command[key], 'Command ' + key)
    creation = _creation(calls, 'CreateCommandList', command['raw'], command['generation'])
    pairs = {}
    for key, api, kind in (('reset_call', 'ResetCommandList', 'HRESULT'), ('bind_call', 'SetDescriptorHeaps', 'void'),
                           ('root0_call', 'SetGraphicsRootDescriptorTable', 'void'), ('close_call', 'CloseCommandList', 'HRESULT'),
                           ('execute_call', 'ExecuteCommandLists', 'void'), ('signal_call', 'Signal', 'HRESULT'),
                           ('completed_call', 'GetCompletedValue', 'uint64'), ('readback_call', 'MapReadback', 'HRESULT')):
        pairs[key] = _call(calls, c[key], api, case, kind)
    reset, bind, root, close, execute = (pairs[k] for k in ('reset_call', 'bind_call', 'root0_call', 'close_call', 'execute_call'))
    require(creation['after']['original_transition'] < reset['before']['original_transition'], 'Command created before recording')
    recording_epoch = 1
    resets = [p for p in calls.values() if p['before']['api'] == 'ResetCommandList' and
              p['before']['args'].get('command') == command['raw'] and
              p['before']['args'].get('command_generation') == command['generation'] and
              creation['after']['original_transition'] < p['before']['original_transition'] <= reset['before']['original_transition']]
    for original_reset in resets:
        exact(original_reset['before']['returns'], 'HRESULT', 'Original Reset return kind')
        exact(original_reset['before']['args']['previous_epoch'], recording_epoch, 'Original recording epoch before Reset')
        if original_reset['after']['hresult'] >= 0:
            recording_epoch += 1
    exact(command['epoch'], recording_epoch, 'Actual successful original Reset sequence')
    for key, pair in pairs.items():
        if key in ('reset_call', 'bind_call', 'root0_call', 'close_call'):
            exact(pair['before']['args']['command'], command['raw'], 'Original command raw')
            exact(pair['before']['args']['command_generation'], command['generation'], 'Original command generation')
            if key == 'reset_call':
                exact(pair['before']['args']['previous_epoch'] + 1, command['epoch'], 'Successful original Reset increments epoch')
            else:
                exact(pair['before']['args']['recording_epoch'], command['epoch'], 'Original recording epoch')
    ordered = [reset, bind, root, close, execute, pairs['signal_call'], pairs['completed_call'], pairs['readback_call']]
    require(all(a['after']['original_transition'] < b['before']['original_transition'] for a, b in zip(ordered, ordered[1:])), 'Owned original recording/submission/readback order')
    heap = c['visible_heap']
    obj(heap, 'raw generation cpu_start gpu_start increment count', 'Selected visible heap')
    for key in heap:
        positive(heap[key], 'Visible heap ' + key)
    exact(heap['count'], 1, 'Single selected visible slot')
    heapref = dict(raw=heap['raw'], generation=heap['generation'])
    exact(c['selected_destination'], dict(heap=heapref, index=0, cpu_handle=heap['cpu_start']), 'Selected original CPU slot')
    heap_create = _creation(calls, 'CreateDescriptorHeap', heap['raw'], heap['generation'])
    for key, expected in (('role', 'visible_srv'), ('type', 0), ('count', 1), ('flags', 1), ('node_mask', 0), ('tracked_srv_heap', True)):
        # Old journals are deliberately not accepted through this new shape.
        exact(heap_create['before']['args'][key], expected, 'Original bound heap declaration')
    cpu_base = [p for p in calls.values() if p['before']['api'] == 'GetCPUDescriptorHandleForHeapStart' and
                p['before']['args'] == {'heap': heap['raw'], 'generation': heap['generation']}]
    require(len(cpu_base) == 1, 'Unique original selected CPU-base getter')
    exact(cpu_base[0]['after']['value'], {'cpu_start': heap['cpu_start']}, 'Actual selected CPU-base return')
    require(heap_create['after']['original_transition'] < cpu_base[0]['before']['original_transition'] and
            cpu_base[0]['after']['original_transition'] < reset['before']['original_transition'], 'Bound heap exists before its original CPU-base query and recording')
    strides = [p for p in calls.values() if p['before']['api'] == 'GetDescriptorHandleIncrementSize' and
               p['before']['args'] == {'device': heap_create['before']['args']['device'], 'type': 0} and
               p['after']['original_transition'] < reset['before']['original_transition']]
    require(strides and all(p['after']['value'] == {'increment': heap['increment']} for p in strides), 'Original selected descriptor increment')
    exact(bind['before']['args']['visible_heap_ref'], heapref, 'Bound visible heap occurrence')
    bound = bind['before']['args']['heaps']
    require(type(bound) is list and len(bound) == 2 and all(type(x) is int and x > 0 for x in bound) and bound[0] == heap['raw'] and bound[0] != bound[1], 'Original heap binding arguments')
    exact(root['before']['args']['parameter'], 0, 'Original root parameter zero')
    exact(root['before']['args']['heap'], heapref, 'Original root heap occurrence')
    exact(root['before']['args']['slot'], 0, 'Original root slot zero')
    exact(root['before']['args']['gpu_handle'], heap['gpu_start'], 'Exact original selected GPU handle')
    gpu = [p for p in calls.values() if p['before']['api'] == 'GetGPUDescriptorHandleForHeapStart' and
           p['before']['args'] == {'heap': heap['raw'], 'generation': heap['generation']}]
    require(len(gpu) == 1, 'Unique original selected GPU-base getter')
    exact(gpu[0]['after']['value'], {'gpu_start': heap['gpu_start']}, 'Actual GPU-base return')
    require(heap_create['after']['original_transition'] < gpu[0]['before']['original_transition'] and
            gpu[0]['after']['original_transition'] < root['before']['original_transition'], 'Original GPU-base known before root binding')
    exact(execute['before']['args']['command_lists'], [command['raw']], 'Original Execute list')
    positive(execute['before']['args']['queue'], 'Original Execute queue')
    exact(execute['before']['args']['command_generation'], command['generation'], 'Execute command generation')
    exact(execute['before']['args']['recording_epoch'], command['epoch'], 'Execute recording epoch')
    # A forgotten later bind/reset/root0 cannot be hidden by selecting old refs.
    within = [p for p in calls.values() if p['before']['case_ordinal'] == case and p['before']['api'] in
              ('ResetCommandList', 'SetDescriptorHeaps', 'CloseCommandList', 'ExecuteCommandLists')]
    exact({p['before']['ordinal'] for p in within}, {reset['before']['ordinal'], bind['before']['ordinal'], close['before']['ordinal'], execute['before']['ordinal']}, 'All selected recording changes accounted')
    rootzero = [p for p in calls.values() if p['before']['case_ordinal'] == case and p['before']['api'] == 'SetGraphicsRootDescriptorTable' and p['before']['args'].get('parameter') == 0]
    exact(len(rootzero), 1, 'No later root-zero replacement')
    selected = _call(calls, c['selected_mutation_call'], 'CreateShaderResourceView' if case == 1 else 'CopyDescriptorsSimple', case, 'void')
    exact(selected['before']['mutation_call_id'], c['selected_mutation_call_id'], 'Original selected mutation ID')
    exact(selected['before']['args']['destination'], c['selected_destination'], 'Actual selected original destination slot')
    require(cpu_base[0]['after']['original_transition'] < selected['before']['original_transition'] and
            all(p['after']['original_transition'] < selected['before']['original_transition'] for p in strides), 'Original slot facts known before selected original mutation')
    if case == 1:
        require(selected['after']['original_transition'] < reset['before']['original_transition'], 'Direct A original write before bind')
    else:
        require(close['after']['original_transition'] < selected['before']['original_transition'] < selected['after']['original_transition'] < execute['before']['original_transition'], 'Actual late Copy after Close and before Execute')
        exact(selected['before']['args']['count'], 1, 'Late original unit copy')
        exact(selected['before']['args']['source']['index'], 1, 'Late original source slot B')
    if c['transport_enabled']:
        require(transport['selected_history_known'], 'Active selected case requires a complete historical relation')
        require(selected['after']['transition'] <= c['snapshot_cutoff'] < execute['before']['transition'], 'Actual native capture before Execute')
        _join_snapshot(w['snapshot'], calls, c['snapshot_cutoff'])
        exact(w['snapshot']['selected_destination'], c['selected_destination'], 'Delivered original selected slot')
        visible = [x for x in w['snapshot']['heaps'] if x['role'] == 'visible'][0]
        for key in ('raw', 'generation', 'cpu_start', 'increment', 'count'):
            exact(visible[key], heap[key], 'Delivered visible heap ' + key)
        exact(transport['history']['selected_version']['call_id'], selected['before']['transport_call_id'], 'Selected exact delivered typed version')
        resource_role = 'texture_A' if case == 1 else 'texture_B'
        origin = [p for p in calls.values() if p['before']['api'] == 'CreateCommittedResource' and p['before']['args'].get('role') == resource_role]
        require(len(origin) == 1, 'Unique expected original texture occurrence')
        exact(transport['history']['selected_version']['origin']['resource'], dict(raw=origin[0]['after']['value']['raw'], generation=origin[0]['after']['value']['transport_generation']), 'Original expected historical A/B resource')
    else:
        exact(w['snapshot'], None, 'Disabled capture never retagged as history')
    producer = c['producer']
    obj(producer, 'fence_raw fence_generation fence_value completed_value raw_file raw_bytes raw_sha256', 'Original producer/readback')
    for key in ('fence_raw', 'fence_generation', 'fence_value', 'completed_value', 'raw_bytes'):
        positive(producer[key], 'Producer ' + key)
    require(producer['fence_value'] <= producer['completed_value'] < MAX_U64, 'Actual finite producer fence completion')
    fence_creation = _creation(calls, 'CreateFence', producer['fence_raw'], producer['fence_generation'])
    exact(fence_creation['before']['args'], {'initial_value': 0, 'flags': 0}, 'Original owned fence starts unsignalled')
    signal, completed, mapped = (pairs[k] for k in ('signal_call', 'completed_call', 'readback_call'))
    require(fence_creation['after']['original_transition'] < signal['before']['original_transition'], 'Original fence existed before producer Signal')
    previous_value = 0
    signals = [p for p in calls.values() if p['before']['api'] == 'Signal' and
               p['before']['args'].get('fence') == producer['fence_raw'] and
               p['before']['args'].get('fence_generation') == producer['fence_generation'] and
               p['before']['original_transition'] <= signal['before']['original_transition']]
    for earlier in signals:
        value = earlier['before']['args']['fence_value']
        positive(value, 'Original signalled fence value')
        require(value > previous_value and fence_creation['after']['original_transition'] < earlier['before']['original_transition'], 'Original fence values strictly advance after creation')
        exact(earlier['before']['args']['queue'], execute['before']['args']['queue'], 'Same owned producer queue throughout fence prefix')
        exact(earlier['after']['hresult'], 0, 'Original earlier fence Signal success')
        previous_value = value
    fence = dict(fence=producer['fence_raw'], fence_generation=producer['fence_generation'])
    exact(signal['before']['args'], dict(queue=execute['before']['args']['queue'], **fence, fence_value=producer['fence_value']), 'Same queue original producer signal')
    exact(completed['before']['args'], fence, 'Exact producer fence polled')
    exact(completed['after']['value'], {'completed_value': producer['completed_value']}, 'Actual original fence return')
    pre = [p for p in calls.values() if p['before']['api'] == 'GetCompletedValue' and p['before']['case_ordinal'] == case and
           signal['after']['original_transition'] < p['before']['original_transition'] < completed['before']['original_transition']]
    require(len(pre) == 1, 'Original fence precheck')
    exact(pre[0]['before']['args'], fence, 'Precheck exact producer fence')
    require(pre[0]['after']['value']['completed_value'] < MAX_U64, 'Original precheck is not device-removal sentinel')
    if c['wait_call'] is None:
        exact(c['event_call'], None, 'No fabricated event registration')
        require(pre[0]['after']['value']['completed_value'] >= producer['fence_value'], 'Actual original precheck already complete')
    else:
        require(pre[0]['after']['value']['completed_value'] < producer['fence_value'], 'Original wait branch was necessary')
        event = _call(calls, c['event_call'], 'SetEventOnCompletion', case, 'HRESULT')
        wait = _call(calls, c['wait_call'], 'WaitForSingleObject', case, 'uint32')
        event_args = event['before']['args']
        exact({k: event_args[k] for k in ('queue', 'fence', 'fence_generation', 'fence_value')}, signal['before']['args'], 'Exact fence event registration')
        positive(event_args['event'], 'Original event handle')
        exact(wait['before']['args'], dict(event=event_args['event'], timeout=4294967295), 'Exact registered event waited')
        exact(wait['after']['value'], {'wait_result': 0}, 'Original WAIT_OBJECT_0')
        require(pre[0]['after']['original_transition'] < event['before']['original_transition'] < event['after']['original_transition'] < wait['before']['original_transition'] < wait['after']['original_transition'] < completed['before']['original_transition'], 'Original event/wait/completion order')
    exact(producer['raw_file'], c['name'] + '.bgra', 'Same-case raw file')
    exact(producer['raw_bytes'], 256 * 144 * 4, 'Exact owned BGRA shape')
    sha(producer['raw_sha256'])
    exact(mapped['before']['args']['range'], [0, producer['raw_bytes']], 'Original readback mapped range')
    positive(mapped['after']['value']['pointer'], 'Original readback Map output')
    readbacks = [p for p in calls.values() if p['before']['api'] == 'CreateCommittedResource' and p['before']['args'].get('role') == 'readback']
    require(len(readbacks) == 1, 'Unique original readback resource')
    rb = readbacks[0]
    created = _creation(calls, 'CreateCommittedResource', rb['after']['value']['raw'], rb['after']['value']['object_generation'])
    require(created['after']['original_transition'] < mapped['before']['original_transition'], 'Original readback created before Map')
    exact(mapped['before']['args']['resource'], created['after']['value']['raw'], 'Exact same readback occurrence')
    pixels = None
    if raw is not None:
        require(type(raw) is bytes and len(raw) == producer['raw_bytes'], 'Actual tight BGRA input')
        exact(hashlib.sha256(raw).hexdigest(), producer['raw_sha256'], 'Same-case actual readback hash')
        pixels = compare_pixels(raw, c['name'])
        exact(pixels['mismatched_bytes'], 0, 'Frozen independent full-byte pixel oracle')
    return dict(schema_version=1, purpose='new_owned_typed_copy_recording_readback_join', name=c['name'],
                case_ordinal=case, original_typed_arguments_joined=bool(c['transport_enabled']),
                selected_history_known=transport['selected_history_known'], native_history=transport,
                original_recording_binding_joined=True, original_selected_mutation_before_execute=True,
                original_same_queue_fence_completion_observed=True, original_readback_hash=producer['raw_sha256'],
                actual_pixels_compared=pixels is not None, pixel_audit=pixels,
                **{key: False for key in FALSE_FACTS})


def audit_case(wire, original_events, context, raw=None, *, require_history=None):
    try:
        return _audit_case(wire, original_events, context, raw, require_history=require_history)
    except (KeyError, IndexError, TypeError) as error:
        raise ValueError('Malformed selected original case fields') from error
