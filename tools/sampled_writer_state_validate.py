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
                'fragment_shading_rate_known', 'effective_write_coverage_known',
                'push_values_identify_sampled_resource', 'fragment_shader_reads_push_constants',
                'depth_clip_value_known', 'effective_clipping_known', 'effective_operand_claimed')
MODES = {'unknown', 'static', 'dynamic', 'with_count'}
UINT32 = (1 << 32)-1
FSR_ATTACHMENT = 0x00200000
# Pinned Vulkan1.3.275 enum values. The auditor does not import the producer's
# header or accept its supplied reason names as its own interpretation.
LEGACY_ALLOWED_FLAGS = 0x20800fc7
PIPELINE_CHAIN_TYPES = {1000320002, 1000290000, 1000470005, 1000226001, 1000044002, 1000192000}
FSR_STATIC_TYPE = 1000226001
FSR_DYNAMIC_TYPE = 1000226000
DEPTH_CLIP_TYPE = 1000102001
REGULAR_DYNAMIC = (0, 1)
COUNT_DYNAMIC = (1000267003, 1000267004)
ALLOWED_DYNAMIC = set(range(9)) | set(range(1000267000, 1000267012)) | {1000377002, 1000377004, FSR_DYNAMIC_TYPE}
FACETS = ('push_layout', 'viewport_declaration', 'scissor_declaration')
REASON_GROUPS = ('structure', *FACETS, 'fragment_shading_rate')
REASONS = (
    'no_pipeline', 'invalid_pipeline_create', 'unknown_pipeline_flags', 'unknown_pipeline_chain',
    'duplicate_pipeline_chain', 'pipeline_chain_limit', 'incomplete_library_list', 'missing_library',
    'overlapping_library_parts', 'invalid_library_parts', 'missing_required_owner', 'library_not_executable',
    'unknown_push_layout', 'incomplete_push_layout', 'incompatible_push_layouts', 'invalid_dynamic_info',
    'unknown_dynamic_chain', 'dynamic_flags', 'dynamic_limit', 'missing_dynamic_values', 'duplicate_dynamic_state',
    'unsupported_dynamic_state', 'conflicting_viewport_modes', 'conflicting_scissor_modes', 'missing_raster_state',
    'invalid_raster_state', 'unknown_raster_chain', 'raster_flags', 'rasterizer_discard', 'missing_viewport_state',
    'invalid_viewport_state', 'unknown_viewport_chain', 'viewport_flags', 'fragment_shading_rate_unobserved',
    'rendering_fsr_attachment', 'fsr_static_declaration', 'fsr_dynamic_declaration',
    'depth_clip_flags', 'depth_clip_unobserved')
REASON_BITS = {name: 1 << index for index, name in enumerate(REASONS)}


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


def bounded_types(value, limit):
    if not isinstance(value, list) or len(value) > limit:
        raise ValueError('Declaration type list exceeded its fixed bound')
    return [integer(item, -(1 << 31), (1 << 31)-1) for item in value]


