#!/usr/bin/env python3
"""
Simple script to print distribution of `skill_strength` in assets/skills_uhf87f.json
"""
import json
import os
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(__file__))
SKILLS_PATH = os.path.join(ROOT, "assets", "skills_uhf87f.json")


def load_skills(path):
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    # possible formats: list of skills, or object with 'skills' key, or dict mapping id->def
    if isinstance(data, dict):
        # try common keys
        if "skills" in data and isinstance(data["skills"], list):
            return data["skills"]
        # if mapping of id->def
        # check whether values look like skill defs
        vals = list(data.values())
        if (
            vals
            and isinstance(vals[0], dict)
            and "name" in vals[0]
            or "skill_strength" in vals[0]
        ):
            return vals
        # fallback: treat dict as single skill
        return [data]
    elif isinstance(data, list):
        return data
    else:
        raise RuntimeError("Unexpected JSON format for skills file")


def analyze(skills):
    cnt = Counter()
    examples = defaultdict(list)
    total = 0
    for s in skills:
        total += 1
        # skill_strength may be missing; treat as None
        val = s.get("skill_strength") if isinstance(s, dict) else None
        # normalize numeric strings
        if isinstance(val, str):
            try:
                ival = int(val)
                val = ival
            except Exception:
                pass
        cnt[val] += 1
        if len(examples[val]) < 6:
            # collect short identifier
            name = s.get("name") or s.get("id") or s.get("icon") or repr(s)[:40]
            examples[val].append(name)
    return total, cnt, examples


def main():
    if not os.path.exists(SKILLS_PATH):
        print("ERROR: skills file not found at", SKILLS_PATH)
        return 2
    skills = load_skills(SKILLS_PATH)
    total, cnt, examples = analyze(skills)
    print("Total skills found:", total)
    print("\nDistribution of skill_strength (value -> count, percent):")
    keys = sorted([k for k in cnt.keys() if k is not None])
    # Print unspecified first
    if None in cnt:
        print(f"  <unspecified> : {cnt[None]} ({cnt[None]*100/total:.1f}%)")
    for k in keys:
        print(f"  {k:3} : {cnt[k]:4} ({cnt[k]*100/total:.1f}%)")
    print("\nExamples per bucket (up to 6 names):")
    if None in examples:
        print("  <unspecified> ->", examples[None])
    for k in keys:
        print(f"  {k} ->", examples[k])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
