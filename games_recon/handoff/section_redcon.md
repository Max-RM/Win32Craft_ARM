STATUS: DONE

## REDCON 1 1.3.0.0 (Hexage, Windows Phone 8.1, C++/CX XAML)

Redcon is a landscape-only game that draws into a code-created SwapChainPanel with Direct3D 11, links the VC120 phone runtime, and uses the phone-only `VibrationDevice`, `HardwareButtons` and `StatusBar`. It runs as a **plain Win32 folder** (not an appx; no Developer Mode, no `Add-AppxPackage`) on the same "x104" fake-object host that runs Minecraft PE 0.10.4. It was about a quarter of the 0.10.4 effort.

### 1. Status

| | |
|---|---|
| Built, packaged | yes: `dist-redcon/REDCON_1.3.0.0_ARM32_Win32Craft/` and `.zip` (24.3 MB, sha256 `660b3045...5b1fe`), generated 2026-10-22 12:43 |
| Runs on Surface 2 (Win10 build 15035 ARM32, Tegra 4) | yes. Log `arm32/device_logs/surface2_win10_redcon_first_run.log`: 4200 frames in 140 s (about 30 fps, ~31 ms per frame), touch, Back, clean exit. Owner also confirmed **sound effects and music** by hand (not visible in that log) |
| Rendering | native, no shader work. The game asks for feature levels 11_1..9_1 itself and the Surface 2 hardware picks 9_1; its 10 shaders already carry level-9 (SM2.0) payloads |
| NOT verified | **Win11 ARM64 / POCO X3 Pro (your device)**, Surface RT / Windows RT 8.1. Nothing is known to be broken there; it was simply never run |

The packaged `win32craft.dll` (sha256 `58d62be8...`) is newer than the first-run log. It includes the two fixes made after that run: the host Back button is off for non-XAML games, and `_exit_app` ends the process at once (it hung 30 s before). The 0.10.4 and 1.2.8 builds are proven on your phone (build 22631, adapter "POCO X3 Pro Adreno 640 GPU", `arm32/device_logs/win11_arm64_0104_friend_r2dll.log`), so loader, window, touch and DXGI layers are known good there. The Redcon-specific risks are listed in section 4.

### 2. How to run it (ARM Windows device, ARM32 desktop code allowed)

1. Unzip `REDCON_1.3.0.0_ARM32_Win32Craft.zip` to a **short, ASCII-only, writable** path such as `C:\Games\Redcon`. The game copies the folder path into a 256-byte buffer, so keep it under about 150 characters. Do not run it from `WindowsApps`. Unsigned code must be allowed. `AppxManifest.xml`, `AppxSignature.p7x` etc. in the folder are inert leftovers from the original appx.
2. Double-click `start_selftest.bat`. It runs the host's 43 ABI/value checks and shows a message box ("all checks passed" or "FAILURES"). Then double-click `start.bat` (runs `Win32Craft.Redcon-arm32.exe`).
3. The log is `win32craft.log` next to the exe (fallback `%TEMP%`). On any problem send it, plus `win32craft.log.old` if present. For "missing DLL / entry point" errors run `loadcheck.exe` and send `loadcheck.txt`.
4. Saves and settings go to `%APPDATA%\Win32Craft\Redcon` (override with `--data-dir=<short ASCII folder>`).
5. Other start files: `start_window_1024/1280.bat`, `start_fullscreen.bat`, `start_touch.bat`, `start_touch_wmtouch.bat` and `start_touch_legacy.bat` (touch API fallbacks), `start_back_button.bat` (small host Back button; a tap is the phone's Back key), `start_inputlog.bat`, `start_trace.bat`. Keyboard Back is Esc or Alt+Left. The game has its own on-screen back buttons.
6. **If the picture is wrong on the phone, try `start_fl91.bat`** (`--feature-level=9_1`). By default Redcon gets no feature-level cap and sees whatever level the Adreno driver offers; this has not been tried on a FL 11 GPU.
7. Purchases: the Windows Phone Store is closed. The host reports every product as not owned (free edition) and purchases cannot complete. This is the owner's policy (`games_recon/GAMES_PLAN.md`, Order step 1); do not change it without asking.

### 3. How it works

**One DLL, a run-time game profile.** `win32craft.dll` is built from `win32_host.c` plus `win32_host_arm32_0104_{core,iids,async,app,panel,platform,input,selftest,boot}.inl` and exports `Win32Bootstrap`. It serves Minecraft PE 0.10.4, Redcon and (a separate path) Minecraft 1.2.8. The game is chosen by the patched exe's **SizeOfImage** (`x104_image_matches` in `win32_host_arm32_0104_boot.inl`) against `x104_profiles[]` in `win32_host_arm32_0104_core.inl` section 1c. `x104_profile_redcon` holds: key `redcon`, patched SizeOfImage `0x1E1000` (stock `0x1E0000`), entry `0x13A445`, 6 sections, call site `0x13A55A`, trampoline `0x1414B0`, WinMain thunk `0x13A91C`, data folder `Win32Craft\Redcon`, and `features = 0`. The three feature bits are `X104_FEAT_MCPE_PATCHES` (Minecraft code/data patches: shader profile string, resolution float, AppPlatform cells, the "report 9_3" lie), `X104_FEAT_IAT_FACTS` (Minecraft's import-table RVAs; other games hook by name only) and `X104_FEAT_XAML_PAGE` (MainPage/AppMain template). Code asks `x104_feat()`. There is **no separate Redcon unit file**: the Redcon-only code sits inside the app (section 7b), panel, platform and input units behind those checks.

