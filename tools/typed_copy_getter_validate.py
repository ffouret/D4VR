"""Pure CPU audit of the NEW source-adapter prototype envelope.

The nested transport/model are read unchanged. Their internal NEW-device
initialization is not a public COM factory result or a vendor/runtime proof.
No source argument, module, descriptor byte or GPU read is attested here.
"""
from __future__ import annotations

import hashlib

import typed_copy_transport_validate as native

FALSE_FACTS = native.FALSE_FACTS
MAX_WIRE = 16384
PURPOSE = 'owned_translator_original_typed_COM_getter_history'
SOURCE_KEYS = ('device_origin public_outer_result_known factory_calls factory_returns '
               'existing_singleton_calls source_calls source_returns ignored_calls '
               'pending case_ordinal observer_events mutations factory_records')


def _ref(value, label):
    native.obj(value, 'raw generation', label)
    for key in ('raw', 'generation'):
        native.positive(value[key], label + ' ' + key)


def _validate_getters(value, replay, healthy):
    """Validate immutable original operands and exact observer publication IDs."""
    source, p = value['source'], replay['producer']
    data = value['getters']
    native.obj(data, 'calls returns ignored_unselected ignored_faulted dropped publication_policy eligibility_scope records', 'Getter accounting')
    native.exact(data['publication_policy'], 'latest_eligible_RETURN_before_publication', 'Declared publication policy')
    native.exact(data['eligibility_scope'], 'original_RETURN_operand_only_not_heap_span_or_bind', 'Historical operand scope')
    for key in ('calls', 'returns', 'ignored_unselected', 'ignored_faulted', 'dropped'):
        native.uint(data[key], 'Getter ' + key)
    native.require(data['returns'] <= data['calls'] <= source['source_calls'], 'Getter calls are source originals')
    native.require(data['returns'] <= source['source_returns'], 'Getter returns are source originals')
    rows, pubs, cutoff = data['records'], value['publications'], value['cutoff']
    native.require(type(rows) is list and len(rows) <= 8, 'Eight retained original getters')
    native.require(type(pubs) is list and len(pubs) <= 2, 'Two observer publications')
    native.obj(cutoff, 'kind original_API transition epoch', 'Observer cutoff')
    native.exact(cutoff['kind'], 'observer_capture_cutoff', 'Cutoff scope')
    native.exact(cutoff['original_API'], False, 'Cutoff is not a D3D API')
    native.exact(cutoff['epoch'], p['epoch'], 'Cutoff source epoch')
    native.uint(cutoff['transition'], 'Cutoff transition')
    native.exact(cutoff['transition'], p['clock'], 'Exact frozen observer cutoff')
    if p['enabled']:
        native.positive(cutoff['transition'], 'Enabled observer cutoff')
    else:
        native.exact(cutoff['transition'], 0, 'Disabled observer cutoff')
    minimum = len(rows) + data['ignored_unselected'] + data['ignored_faulted'] + data['dropped']
    if healthy:
        native.exact(minimum, data['calls'], 'All relevant/ignored/dropped getter calls accounted')
        native.exact(source['source_calls'] + source['source_returns'] + source['observer_events'], p['clock'], 'Original and observer clock domains explicitly counted')
        native.exact(data['ignored_faulted'], 0, 'Healthy getter scope')
        native.exact(data['dropped'], 0, 'Healthy getter retention')
        native.require(source['observer_events'] <= len(pubs) + value['sequence'], 'Healthy prior seal marks have distinct positive increasing sequences')
    else:
        native.require(minimum <= data['calls'], 'Diagnostic getter accounting lower bound')
    native.require(source['observer_events'] >= len(pubs) + bool(cutoff['transition']), 'Every publication and current cutoff is an observer mark')
    native.require(not (data['dropped'] or data['ignored_faulted']) or (p['faulted'] and p['gap']), 'Getter drops/faults remain partial')
    clocks = set()
    def clock(t, label, pending=False):
        native.uint(t, label)
        if pending and not t:
            return
        native.require(0 < t <= p['clock'] and t not in clocks, label + ' unique common-clock occurrence')
        clocks.add(t)
    snapshot = value['native_wire']['snapshot']
    objects = [] if snapshot is None else [snapshot['device']] + snapshot['resources'] + snapshot['heaps']
    for obj in objects:
        for key in ('create_entry', 'create_return', 'release_entry', 'release_return'):
            clock(obj[key], 'Native lifetime ' + key, pending=True)
    for event in source['mutations'] + source['factory_records']:
        clock(event['entry'], 'Original ENTRY')
        clock(event['return'], 'Original RETURN', pending=True)
    clock(cutoff['transition'], 'Observer capture cutoff', pending=not p['enabled'])
    if healthy:
        object_calls = sum(bool(o['create_entry']) + bool(o['release_entry']) for o in objects)
        retained_calls = object_calls + len(source['mutations']) + source['factory_calls'] + source['existing_singleton_calls'] + source['ignored_calls'] + len(rows)
        native.require(retained_calls <= source['source_calls'], 'Getter originals are separate retained source call families')
        native.require(data['ignored_unselected'] <= source['ignored_calls'], 'Ignored getter originals belong to ignored source calls')
    by_id, completed = {}, []
    reasons = ('pending', 'none', 'output_pointer_mismatch', 'lifetime_changed', 'unsupported_type',
               'zero_value', 'value_out_of_range', 'conflicting_value', 'scope_faulted')
    def life(obj, entry, returned):
        return bool(obj['create_return'] and obj['create_hresult'] >= 0 and
                    obj['create_return'] < entry and
                    (not obj['release_entry'] or obj['release_entry'] > returned))
    for row_index, row in enumerate(rows):
        native.obj(row, 'id epoch kind entry return object_kind source_object parent_device argument_heap_type '
                   'caller_output_pointer return_kind returned_pointer value_known value lifetime_valid_through_return arithmetic_eligible reason', 'Original getter')
        native.positive(row['id'], 'Getter source token ID')
        native.require(row['id'] <= source['source_calls'] and row['id'] not in by_id, 'Distinct getter callback identity')
        by_id[row['id']] = row
        if row_index:
            native.require(rows[row_index-1]['id'] < row['id'] and rows[row_index-1]['entry'] < row['entry'], 'Rows reserved in exact source ENTRY order')
        native.exact(row['epoch'], p['epoch'], 'Getter source epoch')
        native.require(row['kind'] in ('cpu_start', 'gpu_start', 'increment'), 'Original getter method')
        inc = row['kind'] == 'increment'
        native.exact(row['object_kind'], 'device' if inc else 'heap', 'Getter source object type')
        native.exact(row['return_kind'], 'UINT32' if inc else 'structure_pointer', 'Original return ABI')
        _ref(row['source_object'], 'Saved source object')
        _ref(row['parent_device'], 'Saved parent device')
        if inc:
            native.uint(row['argument_heap_type'], 'Original enum32 argument', (1 << 32) - 1)
            native.require(row['argument_heap_type'] not in (1, 2, 3), 'Known unselected types are ignored, not retagged SRV')
            native.exact(row['caller_output_pointer'], None, 'UINT return has no output pointer')
            native.exact(row['returned_pointer'], None, 'UINT return is not a pointer')
            native.exact(row['source_object'], row['parent_device'], 'Increment device owns itself')
        else:
            native.exact(row['argument_heap_type'], None, 'Heap start has no heap-type argument')
            native.uint(row['caller_output_pointer'], 'Original caller output pointer')
            if row['return']:
                native.uint(row['returned_pointer'], 'Actual original returned pointer')
            else:
                native.exact(row['returned_pointer'], None, 'Pending pointer return unknown')
        for key in ('value_known', 'lifetime_valid_through_return', 'arithmetic_eligible'):
            native.boolean(row[key], key)
        native.require(row['reason'] in reasons, 'Bounded getter reason')
        clock(row['entry'], 'Getter ENTRY')
        clock(row['return'], 'Getter RETURN', pending=True)
        if row['return']:
            native.require(row['entry'] < row['return'] < cutoff['transition'], 'Original getter returns before observer seal')
        if row['value_known']:
            native.uint(row['value'], 'Observed original value', (1 << 32) - 1 if inc else native.MAX_U64)
        else:
            native.exact(row['value'], None, 'Unobserved value is null')
        if not row['return']:
            for key in ('value_known', 'lifetime_valid_through_return', 'arithmetic_eligible'):
                native.exact(row[key], False, 'Pending getter ' + key)
            native.exact(row['reason'], 'pending', 'Pending original reason')
            continue
        completed.append(row)
        pointer_valid = inc or (row['caller_output_pointer'] != 0 and row['returned_pointer'] == row['caller_output_pointer'])
        if not inc:
            native.exact(row['value_known'], pointer_valid, 'Only exact nonnull original output may be copied')
        if snapshot is not None:
            candidates = [o for o in ([snapshot['device']] if inc else snapshot['heaps'])
                          if {k: o[k] for k in ('raw', 'generation')} == row['source_object']]
            native.require(len(candidates) == 1, 'Saved source generation exists in native input')
            obj = candidates[0]
            native.exact(row['parent_device'], {k: snapshot['device'][k] for k in ('raw', 'generation')}, 'Saved original parent device')
            expected_life = life(obj, row['entry'], row['return']) and life(snapshot['device'], row['entry'], row['return'])
            native.exact(row['lifetime_valid_through_return'], expected_life, 'Historical ENTRY-through-RETURN lifetime')
        if row['arithmetic_eligible']:
            native.require(row['value_known'] and row['value'] > 0 and row['lifetime_valid_through_return'] and
                           (not inc or row['argument_heap_type'] == 0), 'Eligible historical operand prerequisites')
            native.exact(row['reason'], 'none', 'Eligible original reason')
        else:
            native.require(row['reason'] != 'none', 'Refused operand has explicit reason')
        reason = row['reason']
        if not row['value_known']:
            native.exact(reason, 'value_out_of_range' if inc else 'output_pointer_mismatch', 'Value framing rejection')
        elif not row['lifetime_valid_through_return']:
            native.exact(reason, 'lifetime_changed', 'Historical lifetime rejection')
        elif inc and row['argument_heap_type'] != 0:
            native.exact(reason, 'unsupported_type', 'Exact unsupported enum rejection')
        elif row['value'] == 0:
            native.exact(reason, 'zero_value', 'Observed zero remains distinguishable')
        else:
            native.require(reason in ('none', 'scope_faulted', 'conflicting_value'), 'Eligible operand or subsequent admission refusal')
        if reason not in ('none', 'pending'):
            native.require(p['faulted'] and p['gap'], 'A refused retained getter faults the finite scope')
    native.require(sum(not x['return'] for x in rows) <= source['pending'], 'Pending getters remain pending originals')
    native.require(len(completed) <= data['returns'], 'Retained getter RETURNs counted')
    native.require(data['calls'] - data['returns'] <= source['pending'], 'Getter pending belongs to source pending')
    if healthy:
        original_entries = [o[key] for o in objects for key in ('create_entry', 'release_entry') if o[key]]
        original_entries += [r['entry'] for r in source['mutations'] + source['factory_records'] + rows]
        for row in rows:
            prior = sum(t < row['entry'] for t in original_entries)
            later = sum(t > row['entry'] for t in original_entries)
            native.require(1 + prior <= row['id'] <= source['source_calls'] - later,
                           'Getter token ID counts original BEGINs, not observer marks or another namespace')
    for row in sorted(completed, key=lambda r: r['return']):
        prior = [r for r in completed if r['return'] < row['return'] and r['arithmetic_eligible'] and
                 r['kind'] == row['kind'] and r['source_object'] == row['source_object']]
        conflict = bool(prior and max(prior, key=lambda r: r['return'])['value'] != row['value'])
        if row['reason'] == 'conflicting_value':
            native.require(conflict, 'Conflict needs a distinct earlier eligible original value')
        if row['arithmetic_eligible']:
            native.require(not conflict, 'Conflicting original cannot become eligible')
            native.require(not any(r['return'] < row['return'] and r['reason'] != 'none' for r in completed), 'Earlier getter failure remains sticky')
    seen_heaps = set()
    for index, pub in enumerate(pubs, 1):
        native.obj(pub, 'id kind original_API transition epoch heap device heap_count cpu_getter_id increment_getter_id span_arithmetic_valid heap_facts_called published', 'Observer publication')
        native.exact(pub['id'], index, 'Publication identity')
        native.exact(pub['kind'], 'observer_heap_facts_publication', 'Publication scope')
        native.exact(pub['original_API'], False, 'Publication is not an original API')
        native.exact(pub['epoch'], p['epoch'], 'Publication epoch')
        for key in ('span_arithmetic_valid', 'heap_facts_called'):
            native.boolean(pub[key], key)
        native.uint(pub['heap_count'], 'Original heap count', (1 << 32) - 1)
        if pub['heap_facts_called']:
            native.boolean(pub['published'], 'Actual native heapFacts result')
        else:
            native.exact(pub['published'], None, 'No invented return for an uncalled observer operation')
        _ref(pub['heap'], 'Publication heap')
        _ref(pub['device'], 'Publication parent')
        clock(pub['transition'], 'Observer publication mark')
        native.require(pub['transition'] < cutoff['transition'], 'Publication precedes seal')
        if index > 1:
            native.require(pubs[index-2]['transition'] < pub['transition'], 'Publication reservation order')
        heap_key = tuple(pub['heap'][k] for k in ('raw', 'generation'))
        native.require(heap_key not in seen_heaps, 'Each heap publishes once')
        seen_heaps.add(heap_key)
        for key, kind, object_ref in (('cpu_getter_id', 'cpu_start', pub['heap']),
                                      ('increment_getter_id', 'increment', pub['device'])):
            native.positive(pub[key], 'Published original getter ID')
            candidates = [r for r in rows if r['kind'] == kind and r['source_object'] == object_ref and
                          r['parent_device'] == pub['device'] and r['arithmetic_eligible'] and
                          0 < r['return'] < pub['transition']]
            native.require(candidates, 'Publication has actual eligible original prerequisite')
            selected = max(candidates, key=lambda r: r['return'])
            native.exact(pub[key], selected['id'], 'Exact frozen latest-eligible-RETURN ID')
        base = by_id[pub['cpu_getter_id']]['value']
        increment = by_id[pub['increment_getter_id']]['value']
        span = base <= native.MAX_U64 - pub['heap_count'] * increment
        native.exact(pub['span_arithmetic_valid'], span, 'Checked u64 base+count*increment span')
        native.exact(pub['heap_facts_called'], span, 'Only a valid span invokes observer publication')
        if snapshot is not None:
            heaps = [h for h in snapshot['heaps'] if {k: h[k] for k in ('raw', 'generation')} == pub['heap']]
            native.require(len(heaps) == 1, 'Published native heap occurrence')
            native.exact(pub['device'], heaps[0]['device'], 'Publication parent from original heap creation')
            native.exact(pub['heap_count'], heaps[0]['count'], 'Original created heap count')
            native.require(life(heaps[0], pub['transition'], pub['transition']) and
                           life(snapshot['device'], pub['transition'], pub['transition']), 'Publication uses live saved heap and parent occurrences')
            if pub['published']:
                native.exact(heaps[0]['cpu_start'], by_id[pub['cpu_getter_id']]['value'], 'Published CPU base')
                native.exact(heaps[0]['increment'], by_id[pub['increment_getter_id']]['value'], 'Published original UINT increment')
        if pub['published'] is not True:
            native.require(p['faulted'] and p['gap'], 'Failed native publication remains partial')
    if replay['selected_history_known']:
        native.require(len(pubs) == 2 and all(x['published'] is True for x in pubs), 'Known history needs both exact CPU publications')
    if snapshot is not None:
        for event in snapshot['events']:
            args = event['before']
            if event['api'] == 'CreateShaderResourceView':
                slots = [args['destination']]
            elif event['api'] == 'CopyDescriptorsSimple':
                slots = [args['source'], args['destination']]
            else:
                slots = args['source_starts'] + args['destination_starts']
            for slot in slots:
                established = [pub for pub in pubs if pub['heap'] == slot['heap'] and pub['published'] is True
                               and pub['transition'] < event['entry']]
                native.require(len(established) == 1, 'Original typed slot resolution follows its exact successful observer publication')
    if not p['enabled']:
        for key in ('calls', 'returns', 'ignored_unselected', 'ignored_faulted', 'dropped'):
            native.exact(data[key], 0, 'Disabled getter accounting')
        native.exact(rows, [], 'Disabled getter rows')
        native.exact(pubs, [], 'Disabled publications')
    return dict(retained_getters=len(rows), completed_retained_getters=len(completed),
                eligible_historical_operands=sum(r['arithmetic_eligible'] for r in rows),
                exact_CPU_publications=sum(r['published'] is True for r in pubs),
                same_run_bind_provenance_known=False, getter_to_bind_relation_known=False)


