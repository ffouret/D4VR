"""CTest bridge for NEW CPU mock bytes; never a vendor/runtime attestation."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from source_emitted_writer_validate import analyze

_FALSE_CLAIMS = {
    "input_facts_attested", "source_runtime_coverage_proven",
    "actual_sampled_input_identity_known", "shader_uses_selected_binding_proven",
    "sampled_resource_GPU_read_proven", "current_descriptor_contents_known",
    "current_input_texels_known", "sampler_known", "game_numeric_UV_known",
    "renderer_owner_known", "backing_known", "loaded_binary_source_equivalence_proven",
    "generic_PE_to_Unix_mapping_known", "global_API_absence_proven", "VR_solution_proven",
    "native_descriptor_or_image_view_binding_proven", "public_device_factory_result_known",
}
_COUNT_KEYS = {"case_count", "assertions", "original_mock_calls", "emitted_original_calls",
               "forwarding_comparisons", "forwarding_semantic_sha256", "GPU_or_Wine_calls"}


def check(fixture: Path) -> dict:
    observed = {}
    reports = []
    for mode in ("on", "off", "faulted"):
        # Keep subprocess output on disk, then apply the test protocol bounds.
        # This is a CPU mock test process, not an application launch controller.
        with tempfile.TemporaryDirectory(prefix="d4vr-source-wire-") as directory:
            stdout = Path(directory) / "stdout"
            stderr = Path(directory) / "stderr"
            with stdout.open("wb") as out, stderr.open("wb") as err:
                result = subprocess.run(
                    [str(fixture), "--emit-json", "--mode", mode],
                    stdout=out, stderr=err, timeout=30, check=False)
            if stdout.stat().st_size > 512 * 1024 or stderr.stat().st_size > 8192:
                raise ValueError("CPU mock test output exceeded its protocol bound")
            if result.returncode != 0:
                raise ValueError(f"CPU fixture {mode} returned {result.returncode}: "
                                 f"{stderr.read_text(encoding='utf-8', errors='replace')}")
            envelope = json.loads(stdout.read_bytes())
        if set(envelope) != {"schema_version", "purpose", "cases", "counts"}:
            raise ValueError("unexpected CPU sample envelope fields")
        if (type(envelope["schema_version"]) is not int or envelope["schema_version"] != 1
                or envelope["purpose"] != "source_emitted_writer_CPU_mock_samples"):
            raise ValueError("wrong CPU sample purpose")
        counts = envelope["counts"]
        if (not isinstance(counts, dict) or set(counts) != _COUNT_KEYS
                or any(type(counts[k]) is not int or not 0 <= counts[k] <= (1 << 64) - 1
                       for k in _COUNT_KEYS - {"forwarding_semantic_sha256"})
                or not isinstance(counts["forwarding_semantic_sha256"], str)
                or re.fullmatch("[0-9a-f]{64}", counts["forwarding_semantic_sha256"]) is None):
            raise ValueError("wrong CPU count fields or types")
        if (counts["case_count"] != 2 or counts["GPU_or_Wine_calls"] != 0
                or counts["forwarding_comparisons"] <= 0
                or counts["original_mock_calls"] <= 0):
            raise ValueError("missing CPU original-forwarding checks")
        observed[mode] = counts
        cases = envelope["cases"]
        if not isinstance(cases, list) or [c["name"] for c in cases] != ["directA", "lateB"]:
            raise ValueError("wrong finite CPU cases")
        for case, resource in zip(cases, (101, 102)):
            if set(case) != {"name", "mode", "expected_known", "expected_resource_raw", "wire_text"}:
                raise ValueError("unexpected CPU case fields")
            if case["mode"] != mode or not isinstance(case["wire_text"], str):
                raise ValueError("wrong mode or non-text wire")
            expected = mode == "on"
            if (case["expected_known"] is not expected
                    or case["expected_resource_raw"] != (resource if expected else None)):
                raise ValueError("fixture expectations disagree with independent finite cases")
            raw = case["wire_text"].encode("utf-8")
            if len(raw) > 65536:
                raise ValueError("oversize supplied SOURCE wire")
            report = analyze(raw)
            if report["historical_declared_bound_view_known"] is not expected:
                raise ValueError(f"unexpected {mode}/{case['name']} relation: {report['diagnostics']}")
            if (set(report["claims"]) != _FALSE_CLAIMS
                    or any(v is not False for v in report["claims"].values())):
                raise ValueError("a CPU calculation promoted a stronger claim")
            if expected:
                view = report["declared_view"]
                if (view["resource"]["raw"] != resource
                        or report["origin"] != "COM_mutable_table"
                        or report["recording_join_known"] is not True
                        or report["execute_arguments_join_known"] is not True):
                    raise ValueError("wrong historical declared COM view")
            elif report["declared_view"] is not None:
                raise ValueError("diagnostic mode retained a positive view")
            reports.append({"mode": mode, "case": case["name"], "wire_bytes": len(raw),
                            "wire_sha256": hashlib.sha256(raw).hexdigest(),
                            "historical_declared_bound_view_known": expected})
    reference = observed["on"]
    for counts in observed.values():
        for key in ("original_mock_calls", "emitted_original_calls", "forwarding_semantic_sha256"):
            if counts[key] != reference[key]:
                raise ValueError("original mock workload differs across metadata modes")
    return {"schema_version": 1, "purpose": "source_emitted_writer_CPU_mock_wire_check",
            "counts": observed, "samples": reports, "sample_count": len(reports),
            "original_callbacks_are_mocked": True, "vendor_or_GPU_execution_attested": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check(args.fixture.resolve()), sort_keys=True, separators=(",", ":")))
