"""Generates facts/frame_render.json (expected_bytes are read from the exe so they cannot drift)."""
import sys, json, os
sys.path.insert(0, r'D:\repo\Win32Craft-main\tools\arm32')
os.environ.setdefault('ARM32_EXE', r'D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]\MCPE.Host.WindowsPhone.exe')
import arm32re
pe = arm32re.Pe()

def eb(rva, n=8):
    return pe.bytes(rva, n).hex()

F = []
def fact(id, kind, rva, summary, evidence, conf, sig=None, notes=None, ebytes=None, x86sym='n/a (0.10.4 WP8.1 XAML app)', x86val=None):
    d = {"id": "frame_render." + id, "x86_symbol": x86sym, "x86_value": x86val, "kind": kind,
         "arm_rva": ("0x%08x" % rva) if isinstance(rva, int) else rva, "summary": summary, "arm_signature": sig,
         "x86_to_arm_notes": notes, "expected_bytes": (eb(rva, ebytes) if (ebytes and isinstance(rva, int)) else None),
         "evidence": evidence, "confidence": conf}
    F.append(d)

# ---------------------------------------------------------------- classes / globals
fact("class_names", "data", None,
     "The task's 'DisplayResources' is the stock Microsoft template class DX::DeviceResources (RTTI .?AV?$_Ref_count_obj@VDeviceResources@DX@@@std@@ vtable 0xdc8e64). "
     "AppMain = MCPE_Host::AppMain (ctor 0x1d9d14), page = MCPE_Host::MainPage (ctor 0x1dcf5c). There is NO AppMain::runFrame/tick/render: the frame is Minecraft::update (0x1d8a54) + DeviceResources::Present (0x1dbb10) inside a ThreadPool work item (0x1da160).",
     ["rtti DeviceResources -> .?AV?$_Ref_count_obj@VDeviceResources@DX@@@std@@ vtable rva=00dc8e64",
      "make_shared helper 0x1dce2c allocates 0x84 = 0xc (ref-count header) + 0x78 (DeviceResources)",
      "rtti AppMain / MainPage -> vtables 0xdc8830 / 0xdc8af8"], "high")
fact("device_resources.layout", "data", None,
     "DX::DeviceResources instance layout (size 0x78, lives at +0xc inside the std::_Ref_count_obj block). All offsets re-derived from CreateDeviceResources 0x1dafc0, CreateWindowSizeDependentResources 0x1db158, SetSwapChainPanel 0x1dbd40, ctor 0x1dae14.",
     ["+0x00 ID3DUserDefinedAnnotation* (QI of the immediate context, IID b2daad8b-..., 0x1db08e..0x1db0e0 stores to this+0)",
      "+0x04 ID3D11Device2* (QI IID 9d06dffa-d1e5-4d07-83a8-1bb123f2f841 @rdata 0x2df258, 0x1db04a)",
      "+0x08 ID3D11DeviceContext2* (QI IID 420d5b32-b90c-4da4-bef0-359f6a24a83a @0x2df268, 0x1db08e)",
      "+0x0c IDXGISwapChain1* (0x1db3d8 release old, 0x1db3ee [sp]=this+0xc is ppSwapChain of CreateSwapChainForComposition)",
      "+0x10 ID3D11RenderTargetView* (0x1db5f8..0x1db608 CreateRenderTargetView(...,&this+0x10)); +0x14 ID3D11DepthStencilView* (0x1db6f0..0x1db702)",
      "+0x18..+0x2f D3D11_VIEWPORT {TopLeftX,TopLeftY,Width,Height,MinDepth,MaxDepth} (0x1db72a..0x1db758 builds and copies it; RSSetViewports(1,&this+0x18) = ctx slot 44 / 0xb0)",
      "+0x30 SwapChainPanel^ (IInspectable*, set only by SetSwapChainPanel 0x1dbd98); CreateWindowSizeDependentResources returns immediately while it is NULL (0x1db178 cmp [r6,#0x30])",
      "+0x34 byte 'resolution override active' (0x1dc028/0x1dc038), +0x35 byte device-profile flag (init 1 in 0x1dae4c; read by game at 0xa0cba)",
      "+0x38 D3D_FEATURE_LEVEL (init 0x9100 at 0x1dae54; overwritten by D3D11CreateDevice pFeatureLevel out-param, 0x1db00a)",
      "+0x3c/+0x40 float d3dRenderTargetSize W,H; +0x44/+0x48 float outputSize W,H; +0x4c/+0x50 float logicalSize W,H",
      "+0x54 nativeOrientation, +0x58 currentOrientation (DisplayOrientations enum), +0x5c float dpi (init -1.0), +0x60/+0x64 float compositionScaleX/Y (init 1.0)",
      "+0x68 -> device profile row (table @0xdc8850, see device_profile_table), +0x6c/+0x70 int longer/shorter desktop side of output 0, +0x74 IDeviceNotify* (AppMain, set by RegisterDeviceNotify 0x1dbbc0)"],
     "high",
     notes="Template: Microsoft 'Direct3D App (XAML)' WP8.1 DeviceResources. The swap chain is a COMPOSITION swap chain attached through ISwapChainPanelNative::SetSwapChain; there is no HWND and no CoreWindow on this path.")
fact("device_resources.global_instance", "data", 0x00e2afc4,
     "g_DeviceResources (raw DX::DeviceResources*): written by the ctor body 0x1dae14 (str r4,[0x122afc4]); read through getter 0x4b054 (throws std::runtime_error 'Device resource object hasn't b(een created)' when NULL). 20 game call sites use the getter (shader code reads +0x38, render code reads +0x04/+0x08/+0x10/+0x14, AppPlatform_Winrt::initialize reads +0x60/+0x64).",
     ["xref 0xe2afc4 -> sub_4b054+0xc and sub_1dae14+0x46", "callers 4b054 -> 20 sites (4b5f0, 4b5f8, a0cb6, b9e3e, b9e58, ba5e6, ba600, bec5a, c36e2, c38a6, c656e, c8a96, cb1d2, ce35e, ce3d0, d116a, d6ac8, d8e66, 111e02, 111e0a)"],
     "high", sig="DX::DeviceResources* fn4b054(void) -> r0 ; throws if g_DeviceResources==NULL", ebytes=12)

# ---------------------------------------------------------------- construction
fact("device_resources.make_shared", "function", 0x001dce2c,
     "std::make_shared<DX::DeviceResources>(&out): operator new(0x84) -> sub_1dc5a4 (ref counts +4/+8=1, vtable 0xdc8e64 stored, object at +0xc) -> ctor body 0x1dae14 (zero fields, set defaults, g_DeviceResources=this, then CreateDeviceResources 0x1dafc0). out[0]=obj(+0xc), out[1]=ctrl block. Called once, from the MainPage ctor at 0x1dd39a.",
     ["callers 1dce2c -> 001dd39a in MainPage ctor sub_1dcf5c", "func 1dce2c: movs r0,#0x84; bl thunk operator new; bl 0x1dc5a4; str r3 = r0+0xc",
      "callers 1dae14 -> 0x1dc5d2 (sub_1dc5a4) ; 1dae14 contains mov r3,#0x9100; str r3,[r4,#0x38]; str r4,[g_DeviceResources]; bl 0x1dafc0"],
     "high", sig="std::shared_ptr<DeviceResources>* fn(std::shared_ptr<DeviceResources>* out /*r0*/) -> r0=out", ebytes=12,
     notes="Confirms the friend's 0x1DCE2C (it is the make_shared helper, not the ctor body 0x1dae14).")
