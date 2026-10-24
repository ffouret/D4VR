#!/usr/bin/env python3
"""D4VR development launcher. Standard library only; never modifies game files."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "4.7.2-stable"
ASSETS = {
    "Linux": (
        "Godot_v4.7.2-stable_linux.x86_64.zip",
        "9aa00f7a605200940bce3027a567b782f49bd8e940dd06ae9e987bd65aee1b1467edd56ed84fcdcbdd44354bf613bdbb4e5d2913e925850368e150c59ed54c65",
        "Godot_v4.7.2-stable_linux.x86_64",
    ),
    "Windows": (
        "Godot_v4.7.2-stable_win64.exe.zip",
        "83decd58fdf67b9d657958a1ae6bf1929c20785315a81effe245874cdc57acb709bf868e00778a96984338c1b29dafdb453c6847747694621c6ecf5da2259993",
        "Godot_v4.7.2-stable_win64.exe",
    ),
}


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def steamvr_manifest(extra: Path | None = None, home: Path | None = None) -> Path | None:
    """Find a Valve manifest without modifying the registered OpenXR runtime."""
    home = home or Path.home()
    candidates = []
    if extra:
        candidates.append(extra)
    else:
        if os.environ.get("XR_RUNTIME_JSON"):
            candidates.append(Path(os.environ["XR_RUNTIME_JSON"]))
        if platform.system() == "Windows":
            import winreg
            for hive, key, value_name in (
                (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Khronos\OpenXR\1", "ActiveRuntime"),
                (winreg.HKEY_CURRENT_USER, r"SOFTWARE\Valve\Steam", "SteamPath"),
                (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath"),
            ):
                try:
                    with winreg.OpenKey(hive, key) as entry:
                        value = Path(winreg.QueryValueEx(entry, value_name)[0])
                        candidates.append(value if value.suffix == ".json" else value / "steamapps/common/SteamVR/steamxr_win64.json")
                except OSError:
                    pass
        else:
            config = Path(os.environ.get("XDG_CONFIG_HOME", home / ".config"))
            candidates += [config / "openxr/1/active_runtime.json", Path("/etc/xdg/openxr/1/active_runtime.json")]
            vrpaths = read_json(config / "openvr/openvrpaths.vrpath")
            for root in vrpaths.get("runtime", []):
                candidates.append(Path(root) / "steamxr_linux64.json")
            for root in (home / ".local/share/Steam", home / ".steam/steam", home / ".var/app/com.valvesoftware.Steam/data/Steam"):
                candidates.append(root / "steamapps/common/SteamVR/steamxr_linux64.json")
    for candidate in candidates:
        # Resolve symlinks so relative library_path is relative to the real manifest.
        candidate = candidate.expanduser().resolve()
        runtime = read_json(candidate).get("runtime", {})
        if runtime.get("VALVE_runtime_is_steamvr") or runtime.get("name", "").lower() == "steamvr":
            return candidate
    return None


def lutris_prefixes(home: Path | None = None) -> list[Path]:
    """Read only the top-level game.prefix from simple Lutris YAML entries."""
    home = home or Path.home()
    roots = [home / ".local/share/lutris/games", home / ".config/lutris/games", home / ".var/app/net.lutris.Lutris/data/lutris/games"]
    result = []
    for root in roots:
        for config in sorted(root.glob("*.y*ml")):
            try:
                text = config.read_text(encoding="utf-8")
            except OSError:
                continue
            block = re.search(r"^game:\s*\n((?:[ \t]+[^\n]*\n|\s*\n)*)", text, re.MULTILINE)
            if not block:
                continue
            match = re.search(r"^  prefix:\s*(.+?)\s*$", block[1], re.MULTILINE)
            if match:
                value = match[1].strip().strip("\"'")
                if "$" not in value:
                    path = Path(value).expanduser()
                    if path not in result:
                        result.append(path)
    return result


def game_info(directory: Path) -> dict:
    info = {"directory": str(directory), "executable_present": (directory / "Diablo IV.exe").is_file()}
    try:
        with (directory / ".build.info").open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream, delimiter="|")
            for row in reader:
                values = {key.split("!", 1)[0]: value for key, value in row.items() if key}
                if values.get("Active") == "1":
                    info["version"] = values.get("Version", "unknown")
                    break
    except (OSError, csv.Error):
        pass
    return info


def doctor(game_dir: Path | None = None) -> dict:
    candidates = [game_dir] if game_dir else []
    prefixes = lutris_prefixes()
    for prefix in prefixes:
        for base in ("Program Files (x86)", "Program Files"):
            candidates.append(prefix / "drive_c" / base / "Diablo IV")
    if platform.system() == "Windows":
        for variable in ("ProgramFiles(x86)", "ProgramFiles"):
            if os.environ.get(variable):
                candidates.append(Path(os.environ[variable]) / "Diablo IV")
    manifest = steamvr_manifest()
    return {
        "schema_version": 1,
        "environment_os": platform.system(),
        "architecture": platform.machine(),
        "host_os_file_visible": Path("/run/host/etc/os-release").is_file(),
        "steamvr_manifest": str(manifest) if manifest else None,
        "lutris_prefixes": [str(path) for path in prefixes],
        "diablo_iv": [game_info(path) for path in dict.fromkeys(candidates) if path and path.is_dir()],
        "limitations": ["File discovery does not verify a working GPU or headset session.", "No game injection or memory access is performed.", "This report contains local installation paths; review before publishing."],
    }


def engine_path(explicit: str | None, setup: bool = False) -> Path:
    if explicit or os.environ.get("GODOT_BIN"):
        chosen = explicit or os.environ["GODOT_BIN"]
        path = Path(shutil.which(chosen) or chosen).expanduser().resolve()
        if not path.is_file():
            raise RuntimeError(f"Godot introuvable : {path}")
        return path
    if platform.machine().lower() not in ("x86_64", "amd64") or platform.system() not in ASSETS:
        raise RuntimeError("Fournir --godot pour cette plateforme ; téléchargement fourni uniquement pour Linux/Windows x64.")
    asset, checksum, binary = ASSETS[platform.system()]
    tools = ROOT / "work/tools" / VERSION
    path = tools / binary
    if path.is_file():
        return path
    if not setup:
        raise RuntimeError("Godot n'est pas préparé. Exécuter : python3 tools/d4vr.py setup")
    tools.mkdir(parents=True, exist_ok=True)
    archive = tools / (asset + ".download")
    url = f"https://github.com/godotengine/godot/releases/download/{VERSION}/{asset}"
    print(f"Téléchargement officiel de Godot {VERSION} (~78 Mo)…", flush=True)
    try:
        with urllib.request.urlopen(url, timeout=60) as source, archive.open("wb") as target:
            shutil.copyfileobj(source, target)
        digest = hashlib.sha512(archive.read_bytes()).hexdigest()
        if digest != checksum:
            raise RuntimeError("Somme SHA-512 incorrecte ; archive refusée.")
        with zipfile.ZipFile(archive) as source:
            # Extract only official engine binaries, never arbitrary archive paths.
            for name in (binary, binary.replace(".exe", "_console.exe")):
                if name in source.namelist():
                    (tools / name).write_bytes(source.read(name))
        if not path.exists():
            raise RuntimeError("Le binaire attendu est absent de l'archive officielle.")
        path.chmod(0o755)
    finally:
        archive.unlink(missing_ok=True)
    return path


def run_engine(engine: Path, arguments: list[str], runtime: Path | None = None, validate_log: bool = False) -> int:
    env = os.environ.copy()
    data = ROOT / "work/userdata"
    data.mkdir(parents=True, exist_ok=True)
    if platform.system() != "Windows":
        env["XDG_DATA_HOME"] = str(data)
    if runtime:
        env["XR_RUNTIME_JSON"] = str(runtime)
    result = subprocess.run([str(engine), "--path", str(ROOT / "prototype"), "--audio-driver", "Dummy", *arguments], env=env, check=False, capture_output=validate_log, text=True)
    if validate_log:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        if "SCRIPT ERROR:" in result.stderr or any(line.startswith("ERROR:") for line in result.stderr.splitlines()):
            return result.returncode or 1
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", help="Chemin d'un Godot 4.7.2 existant (ou GODOT_BIN)")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("setup", help="Télécharger le moteur officiel dans work/, avec vérification SHA-512")
    run = commands.add_parser("run", help="Lancer la scène 3D de calibration")
    run.add_argument("--desktop", action="store_true", help="Aperçu explicite sur le moniteur")
    run.add_argument("--runtime", type=Path, help="Manifest OpenXR SteamVR pour ce processus uniquement")
    run.add_argument("--capture", type=Path, help="Capturer l'aperçu bureau en PNG puis quitter")
    commands.add_parser("test", help="Tests de caméra sans casque ni GPU")
    commands.add_parser("smoke", help="Charger la scène complète sans casque ni GPU puis quitter")
    diagnostics = commands.add_parser("doctor", help="Inspection des chemins et versions, en lecture seule")
    diagnostics.add_argument("--game-dir", type=Path)
    diagnostics.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.command == "doctor":
        report = json.dumps(doctor(args.game_dir), indent=2, ensure_ascii=False) + "\n"
        if args.output:
            args.output.write_text(report, encoding="utf-8")
            print(f"Diagnostic enregistré : {args.output}")
        else:
            print(report, end="")
        return 0
    engine = engine_path(args.godot, setup=args.command == "setup")
    if args.command == "setup":
        print(f"Godot prêt : {engine}")
        return 0
    if args.command == "test":
        return run_engine(engine, ["--headless", "--xr-mode", "off", "--script", "res://tests/test_rig.gd"], validate_log=True)
    if args.command == "smoke":
        return run_engine(engine, ["--headless", "--xr-mode", "off", "--quit-after", "45", "--", "--desktop"], validate_log=True)
    if args.capture and not args.desktop:
        raise RuntimeError("--capture nécessite --desktop ; aucune capture du casque n'est prévue par cette option.")
    if args.desktop:
        arguments = ["--xr-mode", "off", "--rendering-method", "gl_compatibility", "--", "--desktop"]
        if args.capture:
            arguments.append(f"--capture={args.capture.resolve()}")
        return run_engine(engine, arguments)
    runtime = steamvr_manifest(args.runtime)
    if not runtime:
        raise RuntimeError("Manifest SteamVR OpenXR introuvable. Activer SteamVR comme runtime OpenXR ou fournir --runtime. Utiliser run --desktop pour l'aperçu bureau.")
    print(f"Runtime OpenXR de ce lancement : {runtime}", flush=True)
    return run_engine(engine, ["--xr-mode", "on"], runtime)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, zipfile.BadZipFile) as error:
        print(f"D4VR : {error}", file=sys.stderr)
        raise SystemExit(2)
