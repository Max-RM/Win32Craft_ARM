# Win32Craft ARM32 port - handoff

**Status (2026-10-11): works on two real ARM32 devices.**
1. First device (OS not stated): the user ran the device package and reported "it works".
2. **Windows RT 8.1 (6.3 build 9600)**: after adding the app-local runtime + the `LocalFree` code shim the user reports "it worked now"; `loadcheck.exe` returns
   **0 problems** (log kept in `arm32/device_logs/loadcheck_rt81_6.3.9600_OK.txt`).
No further detail was given (which features were exercised - audio, input, long play sessions - is unknown), so treat the items in "Known open issues" as unverified
until the user says otherwise.

## What this is
`win32craft.dll` + a patched exe let the UWP Minecraft Bedrock **1.2.8 ARM32** build (`Minecraft_1.2.8.0_ARM32/`) run as a Win32 desktop app on
Windows RT 8.1 / Windows 10 ARM32. The host creates a window and a **Direct3D 11 device at feature level 9_1** (the user's devices top out at 9_1;
the game has no D3D9 renderer), fakes the WinRT services the game expects, injects input and runs the game frame loop. Only the 1.2.8 path is ported;
the original x86 versions are unchanged (all ARM code is behind `WIN32CRAFT_ARM32`; `python tools/arm32/check_x86.py` proves the x86 output is identical).

## Deliverable for the device (no Python needed on the device)
`python tools\make_arm32_device_package.py` (on the PC, run last) rebuilds everything and writes
`dist-arm32\device\Minecraft_1.2.8.0_ARM32_Win32Craft\` (complete ready-to-run game folder) and `dist-arm32\Minecraft_1.2.8.0_ARM32_Win32Craft.zip`.
On the device: copy the folder to a writable path, run `Win32Craft.1.2.8-arm32.exe`; `win32craft.log` appears next to it (fallback `%TEMP%`).
`use_silent_audio.bat` / `use_real_audio.bat` swap FMOD (silent stub vs patched real FMOD). `start_fl91/93/100.bat` pick the feature level.
Builds embed date/time, so each build has a new hash; always package last.

## Map of the repo (ARM32 parts)
| path | what |
|---|---|
| `win32_host.c`, `api_compat.c`, `win32_compat.h` | host sources; ARM code under `#ifdef WIN32CRAFT_ARM32`; pristine x86 copies in `arm32/orig/` |
| `win32_host_arm32.h` | GENERATED ARM constants (`tools/arm32/gen_constants.py` from `arm32/facts/*.json`; fmod area skipped). Do not hand-edit |
| `win32craft_arm32.def`, `build_arm32.bat` | ARM DLL build (RT LLVM at `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin`, VS2022 ARM libs, SDK 10.0.19041 ARM libs; env-overridable) |
| `tools/1.2.8-arm32-patcher.py` | exe patcher (PIC trampoline at end of .text, new `.d3imp` import section, `*_APP.dll` renames, XINPUTUAP redirect, optional FMOD patch at file 0x78A4E) |
| `tools/make_arm32_package.py`, `tools/make_arm32_device_package.py` | overlay package / full device folder + zip |
| `fmod_null/`, `build_fmod_null_arm32.bat` | silent FMOD fallback DLL (39 exports, no imports); test `arm32/tests/fmod_null_test.py` (Unicorn) |
| `ARM32.md` | user documentation (build, patch, deploy, options, troubleshooting) |
| `arm32/facts/` | verified reverse-engineering facts (JSON + md + independent verdicts) for gfx_device, gfx_compat, appmain, input, client_state, platform_fixes, pe_imports, winmain, chat_xbox, fl91, fmod, touch_game, touch_host, touch_impl |
| `arm32/RE_GUIDE.md`, `tools/arm32/arm32re.py` | RE toolkit for the ARM exe (pdata function table, annotated Thumb disassembly, RTTI vtables, xrefs, imports, names.jsonl) |
| `arm32/PORT_RULES.md`, `PORT_RESULTS.md`, `GLUE_LOG.md`, `STATUS.md` | porting rules/decisions, per-unit results, integration changes, detailed status/risks |
| `arm32/edits/`, `tools/arm32/apply_edits.py`, `try_unit.py` | the original parallel edit units (already applied IN PLACE; later edits were made directly) |
| `tools/arm32/check_x86.py`, `verify_constants.py` | regression checks (x86 identical; constants vs the real exe - 4 known mislabeled TARGET-BYTES/INCLUDE_INTERFACE items are expected) |