def audit_getter_wire(data):
    value = native.parse(data, MAX_WIRE)
    native.obj(value, 'schema_version purpose prototype_only vendor_built runtime_executed '
               'run_id sequence source getters publications cutoff native_wire public_device_create_result_known source_runtime_relation_known same_run_bind_provenance_known getter_to_bind_relation_known '
               + ' '.join(FALSE_FACTS), 'Source prototype envelope')
    native.exact(value['schema_version'], 2, 'Getter source schema')
    native.exact(value['purpose'], PURPOSE, 'New source purpose')
    for name, expected in (('prototype_only', True), ('vendor_built', False),
                           ('runtime_executed', False),
                           ('public_device_create_result_known', False), ('source_runtime_relation_known', False), ('same_run_bind_provenance_known', False), ('getter_to_bind_relation_known', False)):
        native.exact(value[name], expected, name)
    for key in FALSE_FACTS:
        native.exact(value[key], False, key)
    native.positive(value['sequence'], 'Source sequence')
    replay = native.audit_wire(value['native_wire'])
    native.exact(value['run_id'], replay['run_id'], 'Nested nonce')
    native.exact(value['sequence'], replay['sequence'], 'Nested sequence')
    source = value['source']
    native.obj(source, SOURCE_KEYS, 'Source declarations')
    native.exact(source['public_outer_result_known'], False, 'Public outer result remains unknown')
    native.require(source['device_origin'] in ('unknown', 'internal_new_initialization'), 'Device domain')
    counters = ('factory_calls', 'factory_returns', 'existing_singleton_calls',
                'source_calls', 'source_returns', 'ignored_calls', 'pending', 'case_ordinal', 'observer_events')
    for key in counters:
        native.uint(source[key], 'Source ' + key)
    native.require(1 <= source['case_ordinal'] <= 8, 'Finite source case label')
    native.require(source['pending'] <= 32, 'Pending metadata quota')
    native.require(source['source_returns'] <= source['source_calls'], 'Source BEFORE/AFTER count')
    native.require(source['factory_returns'] <= source['factory_calls'] <= source['source_calls'], 'Factory count')
    native.require(source['factory_returns'] <= source['source_returns'], 'Factory returns counted')
    native.require(source['ignored_calls'] <= source['source_calls'], 'Ignored source calls counted')
    native.require(source['existing_singleton_calls'] <= source['source_calls'], 'Singleton calls counted')
    p = replay['producer']
    healthy = p['enabled'] and not (p['faulted'] or p['gap'] or p['dropped'])
    if healthy:
        native.exact(source['source_calls'] - source['source_returns'], source['pending'], 'Closed source counter accounting')
    else:
        native.require(source['pending'] <= source['source_calls'] - source['source_returns'], 'Pending lower bound')
    rows = source['mutations']
    factories = source['factory_records']
    native.require(type(rows) is list and len(rows) <= 8, 'Mutation metadata quota')
    native.require(type(factories) is list and len(factories) <= 4, 'Factory metadata quota')
    native.require(len(rows) + source['factory_calls'] + source['existing_singleton_calls'] + source['ignored_calls'] <= source['source_calls'], 'Separate source call families')
    if healthy:
        native.exact(len(factories), source['factory_calls'], 'Every retained factory original')
    else:
        native.require(len(factories) <= source['factory_calls'], 'Factory diagnostic records')
    clock = p['clock']
    if healthy:
        native.require(source['source_calls'] + source['source_returns'] <= clock, 'Each captured source edge consumes the common clock')
    entries, returns, ids = set(), set(), set()
    def occurrence(row, label):
        native.positive(row['entry'], label + ' ENTRY')
        native.uint(row['return'], label + ' RETURN')
        native.require(row['entry'] <= clock and row['entry'] not in entries, label + ' unique clock ENTRY')
        entries.add(row['entry'])
        if row['return']:
            native.require(row['entry'] < row['return'] <= clock and row['return'] not in returns, label + ' ordered unique RETURN')
            returns.add(row['return'])
    for row in rows:
        native.obj(row, 'call_id variant entry return', 'Source mutation')
        native.positive(row['call_id'], 'Mutation call ID')
        native.require(row['call_id'] not in ids, 'Unique mutation ID')
        ids.add(row['call_id'])
        native.uint(row['variant'], 'Source variant', 9)
        occurrence(row, 'Mutation')
    for row in factories:
        native.obj(row, 'entry return hresult raw_internal_output outcome', 'Factory diagnostic')
        native.exact(row['outcome'], 'outer_factory_only_unknown', 'Factory domain')
        native.uint(row['raw_internal_output'], 'Internal factory output')
        occurrence(row, 'Factory')
        if row['return']:
            native.require(type(row['hresult']) is int and -(1 << 31) <= row['hresult'] < (1 << 31), 'Original HRESULT')
            if row['hresult'] < 0:
                native.exact(row['raw_internal_output'], 0, 'Failed output is not defined')
        else:
            native.exact(row['hresult'], None, 'Pending factory result')
            native.exact(row['raw_internal_output'], 0, 'Pending factory output')
    native.require(not entries.intersection(returns), 'Clock occurrences do not overlap')
    returned_factories = sum(bool(x['return']) for x in factories)
    if healthy:
        native.exact(returned_factories, source['factory_returns'], 'Every original factory RETURN retained')
    else:
        native.require(returned_factories <= source['factory_returns'], 'Factory returned rows')
    native.require(sum(not x['return'] for x in rows + factories) <= source['pending'], 'Retained pending rows need genuine pending originals')
    snapshot = value['native_wire']['snapshot']
    if snapshot is not None:
        events = snapshot['events']
        native.exact(len(rows), len(events), 'All native source mutations represented')
        for row, event in zip(rows, events):
            for a, b in (('call_id', 'call_id'), ('entry', 'entry'), ('return', 'return')):
                native.exact(row[a], event[b], 'Source/native occurrence ' + a)
            choices = {'CreateShaderResourceView': (0, 1, 2),
                       'CopyDescriptorsSimple': (0, 3, 4, 5, 6, 7, 8, 9),
                       'CopyDescriptors': (0,)}
            native.require(row['variant'] in choices[event['api']], 'Original source variant/API')
            phase = event['phase_before']
            native.require(phase['case_ordinal'] <= source['case_ordinal'], 'Source case cutoff')
            native.exact(phase['api_phase'], {'CreateShaderResourceView': 1, 'CopyDescriptorsSimple': 2, 'CopyDescriptors': 3}[event['api']], 'Source API label')
        native_times = set()
        objects = [snapshot['device']] + snapshot['resources'] + snapshot['heaps']
        for object_ in objects:
            for key in ('create_entry', 'create_return', 'release_entry', 'release_return'):
                if object_[key]:
                    native_times.add(object_[key])
        for event in events:
            native_times.update(t for t in (event['entry'], event['return']) if t)
        native.require(all(t not in native_times for f in factories for t in (f['entry'], f['return']) if t), 'Factory and native originals have distinct common-clock occurrences')
        if healthy:
            native_object_calls = sum(bool(o['create_entry']) + bool(o['release_entry']) for o in objects)
            native.require(native_object_calls + len(rows) + source['factory_calls'] + source['existing_singleton_calls'] + source['ignored_calls'] <= source['source_calls'], 'Native object originals are separate source call families')
            labels = [event['phase_before']['case_ordinal'] for event in events]
            native.require(not labels or (labels[0] == 1 and all(a <= b <= a + 1 for a, b in zip(labels, labels[1:]))), 'Nonempty source windows advance once')
            native.exact(source['case_ordinal'], labels[-1] if labels else 1, 'Source case is actual last mutation window')
    if not p['enabled']:
        for key in counters:
            if key != 'case_ordinal':
                native.exact(source[key], 0, 'Disabled source counters ' + key)
        native.exact(rows, [], 'Disabled source mutation list')
        native.exact(factories, [], 'Disabled source factories')
        native.exact(source['device_origin'], 'unknown', 'Disabled source origin')
    getter_result = _validate_getters(value, replay, healthy)
    known = bool(healthy and not source['pending'] and
                 source['device_origin'] == 'internal_new_initialization' and
                 replay['selected_history_known'])
    native.require(not replay['selected_history_known'] or known, 'A known nested model needs closed internal source observations')
    return dict(schema_version=1, purpose='prototype_immutable_original_getter_history_audit',
                prototype_only=True, vendor_built=False, runtime_executed=False,
                run_id=value['run_id'], sequence=value['sequence'], source=source, getters=value['getters'], publications=value['publications'], cutoff=value['cutoff'], getter_summary=getter_result,
                prototype_source_history_known=known,
                internal_model_replay_only=True, native_model_replay=replay,
                source_runtime_relation_known=False, same_run_bind_provenance_known=False, getter_to_bind_relation_known=False, public_device_create_result_known=False,
                installed_binary_source_equivalence_proven=False,
                canonical_source_envelope_sha256=hashlib.sha256(native._json(value).encode('ascii')).hexdigest(),
                **{key: False for key in FALSE_FACTS})


