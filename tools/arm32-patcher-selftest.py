#!/usr/bin/env python3
"""Self-test for tools/arm32-patcher.py  (prints PASS / FAIL / SKIP lines, exit code 0 = all passed).

Runs both complete profiles on the real inputs and asserts:
  * v1_2_8 : the engine output is byte identical to tools/1.2.8-arm32-patcher.py (exe, fmod, and the
             --redirect-api-sets variant) and to the packaged dist-arm32 exe hash
  * v0_10_4: the plan table (pe_patch_plan.json) is reproduced edit by edit, its SHA-256 (453ce59a...)
             is reproduced, the diff against the stock exe is confined to the planned ranges
  * invariants on every output: no W+X section, SizeOfImage multiple of SectionAlignment, sections
    ascending, imports parse (own parser AND tools/arm32/arm32re.py), entry unchanged, BL call site
    decodes to the trampoline, trampoline decodes (capstone) and its PIC literal reaches the IAT cell,
    no relocation / .pdata over the patched code, checksum policy
  * companion DLLs: the genuine VCLibs.120.00.Phone DLLs (from the .appx) patch to exactly the
    friend copies; the friend copies are recognised and left untouched; PE checksum algorithm
    equals imagehlp
  * negative tests: wrong old bytes, already patched input, W+X section, BL out of range, unknown
    companion, relocation overlap, placeholder profile are all refused
  * the CLI (detect / patch / probe / verify) end to end

Inputs are looked up at the repo-local / owner-machine locations below; override with environment
variables ARM32_EXE_0104, ARM32_VCLIBS120_APPX, ARM32_FRIEND_DIR.  Missing inputs give SKIP, not PASS.
Temporary files go to the system temp dir (set TMP/TEMP to redirect).
"""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True          # do not litter tools/__pycache__
HERE = Path(__file__).resolve().parent
REPO = HERE.parent
PATCHER = HERE / "arm32-patcher.py"
OLD_PATCHER = HERE / "1.2.8-arm32-patcher.py"

EXE_128 = Path(os.environ.get("ARM32_EXE_128", REPO / "Minecraft_1.2.8.0_ARM32" / "Minecraft.Windows.exe"))
FMOD_128 = EXE_128.parent / "fmod_ARM.dll"
PACKAGED_128 = REPO / "dist-arm32" / "Win32Craft.1.2.8-arm32.exe"
PACKAGED_128_SHA = "0f30d9cd8474c5959975711b6b3d9c4421dc593d5bf3021b141652dc250fa36f"
DIR_0104 = Path(os.environ.get(
    "ARM32_DIR_0104", r"D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]"))
EXE_0104 = Path(os.environ.get("ARM32_EXE_0104", DIR_0104 / "MCPE.Host.WindowsPhone.exe"))
FRIEND_DIR = Path(os.environ.get("ARM32_FRIEND_DIR", r"D:\repo\win32craft\0.10.4.40\Win32Craft_ARM\TEST\MCPE_0.10.4.40"))
VCLIBS_APPX = Path(os.environ.get(
    "ARM32_VCLIBS120_APPX", r"D:\repo\armdeps\Microsoft.VCLibs.120.00.Phone_12.0.30113.0_arm__8wekyb3d8bbwe.Appx"))
PLAN_JSON = REPO / "arm32" / "v0_10_4" / "facts" / "pe_patch_plan.json"
PLAN_SHA_NO_FL91 = "453ce59a2100dd7def909ac4871fd58308ec0052572ec73af07c9ac50b15db31"

spec = importlib.util.spec_from_file_location("arm32_patcher", PATCHER)
ap = importlib.util.module_from_spec(spec)
sys.modules["arm32_patcher"] = ap
spec.loader.exec_module(ap)

try:
    import capstone
except Exception:
    capstone = None
try:                                   # the `keystone` name may be shadowed by OpenStack's package
    from keystone.keystone import Ks as _Ks
    from keystone.keystone_const import KS_ARCH_ARM as _KS_ARCH_ARM, KS_MODE_THUMB as _KS_MODE_THUMB
    keystone = True
except Exception:
    keystone = None
try:
    import unicorn
    from unicorn import arm_const as _uc_arm
except Exception:
    unicorn = None

sys.path.insert(0, str(HERE / "arm32"))
try:
    import arm32re
except Exception as error:  # pragma: no cover
    arm32re = None
    ARM32RE_ERROR = error

counts = {"pass": 0, "fail": 0, "skip": 0}


def check(name: str, ok: bool, detail: str = "") -> bool:
    counts["pass" if ok else "fail"] += 1
    detail = " | ".join(str(detail).splitlines()) if detail else ""
    print(("PASS  " if ok else "FAIL  ") + name + (f"   [{detail}]" if detail and (not ok or len(detail) < 120) else ""))
    return ok


def skip(name: str, why: str) -> None:
    counts["skip"] += 1
    print(f"SKIP  {name}   [{why}]")


def refuses(name: str, fn, needle: str = "") -> bool:
    try:
        fn()
    except ap.PatchError as error:
        return check(name, needle in str(error), str(error)[:140])
    except Exception as error:  # a crash is not a clean refusal
        return check(name, False, f"crashed: {type(error).__name__}: {error}")
    return check(name, False, "was NOT refused")


def sha(data) -> str:
    return hashlib.sha256(bytes(data)).hexdigest()


def need(*paths: Path) -> bool:
    return all(Path(p).is_file() for p in paths)


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def diff_runs(a: bytes, b: bytes) -> list[tuple[int, int]]:
    """Maximal runs [start, end) where a and b differ (b may be longer: the tail is one run)."""
    runs: list[list[int]] = []
    n = min(len(a), len(b))
    step = 1 << 16
    for base in range(0, n, step):
        if a[base:base + step] == b[base:base + step]:
            continue
        for i in range(base, min(base + step, n)):
            if a[i] != b[i]:
                if runs and runs[-1][1] == i:
                    runs[-1][1] = i + 1
                else:
                    runs.append([i, i + 1])
    if len(b) > n:
        runs.append([n, len(b)])
    return [tuple(r) for r in runs]


def imagehlp_checksum(path: Path):
    try:
        import ctypes
        from ctypes import wintypes
        lib = ctypes.WinDLL("imagehlp")
        header, computed = wintypes.DWORD(), wintypes.DWORD()
        rc = lib.MapFileAndCheckSumW(ctypes.c_wchar_p(str(path)), ctypes.byref(header), ctypes.byref(computed))
        return (header.value, computed.value) if rc == 0 else None
    except Exception:
        return None


