#!/usr/bin/env python3
"""Make a desktop-runnable (registerable) package of the Windows Phone 8.1 app "Sup" (unofficial WhatsApp client, WhatsApp.Qt_0.2.0.8_ARM).

    python tools\\whatsapp\\make_whatsapp_package.py [--manifest win10|win81] [--src DIR] [--out DIR] [--vclibs APPX] [--no-zip]

Why this is NOT a Win32Craft host job: Sup is a full XAML app (Frame/Page/ListView/ContentDialog/Flyouts/Storyboards/RichTextBlock ... - see
whatsapp/README_WHATSAPP.md), so it needs the real Windows.UI.Xaml of the OS.  The Win32Craft fake-object host works for Minecraft only because the
game uses a handful of XAML objects.  This script instead patches the PACKAGE so that Windows 10 (ARM32 desktop, e.g. Surface 2 build 15035) runs it
as a real packaged app, registered from the folder with Add-AppxPackage -Register (Developer Mode):

  1. AppxManifest.xml REPLACED (the phone one has OSMinVersion 6.3.1 = the phone app model, which a PC refuses with 0x80073CFD): --manifest win10
     (default) writes a Windows 10 manifest, --manifest win81 a Windows 8.1 PC one (for RT 8.1); unqualified copies of the six logos are added;
     package-only files (AppxBlockMap.xml, AppxSignature.p7x, [Content_Types].xml, AppxMetadata) are not copied.
  2. VC120 runtime: msvcp120_app.dll / msvcr120_app.dll / vccorlib120_app.dll of Microsoft.VCLibs.120.00.Phone 12.0.30113.0 are copied next to the exe
     (the framework package of the phone is not installable on a PC).
  3. PE import patch (WhatsApp.exe and Qt5Core.dll): the import of LoadPackagedLibrary from PhoneAppModelHost.dll (a Windows-Phone-only DLL) is
     redirected to KERNELBASE.dll (same function name, hint kept): only the DLL name of the import descriptor changes.
  4. Four Windows-Phone-only wrappers in WhatsApp.exe get 'bx lr' (they would throw ClassNotRegistered on a PC during start-up):
       0x5E694 HardwareButtons.BackPressed += handler   0x82A08 / 0x82CA8 StatusBar colour / show-hide   0x5E930 PhoneCallManager.ShowPhoneCallUI
  5. themes/PhoneTheme.xbf: the phone theme brushes PhoneAccentBrush / PhoneForegroundBrush / PhoneChromeBrush are added (a PC has none of them;
     six pages and the AccentWhenTrue converter need them) - win10: {ThemeResource SystemAccentColor / SystemBaseHighColor / SystemChromeMediumColor},
     win81: fixed colours.  See PHONE_BRUSHES / xbf_add_phone_brushes.
  wad.exe (a Windows 10 Mobile background-service helper) and the AppX signature files are left out.

The source folder is only READ.  Output (default dist-whatsapp\\): the package folder, its zip, PATCH_REPORT.txt inside the folder."""
import argparse, hashlib, os, re, struct, sys, zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
DEFAULT_SRC = Path(os.environ.get("WIN32CRAFT_WHATSAPP_DIR", str(REPO / "WhatsApp.Qt_0.2.0.8_ARM")))
DEFAULT_VCLIBS = Path(os.environ.get("WIN32CRAFT_VCLIBS120_PHONE",
                                     r"D:\repo\armdeps\Microsoft.VCLibs.120.00.Phone_12.0.30113.0_arm__8wekyb3d8bbwe.Appx"))
PACKAGE_NAME = "Sup_WhatsApp_0.2.0.8_ARM_Win10"
SKIP_FILES = {"appxblockmap.xml", "appxsignature.p7x", "[content_types].xml", "wad.exe"}
SKIP_DIRS = {"appxmetadata"}
VC_DLLS = ["msvcp120_app.dll", "msvcr120_app.dll", "vccorlib120_app.dll"]

# (file, rva, expected bytes, new bytes, what).  Thumb-2: the first halfword becomes 'bx lr' (0x4770); the rest of the function is never reached.
BX_LR = bytes.fromhex("7047")
EXE_PATCHES = [
    ("WhatsApp.exe", 0x5E694, bytes.fromhex("2de9b048"), BX_LR, "HardwareButtons.BackPressed += handler (Windows.Phone.UI.Input) - returns its first argument like the original"),
    ("WhatsApp.exe", 0x82A08, bytes.fromhex("2de9f049"), BX_LR, "set StatusBar colour (Windows.UI.ViewManagement.StatusBar, phone only) - void"),
    ("WhatsApp.exe", 0x82CA8, bytes.fromhex("2de9b048"), BX_LR, "StatusBar show / hide (phone only) - void"),
    ("WhatsApp.exe", 0x5E930, bytes.fromhex("2de9b048"), BX_LR, "PhoneCallManager.ShowPhoneCallUI (Windows.ApplicationModel.Calls, phone only) - void"),
]
IMPORT_REDIRECTS = [("WhatsApp.exe", "PhoneAppModelHost.dll", "KERNELBASE.dll"), ("Qt5Core.dll", "PhoneAppModelHost.dll", "KERNELBASE.dll")]


