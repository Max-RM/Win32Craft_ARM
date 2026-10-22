# Sup 0.2.0.8 on Windows 10 desktop (ARM32): what we found and how to support it from source

For: the developer of **Sup** (`ru.computershik.sup`, Computershik).
From: the Win32Craft project (we port old Windows Phone 8.1 / Windows RT apps and games to Windows RT 8.1 and Windows 10 ARM32 devices).

## Short version

We took the published ARM package `ru.computershik.sup_0.2.0.8_arm` and, **without changing any of its app logic**, made it install and run on a
**Surface 2 with Windows 10 build 15035 (ARM32 desktop)**. Login and chats work. The only thing users miss on a PC is a way to go **Back** (the phone's
hardware Back key does not exist there).

Everything we had to patch in the binaries is a few lines in your source. If you rebuild with the changes below, Sup can ship as a normal app for Windows 10
PCs/tablets (and, with a Windows 8.1 head, for Windows RT 8.1 tablets such as the Surface RT / Surface 2).

## What stops the phone package on a PC (in the order Windows hits it)

| # | Problem on a PC | What we did to the binary package (proof only) | Source-level fix |
|---|---|---|---|
| 1 | The manifest is a Windows **Phone** 8.1 manifest: `OSMinVersion 6.3.1` (= phone app model), `mp:PhoneIdentity`, `m3:` elements, dependency on `Microsoft.VCLibs.120.00.Phone`. Windows 10 desktop refuses it: `0x80073CFD ... requested Windows version 6.3 with AppModel version 1, while the current Windows version is 6.3 with AppModel version 0`. | Wrote a complete Windows 10 manifest (`TargetDeviceFamily Windows.Universal`, `uap:` elements; note: `uap:VisualElements` has no `ForegroundText` / `ToastCapable`; `picturesLibrary` / `videosLibrary` are `uap:Capability`). For RT 8.1: `OSMinVersion 6.3.0` + `m2:` elements (`Square30x30Logo`, `Square70x70Logo`). | Add a Windows 10 (UWP) head, or a Windows 8.1 head of the Universal 8.1 project. |
| 2 | `WhatsApp.exe` and `Qt5Core.dll` import `LoadPackagedLibrary` from **`PhoneAppModelHost.dll`**, which exists only on phones, so the process cannot even load. | Renamed that import's DLL to `KERNELBASE.dll` (same function). | Link the PC build against the normal libraries (`kernel32` / `WindowsApp.lib`), not `WindowsPhoneCore.lib`, so `LoadPackagedLibrary` comes from kernel32. Qt for WinRT built for the desktop target does this. |
| 3 | Phone-only WinRT classes activated at start-up throw `ClassNotRegistered` on a PC: `Windows.Phone.UI.Input.HardwareButtons` (BackPressed), `Windows.UI.ViewManagement.StatusBar` (colour, show/hide), and on demand `Windows.ApplicationModel.Calls.PhoneCallManager` (call button). | The four wrapper functions return at once (`bx lr`). | Guard them: on Windows 10 `Windows::Foundation::Metadata::ApiInformation::IsTypePresent(L"Windows.Phone.UI.Input.HardwareButtons")` (same for StatusBar / PhoneCallManager); in an 8.1 build a `try { ... } catch (Platform::Exception^) {}` around the first use. |
| 4 | **No Back on a PC.** With BackPressed unavailable there is no way back from a chat to the chat list. | Not fixed (we stopped here on purpose). | On Windows 10: `SystemNavigationManager::GetForCurrentView()->BackRequested += ...` (same handler logic as BackPressed, set `e->Handled` when `Frame->CanGoBack`) and `AppViewBackButtonVisibility = Visible` (title-bar back arrow; also Win+Backspace and the tablet-mode back button). An on-screen back button in the page header helps on Windows 8.1. |
| 5 | Phone theme resources do not exist on a PC: six pages use `{ThemeResource PhoneAccentBrush}`, ChatPage `{ThemeResource PhoneChromeBrush}`, and the `AccentWhenTrue` converter looks up `PhoneAccentBrush` / `PhoneForegroundBrush` in `Application.Resources` (it throws if missing). | Added the three brushes to `themes/PhoneTheme.xbf` with `Color="{ThemeResource SystemAccentColor / SystemBaseHighColor / SystemChromeMediumColor}"`. | Define `PhoneAccentBrush`, `PhoneForegroundBrush`, `PhoneChromeBrush` in your own `PhoneTheme.xaml` (theme dictionaries Light/Dark), or switch to `SystemControl*` / `SystemAccentColor` resources in the Windows 10 head. |
| 6 | `Pivot` / `PivotItem` and `ContentDialog`: phone controls in 8.1, universal in Windows 10. | Nothing needed under a Windows 10 manifest (the Windows 10 XAML styles contain them). | Fine for a Windows 10 head; a Windows 8.1 head needs replacements (Hub / FlipView / a custom dialog). |
| 7 | The VC++ 2013 runtime framework `Microsoft.VCLibs.120.00.Phone` cannot be installed on a PC. | Shipped `msvcp120_app.dll`, `msvcr120_app.dll`, `vccorlib120_app.dll` (12.0.30113.0) next to the exe. | Depend on `Microsoft.VCLibs.120.00` (PC) - or on VCLibs 140 if you rebuild with a newer toolset. |
| 8 | The package only has `scale-240` logos and a phone `resources.pri`. | Added unqualified logo copies; the phone PRI worked unchanged on 15035. | Normal desktop assets (scale-100/140/180) via the desktop project; MakePri regenerates the PRI. |
| 9 | Phone-only features that cannot work on a PC: `FileOpenPicker` / `FileSavePicker` *AndContinue* calls (attachments), the background helper `wad.exe` (Windows 10 Mobile service `WhatsAppBg`). | Not touched; `wad.exe` was left out of the PC package. | Use `PickSingleFileAsync` / `PickSaveFileAsync` on PCs; replace the helper with a background task or by keeping the connection while the app runs. |

## Test result

* Device: Microsoft Surface 2, Windows 10 build 15035 (ARM32), Developer Mode, `Add-AppxPackage -Register AppxManifest.xml` of the patched folder.
* Sup started, showed the login (QR) page, logged in, and chats worked. Back navigation from a chat to the list is missing (row 4).
* Nothing else in the app was changed: no protocol, crypto, network or UI logic was touched.

## Files we can share (on request)

* The exact patch list with offsets and checksums (`PATCH_REPORT.txt` of our package) and the packaging script, so you can reproduce what we did.
* The Windows 10 and Windows 8.1 manifests we validated with `makeappx pack` (Windows 10 SDK 10.0.19041).

Thank you for Sup - it is one of the few clients that still lets these devices chat.
