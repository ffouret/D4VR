"""Independent transport fakes, not producer or frozen-model test inputs.

Discovery starts no child and never calls the real historical calculation.
The C tests exercise the new private operations separately.
"""
from __future__ import annotations
import copy
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools import owned_source_writer_projection_wire as reader

# Normative 278c names, listed independently of the reader implementation.
EXPECTED_CLAIMS = frozenset((
    "input_facts_attested", "source_runtime_coverage_proven",
    "actual_sampled_input_identity_known", "shader_uses_selected_binding_proven",
    "sampled_resource_GPU_read_proven", "current_descriptor_contents_known",
    "current_input_texels_known", "sampler_known", "game_numeric_UV_known",
    "renderer_owner_known", "backing_known", "loaded_binary_source_equivalence_proven",
    "generic_PE_to_Unix_mapping_known", "global_API_absence_proven", "VR_solution_proven",
    "native_descriptor_or_image_view_binding_proven", "public_device_factory_result_known",
))


def encoded(value):
    return json.dumps(value, ensure_ascii=True, separators=(",", ":")).encode("ascii")


def manifest(raw=b'{"transport_fake_only":1}\n', *, off=False):
    labels = ("compiled_off",) if off else ("on", "observer_bypassed", "faulted")
    modes = [{"mode": label, "original_calls": 91, "original_returns": 91,
              "trace_encoding": "owned_roles_and_values_v1",
              "original_trace_sha256": "ab" * 32, "assertions": 73}
             for label in labels]
    frames = [] if off else [{"name": "cold_on", "mode": "on", "file": "cold_on.wire.json",
        "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest(), "receipt_copy": {
        "begin_status": 0, "run": 123, "seal_status": 0, "owner": 456,
        "seal_bytes": len(raw), "copy_status": 0, "destination_pointer": 0x100000,
        "capacity": 65536, "written": len(raw), "required": len(raw), "guard_bytes": 16,
        "guards_intact": 1, "tail_intact": 1}}]
    return {"schema_version": 1, "purpose": "owned_source_writer_projection_CPU_fixture",
            "modes": modes, "frames": frames}


def child_report():
    # New fake object per call; it is deliberately not an accepted SOURCE input.
    return {"historical_declared_bound_view_known": False,
            "claims": {key: False for key in EXPECTED_CLAIMS},
            "retained_input": {"reader_fake_only": [1, "whole result retained"]},
            "diagnostics": [{"code": "synthetic_unknown", "all_ids": [5, 9, 17]}]}


def wrapper(**changes):
    value = {"returncode": 0, "timed_out": False, "spawn_error": None,
             "wrapper_error": None, "overflow": {"stdout": False, "stderr": False},
             "pipe_errors": {"stdout": None, "stderr": None}, "drain_failed": False}
    value.update(changes)
    return value


class ReaderTransportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="projection-reader-independent-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.raw = b' \n{"transport_fake_only" : [1,2]}\t\n'
        self.data = manifest(self.raw)
        (self.directory / "cold_on.wire.json").write_bytes(self.raw)
        self.fake = child_report()
        self.analyze = mock.patch.object(reader, "analyze", return_value=self.fake).start()
        self.addCleanup(mock.patch.stopall)
        self.no_spawn = mock.patch.object(reader.subprocess, "Popen", side_effect=AssertionError("no discovery child")).start()

    def verify(self, data=None, raw=None, **kwargs):
        return reader.verify_samples(raw if raw is not None else encoded(self.data if data is None else data),
                                     self.directory, **kwargs)

    def refused(self, data=None, raw=None, **kwargs):
        with self.assertRaises((ValueError, OSError)):
            self.verify(data, raw, **kwargs)
        self.analyze.assert_not_called()
        self.no_spawn.assert_not_called()

    def test_exact_raw_bytes_one_call_and_whole_result(self):
        report = self.verify()
        self.analyze.assert_called_once_with(self.raw)
        self.assertEqual(report["reports"][0]["result"], self.fake)
        self.assertIs(report["reports"][0]["result"], self.fake)
        self.assertEqual(report["reports"][0]["raw_sha256"], hashlib.sha256(self.raw).hexdigest())
        self.assertEqual(set(report["reports"][0]["result"]["claims"]), EXPECTED_CLAIMS)
        self.assertEqual(len(EXPECTED_CLAIMS),17)
        self.assertFalse(report["actual_SOURCE_hook_executed"])
        self.assertFalse(report["runtime_or_vendor_facts_attested"])
        self.no_spawn.assert_not_called()

    def test_off_has_no_primary_or_analyze_and_no_cross_process_trace_rule(self):
        (self.directory / "cold_on.wire.json").unlink()
        data = manifest(off=True)
        data["modes"][0]["original_trace_sha256"] = "34" * 32
        report = self.verify(data, compiled_off=True)
        self.assertEqual(report["reports"], [])
        self.assertEqual(report["analyze_invocations"], 0)
        self.analyze.assert_not_called()

    def test_exact_metrics_limit_and_one_over(self):
        base = encoded(self.data)
        self.verify(raw=base + b" " * (8192 - len(base)))
        self.analyze.assert_called_once_with(self.raw)
        self.analyze.reset_mock()
        self.refused(raw=base + b" " * (8193 - len(base)))

    def test_raw_limit_exact_and_oversize(self):
        raw = b" " * 65534 + b"{}"
        (self.directory / "cold_on.wire.json").write_bytes(raw)
        self.verify(manifest(raw))
        self.analyze.assert_called_once_with(raw)
        self.analyze.reset_mock()
        (self.directory / "cold_on.wire.json").write_bytes(raw + b" ")
        self.refused(manifest(raw + b" "))

    def test_closed_root_and_frame_keys(self):
        for target in ("root", "frame", "mode", "receipt"):
            with self.subTest(target=target):
                data = copy.deepcopy(self.data)
                obj = {"root": data, "frame": data["frames"][0], "mode": data["modes"][0],
                       "receipt": data["frames"][0]["receipt_copy"]}[target]
                obj["extra"] = 1
                self.refused(data)

    def test_duplicate_keys_rejected_before_decode_fact_use(self):
        raw = encoded(self.data)
        self.refused(raw=raw.replace(b'"schema_version":1', b'"schema_version":1,"schema_version":1'))

    def test_scalar_types_bool_float_null_nonfinite_and_digit_width(self):
        raw = encoded(self.data)
        for token in (b"true", b"1.0", b"null", b"NaN", b"Infinity", b"-1", b"100000000000000000000", b"18446744073709551616"):
            with self.subTest(token=token):
                self.refused(raw=raw.replace(b'"run":123', b'"run":' + token))

    def test_utf8_surrogate_nul_and_string_budget(self):
        for value in (b"\xff", b'{"x":"\\ud800"}', b'{"x":"\\u0000"}', encoded({"x": "a" * 257})):
            with self.subTest(value=value[:30]):
                self.refused(raw=value)

    def test_depth_and_node_budget_before_closed_shape(self):
        self.refused(raw=b'{"x":' + b"[" * 13 + b"0" + b"]" * 13 + b"}")
        self.refused(raw=encoded({"x": [0] * 513}))

    def test_mode_order_returns_trace_and_encoding(self):
        changes = [(0, "original_returns", 90), (1, "original_calls", 92),
                   (2, "original_trace_sha256", "cd" * 32), (0, "original_trace_sha256", "AB" * 32),
                   (0, "trace_encoding", "raw_pointer_hash"), (0, "assertions", 0),
                   (0, "original_calls", 1 << 32)]
        for i, key, value in changes:
            with self.subTest(key=key, value=value):
                data = copy.deepcopy(self.data); data["modes"][i][key] = value; self.refused(data)
        data = copy.deepcopy(self.data); data["modes"].reverse(); self.refused(data)

    def test_compiled_off_argument_is_real_bool(self):
        for flag in (0, 1, None, "false"):
            with self.subTest(flag=flag): self.refused(compiled_off=flag)

    def test_dynamic_frames_and_path_names_are_not_admitted(self):
        for key, value in (("name", "warm"), ("mode", "faulted"), ("file", "../cold_on.wire.json"), ("file", "/tmp/x")):
            with self.subTest(key=key, value=value):
                data = copy.deepcopy(self.data); data["frames"][0][key] = value; self.refused(data)
        data = copy.deepcopy(self.data); data["frames"].append(copy.deepcopy(data["frames"][0])); self.refused(data)

    def test_every_copy_status_size_guard_and_owner_is_original_strict_scalar(self):
        mutations = {"begin_status": 1, "seal_status": 2, "copy_status": 3, "run": 0, "owner": 0,
                     "seal_bytes": len(self.raw) + 1, "capacity": 65535, "written": 0, "required": 0,
                     "guard_bytes": 15, "guards_intact": 0, "tail_intact": 0}
        for key, value in mutations.items():
            with self.subTest(key=key):
                data = copy.deepcopy(self.data); data["frames"][0]["receipt_copy"][key] = value; self.refused(data)

    def test_copy_guarded_span_checked_at_both_ends(self):
        for pointer in (0, 15, (1 << 64) - 65536 - 16):
            with self.subTest(pointer=pointer):
                data = copy.deepcopy(self.data); data["frames"][0]["receipt_copy"]["destination_pointer"] = pointer
                self.refused(data)
        data = copy.deepcopy(self.data); data["frames"][0]["receipt_copy"]["destination_pointer"] = (1 << 64) - 65536 - 17
        self.verify(data); self.analyze.assert_called_once()

    def test_raw_sha_not_reserialization_and_truncation_refused(self):
        canonical = encoded(json.loads(self.raw))
        data = copy.deepcopy(self.data); data["frames"][0]["sha256"] = hashlib.sha256(canonical).hexdigest(); self.refused(data)
        (self.directory / "cold_on.wire.json").write_bytes(self.raw[:-1]); self.refused()

    def test_all_optional_files_are_validated_before_child_calculation(self):
        (self.directory / "metrics.stdout").write_bytes(encoded(self.data))
        (self.directory / "fixture.stderr").write_bytes(b"\x00\xff")
        (self.directory / ".wrapper-result.json").write_bytes(encoded(wrapper(wrapper_error="late metadata failure")))
        self.refused()

    def test_persisted_metrics_must_equal_supplied_bytes(self):
        (self.directory / "metrics.stdout").write_bytes(encoded(self.data) + b" "); self.refused()

    def test_optional_stderr_empty_or_binary_and_caps(self):
        p = self.directory / "fixture.stderr"
        for raw in (b"", b"\xff" * 65536):
            p.write_bytes(raw); self.verify(); self.analyze.assert_called_once_with(self.raw); self.analyze.reset_mock()
        p.write_bytes(b"x" * 65537); self.refused()

    def test_wrapper_actual_windows_dword_signal_and_unknown_exit_fail_without_conversion(self):
        p = self.directory / ".wrapper-result.json"
        for rc in (0xC0000005, 0xFFFFFFFF, -9, None, True, 1 << 32, -(1 << 31) - 1):
            with self.subTest(rc=rc): p.write_bytes(encoded(wrapper(returncode=rc))); self.refused()

    def test_wrapper_flags_and_error_channels_are_not_truthy_coerced(self):
        p = self.directory / ".wrapper-result.json"
        changes = [{"timed_out": 0}, {"drain_failed": 1}, {"timed_out": True},
                   {"spawn_error": "spawn"}, {"wrapper_error": "wait"},
                   {"overflow": {"stdout": 0, "stderr": False}},
                   {"pipe_errors": {"stdout": "read", "stderr": None}}]
        for update in changes:
            with self.subTest(update=update): p.write_bytes(encoded(wrapper(**update))); self.refused()

    def test_wrapper_key_depth_string_and_duplicate_budgets(self):
        p = self.directory / ".wrapper-result.json"
        for raw in (encoded(wrapper(extra=1)), encoded(wrapper(wrapper_error="e" * 2049)),
                    encoded(wrapper()).replace(b'"returncode":0', b'"returncode":0,"returncode":0'),
                    encoded(wrapper()) + b" " * 16385):
            with self.subTest(raw=raw[:30]): p.write_bytes(raw); self.refused()

    def test_successful_wrapper_preserves_child_full_report(self):
        (self.directory / ".wrapper-result.json").write_bytes(encoded(wrapper()))
        report = self.verify(); self.assertEqual(report["reports"][0]["result"], self.fake)

    def test_unexpected_child_and_nonregular_and_missing_primary(self):
        p = self.directory / "unexpected"; p.write_bytes(b"x"); self.refused(); p.unlink()
        (self.directory / "fixture.stderr").mkdir(); self.refused(); (self.directory / "fixture.stderr").rmdir()
        (self.directory / "cold_on.wire.json").unlink(); self.refused()

    def test_symlink_primary_or_reserved_wrapper_never_followed(self):
        target = self.directory / "cold_on.wire.json"
        saved = target.read_bytes(); target.unlink()
        outside = self.directory.parent / (self.directory.name + "-target")
        outside.write_bytes(saved); self.addCleanup(lambda: outside.unlink(missing_ok=True))
        try: target.symlink_to(outside)
        except OSError as error: self.skipTest(f"symlink capability unavailable: {error}")
        self.refused(); self.assertEqual(outside.read_bytes(), saved); target.unlink(); target.write_bytes(saved)
        (self.directory / "fixture.stderr").symlink_to(outside); self.refused()

    @unittest.skipIf(reader.os.name == "nt", "POSIX identity family, distinct Win32 fakes below")
    def test_posix_changed_open_identity_and_nonblocking_flag(self):
        real_open, real_stat = reader.os.open, reader.os.fstat
        def changed(fd):
            values = list(real_stat(fd)); values[1] += 1; return reader.os.stat_result(values)
        with mock.patch.object(reader, "_WINDOWS", False), mock.patch.object(reader.os, "fstat", side_effect=changed):
            self.refused()
        flags = []
        def observed(path, mode, *rest): flags.append(mode); return real_open(path, mode, *rest)
        with mock.patch.object(reader, "_WINDOWS", False), mock.patch.object(reader.os, "open", side_effect=observed): self.verify()
        self.assertTrue(flags[0] & reader.os.O_NOFOLLOW)
        self.assertTrue(flags[0] & reader.os.O_NONBLOCK)

    def test_create_only_collision_and_dangling_symlink(self):
        p = self.directory / "out.json"; p.write_bytes(b"old")
        with self.assertRaises((ValueError, OSError)): reader._write_new(p, b"new")
        self.assertEqual(p.read_bytes(), b"old"); p.unlink()
        absent = self.directory / "absent"
        try: p.symlink_to(absent)
        except OSError as error: self.skipTest(f"symlink capability unavailable: {error}")
        with self.assertRaises((ValueError, OSError)): reader._write_new(p, b"new")
        self.assertFalse(absent.exists())

    def test_full_model_cap_and_aggregate_cap_refuse_without_truncation(self):
        self.fake["retained_input"]["large"] = "x" * 262144
        with self.assertRaises(ValueError): self.verify()
        self.analyze.assert_called_once_with(self.raw); self.analyze.reset_mock()
        self.fake.pop("retained_input")
        with mock.patch.object(reader, "REPORT_LIMIT", 1):
            with self.assertRaises(ValueError): self.verify()
        self.analyze.assert_called_once_with(self.raw)

    def test_known_or_stronger_claim_or_integer_false_refused(self):
        for key, value in (("historical_declared_bound_view_known", True), ("historical_declared_bound_view_known", 0)):
            with self.subTest(key=key, value=value):
                self.fake[key] = value
                with self.assertRaises(ValueError): self.verify()
                self.analyze.reset_mock()
        self.fake["historical_declared_bound_view_known"] = False
        for value in (True, 0, None):
            with self.subTest(value=value):
                self.fake["claims"]["sampler_known"] = value
                with self.assertRaises(ValueError): self.verify()
                self.analyze.reset_mock()


