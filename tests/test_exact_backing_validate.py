import copy
import hashlib
import json
import unittest
from unittest.mock import patch

from tools import exact_backing_validate as model


RUN_ID = '0123456789abcdef0123456789abcdef'


def expected_pins():
    return [dict(role=role, path='/owned/' + role + '.bin', sha256=str(i + 1) * 64,
                 size=8192 + i, device_major=0, device_minor=60, inode=1000 + i)
            for i, role in enumerate(model.ROLES)]


def snapshot(*, map_files=True, maps_device=36, filesystem_type=0x9123683E):
    """Complete synthetic schema example; no files, process or mmap are opened."""
    counter = 0

    def call(result, value, error=0):
        nonlocal counter
        entry = counter + 1
        counter += 2
        return dict(status='error' if error else 'ok', errno=error, ordinal=entry,
                    return_ordinal=counter, result=-1 if error else result,
                    value=None if error else value)

    records = []
    for i, p in enumerate(expected_pins(), 1):
        pin = {k: v for k, v in p.items() if k != 'role'}
        info = dict(mode=0o100600, size=p['size'], device_major=p['device_major'],
                    device_minor=p['device_minor'], inode=p['inode'],
                    mtime_ns=10000000, ctime_ns=20000000)
        # FD number and address are deliberately reused only after exact cleanup.
        start = 0x70000000
        line = (f'{start:x}-{start+4096:x} ---p 00000000 00:{maps_device:02x} '
                f'{p["inode"]} {p["path"]}\n')
        raw = line.encode()
        maps = dict(text=line, bytes=len(raw), lines=1,
                    sha256=hashlib.sha256(raw).hexdigest(), read_calls=2)
        filehash = dict(sha256=p['sha256'], bytes=p['size'], read_calls=2)
        record = dict(role=p['role'], pin=pin, fd_generation=i, mapping_generation=i)
        record['open'] = call(7, dict(flags=model.OPEN_FLAGS))
        record['fstat_before'] = call(0, copy.deepcopy(info))
        record['hash_before'] = call(p['size'], copy.deepcopy(filehash))
        record['fstatfs'] = call(0, dict(type=filesystem_type, fsid=[-123, 456]))
        record['mmap'] = call(start, dict(fd=7, fd_generation=i, mapping_generation=i,
                                         start=start, length=4096, offset=0, prot=0, flags=2))
        record['maps_before'] = call(len(raw), copy.deepcopy(maps))
        record['map_files'] = dict(
            path=f'/proc/1234/map_files/{start:x}-{start+4096:x}',
            readlink_buffer_bytes=4097,
            readlink_before=call(len(p['path']), p['path'], 0 if map_files else 1),
            stat=call(0, copy.deepcopy(info), 0 if map_files else 1),
            readlink_after=call(len(p['path']), p['path'], 0 if map_files else 1))
        record['maps_after'] = call(len(raw), copy.deepcopy(maps))
        record['fstat_after'] = call(0, copy.deepcopy(info))
        record['hash_after'] = call(p['size'], copy.deepcopy(filehash))
        record['munmap'] = call(0, dict(start=start, length=4096))
        record['close'] = call(0, dict(fd=7))
        records.append(record)
    return dict(schema_version=1, purpose='owned_exact_fd_mmap_backing_control',
                run_id=RUN_ID, host_pid=1234, start_cookie_before=5678,
                start_cookie_after=5678, kernel_release='owned-synthetic-kernel',
                page_size=4096, records=records)


def analyze(value=None, **kwargs):
    return model.analyze(snapshot() if value is None else value,
                         expected_pins=kwargs.get('pins', expected_pins()),
                         run_id=kwargs.get('run_id', RUN_ID))


def maps_payload(record, text, names=('maps_before', 'maps_after')):
    raw = text.encode()
    for name in names:
        record[name]['result'] = len(raw)
        record[name]['value'] = dict(text=text, bytes=len(raw), lines=len(text.splitlines()),
                                     sha256=hashlib.sha256(raw).hexdigest(), read_calls=2)


def renumber(value):
    last = 0
    for record in value['records']:
        calls = dict(record)
        calls.update(readlink_before=record['map_files']['readlink_before'],
                     map_files_stat=record['map_files']['stat'],
                     readlink_after=record['map_files']['readlink_after'])
        for name in model.CALLS:
            call = calls[name]
            if call['status'] == 'not_attempted':
                call.update(errno=0, ordinal=0, return_ordinal=0, result=None, value=None)
            else:
                call.update(ordinal=last+1, return_ordinal=last+2)
                last += 2
    return value