def u16(d, o): return struct.unpack_from("<H", d, o)[0]
def u32(d, o): return struct.unpack_from("<I", d, o)[0]


class PE:
    def __init__(self, data: bytearray):
        self.d = data
        e = u32(data, 0x3C)
        if data[e:e + 4] != b"PE\0\0": raise ValueError("not a PE file")
        self.machine = u16(data, e + 4)
        nsec = u16(data, e + 6)
        opt_size = u16(data, e + 20)
        self.opt = e + 24
        if u16(data, self.opt) != 0x10B: raise ValueError("not PE32")
        self.sec_table = self.opt + opt_size
        self.sections = []
        for i in range(nsec):
            o = self.sec_table + 40 * i
            name = bytes(data[o:o + 8]).rstrip(b"\0").decode("ascii", "replace")
            vsize, va, rsize, rptr = struct.unpack_from("<IIII", data, o + 8)
            self.sections.append(dict(name=name, vsize=vsize, va=va, rsize=rsize, rptr=rptr, hdr=o))
        self.section_align = u32(data, self.opt + 32)
        self.import_rva, self.import_size = struct.unpack_from("<II", data, self.opt + 96 + 8)

    def sec_of_rva(self, rva):
        for s in self.sections:
            if s["va"] <= rva < s["va"] + max(s["vsize"], s["rsize"]): return s
        raise ValueError("RVA %08x is in no section" % rva)

    def off(self, rva):
        s = self.sec_of_rva(rva)
        return s["rptr"] + rva - s["va"]

    def cstr(self, rva):
        o = self.off(rva)
        end = self.d.index(b"\0", o)
        return bytes(self.d[o:end]).decode("ascii", "replace")

    def import_descriptors(self):
        """[(descriptor file offset, dll name, name rva)]"""
        out = []
        o = self.off(self.import_rva)
        while True:
            oft, tds, fwd, name, ft = struct.unpack_from("<IIIII", self.d, o)
            if not name and not ft: break
            out.append((o, self.cstr(name), name))
            o += 20
        return out

    def add_string(self, text: bytes) -> int:
        """append a NUL-terminated string to the slack of the section that holds the import table; returns its RVA"""
        s = self.sec_of_rva(self.import_rva)
        start = (s["vsize"] + 1) & ~1
        need = start + len(text) + 1
        if need > s["rsize"]: raise ValueError("no slack in section %s for %r" % (s["name"], text))
        aligned_footprint = (s["vsize"] + self.section_align - 1) & ~(self.section_align - 1)
        if need > aligned_footprint: raise ValueError("section %s would grow into the next page" % s["name"])
        o = s["rptr"] + start
        if any(self.d[o:o + len(text) + 1]): raise ValueError("slack of section %s is not zero" % s["name"])
        self.d[o:o + len(text)] = text
        self.d[o + len(text)] = 0
        struct.pack_into("<I", self.d, s["hdr"] + 8, need)       # VirtualSize
        s["vsize"] = need
        return s["va"] + start

    def import_names(self):
        return [n for (_, n, _) in self.import_descriptors()]


def sha(data) -> str: return hashlib.sha256(bytes(data)).hexdigest()


