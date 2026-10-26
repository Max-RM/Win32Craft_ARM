#!/usr/bin/env python3
"""Assemble the ready-to-run device package of a Windows Phone 8.1 game on the x104 host for Win32Craft ARM32:
Minecraft PE 0.10.4.40 (default, --game 0104) or REDCON 1 1.3.0.0 (--game redcon; output dist-redcon\\REDCON_1.3.0.0_ARM32_Win32Craft).

    python tools\\make_0104_package.py                 patch + assemble from the existing dist-arm32\\win32craft.dll
    python tools\\make_0104_package.py --game redcon   the same for REDCON 1 (game folder Redcon_1.3.0.0_ARM next to the repo files, WIN32CRAFT_GAME_DIR_REDCON)
    python tools\\make_0104_package.py --build         run build_arm32.bat and build_loadcheck_arm32.bat first
    python tools\\make_0104_package.py --allow-stubs   foundation dry run: units that are still stubs are accepted, the package is named ...-FOUNDATION-ONLY

Output (under dist-0104\\):
    Minecraft_PE_0.10.4.40_ARM32_Win32Craft\\        the complete folder: patched exe + host dll + patched VC120 *_app DLLs + vcruntime140 + Windows RT 8.1
                                                   app-local runtime (UCRT + api-set forwarders) + loadcheck.exe + start*.bat + README_DEVICE.txt + MANIFEST.txt
    Minecraft_PE_0.10.4.40_ARM32_Win32Craft.zip    the same folder zipped
The original game folder is only READ (tools\\arm32-patcher.py copies it).  Steps:
  1. refuse while a unit file still contains '*** STUB' (unless --allow-stubs);  2. optionally build;  refuse a win32craft.dll older than its sources;
  3. tools\\arm32-patcher.py patch --profile v0_10_4 --host-dll  (patched exe Win32Craft.0.10.4-arm32.exe, three patched VC120 DLLs, patch_report.json);
  4. VC++ runtime DLLs the host needs (vcruntime140.dll ...);  5. api-set scan + app-local forwarder DLLs for Windows RT 8.1 (the 8 unproven contracts of
     pe_patch_plan section 6 and everything else the shipped PEs import) + the SDK Universal CRT;  6. loadcheck.exe;  7. start batch files, README, MANIFEST;
  8. dependency check of every PE in the folder;  9. zip.
Environment overrides: WIN32CRAFT_GAME_DIR_0104, WIN32CRAFT_REDIST, WIN32CRAFT_VCLIBS120_APPX (the patcher), LLVM_BIN, DIST."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE / "arm32"))
import arm32re  # noqa: E402

FOLDER = "Minecraft_PE_0.10.4.40_ARM32_Win32Craft"
EXE_NAME = "Win32Craft.0.10.4-arm32.exe"
DEFAULT_GAME = r"D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]"
DEFAULT_REDIST = r"D:\Program Files\vs22buildtools\VC\Redist\MSVC\14.44.35112\onecore\arm\Microsoft.VC143.CRT"
DEFAULT_LLVM_BIN = r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
REQUIRED_VC_DLLS = ("vcruntime140.dll",)           # win32craft.dll imports it (vcruntime.lib); the VC120 *_app DLLs come from the patcher
VC_NAME = re.compile(r"^(vccorlib|msvcp|concrt|vcruntime|vcomp|mfc|atl)[0-9a-z_]*\.dll$", re.I)
UNIT_FILES = ["win32_host_arm32_0104_%s.inl" % u for u in ("async", "app", "panel", "platform", "input")]
# inputs of win32craft.dll: the DLL must not be older than any of them
SOURCES_OF_DLL = ["win32_host.c", "api_compat.c", "win32_compat.h", "win32_host_arm32.h", "build_arm32.bat", "win32craft_arm32.def", "win32_host_arm32_0104.h",
                  "win32_host_arm32_0104.inl", "win32_host_arm32_0104_core.inl", "win32_host_arm32_0104_iids.inl", "win32_host_arm32_0104_boot.inl",
                  "win32_host_arm32_0104_selftest.inl"] + UNIT_FILES
# present on Windows 8.1 RT / Windows 10 ARM32 (not shipped)
SYSTEM_DLLS = {"kernel32.dll", "user32.dll", "shell32.dll", "comdlg32.dll", "d3d11.dll", "dxgi.dll", "d3dcompiler_47.dll", "ws2_32.dll", "xaudio2_8.dll",
               "oleaut32.dll", "ole32.dll", "combase.dll", "ntdll.dll", "advapi32.dll", "gdi32.dll", "iphlpapi.dll", "winmm.dll", "bcrypt.dll", "rpcrt4.dll",
               "propsys.dll", "wintypes.dll", "msvcrt.dll", "imm32.dll", "psapi.dll",
               "d2d1.dll", "dwrite.dll", "mfplat.dll"}          # REDCON 1: text through Direct2D / DirectWrite, music through Media Foundation

BATCHES = [
    ("start.bat", "", "normal start (Direct3D feature level 9_1, HWND swap chain, window fitted to the screen)"),
    ("start_selftest.bat", "--selftest-fakes=only", "RUN THIS FIRST on a new device: checks the ABI of every host fake and exits with a message box (no game)"),
    ("start_selftest_then_game.bat", "--selftest-fakes", "self test, then the game"),
    ("start_window_1024.bat", "--window=1024x576", "window of 1024x576 client pixels"),
    ("start_window_1280.bat", "--window=1280x720", "window of 1280x720 client pixels (may not fit a 1366x768 screen with a taskbar)"),
    ("start_fullscreen.bat", "--fullscreen", "borderless window on the whole screen"),
    ("start_dcomp.bat", "--present=dcomp", "present through DirectComposition instead of the HWND swap chain (path A)"),
    ("start_halfres.bat", "--half-res", "keep the game's own half-resolution rendering (faster, blurrier)"),
    ("start_vsync.bat", "--vsync", "Present with SyncInterval 1 (vertical sync)"),
    ("start_maxfps30.bat", "--max-fps=30", "cap the game at 30 frames per second"),
    ("start_fl93.bat", "--feature-level=9_3", "Direct3D feature level 9_3 device"),
    ("start_fl100.bat", "--feature-level=10_0", "Direct3D feature level 10_0 device"),
    ("start_osk_on.bat", "--osk=on", "always use the Windows touch keyboard for text boxes"),
    ("start_osk_off.bat", "--osk=off", "never open the Windows touch keyboard"),
    ("start_touch.bat", "--input=touch", "pin touch input"),
    ("start_touch_wmtouch.bat", "--touch-api=wmtouch", "touch through WM_TOUCH (Windows RT 8.1 sends no WM_POINTER)"),
    ("start_touch_legacy.bat", "--touch-api=legacy", "touch through emulated mouse messages (single finger)"),
    ("start_no_back_button.bat", "--no-back-button", "no on-screen Back/Pause button (Back = Esc / Alt+Left)"),
    ("start_back_button_always.bat", "--back-button-always", "show the on-screen Back/Pause button in the menus too (a tap there = Back)"),
    ("start_inputlog.bat", "--input-log", "log every input event (large log)"),
    ("start_trace.bat", "--trace-fakes", "log every call of the chatty host fakes (very large log)"),
    ("start_gamestate.bat", "--trace-game-state", "log the game's own state (screen, player, options, touch layout) and the Direct3D draw calls once per second (HUD investigation; see arm32/v0_10_4/HUD_INVESTIGATION.md)"),
]

README = """Win32Craft - Minecraft Pocket Edition 0.10.4.40 (Windows Phone 8.1 build) for ARM32 Windows        [{status}]
===========================================================================================================