**Exe patch (`tools/arm32-patcher.py`, profile `redcon`, `PROFILE_REDCON`).** It clears APPCONTAINER (`DllCharacteristics` 0x9140 to 0x8140), appends a non-executable `.d3imp` section holding a new import of `win32craft.dll!Win32Bootstrap`, rewrites the `bl` at RVA `0x13A55A` (the CRT's call to the game's WinMain thunk `0x13A91C`) to a position-independent trampoline in `.text` padding at `0x1414B0`, and sets SizeOfImage `0x1E1000`. There are no byte patches (Minecraft needs a shader-profile one; Redcon does not). The three VC120 phone runtime DLLs (`msvcr120_app`, `msvcp120_app`, `vccorlib120_app`) are patched out of `Microsoft.VCLibs.120.00.Phone` 12.0.30113.0 with APPCONTAINER cleared. Patched exe sha256 `a900f742...dce4` (in `patch_report.json`).

**Boot.** `x104_bootstrap` identifies the image, parses options, installs the platform (data root, paths, XAudio2 probe), hooks `vccorlib120_app!GetActivationFactoryByPCWSTR` (the activation router), hooks `D3D11CreateDevice`, patches DXGI vtable data cells, creates the window, initialises the units, then calls the game's WinMain thunk on the UI thread. The game's `Application::Start` lands in the fake `IApplicationStatics`, which runs the host's UI loop inside it and calls `OnLaunched`.

**Fake objects.** Every WinRT class the game activates is answered by a C-built COM object whose vtable slots are registered with `X104_SLOT(Interface, Method, thunk)`. A thunk whose signature differs from the generated typedef does not compile. Classes the host does not own fall through to the OS runtime and are logged as `X104 WinRT FALLBACK` (only `ApplicationLanguages` in the first run).

