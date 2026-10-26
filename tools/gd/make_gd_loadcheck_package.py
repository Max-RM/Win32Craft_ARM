"""Assemble dist-gd-loadcheck/ : the Geometry Dash M0a dependency-check package for the Surface 2 / Surface RT.

It gathers, into one folder the user copies to the device and runs:
  - loadcheck.exe (built by build_loadcheck_arm32.bat, with the GD module list)
  - the 8 GD native DLLs from Geometry_Dash_2.0.1.0/ (fmodex_80_arm.dll gets its APPCONTAINER bit cleared)
  - the app-local VC110 trio (msvcr110/msvcp110/vccorlib110) from the VCLibs110 appx
  - the two Win32Craft shims (PhoneAppModelHost.dll, PhoneAudioSes.dll) from dist-gd/

Running loadcheck.exe there LoadLibrary+GetProcAddress's every import of every module through the real OS loader;
"RESULT: 0 problem(s)" proves the whole GD import chain resolves before any host code exists (gd-deps F11).

The game folder and the .appx are READ-ONLY; this only reads/copies from them. Output is a build artifact (not for git).
Run: python -I tools/gd/make_gd_loadcheck_package.py
"""
import os, shutil, zipfile, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, "Geometry_Dash_2.0.1.0")
VCLIBS = os.path.join("D:\\", "repo", "armdeps", "Microsoft.VCLibs.110.00_11.0.51106.1_arm__8wekyb3d8bbwe.Appx")
LOADCHECK = os.path.join(ROOT, "dist-arm32", "loadcheck.exe")
SHIMS = os.path.join(ROOT, "dist-gd")
OUT = os.path.join(ROOT, "dist-gd-loadcheck")

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
    """Clear IMAGE_DLLCHARACTERISTICS_APPCONTAINER (0x1000) in the optional header DllCharacteristics (gd-deps F9)."""
    with open(path, "r+b") as f:
        d = bytearray(f.read())
        pe = int.from_bytes(d[0x3c:0x40], "little")
        dc_off = pe + 24 + 70          # COFF header (24) + optional-header DllCharacteristics at offset 70 (PE32)
        dc = int.from_bytes(d[dc_off:dc_off+2], "little")
        if dc & 0x1000:
            d[dc_off:dc_off+2] = (dc & ~0x1000).to_bytes(2, "little")
            f.seek(0); f.write(d)
            return "cleared APPCONTAINER (0x%04x -> 0x%04x)" % (dc, dc & ~0x1000)
        return "APPCONTAINER already clear (0x%04x)" % dc

def main():
    for p, what in [(GAME, "game folder"), (VCLIBS, "VCLibs110 appx"), (LOADCHECK, "loadcheck.exe"),
                    (os.path.join(SHIMS, SHIM_DLLS[0]), "PhoneAppModelHost.dll")]:
        if not os.path.exists(p):
            print("ERROR: missing %s: %s" % (what, p)); return 2

    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    report = ["Win32Craft - Geometry Dash dependency check (M0a)", ""]

    shutil.copyfile(LOADCHECK, os.path.join(OUT, "loadcheck.exe"))
    report.append("loadcheck.exe <- dist-arm32 (GD module list)")

    for n in GAME_DLLS:
        src = os.path.join(GAME, n)
        if not os.path.exists(src):
            print("ERROR: game DLL not found: %s" % src); return 3
        dst = os.path.join(OUT, n)
        shutil.copyfile(src, dst)
        if not machine_is_armnt(dst):
            print("ERROR: %s is not ARMNT" % n); return 3
    # fmod ships with the APPCONTAINER bit; clear it on our copy only (never the game folder)
    report.append("fmodex_80_arm.dll: " + clear_appcontainer(os.path.join(OUT, "fmodex_80_arm.dll")))
    report.append("game native DLLs copied: " + ", ".join(GAME_DLLS))

    with zipfile.ZipFile(VCLIBS) as z:
        members = {os.path.basename(m).lower(): m for m in z.namelist()}
        for n in VC110:
            m = members.get(n.lower())
            if not m:
                print("ERROR: %s not in the VCLibs110 appx" % n); return 4
            with z.open(m) as fsrc, open(os.path.join(OUT, n), "wb") as fdst:
                shutil.copyfileobj(fsrc, fdst)
            if not machine_is_armnt(os.path.join(OUT, n)):
                print("ERROR: %s from appx is not ARMNT" % n); return 4
    report.append("VC110 runtime (app-local, no rename): " + ", ".join(VC110))

    for n in SHIM_DLLS:
        src = os.path.join(SHIMS, n)
        if not os.path.exists(src):
            print("ERROR: shim not built: %s (run build_gd_shims_arm32.bat first)" % src); return 5
        shutil.copyfile(src, os.path.join(OUT, n))
        if not machine_is_armnt(os.path.join(OUT, n)):
            print("ERROR: %s is not ARMNT" % n); return 5
    report.append("Win32Craft shims: " + ", ".join(SHIM_DLLS))

    with open(os.path.join(OUT, "run_loadcheck.bat"), "w", newline="\r\n") as f:
        f.write("@echo off\r\n")
        f.write('"%~dp0loadcheck.exe" "%~dp0."\r\n')
    with open(os.path.join(OUT, "README.txt"), "w", newline="\r\n") as f:
        f.write("Geometry Dash - dependency check (Win32Craft M0a)\r\n\r\n")
        f.write("Copy this whole folder to the Surface 2 / Surface RT, then run run_loadcheck.bat (or loadcheck.exe).\r\n")
        f.write("It checks that every DLL import of the Geometry Dash native stack resolves on this device.\r\n")
        f.write("Expected last line: '==== RESULT: 0 problem(s) ===='.\r\n")
        f.write("It also writes loadcheck.txt next to it - send that file back if there is any problem.\r\n")

    files = sorted(os.listdir(OUT))
    total = sum(os.path.getsize(os.path.join(OUT, f)) for f in files)
    print("\n".join(report))
    print("\nfolder: %s  (%d files, %.1f MB)" % (OUT, len(files), total / 1e6))
    for f in files:
        print("  %10d  %s" % (os.path.getsize(os.path.join(OUT, f)), f))
    return 0

if __name__ == "__main__":
    sys.exit(main())
