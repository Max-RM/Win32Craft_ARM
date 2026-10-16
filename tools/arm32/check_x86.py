#!/usr/bin/env python3
"""Regression check on the REAL repo sources: the x86 build output of win32_host.c / api_compat.c must be identical
(disassembly + data digest) to the pristine copies in arm32/orig/.   Usage: python tools/arm32/check_x86.py
Exit code 0 = identical."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import try_unit as T

REPO = HERE.parent.parent
out = REPO / "arm32" / "work" / "_x86_check"
out.mkdir(parents=True, exist_ok=True)
bad = 0
for name in ("win32_host.c", "api_compat.c"):
    pr, po = out / (name + ".pristine.obj"), out / (name + ".repo.obj")
    rc1, e1 = T.compile_one(REPO / "arm32" / "orig" / name, pr, T.X86_FLAGS, [str(REPO)])
    rc2, e2 = T.compile_one(REPO / name, po, T.X86_FLAGS, [str(REPO)])
    if rc1 or rc2:
        print("%-14s COMPILE FAILED (pristine rc=%d repo rc=%d)\n%s" % (name, rc1, rc2, (e1 + e2)[:600]))
        bad += 1
        continue
    d1, d2 = T.disasm_digest(pr)[0], T.disasm_digest(po)[0]
    same = d1 == d2
    print("%-14s %s" % (name, "x86 IDENTICAL" if same else "x86 DIFFERENT!"))
    bad += 0 if same else 1
sys.exit(1 if bad else 0)