**What Redcon needed that Minecraft did not**
- **No MainPage.** `OnLaunched` creates its SwapChainPanel in code: `ISwapChainPanelFactory.CreateInstance(NULL, NULL)` in `win32_host_arm32_0104_panel.inl` returns the host's single panel (the window's client area). Alignment setters are no-ops.
- **Frames come from `CompositionTarget.Rendering` on the UI thread** (app unit section 7b, `x104_app_rendering_tick`, called every UI pass). While a handler is registered and the window shows, the UI loop waits at most 1 ms (`win32_host_arm32_0104_async.inl`). The game's own limiter and `Present(1,0)` pace it. Exceptions escaping the handler are guarded; 30 in a row stop the frames.
- **Resize** goes through `CoreWindow.SizeChanged` (the game re-reads `CoreWindow.Bounds`). The game re-creates its swap chain; the host's `CreateSwapChainForHwnd` stub retries up to three descriptors. On the Surface 2, attempts a and b failed with E_ACCESSDENIED and c (effect 0) worked.
- **Graphics.** The game's `CreateSwapChainForComposition` is redirected to `CreateSwapChainForHwnd` on the host window (path B, default); `SetMatrixTransform` returns S_OK; a `Present` stub does frame accounting. For non-Minecraft profiles `x104_bootstrap` (`win32_host_arm32_0104_boot.inl`) sets `arm32_feature_level_cap = 0` unless `--feature-level=` or `WIN32CRAFT_FEATURE_LEVEL` is given, so the game's own list reaches the driver and the true level is reported.
- **Lifecycle and phone classes.** `CoreApplication.Suspending/Resuming` (raised on `WM_CLOSE`), `ApplicationView.SuppressSystemOverlays`, `CoreWindowResizeManager`, the 11 `CoreWindow` events the game registers (`CharacterReceived` is fed from `WM_CHAR`), `HardwareButtons.BackPressed` (Esc / Alt+Left / the optional host button), `StatusBar` and `VibrationDevice` accepted as no-ops.
- **Platform.** `ApplicationData.LocalFolder` is the data root; `Package.InstalledLocation` is the exe folder, so `res\` is read directly. `RoamingSettings.Values` is a real OS `PropertySet` in memory. `ProductLicenses.Lookup(any id)` returns a license with `IsActive == false`. Purchase calls, `LoadListingInformationAsync` and `GetAppReceiptAsync` return `E_NOTIMPL`; the game throws `Platform::NotImplementedException` inside its own code and catches it (seen at 3.3 s in the log, play continued).
- **Audio needs no host object.** The game calls XAudio2 2.8 itself (148 OGG effects) and Media Foundation's Media Engine for the 6 WMA tracks, via `CoCreateInstanceFromApp`, with plain file paths under `res\`. The host only keeps the UI thread MTA and probes XAudio2 at install.

**Tests (x86, run on the PC; never run ARM binaries on the PC).** `tools/arm32/test_redcon_all.c` is a mock Redcon that makes the same calls through the same vtable slots in the same order as `Redcon.exe`, against the real core, async, app, panel, platform and input units. Run `python tools/arm32/test_0104_core.py --src tools/arm32/test_redcon_all.c`: today **108 checks, 0 failures**, including the 43-check selftest under the Redcon profile. `python tools/arm32/try_0104.py --selftest --integration` runs both mock games; `tools/arm32/diff_0104_log.py --against arm32/work/baseline_pre_redcon/integration_set.txt` proves 0.10.4 did not change. They do not cover D3D/DXGI, ARM register passing (that is the on-device selftest) or the real game code.

### 4. How to continue, and what is left

**Build (Windows x64 PC, `cmd.exe`).** Toolchain: LLVM clang `thumbv7-pc-windows-msvc` at `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin`, MSVC ARM libs, Windows SDK ARM libs, freestanding with `/nodefaultlib` and `.def` files. All paths are overridable (`LLVM_BIN`, `VCLIB`, `SDKVER`, `SDKROOT`, `BUILD`, `DIST`).
```
build_arm32.bat                 :: dist-arm32\win32craft.dll
build_loadcheck_arm32.bat       :: dist-arm32\loadcheck.exe
python tools/make_0104_package.py --game redcon --game-dir build-redcon/Redcon_1.3.0.0_ARM_original [--build] [--allow-stubs] [--no-apiset]
                                :: -> dist-redcon/REDCON_1.3.0.0_ARM32_Win32Craft(.zip)
