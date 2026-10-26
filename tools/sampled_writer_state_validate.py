#!/usr/bin/env python3
"""Read-only checks of CPU writer state frozen into delivered image prefixes.

Application-owned argument controls are independent of the observer. Known push
integers do not identify a descriptor, image, sampler or shader resource access.
This tool launches no process and performs no Vulkan or graphics operation.
"""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

import blit_provenance_validate as blit
import captured_writer_validate as writer


NO_INFERENCE = (*writer.NO_INFERENCE, 'effective_raster_state_known',
                'descriptor_placement_known', 'sampled_resource_known', 'coverage_known',
                'push_values_identify_sampled_resource', 'fragment_shader_reads_push_constants')
MODES = {'unknown', 'static', 'dynamic', 'with_count'}
UINT32 = (1 << 32)-1


def claims(value):
    if isinstance(value, dict):
        for key, item in value.items():
            if key in NO_INFERENCE and item is not False:
                raise ValueError('CPU state was promoted to resource, execution or coverage evidence: '+key)
            claims(item)
    elif isinstance(value, list):
        for item in value:
            claims(item)


def integer(value, lower=0, upper=UINT32):
    return blit.integer(value, lower, upper)


def raw_hex(value, size):
    if not isinstance(value, str) or len(value) != size*2 or value.lower() != value:
        raise ValueError('State bytes lost their exact bounded hexadecimal encoding')
    try:
        result = bytes.fromhex(value)
    except ValueError as error:
        raise ValueError('Invalid state hexadecimal bytes') from error
    if len(result) != size:
        raise ValueError('State bytes have the wrong size')
    return result


def array(value, length, lower=0, upper=UINT32):
    if not isinstance(value, list) or len(value) != length:
        raise ValueError('State argument array has the wrong bounded length')
    return [integer(item, lower, upper) for item in value]


def layout(value):
    if not isinstance(value, dict):
        raise ValueError('Immutable push-layout declaration is missing')
    for key in ('generation', 'raw_handle'):
        integer(value[key], 0, (1 << 64)-1)
    integer(value['flags'])
    writer.boolean(value['ranges_complete'], 'ranges_complete')
    ranges, chain = value['ranges'], value['chain_types']
    if not isinstance(ranges, list) or len(ranges) > 32 or not isinstance(chain, list) or len(chain) > 8:
        raise ValueError('Push-layout range or extension bound changed')
    for item in ranges:
        if not isinstance(item, dict) or set(item) != {'stage_flags', 'offset', 'size'}:
            raise ValueError('Push-layout range fields changed')
        for field in ('stage_flags', 'offset', 'size'):
            integer(item[field])
    for item in chain:
        integer(item, -(1 << 31), (1 << 31)-1)
    if value['ranges_complete'] and (not value['generation'] or not value['raw_handle'] or chain):
        raise ValueError('Known push layout has an unknown lifetime or extension')
    return value