fact("create_device.call_site", "function", 0x001dafc0,
     "DeviceResources::CreateDeviceResources: D3D11CreateDevice(NULL adapter, D3D_DRIVER_TYPE_HARDWARE=1, NULL, Flags=0, levels, 5, D3D11_SDK_VERSION=7, &dev, &this->featureLevel(+0x38), &ctx). HRESULT is NOT checked.",
     ["dis 1dafc0: call at 0x1db028 thunk 0x1f433c = IAT 0x20b1a4 d3d11.dll!D3D11CreateDevice (impcallers D3D11CreateDevice -> only this site)",
      "r0=0 (0x1db026), r1=1 (0x1db024), r2=0 (0x1db022), r3=0 Flags (0x1db020); [sp]=levels (0x1db01a), [sp+4]=5 (0x1db016), [sp+8]=7 (0x1db014), [sp+0xc]=&dev, [sp+0x10]=this+0x38, [sp+0x14]=&ctx",
      "afterwards: QI dev -> ID3D11Device2 (this+4), ctx -> ID3D11DeviceContext2 (this+8), ctx -> ID3DUserDefinedAnnotation (this+0), then 0x1db108 bl 0x1db858 (device profile)"],
     "high", ebytes=12,
     sig="void CreateDeviceResources(DeviceResources* this /*r0*/); callee-saved r4-r5,r7,fp; throws Platform::COMException on QI failure",
     notes="No D3D11_CREATE_DEVICE_BGRA_SUPPORT (0x20) and no DEBUG flag. A NULL device (failed create) is not detected here; it faults at the next QI.")
fact("create_device.feature_levels", "data", 0x001dafda,
     "Feature level array is built on the stack from immediates: 0xb100 (11_1) @0x1dafda, 0xb000 (11_0) @0x1dafe0, 0xa100 (10_1) @0x1dafe6, 0xa000 (10_0) @0x1dafec, 0x9300 (9_3) @0x1daff2; count 5 is `movs r3,#5` @0x1db016. "
     "The game does NOT ask for 9_1 or 9_2: on a 9_1-only GPU (Surface RT / Tegra 3) D3D11CreateDevice fails (E_INVALIDARG/DXGI_ERROR_UNSUPPORTED) and the code then dereferences a NULL device.",
     ["dis 0x1dafda: 4ff43143 mov.w r3,#0xb100 ; str [r7,#0x18]; 0x1dafe0 #0xb000 [#0x1c]; 0x1dafe6 #0xa100 [#0x20]; 0x1dafec #0xa000 [#0x24]; 0x1daff2 #0x9300 [#0x28]",
      "imm 9100 -> only sub_1dae14 (default of DR+0x38); imm 9300 -> sub_1dafc0 + the shader-model getters"],
     "high", ebytes=8,
     notes="Host must hook D3D11CreateDevice (IAT cell 0x20b1a4) and replace pFeatureLevels/FeatureLevels (e.g. {0x9100}). The `mov.w` immediates are Thumb-2 T2 'mov.w r3,#imm' and could alternatively be patched, but a list with duplicates is riskier than the IAT hook.")
fact("create_device.host_hook", "iat", 0x0020b1a4,
     "IAT cell of d3d11.dll!D3D11CreateDevice (single caller thunk 0x1f433c). Recommended host hook: force FeatureLevels={D3D_FEATURE_LEVEL_9_1}, driver HARDWARE (fall back to WARP only for debugging), keep Flags (0 is fine; BGRA flag only needed for D2D). "
     "After the real call write 0x9300 into *pFeatureLevel (the game then treats the device exactly like a WP8.1 9_3 phone) OR patch the three shader-model getters (see shader.feature_level_lie).",
     ["imports D3D -> 0020b1a4 d3d11.dll!D3D11CreateDevice", "thunk 0x1f433c = movw/movt ip + ldr.w pc,[ip]"],
     "high", sig="HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**) -> r0; args r0-r3 + 6 stack words (AAPCS)",
     notes="Pointer-sized data cell: patch with VirtualProtect(PAGE_READWRITE) per PORT_RULES #4.")
fact("create_device.device_profile", "function", 0x001db858,
     "DeviceResources::LoadDeviceProfile (called at the end of CreateDeviceResources, 0x1db108): QI device->IDXGIDevice3, GetAdapter (slot 7), adapter->GetParent(IDXGIFactory2), factory->EnumAdapters1(0) (slot 12) ->GetDesc (slot 8, DXGI_ADAPTER_DESC @sp+0x88); device adapter->EnumOutputs(0) (slot 7) -> IDXGIOutput::GetDesc (slot 7): stores longer/shorter side of DesktopCoordinates into DR+0x6c/+0x70 "
     "(skipped when EnumOutputs returns DXGI_ERROR_NOT_FOUND 0x887a0002) and selects DR+0x68 = row of the GPU table at 0xdc8850 whose name is a wcsstr() substring of the adapter description (default row 0 'Default').",
     ["func 1db858: wcsstr IAT 0x20b528 loop over rows 0xdc8868 + k*0x14 (11 rows) ; str r5,[r6,#0x68] default ; str r4,[r6,#0x68] on match",
      "table dump @0xdc8850: Default{0.5,1280,720,0}; 'Adreno 225'{0.667,1280,720,0}; 302{0.5}; 304/305/306{0.667,1280x720}; 320/330{0.667,1920x1080,flag1}; 405/418/420/430{1.0,1920x1080,flag1}",
      "consumer 0x1dbfbc (ApplyResolutionOverride, called by SetSwapChainPanel 0x1dbf20 and SetCompositionScale 0x1dbbe2): if cfg && cfg.scale!=0 && longSide>=cfg.minW && shortSide>=cfg.minH then compositionScale *= cfg.scale; DR+0x34=1; DR+0x35=cfg.flag"],
     "high", ebytes=8,
     notes="On a desktop adapter ('NVIDIA Tegra 3'/'Microsoft Basic Render' do not match) the Default row applies: a >=1280x720 screen renders at HALF resolution (Surface RT 1366x768 -> 683x384 back buffer) and relies on SetMatrixTransform(1/scale) = 2x composition scaling (or DXGI STRETCH on an HWND swap chain). To get native resolution patch the float at RVA 0xdc8854 (Default.scale) from 0.5f (00 00 00 3f) to 1.0f (00 00 80 3f), or the minW/minH floats at 0xdc8858/0xdc885c.")

