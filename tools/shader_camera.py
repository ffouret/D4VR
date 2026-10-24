#!/usr/bin/env python3
"""Conservatively trace constant-buffer data into a DXIL vertex position.

Reads DXC disassembly only. A matched matrix is a static clip-transform
candidate, not proof of a live scene camera or a stereo renderer.
"""
from __future__ import annotations

import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import subprocess

SSA = r"%[A-Za-z0-9_.$-]+"
ASSIGN = re.compile(r"^\s+(" + SSA + r")\s*=\s*(.*)$")
CB_LOAD = re.compile(r"@dx\.op\.cbufferLoadLegacy\.\w+\(i32 59, %dx\.types\.Handle (" + SSA + r"), i32 ([^)]+)\)")
HANDLE = re.compile(r"@dx\.op\.createHandle\(i32 57, i8 2, i32 (\d+), i32 ([^,]+), i1 (?:true|false)\)")
RESOURCE = re.compile(r'^!\d+ = !\{i32 (\d+), %([\w.$]+)\* undef, !"[^"]*", i32 (\d+), i32 (\d+), i32 (\d+), i32 (\d+),')


def canonical(value: str) -> str:
    value = value.strip()
    if value.startswith("%") or value.startswith("0x"):
        return value
    try:
        return format(float(value), ".9g")
    except ValueError:
        return value


def analyze_ir(text: str) -> dict:
    definitions, handles, layouts, loads, stores = {}, {}, {}, {}, {}
    position_ids = set()
    for line in text.splitlines():
        signature = re.match(r'^!\d+ = !\{i32 (\d+), !"SV_Position",', line, re.I)
        if signature:
            position_ids.add(int(signature[1]))
        resource = RESOURCE.match(line)
        if resource:
            identifier, name, space, register, count, size = resource.groups()
            layouts[int(identifier)] = {"name": name.removeprefix("hostlayout."), "space": int(space),
                                        "register": int(register), "array_count": int(count), "bytes": int(size)}
        assignment = ASSIGN.match(line)
        if assignment:
            value, rhs = assignment.groups()
            definitions[value] = rhs.split(" ;", 1)[0]
            handle = HANDLE.search(rhs)
            if handle:
                handles[value] = int(handle[1])
            load = CB_LOAD.search(rhs)
            if load:
                loads[value] = (load[1], load[2].strip())
    if len(position_ids) != 1:
        return {"position_signature_resolved": False, "final_clip_matrix": None,
                "reason": "missing_or_ambiguous_position_signature", "live_camera_proven": False}
    position_id = next(iter(position_ids))
    output = re.compile(r"@dx\.op\.storeOutput\.f32\(i32 5, i32 " + str(position_id)
                        + r", i32 0, i8 ([0-3]), float ([^)]+)\)")
    duplicate_store = False
    for line in text.splitlines():
        store = output.search(line)
        if store:
            component = int(store[1])
            if component in stores:
                duplicate_store = True
            stores[component] = canonical(store[2])

    def coefficient(value):
        rhs = definitions.get(value, "")
        extract = re.fullmatch(r"extractvalue %dx\.types\.CBufRet\.f32 (" + SSA + r"), ([0-3])", rhs)
        if not extract or extract[1] not in loads:
            return None
        handle, register = loads[extract[1]]
        if not register.isdecimal() or handle not in handles:
            return None
        resource = handles[handle]
        if resource not in layouts:
            return None
        return resource, int(register), int(extract[2])

    def terms(value, seen=None):
        seen = set() if seen is None else seen
        if value in seen or len(seen) > 24:
            return [(value, "1")]
        seen = seen | {value}
        rhs = definitions.get(value, "")
        fma = re.search(r"@dx\.op\.tertiary\.f32\(i32 46, float ([^,]+), float ([^,]+), float ([^)]+)\)", rhs)
        if fma:
            return [(canonical(fma[1]), canonical(fma[2]))] + terms(canonical(fma[3]), seen)
        binary = re.fullmatch(r"(fmul|fadd)(?: \w+)* float ([^,]+), (\S+)", rhs)
        if binary:
            left, right = canonical(binary[2]), canonical(binary[3])
            if binary[1] == "fmul":
                return [(left, right)]
            return terms(left, seen) + terms(right, seen)
        return [(value, "1")]

    def recognize(outputs):
        rows = []
        for value in outputs:
            row = []
            for left, right in terms(value):
                lc, rc = coefficient(left), coefficient(right)
                if bool(lc) == bool(rc):
                    row = []
                    break
                row.append((lc or rc, right if lc else left))
            if len(row) != 4 or {r[0][2] for r in row} != {0, 1, 2, 3}:
                rows = []
                break
            rows.append(sorted(row, key=lambda r: r[0][2]))

        if len(rows) == 4:
            resource = rows[0][0][0][0]
            first_register = rows[0][0][0][1]
            vector = [r[1] for r in rows[0]]
            if all(coef[0] == resource and coef[1] == first_register + i
                   for i, row in enumerate(rows) for coef, _ in row) and all([r[1] for r in row] == vector for row in rows):
                layout = layouts[resource]
                offset = first_register * 16
                if layout["array_count"] == 1 and offset + 64 <= layout["bytes"]:
                    return {"buffer": layout, "byte_offset": offset, "byte_size": 64,
                            "legacy_vector_registers": list(range(first_register, first_register + 4)),
                            "layout_in_this_multiply": "four_dot_product_rows", "input_components": vector}
        return None

    matrix = recognize([stores[i] for i in range(4)]) if not duplicate_store and set(stores) == {0, 1, 2, 3} else None
    chain = []
    current = matrix
    seen_vectors = set()
    while current and len(chain) < 4:
        vector = tuple(current["input_components"])
        if vector in seen_vectors:
            break
        seen_vectors.add(vector)
        current = recognize(vector)
        if current:
            chain.append(current)

    pending, visited = list(stores.values()), set()
    while pending:
        value = pending.pop()
        if value in visited:
            continue
        visited.add(value)
        pending.extend(v for v in re.findall(SSA, definitions.get(value, "")) if v in definitions and v not in visited)
    dependencies = set()
    unresolved = 0
    for value in visited:
        if value in loads:
            handle, register = loads[value]
            resource = handles.get(handle)
            if resource in layouts:
                layout = layouts[resource]
                dependencies.add((layout["space"], layout["register"], register))
            else:
                unresolved += 1
    return {"position_signature_resolved": True, "position_signature_id": position_id,
            "position_components_found": len(stores), "multiple_stores_per_component": duplicate_store,
            "position_dependency_instruction_count": len(visited),
            "constant_buffer_dependencies": [{"space": space, "register": register,
                                               "legacy_vector_index": index} for space, register, index in sorted(dependencies)],
            "unresolved_buffer_loads": unresolved, "final_clip_matrix": matrix,
            "input_matrix_chain": chain, "live_camera_proven": False}


