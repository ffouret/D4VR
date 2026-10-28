"""Pure audit of a closed selected original-call relation.

This additive purpose preserves the complete strict owner result unchanged.
It permits a completely recorded, successful *other* instance occurrence to
remain open. It does not rename a producer's incomplete footer, infer why an
instance remains open, or certify a runtime, process, renderer or resource.
The caller still supplies the complete, externally pinned claim/journal set.
No filesystem, process, runtime or memory access is performed here.
"""
from __future__ import annotations

import json

try:
    from . import original_call_owner_validate as strict
except ImportError:
    import original_call_owner_validate as strict


FALSE_FACTS = strict.FALSE_FACTS
PURPOSE = 'closed_original_nonce_call_relation_audit'
MAX_REPORT_BYTES = 32 * 1024 * 1024
# The strict input bounds are inherited unchanged. The new complete result
# duplicates strict's parsed records/state and adds at most 8 scope summaries,
# 8*256 open-instance summaries and 2 selected joins. A separate 32 MiB compact
# JSON cap bounds serialized output, not Python heap/transient allocation and
# not any GPU/SourceHistory storage. No partial legacy result is returned.
_IDENTITY = strict.base.IDENTITY
_TARGET_KEYS = (*_IDENTITY, 'Unix_device', 'Unix_device_generation',
                'Unix_parent_instance_generation', 'Unix_sampler_generation',
                'sampler', 'PE_device')
_GAP_COUNTERS = ('faulted', 'transport_overflow', 'transport_chain_mismatch',
                 'owner_gap', 'owner_records_dropped', 'pending_owner_records',
                 'owner_unknown_phase', 'lifecycle_dropped', 'dropped_events',
                 'unknown_phase_queries', 'phase_unavailable', 'phase_changed',
                 'output_unavailable', 'typed_input_unknown')


def _identity(value):
    return tuple(value[key] for key in _IDENTITY)


def _bounded_report(report):
    size = 0
    encoder = json.JSONEncoder(ensure_ascii=True, allow_nan=False, separators=(',', ':'))
    for part in encoder.iterencode(report):
        size += len(part.encode('utf-8'))
        if size > MAX_REPORT_BYTES:
            raise ValueError('Complete closed-call report exceeds serialized byte limit')
    return report


def _selected_target(legacy):
    """Consume the unchanged strict joins, never filter its refusal strings."""
    joins = legacy['joins']
    if (legacy['unjoined'] or len(joins) != 2 or
            [row['sequence'] for row in joins] != [1, 2] or
            any(joins[0][key] != joins[1][key] for key in _TARGET_KEYS) or
            joins[0]['query_return'] >= joins[1]['marker_entry']):
        return None
    first = joins[0]
    matches = [scope for scope in legacy['scopes']
               if _identity(scope['identity']) == _identity(first)]
    if len(matches) != 1:
        return None
    state = matches[0]['state']
    sampler = state['samplers'].get(first['Unix_sampler_generation'])
    device = state['devices'].get(first['Unix_device_generation'])
    parent = state['instances'].get(first['Unix_parent_instance_generation'])
    if not sampler or not device or not parent:
        return None
    if (not sampler['sampler_lifetime_known'] or not sampler['device_lifetime_known'] or
            sampler['create_return'] >= first['marker_entry'] or
            not (joins[-1]['query_return'] < sampler['destroy_entry'] <
                 sampler['destroy_return'] < device['destroy_entry'] <
                 device['destroy_return'] < parent['destroy_entry'] <
                 parent['destroy_return'])):
        return None
    return dict(identity=dict(matches[0]['identity']),
                Unix_device=first['Unix_device'],
                Unix_device_generation=first['Unix_device_generation'],
                Unix_sampler_generation=first['Unix_sampler_generation'],
                Unix_parent_instance_generation=first['Unix_parent_instance_generation'],
                sampler=first['sampler'],
                sampler_create_entry=sampler['operation_ordinal'],
                sampler_create_return=sampler['create_return'],
                sampler_destroy_entry=sampler['destroy_entry'],
                sampler_destroy_return=sampler['destroy_return'],
                device_create_entry=device['operation_ordinal'],
                device_create_return=device['create_return'],
                device_destroy_entry=device['destroy_entry'],
                device_destroy_return=device['destroy_return'],
                parent_create_entry=parent['operation_ordinal'],
                parent_create_return=parent['create_return'],
                parent_destroy_entry=parent['destroy_entry'],
                parent_destroy_return=parent['destroy_return'])