# ---------------------------------------------------------------- frame loop
fact("frame.render_loop", "function", 0x001da160,
     "THE FRAME LOOP (no CompositionTarget.Rendering, no per-frame DispatcherTimer): WorkItemHandler body started by AppMain::StartRenderLoop on a High-priority TimeSliced ThreadPool item. "
     "while (IAsyncAction.Status == Started(0)) { scoped_lock lock(AppMain+0x10); if (byte AppMain+0x3c /*app initialised*/) { sub_50eac(*(AppMain+0x38)) /*AppPlatform_Winrt::update*/; (*(App**)(AppMain+0x34))->vtable[19]() /*Minecraft::update 0x1d8a54*/; DeviceResources::Present(*(AppMain+4)) /*0x1dbb10*/ } }",
     ["dis 1da160: QI IAsyncInfo (IID 00000036-0000-0000-C000-000000000046 @0x20fbd8) -> slot 7 (get_Status) -> loop only if ==0 (0x1da1a0-0x1da1c8)",
      "scoped_lock ctor/dtor = IAT 0x20b424/0x20b428 (msvcr120_app Concurrency::critical_section::scoped_lock) on AppMain+0x10 (0x1da1d4..0x1da1e4, 0x1da210)",
      "ldrb [AppMain+0x3c] (0x1da1ea) ; bl 0x50eac with *(AppMain+0x38) (0x1da1f4-0x1da1f8); ldr r3,[AppMain+0x34]; vtable+0x4c blx (0x1da1fc-0x1da204); ldr r0,[AppMain+4]; bl 0x1dbb10 (0x1da20a-0x1da20c)",
      "callers 1dbb10 -> only 0x1da20c (so this is the only Present path); vtable 0x33ffcc slot 19 -> Minecraft::vf19 = 0x1d8a54"],
     "high", ebytes=12,
     sig="void loop(WorkItemHandler capture* r0 /*[r0]=AppMain**/, IAsyncAction^ r1 /*action*/); runs forever until Cancel()",
     notes="Approach (b): fake Windows.System.Threading.ThreadPool.RunAsync(handler,priority,options) = start a real thread that calls the WorkItemHandler Invoke(IAsyncAction*) with a fake IAsyncAction (QI IAsyncInfo; get_Status slot 7 returns Started(0) until Cancel slot 9 flips it to Canceled(2)). "
           "Approach (a): host thread does exactly the 3 calls above under AppMain+0x10.")
fact("frame.per_frame_update", "function", 0x001d8a54,
     "Minecraft::update (MinecraftClient vtable 0x33ffcc slot 19): this->vf28(+0x70) pre-update; Timer::advanceTime(this+0xa8) (0x65f10) -> N ticks; for each tick this->vf31(+0x7c)(i,N-1) [tick]; getTime (0x11b07c) frame delta -> this+0xd8/0xe0; this->vf29(+0x74) = render (MinecraftClient::vf29 0x5a114: GameRenderer::render sub_b9e1c ...); FPS counter 0x1198b8; this->vf30(+0x78).",
     ["dis 1d8a54 (174 bytes) vtable calls at +0x70, +0x7c, +0x74, +0x78", "vtable 0x33ffcc: [19]=0x5d8a55 (VA|1) Minecraft::vf19, [29]=0x45a115 -> RVA 0x5a114, [28]=0x459955, [30]=0x456bdd, [31]=0x459a8d (VA|1 values)",
      "0x5a114 calls sub_b9e1c (viewport + bind back buffer, uses DR getters 0x1dbf80/0x1dbcd0)"], "medium", ebytes=12,
     sig="void Minecraft::update(Minecraft* this /*r0*/)",
     notes="vtable cells hold VA|1 (0x400000 base + Thumb bit), not RVAs. Names vf28..vf31 are positional only.")
fact("frame.start_stop", "function", 0x001da8a8,
     "AppMain::StartRenderLoop: if AppMain+0xc (IAsyncAction) is NULL/not Started, build WorkItemHandler (ctor 0x1d9c00, body 0x1da160) and ThreadPool::RunAsync(handler, WorkItemPriority::High=1, WorkItemOptions::TimeSliced=1) via helper 0x114078 (IThreadPoolStatics IID b6bf67dd-84bd-44f8-ac1c-93ebcb9dba91, slot 8); result -> AppMain+0xc. StopRenderLoop 0x1da9d4 = IAsyncInfo::Cancel (slot 9). Visibility handler 0x1df9ac starts/stops it.",
     ["func 1da8a8: QI IAsyncInfo, get_Status, `movs r2,#1; movs r1,#1; bl 0x114078`", "0x114078: movw/movt IID bytes b6bf67dd..., L\"Windows.System.Threading.ThreadPool\" @0x2481c0 passed to sub_2001e4 (RoGetActivationFactory wrapper), call slot 0x20/4=8",
      "1da9d4: IAsyncInfo slot 0x24/4=9 Cancel", "callers of 1da8a8: 1df98c (DispatcherTimer tick), 1df9f0 (Visibility true), 1e01aa/1df4de (resume paths)"],
     "high", ebytes=12)
fact("frame.init_flag", "data", 0x001da2d8,
     "AppMain+0x3c (atomic byte) = 'game initialised'. It is set by the init work item: AppMain ctor chain 0x1d9d14 -> 0x1d9dd4 queues a ThreadPool item (lambda ctor 0x1d9b68, queued by 0x1da4a4 = IThreadPoolStatics::RunAsync(handler) slot 6) whose body (FunctorCapture vf1 @0x1da2d8) calls App::init sub_471e8(App*, platformCell) and then `dmb; strb 1,[cap+8]; dmb` where cap = AppMain+0x34 so cap+8 = AppMain+0x3c.",
     ["dis 1da2d8: ldr r3,[r4,#4]; ldr r1,[r3,#4]; ldr r0,[r3]; bl 0x471e8; movs r2,#1; strb r2,[r3,#8]",
      "1d9dd4 end: bl 0x1d9b68 then bl 0x1da4a4 ; 1da310 reads the flag (ldrb [r0,#0x3c]; dmb)", "0x471e8: this+0xc=r1; (this->vf7=[vt+0x1c])(); this+8=1; b 0x47bf4"],
     "high", ebytes=12,
     notes="This is what the friend's StartMenuScreen crash points to: MinecraftClient+0x118 (textures) / +0x11c (font) are created by App::init (0x471e8 -> vf7). Any host that bypasses MainPage/AppMain must run sub_471e8 (or at least vf7) before the first Minecraft::update.")
fact("frame.timer_tick", "function", 0x001df8a4,
     "MainPage DispatcherTimer Tick (timer created by 0x1dce94: Interval 500000 ticks = 50 ms via IDispatcherTimer slot 7, add_Tick slot 9 with handler 0x1df8a4, Start slot 11; stored MainPage+0x84). "
     "Tick: if AppMain+0x3c==0 return; timer.Stop (slot 12); Logo/UIElement(+0x60).Visibility=Collapsed (0x1e1cf0(x,1)); AppMain::AttachToPanel 0x1da634(AppMain, swapChainPanel(+0x5c), EditBox(+0x64), Button(+0x68)); Visibility again; AppMain::StartRenderLoop 0x1da8a8.",
     ["dis 1df8a4 (246 bytes)", "ctor sub_1dcf5c 0x1dd53a bl 0x1dce94; pool[0x1dd870]=0x7a120; IDispatcherTimer IID d160ce46-cd22-4f5f-8c97-40e61da3e2dc @0x2e2358"],
     "high", ebytes=12,
     sig="void Tick(MainPage* r0 /*capture*/, Object^ sender /*r1*/, Object^ args /*r2*/)",
     notes="No timer => no render loop => nothing ever calls Present (a plausible root of the friend's all-white window if they never ran this path).")
