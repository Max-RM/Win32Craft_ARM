#!/usr/bin/env python3
"""Assemble the ready-to-install Bad Piggies Rebooted package for ARM32 Windows 10 (Surface 2 and, later, RT 8.1), rendering on the GPU at
Direct3D feature level 9_1 via the l9shim D3D11 shim.

Bad Piggies is a Windows 10 UWP appx (Unity 2018.4, IL2CPP) whose MinVersion 10.0.15035 is exactly the Surface 2's build, so it installs natively in
Developer Mode (Add-AppxPackage -Register) - no Win32Craft host.  The only change needed to render is to put the l9shim between Unity and Direct3D and to
make the game self-contained in its runtime:
  1. redirect UnityPlayer.dll's d3d11.dll import to l9d3d11.dll (the shim; it forwards everything to the real d3d11.dll except D3D11CreateDevice, which it
     makes succeed on a 9_1 GPU, swaps the shaders for level-9 and sanitises the states/caps - see tools/badpiggies/l9shim.c);
  2. app-local the VC++ runtime: the binaries import the VCLibs.140 framework (MinVersion 14.0.33519, newer than anything we can ship), so the *_app runtime
     imports are renamed to the plain names and the VS2022 ARM redistributables are shipped next to the exe, and the VCLibs.140 PackageDependency is dropped
     (the Universal CRT is in-box on Windows 10 15035).  Same technique as the Minecraft 1.2.8 ARM package.
  3. ship l9d3d11.dll + Data\\bp\\l9_replacements.bin (the 113 level-9 shader replacements, proven to create on the Surface 2 Tegra by tools/m0probe).

Nothing in the game's own code or assets is changed.  Source: BadPiggiesRebooted_1.0.0.0_ARM.appx (read only).  Output: dist-badpiggies\\BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft\\ (+ .zip).
Run tools/badpiggies/build_l9shim_arm32.bat first (it produces dist-badpiggies/l9shim.dll)."""
import hashlib, importlib.util, os, re, shutil, struct, sys, urllib.parse, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
APPX = ROOT / "BadPiggiesRebooted_1.0.0.0_ARM.appx"
SHIM = ROOT / "dist-badpiggies" / "l9shim.dll"
TABLE = ROOT / "games_recon" / "bad_piggies_shader_proto" / "auto" / "l9_replacements.bin"
REDIST = Path(os.environ.get("WIN32CRAFT_REDIST_VC143",
                             r"D:\Program Files\vs22buildtools\VC\Redist\MSVC\14.44.35112\onecore\arm\Microsoft.VC143.CRT"))
OUT_ROOT = ROOT / "dist-badpiggies"
FOLDER = "BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft"
UNITYPLAYER_SHA = "e3530fd5d6cc0e431e5edb69623b7c18e1a57a8013914d958c5f02711cb03793"
SHIM_NAME = "l9d3d11.dll"          # the shim's file name inside the package (UnityPlayer's d3d11 import is redirected to this)

# per binary: the import DLL renames (old -> new).  *_app runtime -> app-local plain names; UnityPlayer's d3d11 -> the shim.
RENAMES = {
    "BadPiggiesRebooted.exe": [("VCRUNTIME140_APP.dll", "vcruntime140.dll"), ("vccorlib140_app.DLL", "vccorlib140.dll")],
    "UnityPlayer.dll": [("VCRUNTIME140_APP.dll", "vcruntime140.dll"), ("MSVCP140_APP.dll", "msvcp140.dll"),
                        ("vccorlib140_app.DLL", "vccorlib140.dll"), ("d3d11.dll", SHIM_NAME)],
    "GameAssembly.dll": [("VCRUNTIME140_APP.dll", "vcruntime140.dll"), ("MSVCP140_APP.dll", "msvcp140.dll")],
}
VC_DLLS = ["vcruntime140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
           "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll", "vccorlib140.dll", "concrt140.dll"]
SHIM_EXPORTS = {"d3d11createdevice", "createdirect3d11surfacefromdxgisurface", "createdirect3d11devicefromdxgidevice"}

