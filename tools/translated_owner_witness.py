"""Finite positive PE query witnesses at a reviewed Wine thunk boundary.

Pure supplied-fact model. It reads no address/process and runs no application.
The original scope auditor validates Unix callback lifetimes. The two owned PE
calls identify their callback by the unchanged typed operands and private output
spans, never by equality of PE and Unix dispatchable handles. Even a positive
result is neither a general handle conversion nor an absence/GPU-access proof.
"""
import json

import query_scope_validate as scopes
import translated_scope_gate as gates


MAX_WITNESS_BYTES = 16384
FALSE_FACETS = (
    'input_facts_attested', 'global_query_absence_proven',
    'translated_API_query_absence_proven', 'current_contents_at_draw_known',
    'sampled_resource_GPU_read_proven', 'selected_slot_placement_known',
    'exclusive_phase_attribution_proven',
    'application_to_layer_physical_mapping_known',
    'application_to_layer_create_output_slot_mapping_known',
    'general_PE_to_Unix_handle_conversion_proven',
    'game_sampled_resource_identified', 'coverage_repaired',
)


def require(value, message):
    if not value:
        raise ValueError(message)


def integer(value, name, maximum=(1 << 64) - 1):
    require(type(value) is int and 0 < value <= maximum, name)
    return value


def exact_object(value, keys, message):
    require(type(value) is dict and set(value) == set(keys.split()), message)


def process_identity(reports, run_id):
    """Two independently supplied bounded gate reports; never WinPID==PID."""
    require(type(reports) is list and len(reports) == 2, 'Two process gate reports')
    require(len(json.dumps(reports, allow_nan=False).encode('utf-8')) <= 131072,
            'Bounded supplied process reports')
    selected = None
    previous_claims = {}
    for sequence, report in enumerate(reports, 1):
        gates.validate_report(report, run_id, sequence)
        require(type(report) is dict and type(report.get('schema_version')) is int
                and report.get('schema_version') == 1,
                'Process report schema')
        require(report.get('purpose') == 'owned_claimed_process_correlation' and
                report.get('run_id') == run_id and
                type(report.get('gate_sequence')) is int and
                report['gate_sequence'] == sequence, 'Exact process report gate')
        for key in ('input_facts_attested', 'Windows_PID_used_for_host_identity',
                    'Unix_device_owner_known', 'PE_to_Unix_handle_mapping_known',
                    'global_query_absence_proven'):
            require(report.get(key) is False, 'Unsupported process claim: ' + key)
        require(report.get('process_correlation_known') is True,
                'Process correlation remains unknown')
        current = report.get('selected_process')
        exact_object(current, 'host_pid process_start_cookie', 'Process tuple')
        integer(current['host_pid'], 'Host PID', (1 << 32) - 1)
        integer(current['process_start_cookie'], 'Start cookie')
        if selected is None:
            selected = current.copy()
        require(current == selected, 'Process changed between gates')
        claims = report.get('claim_identities')
        require(type(claims) is list and 1 <= len(claims) <= 8,
                'Bounded correlated claim identities')
        seen = set()
        current_claims = {}
        for claim in claims:
            exact_object(claim, 'scope_slot host_pid process_start_cookie dso_epoch',
                         'Claim tuple')
            slot = claim['scope_slot']
            require(type(slot) is int and 0 <= slot < 8 and slot not in seen,
                    'Unique persistent claim slot')
            seen.add(slot)
            integer(claim['host_pid'], 'Claim PID', (1 << 32) - 1)
            integer(claim['process_start_cookie'], 'Claim cookie')
            require(claim['dso_epoch'] == slot + 1 and
                    type(claim['dso_epoch']) is int, 'Claim epoch')
            current_claims[slot] = claim
        require(any(all(c[k] == selected[k] for k in selected)
                    for c in claims), 'Selected process has an original claim')
        require(all(current_claims.get(slot) == claim
                    for slot, claim in previous_claims.items()),
                'Persistent claims cannot disappear or change between gates')
        previous_claims = current_claims
    return selected