fact("frame.present", "function", 0x001dbb10,
     "DeviceResources::Present: swapChain(+0xc)->Present(SyncInterval=0, Flags=0) (IDXGISwapChain slot 8 @vtable+0x20, NOT Present1). hr == DXGI_ERROR_DEVICE_REMOVED (0x887a0005) or DEVICE_RESET (0x887a0007) -> inline HandleDeviceLost (release swapchain, lock-object vf0, CreateDeviceResources + CreateWindowSizeDependentResources, lock-object vf1); other negative hr -> throw COMException.",
     ["dis 1dbb10: r1=0,r2=0, ldr r3,[r3,#0x20] blx ; cmp 0x887a0005 (pool 0x1dbba8) / 0x887a0007 (pool 0x1dbba4)", "callers 1dbb10 -> 0x1da20c only"],
     "high", ebytes=12, sig="void Present(DeviceResources* this /*r0*/)",
     notes="SyncInterval 0 + flip model + IDXGIDevice1::SetMaximumFrameLatency(1) (0x1db4b0) is what throttles the loop; with an HWND/BitBlt chain Present(0,0) will run unthrottled -> host should add its own frame limiter or use sync 1 via a Present hook (vtable slot 8).")

# ---------------------------------------------------------------- swap chain
fact("swapchain.create", "function", 0x001db158,
     "DeviceResources::CreateWindowSizeDependentResources. If panel(+0x30)==NULL return. Else: OMSetRenderTargets(1,{NULL},NULL) (ctx slot 33); release RTV/DSV; ctx->Flush (slot 111 @0x1bc); compute sizes; "
     "if swapChain exists -> ResizeBuffers(2,W,H,DXGI_FORMAT_B8G8R8A8_UNORM=0x57,0) (slot 13; DEVICE_REMOVED/RESET -> HandleDeviceLost 0x1dbab8); else QI dev->IDXGIDevice3, GetAdapter(slot 7), adapter->GetParent(IDXGIFactory2) and "
     "factory->CreateSwapChainForComposition(dev, &desc, NULL, &this->swapChain) (factory vtable slot 24 = +0x60, call at 0x1db400); IDXGIDevice1::SetMaximumFrameLatency(1) (slot 12); "
     "queue the attach lambda on panel.Dispatcher (RunAsync priority 1); QI swapchain->IDXGISwapChain2 and SetMatrixTransform({1/compScaleX,0,0,1/compScaleY}) (slot 34 = +0x88 @0x1db57c); GetBuffer(0,ID3D11Texture2D) -> CreateRenderTargetView(+0x10); "
     "D24_UNORM_S8_UINT depth texture W x H -> CreateDepthStencilView(+0x14); build viewport (+0x18) + RSSetViewports; OMSetRenderTargets(1,&RTV,DSV).",
     ["DXGI_SWAP_CHAIN_DESC1 built @0x1db2c0-0x1db322: Width=lroundf(+0x3c), Height=lroundf(+0x40), Format=0x57, Stereo=0, SampleDesc{1,0}, BufferUsage=0x20 (RENDER_TARGET_OUTPUT), BufferCount=2, Scaling=0 (STRETCH), SwapEffect=3 (FLIP_SEQUENTIAL), AlphaMode=3 (IGNORE), Flags=0",
      "IIDs @rdata: 0x2df278 IDXGIDevice3 (6007896c-3244-4afd-bf18-a6d3beda5023), 0x2df228 IDXGIFactory2 (50c83a1c-e072-4c48-87b0-3630fa36a6d0), 0x2df288 IDXGISwapChain2 (a8be2ac4-199f-4946-b331-79599fb98de7), 0x2df248 ID3D11Texture2D (6f15aaf2-d208-4e89-9ab4-489535d34f9c)",
      "depth desc @0x1db654-0x1db686: Mip=1 Array=1 Format=0x2d(D24S8) Sample{1,0} Usage=0 Bind=0x40; DSV desc ViewDimension=3 (TEXTURE2D)",
      "swapchain_probe_0104.py replays CreateSwapChainForComposition + SetMatrixTransform + RTV + D24S8 + Present(0,0) on a {9_1} device: all S_OK (see md)"],
     "high", ebytes=12, sig="void CreateWindowSizeDependentResources(DeviceResources* this /*r0*/); throws Platform::COMException on any HRESULT<0 except the DEVICE_REMOVED/RESET paths")
fact("swapchain.create_call_patch_point", "hook_site", 0x001db3fe,
     "Exact instruction pair that calls IDXGIFactory2::CreateSwapChainForComposition: `ldr r4,[r4,#0x60]` (24 6e) @0x1db3fe + `blx r4` (a0 47) @0x1db400. Regs at the call: r0=factory, r1=ID3D11Device*(this+4), r2=&DXGI_SWAP_CHAIN_DESC1, r3=NULL (restrictToOutput), [sp]=IDXGISwapChain1** (this+0xc). Returns r0=HRESULT checked at 0x1db402.",
     ["dis 0x1db3ee..0x1db404: str r4,[sp] ; ldr r4,[r7,#0x40]; ldr r1,[r6,#4]; movs r3,#0; mov r0,r4; ldr r4,[r4]; adds r2,r7,#0x50; ldr r4,[r4,#0x60]; blx r4"],
     "high", ebytes=8,
     notes="Only needed for the HWND swap-chain alternative (patch the dxgi factory vtable slot 24 cell instead of code: PORT_RULES prefers data cells). With the DirectComposition alternative (host IDCompositionVisual::SetContent in the fake SetSwapChain) this call and SetMatrixTransform run UNPATCHED.")
fact("swapchain.set_matrix_transform", "hook_site", 0x001db57c,
     "SetMatrixTransform call sequence: 0x1db56c `ldr r3,[r7,#0x3c]` (swapChain2) ... 0x1db578 `ldr.w r3,[r3,#0x88]` ... 0x1db57c `blx r3` (98 47). 0x88/4 = 34 = IDXGISwapChain2::SetMatrixTransform(const DXGI_MATRIX_3X2_F*) (matrix at sp+8: _11=1/DR.compositionScaleX, _22=1/DR.compositionScaleY). hr<0 -> throws. "
     "Valid on composition swap chains (S_OK in probe) but returns DXGI_ERROR_INVALID_CALL 0x887A0001 on HWND swap chains (probe).",
     ["dis 0x1db56c..0x1db57e", "slot count: IDXGISwapChain 3..17, IDXGISwapChain1 18..28, IDXGISwapChain2: 29 SetSourceSize,30 GetSourceSize,31 SetMaximumFrameLatency,32 GetMaximumFrameLatency,33 GetFrameLatencyWaitableObject,34 SetMatrixTransform (=0x88)",
      "probe: composition SC SetMatrixTransform S_OK ; HWND SC 0x887A0001"],
     "high", ebytes=4,
     notes="Friend's 'must be skipped' is only true for an HWND swap chain. Skip patch: replace `98 47` at 0x1db57c with `00 20` (movs r0,#0) so `cmp r0,#0; bge` continues; or patch the dxgi swap-chain vtable cell 34 to a host stub returning S_OK.")
