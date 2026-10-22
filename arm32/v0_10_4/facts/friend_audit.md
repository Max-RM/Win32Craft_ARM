# friend_audit - audit of the friend's 0.10.4.40 attempt

Exe analysed: `MCPE.Host.WindowsPhone.exe` (stock, sha256 6c0a8adc...b29f3), `ARM32_EXE` set for every command. RVAs below are RVAs (IDA VA = 0x400000 + RVA); the friend's notes mix VAs (`sub_5DA5B4`) and RVAs. Machine-readable version: `friend_audit.json` (67 facts, ids `friend.*`; 41 CONFIRMED, 12 WRONG, 3 UNVERIFIABLE, plus findings/prediction/verdict). New labels were stored with `arm32re.py name` (AppMain::*, MinecraftClient::init, Minecraft::update, DX::DeviceResources::*, getShaderModelSuffix, ...).

## 1. Headline

The friend's analysis was **right about the object model and wrong about the control flow**, and that is why the window stays white:

* `AppMain::tick` (sub_5DA5B4 = RVA 0x1DA5B4) is **not a frame tick**. It is `AppMain::onKeyboardHeightChanged(int px)`, called only by the XAML `InputPane` Showing/Hiding handlers (0x1DF714 with `OccludedRect.Height`, 0x1DF700 with 0). `client+0x164` is that keyboard height in GUI pixels, not a delta in ms, and `Screen` vtable slot 30 is only overridden by text-input screens (ChatScreen, CreateWorldScreen, OptionsScreen, ControllerOptionsScreen, TextEditScreen). The host calls it with 16 every frame and believes it is running the game.
* The game's real frame loop is a **ThreadPool work item** (`AppMain::startRenderLoop` 0x1DA8A8 -> lambda 0x1DA160): `while (IAsyncInfo.Status == Started) { lock(AppMain+0x10); if (AppMain+0x3C ready) { AppPlatform::pumpInput; client->vtable[19] = Minecraft::update (0x1D8A54: ticks + render); DeviceResources::present (0x1DBB10: Present(0,0)) } }`. Nothing in the host ever reaches `Minecraft::update` => Draw=0, ClearRTV=0.
* The "client init is skipped" diagnosis is **wrong**: the **AppMain ctor already starts the client init asynchronously** (sub_1D9DD4 -> `IThreadPoolStatics::RunAsync(lambda_c7b7)`; the work item sets `client+0xC = platform holder`, calls `client->vtable[7]` = `MinecraftClient::init` (0x57B84, 2150 bytes: paths, terrain/items meta, `+0x118` resource store, ALL shader compiles, font into `+0x11C`, `Minecraft::init`, first screen) and then stores 1 into `AppMain+0x3C`). The host read `+0x118/+0x11C` and injected a StartMenuScreen ~0.1 s after the ctor, i.e. before the worker finished (the earlier "45 shaders in 3.4 s" run was this worker doing its job).

## 2. What the real XAML path does (ordered) and what the host skips

1. `Application::Start(callback)` (user main 0x1F42A4 -> 0x1EECBC, IApplicationStatics) -> `App2` -> Frame -> `MainPage` ctor (0x1DCF5C): **skipped** (host replaces the wrapper call at 0x200A72).
2. MainPage ctor: `make_shared<DX::DeviceResources>` (0x1DCE2C; D3D11CreateDevice inside) -> DR bound to the SwapChainPanel (DR+0x30) -> **AppMain** `new(0x40)` + ctor 0x1D9D14 (host does this part correctly) -> InputPane Showing/Hiding handlers, VisibilityChanged, DisplayInformation, composition-scale, SizeChanged, BackPressed handlers (host: none) -> 50 ms `DispatcherTimer` (MainPage+0x84, `put_Interval(0x7A120)`, pool 0x1DD870) .
3. AppMain ctor (0x1D9DD4): holder + `AppPlatform_Winrt` (0x88) + `MinecraftClient` (0x168) + app/data paths into `client+0x78/+0x90` + **RunAsync(init job)**. Host: executes it, does not wait.
4. Timer tick `sub_1DF8A4` (every 50 ms until `AppMain+0x3C`): when ready -> Stop timer, flip a XAML element's visibility (sub_1E1CF0), **`AppMain::onPanelSizeChanged` (0x1DA634)**: `AppPlatform_Winrt::initialize` (0x4B55C: DPI/scale into platform+0x38/+0x3C/+0x40, `HID::Initialize` input hookup), panel `ActualWidth/ActualHeight`, platform width/height (+0x30/+0x34), `client->setSize(w,h,scale)` -> **`AppMain::startRenderLoop`**. Host: none of this; it hand-writes `client+0x20/0x24` and the two GUI-scale globals.
5. Steady state: render loop on the worker thread + SizeChanged -> `DeviceResources::CreateWindowSizeDependentResources` (0x1DC0B4) + VisibilityChanged -> start/stop loop.