def validate_witness(value, run_id):
    exact_object(value, 'schema_version purpose run_id windows_pid pe_device '
                 'pe_instance pe_physical queries', 'Closed owned witness schema')
    require(value['schema_version'] == 1 and type(value['schema_version']) is int,
            'Witness schema version')
    require(value['purpose'] == 'owned_PE_original_query_witnesses' and
            value['run_id'] == run_id, 'Witness purpose/nonce')
    scopes.nonce(run_id)
    require(len(json.dumps(value, allow_nan=False).encode('utf-8')) <= MAX_WITNESS_BYTES,
            'Bounded owned witness payload')
    for key in ('windows_pid', 'pe_device', 'pe_instance', 'pe_physical'):
        integer(value[key], 'Positive PE declaration ' + key)
    integer(value['windows_pid'], 'Windows PID', (1 << 32) - 1)
    require(type(value['queries']) is list and len(value['queries']) == 2,
            'Two owned bracketing calls')
    spans = []
    for sequence, query in enumerate(value['queries'], 1):
        exact_object(query, 'sequence phase return_kind original_returned args '
                     'output_hex scratch_span_retained_until_after_queries',
                     'Original query witness fields')
        require(type(query['sequence']) is int and query['sequence'] == sequence,
                'Owned query sequence')
        require(query['phase'] == dict(case_ordinal=8, api_phase=sequence) and
                all(type(v) is int for v in query['phase'].values()),
                'Reserved original phase pair')
        require(query['return_kind'] == 'void' and query['original_returned'] is True,
                'Actual void return required')
        require(query['scratch_span_retained_until_after_queries'] is True,
                'Private spans retained without reuse')
        args = query['args']
        exact_object(args, 'device sampler data_size output_pointer descriptor_type '
                     'info_sType info_pnext', 'Original typed arguments')
        require(args['device'] == value['pe_device'] and
                type(args['device']) is int, 'Original call on declared PE device')
        integer(args['sampler'], 'Original sampler')
        size = integer(args['data_size'], 'Legal finite descriptor footprint', 256)
        pointer = integer(args['output_pointer'], 'Original private output pointer')
        require(pointer <= (1 << 64) - size, 'No overflowing pointer span')
        require(args['descriptor_type'] == 0 and type(args['descriptor_type']) is int
                and args['info_sType'] == 1000316004 and
                type(args['info_sType']) is int and args['info_pnext'] is None,
                'Exact sampler info header without extension')
        output = query['output_hex']
        require(type(output) is str and len(output) == size * 2 and
                all(c in '0123456789abcdef' for c in output), 'Full returned span')
        spans.append((pointer, pointer + size))
    require(spans[0][1] <= spans[1][0] or spans[1][1] <= spans[0][0],
            'Original private output spans must not overlap or be recycled')
    return value