fact("swapchain.attach_lambda", "function", 0x001daf04,
     "DispatchedHandler body that attaches the swap chain: QI panel(DR+0x30) with ISwapChainPanelNative (f92f19d2-3ade-45a6-a20c-f6f1ea90554b @0x2df238) then ISwapChainPanelNative::SetSwapChain(DR+0xc) (vtable slot 3 = +0xc). Queued from CreateWindowSizeDependentResources at 0x1db446..0x1db476: lambda object ctor 0x1dad7c, panel QI IDependencyObject (5c526665-f60e-4912-af59-5fe0680f089d @0x2df1f8) -> Dispatcher getter 0x1dc3fc (vtable slot 11 / +0x2c, out-param) -> CoreDispatcher::RunAsync(priority=1, handler) helper 0x11401c (ICoreDispatcher slot 8).",
     ["guid f92f19d2-... -> 002df238 refs=1 mov32 001daf24 in sub_1daf04", "func 1daf04: ldr r3,[r4]; ldr r3,[r3,#0x30] (DR+0x30 panel) ; QI ; ldr r1,[DR,#0xc]; ldr r3,[r0],[r3,#0xc] blx",
      "vtable 0x11c8a4c FunctorCapture (DispatchedHandler) built by 0x1dad7c; thunk 0x1dbb00 -> b.w 0x1daf04"],
     "high", ebytes=12, sig="void lambda(Capture* r0 /*[r0]=DeviceResources**/)",
     notes="Host (approach b): fake SwapChainPanel must answer QI for ISwapChainPanelNative and its SetSwapChain must either attach the composition swap chain to a DirectComposition visual (dcomp.dll exists on Win8/8.1/RT; SetContent+Commit) or, if an HWND swap chain is used instead, just remember it. Fake CoreDispatcher::RunAsync may run the handler inline.")
fact("swapchain.panel_requirements", "abi", None,
     "Everything DeviceResources/MainPage need from the SwapChainPanel (MainPage+0x5c == DR+0x30), with the value the host must return on a desktop: "
     "IFrameworkElement (a391d09b-4a99-4b7c-9d8d-6fa5d01f6fbf @0x2df208): ActualWidth (helper 0x1dc504, slot 0x34) and ActualHeight (helper 0x1dc490, slot 0x38) = client size in DIPs (double); add_SizeChanged (slot 8 / +0x20 registered at 0x1dd24x). "
     "ISwapChainPanel (c589644f-eba8-427a-b75a-9f1f93a11ae9, IID bytes at RVA 0x20fbf8): get_CompositionScaleX (slot 6), Y (slot 7) = 1.0; add_CompositionScaleChanged (slot 8); CreateCoreIndependentInputSource (slot 10, input worker 0x1de0bc). "
     "IDependencyObject (5c526665-...): Dispatcher (slot 11). IUIElement (676d0be9-b65c-41c6-ba40-58cf87f201c1 @0x2de660): Visibility setter (0x1e1cf0). ISwapChainPanelNative: SetSwapChain.",
     ["IIDs read from rdata: 0x2df208, 0x2df1f8, 0x2de660 (=0x2e2378), 0x2df238, 0x20fbf8", "dis 1dbd40 SetSwapChainPanel: QI IFrameworkElement ->1dc490/1dc504; QI ISwapChainPanel -> slot 0x18 (X, store +0x60) and 0x1c (Y, +0x64)",
      "ctor 0x1dd1ce..0x1dd208 add_CompositionScaleChanged on ISwapChainPanel slot 0x20; 0x1dd24e..0x1dd284 add_SizeChanged on IFrameworkElement"],
     "medium",
     notes="Slots for IFrameworkElement getters are taken from the call shape only (vtable offsets 0x34/0x38 in helpers 0x1dc504/0x1dc490); the faking agent must map them onto the real IFrameworkElement vtable.")
fact("resize.size_changed", "function", 0x001df808,
     "SizeChanged handler = the ONLY caller of DeviceResources::SetSwapChainPanel (0x1dbd40 @0x1df840). Order: scoped_lock(AppMain+0x10); DR->SetSwapChainPanel(MainPage+0x5c) [reads ActualWidth/Height, DisplayInformation native/current orientation, composition scale, LogicalDpi, ApplyResolutionOverride, then CreateWindowSizeDependentResources => swap chain, RTV, DSV, viewport]; "
     "args->get_NewSize (SizeChangedEventArgs slot 7 +0x1c, out Size{w,h} float) -> DR->SetLogicalSize(w,h) 0x1dbc80; sub_bf60 (no-op); AppMain::OnWindowSizeChanged 0x1da32c (platform setScreenWidth/Height 0x4fa40/0x4fa20 = size*dpiScale rounded, App vtable+0x50 (setSize(w,h,scale))).",
     ["dis 1df808", "callers 1dbd40 -> 001df840 only", "handler registered by ctor sub_1dcf5c 0x1dd22c: sub_1dcc5c(SizeChangedEventHandler) + add_SizeChanged on IFrameworkElement(MainPage+0x5c)"],
     "high", ebytes=12, sig="void SizeChanged(MainPage* r0 /*capture*/, Object^ sender /*r1*/, SizeChangedEventArgs^ args /*r2*/)",
     notes="Without a SizeChanged being RAISED by the host after construction there is NO swap chain, NO RTV/DSV (DR+0x10/+0x14 stay NULL) and nothing can ever be drawn => blank/white window. XAML normally fires it after first layout.")
fact("resize.other_handlers", "function", None,
     "Remaining DeviceResources-facing handlers (registered in the MainPage ctor 0x1dcf5c): CompositionScaleChanged 0x1df510 -> SetCompositionScale(panel.CompositionScaleX/Y) 0x1dbbcc; DisplayInformation::DpiChanged 0x1df67c -> SetDpi 0x1dbc5c; DisplayInformation::OrientationChanged 0x1df76c -> SetCurrentOrientation 0x1dbc1c; "
     "static DisplayInformation::DisplayContentsInvalidated 0x1df618 -> ValidateDevice 0x1dc0b4; CoreWindow::VisibilityChanged 0x1df9ac -> MainPage+0x80 byte, Start/StopRenderLoop. Each setter calls CreateWindowSizeDependentResources only when the value changed.",
     ["callers: 1dbbcc<-1df5c2 (sub_1df510), 1dbc5c<-1df6c8 (sub_1df67c), 1dbc1c<-1df7b6 (sub_1df76c), 1dc0b4<-1df64a (sub_1df618)", "registrations in ctor: sub_1dc6ec(1df9ac) ; sub_1dc7d4(1df67c via DisplayInformation slot 0x38 add_DpiChanged), (1df76c via slot 0x20 add_OrientationChanged), sub_1dcd44(1df510); sub_1e183c static event for 1df618"],
     "high",
     notes="Host should NOT raise DisplayContentsInvalidated (ValidateDevice would create a DXGI factory and compare adapter LUIDs). Fake DisplayInformation values: CurrentOrientation=NativeOrientation=Landscape(1) (swapDimensions table 0x1db23c => no W/H swap), LogicalDpi=96 (slot 11 / +0x2c), RawDpi irrelevant.")
