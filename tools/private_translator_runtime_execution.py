"""Bounded supplied-file audit and injectable parent-only launch sequencing.

This module never launches a process.  File consistency is separate from the
frozen supplied-fact calculation, whose full report and false claims survive.
Trusted prelaunch paths select all disk reads; child paths are compared only.
"""
from __future__ import annotations

import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import struct
import sys
import time

# Parent HOST invokes Python -I -S -B. Only this frozen sibling directory is
# added; no working-directory/site/environment dependency lookup is used.
if str(Path(__file__).absolute().parent) not in sys.path:
    sys.path.insert(0, str(Path(__file__).absolute().parent))

from private_translator_runtime_validate import analyze

MAX_METADATA = 524288
MAX_NODES = 16384
MAX_DEPTH = 18
MAX_FILES = 32
MAX_LOG = 8 * 1024 * 1024
MAX_ARTIFACT = 64 * 1024 * 1024
MAX_ARTIFACT_TOTAL = 512 * 1024 * 1024
MAX_OUTPUT = 16 * 1024 * 1024
RAW_BYTES = 256 * 144 * 4
CASES = ('direct_A_clamp', 'after_bind_B_clamp')
ROLES = ('fixture', 'd3d12', 'd3d12core', 'dxgi')
EXPORTS = tuple('D4VR_TypedCopy' + s + 'V1' for s in
                ('Begin', 'Expect', 'Seal', 'ReceiptCopy', 'ReceiptRetain', 'ReceiptDrop'))
BASE_IID = '189819f1-1db6-4b57-be54-1821339b85f7'
ROOT_FILES = ('launch-plan.json', 'wrapper-return.json', 'stdout.log', 'stderr.log')
CHILD_FILES = ('envelope.json', 'original-events.json', 'child-execution.json',
               *(n + ext for n in CASES for ext in ('.bgra', '.constants.bin')))
CAPTURE_FILES = ROOT_FILES + tuple('output/' + n for n in CHILD_FILES)
_FALSE = ('input_facts_attested', 'loaded_memory_attested', 'source_memory_attested',
          'loaded_binary_source_equivalence_proven', 'current_descriptor_contents_known',
          'sampled_resource_GPU_read_proven', 'renderer_owner_known',
          'global_API_absence_proven', 'exclusive_phase_attribution_proven',
          'generic_PE_to_Unix_mapping_known', 'game_or_XR_result_proven')


def _need(condition, message):
    if not condition:
        raise ValueError(message)


def _keys(value, names, label='object'):
    _need(type(value) is dict and set(value) == set(names.split()), label + ' exact keys')


def _uint(value, maximum=(1 << 64) - 1, positive=False):
    _need(type(value) is int and (1 if positive else 0) <= value <= maximum, 'Bounded unsigned integer')
    return value


def _string(value, limit=2048):
    _need(type(value) is str and '\x00' not in value, 'Complete string')
    try:
        _need(len(value.encode('utf8', 'strict')) <= limit, 'UTF8 string byte bound')
    except UnicodeError as error:
        raise ValueError('Strict UTF8 string') from error
    return value


def _winpath(value):
    _string(value)
    _need(re.fullmatch(r'[A-Za-z]:[\\/].+',value) is not None, 'Complete explicit Windows drive path; no basename match')
    parts=re.split(r'[\\/]',value[3:])
    _need(all(part not in ('','.','..') for part in parts), 'No Windows relative/normalized path components')
    return value


def _sha(value):
    _need(type(value) is str and re.fullmatch('[0-9a-f]{64}', value) is not None, 'SHA256 lowercase hex')
    return value


def _hash(raw):
    return hashlib.sha256(raw).hexdigest()


def _pairs(rows):
    result = {}
    for k, v in rows:
        _need(k not in result, 'Duplicate JSON key')
        result[k] = v
    return result


def _json(raw, cap=MAX_METADATA):
    if type(raw) is dict:
        seen_nodes = 0
        string_bytes = 0
        def prewalk(value, depth=0):
            nonlocal seen_nodes, string_bytes
            seen_nodes += 1
            _need(seen_nodes <= MAX_NODES and depth <= MAX_DEPTH, 'JSON node/depth bound before serialization')
            if type(value) is dict:
                for key, item in value.items():
                    _string(key, cap); string_bytes += len(key.encode('utf8')); prewalk(item, depth + 1)
            elif type(value) is list:
                for item in value: prewalk(item, depth + 1)
            elif type(value) is str:
                _string(value, cap); string_bytes += len(value.encode('utf8'))
            elif type(value) is int:
                _need(value.bit_length() <= 65, 'Bounded metadata integer')
            elif type(value) is float:
                _need(math.isfinite(value), 'Finite metadata float')
            else:
                _need(value is None or type(value) is bool, 'JSON value')
            _need(string_bytes <= cap, 'JSON string bytes before serialization')
        prewalk(raw)
        try:
            raw = json.dumps(raw, ensure_ascii=True, allow_nan=False, separators=(',', ':')).encode('ascii')
        except (TypeError, ValueError, RecursionError) as error:
            raise ValueError('Bounded JSON object') from error
    if type(raw) is str:
        try:
            raw = raw.encode('utf8', 'strict')
        except UnicodeError as error:
            raise ValueError('Strict UTF8 JSON') from error
    _need(type(raw) is bytes and len(raw) <= cap, 'JSON byte bound')
    try:
        value = json.loads(raw.decode('utf8', 'strict'), object_pairs_hook=_pairs,
                           parse_constant=lambda _: _need(False, 'Nonfinite JSON'))
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise ValueError('Strict bounded UTF8 JSON') from error
    count = 0
    def visit(x, depth):
        nonlocal count
        count += 1
        _need(count <= MAX_NODES and depth <= MAX_DEPTH, 'JSON node/depth bound')
        if type(x) is dict:
            for k, v in x.items():
                _string(k, MAX_METADATA)
                visit(v, depth + 1)
        elif type(x) is list:
            for v in x:
                visit(v, depth + 1)
        elif type(x) is str:
            _string(x, MAX_METADATA)
        elif type(x) is float:
            _need(math.isfinite(x), 'Finite metadata number')
        else:
            _need(x is None or type(x) in (bool, int), 'JSON value')
    visit(value, 0)
    return value


def _encoded(value, cap=None):
    if cap is None:
        cap = MAX_OUTPUT
    total = 0
    chunks = []
    try:
        for piece in json.JSONEncoder(ensure_ascii=True, allow_nan=False, separators=(',', ':')).iterencode(value):
            total += len(piece)
            _need(total <= cap, 'Complete report byte budget; no truncation')
            chunks.append(piece)
    except (TypeError, UnicodeError, RecursionError) as error:
        raise ValueError('Bounded complete output JSON') from error
    return ''.join(chunks).encode('ascii')


def _path(value):
    value = os.fspath(value)
    _string(value, 8192)
    p = Path(value)
    _need(p.is_absolute() and '..' not in p.parts and '.' not in p.parts, 'Canonical absolute path; no traversal')
    _need(str(p) == value, 'Canonical lexical path; no silent alias normalization')
    return p