def golden(value):
    if not isinstance(value, dict) or type(value.get('schema_version')) is not int or value['schema_version'] != 1:
        raise ValueError('Owned draw state control schema is unavailable')
    claims(value)
    integer(value['frame'], 0, 63);integer(value['draw_index'], 0, 1)
    for field in ('command_handle', 'pipeline_handle', 'bound_layout_handle', 'push_layout_handle'):
        integer(value[field], 1, (1 << 64)-1)
    if value['bound_layout_handle'] == value['push_layout_handle']:
        raise ValueError('Owned compatibility control did not use two layout handles')
    if value['layout_ranges'] != [[17, 0, 32]]:
        raise ValueError('Owned fixture push range declaration changed')
    if value['mode'] not in MODES-{'unknown'}:
        raise ValueError('Owned fixture viewport/scissor mode is not controlled')
    for field in ('dynamic_count_enabled', 'maintenance6_enabled'):
        writer.boolean(value[field], field)
    mode = value['mode']
    expected_count = bool(value['draw_index'] and value['dynamic_count_enabled'] and value['frame'] % 2)
    if mode != ('static' if value['draw_index'] == 0 else 'with_count' if expected_count else 'dynamic'):
        raise ValueError('Owned draw mode was retagged after the original call')
    for name in ('Viewport', 'Scissor'):
        expected = ('pipeline_static' if mode == 'static' else 'vkCmdSet'+name if mode == 'dynamic'
                    else 'vkCmdSet'+name+'WithCount'+('EXT' if value['frame'] % 4 == 3 else ''))
        if value[name.lower()+'_command'] != expected:
            raise ValueError('Owned setter alias does not match the actual control mode')
    expected_push = 'vkCmdPushConstants2KHR' if value['maintenance6_enabled'] and value['frame'] % 2 else 'vkCmdPushConstants'
    if value['push_command'] != expected_push:
        raise ValueError('Owned push alias lacks its explicitly enabled feature')
    raw = raw_hex(value['fragment_push_hex'], 32)
    updates = value['push_updates']
    if not isinstance(updates, list) or len(updates) != 3:
        raise ValueError('Owned full/partial push controls changed')
    rebuilt = bytearray(32)
    for update, offset, size in zip(updates, (0, 8, 24), (32, 4, 4)):
        if (update['stage_flags'], update['offset'], update['size']) != (17, offset, size):
            raise ValueError('Owned push stages or partial-write interval changed')
        rebuilt[offset:offset+size] = raw_hex(update['hex'], size)
    if rebuilt != raw:
        raise ValueError('Owned final push bytes differ from the original incremental updates')
    array(value['viewport_bits'], 6);array(value['scissor_offset'], 2, -(1 << 31), (1 << 31)-1)
    array(value['scissor_extent'], 2)
    if value['viewport_count'] != 1 or value['scissor_count'] != 1:
        raise ValueError('Owned fixture is not the single viewport/scissor control')
    if value['draw_arguments'] != {'count': 3, 'instance_count': 1, 'first': 0, 'first_instance': 0}:
        raise ValueError('Owned original direct-draw arguments changed')
    return value


def rectangle(value, *, viewport):
    if not isinstance(value, dict) or value.get('mode') not in MODES:
        raise ValueError('Writer rectangle mode is unknown or malformed')
    for key in ('value_known', 'effective_known', 'count_known', 'conventional_valid'):
        writer.boolean(value[key], key)
    if viewport:
        bits = array(value['bits'], 6)
        writer.boolean(value['finite'], 'finite')
        floats = struct.unpack('<6f', struct.pack('<6I', *bits))
        finite = all(abs(number) != float('inf') and number == number for number in floats)
        if value['value_known'] and value['finite'] != finite:
            raise ValueError('Viewport bit-level finite classification changed')
    else:
        array(value['offset'], 2, -(1 << 31), (1 << 31)-1);array(value['extent'], 2)
    for key in ('count', 'declared_first', 'declared_count'):
        integer(value[key])
    for key in ('setter_ordinal', 'count_ordinal', 'epoch', 'pipeline_generation', 'last_ordinal'):
        integer(value[key], 0, (1 << 64)-1)
    integer(value['last_first']);integer(value['last_count'])
    if value['effective_known'] and (not value['value_known'] or not value['count_known']
                                      or value['count'] != 1 or not value['conventional_valid']):
        raise ValueError('Effective conventional rectangle was promoted from unknown values or counts')
    if value['mode'] == 'unknown' and (value['value_known'] or value['effective_known'] or value['count_known']):
        raise ValueError('Unknown pipeline rectangle mode retained a known interpretation')
    return value


