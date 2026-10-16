> **2026-10-11 (later): Windows RT 8.1 (6.3.9600) works after the app-local runtime + LocalFree code shim; see HANDOFF_ARM32.md "Windows RT 8.1 loader issues (RESOLVED)".**
> **2026-10-11: the user ran the device package on a real ARM32 device and reported "it works" (no details given). See `HANDOFF_ARM32.md` for the current handoff; the sections below predate that and still say "nothing run on a device".**

# ARM32 port - status (updated 2026-10-11)

Goal: run the Minecraft 1.2.8 **ARM32** UWP exe (`Minecraft_1.2.8.0_ARM32/`) as a desktop app through `win32craft.dll`
(host + patcher) on Windows RT 8.1 / Windows 10 ARM32, **D3D11 at feature level 9_1** (devices top out at 9_1).
**Nothing has been run on a device** (this x64 box cannot execute ARM32). Everything is verified statically
(compile/link/disassembly/Unicorn emulation + independent reviewers).

## State: code complete, ready for a first device test
* `win32_host.c` / `api_compat.c`: all ARM code is under `#ifdef WIN32CRAFT_ARM32`; **x86 output proven identical** (`python tools/arm32/check_x86.py`).
  Pristine copies in `arm32/orig/`; the 125 unit edits (`arm32/edits/`) are applied IN PLACE; later glue/fix passes edited the repo files directly
  (`arm32/GLUE_LOG.md` lists every change and the status of every cross-unit finding).
* Build: `build_arm32.bat` -> `dist-arm32\win32craft.dll` (+ `win32craft_arm32.def`). Link needs armrt.lib, libcmt.lib, legacy_stdio_definitions.lib,
  legacy_stdio_wide_specifiers.lib, ucrt.lib, vcruntime.lib (all in the script). Builds embed date/time (banner), so every build hash differs: **run the package script LAST**.
* Patcher: `python tools/1.2.8-arm32-patcher.py <exe> <out> [--fmod-dll fmod_ARM.dll --fmod-out out] [--redirect-api-sets]` (uses the reloc-safe FMOD patch at file 0x78A4E).
* **Device deliverable (no Python/patching on the device):** `python tools/make_arm32_device_package.py` rebuilds everything and writes the complete ready-to-run
  folder `dist-arm32\device\Minecraft_1.2.8.0_ARM32_Win32Craft\` (+ `.zip`, ~63 MB) with README_DEVICE.txt, start_fl*.bat, use_silent_audio.bat/use_real_audio.bat.
* Package (overlay form) for the device: `python tools/make_arm32_package.py --force` -> `dist-arm32\package\` (patched exe, win32craft.dll, patched fmod_ARM.dll, 4 VC redist DLLs
  from VS `onecore\arm`, `fmod-silent\fmod_ARM.dll` fallback, MANIFEST.txt, INSTALL.txt). Copy these over the game folder (keep backups). User docs: `ARM32.md`.
* Silent FMOD fallback: `fmod_null/` + `build_fmod_null_arm32.bat` (39 exports, no imports, `arm32/tests/fmod_null_test.py`: 16 emulation checks pass).
* FL9_1: D3D11CreateDevice clamp to {0x9100} (option `--feature-level=...` / `WIN32CRAFT_FEATURE_LEVEL`); 197/197 game shader variants compile at `*_4_0_level_9_1`.
* Log: `win32craft.log` next to the patched exe (fallback %TEMP%), ON by default (`WIN32CRAFT_DEBUG=0` disables), every line `[ms Tthread]`, rotates at 4 MB,
  SUMMARY + loaded-module inventory at startup, abort/terminate traps. **First thing to ask the user for after a test: this log.**

## Verification tooling (all in tools/arm32/)
`arm32re.py` (RE toolkit), `check_x86.py`, `verify_constants.py` (4 known mislabel items only), `try_unit.py`, `apply_edits.py`, `gen_constants.py`
(do not re-run casually: it rewrites `win32_host_arm32.h`; fmod area is skipped), `facts_survey.py`, `shader_probe.py` (arm32/tests).

## Known open issues / device-only unknowns
1. Windows RT 8.1: UCRT + missing api-set contracts are now shipped app-local (see HANDOFF_ARM32.md item 1); still unverified on-device beyond the heap-l2 report: contracts the exe/DLLs import (processthreads-l1-1-2, file-l1-2-1, sysinfo-l1-2-1 for fmod) are unverified.
2. Real `fmod_ARM.dll` init waits INFINITE on WinRT DeviceWatcher events: if the game hangs at startup (log stops, no frames) swap in `fmod-silent\fmod_ARM.dll`.
3. `ClientInstance::vf3` render hook caller only identified statically: if it never fires, gameplay mouse capture / text-focus detection stay inactive (log reports after 5/15/30/60 s).
4. `drain_dispatcher_queue` has no exception containment: InputPane::GetForCurrentView throws when a text box takes focus (chat); first suspect cells 0x013695D0/0x013695D4
   (AppPlatform_Winrt vf11/vf12). x86 1.2.8 has the same exposure.
5. Heuristics shared with x86: main-menu exit-confirm click rectangle / `pending_main_menu_exit_confirmation` (can close the app on a click after ESC in Settings).
6. AppView::Run extras not emulated (client-removal restart, AppMain+4 restart); host `game_target` holds an extra RTV reference (ResizeBuffers by the game would fail INVALID_CALL, logged).
7. FL9_1 limits: textures/backbuffer <= 2048 (clamped), no MSAA, 16-bit index meshes only, wipe `ShaderCache2` if the feature level cap changes between runs.
8. 40 of 67 activated WinRT classes are not faked (real runtime answers on Win10; RT 8.1 may fail some): watch for `WinRT fallback` lines in the log.

## Next steps
* User runs the package on the device and sends `win32craft.log` (+ what is on screen). Iterate from the first failing log line.
* If audio hangs: fmod-silent. If text focus crashes: hook vf11/vf12 cells. If hooks report unexpected cell values: the exe is not the 1.2.8.0 ARM build in `facts`.
* Optional later: fmod option B' (route fmod's WinRT activation to host fakes; `arm32/edits/hstring_native/FMOD_HOOK.txt`), more x86 versions are out of scope.