class NativeEmptyFilesFake:
    def __init__(self, infos=None): self.opens=[]; self.closes=[]; self.infos=iter(infos or [("vol", "id", 0, "basic")] * 3); self.nonempty=[]
    def _open(self, path, access, sharing, disposition):
        handle = len(self.opens)+1; self.opens.append((handle, path, access, sharing, disposition)); return handle
    def _info(self, handle, path): return next(self.infos)
    def _close(self, handle, path): self.closes.append(handle)
    def read_wire(self, path, expected): self.nonempty.append((path, expected)); return b"a" * expected


class WindowsOptionalFileTests(unittest.TestCase):
    def test_empty_uses_two_held_native_handles_and_same_identity_family(self):
        files = NativeEmptyFilesFake()
        with mock.patch.object(reader, "_WINDOWS", True), mock.patch.object(reader, "_windows_files", return_value=files):
            self.assertEqual(reader._read_file(Path("stderr"), 0, 65536), b"")
        self.assertEqual([row[0] for row in files.opens], [1,2])
        self.assertTrue(all(row[2:] == (0x80000000, 1, 3) for row in files.opens))
        self.assertEqual(files.closes, [2,1]); self.assertEqual(files.nonempty, [])

    def test_empty_path_identity_change_or_nonzero_size_refuses_and_closes(self):
        for infos in ([("v","id",1)], [("v","id",0),("v","new",0)],
                      [("v","id",0),("v","id",0),("v","other",0)]):
            with self.subTest(infos=infos):
                files = NativeEmptyFilesFake(infos)
                with mock.patch.object(reader, "_WINDOWS", True), mock.patch.object(reader, "_windows_files", return_value=files):
                    with self.assertRaises(ValueError): reader._read_file(Path("stderr"), 0, 65536)
                self.assertIn(1, files.closes)

    def test_nonempty_calls_frozen_backend_and_checks_new_cap(self):
        files = NativeEmptyFilesFake()
        with mock.patch.object(reader, "_WINDOWS", True), mock.patch.object(reader, "_windows_files", return_value=files):
            self.assertEqual(reader._read_file(Path("stderr"), 3, 65536), b"aaa")
            with self.assertRaises(ValueError): reader._read_file(Path("stderr"), 4, 3)
        self.assertEqual(files.nonempty, [(Path("stderr"),3)]); self.assertEqual(files.opens, [])


