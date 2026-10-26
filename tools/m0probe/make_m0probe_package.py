#!/usr/bin/env python3
"""Assemble the m0probe device folder (dist-m0probe\\) from the game folders of this checkout.

    python tools\\m0probe\\make_m0probe_package.py [--out dist-m0probe] [--exe dist-m0probe\\m0probe.exe]

What goes in (the ARM32 exe is built separately by tools\\m0probe\\build_m0probe_arm32.bat into dist-m0probe\\m0probe.exe):
  data\\redcon\\sh00..sh09.dxbc     the 10 Redcon shaders, decrypted here from Redcon_1.3.0.0_ARM\\res\\shaders (fixed MWC stream cipher)
  data\\gd\\*.dxbc + index.txt      the 30 DXBC containers of libcocos2d.dll (15 programs x ps+vs; the game uses 9 programs = 18 containers)
                                  and the 10 containers of libGLESv2_phone.dll (ANGLE internals), found by scanning the DLLs for 'DXBC'
  data\\bp\\l9_replacements.bin     the 113 level_9_1 replacements for Bad Piggies (games_recon\\bad_piggies_shader_proto\\auto)
  data\\bp\\orig\\                  the two Unity Sprites/Default originals used by the draw test
  run_probe.bat, README.txt       what the user double-clicks / reads
Game data must NOT go into git or the source zip: tools\\make_source_package.py already skips every top-level dist-* and build-* folder.
Use --out build-x64probe\\run --exe build-x64probe\\m0probe_x64.exe to lay out a PC test folder (the exe is copied there as m0probe.exe)."""
import argparse
import os
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
REDCON_SHADERS = ROOT / "Redcon_1.3.0.0_ARM" / "res" / "shaders"
GD_DIR = ROOT / "Geometry_Dash_2.0.1.0"
BP_DIR = ROOT / "games_recon" / "bad_piggies_shader_proto"
BP_ORIG_FILES = ["00_Sprites_Default_b000_vs_756ae2904e92.dxbc", "00_Sprites_Default_b004_ps_3aa3dabc65f1.dxbc"]

# games_recon/geometry_dash_gd-render.md F7: program n = pixel container 2n + vertex container 2n+1 (container order inside libcocos2d.dll);
# names come from matching the 15 SHA-1 keys against the GLSL pairs (9 of 15 match = the stock cocos2d programs the game really uses).
GD_USED_PROGRAMS = {
    2: "ShaderControlSwitch", 3: "ShaderPositionTexture", 4: "ShaderPositionTextureColorAlphaTest", 5: "ShaderPositionLengthTextureColor",
    6: "ShaderPosition_uColor", 7: "ShaderPositionTexture_uColor", 11: "ShaderPositionColor", 13: "ShaderPositionTextureColor", 14: "ShaderPositionTextureA8Color",
}

RUN_PROBE_BAT = r"""@echo off
cd /d "%~dp0"
title m0probe
echo m0probe - please wait, this takes a few minutes. Do not close this window.
echo A small empty window may flash on and off; that is normal.
echo.
m0probe.exe
echo.
echo ----------------------------------------------------------------
echo Finished. Please send the file  m0probe.log  (in this folder) back.
echo ----------------------------------------------------------------
pause
"""

README_TXT = """m0probe - Direct3D device probe for the Surface RT and the Surface 2
=====================================================================

What this is
  A small diagnostic program. It installs nothing and changes no setting. It asks Windows which graphics
  hardware the tablet has and tries the graphics operations that three game ports will need. It writes
  everything into one text file, m0probe.log, next to the program. It runs about 2 to 3 minutes.

How to run it (once on each tablet)
  1. Copy this WHOLE folder (m0probe.exe, run_probe.bat and the data folder) to the tablet, for example to the
     Desktop. Do not split it up: the program reads the files in the data folder.
  2. Double-click run_probe.bat. A black window opens and prints progress. A small empty window may flash on and
     off while it tests - that is normal.
  3. Wait until it prints "Finished" and "Press any key to continue". Do not use the tablet in between. If the
     tablet restarts the graphics driver (screen flickers) or the window closes by itself, just run run_probe.bat
     again once.
  4. Send back the file m0probe.log (it is in the same folder). Nothing else is needed. Please say which tablet
     it was (Surface RT or Surface 2).

If it seems stuck for more than 5 minutes: close the window and send m0probe.log anyway. The log shows how far
it got; each test runs in its own process, so one crashing test does not stop the others.

The last part of the log (below the line "SUMMARY") answers the questions in plain words.
"""