def audit_state(value, *, original=None, ordinal=None):
    if not isinstance(value, dict) or type(value.get('schema_version')) is not int or value['schema_version'] != 1:
        raise ValueError('Frozen per-draw writer state schema is missing')
    claims(value)
    for field in ('effective_raster_state_known', 'descriptor_placement_known', 'sampled_resource_known', 'coverage_known'):
        if value[field] is not False:
            raise ValueError('Per-draw declaration does not retain its explicit unknown resource/coverage limits')
    if value.get('available') is not True:
        raise ValueError('The controlled original draw needs its bounded state observation')
    observed_ordinal = integer(value['ordinal'], 1, (1 << 64)-1)
    if ordinal is not None and observed_ordinal != ordinal:
        raise ValueError('Writer state was taken at a different draw ordinal')
    generation = integer(value['pipeline_generation'], 1 if original is not None else 0, (1 << 64)-1)
    for field in ('pipeline_flags', 'pipeline_unknown_flags'):
        integer(value[field], 0, (1 << 64)-1)
    writer.boolean(value['pipeline_declarations_complete'], 'pipeline_declarations_complete')
    writer.boolean(value['overflow'], 'overflow')
    if original is not None and value['overflow']:
        raise ValueError('Controlled draw state exhausted its ordinal epoch')
    if original is not None and (value['pipeline_flags'] != 0 or value['pipeline_unknown_flags'] != 0
                                 or not value['pipeline_declarations_complete']):
        raise ValueError('Owned pipeline declarations are incomplete or carry unmodeled flags')
    push = value['push']
    raw = raw_hex(push['raw_hex'], 32)
    mask = integer(push['byte_known_mask'])
    compatible_mask = integer(push['compatible_word_mask'], 0, 255)
    writer.boolean(push['host_little_endian'], 'host_little_endian')
    writer.boolean(push['layout_known'], 'layout_known')
    bound_generation = integer(push['bound_layout_generation'], 0, (1 << 64)-1)
    bound = layout(push['bound_layout'])
    if bound['generation'] != bound_generation or push['layout_known'] != bool(bound_generation and bound['ranges_complete']):
        raise ValueError('Bound push-layout signature and generation disagree')
    words = push['words']
    if not isinstance(words, list) or len(words) != 8:
        raise ValueError('Push observation exceeded or lost its eight-word bound')
    for index, word in enumerate(words):
        if word['word'] != index:
            raise ValueError('Push word order/index changed')
        known = writer.boolean(word['known'], 'known')
        compatible = writer.boolean(word['compatible'], 'compatible')
        if (mask >> (index*4)) & 15 not in (0, 15):
            raise ValueError('Word-aligned Vulkan push update acquired an invented partial-byte mask')
        if known != ((mask >> (index*4)) & 15 == 15) or compatible != bool(compatible_mask & (1 << index)):
            raise ValueError('Push masks disagree with per-word knowledge/compatibility')
        if compatible and (not known or not push['layout_known']):
            raise ValueError('Unknown push bytes/layout were marked compatible')
        written = integer(word['ordinal'], 0, (1 << 64)-1)
        stages, offset, size = (integer(word[name]) for name in ('stage_flags', 'offset', 'size'))
        source_layout = layout(word['layout'])
        actual_compatible = bool(known and push['layout_known'] and source_layout['ranges_complete']
                                 and source_layout['ranges'] == bound['ranges'])
        if compatible != actual_compatible:
            raise ValueError('Push compatibility does not follow the two frozen layout signatures')
        if known:
            if not 0 < written < observed_ordinal or not stages & 16 or not offset <= index*4 < offset+size:
                raise ValueError('Push provenance is late, wrong-stage or outside its original write')
        for byte in range(index*4, index*4+4):
            if not mask & (1 << byte) and raw[byte] != 0:
                raise ValueError('Unknown push byte leaked an old value')
    for field, index in (('image', 2), ('sampler', 6)):
        known = writer.boolean(push[field+'_index_known'], field+'_index_known')
        expected_known = words[index]['known'] and words[index]['compatible']
        if known != expected_known:
            raise ValueError('Decoded shader index was promoted from unknown/incompatible bytes')
        if known:
            if integer(push[field+'_index']) != int.from_bytes(raw[index*4:index*4+4], 'little' if push['host_little_endian'] else 'big'):
                raise ValueError('Decoded shader index differs from the exact original bytes')
        elif push[field+'_index'] is not None:
            raise ValueError('Unknown shader index was serialized as a current value')
    viewport = rectangle(value['viewport'], viewport=True)
    scissor = rectangle(value['scissor'], viewport=False)
    arguments = value['arguments']
    if arguments['kind'] not in ('unknown', 'direct', 'indexed'):
        raise ValueError('Unknown draw argument interpretation')
    for field in ('command_kind', 'count', 'instance_count', 'first', 'first_instance'):
        integer(arguments[field])
    integer(arguments['vertex_offset'], -(1 << 31), (1 << 31)-1)
    if original is not None:
        original = golden(original)
        if (mask != UINT32 or compatible_mask != 255 or not push['layout_known']
                or not push['host_little_endian'] or not bound_generation
                or raw != raw_hex(original['fragment_push_hex'], 32)):
            raise ValueError('Saved push window differs from the independently recorded original draw')
        for index, word in enumerate(words):
            update_index = 1 if index == 2 else 2 if index == 6 else 0
            expected = original['push_updates'][update_index]
            source_layout = word['layout']
            ranges = [[r['stage_flags'], r['offset'], r['size']] for r in source_layout['ranges']]
            if (word['ordinal'] != observed_ordinal-3+update_index
                    or [word['stage_flags'], word['offset'], word['size']] != [expected['stage_flags'], expected['offset'], expected['size']]
                    or source_layout['raw_handle'] != original['push_layout_handle']
                    or source_layout['flags'] != 0 or source_layout['ranges_complete'] is not True
                    or ranges != original['layout_ranges'] or source_layout['chain_types']):
                raise ValueError('Push provenance/layout was retagged from another original write')
        if len({word['layout']['generation'] for word in words}) != 1 or words[0]['layout']['generation'] == bound_generation:
            raise ValueError('Two compatible original layout lifetimes were conflated or mixed')
        if (bound['raw_handle'] != original['bound_layout_handle'] or bound['flags'] != 0
                or [[r['stage_flags'], r['offset'], r['size']] for r in bound['ranges']] != original['layout_ranges']):
            raise ValueError('Bound layout differs from the independent original pipeline declaration')
        for rectangle_value in (viewport, scissor):
            mode = original['mode']
            if (rectangle_value['mode'] != mode or not rectangle_value['value_known']
                    or not rectangle_value['effective_known'] or not rectangle_value['count_known']
                    or rectangle_value['count'] != 1 or rectangle_value['pipeline_generation'] != generation
                    or not rectangle_value['conventional_valid']):
                raise ValueError('Writer raster rectangle differs from its original static/dynamic declaration')
            if mode == 'static':
                if rectangle_value['setter_ordinal'] != 0 or rectangle_value['count_ordinal'] != 0:
                    raise ValueError('Static state was attributed to a dynamic setter')
            else:
                expected_setter = observed_ordinal-(5 if rectangle_value is viewport else 4)
                if (rectangle_value['setter_ordinal'] != expected_setter
                        or rectangle_value['last_ordinal'] != expected_setter
                        or (rectangle_value['last_first'], rectangle_value['last_count']) != (0, 1)
                        or rectangle_value['count_ordinal'] != (expected_setter if mode == 'with_count' else 0)):
                    raise ValueError('Dynamic value or count was retagged from another original setter')
            if mode != 'static' and (rectangle_value['declared_first'], rectangle_value['declared_count']) != (0, 1):
                raise ValueError('A setter for another viewport/scissor index was reused')
        if (viewport['bits'] != original['viewport_bits'] or not viewport['finite']
                or scissor['offset'] != original['scissor_offset'] or scissor['extent'] != original['scissor_extent']):
            raise ValueError('Saved viewport/scissor bytes differ from original application arguments')
        if (arguments['kind'] != 'direct' or arguments['command_kind'] != 0 or arguments['vertex_offset'] != 0
                or any(arguments[key] != expected for key, expected in original['draw_arguments'].items())):
            raise ValueError('Direct draw arguments were inferred or retagged')
    result = {'draw_ordinal': observed_ordinal, 'pipeline_generation': generation,
            'push_byte_count': 32, 'known_push_bytes': mask.bit_count(),
            'compatible_push_words': compatible_mask.bit_count(),
            'viewport_mode': viewport['mode'], 'scissor_mode': scissor['mode'],
            'owned_original_arguments_compared': original is not None,
            'sampled_resource_known': False, 'coverage_known': False}
    if original is not None:
        result['owned_original_aliases_at_delivered_draw'] = [original[key] for key in ('push_command', 'viewport_command', 'scissor_command')]
    return result


