# 0.10.4.40 - frame loop, DeviceResources, D3D11 device / swap chain, shaders (FL 9_1)

Exe `MCPE.Host.WindowsPhone.exe` (ARM Thumb-2, base 0x400000). All numbers are RVAs unless written VA. Machine-readable twin: `frame_render.json` (31 facts, 27 high / 4 medium).
Scratch/probes: `arm32/v0_10_4/analysis/` (`shader_probe_0104.py`, `extra_files.py`, `include_test.py`, `swapchain_probe_0104.py`, `scan_strb3c.py`, `gen_frame_render_json.py`).
Labels were stored with `arm32re.py name` (DX::DeviceResources::*, MCPE_Host::AppMain::*, MCPE_Host::MainPage handlers, shader:* ...).

## 0. Bottom line

* "DisplayResources" is the stock Microsoft WP8.1 XAML template class **DX::DeviceResources**. The whole graphics core is the template plus the game: nothing in it needs a CoreWindow. It needs a **SwapChainPanel** (QI'd for 5 interfaces), a **CoreDispatcher.RunAsync**, **DisplayInformation** values, **ThreadPool.RunAsync** and a **DispatcherTimer**.
* There is no `AppMain::runFrame/tick/render` and no `CompositionTarget.Rendering`. The frame loop is a **High-priority ThreadPool work item** (`0x1da160`) started by `AppMain::StartRenderLoop` (`0x1da8a8`): `lock; platform update; Minecraft::update; DeviceResources::Present(0,0)`.
* **The game asks for D3D feature levels {11_1,11_0,10_1,10_0,9_3} only** and ignores the HRESULT. On a 9_1-only GPU `D3D11CreateDevice` fails and the very next QI dereferences NULL. A host hook (IAT `0x20b1a4`) is mandatory.
* Shader code throws `"Unkonwn feature level"` for 0x9100/0x9200 and requests `vs/ps_4_0_level_9_3`. **All 76 shader variants compile at `_level_9_1`** (0 failures) and load on a {9_1} device. Recipe: report 0x9300 to the game, change one byte of the profile string (`_level_9_3` -> `_level_9_1`).
* **Likely cause of the friend's all-white window** (derived from the binary, not from their code): the swap chain / RTV / DSV / viewport are created *only* inside the SizeChanged handler (`0x1df808` -> `SetSwapChainPanel` `0x1dbd40`), and the render loop / `AttachToPanel` only start from the 50 ms DispatcherTimer Tick (`0x1df8a4`) after the init work item set `AppMain+0x3c`. If the host never raises SizeChanged, never runs the Tick path, or never runs `App::init` (`0x471e8`), nothing is drawn and `MinecraftClient+0x118/+0x11c` (textures/font, created by `App::init`) stay NULL - exactly the StartMenuScreen crash they report.

## 1. Object / structure map

### 1.1 DX::DeviceResources (size 0x78, lives at +0xc of `std::_Ref_count_obj` 0x84 bytes; vtable of the control block 0xdc8e64)
| off | content | evidence |
|---|---|---|
| +0x00 | ID3DUserDefinedAnnotation* | QI of ctx, IID b2daad8b (0x2df218), `0x1db0b0` |
| +0x04 | ID3D11Device2* | QI IID 9d06dffa (0x2df258) `0x1db03c-0x1db04e` |
| +0x08 | ID3D11DeviceContext2* | QI IID 420d5b32 (0x2df268) `0x1db074` |
| +0x0c | IDXGISwapChain1* | `[sp]=this+0xc` for CreateSwapChainForComposition `0x1db3ee` |
| +0x10 / +0x14 | RTV / DSV | `0x1db5f8-0x1db608`, `0x1db6e6-0x1db702` |
| +0x18..0x2f | D3D11_VIEWPORT | built `0x1db72a-0x1db758`, RSSetViewports |
| +0x30 | SwapChainPanel^ | only written by SetSwapChainPanel `0x1dbd98`; `0x1db178` returns if NULL |
| +0x34 / +0x35 | byte override-active / byte device-profile flag (default 1) | `0x1dae4c`, `0x1dc028` |
| +0x38 | D3D_FEATURE_LEVEL (ctor default 0x9100; D3D11CreateDevice out-param) | `0x1dae54`, `0x1db00a` |
| +0x3c/+0x40, +0x44/+0x48, +0x4c/+0x50 | float d3dRenderTargetSize, outputSize, logicalSize (W,H) | 0x1db1c4.., 0x1dbe1c |
| +0x54 / +0x58 | native / current orientation | SetSwapChainPanel `0x1dbe5e`, `0x1dbe78` |
| +0x5c | float dpi (init -1.0) | `0x1dae62`, `0x1dbf3c` |
| +0x60 / +0x64 | float compositionScaleX / Y (init 1.0) | `0x1dae72` |
| +0x68 | -> device-profile row (see 2.3) | `0x1db9f0/0x1dba10` |
| +0x6c / +0x70 | int longer / shorter side of output-0 desktop rect | `0x1db9ce/0x1db9de` |
| +0x74 | IDeviceNotify* (= AppMain; vtable[0]=OnDeviceLost, [1]=OnDeviceRestored) | `RegisterDeviceNotify 0x1dbbc0` |

Global: `g_DeviceResources` at **RVA 0xe2afc4**, set in ctor body `0x1dae14`, read through getter **`0x4b054`** (20 game call sites; throws std::runtime_error if NULL). Shader code reads `+0x38` through it.

### 1.2 Functions (all labelled in the toolkit)
| RVA | role |
|---|---|
| 0x1dce2c | `std::make_shared<DeviceResources>` helper (operator new 0x84 -> ctor wrapper 0x1dc5a4 -> ctor body 0x1dae14 -> CreateDeviceResources). Friend's `0x1DCE2C` is correct. |
| 0x1dafc0 | CreateDeviceResources (D3D11CreateDevice at 0x1db028, QIs, then 0x1db858 device profile) |
| 0x1db158 | CreateWindowSizeDependentResources (resize/create swap chain, RTV, DSV, viewport) |
| 0x1dbab8 / inline in 0x1dbb10 | HandleDeviceLost |
| 0x1dbb10 | Present |
| 0x1dbbc0 RegisterDeviceNotify, 0x1dbbcc SetCompositionScale, 0x1dbc1c SetCurrentOrientation, 0x1dbc38 SetViewportDepth, 0x1dbc5c SetDpi, 0x1dbc80 SetLogicalSize, 0x1dbcd0 BindRenderTarget, 0x1dbd40 SetSwapChainPanel, 0x1dbf80 SetViewportSize, 0x1dbfbc ApplyResolutionOverride, 0x1dc054 Trim, 0x1dc0b4 ValidateDevice, 0x1dc3fc get_Dispatcher helper, 0x1daf04 SetSwapChain lambda | |
| 0x1d9d14 | AppMain ctor (vtable 0xdc8830; +4 DR*, +8 ctrl, +0xc render-loop IAsyncAction, +0x10 critical_section, +0x34 App*, +0x38 platform cell, +0x3c init flag) |
| 0x1d9dd4 | AppMain::createApp: new AppPlatform_Winrt (0x88, ctor 0x494c0), new MinecraftClient (0x168, via 0x1d9cc0 -> ctor 0x54e90), queue init work item (lambda 0x1d9b68 / 0x1da4a4) |
| 0x1da160 | render-loop body; 0x1da8a8 StartRenderLoop; 0x1da9d4 StopRenderLoop; 0x1da310 IsInitialised; 0x1da32c OnWindowSizeChanged; 0x1da634 AttachToPanel |
| 0x1d8a54 | Minecraft::update (MinecraftClient vtable 0x33ffcc slot 19) |
| 0x1dcf5c | MainPage ctor; handlers 0x1df808 SizeChanged, 0x1df510 CompositionScaleChanged, 0x1df67c DpiChanged, 0x1df76c OrientationChanged, 0x1df618 DisplayContentsInvalidated, 0x1df9ac VisibilityChanged, 0x1df8a4 timer Tick |

## 2. Ordered sequences

### 2.1 Start-up (UI thread, MainPage ctor 0x1dcf5c)
1. `InitializeComponent` = `bl 0x1eddd8` at 0x1dd02a (loads MainPage.xbf; XAML names: `swapChainPanel`, `Logo`, `EditBox`, `Button` -> MainPage +0x5c/+0x60/+0x64/+0x68).
2. `Window::Current` (0x1dac80), CoreWindow (0x1daacc) -> add VisibilityChanged (0x1df9ac); `DisplayInformation::GetForCurrentView` (0x4af58) -> add DpiChanged (0x1df67c, slot 0x38), OrientationChanged (0x1df76c, slot 0x20); static DisplayContentsInvalidated (0x1df618); panel QI ISwapChainPanel -> add_CompositionScaleChanged (0x1df510); panel QI IFrameworkElement -> add_SizeChanged (0x1df808); InputPane Showing/Hiding (0x1df700/0x1df714).
3. `make_shared<DeviceResources>` (0x1dd39a) -> **device only**; no swap chain yet (panel is NULL).
4. `new AppMain(shared_ptr)` (0x1dd4f8): creates AppPlatform_Winrt + MinecraftClient, **queues the init work item** (calls `App::init` 0x471e8, then sets AppMain+0x3c=1), `DR->RegisterDeviceNotify(AppMain)`.
5. Input worker work item (0x1de0bc: `panel.CreateCoreIndependentInputSource(7)` + pointer events + ProcessEvents) stored in MainPage+0x6c (other area).
6. DispatcherTimer (0x1dce94; Interval 500000 ticks = 50 ms, Tick 0x1df8a4, Start).

### 2.2 First layout / init completion
7. XAML raises **SizeChanged** -> `0x1df808`: lock AppMain+0x10 -> `SetSwapChainPanel(MainPage+0x5c)` (ActualWidth/Height, orientations, composition scale, dpi, resolution override) -> `CreateWindowSizeDependentResources` (swap chain, attach lambda, RTV, DSV, viewport) -> `SetLogicalSize(NewSize)` -> `AppMain::OnWindowSizeChanged` (0x1da32c).
8. Tick (every 50 ms until AppMain+0x3c != 0): Stop timer, collapse Logo, `AttachToPanel` (0x1da634 -> AppPlatform_Winrt::initialize 0x4b55c), `StartRenderLoop`.

### 2.3 Per frame (render thread, `0x1da160`)
```
while (IAsyncAction.Status == Started /*0*/) {
    critical_section::scoped_lock lock(AppMain+0x10);          // IAT 0x20b424 / 0x20b428
    if (*(byte*)(AppMain+0x3c)) {
        sub_50eac(*(void**)*(AppMain+0x38));                    // AppPlatform_Winrt::update (drain task queue)
        App* a = *(App**)(AppMain+0x34); a->vtable[19](a);      // Minecraft::update 0x1d8a54
        DeviceResources::Present(*(AppMain+4));                 // 0x1dbb10 -> IDXGISwapChain::Present(0,0)
    }
}
```
`Minecraft::update` (0x1d8a54): vf28(+0x70), Timer::advanceTime(this+0xa8 -> 0x65f10), N x vf31(+0x7c)(i,N-1) (tick), `0x11b07c` time, vf29(+0x74) = **render** (MinecraftClient::vf29 = RVA 0x5a114: GameRenderer begin 0xb9e1c sets viewport 0x1dbf80 and binds the back buffer 0x1dbcd0), FPS 0x1198b8, vf30(+0x78). Vtable cells hold **VA|1**.
Present is `Present(SyncInterval=0, Flags=0)` (slot 8, not Present1); throttling comes from `IDXGIDevice1::SetMaximumFrameLatency(1)` + flip model.

## 3. D3D11 device
`D3D11CreateDevice` (only site 0x1db028, IAT 0x20b1a4, thunk 0x1f433c): adapter NULL, **DriverType 1 (HARDWARE)**, Flags **0**, levels (stack, built from immediates @0x1dafda/e0/e6/ec/f2) `{0xb100,0xb000,0xa100,0xa000,0x9300}`, count 5 (`movs r3,#5` @0x1db016), SDK 7, out FL -> DR+0x38. HRESULT unchecked.
QI chain afterwards (all Win8.1/RT 8.1 capable; replayed OK on a forced {9_1} device): ID3D11Device2, ID3D11DeviceContext2, ID3DUserDefinedAnnotation, IDXGIDevice3.
**Host hook** (IAT cell, RW per PORT_RULES #4): force `FeatureLevels={0x9100}`; after success write **0x9300** to `*pFeatureLevel` (see 5). BGRA flag not needed.

## 4. Swap chain, panel replacement, resize
`CreateSwapChainForComposition(dev, &desc, NULL, &DR+0xc)` via IDXGIFactory2 (device -> IDXGIDevice3 -> GetAdapter -> GetParent(IDXGIFactory2)); call = `ldr r4,[r4,#0x60]` @0x1db3fe + `blx r4` @0x1db400.
DXGI_SWAP_CHAIN_DESC1: W/H = lroundf(render-target size), `B8G8R8A8_UNORM`(0x57), no stereo, 1 sample, usage RENDER_TARGET_OUTPUT(0x20), **2 buffers, SCALING_STRETCH(0), SWAP_EFFECT_FLIP_SEQUENTIAL(3), ALPHA_MODE_IGNORE(3)**, flags 0. If a chain already exists: `ResizeBuffers(2,W,H,0x57,0)` after releasing RTV/DSV and `Flush`.
After creation: `SetMaximumFrameLatency(1)`; panel.Dispatcher.RunAsync(High, lambda 0x1daf04) -> `ISwapChainPanelNative::SetSwapChain(DR+0xc)`; QI IDXGISwapChain2 -> **`SetMatrixTransform({1/scaleX,0,0,1/scaleY})` @0x1db57c (vtable +0x88 = slot 34)**; GetBuffer(0) -> RTV; **D24_UNORM_S8_UINT** depth W x H -> DSV; viewport {0,0,W,H,0,1}; `OMSetRenderTargets(1,&RTV,DSV)`.

What the host must give instead of the SwapChainPanel (DR+0x30 == MainPage+0x5c), all ref-counted COM objects answering QI:
* IFrameworkElement (a391d09b-4a99-4b7c-9d8d-6fa5d01f6fbf): `ActualWidth`/`ActualHeight` (doubles; client size in DIPs), `add_SizeChanged` - **and the host must raise SizeChanged(NewSize) after construction and on every resize** (SizeChangedEventArgs.get_NewSize is slot 7).
* ISwapChainPanel (c589644f-eba8-427a-b75a-9f1f93a11ae9): CompositionScaleX/Y = 1.0, add_CompositionScaleChanged, CreateCoreIndependentInputSource (input area).
* IDependencyObject (5c526665-f60e-4912-af59-5fe0680f089d): get_Dispatcher (slot 11) -> fake CoreDispatcher whose RunAsync(priority, DispatchedHandler) can invoke inline.
* IUIElement (676d0be9-...): Visibility setter. ISwapChainPanelNative (f92f19d2-3ade-45a6-a20c-f6f1ea90554b): SetSwapChain.
* DisplayInformation: CurrentOrientation = NativeOrientation = **Landscape (1)** (orientation byte table 0x1db23c => no W/H swap), LogicalDpi **96**; never raise DisplayContentsInvalidated.

Two ways to present (probe on a {9_1} device, see 7):
* **A (preferred, zero code patches):** host HWND + DirectComposition (`DCompositionCreateDevice`, target for HWND, visual); fake `SetSwapChain(sc)` = `visual->SetContent(sc); target->SetRoot(visual); Commit()`. CreateSwapChainForComposition and SetMatrixTransform run unmodified (both S_OK in the probe). Needs dcomp.dll on the Surface RT (shipped with Windows 8/RT - unverified here).
* **B (fallback):** HWND swap chain. Patch data cells in dxgi.dll vtables: IDXGIFactory2 slot 24 (+0x60) -> stub calling original slot 15 `CreateSwapChainForHwnd(dev,hwnd,desc,NULL,NULL,pp)`; IDXGISwapChain2 slot 34 (+0x88) -> `return S_OK` (HWND chains return 0x887A0001 in the probe). Code-patch alternative: `98 47` at 0x1db57c -> `00 20`. Flip+STRETCH was accepted on Win11; unverified on Win8.1.

Size math (template): output = logical x compositionScale (>=1); swap-chain/depth size = lroundf(output) (no dpi factor). **Resolution override:** `0x1db858` picks a GPU-name row from the table at 0xdc8850 (stride 0x18 for row 0, then 0x14: {wchar* name, float scale, minW, minH, byte flag}): Default 0.5 @1280x720, Adreno 225/304/305/306 0.667, 302 0.5, 320/330 0.667 @1920x1080, 405/418/420/430 1.0. On a desktop adapter the Default row applies, so a >=1280x720 screen renders at **half size** and is stretched by SetMatrixTransform(2x) / DXGI STRETCH. Optional host patch for full resolution: float at **0xdc8854** `00 00 00 3f` -> `00 00 80 3f`.

Device loss: Present/ResizeBuffers compare 0x887a0005/0x887a0007 -> HandleDeviceLost (recreate device + swap chain). AppMain's IDeviceNotify vtable (0xdc8830) is two copies of an empty function (0xbf60): the game does **not** rebuild its GPU resources, so a device loss means restart. ValidateDevice 0x1dc0b4 only runs from DisplayContentsInvalidated. Do not simulate loss.

## 5. Shader pipeline
* One D3DCompile site (0xd6c94, ShaderProgram::ShaderProgram 0xd6a3c, IAT 0x20b000) and one D3DReflect site (0xd0220, Shader::Shader 0xd01b8, IID_ID3D11ShaderReflection @0x2ecf90; IAT 0x20b004). `D3DCompile(src, len, "data/"+path, NULL, (ID3DInclude*)1, "main", profile, 0x8000 /*OPT3*/, 0, ...)`. Errors thrown with prefix "HLSL Compiler error: ".
* Sources are **files**: `data/shaders/dx11/<name>.hlsl` (24 files + `ShaderConstants.fxh`), chosen by `data/materials/{common,fancy,sad}.json` -> `*.material` (vertexShader / fragmentShader / defines). Source text = `#define X\n` per define (sorted) + file; `R8G8B8A8_SNORM_UNSUPPORTED` is added when FL < 0xa000. No cache on disk; programs are cached in memory.
* Include handling: `D3D_COMPILE_STANDARD_FILE_INCLUDE` resolves `ShaderConstants.fxh` relative to **cwd + dirname(pSourceName)** (verified: fails with X1507 from another cwd) -> host must `SetCurrentDirectory(game root)` or hook D3DCompile and pass an ID3DInclude.
* Profile = `vs|ps` (@0x235244/0x235248) + `_` + major (11_x->5 else 4) + `_` + minor (10_1->1 else 0) + suffix (`_level_9_3` @**0x2351f0** for 0x9300, "" for 10_0/10_1/11_x); 0x9100/0x9200 throw `Unkonwn feature level`. => **The game requests `vs_4_0_level_9_3` / `ps_4_0_level_9_3`.** To run on 9_1: lie 0x9300 from the D3D11CreateDevice hook and flip the byte at **RVA 0x2351f9** (`33` -> `31`, verify `_level_9_3\0` first) - or hook D3DCompile and rewrite pTarget. All other FL consumers (0xcb1d6, 0xd8e6a, 0xd116e) only test `< 0xa000`, identical for 9_1 and 9_3.

### Shader feasibility table (D3DCompile 47, flags 0x8000, game-built defines; `shader_probe_0104.py`)
72 material variants -> 76 unique (shader, stage, defines) combos; 18 shader files referenced.
| file | stage | combos | 9_1 | 9_3 | 10_0 | max SM4 instr @9_1 |
|---|---|---|---|---|---|---|
| cloud.vertex | vs | 1 | 1/1 | 1/1 | 1/1 | 13 |
| color.vertex | vs | 1 | 1/1 | 1/1 | 1/1 | 6 |
| color_uv.vertex | vs | 2 | 2/2 | 2/2 | 2/2 | 7 |
| entity.vertex | vs | 14 | 14/14 | 14/14 | 14/14 | 40 |
| position.vertex | vs | 1 | 1/1 | 1/1 | 1/1 | 5 |
| renderchunk.vertex | vs | 15 | 15/15 | 15/15 | 15/15 | 35 |
| sky.vertex | vs | 1 | 1/1 | 1/1 | 1/1 | 7 |
| uv.vertex | vs | 2 | 2/2 | 2/2 | 2/2 | 6 |
| color.fragment | ps | 1 | 1/1 | 1/1 | 1/1 | 2 |
| color_ex.fragment | ps | 1 | 1/1 | 1/1 | 1/1 | 2 |
| color_texture.fragment | ps | 2 | 2/2 | 2/2 | 2/2 | 5 |
| current_color.fragment | ps | 1 | 1/1 | 1/1 | 1/1 | 2 |
| entity.fragment | ps | 14 | 14/14 | 14/14 | 14/14 | 15 |
| flat_white.fragment | ps | 1 | 1/1 | 1/1 | 1/1 | 2 |
| renderchunk.fragment | ps | 15 | 15/15 | 15/15 | 15/15 | 10 |
| texture.fragment | ps | 1 | 1/1 | 1/1 | 1/1 | 2 |
| texture_ccolor.fragment | ps | 2 | 2/2 | 2/2 | 2/2 | 5 |
| texture_cutout.fragment | ps | 1 | 1/1 | 1/1 | 1/1 | 5 |

The 6 unreferenced files (color_ex.vertex, color_texture.vertex, iteminhand.fragment, normal_as_color.vertex, uv1.vertex, uv_as_color.fragment) also compile at 9_1 (3 define sets each). No instruction-slot, sampler or interpolator error anywhere. On a real {9_1} device (hardware and WARP of the probe PC) `CreateVertexShader/CreatePixelShader` succeeded for 76/76 level_9_1 blobs (and also for the level_9_3 blobs - not guaranteed on the Tegra driver, hence compile to 9_1).

## 6. Patch / hook table (host)
| what | where | action | expected bytes |
|---|---|---|---|
| D3D11CreateDevice | IAT cell 0x20b1a4 | wrapper: levels={0x9100}; on success `*pFL=0x9300` | - |
| profile suffix | byte at 0x2351f9 | `0x33 -> 0x31` (or hook IAT 0x20b000) | `5f 6c 65 76 65 6c 5f 39 5f 33 00` @0x2351f0 |
| SetMatrixTransform (HWND path only) | 0x1db57c (or dxgi swapchain vtable cell 34) | `98 47` -> `00 20` | `98 47 00 28` |
| CreateSwapChainForComposition (HWND path only) | dxgi IDXGIFactory2 vtable cell 24 (or code 0x1db3fe/0x1db400) | stub -> CreateSwapChainForHwnd | `24 6e a0 47` |
| full-res override (optional) | float 0xdc8854 | 0.5f -> 1.0f | `00 00 00 3f` |
| InitializeComponent (other area) | `bl 0x1eddd8` at 0x1dd02a | skip/fake; then host must set MainPage+0x5c/0x60/0x64/0x68 | - |
| cwd | process | `SetCurrentDirectory(game root)` | - |
| ThreadPool/Dispatcher/DispatcherTimer fakes | WinRT surface | see 4 / 2 | - |

## 7. Probe results (this PC, x64, forced `{9_1}` device; hardware and WARP identical)
`swapchain_probe_0104.py`: D3D11CreateDevice (flags 0) ok; QI ID3D11Device2 / DeviceContext2 / ID3DUserDefinedAnnotation / IDXGIDevice3 ok; SetMaximumFrameLatency(1) ok; GetAdapter -> GetParent(IDXGIFactory2) ok; **CreateSwapChainForComposition with the game's exact desc ok; QI IDXGISwapChain2 ok; SetMatrixTransform S_OK; GetBuffer + CreateRenderTargetView ok; Present(0,0) ok even with no visual**; HWND chains (STRETCH+FLIP_SEQUENTIAL, NONE+FLIP, STRETCH+DISCARD) create and present fine but **SetMatrixTransform = 0x887A0001**; D24S8 DSV 1366x768 ok.

## 8. Not found / uncertain
* IFrameworkElement getter slots (0x34 ActualWidth via helper 0x1dc504, 0x38 ActualHeight via 0x1dc490) and ISwapChainPanel slots (6/7/8/10) are inferred from call shapes; confirm against the real WinMD. `IDependencyObject` slot 11 for the Dispatcher likewise.
* Real Surface RT behaviour untested: dcomp availability, flip+STRETCH on an HWND, Tegra driver acceptance of level_9_1 blobs, 2048 texture limit (probe PC created a 4096 texture on the {9_1} device, so it proves nothing), 1 render target, occlusion queries/anisotropy/depth-clip on 9_1 (game was written for 9_3).
* pSourceName is inferred as `"data/" + shaderPath`; only the directory is corroborated (include file sits beside the .hlsl files).
* Not decoded: AppPlatform_Winrt::initialize (0x4b55c), InputPane handlers, Canvas/EditBox helper 0x1dfab8, who else writes MainPage+0x70; ValidateDevice only structurally.
* The friend's note "WP8.1-only call at 0x1DB56C": location correct (call sequence starts at 0x1db56c, the `blx` is 0x1db57c, vtable +0x88 = IDXGISwapChain2::SetMatrixTransform) but it is not WP8.1-only; it fails only on non-composition (HWND) swap chains.
