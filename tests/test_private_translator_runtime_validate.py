"""Independent causal tests of supplied OUTER bytes and APP arguments.

The portable seeds are exact outputs of the new CPU mock fixture, not runtime
captures. Discovery never compiles or starts a child. The explicit CLI at the
bottom consumes only the new CPU fixture executables supplied by CTest.
"""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import private_translator_runtime_validate as model

FIXTURES = Path(__file__).resolve().parent / 'fixtures'
KNOWN = 'supplied_outer_copy_and_APP_argument_relation_known'
U64_MAX = (1 << 64) - 1
CLAIMS = (
    'input_facts_attested', 'opaque_descriptor_decoded',
    'actual_source_contents_at_copy_proven', 'current_contents_at_draw_known',
    'unchanged_since_copy_proven', 'sampled_resource_GPU_read_proven',
    'generic_PE_to_Unix_resource_mapping', 'renderer_owner_known',
    'exclusive_phase_attribution_proven', 'global_API_absence_proven',
    'old_process_or_backing_gate_repaired', 'public_device_create_result_known',
    'source_runtime_relation_known', 'same_run_bind_provenance_known',
    'getter_to_bind_relation_known', 'backing_known',
    'loaded_binary_source_equivalence_proven', 'all_original_arguments_captured',
    'current_descriptor_bytes_known', 'readback_bytes_origin_known',
    'generic_hidden_sret_pointer_mapping_known', 'physical_device_mapping_known',
    'source_memory_attested', 'all_source_calls_observed',
    'exclusive_thread_attribution_proven',
)


def seed(mode='on'):
    return json.loads((FIXTURES / ('private_translator_runtime_' + mode + '.json')).read_bytes())


def groups(v):
    result = {}
    for edge in v['app_journal']['events']:
        result.setdefault(edge['ordinal'], []).append(edge)
    return list(result.values())


def edges(v, ordinal):
    return [e for e in v['app_journal']['events'] if e['ordinal'] == ordinal]


def original(v, ref):
    return edges(v, ref['ordinal'])


def args_change(v, ref, **changes):
    for edge in original(v, ref):
        edge['args'].update(copy.deepcopy(changes))


def result_change(v, ref, value):
    for edge in original(v, ref):
        if edge['edge'] == 'after':
            edge['result'] = copy.deepcopy(value)


def renumber(v, ordered=None, clock_start=10001, ordinal_start=1001):
    """Change only APP chronology, including every detached CallRef.

    SOURCE is a UTF8 string and is deliberately never traversed or retimed.
    This is a test transformation, not a parser or normalization path.
    """
    ordered = groups(v) if ordered is None else ordered
    mapping = {}
    transition = clock_start
    events = []
    for index, pair in enumerate(ordered):
        before = next(e for e in pair if e['edge'] == 'before')
        after = next((e for e in pair if e['edge'] == 'after'), None)
        old = before['ordinal']
        new = dict(ordinal=ordinal_start + index, entry=transition,
                   **{'return': transition + 1 if after is not None else 0})
        mapping[old] = new
        for edge in pair:
            edge['ordinal'] = new['ordinal']
            edge['transition'] = new['entry' if edge['edge'] == 'before' else 'return']
            events.append(edge)
        transition += len(pair)
    def refs(x):
        if type(x) is dict:
            if set(x) == {'ordinal', 'entry', 'return'} and x['ordinal'] in mapping:
                x.update(mapping[x['ordinal']])
            else:
                for value in x.values():
                    refs(value)
        elif type(x) is list:
            for value in x:
                refs(value)
    # Ref replacement occurs before the newly assigned event IDs are installed
    # in the root, so event dictionaries cannot masquerade as old CallRefs.
    v['app_journal']['events'] = []
    refs(v)
    v['app_journal']['events'] = sorted(events, key=lambda e: e['transition'])
    return v


def move_before(v, moving_ref, anchor_ref):
    ordered = groups(v)
    pair = next(p for p in ordered if p[0]['ordinal'] == moving_ref['ordinal'])
    ordered.remove(pair)
    position = next(i for i, p in enumerate(ordered) if p[0]['ordinal'] == anchor_ref['ordinal'])
    ordered.insert(position, pair)
    return renumber(v, ordered)


def insert_call_before(v, anchor, api, args, result=None, kind='void', case=1):
    temporary = max(e['ordinal'] for e in v['app_journal']['events']) + 1000
    pair = [dict(ordinal=temporary, transition=0, api=api, edge=edge,
                 case_ordinal=case, return_kind=kind, args=copy.deepcopy(args),
                 original_returned=edge == 'after',
                 result=copy.deepcopy(result) if edge == 'after' else None)
            for edge in ('before', 'after')]
    ordered = groups(v)
    pos = next(i for i, p in enumerate(ordered) if p[0]['ordinal'] == anchor['ordinal'])
    ordered.insert(pos, pair)
    return renumber(v, ordered)


