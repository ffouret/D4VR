#!/usr/bin/env python3
"""Independently audit CPU consumer receipts and existing owned WSI captures.

This program never compiles or executes a process, dispatches Vulkan, or opens
the game. Its inputs are the parent-run unit/forwarding logs, immutable unit
reports, and optionally four already completed WSI on/off captures. Descriptor
queries remain queries; neither placement nor shader consumption is inferred.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re

import source_history_validate
import vulkan_trace

HASH = re.compile(r'[0-9a-f]{64}')
COPY_FIELDS = ('observer_session','source_image','destination_image','recording','ordinal')
NO_INFERENCE = ('gpu_execution_order_known','last_gpu_writer_known',
                'initialized_pixels_verified','whole_scene_verified',
                'common_scene_time_known','xr_pose_known')
CPU_CASES = {'default_disabled','twins_query_not_bound','copy_declared_color_directed_blits',
             'reset_resubmit_reverse_returns','image_view_reuse_frozen_query',
             'descriptor_query_byte_bounds','forward_event_bound_closed_immutable',
             'hard_forward_occurrence_event_caps','unknown_placement_fault_frozen_window',
             'wsi_lifetime_stable_enumeration_and_reuse','serialization_bound_valid_envelope'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def strict_object(pairs):
    result={}
    for key,value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON field: '+key)
        result[key]=value
    return result


def decode(data):
    def reject(value):
        raise ValueError('Nonfinite JSON number: '+value)
    return json.loads(data,object_pairs_hook=strict_object,parse_constant=reject)


def read(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError('Require a regular input file: '+str(path))
    return path.read_bytes()


def integer(value,minimum=0,maximum=(1<<64)-1):
    if type(value) is not int or not minimum<=value<=maximum:
        raise ValueError('Invalid bounded integer observation')
    return value


def fingerprint(value):
    if not isinstance(value,str) or not HASH.fullmatch(value):
        raise ValueError('Require a complete SHA256 fingerprint')
    return value


def copy_ref(value,session):
    if not isinstance(value,dict) or set(value)!=set(COPY_FIELDS):
        raise ValueError('COPY reference fields changed')
    values=tuple(integer(value[key],1) for key in COPY_FIELDS)
    if values[0]!=session or values[1]==values[2]:
        raise ValueError('COPY reference crosses observer or conflates lifetimes')
    return values


def image_use(value,session):
    if integer(value['observer_session'],1)!=session:
        raise ValueError('Image use crosses observers')
    for key in ('image','view','aspects','base_mip','mip_count','base_layer','layer_count'):
        integer(value[key])
    for key in ('image_known','view_known','metadata_complete'):
        if type(value[key]) is not bool:
            raise ValueError('Image-use knowledge flag is not boolean')
    if value['image_known'] and not value['image']:
        raise ValueError('Known image use has no lifetime')
    if value['view_known'] and (not value['view'] or not value['image_known']):
        raise ValueError('Known view use has no exact image/view lifetime')


def fnv1a(data):
    value=14695981039346656037
    for byte in data:
        value=((value^byte)*1099511628211)&((1<<64)-1)
    return value


def query_receipt(query,session,target=None,byte_limit=256):
    integer(query['query'],1)
    image_use(query['image_use'],session)
    if target is not None and query['image_use']['image'] not in target[1:3]:
        raise ValueError('A twin or unrelated lifetime contaminated a target query window')
    if integer(query['type']) not in (1,2,3,10):
        raise ValueError('Descriptor query was not a typed image query')
    integer(query['layout'])
    for key in ('input_chain_complete','complete','bytes_complete'):
        if type(query[key]) is not bool:
            raise ValueError('Descriptor-query status is not boolean')
    for key in ('descriptor_selected','gpu_read_proven','placement_known','sampler_state_known'):
        if query[key] is not False:
            raise ValueError('A query was promoted to descriptor placement or a GPU read')
    size=integer(query['output_size'])
    encoded=query['bytes_hex']
    if not isinstance(encoded,str) or not re.fullmatch(r'(?:[0-9a-f]{2})*',encoded):
        raise ValueError('Descriptor output is not exact lowercase hexadecimal bytes')
    count=len(encoded)//2
    if count>byte_limit or count>size:
        raise ValueError('Descriptor query exceeded its byte/output bound')
    if query['bytes_complete'] and (not query['complete'] or count!=size):
        raise ValueError('Complete query output size/AFTER status differs')
    if not query['complete'] and count:
        raise ValueError('Pending descriptor query invented returned bytes')
    if integer(query['bytes_fnv1a64'])!=(fnv1a(bytes.fromhex(encoded)) if query['bytes_complete'] else 0):
        raise ValueError('Descriptor byte fingerprint differs from its exact retained bytes')
    before=integer(query['before_observed_submission'])
    after=integer(query['after_observed_submission'])
    if query['complete'] and after<before:
        raise ValueError('Descriptor AFTER precedes its CPU BEFORE submission counter')


def window_events(window):
    return [event for occurrence in window['occurrences']
            for command in occurrence['commands'] for event in command['events']]


def observation_scope(report):
    if report.get('observation_only') is not True:
        raise ValueError('Source consumer report is not observation only')
    shortened=report.get('serialization_truncated') is True
    for key in NO_INFERENCE:
        if (key in report or not shortened) and report.get(key) is not False:
            raise ValueError('Source consumer report claims unavailable scene/GPU information: '+key)
    for key in ('gpu_commands_inserted','application_calls_rejected'):
        if (key in report or not shortened) and (type(report.get(key)) is not int or report[key]!=0):
            raise ValueError('Source consumer report inserted or rejected application work')
    return integer(report.get('observer_session'),1)


def audit_history(report,*,enabled=None):
    """Check one structurally parsed report without joining another observer."""
    session=observation_scope(report)
    if report.get('serialization_truncated') is True:
        if report.get('reports')!=[]:
            raise ValueError('Truncated serialization must retain a valid empty report envelope')
        return {'observer_session':session,'consumer_windows':0,'serialization_truncated':True}
    limits=report['limits']
    if integer(report['live_event_nodes'])>integer(limits['live_events'],0,8192):
        raise ValueError('History event budget exceeded')
    if integer(report['snapshot_event_references'])>integer(limits['snapshot_references'],0,32768):
        raise ValueError('History snapshot-reference budget exceeded')
    if len(report['reports'])>integer(limits['reports'],0,8):
        raise ValueError('History report budget exceeded')
    consumer=report.get('consumer_observation_enabled') is True
    if enabled is not None and consumer is not enabled:
        raise ValueError('Source consumer opt-in mode does not match the observed report')
    consumer_limits=None
    if consumer:
        consumer_limits=report['consumer_limits']
        for name,cap in (('events_per_report',512),('effective_events_per_report',512),
                         ('effective_prefix_events_per_report',512),('occurrences_per_report',8),
                         ('descriptor_queries',64),('descriptor_queries_per_report',16),
                         ('descriptor_bytes_per_query',256),('color_attachments',8),
                         ('swapchains',16),('swapchain_images',64)):
            integer(consumer_limits[name],0,cap)
        catalog=report['descriptor_image_query_catalog']
        if (len(catalog)>consumer_limits['descriptor_queries']
                or len({query['query'] for query in catalog})!=len(catalog)
                or report['descriptor_query_catalog_scope']!='live_bounded_cache_at_report'):
            raise ValueError('Descriptor live-cache scope, identity or query bound differs')
        nodes=integer(report['descriptor_query_nodes'],0,64+16*8)
        if len(catalog)>nodes or integer(report['descriptor_query_payload_bytes'])>nodes*consumer_limits['descriptor_bytes_per_query']:
            raise ValueError('Retained descriptor query node/payload budget exceeded')
        for name in ('descriptor_query_calls','descriptor_query_evicted','descriptor_query_deduplicated',
                     'descriptor_query_unknown','late_descriptor_callbacks'):
            integer(report[name])
        for query in catalog:
            query_receipt(query,session,byte_limit=consumer_limits['descriptor_bytes_per_query'])
    windows=[]
    for captured in report['reports']:
        target=copy_ref(captured['target_copy'],session)
        if 'consumer_window' not in captured:
            if enabled:
                raise ValueError('Enabled source consumer window missing')
            continue
        window=captured['consumer_window']
        if window is None:
            if enabled:
                raise ValueError('Enabled source consumer window is null')
            continue
        if enabled is False:
            raise ValueError('Disabled source consumer observer emitted a window')
        if not consumer:
            raise ValueError('Consumer window exists without the explicit observer opt-in')
        if copy_ref(window['target_copy'],session)!=target:
            raise ValueError('Forward window belongs to a different exact COPY')
        for key in ('descriptor_selected','gpu_read_proven','placement_known','gpu_execution_order_known',
                    'presentation_receipt_resource_bridge_known','separate_stencil_and_depth_resolve_complete'):
            if window[key] is not False:
                raise ValueError('Consumer metadata claimed an unavailable resource/GPU/complete route bridge')
        for key in ('closed','truncated'):
            if type(window[key]) is not bool:
                raise ValueError('Consumer-window state is not boolean')
        observed=integer(window['observed_occurrences'],0,consumer_limits['occurrences_per_report'])
        event_count=len(window_events(window))
        if (len(window['occurrences'])>observed or event_count>consumer_limits['effective_events_per_report']
                or integer(window['event_count'])!=event_count):
            raise ValueError('Forward occurrence/event bounds exceeded')
        sequences=[integer(occurrence['submission'],1) for occurrence in window['occurrences']]
        if sequences!=sorted(set(sequences)) or any(value<captured['target_submission'] for value in sequences):
            raise ValueError('Consumer window contains a prior or duplicated CPU occurrence')
        for occurrence in window['occurrences']:
            if type(occurrence['complete']) is not bool or type(occurrence['result']) is not int:
                raise ValueError('Consumer window lost exact CPU AFTER status')
            for command in occurrence['commands']:
                integer(command['recording'],1)
                ordinals=[integer(event['ordinal'],1) for event in command['events']]
                if ordinals!=sorted(ordinals):
                    raise ValueError('Recorded CPU event ordinal decreased')
                if occurrence['submission']==captured['target_submission']:
                    position=(command['batch'],command['command'])
                    target_position=(captured['target_batch'],captured['target_command'])
                    if position<target_position or (position==target_position and any(value<=target[-1] for value in ordinals)):
                        raise ValueError('A COPY prefix contaminated the forward consumer window')
        queries=window['descriptor_image_queries']
        if len(queries)>consumer_limits['descriptor_queries_per_report'] or len({q['query'] for q in queries})!=len(queries):
            raise ValueError('Descriptor-query window bound or identity differs')
        for query in queries:
            query_receipt(query,session,target,consumer_limits['descriptor_bytes_per_query'])
        windows.append(window)
    return {'observer_session':session,'consumer_windows':len(windows),
            'descriptor_query_references':sum(len(window['descriptor_image_queries']) for window in windows),
            'forward_event_references':sum(len(window_events(window)) for window in windows),
            'serialization_truncated':False}


def audit_cpu(unit_summary,reports,forwarding_log):
    if unit_summary.get('source_consumer_observation_passed') is not True:
        raise ValueError('Source consumer CPU unit success summary missing')
    if type(unit_summary.get('gpu_dispatch_waits_or_rejections')) is not int or unit_summary['gpu_dispatch_waits_or_rejections']!=0:
        raise ValueError('CPU unit did not preserve its no-dispatch/no-rejection scope')
    if 'Vulkan forwarding, DGC, aliases, secondary/reused command buffers, errors and reset lifetime: passed' not in forwarding_log:
        raise ValueError('Original-call forwarding test success summary missing')
    forwarding=[decode(line) for line in forwarding_log.splitlines()
                if line.startswith('{') and 'source_consumer_forwarding_passed' in line]
    expected_forwarding={'source_consumer_forwarding_passed':True,'modes':3,'descriptor_queries':3,
                         'blits':9,'swapchain_calls':9,'uncovered_dispatches':6,'observer_gpu_calls':0}
    if (len(forwarding)!=1 or forwarding[0]!=expected_forwarding
            or any(type(forwarding[0].get(key)) is not type(value) for key,value in expected_forwarding.items())):
        raise ValueError('Dedicated consumer-hook exact forwarding controls missing')
    cases=reports['cases']
    names=[case['name'] for case in cases]
    if set(names)!=CPU_CASES or len(set(names))!=len(names) or unit_summary.get('cases')!=len(cases):
        raise ValueError('CPU report cases missing or ambiguous')
    integer(unit_summary.get('assertions'),1)
    checked=[{'name':case['name'],**audit_history(case['report'],enabled=case['name']!='default_disabled')} for case in cases]
    by_name={case['name']:case['report'] for case in cases}
    def first_window(name,index=0):
        return by_name[name]['reports'][index]['consumer_window']
    twin=first_window('twins_query_not_bound')
    if {query['bytes_hex'] for query in twin['descriptor_image_queries']}!={'0001020304050607','cccccccccccccccc'}:
        raise ValueError('Independent source/destination query byte snapshots differ')
    directed=window_events(first_window('copy_declared_color_directed_blits'))
    blits=[event for event in directed if event['kind']=='blit_image']
    if (not any(event['kind']=='declared_color_attachment' for event in directed) or len(blits)!=2
            or blits[0]['image']!=blits[1]['destination_image']
            or blits[0]['destination_image']!=blits[1]['image']
            or blits[0]['ordinal']>=blits[1]['ordinal']):
        raise ValueError('Directed original color-attachment/BLIT controls differ')
    reused=[first_window('image_view_reuse_frozen_query',index) for index in (0,1)]
    reused_queries=[window['descriptor_image_queries'] for window in reused]
    if (list(map(len,reused_queries))!=[1,1]
            or reused_queries[0][0]['bytes_hex']!='10203040'
            or reused_queries[1][0]['bytes_hex']!='50607080'
            or reused_queries[0][0]['image_use']['image']==reused_queries[1][0]['image_use']['image']
            or reused_queries[0][0]['image_use']['view']==reused_queries[1][0]['image_use']['view']):
        raise ValueError('Reused image/view handles lost their frozen independent generations')
    reset=by_name['reset_resubmit_reverse_returns']
    if len(reset['reports'])!=3 or reset['duplicate_after_calls']!=1:
        raise ValueError('Reset/resubmit or duplicate AFTER control differs')
    statuses=[(occurrence['complete'],occurrence['result']) for occurrence in first_window('reset_resubmit_reverse_returns')['occurrences']]
    if statuses!=[(True,0),(True,-4),(True,0)]:
        raise ValueError('Reversed application return statuses changed exact occurrences')
    bounded=by_name['descriptor_query_byte_bounds']
    if integer(bounded['overflow'])<1 or any(query['bytes_complete'] for query in first_window('descriptor_query_byte_bounds')['descriptor_image_queries']):
        raise ValueError('Partial/oversized descriptor output was promoted to complete')
    for name in ('forward_event_bound_closed_immutable','hard_forward_occurrence_event_caps'):
        window=first_window(name)
        if window['closed'] is not True or window['truncated'] is not True:
            raise ValueError('A bounded consumer window failed to close with a gap')
    if by_name['unknown_placement_fault_frozen_window']['faulted'] is not True:
        raise ValueError('History-only fault control missing')
    if by_name['serialization_bound_valid_envelope'].get('serialization_truncated') is not True:
        raise ValueError('Valid truncated serialization control missing')
    return {'unit_summary':unit_summary,'forwarding_summary':forwarding[0],'cases':checked,
            'forwarding_success_summary_present':True,
            'note':'CPU test logs and structurally checked snapshots; not a rendered descriptor-consumer certificate.'}


def command_mode(command,flag):
    settings=[argument.split('=',1)[1] for argument in command
              if isinstance(argument,str) and argument.startswith(flag+'=')]
    if len(settings)!=1 or settings[0] not in ('0','1'):
        raise ValueError('WSI command lacks one explicit source consumer mode')
    if command.count('D4VR_DEPTH_SOURCE_HISTORY=1')!=1:
        raise ValueError('WSI comparison must keep source history enabled in both controls')
    if command.count('D4VR_SOURCE_CONSUMER_FIXTURE=1')!=1:
        raise ValueError('Owned source consumer fixture must be identical in all WSI controls')
    if command.count('D4VR_FRAME_TRACE=1'):
        raise ValueError('Consumer observation unexpectedly enabled complete frame tracing')
    return settings[0]=='1'


def copy_bridges(data,reports):
    """Check exact existing COPY bridges while allowing declared color context."""
    by_ref={}
    for aggregate in reports:
        for captured in aggregate['reports']:
            key=copy_ref(captured['target_copy'],aggregate['observer_session'])
            by_ref[key]=captured
    result=[]
    for request,meta in sorted(data['exports'].items()):
        ref=meta['source_history']
        key=copy_ref(ref,ref['observer_session'])
        if key not in by_ref:
            raise ValueError('Owned depth export lacks its exact same-observer COPY bridge')
        captured=by_ref[key]
        images={image['id']:image for image in captured['images']}
        if any(image not in images or images[image]['known'] is not True
               or images[image]['format']!=126
               or (images[image]['width'],images[image]['height'])!=(data['fixture']['width'],data['fixture']['height'])
               for image in key[1:3]):
            raise ValueError('Exact target COPY image lifetimes/metadata are missing')
        targets=[(occurrence,command) for occurrence in captured['occurrences']
                 for command in occurrence['commands']
                 if occurrence['submission']==captured['target_submission']
                 and command['recording']==ref['recording'] and command['is_target_recording']]
        if len(targets)!=1:
            raise ValueError('Exact original COPY occurrence/recording is ambiguous')
        occurrence,command=targets[0]
        if (occurrence['complete'] is not True or occurrence['result']!=0
                or occurrence['capture_submission']!=meta['capture_submission']):
            raise ValueError('Exact original COPY BEFORE/AFTER correspondence changed')
        if any(event['ordinal']>ref['ordinal'] for event in command['events']):
            raise ValueError('Forward command contaminated the COPY prefix')
        copies=[event for event in command['events'] if event['kind']=='copy_image'
                and event['ordinal']==ref['ordinal'] and event['image']==ref['source_image']
                and event['destination_image']==ref['destination_image']]
        if len(copies)!=1:
            raise ValueError('Original COPY endpoint/ordinal does not identify one exact event')
        result.append({'request':request,'target_copy':ref,'target_submission':captured['target_submission'],
                       'capture_submission':occurrence['capture_submission']})
    return result


def owned_query_rows(directory):
    rows=[decode(line) for line in read(directory/'consumer-descriptor-fixture.jsonl').splitlines() if line]
    expected=[('source',0),('destination',0),('twin0',0),('source',16),
              ('destination',16),('twin1',32),('source',48),('destination',48)]
    if [(row['label'],row['frame']) for row in rows]!=expected or [row['ordinal'] for row in rows]!=list(range(1,9)):
        raise ValueError('Owned descriptor call labels/phases/ordinals differ')
    for row in rows:
        integer(row['image'],1);integer(row['view'],1)
        size=integer(row['size'],1,256)
        if (row['descriptor_type']!=2 or row['layout']!=1
                or row['descriptor_bound'] is not False or row['shader_read_verified'] is not False
                or not isinstance(row['bytes_hex'],str)
                or not re.fullmatch(r'[0-9a-f]{'+str(2*size)+'}',row['bytes_hex'])):
            raise ValueError('Owned original descriptor output or query-only scope differs')
    for label in ('source','destination'):
        uses=[row for row in rows if row['label']==label]
        if len({(row['image'],row['view'],row['size']) for row in uses})!=1:
            raise ValueError('Known fixed source/destination query changed its app-owned lifetime or size')
    if rows[0]['image']==rows[1]['image'] or rows[0]['view']==rows[1]['view']:
        raise ValueError('Owned source and destination query lifetimes were conflated')
    return rows


def compare_queries(rows,reports,enabled):
    queries={}
    catalogs={}
    target_images={}
    seen_candidates=set()
    for aggregate in reports:
        seen_candidates.update(image['id'] for image in aggregate['candidate_images'])
        if enabled:
            for name in ('descriptor_query_evicted','descriptor_query_unknown','late_descriptor_callbacks'):
                if aggregate[name]!=0:
                    raise ValueError('Owned WSI descriptor query observations have a gap')
            for query in aggregate['descriptor_image_query_catalog']:
                ordinal=integer(query['query'],1,8)
                original=rows[ordinal-1]
                if (query['type']!=original['descriptor_type'] or query['layout']!=original['layout']
                        or query['output_size']!=original['size'] or query['bytes_hex']!=original['bytes_hex']
                        or query['complete'] is not True or query['bytes_complete'] is not True):
                    raise ValueError('Global query catalog differs from a same-run original driver output')
                key=(aggregate['observer_session'],ordinal)
                if key in catalogs and catalogs[key]!=query:
                    raise ValueError('Completed live-cache query receipt changed')
                catalogs[key]=query
        for captured in aggregate['reports']:
            for label,key in (('source','source_image'),('destination','destination_image')):
                image=captured['target_copy'][key]
                if label in target_images and target_images[label]!=image:
                    raise ValueError('Known fixed target changed its exact History lifetime')
                target_images[label]=image
            if 'consumer_window' not in captured:
                continue
            target=captured['target_copy']
            for query in captured['consumer_window']['descriptor_image_queries']:
                ordinal=integer(query['query'],1,8)
                original=rows[ordinal-1]
                if original['label'] not in ('source','destination'):
                    raise ValueError('Unrelated twin query was relabeled as a target query')
                expected_image=target['source_image' if original['label']=='source' else 'destination_image']
                if (query['image_use']['image']!=expected_image or query['type']!=original['descriptor_type']
                        or query['layout']!=original['layout'] or query['output_size']!=original['size']
                        or query['bytes_hex']!=original['bytes_hex'] or query['complete'] is not True
                        or query['bytes_complete'] is not True or query['input_chain_complete'] is not True):
                    raise ValueError('History query bytes differ from the same-run original driver output')
                key=(aggregate['observer_session'],ordinal)
                if key in queries and queries[key]!=query:
                    raise ValueError('Completed query receipt changed between aggregate snapshots')
                queries[key]=query
    if enabled and {rows[ordinal-1]['label'] for _,ordinal in queries}!={'source','destination'}:
        raise ValueError('Enabled observer lacks source and destination original query receipts')
    if not enabled and queries:
        raise ValueError('Disabled observer emitted image query receipts')
    catalog_lifetimes={}
    if enabled:
        for (_,ordinal),query in catalogs.items():
            label=rows[ordinal-1]['label']
            use=query['image_use']
            lifetime=(use['image'],use['view'])
            if label in catalog_lifetimes and catalog_lifetimes[label]!=lifetime:
                raise ValueError('Repeated original query migrated to a different History generation')
            catalog_lifetimes[label]=lifetime
            if (use['image_known'] is not True or use['view_known'] is not True
                    or use['metadata_complete'] is not True or use['image'] not in seen_candidates
                    or (use['aspects'],use['base_mip'],use['mip_count'],use['base_layer'],use['layer_count'])!=(2,0,1,0,1)):
                raise ValueError('Original descriptor query lacks its exact candidate/view/range snapshot')
        if (set(catalog_lifetimes)!={'source','destination','twin0','twin1'}
                or len({use[0] for use in catalog_lifetimes.values()})!=4
                or len({use[1] for use in catalog_lifetimes.values()})!=4
                or any(catalog_lifetimes[label][0]!=target_images[label] for label in ('source','destination'))):
            raise ValueError('Source/destination and both twin generations were conflated or omitted')
        final=max(reports,key=lambda row:row['descriptor_query_calls'])
        if (final['descriptor_query_calls']!=8
                or final['descriptor_query_deduplicated']+len(final['descriptor_image_query_catalog'])!=8):
            raise ValueError('Original query call/deduplication accounting differs')
    return {'application_query_calls':len(rows),'exact_unique_target_query_receipts':len(queries),
            'exact_unique_catalog_query_receipts':len(catalogs),'catalog_lifetimes':catalog_lifetimes,
            'twin_queries_in_target_windows':0,'query_encoding_comparison_scope':'same_run_original_driver_outputs'}


def declared_paths(data,reports):
    """Return observed CPU positions and declared resource candidates only."""
    latest={}
    for aggregate in reports:
        for captured in aggregate['reports']:
            latest[copy_ref(captured['target_copy'],aggregate['observer_session'])]=captured
    result=[]
    for request,meta in sorted(data['exports'].items()):
        key=copy_ref(meta['source_history'],meta['source_history']['observer_session'])
        captured=latest[key]
        if 'consumer_window' not in captured:
            continue
        window=captured['consumer_window'];images={image['id']:image for image in window['images']}
        prefix_colors={}
        for occurrence in captured['occurrences']:
            if occurrence['submission']!=captured['target_submission']:
                continue
            for command in occurrence['commands']:
                if command['recording']!=key[3] or not command['is_target_recording']:
                    continue
                for event in command['events']:
                    if event['kind']=='declared_color_attachment' and event['associated_image']==key[1]:
                        prefix_colors[event['image']]=event['view']
        late=[];colors=[];blits=[];kinds={}
        for occurrence in window['occurrences']:
            for command in occurrence['commands']:
                for event in command['events']:
                    kind=event['kind'];kinds[kind]=kinds.get(kind,0)+1
                    position={'submission':occurrence['submission'],'batch':command['batch'],
                              'command':command['command'],'recording':command['recording'],'ordinal':event['ordinal'],
                              'cpu_return_complete':occurrence['complete'],'cpu_return_result':occurrence['result']}
                    if kind=='clear_depth_stencil' and event['image']==key[1] and event['clear_depth_bits']==data['fixture']['late_source_clear_depth_bits']:
                        late.append({**position,'source_image':event['image'],'clear_depth_bits':event['clear_depth_bits']})
                    if kind=='declared_color_attachment' and event['associated_image']==key[1]:
                        colors.append({**position,'declared_color_image':event['image'],'declared_color_view':event['view'],
                                       'associated_depth_image':event['associated_image']})
                    if kind=='blit_image':
                        destination=images.get(event['destination_image'],{})
                        blits.append({**position,'source_image':event['image'],'destination_image':event['destination_image'],
                                      'destination_origin':destination.get('origin','unknown'),
                                      'source_declared_with_target_depth_in_prefix':event['image'] in prefix_colors,
                                      'original_source_layout':event['layout'],'original_destination_layout':event['destination_layout'],
                                      'original_filter':event['filter'],
                                      'original_source_offsets':event['blit_source_offsets'],
                                      'original_destination_offsets':event['blit_destination_offsets']})
        result.append({'request':request,'target_copy':captured['target_copy'],
                       'observed_cpu_occurrences':window['observed_occurrences'],'closed':window['closed'],
                       'truncated':window['truncated'],'event_count':window['event_count'],'event_kinds':kinds,
                       'prefix_color_depth_declarations':prefix_colors,'later_original_source_clears':late,
                       'later_color_depth_declarations':colors,'later_original_blits':blits,
                       'declared_color_to_swapchain_candidate_observed':any(blit['source_declared_with_target_depth_in_prefix']
                                                                          and blit['destination_origin']=='swapchain' for blit in blits),
                       'gpu_execution_order_known':False,'descriptor_read_proven':False,
                       'color_capture_receipt_endpoint_bridge_proven':False})
    return result


def audit_wsi(directory,proof,layer,fixture,flag,root):
    if (proof.get('actual_wsi_loader') is not True or proof.get('game_rendered') is not False
            or proof.get('xr_submission') is not False or proof.get('validation_errors')!=0):
        raise ValueError('Owned WSI execution scope is missing or invalid')
    if (sha(read(layer))!=fingerprint(proof['layer_sha256'])
            or sha(read(fixture))!=fingerprint(proof['fixture_sha256'])):
        raise ValueError('Audited WSI layer/fixture identity changed')
    required={'native/vulkan/trace_layer.cpp','native/vulkan/source_history.cpp',
              'native/vulkan/source_history.hpp','native/core/source_history_id.hpp',
              'native/tests/present_layer_vulkan.cpp'}
    sources=proof.get('source_sha256',{})
    if not required.issubset(sources):
        raise ValueError('Owned WSI source fingerprints are incomplete')
    for name,expected in sources.items():
        path=Path(name)
        fingerprint(expected)
        # The parent runner pinned an earlier revision of this independent CPU
        # auditor. Re-evaluating captured data uses our current code; that is not
        # a change to any GPU fixture, layer, shader or original execution.
        if name=='tools/source_consumer_validate.py':
            continue
        if path.is_absolute() or '..' in path.parts or sha(read(root/path))!=expected:
            raise ValueError('Audited WSI source certificate changed: '+name)
    if len(proof.get('cases',[]))!=4:
        raise ValueError('Require native/runtime consumer off/on WSI observations')
    groups={}
    result=[]
    for case in proof['cases']:
        name=case['case']
        if not isinstance(name,str) or not re.fullmatch(r'[A-Za-z0-9_-]+',name):
            raise ValueError('Invalid WSI case directory')
        where=directory/name
        command=decode(read(where/'command.json'))
        enabled=command_mode(command,flag)
        environment='steam-runtime' if name.endswith('-runtime') else 'native' if name.endswith('-native') else None
        if environment is None or (environment,enabled) in groups:
            raise ValueError('WSI environment/mode coverage is ambiguous')
        log=read(where/'run.log').decode()
        if any(marker in log for marker in ('VUID-','Validation Error','SYNC-HAZARD-')):
            raise ValueError('Owned WSI validation failed')
        data=source_history_validate.common_artifacts(where,log)
        wrappers=[decode(read(path)) for path in sorted(where.glob('source-history-*.json'))
                  if path.name!='source-history-fixture.json']
        if any(wrapper.get('consumer_history_enabled') is not enabled or wrapper.get('frame_trace_enabled') is not False
               for wrapper in wrappers):
            raise ValueError('WSI wrapper consumer mode or frame-trace isolation changed')
        if len({(wrapper['process_id'],wrapper['device'],wrapper['history']['observer_session']) for wrapper in wrappers})!=1:
            raise ValueError('Owned WSI reports crossed a process/device/observer namespace')
        reports=[wrapper['history'] for wrapper in wrappers]
        if not reports:
            raise ValueError('Owned WSI aggregate reports missing')
        audits=[audit_history(report,enabled=enabled) for report in reports]
        bridges=copy_bridges(data,reports)
        query_check=compare_queries(owned_query_rows(where),reports,enabled)
        paths=declared_paths(data,reports)
        if (data['summary'].get('source_consumer_fixture') is not True
                or data['summary'].get('original_descriptor_image_queries')!=8
                or data['summary'].get('descriptor_queries_bound_or_sampled') is not False):
            raise ValueError('Owned descriptor fixture call counts or query-only scope differ')
        groups[environment,enabled]=data
        result.append({'case':name,'environment':environment,'consumer_observer_enabled':enabled,
                       'original_controls':data['summary'],
                       'original_reference_files':len(data['pixels']),
                       'original_reference_bytes':sum(map(len,data['pixels'].values())),
                       'aggregate_reports':len(reports),
                       'consumer_window_references_across_aggregate_snapshots':sum(row['consumer_windows'] for row in audits),
                       'unique_consumer_windows':len(paths),
                       'copy_bridges':bridges,'query_outputs':query_check,'declared_path_observations':paths,
                       'artifacts_sha256':{str(path.relative_to(where)):sha(read(path))
                                           for path in sorted(where.iterdir()) if path.is_file()},
                       'validation_errors':0})
    if set(groups)!={(env,mode) for env in ('native','steam-runtime') for mode in (False,True)}:
        raise ValueError('Native/runtime on/off WSI comparison incomplete')
    for environment in ('native','steam-runtime'):
        before,after=groups[environment,False],groups[environment,True]
        if before['pixels']!=after['pixels'] or before['summary']!=after['summary']:
            raise ValueError('Consumer observer changed original application counts or reference bytes')
    return {'cases':result,'same_environment_original_bytes_and_controls_equal':True,
            'total_original_reference_bytes':sum(case['original_reference_bytes'] for case in result),
            'original_bytes_compared_to_same_environment_controls':sum(case['original_reference_bytes'] for case in result if case['consumer_observer_enabled']),
            'unique_consumer_windows':sum(case['unique_consumer_windows'] for case in result),
            'previous_validation_auditor_source_sha256':sources.get('tools/source_consumer_validate.py'),
            'descriptor_queries_actually_exercised_by_wsi':True,
            'query_coverage_scope':'Eight app-owned CPU queries per WSI run; retained source/destination receipt bytes compared to same-run original outputs. Twin queries remain excluded from target windows, with no binding or read claim.'}


def validate(a):
    if a.output.exists() or a.output.is_symlink():
        raise ValueError('Use a new private audit output directory')
    root=Path(__file__).resolve().parents[1]
    inputs={'unit_log':a.unit_log,'unit_report':a.unit_report,'forwarding_log':a.forwarding_log}
    summary=decode(read(a.unit_log))
    cpu=audit_cpu(summary,decode(read(a.unit_report)),read(a.forwarding_log).decode())
    unit_proof_path=a.unit_proof or a.unit_report.parent/'proof.json'
    unit_binary=a.unit or a.unit_report.parent/'source_consumer_tests'
    unit_certificate=decode(read(unit_proof_path))
    if (unit_certificate.get('unit')!=summary
            or unit_certificate.get('actual_gpu_subprocesses')!=0
            or unit_certificate.get('gpu_order_proven') is not False
            or unit_certificate.get('normal_build',{}).get('passed') is not True
            or unit_certificate.get('normal_build',{}).get('vulkan_loader_linked') is not False):
        raise ValueError('CPU unit certificate scope or executed summary differs')
    for path,key in ((a.unit_report,'cases_sha256'),(a.unit_log,'unit_log_sha256'),(unit_binary,'binary_sha256')):
        if sha(read(path))!=fingerprint(unit_certificate[key]):
            raise ValueError('CPU unit certificate artifact identity changed')
    required_unit={'native/tests/source_consumer_tests.cpp','native/vulkan/source_history.cpp',
                   'native/vulkan/source_history.hpp','native/core/source_history_id.hpp'}
    if not required_unit.issubset(unit_certificate.get('source_sha256',{})):
        raise ValueError('CPU unit source certificate is incomplete')
    for name,expected in unit_certificate['source_sha256'].items():
        path=Path(name)
        if path.is_absolute() or '..' in path.parts or sha(read(root/path))!=fingerprint(expected):
            raise ValueError('CPU unit source certificate changed: '+name)
    cpu['certificate']=unit_certificate
    inputs.update(unit_certificate=unit_proof_path,unit_binary=unit_binary)
    wsi=None
    if a.validation:
        if not a.layer or not a.fixture:
            raise ValueError('WSI audit requires the exact layer and fixture files')
        layer=a.layer/vulkan_trace.LIBRARY if a.layer.is_dir() else a.layer
        proof_path=a.validation/'proof.json'
        wsi=audit_wsi(a.validation,decode(read(proof_path)),layer,a.fixture,a.consumer_flag,root)
        inputs.update(wsi_proof=proof_path,layer=layer,fixture=a.fixture)
    result={'schema_version':1,'scope':'independent_read_only_cpu_audit',
            'new_gpu_execution_performed_by_validator':False,'game_rendered':False,
            'xr_submission':False,'gpu_execution_order_known':False,
            'descriptor_query_implies_binding_or_read':False,'placement_known':False,
            'selected_descriptor_slot_known':False,'whole_scene_verified':False,
            'common_scene_time_known':False,'cpu':cpu,'wsi':wsi,
            'input_sha256':{name:sha(read(path)) for name,path in inputs.items()},
            'source_sha256':{'tools/source_consumer_validate.py':sha(read(Path(__file__))),
                             'tools/source_history_validate.py':sha(read(root/'tools/source_history_validate.py'))},
            'limits':['CPU occurrence order and API return status do not establish GPU execution or completion.',
                      'A query encoding, declared attachment or blit endpoint is not a proven shader read.',
                      'Opaque descriptor bytes do not establish host placement, active binding or dynamic index.',
                      'Unknown pNext, aliasing, unobserved routes and bounded-window gaps remain unknown.',
                      'No cross-process or cross-observer resource-identity join is made.']}
    a.output.mkdir(mode=0o700,parents=True)
    path=a.output/'proof.json'
    path.write_text(json.dumps(result,indent=2)+'\n');path.chmod(0o600)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('unit-report','unit-log','forwarding-log','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    for name in ('validation','layer','fixture','unit-proof','unit'):
        parser.add_argument('--'+name,type=Path)
    parser.add_argument('--consumer-flag',default='D4VR_SOURCE_CONSUMER_HISTORY')
    print(json.dumps(validate(parser.parse_args()),indent=2))


if __name__=='__main__':
    try:
        main()
    except (OSError,ValueError,KeyError,TypeError) as error:
        raise SystemExit(str(error))