def emulate_bootstrap(data: bytes, profile: dict, base: int, cell_rva: int) -> tuple[bool, str]:
    """Run call site -> trampoline in Unicorn with the image mapped at `base` (no relocation applied:
    the code path must not depend on the base)."""
    pe = ap.PE(data)
    cs = profile["exe"]["bootstrap"]["call_site"]
    uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB)
    uc.mem_map(base, pe.get("size_of_image"))
    uc.mem_write(base, bytes(data[:pe.get("size_of_headers")]))
    for s in pe.sections:
        uc.mem_write(base + s.va, bytes(data[s.roff:s.roff + min(s.rsize, s.extent)]))
    export = 0x70000000
    uc.mem_map(export, 0x1000)
    uc.mem_write(export, bytes.fromhex("00bf00bf"))
    uc.mem_write(base + cell_rva, struct.pack("<I", export | 1))       # what the loader stores
    uc.reg_write(_uc_arm.UC_ARM_REG_R0, 0x11111111)
    uc.reg_write(_uc_arm.UC_ARM_REG_R3, 0x33333333)
    try:
        uc.emu_start((base + cs["rva"]) | 1, export, count=64)
    except unicorn.UcError as error:
        return False, f"emulation error {error}"
    pc = uc.reg_read(_uc_arm.UC_ARM_REG_PC)
    lr = uc.reg_read(_uc_arm.UC_ARM_REG_LR)
    ip = uc.reg_read(_uc_arm.UC_ARM_REG_R12)
    ok = pc == export and lr == (base + cs["rva"] + 4) | 1 and ip == export | 1 and         uc.reg_read(_uc_arm.UC_ARM_REG_R0) == 0x11111111 and uc.reg_read(_uc_arm.UC_ARM_REG_R3) == 0x33333333
    return ok, f"pc=0x{pc:X} lr=0x{lr:X} ip=0x{ip:X}"


def invariants(label: str, data: bytes, orig: bytes, profile: dict, host_dll: str = "win32craft.dll",
               export: str = "Win32Bootstrap", expect_checksum_kept: bool = True) -> None:
    pe = ap.PE(data, label)
    po = ap.PE(orig, label + " (orig)")
    sa = pe.get("section_alignment")
    wx = [s.name for s in pe.sections if s.executable and s.writable]
    check(f"{label}: no section is both writable and executable", not wx, ",".join(wx))
    check(f"{label}: SizeOfImage multiple of SectionAlignment",
          pe.get("size_of_image") % sa == 0, f"0x{pe.get('size_of_image'):X}")
    last = max(pe.sections, key=lambda s: s.va)
    check(f"{label}: SizeOfImage == align(last section end)",
          pe.get("size_of_image") == ap.align_up(last.va + last.extent, sa))
    ordered = all(a.va + a.extent <= b.va for a, b in zip(pe.sections, pe.sections[1:]))
    check(f"{label}: sections ascending and non-overlapping", ordered)
    check(f"{label}: entry point unchanged", pe.get("entry") == po.get("entry"), f"0x{pe.get('entry'):X}")
    check(f"{label}: APPCONTAINER cleared",
          not pe.get("dll_characteristics") & ap.DLLCHAR_APPCONTAINER and
          po.get("dll_characteristics") & ap.DLLCHAR_APPCONTAINER,
          f"0x{po.get('dll_characteristics'):04X} -> 0x{pe.get('dll_characteristics'):04X}")
    if expect_checksum_kept:
        check(f"{label}: header checksum kept as in the stock exe", pe.get("checksum") == po.get("checksum"),
              f"0x{pe.get('checksum'):X}")

    # own import parser
    imports = pe.imports()
    host = imports[-1]
    check(f"{label}: own parser: last import is {host_dll}!{export}",
          host.dll.lower() == host_dll and [f.name for f in host.funcs] == [export],
          f"{host.dll} {[f.name for f in host.funcs]}")
    cell = host.funcs[0].iat_rva
    hint_rva = pe.u32_rva(cell)
    check(f"{label}: IAT cell holds the hint RVA of {export}", pe.cstr(hint_rva + 2) == export.encode())
    sec = pe.section_of_rva(cell)
    check(f"{label}: IAT cell lives in a non-executable, writable-or-readonly data section",
          sec is not None and not sec.executable, f"{sec.name if sec else None}")

    # arm32re (independent parser used by the RE toolkit)
    if arm32re is None:
        skip(f"{label}: arm32re import parse", f"arm32re not importable: {ARM32RE_ERROR}")
    else:
        with tempfile.NamedTemporaryFile(suffix=".exe", delete=False) as tmp:
            tmp.write(data)
            tmp_path = tmp.name
        try:
            re_pe = arm32re.Pe(tmp_path)
            re_imports = re_pe.imports()
            mine = [(d.dll, f.name if f.name else "#%d" % f.ordinal, f.iat_rva) for d in imports for f in d.funcs]
            theirs = [(i["dll"], i["name"], i["iat"]) for i in re_imports if not i["delay"]]
            check(f"{label}: arm32re parses the import table identically ({len(theirs)} imports)", mine == theirs,
                  f"{len(mine)} vs {len(theirs)}")
            ours = [i for i in re_imports if i["dll"].lower() == host_dll and i["name"] == export]
            check(f"{label}: arm32re sees {host_dll}!{export} at IAT 0x{cell:X}",
                  len(ours) == 1 and ours[0]["iat"] == cell)
            check(f"{label}: arm32re entry == stock entry", re_pe.entry == po.get("entry"))
        finally:
            os.unlink(tmp_path)

    # call site / trampoline
    boot = profile["exe"]["bootstrap"]
    cs, tr = boot["call_site"], boot["trampoline"]
    tmpl = ap.TRAMPOLINES[tr["kind"]]
    decoded = ap.decode_branch(pe.read_rva(cs["rva"], 4), cs["rva"])
    check(f"{label}: BL at 0x{cs['rva']:X} decodes (own decoder) to trampoline 0x{tr['rva']:X}",
          decoded == (cs["kind"], tr["rva"]), str(decoded))
    check(f"{label}: original call site targeted 0x{cs['old_target']:X}",
          ap.decode_branch(po.read_rva(cs["rva"], 4), cs["rva"]) == (cs["kind"], cs["old_target"]))
    if capstone is None:
        skip(f"{label}: capstone decode of call site / trampoline", "capstone not installed")
    else:
        md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
        ins = list(md.disasm(pe.read_rva(cs["rva"], 4), cs["rva"]))
        check(f"{label}: capstone: call site is `bl #0x{tr['rva']:x}`",
              len(ins) == 1 and ins[0].mnemonic == "bl" and ins[0].op_str == f"#0x{tr['rva']:x}",
              ins[0].mnemonic + " " + ins[0].op_str if ins else "no decode")
        body = pe.read_rva(tr["rva"], tmpl["length"])
        text = [f"{i.mnemonic} {i.op_str}".strip() for i in md.disasm(body[:tmpl["literal_offset"]], tr["rva"])]
        check(f"{label}: capstone: trampoline = {' ; '.join(tmpl['asm'])}", text == tmpl["asm"], "; ".join(text))
        # micro emulation of the 4 instructions (PC semantics of Thumb-2)
        literal = struct.unpack_from("<I", body, tmpl["literal_offset"])[0]
        pc_ldr = (tr["rva"] + 4) & ~3
        ip = struct.unpack_from("<I", body, (pc_ldr + 8) - tr["rva"])[0]            # ldr.w ip,[pc,#8]
        ip = (ip + (tr["rva"] + 4 + 4)) & 0xFFFFFFFF                                 # add ip,pc (pc=tramp+8)
        check(f"{label}: emulation: trampoline computes IAT cell RVA 0x{cell:X} at any base (no absolute address)",
              ip == cell and literal == struct.unpack_from("<I", body, 12)[0], f"computed 0x{ip:X}")
    if unicorn is None:
        skip(f"{label}: unicorn run of call site -> trampoline -> export", "unicorn not installed")
    else:
        for base in (0x400000, 0x2A30000):
            ok, detail = emulate_bootstrap(data, profile, base, cell)
            check(f"{label}: unicorn: image mapped at 0x{base:X}, `bl` -> trampoline -> `bx` lands on the export "
                  f"read from the IAT cell, LR = call site + 4", ok, detail)
    check(f"{label}: no base relocation over call site / trampoline",
          not pe.relocs_overlapping(cs["rva"], 4) and not pe.relocs_overlapping(tr["rva"], tmpl["length"]))
    check(f"{label}: no .pdata function over the trampoline", pe.function_covering(tr["rva"], tmpl["length"]) is None)
    text = pe.section_by_name(tr["section"])
    check(f"{label}: trampoline inside {text.name} VirtualSize", tr["rva"] + tmpl["length"] <= text.va + text.vsize and
          text.executable)
    check(f"{label}: relocation and .pdata directories untouched",
          pe.dd(5) == po.dd(5) and pe.dd(3) == po.dd(3) and pe.dd(10) == po.dd(10))


