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
* On-screen keyboard (FIRST VERSION, superseded 2026-10-17 - see the section 'Touch keyboard (TabTip) fix'): `TryShow` starts `TabTip.exe` / calls `ITipInvocation::Toggle`, `TryHide` closes it (only if the host opened it), `OccludedRect` = keyboard window rectangle in buffer pixels.
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

## Touch keyboard (TabTip) fix (2026-10-17, CONFIRMED WORKING on the user's Surface 2, Windows 10 ARM32)
* Symptom: the touch keyboard came up when the game asked for a text box but would not close and the game UI would not go back down (same family of bug as TelegArm). Reproduced on both Windows 10 and RT 8.1.
* Cause (found with the device lab `tools/kbdprobe`, build `build_kbdprobe_arm32.bat`, log in `arm32/device_logs/kbdprobe_surface2_win10_15035.log`): the first controller used `IsWindowVisible(IPTip_Main_Window)` as the
  visibility signal; that window is always visible and its rectangle never changes, so the host reported a keyboard rectangle forever, never toggled a running TabTip, and never raised the game's InputPane Showing/Hiding
  handlers (the game registers them: Showing body 0x57e2e8, Hiding body 0x57e398; the old fake `add_Showing/add_Hiding` threw the delegates away).
* Fix (win32_host.c, ARM block `Windows touch keyboard (TabTip) for text boxes` + fake InputPane): state from `IFrameworkInputPane::Location` / WinRT InputPane `OccludedRect` (strong) or the DWM cloak flag (weak) or none;
  show = Toggle only when the state says hidden (launch `TabTip.exe` only when its window is missing); hide (only a keyboard the host showed) = `IInputPane2::TryHide`, `SC_CLOSE`, `Toggle`, each verified; stored game
  Showing/Hiding delegates are raised from the main loop with a fake `InputPaneVisibilityEventArgs`; `OccludedRect` is non-zero only while the keyboard is up; keyboard X by the user = visible->hidden while ours -> Hiding raised,
  and a later tap while the text box still has focus reopens it. Full facts, the measured signal table and the design: `arm32/OSK_NOTES.md`. Log prefix `ARM32 osk:` (first line `ARM32 osk: signals: ... primary=`).
* Windows RT 8.1 has NOT been measured with the probe (TelegArm: InputPane interop missing, cloak flag unreliable there); the controller falls back through the signals and logs which one it used. If it misbehaves on the Surface RT,
  run `kbdprobe.exe` there and compare with the Windows 10 table in `arm32/OSK_NOTES.md`.
