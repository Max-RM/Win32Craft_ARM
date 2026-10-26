# New games: go / no-go and plan (2026-10-22)

Sources: the 15 recon files in this folder (all `STATUS: DONE`). Workflows `wf_21d6ddae-98a` (11 areas)
and `wf_e42b4401-720` (Bad Piggies GPU route, 6 areas). Main-session checks are marked **[checked]**.

## Verdicts

| Game | Verdict | Route | Render at FL 9_1 | Size |
|---|---|---|---|---|
| **REDCON 1** 1.3.0.0 (WP8.1 XAML) | **GO - do first** | 0.10.4 fake-object host + a `redcon` profile + one new unit | **native**: the game asks for 11_1..9_1 itself **[checked: Redcon.exe 0x6DF98..0x6DFC0]**; its 10 shaders already carry level-9 (SM2.0) payloads | ~3-3.5k lines, ~1/4 of the 0.10.4 effort |
| **Geometry Dash** 2.0.1.0 (WP8.0 Silverlight) | **POSSIBLE** | new host type: replace the Silverlight shell + DrawingSurface compositor (4-method contract, decoded) | 9 shader programs are level 9_3 and the game tells ANGLE "9_3"; fix = 18 tiny level_9_1 shaders + FL override (proof of concept done), or WARP | medium; imports are easy (VC110 trio from VCLibs 110 **[checked]** + 2 small shim DLLs) |
| **Bad Piggies Rebooted** 1.0.0.0 (Unity 2018.4 UWP) | **POSSIBLE, biggest; gated on M0** | D3D11 shim ("S+") + appx on Surface 2, or Win32Craft host on Surface RT | Unity needs FL 10; shim swaps in level-9 shaders (111/112 ready, tested on a PC GPU forced to 9_1) | 3-5 weeks (Surface 2 GPU) or 6-10+ weeks (Surface RT only) |

## The open question for all three: M0 device probe

**[checked]** every Surface 2 (Win10 15035) log names the D3D adapter `"Software Adapter" VendorId=0000 ... 0 MiB`,
never NVIDIA. That build may have no Tegra GPU driver at all, so "hardware FL 9_1" there may already be
software rendering. Our host forces FL 9_1 by default, so neither tablet's true maximum feature level was ever logged.

One ARM32 probe exe, run once on Surface RT and Surface 2, answers it for all games:
1. adapters (name, vendor, software flag), true max FL for HARDWARE and WARP, d3d10warp.dll present, which driver DLL loads;
2. Bad Piggies: create all 113 level-9 replacements, the sprite draw + read-back, full vs short device vtable copy with swap chain/Present/ResizeBuffers, cbuffer-offset draw (sources: `bad_piggies_bench/`);
3. Geometry Dash: CreateVertexShader/CreatePixelShader on the 18 original 9_3 containers at FL 9_1;
4. Redcon: its 10 shaders + its 5 blend states (separate alpha) at FL 9_1;
5. fill-rate test HARDWARE vs WARP; 500-draw CPU test.

## Order

1. **Redcon** (no render issue): step 0 = profile record in the host, prove 0.10.4 unchanged (x86 identity, mock tests, patcher hash, old-vs-new log); step 1 = `PROFILE_REDCON` in the patcher + IIDs; then the `redcon` unit (SwapChainPanel factory, CompositionTarget tick, ApplicationView, CoreApplication, settings map, CurrentApp fake that answers safely, Launcher ...), PC mock test, Surface 2, Surface RT.
   Policy: in-app purchases cannot complete (Store closed); the fake reports nothing owned (free edition).
2. **M0 probe** in parallel (it decides Bad Piggies and Geometry Dash).
3. Then Geometry Dash or Bad Piggies, depending on the M0 results.