def audit_facets(value):
    """Validate schema2 declarations without upgrading any schema1 receipt."""
    if value.get('conventional_operand_scope') is not True:
        raise ValueError('Schema2 must name its limited conventional-operand scope')
    if value.get('pipeline_declarations_complete_scope') != 'legacy_header_admission_summary':
        raise ValueError('Legacy admission summary was promoted to whole-pipeline knowledge')
    for field in ('fragment_shading_rate_known', 'effective_write_coverage_known'):
        if value.get(field) is not False:
            raise ValueError('Independent declarations cannot establish a fragment rate or coverage')
    facets, reasons, masks = (value[key] for key in ('pipeline_facet_known', 'pipeline_facet_reasons', 'pipeline_facet_reason_masks'))
    if (not isinstance(facets, dict) or set(facets) != set(FACETS)
            or not isinstance(reasons, dict) or set(reasons) != set(REASON_GROUPS)
            or not isinstance(masks, dict) or set(masks) != set(REASON_GROUPS)):
        raise ValueError('Schema2 facet names or reason groups changed')
    version = value['schema_version']
    for group in REASON_GROUPS:
        mask = integer(masks[group], 0, (1 << (38 if version == 3 else 37))-1)
        names = [name for index, name in enumerate(REASONS) if mask & (1 << index)]
        if reasons[group] != names:
            raise ValueError('Facet reason names disagree with their fixed bit assignments')
    structure = masks['structure']
    for facet in FACETS:
        writer.boolean(facets[facet], facet)
        if facets[facet] != (masks[facet] == 0) or masks[facet] & structure != structure:
            raise ValueError('Known facet erased its own reason or a structural ambiguity')
    creation = value['pipeline_creation']
    for field in ('own_parts', 'parts', 'pre_raster_owners', 'fragment_owners', 'legacy_flags'):
        integer(creation[field])
    inherited = integer(creation['inherited_rendering_flags'], 0, (1 << 64)-1)
    inherited_unknown = integer(creation['inherited_unknown_flags'], 0, (1 << 64)-1)
    recognized = integer(value['pipeline_recognized_rendering_flags'], 0, (1 << 64)-1)
    flags, unknown = value['pipeline_flags'], value['pipeline_unknown_flags']
    if recognized != flags & FSR_ATTACHMENT or inherited & ~FSR_ATTACHMENT:
        raise ValueError('An unreviewed rendering flag was silently recognized')
    if unknown != flags & ~(LEGACY_ALLOWED_FLAGS | FSR_ATTACHMENT):
        raise ValueError('Unknown pipeline flags were silently dropped or reclassified')
    if (unknown or inherited_unknown) and not structure & REASON_BITS['unknown_pipeline_flags']:
        raise ValueError('Unknown own/inherited pipeline flag lost its structural reason')
    generation = value['pipeline_generation']
    for field in ('structural_known', 'chain_truncated', 'chain_duplicate', 'fsr_static_declared'):
        writer.boolean(creation[field], field)
    if creation['structural_known'] != bool(generation and structure == 0):
        raise ValueError('Structural declaration knowledge differs from its immutable reasons')
    if not generation and not structure & REASON_BITS['no_pipeline']:
        raise ValueError('Missing original pipeline was relabeled as a known declaration')
    chain = bounded_types(creation['chain_types'], 32)
    if creation['chain_duplicate'] != (len(set(chain)) != len(chain)):
        raise ValueError('Duplicate creation-chain diagnostics changed')
    if creation['chain_duplicate'] and not structure & REASON_BITS['duplicate_pipeline_chain']:
        raise ValueError('Duplicate creation chain lost its conservative reason')
    if creation['chain_truncated'] and (len(chain) != 32 or not structure & REASON_BITS['pipeline_chain_limit']):
        raise ValueError('Truncated creation chain was admitted without its fixed-bound reason')
    unrecognized = list(dict.fromkeys(item for item in chain if item not in PIPELINE_CHAIN_TYPES))
    if (bounded_types(creation['unknown_chain_types'], 8) != unrecognized[:8]
            or integer(creation['unknown_chain_count'], 0, 32) != len(unrecognized)):
        raise ValueError('Unknown creation-chain types/count were lost or fabricated')
    if unrecognized and not structure & REASON_BITS['unknown_pipeline_chain']:
        raise ValueError('Unreviewed creation extension silently admitted the pipeline')
    source = creation['flag_source']
    if source not in ('legacy', 'flags2') or (source == 'flags2') != (1000470005 in chain):
        raise ValueError('Effective flag source differs from the retained original chain')
    if source == 'legacy' and creation['legacy_flags'] != flags:
        raise ValueError('Legacy pipeline flags were replaced by another creation')
    if creation['own_parts'] & ~15 and not structure & REASON_BITS['invalid_library_parts']:
        raise ValueError('Invalid GPL ownership bits lost their reason')
    for facet in FACETS:
        missing_owner = creation['parts'] != 15 or creation['pre_raster_owners'] != 1
        if facet == 'push_layout':
            missing_owner = missing_owner or creation['fragment_owners'] != 1
        if generation and missing_owner and not masks[facet] & REASON_BITS['missing_required_owner']:
            raise ValueError('Missing or ambiguous GPL owner was promoted to a complete facet')
        if flags & 0x800 and not masks[facet] & REASON_BITS['library_not_executable']:
            raise ValueError('A library declaration was treated as an executable pipeline')
    rate = masks['fragment_shading_rate']
    if not rate & REASON_BITS['fragment_shading_rate_unobserved']:
        raise ValueError('Unobserved rate sources were cleared by a narrow facet admission')
    if bool(rate & REASON_BITS['rendering_fsr_attachment']) != bool(recognized or inherited):
        raise ValueError('The recognized FSR attachment flag lost its separate rendering gap')
    if creation['fsr_static_declared'] != bool(FSR_STATIC_TYPE in chain and creation['own_parts'] & 6):
        raise ValueError('FSR static declaration was inferred from ignored link-only metadata')
    if creation.get('fsr_static_declared_scope') != 'own_parts':
        raise ValueError('FSR static metadata was promoted from an ignored or inherited creation pointer')
    if creation['fsr_static_declared'] and not rate & REASON_BITS['fsr_static_declaration']:
        raise ValueError('FSR static presence was promoted without its unknown-rate reason')
    expected_summary = bool(generation and not structure and not rate & ~REASON_BITS['fragment_shading_rate_unobserved'])
    if value['pipeline_declarations_complete'] != expected_summary:
        raise ValueError('Legacy whole-header summary was retagged by facet admission')
    if value['push']['layout_known'] != facets['push_layout']:
        raise ValueError('Bound push layout was promoted beyond its declaration facet')
    raster = value['raster_declarations']
    if raster.get('scope') != 'pre_raster_owner':
        raise ValueError('Raster metadata was detached from its actual declaration owner')
    for info in ('dynamic', 'raster', 'viewport'):
        observed = writer.boolean(raster[info+'_info_observed'], info+'_info_observed')
        absent = writer.boolean(raster[info+'_info_absent'], info+'_info_absent')
        if observed and absent:
            raise ValueError('An inspected state structure was also asserted absent')
        if absent and raster[info+'_chain_types']:
            raise ValueError('Absent state structure contains an invented extension chain')
    integer(raster['owner_generation'], 0, (1 << 64)-1)
    dynamic = bounded_types(raster['dynamic_state_types'], 64)
    count = integer(raster['dynamic_state_count'])
    if raster['dynamic_info_absent'] and (dynamic or count):
        raise ValueError('Absent dynamic-state pointer was promoted to a parsed array')
    for info, missing in (('raster', 'missing_raster_state'), ('viewport', 'missing_viewport_state')):
        if raster[info+'_info_absent'] and any(not masks[facet] & REASON_BITS[missing]
                    for facet in ('viewport_declaration', 'scissor_declaration')):
            raise ValueError('A missing required structure lost its original refusal reason')
    writer.boolean(raster['dynamic_truncated'], 'dynamic_truncated')
    if raster['dynamic_truncated'] != (count > 64):
        raise ValueError('Dynamic declaration bound/truncation changed')
    if len(dynamic) != count and not any(masks[facet] & sum(REASON_BITS[key] for key in
            ('invalid_dynamic_info', 'unknown_dynamic_chain', 'dynamic_flags', 'dynamic_limit', 'missing_dynamic_values'))
            for facet in ('viewport_declaration', 'scissor_declaration')):
        raise ValueError('Dynamic declarations disappeared without an explicit unknown reason')
    rejected = [item for item in dynamic if item not in ALLOWED_DYNAMIC]
    if (bounded_types(raster['rejected_dynamic_types'], 8) != rejected[:8]
            or integer(raster['rejected_dynamic_count'], 0, 64) != len(rejected)):
        raise ValueError('Unsupported dynamic declarations/count were lost or fabricated')
    if not raster['dynamic_info_observed'] and (dynamic or rejected or raster['fsr_dynamic_declared']):
        raise ValueError('An unobserved dynamic header acquired interpreted array values')
    invalid_headers = {'dynamic': 'invalid_dynamic_info', 'raster': 'invalid_raster_state',
                       'viewport': 'invalid_viewport_state'}
    for info, reason_name in invalid_headers.items():
        invalid_header = all(masks[facet] & REASON_BITS[reason_name]
                             for facet in ('viewport_declaration', 'scissor_declaration'))
        # A refused header can retain bounded raw count/chain diagnostics. It
        # cannot acquire interpreted array values or assert a missing pointer.
        unobserved = not raster[info+'_info_observed']
        if unobserved and (raster[info+'_info_absent'] or not invalid_header):
            if (raster[info+'_chain_types'] or raster[info+'_chain_truncated']
                    or (info == 'dynamic' and (count or raster['dynamic_truncated']))):
                raise ValueError('Unobserved state metadata has no retained invalid-header explanation')
    for facet in ('viewport_declaration', 'scissor_declaration'):
        mask = masks[facet]
        if rejected and not mask & REASON_BITS['unsupported_dynamic_state']:
            raise ValueError('Unsupported dynamic state silently restored a conventional operand')
        if count > 64 and not mask & REASON_BITS['dynamic_limit']:
            raise ValueError('Unbounded dynamic array lost its refusal reason')
        if len(set(dynamic)) != len(dynamic) and not mask & REASON_BITS['duplicate_dynamic_state']:
            raise ValueError('Duplicate dynamic state lost its refusal reason')
    for prefix, reason_name in (('dynamic', 'unknown_dynamic_chain'), ('raster', 'unknown_raster_chain'), ('viewport', 'unknown_viewport_chain')):
        types = bounded_types(raster[prefix+'_chain_types'], 8)
        truncated = writer.boolean(raster[prefix+'_chain_truncated'], prefix+'_chain_truncated')
        if truncated and len(types) != 8:
            raise ValueError('Raster subchain truncation lost its bounded prefix')
        unreviewed = bool(types or truncated)
        if version == 3 and prefix == 'raster':
            unreviewed = audit_depth_clip(value, types, truncated)
        if unreviewed and any(not masks[facet] & REASON_BITS[reason_name]
                                      for facet in ('viewport_declaration', 'scissor_declaration')):
            raise ValueError('Unreviewed raster subchain was cleared by another facet')
    writer.boolean(raster['fsr_dynamic_declared'], 'fsr_dynamic_declared')
    if raster['fsr_dynamic_declared'] != (FSR_DYNAMIC_TYPE in dynamic):
        raise ValueError('FSR dynamic presence differs from the retained original declarations')
    if raster['fsr_dynamic_declared'] and not rate & REASON_BITS['fsr_dynamic_declaration']:
        raise ValueError('FSR dynamic declaration lost the unknown rate-value gap')
    for field in ('viewport_count', 'scissor_count'):
        integer(raster[field])
    for field, facet in (('viewport', 'viewport_declaration'), ('scissor', 'scissor_declaration')):
        mode = raster[field+'_mode']
        if mode not in MODES:
            raise ValueError('Raster declaration has an unsupported mode')
        observation = value[field]
        if observation.get('effective_known_scope') != 'single_conventional_operand':
            raise ValueError('Conventional operand knowledge was promoted to complete raster coverage')
        if not facets[facet]:
            if observation['mode'] != 'unknown' or observation['value_known'] or observation['count_known'] or observation['effective_known']:
                raise ValueError('Unknown declaration facet retained a known conventional operand')
        else:
            if (not raster['owner_generation'] or not raster['raster_info_observed']
                    or not (raster['dynamic_info_observed'] or raster['dynamic_info_absent'])):
                raise ValueError('Known conventional declaration has no observed immutable owner structures')
            if mode == 'unknown':
                raise ValueError('Known creation facet has no captured conventional declaration mode')
            if mode != 'with_count' and not raster['viewport_info_observed']:
                raise ValueError('Known static-count declaration was inferred from an ignored viewport pointer')
            expected_owner = raster['owner_generation'] if mode == 'static' else generation
            if observation['pipeline_generation'] != expected_owner:
                raise ValueError('Saved conventional operand was attributed to a different owner generation')
            overflow_unknown = (value['overflow'] and observation['mode'] == 'unknown'
                                and not any(observation[key] for key in ('value_known', 'count_known', 'effective_known')))
            if observation['mode'] != mode and not overflow_unknown:
                raise ValueError('Saved conventional mode differs from its immutable owner declaration')
        if facets[facet] and mode == 'with_count' and COUNT_DYNAMIC[0 if field == 'viewport' else 1] not in dynamic:
            raise ValueError('WithCount mode was inferred without its original dynamic declaration')
        if facets[facet] and mode == 'dynamic' and REGULAR_DYNAMIC[0 if field == 'viewport' else 1] not in dynamic:
            raise ValueError('Regular dynamic mode was inferred without its original declaration')
    return facets