# ---------------------------------------------------------------------------
# 0. unit tests
# ---------------------------------------------------------------------------

def test_units() -> None:
    rnd = random.Random(1234)
    ok = True
    for _ in range(4000):
        site = rnd.randrange(0, 1 << 26) & ~1
        disp = rnd.randrange(-(1 << 24), 1 << 24) & ~1
        target = site + 4 + disp
        if target < 0:
            continue
        for kind in ("bl", "b.w"):
            code = ap.encode_branch(kind, site, target)
            ok &= ap.decode_branch(code, site) == (kind, target)
    check("branch encode/decode round trip (4000 random sites x BL/B.W, +-16 MiB)", ok)
    edge = []
    for disp in (-(1 << 24), (1 << 24) - 2, 0, -2, 2):
        edge.append(ap.decode_branch(ap.encode_branch("bl", 0x400000, 0x400000 + 4 + disp), 0x400000)
                    == ("bl", 0x400000 + 4 + disp))
    check("branch range edges (+-16 MiB, 0, +-2) encode and decode", all(edge))
    refuses("BL beyond +16 MiB is refused", lambda: ap.encode_branch("bl", 0x1000, 0x1000 + 4 + (1 << 24)), "outside")
    refuses("BL beyond -16 MiB is refused", lambda: ap.encode_branch("bl", 0x2000000, 0x2000000 + 4 - (1 << 24) - 2), "outside")
    refuses("BL to an odd target is refused", lambda: ap.encode_branch("bl", 0x1000, 0x2001), "odd")
    refuses("encoding a BLX/other kind is refused", lambda: ap.encode_branch("blx", 0x1000, 0x2000), "cannot encode")
    if keystone is None:
        skip("keystone cross-check of BL / B.W / trampoline encodings", "keystone not installed")
    else:
        ks = _Ks(_KS_ARCH_ARM, _KS_MODE_THUMB)
        good = True
        for _ in range(300):
            site = (rnd.randrange(0x1000, 1 << 24)) & ~1
            target = (site + 4 + (rnd.randrange(-(1 << 22), 1 << 22) & ~1)) & ~1
            for kind, mnem in (("bl", "bl"), ("b.w", "b.w")):
                enc, _ = ks.asm(f"{mnem} #{target}", site)
                good &= bytes(enc) == ap.encode_branch(kind, site, target)
        check("keystone agrees with encode_branch (300 random BL + B.W)", good)
        enc, _ = ks.asm("; ".join(ap.TRAMPOLINES["pic_ldr_ip"]["asm"]), 0x20A364)
        check("keystone assembles the trampoline template to the table bytes",
              bytes(enc) == ap.TRAMPOLINES["pic_ldr_ip"]["code"], bytes(enc).hex())
    refuses("placeholder profile v1_16_40 is refused", lambda: ap.ExePatcher(b"MZ" + b"\0" * 400, ap.PROFILES["v1_16_40"]),
            "placeholder")


# ---------------------------------------------------------------------------
# 1. v1_2_8
# ---------------------------------------------------------------------------

