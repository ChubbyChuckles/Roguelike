#!/usr/bin/env python3
"""Simple commit message lint: enforce imperative mood heuristic & length.

Heuristics:
 - First line <= 72 chars
 - First line: starts with a capital letter, not past tense (endswith 'ed')
 - Avoid trailing period
Exit non-zero on violations.
"""
from __future__ import annotations
import re, sys, pathlib

def main(path: str) -> int:
    p = pathlib.Path(path)
    text = p.read_text(encoding="utf-8").splitlines()
    if not text:
        print("Empty commit message", file=sys.stderr)
        return 1
    subject = text[0].strip()
    errors = []
    if len(subject) > 72:
        errors.append(f"Subject too long ({len(subject)} > 72)")
    if subject.endswith('.'):
        errors.append("Subject should not end with a period")
    first_word = subject.split()[0].lower() if subject.split() else ''
    if re.fullmatch(r"[a-z]+ed", first_word):
        errors.append("Use imperative mood (avoid past tense like 'Added')")
    if errors:
        print("Commit message lint failures:")
        for e in errors:
            print(f" - {e}")
        return 1
    return 0

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: commit_msg_lint.py <commit-msg-file>", file=sys.stderr)
        sys.exit(1)
    sys.exit(main(sys.argv[1]))