fact("resize.size_math", "data", 0x001db1c4,
     "Size math (template): outputSize(+0x44,+0x48) = logicalSize(+0x4c,+0x50) x compositionScale(+0x60,+0x64) (clamped to >=1.0); render-target size (+0x3c,+0x40) = outputSize, W/H swapped only when native=Landscape/Portrait and current orientation is Portrait(2)/PortraitFlipped(8) (byte table at 0x1db23c); swap chain / depth size = lroundf(+0x3c) x lroundf(+0x40). "
     "DPI (+0x5c) is stored but not used for sizing. compositionScale is first multiplied by the device-profile scale (0.5 on a >=1280x720 desktop for the Default row).",
     ["dis 0x1db1c4..0x1db260", "tbb table bytes 06 04 06 06 06 06 06 04 @0x1db23c (idx = currentOrientation-1)"], "medium",
     notes="Size is NOT multiplied by dpi/96: the XAML layout size in DIPs is already the physical size at scale 1.")

# ---------------------------------------------------------------- device lost
fact("device_lost.handling", "function", 0x001dbab8,
     "HandleDeviceLost: release swap chain (+0xc), call DR+0x74 (IDeviceNotify) vtable[0] OnDeviceLost, CreateDeviceResources (0x1dafc0), CreateWindowSizeDependentResources (0x1db158), notify vtable[1] OnDeviceRestored. AppMain's IDeviceNotify vtable (0xdc8830) has BOTH slots = 0xbf60 which is an empty function (push {fp,lr}; mov fp,sp; pop {fp,pc}): the game never re-creates its GPU resources after a device loss.",
     ["bytes dc8830: 61 bf 40 00 61 bf 40 00 (VA|1 of 0x40bf60 twice); dis bf60 = push.w {fp,lr}; mov fp,sp; pop.w {fp,pc}", "RegisterDeviceNotify 0x1dbbc0 = str r1,[r0,#0x74]; called from AppMain ctor 0x1d9d90",
      "Present 0x1dbb10 and ResizeBuffers path 0x1db2a6 compare 0x887a0005/0x887a0007; ValidateDevice 0x1dc0b4 (adapter LUID compare + GetDeviceRemovedReason slot 39 @0x9c) only reached via DisplayContentsInvalidated 0x1df618"],
     "high", ebytes=12,
     notes="On a desktop there is no reason to lose the device; do not simulate DEVICE_REMOVED. If the host ever sees it, restart the game.")

# ---------------------------------------------------------------- shaders
fact("shader.d3dcompile_call", "hook_site", 0x000d6c94,
     "Only D3DCompile call site (IAT 0x20b000, thunk 0x1f4330) in ShaderProgram::ShaderProgram sub_d6a3c: D3DCompile(pSrcData=std::string data, SrcDataSize=len, pSourceName='data/'+shaderPath, pDefines=NULL, pInclude=(ID3DInclude*)1 /*D3D_COMPILE_STANDARD_FILE_INCLUDE*/, pEntrypoint=\"main\" (@0x2352a0), pTarget=<vs|ps>_<major>_<minor><suffix>, Flags1=0x8000 (D3DCOMPILE_OPTIMIZATION_LEVEL3), Flags2=0, &code, &errors). Error text prefixed 'HLSL Compiler error: ' (@0x2352c8) and thrown.",
     ["dis d6c58..d6c94: r0=src, r1=len, r2=c_str('data/'+path) built by sub_10d2c(&buf,\"data/\",name @0x235298), r3=0; [sp]=1, [sp+4]=\"main\", [sp+8]=profile string (r7+0xd0), [sp+0xc]=0x8000, [sp+0x10]=0, [sp+0x14]=&code (r7+0xb0), [sp+0x18]=&err (r7+0xb4)",
      "impcallers D3DCompile -> single site 0xd6c94 in sub_d6a3c ; D3DReflect single site 0xd0220 in sub_d01b8 (IID_ID3D11ShaderReflection 8d536ca1-0cca-4956-a837-786963755584 @0x2ecf90)"],
     "high", ebytes=12,
     sig="HRESULT D3DCompile(LPCVOID r0, SIZE_T r1, LPCSTR r2, const D3D_SHADER_MACRO* r3, ID3DInclude* [sp], LPCSTR [sp+4], LPCSTR [sp+8], UINT [sp+0xc], UINT [sp+0x10], ID3DBlob** [sp+0x14], ID3DBlob** [sp+0x18])",
     notes="D3DCOMPILER_47.dll exists on Windows 8.1/RT 8.1 (system32), so the import resolves natively. STANDARD_FILE_INCLUDE resolves `#include \"ShaderConstants.fxh\"` relative to the *current directory* + dirname(pSourceName) = <cwd>\\data\\shaders\\dx11\\ (verified with include_test.py: fails with X1507 from another cwd). Host: SetCurrentDirectory(game root) or hook IAT 0x20b000 and pass its own ID3DInclude.")
fact("shader.profile_selection", "function", 0x000d73fc,
     "Profile string = getShaderProfileType (sub_d74f8: 0->\"vs\" @0x235244, 1->\"ps\" @0x235248) + '_' + getMajorShaderModel (sub_d7230) + '_' + getMinorShaderModel (sub_d7314) + getShaderModelSuffix (sub_d73fc) where the feature level is DR+0x38 (via getter 0x4b054 at 0xd6ac8): "
     "major: 0x9300/0xa000/0xa100 -> 4, 0xb000/0xb100 -> 5; minor: 0xa100 -> 1, else 0; suffix: 0x9300 -> \"_level_9_3\" (@RVA 0x2351f0), 0xa000/0xa100/0xb000/0xb100 -> \"\". "
     "Any other level (0x9100, 0x9200, anything unknown) throws std::runtime_error \"Unkonwn feature level @ `anonymous-namespace'::getShaderModelSuffix\" (also from getMajor/getMinor).",
     ["dis d73fc: cmp r1,#0xa100 bgt; cmp #0x9300 beq; cmp #0xa000; cmp #0xb000/#0xb100; falls to memset + 'Unkonwn feature level' @0x23522c", "dis d7230/d7314 compare list {0xa100,0x9300,0xa000,(0xb000,0xb100)} -> movs r0,#4 / #5 (0xd72f6) ; minor movs #1 for 0xa100 (0xd7346)",
      "string bytes @0x2351f0: 5f 6c 65 76 65 6c 5f 39 5f 33 00 = '_level_9_3'"],
     "high", ebytes=12,
     notes="Resulting game request on a (real or reported) 9_3 device: vs_4_0_level_9_3 / ps_4_0_level_9_3 (what WP8.1 phones used). The game never requests level_9_1.")