This folder is COMPLETE and READY TO RUN. Nothing needs to be patched or installed on the device (no Python).
Target devices: Surface RT / Surface 2 class (Direct3D feature level 9_1), Windows RT 8.1 or Windows 10 ARM32 (desktop).
{foundation_note}
1. Copy this whole folder to the device, to a normal WRITABLE location whose path contains ONLY plain ASCII letters/digits/spaces/punctuation
   (for example C:\\Games\\MCPE0104).  !!! A non-ASCII character anywhere in the folder path (accented letters, Arabic, Chinese ...) corrupts the game's
   file paths: the host refuses to start and shows a message box.  Do NOT run it from the WindowsApps folder.  The saved worlds go to
   %APPDATA%\\MinecraftPE\\0.10.4 (or --data-dir=<ASCII folder>); that path must be ASCII too (a user name with non-ASCII letters is handled by falling
   back to <this folder>\\LocalState).
   Windows RT 8.1: the Universal C Runtime and the missing API-set DLLs are included (ucrtbase.dll + api-ms-win-*.dll, ignored on Windows 10).
   Unsigned code must be allowed to run on the device (as for any desktop app you built yourself).
2. FIRST: double-click  start_selftest.bat  .  It checks every host fake (ABI, values) before the game and shows "all checks passed" or "FAILURES".
   Then double-click  start.bat  (or  {exe}  directly).  The window opens when the game has built its UI (a few seconds), the game compiles its shaders
   (several seconds, black or empty window meanwhile) and then shows the main menu.