INSTALL_PS1 = """# Install Bad Piggies Rebooted (Win32Craft FL 9_1 build) on this device.  Needs Developer Mode (Settings > Update & security > For developers).
Set-Location -LiteralPath $PSScriptRoot
$here = (Get-Location).Path
# The AppX deployment service cannot register an app from the ROOT of a drive (fails 0x80073CF9 / 0x80070020 "cannot access deployment target at C:").
# If we are at a drive root, copy ourselves into a normal subfolder and register from there.
if ($here -match '^[A-Za-z]:\\\\?$') {
    $dst = Join-Path $env:USERPROFILE 'Desktop\\BadPiggiesRebooted'
    Write-Host "This folder is at the drive root, which Windows cannot deploy from.  Copying to $dst ..."
    Copy-Item -LiteralPath $here -Destination $dst -Recurse -Force
    Set-Location -LiteralPath $dst
    $here = $dst
}
Write-Host "Registering Bad Piggies Rebooted from $here ..."
try {
    Add-AppxPackage -Register .\\AppxManifest.xml -ForceApplicationShutdown -ErrorAction Stop
    Write-Host 'Done.  Launch "Bad Piggies::Rebooted" from the Start menu.'
    Write-Host 'If it fails to start or looks wrong, send back the file  l9shim.log  from this folder (or your %TEMP% folder).'
} catch {
    Write-Host ''
    Write-Host 'REGISTRATION FAILED.  Writing the deployment log to appxlog.txt in this folder - please send that file back.'
    Write-Host $_.Exception.Message
    try { Get-AppxLog | Out-File -FilePath (Join-Path $here 'appxlog.txt') -Encoding utf8 } catch {}
    Write-Host 'Common causes: Developer Mode is off (Settings > Update & security > For developers), or the folder is read-only.'
}
"""
UNINSTALL_PS1 = """$ErrorActionPreference = 'SilentlyContinue'
Get-AppxPackage *BadPiggiesRebooted* | Remove-AppxPackage
Write-Host 'Removed.'
"""
BAT = "@echo off\r\npowershell -NoProfile -ExecutionPolicy Bypass -File \"%~dp0{0}.ps1\"\r\npause\r\n"
README = """Win32Craft - Bad Piggies Rebooted for ARM32 Windows, GPU rendering at Direct3D feature level 9_1   [FIRST DEVICE TEST]
==================================================================================================================

Target: Surface 2 (Windows 10 ARM32, build 15035) - its Tegra 4 GPU is a feature-level 9_1 device.  Stock Bad Piggies needs feature level 10 and so
renders in software (very slow) or refuses to start; this build adds a small shim (l9d3d11.dll) between Unity and Direct3D that runs the game on the GPU
at 9_1.  Nothing in the game's own code or assets is changed.

HOW TO INSTALL (once)
  1. On the tablet turn ON Developer Mode:  Settings > Update & security > For developers > Developer mode.
  2. Copy this WHOLE folder to the tablet, to a normal place such as the Desktop or C:\\Apps\\BadPiggies.
     IMPORTANT: do NOT put it at the root of a drive (not C:\\BadPiggies... directly) - Windows cannot register an app from a drive root
     (it fails with 0x80073CF9 / "cannot access deployment target at C:").  install.bat will move itself off the root automatically if needed.
  3. Double-click  install.bat  (or right-click install.ps1 > Run with PowerShell).  It registers the app.
  4. Launch "Bad Piggies::Rebooted" from the Start menu.

IF IT DOES NOT START or renders wrong
  Send back the file  l9shim.log  that appears in this folder (or in your %TEMP% folder) after you try to launch it.  It records which GPU and feature
  level the shim got, how many shaders it swapped, and any error.  That file tells me exactly what to fix.

WHAT IS IN HERE
  l9d3d11.dll                       the feature-level-9_1 Direct3D shim (UnityPlayer loads it in place of d3d11.dll)
  Data\\bp\\l9_replacements.bin       the 113 level-9 shader replacements the shim swaps in
  vcruntime140.dll msvcp140.dll ...  the Visual C++ runtime, app-local (so no framework package needs installing)
  UnityPlayer.dll / GameAssembly.dll / BadPiggiesRebooted.exe   the game, with its d3d11 and runtime imports repointed (nothing else changed)
  AppxManifest.xml, Assets\\, Data\\, resources.pri             the game's own files
  install.bat / uninstall.bat       register / remove the app
  PATCH_REPORT.txt, MANIFEST.txt    exactly what was changed, and the SHA-256 of every file

Windows RT 8.1 (Surface RT) is not covered by this build yet.
"""


def sha(data) -> str:
    return hashlib.sha256(bytes(data)).hexdigest()


def load_pe_class():
    spec = importlib.util.spec_from_file_location("mw", ROOT / "tools" / "whatsapp" / "make_whatsapp_package.py")
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except SystemExit:
        pass
    return mod.PE


