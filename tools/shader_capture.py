#!/usr/bin/env python3
"""Collect VKD3D shader dumps for one Linux launcher session; keep game DLLs intact."""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys

import probe


def launcher_processes() -> dict[str, int]:
    script = """import pathlib,json
counts={}
wanted={'battle.net.exe','battle.net launcher.exe','agent.exe','diablo iv.exe','diablo iv launcher.exe'}
for p in pathlib.Path('/proc').glob('[0-9]*'):
 try:
  names={a.decode(errors='replace').replace('\\\\','/').rsplit('/',1)[-1].casefold() for a in (p/'cmdline').read_bytes().split(b'\\0')}
  for name in names & wanted: counts[name]=counts.get(name,0)+1
 except OSError: pass
print(json.dumps(counts))
"""
    bridge = Path("/run/host/usr/bin/flatpak-spawn")
    command = [str(bridge), "--host", "python3", "-c", script] if bridge.exists() else [sys.executable, "-c", script]
    result = subprocess.run(command, capture_output=True, text=True, timeout=15, check=True)
    return json.loads(result.stdout)


def inventory(directory: Path) -> dict:
    counts = Counter()
    total = 0
    for item in directory.iterdir():
        if not item.is_file() or item.is_symlink():
            continue
        counts[item.suffix.casefold()] += 1
        total += item.stat().st_size
    return {"files": sum(counts.values()), "bytes": total, "by_extension": dict(sorted(counts.items())),
            "live_camera_values_captured": False, "native_stereo_proven": False}


def prepare(game: Path, launcher: Path, output: Path, expected: dict) -> dict:
    game, launcher, output = game.resolve(), launcher.resolve(), output.resolve()
    if os.name != "posix" or sys.platform != "linux":
        raise ValueError("Le lanceur de capture VKD3D est réservé à Linux")
    if not launcher.is_file():
        raise ValueError("Fournir le script de lancement autonome exporté par Lutris")
    if output == game or game in output.parents:
        raise ValueError("La capture doit être enregistrée hors du répertoire du jeu")
    if output.exists() and any(output.iterdir()):
        raise ValueError("Le répertoire de capture doit être vide pour distinguer ce relevé")
    probe.check_target(game, expected)
    if any((game / name).exists() for name in (probe.BACKUP, probe.STATE, probe.CONFIG)):
        raise ValueError("Restaurer la sonde Streamline avant cette capture")
    if launcher_processes():
        raise ValueError("Fermer Diablo IV et Battle.net, y compris son icône de notification, avant ce lancement")
    shaders, cache = output / "shaders", output / "cache"
    shaders.mkdir(parents=True)
    cache.mkdir()
    variables = {"VKD3D_SHADER_DUMP_PATH": probe.windows_path(shaders),
                 "VKD3D_SHADER_CACHE_PATH": probe.windows_path(cache),
                 "VKD3D_DEBUG": "info", "VKD3D_LOG_FILE": probe.windows_path(output / "vkd3d.log")}
    manifest = {"schema_version": 1, "created_utc": datetime.now(timezone.utc).isoformat(),
                "game_version": expected["game_version"], "game_directory": str(game),
                "launcher": str(launcher), "variables": variables,
                "game_files_changed": False, "phase": "prepared"}
    probe.write_json(output / "capture.json", manifest)
    return manifest


def run(game: Path, launcher: Path, output: Path) -> int:
    expected = probe.profile()
    manifest = prepare(game, launcher, output, expected)
    output = output.resolve()
    assignments = [key + "=" + value for key, value in manifest["variables"].items()]
    bridge = Path("/run/host/usr/bin/flatpak-spawn")
    command = ([str(bridge), "--host", "/usr/bin/env"] if bridge.exists() else ["/usr/bin/env"])
    command += assignments + ["/usr/bin/bash", str(launcher.resolve())]
    print("Lancement Battle.net avec relevé VKD3D ; entrer en partie puis se déplacer 30 secondes.", flush=True)
    print("La compilation utilise un cache temporaire et peut allonger ce démarrage.", flush=True)
    log = output / "launcher-private.log"
    with log.open("wb") as stream:
        log.chmod(0o600)
        manifest["phase"] = "launching"
        probe.write_json(output / "capture.json", manifest)
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    manifest["phase"] = "launcher_exited"
    manifest["launcher_exit_code"] = result.returncode
    manifest["inventory"] = inventory(output / "shaders")
    probe.write_json(output / "capture.json", manifest)
    print(json.dumps(manifest["inventory"], indent=2))
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    launch = sub.add_parser("run")
    launch.add_argument("--game-dir", type=Path, required=True)
    launch.add_argument("--launcher", type=Path, required=True)
    launch.add_argument("--output", type=Path, required=True)
    status = sub.add_parser("status")
    status.add_argument("capture_directory", type=Path)
    args = parser.parse_args()
    if args.command == "run":
        return run(args.game_dir, args.launcher, args.output)
    print(json.dumps(inventory(args.capture_directory / "shaders"), indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"D4VR capture : {error}", file=sys.stderr)
        raise SystemExit(2)
