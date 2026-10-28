#!/usr/bin/env python3
"""Read private bounded Vulkan metadata/descriptor receipts; summarize without raw encodings."""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
import os
from pathlib import Path
import re
import sys
import tempfile

import probe

MAX_BYTES = 8 * 1024 * 1024
DRAW_NAMES = ('draw', 'indexed', 'indirect', 'indexed_indirect', 'indirect_count',
              'indexed_indirect_count', 'generated_ext', 'generated_nv')
ADDRESS_STATUS=('unknown','unique_observed_range','ambiguous','invalid','incomplete_registry')

def unsigned(value,bits=64):
    return type(value) is int and 0<=value<2**bits


MEMORY_STATUS=('unknown_buffer','binding_not_observed','unknown_allocation','observed_binding','invalid_bounds','incomplete_registry','released_allocation','indeterminate_binding')

def memory_observations(data: dict) -> None:
    rows=data.get('buffer_memory',[]);atom=data.get('non_coherent_atom_size',0)
    if (len(rows)>4096 or not unsigned(atom) or type(data.get('memory_properties_known',False)) is not bool
            or type(data.get('memory_observations_truncated',False)) is not bool
            or ('buffer_memory' in data and data.get('memory_observation_scope')!='last_recorded_cpu_submission')):
        raise ValueError('Invalid memory observation scope/budget')
    seen=set()
    numbers=('buffer_id','buffer_size','memory_id','allocation_size','binding_offset','map_generation','map_offset','map_size','flush_calls','invalidate_calls',
             'last_flush_offset','last_flush_size','last_invalidate_offset','last_invalidate_size')
    flags=('binding_complete','allocation_chain_complete','properties_known','allocation_live','mapped','mapping_complete','buffer_payload_fully_mapped','last_flush_range_known','last_invalidate_range_known')
    for s in rows:
        if (any(not unsigned(s[k]) for k in numbers) or any(not unsigned(s[k],32) for k in ('memory_type','property_flags','allocation_flags','device_mask'))
                or any(type(s[k]) is not bool for k in flags) or s.get('gpu_visibility_proven') is not False or not s['buffer_id'] or s['buffer_id'] in seen
                or type(s['status']) is not int or s['status'] not in range(len(MEMORY_STATUS))):
            raise ValueError('Invalid memory provenance observation')
        seen.add(s['buffer_id']);known=s['status'] in (3,4,6)
        if bool(s['memory_id'])!=known or (known and not s['allocation_size']) or (not known and (s['allocation_live'] or s['properties_known'] or s['mapped'])):
            raise ValueError('Memory observation invented an allocation')
        if s['properties_known'] and (not data.get('memory_properties_known',False) or s['memory_type']>=32):raise ValueError('Unknown memory type properties claimed')
        if not s['properties_known'] and s['property_flags']:raise ValueError('Unknown allocation invented memory flags')
        fits=s['binding_offset']<=s['allocation_size'] and s['buffer_size']<=s['allocation_size']-s['binding_offset']
        if (s['status']==3 and (not fits or not s['allocation_live'])) or (s['status']==4 and (fits or not s['allocation_live'])) or (s['status']==6 and s['allocation_live']):
            raise ValueError('Memory binding bounds/lifetime disagree')
        if s['mapped'] and s['mapping_complete'] and (not s['map_generation'] or s['map_offset']>=s['allocation_size'] or not s['map_size'] or s['map_size']>s['allocation_size']-s['map_offset']):
            raise ValueError('Invalid observed mapping bounds')
        covered=s['status']==3 and s['mapped'] and s['mapping_complete'] and s['binding_offset']>=s['map_offset'] and s['binding_offset']-s['map_offset']<=s['map_size'] and s['buffer_size']<=s['map_size']-(s['binding_offset']-s['map_offset'])
        if s['buffer_payload_fully_mapped']!=covered:raise ValueError('Mapping coverage claim disagrees with bounds')
        for name,calls in (('flush','flush_calls'),('invalidate','invalidate_calls')):
            if s[f'last_{name}_range_known']:
                off=s[f'last_{name}_offset'];size=s[f'last_{name}_size']
                if (not s[calls] or not s['mapped'] or not s['mapping_complete'] or not atom or off%atom or not size
                        or off<s['map_offset'] or off-s['map_offset']>s['map_size'] or size>s['map_size']-(off-s['map_offset'])
                        or (size%atom and off+size!=s['allocation_size'])):
                    raise ValueError('Invalid cache-operation observation')