## Key design decisions (do not undo without reading the facts)
* Hooks patch **data cells** (vtable/IAT/delay-IAT, string bytes) with `PAGE_READWRITE`; the only code patch is in the exe (call site -> trampoline) done offline by the patcher.
* `D3D11CreateDevice` feature levels clamped to `{0x9100}` (option `--feature-level=9_1|9_2|9_3|10_0|10_1|11_0|11_1|game`, env `WIN32CRAFT_FEATURE_LEVEL`); the hook must stay installed for
  the whole process (the game zeroes `owner+0xb8` on device loss). Textures/backbuffer are clamped to 2048 at 9_1/9_2. 197/197 game shader variants compile at `*_4_0_level_9_1`.
* HSTRING functions forward to native combase on ARM (the genuine desktop vccorlib/FMOD use real HSTRINGs); Wine-era vccorlib shims, CreateFile2 hook, fmod delay-import resolver are not used on ARM.
* FMOD: patched UWP `fmod_ARM.dll` skips the package-identity WinRT step (`00 23 BB 65 -> 00 24 86 E1`); fallback silent stub if its INFINITE device-watcher waits hang.
* The log is ON by default and is the main debugging channel (`[ms Tthread]` prefix, startup SUMMARY, module inventory, abort/terminate traps, rotation at 4 MB).

## Known open issues / ideas (all unverified on device unless the user reports otherwise)
1. (RESOLVED, see section at the end) Windows RT 8.1 (user's first failure: `api-ms-win-core-heap-l2-1-0.dll` missing): the device folder now ships an app-local runtime - SDK `ucrtbase.dll` + 40 SDK api-set shims + 24 generated forwarders
   (`tools/make_arm32_apiset_shims.py` -> `dist-arm32/win81-runtime/`, also zipped as `Win32Craft-ARM32-win81-runtime-overlay.zip`). Which contracts RT 8.1 really lacks could not be checked offline; if another
   "missing DLL / entry point not found" appears, run `python tools/arm32/apiset_scan.py <folder>` and extend the generator (forward targets are KERNEL32/OLE32/ADVAPI32/COMBASE).
2. `ClientInstance::vf3` hook caller only found statically - if it never fires, gameplay mouse capture and chat text-focus detection stay inactive (log reports after 5/15/30/60 s).
3. Text-box focus calls `InputPane::GetForCurrentView` (throws on a plain HWND): `drain_dispatcher_queue` has no exception containment; suspects: AppPlatform_Winrt vf11/vf12 cells 0x013695D0/0x013695D4.
4. Main-menu exit-confirm click rectangle / `pending_main_menu_exit_confirmation` heuristic (shared with x86) can close the app after ESC in Settings.
5. AppView::Run extras not emulated (client-removal restart, AppMain+4 restart); host holds an extra RTV reference (a game-initiated ResizeBuffers would fail with INVALID_CALL, logged).
6. FL9_1 limits: no MSAA, 16-bit index meshes, <=2048 textures; wipe the game's `ShaderCache2` if the feature level cap is changed between runs.
7. 40 of 67 activated WinRT classes are not faked (the real runtime answers on Win10; RT 8.1 may fail some) - look for `WinRT fallback` lines in the log.
8. Optional enhancements: fmod option B' (route fmod's WinRT activation to host fakes, see `arm32/edits/hstring_native/FMOD_HOOK.txt`); a native ARM32 patcher exe if on-device patching is ever wanted;
   porting other Minecraft versions would need their ARM binaries and a new RE round with `arm32re.py`.

## How to continue
* Ask the user for `win32craft.log` (and `.log.old`) plus a note of what is on screen; start from the first failing/odd line. Ask what exactly "works" covers (rendering, input, mouse look, audio, chat, world load).
* After any code change: `build_arm32.bat`, `python tools/arm32/check_x86.py`, `python tools/arm32/verify_constants.py`, then `python tools/make_arm32_device_package.py`.
* If a constant/offset looks wrong: re-derive with `python tools/arm32/arm32re.py ...` (see `arm32/RE_GUIDE.md`) and fix the fact JSON, then regenerate the header.
* Shell gotcha seen in this project: heredocs passed through the tool shell collapse `\\` - write scripts containing backslashes with the file Write tool.

## Windows RT 8.1 loader issues (session 2026-10-11, RESOLVED)
* Run 1: missing `api-ms-win-core-heap-l2-1-0.dll` -> app-local runtime added (SDK `ucrtbase.dll` + SDK api-set shims + generated shims; `tools/make_arm32_apiset_shims.py`
  -> `dist-arm32/win81-runtime/`, also shipped as `Win32Craft-ARM32-win81-runtime-overlay.zip`).
