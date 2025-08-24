#!/usr/bin/env python3
"""
Phase 0 — Include/dependency graph: generate DOT + detect cycles.

Scans the source tree for #include lines and builds a graph (file->file).
Outputs:
  build/reports/code_audit_YYYYMMDD/include_graph.json
  build/reports/code_audit_YYYYMMDD/include_graph.dot
  build/reports/code_audit_YYYYMMDD/include_cycles.txt (if any)
"""
from __future__ import annotations
import re, json, datetime
from pathlib import Path
from typing import List, Set, Tuple

ROOT = Path(__file__).resolve().parents[2]
SRC_DIRS = [ROOT / "src", ROOT / "include"]
VALID_EXT = {".c", ".h", ".cpp", ".hpp"}

INCLUDE_RE = re.compile(r"^\s*#\s*include\s*\"([^\"]+)\"")


def gather_files() -> list[Path]:
    files: list[Path] = []
    for base in SRC_DIRS:
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if p.is_file() and p.suffix.lower() in VALID_EXT:
                files.append(p)
    return files


def parse_includes(path: Path) -> list[str]:
    incs: list[str] = []
    try:
        for line in path.read_text(errors="ignore").splitlines():
            m = INCLUDE_RE.match(line)
            if m:
                incs.append(m.group(1))
    except Exception:
        pass
    return incs


def resolve_include(src: Path, inc: str) -> Path | None:
    # Try relative to source file directory first
    local = (src.parent / inc).resolve()
    if local.exists():
        return local
    # Try include/ tree
    cand = (ROOT / "include" / inc).resolve()
    if cand.exists():
        return cand
    # Fallback: search once under src/include
    for base in SRC_DIRS:
        p = (base / inc).resolve()
        if p.exists():
            return p
    return None


def build_graph() -> dict[str, list[str]]:
    graph: dict[str, list[str]] = {}
    files = gather_files()
    for p in files:
        key = str(p.resolve())
        graph.setdefault(key, [])
        for inc in parse_includes(p):
            tgt = resolve_include(p, inc)
            if tgt is not None:
                graph[key].append(str(tgt.resolve()))
    return graph


def detect_cycles(graph: dict[str, list[str]]) -> list[list[str]]:
    cycles: list[list[str]] = []
    WHITE, GRAY, BLACK = 0, 1, 2
    color: dict[str, int] = {n: WHITE for n in graph}
    stack: list[str] = []

    def dfs(u: str):
        color[u] = GRAY
        stack.append(u)
        for v in graph.get(u, []):
            if color.get(v, WHITE) == WHITE:
                dfs(v)
            elif color.get(v) == GRAY:
                # found a cycle; slice stack from v to end
                if v in stack:
                    i = stack.index(v)
                    cyc = stack[i:] + [v]
                    cycles.append(cyc)
        color[u] = BLACK
        stack.pop()

    for node in list(graph.keys()):
        if color[node] == WHITE:
            dfs(node)
    # Deduplicate cycles by normalized tuple
    normd: Set[Tuple[str, ...]] = set()
    uniq: List[List[str]] = []
    for c in cycles:
        # rotate so min string path first
        pivot = min(range(len(c) - 1), key=lambda i: c[i])
        r = c[pivot:-1] + c[:pivot]
        key = tuple(r)
        if key not in normd:
            normd.add(key)
            uniq.append(c)
    return uniq


def write_reports(graph: dict[str, list[str]], cycles: list[list[str]]):
    stamp = datetime.datetime.now().strftime("%Y%m%d")
    out_dir = ROOT / "build" / "reports" / f"code_audit_{stamp}"
    out_dir.mkdir(parents=True, exist_ok=True)
    # JSON graph
    (out_dir / "include_graph.json").write_text(json.dumps(graph, indent=2))

    # DOT
    def short(p: str) -> str:
        try:
            return str(Path(p).relative_to(ROOT)).replace("\\", "/")
        except Exception:
            return p

    with (out_dir / "include_graph.dot").open("w", encoding="utf-8") as f:
        f.write("digraph includes {\n  rankdir=LR;\n  node [shape=box, fontsize=10];\n")
        for u, vs in graph.items():
            su = short(u)
            for v in vs:
                sv = short(v)
                f.write(f'  "{su}" -> "{sv}";\n')
        f.write("}\n")
    # Cycles
    if cycles:
        lines: List[str] = []
        for cyc in cycles:
            lines.append(" -> ".join(short(n) for n in cyc))
        (out_dir / "include_cycles.txt").write_text("\n\n".join(lines))


def main() -> int:
    graph = build_graph()
    cycles = detect_cycles(graph)
    write_reports(graph, cycles)
    print(f"Scanned {len(graph)} files, cycles: {len(cycles)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
