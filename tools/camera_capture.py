#!/usr/bin/env python3
"""Launch one bounded VKD3D matrix observation and parse its typed messages."""
from __future__ import annotations

import argparse
from collections import Counter
import json
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

import probe
import shader_capture

MARKER = 0x44345652
PAIRED_MARKER = 0x44345653
MESSAGE = re.compile(r"Shader: ([0-9a-fA-F]+): Instance (\d+), ID \((\d+), (\d+), (\d+)\):\s*(.*)$")


def describe_projection(rows: list[list[float]]) -> dict:
    """Describe numerical shape only; this cannot identify the rendered pass."""
    if len(rows) != 4 or any(len(row) != 4 for row in rows):
        raise ValueError("Expected four float4 rows")
    flat = [v for row in rows for v in row]
    if not all(math.isfinite(v) for v in flat):
        raise ValueError("Nonfinite matrix")
    close = lambda value, expected=0: abs(value - expected) <= 1e-6
    allowed = {0, 2, 5, 6, 10, 11, 14}
    if (all(close(v) for i, v in enumerate(flat) if i not in allowed)
            and close(flat[14], -1) and flat[0] > 0 and flat[5] > 0):
        result = {"shape": "right_handed_perspective", "vertical_fov_degrees": math.degrees(2 * math.atan(1 / flat[5])),
                  "aspect_ratio": flat[5] / flat[0], "projection_offset_xy": [flat[2], flat[6]]}
        if flat[10] < -1 and flat[11] < 0:
            # Conditional interpretation: right-handed depth range [0, 1].
            result["near_far_if_depth_zero_to_one"] = [flat[11] / flat[10], flat[11] / (flat[10] + 1)]
        return result
    if all(close(flat[i]) for i in (1, 2, 4, 6, 8, 9, 12, 13, 14)) and close(flat[15], 1):
        return {"shape": "axis_aligned_affine_or_orthographic"}
    return {"shape": "unclassified"}


def parse(log: Path) -> dict:
    if log.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("Diagnostic log exceeds 64 MiB")
    groups, rejected = {}, 0
    segment = 0
    with log.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if "Enabling shader debug ring of size:" in line:
                segment += 1
            match = MESSAGE.search(line)
            if not match or int(match[3]) not in (MARKER, PAIRED_MARKER):
                continue
            shader, invocation, marker, sample, row, text = match.groups()
            marker = int(marker)
            row_count = 8 if marker == PAIRED_MARKER else 4
            sample, row = int(sample), int(row)
            if int(invocation) != sample or sample >= 1024 or row >= row_count:
                rejected += 1
                continue
            try:
                values = [float(v.strip()) for v in text.split(",")]
            except ValueError:
                values = []
            if len(values) != 4 or not all(math.isfinite(v) and abs(v) < 1e30 for v in values):
                rejected += 1
                continue
            key = segment, shader.lower().zfill(16), sample, marker
            rows = groups.setdefault(key, {})
            if row in rows:
                # A duplicate is not silently combined with another invocation.
                rows["duplicate"] = True
                rejected += 1
            else:
                rows[row] = values
    samples = []
    for key, rows in sorted(groups.items()):
        row_count = 8 if key[3] == PAIRED_MARKER else 4
        if set(rows) != set(range(row_count)):
            continue
        sample = {"segment": key[0], "shader_hash": key[1], "sample_id": key[2],
                  "matrix_rows": [rows[i] for i in range(4)]}
        sample["projection_description"] = describe_projection(sample["matrix_rows"])
        if row_count == 8:
            sample["input_transform_rows"] = [rows[i] for i in range(4, 8)]
        samples.append(sample)
    distinct = {tuple(v for row in s["matrix_rows"] for v in row) for s in samples}
    return {"schema_version": 2, "complete_matrix_samples": len(samples),
            "paired_matrix_samples": sum("input_transform_rows" in s for s in samples),
            "distinct_matrices_at_log_precision": len(distinct),
            "incomplete_or_duplicate_groups": len(groups) - len(samples), "rejected_rows": rejected,
            "samples_by_shader": dict(Counter(s["shader_hash"] for s in samples)), "samples": samples,
            "projection_shapes": dict(Counter(s["projection_description"]["shape"] for s in samples)),
            "sample_budget_reached": any(s["sample_id"] == 1023 for s in samples),
            "time_coverage_known": False,
            "scene_camera_proven": False, "native_stereo_proven": False}


def validate_bundle(directory: Path) -> dict:
    manifest = json.loads((directory / "bundle.json").read_text(encoding="utf-8"))
    if manifest.get("schema_version") not in (1, 2) or manifest.get("gpu_fixture_passed") is not True:
        raise ValueError("A validated GPU observer bundle is required")
    runtime = Path(manifest["runtime_module"])
    if probe.digest(runtime) != manifest["runtime_module_sha256"]:
        raise ValueError("VKD3D runtime changed since preparation")
    if not manifest["entries"]:
        raise ValueError("Empty observer bundle")
    for entry in manifest["entries"]:
        name = entry["file"]
        if not re.fullmatch(r"[0-9a-f]{16}\.spv", name):
            raise ValueError("Invalid override filename")
        if probe.digest(directory / "overrides" / name) != entry["override_sha256"]:
            raise ValueError("Observer override fingerprint changed")
    return manifest