* x86 untouched (`tools/arm32/check_x86.py` identical).
* Independent code review (2026-10-17) of the first version found, and the second DLL (hardening, built after the user's successful test of the first one; the user then tested it on the Surface 2 and reported it okay and better than the first) fixes: (1) a second `TryShow` while a show was in flight
  restarted the request and could send a second state-blind `Toggle` (now ignored while a request is in flight, and no Toggle within 2 s of a successful one); (3) `TryHide` right after `TryShow` left the keyboard unowned (now
  hidden as soon as it is visible); (4) `TryShow` right after `TryHide` cancelled the hide and could flicker or never show (now queued until the hide is done, and a hide landing within 1.5 s of a TryHide is not a user dismissal);
  (5) stale `IFrameworkInputPane`/`InputPane` state after TabTip restarts (no TabTip window / not visible = hidden); (6) the no-signal path left the state stuck at "visible"; (7) retry spacing (2.5 s after a successful Toggle, 300 ms
  polling after a TabTip launch); (8) tap-to-reopen delay 300 ms. Hardening against a BLIND shell signal (reviewer's finding 2, the host thread is MTA while kbdprobe ran in an STA): `Location` must succeed at init, the signal counts as
  "proven" only after it changed or a shell callback fired, and an unproven signal never triggers a second Toggle after a successful one (the host then assumes "visible"). Open: finding 9 (blocking cross-apartment COM calls on the
  render loop, no timeout), finding 10 (FIP never Unadvised; game delegates never released at exit) - both benign so far.
* New repo parts: `tools/kbdprobe/` (+ `build_kbdprobe_arm32.bat`, `dist-arm32/kbdprobe.zip`), `arm32/OSK_NOTES.md`, `arm32/device_logs/kbdprobe_surface2_win10_15035.log`.

## Other versions in progress (2026-10-17)
* 1.16.40.02 is PARKED (user decision): groundwork only, see `arm32/v1_16_40/README_V116.md` (status block at the top, SCOPE.md, per-exe label files, one-DLL-for-all-versions plan, table-driven unified patcher planned).
* 0.10.4.40 (Windows Phone 8.1 XAML build `MCPE.Host.WindowsPhone.exe`, outside this repo at `D:/repo/win32craft/0.10.4.40`; a friend's attempt reached a blank window / StartMenuScreen crash): RECON DONE 2026-10-17 (workflow `v0104-recon`, 7 areas, every
  area independently re-checked: 434 verdicts = 391 confirmed / 28 corrected / 2 refuted / 13 unverifiable). Read `arm32/v0_10_4/RECON_SUMMARY.md` first, then `arm32/v0_10_4/facts/*.md` (+ `.json`, `.verdicts.json`). Decision: FAITHFUL - run the game's own
  Application::Start -> App2 -> OnLaunched -> MainPage -> AppMain code against ~30 host fakes (XAML/WinRT objects, ~80 vtable slots); the friend's bypass approach is wrong (their 'tick' 0x1DA5B4 is SetKeyboardHeight; real frames come from a ThreadPool render
  loop the host never started; MinecraftClient::init runs on a worker from the AppMain ctor; input registers only via Tick -> AttachToPanel -> HID::Initialize). FL9_1: all 76 shader variants compile at level_9_1, but the game never requests 9_1 and
  getShaderModelSuffix (0xD73FC) throws for it -> hook D3D11CreateDevice (clamp {0x9100}, report 0x9300) + 1-byte profile patch at 0x2351F9 (0x33->0x31). Milestones M0 patcher/loadcheck, M1 menu, M2 input+TabTip (level-triggered keyboard), M3 world, M4 audio/net.
  Effort estimate ~0.5-0.6x of 1.2.8. NOTHING of the friend's code is reused. No host code written yet. See `arm32/v0_10_4/README_V0104.md`.

* 0.10.4.40 host FOUNDATION written (2026-10-17): `arm32/v0_10_4/HOST_DESIGN.md` (design, rules, cross-unit API), `arm32/v0_10_4/units/*.md` (five implementer briefs + generated `SLOTS.md`), toolkit `win32_host_arm32_0104_core.inl`, bootstrap `win32_host_arm32_0104_boot.inl`,
  umbrella `win32_host_arm32_0104.inl` (included in front of `Win32Bootstrap`, `host_is_0104` selects it by PE machine + SizeOfImage 0x00E6F000), five unit STUB files (`win32_host_arm32_0104_{async,app,panel,platform,input}.inl`, pristine copies in `arm32/v0_10_4/stubs/`),
  generated `win32_host_arm32_0104_iids.inl` (`tools/arm32/gen_0104_iids.py`, SDK IIDs/slots cross-checked against the exe facts), `tools/arm32/try_0104.py`, `tools/arm32/test_0104_core.py`. The unit bodies, the exe patcher (M0) and the device package for 0.10.4 are NOT done yet.

## 0.10.4.40 device package built (2026-10-18) - FIRST DEVICE TEST PENDING
* `python tools/make_0104_package.py --build` -> `dist-0104/Minecraft_PE_0.10.4.40_ARM32_Win32Craft/` + `.zip` (15.2 MB, sha256 9effd481...): patched exe `Win32Craft.0.10.4-arm32.exe` (arm32-patcher profile v0_10_4, sha256 cb44c09a..., FL9_1 profile byte on), the one `win32craft.dll` (1.2.8 + 0.10.4 host), patched VC120 `*_app` DLLs, app-local UCRT + api-set forwarders for RT 8.1, loadcheck.exe, start*.bat (selftest, window sizes, fullscreen, dcomp, halfres, vsync, fps cap, FL, osk, touch variants), README_DEVICE.txt, MANIFEST.txt.
* The user skipped the two independent cross-unit reviews (ordering/threading, ABI/slot coverage) to test directly; they can still be run later with the resume command in `arm32/v0_10_4/README_V0104.md`. Test order on the device: `start_selftest.bat` (ABI check of every fake) -> `start.bat` -> send `win32craft.log` (+ `.old`); startup DLL errors -> `loadcheck.exe` -> `loadcheck.txt`. Path of the folder must be ASCII.

## 0.10.4.40 FIRST DEVICE RUN (2026-10-18, Surface 2 Windows 10 ARM32): IT RUNS
* Log kept: `arm32/device_logs/win32craft_0104_surface2_win10_first_run.log` (+ photos in the chat). Boot, selftest path, shader compilation (~2 s, only X3205 warnings), HWND swap chain (path B, dxgi vtable cells patched, Present stub counts frames), main menu (Play button, splash, version text),
  WM_POINTER touch (`handlers=2`), the new TabTip controller (IFrameworkInputPane, `ARM32 osk:` lines: show on EditBox.Focus, X-dismiss detected, Hiding raised) and a 3D world at ~60 fps for 160 s with a clean WM_CLOSE -> Suspending -> exit all work.
* PROBLEM: in the world there is NO GUI (no hotbar, no on-screen touch buttons d-pad/jump/pause/chat, no crosshair), so it cannot be played without keyboard/mouse. Investigation: `arm32/v0_10_4/HUD_INVESTIGATION.md` (agent started 2026-10-18; hypotheses: hideGui/useTouchJoypad options, scale/size values the host reports, render state after the world pass, missing GUI textures, input-mode dependent controls).
* Known non-blockers seen in the log: `refcount underflow on Uri` warning (clamped, harmless), Play on Realms crashes (Windows.Web.Http not faked, known).
* HUD investigation result (2026-10-18, `arm32/v0_10_4/HUD_INVESTIGATION.md`): root cause NOT provable offline. Certain: `GameRenderer::render 0xB9E1C` draws `Gui::render 0x6E2C4` only if hideGui (`options+0x244`) == 0; hotbar `0x6F410` and touch buttons (`player+0xD8C -> TouchscreenInput::vf2 0x9B14C`) only if `client+0x6C` (Screen) == NULL; the world is visible so the player is in load state 3 and the game wants to draw the HUD. Refuted: input-feed dependence, useTouchscreen/InputHolder, resolution override (`--half-res` did not help), scale/size values (menu uses the same). Open: hideGui/ctrl_usetouchjoypad in options.txt, D3D state left by the world pass (RTV/viewport/scissor/blend), an ignored failed Create*/Map. Diagnostics added: `--trace-game-state` (`start_gamestate.bat`, log prefix `X104 gstate` + `X104 d3dtrace` context tracer + always-on failed-Create* wrappers `X104 panel: Create... FAILED`); decision table in HUD_INVESTIGATION.md section 5.2. Diagnostic package built (dist-0104, zip sha256 1abc39b6...) and DLL + bat sent to the user.
* User note: Surface RT and Surface 2 are FL9_1 only (see memory feedback_feature_level_auto: do not force 9_1 as the permanent default; Lumia FL9_3 devices have a broken 9_1 path).

## 0.10.4.40 in-world HUD FIXED (2026-10-18, confirmed on the Surface 2)
* Cause: textured HUD draws bind their dynamic vertex buffer with stride 0 (game side, reason unknown) -> collapsed quads -> invisible HUD although the draws, textures and matrices were right. Fix: panel unit vertex-buffer stride fixer (default ON, `--no-vb-stride-fix` off): layout strides are recorded at CreateInputLayout and the buffer is re-bound with the right stride before a draw that has stride 0. Full story, evidence and the diagnostics that found it: `arm32/v0_10_4/HUD_INVESTIGATION.md` section 7 (logs `arm32/device_logs/win32craft_0104_gamestate_run3..7*.log`).
* Also learned: the Direct3D runtime rewrites the draw/clear cells of its heap context vtable (they must be re-patched after state calls); heredoc backslashes collapse in the Bash tool - use the Write tool for scripts containing C strings with backslashes.
* Status of 0.10.4: boots, menu, touch, TabTip, world, HUD and on-screen buttons all work on the Surface 2 (Windows 10 ARM32, FL9_1). Not yet verified: audio, saving/loading worlds across runs, Windows RT 8.1 (api-sets, VC120 DllMain), Play-on-Realms (crashes, known). Skipped on request: the two independent cross-unit reviews. Pending design item: feature-level auto policy (see memory feedback_feature_level_auto: do not force FL9_1 as the permanent default; Lumia FL9_3 devices).

## Hand-off pack for git (2026-10-18)
* `python tools/make_source_package.py` -> `dist-handoff/Win32Craft_source_<date>.zip` (source only: no game folders, no build outputs, no RE index caches; adds .gitignore, README_SOURCE.md, MANIFEST.txt). `dist-handoff/` also holds the two ready-to-run game packages (1.2.8 and 0.10.4.40, same win32craft.dll in both), SHA256SUMS.txt and PACKAGE_README.txt. Game zips are build artifacts: attach to a release / LFS, do not commit them into git history.

## Windows 11 ARM64 fixes (2026-10-18, from the friend's logs on a POCO X3 Pro phone running Windows 11 ARM64 build 22631 - corrected 2026-10-21, first written as a Surface-Pro-X class machine)
Logs: `arm32/device_logs/win11_arm64_0104_friend.log`, `win11_arm64_128_friend_last_session.log` (+ an excerpt of the earlier 1.2.8 sessions). Report: "TabTip problem in both games, and in 1.2.8 move and look at the same time impossible".
* Touch (1.2.8): in the last session the first finger arrived as a lone emulated mouse message BEFORE any `WM_POINTER` finger (Windows also sends such a stray tap on working pointer stacks - the same stray tap is in the
  friend's other sessions, dropped because pointers were already seen). The old code treated it as proof that pointers do not work and switched to single-touch legacy for good (`touchApi=legacy`, `moveMsgs=0`). Fix: the emulated
  message is converted to one contact (as legacy did) but the host STAYS in pointer mode; the first real `WM_POINTERDOWN` finger takes over (a converted contact still down is released). A device that never sends pointer messages
  (the Surface 2 log `win32craft_surfacert_touch_legacy_singletouch.log`) behaves as before: single touch, and no late `WM_TOUCH` takeover. Earlier friend sessions prove the pointer path does 2-finger play (held joystick + camera drags).
* Keyboard (both games): Windows 11 never shows `IPTip_Main_Window`, so the Windows 10 rule "window not visible = hidden" kept the state at "hidden" while the shell's Showing/Hiding callbacks fired ~350 ms after every Toggle;
  the retry logic then Toggled every 2.5 s (ten times) = keyboard flickering open/shut, and the hide was ignored. Fix (Windows 11 only): callbacks are the state, Toggle-first hide, held-back Toggle retried, max 4 attempts, richer log
  (`ARM32 osk: snapshot ...`). Details and the Windows 11 facts: `arm32/OSK_NOTES.md` (last section). Windows 8.1 / 10 paths are unchanged: `python tools/arm32/test_osk.py` simulates the real controller on the PC (11 scenarios;
  with `WIN32CRAFT_HOST_C=<old win32_host.c>` the 2026-10-18 09:42 controller fails 8/9 Windows 11 scenarios and passes both Windows 10 ones). NOT verified on a real Windows 11 machine yet: ask the friend to send the new log
  (`ARM32 osk:` lines incl. `snapshot`).
* Log size: `win32craft.log` is capped at 1 MB (emptied at start when above 1 MB, restarted mid-run with the first 8 KB of the run kept; `.old` no longer used). `python tools/arm32/test_logcap.py` tests the real code.
* Test tooling fix: `tools/arm32/test_0104_all.c` lacked a stub for `arm32_iat_describe_address` (the integration test had stopped compiling since the HUD tracer); one timing check (W key repeat) in the integration test is flaky.

## Next app: Sup (unofficial WhatsApp client, Windows Phone 8.1 XAML app) - first package built (2026-10-18)
* User request: run `WhatsApp.Qt_0.2.0.8_ARM` (repo root, third-party, not in the source zip) "with our app" and patch it.  Recon says it is a FULL XAML app (~120 WinRT classes), so the Win32Craft fake-object
  host is the wrong tool; the app is patched to run as a real packaged app on Windows 10 ARM32 (`tools/whatsapp/make_whatsapp_package.py` -> `dist-whatsapp/Sup_WhatsApp_0.2.0.8_ARM_Win10.zip`: manifest converted,
  VC120 runtime app-local, `PhoneAppModelHost!LoadPackagedLibrary` redirected to KERNELBASE, four phone-only wrappers `bx lr`).  Details, reasons, expected first-run problems: `whatsapp/README_WHATSAPP.md`.
  NOT tested on a device yet (Surface 2: Developer Mode, `install.bat`, `diag.bat` -> diag.txt).  RT 8.1 variant not started.

## Friend's second Windows 11 run, Sup fix round 2, two keyboard fixes (2026-10-21)
* Workflow `win11-logs-and-sup-blockers` (5 investigators; findings verified by the main session, skeptic agents stopped on user request; cheaper models from now on).
* 0.10.4 on Windows 11 with the 2026-10-18 DLL: the keyboard fix works (6 Toggles for 4 shows + 2 hides, no flicker; log `arm32/device_logs/win11_arm64_0104_friend_r2dll.log`).  The 1.2.8 log ran the OLD
  DLL (09:42:51): the friend's 1.2.8 folder was never updated (the current zips are right) - replace win32craft.dll there; the log must say `host build Oct 21 2026 12:49:11`.
* New DLL (build Oct 21 2026 12:49:11, 445952 bytes, sha256 08536fe9d662d674...): Windows 11 shell callbacks processed even while idle (the friend's 1.2.8 log with the 10-18 DLL
  showed 3 unanswered show Toggles and 4 unseen idle callbacks), Windows 11 keyboard RECTANGLE from the shell (was a 40% guess in 0.10.4 / nothing in 1.2.8) and a faster reopen after a
  held-back Toggle (2.9 s -> ~1.2-1.8 s).  Details + test evidence: `arm32/OSK_NOTES.md` last section, `python tools/arm32/test_osk.py` (16 scenarios).
* Device correction: the friend's machine is a POCO X3 Pro phone (Adreno 640) with Windows 11 ARM64, not a Surface Pro X.
* Known open issues found in these logs (not fixed): 0.10.4 every exit hangs until the 30 s exit guard kills the process (also on the Surface 2; next step: make the guard dump the
  threads' PC/LR before TerminateProcess); 0.10.4 renders uncapped (120-330 fps on the phone, Present sync 0; cap only above the display refresh, so the Surface 2 is unaffected); world-load
  hitches of 2-5 s from runtime D3DCompile at 9_1 (an on-disk shader cache would help); the feature level is still forced to 9_1 even on Adreno 640 (auto policy pending); VEH full-stack
  dumps of benign web::http exceptions take ~40% of a 1.2.8 log.
* Sup (WhatsApp client): registration failed (0x80073CFD, app model 1 = phone).  Packager now writes a complete Windows 10 manifest (default; `--manifest win81` for RT 8.1), adds unqualified
  logos and patches `themes/PhoneTheme.xbf` with the three phone theme brushes the app needs (theme-following under Windows 10).  makeappx validates both manifests (it caught a
  `ForegroundText`/`ToastCapable` error in the investigated Windows 10 text).  Details: `whatsapp/README_WHATSAPP.md`.

## 2026-10-21 evening: Surface 2 keyboard fixes, 0.10.4 Back/Pause button, Sup handed to its developer, method for WP8.1 apps
* DLL **Oct 21 2026 13:46:04** (448000 bytes, sha256 0a9d1776da03053a...) in both packages:
  - 1.2.8 keyboard on the Surface 2 (Windows 10): the game's TryHide+TryShow pairs no longer close and reopen the keyboard (300 ms hide delay cancelled by a TryShow), the keyboard
    rectangle is held back while the finger that opened it is down, and the Toggle guard ends once the shell confirmed the last Toggle.  Windows 11 friend: 1.2.8 keyboard reported FIXED
    with the 12:49 DLL (log `arm32/device_logs/win11_arm64_128_friend_r2dll.log` and the later -3 run).  Details: `arm32/OSK_NOTES.md` last items; `python tools/arm32/test_osk.py` (22 scenarios).
  - 0.10.4: the Windows Phone build has NO pause button (pause / menu back = the phone's Back key = game key 0xFF, `MinecraftClient::tick` 0x59EE6).  The host now shows a small
    semi-transparent Back/Pause button (owned layered popup, user32 only) over the top-LEFT of the game window (moved from top-right on user request); a tap raises BackPressed like Esc.  `--no-back-button` /
    `start_no_back_button.bat` turn it off.  Integration test: created, visible, top-left, a tap = one BackPressed (150 checks).  Seen on the Surface 2 (log surface2_win10_0104_1346dll_backbutton.log: tap -> BackPressed handled=1).
* Sup (WhatsApp client): proved working on the Surface 2 as a patched appx (only Back missing).  The user put it aside - its developer is active; hand-off for him:
  `whatsapp/HANDOFF_FOR_SUP_DEVELOPER.md` (the 9 PC blockers with the source-level fix for each).
* The method is kept for abandoned Windows Phone 8.1 apps: `wp81/WP81_APP_METHOD.md` (route choice + 8 steps + gaps) and `tools/wp81/wp81scan.py` (recon report; on Sup it
  reproduces every finding, on 0.10.4 it answers "game -> Win32Craft host").
* Surface 2 re-test with the 13:46:04 DLL (logs `arm32/device_logs/surface2_win10_128_1346dll_keyboard_ok.log`, `surface2_win10_0104_1346dll_backbutton.log`): 1.2.8 keyboard 13 shows,
  8 hide+show pairs absorbed by the hide delay, no failure; 0.10.4 button works.  User: move it to the top-left -> DLL Oct 21 2026 15:06:55 (sha256 c538646c...).
* User: the button showed in the game's menus too; it should be there only while playing -> DLL **Oct 21 2026 15:26:02** (sha256 e0721087...): shown only when the game's MinecraftClient has a
  level (+0x68) and a player (+0x12C) and NO screen open (+0x6C; start menu, pause, chat, inventory, death and loading screens are Screens) - the same fields the HUD needs.
  `--back-button-always` / `start_back_button_always.bat` shows it in menus too; `--no-back-button` hides it.  Integration test: hidden without a world, shown top-left when forced,
  a tap = one BackPressed.  Not yet seen on a device.
* 0.10.4 storage path (friend's note): worlds go to `%APPDATA%\MinecraftPE\0.10.4\games\com.mojang\minecraftWorlds`, not `%APPDATA%\MinecraftPE\games\...`.  This is ON PURPOSE
  (platform unit, data root = `--data-dir=` or `%APPDATA%\MinecraftPE\0.10.4` or `<game>\LocalState`): `%APPDATA%\MinecraftPE` is the 1.2.8 host's folder, and the two versions must not
  share worlds/options - 1.2.8 upgrades old worlds one way, 0.10.4 cannot read 1.x worlds, and the options.txt formats differ.  Anyone who wants the other folder can start
  0.10.4 with `--data-dir=%APPDATA%\MinecraftPE` (at their own risk).  Not changed.

## New games recon (2026-10-22)

Redcon 1.3.0.0, Geometry Dash 2.0.1.0 and Bad Piggies Rebooted 1.0.0.0 were investigated (read-only).
Results: games_recon/*.md (15 files, all STATUS: DONE); decisions and order: games_recon/GAMES_PLAN.md.
Redcon = GO first (0.10.4 host + profile); Geometry Dash = possible; Bad Piggies = possible, gated on a device probe (M0).
Surface 2 logs name the D3D adapter "Software Adapter" (vendor 0): the Win10 15035 build may have no Tegra GPU driver.

## REDCON 1 1.3.0.0 on the x104 host (2026-10-22)

The x104 host (0.10.4) serves a second Windows Phone 8.1 game through a run-time game profile; ONE win32craft.dll.
* Profile: `win32_host_arm32_0104_core.inl` section 1c (`X104Profile`, `x104_profile_mcpe0104`, `x104_profile_redcon`, `x104_feat()`), selected in
  `x104_image_matches` by the patched SizeOfImage (0.10.4 0x00E6F000, REDCON 0x001E1000).  Feature bits: MCPE_PATCHES (shader profile string, resolution
  float, AppPlatform cells, the "report 9_3" lie), IAT_FACTS (0.10.4 IAT rvas as fallbacks; other games hook by name only and skip absent imports),
  XAML_PAGE (MainPage / AppMain / ready flag / first-SizeChanged rule / timer gate).  REDCON has none of them.
* REDCON specifics (games_recon/redcon_*.md): no MainPage - OnLaunched builds a SwapChainPanel through ISwapChainPanelFactory(NULL, NULL) (panel unit),
  frames come from CompositionTarget.Rendering on the UI thread (app unit 7b, `x104_app_rendering_tick`; the loop waits 1 ms while it is active),
  resizes through CoreWindow.SizeChanged (panel `x104_panel_core_resize_tick` -> input `x104_input_raise_core_size_changed`), Suspending/Resuming on
  CoreApplication (raised by the exit proc together with Application.Suspending), 5 more CoreWindow events + CharacterReceived from WM_CHAR, every key
  delivered (Minecraft's whitelist only for XAML_PAGE), Back button shown while the game runs, feature-level cap 0 (the game's own list 11_1..9_1; its
  shaders are level_9), RoamingSettings.Values = OS PropertySet (in memory), ProductLicenses answers every product NOT owned (the Store is closed).
  ApplicationLanguages / PropertyValue / Launcher go to the OS runtime (desktop-capable, logged as "X104 WinRT FALLBACK").
* Proof that 0.10.4 did not change: `python tools/arm32/try_0104.py --selftest --integration` (151 checks 0.10.4 + 107 checks REDCON mock incl. the 43-check selftest under the REDCON profile
  `tools/arm32/test_redcon_all.c`), `python tools/arm32/diff_0104_log.py --against arm32/work/baseline_pre_redcon/integration_set.txt` (IDENTICAL),
  patcher output hash of 0.10.4 unchanged, the regenerated 0.10.4 package differs only in win32craft.dll + patch_report.json.
* Patcher: `tools/arm32-patcher.py` profile `redcon` (call site 0x13A55A, trampoline 0x1414B0, all predicted outputs confirmed by the engine).
* Package: `python tools/make_0104_package.py --game redcon --game-dir build-redcon/Redcon_1.3.0.0_ARM_original` -> dist-redcon/.  The game folder
  Redcon_1.3.0.0_ARM in the repo holds a MODIFIED Redcon.exe (a third-party "Premium Patch", config.json); the patcher accepts only the stock exe, so the
  package is built from a clean copy unpacked from Redcon_1.3.0.0_ARM.appx.
* Open, device-only: music (Media Foundation Media Engine), the game's XAML dialog widgets (Popup/TextBlock/Border/StackPanel/TextBox/Button: NOT faked
  yet - the log will name them), CurrentApp.LoadListingInformationAsync / GetAppReceiptAsync (E_NOTIMPL), roaming settings not persisted, Windows RT 8.1.

### REDCON device result + fixes (2026-10-22)
* Surface 2 (Win10 15035): REDCON runs fine (~30 fps, touch, Back, exit) - arm32/device_logs/surface2_win10_redcon_first_run.log.  With the game's full
  level list the HARDWARE device still selects 9_1: the Surface 2's true maximum is 9_1 (the "Software Adapter" name is not WARP).
* Host fixes after it: the host Back button is off for non-XAML games unless --back-button-always; `_exit_app` no longer hangs 30 s at exit (it waited
  for CoreApplication.Exit, which never returns in a desktop process - 0.10.4 had the same hang): the host ends the process at once, like the guard did.
* m0probe crashed on the Surface 2 (arm32/device_logs/m0probe_surface2_varargs_crash.log): MSVC's <stdarg.h> va_start is wrong for clang on ARM
  (assumes MSVC argument homing).  Fixed with the __builtin_va_* macros; the host already used them.  Rule: never use <stdarg.h> va_start in
  clang-built ARM32 code - use __builtin_va_start/__builtin_va_arg.

### Bad Piggies Rebooted - the l9shim D3D11 shim, milestone 1 (2026-10-22)
Probe result on the Surface 2 (arm32/device_logs/m0probe_surface2.log): the Tegra 4 is a REAL GPU (adapter "NVIDIA Tegra 4", nvd3dum.dll loads, 28x WARP);
true max feature level 9_1; every game's shaders load at 9_1 (Bad Piggies 113/113 + sprite pixel correct).  The "Software Adapter" name is just
Microsoft's D3D10Level9 layer.  So Bad Piggies can render on the GPU at 9_1 with a shim between Unity and Direct3D.
* tools/badpiggies/l9shim.c (+ l9wrap.inc, l9shim.def): a d3d11.dll replacement that UnityPlayer's d3d11 import is redirected to (same import-redirect as
  Sup).  It forwards every d3d11 export to the real system d3d11.dll except D3D11CreateDevice: a FL10+ device is returned untouched (newer GPUs run Unity
  unchanged), a sub-FL10 device is recreated at 9_x and WRAPPED - a full-length private vtable copy (174 slots on PC, measured at run time; never size it
  from the SDK header, critic F2), VS/PS bytecode swapped by DXBC checksum from Data/bp/l9_replacements.bin (NOT CreateInputLayout, critic F1), and the
  texture/view/sampler/rasterizer/blend/depth-stencil descriptors sanitised to the 10Level9 rules.
* Builds: tools/badpiggies/build_l9shim_x64.bat (shim + harness l9shim_test.c), build_l9shim_arm32.bat -> dist-badpiggies/l9shim.dll (ARMNT, imports only
  KERNEL32, d3d11 loaded dynamically).  PC proof (L9SHIM_FORCE_9X=1): the raw 9_1 device refuses Unity's SM4 shaders and the illegal states/formats; the
  shim makes all of them succeed - all checks pass.
* REMAINING: M2 l9caps.inc - patch Unity's in-memory caps object so it never asks for 32-bit index buffers / MRT / instancing (ESSENTIAL: a single 32-bit
  index bind can remove the device at 9_1; the shim's creation-time sanitisers do not cover context-side binds).  Anchor RVAs in UnityPlayer sub_1007434
  (GetFeatureLevel blx 0x0100759c, caps table .rdata 0x026767b8 row 0, the hard-coded stores in games_recon/bad_piggies_hw-unitygfx.md F4/F5); guard on the
  expected bytes so it is a no-op on another build; device-gated.  M3 (optional) a DXGI Present hook for fps / device-lost.  M4 make_badpiggies_package.py:
  repackage the appx with the d3d11 import redirected + l9shim.dll + Data/bp/l9_replacements.bin, re-signed and registered.  Bad Piggies is a UWP appx with
  MinVersion 10.0.15035 (= the Surface 2), so it installs natively - no Win32Craft host, no HWND swap-chain translation.

### Bad Piggies l9shim - milestone 2: the 9_1 caps correction (2026-10-22)
tools/badpiggies/l9caps.inc (included by l9shim, armed from wrap_device, ONLY in the 9_x path - FL10+ devices untouched).  Unity's GfxDeviceD3D11 caps init
(UnityPlayer sub_1007434) treats a 9_1 device as FL 12_1 because its caps table has no 9_x row; it would then issue 32-bit index binds (device-removal at
9_1), MRT, instancing.  l9caps overwrites Unity's caps object (pointer at UnityPlayer base + 0x0298ed40) with 9_1 values: no 32-bit indices (c+0x104=0),
one render target (c+0xec=1), shaderCaps 0xE3 (keeps the shader programs), no UAV/3D/MSAA/mirror-once/readback/logic-op, maxtex 2048, cube 512, row index 0.
Every offset was confirmed by disassembling sub_1007434 (not just the recon text).  It is guarded by two anchor instructions so it is a silent no-op on any
other UnityPlayer build, and it fires once from the CheckFeatureSupport hook on Unity's last caps-init query (D3D11_FEATURE 0xf, OS build >= 1000).  Proof:
the l9shim_selftest_caps export checks every field against a synthetic caps object (0 wrong) and the full PC harness still passes; the ARM32 shim rebuilt
and still imports only KERNEL32.  Next: M4 = tools/badpiggies/make_badpiggies_package.py (repackage the appx with UnityPlayer's d3d11 import redirected to
l9shim + ship l9shim.dll + Data/bp/l9_replacements.bin, re-signed/registered) -> the first build testable on the Surface 2.

### Bad Piggies l9shim - milestone 4: the device package (2026-10-22)
tools/badpiggies/make_badpiggies_package.py -> dist-badpiggies/BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft(.zip).  Bad Piggies is a Windows 10 UWP appx
(MinVersion 15035 = the Surface 2), so it installs natively in Developer Mode (install.bat -> Add-AppxPackage -Register) - no host.  The packager unpacks
the clean game from the .appx (verifies UnityPlayer.dll sha256 e3530fd5...), redirects UnityPlayer's d3d11.dll import to l9d3d11.dll (the shim), app-locals
the VC++ runtime (renames the *_app imports to plain names and ships the VS2022 ARM VC143 redist, because the appx needs VCLibs.140 >= 14.0.33519 which we
cannot supply - armdeps has only 14.0.32530), drops the VCLibs.140 PackageDependency, ships Data/bp/l9_replacements.bin, and strips the packaged-install
metadata for -Register.  Verified statically: every patched binary imports only package-local or in-box (15035) DLLs; the shim and runtime are ARMNT.  NOT
run on a device yet - the first Surface 2 run writes l9shim.log; watch the splash shaders, the one missing IMGUI shader, and the CoreWindow swap chain
through the wrapped device (no dxgi/Present hook yet = M3, optional).  Windows RT 8.1 is end-of-project.

### Bad Piggies first device run: the l9shim varargs crash (2026-10-22)
First launch crashed (white->empty window).  Device l9shim.log (...\Packages\BadPiggiesRebooted_x0xr9mv5edyrp\AC\Temp\l9shim.log) + UnityPlayer.log
(stopped at "GfxDevice: creating device client") showed the shim's own logger was the culprit: fmt()/lgf() used MSVC's <stdarg.h> va_start, which is wrong
under clang-ARM (the same bug that crashed m0probe) - every logged value was garbage and a misread %s dereference faulted during D3D11CreateDevice.  Fixed
with __builtin_va_* (L9_VA_LIST).  The caps correction is re-enabled but content-gated (l9caps_on_check_feature only writes if the caps object's current
values look like Unity's GfxCaps), so a mis-identified pointer cannot corrupt memory.  Rebuilt dist-badpiggies.  RULE: never use <stdarg.h> va_start in any
clang-built ARM32 code in this repo - use __builtin_va_start/arg/end (the host already did; the probe and now the shim were fixed).
