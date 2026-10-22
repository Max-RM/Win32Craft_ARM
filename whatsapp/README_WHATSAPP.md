# Sup (unofficial WhatsApp client) 0.2.0.8 ARM - running it on the user's ARM32 devices

STATUS 2026-10-21: round 2 package built (Windows 10 manifest + phone theme brushes), NOT yet tested on a device.  Round 1 was refused by Windows at registration (see below).
Target: Surface 2, Windows 10 build 15035 (ARM32 desktop).  Windows RT 8.1 is a later variant (see "RT 8.1").

## What the app is (recon, all offline)
* `WhatsApp.Qt_0.2.0.8_ARM/` (repo root, third-party, NOT in the source zip): a Windows Phone 8.1 XAML app, `ru.computershik.sup` v0.2.0.8, publisher "Computershik", manifest has `mp:PhoneIdentity` and the
  dependency `Microsoft.VCLibs.120.00.Phone` (>= 12.0.30113.0), VS2013 U4 (cl 18.00.40629), C++/CX.  Files: `WhatsApp.exe` (XAML app + UI), `WaCore.dll` (7 MB, WhatsApp protocol, ws2_32, WinRT crypto),
  `Qt5Core.dll` (Qt for WinRT), `LovitTls/WebP/Lottie.dll`, `WhatsApp.winmd`, compiled XAML (`App.xbf`, `pages/*.xbf`, `themes/PhoneTheme.xbf`), `wad.exe`.
* `WhatsApp.exe` activates ~120 WinRT classes - a FULL XAML UI: Frame, Page, ListViewBase, ContentDialog, Flyout/MenuFlyout, RichTextBlock + Hyperlink/Run/Paragraph, Storyboard/keyframe animations,
  WriteableBitmap, VisualTreeHelper, DispatcherTimer, ... plus MediaCapture, FileOpenPicker/FileSavePicker, ToastNotification, BitmapDecoder/Encoder, StreamSocket.
* Windows-Phone-ONLY pieces: `PhoneAppModelHost.dll!LoadPackagedLibrary` (imported by name, hint 2, by `WhatsApp.exe` and `Qt5Core.dll`; the DLL does not exist on a PC), and four wrappers in `WhatsApp.exe`, each
  activating one phone-only class (a C++/CX throw on a PC, in the page constructor path `sub_599c` at start-up): `Windows.Phone.UI.Input.HardwareButtons` (0x5E694, add_BackPressed), `Windows.UI.ViewManagement.StatusBar`
  (0x82DE8 GetForCurrentView, used by 0x82A08 colour and 0x82CA8 show/hide), `Windows.ApplicationModel.Calls.PhoneCallManager` (0x5E930).  `Qt5Core/WaCore/Lovit*` use only desktop-safe classes.
* `wad.exe`: a Windows 10 Mobile background-service helper (service `WhatsAppBg`, `C:\Data\Users\DefApps\...`, pipes in the AppContainer namespace, `setup/launch/halt/status/remove/console`; imports `a32.dll`).  Phone-only, left out.

## Decision: NOT the Win32Craft fake-object host
Win32Craft (and the 0.10.4 host) run a game against fake XAML objects; that works because a game uses ~30 of them and draws itself with D3D.  Sup needs a real XAML framework (layout, templating, text, ListView
virtualisation, animations): faking it would be a XAML re-implementation.  A desktop Win32 process cannot host the OS XAML (XAML Islands needs Windows 10 1903+; the Surface 2 has 15035).
So the app has to run as what it is, a PACKAGED app, and the PACKAGE is patched (same spirit as `tools/arm32-patcher.py`, different target): `tools/whatsapp/make_whatsapp_package.py`.

## The patch (all in `make_whatsapp_package.py`, listed with checksums in the package's `PATCH_REPORT.txt`)
1. `AppxManifest.xml`: PhoneIdentity + phone VCLibs dependency removed (it becomes a plain Windows 8.1 style app; Windows 10 runs those).  AppxBlockMap/AppxSignature/[Content_Types]/AppxMetadata not copied.
2. VC120 runtime (`msvcp120_app`, `msvcr120_app`, `vccorlib120_app`, 12.0.30113.0) taken from `D:\repo\armdeps\Microsoft.VCLibs.120.00.Phone_12.0.30113.0_arm...Appx` and placed next to the exe (app-local).
3. Import redirect in `WhatsApp.exe` and `Qt5Core.dll`: descriptor `PhoneAppModelHost.dll` -> `KERNELBASE.dll` (new name string in the slack of the import section, VirtualSize bumped; the function stays imported by name).
4. `bx lr` at the four wrappers (expected bytes checked first).
Install on the device: Developer Mode on, run `install.bat` (`Add-AppxPackage -Register AppxManifest.xml`); `diag.bat` writes `diag.txt` (registration, AppX deployment, TWinUI, AppModel-Runtime, Application log) because a packaged app has no
win32craft.log.  Package: `dist-whatsapp/Sup_WhatsApp_0.2.0.8_ARM_Win10.zip` (8.6 MB).