def main() -> int:
    PE = load_pe_class()
    if not APPX.is_file():
        print("ERROR: %s not found" % APPX); return 1
    if not SHIM.is_file():
        print("ERROR: %s not found - run tools/badpiggies/build_l9shim_arm32.bat first" % SHIM); return 1
    if not TABLE.is_file():
        print("ERROR: shader table %s not found" % TABLE); return 1
    if not REDIST.is_dir():
        print("ERROR: VC143 ARM redistributable folder not found: %s (set WIN32CRAFT_REDIST_VC143)" % REDIST); return 1

    target = OUT_ROOT / FOLDER
    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    if target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)
    report = ["Win32Craft Bad Piggies Rebooted ARM32 package", "", "Source: %s" % APPX.name, ""]

    # 1. unpack the clean game from the appx.
    #    AppX/OPC part names are percent-encoded (RFC 3986): the one asset with a space,
    #    "Data/Resources/unity%20default%20resources", MUST land on disk as "unity default resources"
    #    or Unity's ResourceManager cannot load it and every builtin shader (incl. Internal-ErrorShader)
    #    resolves to NULL -> crash.  A naive unzip keeps the %20 and breaks the game, so decode each segment.
    decoded = []
    with zipfile.ZipFile(APPX) as z:
        for n in z.namelist():
            if n.endswith("/"):
                continue
            seg = [urllib.parse.unquote(s) for s in n.split("/")]
            if seg != n.split("/"):
                decoded.append((n, "/".join(seg)))
            dst = target / Path(*seg)
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(z.read(n))
    for raw, dec in decoded:
        report.append("percent-decoded part name: %s -> %s" % (raw, dec))
    up = target / "UnityPlayer.dll"
    if sha(up.read_bytes()) != UNITYPLAYER_SHA:
        print("ERROR: UnityPlayer.dll from the appx has an unexpected SHA-256 (%s) - wrong game build" % sha(up.read_bytes())[:16]); return 2
    report.append("UnityPlayer.dll verified: sha256 %s" % UNITYPLAYER_SHA)

    # 2. remove the packaged-install metadata (we register from the loose folder in Developer Mode)
    for junk in ("AppxSignature.p7x", "AppxBlockMap.xml", "AppxMetadata", "[Content_Types].xml", "StoreManifest.xml"):
        p = target / junk
        if p.is_dir():
            shutil.rmtree(p)
        elif p.is_file():
            p.unlink()
    report.append("removed the packaged-install metadata (AppxSignature.p7x, AppxBlockMap.xml, AppxMetadata, [Content_Types].xml) for -Register")

    # 3. import renames / redirect
    for fname, renames in RENAMES.items():
        path = target / fname
        pe = PE(bytearray(path.read_bytes()))
        have = {n.lower(): n for n in pe.import_names()}
        for old, new in renames:
            if old.lower() not in have:
                print("ERROR: %s does not import %s (found: %s)" % (fname, old, ", ".join(sorted(have)))); return 3
        if fname == "UnityPlayer.dll":
            d3d = [n for (_, n, _) in pe.import_descriptors() if n.lower() == "d3d11.dll"]
            funcs = {f.lower() for f in pe.imported_functions("d3d11.dll")} if hasattr(pe, "imported_functions") else SHIM_EXPORTS
            missing = funcs - SHIM_EXPORTS
            if missing:
                print("ERROR: UnityPlayer imports d3d11 functions the shim does not export: %s" % ", ".join(sorted(missing))); return 3
        for old, new in renames:
            hits = [(o, n, r) for (o, n, r) in pe.import_descriptors() if n.lower() == old.lower()]
            if len(hits) != 1:
                print("ERROR: %s: expected one import descriptor for %s, found %d" % (fname, old, len(hits))); return 3
            o = hits[0][0]
            new_rva = pe.add_string(new.encode("ascii"))
            struct.pack_into("<I", pe.d, o + 12, new_rva)   # IMAGE_IMPORT_DESCRIPTOR.Name
            struct.pack_into("<I", pe.d, o + 4, 0)          # TimeDateStamp: not bound
            report.append("%s: import %s -> %s (name RVA %08x)" % (fname, old, new, new_rva))
        path.write_bytes(bytes(pe.d))
        again = PE(bytearray(path.read_bytes()))
        now = [n.lower() for n in again.import_names()]
        for old, new in renames:
            assert old.lower() not in now and new.lower() in now, (fname, old, new, now)

    # 4. ship the shim as l9d3d11.dll
    shutil.copyfile(SHIM, target / SHIM_NAME)
    report.append("%s = l9shim.dll (%d bytes, sha256 %s)" % (SHIM_NAME, SHIM.stat().st_size, sha(SHIM.read_bytes())[:16]))

    # 5. app-local VC++ runtime
    for dll in VC_DLLS:
        src = REDIST / dll
        if not src.is_file():
            print("ERROR: redistributable %s not found in %s" % (dll, REDIST)); return 4
        shutil.copyfile(src, target / dll)
    report.append("VC143 ARM runtime shipped app-local: %s" % ", ".join(VC_DLLS))

    # 6. shader table
    (target / "Data" / "bp").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(TABLE, target / "Data" / "bp" / "l9_replacements.bin")
    report.append("Data\\bp\\l9_replacements.bin (%d bytes, sha256 %s)" % (TABLE.stat().st_size, sha(TABLE.read_bytes())[:16]))

    # 7. manifest: drop the VCLibs.140 PackageDependency (the runtime is now app-local)
    man = target / "AppxManifest.xml"
    text = man.read_text(encoding="utf-8")
    new_text = re.sub(r"[ \t]*<PackageDependency Name=\"Microsoft\.VCLibs\.140\.00\"[^>]*/>\s*\r?\n", "", text)
    if new_text == text:
        print("WARNING: the VCLibs.140 PackageDependency was not found in the manifest (already removed?)")
    man.write_text(new_text, encoding="utf-8")
    report.append("AppxManifest.xml: removed the Microsoft.VCLibs.140.00 PackageDependency")

    # 8. scripts + readme
    (target / "install.ps1").write_text(INSTALL_PS1, encoding="ascii")
    (target / "uninstall.ps1").write_text(UNINSTALL_PS1, encoding="ascii")
    (target / "install.bat").write_bytes(BAT.format("install").encode("ascii"))
    (target / "uninstall.bat").write_bytes(BAT.format("uninstall").encode("ascii"))
    (target / "README_DEVICE.txt").write_bytes(README.replace("\n", "\r\n").encode("ascii", "replace"))

    # 9. verification: no *_app or d3d11 import survives; a local/system import view
    report.append("")
    report.append("imports of the patched binaries (L = shipped in the package, S = expected from Windows):")
    local = {p.name.lower() for p in target.rglob("*") if p.is_file()}
    problems = 0
    for fname in ("BadPiggiesRebooted.exe", "UnityPlayer.dll", "GameAssembly.dll"):
        pe = PE(bytearray((target / fname).read_bytes()))
        names = pe.import_names()
        bad = [n for n in names if n.lower().endswith("_app.dll") or n.lower() == "d3d11.dll"]
        if bad:
            print("ERROR: %s still imports %s" % (fname, ", ".join(bad))); problems += 1
        parts = ["%s%s" % ("L:" if n.lower() in local else "S:", n) for n in names if not n.lower().startswith(("api-ms-", "ext-ms-"))]
        report.append("  %-26s %s" % (fname, " ".join(parts)))
    if problems:
        return 5

    # 10. MANIFEST + report
    (target / "PATCH_REPORT.txt").write_text("\r\n".join(report) + "\r\n", encoding="ascii", errors="replace")
    lines = ["Win32Craft Bad Piggies Rebooted ARM32 package - SHA-256 of every file", ""]
    for p in sorted(target.rglob("*")):
        if p.is_file():
            lines.append("  %s  %10d  %s" % (sha(p.read_bytes()), p.stat().st_size, p.relative_to(target).as_posix()))
    (target / "MANIFEST.txt").write_text("\r\n".join(lines) + "\r\n", encoding="ascii", errors="replace")

    # 11. zip
    zpath = OUT_ROOT / (FOLDER + ".zip")
    if zpath.exists():
        zpath.unlink()
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(target.rglob("*")):
            if p.is_file():
                z.write(p, Path(FOLDER) / p.relative_to(target))

    total = sum(p.stat().st_size for p in target.rglob("*") if p.is_file())
    print("\n".join(report))
    print("\nfolder: %s  (%.1f MB, %d files)" % (target, total / 1e6, sum(1 for p in target.rglob('*') if p.is_file())))
    print("zip:    %s  (%.1f MB)  sha256 %s" % (zpath, zpath.stat().st_size / 1e6, sha(zpath.read_bytes())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