# The source manifest is a Windows PHONE 8.1 one: OSMinVersion 6.3.1 = app model 1 (phone), m3 (2014) elements.  A Windows 10 desktop refuses it
# (Surface 2 diag: 0x80073CFD "requested Windows version 6.3 with AppModel version 1, while the current ... AppModel version 0").  Complete
# replacement manifests instead of regex edits:
#   win10 (default): a Windows 10 manifest - the OS then uses its Windows 10 XAML styles, which have the Pivot / ContentDialog / CommandBar
#                    templates Sup uses (the Windows 8.1 style set Windows.UI.Xaml.Resources.win81.dll has no Pivot / ContentDialog);
#   win81:           a Windows 8.1 PC manifest (OSMinVersion 6.3.0, m2 elements: Square30x30Logo / Square70x70Logo) - the form Windows RT 8.1 needs.
# Both texts pass makeappx 10.0.19041 'pack' validation (uap:VisualElements has no ForegroundText / ToastCapable - Windows 10 dropped them;
# an investigation claimed the opposite, makeappx refused it).  Re-check with makeappx after any change.
MANIFEST_WIN10 = """<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10" xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10" IgnorableNamespaces="uap">
  <Identity Name="ru.computershik.sup" Publisher="CN=Computershik" Version="0.2.0.8" ProcessorArchitecture="arm" />
  <Properties>
    <DisplayName>Sup</DisplayName>
    <PublisherDisplayName>Computershik</PublisherDisplayName>
    <Logo>Assets\\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Universal" MinVersion="10.0.10240.0" MaxVersionTested="10.0.10240.0" />
  </Dependencies>
  <Resources>
    <Resource Language="RU-RU" />
  </Resources>
  <Applications>
    <Application Id="App" Executable="WhatsApp.exe" EntryPoint="WhatsApp.App">
      <uap:VisualElements DisplayName="Sup" Square150x150Logo="Assets\\Logo.png" Square44x44Logo="Assets\\SmallLogo.png" Description="Неофициальный клиент WhatsApp" BackgroundColor="transparent">
        <uap:DefaultTile Wide310x150Logo="Assets\\WideLogo.png" Square71x71Logo="Assets\\Square71x71Logo.png" ShortName="Sup">
          <uap:ShowNameOnTiles>
            <uap:ShowOn Tile="square150x150Logo" />
            <uap:ShowOn Tile="wide310x150Logo" />
          </uap:ShowNameOnTiles>
        </uap:DefaultTile>
        <uap:SplashScreen Image="Assets\\SplashScreen.png" BackgroundColor="#075E54" />
      </uap:VisualElements>
    </Application>
  </Applications>
  <Capabilities>
    <Capability Name="internetClientServer" />
    <uap:Capability Name="picturesLibrary" />
    <uap:Capability Name="videosLibrary" />
    <DeviceCapability Name="microphone" />
  </Capabilities>
</Package>
"""
MANIFEST_WIN81 = """<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/2010/manifest" xmlns:m2="http://schemas.microsoft.com/appx/2013/manifest">
  <Identity Name="ru.computershik.sup" Publisher="CN=Computershik" Version="0.2.0.8" ProcessorArchitecture="arm" />
  <Properties>
    <DisplayName>Sup</DisplayName>
    <PublisherDisplayName>Computershik</PublisherDisplayName>
    <Logo>Assets\\StoreLogo.png</Logo>
  </Properties>
  <Prerequisites>
    <OSMinVersion>6.3.0</OSMinVersion>
    <OSMaxVersionTested>6.3.0</OSMaxVersionTested>
  </Prerequisites>
  <Resources>
    <Resource Language="RU-RU" />
  </Resources>
  <Applications>
    <Application Id="App" Executable="WhatsApp.exe" EntryPoint="WhatsApp.App">
      <m2:VisualElements DisplayName="Sup" Square150x150Logo="Assets\\Logo.png" Square30x30Logo="Assets\\SmallLogo.png" Description="Неофициальный клиент WhatsApp" ForegroundText="light" BackgroundColor="transparent" ToastCapable="true">
        <m2:DefaultTile Wide310x150Logo="Assets\\WideLogo.png" Square70x70Logo="Assets\\Square71x71Logo.png" ShortName="Sup">
          <m2:ShowNameOnTiles>
            <m2:ShowOn Tile="square150x150Logo" />
            <m2:ShowOn Tile="wide310x150Logo" />
          </m2:ShowNameOnTiles>
        </m2:DefaultTile>
        <m2:SplashScreen Image="Assets\\SplashScreen.png" BackgroundColor="#075E54" />
      </m2:VisualElements>
    </Application>
  </Applications>
  <Capabilities>
    <Capability Name="internetClientServer" />
    <Capability Name="picturesLibrary" />
    <Capability Name="videosLibrary" />
    <DeviceCapability Name="microphone" />
  </Capabilities>
</Package>
"""
MANIFESTS = {"win10": MANIFEST_WIN10, "win81": MANIFEST_WIN81}
LOGOS = ["Logo", "SmallLogo", "SplashScreen", "Square71x71Logo", "StoreLogo", "WideLogo"]