def _windows_handle(path, directory=False, create=False):
    # Windows CI uses native no-reparse handles, never a lstat/open fallback.
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
    access = 0x40000000 if create else 0x80000000
    handle = kernel.CreateFileW(str(path), access, 1, None, 1 if create else 3,
                                0x00200000 | (0x02000000 if directory else 0), None)
    _need(handle != ctypes.c_void_p(-1).value, 'No-follow Win32 file open')
    class INFO(ctypes.Structure):
        _fields_ = [('attributes', wintypes.DWORD), ('creation', wintypes.FILETIME),
                    ('access', wintypes.FILETIME), ('write', wintypes.FILETIME),
                    ('volume', wintypes.DWORD), ('size_hi', wintypes.DWORD), ('size_lo', wintypes.DWORD),
                    ('links', wintypes.DWORD), ('index_hi', wintypes.DWORD), ('index_lo', wintypes.DWORD)]
    kernel.GetFileInformationByHandle.argtypes = (wintypes.HANDLE, ctypes.POINTER(INFO))
    kernel.GetFileType.argtypes = (wintypes.HANDLE,)
    try:
        info = INFO()
        _need(kernel.GetFileInformationByHandle(handle, ctypes.byref(info)) != 0, 'Win32 handle metadata')
        _need(not info.attributes & 0x400 and bool(info.attributes & 0x10) == directory,
              'No reparse points; regular file or directory')
        _need(kernel.GetFileType(handle) == 1, 'Disk file handle')
        if not directory:
            _need(info.links == 1, 'Single-link regular file')
        return handle, kernel
    except BaseException:
        kernel.CloseHandle(handle)
        raise


def _open_parent(path):
    """Hold the admitted directory chain until the final file open finishes."""
    if os.name == 'nt':
        handles = []
        try:
            for ancestor in reversed((path.parent, *path.parent.parents)):
                handles.append(_windows_handle(ancestor, directory=True))
            return handles
        except BaseException:
            for handle, kernel in reversed(handles):
                kernel.CloseHandle(handle)
            raise
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | getattr(os, 'O_CLOEXEC', 0)
    fd = os.open(path.anchor, flags)
    try:
        for part in path.parent.parts[1:]:
            child = os.open(part, flags, dir_fd=fd)
            os.close(fd)
            fd = child
        return fd
    except BaseException:
        os.close(fd)
        raise


def _close_parent(parent):
    if os.name == 'nt':
        for handle, kernel in reversed(parent):
            kernel.CloseHandle(handle)
    else:
        os.close(parent)


def read_regular(path, limit):
    """No-follow ordinary disk buffer reads only; never mapped payload/proc reads."""
    path = _path(path)
    parent = _open_parent(path)
    fd = None
    try:
        if os.name == 'nt':
            import msvcrt
            handle, _ = _windows_handle(path)
            fd = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
        else:
            fd = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK |
                         getattr(os, 'O_CLOEXEC', 0), dir_fd=parent)
        before = os.fstat(fd)
        _need(stat.S_ISREG(before.st_mode) and before.st_nlink == 1 and before.st_size <= limit,
              'Single-link regular bounded file')
        chunks = []
        size = 0
        while True:
            part = os.read(fd, min(65536, limit + 1 - size))
            if not part:
                break
            size += len(part)
            _need(size <= limit, 'Read growth exceeds bound')
            chunks.append(part)
        after = os.fstat(fd)
        _need((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) ==
              (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns), 'File changed during read')
        _need(size == before.st_size, 'Complete ordinary file read')
        return b''.join(chunks)
    except OSError as error:
        raise ValueError('No-follow bounded ordinary file read') from error
    finally:
        if fd is not None:
            os.close(fd)
        _close_parent(parent)


def _save_new(path, value):
    _write_new_bytes(path, _encoded(value, MAX_METADATA))


def _write_new_bytes(path, raw):
    _need(type(raw) is bytes and len(raw) <= MAX_OUTPUT, 'Bounded create-only ordinary bytes')
    path = _path(path)
    parent = _open_parent(path)
    fd = None
    try:
        if os.name == 'nt':
            import msvcrt
            handle, _ = _windows_handle(path, create=True)
            fd = msvcrt.open_osfhandle(handle, os.O_WRONLY | os.O_BINARY)
        else:
            fd = os.open(path.name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW |
                         getattr(os, 'O_CLOEXEC', 0), 0o600, dir_fd=parent)
        _need(stat.S_ISREG(os.fstat(fd).st_mode), 'Create-only regular output')
        offset = 0
        while offset < len(raw):
            written = os.write(fd, raw[offset:])
            _need(written > 0, 'Complete output write')
            offset += written
        os.fsync(fd)
        if os.name != 'nt':
            os.fsync(parent)
    except OSError as error:
        raise ValueError('Create-only output; never overwrite') from error
    finally:
        if fd is not None:
            os.close(fd)
        _close_parent(parent)


def _plan(value):
    p = _json(value)
    _keys(p, 'schema_version purpose run_id mode facts_origin directory output_windows_path command cwd environment artifacts modules shader_paths preconditions', 'Frozen launch plan')
    _need(p['schema_version'] == 1 and type(p['schema_version']) is int and p['purpose'] == 'private_translator_runtime_frozen_launch', 'Launch purpose/schema')
    _need(type(p['run_id']) is str and re.fullmatch('[0-9a-f]{32}', p['run_id']) is not None, 'Run nonce128')
    _need(p['mode'] in ('off', 'on') and p['facts_origin'] in ('CPU_test', 'parent_owned_execution'), 'Declared mode/fact scope')
    _path(p['directory']); _path(p['cwd']); _winpath(p['output_windows_path'])
    _keys(p['shader_paths'], 'vertex fragment', 'Explicit shader Windows paths')
    for v in p['shader_paths'].values(): _winpath(v)
    _need(type(p['command']) is list and 1 <= len(p['command']) <= 64, 'Bounded exact argv')
    for v in p['command']: _string(v, 8192)
    _need(type(p['environment']) is dict and len(p['environment']) <= 128, 'Bounded exact environment')
    for k, v in p['environment'].items():
        _string(k, 256); _string(v, 16384); _need(k and '=' not in k, 'Environment name')
    _keys(p['preconditions'], 'CPU_review_complete source_freeze_sha256 parent_authorized fresh_prefix no_game_or_XR', 'Explicit preconditions')
    for k, v in p['preconditions'].items():
        if k == 'source_freeze_sha256': _sha(v)
        else: _need(v is True, 'All declared preconditions required')
    _need(type(p['artifacts']) is list and 9 <= len(p['artifacts']) <= 32, 'Artifact count')
    artifacts = {}; paths = set(); total = 0
    for row in p['artifacts']:
        _keys(row, 'role path bytes sha256', 'Artifact pin')
        role = _string(row['role'], 128); _path(row['path']); _sha(row['sha256']); _uint(row['bytes'], MAX_ARTIFACT, True)
        _need(role not in artifacts and row['path'] not in paths, 'Unique explicit artifact role/path')
        total += row['bytes']; artifacts[role] = row; paths.add(row['path'])
    _need(total <= MAX_ARTIFACT_TOTAL and set(('fixture', 'vertex', 'fragment', 'd3d12', 'd3d12core', 'dxgi', 'oracle_A', 'oracle_B', 'launcher')) <= set(artifacts), 'Required artifact pins/total bytes')
    if p['facts_origin']=='parent_owned_execution':
        _need(set(('reader','runtime_analyzer','runner','source_freeze'))<=set(artifacts), 'Actual parent execution requires prelaunch code/freeze artifact pins')
        _need(artifacts['source_freeze']['sha256']==p['preconditions']['source_freeze_sha256'], 'Source freeze exact ordinary byte SHA, no self/plan cycle')
        _need(artifacts['runtime_analyzer']['sha256']=='94bf3f32cf1ec0a41dc2f8abdf7a9e842f7334f10204284efabd77e206f7a19a', 'Exact frozen supplied-fact analyzer revision')
    _need(p['command'][0] == artifacts['launcher']['path'], 'Explicit pinned launcher argv0')
    _need(type(p['modules']) is list and len(p['modules']) == 4, 'Four finite module mappings')
    modules = {}
    for row in p['modules']:
        _keys(row, 'role windows_path artifact_role', 'Explicit module path mapping')
        _need(type(row['role']) is str and row['role'] in ROLES and row['role'] not in modules, 'Unique module role')
        _need(row['artifact_role'] == row['role'], 'Module pin exact role'); _winpath(row['windows_path'])
        modules[row['role']] = row
    _need(len({m['windows_path'] for m in modules.values()}) == 4, 'Distinct complete module paths')
    expected_tail = [modules['fixture']['windows_path'], '--vertex', p['shader_paths']['vertex'], '--fragment', p['shader_paths']['fragment'], '--output', p['output_windows_path'], '--run-id', p['run_id'], '--transport', p['mode'], '--core', modules['d3d12core']['windows_path']]
    _need(p['command'][-13:] == expected_tail, 'Exact original PE argv/profile')
    return p


