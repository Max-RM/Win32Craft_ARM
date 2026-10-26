# Geometry Dash (Win32Craft) — state, the fix that made it playable, and what to improve

For the dev friend continuing the port. Companion to `tools/gd/GD_BUILD.md` (the full living build log with every
decoded contract and milestone) and `games_recon/geometry_dash_gd-{shell,render,component,deps,platform}.md` (the recon).

---

## 1. Current state — PLAYABLE

Geometry Dash 2.0.1.0 **boots to the menu and plays** as native Win32 on the **Surface 2** (Windows 10 build 15035
ARM32, Tegra 4, Direct3D **feature level 9_1**). Confirmed on device 2026-10-25: the real menu, level select, and
gameplay render upright, music and sound-effects play (FMOD via the `PhoneAudioSes` shim), and touch/keyboard input
works. A level was played start-to-death ("NEW BEST! 3%").

What works end to end:
- The hand-written host (`tools/gd/gd_host.c` → `gd_host.exe`, ARM32, no CRT, KERNEL32+USER32 imports only) drives the
  Windows-Phone 8 `DrawingSurface` compositor contract directly — it supplies ~6 fake COM objects, calls the component's
  `Connect`/`PrepareResources`/`Draw` each frame, and presents the portrait render target rotated 90° into a landscape
  window. See `GD_BUILD.md` for the decoded call order (gd-component F11).
- ANGLE-for-WP8 (`libEGL_phone`/`libGLESv2_phone`) renders cocos2d-x into the host's D3D11 device.
- Asset paths, save folder, audio, and input are all host-provided; no Store/AppContainer/XAML shell.

A touch/jump input-latency rough edge was found and **fixed + confirmed on device** (see §4) — jumps are now responsive. Everything else is solid.

**The package to run:** `dist-gd-host/` (`gd_host.exe` + the 8 game DLLs + VC110 runtime trio + the 2 shims
`PhoneAppModelHost.dll`/`PhoneAudioSes.dll` + `Assets/`). Zipped as `dist-gd-host.zip`. Copy the folder to a
**non-root** location on the device (e.g. the Desktop) and run `gd_host.exe`.

---

## 2. The journey — what the real bug actually was (so you don't re-chase ghosts)

The symptom was: loads to ~80%, freezes, music loops, no crash. That symptom was **deeply misleading**. The real cause
was found only after building diagnostics into the host. The chain of wrong-then-right diagnosis:

1. **"Async loading stall / lost continuation."** Hypothesis: a PPL `task.then()` continuation was lost because the host
   returned `E_NOTIMPL` from `CaptureUiThreadContext`, so completions ran on the thread pool instead of the render
   thread. Implemented a working UI-thread `IContextCallback` that marshals continuations onto the render thread.
   **Wrong** — static RE (and the runtime counters `cutc_cap=0 ctx_calls=0`) proved GD's loading is a **synchronous
   14-step state machine** (`LoadingLayer::loadAssets`, GeometryJumpComponent `sub_1057c8`), each step paced by a
   self-re-arming `CCSequence(CCDelayTime(0.01), CCCallFunc)`. It waits on nothing async. The `CaptureUiThreadContext`
   hook is a **harmless no-op** and was left in place. The loading bar maxes at **91%** by design (7% × 13 steps; the
   final step does not move the bar), so "~80%" was simply the top of the bar.
2. **The real tell: the music.** `menuLoop.mp3` is only *played* at loading step 13 (which then does
   `replaceScene(MenuLayer)`). So the looping menu music proved loading had actually **finished** and the scene had
   switched — the frozen picture was a stale frame, not a stuck loader.
3. **CONFIRMED ROOT CAUSE: GPU device removal.** A per-frame health probe (added to the host) read
   `present=0x887a0005 (DXGI_ERROR_DEVICE_REMOVED)` + `removedReason=0x887a0020 (DXGI_ERROR_DRIVER_INTERNAL_ERROR)` +
   a frozen RT checksum, from early in loading onward. ANGLE *swallows* the removal (its `Draw` keeps returning `S_OK`),
   so the game logic sails on while nothing renders and the host re-presents a dead frame. ANGLE's own `debug.txt`
   (written to the process CWD) showed `rx::Image11::createStagingTexture failed` / `loadData: Could not map image` —
   the driver died during a **texture upload**.
4. **Why the driver died — the actual fix.** GeometryJumpComponent `sub_36390` (the ANGLE/EGL init) passes a
   feature-level index `r6` to `libGLESv2_phone!CreateWinrtEglWindow` (ordinal 181, call at RVA `0x36496`). `r6`
   **defaults to 3 = FL 9_3** (`movs r6,#3` at RVA `0x363fc`, overridden to 1/2 only for specific stored values). But
   the host's device is real **FL 9_1** (Tegra max). So ANGLE was driving a 9_1 device **as if it were 9_3**, and the
   first 9_3-only path during texture upload tripped `DRIVER_INTERNAL_ERROR` and removed the device.

