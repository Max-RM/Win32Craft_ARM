"""Assemble dist-gd-host/ : the Geometry Dash M1 host probe package for the Surface 2 / Surface RT.

Contents:
  - gd_host.exe (the M1 headless host; build_gd_host_arm32.bat)
  - the 8 GD native DLLs from Geometry_Dash_2.0.1.0/ (fmodex_80_arm.dll APPCONTAINER bit cleared)
  - the app-local VC110 trio (msvcr110/msvcp110/vccorlib110) from the VCLibs110 appx
  - the two Win32Craft shims (PhoneAppModelHost.dll, PhoneAudioSes.dll)
  - the game's Assets/ folder (cocos2d-x resources)

Running gd_host.exe loads GeometryJumpComponent.dll, drives the DrawingSurface call sequence, creates a FL9_1 device
and calls Draw. It writes gd_host.log (every step + every host-object slot the component calls) and, if the first
frame renders, gd_frame.bmp - both next to the exe's %LOCALAPPDATA% (printed in the log). NOTE: the first *rendered*
frame needs the WinRT asset-path shim (Windows.ApplicationModel.Package / Storage) which is not built yet; this run is
expected to drive the component correctly and show exactly where cocos2d stops on asset loading.

The game folder and the .appx are READ-ONLY; this only reads/copies from them. Output is a build artifact (not for git).
Run: python -I tools/gd/make_gd_host_package.py
"""
import os, shutil, zipfile, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, "Geometry_Dash_2.0.1.0")
VCLIBS = os.path.join("D:\\", "repo", "armdeps", "Microsoft.VCLibs.110.00_11.0.51106.1_arm__8wekyb3d8bbwe.Appx")
HOSTEXE = os.path.join(ROOT, "dist-gd", "gd_host.exe")
SHIMS = os.path.join(ROOT, "dist-gd")
OUT = os.path.join(ROOT, "dist-gd-host")

GAME_DLLS = ["GeometryJumpComponent.dll", "libcocos2d.dll", "libEGL_phone.dll", "libGLESv2_phone.dll",
             "fmodex_80_arm.dll", "libcurl.dll", "libeay32.dll", "ssleay32.dll"]
VC110 = ["msvcr110.dll", "msvcp110.dll", "vccorlib110.dll"]
SHIM_DLLS = ["PhoneAppModelHost.dll", "PhoneAudioSes.dll"]

def machine_is_armnt(path):
    with open(path, "rb") as f:
        d = f.read(0x400)
    pe = int.from_bytes(d[0x3c:0x40], "little")
    return d[:2] == b"MZ" and d[pe:pe+4] == b"PE\0\0" and int.from_bytes(d[pe+4:pe+6], "little") == 0x1c4

def clear_appcontainer(path):
    with open(path, "r+b") as f:
        d = bytearray(f.read())
        pe = int.from_bytes(d[0x3c:0x40], "little")
        off = pe + 24 + 70
        dc = int.from_bytes(d[off:off+2], "little")
        if dc & 0x1000:
            d[off:off+2] = (dc & ~0x1000).to_bytes(2, "little")
            f.seek(0); f.write(d)
            return "cleared APPCONTAINER (0x%04x -> 0x%04x)" % (dc, dc & ~0x1000)
        return "APPCONTAINER already clear (0x%04x)" % dc

def main():
    for p, what in [(GAME, "game folder"), (VCLIBS, "VCLibs110 appx"), (HOSTEXE, "gd_host.exe"),
                    (os.path.join(GAME, "Assets"), "game Assets folder")]:
        if not os.path.exists(p):
            print("ERROR: missing %s: %s" % (what, p)); return 2

    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    report = ["Win32Craft - Geometry Dash host probe (M1)", ""]

    shutil.copyfile(HOSTEXE, os.path.join(OUT, "gd_host.exe"))
    if not machine_is_armnt(os.path.join(OUT, "gd_host.exe")):
        print("ERROR: gd_host.exe is not ARMNT"); return 3
    report.append("gd_host.exe <- dist-gd")

    for n in GAME_DLLS:
        src = os.path.join(GAME, n)
        if not os.path.exists(src):
            print("ERROR: game DLL not found: %s" % src); return 3
        dst = os.path.join(OUT, n)
        shutil.copyfile(src, dst)
        if not machine_is_armnt(dst):
            print("ERROR: %s is not ARMNT" % n); return 3
    report.append("fmodex_80_arm.dll: " + clear_appcontainer(os.path.join(OUT, "fmodex_80_arm.dll")))
    report.append("game native DLLs: " + ", ".join(GAME_DLLS))

    with zipfile.ZipFile(VCLIBS) as z:
        members = {os.path.basename(m).lower(): m for m in z.namelist()}
        for n in VC110:
            with z.open(members[n.lower()]) as fsrc, open(os.path.join(OUT, n), "wb") as fdst:
                shutil.copyfileobj(fsrc, fdst)
    report.append("VC110 runtime (app-local): " + ", ".join(VC110))

    for n in SHIM_DLLS:
        shutil.copyfile(os.path.join(SHIMS, n), os.path.join(OUT, n))
    report.append("Win32Craft shims: " + ", ".join(SHIM_DLLS))

    # the game's assets (cocos2d-x resources) beside the exe
    shutil.copytree(os.path.join(GAME, "Assets"), os.path.join(OUT, "Assets"))
    nassets = sum(len(fs) for _, _, fs in os.walk(os.path.join(OUT, "Assets")))
    report.append("Assets/: %d files copied beside the exe" % nassets)

    with open(os.path.join(OUT, "run_gd_host.bat"), "w", newline="\r\n") as f:
        f.write("@echo off\r\n")
        f.write('cd /d "%~dp0"\r\n')
        f.write("gd_host.exe\r\n")
        f.write("echo.\r\n")
        f.write("echo The log is at %%LOCALAPPDATA%%\\gd_host.log  (and gd_frame.bmp if a frame rendered).\r\n")
        f.write("pause\r\n")
    with open(os.path.join(OUT, "README.txt"), "w", newline="\r\n") as f:
        f.write("Geometry Dash - host probe (Win32Craft M1)\r\n\r\n")
        f.write("Copy this whole folder to the Surface 2, then run run_gd_host.bat.\r\n")
        f.write("It loads the game's native component and drives the WP8 DrawingSurface sequence, then calls Draw.\r\n")
        f.write("Afterwards send back these two files from %LOCALAPPDATA% (C:\\Users\\<you>\\AppData\\Local):\r\n")
        f.write("  gd_host.log    - every step + every call the game makes back into the host\r\n")
        f.write("  gd_frame.bmp   - the first rendered frame, IF it got that far\r\n")

    files = sorted(os.listdir(OUT))
    total = 0
    for dp, _, fs in os.walk(OUT):
        for fn in fs:
            total += os.path.getsize(os.path.join(dp, fn))
    print("\n".join(report))
    print("\nfolder: %s  (%.1f MB)" % (OUT, total / 1e6))
    for f in files:
        p = os.path.join(OUT, f)
        if os.path.isdir(p):
            print("  <dir>       %s/" % f)
        else:
            print("  %10d  %s" % (os.path.getsize(p), f))
    return 0

if __name__ == "__main__":
    sys.exit(main())