## Expected first-run problems / next steps (iterate from diag.txt)
* Registration refused (0x80073CFF = Developer Mode off; a framework / architecture error = the manifest conversion needs another change).
* Start-up crash: look for the next phone-only class (grep the activation list above; `sub_16c024` is the C++/CX GetActivationFactory wrapper - all `Windows.*` names are inline wide strings, find the callers with
  `ARM32_EXE=<WhatsApp.exe> python tools/arm32/arm32re.py strx -w <class>`), or an XAML parse error (phone-only controls / styles in `themes/PhoneTheme.xbf`).
* Features: no hardware Back button; `FileOpenPicker` phone continuation APIs (`PickSingleFileAndContinue`) do not exist on a PC; toast/background activity needs `wad.exe` (cannot run); `ContentDialog` exists on Windows 10 only.
* An in-process activation hook (a DLL loaded by an import added to `WhatsApp.exe`, hooking `GetActivationFactoryByPCWSTR` of `vccorlib120_app.dll` as `win32_host.c` does for Minecraft) is the next tool for phone-only classes
  that must keep working (e.g. a Back button from the keyboard) - the activation router / fakes of the Minecraft hosts can be reused.

## RT 8.1 (not started)
RT 8.1 has the Windows 8.1 XAML (no `ContentDialog`; Win8.1 PC controls) and VCLibs.120.00 (PC) `12.0.21005.1` < the required 12.0.30113 (use the Phone DLLs app-local as here).  Needs: a ContentDialog replacement or
its users (`sub_994f4`) neutralised, Developer License + `Add-AppxPackage -Register` on RT, the same import / wrapper patches.  Ask for a Surface RT trip only after the Surface 2 run works.

## Round 1 result and round 2 changes (2026-10-21)
* Round 1 never ran: `diag_surface2_win10_15035_register_failed.txt` (in this folder) - 0x80073CFD "The package requested Windows version 6.3 with AppModel version 1, while the current Windows
  version is 6.3 with AppModel version 0".  The third field of OSMinVersion is the app model: 6.3.1 = Windows Phone 8.1, 6.3.0 = Windows 8.1 PC; the m3 (2014) manifest elements are only valid
  with 6.3.1.  Round 1 had kept 6.3.1 and the m3 elements.
* Round 2 (`make_whatsapp_package.py`): a complete manifest is written - `--manifest win10` (default): Windows 10 manifest (TargetDeviceFamily Windows.Universal 10.0.10240.0, uap elements, no
  ForegroundText/ToastCapable), chosen because the Windows 10 XAML style set has Pivot / ContentDialog / CommandBar templates and the win81 set has no Pivot / ContentDialog;
  `--manifest win81`: Windows 8.1 PC manifest (6.3.0, m2: Square30x30Logo, Square70x70Logo) for a later RT 8.1 attempt.  Both pass `makeappx pack` validation (Windows 10 SDK 19041).
  Unqualified copies of the six logos are added (resources.pri unchanged).
* Phone theme brushes: six pages use {ThemeResource PhoneAccentBrush}, ChatPage {ThemeResource PhoneChromeBrush}, and the AccentWhenTrue converter looks up PhoneAccentBrush /
  PhoneForegroundBrush in Application.Resources (throws when missing).  No desktop theme has them.  `themes/PhoneTheme.xbf` (merged into Application.Resources by OnLaunched/sub_41a8 before any
  page) gets three SolidColorBrush entries, cloned from its own 'AvatarPlaceholderBrush' element (XBF v1 node stream = plain tokens without offsets; header dword 0x08 = node length).
  Windows 10: Color = {ThemeResource SystemAccentColor / SystemBaseHighColor / SystemChromeMediumColor} (light/dark aware, keys present in the oldest Windows 10 theme 'th' and in 'rs2');
  8.1: fixed #FF25D366 / #FFFFFFFF / #FF1F1F1F (byte-identical to an independently produced patch of the investigation).  First run shows LoginPage, which uses none of these.
* Remaining risks (cannot be settled offline): the phone PRI (`win://WindowsPhone/1`, TargetOS 6.3.1) under a desktop package - fallback: regenerate it with makepri (`createconfig /pv 10.0.0`);
  the XBF type entries of Pivot/PivotItem carry the namespace 'Windows.UI.Xaml' (Mojang's 0.10.4 XBF has the same for Page/Button and runs on Windows 10 Mobile, so the reader resolves by
  short name) - fallback: set their namespace index to 0 in MainPage.xbf (dwords at 0x10EE, 0x1112) and ChatMediaPage.xbf (0xC7E, 0xCA2); FileOpenPicker uses the phone-only
  *AndContinue methods (attachments), no Back button, wad.exe (background service) not available.
* Next device step: Developer Mode on, `install.bat`, start "Sup"; if it closes or does not register, `diag.bat` -> diag.txt.  Expected first screen: the login page (QR code).
