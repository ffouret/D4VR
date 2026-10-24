#!/usr/bin/env python3
"""Optional Windows x64 cross-build using an existing Zig C++ compiler."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "work/native")
    args = parser.parse_args()
    output = args.output.resolve()
    for directory in (output / "probe", output / "fixture", output / "tools"):
        directory.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["ZIG_GLOBAL_CACHE_DIR"] = str(ROOT / "work/zig-cache")
    env["ZIG_LOCAL_CACHE_DIR"] = str(ROOT / "work/zig-local")
    native = ROOT / "native"
    common = [str(args.zig.resolve()), "c++", "-target", "x86_64-windows-gnu", "-std=c++17", "-O2"]
    for directory in (native / "probe", native / "tests", native / "third_party/streamline/include"):
        common += ["-I", str(directory)]
    jobs = [
        (["-shared", str(native / "probe/proxy.cpp"), str(native / "probe/exports.def")], output / "probe/sl.interposer.dll"),
        (["-shared", str(native / "tests/original_fixture.cpp"), str(native / "tests/original_fixture.def")], output / "fixture/d4vr_streamline_original.dll"),
        ([str(native / "tests/forwarding_harness.cpp")], output / "fixture/d4vr_forwarding_harness.exe"),
        ([str(native / "tests/observation_tests.cpp")], output / "fixture/d4vr_observation_tests.exe"),
        ([str(native / "tests/vendor_loader.cpp")], output / "tools/d4vr_vendor_loader.exe"),
    ]
    for source, target in jobs:
        print(f"Building {target.name}", flush=True)
        subprocess.run(common + source + ["-o", str(target)], env=env, check=True)
    shutil.copy2(output / "probe/sl.interposer.dll", output / "fixture/sl.interposer.dll")
    print(f"Probe: {output / 'probe/sl.interposer.dll'}")
    print(f"Synthetic test fixtures (never install in a game): {output / 'fixture'}")


if __name__ == "__main__":
    main()