def audit_receipt(receipt, controls, *, observer_enabled=True):
    prefix = receipt['captured_writer_prefix']
    observed = receipt['source_blit']
    writer.audit_prefix(prefix, original=observed['original'],
                        capture_sequence=receipt.get('capture_sequence', receipt.get('sequence')),
                        history_submission=observed['history_submission'],
                        capture_submission=observed['capture_submission'])
    present = integer(receipt.get('source_present', receipt.get('present')), 1, 64)
    source = prefix['target_blit']['source_image']
    positions = [(occurrence, command) for occurrence in prefix['occurrences']
                 if occurrence['submission'] == prefix['target_submission']
                 for command in occurrence['commands']
                 if (command['batch'], command['command']) == (prefix['target_batch'], prefix['target_command'])]
    if len(positions) != 1:
        raise ValueError('Controlled draw prefix has no unique target command')
    draws = [event for event in positions[0][1]['events'] if event['kind'] == 'draw']
    if len(draws) != 2:
        raise ValueError('Controlled delivered source prefix must retain the two original draw calls')
    results = []
    for index, event in enumerate(draws):
        if source not in event.get('draw_color_images', []):
            raise ValueError('Per-draw state is attached to an unrelated source lifetime')
        if observer_enabled:
            if event['writer_state_original_pipeline_handle'] != controls[(present-1, index)]['pipeline_handle']:
                raise ValueError('State pipeline was joined to a different raw original draw handle')
            results.append(audit_state(event['writer_state'], original=controls[(present-1, index)], ordinal=event['ordinal']))
        elif 'writer_state' in event or 'writer_state_original_pipeline_handle' in event:
            raise ValueError('Default disabled observer changed the legacy draw-event format')
    return {'capture_sequence': prefix['capture_sequence'], 'source_present': present,
            'draw_states_compared': len(results), 'legacy_draw_events_unchanged': not observer_enabled,
            'states': results}


