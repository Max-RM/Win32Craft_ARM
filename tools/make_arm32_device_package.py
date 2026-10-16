#!/usr/bin/env python3
"""Assemble the COMPLETE, ready-to-run device folder (no Python or patching needed on the device).

    python tools\\make_arm32_device_package.py            rebuild everything, assemble folder + zip
    python tools\\make_arm32_device_package.py --no-build reuse the existing dist-arm32 outputs

Output (under dist-arm32\\):
    device\\Minecraft_1.2.8.0_ARM32_Win32Craft\\   a copy of the game folder with the patched exe, host DLL, patched FMOD, VC++
                                                  runtime DLLs, silent-FMOD fallback and helper .bat files already in place
    Minecraft_1.2.8.0_ARM32_Win32Craft.zip        the same folder zipped (copy either to the device)
The original game folder is only read.  Steps: make_arm32_package.py --build --force (build_arm32.bat + patcher + overlay),
copy the game folder, apply the overlay, dependency-check every PE in the folder, write README_DEVICE.txt/MANIFEST.txt, zip.
"""
import argparse, hashlib, os, shutil, subprocess, sys, zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(HERE / "arm32"))
import arm32re  # noqa: E402

FOLDER = "Minecraft_1.2.8.0_ARM32_Win32Craft"
SYSTEM_DLLS = {  # present on Windows 8.1+/10 ARM32 desktop (not shipped)
    "kernel32.dll", "user32.dll", "shell32.dll", "comdlg32.dll", "d3d11.dll", "dxgi.dll", "d3dcompiler_47.dll",
    "ws2_32.dll", "iphlpapi.dll", "mmdevapi.dll", "mfplat.dll", "mfreadwrite.dll", "combase.dll", "ole32.dll",
    "winmm.dll", "ntdll.dll", "advapi32.dll", "gdi32.dll", "oleaut32.dll", "ucrtbase.dll", "wintypes.dll",
    "propsys.dll", "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll", "bcrypt.dll", "crypt32.dll", "rpcrt4.dll",
}
PE_SUFFIXES = (".exe", ".dll")

README = """Win32Craft - Minecraft 1.2.8 ARM32 (Win32 desktop host)           [NOT yet verified on a real device]
==========================================================================================================

This folder is COMPLETE and READY TO RUN. Nothing needs to be patched or installed on the device (no Python).

1. Copy this whole folder to the device, to a normal writable location (for example C:\\Games\\Minecraft128).
   Do NOT run it from the WindowsApps folder.  The device needs: Windows RT 8.1 (the Universal C Runtime and the
   missing API-set DLLs are included in this folder, no KB2999226 needed) or Windows 10 ARM32 desktop.  Unsigned code must be
   allowed to run on the device (as for any desktop app you built yourself).
2. Start  Win32Craft.1.2.8-arm32.exe   (double-click).  It opens a window and runs the game at Direct3D feature level 9_1.
   Other level: start_fl93.bat / start_fl100.bat, or run  Win32Craft.1.2.8-arm32.exe --feature-level 9_3 .
3. LOG: win32craft.log is written next to the exe (or in %TEMP% if the folder is read-only).
   If anything goes wrong - black window, crash, freeze, no input - close the game and send me win32craft.log
   (and win32craft.log.old if it exists) plus a short note of what you saw on screen.

STARTUP ERRORS ("missing DLL", "procedure entry point ... could not be located"): run  loadcheck.exe  from this folder. It checks every DLL import
   of the game, the host and FMOD against your Windows in one go, shows which file served each api-ms-win-* name, and writes loadcheck.txt.
   Send me loadcheck.txt (plus the exact text of the error dialog, including the DLL name in it).

TOUCH (no keyboard / mouse attached, e.g. Surface RT): just start the game. The on-screen touch controls (joystick, jump, sneak) appear once a
world is open and the host has seen a finger; the log lines "ARM32 input: game input mode ... -> Touch" and "ARM32 touch: down/up" show it.
   If touch does not work, try in this order and send win32craft.log after each try:  start_touch.bat  (pin touch),  start_touch_wmtouch.bat,
   start_touch_legacy.bat  (single finger only).  start_mouse.bat = the old behaviour (finger = mouse, no touch controls).
   start_touch_inputlog.bat logs every touch event (large log).  A text box (chat) opens the Windows touch keyboard.

Audio:  the folder uses the real game FMOD (patched).  If the game FREEZES at startup (window stays blank, log stops growing),
run  use_silent_audio.bat  (replaces fmod_ARM.dll with a silent stub: no sound, no hang) and start again.
        use_real_audio.bat  switches back.

Files added to the original game folder:
  Win32Craft.1.2.8-arm32.exe   patched game executable (the original Minecraft.Windows.exe is left untouched)
  win32craft.dll               the ARM32 host (window, Direct3D device, fake WinRT services, input)
  fmod_ARM.dll                 patched FMOD (original saved as fmod_ARM.dll.orig)
  fmod-silent\\fmod_ARM.dll     silent FMOD fallback
  vccorlib140.dll msvcp140.dll concrt140.dll vcruntime140.dll   desktop Visual C++ runtime (ARM) the patched exe uses
  ucrtbase.dll + api-ms-win-*.dll (64 small files)   app-local Universal CRT and API-set forwarders for WINDOWS RT 8.1
                               (Windows 8.1 lacks api-ms-win-core-heap-l2-1-0 and ~20 other contracts; the loader then
                               picks these files from this folder. On Windows 10 they are ignored/harmless.)
  loadcheck.exe                diagnostic: finds missing DLLs / entry points on the device
  use_silent_audio.bat use_real_audio.bat start_fl*.bat          helpers
  start_touch*.bat start_mouse.bat   input helpers (see TOUCH above)
  MANIFEST.txt                 SHA-256 of the added files
"""

