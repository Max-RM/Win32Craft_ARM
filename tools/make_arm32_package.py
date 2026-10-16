#!/usr/bin/env python3
"""Assemble dist-arm32\\package\\ : an OVERLAY for the Minecraft 1.2.8 ARM32 game folder.

Contents of the package:
  Win32Craft.1.2.8-arm32.exe   patched game exe       (tools/1.2.8-arm32-patcher.py)
  fmod_ARM.dll                 patched real FMOD      (same patcher, --fmod-dll/--fmod-out)
  win32craft.dll               the ARM32 host         (build_arm32.bat)
  vccorlib140/msvcp140/concrt140/vcruntime140.dll     desktop VC++ onecore ARM redistributables
  (+ any further redistributable DLL imported transitively, e.g. nothing today)
  fmod-silent\\fmod_ARM.dll     silent FMOD fallback   (build_fmod_null_arm32.bat)
  MANIFEST.txt                 SHA-256 of every file + the input hashes
  INSTALL.txt                  short installation instructions

The original game folder is only READ (its Minecraft.Windows.exe and fmod_ARM.dll are the patcher inputs); nothing is
written outside the package folder and the patcher's usual dist-arm32 outputs.

Usage (from the repository root):
  python tools\\make_arm32_package.py                    run the patcher, assemble the package
  python tools\\make_arm32_package.py --use-existing     reuse dist-arm32\\Win32Craft.1.2.8-arm32.exe / fmod_ARM.dll
  python tools\\make_arm32_package.py --build            run build_arm32.bat (and build_fmod_null_arm32.bat if the stub is missing) first
  options: --game-dir --dist --out --redist --fmod-silent --llvm-bin --redirect-api-sets --force

Environment overrides (used when the option is absent): WIN32CRAFT_GAME_DIR, WIN32CRAFT_REDIST, LLVM_BIN, DIST.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_REDIST = (r"D:\Program Files\vs22buildtools\VC\Redist\MSVC\14.44.35112"
                  r"\onecore\arm\Microsoft.VC143.CRT")
DEFAULT_LLVM_BIN = r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
REQUIRED_VC_DLLS = ("vccorlib140.dll", "msvcp140.dll", "concrt140.dll", "vcruntime140.dll")
# DLL names that belong to the VC++ redistributable (checked against the transitive imports)
VC_NAME = re.compile(r"^(vccorlib|msvcp|concrt|vcruntime|vcomp|mfc|atl)[0-9a-z_]*\.dll$", re.I)
PATCHED_EXE_NAME = "Win32Craft.1.2.8-arm32.exe"
# inputs of win32craft.dll (build_arm32.bat): the DLL must not be older than any of them
SOURCES_OF_DLL = ("win32_host.c", "api_compat.c", "win32_compat.h", "win32_host_arm32.h", "build_arm32.bat")
IMAGE_FILE_MACHINE_ARMNT = 0x01C4


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def pe_machine(path: Path) -> int:
    with path.open("rb") as stream:
        head = stream.read(0x40)
        if head[:2] != b"MZ":
            return -1
        stream.seek(struct.unpack_from("<I", head, 0x3C)[0])
        signature = stream.read(6)
    if signature[:4] != b"PE\0\0":
        return -1
    return struct.unpack_from("<H", signature, 4)[0]


def need_file(path: Path, what: str) -> Path:
    if not path.is_file():
        die(f"{what} not found: {path}")
    return path


def need_arm(path: Path, what: str) -> None:
    machine = pe_machine(path)
    if machine != IMAGE_FILE_MACHINE_ARMNT:
        die(f"{what} is not an ARMNT PE image (machine 0x{machine:x}): {path}")


def run(command: list[str], **kwargs) -> None:
    print("+", " ".join(f'"{c}"' if " " in c else c for c in command))
    result = subprocess.run(command, **kwargs)
    if result.returncode:
        die(f"command failed with exit code {result.returncode}")


def imports_of(readobj: Path | None, image: Path) -> set[str]:
    """Lower-case DLL names imported (also delay-loaded) by an image; empty set if llvm-readobj is unavailable."""
    if not readobj:
        return set()
    result = subprocess.run([str(readobj), "--coff-imports", str(image)],
                            capture_output=True, text=True, errors="replace")
    if result.returncode:
        die(f"llvm-readobj failed on {image}: {result.stderr.strip()}")
    return {m.group(1).lower() for m in re.finditer(r"^\s*Name:\s*(\S+\.dll)\s*$", result.stdout, re.I | re.M)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game-dir", type=Path,
                        default=Path(os.environ.get("WIN32CRAFT_GAME_DIR", ROOT / "Minecraft_1.2.8.0_ARM32")),
                        help="folder holding the ORIGINAL Minecraft.Windows.exe and fmod_ARM.dll (read only)")
    parser.add_argument("--dist", type=Path, default=Path(os.environ.get("DIST", ROOT / "dist-arm32")),
                        help="dist-arm32 folder (win32craft.dll, patcher outputs)")
    parser.add_argument("--out", type=Path, default=None, help="package folder (default <dist>\\package)")
    parser.add_argument("--redist", type=Path, default=Path(os.environ.get("WIN32CRAFT_REDIST", DEFAULT_REDIST)),
                        help="VC++ onecore\\arm redistributable folder")
    parser.add_argument("--fmod-silent", type=Path, default=ROOT / "dist-fmod-null-arm32" / "fmod_ARM.dll",
                        help="silent fmod_ARM.dll built by build_fmod_null_arm32.bat")
    parser.add_argument("--llvm-bin", type=Path, default=Path(os.environ.get("LLVM_BIN", DEFAULT_LLVM_BIN)),
                        help="folder with llvm-readobj.exe (transitive import check)")
    parser.add_argument("--redirect-api-sets", action="store_true", help="pass --redirect-api-sets to the patcher")
    parser.add_argument("--use-existing", action="store_true",
                        help="do not run the patcher: use the exe/fmod already in --dist")
    parser.add_argument("--build", action="store_true", help="run build_arm32.bat first (needs cmd.exe)")
    parser.add_argument("--force", action="store_true", help="overwrite an existing non-empty package folder")
    args = parser.parse_args()

    dist = args.dist.resolve()
    out = (args.out or dist / "package").resolve()
    game = args.game_dir.resolve()
    redist = args.redist
    readobj = args.llvm_bin / "llvm-readobj.exe"
    readobj = readobj if readobj.is_file() else None
    if not readobj:
        print("WARNING: llvm-readobj.exe not found: the transitive import check is skipped "
              "(only the four required redistributable DLLs are copied)")
    if out == game or game in out.parents:
        die("the package folder must not be inside the game folder")

    if args.build:
        run(["cmd", "/c", str(ROOT / "build_arm32.bat")], cwd=ROOT)
        if not args.fmod_silent.is_file():
            run(["cmd", "/c", str(ROOT / "build_fmod_null_arm32.bat")], cwd=ROOT)

    original_exe = need_file(game / "Minecraft.Windows.exe", "original Minecraft.Windows.exe")
    original_fmod = need_file(game / "fmod_ARM.dll", "original fmod_ARM.dll")
    dll = need_file(dist / "win32craft.dll", "win32craft.dll (run build_arm32.bat)")
    silent = need_file(args.fmod_silent, "silent fmod_ARM.dll (run build_fmod_null_arm32.bat)")
    if not redist.is_dir():
        die(f"redistributable folder not found: {redist} (set WIN32CRAFT_REDIST or --redist)")

    # a package must never carry a host DLL older than the sources it is built from (a stale package would silently run
    # without the latest fixes on the device): refuse unless the DLL is at least as new as every ARM32 source/header
    stale = [name for name in SOURCES_OF_DLL
             if (ROOT / name).is_file() and (ROOT / name).stat().st_mtime > dll.stat().st_mtime + 1.0]
    if stale:
        die(f"{dll} is OLDER than {', '.join(stale)}: rebuild first (build_arm32.bat, or use --build)")

    dist.mkdir(parents=True, exist_ok=True)
    patched_exe = dist / PATCHED_EXE_NAME
    patched_fmod = dist / "fmod_ARM.dll"
    if args.use_existing:
        need_file(patched_exe, "patched exe")
        need_file(patched_fmod, "patched fmod_ARM.dll")
    else:
        command = [sys.executable, str(ROOT / "tools" / "1.2.8-arm32-patcher.py"),
                   str(original_exe), str(patched_exe),
                   "--fmod-dll", str(original_fmod), "--fmod-out", str(patched_fmod)]
        if args.redirect_api_sets:
            command.append("--redirect-api-sets")
        run(command, cwd=ROOT)
    for path, what in ((patched_exe, "patched exe"), (patched_fmod, "patched fmod_ARM.dll"),
                       (dll, "win32craft.dll"), (silent, "silent fmod_ARM.dll")):
        need_arm(path, what)
    if sha256(patched_exe) == sha256(original_exe):
        die("the patched exe is identical to the original")
    if sha256(patched_fmod) == sha256(original_fmod):
        die("the patched fmod_ARM.dll is identical to the original")

    # redistributable DLLs: the four required ones plus anything VC-like imported transitively
    wanted: dict[str, Path] = {}
    for name in REQUIRED_VC_DLLS:
        wanted[name] = need_file(redist / name, f"redistributable {name}")
    queue = [patched_exe, dll, patched_fmod] + list(wanted.values())
    seen: set[str] = set()
    while queue:
        image = queue.pop()
        for imported in imports_of(readobj, image):
            if imported in seen or not VC_NAME.match(imported):
                continue
            seen.add(imported)
            candidate = redist / imported
            if imported not in wanted:
                if not candidate.is_file():
                    die(f"{image.name} imports {imported}, which is not in {redist}")
                wanted[imported] = candidate
                queue.append(candidate)
                print(f"extra redistributable DLL needed: {imported}")
    for name, path in wanted.items():
        need_arm(path, f"redistributable {name}")

    if out.exists() and any(out.iterdir()):
        if not args.force:
            die(f"{out} already exists and is not empty (use --force to overwrite its files)")
    (out / "fmod-silent").mkdir(parents=True, exist_ok=True)
    files: list[tuple[Path, str]] = [
        (patched_exe, PATCHED_EXE_NAME),
        (patched_fmod, "fmod_ARM.dll"),
        (dll, "win32craft.dll"),
    ]
    files += [(path, name) for name, path in sorted(wanted.items())]
    files.append((silent, "fmod-silent/fmod_ARM.dll"))
    for source, relative in files:
        shutil.copyfile(source, out / relative)
        if sha256(source) != sha256(out / relative):
            die(f"{relative}: the copy in the package differs from its source {source}")
    if sha256(out / "win32craft.dll") != sha256(dist / "win32craft.dll"):
        die("package/win32craft.dll differs from dist-arm32/win32craft.dll")

    lines = [
        "Win32Craft 1.2.8 ARM32 package manifest",
        "",
        "Inputs (read only):",
        f"  Minecraft.Windows.exe  {sha256(original_exe)}  {original_exe.stat().st_size} bytes",
        f"  fmod_ARM.dll           {sha256(original_fmod)}  {original_fmod.stat().st_size} bytes",
        f"  redistributable dir    {redist}",
        "",
        "Package files (SHA-256, size, source):",
    ]
    for source, relative in files:
        target = out / relative
        lines.append(f"  {sha256(target)}  {target.stat().st_size:>9}  {relative}   <- {source}")
    lines += ["", "Nothing in this package has been run on a device yet.", ""]
    (out / "MANIFEST.txt").write_text("\r\n".join(lines), encoding="ascii", errors="replace")

    install = f"""Win32Craft 1.2.8 ARM32 - installation (overlay for the game folder)

