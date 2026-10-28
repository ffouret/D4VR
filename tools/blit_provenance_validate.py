#!/usr/bin/env python3
"""Read-only audit of already completed owned presentation-BLIT captures.

No executable, Vulkan loader, game, UI, or subprocess is invoked. Original
application arguments, frozen hook receipts, successful original submissions,
and delivered capture receipts are separate evidence. CPU submission order is
not GPU execution order; a producer-completion receipt is not a shader-read,
whole-scene, common-time, or XR receipt.
"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import re


REF_FIELDS = ('observer_session', 'source_image', 'destination_image', 'recording', 'ordinal')
FLAGS = ('identity_known', 'region_known', 'original_submit_succeeded',
         'history_submission_known', 'capture_submission_known', 'gpu_complete',
         'bridge_known')
REGION_FIELDS = ('source_subresource', 'destination_subresource', 'source_offsets', 'destination_offsets')
ARGUMENT_FIELDS = ('source_image_handle', 'destination_image_handle', 'source_layout',
                   'destination_layout', 'filter', *REGION_FIELDS)
FROZEN_FIELDS = ('original', *ARGUMENT_FIELDS, 'identity_known', 'region_known')
OBSERVATION_FIELDS = (*FROZEN_FIELDS, 'status', *FLAGS, 'history_submission', 'capture_submission')
NO_INFERENCE = ('whole_scene_verified', 'common_scene_time_known', 'xr_pose_known',
                'gpu_execution_order_known', 'descriptor_selected', 'gpu_read_proven',
                'placement_known', 'initialized_pixels_verified')
STATUSES = {'absent', 'recorded', 'submitted', 'producer_complete', 'ambiguous'}


def decode(data):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('Duplicate JSON field: '+key)
            result[key] = value
        return result

    def reject(value):
        raise ValueError('Nonfinite JSON number: '+value)

    return json.loads(data, object_pairs_hook=pairs, parse_constant=reject)


def read(path):
    path = Path(path)
    if path.is_symlink() or not path.is_file():
        raise ValueError('Require a regular input file: '+str(path))
    return path.read_bytes()


def integer(value, minimum=0, maximum=(1 << 64)-1):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError('Invalid bounded integer observation')
    return value


def no_inference(value):
    if not isinstance(value, dict):
        raise ValueError('Require an object receipt')
    for key in NO_INFERENCE:
        if key in value and value[key] is not False:
            raise ValueError('BLIT receipt promoted to a shader, scene, time, or XR claim')


def blit_ref(value, *, known=False):
    if not isinstance(value, dict) or set(value) != {*REF_FIELDS, 'event_retained'}:
        raise ValueError('Original BLIT reference fields changed')
    if type(value['event_retained']) is not bool:
        raise ValueError('BLIT journal retention flag is not boolean')
    result = tuple(integer(value[key]) for key in REF_FIELDS)
    if known and (not all(result) or result[1] == result[2]):
        raise ValueError('Original BLIT lifetime/reference is unknown or conflated')
    return result


def region(value, *, known=False):
    if not isinstance(value, dict):
        raise ValueError('Original BLIT region is not an object')
    for key in ('source_subresource', 'destination_subresource'):
        subresource = value[key]
        if not isinstance(subresource, list) or len(subresource) != 4:
            raise ValueError('Original BLIT subresource fields changed')
        for field in subresource:
            integer(field, 0, (1 << 32)-1)
        if known and (not subresource[0] or not subresource[3]):
            raise ValueError('Known original BLIT has an empty subresource')
    for key in ('source_offsets', 'destination_offsets'):
        offsets = value[key]
        if not isinstance(offsets, list) or len(offsets) != 2:
            raise ValueError('BLIT requires both original offsets')
        for offset in offsets:
            if not isinstance(offset, list) or len(offset) != 3:
                raise ValueError('BLIT offset is not an original XYZ value')
            for coordinate in offset:
                integer(coordinate, -(1 << 31), (1 << 31)-1)
    return value


def source_blit(value, *, require_bridge=False):
    """Validate one value snapshot without inferring external execution facts."""
    no_inference(value)
    for key in FLAGS:
        if type(value[key]) is not bool:
            raise ValueError('BLIT evidence flag is not boolean: '+key)
    status = value['status']
    if status not in STATUSES:
        raise ValueError('Unknown BLIT capture state')
    ref = blit_ref(value['original'], known=value['identity_known'])
    for key in ('source_image_handle', 'destination_image_handle', 'history_submission', 'capture_submission'):
        integer(value[key])
    for key in ('source_layout', 'destination_layout', 'filter'):
        integer(value[key], 0, (1 << 32)-1)
    region(value, known=value['region_known'])
    if value['identity_known'] and (not value['source_image_handle'] or not value['destination_image_handle']
                                    or value['source_image_handle'] == value['destination_image_handle']):
        raise ValueError('Known BLIT lacks distinct original application endpoints')
    if value['original']['event_retained'] and not all(ref):
        raise ValueError('A retained journal event lacks its original BLIT reference')
    for flag, counter in (('history_submission_known', 'history_submission'),
                          ('capture_submission_known', 'capture_submission')):
        if value[flag] is not bool(value[counter]):
            raise ValueError('Submission knowledge and its own namespace counter differ')
    if status in ('absent', 'recorded') and (value['original_submit_succeeded']
            or value['history_submission_known'] or value['capture_submission_known'] or value['gpu_complete']):
        raise ValueError('Record-time reference was promoted to an original submission')
    if status in ('submitted', 'producer_complete', 'ambiguous') and not value['original_submit_succeeded']:
        raise ValueError('Submission state lacks the successful original callback')
    if value['gpu_complete'] and (status != 'producer_complete' or not value['original_submit_succeeded']):
        raise ValueError('Producer completion lacks the successful original submission')
    if status == 'producer_complete' and not value['gpu_complete']:
        raise ValueError('Producer-complete state lacks its fence observation')
    bridge = (status == 'producer_complete' and value['identity_known'] and value['region_known']
              and value['original_submit_succeeded'] and value['history_submission_known']
              and value['capture_submission_known'] and value['gpu_complete'])
    if value['bridge_known'] is not bridge:
        raise ValueError('Exact BLIT bridge was asserted without its separate evidence')
    if require_bridge and not bridge:
        raise ValueError('An exact original presentation BLIT bridge is required')
    return ref


def source_blit_stages(value, final, *, require_known=False):
    """Check saved lifecycle values, never manufacture them from final state.

    These are producer-owned snapshots carried by the selected capture, not
    independent original-application observations. The latter remain mandatory
    in ``audit_receipts``. A missing global trace row does not erase a saved
    value, and a saved value does not invent such a row.
    """
    if not isinstance(value, dict) or set(value) != {
            'recorded_known', 'submitted_known', 'recorded', 'submitted'}:
        raise ValueError('Saved BLIT lifecycle fields changed')
    source_blit(final)
    for phase in ('recorded', 'submitted'):
        known = value[phase+'_known']
        if type(known) is not bool:
            raise ValueError('Saved BLIT lifecycle knowledge is not boolean')
        snapshot = value[phase]
        source_blit(snapshot)
        if not known:
            if (snapshot['status'] != 'absent' or any(snapshot[key] for key in FLAGS)
                    or any(snapshot[key] for key in ('history_submission', 'capture_submission',
                                                      'source_image_handle', 'destination_image_handle',
                                                      'source_layout', 'destination_layout', 'filter'))
                    or any(snapshot['original'][key] for key in REF_FIELDS)
                    or snapshot['original']['event_retained']
                    or any(any(coordinate for coordinate in offset)
                           for key in ('source_offsets', 'destination_offsets') for offset in snapshot[key])
                    or any(any(snapshot[key]) for key in ('source_subresource', 'destination_subresource'))):
                raise ValueError('Unknown saved BLIT lifecycle was filled from later state')
            continue
        if snapshot['status'] != phase:
            raise ValueError('Saved BLIT lifecycle phase was replaced with later state')
        if snapshot['gpu_complete'] or snapshot['bridge_known']:
            raise ValueError('Saved BLIT lifecycle phase contains future fence completion')
        if any(snapshot[key] != final[key] for key in FROZEN_FIELDS):
            raise ValueError('Saved BLIT lifetime/arguments differ from delivered capture')
        if phase == 'submitted' and any(snapshot[key] != final[key]
                for key in ('history_submission', 'capture_submission',
                            'history_submission_known', 'capture_submission_known')):
            raise ValueError('Saved original submission counters were retagged')
    if value['submitted_known'] and not value['recorded_known']:
        raise ValueError('Saved submission lacks its original saved record value')
    if require_known and not (value['recorded_known'] and value['submitted_known']):
        raise ValueError('Delivered capture lacks its saved record/submission snapshots')
    return value


def event_region(event):
    if event['mip_count'] != 1 or event['destination_mip_count'] != 1:
        raise ValueError('Original BLIT event does not preserve its single mip subresources')
    return {'source_subresource': [event['aspects'], event['base_mip'], event['base_layer'], event['layer_count']],
            'destination_subresource': [event['destination_aspects'], event['destination_base_mip'], event['destination_base_layer'], event['destination_layer_count']],
            'source_offsets': event['blit_source_offsets'], 'destination_offsets': event['blit_destination_offsets']}


def history_events(wrappers):
    """Index retained journal events separately from hook refs and GPU leases."""
    result = {}
    namespaces = set()
    for wrapper in wrappers:
        namespace = (integer(wrapper['process_id'], 1), integer(wrapper['device'], 1),
                     integer(wrapper['history']['observer_session'], 1))
        namespaces.add(namespace)
        aggregate = wrapper['history']
        no_inference(aggregate)
        for report in aggregate.get('reports', []):
            scopes = [report]
            if 'consumer_window' in report:
                scopes.append(report['consumer_window'])
            for scope in scopes:
                no_inference(scope)
                for occurrence in scope.get('occurrences', []):
                    for command in occurrence.get('commands', []):
                        for event in command.get('events', []):
                            if event.get('kind') != 'blit_image':
                                continue
                            ref = (namespace[2], event['image'], event['destination_image'],
                                   command['recording'], event['ordinal'])
                            key = (ref, integer(occurrence['submission'], 1))
                            frozen = {'source_layout': event['layout'], 'destination_layout': event['destination_layout'],
                                      'filter': event['filter'], **event_region(event)}
                            row = {'frozen': frozen, 'complete': occurrence['complete'], 'result': occurrence['result'],
                                   'capture_submission': occurrence['capture_submission']}
                            if key in result:
                                old = result[key]
                                if old['frozen'] != frozen or old['capture_submission'] != row['capture_submission']:
                                    raise ValueError('Retained original BLIT event was retagged')
                                if old['complete'] and row['complete'] and old['result'] != row['result']:
                                    raise ValueError('Original AFTER result changed')
                                if old['complete'] and not row['complete']:
                                    continue
                            result[key] = row
    if len(namespaces) > 1:
        raise ValueError('Owned BLIT artifacts cross process/device/observer namespaces')
    return result, next(iter(namespaces), None)


def original_rows(rows):
    result = {}
    for row in rows:
        no_inference(row)
        frame = integer(row['frame'])
        if frame in result:
            raise ValueError('Ambiguous original application BLIT frame')
        for key in ('command', 'source_image_handle', 'destination_image_handle'):
            integer(row[key], 1)
        for key in ('source_layout', 'destination_layout', 'filter'):
            integer(row[key], 0, (1 << 32)-1)
        region(row, known=True)
        integer(row['original_submit_result'], -(1 << 31), (1 << 31)-1)
        result[frame] = row
    if not result:
        raise ValueError('Independent original application BLIT arguments are missing')
    return result


def audit_receipts(receipts, originals, hooks, submissions, wrappers=(), *, require_bridge=True,
                   require_stages=False):
    """Join distinct evidence sources; never re-resolve a live handle/pose."""
    originals = original_rows(originals)
    journal, namespace = history_events(wrappers)
    by_sequence = {}
    for hook in hooks:
        if namespace and 'device' in hook and hook['device'] != namespace[1]:
            raise ValueError('Recording hook was joined across original devices')
        sequence = integer(hook['sequence'], 1)
        if sequence in by_sequence:
            raise ValueError('Capture sequence has multiple recording hooks')
        source_blit(hook['source_blit'])
        if hook['source_blit']['status'] != 'recorded':
            raise ValueError('Original recording hook was replaced with later capture state')
        integer(hook['command'], 1)
        by_sequence[sequence] = hook
    submit_index = {}
    for submit in submissions:
        if namespace and 'device' in submit and submit['device'] != namespace[1]:
            raise ValueError('Submission hook was joined across original devices')
        key = integer(submit['sequence'], 1)
        source_blit(submit['source_blit'])
        if submit['source_blit']['status'] != 'submitted':
            raise ValueError('Original submission receipt lacks its unique successful callback')
        if key in submit_index:
            raise ValueError('Ambiguous original submission receipt')
        submit_index[key] = submit
    seen_sequences = set()
    seen_leases = set()
    audited = []
    for receipt in receipts:
        no_inference(receipt)
        sequence = integer(receipt.get('capture_sequence', receipt.get('sequence')), 1)
        present = integer(receipt.get('source_present', receipt.get('present')), 1)
        if sequence in seen_sequences:
            raise ValueError('Capture was delivered more than once')
        seen_sequences.add(sequence)
        if 'lease' in receipt:
            lease = integer(receipt['lease'], 1)
            if lease in seen_leases:
                raise ValueError('Lease token was reused for a different capture')
            seen_leases.add(lease)
        observed = receipt['source_blit']
        ref = source_blit(observed, require_bridge=require_bridge)
        stages = None
        if 'source_blit_stages' in receipt:
            stages = source_blit_stages(receipt['source_blit_stages'], observed, require_known=True)
        elif require_stages:
            raise ValueError('Delivered capture lacks saved BLIT lifecycle evidence')
        if namespace and ref[0] and ref[0] != namespace[2]:
            raise ValueError('Capture original ref crosses the audited observer namespace')
        hook = by_sequence.get(sequence)
        original = originals.get(present-1)
        submit = submit_index.get(sequence)
        generic_without_hook = (not require_bridge and hook is None and submit is None
                                and not observed['identity_known'] and not any(ref)
                                and not observed['original']['event_retained'] and not observed['bridge_known'])
        if original is None or hook is None and not generic_without_hook and stages is None:
            raise ValueError('Delivered capture lacks exact original frame/record evidence')
        for key in ARGUMENT_FIELDS:
            if observed[key] != original[key]:
                raise ValueError('Delivered BLIT differs from original directed application arguments')
        if (original['original_submit_result'] != 0 or not observed['original_submit_succeeded']
                or not observed['gpu_complete'] or observed['status'] != 'producer_complete'):
            raise ValueError('Original record ref lacks its exact successful original submission')
        if hook is not None:
            frozen = hook['source_blit']
            if any(frozen[key] != observed[key] for key in FROZEN_FIELDS):
                raise ValueError('Delivered BLIT lifetime/recording/ordinal/geometry was retagged')
            if hook['command'] != original['command']:
                raise ValueError('Delivered BLIT was joined to a reset/reused foreign command')
            if stages is not None and any(frozen[key] != stages['recorded'][key]
                                          for key in OBSERVATION_FIELDS):
                raise ValueError('Saved record snapshot differs from its independent trace row')
        if submit is not None:
            submitted = submit['source_blit']
            if any(submitted[key] != observed[key] for key in (*FROZEN_FIELDS, 'history_submission', 'capture_submission')):
                raise ValueError('Delivered BLIT differs from its original successful submission snapshot')
            if stages is not None and any(submitted[key] != stages['submitted'][key]
                                          for key in OBSERVATION_FIELDS):
                raise ValueError('Saved submission snapshot differs from its independent trace row')
        elif not generic_without_hook and stages is None:
            raise ValueError('Original record ref lacks its exact successful original submission')
        retained = journal.get((ref, observed['history_submission']))
        if not observed['original']['event_retained'] and any(key[0] == ref for key in journal):
            raise ValueError('An unretained hook ref was asserted as a journal node')
        if retained:
            if (not retained['complete'] or retained['result'] != 0
                    or retained['capture_submission'] != observed['capture_submission']
                    or any(retained['frozen'][key] != observed[key]
                           for key in ('source_layout', 'destination_layout', 'filter', *REGION_FIELDS))):
                raise ValueError('Journal event differs from the exact successful original BLIT')
        camera = receipt.get('camera_provenance', {}).get('copy_submit')
        if camera and (camera['first_submission'] != observed['capture_submission']
                       or camera['last_submission'] != observed['capture_submission']):
            raise ValueError('Camera capture counter was conflated with History submission')
        audited.append({'capture_sequence': sequence, 'source_present': present,
                        'original': deepcopy(observed['original']),
                        'history_submission': observed['history_submission'],
                        'capture_submission': observed['capture_submission'],
                        'event_retained_at_record': observed['original']['event_retained'],
                        'bridge_known': observed['bridge_known'],
                        'record_and_submit_snapshots_independently_available': hook is not None and submit is not None,
                        'delivered_record_and_submit_snapshots_available': stages is not None,
                        'journal_event_independently_available': retained is not None,
                        'lease': receipt.get('lease'), 'producer_completion_receipt': True})
    if not audited:
        raise ValueError('No completed capture receipt was supplied')
    return {'original_application_blit_calls': len(originals),
            'exact_original_blit_receipts': sum(row['bridge_known'] for row in audited),
            'generic_completed_capture_receipts': sum(not row['bridge_known'] for row in audited), 'receipts': audited,
            'retained_journal_events_matched': sum(row['journal_event_independently_available'] for row in audited),
            'hook_only_original_refs_matched': sum(row['bridge_known'] and not row['event_retained_at_record'] for row in audited),
            'delivered_lifecycle_snapshot_pairs': sum(row['delivered_record_and_submit_snapshots_available'] for row in audited),
            'generic_capture_ids_used_as_history_ids': False,
            'gpu_execution_order_known': False, 'descriptor_query_implies_read': False,
            'whole_scene_verified': False, 'common_scene_time_known': False, 'xr_submission': False}


def json_lines(path):
    return [decode(line) for line in read(path).splitlines() if line.strip()]


def audit_directory(directory, *, require_bridge=True, require_stages=False):
    """Read a parent-run WSI directory. Filenames are evidence, never commands."""
    directory = Path(directory)
    traces = sorted(directory.glob('vulkan-*.jsonl'))
    if len(traces) != 1 or not re.fullmatch(r'vulkan-[1-9][0-9]*\.jsonl', traces[0].name):
        raise ValueError('Require exactly one original process trace')
    process = int(traces[0].stem.split('-')[1])
    rows = json_lines(traces[0])
    errors = ('presentation_error', 'source_history_unavailable', 'source_history_error',
              'pose_capture_error', 'held_depth_export_error', 'camera_update_fatal')
    if any(row.get('event') in errors for row in rows):
        raise ValueError('Owned WSI layer error in existing trace')
    log = read(directory/'run.log').decode()
    if any(marker in log for marker in ('VUID-', 'Validation Error', 'SYNC-HAZARD-')):
        raise ValueError('Owned WSI validation failed')
    wrappers = [decode(read(path)) for path in sorted(directory.glob('source-history-*.json'))
                if path.name != 'source-history-fixture.json']
    if any(wrapper['process_id'] != process for wrapper in wrappers):
        raise ValueError('History reports were joined across original processes')
    devices = {integer(wrapper['device'], 1) for wrapper in wrappers}
    if len(devices) > 1:
        raise ValueError('History reports were joined across original devices')
    originals = json_lines(directory/'blit-fixture.jsonl')
    hooks = [row for row in rows if row.get('event') == 'presentation_blit_recorded']
    submissions = [row for row in rows if row.get('event') == 'presentation_blit_submitted']
    receipts = []
    exact_bytes = 0
    for path in sorted(directory.glob('image-*.json')):
        receipt = decode(read(path))
        match = re.fullmatch(r'image-([1-9][0-9]*)-([1-9][0-9]*)-([1-9][0-9]*)\.json', path.name)
        if (match is None or int(match[1]) != process or devices and int(match[2]) not in devices
                or int(match[3]) != receipt['sequence']):
            raise ValueError('Image receipt filename crosses process/device/capture identity')
        raw = read(path.with_suffix('.pixels'))
        expected = read(directory/('source-frame-'+str(receipt['present']-1)+'.pixels'))
        if raw != expected:
            raise ValueError('Capture pixels differ from that original source frame')
        exact_bytes += len(raw)
        receipts.append(receipt)
    for path in sorted(directory.glob('pose-pair-*.json')):
        pair = decode(read(path))
        match = re.fullmatch(r'pose-pair-([1-9][0-9]*)-([1-9][0-9]*)-([1-9][0-9]*)-([1-9][0-9]*)\.json', path.name)
        if (match is None or int(match[1]) != process or devices and int(match[2]) not in devices
                or (int(match[3]), int(match[4])) != (pair['session'], pair['request'])):
            raise ValueError('Pair receipt filename crosses process/device/request identity')
        no_inference(pair)
        if pair.get('xr_submission') is not False or pair.get('actual_xr_pose_source') is not False:
            raise ValueError('Owned diagnostic pair was promoted to actual XR submission')
        size = integer(pair['width'], 1, 4096)*integer(pair['height'], 1, 4096)*4
        raw = read(path.with_suffix('.pixels'))
        if len(raw) != size*2 or len(pair['eyes']) != 2:
            raise ValueError('Pair pixel/eye receipt size differs')
        for eye, receipt in enumerate(pair['eyes']):
            if receipt['eye'] != eye or raw[eye*size:(eye+1)*size] != read(directory/('source-frame-'+str(receipt['present']-1)+'.pixels')):
                raise ValueError('Pair pixels were joined to another frame or eye')
            exact_bytes += size
            receipts.append(receipt)
    for row in rows:
        if row.get('event') == 'gpu_capture_lease':
            if row.get('xr_submission') is not False or row.get('game_images_read_by_consumer') is not False:
                raise ValueError('Dry-run lease was promoted to actual XR or a pixel read')
            receipts.append(row)
    result = audit_receipts(receipts, originals, hooks, submissions, wrappers,
                            require_bridge=require_bridge, require_stages=require_stages)
    result['original_pixel_bytes_compared'] = exact_bytes
    paths = [path for path in sorted(directory.iterdir()) if path.is_file() and path.suffix in ('.json', '.jsonl', '.log', '.pixels')]
    result['input_sha256'] = {path.name: hashlib.sha256(read(path)).hexdigest() for path in paths}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation', type=Path, required=True, help='Already completed parent-run WSI case directory')
    parser.add_argument('--allow-generic', action='store_true', help='Accept completed generic captures while keeping exact History bridge count separate')
    parser.add_argument('--require-stages', action='store_true', help='Require saved record/submission values carried by every delivered capture; missing global trace rows are kept explicit')
    args = parser.parse_args()
    print(json.dumps({'schema_version': 1, 'scope': 'independent_read_only_blit_audit',
                      'new_gpu_execution_performed_by_validator': False,
                      'audit': audit_directory(args.validation, require_bridge=not args.allow_generic,
                                               require_stages=args.require_stages)}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(str(error))
