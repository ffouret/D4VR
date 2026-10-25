#!/usr/bin/env python3
"""Build a local GPU diagnostic, validate every shader, and run its GPU fixture.

Game-derived bytecode stays in the requested local output directory. This
preparation does not install anything into Diablo IV or change its launcher.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import shutil
import subprocess

import probe
import spirv_probe


def select_candidates(candidates: dict,projection_only: bool=False) -> list[dict]:
    selected=[]
    for candidate in candidates['shaders']:
        final=candidate.get('final_clip_matrix')
        if not final or (final['buffer']['space'],final['buffer']['register'],final['byte_offset'])!=(0,0,1152):continue
        if projection_only:selected.append(candidate);continue
        chain=candidate.get('input_matrix_chain',[])
        if chain and (chain[0]['buffer']['space'],chain[0]['buffer']['register'],chain[0]['byte_offset'])==(0,1,896):selected.append(candidate)
    return selected


def build(args) -> dict:
    projection_only=getattr(args,'projection_only_helper',False)
    if projection_only and not args.stereo:raise ValueError('Projection-only construction requires stereo mode')
    if args.output.exists() and any(args.output.iterdir()):
        raise ValueError("Use an empty directory for this observer bundle")
    args.output.mkdir(parents=True, exist_ok=True)
    args.output.chmod(0o700)
    validation = args.output / "validation"
    patched_fixture = validation / "fixture.observed.spv"
    spirv_probe.build(args.fixture_vertex, args.helper, patched_fixture, args.tools, 3, 0, 72, 1, 56, args.geometry, args.stereo,projection_only,projection_only)
    bridge = Path("/run/host/usr/bin/flatpak-spawn")
    command = [str(bridge), "--host"] if bridge.exists() else []
    command += [str(p.resolve()) for p in (args.gpu_fixture, args.fixture_vertex, patched_fixture, args.fixture_fragment)]
    if args.stereo:
        command.append("--stereo-no-telemetry" if projection_only else "--stereo")
    elif args.geometry:
        command.append("--geometry")
    result = subprocess.run(command, check=True, capture_output=True, text=True, timeout=60)
    proof = json.loads(result.stdout.splitlines()[-1])
    valid = (proof.get("geometry_parallax_verified") is True and proof.get("non_target_projection_unchanged") is True and proof.get("geometry_telemetry_verified") is True
             if args.geometry else proof.get("matrix_values_verified") == 32 and proof.get("sparse_sampling_and_budget_verified") is True)
    if args.stereo:
        valid = (proof.get('stereo_gpu_fixture_passed') is True and proof.get('packet_gpu_updates_verified') is True
                 and proof.get('independent_reference_images_verified') is True and proof.get('inactive_and_non_target_unchanged') is True)
        if projection_only:valid=valid and proof.get('telemetry_disabled_and_buffers_preserved') is True and proof.get('telemetry_samples')==0
    if not valid or (not args.stereo and proof.get("gpu_fixture_passed") is not True):
        raise ValueError("GPU diagnostic fixture did not pass")
    (validation / "gpu-result.json").write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    identities = {s["file"]: s["sha256"] for s in inventory["shaders"]}
    candidates = json.loads(args.candidates.read_text(encoding="utf-8"))
    selected = select_candidates(candidates,projection_only)
    if not selected:
        raise ValueError("No statically matched projection/input-transform pair")
    overrides = args.output / "overrides"
    overrides.mkdir()

    def prepare(candidate):
        name = candidate["file"]
        if Path(name).name != name or Path(name).suffix != ".dxil":
            raise ValueError("Invalid captured shader filename")
        original_dxil, original_spirv = args.shaders / name, args.shaders / Path(name).with_suffix(".spv")
        if probe.digest(original_dxil) != candidate["sha256"] or identities.get(name) != candidate["sha256"]:
            raise ValueError("DXIL identity differs from analyzed capture")
        if probe.digest(original_spirv) != identities.get(original_spirv.name):
            raise ValueError("SPIR-V identity differs from captured inventory")
        output = overrides / original_spirv.name
        spirv_probe.build(original_spirv, args.helper, output, args.tools, 3, 0, 72, 1, 56, args.geometry, args.stereo,projection_only,projection_only)
        return {"file": output.name, "original_sha256": identities[original_spirv.name],
                "override_sha256": probe.digest(output)}

    with ThreadPoolExecutor(max_workers=4) as executor:
        entries = list(executor.map(prepare, selected))
    shutil.rmtree(overrides / "intermediate")
    runtime = args.proton_path / "files/lib/wine/vkd3d-proton/x86_64-windows/d3d12core.dll"
    manifest = {"schema_version": 2, "purpose": "read_only_paired_clip_transform_diagnostic",
                "game_version": probe.profile()["game_version"], "proton_path": str(args.proton_path.resolve()),
                "runtime_module": str(runtime.resolve()), "runtime_module_sha256": probe.digest(runtime),
                "helper_sha256": probe.digest(args.helper), "uniform_descriptor_set": 3,
                "uniform_bindings": [0, 1], "matrix_byte_offsets": [1152, 896],
                "matrices_per_sample": 2, "sample_stride_qualified_invocations": 65536,
                "maximum_matrix_samples": 1024, "gpu_fixture_passed": True,
                "gpu_validation": proof, "entries": entries}
    if args.geometry:
        manifest.update(purpose="geometry_shift_diagnostic", geometry_eye_offset_view_units=0.10,
                        projection_filter_vertical_fov_degrees=35, projection_filter_aspect_ratio=16 / 9,
                        maximum_matrix_samples=0, matrices_per_sample=0, geometry_telemetry=True,
                        maximum_geometry_samples=1024)
    if args.stereo:
        manifest.update(purpose='eye_transform_diagnostic',eye_state_abi=1,eye_state_specialization_id=4,
                        projection_filter_vertical_fov_degrees=35,projection_filter_aspect_ratio=16/9,
                        maximum_matrix_samples=0,matrices_per_sample=0,stereo_telemetry=True,maximum_stereo_samples=1024)
    if projection_only:
        manifest.update(projection_only_helper=True,helper_no_telemetry=True,uniform_bindings=[0],matrix_byte_offsets=[1152],
                        stereo_telemetry=False,maximum_stereo_samples=0,selection_requires_input_matrix_chain=False)
    probe.write_json(args.output / "bundle.json", manifest)
    return {k: v for k, v in manifest.items() if k != "entries"} | {"validated_shaders": len(entries)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("shaders", "inventory", "candidates", "helper", "tools", "proton-path", "gpu-fixture",
                   "fixture-vertex", "fixture-fragment", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--geometry", action="store_true", help="Build the visible vertex displacement test")
    modes.add_argument("--stereo", action="store_true", help="Build the controlled per-eye geometry helper")
    parser.add_argument('--projection-only-helper',action='store_true',help='Use the verified telemetry-free stereo helper without requiring an object-matrix binding')
    print(json.dumps(build(parser.parse_args()), indent=2))


if __name__ == "__main__":
    main()