3. LOG: win32craft.log is written next to the exe (or in %TEMP% if the folder is read-only).  If anything goes wrong - nothing starts, black window,
   crash, freeze, no input, no sound - close the game and send me win32craft.log (and win32craft.log.old if it exists) plus a short note of what you saw.
   STARTUP ERRORS ("missing DLL", "procedure entry point ... could not be located"): run  loadcheck.exe  from this folder.  It checks every DLL import of the
   game exe, the host and the VC++ runtimes against your Windows in one go and writes loadcheck.txt - send that file too.
4. Options (start*.bat show the useful ones):
{batches}
   Several options can be combined on one command line, e.g.  {exe} --window=1024x576 --vsync .
5. Touch: tap = click, the Windows touch keyboard opens for text boxes (chat, world name).  If touch does not work, try start_touch_wmtouch.bat and
   start_touch_legacy.bat and send the log.
   Back / Pause: this Windows Phone build has no pause button of its own (phones have a Back key).  The host shows a small Back/Pause button in the
   top-left corner of the window while you play in a world (hidden in menus); a tap opens the pause menu.  On a keyboard: Esc (or Alt+Left).
6. Known limitations of this first build: see the notes in the repository (arm32/v0_10_4/HOST_DESIGN.md section 12).  "Play on Realms" is dead code in
   this game version and may crash; nothing else is known to be missing but NOTHING was run on a real device yet.

Files:
  {exe}       patched game executable (the stock MCPE.Host.WindowsPhone.exe is not shipped)
  win32craft.dll                    the ARM32 host (window, Direct3D device, fake WinRT/XAML services, input)
  msvcr120_app.dll msvcp120_app.dll vccorlib120_app.dll    the Visual C++ 2013 runtime of the game (genuine Microsoft files, one flag in the header changed)
  vcruntime140.dll                  runtime of the host
  ucrtbase.dll + api-ms-win-*.dll   app-local Universal CRT and API-set forwarders for WINDOWS RT 8.1
  data\\ , *.xbf , resources.pri ...  the game's own files
  loadcheck.exe                     diagnostic: finds missing DLLs / entry points
  start*.bat                        helpers
  APISETS.txt                       which API-set contracts the shipped binaries import
  patch_report.json                 every byte the patcher changed
  MANIFEST.txt                      SHA-256 of every file
