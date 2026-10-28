"""Independent bounded CPU wire-reader controls.

Discovery never compiles or starts a process. Analysis is patched in these
transport tests: their small dummy files are NOT claimed as SOURCE captures.
The separate native C test exercises the real C ABI and exports actual copied
primary bytes; its explicit bridge uses the frozen calculation once per frame.
"""
from __future__ import annotations

import copy
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools import owned_source_writer_source_native_wire as reader

STRONG_CLAIMS = (
    "input_facts_attested", "source_runtime_coverage_proven",
    "actual_sampled_input_identity_known", "shader_uses_selected_binding_proven",
    "sampled_resource_GPU_read_proven", "current_descriptor_contents_known",
    "current_input_texels_known", "sampler_known", "game_numeric_UV_known",
    "renderer_owner_known", "backing_known", "loaded_binary_source_equivalence_proven",
    "generic_PE_to_Unix_mapping_known", "global_API_absence_proven", "VR_solution_proven",
    "native_descriptor_or_image_view_binding_proven", "public_device_factory_result_known",
)


def encoded(value):
    return json.dumps(value, separators=(",", ":"), allow_nan=False).encode("utf-8")


def complete_child(known=True):
    # A patchable model-return sentinel, not a construction of SOURCE facts.
    return {"historical_declared_bound_view_known": known,
            "claims": {name: False for name in STRONG_CLAIMS},
            "retained_complete_sentinel": {"original": ["all", "fields", "stay"],
                                           "null": None, "zero": 0},
            "diagnostics": [{"ids": [71, 5], "message": "keep this entire result"}]}


