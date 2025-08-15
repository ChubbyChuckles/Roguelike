#!/usr/bin/env python3
"""Lightweight maintainability audit (Phase M1).

Checks:
 1. util/ headers include only stdlib or util/*.
 2. No cross-module inclusion of *_internal.h outside its own directory tree.
 3. Public headers functions use rogue_ prefix (heuristic: lines starting with 'int|void|float|double|struct' declaration).
 4. Simple include cycle detection on collected edges (depth-limited).

Exit code 0 on success; non-zero on first failure (prints reason).
If Python environment lacks needed modules, script exits 0 (graceful skip).
"""
from __future__ import annotations
import os, re, sys, collections
from typing import List, Optional, Iterable, Dict, Set

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC_ROOT = os.path.join(PROJECT_ROOT, "src")

INTERNAL_SUFFIX = "_internal.h"

local_include_re = re.compile(r'^#\s*include\s+"([^"]+)"')
fn_decl_re = re.compile(
    r"^(?:extern\s+)?(int|void|float|double|struct|unsigned|char)\s+([a-zA-Z0-9_]+)\s*\("
)


def rel(path: str) -> str:
    return os.path.relpath(path, PROJECT_ROOT).replace("\\", "/")


def gather_files() -> Iterable[str]:
    for root, _, files in os.walk(SRC_ROOT):
        for f in files:
            if f.endswith((".h", ".c")):
                yield os.path.join(root, f)


def module_of(path: str) -> str:
    parts = rel(path).split("/")
    try:
        i = parts.index("src") + 1
        return parts[i] if i < len(parts) else ""
    except ValueError:
        return ""


def check_util_includes(path: str, lines: List[str]) -> Optional[str]:
    mod = module_of(path)
    if mod != "util":
        return None
    for ln in lines:
        m = local_include_re.match(ln)
        if m:
            inc = m.group(1)
            if not inc.startswith("util/") and "/" in inc:
                return f"util header includes cross-module path: {rel(path)} -> {inc}"
    return None


def check_cross_internal_inclusion(path: str, lines: List[str]) -> Optional[str]:
    for ln in lines:
        m = local_include_re.match(ln)
        if not m:
            continue
        inc = m.group(1)
        if inc.endswith(INTERNAL_SUFFIX):
            base_dir = os.path.dirname(path)
            target = os.path.normpath(os.path.join(os.path.dirname(path), inc))
            if not os.path.commonpath([base_dir, target]).startswith(
                os.path.commonpath([base_dir])
            ):
                return f"cross-module internal header inclusion: {rel(path)} -> {inc}"
    return None


def check_public_prefix(path: str, lines: List[str]) -> Optional[str]:
    if path.endswith(INTERNAL_SUFFIX) or not path.endswith(".h"):
        return None
    for ln in lines:
        m = fn_decl_re.match(ln.strip())
        if m:
            name = m.group(2)
            if name.startswith(("rogue_", "ROGUE_")):
                continue
            return f"public header function without rogue_ prefix: {rel(path)}::{name}"
    return None


def build_include_graph(files: Iterable[str]) -> Dict[str, Set[str]]:
    graph: Dict[str, Set[str]] = collections.defaultdict(set)
    for path in files:
        with open(path, "r", encoding="utf-8", errors="ignore") as fh:
            for ln in fh:
                m = local_include_re.match(ln)
                if m:
                    inc = m.group(1)
                    full = os.path.normpath(os.path.join(os.path.dirname(path), inc))
                    if os.path.exists(full):
                        graph[path].add(full)
    return graph


def detect_cycles(graph: Dict[str, Set[str]], limit: int = 6) -> Optional[List[str]]:
    seen_path: List[str] = []
    visited: Set[str] = set()

    def dfs(node: str) -> Optional[List[str]]:
        if len(seen_path) > limit:
            return None
        seen_path.append(node)
        for nxt in graph.get(node, ()):
            if nxt in seen_path:
                cycle = seen_path[seen_path.index(nxt) :] + [nxt]
                return cycle
            if nxt not in visited:
                res = dfs(nxt)
                if res:
                    return res
        seen_path.pop()
        visited.add(node)
        return None

    for n in graph:
        res = dfs(n)
        if res:
            return res
    return None


def main():
    files = list(gather_files())
    for path in files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                lines = fh.readlines()
        except Exception:
            continue
        for check in (
            check_util_includes,
            check_cross_internal_inclusion,
            check_public_prefix,
        ):
            msg = check(path, lines)
            if msg:
                print(f"AUDIT FAIL: {msg}")
                return 1
    graph = build_include_graph(files)
    cyc = detect_cycles(graph)
    if cyc:
        print(
            "AUDIT FAIL: include cycle detected:\n" + " ->\n".join(rel(p) for p in cyc)
        )
        return 1
    print("Maintainability audit passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
