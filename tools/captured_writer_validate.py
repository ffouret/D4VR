#!/usr/bin/env python3
"""Read-only validation of bounded CPU prefixes carried by selected captures.

This reader never executes a process or dispatches Vulkan. A CPU prefix names
recorded attachments, draws and transfers before one original presentation BLIT;
it does not establish execution order, the last GPU writer, initialized pixels,
descriptor selection, shader reads, complete scene coverage or XR delivery.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import blit_provenance_validate as blit


REF_FIELDS = blit.REF_FIELDS
UNAVAILABLE = {'absent', 'disabled', 'unknown_identity', 'hook_mismatch', 'not_admitted',
               'ticket_limit', 'event_budget', 'reference_budget', 'reset_before_submit',
               'observer_fault', 'observer_destroyed', 'serialization_budget',
               'duplicate_occurrence', 'pending'}
NO_INFERENCE = (*blit.NO_INFERENCE, 'last_gpu_writer_known', 'actual_gpu_write_proven',
                'image_memory_aliasing_observed', 'all_write_routes_observed',
                'gpu_write_proven', 'sampled_consumer_proven')


def claims(value):
    """Check nested observations without promoting declarations to execution."""
    if isinstance(value, list):
        for item in value:
            claims(item)
    elif isinstance(value, dict):
        for key, item in value.items():
            if key in NO_INFERENCE and item is not False:
                raise ValueError('Captured writer prefix asserts unavailable GPU/scene information: '+key)
            if key in ('gpu_commands_inserted', 'application_calls_rejected') and (
                    type(item) is not int or item != 0):
                raise ValueError('Captured writer observation inserted or rejected application work')
            claims(item)


def boolean(value, name):
    if type(value) is not bool:
        raise ValueError('Captured writer status is not boolean: '+name)
    return value


def reference(value, *, known=False):
    if not isinstance(value, dict) or set(value) != set(REF_FIELDS):
        raise ValueError('Captured writer target reference fields changed')
    result = tuple(blit.integer(value[key]) for key in REF_FIELDS)
    if known and (not all(result) or result[1] == result[2]):
        raise ValueError('Captured writer target has unknown or conflated lifetimes')
    return result


def expected_reference(value):
    if not isinstance(value, dict) or not set(REF_FIELDS).issubset(value):
        raise ValueError('Expected original BLIT reference is absent')
    return tuple(blit.integer(value[key]) for key in REF_FIELDS)


def unavailable(value, expected, sequence, *, require_available):
    reason = value.get('unavailable_reason')
    if reason not in UNAVAILABLE:
        raise ValueError('Unknown captured writer unavailability reason')
    if value.get('available') is not False or value.get('frozen') is not False:
        raise ValueError('Unavailable captured writer was asserted as a frozen observation')
    shortened = 'capture_sequence' not in value or 'target_blit' not in value
    if shortened:
        if reason != 'observer_fault':
            raise ValueError('Unavailable captured writer lost its exact target identity')
    default_absence = (not shortened and reason == 'absent'
                       and blit.integer(value['capture_sequence']) == 0 and not any(reference(value['target_blit'])))
    if not shortened and not default_absence and (blit.integer(value['capture_sequence']) != sequence
          or reference(value['target_blit']) != expected):
        raise ValueError('Unavailable captured writer was attached to a different capture')
    if value.get('occurrences') or value.get('catalog') or value.get('images'):
        raise ValueError('Unavailable captured writer contains invented retained observations')
    if require_available:
        raise ValueError('A bounded frozen captured writer observation is required')
    return {'available': False, 'frozen': False, 'unavailable_reason': reason,
            'identity_unavailable_after_serialization_fault': shortened,
            'bound_to_capture': not shortened and not default_absence,
            'capture_sequence': value.get('capture_sequence'), 'event_references': 0, 'cpu_occurrences': 0}


def completion(value):
    complete = boolean(value['complete'], 'complete')
    result = blit.integer(value['result'], -(1 << 31), (1 << 31)-1)
    if not complete and result != 1:  # VK_NOT_READY, not a returned original result.
        raise ValueError('Pending CPU occurrence invented an AFTER result')
    return complete, result


def pipeline_catalog(value):
    if not isinstance(value, list) or len(value) > 64:
        raise ValueError('Captured writer pipeline catalog exceeded its event bound')
    result = {}
    for pipeline in value:
        identity = blit.integer(pipeline['id'], 1)
        if identity in result or pipeline['original_metadata_only'] is not True:
            raise ValueError('Captured writer pipeline identity/metadata origin changed')
        boolean(pipeline['missing_libraries'], 'missing_libraries')
        boolean(pipeline['metadata_truncated'], 'metadata_truncated')
        shaders = pipeline['shaders']
        if not isinstance(shaders, list) or len(shaders) > 16:
            raise ValueError('Captured writer original shader catalog exceeded its bound')
        for shader in shaders:
            blit.integer(shader['stage'], 1, (1 << 32)-1)
            blit.integer(shader['fnv'])
            blit.integer(shader['bytes'])
            boolean(shader['known'], 'shader_known')
            boolean(shader['identifier_truncated'], 'identifier_truncated')
            if not isinstance(shader['identifier'], str) or len(shader['identifier']) > 128:
                raise ValueError('Captured writer shader identifier exceeded its bound')
        result[identity] = pipeline
    return result


def event_scope(event, source):
    """Return explicit declared membership; never a GPU write assertion."""
    kind = event['kind']
    image = blit.integer(event['image'])
    destination = blit.integer(event['destination_image'])
    if kind in ('copy_image', 'blit_image'):
        if destination != source:
            raise ValueError('An outgoing read transfer was relabeled as a captured-source writer')
        return 'incoming_transfer'
    if kind == 'draw':
        colors = event.get('draw_color_images', [])
        views = event.get('draw_color_views', [])
        if not isinstance(colors, list) or not isinstance(views, list) or len(colors) != len(views) or len(colors) > 8:
            raise ValueError('Draw color membership/footprint exceeded its bound')
        for identity in (*colors, *views):
            blit.integer(identity)
        if 'rendering_scope_ordinal' in event:
            scope = blit.integer(event['rendering_scope_ordinal'], 1)
            if scope >= blit.integer(event['ordinal'], 1) or event.get('declared_color_membership_only') is not True:
                raise ValueError('Draw membership was not saved from an earlier explicit rendering scope')
        elif colors:
            raise ValueError('Draw color membership lacks its original rendering scope')
        if image != source and source not in colors:
            raise ValueError('Draw has no explicit membership in the captured source image')
        return 'draw'
    if image != source:
        raise ValueError('Unrelated or unknown image was added to a captured-source writer prefix')
    return 'declared_or_source_context'


def audit_prefix(value, *, original, capture_sequence, history_submission=0,
                 capture_submission=0, require_available=True, require_complete=True):
    """Audit one delivered value against its own original BLIT provenance."""
    if not isinstance(value, dict):
        raise ValueError('Captured writer prefix schema is absent')
    blit.integer(value.get('schema_version'), 1, 1)
    claims(value)
    expected = expected_reference(original)
    sequence = blit.integer(capture_sequence, 1)
    available = boolean(value['available'], 'available')
    shortened_fault = (not available and value.get('unavailable_reason') == 'observer_fault'
                       and ('capture_sequence' not in value or 'target_blit' not in value))
    if value.get('observation_only') is not True and not (shortened_fault and 'observation_only' not in value):
        raise ValueError('Captured writer prefix is not an observation-only receipt')
    if not available:
        return unavailable(value, expected, sequence, require_available=require_available)
    if not value.get('frozen') is True or value.get('unavailable_reason') != 'none':
        raise ValueError('Available captured writer has no immutable BEFORE body')
    if (blit.integer(value['capture_sequence'], 1) != sequence
            or reference(value['target_blit'], known=True) != expected):
        raise ValueError('Captured writer prefix was joined to a different original capture')
    target = blit.integer(value['target_submission'], 1)
    position = (blit.integer(value['target_batch'], 0, 255),
                blit.integer(value['target_command'], 0, 255))
    if value.get('cutoff_exclusive') is not True or blit.integer(value['cutoff_ordinal'], 1) != expected[-1]:
        raise ValueError('Captured writer cutoff differs from the original BLIT command')
    if history_submission and target != blit.integer(history_submission, 1):
        raise ValueError('Captured writer original submission was retagged at delivery')
    if value.get('missing_before_admission') is not True:
        raise ValueError('Captured writer lifetime history before admission was invented')
    prefix_missing = boolean(value['prefix_missing'], 'prefix_missing')
    truncated = boolean(value['truncated'], 'truncated')
    if truncated and not prefix_missing:
        raise ValueError('Truncated captured writer body lost its missing-prefix warning')
    matching = blit.integer(value['matching_original_occurrences'], 1)
    ambiguous = boolean(value['ambiguous'], 'ambiguous')
    if ambiguous is not (matching != 1):
        raise ValueError('Captured writer occurrence ambiguity was hidden')
    complete = boolean(value['original_submit_complete'], 'original_submit_complete')
    result = blit.integer(value['original_submit_result'], -(1 << 31), (1 << 31)-1)
    if not complete and result != 1:
        raise ValueError('Captured writer target invented its original AFTER result')
    if require_complete and (ambiguous or not complete or result != 0):
        raise ValueError('Delivered capture lacks a unique successful original CPU AFTER')
    limits = value['limits']
    maximum_events = blit.integer(limits['events_per_prefix'], 1, 64)
    maximum_occurrences = blit.integer(limits['occurrences_per_prefix'], 1, 4)
    maximum_bytes = blit.integer(limits['serialized_bytes'], 1, 131072)
    if len(json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode()) > maximum_bytes:
        raise ValueError('Captured writer JSON exceeded its complete-object byte bound')
    if value.get('retention') != 'immutable_cpu_values':
        raise ValueError('Captured writer prefix was reconstructed from live resource metadata')
    for key in ('event_nodes_held', 'snapshot_event_references_held'):
        if blit.integer(value[key]) != 0:
            raise ValueError('Delivered writer body unexpectedly pins live journal resources')
    admission = value['admission_start']
    admission_recording = blit.integer(admission['recording'], 1)
    admission_ordinal = blit.integer(admission['ordinal'], 1)
    if (blit.integer(admission['before_observed_submission']) >= target
            or admission_recording == expected[-2] and admission_ordinal > expected[-1]):
        raise ValueError('Captured writer admission was moved past the captured BLIT')
    occurrences = value['occurrences']
    if not isinstance(occurrences, list) or not 1 <= len(occurrences) <= maximum_occurrences:
        raise ValueError('Captured writer CPU occurrence bound exceeded')
    sequences = [blit.integer(item['submission'], 1) for item in occurrences]
    if sequences != sorted(set(sequences)) or sequences[-1] != target:
        raise ValueError('Captured writer contains a duplicate or future original occurrence')
    cpu = value['cpu_completions']
    if not isinstance(cpu, list) or [blit.integer(item['submission'], 1) for item in cpu] != sequences:
        raise ValueError('Captured writer CPU AFTER snapshots differ from saved BEFORE occurrences')
    for item in cpu:
        completion(item)
    if completion(cpu[-1]) != (complete, result):
        raise ValueError('Captured writer target AFTER was borrowed from another occurrence')
    catalog = pipeline_catalog(value['catalog'])
    images = value['images']
    if not isinstance(images, list) or len(images) > 2+64*10:
        raise ValueError('Captured writer image catalog exceeded its event/member bound')
    image_ids = [blit.integer(image['id'], 1) for image in images]
    if len(set(image_ids)) != len(image_ids) or expected[1] not in image_ids:
        raise ValueError('Captured writer exact source image catalog is absent or ambiguous')
    event_count = 0
    draw_count = 0
    incoming_count = 0
    target_commands = []
    for occurrence in occurrences:
        submission = occurrence['submission']
        if 'complete' in occurrence or 'result' in occurrence:
            raise ValueError('Immutable BEFORE body was relabeled with later CPU AFTER state')
        blit.integer(occurrence['capture_submission'])
        boolean(occurrence['truncated'], 'occurrence_truncated')
        commands = occurrence['commands']
        if not isinstance(commands, list) or len(commands) > 256:
            raise ValueError('Captured writer command metadata exceeded its submission bound')
        positions = []
        for command in commands:
            current = (blit.integer(command['batch'], 0, 255), blit.integer(command['command'], 0, 255))
            positions.append(current)
            recording = blit.integer(command['recording'], 1)
            if submission == target:
                if current > position:
                    raise ValueError('A later command contaminated the captured writer prefix')
                if current == position:
                    if recording != expected[-2]:
                        raise ValueError('Captured writer cutoff command has a different recording generation')
                    target_commands.append(command)
                    if capture_submission and occurrence['capture_submission'] != capture_submission:
                        raise ValueError('Captured writer History and capture submission namespaces were conflated')
            events = command['events']
            if not isinstance(events, list):
                raise ValueError('Captured writer events are not a bounded list')
            ordinals = [blit.integer(event['ordinal'], 1) for event in events]
            if ordinals != sorted(ordinals):
                raise ValueError('Captured writer original event ordinals decreased')
            for event in events:
                if submission == target and current == position and event['ordinal'] >= expected[-1]:
                    raise ValueError('Captured BLIT or later event contaminated its exclusive writer prefix')
                scope = event_scope(event, expected[1])
                draw_count += scope == 'draw'
                incoming_count += scope == 'incoming_transfer'
                pipeline = blit.integer(event['original_pipeline_id'])
                if pipeline and pipeline not in catalog:
                    raise ValueError('Captured writer lost its original pipeline catalog entry')
                if event.get('pipeline_substituted') is True and event.get('selected_pipeline_metadata_known') is not False:
                    raise ValueError('Original shader metadata was attributed to a private selected variant')
            event_count += len(events)
        if positions != sorted(set(positions)):
            raise ValueError('Captured writer command positions are duplicated or unordered')
    if len(target_commands) != 1:
        raise ValueError('Captured writer exact original target command metadata is absent or ambiguous')
    if event_count != blit.integer(value['event_count']) or event_count > maximum_events:
        raise ValueError('Captured writer retained event budget/count differs')
    return {'available': True, 'frozen': True, 'capture_sequence': sequence,
            'target_blit': dict(zip(REF_FIELDS, expected)), 'target_submission': target,
            'event_references': event_count, 'cpu_occurrences': len(occurrences),
            'draw_declarations': draw_count, 'incoming_transfer_declarations': incoming_count,
            'missing_before_admission': True, 'prefix_missing': prefix_missing, 'truncated': truncated,
            'unique_successful_original_cpu_after': matching == 1 and not ambiguous and complete and result == 0,
            'original_pipeline_metadata_entries': len(catalog), 'actual_gpu_write_proven': False}


def audit_receipts(receipts, *, require_available=True):
    results = []
    sequences = set()
    for receipt in receipts:
        sequence = blit.integer(receipt.get('capture_sequence', receipt.get('sequence')), 1)
        if sequence in sequences:
            raise ValueError('Captured writer was delivered twice for one capture sequence')
        sequences.add(sequence)
        observed = receipt['source_blit']
        blit.source_blit(observed, require_bridge=require_available)
        result = audit_prefix(receipt['captured_writer_prefix'], original=observed['original'],
                              capture_sequence=sequence,
                              history_submission=observed['history_submission'],
                              capture_submission=observed['capture_submission'],
                              require_available=require_available)
        results.append(result)
    if not results:
        raise ValueError('No delivered captured writer receipt was supplied')
    return {'delivered_images': len(results), 'available_prefixes': sum(item['available'] for item in results),
            'event_references': sum(item['event_references'] for item in results), 'receipts': results,
            'gpu_execution_order_known': False, 'last_gpu_writer_known': False,
            'whole_scene_verified': False, 'initialized_pixels_verified': False,
            'common_scene_time_known': False, 'xr_submission': False}


def shader_references(directory):
    result = {}
    for stage, suffix in ((1, 'vert'), (16, 'frag')):
        raw = blit.read(Path(directory)/('present.'+suffix+'.spv'))
        if len(raw) < 20 or len(raw) % 4 or raw[:4] != b'\x03\x02\x23\x07':
            raise ValueError('Owned original shader reference is not SPIR-V words')
        fnv = 14695981039346656037
        for byte in raw:
            fnv = ((fnv ^ byte)*1099511628211) & ((1 << 64)-1)
        result[stage] = {'fnv': fnv, 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}
    return result


def owned_writer(prefix, original, shaders):
    """Compare declared owned writes with independent original call controls.

    Frame-catalog labels and raw Vulkan pipeline/view handles are separate
    namespaces. Shader fingerprints, attachment properties and explicit saved
    memberships are compared; raw handles are not silently joined to labels.
    """
    blit.integer(original['writer_pipeline_handle'], 1)
    blit.integer(original['writer_view_handle'], 1)
    expected_draws = blit.integer(original['writer_draw_calls'], 1, 64)
    expected_colors = blit.integer(original['writer_color_attachment_count'], 1, 8)
    for key in ('writer_load_op', 'writer_store_op'):
        blit.integer(original[key], 0, (1 << 32)-1)
    if not isinstance(original['writer_render_area'], list) or len(original['writer_render_area']) != 4:
        raise ValueError('Owned original writer render area is not its exact four arguments')
    for index, field in enumerate(original['writer_render_area']):
        blit.integer(field, -(1 << 31) if index < 2 else 0, (1 << 32)-1)
    source = prefix['target_blit']['source_image']
    target_position = (prefix['target_batch'], prefix['target_command'])
    targets = [command for occurrence in prefix['occurrences']
               if occurrence['submission'] == prefix['target_submission']
               for command in occurrence['commands']
               if (command['batch'], command['command']) == target_position]
    if len(targets) != 1:
        raise ValueError('Owned original writer target command is absent or ambiguous')
    events = targets[0]['events']
    draws = [event for event in events if event['kind'] == 'draw']
    if len(draws) != expected_draws:
        raise ValueError('Owned original writer draw-call count differs from the saved target prefix')
    catalog = pipeline_catalog(prefix['catalog'])
    checked = set()
    for draw in draws:
        colors, views = draw.get('draw_color_images', []), draw.get('draw_color_views', [])
        if len(colors) != expected_colors or len(views) != expected_colors or source not in colors:
            raise ValueError('Owned color writer was inferred from an unrelated depth attachment')
        scope = blit.integer(draw['rendering_scope_ordinal'], 1)
        matches = [event for event in events if event['kind'] == 'declared_color_attachment'
                   and event['ordinal'] == scope and event['image'] == source]
        if len(matches) != 1 or matches[0]['view'] != views[colors.index(source)]:
            raise ValueError('Owned draw does not name the exact declared color attachment/view scope')
        attachment = matches[0]
        blit.integer(attachment['view'], 1)
        for field in attachment['area']:
            blit.integer(field, -(1 << 31), (1 << 32)-1)
        blit.integer(attachment['load_op'], 0, (1 << 32)-1)
        blit.integer(attachment['store_op'], 0, (1 << 32)-1)
        if (attachment['area'] != original['writer_render_area']
                or attachment['load_op'] != original['writer_load_op']
                or attachment['store_op'] != original['writer_store_op']
                or attachment['null_attachment'] is not False
                or attachment['declared_attachment_use'] is not True):
            raise ValueError('Owned original color attachment properties were retagged')
        pipeline = catalog.get(draw['original_pipeline_id'])
        if (not pipeline or pipeline['missing_libraries'] or pipeline['metadata_truncated']
                or draw.get('selected_pipeline_metadata_known') is not False):
            raise ValueError('Owned writer original shader catalog is unavailable or promoted to its variant')
        for stage, expected in shaders.items():
            stages = [shader for shader in pipeline['shaders'] if shader['stage'] == stage]
            if (len(stages) != 1 or stages[0]['known'] is not True
                    or stages[0]['fnv'] != expected['fnv'] or stages[0]['bytes'] != expected['bytes']):
                raise ValueError('Owned writer original shader differs from the independent SPIR-V input')
        checked.add(pipeline['id'])
    return {'original_draw_calls_compared': len(draws),
            'explicit_color_scope_memberships_compared': len(draws),
            'original_shader_catalog_entries_compared': len(checked),
            'raw_pipeline_or_view_handle_used_as_catalog_label': False,
            'gpu_writer_execution_proven': False}


def audit_directory(directory, *, require_available=True, shader_directory=None):
    """Re-use exact app golden pixels/arguments, then audit the saved prefix."""
    directory = Path(directory)
    original = blit.audit_directory(directory, require_bridge=require_available, require_stages=True)
    receipts = [blit.decode(blit.read(path)) for path in sorted(directory.glob('image-*.json'))]
    for path in sorted(directory.glob('pose-pair-*.json')):
        receipts.extend(blit.decode(blit.read(path))['eyes'])
    for path in sorted(directory.glob('vulkan-*.jsonl')):
        receipts.extend(row for row in blit.json_lines(path) if row.get('event') == 'gpu_capture_lease')
    result = audit_receipts(receipts, require_available=require_available)
    if require_available:
        references = shader_references(shader_directory or directory.parent)
        originals = blit.original_rows(blit.json_lines(directory/'blit-fixture.jsonl'))
        checks = []
        for receipt in receipts:
            present = blit.integer(receipt.get('source_present', receipt.get('present')), 1)
            checks.append(owned_writer(receipt['captured_writer_prefix'], originals[present-1], references))
        result['owned_original_writer_controls'] = checks
        result['owned_original_shader_references'] = {str(stage): value for stage, value in references.items()}
        result['owned_original_writer_draw_calls_compared'] = sum(row['original_draw_calls_compared'] for row in checks)
    result['original_pixel_bytes_compared'] = original['original_pixel_bytes_compared']
    result['input_sha256'] = original['input_sha256']
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation', required=True, type=Path, help='Already completed owned WSI case')
    parser.add_argument('--allow-unavailable', action='store_true', help='Keep explicit absent/budget/fault diagnostics unknown')
    parser.add_argument('--shader-directory', type=Path, help='Owned original shader inputs; defaults to validation parent')
    args = parser.parse_args()
    print(json.dumps({'schema_version': 1, 'new_gpu_execution_performed': False,
                      'audit': audit_directory(args.validation, require_available=not args.allow_unavailable,
                                               shader_directory=args.shader_directory)}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(str(error))
