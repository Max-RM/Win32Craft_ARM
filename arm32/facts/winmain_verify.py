#!/usr/bin/env python3
"""Dry-run of the ARM32 1.2.8 PE patch plan (area: winmain).

Builds the patched image in memory exactly as described in winmain.md, then
verifies it (structure, capstone round-trip of the new bytes, BL range,
unicorn smoke test of call site -> trampoline -> import cell).

    python arm32/facts/winmain_verify.py [--write out.exe] [--src path]

This script is evidence/prototype only; the real patcher belongs to the porter.
"""
import argparse, hashlib, struct, sys
from pathlib import Path

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
try:
    from keystone import Ks, KS_ARCH_ARM, KS_MODE_THUMB
except ImportError:  # an OpenStack "keystone" package may shadow keystone-engine
    from keystone.keystone import Ks, KS_ARCH_ARM, KS_MODE_THUMB
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_PROT_ALL
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R4, UC_ARM_REG_LR, UC_ARM_REG_PC, \
    UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "Minecraft_1.2.8.0_ARM32" / "Minecraft.Windows.exe"
EXPECTED_SHA256 = "b01d5f1237d2daad8cc076d7218f0788751b55a9576c779ccdba08eef6fcfcf8"

IMAGE_BASE = 0x400000
CALL_SITE_RVA = 0x00B49ACA          # bl sub_b4a424 inside __scrt_common_main_seh (sub_b49a24)
CALL_SITE_ORIG = bytes.fromhex("00f0abfc")
OLD_CALLEE_RVA = 0x00B4A424
RETURN_RVA = 0x00B49ACE             # mov r4, r0
TRAMP_RVA = 0x00C36EA0              # .text slack (0xC36E9C..0xC37000 is zero, unreferenced)
NEW_SECTION_RVA = 0x01730000
EXPECTED_NEW_IMAGE_SIZE = 0x01731000


def align(v, a):
    return (v + a - 1) & ~(a - 1)