fact("shader.feature_level_lie", "hook_site", 0x002351f0,
     "To run the unmodified shader code on FL9_1 hardware: (1) D3D11CreateDevice hook makes the device at {9_1} but stores 0x9300 into *pFeatureLevel (so getMajor/getMinor/getSuffix and the FL<0xa000 branches see '9_3'); (2) patch ONE data byte: RVA 0x2351f0+9 '3'(0x33) -> '1'(0x31) so the suffix becomes \"_level_9_1\" and every vs/ps is compiled for vs_4_0_level_9_1 / ps_4_0_level_9_1. Alternative to (2): hook IAT 0x20b000 (D3DCompile) and rewrite pTarget.",
     ["all 76 (shader,stage,defines) combos compile at 4_0_level_9_1 (shader.feasibility) ; suffix string referenced only by sub_d73fc (strx '_level_9_3' refs=1)", "the three other FL consumers (cb1d6 FL<0xa000 -> R8G8B8A8_SNORM_UNSUPPORTED, d8e6a FL<0xa000 -> pow2 rounding, d116e FL<0xa000 -> vertex format 28 instead of 31) treat 9_1 and 9_3 identically (only `cmp #0xa000`)"],
     "high", ebytes=12,
     notes="Alternative without lying: patch the immediates in the three getters (0xd7252/0xd7336/0xd7420 `cmp.w r0,#0x9300`) to accept 0x9100 - more invasive. Probe note: on this x64 PC even _level_9_3 bytecode loaded on a {9_1} device (76/76 ok on hardware and WARP) but that is not guaranteed on the Tegra RT driver, so compile to level_9_1.")
fact("shader.sources", "data", 0x000c9cd4,
     "Shader sources are plain files, not embedded: material JSON (data/materials/{common,fancy,sad}.json index -> *.material; keys vertexShader/fragmentShader like \"shaders/renderchunk.vertex\", \"defines\", \"states\") parsed by MaterialGroup sub_c9cd4 / defines builder sub_cb16c; path = dirname + \"/dx11\" + basename + \".hlsl\" (strings 'shaders' @0x231ba0, '/dx11' @0x231bb0, '.hlsl' @0x231ba8) => data/shaders/dx11/<name>.hlsl read via the AppPlatform asset reader ([0xddd7e0] vtable+0x38) in sub_d65cc. "
     "Source text = for each define (std::set, sorted) \"#define NAME\\n\" (string '#define ' @0x231b94 in sub_cb16c) + file contents, prepended with std::string::insert(0,defines) (sub_10f04). No shader cache (every ShaderProgram compiles at load; programs cached in memory by name key in ShaderGroup+0x14 map).",
     ["strx hlsl/shaders/'#define ' -> sub_c9cd4+0x3ac..0x452, sub_cb16c+0x138", "sub_cb16c: reads JSON \"defines\" (@0x231b70) into a set; if DR.featureLevel < 0xa000 inserts \"R8G8B8A8_SNORM_UNSUPPORTED\" (@0x231b78)", "package listing: data/shaders/dx11/*.hlsl 24 files + ShaderConstants.fxh; data/shaders/*.vertex|*.fragment are GLSL (unused by D3D path)"],
     "high",
     notes="Defines seen in materials: LOW_PRECISION, FOG, NEAR_WATER, ALPHA_TEST, ALLOW_FADE, FANCY (+ per-file USE_OVERLAY, COLOR_BASED, USE_EMISSIVE, FLIP_BACKFACES, R8G8B8A8_SNORM_UNSUPPORTED). D3DReflect is used for the input layout (sub_d01b8) and CreateVertexShader (device slot 12 @0xd6d80) / CreatePixelShader (slot 15).")
fact("shader.feasibility", "data", None,
     "OFFLINE FEASIBILITY (arm32/v0_10_4/analysis/shader_probe_0104.py, d3dcompiler_47 x64, flags 0x8000, defines as the game builds them incl. R8G8B8A8_SNORM_UNSUPPORTED): 72 material variants -> 76 unique (shader,stage,defines) combos over 18 distinct shader files; "
     "ALL 76 compile at vs/ps_4_0_level_9_1 (0 failures), also at 4_0_level_9_3 and 4_0. The 6 unreferenced files (color_ex.vertex, color_texture.vertex, iteminhand.fragment, normal_as_color.vertex, uv1.vertex, uv_as_color.fragment) also compile at 9_1 with several define sets. "
     "Max SM4 instruction counts at 9_1: renderchunk.vertex 35, entity.vertex 40, entity.fragment 15, renderchunk.fragment 10, everything else <= 13. Real {9_1} device (hardware + WARP of the probe PC): CreateVertexShader/CreatePixelShader succeeded for 76/76 level_9_1 blobs.",
     ["python arm32/v0_10_4/analysis/shader_probe_0104.py --device --json shader_probe_0104_results.json", "extra_files.py for the 6 unused files"], "high",
     notes="Not tested on the real Surface RT (Tegra 3) driver. Sampler/interpolator/instruction limits did not trigger on the compiler; NPOT/mip, 2048 max texture and 1 render target remain runtime constraints of FL9_1.")
fact("shader.other_fl_consumers", "data", 0x000cb1d6,
     "Other readers of DR.featureLevel (+0x38) in game code, all `cmp r3,#0xa000`: sub_cb16c @0xcb1d6/0xcb1e0 (add R8G8B8A8_SNORM_UNSUPPORTED define), sub_d8e4c @0xd8e6a/0xd8e6c (round texture dims up to power of two, ceilf), sub_d1138 @0xd116e/0xd1170 (vertex field format 0x1c UNORM instead of 0x1f SNORM), plus getMajor/Minor/Suffix. 9_1 and 9_3 behave the same.",
     ["dis 0xcb1c8, 0xd8e5e, 0xd1160"], "high", ebytes=8)


fact("abi.com_slots_used", "abi", None,
     "COM vtable slots the game's graphics core calls (host fakes / hooks must honour these): ID3D11Device: 5 CreateTexture2D(0x14), 9 CreateRenderTargetView(0x24), 10 CreateDepthStencilView(0x28), 12 CreateVertexShader(0x30), 15 CreatePixelShader(0x3c), 39 GetDeviceRemovedReason(0x9c); "
     "ID3D11DeviceContext: 33 OMSetRenderTargets(0x84), 44 RSSetViewports(0xb0), 111 Flush(0x1bc); IDXGIDevice 7 GetAdapter(0x1c); IDXGIDevice1 12 SetMaximumFrameLatency(0x30); IDXGIDevice3 17 Trim(0x44); IDXGIAdapter 6 GetParent(0x18); "
     "IDXGIFactory2 24 CreateSwapChainForComposition(0x60); IDXGISwapChain1 8 Present(0x20), 9 GetBuffer(0x24), 13 ResizeBuffers(0x34); IDXGISwapChain2 34 SetMatrixTransform(0x88); IAsyncInfo 7 get_Status(0x1c), 9 Cancel(0x24); IDispatcherTimer 7 put_Interval(0x1c), 9 add_Tick(0x24), 11 Start(0x2c), 12 Stop(0x30); ICoreDispatcher 8 RunAsync(0x20); ISwapChainPanelNative 3 SetSwapChain(0xc).",
     ["decoded from the vtable offsets at the call sites listed in the other frame_render.* facts; slot numbers cross-checked against the documented interface order and replayed by swapchain_probe_0104.py on a real {9_1} device (slots 12, 24, 34, 9, 5, 8 executed)"],
     "high")