## 3. Single most likely next blocker (after the StartMenuScreen crash)

If the injection is removed / the host waits for `AppMain+0x3C`, the next hard stop is: **nothing starts the render loop, and the existing fake `IAsyncInfo::get_Status` returns Completed(1) forever** (win32_host.c:5046; Cancel is a no-op) while the game's loop only runs while status == Started(0) - it would exit on the first iteration. After that: platform scale (+0x3C/+0x40) and client size are 0 until `AppPlatform_Winrt::initialize` + `setSize` are replayed; then, on the Surface RT, the 9_1 wall (section 6).

## 4. Patcher review (RVAs recomputed from the real PE, not the comments)

| item | verdict |
|---|---|
| input size 14,759,424 / sha256 / ARMNT / base 0x400000 / 6 sections / 24 import descriptors (dir 0x33AC88/0x1F4) | CONFIRMED |
| DllCharacteristics 0x9140 -> 0x8140 (file offset 0x17E) | CONFIRMED |
| new section `.d3imp` RVA 0xE6E000, raw 0xE13600, VS 0x24C, RS 0x400, SizeOfImage 0xE6F000, header room (table ends 0x330 < 0x400) | CONFIRMED |
| import dir -> 0xE6E000/0x208, descriptor 24 = {ILT 0xE6E22C, name 0xE6E208, IAT cell 0xE6E234, hint 0xE6E218} | CONFIRMED |
| call site RVA 0x200A72 / file 0x1FFE72 = `bl 0x203EFC` (`03f043fa`), inside `__scrt_common_main_seh`, after `_initterm`; result r0 stored as exit code | CONFIRMED |
| trampoline 0xE6E23C: `ldr.w ip,[pc,#8]; add ip,pc; ldr.w ip,[ip]; bx ip` + literal 0xFFFFFFF0 -> cell 0xE6E234; patched BL decodes to `bl 0xE6E23C` | CONFIRMED (PIC, LR preserved) |
| section flags 0xE0000060 (read/write/**execute**) | value CONFIRMED, **risk**: our proven 1.2.8 layout is RW only (0xC0000040) with the trampoline in .text. Loaded on Win11 ARM64 (proves nothing for RT code integrity). Do not reuse. |
| VCLibs 120 DLLs `0x1140 -> 0x0140` | CONFIRMED on the three DLLs in TEST; their api-set imports (rtlsupport-l1-2-0, fibers-l1-1-1, heap-l1-2-0, debug-l1-1-1, libraryloader-l1-1-1, processenvironment-l1-2-0) and the exe's errorhandling-l1-1-2 / localization-l1-2-1 are not in our RT 8.1 loadcheck log: run `loadcheck` on the device |
| IAT cells 0x20B000 D3DCompile, 0x20B004 D3DReflect, 0x20B1A4 D3D11CreateDevice, 0x20B1AC CreateDXGIFactory1, 0x20B5D4 _CxxThrowException, 0x20B5E0 operator new, 0x20B5FC GetActivationFactoryByPCWSTR | all CONFIRMED |

Loadable PE32 ARM image: yes (structure valid; the host log shows it mapped at 0x540000 / 0x2D0000 with `sections=7`). RT-specific loadability (W+X section, apisets) is unproven.

## 5. Host-code review (win32_host.c, host_is_0104 block)

* Calling conventions / Thumb bit: fine (`make_dr(out)`, `AppMain ctor(this, shared_ptr*)`, StartMenu ctor(this), setScreen(client, screen), setSize(client,w,h,float s0) all match the callee code; operator new from IAT cell has the Thumb bit). The defect is *which* function is called and *when*.
* Data race: host writes `client+0x20/0x24` and `.data` floats and reads client fields while the init worker is inside `Minecraft::init -> setSize`; no use of the AppMain critical section (`AppMain+0x10`) that the real code takes around every DR/platform call.
* `SetMatrixTransform` skip at 0x1DB56C: bytes `fb6b17f10801` -> `00 20 00 f0 19 b8` are right (branch lands on 0x1DB5A4), but (a) the stated reason is wrong: desktop `IDXGISwapChain2` slot 34 (0x88) **is** `SetMatrixTransform` (SetMaximumFrameLatency is 31); it fails because the chain is not a composition chain; (b) it is a code patch with `PAGE_EXECUTE_READWRITE`, risky on Windows RT. Prefer overriding vtable slot 34 of the HWND chain.
* "first clear" in `windowSizeChanged` does not exist (`0x1DB158` has no `ClearRenderTargetView`); back buffer is undefined => plausible contributor to the white window.
* Host `Present(1,0)` fallback every frame conflicts with the engine's own `Present(0,0)` from the render thread.
* `host_is_128 = TRUE` hack reuses the 1.2.8 fakes; the 0.10.4 exe also needs InputPane, Window, DisplayInformation, DispatcherTimer, HardwareButtons, VibrationDevice, CurrentAppSimulator, Web.Http, NetworkInformation surfaces (README_V0104).
* No input forwarded at all; `WM_SIZE` ignored; COM is MTA on the main thread while the real app is on the UI STA (ok only because every UI class is faked).
* D3D: game's device-creation list is `{B100,B000,A100,A000,9300}` with flags 0 and no 9_1 (0x1DAFC0). `getShaderModelSuffix` (0xD73FC) **throws** for 9_1/9_2 (only `_level_9_3` and >=10_0 accepted). The friend's default (no clamp) hid this on Win11; on the Surface RT CreateDevice fails, and clamping to 9_1 hits the throw.

## 6. 9_1 requirements (user constraint, never addressed by the friend)

Rewrite the level array at the `D3D11CreateDevice` call (0x1DB028) in our IAT hook (append/replace 0x9100, add BGRA flag); patch `getShaderModelSuffix` (0xD73FC, bytes `2de990480df1080b`) to produce `_level_9_1`; verify the shaders against 9_1 limits (the game only ever requests `_level_9_3`).

## 7. Reuse verdict

**Reliable seeds (re-derived here):** exe identity, all IAT cells, boot chain, call site 0x200A72 and trampoline mechanics, section geometry (not the flags), App2/MainPage/AppMain/DeviceResources vtable RVAs and sizes, `MainPage+0x74/+0x7C`, `make_shared<DR>` ABI + DR field offsets (+0 annotation, +4 device(D3D11.1), +8 context(D3D11.1), +0xC chain, +0x10 RTV, +0x14 DSV, +0x30 panel gate, +0x38 FL, +0x4C/+0x50, +0x54/+0x58, +0x60/+0x64, +0x74 owner), CreateDeviceResources level list, DR global 0xE2AFC4, GPU whitelist table (RVA 0xDC8850, stride 0x14, Adreno names), WSD structure, the SetMatrixTransform skip *bytes*, client 0x168 / ctor 0x54E90 / vtable 0x33FFCC / factory 0x1D9CC0, setScreen and setSize behaviour, StartMenuScreen (vtable 0x34266C, ctor 0x995EC, 0x388), GUI-scale globals (0xDDE814 / 0x340D40).

**Discard:** `AppMain::tick` role, `client+0x164 = delta`, `Screen::update = vtable[30]`, "runFrame" naming of 0x1DA634, "SetMaximumFrameLatency" explanation, "first clear", the `sub_45D31C` "setter" (it is `ImageButton::ImageButton`), "vf32 = start menu" (it is a leave-game helper that creates StartMenuScreen only when arg2 != 0), "host skips the client init", "frame driving via DispatcherTimer", "FL 9_3+" reasoning, the class name "DisplayResources" (RTTI: `DX::DeviceResources`). **Unverifiable:** shader count/timings, frame timings, exact reason for the white colour. **All friend code:** discard (RWX patch section, RWX code patch, host_is_128 hack, hard-coded 1280x720, wrong frame driver).

## 8. Not found / uncertain

* The second crash (worker thread T317C: `setSize -> sub_915E8 -> StartMenuScreen::vf3 -> texture map NULL`): which code had installed the screen on the worker is unresolved (race with the host injection is the likely cause).
* Class/role of the XAML objects at `MainPage+0x5C/+0x60/+0x64/+0x68` and the exact interface behind IID 676D0BE9-B65C-41C6-BA40-58CF87F201C1 (ActualWidth/ActualHeight via slot 0x5C) were inferred, not decoded; IID D160CE46-... used as IDispatcherTimer (Stop = slot 12) is inferred from the slot usage.
* IID 5C526665-F60E-4912-AF59-5FE0680F089D (native swap-chain-panel interface, VA 0x6DF1F8) not matched to a documented name; irrelevant while the host pre-fills `DR+0xC`.
* Whether RT 8.1 accepts a W+X section / RWX code patch, and serves the extra api-set contracts: untested.
* Whether the game's HLSL runs at `_level_9_1`: untested.
* Why the window is exactly white: only the structural reasons are proven (no clear, no render, no paint).
