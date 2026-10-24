from pathlib import Path
import hashlib
import json
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import probe
from pe_image import PEImage


def small_pe(forward=False):
    data = bytearray(0x1000)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", data, 0x84, 0x8664, 1)
    struct.pack_into("<HH", data, 0x94, 240, 0x2022)
    optional = 0x98
    struct.pack_into("<H", data, optional, 0x20B)
    struct.pack_into("<I", data, optional + 60, 0x200)
    struct.pack_into("<II", data, optional + 112, 0x1000, 0x400)
    struct.pack_into("<IIII", data, optional + 240 + 8, 0xE00, 0x1000, 0xE00, 0x200)
    struct.pack_into("<IIIIII", data, 0x210, 1, 2, 2, 0x1040, 0x1050, 0x1060)
    struct.pack_into("<II", data, 0x240, 0x1500, 0x1100 if forward else 0x1501)
    struct.pack_into("<II", data, 0x250, 0x1080, 0x1090)
    struct.pack_into("<HH", data, 0x260, 0, 1)
    for offset, text in [(0x280, b"slInit\0"), (0x290, b"CreateDXGIFactory\0"), (0x300, b"d4vr_streamline_original.CreateDXGIFactory\0")]:
        data[offset:offset + len(text)] = text
    return bytes(data)


class ProbeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.game = self.root / "game"
        self.game.mkdir()
        self.original = small_pe()
        (self.game / probe.LIBRARY).write_bytes(self.original)
        (self.game / "Diablo IV.exe").write_bytes(b"test executable identity")
        self.binary = self.root / "probe.dll"
        self.binary.write_bytes(small_pe(True))
        self.expected = {"game_version": "test", "streamline_version": "test", "installation_allowed": True,
                         "original_sha256": hashlib.sha256(self.original).hexdigest(),
                         "game_exe_sha256": probe.digest(self.game / "Diablo IV.exe"),
                         "exports": [{"name": "slInit", "ordinal": 1}, {"name": "CreateDXGIFactory", "ordinal": 2}]}
        self.log = self.root / "logs/capture.jsonl"

    def install(self):
        with patch.object(probe, "running_game_processes", return_value=[]):
            return probe.install(self.game, self.binary, self.log, self.expected)

    def test_exports_and_forwarders(self):
        probe.validate_probe(self.binary, self.expected)
        self.assertEqual(PEImage(self.binary).exports()[1]["forwarder"], "d4vr_streamline_original.CreateDXGIFactory")

    def test_known_blocked_profile_cannot_be_installed(self):
        self.expected.update(installation_allowed=False, deployment_status="blocked_by_game_signature_check")
        with self.assertRaisesRegex(ValueError, "signature"):
            self.install()
        self.assertEqual((self.game / probe.LIBRARY).read_bytes(), self.original)
        self.assertFalse((self.game / probe.BACKUP).exists())
        self.assertFalse((self.game / probe.STATE).exists())
        self.assertFalse(self.log.parent.exists())

    def test_truncated_pe_rejected(self):
        self.binary.write_bytes(b"MZ")
        with self.assertRaises(ValueError):
            PEImage(self.binary)

    def test_unknown_game_build_leaves_files_untouched(self):
        (self.game / "Diablo IV.exe").write_bytes(b"updated game")
        with self.assertRaisesRegex(ValueError, "différente"), patch.object(probe, "running_game_processes", return_value=[]):
            probe.install(self.game, self.binary, self.log, self.expected)
        self.assertEqual((self.game / probe.LIBRARY).read_bytes(), self.original)
        self.assertFalse((self.game / probe.BACKUP).exists())

    def test_install_and_restore_preserve_original_bytes(self):
        state = self.install()
        self.assertEqual(state["phase"], "installed")
        self.assertEqual((self.game / probe.BACKUP).read_bytes(), self.original)
        with patch.object(probe, "running_game_processes", return_value=[]):
            self.assertTrue(probe.uninstall(self.game)["restored_original"])
        self.assertEqual((self.game / probe.LIBRARY).read_bytes(), self.original)
        self.assertFalse((self.game / probe.STATE).exists())
        self.assertFalse((self.game / probe.CONFIG).exists())

    def test_do_not_overwrite_preexisting_backup(self):
        (self.game / probe.BACKUP).write_bytes(b"user file")
        with self.assertRaisesRegex(ValueError, "déjà"):
            self.install()
        self.assertEqual((self.game / probe.BACKUP).read_bytes(), b"user file")

    def test_do_not_restore_over_new_game_update(self):
        self.install()
        (self.game / probe.LIBRARY).write_bytes(b"new Blizzard update")
        with patch.object(probe, "running_game_processes", return_value=[]), self.assertRaisesRegex(ValueError, "mis à jour"):
            probe.uninstall(self.game)
        self.assertEqual((self.game / probe.LIBRARY).read_bytes(), b"new Blizzard update")
        self.assertTrue((self.game / probe.BACKUP).exists())

    def test_failed_swap_rolls_back_prepared_installation(self):
        replace = probe.os.replace
        def fail(source, target):
            if Path(source).name == probe.LIBRARY + ".d4vr-tmp":
                raise OSError("simulated swap failure")
            return replace(source, target)
        with patch.object(probe.os, "replace", side_effect=fail), self.assertRaisesRegex(OSError, "simulated"):
            self.install()
        self.assertEqual((self.game / probe.LIBRARY).read_bytes(), self.original)
        self.assertFalse((self.game / probe.STATE).exists())
        self.assertFalse((self.game / probe.BACKUP).exists())

    def test_running_game_is_not_modified(self):
        with patch.object(probe, "running_game_processes", return_value=[123]), self.assertRaisesRegex(ValueError, "Fermer"):
            probe.install(self.game, self.binary, self.log, self.expected)

    def test_staging_keeps_game_unchanged(self):
        destination = self.root / "staging"
        result = probe.stage(self.game, self.binary, destination, self.expected)
        self.assertFalse(result["original_game_files_changed"])
        self.assertEqual((self.game / probe.LIBRARY).read_bytes(), self.original)

    def test_report_does_not_claim_depth_pixels_or_stereo(self):
        self.log.parent.mkdir()
        self.log.write_text(json.dumps({"schema_version": 1, "event": "constants", "metadata_finite_and_plausible": True, "result": 0}) + "\n")
        report = probe.summarize(self.log)
        self.assertEqual(report["accepted_plausible_camera_samples"], 1)
        self.assertFalse(report["native_stereo_proven"])
        self.assertFalse(report["depth_pixels_captured"])


if __name__ == "__main__":
    unittest.main()
