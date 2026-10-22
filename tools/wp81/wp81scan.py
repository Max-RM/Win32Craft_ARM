#!/usr/bin/env python3
"""wp81scan - recon of a Windows Phone 8.1 app package for running it on a Windows 10 ARM32 desktop (or Windows RT 8.1).

    python tools\\wp81\\wp81scan.py <extracted package folder or .appx> [--out report.md]

It automates the recon step of wp81/WP81_APP_METHOD.md (the method proven on the app "Sup", 2026-10-21) and writes a Markdown report:
  1. manifest: app model (OSMinVersion 6.3.1 = phone), PhoneIdentity, m3 elements, framework dependencies, capabilities, logos and their scales;
  2. every PE file: machine, imports (phone-only DLLs such as PhoneAppModelHost.dll), the VC++ runtime it needs;
  3. WinRT classes named in each binary (C++/CX and Qt name classes with inline UTF-16 strings): phone-only ones are listed with the functions that
     use them when the file is the main exe (via tools/arm32/arm32re.py), so they can be stubbed or guarded;
  4. compiled XAML (.xbf): version, Phone* theme resource keys (missing on a PC), phone-only / Windows-10-only controls (Pivot, ContentDialog ...);
  5. a verdict: a "game" (a SwapChainPanel/CoreWindow app with a handful of XAML objects -> the Win32Craft fake-object host can run it as Win32) or a
     full XAML app (-> the package route: patched manifest + registration as a packaged app).
Read-only: the input is never modified.  It does not decide patches for you; it lists what the method's steps have to handle."""
import argparse, os, re, struct, subprocess, sys, tempfile, zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
ARM32RE = REPO / "tools" / "arm32" / "arm32re.py"

PHONE_ONLY_DLLS = {"phoneappmodelhost.dll": "Windows Phone app-model host (LoadPackagedLibrary etc.) - redirect the import (KERNELBASE.dll exports LoadPackagedLibrary)"}
PHONE_ONLY_CLASS_PREFIXES = ("Windows.Phone.", "Windows.ApplicationModel.Calls.", "Windows.UI.ViewManagement.StatusBar",
                             "Windows.ApplicationModel.Activation.FileOpenPickerContinuation", "Windows.ApplicationModel.Activation.FileSavePickerContinuation",
                             "Windows.ApplicationModel.Activation.FolderPickerContinuation", "Windows.ApplicationModel.Activation.WebAuthenticationBrokerContinuation",
                             "Windows.Phone.Management", "Windows.ApplicationModel.Wallet", "Windows.Devices.Sms")
# types that Windows 8.1 PC XAML lacks but Windows Phone 8.1 and Windows 10 have (-> prefer the Windows 10 manifest)
WIN10_OR_PHONE_XAML = ("Pivot", "PivotItem", "ContentDialog", "PickerFlyout", "ListPickerFlyout", "DatePickerFlyout", "TimePickerFlyout",
                       "AutoSuggestBox", "MediaTransportControls")
GAME_HINTS = ("Windows.UI.Xaml.Controls.SwapChainPanel", "Windows.UI.Xaml.Controls.SwapChainBackgroundPanel", "Windows.UI.Core.CoreIndependentInputSource")


def u16(d, o): return struct.unpack_from("<H", d, o)[0]
def u32(d, o): return struct.unpack_from("<I", d, o)[0]


def pe_info(data):
    """(machine, [import dll names]) or None for a non-PE file"""
    if len(data) < 0x40 or data[:2] != b"MZ": return None
    e = u32(data, 0x3C)
    if data[e:e + 4] != b"PE\0\0": return None
    machine = u16(data, e + 4)
    nsec = u16(data, e + 6); opt = e + 24; opt_size = u16(data, e + 20)
    magic = u16(data, opt)
    dd = opt + (96 if magic == 0x10B else 112)
    imp_rva = u32(data, dd + 8)
    secs = []
    for i in range(nsec):
        o = opt + opt_size + 40 * i
        vs, va, rs, rp = struct.unpack_from("<IIII", data, o + 8)
        secs.append((va, max(vs, rs), rp))
    def off(rva):
        for va, size, rp in secs:
            if va <= rva < va + size: return rp + rva - va
        return None
    names = []
    o = off(imp_rva) if imp_rva else None
    while o is not None and o + 20 <= len(data):
        name_rva, ft = u32(data, o + 12), u32(data, o + 16)
        if not name_rva and not ft: break
        no = off(name_rva)
        if no is not None:
            names.append(data[no:data.index(b"\0", no)].decode("ascii", "replace"))
        o += 20
    return machine, names


