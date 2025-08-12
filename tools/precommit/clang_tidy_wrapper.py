#!/usr/bin/env python3
"""Manual clang-tidy runner for the project.

Usage: pre-commit run clang-tidy-optional --all-files
Requires clang-tidy in PATH and prior build (compile_commands.json).
"""
from __future__ import annotations
import os, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "precommit"

def find_tidy() -> str:
    exe = shutil.which("clang-tidy")
    if not exe:
        print("clang-tidy not found; skipping", file=sys.stderr)
        sys.exit(0)
    return exe

def main():
    compile_db = BUILD / "compile_commands.json"
    if not compile_db.exists():
        print("compile_commands.json missing – run a build first", file=sys.stderr)
        return 1
    tidy = find_tidy()
    sources = [p for p in (ROOT / "src").rglob("*.c")]
    rc = 0
    for src in sources:
        cmd = [tidy, str(src), f"-p={BUILD}"]
        print("[clang-tidy]", " ".join(cmd))
        if subprocess.run(cmd).returncode != 0:
            rc = 1
    return rc

if __name__ == "__main__":
    raise SystemExit(main())