def audit_depth_clip(value, types, truncated):
    """Review a declaration's flags only, never its clipping payload or effect."""
    raster = value['raster_declarations']
    declared = writer.boolean(raster['depth_clip_declared'], 'depth_clip_declared')
    indices = [index for index, item in enumerate(types) if item == DEPTH_CLIP_TYPE]
    if declared != bool(indices):
        raise ValueError('DepthClip declaration presence differs from the retained type list')
    nodes = raster['reviewed_depth_clip_nodes']
    if not isinstance(nodes, list) or len(nodes) != len(indices):
        raise ValueError('Reviewed DepthClip node count differs from the original bounded chain')
    prior_refusal = any(value['pipeline_facet_reason_masks'][facet] & (
        REASON_BITS['raster_flags'] | REASON_BITS['rasterizer_discard'])
        for facet in ('viewport_declaration', 'scissor_declaration'))
    reviewable = raster['raster_info_observed'] and not truncated and len(set(types)) == len(types) and not prior_refusal
    bad_flags = False
    for node, index in zip(nodes, indices):
        if set(node) != {'chain_index', 'flags_known', 'flags'} or integer(node['chain_index'], 0, 7) != index:
            raise ValueError('DepthClip flags were attributed to a different original chain position')
        known = writer.boolean(node['flags_known'], 'flags_known')
        if known != reviewable:
            raise ValueError('DepthClip flags were promoted from an unreviewed chain or erased after review')
        if known:
            bad_flags |= bool(integer(node['flags']))
        elif node['flags'] is not None:
            raise ValueError('Unknown DepthClip flags contain a current value')
    for facet in ('viewport_declaration', 'scissor_declaration'):
        if bool(value['pipeline_facet_reason_masks'][facet] & REASON_BITS['depth_clip_flags']) != bad_flags:
            raise ValueError('Nonzero DepthClip flags lost their independent conventional-operand refusal')
    if (value.get('depth_clip_value_known') is not False or value.get('effective_clipping_known') is not False
            or value.get('clipping_reasons') != ['depth_clip_unobserved']
            or value.get('clipping_reason_mask') != REASON_BITS['depth_clip_unobserved']):
        raise ValueError('A reviewed declaration was promoted to a known clipping value or effect')
    return bool(types) and (not reviewable or any(item != DEPTH_CLIP_TYPE for item in types))