START_BAT = """@echo off
cd /d "%~dp0"
start "" "Win32Craft.1.2.8-arm32.exe" --feature-level {level}
"""

INPUT_BAT = """@echo off
cd /d "%~dp0"
start "" "Win32Craft.1.2.8-arm32.exe" {options}
"""

SILENT_BAT = """@echo off
cd /d "%~dp0"
if not exist "fmod-silent\\fmod_ARM.dll" (echo fmod-silent\\fmod_ARM.dll is missing & pause & exit /b 1)
if not exist "fmod_ARM.real.dll" copy /y "fmod_ARM.dll" "fmod_ARM.real.dll" >nul
copy /y "fmod-silent\\fmod_ARM.dll" "fmod_ARM.dll" >nul && echo Silent FMOD installed. Start the game again.
pause
"""

REAL_BAT = """@echo off
cd /d "%~dp0"
if not exist "fmod_ARM.real.dll" (echo Already using the real FMOD, nothing to restore. & pause & exit /b 0)
copy /y "fmod_ARM.real.dll" "fmod_ARM.dll" >nul && echo Real (patched) FMOD restored.
pause
"""


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run(cmd, **kw):
    print("+", " ".join(str(c) for c in cmd))
    r = subprocess.run(cmd, **kw)
    if r.returncode:
        raise SystemExit("command failed (%d): %s" % (r.returncode, cmd))


