import copy
import json
import unittest
from unittest.mock import patch

from tools import original_call_owner_validate as audit


RUN = '0123456789abcdef0123456789abcdef'
BUILD = 'a' * 64
FALSE_KEYS = ('resource_generation_known', 'selected_slot_placement_known',
              'current_contents_at_draw_known', 'sampled_resource_GPU_read_proven',
              'exclusive_phase_attribution_proven')


def sampler_info():
    return dict(sType=31, pNext=None, flags=0, mag_filter=0, min_filter=0, mipmap_mode=0,
                address_modes=[3, 1, 2], mip_lod_bias_bits=0, anisotropy_enable=0,
                max_anisotropy_bits=1065353216, compare_enable=0, compare_op=0,
                min_lod_bits=0, max_lod_bits=0, border_color=4, unnormalized_coordinates=0)


def packet(value):
    return (json.dumps(value, separators=(',', ':')) + '\n').encode()


def pe_receipt():
    activation = dict(schema_version=1, run_id=RUN, IID='39da4e09-bd1c-4198-9fae-86bbe3be41fd',
                      handle_domain='PE_facing_vkd3d_contract', PE_to_Unix_handle_mapping_known=False,
                      query_interface_hresult=0, get_vulkan_handles_hresult=0,
                      outputs=dict(instance=10001, physical_device=10002, device=10003),
                      extensions_count_hresult=0, extensions_values_hresult=0,
                      device_features_hresult=0, enabled_extensions=['VK_EXT_descriptor_buffer'],
                      enabled_feature_chain_types=[1000316002], descriptorBuffer=True,
                      sampler_descriptor_size=4, create_sampler_result=0, sampler_PE=501,
                      activated=True, instance_extensions_count_hresult=0,
                      instance_extensions_values_hresult=0, instance_extension_count=1,
                      instance_extension_capacity=1, enabled_instance_extensions=['VK_EXT_debug_utils'],
                      device_extension_count=1, device_extension_capacity=1)
    events = []
    calls = [('vkCreateSampler', 0, dict(device=10003, create_info=sampler_info(),
                                      allocator_pointer=0), 'VkResult'),
             ('vkSetDebugUtilsObjectNameEXT', 1, dict(device=10003, sType=1000128000, pNext=None,
                object_type=21, object_handle=501, name='D4VR_CALL_OWNER_1:' + RUN + ':1'), 'VkResult'),
             ('vkGetDescriptorEXT', 1, dict(device=10003, sampler=501, descriptor_type=0,
                info_sType=1000316004, info_pnext=None, data_size=4, output_pointer=4096), 'void'),
             ('vkSetDebugUtilsObjectNameEXT', 2, dict(device=10003, sType=1000128000, pNext=None,
                object_type=21, object_handle=501, name='D4VR_CALL_OWNER_1:' + RUN + ':2'), 'VkResult'),
             ('vkGetDescriptorEXT', 2, dict(device=10003, sampler=501, descriptor_type=0,
                info_sType=1000316004, info_pnext=None, data_size=4, output_pointer=8192), 'void'),
             ('vkDestroySampler', 0, dict(device=10003, sampler=501, allocator_pointer=0), 'void')]
    for api, sequence, args, kind in calls:
        for edge in ('before', 'after'):
            row = dict(ordinal=len(events)+1, api=api, edge=edge, run_id=RUN, sequence=sequence,
                       phase=dict(case_ordinal=8, api_phase=sequence),
                       args=copy.deepcopy(args), return_kind=kind, original_returned=edge == 'after',
                       result=0 if edge == 'after' and kind == 'VkResult' else None)
            if edge == 'after' and api == 'vkCreateSampler':
                row['sampler'] = 501
            if edge == 'after' and api == 'vkGetDescriptorEXT':
                row['output_hex'] = '41424344'
            events.append(row)
    return dict(schema_version=1, purpose='owned_original_call_owner', run_id=RUN,
                activation=activation, events=events, scratch_spans_retained_until_after_queries=True)


