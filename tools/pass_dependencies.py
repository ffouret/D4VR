#!/usr/bin/env python3
"""Find possible attachment CLEAR producers in recorded metadata, without authorizing replay."""
from __future__ import annotations
import argparse
from collections import defaultdict
import json
from pathlib import Path
import frame_trace
import probe


def identity(a):
    fields = ('image_id','view_id','base_mip','mip_count','base_layer','layer_count')
    if (a.get('role') not in ('color','depth','stencil') or not a.get('image_known') or not a.get('view_known')
            or any(type(a.get(k)) is not int or a[k] < 0 for k in fields) or not a['image_id'] or not a['view_id']):
        return None
    return (a['role'],)+(tuple(a[k] for k in fields))


def covers(producer,consumer):
    if len(producer) != 4 or len(consumer) != 4:
        return False
    x,y,w,h = producer; a,b,c,d = consumer
    return min(w,h,c,d) > 0 and x <= a and y <= b and x+w >= a+c and y+h >= b+d


def queue(p):
    values = (p.get('queue_family'),p.get('queue_index'))
    return values if all(type(v) is int and 0 <= v < 0xffffffff for v in values) else None


def audit(data: dict,scopes: list[int]) -> dict:
    summary = frame_trace.summarize(data); passes = summary['passes']
    if not scopes or len(scopes) > 64 or len(set(scopes)) != len(scopes) or any(type(i) is not int or not 0 <= i < len(passes) for i in scopes):
        raise ValueError('Select 1 to 64 distinct recorded rendering scopes')
    history = defaultdict(list)
    for p in passes:
        for a in p['attachments']:
            key = identity(a)
            if key is not None:
                history[key].append((p,a))
    required = set(scopes); pending = list(scopes); processed = set(); loads = []
    while pending and len(processed) < 256:
        index = pending.pop(0)
        if index in processed:
            continue
        processed.add(index); consumer = passes[index]
        for attachment in consumer['attachments']:
            if attachment.get('load_op') != 0 or attachment['role'] == 'color_resolve':
                continue
            key = identity(attachment); previous = [(p,a) for p,a in history.get(key,[]) if p['index'] < index]
            source = None; reason = 'no_full_clear_observed'; examined = []
            for p,a in reversed(previous):
                examined.append(p['index'])
                if (queue(consumer) is None or queue(p) != queue(consumer)
                        or p['secondary_depth'] or consumer['secondary_depth']):
                    reason = 'unknown_or_cross_queue_or_secondary_order'; break
                if a.get('store_op') != 0 or a.get('load_op') not in (0,1):
                    reason = 'discard_or_unknown_load_store'; break
                if a['load_op'] == 1 and covers(p['area'],consumer['area']):
                    source = (p,a); break
            if key is None:
                reason = 'unknown_or_incomplete_attachment_identity'
            supporting = sorted(set(examined)) if source else []
            record = {'scope':index,'role':attachment['role'],'slot':attachment.get('slot'),
                      'image_id':attachment.get('image_id'),'view_id':attachment.get('view_id'),
                      'possible_clear_scope':source[0]['index'] if source else None,
                      'clear_value_words_available':bool(source and source[1].get('clear_value_words') is not None),
                      'supporting_attachment_scopes':supporting,'unresolved_reason':None if source else reason,
                      'gpu_dependency_proven':False}
            loads.append(record)
            for other in supporting:
                if other not in required:
                    required.add(other); pending.append(other)
    pipeline_states = summary['pipeline_state_declarations']
    return {'schema_version':1,'scope':'possible attachment producers in CPU metadata sequence only',
            'requested_scopes':scopes,'required_attachment_scope_candidates':sorted(required),
            'expanded_scope_count':len(processed),'closure_truncated':bool(pending),'load_dependencies':loads,
            'loads_without_observed_clear':sum(r['possible_clear_scope'] is None for r in loads),
            'possible_clear_scopes':sorted({r['possible_clear_scope'] for r in loads if r['possible_clear_scope'] is not None}),
            'clear_values_missing':sum(r['possible_clear_scope'] is not None and not r['clear_value_words_available'] for r in loads),
            'pipeline_declarations_recorded':len(pipeline_states),'effective_dynamic_states_known':sum(p['effective_dynamic_states_known'] for p in pipeline_states),
            'metadata_incomplete':summary['truncated'] or bool(summary['missing_command_buffers'] or summary['failed_or_incomplete_submissions']),
            'gpu_execution_order_known':False,'image_reads_and_barriers_traced':False,
            'shader_resource_dependencies_audited':False,'replay_allowed':False,'game_stereo_submitted':False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file',type=Path);parser.add_argument('--scopes',type=int,nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    result=audit(frame_trace.read_frame(args.file),args.scopes);probe.write_json(args.output,result)
    print(json.dumps({k:v for k,v in result.items() if k!='load_dependencies'},indent=2))


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TypeError) as error:
        print(f'D4VR dependencies: {error}');raise SystemExit(2)
