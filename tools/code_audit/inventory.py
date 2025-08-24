#!/usr/bin/env python3
"""
Phase 0 — Source inventory: map files → logical modules (script + report).

Outputs:
  build/reports/code_audit_YYYYMMDD/inventory.json
  build/reports/code_audit_YYYYMMDD/inventory.csv

Heuristic mapping based on repository layout and the taxonomy in roadmaps/codebase_optimization.txt.
This is intentionally dependency-free (pure stdlib) and Windows-friendly.
"""
from __future__ import annotations
import json, csv, os, sys, datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
INCLUDE = ROOT / "include"
BUILD = ROOT / "build"

# Map top-level directories to canonical modules (best-effort heuristic)
MODULE_MAP = {
    "core": "engine_core",
    "math": "math_geom",
    "audio_vfx": "audio_vfx",
    "ai": "systems_ai",
    "world": "systems_worldgen",
    "ui": "ui",
    "platform": "core_platform",
    "util": "core_base",
    "graphics": "integration",  # renderer/adapters land here for now
    "input": "integration",
    "game": "gameplay",  # umbrella for gameplay glue currently
    "entities": "gameplay",
}

VALID_EXT = {".c", ".h", ".cpp", ".hpp"}


def classify(path: Path) -> str:
    # Determine module from path components
    parts = path.relative_to(ROOT).parts
    # Expected like ("src", "ai", ...)
    if len(parts) >= 2 and parts[0] == "src":
        top = parts[1]
        return MODULE_MAP.get(top, f"unknown:{top}")
    if len(parts) >= 2 and parts[0] == "include":
        # include/rogue/<module>/...
        if len(parts) >= 3 and parts[1] == "rogue":
            return parts[2]
        return "public_headers"
    return "other"


def collect() -> list[dict]:
    records = []
    for base in (SRC, INCLUDE):
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if p.is_file() and p.suffix.lower() in VALID_EXT:
                mod = classify(p)
                size = p.stat().st_size
                records.append(
                    {
                        "path": str(p.relative_to(ROOT)).replace("\\", "/"),
                        "module": mod,
                        "ext": p.suffix.lower(),
                        "bytes": size,
                    }
                )
    return records


def ensure_report_dir() -> Path:
    stamp = datetime.datetime.now().strftime("%Y%m%d")
    out_dir = BUILD / "reports" / f"code_audit_{stamp}"
    out_dir.mkdir(parents=True, exist_ok=True)
    return out_dir


def write_reports(records: list[dict], out_dir: Path) -> None:
    # JSON
    (out_dir / "inventory.json").write_text(json.dumps(records, indent=2))
    # CSV
    with (out_dir / "inventory.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["path", "module", "ext", "bytes"])
        w.writeheader()
        for r in records:
            w.writerow(r)
    # Simple summary
    by_module: dict[str, int] = {}
    for r in records:
        by_module[r["module"]] = by_module.get(r["module"], 0) + 1
    lines = ["Inventory summary by module:"] + [
        f"- {m}: {n} files" for m, n in sorted(by_module.items())
    ]
    (out_dir / "inventory_summary.txt").write_text("\n".join(lines))


def main(argv: list[str]) -> int:
    records = collect()
    out_dir = ensure_report_dir()
    write_reports(records, out_dir)
    print(f"Wrote {len(records)} records to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
