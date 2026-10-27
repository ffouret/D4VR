"""Independent synthetic original calls; optional NEW CPU-native wire bridge."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import typed_copy_transport_validate as a
from test_typed_descriptor_copy import lifetime, ref, resource_description, srv_description, slot

RUN = '438471ad23b94c929c9199a840435067'


def raw_pixels(image):
    out = bytearray()
    for y in range(144):
        yy = min(7, max(0, ((2 * y + 1) * 4) // 144 - 1))
        for x in range(256):
            xx = min(7, max(0, ((2 * x + 1) * 4) // 256 + 1))
            rgba = ((17 + 13 * xx, 29 + 19 * yy, 41 + 7 * xx + 11 * yy, 255) if image == 0 else
                    (233 - 11 * xx, 211 - 17 * yy, 197 - 5 * xx - 9 * yy, 83 + 7 * xx + 11 * yy))
            out.extend((rgba[2], rgba[1], rgba[0], rgba[3]))
    return bytes(out)


class Facts:
    """Independent call log counter and native transition counter are distinct."""
    def __init__(self, enabled=True):
        self.enabled = enabled
        self.rows = []
        self.clock = 0
        self.original_clock = 0
        self.ordinal = 0
        self.events = []
        self.contexts = []
        self.wires = []

    def call(self, api, args, *, case=0, phase=0, kind='void', value=None,
             generation=0, transport_generation=0, mutation=0):
        self.ordinal += 1
        entry = self.clock + 1
        self.clock += 2
        original_entry = self.original_clock + 1
        self.original_clock += 2
        for returned in (False, True):
            self.rows.append(dict(ordinal=self.ordinal,
                 original_transition=original_entry + int(returned),
                 transition=(entry + int(returned)) if self.enabled else 0,
                 run_id=RUN, case_ordinal=case, api_phase=phase, api=api,
                 edge='after' if returned else 'before', returns=kind, returned=returned,
                 mutation_call_id=mutation, transport_call_id=mutation if self.enabled else 0,
                 object_generation=generation, transport_generation=transport_generation if self.enabled else 0,
                 hresult=0 if kind == 'HRESULT' and returned else None,
                 value=copy.deepcopy(value) if returned else None, args=copy.deepcopy(args)))
        return dict(ordinal=self.ordinal, entry=original_entry, **{'return': original_entry + 1},
                    transport_entry=entry if self.enabled else 0,
                    transport_return=entry + 1 if self.enabled else 0)

    def creation(self, api, raw, generation, args, tracked=False):
        c = self.call(api, args, kind='HRESULT', generation=generation,
                      transport_generation=generation if tracked else 0,
                      value=dict(raw=raw, object_generation=generation,
                                 transport_generation=generation if tracked and self.enabled else 0))
        return lifetime(raw, generation, c['transport_entry'])

    def mutate(self, api, args, case, phase, role=None):
        mutation = len(self.events) + 1
        original = copy.deepcopy(args)
        if role is not None:
            original['role'] = role
        call = self.call(api, original, case=case, phase=phase, mutation=mutation)
        self.events.append(dict(call_id=mutation, entry=call['transport_entry'],
              **{'return': call['transport_return']}, api=api, return_kind='void',
              original_returned=True, result=None,
              phase_before=dict(case_ordinal=case, api_phase=phase),
              phase_after=dict(case_ordinal=case, api_phase=phase), before=copy.deepcopy(args)))
        return call

    def journal(self):
        return dict(schema_version=1, purpose='owned_typed_copy_original_call_journal', run_id=RUN,
                    owned_fixture=True, before_after_original_D3D12_calls=True,
                    transition_domain='Session_common_transition',
                    original_transition_domain='independent_original_call_transition', events=self.rows)


def owned_fixture(enabled=True, wait=False, future_fence=False):
    f = Facts(enabled)
    device = f.creation('D3D12CreateDevice', 11, 1, {'adapter': None, 'minimum_feature_level': 45056}, True)
    resources = []
    for i in range(2):
        args = dict(device=11, device_ref=ref(11, 1), role='texture_A' if i == 0 else 'texture_B',
                    tracked_texture=True, description=resource_description())
        r = f.creation('CreateCommittedResource', 101 + i, i + 1, args, True)
        r.update(device=ref(11, 1), description=resource_description())
        resources.append(r)
    heaps = []
    for i, role in enumerate(('cpu', 'visible')):
        args = dict(device=11, device_ref=ref(11, 1), role='cpu_srv' if i == 0 else 'visible_srv',
                    tracked_srv_heap=True, type=0, count=2 if i == 0 else 1, flags=i, node_mask=0)
        h = f.creation('CreateDescriptorHeap', 201 + i, i + 1, args, True)
        h.update(device=ref(11, 1), role=role, heap_type=0, count=2 if i == 0 else 1,
                 flags=i, node_mask=0, cpu_start=1000 if i == 0 else 2000, increment=32)
        heaps.append(h)
    for i in range(2):
        f.call('GetCPUDescriptorHandleForHeapStart', dict(heap=201 + i, generation=1 + i),
               kind='uint64', value={'cpu_start': 1000 if i == 0 else 2000})
        if i == 1:
            f.call('GetGPUDescriptorHandleForHeapStart', dict(heap=202, generation=2), kind='uint64', value={'gpu_start': 8000})
        f.call('GetDescriptorHandleIncrementSize', dict(device=11, type=0), kind='uint32', value={'increment': 32})
    f.creation('CreateCommandList', 301, 1, {'type': 0})
    if not future_fence:
        f.creation('CreateFence', 401, 1, {'initial_value': 0, 'flags': 0})
    f.creation('CreateCommittedResource', 501, 1, dict(role='readback', tracked_texture=False))
    for case, name in enumerate(a.NAMES, 1):
        for i in range(2):
            f.mutate('CreateShaderResourceView', dict(device=ref(11, 1), resource=ref(101 + i, 1 + i),
                     destination=slot('cpu', i), description=srv_description()), case, 1,
                     'cpu_source_A' if i == 0 else 'cpu_source_B')
        selected = f.mutate('CreateShaderResourceView', dict(device=ref(11, 1), resource=ref(101, 1),
                            destination=slot('visible'), description=srv_description()), case, 1, 'selected_visible_slot')
        command = dict(raw=301, generation=1, epoch=case + 1)
        cmdargs = dict(command=301, command_generation=1, recording_epoch=case + 1)
        reset = f.call('ResetCommandList', dict(command=301, command_generation=1, previous_epoch=case), case=case, kind='HRESULT')
        bind = f.call('SetDescriptorHeaps', dict(**cmdargs, heaps=[202, 203], visible_heap_ref=ref(202, 2)), case=case)
        root = f.call('SetGraphicsRootDescriptorTable', dict(**cmdargs, parameter=0, heap=ref(202, 2), slot=0, gpu_handle=8000), case=case)
        close = f.call('CloseCommandList', cmdargs, case=case, kind='HRESULT')
        if case == 2:
            selected = f.mutate('CopyDescriptorsSimple', dict(device=ref(11, 1), heap_type=0, count=1,
                         source=slot('cpu', 1), destination=slot('visible')), case, 4)
        cutoff = f.clock if enabled else 0
        snap = dict(schema_version=1, purpose='owned_typed_D3D12_descriptor_history', run_id=RUN,
                    domain='owned_two_texture_three_SRV_slots', at_transition=cutoff,
                    complete=True, gap=False, device=copy.deepcopy(device),
                    resources=copy.deepcopy(resources), heaps=copy.deepcopy(heaps),
                    events=copy.deepcopy(f.events), selected_destination=slot('visible'))
        wire = dict(schema_version=1, purpose='owned_original_typed_copy_transport', sequence=case,
                    run_id=RUN, producer=dict(enabled=enabled, faulted=False, gap=False, dropped=0,
                    clock=cutoff, epoch=1, nonfinal_release_returns=0, snapshot_available=enabled),
                    snapshot=snap if enabled else None, **{k: False for k in a.FALSE_FACTS})
        execute = f.call('ExecuteCommandLists', dict(queue=601, command_lists=[301], command_generation=1, recording_epoch=case + 1), case=case, phase=5)
        fence_args = dict(queue=601, fence=401, fence_generation=1, fence_value=case)
        signal = f.call('Signal', fence_args, case=case, phase=5, kind='HRESULT')
        f.call('GetCompletedValue', dict(fence=401, fence_generation=1), case=case, phase=5,
               kind='uint64', value={'completed_value': case - 1 if wait else case})
        event_call = wait_call = None
        if wait:
            event_call = f.call('SetEventOnCompletion', dict(**fence_args, event=701), case=case, phase=5, kind='HRESULT')
            wait_call = f.call('WaitForSingleObject', dict(event=701, timeout=4294967295), case=case, phase=5, kind='uint32', value={'wait_result': 0})
        completed = f.call('GetCompletedValue', dict(fence=401, fence_generation=1), case=case, phase=5,
                           kind='uint64', value={'completed_value': case})
        mapped = f.call('MapReadback', dict(resource=501, range=[0, 147456]), case=case, phase=6, kind='HRESULT', value={'pointer': 9000})
        raw = raw_pixels(case - 1)
        c = dict(schema_version=1, purpose='owned_typed_copy_case_context', run_id=RUN,
                 case_ordinal=case, name=name, transport_enabled=enabled, transport_capture_succeeded=True,
                 transport_clock_domain='Session_common_transition', journal_clock_domain='independent_original_call_transition',
                 snapshot_cutoff=cutoff, transport_epoch=1, command=command, selected_destination=slot('visible'),
                 visible_heap=dict(raw=202, generation=2, cpu_start=2000, gpu_start=8000, increment=32, count=1),
                 selected_mutation_call_id=len(f.events), selected_mutation_call=selected,
                 reset_call=reset, bind_call=bind, root0_call=root, close_call=close,
                 execute_call=execute, signal_call=signal, event_call=event_call, wait_call=wait_call,
                 completed_call=completed, readback_call=mapped,
                 producer=dict(fence_raw=401, fence_generation=1, fence_value=case, completed_value=case,
                               raw_file=name + '.bgra', raw_bytes=147456, raw_sha256=hashlib.sha256(raw).hexdigest()),
                 current_contents_at_draw_known=False, sampled_resource_GPU_read_proven=False, renderer_owner_known=False)
        f.wires.append(wire)
        f.contexts.append(c)
    if future_fence:
        f.creation('CreateFence', 401, 1, {'initial_value': 0, 'flags': 0})
    return f


class TransportAuditTests(unittest.TestCase):
    def setUp(self):
        for target in ('subprocess.Popen', 'subprocess.run', 'os.system', 'os.popen'):
            p = patch(target, side_effect=AssertionError('No process in pure auditor tests'))
            p.start()
            self.addCleanup(p.stop)

    def audit(self, f, index=0, raw=True):
        return a.audit_case(f.wires[index], f.journal(), f.contexts[index], raw_pixels(index) if raw else None)

    def test_both_new_cases_exact_original_arguments_and_pixels(self):
        f = owned_fixture()
        for i in range(2):
            r = self.audit(f, i)
            self.assertTrue(r['selected_history_known'])
            self.assertTrue(r['original_recording_binding_joined'])
            self.assertTrue(r['original_same_queue_fence_completion_observed'])
            self.assertEqual(r['pixel_audit']['mismatched_bytes'], 0)
            self.assertEqual(len(r['native_history']['history']['mutations']), 3 if i == 0 else 7)
            for k in a.FALSE_FACTS:
                self.assertIs(r[k], False)

    def test_real_wait_branch_binds_event_and_actual_return(self):
        f = owned_fixture(wait=True)
        self.assertTrue(self.audit(f)['original_same_queue_fence_completion_observed'])
        f.contexts[0]['event_call'] = None
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_disabled_same_original_cases_and_pixels_never_retagged(self):
        f = owned_fixture(False)
        for i in range(2):
            r = self.audit(f, i)
            self.assertFalse(r['selected_history_known'])
            self.assertFalse(r['original_typed_arguments_joined'])
            self.assertTrue(r['original_recording_binding_joined'])
        with self.assertRaises(ValueError):
            a.audit_case(f.wires[0], f.journal(), f.contexts[0], require_history=True)

    def test_root_GPUhandle_heap_generation_epoch_and_command_forgeries(self):
        for key in ('gpu_start', 'raw', 'generation', 'increment'):
            f = owned_fixture()
            f.contexts[0]['visible_heap'][key] += 1
            with self.subTest(heap=key), self.assertRaises(ValueError):
                self.audit(f)
        for key in ('raw', 'generation', 'epoch'):
            f = owned_fixture()
            f.contexts[0]['command'][key] += 1
            with self.subTest(command=key), self.assertRaises(ValueError):
                self.audit(f)

    def test_coherent_epoch_relabel_still_requires_real_Reset_history(self):
        f = owned_fixture()
        f.contexts[0]['command']['epoch'] = 100
        for row in f.rows:
            if row['case_ordinal'] == 1:
                if 'recording_epoch' in row['args']:
                    row['args']['recording_epoch'] = 100
                if 'previous_epoch' in row['args']:
                    row['args']['previous_epoch'] = 99
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_old_refs_late_copy_and_capture_after_Execute_rejected(self):
        for key in ('bind_call', 'root0_call', 'close_call', 'execute_call', 'completed_call', 'readback_call'):
            f = owned_fixture()
            f.contexts[1][key] = copy.deepcopy(f.contexts[0][key])
            with self.subTest(ref=key), self.assertRaises(ValueError):
                self.audit(f, 1)
        f = owned_fixture()
        f.contexts[1]['selected_mutation_call'] = f.contexts[1]['close_call']
        with self.assertRaises(ValueError):
            self.audit(f, 1)
        f = owned_fixture()
        f.wires[0]['producer']['clock'] = f.contexts[0]['execute_call']['transport_return']
        f.wires[0]['snapshot']['at_transition'] = f.wires[0]['producer']['clock']
        f.contexts[0]['snapshot_cutoff'] = f.wires[0]['producer']['clock']
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_unknown_or_omitted_typed_mutation_and_BEFORE_args_rejected(self):
        f = owned_fixture()
        f.wires[1]['snapshot']['events'].pop(0)
        with self.assertRaises(ValueError):
            self.audit(f, 1)
        f = owned_fixture()
        f.wires[1]['snapshot']['events'][-1]['before']['source'] = slot('cpu', 0)
        with self.assertRaises(ValueError):
            self.audit(f, 1)
        f = owned_fixture()
        for row in f.rows:
            if row['api'] == 'CreateShaderResourceView' and row['mutation_call_id'] == 1:
                row['args']['description']['format'] = 50
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_clearing_transport_label_cannot_erase_an_original_mutation(self):
        f = owned_fixture()
        f.wires[0]['snapshot']['events'].pop(1)
        for row in f.rows:
            if row['mutation_call_id'] == 2:
                row['transport_call_id'] = 0
        with self.assertRaises(ValueError):
            self.audit(f)
        # Clearing the independent original label as well still cannot hide
        # that an original CreateShaderResourceView call actually occurred.
        for row in f.rows:
            if row['mutation_call_id'] == 2:
                row['mutation_call_id'] = 0
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_native_sample_bridge_preserves_independent_expected_origin(self):
        f = owned_fixture()
        sample = dict(typed_copy_transport_cpu_passed=True, cases=1, assertions=1, original_calls=1,
                      GPU_or_Wine_calls=0, samples=[dict(name='synthetic_native_wire', expected_known=True,
                                                       expected_origin=ref(101, 1), wire=f.wires[0])])
        self.assertEqual(a.audit_native_samples(sample)['known_histories'], 1)
        sample['samples'][0]['expected_origin']['raw'] = 102
        with self.assertRaises(ValueError):
            a.audit_native_samples(sample)

    def test_scalar_COM_device_and_one_increment_return_cannot_hide_behind_refs(self):
        for api in ('CreateCommittedResource', 'CreateDescriptorHeap'):
            f = owned_fixture()
            for row in f.rows:
                if row['api'] == api and row['args'].get('tracked_texture', row['args'].get('tracked_srv_heap', False)):
                    row['args']['device'] = 999
            with self.subTest(api=api), self.assertRaises(ValueError):
                self.audit(f)
        f = owned_fixture()
        changed = next(row for row in f.rows if row['api'] == 'GetDescriptorHandleIncrementSize' and row['edge'] == 'after')
        changed['value']['increment'] = 64
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_unrecorded_reset_late_root_or_Release_cannot_be_made_old(self):
        f = owned_fixture()
        # The original and native clocks have different labels, but the same
        # actual occurrence cannot move backwards within their mapping.
        f.rows[-1]['transition'] = 1
        with self.assertRaises(ValueError):
            self.audit(f)
        f = owned_fixture(False)
        f.rows[0]['transition'] = 1
        f.rows[1]['transition'] = 2
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_void_result_forgery_and_original_pending_edges_rejected(self):
        f = owned_fixture()
        row = next(x for x in f.rows if x['api'] == 'CreateShaderResourceView' and x['edge'] == 'after')
        row['hresult'] = 0
        with self.assertRaises(ValueError):
            self.audit(f)
        f = owned_fixture()
        f.rows.pop()
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_completed_fence_queue_event_wait_and_readback_forgery(self):
        for key in ('fence_raw', 'fence_generation', 'fence_value', 'completed_value'):
            f = owned_fixture(wait=True)
            f.contexts[0]['producer'][key] += 1
            with self.subTest(field=key), self.assertRaises(ValueError):
                self.audit(f)
        for api, field in (('ExecuteCommandLists', 'queue'), ('WaitForSingleObject', 'event')):
            f = owned_fixture(wait=True)
            for row in f.rows:
                if row['api'] == api and row['case_ordinal'] == 1:
                    row['args'][field] += 1
            with self.subTest(api=api), self.assertRaises(ValueError):
                self.audit(f)

    def test_precheck_removal_sentinel_refused(self):
        f = owned_fixture()
        ctx = f.contexts[0]
        ordinal = ctx['completed_call']['ordinal']
        pre = next(row for row in f.rows if row['api'] == 'GetCompletedValue' and
                   row['case_ordinal'] == 1 and row['edge'] == 'after' and row['ordinal'] != ordinal)
        pre['value']['completed_value'] = (1 << 64) - 1
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_auxiliary_raw_queue_readback_bool_or_failed_create_not_admitted(self):
        f = owned_fixture()
        for row in f.rows:
            if row['case_ordinal'] == 1 and row['api'] in ('ExecuteCommandLists', 'Signal'):
                row['args']['queue'] = True
        with self.assertRaises(ValueError):
            self.audit(f)

        f = owned_fixture()
        for row in f.rows:
            if row['api'] == 'CreateCommittedResource' and row['args'].get('role') == 'readback' and row['edge'] == 'after':
                row['value']['raw'] = True
            if row['api'] == 'MapReadback' and row['case_ordinal'] == 1:
                row['args']['resource'] = True
        with self.assertRaises(ValueError):
            self.audit(f)
        f = owned_fixture()
        rb = next(row for row in f.rows if row['api'] == 'CreateCommittedResource' and
                  row['args'].get('role') == 'readback' and row['edge'] == 'after')
        rb['hresult'] = -1
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_future_fence_creation_cannot_establish_prior_signal_or_completion(self):
        f = owned_fixture(future_fence=True)
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_initially_signalled_or_reused_fence_value_does_not_prove_new_work(self):
        f = owned_fixture()
        for row in f.rows:
            if row['api'] == 'CreateFence':
                row['args']['initial_value'] = 999
            if row['api'] == 'GetCompletedValue' and row['case_ordinal'] == 1 and row['edge'] == 'after':
                row['value']['completed_value'] = 999
        f.contexts[0]['producer']['completed_value'] = 999
        with self.assertRaises(ValueError):
            self.audit(f)
        f = owned_fixture()
        for row in f.rows:
            if row['api'] == 'Signal' and row['case_ordinal'] == 1:
                row['args']['fence_value'] = 2
        with self.assertRaises(ValueError):
            self.audit(f, 1)

    def test_wrong_pixel_with_truthful_changed_hash_is_not_accepted(self):
        f = owned_fixture()
        raw = bytearray(raw_pixels(0))
        raw[123] ^= 1
        f.contexts[0]['producer']['raw_sha256'] = hashlib.sha256(raw).hexdigest()
        with self.assertRaises(ValueError):
            a.audit_case(f.wires[0], f.journal(), f.contexts[0], bytes(raw))
        f = owned_fixture()
        f.contexts[0]['producer']['raw_file'] = 'after_bind_B_clamp.bgra'
        with self.assertRaises(ValueError):
            self.audit(f)

    def test_different_original_and_transport_clocks_are_not_equated(self):
        f = owned_fixture()
        # Add transport-only observation marks while preserving each exact
        # original occurrence mapping. This is not runtime normalization.
        def native(x):
            return 2 * x if x else 0
        for row in f.rows:
            row['transition'] = native(row['transition'])
        for w, c in zip(f.wires, f.contexts):
            w['producer']['clock'] = native(w['producer']['clock'])
            c['snapshot_cutoff'] = native(c['snapshot_cutoff'])
            s = w['snapshot']
            s['at_transition'] = native(s['at_transition'])
            for o in [s['device']] + s['resources'] + s['heaps']:
                for key in ('create_entry', 'create_return', 'release_entry', 'release_return'):
                    o[key] = native(o[key])
            for e in s['events']:
                e['entry'], e['return'] = native(e['entry']), native(e['return'])
            for key, value in c.items():
                if key.endswith('_call') and value is not None:
                    value['transport_entry'] = native(value['transport_entry'])
                    value['transport_return'] = native(value['transport_return'])
        self.assertTrue(self.audit(f, 1)['selected_history_known'])

    def test_snapshot_producer_flags_bool_int_lateids_and_stronger_facts_refused(self):
        changes = (
            lambda w: w.update(sequence=True), lambda w: w.update(schema_version=True),
            lambda w: w['producer'].update(epoch=True), lambda w: w['producer'].update(snapshot_available=1),
            lambda w: w['producer'].update(clock=w['producer']['clock'] + 1),
            lambda w: w['producer'].update(dropped=1),
            lambda w: w['producer'].update(faulted=True, gap=True),
            lambda w: w.update(input_facts_attested=True), lambda w: w.update(sampled_resource_GPU_read_proven=True),
        )
        for i, change in enumerate(changes):
            f = owned_fixture()
            change(f.wires[0])
            with self.subTest(index=i), self.assertRaises(ValueError):
                self.audit(f)

    def test_parse_UTF8_only_duplicate_keys_nonfinite_depth_nodes_and_caps(self):
        f = owned_fixture()
        raw = json.dumps(f.wires[0])
        self.assertTrue(a.audit_wire(raw.encode('utf8'))['selected_history_known'])
        for data in (raw.encode('utf16'), raw.encode('utf32'), '{"n":1,"n":2}', '{"n":NaN}',
                     ' ' * 16385, [[[[[[[[[[[[[[[[[[[[[[[[[[0]]]]]]]]]]]]]]]]]]]]]]]]]],
                     [0] * 769, {'x': 'a' * 4097}, {'x': 0.0}):
            with self.subTest(kind=type(data).__name__), self.assertRaises(ValueError):
                a.parse(data)

    def test_returned_reports_are_detached_and_no_IO_is_needed(self):
        f = owned_fixture()
        with patch('builtins.open', side_effect=AssertionError('No file I/O')), \
             patch.object(Path, 'open', side_effect=AssertionError('No path I/O')):
            report = self.audit(f, raw=False)
        saved = json.dumps(report, sort_keys=True)
        f.contexts[0]['command']['raw'] = 999
        f.wires[0]['snapshot']['resources'][0]['raw'] = 998
        self.assertEqual(json.dumps(report, sort_keys=True), saved)


def native_bridge(executable):
    # Explicit opt-in used ONLY by CTest with the NEW CPU-only target. Default
    # unittest discovery never starts a process or requires an installed build.
    executable = Path(executable).resolve(strict=True)
    result = subprocess.run([str(executable), '--emit-json'], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False, timeout=30)
    if result.returncode:
        raise ValueError('New CPU executable failed: ' + result.stderr.decode('utf8', 'replace')[:4096])
    return a.audit_native_samples(result.stdout)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--native-wire':
        audit = native_bridge(sys.argv[2])
        print(json.dumps({'native_wire_bridge_passed': True, 'audit': audit}, sort_keys=True))
        sys.argv = [sys.argv[0]]
    unittest.main()