* Run 2: "procedure entry point LocalFree could not be located": the forwarder shim (`LocalFree=KERNEL32.LocalFree`) did not work on RT 8.1 (likely `KERNEL32!LocalFree` forwards back
  into the very api-set name the OS lacks, so the chain breaks; not proven). Fix = `api-ms-win-core-heap-l2-1-0.dll` as a REAL code shim (`LocalFree` -> `HeapFree(GetProcessHeap())`).
  The other forwarder shim that RT 8.1 needs (`libraryloader-l2-1-0`: `LoadPackagedLibrary` -> `KERNEL32`) works, so forwarders themselves are fine.
* **Empirical result from `loadcheck.exe` on Windows 6.3.9600:** only these api-set names are served from the game folder: `api-ms-win-core-heap-l2-1-0`, `api-ms-win-core-libraryloader-l2-1-0`
  and the `api-ms-win-crt-*` set (+ app-local `ucrtbase.dll`). EVERY other contract (sysinfo-l1-2-0/-2-1, file-l1-2-1, processthreads-l1-1-2, com-l1-1-1, winrt-*, eventing-provider, delayload ...)
  is provided natively by KERNELBASE/KERNEL32/combase. The other ~22 generated forwarder DLLs are therefore unused on 8.1 (harmless; they could be dropped from the package to reduce clutter).
* `loadcheck.exe` (`tools/loadcheck/loadcheck.c`, `build_loadcheck_arm32.bat`, shipped in the device folder and overlay zip) is the standard first step for any future "missing DLL / entry point" report:
  ask the user to run it and send `loadcheck.txt`. Build it for any machine class with the same script.
* Not yet known: how well the game behaves on RT 8.1 in detail (FL9_1 rendering, input, audio/FMOD, chat). Ask for `win32craft.log` and what works.

## Touch input / on-screen touch controls (session 2026-10-12, IMPLEMENTED, UNVERIFIED ON DEVICE)
**Report (Surface RT, Windows RT 8.1, no keyboard/mouse):** menus work by touch, the on-screen game controls never appear.
**Root cause (two independent RE reports, `arm32/facts/touch_game.*`, `touch_host.*`; five key claims re-verified by hand with `arm32re.py`):** there is no setting and no
hardware-presence query in the game. `InputHandler::tick` (0x5abb44) asks every mapper whether it saw input and the LAST one that did decides the InputMode
(mapper order Gaze, Keyboard, Mouse, GamePad, Touch, Motion, ...; Touch beats Mouse). Only `MultitouchItem`s (HID item type 1, factory 0x62a174, `Multitouch::feed` 0x5b00ec)
make `TouchMapper::tick` (0x5b21ac) report; `renderInputMapperControls` (0x3783ac) calls `TouchMapper::render` only while the mode is Touch. The host only ever queued MouseItems
(Windows emulates a mouse from a finger when `window_proc` passes `WM_POINTER*` to DefWindowProc), so the game stayed in Mouse mode.
**What was done (all `#ifdef WIN32CRAFT_ARM32`, x86 output identical, `check_x86.py` green):**
* `win32_host.c`: block "ARM32 touch input" in front of `window_proc` (+ `queue_touch_event` after the ARM `queue_mouse_event`, option parser `arm32_parse_input_options` before
  `Win32CraftParseHostOptions`, fake InputPane `TryShow/TryHide` split + `OccludedRect`, `game_requests_relative_mouse` gated by `arm32_relative_mouse_gate`, wiring in
  `Win32Bootstrap` / `arm32_boot_run_loop`). Touch pointers (`GetPointerType == PT_TOUCH`, resolved with GetProcAddress) are consumed without DefWindowProc and queued as
  MultitouchItems (slots 0..11, press (1,1), move (0,0) once per loop pass, release (1,0)); mouse/pen pointers go to DefWindowProc as before.
* Options: `--input=auto|touch|mouse` (env `WIN32CRAFT_INPUT`), `--touch-api=auto|pointer|wmtouch|legacy`, `--osk=auto|on|off`, `--input-log` (see ARM32.md). Default `auto` = last used device
  wins (touch down -> TOUCH; real mouse move/click/wheel or key -> MOUSEKB, which releases touches and, 4 passes later, calls `TouchMapper::vf2` reset so stale released flags do not keep Touch winning).