def audit_directory(directory, *, observer_enabled=True, shader_directory=None):
    directory = Path(directory)
    base = writer.audit_directory(directory, require_available=True, shader_directory=shader_directory)
    originals = [golden(row) for row in blit.json_lines(directory/'sampled-writer-fixture.jsonl')]
    controls = {(row['frame'], row['draw_index']): row for row in originals}
    if len(originals) != 128 or len(controls) != 128 or set(controls) != {(frame, draw) for frame in range(64) for draw in range(2)}:
        raise ValueError('Original fixture must retain exactly128 unique original draw controls')
    features = {(row['dynamic_count_enabled'], row['maintenance6_enabled']) for row in originals}
    if len(features) != 1:
        raise ValueError('Fixture device features changed within one original device lifetime')
    receipts = [blit.decode(blit.read(path)) for path in sorted(directory.glob('image-*.json'))]
    for path in sorted(directory.glob('pose-pair-*.json')):
        receipts.extend(blit.decode(blit.read(path))['eyes'])
    for path in sorted(directory.glob('vulkan-*.jsonl')):
        receipts.extend(row for row in blit.json_lines(path) if row.get('event') == 'gpu_capture_lease')
    rows = [audit_receipt(receipt, controls, observer_enabled=observer_enabled) for receipt in receipts]
    dynamic_count, maintenance6 = next(iter(features))
    compared = [state for row in rows for state in row['states']]
    return {'schema_version': 1, 'observation_only': True, 'observer_enabled': observer_enabled,
            'original_draw_controls': 128, 'delivered_images': len(rows),
            'delivered_draw_states_compared': sum(row['draw_states_compared'] for row in rows),
            'delivered_viewport_mode_counts': {mode: sum(state['viewport_mode'] == mode for state in compared)
                                               for mode in sorted(MODES-{'unknown'})},
            'owned_original_aliases_at_delivered_draws': sorted({alias for state in compared
                                                                for alias in state['owned_original_aliases_at_delivered_draw']}),
            'original_pixel_bytes_compared': base['original_pixel_bytes_compared'],
            'dynamic_count_enabled': dynamic_count, 'maintenance6_enabled': maintenance6,
            'original_setter_aliases': sorted({row['viewport_command'] for row in originals}
                                             | {row['scissor_command'] for row in originals}
                                             | {row['push_command'] for row in originals}),
            'sampled_resource_known': False, 'coverage_known': False, 'xr_submission': False,
            'receipts': rows, 'input_sha256': base['input_sha256']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation', type=Path, required=True)
    parser.add_argument('--observer-disabled', action='store_true')
    parser.add_argument('--shader-directory', type=Path)
    args = parser.parse_args()
    print(json.dumps({'new_gpu_execution_performed': False,
                      'audit': audit_directory(args.validation, observer_enabled=not args.observer_disabled,
                                               shader_directory=args.shader_directory)}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(str(error))