def winrt_names(data):
    out = set()
    for m in re.finditer(rb"W\x00i\x00n\x00d\x00o\x00w\x00s\x00\.\x00(?:[A-Za-z0-9_\.]\x00){3,100}", data):
        out.add(m.group().decode("utf-16le"))
    for m in re.finditer(rb"Windows\.[A-Za-z0-9_\.]{3,100}", data):
        out.add(m.group().decode("ascii"))
    return out


def xbf_strings(data):
    return set(m.group().decode("utf-16le") for m in re.finditer(rb"(?:[A-Za-z0-9_.:]\x00){3,80}", data))


def users_of(exe: Path, cls: str):
    """functions of the main exe that reference the class name (arm32re strx -w); '' when the helper is unavailable"""
    if not ARM32RE.exists(): return ""
    env = dict(os.environ, ARM32_EXE=str(exe))
    try:
        r = subprocess.run([sys.executable, str(ARM32RE), "strx", "-w", cls], capture_output=True, text=True, timeout=600, env=env)
    except Exception:
        return ""
    funcs = sorted(set(re.findall(r"in (sub_[0-9a-f]+)", r.stdout)))
    return ", ".join(funcs)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("package", help="extracted package folder (with AppxManifest.xml) or an .appx file")
    ap.add_argument("--out", help="write the report here (default: print)")
    ap.add_argument("--no-re", action="store_true", help="do not run arm32re to find the functions that use phone-only classes")
    args = ap.parse_args()
    src = Path(args.package)
    tmp = None
    if src.is_file() and src.suffix.lower() in (".appx", ".zip"):
        tmp = tempfile.mkdtemp(prefix="wp81scan_")
        with zipfile.ZipFile(src) as z: z.extractall(tmp)
        src = Path(tmp)
    manifest_path = src / "AppxManifest.xml"
    if not manifest_path.exists():
        print("ERROR: no AppxManifest.xml in %s" % src); return 2
    man = manifest_path.read_text(encoding="utf-8-sig", errors="replace")
    rep = ["# wp81scan report: %s" % args.package, ""]

    # 1. manifest
    osmin = re.search(r"<OSMinVersion>([^<]+)</OSMinVersion>", man)
    ident = re.search(r"<Identity([^>]*)/>", man)
    app = re.search(r"<Application ([^>]*)>", man)
    deps = re.findall(r'<PackageDependency Name="([^"]+)"[^>]*MinVersion="([^"]+)"', man)
    caps = re.findall(r'<(?:\w+:)?(?:Device)?Capability Name="([^"]+)"', man)
    logos = sorted(set(re.findall(r'(?:Logo|Image)="([^"]+)"', man)))
    rep += ["## 1. Manifest", ""]
    rep.append("* Identity: `%s`" % (ident.group(1).strip() if ident else "?"))
    rep.append("* Application: `%s`" % (app.group(1).strip() if app else "?"))
    rep.append("* OSMinVersion: `%s` -> %s" % (osmin.group(1) if osmin else "none", "WINDOWS PHONE app model (a PC refuses it: 0x80073CFD) - write a new manifest"
               if osmin and osmin.group(1).strip().endswith(".1") else "PC app model"))
    rep.append("* PhoneIdentity: %s; m3 (2014, phone) elements: %s" % ("yes" if "PhoneIdentity" in man else "no", "yes" if "m3:" in man else "no"))
    rep.append("* Framework dependencies: %s" % (", ".join("%s >= %s" % d for d in deps) or "none"))
    rep.append("* Capabilities: %s" % (", ".join(caps) or "none"))
    rep.append("* Images in the manifest: %s" % ", ".join(logos))
    assets = sorted(p.name for p in (src / "Assets").glob("*") if p.is_file()) if (src / "Assets").exists() else []
    scales = sorted(set(re.findall(r"\.scale-(\d+)\.", " ".join(assets))))
    rep.append("* Asset scales present: %s%s" % (", ".join(scales) or "unqualified only", " (phone-only scale: add unqualified copies)" if scales == ["240"] else ""))
    rep.append("")

    # 2./3. binaries
    rep += ["## 2. Binaries and imports", "", "| file | machine | imports of note | phone-only classes named |", "|---|---|---|---|"]
    exe_name = None
    m = re.search(r'Executable="([^"]+)"', man)
    if m: exe_name = m.group(1)
    all_names, phone_classes_by_file, vc, code_keys = set(), {}, set(), {}
    for p in sorted(src.rglob("*")):
        if not p.is_file() or p.suffix.lower() not in (".exe", ".dll", ".winmd"): continue
        data = p.read_bytes()
        info = pe_info(data)
        if not info: continue
        machine, imports = info
        notes = []
        for name in imports:
            low = name.lower()
            if low in PHONE_ONLY_DLLS: notes.append("**%s**" % name)
            if re.match(r"(msvcp|msvcr|vccorlib)1[0-9]0_app\.dll", low): vc.add(low)
            if low.startswith(("qt5", "qt6")) or low in ("ws2_32.dll", "d3d11.dll", "dxgi.dll"): notes.append(name)
        names = winrt_names(data)
        all_names |= names
        for k in re.findall(rb"P\x00h\x00o\x00n\x00e\x00(?:[A-Za-z]\x00){4,40}", data):
            key = k.decode("utf-16le")
            if key.endswith(("Brush", "Color", "Style", "FontFamily", "FontSize")): code_keys.setdefault(key, set()).add(p.name)
        phone = sorted(n for n in names if n.startswith(PHONE_ONLY_CLASS_PREFIXES))
        if phone: phone_classes_by_file[p] = phone
        rep.append("| %s | 0x%X | %s | %s |" % (p.relative_to(src).as_posix(), machine, ", ".join(notes) or "-", ", ".join(phone) or "-"))
    if code_keys:
        rep += ["", "Phone theme keys looked up BY NAME in code (Application.Resources lookups throw when the key is missing): " +
                ", ".join("`%s` (%s)" % (k, ", ".join(sorted(v))) for k, v in sorted(code_keys.items()))]
    rep += ["", "VC++ runtime DLLs imported: %s (ship them app-local from the matching VCLibs .Phone appx, or depend on the PC framework)" % (", ".join(sorted(vc)) or "none"), ""]

    if phone_classes_by_file:
        rep += ["## 3. Phone-only WinRT classes (each one throws ClassNotRegistered on a PC when activated)", ""]
        for p, classes in phone_classes_by_file.items():
            for c in classes:
                users = "" if args.no_re or p.name != exe_name else users_of(p, c)
                rep.append("* `%s` in %s%s" % (c, p.name, (" - used by " + users) if users else ""))
        rep += ["", "For each user: check what calls it and its return convention, then stub it (`bx lr` keeps r0) or guard it; start-up users first.", ""]

    # 4. XAML
    xbfs = sorted(src.rglob("*.xbf"))
    rep += ["## 4. Compiled XAML", ""]
    if xbfs:
        phone_keys, win10_types, versions = {}, {}, set()
        for x in xbfs:
            d = x.read_bytes()
            if d[:4] == b"XBF\0": versions.add("%d.%d" % (u32(d, 12), u32(d, 16)))
            s = xbf_strings(d)
            for k in s:
                if k.startswith("Phone") and len(k) > 6: phone_keys.setdefault(k, []).append(x.name)
                if k in WIN10_OR_PHONE_XAML: win10_types.setdefault(k, []).append(x.name)
        rep.append("* %d XBF files, version(s) %s" % (len(xbfs), ", ".join(sorted(versions))))
        rep.append("* Phone* resource keys (define the missing ones in the app's own merged dictionary; Windows 10 has PhoneFontFamilyNormal/SemiLight only):")
        for k in sorted(phone_keys): rep.append("  * `%s`: %s" % (k, ", ".join(sorted(set(phone_keys[k])))))
        rep.append("* Controls missing from Windows 8.1 PC XAML (fine with a Windows 10 manifest): %s" % (
            ", ".join("%s (%s)" % (k, ", ".join(sorted(set(v)))) for k, v in sorted(win10_types.items())) or "none"))
    else:
        rep.append("* no .xbf files")
    rep.append("")

    # 5. verdict
    xaml_classes = sorted(n for n in all_names if n.startswith("Windows.UI.Xaml.") and not n.endswith("Handler"))
    is_game = any(h in all_names for h in GAME_HINTS) and len(xaml_classes) < 40
    rep += ["## 5. Verdict", ""]
    rep.append("* Windows.UI.Xaml classes named: %d; SwapChainPanel / CoreIndependentInputSource: %s" % (
        len(xaml_classes), "yes" if any(h in all_names for h in GAME_HINTS) else "no"))
    rep.append("* Route: **%s**" % ("Win32Craft fake-object host (a game: few XAML objects, draws with Direct3D) - see arm32/v0_10_4" if is_game
                                    else "package route (full XAML app needs the OS XAML and package identity) - wp81/WP81_APP_METHOD.md"))
    text = "\n".join(rep) + "\n"
    if args.out:
        Path(args.out).write_text(text, encoding="utf-8"); print("report: %s" % args.out)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