def audit_owned_inputs(witness, ready_packets, activation_bytes, query_events_bytes):
    """Join the actual PE ready/activation and original edges before Unix joins.

    File/module/source pins remain the caller's independent responsibility. This
    does not turn reports into loaded-code or process-memory attestations.
    """
    run_id = witness['run_id']
    validate_witness(witness, run_id)
    require(type(ready_packets) is list and len(ready_packets) == 2,
            'Two original ready packets')
    outputs = dict(instance=witness['pe_instance'], physical_device=witness['pe_physical'],
                   device=witness['pe_device'])
    first = None
    for sequence, packet in enumerate(ready_packets, 1):
        ready = gates.parse_ready(packet, run_id, sequence)
        require(ready['windows_pid'] == witness['windows_pid'],
                'Original Windows PID within its own domain')
        if first is not None:
            require(all(ready[k] == first[k] for k in
                        ('windows_pid', 'device_interface', 'modules', 'interop')),
                    'Original ready declarations stable at both gates')
        first = ready
        interop = ready['interop']
        require(interop['requested'] is True, 'Original interop requested')
        scopes.exact(interop['outputs'], outputs, 'Actual ready GetVulkanHandles outputs')
        for key in ('query_interface_hresult', 'get_vulkan_handles_hresult'):
            require(type(interop[key]) is int and 0 <= interop[key] <= 0x7fffffff,
                    'Successful original interop HRESULT')
    activation = gates.loads(activation_bytes, MAX_WITNESS_BYTES)
    exact_object(activation, 'schema_version run_id IID handle_domain '
                 'PE_to_Unix_handle_mapping_known query_interface_hresult '
                 'get_vulkan_handles_hresult outputs extensions_count_hresult '
                 'extensions_values_hresult device_features_hresult enabled_extensions '
                 'enabled_feature_chain_types descriptorBuffer sampler_descriptor_size '
                 'create_sampler_result sampler_PE activated', 'Closed activation receipt')
    require(type(activation['schema_version']) is int and activation['schema_version'] == 1
            and activation['run_id'] == run_id and
            activation['IID'] == '39da4e09-bd1c-4198-9fae-86bbe3be41fd' and
            activation['handle_domain'] == 'PE_facing_vkd3d_contract' and
            activation['PE_to_Unix_handle_mapping_known'] is False and
            activation['activated'] is True and activation['descriptorBuffer'] is True,
            'Explicit successful owned interop activation')
    scopes.exact(activation['outputs'], outputs, 'Activation outputs match actual ready')
    for key in ('query_interface_hresult', 'get_vulkan_handles_hresult',
                'extensions_count_hresult', 'extensions_values_hresult', 'device_features_hresult'):
        require(type(activation[key]) is int and 0 <= activation[key] <= 0x7fffffff,
                'Actual successful activation HRESULT ' + key)
    require(all(activation[k] == first['interop'][k] for k in
                ('query_interface_hresult', 'get_vulkan_handles_hresult')),
            'Interop returns match ready')
    extensions = activation['enabled_extensions']
    require(type(extensions) is list and 1 <= len(extensions) <= 256 and
            all(type(x) is str and 0 < len(x.encode()) < 256 for x in extensions) and
            len(set(extensions)) == len(extensions) and 'VK_EXT_descriptor_buffer' in extensions,
            'Enabled descriptor-buffer extension required')
    chain = activation['enabled_feature_chain_types']
    require(type(chain) is list and 1 <= len(chain) <= 128 and
            all(type(t) is int and 0 <= t <= 0xffffffff for t in chain) and
            chain.count(1000316002) == 1, 'Bounded enabled descriptorBuffer feature chain')
    size = integer(activation['sampler_descriptor_size'], 'Original descriptor property', 256)
    require(type(activation['create_sampler_result']) is int and
            activation['create_sampler_result'] == 0, 'Actual successful sampler creation')
    sampler = integer(activation['sampler_PE'], 'Original owned sampler')
    events = gates.loads(query_events_bytes, MAX_WITNESS_BYTES)
    exact_object(events, 'schema_version run_id events', 'Original PE query journal')
    require(type(events['schema_version']) is int and events['schema_version'] == 1 and
            events['run_id'] == run_id and type(events['events']) is list and
            len(events['events']) == 4, 'Two complete original calls')
    for sequence, query in enumerate(witness['queries'], 1):
        require(query['args']['sampler'] == sampler and query['args']['data_size'] == size,
                'Query matches activated sampler and original property')
        before, after = events['events'][2 * (sequence - 1):2 * sequence]
        for edge, row in (('before', before), ('after', after)):
            exact_object(row, 'sequence api run_id phase args return_kind edge '
                         'original_returned result' + (' output_hex' if edge == 'after' else ''),
                         'Exact original PE query edge')
            scopes.exact(row['phase'], query['phase'], 'Exact original phase types')
            scopes.exact(row['args'], query['args'], 'Exact original operand types')
            require(type(row['sequence']) is int and row['sequence'] == sequence and
                    row['api'] == 'vkGetDescriptorEXT' and row['run_id'] == run_id and
                    row['return_kind'] == 'void' and row['edge'] == edge and
                    row['original_returned'] is (edge == 'after') and row['result'] is None,
                    'Original BEFORE/AFTER arguments and void semantics')
        require(after['output_hex'] == query['output_hex'], 'Original returned bytes witness')
    return dict(original_PE_query_edges_verified=4, original_PE_queries=2,
                ready_and_activation_outputs_joined=True,
                descriptorBuffer_enabled_original_receipt=True,
                original_returned_opaque_bytes=2 * size,
                input_facts_attested=False, PE_to_Unix_handle_mapping_known=False)


