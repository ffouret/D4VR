"""Pure finite original-name/query owner audit; never a module backing policy.

Schema 3 is parsed in its own transition clock. No schema-2 projection, process
inventory, filesystem access, runtime execution or loaded-memory read occurs.
The frozen scope module supplies only unchanged scalar/query/claim validators.
External source/module pins and the reviewed x64 thunk remain caller duties.
"""
from __future__ import annotations

import json
import re

try:
    from . import query_scope_validate as base
except ImportError:
    import query_scope_validate as base


MAX_PE_BYTES = 256 * 1024
FALSE_FACTS = ('input_facts_attested', 'module_backing_identity_known',
               'loaded_memory_bytes_attested', 'render_device_owner_known',
               'selected_slot_placement_known', 'current_contents_at_draw_known',
               'sampled_resource_GPU_read_proven', 'global_query_absence_proven',
               'translated_API_query_absence_proven', 'exclusive_phase_attribution_proven',
               'PE_to_Unix_handle_mapping_known', 'general_PE_to_Unix_handle_conversion_proven',
               'application_to_layer_physical_mapping_known',
               'application_to_layer_create_output_slot_mapping_known',
               'old_process_gate_repaired', 'game_sampled_resource_identified', 'coverage_repaired')
OWNER_LIMITS = dict(sampler_occurrences=32, sampler_creates_per_device=16,
                    name_records=32, names_per_device=8, owner_records=96,
                    name_bytes_including_nul=64, instance_extension_names=256,
                    extension_name_bytes=256, selected_case_ordinal=8)
SAMPLER_INFO = dict(sType=31, pNext=None, flags=0, mag_filter=0, min_filter=0,
                    mipmap_mode=0, address_modes=[3, 1, 2], mip_lod_bias_bits=0,
                    anisotropy_enable=0, max_anisotropy_bits=1065353216,
                    compare_enable=0, compare_op=0, min_lod_bits=0, max_lod_bits=0,
                    border_color=4, unnormalized_coordinates=0)
OWNER_FIELDS = ('kind operation_ordinal return_ordinal device device_generation '
                'parent_instance_generation device_lifetime_known sampler sampler_generation '
                'sampler_lifetime_known info_pointer name_pointer allocator_pointer output_pointer '
                'info_present info_sType chain_present original_returned return_kind result '
                'create_info_captured create_info object_type name_known '
                'name_bytes_including_nul name_hex phase_before phase_after phase_unchanged')
OWNER_COUNTERS = ('owner_calls_seen owner_original_returns owner_records_retained '
                  'owner_records_dropped sampler_occurrences name_records pending_owner_records '
                  'active_sampler_occurrences owner_ignored_bootstrap owner_ignored_unselected '
                  'owner_unknown_phase')
HEADER_FIELDS = ('scope_claimed build_id_known build_id build_id_is_binary_attestation '
                 'scope_slots global_event_limit global_events_per_case_limit lifecycle_limit '
                 'scope enabled phase_source transport_scope route_capacity limits purpose owner_limits')
FOOTER_FIELDS = (base.COUNTERS + ' faulted transport_overflow transport_chain_mismatch '
                 'coverage_complete calls_by_case returns_by_case events_by_case dropped_by_case '
                 + OWNER_COUNTERS + ' owner_gap')

require = base.require
exact = base.exact
obj = base.obj
uint = base.uint


def positive(value, label, maximum=base.MAX_U64):
    uint(value, label, maximum)
    require(value > 0, 'Positive: ' + label)


def vkresult(value):
    require(type(value) is int and -(1 << 31) <= value < (1 << 31), 'Exact signed VkResult')


def original_name(run_id, sequence):
    base.nonce(run_id)
    require(type(sequence) is int and sequence in (1, 2), 'Name sequence')
    name = 'D4VR_CALL_OWNER_1:' + run_id + ':' + str(sequence)
    require(len(name.encode()) + 1 <= 64, 'Owned finite name')
    return name


def _extensions(names, count, capacity, required):
    positive(count, 'Original extension count', 256)
    positive(capacity, 'Original extension capacity', 256)
    require(type(names) is list and len(names) == count == capacity,
            'Stable count-then-values extension count/list')
    require(all(type(n) is str and re.fullmatch(r'VK_[A-Za-z0-9_]+', n)
                and len(n.encode()) < 256 for n in names)
            and len(set(names)) == len(names), 'Bounded unique extension names')
    require(required in names, 'Actual ENABLED extension required: ' + required)


