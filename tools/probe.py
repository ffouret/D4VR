#!/usr/bin/env python3
"""Stage, install and restore the version-pinned Streamline observation probe."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime, timezone

from pe_image import PEImage

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "native/probe/target_profile.json"
LIBRARY = "sl.interposer.dll"
BACKUP = "d4vr_streamline_original.dll"
STATE = "d4vr-probe-state.json"
CONFIG = "d4vr-probe-output.txt"
INTERCEPTED = {"slInit", "slShutdown", "slSetConstants", "slSetTag", "slSetTagForFrame"}


def digest(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def profile() -> dict:
    return json.loads(PROFILE.read_text(encoding="utf-8"))


def validate_probe(binary: Path, expected: dict) -> None:
    image = PEImage(binary)
    if image.machine != 0x8664 or not image.characteristics & 0x2000:
        raise ValueError("La sonde doit être une DLL Windows x64")
    exports = image.exports()
    if [{"name": e["name"], "ordinal": e["ordinal"]} for e in exports] != expected["exports"]:
        raise ValueError("Les exports de la sonde ne correspondent pas au profil NVIDIA")
    for entry in exports:
        wanted = None if entry["name"] in INTERCEPTED else "d4vr_streamline_original." + entry["name"]
        if entry["forwarder"] != wanted:
            raise ValueError("Forwarder inattendu : " + entry["name"])


def audit(game: Path, expected: dict) -> dict:
    original = game / LIBRARY
    image = PEImage(original)
    exports = [{"name": e["name"], "ordinal": e["ordinal"]} for e in image.exports()]
    return {
        "expected_game_version": expected["game_version"],
        "expected_streamline_version": expected["streamline_version"],
        "original_library_matches": digest(original) == expected["original_sha256"],
        "game_executable_matches": digest(game / "Diablo IV.exe") == expected["game_exe_sha256"],
        "exports_match": exports == expected["exports"],
        "named_exports": len(exports),
        "machine": hex(image.machine),
    }


def check_target(game: Path, expected: dict) -> dict:
    evidence = audit(game, expected)
    if not all(evidence[key] for key in ("original_library_matches", "game_executable_matches", "exports_match")):
        raise ValueError("Version ou bibliothèque différente du profil testé ; installation refusée")
    return evidence


def windows_path(path: Path) -> str:
    absolute = str(path.resolve())
    return absolute if os.name == "nt" else "Z:" + absolute.replace("/", "\\")


def stage(game: Path, binary: Path, destination: Path, expected: dict) -> dict:
    game, destination = game.resolve(), destination.resolve()
    if destination == game or game in destination.parents:
        raise ValueError("Le répertoire d'essai doit être séparé du jeu")
    check_target(game, expected)
    validate_probe(binary, expected)
    destination.mkdir(parents=True, exist_ok=True)
    if any(destination.iterdir()):
        raise ValueError("Le répertoire d'essai doit être vide")
    shutil.copy2(binary, destination / LIBRARY)
    shutil.copy2(game / LIBRARY, destination / BACKUP)
    return {"staging_directory": str(destination), "original_game_files_changed": False, "exports": len(expected["exports"])}


def running_game_processes() -> list[int]:
    if os.name == "nt":
        result = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Diablo IV.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True, check=True)
        return [int(row[1]) for row in csv.reader(result.stdout.splitlines()) if len(row) > 1 and row[0].casefold() == "diablo iv.exe"]
    script = """import pathlib,json
found=[]
for p in pathlib.Path('/proc').glob('[0-9]*'):
 try:
  comm=(p/'comm').read_text().strip().casefold()
  args=(p/'cmdline').read_bytes().split(b'\\0')
  if comm=='diablo iv.exe' or any(a.decode(errors='replace').replace('\\\\','/').rsplit('/',1)[-1].casefold()=='diablo iv.exe' for a in args): found.append(int(p.name))
 except OSError: pass