def analyze(journals, claims, run_id, build_id, process_reports, witness,
            overflow=False):
    """Positive callback join only. Caller-supplied facts are not attested here."""
    unix = scopes.analyze(journals, claims, run_id, build_id, overflow=overflow)
    selected = process_identity(process_reports, run_id)
    witness = validate_witness(witness, run_id)
    # A final claim not inventoried at the second live-process gate is a gap.
    known = {c['scope_slot']: c for c in process_reports[1]['claim_identities']}
    require(set(known) == set(claims), 'Final claims differ from inventoried claims')
    for scope in unix['scopes']:
        identity = scope['identity']
        c = known[identity['scope_slot']]
        require(all(identity[k] == c[k] for k in
                    ('host_pid', 'process_start_cookie', 'dso_epoch')),
                'Final claim changed since process gate')
    joins, missing = [], []
    for query in witness['queries']:
        args = query['args']
        candidates = []
        for scope in unix['scopes']:
            if scope['status'] != 'valid':
                continue
            identity = scope['identity']
            for event in scope['queries']:
                # PE and Unix dispatchable devices are deliberately NOT compared.
                # This is the original unchanged private pData/size/type/input
                # witness through the pinned x64 thunk, not opaque-byte identity.
                if (event['descriptor_type'] == args['descriptor_type'] and
                        event['info_sType'] == args['info_sType'] and
                        event['data_size'] == args['data_size'] and
                        event['output_pointer'] == args['output_pointer'] and
                        event['typed_input']['known'] and
                        event['typed_input']['kind'] == 'sampler' and
                        event['typed_input']['sampler'] == args['sampler'] and
                        event['phase_before'] == dict(status='known', **query['phase']) and
                        event['phase_after'] == dict(status='known', **query['phase']) and
                        event['phase_unchanged'] and event['original_returned'] and
                        event['output_captured'] and
                        event['output_hex'] == query['output_hex']):
                    candidates.append((scope, event))
        if len(candidates) != 1:
            missing.append(dict(sequence=query['sequence'],
                                reason='original_witness_absent' if not candidates
                                else 'original_witness_ambiguous'))
            continue
        scope, event = candidates[0]
        identity = scope['identity']
        if any(identity[k] != selected[k] for k in selected):
            missing.append(dict(sequence=query['sequence'], reason='host_owner_mismatch'))
            continue
        if not event['device_lifetime_known']:
            missing.append(dict(sequence=query['sequence'], reason='device_lifetime_unknown'))
            continue
        joins.append(dict(sequence=query['sequence'], scope_slot=identity['scope_slot'],
                          host_pid=identity['host_pid'],
                          process_start_cookie=identity['process_start_cookie'],
                          dso_epoch=identity['dso_epoch'],
                          Unix_device=event['device'],
                          Unix_device_generation=event['device_generation'],
                          Unix_parent_instance_generation=event['parent_instance_generation'],
                          query_operation_ordinal=event['operation_ordinal'],
                          query_return_ordinal=event['return_ordinal'],
                          PE_device=args['device'], exact_original_witness=True))
    same = len(joins) == 2 and all(joins[0][k] == joins[1][k] for k in
                 ('scope_slot', 'host_pid', 'process_start_cookie', 'dso_epoch',
                  'Unix_device', 'Unix_device_generation',
                  'Unix_parent_instance_generation', 'PE_device'))
    if same:
        same = joins[0]['query_return_ordinal'] < joins[1]['query_operation_ordinal']
    positive = same and not missing
    return dict(schema_version=1, purpose='owned_translated_original_query_join',
                run_id=run_id, process_correlation_known=True,
                original_PE_query_device_witnessed=positive,
                finite_saved_scope_and_owner_complete=positive and
                unix['all_saved_scopes_complete'],
                joins=joins, unjoined=missing,
                bracketing_queries_same_live_device=same,
                Unix_scope_audit=unix,
                query_boundary_uses_private_original_output_spans=True,
                windows_PID_compared_to_host_PID=False,
                PE_device_equality_to_Unix_device_used=False,
                loader_thunk_source_contract_is_loaded_binary_attestation=False,
                **{key: False for key in FALSE_FACETS})