def audit_native_samples(data):
    value = native.parse(data, 2 * 1024 * 1024)
    native.obj(value, 'typed_copy_getter_adapter_cpu_passed cases assertions original_calls samples GPU_or_Wine_calls', 'Source CPU result')
    native.exact(value['typed_copy_getter_adapter_cpu_passed'], True, 'Source CPU checks')
    native.exact(value['GPU_or_Wine_calls'], 0, 'CPU-only scope')
    for key in ('cases', 'assertions', 'original_calls'):
        native.positive(value[key], key)
    results = _audit_samples(value['samples'])
    return dict(native_cases=value['cases'], native_assertions=value['assertions'],
                original_mock_calls=value['original_calls'], **_samples_report(results))


def _audit_samples(samples):
    native.require(type(samples) is list and 0 < len(samples) <= 96, 'Finite source samples')
    names, results = set(), []
    for sample in samples:
        native.obj(sample, 'name expected_known expected_origin wire', 'New source sample')
        native.require(type(sample['name']) is str and 0 < len(sample['name']) <= 100 and sample['name'] not in names, 'Unique source sample')
        names.add(sample['name'])
        native.boolean(sample['expected_known'], 'Independent expected internal history')
        result = audit_getter_wire(sample['wire'])
        native.exact(result['prototype_source_history_known'], sample['expected_known'], sample['name'])
        if sample['expected_known']:
            native.obj(sample['expected_origin'], 'raw generation', 'Expected internal resource')
            for key in ('raw', 'generation'):
                native.positive(sample['expected_origin'][key], 'Expected origin')
            native.exact(result['native_model_replay']['history']['selected_version']['origin']['resource'], sample['expected_origin'], 'Independent original expected resource')
        else:
            native.exact(sample['expected_origin'], None, 'No known origin')
        results.append(dict(name=sample['name'], prototype_source_history_known=result['prototype_source_history_known'],
                            snapshot_available=result['native_model_replay']['snapshot_available']))
    return results


def _samples_report(results):
    return dict(emitted_samples=len(results),
                known_internal_histories=sum(x['prototype_source_history_known'] for x in results),
                snapshots_consumed_by_frozen_model=sum(x['snapshot_available'] for x in results),
                samples=results, frozen_model_sha256=native.MODEL_SHA256,
                source_runtime_relation_known=False, same_run_bind_provenance_known=False, getter_to_bind_relation_known=False, public_device_create_result_known=False,
                prototype_only=True, vendor_built=False, runtime_executed=False,
                **{key: False for key in FALSE_FACTS})


def audit_integration_samples(data):
    """Only the NEW .inc -> real C ABI -> immutable-wire CPU harness."""
    value = native.parse(data, 2 * 1024 * 1024)
    native.obj(value, 'purpose cases assertions original_calls', 'New real getter C ABI integration')
    native.positive(value['assertions'], 'Integration assertions')
    native.positive(value['original_calls'], 'Integration original mock calls')
    native.exact(value['purpose'], 'real_C_ABI_getter_wrapper_CPU_integration', 'Integration purpose')
    results = _audit_samples(value['cases'])
    return dict(purpose='prototype_real_C_ABI_getter_wrapper_integration_audit', **_samples_report(results))
