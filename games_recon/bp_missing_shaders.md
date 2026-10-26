STATUS: DONE
AREA: Bad Piggies Rebooted on the Surface 2 (Tegra 4, D3D11 FL 9_1) - the 4 shader programs Unity creates at run time that have no entry in l9_replacements.bin
(device log: VS MISS #1 = 1408 B, VS MISS #2 = 1508 B, PS MISS #1 = 212 B, PS MISS #2 = 408 B)

Method: pure read-only static analysis with x64 Python (games_recon/bad_piggies_shader_proto/tools/unityparse.py, sm4tohlsl.py, sm4emu.py, d3dc.py) and d3dcompiler_47. No ARM binary was run,
the game folder, the .appx/.xap and the shader table were not touched, nothing in the repo was modified except this file. One x64 test used a WARP device limited to FL 9_1 (see the verification list at the end of section 2).
The device-log folder (E:\BadPiggiesRebooted_x0xr9mv5edyrp\AC\l9shim.log, UnityPlayer.log) went offline while I worked; I read it first and appendix D reproduces every HIT/MISS line it contained.

## 1. Answer: all 4 positively identified

All four are `LightMode=SHADOWCASTER` passes (depth/shadow-map passes). The earlier recon dropped SHADOWCASTER from the "needed" set (table.py `needed()`) and classed
`Legacy Shaders/VertexLit` as `D:unused`; both assumptions are wrong for this run. Each of the four was already `verdict=mechanical` in auto/all_programs.csv.

| log line | size | DXBC checksum (bytes 4..19) | Unity shader [asset#pathID] | pass / keywords | level-9 result (built + tested this session) |
|---|---|---|---|---|---|
| VS MISS #1 | 1408 | `7eda7bd3a50b1aea45191dd3db67f4af` | Legacy Shaders/VertexLit [Resources/unity_builtin_extra#6] | SubShader0 pass2, LightMode SHADOWCASTER, kw `SHADOWS_DEPTH`, tiers 0,1,2 (blob 8) | vs_4_0_level_9_1, 37 slots, 13 const regs, 3644 B |
| PS MISS #1 | 212  | `2149265952758e8e2aa16f30616fa85e` | Legacy Shaders/VertexLit [unity_builtin_extra#6] | same pass, kw `SHADOWS_DEPTH` and `SHADOWS_CUBE` (blobs 10 and 11 are byte-identical) | ps_4_0_level_9_1, 2 slots, 0 const regs, 524 B |
| VS MISS #2 | 1508 | `e8e7fc89f0811cee3a03ce9369229eaa` | Particles/Standard Unlit [sharedassets0.assets#87] | SubShader0 pass1, SHADOWCASTER, kw `SHADOWS_CUBE _ALPHABLEND_ON`, tiers 0,1,2 (blob 1) | vs_4_0_level_9_1, 34 slots, 14 const regs, 3888 B |
| PS MISS #2 | 408  | `a8e80925af66290d2f3da26d61e3adc2` | Particles/Standard Unlit [sharedassets0.assets#87] | same pass, kw `SHADOWS_CUBE _ALPHABLEND_ON` and `SHADOWS_DEPTH _ALPHABLEND_ON` (blobs 9 and 11 identical) | ps_4_0_level_9_1, 6 slots (5 ALU + 1 tex), 0 const regs, 896 B |

Log order VS,PS,VS,PS = two (VS,PS) pairs: pair 1 = VertexLit shadow caster (1408 + 212), pair 2 = Particles/Standard Unlit shadow caster (1508 + 408).
What they compute: VS = Unity's standard shadow-caster vertex stage (world-space normal bias from unity_LightShadowBias.z, clip-space depth bias from .x/.y); PS 212 = `mov o0, 0`
(depth-only); PS 408 = alpha clip `if (tex(_MainTex,uv).a * vertexColor.a - 0.5 < 0) discard; o0 = 0`.

Why I trust the identification (evidence, strongest first):
1. Size = DXBC container size, proven: all 25 VS + 25 PS HIT lines of the same log resolve to a table entry (one, or for 5 PS lines two same-size siblings) whose original DXBC size AND replacement size both equal the logged `orig -> replacement` pair; none fails to resolve (appendix D).
2. Searching EVERY DXBC in the package (402 asset programs, 30 in UnityPlayer.dll, 30 cs_5_0, none in GameAssembly/exe) for the exact stage+size: VS 1408 -> 1 candidate, VS 1508 -> 1, PS 408 -> 1,
   PS 212 -> 2 (`e7ece845...` = Hidden/InternalErrorShader, already in the table and visible as HIT PS#8 `212 -> 512`; so the miss must be the other one, `2149...`).
   UnityPlayer.dll has no program of these sizes (nearest: VS 1504 at file offset 0x266c8a0, a GPU-skinning VS - not 1508).
3. Pairing: PS 212 takes only SV_POSITION (= VertexLit VS output, which is only SV_POSITION); PS 408 takes exactly TEXCOORD1.xy + TEXCOORD3.w (= Particles VS outputs TEXCOORD1.xy, TEXCOORD3.xyzw, SV_POSITION).
4. Reproduced on an x64 FL 9_1 WARP device: the original 1408 B VS and 212 B PS fail `CreateVertexShader/CreatePixelShader` with 0x80070057, the exact HRESULT in the device log;
   the level-9 replacements below create with S_OK.
Residual risk: sizes can in principle collide with a program I do not know about (e.g. generated at run time). The final proof is the checksum of the dumped bytes, see 5.4.

## 2. The four programs in detail

### 2.1 VS MISS #1 - Legacy Shaders/VertexLit, SHADOWCASTER, SHADOWS_DEPTH - 1408 B - 7eda7bd3a50b1aea45191dd3db67f4af
- Asset: Data/Resources/unity_builtin_extra, Shader object #6, platforms=[4] only. SubShader0 (RenderType Opaque, LOD 100), passes: p0 Vertex, p1 VertexLM, p2 SHADOWCASTER (tags SHADOWSUPPORT=true, instancing capable). No Fallback.
- Input signature: POSITION0 xyzw (r0), NORMAL0 xyz (r1), TEXCOORD0 xyzw (r2, declared but unused). Output: SV_POSITION.
- Unity binding metadata (same for tiers 0/1/2): UnityLighting b0 (768 B) uses `_WorldSpaceLightPos0`@0; UnityShadows b1 (416 B) uses `unity_LightShadowBias`@80; UnityPerDraw b2 (176 B) uses `unity_ObjectToWorld`@0, `unity_WorldToObject`@64; UnityPerFrame b3 (368 B) uses `unity_MatrixVP`@272. No textures.
- Level-9 constant map: c1<-cb0[0] x1, c2<-cb1[5] x1, c3..c9<-cb2[0..6] x7, c10..c13<-cb3[17..20] x4 (13 registers, limit 255), 37 vs_2_0 slots (limit 256).

### 2.2 PS MISS #1 - same pass - 212 B - 2149265952758e8e2aa16f30616fa85e
- Body is one instruction, `mov o0.xyzw, l(0,0,0,0)`. Input SV_POSITION (unused), output SV_Target0. No cbuffers, no textures. Level 9: 2 slots.
- Readable HLSL (this is exactly what the translator emits):
```
struct IN_T { float4 i0 : SV_POSITION; };  struct OUT_T { float4 o0 : SV_Target0; };
OUT_T main(IN_T IN) { OUT_T OUT; OUT.o0 = float4(0.0, 0.0, 0.0, 0.0); return OUT; }
```

### 2.3 VS MISS #2 - Particles/Standard Unlit, SHADOWCASTER, SHADOWS_CUBE + _ALPHABLEND_ON - 1508 B - e8e7fc89f0811cee3a03ce9369229eaa
- Asset: Data/sharedassets0.assets, Shader object #87, platforms=[4] only, Fallback `VertexLit`. SubShader0 passes: p0 type-2 pass with no programs (GrabPass), p1 SHADOWCASTER (SHADOWSUPPORT=true, instancing + procedural instancing), p2 FORWARDBASE.
- Input signature: POSITION0 xyzw, NORMAL0 xyz, COLOR0 xyzw, TEXCOORD0 xy. Output: TEXCOORD1.xy, TEXCOORD3.xyzw, SV_POSITION.
- Unity bindings: $Globals b0 (64 B) `_MainTex_ST`@32; UnityLighting b1 `_WorldSpaceLightPos0`@0; UnityShadows b2 `unity_LightShadowBias`@80; UnityPerDraw b3; UnityPerFrame b4 `unity_MatrixVP`@272.
- Level-9 constant map: c1<-cb0[2] x1, c2<-cb1[0] x1, c3<-cb2[5] x1, c4..c10<-cb3[0..6] x7, c11..c14<-cb4[17..20] x4 (14 registers), 34 slots.

### 2.4 PS MISS #2 - same pass - 408 B - a8e80925af66290d2f3da26d61e3adc2
- Input TEXCOORD1.xy, TEXCOORD3.w; t0/s0 = `_MainTex`; `sample; mad r0.x = a*v1.w-0.5; lt; discard_nz; mov o0, 0`. Level 9: 6 slots (1 texture, 5 ALU, `texkill`).
- Readable HLSL (translator output): `r0 = t0.Sample(s0, uv); r0.x = r0.w * v1.w + -0.5; r0.x = (float)(r0.x < 0.0); if (r0.x != 0) discard; o0 = 0;`

Verification done for all four (script in section 5, `OK ... sm4-equal(24/24) signatures-identical`):
- translated to register-level HLSL keeping cbuffer slots/offsets, t#/s# slots and the input/output signature; recompiled for SM4 and run through the repo's SM4 interpreter (tools/sm4emu.py)
  against Unity's original on 24 random input/constant sets: 0 mismatches for all four;
- compiled with `vs_4_0_level_9_1` / `ps_4_0_level_9_1`; the ISGN/OSGN chunks of the result are byte-identical to the original's (semantic, index, system value, type, register, mask), so the input layout Unity builds from the original VS signature stays valid;
- created on an x64 WARP device whose only allowed feature level is 9_1 (`D3D11CreateDevice(WARP, {9_1})` -> feature level 0x9100): all four `CreateVertexShader/CreatePixelShader` return 0, the wrong-stage control and the ORIGINAL SM4 blobs return 0x80070057.
  (The Tegra driver itself accepted 113/113 of the earlier level-9 blobs; these four have not been run on the device.)

## 3. Can a level-9 variant be lifted from the same Unity shader asset? No

- Both shader objects carry `platforms=[4]` (ShaderCompilerPlatform D3D11) only. In fact all 81 shader objects that have programs have only platform 4; the 1128 sub-programs are all `vs/ps_4_0` (or `_5_0`), no D3D9, GLES or D3D11_9x variant exists in any asset, bundle or in UnityPlayer.dll.
- Each shader has one SubShader, so there is no alternative cheaper SubShader inside the asset. Particles/Standard Unlit falls back to `VertexLit`, and VertexLit has no fallback and no level-9 programs either.
- Therefore the only route is the one the table already uses: convert the original SM4 bytecode. Because these four are tiny (<= 37 vs_2_0 slots, no loops, no integer ops, no derivatives, no sample_l), the mechanical pipeline
  (DXBC -> D3DDisassemble -> sm4tohlsl.py -> fxc level_9_1) is enough; no hand-trimmed HLSL is needed (contrast with the 4 tier-1 lit PS that needed hand work).
- The level-9 blobs are identical for all tiers (the original blobs are shared by tiers 0/1/2), so one table entry per checksum covers whatever graphics tier Unity picks.

## 4. Context: why these programs are requested at all (facts first, then clearly-marked hypotheses)

Facts (all from static data or the two logs):
- The four MISS lines are created during the first scene load, before any scene material is drawn: in l9shim.log they sit between the first `Hidden/Internal-GUITexture` pair (HIT #2) and the first `Hidden/Internal-GUITextureClip` pair (HIT #3);
  in UnityPlayer.log the four `D3D shader create error for vertex/pixel shader [0x80070057]` lines follow the `BackgroundBox` missing-script messages and precede the first `UnloadTime`.
- Real shadows cannot be what is being drawn: the only QualitySettings level ("Fast") has shadows=0, pixelLightCount=0, softParticles=false; the game has 0 Light objects; the IL2CPP metadata has no `depthTextureMode`/`LightShadows`/`WarmupAllShaders`/`ShaderVariantCollection` strings
  (only the renderer properties `shadowCastingMode`/`receiveShadows`). So Unity is creating these programs without a shadow draw.
- Only two shaders in the whole game define a SHADOWCASTER pass: Legacy Shaders/VertexLit and Particles/Standard Unlit - exactly the owners of the missing programs. VertexLit is reachable although the earlier recon marked it unused: it is the Fallback of
  Particles/Standard Unlit, of `e2d/Curve` (resources.assets#328, 3 Materials e2dCurve/_0/_1 in sharedassets1.assets) and of `Legacy Shaders/Diffuse` (unity_builtin_extra#7), which is itself the Fallback of `Depth Mask/Unlit Transparent (CG)` (3 Materials in resources.assets).
- Each owner has one Unity default material in sharedassets0.assets: `Default-ParticleSystem` (pathID 4; the only Material using Particles/Standard Unlit; its keyword set is exactly `_ALPHABLEND_ON`, the same keyword as the missed Particles variant)
  and `Default-Diffuse` (pathID 2; the only Material using Legacy Shaders/Diffuse, whose Fallback chain ends in VertexLit's shadow caster). A byte-pattern search of the ParticleSystemRenderer objects found none whose material array is Default-ParticleSystem,
  so these are unlikely to be what the game's particles use.
- `e2d/Curve` (shader whose shadow caster is VertexLit's) draws normally in the same log with its own programs (HIT #13 and #21: VS 840 -> 2520, PS 580 -> 1248). A failed fallback shadow-caster program therefore does not by itself stop the colour pass of a shader.
- Separate lead, unexplained: HIT #8 (VS 532 -> 1920, PS 212 -> 512) is `Hidden/InternalErrorShader`, Unity's magenta error shader, created right after the first Unlit/Transparent draws. Something in the scenes was drawn with the error shader in that run.
Hypotheses (NOT verified; ARM code was not disassembled):
- H1 (most likely): at material load Unity prepares the shadow-caster program of the material's default keyword variant (VertexLit: `SHADOWS_DEPTH`; Particles with `_ALPHABLEND_ON`: `SHADOWS_CUBE _ALPHABLEND_ON`) without drawing it. Then the four misses cost only four log
  errors and "invisible" applies only to a shadow/depth draw that never happens with shadows off. They are still worth fixing: cheap, mechanical, and they remove the four failure lines (and any risk that a depth/shadow pass is ever drawn).
- H2 (considered, evidence against): a failed program makes Unity declare the shader unusable and fall back to VertexLit and then the error shader. e2d/Curve working in the same log argues against it.
- The error-shader pair of HIT #8 is probably unrelated to the four misses (e.g. a Renderer with a missing/null material); to check, see below.
Check on the next device run: after installing the extended table the log must show four new HIT lines `(1408 -> 3644)`, `(212 -> 524)`, `(1508 -> 3888)`, `(408 -> 896)` and no MISS (table line: `shader table loaded: 117 replacements (303888 bytes)`);
if the `532 -> 1920` / `212 -> 512` error-shader pair is still there it is a separate problem and the next step is to find which Renderer has no valid material.

## 5. How to produce, merge and check the replacements

### 5.1 Exact steps (tested; the script below is the one I ran)
1. Save the script in section 5.5 as `build_missing_l9.py` OUTSIDE the repo tools folder.
2. Run (x64 Python 3, d3dcompiler path as set in tools/d3dc.py; `-B` keeps Python from writing __pycache__ into the repo):
```
python -B -I build_missing_l9.py ^
  --data  D:\repo\Win32Craft-main\BadPiggiesRebooted_1.0.0.0_ARM\Data ^
  --tools D:\repo\Win32Craft-main\games_recon\bad_piggies_shader_proto\tools ^
  --out   <new empty dir> ^
  --table D:\repo\Win32Craft-main\games_recon\bad_piggies_shader_proto\auto\l9_replacements.bin ^
  --merge-out <new empty dir>\l9_replacements.bin            (default preset = the 4 misses -> 117 entries, 303,888 bytes)
  [--preset shadowcasters]                                    (4 misses + 4 siblings, 121 entries, 318,668 bytes)
```
   Output per program: `<checksum>.l9.cso`, `.hlsl`, `.orig.txt` (SM4 disassembly), `.l9.asm`. Expected output (took 2 s):
```
7eda7bd3a50b1aea45191dd3db67f4af OK vs mode=keep orig=1408B l9=3644B slots=37 sm4-equal(24/24) signatures-identical
2149265952758e8e2aa16f30616fa85e OK ps mode=keep orig=212B  l9=524B  slots=2  sm4-equal(24/24) signatures-identical
e8e7fc89f0811cee3a03ce9369229eaa OK vs mode=keep orig=1508B l9=3888B slots=34 sm4-equal(24/24) signatures-identical
a8e80925af66290d2f3da26d61e3adc2 OK ps mode=keep orig=408B  l9=896B  slots=6  sm4-equal(24/24) signatures-identical
```
3. Deploy: the shim reads `Data\bp\l9_replacements.bin` beside the game (tools/badpiggies/make_badpiggies_package.py copies auto/l9_replacements.bin there; its `TABLE` constant would need to point at the new file, or overwrite auto/l9_replacements.bin
   and re-run make_badpiggies_package.py). For a quick device test just replace that one file in the installed folder (Desktop\BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft\Data\bp\). The table is sorted by checksum, the shim does a linear scan, any count works.
4. For the human-readable listing append these lines to auto/l9_replacements.txt (format of make_table.py):
```
7eda7bd3a50b1aea45191dd3db67f4af   3644 mechanical   Legacy Shaders/VertexLit [Resources/unity_builtin_extra#6] ss0.p2 SHADOWCASTER - vs b8 tiers012 kw=SHADOWS_DEPTH
2149265952758e8e2aa16f30616fa85e    524 mechanical   Legacy Shaders/VertexLit [Resources/unity_builtin_extra#6] ss0.p2 SHADOWCASTER - ps b10 tiers012 kw=SHADOWS_DEPTH (b11 SHADOWS_CUBE identical)
e8e7fc89f0811cee3a03ce9369229eaa   3888 mechanical   Particles/Standard Unlit [sharedassets0.assets#87] ss0.p1 SHADOWCASTER - vs b1 tiers012 kw=SHADOWS_CUBE _ALPHABLEND_ON
a8e80925af66290d2f3da26d61e3adc2    896 mechanical   Particles/Standard Unlit [sharedassets0.assets#87] ss0.p1 SHADOWCASTER - ps b9 tiers012 kw=SHADOWS_CUBE _ALPHABLEND_ON (b11 SHADOWS_DEPTH identical)
```
5. To make the repo pipeline reproduce this permanently (not done, repo is read-only for this task): in tools/table.py drop `'SHADOWCASTER'` from the exclusion list in `needed()` and make `usage()` return a used class for any shader named as `Fallback`
   of a used shader (VertexLit, Legacy Shaders/Diffuse), then re-run batch.py/table.py/make_table.py.

### 5.2 Insurance: the four siblings (same two shaders, other keyword combinations) - build with `--preset shadowcasters`
Keyword choice (DEPTH vs CUBE) differs between the two shaders in the log, so the other keyword of each is a likely next miss. All are mechanical, interpreter-equal, signature-identical and create on WARP 9_1:

| checksum | size | shader / keywords | l9 size |
|---|---|---|---|
| `699b506753c526a3c56b52f1f901d88c` | VS 1292 | Legacy Shaders/VertexLit SHADOWCASTER `SHADOWS_CUBE` | 3452 |
| `c350ad9b364ec53a4326f0ffab814a61` | VS 1624 | Particles/Standard Unlit SHADOWCASTER `SHADOWS_DEPTH _ALPHABLEND_ON` (the material keyword the game uses) | 4080 |
| `f7e32a0910040df455b1a7d4a63b1265` | VS 1320 | Particles/Standard Unlit SHADOWCASTER `SHADOWS_CUBE` (no alpha-blend) | 3480 |
| `ec874e4202b5a9e78480ffa113e9ec23` | VS 1436 | Particles/Standard Unlit SHADOWCASTER `SHADOWS_DEPTH` (no alpha-blend) | 3672 |

The PS for non-alpha-blend Particles (176 B, `dc56c05d...`) is already in the table. With these 8 the whole non-instanced SM4 SHADOWCASTER family of the game (VertexLit + Particles/Standard Unlit) is covered.
The instanced ones (`PROCEDURAL_INSTANCING_ON`, vs/ps_5_0, StructuredBuffer) cannot exist at 9_1 and are not requested because the caps correction turns instancing off.

### 5.3 Whole-game completeness (optional)
`--preset all-mechanical` tried all 291 programs not in the table (48 s): 173 build OK (59 VS + 114 PS; 580,632 bytes of cso; all 173 create on WARP 9_1; the table would grow from 294,840 to about 0.88 MB),
118 cannot be converted: 54 comparison-sampler PS (shadow-map sampling), 34 exceed ps_2_0/vs_2_0 limits (lit FORWARDADD/DEFERRED/PREPASS PS), 12 stereo/instancing VS (4 SV_RenderTargetArrayIndex, 4 `ishl`, 4 SV_InstanceID/VertexID), 6 structured-buffer (procedural instancing),
3 loop VS (the `Vertex` pass of `Legacy Shaders/VertexLit`, blobs 0-2 = kw none/POINT/SPOT), 3 integer converts (2 ftoi, 1 itof), 2 texture2darray, and one each of VPOS, MSAA texture, ddx/ddy, immediate constant buffer. None of these 118 has been requested in the logged run.
Per-program verdict is in appendix A column 7 (`buildable` / `no:<reason>` / `BUILT-OK(tested)`). Recommended: only the 4 + 4 above; the rest is optional.

### 5.4 Matching the authentic dump (the next step you described)
The checksum is stored inside the dumped DXBC (bytes 4..19), so matching is a read, not a hash. For a dumped `l9_miss_vs_1.dxbc` (the current shim source, l9shim.c `dump_blob`, writes `l9_miss_vs_N.dxbc` / `l9_miss_ps_N.dxbc` next to the log; the log in question has no `dumped` suffix on its MISS lines and AC/ held no .dxbc, so that build predates the dump):
```
python -B -I -c "import sys;d=open(sys.argv[1],'rb').read();print(len(d),d[4:20].hex(),d[:4])" l9_miss_vs_1.dxbc
expected:  1408 7eda7bd3a50b1aea45191dd3db67f4af b'DXBC'      (ps_1: 212 2149265952758e8e2aa16f30616fa85e; vs_2: 1508 e8e7fc89f0811cee3a03ce9369229eaa; ps_2: 408 a8e80925af66290d2f3da26d61e3adc2)
```
Any other checksum means the size match was a coincidence: look the checksum up in appendix A/B (or tell me the checksum). To compare the whole blob byte for byte: extract it with section 5.5's loader (`progs[ck][0]`) and `cmp`.

### 5.5 The script (tested; it reuses the repo tools by import and writes only under --out)
```python
"""build_missing_l9.py - build level_9_1 replacements for specific Bad Piggies DXBC programs (by original DXBC checksum) straight from the game's Data/,
using the existing tools in games_recon/bad_piggies_shader_proto/tools (sm4tohlsl translator + sm4emu interpreter + d3dcompiler_47).
Read-only on the game; x64 python; nothing ARM is executed.  Writes ONLY under --out.

  python -B -I build_missing_l9.py --data  <...>\\BadPiggiesRebooted_1.0.0.0_ARM\\Data ^
                                   --tools <...>\\games_recon\\bad_piggies_shader_proto\\tools ^
                                   --out   <empty dir> ^
                                   [--preset misses|shadowcasters|all-mechanical] [--ck <32 hex> ...] ^
                                   [--table <...>\\auto\\l9_replacements.bin --merge-out <new .bin>]
Per program it: translates the SM4 text to register-level HLSL, recompiles it for SM4 and proves it equal to Unity's original on 24 random input sets
(sm4emu), compiles it for vs/ps_4_0_level_9_1, checks that the input/output signatures are byte-identical to the original, and writes
<ck>.l9.cso / <ck>.hlsl / <ck>.orig.txt / <ck>.l9.asm.  --merge-out writes a new 'L9SW' table = --table + the new entries (the old table is untouched).
"""
import sys, os, re, struct, argparse, collections
sys.dont_write_bytecode = True
ap = argparse.ArgumentParser()
ap.add_argument('--data', required=True); ap.add_argument('--tools', required=True); ap.add_argument('--out', required=True)
ap.add_argument('--preset', choices=['misses', 'shadowcasters', 'all-mechanical'], default='misses')
ap.add_argument('--ck', nargs='*', default=[]); ap.add_argument('--table'); ap.add_argument('--merge-out')
a = ap.parse_args()
sys.path.insert(0, a.tools)
from unityparse import read_serialized, parse_shader, shader_platform_blobs
from sm4tohlsl import translate, Unsupported
from d3dc import compile, disassemble
from sm4emu import equivalent

MISSES = ['7eda7bd3a50b1aea45191dd3db67f4af',   # VS 1408 Legacy Shaders/VertexLit  SHADOWCASTER SHADOWS_DEPTH
          '2149265952758e8e2aa16f30616fa85e',   # PS  212 Legacy Shaders/VertexLit  SHADOWCASTER (SHADOWS_DEPTH and SHADOWS_CUBE)
          'e8e7fc89f0811cee3a03ce9369229eaa',   # VS 1508 Particles/Standard Unlit  SHADOWCASTER SHADOWS_CUBE _ALPHABLEND_ON
          'a8e80925af66290d2f3da26d61e3adc2']   # PS  408 Particles/Standard Unlit  SHADOWCASTER (+_ALPHABLEND_ON, DEPTH and CUBE)
SIBLINGS = ['699b506753c526a3c56b52f1f901d88c',  # VS 1292 VertexLit SHADOWS_CUBE
            'c350ad9b364ec53a4326f0ffab814a61',  # VS 1624 Particles SHADOWS_DEPTH _ALPHABLEND_ON
            'f7e32a0910040df455b1a7d4a63b1265',  # VS 1320 Particles SHADOWS_CUBE
            'ec874e4202b5a9e78480ffa113e9ec23']  # VS 1436 Particles SHADOWS_DEPTH

def sigs(dx):
    nch = struct.unpack_from('<I', dx, 28)[0]; out = {}
    for k in range(nch):
        off = struct.unpack_from('<I', dx, 32 + 4 * k)[0]; tag = dx[off:off + 4].decode('latin1')
        if tag in ('ISGN', 'OSGN'):
            n = struct.unpack_from('<I', dx, off + 8)[0]; els = []
            for e in range(n):
                nameoff, sidx, sysv, ctype, reg, mask = struct.unpack_from('<IIIIII', dx, off + 16 + e * 24)
                els.append((dx[off + 8 + nameoff: dx.index(b'\0', off + 8 + nameoff)], sidx, sysv, ctype, reg, mask))
            out[tag] = els
    return out

# ---- collect every D3D11 program of every Shader object: checksum -> (dxbc, [owner strings])
files = []
for f in sorted(os.listdir(a.data)):
    p = os.path.join(a.data, f)
    if os.path.isfile(p) and not f.endswith(('.resS', '.resource', '.config')):
        files.append((f, open(p, 'rb').read()))
rd = os.path.join(a.data, 'Resources')
for f in sorted(os.listdir(rd)):
    files.append(('Resources/' + f, open(os.path.join(rd, f), 'rb').read()))
progs = {}
for fn, d in files:
    sf = read_serialized(d)
    for (pid, start, size, cid) in sf['objects']:
        if cid != 48:
            continue
        sh = parse_shader(d, start, size)
        for plat, subs, dec in shader_platform_blobs(sh):
            for bi, sp in enumerate(subs):
                c = sp['code']; i = c.find(b'DXBC'); dx = c[i:i + struct.unpack_from('<I', c, i + 24)[0]]
                progs.setdefault(dx[4:20].hex(), (dx, []))[1].append('%s [%s#%d] blob%d kw=%s' % (sh['parsed']['name'], fn, pid, bi, ' '.join(sp['keywords']) or '-'))

# ---- existing table
covered = {}
if a.table:
    t = open(a.table, 'rb').read(); _, _, cnt = struct.unpack_from('<4sII', t, 0)
    for i in range(cnt):
        ck = t[12 + i * 24:28 + i * 24]; off, sz = struct.unpack_from('<II', t, 28 + i * 24); covered[ck.hex()] = t[off:off + sz]

want = list(a.ck)
if a.preset == 'misses' and not want: want = MISSES
elif a.preset == 'shadowcasters': want = MISSES + SIBLINGS + want
elif a.preset == 'all-mechanical': want = [ck for ck in progs if ck not in covered] + want
os.makedirs(a.out, exist_ok=True)
ok = {}; res = []
for ck in want:
    if ck not in progs:
        res.append((ck, 'NOT FOUND in Data/')); continue
    dx, owners = progs[ck]
    dis = disassemble(dx)
    stage = 'vs' if re.search(r'^\s*vs_\d_\d', dis, re.M) else 'ps'
    open(os.path.join(a.out, ck + '.orig.txt'), 'w').write(dis)
    r = None
    for mode in ('keep', 'bias'):
        try:
            src, tr = translate(dis, stage, lod_mode=mode)
        except Unsupported as e:
            r = 'UNSUPPORTED: %s' % e; break
        except Exception as e:
            r = 'ERROR: %r' % e; break
        bc4, m4 = compile(src, stage + '_4_0')
        if not bc4:
            r = 'SM4 recompile failed: ' + m4.strip()[:160]; break
        try:
            nb, worst = equivalent(dis, disassemble(bc4), stage)
        except Exception as e:
            r = 'emulator error %r' % e; break
        if nb:
            r = 'NOT EQUAL to original in %d/24 cases' % nb; break
        bc9, m9 = compile(src, stage + '_4_0_level_9_1')
        if not bc9:
            r = 'level_9_1 FAIL: ' + m9.strip()[:160]
            if 'SampleLevel' in src and mode == 'keep': continue
            break
        s0, s9 = sigs(dx), sigs(bc9)
        if s0 != s9:
            r = 'SIGNATURE DIFFERS from original'; break
        d9 = disassemble(bc9)
        open(os.path.join(a.out, ck + '.hlsl'), 'w').write(src)
        open(os.path.join(a.out, ck + '.l9.cso'), 'wb').write(bc9)
        open(os.path.join(a.out, ck + '.l9.asm'), 'w').write(d9)
        m = re.search(r'approximately (\d+) instruction slots used', d9)
        ok[ck] = bc9
        r = 'OK %s mode=%s orig=%dB l9=%dB slots=%s sm4-equal(24/24) signatures-identical' % (stage, mode, len(dx), len(bc9), m.group(1) if m else '?')
        break
    res.append((ck, r))
cnt = collections.Counter(r.split()[0] for _, r in res)
for ck, r in res:
    print(ck, r, '|', progs[ck][1][0] if ck in progs else '')
print('summary', dict(cnt))

if a.merge_out:
    allent = dict(covered); allent.update(ok)
    ents = sorted(allent.items())
    hdr = struct.pack('<4sII', b'L9SW', 1, len(ents)); off = len(hdr) + 24 * len(ents); tab = b''; data = b''
    for bytes_ck, blob in ents:
        tab += bytes.fromhex(bytes_ck) + struct.pack('<II', off + len(data), len(blob)); data += blob
    open(a.merge_out, 'wb').write(hdr + tab + data)
    print('wrote', a.merge_out, len(ents), 'entries (%d old + %d new)' % (len(covered), len(ok)))
```

## 6. How the index was built (so it can be re-done)
- Containers parsed with tools/unityparse.py: 83 serialized-file candidates = Data/globalgamemanagers, globalgamemanagers.assets, level0..level32, resources.assets, sharedassets0..32.assets, Resources/unity default resources, Resources/unity_builtin_extra, and the nodes of the 12 UnityFS bundles in StreamingAssets/AssetBundles (11 content bundles + the StandaloneWindows manifest bundle);
  82 Shader objects (class 48) in 7 files: globalgamemanagers.assets 2, resources.assets 28, sharedassets0 9, sharedassets1 1, sharedassets29 2, unity default resources 5, unity_builtin_extra 35; the level*, other sharedassets and all bundles contain 0 Shader objects;
  every Shader decoded with `consumed == byteSize`. `AB/StandaloneWindows` (the 4,872-byte AssetBundleManifest bundle) is not a serialized file; the raw scan found nothing in it.
- 81 shaders carry programs (`UI/Default Font` has none): 1128 sub-programs (503 VS SM4 + 10 VS SM5 + 603 PS SM4 + 12 PS SM5), all platform 4. DXBC = first `DXBC` in each m_ProgramCode (after a 6-byte prefix that varies per program, e.g. 01 00 04 00 00 00), length = DXBC header dword at +24,
  checksum = header bytes 4..19. 1128 sub-programs collapse to 402 unique checksums (116 VS + 286 PS) which equals the earlier recon's 402 unique SHA-1s one-to-one. No sub-program lacks DXBC.
- Raw magic scan of all 157 files of the package (270 MB, including the .resS/.resource files and all DLLs): DXBC containers only in UnityPlayer.dll (30, section B) and Data/Resources/unity default resources (30 = 15 unique cs_5_0, section C).
- Cross-reference: l9_replacements.bin has 113 entries; all 113 resolve - 111 to asset programs, 2 to the UnityPlayer.dll blit pair (`6e4f5026...` VS 568 B, `5daf5b73...` PS 600 B); 291 asset programs and 28 UnityPlayer.dll programs are not in it.
- Owner pass/keywords/tiers come from the parsed ShaderLab form (SerializedProgram sub-programs -> pass, LightMode tag, tier, keywords stored in the blob). "Use class" and "verdict" columns are joined from auto/all_programs.csv by SHA-1.

## 7. Short list of things worth doing next (in order)
1. Build and ship the 4 (or 8) programs with section 5.1 and read the next log (checks in section 4).
2. Look at whether particles (Particles/Standard Unlit) and e2d/Curve render, and whether anything is magenta; relate to the `212 -> 512` InternalErrorShader pair in the log.
3. Optionally fix `needed()`/`usage()` in tools/table.py so the generated table includes the shadow-caster family and Fallback targets (section 5.1 step 5), and decide whether to ship the 173-program superset.

## APPENDICES - the checksum + size index
Lookup tip: all lines are `|` separated and sorted by stage, size, checksum, so `grep "|1408|"` or `grep 7eda7bd3` finds a program; section D resolves every logged size to checksums.

### A. All 402 unique D3D11 DXBC programs in the game assets (116 VS + 286 PS), sorted by stage, size, checksum

Columns (pipe separated): `checksum` (bytes 4..19 of the DXBC, hex) | stage | DXBC size in bytes (= what Unity passes to Create*Shader) | shader model | table status | earlier-recon verdict (auto/all_programs.csv) | can this session build it? | use class of owners (A=material, B=script, C=engine, D=unused) | owners as `Shader[file#pathID] s<subshader>p<pass> <LightMode> kw=<keywords> t<tiers>`.
Source files: R/ = Data/Resources/. `IN-TABLE` = present in games_recon/bad_piggies_shader_proto/auto/l9_replacements.bin (113 entries; 111 of them are asset programs listed here, the other 2 are the UnityPlayer.dll blits of section B).

```
dc56c05df639b144a3054a65f0fd89a5|ps|176|ps_4_0|IN-TABLE(l9=488B)|mechanical||AC|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=SHADOWS_DEPTH;SHADOWS_CUBE t012 + Hidden/Internal-StencilWrite[R/unity_builtin_extra#19] s0p0 - kw=- t012
4c8790bb683a2bcbbc8e844c0193ffcf|ps|180|ps_5_0|uncovered|mechanical|buildable|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=PROCEDURAL_INSTANCING_ON SHADOWS_DEPTH;PROCEDURAL_INSTANCING_ON SHADOWS_CUBE t012
2149265952758e8e2aa16f30616fa85e|ps|212|ps_4_0|**MISS#1 PS**|mechanical|BUILT-OK(tested)|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p2 SHADOWCASTER kw=SHADOWS_DEPTH;SHADOWS_CUBE t012
e7ece84581386f0c00a49f263b529d7c|ps|212|ps_4_0|IN-TABLE(l9=512B)|mechanical||C|Hidden/InternalErrorShader[R/unity%20default%20resources#17] s0p0 - kw=-;UNITY_SINGLE_PASS_STEREO;STEREO_MULTIVIEW_ON t012
7647a3fc9745a3c31eaaeae1f8365f12|ps|220|ps_4_0|IN-TABLE(l9=708B)|mechanical||A|_Custom/Unlit_Monochrome[resources.assets#340] s0p0 - kw=- t012
4edd109775107ca535a082e40c949ac9|ps|244|ps_4_0|IN-TABLE(l9=532B)|mechanical||AC|_Custom/Utility_ZWrite[resources.assets#338] s0p0 - kw=- t012 + Hidden/Internal-Colored[R/unity%20default%20resources#69] s0p0 - kw=-;UNITY_SINGLE_PASS_STEREO;STEREO_MULTIVIEW_ON t012
593e80893489793fdf8191f9126555f5|ps|244|ps_4_0|IN-TABLE(l9=532B)|mechanical||C|Hidden/InternalClear[R/unity%20default%20resources#68] s0p0 - kw=-;UNITY_SINGLE_PASS_STEREO;STEREO_MULTIVIEW_ON t012 + Hidden/InternalClear[R/unity%20default%20resources#68] s0p1 - kw=-;UNITY_SINGLE_PASS_STEREO;STEREO_MULTIVIEW_ON t012 + (6 more owners: Hidden/InternalClear)
baba3d308b350296dd1eaf6540e3cf46|ps|248|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s0p0 - kw=- t012 + Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s2p0 - kw=- t012 + Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s4p0 - kw=- t012
6d5f8774b90c476c5a68f05c2f37e7fd|ps|264|ps_4_0|uncovered|impossible|no:integer signature element SV_RenderTarge|C|Hidden/InternalErrorShader[R/unity%20default%20resources#17] s0p0 - kw=STEREO_INSTANCING_ON t012
16179b0c5bc6cbb54cb45d28ce15ca0b|ps|292|ps_4_0|uncovered|mechanical|buildable|D|Hidden/ConvertTexture[R/unity_builtin_extra#68] s0p0 - kw=- t012
49cb703056b6baf7228c1691966a0730|ps|292|ps_4_0|IN-TABLE(l9=684B)|mechanical||AD|Depth Mask/Unlit Transparent ZAlways[resources.assets#325] s0p0 - kw=- t012 + Unlit/Transparent[sharedassets0.assets#88] s0p0 - kw=- t012 + Hidden/BlitCopyDepth[R/unity_builtin_extra#67] s0p0 - kw=- t012
961bd10b8bbe4a40a5a1089fad32842c|ps|292|ps_4_0|uncovered|impossible|no:integer signature element SV_RenderTarge|C|Hidden/Internal-Colored[R/unity%20default%20resources#69] s0p0 - kw=STEREO_INSTANCING_ON t012
afad148af06066fcda2851fde09190a6|ps|292|ps_4_0|uncovered|impossible|no:integer signature element SV_RenderTarge|C|Hidden/InternalClear[R/unity%20default%20resources#68] s0p0 - kw=STEREO_INSTANCING_ON t012 + Hidden/InternalClear[R/unity%20default%20resources#68] s0p1 - kw=STEREO_INSTANCING_ON t012 + (6 more owners: Hidden/InternalClear)
45790cb6dd25a6cfe7bca9bcf15e088e|ps|320|ps_4_0|uncovered|mech-approx|buildable|D|Hidden/CubeBlur[R/unity_builtin_extra#15104] s1p0 - kw=- t012 + Hidden/CubeCopy[R/unity_builtin_extra#15105] s0p0 - kw=- t012 + Hidden/CubeCopy[R/unity_builtin_extra#15105] s1p0 - kw=- t012
07d385d22869647126011e966a484fd4|ps|332|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p2 PREPASSBASE kw=- t012
a2457187a5f61cfd05753c4d98725964|ps|340|ps_4_0|IN-TABLE(l9=768B)|mechanical||A|Unlit/Texture[resources.assets#323] s0p0 - kw=- t012
a52e986e36e5a8d9529f5c4a0f5c9b45|ps|340|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredReflections[R/unity_builtin_extra#74] s0p1 - kw=UNITY_HDR_ON t012
1c2735a219488e457e1d759d3937b480|ps|344|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredReflections[R/unity_builtin_extra#74] s0p1 - kw=- t012
4f7e6b97f3c1fe24494c38af43c162c8|ps|344|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p1 - kw=- t012
f0bde3d56fe0f0cceffcb66b525c3952|ps|348|ps_4_0|IN-TABLE(l9=968B)|mechanical||ABC|e2d/Fill[resources.assets#341] s0p0 - kw=- t012 + Depth Mask/Unlit Transparent (CG)[resources.assets#344] s0p0 - kw=- t012 + (5 more owners: e2d/Fill, Depth Mask/Unlit Transparent (CG), _Custom/PreAlpha_Unlit_ColorTransparent_GeometryZ, _Custom/Unlit_ColorTransparent_Geometry, _Custom/PreAlpha_Unlit_ColorTransparent_Geometry, _Custom/Unlit_Color_Geometry, Hidden/BlitCopy)
e5dfe82abb6e568a2bcb8b3dbd8defd1|ps|360|ps_4_0|uncovered|impossible|no:resource texture2darray|D|Hidden/VR/BlitTexArraySlice[R/unity_builtin_extra#15304] s0p0 - kw=- t012
4f9326cbcb888a96931ac8ecf9ff698c|ps|368|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-Flare[R/unity_builtin_extra#102] s0p0 - kw=- t012
802266c90d74f987558ce4233950d2e5|ps|368|ps_4_0|IN-TABLE(l9=788B)|mechanical||A|Spine/Skeleton[sharedassets0.assets#90] s0p0 - kw=- t012
c7f16e0aa60e5a54b52eb61befda0048|ps|368|ps_4_0|uncovered|mechanical|buildable|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p3 - kw=- t012 + Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p4 - kw=- t012
12b2f29cba02eac82e5adf13f9284c38|ps|372|ps_4_0|IN-TABLE(l9=1004B)|mechanical||A|_Custom/Unlit_Alpha8Bit_Color[resources.assets#337] s0p0 - kw=- t012
52bcaff9b603e5b9d5da428e96295d22|ps|372|ps_4_0|IN-TABLE(l9=1004B)|mechanical||B|_Custom/SilhouetteShader[sharedassets29.assets#14] s0p0 - kw=- t012
5e4ad9e3634b4dc9bcd9e1277856658a|ps|388|ps_4_0|IN-TABLE(l9=820B)|mechanical||A|_Custom/Text Shader With Z Test[resources.assets#339] s0p0 - kw=- t012
adbaeb3fd7af8c9e771807c6c6fdebd4|ps|388|ps_4_0|IN-TABLE(l9=1048B)|mechanical||A|_Custom/PreAlpha_Unlit_ColorTransparent_Geometry_Gray[sharedassets0.assets#89] s0p0 - kw=- t012
f0403b994737c3a6b66059414bf7d8dc|ps|388|ps_4_0|IN-TABLE(l9=820B)|mechanical||A|GUI/Text Shader[R/unity%20default%20resources#10101] s0p0 - kw=-;UNITY_SINGLE_PASS_STEREO;STEREO_MULTIVIEW_ON t012
fe7c437e0d6b0be7ce1a28320e4befdc|ps|388|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-Halo[R/unity_builtin_extra#105] s0p0 - kw=- t012
05c5f3d1a3006f5775954658039b103c|ps|396|ps_4_0|IN-TABLE(l9=832B)|mechanical||C|Hidden/Internal-GUITexture[R/unity_builtin_extra#9002] s0p0 - kw=- t012 + Hidden/Internal-GUITexture[R/unity_builtin_extra#9002] s1p0 - kw=- t012
d2c5a23729797cfa0fa318a4e3c8563c|ps|396|ps_4_0|IN-TABLE(l9=928B)|mechanical||A|Legacy Shaders/Particles/Alpha Blended Premultiply[sharedassets0.assets#86] s0p0 - kw=- t012
a8e80925af66290d2f3da26d61e3adc2|ps|408|ps_4_0|**MISS#2 PS**|mechanical|BUILT-OK(tested)|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=SHADOWS_CUBE _ALPHABLEND_ON;SHADOWS_DEPTH _ALPHABLEND_ON t012
45e36549b055e6f8061707970cea61ec|ps|416|ps_4_0|IN-TABLE(l9=852B)|mechanical||A|Sprites/Default[globalgamemanagers.assets#3] s0p0 - kw=-;PIXELSNAP_ON t012
52f2aefe9bf708aaa84b2912741a0d14|ps|416|ps_4_0|IN-TABLE(l9=1124B)|mechanical||A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=_ALPHABLEND_ON t012
e6f10a6781dc5c98d840e103554685e5|ps|416|ps_4_0|IN-TABLE(l9=1100B)|mechanical||C|Sprites/Mask[R/unity_builtin_extra#10757] s0p0 - kw=-;PIXELSNAP_ON t012
03161913fee29188449021ce2a5e17db|ps|420|ps_5_0|uncovered|mechanical|buildable|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=PROCEDURAL_INSTANCING_ON SHADOWS_CUBE _ALPHABLEND_ON;PROCEDURAL_INSTANCING_ON SHADOWS_DEPTH _ALPHABLEND_ON t0
93bae1fa7e72780c134b0f0e8b58a878|ps|420|ps_5_0|uncovered|mechanical|buildable|D|Hidden/BlitToDepth[R/unity_builtin_extra#109] s0p0 - kw=- t012
189298ed784f67aae7d7912c3d3ef0ad|ps|424|ps_4_0|IN-TABLE(l9=876B)|mechanical||A|Depth Mask/MaskOverlay[resources.assets#330] s0p0 Vertex kw=- t012
23e759a7612ed98dfb72452998007a85|ps|424|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s6p0 - kw=- t012
411e6568bc8ab6c763440b14f7f4fb53|ps|428|ps_5_0|uncovered|mechanical|buildable|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=PROCEDURAL_INSTANCING_ON _ALPHABLEND_ON t012
294f96de39ff91ae168c709db3136136|ps|436|ps_4_0|IN-TABLE(l9=1180B)|mechanical||A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=- t012
7b6dc937a039c2b47cbddb47ef4a4d39|ps|436|ps_4_0|IN-TABLE(l9=904B)|mechanical||A|Skybox/Procedural[sharedassets29.assets#13] s0p0 - kw=_SUNDISK_NONE t012
8cb14f826b6b52a44bf747c217222dbd|ps|440|ps_4_0|IN-TABLE(l9=1148B)|mechanical||A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=SOFTPARTICLES_ON _ALPHABLEND_ON t012
a2096569ae5ddc30b43a012105d21e7d|ps|440|ps_4_0|IN-TABLE(l9=1088B)|mechanical||AC|TextMeshPro/Sprite[resources.assets#348] s0p0 - kw=- t012 + UI/Default[R/unity_builtin_extra#10770] s0p0 - kw=- t012
ffa55ef2b49df1de4681737825ade744|ps|440|ps_4_0|uncovered|impossible|no:integer signature element SV_RenderTarge|A|GUI/Text Shader[R/unity%20default%20resources#10101] s0p0 - kw=STEREO_INSTANCING_ON t012
f82215ccca9bc32b68dcc9e327a2e3bb|ps|444|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-CubemapToEquirect[R/unity_builtin_extra#15309] s0p0 - kw=- t012
77a4a2f257424a66bbdbecf135e72815|ps|448|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s5p0 - kw=- t012 + Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s5p1 - kw=- t012
f4b955948b308486ddc4df817c00c090|ps|448|ps_5_0|uncovered|mechanical|buildable|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=PROCEDURAL_INSTANCING_ON t012
f8b2e1cf120ac8cb8f65cdfc7821cda6|ps|448|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s3p0 - kw=- t012
9a656fa1d2c5fe314500bae9ee5d433b|ps|452|ps_5_0|uncovered|mechanical|buildable|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=PROCEDURAL_INSTANCING_ON SOFTPARTICLES_ON _ALPHABLEND_ON t012
5fef5724fb393f96ee90dfe410cfbca2|ps|460|ps_4_0|IN-TABLE(l9=1204B)|mechanical||A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=SOFTPARTICLES_ON t012
eb8c504ebfd500faac20329e23c79345|ps|460|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s1p0 - kw=- t012
7a3ef9449c82dc621028bf3b03c5092f|ps|468|ps_4_0|uncovered|mechanical|buildable|D|Hidden/BlitCopyWithDepth[R/unity_builtin_extra#107] s0p0 - kw=- t012
46aa409ebb5ff39b1786731108b8014d|ps|472|ps_5_0|uncovered|mechanical|buildable|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=PROCEDURAL_INSTANCING_ON SOFTPARTICLES_ON t012
6d8d6ff373a04e1ba72af3c8fd5432e0|ps|472|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s0p0 - kw=- t012 + Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s4p0 - kw=- t012
92dafc26e43df26366dfaea086a2af2d|ps|472|ps_4_0|uncovered|mechanical|buildable|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p0 Vertex kw=-;POINT;SPOT t012
b5084677c69b366a087b348e49557716|ps|476|ps_4_0|uncovered|mechanical|buildable|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p5 - kw=- t012
052928bb676cbb9e66a5677c50fdd84a|ps|492|ps_4_0|IN-TABLE(l9=944B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=- t012
c02ed7bc8f829651fd45f74e3296e700|ps|496|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s2p0 - kw=- t012
f85457265536e831239965f874caa992|ps|496|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s7p0 - kw=- t012 + Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s8p0 - kw=- t012
04eb1e04c67e8b2ffdbec7ae7a664a19|ps|516|ps_4_0|IN-TABLE(l9=1076B)|mechanical||C|Hidden/Internal-GUITextureClipText[R/unity_builtin_extra#9001] s0p0 - kw=- t012 + Hidden/Internal-GUITextureClipText[R/unity_builtin_extra#9001] s1p0 - kw=- t012
a5076f2155ba4218a4b8b47da7e14c34|ps|528|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Compositing[R/unity_builtin_extra#17000] s0p0 - kw=- t012
4d44f84456b4ecd2950c41fdc3b01162|ps|536|ps_4_0|IN-TABLE(l9=1236B)|mechanical||AC|TextMeshPro/Sprite[resources.assets#348] s0p0 - kw=UNITY_UI_ALPHACLIP t012 + UI/Default[R/unity_builtin_extra#10770] s0p0 - kw=UNITY_UI_ALPHACLIP t012
279ea87898aecd477808c52cb5b9e7b6|ps|548|ps_4_0|IN-TABLE(l9=1212B)|mechanical||B|Hidden/SnapshotEffect[resources.assets#346] s0p0 - kw=- t012
867a9fa9b4d6a95ed03b6c086606113a|ps|548|ps_4_0|IN-TABLE(l9=1364B)|mechanical||A|_Custom/PreAlpha_Unlit_ColorTransparent_Geometry_Shiny[resources.assets#332] s0p0 - kw=- t012
47742c79f0286b3c42d68725520b56ad|ps|568|ps_4_0|IN-TABLE(l9=1504B)|mechanical||C|Sprites/Mask[R/unity_builtin_extra#10757] s0p0 - kw=ETC1_EXTERNAL_ALPHA;ETC1_EXTERNAL_ALPHA PIXELSNAP_ON t012
c4fbe6c3013f034640869cffc4391ea0|ps|568|ps_4_0|IN-TABLE(l9=1288B)|mechanical||A|Sprites/Default[globalgamemanagers.assets#3] s0p0 - kw=ETC1_EXTERNAL_ALPHA;ETC1_EXTERNAL_ALPHA PIXELSNAP_ON t012
6b48406f66b16f990b643a03959547e8|ps|572|ps_4_0|IN-TABLE(l9=1104B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=OUTLINE_ON t012
7abbd1bf412e11d2cfd2ebc5c5fd51a9|ps|580|ps_4_0|IN-TABLE(l9=1248B)|mechanical||A|e2d/Curve[resources.assets#328] s0p0 - kw=- t012
9fdd9c2ffe582a1bf4cdac5829660a41|ps|580|ps_4_0|uncovered|mechanical|buildable|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p1 VertexLM kw=- t012
1b74d2a488eaa71c016e608bf12e3f14|ps|588|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=DIRECTIONAL t0
4818f8c1bb318b9e5e27e5227e6aea85|ps|588|ps_4_0|IN-TABLE(l9=1092B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=UNITY_UI_ALPHACLIP t012
aa0e4f9a4f595eec5912a1cfb73074a4|ps|588|ps_4_0|IN-TABLE(l9=1464B)|mechanical||A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL t0
e66195d4619254f0e6bbc1c743892d88|ps|620|ps_4_0|uncovered|mechanical|buildable|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p8 - kw=- t012
cedc6bd6e5f451dccf2039fe0d06c5bc|ps|632|ps_5_0|uncovered|impossible|no:SV_Position read in PS (VPOS)|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=PROCEDURAL_INSTANCING_ON SHADOWS_CUBE _ALPHABLEND_ON;PROCEDURAL_INSTANCING_ON SHADOWS_DEPTH _ALPHABLEND_ON t12
e189433c94caa592562407265069bd82|ps|636|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p3 PREPASSFINAL kw=UNITY_HDR_ON;LIGHTPROBE_SH UNITY_HDR_ON t012
3b99b731600d35362ff0d69972178fcd|ps|640|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s6p0 - kw=- t012
6c6d68f954f4600e11f3d4cf253451b2|ps|660|ps_4_0|IN-TABLE(l9=1612B)|mechanical||AC|TextMeshPro/Sprite[resources.assets#348] s0p0 - kw=UNITY_UI_CLIP_RECT t012 + UI/Default[R/unity_builtin_extra#10770] s0p0 - kw=UNITY_UI_CLIP_RECT t012
76d4975a0feea4408212da1b1f26bf76|ps|660|ps_4_0|IN-TABLE(l9=1568B)|mechanical||A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t0
b2d01400e7f1edd1cdfff3ce0f96cfae|ps|660|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p3 PREPASSFINAL kw=-;LIGHTPROBE_SH t012
babe34a6cb0681e6b8e3cc8c75923265|ps|664|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s3p0 - kw=- t012
d818c32166a3b9818092040b75f1043e|ps|664|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s5p0 - kw=- t012 + Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s5p1 - kw=- t012
6cc850ca9d06765a3cb102017053e9a5|ps|668|ps_4_0|IN-TABLE(l9=1252B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=OUTLINE_ON UNITY_UI_ALPHACLIP t012
657c3d822f001ce7c37ca2ac6bdbd93f|ps|676|ps_4_0|IN-TABLE(l9=2184B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=UNITY_UI_CLIP_RECT t012
f7fc610176625018bee08b99ae4fe267|ps|676|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s1p0 - kw=- t012
673904cdfad1111e37649f1b2a211955|ps|696|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p2 DEFERRED kw=- t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p2 DEFERRED kw=- t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
d5adf58d5ddd4e924658274fe615c4cf|ps|696|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p2 DEFERRED kw=UNITY_HDR_ON t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p2 DEFERRED kw=UNITY_HDR_ON t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
06778bb98a7763a31500bd1b39ea9646|ps|700|ps_5_0|uncovered|impossible|no:resource texture2dms|D|Hidden/BlitToDepth_MSAA[R/unity_builtin_extra#110] s0p0 - kw=- t012
0c233d02e471a550706e7f42a42ca8be|ps|704|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p4 DEFERRED kw=UNITY_HDR_ON t012
128727ad33eca9bf65ab160fe7e33baf|ps|704|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-MotionVectors[R/unity_builtin_extra#75] s0p0 MOTIONVECTORS kw=- t012
ee881aae3e114e40cc9794515d195407|ps|704|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p4 DEFERRED kw=- t012
9d17d20daa7be0ff203620be1f8e70fe|ps|712|ps_4_0|IN-TABLE(l9=1616B)|mechanical||A|_Custom/DailyChallengeMask[sharedassets0.assets#92] s0p0 - kw=- t012
ed80985f6c7fc1e6ca3bc9b8c634322f|ps|712|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s7p0 - kw=- t012 + Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s8p0 - kw=- t012
591119e257a1b008f408971f14d7bb13|ps|732|ps_4_0|uncovered|mech-approx|buildable|D|Hidden/CubeBlend[R/unity_builtin_extra#15106] s0p0 - kw=- t012 + Hidden/CubeBlend[R/unity_builtin_extra#15106] s1p0 - kw=- t012
b3b34961a4ba122133f4ae8047bd84c4|ps|736|ps_4_0|uncovered|mechanical|buildable|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p0 - kw=- t012
2e5f1c3a89c9d98476bfe9d51c6c4521|ps|744|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p2 DEFERRED kw=- t012 + INShaders/Unlit_Color[resources.assets#327] s0p2 DEFERRED kw=- t012 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
e69532ecf4424fc9aaffc2de9ac1bcf2|ps|744|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p2 DEFERRED kw=UNITY_HDR_ON t012 + INShaders/Unlit_Color[resources.assets#327] s0p2 DEFERRED kw=UNITY_HDR_ON t012 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
99a35508129616530a30c39aefa98cc1|ps|748|ps_4_0|IN-TABLE(l9=1612B)|mechanical||A|Depth Mask/MaskOverlayNV[resources.assets#342] s0p0 - kw=- t012
cc56cb127bc393ed2a0a680d61ebed80|ps|752|ps_4_0|IN-TABLE(l9=2068B)|mechanical||A|Legacy Shaders/Particles/Alpha Blended Premultiply[sharedassets0.assets#86] s0p0 - kw=SOFTPARTICLES_ON t012
3a039964b1c4d5afd30534e97fdd7bbc|ps|756|ps_4_0|IN-TABLE(l9=1756B)|mechanical||AC|TextMeshPro/Sprite[resources.assets#348] s0p0 - kw=UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012 + UI/Default[R/unity_builtin_extra#10770] s0p0 - kw=UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
fd31b10ef969cec3e25d3b981426c477|ps|756|ps_4_0|IN-TABLE(l9=2344B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=OUTLINE_ON UNITY_UI_CLIP_RECT t012
17e9b4a2b9a87acaab82d840a63a2430|ps|772|ps_4_0|IN-TABLE(l9=1892B)|mechanical||C|Hidden/Internal-GUITextureBlit[R/unity_builtin_extra#9003] s0p0 - kw=- t012 + Hidden/Internal-GUITextureBlit[R/unity_builtin_extra#9003] s1p0 - kw=- t012
787ed280c3fc0299f16f0d3df33002fd|ps|772|ps_4_0|IN-TABLE(l9=2332B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
6c6c056e105ef97c9b724886d005777d|ps|776|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p4 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t012
6fe81186ead3dbc8f3f3c3b77eb36b6f|ps|792|ps_4_0|uncovered|mechanical|buildable|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p9 - kw=- t012
7be7c2a7795d58266d7c4f3539d6ee6f|ps|792|ps_4_0|IN-TABLE(l9=1748B)|mechanical||C|Hidden/Internal-GUITextureClip[R/unity_builtin_extra#9000] s0p0 - kw=- t012 + Hidden/Internal-GUITextureClip[R/unity_builtin_extra#9000] s1p0 - kw=- t012
489de7c2752b8485c4c4b881c9d507ff|ps|800|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p4 DEFERRED kw=LIGHTPROBE_SH t012
647c14cbc4896a526c2c6dd7a493c1a2|ps|816|ps_4_0|uncovered|mechanical|buildable|D|Hidden/FrameDebuggerRenderTargetDisplay[R/unity%20default%20resources#10755] s0p0 - kw=- t012
74ff01c1b8b04ca54dcaf8f23c3654de|ps|816|ps_4_0|uncovered|impossible|no:resource texture2darray|D|Hidden/FrameDebuggerRenderTargetDisplay[R/unity%20default%20resources#10755] s1p0 - kw=- t012
df966e680adbc7563a93e7210fee2a04|ps|816|ps_4_0|uncovered|mechanical|buildable|D|Hidden/FrameDebuggerRenderTargetDisplay[R/unity%20default%20resources#10755] s0p1 - kw=- t012
9bdaa4688788a8b6d34c2b4848a2bc52|ps|848|ps_4_0|IN-TABLE(l9=2156B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=UNDERLAY_ON t012
f3f6af514c11de1b943e1f5918e3cf2b|ps|852|ps_4_0|IN-TABLE(l9=2492B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=OUTLINE_ON UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
832f11385b2bbecc0cbc11fedb27ebf1|ps|860|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t0
8b8ca8a344d274faf7c45f19fd6e3981|ps|872|ps_4_0|uncovered|mechanical|buildable|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p1 - kw=- t012
7a3b69d5e02537bddf8d055e0f3cd111|ps|896|ps_4_0|IN-TABLE(l9=1964B)|mechanical||A|Skybox/Procedural[sharedassets29.assets#13] s0p0 - kw=_SUNDISK_SIMPLE t012
974e1e1f69c3e95afb4107c03bbe591b|ps|908|ps_4_0|uncovered|mechanical|buildable|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p2 - kw=- t012
213de01a5a444e2dc47e384192812eb6|ps|928|ps_4_0|IN-TABLE(l9=2316B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=OUTLINE_ON UNDERLAY_ON t012
cf4898e1ee2ba727bc08be6927a0c0fa|ps|944|ps_4_0|IN-TABLE(l9=2280B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=UNDERLAY_ON UNITY_UI_ALPHACLIP t012
b03b590adcfd95385884448fbd844011|ps|968|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-CombineDepthNormals[R/unity_builtin_extra#65] s0p0 - kw=- t012
1849b3c4a0fbaa84d35ac7988d954b02|ps|996|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=POINT t0
fe6d42ccc0aa6cb586ed0caab1c8c1b5|ps|1016|ps_4_0|IN-TABLE(l9=2720B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=UNDERLAY_ON UNITY_UI_CLIP_RECT t012
03c63f5fd397309c0e665d9344ffa82a|ps|1024|ps_4_0|IN-TABLE(l9=2440B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=OUTLINE_ON UNDERLAY_ON UNITY_UI_ALPHACLIP t012
1ddc626cf91309f80cc3d15dc4f42c47|ps|1088|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=POINT_COOKIE t0
96030e7cecf6ff9eb02483954623a70a|ps|1088|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=DIRECTIONAL t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=DIRECTIONAL t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
23f013a15d0a48a54979a08fbac4eddd|ps|1096|ps_4_0|IN-TABLE(l9=2880B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=OUTLINE_ON UNDERLAY_ON UNITY_UI_CLIP_RECT t012
4d67270419d4963c8f9654f68f6ee0fd|ps|1112|ps_4_0|IN-TABLE(l9=2844B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=UNDERLAY_ON UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
8bc8e4ab3944b90d284b474a345143c2|ps|1120|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=DIRECTIONAL t0 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=DIRECTIONAL t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
f90303edb543af047b3864d51aacdf30|ps|1136|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s1p0 - kw=SHADOWS_SINGLE_CASCADE;SHADOWS_SINGLE_CASCADE SHADOWS_SPLIT_SPHERES t012
5aa2d6e0a064952ab998d001d50ba7cd|ps|1168|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p2 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p2 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
239534bacbefb4d72ad234f1186d8d66|ps|1192|ps_4_0|IN-TABLE(l9=3004B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=OUTLINE_ON UNDERLAY_ON UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
7e05cbcfd0eb1bf75cacb149705fee6c|ps|1192|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p2 DEFERRED kw=LIGHTPROBE_SH t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p2 DEFERRED kw=LIGHTPROBE_SH t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
121602558212746433a72b1140a94582|ps|1208|ps_4_0|IN-TABLE(l9=4280B)|mechanical||A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL t12
ecadfc84b73ed45e8e317a25c9499f38|ps|1208|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=DIRECTIONAL t12
2163a6ca7a41750adb86fa0000eb4b5a|ps|1216|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p2 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t0 + INShaders/Unlit_Color[resources.assets#327] s0p2 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
6152a84acd29b3c315df8b2945ee9f6e|ps|1232|ps_4_0|uncovered|hand|no:opcode ftoi|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p6 - kw=- t012
925adfdffd122f6de622711c4ea1d0d8|ps|1236|ps_4_0|IN-TABLE(l9=2620B)|mechanical||A|Skybox/Procedural[sharedassets29.assets#13] s0p0 - kw=_SUNDISK_HIGH_QUALITY t012
1492f66215a92f83a8ae077c650780af|ps|1240|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=SPOT t0
b480a8f5257f1fdcf0bcc7ebecb77a64|ps|1240|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p2 DEFERRED kw=LIGHTPROBE_SH t0 + INShaders/Unlit_Color[resources.assets#327] s0p2 DEFERRED kw=LIGHTPROBE_SH t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
39be6b2e9d8824039eecd4b677a6da12|ps|1264|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s0p0 - kw=SHADOWS_SINGLE_CASCADE;SHADOWS_SINGLE_CASCADE SHADOWS_SPLIT_SPHERES t012
ad6979f0af59e9bdea16979034d4e287|ps|1280|ps_4_0|IN-TABLE(l9=4384B)|mechanical||A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t12
0756dd7db373e70b9c2580737da74a5a|ps|1340|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-MotionVectors[R/unity_builtin_extra#75] s0p1 - kw=- t012
c441a67a2221e21e99fdec0c97da2fc4|ps|1348|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
a922edaaa5214226fbb75d2ddb3d5f3d|ps|1380|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t0 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
b4dc0508555a540e4ddd72bfbbf08869|ps|1392|ps_4_0|uncovered|hand|no:opcode ftoi|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p7 - kw=- t012
53f442acb1a5e9644d18e02fcd70482f|ps|1396|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-MotionVectors[R/unity_builtin_extra#75] s0p2 - kw=- t012
807eadc67afa323cd95d9b666961b32f|ps|1396|ps_4_0|uncovered|impossible|no:comparison sampler|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t0
3d728d7f1ceb604813ae2d4485062667|ps|1468|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t12
56bda13c4f358193e49db743c21d4752|ps|1480|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=POINT t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=POINT t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
c9f7eee609c1b7214ad6378dbdc83bab|ps|1512|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=POINT t0 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=POINT t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
212c78fb22187a92e4b94342ea2c3f0a|ps|1556|ps_4_0|IN-TABLE(l9=3700B)|mechanical||A|TextMeshPro/Distance Field[resources.assets#334] s0p0 - kw=- t012
813178f4cfb57f95a3dcab231cc5d0ba|ps|1572|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=POINT_COOKIE t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=POINT_COOKIE t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
64009ef1c0fde6e30b055e4513b322aa|ps|1584|ps_4_0|IN-TABLE(l9=3260B)|mech-approx||ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
8407caed32052da03e4ab98695087ca4|ps|1604|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=POINT_COOKIE t0 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=POINT_COOKIE t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
ec2db7c8e71248c45d6f47cd30c0c74d|ps|1604|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=POINT t12
3fca89d45beaee6184f657d23798943e|ps|1616|ps_4_0|IN-TABLE(l9=3380B)|mech-approx||AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL t0 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
6c4d649f611f608e4b07b954553c775a|ps|1632|ps_4_0|IN-TABLE(l9=3848B)|mechanical||A|TextMeshPro/Distance Field[resources.assets#334] s0p0 - kw=UNITY_UI_ALPHACLIP t012
0d78881762c51a1c8948ee2bc9a39936|ps|1640|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL UNITY_HDR_ON t0
f4014dfeb3edafb333afb7741d938df8|ps|1664|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL t0
6288aacf5e2f8646ae993f9a571193f1|ps|1696|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=POINT_COOKIE t12
15043dfaa0c09821a1ec80c96df01a38|ps|1724|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=SPOT t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=SPOT t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
6c7098ef63b01887687ad152b3ca150f|ps|1724|ps_4_0|IN-TABLE(l9=4140B)|mechanical||A|TextMeshPro/Distance Field[resources.assets#334] s0p0 - kw=UNITY_UI_CLIP_RECT t012
c55d84d9edd0f6bb93946fd8bb01eb6f|ps|1756|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=SPOT t0 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=SPOT t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
9338e817ac93f145e55c866f57c69800|ps|1784|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=DIRECTIONAL t012
f46792880c806289febd6d21c509ed8e|ps|1788|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t12
a454ee4f0dbd8aabe40052cfd8023e8d|ps|1800|ps_4_0|IN-TABLE(l9=4288B)|mechanical||A|TextMeshPro/Distance Field[resources.assets#334] s0p0 - kw=UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
945a9394f22ae51f76f0d16375f7b9ce|ps|1808|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=DIRECTIONAL t012
26bf3c2757f40427c0a1519ce5795ec1|ps|1848|ps_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p1 FORWARDADD kw=SPOT t12
8551aada26d4abf2b711eb6007237ca7|ps|1852|ps_4_0|uncovered|impossible|no:comparison sampler|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t0
869d30f95069d0ccc1b0efc549b5aec7|ps|1868|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT UNITY_HDR_ON t0
734e0aed66218e9fa39f7bf9b9905e6a|ps|1884|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL_COOKIE UNITY_HDR_ON t0
58637d2a93d7b324f8ae0eefdcd66726|ps|1892|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT t0
538c1146d0e427a88f243915c9d6ac33|ps|1908|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL_COOKIE t0
5a5a3667f4632076473449928fe1960e|ps|2008|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s1p0 - kw=- t012
09bf8268acc2e6d3a884b3a173c2b61b|ps|2020|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=DIRECTIONAL SHADOWS_SCREEN t012
97d56a90dcf4049ab5ccd8086ce634ab|ps|2024|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL SHADOWS_SCREEN UNITY_HDR_ON t0
9c1ea7cf9391cdd81ce6d42dc72540d3|ps|2028|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p2 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p2 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
037ed02d8ea5bae4b56987de674a501e|ps|2044|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=DIRECTIONAL SHADOWS_SCREEN t012
f9898f3c5fee959e826195e05fa980bf|ps|2048|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL SHADOWS_SCREEN t0
2ca4e65b6799f8639c58fe156af9fbd9|ps|2052|ps_4_0|IN-TABLE(l9=5284B)|mech-approx||ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
f50b221e5541f2b989fe9cbda7d07c35|ps|2052|ps_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p2 DEFERRED kw=LIGHTPROBE_SH t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p2 DEFERRED kw=LIGHTPROBE_SH t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
5661b650f067822c425c25ddf06a7a46|ps|2060|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=POINT t012
56647ca5d132bb1bcd820e096602648b|ps|2076|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=DIRECTIONAL_COOKIE t012
d30b06e75a8076236a38866e4770db88|ps|2076|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p2 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t12 + INShaders/Unlit_Color[resources.assets#327] s0p2 DEFERRED kw=LIGHTPROBE_SH UNITY_HDR_ON t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
5475c239ca7005d1ee2896c70de0d915|ps|2084|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=POINT t012
a0afb467e9f430c01dc1ffd1884b7792|ps|2084|ps_4_0|IN-TABLE(l9=5404B)|mech-approx||AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t0 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
83c6cbf814d06bdccc5a30d13ecf4dee|ps|2100|ps_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p2 DEFERRED kw=LIGHTPROBE_SH t12 + INShaders/Unlit_Color[resources.assets#327] s0p2 DEFERRED kw=LIGHTPROBE_SH t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
8bcc79c7cf5bf94f9c5e8c8995b41c05|ps|2100|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=DIRECTIONAL_COOKIE t012
af813b1e2c4fa171cc1eaf21ba6fbf86|ps|2112|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE UNITY_HDR_ON t0
abd187c6bacb4d7459aa93f8de396796|ps|2128|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s0p0 - kw=- t012
051c1bed0092bf5b464407ddc2604892|ps|2136|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE t0
a75db2ad811deaa8c82623404d26c47e|ps|2216|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SPOT UNITY_HDR_ON t0
a2ee8327eced3642c937891162f227b2|ps|2240|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SPOT t0
d20d5f1f90d962c413ea38e265cac7b1|ps|2264|ps_4_0|uncovered|mech-approx|buildable|D|Hidden/Internal-DeferredReflections[R/unity_builtin_extra#74] s0p0 - kw=- t0
10be34c72e7fe9b2edcaf549dcb53d5b|ps|2268|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL_COOKIE SHADOWS_SCREEN UNITY_HDR_ON t0
a69914aa0271e396d25dea1c1573bbd4|ps|2280|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=DIRECTIONAL_COOKIE SHADOWS_SCREEN t012
0f88bf255eef012e17d39ae3dc33941c|ps|2284|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s1p0 - kw=SHADOWS_SPLIT_SPHERES t012
74339ef3d70c24b3cc4e3361fbe7144d|ps|2292|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL_COOKIE SHADOWS_SCREEN t0
60433aa29cd79abc1f15b0125667087d|ps|2304|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=POINT_COOKIE t012
f6370db9a08c8070732632914e259cb0|ps|2304|ps_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=DIRECTIONAL_COOKIE SHADOWS_SCREEN t012
fb0ffabce22295f350449245f8b460d7|ps|2304|ps_4_0|uncovered|impossible|no:comparison sampler|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
105030bdeab7e6d11d52ff5642d861a2|ps|2328|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=POINT_COOKIE t012
75b637cc7dbc687ba187997f5596a099|ps|2336|ps_4_0|uncovered|impossible|no:comparison sampler|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t0 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
25d9752ac6af94c7e96c2ba1ae8942f4|ps|2392|ps_4_0|uncovered|hand|no:l9_1-limits|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=DIRECTIONAL t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=DIRECTIONAL t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
558c2463565b5141a5d58e3dcc625c44|ps|2408|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=SPOT t012
956cbe45f2502f5443ff24322f486cb6|ps|2412|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s0p0 - kw=SHADOWS_SPLIT_SPHERES t012
3e2a908bb6e4b511bc16d53d1e5fdcb3|ps|2424|ps_4_0|uncovered|hand|no:l9_1-limits|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=DIRECTIONAL t12 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=DIRECTIONAL t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
d60eae5915f95cd0c9d94987e5005c0d|ps|2432|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=SPOT t012
cc13e274254874f24c90cb0d1d887ebc|ps|2616|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT SHADOWS_CUBE UNITY_HDR_ON t0
a491559547b516601fc0bc2c39e4af1b|ps|2628|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=POINT SHADOWS_CUBE t012
ede19f60c070953800ae17161a5c18df|ps|2640|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT SHADOWS_CUBE t0
a74830f81baf4695c92d43a7510d1ddd|ps|2652|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=POINT SHADOWS_CUBE t012
e443191113ea5f4c367accd652cc08b5|ps|2652|ps_4_0|uncovered|hand|no:l9_1-limits|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
2f02f055a29676a559d8ca7f1e516b20|ps|2684|ps_4_0|uncovered|hand|no:l9_1-limits|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t12 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
44a62ca911886761d654bfa2a05ccc3d|ps|2772|ps_4_0|uncovered|impossible|no:comparison sampler|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t0 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t0 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
fe4f6d7793eccfe31672e067dadfee32|ps|2780|ps_4_0|uncovered|hand|no:l9_1-limits|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=POINT t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=POINT t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
444f9c5e0cefe05204f3bdecd91a94f2|ps|2804|ps_4_0|uncovered|impossible|no:comparison sampler|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t0 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t0 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
d883b0da0e437e31c99b2af82dfda742|ps|2812|ps_4_0|uncovered|hand|no:l9_1-limits|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=POINT t12 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=POINT t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
411759cb9440eb52393098b9b1539572|ps|2856|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SHADOWS_DEPTH SPOT UNITY_HDR_ON t0
7b33a159449f3a8c5db4817b9731b84b|ps|2860|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE UNITY_HDR_ON t0
0a81f0165328dc3cae38ed3183b82d71|ps|2868|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=SHADOWS_DEPTH SPOT t012
06d91b64b91f18350a5e116bf7313527|ps|2872|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=POINT_COOKIE SHADOWS_CUBE t012
cc76e490d98159132cff612cdfe6e4f0|ps|2872|ps_4_0|uncovered|hand|no:l9_1-limits|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=POINT_COOKIE t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=POINT_COOKIE t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
395f27915ae3abfc4869a2f01b130bcb|ps|2880|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SHADOWS_DEPTH SPOT t0
c3f6ac544db92633f761c350e862f8ee|ps|2884|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE t0
89f04f62074cbd8b2375a5314c01133c|ps|2892|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=SHADOWS_DEPTH SPOT t012
df955bb2f958e776ffd3e3daa008ce24|ps|2896|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE t012
0e1b8bc3558e09429240233d4689eddb|ps|2904|ps_4_0|uncovered|hand|no:l9_1-limits|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=POINT_COOKIE t12 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=POINT_COOKIE t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
aeac9a1c64c019d4fa1f60a1e3c4949e|ps|2940|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredReflections[R/unity_builtin_extra#74] s0p0 - kw=- t12
b2d9b887567e13aa7bcea29c7b707d33|ps|2976|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL UNITY_HDR_ON t12
1e4f59035c7a95689da56f9f77f37fe8|ps|3000|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL t12
875a6b6e856f76692f28d765019519f4|ps|3012|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=POINT SHADOWS_CUBE SHADOWS_SOFT t012
b5ceee521356442ecc51e178c073ca0d|ps|3016|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT SHADOWS_CUBE SHADOWS_SOFT UNITY_HDR_ON t0
e2fba83aec146972fa7de4e10b17c548|ps|3016|ps_4_0|uncovered|hand|no:l9_1-limits|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12
f54aec78b6129c8c1b4d54317a811e68|ps|3024|ps_4_0|uncovered|hand|no:l9_1-limits|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=SPOT t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=SPOT t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
59d974e6da5fad9bedc3ff5b83aa7f49|ps|3036|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=POINT SHADOWS_CUBE SHADOWS_SOFT t012
55e67cf2e85d663544c74cf1110ed07c|ps|3040|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT SHADOWS_CUBE SHADOWS_SOFT t0
8631380ae482c8463a76c6043db8ef3a|ps|3056|ps_4_0|uncovered|hand|no:l9_1-limits|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=SPOT t12 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=SPOT t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
316c799b1bed0e3f2470e17aac3f488b|ps|3196|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT UNITY_HDR_ON t12
e3e5109f2f95933171834272ff00c56f|ps|3216|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL_COOKIE UNITY_HDR_ON t12
956d68e715cdad11a20c1655dec9ceed|ps|3220|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT t12
9f1a170b5e2c01c3b6947197dff4bda0|ps|3240|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL_COOKIE t12
797604761ca9c8d09d6adc8fc63a3449|ps|3256|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=POINT_COOKIE SHADOWS_CUBE SHADOWS_SOFT t012
3d9d7789b57bf5733c4bc74103f983c3|ps|3260|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE SHADOWS_SOFT UNITY_HDR_ON t0
dd957f5287dad3472313aee4e8b035ec|ps|3280|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE SHADOWS_SOFT t012
29a50d3d19f9b3a43a74e5a8d6c1afc6|ps|3284|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE SHADOWS_SOFT t0
1c3df84a9b7867de9656b8619907f0a0|ps|3348|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL SHADOWS_SCREEN UNITY_HDR_ON t12
f74a3eec17841f6aa134885e38ac6a6c|ps|3372|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL SHADOWS_SCREEN t12
782aeff20563a9202e341c872c00551e|ps|3440|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE UNITY_HDR_ON t12
152b123a58110d9bef15ed811ee42298|ps|3464|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE t12
e9fd52c1e47debe522506079e830aa65|ps|3488|ps_4_0|uncovered|hand|no:derivatives (deriv_rtx)|C|Hidden/Internal-GUIRoundedRect[R/unity_builtin_extra#9004] s0p0 - kw=- t012 + Hidden/Internal-GUIRoundedRect[R/unity_builtin_extra#9004] s1p0 - kw=- t012
379ca98e1e913230e310af67a1a14419|ps|3540|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SPOT UNITY_HDR_ON t12
fae3604212d8db5ad67a0a9f59d1227f|ps|3564|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SPOT t12
921f03abe40570bcb3b31ae8a7ea4468|ps|3592|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL_COOKIE SHADOWS_SCREEN UNITY_HDR_ON t12
c7a00817bcae401d22b1e9a9ec484c32|ps|3616|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=DIRECTIONAL_COOKIE SHADOWS_SCREEN t12
008856c744957fb432cc49eff3a844ed|ps|3936|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT SHADOWS_CUBE UNITY_HDR_ON t12
aa05ef14e56e4dfaf960e2ab63369782|ps|3960|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT SHADOWS_CUBE t12
4d87e50f7a7469d66442a1f1d12bb4dc|ps|4100|ps_4_0|IN-TABLE(l9=2884B)|hand||ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
9da1554febdda34cf03553bc63268721|ps|4132|ps_4_0|IN-TABLE(l9=2976B)|hand||AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL t12 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
af88068d94f8a41f567766d83ad372f3|ps|4180|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE UNITY_HDR_ON t12
f717855ac8fa2817ca18c1499bd4f1a5|ps|4180|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SHADOWS_DEPTH SPOT UNITY_HDR_ON t12
3266f1c64bae6cc24a699f7e49f9f670|ps|4204|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SHADOWS_DEPTH SPOT t12
446b93e47cac11dd76bd95511801578f|ps|4204|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE t12
4d1e7daf2eb6f32b35d24a85dbd3768e|ps|4216|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SHADOWS_DEPTH SHADOWS_SOFT SPOT UNITY_HDR_ON t0
22f8993caf2f98c7108606a7fe91d02d|ps|4228|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=SHADOWS_DEPTH SHADOWS_SOFT SPOT t012
9067cb10221dc3934a06600886e96a21|ps|4240|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SHADOWS_DEPTH SHADOWS_SOFT SPOT t0
517c7eee215fa5d2cb2a499e9d92ba3b|ps|4252|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=SHADOWS_DEPTH SHADOWS_SOFT SPOT t012
776d87a139626ea7e4184fc9aa8caaef|ps|4316|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT SHADOWS_CUBE SHADOWS_SOFT UNITY_HDR_ON t12
b0970b675be9e8352afb0ec3f7d06550|ps|4340|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT SHADOWS_CUBE SHADOWS_SOFT t12
533bc806736f53a2b36824d47303b3d0|ps|4404|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s3p0 - kw=SHADOWS_SINGLE_CASCADE;SHADOWS_SINGLE_CASCADE SHADOWS_SPLIT_SPHERES t012
31df1effe25eba5a5c7a3a4c3e36178f|ps|4532|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s2p0 - kw=SHADOWS_SINGLE_CASCADE;SHADOWS_SINGLE_CASCADE SHADOWS_SPLIT_SPHERES t012
353fc852be8bc463f778cd042b1152e7|ps|4560|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE SHADOWS_SOFT UNITY_HDR_ON t12
c2eb9f3cce569b042355954452771fda|ps|4584|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=POINT_COOKIE SHADOWS_CUBE SHADOWS_SOFT t12
639ddcd64efa50107a79cf5017cab316|ps|4604|ps_4_0|uncovered|hand|no:l9_1-limits|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
68f5c5c7280df8d7bb3b583811b2ada6|ps|4636|ps_4_0|uncovered|hand|no:l9_1-limits|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t12 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
73196949f126699f2a0d4edfaf82f3e2|ps|5296|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s3p0 - kw=- t012
3be91010b107384a49c446cc10665d6c|ps|5300|ps_4_0|IN-TABLE(l9=3592B)|hand||ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
b18400dcf7c8a8f6491f6fb26d2b468f|ps|5332|ps_4_0|IN-TABLE(l9=3684B)|hand||AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t12 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
4042aff525a239ec524bc61fae8ad2d2|ps|5416|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s2p0 - kw=- t012
72c0d21230446d6c0f11ab1e84ffd58d|ps|5504|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SHADOWS_DEPTH SHADOWS_SOFT SPOT UNITY_HDR_ON t12
5d70b05eb52252877d9ca532c1221406|ps|5528|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p0 - kw=SHADOWS_DEPTH SHADOWS_SOFT SPOT t12
f0af46c5182689618ff4116000fca9c5|ps|5572|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s3p0 - kw=SHADOWS_SPLIT_SPHERES t012
d090f359cf9b74ff29dff411639af05e|ps|5700|ps_4_0|uncovered|impossible|no:comparison sampler|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s2p0 - kw=SHADOWS_SPLIT_SPHERES t012
3258341b2d6c60d792c1caf41aa975cf|ps|5804|ps_4_0|uncovered|hand|no:l9_1-limits|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
459104670b45bcedc447823e490a7089|ps|5836|ps_4_0|uncovered|hand|no:l9_1-limits|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
d55ee1ea9b7f15cc377efca601b4216a|ps|13220|ps_4_0|uncovered|hand|no:l9_1-limits|D|Hidden/CubeBlur[R/unity_builtin_extra#15104] s0p0 - kw=- t012
6d31dcd91ab28cc3e6bf576e54baee55|vs|532|vs_4_0|IN-TABLE(l9=1920B)|mechanical||AC|_Custom/Unlit_Monochrome[resources.assets#340] s0p0 - kw=- t012 + Hidden/InternalErrorShader[R/unity%20default%20resources#17] s0p0 - kw=-;STEREO_MULTIVIEW_ON t012 + Hidden/Internal-StencilWrite[R/unity_builtin_extra#19] s0p0 - kw=- t012
7ccd6412f9053d38a8fbcf5d506edf40|vs|612|vs_4_0|uncovered|hand|no:opcode ishl|C|Hidden/InternalErrorShader[R/unity%20default%20resources#17] s0p0 - kw=UNITY_SINGLE_PASS_STEREO t012
8132d2f45c7207338468c36c6c349d6e|vs|636|vs_4_0|IN-TABLE(l9=2092B)|mechanical||A|_Custom/Utility_ZWrite[resources.assets#338] s0p0 - kw=- t012
bec3e12adf7e0d6ea3a5ea4d330fa1e8|vs|636|vs_4_0|IN-TABLE(l9=2048B)|mechanical||C|Hidden/InternalClear[R/unity%20default%20resources#68] s0p0 - kw=-;STEREO_MULTIVIEW_ON t012 + Hidden/InternalClear[R/unity%20default%20resources#68] s0p1 - kw=-;STEREO_MULTIVIEW_ON t012 + (6 more owners: Hidden/InternalClear)
056167a6cb85f022e72890ab407350fd|vs|644|vs_4_0|IN-TABLE(l9=2056B)|mechanical||BCD|Hidden/SnapshotEffect[resources.assets#346] s0p0 - kw=- t012 + Hidden/Internal-DeferredShading[R/unity_builtin_extra#69] s0p1 - kw=- t012 + Sprites/Mask[R/unity_builtin_extra#10757] s0p0 - kw=-;ETC1_EXTERNAL_ALPHA t012
3e028ca2c0a6eeda2c41a9cfc5021190|vs|644|vs_4_0|uncovered|mechanical|buildable|D|Hidden/FrameDebuggerRenderTargetDisplay[R/unity%20default%20resources#10755] s0p0 - kw=- t012 + Hidden/FrameDebuggerRenderTargetDisplay[R/unity%20default%20resources#10755] s0p1 - kw=- t012 + (3 more owners: Hidden/FrameDebuggerRenderTargetDisplay, Hidden/CubeBlend)
743d819175353181210eb73d7aad4577|vs|664|vs_4_0|IN-TABLE(l9=2240B)|mechanical||C|Hidden/Internal-Colored[R/unity%20default%20resources#69] s0p0 - kw=-;STEREO_MULTIVIEW_ON t012
9610a57a174d2256147a210ecc6a0557|vs|676|vs_4_0|uncovered|mechanical|buildable|D|Hidden/CubeBlur[R/unity_builtin_extra#15104] s0p0 - kw=- t012 + Hidden/CubeBlur[R/unity_builtin_extra#15104] s1p0 - kw=- t012 + (2 more owners: Hidden/CubeBlur, Hidden/CubeCopy)
bd02211fcbf84cc2adaa61feed1f47f2|vs|676|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s0p0 - kw=- t012
36d14d52b758f8d4b8a5cc86eeef4266|vs|684|vs_4_0|IN-TABLE(l9=2264B)|mechanical||ABCD|Unlit/Texture[resources.assets#323] s0p0 - kw=- t012 + Depth Mask/Unlit Transparent ZAlways[resources.assets#325] s0p0 - kw=- t012 + (17 more owners: Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG), Unlit/Transparent, _Custom/PreAlpha_Unlit_ColorTransparent_Geometry_Gray, _Custom/PreAlpha_Unlit_ColorTransparent_GeometryZ...)
4b93201f1cdfeebc2f07e6e193b1e912|vs|684|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Compositing[R/unity_builtin_extra#17000] s0p0 - kw=- t012
bbdfaf3bd6e4385af84da66da1b03cb3|vs|688|vs_5_0|uncovered|mechanical|buildable|D|Hidden/BlitToDepth[R/unity_builtin_extra#109] s0p0 - kw=- t012 + Hidden/BlitToDepth_MSAA[R/unity_builtin_extra#110] s0p0 - kw=- t012
416eaaa83f5b2434d319474003855516|vs|716|vs_4_0|uncovered|hand|no:opcode ishl|C|Hidden/InternalClear[R/unity%20default%20resources#68] s0p0 - kw=UNITY_SINGLE_PASS_STEREO t012 + Hidden/InternalClear[R/unity%20default%20resources#68] s0p1 - kw=UNITY_SINGLE_PASS_STEREO t012 + (6 more owners: Hidden/InternalClear)
1e2dffc8a2d7da1672bc158b73601bb3|vs|744|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DeferredReflections[R/unity_builtin_extra#74] s0p1 - kw=-;UNITY_HDR_ON t012
690508e368854bdff249a82997bc642a|vs|744|vs_4_0|uncovered|mechanical|buildable|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p4 - kw=- t012 + Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p5 - kw=- t012 + (4 more owners: Hidden/VideoDecode)
9397d8fca457c2ba690a3728e33814a9|vs|744|vs_4_0|uncovered|hand|no:opcode ishl|C|Hidden/Internal-Colored[R/unity%20default%20resources#69] s0p0 - kw=UNITY_SINGLE_PASS_STEREO t012
3ddf4510a9f21d014f3d7f6d4b883e79|vs|748|vs_4_0|IN-TABLE(l9=2428B)|mechanical||A|_Custom/Text Shader With Z Test[resources.assets#339] s0p0 - kw=- t012
5fe614a70abfd7023f09eba0c2404950|vs|748|vs_4_0|uncovered|hand|no:opcode itof|D|Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p0 - kw=- t012 + Hidden/VideoDecode[R/unity_builtin_extra#16000] s0p1 - kw=- t012 + (2 more owners: Hidden/VideoDecode)
0794f846bc17b8a654989f35a1b803ab|vs|780|vs_4_0|uncovered|impossible|no:SV_InstanceID/VertexID|C|Hidden/InternalErrorShader[R/unity%20default%20resources#17] s0p0 - kw=STEREO_INSTANCING_ON t012
316e59ff5b6457933a79e7dfdbbd6e3b|vs|784|vs_4_0|IN-TABLE(l9=2432B)|mechanical||A|Spine/Skeleton[sharedassets0.assets#90] s0p0 - kw=- t012
71fe6a281010be801633621c36a49d6d|vs|784|vs_4_0|IN-TABLE(l9=2420B)|mechanical||A|Legacy Shaders/Particles/Alpha Blended Premultiply[sharedassets0.assets#86] s0p0 - kw=- t012
83010061c3fb1757d2e1fc384afc8ec9|vs|784|vs_4_0|IN-TABLE(l9=2388B)|mechanical||CD|Hidden/Internal-Flare[R/unity_builtin_extra#102] s0p0 - kw=- t012 + Hidden/Internal-Halo[R/unity_builtin_extra#105] s0p0 - kw=- t012 + (2 more owners: Hidden/Internal-Flare, Hidden/Internal-Halo, Hidden/Internal-GUITexture)
ea39864731de6efa4f8456344897a7c0|vs|784|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s1p0 - kw=- t012
672d44e41ca1499544a51a1fe5b07b05|vs|796|vs_4_0|IN-TABLE(l9=2436B)|mechanical||A|GUI/Text Shader[R/unity%20default%20resources#10101] s0p0 - kw=-;STEREO_MULTIVIEW_ON t012
bde71c575539fc6a381518d43bf37a7a|vs|816|vs_4_0|IN-TABLE(l9=2480B)|mechanical||A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=-;_ALPHABLEND_ON t012
2fb5c1201c23ff3e66645b3f7773c09e|vs|840|vs_4_0|IN-TABLE(l9=2520B)|mechanical||A|e2d/Curve[resources.assets#328] s0p0 - kw=- t012
7139eb8a8bfbe9d9e280463c1b23a457|vs|840|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-MotionVectors[R/unity_builtin_extra#75] s0p1 - kw=- t012 + Hidden/Internal-MotionVectors[R/unity_builtin_extra#75] s0p2 - kw=- t012
114c264921d354d0d03d5d80cd0e22db|vs|852|vs_4_0|IN-TABLE(l9=2576B)|mechanical||AC|TextMeshPro/Sprite[resources.assets#348] s0p0 - kw=-;UNITY_UI_ALPHACLIP;UNITY_UI_CLIP_RECT;UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012 + UI/Default[R/unity_builtin_extra#10770] s0p0 - kw=-;UNITY_UI_ALPHACLIP;UNITY_UI_CLIP_RECT;UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
a3c96d5b04b0c7fceb7126991f4f9c7a|vs|852|vs_4_0|IN-TABLE(l9=2616B)|mechanical||A|Sprites/Default[globalgamemanagers.assets#3] s0p0 - kw=-;ETC1_EXTERNAL_ALPHA t012
1539d0cc6aad125e4fe8c525acb58827|vs|856|vs_4_0|IN-TABLE(l9=2732B)|mechanical||C|Sprites/Mask[R/unity_builtin_extra#10757] s0p0 - kw=PIXELSNAP_ON;ETC1_EXTERNAL_ALPHA PIXELSNAP_ON t012
f4e3bd5f634722008fe7e9fcde126914|vs|876|vs_4_0|uncovered|hand|no:opcode ishl|A|GUI/Text Shader[R/unity%20default%20resources#10101] s0p0 - kw=UNITY_SINGLE_PASS_STEREO t012
68a2f015493c7232fae385677af1d6f4|vs|884|vs_4_0|IN-TABLE(l9=2776B)|mechanical||A|Depth Mask/MaskOverlayNV[resources.assets#342] s0p0 - kw=- t012
f8459e2f4bc103891dd7232aba418051|vs|884|vs_4_0|IN-TABLE(l9=2808B)|mechanical||A|_Custom/PreAlpha_Unlit_ColorTransparent_Geometry_Shiny[resources.assets#332] s0p0 - kw=- t012
f8b140c9ca775ea0e1a3469ea618b6f3|vs|884|vs_4_0|uncovered|impossible|no:SV_InstanceID/VertexID|C|Hidden/InternalClear[R/unity%20default%20resources#68] s0p0 - kw=STEREO_INSTANCING_ON t012 + Hidden/InternalClear[R/unity%20default%20resources#68] s0p1 - kw=STEREO_INSTANCING_ON t012 + (6 more owners: Hidden/InternalClear)
1fc8c6ba193b9e6e8a9e9893888138ae|vs|896|vs_4_0|uncovered|mechanical|buildable|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p1 VertexLM kw=- t012
33caf2bb06d711cb43a909054466cc6d|vs|912|vs_4_0|uncovered|impossible|no:SV_InstanceID/VertexID|C|Hidden/Internal-Colored[R/unity%20default%20resources#69] s0p0 - kw=STEREO_INSTANCING_ON t012
dbe97c6d336bccc5f44cef363f1e027b|vs|1036|vs_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p2 PREPASSBASE kw=- t012
8b83860e090964e462ca2f43953811cf|vs|1048|vs_4_0|uncovered|impossible|no:SV_InstanceID/VertexID|A|GUI/Text Shader[R/unity%20default%20resources#10101] s0p0 - kw=STEREO_INSTANCING_ON t012
71fe3617ccb97e20fabd4f511bf0aaa9|vs|1064|vs_4_0|IN-TABLE(l9=3296B)|mechanical||A|Sprites/Default[globalgamemanagers.assets#3] s0p0 - kw=PIXELSNAP_ON;ETC1_EXTERNAL_ALPHA PIXELSNAP_ON t012
71aa6655ebbf1a00e965cb46162c4322|vs|1076|vs_4_0|IN-TABLE(l9=3024B)|mechanical||C|Hidden/Internal-GUITextureClip[R/unity_builtin_extra#9000] s0p0 - kw=- t012 + Hidden/Internal-GUITextureClip[R/unity_builtin_extra#9000] s1p0 - kw=- t012
e9944487cd9daa2744e9f9fce2134ca6|vs|1076|vs_4_0|IN-TABLE(l9=2996B)|mechanical||C|Hidden/Internal-GUITextureBlit[R/unity_builtin_extra#9003] s0p0 - kw=- t012 + Hidden/Internal-GUITextureBlit[R/unity_builtin_extra#9003] s1p0 - kw=- t012
a09ac01c39d48fe08fb544f62b2e8019|vs|1084|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s4p0 - kw=- t012
9c82ad4bfe195586520bf40e118ec35b|vs|1088|vs_4_0|IN-TABLE(l9=3000B)|mechanical||C|Hidden/Internal-GUITextureClipText[R/unity_builtin_extra#9001] s0p0 - kw=- t012 + Hidden/Internal-GUITextureClipText[R/unity_builtin_extra#9001] s1p0 - kw=- t012
c2787c3269d7057ec7c663f1a2352912|vs|1112|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p0 - kw=POINT;DIRECTIONAL;SPOT;POINT_COOKIE;DIRECTIONAL_COOKIE;SHADOWS_DEPTH SPOT;SHADOWS_DEPTH SHADOWS_SOFT SPOT;DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL_COOKIE SHADOWS_SCREEN;POINT SHADOWS_CUBE;POINT SHADOWS_CUBE SHADOWS_SOFT;POINT_COOKIE SHADOWS_CUBE;POINT_COOKIE SHADOWS_CUBE SHADOWS_SOFT t012 + Hidden/Internal-PrePassLighting[R/unity_builtin_extra#63] s0p1 - kw=POINT;DIRECTIONAL;SPOT;POINT_COOKIE;DIRECTIONAL_COOKIE;SHADOWS_DEPTH SPOT;SHADOWS_DEPTH SHADOWS_SOFT SPOT;DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL_COOKIE SHADOWS_SCREEN;POINT SHADOWS_CUBE;POINT SHADOWS_CUBE SHADOWS_SOFT;POINT_COOKIE SHADOWS_CUBE;POINT_COOKIE SHADOWS_CUBE SHADOWS_SOFT t012 + (2 more owners: Hidden/Internal-PrePassLighting, Hidden/Internal-DeferredShading, Hidden/Internal-DeferredReflections)
8a4459b79917ee09e40a937b0ac38257|vs|1132|vs_4_0|IN-TABLE(l9=3052B)|mechanical||C|Hidden/Internal-GUIRoundedRect[R/unity_builtin_extra#9004] s0p0 - kw=- t012 + Hidden/Internal-GUIRoundedRect[R/unity_builtin_extra#9004] s1p0 - kw=- t012
e73fdf6998774f77a26ecb5486a3f095|vs|1180|vs_4_0|IN-TABLE(l9=3268B)|mechanical||A|Legacy Shaders/Particles/Alpha Blended Premultiply[sharedassets0.assets#86] s0p0 - kw=SOFTPARTICLES_ON t012
5fd9876b8a2302be351fd235f4efb6f3|vs|1184|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s5p0 - kw=- t012 + Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s5p1 - kw=- t012
82f090f56579eb81fd0bcc9892d1cf8c|vs|1212|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=DIRECTIONAL t012 + Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p2 DEFERRED kw=-;UNITY_HDR_ON t012 + (18 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
88911eccb6245bb4671cfaae631039a0|vs|1212|vs_4_0|IN-TABLE(l9=3136B)|mechanical||A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL;DIRECTIONAL VERTEXLIGHT_ON t012
975cb44c19ddb66b2502a1ce2c2ba05a|vs|1212|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=DIRECTIONAL t012 + INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p2 DEFERRED kw=-;UNITY_HDR_ON t012 + (7 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
a9d10993c51d483a407da6d33793125b|vs|1212|vs_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p4 DEFERRED kw=-;UNITY_HDR_ON t012
c3e16448ca97c112a0dffa914fea01c8|vs|1212|vs_4_0|IN-TABLE(l9=3328B)|mechanical||A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=SOFTPARTICLES_ON;SOFTPARTICLES_ON _ALPHABLEND_ON t012
03090d7cd167f1cc308c0d30a4de4579|vs|1252|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s0p0 - kw=-;SHADOWS_SPLIT_SPHERES;SHADOWS_SINGLE_CASCADE;SHADOWS_SINGLE_CASCADE SHADOWS_SPLIT_SPHERES t012 + Hidden/Internal-ScreenSpaceShadows[R/unity_builtin_extra#64] s1p0 - kw=-;SHADOWS_SPLIT_SPHERES;SHADOWS_SINGLE_CASCADE;SHADOWS_SINGLE_CASCADE SHADOWS_SPLIT_SPHERES t012 + (2 more owners: Hidden/Internal-ScreenSpaceShadows)
d530777c519960ed63b00b40969b056a|vs|1260|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s6p0 - kw=- t012
719678d88b720fbf90a92ceb89d155f9|vs|1280|vs_4_0|IN-TABLE(l9=3216B)|mechanical||AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL;DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL VERTEXLIGHT_ON;DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON t012 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL;DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL VERTEXLIGHT_ON;DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON t012 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
778fbaf11d8ef26044d432f59996a03b|vs|1280|vs_4_0|IN-TABLE(l9=3184B)|mechanical||ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL;DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL VERTEXLIGHT_ON;DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL;DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL VERTEXLIGHT_ON;DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
699b506753c526a3c56b52f1f901d88c|vs|1292|vs_4_0|uncovered|mechanical|BUILT-OK(tested)|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p2 SHADOWCASTER kw=SHADOWS_CUBE t012
7d78fbfaf1bc11651853e18f3a18424d|vs|1296|vs_4_0|uncovered|hand|no:immediate constant buffer|D|Hidden/ConvertTexture[R/unity_builtin_extra#68] s0p0 - kw=- t012
b5f14cfc3b3131e62d905af3f4195d6a|vs|1316|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-MotionVectors[R/unity_builtin_extra#75] s0p0 MOTIONVECTORS kw=- t012
f7e32a0910040df455b1a7d4a63b1265|vs|1320|vs_4_0|uncovered|mechanical|BUILT-OK(tested)|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=SHADOWS_CUBE t012
4f7042b5d6980f83ba339783f99fd29b|vs|1404|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
afd6e639b4690adb41788a23baf57278|vs|1404|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON t12 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL SHADOWS_SCREEN;DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON t12 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
7eda7bd3a50b1aea45191dd3db67f4af|vs|1408|vs_4_0|**MISS#1 VS**|mechanical|BUILT-OK(tested)|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p2 SHADOWCASTER kw=SHADOWS_DEPTH t012
ec874e4202b5a9e78480ffa113e9ec23|vs|1436|vs_4_0|uncovered|mechanical|BUILT-OK(tested)|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=SHADOWS_DEPTH t012
9b8b9f3e0ba4b3a85b15e5ff574459f6|vs|1440|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
b42b01e7eeae6cf48b2de7e5c796af08|vs|1440|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=POINT;POINT_COOKIE t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=POINT;POINT_COOKIE t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
d063d74c51fe6e32e39586c4213b9833|vs|1440|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=POINT;POINT_COOKIE t012 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=POINT;POINT_COOKIE t012 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
d12c2ced8907f72a76da85f1d5fa6f49|vs|1440|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t012 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=DIRECTIONAL_COOKIE t012 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
d1e9355ed399542cd9989229c9fa6fb4|vs|1440|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p1 FORWARDADD kw=SPOT t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p1 FORWARDADD kw=SPOT t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
ebf459042437dbafba62b6d5ff672f79|vs|1440|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p1 FORWARDADD kw=SPOT t012 + INShaders/Unlit_Color[resources.assets#327] s0p1 FORWARDADD kw=SPOT t012 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
53ebb3f18e7f089573781d1880c5a878|vs|1484|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s6p0 - kw=- t012
e8e7fc89f0811cee3a03ce9369229eaa|vs|1508|vs_4_0|**MISS#2 VS**|mechanical|BUILT-OK(tested)|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=SHADOWS_CUBE _ALPHABLEND_ON t012
05677036cc9635d34a799ed542399ade|vs|1516|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p2 DEFERRED kw=LIGHTPROBE_SH;LIGHTPROBE_SH UNITY_HDR_ON t012 + INShaders/Unlit_Color[resources.assets#327] s0p2 DEFERRED kw=LIGHTPROBE_SH;LIGHTPROBE_SH UNITY_HDR_ON t012 + (2 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent)
0d6b7c94f102ab847f9733c54c41941d|vs|1516|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p2 DEFERRED kw=LIGHTPROBE_SH;LIGHTPROBE_SH UNITY_HDR_ON t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p2 DEFERRED kw=LIGHTPROBE_SH;LIGHTPROBE_SH UNITY_HDR_ON t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
b3040ffef74e4dd42c10acb7cc26cb74|vs|1544|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s0p0 - kw=- t012
57f3eb86400124cc84151264dfc19702|vs|1584|vs_4_0|IN-TABLE(l9=5108B)|mechanical||AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH;DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t012 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH;DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t012 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
c7a5775db8c0f630800384b5df4e5b71|vs|1584|vs_4_0|IN-TABLE(l9=5076B)|mechanical||ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH;DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH;DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
c350ad9b364ec53a4326f0ffab814a61|vs|1624|vs_4_0|uncovered|mechanical|BUILT-OK(tested)|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=SHADOWS_DEPTH _ALPHABLEND_ON t012
b8c4fad0c01f1359811468e35bf22dfe|vs|1652|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s1p0 - kw=- t012
0416e432da40f318c7d5c68698fc2da9|vs|1708|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
bb6b466c2a8bb6489f3a0102c3b061e8|vs|1708|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN t12 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
09efb6d3170fa62f7653a801d60b0bb1|vs|1880|vs_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p4 DEFERRED kw=LIGHTPROBE_SH;LIGHTPROBE_SH UNITY_HDR_ON t012
1e5fefa6e527e889e6a147cd6fadfe67|vs|1880|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s2p0 - kw=- t012
9e111216a071971c6c1a7d0ca589d86b|vs|1880|vs_4_0|IN-TABLE(l9=5632B)|mechanical||A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH t012
f6303d9d8188a1a8921fabd410348ea1|vs|1968|vs_5_0|uncovered|impossible|no:structured-buffer|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=PROCEDURAL_INSTANCING_ON;PROCEDURAL_INSTANCING_ON _ALPHABLEND_ON t012
a7acf23abc86487abb2a7c615d586104|vs|1972|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s4p0 - kw=- t012
ebd21fc8e5061adc5f8f8e1724ec7f97|vs|2016|vs_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p3 PREPASSFINAL kw=-;LIGHTPROBE_SH;UNITY_HDR_ON;LIGHTPROBE_SH UNITY_HDR_ON t012
9211201ff6f3c4bf50a7a368d9e23579|vs|2060|vs_5_0|uncovered|impossible|no:structured-buffer|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=PROCEDURAL_INSTANCING_ON SHADOWS_CUBE t012
5b9af1877661d1ac8d3844b97afe2a92|vs|2072|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s5p0 - kw=- t012
ad54b86ca0883e31ec9510ec87f2cc05|vs|2088|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s8p0 - kw=- t012
77fec71569b799001fa2203f47154d6a|vs|2096|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s5p1 - kw=- t012
3f0d1dd7fede8a286a74f387c674c82e|vs|2176|vs_5_0|uncovered|impossible|no:structured-buffer|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=PROCEDURAL_INSTANCING_ON SHADOWS_DEPTH t012
dd55b16e807f47553977e2bfc95c71c1|vs|2244|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s7p0 - kw=- t012
0bf1ad9711b8d3819194e6ac2b9dcb15|vs|2348|vs_5_0|uncovered|impossible|no:structured-buffer|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p2 FORWARDBASE kw=PROCEDURAL_INSTANCING_ON SOFTPARTICLES_ON;PROCEDURAL_INSTANCING_ON SOFTPARTICLES_ON _ALPHABLEND_ON t012
6b6510aa7690651cf207c064d9af4e01|vs|2456|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH VERTEXLIGHT_ON;DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON t012 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH VERTEXLIGHT_ON;DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON t012 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
c17843c26b6f086665b6a3e64d1122f9|vs|2456|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH VERTEXLIGHT_ON;DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON t012 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH VERTEXLIGHT_ON;DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON t012 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
1c6837dca8ff1888c39d242178c2d3c6|vs|2608|vs_4_0|uncovered|mechanical|buildable|ABD|Hidden/BlitCopyHDRTonemap[globalgamemanagers.assets#4] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON t12 + TextMeshPro/Mobile/Distance Field (Surface)[resources.assets#326] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON t12 + (8 more owners: Hidden/BlitCopyHDRTonemap, TextMeshPro/Mobile/Distance Field (Surface), TextMeshPro/Distance Field, INShaders/Unlit_ColorTransparent_GrayOverlay, Unlit/Cyber, TextMeshPro/Distance Field (Surface), TextMeshPro/Mobile/Distance Field - Masking, TextMeshPro/Mobile/Distance Field...)
c833d258f770055f80ce7042c4147ad2|vs|2608|vs_4_0|uncovered|mechanical|buildable|AB|INShaders/Unlit_ColorTransparent_SolidColor[resources.assets#324] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON t12 + INShaders/Unlit_Color[resources.assets#327] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON t12 + (3 more owners: INShaders/Unlit_ColorTransparent_SolidColor, INShaders/Unlit_Color, TextMeshPro/Sprite, INShaders/Unlit_ColorTransparent, Legacy Shaders/Diffuse)
6a1fa19b44c67e16b0f66e91135cc4d7|vs|2636|vs_4_0|uncovered|mechanical|buildable|A|Legacy Shaders/Diffuse[R/unity_builtin_extra#7] s0p0 FORWARDBASE kw=DIRECTIONAL LIGHTPROBE_SH VERTEXLIGHT_ON t012
23fc56d795371a8c8d78b7e613ab81cb|vs|2648|vs_5_0|uncovered|impossible|no:structured-buffer|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=PROCEDURAL_INSTANCING_ON SHADOWS_CUBE _ALPHABLEND_ON t012
2b35b71b5d35dc10d0293933cff2adbb|vs|2756|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-ODSWorldTexture[R/unity_builtin_extra#15308] s3p0 - kw=- t012
520b919dd37a4b2213a8d7e7a5defbcb|vs|2764|vs_5_0|uncovered|impossible|no:structured-buffer|A|Particles/Standard Unlit[sharedassets0.assets#87] s0p1 SHADOWCASTER kw=PROCEDURAL_INSTANCING_ON SHADOWS_DEPTH _ALPHABLEND_ON t012
a9bc2f9027652bac49696ceaa84c01ac|vs|2884|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s2p0 - kw=- t012
46b1cae45bf8b27c344b4e52aae541b4|vs|2932|vs_4_0|IN-TABLE(l9=6576B)|mechanical||A|Depth Mask/MaskOverlay[resources.assets#330] s0p0 Vertex kw=- t012
eb872e071a29e76ec3077d476c7cb9b8|vs|2956|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s8p0 - kw=- t012
f647aa27493cd8477da72b7898bec3b9|vs|3052|vs_4_0|IN-TABLE(l9=7244B)|mechanical||A|TextMeshPro/Distance Field[resources.assets#334] s0p0 - kw=-;UNITY_UI_ALPHACLIP;UNITY_UI_CLIP_RECT;UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
435fa5abf82424e4fb7b800f53ad0adf|vs|3092|vs_4_0|IN-TABLE(l9=7224B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=-;OUTLINE_ON;UNITY_UI_ALPHACLIP;OUTLINE_ON UNITY_UI_ALPHACLIP;UNITY_UI_CLIP_RECT;OUTLINE_ON UNITY_UI_CLIP_RECT;UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT;OUTLINE_ON UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
d5e8f044098b225bc0b6efb47a09875d|vs|3112|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s7p0 - kw=- t012
5e62f5a68548f2b45ed2e7bc852aa6ad|vs|3188|vs_4_0|uncovered|mechanical|buildable|D|Hidden/Internal-DepthNormalsTexture[R/unity_builtin_extra#62] s3p0 - kw=- t012
939a2d9bb6aabb263232ad8875b3ede9|vs|3288|vs_4_0|uncovered|hand|no:loop/switch flow control (loop)|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p0 Vertex kw=- t012
072d888e0396c7e920fe3a62784757d7|vs|3680|vs_4_0|uncovered|hand|no:loop/switch flow control (loop)|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p0 Vertex kw=POINT t012
026ed52ac59088409db21aa745b19892|vs|3696|vs_4_0|IN-TABLE(l9=8188B)|mechanical||A|TextMeshPro/Mobile/Distance Field[resources.assets#350] s0p0 - kw=UNDERLAY_ON;OUTLINE_ON UNDERLAY_ON;UNDERLAY_ON UNITY_UI_ALPHACLIP;OUTLINE_ON UNDERLAY_ON UNITY_UI_ALPHACLIP;UNDERLAY_ON UNITY_UI_CLIP_RECT;OUTLINE_ON UNDERLAY_ON UNITY_UI_CLIP_RECT;UNDERLAY_ON UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT;OUTLINE_ON UNDERLAY_ON UNITY_UI_ALPHACLIP UNITY_UI_CLIP_RECT t012
6acf7701ebe7b54c0611f78b2ca4c522|vs|3860|vs_4_0|uncovered|hand|no:loop/switch flow control (loop)|D|Legacy Shaders/VertexLit[R/unity_builtin_extra#6] s0p0 Vertex kw=SPOT t012
886f88d918b35901eec27e7adce8be57|vs|5376|vs_4_0|IN-TABLE(l9=9928B)|mechanical||A|Skybox/Procedural[sharedassets29.assets#13] s0p0 - kw=_SUNDISK_NONE t012
880dca139a4ae1a1aa077bbde8f82390|vs|5640|vs_4_0|IN-TABLE(l9=10364B)|mechanical||A|Skybox/Procedural[sharedassets29.assets#13] s0p0 - kw=_SUNDISK_HIGH_QUALITY t012
fd7da3c6de3aa924209ff8c042d94124|vs|5660|vs_4_0|IN-TABLE(l9=10400B)|mechanical||A|Skybox/Procedural[sharedassets29.assets#13] s0p0 - kw=_SUNDISK_SIMPLE t012
```

### B. DXBC containers inside UnityPlayer.dll (file offset | checksum | size | SM | status | what | chunks) - 30 containers, all with RDEF

None has a size of 1408, 1508, 212 or 408 (nearest: VS 1504 at 0x266c8a0, a GPU-skinning VS). GameAssembly.dll and BadPiggiesRebooted.exe contain no DXBC.

```
0x26491d8|6906c16e81f484f2ed38639f9edde066|616|vs_5_0|uncovered|SM5 copy of the 2D blit VS|RDEF,ISGN,OSGN,SHEX,STAT
0x2649440|670252d1e786e5193732f7bf9a80f515|672|ps_5_0|uncovered|SM5 copy of the 2D blit PS|RDEF,ISGN,OSGN,SHEX,STAT
0x26496e0|7ea9b46bfc51276187cbca77a076833d|1324|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x2649c28|56ef3b8b686c63ccc089869570d76d82|1752|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264a300|992ad02098b35afba15cba51932e38b1|2176|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264ab80|eed5f13664a8ffa6a829a32e36e46927|1476|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264b160|bd4e1de5f8f1ba7861f07735df05f148|1904|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264b8d0|648de39202252aab42276ddea6e4479e|2328|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264c1e8|49589561b581010bbddcc80d3c433f0b|1700|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264c8a8|88ae99b92a499b696cee780fc0a0464a|2128|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264d0f8|25ad461786ea2e79621eb3c0fb4d78e5|2552|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264daf0|efbe1a43044af932c35e2465b4a4f166|1540|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264e118|fbb703053151e18c887ddf9fd2370fc5|1968|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x264e8c8|7714d2c4541f1c83e43c97a1f0b97245|2392|vs_5_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHEX,SFI0,STAT
0x266ba38|6e4f50260a72b3176ea53455f4954e4b|568|vs_4_0|IN-TABLE|internal 2D blit VS|RDEF,ISGN,OSGN,SHDR,STAT
0x266bc70|5daf5b7320d46e5d031dbcf082170dbe|600|ps_4_0|IN-TABLE|internal 2D blit PS|RDEF,ISGN,OSGN,SHDR,STAT
0x266bec8|be23d2ca6b3184ae758cb74f0c7b66e5|744|vs_4_0|uncovered|blit-to-texture-array VS (SV_InstanceID, SV_RenderTargetArrayIndex)|RDEF,ISGN,OSGN,SHDR,SFI0,STAT
0x266c1b0|b3c5f91d2b3481590b62b0df9bca05db|660|ps_4_0|uncovered|blit-to-texture-array PS (texture2darray)|RDEF,ISGN,OSGN,SHDR,STAT
0x266c448|6ed51d22bacc7dfa9f1f9241b93cae4d|1112|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266c8a0|6c7b910ac684d970cc80fe3774435c88|1504|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266ce80|ee619cc88eb5f01763131d006d9606fd|1892|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266d5e8|77f061683b7b977213d9122967d26791|1176|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266da80|504478589497b594c346c318048e74fc|1568|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266e0a0|531c24d6143e28a5399c340579ec70f0|1956|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266e848|0f1c407c6bc7a7b71a53b22a6421dfbd|1260|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266ed38|68456a49f284dbedff5c2cafe2c8ca6a|1652|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266f3b0|dd10bbf1019f0f80d868fdafb622913d|2040|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x266fba8|34ebdc0f2354e2db81f95d1d92db7178|1196|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x2670058|ebf22eef1297709cfc845b540aa69e51|1588|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
0x2670690|48774becf57998a8dc4d412adb701ad2|1976|vs_4_0|uncovered|GPU skinning VS (tbuffer texBones + ld, stream-out)|RDEF,ISGN,OSGN,SHDR,STAT
```

### C. cs_5_0 ComputeShader programs in Data/Resources/unity default resources (raw, uncompressed) - 15 unique, each present twice; cannot exist at feature level 9_1 (no compute), the game does not use compute

```
4a0bd292c933f0bc35f6a06c05ce7847|420|cs_5_0|file offsets 0x1b5e0,0x1bf9c
7608c1d7be07f1901ca76863712ab7af|584|cs_5_0|file offsets 0x1b844,0x1c200
3f06b36ec826dff7d32d3fb314af3ffa|768|cs_5_0|file offsets 0x1bb4c,0x1c508
4279bdb1b31bf1b6839bf1d8074551c4|872|cs_5_0|file offsets 0xe350,0x14ca4
8dd7856e1f1a49707629471fede7bf51|1068|cs_5_0|file offsets 0xe7c0,0x15114
094208d03b2671b25248245ff535f0fe|1088|cs_5_0|file offsets 0xecf4,0x15648
54ec5caf538fa1773f1f652b26714de1|1244|cs_5_0|file offsets 0xf23c,0x15b90
49763982fafdba5942df64400ed38629|1416|cs_5_0|file offsets 0xf820,0x16174
5af1c7fd873226ea77fd77d699a2996d|1780|cs_5_0|file offsets 0xfeb0,0x16804
fc864a997f85327b42684fcdb46478cc|1780|cs_5_0|file offsets 0x106ac,0x17000
081b3606458e1fa1af7ad9a10eb3c5f6|2124|cs_5_0|file offsets 0x10ea8,0x177fc
a9ac3ea81105c49484b42e0a4ffd2ddc|2488|cs_5_0|file offsets 0x117fc,0x18150
66090875370d36b0d4ec222d5d353f06|3092|cs_5_0|file offsets 0x122bc,0x18c10
03d94f36798e5eeafc72232f1b297331|3092|cs_5_0|file offsets 0x12fd8,0x1992c
c818853bb4ed4a3d7139f03703b48c9c|3676|cs_5_0|file offsets 0x13cf4,0x1a648
```

### D. The device log (AC/l9shim.log) resolved to checksums, in creation order

Every HIT resolves to a covered DXBC whose container size AND replacement size both match the log (this is the proof that the logged size = DXBC container size). Order = first use. The four MISS lines sit between the first `Hidden/Internal-GUITexture` pair and the first `Hidden/Internal-GUITextureClip` pair, i.e. during scene load, before any scene material is drawn. HIT PS#8 (212 -> 512) is `Hidden/InternalErrorShader` (checksum e7ece845...), a DIFFERENT 212-byte program from MISS PS#1 (2149...).

```
VS HIT #1|636->2048|bec3e12adf7e0d6ea3a5ea4d330fa1e8|Hidden/InternalClear
PS HIT #1|244->532|4edd109775107ca535a082e40c949ac9 / 593e80893489793fdf8191f9126555f5|_Custom/Utility_ZWrite, Hidden/Internal-Colored / Hidden/InternalClear
VS HIT #2|784->2388|83010061c3fb1757d2e1fc384afc8ec9|Hidden/Internal-Flare, Hidden/Internal-Halo, Hidden/Internal-GUITexture
PS HIT #2|396->832|05c5f3d1a3006f5775954658039b103c|Hidden/Internal-GUITexture
VS MISS #1|1408|7eda7bd3a50b1aea45191dd3db67f4af|Legacy Shaders/VertexLit
PS MISS #1|212|2149265952758e8e2aa16f30616fa85e|Legacy Shaders/VertexLit
VS MISS #2|1508|e8e7fc89f0811cee3a03ce9369229eaa|Particles/Standard Unlit
PS MISS #2|408|a8e80925af66290d2f3da26d61e3adc2|Particles/Standard Unlit
VS HIT #3|1076->3024|71aa6655ebbf1a00e965cb46162c4322|Hidden/Internal-GUITextureClip
PS HIT #3|792->1748|7be7c2a7795d58266d7c4f3539d6ee6f|Hidden/Internal-GUITextureClip
VS HIT #4|1088->3000|9c82ad4bfe195586520bf40e118ec35b|Hidden/Internal-GUITextureClipText
PS HIT #4|516->1076|04eb1e04c67e8b2ffdbec7ae7a664a19|Hidden/Internal-GUITextureClipText
VS HIT #5|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #5|348->968|f0bde3d56fe0f0cceffcb66b525c3952|e2d/Fill, Depth Mask/Unlit Transparent (CG), _Custom/PreAlpha_Unlit_ColorTransparent_GeometryZ, _Custom/Unlit_ColorTransparent_Geometry, _Custom/PreAlpha_Unlit_ColorTransparent_Geometry +2
VS HIT #6|852->2576|114c264921d354d0d03d5d80cd0e22db|TextMeshPro/Sprite, UI/Default
PS HIT #6|440->1088|a2096569ae5ddc30b43a012105d21e7d|TextMeshPro/Sprite, UI/Default
VS HIT #7|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #7|292->684|49cb703056b6baf7228c1691966a0730|Depth Mask/Unlit Transparent ZAlways, Unlit/Transparent, Hidden/BlitCopyDepth
VS HIT #8|532->1920|6d31dcd91ab28cc3e6bf576e54baee55|_Custom/Unlit_Monochrome, Hidden/InternalErrorShader, Hidden/Internal-StencilWrite
PS HIT #8|212->512|e7ece84581386f0c00a49f263b529d7c|Hidden/InternalErrorShader
VS HIT #9|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #9|340->768|a2457187a5f61cfd05753c4d98725964|Unlit/Texture
VS HIT #10|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #10|372->1004|12b2f29cba02eac82e5adf13f9284c38 / 52bcaff9b603e5b9d5da428e96295d22|_Custom/Unlit_Alpha8Bit_Color / _Custom/SilhouetteShader
VS HIT #11|532->1920|6d31dcd91ab28cc3e6bf576e54baee55|_Custom/Unlit_Monochrome, Hidden/InternalErrorShader, Hidden/Internal-StencilWrite
PS HIT #11|220->708|7647a3fc9745a3c31eaaeae1f8365f12|_Custom/Unlit_Monochrome
VS HIT #12|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #12|348->968|f0bde3d56fe0f0cceffcb66b525c3952|e2d/Fill, Depth Mask/Unlit Transparent (CG), _Custom/PreAlpha_Unlit_ColorTransparent_GeometryZ, _Custom/Unlit_ColorTransparent_Geometry, _Custom/PreAlpha_Unlit_ColorTransparent_Geometry +2
VS HIT #13|840->2520|2fb5c1201c23ff3e66645b3f7773c09e|e2d/Curve
PS HIT #13|580->1248|7abbd1bf412e11d2cfd2ebc5c5fd51a9|e2d/Curve
VS HIT #14|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #14|348->968|f0bde3d56fe0f0cceffcb66b525c3952|e2d/Fill, Depth Mask/Unlit Transparent (CG), _Custom/PreAlpha_Unlit_ColorTransparent_GeometryZ, _Custom/Unlit_ColorTransparent_Geometry, _Custom/PreAlpha_Unlit_ColorTransparent_Geometry +2
VS HIT #15|784->2432|316e59ff5b6457933a79e7dfdbbd6e3b|Spine/Skeleton
PS HIT #15|368->788|802266c90d74f987558ce4233950d2e5|Spine/Skeleton
VS HIT #16|796->2436|672d44e41ca1499544a51a1fe5b07b05|GUI/Text Shader
PS HIT #16|388->820|5e4ad9e3634b4dc9bcd9e1277856658a / f0403b994737c3a6b66059414bf7d8dc|_Custom/Text Shader With Z Test / GUI/Text Shader
VS HIT #17|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #17|348->968|f0bde3d56fe0f0cceffcb66b525c3952|e2d/Fill, Depth Mask/Unlit Transparent (CG), _Custom/PreAlpha_Unlit_ColorTransparent_GeometryZ, _Custom/Unlit_ColorTransparent_Geometry, _Custom/PreAlpha_Unlit_ColorTransparent_Geometry +2
VS HIT #18|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #18|348->968|f0bde3d56fe0f0cceffcb66b525c3952|e2d/Fill, Depth Mask/Unlit Transparent (CG), _Custom/PreAlpha_Unlit_ColorTransparent_GeometryZ, _Custom/Unlit_ColorTransparent_Geometry, _Custom/PreAlpha_Unlit_ColorTransparent_Geometry +2
VS HIT #19|532->1920|6d31dcd91ab28cc3e6bf576e54baee55|_Custom/Unlit_Monochrome, Hidden/InternalErrorShader, Hidden/Internal-StencilWrite
PS HIT #19|220->708|7647a3fc9745a3c31eaaeae1f8365f12|_Custom/Unlit_Monochrome
VS HIT #20|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #20|348->968|f0bde3d56fe0f0cceffcb66b525c3952|e2d/Fill, Depth Mask/Unlit Transparent (CG), _Custom/PreAlpha_Unlit_ColorTransparent_GeometryZ, _Custom/Unlit_ColorTransparent_Geometry, _Custom/PreAlpha_Unlit_ColorTransparent_Geometry +2
VS HIT #21|840->2520|2fb5c1201c23ff3e66645b3f7773c09e|e2d/Curve
PS HIT #21|580->1248|7abbd1bf412e11d2cfd2ebc5c5fd51a9|e2d/Curve
VS HIT #22|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #22|340->768|a2457187a5f61cfd05753c4d98725964|Unlit/Texture
VS HIT #23|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #23|372->1004|12b2f29cba02eac82e5adf13f9284c38 / 52bcaff9b603e5b9d5da428e96295d22|_Custom/Unlit_Alpha8Bit_Color / _Custom/SilhouetteShader
VS HIT #24|684->2264|36d14d52b758f8d4b8a5cc86eeef4266|Unlit/Texture, Depth Mask/Unlit Transparent ZAlways, _Custom/Unlit_Alpha8Bit_Color, e2d/Fill, Depth Mask/Unlit Transparent (CG) +14
PS HIT #24|388->1048|adbaeb3fd7af8c9e771807c6c6fdebd4|_Custom/PreAlpha_Unlit_ColorTransparent_Geometry_Gray
VS HIT #25|748->2428|3ddf4510a9f21d014f3d7f6d4b883e79|_Custom/Text Shader With Z Test
PS HIT #25|388->820|5e4ad9e3634b4dc9bcd9e1277856658a / f0403b994737c3a6b66059414bf7d8dc|_Custom/Text Shader With Z Test / GUI/Text Shader
```

