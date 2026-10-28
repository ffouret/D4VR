#!/usr/bin/env python3
"""CTest bridge for NEW tagged CPU mocks; no vendor or runtime attestation.

Each original UTF8 wire_text is passed to the new analyzer exactly once. Full
results are preserved. Expected decisions here concern supplied mock histories.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.owned_source_writer_validate import analyze

CLAIMS = frozenset((
    "input_facts_attested", "source_runtime_coverage_proven",
    "actual_sampled_input_identity_known", "shader_uses_selected_binding_proven",
    "sampled_resource_GPU_read_proven", "current_descriptor_contents_known",
    "current_input_texels_known", "sampler_known", "game_numeric_UV_known",
    "renderer_owner_known", "backing_known", "loaded_binary_source_equivalence_proven",
    "generic_PE_to_Unix_mapping_known", "global_API_absence_proven", "VR_solution_proven",
    "native_descriptor_or_image_view_binding_proven", "public_device_factory_result_known",
))
COUNT_KEYS = frozenset(("assertions", "workload_mock_delegates", "guard_mock_delegates",
                        "cleanup_mock_delegates", "cold_ON_wire_bytes",
                        "warm_ON_wire_bytes", "GPU_or_Wine_calls"))


def need(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def pairs(values):
    result = {}
    for key, value in values:
        need(key not in result, "Duplicate mock envelope key")
        result[key] = value
    return result


def verify_samples(raw: bytes) -> dict:
    need(type(raw) is bytes and len(raw) <= 1048576, "Mock envelope byte cap")
    data = json.loads(raw.decode("utf-8", "strict"), object_pairs_hook=pairs,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))
    need(type(data) is dict and set(data) == {"schema_version", "purpose", "cases", "counts"},
         "Mock envelope keys")
    need(type(data["schema_version"]) is int and data["schema_version"] == 1 and
         data["purpose"] == "owned_source_writer_CPU_mock_samples", "Mock purpose/version")
    counts = data["counts"]
    need(type(counts) is dict and set(counts) == COUNT_KEYS and
         all(type(v) is int and 0 <= v <= (1 << 64) - 1 for v in counts.values()), "Mock counts")
    need(counts["assertions"] > 0 and counts["workload_mock_delegates"] == 303 and
         counts["cleanup_mock_delegates"] == 48 and counts["guard_mock_delegates"] == 76 and
         counts["GPU_or_Wine_calls"] == 0, "Exact new mock delegate workload")
    need(type(data["cases"]) is list and len(data["cases"]) == 6, "Six mock frames")
    expected = [(label, mode) for mode in ("on", "off", "faulted")
                for label in ("cold_directA", "warm_reset3_quota")]
    reports = []
    for case, (label, mode) in zip(data["cases"], expected):
        need(type(case) is dict and set(case) == {"name", "mode", "expected_known",
             "expected_resource_raw", "wire_text"}, "Mock case keys")
        known = label == "cold_directA" and mode == "on"
        need(case["name"] == label and case["mode"] == mode and
             type(case["expected_known"]) is bool and case["expected_known"] is known and
             (type(case["expected_resource_raw"]) is int and case["expected_resource_raw"] == 101
              if known else case["expected_resource_raw"] is None), "Independent mock expected decision")
        need(type(case["wire_text"]) is str, "Raw original wire string")
        wire = case["wire_text"].encode("utf-8", "strict")
        need(len(wire) <= 65536, "Whole mock frame byte cap")
        if mode == "on":
            counter = "cold_ON_wire_bytes" if label == "cold_directA" else "warm_ON_wire_bytes"
            need(counts[counter] == len(wire), "Actual original mock frame byte count")
        result = analyze(wire)  # Exactly once; never serialize/re-audit nested data.
        need(type(result) is dict and result["historical_declared_bound_view_known"] is known,
             "Supplied cold/warm mock relation decision")
        need(type(result["claims"]) is dict and set(result["claims"]) == CLAIMS and
             all(value is False for value in result["claims"].values()), "All seventeen claims false")
        if known:
            need(result["declared_view"]["resource"]["raw"] == 101,
                 "Cold original mock A origin")
        reports.append({"name": label, "mode": mode, "wire_bytes": len(wire), "result": result})
    result = {"owned_source_writer_CPU_mock_bridge_passed": True,
              "counts": counts, "analyze_invocations": 6, "reports": reports,
              "runtime_or_vendor_facts_attested": False}
    need(len(json.dumps(result, ensure_ascii=True, separators=(",", ":")).encode()) <= 2097152,
         "Whole six-result bridge cap")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", required=True)
    args = parser.parse_args()
    completed = subprocess.run([args.fixture, "--emit-json"], check=False,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    need(completed.returncode == 0, "New CPU fixture failed")
    result = verify_samples(completed.stdout)
    print(json.dumps(result, ensure_ascii=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
