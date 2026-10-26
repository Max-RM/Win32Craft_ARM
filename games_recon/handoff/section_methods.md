STATUS: DONE

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