def pe_imports(path):
    try:
        pe = arm32re.Pe(str(path))
    except Exception as e:  # not a valid PE
        return None, str(e)
    names = set()
    for imp in pe.imports():
        if not imp["delay"]:
            names.add(imp["dll"].lower())
    return names, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", default=str(REPO / "Minecraft_1.2.8.0_ARM32"))
    ap.add_argument("--dist", default=str(REPO / "dist-arm32"))
    ap.add_argument("--no-build", action="store_true", help="reuse the existing dist-arm32 outputs and package")
    a = ap.parse_args()
    game, dist = Path(a.game_dir), Path(a.dist)
    pkg = dist / "package"
    if not a.no_build:
        run([sys.executable, str(HERE / "make_arm32_package.py"), "--build", "--force"], cwd=str(REPO))
    for need in ("Win32Craft.1.2.8-arm32.exe", "win32craft.dll", "fmod_ARM.dll", "fmod-silent/fmod_ARM.dll",
                 "vccorlib140.dll", "msvcp140.dll", "concrt140.dll", "vcruntime140.dll"):
        if not (pkg / need).exists():
            raise SystemExit("package is missing %s - run without --no-build" % need)
    if (pkg / "win32craft.dll").read_bytes() != (dist / "win32craft.dll").read_bytes():
        raise SystemExit("package win32craft.dll differs from dist-arm32\\win32craft.dll (stale package): run without --no-build")

    out_root = dist / "device"
    target = out_root / FOLDER
    if target.exists():
        shutil.rmtree(target)  # only our own generated output folder
    out_root.mkdir(parents=True, exist_ok=True)
    print("copying game folder ->", target)
    shutil.copytree(game, target)

    added = []
    orig_fmod = target / "fmod_ARM.dll"
    orig_fmod.rename(target / "fmod_ARM.dll.orig")
    for name in ("Win32Craft.1.2.8-arm32.exe", "win32craft.dll", "fmod_ARM.dll", "vccorlib140.dll", "msvcp140.dll",
                 "concrt140.dll", "vcruntime140.dll"):
        shutil.copy2(pkg / name, target / name)
        added.append(name)
    # --- Windows 8.1 app-local runtime: SDK Universal CRT + forwarders for api-set contracts 8.1 lacks
    run([sys.executable, str(HERE / "make_arm32_apiset_shims.py")], cwd=str(REPO))
    runtime_dir = dist / "win81-runtime"
    runtime_files = sorted(runtime_dir.glob("*.dll"))
    if len(runtime_files) < 60:
        raise SystemExit("win81-runtime looks incomplete (%d files)" % len(runtime_files))
    for p in runtime_files:
        shutil.copy2(p, target / p.name)
        added.append(p.name)
    run(["cmd", "/c", str(REPO / "build_loadcheck_arm32.bat")], cwd=str(REPO))
    shutil.copy2(dist / "loadcheck.exe", target / "loadcheck.exe")
    added.append("loadcheck.exe")
    (target / "fmod-silent").mkdir()
    shutil.copy2(pkg / "fmod-silent" / "fmod_ARM.dll", target / "fmod-silent" / "fmod_ARM.dll")
    added.append("fmod-silent/fmod_ARM.dll")
    for fname, level in (("start_fl91.bat", "9_1"), ("start_fl93.bat", "9_3"), ("start_fl100.bat", "10_0")):
        (target / fname).write_bytes(START_BAT.format(level=level).replace("\n", "\r\n").encode("ascii"))
        added.append(fname)
    for fname, options in (("start_touch.bat", "--input=touch"), ("start_mouse.bat", "--input=mouse"),
                           ("start_touch_legacy.bat", "--touch-api=legacy"), ("start_touch_wmtouch.bat", "--touch-api=wmtouch"),
                           ("start_touch_inputlog.bat", "--input-log")):
        (target / fname).write_bytes(INPUT_BAT.format(options=options).replace("\n", "\r\n").encode("ascii"))
        added.append(fname)
    (target / "use_silent_audio.bat").write_bytes(SILENT_BAT.replace("\n", "\r\n").encode("ascii"))
    (target / "use_real_audio.bat").write_bytes(REAL_BAT.replace("\n", "\r\n").encode("ascii"))
    added += ["use_silent_audio.bat", "use_real_audio.bat"]
    (target / "README_DEVICE.txt").write_bytes(README.replace("\n", "\r\n").encode("ascii", "replace"))
    added.append("README_DEVICE.txt")

    # --- dependency check of every PE in the folder root (+ the silent fmod)
    print("\ndependency check:")
    present = {p.name.lower() for p in target.iterdir() if p.is_file()}
    problems = 0
    for pe_path in sorted(list(target.glob("*.exe")) + list(target.glob("*.dll")) + [target / "fmod-silent" / "fmod_ARM.dll"]):
        names, err = pe_imports(pe_path)
        if names is None:
            print("  %-34s not parseable: %s" % (pe_path.name, err))
            problems += 1
            continue
        missing = sorted(n for n in names if n not in present and n not in SYSTEM_DLLS
                         and not n.startswith(("api-ms-win-", "ext-ms-win-")))
        print("  %-34s %3d import DLLs%s" % (pe_path.name, len(names),
                                             ("   UNRESOLVED: " + ", ".join(missing)) if missing else ""))
        problems += len(missing)
    if problems:
        print("\nWARNING: %d unresolved dependencies (see above)" % problems)

    # --- manifest + zip
    lines = ["Added/changed files (SHA-256, size):", ""]
    for rel in sorted(added):
        p = target / rel
        lines.append("%s  %10d  %s" % (sha256(p), p.stat().st_size, rel))
    lines += ["", "Original files preserved: Minecraft.Windows.exe (untouched), fmod_ARM.dll.orig (original FMOD)",
              "Original Minecraft.Windows.exe SHA-256: " + sha256(game / "Minecraft.Windows.exe")]
    (target / "MANIFEST.txt").write_text("\r\n".join(lines) + "\r\n", encoding="ascii")
    # overlay zip: just the Windows 8.1 runtime, to extract INTO an already copied game folder
    ozip = dist / "Win32Craft-ARM32-win81-runtime-overlay.zip"
    if ozip.exists():
        ozip.unlink()
    with zipfile.ZipFile(ozip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in runtime_files:
            z.write(p, p.name)
        z.write(dist / "loadcheck.exe", "loadcheck.exe")
        z.writestr("README_WIN81_RUNTIME.txt",
                   "Extract ALL files from this zip INTO the game folder (next to Win32Craft.1.2.8-arm32.exe), overwrite if asked.\r\n"
                   "They are the app-local Universal CRT (ucrtbase.dll) and API-set forwarder DLLs needed on Windows RT 8.1.\r\n"
                   "Windows 10 ignores them. Nothing else changes.\r\n")
    print("overlay zip:", ozip, "(%.2f MB)" % (ozip.stat().st_size / 1e6))
    zpath = dist / (FOLDER + ".zip")
    if zpath.exists():
        zpath.unlink()
    print("\nzipping ->", zpath)
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(target.rglob("*")):
            if p.is_file():
                z.write(p, Path(FOLDER) / p.relative_to(target))
    total = sum(p.stat().st_size for p in target.rglob("*") if p.is_file())
    print("\nfolder: %s  (%.1f MB)\nzip:    %s  (%.1f MB)  sha256 %s" % (
        target, total / 1e6, zpath, zpath.stat().st_size / 1e6, sha256(zpath)))
    print("unresolved dependencies:", problems)


if __name__ == "__main__":
    main()