def build(data: bytes, variant: str = "pic"):
    assert hashlib.sha256(data).hexdigest() == EXPECTED_SHA256, "unexpected input exe"
    img = bytearray(data)
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    fh = pe + 4
    opt = fh + 20
    nsec = struct.unpack_from("<H", img, fh + 2)[0]
    sec_tab = opt + struct.unpack_from("<H", img, fh + 16)[0]
    fa = struct.unpack_from("<I", img, opt + 0x24)[0]
    sa = struct.unpack_from("<I", img, opt + 0x20)[0]
    secs = [struct.unpack_from("<4I", img, sec_tab + i * 40 + 8) for i in range(nsec)]  # vs, va, rs, ro

    def r2o(rva):
        for vs, va, rs, ro in secs:
            if va <= rva < va + max(vs, rs):
                return ro + rva - va
        if rva < 0x400:
            return rva
        raise ValueError(hex(rva))

    def cstr(rva):
        o = r2o(rva)
        return bytes(img[o:img.index(0, o)])

    def w(off, old, new, label):
        assert bytes(img[off:off + len(old)]) == old, f"{label}: file+{off:#x} = {bytes(img[off:off+len(old)]).hex()}"
        assert len(old) == len(new)
        img[off:off + len(new)] = new

    # ---- header fields -------------------------------------------------
    assert struct.unpack_from("<H", img, fh + 18)[0] == 0x0122          # LAA already set
    w(opt + 0x46, struct.pack("<H", 0x9140), struct.pack("<H", 0x8140), "DllCharacteristics")  # drop APPCONTAINER only
    assert struct.unpack_from("<2H", img, opt + 0x28) == (6, 2)         # OS 6.2 kept
    assert struct.unpack_from("<2H", img, opt + 0x30) == (6, 2)         # subsystem 6.2 kept

    # ---- in-place DLL name renames (every occurrence, counted) ---------
    renames = [(b"vccorlib140_app.DLL", b"vccorlib140.DLL", 0x1328CDE),
               (b"MSVCP140_APP.dll", b"MSVCP140.dll", 0x132C3C8),
               (b"CONCRT140_APP.dll", b"CONCRT140.dll", 0x132CD0A),
               (b"VCRUNTIME140_APP.dll", b"VCRUNTIME140.dll", 0x132CE36)]
    for old, new, off in renames:
        assert bytes(img).count(old) == 1 and bytes(img).find(old) == off
        w(off, old, new.ljust(len(old), b"\0"), old.decode())

    # ---- import directory: copy 50 descriptors + win32craft.dll --------
    imp_rva, imp_size = struct.unpack_from("<2I", img, opt + 0x68)
    assert (imp_rva, imp_size) == (0x13269A8, 0x3FC)
    cur = r2o(imp_rva)
    descs = []
    while True:
        d = bytes(img[cur:cur + 20])
        if not any(struct.unpack("<5I", d)):
            break
        descs.append(d)
        cur += 20
    assert len(descs) == 50
    nd = len(descs) + 2                                  # + win32craft + null
    sect = bytearray(nd * 20)

    def append(b, al=1):
        sect.extend(b"\0" * ((-len(sect)) % al))
        o = len(sect)
        sect.extend(b)
        return o

    n_name = append(b"win32craft.dll\0")
    n_hint = append(b"\0\0Win32Bootstrap\0", 2)
    n_ilt = append(struct.pack("<2I", NEW_SECTION_RVA + n_hint, 0), 4)
    n_iat = append(struct.pack("<2I", NEW_SECTION_RVA + n_hint, 0), 4)
    sect[:nd * 20] = b"".join(descs) + struct.pack(
        "<5I", NEW_SECTION_RVA + n_ilt, 0, 0, NEW_SECTION_RVA + n_name, NEW_SECTION_RVA + n_iat) + b"\0" * 20
    vsize = len(sect)
    rsize = align(vsize, fa)
    sect.extend(b"\0" * (rsize - vsize))
    new_raw = align(len(img), fa)
    assert new_raw == len(data) == 0x1680A00
    last_va, last_vs, last_rs = secs[-1][1], secs[-1][0], secs[-1][2]
    assert align(last_va + max(last_vs, last_rs), sa) == NEW_SECTION_RVA
    img.extend(b"\0" * (new_raw - len(img)))
    img.extend(sect)
    cell_rva = NEW_SECTION_RVA + n_iat

    # section header
    new_hdr = sec_tab + nsec * 40
    assert new_hdr + 40 <= struct.unpack_from("<I", img, opt + 0x3C)[0]
    w(new_hdr, b"\0" * 40, struct.pack("<8s6I2HI", b".d3imp\0\0", vsize, NEW_SECTION_RVA, rsize, new_raw,
                                         0, 0, 0, 0, 0xC0000040), "section header")
    w(fh + 2, struct.pack("<H", nsec), struct.pack("<H", nsec + 1), "NumberOfSections")
    old_init = struct.unpack_from("<I", img, opt + 8)[0]
    w(opt + 8, struct.pack("<I", old_init), struct.pack("<I", old_init + rsize), "SizeOfInitializedData")
    new_size = align(NEW_SECTION_RVA + vsize, sa)
    assert new_size == EXPECTED_NEW_IMAGE_SIZE
    w(opt + 0x38, struct.pack("<I", 0x1730000), struct.pack("<I", new_size), "SizeOfImage")
    w(opt + 0x68, struct.pack("<2I", imp_rva, imp_size), struct.pack("<2I", NEW_SECTION_RVA, nd * 20), "import dir")

    # ---- trampoline in .text slack -------------------------------------
    t_off = r2o(TRAMP_RVA)
    assert bytes(img[t_off:t_off + 0x160]) == b"\0" * 0x160
    ks = Ks(KS_ARCH_ARM, KS_MODE_THUMB)
    if variant == "pic":
        # ip = literal ; ip += pc ; ip = *ip ; bx ip      literal = cell_rva - (TRAMP_RVA + 8)
        lit = cell_rva - (TRAMP_RVA + 8)
        asm = "ldr.w ip, [pc, #8]; add ip, pc; ldr.w ip, [ip]; bx ip"
        code, _ = ks.asm(asm, IMAGE_BASE + TRAMP_RVA)
        code = bytes(code)
        code += b"\0" * (12 - len(code)) + struct.pack("<I", lit & 0xFFFFFFFF)
    else:
        lit = IMAGE_BASE + cell_rva
        asm = "ldr.w ip, [pc, #8]; ldr.w ip, [ip]; bx ip"
        code, _ = ks.asm(asm, IMAGE_BASE + TRAMP_RVA)
        code = bytes(code)
        code += b"\0" * (12 - len(code)) + struct.pack("<I", lit)
    img[t_off:t_off + len(code)] = code

    # ---- call site ------------------------------------------------------
    bl, _ = ks.asm("bl #0x%x" % (IMAGE_BASE + TRAMP_RVA), IMAGE_BASE + CALL_SITE_RVA)
    bl = bytes(bl)
    assert len(bl) == 4
    w(r2o(CALL_SITE_RVA), CALL_SITE_ORIG, bl, "call site")
    return img, dict(cell_rva=cell_rva, tramp=code, bl=bl, vsize=vsize, rsize=rsize, new_size=new_size,
                     lit=lit & 0xFFFFFFFF, n_name=n_name, n_hint=n_hint, n_ilt=n_ilt, n_iat=n_iat,
                     call_file=r2o(CALL_SITE_RVA), tramp_file=t_off)