**The fix (in `gd_host.c`):** a runtime in-memory `code_patch()` that **clamps the requested ANGLE feature level to the
device's actual maximum** — on a 9_1 device it patches `r6`'s default `movs r6,#3` (RVA `0x363fc`, byte `03`→`01`) and
the explicit-9_2 branch (RVA `0x3641a`, `02`→`01`). 9_2 device → `02`; 9_3+ → left at the game default. It verifies the
current byte before writing and never touches the shipped DLL on disk. This is a **per-device runtime policy (clamp to
hardware), not a blanket forced 9_1** — the correct approach (do not hardcode 9_1 everywhere; a more capable GPU must get
its real level). `CreateWinrtEglWindow` is an **ordinal** import (181), which the host's `iat_patch` can't hook (it skips
ordinal imports), so the code-patch at the `r6` source is the chosen mechanism.

**Lesson for future bring-ups:** when a ported game "hangs" but keeps running, suspect a **silently-removed GPU device**
before an async stall. The host now has the probes for it: per-second `[health]` line (RT checksum + `Present` HRESULT +
`GetDeviceRemovedReason`) and a per-frame "first device-removal" pinpoint that logs the last asset opened.

---

## 3. Diagnostics already built into the host (use these first)

All in `gd_host.c`, all write to `gd_host.log` next to the exe (and `%LOCALAPPDATA%` fallback):
- `[tid]` prefix on every line (tell the render thread from pool/worker threads).
- `[fopen #N] <path> (mode)` — every file the game opens (cap 3000); the game DLL + cocos2d + ANGLE are all traced.
- `[health @N] rtsum=0x.. (changing|FROZEN) present=0x.. removed=0x..` every 60 frames — the single most useful line.
- `!!! DEVICE REMOVED first seen at frame N ... last asset: <path>` — binds a removal to the triggering upload.
- `[counters @N] fopen=.. cutc_cap=.. cutc_sta=.. ctx_calls=..` at frames 300/900.
- `!!! VEH fault` / `!!! CRASH` with module+offset for any hard fault.
- `dump_rt_bmp` writes `gd_frame.bmp` (frame 0) and `gd_frame_900.bmp` (~15s in) next to the log — a BMP of the portrait
  RT, to see what actually rendered.
- ANGLE's own `debug.txt` appears in the CWD (and as `[fopen debug.txt]` lines) — read it for GL/D3D errors.

---

## 4. The touch/jump latency — diagnosis and the fix applied

**Symptom:** on the touch tablet, jumps (taps) feel delayed and mistime, so you die early. The pointer *plumbing* is not
the bottleneck — taps are delivered synchronously in the window proc (`send_pointer` → the component's manipulation
handler) before each `Draw`. The delay is from **how Windows delivers touch**.

**Root cause (high confidence):** the window originally handled only **mouse** messages (`WM_LBUTTONDOWN` etc.). On a
touch device Windows synthesizes those from touch, but only **after gesture recognition** — press-and-hold detection adds
~300–500 ms before/around promoting a touch to a mouse click. That promotion delay is the jump lag.

**Fix applied (build 2026-10-25 "11:57:32") — CONFIRMED working on the Surface 2: jumps are responsive, the delay is gone.**
- Handle raw **`WM_TOUCH`** (`0x0240`) directly via `GetTouchInputInfo` — touch arrives immediately, no promotion/
  gesture delay. `RegisterTouchWindow(hwnd, 0)` in `make_window`. Coordinates are hundredths-of-a-pixel screen → `/100`
  + `ScreenToClient`. Down/Up/Move → `send_pointer(0/2/1, …, dwID+2)`.
- **Disable press-and-hold** via `WM_TABLETQUERYSYSTEMGESTURESTATUS` (`0x02CC`) → return
  `TABLET_DISABLE_PRESSANDHOLD | TABLET_DISABLE_FLICKS`.
- **Dup guard:** the mouse handlers now ignore clicks within 1000 ms of a real `WM_TOUCH` (`g_last_touch`), so a late
  promoted mouse click can't cause a double-jump. The mouse handlers remain as a fallback for mouse/trackpad, so there is
  no regression if `WM_TOUCH` doesn't fire.
- **Cut the present queue:** `IDXGIDevice1::SetMaximumFrameLatency(1)` in `present_init` (DXGI defaults to 3 queued
  frames ≈ up to 50 ms). The present still uses vsync (`Present(1,0)`).