def replace_outer(v, index, text):
    """Keep all declarations and actual copied spans coherent with new bytes."""
    receipt = v['source_receipts'][index]
    raw = text.encode('utf8')
    receipt.update(outer_wire_text=text, outer_bytes=len(raw),
                   outer_sha256=hashlib.sha256(raw).hexdigest())
    seal = original(v, receipt['seal_call'])[-1]
    seal['result']['outputs_after']['bytes'] = len(raw)
    ids = {r['ordinal'] for r in [receipt['primary_copy_call']] + receipt['repeat_copy_calls']}
    for b in v['copy_buffers']:
        if b['call']['ordinal'] not in ids:
            continue
        capacity = len(raw) + 17
        b.update(capacity=capacity, allocation_bytes=capacity + 32,
                 base_pointer=0x10000000 + b['buffer_id'] * 0x10000)
        b['before_hex'] = (b'\xc3' * 16 + b'\xa5' * capacity + b'\x3c' * 16).hex()
        b['after_hex'] = (b'\xc3' * 16 + raw + b'\xa5' * 17 + b'\x3c' * 16).hex()
        args_change(v, b['call'], capacity=capacity, output_pointer=b['base_pointer'] + 16)
        result_change(v, b['call'], dict(status=0, outputs_after=dict(written=len(raw), required=len(raw))))
    return v


def alter_outer(v, index, change):
    outer = json.loads(v['source_receipts'][index]['outer_wire_text'])
    change(outer)
    return replace_outer(v, index, json.dumps(outer, separators=(',', ':'), ensure_ascii=True))


def overlap_returns(v, first, second):
    """Make two genuine calls overlap, with the earlier ENTRY returning last."""
    transition = {e['transition']: e['transition'] * 10 for e in v['app_journal']['events']}
    transition[first['return']] = second['return'] * 10 + 1
    def update(x):
        if type(x) is dict:
            if set(x) == {'ordinal', 'entry', 'return'}:
                x['entry'] = transition[x['entry']]
                if x['return']: x['return'] = transition[x['return']]
            else:
                for child in x.values(): update(child)
        elif type(x) is list:
            for child in x: update(child)
    update(v)
    for e in v['app_journal']['events']:
        e['transition'] = transition[e['transition']]
    v['app_journal']['events'].sort(key=lambda e: e['transition'])
    return v


