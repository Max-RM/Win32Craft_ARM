#!/usr/bin/env python3
"""Survey / iterate the area fact files (arm32/facts/*.json)."""
import json, glob, re, os, collections, sys

FACT_DIR = os.environ.get("ARM32_FACTS_DIR") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "arm32", "facts")   # per game version, e.g. arm32/v1_16_40/facts
SKIP = ("verdicts.json", "imports_table.json", "audit.json")


def fact_files():
    for fn in sorted(glob.glob(os.path.join(FACT_DIR, "*.json"))):
        base = os.path.basename(fn)
        if base.endswith(SKIP[0]) or base in SKIP[1:]:
            continue
        yield base, fn


def all_facts():
    for base, fn in fact_files():
        d = json.load(open(fn, encoding="utf-8"))
        for f in d.get("facts", []):
            yield base[:-5], f


def rva_format(r):
    if r is None:
        return "null"
    if isinstance(r, int):
        return "int"
    if isinstance(r, str) and re.fullmatch(r"0x[0-9a-fA-F]+", r):
        return "hex"
    return "other"


if __name__ == "__main__":
    kinds = collections.Counter()
    fmts = collections.Counter()
    other = {}
    tot = 0
    for area, f in all_facts():
        tot += 1
        kinds[f.get("kind")] += 1
        fm = rva_format(f.get("arm_rva"))
        fmts[(f.get("kind"), fm)] += 1
        if fm == "other" and len(other) < 12:
            other[f["id"]] = f["arm_rva"]
    print("total facts", tot)
    print(kinds)
    for k, v in sorted(fmts.items(), key=lambda x: str(x)):
        print(k, v)
    print(other)