def descriptor_receipts(data: dict) -> None:
    rows=data.get('descriptor_query_receipts',[]);queries=data.get('descriptor_queries_observed',0)
    if (len(rows)>4096 or not unsigned(queries) or len(rows)>queries
            or type(data.get('descriptor_query_catalog_truncated',False)) is not bool
            or data.get('descriptor_query_selection_proven',False) is not False):
        raise ValueError('Invalid descriptor query catalog or unsupported selection claim')
    total=last=0
    for index,r in enumerate(rows):
        if (type(r.get('id')) is not int or r['id']!=index+1 or not unsigned(r.get('first_observed_query')) or not last<r['first_observed_query']<=queries
                or not unsigned(r.get('type'),32) or type(r.get('kind')) is not int or r['kind'] not in range(4)):
            raise ValueError('Invalid descriptor query identity/order')
        last=r['first_observed_query'];n=r['encoding_bytes'];encoding=r['encoding_hex']
        if (not unsigned(n) or not 1<=n<=4096 or not isinstance(encoding,str) or len(encoding)!=n*2 or not re.fullmatch('[0-9a-f]+',encoding)):
            raise ValueError('Invalid exact descriptor encoding')
        total+=n
        if any(type(r.get(k)) is not bool for k in ('input_chain_complete','input_pointer_null','null_descriptor','range_within_observed_buffer','sampler_used','sampler_present')) or r.get('sampler_identity_captured') is not False:
            raise ValueError('Invalid descriptor query flags or unsupported sampler identity')
        kind=1 if r['type'] in (4,5,6,7) else 2 if r['type'] in (1,2,3,10) else 3 if r['type']==0 else 0
        if r['kind']!=kind or r['sampler_used']!=(r['type'] in (0,1)) or (r['sampler_present'] and not r['sampler_used']):
            raise ValueError('Descriptor union selection disagrees with its type')
        if not unsigned(r['range_bytes']) or not unsigned(r['format'],32):raise ValueError('Invalid descriptor range/format')
        if r['type'] in (6,7) and r['format']:raise ValueError('Ignored buffer format was captured')
        b=r['buffer'];v=r['view']
        if type(b.get('status')) is not int or b['status'] not in range(len(ADDRESS_STATUS)) or type(b.get('creation_chain_complete')) is not bool:
            raise ValueError('Invalid descriptor source buffer status')
        if any(not unsigned(b[k]) for k in ('buffer_id','size','buffer_usage','address_offset','remaining_bytes')):
            raise ValueError('Invalid descriptor source buffer bounds')
        if b['status']==1:
            if not b['buffer_id'] or not b['size'] or b['address_offset']>=b['size'] or b['remaining_bytes']!=b['size']-b['address_offset']:
                raise ValueError('Descriptor source buffer bounds disagree')
        elif any(b[k] for k in ('buffer_id','size','address_offset','remaining_bytes')):
            raise ValueError('Unresolved descriptor source invented a buffer owner')
        fits=b['status']==1 and 0<r['range_bytes']<2**64-1 and r['range_bytes']<=b['remaining_bytes']
        if r['range_within_observed_buffer']!=fits:raise ValueError('Descriptor range fit claim disagrees with bounds')
        if (any(not unsigned(v[k]) for k in ('view_id','image_id','layout')) or any(type(v[k]) is not bool for k in ('view_known','image_known'))
                or bool(v['view_id'])!=v['view_known'] or bool(v['image_id'])!=v['image_known'] or (v['image_known'] and not v['view_known'])):
            raise ValueError('Invalid descriptor source image/view identity')
        if ((kind!=1 and (r['range_bytes'] or b['status'] or b['buffer_id'])) or (kind!=2 and (v['view_id'] or v['image_id'] or v['layout']))
                or (r['null_descriptor'] and (b['buffer_id'] or v['view_id'])) or (kind in (1,2) and r['input_pointer_null'] and not r['null_descriptor'])):
            raise ValueError('Descriptor source fields disagree with the selected resource')
    if total>1024*1024:raise ValueError('Descriptor query byte budget exceeded')