python tools/arm32-patcher.py patch --profile redcon <game-folder> <out-folder> --host-dll dist-arm32/win32craft.dll   (also: detect, verify, profiles)
```
The packager refuses a `win32craft.dll` older than its sources, or any unit file that still contains `*** STUB`. It needs the VC `onecore\arm` redistributable (for `vcruntime140.dll`; `--redist`, `WIN32CRAFT_REDIST`) and the VCLibs 120 phone appx (`--companion-appx` or `WIN32CRAFT_VCLIBS120_APPX`; default search `D:/repo/armdeps/Microsoft.VCLibs.120.00.Phone_12.0.30113.0_arm__8wekyb3d8bbwe.Appx`). You supply the game folder yourself.

**Clean exe only.** The repo folder `Redcon_1.3.0.0_ARM` holds an exe modified by a third-party "Premium Patch" (`config.json`, sha256 `0f24aa01...`). Do not support it. The patcher accepts only the stock exe (sha256 `3aca1266...5f74`, 1,941,504 bytes). Unpack `Redcon_1.3.0.0_ARM.appx` (a zip) to a clean folder, as done in `build-redcon/Redcon_1.3.0.0_ARM_original`.

**Rules and pitfalls.** MSVC `<stdarg.h>` `va_start` is wrong under clang on ARM (it assumes MSVC argument homing). Use `__builtin_va_start/_arg/_end`; this crashed `m0probe` on the Surface 2. Unit code may only use symbols from `core.inl`, the generated iids/header files and `win32_host.c` (lint: `tools/arm32/try_0104.py`). Design rules, threading and the ordering/protocol rules R1..R17 (for example R1: never invoke a game delegate synchronously from inside a fake) are in `arm32/v0_10_4/HOST_DESIGN.md`. RVA-level recon is in `games_recon/redcon_redcon-{startup,render,platform,reuse}.md`. Game folders, `*.appx`, `*.xap` and `*.pdf` must never go into the public source zip or git. The run package above contains the game's extracted assets and is for personal use only.

**Open items, most important first (all minor polish, none is a known blocker)**
1. **First run on the POCO X3 Pro.** Run `start_selftest.bat`, then `start.bat`, and check the `X104 panel: selected feature level` log line, the picture, touch, sound and music. Send the log. If it renders wrongly, compare with `start_fl91.bat`.
2. **Windows RT 8.1 / Surface RT is unverified** for Redcon (UCRT and api-set shims are shipped; run `loadcheck.exe` first). Music through the Media Engine (`CoCreateInstanceFromApp`) was never measured there.
3. **XAML dialog helper not faked** (`Popup`, `Border`, `StackPanel`, `TextBlock`, `TextBox`, `Button`; the game's `DialogHelper`). First-run prompts (age gate, cloud save, score submission, "Service Unavailable") may use it. It was not hit in the 148 s run; if it is, the log names the missing class. Options: hook the game function at RVA `0x529A4` (its two dialog-builder calls are at `0x52A3C` / `0x52B1A`; RVAs from `games_recon/redcon_redcon-platform.md` F19) with a Win32 dialog, which needs a short RE of its callback contract first, or fake about ten XAML classes.
4. **Store fakes.** `LoadListingInformationAsync` and `GetAppReceiptAsync` could return completed async operations instead of `E_NOTIMPL`. In-app purchases cannot complete (policy above).
5. **Roaming settings are not persisted** (in-memory `PropertySet`). The game's own `settings` and `save<N>` files in the data root do persist. Which roaming keys are lost between runs was not determined.
6. **Record `expected_outputs`** in `PROFILE_REDCON` (`tools/arm32-patcher.py`, currently `[]`): the patched exe hash above is now proven by a device run.
7. **Frame rate.** About 31 ms per frame on the Surface 2; whether that is the game's own 30 fps limiter was not investigated.
8. **Cosmetic.** The package `MANIFEST.txt` footer still says "Nothing in this package has been run on a device yet." (a fixed string in `tools/make_0104_package.py`). The log line `refcount underflow on SuspendingDeferral ... clamped to 1` at exit is harmless.
