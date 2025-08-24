#!/usr/bin/env python3
"""
Phase 0 — Coverage snapshot (where available) and fuzz/test counts.

This script doesn't compute code coverage on MSVC by default, but it
produces a fast snapshot of the test inventory using `ctest -N`, grouped
by common prefixes (e.g., test_audio_vfx_*, test_equipment_*, etc.).

Outputs:
  build/reports/code_audit_YYYYMMDD/tests_overview.txt
  build/reports/code_audit_YYYYMMDD/tests_overview.json
"""
from __future__ import annotations
import subprocess, json, datetime, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"


def ctest_list() -> list[str]:
    try:
        p = subprocess.run(
            ["ctest", "--test-dir", "build", "-C", "Debug", "-N"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        out = p.stdout
    except Exception as e:
        out = str(e)
    return out.splitlines()


LINE_RE = re.compile(r"^\s*Test\s*#\d+\s*:\s*(\S+)")


def parse_tests(lines: list[str]) -> list[str]:
    names: list[str] = []
    for line in lines:
        m = LINE_RE.match(line)
        if m:
            names.append(m.group(1))
    return names


def group_by_prefix(names: list[str]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for n in names:
        # Use first two tokens as suite hint when available
        # e.g., test_audio_vfx_phase4_1 -> test_audio_vfx
        parts = n.split("_")
        if len(parts) >= 3:
            key = (
                "_".join(parts[:3])
                if parts[2].startswith("phase")
                else "_".join(parts[:2])
            )
        elif len(parts) >= 2:
            key = "_".join(parts[:2])
        else:
            key = parts[0]
        counts[key] = counts.get(key, 0) + 1
    return counts


def write_reports(names: list[str], groups: dict[str, int]) -> None:
    stamp = datetime.datetime.now().strftime("%Y%m%d")
    out_dir = BUILD / "reports" / f"code_audit_{stamp}"
    out_dir.mkdir(parents=True, exist_ok=True)
    # text
    lines = [f"Total tests: {len(names)}", "", "By group:"]
    for k, v in sorted(groups.items()):
        lines.append(f"- {k}: {v}")
    (out_dir / "tests_overview.txt").write_text("\n".join(lines))
    # json
    (out_dir / "tests_overview.json").write_text(
        json.dumps(
            {
                "total": len(names),
                "groups": groups,
                "names": names,
            },
            indent=2,
        )
    )


def main() -> int:
    lines = ctest_list()
    names = parse_tests(lines)
    groups = group_by_prefix(names)
    write_reports(names, groups)
    print(f"Summarized {len(names)} tests across {len(groups)} groups")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