# ---- themes/PhoneTheme.xbf: the Windows Phone theme brushes Sup uses but a PC does not have -----------------------------------------------------
# Six pages use {ThemeResource PhoneAccentBrush}, ChatPage {ThemeResource PhoneChromeBrush}, and WhatsApp.exe looks up PhoneAccentBrush and
# PhoneForegroundBrush in Application.Resources (AccentWhenTrue converter, sub_1779c -> IMap::Lookup, throws when missing).  A phone supplies them;
# no Windows desktop XAML theme does (Windows 10 SDK themeresources, Windows.UI.Xaml.Resources*.dll: 0 hits).  The app merges themes/PhoneTheme.xaml
# into Application.Resources before any page loads (OnLaunched -> sub_41a8), so the three brushes are added THERE.
# XBF v1 node stream (checked on this file): a plain token sequence without offsets or child counts -
#   0x0A LineInfo (2 x int16 deltas) | 0x01 StartObject (u32 type, u32 flags) | 0x03 StartMember (u32 property, u32 flags) |
#   0x06 Value (u8 kind: 0x05 = u32 length + UTF-16 text, 0x0A = colour as little-endian 0xAARRGGBB) | 0x04 EndMember | 0x02 EndObject
# and the header dword at 0x08 is the node-section length (file = 12-byte prefix + metadata (dword 0x04) + nodes).  The new elements are built from
# the existing SolidColorBrush element 'AvatarPlaceholderBrush' (same LineInfo tokens) and inserted right after it.
# win10: each brush's Color is {ThemeResource <Windows 10 colour>} so it follows the light / dark mode like the phone brushes did
#        (SystemAccentColor, SystemBaseHighColor = white/black, SystemChromeMediumColor = #1F1F1F/#E6E6E6 - present in the oldest Windows 10 theme 'th'
#        and in 'rs2'); win81: fixed colours (the Windows 8.1 style set has none of those keys; 8.1 apps default to the dark theme).
PHONE_BRUSHES = {
    "win10": [("PhoneAccentBrush", "SystemAccentColor"), ("PhoneForegroundBrush", "SystemBaseHighColor"), ("PhoneChromeBrush", "SystemChromeMediumColor")],
    "win81": [("PhoneAccentBrush", 0xFF25D366), ("PhoneForegroundBrush", 0xFFFFFFFF), ("PhoneChromeBrush", 0xFF1F1F1F)],
}
XBF_TEMPLATE_KEY = "AvatarPlaceholderBrush"


def xbf_tables(d):
    """XBF v1 metadata tables: strings, types [(flags, namespace, name)], properties [(flags, type index, name)], node-section start."""
    if d[:4] != b"XBF\0" or u32(d, 0x0C) != 1:
        raise ValueError("not an XBF version 1 file")
    offs = [u32(d, 0x14 + 8 * i) + 0x0C for i in range(6)]
    n = u32(d, offs[0]); p = offs[0] + 4; strings = []
    for _ in range(n):
        k = u32(d, p); p += 4
        strings.append(bytes(d[p:p + 2 * k]).decode("utf-16le")); p += 2 * k
    if p != offs[1]:
        raise ValueError("XBF string table does not end where the assembly table starts")
    n = u32(d, offs[2]); namespaces = [strings[u32(d, offs[2] + 8 + 8 * i)] for i in range(n)]
    n = u32(d, offs[3]); types = []
    for i in range(n):
        fl, ns, nm = struct.unpack_from("<3I", d, offs[3] + 4 + 12 * i)
        types.append((fl, namespaces[ns], strings[nm]))
    n = u32(d, offs[4]); props = []
    for i in range(n):
        fl, ty, nm = struct.unpack_from("<3I", d, offs[4] + 4 + 12 * i)
        props.append((fl, ty, strings[nm]))
    nodes = 0x0C + u32(d, 0x04)
    if nodes + u32(d, 0x08) != len(d):
        raise ValueError("XBF header lengths do not add up to the file size")
    return strings, types, props, nodes


def xbf_tokens(d, start):
    """walk ONE element from 'start' (a LineInfo or StartObject) to its EndObject; returns [(kind, payload bytes)] and the end offset.
    Only the token kinds a simple keyed object uses are accepted - anything else stops the patch."""
    out, p, depth = [], start, 0
    while True:
        t = d[p]
        if t == 0x0A: out.append((t, bytes(d[p + 1:p + 5]))); p += 5
        elif t in (0x01, 0x03):
            out.append((t, bytes(d[p + 1:p + 9]))); p += 9
            if t == 0x01: depth += 1
        elif t == 0x06:
            kind = d[p + 1]
            if kind == 0x05:
                n = u32(d, p + 2); out.append((t, bytes(d[p + 1:p + 6 + 2 * n]))); p += 6 + 2 * n
            elif kind == 0x0A:
                out.append((t, bytes(d[p + 1:p + 6]))); p += 6
            else:
                raise ValueError("XBF value kind 0x%02x at 0x%x is not handled" % (kind, p))
        elif t == 0x04: out.append((t, b"")); p += 1
        elif t == 0x02:
            out.append((t, b"")); p += 1; depth -= 1
            if depth == 0: return out, p
        else:
            raise ValueError("XBF token 0x%02x at 0x%x is not handled" % (t, p))