class PrivateRuntimeTests(unittest.TestCase):
    def setUp(self):
        p = patch.object(subprocess, 'Popen', side_effect=AssertionError('Pure CPU analysis starts no child'))
        p.start()
        self.addCleanup(p.stop)

    def denied(self, value):
        try:
            result = model.analyze(value)
        except ValueError:
            return
        self.assertIs(result[KNOWN], False)

    def positive(self, value):
        r = model.analyze(value)
        self.assertIs(r[KNOWN], True)
        self.assertEqual(r['claims'], {k: False for k in CLAIMS})
        return r

    def test_real_new_CPU_seeds_on_off_faulted(self):
        for mode, wanted, receipts in (('on', True, 2), ('off', False, 0), ('faulted', False, 2)):
            with self.subTest(mode=mode):
                v = seed(mode)
                r = model.analyze(v)
                self.assertIs(r[KNOWN], wanted)
                self.assertEqual(len(r['source_receipts']), receipts)
                self.assertEqual(r['claims'], {k: False for k in CLAIMS})
                self.assertFalse(r['readback_digest_recomputed_here'])
                for c in r['cases']:
                    self.assertFalse(c['readback_bytes_origin_known'])
        r = self.positive(seed())
        self.assertEqual([c['historical_origin_app_resource'] for c in r['cases']],
                         [dict(raw=101, generation=51), dict(raw=102, generation=52)])
        self.assertEqual([len(c['typed_links']) for c in r['cases']], [3, 7])

    def test_real_runtime_disabled_context_can_Seal_and_Copy_diagnostic_payloads(self):
        # Exact fourth native CPU output: no synthetic reconstruction of a
        # disabled producer, and no reuse of the compiled-OFF profile.
        raw = (FIXTURES / 'private_translator_runtime_disabled.json').read_bytes()
        v = json.loads(raw)
        completed = [e for e in v['app_journal']['events'] if e['edge'] == 'after']
        begin = [e for e in completed if e['api'] == 'D4VR_TypedCopyBeginV1']
        expects = [e for e in completed if e['api'] == 'D4VR_TypedCopyExpectV1']
        seals = [e for e in completed if e['api'] == 'D4VR_TypedCopySealV1']
        self.assertEqual(len(begin), 1)
        self.assertEqual(begin[0]['args']['enabled'], 0)
        self.assertEqual(begin[0]['result']['status'], 0)
        self.assertEqual([e['result']['status'] for e in expects], [2] * 5)
        self.assertEqual([e['result']['status'] for e in seals], [0, 0])
        self.assertEqual([e['result']['outputs_after']['bytes'] for e in seals], [3109, 3109])
        self.assertEqual(v['mode'], 'on')
        real = model.audit_private_source_wire
        with patch.object(model, 'audit_private_source_wire', wraps=real) as probe:
            report = model.analyze(raw)
        self.assertEqual(probe.call_count, 2)
        self.assertFalse(report[KNOWN])
        self.assertFalse(report['control_replay']['enabled'])
        self.assertFalse(report['control_replay']['ready'])
        self.assertEqual(report['claims'], {key: False for key in CLAIMS})
        self.assertEqual(len(report['source_receipts']), 2)
        for i, receipt in enumerate(v['source_receipts']):
            child = real(receipt['outer_wire_text'].encode('utf8'))
            self.assertFalse(child['private_source_history_known'])
            self.assertEqual(report['source_receipts'][i]['private_control_audit'], child)
            self.assertEqual(report['cases'][i]['private_control_audit'], child)

    def test_full_child_once_per_primary_repeat_copies_do_not_reaudit(self):
        real = model.audit_private_source_wire
        for mode, count in (('on', 2), ('faulted', 2), ('off', 0)):
            v = seed(mode)
            with patch.object(model, 'audit_private_source_wire', wraps=real) as probe:
                r = model.analyze(v)
            self.assertEqual(probe.call_count, count)
            for i, item in enumerate(v['source_receipts']):
                expected = real(item['outer_wire_text'].encode('utf8'))
                self.assertEqual(r['source_receipts'][i]['private_control_audit'], expected)
                self.assertEqual(r['cases'][i]['private_control_audit'], expected)

    def test_OFF_cannot_be_retagged_as_present_exports_or_source(self):
        v = seed('off'); v['control_binding'] = seed()['control_binding']; self.denied(v)
        v = seed('off'); v['mode'] = 'on'; self.denied(v)
        v = seed(); v['mode'] = 'off'; self.denied(v)

    def test_all_stronger_claims_exact_false_and_origin_is_only_supplied(self):
        for key in CLAIMS:
            v = seed(); v['claims'][key] = True; self.denied(v)
        v = seed(); v['facts_origin'] = 'supplied_owned_PE_original'
        r = self.positive(v)
        self.assertFalse(r['claims']['input_facts_attested'])

    def test_exact_outer_whitespace_is_preserved_not_canonicalized(self):
        v = seed()
        for i in range(2):
            replace_outer(v, i, v['source_receipts'][i]['outer_wire_text'] + ' \n')
        r = self.positive(v)
        for i, item in enumerate(v['source_receipts']):
            self.assertEqual(r['source_receipts'][i]['outer_sha256'], hashlib.sha256(item['outer_wire_text'].encode()).hexdigest())
            self.assertTrue(r['source_receipts'][i]['original']['outer_wire_text'].endswith(' \n'))

    def test_declared_text_digest_and_written_bytes_must_all_agree(self):
        for field, value in (('outer_sha256', '0' * 64), ('outer_bytes', 1), ('outer_wire_text', '{}')):
            v = seed(); v['source_receipts'][0][field] = value; self.denied(v)
        v = seed(); b = v['copy_buffers'][1]; raw = bytearray.fromhex(b['after_hex']); raw[17] ^= 1; b['after_hex'] = raw.hex(); self.denied(v)

    def test_prefix_suffix_unused_capacity_and_before_initialization_are_checked(self):
        for field, offset in (('after_hex', 0), ('after_hex', -1), ('after_hex', -17), ('before_hex', 16)):
            v = seed(); b = v['copy_buffers'][0]; raw = bytearray.fromhex(b[field]); raw[offset] ^= 1; b[field] = raw.hex(); self.denied(v)

    def test_Copy_written_required_capacity_are_original_typed_outputs(self):
        for key in ('written', 'required'):
            for value in (0, True, 1 << 32):
                v = seed(); c = original(v, v['source_receipts'][0]['primary_copy_call'])[-1]; c['result']['outputs_after'][key] = value; self.denied(v)
        v = seed(); args_change(v, v['source_receipts'][0]['primary_copy_call'], capacity=16385); self.denied(v)

    def test_Copy_output_and_scalar_spans_are_checked_without_memory_reads(self):
        for key, value in (('base_pointer', U64_MAX), ('retained_until_final_snapshot', False), ('buffer_generation', True)):
            v = seed(); v['copy_buffers'][0][key] = value; self.denied(v)
        v = seed(); b = v['copy_buffers'][0]; args_change(v, b['call'], written_pointer=b['base_pointer'] + 16); self.denied(v)
        v = seed(); b = v['copy_buffers'][0]; a = original(v, b['call'])[0]['args']; args_change(v, b['call'], required_pointer=a['written_pointer']); self.denied(v)
        v = seed(); b = v['copy_buffers'][1]; b['base_pointer'] = v['copy_buffers'][0]['base_pointer']; args_change(v, b['call'], output_pointer=b['base_pointer'] + 16); self.denied(v)

    def test_Begin_Seal_and_footprint_pointer_aliases_are_not_silent_outputs(self):
        v = seed(); ref = v['control_binding']['begin_call']; a = original(v, ref)[0]['args']; args_change(v, ref, run_output_pointer=a['nonce_pointer'] + 8); self.denied(v)
        v = seed(); ref = v['cases'][0]['seal_call']; a = original(v, ref)[0]['args']; args_change(v, ref, bytes_output_pointer=a['owner_alias_output_pointer'] + 4); self.denied(v)
        v = seed(); ref = v['cases'][0]['readback']['footprint']['call']; a = original(v, ref)[0]['args']; args_change(v, ref, row_bytes_output_pointer=a['layout_output_pointer'] + 8); self.denied(v)
        for key in ('nonce_pointer', 'run_output_pointer'):
            v = seed(); args_change(v, v['control_binding']['begin_call'], **{key: U64_MAX}); self.denied(v)

    def test_every_original_control_or_footprint_output_span_has_checked_end(self):
        for family, keys in (
            ('Seal', ('owner_alias_output_pointer', 'bytes_output_pointer')),
            ('Retain', ('new_owner_output_pointer',)),
            ('Copy', ('written_pointer', 'required_pointer')),
            ('Footprint', ('layout_output_pointer', 'num_rows_output_pointer', 'row_bytes_output_pointer', 'total_bytes_output_pointer')),
        ):
            for key in keys:
                v = seed()
                ref = v['cases'][0]['seal_call'] if family == 'Seal' else v['control_binding']['retains'][0] if family == 'Retain' else v['cases'][0]['copy_call'] if family == 'Copy' else v['cases'][0]['readback']['footprint']['call']
                args_change(v, ref, **{key: U64_MAX}); self.denied(v)

    def test_NULL_Copy_has_no_invented_buffer_and_obeys_cap_before_pointer(self):
        for small, bad_run in ((False, False), (True, False), (False, True)):
            v = seed(); r = v['source_receipts'][0]; ref = r['repeat_copy_calls'].pop()
            v['copy_buffers'] = [b for b in v['copy_buffers'] if b['call']['ordinal'] != ref['ordinal']]
            args_change(v, ref, output_pointer=0, capacity=1 if small else 16384, **({'run_token': 999} if bad_run else {}))
            required = 0 if bad_run else r['outer_bytes']
            result_change(v, ref, dict(status=5 if small else 1, outputs_after=dict(written=0, required=required)))
            report = model.analyze(v)
            self.assertFalse(report[KNOWN]); self.assertEqual(len(report['source_receipts']), 2)
            self.assertEqual(len(report['copy_buffers']), 3)
            self.assertIn('control_or_source_unavailable', report['reasons'])
            result_change(v, ref, dict(status=5 if small else 1, outputs_after=dict(written=0, required=required + 1)))
            self.denied(v)

    def test_SMALL_nonNULL_Copy_preserves_whole_storage_without_fabricated_payload(self):
        v = seed(); r = v['source_receipts'][0]; ref = r['repeat_copy_calls'].pop()
        b = next(b for b in v['copy_buffers'] if b['call']['ordinal'] == ref['ordinal'])
        b.update(capacity=1, allocation_bytes=33)
        b['before_hex'] = (b'\xc3' * 16 + b'\xa5' + b'\x3c' * 16).hex()
        b['after_hex'] = b['before_hex']
        args_change(v, ref, capacity=1)
        result_change(v, ref, dict(status=5, outputs_after=dict(written=0, required=r['outer_bytes'])))
        report = model.analyze(v)
        self.assertFalse(report[KNOWN]); self.assertEqual(len(report['source_receipts']), 2)
        b['after_hex'] = '00' + b['after_hex'][2:]; self.denied(v)

    def test_retain_alias_distinct_drop_once_and_same_run(self):
        for mutate in (
            lambda v: result_change(v, v['control_binding']['retains'][0], dict(status=0, outputs_after=dict(new_owner_alias=1))),
            lambda v: args_change(v, v['source_receipts'][0]['primary_copy_call'], run_token=99),
            lambda v: args_change(v, v['control_binding']['drops'][1], owner_alias=1),
            lambda v: args_change(v, v['control_binding']['retains'][0], owner_alias=999),
        ):
            v = seed(); mutate(v); self.denied(v)
        v = seed(); move_before(v, v['control_binding']['drops'][0], v['source_receipts'][0]['primary_copy_call']); self.denied(v)

    def test_drop_original_alias_keeps_distinct_retained_alias_and_bytes_live(self):
        v = seed(); move_before(v, v['control_binding']['drops'][0], v['source_receipts'][0]['repeat_copy_calls'][0])
        report = self.positive(v)
        self.assertEqual(report['control_replay']['live_owners'], [])
        self.assertEqual([x['original_outer_bytes'] for x in report['control_replay']['copies']],
                         [v['source_receipts'][0]['outer_bytes']] * 2 + [v['source_receipts'][1]['outer_bytes']] * 2)

    def test_control_nonce_abi_role_sequence_and_exports_are_exact(self):
        for key, value in (('nonce_text', 'f' * 32), ('abi_version', 2), ('nonce_bytes', 31), ('enabled', True)):
            v = seed(); args_change(v, v['control_binding']['begin_call'], **{key: value}); self.denied(v)
        v = seed(); args_change(v, v['control_binding']['expect_calls'][1], role=1); self.denied(v)
        v = seed(); v['control_binding']['exports'][0]['address'] = True; self.denied(v)

    def test_Seal_before_late_Copy_or_after_Execute_cannot_certify_history(self):
        v = seed(); move_before(v, v['cases'][1]['seal_call'], v['cases'][1]['typed_calls'][-1]); self.denied(v)
        v = seed(); move_before(v, v['cases'][1]['execute_call'], v['cases'][1]['seal_call']); self.denied(v)
        v = seed(); args_change(v, v['cases'][1]['seal_call'], sequence=1); self.denied(v)

    def test_overlapping_control_and_command_returns_are_not_serialized_by_parser(self):
        v = seed(); overlap_returns(v, v['cases'][0]['seal_call'], v['cases'][0]['copy_call']); self.denied(v)
        v = seed(); overlap_returns(v, v['cases'][0]['root0_call'], v['cases'][0]['close_call']); self.denied(v)

    def test_source_nonce_prefix_epoch_and_getter_ID_cannot_be_substituted(self):
        for change in (
            lambda o: o['getter_wire'].update(run_id='f' * 32),
            lambda o: o['getter_wire']['native_wire']['producer'].update(epoch=99),
            lambda o: o['getter_wire']['getters']['records'][0].update(id=1),
            lambda o: o['getter_wire']['native_wire']['snapshot']['events'][0]['before']['resource'].update(raw=102, generation=2),
        ):
            v = seed(); alter_outer(v, 1, change); self.denied(v)

    def test_old_child_object_is_not_original_private_OUTER_bytes(self):
        v = seed(); old_child = json.loads(v['source_receipts'][0]['outer_wire_text'])['getter_wire']
        replace_outer(v, 0, json.dumps(old_child, separators=(',', ':')))
        self.denied(v)

    def test_hidden_getter_pointer_is_not_equated_to_APP_scalar_getter(self):
        def shift(o):
            for record in o['getter_wire']['getters']['records']:
                for key in ('caller_output_pointer', 'returned_pointer'):
                    if record[key] is not None:
                        record[key] += 16
        v = seed()
        for i in range(2): alter_outer(v, i, shift)
        self.positive(v)
        v = seed(); alter_outer(v, 1, shift); self.denied(v)

    def test_APP_clock_and_generation_namespaces_can_differ_from_SOURCE(self):
        v = seed(); renumber(v, clock_start=50001, ordinal_start=5001)
        def app_refs(x):
            if type(x) is dict:
                if set(x) == {'raw', 'generation'}: x['generation'] += 1000
                else:
                    for value in x.values(): app_refs(value)
            elif type(x) is list:
                for value in x: app_refs(value)
        app_refs(v)
        self.positive(v)

    def test_original_void_never_gains_an_HRESULT_or_a_value(self):
        v0 = seed()
        for ref in (v0['cases'][0]['typed_calls'][0], v0['cases'][1]['typed_calls'][-1], v0['cases'][0]['execute_call'], v0['cases'][0]['readback']['footprint']['call']):
            v = seed(); result_change(v, ref, dict(hresult=0)); self.denied(v)
        v = seed(); ref = v['cases'][0]['typed_calls'][0]
        for e in original(v, ref): e['return_kind'] = 'HRESULT'
        result_change(v, ref, dict(hresult=0, output=None)); self.denied(v)

    def test_before_arguments_are_immutable_through_RETURN(self):
        for ref in (seed()['cases'][0]['root0_call'], seed()['cases'][1]['typed_calls'][-1]):
            v = seed(); original(v, ref)[-1]['args']['device' if 'device' in original(v, ref)[-1]['args'] else 'gpu_handle'] = 999; self.denied(v)

    def test_APP_pending_and_original_failure_remain_unavailable(self):
        v = seed(); ordinal = v['cases'][0]['map_call']['ordinal']
        v['app_journal']['events'] = [e for e in v['app_journal']['events'] if not (e['ordinal'] == ordinal and e['edge'] == 'after')]
        r = model.analyze(v); self.assertFalse(r[KNOWN]); self.assertIn('original_APP_call_failed_or_pending', r['reasons'])
        v = seed(); result_change(v, v['cases'][0]['map_call'], dict(hresult=-1, output=None)); self.denied(v)

    def test_release_selected_before_Seal_is_unavailable_even_nonfinal(self):
        for role, kind in (('visible_heap', 'heap'), ('resource_B', 'resource'), ('device', 'device')):
            v = seed(); create = original(v, v['cases'][0]['object_calls'][role])[-1]
            selected = copy.deepcopy(create['result']['output'])
            # Move the actual cleanup original instead of fabricating a second
            # Release of the same occurrence; preserve a nonfinal ULONG=1.
            release = next(p for p in groups(v) if p[0]['api'] == 'Release' and p[0]['args']['object'] == selected)
            release[-1]['result']['value'] = 1
            ref = dict(ordinal=release[0]['ordinal'], entry=release[0]['transition'], **{'return': release[-1]['transition']})
            move_before(v, ref, v['cases'][0]['seal_call']); self.denied(v)
        self.positive(seed())  # all original cleanup Releases occur later

    def test_wrong_parent_generation_or_reused_object_cannot_join(self):
        for role in ('resource_A', 'cpu_heap', 'visible_heap'):
            v = seed(); ref = v['cases'][0]['object_calls'][role]
            a = copy.deepcopy(original(v, ref)[0]['args']); a['device']['generation'] += 1
            args_change(v, ref, **a); self.denied(v)
        v = seed(); v['cases'][0]['selected_heap']['generation'] += 1; self.denied(v)
        v = seed(); c = v['cases'][0]; result_change(v, c['object_calls']['resource_B'], dict(hresult=0, output=dict(raw=101, generation=52))); self.denied(v)

    def test_unreferenced_root_or_heap_override_before_Close_is_not_ignored(self):
        for api, key, changes in (('SetGraphicsRootDescriptorTable', 'root0_call', dict(gpu_handle=9001)), ('SetDescriptorHeaps', 'bind_call', dict(heaps=[dict(raw=201, generation=61)]))):
            v = seed(); c = v['cases'][0]; a = copy.deepcopy(original(v, c[key])[0]['args']); a.update(changes)
            insert_call_before(v, c['close_call'], api, a); self.denied(v)

    def test_extra_Execute_or_Signal_before_Map_is_not_ignored(self):
        for api, key in (('ExecuteCommandLists', 'execute_call'), ('Signal', 'signal_call')):
            v = seed(); c = v['cases'][0]; pair = original(v, c[key]);
            insert_call_before(v, c['map_call'], api, pair[0]['args'], pair[-1]['result'], pair[0]['return_kind']); self.denied(v)

    def test_command_Reset_Close_epoch_and_root0_are_exact(self):
        for key, changes in (('reset_call', dict(recording_epoch=3)), ('root0_call', dict(parameter=1)), ('close_call', dict(recording_epoch=2)), ('root0_call', dict(gpu_handle=9001))):
            v = seed(); args_change(v, v['cases'][0][key], **changes); self.denied(v)
        v = seed(); result_change(v, v['cases'][0]['close_call'], dict(hresult=-1, output=None)); self.denied(v)

    def test_late_typed_copy_direction_slot_count_and_parent_are_exact(self):
        for key, change in (('count', 0), ('heap_type', 1), ('device', dict(raw=11, generation=42))):
            v = seed(); args_change(v, v['cases'][1]['typed_calls'][-1], **{key: change}); self.denied(v)
        v = seed(); ref = v['cases'][1]['typed_calls'][-1]; a = original(v, ref)[0]['args']; args_change(v, ref, source=a['destination'], destination=a['source']); self.denied(v)
        v = seed(); ref = v['cases'][1]['typed_calls'][-1]; a = copy.deepcopy(original(v, ref)[0]['args']); a['source'].update(index=0, cpu_handle=1000); args_change(v, ref, **a); self.denied(v)

    def test_getter_equal_value_duplicate_is_not_original_identity(self):
        v = seed(); c = v['cases'][0]; ref = c['getter_calls']['visible_cpu']; pair = original(v, ref)
        insert_call_before(v, c['bind_call'], pair[0]['api'], pair[0]['args'], pair[-1]['result'], pair[0]['return_kind']); self.denied(v)
        v = seed(); move_before(v, v['cases'][0]['typed_calls'][0], v['cases'][0]['getter_calls']['visible_cpu']); self.denied(v)

    def test_omitted_or_reversed_original_edges_are_not_repaired(self):
        v = seed(); ordinal = v['cases'][0]['typed_calls'][1]['ordinal']; v['app_journal']['events'] = [e for e in v['app_journal']['events'] if e['ordinal'] != ordinal]; self.denied(v)
        v = seed(); ref = v['cases'][0]['typed_calls'][0]; pair = original(v, ref); pair[0]['transition'], pair[1]['transition'] = pair[1]['transition'], pair[0]['transition']; self.denied(v)

    def test_queue_fence_registration_wait_and_completed_values_join(self):
        for refkey, changes in (('signal_call', dict(value=0)), ('event_call', dict(event=602)), ('signal_call', dict(fence=dict(raw=501, generation=92)))):
            v = seed(); args_change(v, v['cases'][0][refkey], **changes); self.denied(v)
        for key, value in (('wait_call', 258), ('completed_call', U64_MAX), ('completed_call', 0)):
            v = seed(); result_change(v, v['cases'][0][key], dict(value=value)); self.denied(v)
        v = seed(); args_change(v, v['cases'][0]['object_calls']['fence'], initial_value=999); self.denied(v)
        v = seed(); args_change(v, v['cases'][1]['signal_call'], value=1); self.denied(v)

    def test_footprint_actual_total_and_Maprange_not_packed_file_assumption(self):
        v = seed()
        for c in v['cases']:
            r = c['readback']; r['footprint']['outputs_after']['total_bytes'] += 17; r['buffer_resource_width'] += 17
        ref = v['cases'][0]['object_calls']['readback']; a = copy.deepcopy(original(v, ref)[0]['args']); a['description']['width'] += 17; args_change(v, ref, **a)
        self.positive(v)
        for change in ('pitch', 'total', 'width', 'map'):
            v = seed(); c = v['cases'][0]
            if change == 'pitch': c['readback']['footprint']['outputs_after']['layout']['row_pitch'] = 1025
            elif change == 'total': c['readback']['footprint']['outputs_after']['total_bytes'] = 147455
            elif change == 'width': c['readback']['buffer_resource_width'] = 32
            else: args_change(v, c['map_call'], range=[0, 32])
            self.denied(v)

    def test_readback_original_creation_success_Map_pointer_and_hash_framing(self):
        v = seed(); result_change(v, v['cases'][0]['object_calls']['readback'], dict(hresult=-1, output=None)); self.denied(v)
        for pointer in (0, True, 1 << 64):
            v = seed(); result_change(v, v['cases'][0]['map_call'], dict(hresult=0, output=dict(pointer=pointer))); self.denied(v)
        v = seed(); v['cases'][0]['readback']['sha256'] = 'z' * 64; self.denied(v)

    def test_silent_overwrite_observational_twins_never_prove_current_or_GPU(self):
        a = seed(); b = seed()
        # Two possible external contents share this supplied call history. The
        # pure auditor sees only a declared hash, not bytes at a Map pointer.
        b['cases'][0]['readback']['sha256'] = 'f' * 64
        ra = self.positive(a); rb = self.positive(b)
        self.assertEqual(ra['cases'][0]['historical_origin_app_resource'], rb['cases'][0]['historical_origin_app_resource'])
        self.assertFalse(rb['cases'][0]['readback_bytes_origin_known'])
        self.assertEqual(ra['claims'], rb['claims'])

    def test_strict_bool_U64_UINT_and_exact_nulls(self):
        for change in (
            lambda v: v.update(schema_version=True),
            lambda v: v['cases'][0].update(selected_index=True),
            lambda v: v['cases'][0]['command'].update(raw=True),
            lambda v: v['cases'][0]['readback'].update(bytes=147456.0),
            lambda v: args_change(v, v['cases'][0]['root0_call'], gpu_handle=1 << 64),
            lambda v: args_change(v, v['cases'][0]['typed_calls'][0], device=True),
        ):
            v = seed(); change(v); self.denied(v)

    def test_UTF8_duplicate_keys_float_nonfinite_and_unknown_keys(self):
        raw = json.dumps(seed(), separators=(',', ':'))
        for bad in (raw.encode('utf16'), raw.replace('"schema_version":1', '"schema_version":1,"schema_version":1', 1), raw.replace('"schema_version":1', '"schema_version":1.0', 1), raw.replace('"schema_version":1', '"schema_version":NaN', 1)):
            self.denied(bad)
        v = seed(); v['invented'] = False; self.denied(v)

    def test_malformed_API_or_object_kind_has_public_ValueError_contract(self):
        for bad in ([], {}):
            v = seed()
            for edge in original(v, v['cases'][0]['execute_call']): edge['api'] = bad
            with self.assertRaises(ValueError): model.analyze(v)
            v = seed()
            for edge in v['app_journal']['events']:
                if edge['api'] == 'Release': edge['args']['object_kind'] = bad
            with self.assertRaises(ValueError): model.analyze(v)

    def test_outer_exact_byte_node_depth_and_edge_bounds(self):
        raw = json.dumps(seed(), separators=(',', ':')).encode()
        self.positive(raw + b' ' * (524288 - len(raw)))
        self.denied(raw + b' ' * (524289 - len(raw)))
        v = seed(); v['claims']['extra'] = [0] * 16385; self.denied(v)
        v = seed(); x = []
        for _ in range(20): x = [x]
        v['claims']['extra'] = x; self.denied(v)
        v = seed(); v['app_journal']['events'] *= 4; self.denied(v)
        v = seed(); v['source_receipts'].append(copy.deepcopy(v['source_receipts'][0])); self.denied(v)
        v = seed(); v['copy_buffers'].append(copy.deepcopy(v['copy_buffers'][0])); self.denied(v)

    def test_full_report_budget_is_complete_or_refuses_not_truncated(self):
        r = self.positive(seed()); size = len(json.dumps(r, ensure_ascii=True, allow_nan=False, separators=(',', ':')).encode('ascii'))
        self.assertEqual(model._complete_budget(r, size), size)
        with self.assertRaises(ValueError): model._complete_budget(r, size - 1)
        with patch.object(model, 'MAX_OUTPUT', 128):
            with self.assertRaises(ValueError): model.analyze(seed())

    def test_detached_report_survives_eighty_input_recycles_without_replay(self):
        v = seed(); report = self.positive(v); saved = copy.deepcopy(report)
        for i in range(80):
            v['cases'][0]['recording_epoch'] = i + 100
            v['copy_buffers'][0]['after_hex'] = ''
            v['source_receipts'].clear()
            v = seed()
            self.assertEqual(report, saved)

    def test_analyze_performs_no_file_process_or_driver_operations(self):
        v = seed()
        with patch('builtins.open', side_effect=AssertionError('No input opening')), \
             patch.object(Path, 'open', side_effect=AssertionError('No file opening')), \
             patch('os.open', side_effect=AssertionError('No OS opening')), \
             patch('os.system', side_effect=AssertionError('No process')), \
             patch('os.popen', side_effect=AssertionError('No process')):
            self.positive(v)


