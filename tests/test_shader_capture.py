from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import shader_capture


class CaptureTests(unittest.TestCase):
    def test_inventory_keeps_pixel_and_camera_claims_false(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "one.dxil").write_bytes(b"abcd")
            (root / "one.spv").write_bytes(b"efgh")
            (root / "ignored").mkdir()
            result = shader_capture.inventory(root)
            self.assertEqual(result["files"], 2)
            self.assertEqual(result["bytes"], 8)
            self.assertEqual(result["by_extension"], {".dxil": 1, ".spv": 1})
            self.assertFalse(result["live_camera_values_captured"])
            self.assertFalse(result["native_stereo_proven"])

    @unittest.skipUnless(sys.platform == "linux", "Linux launcher precondition")
    def test_existing_launcher_session_cannot_silently_miss_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / "game"; game.mkdir()
            launcher = root / "launcher.sh"; launcher.write_text("#!/bin/sh\n")
            with patch.object(shader_capture.probe, "check_target"), \
                    patch.object(shader_capture, "launcher_processes", return_value={"battle.net.exe": 1}):
                with self.assertRaisesRegex(ValueError, "Fermer"):
                    shader_capture.prepare(game, launcher, root / "capture", {"game_version": "test"})
            self.assertFalse((root / "capture").exists())


if __name__ == "__main__":
    unittest.main()