def xbf_add_phone_brushes(data: bytes, flavour: str):
    d = bytearray(data)
    strings, types, props, nodes = xbf_tables(d)
    find_type = lambda name: [i for i, t in enumerate(types) if t[2] == name]
    find_prop = lambda name, ty: [i for i, p in enumerate(props) if p[2] == name and (ty is None or p[1] == ty)]
    t_brush, t_theme = find_type("SolidColorBrush"), find_type("ThemeResource")
    if len(t_brush) != 1 or len(t_theme) != 1:
        raise ValueError("PhoneTheme.xbf: SolidColorBrush / ThemeResource type not found exactly once")
    t_brush, t_theme = t_brush[0], t_theme[0]
    p_key = [i for i in find_prop("Key", None) if props[i][0] == 2]
    p_color, p_reskey = find_prop("Color", t_brush), find_prop("ResourceKey", t_theme)
    if len(p_key) != 1 or len(p_color) != 1 or len(p_reskey) != 1:
        raise ValueError("PhoneTheme.xbf: x:Key / SolidColorBrush.Color / ThemeResource.ResourceKey not found exactly once")
    p_key, p_color, p_reskey = p_key[0], p_color[0], p_reskey[0]
    for name, _ in PHONE_BRUSHES[flavour]:
        if name.encode("utf-16le") in d[nodes:]:
            raise ValueError("PhoneTheme.xbf already contains %s" % name)
    # the template element: the keyed SolidColorBrush 'AvatarPlaceholderBrush' (LineInfo, StartObject, LineInfo, x:Key, LineInfo, Color, EndObject)
    key_value = b"\x06\x05" + struct.pack("<I", len(XBF_TEMPLATE_KEY)) + XBF_TEMPLATE_KEY.encode("utf-16le")
    kv = d.find(key_value, nodes)
    start = d.rfind(b"\x01" + struct.pack("<II", t_brush, 0), nodes, kv) - 5
    if kv < 0 or start < nodes or d[start] != 0x0A:
        raise ValueError("PhoneTheme.xbf: the %s element was not found" % XBF_TEMPLATE_KEY)
    toks, end = xbf_tokens(d, start)
    shape = [k for k, _ in toks]
    if shape != [0x0A, 0x01, 0x0A, 0x03, 0x06, 0x04, 0x0A, 0x03, 0x06, 0x04, 0x02] or \
            toks[1][1] != struct.pack("<II", t_brush, 0) or toks[3][1] != struct.pack("<II", p_key, 0) or toks[7][1] != struct.pack("<II", p_color, 0):
        raise ValueError("PhoneTheme.xbf: the %s element does not have the expected shape: %s" % (XBF_TEMPLATE_KEY, shape))
    li_first, li_key, li_color = toks[0][1], toks[2][1], toks[6][1]
    so = lambda t: b"\x01" + struct.pack("<II", t, 0)
    sm = lambda p: b"\x03" + struct.pack("<II", p, 0)
    vstr = lambda s: b"\x06\x05" + struct.pack("<I", len(s)) + s.encode("utf-16le")
    new = b""
    for name, value in PHONE_BRUSHES[flavour]:
        if isinstance(value, str):
            color = so(t_theme) + sm(p_reskey) + vstr(value) + b"\x04\x02"          # {ThemeResource value}
        else:
            color = b"\x06\x0a" + struct.pack("<I", value)
        new += b"\x0a" + li_first + so(t_brush) + b"\x0a" + li_key + sm(p_key) + vstr(name) + b"\x04" + b"\x0a" + li_color + sm(p_color) + color + b"\x04\x02"
    d[end:end] = new
    struct.pack_into("<I", d, 0x08, u32(d, 0x08) + len(new))
    # verify: lengths add up again and the three new elements walk cleanly with the right keys and values
    xbf_tables(d)
    p, seen = end, []
    for name, value in PHONE_BRUSHES[flavour]:
        toks, p = xbf_tokens(d, p)
        keys = [pl[5:].decode("utf-16le") for k, pl in toks if k == 0x06 and pl[:1] == b"\x05"]
        if keys[0] != name or (isinstance(value, str) and keys[1:] != [value]):
            raise ValueError("PhoneTheme.xbf: verification of the inserted %s failed: %s" % (name, keys))
        seen.append(name)
    if p != end + len(new):
        raise ValueError("PhoneTheme.xbf: the inserted elements do not end where expected")
    return bytes(d), end, len(new), seen