# ---------------------------------------------------------------- misc / hints for other areas
fact("hints.other_areas", "data", None,
     "Seen while tracing (owned by other areas): MainPage fields +0x5c SwapChainPanel (xbf name 'swapChainPanel'), +0x60 'Logo' Image, +0x64 'EditBox' TextBox, +0x68 'Button' (Canvas children), +0x6c input-worker IAsyncAction (ThreadPool item 0x1de0bc: panel.CreateCoreIndependentInputSource(7)+event hookup+ProcessEvents), +0x70 CoreInputSourceBase, +0x74/+0x78 shared_ptr<DeviceResources>, +0x7c AppMain*, +0x80 visible byte, +0x84 DispatcherTimer. "
     "InitializeComponent is the bl at 0x1dd02a (MainPage::vf9 0x1eddd8). AppMain::AttachToPanel 0x1da634 -> AppPlatform_Winrt::initialize 0x4b55c(platform, DisplayInformation, panel, EditBox, Button).",
     ["MainPage.xbf strings: swapChainPanel, Logo, EditBox, Button", "dis 1dcf5c", "dis 1da634"], "medium")

out = {"area": "frame_render", "facts": F,
       "port_instructions": [
           "ARCHITECTURE (this area): choose approach (b) 'faithful'. The frame loop, DeviceResources and Present are self-contained game code; the host only has to give them: (1) a fake Windows.System.Threading.ThreadPool.RunAsync that really runs the WorkItemHandler on a thread, with an IAsyncAction whose IAsyncInfo.Status reads Started until Cancel; (2) a fake DispatcherTimer that raises Tick every 50 ms; (3) a fake SwapChainPanel / FrameworkElement / CoreDispatcher / DisplayInformation (see swapchain.panel_requirements); (4) the D3D11CreateDevice and shader patches below.",
           "STARTUP ORDER the host must reproduce (MainPage ctor 0x1dcf5c does it): InitializeComponent(0x1dd02a) -> Window/CoreWindow/DisplayInformation event hookups -> add_CompositionScaleChanged / add_SizeChanged on the panel -> make_shared<DeviceResources>(0x1dce2c) [creates the D3D11 device only] -> AppMain ctor 0x1d9d14 [spawns the init work item] -> input worker item -> DispatcherTimer 50 ms Start. THEN the host MUST raise SizeChanged(NewSize=client size) on the panel exactly once after construction (and again on every WM_SIZE): that call (0x1df808) creates the swap chain, RTV, DSV and viewport. The Tick handler then (when AppMain+0x3c==1) runs AttachToPanel and StartRenderLoop.",
           "Per frame (all on the render thread, nothing for the host to do in approach (b)): lock AppMain+0x10 -> sub_50eac(platform) -> Minecraft::update 0x1d8a54 (tick + render) -> DeviceResources::Present 0x1dbb10 = IDXGISwapChain::Present(0,0).",
           "D3D11CreateDevice hook (IAT cell RVA 0x20b1a4): pFeatureLevels={0x9100}, FeatureLevels=1, DriverType HARDWARE, Flags unchanged (add 0x20 only if you want D2D); after the call, if hr>=0 and *pFeatureLevel==0x9100 store 0x9300 into *pFeatureLevel. The game then QIs ID3D11Device2/IDXGIDevice3/ID3D11DeviceContext2/ID3DUserDefinedAnnotation (all available on Win8.1/RT 8.1; probe QIs succeeded on a {9_1} device).",
           "Shader profile: patch the byte at RVA 0x2351f9 ('3' -> '1', string '_level_9_3' at 0x2351f0) with VirtualProtect RW + verify '_level_9_3' first; or hook D3DCompile (IAT 0x20b000). Also SetCurrentDirectory(game root) so STANDARD_FILE_INCLUDE finds data\\shaders\\dx11\\ShaderConstants.fxh (or supply an ID3DInclude).",
           "Swap-chain alternatives (pick one): (A, preferred, zero code patches) create HWND + IDCompositionDevice/Target/Visual in the host; fake ISwapChainPanelNative::SetSwapChain(sc) does visual->SetContent(sc); target->SetRoot(visual); dcompDevice->Commit(). CreateSwapChainForComposition (0x1db400) and SetMatrixTransform (0x1db57c) then work natively (probe: S_OK on {9_1}). Verify dcomp.dll on the Surface RT. (B) HWND swap chain: patch the dxgi IDXGIFactory2 vtable cell slot 24 (+0x60) with a stub that calls the original slot 15 CreateSwapChainForHwnd(dev,hwnd,desc,NULL,NULL,pp) and the IDXGISwapChain2 vtable cell 34 (+0x88) with `return S_OK`; or code-patch 0x1db57c (98 47 -> 00 20). Probe: HWND FLIP_SEQUENTIAL+STRETCH+BGRA+2 buffers accepted on Win11; unverified on Win8.1 RT.",
           "Fake values: ActualWidth/ActualHeight = client size (double DIPs), CompositionScaleX/Y = 1.0, DisplayInformation.CurrentOrientation = NativeOrientation = Landscape(1), LogicalDpi = 96. Do not raise DisplayContentsInvalidated. Device profile Default row halves the render size on >=1280x720 screens: patch float at RVA 0xdc8854 to 1.0f to render at native size (optional).",
           "If approach (a) (host drives the loop) is ever used instead: the host must run, in this order, on its own thread after SizeChanged has been delivered: App::init sub_471e8(App*, platformCell) [or vf7] once, set AppMain+0x3c=1, then loop { lock AppMain+0x10; sub_50eac(*(AppMain+0x38)); (*(App**)(AppMain+0x34))->vtable[19](); DeviceResources::Present(*(AppMain+4)) }."],
       "open_questions": [
           "ISwapChainPanel/IFrameworkElement exact vtable slots of the getters are inferred from call shapes (0x34/0x38 for ActualWidth/Height, 0x18/0x1c CompositionScale) - the faking agent must confirm against the real WinMD.",
           "Whether dcomp.dll DirectComposition works for a desktop HWND on Windows RT 8.1 (not testable here); fallback is the HWND swap chain (B).",
           "Whether flip-sequential + DXGI_SCALING_STRETCH is accepted by CreateSwapChainForHwnd on Win8.1/RT (accepted on the Win11 probe machine).",
           "The shader/probe results come from an x64 desktop compiler and a desktop driver forced to {9_1}; the Tegra RT driver was not exercised. 9_1 texture limit (2048) was not enforced by the probe machine (4096x4096 texture creation returned S_OK), so it cannot be used as evidence.",
           "Exact pSourceName string ('data/' + shader path) is inferred (sub_10d2c(&buf,\"data/\",name)); the directory part is confirmed only by the fact that ShaderConstants.fxh lives next to the .hlsl files.",
           "Not analysed: what writes MainPage+0x6c/0x70 beyond the input worker, InputPane handlers (0x1df700/0x1df714), AppPlatform_Winrt::initialize internals (0x4b55c), game-side D3D state creation (sampler/rasteriser/query objects) for FL9_1-specific restrictions (occlusion queries, anisotropy, depth-clip) - run-time constraints on 9_1 vs the 9_3 the game was built for.",
           "GetDeviceRemovedReason/ValidateDevice path (0x1dc0b4) only decoded structurally."]}
os.makedirs(r'D:\repo\Win32Craft-main\arm32\v0_10_4\facts', exist_ok=True)
with open(r'D:\repo\Win32Craft-main\arm32\v0_10_4\facts\frame_render.json', 'w', encoding='utf-8') as fh:
    json.dump(out, fh, indent=1)
print(len(F), 'facts written')