def audit_PE(data, run_id):
    """Validate actual original PE edges and activation; no Unix-handle equality."""
    base.nonce(run_id)
    require(type(data) is bytes and len(data) <= MAX_PE_BYTES and data.endswith(b'\n'),
            'Bounded complete PE receipt')
    pe = base.loads(data)
    obj(pe, 'schema_version purpose run_id activation events '
        'scratch_spans_retained_until_after_queries', 'PE original receipt')
    exact(pe['schema_version'], 1, 'PE schema')
    exact(pe['purpose'], 'owned_original_call_owner', 'New PE purpose')
    exact(pe['run_id'], run_id, 'Pinned PE nonce')
    exact(pe['scratch_spans_retained_until_after_queries'], True, 'Original private spans retained')
    a = pe['activation']
    obj(a, 'schema_version run_id IID handle_domain PE_to_Unix_handle_mapping_known '
        'query_interface_hresult get_vulkan_handles_hresult outputs extensions_count_hresult '
        'extensions_values_hresult device_features_hresult enabled_extensions '
        'enabled_feature_chain_types descriptorBuffer sampler_descriptor_size '
        'create_sampler_result sampler_PE activated instance_extensions_count_hresult '
        'instance_extensions_values_hresult instance_extension_count instance_extension_capacity '
        'enabled_instance_extensions device_extension_count device_extension_capacity',
        'Original enabled activation')
    exact(a['schema_version'], 1, 'Activation schema')
    exact(a['run_id'], run_id, 'Activation nonce')
    exact(a['IID'], '39da4e09-bd1c-4198-9fae-86bbe3be41fd', 'Reviewed typed interface')
    exact(a['handle_domain'], 'PE_facing_vkd3d_contract', 'Separate PE domain')
    exact(a['PE_to_Unix_handle_mapping_known'], False, 'Getter is no generic bridge')
    for key in ('activated', 'descriptorBuffer'):
        exact(a[key], True, 'Enabled ' + key)
    for key in ('query_interface_hresult', 'get_vulkan_handles_hresult',
                'extensions_count_hresult', 'extensions_values_hresult', 'device_features_hresult',
                'instance_extensions_count_hresult', 'instance_extensions_values_hresult'):
        require(type(a[key]) is int and 0 <= a[key] < (1 << 31), 'Successful original HRESULT ' + key)
    _extensions(a['enabled_instance_extensions'], a['instance_extension_count'],
                a['instance_extension_capacity'], 'VK_EXT_debug_utils')
    _extensions(a['enabled_extensions'], a['device_extension_count'],
                a['device_extension_capacity'], 'VK_EXT_descriptor_buffer')
    chain = a['enabled_feature_chain_types']
    require(type(chain) is list and 1 <= len(chain) <= 128 and
            all(type(x) is int and 0 <= x <= 0xffffffff for x in chain)
            and chain.count(1000316002) == 1, 'Original enabled descriptor-buffer feature chain')
    obj(a['outputs'], 'instance physical_device device', 'Actual PE handles')
    for key, value in a['outputs'].items():
        positive(value, 'PE ' + key)
    exact(a['create_sampler_result'], 0, 'Actual sampler creation success')
    positive(a['sampler_PE'], 'Owned sampler')
    positive(a['sampler_descriptor_size'], 'Actual legal footprint', 256)
    events = pe['events']
    require(type(events) is list and len(events) == 12, 'Six complete original calls')
    device, sampler, size = a['outputs']['device'], a['sampler_PE'], a['sampler_descriptor_size']
    calls = [('vkCreateSampler', 0, 'VkResult'),
             ('vkSetDebugUtilsObjectNameEXT', 1, 'VkResult'), ('vkGetDescriptorEXT', 1, 'void'),
             ('vkSetDebugUtilsObjectNameEXT', 2, 'VkResult'), ('vkGetDescriptorEXT', 2, 'void'),
             ('vkDestroySampler', 0, 'void')]
    originals = []
    for index, (api, sequence, signature) in enumerate(calls):
        before, after = events[2*index:2*index+2]
        for edge, row, ordinal in (('before', before, 2*index+1), ('after', after, 2*index+2)):
            fields = 'ordinal api edge run_id sequence phase args return_kind original_returned result'
            if edge == 'after' and api == 'vkCreateSampler':
                fields += ' sampler'
            if edge == 'after' and api == 'vkGetDescriptorEXT':
                fields += ' output_hex'
            obj(row, fields, 'Original PE edge')
            for key, value in dict(ordinal=ordinal, api=api, edge=edge, run_id=run_id,
                                    sequence=sequence, return_kind=signature,
                                    original_returned=edge == 'after').items():
                exact(row[key], value, 'Original PE ' + key)
            exact(row['phase'], dict(case_ordinal=8, api_phase=sequence), 'Actual original PE phase')
            if edge == 'before' or signature == 'void':
                exact(row['result'], None, 'No fabricated original result')
            else:
                vkresult(row['result'])
                exact(row['result'], 0, 'Original call success')
        exact(before['args'], after['args'], 'Frozen original BEFORE operands')
        args = before['args']
        if api == 'vkCreateSampler':
            exact(args, dict(device=device, create_info=SAMPLER_INFO, allocator_pointer=0),
                  'Owned creation arguments')
            exact(after['sampler'], sampler, 'Original creation output')
        elif api == 'vkDestroySampler':
            exact(args, dict(device=device, sampler=sampler, allocator_pointer=0),
                  'Original destroy arguments')
        elif api == 'vkSetDebugUtilsObjectNameEXT':
            exact(args, dict(device=device, sType=1000128000, pNext=None, object_type=21,
                             object_handle=sampler, name=original_name(run_id, sequence)),
                  'Original typed name marker')
        else:
            obj(args, 'device sampler descriptor_type info_sType info_pnext data_size '
                'output_pointer', 'Original sampler query')
            for key, value in dict(device=device, sampler=sampler, descriptor_type=0,
                                    info_sType=1000316004, info_pnext=None, data_size=size).items():
                exact(args[key], value, 'Original query ' + key)
            positive(args['output_pointer'], 'Private output pointer')
            require(args['output_pointer'] <= base.MAX_U64 - size, 'Nonoverflowing private output span')
            require(type(after['output_hex']) is str and
                    re.fullmatch('[0-9a-f]{%d}' % (size*2), after['output_hex']),
                    'Exact original returned bytes')
        originals.append(dict(api=api, sequence=sequence, args=args,
                              entry=before['ordinal'], returned=after['ordinal'], after=after))
    spans = [(x['args']['output_pointer'], x['args']['output_pointer'] + size)
             for x in originals if x['api'] == 'vkGetDescriptorEXT']
    require(spans[0][1] <= spans[1][0] or spans[1][1] <= spans[0][0], 'Distinct retained private spans')
    return dict(activation=a, calls=originals, original_PE_edges=12, original_PE_queries=2,
                original_PE_name_markers=2, **{key: False for key in FALSE_FACTS})


def _phase_pair(row):
    base.phase(row['phase_before'])
    base.phase(row['phase_after'])
    before, after = row['phase_before'], row['phase_after']
    same = before['status'] == after['status'] == 'known' and before == after
    exact(row['phase_unchanged'], same, 'Actual observed phase equality')
    if not row['original_returned']:
        exact(after, dict(status='missing', case_ordinal=0, api_phase=0), 'No pending AFTER phase')