INSTALL_PS1 = r'''# Registers Sup (WhatsApp client) from THIS folder.  Needs Developer Mode: Settings > Update & security > For developers > Developer mode.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$name = 'ru.computershik.sup'
$old = Get-AppxPackage -Name $name
if ($old) { Write-Host 'Removing the previous registration ...'; $old | Remove-AppxPackage }
Write-Host ('Registering ' + $here + '\AppxManifest.xml ...')
Add-AppxPackage -Register (Join-Path $here 'AppxManifest.xml')
Get-AppxPackage -Name $name | Format-List Name, Version, Architecture, PackageFullName, InstallLocation, Status
Write-Host 'Done: start "Sup" from the Start menu (All apps).  If it closes at once, run diag.bat and send diag.txt.'
'''
UNINSTALL_PS1 = r'''$ErrorActionPreference = 'Stop'
Get-AppxPackage -Name 'ru.computershik.sup' | Remove-AppxPackage
Write-Host 'Removed (the folder itself stays).'
'''
DIAG_PS1 = r'''# Collects what Windows says about the app: registration, deployment errors, activation / crash events.  Writes diag.txt next to this script.
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here 'diag.txt'
function Section($title, [scriptblock]$body) {
    Add-Content -Path $out -Value ("`r`n==== " + $title + " ====")
    try { $r = & $body | Out-String -Width 220; Add-Content -Path $out -Value $r } catch { Add-Content -Path $out -Value ('(failed: ' + $_.Exception.Message + ')') }
}
Set-Content -Path $out -Value ('diag ' + (Get-Date) + '  ' + [Environment]::OSVersion.VersionString)
Section 'Get-AppxPackage' { Get-AppxPackage -Name 'ru.computershik.sup' | Format-List * }
Section 'AppXDeployment-Server (last 40)' { Get-WinEvent -LogName 'Microsoft-Windows-AppXDeploymentServer/Operational' -MaxEvents 40 | Format-List TimeCreated, Id, LevelDisplayName, Message }
Section 'TWinUI (last 40)' { Get-WinEvent -LogName 'Microsoft-Windows-TWinUI/Operational' -MaxEvents 40 | Format-List TimeCreated, Id, LevelDisplayName, Message }
Section 'AppModel-Runtime Admin (last 40)' { Get-WinEvent -LogName 'Microsoft-Windows-AppModel-Runtime/Admin' -MaxEvents 40 | Format-List TimeCreated, Id, LevelDisplayName, Message }
Section 'Application log: WhatsApp.exe / ru.computershik.sup (last 20)' {
    Get-WinEvent -LogName 'Application' -MaxEvents 400 | Where-Object { $_.Message -match 'WhatsApp\.exe|ru\.computershik\.sup|Sup_' } | Select-Object -First 20 | Format-List TimeCreated, Id, ProviderName, Message }
Write-Host ('Written: ' + $out)
'''
BAT = '@echo off\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0{0}.ps1"\r\npause\r\n'