def run(game: Path, launcher: Path, bundle: Path, output: Path, geometry_test: bool = False) -> int:
    manifest = validate_bundle(bundle)
    if (manifest["purpose"] == "geometry_shift_diagnostic") != geometry_test:
        raise ValueError("The bundle purpose must match the explicit geometry-test mode")
    expected = probe.profile()
    if manifest["game_version"] != expected["game_version"]:
        raise ValueError("Observer bundle targets another game version")
    source = launcher.read_text(encoding="utf-8")
    pattern = r'^export PROTONPATH="GE-Proton"$'
    if len(re.findall(pattern, source, re.M)) != 1:
        raise ValueError("Expected the reviewed Lutris GE-Proton launcher")
    # Pin only this private copy; leave the user's launcher configuration intact.
    replacement = 'export PROTONPATH=' + shlex.quote(manifest["proton_path"])
    source = re.sub(pattern, lambda _: replacement, source, flags=re.M)
    capture = shader_capture.prepare(game, launcher, output, expected)
    output = output.resolve()
    private_launcher = output / "battle-net-camera.sh"
    private_launcher.write_text(source, encoding="utf-8")
    private_launcher.chmod(0o700)
    variables = {"VKD3D_SHADER_OVERRIDE": probe.windows_path(bundle.resolve() / "overrides"),
                 "VKD3D_SHADER_DEBUG_RING_SIZE_LOG2": "24", "VKD3D_SHADER_CACHE_PATH": capture["variables"]["VKD3D_SHADER_CACHE_PATH"],
                 "VKD3D_DEBUG": "info", "VKD3D_LOG_FILE": probe.windows_path(output / "camera.log")}
    if geometry_test:
        del variables["VKD3D_SHADER_DEBUG_RING_SIZE_LOG2"]
    capture.update(variables=variables, observer_bundle=str(bundle.resolve()), phase="launching", game_files_changed=False)
    probe.write_json(output / "capture.json", capture)
    bridge = Path("/run/host/usr/bin/flatpak-spawn")
    command = ([str(bridge), "--host", "/usr/bin/env"] if bridge.exists() else ["/usr/bin/env"])
    command += [key + "=" + value for key, value in variables.items()] + ["/usr/bin/bash", str(private_launcher)]
    if geometry_test:
        print("Battle.net : test visible de déplacement des sommets, pas encore de rendu VR.", flush=True)
        print("Observer le décor et le personnage une minute, puis quitter le jeu et Battle.net.", flush=True)
    else:
        print("Battle.net : sonde GPU bornée, DLL du jeu et de NVIDIA originales.", flush=True)
        print("Entrer en partie, rester immobile 10 secondes, marcher 2 minutes, puis quitter le jeu et Battle.net.", flush=True)
    observations = []
    started = time.monotonic()
    with (output / "launcher-private.log").open("wb") as stream:
        os.chmod(stream.name, 0o600)
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
        while True:
            log = output / "camera.log"
            size = log.stat().st_size if log.exists() else 0
            # CPU observation time bounds log visibility, not GPU execution time.
            observations.append({"elapsed_seconds": round(time.monotonic() - started, 3), "log_bytes": size})
            try:
                exit_code = process.wait(timeout=1)
                break
            except subprocess.TimeoutExpired:
                pass
    probe.write_json(output / "log-observations.json", {"gpu_timestamps": False, "observations": observations})
    capture.update(phase="launcher_exited", launcher_exit_code=exit_code)
    if (output / "camera.log").exists():
        report = parse(output / "camera.log")
        probe.write_json(output / "matrices.json", report)
        capture["complete_matrix_samples"] = report["complete_matrix_samples"]
    probe.write_json(output / "capture.json", capture)
    print(json.dumps(capture, indent=2))
    return exit_code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    launch = sub.add_parser("run")
    launch.add_argument("--geometry-test", action="store_true")
    for name in ("game-dir", "launcher", "bundle", "output"):
        launch.add_argument("--" + name, type=Path, required=True)
    report = sub.add_parser("report")
    report.add_argument("log", type=Path)
    report.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "run":
        return run(args.game_dir, args.launcher, args.bundle, args.output, args.geometry_test)
    result = parse(args.log)
    probe.write_json(args.output, result)
    print(json.dumps({k: v for k, v in result.items() if k != "samples"}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.SubprocessError, KeyError) as error:
        print(f"D4VR camera : {error}", file=sys.stderr)
        raise SystemExit(2)
