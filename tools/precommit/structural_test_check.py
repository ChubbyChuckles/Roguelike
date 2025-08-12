#!/usr/bin/env python3
"""Ensure each src/*.c (excluding main.c and platform/) has a test referencing it.
Heuristic: test file contains module basename or its header name.
"""
from __future__ import annotations
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
TEST_DIRS = [ROOT / "tests" / "unit", ROOT / "tests" / "integration"]

def gather_modules():
    mods = []
    for p in SRC.rglob("*.c"):
        rel = p.relative_to(SRC)
        if rel.parts[0] == "platform":
            continue
        if p.name == "main.c":
            continue
        mods.append(p.stem)
    return sorted(set(mods))

def test_texts():
    texts = []
    for d in TEST_DIRS:
        if not d.exists():
            continue
        for f in d.rglob("test_*.c"):
            try:
                texts.append(f.read_text(encoding="utf-8", errors="ignore"))
            except OSError:
                pass
    return texts

def main():
    mods = gather_modules()
    texts = test_texts()
    missing = []
    for m in mods:
        header = f"{m}.h"
        if not any((m in t) or (header in t) for t in texts):
            missing.append(m)
    if missing:
        print("Structural test coverage missing for modules:")
        for m in missing:
            print(f" - {m}.c")
        print("Add at least one test referencing each module name or header.")
        return 1
    print("Structural test coverage OK")
    return 0

if __name__ == "__main__":
    sys.exit(main())
