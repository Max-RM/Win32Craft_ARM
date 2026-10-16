#!/usr/bin/env python3
"""List api-set (api-ms-win-*/ext-ms-*) imports of a set of PE files and which of them the SDK app-local UCRT set covers.

    python tools/arm32/apiset_scan.py <pe-or-folder> [...]  [--ucrt DIR] [--json OUT]
Prints: contract -> importing modules -> functions, and flags contracts NOT shipped by the SDK UCRT redistributable."""
import argparse, json, os, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import arm32re


def collect(paths):
    files = []
    for p in paths:
        p = Path(p)
        if p.is_dir():
            files += sorted(list(p.glob("*.dll")) + list(p.glob("*.exe")))
        else:
            files.append(p)
    return files


def scan(files):
    """-> {contract(lower): {module: set(functions)}}  (normal + delay imports)"""
    res = {}
    for f in files:
        try:
            pe = arm32re.Pe(str(f))
        except Exception:
            continue
        for imp in pe.imports():
            d = imp["dll"].lower()
            if d.startswith(("api-ms-win-", "ext-ms-win-", "api-ms-onecoreuap-")):
                res.setdefault(d, {}).setdefault(f.name, set()).add(imp["name"])
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--ucrt", default=r"D:\Windows Kits\10\Redist\ucrt\DLLs\arm")
    ap.add_argument("--json")
    a = ap.parse_args()
    files = collect(a.paths)
    res = scan(files)
    have = {p.name.lower() for p in Path(a.ucrt).glob("*.dll")}
    out = {}
    for c in sorted(res):
        funcs = sorted(set().union(*res[c].values()))
        out[c] = {"covered_by_sdk_ucrt": c in have, "modules": sorted(res[c]), "functions": funcs}
        print("%-48s %-9s modules=%-34s funcs=%d" % (c, "SDK-ucrt" if c in have else "MISSING", ",".join(sorted(res[c]))[:34], len(funcs)))
    miss = [c for c in out if not out[c]["covered_by_sdk_ucrt"]]
    print("\n%d contracts imported, %d not covered by the SDK UCRT set" % (len(out), len(miss)))
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
