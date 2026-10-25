#!/usr/bin/env python3
"""Resolve bound UBO sources in the closed, owned stereo MRT fixture only.

This consumes a caller-owned immutable storage copy, never a mapped pointer.
The fixture contract supplies writer exclusion and catalog completeness; neither
is inferred from a passive trace, endpoint equality, or a single byte match.
This is not a game capture adapter or a proof of dynamic shader accesses.
"""
from __future__ import annotations
from collections import Counter
import argparse
import json
from pathlib import Path
import re
import frame_trace


def _require(value, message):
    if not value:
        raise ValueError(message)


def _uint(value, limit=2**64):
    return type(value) is int and 0 <= value < limit


def _bytes(value, limit):
    _require(isinstance(value, str) and 0 < len(value) <= limit*2 and len(value)%2 == 0
             and re.fullmatch('[0-9a-f]+', value), 'Invalid bounded storage encoding')
    return bytes.fromhex(value)


def resolve_owned_fixture(data: dict, oracle: dict) -> dict:
    """Input frame must first pass frame_trace.read_frame. IDs stay process-local.

    Deliberately supports only the fixture's four sets (three empty, set3 with
    two scalar UBOs), monolithic layouts, one storage buffer and one command
    buffer in one successful submission. Unknown state stays unresolved.
    """
    storage = oracle.get('owned_storage', {})
    _require(oracle.get('schema_version') == 1 and storage.get('scope') == 'owned_synthetic_fixture'
             and storage.get('coherent_exclusive_immutable_storage') is True
             and storage.get('completed_submissions') == 8, 'Owned immutable fixture contract absent')
    payload = _bytes(storage.get('encoding_hex'), 1024*1024)
    _require(payload == _bytes(storage.get('after_encoding_hex'), 1024*1024), 'Storage changed across completed work')
    width, alignment, stride = (storage.get(k) for k in ('uniform_descriptor_size', 'offset_alignment', 'set_stride'))
    _require(_uint(width,4097) and width > 0 and _uint(alignment) and alignment > 0
             and alignment & (alignment-1) == 0 and _uint(stride) and stride > 0
             and stride%alignment == 0 and len(payload) == stride*3, 'Invalid fixture descriptor properties')
    _require(data.get('truncated') is False and data.get('memory_observations_truncated') is False
             and data.get('descriptor_query_catalog_truncated') is False
             and data.get('descriptor_queries_observed') == 7, 'Incomplete fixture observation')
    receipts = data.get('descriptor_query_receipts', [])
    queries = oracle.get('queries', [])
    _require(len(receipts) == len(queries) == 7
             and [{k:r[k] for k in ('type','range_bytes','encoding_hex')} for r in receipts] == queries
             and [r['type'] for r in receipts] == [6]*6+[2], 'Frame and owned fixture catalog differ')
    encodings = [_bytes(r['encoding_hex'],4096) for r in receipts[:6]]
    _require(all(len(b) == width for b in encodings), 'Descriptor property and query size differ')
    writes = storage.get('query_write_offsets', [])
    _require(len(writes) == 6 and all(_uint(o) and o+width <= len(payload) for o in writes)
             and all(payload[o:o+width] == b for o,b in zip(writes,encodings)), 'Owned write slots differ from query outputs')
    layouts = {p['snapshot_id']:p for p in data['pipeline_layouts']}
    sets = {s['snapshot_id']:s for s in data['descriptor_set_layouts']}
    pipelines = {p['id']:p for p in data['pipelines']}
    memory = {m['buffer_id']:m for m in data['buffer_memory']}
    submissions = data['submissions']
    _require(len(submissions) == 1 and submissions[0]['result'] == 0 and submissions[0]['complete'] is True
             and len(submissions[0]['commands']) == 1, 'Only one complete owned submission is supported')
    command = submissions[0]['commands'][0]
    _require(not command['missing'] and not command['truncated'], 'Incomplete owned command recording')
    events = command.get('resource_events', [])
    _require(events and all(not e['truncated'] for e in events), 'Incomplete resource event sequence')
    ids = {b['buffer_id'] for e in events if e['kind'] == 'descriptor_buffers' for b in e['bindings']}
    _require(len(ids) == 1, 'Fixture must own exactly one observed descriptor storage buffer')
    storage_id = next(iter(ids))

    def coherent_owned(buffer_id):
        m = memory.get(buffer_id, {})
        return (m.get('status') == 3 and m.get('allocation_live') is True and m.get('properties_known') is True
                and m.get('property_flags',0)&6 == 6 and m.get('binding_complete') is True
                and m.get('allocation_chain_complete') is True and m.get('buffer_payload_fully_mapped') is True
                and m.get('mapping_complete') is True and m.get('mapped') is True)

    _require(coherent_owned(storage_id) and memory[storage_id]['buffer_size'] == len(payload), 'Storage mapping provenance differs from fixture')

    def profile(layout):
        if not layout or not layout['definition_complete'] or layout['flags'] or layout['set_count'] != 4:
            return None
        group = [sets.get(s['snapshot_id']) if not s['null_slot'] else None for s in layout['sets']]
        if (len(group) != 4 or any(not s or not s['definition_complete'] or s['flags'] != 16 for s in group)
                or any(s['bindings'] for s in group[:3])):
            return None
        last = group[3]
        if (not last['size_known'] or not 0 < last['size_bytes'] <= stride or len(last['bindings']) != 2
                or [(b['binding'],b['type'],b['count'],b['flags'],b['stages']) for b in last['bindings']] != [(0,6,1,0,33),(1,6,1,0,33)]
                or any(b['immutable_samplers_unresolved'] or not b['offset_known']
                       or b['offset_bytes']+width > last['size_bytes'] for b in last['bindings'])):
            return None
        return group

    bound = []
    state = {0:None, 1:None}
    resolved = []
    for e in events:
        kind = e['kind']
        if kind == 'descriptor_buffers':
            bound = e['bindings'] if e['count'] == len(e['bindings']) == 1 else []
            # Rebinding descriptor buffers invalidates offsets at both points.
            state = {0:None, 1:None}
        elif kind in ('descriptor_offsets','classic_descriptors'):
            point = e['bind_point']
            _require(point in state, 'Unsupported bind point')
            # Only the exact fixture set update is supported. Partial or other
            # layouts conservatively clear the point; no compatibility guess.
            state[point] = (e if kind == 'descriptor_offsets' and e['first_set'] == 3
                            and e['count'] == len(e['indices']) == len(e['offsets']) == 1 else None)
        elif kind in ('draw','dispatch'):
            row = {'event_index':e['observed_event_index'], 'kind':kind, 'pipeline_id':e['pipeline_id'],
                   'status':'unresolved_binding_state', 'sources':[]}
            resolved.append(row)
            active = state[1 if kind == 'dispatch' else 0]
            p = pipelines.get(e['pipeline_id'], {})
            pipeline_layout = layouts.get(p.get('layout_snapshot_id'))
            if not active or not pipeline_layout or p.get('missing_libraries'):
                continue
            layout = layouts.get(active['layout_snapshot_id'])
            group = profile(layout)
            pstate = p.get('pipeline_state')
            if (not group or pipeline_layout['id'] != layout['id'] or not pipeline_layout['definition_complete']
                    or (kind == 'draw' and (not pstate or not pstate.get('declarations_complete')))
                    or (pstate and (pstate['linked_libraries'] or pstate['is_library']))):
                row['status'] = 'unsupported_layout'; continue
            original_sets = pipeline_layout['sets']
            if (len(original_sets) != 4 or any(s['null_slot'] or sets[s['snapshot_id']]['id'] != g['id'] for s,g in zip(original_sets,group))):
                row['status'] = 'unsupported_layout'; continue
            if any(writes[s*2+b] != s*stride+group[3]['bindings'][b]['offset_bytes'] for s in range(3) for b in range(2)):
                row['status'] = 'unsupported_write_layout'; continue
            index, offset = active['indices'][0], active['offsets'][0]
            if (not _uint(index) or index >= len(bound) or not _uint(offset) or offset%alignment):
                row['status'] = 'unresolved_binding_state'; continue
            binding = bound[index]
            if (binding['status'] != 1 or binding['buffer_id'] != storage_id or not binding['creation_chain_complete']
                    or binding['binding_usage'] != 4194304 or binding['buffer_usage']&4194304 == 0):
                row['status'] = 'unresolved_storage'; continue
            base = binding['address_offset']+offset
            if base+group[3]['size_bytes'] > len(payload) or offset+group[3]['size_bytes'] > binding['remaining_bytes']:
                row['status'] = 'storage_out_of_bounds'; continue
            row['status'] = 'resolved_owned_sources'
            for definition in group[3]['bindings']:
                start = base+definition['offset_bytes']
                candidates = [r for r,b in zip(receipts[:6],encodings) if b == payload[start:start+width]]
                if len(candidates) != 1:
                    row['status'] = 'ambiguous_encoding' if candidates else 'unknown_encoding'; break
                r = candidates[0]; source = r['buffer']
                if (r['kind'] != 1 or not r['input_chain_complete'] or r['null_descriptor'] or r['input_pointer_null']
                        or source['status'] != 1 or not source['creation_chain_complete'] or not r['range_within_observed_buffer']
                        or not coherent_owned(source['buffer_id']) or memory[source['buffer_id']]['buffer_size'] != source['size']):
                    row['status'] = 'unresolved_source'; break
                row['sources'].append({'set':3,'binding':definition['binding'],'receipt_id':r['id'],
                    'buffer_id':source['buffer_id'],'buffer_offset':source['address_offset'],'range_bytes':r['range_bytes']})
            if row['status'] != 'resolved_owned_sources':
                row['sources'] = []  # A partial pair is never a resolved event.
        else:
            raise ValueError('Unsupported event in closed fixture profile')
    counts = dict(Counter(r['status'] for r in resolved))
    return {'schema_version':1,'scope':'owned_synthetic_fixture','storage_bytes':len(payload),
            'completed_immutable_storage_checks':storage['completed_submissions'],
            'event_status_counts':counts,'events':resolved,
            'game_capture_supported':False,'dynamic_shader_accesses_proven':False,
            'resource_content_captured':False,'resource_aliases_excluded':False,
            'game_resource_admission':False,'game_stereo_submitted':False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('frame',type=Path); parser.add_argument('owned_oracle',type=Path)
    args = parser.parse_args()
    _require(args.owned_oracle.stat().st_size <= 5*1024*1024, 'Owned oracle exceeds budget')
    result = resolve_owned_fixture(frame_trace.read_frame(args.frame),json.loads(args.owned_oracle.read_text()))
    print(json.dumps({k:v for k,v in result.items() if k != 'events'},indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError,ValueError,KeyError,TypeError) as error:
        raise SystemExit(f'D4VR owned descriptors: {error}')