def test_v128(tmp: Path) -> None:
    if not need(EXE_128, OLD_PATCHER):
        skip("v1_2_8 tests", f"missing {EXE_128} or old patcher")
        return
    profile = ap.PROFILES["v1_2_8"]
    data = EXE_128.read_bytes()
    out, rep = ap.ExePatcher(data, profile).run()
    check("v1_2_8: engine runs; post-patch verification clean", all(c["ok"] for c in rep["checks"]),
          f"{len(rep['checks'])} checks")
    old_dir = tmp / "old128"
    old_dir.mkdir()
    old_exe, old_fmod = old_dir / "old.exe", old_dir / "old_fmod.dll"
    cmd = [sys.executable, str(OLD_PATCHER), str(EXE_128), str(old_exe)]
    if FMOD_128.is_file():
        cmd += ["--fmod-dll", str(FMOD_128), "--fmod-out", str(old_fmod)]
    run = subprocess.run(cmd, capture_output=True, text=True)
    check("v1_2_8: old patcher (tools/1.2.8-arm32-patcher.py) ran", run.returncode == 0, run.stderr[-200:])
    old = old_exe.read_bytes()
    check(f"v1_2_8: engine output BYTE-IDENTICAL to the old patcher ({sha(out)[:16]}...)", out == old,
          f"engine {sha(out)} old {sha(old)}")
    check("v1_2_8: SHA-256 == 0f30d9cd... recorded for the device-proven exe", sha(out) == PACKAGED_128_SHA)
    if PACKAGED_128.is_file():
        check("v1_2_8: == dist-arm32/Win32Craft.1.2.8-arm32.exe on disk", sha(out) == sha(PACKAGED_128.read_bytes()))
    else:
        skip("v1_2_8: compare with dist-arm32 exe", "file not present")
    runs = diff_runs(data, out)
    expected_runs = [(0x146, 0x148), (0x160, 0x164), (0x190, 0x194), (0x19E, 0x1A0), (0x1C0, 0x1C8),
                     (0x240, 0x244), (0x350, 0x378), (0xB48ECA, 0xB48ECE), (0xC362A0, 0xC362B0),
                     (0x1328CDE, 0x1328CF1), (0x132C3C8, 0x132C3D8), (0x132CD0A, 0x132CD1B),
                     (0x132CE36, 0x132CE4A)]
    flat = [r for r in runs if r[0] < len(data)]
    check("v1_2_8: independent diff vs stock exe only touches the documented ranges + appended section",
          all(any(a <= r[0] and r[1] <= b for a, b in [(x, y) for x, y in expected_runs]) for r in flat) and
          runs[-1] == (len(data), len(out)), f"{len(runs)} runs")
    invariants("v1_2_8 exe", out, data, profile)
    redirected, rrep = ap.ExePatcher(data, profile, ap.Options(redirect_api_sets=True)).run()
    old_r = old_dir / "old_redirect.exe"
    subprocess.run([sys.executable, str(OLD_PATCHER), str(EXE_128), str(old_r), "--redirect-api-sets"],
                   capture_output=True, text=True, check=True)
    check("v1_2_8: --redirect-api-sets variant BYTE-IDENTICAL to the old patcher", redirected == old_r.read_bytes(),
          sha(redirected))
    if FMOD_128.is_file() and old_fmod.is_file():
        comp = profile["companions"][0]
        fmod_out, frep = ap.DllPatcher(FMOD_128.read_bytes(), comp, ap.Options(), "fmod").run()
        check("v1_2_8: fmod_ARM.dll BYTE-IDENTICAL to the old patcher", fmod_out == old_fmod.read_bytes(), sha(fmod_out))
        check("v1_2_8: fmod structural checks", all(c["ok"] for c in frep["checks"]), f"{len(frep['checks'])} checks")
        keep, krep = ap.DllPatcher(FMOD_128.read_bytes(), comp, ap.Options(skip={"skip_winrt_init"}), "fmod").run()
        check("v1_2_8: --keep-winrt-init leaves exactly the 4 skip-init bytes alone",
              diff_runs(keep, fmod_out) == [(0x78A4F, 0x78A52)])
    # negative
    bad = bytearray(data)
    bad[0xB48ECA] ^= 0xFF
    refuses("v1_2_8: corrupted call-site bytes refused (hash mismatch)", lambda: ap.ExePatcher(bytes(bad), profile).run(),
            "unsupported input SHA-256")
    refuses("v1_2_8: corrupted call-site bytes refused even with --allow-hash-mismatch (old-bytes check)",
            lambda: ap.ExePatcher(bytes(bad), profile, ap.Options(allow_hash_mismatch=True)).run(), "")
    refuses("v1_2_8: already patched exe refused", lambda: ap.ExePatcher(out, profile).run(), "")


# ---------------------------------------------------------------------------
# 2. v0_10_4
# ---------------------------------------------------------------------------

