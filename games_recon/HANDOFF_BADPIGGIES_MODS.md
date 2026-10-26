# Bad Piggies Rebooted on ARM Windows tablets: the Win32Craft port modifications

Hand-off for the developer who continues this work on an ARM Windows tablet. Written 2026-10-25.
Repo root `D:\repo\Win32Craft-main`; paths below are relative to it unless they start with a drive letter.
Everything here was read from the repo files named in each section. Where the files do not say, this document says "not recorded".

**Status in one paragraph.** Bad Piggies Rebooted 1.0.0.0 renders and runs on a Surface 2 (Windows 10 build 15035 ARM32, Tegra 4) at Direct3D
feature level 9_1 on the real GPU (not WARP): menus and gameplay draw correctly. The user's verdict after the last device run was "ran fine, but
still needs work"; what the remaining work is was not written down anywhere in the repo. The game's own code and assets are untouched. The port is
(a) a D3D11 shim, `l9shim`, loaded in place of `d3d11.dll`, (b) a table of 121 prebuilt level-9 shaders, (c) a packager that re-wires three PE
import tables. Surface RT / Tegra 3 / Windows RT 8.1 and the POCO X3 Pro have never run it.

## 0. Files to know

| What | Where |
|---|---|
| Shim source (log, table loader, exports, DllMain) | `tools/badpiggies/l9shim.c` |
| Shim: `D3D11CreateDevice` policy + device-vtable hooks | `tools/badpiggies/l9wrap.inc` |
| Shim: in-memory correction of Unity's caps object | `tools/badpiggies/l9caps.inc` |
| Shim export list | `tools/badpiggies/l9shim.def` |
| Build ARM32 / x64 + PC harness | `tools/badpiggies/build_l9shim_arm32.bat`, `build_l9shim_x64.bat`, `l9shim_test.c` |
| Packager | `tools/badpiggies/make_badpiggies_package.py` (reuses class `PE` from `tools/whatsapp/make_whatsapp_package.py`) |
| Shader table (game-derived) | `games_recon/bad_piggies_shader_proto/auto/l9_replacements.bin` / `.txt`, `needed/index.csv`, `all_programs.csv`, `shadowcasters/` |
| Shader tool chain (x64 Python + `d3dcompiler_47`) | `games_recon/bad_piggies_shader_proto/tools/` (`make_table.py`, `build_missing_l9.py`, `sm4tohlsl.py`, `sm4emu.py`, ...), `README.md` |
| Recon | `games_recon/bad_piggies_hw-unitygfx.md`, `-hw-shaders.md`, `-hw-content.md`, `-hw-priorart.md`, `-hw-design.md`, `-hw-critic.md`, `bad_piggies_bp-app.md`, `-bp-platform.md`, `bp_missing_shaders.md` |
| Mod bugs (not ours) | `games_recon/bp_managed_exceptions.md`, `games_recon/HANDOFF_FOR_BADPIGGIES_DEV.md` |
| Device probe + its Surface 2 log | `tools/m0probe/` (`README.md`), `arm32/device_logs/m0probe_surface2.log` |
| Overview hand-off for all games | `dist-handoff/Win32Craft_Handoff/HANDOFF_FOR_FRIEND.md`, `games_recon/handoff/section_badpiggies.md`, `HANDOFF_ARM32.md` (Bad Piggies sections at the end) |

`games_recon/bad_piggies_bench/{design,critic}/` holds the PC bench sources (`l9bench.c`, `crit_il.c`, `crit_vt.c`) and RE dumps. The game folders, the `.appx`,
`dist-*`, `build-*` and the game-derived parts of `games_recon/bad_piggies_shader_proto/{orig,auto,hlsl,build}` are excluded from git and from the public
source zip (`tools/make_source_package.py`), so they must be passed on separately. They contain the third party's assets: never commit them.

## 1. What Bad Piggies is and why it needed modifying

### 1.1 The game

- "Bad Piggies::Rebooted" (BP::RE) is a community mod of Rovio's Bad Piggies, not Rovio's own. Its public Unity project (github.com/Goggs77/bp-rebooted) is
  Unity 2021.3.45f2 plus DOTS Entities 0.51 and states it is "not open source". The package in this repo is a different build, by an unknown third party:
  Unity **2018.4.36f1**, **IL2CPP** (`GameAssembly.dll`, `global-metadata.dat` v24), UWP **ARM32** (`hw-priorart` F4).
- File: `BadPiggiesRebooted_1.0.0.0_ARM.appx` (92,677,462 bytes, sha256 `7dc84f1288f0aab26823d0121aaf131a31cf17c85760b87fbab2626386d7b04f`).
  Manifest: `Identity Name="BadPiggiesRebooted" Publisher="CN=_Imaginary_" ProcessorArchitecture="arm"`, `TargetDeviceFamily Windows.Universal
  MinVersion="10.0.15035.0" MaxVersionTested="10.0.19041.0"`. 15035 is exactly the Surface 2's build, so the package installs natively in Developer Mode.
  No Win32Craft host (`win32craft.dll`) is involved on Windows 10.
- `BadPiggiesRebooted.exe` (28 KB) is a plain CoreWindow `IFrameworkView` ("Unity UWP D3D" project type), not XAML (`bp-app` F1). All game logic is in
  `UnityPlayer.dll` and `GameAssembly.dll`.
- Content is mostly 9_1-friendly (`hw-content`): 907 meshes, all 16-bit indexed (max 126 vertices); textures only DXT1/DXT5/RGBA32/RGB24/Alpha8; all 48
  cameras orthographic with no render textures, lights, shadows or depth textures; one quality level "Fast" (no AA, no anisotropy); Gamma colour space.
  Exceptions: 1 texture over 2048 (`Innovation_VersionSelector_Background`, 2160x1080) and 3 NPOT textures with mips/Repeat (`loading` 124x124 with 7 mips,
  `Comic_DailyChallenge` 1024x652, and that same 2160x1080 one).

### 1.2 The problem: it is built for feature level 10, the Tegras stop at 9_1

- Surface RT = Tegra 3, Surface 2 = Tegra 4. Measured on the Surface 2 (`arm32/device_logs/m0probe_surface2.log`): the GPU is adapter 0 "NVIDIA Tegra 4"
  (user-mode driver `nvd3dum.dll` 9.17.13.2822); HARDWARE single-level requests for 11_1, 11_0, 10_1, 10_0, 9_3, 9_2 all return `0x887a0004`; 9_1 succeeds.
  Unity's exact first call (`EnumAdapters(0)`, UNKNOWN, `[11_1..10_0]`, flags `0x21`) returns `0x887a0004`: the stock game fails here.
- UnityPlayer's level list stops at 10_0: `sub_ffcba4` builds it (defaults at RVA `0x00ffcde2`..`0x00ffce22`, last entry `mov.w r2,#0xa000` at `0x00ffce1a`);
  `D3D11CreateDevice` (IAT `0x01717894`) is called from `sub_ffc7b4` at `0x00ffc844` and, as a WARP retry, at `0x00ffc88a`. The 2018.4 manual still lists
  `-force-feature-level-9-x`, but the player only has the 10-0/10-1/11-0/11-1 switches (Unity 2017.1 raised the UWP minimum to 9_3, 2018.2 dropped 9.x).
