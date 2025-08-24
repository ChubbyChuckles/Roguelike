#!/usr/bin/env python3
"""
Phase 0 — Build time & test baseline (Debug, SDL2, -j8); store report.

Runs cmake --build and ctest with parallelism and records timings.
Outputs CSV under build/reports/code_audit_YYYYMMDD/build_times.csv
"""
from __future__ import annotations
import subprocess, time, datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"


def run(cmd: list[str]) -> tuple[int, float, str]:
    t0 = time.perf_counter()
    try:
        # Force UTF-8 with ignore to avoid Windows cp1252 decode errors on arbitrary output
        p = subprocess.run(
            cmd,
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="ignore",
            check=False,
        )
        dt = time.perf_counter() - t0
        out = (p.stdout or "") + (p.stderr or "")
        return p.returncode, dt, out[-4000:]
    except BaseException as e:
        dt = time.perf_counter() - t0
        name = getattr(e, "__class__", type(e)).__name__
        return 130 if name == "KeyboardInterrupt" else 1, dt, f"{name}: {e}"


def ensure_build_dir() -> None:
    (BUILD).mkdir(parents=True, exist_ok=True)


def main() -> int:
    ensure_build_dir()
    stamp = datetime.datetime.now().strftime("%Y%m%d")
    report_dir = BUILD / "reports" / f"code_audit_{stamp}"
    report_dir.mkdir(parents=True, exist_ok=True)

    # Configure ensures SDL enabled; if cache exists, it will reuse.
    cfg_cmd = [
        "cmake",
        "-S",
        ".",
        "-B",
        "build",
        "-DROGUE_ENABLE_SDL=ON",
        "-DCMAKE_BUILD_TYPE=Debug",
    ]
    code_cfg, t_cfg, _ = run(cfg_cmd)

    # Build Debug with parallel 8
    bld_cmd = ["cmake", "--build", "build", "--config", "Debug", "--parallel", "8"]
    code_bld, t_bld, out_bld = run(bld_cmd)

    # Run tests with -j8
    ctest_cmd = [
        "ctest",
        "--test-dir",
        "build",
        "-C",
        "Debug",
        "-j8",
        "--output-on-failure",
    ]
    code_tst, t_tst, out_tst = run(ctest_cmd)

    # Write CSV
    csv_path = report_dir / "build_times.csv"
    lines = [
        "step,code,seconds",
        f"configure,{code_cfg},{t_cfg:.3f}",
        f"build,{code_bld},{t_bld:.3f}",
        f"ctest,{code_tst},{t_tst:.3f}",
    ]
    csv_path.write_text("\n".join(lines))

    # Minimal summaries for debugging
    (report_dir / "build_tail.txt").write_text(out_bld)
    (report_dir / "ctest_tail.txt").write_text(out_tst)

    print(f"Wrote timing CSV to {csv_path}")
    # Always return 0 to not break local dev loops; failures are visible in CSV and tails
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
