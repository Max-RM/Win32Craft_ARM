# ARM32 glue phase log (task glue_frame_path)

Single editor of `win32_host.c` / `api_compat.c` in this phase. Every change is ARM-only (`#ifdef WIN32CRAFT_ARM32`, x86 text
untouched; `python tools/arm32/check_x86.py` = IDENTICAL for both files). Nothing has run on a device.
Sources use LF line endings in this checkout (not CRLF); the Edit tool kept them.

## Where the log is written

`win32craft.log` is written **in the directory of the patched exe** (`Win32Bootstrap` makes that the working directory before the
first log line); if that cannot be created it falls back to `%TEMP%\win32craft.log`. The very first lines of the file are
`==== win32craft.log opened ====`, `Win32Craft 1.2.8 ARM32 Win32Bootstrap entered (host build <date> <time>)` and
`THIS LOG FILE: <absolute path>`; after option parsing a `SUMMARY:` line carries build, pid, image base, SizeOfImage, feature-level
cap, frame-limit option, log path and the command line. Logging is **on by default** on ARM32 (opt out with `WIN32CRAFT_DEBUG=0`).

## Unowned findings from PORT_RESULTS.md: status in the merged code

| finding | status |
|---|---|
| appmain: bind targets via `arm32_gfx_bind_frame_targets` | done here (change 4) |
| appmain/gfx: platform+0x330/0x334 = clamped size, `arm32_gfx_mirror_client_size` | `resize_game_window` already did it; `activate_game_app_128` fixed (change 5); CoreWindow.Bounds fake fixed (change 6) |
| iat_hooks: scissor hook clamp | done (change 7) |
| input_hid: scale mouse coordinates when client > buffer | done (change 8) |
| bootstrap: remove `ARM32_BOOT_RENDER_FRAME`, use int return | done (change 9) |
| bootstrap: exit on D3D / AppMain / activate failure | the codes 15/16/17 already existed; now they also show a message box and end the process (change 10) |
| bootstrap: install_arm32_input_hooks / client-state hook failure non-fatal | done (change 10) |
| bootstrap: WM_GETMINMAXINFO clamp | already present (`arm32_clamp_min_max_info`), checked: returns 0, safe before WM_NCCREATE |
| bootstrap: `WIN32CRAFT_DEBUG` forced on | already done by the bootstrap; now also built into `log_line` (change 1) |
| bootstrap: `ctrl_sensitivity2` default | done (change 11) |
| abi_audit: HSTRING leak in `fake_launcher_launch_uri` | done (change 12) |
| abi_audit: throttle `fake_core_window_dispatcher` (+ two siblings) | done (change 13) |
| iat_hooks: activation redirect before AppMain, `Win32CraftActivationDispatcher` | verified (trace, steps 6 and 9) |
| hstring_native: pre-resolve combase on the main thread | done (change 15) |
| 00_core: `log_line` thread safety / cached env / retry storm | done (change 1) |
| window_proc exit-confirm rectangle / `pending_main_menu_exit_confirmation` flaw | NOT changed, see "Open issues" |
| fmod WinRT hook (`install_arm32_fmod_winrt_hook`) | NOT installed: patcher default is the code patch of fmod_ARM.dll; B' stays an optional fallback |

## Changes (numbered)

1. **`log_line` (win32_host.c, ARM branch)**. The x86 function lazily `fopen()`ed without a lock, cached the
   `WIN32CRAFT_DEBUG` check, wrote nothing unless the variable existed, buffered through the CRT and retried `fopen` on every
   line when it failed. ARM version: on unless `WIN32CRAFT_DEBUG=0`; file opened once under a spin lock with
   `CreateFileW(FILE_APPEND_DATA, share r/w)`; every line is one `WriteFile` (no CRT buffer: nothing is lost on a crash or
   `TerminateProcess`); absolute path via `GetFullPathNameW`, fallback `%TEMP%`; at most 3 open attempts; the resolved path is
   published in the global `Win32CraftLogPathW`.
2. **api_compat.c `compat_log`**: appends to `Win32CraftLogPathW` (same file as `log_line`, independent of later working-directory
   changes) and uses the same "on unless `WIN32CRAFT_DEBUG=0`" rule.
3. **Identifying log lines**: `THIS LOG FILE:` line right after the first banner (bootstrap), `arm32_boot_log_startup_summary()`
   after option parsing.