def _artifact_bytes(plan):
    result = {}
    for row in plan['artifacts']:
        raw = read_regular(row['path'], MAX_ARTIFACT)
        _need(len(raw) == row['bytes'] and _hash(raw) == row['sha256'], 'Pinned external artifact bytes')
        result[row['role']] = raw
    return result


def _file_limit(name):
    if name.endswith('.bgra'): return RAW_BYTES
    if name.endswith('.constants.bin'): return 8
    if name.endswith('.log'): return MAX_LOG
    return MAX_METADATA


def _inventory(directory):
    directory = _path(directory)
    root = _open_parent(directory / '_inventory')
    found = []
    def collect(entries, prefix=''):
        for entry in entries:
            _need(len(found) < MAX_FILES, 'Capture file count bound')
            _need(entry.is_file(follow_symlinks=False), 'Regular fixed capture files only')
            found.append(prefix + entry.name)
    try:
        # Linux enumeration is relative to the SAME admitted directory FD.
        # Windows ancestors remain locked against WRITE/DELETE until enumeration
        # and the child open complete; no absolute-path unlocked gap exists.
        with os.scandir(directory if os.name == 'nt' else root) as entries:
            for entry in entries:
                _need(len(found) < MAX_FILES, 'Capture file count bound')
                if entry.name == 'output':
                    if os.name == 'nt':
                        child = _open_parent(directory / 'output' / '_inventory')
                        try:
                            with os.scandir(directory / 'output') as children: collect(children, 'output/')
                        finally: _close_parent(child)
                    else:
                        child = os.open('output', os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC, dir_fd=root)
                        try:
                            with os.scandir(child) as children: collect(children, 'output/')
                        finally: os.close(child)
                else:
                    _need(entry.is_file(follow_symlinks=False), 'No capture symlink/FIFO/extra directory')
                    found.append(entry.name)
    finally:
        _close_parent(root)
    _need(set(found) == set(CAPTURE_FILES) and len(found) == len(CAPTURE_FILES), 'Exact fixed capture files, no replacement/extra files')
    return found


