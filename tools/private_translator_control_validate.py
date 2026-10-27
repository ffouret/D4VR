"""Pure audit of the new private-source admission/control CPU prototype.

This validates supplied receipt facts. It neither loads the translator nor
attests an object, descriptor, process, original call or GPU observation.
"""
from __future__ import annotations

import json

import typed_copy_getter_validate as getters

n = getters.native
MAX_WIRE = 16384
MAX_REPORT = 1048576
MAX_BATCH = 1048576
MAX_BATCH_REPORT = 4194304
PURPOSE = 'owned_private_translator_source_control_CPU_prototype'
CLAIMS = (
    'input_facts_attested', 'opaque_descriptor_decoded',
    'actual_source_contents_at_copy_proven', 'current_contents_at_draw_known',
    'unchanged_since_copy_proven', 'sampled_resource_GPU_read_proven',
    'generic_PE_to_Unix_resource_mapping', 'renderer_owner_known',
    'exclusive_phase_attribution_proven', 'global_API_absence_proven',
    'old_process_or_backing_gate_repaired', 'public_device_create_result_known',
    'source_runtime_relation_known', 'same_run_bind_provenance_known',
    'getter_to_bind_relation_known', 'backing_known',
    'loaded_binary_source_equivalence_proven',
)


def _complete_budget(value, cap):
    size = 0
    try:
        for chunk in json.JSONEncoder(ensure_ascii=True, allow_nan=False,
                                      separators=(',', ':')).iterencode(value):
            size += len(chunk.encode('ascii'))
            n.require(size <= cap, 'Complete report budget; no truncation')
    except (TypeError, UnicodeError, RecursionError) as error:
        raise ValueError('Bounded integer JSON report') from error


def audit_private_source_wire(data):
    """Check one complete <=16KiB receipt; frozen getter audit runs once."""
    value = n.parse(data, MAX_WIRE)
    n.obj(value, 'schema_version purpose prototype_only vendor_built '
                'runtime_executed admission getter_wire claims', 'Private CPU receipt')
    for key, expected in (('schema_version', 1), ('purpose', PURPOSE),
                          ('prototype_only', True), ('vendor_built', False),
                          ('runtime_executed', False)):
        n.exact(value[key], expected, 'Private CPU ' + key)
    n.obj(value['claims'], ' '.join(CLAIMS), 'Explicit unknown stronger facets')
    for key in CLAIMS:
        n.exact(value['claims'][key], False, key)
    admission = value['admission']
    n.obj(admission, 'selected_reserved selected_committed unselected rejected '
                     'pending_admissions pending_originals faulted', 'Source admission')
    for key in ('selected_reserved', 'selected_committed', 'rejected'):
        n.uint(admission[key], 'Admission ' + key, 5)
    n.uint(admission['unselected'], 'Unselected original creations')
    for key in ('pending_admissions', 'pending_originals'):
        n.exact(admission[key], 0, 'Seal refuses pending callbacks')
    n.boolean(admission['faulted'], 'Admission fault is explicit')
    n.exact(admission['selected_reserved'],
            admission['selected_committed'] + admission['rejected'],
            'Every consumed role finalized once before seal')
    n.require(not admission['rejected'] or admission['faulted'],
              'Rejected selected metadata faults the scope')
    legacy = getters.audit_getter_wire(value['getter_wire'])
    producer = value['getter_wire']['native_wire']['producer']
    n.require(not (producer['faulted'] or producer['gap']) or admission['faulted'],
              'Inner sticky metadata failure is visible in outer admission')
    if not admission['faulted']:
        source = legacy['source']
        n.require(admission['unselected'] <= source['ignored_calls'],
                  'Unselected creation originals belong to ignored source calls')
        n.require(admission['selected_committed'] + admission['unselected'] <=
                  source['source_calls'],
                  'Committed and unselected creations are distinct source originals')
    if legacy['prototype_source_history_known']:
        n.exact(admission['selected_committed'], 5,
                'Five selected original object occurrences passed admission')
    known = (legacy['prototype_source_history_known'] and
             not admission['faulted'] and not admission['rejected'])
    report = dict(
        schema_version=1, purpose='private_translator_control_CPU_audit',
        prototype_only=True, vendor_built=False, runtime_executed=False,
        run_id=legacy['run_id'], sequence=legacy['sequence'],
        private_source_history_known=known,
        reasons=[] if known else ['supplied_private_source_history_unavailable'],
        admission=admission, getter_audit=legacy,
        claims={key: False for key in CLAIMS},
    )
    _complete_budget(report, MAX_REPORT)
    return report


def audit_samples(data):
    """Audit <=64 detached CPU test samples; not a retained-ticket quota."""
    value = n.parse(data, MAX_BATCH)
    n.obj(value, 'schema_version purpose samples', 'CPU samples')
    n.exact(value['schema_version'], 1, 'CPU samples schema')
    n.exact(value['purpose'], 'private_translator_control_CPU_samples',
            'Distinct new CPU samples purpose')
    samples = value['samples']
    n.require(type(samples) is list and 0 < len(samples) <= 64,
              'At most64 detached CPU samples')
    names, reports = set(), []
    for sample in samples:
        n.obj(sample, 'name expected_history_known wire', 'New CPU sample')
        name = sample['name']
        n.require(type(name) is str and 0 < len(name) <= 64 and name not in names,
                  'Distinct bounded sample names')
        names.add(name)
        n.boolean(sample['expected_history_known'], 'Declared CPU expectation')
        report = audit_private_source_wire(sample['wire'])
        n.exact(report['private_source_history_known'], sample['expected_history_known'],
                'Actual new CPU sample expectation')
        reports.append(dict(name=name, audit=report))
    result = dict(schema_version=1, purpose='private_translator_control_CPU_samples_audit',
                  sample_count=len(reports),
                  known_histories=sum(x['audit']['private_source_history_known'] for x in reports),
                  samples=reports, prototype_only=True, vendor_built=False,
                  runtime_executed=False, input_facts_attested=False)
    _complete_budget(result, MAX_BATCH_REPORT)
    return result