def golden(value):
    if not isinstance(value, dict) or type(value.get('schema_version')) is not int or value['schema_version'] != 1:
        raise ValueError('Owned draw state control schema is unavailable')
    claims(value)
    clip_fields = ('depth_clip_feature_enabled', 'depth_clip_chain_type', 'depth_clip_state_flags', 'depth_clip_enable_payload')
    if any(key in value for key in clip_fields):
        if (value.get('depth_clip_feature_enabled') is not True or value.get('depth_clip_chain_type') != DEPTH_CLIP_TYPE
                or type(value.get('depth_clip_state_flags')) is not int or value['depth_clip_state_flags'] != 0
                or value.get('depth_clip_enable_payload') is not True):
            raise ValueError('Owned DepthClip control did not preserve its explicitly enabled fixed configuration')
    flags = integer(value.get('pipeline_flags', 0), 0, (1 << 64)-1)
    if flags:
        if flags != FSR_ATTACHMENT or value.get('attachment_fsr_feature_enabled') is not True:
            raise ValueError('Owned flag-only control did not enable the exact FSR attachment feature')
        for key in ('rate_attachment_present', 'static_rate_pnext_present', 'dynamic_rate_declared'):
            if value.get(key) is not False:
                raise ValueError('Owned FSR control changed its explicit flag-only rendering configuration')
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
        conventional_valid = finite and floats[2] > 0 and floats[3] != 0 and 0 <= floats[4] <= 1 and 0 <= floats[5] <= 1
    else:
        offsets = array(value['offset'], 2, -(1 << 31), (1 << 31)-1)
        extents = array(value['extent'], 2)
        conventional_valid = all(offset >= 0 and offset+extent <= (1 << 31)-1
                                 for offset, extent in zip(offsets, extents))
    if value['value_known'] and value['conventional_valid'] != conventional_valid:
        raise ValueError('Conventional operand validity disagrees with its exact saved values')
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