def layout_tables(data: dict) -> dict:
    sets=data.get('descriptor_set_layouts',[]);pipelines=data.get('pipeline_layouts',[])
    if len(sets)+len(pipelines)>65536:raise ValueError('Layout snapshot budget exceeded')
    seen=set();set_ids={};pipeline_ids={};words=0
    for rows,dest in ((sets,set_ids),(pipelines,pipeline_ids)):
        for row in rows:
            sid=row.get('snapshot_id')
            if (not unsigned(sid) or not sid or sid in seen or not unsigned(row.get('id')) or not row['id']
                    or not unsigned(row.get('flags'),32) or type(row.get('definition_complete')) is not bool):
                raise ValueError('Invalid or duplicate layout identity')
            seen.add(sid);dest[sid]=row
    for s in sets:
        bindings=s['bindings'];words+=16+len(bindings)*8
        if (len(bindings)>4096 or not unsigned(s['binding_count'],32) or type(s['size_known']) is not bool or not unsigned(s['size_bytes'])
                or (not s['size_known'] and s['size_bytes']) or len(bindings)>s['binding_count']):
            raise ValueError('Invalid set layout bounds')
        numbers=[]
        for b in bindings:
            if (any(not unsigned(b[k],32) for k in ('binding','type','count','stages','flags')) or not unsigned(b['offset_bytes'])
                    or type(b['offset_known']) is not bool or type(b['immutable_samplers_unresolved']) is not bool
                    or (not b['offset_known'] and b['offset_bytes'])):
                raise ValueError('Invalid descriptor layout binding')
            numbers.append(b['binding'])
        if numbers!=sorted(numbers):raise ValueError('Unsorted descriptor layout bindings')
        if s['definition_complete'] and (len(bindings)!=s['binding_count'] or len(set(numbers))!=len(numbers) or any(b['immutable_samplers_unresolved'] for b in bindings)):
            raise ValueError('Incomplete set layout claims complete definition')
    for p in pipelines:
        slots=p['sets'];push=p['push_ranges'];words+=16+len(slots)*2+len(push)*3
        if (len(slots)>32 or len(push)>32 or not unsigned(p['set_count'],32) or not unsigned(p['push_count'],32)
                or len(slots)>p['set_count'] or len(push)>p['push_count']):
            raise ValueError('Pipeline layout array budget exceeded')
        for slot in slots:
            sid=slot['snapshot_id']
            if (not unsigned(sid) or type(slot['null_slot']) is not bool or (slot['null_slot'] and sid) or (sid and sid not in set_ids)):
                raise ValueError('Invalid set layout reference')
        if any(any(not unsigned(r[k],32) for k in ('stages','offset','size')) for r in push):raise ValueError('Invalid push range')
        if p['definition_complete'] and (len(slots)!=p['set_count'] or len(push)!=p['push_count']
                or any(not slot['null_slot'] and (not slot['snapshot_id'] or not set_ids[slot['snapshot_id']]['definition_complete']) for slot in slots)):
            raise ValueError('Incomplete pipeline layout claims complete definition')
    if words>1024*1024:raise ValueError('Layout word budget exceeded')
    return pipeline_ids


