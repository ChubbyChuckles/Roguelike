#!/usr/bin/env python3
"""
Phase 1.5 — Dependency rule checker (forbid upward deps across layers).

Scans project-internal #include "..." edges and enforces a simple layering:
UI (highest) → Integration Bridges → Gameplay → Systems → Engine Core (lowest)

Exit non-zero on violations and print a concise report.
"""
from __future__ import annotations
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"

INCLUDE_RE = re.compile(r"^\s*#\s*include\s*\"([^\"]+)\"", re.M)

# Layer indices: smaller = lower-level; includes must go downward or same layer
LAYER_ORDER = [
    ("core/integration", 1),  # Engine Core (event/snapshot/tx/persist/platform etc.)
    ("core/persistence", 1),
    ("core/progression", 2),  # Systems/Game core bits — adjust as needed
    ("core/crafting", 2),
    ("core/vendor", 2),
    ("core/equipment", 2),
    ("core/loot", 2),
    ("ai", 3),  # Gameplay/AI layer
    ("game", 3),  # Combat/skills runtime bits live here too
    ("ui", 4),  # UI
]


def classify(path: Path) -> int:
    try:
        rel = path.relative_to(SRC).as_posix()
    except ValueError:
        return -1
    for prefix, lvl in LAYER_ORDER:
        if rel.startswith(prefix + "/") or rel == prefix:
            return lvl
    # Default: engine-ish base, lowest layer (0) to reduce false positives
    return 0


def resolve_include(src_file: Path, inc: str) -> Path | None:
    # Try relative to including file first
    cand = (src_file.parent / inc).resolve()
    if cand.exists():
        return cand
    # Try under src root
    cand = (SRC / inc).resolve()
    if cand.exists():
        return cand
    return None


def project_sources() -> list[Path]:
    return [p for p in SRC.rglob("*.*") if p.suffix in (".c", ".h")]


def main() -> int:
    sources = project_sources()
    violations: list[tuple[str, str, int, int]] = []

    for sf in sources:
        try:
            text = sf.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            continue
        for m in INCLUDE_RE.finditer(text):
            inc = m.group(1)
            tgt = resolve_include(sf, inc)
            if not tgt:
                continue  # external or missing include; ignore
            la = classify(sf)
            lb = classify(tgt)
            # Upward dep if including file layer is lower than target layer
            if la >= 0 and lb >= 0 and la < lb:
                violations.append(
                    (
                        sf.relative_to(ROOT).as_posix(),
                        tgt.relative_to(ROOT).as_posix(),
                        la,
                        lb,
                    )
                )

    if violations:
        print("Dependency layering violations (including -> included):")
        for a, b, la, lb in sorted(violations):
            # Use ASCII arrows only to avoid Windows console encoding issues
            print(f"  [{la}->{lb}] {a} -> {b}")
        print(f"Total: {len(violations)} violation(s).")
        return 2
    print("Layering check passed: no upward dependencies detected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
