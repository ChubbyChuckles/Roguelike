#!/usr/bin/env python3
"""Check C/C header files for presence of license header line 1 containing 'MIT License'.
Auto-fix by prepending template if missing when ROGUE_LICENSE_AUTO=1.
"""
from __future__ import annotations
import os, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LICENSE_FILE = ROOT / "LICENSE"
if LICENSE_FILE.exists():
    TEMPLATE = LICENSE_FILE.read_text(encoding="utf-8").splitlines()
else:
    TEMPLATE = ["MIT License"]
TOKEN = "MIT License"

def scan():
    for ext in (".c", ".h"):
        for p in (ROOT / "src").rglob(f"*{ext}"):
            yield p

def has_token(lines):
    return any(TOKEN in l for l in lines[:6])

def main():
    auto = os.environ.get("ROGUE_LICENSE_AUTO") == "1"
    missing = []
    for f in scan():
        try:
            text = f.read_text(encoding="utf-8")
        except OSError:
            continue
        if not has_token(text.splitlines()):
            missing.append(f)
            if auto:
                header = "/*\n" + "\n".join(TEMPLATE) + "\n*/\n"
                f.write_text(header + text, encoding="utf-8")
    if missing and not auto:
        print("Missing license header:")
        for f in missing:
            print(" -", f.relative_to(ROOT))
        print("Set ROGUE_LICENSE_AUTO=1 to auto-add headers.")
        return 1
    if missing and auto:
        print(f"Added license headers to {len(missing)} files.")
    else:
        print("All license headers present.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