def _facet(value, row):
    obj(value, 'inspected names_complete extension_count name_occurrences '
        'enabled_name_present enabled_on_success', 'Original instance extension facet')
    for key in ('inspected', 'names_complete', 'enabled_name_present', 'enabled_on_success'):
        base.boolean(value[key], key)
    uint(value['extension_count'], 'Original extension count', 0xffffffff)
    uint(value['name_occurrences'], 'Debug extension occurrences', 256)
    require(value['name_occurrences'] <= value['extension_count'], 'Occurrence/count relation')
    if value['names_complete']:
        require(value['inspected'] and value['extension_count'] <= 256, 'Complete bounded inspection')
    if value['names_complete']:
        require(value['name_occurrences'] <= 1, 'No duplicate enabled name in complete facet')
    exact(value['enabled_name_present'], value['names_complete'] and
          value['name_occurrences'] == 1, 'Actual complete name presence')
    enabled = (value['inspected'] and value['names_complete'] and value['enabled_name_present']
               and row['original_returned'] and row['result'] == 0)
    exact(value['enabled_on_success'], enabled, 'Exact ENABLED-after-success relation')
    if row['kind'] != 'create_instance':
        exact(value, dict(inspected=False, names_complete=False, extension_count=0,
                          name_occurrences=0, enabled_name_present=False, enabled_on_success=False),
              'Ignored non-instance extension facet')


def _edge(row):
    positive(row['operation_ordinal'], 'Original entry transition')
    base.boolean(row['original_returned'], 'Original returned')
    if row['original_returned']:
        positive(row['return_ordinal'], 'Original return transition')
        require(row['return_ordinal'] > row['operation_ordinal'], 'Original return follows entry')
    else:
        exact(row['return_ordinal'], 0, 'No pending return')
        exact(row['result'], None, 'No pending result')


def _owner_payload(row):
    obj(row, base.COMMON + ' ' + OWNER_FIELDS, 'Original owner row')
    require(row['kind'] in ('create_sampler', 'destroy_sampler', 'object_name'), 'Owner kind')
    _edge(row)
    for key in ('device', 'device_generation', 'parent_instance_generation', 'sampler',
                'sampler_generation', 'info_pointer', 'name_pointer', 'allocator_pointer', 'output_pointer'):
        uint(row[key], key)
    for key in ('info_sType', 'object_type'):
        uint(row[key], key, 0xffffffff)
    for key in ('device_lifetime_known', 'sampler_lifetime_known', 'info_present',
                'chain_present', 'create_info_captured', 'name_known', 'phase_unchanged'):
        base.boolean(row[key], key)
    exact(row['info_present'], row['info_pointer'] != 0, 'Original owner input pointer presence')
    exact(row['return_kind'], 'void' if row['kind'] == 'destroy_sampler' else 'VkResult',
          'Original owner return kind')
    if row['original_returned']:
        if row['return_kind'] == 'void':
            exact(row['result'], None, 'Original void owner result')
        else:
            vkresult(row['result'])
    if row['device_lifetime_known']:
        for key in ('device', 'device_generation', 'parent_instance_generation'):
            positive(row[key], key)
    else:
        exact(row['device_generation'], 0, 'Unknown device generation')
        exact(row['parent_instance_generation'], 0, 'Unknown parent generation')
    if row['sampler_lifetime_known']:
        require(row['device_lifetime_known'], 'Sampler needs known original device')
        positive(row['sampler_generation'], 'Sampler occurrence')
        positive(row['sampler'], 'Original sampler')
    if row['create_info_captured']:
        require(row['kind'] == 'create_sampler' and row['info_present'] and
                row['info_sType'] == 31 and not row['chain_present'], 'Captured legal sampler header')
        ci = row['create_info']
        obj(ci, ' '.join(SAMPLER_INFO), 'Captured typed sampler creation')
        exact(ci['sType'], 31, 'Original sampler sType')
        exact(ci['pNext'], None, 'Original sampler chain')
        for key in set(ci) - {'sType', 'pNext', 'address_modes'}:
            uint(ci[key], 'Sampler word ' + key, 0xffffffff)
        require(type(ci['address_modes']) is list and len(ci['address_modes']) == 3,
                'Three original sampler address modes')
        for x in ci['address_modes']:
            uint(x, 'Original address mode', 0xffffffff)
    else:
        exact(row['create_info'], None, 'No unknown sampler payload')
    uint(row['name_bytes_including_nul'], 'Name byte quota', 64)
    if row['name_known']:
        require(row['kind'] == 'object_name' and row['info_present'] and
                row['info_sType'] == 1000128000 and not row['chain_present'], 'Known name header')
        n = row['name_bytes_including_nul']
        positive(row['name_pointer'], 'Original complete name pointer')
        require(n > 0 and type(row['name_hex']) is str and
                re.fullmatch('[0-9a-f]{%d}' % (n*2), row['name_hex']), 'Complete original name bytes')
        raw = bytes.fromhex(row['name_hex'])
        require(raw.endswith(b'\0') and b'\0' not in raw[:-1], 'Exact name terminator')
        try:
            raw[:-1].decode('utf-8')
        except UnicodeError as error:
            raise ValueError('Original name UTF8') from error
    else:
        exact(row['name_hex'], '', 'No unknown name bytes')
        exact(row['name_bytes_including_nul'], 0, 'No unknown name length')
    if row['kind'] != 'object_name':
        exact(row['name_known'], False, 'Ignored non-name payload')
        exact(row['object_type'], 0, 'Ignored non-name object type')
    _phase_pair(row)
    require(row['phase_before']['status'] == 'known' and
            row['phase_before']['case_ordinal'] == 8, 'Only selected owner rows retained')


