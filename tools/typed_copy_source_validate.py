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
PURPOSE = 'owned_translator_original_typed_COM_history'
SOURCE_KEYS = ('device_origin public_outer_result_known factory_calls factory_returns '
               'existing_singleton_calls source_calls source_returns ignored_calls '
               'pending case_ordinal mutations factory_records')


def audit_source_wire(data):
    value = native.parse(data, MAX_WIRE)
    native.obj(value, 'schema_version purpose prototype_only vendor_built runtime_executed '
               'run_id sequence source native_wire public_device_create_result_known '
               + ' '.join(FALSE_FACTS), 'Source prototype envelope')
    native.exact(value['schema_version'], 1, 'Source schema')
    native.exact(value['purpose'], PURPOSE, 'New source purpose')
    for name, expected in (('prototype_only', True), ('vendor_built', False),
                           ('runtime_executed', False),
                           ('public_device_create_result_known', False)):
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
                'source_calls', 'source_returns', 'ignored_calls', 'pending', 'case_ordinal')
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
    known = bool(healthy and not source['pending'] and
                 source['device_origin'] == 'internal_new_initialization' and
                 replay['selected_history_known'])
    native.require(not replay['selected_history_known'] or known, 'A known nested model needs closed internal source observations')
    return dict(schema_version=1, purpose='prototype_source_original_typed_COM_history_audit',
                prototype_only=True, vendor_built=False, runtime_executed=False,
                run_id=value['run_id'], sequence=value['sequence'], source=source,
                prototype_source_history_known=known,
                internal_model_replay_only=True, native_model_replay=replay,
                source_runtime_relation_known=False, public_device_create_result_known=False,
                installed_binary_source_equivalence_proven=False,
                canonical_source_envelope_sha256=hashlib.sha256(native._json(value).encode('ascii')).hexdigest(),
                **{key: False for key in FALSE_FACTS})


def audit_native_samples(data):
    value = native.parse(data, 2 * 1024 * 1024)
    native.obj(value, 'typed_copy_source_adapter_cpu_passed cases assertions original_calls samples GPU_or_Wine_calls', 'Source CPU result')
    native.exact(value['typed_copy_source_adapter_cpu_passed'], True, 'Source CPU checks')
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
        result = audit_source_wire(sample['wire'])
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
                source_runtime_relation_known=False, public_device_create_result_known=False,
                prototype_only=True, vendor_built=False, runtime_executed=False,
                **{key: False for key in FALSE_FACTS})


def audit_integration_samples(data):
    """Only the NEW .inc -> real C ABI -> immutable-wire CPU harness."""
    value = native.parse(data, 2 * 1024 * 1024)
    native.obj(value, 'purpose cases', 'New real C ABI integration')
    native.exact(value['purpose'], 'real_C_ABI_source_wrapper_CPU_integration', 'Integration purpose')
    results = _audit_samples(value['cases'])
    native.exact([x['name'] for x in results],
                 ['source_wrapped_srv_A', 'source_wrapped_simple_B', 'source_wrapped_ranges_NULL_A'],
                 'Independent wrapper integration cases')
    native.require(all(x['prototype_source_history_known'] for x in results), 'All real-wrapper finite histories')
    return dict(purpose='prototype_real_C_ABI_source_wrapper_integration_audit', **_samples_report(results))
