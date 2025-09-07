#!/usr/bin/env python3
"""atlas_generate.py - Phase 7 asset processing scaffold

Generates a horizontal atlas description (no image packing yet) by taking a list
of texture file paths and writing a manifest JSON containing:
  - input list
  - cumulative width (sum of widths placeholder; real probe deferred)
  - placeholder UV partitions (even slice by count)

Future work: integrate with actual image loading (Pillow) and emit a packed
PNG plus precise UVs. For now this script validates invocation plumbing.
"""

import argparse, json, os, sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inputs", nargs="+", help="Input texture paths")
    ap.add_argument("--output", required=True, help="Output JSON manifest")
    args = ap.parse_args()
    inputs = [os.path.normpath(p).replace("\\", "/") for p in args.inputs]
    if not inputs:
        print("No inputs provided", file=sys.stderr)
        return 1
    count = len(inputs)
    # Placeholder total width heuristic (fixed 64 each until real probe)
    width_each = 64
    total_width = width_each * count
    uvs = []
    for i in range(count):
        u0 = i / count
        u1 = (i + 1) / count
        uvs.append(
            {"index": i, "path": inputs[i], "u0": u0, "v0": 0.0, "u1": u1, "v1": 1.0}
        )
    manifest = {
        "version": 1,
        "inputs": inputs,
        "total_width_estimate": total_width,
        "uvs_even_slice": uvs,
        "note": "Placeholder atlas; real packing pending Phase 7 extension",
    }
    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print(f"Wrote atlas manifest {args.output} with {count} entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
