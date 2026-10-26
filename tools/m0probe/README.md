# m0probe - the M0 device probe for Surface RT and Surface 2

One small console program that the user runs **once on each ARM32 tablet** (Surface RT = Windows RT 8.1 / Tegra 3,
Surface 2 = Windows 10 build 15035 ARM32 / Tegra 4). It answers, in **one log file**, the Direct3D questions that
decide three game ports (spec: `games_recon/GAMES_PLAN.md`, section "M0 device probe"):

| Game | What the probe settles |
|---|---|
| Redcon 1.3.0.0 (WP8.1 XAML) | do its 10 level-9 shaders and its 5 blend states (separate alpha) work on the real 9_1 device? |
| Geometry Dash 2.0.1.0 (WP8.0 Silverlight/ANGLE) | does a real FL 9_1 Tegra accept the 9 shader programs that are `level_9_3` (ps_2_x / vs_2_x)? or must they be replaced / WARP used? |
| Bad Piggies Rebooted 1.0.0.0 (Unity 2018.4 UWP) | is the "hardware" 9_1 device really a GPU? do the 113 level_9_1 replacements, the sprite draw, signature-only input layouts, the cbuffer-offset path and the full vtable copy + Unity-style swap chain work on a Tegra driver? |

Everything was proven on the x64 PC first (same source, `build_m0probe_x64.bat`); the ARM32 binary is never run on the PC.

## Files