4. **`render_game_target_128`**: the inline `GetClientRect` + `RSSetViewports` + `OMSetRenderTargets(game_target)` block is replaced by
   `arm32_gfx_bind_frame_targets()` (viewport = clamped buffer size; RTV/DSV re-read from `renderer+0x118/+0x120` every frame, so a
   resize or recreation can never leave the host bound to a stale view; `game_target` is refreshed with AddRef/Release). The frame is
   skipped (logged once) when the device was lost (`arm32_gfx_device_is_lost()`; the Present hook already posted `WM_CLOSE`) or when
   the renderer has no RTV. The `!game_target` pre-check was dropped (a failed resize used to freeze all frames forever).
5. **`activate_game_app_128`**: writes the swap-chain buffer size (`arm32_gfx_get_backbuffer_size`, = client size unless clamped at
   FL9_1/9_2 to 2048) to `platform+0x330/+0x334` and to `vf23 setSize` / `vf24 windowSizeChanged`, and calls
   `arm32_gfx_mirror_client_size()` (owner+0x94/+0x98, renderer+0xe8/+0xec). `resize_game_window` already did all of that.
6. **`fake_core_window_bounds`** (CoreWindow.Bounds read by `AppMain::init`): reports the buffer size instead of the client size so
   the first platform size is consistent with change 5 (forward declaration of `arm32_gfx_get_backbuffer_size`).
7. **`win32_set_scissor_rects`**: clamps to the buffer size instead of `GetClientRect` (and applies the old Y offset with it).
8. **Mouse**: new `arm32_scale_pointer_to_buffer()`; absolute client coordinates (`payload == 0`) are scaled by buffer/client in
   `queue_mouse_event` (float math, identity when equal; relative deltas untouched; logged once).
9. **`ARM32_BOOT_RENDER_FRAME`** (`__builtin_choose_expr`) removed: `quit = render_game_target_128();`.
10. **Bootstrap failure handling**: `arm32_boot_fail()` now logs, stops the watchdog, shows a message box (error code, step, reason,
    log path) and ends the process with `TerminateProcess(code)`; returning into the CRT `exit()` would run the static destructors of a
    half-initialised game (hang or crash with a dead window left behind). `install_arm32_input_hooks` failure is a warning (the
    x86 host ignored the result; only mouse-mode tracking is affected), as is the client-state hook. `install_crash_trace` moved
    before identification/option parsing. New exit guard: after the main loop a thread terminates the process if the CRT `exit()`
    has not finished within 30 s (log line says so).
11. **`create_user_data`**: first-run options.txt gets `ctrl_sensitivity2:0.33` (the ARM exe only reads `ctrl_sensitivity2`; the old
    `ctrl_sensitivity:0.33` was dead). `gfx_guiscale_offset` deliberately not written (value domain unverified).
12. **`fake_launcher_launch_uri`**: `WindowsDeleteString(text)` after use (the HSTRING from `IUriRuntimeClass::get_AbsoluteUri` is a
    real combase HSTRING on ARM and was leaked).
13. **Log volume**: `CoreWindow.Dispatcher`, `DisplayInformation.GetForCurrentView`, `ApplicationView.GetForCurrentView` log the first
    4 calls only; `win32_get_activation_factory` logs its "redirected ..." lines for the first 400 requests, afterwards only
    `WinRT fallback ...` lines (the evidence for what the game still needs from the real runtime); the vectored handler dumps
    registers/stack for the first 32 C++ throws only (the `_CxxThrowException` hook keeps a sampled count).
14. **`initialize_game_d3d`**: a failed `install_swap_chain_present_hook` now aborts (return code 15) instead of being ignored: without
    the hook DEVICE_REMOVED/RESET reach the game, whose `releaseDevice` frees host-owned objects, and the present bookkeeping of
    `render_game_target_128` double-presents.
15. **`Win32Bootstrap`**: `WindowsIsStringEmpty(NULL)` on the main thread before `RoInitialize`: resolves combase/wintypes outside any
    loader lock and puts the `HSTRING self-test PASSED/FAILED` line early in the log.
16. **Option parsing**: a trailing `--feature-level` without value is logged (was silent).

## Executor trace of the merged startup (what was checked)

1. Trampoline (facts winmain): `bl 0xC36EA0` -> PIC stub -> `.d3imp` IAT cell -> `win32craft.dll!Win32Bootstrap`, LR kept, return value
   -> `exit(r4)`; C++/CX `InitializeData` already ran. `int Win32Bootstrap(void)` ignores r0-r3. OK.
2. Identification: `e_lfanew`, machine 0x01C4, magic 0x10B, `SizeOfImage` at `pe+0x50` = 0x01731000 (stock 0x01730000 and fallback
   0x01732000 produce a specific hint). Offsets re-derived (pe+0x28 entry, +0x34 base, +0x5e DllCharacteristics). OK.
