#!/usr/bin/env python3
"""Re-checks the machine-verifiable claims of touch_game.json against the ARM32 exe.

    python arm32/facts/touch_game_verify.py

Checks: expected_bytes at every arm_rva, vtable cells (expected_cell_value), the five InputMode stubs returned by the
mapper getInputMode slots, the processHIDItem tbb table, the mapper vtable slot layout, the MultitouchItem layout written
by the factory and the 1.0f scale constant.  Prints FAIL lines and exits 1 on any mismatch.
"""
import json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "arm32"))
import arm32re  # noqa: E402

pe = arm32re.Pe()
BASE = pe.base
fails = []


def check(cond, msg):
    if not cond:
        fails.append(msg)
        print("FAIL", msg)


def u32(rva):
    return pe.u32(rva)


def code_target(raw):  # stored Thumb pointer -> even RVA
    return (raw - BASE) & ~1


facts = json.load(open(ROOT / "arm32" / "facts" / "touch_game.json", encoding="utf-8"))["facts"]
n = 0
for f in facts:
    if f.get("expected_bytes"):
        rva = int(f["arm_rva"], 16)
        got = bytes(pe.bytes(rva, len(f["expected_bytes"]) // 2)).hex()
        check(got == f["expected_bytes"], "%s bytes at %s: %s != %s" % (f["id"], f["arm_rva"], got, f["expected_bytes"]))
        n += 1
    for k in [k for k in f if k.endswith("_expected_bytes") and k != "expected_bytes"]:
        rva = int(f[k.replace("_expected_bytes", "_rva")], 16)
        got = bytes(pe.bytes(rva, len(f[k]) // 2)).hex()
        check(got == f[k], "%s %s at %x: %s != %s" % (f["id"], k, rva, got, f[k]))
        n += 1
    if f.get("cell_rva"):
        cell = int(f["cell_rva"], 16)
        want = int(f["expected_cell_value"], 16)
        # file holds ImageBase-relative preferred values (ImageBase is the preferred base 0x400000)
        check(u32(cell) == want, "%s cell %s = %08x != %08x" % (f["id"], f["cell_rva"], u32(cell), want))
        n += 1

# InputMode stubs: `push {fp,lr}; mov fp,sp; movs r0,#N; pop {fp,pc}`
stub = lambda rva: bytes(pe.bytes(rva, 12)).hex()
for rva, val in ((0x0ad200, 1), (0x2044e0, 2), (0x2d0ca0, 3), (0x3b8da0, 4), (0x08b980, 0)):
    check(stub(rva) == "2de90048eb46%02x20bde80088" % val, "getInputMode stub %x should return %d" % (rva, val))

# mapper vtables (rodata): slot 4 = getInputMode, slot 3 = tick
mappers = {  # name: (vtable, tick, mode_stub)
    "Mouse": (0x0dbb970, 0x5afc2c, 0x0ad200),
    "Keyboard": (0x0dbb940, 0x5af410, 0x08b980),
    "GamePad": (0x0dbb52c, 0x5a9d18, 0x2d0ca0),
    "Touch": (0x0dbba20, 0x5b21ac, 0x2044e0),
    "Motion": (0x0dbba70, 0x5a9d18, 0x3b8da0),
}
for name, (vt, tick, stubrva) in mappers.items():
    check(code_target(u32(vt + 12)) == tick, "%sMapper vtable[3] != %x" % (name, tick))
    check(code_target(u32(vt + 16)) == stubrva, "%sMapper vtable[4] != %x" % (name, stubrva))
check(code_target(u32(0x0dbba20 + 24)) == 0x5b242c, "TouchMapper vtable[6] (render) != 0x5b242c")

# processHIDItem tbb table at 0x583036: 03 2d 3b 40 49 -> pc base 0x583036
tbl = bytes(pe.bytes(0x583036, 5))
check(tbl.hex() == "032d3b4049", "processHIDItem tbb table %s" % tbl.hex())
targets = [0x583036 + 2 * b for b in tbl]
check(targets == [0x58303c, 0x583090, 0x5830ac, 0x5830b6, 0x5830c8], "tbb targets %s" % [hex(t) for t in targets])

# MultitouchItem factory stores: type byte 1 at +4, x +8, y +0xa, a +0xc, b +0xd, pid +0xe, vtable 0x176ddf8
code = bytes(pe.bytes(0x62a174, 0x48))
check(code.hex().find("0371") >= 0, "factory: strb r3,[r0,#4] missing")   # 03 71 = strb r3,[r0,#4]
check(code.hex().find("0481") >= 0 and code.hex().find("4581") >= 0, "factory: strh r4/r5,[r0,#8/#0xa] missing")
check(code.hex().find("0273") >= 0 and code.hex().find("4673") >= 0 and code.hex().find("8173") >= 0, "factory: strb r2/r6/r1,[r0,#0xc/#0xd/#0xe] missing")
check(u32(0x136ddf8) == BASE + 0x623bcc + 1, "MultitouchItem vtable[0] != 0x623bcc")

# InputAdapter ctor default pointer scale 1.0f at pool 0x586c14
check(struct.unpack("<f", bytes(pe.bytes(0x586c14, 4)))[0] == 1.0, "adapter default scale != 1.0f")

# default input mode function and device-type getter
check(bytes(pe.bytes(0x62212c, 12)).hex() == "2de90048eb4603461b68d3f8", "vf154 prologue")
check(bytes(pe.bytes(0x583aa0, 12)).hex() == "2de90048eb46d0f8a803bde8", "vf116 `ldr.w r0,[r0,#0x3a8]`")

# strings
check(bytes(pe.bytes(0xdbb650, 6)) == b"Touch\0", "string Touch")
check(bytes(pe.bytes(0xdbb660, 6)) == b"Mouse\0", "string Mouse")

print("checked %d byte/cell claims + structure" % n)
if fails:
    print("%d FAILURES" % len(fails))
    sys.exit(1)
print("OK")