**If that isn't enough, next levers (in order):**
1. **Measure FPS.** Add a frame-time log (e.g. `QueryPerformanceCounter` around the loop, average per second). If GD is
   running below ~50 fps on the Tegra, that alone makes a rhythm game feel laggy and mistimes jumps. The present is
   vsync-capped at the display rate; the limiter is likely the per-`Draw` cost (ANGLE + cocos2d) on the Tegra 4.
2. **Drop vsync for lower latency:** try `Present(0, 0)` (tearing risk) or a `DXGI_SWAP_EFFECT_FLIP_*` + waitable swap
   chain. The current chain is blt/`DISCARD` (chosen because FLIP invalidated the back-buffer RTV and crashed early on;
   if you revisit FLIP, re-acquire the back-buffer RTV after each Present).
3. **Verify input timing vs. the game's `dt`.** GD's physics is time-stepped; cocos2d's `calculateDeltaTime` on WP8 uses
   `GetLocalTime`/`_mktime64` (see the loading recon). If `dt` is coarse/jittery the jump window shifts. Worth logging
   the per-frame `dt` the game computes if latency persists after the touch + FPS work.
4. **Confirm the tap maps to the right spot.** GD jumps on a tap anywhere, so spatial mapping shouldn't matter for jump,
   but the present rotation means window↔RT coordinates are rotated (see `send_pointer`); double-check for menus.

---

## 5. Other improvements / what's left

- **Generalize the FL clamp.** The current `code_patch` only lowers `r6` (9_1/9_2 devices). On a ≥9_3 device it leaves
  the game default (3), which is correct there. But the override branches in `sub_36390` could still request a level
  above the device on an exotic config — ideally clamp `r6 = min(requested, device_index)` rather than forcing the
  device index. Not needed for the current tablets; note it for ARM64/other GPUs.
- **Windows RT 8.1 / Tegra 3 (Surface RT):** never tested. Expect the same FL-clamp need (and possibly the ANGLE
  `9_3→9_1` matters even more). Run `tools/gd/dist-gd-loadcheck` there first (it already prints "0 problem(s)" on the
  Surface 2) to confirm all imports resolve, then `gd_host.exe`.
- **Remaining WinRT shims (on demand):** DisplayProperties, ApplicationData *save* (currently read paths work; writing
  high scores/settings), `Launcher` (for links), Accelerometer, `InputPane` (on-screen keyboard). Add them the same way
  as the Package/ApplicationData fakes in `hook_RoGAF` when a feature needs them. GD is playable without them.
- **The `[cc] ????` lines** in the log are ANGLE's `OutputDebugStringW` called with a narrow string
  ("WinPhone8XamlD3DWindow()"/"WinRtEglWindow()") read as UTF-16 — cosmetic, not a bug.
- **Resolution / aspect:** the host renders the portrait 480×800 RT into an 800×480 landscape window with a 90° rotation.
  If you want a different window size or fullscreen, adjust `g_win_w/g_win_h` and the swap-chain size (the rotate-quad
  UVs and `send_pointer` mapping must stay consistent — they were fixed once already when the first run was upside-down).

---

## 6. Build & run

- Build (on an x64 PC, **never run ARM binaries on the PC**): `tools\gd\build_gd_host_arm32.bat` → `dist-gd\gd_host.exe`.
  Toolchain: clang `--target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding
  -fno-builtin /nodefaultlib`; links `kernel32.lib user32.lib armrt.lib`. (RECURRING TRAP: use `__builtin_va_*`, never
  MSVC `<stdarg.h>` `va_start`, under clang-ARM — see `GD_VA` in `gd_host.c`.)
- The shims: `tools\gd\build_gd_shims_arm32.bat` → `PhoneAppModelHost.dll` + `PhoneAudioSes.dll`.
- Package: `python tools\gd\make_gd_host_package.py` → `dist-gd-host\` + `dist-gd-host.zip` (copies the latest
  `gd_host.exe`, the game DLLs with the fmod APPCONTAINER byte cleared, the VC110 trio, the 2 shims, and `Assets/`).
- Run on device: copy `dist-gd-host\` to a non-root folder (Desktop) and run `gd_host.exe`. Read `gd_host.log` + the
  `gd_frame*.bmp` + ANGLE `debug.txt` for diagnostics. The log header prints the build date/time — confirm it matches the
  build you shipped.

The tiny logger `lg()` supports `%s %S %d %u %x %p` plus `l`/width/`0` flag skipping — **never** use `%c`, `%lld`, or
`%%`; a format/arg mismatch dereferences garbage (this already cost one device run).
