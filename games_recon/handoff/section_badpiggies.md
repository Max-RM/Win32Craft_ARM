STATUS: DONE
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