class ExactBackingTests(unittest.TestCase):
    def setUp(self):
        for name in ('run', 'Popen', 'call', 'check_call', 'check_output'):
            blocker = patch('subprocess.' + name, side_effect=AssertionError('Subprocess forbidden'))
            blocker.start()
            self.addCleanup(blocker.stop)

    def test_positive_keeps_distinct_device_domains(self):
        result = analyze()
        self.assertTrue(result['all_owned_fd_mmap_maps_joins_known'])
        self.assertTrue(result['all_map_files_backing_metadata_known'])
        self.assertEqual(result['transition_count'], 140)
        for row in result['records']:
            self.assertEqual(row['maps_device']['device_minor'], 36)
            self.assertEqual(row['FD_device']['device_minor'], 60)
            self.assertFalse(row['device_fields_equal'])
        self.assertTrue(all(result[k] is False for k in model.FALSE_FACTS))

    def test_strict_bounded_snapshot_transport(self):
        raw = (json.dumps(snapshot()) + '\n').encode()
        self.assertEqual(model.audit_bytes(raw, expected_pins=expected_pins(), run_id=RUN_ID),
                         analyze())
        variants = (raw[:-1], b'{"x":1,"x":1}\n', b'{"x":NaN}\n',
                    b'{"x":1.0}\n', b'{"x":111111111111111111111}\n', b'\xff\n')
        for value in variants:
            with self.subTest(value=value[:30]), self.assertRaises(ValueError):
                model.parse_snapshot(value)
        with patch.object(model, 'MAX_SNAPSHOT', len(raw) - 1), self.assertRaises(ValueError):
            model.parse_snapshot(raw)

    def test_permission_denial_retains_local_mmap_not_backing_identity(self):
        result = analyze(snapshot(map_files=False))
        self.assertTrue(result['all_owned_fd_mmap_maps_joins_known'])
        self.assertFalse(result['all_map_files_backing_metadata_known'])
        for row in result['records']:
            self.assertIn('map_files_stat:error', row['map_files_reasons'])
            self.assertFalse(result['remote_backing_identity_proven'])

    def test_equal_devices_and_nonbtrfs_do_not_infer_filesystem_cause(self):
        result = analyze(snapshot(maps_device=60, filesystem_type=0xEF53))
        self.assertTrue(result['all_owned_fd_mmap_maps_joins_known'])
        self.assertTrue(all(r['device_fields_equal'] for r in result['records']))
        self.assertEqual(result['records'][0]['filesystem']['type'], 0xEF53)
        self.assertFalse(result['actual_Btrfs_cause_proven'])

    def test_wrong_nonce_and_pin_role_swaps_rejected(self):
        with self.assertRaises(ValueError):
            analyze(run_id='f' * 32)
        value = snapshot()
        value['records'][0]['pin'], value['records'][1]['pin'] = (
            value['records'][1]['pin'], value['records'][0]['pin'])
        with self.assertRaises(ValueError):
            analyze(value)
        pins = expected_pins()
        pins[0], pins[1] = pins[1], pins[0]
        with self.assertRaises(ValueError):
            analyze(pins=pins)

    def test_same_hash_distinct_file_not_interchangeable(self):
        value = snapshot()
        record = value['records'][0]
        # Keep the intended hash, change only the actual observed file inode/path.
        for name in ('fstat_before', 'fstat_after'):
            record[name]['value']['inode'] += 1
        result = analyze(value)
        self.assertFalse(result['records'][0]['owned_fd_mmap_maps_join_known'])
        self.assertFalse(result['records'][0]['map_files_backing_metadata_known'])
        self.assertTrue(result['records'][1]['owned_fd_mmap_maps_join_known'])

    def test_wrong_original_mmap_arguments_unknown(self):
        changes = dict(fd=8, fd_generation=2, mapping_generation=2, length=8192,
                       offset=4096, prot=1, flags=1)
        for field, bad in changes.items():
            value = snapshot()
            value['records'][0]['mmap']['value'][field] = bad
            if field == 'length':
                value['records'][0]['munmap']['value']['length'] = bad
            with self.subTest(field=field):
                self.assertFalse(analyze(value)['records'][0]['owned_fd_mmap_maps_join_known'])

    def test_wrong_open_flags_unknown_no_guard_relaxation(self):
        for flags in (0, model.OPEN_FLAGS & ~0x20000, model.OPEN_FLAGS & ~0x800):
            value = snapshot()
            value['records'][0]['open']['value']['flags'] = flags
            self.assertFalse(analyze(value)['records'][0]['owned_fd_mmap_maps_join_known'])

    def test_wrong_range_offset_protection_path_inode_unknown(self):
        original = snapshot()['records'][0]['maps_before']['value']['text']
        variants = [original.replace('70000000-70001000', '6ffff000-70001000'),
                    original.replace('00000000', '00001000'),
                    original.replace('---p', 'r--p'), original.replace('1000 ', '1001 '),
                    original.replace('/owned/fixture.bin', '/other/fixture.bin'),
                    original.replace('/owned/fixture.bin', '/owned/fixture.bin (deleted)')]
        for text in variants:
            value = snapshot()
            maps_payload(value['records'][0], text)
            with self.subTest(text=text):
                self.assertFalse(analyze(value)['records'][0]['owned_fd_mmap_maps_join_known'])

    def test_map_files_exact_pid_and_range_required(self):
        for name in ('/proc/self/map_files/70000000-70001000',
                     '/proc/9999/map_files/70000000-70001000',
                     '/proc/1234/map_files/70001000-70002000', '/owned/fixture.bin'):
            value = snapshot()
            value['records'][0]['map_files']['path'] = name
            result = analyze(value)
            self.assertTrue(result['records'][0]['owned_fd_mmap_maps_join_known'])
            self.assertFalse(result['records'][0]['map_files_backing_metadata_known'])

    def test_link_target_renamed_deleted_or_replaced_unknown(self):
        for target in ('/other/fixture.bin', '/owned/fixture.bin (deleted)'):
            value = snapshot()
            call = value['records'][0]['map_files']['readlink_after']
            call.update(result=len(target), value=target)
            self.assertFalse(analyze(value)['records'][0]['map_files_backing_metadata_known'])
        for field, bad in (('device_minor', 36), ('inode', 1001), ('size', 9000),
                           ('mode', 0o120777)):
            value = snapshot()
            value['records'][0]['map_files']['stat']['value'][field] = bad
            self.assertFalse(analyze(value)['records'][0]['map_files_backing_metadata_known'])

    def test_maps_cookie_hash_and_timestamp_changes_unknown(self):
        for field in ('cookie', 'maps', 'hash', 'mtime', 'ctime'):
            value = snapshot()
            record = value['records'][0]
            if field == 'cookie':
                value['start_cookie_after'] += 1
            elif field == 'maps':
                maps_payload(record, record['maps_after']['value']['text'].replace('00:24', '00:25'),
                             names=('maps_after',))
            elif field == 'hash':
                record['hash_after']['value']['sha256'] = 'f' * 64
            else:
                record['fstat_after']['value'][field + '_ns'] += 1
            with self.subTest(field=field):
                self.assertFalse(analyze(value)['records'][0]['owned_fd_mmap_maps_join_known'])

    def test_unrelated_VMA_change_permitted_target_change_unknown(self):
        value = snapshot()
        record = value['records'][0]
        target = record['maps_before']['value']['text']
        maps_payload(record, target + '80000000-80001000 rw-p 0 00:00 0 [heap]\n',
                     names=('maps_after',))
        row = analyze(value)['records'][0]
        self.assertTrue(row['owned_fd_mmap_maps_join_known'])
        self.assertTrue(row['target_VMA_rows_identical'])
        self.assertFalse(row['maps_text_identical'])
        for replacement in ('70001000-70002000', '70000000-70002000'):
            maps_payload(record, target.replace('70000000-70001000', replacement),
                         names=('maps_after',))
            self.assertFalse(analyze(value)['records'][0]['owned_fd_mmap_maps_join_known'])

    def test_result_metadata_types_and_helper_integrity_rejected(self):
        changes = [('fstat_before', 'result', True), ('hash_before', 'result', 1),
                   ('maps_before', 'result', 1), ('open', 'errno', 1),
                   ('mmap', 'result', 123)]
        for name, key, bad in changes:
            value = snapshot()
            value['records'][0][name][key] = bad
            with self.subTest(name=name, key=key), self.assertRaises(ValueError):
                analyze(value)
        value = snapshot()
        value['records'][0]['maps_before']['value']['sha256'] = '0' * 64
        with self.assertRaises(ValueError):
            analyze(value)

    def test_generation_clock_and_cleanup_cannot_retag(self):
        for field in ('fd_generation', 'mapping_generation'):
            value = snapshot()
            value['records'][1][field] = 1
            with self.assertRaises(ValueError):
                analyze(value)
        for name in ('mmap', 'maps_before', 'munmap', 'close'):
            value = snapshot()
            value['records'][0][name]['return_ordinal'] += 1
            with self.assertRaises(ValueError):
                analyze(value)
        for field in ('fd',):
            value = snapshot()
            value['records'][0]['close']['value'][field] += 1
            with self.assertRaises(ValueError):
                analyze(value)

    def test_old_report_survives_address_FD_reuse_and_input_mutation(self):
        value = snapshot()
        result = analyze(value)
        saved = json.dumps(result, sort_keys=True)
        self.assertEqual({r['mmap']['result'] for r in value['records']}, {0x70000000})
        self.assertEqual({r['open']['result'] for r in value['records']}, {7})
        for record in value['records']:
            record['fstatfs']['value']['fsid'][0] += 1
            record['pin']['inode'] += 1
            record['maps_before']['value']['text'] = 'changed'
        self.assertEqual(json.dumps(result, sort_keys=True), saved)

    def test_observed_FD_device_is_not_filled_from_expected_pin(self):
        value = snapshot()
        value['records'][0]['fstat_before']['value']['device_minor'] = 99
        result = analyze(value)
        self.assertEqual(result['records'][0]['FD_device']['device_minor'], 99)
        self.assertEqual(result['records'][0]['pinned_device']['device_minor'], 60)
        self.assertFalse(result['records'][0]['owned_fd_mmap_maps_join_known'])

    def test_bounds_roles_lines_link_and_helpers(self):
        for mutate in ('sixth', 'four', 'page', 'hash', 'hashcalls', 'mapcalls', 'linkbuffer'):
            value = snapshot()
            if mutate == 'sixth':
                value['records'].append(copy.deepcopy(value['records'][0]))
            elif mutate == 'four':
                value['records'].pop()
            elif mutate == 'page':
                value['page_size'] = 4097
            elif mutate == 'hash':
                value['records'][0]['hash_after']['value']['bytes'] = model.MAX_FILE + 1
            elif mutate == 'hashcalls':
                value['records'][0]['hash_after']['value']['read_calls'] = 4098
            elif mutate == 'mapcalls':
                value['records'][0]['maps_after']['value']['read_calls'] = 514
            else:
                value['records'][0]['map_files']['readlink_buffer_bytes'] = 4096
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                analyze(value)
        value = snapshot()
        link = value['records'][0]['map_files']['readlink_before']
        link.update(value='x' * 4097, result=4097)
        with self.assertRaises(ValueError):
            analyze(value)
        with self.assertRaises(ValueError):
            model.parse_maps(''.join(f'{i*8192:x}-{i*8192+4096:x} ---p 0 00:00 0\n'
                                     for i in range(8193)))

    def test_pending_error_and_unattempted_envelopes_not_success(self):
        value = snapshot()
        for name in ('readlink_before', 'stat', 'readlink_after'):
            call = value['records'][0]['map_files'][name]
            call.update(status='error', errno=13, result=-1, value=None)
        self.assertFalse(analyze(value)['records'][0]['map_files_backing_metadata_known'])
        value['records'][0]['map_files']['stat']['result'] = 0
        with self.assertRaises(ValueError):
            analyze(value)
        value = snapshot()
        value['records'][-1]['munmap'].update(status='error', errno=22, result=-1, value=None)
        self.assertFalse(analyze(value)['all_cleanup_complete'])
        value['records'][-1]['munmap']['status'] = 'pending'
        with self.assertRaises(ValueError):
            analyze(value)

    def test_wellformed_failed_open_or_mapping_never_invents_resource(self):
        for failed in ('open', 'mmap'):
            value = snapshot()
            record = value['records'][0]
            record[failed].update(status='error', errno=13, result=-1, value=None)
            absent = (set(model.CALLS)-{'open'} if failed == 'open' else
                      {'maps_before', 'maps_after', 'readlink_before', 'map_files_stat',
                       'readlink_after', 'munmap'})
            for name in absent:
                call = record['map_files']['stat'] if name == 'map_files_stat' else (
                    record['map_files'][name] if name.startswith('readlink_') else record[name])
                call['status'] = 'not_attempted'
            record['map_files']['path'] = None
            renumber(value)
            result = analyze(value)
            self.assertFalse(result['records'][0]['owned_fd_mmap_maps_join_known'])
            self.assertFalse(result['records'][0]['map_files_backing_metadata_known'])
            self.assertIsNone(result['records'][0]['selected_VMA'])
            self.assertTrue(result['all_cleanup_complete'])
            self.assertTrue(result['records'][1]['owned_fd_mmap_maps_join_known'])

    def test_one_live_mapping_cannot_be_overlapped_by_later_record(self):
        value = snapshot()
        value['records'][0]['munmap'].update(status='error', errno=22, result=-1, value=None)
        with self.assertRaises(ValueError):
            analyze(value)

    def test_unattempted_map_files_metadata_stays_unknown(self):
        value = snapshot()
        for name in ('readlink_before', 'stat', 'readlink_after'):
            value['records'][0]['map_files'][name]['status'] = 'not_attempted'
        renumber(value)
        row = analyze(value)['records'][0]
        self.assertTrue(row['owned_fd_mmap_maps_join_known'])
        self.assertFalse(row['map_files_backing_metadata_known'])
        self.assertIn('map_files_stat:not_attempted', row['map_files_reasons'])


if __name__ == '__main__':
    unittest.main()