def _life_payload(row):
    obj(row, base.COMMON + ' ' + base.LIFE + ' instance_debug_utils', 'Original lifecycle3')
    kind = row['kind']
    require(kind in ('create_instance', 'create_device', 'destroy_instance', 'destroy_device',
                     'enumerate_physical_devices'), 'Original lifecycle kind')
    _edge(row)
    for key in ('instance_generation', 'device_generation', 'parent_instance_generation',
                'raw_instance', 'raw_device', 'raw_physical', 'output_pointer'):
        uint(row[key], key)
    for key in ('input_capacity', 'returned_count'):
        uint(row[key], key, 0xffffffff)
    for key in ('chain_present', 'count_only'):
        base.boolean(row[key], key)
    is_void = kind.startswith('destroy_')
    exact(row['return_kind'], 'void' if is_void else 'VkResult', 'Original lifecycle signature')
    if row['original_returned']:
        if is_void:
            exact(row['result'], None, 'Original void destroy')
        else:
            vkresult(row['result'])
    app = row['application']
    obj(app, 'captured api_version name name_truncated engine_name engine_name_truncated', 'App')
    base.boolean(app['captured'], 'App captured')
    uint(app['api_version'], 'App API version', 0xffffffff)
    for key in ('name', 'engine_name'):
        require(type(app[key]) is str and len(app[key].encode()) <= 64 and '\0' not in app[key],
                'Bounded original app prefix')
        base.boolean(app[key+'_truncated'], 'Prefix truncation')
        if app[key+'_truncated']:
            exact(len(app[key].encode()), 64, 'Full captured prefix')
    if kind != 'create_instance' or not app['captured']:
        exact(app, dict(captured=False, api_version=0, name='', name_truncated=False,
                        engine_name='', engine_name_truncated=False), 'Ignored/unknown app')
    _facet(row['instance_debug_utils'], row)
    physicals = row['physical_devices']
    require(type(physicals) is list and len(physicals) <= 32, 'Physical array bound')
    for value in physicals:
        positive(value, 'Original physical handle')
    require(len(set(physicals)) == len(physicals), 'Unique returned physical handles')
    if kind != 'enumerate_physical_devices':
        exact(row['count_only'], False, 'Unused count flag')
        exact(row['input_capacity'], 0, 'Unused input capacity')
        exact(row['returned_count'], 0, 'Unused returned count')
        exact(physicals, [], 'Unused physical array')
    elif not row['original_returned'] or row['result'] not in (0, 5):
        exact(row['returned_count'], 0, 'No failed enumeration outputs')
        exact(physicals, [], 'No failed enumeration handles')
    elif row['count_only']:
        exact(row['input_capacity'], 0, 'Original count-only capacity')
        exact(row['output_pointer'], 0, 'Count-only no values pointer')
        exact(physicals, [], 'No count-only handle values')
    else:
        positive(row['output_pointer'], 'Returned physical array pointer')
        exact(len(physicals), min(row['input_capacity'], row['returned_count'], 32),
              'Bounded actual physical array')
    zeros = dict(create_instance=('device_generation', 'parent_instance_generation', 'raw_device', 'raw_physical'),
                 destroy_instance=('device_generation', 'parent_instance_generation', 'raw_device', 'raw_physical', 'output_pointer'),
                 create_device=('instance_generation', 'raw_instance'),
                 destroy_device=('instance_generation', 'raw_instance', 'raw_physical', 'output_pointer'),
                 enumerate_physical_devices=('device_generation', 'parent_instance_generation', 'raw_device', 'raw_physical'))
    for key in zeros[kind]:
        exact(row[key], 0, 'Unused lifecycle field ' + key)
    if not kind.startswith('create_'):
        exact(row['chain_present'], False, 'Ignored lifecycle chain')
    if kind in ('create_instance', 'create_device'):
        entity = 'instance' if kind == 'create_instance' else 'device'
        positive(row[entity+'_generation'], 'Reserved original generation')
        if row['original_returned'] and row['result'] == 0:
            positive(row['raw_'+entity], 'Actual creation output')
            positive(row['output_pointer'], 'Actual creation output pointer')
        else:
            exact(row['raw_'+entity], 0, 'No pending/failed creation handle')
    if kind == 'create_device':
        positive(row['raw_physical'], 'Original physical input')


