#!/usr/bin/env python3
"""Cross-check win32_host_arm32.h against the real ARM32 exe.

For every `#define ARM_X 0x...` that has a matching `static const BYTE ARM_X_EXPECTED[N] = {...}` the bytes at that RVA in
Minecraft.Windows.exe must equal the array.  Also reports code RVAs that are odd (Thumb bit mistakenly included) or not
pdata function starts when the constant is documented as a `function`."""
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arm32re

# 1.2.8 by default; for another game version set ARM32_HDR=<header> ARM32_PREFIX=<macro prefix> and ARM32_EXE=<its exe>
HDR = os.environ.get("ARM32_HDR") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "win32_host_arm32.h")
PFX = os.environ.get("ARM32_PREFIX", "ARM")
text = open(HDR, encoding="utf-8").read()
defs = {}
kinds = {}
for m in re.finditer(r"#define (" + PFX + r"_\w+)\s+(0x[0-9a-fA-F]+)\s+/\* (\w+):", text):
    defs[m.group(1)] = int(m.group(2), 16)
    kinds[m.group(1)] = m.group(3)
exps = {}
for m in re.finditer(r"static const BYTE (" + PFX + r"_\w+)_EXPECTED\[(\d+)\] = \{([^}]*)\};", text):
    exps[m.group(1)] = bytes(int(x, 16) for x in m.group(3).replace(" ", "").split(",") if x)

c = arm32re.Ctx()
pe = c.pe
fstarts = set(c.idx.fstarts)
bad = 0
checked = 0
for name, want in sorted(exps.items()):
    if name.startswith(PFX + "_FMOD_"):
        continue  # fmod_ARM.dll facts describe another image, not the game exe
    if name not in defs:
        print("NO-DEFINE", name)
        bad += 1
        continue
    rva = defs[name]
    if rva >= pe.size_of_image:
        continue  # an offset/size, not an address
    got = pe.bytes(rva, len(want))
    checked += 1
    if got != want:
        if kinds.get(name) in ("offset", "vtable_slot", "data", "abi", "other", "pe"):
            # for these kinds the expected bytes describe the TARGET (e.g. the function a cell points to), not the address
            print("info-TARGET-BYTES %-44s (kind=%s): expected bytes are not at the constant's own address" % (name, kinds.get(name)))
            continue
        bad += 1
        print("MISMATCH %-48s kind=%s rva=%08x want=%s got=%s" % (name, kinds.get(name), rva, want.hex(), got.hex()))
fn_issues = 0
for name, rva in sorted(defs.items()):
    if kinds[name] in ("function", "hook_site", "vtable_slot") and rva < pe.size_of_image and pe.in_text(rva):
        if rva & 1:
            print("ODD-CODE-RVA", name, hex(rva))
            fn_issues += 1
        elif rva not in fstarts:
            print("note: %s %08x is not a pdata function start (kind=%s)" % (name, rva, kinds[name]))
print("checked %d expected-byte arrays, %d mismatches, %d odd code RVAs" % (checked, bad, fn_issues))
sys.exit(1 if bad or fn_issues else 0)