def rows(slot=0, pid=1234, cookie=5678):
    def common(event):
        return dict(schema_version=3, event=event, run_id=RUN, scope_slot=slot,
                    host_pid=pid, process_start_cookie=cookie, dso_epoch=slot+1,
                    **{k: False for k in FALSE_KEYS})

    def facet():
        return dict(inspected=False, names_complete=False, extension_count=0,
                    name_occurrences=0, enabled_name_present=False, enabled_on_success=False)

    def life(kind, entry):
        row = common('lifecycle')
        row.update(kind=kind, operation_ordinal=entry, return_ordinal=entry+1,
                   original_returned=True, return_kind='void' if kind.startswith('destroy_') else 'VkResult',
                   result=None if kind.startswith('destroy_') else 0, instance_generation=0,
                   device_generation=0, parent_instance_generation=0, raw_instance=0,
                   raw_device=0, raw_physical=0, output_pointer=0, chain_present=False,
                   application=dict(captured=False, api_version=0, name='', name_truncated=False,
                                    engine_name='', engine_name_truncated=False), count_only=False,
                   input_capacity=0, returned_count=0, physical_devices=[], instance_debug_utils=facet())
        return row

    def owner(kind, entry, phase):
        row = common('original_call_owner')
        row.update(kind=kind, operation_ordinal=entry, return_ordinal=entry+1, device=100,
                   device_generation=1, parent_instance_generation=1, device_lifetime_known=True,
                   sampler=501, sampler_generation=1, sampler_lifetime_known=True,
                   info_pointer=0, name_pointer=0, allocator_pointer=0, output_pointer=0,
                   info_present=False, info_sType=0, chain_present=False, original_returned=True,
                   return_kind='void' if kind == 'destroy_sampler' else 'VkResult',
                   result=None if kind == 'destroy_sampler' else 0, create_info_captured=False,
                   create_info=None, object_type=0, name_known=False, name_bytes_including_nul=0,
                   name_hex='', phase_before=dict(status='known', case_ordinal=8, api_phase=phase),
                   phase_after=dict(status='known', case_ordinal=8, api_phase=phase), phase_unchanged=True)
        return row

    def query(entry, sequence, pointer):
        row = common('get_descriptor')
        row.update(operation_ordinal=entry, return_ordinal=entry+1, device_generation=1,
                   parent_instance_generation=1, device_lifetime_known=True, sampler_generation=1,
                   sampler_lifetime_known=True, ordinal=sequence, device=100, descriptor_type=0,
                   info_sType=1000316004, info_present=True, chain_present=False, data_size=4,
                   output_pointer=pointer, original_returned=True, return_kind='void',
                   typed_input=dict(known=True, kind='sampler', reason='none', raw=501,
                                    image_view=0, sampler=501, image_layout=0, address=0, range=0, format=0),
                   phase_before=dict(status='known', case_ordinal=8, api_phase=sequence),
                   phase_after=dict(status='known', case_ordinal=8, api_phase=sequence), phase_unchanged=True,
                   output_captured=True, output_hex='41424344')
        return row

    h = common('observer_header')
    h.update(scope_claimed=True, build_id_known=True, build_id=BUILD, build_id_is_binary_attestation=False,
             scope_slots=8, global_event_limit=256, global_events_per_case_limit=32, lifecycle_limit=256,
             scope='owned_translated_original_get_descriptor', enabled=True, phase_source='file',
             transport_scope='owned_single_loader_chain', route_capacity=32,
             limits=dict(events=32, events_per_case=4, output_bytes=256, phase_file_bytes=256),
             purpose='original_nonce_call_owner', owner_limits=dict(sampler_occurrences=32,
             sampler_creates_per_device=16, name_records=32, names_per_device=8, owner_records=96,
             name_bytes_including_nul=64, instance_extension_names=256, extension_name_bytes=256,
             selected_case_ordinal=8))
    ci = life('create_instance', 1)
    ci.update(instance_generation=1, raw_instance=50, output_pointer=9000,
              instance_debug_utils=dict(inspected=True, names_complete=True, extension_count=1,
                  name_occurrences=1, enabled_name_present=True, enabled_on_success=True))
    enum = life('enumerate_physical_devices', 3)
    enum.update(instance_generation=1, raw_instance=50, output_pointer=9008,
                input_capacity=1, returned_count=1, physical_devices=[40])
    cd = life('create_device', 5)
    cd.update(device_generation=1, parent_instance_generation=1, raw_device=100,
              raw_physical=40, output_pointer=9016)
    dd = life('destroy_device', 19)
    dd.update(device_generation=1, parent_instance_generation=1, raw_device=100)
    di = life('destroy_instance', 21)
    di.update(instance_generation=1, raw_instance=50)
    cs = owner('create_sampler', 7, 0)
    cs.update(info_pointer=9024, output_pointer=9032, info_present=True, info_sType=31,
              create_info_captured=True, create_info=sampler_info())
    n1, n2 = owner('object_name', 9, 1), owner('object_name', 13, 2)
    for sequence, n in ((1, n1), (2, n2)):
        raw = ('D4VR_CALL_OWNER_1:' + RUN + ':' + str(sequence)).encode() + b'\0'
        n.update(info_pointer=9100+sequence, name_pointer=9200+sequence, info_present=True,
                 info_sType=1000128000, object_type=21, name_known=True,
                 name_bytes_including_nul=len(raw), name_hex=raw.hex())
    ds = owner('destroy_sampler', 17, 0)
    q1, q2 = query(11, 1, 4096), query(15, 2, 8192)
    f = common('observer_footer')
    f.update(lifecycle_calls_seen=5, lifecycle_retained=5, lifecycle_dropped=0, transition_count=22,
             owner_calls_seen=4, owner_original_returns=4, owner_records_retained=4,
             owner_records_dropped=0, sampler_occurrences=1, name_records=2, pending_owner_records=0,
             active_sampler_occurrences=0, owner_ignored_bootstrap=0, owner_ignored_unselected=0,
             owner_unknown_phase=0, owner_gap=False, calls_seen=2, original_returns=2, events_retained=2,
             dropped_events=0, skipped_bootstrap_queries=0, unknown_phase_queries=0, phase_unavailable=0,
             phase_changed=0, output_unavailable=0, typed_input_unknown=0, active_instances=0,
             active_devices=0, faulted=False, transport_overflow=False, transport_chain_mismatch=False,
             coverage_complete=True, calls_by_case=[0]*8+[2], returns_by_case=[0]*8+[2],
             events_by_case=[0]*8+[2], dropped_by_case=[0]*9)
    return [h, ci, enum, cd, dd, di, cs, n1, n2, ds, q1, q2, f]