class ReaderControls(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="d4vr-owned-C-reader-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.raw = b'\n { "transport_unit_dummy" : 1 } \t\n'
        self.metrics = {"schema_version": 1, "purpose": "owned_source_writer_source_CPU_fixture",
                        "modes": [{"mode": "independent_cpu", "original_calls": 3,
                                   "original_trace_sha256": "a" * 64, "assertions": 9}],
                        "frames": []}
        self.expected = {}
        self.add_frame("one", self.raw, True)

    def add_frame(self, name, raw, known):
        (self.directory / (name + ".wire.json")).write_bytes(raw)
        self.metrics["frames"].append({"name": name, "mode": "independent_cpu",
            "file": name + ".wire.json", "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest()})
        self.expected[name] = known

    def verify(self, metrics=None, expected=None):
        return reader.verify_samples(encoded(self.metrics) if metrics is None else metrics,
            self.directory, self.expected if expected is None else expected,
            expected_modes=("independent_cpu",), require_matching_trace=False)

    def refused_before_analysis(self, metrics=None, expected=None):
        with mock.patch.object(reader, "analyze") as analyze:
            with self.assertRaises(ValueError):
                self.verify(metrics, expected)
            analyze.assert_not_called()

    def test_raw_whitespace_exact_and_whole_result_once(self):
        child = complete_child()
        with mock.patch.object(reader, "analyze", return_value=child) as analyze:
            result = self.verify()
        analyze.assert_called_once_with(self.raw)
        self.assertEqual(result["reports"][0]["result"], child)
        self.assertEqual(result["reports"][0]["raw_sha256"], hashlib.sha256(self.raw).hexdigest())
        self.assertEqual(result["analyze_invocations"], 1)
        self.assertIs(result["runtime_or_vendor_facts_attested"], False)

    def test_two_primaries_use_two_whole_distinct_returns(self):
        other = b'{"second_transport_dummy":2}'
        self.add_frame("two", other, False)
        children = [complete_child(True), complete_child(False)]
        with mock.patch.object(reader, "analyze", side_effect=children) as analyze:
            result = self.verify()
        self.assertEqual(analyze.call_args_list, [mock.call(self.raw), mock.call(other)])
        self.assertEqual([x["result"] for x in result["reports"]], children)

    def test_empty_off_has_no_fabricated_wire_or_analysis(self):
        (self.directory / "one.wire.json").unlink()
        self.metrics["frames"] = []
        with mock.patch.object(reader, "analyze") as analyze:
            result = self.verify(expected={})
        analyze.assert_not_called()
        self.assertEqual(result["reports"], [])

    def test_second_bad_sha_prevents_even_first_analysis(self):
        self.add_frame("two", b'{"later":2}', False)
        self.metrics["frames"][1]["sha256"] = "0" * 64
        self.refused_before_analysis()

    def test_same_raw_digest_cannot_replace_real_file_bytes(self):
        (self.directory / "one.wire.json").write_bytes(self.raw.replace(b"1", b"2"))
        self.refused_before_analysis()

    def test_missing_or_truncated_actual_file(self):
        (self.directory / "one.wire.json").write_bytes(self.raw[:-1])
        self.refused_before_analysis()

    def test_wire_limit_exact_and_over_limit(self):
        (self.directory / "one.wire.json").unlink()
        self.metrics["frames"] = []; self.expected = {}
        raw = b" " * 65536
        self.add_frame("one", raw, False)
        with mock.patch.object(reader, "analyze", return_value=complete_child(False)) as analyze:
            self.verify()
        analyze.assert_called_once_with(raw)
        self.metrics["frames"][0]["bytes"] = 65537
        self.refused_before_analysis()

    def test_ninth_primary_is_not_eight_aliases(self):
        for i in range(2, 10):
            self.add_frame("item" + str(i), b"{}", False)
        self.refused_before_analysis()

    def test_duplicate_primary_name_or_filename(self):
        self.metrics["frames"].append(copy.deepcopy(self.metrics["frames"][0]))
        self.refused_before_analysis()

    def test_relative_absolute_and_alias_filename_are_refused(self):
        original = self.metrics["frames"][0]["file"]
        for value in ("../one.wire.json", "./one.wire.json", "/one.wire.json", "ONE.wire.json"):
            with self.subTest(value=value):
                self.metrics["frames"][0]["file"] = value
                self.refused_before_analysis()
        self.metrics["frames"][0]["file"] = original

    def test_strict_uint64_boolean_float_and_nonfinite_metrics(self):
        for value in (True, -1, 1 << 64, 1.0, None):
            with self.subTest(value=value):
                current = copy.deepcopy(self.metrics)
                current["modes"][0]["original_calls"] = value
                self.refused_before_analysis(encoded(current))
        for raw in (b"NaN", b"Infinity", b"-Infinity"):
            self.refused_before_analysis(encoded(self.metrics).replace(b'"original_calls":3', b'"original_calls":' + raw))

    def test_boolean_frame_size_and_schema_version(self):
        for root, key in [(self.metrics, "schema_version"), (self.metrics["frames"][0], "bytes")]:
            old = root[key]; root[key] = True
            self.refused_before_analysis(); root[key] = old

    def test_closed_keys_and_wrong_purpose(self):
        self.metrics["extra"] = 0; self.refused_before_analysis(); del self.metrics["extra"]
        self.metrics["frames"][0]["typed_facts"] = 1
        self.refused_before_analysis(); del self.metrics["frames"][0]["typed_facts"]
        self.metrics["purpose"] = "old_source_transport"
        self.refused_before_analysis()

    def test_duplicate_keys_utf16_invalidutf8_and_incomplete_json(self):
        base = encoded(self.metrics)
        for raw in (base.replace(b'"schema_version":1', b'"schema_version":1,"schema_version":1'),
                    base.decode().encode("utf-16"), b"\xff", base[:-1]):
            with self.subTest(raw=raw[:35]):
                self.refused_before_analysis(raw)

    def test_metrics_raw_depth_string_and_nodes_are_bounded(self):
        self.refused_before_analysis(b" " * 8193)
        self.refused_before_analysis(b"[" * 13 + b"0" + b"]" * 13)
        huge = copy.deepcopy(self.metrics); huge["purpose"] = "z" * 257
        self.refused_before_analysis(encoded(huge))
        huge = copy.deepcopy(self.metrics); huge["extra"] = [0] * 513
        self.refused_before_analysis(encoded(huge))

    def test_mode_and_trace_hash_are_not_boolean_or_unbounded(self):
        for key, value in (("mode", []), ("assertions", 0),
                           ("original_trace_sha256", "A" * 64),
                           ("original_trace_sha256", "a" * 65)):
            current = copy.deepcopy(self.metrics); current["modes"][0][key] = value
            self.refused_before_analysis(encoded(current))

    def test_same_process_trace_comparison_is_explicit(self):
        current = copy.deepcopy(self.metrics)
        current["modes"].append({"mode": "second", "original_calls": 3,
                                "original_trace_sha256": "b" * 64, "assertions": 9})
        with mock.patch.object(reader, "analyze") as analyze:
            with self.assertRaises(ValueError):
                reader.verify_samples(encoded(current), self.directory, self.expected,
                                      expected_modes=("independent_cpu", "second"))
            analyze.assert_not_called()

    def test_stronger_claims_are_all_exact_false(self):
        for name in STRONG_CLAIMS:
            child = complete_child(); child["claims"][name] = True
            with self.subTest(name=name), mock.patch.object(reader, "analyze", return_value=child) as analyze:
                with self.assertRaises(ValueError): self.verify()
                analyze.assert_called_once_with(self.raw)

    def test_literal_relation_bool_and_entire_model_budget(self):
        child = complete_child(); child["historical_declared_bound_view_known"] = 1
        with mock.patch.object(reader, "analyze", return_value=child):
            with self.assertRaises(ValueError): self.verify()
        child = complete_child(); child["retained_complete_sentinel"]["large"] = "x" * 2048
        with mock.patch.object(reader, "analyze", return_value=child), mock.patch.object(reader, "MODEL_REPORT_LIMIT", 1024):
            with self.assertRaises(ValueError): self.verify()

    def test_complete_aggregate_budget_rejects_without_truncation(self):
        child = complete_child()
        with mock.patch.object(reader, "analyze", return_value=child) as analyze, mock.patch.object(reader, "REPORT_LIMIT", 100):
            with self.assertRaises(ValueError): self.verify()
        analyze.assert_called_once_with(self.raw)
        self.assertEqual(child, complete_child())

    def test_missing_unknown_child_fields_are_not_silently_projected(self):
        child = complete_child(); child["claims"]["invented_extra"] = False
        with mock.patch.object(reader, "analyze", return_value=child):
            with self.assertRaises(ValueError): self.verify()

    def test_extra_file_or_wrapper_directory_refused_before_analysis(self):
        extra = self.directory / "unrecorded.wire.json"; extra.write_bytes(b"{}")
        self.refused_before_analysis(); extra.unlink()
        (self.directory / "metrics.stdout").mkdir()
        self.refused_before_analysis()

    def test_symlink_primary_and_reserved_wrapper_refused(self):
        outside = self.directory.parent / (self.directory.name + "-target")
        outside.write_bytes(self.raw); self.addCleanup(lambda: outside.unlink(missing_ok=True))
        path = self.directory / "one.wire.json"; path.unlink()
        try:
            path.symlink_to(outside)
        except (OSError, NotImplementedError) as error:
            self.skipTest("Host cannot create test symlinks: " + str(error))
        self.refused_before_analysis(); path.unlink(); path.write_bytes(self.raw)
        (self.directory / "fixture.stderr").symlink_to(outside)
        self.refused_before_analysis()
        self.assertEqual(outside.read_bytes(), self.raw)

    def test_raw_replacement_between_lstat_and_open_refused(self):
        original_open = reader.os.open
        replaced = False

        def opening(path, flags, *args, **kwargs):
            nonlocal replaced
            if Path(path).name == "one.wire.json" and not replaced:
                replaced = True
                Path(path).rename(self.directory / "old-identity-held")
                Path(path).write_bytes(self.raw)
            return original_open(path, flags, *args, **kwargs)

        with mock.patch.object(reader.os, "open", side_effect=opening):
            self.refused_before_analysis()
        self.assertTrue(replaced)

    def test_raw_size_change_during_read_refused(self):
        original_fstat = reader.os.fstat
        calls = 0

        def fstat(fd):
            nonlocal calls
            value = original_fstat(fd); calls += 1
            if calls == 1:
                with (self.directory / "one.wire.json").open("ab") as f: f.write(b" ")
            return value

        with mock.patch.object(reader.os, "fstat", side_effect=fstat):
            self.refused_before_analysis()

    def test_create_only_wrapper_collision_and_symlink_never_change_target(self):
        target = self.directory / "target"; target.write_bytes(b"keep")
        with self.assertRaises(FileExistsError): reader._write_new(target, b"overwrite")
        self.assertEqual(target.read_bytes(), b"keep")
        link = self.directory / "new_link"
        try: link.symlink_to(self.directory / "absent")
        except (OSError, NotImplementedError) as error:
            self.skipTest("Host cannot create test symlinks: " + str(error))
        with self.assertRaises(FileExistsError): reader._write_new(link, b"new")
        self.assertFalse((self.directory / "absent").exists())


class FakeChild:
    def __init__(self, raw=b"metrics", errors=b"", code=0):
        self.stdout, self.stderr = io.BytesIO(raw), io.BytesIO(errors)
        self.code = code; self.waits = []; self.kills = 0

    def wait(self, timeout=None):
        self.waits.append(timeout); return self.code

    def kill(self): self.kills += 1


class InlineThread:
    def __init__(self, *, target, args, daemon):
        self.target, self.args = target, args

    def start(self): self.target(*self.args)
    def join(self, timeout=None): pass
    def is_alive(self): return False


class FixtureWrapperControls(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="d4vr-owned-C-wrapper-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.fixture = self.base / "fake_fixture"; self.fixture.write_bytes(b"not executed")
        self.output = self.base / "fresh"

    def test_fake_child_actual_return_persisted_before_any_wire_probe(self):
        child = FakeChild(code=17)
        with mock.patch.object(reader.subprocess, "Popen", return_value=child), mock.patch.object(reader.threading, "Thread", InlineThread):
            with self.assertRaises(ValueError): reader.run_fixture(self.fixture, self.output)
        receipt = json.loads((self.output / ".wrapper-result.json").read_bytes())
        self.assertEqual(receipt["returncode"], 17)
        self.assertEqual(child.waits, [30])
        self.assertEqual((self.output / "metrics.stdout").read_bytes(), b"metrics")

    def test_fake_child_stdout_overflow_rejected_with_actual_return(self):
        child = FakeChild(raw=b"x" * 8193, code=19)
        with mock.patch.object(reader.subprocess, "Popen", return_value=child), mock.patch.object(reader.threading, "Thread", InlineThread):
            with self.assertRaises(ValueError): reader.run_fixture(self.fixture, self.output)
        receipt = json.loads((self.output / ".wrapper-result.json").read_bytes())
        self.assertEqual(receipt["returncode"], 19)
        self.assertIs(receipt["stdout_overflow"], True)
        self.assertEqual(receipt["stdout_retained_bytes"], 8192)
        self.assertGreaterEqual(child.kills, 1)

    def test_thread_start_failure_still_waits_and_preserves_real_exit(self):
        class BrokenStart(InlineThread):
            def start(self): raise RuntimeError("controlled thread metadata failure")
        child = FakeChild(code=17)
        with mock.patch.object(reader.subprocess, "Popen", return_value=child), mock.patch.object(reader.threading, "Thread", BrokenStart):
            try: reader.run_fixture(self.fixture, self.output)
            except Exception: pass
        receipt = json.loads((self.output / ".wrapper-result.json").read_bytes())
        self.assertEqual(receipt["returncode"], 17)
        self.assertGreaterEqual(len(child.waits), 1)

    def test_thread_construction_failure_still_owns_actual_child(self):
        child = FakeChild(code=23)
        def broken_construct(**_): raise RuntimeError("controlled constructor failure")
        with mock.patch.object(reader.subprocess, "Popen", return_value=child), mock.patch.object(reader.threading, "Thread", broken_construct):
            with self.assertRaises(ValueError): reader.run_fixture(self.fixture, self.output)
        receipt = json.loads((self.output / ".wrapper-result.json").read_bytes())
        self.assertEqual(receipt["returncode"], 23)
        self.assertEqual(child.waits, [3])
        self.assertIn("constructor failure", receipt["wrapper_error"])

    def test_join_failure_keeps_return_and_rejects_incomplete_wrapper(self):
        class BrokenJoin(InlineThread):
            def join(self, timeout=None): raise RuntimeError("controlled join failure")
        child = FakeChild(code=0)
        with mock.patch.object(reader.subprocess, "Popen", return_value=child), mock.patch.object(reader.threading, "Thread", BrokenJoin):
            with self.assertRaises(ValueError): reader.run_fixture(self.fixture, self.output)
        receipt = json.loads((self.output / ".wrapper-result.json").read_bytes())
        self.assertEqual(receipt["returncode"], 0)
        self.assertEqual(child.waits, [30])
        self.assertIn("join failure", receipt["wrapper_error"])

    def test_read_and_close_errors_do_not_certify_actual_zero_exit(self):
        class BadRead(io.BytesIO):
            def read(self, _=None): raise ValueError("controlled read failure")
        class BadClose(io.BytesIO):
            def close(self): raise ValueError("controlled close failure")
        for number, stream in enumerate((BadRead(), BadClose(b"metrics"))):
            with self.subTest(stream=type(stream).__name__):
                output = self.base / ("stream_failure_" + str(number))
                child = FakeChild(code=0); child.stdout = stream
                with mock.patch.object(reader.subprocess, "Popen", return_value=child), mock.patch.object(reader.threading, "Thread", InlineThread):
                    with self.assertRaises(ValueError): reader.run_fixture(self.fixture, output)
                receipt = json.loads((output / ".wrapper-result.json").read_bytes())
                self.assertEqual(receipt["returncode"], 0)
                self.assertIsNotNone(receipt["pipe_errors"]["stdout"])


if __name__ == "__main__":
    unittest.main()
