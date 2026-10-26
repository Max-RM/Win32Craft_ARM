# Bad Piggies Rebooted - level_9_1 shader prototype

Goal: run the game's Unity 2018.4 shaders on the user's FL 9_1 GPUs (Surface RT, Surface 2).
A FL 9_1 device only accepts `vs_4_0_level_9_1` / `ps_4_0_level_9_1` bytecode. Unity ships only plain
`vs_4_0`/`ps_4_0` (plus a few `*_5_0`) programs, so each program the game uses needs a level-9 replacement.
Findings and evidence: `../bad_piggies_hw-shaders.md`.

## Results in one paragraph

The game has 82 Shader objects = 402 unique DXBC programs. It actually needs about 112 of them
(38 VS + 74 PS), plus 2 internal blit programs embedded in `UnityPlayer.dll`.
107 convert mechanically (register-level DXBC -> HLSL -> fxc level_9_1, proven equal to the original by an
SM4 interpreter), 4 more are hand-trimmed versions that keep the original bindings (`hlsl/dummy_lit_forwardbase_tier1.hlsl`),
1 (IMGUI rounded-rect, uses `ddx`) is left and is only needed if the game draws IMGUI rounded rects.
Nothing the game needs is impossible at 9_1.

## Folder

| Path | What |
|---|---|
| `hlsl/sprites_default.hlsl` | Sprites/Default, readable, keywords `PIXELSNAP_ON`, `ETC1_EXTERNAL_ALPHA` |
| `hlsl/unlit_transparent.hlsl` | Unlit/Transparent and (`-D COLOR_TINT`) _Custom/Unlit_ColorTransparent_Geometry - the VS is shared by 10 shaders |
| `hlsl/ui_default.hlsl` | UI/Default == TextMeshPro/Sprite #348, keywords `UNITY_UI_CLIP_RECT`, `UNITY_UI_ALPHACLIP` |
| `hlsl/dummy_lit_forwardbase_tier1.hlsl` | trimmed tier-1/2 FORWARDBASE PS of the "dummy" Standard-lit shaders (`-D LIGHTPROBE_SH`, `-D COLOR_TINT`, `-D FULL_BRDF`) |
| `auto/*.hlsl` | mechanical translation of the most complex program (TMP Mobile Distance Field OUTLINE_ON UNDERLAY_ON) |
| `auto/needed/` | generated HLSL + level-9 `.cso` for all 112 needed programs; `index.csv` = checksum, verdict, sizes, users |
| `auto/unityplayer/` | the 2 internal blit programs from UnityPlayer.dll (orig disassembly, HLSL, level-9 cso) |
| `auto/l9_replacements.bin` / `.txt` | lookup table for a CreateVertexShader/CreatePixelShader/CreateInputLayout hook (113 entries) |
| `orig/` | Unity's original DXBC + disassembly for the prototype programs, `unity_bindings.json` (Unity's cbuffer/texture metadata) |
| `build/` | output of `tools/verify_proto.py`: `*.l9.cso`, `*.l9.asm`, `report.txt` |
| `tools/` | the whole tool chain (pure Python + d3dcompiler_47 via ctypes) |

## How to reproduce

All x64 tools; nothing here runs ARM code. `d3dcompiler_47.dll` path is set in `tools/d3dc.py`
(`D:\Windows Kits\10\bin\10.0.19041.0\x64`).

```
python -I tools/verify_proto.py      # compiles hlsl/*.hlsl for SM4 and level_9_1, diffs against orig/, writes build/report.txt
python -I tools/make_table.py        # packs auto/needed/*.cso + build hand files + blit into auto/l9_replacements.bin
```

Full pipeline from the game files (writes `tools/out/`):

```
python -I tools/scan1.py   # parse Data/ + AssetBundles, decode every Shader object -> out/shaders.pkl
python -I tools/scan3.py   # LZ4-decompress blobs, extract + disassemble every DXBC -> out/dis/, out/analysis.json
python -I tools/refs.py    # materials / PPtr references -> out/refs.json
python -I tools/batch.py   # translate -> SM4 recompile + interpreter check -> level_9_1 compile -> out/batch.json
python -I tools/table.py   # per shader/pass/variant verdict table -> out/table.json
```