def _replay(lives, owners, queries, footer):
    """Reconstruct the schema3 clock directly, including new original calls."""
    transitions = {}
    for category, rows in (('life', lives), ('owner', owners), ('query', queries)):
        for row in rows:
            for edge, number in (('entry', row['operation_ordinal']),
                                 ('return', row['return_ordinal'])):
                if edge == 'return' and not row['original_returned']:
                    continue
                require(0 < number <= footer['transition_count'] and number not in transitions,
                        'Unique shared original transition')
                transitions[number] = (category, edge, row)
    instances, devices, samplers = {}, {}, {}
    live_i, live_d, live_s = {}, {}, {}
    pending_i, pending_d, pending_s = set(), set(), set()
    pending_calls = {}
    physicals = {}
    last_gen = dict(instance=0, device=0, sampler=0)
    gap = False

    def device_at(row):
        if not row.get('device_lifetime_known', False):
            return None
        g, p = row['device_generation'], row['parent_instance_generation']
        require(g in live_d and g not in pending_d and p in live_i and p not in pending_i,
                'Original device and parent live at entry')
        d = live_d[g]
        exact(row['device'], d['raw_device'], 'Exact Unix device generation')
        exact(p, d['parent_instance_generation'], 'Exact Unix parent generation')
        return g

    def sampler_at(row):
        if not row['sampler_lifetime_known']:
            return None
        g = row['sampler_generation']
        require(g in live_s and g not in pending_s, 'Original owned sampler alive')
        s = live_s[g]
        raw = (row['typed_input']['sampler'] if row['event'] == 'get_descriptor' else row['sampler'])
        exact(raw, s['sampler'], 'Original sampler handle generation')
        exact(row['device_generation'], s['device_generation'], 'Original sampler device')
        exact(row['parent_instance_generation'], s['parent_instance_generation'], 'Sampler parent')
        exact(row['device'], s['device'], 'Original sampler device handle')
        return g

    for number, (category, edge, row) in sorted(transitions.items()):
        kind = row.get('kind')
        if category == 'life':
            if edge == 'entry':
                if kind in ('create_instance', 'create_device'):
                    entity = 'instance' if kind == 'create_instance' else 'device'
                    exact(row[entity+'_generation'], last_gen[entity]+1, 'New creation generation')
                    last_gen[entity] += 1
                    if entity == 'device':
                        p = row['parent_instance_generation']
                        if not p:
                            gap = True
                        else:
                            require(p in live_i and p not in pending_i and
                                    (p, row['raw_physical']) in physicals and
                                    physicals[p, row['raw_physical']] < number,
                                    'Exact prior original enumeration parent')
                elif kind in ('destroy_instance', 'enumerate_physical_devices'):
                    g = row['instance_generation']
                    if not g:
                        gap = True
                    else:
                        require(g in live_i and g not in pending_i and
                                live_i[g]['raw_instance'] == row['raw_instance'], 'Original instance live')
                        if kind == 'destroy_instance':
                            require(not any(d['parent_instance_generation'] == g for d in live_d.values()),
                                    'Device teardown before parent destroy')
                            pending_i.add(g)
                else:
                    g = row['device_generation']
                    if not g:
                        gap = True
                    else:
                        require(g in live_d and g not in pending_d and
                                live_d[g]['raw_device'] == row['raw_device'] and
                                live_d[g]['parent_instance_generation'] == row['parent_instance_generation'],
                                'Original exact device destroy')
                        require(not any(s['device_generation'] == g for s in live_s.values()),
                                'Owned samplers destroyed before target device')
                        require(not any(x[0] == g for x in pending_calls.values()),
                                'No target destroy during pending original call')
                        pending_d.add(g)
            elif kind == 'create_instance' and row['result'] == 0:
                g = row['instance_generation']
                require(all(x['raw_instance'] != row['raw_instance'] for x in live_i.values()),
                        'No concurrent raw instance reuse')
                live_i[g] = row
                instances[g] = dict(row, create_return=number, destroy_entry=0, destroy_return=0)
            elif kind == 'create_device' and row['result'] == 0:
                g, p = row['device_generation'], row['parent_instance_generation']
                if p:
                    require(p in live_i and p not in pending_i, 'Parent live at original device return')
                require(all(x['raw_device'] != row['raw_device'] for x in live_d.values()),
                        'No concurrent raw device reuse')
                live_d[g] = row
                devices[g] = dict(row, create_return=number, destroy_entry=0, destroy_return=0)
            elif kind == 'destroy_instance' and row['instance_generation']:
                g = row['instance_generation']
                require(g in pending_i, 'Original instance destroy token')
                instances[g].update(destroy_entry=row['operation_ordinal'], destroy_return=number)
                del live_i[g]
                pending_i.remove(g)
                physicals = {k: v for k, v in physicals.items() if k[0] != g}
            elif kind == 'destroy_device' and row['device_generation']:
                g = row['device_generation']
                require(g in pending_d, 'Original device destroy token')
                devices[g].update(destroy_entry=row['operation_ordinal'], destroy_return=number)
                del live_d[g]
                pending_d.remove(g)
            elif kind == 'enumerate_physical_devices' and row['instance_generation']:
                p = row['instance_generation']
                require(p in live_i and p not in pending_i, 'Original enumeration return parent')
                for raw in row['physical_devices']:
                    require(not any(other == raw and parent != p for parent, other in physicals),
                            'No ambiguous physical parent')
                    physicals[p, raw] = number
                if row['returned_count'] > row['input_capacity'] or row['returned_count'] > 32:
                    if not row['count_only']:
                        gap = True
            continue
        if edge == 'entry':
            d = device_at(row)
            if d is None:
                gap = True
            if category == 'owner' and kind == 'create_sampler':
                exact(row['sampler_generation'], last_gen['sampler']+1, 'New sampler occurrence')
                last_gen['sampler'] += 1
                s = None
            else:
                uses_sampler = category == 'owner' or row['descriptor_type'] == 0
                s = sampler_at(row) if uses_sampler else None
                if uses_sampler and s is None:
                    gap = True
                if category == 'owner' and kind == 'destroy_sampler' and s is not None:
                    require(not any(x[1] == s for x in pending_calls.values()),
                            'No sampler destruction during pending call')
                    pending_s.add(s)
            pending_calls[row['operation_ordinal']] = (d, s)
        else:
            bound = pending_calls.pop(row['operation_ordinal'], None)
            require(bound is not None, 'Original owner/query return token')
            d, s = bound
            if d is not None:
                require(d in live_d and d not in pending_d, 'Original device survives exact call')
            if category == 'owner' and kind == 'create_sampler':
                if row['result'] == 0:
                    positive(row['sampler'], 'Successful original sampler output')
                    positive(row['output_pointer'], 'Original sampler output slot')
                    g = row['sampler_generation']
                    require(not any(x['sampler'] == row['sampler'] and
                                    x['device_generation'] == row['device_generation']
                                    for x in live_s.values()), 'No concurrent sampler handle reuse')
                    live_s[g] = row
                    samplers[g] = dict(row, create_return=number, destroy_entry=0, destroy_return=0)
                else:
                    exact(row['sampler'], 0, 'Failed creation publishes no sampler')
                    exact(row['sampler_lifetime_known'], False, 'Failed sampler is unknown')
            elif category == 'owner' and kind == 'destroy_sampler' and s is not None:
                require(s in pending_s, 'Original sampler destroy token')
                samplers[s].update(destroy_entry=row['operation_ordinal'], destroy_return=number)
                pending_s.remove(s)
                del live_s[s]
    if not footer['lifecycle_dropped']:
        expected_transitions = (len(lives) + sum(r['original_returned'] for r in lives)
                                + footer['owner_calls_seen'] + footer['owner_original_returns']
                                + footer['calls_seen'] + footer['original_returns'])
        exact(footer['transition_count'], expected_transitions, 'All schema3 original transitions')
        if not any(footer[k] for k in ('faulted', 'transport_overflow', 'transport_chain_mismatch')):
            exact(footer['active_instances'], len(live_i), 'Actual active instance count')
            exact(footer['active_devices'], len(live_d), 'Actual active device count')
    if not footer['owner_records_dropped']:
        exact(footer['active_sampler_occurrences'], len(live_s), 'Actual active sampler count')
    return dict(instances=instances, devices=devices, samplers=samplers, gap=gap,
                pending_lifecycle=sum(not r['original_returned'] for r in lives),
                pending_calls=len(pending_calls))