def stream(values):
    return b''.join(packet(row) for row in values)


def claim(slot=0, pid=1234, cookie=5678):
    return f'D4VR_QUERY_SCOPE_2 {RUN} {slot} {pid} {cookie} {slot+1}\n'.encode()


def result(values=None, pe=None, **kwargs):
    return audit.analyze(kwargs.get('journals', {0: stream(rows() if values is None else values)}),
                         kwargs.get('claims', {0: claim()}), packet(pe_receipt() if pe is None else pe),
                         kwargs.get('run_id', RUN), kwargs.get('build_id', BUILD),
                         overflow=kwargs.get('overflow', False))


class OriginalCallOwnerTests(unittest.TestCase):
    def setUp(self):
        for name in ('run', 'Popen', 'call', 'check_call', 'check_output'):
            blocker = patch('subprocess.' + name, side_effect=AssertionError('Subprocess forbidden'))
            blocker.start()
            self.addCleanup(blocker.stop)

    def test_positive_original_nonce_relation_only(self):
        report = result()
        self.assertTrue(report['original_call_owner_known'], report)
        self.assertEqual(len(report['joins']), 2)
        self.assertTrue(all(report[key] is False for key in audit.FALSE_FACTS))
        self.assertNotEqual(report['joins'][0]['PE_device'], report['joins'][0]['Unix_device'])
        self.assertFalse(report['PE_device_equality_used'])

    def test_nonnull_function_or_device_extension_does_not_enable_instance(self):
        for mutate in ('absent', 'wrong_list', 'failed_hresult', 'changed_count', 'shrunk_count', 'duplicate'):
            pe = pe_receipt()
            a = pe['activation']
            if mutate == 'absent':
                a['enabled_instance_extensions'] = ['VK_KHR_surface']
            elif mutate == 'wrong_list':
                a['enabled_extensions'].append('VK_EXT_debug_utils')
                a['device_extension_count'] = a['device_extension_capacity'] = 2
                a['enabled_instance_extensions'] = ['VK_KHR_surface']
            elif mutate == 'failed_hresult':
                a['instance_extensions_values_hresult'] = -1
            elif mutate == 'changed_count':
                a['instance_extension_count'] = 2
            elif mutate == 'shrunk_count':
                a['instance_extension_capacity'] = 2
            else:
                a['enabled_instance_extensions'] *= 2
                a['instance_extension_count'] = a['instance_extension_capacity'] = 2
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                result(pe=pe)

    def test_PE_marker_nonce_and_header_exact(self):
        for field, bad in (('object_type', 0), ('object_handle', 502), ('sType', 31),
                           ('pNext', 1), ('name', 'D4VR_CALL_OWNER_1:'+'f'*32+':1')):
            pe = pe_receipt()
            for edge in pe['events'][2:4]:
                edge['args'][field] = bad
            with self.subTest(field=field), self.assertRaises(ValueError):
                result(pe=pe)

    def test_original_BEFORE_stays_frozen_and_void_result_not_fabricated(self):
        pe = pe_receipt()
        pe['events'][3]['args']['name'] += 'x'
        with self.assertRaises(ValueError):
            result(pe=pe)
        for index in (4, 5, 8, 9, 10, 11):
            pe = pe_receipt()
            pe['events'][index]['result'] = 0
            with self.assertRaises(ValueError):
                result(pe=pe)

    def test_PE_types_counts_and_edges_are_strict(self):
        for mutate in ('bool_schema', 'bool_query_type', 'bool_name_seq', 'extra_edge', 'bad_clock'):
            pe = pe_receipt()
            if mutate == 'bool_schema':
                pe['schema_version'] = True
            elif mutate == 'bool_query_type':
                for edge in pe['events'][4:6]:
                    edge['args']['descriptor_type'] = False
            elif mutate == 'bool_name_seq':
                pe['events'][2]['sequence'] = True
            elif mutate == 'extra_edge':
                pe['events'].append(copy.deepcopy(pe['events'][0]))
            else:
                pe['events'][1]['ordinal'] = 1
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                result(pe=pe)

    def test_original_null_allocators_and_full_creation_words(self):
        for index in (0, 10):
            pe = pe_receipt()
            for edge in pe['events'][index:index+2]:
                edge['args']['allocator_pointer'] = 1
            with self.subTest(index=index), self.assertRaises(ValueError):
                result(pe=pe)
        for key in ('flags', 'mag_filter', 'max_anisotropy_bits', 'unnormalized_coordinates'):
            values = rows()
            create = next(r for r in values if r.get('kind') == 'create_sampler')
            create['create_info'][key] += 1
            self.assertFalse(result(values)['original_call_owner_known'])
        values = rows()
        next(r for r in values if r.get('kind') == 'create_sampler')['sampler_lifetime_known'] = False
        self.assertFalse(result(values)['original_call_owner_known'])

    def test_native_marker_values_not_info_pointer_identity(self):
        values = rows()
        for row in values:
            if row['event'] == 'original_call_owner' and row['kind'] == 'object_name':
                row['info_pointer'] += 1000000
                row['name_pointer'] += 2000000
        self.assertTrue(result(values)['original_call_owner_known'])
        for key, bad in (('object_type', 10), ('sampler', 502), ('result', -1)):
            values = rows()
            marker = next(r for r in values if r.get('kind') == 'object_name')
            marker[key] = bad
            self.assertFalse(result(values)['original_call_owner_known'])

    def test_native_pointer_presence_is_consistent_without_cross_domain_equality(self):
        for kind, field in (('create_sampler', 'info_pointer'), ('create_sampler', 'output_pointer'),
                            ('object_name', 'info_pointer'), ('object_name', 'name_pointer')):
            values = rows()
            next(r for r in values if r.get('kind') == kind)[field] = 0
            with self.subTest(kind=kind, field=field):
                self.assertFalse(result(values)['original_call_owner_known'])

    def test_native_name_wrong_or_ambiguous_never_joins(self):
        values = rows()
        marker = next(r for r in values if r.get('kind') == 'object_name')
        marker['name_hex'] = marker['name_hex'].replace('30', '31', 1)
        self.assertFalse(result(values)['original_call_owner_known'])
        self.assertFalse(result(journals={0: stream(rows()), 1: stream(rows(1, 999, 333))},
                                claims={0: claim(), 1: claim(1, 999, 333)})['original_call_owner_known'])

    def test_native_exact_query_arguments_and_bytes_required(self):
        for field, bad in (('output_pointer', 4097), ('data_size', 3), ('output_hex', '01020304')):
            values = rows()
            q = next(r for r in values if r['event'] == 'get_descriptor')
            q[field] = bad
            self.assertFalse(result(values)['original_call_owner_known'])

    def test_missing_enabled_parent_and_unreturned_names_unknown(self):
        values = rows()
        ci = next(r for r in values if r.get('kind') == 'create_instance')
        ci['instance_debug_utils'].update(name_occurrences=0, enabled_name_present=False,
                                           enabled_on_success=False)
        self.assertFalse(result(values)['original_call_owner_known'])
        values = rows()
        values[1]['instance_debug_utils']['extension_count'] = 2
        # Wine's original Unix instance extension list is its own domain.
        # The enabled debug_utils name is checked there; list counts are not
        # treated as a PE-to-Unix extension-list conversion witness.
        self.assertTrue(result(values)['original_call_owner_known'])
        values = rows()
        marker = next(r for r in values if r.get('kind') == 'object_name')
        marker.update(original_returned=False, return_ordinal=0, result=None,
                      phase_after=dict(status='missing', case_ordinal=0, api_phase=0), phase_unchanged=False)
        self.assertFalse(result(values)['original_call_owner_known'])

    def test_physical_parent_and_generations_are_original_not_dispatch_key(self):
        for mutate in ('missing_enum', 'physical', 'device_generation', 'sampler_generation'):
            values = rows()
            if mutate == 'missing_enum':
                values = [r for r in values if r.get('kind') != 'enumerate_physical_devices']
            elif mutate == 'physical':
                next(r for r in values if r.get('kind') == 'create_device')['raw_physical'] = 41
            else:
                next(r for r in values if r.get('kind') == 'object_name')[mutate] += 1
            self.assertFalse(result(values)['original_call_owner_known'])

    def test_spans_distinct_and_retained(self):
        pe = pe_receipt()
        for edge in pe['events'][8:10]:
            edge['args']['output_pointer'] = 4098
        with self.assertRaises(ValueError):
            result(pe=pe)
        pe = pe_receipt()
        pe['scratch_spans_retained_until_after_queries'] = False
        with self.assertRaises(ValueError):
            result(pe=pe)

    def test_unrelated_unclosed_instance_does_not_become_query_absence(self):
        values = rows()
        extra = copy.deepcopy(values[1])
        extra.update(instance_generation=2, raw_instance=51, operation_ordinal=23, return_ordinal=24)
        values.insert(6, extra)
        values[-1].update(lifecycle_calls_seen=6, lifecycle_retained=6, transition_count=24,
                          active_instances=1, coverage_complete=False)
        report = result(values)
        self.assertFalse(report['original_call_owner_known'], report)
        self.assertEqual(len(report['joins']), 2)
        self.assertFalse(report['all_saved_scopes_complete'])
        self.assertFalse(report['global_query_absence_proven'])

    def test_completed_reuse_and_failed_generations_do_not_retag_old_lifetimes(self):
        for failed in (False, True):
            values = rows()
            extra = copy.deepcopy(next(r for r in values if r.get('kind') == 'create_sampler'))
            shift = 2 if failed else 4
            for row in values[1:-1]:
                if row['operation_ordinal'] >= 7:
                    row['operation_ordinal'] += shift
                    row['return_ordinal'] += shift
                if row['event'] in ('original_call_owner', 'get_descriptor'):
                    row['sampler_generation'] = 2
            owners = [r for r in values if r['event'] == 'original_call_owner']
            if failed:
                extra.update(result=-1, sampler=0, sampler_lifetime_known=False)
                extras = [extra]
            else:
                destroy = copy.deepcopy(next(r for r in owners if r['kind'] == 'destroy_sampler'))
                destroy.update(operation_ordinal=9, return_ordinal=10, sampler_generation=1)
                extras = [extra, destroy]
            first_owner = next(i for i, r in enumerate(values) if r['event'] == 'original_call_owner')
            values[first_owner:first_owner] = extras
            values[-1].update(owner_calls_seen=4+len(extras), owner_original_returns=4+len(extras),
                              owner_records_retained=4+len(extras), sampler_occurrences=2,
                              transition_count=22+shift)
            with self.subTest(failed=failed):
                report = result(values)
                self.assertTrue(report['original_call_owner_known'], report)
                self.assertEqual({j['Unix_sampler_generation'] for j in report['joins']}, {2})
                old = copy.deepcopy(values)
                next(r for r in old if r.get('kind') == 'object_name')['sampler_generation'] = 1
                self.assertFalse(result(old)['original_call_owner_known'])

    def test_declared_quotas_counters_and_typed_boolean_facets_are_not_trusted(self):
        for kind, field, bad in (('header', 'owner_records', 97),
                                  ('footer', 'name_records', 1),
                                  ('footer', 'owner_original_returns', 5),
                                  ('footer', 'owner_calls_seen', True),
                                  ('footer', 'active_sampler_occurrences', 1)):
            values = rows()
            if kind == 'header':
                values[0]['owner_limits'][field] = bad
            else:
                values[-1][field] = bad
            with self.subTest(field=field):
                self.assertFalse(result(values)['original_call_owner_known'])
        for field in ('enabled_name_present', 'enabled_on_success'):
            values = rows()
            values[1]['instance_debug_utils'][field] = 1
            self.assertFalse(result(values)['original_call_owner_known'])

    def test_unaccounted_query_calls_cannot_hide_in_the_shared_clock(self):
        values = rows()
        # Retain every real row unchanged, but invent one additional call/return
        # and two holes in the clock with no bootstrap/phase/quota explanation.
        values[-1].update(calls_seen=3, original_returns=3, transition_count=24)
        report = result(values)
        self.assertFalse(report['original_call_owner_known'])
        self.assertTrue(report['scope_errors'])
        values = rows()
        values[-1]['calls_by_case'][8] = 3
        values[-1]['returns_by_case'][8] = 3
        values[-1].update(calls_seen=3, original_returns=3, transition_count=24)
        self.assertFalse(result(values)['original_call_owner_known'])

    def test_explicit_bootstrap_counters_do_not_consume_selected_storage(self):
        values = rows()
        for row in values[1:-1]:
            if row['operation_ordinal'] >= 7:
                row['operation_ordinal'] += 1000
                row['return_ordinal'] += 1000
        f = values[-1]
        f.update(owner_calls_seen=204, owner_original_returns=204, owner_ignored_bootstrap=200,
                 calls_seen=302, original_returns=302, skipped_bootstrap_queries=300,
                 transition_count=1022)
        f['calls_by_case'][0] = f['returns_by_case'][0] = 300
        # Selected query ordinals count ALL original GetDescriptor calls.
        for row in values:
            if row['event'] == 'get_descriptor':
                row['ordinal'] += 300
        report = result(values)
        self.assertTrue(report['original_call_owner_known'], report)
        self.assertEqual(f['events_retained'], 2)
        self.assertEqual(f['owner_records_retained'], 4)
        self.assertTrue(all(report[k] is False for k in audit.FALSE_FACTS))

    def test_raw_json_duplicates_nonfinite_and_payload_bounds_refused(self):
        pe_data = packet(pe_receipt())
        with self.assertRaises(ValueError):
            audit.audit_PE(pe_data.replace(b'"schema_version":1',
                                          b'"schema_version":1,"schema_version":1', 1), RUN)
        with self.assertRaises(ValueError):
            audit.audit_PE(pe_data.replace(b'"sampler_descriptor_size":4',
                                          b'"sampler_descriptor_size":NaN'), RUN)
        with self.assertRaises(ValueError):
            audit.audit_PE(b' ' * (audit.MAX_PE_BYTES + 1) + b'\n', RUN)
        self.assertFalse(result(journals={0: b' ' * (audit.base.MAX_BYTES + 1) + b'\n'})[
            'original_call_owner_known'])

    def test_PE_and_Unix_phase_values_are_independently_joined(self):
        pe = pe_receipt()
        pe['events'][2]['phase']['api_phase'] = 0
        with self.assertRaises(ValueError):
            result(pe=pe)
        values = rows()
        marker = next(r for r in values if r.get('kind') == 'object_name')
        marker['phase_after']['api_phase'] = 0
        marker['phase_unchanged'] = False
        self.assertFalse(result(values)['original_call_owner_known'])

    def test_quota_gap_unknown_build_fork_and_old_schema_not_promoted(self):
        self.assertFalse(result(overflow=True)['original_call_owner_known'])
        for mutate in ('owner_gap', 'wrong_PID', 'old_schema', 'unknown_build'):
            values = rows()
            if mutate == 'owner_gap':
                values[-1].update(owner_gap=True, coverage_complete=False)
            elif mutate == 'wrong_PID':
                values[6]['host_pid'] = 1235
            elif mutate == 'old_schema':
                for row in values:
                    row['schema_version'] = 2
            else:
                values[0].update(build_id_known=False, build_id='unknown')
            self.assertFalse(result(values)['original_call_owner_known'])

    def test_clock_overlap_destroy_and_return_reversal_refused(self):
        for mutate in ('duplicate', 'destroy_early', 'marker_after_query'):
            values = rows()
            marker = next(r for r in values if r.get('kind') == 'object_name')
            if mutate == 'duplicate':
                marker['return_ordinal'] = marker['operation_ordinal']
            elif mutate == 'destroy_early':
                destroy = next(r for r in values if r.get('kind') == 'destroy_sampler')
                destroy.update(operation_ordinal=10, return_ordinal=11)
            else:
                marker.update(operation_ordinal=11, return_ordinal=12)
            self.assertFalse(result(values)['original_call_owner_known'])

    def test_opaque_equal_bytes_are_not_owner_or_resource_identity(self):
        values = rows()
        q = [r for r in values if r['event'] == 'get_descriptor']
        self.assertEqual(q[0]['output_hex'], q[1]['output_hex'])
        q[1]['sampler_generation'] = 2
        report = result(values)
        self.assertFalse(report['original_call_owner_known'])
        self.assertTrue(all(report[k] is False for k in audit.FALSE_FACTS))

    def test_each_returned_encoding_is_joined_without_canonical_byte_assumption(self):
        values, pe = rows(), pe_receipt()
        second = [r for r in values if r['event'] == 'get_descriptor'][1]
        second['output_hex'] = pe['events'][9]['output_hex'] = '05060708'
        report = result(values, pe=pe)
        self.assertTrue(report['original_call_owner_known'], report)
        swapped = copy.deepcopy(values)
        for q in (r for r in swapped if r['event'] == 'get_descriptor'):
            q['output_hex'] = '05060708' if q['ordinal'] == 1 else '41424344'
        self.assertFalse(result(swapped, pe=pe)['original_call_owner_known'])


if __name__ == '__main__':
    unittest.main()
