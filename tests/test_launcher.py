import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("d4vr_launcher", Path(__file__).resolve().parents[1] / "tools/d4vr.py")
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class DiscoveryTests(unittest.TestCase):
    def test_steamvr_symlink_resolves_to_real_manifest(self):
        with tempfile.TemporaryDirectory() as folder:
            home = Path(folder)
            runtime = home / "SteamVR/steamxr_linux64.json"
            runtime.parent.mkdir()
            runtime.write_text(json.dumps({"runtime": {"name": "SteamVR", "library_path": "bin/linux64/vrclient.so"}}))
            active = home / ".config/openxr/1/active_runtime.json"
            active.parent.mkdir(parents=True)
            try:
                active.symlink_to(runtime)
            except OSError:
                self.skipTest("Symbolic links unavailable to this Windows account")
            with patch.dict(os.environ, {}, clear=True), patch.object(launcher.platform, "system", return_value="Linux"):
                self.assertEqual(launcher.steamvr_manifest(home=home), runtime)

    def test_explicit_other_runtime_is_not_silently_used(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "other.json"
            path.write_text(json.dumps({"runtime": {"name": "OtherXR"}}))
            self.assertIsNone(launcher.steamvr_manifest(path))

    def test_lutris_only_reads_game_prefix_and_deduplicates(self):
        with tempfile.TemporaryDirectory() as folder:
            home = Path(folder)
            configs = home / ".local/share/lutris/games"
            configs.mkdir(parents=True)
            text = 'game:\n  exe: "drive_c/Battle.net.exe"\n  prefix: "/games/prefix"\nsystem:\n  env:\n    prefix: /irrelevant\nwine:\n  version: runner\n'
            (configs / "battle.yml").write_text(text)
            (configs / "reference.yaml").write_text(text)
            self.assertEqual(launcher.lutris_prefixes(home), [Path("/games/prefix")])

    def test_build_version_comes_from_active_row(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / ".build.info").write_text('Active!DEC:1|Version!STRING:0|Build Key!HEX:16\n0|old|secret1\n1|3.2.3.73883|secret2\n')
            (root / "Diablo IV.exe").touch()
            info = launcher.game_info(root)
            self.assertEqual(info["version"], "3.2.3.73883")
            self.assertTrue(info["executable_present"])
            self.assertNotIn("secret", json.dumps(info))

    def test_missing_metadata_does_not_claim_a_version(self):
        with tempfile.TemporaryDirectory() as folder:
            info = launcher.game_info(Path(folder))
            self.assertFalse(info["executable_present"])
            self.assertNotIn("version", info)

    def test_missing_engine_provides_setup_instruction(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(launcher, "ROOT", Path(folder)), patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(RuntimeError, "setup"):
                launcher.engine_path(None)


if __name__ == "__main__":
    unittest.main()