def resource_event(event: dict,index: int,operations: int,truncated: bool,layouts: dict) -> None:
    kinds=('descriptor_buffers','descriptor_offsets','classic_descriptors','draw','dispatch')
    if (event.get('kind') not in kinds or type(event.get('observed_event_index')) is not int or event.get('observed_event_index')!=index
            or type(event.get('layout_identity_captured')) is not bool or type(event.get('truncated')) is not bool):
        raise ValueError('Invalid resource observation')
    sid=event.get('layout_snapshot_id',0)
    if (not unsigned(sid) or bool(sid)!=event['layout_identity_captured'] or (sid and (sid not in layouts
            or event['kind'] not in ('descriptor_offsets','classic_descriptors') or (not layouts[sid]['definition_complete'] and not event['truncated'])))):
        raise ValueError('Invalid resource layout identity')
    for key in ('operation_count','pipeline_id','draw_kind','bind_point','first_set','count'):
        if type(event.get(key)) is not int or not 0<=event[key]<2**64:
            raise ValueError('Invalid resource observation integer')
    if not truncated and event['operation_count']>operations:
        raise ValueError('Resource observation refers outside recorded operations')
    if event['kind']=='draw' and event['draw_kind']>=8:
        raise ValueError('Unknown observed draw kind')
    bindings=event.get('bindings',[]);indices=event.get('indices',[]);offsets=event.get('offsets',[])
    if len(bindings)>32 or len(indices)>32 or len(indices)!=len(offsets):
        raise ValueError('Resource observation array budget exceeded')
    if any(type(x) is not int or not 0<=x<2**32 for x in indices) or any(type(x) is not int or not 0<=x<2**64 for x in offsets):
        raise ValueError('Invalid descriptor buffer index/offset')
    for b in bindings:
        if type(b.get('status')) is not int or b['status'] not in range(len(ADDRESS_STATUS)) or type(b.get('creation_chain_complete')) is not bool:
            raise ValueError('Invalid observed buffer status')
        for key in ('buffer_id','size','buffer_usage','binding_usage','address_offset','remaining_bytes'):
            if type(b.get(key)) is not int or not 0<=b[key]<2**64:
                raise ValueError('Invalid observed buffer range')
        if b['status']==1:
            if not b['buffer_id'] or not b['size'] or b['address_offset']>=b['size'] or b['remaining_bytes']!=b['size']-b['address_offset']:
                raise ValueError('Observed buffer bounds disagree')
        elif any(b[key] for key in ('buffer_id','size','address_offset','remaining_bytes')):
            raise ValueError('Unresolved buffer observation invented an owner')


def fnv1a64(data: bytes) -> str:
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f'{value:016x}'


def request(directory: Path) -> int:
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('Expected an existing private capture directory')
    capture = json.loads((directory / 'capture.json').read_text(encoding='utf-8'))
    if capture.get('frame_trace_requested') is not True or capture.get('phase') != 'launching':
        raise ValueError('An active launch with --frame-trace is required')
    path = directory / 'frame.request'
    if path.is_symlink() or (path.exists() and path.stat().st_size > 64):
        raise ValueError('Invalid request file')
    sequence = int(path.read_text(encoding='ascii')) + 1 if path.exists() else 1
    if not 1 <= sequence <= 4:
        raise ValueError('At most four requests per device/session are supported')
    fd, name = tempfile.mkstemp(prefix='.frame-request-', dir=directory)
    try:
        with os.fdopen(fd, 'w', encoding='ascii') as stream:
            stream.write(str(sequence) + '\n')
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)
    return sequence