| File | What |
|---|---|
| `m0probe.c` | the single source (x64 `cl` and ARM32 `clang`, freestanding, no CRT; imports KERNEL32 + USER32 only, everything else - d3d11, dxgi, d3dcompiler_47, version, advapi32, psapi - is loaded at run time) |
| `build_m0probe_arm32.bat` | builds `dist-m0probe\m0probe.exe` (same toolchain as `build_kbdprobe_arm32.bat`; object file in `build-x64probe\obj-arm32`) and lists the imports |
| `build_m0probe_x64.bat` | builds `build-x64probe\m0probe_x64.exe` (PC proof only, never ship it) |
| `make_m0probe_package.py` | fills `dist-m0probe\` with the shader data (Redcon decrypted by the script, GD containers cut out of the two DLLs, BP table + 2 originals), `run_probe.bat`, `README.txt`. `--out build-x64probe\run --exe build-x64probe\m0probe_x64.exe` lays out a PC test folder |

`dist-m0probe\` and `build-x64probe\` hold game-derived data and are skipped by `tools\make_source_package.py` (it drops every top-level `dist-*` / `build-*` folder).

## Build, package, run

```
tools\m0probe\build_m0probe_arm32.bat                      -> dist-m0probe\m0probe.exe   (prints the import list: KERNEL32 + USER32)
python tools\m0probe\make_m0probe_package.py               -> dist-m0probe\  (data\, run_probe.bat, README.txt)
```
Give the user the folder `dist-m0probe\`; they copy it to the tablet, double-click `run_probe.bat`, wait ~2-3 minutes and send back `m0probe.log`.

PC proof run:
```
tools\m0probe\build_m0probe_x64.bat
python tools\m0probe\make_m0probe_package.py --out build-x64probe\run --exe build-x64probe\m0probe_x64.exe
build-x64probe\run\m0probe.exe          (--quick skips the speed tests; --extra idx32 --extra vtshort run the explicit-only tests)
```

## How it works

* The **parent** prints T1 itself, then re-launches the same exe as `m0probe.exe --test <name> [--case <c>]` for every test that talks to a
  graphics driver. It waits (60-90 s), logs the exit code, and goes on. A driver crash prints `[run] ...: CRASHED ... exit code 0x...` and the
  child's own `!!! UNHANDLED EXCEPTION ... in <module> + <offset>` line (so a crash inside an NVIDIA UMD names the DLL); a hang prints `TIMEOUT`
  and the child is terminated. One broken test never costs the rest of the log.
* Children append to `m0probe.log`; they also write `key=value` lines to `m0probe.res`, from which the parent builds the SUMMARY block. An older log is kept as `m0probe.log.old`.
* All output is plain ASCII with CRLF line ends and is mirrored on the console.

## The tests and how to read them

Every line `X hr=0x........ as-expected | UNEXPECTED | info` is a D3D call; `as-expected` compares with what the PC returned
(`games_recon/bad_piggies_bench`), `info` is just recorded. Table rows end with `[same as PC]` or `<<<< DIFFERS FROM PC` - **grep the log for `DIFFERS`
and `UNEXPECTED`**: those are the tablet-specific behaviours. Per-test status at the end: PASS (all expectations held) / FAIL / INFO (measurements only) / CRASH / TIMEOUT.

| Test | Runs as | Answers | Look at |
|---|---|---|---|
| **T1 system** | parent | OS version (RtlGetVersion), CPU count/name, RAM, exe path | `OS (RtlGetVersion)`, `CPU` |
| **T2 adapters** | `adapters` | every DXGI adapter: name, VendorId/DeviceId/SubSysId/Revision, dedicated/shared memory, `DXGI_ADAPTER_FLAG_SOFTWARE`, LUID, user-mode driver version; does `System32\d3d10warp.dll` exist (+ version); NVIDIA-looking files in System32 and DriverStore; the display-class registry keys with `InstalledDisplayDrivers` / `UserModeDriverName` | `class:` per adapter (`NVIDIA hardware` / `hardware` / `SOFTWARE`), `display class key` block |
| **T3 feature levels** | `fl hw`, `fl a0..aN`, `fl warp`, `fl unity`, `fl hwsingle`, `fl warpsingle` | `D3D11CreateDevice` with the full list `[11_1..9_1]` for (a) NULL adapter + HARDWARE, (b) every adapter + UNKNOWN, (c) WARP (retry without 11_1 on `E_INVALIDARG`) -> hr and chosen level; Unity's exact first call (`EnumAdapters(0)`, UNKNOWN, `[11_1..10_0]`, flags 0x21) -> what the stock game gets today; single-level requests. For every device: D3D9_OPTIONS (FullNonPow2), D3D9_SHADOW, D3D9_SIMPLE_INSTANCING, D3D11_OPTIONS (OutputMergerLogicOp, **ConstantBufferOffsetting**, ...), D3D10_X_HARDWARE_OPTIONS, ARCHITECTURE_INFO (tile based), and the **modules loaded by device creation** | `chosen feature level`, `runs on adapter ... -> SOFTWARE/hardware`, `>>> new module after device creation: ...d3d10warp.dll <-- WARP` or `... <-- NVIDIA user-mode driver?` |
| **T4 shaders** | `shaders hw`, `shaders warp` | a device with **only `{9_1}`** (exactly what the Win32Craft host asks for): `CreateVertexShader/PixelShader` for all 10 Redcon blobs, the 30 GD containers of libcocos2d.dll (flag: used by the game or not; payload `level_9_3`) and the 10 ANGLE internal ones of libGLESv2_phone.dll, the 113 BP replacements, and the BP Unity originals (must be **refused**, `0x80070057`) | `Redcon shaders created at 9_1: 10/10`, `Geometry Dash ... 30/30` and `... USES: 18/18`, `Bad Piggies replacements ... 113/113` |
| **T5 render** | `render draw / il / cboff / blend / table` | `draw`: Unity's Sprites/Default through the checksum lookup, draw, read-back (**expect `200 100 50 255`**). `il`: input layout from a **signature-only DXBC** (header + one ISGN chunk + fresh checksum, built and checksummed by the probe - the checksum code is self-checked against Unity's own file) and three variants (original VS ok, replacement VS ok, zero checksum refused), then the same draw. `cboff`: constant buffers bound as 4 windows of one 64 KB buffer with `VSSetConstantBuffers1` (skipped when the device reports no ConstantBufferOffsetting). `blend`: Redcon's depth-stencil / 2 rasterizers / default sampler / **5 blend states with fixed alpha factors ONE/ZERO/ADD** are created, then each is **drawn and the pixel compared with the arithmetic result**. `table`: CheckFormatSupport rows and the texture/sampler/rasterizer/blend/query/buffer table (A8 with mips, B4G4R4A4, BC1/BC3 NPOT, 124x124x7 NPOT mips, 2049x16, 2160x1080, D24S8 (+SRV), R24G8_TYPELESS+SRV, R32G8X24 depth, RGBA8 TYPELESS RT, anisotropy 16 vs 2, MIRROR_ONCE, DepthClip off, AlphaToCoverage, VB\|IB, occlusion/timestamp queries) | `read-back centre pixel`, `[ ok ]/[FAIL]` rows of the blend check, `DIFFERS FROM PC` rows |
| **T6 vtable copy** | `vt` (`vtshort` explicit only) | the Bad Piggies shim idea: measure the device vtable length (consecutive entries pointing into d3d11.dll, cap 512), swap in a private **full** copy with 5 hooked slots, create an HWND swap chain like Unity's (B8G8R8A8, 2 buffers, FLIP_SEQUENTIAL, usage 0x30, flags 0x40; falls back with a log line if refused), 30 x Present, ResizeBuffers (fails while the RTV is alive, works after), `ID3D11Device3` call through the copy | `vtable swapped (N slots ...)`, `CreateSwapChainForHwnd [...]`, `30 x Present`, `ResizeBuffers 800x450` |
| **T7 speed** | `speed hw91`, `speed hwbest`, `speed warpbest` | offscreen 1280x720: `N` alpha-blended full-screen textured quads per frame (200, scaled down when a frame takes more than 1 s) synchronised with an event query -> **ms/frame and Mpixel/s** for HARDWARE `{9_1}`, HARDWARE best level (skipped when the best level is 9_1), WARP best level. CPU draw-call cost: 500 small draws per frame with a constant-buffer update per draw: `Map(DISCARD)`, `UpdateSubresource`, and (if offered) `VSSetConstantBuffers1` windows -> us per draw | `FILL RATE:`, `CPU draws [...]` |

Explicit-only tests (run_probe.bat does not call them): `m0probe.exe --test idx32` (`IASetIndexBuffer(R32_UINT)` at 9_1: removed the Intel device on the PC),
`m0probe.exe --test vtshort` (54-slot vtable copy: crashes inside DXGI at swap-chain creation on the PC, proving why the copy must be full length).
`--extra idx32 --extra vtshort` on the parent runs them after the normal plan; `--extra selftest_crash` / `selftest_hang` prove the parent's crash and timeout handling.
Other switches: `--quick` (no speed tests), `--no-spawn` (run every test inside the parent process - also the automatic fallback when `CreateProcess` fails, e.g. under a code-integrity policy; no crash isolation then),
`--summary-only` (re-print the SUMMARY from an existing `m0probe.res`), `--test selftest_fmt` (checks the own printf and the DXBC checksum code against reference values; run it on the tablet if the log text looks garbled).

### The SUMMARY block (last part of the log)

```
GPU: <name> (NVIDIA hardware adapter, vendor 0x10DE) | NO GPU DRIVER: software adapter only ("Software Adapter")
Default HARDWARE device (what our host and Unity get) runs on: "<name>" -> hardware | SOFTWARE
True max feature level: HW <x>, WARP <y>
Unity's exact first call ...: hr=..., level ...        (does the stock game start, and on which device?)
[HARDWARE {9_1}] Redcon shaders at 9_1: n/10
[HARDWARE {9_1}] GD 9_3 shaders at 9_1: n/30 (... used by the game: n/18); ANGLE internal level_9_1 shaders: n/10
[HARDWARE {9_1}] Bad Piggies replacements at 9_1: n/113; Unity originals refused (expected all): 2/2
Bad Piggies sprite draw + read-back: pixel 200 100 50 255 -> OK   (+ signature-only layout, + VSSetConstantBuffers1)
Redcon blend states drawn and read back correctly: 5/5
Device vtable copy: OK, measured vtable length N slots
Fill rate (1280x720 ...): HARDWARE {9_1} a Mpixel/s | HARDWARE best b | WARP best c    + the ratio and a verdict line
PER-TEST RESULT ...
```

Decision rules (from `bad_piggies_hw-design.md` F1/F4 and `geometry_dash_gd-render.md` F7/F10):

* **Is it a GPU?** `NO GPU DRIVER`, class `SOFTWARE`, VendorId 0 or 0x1414, `d3d10warp.dll` loaded by the HARDWARE device creation, and a HARDWARE/WARP fill-rate ratio below ~1.5 all point to a software renderer: then the "hardware" 9_1 device is not a Tegra and no GPU path exists on that OS image. A vendor module in `DriverStore` / `nv*.dll` loaded after creation, an NVIDIA adapter and a ratio of 5x or more mean a real GPU.
* **True max feature level.** If HARDWARE gives >= 9_3 (or the software adapter does) Geometry Dash can run its shaders unchanged (and ANGLE must be told the real level, gd-render F4); if only 9_1, the `GD ... USES: 18/18` line decides whether the 9_3 containers are accepted anyway or the 18 level_9_1 replacements are needed (F7).
* **Bad Piggies:** all of `113/113`, `2/2`, the three pixels `200 100 50 255`, `vtable copy: OK` and no `DIFFERS FROM PC` in the table are the gate for building the shim; any `DIFFERS` row (for example A8 with mips, `AlphaToCoverage`, `MIRROR_ONCE`, NPOT) is a sanitiser rule to add. `FullNonPow2TextureSupport`, `ConstantBufferOffsetting`, `OutputMergerLogicOp` per device drive the caps fix-up.
* **Redcon:** `10/10` and `5/5` and no `UNEXPECTED` in the blend section.
* **Speed:** compare `FILL RATE` and `us per draw` with the PC numbers below; a frame budget for a 2D game at 60 fps is 16 ms.

## PC reference (x64, Surface Book 2: Intel UHD 620 + GTX 1060 + WARP; `build-x64probe\run\m0probe.log`)

* HARDWARE and WARP both give 11_1; every single level 11_1..9_1 can be requested; Unity's first call succeeds on the Intel GPU.
* Redcon 10/10, GD 30/30 (18/18 used, ANGLE 10/10), BP 113/113, originals refused 2/2 - on HARDWARE `{9_1}` and on WARP `{9_1}` (this PC accepts the 9_3 shaders, which proves nothing about Tegra).
* Sprite pixel `200 100 50 255` for the replacement layout, the signature-only layout and the `VSSetConstantBuffers1` path; zero-checksum signature refused (`0x80070057`); the DXBC checksum code reproduces the file's checksum.
* Blend states functional 5/5. Format/state table: 0 differences from the bench (`bench_hw.txt`), plus rows measured today (A8 1 mip ok, BC1 NPOT ok, 2049x16 ok, OCCLUSION_PREDICATE refused, ...).
* Vtable: 174 slots, swap chain with Unity's desc OK (DXGI makes 16 QI calls into the hooked slot 0 and none into the others), 30 Present, ResizeBuffers while the RTV is alive `0x887a0001`, after release OK; `CreateTexture2D1` through the copy bypasses the slot-5 hook.
* Speed (x64): Intel UHD 620 `{9_1}` about 4700 Mpixel/s (39 ms per 200 quads), WARP about 350 Mpixel/s (530 ms), 1.6 us per draw on the GPU path, 31 us per draw on WARP.

## Limits (what this probe cannot show)

It measures what the runtime and driver accept and what a draw reads back. It does not judge precision (FP20 pixel shaders on Tegra), the visual result of NPOT + WRAP sampling, texture memory limits under load, or sustained thermal behaviour. The fill-rate test is one alpha-blended textured layer; real games overdraw more and use several textures.