def _route_lower_bound_peak(occurrences):
    """Only [create RETURN, destroy ENTRY) is certainly retained by a route.

    Runtime route insertion precedes the recorded create return, and removal
    can precede the recorded destroy return. Counting through destroy return
    would therefore invent a simultaneous-route peak for concurrent originals.
    """
    edges = []
    for value in occurrences.values():
        edges.append((value['create_return'], 1))
        if value['destroy_entry']:
            edges.append((value['destroy_entry'], -1))
    live = peak = 0
    for _, delta in sorted(edges):
        live += delta
        peak = max(peak, live)
    return peak


def _route_bounds(scope):
    # An outstanding destroy's ENTRY exists in the lifecycle rows but its
    # replay state is not finalized until AFTER. Keep these scalar diagnostics
    # unknown rather than counting its route through that unclosed interval.
    state, header, footer = scope['state'], scope['header'], scope['footer']
    if (state['pending_lifecycle'] or state['gap'] or
            not header['enabled'] or not header['build_id_known'] or
            any(footer[key] for key in _GAP_COUNTERS)):
        return dict(instances=None, devices=None)
    return {key: _route_lower_bound_peak(state[key]) for key in ('instances', 'devices')}


def _scope_recording_reasons(scope):
    """Require complete accounting, independently of live-instance count."""
    h, f, state = scope['header'], scope['footer'], scope['state']
    reasons = []
    if not h['enabled'] or not h['build_id_known']:
        reasons.append('observer_or_build_not_known')
    reasons.extend('recording_' + key for key in _GAP_COUNTERS if f[key])
    if state['gap']:
        reasons.append('original_lifetime_gap')
    if state['pending_lifecycle'] or state['pending_calls']:
        reasons.append('pending_original_transition')
    if (f['calls_seen'] != f['original_returns'] or
            f['owner_calls_seen'] != f['owner_original_returns'] or
            f['calls_by_case'] != f['returns_by_case'] or
            sum(f['calls_by_case']) != f['calls_seen']):
        reasons.append('original_call_return_accounting_incomplete')
    if any(not row['original_returned'] for row in scope['lifecycle']):
        reasons.append('lifecycle_original_not_returned')
    if any(not row['original_returned'] or not row['phase_unchanged']
           for row in (*scope['owners'], *scope['queries'])):
        reasons.append('owner_or_query_not_returned_in_observed_phase')
    if f['active_devices'] or f['active_sampler_occurrences']:
        reasons.append('active_device_or_sampler')
    if f['active_instances'] > h['route_capacity']:
        reasons.append('active_instances_exceed_original_route_capacity')
    for key, lower_bound in _route_bounds(scope).items():
        if lower_bound is not None and lower_bound > h['route_capacity']:
            reasons.append('guaranteed_' + key + '_route_lower_bound_exceeds_capacity')
    # Counts of active instances/devices were already recomputed by strict's
    # shared-clock replay. The extra zero requirement above is a new local
    # relation precondition, not a reinterpretation of coverage_complete.
    return reasons