def read_frame(path: Path) -> dict:
    if path.is_symlink() or path.stat().st_size > MAX_BYTES:
        raise ValueError('Invalid or oversized frame metadata')
    data = json.loads(path.read_text(encoding='utf-8'))
    if data.get('schema_version') != 1 or data.get('game_stereo_submitted') is not False:
        raise ValueError('Unsupported metadata schema')
    if data.get('descriptor_contents_captured',False) is not False or data.get('resource_aliases_excluded',False) is not False:
        raise ValueError('Resource contents and alias exclusion are not part of this schema')
    if data.get('scope') not in ('single_submission', 'cpu_submission_interval_between_present_calls'):
        raise ValueError('Unknown capture scope')
    layouts=layout_tables(data)
    descriptor_receipts(data)
    memory_observations(data)
    algorithm = data.get('shader_identifier_algorithm', '')
    if algorithm and not re.fullmatch('[0-9a-f]{32}', algorithm):
        raise ValueError('Invalid shader identifier algorithm')
    pipelines = data['pipelines']
    if len(pipelines) > 65536 or len({p['id'] for p in pipelines}) != len(pipelines):
        raise ValueError('Duplicate pipelines or pipeline budget exceeded')
    for p in pipelines:
        sid=p.get('layout_snapshot_id',0)
        if not unsigned(sid) or (sid and sid not in layouts):raise ValueError('Invalid shader pipeline layout reference')
        state = p.get('pipeline_state')
        if state is not None:
            blocks = state['declarations']
            if len(blocks) > 32 or any(type(state[k]) is not bool for k in
                    ('is_library','linked_libraries','declarations_complete','effective_dynamic_states_known','graphics_state_snapshot_complete')):
                raise ValueError('Invalid pipeline state declarations')
            if state['graphics_state_snapshot_complete']:
                raise ValueError('Full graphics state is not part of this metadata schema')
            for block in blocks:
                if (len(block['states']) > 64 or any(type(v) is not int or not 0 <= v < 2**31 for v in block['states'])
                        or type(block['truncated']) is not bool):
                    raise ValueError('Invalid dynamic state declaration block')
            if state['declarations_complete'] and any(b['truncated'] for b in blocks):
                raise ValueError('Truncated declarations cannot be complete')
            if state['effective_dynamic_states_known'] and (not state['declarations_complete'] or state['is_library'] or state['linked_libraries']
                    or state['own_parts'] != 15 or len(blocks) != 1 or blocks[0]['truncated']):
                raise ValueError('Linked or incomplete declarations cannot identify effective states')
            if state['effective_dynamic_states_known'] and len(set(blocks[0]['states'])) != len(blocks[0]['states']):
                raise ValueError('Duplicate effective dynamic state declarations')
        if len(p['shaders']) > 512:
            raise ValueError('Shader metadata budget exceeded')
        for s in p['shaders']:
            if not re.fullmatch('[0-9a-f]{16}', s['fnv1a64']) or not 0 <= s['code_bytes'] <= 64 * 1024 * 1024:
                raise ValueError('Invalid shader fingerprint')
            identifier = s.get('module_identifier', '')
            if identifier and (not re.fullmatch('[0-9a-f]+', identifier) or len(identifier) > 64 or len(identifier) % 2):
                raise ValueError('Invalid module identifier')
    operations = 0
    if len(data['submissions']) > 256:
        raise ValueError('Submission budget exceeded')
    for submit in data['submissions']:
        if len(submit['commands']) > 256:
            raise ValueError('Command-buffer budget exceeded')
        for command in submit['commands']:
            operations += len(command['operations'])
            events=command.get('resource_events',[])
            if len(events)>2048:raise ValueError('Resource event budget exceeded')
            for index,event in enumerate(events):resource_event(event,index,len(command['operations']),command['truncated'],layouts)
            for op in command['operations']:
                if len(op.get('colors', [])) > 16 or len(op.get('draws', [])) > 512:
                    raise ValueError('Pass metadata budget exceeded')
                for attachment in op.get('colors', []) + [op[r] for r in ('depth','stencil') if op.get(r)]:
                    words = attachment.get('clear_value_words')
                    if words is not None and (attachment.get('load') != 1 or len(words) != 4
                            or any(type(v) is not int or not 0 <= v < 2**32 for v in words)):
                        raise ValueError('Invalid clear value words')
                for draw in op.get('draws', []):
                    if len(draw['counts']) != 8 or any(type(v) is not int or v < 0 for v in draw['counts']):
                        raise ValueError('Invalid draw counts')
    if operations > 8192:
        raise ValueError('Operation budget exceeded')
    return data