def audit_raw_setters(value, *, original=None):
    """Audit saved call arguments independently of pipeline applicability."""
    banks = value['raw_setter_arguments']
    if not isinstance(banks, dict) or set(banks) != {'viewport', 'scissor'}:
        raise ValueError('Raw setter banks are not the two fixed argument snapshots')
    epochs, setter_occurrences = [], []
    for name in ('viewport', 'scissor'):
        raw, interpreted = banks[name], value[name]
        if raw.get('scope') != 'index_zero_argument_snapshot' or raw.get('effective_operand_claimed') is not False:
            raise ValueError('A raw setter argument was promoted to a current effective operand')
        captured = writer.boolean(raw['value_captured'], 'value_captured')
        bank_known = writer.boolean(raw['recording_bank_known'], 'recording_bank_known')
        origin = integer(raw['origin_ordinal'], 0, (1 << 64)-1)
        captured_epoch = integer(raw['capture_epoch'], 0, (1 << 64)-1)
        epoch = integer(raw['recording_epoch'], 1, (1 << 64)-1);epochs.append(epoch)
        if captured_epoch > epoch:
            raise ValueError('Historical setter capture epoch is later than its recording epoch')
        matches = writer.boolean(raw['recording_epoch_matches'], 'recording_epoch_matches')
        if matches != bool(origin and captured_epoch == epoch):
            raise ValueError('Raw setter capture epoch was relabeled as the current recording epoch')
        form, first, count = raw['form'], raw['first'], integer(raw['count'])
        if form not in ('unknown', 'regular', 'with_count'):
            raise ValueError('Raw setter API form was invented')
        if form == 'regular': integer(first)
        elif first is not None:
            raise ValueError('WithCount or absent setter acquired an invented first argument')
        if origin:
            if not origin < value['ordinal'] or not captured_epoch or form == 'unknown':
                raise ValueError('Raw setter origin is late, unordered or lacks its original form/epoch')
        elif captured or captured_epoch or count or form != 'unknown':
            raise ValueError('An absent setter acquired historical arguments')
        payload_fields = ('bits',) if name == 'viewport' else ('offset', 'extent')
        if captured:
            if not origin or count != 1 or form == 'regular' and first != 0:
                raise ValueError('Raw payload was read outside its safe single index-zero original call')
            if name == 'viewport': array(raw['bits'], 6)
            else:
                array(raw['offset'], 2, -(1 << 31), (1 << 31)-1);array(raw['extent'], 2)
        elif any(raw[field] is not None for field in payload_fields):
            raise ValueError('Unavailable raw setter payload retained bytes from another call')
        if bank_known and (not captured or not matches or value['overflow']):
            raise ValueError('Historical raw arguments were promoted to a known current recording bank')
        last = raw['last_observed_call']
        last_ordinal = integer(last['ordinal'], 0, (1 << 64)-1)
        last_count = integer(last['count']);last_form, last_first = last['form'], last['first']
        last_captured = writer.boolean(last['payload_captured_for_this_call'], 'payload_captured_for_this_call')
        if last_form not in ('unknown', 'regular', 'with_count'):
            raise ValueError('Last observed setter API form was invented')
        if last_form == 'regular': integer(last_first)
        elif last_first is not None:
            raise ValueError('Last WithCount or absent call acquired an invented first argument')
        if last_ordinal:
            if not origin <= last_ordinal < value['ordinal'] or last_form == 'unknown':
                raise ValueError('Last observed call is late or precedes the saved candidate origin')
            if not origin and (last_form == 'with_count' or last_first == 0 and last_count):
                raise ValueError('An observed candidate-changing call lost its original candidate metadata')
        elif last_count or last_form != 'unknown' or origin:
            raise ValueError('Absent last call was retagged with a candidate or arguments')
        if last_captured != bool(captured and origin == last_ordinal):
            raise ValueError('The last observed call was confused with an older captured argument')
        if origin == last_ordinal and origin and (form, first, count) != (last_form, last_first, last_count):
            raise ValueError('The same original setter occurrence has two different argument forms')
        if origin and last_ordinal > origin and not (last_form == 'regular' and (last_first > 0 or last_count == 0)):
            raise ValueError('A later candidate-changing call retained an earlier payload origin')
        if interpreted['epoch'] and interpreted['epoch'] != epoch:
            raise ValueError('Raw and interpreted snapshots came from different recording epochs')
        if value['pipeline_generation'] and (last_ordinal, last_count, 0 if last_first is None else last_first) != (
                interpreted['last_ordinal'], interpreted['last_count'], interpreted['last_first']):
            raise ValueError('Raw last-call arguments differ from the same draw snapshot metadata')
        mode = interpreted['mode']
        if mode in ('static', 'dynamic') and interpreted['count_known'] and interpreted['count'] != value['raster_declarations'][name+'_count']:
            raise ValueError('Conventional count was changed from its immutable pipeline declaration')
        if mode == 'static' and (interpreted['declared_first'] or interpreted['declared_count'] != interpreted['count']
                                  or interpreted['setter_ordinal'] or interpreted['count_ordinal']):
            raise ValueError('A static conventional operand was attributed to a different declaration or setter')
        if mode == 'dynamic' and interpreted['count_ordinal']:
            raise ValueError('Regular dynamic mode acquired a WithCount count origin')
        if mode == 'with_count':
            count_ordinal = interpreted['count_ordinal']
            if (count_ordinal and not count_ordinal < value['ordinal']) or interpreted['count_known'] and not count_ordinal:
                raise ValueError('WithCount knowledge was attributed to an unordered or late count occurrence')
            if interpreted['value_known'] and interpreted['count_known'] and (count_ordinal != origin or interpreted['count'] != count):
                raise ValueError('Known WithCount operands and count came from different original setter occurrences')
        if interpreted['value_known'] and interpreted['mode'] in ('dynamic', 'with_count'):
            if (not captured or not bank_known or not matches or origin != interpreted['setter_ordinal']
                    or form != ('regular' if interpreted['mode'] == 'dynamic' else 'with_count')
                    or count != interpreted['declared_count'] or interpreted['declared_first'] != 0):
                raise ValueError('Interpreted conventional operand was promoted from an unrelated raw setter')
            if any(raw[field] != interpreted[field] for field in payload_fields):
                raise ValueError('Interpreted operand bytes differ from the original captured setter arguments')
        if original is not None:
            if original['mode'] == 'static':
                if captured or origin or last_ordinal:
                    raise ValueError('Owned first static draw was retagged with a setter from another draw')
            else:
                expected_form = 'with_count' if original['mode'] == 'with_count' else 'regular'
                if (not captured or not bank_known or not matches or form != expected_form or count != 1
                        or last_ordinal != origin or not last_captured):
                    raise ValueError('Owned dynamic draw lost its exact original raw argument occurrence')
                for field in payload_fields:
                    golden_field = 'viewport_bits' if field == 'bits' else 'scissor_'+field
                    if raw[field] != original[golden_field]:
                        raise ValueError('Raw setter bytes differ from independently saved original application arguments')
        setter_occurrences.append({origin, last_ordinal, interpreted['count_ordinal']} - {0})
    if epochs[0] != epochs[1]:
        raise ValueError('Raw viewport/scissor arguments were taken from different recordings')
    if setter_occurrences[0] & setter_occurrences[1]:
        raise ValueError('Distinct original viewport/scissor calls acquired the same command ordinal')
    return banks