def dis(code, va):
    cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    return [(i.address, i.bytes.hex(), i.mnemonic, i.op_str) for i in cs.disasm(code, va)]


def smoke(info, base, variant):
    """Run call site -> trampoline -> fake Win32Bootstrap and check return path (base may differ = ASLR)."""
    mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    page = 0x1000
    site_va = base + CALL_SITE_RVA
    tr_va = base + TRAMP_RVA
    cell_va = base + info["cell_rva"]
    stub = 0x7000_0000
    for a in {site_va & ~0xFFF, tr_va & ~0xFFF, cell_va & ~0xFFF, stub, 0x6000_0000}:
        mu.mem_map(a, page, UC_PROT_ALL)
    mu.mem_write(site_va, info["bl"] + bytes.fromhex("0446"))          # bl tramp ; mov r4,r0
    mu.mem_write(tr_va, info["tramp"])
    mu.mem_write(cell_va, struct.pack("<I", stub | 1))                 # loader-filled IAT cell
    mu.mem_write(stub, bytes.fromhex("4ff0 2a00 7047".replace(" ", "")))  # movs r0,#0x2a? -> mov.w r0,#0x2a ; bx lr
    mu.reg_write(UC_ARM_REG_SP, 0x6000_0800)
    mu.reg_write(UC_ARM_REG_R0, 0x400000)
    mu.reg_write(UC_ARM_REG_R1, 0)
    mu.reg_write(UC_ARM_REG_R2, 0x1234)
    mu.reg_write(UC_ARM_REG_R3, 0)
    try:
        mu.emu_start(site_va | 1, base + RETURN_RVA + 2, count=40)
    except Exception as e:  # noqa
        return f"FAIL {e}"
    ok = mu.reg_read(UC_ARM_REG_R4) == 0x2A and mu.reg_read(UC_ARM_REG_PC) == base + RETURN_RVA + 2
    lr = mu.reg_read(UC_ARM_REG_LR)
    return f"{'OK' if ok else 'FAIL'} r4={mu.reg_read(UC_ARM_REG_R4):#x} lr={lr:#x}(expect {base+RETURN_RVA|1:#x}) ip={mu.reg_read(UC_ARM_REG_R12):#x}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=str(SRC))
    ap.add_argument("--write")
    ap.add_argument("--variant", default="pic", choices=["pic", "abs"])
    a = ap.parse_args()
    data = Path(a.src).read_bytes()
    print("input sha256", hashlib.sha256(data).hexdigest())
    print("--- original call site")
    for x in dis(CALL_SITE_ORIG + bytes.fromhex("0446"), IMAGE_BASE + CALL_SITE_RVA):
        print("  %08x %-10s %s %s" % x)
    img, info = build(data, a.variant)
    print("--- patched call site (file+%#x)" % info["call_file"])
    for x in dis(info["bl"] + bytes.fromhex("0446"), IMAGE_BASE + CALL_SITE_RVA):
        print("  %08x %-10s %s %s" % x)
    delta = TRAMP_RVA - (CALL_SITE_RVA + 4)
    print("  BL displacement %+d bytes (limit +-16777214): %s" % (delta, "in range" if abs(delta) < (1 << 24) else "OUT OF RANGE"))
    print("--- trampoline (%s) at RVA %#x file+%#x bytes %s" % (a.variant, TRAMP_RVA, info["tramp_file"], info["tramp"].hex()))
    for x in dis(info["tramp"][:12], IMAGE_BASE + TRAMP_RVA):
        print("  %08x %-10s %s %s" % x)
    print("  literal @+12 = %#x ; cell RVA %#x" % (info["lit"], info["cell_rva"]))
    for base in (0x400000, 0x1C3A0000):
        print("smoke base %#x:" % base, smoke(info, base, a.variant))
    print("new section vsize %#x rsize %#x SizeOfImage %#x" % (info["vsize"], info["rsize"], info["new_size"]))
    print("patched sha256", hashlib.sha256(img).hexdigest(), "len", hex(len(img)))
    if a.write:
        Path(a.write).write_bytes(img)
        print("wrote", a.write)


if __name__ == "__main__":
    main()
