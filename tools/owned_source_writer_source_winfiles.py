"""Windows leaf-file IO for the CPU-only writer test reader.

Use one Win32 information family rather than mixing CPython stat/fstat paths.
CreateFileW OPEN_REPARSE_POINT protects the leaf; ancestor canonicalization is
performed by the caller. Unsupported file-ID queries fail, without a fallback.
Microsoft fileapi CreateFileW/GetFileInformationByHandleEx/ReadFile/WriteFile.
"""
from __future__ import annotations

import ctypes as C
import errno
import os
from pathlib import Path

DWORD = C.c_uint32
BOOL = C.c_int32
HANDLE = C.c_void_p

class FileIdInfo(C.Structure):
    _fields_ = [("volume", C.c_uint64), ("identifier", C.c_ubyte * 16)]

class FileBasicInfo(C.Structure):
    _fields_ = [("creation", C.c_int64), ("access", C.c_int64),
                ("write", C.c_int64), ("change", C.c_int64),
                ("attributes", DWORD)]

if (C.sizeof(FileIdInfo), C.alignment(FileIdInfo), C.sizeof(FileBasicInfo),
        C.alignment(FileBasicInfo)) != (24, 8, 40, 8):
    raise RuntimeError("Unsupported Win32 file-information layout")


class WindowsFiles:
    """Synchronous bounded file IO; injectable API is used only by CPU mocks."""
    def __init__(self, api=None, last_error=None):
        if api is None:
            if os.name != "nt":
                raise RuntimeError("Win32 backend requested on another OS")
            api = C.WinDLL("kernel32", use_last_error=True)
        self.api = api
        self.last_error = last_error or getattr(C, "get_last_error", lambda: 1)
        specs = {
            "CreateFileW": ([C.c_wchar_p, DWORD, DWORD, C.c_void_p, DWORD,
                             DWORD, HANDLE], HANDLE),
            "GetFileType": ([HANDLE], DWORD),
            "GetFileInformationByHandleEx": ([HANDLE, C.c_int32, C.c_void_p, DWORD], BOOL),
            "GetFileSizeEx": ([HANDLE, C.POINTER(C.c_int64)], BOOL),
            "ReadFile": ([HANDLE, C.c_void_p, DWORD, C.POINTER(DWORD), C.c_void_p], BOOL),
            "WriteFile": ([HANDLE, C.c_void_p, DWORD, C.POINTER(DWORD), C.c_void_p], BOOL),
            "CloseHandle": ([HANDLE], BOOL),
        }
        for name, (args, result) in specs.items():
            function = getattr(api, name)
            function.argtypes = args
            function.restype = result

    def _error(self, path):
        code = self.last_error()
        if code in (80, 183):
            raise FileExistsError(errno.EEXIST, "Windows file already exists", str(path))
        if code in (2, 3):
            raise FileNotFoundError(errno.ENOENT, "Windows file unavailable", str(path))
        raise OSError(code, "Win32 file operation failed", str(path))

    def _open(self, path, access, share, disposition):
        handle = self.api.CreateFileW(str(path), access, share, None, disposition,
                                      0x00200000 | 0x80, None)
        if handle in (None, 0, C.c_void_p(-1).value):
            self._error(path)
        return handle

    def _close(self, handle, path):
        if not self.api.CloseHandle(handle):
            self._error(path)

    def _info(self, handle, path):
        if self.api.GetFileType(handle) != 1:
            raise ValueError("CPU raw handle is not a disk file")
        identity, basic, size = FileIdInfo(), FileBasicInfo(), C.c_int64()
        if not self.api.GetFileInformationByHandleEx(handle, 18, C.byref(identity), C.sizeof(identity)):
            self._error(path)
        if not self.api.GetFileInformationByHandleEx(handle, 0, C.byref(basic), C.sizeof(basic)):
            self._error(path)
        if not self.api.GetFileSizeEx(handle, C.byref(size)):
            self._error(path)
        if basic.attributes & (0x10 | 0x400) or size.value < 0:
            raise ValueError("CPU raw handle is a directory/reparse point or has invalid size")
        return (identity.volume, bytes(identity.identifier), size.value,
                basic.creation, basic.write, basic.change, basic.attributes)

    def read_wire(self, path: Path, expected_bytes: int) -> bytes:
        handle = self._open(path, 0x80000000, 1, 3)  # READ/share READ/OPEN_EXISTING
        try:
            before = self._info(handle, path)
            if before[2] != expected_bytes or not 0 < expected_bytes <= 65536:
                raise ValueError("CPU raw handle size differs from declared size")
            buffer, count = C.create_string_buffer(65537), DWORD()
            if not self.api.ReadFile(handle, buffer, 65537, C.byref(count), None):
                self._error(path)
            if count.value != expected_bytes or count.value > 65536:
                raise ValueError("CPU raw read incomplete or exceeds cap")
            if self._info(handle, path) != before:
                raise ValueError("CPU raw handle changed during read")
            # The first handle remains open without write/delete sharing.
            other = self._open(path, 0x80000000, 1, 3)
            try:
                if self._info(other, path) != before:
                    raise ValueError("CPU raw path no longer names the read handle")
            finally:
                self._close(other, path)
            return buffer.raw[:count.value]
        finally:
            self._close(handle, path)

    def write_new(self, path: Path, raw: bytes) -> None:
        if type(raw) is not bytes or len(raw) > 4194304:
            raise ValueError("CPU wrapper output cap/type")
        handle = self._open(path, 0x40000080, 0, 1)  # WRITE+READ_ATTRIBUTES/no sharing/CREATE_NEW
        try:
            before = self._info(handle, path)
            if before[2] != 0:
                raise ValueError("New CPU wrapper file was not empty")
            if raw:
                buffer, count = C.create_string_buffer(raw, len(raw)), DWORD()
                if not self.api.WriteFile(handle, buffer, len(raw), C.byref(count), None):
                    self._error(path)
                if count.value != len(raw):
                    raise ValueError("Incomplete CPU wrapper write")
            after = self._info(handle, path)
            if after[:2] != before[:2] or after[2] != len(raw):
                raise ValueError("CPU wrapper identity/size changed during write")
        finally:
            self._close(handle, path)