def _integer_reference(alternate):
    # Closed integer reduction of the frozen owned shader at pixel centers.
    rows = []
    for y in range(144):
        t = max(0, min(7, y // 18 - 1))
        row = bytearray()
        for x in range(256):
            s = min(7, x // 32 + 1)
            row.extend((197 - 5*s - 9*t, 211 - 17*t, 233 - 11*s, 83 + 7*s + 11*t) if alternate else
                       (41 + 7*s + 11*t, 29 + 19*t, 17 + 13*s, 255))
        rows.append(bytes(row))
    return b''.join(rows)


def _validation(logs):
    raw = b'\n'.join(logs)
    # Ordinary runtime diagnostics may contain arbitrary locale bytes. Keep the
    # original bytes/hash; latin1 is a bijection for ASCII diagnostic matching.
    text = raw.decode('latin1')
    sentinel = 'VUID-VkDebugUtilsMessengerCallbackDataEXT-pNext-pNext'
    marker = 'Validation layers seem to be loaded correctly.'
    vuids = re.findall(r'VUID-[A-Za-z0-9_\-]+', text)
    # The pinned vkd3d callback consumes its deliberate sentinel BEFORE logging.
    # An arbitrary logged Validation Error containing that spelling is NOT the
    # selftest and never receives a line-wide exemption.
    bad_lines = [line for line in text.splitlines() if
                 'Validation Error:' in line or
                 ('vkd3d_debug_messenger_callback:' in line and 'err:' in line.lower()) or
                 'Validation layers do not seem to be loaded correctly.' in line]
    pattern = re.compile(r'^[0-9]+(?:\.[0-9]+)?:[0-9a-fA-F]+:[0-9a-fA-F]+:info:vkd3d-proton:vkd3d_init_debug_messenger_callback: Validation layers seem to be loaded correctly\.$', re.MULTILINE)
    markers = len(pattern.findall(text))
    _need(markers > 0 and not vuids and not bad_lines, 'Canonical validation activation marker and no logged VUID/error diagnostics')
    return dict(activation_markers=markers, sentinel_VUID_occurrences=0,
                deliberate_selftest_consumed_by_callback_reported=True,
                non_sentinel_VUIDs=[], non_sentinel_errors=len(bad_lines),
                warning_lines=sum('warn' in line.lower() for line in text.splitlines()),
                universal_error_absence_proven=False)


def _same(value, expected):
    # Numeric equality never lets a bool masquerade as an original number.
    if type(expected) is dict:
        return type(value) is dict and set(value)==set(expected) and all(_same(value[k],v) for k,v in expected.items())
    if type(expected) is list:
        return type(value) is list and len(value)==len(expected) and all(_same(a,b) for a,b in zip(value,expected))
    return type(value) is type(expected) and value==expected


def _callref(row, value):
    _keys(value, 'ordinal entry return', 'Original CallRef')
    for k in value: _uint(value[k], positive=True)
    return row['ordinal'] == value['ordinal'] and row['transition'] == value['return']


def _child(child, envelope, journal, files, plan, artifacts):
    _keys(child, 'schema_version purpose run_id transport windows_pid owned_fixture game_rendered width height target_DXGI_format texture_DXGI_format features UV raster shaders cases module_snapshots control_exports device_create envelope_file envelope_sha256 journal_file journal_sha256 metadata_ok', 'Child execution')
    _need(type(child['schema_version']) is int and child['schema_version'] == 1 and child['purpose'] == 'owned_private_translator_runtime_execution', 'New child execution purpose')
    _need(child['run_id'] == plan['run_id'] == envelope['run_id'] and child['transport'] == plan['mode'] == envelope['mode'], 'Same run nonce/mode')
    _need(envelope['facts_origin']==('supplied_owned_PE_original' if plan['facts_origin']=='parent_owned_execution' else 'supplied_CPU_mock'), 'Explicit declared file origin domain')
    _uint(child['windows_pid'], (1 << 32)-1, True)
    _need(child['owned_fixture'] is True and child['game_rendered'] is False and child['metadata_ok'] is True, 'Owned fixture complete metadata')
    for k,v in dict(width=256,height=144,target_DXGI_format=87,texture_DXGI_format=28).items():
        _need(type(child[k]) is int and child[k] == v, 'Exact finite image profile')
    _need(_same(child['UV'], dict(source='SV_Position.xy',divisor=[256,144],offset=[.125,-.125],flip_y=False,LOD=0)), 'Frozen UV declarations')
    _need(_same(child['raster'], dict(viewport=[0,0,256,144,0,1],scissor=[0,0,256,144],blend_enable=False,logic_op_enable=False,color_write_mask=15,sample_count=1)), 'Frozen raster declarations')
    f = child['features']; _keys(f, 'shader_model_requested shader_model_supported format_support', 'Actual feature results')
    _need(f['shader_model_requested'] == 96 and type(f['shader_model_requested']) is int, 'SM6 requested')
    _need(_uint(f['shader_model_supported'], (1<<32)-1) >= 96, 'SM6 support')
    _need(type(f['format_support']) is list and len(f['format_support']) == 2, 'Two format support queries')
    for row, fmt, required in zip(f['format_support'], (28,87), (0x220,0x4020)):
        _keys(row, 'DXGI_format support1 support2 required_support1', 'Format support')
        for v in row.values(): _uint(v, (1<<32)-1)
        _need(row['DXGI_format']==fmt and row['required_support1']==required and row['support1'] & required == required, 'Exact feature mask supported')
    _need(child['envelope_file']=='envelope.json' and child['journal_file']=='original-events.json', 'Fixed child filenames')
    _need(child['envelope_sha256']==_hash(files['output/envelope.json']) and child['journal_sha256']==_hash(files['output/original-events.json']), 'Child exact ordinary file digests')
    _keys(journal, 'schema_version purpose run_id clock_domain events', 'Complete original journal')
    _need(journal['schema_version']==1 and type(journal['schema_version']) is int and journal['purpose']=='owned_private_translator_APP_original_and_control_journal' and journal['run_id']==plan['run_id'] and journal['clock_domain']=='independent_APP_original_transition', 'Original journal framing')
    _need(type(journal['events']) is list and len(journal['events']) <= 512, 'Original edge bound')
    transitions = set(); by_ordinal = {}; previous = 0; last_entry_ordinal = 0
    for row in journal['events']:
        _keys(row, 'ordinal transition api edge case_ordinal return_kind args original_returned result', 'Original edge')
        _uint(row['ordinal'], positive=True); _uint(row['transition'], positive=True); _uint(row['case_ordinal'],2)
        _string(row['api'],128); _string(row['return_kind'],64)
        aux_returns={'GetDesc':'RESOURCE_DESC','GetBufferPointer':'pointer','GetBufferSize':'uint64',
                     'D3D12SerializeRootSignature':'HRESULT','CreateRootSignature':'HRESULT','CreateGraphicsPipelineState':'HRESULT','CopyTextureRegion':'void'}
        if row['api'] in aux_returns:
            _need(row['return_kind']==aux_returns[row['api']], 'Exact finite auxiliary original return ABI')
        _need(row['transition']>previous and row['transition'] not in transitions and row['edge'] in ('before','after'), 'Original ordered edge clock')
        previous=row['transition']; transitions.add(previous)
        if row['edge']=='before':
            _need(row['ordinal']>last_entry_ordinal, 'Original ordinal increases at ENTRY, independently of RETURN order')
            last_entry_ordinal=row['ordinal']
        group=by_ordinal.setdefault(row['ordinal'],[]); group.append(row)
    _need(len(by_ordinal)<=256, 'Original call bound')
    for group in by_ordinal.values():
        _need(len(group)==2 and group[0]['edge']=='before' and group[1]['edge']=='after', 'Every original call returned once')
        b,a=group
        _need(b['original_returned'] is False and b['result'] is None and a['original_returned'] is True, 'Original BEFORE/AFTER return facts')
        for k in ('ordinal','api','case_ordinal','return_kind','args'): _need(_same(b[k],a[k]), 'Original args frozen at ENTRY with exact JSON types')
        if a['return_kind']=='void': _need(a['result'] is None, 'Void original no HRESULT')
        elif a['return_kind']=='HRESULT':
            _keys(a['result'],'hresult output','Captured original HRESULT')
            _need(type(a['result']['hresult']) is int and 0<=a['result']['hresult']<1<<31, 'All captured HRESULT calls succeeded')
        elif a['return_kind'] in ('uint32','uint64','CPU_HANDLE','GPU_HANDLE','pointer'):
            _keys(a['result'],'value','Captured original scalar return')
            _uint(a['result']['value'],(1<<(32 if a['return_kind']=='uint32' else 64))-1)
        elif a['return_kind']=='RESOURCE_DESC':
            _keys(a['result'],'description','Original GetDesc value')
            _keys(a['result']['description'],'dimension width height depth_or_array_size mip_levels format sample_count flags','Original finite ResourceDesc projection')
            for field in a['result']['description'].values(): _uint(field)
        else:
            _need(a['return_kind']=='control_status_u32' and a['api'].startswith('D4VR_TypedCopy'),'Known finite original return domain')
    # Exact finite blob-to-root relation from ORIGINAL returned values. This
    # compares no hidden ABI pointer across SOURCE/APP domains and reads no blob.
    for group in by_ordinal.values():
        before,after=group
        if before['api']!='CreateRootSignature': continue
        args=before['args']
        _need(type(args) is dict and set(('blob','blob_data_pointer','blob_bytes'))<=set(args), 'Root signature original blob arguments')
        _keys(args['blob'],'raw generation','Original blob reference')
        for value in args['blob'].values(): _uint(value,positive=True)
        pointer=_uint(args['blob_data_pointer'],positive=True); size=_uint(args['blob_bytes'],positive=True)
        _need(pointer+size<=(1<<64)-1,'Original serialized blob span no U64 overflow')
        for api,key in (('GetBufferPointer','blob_data_pointer'),('GetBufferSize','blob_bytes')):
            witnesses=[a for b,a in by_ordinal.values() if b['api']==api and
                       _same(b['args'].get('blob'),args['blob']) and a['transition']<before['transition']]
            _need(len(witnesses)==1 and type(witnesses[0]['result']) is dict and
                  type(witnesses[0]['result'].get('value')) is int and witnesses[0]['result']['value']==args[key],
                  'Root blob argument exact unique earlier original getter return')
        _need(not any(b['api']=='Release' and _same(b['args'].get('object'),args['blob']) and
                      b['transition']<before['transition'] for b,a in by_ordinal.values()),
              'Original blob not released before root signature use')
    projection=envelope['app_journal']['events']
    actual={r['transition']:r for r in journal['events']}
    _need(all(_same(actual.get(r['transition']),r) for r in projection), 'Pure envelope edges exact subset of original full journal')
    for row in journal['events']:
        if row['api'] in ('CreateShaderResourceView','CopyDescriptorsSimple','CopyDescriptors') or row['api'].startswith('D4VR_TypedCopy'):
            _need(any(_same(row,p) for p in projection), 'All selected typed/control occurrences retained')
    d=child['device_create']; _keys(d, 'call adapter minimum_feature_level requested_iid output_pointer hresult defined_interface', 'Actual public device creation')
    _uint(d['adapter']); _uint(d['output_pointer'],(1<<64)-8,True); _uint(d['defined_interface'],positive=True)
    _need(d['adapter']==0 and d['minimum_feature_level']==45056 and type(d['minimum_feature_level']) is int and d['requested_iid']==BASE_IID and type(d['hresult']) is int and d['hresult']==0, 'Source reviewed base IID direct S_OK route')
    matches=[r for r in projection if r['edge']=='after' and r['api']=='D3D12CreateDevice' and _callref(r,d['call'])]
    _need(len(matches)==1 and by_ordinal[d['call']['ordinal']][0]['transition']==d['call']['entry'] and matches[0]['result']['hresult']==d['hresult'] and matches[0]['result']['output']['raw']==d['defined_interface'], 'Exact actual public result/CallRef (source NEW result separate)')
    _need(type(child['shaders']) is list and len(child['shaders'])==2, 'Two unchanged shader snapshots')
    artifact_rows={r['role']:r for r in plan['artifacts']}
    for row,role,stage,entry in zip(child['shaders'],('vertex','fragment'),('vertex','pixel'),('VSMain','PSMain')):
        _keys(row,'stage path entry sha256 bytes','Shader snapshot')
        _need(_same(row,dict(stage=stage,path=plan['shader_paths'][role],entry=entry,sha256=artifact_rows[role]['sha256'],bytes=artifact_rows[role]['bytes'])), 'Exact pinned original shader snapshot')
    _need(type(child['module_snapshots']) is list and len(child['module_snapshots'])==4, 'Four own-process module checkpoints')
    mappings={r['role']:r for r in plan['modules']}; module_report=[]
    for snap,point in zip(child['module_snapshots'],('before_device','before_cases','after_cases','after_release')):
        _keys(snap,'point modules','Module checkpoint'); _need(snap['point']==point, 'Exact module checkpoint ordering')
        _need(type(snap['modules']) is list and 4<=len(snap['modules'])<=6, 'Finite named module observations')
        seen=set()
        for row in snap['modules']:
            _keys(row,'role loaded path','Own PE module observation')
            _need(type(row['role']) is str and row['role'] in (*ROLES,'winevulkan','vulkan-1') and row['role'] not in seen, 'Distinct finite observed module role'); seen.add(row['role'])
            _need(type(row['loaded']) is bool,'Actual module loaded BOOL')
            if row['loaded']: _winpath(row['path'])
            else: _need(row['path'] is None,'Unavailable module path null')
            if row['role'] in ROLES:
                _need(row['loaded'] is True and row['path']==mappings[row['role']]['windows_path'], 'Explicit exact own-child module path mapping')
        _need(set(ROLES)<=seen,'Required module observations')
        module_report.append(snap)
    _need(type(child['control_exports']) is list and len(child['control_exports'])==6, 'Six original symbol lookups')
    addresses=[]
    for row,name in zip(child['control_exports'],EXPORTS):
        _keys(row,'name address lookup_error module_from_address','Control PFN observation')
        _need(row['name']==name,'Exact export name'); _uint(row['address']); _uint(row['lookup_error'],(1<<32)-1)
        if plan['mode']=='off':
            _need(row['address']==0 and row['module_from_address'] is None,'OFF absent PFN; no FROM_ADDRESS call fabricated')
        else:
            _need(row['address']>0,'ON export exists'); addresses.append(row['address'])
            m=row['module_from_address']; _keys(m,'returned handle path last_error','PFN original module API result')
            _need(m['returned'] is True,'FROM_ADDRESS succeeded'); _uint(m['handle'],positive=True); _uint(m['last_error'],(1<<32)-1)
            _need(m['path']==mappings['d3d12core']['windows_path'],'PFN owned by exact planned core path')
    if plan['mode']=='on':
        _need(len(set(addresses))==6,'Distinct control entry points')
        _need(envelope['control_binding']['exports']==[dict(name=r['name'],address=r['address']) for r in child['control_exports']], 'Exact exported addresses used by control binding')
    _need(type(child['cases']) is list and len(child['cases'])==2,'Two actual case outputs')
    result=[]
    for i,(row,name,context) in enumerate(zip(child['cases'],CASES,envelope['cases'])):
        _keys(row,'name case_ordinal texture_index sampler_index method raw_file bytes sha256 constants_file constants_sha256 producer_fence_value producer_completed','Actual packed output')
        raw=files['output/'+name+'.bgra']; constants=files['output/'+name+'.constants.bin']
        _need(row['name']==name and type(row['case_ordinal']) is int and row['case_ordinal']==i+1 and type(row['texture_index']) is int and row['texture_index']==i and type(row['sampler_index']) is int and row['sampler_index']==0 and row['method']==('direct_create','copy_after_bind')[i], 'Exact case method/resource/sampler')
        _need(row['raw_file']==name+'.bgra' and type(row['bytes']) is int and row['bytes']==RAW_BYTES and len(raw)==RAW_BYTES and row['sha256']==_hash(raw),'Actual packed raw bytes/digest')
        _need(row['constants_file']==name+'.constants.bin' and len(constants)==8 and constants==struct.pack('<ff',.125,-.125) and row['constants_sha256']==_hash(constants),'Exact original constants bytes')
        _need(raw==artifacts[('oracle_A','oracle_B')[i]]==_integer_reference(bool(i)), 'Every BGRA byte equals frozen integer reference')
        _uint(row['producer_fence_value'],positive=True); _need(row['producer_completed'] is True,'Producer fence consumed')
        after=next(r for r in projection if r['edge']=='after' and r['ordinal']==context['signal_call']['ordinal'])
        _need(after['args']['value']==row['producer_fence_value'] and context['readback']['sha256']==row['sha256'] and context['readback']['relative_file']==row['raw_file'], 'Same original Signal/readback metadata join')
        result.append(dict(name=name,bytes=RAW_BYTES,sha256=_hash(raw),all_BGRA_bytes_equal_integer_reference=True))
    return dict(cases=result,module_snapshots=module_report,control_exports=child['control_exports'],
                windows_pid=child['windows_pid'],windows_pid_mapped_to_host_PID=False,
                full_original_journal=journal,device_create=d)


def audit_run(directory, pins):
    """Read only the fixed capture and externally pinned ordinary artifacts."""
    pins=_json(pins); _keys(pins,'schema_version purpose plan_sha256 capture_files','External post-run pins')
    _need(pins['schema_version']==1 and type(pins['schema_version']) is int and pins['purpose']=='private_translator_runtime_execution_pins','External pins framing'); _sha(pins['plan_sha256'])
    directory=_path(directory); names=_inventory(directory)
    _need(type(pins['capture_files']) is list and len(pins['capture_files'])==len(names),'Complete capture pins')
    file_pins={}
    for row in pins['capture_files']:
        _keys(row,'name bytes sha256','Post-run capture file pin')
        _need(type(row['name']) is str and row['name'] in CAPTURE_FILES and row['name'] not in file_pins,'Unique permitted capture filename')
        _uint(row['bytes'],_file_limit(row['name'])); _sha(row['sha256']); file_pins[row['name']]=row
    files={name:read_regular(str(directory/name),_file_limit(name)) for name in names}
    for name,raw in files.items(): _need(len(raw)==file_pins[name]['bytes'] and _hash(raw)==file_pins[name]['sha256'],'Exact externally pinned capture bytes')
    _need(_hash(files['launch-plan.json'])==pins['plan_sha256'],'Exact prelaunch plan SHA')
    plan=_plan(files['launch-plan.json']); _need(plan['directory']==str(directory),'Exact planned capture directory')
    artifacts=_artifact_bytes(plan)
    returned=_json(files['wrapper-return.json']); _keys(returned,'schema_version purpose run_id mode spawn_succeeded returncode host_seconds stdout_lost_bytes stderr_lost_bytes executor_error','Actual wrapper return')
    _need(type(returned['schema_version']) is int and returned['schema_version']==1 and returned['purpose']=='parent_wrapper_actual_return_before_postprobe' and returned['run_id']==plan['run_id'] and returned['mode']==plan['mode'],'Actual return nonce/mode/purpose')
    _need(returned['spawn_succeeded'] is True and type(returned['returncode']) is int and returned['returncode']==0 and returned['executor_error'] is None,'Original wrapper exited zero, no automatic retry')
    _need(type(returned['host_seconds']) in (float,int) and math.isfinite(returned['host_seconds']) and returned['host_seconds']>=0,'Host seconds actual timer')
    _need(type(returned['stdout_lost_bytes']) is int and type(returned['stderr_lost_bytes']) is int and returned['stdout_lost_bytes']==returned['stderr_lost_bytes']==0,'Full logs; no dropped bytes certified')
    envelope=_json(files['output/envelope.json']); child=_json(files['output/child-execution.json']); journal=_json(files['output/original-events.json'])
    # Exactly one frozen supplied-fact audit per run; preserve every field.
    supplied=analyze(files['output/envelope.json'])
    observed=_child(child,envelope,journal,files,plan,artifacts)
    validation=_validation([files['stdout.log'],files['stderr.log']])
    # Rehash all admitted paths after the whole calculation, not just after each read.
    for name,raw in files.items(): _need(read_regular(str(directory/name),_file_limit(name))==raw,'Capture unchanged across complete audit')
    for row in plan['artifacts']: _need(_hash(read_regular(row['path'],MAX_ARTIFACT))==row['sha256'],'External artifacts unchanged across complete audit')
    _need(set(_inventory(directory))==set(names),'Capture inventory unchanged')
    result=dict(schema_version=1,purpose='supplied_private_translator_execution_file_audit',
                run_id=plan['run_id'],mode=plan['mode'],facts_origin=plan['facts_origin'],
                supplied_files_consistent=True,source_relation_observed=supplied['supplied_outer_copy_and_APP_argument_relation_known'],
                milestone_objective_met=plan['mode']=='on' and supplied['supplied_outer_copy_and_APP_argument_relation_known'] is True,execution_declared=plan['facts_origin']=='parent_owned_execution',
                supplied_relation_audit=supplied,observations=observed,validation=validation,
                wrapper_return=returned,prelaunch_plan_sha256=pins['plan_sha256'],
                post_exit_capture_pins=pins['capture_files'],external_artifact_pins=plan['artifacts'],
                claims={key:False for key in _FALSE})
    _encoded(result)
    return result


def audit_pair(root, pins):
    pins=_json(pins); _keys(pins,'schema_version purpose runs','Pair pins')
    _need(type(pins['schema_version']) is int and pins['schema_version']==1 and pins['purpose']=='private_translator_runtime_pair_pins','Pair purpose'); _keys(pins['runs'],'off on','Pair modes')
    root=_path(root)
    reports={m:audit_run(str(root/m),pins['runs'][m]) for m in ('off','on')}
    off,on=reports['off'],reports['on']
    _need(off['mode']=='off' and on['mode']=='on' and off['run_id']!=on['run_id'],'Distinct OFF/ON nonces')
    artifacts=[{r['role']:(r['bytes'],r['sha256']) for r in reports[m]['external_artifact_pins']} for m in ('off','on')]
    _need(set(artifacts[0])==set(artifacts[1]), 'Same finite external artifact roles')
    for role in artifacts[0]:
        if role != 'd3d12core':
            _need(artifacts[0][role]==artifacts[1][role],'Same original workload/shaders/oracles/runtime, only selected core differs')
    _need(_same(off['observations']['cases'],on['observations']['cases']),'Every corresponding OFF/ON packed byte exact via same closed reference')
    _need(off['supplied_relation_audit']['supplied_outer_copy_and_APP_argument_relation_known'] is False,'OFF no source relation fabricated')
    # Compare captured argument/result roles via ORIGINAL successful creation
    # outputs and ORIGINAL getter returns, never first raw-value/proximity joins.
    def workload(report):
        events=[r for r in report['observations']['full_original_journal']['events']
                if not r['api'].startswith('D4VR_TypedCopy')]
        objects={}; raw_objects={}; heaps={}; increments={}; handles=[]; events_seen=set()
        creation_counts={}; release_values=[]
        for row in events:
            if row['edge']!='after': continue
            result=row['result']; api=row['api']; args=row['args']
            if row['return_kind']=='HRESULT' and type(result) is dict:
                _need(type(result.get('hresult')) is int and 0<=result['hresult']<1<<31, 'All captured original HRESULTs succeeded')
                out=result.get('output')
                def output_refs(value,path='output'):
                    if type(value) is dict and set(value)=={'raw','generation'}:
                        yield path,value
                    elif type(value) is dict:
                        for field,item in value.items():
                            yield from output_refs(item,path+'.'+field)
                refs=list(output_refs(out)) if api.startswith(('Create','D3D12Create','D3D12Serialize')) else []
                if refs:
                    creation_counts[api]=creation_counts.get(api,0)+1
                for field,outref in refs:
                    if outref=={'raw':0,'generation':0}: continue
                    _uint(outref['raw'],positive=True); _uint(outref['generation'],positive=True)
                    role=api+'#'+str(creation_counts[api])+'.'+field; key=(outref['raw'],outref['generation'])
                    _need(key not in objects and outref['raw'] not in raw_objects, 'Unambiguous captured original creation roles')
                    objects[key]=role; raw_objects[outref['raw']]=role
                    if api=='CreateDescriptorHeap': heaps[key]=args['description']
            if api=='GetDescriptorHandleIncrementSize':
                value=_uint(result['value'],(1<<32)-1,True); kind=args['heap_type']
                _need(kind not in increments or increments[kind]==value, 'Consistent actual descriptor increments')
                increments[kind]=value
            if api in ('GetCPUDescriptorHandleForHeapStart','GetGPUDescriptorHandleForHeapStart'):
                ref=args['heap']; key=(ref['raw'],ref['generation'])
                _need(key in heaps, 'Getter targets original created heap')
                desc=heaps[key]; base=_uint(result['value'],positive=True)
                stride=increments.get(desc['heap_type'])
                _need(desc['count']==1 or stride is not None,'Original increment observed before multi-slot getter')
                handles.append((api,objects[key],base,desc['count'],stride or 1))
            if 'event' in args: events_seen.add(_uint(args['event'],positive=True))
            if api=='Release': release_values.append(result)
        _need(len(events_seen)<=1, 'One captured producer completion event')
        def descriptor(value,gpu):
            candidates=[]
            for api,role,base,count,stride in handles:
                if gpu != (api=='GetGPUDescriptorHandleForHeapStart'): continue
                if value>=base and (value-base)%stride==0 and (value-base)//stride<count:
                    candidates.append((role,(value-base)//stride))
            _need(len(set(candidates))==1,'Descriptor argument uniquely joined to exact original getter slot')
            role,index=candidates[0]
            return {'original_heap_role':role,'slot':index,'domain':'GPU_HANDLE' if gpu else 'CPU_HANDLE'}
        def normalize(value,key='',api=''):
            if type(value) is dict:
                if set(value)=={'raw','generation'}:
                    pair=(value['raw'],value['generation'])
                    if pair==(0,0): return {'uncaptured_interface':True}
                    _need(pair in objects,'Explicit original creation role for APP reference')
                    return {'original_object_role':objects[pair]}
                return {k:normalize(v,k,api) for k,v in value.items()}
            if type(value) is list: return [normalize(v,key,api) for v in value]
            if type(value) is int:
                if key.endswith('_pointer') or key=='pointer':
                    return {'caller_pointer_field':key,'nonnull':value!=0}
                if key=='event': return {'producer_completion_event':value in events_seen}
                if key in ('destination','source') and api=='CopyTextureRegion':
                    _need(value in raw_objects,'CopyTextureRegion exact original resource output')
                    return {'original_object_role':raw_objects[value]}
                if key=='cpu_handle' or (key in ('destination','source') and api in ('CreateShaderResourceView','CopyDescriptorsSimple','CopyDescriptors')):
                    return descriptor(value,False)
                if key=='gpu_handle': return descriptor(value,True)
                if (key=='resource' and api in ('ResourceBarrier','CreateRenderTargetView')) or key in ('root','pipeline'):
                    _need(value in raw_objects,'Raw resource argument exact original creation output')
                    return {'original_object_role':raw_objects[value]}
            return value
        records=[]
        for row in events:
            result=row['result']
            if row['edge']=='after' and row['api'] in ('GetCPUDescriptorHandleForHeapStart','GetGPUDescriptorHandleForHeapStart'):
                result={'value':descriptor(result['value'],row['api']=='GetGPUDescriptorHandleForHeapStart')}
            elif row['return_kind']=='pointer' and row['edge']=='after':
                result={'original_pointer_return_nonnull':result['value']!=0}
            elif row['api']=='Release' and row['edge']=='after':
                # ULONG is an observed refcount result, not a workload input or
                # a promise both core implementations have identical internals.
                result={'observed_ULONG_return':True}
            records.append(dict(api=row['api'],edge=row['edge'],case_ordinal=row['case_ordinal'],return_kind=row['return_kind'],
                                args=normalize(row['args'],api=row['api']),result=normalize(result,api=row['api'])))
        return records,release_values
    a,ra=workload(off); b,rb=workload(on)
    _need(_same(a,b), 'OFF/ON captured original arguments/HRESULTs equal under explicit original creation/getter roles')
    result=dict(schema_version=1,purpose='supplied_private_translator_execution_pair_audit',
                supplied_pair_files_consistent=True,milestone_objective_met=on['milestone_objective_met'],source_ON_relation_observed=on['source_relation_observed'],runs=reports,readbacks=4,readback_bytes=4*RAW_BYTES,
                off_on_bytes_compared_once=2*RAW_BYTES,original_API_occurrence_schedule_equal=True,captured_original_argument_roles_and_HRESULTs_equal=True,
                original_Release_ULONG_values_equal=_same(ra,rb),all_original_arguments_captured=False,
                original_argument_identity_across_processes_proven=False,
                claims={key:False for key in _FALSE})
    _encoded(result)
    return result


def drain_original_process(process, *, write, ready=None, read=None):
    """Bounded dual-stream drain for the parent Linux executor, CPU-injectable.

    Process creation is outside this module. Both original pipes are captured
    before any observer operation. There is no set_blocking, clock or sleep.
    Readiness precedes each blocking read; one consumer owns each pipe.
    """
    import select
    if ready is None:
        ready=lambda streams,timeout:select.select(streams,[],[],timeout)[0]
    if read is None: read=os.read
    streams={'stdout':process.stdout,'stderr':process.stderr}
    _need(all(pipe is not None for pipe in streams.values()),'Both original pipes present')
    pending=dict(streams); retained={k:0 for k in streams}; lost={k:0 for k in streams}; drained={k:0 for k in streams}
    failed=set(); exited_ticks=0; observed_code=None
    try:
        while pending:
            readable=ready(list(pending.values()),.1)
            progressed=False
            for pipe in readable:
                labels=[k for k,v in pending.items() if v is pipe]
                _need(len(labels)==1,'Readiness is one original pending pipe')
                label=labels[0]
                try: chunk=read(pipe.fileno(),65536)
                except InterruptedError: continue
                except OSError:
                    raise
                _need(type(chunk) is bytes and len(chunk)<=65536,'Bounded ordinary pipe chunk')
                if not chunk:
                    pending.pop(label); progressed=True; continue
                progressed=True; drained[label]+=len(chunk)
                cap=max(0,MAX_LOG-retained[label]); kept=chunk[:cap]
                lost[label]+=len(chunk)-len(kept)
                if label in failed:
                    lost[label]+=len(kept)
                elif kept:
                    try:
                        write(label,kept)
                        retained[label]+=len(kept)
                    except Exception:
                        # The sink may have partly written. The exact lost count
                        # is unavailable; positive means complete logs REFUSED.
                        failed.add(label); lost[label]+=len(kept)
            code=process.poll()
            if code is not None:
                _need(type(code) is int,'Actual observed wrapper exit')
                observed_code=code
                exited_ticks=0 if progressed else exited_ticks+1
                if exited_ticks>=20 or any(size>MAX_LOG for size in drained.values()):
                    # Never cut a finite <=quota buffered log merely because
                    # the wrapper already exited. Quiet inherited pipes or an
                    # actual over-quota/error stream cannot yield complete logs.
                    failed.update(pending); break
        try:
            code=process.wait()
            _need(type(code) is int,'Actual wait return')
            observed_code=code
        except Exception:
            # A previously returned poll result is already an actual observation.
            if observed_code is None: raise
            failed.update(streams)
        return dict(spawn_succeeded=True,returncode=observed_code,
                    stdout_lost_bytes=max(lost['stdout'],1 if 'stdout' in failed else 0),
                    stderr_lost_bytes=max(lost['stderr'],1 if 'stderr' in failed else 0))
    except Exception:
        # A metadata/readiness failure must not close a started wrapper's pipes
        # before waiting: they could fill and deadlock that original. Emergency
        # readers discard from BOTH existing pipes while the SAME wait runs.
        import threading
        def discard(pipe):
            try:
                while True:
                    chunk=read(pipe.fileno(),65536)
                    if not chunk: break
            except Exception:
                pass
        readers=[]
        try:
            for pipe in streams.values():
                worker=threading.Thread(target=discard,args=(pipe,),daemon=True)
                worker.start(); readers.append(worker)
        except Exception:
            # Actual failure to allocate emergency readers is not recoverable
            # metadata. The SAME actual wait is still attempted below; no
            # universal progress under broken OS pipes/thread resources claimed.
            pass
        finally:
            try:
                code=process.wait()
                _need(type(code) is int,'Actual wait after drain metadata failure')
                observed_code=code
            except Exception:
                if observed_code is None: raise
            finally:
                for worker in readers:
                    try: worker.join(timeout=.1)
                    except Exception: pass
        return dict(spawn_succeeded=True,returncode=observed_code,
                    stdout_lost_bytes=max(1,lost['stdout']),stderr_lost_bytes=max(1,lost['stderr']))
    finally:
        for pipe in streams.values():
            try: pipe.close()
            except OSError: pass


def execute_frozen_run(plan, *, executor, postprobe=None, clock=time.monotonic, armed=False):
    """Single shared path used by the disarmed CLI and pure CPU test doubles.

    No executor default exists.  The caller must explicitly arm.  Actual wait
    results are persisted create-only before ANY postprobe; errors cannot turn
    into zero.  Never retries or stops the original workload for metadata.
    """
    _need(armed is True,'Explicit parent arm required; default disarmed')
    plan_raw = plan if type(plan) is bytes else (plan.encode('utf8', 'strict') if type(plan) is str else _encoded(plan, MAX_METADATA))
    plan=_plan(plan_raw); _need(callable(executor),'Explicit injected executor')
    _artifact_bytes(plan)
    cwd = _open_parent(_path(plan['cwd']) / '_cwd')
    _close_parent(cwd)
    directory=_path(plan['directory'])
    parent=_open_parent(directory); _close_parent(parent)
    try: directory.mkdir(mode=0o700)
    except OSError as error: raise ValueError('Fresh capture root; never overwrite') from error
    _write_new_bytes(str(directory/'launch-plan.json'),plan_raw)
    start=None; actual=None; error=None
    try:
        start=clock()
        _need(type(start) in (int,float) and math.isfinite(start), 'Finite start timer')
    except Exception as caught:
        error={'type':'timer_'+type(caught).__name__,'message':str(caught)[:2048]}
    actual=dict(spawn_succeeded=None,returncode=None,stdout_lost_bytes=None,stderr_lost_bytes=None)
    try:
        candidate=executor(plan)
        # Preserve independently observed fields BEFORE checking the rest of the
        # metadata. A malformed loss counter must never erase an actual rc0.
        if type(candidate) is dict:
            for key in actual:
                value=candidate.get(key)
                if key=='spawn_succeeded' and type(value) is bool:
                    actual[key]=value
                elif key=='returncode' and type(value) is int:
                    actual[key]=value
                elif key.endswith('_lost_bytes') and type(value) is int and 0<=value<(1<<64):
                    actual[key]=value
        _keys(candidate,'spawn_succeeded returncode stdout_lost_bytes stderr_lost_bytes','Executor actual wait result')
        _need(type(candidate['spawn_succeeded']) is bool,'Actual spawn BOOL')
        _need((candidate['spawn_succeeded'] and type(candidate['returncode']) is int) or (not candidate['spawn_succeeded'] and candidate['returncode'] is None),'Actual return code, never fabricated')
        _uint(candidate['stdout_lost_bytes']); _uint(candidate['stderr_lost_bytes'])
    except Exception as caught:
        error=error or {'type':type(caught).__name__,'message':str(caught)[:2048]}
    elapsed=None
    try:
        stop=clock()
        _need(start is not None and type(stop) in (float,int) and math.isfinite(stop), 'Finite return timer')
        elapsed=stop-start
        _need(math.isfinite(elapsed) and elapsed>=0,'Monotonic elapsed host time')
    except Exception as caught:
        elapsed=None
        error=error or {'type':'timer_'+type(caught).__name__,'message':str(caught)[:2048]}
    receipt=dict(schema_version=1,purpose='parent_wrapper_actual_return_before_postprobe',run_id=plan['run_id'],mode=plan['mode'],**actual,host_seconds=elapsed,executor_error=error)
    _save_new(str(directory/'wrapper-return.json'),receipt)
    if postprobe is not None:
        try: postprobe(plan,receipt)
        except Exception as caught:
            _save_new(str(directory/'postprobe-error.json'),dict(type=type(caught).__name__,message=str(caught)[:2048],actual_return_preserved=True))
            raise
    return receipt


def _file_boundary(function):
    def checked(*args, **kwargs):
        try:
            return function(*args, **kwargs)
        except (OSError, KeyError, IndexError, TypeError, StopIteration, UnicodeError, RecursionError) as error:
            raise ValueError('Malformed or unavailable bounded execution inputs') from error
    checked.__name__ = function.__name__
    checked.__doc__ = function.__doc__
    return checked

# Public malformed file/metadata inputs have one refusal exception contract.
read_regular = _file_boundary(read_regular)
audit_run = _file_boundary(audit_run)
audit_pair = _file_boundary(audit_pair)
execute_frozen_run = _file_boundary(execute_frozen_run)


def main(argv=None):
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory',required=True)
    parser.add_argument('--pins',required=True)
    parser.add_argument('--pair',action='store_true')
    parser.add_argument('--output')
    args=parser.parse_args(argv)
    pins=read_regular(args.pins,MAX_METADATA)
    report=(audit_pair if args.pair else audit_run)(args.directory,pins)
    if args.output:
        _write_new_bytes(args.output,_encoded(report))
    else:
        print(_encoded(report).decode('ascii'))
    return 0 if report.get('milestone_objective_met') is True or (not args.pair and report['mode']=='off') else 1


if __name__=='__main__':
    raise SystemExit(main())