Equivalent fxc command lines for the hand files (use `-D` per keyword variant):

```
fxc -nologo -O3 -T vs_4_0_level_9_1 -E SpriteVert -Fo sprites_vs.cso hlsl\sprites_default.hlsl [-D PIXELSNAP_ON]
fxc -nologo -O3 -T ps_4_0_level_9_1 -E SpriteFrag -Fo sprites_ps.cso hlsl\sprites_default.hlsl [-D ETC1_EXTERNAL_ALPHA]
fxc -nologo -O3 -T vs_4_0_level_9_1 -E vert -Fo unlit_vs.cso hlsl\unlit_transparent.hlsl
fxc -nologo -O3 -T ps_4_0_level_9_1 -E frag -Fo unlit_ps.cso hlsl\unlit_transparent.hlsl [-D COLOR_TINT]
fxc -nologo -O3 -T vs_4_0_level_9_1 -E vert -Fo ui_vs.cso hlsl\ui_default.hlsl
fxc -nologo -O3 -T ps_4_0_level_9_1 -E frag -Fo ui_ps.cso hlsl\ui_default.hlsl [-D UNITY_UI_CLIP_RECT] [-D UNITY_UI_ALPHACLIP]
fxc -nologo -O3 -T ps_4_0_level_9_1 -E main -Fo lit_t1.cso hlsl\dummy_lit_forwardbase_tier1.hlsl [-D LIGHTPROBE_SH] [-D COLOR_TINT]
```
(In Git Bash set `MSYS_NO_PATHCONV=1` or use `-T` style switches, otherwise `/T` becomes a path.)

## What `verify_proto.py` checks (22 checks, all OK)

1. compiles for `vs/ps_4_0` and `vs/ps_4_0_level_9_1`;
2. input + output signature identical to Unity's (semantic, index, mask, register, system value, format);
3. `dcl_constantbuffer` slot/size and `t#`/`s#` declarations identical (trimmed shaders: a subset);
4. every member in Unity's metadata (`orig/unity_bindings.json`) is at the same byte offset in our RDEF;
5. the SM4 compile computes the same outputs as Unity's original for 24 random input/constant sets
   (`tools/sm4emu.py`, texture fetch = deterministic function of slot+coords; trimmed shaders are compared
   under the game's conditions: no realtime light, no light-probe-proxy volume, no probe box projection/blend).

## Why bindings must match

Unity strips RDEF from its DXBC and does not import d3dcompiler, so it never reflects bytecode. It fills
cbuffers by the byte offsets in its own metadata and binds them / textures / samplers to the slot numbers
recorded for each sub-program. A replacement is safe exactly when slots, offsets and signatures are unchanged.

## Suggested integration (see finding F8)

Hook `ID3D11Device::CreateInputLayout` (#11), `CreateVertexShader` (#12), `CreatePixelShader` (#15) in the
host after creating the FL 9_1 device. Key = bytes 4..19 of the incoming bytecode (the DXBC checksum).
Table format of `auto/l9_replacements.bin`: `'L9SW'`, u32 version 1, u32 count, then count x
{u8[16] checksum, u32 offset, u32 size} sorted by checksum, then the blobs. On a miss: log the checksum, return E_FAIL.

## Caveats / unverified

- Which graphics tier UnityPlayer picks on a spoofed device (tier-0 vs tier-1/2 sub-programs); the table covers both.
- The 4 trimmed lit programs are exact only without realtime lights / LPPV / box-projected or blended probes (the
  game has 0 Light objects; probes not checked).
- tex2Dlod on the reflection cubemap became tex2Dbias(6) in the 4 generated tier-0 lit programs and the 4 trimmed tier-1 ones (ps_2_0 has no tex2Dlod).
- No program has been run on a real FL 9_1 GPU yet; Tegra pixel-shader precision may be lower than fp32.
- The 6-byte prefix before Unity's DXBC (e.g. `01 00 04 00 00 00`) is not understood; irrelevant for the hook,
  relevant only if the asset files are rewritten instead.