def bridge(normal_path, off_path):
    """Explicit CPU CTest bridge; never runs during unittest discovery."""
    normal = Path(normal_path).resolve(strict=True)
    off = Path(off_path).resolve(strict=True)
    summaries = []
    for exe, mode, expected, primaries in ((normal, 'on', True, 2), (normal, 'faulted', False, 2), (off, 'off', False, 0)):
        completed = subprocess.run([str(exe), '--mode', mode, '--emit-json'],
                                   capture_output=True, check=True, timeout=60)
        if len(completed.stdout) > 524288:
            raise ValueError('Entire new CPU input exceeds contract')
        real = model.audit_private_source_wire
        with patch.object(model, 'audit_private_source_wire', wraps=real) as probe:
            result = model.analyze(completed.stdout)
        if result[KNOWN] is not expected or probe.call_count != primaries:
            raise ValueError('Independent ON/OFF/faulted schedule or single child invocation')
        if result['claims'] != {k: False for k in CLAIMS}:
            raise ValueError('No stronger fact promotion')
        if expected and [x['historical_origin_app_resource'] for x in result['cases']] != [dict(raw=101, generation=51), dict(raw=102, generation=52)]:
            raise ValueError('Independent two-case typed origins')
        for i, child in enumerate(result['source_receipts']):
            if child['private_control_audit'] != result['cases'][i]['private_control_audit']:
                raise ValueError('Full child result preserved')
        size = model._complete_budget(result, 4194304)
        summaries.append(dict(mode=mode, relation=expected, primary_audits=probe.call_count,
                              complete_report_bytes=size))
    print(json.dumps(dict(cpu_mock_only=True, results=summaries, stronger_facts=False), sort_keys=True))


if __name__ == '__main__':
    if len(sys.argv) == 5 and sys.argv[1] == '--native-wire' and sys.argv[3] == '--off-native-wire':
        bridge(sys.argv[2], sys.argv[4])
    else:
        unittest.main()