def audit_journal(data, expected):
    """Dedicated schema3 journal/clock parser; never rewrites the input schema."""
    require(type(data) is bytes and 0 < len(data) <= base.MAX_BYTES and data.endswith(b'\n'),
            'Bounded complete schema3 journal')
    lines = data.splitlines()
    require(2 <= len(lines) <= 386 and all(lines), 'Schema3 row quota')
    rows = [base.loads(line) for line in lines]
    h, f = rows[0], rows[-1]
    obj(h, base.COMMON + ' ' + HEADER_FIELDS, 'Schema3 header')
    obj(f, base.COMMON + ' ' + FOOTER_FIELDS, 'Schema3 footer')
    base.identity(expected)
    for row in rows:
        exact(row['schema_version'], 3, 'Only explicit new schema3')
        base.false_facets(row)
        for key in base.IDENTITY:
            exact(row[key], expected[key], 'Pinned original scope ' + key)
    exact(h['event'], 'observer_header', 'Header kind')
    exact(f['event'], 'observer_footer', 'Footer kind')
    for key in ('scope_claimed', 'build_id_known', 'enabled'):
        base.boolean(h[key], key)
    exact(h['scope_claimed'], True, 'Persisted claim')
    exact(h['build_id_is_binary_attestation'], False, 'Build label is not loaded bytes')
    exact(h['build_id'], expected['build_id'], 'Pinned source build declaration')
    if h['build_id_known']:
        require(type(h['build_id']) is str and re.fullmatch('[0-9a-f]{16,64}', h['build_id']),
                'Finite declared build ID')
    else:
        exact(h['build_id'], 'unknown', 'Explicit unknown build')
    for key, value in dict(scope_slots=8, global_event_limit=256,
                            global_events_per_case_limit=32, lifecycle_limit=256,
                            purpose='original_nonce_call_owner', phase_source='file',
                            scope='owned_translated_original_get_descriptor',
                            transport_scope='owned_single_loader_chain', route_capacity=32).items():
        exact(h[key], value, 'Fixed schema3 protocol ' + key)
    exact(h['limits'], dict(events=32, events_per_case=4, output_bytes=256, phase_file_bytes=256),
          'Unchanged query storage bounds')
    exact(h['owner_limits'], OWNER_LIMITS, 'Separate non-evicting owner bounds')
    for key in (base.COUNTERS + ' ' + OWNER_COUNTERS).split():
        uint(f[key], 'Counter ' + key)
    for key in ('faulted', 'transport_overflow', 'transport_chain_mismatch',
                'coverage_complete', 'owner_gap'):
        base.boolean(f[key], key)
    for key in ('calls_by_case', 'returns_by_case', 'events_by_case', 'dropped_by_case'):
        require(type(f[key]) is list and len(f[key]) == 9, 'Nine original case counters')
        for value in f[key]:
            uint(value, key)
    lives, owners, queries = [], [], []
    section = 0
    for row in rows[1:-1]:
        if row['event'] == 'lifecycle':
            require(section == 0, 'Lifecycle section first')
            _life_payload(row)
            lives.append(row)
        elif row['event'] == 'original_call_owner':
            require(section <= 1, 'Owner section before query section')
            section = 1
            _owner_payload(row)
            owners.append(row)
        else:
            exact(row['event'], 'get_descriptor', 'Original query event')
            section = 2
            obj(row, base.COMMON + ' ' + base.QUERY +
                ' sampler_generation sampler_lifetime_known', 'Original query3 fields')
            base.query_payload(row)
            uint(row['sampler_generation'], 'Query sampler occurrence')
            base.boolean(row['sampler_lifetime_known'], 'Query sampler known')
            if row['sampler_lifetime_known']:
                require(row['device_lifetime_known'] and row['descriptor_type'] == 0
                        and row['typed_input']['known'], 'Known sampler query operand')
                positive(row['sampler_generation'], 'Query sampler generation')
            else:
                exact(row['sampler_generation'], 0, 'No unknown sampler generation')
            queries.append(row)
    require(len(lives) <= 256 and len(owners) <= 96 and len(queries) <= 32, 'Finite rows')
    for group in (lives, owners, queries):
        require(all(a['operation_ordinal'] < b['operation_ordinal'] for a, b in zip(group, group[1:])),
                'Entry-order serialization')
    exact(f['lifecycle_retained'], len(lives), 'Life retention')
    exact(f['lifecycle_calls_seen'], len(lives) + f['lifecycle_dropped'], 'Life quota')
    exact(f['owner_records_retained'], len(owners), 'Owner retention')
    exact(f['owner_calls_seen'], len(owners) + f['owner_records_dropped'] +
          f['owner_ignored_bootstrap'] + f['owner_ignored_unselected'] + f['owner_unknown_phase'],
          'Selected owner quota and ignored original calls')
    require(sum(r['original_returned'] for r in owners) <= f['owner_original_returns']
            <= f['owner_calls_seen'], 'Exact original owner returns')
    exact(f['pending_owner_records'], sum(not r['original_returned'] for r in owners),
          'Pending original owner rows')
    creates = [r for r in owners if r['kind'] == 'create_sampler']
    names = [r for r in owners if r['kind'] == 'object_name']
    exact(f['sampler_occurrences'], len(creates), 'Retained reserved sampler occurrences')
    exact(f['name_records'], len(names), 'Retained names')
    require(len(creates) <= 32 and len(names) <= 32, 'Owner kind quotas')
    for group, cap in ((creates, 16), (names, 8)):
        counts = {}
        for row in group:
            key = row['device'], row['device_generation']
            counts[key] = counts.get(key, 0) + 1
        require(all(n <= cap for n in counts.values()), 'Original per-device quota')
    if f['owner_records_dropped']:
        exact(f['owner_gap'], True, 'Dropped owner rows are explicit gap')
    hist = [0] * 9
    changed = unavailable = unknown = output_missing = 0
    last_ordinal = 0
    for q in queries:
        require(last_ordinal < q['ordinal'] <= f['calls_seen'], 'Query occurrence ordinal')
        last_ordinal = q['ordinal']
        case = q['phase_before']['case_ordinal']
        hist[case] += 1
        if q['original_returned']:
            unknown += not q['typed_input']['known']
            output_missing += not q['output_captured']
            if not q['phase_unchanged']:
                changed += q['phase_after']['status'] == 'known'
                unavailable += q['phase_after']['status'] != 'known'
    exact(f['events_retained'], len(queries), 'Query retention')
    exact(f['events_by_case'], hist, 'Actual retained case histogram')
    exact(hist[0], 0, 'Bootstrap no retained query')
    exact(f['skipped_bootstrap_queries'], f['calls_by_case'][0], 'Bootstrap count')
    exact(f['dropped_by_case'][0], 0, 'Bootstrap no payload budget')
    exact(f['dropped_events'], sum(f['dropped_by_case']), 'Query drop total')
    require(sum(f['calls_by_case']) + f['unknown_phase_queries'] <= f['calls_seen']
            and sum(f['returns_by_case']) <= f['original_returns'] <= f['calls_seen'],
            'Original query call/return totals')
    if not f['faulted']:
        exact(sum(f['calls_by_case']) + f['unknown_phase_queries'], f['calls_seen'],
              'Every unfaulted original query entry classified')
    exact(f['output_unavailable'], output_missing, 'Returned uncaptured spans')
    exact(f['typed_input_unknown'], unknown, 'Returned unknown typed input')
    require(changed <= f['phase_changed'] <= f['original_returns'] and
            unavailable <= f['phase_unavailable'] <= f['original_returns'], 'Original phase uncertainty')
    for case in range(9):
        require(hist[case] <= 4 and f['returns_by_case'][case] <= f['calls_by_case'][case],
                'Finite query per-case counts')
        if case:
            require(hist[case] + f['dropped_by_case'][case] <= f['calls_by_case'][case], 'Case quota')
            if not f['faulted']:
                exact(hist[case] + f['dropped_by_case'][case], f['calls_by_case'][case],
                      'No unaccounted unfaulted selected query entry')
    state = _replay(lives, owners, queries, f)
    query_lifetimes_closed = all(q['device_lifetime_known'] and q['original_returned'] and
                                q['device_generation'] in state['devices'] and
                                state['devices'][q['device_generation']]['destroy_entry'] >
                                q['return_ordinal'] for q in queries)
    complete = (h['enabled'] and not any(f[k] for k in
                ('faulted', 'transport_overflow', 'transport_chain_mismatch', 'owner_gap',
                 'owner_records_dropped', 'pending_owner_records', 'active_sampler_occurrences',
                 'owner_unknown_phase',
                 'dropped_events', 'unknown_phase_queries', 'phase_unavailable', 'phase_changed',
                 'active_instances', 'active_devices', 'lifecycle_dropped'))
                and f['owner_calls_seen'] == f['owner_original_returns']
                and all(row['phase_unchanged'] for row in owners)
                and f['calls_seen'] == f['original_returns']
                and not state['pending_lifecycle'] and query_lifetimes_closed)
    if complete:
        exact(sum(f['returns_by_case']), f['original_returns'], 'Complete classified original returns')
    exact(f['coverage_complete'], complete, 'Declared finite schema3 coverage')
    return dict(identity={key: h[key] for key in base.IDENTITY}, header=h, footer=f,
                lifecycle=lives, owners=owners, queries=queries, state=state,
                finite_saved_scope_complete=complete and h['build_id_known'] and not state['gap'])