def correlate(data: dict, directory: Path | None, camera_chain: Path | None = None,
              identifier_catalog: Path | None = None) -> dict:
    """FNV+size candidates only; no shader bytecode is emitted or copied."""
    wanted = {(s['fnv1a64'], s['code_bytes']) for p in data['pipelines']
              for s in p['shaders'] if s['known']}
    sizes = {size for _, size in wanted}
    matches = defaultdict(list)
    files = sorted(directory.glob('*.spv')) if directory else []
    if len(files) > 32768:
        raise ValueError('Shader catalog file budget exceeded')
    scanned_bytes = 0
    for path in files:
        if path.is_symlink() or not re.fullmatch('[0-9a-fA-F]{16}\\.spv', path.name):
            continue
        size = path.stat().st_size
        if size not in sizes:
            continue
        scanned_bytes += size
        if scanned_bytes > 1024 * 1024 * 1024:
            raise ValueError('Shader catalog byte budget exceeded')
        fingerprint = (fnv1a64(path.read_bytes()), size)
        if fingerprint in wanted:
            matches[fingerprint].append(path.stem.lower())
    id_matches = defaultdict(list)
    identifier_rows = []
    if identifier_catalog:
        if identifier_catalog.is_symlink() or identifier_catalog.stat().st_size > MAX_BYTES:
            raise ValueError('Invalid implementation identifier catalog')
        catalog = json.loads(identifier_catalog.read_text(encoding='utf-8'))
        if (catalog.get('schema_version') != 1 or not data.get('shader_identifier_algorithm') or
                catalog.get('shader_identifier_algorithm') != data['shader_identifier_algorithm']):
            raise ValueError('Shader identifier algorithms differ or are missing')
        identifier_rows = catalog['shaders']
        if len(identifier_rows) > 32768:
            raise ValueError('Identifier catalog budget exceeded')
        for row in identifier_rows:
            name = row['catalog_id'].lower()
            if not re.fullmatch('[0-9a-f]{16}', name):
                raise ValueError('Invalid catalog identifier')
            id_matches[row['module_identifier']].append(name)
            fingerprint = (row['fnv1a64'], row['code_bytes'])
            if fingerprint in wanted and name not in matches[fingerprint]:
                matches[fingerprint].append(name)
    camera = {}
    if camera_chain:
        if camera_chain.stat().st_size > 32 * 1024 * 1024:
            raise ValueError('Camera analysis is oversized')
        camera = {Path(row['file']).stem.lower(): row for row in
                  json.loads(camera_chain.read_text(encoding='utf-8'))['shaders']}
    def candidates_for(names):
        candidates = []
        for name in names:
            row = camera.get(name, {})
            matrix = row.get('final_clip_matrix')
            candidates.append({'catalog_id': name, 'static_final_clip_matrix_found': bool(matrix),
                               'static_input_matrix_chain_found': bool(row.get('input_matrix_chain'))})
        return candidates
    shaders = []
    for fnv, size in sorted(wanted):
        candidates = candidates_for(matches[fnv, size])
        shaders.append({'fnv1a64': fnv, 'code_bytes': size, 'candidates': candidates})
    identifiers = {s.get('module_identifier') for p in data['pipelines'] for s in p['shaders'] if s.get('module_identifier')}
    for identifier in sorted(identifiers):
        shaders.append({'module_identifier': identifier, 'candidates': candidates_for(id_matches[identifier])})
    return {'method': 'FNV-1a64 plus byte length, or implementation module identifier with equal algorithm UUID; candidate associations',
            'catalog_files': len(files), 'catalog_bytes_hashed': scanned_bytes, 'shaders': shaders,
            'identifier_catalog_files': len(identifier_rows), 'module_identifiers_observed': len(identifiers),
            'visible_scene_role_proven': False, 'live_camera_values_proven': False}


