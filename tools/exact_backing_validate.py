"""Finite owned FD/mmap metadata audit, never a remote process identity policy.

This module reads no files or processes and creates no mappings. Its inputs are
caller-owned snapshots. Hash/maps results describe bounded logical helpers, not
a claim that one POSIX read syscall returned the whole buffer. A local original
mmap FD witness and optional map_files metadata are separate facets. Device
numbers from maps and fstat are preserved in their separate domains.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import PurePosixPath
import re
import stat


ROLES = ('fixture', 'd3d12', 'd3d12core', 'dxgi', 'observer')
MAX_FILE = 64 * 1024 * 1024
MAX_MAPS = 1024 * 1024
MAX_LINES = 8192
MAX_LINK = 4096
MAX_SNAPSHOT = 24 * 1024 * 1024
MAX_U64 = (1 << 64) - 1
OPEN_FLAGS = 0xA0800  # Linux control: O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK.
CALLS = ('open', 'fstat_before', 'hash_before', 'fstatfs', 'mmap',
         'maps_before', 'readlink_before', 'map_files_stat', 'readlink_after',
         'maps_after', 'fstat_after', 'hash_after', 'munmap', 'close')
FALSE_FACTS = ('input_facts_attested', 'remote_backing_identity_proven',
               'loaded_memory_bytes_attested', 'translated_Unix_device_owner_known',
               'PE_to_Unix_handle_mapping_known', 'global_query_absence_proven',
               'GPU_sampled_resource_known', 'global_device_translation_admitted',
               'OFF2_retrospectively_repaired', 'actual_Btrfs_cause_proven')


def require(value, message):
    if not value:
        raise ValueError(message)


def exact(value, keys, label):
    require(type(value) is dict and set(value) == set(keys), 'Exact object: ' + label)


def integer(value, label, maximum=MAX_U64, minimum=0):
    require(type(value) is int and minimum <= value <= maximum, 'Integer bound: ' + label)
    return value


def text(value, label, limit):
    require(type(value) is str and 0 < len(value.encode('utf-8')) <= limit
            and not any(ord(c) < 32 for c in value), 'Bounded text: ' + label)
    return value


def path(value, label):
    text(value, label, MAX_LINK)
    p = PurePosixPath(value)
    require(p.is_absolute() and str(p) == value and '..' not in p.parts
            and not value.startswith('//'), 'Exact absolute path: ' + label)
    return value


def sha(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{64}', value) is not None, 'SHA256')


def _bounded(value):
    stack = [(value, 0)]
    nodes = 0
    string_bytes = 0
    while stack:
        item, depth = stack.pop()
        nodes += 1
        require(nodes <= 4096 and depth <= 10, 'Snapshot structural cap')
        if type(item) is dict:
            require(all(type(k) is str for k in item), 'String JSON keys')
            require(len(item) <= 4096 - nodes, 'Snapshot object cap')
            stack.extend((v, depth + 1) for v in item.values())
        elif type(item) is list:
            require(len(item) <= 4096 - nodes, 'Snapshot array cap')
            stack.extend((v, depth + 1) for v in item)
        elif type(item) is str:
            string_bytes += len(item.encode('utf-8'))
            require(string_bytes <= MAX_SNAPSHOT, 'Snapshot string cap')
        else:
            require(item is None or type(item) in (int, bool), 'Finite typed JSON values')
    require(len(json.dumps(value, ensure_ascii=False, separators=(',', ':'),
                           allow_nan=False).encode('utf-8')) <= MAX_SNAPSHOT,
            'Snapshot serialized cap')


def parse_snapshot(data):
    """Strict bounded JSON transport; no filesystem or process access."""
    require(type(data) is bytes and len(data) <= MAX_SNAPSHOT and data.endswith(b'\n'),
            'Bounded newline snapshot bytes')

    def pairs(items):
        value = {}
        for key, item in items:
            require(key not in value, 'Duplicate JSON key')
            value[key] = item
        return value

    def number(value):
        require(len(value.lstrip('-')) <= 20, 'JSON integer digit cap')
        return int(value)

    def noninteger(_):
        raise ValueError('Noninteger/nonfinite JSON value')

    try:
        value = json.loads(data.decode('utf-8'), object_pairs_hook=pairs,
                           parse_int=number, parse_float=noninteger,
                           parse_constant=noninteger)
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise ValueError('Malformed snapshot JSON') from error
    _bounded(value)
    return value


def audit_bytes(data, *, expected_pins, run_id):
    return analyze(parse_snapshot(data), expected_pins=expected_pins, run_id=run_id)


def parse_maps(value):
    require(type(value) is str and value.endswith('\n')
            and len(value.encode('utf-8')) <= MAX_MAPS, 'Bounded complete maps')
    lines = value.splitlines()
    require(0 < len(lines) <= MAX_LINES, 'Maps line cap')
    previous = 0
    rows = []
    pattern = (r'([0-9a-f]{1,16})-([0-9a-f]{1,16}) ([r-][w-][x-][ps]) '
               r'([0-9a-f]{1,16}) ([0-9a-f]{1,8}):([0-9a-f]{1,8}) '
               r'([0-9]{1,20})(?: +(.*))?')
    for line in lines:
        m = re.fullmatch(pattern, line)
        require(m is not None, 'Exact maps line')
        lo, hi, perms, offset, major, minor, inode, name = m.groups()
        lo, hi, offset, major, minor = [int(x, 16) for x in (lo, hi, offset, major, minor)]
        inode = integer(int(inode), 'maps inode')
        require(previous <= lo < hi <= MAX_U64, 'Ordered nonoverlapping maps')
        previous = hi
        name = name or ''
        require(len(name.encode('utf-8')) <= MAX_LINK and '\x00' not in name, 'Maps path cap')
        rows.append(dict(start=lo, end=hi, permissions=perms, offset=offset,
                         device_major=major, device_minor=minor, inode=inode, path=name))
    return rows


def _call(value, last, name):
    exact(value, ('status', 'errno', 'ordinal', 'return_ordinal', 'result', 'value'), name)
    require(type(value['status']) is str and value['status'] in ('ok', 'error', 'not_attempted'),
            'Call status: ' + name)
    integer(value['errno'], 'errno', 4095)
    integer(value['ordinal'], 'entry', 140)
    integer(value['return_ordinal'], 'return', 140)
    if value['status'] == 'not_attempted':
        require(value['errno'] == 0 and value['ordinal'] == value['return_ordinal'] == 0
                and value['result'] is None and value['value'] is None, 'Absent call: ' + name)
        return last
    require(value['ordinal'] == last + 1 and value['return_ordinal'] == last + 2,
            'One serial entry/return clock: ' + name)
    if value['status'] == 'error':
        require(value['errno'] > 0 and type(value['result']) is int and value['result'] == -1
                and value['value'] is None, 'Exact error: ' + name)
    else:
        require(value['errno'] == 0, 'Successful errno: ' + name)
        integer(value['result'], 'Successful result: ' + name)
    return value['return_ordinal']


def _stat(value):
    exact(value, ('mode', 'size', 'device_major', 'device_minor', 'inode',
                  'mtime_ns', 'ctime_ns'), 'stat')
    integer(value['mode'], 'stat mode', 0xffff)
    integer(value['size'], 'stat size')
    integer(value['device_major'], 'stat major', 0xffffffff)
    integer(value['device_minor'], 'stat minor', 0xffffffff)
    integer(value['inode'], 'stat inode', minimum=1)
    for name in ('mtime_ns', 'ctime_ns'):
        integer(value[name], name, (1 << 63) - 1, -(1 << 63))


def _stat_matches(value, pin):
    return stat.S_ISREG(value['mode']) and all(value[k] == pin[k] for k in
               ('size', 'device_major', 'device_minor', 'inode'))


def _pin(value):
    exact(value, ('path', 'sha256', 'size', 'device_major', 'device_minor', 'inode'), 'pin')
    path(value['path'], 'pin')
    sha(value['sha256'])
    integer(value['size'], 'pin size', MAX_FILE, 1)
    for name in ('device_major', 'device_minor'):
        integer(value[name], 'pin ' + name, 0xffffffff)
    integer(value['inode'], 'pin inode', minimum=1)


def _payloads(calls):
    """Validate emitted types independently of whether the semantic join succeeds."""
    for name, c in calls.items():
        if c['status'] != 'ok':
            continue
        v = c['value']
        if name == 'open':
            exact(v, ('flags',), name)
            integer(c['result'], 'fd number', 0x7fffffff)
            integer(v['flags'], 'open flags', 0xffffffff)
        elif name in ('fstat_before', 'fstat_after', 'map_files_stat'):
            require(c['result'] == 0, 'stat original return')
            _stat(v)
        elif name in ('hash_before', 'hash_after'):
            exact(v, ('sha256', 'bytes', 'read_calls'), name)
            sha(v['sha256'])
            integer(v['bytes'], 'hash bytes', MAX_FILE)
            integer(v['read_calls'], 'hash read calls', 4097, 1)
            require(c['result'] == v['bytes'], 'Hash helper byte count')
        elif name == 'fstatfs':
            exact(v, ('type', 'fsid'), name)
            integer(v['type'], 'filesystem type', MAX_U64)
            require(type(v['fsid']) is list and len(v['fsid']) == 2, 'FSID shape')
            for x in v['fsid']:
                integer(x, 'FSID word', (1 << 31) - 1, -(1 << 31))
            require(c['result'] == 0, 'fstatfs return')
        elif name == 'mmap':
            exact(v, ('fd', 'fd_generation', 'mapping_generation', 'start', 'length',
                       'offset', 'prot', 'flags'), name)
            for k in v:
                integer(v[k], 'mmap ' + k)
            require(c['result'] == v['start'], 'Original mmap return address')
        elif name in ('maps_before', 'maps_after'):
            exact(v, ('text', 'bytes', 'lines', 'sha256', 'read_calls'), name)
            integer(v['bytes'], 'maps bytes', MAX_MAPS, 1)
            integer(v['lines'], 'maps lines', MAX_LINES, 1)
            integer(v['read_calls'], 'maps read calls', 513, 1)
            sha(v['sha256'])
            require(type(v['text']) is str and v['bytes'] == len(v['text'].encode('utf-8'))
                    and c['result'] == v['bytes'] and v['sha256'] == hashlib.sha256(
                        v['text'].encode('utf-8')).hexdigest(), 'Exact maps helper payload')
            require(len(parse_maps(v['text'])) == v['lines'], 'Exact maps line count')
        elif name in ('readlink_before', 'readlink_after'):
            text(v, 'map_files link', MAX_LINK)
            require(c['result'] == len(v.encode('utf-8')), 'readlink exact byte count')
        elif name == 'munmap':
            exact(v, ('start', 'length'), name)
            integer(v['start'], 'unmap start')
            integer(v['length'], 'unmap length')
            require(c['result'] == 0, 'munmap return')
        else:
            exact(v, ('fd',), name)
            integer(v['fd'], 'close fd', 0x7fffffff)
            require(c['result'] == 0, 'close return')


def analyze(snapshot, *, expected_pins, run_id):
    """Audit one owned serial control, returning fresh immutable-by-value facts.

    Malformed/contradictory envelopes raise ValueError. Well-formed errors,
    missing metadata and semantic mismatches produce explicit unknown facets.
    The result cannot authorize a remote process gate or reinterpret OFF2.
    """
    _bounded(snapshot)
    require(type(expected_pins) is list and len(expected_pins) == 5, 'Five external frozen pins')
    require(type(run_id) is str and re.fullmatch('[0-9a-f]{32}', run_id), 'Expected run nonce')
    for role, p in zip(ROLES, expected_pins):
        exact(p, ('role', 'path', 'sha256', 'size', 'device_major', 'device_minor', 'inode'),
              'external pin')
        require(type(p['role']) is str and p['role'] == role, 'External fixed role order')
        _pin({k: v for k, v in p.items() if k != 'role'})
    exact(snapshot, ('schema_version', 'purpose', 'run_id', 'host_pid',
                     'start_cookie_before', 'start_cookie_after', 'kernel_release',
                     'page_size', 'records'), 'snapshot')
    require(type(snapshot['schema_version']) is int and snapshot['schema_version'] == 1
            and snapshot['purpose'] == 'owned_exact_fd_mmap_backing_control', 'Snapshot schema')
    require(type(snapshot['run_id']) is str and snapshot['run_id'] == run_id,
            'Run nonce')
    integer(snapshot['host_pid'], 'owned host PID', 0xffffffff, 1)
    for key in ('start_cookie_before', 'start_cookie_after'):
        integer(snapshot[key], key, minimum=1)
    text(snapshot['kernel_release'], 'reported kernel release', 256)
    page = integer(snapshot['page_size'], 'page size', 65536, 4096)
    require(page & (page - 1) == 0, 'Power-of-two page size')
    records = snapshot['records']
    require(type(records) is list and len(records) == 5, 'Exactly five fixed roles')
    last = 0
    paths = set()
    output = []
    live_mapping = None
    cookie_known = snapshot['start_cookie_before'] == snapshot['start_cookie_after']
    for index, (role, record) in enumerate(zip(ROLES, records), 1):
        exact(record, ('role', 'pin', 'fd_generation', 'mapping_generation', 'open',
                       'fstat_before', 'hash_before', 'fstatfs', 'mmap', 'maps_before',
                       'map_files', 'maps_after', 'fstat_after', 'hash_after', 'munmap',
                       'close'), 'record')
        require(type(record['role']) is str and record['role'] == role, 'Fixed role order')
        require(type(record['fd_generation']) is int and type(record['mapping_generation']) is int
                and record['fd_generation'] == record['mapping_generation'] == index,
                'New serial FD/mapping generations')
        pin = record['pin']
        _pin(pin)
        require(pin['size'] >= page, 'Predeclared file contains one page')
        require(pin == {k: v for k, v in expected_pins[index - 1].items() if k != 'role'},
                'Exact external frozen role pin')
        require(pin['path'] not in paths, 'Five distinct exact paths')
        paths.add(pin['path'])
        mf = record['map_files']
        exact(mf, ('path', 'readlink_buffer_bytes', 'readlink_before', 'stat', 'readlink_after'),
              'map_files')
        require(type(mf['readlink_buffer_bytes']) is int and mf['readlink_buffer_bytes'] == 4097,
                'Original bounded readlink buffer')
        require(mf['path'] is None or type(mf['path']) is str, 'Optional exact map_files path')
        calls = {name: record[name] for name in CALLS if name not in
                 ('readlink_before', 'map_files_stat', 'readlink_after')}
        calls.update(readlink_before=mf['readlink_before'], map_files_stat=mf['stat'],
                     readlink_after=mf['readlink_after'])
        for name in CALLS:
            last = _call(calls[name], last, role + ':' + name)
        _payloads(calls)
        reasons = []
        if not cookie_known:
            reasons.append('owned_process_cookie_changed')
        needed = ('open', 'fstat_before', 'hash_before', 'mmap', 'maps_before',
                  'maps_after', 'fstat_after', 'hash_after')
        reasons.extend(name + ':' + calls[name]['status'] for name in needed
                       if calls[name]['status'] != 'ok')
        opened = calls['open']['status'] == 'ok'
        fd = calls['open']['result'] if opened else None
        mapped = calls['mmap']['status'] == 'ok'
        mv = calls['mmap']['value'] if mapped else None
        if not opened:
            require(all(calls[name]['status'] == 'not_attempted' for name in CALLS
                        if name != 'open'), 'No FD-dependent calls without a returned FD')
        if not mapped:
            require(all(calls[name]['status'] == 'not_attempted' for name in
                        ('maps_before', 'readlink_before', 'map_files_stat', 'readlink_after',
                         'maps_after', 'munmap')) and mf['path'] is None,
                    'No VMA metadata or unmap without a returned mapping')
        if opened and calls['open']['value']['flags'] != OPEN_FLAGS:
            reasons.append('original_open_flags_not_readonly_nofollow_cloexec')
        if mapped:
            require(live_mapping is None, 'Only one live owned mapping')
            live_mapping = dict(start=mv['start'], length=mv['length'])
            if (not opened or mv['fd'] != fd or mv['fd_generation'] != index
                    or mv['mapping_generation'] != index or mv['start'] == 0
                    or mv['start'] % page or mv['length'] != page or mv['offset'] != 0
                    or mv['prot'] != 0 or mv['flags'] != 2
                    or mv['start'] + mv['length'] > MAX_U64):
                reasons.append('original_mmap_arguments_not_exact_owned_one_page')
        for name in ('fstat_before', 'fstat_after'):
            if calls[name]['status'] == 'ok' and not _stat_matches(calls[name]['value'], pin):
                reasons.append(name + ':pin_or_regular_file_mismatch')
        if all(calls[name]['status'] == 'ok' for name in ('fstat_before', 'fstat_after')):
            if calls['fstat_before']['value'] != calls['fstat_after']['value']:
                reasons.append('held_FD_stat_changed')
        for name in ('hash_before', 'hash_after'):
            if calls[name]['status'] == 'ok' and calls[name]['value']['sha256'] != pin['sha256']:
                reasons.append(name + ':file_hash_mismatch')
            if calls[name]['status'] == 'ok' and calls[name]['value']['bytes'] != pin['size']:
                reasons.append(name + ':file_size_mismatch')
        selected = None
        target_identical = None
        texts_identical = None
        if all(calls[name]['status'] == 'ok' for name in ('maps_before', 'maps_after')):
            texts_identical = (calls['maps_before']['value']['text'] ==
                               calls['maps_after']['value']['text'])
            if mapped:
                selected_rows = []
                for name in ('maps_before', 'maps_after'):
                    rows = parse_maps(calls[name]['value']['text'])
                    hits = [r for r in rows if r['start'] == mv['start'] and
                            r['end'] == mv['start'] + mv['length']]
                    if len(hits) != 1:
                        reasons.append(name + ':unique_exact_VMA_not_present')
                    else:
                        selected_rows.append(hits[0])
                        if name == 'maps_before':
                            selected = hits[0]
                if len(selected_rows) == 2:
                    target_identical = selected_rows[0] == selected_rows[1]
                    if not target_identical:
                        reasons.append('exact_target_VMA_changed')
                if selected is not None:
                    if (selected['offset'] != 0 or selected['permissions'] != '---p'
                            or selected['path'] != pin['path'] or selected['inode'] != pin['inode']):
                        reasons.append('exact_VMA_path_inode_offset_or_permissions_mismatch')
        if calls['munmap']['status'] == 'ok':
            require(mapped and calls['munmap']['value'] == live_mapping,
                    'Original unmap exact held span')
            live_mapping = None
        if calls['close']['status'] == 'ok':
            require(opened and calls['close']['value']['fd'] == fd, 'Original close exact held FD')
        cleanup = ((not mapped or calls['munmap']['status'] == 'ok')
                   and (not opened or calls['close']['status'] == 'ok'))
        if not cleanup:
            reasons.append('cleanup_incomplete')
        local = not reasons
        backing_reasons = []
        expected_link = ('/proc/' + str(snapshot['host_pid']) + '/map_files/'
                         + format(mv['start'], 'x') + '-'
                         + format(mv['start'] + mv['length'], 'x')) if mapped else None
        if mf['path'] != expected_link:
            backing_reasons.append('map_files_path_not_exact_original_VMA')
        for name in ('readlink_before', 'map_files_stat', 'readlink_after'):
            c = calls[name]
            if c['status'] != 'ok':
                backing_reasons.append(name + ':' + c['status'])
            elif name == 'map_files_stat':
                if (not _stat_matches(c['value'], pin)
                        or calls['fstat_before']['status'] != 'ok'
                        or c['value'] != calls['fstat_before']['value']):
                    backing_reasons.append('map_files_target_stat_not_held_FD')
            elif c['value'] != pin['path']:
                backing_reasons.append(name + ':target_mismatch_deleted_or_truncated')
        if not local:
            backing_reasons.append('owned_mmap_FD_join_unknown')
        fs = calls['fstatfs']
        observed_stat = (calls['fstat_before']['value']
                         if calls['fstat_before']['status'] == 'ok' else None)
        observed_device = ({k: observed_stat[k] for k in ('device_major', 'device_minor')}
                           if observed_stat is not None else None)
        output.append(dict(role=role, fd_generation=index, mapping_generation=index,
                           fact_scope='owned_original_mapping_only',
                           pin=dict(pin), owned_fd_mmap_maps_join_known=local,
                           owned_join_reasons=reasons,
                           map_files_backing_metadata_known=not backing_reasons,
                           map_files_reasons=backing_reasons, cleanup_complete=cleanup,
                           maps_text_identical=texts_identical,
                           target_VMA_rows_identical=target_identical,
                           filesystem_metadata_known=fs['status'] == 'ok',
                           filesystem=({'type': fs['value']['type'], 'fsid': list(fs['value']['fsid'])}
                                       if fs['status'] == 'ok' else None),
                           maps_device=({k: selected[k] for k in ('device_major', 'device_minor')}
                                        if selected is not None else None),
                           FD_device=observed_device,
                           pinned_device={k: pin[k] for k in ('device_major', 'device_minor')},
                           device_fields_equal=(all(selected[k] == observed_stat[k] for k in
                                               ('device_major', 'device_minor'))
                                                if selected is not None and observed_stat is not None else None),
                           selected_VMA=dict(selected) if selected is not None else None))
    return dict(schema_version=1, purpose='owned_exact_backing_audit', run_id=snapshot['run_id'],
                owned_process_cookie_stable=cookie_known, transition_count=last,
                all_owned_fd_mmap_maps_joins_known=all(r['owned_fd_mmap_maps_join_known'] for r in output),
                all_map_files_backing_metadata_known=all(r['map_files_backing_metadata_known'] for r in output),
                all_cleanup_complete=all(r['cleanup_complete'] for r in output), records=output,
                **{key: False for key in FALSE_FACTS})
