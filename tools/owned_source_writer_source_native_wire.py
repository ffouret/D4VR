#!/usr/bin/env python3
"""Bounded bridge for new C sink CPU mocks; never runtime attestation.

Primary files contain the bytes written by C ReceiptCopy. Each primary is given
to the unchanged calculation once; its complete result is retained. A separate
compiled-OFF fixture is optional. Raw pointers and trace hashes are never equated
between processes. Importing this module does not start a fixture.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.owned_source_writer_validate import analyze

METRICS_LIMIT = 8192
STDERR_LIMIT = 65536
WIRE_LIMIT = 65536
WIRE_COUNT_LIMIT = 8
REPORT_LIMIT = 4194304
MODEL_REPORT_LIMIT = 262144
METRICS_NODE_LIMIT = 512
METRICS_DEPTH_LIMIT = 12
DIRECTORY_ENTRY_LIMIT = 12
FIXTURE_TIMEOUT = 30
PURPOSE = "owned_source_writer_source_CPU_fixture"
DEFAULT_MODES = ("on", "observer_bypassed", "faulted")
DEFAULT_EXPECTED = {"cold_on": True}
_HEX = re.compile(r"[0-9a-f]{64}\Z")
_NAME = re.compile(r"[a-z][a-z0-9_]{0,63}\Z")
_WRAPPER_FILES = frozenset(("metrics.stdout", "fixture.stderr", ".wrapper-result.json"))
CLAIMS = frozenset((
    "input_facts_attested", "source_runtime_coverage_proven",
    "actual_sampled_input_identity_known", "shader_uses_selected_binding_proven",
    "sampled_resource_GPU_read_proven", "current_descriptor_contents_known",
    "current_input_texels_known", "sampler_known", "game_numeric_UV_known",
    "renderer_owner_known", "backing_known", "loaded_binary_source_equivalence_proven",
    "generic_PE_to_Unix_mapping_known", "global_API_absence_proven", "VR_solution_proven",
    "native_descriptor_or_image_view_binding_proven", "public_device_factory_result_known",
))


def need(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def _encoded(value: object) -> bytes:
    return json.dumps(value, ensure_ascii=True, separators=(",", ":"),
                      allow_nan=False).encode("ascii")


def _pairs(items):
    result = {}
    for key, value in items:
        need(key not in result, "Duplicate CPU metrics key")
        result[key] = value
    return result


def _integer(text: str) -> int:
    need(len(text) <= 20 and not text.startswith("-"), "CPU metrics integer width")
    value = int(text)
    need(value <= (1 << 64) - 1, "CPU metrics integer overflow")
    return value


def _reject_number(_):
    raise ValueError("CPU metrics floats and nonfinite numbers are unsupported")


def _metrics(raw: bytes) -> dict:
    need(type(raw) is bytes and len(raw) <= METRICS_LIMIT, "CPU metrics byte cap")
    text = raw.decode("utf-8", "strict")
    depth = 0
    quoted = escaped = False
    for char in text:
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char in "[{":
            depth += 1
            need(depth <= METRICS_DEPTH_LIMIT, "CPU metrics depth cap")
        elif char in "]}":
            depth -= 1
            need(depth >= 0, "CPU metrics bracket order")
    need(depth == 0 and not quoted, "Incomplete CPU metrics")
    data = json.loads(text, object_pairs_hook=_pairs, parse_int=_integer,
                      parse_float=_reject_number, parse_constant=_reject_number)
    nodes = 0

    def walk(value):
        nonlocal nodes
        nodes += 1
        need(nodes <= METRICS_NODE_LIMIT, "CPU metrics node cap")
        if type(value) is dict:
            for key, child in value.items():
                need(len(key.encode("utf-8")) <= 256, "CPU metrics key cap")
                walk(child)
        elif type(value) is list:
            for child in value:
                walk(child)
        elif type(value) is str:
            need(len(value.encode("utf-8")) <= 256 and "\0" not in value,
                 "CPU metrics string cap")
        else:
            need(type(value) is int, "CPU metrics scalar type")

    walk(data)
    need(type(data) is dict and set(data) == {"schema_version", "purpose", "modes", "frames"},
         "CPU metrics root keys")
    need(type(data["schema_version"]) is int and data["schema_version"] == 1
         and data["purpose"] == PURPOSE, "CPU metrics purpose/version")
    return data


def _identity(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def _regular(info) -> bool:
    return stat.S_ISREG(info.st_mode) and not (getattr(info, "st_file_attributes", 0) & 0x400)


def _read_wire(base: Path, filename: str, expected_bytes: int) -> bytes:
    # Canonicalize the host root; child names cannot introduce a path component.
    path = base / filename
    before = path.lstat()
    need(_regular(before) and before.st_size == expected_bytes, "CPU raw file type/size")
    flags = (os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
             | getattr(os, "O_NONBLOCK", 0))
    descriptor = os.open(path, flags)
    try:
        opened = os.fstat(descriptor)
        need(_regular(opened) and _identity(opened) == _identity(before),
             "CPU raw file changed before open")
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            raw = stream.read(WIRE_LIMIT + 1)
        after = os.fstat(descriptor)
        need(_identity(after) == _identity(opened), "CPU raw file changed during read")
    finally:
        os.close(descriptor)
    need(len(raw) == expected_bytes and len(raw) <= WIRE_LIMIT,
         "Incomplete/oversize CPU raw file")
    need(_identity(path.lstat()) == _identity(before), "CPU raw path changed during read")
    return raw


def verify_samples(metrics_raw: bytes, directory: Path,
                   expected: dict[str, bool] | None = None, *,
                   expected_modes: tuple[str, ...] = DEFAULT_MODES,
                   require_matching_trace: bool = True) -> dict:
    """Verify supplied CPU files, retaining every complete child calculation.

    ``expected`` is the independent test oracle, never input to the C producer.
    Independent native controls may use their own explicit mode list. No supplied
    metrics become proof that original SOURCE or a vendor binary executed.
    """
    need(expected is None or type(expected) is dict, "CPU test oracle type")
    expected = dict(DEFAULT_EXPECTED if expected is None else expected)
    need(len(expected) <= WIRE_COUNT_LIMIT and all(type(k) is str and _NAME.fullmatch(k)
         and type(v) is bool for k, v in expected.items()), "CPU test oracle")
    need(type(expected_modes) is tuple and 1 <= len(expected_modes) <= 8
         and all(type(x) is str and _NAME.fullmatch(x) for x in expected_modes)
         and len(set(expected_modes)) == len(expected_modes), "CPU mode oracle")
    data = _metrics(metrics_raw)
    modes = data["modes"]
    need(type(modes) is list and len(modes) == len(expected_modes), "CPU mode count")
    for mode, label in zip(modes, expected_modes):
        need(type(mode) is dict and set(mode) == {"mode", "original_calls",
             "original_trace_sha256", "assertions"}, "CPU mode keys")
        need(mode["mode"] == label and type(mode["original_calls"]) is int
             and mode["original_calls"] > 0 and type(mode["assertions"]) is int
             and mode["assertions"] > 0 and type(mode["original_trace_sha256"]) is str
             and _HEX.fullmatch(mode["original_trace_sha256"]), "CPU mode values")
    if require_matching_trace:
        need(all(x["original_calls"] == modes[0]["original_calls"]
                 and x["original_trace_sha256"] == modes[0]["original_trace_sha256"]
                 for x in modes), "Same-process original mock traces differ")
    frames = data["frames"]
    need(type(frames) is list and len(frames) <= WIRE_COUNT_LIMIT
         and len(frames) == len(expected), "CPU primary frame count")
    names = set()
    filenames = set()
    for frame in frames:
        need(type(frame) is dict and set(frame) == {"name", "mode", "file", "bytes", "sha256"},
             "CPU primary keys")
        name = frame["name"]
        need(type(name) is str and _NAME.fullmatch(name) and name in expected
             and name not in names, "CPU primary name")
        need(frame["mode"] in expected_modes and frame["file"] == name + ".wire.json"
             and type(frame["bytes"]) is int and 0 < frame["bytes"] <= WIRE_LIMIT
             and type(frame["sha256"]) is str and _HEX.fullmatch(frame["sha256"]),
             "CPU primary filename/size/SHA")
        names.add(name)
        filenames.add(frame["file"])
    need(names == set(expected), "CPU primary names differ from test oracle")
    base = Path(directory).resolve(strict=True)
    need(base.is_dir(), "CPU output directory")
    entries = 0
    with os.scandir(base) as listing:
        for entry in listing:
            entries += 1
            need(entries <= DIRECTORY_ENTRY_LIMIT, "CPU output directory entry cap")
            need(entry.name in filenames or entry.name in _WRAPPER_FILES,
                 "Unexpected CPU output artifact")
            need(entry.is_file(follow_symlinks=False) and not entry.is_symlink(),
                 "CPU output artifact is not a regular child file")
    primaries = []
    for frame in frames:
        raw = _read_wire(base, frame["file"], frame["bytes"])
        digest = hashlib.sha256(raw).hexdigest()
        need(digest == frame["sha256"], "CPU primary raw SHA mismatch")
        primaries.append((frame, raw, digest))
    # At most 8*65536 detached test bytes; validate all sources before any model call.
    reports = []
    for frame, raw, digest in primaries:
        result = analyze(raw)  # Once per primary; never reserialize/audit a nested input.
        need(type(result) is dict and result["historical_declared_bound_view_known"]
             is expected[frame["name"]], "CPU supplied historical relation")
        need(type(result["claims"]) is dict and set(result["claims"]) == CLAIMS
             and all(value is False for value in result["claims"].values()),
             "Seventeen stronger claims must remain false")
        need(len(_encoded(result)) <= MODEL_REPORT_LIMIT, "Complete CPU model report cap")
        reports.append({"name": frame["name"], "mode": frame["mode"],
                        "wire_bytes": len(raw), "raw_sha256": digest, "result": result})
    result = {"schema_version": 1, "purpose": "owned_source_writer_source_CPU_bridge",
              "fixture_CPU_protocol_verified": True, "modes": modes,
              "analyze_invocations": len(reports), "reports": reports,
              "runtime_or_vendor_facts_attested": False}
    need(len(_encoded(result)) <= REPORT_LIMIT, "Complete CPU bridge report cap")
    return result


def _write_new(path: Path, raw: bytes) -> None:
    # A CPU child must not turn a reserved wrapper output into a followed link.
    flags = (os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0)
             | getattr(os, "O_NOFOLLOW", 0))
    descriptor = os.open(path, flags, 0o600)
    with os.fdopen(descriptor, "wb") as stream:
        stream.write(raw)


def run_fixture(fixture: Path, directory: Path) -> tuple[bytes, dict]:
    """Run only the explicitly selected CPU child with bounded pipe capture."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    buffers = {"stdout": bytearray(), "stderr": bytearray()}
    overflow = {"stdout": False, "stderr": False}
    pipe_errors = {"stdout": None, "stderr": None}
    limits = {"stdout": METRICS_LIMIT, "stderr": STDERR_LIMIT}
    outcome = {"returncode": None, "timed_out": False, "spawn_error": None,
               "wrapper_error": None}
    workers = []
    started_labels = set()
    child = None
    try:
        child = subprocess.Popen([str(Path(fixture).resolve(strict=True)), "--emit-files", str(directory)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False)

        def capture(label, stream):
            try:
                while True:
                    room = limits[label] - len(buffers[label])
                    chunk = stream.read(min(4096, room + 1))
                    if not chunk:
                        break
                    buffers[label].extend(chunk[:room])
                    if len(chunk) > room:
                        overflow[label] = True
                        try:
                            child.kill()
                        except OSError:
                            pass
                        break
            except Exception as error:
                pipe_errors[label] = f"{type(error).__name__}: {error}"[:2048]
            finally:
                try:
                    stream.close()
                except Exception as error:
                    pipe_errors[label] = f"{type(error).__name__}: {error}"[:2048]

        for label, stream in (("stdout", child.stdout), ("stderr", child.stderr)):
            worker = threading.Thread(target=capture, args=(label, stream), daemon=True)
            worker.start()
            workers.append(worker)
            started_labels.add(label)
        try:
            outcome["returncode"] = child.wait(timeout=FIXTURE_TIMEOUT)
        except subprocess.TimeoutExpired:
            outcome["timed_out"] = True
    except Exception as error:
        field = "spawn_error" if child is None else "wrapper_error"
        outcome[field] = f"{type(error).__name__}: {error}"[:2048]
    finally:
        if child is not None and outcome["returncode"] is None:
            # Setup/capture failure also owns a child; do not leave it unwaited.
            try:
                child.kill()
            except OSError as error:
                outcome["wrapper_error"] = f"kill {type(error).__name__}: {error}"[:2048]
            try:
                outcome["returncode"] = child.wait(timeout=3)
            except Exception as error:
                outcome["wrapper_error"] = f"wait {type(error).__name__}: {error}"[:2048]
        for worker in workers:
            try:
                worker.join(timeout=3)
            except Exception as error:
                outcome["wrapper_error"] = f"join {type(error).__name__}: {error}"[:2048]
        if child is not None:
            for label, stream in (("stdout", child.stdout), ("stderr", child.stderr)):
                if label not in started_labels and stream is not None:
                    try:
                        stream.close()
                    except Exception as error:
                        pipe_errors[label] = f"{type(error).__name__}: {error}"[:2048]
        outcome.update({"stdout_overflow": overflow["stdout"],
                        "stderr_overflow": overflow["stderr"],
                        "stdout_retained_bytes": len(buffers["stdout"]),
                        "stderr_retained_bytes": len(buffers["stderr"]),
                        "pipe_errors": pipe_errors,
                        "capture_complete": all(not x.is_alive() for x in workers)})
        # Persist the actual child return before parsing metrics or probing raw files.
        _write_new(directory / ".wrapper-result.json", _encoded(outcome))
        _write_new(directory / "metrics.stdout", bytes(buffers["stdout"]))
        _write_new(directory / "fixture.stderr", bytes(buffers["stderr"]))
    need(outcome["returncode"] == 0 and not outcome["timed_out"]
         and outcome["spawn_error"] is None and outcome["wrapper_error"] is None
         and outcome["capture_complete"]
         and not any(overflow.values()) and not any(pipe_errors.values()),
         "CPU fixture failed or bounded capture incomplete")
    return bytes(buffers["stdout"]), outcome


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--off-fixture", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    base = args.output_dir or Path(tempfile.mkdtemp(prefix="d4vr-owned-C-CPU-", dir=Path.cwd()))
    if args.output_dir:
        base.mkdir(parents=True, exist_ok=False)
    base = base.resolve(strict=True)
    report_path = args.report or base / "bridge-report.json"
    raw, outcome = run_fixture(args.fixture, base / "main")
    main_report = verify_samples(raw, base / "main")
    result = {"CPU_bridge_verified": True, "CPU_output_directory": str(base),
              "complete_report_path": str(report_path.absolute()),
              "main": main_report, "main_child": outcome,
              "compiled_OFF": None, "runtime_or_vendor_facts_attested": False}
    if args.off_fixture:
        raw_off, off_outcome = run_fixture(args.off_fixture, base / "compiled_OFF")
        off_report = verify_samples(raw_off, base / "compiled_OFF", {},
                                    expected_modes=("compiled_off",), require_matching_trace=False)
        need(off_report["modes"][0]["original_calls"] == main_report["modes"][0]["original_calls"],
             "Compiled OFF original call count differs")
        result["compiled_OFF"] = {"report": off_report, "child": off_outcome,
                                  "raw_trace_hash_or_pointers_equated_between_processes": False}
    encoded = _encoded(result)
    need(len(encoded) <= REPORT_LIMIT, "Complete aggregate CPU report cap")
    _write_new(report_path, encoded)
    print(encoded.decode("ascii"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
