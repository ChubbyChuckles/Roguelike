#!/usr/bin/env python3
"""Reject TODO comments lacking issue reference pattern TODO(#<digits>)."""
from __future__ import annotations
import re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATTERN = re.compile(r"TODO\(#\d+\)")

SCAN_EXT = {".c", ".h", ".md"}

def gather_files():
    for base in ("src", "tests"):
        d = ROOT / base
        if not d.exists():
            continue
        for p in d.rglob("*"):
            if p.is_file() and p.suffix in SCAN_EXT:
                yield p
    for p in ROOT.glob("*.md"):
        yield p

def main():
    bad = []
    for f in gather_files():
        try:
            for i, line in enumerate(f.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
                if "TODO" in line:
                    if not PATTERN.search(line):
                        bad.append((f, i, line.strip()))
        except OSError:
            continue
    if bad:
        print("Found TODOs without issue reference (use TODO(#123): ...):")
        for f, i, txt in bad:
            print(f" - {f.relative_to(ROOT)}:{i}: {txt}")
        return 1
    print("All TODOs properly referenced.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