def audit_state(value, *, original=None, ordinal=None):
    if not isinstance(value, dict) or type(value.get('schema_version')) is not int or value['schema_version'] not in (1, 2, 3):
        raise ValueError('Frozen per-draw writer state schema is missing')
    version = value['schema_version']
    if version == 1 and any(key in value for key in ('pipeline_facet_known', 'pipeline_recognized_rendering_flags',
            'pipeline_facet_reasons', 'pipeline_facet_reason_masks', 'pipeline_creation', 'raster_declarations',
            'fragment_shading_rate_known', 'effective_write_coverage_known', 'conventional_operand_scope')):
        raise ValueError('Schema1 receipt was retroactively retagged with newer facet semantics')
    if version < 3 and (any(key in value for key in ('raw_setter_arguments', 'depth_clip_value_known',
            'effective_clipping_known', 'clipping_reasons', 'clipping_reason_mask'))
            or any(key in value.get('raster_declarations', {}) for key in ('depth_clip_declared', 'reviewed_depth_clip_nodes'))):
        raise ValueError('Historical receipt was retroactively retagged with raw setter or DepthClip semantics')
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
    if version == 1 and original is not None and (value['pipeline_flags'] != 0 or value['pipeline_unknown_flags'] != 0
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
            end = offset+size
            covered = source_layout['ranges_complete'] and any(
                item['stage_flags'] & stages == stages and item['offset'] <= offset
                and item['offset']+item['size'] >= end for item in source_layout['ranges'])
            if (not 0 < written < observed_ordinal or not stages & 16 or offset % 4 or size % 4
                    or not size or end > UINT32 or not offset <= index*4 or index*4+4 > end or not covered):
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
    facets = audit_facets(value) if version >= 2 else None
    if version == 3:
        audit_raw_setters(value, original=original)
    if version == 1 and (value['pipeline_unknown_flags'] or not value['pipeline_declarations_complete']):
        if push['layout_known'] or any(r['value_known'] or r['count_known'] or r['effective_known'] for r in (viewport, scissor)):
            raise ValueError('Legacy unknown whole-pipeline gate was silently upgraded to independent facets')
    arguments = value['arguments']
    if arguments['kind'] not in ('unknown', 'direct', 'indexed'):
        raise ValueError('Unknown draw argument interpretation')
    for field in ('command_kind', 'count', 'instance_count', 'first', 'first_instance'):
        integer(arguments[field])
    integer(arguments['vertex_offset'], -(1 << 31), (1 << 31)-1)
    if original is not None:
        original = golden(original)
        expected_flags = original.get('pipeline_flags', 0)
        if version >= 2:
            creation = value['pipeline_creation']
            if (value['pipeline_flags'] != expected_flags or value['pipeline_unknown_flags'] != 0
                    or not all(facets.values()) or not creation['structural_known']
                    or value['pipeline_declarations_complete'] != (expected_flags == 0)
                    or creation['own_parts'] != 15 or creation['parts'] != 15
                    or creation['pre_raster_owners'] != 1 or creation['fragment_owners'] != 1
                    or creation['flag_source'] != 'legacy' or creation['legacy_flags'] != expected_flags
                    or creation['inherited_rendering_flags'] or creation['inherited_unknown_flags']
                    or creation['chain_types'] != [1000044002]
                    or creation['fsr_static_declared'] or value['raster_declarations']['fsr_dynamic_declared']):
                raise ValueError('Owned narrow facet control differs from its exact original pipeline declaration')
            raster = value['raster_declarations']
            expected_dynamic = [] if original['mode'] == 'static' else list(COUNT_DYNAMIC if original['mode'] == 'with_count' else REGULAR_DYNAMIC)
            expected_raster_chain = [DEPTH_CLIP_TYPE] if version == 3 and 'depth_clip_chain_type' in original else []
            if (raster['dynamic_state_types'] != expected_dynamic or raster['dynamic_state_count'] != len(expected_dynamic)
                    or any(raster[key] for key in ('dynamic_chain_types', 'viewport_chain_types'))
                    or raster['raster_chain_types'] != expected_raster_chain
                    or raster['dynamic_info_observed'] != (original['mode'] != 'static')
                    or raster['dynamic_info_absent'] != (original['mode'] == 'static')
                    or raster['raster_info_observed'] is not True or raster['raster_info_absent']
                    or raster['viewport_info_observed'] != (original['mode'] != 'with_count')
                    or raster['viewport_info_absent']):
                raise ValueError('Owned raster declaration differs from its original controlled state')
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
    if version >= 2:
        result.update(writer_state_schema_version=version, pipeline_facet_known=dict(facets),
                      pipeline_recognized_rendering_flags=value['pipeline_recognized_rendering_flags'],
                      fragment_shading_rate_known=False, coverage_known=False)
    if version == 3:
        result.update(raw_argument_banks_audited=2,
                      raw_captured_payload_banks=sum(bank['value_captured'] for bank in value['raw_setter_arguments'].values()),
                      depth_clip_declared=value['raster_declarations']['depth_clip_declared'],
                      depth_clip_value_known=False, effective_clipping_known=False)
    return result


def audit_receipt(receipt, controls, *, observer_enabled=True, require_facets=False, require_raw=False):
    require_facets = require_facets or require_raw
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
        if require_raw and controls[(present-1, index)].get('depth_clip_feature_enabled') is not True:
            raise ValueError('Required raw control lacks the independently saved original DepthClip feature/configuration')
        if observer_enabled:
            if require_facets and event['writer_state'].get('schema_version') not in (2, 3):
                raise ValueError('New FSR facet control requires a newly produced facet observation')
            if require_raw and event['writer_state'].get('schema_version') != 3:
                raise ValueError('New raw setter control requires a newly produced schema3 observation')
            if event['writer_state_original_pipeline_handle'] != controls[(present-1, index)]['pipeline_handle']:
                raise ValueError('State pipeline was joined to a different raw original draw handle')
            results.append(audit_state(event['writer_state'], original=controls[(present-1, index)], ordinal=event['ordinal']))
        elif 'writer_state' in event or 'writer_state_original_pipeline_handle' in event:
            raise ValueError('Default disabled observer changed the legacy draw-event format')
    return {'capture_sequence': prefix['capture_sequence'], 'source_present': present,
            'draw_states_compared': len(results), 'legacy_draw_events_unchanged': not observer_enabled,
            'states': results}


def audit_directory(directory, *, observer_enabled=True, shader_directory=None, require_facets=False, require_raw=False):
    require_facets = require_facets or require_raw
    directory = Path(directory)
    base = writer.audit_directory(directory, require_available=True, shader_directory=shader_directory)
    originals = [golden(row) for row in blit.json_lines(directory/'sampled-writer-fixture.jsonl')]
    if require_raw and any(row.get('depth_clip_feature_enabled') is not True for row in originals):
        raise ValueError('Required raw setter trial did not preserve the explicit original DepthClip feature/configuration')
    controls = {(row['frame'], row['draw_index']): row for row in originals}
    if len(originals) != 128 or len(controls) != 128 or set(controls) != {(frame, draw) for frame in range(64) for draw in range(2)}:
        raise ValueError('Original fixture must retain exactly128 unique original draw controls')
    flags = {row.get('pipeline_flags', 0) for row in originals}
    if len(flags) != 1 or require_facets and flags != {FSR_ATTACHMENT}:
        raise ValueError('Required FSR pipeline flag was not held identical throughout the original workload')
    features = {(row['dynamic_count_enabled'], row['maintenance6_enabled']) for row in originals}
    if len(features) != 1:
        raise ValueError('Fixture device features changed within one original device lifetime')
    receipts = [blit.decode(blit.read(path)) for path in sorted(directory.glob('image-*.json'))]
    for path in sorted(directory.glob('pose-pair-*.json')):
        receipts.extend(blit.decode(blit.read(path))['eyes'])
    for path in sorted(directory.glob('vulkan-*.jsonl')):
        receipts.extend(row for row in blit.json_lines(path) if row.get('event') == 'gpu_capture_lease')
    rows = [audit_receipt(receipt, controls, observer_enabled=observer_enabled, require_facets=require_facets,
                          require_raw=require_raw) for receipt in receipts]
    dynamic_count, maintenance6 = next(iter(features))
    compared = [state for row in rows for state in row['states']]
    result = {'schema_version': 1, 'observation_only': True, 'observer_enabled': observer_enabled,
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
    if require_facets or any(row.get('writer_state_schema_version') in (2, 3) for row in compared):
        result.update(facet_schema_required=require_facets, application_pipeline_flags=next(iter(flags)),
                      newly_produced_schema2_draws=sum(row.get('writer_state_schema_version') == 2 for row in compared),
                      fragment_shading_rate_known=False)
    if require_raw or any(row.get('writer_state_schema_version') == 3 for row in compared):
        result.update(raw_schema_required=require_raw,
                      newly_produced_schema3_draws=sum(row.get('writer_state_schema_version') == 3 for row in compared),
                      depth_clip_value_known=False, effective_clipping_known=False)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation', type=Path, required=True)
    parser.add_argument('--observer-disabled', action='store_true')
    parser.add_argument('--shader-directory', type=Path)
    parser.add_argument('--require-facets', action='store_true', help='Require new schema2 states from the controlled FSR pipeline-flag configuration')
    parser.add_argument('--require-raw', action='store_true', help='Require schema3 raw arguments and the controlled original DepthClip configuration')
    args = parser.parse_args()
    print(json.dumps({'new_gpu_execution_performed': False,
                      'audit': audit_directory(args.validation, observer_enabled=not args.observer_disabled,
                                               shader_directory=args.shader_directory, require_facets=args.require_facets,
                                               require_raw=args.require_raw)}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(str(error))