* Touch-only machine (ready digitizer + touch points + no real raw mouse/keyboard device; `SM_MOUSEPRESENT` is logged but not trusted): the `getDefaultInputMode` vtable cell 0x0136980c is
  replaced so the game starts in Touch mode. Relative mouse capture is suppressed while touch is the active source. Touch-sourced mouse messages (extra info `0xFF515780`) are dropped;
  if they arrive although no PT_TOUCH pointer message was ever seen the host switches itself to `legacy` (single finger). Stationary `WM_MOUSEMOVE`s are dropped while touch is active.
* On-screen keyboard: `TryShow` starts `TabTip.exe` / calls `ITipInvocation::Toggle`, `TryHide` closes it (only if the host opened it), `OccludedRect` = keyboard window rectangle in buffer pixels.
* Facts: new `arm32/facts/touch_impl.json` (mapper vtables, InputHandler offsets, TouchMapper reset, vf154 cell; verified against the exe by `verify_constants.py`), `win32_host_arm32.h` regenerated.
* Device package: `start_touch.bat`, `start_mouse.bat`, `start_touch_wmtouch.bat`, `start_touch_legacy.bat`, `start_touch_inputlog.bat` + README text (`tools/make_arm32_device_package.py`).
**Unverified (needs the device log; grep `ARM32 input:`, `ARM32 touch:`, `ARM32 osk:`):**
1. that `WM_POINTERDOWN` reaches the window on RT 8.1 and that returning 0 stops the emulated mouse (`legacyFromTouch` stays 0 in the stats line); 2. that the game's mode really flips to Touch
(line `game input mode Mouse -> Touch`) and the HUD is drawn once a world is open; 3. HUD layout/safe-zone/size at 1366x768 (the 1280x720 resizable framed window is awkward for touch - a
borderless/maximised start for touch-only devices is NOT done); 4. TabTip class name / path / ITipInvocation on RT 8.1, and whether the chat box is covered by the keyboard (the game's
Showing/Hiding handlers are not invoked - phase 2 in `touch_host.md`); 5. whether `SM_DIGITIZER`/raw device inventory identify the Surface RT as touch-only (otherwise it starts in Mouse mode and flips on the first tap);
6. mixed use with a Type Cover flips the HUD with every key press/touch by design (add hysteresis in `arm32_input_note_source` if annoying); 7. `TouchMapper::vf2` reset call after touch->mouse is untested.
**Next step if it fails:** ask for `win32craft.log` and look at (in order) `touch handling active: api=`, `first PT_TOUCH WM_POINTERDOWN`, `item queued`, `mapper(s)` (TouchMapper present?), `game input mode`.

## Touch input: device result and multi-touch fix (2026-10-12)
* First Surface RT test (log kept in `arm32/device_logs/win32craft_surfacert_touch_legacy_singletouch.log`): the touch HUD appeared and worked (confirms the root cause: the game switches
  to Touch mode when it receives MultitouchItems), but only ONE finger worked. Log stats: `pointerMouse=0 pointerPen=0 pointerOther=0 wmTouch=0 legacyFromTouch=2186` - Windows RT 8.1 never
  delivered a single WM_POINTER message to the window (touch arrived only as emulated mouse messages with the 0xFF5157xx signature), so the auto-fallback went to the single-touch `legacy` mode.
* Fix: in `--touch-api=auto` the window is now ALSO registered with `RegisterTouchWindow` at startup; the first WM_TOUCH while no WM_POINTER touch was ever seen switches the effective API to `wmtouch`
  (real multi-touch, existing slot/contact code). If WM_POINTER works on another OS it stays primary. `start_touch_wmtouch.bat` still forces it. Log lines to look for:
  `window also registered for WM_TOUCH`, `WM_TOUCH arrived but no WM_POINTER touch message was ever seen - switching to --touch-api=wmtouch`, `ARM32 touch: first WM_TOUCH received`, and in the stats line `wmTouch=N`.
* UNVERIFIED on device until the user reports: that two simultaneous contacts (move + jump) now work; if not, send the log (the stats line shows wmTouch and the slot numbers of the touch events).

## Other Minecraft versions: feasibility check (2026-10-12)
* `Minecraft_1.21.82.01` (ARM32 UWP, 1.21.8201.0, built 2025-05) was examined and DROPPED by the user: its precompiled `data/renderer/materials/*.material.bin` only contain DXBC for SM4.0/5.0/6.0/6.5 (no
  feature-level-9 variants) and the exe has no runtime shader compiler, so it cannot run on FL9_1 hardware such as the Surface RT. It also imports `d3d12.dll`, is ~3x larger (278k functions) and uses a different
  engine stack (RenderDragon, cohtml, ChakraCore, xsapi) - a port would be a complete new RE round. Its folder contains third-party `Auto_Modificator_*` executables (not ours; never run unknown exes).