def scan(dxc: Path, directory: Path, inventory: Path, jobs: int, limit: int = 0) -> dict:
    data = json.loads(inventory.read_text(encoding="utf-8"))
    candidates = [s for s in data["shaders"] if s["format"] == "directx_container" and s["stages"] == ["vertex"]]
    if limit:
        candidates = candidates[:limit]
    def inspect(candidate):
        filename = candidate["file"]
        path = directory / filename
        try:
            if Path(filename).name != filename or hashlib.sha256(path.read_bytes()).hexdigest() != candidate["sha256"]:
                raise ValueError("Shader identity differs from captured inventory")
            result = subprocess.run([str(dxc.resolve()), "-dumpbin", str(path.resolve())], capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=30, check=True)
            parsed = analyze_ir(result.stdout)
            return {"file": filename, "sha256": candidate["sha256"],
                    "input_semantics": sorted({p["semantic"] for p in candidate["input_signature"]}), **parsed}
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            return {"file": filename, "error": str(error)}
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        results = list(executor.map(inspect, candidates))
    groups = Counter()
    for item in results:
        matrix = item.get("final_clip_matrix")
        if matrix:
            buffer = matrix["buffer"]
            groups[(buffer["name"], buffer["space"], buffer["register"], buffer["bytes"], matrix["byte_offset"])] += 1
    return {"schema_version": 1, "vertex_files_scanned": len(results),
            "errors": [r for r in results if "error" in r],
            "matched_final_clip_matrices": sum(groups.values()),
            "matrix_groups": [{"buffer_name": key[0], "space": key[1], "register": key[2],
                               "buffer_bytes": key[3], "byte_offset": key[4], "shader_count": count}
                              for key, count in groups.most_common()],
            "shaders": results, "live_camera_proven": False, "native_stereo_proven": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dxc", type=Path, required=True)
    parser.add_argument("--shaders", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4, choices=range(1, 9))
    parser.add_argument("--limit", type=int, default=0)
    args = parser.parse_args()
    if args.limit < 0:
        parser.error("--limit must not be negative")
    report = scan(args.dxc, args.shaders, args.inventory, args.jobs, args.limit)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k != "shaders"}, indent=2))


if __name__ == "__main__":
    main()
