# Running abandoned Windows Phone 8.1 apps on Windows 10 ARM32 (and later Windows RT 8.1)

Proven 2026-10-21 on **Sup** (a full-XAML Windows Phone 8.1 app, C++/CX + Qt): after these steps it installed and worked on a Surface 2 with
Windows 10 build 15035 (ARM32 desktop). Use this for apps whose developers are gone and whose source is lost. When the developer can still be
reached, send him a hand-off instead (example: `whatsapp/HANDOFF_FOR_SUP_DEVELOPER.md`) - a rebuild from source is always better than patching.

## 0. Pick the route

Run the recon tool on the extracted package (or the `.appx`):

    python tools\wp81\wp81scan.py <package folder or .appx> --out report.md

Its verdict section decides:
* **Game** (a SwapChainPanel / CoreIndependentInputSource app that names a few dozen XAML classes and draws itself with Direct3D) -> the
  **Win32Craft fake-object host** runs it as a plain Win32 program, no installation (worked example: Minecraft PE 0.10.4.40, `arm32/v0_10_4/`).
* **Full XAML app** (pages, lists, dialogs, styles) -> the **package route** below. It needs the OS XAML framework and package identity, so it is
  installed (registered) as an app; a Win32 host would have to re-implement XAML.

## 1. Package route, step by step (worked example: `tools/whatsapp/make_whatsapp_package.py`)

| Step | Why | How (proven) |
|---|---|---|
| 1. Manifest | A phone manifest (`OSMinVersion 6.3.1` = phone app model, `mp:PhoneIdentity`, `m3:` elements, `Microsoft.VCLibs.120.00.Phone`) is refused by a PC: `0x80073CFD ... AppModel version 1 ... current ... AppModel version 0`. | Write a COMPLETE new manifest (do not regex-edit): Windows 10 = `foundation/windows10` + `uap`, `TargetDeviceFamily Windows.Universal 10.0.10240.0`, `uap:VisualElements` WITHOUT `ForegroundText`/`ToastCapable`, `uap:Capability` for picturesLibrary/videosLibrary/musicLibrary. Windows RT 8.1 = `OSMinVersion 6.3.0` + `m2:` (`Square30x30Logo`, `Square70x70Logo`). Prefer Windows 10: its XAML styles include Pivot / ContentDialog. |
| 2. Validate | Registration errors cost a device round trip. | `makeappx pack /d <folder> /p test.appx /o` (Windows 10 SDK) from **PowerShell** (Git Bash rewrites `/d /p /o` as paths). It validates the manifest and the referenced files. |
| 3. Logos | Phone packages carry only `scale-240` images. | Add unqualified copies (`Logo.png` from `Logo.scale-240.png` ...); keep `resources.pri` (the phone PRI worked on 15035; fallback: `makepri new` with `/pv 10.0.0`). |
| 4. VC++ runtime | The phone framework package cannot be installed on a PC. | Ship `msvcp120_app.dll`, `msvcr120_app.dll`, `vccorlib120_app.dll` from `Microsoft.VCLibs.120.00.Phone_12.0.30113.0_arm` (`D:\repo\armdeps`) next to the exe; no `<Dependencies>` entry. |
| 5. Phone-only DLL imports | `PhoneAppModelHost.dll` exists only on phones; the process does not even load. | Rename the import descriptor's DLL name to `KERNELBASE.dll` (new string in the import section's slack, VirtualSize bumped; the function stays imported by name). Usually only `LoadPackagedLibrary` is imported from it. |
| 6. Phone-only WinRT classes | `Windows.Phone.*`, `StatusBar`, `PhoneCallManager`, `VibrationDevice` ... throw `ClassNotRegistered` - at start-up that kills the app. | The scanner lists each class with the function that activates it (C++/CX: one wrapper per class, the class name is an inline UTF-16 string). Read the wrapper and its callers (`tools/arm32/arm32re.py func/callers`), check the return convention, then patch the first halfword to `bx lr` (0x4770) when the result is unused or the function returns its first argument. Check the expected bytes before patching. |
| 7. Phone theme resources | `{ThemeResource PhoneAccentBrush}` etc. and code lookups by name throw on a PC (no desktop theme defines them). | Add the missing keys to the app's own XBF dictionary that is merged into `Application.Resources` (XBF v1: a plain token stream, header dword 0x08 = node-section length; clone an existing keyed element of the same type - see `xbf_add_phone_brushes`). On Windows 10 use `{ThemeResource System*Color}` so they follow light/dark mode. |
| 8. Install | Registration of a loose folder needs Developer Mode. | `Add-AppxPackage -Register AppxManifest.xml` (`install.ps1`), `diag.ps1` collects the AppX deployment / TWinUI / AppModel-Runtime / Application event logs - packaged apps write no win32craft.log. |

## 2. Known gaps of the package route

* **Back**: phone apps navigate back with the hardware key (`HardwareButtons.BackPressed`), which step 6 had to switch off. The Windows 10 way is
  `SystemNavigationManager.BackRequested` + `AppViewBackButtonVisibility` (title-bar arrow, Win+Backspace, tablet-mode back) - this needs code in the
  app, so it is either a developer fix or an extra in-process helper (not built yet).
* Phone-only file pickers (`PickSingleFileAndContinue` family), background services / tasks of the phone, phone sensors.
* Windows RT 8.1: no Pivot / ContentDialog in its XAML; a Developer License is needed to register a folder; untested.

## 3. Checklist for a new app

1. `wp81scan.py` -> route + list of items.  2. Copy `tools/whatsapp/make_whatsapp_package.py` to `tools/<app>/`, change the identity, file lists, the
`EXE_PATCHES` (step 6 RVAs + expected bytes), `IMPORT_REDIRECTS` and the theme keys.  3. Build, `makeappx pack` both manifests.  4. Device:
Developer Mode, `install.bat`, start the app; if it closes, `diag.bat` -> diag.txt, iterate from the first error.  5. Write down the result in
`<app>/README.md` and the memory.