"""


REDCON_BATCHES = [
    ("start.bat", "", "normal start (the game's own Direct3D feature-level list 11_1..9_1, HWND swap chain, window fitted to the screen)"),
    ("start_selftest.bat", "--selftest-fakes=only", "RUN THIS FIRST on a new device: checks the ABI of every host fake and exits with a message box (no game)"),
    ("start_selftest_then_game.bat", "--selftest-fakes", "self test, then the game"),
    ("start_window_1024.bat", "--window=1024x576", "window of 1024x576 client pixels"),
    ("start_window_1280.bat", "--window=1280x720", "window of 1280x720 client pixels (may not fit a 1366x768 screen with a taskbar)"),
    ("start_fullscreen.bat", "--fullscreen", "borderless window on the whole screen"),
    ("start_fl91.bat", "--feature-level=9_1", "force a Direct3D feature level 9_1 device (the game normally picks the highest itself)"),
    ("start_touch.bat", "--input=touch", "pin touch input"),
    ("start_touch_wmtouch.bat", "--touch-api=wmtouch", "touch through WM_TOUCH (Windows RT 8.1 sends no WM_POINTER)"),
    ("start_touch_legacy.bat", "--touch-api=legacy", "touch through emulated mouse messages (single finger)"),
    ("start_back_button.bat", "--back-button-always", "show a small on-screen Back button in the top-left corner (a tap = the phone's Back key)"),
    ("start_inputlog.bat", "--input-log", "log every input event (large log)"),
    ("start_trace.bat", "--trace-fakes", "log every call of the chatty host fakes (very large log)"),
]

REDCON_README = """Win32Craft - REDCON 1 1.3.0.0 (Hexage, Windows Phone 8.1 build) for ARM32 Windows        [{status}]
===========================================================================================================

This folder is COMPLETE and READY TO RUN. Nothing needs to be patched or installed on the device (no Python).
Target devices: Surface RT / Surface 2 class, Windows RT 8.1 or Windows 10 ARM32 (desktop).  The game ships Direct3D "level 9" shaders and asks for the
best feature level itself, so feature level 9_1 GPUs are supported without any trick.
{foundation_note}
1. Copy this whole folder to the device, to a SHORT, writable location whose path contains ONLY plain ASCII letters/digits/spaces/punctuation
   (for example C:\\Games\\Redcon).  The game copies the folder path into a 256-byte buffer: keep it well below 150 characters and ASCII-only.
   Do NOT run it from the WindowsApps folder.  Saves and settings go to %APPDATA%\\Win32Craft\\Redcon (or --data-dir=<short ASCII folder>).
   Windows RT 8.1: the Universal C Runtime and the missing API-set DLLs are included (ucrtbase.dll + api-ms-win-*.dll, ignored on Windows 10).
2. FIRST: double-click  start_selftest.bat  .  It checks every host fake (ABI, values) before the game and shows "all checks passed" or "FAILURES".
   Then double-click  start.bat  (or  {exe}  directly).
3. LOG: win32craft.log is written next to the exe (or in %TEMP% if the folder is read-only).  If anything goes wrong - nothing starts, black window,
   crash, freeze, no input, no sound - close the game and send me win32craft.log (and win32craft.log.old if it exists) plus a short note of what you saw.
   STARTUP ERRORS ("missing DLL", "procedure entry point ... could not be located"): run  loadcheck.exe  and send loadcheck.txt too.
4. Options (start*.bat show the useful ones):
{batches}
5. Back: the game has its own back buttons on screen.  On a keyboard the phone's Back key is Esc (or Alt+Left); start_back_button.bat adds a small
   host Back button in the top-left corner of the window (a tap = Back) if you ever need one.
6. Purchases: REDCON 1 sells its premium editions as in-app products of the Windows Phone Store, which is closed.  This host reports every product as
   NOT owned (free edition); buying is not possible.
7. Known limitations: verified on a Surface 2 only.  Music (Media Foundation), the online leaderboard and the game's own
   dialogs (XAML pop-ups) are the parts most likely to need a fix - the log will show it.

