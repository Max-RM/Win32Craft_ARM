# MCPE 0.10.4.40 (WP8.1 ARM32) - recon summary for the owner

Sources: 7 fact areas in `facts/` (each finder output was re-checked by an independent verifier: 434 verdicts = 391 CONFIRMED, 28 CORRECTED, 2 REFUTED, 13 UNVERIFIABLE), the friend's `WORK_SUMMARY.md`, and my own re-derivations with `arm32re.py` (ARM32_EXE set to the 0.10.4 exe). All addresses are RVAs (IDA VA = RVA + 0x400000). The friend's notes mix VAs and RVAs. No host code was written.
Re-derived by me on the exe: render-loop body 0x1DA160 (spins, no sleep, only works while `IAsyncInfo.Status==Started`), `AppMain::SetKeyboardHeight` 0x1DA5B4 (the friend's "tick"), the DR gate `[DR+0x30]==NULL -> return` at 0x1DB178, `getShaderModelSuffix` 0xD73FC (accepts only 0x9300 -> "_level_9_3" or >=0xA000, throws "Unkonwn feature level" otherwise) and its string at 0x2351F0 (exactly ONE xref, so a 1-byte patch is safe), trampoline slot 0x20A364 (zero bytes, fits), TabTip focus state (`movs r1,#3` = Programmatic, at 0x11091E), pumpInput deadline = VibrationDevice (not StatusBar), client+0xAC = ExternalFileLevelStorageSource.

## 1. The friend: right / wrong / missed

**Right (reusable as hints, all re-derived):** exe identity, all IAT cells (0x20B000 D3DCompile, 0x20B1A4 D3D11CreateDevice, 0x20B5FC activation, 0x20B5E0 new), patch site 0x200A72, `AppMain` = 0x40 bytes / ctor 0x1D9D14, `make_shared<DR>` 0x1DCE2C, client = 0x168 bytes / ctor 0x54E90 / vtable 0x33FFCC, StartMenuScreen crash chain through `client+0x118`, SetMatrixTransform skip location (0x1DB56C).

**Wrong (audit: 12 of 67 claims WRONG, 6 more corrected):**
* `AppMain::tick` (0x1DA5B4) is `SetKeyboardHeight(px)`, called only by the InputPane Showing/Hiding handlers. They called it with 16 every frame and believed it ran the game. `client+0x164` is a keyboard height, not a delta.
* "Client init is skipped" is false: the AppMain ctor already queues `MinecraftClient::init` on the ThreadPool (`+0x118` texture store, `+0x11C` font, all shaders, first screen, then sets `AppMain+0x3C`). Their host injected a StartMenuScreen and `setSize` while the worker was still inside init (data race; the second crash on thread T317C). The "45 shaders in 3.4 s" run was that worker doing its job.
* Their WSD claim ("returns at once because DR+0x30 is NULL") is true of the binary but REFUTED for their run (they stored a dummy pointer there). No clear, no render, no frame ever ran.
* Discard names/roles: "DisplayResources" (= `DX::DeviceResources`), `sub_5DA634` "runFrame" (= `AttachToPanel`, once), `sub_45D31C` "setter" (= ImageButton ctor), "vf32 = start menu" (= leave-game helper), SetMaximumFrameLatency explanation, "first clear" (does not exist), Device1/Context1 (really Device2/Context2).

**Missed:**
* The real frame loop is a High-priority ThreadPool item that the host never started. Their fake `IAsyncInfo.get_Status` returns Completed forever, so the loop would exit at once.
* Input registers nothing until `Tick -> AttachToPanel -> HID::Initialize`, so their host had zero input.
* The nine MainPage handlers (SizeChanged, Visibility, Dpi, Orientation, CompositionScale, DisplayContentsInvalidated, BackPressed, InputPane x2) and the 50 ms Tick gate.
* FL 9_1: the game never asks for 9_1 and `getShaderModelSuffix` throws for it. Also the half-resolution GPU table, shader cwd/include, ASCII-only paths, XAudio2, MemoryManager, the 8 unproven RT api-sets.
* Their patcher uses an RWX `.d3imp` section and an RWX code patch (risky on RT); `host_is_128` reuses the 1.2.8 fakes. Do not reuse.

**Why white:** not provable offline. The window class brush is black and WM_ERASEBKGND returns 1, so white is probably an uncleared back buffer or a Not-Responding ghost after their AV. Structurally no frame ever ran.
**Verdict: use nothing of their code. Their notes are hint material only.**

## 2. The real picture (boot / frame / input / platform)

1. CRT entry 0x20095C -> `_initterm` (vccorlib `InitializeData(2)` = MTA) -> `bl 0x203EFC` at 0x200A72 (our patch point) -> user main 0x1F42A4 -> `Application::Start(cb)` 0x1EECBC (IApplicationStatics slot 7).
2. Start blocks in the XAML runtime. Real flow: `cb->Invoke` -> `App2` (aggregates Application via `IApplicationFactory.CreateInstance`) -> `OnLaunched` 0x1E3A30 (hide StatusBar, `new MainPage`, `Window.Content=page`, `Activate`).
3. MainPage ctor 0x1DCF5C (1.9 KB, thin glue): `InitializeComponent` (LoadComponent + `FindName` x4: `swapChainPanel`, `Logo`, `EditBox`, `Button`), 9 delegate registrations, `make_shared<DX::DeviceResources>` (D3D11CreateDevice inside, global 0xE2AFC4), input work item, `new AppMain`, 50 ms DispatcherTimer.
4. AppMain ctor -> `createApp`: `AppPlatform_Winrt` (0x88) + `MinecraftClient` (0x168) + paths (Package.InstalledLocation, ApplicationData.LocalFolder) + `ThreadPool.RunAsync(init job)`. The job runs `runInit` 0x471E8 -> `client->init` 0x57B84 (textures `+0x118`, font `+0x11C`, ~76 shader variants via D3DCompile, XAudio2, StartMenuScreen created here), then writes `AppMain+0x3C=1`.
5. XAML layout raises `panel.SizeChanged` -> 0x1DF808 -> `DR::SetSwapChainPanel` -> `CreateWindowSizeDependentResources` 0x1DB158 (composition swap chain, panel `SetSwapChain` through `CoreDispatcher.RunAsync`, `SetMatrixTransform`, RTV/DSV/viewport) -> `AppMain::OnWindowSizeChanged` -> `client->setSize`.
6. The 50 ms Tick (0x1DF8A4) waits for `AppMain+0x3C`, then stops, runs `AttachToPanel` 0x1DA634 (`HID::Initialize`, DPI/scale into platform+0x38/3C/40, `setSize` from `panel.RenderSize`) and `StartRenderLoop` 0x1DA8A8. The Tick is a one-shot "ready" gate, not a frame driver.
7. Frames: ThreadPool body 0x1DA160 loops `lock; pumpInput 0x50EAC; client->vtbl[19] = Minecraft::update 0x1D8A54; DR::Present 0x1DBB10 (Present(0,0))` while `IAsyncInfo.Status==Started`. It busy-spins (no sleep), even before ready. Present is the game's own; the host must not add one.
8. Input is touch-only (`useTouchscreen` always true, cell 0x33FB88). All input goes through `HIDController`: CoreWindow KeyDown/KeyUp, pointer Pressed/Moved/Released (on a CoreIndependentInputSource of the panel, via a ThreadPool item that blocks in `ProcessEvents`), `TextBox.TextChanged`. Mouse works as a touch contact. Events are queued and drained by `pumpInput` on the render thread. Always send Released for every Pressed.
9. Keyboard: show = `Focus(Programmatic)` on a hidden XAML TextBox (lambda 0x110BE8), hide = `Focus(Programmatic)` on the Button (0x1108D4). The game never calls an InputPane hide. Height reaches the game only via InputPane Showing/Hiding -> `SetKeyboardHeight`. Enter must arrive as CR+LF in the TextBox text.
10. Platform surface is small: 36 activation sites through one choke point (IAT 0x20B5FC): XAML/UI (Application, Page, Window, DispatcherTimer, Canvas, StatusBar, InputPane, HardwareButtons, DisplayInformation, Uri, CoreWindow/CoreCursor) plus Package, ApplicationData, MemoryManager, ThreadPool, Vibration, NetworkInformation, CurrentAppSimulator, AsyncCausalityTracer. Windows.Web.Http is dead code. No StorageFile API: all file I/O is CRT/Win32.
11. Assets are plain files under `<InstalledLocation>/data`; saves under `<LocalFolder>/games/com.mojang/minecraftpe`. Both paths are narrowed by truncation, so ASCII only. Shaders are HLSL files compiled at runtime by D3DCompile with cwd-relative includes, so cwd must be the game root.
12. Audio is XAudio2_8 (#1, PCM tables inside the exe). Network is RakNet over Winsock UDP (native). No FMOD, no GameInput.
13. Device loss is unrecoverable in the game (AppMain's IDeviceNotify slots are empty): treat DXGI removed/reset as fatal.

## 3. Architecture decision: FAITHFUL (b)

Run the game's own `Application::Start` -> App2 -> OnLaunched -> MainPage -> AppMain code, unmodified, against host fakes. Exe patches limited to the 1-byte profile patch and the entry trampoline; everything else is IAT/activation hooks.

Reasons (from verified facts):
* The two finders disagreed (startup_flow: faithful; client_init: hybrid). I re-derived the cost split. A hybrid/replay still needs the same hard fakes (ThreadPool + IAsyncInfo, ApplicationData, Package, MemoryManager, Window.Current/CoreWindow, DisplayInformation, SwapChainPanel incl. RenderSize/dispatcher/SwapChainPanelNative, TextBox/Button, ISwapChainPanel scale). It saves only about ten trivial fakes (Application, Page, Uri/FindName, DispatcherTimer, InputPane, HardwareButtons, StatusBar, Canvas), each "store delegate, return S_OK".
* Replay costs exact bookkeeping (AppMain lock order, DR listener, SizeChanged-before-Tick ordering, DPI/orientation/visibility/back/resume/composition-scale handlers). Missing exactly that is how the friend failed. Faithful gets these free and identical to a phone.
* TabTip: the only path from keyboard to game is MainPage's InputPane handlers and HID. Faithful keeps that path real, so the TabTip state machine can be built on the fake TextBox/InputPane without inventing glue.
* Game-side MainPage logic after InitializeComponent is small (about 3.7 KB, 6 AppMain entry points, 8 DR setters), touching about 30 interfaces / about 80 slots, all simple getters, event adders, or fire-and-forget calls. This satisfies the README decision rule.
* Escape hatch (hybrid-lite, only if App2 aggregation misbehaves): skip `Application::Start`/App2 and call the MainPage ctor 0x1DCF5C directly, then `Window.Content`. All other fakes stay the same. Do not hand-drive AppMain.

Rejected: pure replay (friend's approach, needs every MainPage step re-implemented by hand); blocked (nothing is blocking).

## 4. Feature level 9_1 verdict

**Works, with patches, for shaders. Hardware behaviour is unproven.**
* All 76 unique shader variants (72 material variants, 18 files) compile at `vs/ps_4_0_level_9_1` with 0 failures (max 40 SM4 instructions). All 76 load via CreateVertex/PixelShader on a forced-{9_1} desktop device and on WARP. Not tested on Tegra.
* Must patch:
  1. `D3D11CreateDevice` IAT 0x20B1A4: levels = {0x9100}, then write 0x9300 into `*pFeatureLevel` (the game then takes its `_level_9_3` path; other FL checks only test `< 0xA000`). Flags 0 worked in the probe; BGRA flag is optional (finder and friend-audit disagreed, probe says not needed).
  2. Profile string byte at RVA 0x2351F9: `0x33 -> 0x31` (`_level_9_3` -> `_level_9_1`; verify `5F 6C 65 76 65 6C 5F 39 5F 33 00` at 0x2351F0 first). Alternative: hook D3DCompile (IAT 0x20B000) and rewrite `pTarget`. Without a patch, clamping to 9_1 makes init throw, so `AppMain+0x3C` is never set.
  3. `SetCurrentDirectory(game root)` (or ID3DInclude hook) for `ShaderConstants.fxh`.
  4. Optional: resolution table float at 0xDC8854 `0.5 -> 1.0`. The "Default" GPU row renders at half size when the output is >= 1280x720.
* Unexamined 9_1 limits: 2048 texture cap, 1 render target, 32-bit index buffers (the game maps element size 4 to R32_UINT), anisotropy, occlusion queries, depth clip, NPOT. These matter at the world milestone, not the menu.

## 5. Implementation plan (ordered milestones)

**M0 Pre-flight (offline + device).** Table-driven patcher from `pe_patch_plan.json` (`patch_profile`): RW-only `.d3imp` section 0xE6E000, SizeOfImage 0xE6F000, trampoline at 0x20A364 (in `.text` padding), call site 0x200A72, clear APPCONTAINER, VC120 `*_app` DLL copies (0x1140 -> 0x0140). Run `loadcheck` on the RT for the 8 unproven api-sets (errorhandling-l1-1-2, localization-l1-2-1, debug-l1-1-1, fibers-l1-1-1, heap-l1-2-0, libraryloader-l1-1-1, processenvironment-l1-2-0, rtlsupport-l1-2-0), plus XAudio2_8 and dxgi. Have: full patch table, byte checks, DLL edit table. Missing: device loadcheck result, on-device behaviour of the VC120 DllMains/436 static ctors.

**M1 Menu visible.** Facts: `startup_flow`, `frame_render`, `platform_services`, `pe_patch_plan`.
* Hooks: activation router 0x20B5FC, 0x20B1A4, profile byte, cwd, ASCII data dir.
* Fakes: Application.Start with a real UI-thread loop (dispatcher queue + 50 ms timer); IApplicationFactory/IPageFactory aggregation (`CreateInstance(outer,&inner,&instance)`; AddRef outer; inner must QI IApplication); Window/CoreWindow (answer IAgileObject); StatusBar; Uri/LoadComponent no-op/FindName returning 4 elements; panel (IFrameworkElement slots 13/14/47/51, ISwapChainPanel 6/7/8/10, IUIElement 22/23, IDependencyObject 11, SwapChainPanelNative 3); DisplayInformation (Landscape, 96 dpi); ThreadPool + IAsyncAction/IAsyncInfo (Started until Cancel; catch worker exceptions; yield/below-normal for the busy loop); ApplicationData (LocalFolder slot 12, LocalSettings slot 10 -> Values slot 8, non-NULL); Package; MemoryManager (limit-usage > 0x1EA00000); InputPane; HardwareButtons; Canvas; DispatcherTimer; CoreDispatcher.RunAsync (queue and run on the UI thread). Every fake: QI/AddRef/Release, IInspectable slots, S_OK everywhere, named stubs that log unknown slots.
* Ordering: raise the first SizeChanged only after `client+0xC != NULL` (else crash at 0x58718) and before the first Tick sees ready; start the timer after the first SizeChanged; `ActualWidth/Height >= 0`, scale non-zero.
* Presentation: path A = HWND + DirectComposition, fake `SetSwapChain` = visual `SetContent` (CreateSwapChainForComposition and SetMatrixTransform then run unmodified). Path B = patch dxgi vtable DATA cells (Factory2 slot 24 -> CreateSwapChainForHwnd stub, SwapChain2 slot 34 -> S_OK). No code patch at 0x1DB56C.
* Done when the log shows: init job finished, `AppMain+0x3C=1`, `AttachToPanel`, render loop running, the start menu on screen.
* Still missing: `AppPlatform_Winrt::initialize` 0x4B55C and `HID::Initialize` internals only inventoried; `IApplicationFactory` refcount/aggregation behaviour (m9); real vccorlib behaviour on RT; exact IID/slot confirmation of a few inferred interfaces (IDependencyObject {5C526665}, HardwareButtons slot 6) against the real WinMD.

**M2 Input + TabTip.** Facts: `input_text` (+ corrections: FocusState is Programmatic 3, Enter = CR+LF, fire TextChanged once after any game `put_Text`, `pumpInput` deadline is vibration).
* Fake CoreWindow/CoreIndependentInputSource/PointerEventArgs (PointerPoint, PointerDevice, Properties.IsLeftButtonPressed), KeyEventArgs; map WM_POINTER/mouse/key to delegate Invoke from the UI thread; Back = Esc -> BackPressed (Handled stays false on the start menu).
* TabTip (shares the owner's open 1.2.8 problem): make the host LEVEL-TRIGGERED. Desired state = `AppPlatform_Winrt+4` / `HID+0x44`. Show on `Focus` of the fake TextBox; hide on `Focus` of the Button. Publish `OccludedRect` before raising Showing; raise Hiding(0) only after TabTip is observed gone; debounce about 150-300 ms; never from the game thread. The game never issues an explicit hide, so a host that waits for one leaves the keyboard open.
* Missing: `Options+0x276` default; text-diff corners at medium confidence; real TabTip behaviour on the device (needs the telegArm lessons; not part of this recon).

**M3 World.** Menu -> create/load world -> chunks render. Needs: 9_1 limits check on chunk/entity shaders and index buffers, vibrate bypass (cell 0x33FB94 -> image+0xBF61), LevelDB `WinRTEnv` (native), `Options` memory tier, GUI scale (globals 0xDDE814 / 0x340D40). Missing: client vtable slots 9-12/33/34/17/39, item/tile registration calls at the end of `Minecraft::init2` (0x19B528, 0x1CC2C0, 0x15A800, 0x163384, 0x1BACA4), identity of `client+0x68`, world auto-load branch (`options[0x290]`), the game at 9_1 on Tegra.

**M4 Audio / network / lifecycle.** XAudio2_8 real (fallback: no-op cells 0xDB804C / 0xDB8050 if `XAudio2Create` fails; `play` has no null check); RakNet UDP real; cells 0x33FB6C (network) / 0x33FB7C (buy) no-op; suspend/resume/visibility via MainPage handlers (0x1DF9AC, 0x1E3C34, 0x1E3DA0, client vtable 2..5); resize/WM_SIZE through SizeChanged; DPI changes. Missing: suspend/resume helpers 0x4743C/0x4736C/0x473D4/0x474A4 only located.

## 6. Top risks
1. **Loader on RT 8.1:** 8 unproven api-sets, patched `*_app` DLLs with broken Authenticode (needs the same relaxed code policy as 1.2.8), 436 static ctors and vccorlib init never run on RT. Only the device decides.
2. **Fake COM surface (about 80 slots):** C++/CX delegate protocol, QI matrix, aggregation, and "any HRESULT<0 becomes a thrown Platform exception". A wrong slot crashes far from its cause. Build the fakes with named, logging stubs and fail loudly.
3. **Threading and ordering:** init job vs first SizeChanged vs Tick; exceptions escaping worker items; busy-spinning render loop; main thread is MTA while the real UI thread is STA; the game mutates `client+0x20/0x24` concurrently if SizeChanged comes early.
4. **Presentation on RT:** dcomp.dll on RT 8.1 and flip+STRETCH on an HWND chain are unverified; fallback B needs dxgi data-cell patches. No waitable pacing, so occluded or minimised windows spin.
5. **9_1 on Tegra:** shader blobs only proven on desktop/WARP; limits above unexamined.
6. **Environment traps:** non-ASCII install/profile path, wrong cwd (shaders), XAudio2 failure with no null check, device loss = restart.
7. **Trust:** about 6 interface slots are inferred (medium confidence). Every inferred slot should be logged on first call.

## 7. Effort relative to the 1.2.8 port

Roughly **0.5-0.6x** of the 1.2.8 effort (my estimate, not measured). Cheaper: 23 activated classes instead of 67, no FMOD, no GameInput/Xbox, 76 instead of 197 shader variants, plain CRT file I/O, small game-side MainPage, the toolkit and PORT_RULES already exist, and the recon is already done. More expensive per item: a new fake domain (XAML-style COM objects with events, aggregation, a real UI-thread loop), the thread-pool frame driver, composition presentation. M1 is about half the total and carries most of the risk; once the menu renders, M2-M4 are mostly routine. M0 and any RT-specific surprises are not included and cannot be estimated offline.