class FakeProcess:
    def __init__(self, stdout=b"", stderr=b"", rc=17):
        self.stdout=io.BytesIO(stdout); self.stderr=io.BytesIO(stderr); self.rc=rc; self.waits=[]; self.kills=0
    def wait(self, timeout): self.waits.append(timeout); return self.rc
    def kill(self): self.kills+=1


class InlineWorker:
    def __init__(self, *, target, args, daemon): self.target=target; self.args=args
    def start(self): self.target(*self.args)
    def join(self, timeout): pass
    def is_alive(self): return False


class ProcessStatusFakeTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="projection-process-fake-"); self.addCleanup(self.temp.cleanup)
        self.output=Path(self.temp.name)/"new"
        self.analyze=mock.patch.object(reader,"analyze",side_effect=AssertionError("no real model")).start(); self.addCleanup(mock.patch.stopall)
    def run_fake(self, process, worker=InlineWorker):
        with mock.patch.object(reader.subprocess,"Popen",return_value=process), mock.patch.object(reader.threading,"Thread",worker):
            with self.assertRaises((ValueError, OSError, RuntimeError)): reader.run_fixture(Path(__file__),self.output)
    def outcome(self): return json.loads((self.output/".wrapper-result.json").read_bytes())
    def test_nonzero_windows_dword_preserved_before_verify(self):
        child=FakeProcess(b"not metrics",b"actual stderr",0xC0000005); self.run_fake(child)
        self.assertEqual(self.outcome()["returncode"],0xC0000005)
        self.assertEqual((self.output/"fixture.stderr").read_bytes(),b"actual stderr")
        self.analyze.assert_not_called()
    def test_worker_start_failure_still_waits_and_persists_actual_rc(self):
        class Broken(InlineWorker):
            def start(self): raise RuntimeError("new worker start failure")
        child=FakeProcess(rc=17); self.run_fake(child,Broken)
        value=self.outcome(); self.assertEqual(value["returncode"],17); self.assertIsNone(value["spawn_error"])
        self.assertIn("start failure",value["wrapper_error"]); self.assertEqual(child.kills,1)
        self.analyze.assert_not_called()
    def test_worker_join_failure_cannot_skip_persisted_actual_return(self):
        class Broken(InlineWorker):
            def join(self, timeout): raise RuntimeError("new worker join failure")
        child=FakeProcess(rc=17); self.run_fake(child,Broken)
        value=self.outcome(); self.assertEqual(value["returncode"],17); self.assertIn("join failure",value["wrapper_error"])
        self.analyze.assert_not_called()
    def test_pipe_read_and_close_errors_preclude_false_success(self):
        class Broken:
            def read(self, cap): raise ValueError("new read failure")
            def close(self): raise ValueError("new close failure")
        child=FakeProcess(rc=0); child.stdout=Broken(); self.run_fake(child)
        value=self.outcome(); self.assertEqual(value["returncode"],0); self.assertIn("close failure",value["pipe_errors"]["stdout"])
        self.analyze.assert_not_called()
    def test_overflow_both_streams_bounded_and_real_return_retained(self):
        child=FakeProcess(b"x"*8193,b"y"*65537,17); self.run_fake(child)
        value=self.outcome(); self.assertEqual(value["overflow"],{"stdout":True,"stderr":True}); self.assertEqual(value["returncode"],17)
        self.assertEqual((self.output/"metrics.stdout").stat().st_size,8192); self.assertEqual((self.output/"fixture.stderr").stat().st_size,65536)
        self.analyze.assert_not_called()

    def test_no_actual_return_is_not_fabricated_or_mislabeled_spawn_failure(self):
        child=FakeProcess(rc=None); self.run_fake(child)
        value=self.outcome(); self.assertIsNone(value["returncode"]); self.assertIsNone(value["spawn_error"])
        self.assertIn("No actual",value["wrapper_error"]); self.assertEqual(child.kills,1)
        self.assertEqual(child.waits,[30,5]); self.analyze.assert_not_called()
    def test_actual_timeout_keeps_final_return_and_timeout_flag(self):
        class Timed(FakeProcess):
            def wait(self, timeout):
                self.waits.append(timeout)
                if len(self.waits)==1: raise reader.subprocess.TimeoutExpired("fake_owned_child",timeout)
                return 23
        child=Timed(); self.run_fake(child)
        value=self.outcome(); self.assertEqual(value["returncode"],23); self.assertTrue(value["timed_out"])
        self.assertEqual(child.kills,1); self.analyze.assert_not_called()
    def test_spawn_failure_persists_distinct_error_and_no_return(self):
        with mock.patch.object(reader.subprocess,"Popen",side_effect=OSError("fake spawn unavailable")):
            with self.assertRaises(ValueError): reader.run_fixture(Path(__file__),self.output)
        value=self.outcome(); self.assertIsNone(value["returncode"]); self.assertIn("spawn unavailable",value["spawn_error"])
        self.assertIsNone(value["wrapper_error"]); self.analyze.assert_not_called()
    def test_worker_alive_failure_persists_actual_return(self):
        class Broken(InlineWorker):
            def is_alive(self): raise RuntimeError("new is_alive failure")
        child=FakeProcess(rc=17); self.run_fake(child,Broken)
        value=self.outcome(); self.assertEqual(value["returncode"],17); self.assertIn("is_alive failure",value["wrapper_error"])
        self.assertTrue(value["drain_failed"]); self.analyze.assert_not_called()
    def test_success_persists_before_exactly_one_whole_fake_model_call(self):
        raw=b'{"fresh_process_transport_fake":2}\n'; data=manifest(raw); metrics=encoded(data); child=FakeProcess(metrics,b"",0)
        fresh=child_report()
        def spawn(*args,**kwargs):
            self.assertIs(kwargs["shell"],False)
            (self.output/"cold_on.wire.json").write_bytes(raw)
            return child
        def analyze(received):
            self.assertEqual(received,raw)
            self.assertEqual(json.loads((self.output/".wrapper-result.json").read_bytes())["returncode"],0)
            self.assertEqual((self.output/"metrics.stdout").read_bytes(),metrics)
            return fresh
        self.analyze.side_effect=analyze
        with mock.patch.object(reader.subprocess,"Popen",side_effect=spawn),mock.patch.object(reader.threading,"Thread",InlineWorker):
            result=reader.run_fixture(Path(__file__),self.output)
        self.analyze.assert_called_once_with(raw); self.assertIs(result["reports"][0]["result"],fresh)


if __name__ == "__main__": unittest.main()