3. Options: `host_is_128` set before parsing; environment/command-line feature level; default cap 0x9100. OK (+16).
4. Crash trace, RoInitialize (RPC_E_CHANGED_MODE tolerated and logged), user data (`game_data_path` fallback next to the exe).
5. `game_image` is set first thing, before any hook and before the first `refresh_*`/`queue_*`.
6. `install_arm32_iat_hooks`: cells are resolved BY NAME in the live PE directories (normal + delay) and compared with the fact RVAs;
   `arm32_patch_cell` = PAGE_READWRITE only, expected-value check, icache flush; the hook is idempotent. Order: exception trace,
   activation redirect (initialises every fake object incl. `win32_core_window` BEFORE anything creates the owner/AppMain),
   `Win32CraftActivationDispatcher`, D3DCompile/D3DPreprocess, D3D11CreateDevice, TCUI delay cells (lazy-stub value check). All
   before the first use. Hook functions are plain AAPCS; stored pointers carry the Thumb bit.
7. Platform fixes (pickFile cell, getAsset cells, Xbox strings) and input hooks (3 std::function cells; bytes checked) and the
   client-state vtable cell: all data-cell patches before AppMain exists (the Xbox strings are read at controller registration).
8. Window: `WM_GETMINMAXINFO`/`WM_SIZE` arrive during `CreateWindowExW`, before `game_window` is set: `resize_game_window` returns FALSE
   (no swap chain), `update_mouse_capture` -> `game_requests_relative_mouse` returns early without `game_app_main`. OK.