def analyze_closed_call_relation(journals, claims, PE_data, run_id, build_id,
                                 overflow=False):
    """Return a distinct finite selected relation and the exact strict result.

    Invalid PE/claim framing keeps the strict API's ValueError behavior. An
    invalid journal remains a recorded scope error and prevents a positive;
    no journal is silently omitted. Producer footers and legacy refusal
    reasons are returned verbatim under ``legacy_strict_result``.
    """
    legacy = strict.analyze(journals, claims, PE_data, run_id, build_id,
                            overflow=overflow)
    reasons = []
    if overflow:
        reasons.append('persistent_scope_overflow')
    if legacy['scope_errors'] or len(legacy['scopes']) != len(claims):
        reasons.append('claim_journal_set_not_fully_validated')
    target = _selected_target(legacy)
    if target is None:
        reasons.append('two_unique_closed_selected_original_pairs_missing')

    scope_status, open_instances = [], []
    # This global fact is independent of whether the selected nonce exists.
    # A wrong nonce may defeat the relation while all recorded lifetimes close.
    all_lifecycle_closed = (bool(legacy['scopes']) and not overflow and
                            not legacy['scope_errors'] and
                            len(legacy['scopes']) == len(claims))
    for scope in legacy['scopes']:
        identity = scope['identity']
        local_reasons = _scope_recording_reasons(scope)
        state = scope['state']
        route_bounds = _route_bounds(scope)
        live = []
        for generation, instance in state['instances'].items():
            if instance['destroy_return']:
                continue
            selected_parent = bool(target and
                                   _identity(identity) == _identity(target['identity']) and
                                   generation == target['Unix_parent_instance_generation'])
            successful = (instance['original_returned'] and instance['result'] == 0 and
                          instance['instance_generation'] == generation and generation > 0 and
                          instance['raw_instance'] > 0 and
                          instance['operation_ordinal'] < instance['create_return'])
            pending_destroy = bool(instance['destroy_entry']) or any(
                row['kind'] == 'destroy_instance' and
                row['instance_generation'] == generation and not row['original_returned']
                for row in scope['lifecycle'])
            if not successful or pending_destroy or selected_parent:
                local_reasons.append('open_instance_not_a_known_other_occurrence')
            item = dict(identity=dict(identity), instance_generation=generation,
                        create_entry=instance['operation_ordinal'],
                        create_return=instance['create_return'],
                        successful_original_creation_known=bool(successful),
                        pending_destroy=bool(pending_destroy),
                        is_selected_parent=selected_parent)
            live.append(item)
            open_instances.append(item)
        if len(live) != scope['footer']['active_instances']:
            local_reasons.append('open_instance_replay_count_mismatch')
        if live or local_reasons:
            all_lifecycle_closed = False
        reasons.extend('scope_' + str(identity['scope_slot']) + ':' + reason
                       for reason in local_reasons)
        scope_status.append(dict(identity=dict(identity),
                                 recorded_originals_fully_accounted_for=not local_reasons,
                                 guaranteed_route_peaks_known=
                                 all(value is not None for value in route_bounds.values()),
                                 guaranteed_instance_route_peak_lower_bound=
                                 route_bounds['instances'],
                                 guaranteed_device_route_peak_lower_bound=
                                 route_bounds['devices'],
                                 open_instance_generations=[x['instance_generation'] for x in live],
                                 producer_coverage_complete=scope['footer']['coverage_complete'],
                                 reasons=local_reasons))

    report = dict(schema_version=1, purpose=PURPOSE, run_id=run_id,
                closed_selected_original_call_relation_known=not reasons,
                all_saved_scopes_lifecycle_closed=bool(all_lifecycle_closed),
                legacy_strict_result=legacy, target=target,
                open_instances=open_instances, scope_status=scope_status,
                reasons=reasons,
                relation_scope='only_the_two_selected_original_nonce_marker_query_pairs',
                producer_footers_reinterpreted=False,
                unrelated_instance_origin_inferred=False,
                source_contract_is_loaded_binary_attestation=False,
                Windows_PID_used_as_host_identity=False, PE_device_equality_used=False,
                serialization_limit_bytes=MAX_REPORT_BYTES,
                serialization_policy='compact_ASCII_JSON_encoded_as_UTF8',
                report_truncated=False,
                **{key: False for key in FALSE_FACTS})
    return _bounded_report(report)