def test_v0104(tmp: Path) -> None:
    if not need(EXE_0104):
        skip("v0_10_4 tests", f"missing {EXE_0104}")
        return
    profile = ap.PROFILES["v0_10_4"]
    data = EXE_0104.read_bytes()
    check("v0_10_4: stock exe SHA-256 == plan input 6c0a8adc...", sha(data).startswith("6c0a8adc") and
          len(data) == 14759424)
    plain, prep = ap.ExePatcher(data, profile, ap.Options(skip={"fl91"})).run()
    check("v0_10_4: plan table only -> SHA-256 == 453ce59a... (pe_patch_plan.json)", sha(plain) == PLAN_SHA_NO_FL91, sha(plain))
    full, frep = ap.ExePatcher(data, profile).run()
    check("v0_10_4: default output SHA-256 == recorded variant hash", frep["expected_output"]["match"] is True,
          sha(full))
    check("v0_10_4: output size 14760448, SizeOfImage 0xE6F000, 7 sections",
          len(full) == 14760448 and ap.PE(full).get("size_of_image") == 0xE6F000 and ap.PE(full).nsec == 7)
    d3 = ap.PE(full).section_by_name(".d3imp")
    check("v0_10_4: .d3imp = RVA 0xE6E000 VS 0x23C raw 0xE13600 RS 0x400 flags 0xC0000040 (RW, not X)",
          (d3.va, d3.vsize, d3.roff, d3.rsize, d3.flags) == (0xE6E000, 0x23C, 0xE13600, 0x400, 0xC0000040), str(d3.as_list()))

    # plan edit table, entry by entry (independent data: pe_patch_plan.json)
    if PLAN_JSON.is_file():
        plan = json.loads(PLAN_JSON.read_text(encoding="utf-8"))["patch_profile"]
        mine = {(e["file_offset"], e["old_hex"], e["new_hex"]) for e in prep["edits"] if not e.get("appended")}
        miss = []
        for e in plan["edits"]:
            key = (int(e["file_offset"], 16), e["old_hex"], e["new_hex"])
            if key not in mine:
                miss.append(e["id"])
        check(f"v0_10_4: all {len(plan['edits'])} plan edits reproduced (offset, old, new)", not miss, f"missing {miss}")
        app = plan["append_section"]
        blob = plain[int(app["raw_offset"], 16):int(app["raw_offset"], 16) + 0x23C]
        check("v0_10_4: appended .d3imp content == plan content_hex / content_sha256",
              blob.hex() == app["content_hex_first_0x23C_bytes"] and sha(blob) == app["content_sha256"])
        check("v0_10_4: trampoline literal / bytes == plan derived_values",
              plain[0x209764:0x209774].hex() == plan["derived_values"]["trampoline_bytes"] and
              plain[0x1FFE72:0x1FFE76].hex() == plan["derived_values"]["callsite_new_bytes"])
        # plan V2: byte diff touches ONLY the listed ranges
        allowed = [(0x126, 0x128), (0x140, 0x144), (0x170, 0x174), (0x17E, 0x180), (0x1A0, 0x1A8), (0x220, 0x224),
                   (0x308, 0x330), (0x1FFE72, 0x1FFE76), (0x209764, 0x209774)]
        runs = diff_runs(data, plain)
        inside = [r for r in runs if r[0] < len(data)]
        check("v0_10_4: independent diff vs stock exe == plan V2 ranges + 0x400 appended bytes",
              all(any(a <= r[0] and r[1] <= b for a, b in allowed) for r in inside) and runs[-1] == (len(data), len(data) + 0x400),
              f"{runs[:3]}...")
        fl_runs = diff_runs(plain, full)
        check("v0_10_4: FL 9_1 edit is exactly ONE byte: file 0x2339F9 (RVA 0x2351F9) 0x33 -> 0x31",
              fl_runs == [(0x2339F9, 0x2339FA)] and plain[0x2339F9] == 0x33 and full[0x2339F9] == 0x31)
    else:
        skip("v0_10_4: plan edit table comparison", f"{PLAN_JSON} missing")
    check("v0_10_4: the stock bytes at the FL 9_1 string are `_level_9_3\\0` (5f6c6576656c5f395f3300 @ RVA 0x2351F0)",
          ap.PE(data).read_rva(0x2351F0, 11).hex() == "5f6c6576656c5f395f3300")
    # independent re-derivation of the 'exactly one xref' claim for the string whose byte we patch
    pe_in = ap.PE(data)
    base = pe_in.get("image_base")
    refs = []
    for rva, kind in pe_in.reloc_entries():
        if kind == 3:
            value = pe_in.u32_rva(rva)
        elif kind == 7:
            a, b = ap.decode_ins(pe_in, rva), ap.decode_ins(pe_in, rva + 4)
            if not (a.op == "movw" and b.op == "movt"):
                continue
            value = (b.imm << 16) | a.imm
        else:
            continue
        if base + 0x2351F0 <= value < base + 0x2351FB:
            refs.append(rva)
    check("v0_10_4: the `_level_9_3` string has exactly ONE relocated reference (re-derived from .reloc: MOV32 at 0xD742C)",
          refs == [0xD742C], str([hex(r) for r in refs]))
    check("v0_10_4: `_level_9_` occurs once in the stock file (ASCII), never as UTF-16",
          data.count(b"_level_9_") == 1 and data.count("_level_9_".encode("utf-16le")) == 0)
    invariants("v0_10_4 exe", full, data, profile)
    invariants("v0_10_4 exe (no FL91)", plain, data, profile)

    # recompute checksum policy, validated against imagehlp
    rc, rrep = ap.ExePatcher(data, profile, ap.Options(checksum="recompute")).run()
    got = ap.PE(rc).get("checksum")
    p = tmp / "recomputed.exe"
    p.write_bytes(rc)
    ih = imagehlp_checksum(p)
    if ih is None:
        skip("v0_10_4: --checksum recompute equals imagehlp!MapFileAndCheckSum", "imagehlp unavailable")
    else:
        check("v0_10_4: --checksum recompute == imagehlp!MapFileAndCheckSum", got == ih[1] and got != 0,
              f"mine 0x{got:X} imagehlp 0x{ih[1]:X}")
    check("v0_10_4: recompute changes ONLY the checksum field vs the default output",
          diff_runs(full, rc)[0][0] == ap.PE(full).off("checksum") and len(diff_runs(full, rc)) == 1)
    # base relocation policy 'drop' (not used by any profile) stays structurally valid
    dr, _ = ap.ExePatcher(data, profile, ap.Options(base_relocs="drop")).run()
    pd = ap.PE(dr)
    check("v0_10_4: base_relocs=drop clears the directory, sets RELOCS_STRIPPED, clears DYNAMIC_BASE (option only)",
          pd.dd(5) == (0, 0) and pd.get("characteristics") & 1 and not pd.get("dll_characteristics") & 0x40)

    # informational comparison with the untrusted friend exe
    friend = FRIEND_DIR / "Win32Craft.0.10.4-arm32.exe"
    if friend.is_file():
        fd = friend.read_bytes()
        fruns = diff_runs(fd, plain)
        pf = ap.PE(fd)
        d3f = pf.section_by_name(".d3imp")
        print(f"INFO  friend exe differs from ours in {len(fruns)} runs; its .d3imp flags 0x{d3f.flags:08X} "
              f"({ap.flags_text(d3f.flags)}), trampoline in .d3imp, ours in .text padding (expected differences)")
        check("v0_10_4: friend's .d3imp is W+X (confirms why it is not reproduced)", d3f.executable and d3f.writable)

    # negative tests
    refuses("v0_10_4: already patched (our own output) refused", lambda: ap.ExePatcher(full, profile).run(), "already patched")
    if friend.is_file():
        refuses("v0_10_4: friend's patched exe refused", lambda: ap.ExePatcher(friend.read_bytes(), profile).run(), "")
    bad = bytearray(data)
    bad[0x1FFE72] ^= 0x01
    refuses("v0_10_4: call-site bytes changed -> refused even with --allow-hash-mismatch",
            lambda: ap.ExePatcher(bytes(bad), profile, ap.Options(allow_hash_mismatch=True)).run(), "")
    bad = bytearray(data)
    bad[0x2339F9] = 0x32
    refuses("v0_10_4: FL91 byte not 0x33 -> refused (context / old-bytes verification)",
            lambda: ap.ExePatcher(bytes(bad), profile, ap.Options(allow_hash_mismatch=True)).run(), "")
    bad = bytearray(data)
    bad[0x2339F2] = 0x41
    refuses("v0_10_4: FL91 context bytes changed -> refused",
            lambda: ap.ExePatcher(bytes(bad), profile, ap.Options(allow_hash_mismatch=True)).run(), "context")
    bad = bytearray(data)
    bad[0x209770] = 0x01
    refuses("v0_10_4: trampoline slack not zero -> refused",
            lambda: ap.ExePatcher(bytes(bad), profile, ap.Options(allow_hash_mismatch=True)).run(), "not all zero")
    wx = json.loads(json.dumps(profile))
    wx["exe"]["new_section"]["characteristics"] = 0xE0000060
    refuses("v0_10_4: profile asking for a W+X section is refused", lambda: ap.ExePatcher(data, wx).run(), "W+X")
    far = json.loads(json.dumps(profile))
    far["exe"]["bootstrap"]["call_site"]["rva"] = 0x200A72
    far["exe"]["bootstrap"]["trampoline"]["rva"] = 0x20A364
    far["exe"]["bootstrap"]["call_site"]["old_target"] = 0x203EFD
    refuses("v0_10_4: wrong expected callee of the call site -> refused", lambda: ap.ExePatcher(data, far).run(), "profile expects")
    rel = json.loads(json.dumps(profile))
    first_reloc = ap.PE(data).reloc_entries()[0][0]
    off = ap.PE(data).rva_to_off(first_reloc, raw_backed=True)
    rel["exe"]["byte_patches"] = [{"id": "test.reloc", "rva": first_reloc, "old": data[off:off + 2].hex(),
                                   "new": "0000", "reason": "test"}]
    refuses("v0_10_4: byte patch over a base relocation is refused", lambda: ap.ExePatcher(data, rel).run(), "relocation")
    wrong = json.loads(json.dumps(profile))
    wrong["exe"]["new_section"]["expect_size_of_image"] = 0xE70000
    refuses("v0_10_4: unexpected SizeOfImage vs profile refused", lambda: ap.ExePatcher(data, wrong).run(), "profile expects")
    far2 = json.loads(json.dumps(profile))
    far2["exe"]["bootstrap"]["trampoline"]["rva"] = 0x20A380
    far2["exe"]["bootstrap"]["trampoline"]["slack"] = None
    far2["exe"]["bootstrap"]["trampoline"].pop("expect_literal")
    far2["exe"]["bootstrap"]["call_site"].pop("new")
    out2, _ = ap.ExePatcher(data, far2).run()
    check("v0_10_4: moving the trampoline slot inside the zero slack works and re-verifies (engine is data driven)",
          ap.decode_branch(ap.PE(out2).read_rva(0x200A72, 4), 0x200A72) == ("bl", 0x20A380))
    custom, crep = ap.ExePatcher(data, profile, ap.Options(dll_name="host.dll", export="Boot")).run()
    cpe = ap.PE(custom)
    check("v0_10_4: custom --dll-name host.dll --export Boot builds and verifies",
          cpe.imports()[-1].dll == "host.dll" and cpe.imports()[-1].funcs[0].name == "Boot" and
          cpe.get("size_of_image") == 0xE6F000, crep["expected_output"].get("note", ""))


