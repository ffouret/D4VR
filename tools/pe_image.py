"""Bounds-checked PE32+ export reader; no execution and no external packages."""
from pathlib import Path
import struct


class PEImage:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        if self.data[:2] != b"MZ":
            raise ValueError("Not a PE image")
        pe = self.u32(0x3C)
        if self.take(pe, 4) != b"PE\0\0":
            raise ValueError("Invalid PE signature")
        self.machine = self.u16(pe + 4)
        sections = self.u16(pe + 6)
        optional_size = self.u16(pe + 20)
        self.characteristics = self.u16(pe + 22)
        optional = pe + 24
        if self.u16(optional) != 0x20B or optional_size < 120 or sections > 96:
            raise ValueError("Expected bounded PE32+ headers")
        self.headers_size = self.u32(optional + 60)
        self.export_rva, self.export_size = struct.unpack("<II", self.take(optional + 112, 8))
        self.sections = []
        for index in range(sections):
            offset = optional + optional_size + index * 40
            self.take(offset, 40)
            virtual_size, address, raw_size, raw_offset = struct.unpack("<IIII", self.take(offset + 8, 16))
            self.sections.append((address, virtual_size, raw_offset, raw_size))

    def take(self, offset, size):
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise ValueError("Truncated PE data")
        return self.data[offset:offset + size]

    def u16(self, offset):
        return struct.unpack("<H", self.take(offset, 2))[0]

    def u32(self, offset):
        return struct.unpack("<I", self.take(offset, 4))[0]

    def offset(self, rva, size=1):
        if rva < self.headers_size:
            self.take(rva, size)
            return rva
        for address, virtual_size, raw_offset, raw_size in self.sections:
            delta = rva - address
            if 0 <= delta < max(raw_size, virtual_size):
                if delta + size > raw_size:
                    raise ValueError("RVA refers to unbacked virtual data")
                self.take(raw_offset + delta, size)
                return raw_offset + delta
        raise ValueError("Unmapped PE RVA")

    def string(self, rva):
        offset = self.offset(rva)
        end = self.data.find(b"\0", offset, min(offset + 512, len(self.data)))
        if end < 0:
            raise ValueError("Unterminated PE name")
        self.offset(rva, end - offset + 1)
        return self.data[offset:end].decode("ascii")

    def exports(self):
        if not self.export_rva:
            return []
        directory = self.offset(self.export_rva, 40)
        base, count, named, functions_rva, names_rva, ordinals_rva = struct.unpack("<IIIIII", self.take(directory + 16, 24))
        if count > 100000 or named > 100000:
            raise ValueError("Unreasonable PE export count")
        functions = self.offset(functions_rva, count * 4)
        names = self.offset(names_rva, named * 4)
        ordinals = self.offset(ordinals_rva, named * 2)
        result = []
        unique = set()
        for index in range(named):
            slot = self.u16(ordinals + index * 2)
            if slot >= count:
                raise ValueError("Export ordinal out of range")
            name = self.string(self.u32(names + index * 4))
            if name in unique:
                raise ValueError("Duplicate named export")
            unique.add(name)
            address = self.u32(functions + slot * 4)
            forwarder = self.string(address) if self.export_rva <= address < self.export_rva + self.export_size else None
            result.append({"name": name, "ordinal": base + slot, "forwarder": forwarder})
        return sorted(result, key=lambda item: (item["ordinal"], item["name"]))