def analyze(journals, claims, PE_data, run_id, build_id, *, overflow=False):
    """Join a new original-call purpose; never consult old process gate reports.

    The marker's explicit typed nonce identifies the call boundary. Disk/module
    pins, PE process backing and rendering ownership are separate facts and are
    never inferred from this relation or from coincident dispatchable handles.
    """
    base.nonce(run_id)
    base.boolean(overflow, 'Persistent scope overflow')
    require(type(build_id) is str and (build_id == 'unknown' or
            re.fullmatch('[0-9a-f]{16,64}', build_id)), 'External source build pin')
    require(type(journals) is dict and type(claims) is dict and
            set(journals) == set(claims) and 1 <= len(claims) <= 8, 'Exact bounded claim/journal set')
    original = audit_PE(PE_data, run_id)
    scopes, errors = [], []
    for slot in sorted(claims):
        uint(slot, 'Persistent slot', 7)
        expected = base.parse_claim(claims[slot], run_id, slot)
        expected['build_id'] = build_id
        try:
            scopes.append(audit_journal(journals[slot], expected))
        except (ValueError, KeyError, TypeError) as error:
            errors.append(dict(scope_slot=slot, reason=str(error)))
    require(sum(len(s['queries']) for s in scopes) <= 256, 'Unchanged global query quota')
    for case in range(1, 9):
        require(sum(s['footer']['events_by_case'][case] for s in scopes) <= 32,
                'Unchanged global per-case query quota')
    reasons = []
    if overflow:
        reasons.append('persistent_scope_overflow')
    if errors:
        reasons.append('scope3_journal_invalid_or_missing_original_closure')
    for s in scopes:
        f, h = s['footer'], s['header']
        if not s['finite_saved_scope_complete']:
            reasons.append('saved_scope_not_completely_closed_' + str(s['identity']['scope_slot']))
        if (not h['enabled'] or not h['build_id_known'] or
                any(f[k] for k in ('faulted', 'transport_overflow', 'transport_chain_mismatch',
                                   'owner_gap', 'owner_records_dropped', 'pending_owner_records',
                                   'lifecycle_dropped', 'owner_unknown_phase'))):
            reasons.append('owner_recording_incomplete_scope_' + str(s['identity']['scope_slot']))
    names = [x for x in original['calls'] if x['api'] == 'vkSetDebugUtilsObjectNameEXT']
    queries = [x for x in original['calls'] if x['api'] == 'vkGetDescriptorEXT']
    joins, missing = [], []
    for name, query in zip(names, queries):
        sequence = name['sequence']
        name_hex = (name['args']['name'].encode('utf-8') + b'\0').hex()
        expected_phase = dict(status='known', case_ordinal=8, api_phase=sequence)
        marker_candidates, query_candidates = [], []
        for s in scopes:
            for row in s['owners']:
                if (row['kind'] == 'object_name' and row['name_known'] and
                        row['name_hex'] == name_hex and row['object_type'] == 21 and
                        row['sampler'] == name['args']['object_handle'] and row['info_present'] and
                        row['info_sType'] == 1000128000 and not row['chain_present'] and
                        row['original_returned'] and row['result'] == name['after']['result'] and
                        row['device_lifetime_known'] and row['sampler_lifetime_known'] and
                        row['phase_before'] == row['phase_after'] == expected_phase and
                        row['phase_unchanged']):
                    marker_candidates.append((s, row))
            for row in s['queries']:
                args = query['args']
                if (row['descriptor_type'] == 0 and row['info_sType'] == args['info_sType'] and
                        row['info_present'] and not row['chain_present'] and
                        row['typed_input']['known'] and row['typed_input']['kind'] == 'sampler' and
                        row['typed_input']['sampler'] == args['sampler'] and
                        row['data_size'] == args['data_size'] and
                        row['output_pointer'] == args['output_pointer'] and
                        row['original_returned'] and row['output_captured'] and
                        row['output_hex'] == query['after']['output_hex'] and
                        row['device_lifetime_known'] and row['sampler_lifetime_known'] and
                        row['phase_before'] == row['phase_after'] == expected_phase and
                        row['phase_unchanged']):
                    query_candidates.append((s, row))
        if len(marker_candidates) != 1 or len(query_candidates) != 1:
            missing.append(dict(sequence=sequence, reason='original_marker_or_query_absent_or_ambiguous',
                                marker_matches=len(marker_candidates), query_matches=len(query_candidates)))
            continue
        ns, marker = marker_candidates[0]
        qs, q = query_candidates[0]
        keys = ('device', 'device_generation', 'parent_instance_generation', 'sampler_generation')
        if (ns['identity'] != qs['identity'] or any(marker[k] != q[k] for k in keys)
                or marker['return_ordinal'] >= q['operation_ordinal']):
            missing.append(dict(sequence=sequence, reason='original_marker_query_owner_or_order_mismatch'))
            continue
        state = ns['state']
        d = state['devices'].get(marker['device_generation'])
        i = state['instances'].get(marker['parent_instance_generation'])
        sampler = state['samplers'].get(marker['sampler_generation'])
        if (not d or not i or not sampler or not i['instance_debug_utils']['enabled_on_success']
                or not sampler['device_lifetime_known'] or not sampler['sampler_lifetime_known']
                or not sampler['create_info_captured'] or
                sampler['create_info'] != SAMPLER_INFO or sampler['allocator_pointer'] != 0 or
                sampler['sampler'] != original['activation']['sampler_PE'] or
                sampler['create_return'] >= marker['operation_ordinal'] or
                sampler['destroy_entry'] <= q['return_ordinal'] or not sampler['destroy_return'] or
                d['destroy_entry'] <= sampler['destroy_return'] or not d['destroy_return'] or
                i['destroy_entry'] <= d['destroy_return'] or not i['destroy_return']):
            missing.append(dict(sequence=sequence, reason='enabled_instance_or_closed_original_sampler_device_lifetime_missing'))
            continue
        joins.append(dict(sequence=sequence, **ns['identity'],
                          Unix_device=marker['device'], Unix_device_generation=marker['device_generation'],
                          Unix_parent_instance_generation=marker['parent_instance_generation'],
                          Unix_sampler_generation=marker['sampler_generation'],
                          sampler=marker['sampler'], PE_device=name['args']['device'],
                          marker_entry=marker['operation_ordinal'], marker_return=marker['return_ordinal'],
                          query_entry=q['operation_ordinal'], query_return=q['return_ordinal']))
    same = len(joins) == 2 and all(joins[0][k] == joins[1][k] for k in
                (*base.IDENTITY, 'Unix_device', 'Unix_device_generation',
                 'Unix_parent_instance_generation', 'Unix_sampler_generation', 'sampler', 'PE_device'))
    if same:
        same = joins[0]['query_return'] < joins[1]['marker_entry']
    if not same:
        reasons.append('two_original_pairs_not_same_closed_device_sampler_in_order')
    if missing:
        reasons.append('original_call_pair_unjoined')
    return dict(schema_version=1, purpose='original_nonce_call_owner_audit', run_id=run_id,
                original_call_owner_known=not reasons, original_PE_edges=12,
                all_saved_scopes_complete=not errors and all(
                    s['finite_saved_scope_complete'] for s in scopes),
                original_name_markers=2, original_sampler_queries=2, joins=joins,
                unjoined=missing, reasons=reasons, scope_errors=errors,
                scopes=scopes, source_contract_is_loaded_binary_attestation=False,
                Windows_PID_used_as_host_identity=False, PE_device_equality_used=False,
                **{key: False for key in FALSE_FACTS})
