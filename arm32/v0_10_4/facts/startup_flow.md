# startup_flow - genuine boot of MCPE 0.10.4.40 (WP8.1 XAML app, ARM32)

All RVAs are image-relative (IDA VA = 0x400000 + RVA; friend names like `sub_5DCE2C` are VAs -> RVA 0x1DCE2C). Code RVAs are even; pointers stored in data carry the Thumb bit.
ABI = AAPCS-VFP (hard float): COM methods get `this` in r0, floats/doubles in s*/d* (e.g. `Canvas.SetLeft(elem, double)` -> r1=elem, d0=len), 64-bit ints in r2:r3 (`put_Interval(TimeSpan)`), `Size`/`Rect` are passed/returned through pointers.
Every vtable slot below was read from the disassembly and cross-checked against the Windows SDK 10.0.19041 `.idl` vtable order (WP8.1 interfaces missing from the SDK - HardwareButtons - are marked). Machine-readable twin: `startup_flow.json` (90 facts).

## 0. Answer in five lines
1. The app is **Application::Start(callback) -> App2 (Application subclass) -> OnLaunched -> `new MainPage` -> Window.Content = page -> Activate**. There is no Frame, no navigation, no CompositionTarget.Rendering and no SwapChainPanel render event.
2. `MainPage` is **thin glue** (ctor 1888 B + ten handlers <= 250 B + InitializeComponent 854 B, ~3.7 KB total). All engine objects are created from the ctor: `DX::DeviceResources` (the friend's "DisplayResources"), the input work item, `AppMain` (which creates `AppPlatform_Winrt` and `MinecraftClient`) and a 50 ms `DispatcherTimer`.
3. **Frames are NOT driven by XAML.** A ThreadPool work item (`AppMain::StartRenderLoop`, body RVA 0x1DA160) loops `platform->update(); client->vf19 (Minecraft::update); DeviceResources::present` until cancelled. The DispatcherTimer is only a one-shot "client is ready" gate.
4. The friend's blank/white window has three concrete causes (section 7): `AppMain::tick sub_5DA5B4` is really *setKeyboardHeight* (they called it with 16 every frame), `DisplayResources::windowSizeChanged sub_5DB158` returns immediately while `DR+0x30` (the SwapChainPanel) is NULL, and `MinecraftClient::init` (vf7, 0x57B84) is never run (it fills client+0x118/+0x11C and is the only thing that sets `AppMain+0x3C`).
5. **Faithful option (b) is feasible**: the game's own App2/MainPage code can run unmodified against ~30 small fakes (about 80 vtable slots, table 2a). Details in sections 3-6.

## 1. Ordered boot sequence (genuine flow)

| # | step | RVA (function) | ABI / what it does | objects created |
|---|---|---|---|---|
| 1 | PE entry | 0x20095C (entry field 0x20095D) | `/GS` cookie init (0x200E6C) then `b.w 0x200970` | - |
| 2 | CRT startup | 0x200970 | `_initterm_e(table 0x20BDA0..0x20BDB4)`: sub_200878 (CRT locale/fmode; RoInitialize(1) only if [0xE2B03C]!=0, it is 0), sub_2004F8, **sub_1FFECC = vccorlib `Platform::Details::InitializeData([0xDC9F5C]=2)`**; `_initterm(table 0x20B6C8..0x20BD9C)` C++ statics; parses cmdline; **`bl 0x203EFC` at 0x200A72** with r0=0x400000, r1=0, r2=cmdline tail, r3=0 | - |
| 3 | WinMain thunk -> wrapper | 0x203EFC -> 0x20405C | `GetCmdArguments`, builds `Platform::Array<String^>` (Heap::Allocate(0x30,0x48), HSTRINGs via 0x1EDF0), calls user main | Array<String^> |
| 4 | user `main(Array^)` | 0x1F42A4 | `Heap::Allocate(0xC,0x58)`; `sub_1EA914` ctor of `Windows::UI::Xaml::ApplicationInitializationCallback` (vtables 0xDC96C8/E0/FC, FunctorCapture lambda at +0x14, functor ptr [+0x54]); then `sub_1EECBC` | callback delegate (0x58 B) |
| 5 | **Application::Start** | 0x1EECBC | `GetActivationFactory(L"Windows.UI.Xaml.Application", IID_IApplicationStatics {06499997-F7B4-45FE-B763-7577D1D3CB4A})` (via 0x2001E4 -> IAT 0x20B5FC); `statics->vtbl[7](statics, cb)` = `Start(cb)`; real runtime blocks here until app exit | - |
| 6 | runtime calls `cb->Invoke(params)` | vtable 0xDC96C8 slot 3 = 0x1F2F64 | `if vtable==0xDC96C8: functor=[this+0x54]; functor->vtbl[1]()` = lambda body 0x1EE380; `params` unused; returns 0 | - |
| 7 | **App2 created** | lambda 0x1EE380 -> `Heap::Allocate(0x34,0x40)` -> **0x1E2B48 App2::App2** | sets 9 sub-object vtables (0xDC9158@+0, 9174@+4, 91B4@+8, 9198@+0xC, 9230@+0x10, 9294@+0x18, 92C8@+0x1C, 92EC@+0x20, 9304@+0x24); `sub_1E2CF0(this+8)`: `GetActivationFactory(Application, IApplicationFactory {93BBE361-BE5A-4EE3-B4A3-95118DC97A89})` + `CreateInstance(outer=this+8, &inner=this+0x14, &instance)` (factory slot 6, helper 0x1E3228); `add_Suspending` (IApplication slot 13) with `SuspendingEventHandler`->0x1E3DA0; `add_Resuming` (slot 15) with `EventHandler<Object>`->0x1E3C34 (wrappers 0x1E5F50/0x1E5EDC QI the inner for IApplication {74B861A1..}). The lambda then Releases its handle - the runtime keeps `outer` alive | App2 (0x40 B) |
| 8 | runtime delivers launch | QI(outer, IApplicationOverrides {25F99FF7-9347-459A-9FAC-B2D0E11C1A0F}) -> App2+0x10 (QI fn 0x1E4C84); `vtbl[7]` (+0x1C) of vtable 0xDC9230 = thunk 0x1E54D4 -> 0x1E5480 (checks disposed byte [App2+0x3C]) -> **0x1E3A30 App2::OnLaunched(this=App2, args)** | only OnLaunched is overridden; OnActivated/OnWindowCreated etc. (0x1E37C4...) are base forwarders | - |
| 9 | OnLaunched part 1 | 0x1E3A30 | StatusBar: `GetActivationFactory("Windows.UI.ViewManagement.StatusBar", IStatusBarStatics {8B463FDF})` slot 6 `GetForCurrentView` (0x1E3394) -> `IStatusBar::HideAsync` slot 7 (0x1E350C), result released | - |
| 10 | OnLaunched part 2 | 0x1E3A30 | `if ([App2+0x30]==0)`: `Heap::Allocate(0x88,0x94)` + **0x1DCF5C MainPage ctor**; store `[App2+0x30]` | MainPage (0x94 B) |
| 11 | MainPage ctor | 0x1DCF5C | see 1b below | everything |
| 12 | OnLaunched part 3 | 0x1E3A30 | `QI(args, IActivatedEventArgs {CF651713..})->get_PreviousExecutionState` (slot 7); if ==3 (Terminated) -> `ApplicationData::Current.LocalSettings (IApplicationData slot 10).Values (IApplicationDataContainer slot 8)` -> `MainPage::vf11` (0x1DF4C4) | - |
| 13 | OnLaunched part 4 | 0x1E3A30 | `QI(page, IUIElement {676D0BE9})`; `Window::Current` (IWindowStatics {93328409} slot 6, helper 0x1DAC80) -> `IWindow::put_Content` (slot 9, 0x24) -> `IWindow::Activate` (slot 20, 0x50) | - |
| 14 | layout / first events (runtime) | - | XAML layout runs, **`swapChainPanel.SizeChanged`** fires -> `MainPage::OnSizeChanged` 0x1DF808 (section 1c) | swap chain |
| 15 | init job (thread pool) | lambda 0x1DA2D8 | `sub_471E8(client, holder)`: `client[+0xC]=holder`; **`client->vtbl[7]()` = MinecraftClient::init 0x57B84**; `client[+8]=1`; `sub_47BF4(client, 0.0f)`; then **`AppMain[+0x3C]=1` (isReady) with DMB** | TextureManager (+0x118), Font (+0x11C), shaders, input holders, StartMenuScreen... |
| 16 | first successful Tick | 0x1DF8A4 | gate `AppMain::IsAppInitialized` (0x1DA310); `timer.Stop()`; `Logo.put_Visibility(Collapsed)`; **AppMain::AttachToPanel(panel, EditBox, Button)** 0x1DA634; `Logo` hidden again; **AppMain::StartRenderLoop** 0x1DA8A8 | HID, render IAsyncAction |
| 17 | steady state | render loop body 0x1DA160 | per iteration: `IAsyncInfo.get_Status==Started`; `lock(AppMain+0x10)`; if ready: `AppPlatform_Winrt::update` (0x50EAC), `client->vtbl[19]` = `Minecraft::update` (0x1D8A54), `DX::DeviceResources::present` (0x1DBB10: `swapChain->Present(0,0)`); `unlock` | frames |

### 1b. MainPage ctor 0x1DCF5C in call order (object `this`, 0x94 bytes)
1. vtables (0xDC8AF8@+0, 8B2C@+4, 8B50@+0x20, 8B6C@+0xC, 8B98@+8, 8BEC@+0x4C, 8C10@+0x50, 8C28@+0x54); zero +0x5C..+0x78; `+0x80 = 1` (visible); `+0x84 = 0`; `+0x8C = -1`.
2. `sub_1DD874(this+8)`: `GetActivationFactory("Windows.UI.Xaml.Controls.Page", IPageFactory {DFA149AC})` `CreateInstance(outer=this+8, &inner=this+0x24, &instance)` (slot 6).
3. **`MainPage::InitializeComponent` 0x1EDDD8**: guard `[this+0x58]`; `Uri(L"ms-appx:///MainPage.xaml")` (IUriRuntimeClassFactory slot 6, helper 0x1088A0); `Application::LoadComponentWithResourceLocation(this, uri, 0)` (IApplicationStatics **slot 9**, helper 0x1EE424); `QI(this, IFrameworkElement)->FindName` (slot 51, helper 0x1ED378) for `"swapChainPanel"`->QI ISwapChainPanel->[+0x5C], `"Logo"`->IImage->[+0x60], `"EditBox"`->ITextBox->[+0x64], `"Button"`->IButton->[+0x68]. (MainPage.xbf: Page/SwapChainPanel{Image Logo Assets/NewLogo.png, TextBox EditBox, Button Button}.)
4. `Window.Current` (0x1DAC80) -> `get_CoreWindow` (IWindow slot 10, helper 0x1DAACC) -> **`ICoreWindow` slot 56 add_VisibilityChanged** (delegate 0x1DF9AC, token on stack).
5. `DisplayInformation::GetForCurrentView` (IDisplayInformationStatics slot 6, helper 0x4AF58) -> **slot 14 add_DpiChanged** (0x1DF67C), **slot 8 add_OrientationChanged** (0x1DF76C).
6. `IDisplayInformationStatics` **slot 9 add_DisplayContentsInvalidated** (static, helper 0x1E183C; handler 0x1DF618).
7. `QI([+0x5C], ISwapChainPanel)` **slot 8 add_CompositionScaleChanged** (handler 0x1DF510).
8. `QI([+0x5C], IFrameworkElement)` **slot 47 add_SizeChanged** (helper 0x1E1720; handler 0x1DF808).
9. `HardwareButtons` statics (IID {594B8780-DA66-4FD8-A776-7506BD0CBFA7}) **slot 6 add_BackPressed** (helper 0x1E1798; handler 0x1DF4E8).
10. `InputPane::GetForCurrentView` (IInputPaneStatics slot 6, 0x1DE9CC) -> **slot 8 add_Hiding** (0x1DF700) then **slot 6 add_Showing** (0x1DF714).
11. **DeviceResources**: `sub_1DCE2C(&shared_ptr)` = `make_shared<DX::DeviceResources>` (new 0x84; D3D11CreateDevice happens in the ctor, section 3) -> `[this+0x74]/[this+0x78]`.
12. Input work item: `Allocate(0xC,0x58)` + `sub_1DC5F4` (WorkItemHandler, functor lambda_25d8f6e4 vtable 0xDC8E90, body **0x1DE0BC**) + `ThreadPool.RunWithPriorityAndOptionsAsync(handler, 1, 1)` (IThreadPoolStatics **slot 8**, helper 0x114078) -> `[this+0x6C]`. Body: `ISwapChainPanel::CreateCoreIndependentInputSource(7)` (slot 10) -> `[this+0x70]`; `ICorePointerInputSource` add_PointerPressed (slot 20) / add_PointerMoved (18) / add_PointerReleased (22) with **empty** handlers (0xBF60/0x1DF7F0); `ICoreInputSourceBase.get_Dispatcher` (slot 6) `.ProcessEvents(2=ProcessUntilQuit)` (ICoreDispatcher slot 7).
13. `Canvas.SetLeft(EditBox, 3000.0)` and `Canvas.SetLeft(Button, 3000.0)` (ICanvasStatics {40CE5C46} slot 8, helper 0x1DFAB8, double from literal 0x1DD500).
14. **AppMain**: `operator new(0x40)`, zero 0x40, `sub_1D9D14(app, &shared_ptr<DR>)` -> `[this+0x7C]` (replaces/deletes any previous).
15. **DispatcherTimer**: `sub_1DCE94` (IDispatcherTimerFactory {E9961E6E} slot 6 CreateInstance(NULL,NULL,&t)) -> `[this+0x84]`; QI IDispatcherTimer {D160CE46}: **slot 7 put_Interval(TimeSpan 500000 ticks = 50 ms, r2:r3)**, **slot 9 add_Tick** (handler 0x1DF8A4), **slot 11 Start**.

### 1c. What the first SizeChanged does (the event the friend never raised)
`OnSizeChanged 0x1DF808(this, sender, args)`: `lock(app+0x10)`; **`DR::SetSwapChainPanel(dr, [this+0x5C])` 0x1DBD40** -> `[DR+0x30]=panel`; `ActualWidth` (IFrameworkElement slot 13) / `ActualHeight` (slot 14) -> logical size [DR+0x4C/+0x50] (must be finite and >0, else `InvalidArgumentException`); NativeOrientation (IDisplayInformation slot 7)/CurrentOrientation (slot 6)/LogicalDpi (slot 11); `ISwapChainPanel.get_CompositionScaleX/Y` (slots 6/7) -> [DR+0x60/+0x64]; then **`CreateWindowSizeDependentResources 0x1DB158`** (returns at 0x1DB17C when [DR+0x30]==NULL): `IDXGIFactory2::CreateSwapChainForComposition` (slot 24), `IDependencyObject::get_Dispatcher` (slot 11) + `ICoreDispatcher::RunAsync(1, DispatchedHandler)` (slot 8) whose handler QIs the panel for `ISwapChainPanelNative {F92F19D2-3ADE-45A6-A20C-F6F1EA90554B}` and calls **`SetSwapChain` (slot 3)**; `IDXGIDevice1::SetMaximumFrameLatency(1)`; RTV/DSV/viewport; `IDXGISwapChain2::SetMatrixTransform` (slot 34, friend patched this at 0x1DB56C). Then `args->get_NewSize` (ISizeChangedEventArgs **slot 7**, called directly without QI) -> `DR::SetLogicalSize(dr, s0=w, s1=h)` 0x1DBC80 -> `AppMain::OnWindowSizeChanged(app, sender, args)` 0x1DA32C -> `client->vtbl[20](client, (int)((w+0.5)*platform[+0x3C]), (int)((h+0.5)*platform[+0x40]), s0)` (MinecraftClient::setSize 0x59738) and `platform+0x30/+0x34 = width/height`.

### 1d. What the first successful Tick does (`AppMain::AttachToPanel` 0x1DA634, once)
`lock`; `platform = [[app+0x38]]`; `Window.Current.CoreWindow`; **`AppPlatform_Winrt::initialize` 0x4B55C(platform, coreWindow, panel, EditBox, Button)** = `HID::Initialize` 0x111C1C (CoreWindow slots 34 KeyDown, 36 KeyUp, 44 PointerMoved, 46 PointerPressed, 48 PointerReleased; ITextBox slot 30 add_TextChanged; ITextBox2 {F7168C00} slot 15 put_PreventKeyboardDisplayOnProgrammaticFocus; ThreadPool slot 8) and DPI scale math (`IDisplayInformation` slots 12/13 RawDpiX/Y, 11 LogicalDpi; `DX::DeviceResources::get()` composition scale +0x60/+0x64) -> `platform+0x38/+0x3C/+0x40`; then `QI(panel,IUIElement).get_RenderSize` (slot 23) -> `client->setSize(...)` (same formula as above) and `platform+0x30/+0x34`.

## 2. XAML / WinRT surface the host must provide (complete activation inventory: 38 call sites of 0x2001E4)

Activation = `sub_2001E4(L"class", GUID*, void** out)` -> vccorlib `GetActivationFactoryByPCWSTR` (IAT **0x20B5FC**; only caller of that import). Hook once and route by class name.

| class (L"...") | IID requested | helper RVA | phase |
|---|---|---|---|
| Windows.UI.Xaml.Application | IApplicationStatics {06499997} (Start, LoadComponent) | 0x1EECBC, 0x1EE424 | startup |
| Windows.UI.Xaml.Application | IApplicationFactory {93BBE361} | 0x1E2CF0 | startup |
| Windows.UI.Xaml.Controls.Page | IPageFactory {DFA149AC} | 0x1DD874 | startup |
| Windows.UI.Xaml.Window | IWindowStatics {93328409} | 0x1DAC80 | startup |
| Windows.UI.Xaml.DispatcherTimer | IDispatcherTimerFactory {E9961E6E} | 0x1DCE94 | startup |
| Windows.UI.Xaml.Controls.Canvas | ICanvasStatics {40CE5C46} | 0x1DFAB8 | startup |
| Windows.UI.ViewManagement.StatusBar | IStatusBarStatics {8B463FDF} | 0x1E3394 | startup |
| Windows.UI.ViewManagement.InputPane | IInputPaneStatics {95F4AF3A} | 0x1DE9CC | startup |
| Windows.Phone.UI.Input.HardwareButtons | {594B8780-DA66-4FD8-A776-7506BD0CBFA7} (IHardwareButtonsStatics, not in SDK idl; slot 6 = add_BackPressed) | 0x1E1798 | startup |
| Windows.Graphics.Display.DisplayInformation | IDisplayInformationStatics {C6A02A6C} | 0x4AF58, 0x1E183C | startup |
| Windows.System.Threading.ThreadPool | IThreadPoolStatics {B6BF67DD} | 0x1DA4A4 (RunAsync), 0x114078 (RunWithPriorityAndOptions) | startup |
| Windows.Storage.ApplicationData | IApplicationDataStatics {5612147B} | 0x1DAB28 | startup |
| Windows.ApplicationModel.Package | IPackageStatics {4E534BDF} | 0x4E840 | startup (AppPlatform ctor) |
| Windows.Foundation.Uri | IUriRuntimeClassFactory {44A9796F} | 0x1088A0 | startup (InitializeComponent) |
| Windows.UI.Core.CoreCursor / CoreWindow | ICoreCursorFactory {F6359621} / ICoreWindowStatic {4D239005} | 0x1102E8 / 0x111420 | lazy (cursor changes) |
| Windows.Phone.Devices.Notification.VibrationDevice | {332FD2F1-1C69-4C91-949E-4BB67A85BDC7} | 0x4AEB8 | lazy (vibrate) |
| Windows.Networking.Connectivity.NetworkInformation | INetworkInformationStatics {5074F851} | 0x4B108 | lazy |
| Windows.System.MemoryManager | IMemoryManagerStatics {5C6C279C} | 0x4E69C, 0x4E740 | lazy |
| Windows.ApplicationModel.Store.CurrentAppSimulator | {4E51F0AB..}, {F17F9DB1..} | 0x4B44C, 0x4E93C | lazy (store) |
| Windows.Foundation.Diagnostics.AsyncCausalityTracer | IAsyncCausalityTracerStatics {50850B26} (5 sites in PPL tasks) | 0x4B708,0x4B7CC,0x4B898,0x4B95C,0x4BA04 | lazy; a NO-OP success is enough |
| Windows.Web.Http.{HttpRequestMessage,HttpStringContent,HttpMethod} | IActivationFactory / IHttpStringContentFactory / IHttpMethodStatics | 0x108698.. 0x10C5E8.. | lazy (online features) |

### 2a. Object -> exact vtable slots the game calls (startup + frame orchestration)
(`slot N = offset N*4`; every method returns HRESULT in r0; `-` = unused)

| object / interface (IID) | slots used (name) | called from |
|---|---|---|
| IApplicationStatics | 7 Start(cb); 9 LoadComponentWithResourceLocation(component, uri, 0) | 0x1EECBC; 0x1EE424 |
| IApplicationFactory | 6 CreateInstance(outer, &inner, &instance) | 0x1E3228 (`outer`=App2+8 must be AddRef'd; inner must be an IInspectable that also QIs IApplication) |
| IApplication (inner) | 13 add_Suspending(h, &tok); 15 add_Resuming(h, &tok) | 0x1E5F50, 0x1E5EDC |
| IApplicationOverrides (App2+0x10) | 7 OnLaunched (called BY the host) | - |
| IActivatedEventArgs (launch args) | 7 get_PreviousExecutionState (return 0) | OnLaunched |
| IWindowStatics | 6 get_Current | 0x1DAC80 |
| IWindow {3276167D} | 9 put_Content(IUIElement*), 10 get_CoreWindow, 20 Activate | OnLaunched, ctor, AttachToPanel |
| ICoreWindow {79B9D5F2} | 56 add_VisibilityChanged; (HID) 34 add_KeyDown, 36 add_KeyUp, 44 add_PointerMoved, 46 add_PointerPressed, 48 add_PointerReleased | ctor; HID::Initialize |
| IVisibilityChangedEventArgs (primary iface) | 6 get_Visible(byte*) | 0x1DF9AC |
| IDisplayInformationStatics | 6 GetForCurrentView; 9 add_DisplayContentsInvalidated | 0x4AF58; 0x1E183C |
| IDisplayInformation {BED112AE} | 6 get_CurrentOrientation; 7 get_NativeOrientation; 8 add_OrientationChanged; 11 get_LogicalDpi(float*); 12 get_RawDpiX; 13 get_RawDpiY; 14 add_DpiChanged | ctor, handlers, DR, platform init |
| ISwapChainPanel {C589644F} | 6 get_CompositionScaleX; 7 get_CompositionScaleY; 8 add_CompositionScaleChanged; 10 CreateCoreIndependentInputSource(7, &src) | ctor, DR, 0x1DF510, input item |
| IFrameworkElement {A391D09B} (panel and page) | 13 get_ActualWidth(double*); 14 get_ActualHeight; 47 add_SizeChanged; 51 FindName(HSTRING, IInspectable**) | DR, ctor, InitializeComponent |
| ISizeChangedEventArgs (primary iface) | 7 get_NewSize(Size*) | 0x1DF808, 0x1DA32C |
| IUIElement {676D0BE9} | 22 put_Visibility(1); 23 get_RenderSize(Size*) | Tick, AttachToPanel (QI of Logo/panel/page) |
| IDependencyObject {5C526665} | 11 get_Dispatcher | DR (0x1DC3FC) |
| ICoreDispatcher {60DB2FA8} | 7 ProcessEvents(opt); 8 RunAsync(1, DispatchedHandler*, &op) | input item, DR, HID |
| ISwapChainPanelNative {F92F19D2} | 3 SetSwapChain(IDXGISwapChain*) | 0x1DAF04 |
| ICorePointerInputSource / ICoreInputSourceBase | 20/18/22 add_PointerPressed/Moved/Released; 6 get_Dispatcher | 0x1DE0BC |
| IDispatcherTimerFactory / IDispatcherTimer | 6 CreateInstance; 7 put_Interval; 9 add_Tick; 11 Start; 12 Stop | ctor, Tick |
| IInputPaneStatics / IInputPane | 6 GetForCurrentView; 6 add_Showing; 8 add_Hiding; 10 get_OccludedRect(Rect*) | ctor, 0x1DF714 |
| IHardwareButtonsStatics / BackPressedEventArgs | 6 add_BackPressed; args 7 put_Handled(bool) | 0x1E1798, 0x1DF4E8 |
| ICanvasStatics | 8 SetLeft(IUIElement*, double d0) | 0x1DFAB8 |
| IThreadPoolStatics | 6 RunAsync(h, &op); 8 RunWithPriorityAndOptionsAsync(h, prio, opts, &op) | 0x1DA4A4; 0x114078 |
| WorkItemHandler / IAsyncAction / IAsyncInfo {00000036} | handler Invoke = slot 3 (arg = the IAsyncAction); IAsyncInfo 7 get_Status (0=Started), 9 Cancel | render loop 0x1DA160, StartRenderLoop, StopRenderLoop |
| IApplicationDataStatics / IApplicationData / IApplicationDataContainer / IStorageItem {4207A996} | 6 get_Current; 10 get_LocalSettings; 12 get_LocalFolder; 8 get_Values; 12 get_Path(HSTRING*) | 0x1DAB28.., AppMain createApp |
| IPackageStatics / IPackage | 6 get_Current; 7 get_InstalledLocation | AppPlatform_Winrt ctor |
| IStatusBarStatics / IStatusBar | 6 GetForCurrentView; 7 HideAsync | OnLaunched |
| IPageFactory | 6 CreateInstance(outer, &inner, &instance) | 0x1DD874 |
| IUriRuntimeClassFactory | 6 CreateUri(HSTRING) | 0x1088A0 |
| IImage / ITextBox / ITextBox2 / IButton (fake elements) | QI targets for FindName results; ITextBox 30 add_TextChanged; ITextBox2 15 put_PreventKeyboardDisplayOnProgrammaticFocus; focus helper 0x11127C (IControl {A8912263} slot 46 = Focus(FocusState)) | InitializeComponent, HID |

Event delegate protocol: the pointer passed to `add_X` is the delegate object base; invoke `h->vtbl[3](h, sender, args)` (r0,r1,r2). Tokens are 64-bit written through the out pointer. All MainPage handlers take `AppMain`'s critical section, so they may be called from any thread but never re-entrantly with the render thread holding it for long.

### 2b. Things that are NOT used (so the host need not fake them)
CompositionTarget (no GUID 2B1AF03D, no string), Frame/navigation, SplashScreen, `Application.Current`, `Application.Resources`, `IComponentConnector.Connect` (names are resolved by FindName), `IXamlMetadataProvider` (only the real XAML parser queries it), `Window.Activated/SizeChanged/Closed`, `CoreWindow.Dispatcher` outside HID, `SwapChainPanel.CompositionScaleChanged` is registered but optional.

## 3. Where DeviceResources, AppMain and MinecraftClient are created, in order
1. **DX::DeviceResources** (friend: "DisplayResources"): MainPage ctor step 11, `make_shared` 0x1DCE2C -> ctor 0x1DAE14 (stores `this` in global **0xE2AFC4**, read by `DX::DeviceResources::get()` 0x4B054 at 20 sites) -> `CreateDeviceResources` 0x1DAFC0: `D3D11CreateDevice(NULL, HARDWARE, NULL, Flags=0, {0xB100,0xB000,0xA100,0xA000,0x9300}, 5, 7, &dev, &FL@DR+0x38, &ctx)` through IAT **0x20B1A4**. **Surface RT needs the FL clamp to 9_1 and BGRA flag here.** No swap chain yet.
2. **AppMain** (0x40 B): MainPage ctor step 14. Ctor 0x1D9D14: `[+0]` vtable 0xDC8830 (2 empty slots = IDeviceNotify), `[+4]`/`[+8]` copy of shared_ptr<DR>, `[+0xC]=0`, `critical_section` at +0x10, `createApp` 0x1D9DD4 on `+0x34`, `DR::setOwner` 0x1DBBC0 (`[DR+0x74]=app`).
   - `createApp` order: `holder = new(8)` -> `platform = new(0x88)+memset+AppPlatform_Winrt::ctor 0x494C0` (HIDController at +0x2C, default size 0x780x0x438 at +0x30/+0x34, Package.InstalledLocation.Path -> narrow string at +0x14) -> `holder[0]=platform` -> **`client = newMinecraftClient 0x1D9CC0` (new 0x168, memset, ctor 0x54E90; client+0x118/+0x11C zeroed by the ctor)** -> `platform[+0x10]=client` -> `ApplicationData.LocalFolder.Path` -> platform +0x78 and +0x90 -> `ThreadPool.RunAsync(init job)`.
3. **MinecraftClient::init** (vf7, 0x57B84): runs later on a pool thread via 0x471E8 (client+0xC=holder required). It creates the TextureManager (`new 0xB8` -> 0xDBF98 -> stored client+0x118) and the Font (`new 0x890`, `"font/default8.png"`, 0x548A0 -> client+0x11C) plus shaders (D3DCompile via IAT 0x20B000) and the first screen.
4. **AppMain::isReady = byte [AppMain+0x3C]** is the third member of the 3-word struct at AppMain+0x34 `{MinecraftClient*, holder*, byte ready}`; set to 1 **only** by the init-job lambda 0x1DA2D8 after `sub_471E8` returns (`strb [s+8]` with DMBs). Read by `IsAppInitialized` 0x1DA310.

Field maps (verified offsets):
- **App2** (0x40): +0x14 inner (Application base), +0x30 MainPage^, +0x38 int(-1), +0x3C disposed byte; QI: Overrides->+0x10, IXamlMetadataProvider->+0x18, IComponentConnector->+0x1C.
- **MainPage** (0x94): +0x08 Page outer, +0x24 Page inner, +0x58 contentLoaded, +0x5C panel, +0x60 Logo, +0x64 EditBox, +0x68 Button, +0x6C input IAsyncAction, +0x70 CoreIndependentInputSource, +0x74/+0x78 shared_ptr<DR>, +0x7C AppMain*, +0x80 visible byte (init 1), +0x84 DispatcherTimer, +0x8C int.
- **AppMain** (0x40): +4 DR*, +8 ctrl, +0xC render IAsyncAction, +0x10..0x33 critical_section, +0x34 client, +0x38 holder, +0x3C isReady.
- **DeviceResources**: +4 device, +8 context, +0xC swap chain, +0x30 panel, +0x38 FL, +0x4C/+0x50 logical size, +0x54/+0x58 native/current orientation, +0x5C dpi, +0x60/+0x64 composition scale, +0x74 listener.
- **AppPlatform_Winrt** (0x88): +0x10 client, +0x14 install path (std::string), +0x2C HID, +0x30/+0x34 width/height, +0x38/+0x3C/+0x40 scale floats, +0x50 critical_section, +0x70 event deque, +0x78/+0x90 local-folder path strings.

## 4. Pure game logic vs XAML-runtime dependent

| step | classification |
|---|---|
| CRT, main wrapper, ApplicationInitializationCallback, App2 ctor body | C++/CX plumbing, runs unmodified; needs activation fakes only |
| `Application::Start` blocking loop, IApplicationFactory CreateInstance, OnLaunched delivery | **XAML runtime** (host re-implements) |
| StatusBar hide, Window.Content/Activate, Uri/LoadComponent/FindName, Page base | **XAML runtime** (fakes) |
| MainPage ctor event registrations, handlers | thin glue, unmodified; fakes store/fire delegates |
| `DX::DeviceResources` ctor/CreateDeviceResources | game code + D3D11 (hook IAT 0x20B1A4) |
| DR::SetSwapChainPanel / CreateWindowSizeDependentResources | game code but needs panel getters, dispatcher RunAsync, `ISwapChainPanelNative::SetSwapChain` and a composition-capable swap chain (**XAML/DComp dependent**) |
| `AppMain` ctor, `createApp`, `AppPlatform_Winrt::ctor`, `newMinecraftClient` | game logic; WinRT only for Package/ApplicationData paths and ThreadPool |
| init job, `MinecraftClient::init` | pure game logic + D3D (thread-pool thread) |
| Tick gate, `AttachToPanel`, `HID::Initialize` | game logic that registers CoreWindow/TextBox events (**fake CoreWindow/TextBox**) |
| render loop `0x1DA160`, `AppPlatform_Winrt::update`, `Minecraft::update`, `DR::present` | pure game logic + D3D; only needs ThreadPool/IAsyncInfo semantics |
| InputPane Showing/Hiding -> `AppMain::onKeyboardHeightChanged` | game logic fed by a XAML event |

## 5. Replay sequence

### 5a. Faithful option (b) - the host only does this (ordered)
1. Before the game runs: DLL loaded by the patched exe (as for 1.2.8); install IAT hooks: **0x20B5FC** (activation router), **0x20B1A4** (D3D11CreateDevice: Flags|=BGRA(0x20), levels={0x9100}), D3DCompile 0x20B000 if profiles need forcing. `vccorlib120_app.dll`, `msvcr120_app.dll`, `msvcp120_app.dll` must be loadable.
2. Let the CRT run to `Application::Start` (route "Windows.UI.Xaml.Application" to the fake; `statics->Start` = slot 7).
3. In fake Start (UI thread): create the HWND; `cb->vtbl[3](cb,NULL)`; verify `CreateInstance` was called and AddRef'd `outer`; `QI(outer, IApplicationOverrides)`; call `p->vtbl[7](p, launchArgs)` with `PreviousExecutionState=0`.
4. During OnLaunched the game runs: fakes for StatusBar, Page, Window, CoreWindow, DisplayInformation, InputPane, HardwareButtons, DispatcherTimer, Canvas, ThreadPool, SwapChainPanel/Image/TextBox/Button as in 2a.
5. After `Window.Content` is set raise `SizeChanged(NewSize=client DIPs)` on the panel (make `ActualWidth/Height` and `CompositionScaleX/Y` valid first). `DR` then creates the swap chain and calls `SetSwapChain` through your dispatcher.
6. Run a message loop in Start: DispatcherTimer 50 ms Tick (until Stop), `CoreDispatcher.RunAsync` queue (DispatchedHandler.Invoke = slot 3), cursor/keyboard events -> CoreWindow delegates.
7. Everything else (init job, ready flag, attach, render loop) runs by itself on thread-pool threads created by your ThreadPool fake. Present the composition swap chain on the HWND.
8. On WM_CLOSE: return S_OK from Start (optionally fire Suspending first: handler 0x1E3DA0).

### 5b. Hybrid "skip the XAML loader only" (what the friend should have replayed)
| # | game function (RVA) | ABI / args |
|---|---|---|
| 1 | `make_shared<DR>` 0x1DCE2C | `r0 = out shared_ptr*` -> r0 (creates D3D device via hooked IAT) |
| 2 | `AppMain::AppMain` 0x1D9D14 | `r0 = operator new(0x40) zeroed`, `r1 = &shared_ptr<DR>` (internally: platform, client ctor, LocalFolder/Package paths [needs ApplicationData/Package fakes], init job on ThreadPool fake) |
| 3 | poll `AppMain::IsAppInitialized` 0x1DA310 | `r0=app` -> bool (byte +0x3C) |
| 4 | under `lock(app+0x10)`: `DR::SetSwapChainPanel` 0x1DBD40 | `r0=dr, r1=ISwapChainPanel*` (needs panel fakes + DisplayInformation fake) |
| 5 | `DR::SetLogicalSize` 0x1DBC80 | `r0=dr, s0=w, s1=h` (float DIPs) |
| 6 | `AppMain::OnWindowSizeChanged` 0x1DA32C | `r0=app, r1=sender, r2=ISizeChangedEventArgs*` |
| 7 | `AppMain::AttachToPanel` 0x1DA634 | `r0=app, r1=panel, r2=EditBox(ITextBox*), r3=Button(IButton*)` (needs Window.Current + CoreWindow fake) |
| 8 | `AppMain::StartRenderLoop` 0x1DA8A8 | `r0=app` (needs ThreadPool fake) |
| later | keyboard `0x1DA5B4(app, px)`; back `0x1DA248(app)` -> bool; suspend `0x1DC054(dr)`, `0x1DA9D4(app)`, `0x1DAA3C(app)`; resume `0x1DA438(app)` + `0x1DA8A8(app)`; visibility false -> `0x1DA9D4`, true -> `0x1DA8A8` |
Replay still needs the same Window/CoreWindow/DisplayInformation/panel/ThreadPool/ApplicationData/Package fakes, so it saves only Application/Page/FindName/Timer/InputPane/HardwareButtons/StatusBar/Canvas plumbing and costs exact bookkeeping (lock order, AppMain+0x34 struct, DR listener). Not recommended unless a fake Application is unwanted.

## 6. (e) Can the game's own MainPage code run unmodified?
Yes. After InitializeComponent the MainPage logic is: one 1.9 KB constructor that only registers 9 delegates and instantiates {DeviceResources, input work item, AppMain, DispatcherTimer}; handlers of 18-250 B that forward to 6 AppMain entry points (IsAppInitialized, StartRenderLoop, StopRenderLoop, AttachToPanel, OnWindowSizeChanged, onKeyboardHeightChanged + HandleBackPressed/resume/suspend helpers) and 8 DeviceResources setters. It touches ~30 interfaces/~80 slots (table 2a), all of which are simple property getters, event adders (store delegate, return token), or fire-and-forget calls. InitializeComponent itself needs only a no-op `LoadComponentWithResourceLocation` plus a `FindName` that returns four host objects. **Choose (b), faithful.** Patches to game code are not required (only IAT/activation hooks); what the 1.2.8 CoreWindow design did with a custom main loop is here done by a fake `Application.Start` + thread-pool fake.

## 7. What the friend notes got wrong / missed (startup-related)
- `sub_5DA5B4` "AppMain::tick(ms)" = `AppMain::onKeyboardHeightChanged(int px)` (0x1DA5B4): scales `px*platform[+0x40]` and calls `MinecraftClient::setKeyboardHeight` (0x59578: `client+0x164=...`, `screen->vf30`). Calling it with 16 each frame never renders anything (explains `Draw=0`).
- `sub_5DA634` "runFrame" = `AppMain::AttachToPanel` (once, from the Tick gate) - not a frame function. The real frame function is the ThreadPool body 0x1DA160 -> `client->vtbl[19]` (0x1D8A54) + `DR::present`.
- `sub_5DB158` "windowSizeChanged" = `DX::DeviceResources::CreateWindowSizeDependentResources`; it begins with `if ([DR+0x30]==NULL) return` (`cmp r3,#0 ; beq.w 0x1DB7AE` at 0x1DB17A/0x1DB17C). Injecting a swap chain at DR+0xC without the panel made it a **no-op**, so no RTV/DSV/viewport existed.
- "No setter for AppMain+0x3C": it is written by the init-job lambda 0x1DA2D8 (`strb [s+8]`), not by any AppMain/MainPage function body.
- `client+0x118/+0x11C == NULL` and `client+0xC == NULL`: all produced by `MinecraftClient::init` (vf7 0x57B84) / `sub_471E8` which the friend never called. Hand-writing 1280x720 into client fields and calling `vf20 setSize` before that crashes exactly as they saw.
- Their tick "60 fps host loop calling AppMain + Present(chain,1,0)" skips: `platform->update` (input), `Minecraft::update`, the DR swap-chain `SetSwapChain` on the panel, and `AttachToPanel`/HID registration.
- Verified true: patch site 0x200A72, IAT cells 0x20B000/0x20B1A4/0x20B5FC/0x20B1AC, `AppMain` 0x40 bytes, ctor 0x1D9D14, DR make_shared 0x1DCE2C, matrix-transform patch bytes at 0x1DB56C, vtable of MinecraftClient 0x33FFCC, `StartMenuScreen` crash chain via client+0x118.

## 8b. Keyboard / TabTip contract of this exe (relevant to the TabTip work)
The engine never talks to a Win32 keyboard API. It (1) focuses the hidden XAML TextBox `EditBox` (HID lambdas 0x110BE8/0x11079C, ITextBox/ITextBox2/IControl.Focus; `PreventKeyboardDisplayOnProgrammaticFocus` is set via ITextBox2 slot 15) to open the keyboard and focuses the `Button` (lambda 0x1108D4) to close it; (2) receives text through `ITextBox.TextChanged` (HID onTextChanged 0x113D8C) and CoreWindow KeyDown/KeyUp; (3) learns the keyboard height only from `InputPane.Showing/Hiding` (handlers 0x1DF714 / 0x1DF700 -> `AppMain::onKeyboardHeightChanged(px)` 0x1DA5B4 -> `client+0x164`, `screen->vf30`). A fake TextBox can therefore drive TabTip (show on focus, hide on unfocus) while the host raises InputPane Showing(OccludedRect.Height)/Hiding(0) from its own state machine, instead of depending on TabTip window notifications.

## 8. Not found / uncertain
- Float constants in `setSize` call and DPI divisor (pool words at 0x1DA74C, 0x1DA3A8, 0x4B5DA/0x4B670) not decoded.
- `HID::Initialize` internals (only slot inventory above); helpers 0x113EC8/0x114194/0x114380 unidentified.
- Apartment type of the main/UI thread under the real runtime (CRT RoInitialize is conditional and not taken from the file); fakes should be agile.
- No explicit `Application.Exit()` or window-closed handler exists; app end = process end after `Start` returns.
- Resume/suspend client helpers 0x4743C/0x4736C/0x473D4/0x474A4 only located.
- Confidence: 86 facts high, 4 medium (DR field layout, platform scale formula, ITextBox2 usage, IHardwareButtons slot).
