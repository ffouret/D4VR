#!/usr/bin/env python3
"""Read DXBC/DXIL and SPIR-V stage/resource metadata without executing shaders.

Format references:
https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/DxilContainer/DxilContainer.h
https://github.com/KhronosGroup/SPIRV-Headers/blob/main/include/spirv/unified1/spirv.hpp
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

DX_STAGES = {0: "pixel", 1: "vertex", 2: "geometry", 3: "hull", 4: "domain", 5: "compute",
             6: "library", 13: "mesh", 14: "amplification", 15: "node"}
SPV_STAGES = {0: "vertex", 1: "hull", 2: "domain", 3: "geometry", 4: "pixel", 5: "compute",
              5313: "ray_generation", 5314: "intersection", 5315: "any_hit", 5316: "closest_hit",
              5317: "miss", 5318: "callable",
              5267: "amplification", 5268: "mesh", 5364: "amplification", 5365: "mesh"}
STORAGE = {0: "uniform_constant", 2: "uniform", 9: "push_constant", 12: "storage_buffer"}
MAX_FILE = 16 * 1024 * 1024


def dx_program(data: bytes) -> dict:
    if len(data) < 8:
        raise ValueError("Truncated shader program")
    version = struct.unpack_from("<I", data)[0]
    return {"stage": DX_STAGES.get(version >> 16, "unknown"),
            "shader_model": f"{(version >> 4) & 15}.{version & 15}"}


def signature(data: bytes) -> list[dict]:
    if len(data) < 8:
        raise ValueError("Truncated signature header")
    count, offset = struct.unpack_from("<II", data)
    if count > 512 or offset < 8 or offset + count * 32 > len(data):
        raise ValueError("Invalid signature element table")
    result = []
    for index in range(count):
        position = offset + index * 32
        stream, name, semantic_index, system_value, component_type, register = struct.unpack_from("<6I", data, position)
        if name >= len(data):
            raise ValueError("Signature name outside part")
        end = data.find(b"\0", name)
        if end < 0 or end - name > 256:
            raise ValueError("Unterminated signature name")
        result.append({"semantic": data[name:end].decode("ascii", errors="replace"), "index": semantic_index,
                       "system_value": system_value, "register": register, "mask": data[position + 24]})
    return result


def directx(data: bytes) -> dict:
    if len(data) < 32 or data[:4] != b"DXBC":
        raise ValueError("Expected DXBC/DXIL container")
    size, count = struct.unpack_from("<II", data, 24)
    if size != len(data) or count > 1024 or 32 + count * 4 > size:
        raise ValueError("Invalid DXBC size or part table")
    parts, programs, inputs, outputs = [], [], [], []
    for index in range(count):
        offset = struct.unpack_from("<I", data, 32 + index * 4)[0]
        if offset < 32 + count * 4 or offset + 8 > size:
            raise ValueError("DXBC part offset outside container")
        tag = data[offset:offset + 4]
        length = struct.unpack_from("<I", data, offset + 4)[0]
        if offset + 8 + length > size:
            raise ValueError("DXBC part outside container")
        parts.append(tag.decode("ascii", errors="replace"))
        if tag in (b"DXIL", b"SHDR", b"SHEX"):
            programs.append(dx_program(data[offset + 8:offset + 8 + length]))
        elif tag == b"ISG1":
            inputs = signature(data[offset + 8:offset + 8 + length])
        elif tag == b"OSG1":
            outputs = signature(data[offset + 8:offset + 8 + length])
    return {"format": "directx_container", "parts": parts, "programs": programs,
            "stages": sorted({p["stage"] for p in programs}), "input_signature": inputs, "output_signature": outputs}


def spirv(data: bytes) -> dict:
    if len(data) < 20 or len(data) % 4:
        raise ValueError("Truncated SPIR-V header or word")
    if data[:4] == b"\x03\x02\x23\x07":
        endian = "<"
    elif data[:4] == b"\x07\x23\x02\x03":
        endian = ">"
    else:
        raise ValueError("Invalid SPIR-V magic")
    words = struct.unpack(endian + str(len(data) // 4) + "I", data)
    if not words[3]:
        raise ValueError("Invalid SPIR-V ID bound")
    entries, names, decorations, variables = [], {}, {}, []
    matrix_types = 0
    index = 5
    def literal(values):
        return struct.pack(endian + str(len(values)) + "I", *values).split(b"\0", 1)[0].decode("utf-8", errors="replace")[:256]
    while index < len(words):
        count, opcode = words[index] >> 16, words[index] & 0xFFFF
        if not count or index + count > len(words):
            raise ValueError("Invalid SPIR-V instruction extent")
        values = words[index + 1:index + count]
        if opcode == 15 and len(values) >= 3:
            entries.append({"stage": SPV_STAGES.get(values[0], "unknown"), "execution_model": values[0], "name": literal(values[2:])})
        elif opcode == 5 and len(values) >= 2:
            names[values[0]] = literal(values[1:])
        elif opcode == 71 and len(values) == 3 and values[1] in (33, 34):
            decorations.setdefault(values[0], {})["binding" if values[1] == 33 else "descriptor_set"] = values[2]
        elif opcode == 59 and len(values) >= 3 and values[2] in STORAGE:
            variables.append((values[1], STORAGE[values[2]]))
        elif opcode == 24:
            matrix_types += 1
        index += count
    resources = [{"id": identifier, "storage_class": storage, "name": names.get(identifier),
                  **decorations.get(identifier, {})} for identifier, storage in variables]
    return {"format": "spirv", "entries": entries, "stages": sorted({p["stage"] for p in entries}),
            "declared_resources": resources, "matrix_type_declarations": matrix_types}


def analyze(directory: Path) -> dict:
    files = sorted(p for p in directory.iterdir() if p.suffix.lower() in {".dxbc", ".dxil", ".spv"} and p.is_file() and not p.is_symlink())
    if len(files) > 50000:
        raise ValueError("Too many shader files for one diagnostic capture")
    if sum(path.stat().st_size for path in files) > 1024 * 1024 * 1024:
        raise ValueError("Capture exceeds diagnostic total size limit")
    entries, errors, stages = [], [], Counter()
    total = 0
    for path in files:
        try:
            if path.stat().st_size > MAX_FILE:
                raise ValueError("Shader exceeds diagnostic size limit")
            data = path.read_bytes()
            total += len(data)
            item = spirv(data) if path.suffix.lower() == ".spv" else directx(data)
            entries.append({"file": path.name, "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data), **item})
            for stage in item["stages"]:
                stages[item["format"] + ":" + stage] += 1
        except ValueError as error:
            errors.append({"file": path.name, "error": str(error)})
    position_inputs = [s["file"] for s in entries if "vertex" in s["stages"]
                       and any(p["semantic"].upper() == "POSITION" and p["system_value"] == 0 for p in s.get("input_signature", []))]
    return {"schema_version": 1, "files_seen": len(files), "files_parsed": len(entries), "bytes_read": total,
            "stage_counts_by_format": dict(sorted(stages.items())), "errors": errors, "shaders": entries,
            "vertex_files_with_position_input": position_inputs,
            "world_geometry_shader_identified": False, "live_camera_values_captured": False, "native_stereo_proven": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.directory)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    summary = {k: v for k, v in result.items() if k not in {"shaders", "vertex_files_with_position_input"}}
    summary["vertex_files_with_position_input"] = len(result["vertex_files_with_position_input"])
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