# ---------------------------------------------------------------------------
# 3. companions
# ---------------------------------------------------------------------------

def test_companions(tmp: Path) -> None:
    profile = ap.PROFILES["v0_10_4"]
    if not need(VCLIBS_APPX):
        skip("companion DLL tests (genuine originals)", f"{VCLIBS_APPX} missing")
    ident = ap.appx_identity(VCLIBS_APPX) if need(VCLIBS_APPX) else None
    if ident:
        check("VCLibs appx identity is Microsoft.VCLibs.120.00.Phone 12.0.30113.0 arm",
              (ident.get("Name"), ident.get("Version"), ident.get("ProcessorArchitecture")) ==
              ("Microsoft.VCLibs.120.00.Phone", "12.0.30113.0", "arm"), str(ident))
    for comp in profile["companions"]:
        name = comp["name"]
        original = ap.read_appx_member(VCLIBS_APPX, name) if ident else None
        friend = (FRIEND_DIR / name).read_bytes() if (FRIEND_DIR / name).is_file() else None
        if original is not None:
            check(f"{name}: appx original SHA-256 == profile sha256_original", sha(original) == comp["sha256_original"])
            out, rep = ap.DllPatcher(original, comp, ap.Options(), name).run()
            check(f"{name}: original -> patched SHA-256 == profile sha256_patched", sha(out) == comp["sha256_patched"])
            runs = diff_runs(original, out)
            off = comp["dll_characteristics"]["offset"]
            check(f"{name}: patch = exactly the high byte of DllCharacteristics (0x1140 -> 0x0140) at file+0x{off + 1:X}",
                  runs == [(off + 1, off + 2)] and original[off:off + 2] == b"\x40\x11" and out[off:off + 2] == b"\x40\x01",
                  str(runs))
            if friend is not None:
                check(f"{name}: friend copy == our patch of the genuine original", friend == out)
                fruns = diff_runs(original, friend)
                check(f"{name}: friend copy differs from the genuine original ONLY at file+0x{off + 1:X} (0x11 -> 0x01)",
                      fruns == [(off + 1, off + 2)])
            co = ap.PE(original)
            check(f"{name}: genuine header checksum == recomputed (algorithm check; Authenticode kept)",
                  co.get("checksum") == ap.pe_checksum(original), f"0x{co.get('checksum'):X}")
            p = tmp / ("orig_" + name)
            p.write_bytes(original)
            ih = imagehlp_checksum(p)
            if ih:
                check(f"{name}: pe_checksum == imagehlp!MapFileAndCheckSum", ap.pe_checksum(original) == ih[1],
                      f"0x{ap.pe_checksum(original):X}/0x{ih[1]:X}")
            check(f"{name}: all structural checks pass", all(c["ok"] for c in rep["checks"]),
                  "; ".join(c["name"] for c in rep["checks"] if not c["ok"]))
            # the clearing changes the checksum-visible content: header checksum is stale unless recomputed
            ph = ap.PE(out)
            check(f"{name}: kept header checksum equals the checksum of the file with 0x1000 SET (plan: provenance proof)",
                  ph.get("checksum") == ap.pe_checksum(original))
            rc, _ = ap.DllPatcher(original, comp, ap.Options(checksum="recompute"), name).run()
            check(f"{name}: --checksum recompute gives a header checksum valid for the patched file",
                  ap.PE(rc).get("checksum") == ap.pe_checksum(rc))
            tampered = bytearray(original)
            tampered[0x400] ^= 0xFF
            refuses(f"{name}: unknown SHA-256 refused without --allow-unknown-companion",
                    lambda t=bytes(tampered): ap.DllPatcher(t, comp, ap.Options(), name).run(), "neither the original")
            forced, _ = ap.DllPatcher(bytes(tampered), comp, ap.Options(allow_unknown_companion=True), name).run()
            check(f"{name}: --allow-unknown-companion patches only the DllCharacteristics byte",
                  diff_runs(bytes(tampered), forced) == [(off + 1, off + 2)])
        if friend is not None:
            out_f, rep_f = ap.DllPatcher(friend, comp, ap.Options(), name).run()
            off = comp["dll_characteristics"]["offset"]
            restored = bytearray(friend)
            restored[off + 1] = 0x11
            check(f"{name}: friend copy provenance proof: checksum recomputed with 0x1000 SET == its header checksum",
                  ap.pe_checksum(bytes(restored)) == ap.PE(friend).get("checksum"))
            check(f"{name}: friend copy recognised (state patched_known) and left byte-identical",
                  out_f == friend and rep_f["state_in"] == "patched_known" and not rep_f["edits"])
            check(f"{name}: friend copy structure (ARM DLL, APPCONTAINER clear, no W+X, imports resolve to ARM system DLLs)",
                  all(c["ok"] for c in rep_f["checks"]), "; ".join(c["name"] for c in rep_f["checks"] if not c["ok"]))
        elif original is None:
            skip(f"{name}: companion tests", "neither appx original nor friend copy available")
    # imports of the three DLLs use only system / sibling names
    if need(VCLIBS_APPX):
        names = set()
        for comp in profile["companions"]:
            data = ap.read_appx_member(VCLIBS_APPX, comp["name"])
            names |= {d.dll.lower() for d in ap.PE(data).imports()}
        odd = sorted(n for n in names if not ap.KNOWN_SYSTEM_DLL.match(n))
        check("VC120 DLL imports: api-ms-win-core-* sets, OLEAUT32 and each other only", not odd, str(odd))


