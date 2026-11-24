#!/usr/bin/env python3
"""Clean Hill Climb Racing 1.27.0.32 ARM32 package from the stock APPX.

Extracts the APPX, drops AppxSignature.p7x, applies the host bootstrap
(.d3imp import section, the CRT call at file 0x119690, the trampoline at
file 0x11A350) and the coin-pack queue patch, then adds win32craft.dll,
AutoModificator with config.json, the Open-Saves-Folder helper and the
.Win32Craft marker.

The executable is named Win32Craft.HillClimbRacing.exe. The stock
HillClimbRacing.WindowsPhone.exe is removed: .WindowsPhone is omitted and
the name is not shortened to HCR. Win32Craft.HillClimbRacing.exe.orig is
the same exe after the bootstrap and before the coin patch, so
AutoModificator can turn the coin switch off.

arm32/bin ships the ARM32 extras the package needs on a machine that has
no Visual Studio: Auto_Modificator_llvm_ARM32.exe and vcruntime140.dll
(Win11 ARM64 has no ARM32 copy in SysArm32; without it the exe exits
0xC000007B before the host log is written).
"""
from __future__ import annotations

import argparse
import shutil
import zipfile
from pathlib import Path

import hcr_coins
from mcbe_package import remove_p7x_files, write_game_marker
from saves_folder import write_saves_helper

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BIN = ROOT / "arm32" / "bin"
DEFAULT_AM = BIN / "Auto_Modificator_llvm_ARM32.exe"
VCRUNTIME = BIN / "vcruntime140.dll"

STOCK_EXE = "HillClimbRacing.WindowsPhone.exe"
EXE_NAME = hcr_coins.EXE_NAME
MARKER_TEXT = "Hill Climb Racing"
DATA_SUBDIR = "Win32Craft\\HillClimbRacing"
GAME_TITLE = "Hill Climb Racing"

BOOTSTRAP_EDITS = (
    (0x10E, bytes.fromhex("07")),
    (0x129, bytes.fromhex("a4")),
    (0x159, bytes.fromhex("90")),
    (0x167, bytes.fromhex("81")),
    (0x188, bytes.fromhex("00801c")),
    (0x18C, bytes.fromhex("90")),
    (0x208, bytes.fromhex("60")),
    (0x2F0, bytes.fromhex("2e6433696d70")),
    (0x2F8, bytes.fromhex("c401")),
    (0x2FD, bytes.fromhex("801c")),
    (0x301, bytes.fromhex("02")),
    (0x305, bytes.fromhex("381c")),
    (0x314, bytes.fromhex("40")),
    (0x317, bytes.fromhex("c0")),
    (0x119690, bytes.fromhex("5ffe")),
    (0x11A350, bytes.fromhex("dff808c0fc44dcf800c0604764d20a00")),
)

BOOTSTRAP_TAIL = bytes.fromhex(
    "50e5170000000000000000007cef1700e8b0110024e5170000000000000000008aef1700bcb01100a8e4170000000000"
    "0000000076f0170040b0110068e4170000000000000000009af0170000b0110090e417000000000000000000baf01700"
    "28b01100b4e417000000000000000000dcf017004cb0110084e417000000000000000000fef017001cb0110084e51700"
    "00000000000000001c4d18001cb11100a8ee17000000000000000000c457180040ba110094ec17000000000000000000"
    "046018002cb8110034ed17000000000000000000c6661800ccb81100d8e417000000000000000000fe68180070b01100"
    "98e4170000000000000000001e69180030b01100cce4170000000000000000004869180064b0110000e5170000000000"
    "000000009c6a180098b01100e4e417000000000000000000c46a18007cb01100f0e417000000000000000000ec6a1800"
    "88b01100f8e417000000000000000000146b180090b01100b4811c00000000000000000090811c00bc811c0000000000"
    "0000000000000000000000000000000077696e333263726166742e646c6c0000000057696e3332426f6f747374726170"
    "00000000a0811c0000000000a0811c000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
)


def apply_bootstrap(exe: bytearray) -> None:
    for offset, data in BOOTSTRAP_EDITS:
        exe[offset:offset + len(data)] = data
    exe += BOOTSTRAP_TAIL


def build(appx: Path, out: Path, dll: Path, am: Path | None, runtime: Path | None, force: bool) -> None:
    if out.exists():
        if not force:
            raise SystemExit("output already exists: %s (pass --force to replace it)" % out)
        shutil.rmtree(out)
    out.mkdir(parents=True)
    with zipfile.ZipFile(appx) as package:
        package.extractall(out)
    stock = out / STOCK_EXE
    if not stock.is_file():
        raise SystemExit("APPX has no %s" % STOCK_EXE)
    exe = bytearray(stock.read_bytes())
    stock.unlink()
    apply_bootstrap(exe)
    original = bytearray(exe)
    state = hcr_coins.apply_coins(exe)
    (out / EXE_NAME).write_bytes(exe)
    (out / (EXE_NAME + ".orig")).write_bytes(original)
    print("exe %s (%s); bootstrap copy is %s.orig" % (EXE_NAME, state, EXE_NAME))
    shutil.copy2(dll, out / "win32craft.dll")
    if am is not None:
        shutil.copy2(am, out / "Auto_Modificator_llvm_ARM32.exe")
    hcr_coins.write_config(out / "config.json")
    if not VCRUNTIME.is_file():
        raise SystemExit("missing ARM32 vcruntime140.dll: %s" % VCRUNTIME)
    shutil.copy2(VCRUNTIME, out / VCRUNTIME.name)
    print("runtime %s" % VCRUNTIME.name)
    if runtime is not None:
        for path in sorted(runtime.iterdir()):
            if path.is_file() and path.suffix.lower() == ".dll" and not (out / path.name).exists():
                shutil.copy2(path, out / path.name)
                print("runtime %s" % path.name)
    for path in remove_p7x_files(out):
        print("removed %s" % path.relative_to(out))
    write_game_marker(out, MARKER_TEXT)
    marker = (out / ".Win32Craft").read_bytes()
    if marker != MARKER_TEXT.encode("ascii"):
        raise SystemExit("marker bytes are %r" % marker)
    created = write_saves_helper(out, DATA_SUBDIR, GAME_TITLE)
    print("marker .Win32Craft; helper %s" % ", ".join(created))
    bat = out / "run-fl91.bat"
    bat.write_bytes(
        b"@echo off\r\n"
        b"cd /d \"%~dp0\"\r\n"
        b"if exist win32craft.log move /y win32craft.log win32craft.log.prev >nul\r\n"
        b"start \"\" Win32Craft.HillClimbRacing.exe\r\n"
    )
    print("launch run-fl91.bat")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--appx", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--am", type=Path, default=DEFAULT_AM if DEFAULT_AM.is_file() else None)
    parser.add_argument("--runtime-dir", type=Path)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    if not args.appx.is_file():
        raise SystemExit("APPX not found: %s" % args.appx)
    if not args.dll.is_file():
        raise SystemExit("host DLL not found: %s" % args.dll)
    if args.am is not None and not args.am.is_file():
        raise SystemExit("AutoModificator not found: %s" % args.am)
    build(args.appx, args.out, args.dll, args.am, args.runtime_dir, args.force)


if __name__ == "__main__":
    main()