Requirements: Windows RT 8.1 (also copy the win81-runtime files, see make_arm32_device_package.py) or Windows 10 ARM32 desktop;
Direct3D 11.2 runtime. The game runs at Direct3D feature level 9_1 by default.

1. Copy your Minecraft 1.2.8 ARM32 game folder (the one with Minecraft.Windows.exe, data\\, resources.pri) to a
   writable location outside WindowsApps, e.g. C:\\Games\\Minecraft128. Keep the original folder untouched.
2. In the COPY, rename fmod_ARM.dll to fmod_ARM.dll.orig (backup of the original).
3. Copy everything in this folder EXCEPT MANIFEST.txt, INSTALL.txt and the fmod-silent folder into the game copy:
     {PATCHED_EXE_NAME}, fmod_ARM.dll (patched), win32craft.dll,
     {", ".join(sorted(wanted))}
   Minecraft.Windows.exe stays as it is (the original is not modified).
4. Run {PATCHED_EXE_NAME}. The log is written to win32craft.log next to the exe (or in %TEMP% if that folder is
   not writable). If anything fails, send that file.

No sound / hang during startup in FMOD: replace the copied fmod_ARM.dll with fmod-silent\\fmod_ARM.dll
(a silent stub, the game then runs without audio).
Other feature level: {PATCHED_EXE_NAME} --feature-level 10_0   (or set WIN32CRAFT_FEATURE_LEVEL).
See ARM32.md in the repository for details. This build has not been verified on a real device.
"""
    (out / "INSTALL.txt").write_text(install.replace("\n", "\r\n"), encoding="ascii")

    print()
    print(f"Package assembled in {out}")
    for path in sorted(out.rglob("*")):
        if path.is_file():
            print(f"  {sha256(path)}  {path.stat().st_size:>9}  {path.relative_to(out)}")


if __name__ == "__main__":
    main()