# ---------------------------------------------------------------------------
# 4. CLI
# ---------------------------------------------------------------------------

def run_cli(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(PATCHER), *args], capture_output=True, text=True)


def test_cli(tmp: Path) -> None:
    if not need(EXE_0104):
        skip("CLI tests", "0.10.4 exe missing")
        return
    r = run_cli("detect", str(DIR_0104))
    check("CLI detect <0.10.4 folder> -> v0_10_4, stock, high confidence",
          r.returncode == 0 and "profile      : v0_10_4" in r.stdout and "state        : stock" in r.stdout and "high" in r.stdout,
          r.stdout[-200:] + r.stderr[-200:])
    if need(EXE_128):
        r = run_cli("detect", str(EXE_128.parent))
        check("CLI detect <1.2.8 folder> -> v1_2_8, stock", "profile      : v1_2_8" in r.stdout and "state        : stock" in r.stdout)
    friend = FRIEND_DIR / "Win32Craft.0.10.4-arm32.exe"
    if friend.is_file():
        r = run_cli("detect", str(friend))
        check("CLI detect <friend patched exe> -> v0_10_4, patched (not by this tool), W+X warning",
              "patched (not by this tool)" in r.stdout and "W+X" in r.stdout, r.stdout[-300:])
    r = run_cli("detect", str(PATCHER))
    check("CLI detect on a non-PE file fails cleanly (exit 1, no traceback)", r.returncode == 1 and "Traceback" not in r.stderr,
          r.stderr[-160:])

    before = {p.name: sha(p.read_bytes()) for p in DIR_0104.glob("*") if p.is_file()}
    out = tmp / "cli_out"
    args = ["patch", "--profile", "auto", str(DIR_0104), str(out)]
    if not need(VCLIBS_APPX) and FRIEND_DIR.is_dir():
        args += ["--companion-dir", str(FRIEND_DIR)]       # fall back to the friend copies
    r = run_cli(*args)
    check("CLI patch --profile auto <0.10.4 folder> <out> exit 0", r.returncode == 0, (r.stdout + r.stderr)[-400:])
    after = {p.name: sha(p.read_bytes()) for p in DIR_0104.glob("*") if p.is_file()}
    check("CLI patch: original game folder untouched (read-only input)", before == after)
    rep_path = out / "patch_report.json"
    if rep_path.is_file():
        rep = json.loads(rep_path.read_text(encoding="utf-8"))
        names = sorted(p.name for p in out.iterdir())
        check("CLI patch: out folder holds patched exe + 3 VC120 DLLs + report + copied game files and NOT the stock exe",
              "Win32Craft.0.10.4-arm32.exe" in names and "MCPE.Host.WindowsPhone.exe" not in names and
              all(c in names for c in ("msvcr120_app.dll", "msvcp120_app.dll", "vccorlib120_app.dll", "data", "AppxManifest.xml")),
              str(names))
        check("CLI patch: patch_report.json lists every edit with offset/old/new/reason",
              all({"file_offset", "old_hex", "new_hex", "reason"} <= set(e) for e in rep["exe"]["edits"]) and
              len(rep["exe"]["edits"]) == 11, f"{len(rep['exe']['edits'])} edits")
        check("CLI patch: report verify section has no failed check", rep["verify"]["checks"] and
              not [c for c in rep["verify"]["checks"] if not c["ok"]], f"{len(rep['verify']['checks'])} checks")
        check("CLI patch: report exe sha256 == recorded variant hash and equals the file on disk",
              rep["exe"]["expected_output"]["match"] is True and
              sha((out / "Win32Craft.0.10.4-arm32.exe").read_bytes()) == rep["exe"]["output"]["sha256"])
        check("CLI patch: companions came from the genuine appx (state original) and match recorded hashes",
              all(c["state_in"] == "original" and c["matches_expected"] for c in rep["companions"]) or
              all(c["matches_expected"] for c in rep["companions"]),
              str([(c["name"], c["state_in"]) for c in rep["companions"]]))
        r = run_cli("verify", str(out))
        check("CLI verify <out folder> exit 0", r.returncode == 0 and "FAIL" not in r.stdout, r.stdout[-300:])
        # run the verifier on a damaged copy: flip the trampoline literal
        damaged = bytearray((out / "Win32Craft.0.10.4-arm32.exe").read_bytes())
        damaged[0x209770] ^= 0x01
        dp = tmp / "damaged.exe"
        dp.write_bytes(damaged)
        r = run_cli("verify", str(dp), "--profile", "v0_10_4")
        check("CLI verify flags a corrupted trampoline literal (exit != 0, FAIL line)", r.returncode != 0 and "FAIL" in r.stdout)
    r = run_cli("patch", "--profile", "auto", str(DIR_0104), str(out))
    check("CLI patch refuses a non-empty output folder without --force", r.returncode == 1 and "not empty" in r.stderr, r.stderr[-160:])
    r = run_cli("patch", "--profile", "auto", str(DIR_0104), str(DIR_0104 / "sub_out"))
    check("CLI patch refuses an output folder inside the game folder", r.returncode == 1 and "inside" in r.stderr, r.stderr[-160:])
    r = run_cli("patch", "--profile", "v1_16_40", str(DIR_0104), str(tmp / "never"))
    check("CLI patch --profile v1_16_40 (placeholder slot) is refused and lists what is missing",
          r.returncode == 1 and "placeholder" in r.stderr and "still needs" in r.stderr and not (tmp / "never").exists(),
          r.stderr[-200:])
    r = run_cli("patch", "--profile", "v1_2_8", str(DIR_0104), str(tmp / "never2"))
    check("CLI patch with the wrong profile (v1_2_8 on the 0.10.4 exe) is refused", r.returncode == 1 and not (tmp / "never2").exists(),
          r.stderr[-200:])
    r = run_cli("profiles")
    check("CLI profiles lists v0_10_4, v1_2_8 complete and v1_16_40 placeholder",
          "v0_10_4    complete" in r.stdout and "v1_2_8     complete" in r.stdout and "v1_16_40   placeholder" in r.stdout)
    # JSON profile file mechanism: dump, re-load under another name, patch with it
    dump = json.loads(run_cli("profiles", "--dump", "v0_10_4").stdout)
    dump["name"] = "v0_10_4_clone"
    pf = tmp / "clone.json"
    pf.write_text(json.dumps(dump), encoding="utf-8")
    out2 = tmp / "clone_out"
    r = run_cli("--profile-file", str(pf), "patch", "--profile", "v0_10_4_clone", "--no-copy-folder", "--skip-companions",
                str(DIR_0104), str(out2))
    check("profiles are data: a dumped JSON profile loaded with --profile-file patches to the same bytes",
          r.returncode == 0 and sha((out2 / "Win32Craft.0.10.4-arm32.exe").read_bytes()) ==
          ap.PROFILES["v0_10_4"]["exe"]["expected_outputs"][1]["sha256"], (r.stdout + r.stderr)[-200:])

    # probe on the two known exes: the real call site is the top candidate
    for label, exe, profile_name in (("0.10.4", EXE_0104, "v0_10_4"), ("1.2.8", EXE_128, "v1_2_8")):
        if not exe.is_file():
            skip(f"CLI probe {label}", "exe missing")
            continue
        jp = tmp / f"probe_{profile_name}.json"
        r = run_cli("probe", str(exe), "--json", str(jp))
        site = ap.PROFILES[profile_name]["exe"]["bootstrap"]["call_site"]["rva"]
        pj = json.loads(jp.read_text(encoding="utf-8")) if jp.is_file() else {}
        top = pj.get("startup", {}).get("candidates", [{}])[0].get("site")
        check(f"CLI probe {label}: entry chain, CRT call-site candidate #1 == profile call site 0x{site:X}",
              r.returncode == 0 and top == site and "UNKNOWN version" in r.stdout, f"top {top and hex(top)}")
        check(f"CLI probe {label}: lists padding slots, IAT cells, app model and the profile-needs list",
              "trampoline slots" in r.stdout and "GetActivationFactoryByPCWSTR" in r.stdout and
              "A new profile for this version needs" in r.stdout and "app model:" in r.stdout)
    r = run_cli("probe", str(EXE_0104))
    check("CLI probe 0.10.4 detects a XAML app", "XAML app" in r.stdout)
    if need(EXE_128):
        r = run_cli("probe", str(EXE_128))
        check("CLI probe 1.2.8 detects a CoreWindow app", "CoreWindow app" in r.stdout)
    # probe against an exe of an unknown version: the patched 0.10.4 output has no profile hash but must still be probed
    (tmp / "unk").mkdir()
    unk = tmp / "unk" / "unknown.exe"
    data = bytearray(EXE_0104.read_bytes())
    unk.write_bytes(data)
    r = run_cli("probe", str(unk))
    check("CLI probe on a renamed copy still identifies the profile by SHA-256", "known profile: v0_10_4 (high" in r.stdout)
    data[0x300000] ^= 0x55                      # same layout, different bytes: an 'unknown build of a known layout'
    unk.write_bytes(data)
    r = run_cli("probe", str(unk))
    check("CLI probe on a modified copy: only a medium-confidence guess, call site still found",
          "known profile: v0_10_4 (medium" in r.stdout and "equals the profile's call site" in r.stdout)
    r = run_cli("patch", "--profile", "auto", "--no-copy-folder", "--skip-companions", str(unk), str(tmp / "unk_out"))
    check("CLI patch refuses the modified copy (unsupported SHA-256), writes nothing",
          r.returncode == 1 and "unsupported input SHA-256" in r.stderr and not (tmp / "unk_out" / "Win32Craft.0.10.4-arm32.exe").exists(),
          r.stderr[-200:])


def main() -> int:
    print(f"arm32-patcher self test  (python {sys.version.split()[0]}, capstone {'yes' if capstone else 'NO'}, "
          f"keystone {'yes' if keystone else 'no'}, arm32re {'yes' if arm32re else 'NO'})")
    with tempfile.TemporaryDirectory(prefix="arm32-selftest-") as tmp_name:
        tmp = Path(tmp_name)
        for label, fn in (("unit", test_units), ("v1_2_8", test_v128), ("v0_10_4", test_v0104),
                          ("companions", test_companions), ("cli", test_cli)):
            print(f"--- {label}")
            try:
                fn(tmp) if fn is not test_units else fn()
            except Exception as error:
                import traceback
                traceback.print_exc()
                check(f"{label}: test block completed without an unexpected exception", False, repr(error))
    print(f"RESULT: {counts['pass']} PASS, {counts['fail']} FAIL, {counts['skip']} SKIP")
    print("OVERALL: " + ("PASS" if counts["fail"] == 0 else "FAIL"))
    return 1 if counts["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
