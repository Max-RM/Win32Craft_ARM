# Win32Craft — hand-off

**What this is.** Win32Craft runs UWP and Windows-Phone **ARM32** apps and games as **native Win32** on ARM Windows —
no Store, no AppContainer, no original shell. For each app we replace its shell/launcher with a small native host and a
handful of shims, so the app's own ARM32 code (its engine, its D3D, its audio) runs directly on the device. It targets
old ARM tablets that are stuck at Direct3D **feature level 9_1**: a Surface RT (Windows RT 8.1, Tegra 3) and a Surface 2
(Windows 10 build 15035 ARM32, Tegra 4), and it also runs on your **POCO X3 Pro (Windows 11 ARM64)** via WOW.

This bundle is three games plus the full source to continue them.

## What's in this bundle

| | |
|---|---|
| `HANDOFF_FOR_FRIEND.md` | this document — status, how to run, how it works, how to continue, for every game |
| `runnable/` | the three built packages you can copy to a device and run (they contain extracted game DLLs/assets) |
| `source.zip` | the Win32Craft source + tooling to rebuild/continue (extracts to `Win32Craft_src/`; **no game folders / .appx / .xap** — bring your own game copies) |

Inside `runnable/`: `REDCON_1.3.0.0_ARM32_Win32Craft.zip`, `BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft.zip`,
`GeometryDash_1.0.0.0_ARM32_Win32Craft.zip` (the playable GD host), and `gd_frame_900.png` (proof: live GD gameplay at
FL 9_1). Two per-game improvement/continuation docs accompany the bundle: `HANDOFF_GD_IMPROVEMENTS.md` (Geometry Dash)
and `HANDOFF_BADPIGGIES_MODS.md` (Bad Piggies).

## Status at a glance

| Game | State | Renders at FL 9_1 | Host type | What's left |
|---|---|---|---|---|
| **Redcon** 1.3.0.0 (WP8.1 XAML) | ✅ **runs** on Surface 2 — ~30 fps, sound + music, touch, Back, exit | native (the game asks 11_1..9_1 itself; shaders already level-9) | x104 "fake-object" host (shared `win32craft.dll`), `redcon` profile | minor polish (XAML dialog widgets, store/IAP stubs) |
| **Bad Piggies Rebooted** 1.0.0.0 (Unity 2018.4 IL2CPP UWP) | ✅ **renders + runs** on Surface 2's Tegra 4 — real GPU, menus + gameplay | yes, via the `l9shim` D3D11 shim (FL10→9_1) | `l9d3d11.dll` shim injected into UnityPlayer | optional: patch the mod's own C# bug; Tegra 3 / RT 8.1 test |
| **Geometry Dash** 2.0.1.0 (WP8.0 Silverlight + cocos2d/ANGLE) | ✅ **plays** on Surface 2 — menu + levels, upright, music + SFX, responsive touch | yes, after clamping ANGLE's feature level to the device's real 9_1 (it asked for 9_3) | new standalone host `gd_host.exe` | device matrix (Surface RT 8.1 / POCO); optional on-demand WinRT (save, keyboard); see `HANDOFF_GD_IMPROVEMENTS.md` |

All three are playable ports now. Redcon, Bad Piggies, and Geometry Dash each boot and run on the Surface 2's Tegra 4 at
FL 9_1. For GD the whole shell was built out (window, present-with-rotate, input, audio) and the one hard bug — a GPU
device-removal that masqueraded as a loading stall — was fixed by clamping ANGLE's feature level to the real hardware.
What remains across all three is polish and the wider device matrix (Surface RT 8.1, your POCO X3 Pro).

## Devices & how you fit in

- **Surface RT** — Windows RT 8.1, Tegra 3, FL 9_1. The oldest target; some things (WARP vs HW) still unmeasured there.
- **Surface 2** — Windows 10 build 15035 ARM32, Tegra 4, FL 9_1. The main test device for all three games.
- **POCO X3 Pro** — Windows 11 ARM64 (yours). Runs the ARM32 binaries under WOW; good for continuing GD (M2+) and for a
  second data point. Everything in `runnable/` should run there the same way it does on the Surface 2.

**Golden rules** (see *Methods & toolchain* below): build the ARM32 binaries on an x64 PC with clang, and **never run the
ARM binaries on the x64 PC** — only on an ARM device. The public source must never contain the game folders or their
`.appx`/`.xap`. Don't hard-wire feature level 9_1 as a permanent default — let a capable GPU pick its own.

## How to read the rest of this document

1. **Methods & toolchain** — the two host architectures, the reusable techniques, the exact build toolchain, the
   diagnostic tools (`loadcheck`, `m0probe`), the repo map. Read this first if you're going to build.
2. **Redcon** — a finished WP8.1 XAML port on the shared x104 host.
3. **Bad Piggies Rebooted** — a finished Unity/FL9_1 port via the D3D11 shim.
4. **Geometry Dash** — now a playable port (menu + levels, music, responsive touch). The section covers how the host
   works; `games_recon/HANDOFF_GD_IMPROVEMENTS.md` covers the root-cause fix, the touch-latency fix, and what to improve.

There is also a separate `games_recon/HANDOFF_FOR_BADPIGGIES_DEV.md` — a clean bug report for the Bad Piggies *mod author*
about three pre-existing C# bugs we found (not our port's fault). Pass it along if you're in touch with them.

---

---

## Methods, Toolchain & How to Work on This Project

Shared developer guide for all ports. Everything below was checked against the files in the repo; where something is not
known or not measured, it says so. Rule of thumb for every game: **recon read-only -> prove imports load (loadcheck) ->
prove the GPU path (m0probe) -> pick a host -> iterate on device logs -> package last.**

### 1. Two host architectures (and the no-host routes) - when to use which

| | x104 "fake-object" host | standalone `gd_host.c` | no host (package / shim route) |
|---|---|---|---|
| Code | `win32_host.c` + `win32_host_arm32_0104_{core,boot,async,app,panel,platform,input,selftest}.inl` + generated `_iids.inl` (included from `win32_host.c` in front of `Win32Bootstrap`) -> **one** `win32craft.dll` | `tools/gd/gd_host.c` (527 lines, own freestanding `gd_host.exe`, KERNEL32 imports only) | `tools/badpiggies/`, `tools/whatsapp/`, `wp81/WP81_APP_METHOD.md` |
| Game shape | WP8.1 XAML / CoreWindow game with its own `.exe`: `SwapChainPanel` + D3D11, touching a few dozen XAML/WinRT classes. Minecraft PE 0.10.4, Redcon (1.2.8 is the older CoreWindow path inside `win32_host.c`) | WP8.0 **Silverlight** shell + `DrawingSurface` + native cocos2d-x/ANGLE **component DLL** (no game exe to patch): Geometry Dash | Already a Win10 UWP appx whose MinVersion <= device build (Bad Piggies, MinVersion 15035), or a full XAML app (Sup) |
| Mechanism | A patched exe (`tools/arm32-patcher.py`) redirects one call site to `win32craft.dll!Win32Bootstrap`; the host then runs the game's **own** `Application::Start -> OnLaunched -> MainPage` code against ~30 hand-written fake WinRT/XAML COM objects (~80 vtable slots). Profile (`X104Profile`, `core.inl` section 1c) is picked at run time from the patched `SizeOfImage` (0.10.4 `0x00E6F000`, Redcon `0x001E1000`); feature bits keep Minecraft-only patches out of other games | Host loads `GeometryJumpComponent.dll`, makes fakes for the 4-method `IDrawingSurfaceBackgroundContentProviderNative` contract (Connect / PrepareResources / Draw) and calls `Draw()` once per cocos2d frame into a render target it owns | Game stays a normal app; only the missing pieces are supplied (import redirect + shim DLL + app-local runtime) |
| Read first | `arm32/v0_10_4/HOST_DESIGN.md` (single source of truth: threading, rules R1..R17), `HANDOFF_ARM32.md` | `tools/gd/GD_BUILD.md` (decoded contract + milestone log) | `games_recon/bad_piggies_hw-design.md`, `HANDOFF_ARM32.md` (BP sections) |