* **Quick check for any candidate ARM32 build before porting** (needs the exe and its `data` folder):
  1. `llvm-readobj --file-headers Minecraft.Windows.exe` -> Machine must be ARMNT (0x1C4); `AppxManifest.xml` ProcessorArchitecture="arm".
  2. FL9_1 viability: the exe should import `D3DCOMPILER_47.dll!D3DCompile` and contain the strings `_level_9_1` / `_level_9_3` (as 1.2.8 does: `python tools/arm32/arm32re.py str _level_9_1`), or its material files must ship Level9 shader variants.
     Run the offline probe idea in `arm32/tests/shader_probe.py` (compile the shipped HLSL at `*_4_0_level_9_1`). No D3DCompile + only SM4+ DXBC = cannot run on a 9_1 GPU.
  3. Prefer the HLSL-source era (roughly 1.2 - 1.16, before RenderDragon); the host would need a new facts round with `tools/arm32/arm32re.py` (follow RE_GUIDE.md) - the 1.2.8 RVAs do not transfer.
* `Minecraft_1.18.12.1` (ARM32 UWP, built 2022-02) checked the same way and ALSO NOT viable on FL9_1: no D3DCompile import, no `_level_9` strings, RenderDragon, `material.bin` only SM4.0/5.0/6.0/6.5 DXBC (its `data/shaders/hlsl` holds only cohtml UI shaders), imports d3d12.dll. Best next candidate: an ARM32 build of the repo's already-supported x86 era (e.g. 1.16.20.03, see `tools/1.16.20.03-patcher.py`, `win32_host_116.inl`) - run the checklist above first.
* `Minecraft_1.16.40.02` (ARM32 UWP, built 2020-09-03, 140k functions, image 0x2BD9000) passes the quick check and IS viable on FL9_1: imports `D3DCOMPILER_47!D3DCompile` (single caller sub_1062384) and `D3DReflect`, d3d11 only (no d3d12), no RenderDragon,
  strings `_level_9_1` / `_level_9_3` / `#define VERSION `, 19 JSON `.material` files + HLSL sources in `data/shaders/hlsl`; fxc test: renderchunk/entity/color/position/sky/cloud/texture_cutout compile at `vs/ps_4_0_level_9_1`.
  Uses `fmod_ARM.dll` (2017 build, like 1.2.8) and imports `chakra.dll` (Windows 10 JS engine: absent on RT 8.1 -> would need a shim/redirect, e.g. to jscript9.dll JSRT, UNVERIFIED) and `vccorlib140_app.DLL`.
  The repo already supports the x86 1.16.20.03 (`win32_host_116.inl`, `tools/1.16.20.03-patcher.py`) which gives semantics for the fake WinRT objects. A port = new RE round (same method as 1.2.8, see RE_GUIDE.md) + host/patcher work; NOT started (waiting for the user's go-ahead).
* 1.16.40.02 cheap feasibility test results (2026-10-12): (a) 52/52 game vertex/fragment HLSL files compile at `*_4_0_level_9_1` with default defines (`level_9_1` = ps_2_0-class target; same mechanism the 1.2.8 game already runs with on the Surface RT);
  geometry shaders (14) are VR-only. (b) `chakra.dll` import = 38 core JSRT functions (JsCreateRuntime/JsRunScript/...), Win10-only DLL -> needs a shim on RT 8.1 (JSRT exists in jscript9.dll on 8.1 but JsSetPromiseContinuationCallback is Win10-only; a stub like `fmod_null` is the fallback).
  (c) 46 api-set contracts imported, 41 already proven on RT 8.1 by loadcheck; unproven: processtopology-obsolete-l1-1-0 (SetThreadAffinityMask), synch-ansi-l1-1-0 (OpenMutexA), synch-l1-2-1 (WaitForMultipleObjects), windowserrorreporting-l1-1-0 (WerRegisterFile), wow64-l1-1-0 (IsWow64Process) -> handle with `tools/make_arm32_apiset_shims.py` + `loadcheck.exe`.
  (d) 139 WinRT class-name strings (many are generic type names), 33 already faked by existing host code, ~100 would fall to the real runtime. (e) `arm32/tests/shader_probe.py` now accepts `SHADER_PROBE_GAME=<folder>`.
  Verdict: viable; no blocker found; effort = a new RE round + host/patcher work (est. 60-80% of the 1.2.8 effort thanks to existing tooling). Not started.