Files:
  {exe}       patched game executable (the stock Redcon.exe is not shipped)
  win32craft.dll                    the ARM32 host (window, Direct3D device, fake WinRT/XAML services, input)
  msvcr120_app.dll msvcp120_app.dll vccorlib120_app.dll    the Visual C++ 2013 runtime of the game (genuine Microsoft files, one flag in the header changed)
  vcruntime140.dll                  runtime of the host
  ucrtbase.dll + api-ms-win-*.dll   app-local Universal CRT and API-set forwarders for WINDOWS RT 8.1
  res\\ , Assets\\ , resources.pri  the game's own files
  loadcheck.exe                     diagnostic: finds missing DLLs / entry points
  start*.bat                        helpers
  APISETS.txt                       which API-set contracts the shipped binaries import
  patch_report.json                 every byte the patcher changed
  MANIFEST.txt                      SHA-256 of every file
"""

# per game: what the generic steps below need
GAMES = {
    "0104": {"title": "Minecraft PE 0.10.4.40", "folder": FOLDER, "exe_name": EXE_NAME, "stock_exe": "MCPE.Host.WindowsPhone.exe", "profile": "v0_10_4",
             "game_env": "WIN32CRAFT_GAME_DIR_0104", "default_game": DEFAULT_GAME, "default_out": "dist-0104", "batches": BATCHES, "readme": README,
             "status": "verified on a Surface 2 (Windows 10 ARM32): menu, touch, keyboard, world, HUD work; NOT yet verified: Windows RT 8.1, audio, saving",
             "work": "apiset-shims-0104"},
    "redcon": {"title": "REDCON 1 1.3.0.0", "folder": "REDCON_1.3.0.0_ARM32_Win32Craft", "exe_name": "Win32Craft.Redcon-arm32.exe", "stock_exe": "Redcon.exe",
               "profile": "redcon", "game_env": "WIN32CRAFT_GAME_DIR_REDCON", "default_game": str(ROOT / "Redcon_1.3.0.0_ARM"), "default_out": "dist-redcon",
               "batches": REDCON_BATCHES, "readme": REDCON_README,
               "status": "runs on a Surface 2 (Windows 10 ARM32, first device test 2026-10-22); NOT yet verified: Windows RT 8.1",
               "work": "apiset-shims-redcon"},
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def die(message: str) -> None:
    print("ERROR:", message, file=sys.stderr)
    sys.exit(1)


def run(command: list, **kwargs) -> subprocess.CompletedProcess:
    print("+", " ".join('"%s"' % c if " " in str(c) else str(c) for c in command))
    result = subprocess.run([str(c) for c in command], **kwargs)
    return result


def need_ok(result: subprocess.CompletedProcess, what: str) -> None:
    if result.returncode:
        die("%s failed with exit code %d" % (what, result.returncode))


def pe_imports(path: Path) -> set[str] | None:
    try:
        pe = arm32re.Pe(str(path))
    except Exception:
        return None
    return {imp["dll"].lower() for imp in pe.imports()}


def pe_machine(path: Path) -> int:
    with path.open("rb") as stream:
        head = stream.read(0x40)
        if head[:2] != b"MZ":
            return -1
        stream.seek(int.from_bytes(head[0x3C:0x40], "little"))
        sig = stream.read(6)
    return int.from_bytes(sig[4:6], "little") if sig[:4] == b"PE\0\0" else -1


def main() -> None:
    global FOLDER, EXE_NAME, BATCHES, README
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", choices=sorted(GAMES), default="0104", help="which game (default 0104 = Minecraft PE 0.10.4.40)")
    parser.add_argument("--game-dir", type=Path, help="folder with the ORIGINAL game exe (read only); default per game (WIN32CRAFT_GAME_DIR_0104 / _REDCON)")
    parser.add_argument("--dist", type=Path, default=Path(os.environ.get("DIST", ROOT / "dist-arm32")), help="dist-arm32 (win32craft.dll, loadcheck.exe)")
    parser.add_argument("--out", type=Path, help="output root (default dist-0104 / dist-redcon)")
    parser.add_argument("--redist", type=Path, default=Path(os.environ.get("WIN32CRAFT_REDIST", DEFAULT_REDIST)), help="VC++ onecore\\arm redistributable folder")
    parser.add_argument("--llvm-bin", type=Path, default=Path(os.environ.get("LLVM_BIN", DEFAULT_LLVM_BIN)))
    parser.add_argument("--companion-appx", type=Path, help="Microsoft.VCLibs.120.00.Phone appx (passed to the patcher)")
    parser.add_argument("--build", action="store_true", help="run build_arm32.bat and build_loadcheck_arm32.bat first (needs cmd.exe)")
    parser.add_argument("--allow-stubs", action="store_true", help="accept unit files that are still stubs (foundation dry run; package is marked FOUNDATION-ONLY)")
    parser.add_argument("--no-apiset", action="store_true", help="skip the Windows RT 8.1 runtime (UCRT + api-set forwarders)")
    parser.add_argument("--no-zip", action="store_true")
    args = parser.parse_args()
    G = GAMES[args.game]
    FOLDER, EXE_NAME, BATCHES, README = G["folder"], G["exe_name"], G["batches"], G["readme"]
    if args.game_dir is None:
        args.game_dir = Path(os.environ.get(G["game_env"], G["default_game"]))
    if args.out is None:
        args.out = ROOT / G["default_out"]

    game = args.game_dir.resolve()
    dist = args.dist.resolve()
    out_root = args.out.resolve()
    target = out_root / FOLDER
    if game in target.parents or target == game:
        die("the package folder must not be inside the game folder")
    if not (game / G["stock_exe"]).is_file():
        die("%s not found in %s (set --game-dir or %s)" % (G["stock_exe"], game, G["game_env"]))
    if any(ord(ch) > 127 for ch in str(target)):
        print("WARNING: the OUTPUT path %s is not ASCII-only; copy the finished folder to an ASCII-only path on the device (the host refuses non-ASCII paths)" % target)

    # 1. stubs gate
    stubs = [name for name in UNIT_FILES if "*** STUB" in (ROOT / name).read_text(encoding="utf-8", errors="replace")]
    if stubs and not args.allow_stubs:
        die("these unit files are still STUBS: %s - the package would not run the game (use --allow-stubs for a foundation dry run)" % ", ".join(stubs))
    status = "FOUNDATION-ONLY: units still stubs: " + ", ".join(s.replace("win32_host_arm32_0104_", "").replace(".inl", "") for s in stubs) if stubs else G["status"]
    folder_name = FOLDER + ("-FOUNDATION-ONLY" if stubs else "")
    target = out_root / folder_name

    # 2. build
    if args.build:
        need_ok(run(["cmd", "/c", str(ROOT / "build_arm32.bat")], cwd=ROOT), "build_arm32.bat")
        need_ok(run(["cmd", "/c", str(ROOT / "build_loadcheck_arm32.bat")], cwd=ROOT), "build_loadcheck_arm32.bat")
    dll = dist / "win32craft.dll"
    loadcheck = dist / "loadcheck.exe"
    for path, what in ((dll, "win32craft.dll (run build_arm32.bat or use --build)"), (loadcheck, "loadcheck.exe (run build_loadcheck_arm32.bat or use --build)")):
        if not path.is_file():
            die("%s not found: %s" % (what, path))
    if pe_machine(dll) != 0x1C4 or pe_machine(loadcheck) != 0x1C4:
        die("win32craft.dll / loadcheck.exe are not ARMNT images")
    stale = [n for n in SOURCES_OF_DLL if (ROOT / n).is_file() and (ROOT / n).stat().st_mtime > dll.stat().st_mtime + 1.0]
    if stale:
        die("%s is OLDER than %s: rebuild first (build_arm32.bat, or use --build)" % (dll, ", ".join(stale)))
    redist = args.redist
    if not redist.is_dir():
        die("VC++ redistributable folder not found: %s (set WIN32CRAFT_REDIST or --redist)" % redist)

    # 3. patcher (it copies the game folder: the input is never modified)
    out_root.mkdir(parents=True, exist_ok=True)
    marker = target / ".win32craft-package"
    if target.exists():
        if marker.exists():
            shutil.rmtree(target)                       # only our own generated output folder
        elif any(target.iterdir()):
            die("%s exists, is not empty and is not a package generated by this tool" % target)
    command = [sys.executable, ROOT / "tools" / "arm32-patcher.py", "patch", "--profile", G["profile"], game, target, "--host-dll", dll]
    if args.companion_appx:
        command += ["--companion-appx", args.companion_appx]
    need_ok(run(command, cwd=ROOT), "the patcher (tools/arm32-patcher.py)")
    exe = target / EXE_NAME
    for name in (EXE_NAME, "win32craft.dll", "msvcr120_app.dll", "msvcp120_app.dll", "vccorlib120_app.dll", "patch_report.json"):
        if not (target / name).is_file():
            die("the patcher did not produce %s" % name)
    if sha256(target / "win32craft.dll") != sha256(dll):
        die("package win32craft.dll differs from %s" % dll)
    if sha256(exe) == sha256(game / G["stock_exe"]):
        die("the patched exe is identical to the original")
    marker.write_text("generated by tools/make_0104_package.py\n", encoding="ascii")

    # 4. VC++ runtime DLLs of the host (+ anything VC-like imported transitively)
    wanted: dict[str, Path] = {}
    for name in REQUIRED_VC_DLLS:
        candidate = redist / name
        if not candidate.is_file():
            die("redistributable %s not found in %s" % (name, redist))
        wanted[name] = candidate
    queue = [dll] + list(wanted.values())
    seen: set[str] = set()
    while queue:
        image = queue.pop()
        for imported in pe_imports(image) or set():
            if imported in seen or not VC_NAME.match(imported) or imported.endswith("_app.dll"):
                continue
            seen.add(imported)
            if imported not in wanted:
                candidate = redist / imported
                if not candidate.is_file():
                    die("%s imports %s, which is not in %s" % (image.name, imported, redist))
                wanted[imported] = candidate
                queue.append(candidate)
                print("extra redistributable DLL needed:", imported)
    for name, path in wanted.items():
        if pe_machine(path) != 0x1C4:
            die("redistributable %s is not ARMNT" % path)
        shutil.copyfile(path, target / name)

    # 5. api-set scan + Windows RT 8.1 app-local runtime
    apiset_lines: list[str] = []
    pes = sorted(p for p in target.iterdir() if p.is_file() and p.suffix.lower() in (".exe", ".dll"))
    scan = subprocess.run([sys.executable, ROOT / "tools" / "arm32" / "apiset_scan.py"] + [str(p) for p in pes], capture_output=True, text=True)
    apiset_lines = (scan.stdout or "").splitlines()
    (target / "APISETS.txt").write_text("api-set contracts imported by the shipped binaries (\"MISSING\" = not in the SDK app-local UCRT set; forwarder DLLs for them are generated)\r\n\r\n"
                                        + "\r\n".join(apiset_lines) + "\r\n", encoding="ascii", errors="replace")
    runtime_files: list[Path] = []
    if not args.no_apiset:
        staging = out_root / "_win81-runtime"
        result = run([sys.executable, ROOT / "tools" / "make_arm32_apiset_shims.py", "--out", staging, "--work", ROOT / "build-arm32" / G["work"],
                      "--pe"] + [str(p) for p in pes], cwd=ROOT)
        need_ok(result, "make_arm32_apiset_shims.py (a contract could not be satisfied)")
        runtime_files = sorted(staging.glob("*.dll"))
        if len(runtime_files) < 40:
            die("the win81 runtime looks incomplete (%d files)" % len(runtime_files))
        for path in runtime_files:
            shutil.copyfile(path, target / path.name)
    shutil.copyfile(loadcheck, target / "loadcheck.exe")

    # 7. batch files, README
    option_help = []
    for name, options, text in BATCHES:
        (target / name).write_bytes(('@echo off\r\ncd /d "%~dp0"\r\nstart "" "{exe}" {opt}\r\n'.format(exe=EXE_NAME, opt=options)).encode("ascii"))
        option_help.append("   %-30s %-24s %s" % (name, options or "(none)", text))
    foundation_note = ""
    if stubs:
        foundation_note = ("\n   *** THIS IS A FOUNDATION-ONLY BUILD: the host units are not implemented yet (%s); the game will NOT start.  It exists to verify\n"
                           "   the packaging (patched exe, runtime DLLs, loadcheck).  Run loadcheck.exe on the device to test the loader side only. ***\n" % ", ".join(
                               s.replace("win32_host_arm32_0104_", "").replace(".inl", "") for s in stubs))
    (target / "README_DEVICE.txt").write_bytes(README.format(status=status, exe=EXE_NAME, batches="\n".join(option_help), foundation_note=foundation_note)
                                               .replace("\n", "\r\n").encode("ascii", "replace"))

    # 8. dependency check
    print("\ndependency check:")
    present = {p.name.lower() for p in target.iterdir() if p.is_file()}
    problems = 0
    for pe_path in sorted(list(target.glob("*.exe")) + list(target.glob("*.dll"))):
        names = pe_imports(pe_path)
        if names is None:
            print("  %-34s not parseable" % pe_path.name)
            problems += 1
            continue
        missing = sorted(n for n in names if n not in present and n not in SYSTEM_DLLS and not n.startswith(("api-ms-win-", "ext-ms-win-")))
        if missing or pe_path.name.lower() in ("win32craft.dll", EXE_NAME.lower()):
            print("  %-34s %3d import DLLs%s" % (pe_path.name, len(names), ("   UNRESOLVED: " + ", ".join(missing)) if missing else ""))
        problems += len(missing)
    if problems:
        print("WARNING: %d unresolved dependencies (see above)" % problems)

    # patcher report summary for the manifest
    report = json.loads((target / "patch_report.json").read_text(encoding="utf-8"))
    summary = "patch_report.json: profile %s" % report.get("profile", report.get("exe", {}).get("profile", "?")) if isinstance(report, dict) else "patch_report.json"

    # MANIFEST
    lines = ["Win32Craft %s ARM32 package manifest" % G["title"], "", "Status: " + status, "Generated: " + time.strftime("%Y-%m-%d %H:%M:%S"), "",
             "Inputs (read only):",
             "  %-26s  %s  %d bytes" % (G["stock_exe"], sha256(game / G["stock_exe"]), (game / G["stock_exe"]).stat().st_size),
             "  win32craft.dll (built %s)   %s  %d bytes" % (time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(dll.stat().st_mtime)), sha256(dll), dll.stat().st_size),
             "  redistributable dir         %s" % redist, "  " + summary, "",
             "Package files (SHA-256, size):"]
    for path in sorted(p for p in target.rglob("*") if p.is_file() and p.name not in (".win32craft-package", "MANIFEST.txt")):
        lines.append("  %s  %10d  %s" % (sha256(path), path.stat().st_size, path.relative_to(target).as_posix()))
    lines += ["", "Nothing in this package has been run on a device yet." if not stubs else "FOUNDATION-ONLY: the host units are stubs, the game does not start.", ""]
    (target / "MANIFEST.txt").write_text("\r\n".join(lines), encoding="ascii", errors="replace")

    # 9. zip
    zpath = out_root / (folder_name + ".zip")
    if not args.no_zip:
        if zpath.exists():
            zpath.unlink()
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for path in sorted(target.rglob("*")):
                if path.is_file() and path.name != ".win32craft-package":
                    archive.write(path, Path(folder_name) / path.relative_to(target))
    total = sum(p.stat().st_size for p in target.rglob("*") if p.is_file())
    print("\nfolder: %s  (%.1f MB, %d files)" % (target, total / 1e6, sum(1 for p in target.rglob('*') if p.is_file())))
    if not args.no_zip:
        print("zip:    %s  (%.1f MB)  sha256 %s" % (zpath, zpath.stat().st_size / 1e6, sha256(zpath)))
    print("status:", status)
    print("unresolved dependencies:", problems)


if __name__ == "__main__":
    main()