- Stock behaviour on a 9_1-only tablet: the hardware call fails, Unity silently retries on WARP (software, FL 11_1; `d3d10warp.dll` is present on 15035) and only if
  that fails too prints "At least DirectX 11 feature level 10.0 is required" (string RVA `0x01f74418`, referenced at `0x008c2812`). The WARP retry is read from code, not
  observed in a stock run. m0probe fill rate: Tegra 4 at 9_1 = 1055.7 Mpixel/s vs WARP = 37.7 (28.5x).
- Beyond device creation, everything else is also FL 10 (`hw-unitygfx`, `hw-shaders`):
  - Shaders: 82 Shader objects, 1128 sub-programs, 402 unique DXBC, all `vs/ps_4_0` (a few `_5_0`), no `Aon9` chunk. A 9_1 device accepts only `vs/ps_4_0_level_9_1`
    (anything else returns `0x80070057`; m0probe confirms for Unity's Sprites/Default pair).
  - Caps: Unity maps the real feature level through a table with no 9_x row and treats a 9_1 device as FL 12_1 (see section 3).
  - Formats/states: back-buffer depth `R32G8X24_TYPELESS`, typeless colour render targets, anisotropy 16, `ZClip False`, alpha-to-coverage, separate alpha blend,
    two-sided stencil, `MIRROR_ONCE`.
  - The package declares `D3D11_HWFL_10_0` in `StoreManifest.xml`; the packager deletes that file (whether a sideloaded package evaluates it is unverified).

### 1.3 What does NOT apply to this game

- **FMOD is statically linked inside `UnityPlayer.dll`** ("FMOD WASAPI Output" string at RVA `0x2693db8`; no `fmod*.dll` in the package; `bp-platform` F5). So the
  "clear the fmod APPCONTAINER byte" and audio-shim steps used for Geometry Dash and Minecraft 1.2.8 do **not** apply. The AppContainer bit is deliberately left set
  on the exe, `UnityPlayer.dll` and `GameAssembly.dll` because the game stays a real UWP app. If FMOD cannot open a device it falls back to a nosound output and the game
  keeps running. Whether sound works on the Surface 2: not recorded.
- No Win32Craft host work (fake CoreApplication, XAML fakes, input synthesis) on Windows 10. It is needed only for Windows RT 8.1 (section 7.3).

## 2. The l9shim approach

### 2.1 Entry point: import redirect

`UnityPlayer.dll` imports exactly three functions from `d3d11.dll`: `D3D11CreateDevice`, `CreateDirect3D11SurfaceFromDXGISurface`,
`CreateDirect3D11DeviceFromDXGIDevice` (`bp-app` F5). The packager rewrites that import descriptor's name to `l9d3d11.dll` (the shim, built as `l9shim.dll`).

- `l9shim.def` exports those three (plus a test-only `l9shim_selftest_caps` via `__declspec(dllexport)`). Only `D3D11CreateDevice` is real logic. The two WinRT-interop
  exports are pure forwards, resolved lazily with `GetProcAddress` on `System32\d3d11.dll` and returning `E_NOTIMPL` if absent (this is what lets RT 8.1, whose d3d11
  probably lacks them, load UnityPlayer).
  The comments in `l9shim.c` and `HANDOFF_ARM32.md` say the shim "forwards every d3d11 export"; it exports only these three.
- The shim is freestanding: no CRT, imports **KERNEL32 only** (15,872 bytes ARMNT), own `memset/memcpy/memcmp`, `armrt.lib` stubs for `__security_cookie`,
  `__security_check_cookie`, `__GSHandlerCheck`. The real `d3d11.dll` is loaded dynamically from the system directory.
- **`va_start` rule.** MSVC `<stdarg.h>` `va_start` is wrong under clang-ARM (assumes argument homing). The first device launch crashed in the shim's own logger because
  of it. `l9shim.c` re-defines `va_start/va_arg/va_end` as `__builtin_va_*` (`L9_VA_LIST`). Use the same in every clang-built ARM32 file.

### 2.2 Device policy (`l9wrap.inc`, `D3D11CreateDevice`)

1. Call the real `D3D11CreateDevice` with Unity's arguments unchanged. If it returns a device at FL >= 10_0 (`0xA000`), return it untouched: log "FL10+, the shim does nothing".
   This keeps the project rule "never force a feature level": a capable GPU runs Unity stock.
2. Otherwise release anything created, log the `hr`, and call the real function again with the same adapter/driver/flags and the list `{9_3, 9_2, 9_1}`. The runtime picks the
   highest the hardware supports (Tegra 4: 9_1). If this fails, the error is returned and Unity's own WARP retry runs as before (the silent WARP fallback is not disabled).
3. Query `D3D11_FEATURE_D3D9_SHADOW_SUPPORT` into `g_shadow_support`, wrap the device (2.3), return `S_OK` with the **true** level in `*pFeatureLevel` (Unity never reads it again).

Test hook: env `L9SHIM_FORCE_9X=1` skips step 1 so a PC with a real GPU exercises the wrap path.
Limit of the policy: it decides on feature level, not on adapter class (`hw-critic` F4 asked for an adapter check; not implemented). On the Surface 2 this is moot: adapter 0 is
the Tegra and refuses FL 10. If Unity were ever handed the Basic Render adapter (FL 11_1 on the CPU) the shim would step aside and the game would run in software.

### 2.3 The device wrap: a full-length private vtable copy

- `ID3D11Device`..`Device5` are the same object with the same vtable (QI returns the same pointer). The vtable lives in a read-only image page. The immediate context's
  vtable is in a private RW heap page that the runtime rewrites, so **no context methods are hooked**.
- `wrap_device`: count consecutive vtable entries that point into `d3d11.dll`'s image (cap `VT_MAX` 512), copy **all** of them into a static array, patch 14 slots, write the new
  vtable pointer into the device object. Refuses to wrap if fewer than 46 slots are found (slot 45 = `CreateBlendState1`).
- **Never size the copy from the SDK header.** Measured length: 174 on the PC; **121 on the Surface 2** (m0probe T6: `ID3D11Device`=43, `Device1`=50, `Device2`=54, `Device3`=65,
  `Device4`=67, `Device5`=69). A 54-slot copy dies with `0xC0000005` inside `CreateSwapChainForHwnd` (`hw-critic` F2): DXGI calls through the swapped pointer into slots past `ID3D11Device2`.
- Known blind spot: callers using `ID3D11Device3+` creation methods (`CreateTexture2D1`) bypass the slot-5 hook. Unity does not QI Device3.

### 2.4 Hooked slots

| Slot | Method | What the shim does |
|---|---|---|
| 5 | `CreateTexture2D` | `R8G8B8A8_TYPELESS`/`B8G8R8A8_TYPELESS` to `*_UNORM`; `R32G8X24_TYPELESS` and `R32_TYPELESS` to `R24G8_TYPELESS` (32-bit depth to 24-bit); `SHADER_RESOURCE` dropped from `R24G8_TYPELESS` only if `!g_shadow_support`; `A8_UNORM` forced to 1 mip; `MISC_SHARED` cleared on `R8G8B8A8`; sizes over 2048 are only logged |
| 7 | `CreateShaderResourceView` | `R32_FLOAT_X8X24_TYPELESS` to `R24_UNORM_X8_TYPELESS`; typeless colour to UNORM; on failure retry once with a NULL desc (view inherits the now-typed resource). Recovers the `d3d11: failed to create 2D texture shader resource view id=41 [80070057]` failure (attributed to an A8 texture in `games_recon/handoff/section_badpiggies.md`) |
| 9 | `CreateRenderTargetView` | typeless colour to UNORM |
| 10 | `CreateDepthStencilView` | `D32_FLOAT_S8X24_UINT` to `D24_UNORM_S8_UINT`; `D32_FLOAT` to `D16_UNORM` |
| 12, 15 | `CreateVertexShader`, `CreatePixelShader` | swap bytecode by DXBC checksum from the table (2.5) |
| 20 | `CreateBlendState` | `AlphaToCoverage` off, `IndependentBlend` off, alpha factors/op := colour factors/op, RT[0] copied to RT[1..7] |
| 45 | `CreateBlendState1` | as 20, plus `LogicOpEnable` off |
| 21 | `CreateDepthStencilState` | `BackFace := FrontFace` (no two-sided stencil at 9_1) |
| 22 | `CreateRasterizerState` | `DepthClipEnable := TRUE`, `DepthBiasClamp := 0` |
| 23 | `CreateSamplerState` | anisotropy clamp to 2 (see bug in 7.6), `MIRROR_ONCE` to `MIRROR` on U/V/W, comparison bit stripped when `!g_shadow_support` |
| 29, 37 | `CheckFormatSupport`, `GetFeatureLevel` | pass-through (answers are the device's true ones; `GetFeatureLevel` just sets a flag) |
| 33 | `CheckFeatureSupport` | pass-through, then `l9caps_on_check_feature(feature)` (section 3) |

Deliberately **not** hooked:
- `CreateInputLayout`. Unity never passes a shader there: it builds a one-chunk `ISGN` container with its own checksum (`sub_100fa88` / `sub_fff01c`), so a checksum swap would miss 100% of
  the time and leave the game with no geometry. The 9_1 runtime accepts that blob as is (`hw-critic` F1, proven on the PC and on the Tegra 4 in m0probe).
- Context methods, `CreateBuffer`, anything in DXGI (so there is no `Present` hook; section 7.4).

### 2.5 Shader swap

At `DllMain` the shim reads `Data\bp\l9_replacements.bin` next to the exe (format `L9SW`: `"L9SW"`, u32 version=1, u32 count, then `count` x {u8[16] checksum, u32 offset, u32 size}, then the
blobs; written sorted, read by linear scan). The key is bytes 4..19 of the DXBC container Unity passes. Hit: create the replacement. **Miss: the original bytes are passed through** to the driver,
which refuses them (`0x80070057`); the shim logs `MISS`, and dumps the first 8 misses as `l9_miss_vs_N.dxbc` / `l9_miss_ps_N.dxbc` beside the log. A missed program is simply invisible (Unity
logs `D3D shader create error for vertex/pixel shader [0x80070057]`), it does not fall back to the pink error shader. Logging is capped: the first 80 HIT lines and 40 MISS lines per stage.

### 2.6 Logs

Lines are written to `l9shim.log` and `OutputDebugString`. Open order: `%LOCALAPPDATA%`, then `%TEMP%`, then the exe folder. Inside the UWP AppContainer `%LOCALAPPDATA%` is the package's `AC` folder:
**observed location `%LOCALAPPDATA%\Packages\BadPiggiesRebooted_x0xr9mv5edyrp\AC\l9shim.log`** (on the first run under `AC\Temp\`; search `AC\` recursively). The source comment says `LocalCache`, which is
not what was seen. Unity's own log is `...\Packages\<family>\TempState\UnityPlayer.log`; saves are in `LocalState\`. The game folder itself is read-only to the app.

### 2.7 Build and PC test

```
tools\badpiggies\build_l9shim_arm32.bat            -> dist-badpiggies\l9shim.dll   (clang thumbv7-pc-windows-msvc, -march=armv7-a -mfpu=neon -mfloat-abi=hard -ffreestanding; lld-link /dll /machine:arm)
tools\badpiggies\build_l9shim_x64.bat              -> build-badpiggies\l9shim_x64.dll + l9shim_test_x64.exe
cd build-badpiggies && l9shim_test_x64.exe         (needs Data\bp\l9_replacements.bin and Data\bp\orig\*.dxbc in the cwd; exit 0 = all checks passed)
```
Overridable env: `LLVM_BIN SDKROOT SDKVER MSVCINC BUILD DIST`; defaults are `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin`, `D:\Windows Kits\10`, SDK 10.0.19041.0, MSVC 14.44.35207 includes.
The harness forces the 9_x path, then checks that Unity's original Sprites/Default SM4 bytecode is accepted via the swap (and refused on a raw 9_1 device), and that anisotropy 16, `DepthClip=FALSE`,
alpha-to-coverage + separate alpha, `R8G8B8A8_TYPELESS` and `R32G8X24_TYPELESS` are accepted through the shim and refused raw. It also calls `l9shim_selftest_caps`.
Re-run on 2026-10-25 on this PC: all checks pass. Note that the wrapped device on this PC came out as **FL 9_3** (`0x9300`), where anisotropy 16 is legal, so the harness does not prove the 9_1-only
rules (see 7.6). Never run the ARM binaries on the x64 PC.

## 3. The caps correction (`l9caps.inc`)

### 3.1 Why

Unity's D3D11 caps init is `sub_1007434` (called from `sub_100400c` right after device init). It maps the true feature level (`GetFeatureLevel`, vtable `+0x94`, call at `0x0100759c`) through a 6-row
table at `.rdata` RVA `0x026767b8` (rows `{FL, shaderCaps, maxTex, maxCube, maxUAV}`: `0xa000: 0x1fff,8192,8192,1` ... `0xb000/0xb100/0xc000/0xc100: 0x13ffff,16384,16384,8`). The lookup (`sub_100ef64`) returns
row index 5 when the level is not found, so a 9_1 device is treated as FL 12_1: compute, tessellation, instancing, 2D/cube arrays, 16384 textures, 8 render targets, plus constants that are
hard-coded true whatever the level (32-bit index buffers, MRT count 8, full NPOT, 3D textures, MSAA textures, mirror-once, async readback). Unity then issues calls a 9_1 GPU rejects. The
one measured to be fatal is a 32-bit index bind: on the PC's Intel HW device it **removed the device** (`0x887A0020`, DXGI_ERROR_DRIVER_INTERNAL_ERROR; WARP only logged an error; not measured on the Tegra).
The creation-time sanitisers cannot see that context-side bind, so the caps record itself is corrected (`hw-unitygfx` F4/F5, `hw-critic` F6).

### 3.2 Where, how, and the guards

- The caps object pointer is at `UnityPlayer.dll` base + **`0x0298ed40`** (`.data`; `movw/movt 0x1298ed40` in `sub_733428`).
- It runs **only in the shim's 9_x wrap path** (armed by `wrap_device` -> `l9caps_arm`), so FL10+ devices are untouched.
- **Two anchor-instruction guards** make it a silent no-op on any other UnityPlayer build: RVA `0x0100776c` must hold `86 f8 04 91` (`strb.w sb,[r6,#0x104]`) and RVA `0x0100763a` must hold
  `c6 f8 bc 30` (`str.w r3,[r6,#0xbc]`). Both were checked against the clean `UnityPlayer.dll` (sha256 `e3530fd5d6cc0e431e5edb69623b7c18e1a57a8013914d958c5f02711cb03793`) on 2026-10-25.
- **It fires once**, from the `CheckFeatureSupport` hook when Unity asks `D3D11_FEATURE` `0xf` (`D3D11_OPTIONS3`), Unity's last query in `sub_1007434`. By then the caps object is filled.
- A **content gate** guards the write: row index <= 8, shaderCaps not 0 / `0xffffffff`, idx32 byte <= 1, plausible `maxTex`. If the object does not look like Unity's `GfxCaps`, nothing is written.
  The log prints the values BEFORE the correction, so a device log shows the fix was needed. `g_caps_logonly` (default 0) switches to log-only.

### 3.3 Fields written

| Offset | Meaning | Unity's 12_1 value | Written |
|---|---|---|---|
| `+0x104` | Supports32bitsIndexBuffer (byte) | 1 | **0** (the one that device-removes) |
| `+0x0bc` | shaderCaps mask | `0x13ffff` | **`0xE3`** ("target 3.0") |
| `+0x0dc` / `+0x0e4` / `+0x0e0` | max texture / max RT / max cube | 16384 | 2048 / 2048 / 512 |
| `+0x0e8` | max 3D size | `0x800` | 256 |
| `+0x0ec` | render-target count | 8 | 1 |
| `+0x0f0` | UAV count | 8 | 0 |
| `+0x378`, `+0x37c` | NPOT support | 2 (full) | 1 (restricted: 1 mip, no Wrap) |
| `+0x3f4` | caps-table row index | 5 | 0 |
| `+0x394` | idx>=2 feature word | `0x4d` | 0 |
| bytes `+0x0ff`, `+0x101`, `+0x107`, `+0x36c`, `+0x375`, `+0x376`, `+0x386`, `+0x3d4`, `+0x3d5`, `+0x3d9`, `+0x3f8` | separate-RT blend, OutputMergerLogicOp, MSAA textures, mirror-once, 3D textures, 3D RTs, async readback, idx>=1/2 flags | 1 (hard-coded or derived from the row index; `+0x101` comes from the device and is already 0 on the Tegra 4, forced 0 as insurance) | 0 |

Why `0xE3` and not the honest `0x1`: `sub_742580` skips any sub-program whose requirement mask is not a subset of caps `+0xbc` (and whose tier does not match `+0xc0`). The programs the game
needs use masks `0x1` (210), `0x21` (318) and `0xE3` (180); `0x1` would drop TextMeshPro and `INShaders/*`. The bits are about the original HLSL target, not about what the level-9 replacements need.
Not written (left as Unity set them): `+0x3e4` CopyTextureSupport (`0x1f`), `+0xd4` texture-parameter limit (`0x40`), `+0x3f9` ConstantBufferOffsetting (=1 on the Tegra 4; Unity uses
`VSSetConstantBuffers1` windows; works, 12.7 us/draw), `+0xc0` tier, `+0xc9` reversed Z (legal).

The renderer string `Direct3D 11.0 [level %i.%i]` is built from the true level, so Unity's log honestly says "level 9.1".

## 4. The shader replacement pipeline

### 4.1 Inputs and outputs

- The game ships 82 Shader objects, all `platforms=[4]` (D3D11) only: 1128 sub-programs, **402 unique DXBC** (116 VS + 286 PS), no D3D11_9x variant anywhere. Some of them can never exist at 9_1 (comparison samplers, MRT, instancing, structured
  buffers, `_5_0` programs), but the game does not need those.
  Unity strips RDEF and does not import `d3dcompiler`, so it never reflects bytecode; it binds by its own metadata (cbuffer slot + byte offsets, texture/sampler slots). A replacement is therefore
  safe exactly when slots, offsets and input/output signatures are unchanged.
- The shipped table: `games_recon/bad_piggies_shader_proto/auto/l9_replacements.bin`, **121 entries, 318,668 bytes**, sha256 `8639327cbd923868271befab08aef58031ba1aefaad554c7ad2a67747bdc528e`, sorted by checksum.
  Listing: `auto/l9_replacements.txt`. Key = DXBC checksum of the ORIGINAL program; value = its `vs/ps_4_0_level_9_1` replacement.

### 4.2 What the 121 are

| Group | Count | Notes |
|---|---|---|
| Mechanical, from the 112 "needed" set | 107 | translated, proven, level-9 compiled |
| Hand-trimmed tier-1/2 lit FORWARDBASE pixel shaders | 4 | `hlsl/dummy_lit_forwardbase_tier1.hlsl`; checksums `4d87e50f...`, `3be91010...`, `9da1554f...`, `b18400dc...` (122-175 SM4 instructions do not fit ps_2_0's 64 ALU slots; trimmed under the game's conditions: no realtime light, no LPPV, no box projection/blend) |
| `UnityPlayer.dll` internal 2D blit pair | 2 | `6e4f5026...` (VS, 568 B), `5daf5b73...` (PS, 600 B); source blit table at RVA `0x29b15c8`, reader `sub_1003c98` |
| Shadow-caster programs (new in this build, was 113 entries) | 8 | `Legacy Shaders/VertexLit` and `Particles/Standard Unlit` SHADOWCASTER passes |

The eight shadow casters exist because the first Surface 2 run logged 4 MISSes during scene load (VS 1408 B `7eda7bd3...`, PS 212 B `21492659...`, VS 1508 B `e8e7fc89...`, PS 408 B `a8e80925...`), and 4 keyword
siblings were added as insurance (`699b5067...`, `c350ad9b...`, `f7e32a09...`, `ec874e42...`). No 9_x variant could be "lifted" from the asset: every shader has exactly one SubShader and only platform 4 programs,
and `VertexLit` (the Fallback of `Particles/Standard Unlit`, `e2d/Curve`, `Legacy Shaders/Diffuse`) has no level-9 programs either. So they were built from the game's own SM4 (`bp_missing_shaders.md` section 3).
The earlier "needed" filter had wrongly dropped SHADOWCASTER; whether Unity merely prepares these variants at material load or really draws them with shadows off is a hypothesis (H1), not verified.

### 4.3 How a replacement is made

`DXBC -> D3DDisassemble -> sm4tohlsl.py (register-level HLSL, same cbuffer slots via packoffset, same t#/s#, same semantics/masks) -> fxc vs/ps_4_0_level_9_1`. Every result is checked: SM4 recompile computes the same outputs as
Unity's original on 24 random input/constant sets (`sm4emu.py`, texture fetch is a deterministic function of slot+coords), input/output signatures are byte-identical, and it creates on an x64 WARP device limited to `{9_1}`.
113/113 of the original table creates on the **real Tegra 4** (m0probe T4). The 8 shadow casters were proven on WARP 9_1 only; their device creation was not recorded.
Level-9 sizes: PS max 52 slots (limit 64 ALU + 32 tex), VS max 178 slots (`Skybox/Procedural`; limit 256). `tex2Dlod`/`sample_l` on the reflection cubemap became `SampleBias(6.0)` in the 4 generated tier-0 lit PS and the 4 trimmed ones
(ps_2_0 has no `tex2Dlod`): a visual difference in reflection blur only.

Commands (x64 Python with `-I`; `d3dcompiler_47` path is hard-coded in `tools/d3dc.py` as `D:\Windows Kits\10\bin\10.0.19041.0\x64`; needs the game's `Data/`):
```
python -I tools/scan1.py ; scan3.py ; refs.py ; batch.py ; table.py        # full pipeline -> tools/out/ (all_programs.csv, needed/index.csv)
python -I tools/make_table.py                                              # packs needed/*.cso + hand files + blits + shadowcasters/ -> auto/l9_replacements.bin/.txt
python -B -I tools/build_missing_l9.py --data <Data> --tools <tools> --out <empty dir> --ck <32-hex DXBC checksum> ... [--table <bin> --merge-out <new bin>]
                                         [--preset misses|shadowcasters|all-mechanical]
```
To add a program seen as MISS: take the dumped `l9_miss_*.dxbc`, read its checksum (bytes 4..19), feed it to `build_missing_l9.py --ck`, put the `.l9.cso` in `auto/shadowcasters/` (or merge), re-run `make_table.py`,
then the packager. The shim does not need rebuilding: any count works.

### 4.4 The one known gap

`Hidden/Internal-GUIRoundedRect` pixel shader: DXBC checksum `e9fd52c1e47debe522506079e830aa65` (SHA-1 prefix `640b0073a23b`), 93 SM4 instructions, verdict "hand: derivatives (deriv_rtx)" (`ddx`/`fwidth`; ps_2_0 has no
gradients, and `GRADIENTINSTRUCTIONS` exists only from 9_3). Its VS (`8a4459b79917ee09e40a937b0ac38257`, 3052 B) is already in the table. The shader belongs to IMGUI (`OnGUI`) rounded rects, used by the debug scripts `CheatsUtility`
and `ProgressResetButton`. Effect of the miss: those rects are invisible (cosmetic). It is the only needed program without a replacement.

## 5. Packaging (`tools/badpiggies/make_badpiggies_package.py`)

Run `build_l9shim_arm32.bat` first (the packager copies `dist-badpiggies/l9shim.dll`). It needs `BadPiggiesRebooted_1.0.0.0_ARM.appx` in the repo root, the table above, and the VC143 ARM redist
(`D:\Program Files\vs22buildtools\VC\Redist\MSVC\14.44.35112\onecore\arm\Microsoft.VC143.CRT`, override with `WIN32CRAFT_REDIST_VC143`). Output: `dist-badpiggies\BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft\` and `.zip`.

1. **Unpack the clean game** from the `.appx` and verify `UnityPlayer.dll` sha256 `e3530fd5d6cc0e431e5edb69623b7c18e1a57a8013914d958c5f02711cb03793` (abort otherwise: the caps anchors are tied to this build).
   **Percent-decode every path segment.** The OPC part `Data/Resources/unity%20default%20resources` must land on disk as `unity default resources`. A naive unzip keeps `%20`, Unity cannot load its built-in resources, every
   built-in shader (including `Internal-ErrorShader`) resolves to NULL and the game crashes. (`dist-badpiggies/fix_badpiggies_resource.ps1` repairs an already-extracted folder.)
2. **Strip the packaged-install metadata** for the `-Register` route: `AppxSignature.p7x`, `AppxBlockMap.xml`, `AppxMetadata`, `[Content_Types].xml`, and `StoreManifest.xml` (the D3D11_HWFL_10_0 declaration).
3. **Redirect imports** (the `IMAGE_IMPORT_DESCRIPTOR.Name` is rewritten to a new string, `TimeDateStamp` zeroed):
   - `BadPiggiesRebooted.exe`: `VCRUNTIME140_APP`, `vccorlib140_app` -> `vcruntime140`, `vccorlib140`
   - `UnityPlayer.dll`: the same two, `MSVCP140_APP` -> `msvcp140`, and **`d3d11.dll` -> `l9d3d11.dll`**
   - `GameAssembly.dll`: `VCRUNTIME140_APP`, `MSVCP140_APP` -> plain names
4. **Ship `l9d3d11.dll`** (the shim) and the **VS2022 ARM VC143 runtime app-local** (8 DLLs: `vcruntime140`, `msvcp140`, `msvcp140_1`, `msvcp140_2`, `msvcp140_atomic_wait`, `msvcp140_codecvt_ids`, `vccorlib140`, `concrt140`).
   Reason: the manifest wants `Microsoft.VCLibs.140` >= 14.0.33519 and only 14.0.32530 is available. The Universal CRT is in-box on 15035 (measured on the Surface 2).
5. **Ship `Data\bp\l9_replacements.bin`.**
6. **Drop the `Microsoft.VCLibs.140.00` `PackageDependency`** from `AppxManifest.xml`. No other manifest change; it declares no capabilities, so there is no `internetClient` and the game's PlayFab/Rovio HTTPS calls fail and it runs offline.
7. Write `install.ps1/.bat`, `uninstall.ps1/.bat`, `README_DEVICE.txt`, `PATCH_REPORT.txt`, `MANIFEST.txt` (SHA-256 of every file), and zip.

Installation on the tablet: Developer Mode on (Settings > Update & security > For developers), extract the zip to the Desktop or `C:\Apps\...`, run `install.bat` -> `Add-AppxPackage -Register .\AppxManifest.xml -ForceApplicationShutdown`.
**Drive-root gotcha:** AppX deployment cannot register from the root of a drive (fails `0x80073CF9` / `0x80070020` "cannot access deployment target at C:"); `install.ps1` copies itself to
`Desktop\BadPiggiesRebooted` when it detects a root. The app runs from that folder, so do not move or delete it afterwards. On failure it writes `appxlog.txt`. To swap in a new shim on an installed copy:
`dist-badpiggies/update_badpiggies_dll.ps1` with the new `l9d3d11.dll` beside it.

Shipped artifacts (2026-10-22/23):

| Item | sha256 |
|---|---|
| `BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft.zip` (91.0 MB; 169 files, 272 MB unpacked) | `ad249622a2daf31b2c83f709657913ea6993e178abe09bcdc9fccaf3ee383a3e` |
| `l9d3d11.dll` (= `dist-badpiggies/l9shim.dll`, 15,872 B) | `e382b65f7c2362b72e0f3a7c0f465e3813430cd3a1eac861a505c4c6671a08cf` |
| `Data/bp/l9_replacements.bin` (121 entries) | `8639327cbd923868271befab08aef58031ba1aefaad554c7ad2a67747bdc528e` |
| clean / patched `UnityPlayer.dll` | `e3530fd5d6cc0e431e5edb69623b7c18e1a57a8013914d958c5f02711cb03793` / `59cdd1235002fdd9a43ce0052e4638b5788b5ebf36f3e618723d16409d355f61` |
| clean / patched `GameAssembly.dll` | `f5d25764b59f4f8353b8236295446c8e58f8f16b2b08cb304c0d7d053e0250df` / `0259e55c4184ea17c4fdc439a6b53641dd60c8273317dee432dc6606c3983f4d` |
| clean / patched `BadPiggiesRebooted.exe` | `6a75949c1b6e37b3a66049f0b94ca2f36154ad6acc163bf555f3e13acca5a016` / `1a05432e6a58777ca50463ac185417bdceca4754287df7a002b19539f694e188` |

The zip contains third-party assets and DLLs: a personal run-package only.

## 6. Current state and what is verified

### 6.1 State

- **Surface 2 (10.0.15035 ARM32, Tegra 4, 1990 MiB RAM): renders and runs on the real GPU.** Menus (including the version-select screen) and gameplay (level 1-1) draw correctly, per the user's report; remaining console errors are the three mod bugs in 7.1.
- Surface RT / Tegra 3 / Windows RT 8.1: never run. POCO X3 Pro (Win11 ARM64): never run. Frame rate, audio, memory behaviour on the Surface 2: not recorded.
- History of device runs: (1) crash, the shim's own `va_start` (fixed); (2) crash, the `%20` resource name (fixed in the packager); (3) ran with the 113-entry table: 25 VS + 25 PS HITs and 4 MISSes (`bp_missing_shaders.md`, appendix D);
  (4) the 121-entry package built 2026-10-23. **The device log of the 121-entry build is not in the repo** (the `E:` copy of the package data folder went offline), so "no MISS" for it is expected, not recorded.

### 6.2 Evidence ledger

| Claim | Evidence |
|---|---|
| The Tegra 4 is a real GPU at 9_1 | `m0probe_surface2.log`: adapter 0 "NVIDIA Tegra 4", `nvd3dum.dll` loaded, `d3d10warp.dll` not loaded; true max level 9_1. The "Software Adapter" vendor-0 name on the 9_1 device is the D3D10Level9 layer's label (`HANDOFF_ARM32.md`); the probe's SUMMARY text still prints a misleading "SOFTWARE renderer" line next to "GPU vendor driver module loaded: YES" |
| 113 replacements create on the Tegra; Unity's SM4 originals are refused | m0probe T4: 113/113 OK, originals 2/2 `0x80070057` |
| A drawn sprite reads back correctly | m0probe T5: pixel 200 100 50 255 with the replacement layout, with a signature-only input layout, and with `VSSetConstantBuffers1` windows |
| Full vtable copy + Unity-style swap chain survive | m0probe T6: 121 slots; `CreateSwapChainForHwnd` with Unity's desc (usage `0x30`, FLIP_SEQUENTIAL, flags `0x40`) OK; 30 Present, `ResizeBuffers` OK |
| Creation-time formats/states behave like the PC | m0probe T5 table: 82 lines, all `[same as PC]`, zero `DIFFERS`/`UNEXPECTED`. Includes: typeless and `R32G8X24` depth refused, `R24G8_TYPELESS` DS+SR OK (driver reports shadow support), `D24_UNORM_S8_UINT` DS+SR refused, A8 with 8 mips refused, NPOT with mips and 2160x1080 / 4096x2048 accepted at creation, `MIRROR_ONCE`, `ZClip False`, alpha-to-coverage refused |
| Speed | Tegra 4 9_1: 1055.7 Mpixel/s (200 alpha-blended 1280x720 quads, 174.6 ms/frame); WARP 37.7. CPU per draw with a cbuffer update ~12.7-13.2 us total (3.8-4.5 us submit) vs 80-92 us on WARP |
| Device caveats | D3D9_OPTIONS `FullNonPow2TextureSupport=1`, `D3D9_SHADOW_SUPPORT=1`, `SimpleInstancingSupported=1`, `OutputMergerLogicOp=0`, `ConstantBufferOffsetting=1` |
| Caps anchors match | verified on the clean `UnityPlayer.dll` (section 3.2) |
| PC harness | `build-badpiggies/l9shim_test_x64.exe`: all checks pass (2026-10-25; device was FL 9_3, see 7.6) |

### 6.3 Reading a device log in 60 seconds

Healthy Tegra path in `l9shim.log`, in this order: `shader table loaded: 121 replacements (318668 bytes)`; `the game's request did not give a FL10+ device (hr=0x...)` (expected `0x887a0004`);
`created a 9_1 device ... wrapping it`; `D3D9_SHADOW_SUPPORT=1`; `device wrapped (vtable 121 slots copied ...)`; `l9caps: armed`; `l9caps: caps object ... BEFORE: ...`; `l9caps: caps object ... corrected to 9_1`;
then `CreateVertexShader/CreatePixelShader HIT` lines and **no `MISS`**. If you instead see `D3D11CreateDevice -> 11_x ... the shim does nothing`, the GPU is capable and the shim is idle. If you see
`l9caps: ... anchors do not match` the UnityPlayer is a different build. Cross-check `TempState\UnityPlayer.log` for `D3D shader create error` and `failed to create 2D texture shader resource view`.

## 7. Known issues and what is left

### 7.1 The three mod bugs (NOT our port)

Full analysis: `games_recon/bp_managed_exceptions.md`; a clean bug report for the mod author: `games_recon/HANDOFF_FOR_BADPIGGIES_DEV.md`. All three are CPU-side C#/content logic and appear on any hardware.
1. **`PartListing.GetGreyMaterial` throws `ArgumentOutOfRangeException`.** Pass 2 loops to `normalMaterials.Count` (5) but indexes `renderQueueMaterials` (3), and the material `IngameAtlas4` (38 part icons, 157 renderers)
   was never added to `AtlasMaterials`. Fires only for locked parts, i.e. any fresh save; `CreateGUI` aborts so the in-game part list is incomplete. The one visible effect.
2. **`MenuContraptionManager.CreateContraption` NullReferenceException** (main menu, once; the menu keeps working). Almost certainly `Contraption.FindPig()` returning null. The exact null is **not pinned**; "not GPU related"
   is high-confidence, not proven. The cheapest decisive test: run the same appx on a real-GPU device (the POCO X3 Pro) and see whether the NRE reproduces.
3. **Missing scripts**: `BackgroundBox` x2 (null `m_Script` baked into `CrateCrazePopup` / `CoinSalePopup`) and `REConsoleInterface` (class absent from the IL2CPP build; pulled in via `Resources.Load("Innovation/INInitializer")`). Harmless.

Optional binary patch for #1 (not done, would be the first change to game code): `GameAssembly.dll` methodIndex 44635 at VA `0x1020fefc` (ImageBase `0x10000000`, Thumb-2); pass 2 reads `AtlasMaterials +0x14` (`normalMaterials`) where it should read `+0x1c`
(`renderQueueMaterials`), or fall back to `this+0x74` (`grayMaterial`). The exact instruction bytes are not located; recipe in the appendix of `bp_managed_exceptions.md`. Guard it with an expected-bytes check like `l9caps`.
The proper fix is registering `IngameAtlas4` in the mod's `AtlasMaterials` asset.

### 7.2 Missing IMGUI RoundedRect pixel shader

See 4.4. Cosmetic: the rounded rects of the two debug `OnGUI` scripts are invisible.

### 7.3 Tegra 3 / Windows RT 8.1: never tested

- m0probe has only been run on the Surface 2; no Surface RT log exists. Tegra 3 is expected to be 9_1 too (not measured). Unknown there: `D3D9_SHADOW_SUPPORT`, NPOT, vtable length, whether `nvd3dum` behaves like the Tegra 4 driver.
- The Windows 10 appx route is impossible on 8.1. The only route is a Win32Craft host (`bp-app` F8-F10): patch APPCONTAINER off the three PEs, a fake `CoreApplication`/`CoreWindow` with active event delivery, a `d3d12.dll` stub
  (UnityPlayer statically imports `D3D12SerializeRootSignature` and ordinals 101/102), app-local UCRT and api-set shims (6 api-sets unmeasured). Effort estimate: "weeks", unmeasured.
- `l9shim` and the table should be reusable as is, and the two interop forwards help the loader. **But the caps correction would not fire on RT 8.1**: it triggers on `D3D11_FEATURE` `0xf`, which Unity queries only when
  `sub_2b2104() >= 0x3e8` (Windows 10). The comment in `l9caps.inc` claims RT 8.1 too; that is wrong per `hw-critic` F6. A different one-shot trigger is needed (critic suggests a later device call, e.g. the first `CreateBuffer`
  from the GfxDeviceD3D11 constructor `sub_1001154`), or the 32-bit index risk returns. Unity also stores `+0x3e4 = 0x1f` after the OPTIONS1 call and nothing later corrects it on 8.1 (harmless for this game).
- If `D3D9_SHADOW_SUPPORT` is 0 on the Tegra 3, the shim drops `SHADER_RESOURCE` from the depth texture but Unity's back-buffer depth SRV request then fails; the critic's 1x1 dummy-SRV fallback was not implemented.

### 7.4 No DXGI `Present` hook

Design item M3 (optional), not built. Consequences: no fps counter, no `Present` error log, no `GetDeviceRemovedReason`, and no recovery if the device is lost (Unity then only prints "Skipped rendering frame because GfxDevice is in invalid state (device lost)",
string `0x01cf9880`). The swap chain Unity creates (`CreateSwapChainForCoreWindow`, usage `0x30`, FLIP_SEQUENTIAL, flags `0x40`) evidently works on the Tegra 4 because the game renders.

### 7.5 Unverified or not recorded

- Frame rate and smoothness on the Surface 2; audio; out-of-memory behaviour (2 GB RAM, 32-bit address space, ~110 MB of texture data if all resident); what the user means by "still needs work".
- The first-run log showed `Hidden/InternalErrorShader` programs being created (VS 532->1920 / PS 212->512, HIT #8) right after the first `Unlit/Transparent` draws: something in a scene drew with the error shader. Unexplained;
  check the next log for magenta objects.
- Whether the SRV NULL-desc retry actually recovered `id=41` in a real run, whether any texture over 2048 is created (only logged), the `Direct3D 11.0 [level 9.1]` string in Unity's log, POCO X3 Pro behaviour (the shim should step aside on its Adreno FL10+).
- Which graphics tier Unity picks (tier 0 vs 1/2); the table covers both.

### 7.6 Latent code issues found while writing this document (from reading the source)

- **Sampler anisotropy clamp (`l9wrap.inc`, `h_CreateSampler`) tests `(d.Filter & 0xC0) == 0xC0`.** `D3D11_ANISOTROPIC_FILTERING_BIT` is `0x40` and the comparison bit is `0x80`, `D3D11_FILTER_ANISOTROPIC` is `0x55`, so this only matches
  `COMPARISON_ANISOTROPIC` (`0xD5`). Plain anisotropic samplers with `MaxAnisotropy > 2` are not clamped and `CreateSamplerState` would fail at 9_1. Probably dormant (QualitySettings anisotropy is "Disable"; per-texture aniso 16 exists on 9 textures)
  and the PC harness cannot catch it because its wrapped device is 9_3. Fix: test `(d.Filter & 0x40)`, and make the harness able to force a 9_1-only list.
- `l9caps` is armed once; `g_caps_done` is never reset, so a second device creation would not get a second correction. Unity does not recreate the device today (it keeps running after device loss), but a device-loss recovery feature would need this.
- A `R32_TYPELESS` (shadow-map) texture maps to `R24G8_TYPELESS` while its `D32_FLOAT` DSV maps to `D16_UNORM`: inconsistent. Unreachable here (no shadows).
- The packager's "UnityPlayer imports d3d11 functions the shim does not export" check never runs: the `PE` class has no `imported_functions`, so the set compared is the shim's own. The recon says the import set is exactly the three exports.
- The hit/miss/fix counters (`g_vs_hit`, `g_ps_miss`, `g_tex_fix`, `g_state_fix`, `g_srv_fail`) are incremented but never printed; there is no `DLL_PROCESS_DETACH` summary.
- The device policy is not adapter-aware (2.2).

### 7.7 Operational gotchas

Developer Mode is required; do not install from a drive root; do not move the folder after `-Register`; the log is under `AC\`, not beside the exe; `README_DEVICE.txt` and `install.ps1` still point users at "this folder or %TEMP%" and say 113 replacements (Appendix A).

## 8. Possible further modifications and improvements

1. **Build the missing RoundedRect pixel shader.** Needs a hand-written `ps_4_0_level_9_1` with the same signature and slots as checksum `e9fd52c1...` (`auto/needed/index.csv` row `640b0073a23b`; disassembly via `build_missing_l9.py`'s loader), with `ddx/fwidth`
   replaced by an analytic or constant approximation (suggestion, not from the recon), proven with `sm4emu.py` under a fixed derivative, then added to the table. Low value (debug UI only).
2. **A `Present` / swap-chain layer (M3).** Per `hw-critic` F8: redirect UnityPlayer's `dxgi.dll` import the same way as `d3d11.dll` (it imports `CreateDXGIFactory1` IAT `0x017178b8`, `CreateDXGIFactory2` IAT `0x017178b4`) or patch the factory vtable cell; hook
   `CreateSwapChainForCoreWindow` (`IDXGIFactory2` vtable `+0x40`, Unity call in `sub_2f458c` at `0x002f45fe`) to log desc and `hr`, retry with `BufferUsage 0x20` on failure; give the returned swap chain a full-length private vtable copy with
   `Present`/`Present1`/`ResizeBuffers` hooks that count frames, log fps every few seconds, and log any failing `hr` with `GetDeviceRemovedReason`. Only in the 9_x path. This also makes frame time measurable, which is the missing number for every tuning idea below.
3. **Make bring-up less blind.** Print the counters at detach; log the adapter class and `GetDesc1` flags; optionally return a level-9 "magenta" program with the right input signature on a shader miss so a missing variant is visible on screen (`hw-critic` F9); add a `CreateBuffer`/render-target-view logger for multi-bind buffers and cube/array RTVs (F11).
4. **Fix 7.6** (sampler test, harness that forces 9_1-only, `g_caps_done` reset, depth-SRV dummy fallback, packager check) and add an adapter-aware policy: if the FL10+ device came from a software adapter (`DXGI_ADAPTER_FLAG_SOFTWARE`, vendor `0x1414`) and a hardware adapter offers 9_x, prefer the hardware one.
5. **Tegra 3 / RT 8.1.** First run `tools/m0probe` on the Surface RT (it records adapters, true max level, shadow support, NPOT, vtable length, shader creation, fill rate and CPU cost per draw in one log). Then the host route (7.3), a RT-capable caps trigger, and a real test.
6. **Performance ideas from the recon (all unmeasured on the game):**
   - Measured baseline: Tegra 4 blended fill 1055.7 Mpixel/s at 1280x720. `hw-design` F7 assumes overdraw 6 (not measured): 1920x1080 at 60 fps would need ~746 Mpixel/s (arithmetic from those assumptions), so 30 fps looks safe and 60 borderline; Tegra 3 (1366x768) is estimated at 30 plausible, 60 unlikely.
   - Only knob without code on the appx route: the tablet's Windows display resolution (Unity renders at CoreWindow size; a shim cannot shrink the swap chain behind its back).
   - If the CPU turns out to be the limit (Cortex-A15/A9, 4-13 us/draw): add a switch writing caps `+0x3f9 = 0` (turns off constant-buffer offsetting, `hw-critic` F7), reduce draw calls in content.
   - Memory: `QualitySettings.textureQuality` u32 at `Data/globalgamemanagers` file offset `0x18824` (1 = half-resolution of the 46 mipped textures only; the 2048x2048 atlases have 1 mip), converting the 4 RGB24 textures to DXT1 (saves ~12-20 MB).
   - Skip the Unity splash if it ever blocks bring-up: `m_ShowUnitySplashScreen` / `m_ShowUnitySplashLogo` bytes at `Data/globalgamemanagers` offsets `0x1074` / `0x1075` (the splash uses `Hidden/Internal-GUITexture` and `Hidden/BlitCopy`, so it is on the first-frame path).
7. **Content fixes for 9_1 strictness** (only if logs show failures): the three NPOT/oversize textures (`hw-content` F4/F5). With caps NPOT=1 Unity rounds up to POT and forces clamp (`loading` -> 128x128 + 8 mips; `Comic_DailyChallenge` -> 1024x1024; the 2160x1080 one -> 4096x2048, over 2048). The Tegra 4 accepted all of these at creation in the probe.
8. **Wider shader coverage if a new variant shows up as MISS:** `--preset all-mechanical` can build 173 more programs (table would grow to ~0.88 MB); 118 cannot be converted (54 comparison-sampler PS, 34 over the 2.0 limits, instancing/stereo/loop VS, structured buffers, ...). None has been requested in a logged run.
9. **POCO X3 Pro:** install the same package (`Add-AppxPackage -Register` with the `arm` package on Win11 is unverified; audio and touch unknown) to check that the shim steps aside and to settle the `CreateContraption` NRE.

## Appendix A. Discrepancies and stale text found in the repo files

| Where | Says | Actual |
|---|---|---|
| `make_badpiggies_package.py` docstring, `README_DEVICE.txt` text, `bad_piggies_shader_proto/README.md`, `hw-shaders` F8/summary, m0probe | 113 replacements | The shipped table has **121** entries (header of `l9_replacements.bin`, `PATCH_REPORT.txt`, `MANIFEST.txt`). 113 is the table m0probe tested and the first device run used |
| `l9shim.c` header, `HANDOFF_ARM32.md` | shim forwards "every d3d11 export" | `l9shim.def` exports 3 functions (the 3 UnityPlayer imports); only the 2 interop ones are forwards |
| `l9shim.c` comment | log findable under `...\LocalCache\...` | Observed `AC\` (first run `AC\Temp\`) under the package folder; `README_DEVICE.txt`/`install.ps1` say "this folder or %TEMP%" |
| `hw-design` F5, `bad_piggies_shader_proto/README.md` ("on a miss: log, return E_FAIL") | miss -> `E_FAIL`, "Unity then uses its error shader" | The shim passes the original bytes through; the driver refuses (`0x80070057`); the object is invisible (critic F9) |
| `hw-design` F5 | caps hook at the `CheckFeatureSupport` return addresses `0x010079e5` / `0x01007a41` | Implemented as "feature id `0xf`" in the hook; effective on Windows 10 only (7.3) |
| `l9caps.inc` comment | feature `0xf` is the last query on "Surface 2 / RT 8.1 (OS build >= 1000)" | RT 8.1 never makes that query |
| `hw-critic` F13 | always drop `SHADER_RESOURCE` on depth + dummy SRV | Implemented the "shadow-capable driver" alternative (`R24G8_TYPELESS` + DSV `D24S8` + SRV `R24_UNORM_X8`), proven OK on the Tegra 4; no dummy SRV |
| `hw-critic` F4 | classify adapters before deciding | Not implemented (moot on the Surface 2) |
| `hw-design` F1, `hw-critic` F4, `HANDOFF_ARM32.md` early text | "Software Adapter" may mean no GPU | Resolved by m0probe: real Tegra 4; but the probe SUMMARY still prints ">>> ... is a SOFTWARE renderer" next to "GPU vendor driver module loaded: YES" |
| `hw-unitygfx` F1/F2 | 4-byte patch to add 9_1 to Unity's list; disable the WARP fallback | Not used: the shim creates the 9_x device itself; the WARP fallback is left as is |
| `hw-critic` F2, `HANDOFF_ARM32.md` | vtable length 174 | 174 on the PC, **121 on the Surface 2** |
| `make_badpiggies_package.py` report text | lists 4 removed metadata files | It also removes `StoreManifest.xml` |
| `l9shim_test.c` comment/expectation "anisotropy clamped to 2" | proves the clamp | On this PC the wrapped device is 9_3, where 16 is legal; the clamp condition itself is wrong for plain anisotropic filters (7.6) |