**Choose by running** `python tools\wp81\wp81scan.py <folder|.appx> --out report.md`: "game that draws itself with D3D" -> x104 host; "full
XAML app" -> package route (`wp81/WP81_APP_METHOD.md`). Native component with a tiny COM contract and no exe -> standalone host. A UWP appx that
already installs on the target OS -> do not write a host at all.
State (details in the status section): GD is at M1 (headless; renders its real splash frame at FL 9_1 on the Surface 2; no window, present, input or audio yet);
Redcon runs on the Surface 2; Bad Piggies runs on the Surface 2 through the shim (menu and a level load/play per `games_recon/bp_managed_exceptions.md`; 2 shadow-caster shader pairs still miss the replacement table, `bp_missing_shaders.md`).

### 2. Reusable techniques

* **PE import redirection** - rewrite `IMAGE_IMPORT_DESCRIPTOR.Name` (+12) to a new string, zero `TimeDateStamp` (+4); function names stay. Used for UnityPlayer
  `d3d11.dll -> l9d3d11.dll` and the `*_app` runtime -> plain names (`tools/badpiggies/make_badpiggies_package.py` step 3; UnityPlayer imports exactly the 3 d3d11 functions the shim exports -
  checked with `llvm-readobj --coff-imports`; the packager's own "shim exports everything imported" guard is vacuous because the `PE` helper has no `imported_functions`, so re-check by hand for a new game), Sup `PhoneAppModelHost.dll -> KERNELBASE.dll` (`IMPORT_REDIRECTS`, `tools/whatsapp/make_whatsapp_package.py`, whose `PE` class has `import_descriptors / add_string / import_names`).
  `tools/arm32-patcher.py` is the general, table-driven version (profiles `v0_10_4`, `redcon`, `v1_2_8`; commands `detect | probe | patch | verify | dll`;
  checks the expected old bytes of every edit, writes `patch_report.json`). **Cheaper alternative with no patching:** ship a same-named DLL next to the exe - the loader finds the
  app-local copy first (GD's `PhoneAppModelHost.dll`, `PhoneAudioSes.dll`).
* **Runtime IAT hooking** - patch *data* cells (IAT/vtable) with `PAGE_READWRITE`, never code, after verifying the original value; a pointer to Thumb code stored in data needs bit 0 set.
  x104: `arm32_iat_find_cell / arm32_iat_hook_import / arm32_patch_cell` hook `vccorlib120_app!?GetActivationFactoryByPCWSTR@@YAJPAXAAVGuid@Platform@@PAPAX@Z` (the activation router; unknown
  classes fall through to the real OS and are logged as `X104 WinRT FALLBACK`) and `D3D11CreateDevice`. `gd_host.c`: `iat_patch()` over the game DLLs + `hook_RoGAF`, which hands out fake `Windows.ApplicationModel.Package` and
  `Windows.Storage.ApplicationData` factories. Gotcha found on device: vccorlib asks `RoGetActivationFactory` for the generic `IActivationFactory` `{00000035-...}` first and only then QIs for the statics IID, so the
  fake must be returned directly and answer that QI.
* **Fake WinRT/XAML objects** - hand-built COM vtables. x104 registers every slot with `X104_SLOT(Interface, Method, thunk)`, a compile-time signature check against typedefs generated from the SDK WinRT headers
  (`tools/arm32/gen_0104_iids.py`); a wrong signature is a compile error instead of a device crash. Objects must be agile (`QI(IAgileObject)`), and a fake must never invoke a game delegate synchronously (rule R1, deadlocks on the game's AppMain lock). First device step: `start_selftest.bat` (`--selftest-fakes`).
* **D3D11 device-vtable shim** (`tools/badpiggies/l9shim.c`, `l9wrap.inc`, `l9caps.inc`) - replaces `d3d11.dll` for Unity (same 3 exports); calls the game's `D3D11CreateDevice` unchanged and only when that yields a sub-FL10 device creates a 9_x device and wraps it:
  a private **full-length** copy of the device vtable (length *measured* at run time - 121 slots on the Surface 2, 174 on the PC; a short copy crashes inside DXGI), VS/PS bytecode swapped by DXBC checksum from `Data\bp\l9_replacements.bin`
  (not at `CreateInputLayout`), descriptors sanitised to 10Level9 rules, Unity's caps object corrected by `l9caps.inc` (anchored on verified RVAs, content-gated, no-op on another build). PC harness: `tools\badpiggies\build_l9shim_x64.bat`.
* **Small dependency shim DLLs** (freestanding, exports via `.def`): `tools/gd/phoneappmodelhost.c` (`CaptureUiThreadContext` **must fail** with `E_NOTIMPL`, otherwise PPL continuations marshal to a UI context that does not exist; `LoadPackagedLibrary -> LoadLibraryExW`),
  `tools/gd/phoneaudioses.c` (3 exports on mmdevapi; signatures inferred, audio untested), `fmod_null/` (silent FMOD, no imports), `tools/make_arm32_apiset_shims.py` (api-set DLLs RT 8.1 lacks; only `heap-l2-1-0` needed real code - `LocalFree -> HeapFree` - because a pure forwarder broke).
* **VC++ runtime app-local** - packaged builds import `*_app` framework DLLs that a desktop process does not have. Take them from the appx files in `D:\repo\armdeps` (VC110 `Microsoft.VCLibs.110.00_11.0.51106.1_arm`, VC120 Phone `...120.00.Phone_12.0.30113.0_arm`,
  VC140 `...140.00_14.0.32530.0_arm`) or the VS2022 redist (`...\VC\Redist\MSVC\14.44.35112\onecore\arm\Microsoft.VC143.CRT`); clear `IMAGE_DLLCHARACTERISTICS_APPCONTAINER` (`0x1140 -> 0x0140`) where the DLL refuses to load, and rename the imports. On RT 8.1 also ship the SDK `ucrtbase.dll` + `api-ms-win-crt-*`.
  Bad Piggies needs VCLibs.140 >= 14.0.33519 (we only have 14.0.32530), so it ships VC143 and drops the PackageDependency.

### 3. Build toolchain (build on the x64 PC, test on the ARM device)

Defaults point at the original developer's PC; every script honours the env vars `LLVM_BIN SDKROOT SDKVER MSVCINC VCLIB BUILD DIST`:
LLVM `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin` (custom "Windows RT" build, `clang 23.1.1-rt2.1`; its patch notes `BATCH-*.md` are in `D:\repo\llvm-rt`, not in this repo - **whether a stock LLVM works is not recorded**, ask the user for the toolchain),
SDK `D:\Windows Kits\10`, `10.0.19041.0`, ARM libs `...\Lib\10.0.19041.0\um\arm` (incl. `armrt.lib`), MSVC `D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207` (`include`, `lib\arm`).

Freestanding tool/DLL (as in `tools\gd\build_gd_host_arm32.bat`; shims add `/dll /def:x.def /entry:DllMain /subsystem:windows,6.02`):
```
clang.exe --target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wno-microsoft-cast -D_WIN32_WINNT=0x0603 -DWINVER=0x0603 -isystem <MSVC>\include -isystem <SDK>\Include\10.0.19041.0\um -isystem ...\shared -isystem ...\ucrt -c x.c -o x.obj
lld-link.exe /machine:arm /subsystem:console,6.02 /entry:mainCRTStartup /nodefaultlib /brepro /out:x.exe x.obj <UM>\kernel32.lib <UM>\armrt.lib
llvm-readobj.exe --coff-imports x.exe          (tools must import only KERNEL32 [+USER32/ole32]; --file-headers: Machine ARMNT 0x1C4)
```
* **`win32craft.dll` is the exception** (`build_arm32.bat`): not freestanding - links `libcmt`, `legacy_stdio_*`, `ucrt.lib`, `vcruntime.lib` (so the package must carry `vcruntime140.dll` + UCRT), `/def:win32craft_arm32.def` (exports `Win32Bootstrap`, `XInputGetState`, HSTRING/Ro* forwarders), `/stack:0x200000,0x1000 /opt:ref /opt:icf`.
* Freestanding rules: your own `memset/memcpy/memcmp` (the compiler emits them); if you divide integers, `armrt.lib`'s `__rt_*` helpers need `__security_cookie`, `__security_check_cookie`, `__GSHandlerCheck` stubs (copy from `tools/m0probe/m0probe.c` ~line 92 or `l9shim.c` ~line 62); `uuid.lib/dxguid.lib` or inline GUIDs; `-DCOBJMACROS` for C COM calls; hard-float ABI = floats/HFAs travel in `s` registers (GD `set_WindowBounds` Size in `s0/s1`).
* `set "LINK="` in scripts (lld-link reads `%LINK%` as extra arguments). Always package last: the host embeds build date/time, so every build has a new hash.
* **Never run ARM binaries on the x64 PC.** Proof without a device: same source built for x64 (`tools\m0probe\build_m0probe_x64.bat`, `build_l9shim_x64.bat`), x86 mock harnesses (`python tools/arm32/try_0104.py --selftest --integration`: ARM + x86 compile, 130-check core test, mock game with all real units; `diff_0104_log.py` old-vs-new log diff; `test_osk.py`, `test_logcap.py`), and `python tools/arm32/check_x86.py` (x86 output of `win32_host.c` identical to `arm32/orig/`; all ARM code is under `#ifdef WIN32CRAFT_ARM32`). *Your POCO X3 Pro (Win11 ARM64) does run ARM32 binaries via WOW, so you can test locally - but it is not the Surface 2 / RT 8.1 environment; a pass there does not prove them.*

### 4. WARNING - clang-ARM `va_start` (crashed two device runs)

MSVC's `<stdarg.h>` (pulled in by `windows.h`) defines `va_start` as "address of the last named parameter + size", which assumes MSVC argument homing. **clang on ARM does not home r0-r3 that way, so every variadic argument is garbage**:
logs print `(null)`/junk, and a misread `%s` faults. It crashed `m0probe` on the Surface 2 (`arm32/device_logs/m0probe_surface2_varargs_crash.log`) and the `l9shim` logger inside Bad Piggies' `D3D11CreateDevice`. **Always** use
`__builtin_va_list / __builtin_va_start / __builtin_va_arg / __builtin_va_end` (examples: `X104VaList` in `win32_host_arm32_0104_core.inl`, `GD_VA` in `gd_host.c`, `L9_VA_LIST` in `l9shim.c`, which also `#undef`s the MSVC macros). Before shipping any new ARM file: `grep -n "va_start" <file>` must show only `__builtin_va_start`.

### 5. Diagnostic tooling - run these first on any new device

1. **`tools/loadcheck/loadcheck.c`** - `build_loadcheck_arm32.bat` -> `dist-arm32\loadcheck.exe` (KERNEL32+USER32 only). `loadcheck.exe [folder] [/nopause]` (default: its own folder) parses the import table of each module in its hard-coded list
   (`modules[]` in `mainCRTStartup`: Win32Craft exes/DLL, fmod, VC120/VC140, UCRT, GD DLLs + VC110 + shims; missing ones are "skipped" - add your game's DLLs there and rebuild), `LoadLibrary`s every imported DLL (folder = DLL directory) and `GetProcAddress`es every import. Output + `loadcheck.txt`:
   `[ok] api-ms-... served by <path>` (folder = our shim, System32 = OS-native), `[FAIL] LoadLibrary`, `[MISSING] X imports DLL!fn` (= the Windows "entry point not found" dialog), `==== RESULT: n problem(s) ====`, exit code 1 on problems. Limits (from the source): normal import table only (**delay imports are not checked**), and it loads each module's *imports*, not the module itself. Packaged for GD by `tools/gd/make_gd_loadcheck_package.py` (Surface 2: 0 problems).
2. **`tools/m0probe`** - `tools\m0probe\build_m0probe_arm32.bat` then `python tools\m0probe\make_m0probe_package.py` -> `dist-m0probe\` (`run_probe.bat`, ~2-3 min) -> `m0probe.log` + `.res`. The parent relaunches itself per test, so a driver crash/hang is logged (`CRASHED`/`TIMEOUT`) without losing the rest. Reads: adapters (DXGI, `d3d10warp.dll`, vendor driver modules), **true max feature level HW vs WARP**, whether each game's precompiled shaders create on a `{9_1}` device (Redcon 10, GD 30 containers/18 used, Bad Piggies 113), sprite/blend read-back, vtable copy + swap chain, fill rate. Grep the log for `DIFFERS` and `UNEXPECTED`; the SUMMARY is at the end. Switches: `--quick`, `--no-spawn`, `--summary-only`, `--test selftest_fmt`. Full test table: `tools/m0probe/README.md`.
   Surface 2 result (`arm32/device_logs/m0probe_surface2.log`): Tegra 4 is a **real GPU**, HW max 9_1 (WARP 11_1), 10/10, 30/30 (18/18), 113/113, blend 5/5, 1055 vs 37.7 Mpixel/s. **Trap:** its SUMMARY still prints "`-> SOFTWARE` ... no GPU acceleration" because the D3D10Level9 layer names the adapter "Software Adapter" - trust `GPU vendor driver module loaded: YES` and the HW/WARP ratio. No m0probe log from the POCO X3 Pro exists yet.
3. Also: `tools/kbdprobe` (`build_kbdprobe_arm32.bat`, touch-keyboard lab, notes in `arm32/OSK_NOTES.md`), and the logs themselves - `win32craft.log` (`X104 ...`, `ARM32 osk:` prefixes), `gd_host.log`, `l9shim.log` (packaged app: in the package data folder `...\Packages\BadPiggiesRebooted_x0xr9mv5edyrp\`, seen under `AC\Temp\` and `AC\`).

### 6. Repo map and project constraints

| Where | What |
|---|---|
| `win32_host.c`, `api_compat.c`, `win32_compat.h`, `win32_host_arm32.h` (generated, do not edit), `build_arm32.bat` | host DLL; 1.2.8 path + shared code |
| `win32_host_arm32_0104_*.inl`, `arm32/v0_10_4/` (HOST_DESIGN, RECON_SUMMARY, HUD_INVESTIGATION, `facts/`, `units/`) | x104 host (0.10.4 + Redcon profile) |
| `tools/arm32-patcher.py`, `tools/make_0104_package.py [--game redcon] [--build]`, `tools/make_arm32_device_package.py` | exe patcher, packagers (`dist-0104/`, `dist-redcon/`, `dist-arm32/`) |
| `tools/arm32/` (`arm32re.py` RE toolkit, `try_0104.py`, `check_x86.py`, `gen_*.py`, tests), `arm32/` (`RE_GUIDE`, `PORT_RULES`, `facts/`, `OSK_NOTES`, `device_logs/`) | RE + proof tooling, 1.2.8 facts, real device logs |
| `games_recon/` (`GAMES_PLAN.md`, `redcon_*`, `geometry_dash_*`, `bad_piggies_*`, `bp_*.md`, `HANDOFF_FOR_BADPIGGIES_DEV.md`) | recon of the three new games (all STATUS: DONE) |
| `tools/gd/` + `GD_BUILD.md`; `tools/badpiggies/`; `tools/whatsapp/`, `whatsapp/`, `wp81/`, `tools/wp81/wp81scan.py` | Geometry Dash; Bad Piggies; Sup + the abandoned-WP8.1-app method |
| `tools/loadcheck`, `tools/m0probe`, `tools/kbdprobe`, `fmod_null/` | diagnostics, silent FMOD |
| `HANDOFF_ARM32.md` (chronological log), `ARM32.md` (user docs) | history, decisions, device results |

Constraints (standing rules of this project):
1. **Build on x64, test on the ARM device** (section 3); the x64 PC never executes ARM code.
2. **Game folders, `*.appx`, `*.xap`, `*.pdf` never go into git or the public source zip.** `tools/make_source_package.py` drops top-level `Minecraft_*, WhatsApp*, Redcon_*, Geometry_Dash*, BadPiggies*, build-*, build_*, dist-*` and the game-derived `games_recon/bad_piggies_shader_proto/{orig,auto,hlsl,build}`; `.gitignore` lists `/*.zip /*.appx /*.xap /*.pdf`.
   The run-packages (`dist-*`, e.g. `dist-redcon`, `dist-gd-host`) *do* contain extracted game DLLs/assets - personal test builds, never published. Packagers only read the game folder/appx (the Redcon package is built from a clean copy unpacked from the appx, not the repo folder that holds a third-party-patched `Redcon.exe`).
3. **Do not force FL 9_1 as a permanent default on capable GPUs** (the developer's standing rule; a Lumia-class FL 9_3 device has a broken 9_1 path). Today 1.2.8 and 0.10.4 still clamp to 9_1 (override `--feature-level=9_1..11_1|game` or `WIN32CRAFT_FEATURE_LEVEL`) - an auto policy is the open item; Redcon passes the game's own list; `gd_host.c` and `l9shim` ask for the device's real level.
4. x86 build output stays byte-identical; OS-specific fixes are gated to their OS and proven not to change the others (old-vs-new simulation).
5. Write build scripts that contain backslashes with a file editor, not shell heredocs (they collapse `\\`); run `makeappx` from PowerShell (Git Bash rewrites `/d /p /o`).

---

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

---

## Bad Piggies Rebooted 1.0.0.0 (Unity 2018.4.36f1, IL2CPP, UWP ARM32) - the `l9shim` feature-level 9_1 route

### 1. Status

| Item | State |
|---|---|
| Surface 2 (Windows 10 build 15035 ARM32, Tegra 4, hardware FL 9_1) | **DONE.** Renders and runs on the real GPU. Menus and gameplay confirmed on the device. |
| POCO X3 Pro (Windows 11 ARM64) | **Never run.** Details in section 2. |
| Surface RT / Windows RT 8.1 (Tegra 3) | **Not possible with this package, nothing built.** See section 4. |
| Console errors that remain | 3 C# exceptions. All three are bugs in the "Rebooted" fan mod, not in the port (section 4). |

This game is **not** hosted by `win32craft.dll`. It is a real packaged UWP app (a plain CoreWindow `IFrameworkView`, no XAML). Its manifest `MinVersion` is 10.0.15035, which is exactly the Surface 2, so it registers natively in Developer Mode. The only thing wrong on this hardware is Direct3D. Unity 2018.4 demands feature level 10_0 and the Tegra tops out at 9_1. That is why `l9shim` exists.

### 2. How to run it

The package is `dist-badpiggies/BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft.zip` (91.0 MB, 272 MB unpacked, 169 files, sha256 `ad249622a2daf31b2c83f709657913ea6993e178abe09bcdc9fccaf3ee383a3e`). Every file's SHA-256 is in the package's `MANIFEST.txt`, and `PATCH_REPORT.txt` lists every change.

1. Turn on Developer Mode (Settings > Update & security > For developers).
2. Extract the zip to the Desktop or `C:\Apps\...`, **not to a drive root**. AppX deployment fails with 0x80073CF9 or 0x80070020 there. `install.bat` copies itself to `Desktop\BadPiggiesRebooted` when it detects a root.
3. Run `install.bat`. It runs `install.ps1`, which does `Add-AppxPackage -Register .\AppxManifest.xml -ForceApplicationShutdown`. On failure it writes `appxlog.txt`.
4. Start **Bad Piggies::Rebooted** from the Start menu. The app runs from the extracted folder, so do not move or delete that folder afterwards. `uninstall.bat` removes it.

**Logs.** The game folder is read-only to an AppContainer, so the logs are in the package data folder `%LOCALAPPDATA%\Packages\BadPiggiesRebooted_x0xr9mv5edyrp\`:
- `AC\` holds `l9shim.log`. It was under `AC\Temp\` on the first run, so search `AC\` recursively. Dumped `l9_miss_*.dxbc` files land beside it.
- `TempState\UnityPlayer.log` is Unity's own log.
- `LocalState\` holds the saves.

`README_DEVICE.txt` and `install.ps1` tell the user to look beside the exe or in `%TEMP%`. That is imprecise, so use the paths above.

**Reading `l9shim.log` on a new device.** Check these lines first:
- `created a 9_1 device ... wrapping it`, then `shader table loaded: 121 replacements (318668 bytes)`, `device wrapped`, `l9caps: armed` and `l9caps: caps object ... corrected to 9_1`. This is the healthy Tegra path. There should be no `MISS` lines.
- `D3D11CreateDevice -> 11_x on the game's own request; FL10+, the shim does nothing`. The GPU is capable and Unity runs stock. The shim and the table are then unused.

**POCO X3 Pro.** Untested. Route-A analysis (`games_recon/bad_piggies_bp-app.md`, F7) says ARM32 UWP packages run on ARM64 Windows and that the Adreno 640 should hand Unity a FL10+ device, so the shim should step aside. All of that is **unverified**. Open questions: whether `Add-AppxPackage -Register` accepts the `ProcessorArchitecture="arm"` package on Win11, audio, and touch. Input comes from the real OS, because no Win32Craft host is involved. If it runs, it also settles the mod's `MenuContraptionManager` NRE (section 4).

The zip contains the third-party game's assets and DLLs. Treat it as a personal run-package. Never commit it, and never put it in the public source zip. The same goes for `BadPiggiesRebooted_1.0.0.0_ARM.appx` in the repo root.

### 3. How it works

**Package transform** (`tools/badpiggies/make_badpiggies_package.py`). The game's code and assets are untouched except for the import tables of three PEs:

| Binary | Import redirects |
|---|---|
| `BadPiggiesRebooted.exe` | `VCRUNTIME140_APP`, `vccorlib140_app` -> plain `vcruntime140`, `vccorlib140` |
| `UnityPlayer.dll` | the same two, plus `MSVCP140_APP` -> `msvcp140`, and **`d3d11.dll` -> `l9d3d11.dll`** (the shim) |
| `GameAssembly.dll` | `VCRUNTIME140_APP`, `MSVCP140_APP` -> plain names |

- The stock manifest needs `Microsoft.VCLibs.140` 14.0.33519, which cannot be supplied. The packager therefore ships the VS2022 ARM VC143 runtime app-local (8 DLLs) and removes the `PackageDependency` from `AppxManifest.xml`. The Universal CRT is in-box on build 15035.
- It deletes `AppxSignature.p7x`, `AppxBlockMap.xml`, `AppxMetadata`, `[Content_Types].xml` and `StoreManifest.xml`. The last declares `D3D11_HWFL_10_0`. Whether a sideloaded package evaluates it is unverified, so it is removed as insurance.
- It checks `UnityPlayer.dll` against sha256 `e3530fd5d6cc0e431e5edb69623b7c18e1a57a8013914d958c5f02711cb03793` before patching. The shim's caps anchors are tied to this exact build.
- It adds `l9d3d11.dll` and `Data\bp\l9_replacements.bin`.
- It reuses the `PE` import-patching class from `tools/whatsapp/make_whatsapp_package.py`.

**Two gotchas, both handled by the packager:**
- **Percent-encoded part name.** The appx stores `Data/Resources/unity%20default%20resources`. A naive unzip keeps the `%20`. Unity then cannot load its builtin resources, every builtin shader (including `Internal-ErrorShader`) resolves to NULL, and the game crashes. The packager runs `urllib.parse.unquote` on every path segment, so the file lands as `unity default resources`. `dist-badpiggies/fix_badpiggies_resource.ps1` repairs an already-extracted folder in place.
- **APPCONTAINER bits and FMOD.** The AppContainer bit is deliberately left **set** on the exe, `UnityPlayer.dll` and `GameAssembly.dll`, because the game stays a real UWP app. FMOD is statically linked inside `UnityPlayer.dll` (`bad_piggies_bp-platform.md`, F5) and the package contains no `fmod*.dll`. The "clear the fmod APPCONTAINER byte" recipe belongs to Geometry Dash and Minecraft 1.2.8. Do not apply it here.

**The shim** (`tools/badpiggies/l9shim.c`, `l9wrap.inc`, `l9caps.inc`, `l9shim.def`). It is built freestanding with no CRT and imports only KERNEL32. It loads the real `System32\d3d11.dll` dynamically and exports `D3D11CreateDevice` plus pure forwards of `CreateDirect3D11SurfaceFromDXGISurface` and `CreateDirect3D11DeviceFromDXGIDevice`.

- **Device policy.** `D3D11CreateDevice` first makes Unity's own call unchanged. If that returns FL 10_0 or higher, the shim steps aside. This follows the project rule "never force a feature level". Otherwise it releases that device and creates a `{9_3, 9_2, 9_1}` device, then wraps it.
- **Vtable wrap.** The shim copies the device vtable into a private full-length copy. The length is measured at run time by counting entries that point into `d3d11.dll`. A short copy crashes inside DXGI. It then replaces 14 slots:
  - 5 `CreateTexture2D`, 7 `CreateShaderResourceView`, 9 `CreateRenderTargetView`, 10 `CreateDepthStencilView`
  - 12 `CreateVertexShader`, 15 `CreatePixelShader`
  - 20 `CreateBlendState`, 21 `CreateDepthStencilState`, 22 `CreateRasterizerState`, 23 `CreateSamplerState`, 45 `CreateBlendState1`
  - 29 `CheckFormatSupport`, 33 `CheckFeatureSupport`, 37 `GetFeatureLevel`
- **Shader swap.** The key is the DXBC checksum at bytes 4..19 of the bytecode Unity passes. The lookup is a linear scan of `Data\bp\l9_replacements.bin` (format `L9SW`: header, then 24-byte entries of checksum, offset and size, then the blobs). The table has **121** entries: 113 from the original needed-set analysis (including 4 hand-trimmed tier-1 lit pixel shaders and the 2 internal blits from `UnityPlayer.dll`) plus 8 shadow-caster programs. Of those 8, four were seen as MISS on the Surface 2 and four are keyword siblings. Each replacement is a `vs/ps_4_0_level_9_1` blob that keeps Unity's cbuffer slots and offsets, `t#/s#` slots and signatures, because Unity never reflects bytecode. `CreateInputLayout` is deliberately **not** hooked, since Unity passes a signature-only blob there and a checksum swap would miss. A miss logs `CreateVertexShader/PixelShader MISS`, dumps the blob as `l9_miss_vs_N.dxbc` or `l9_miss_ps_N.dxbc` (first 8), and the object stays invisible.
- **Sanitisers** (10Level9 rules):
  - Textures: typeless colour -> UNORM; 32-bit depth -> `R24G8_TYPELESS`; `SHADER_RESOURCE` dropped if the driver lacks D3D9 shadow support; A8 forced to 1 mip; `MISC_SHARED` cleared on RGBA8.
  - Views: `R32_FLOAT_X8X24` -> `R24_UNORM_X8`; D32 -> D24S8 or D16.
  - Samplers: anisotropy capped at 2, `MIRROR_ONCE` -> `MIRROR`.
  - Rasterizer: `DepthClipEnable=TRUE`, `DepthBiasClamp=0`.
  - Blend: alpha-to-coverage and independent blend off, separate-alpha factors collapsed, RT0 copied to all slots.
  - Depth-stencil: back face copied from front face.
  - **SRV auto-recovery.** A failing `CreateShaderResourceView` is retried with a NULL desc. This fixes the `id=41` `TexturesD3D11.cpp:388` E_INVALIDARG failure (an A8 texture).
- **Caps correction** (`l9caps.inc`). Unity's caps init (`UnityPlayer` `sub_1007434`) has no 9_x row in its table, so a 9_1 device is treated as FL 12_1. Unity would then bind 32-bit index buffers (which can remove the device), use MRT and use instancing. The shim fires once from its `CheckFeatureSupport` hook on Unity's last caps query (`D3D11_FEATURE` 0xf). It overwrites Unity's caps object (pointer at `UnityPlayer` base + `0x0298ed40`) with 9_1 values: max texture 2048, cube 512, 1 render target, no 32-bit indices, no MRT, no 3D textures and no mirror-once. It only runs in the 9_x path. It is armed only if two anchor instruction sequences match: RVA `0x0100776c` = `86 f8 04 91` and RVA `0x0100763a` = `c6 f8 bc 30`. I checked both against the shipped `UnityPlayer.dll`, and they match. A content gate on the caps object also guards the writes. On any other `UnityPlayer` build it is a silent no-op.

**Shader table pipeline** (x64 Python plus `d3dcompiler_47` via ctypes; see `games_recon/bad_piggies_shader_proto/README.md`). `tools/scan1.py`, `scan3.py`, `refs.py`, `batch.py` and `table.py` extract and translate every program. `tools/make_table.py` packs `auto/l9_replacements.bin` and `.txt`, including the `auto/shadowcasters/` blobs. `tools/build_missing_l9.py` builds a replacement for any program by checksum (`--ck`, or `--preset misses|shadowcasters|all-mechanical`, optionally `--merge-out`). Each one is proven equal to Unity's original on 24 random inputs with the repo's SM4 interpreter, has identical signatures, and creates on an x64 WARP device limited to FL 9_1. The d3dcompiler path is set in `tools/d3dc.py`.

**Build and test.** Run these from the repo root:
```
tools\badpiggies\build_l9shim_arm32.bat            -> dist-badpiggies\l9shim.dll   (15,872 B, ARMNT, imports KERNEL32 only)
python tools\badpiggies\make_badpiggies_package.py -> dist-badpiggies\BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft\ (+ .zip)
tools\badpiggies\build_l9shim_x64.bat              -> build-badpiggies\l9shim_x64.dll + l9shim_test_x64.exe (PC harness)
```
- The ARM build uses clang `--target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -fno-stack-protector`. It links with `lld-link /dll /machine:arm /nodefaultlib /def:l9shim.def` against `kernel32.lib` and `armrt.lib`. Stubs for `__security_cookie`, `__security_check_cookie` and `__GSHandlerCheck` are defined in `l9shim.c` so `armrt.lib` links.
- Overridable environment variables: `LLVM_BIN SDKROOT SDKVER MSVCINC BUILD DIST`, defaulting to `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin`, `D:\Windows Kits\10`, SDK 10.0.19041.0 and MSVC 14.44.35207.
- The packager also needs `BadPiggiesRebooted_1.0.0.0_ARM.appx` in the repo root, `games_recon/bad_piggies_shader_proto/auto/l9_replacements.bin`, and the VC143 ARM redist (default `D:\Program Files\vs22buildtools\VC\Redist\MSVC\14.44.35112\onecore\arm\Microsoft.VC143.CRT`, override with `WIN32CRAFT_REDIST_VC143`).
- The PC harness is x64 only. Set `L9SHIM_FORCE_9X=1` and run `l9shim_test_x64.exe` from a folder that has `Data\bp\l9_replacements.bin` and `Data\bp\orig\`. It forces the 9_x path on a PC GPU, and exit code 0 means every check passed. The `l9shim_selftest_caps` export checks the caps values. **Never run the ARM binaries on the x64 PC.**
- To swap in a new shim on an installed copy, use `dist-badpiggies/update_badpiggies_dll.ps1` with the new `l9d3d11.dll` beside it.
- **Recurring bug.** MSVC `<stdarg.h>` `va_start` is wrong under clang-ARM. The shim's logger hit it and the first device launch crashed. `l9shim.c` redefines the macros with `__builtin_va_*` under `__clang__`. Do the same in any ARM32 code.

Design and evidence: `games_recon/bad_piggies_hw-design.md` (route "S+"), `-hw-critic.md` (corrections F1, F2, F6), `-hw-unitygfx.md`, `-hw-shaders.md`, `bp_missing_shaders.md`.

### 4. What is left

- **Required: nothing for the Surface 2.** The remaining items are optional.
- **The 3 console exceptions are mod bugs, not the port's.** They are fully diagnosed, with fixes, in `games_recon/bp_managed_exceptions.md` and `games_recon/HANDOFF_FOR_BADPIGGIES_DEV.md`, which is a separate document for the mod author:
  1. `PartListing.GetGreyMaterial` throws `ArgumentOutOfRange`. The material `IngameAtlas4` was never registered in `AtlasMaterials`, and the pass-2 loop is bounded by `normalMaterials.Count` (5) while it indexes `renderQueueMaterials` (3). It fires only for **locked** parts with `IngameAtlas4` icons, such as any fresh save. The part list in a level is then incomplete. This is the one visible effect.
  2. `MenuContraptionManager.CreateContraption` throws a NRE. It is most likely `FindPig()` returning null, but the exact null is **not pinned**. The cheapest decisive test is the same appx on a real-GPU device, such as the POCO. If the NRE reproduces there, it is the mod.
  3. Three missing-script warnings (`BackgroundBox` x2, `REConsoleInterface`) are harmless.
- **Optional binary patch of `GetGreyMaterial`** to stop exception 1:
  - Location: `GameAssembly.dll`, methodIndex 44635 at VA `0x1020fefc` (ImageBase `0x10000000`, Thumb-2). The pass-2 bound reads `AtlasMaterials` `+0x14` (`normalMaterials`) but should read `+0x1c` (`renderQueueMaterials`), or the method should fall back to `grayMaterial` (`this+0x74`).
  - The exact instruction bytes are **not yet located**. The Thumb-2 disassembly recipe and addresses are in the appendix of `bp_managed_exceptions.md`.
  - This would be the first change to game code in the package. Guard it with an expected-bytes check like `l9caps`. Registering `IngameAtlas4` in the serialized `AtlasMaterials` asset is the cleaner fix and belongs to the mod author.
- **End-of-project Tegra 3 / Windows RT 8.1 test.** Nothing is built or run.
  - This package cannot register on RT 8.1, because the Windows 10 manifest and app model are absent there.
  - The only route is a Win32Craft host (routes B and C in `bad_piggies_bp-app.md`, F8-F10). That means a patcher that clears APPCONTAINER on the three PEs, a fake `CoreApplication` and `CoreWindow` with **active** event delivery (the 1.2.8 host's are inert), a `d3d12.dll` stub, an app-local UCRT and api-set shims.
  - `l9shim` and the table should be reusable as is, and the shim already exports the two interop forwards RT's `d3d11.dll` probably lacks. The Tegra 3 is expected to be FL 9_1 as well (not measured in this repo), and the shim has never run on it.
  - The effort is unmeasured. The analysis estimates "weeks".
- **Known gaps:**
  - No DXGI `Present` hook (milestone M3), so there is no fps counter or device-lost handling.
  - `CreateTexture2D` over 2048 is only logged. Whether such textures exist is not recorded.
  - 118 shader programs cannot be converted mechanically: 54 comparison-sampler pixel shaders, 34 over the ps/vs_2_0 limits, plus instancing, stereo and loop vertex shaders and others. None has been requested in the logged runs. The IMGUI rounded-rect shader (`ddx`) is also unconverted.
  - To add a program, take the dumped `l9_miss_*.dxbc`, read its checksum from bytes 4..19, and feed it to `build_missing_l9.py --ck`. Then re-run `make_table.py` and the packager.
  - The manifest declares no capabilities, so there is no `internetClient` and the game's PlayFab and Rovio HTTPS calls fail. It falls back to offline.
  - Audio and frame rate on the Surface 2 are not recorded in the repo notes.
- **Stale text to fix when repackaging.** `README_DEVICE.txt` and the packager docstring say "113 replacements", but the shipped table has 121. The log location advice is imprecise (see section 2).

---

## Geometry Dash 2.0.1.0 (WP8.0 Silverlight + cocos2d-x/ANGLE)

GD is a different kind of port from Minecraft PE and Redcon. It is **not** hosted by the x104 fake-object XAML host. The original app is a managed Silverlight shell (a `DrawingSurfaceBackground` compositor) around one native WinRT component, `GeometryJumpComponent.dll`. That component wraps cocos2d-x (`libcocos2d.dll`), ANGLE for WP8 (`libEGL_phone.dll` and `libGLESv2_phone.dll`), FMOD Ex (`fmodex_80_arm.dll`) and libcurl/OpenSSL. We replace the Silverlight shell and compositor with a small standalone native host, `tools/gd/gd_host.c`. The living build log is `tools/gd/GD_BUILD.md`. The decoded contracts are in `games_recon/geometry_dash_gd-{shell,component,render,deps,platform}.md` (cited below as gd-component F8 and so on). The go/no-go summary is in `games_recon/GAMES_PLAN.md`.

### 1. Status: PLAYABLE (2026-10-25) — boots to the menu, plays levels, smooth touch

> **Full current write-up: `games_recon/HANDOFF_GD_IMPROVEMENTS.md`** — the real root cause, the fix that made it
> playable, the touch-latency fix, the built-in diagnostics, and what to improve next. Read that alongside this section;
> the milestone table and "how it works" below remain accurate background.

GD now **boots to the menu and plays** on the Surface 2 (Win10 15035 ARM32, Tegra 4, FL 9_1): the real menu, level
select, and gameplay render upright; music + sound effects play (FMOD via the `PhoneAudioSes` shim); touch and keyboard
work, and after the touch-latency fix **jumps are responsive**. Verified on device — a level was played to death.

| Milestone | State |
|---|---|
| M0a import-chain check (`loadcheck`) | Done. 0 problems on the Surface 2. |
| M0b shaders at feature level 9_1 | Done. `tools/m0probe` created 30/30 game shader containers at 9_1 (the 18 used). |
| **M1 headless host** | **Done.** Renders the real splash at FL 9_1. |
| **M2 window + present-with-rotate + loop** | **Done + verified.** Landscape 800×480 window, portrait RT rotated 90°, upright + stable. |
| **M3 input** | **Done + verified.** Touch (raw `WM_TOUCH`, low latency) + mouse + keyboard. |
| **M4 on-demand WinRT** | Boot-path done (Package/ApplicationData); the rest (save, Launcher, InputPane…) on demand — not needed to play. |
| **M5 audio** | **Done + verified.** FMOD via the `PhoneAudioSes` shim; music + SFX confirmed. |
| **M6 package + device matrix** | Surface 2 done. Surface RT 8.1 / POCO X3 Pro not yet tested. |

**The fix that made it playable (full detail in the improvements doc):** the game told ANGLE to target feature level
**9_3** (`CreateWinrtEglWindow`, GeometryJumpComponent `sub_36390`), but the Tegra device is real **9_1**. ANGLE driving a
9_1 device as 9_3 removed the GPU device (`DXGI_ERROR_DRIVER_INTERNAL_ERROR`) during a texture upload, and the host kept
presenting the stale frame — which *looked* like an 80% loading stall for days of debugging. The host now **clamps the
requested ANGLE feature level to the device's actual maximum** at runtime (in-memory code patch, never edits the shipped
DLL; per-device, not a blanket forced 9_1). The host also has built-in device-removal / health diagnostics (`[health]`
line, device-removal pinpoint) — use them first on any new device.

What is **left / not verified:**
- Only the Surface 2 has been played. Surface RT 8.1 (Tegra 3) and your POCO X3 Pro (Win11 ARM64) have not run it — expect
  the same FL-clamp to apply; run `loadcheck` there first.
- The remaining on-demand WinRT shims (saving high scores/settings, `Launcher`, on-screen keyboard) are not implemented —
  the game is playable without them.
- The FL clamp currently only *lowers* the level (9_1/9_2 devices); on a ≥9_3 GPU it leaves the game default (correct
  there). See the improvements doc for generalizing it.
- `gd_frame_900.bmp` (a captured gameplay frame) and the device logs are the proof; the raw logs aren't archived under
  `arm32/device_logs`.

### 2. How to run it on an ARM Windows device

**Package.** In the hand-off bundle it is `runnable/GeometryDash_M1_host.zip`. It is the same content as the repo's `dist-gd-host.zip` (48,478,324 bytes) and `dist-gd-host/` folder (57 MB, 328 asset files). The zip has no top-level folder, so extract it into a new empty folder. It contains:
- `gd_host.exe` (ARM32 console exe, 14 KB, imports KERNEL32 only).
- The 8 game DLLs, with the APPCONTAINER bit cleared in `fmodex_80_arm.dll`.
- The app-local VC110 trio `msvcr110.dll`, `msvcp110.dll` and `vccorlib110.dll`.
- The two shims `PhoneAppModelHost.dll` and `PhoneAudioSes.dll`.
- `Assets\Resources\` and `run_gd_host.bat`.

It contains extracted game DLLs and assets. Keep it private; it must never go in the public source zip or git.

**Steps.**
1. Extract the zip to a writable folder, for example `C:\gd\`, and keep the folder intact. The exe's folder is the game's "install location" and must contain `Assets\Resources\`.
2. Run `run_gd_host.bat`. It does `cd /d "%~dp0"`, runs `gd_host.exe`, and pauses.
3. Wait. The first `Draw` blocks for seconds while ANGLE starts and `CCApplication::run` loads the assets, and nothing prints in between. Do not kill it.
4. Collect the results from `%LOCALAPPDATA%` (`C:\Users\<you>\AppData\Local`):
   - `gd_host.log` is **appended** across runs and flushed per line, so it survives a hard crash. Delete it first for a clean run.
   - `gd_frame.bmp` is the first rendered frame.
   - `GeometryDash\` is the game's save folder.

**Success looks like** these lines in `gd_host.log`:
- `D3D11 device created, feature level 0x...`
- `Draw #0 hr=0x0`
- `wrote gd_frame.bmp (480x800)`
- `Draw #7 hr=0x0`
- `==== gd_host M1 done`

The host asks for the device's real maximum feature level (list 11_1 down to 9_1, BGRA, with a WARP fallback) and does not force 9_1. The Surface 2 gives `0x9100`. On the POCO X3 Pro expect a level of 9_3 or higher. That is unmeasured, and so is whether the ARM32 D3D11 driver path works there.

**If it fails:**
- The log stops at the last completed step.
- `!!! CRASH code=... in module+off` comes from the unhandled-exception filter. C++/CX fail-fast bypasses that filter, so a silent stop usually means a WinRT call threw.
- Every call the component makes into a host fake is logged, and unexpected ones say `UNEXPECTED`.
- For DLL load problems use `dist-gd-loadcheck.zip` (`run_loadcheck.bat`). It should print `RESULT: 0 problem(s)`.
- Never run any of the ARM binaries on the x64 PC.

### 3. How it works

**The DrawingSurface contract.** `gd_host.c` `mainCRTStartup` implements this order (gd-component F11):
1. `RoInitialize(MTA)`. Create the D3D11 device with BGRA support. Create a **portrait 480x800** BGRA8 render target with an RTV (`RENDER_TARGET|SHADER_RESOURCE`).
2. `LoadLibrary("GeometryJumpComponent.dll")`, then `DllGetActivationFactory`.
3. `BroswerEventHelper` statics {6AF73B9D-6C05-39B0-90FC-1CA5238260B1}, slot 6 `SetCallback(ICallback)`. The class name is misspelled that way in the game.
4. `Direct3DInterop` factory {E8D96887-B552-373B-AFBF-838B9195D4FD}, slot 6 `CreateInstance(0)` (0 = Landscape). Then QI {D8485BA8-71AD-35C3-83DF-5B244DA34AA6}:
   - slot 19 `set_WindowBounds` takes a `Size{800,480}` passed in VFP registers s0/s1.
   - slots 20/21/22 are `SetCocos2dEventDelegate`, `SetCocos2dMessageBoxDelegate` and `SetCocos2dEditBoxDelegate`. They must be set **before** the first `Draw`; the component copies them to the view only after `run()` returns.
5. Slot 6 `CreateContentProvider`, then QI `IDrawingSurfaceBackgroundContentProviderNative` {262C1892-975A-45AA-8FE7-F25C2C4088E3}. Separately QI {171B7EF7-A96F-4E37-B936-7A0B10F35163} and call `SetManipulationHost` (slot 6).
6. `Connect(runtimeHost, device)` is slot 3. Then per frame, on one thread, `PrepareResources` is slot 5 and `Draw(device, ctx, rtv)` is slot 6.

**One `Draw()` is one whole cocos2d frame.** It drains queued input, renders, and ends with `RequestAdditionalFrame`. The very first `Draw` also boots the game. ANGLE reuses the **host's** D3D11 device. There is no `D3D11CreateDevice`, HWND or swap chain inside the component. ANGLE renders offscreen and copies into the host RTV on `eglSwapBuffers` (gd-render F9). The game draws landscape content into a portrait-shaped texture with its own 90-degree projection rotation, so the **host must rotate when presenting**.

**Hand-written COM fakes.** These are static C vtables in `gd_host.c`. AddRef and Release are no-ops, and every slot is logged:
- `ICallback` {009BC214-16B9-37D2-8A4F-DD7628D0B7CB}, 5 methods. It must always return `S_OK`: a failure HRESULT makes the component throw a C++/CX exception.
- The 3 delegates (event {0ED04E82-...}, message box {4DBE8C04-...}, edit box {3C7D6A8A-...}). `Invoke` is slot 3.
- `IDrawingSurfaceRuntimeHostNative`: only slot 3 `RequestAdditionalFrame` is used.
- `DrawingSurfaceManipulationHost`: a 12-slot IInspectable. `add_PointerMoved`, `add_PointerPressed` and `add_PointerReleased` are slots 6, 8 and 10. The component's three TypedEventHandlers are stored in `g_ptr_moved`, `g_ptr_pressed` and `g_ptr_released` for M3.

All fakes answer `IAgileObject`. If they did not, the component would capture COM contexts.

**The WinRT trap, solved.** cocos2d finds `Assets\Resources\` through `Package.Current.InstalledLocation.Path` and its save root through `ApplicationData.Current.LocalFolder.Path`. Without package identity, both fail-fast in a plain Win32 process. The host patches the IAT of `RoGetActivationFactory` (`iat_patch`) in `vccorlib110`, `libcocos2d`, `libEGL_phone`, `libGLESv2_phone` and `fmodex_80_arm`. `hook_RoGAF` returns fake statics for `Windows.ApplicationModel.Package` and `Windows.Storage.ApplicationData`. The chain is `IPackageStatics` {4E534BDF-...} to `IPackage` to a `StorageItem` whose `IStorageItem.get_Path` is slot 12. The paths are the exe folder and `%LOCALAPPDATA%\GeometryDash`. Every other class is forwarded to combase and logged; for example `Windows.System.Threading.ThreadPool` works from the real OS. Lesson learned: vccorlib asks for the generic `IActivationFactory` {00000035-...} first and then QIs it for the statics interface. The fake has to return itself directly and answer both IIDs.

**Dependency shims.**
- `PhoneAppModelHost.dll` (`tools/gd/phoneappmodelhost.c`): `CaptureUiThreadContext` returns `E_NOTIMPL`. It **must** fail, or PPL continuations marshal to a non-existent UI context. `LoadPackagedLibrary` calls `LoadLibraryExW`.
- `PhoneAudioSes.dll` (`tools/gd/phoneaudioses.c`): maps the phone WASAPI-activation layer onto mmdevapi.
- VC110 runtime: app-local, taken from `D:\repo\armdeps\Microsoft.VCLibs.110.00_11.0.51106.1_arm__8wekyb3d8bbwe.Appx`.
- No api-set shims were needed (`loadcheck`).

### 4. Build (on the x64 PC, test on the device)

Defaults are in the `.bat` files and can be overridden with `LLVM_BIN`, `SDKROOT`, `SDKVER`, `MSVCINC`, `BUILD` and `DIST`. The defaults are our custom LLVM at `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin`, the Windows SDK 10.0.19041 at `D:\Windows Kits\10`, and the MSVC headers of VS2022 BuildTools. Point them at your own installs. Any clang plus `lld-link` that targets `thumbv7-pc-windows-msvc` should work, but this has not been tried with a stock LLVM.

```
tools\gd\build_gd_shims_arm32.bat     -> dist-gd\PhoneAppModelHost.dll, dist-gd\PhoneAudioSes.dll
tools\gd\build_gd_host_arm32.bat      -> dist-gd\gd_host.exe
python -I tools\gd\make_gd_host_package.py   -> dist-gd-host\
```

The compiler flags are `--target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin`. The link flags are `/machine:arm /nodefaultlib /entry:mainCRTStartup` with `kernel32.lib` and `armrt.lib`.

`make_gd_host_package.py` needs these inputs:
- `Geometry_Dash_2.0.1.0\` in the repo root (the extracted xap, read-only).
- The VCLibs110 appx.
- The built `dist-gd\` files.

It copies the DLLs and assets and clears the fmod APPCONTAINER byte (file offset 0x15F, `0x11` to `0x01`). It does **not** create the zip. Compress `dist-gd-host\`'s contents yourself.

**Shortest dev loop.** `source.zip` has no game folders, so `make_gd_host_package.py` will not run without your own copy of the game. You do not need it. Extract `runnable/GeometryDash_M1_host.zip` once, rebuild only `gd_host.exe` with the second `.bat`, and copy it over the one in the extracted folder.

**Recurring bug.** MSVC `<stdarg.h>` `va_start` is wrong under clang-ARM and gives garbage varargs. Use `__builtin_va_*`, as `gd_host.c` does with `GD_VA`. The tiny logger supports `%s %S %d %u %x %p`, not width: `%08x` prints as `08x` (cosmetic).

### 5. How to continue

**M2: visible, upright, animated.** This is the next step.
- Create a Win32 window and an `IDXGISwapChain1` on the **same** device with `IDXGIFactory2::CreateSwapChainForHwnd`. A working freestanding pattern is T6 in `tools/m0probe/m0probe.c` (`make_window` and `CreateSwapChainForHwnd`, about line 1966).
- Write a present pass. `m0probe.c` has a ready textured-quad kit (`quad_init`, about line 1404). It uses fxc-compiled `vs_4_0_level_9_1` and `ps_4_0_level_9_1` blobs (`k_quad_vs` and `k_quad_ps`) with a POSITION+TEXCOORD0 layout. Use the portrait RT as the SRV and assign the quad's UVs rotated by 90 degrees **counter-clockwise** (orientation flag 0 produces a picture rotated clockwise). This should need no shader change.
- Set all pipeline state explicitly every frame. ANGLE shares the host immediate context and leaves its own state behind. Unbind the RTV before sampling the texture.
- Run `PeekMessage`, `PrepareResources`, `Draw` and present on **one** thread (the thread that owns the D3D11 context), at display rate. The `RequestAdditionalFrame` flag can be ignored.
- Keep the RT size constant. A size change triggers ANGLE's size-dependent reset. The game's logical frame is the texture size with width and height swapped.
- Delegates are invoked from the Draw thread. They must not block; post to the window thread. `Cocos2dEventDelegate.Invoke(2)` (TerminateApp) sets `g_want_quit` today. The game never exits any other way, so turn it into `WM_CLOSE`, then `Disconnect()` (slot 4).
- On minimise, `Disconnect` pauses the game, and `Connect` plus `Draw` resumes it.

**M3: input.** Fill in the stored handlers.
- Build a fake `PointerEventArgs`: it must QI `IAgileObject`, and its slot 6 `get_CurrentPoint` returns the point.
- Build a fake `PointerPoint`: slot 7 `get_Position` writes two floats (x, then y), and slot 9 `get_PointerId` returns a `UINT32`.
- Call the stored handler's `Invoke` (slot 3) as `(handler, &g_manip_host, args)` from the window thread. The component queues the event under a mutex and runs it inside the next `Draw`, so this is thread-safe.
- Coordinates are **RT pixels in the portrait frame**, with no ResolutionScale in XAML mode (gd-component F8, corrected by gd-platform F3). For orientation 1: `xp = Wp - gy*scale`, `yp = gx*scale`. Log the `CCPoint` the game receives to verify. If the picture or the touch is mirrored, try orientation 4 via `CreateInstance(1)` or `updateOrientation` (slot 11).
- Keyboard and back go through `Direct3DInterop`: slot 12 `OnBackKeyPress` (map Esc, Back and Alt+Left; never auto-close on them), slot 13 `OnCocos2dKeyEvent(key)` and slot 14 `OnCocos2dKeyEvent(key, text)`. The key enum is Text=0, Escape=1, Back=2, Enter=3. Send `WM_CHAR` as Text with UTF-16 and never send empty text.
- Slot 15 `OnCocos2dEditboxEvent` returns the result of the modal edit box, which needs a real dialog (gd-shell F14). Message boxes are fire-and-forget (gd-shell F13).
- `ShowKeyboard` and `HideKeyboard` (event 0 and 1) should drive the TabTip controller already in `win32_host.c`. See `arm32/OSK_NOTES.md`; the Windows 10 and Windows 11 logic differ. Keep all three delegates registered. Otherwise the game falls back to the phone-only `KeyboardInputBuffer`, which does not exist on desktop.

**M4: remaining WinRT and `ICallback`.**
- Still to do: `Windows.Foundation.Uri` plus `Launcher.LaunchUriAsync` for `openURL`, and optionally `Accelerometer`. Neither is on the boot path. Unknown classes are already forwarded and logged. Whether the OS `Launcher` works unpackaged is unmeasured; the 0.10.4 host has a `Launcher` fake (gd-platform F11) to copy.
- `DisplayProperties` and `CoreWindow` are **not needed**, because the XAML-mode byte `CCEGLView+0x17C` is set.
- `ICallback` is stubbed today: `isNetworkAvailable` returns TRUE, `isLowMemoryDevice` returns FALSE, `getUniqueUserID` returns 40 zeros (a stable 40-hex ID per device would be better), and `openEmail` and `rateApp` are ignored.

**M5: audio.** FMOD's WASAPI output module calls the `PhoneAudioSes` shim during `System::init`:
- `GetDefaultAudioRenderId(0)` and `GetDefaultAudioCaptureId(0)` return `CoTaskMemAlloc`'d UTF-16 endpoint ids.
- `ActivateAudioInterface(id, iid, &ppv)` is synchronous and asks for IAudioClient2 {726778CD-...} or IAudioClient {1CB9AD4C-...}.

These signatures are **inferred** from call sites (gd-deps F5). Confirm them on the first audio run and add logging to the shim. The fallback is a silent shim.

**M6: package and device matrix.** Test on the Surface 2, then Surface RT 8.1 (Tegra 3, FL 9_1, RT 8.1 never run), then the POCO X3 Pro. Run the M1 package on the POCO X3 Pro first and keep `gd_host.log` and `gd_frame.bmp`: they are the first data point for that device.

**ANGLE feature-level knob.** The game always tells ANGLE "9_3", whatever the real device level is. Its precompiled shaders are level 9_3. A real 9_1 GPU may need ANGLE on its 9_1 paths:
- Option A: patch `GeometryJumpComponent.dll` at file offset `0x357FC` from `03 26` to `01 26`. The package DLL is currently unpatched.
- Option B, preferred: hook `CreateWinrtEglWindow` and rewrite its 2nd argument. The component imports it from `libGLESv2_phone.dll` **by ordinal 181**. `iat_patch` currently skips ordinal imports, so it needs extending.
- Gate either option on the device's real `GetFeatureLevel() == 0x9100`. Do not force 9_1 on capable GPUs.
- If a real 9_1 GPU rejects the shaders, the fallback is 18 hand-written level_9_1 replacements swapped in by DXBC checksum (gd-render F7, with a proof of concept), or WARP at 9_3. The Surface 2 Tegra 4 needed neither for the splash.

**Open risks:**
- Performance on Tegra is unmeasured beyond the splash.
- Non-power-of-two textures with GL_REPEAT on 9_1 were not checked per texture.
- The portrait-RT direction and the pointer mapping are inferred.

**Rules.**
- Build on the x64 PC and test on the device. Never run ARM binaries on the x64 PC.
- Keep game folders, `*.appx`, `*.xap` and `*.pdf` out of git and out of the public zip.

---

*Hand-off compiled 2026-10-23. The full decoded recon is in `games_recon/`, the chronological history is in `HANDOFF_ARM32.md`, and each game's build log/notes are cited inline above.*
