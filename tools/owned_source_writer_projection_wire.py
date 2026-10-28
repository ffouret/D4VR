#!/usr/bin/env python3
"""Bounded CPU-only transport for the private bridge and digest mocks.

The input manifest is supplied by a new owned fixture. Only actual primary
ReceiptCopy bytes are passed to the frozen calculation, once per primary. This
does not attest a vendor, SOURCE hook, current contents, game input or VR.
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
import time

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.owned_source_writer_validate import analyze

MODEL_SHA256 = "660623fb5da3cbec1ea7fbe2c6f1513beadd3950c97416a6126a71de119479bf"
METRICS_LIMIT = 8192
STDERR_LIMIT = 65536
WIRE_LIMIT = 65536
WIRE_COUNT_LIMIT = 8
WRAPPER_LIMIT = 16384
MODEL_REPORT_LIMIT = 262144
REPORT_LIMIT = 4194304
FIXTURE_TIMEOUT = 30
DRAIN_TIMEOUT = 5
PURPOSE = "owned_source_writer_projection_CPU_fixture"
MODES = ("on", "observer_bypassed", "faulted")
_WINDOWS = os.name == "nt"
_U64 = (1 << 64) - 1
_U32 = (1 << 32) - 1
_HEX = re.compile(r"[0-9a-f]{64}\Z")
_WRAPPERS = frozenset(("metrics.stdout", "fixture.stderr", ".wrapper-result.json"))
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


def _uint(value, maximum=_U64, minimum=0) -> bool:
    return type(value) is int and minimum <= value <= maximum


def _pairs(items):
    result = {}
    for key, value in items:
        need(key not in result, "Duplicate CPU transport key")
        result[key] = value
    return result


def _reject_number(_):
    raise ValueError("CPU transport float/nonfinite value")


def _parse(raw: bytes, *, wrapper=False):
    cap, depth_cap, string_cap = (WRAPPER_LIMIT, 8, 2048) if wrapper else (METRICS_LIMIT, 12, 256)
    need(type(raw) is bytes and len(raw) <= cap, "CPU JSON byte cap/type")
    text = raw.decode("utf-8", "strict")
    depth = 0
    quoted = escaped = False
    for character in text:
        if quoted:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                quoted = False
        elif character == '"':
            quoted = True
        elif character in "[{":
            depth += 1
            need(depth <= depth_cap, "CPU JSON depth cap")
        elif character in "]}":
            depth -= 1
            need(depth >= 0, "CPU JSON bracket order")
    need(depth == 0 and not quoted, "Incomplete CPU JSON")

    def integer(token):
        digits = token[1:] if token.startswith("-") else token
        need(len(digits) <= 20 and (wrapper or not token.startswith("-")),
             "CPU integer sign/width")
        value = int(token)
        need(-_U64 <= value <= _U64, "CPU integer overflow")
        return value

    data = json.loads(text, object_pairs_hook=_pairs, parse_int=integer,
                      parse_float=_reject_number, parse_constant=_reject_number)
    nodes = 0

    def walk(value):
        nonlocal nodes
        nodes += 1
        need(nodes <= 512, "CPU JSON node cap")
        if type(value) is dict:
            for key, child in value.items():
                need(len(key.encode("utf-8", "strict")) <= 256 and "\0" not in key,
                     "CPU JSON key cap")
                walk(child)
        elif type(value) is list:
            for child in value:
                walk(child)
        elif type(value) is str:
            need(len(value.encode("utf-8", "strict")) <= string_cap and "\0" not in value,
                 "CPU JSON string cap")
        else:
            need(type(value) is int or (wrapper and (value is None or type(value) is bool)),
                 "CPU JSON scalar type")

    walk(data)
    need(type(data) is dict, "CPU JSON root type")
    return data


def _windows_files():
    from tools.owned_source_writer_source_winfiles import WindowsFiles
    return WindowsFiles()


def _identity(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def _regular(info):
    return stat.S_ISREG(info.st_mode) and not (getattr(info, "st_file_attributes", 0) & 0x400)


def _read_file(path: Path, expected: int, cap: int) -> bytes:
    need(_uint(expected, cap), "CPU child file size cap")
    if _WINDOWS:
        files = _windows_files()
        if expected:
            raw = files.read_wire(path, expected)
        else:
            # The pinned backend deliberately rejects empty primary wires.
            # Empty optional stderr uses its same native identity family,
            # leaf reparse refusal and no-write/delete sharing, without a CRT
            # fallback or any change to that existing module.
            handle = files._open(path, 0x80000000, 1, 3)
            try:
                before = files._info(handle, path)
                need(before[2] == 0 and files._info(handle, path) == before,
                     "CPU empty file changed")
                other = files._open(path, 0x80000000, 1, 3)
                try:
                    need(files._info(other, path) == before, "CPU empty path changed")
                finally:
                    files._close(other, path)
                raw = b""
            finally:
                files._close(handle, path)
        need(type(raw) is bytes and len(raw) == expected and len(raw) <= cap,
             "CPU native file length")
        return raw
    before = path.lstat()
    need(_regular(before) and before.st_size == expected, "CPU child file type/size")
    flags = (os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
             | getattr(os, "O_NONBLOCK", 0))
    descriptor = os.open(path, flags)
    try:
        opened = os.fstat(descriptor)
        need(_regular(opened) and _identity(opened) == _identity(before),
             "CPU child changed before open")
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            raw = stream.read(cap + 1)
        need(_identity(os.fstat(descriptor)) == _identity(opened),
             "CPU child changed during read")
    finally:
        os.close(descriptor)
    need(len(raw) == expected and len(raw) <= cap, "CPU child incomplete/oversize")
    need(_identity(path.lstat()) == _identity(before), "CPU child path changed")
    return raw


def _wrapper_healthy(raw: bytes) -> None:
    data = _parse(raw, wrapper=True)
    need(set(data) == {"returncode", "timed_out", "spawn_error", "wrapper_error",
                       "overflow", "pipe_errors", "drain_failed"}, "CPU wrapper keys")
    rc = data["returncode"]
    need(rc is None or (type(rc) is int and -(1 << 31) <= rc <= _U32),
         "Actual POSIX/Windows returncode domain")
    for field in ("timed_out", "drain_failed"):
        need(type(data[field]) is bool, "CPU wrapper flag type")
    for field in ("spawn_error", "wrapper_error"):
        need(data[field] is None or type(data[field]) is str, "CPU wrapper error type")
    for field in ("overflow", "pipe_errors"):
        need(type(data[field]) is dict and set(data[field]) == {"stdout", "stderr"},
             "CPU wrapper pipe keys")
    need(all(type(v) is bool for v in data["overflow"].values()), "CPU overflow flag type")
    need(all(v is None or type(v) is str for v in data["pipe_errors"].values()),
         "CPU pipe error type")
    need(rc == 0 and data["timed_out"] is False and data["drain_failed"] is False
         and data["spawn_error"] is None and data["wrapper_error"] is None
         and all(v is False for v in data["overflow"].values())
         and all(v is None for v in data["pipe_errors"].values()),
         "CPU child wrapper failed or has no actual successful return")


def _receipt(data: dict, wire_bytes: int) -> None:
    need(type(data) is dict and set(data) == {
        "begin_status", "run", "seal_status", "owner", "seal_bytes", "copy_status",
        "destination_pointer", "capacity", "written", "required", "guard_bytes",
        "guards_intact", "tail_intact"}, "CPU ReceiptCopy keys")
    need(all(_uint(v) for v in data.values()), "CPU ReceiptCopy integer types")
    need(all(data[k] == 0 for k in ("begin_status", "seal_status", "copy_status"))
         and data["run"] > 0 and data["owner"] > 0, "CPU actual control status/owner")
    need(data["capacity"] == WIRE_LIMIT and data["guard_bytes"] == 16
         and data["guards_intact"] == 1 and data["tail_intact"] == 1,
         "CPU owned copy span/guards")
    need(all(data[k] == wire_bytes for k in ("seal_bytes", "written", "required")),
         "CPU actual written/required/Seal size")
    need(16 <= data["destination_pointer"] <= _U64 - WIRE_LIMIT - 16,
         "CPU owned copy guarded address overflow")


def verify_samples(metrics_raw: bytes, directory: Path, *, compiled_off: bool = False) -> dict:
    """Validate supplied CPU files; no child process is started here."""
    need(type(compiled_off) is bool, "CPU reader profile type")
    data = _parse(metrics_raw)
    need(set(data) == {"schema_version", "purpose", "modes", "frames"}
         and data["schema_version"] == 1 and data["purpose"] == PURPOSE,
         "CPU manifest root keys/version/purpose")
    labels = ("compiled_off",) if compiled_off else MODES
    modes = data["modes"]
    need(type(modes) is list and len(modes) == len(labels), "CPU mode count")
    for mode, label in zip(modes, labels):
        need(type(mode) is dict and set(mode) == {
            "mode", "original_calls", "original_returns", "trace_encoding",
            "original_trace_sha256", "assertions"}, "CPU mode keys")
        need(mode["mode"] == label
             and all(_uint(mode[k], _U32, 1) for k in (
                 "original_calls", "original_returns", "assertions"))
             and mode["original_calls"] == mode["original_returns"]
             and mode["trace_encoding"] == "owned_roles_and_values_v1"
             and type(mode["original_trace_sha256"]) is str
             and _HEX.fullmatch(mode["original_trace_sha256"]), "CPU mode values")
    need(all((m["original_calls"], m["original_returns"], m["original_trace_sha256"])
             == (modes[0]["original_calls"], modes[0]["original_returns"],
                 modes[0]["original_trace_sha256"]) for m in modes),
         "Same-process owned mock schedule differs")
    frames = data["frames"]
    need(type(frames) is list and len(frames) <= WIRE_COUNT_LIMIT
         and len(frames) == (0 if compiled_off else 1), "CPU primary count")
    for frame in frames:
        need(type(frame) is dict and set(frame) == {
            "name", "mode", "file", "bytes", "sha256", "receipt_copy"}, "CPU primary keys")
        need(frame["name"] == "cold_on" and frame["mode"] == "on"
             and frame["file"] == "cold_on.wire.json"
             and _uint(frame["bytes"], WIRE_LIMIT, 1)
             and type(frame["sha256"]) is str and _HEX.fullmatch(frame["sha256"]),
             "CPU primary name/file/size/SHA")
        _receipt(frame["receipt_copy"], frame["bytes"])

    base = Path(directory).resolve(strict=True)  # Host canonicalization, not a nofollow waiver.
    need(base.is_dir(), "CPU output directory")
    names = {frame["file"] for frame in frames}
    entries = {}
    with os.scandir(base) as listing:
        for entry in listing:
            need(len(entries) < 12, "CPU directory entry cap")
            need(entry.name in names or entry.name in _WRAPPERS,
                 "Unexpected CPU artifact")
            need(entry.is_file(follow_symlinks=False) and not entry.is_symlink(),
                 "CPU artifact is not a regular child file")
            entries[entry.name] = entry.stat(follow_symlinks=False).st_size
    need(names <= set(entries), "Missing CPU primary file")
    if "metrics.stdout" in entries:
        saved = _read_file(base / "metrics.stdout", entries["metrics.stdout"], METRICS_LIMIT)
        need(saved == metrics_raw, "Persisted CPU metrics differ")
    if "fixture.stderr" in entries:
        _read_file(base / "fixture.stderr", entries["fixture.stderr"], STDERR_LIMIT)
    if ".wrapper-result.json" in entries:
        _wrapper_healthy(_read_file(base / ".wrapper-result.json",
                                   entries[".wrapper-result.json"], WRAPPER_LIMIT))
    primaries = []
    for frame in frames:
        raw = _read_file(base / frame["file"], frame["bytes"], WIRE_LIMIT)
        digest = hashlib.sha256(raw).hexdigest()
        need(digest == frame["sha256"], "CPU primary raw SHA differs")
        primaries.append((frame, raw, digest))
    # All files and the persisted real wrapper result precede any model call.
    model_path = ROOT / "tools" / "owned_source_writer_validate.py"
    need(hashlib.sha256(model_path.read_bytes()).hexdigest() == MODEL_SHA256,
         "Frozen CPU model changed")
    reports = []
    for frame, raw, digest in primaries:
        result = analyze(raw)  # Exactly once per actual primary.
        need(type(result) is dict and result.get("historical_declared_bound_view_known") is False,
             "Faithful nested NULL-setter profile must remain unknown")
        need(type(result.get("claims")) is dict and set(result["claims"]) == CLAIMS
             and all(v is False for v in result["claims"].values()), "Stronger CPU claims")
        need(len(_encoded(result)) <= MODEL_REPORT_LIMIT, "Complete CPU model report cap")
        reports.append({"name": frame["name"], "mode": frame["mode"],
                        "wire_bytes": len(raw), "raw_sha256": digest,
                        "receipt_copy": frame["receipt_copy"], "result": result})
    result = {"schema_version": 1, "purpose": "owned_source_writer_projection_CPU_bridge",
              "fixture_CPU_protocol_verified": True, "compiled_off": compiled_off,
              "modes": modes, "analyze_invocations": len(reports), "reports": reports,
              "runtime_or_vendor_facts_attested": False, "actual_SOURCE_hook_executed": False}
    need(len(_encoded(result)) <= REPORT_LIMIT, "Complete CPU reader report cap")
    return result


def _write_new(path: Path, raw: bytes) -> None:
    need(type(raw) is bytes and len(raw) <= REPORT_LIMIT, "CPU output byte cap/type")
    if _WINDOWS:
        _windows_files().write_new(path, raw)
        return
    flags = (os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0)
             | getattr(os, "O_NOFOLLOW", 0))
    descriptor = os.open(path, flags, 0o600)
    with os.fdopen(descriptor, "wb") as stream:
        need(stream.write(raw) == len(raw), "Incomplete CPU output write")


def _error(error: Exception) -> str:
    # Bound UTF8 bytes, not just Python character count.
    try:
        message = f"{type(error).__name__}: {error}"
    except Exception:
        message = f"{type(error).__name__}: unprintable error"
    return message.encode("utf-8", "replace")[:2048].decode("utf-8", "ignore")


def run_fixture(fixture: Path, directory: Path, *, compiled_off: bool = False) -> dict:
    """Own one selected CPU child, persist its actual result, then verify once."""
    need(type(compiled_off) is bool, "CPU reader profile type")
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    directory = directory.resolve(strict=True)
    buffers = {"stdout": bytearray(), "stderr": bytearray()}
    limits = {"stdout": METRICS_LIMIT, "stderr": STDERR_LIMIT}
    outcome = {"returncode": None, "timed_out": False, "spawn_error": None,
               "wrapper_error": None, "overflow": {"stdout": False, "stderr": False},
               "pipe_errors": {"stdout": None, "stderr": None}, "drain_failed": False}
    lock = threading.Lock()
    child = None
    workers = []
    try:
        child = subprocess.Popen([str(Path(fixture).resolve(strict=True)), "--emit-files", str(directory)],
                                 stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, shell=False)

        def capture(label, stream):
            try:
                while True:
                    with lock:
                        room = limits[label] - len(buffers[label])
                    chunk = stream.read(min(4096, room + 1))
                    if not chunk:
                        break
                    need(type(chunk) is bytes, "CPU pipe returned nonbytes")
                    with lock:
                        buffers[label].extend(chunk[:room])
                        overflow = len(chunk) > room
                        outcome["overflow"][label] = outcome["overflow"][label] or overflow
                    if overflow:
                        try:
                            child.kill()
                        except OSError:
                            pass
                        break
            except Exception as error:
                with lock:
                    outcome["pipe_errors"][label] = _error(error)
            finally:
                try:
                    stream.close()
                except Exception as error:
                    with lock:
                        outcome["pipe_errors"][label] = _error(error)

        for label, stream in (("stdout", child.stdout), ("stderr", child.stderr)):
            need(stream is not None, "CPU child pipe missing")
            worker = threading.Thread(target=capture, args=(label, stream), daemon=True)
            worker.start()
            workers.append(worker)
        try:
            outcome["returncode"] = child.wait(timeout=FIXTURE_TIMEOUT)
        except subprocess.TimeoutExpired:
            outcome["timed_out"] = True
    except Exception as error:
        outcome["spawn_error" if child is None else "wrapper_error"] = _error(error)
    finally:
        if child is not None and outcome["returncode"] is None:
            try:
                child.kill()
            except Exception as error:
                outcome["wrapper_error"] = _error(error)
            try:
                outcome["returncode"] = child.wait(timeout=DRAIN_TIMEOUT)
            except Exception as error:
                outcome["wrapper_error"] = _error(error)
            if outcome["returncode"] is None and outcome["wrapper_error"] is None:
                outcome["wrapper_error"] = "No actual child returncode was obtained"
        deadline = time.monotonic() + DRAIN_TIMEOUT
        drain_failed = False
        for worker in workers:
            try:
                worker.join(max(0, deadline - time.monotonic()))
            except Exception as error:
                outcome["wrapper_error"] = _error(error)
                drain_failed = True
        for worker in workers:
            try:
                drain_failed = worker.is_alive() or drain_failed
            except Exception as error:
                outcome["wrapper_error"] = _error(error)
                drain_failed = True
        with lock:
            outcome["drain_failed"] = drain_failed
            captured = {key: bytes(value) for key, value in buffers.items()}
            persisted = _encoded(outcome)
        need(len(persisted) <= WRAPPER_LIMIT, "Complete CPU wrapper record cap")
        # Persist BEFORE checking the real status or parsing any supplied data.
        _write_new(directory / "metrics.stdout", captured["stdout"])
        _write_new(directory / "fixture.stderr", captured["stderr"])
        _write_new(directory / ".wrapper-result.json", persisted)
    _wrapper_healthy(persisted)
    return verify_samples(captured["stdout"], directory, compiled_off=compiled_off)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--off-fixture", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    artifacts = Path(tempfile.mkdtemp(prefix="d4vr-projection-cpu-"))
    try:
        main_report = run_fixture(args.fixture, artifacts / "main")
        if args.off_fixture is None:
            report = main_report
        else:
            off = run_fixture(args.off_fixture, artifacts / "off", compiled_off=True)
            report = {"schema_version": 1, "purpose": "owned_source_writer_projection_CPU_pair",
                      "main": main_report, "compiled_off": off,
                      "runtime_or_vendor_facts_attested": False}
        raw = _encoded(report)
        need(len(raw) <= REPORT_LIMIT, "Complete CPU pair report cap")
        if args.output is None:
            sys.stdout.buffer.write(raw + b"\n")
        else:
            _write_new(args.output, raw)
        return 0
    except Exception as error:
        print(_error(error), file=sys.stderr)
        return 1
    finally:
        # Keep the actual child streams/status even when verification fails.
        print(f"CPU artifacts retained at {artifacts}", file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