def summarize(data: dict, correlation: dict | None = None) -> dict:
    pipeline_ids = {p['id'] for p in data['pipelines']}
    stages = [s for p in data['pipelines'] for s in p['shaders']]
    operations = Counter()
    draw_counts = Counter()
    passes = []
    image_writes = defaultdict(list)
    truncated = bool(data['truncated']) or data.get('descriptor_query_catalog_truncated',False) or data.get('memory_observations_truncated',False)
    missing_commands = unknown_pipeline_commands = failed = 0
    resource_counts=Counter();address_counts=Counter();buffer_ids=set();layout_counts=Counter();layout_ids=set();set_ids=set();queried_sizes=set();queried_offsets=set()
    layouts={p['snapshot_id']:p for p in data.get('pipeline_layouts',[])};sets={s['snapshot_id']:s for s in data.get('descriptor_set_layouts',[])}
    for si, submit in enumerate(data['submissions']):
        if not submit['complete'] or submit['result'] != 0:
            failed += 1
            continue  # Recorded calls in a failed submission are not executions.
        for ci, command in enumerate(submit['commands']):
            missing_commands += bool(command['missing'])
            truncated |= bool(command['truncated'])
            for event in command.get('resource_events',[]):
                resource_counts[event['kind']]+=1;truncated|=event['truncated']
                p=layouts.get(event.get('layout_snapshot_id',0))
                if p:
                    layout_counts[event['kind']]+=1;layout_ids.add(p['id'])
                    for slot in p['sets']:
                        s=sets.get(slot['snapshot_id'])
                        if s:
                            set_ids.add(s['id'])
                            if s['size_known']:queried_sizes.add(s['id'])
                            queried_offsets.update((s['id'],b['binding']) for b in s['bindings'] if b['offset_known'])
                for b in event['bindings']:
                    address_counts[ADDRESS_STATUS[b['status']]]+=1
                    if b['buffer_id']:buffer_ids.add(b['buffer_id'])
            for oi, op in enumerate(command['operations']):
                operations[op['kind']] += 1
                truncated |= bool(op['truncated'])
                if 'draws' not in op:
                    continue
                index = len(passes)
                counts = Counter()
                for draw in op['draws']:
                    counts.update(dict(zip(DRAW_NAMES, draw['counts'])))
                    if draw['pipeline_id'] not in pipeline_ids:
                        unknown_pipeline_commands += sum(draw['counts'])
                draw_counts.update(counts)
                attachments = []
                for slot, a in enumerate(op['colors']):
                    attachments.append({'role': 'color', 'slot': slot, **a['view'], 'load_op':a.get('load'), 'store_op':a.get('store'),
                                        'clear_value_words':a.get('clear_value_words')})
                    if a['resolve_mode']:
                        attachments.append({'role': 'color_resolve', 'slot': slot, **a['resolve_view']})
                for role in ('depth', 'stencil'):
                    if op[role]:
                        attachments.append({'role': role, **op[role]['view'], 'load_op':op[role].get('load'), 'store_op':op[role].get('store'),
                                            'clear_value_words':op[role].get('clear_value_words')})
                for image_id in {a['image_id'] for a in attachments if a['image_id']}:
                    image_writes[image_id].append(index)
                passes.append({'index': index, 'submission': si, 'command': ci, 'operation': oi,
                               'kind': op['kind'], 'area': op['area'], 'draw_commands': sum(counts.values()),
                               'counts': dict(counts), 'pipeline_ids': [d['pipeline_id'] for d in op['draws']],
                               'attachments': attachments, 'scene_role': 'unclassified'})
                passes[-1].update(queue_family=submit.get('queue_family'),queue_index=submit.get('queue_index'),
                                  secondary_depth=command.get('secondary_depth',0),command_id=command.get('id'),command_generation=command.get('generation'))
    memory=data.get('buffer_memory',[]);allocations={m['memory_id']:m for m in memory if m['memory_id']}
    result = {'schema_version': 1, 'request_sequence': data['request_sequence'], 'scope': data['scope'],
              'present_before': data['present_before'], 'present_after': data['present_after'],
              'submission_count': len(data['submissions']), 'failed_or_incomplete_submissions': failed,
              'operation_counts': dict(operations), 'draw_command_counts': dict(draw_counts),
              'resource_observation_counts':dict(resource_counts),'buffer_address_status_counts':dict(address_counts),'observed_descriptor_buffer_ids':sorted(buffer_ids),
              'descriptor_contents_captured':False,'resource_aliases_excluded':False,
              'layout_observation_counts':dict(layout_counts),'observed_pipeline_layout_ids':sorted(layout_ids),'observed_set_layout_ids':sorted(set_ids),
              'descriptor_layout_query_counts':{'sizes':len(queried_sizes),'binding_offsets':len(queried_offsets)},
              'descriptor_query_counts':{'observed_calls':data.get('descriptor_queries_observed',0),'owned_receipts':len(data.get('descriptor_query_receipts',[])),
                  'encoding_bytes':sum(r['encoding_bytes'] for r in data.get('descriptor_query_receipts',[])),
                  'resource_kinds':dict(Counter(r['kind'] for r in data.get('descriptor_query_receipts',[])))},
              'descriptor_query_catalog_truncated':data.get('descriptor_query_catalog_truncated',False),'descriptor_query_selection_proven':False,
              'memory_observation_counts':dict(Counter(MEMORY_STATUS[m['status']] for m in memory)),
              'memory_observations_truncated':data.get('memory_observations_truncated',False),'memory_gpu_visibility_proven':False,
              'memory_allocation_count':len(allocations),'memory_fully_mapped_buffer_payloads':sum(m['buffer_payload_fully_mapped'] for m in memory),
              'memory_cache_call_counts':{'flush':sum(m['flush_calls'] for m in allocations.values()),'invalidate':sum(m['invalidate_calls'] for m in allocations.values())},
              'truncated': truncated, 'missing_command_buffers': missing_commands,
              'draw_commands_with_unknown_pipeline': unknown_pipeline_commands,
              'pipeline_count': len(pipeline_ids), 'passes': passes,
              'pipeline_state_declarations':[{'pipeline_id':p['id'],**p['pipeline_state']} for p in data['pipelines'] if p.get('pipeline_state') is not None],
              'shader_stage_occurrences': len(stages), 'stages_with_code_fingerprint': sum(s['known'] for s in stages),
              'stages_with_module_identifier': sum(bool(s.get('module_identifier')) for s in stages),
              'stages_without_code_or_identifier': sum(not s['known'] and not s.get('module_identifier') for s in stages),
              'image_attachment_scopes': {str(k): v for k, v in image_writes.items()},
              'gpu_execution_order_known': False, 'texture_content_captured': False,
              'image_reads_and_barriers_traced': False, 'scene_pass_identified': False,
              'game_stereo_submitted': False}
    if correlation is not None:
        result['shader_correlation'] = correlation
        result['shader_keys_matched'] = sum(bool(s['candidates']) for s in correlation['shaders'])
        result['shader_keys_observed'] = len(correlation['shaders'])
        by_code = {(s['fnv1a64'], s['code_bytes']): s['candidates'] for s in correlation['shaders'] if 'fnv1a64' in s}
        by_identifier = {s['module_identifier']: s['candidates'] for s in correlation['shaders'] if 'module_identifier' in s}
        associations = {}
        for p in data['pipelines']:
            candidates = {}
            for s in p['shaders']:
                if s['stage'] != 1:  # Vertex stage; do not infer mesh/RT coverage.
                    continue
                rows = list(by_code.get((s['fnv1a64'], s['code_bytes']), [])) if s['known'] else []
                rows += by_identifier.get(s.get('module_identifier'), [])
                for row in rows:
                    candidates[row['catalog_id']] = row
            associations[p['id']] = {'pipeline_id': p['id'], 'vertex_candidates': list(candidates.values())}
        result['pipeline_associations'] = list(associations.values())
        for item in passes:
            op = data['submissions'][item['submission']]['commands'][item['command']]['operations'][item['operation']]
            item['draw_commands_with_static_projection_candidate'] = sum(sum(d['counts']) for d in op['draws']
                if any(c['static_final_clip_matrix_found'] for c in associations.get(d['pipeline_id'], {}).get('vertex_candidates', [])))
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    arm = sub.add_parser('request'); arm.add_argument('directory', type=Path)
    report = sub.add_parser('report'); report.add_argument('file', type=Path)
    report.add_argument('--shader-directory', type=Path); report.add_argument('--camera-chain', type=Path)
    report.add_argument('--identifier-catalog', type=Path)
    report.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'request':
        print(json.dumps({'request_sequence': request(args.directory)})); return 0
    data = read_frame(args.file)
    correlation = correlate(data, args.shader_directory, args.camera_chain, args.identifier_catalog) if args.shader_directory or args.identifier_catalog else None
    result = summarize(data, correlation); probe.write_json(args.output, result)
    print(json.dumps({k: v for k, v in result.items() if k not in ('passes', 'image_attachment_scopes', 'shader_correlation', 'pipeline_associations','pipeline_state_declarations')}, indent=2))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'D4VR passes : {error}', file=sys.stderr); raise SystemExit(2)
