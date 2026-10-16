# gfx_compat (D3D11 / DXGI / HLSL compatibility hooks) - ARM32 1.2.8

Machine-readable: `gfx_compat.json` (22 facts: 19 high, 3 medium).

## ARM IAT cells (by import name)
| import | ARM IAT RVA | x86 RVA_128 | notes |
|---|---|---|---|
| D3DCOMPILER_47!D3DCompile | **0x00C370B4** | 0x011d20b4 | single call site 0x5BE4BA (compileShaderProgram) |
| d3d11!D3D11CreateDevice | **0x00C37A50** | 0x011d2a44 | calls at 0x5BC3A0 / 0x5BC3EC in GameOwner::InitDevice 0x5BC258 |
| D3DCOMPILER_47!D3DPreprocess | 0x00C370B0 | (unhooked) | called first, with the game's include object |
| D3DCOMPILER_47!D3DReflect | 0x00C370B8 | (unhooked) | caller 0x5B8EDC |

No dxgi.dll / CreateDirect3D11DeviceFromDXGIDevice / CreateDXGIFactory2 imports exist in the ARM exe.

## IID constants in .rdata
| x86 constant | meaning | ARM RVA | game users |
|---|---|---|---|
| RVA_128_DXGI_DEVICE3_IID 0x012d8b28 | IID_IDXGIDevice3 | 0x00DBC2C4 | 0x5BC258 InitDevice, 0x5B9F4C queryAdapterInfo, 0x5BB774 createWindowSizeDependentResources, 0x5BB928 trim (IDXGIDevice3::Trim), 0x5BEAAC swap-chain desc builder |
| RVA_128_D3D_CONTEXT_IID 0x012d8c7c | IID_ID3D11DeviceContext2 | 0x00DBC3FC | 0x5BC258 -> renderer+0x114; consumed by DiscardWindowViews 0x582970 |
| RVA_128_D3D_ANNOTATION_IID 0x012d8c4c | IID_ID3DUserDefinedAnnotation | 0x00DBC3DC | 0x5BC258 -> renderer+0x140 (no consumer found) |
| (extra) | IID_IDXGIAdapter3 | 0x00DBC2A4 | 0x5B9F4C, 0x5BC258 (null-checked, Win10 only) |
| (extra) | IID_IDXGIFactory2 | 0x00DBC5AC | 0x5BEAAC |

No ID3D11Device1/2/3 or DeviceContext1/3 GUID exists in the exe. On x86 1.2.8 the CONTEXT/ANNOTATION constants are defined but never used by any patch (the Device2/Context2 patch is `!host_is_128` only).

## Per-patch verdict
| x86 item | what it writes | ARM verdict |
|---|---|---|
| patch_win7_dxgi_device3_query_to_base | 16 bytes: IDXGIDevice3 IID -> IDXGIDevice IID (data) | **DROP** - Win7 only; DXGI 1.3 exists on RT 8.1/Win10 ARM. Would be 16 bytes at 0x00DBC2C4. |
| patch_win7_device2_queries_to_base | IID data, not even applied for 1.2.8 | **DROP** |
| patch_win7_discard_views | first byte of DiscardWindowViews -> 0xc3 | **DROP** (ARM fn 0x00582970; ctx2 exists on 8.1+) |
| install_d3d11_device_compat | IAT hook: flags|=0x20, Win7 11_1 filter, WARP retry, captured base device | **Reduce** to `flags |= 0x20` (optional). Win7 filter, WARP retry (game retries with WARP itself at 0x5BC3EC), captured_base_* all dropped. |
| install_d3d_compile_trace + win32_d3d_compile | IAT hook, include proxy, text patches, error log | **Reduce** to forwarder + failure logging. Proxy / LOD patch kept out of first build. |
| patch_hlsl_lod_intrinsic | text: `tex.CalculateLevelOfDetailUnclamped(TextureSampler0, uv)` -> `(0.0f)` | Literal exists once (data/shaders/hlsl/util.fxh:8) but very probably unnecessary (same system D3DCompiler_47 as the UWP app; call is under `VERSION >= 0xa000`). Re-add only if a compile failure mentions it. |
| patch_hlsl_root_signature | text `#define ROOT_SIGNATURE [RootSignature(...)]` | **DROP** - string absent in 1.2.8 data. |
| win32_include_open/close, win32_include, shader_include_directory | handles D3D_COMPILE_STANDARD_FILE_INCLUDE | **DROP** - ARM game never passes 1/NULL. |
| install_scissor_fix / win32_set_scissor_rects | d3d11 context vtable[45] data patch | **KEEP unchanged** (arch independent). |
| install_draw_trace + win32_draw* stubs | context vtable[9..53] | **DROP** - dead code (no caller). |
| host_is_windows7 / GetVersion | gates all of the above | hard-wire FALSE |

## Shader compile path (does the host hook behave the same?)
`impcallers D3DCompile` -> only `mce::ShaderProgramDX11::compileShaderProgram` (0x005BDECC), reached from 0x005BD3D0 (3 stages) and 0x004A1DF0.
It first calls D3DPreprocess (cache key only), then D3DCompile with: pSrcData=std::string at this+0x58, pSourceName=non-null string, pDefines=`{{"VERSION","<decimal feature level>"},{0,0}}`, pInclude=**stack object with vptr HeaderInclude 0x0136B164** (custom include, not NULL, not 1), entry "main", target `<vs|ps|gs>_<4|5>_<0|1>[_level_9_x]` (9_x -> 4_0_level_9_x, 10_0 -> 4_0, 10_1 -> 4_1, 11_x -> 5_0), flags1=0x8000, flags2=0. So the x86 hook's custom-include/proxy branch is the one that would run; the ID3DInclude vtable is identical (slot0 Open at 0x5BB180, slot1 Close = no-op stub 0x8B980), so the proxy source is ARM-compatible if ever re-enabled. Macros/flags pass through unchanged.

## Game struct offsets touched by this area
Only `renderer+0x110` (base ID3D11DeviceContext, passed to install_scissor_fix) - **same on ARM**. Also confirmed identical: renderer+0x114 (Context2), +0x118 RTV, +0x120 DSV, +0x140 annotation, owner+0xC0 device, owner+0xB8 feature level, owner+0xAC renderer, renderer+0x108 owner back-pointer. Compile path needs owner global **0x015B86AC** populated (VERSION macro: G->+0xAC->+0x108->+0xB8). Scissor math falls back to the screen-size object at 0x015B86B0 (+0x14 W, +0x18 H floats).

## Scissor
Single RSSetScissorRects call site: `gfx:Renderer::setScissorRect` 0x005BB560 (vtbl+0xB4 = slot 45, NumRects=1, RECT built from normalized floats: top=H*(1-y-h), bottom=H*(1-y)); applied from 0x005C2510. Same Y-flip logic the x86 comment describes, so keep the clamp hook (idempotent for sane rects).

## Which hooks the ARM port can drop
Drop: all `patch_win7_*`, Win7 FL filter, WARP retry, captured base device/context, root-signature patch, standard-file-include branch, draw trace, `host_is_windows7`.
Keep: scissor hook (as is), a thin D3D11CreateDevice hook (`flags | 0x20`, optional), a thin D3DCompile forwarder with error logging (optional diagnostic). LOD patch + include proxy: keep out until proven necessary.

Open items: see `open_questions` in the JSON (LOD patch necessity on device, scissor offset reproduction, annotation consumer, shader-cache file I/O).
