#!/usr/bin/env python3
"""Pre-commit helper: fast build + test (and optional coverage) gate.

Behavior:
 - Configures CMake (Debug, SDL OFF) into build/precommit/ if missing.
 - Builds core + tests.
 - Runs ctest (verbose on failure).
 - If --coverage-only or env ROGUE_PRECOMMIT_COVERAGE=1, rebuild with
   coverage flags (ROGUE_CODE_COVERAGE=ON) then invokes tools/coverage_check.sh
   when lcov is available (skips gracefully on unsupported platforms).

Designed to be quick; heavy jobs (full matrix, clang-tidy) run in CI.
"""
from __future__ import annotations
import os
import platform
import subprocess
import sys
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
BUILD_BASE = ROOT / "build" / "precommit"


def find_cmake() -> str:
    # Honor explicit path
    env = os.environ.get("CMAKE_EXE")
    if env and Path(env).exists():
        return env
    # Try PATH
    cmake_on_path = shutil.which("cmake")
    if cmake_on_path:
        return cmake_on_path
    # Common Windows install path
    common_win = Path("C:/Program Files/CMake/bin/cmake.exe")
    if common_win.exists():
        return str(common_win)
    raise SystemExit(
        "CMake not found. Install from https://cmake.org/download/ and ensure 'cmake' is in PATH or set CMAKE_EXE env var."
    )


def find_ctest() -> str:
    env = os.environ.get("CTEST_EXE")
    if env and Path(env).exists():
        return env
    which_ctest = shutil.which("ctest")
    if which_ctest:
        return which_ctest
    common_win = Path("C:/Program Files/CMake/bin/ctest.exe")
    if common_win.exists():
        return str(common_win)
    raise SystemExit(
        "ctest not found. Ensure it is installed with CMake and in PATH or set CTEST_EXE env var."
    )


def run(cmd: list[str], cwd: Path = ROOT, check: bool = True) -> int:
    print(f"[pre-commit] $ {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=cwd)
    if check and proc.returncode != 0:
        print(f"Command failed with code {proc.returncode}", file=sys.stderr)
        sys.exit(proc.returncode)
    return proc.returncode


def configure(extra: list[str]):
    BUILD_BASE.mkdir(parents=True, exist_ok=True)
    cmake_exe = find_cmake()
    cmake_cmd = [
        cmake_exe,
        "-S",
        str(ROOT),
        "-B",
        str(BUILD_BASE),
        "-DROGUE_ENABLE_SDL=OFF",
        "-DROGUE_WARNINGS_AS_ERRORS=ON",
    ] + extra
    run(cmake_cmd)


def build():
    cmake_exe = find_cmake()
    run([cmake_exe, "--build", str(BUILD_BASE), "--config", "Debug", "-j", str(os.cpu_count() or 2)])


def ctest():
    ctest_exe = find_ctest()
    run([
        ctest_exe,
        "--test-dir",
        str(BUILD_BASE),
        "-C",
        "Debug",
        "--output-on-failure",
    ])


def coverage():
    cov_extra = ["-DROGUE_CODE_COVERAGE=ON"]
    configure(cov_extra)
    build()
    ctest()
    script = ROOT / "tools" / "coverage_check.sh"
    if script.exists() and os.access(script, os.X_OK) and platform.system() != "Windows":
        run(["bash", str(script)], cwd=ROOT)
    else:
        print("[pre-commit] Skipping lcov enforcement (unavailable on this platform)")


def main():
    coverage_only = False
    if "--coverage-only" in sys.argv:
        coverage_only = True
    if os.environ.get("ROGUE_PRECOMMIT_COVERAGE") == "1":
        coverage_only = True

    if coverage_only:
        coverage()
        return 0

    if not (BUILD_BASE / "CMakeCache.txt").exists():
        configure([])
    build()
    ctest()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
