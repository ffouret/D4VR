#!/usr/bin/env python3
"""Explicit bridge for independent C mocks; import never starts a process.

The four primaries are real C ReceiptCopy output. Expectations are authored
independently from the sink and model. Each is analyzed once by the bounded
reader, and its entire result is retained without changing its input bytes.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools import owned_source_writer_source_native_wire as reader

EXPECTED = {"direct_a": True, "late_b_copy_entry": True,
            "pending_cut": False, "first_noemit": False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    base = args.output_dir or Path(tempfile.mkdtemp(prefix="d4vr-independent-C-", dir=Path.cwd()))
    if args.output_dir:
        base.mkdir(parents=True, exist_ok=False)
    raw, actual_return = reader.run_fixture(args.fixture, base / "frames")
    # Save each entire first calculation even if a later expectation fails.
    # The calculation receives the original byte string; no second analysis is
    # used to write these standalone comparison files.
    calculate = reader.analyze
    calculations = 0
    def retain_calculation(raw_wire):
        nonlocal calculations
        result = calculate(raw_wire)
        calculations += 1
        complete = json.dumps(result, separators=(",", ":"), ensure_ascii=True,
                              allow_nan=False).encode("ascii")
        reader._write_new(base / ("primary-result-" + str(calculations) + ".json"), complete)
        return result
    reader.analyze = retain_calculation
    try:
        report = reader.verify_samples(raw, base / "frames", EXPECTED,
            expected_modes=("independent_cpu",), require_matching_trace=False)
    except Exception as error:
        reader._write_new(base / "inquiry.json", json.dumps({
            "error_type": type(error).__name__, "message": str(error),
            "completed_calculations": calculations}, separators=(",", ":")).encode("ascii"))
        raise
    finally:
        reader.analyze = calculate
    report_path = args.report or base / "report.json"
    result = {"independent_CPU_bridge_verified": True,
              "output_directory": str(base.resolve()),
              "complete_report_path": str(report_path.absolute()),
              "complete_primary_report": report, "actual_child": actual_return,
              "runtime_or_vendor_facts_attested": False}
    output = json.dumps(result, separators=(",", ":"), ensure_ascii=True,
                        allow_nan=False).encode("ascii")
    if len(output) > reader.REPORT_LIMIT:
        raise ValueError("Complete independent CPU bridge report cap")
    reader._write_new(report_path, output)
    print(json.dumps({"independent_CPU_bridge_verified": True,
        "primary_count": len(report["reports"]),
        "analyze_invocations": report["analyze_invocations"],
        "output_directory": str(base.resolve()),
        "complete_report_path": str(report_path.absolute()),
        "expected": EXPECTED, "report_bytes": len(output),
        "runtime_or_vendor_facts_attested": False}, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