README_DEVICE = """Sup (unofficial WhatsApp client, Windows Phone 8.1 build 0.2.0.8, ARM32) patched to run as a REAL packaged app on a Windows 10 ARM32 desktop (Surface 2, build 15035)
==============================================================================================================================================================

Install (once)
  1. Settings > Update & security > For developers > turn on "Developer mode" (needed to register an app from a folder).
  2. Copy this whole folder to the device (any local path, e.g. C:\\Sup) and run install.bat.  The app "Sup" appears in Start > All apps.
  3. If it does not start or closes at once: run diag.bat, then send diag.txt (and tell what you saw on screen).
  Remove: uninstall.bat.  Do not move the folder after installing (the registration points to it); run install.bat again after moving.

What was changed (PATCH_REPORT.txt has the details and checksums)
  * AppxManifest.xml: a Windows 10 desktop manifest instead of the Windows Phone one (the first build kept the phone app model 6.3.1 and Windows
    refused it with 0x80073CFD); the VC120 runtime DLLs (msvcp120_app, msvcr120_app, vccorlib120_app) are in this folder.
  * themes/PhoneTheme.xbf: the three Windows Phone theme colours the app uses (accent, foreground, chrome) are added - a PC does not have them.
    They follow the light / dark mode of Windows (Settings > Personalization > Colors).
  * WhatsApp.exe / Qt5Core.dll: the one import from PhoneAppModelHost.dll (exists on phones only) now comes from KERNELBASE.dll.
  * WhatsApp.exe: four Windows-Phone-only calls are switched off: hardware Back button, status bar colour / show-hide, "call" button (PhoneCallManager).
  * Not included: wad.exe (a Windows 10 Mobile background-service helper), the package signature files.

Known limits of this first build (please report what you hit)
  * No hardware Back button on a PC: use the on-screen back arrows of the app, if it has them.
  * Picking photos / videos may fail: the phone build uses the phone-only "PickSingleFileAndContinue" family.
  * Notifications while the app is closed need the background helper (wad.exe), which cannot run here.
  * First run on a PC: nothing is verified yet - the log files are Windows' own (diag.bat).
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=str(DEFAULT_SRC))
    ap.add_argument("--out", default=str(REPO / "dist-whatsapp"))
    ap.add_argument("--vclibs", default=str(DEFAULT_VCLIBS))
    ap.add_argument("--no-zip", action="store_true")
    ap.add_argument("--manifest", choices=sorted(MANIFESTS), default="win10",
                    help="win10 (default, Surface 2 / Windows 10): Windows 10 manifest + theme-following brushes; win81: Windows 8.1 PC manifest (for RT 8.1)")
    args = ap.parse_args()
    src, out_root, vclibs = Path(args.src), Path(args.out), Path(args.vclibs)
    for p, what in ((src / "AppxManifest.xml", "the app folder (AppxManifest.xml)"), (vclibs, "the VCLibs.120.00.Phone appx")):
        if not p.exists():
            print("ERROR: %s not found: %s" % (what, p)); return 2
    package_name = PACKAGE_NAME if args.manifest == "win10" else PACKAGE_NAME.replace("_Win10", "_Win81")
    folder = out_root / package_name
    if folder.exists():
        for p in sorted(folder.rglob("*"), reverse=True):
            p.unlink() if p.is_file() else p.rmdir()
        folder.rmdir()
    folder.mkdir(parents=True)
    report = ["Sup (WhatsApp client) 0.2.0.8 ARM - desktop package patch report", "source: %s" % src, ""]

    # 1. copy the app files
    copied = 0
    for path in sorted(src.rglob("*")):
        rel = path.relative_to(src)
        if path.is_dir(): continue
        if rel.parts[0].lower() in SKIP_DIRS or (len(rel.parts) == 1 and rel.name.lower() in SKIP_FILES): continue
        dest = folder / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(path.read_bytes())
        copied += 1
    report.append("copied %d files (skipped: %s, %s/)" % (copied, ", ".join(sorted(SKIP_FILES)), ", ".join(sorted(SKIP_DIRS))))

    # 2. manifest (complete replacement, see MANIFESTS) + unqualified logo copies (the package only has the phone's scale-240 files)
    (folder / "AppxManifest.xml").write_text(MANIFESTS[args.manifest], encoding="utf-8")
    report.append("AppxManifest.xml: %s manifest written (no PhoneIdentity, no Microsoft.VCLibs.120.00.Phone dependency, %s)" % (
        args.manifest, "TargetDeviceFamily Windows.Universal 10.0.10240.0" if args.manifest == "win10" else "OSMinVersion 6.3.0, m2 elements"))
    for logo in LOGOS:
        q = folder / "Assets" / (logo + ".scale-240.png")
        if not q.exists():
            print("ERROR: %s is missing" % q); return 2
        (folder / "Assets" / (logo + ".png")).write_bytes(q.read_bytes())
    report.append("Assets: unqualified copies of %s (from the scale-240 files; resources.pri kept unchanged)" % ", ".join(l + ".png" for l in LOGOS))

    # 2b. themes/PhoneTheme.xbf: the Windows Phone theme brushes (see PHONE_BRUSHES)
    theme = folder / "themes" / "PhoneTheme.xbf"
    before = theme.read_bytes()
    patched, at, size, keys = xbf_add_phone_brushes(before, args.manifest)
    theme.write_bytes(patched)
    report.append("themes/PhoneTheme.xbf: %d bytes inserted at 0x%X (node length +%d): %s -> %s  (sha256 %s -> %s)" % (
        size, at, size, ", ".join(keys), ", ".join(str(v) if isinstance(v, str) else "#%08X" % v for _, v in PHONE_BRUSHES[args.manifest]),
        sha(before)[:16], sha(patched)[:16]))

    # 3. VC120 runtime from the phone framework package (read from the appx, the appx itself is only read)
    with zipfile.ZipFile(vclibs) as z:
        names = {n.lower(): n for n in z.namelist()}
        for dll in VC_DLLS:
            if dll not in names:
                print("ERROR: %s is not inside %s" % (dll, vclibs)); return 2
            data = z.read(names[dll])
            (folder / dll).write_bytes(data)
            report.append("VC120 runtime: %s (%d bytes, sha256 %s) from %s" % (dll, len(data), sha(data)[:16], vclibs.name))

    # 4. PE import redirects
    for fname, old, new in IMPORT_REDIRECTS:
        path = folder / fname
        pe = PE(bytearray(path.read_bytes()))
        hits = [(o, n, r) for (o, n, r) in pe.import_descriptors() if n.lower() == old.lower()]
        if len(hits) != 1:
            print("ERROR: %s: expected exactly one import descriptor for %s, found %d" % (fname, old, len(hits))); return 3
        o, _, _ = hits[0]
        new_rva = pe.add_string(new.encode("ascii"))
        struct.pack_into("<I", pe.d, o + 12, new_rva)            # Name
        struct.pack_into("<I", pe.d, o + 4, 0)                   # TimeDateStamp: not bound
        path.write_bytes(bytes(pe.d))
        again = PE(bytearray(path.read_bytes()))
        names_now = [n.lower() for n in again.import_names()]
        assert old.lower() not in names_now and names_now.count(new.lower()) >= 1, (fname, names_now)
        report.append("import redirect %s: %s -> %s (descriptor name RVA %08x; the function is imported by name, hint kept)" % (fname, old, new, new_rva))

    # 5. byte patches
    for fname, rva, expected, new, what in EXE_PATCHES:
        path = folder / fname
        pe = PE(bytearray(path.read_bytes()))
        o = pe.off(rva)
        if bytes(pe.d[o:o + len(expected)]) != expected:
            print("ERROR: %s rva %05x: expected %s, found %s" % (fname, rva, expected.hex(), bytes(pe.d[o:o + len(expected)]).hex())); return 4
        pe.d[o:o + len(new)] = new
        path.write_bytes(bytes(pe.d))
        report.append("patch %s rva 0x%05X (file offset 0x%X): %s -> %s  [%s]" % (fname, rva, o, expected[:len(new)].hex(), new.hex(), what))

    # 6. scripts and readme
    (folder / "install.ps1").write_text(INSTALL_PS1, encoding="ascii")
    (folder / "uninstall.ps1").write_text(UNINSTALL_PS1, encoding="ascii")
    (folder / "diag.ps1").write_text(DIAG_PS1, encoding="ascii")
    for n in ("install", "uninstall", "diag"):
        (folder / (n + ".bat")).write_bytes(BAT.format(n).encode("ascii"))
    (folder / "README_DEVICE.txt").write_text(README_DEVICE, encoding="ascii")

    # 7. dependency view of the result (informational: system DLLs are not checked against a Windows image)
    report.append("")
    report.append("imports of the patched binaries (L = file in the package, S = expected from Windows):")
    local = {p.name.lower() for p in folder.iterdir() if p.is_file()}
    for fname in ("WhatsApp.exe", "Qt5Core.dll", "WaCore.dll", "LovitTls.dll", "LovitWebP.dll", "LovitLottie.dll") + tuple(VC_DLLS):
        pe = PE(bytearray((folder / fname).read_bytes()))
        parts = ["%s%s" % ("L:" if n.lower() in local else "S:", n) for n in pe.import_names()]
        report.append("  %-18s %s" % (fname, " ".join(parts)))
        bad = [n for n in pe.import_names() if n.lower() == "phoneappmodelhost.dll"]
        if bad: print("ERROR: %s still imports PhoneAppModelHost.dll" % fname); return 5
    report.append("")
    report.append("sha256 of every file:")
    for p in sorted(folder.rglob("*")):
        if p.is_file() and p.name != "PATCH_REPORT.txt":
            report.append("  %s  %s" % (sha(p.read_bytes()), p.relative_to(folder).as_posix()))
    (folder / "PATCH_REPORT.txt").write_text("\n".join(report) + "\n", encoding="utf-8")
    print("\n".join(report[:14]))

    if not args.no_zip:
        zip_path = out_root / (package_name + ".zip")
        if zip_path.exists(): zip_path.unlink()
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for p in sorted(folder.rglob("*")):
                if p.is_file(): z.write(p, package_name + "/" + p.relative_to(folder).as_posix())
        print("zip: %s (%.1f MB) sha256 %s" % (zip_path, zip_path.stat().st_size / 1e6, sha(zip_path.read_bytes())))
    print("folder: %s" % folder)
    return 0


if __name__ == "__main__":
    sys.exit(main())