print(json.dumps(found))
"""
    bridge = Path("/run/host/usr/bin/flatpak-spawn")
    command = [str(bridge), "--host", "python3", "-c", script] if bridge.exists() else [sys.executable, "-c", script]
    result = subprocess.run(command, capture_output=True, text=True, timeout=15, check=True)
    return json.loads(result.stdout)


def write_json(path: Path, value: dict) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def install(game: Path, binary: Path, output: Path, expected: dict) -> dict:
    game, binary, output = game.resolve(), binary.resolve(), output.resolve()
    if running_game_processes():
        raise ValueError("Fermer Diablo IV avant de modifier son installation")
    check_target(game, expected)
    validate_probe(binary, expected)
    if any((game / name).exists() for name in (BACKUP, STATE, CONFIG)):
        raise ValueError("Une sauvegarde ou configuration existe déjà ; ne rien écraser")
    if output == game or game in output.parents:
        raise ValueError("Le journal doit être enregistré hors du répertoire du jeu")
    output.parent.mkdir(parents=True, exist_ok=True)
    config = windows_path(output) + "\n"
    state = {"schema_version": 1, "phase": "prepared", "original_sha256": expected["original_sha256"],
             "probe_sha256": digest(binary), "config_sha256": hashlib.sha256(config.encode()).hexdigest(),
             "log_file": str(output), "installed_at": datetime.now(timezone.utc).isoformat()}
    temporary = game / (LIBRARY + ".d4vr-tmp")
    if temporary.exists() or (game / (STATE + ".tmp")).exists():
        raise ValueError("Un fichier temporaire d'installation existe déjà")
    backup_created = False
    try:
        shutil.copy2(game / LIBRARY, game / BACKUP)
        backup_created = True
        if digest(game / BACKUP) != state["original_sha256"]:
            raise ValueError("La sauvegarde n'est pas identique à l'original")
        shutil.copy2(binary, temporary)
        if digest(temporary) != state["probe_sha256"]:
            raise ValueError("Copie de la sonde incorrecte")
        (game / CONFIG).write_text(config, encoding="utf-8", newline="\n")
        write_json(game / STATE, state)
        os.replace(temporary, game / LIBRARY)
        state["phase"] = "installed"
        write_json(game / STATE, state)
        return state
    except Exception:
        # Roll back only bytes still owned by this installation attempt.
        if backup_created and digest(game / LIBRARY) == state["probe_sha256"]:
            shutil.copy2(game / BACKUP, temporary)
            os.replace(temporary, game / LIBRARY)
        if digest(game / LIBRARY) == state["original_sha256"]:
            for name in (CONFIG, STATE, STATE + ".tmp", LIBRARY + ".d4vr-tmp"):
                (game / name).unlink(missing_ok=True)
            if backup_created:
                (game / BACKUP).unlink(missing_ok=True)
        raise


def uninstall(game: Path) -> dict:
    if running_game_processes():
        raise ValueError("Fermer Diablo IV avant de restaurer sa bibliothèque")
    game = game.resolve()
    state = json.loads((game / STATE).read_text(encoding="utf-8"))
    if state.get("schema_version") != 1:
        raise ValueError("Format de sauvegarde inconnu")
    if digest(game / BACKUP) != state["original_sha256"]:
        raise ValueError("La sauvegarde a changé ; restauration refusée")
    if digest(game / LIBRARY) not in (state["probe_sha256"], state["original_sha256"]):
        raise ValueError("Le jeu a été mis à jour ou modifié ; conserver la sauvegarde et ne rien écraser")
    temporary = game / (LIBRARY + ".d4vr-restore")
    if temporary.exists():
        raise ValueError("Un fichier de restauration existe déjà")
    shutil.copy2(game / BACKUP, temporary)
    if digest(temporary) != state["original_sha256"]:
        raise ValueError("Copie de restauration incorrecte")
    os.replace(temporary, game / LIBRARY)
    (game / BACKUP).unlink()
    if (game / CONFIG).exists() and digest(game / CONFIG) == state["config_sha256"]:
        (game / CONFIG).unlink()
    (game / STATE).unlink()
    return {"restored_original": True, "sha256": digest(game / LIBRARY)}


def summarize(log: Path) -> dict:
    if log.stat().st_size > 8 * 1024 * 1024:
        raise ValueError("Journal trop volumineux pour cette sonde bornée")
    result = {"events": 0, "initializations": 0, "camera_samples": 0, "accepted_plausible_camera_samples": 0, "resource_tags": {},
              "depth_pixels_captured": False, "native_stereo_proven": False}
    with log.open(encoding="utf-8") as stream:
        for line in stream:
            item = json.loads(line)
            if item.get("schema_version") != 1:
                raise ValueError("Schéma de journal inconnu")
            result["events"] += 1
            if item.get("event") == "init": result["initializations"] += 1
            if item.get("event") == "constants":
                result["camera_samples"] += 1
                if item.get("result") == 0 and item.get("metadata_finite_and_plausible") is True:
                    result["accepted_plausible_camera_samples"] += 1
            if item.get("event") == "resource_tag" and item.get("result") == 0 and item.get("resource_present") is True:
                key = str(item.get("tag"))
                result["resource_tags"][key] = result["resource_tags"].get(key, 0) + 1
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("audit", "stage", "install", "uninstall"):
        command = sub.add_parser(name)
        command.add_argument("--game-dir", required=True, type=Path)
        if name in ("stage", "install"):
            command.add_argument("--probe", required=True, type=Path)
            command.add_argument("--output", required=True, type=Path)
    report = sub.add_parser("report")
    report.add_argument("log", type=Path)
    args = parser.parse_args()
    expected = profile()
    if args.command == "audit": result = audit(args.game_dir, expected)
    elif args.command == "stage": result = stage(args.game_dir, args.probe, args.output, expected)
    elif args.command == "install": result = install(args.game_dir, args.probe, args.output, expected)
    elif args.command == "uninstall": result = uninstall(args.game_dir)
    else: result = summarize(args.log)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"D4VR probe : {error}", file=sys.stderr)
        raise SystemExit(2)