9. `initialize_game_d3d`: operator new(0xd0) + ctor (bytes verified), `owner+0xb8 = 0x9100`, owner global published, `InitDevice`
   (D3D11CreateDevice hook: BGRA flag + level list clamped to {0x9100}; the game's own WARP retry also goes through it), device /
   context null-checks, scissor hook once, swap chain 2 buffers B8G8R8A8 DISCARD, size clamped to the feature-level limit,
   `MakeWindowAssociation(NO_ALT_ENTER)` (slot 8), IDXGISwapChain1 pointer preferred, `InitTargets` with the 0x20-byte display
   descriptor, swap chain stored at renderer+0x104, Present (slot 8) + Present1 (slot 22) hooks, first Present. COM vtable slots
   re-derived: GetFeatureLevel 37, GetDeviceRemovedReason 39, CreateTexture2D 5, RSSetScissorRects 45. Thread: main thread only.
10. `initialize_game_app_128`: factory bytes + `update_frame` bytes checked, fake CoreWindow initialised, static 8-byte-aligned StartupCtx
    (capacity 0xF at +0x1c), factory called with a plain AAPCS pointer (Thumb bit via `ARM32_THUMB_FUNCTION`), AppMain vtable / platform
    / game checked. Fake CoreWindow.Bounds now = buffer size (change 6).
11. `activate_game_app_128`: platform vtable[27]/[21] cell checks, calls; sizes (change 5); `vf23` float in s0 (hard-float); HID probe
    0x170 (`ARM_APPMAIN_HID_INPUT_HANDLER_PROBE_0X1D0`).
12. Main loop: PeekMessage, `drain_dispatcher_queue`, `refresh_relative_mouse_from_client_128`, `update_mouse_capture`, frame (change 4),
    quit -> `PostQuitMessage`. Watchdog + heartbeat run on a separate thread (no logging while the main thread is suspended).

## Open issues (no device, not fixable statically)

* Exit-confirm rectangle in `window_proc` (35..65 % x 54..65 % of the client area after ESC on any non-world screen) is still the x86
  heuristic: a click in that rectangle after ESC on Settings/Play can close the application. Needs the real GUI scale or the
  start-screen identity; left as is (shared with x86, risky to guess).
* The host keeps one extra reference on the render-target view (`game_target`). If the game itself ever resizes the swap chain
  (`createWindowSizeDependentResources`), `ResizeBuffers` fails with INVALID_CALL (logged, non-fatal, targets rebuilt).
* `drain_dispatcher_queue` has no exception containment: a C++/CX exception escaping a `RunAsync` lambda ends the process
  (VEH/`_CxxThrowException` hooks log it first).
* `AppView::Run` behaviours that are not emulated (client removal restart) and the process-exit static destructors (30 s guard).
* Everything the facts list as device-only: UCRT / api-sets on Windows RT 8.1, fmod_ARM.dll init (DeviceWatcher INFINITE waits),
  whether `ClientInstance::vf3` is called on the host thread (text focus / mouse capture), shader compilation at FL9_1.


## Fix pass

Inputs: 3 MUST_FIX and 5 SHOULD_FIX review findings. All claims were re-verified before changing anything. Final state:
`build_arm32.bat` OK (dist-arm32\win32craft.dll 143872 bytes), `check_x86.py` IDENTICAL for win32_host.c and api_compat.c,
`verify_constants.py` 78 arrays / 4 mismatches (only the known TARGET-BYTES/INCLUDE_INTERFACE items), package DLL sha256 equals
dist-arm32 DLL (d1af7af3...). Note: the sources are LF, not CRLF (as they always were; pristine copies in arm32\orig are LF too).

MUST_FIX
1. **Header regenerated with ARM_FMOD_* constants** (confirmed: header 330 -> 354 lines, verify_constants 4 -> 7 mismatches).
   `tools/arm32/gen_constants.py` now skips area `fmod` (those facts describe fmod_ARM.dll, which the patcher handles; no host
   source uses them), understands `--help` and `--out PATH` and rejects unknown arguments (it used to rewrite the repo header
   on any invocation). `win32_host_arm32.h` was regenerated: 330 lines, 0 ARM_FMOD hits. `verify_constants.py` also ignores
   `ARM_FMOD_*` as a second guard. Result: 78 arrays / 4 mismatches again.
2. + 3. **Stale deployable package** (confirmed: package DLL 136192 bytes, no 'THIS LOG FILE'). Rebuilt and re-ran
   `make_arm32_package.py --use-existing --force`; package DLL == dist-arm32 DLL, MANIFEST.txt regenerated. The script now (a) refuses
   to package a win32craft.dll older than win32_host.c / api_compat.c / win32_compat.h / win32_host_arm32.h / build_arm32.bat,
   (b) sha256-verifies every copied file against its source and the package DLL against dist-arm32. Always run the package script
   as the LAST step after the final build and send the `package` folder, not `dist-arm32`.

SHOULD_FIX (all done, ARM-only)
4. **Scissor correction evidence**: kept behaviour (PORT_RULES: keep) but every out-of-range rectangle (first 8 corrections) logs
   `ARM32 scissor: game submitted rect i/n = L T R B, limit (buffer) WxH` and flags the wholly-negative-Y translation. The device log
   now answers whether the x86 UWP offset exists on ARM; if a translated rect shows in-range values the translation must be
   disabled on ARM.
5. **Abort / terminate trace** (`arm32_install_abort_traps`, right after the crash trace): `signal(SIGABRT)` (resolved from
   api-ms-win-crt-runtime-l1-1-0.dll, fallback ucrtbase.dll), `_set_invalid_parameter_handler`, `_set_purecall_handler`. The handler
   logs reason, thread, stack frames with game RVAs, a stack scan and the LAST game C++ throw (number, thread, type). std::set_terminate
   was deliberately not used: the UCRT terminate handler is per-thread, whereas the SIGABRT action is process-wide and std::terminate
   ends in abort() which raises SIGABRT first. The invalid-parameter/pure-call handlers log and then TerminateProcess (keeping
   the default "process dies" outcome). __fastfail without abort() (e.g. Platform::Details::ReportFailure) cannot be intercepted;
   the preceding `_CxxThrowException` lines are its only trace. `_CxxThrowException` hook: stacks for the first 64 throws as before,
   then ONE short line per throw up to #3000 (was: every 64th), afterwards every 64th; the last throw is always recorded.
6. **Module inventory** (`arm32_log_module_inventory`, after "IAT hooks installed"): win32craft, fmod_ARM, vccorlib140, msvcp140,
   concrt140, vcruntime140, ucrtbase, d3d11, dxgi, d3dcompiler_47, combase, wintypes: base, file size, PE timestamp, SizeOfImage,
   path; plus a reminder line (silent stub 6144 bytes, patched FMOD 1118720 bytes).
7. + 8. **Log prefix / rotation**: every line (host `log_line` and api_compat `compat_log`) starts with `[ ms-since-first-line Tthread] `
   via the shared `Win32CraftLogPrefix` (win32_host.c, ARM-only). The log file is moved to `win32craft.log.old` when it exceeds 4 MB at
   open (MOVEFILE_REPLACE_EXISTING); the banner now carries local date/time and the pid.

Not changed / notes
* Build warnings: none new. The x86 build is untouched (all edits under WIN32CRAFT_ARM32 or in ARM-only files/tools).
* `tools/arm32/gen_constants.py` must be re-run only intentionally (it now accepts `--out` to write elsewhere).