def mwc_dec(data, w=0x1F123BB5, z=0x159A55E5):
    """Redcon's fixed stream cipher (multiply-with-carry): XOR every 32-bit word with (w + (z << 16))."""
    out = bytearray(data)
    n = len(out)
    i = 0
    for _ in range(n // 4):
        r5 = (w + (z << 16)) & 0xFFFFFFFF
        out[i] ^= r5 & 0xFF
        out[i + 1] ^= (r5 >> 8) & 0xFF
        out[i + 2] ^= (r5 >> 16) & 0xFF
        out[i + 3] ^= (r5 >> 24) & 0xFF
        i += 4
        z = ((z & 0xFFFF) * 36969 + ((z >> 16) & 0xFFFF)) & 0xFFFFFFFF
        w = ((w & 0xFFFF) * 18000 + ((w >> 16) & 0xFFFF)) & 0xFFFFFFFF
    r5 = (w + (z << 16)) & 0xFFFFFFFF
    for k in range(n & 3):
        out[i] ^= (r5 >> (8 * k)) & 0xFF
        i += 1
    return bytes(out)


def split_redcon(dec):
    """Decrypted file: big-endian u16 count, then per item big-endian u32 length + DXBC."""
    count = struct.unpack(">H", dec[:2])[0]
    pos = 2
    items = []
    for _ in range(count):
        ln = struct.unpack(">I", dec[pos:pos + 4])[0]
        pos += 4
        items.append(dec[pos:pos + ln])
        pos += ln
    if pos > len(dec):
        raise SystemExit("redcon shaders: item table runs past the end of the file - cipher or format changed")
    if pos != len(dec):   # the 470-byte tail is the program table (basic_p, basic_pc, basic_pct, basic_pt, particles: names + attribute/uniform lists)
        print("redcon: %d trailing bytes after the %d shaders (program/uniform table of the game, not needed here)" % (len(dec) - pos, count))
    for k, it in enumerate(items):
        if it[:4] != b"DXBC":
            raise SystemExit("redcon shader %d does not start with DXBC - decryption failed" % k)
    return items


def find_containers(raw):
    """All DXBC containers embedded in a PE image (same scan as the recon helper dxbc.py)."""
    res = []
    pos = 0
    while True:
        i = raw.find(b"DXBC", pos)
        if i < 0:
            break
        if i + 32 <= len(raw):
            one, total, nch = struct.unpack_from("<III", raw, i + 20)
            if one == 1 and 0 < nch < 32 and 32 < total < 0x100000 and i + total <= len(raw):
                res.append((i, total))
                pos = i + total
                continue
        pos = i + 4
    return res


def chunk_of(blob, fourcc):
    nch = struct.unpack_from("<I", blob, 28)[0]
    for k in range(nch):
        o = struct.unpack_from("<I", blob, 32 + 4 * k)[0]
        if blob[o:o + 4] == fourcc:
            return o + 8, struct.unpack_from("<I", blob, o + 4)[0]
    return None


def shader_type(blob):
    c = chunk_of(blob, b"SHDR") or chunk_of(blob, b"SHEX")
    if not c:
        return "??"
    return {0: "ps", 1: "vs", 2: "gs"}.get(struct.unpack_from("<I", blob, c[0])[0] >> 16, "??")


def level9(blob):
    c = chunk_of(blob, b"Aon9")
    if not c:
        return "none"
    off = struct.unpack_from("<I", blob, c[0] + 12)[0]
    tok = struct.unpack_from("<I", blob, c[0] + off)[0]
    return {0x0200: "9_1", 0x0201: "9_3"}.get(tok & 0xFFFF, "9_?")


def write(path, data, text=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    if text:
        with open(path, "w", newline="\r\n", encoding="ascii") as f:
            f.write(data)
    else:
        path.write_bytes(data)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=str(ROOT / "dist-m0probe"), help="folder to assemble (default dist-m0probe)")
    ap.add_argument("--exe", default=str(ROOT / "dist-m0probe" / "m0probe.exe"), help="the built probe exe (default dist-m0probe\\m0probe.exe); copied as m0probe.exe")
    args = ap.parse_args()
    out = Path(args.out)
    if not out.is_absolute():
        out = ROOT / out
    exe = Path(args.exe)
    if not exe.is_absolute():
        exe = ROOT / exe
    data = out / "data"
    out.mkdir(parents=True, exist_ok=True)

    # ---- Redcon ----
    if not REDCON_SHADERS.is_file():
        raise SystemExit("missing %s (the Redcon game folder)" % REDCON_SHADERS)
    items = split_redcon(mwc_dec(REDCON_SHADERS.read_bytes()))
    for k, it in enumerate(items):
        write(data / "redcon" / ("sh%02d.dxbc" % k), it)
    print("redcon: %d shaders (%s)" % (len(items), " ".join("%s/%s" % (shader_type(i), level9(i)) for i in items)))

    # ---- Geometry Dash ----
    index = ["# file used level label   (used = 1: one of the 9 programs the game really uses; level = Direct3D 9 payload of the container)"]
    for dll, prefix in (("libcocos2d.dll", "libcocos2d"), ("libGLESv2_phone.dll", "libGLESv2_phone")):
        p = GD_DIR / dll
        if not p.is_file():
            raise SystemExit("missing %s (the Geometry Dash game folder)" % p)
        raw = p.read_bytes()
        cs = find_containers(raw)
        print("%s: %d DXBC containers" % (dll, len(cs)))
        for n, (off, total) in enumerate(cs):
            blob = raw[off:off + total]
            t = shader_type(blob)
            name = "%s_%02d_%s.dxbc" % (prefix, n, t)
            write(data / "gd" / name, blob)
            if prefix == "libcocos2d":
                prog = n // 2
                want = "ps" if n % 2 == 0 else "vs"
                used = prog in GD_USED_PROGRAMS
                if t != want:
                    raise SystemExit("%s container %d is %s, expected %s (pixel = 2n, vertex = 2n+1)" % (dll, n, t, want))
                label = ("%s (%s)" % (GD_USED_PROGRAMS[prog], t)) if used else ("unused sample-app program #%d (%s)" % (prog, t))
                index.append("%s %d %s %s" % (name, 1 if used else 0, level9(blob), label))
            else:
                index.append("%s 0 %s ANGLE internal blit/clear shader (%s)" % (name, level9(blob), t))
    write(data / "gd" / "index.txt", "\n".join(index) + "\n", text=True)
    used_n = sum(1 for l in index if l.split(" ")[1] == "1")
    print("gd: index.txt written, %d containers marked as used by the game (expected 18)" % used_n)
    if used_n != 18:
        raise SystemExit("expected 18 used containers")

    # ---- Bad Piggies ----
    binp = BP_DIR / "auto" / "l9_replacements.bin"
    if not binp.is_file():
        raise SystemExit("missing %s" % binp)
    table = binp.read_bytes()
    if table[:4] != b"L9SW":
        raise SystemExit("l9_replacements.bin: bad magic")
    count = struct.unpack_from("<I", table, 8)[0]
    keys = {table[12 + 24 * i:12 + 24 * i + 16] for i in range(count)}
    write(data / "bp" / "l9_replacements.bin", table)
    for fn in BP_ORIG_FILES:
        src = BP_DIR / "orig" / fn
        if not src.is_file():
            raise SystemExit("missing %s" % src)
        blob = src.read_bytes()
        if blob[4:20] not in keys:
            raise SystemExit("%s has no replacement in the table" % fn)
        write(data / "bp" / "orig" / fn, blob)
    print("bp: l9_replacements.bin (%d entries) + %d originals (both found in the table by checksum)" % (count, len(BP_ORIG_FILES)))

    # ---- scripts, readme, exe ----
    write(out / "run_probe.bat", RUN_PROBE_BAT, text=True)
    write(out / "README.txt", README_TXT, text=True)
    dst = out / "m0probe.exe"
    if exe.is_file():
        if exe.resolve() != dst.resolve():
            shutil.copy2(exe, dst)
    else:
        print("WARNING: %s not found - run tools\\m0probe\\build_m0probe_arm32.bat first (or pass --exe)" % exe)

    print("\nassembled %s:" % out)
    total = 0
    for root, _dirs, files in os.walk(out):
        for fn in sorted(files):
            fp = Path(root) / fn
            sz = fp.stat().st_size
            total += sz
            print("  %-60s %9d" % (fp.relative_to(out), sz))
    print("  %-60s %9d" % ("TOTAL", total))


if __name__ == "__main__":
    sys.exit(main())
