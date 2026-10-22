# Minecraft PE 0.10.4.40 (WP8.1 XAML) -> Win32Craft ARM32 host: design (single source of truth)

Status (2026-10-18): foundation + all five units IMPLEMENTED, reviewed unit by unit and INTEGRATED (section 14b; `INTEGRATION_NOTES.md` = what is implemented, what only a device can prove, the log lines per milestone, the test plan). The independent review of the first foundation found 12 defects (1 blocker, 2 high, 9 medium) and 11 coverage
gaps; all are fixed in place and listed in section 14 ("Decisions after review"). NOT yet run on a device.
Read this document completely, then your brief `arm32/v0_10_4/units/<unit>.md`, then `units/SLOTS.md` (generated slot/ABI tables + typedef names) and the area notes it cites
(`facts/*.md`, `RECON_SUMMARY.md`; fact ids like `startup_flow.dr_set_panel` are keys in `facts/<area>.json` / `.verdicts.json`).
Never trust `D:\repo\win32craft\0.10.4.40` (friend notes). Rules of the port: `arm32/PORT_RULES.md` (Thumb-2 hard-float AAPCS, plain C function pointers for game calls,
patch DATA cells with PAGE_READWRITE, log generously). x86 output of win32_host.c must stay byte-identical.

## 1. Decided architecture (do not re-litigate)
FAITHFUL: the game's own code runs UNMODIFIED against host fakes of ~30 XAML/WinRT objects (~80 vtable slots, `startup_flow.md` table 2a):
CRT -> user main 0x1F42A4 -> `Application::Start` 0x1EECBC (IApplicationStatics slot 7 = a HOST fake that runs the UI loop) -> `cb->Invoke` -> `App2` (aggregates
Application through IApplicationFactory) -> host calls `App2::OnLaunched` through IApplicationOverrides slot 7 -> `new MainPage` (aggregates Page) -> `Window.Content = page`
-> `Window.Activate` -> UI loop. MainPage ctor creates `DX::DeviceResources` (D3D11CreateDevice), the input work item, `AppMain` (platform + client + init job on the thread
pool) and a 50 ms `DispatcherTimer`. The first `SizeChanged` creates the swap chain; the first ready `Tick` runs `AttachToPanel` (HID::Initialize, sizes) and
`StartRenderLoop` (a High-priority thread-pool item that spins `lock; pumpInput; Minecraft::update; Present(0,0)` while `IAsyncInfo.Status==Started`).

Game code is patched only by: the 1-byte shader profile patch (rva 0x2351F9 `0x33 -> 0x31`; done by the exe patcher by default AND verified/idempotently applied by the panel unit), the D3D11CreateDevice hook (IAT 0x20B1A4: levels {0x9100}, report 0x9300),
IAT hooks, data-cell patches (platform, optional dxgi cells of another module), and the entry redirect made by the exe patcher (call site 0x200A72 -> trampoline 0x20A364 ->
`win32craft.dll!Win32Bootstrap`). ONE `win32craft.dll` serves 1.2.8 and 0.10.4: `Win32Bootstrap` selects 0.10.4 when PE machine == 0x01C4 and SizeOfImage == 0x00E6F000
(stock 0x00E6E000 is rejected; 1.2.8 patched is 0x01731000). The host then calls the game's WinMain wrapper (rva 0x203EFC) itself on the Win32Bootstrap thread.

Presentation: path B is the default: the game keeps calling `IDXGIFactory2::CreateSwapChainForComposition` (vtable slot 24) and `IDXGISwapChain2::SetMatrixTransform`
(slot 34); the panel unit patches the dxgi vtable DATA cells so the former creates an HWND swap chain on the game window, the latter returns S_OK, and `IDXGISwapChain::Present` (slot 8) is bracketed
by the frame accounting/pacing helpers of the core. Path B is **self-checking**: `SetSwapChain` compares the game's chain with the one the stub recorded and falls back to path A (DirectComposition) with an ERROR line
on a mismatch; a 3 s rule logs a black window. Path A (fake `SetSwapChain` = visual SetContent) is also selectable (`--present=dcomp`).

## 2. File layout, ownership, naming, tooling
| file | owner | content |
|---|---|---|
| `win32_host_arm32_0104.inl` | designer | umbrella, included from win32_host.c in front of `Win32Bootstrap` |
| `win32_host_arm32_0104.h` | generated (`tools/arm32/gen_constants.py`) | verified RVAs `ARM0104_*` (do not edit) |
| `win32_host_arm32_0104_iids.inl` + `units/SLOTS.md` | generated (`tools/arm32/gen_0104_iids.py` from the Windows SDK WinRT headers) | `X104_IID_<I>`, `X104_SLOT_<I>_<Method>`, `X104_NSLOTS_<I>`, **`X104_FN_<I>_<Method>` (the exact C type of every slot thunk)**, `X104Size/Point/Rect`; the 93 slot facts of the exe were cross-checked: 0 mismatches; all 16 IIDs readable in the exe equal the SDK IIDs |
| `win32_host_arm32_0104_core.inl` | designer | toolkit (section 5), global state `x104`, cross-unit prototypes, shared constants/helpers (`X104_TIMER_PUMP`, `X104_WM_*`, `x104_ui_pass`, `x104_present_*`, `x104_set_text_input_active`, `x104_aggregate_create`, `X104_SLOT`) |
| `win32_host_arm32_0104_async.inl` | async implementer | ThreadPool, IAsyncAction/IAsyncInfo, CoreDispatcher, DispatcherTimer, UI loop |
| `win32_host_arm32_0104_app.inl` | app implementer | Application, Page, Window, Uri, StatusBar, Canvas, HardwareButtons, DisplayInformation, lifecycle, exit |
| `win32_host_arm32_0104_panel.inl` | panel implementer | SwapChainPanel, D3D11CreateDevice hook, profile patch, presentation (path B/A), Present stub, device diagnostics, WM_SIZE, device loss |
| `win32_host_arm32_0104_platform.inl` | platform implementer | Package, ApplicationData, MemoryManager, Vibration, Network, Store, tracer, Http stubs, paths, data cells |
| `win32_host_arm32_0104_input.inl` | input implementer | CoreWindow, pointer/key/text, XAML elements, InputPane, TabTip, `text_input_active`, window message translation |
| `win32_host_arm32_0104_selftest.inl` | designer | `--selftest-fakes`: on-device ABI/contract test of every implemented fake through the generated typedefs (section 5.1) |
| `win32_host_arm32_0104_boot.inl` | designer | identification, options, IAT hooks + diagnostics, VC120 `RoGetActivationFactory` forwarders + module list, window (hidden, clamped) + window proc, `x104_bootstrap` |
| `tools/make_0104_package.py`, `tools/loadcheck/` | designer | the package (section 13) |

**Rules for implementers**
1. Edit ONLY your unit file. Never edit another unit, the core, the boot, the selftest, win32_host.c or the generated files. A missing toolkit feature or a contract change goes in your
   report to the designer (meanwhile write a unit-private static helper). Keep the public function names and signatures of your unit EXACTLY (they are the contract, section 8).
2. **Naming**: everything file-scope that is not part of the contract starts with `x104_<unit>_` (statics, types, vtable arrays, class descriptors). The whole host is ONE
   translation unit; a clash breaks the other units' compile.
3. **A unit may use only symbols of the core, of the generated iids/header files and of win32_host.c** - never anything defined in `boot.inl` (it is included LAST: the first foundation told the async unit to use `X104_TIMER_PUMP`, which was boot-only and could not compile) or in the selftest, and no private `x104_<other unit>_` symbol (only the cross-unit functions of section 8). `try_0104.py` lints this.
4. **Every vtable slot is registered with `X104_SLOT(Interface, Method, thunk)`** (or `X104_SLOT_NOARGS` for a thunk that ignores all arguments, e.g. `x104_nop`). The macro refuses at compile time a thunk whose C signature differs from the generated typedef `X104_FN_<Interface>_<Method>` (units/SLOTS.md): the vtable arrays hold untyped pointers, so a missing `LONGLONG`, a `float` where a `double` is expected or an `int` where a `BYTE *` is expected would otherwise only show up as a crash on the device. A raw `[X104_SLOT_x_y] = fn` is refused by the lint. A slot without a typedef (a struct by value other than Size/Point/Rect) needs a generator extension.
5. Compile check (seconds): `python tools/arm32/try_0104.py --unit <async|app|panel|platform|input>` builds a temp copy where only your file comes from the repo and the other four
   come from the pristine stubs in `arm32/v0_10_4/stubs/`; it lints your file, compiles, links and runs the x86 identity check. `python tools/arm32/try_0104.py` checks the real tree;
   `--selftest` also runs the x86 host-side test of the toolkit (`tools/arm32/test_0104_core.py`, 130 checks + the X104_SLOT negative-compile tests). Zero ARM errors and zero warnings in 0104 files are required.
   Optional PC-side unit test for pure logic (text diff, queues, ordering): copy `tools/arm32/test_0104_core.c` to `tools/arm32/test_0104_<unit>.c`, include your real unit file instead of its stub
   and run `python tools/arm32/test_0104_core.py --src tools/arm32/test_0104_<unit>.c` (x86 build of the same sources; COM thunks are WINAPI so the vtable calls work on both ABIs). New files named after your unit are allowed.
6. Everything you add must be ARM-only (the .inl is only included under `WIN32CRAFT_ARM32`) and must not change any x86 code.
7. A device run is the only real test: every failure path logs what failed, the HRESULT and the actual bytes/values (`x104_logf`), no silent returns. `--selftest-fakes` (section 5.1) is the first device step: run it before the game.

## 3. Threading model
| thread | what it is | runs |
|---|---|---|
| **UI thread** = host main thread | the CRT main thread: MTA (vccorlib `InitializeData(2)` = RoInitialize(MTA); the host's own RoInitialize returns S_FALSE = fine). It creates the HWND, calls the WinMain wrapper, and inside it the fake `Application::Start` runs `x104_ui_run_loop` | window procedure, all game delegates (SizeChanged, Tick, Pointer*, Key*, TextChanged, BackPressed, InputPane, Visibility, Dpi...), `CoreDispatcher.RunAsync` items, DispatcherTimer ticks, OnLaunched, the MainPage constructor and everything it calls |
| **pool threads** | created by the async unit, one real thread per work item (never reused), MTA (`RoInitialize(MTA)` ignoring S_FALSE/RPC_E_CHANGED_MODE), 2 MiB stack | the init job (`ThreadPool.RunAsync`: `MinecraftClient::init`, seconds of D3DCompile), the render loop (High + TimeSliced; busy loop; calls the Present stub), two input work items that block in `ProcessEvents(ProcessUntilQuit)` (MainPage ctor 0x1DE0BC and HID::Initialize 0x110940) |
| watchdog / exit guard | existing `arm32_boot_*` threads of win32_host.c | stall reports, the 30 s heartbeat (frames counted by the Present stub), 30 s exit guard |

* There is NO real XAML STA. All host objects are agile (class flag `X104_CLASS_AGILE`, `QI(IAgileObject)` = S_OK - mandatory for CoreWindow, `HID` keeps it in `Platform::Agile`).
  Any fake method may be called from ANY thread unless its brief says otherwise; the fakes must be thread-safe with leaf locks (`X104Lock` spin lock, never held across a call
  into the game or into another unit).
* **The game's AppMain critical section** (`AppMain+0x10`, a non-recursive `Concurrency::critical_section`, taken by the MainPage handlers on the UI thread AND by the render loop on its
  pool thread every iteration) is the central hazard. While the UI thread is inside a game handler it may hold it, and the render thread blocks in it. Therefore:
  a fake called from inside a game handler must NEVER (a) invoke a game delegate/callback synchronously, (b) wait for the render thread, (c) wait for the UI loop, (d) take a host lock that
  another thread holds across a game call. `CoreDispatcher.RunAsync` ALWAYS queues (even from the UI thread), `DispatcherTimer.Stop` only flips state, `add_X` only stores, a late `IAsyncAction.put_Completed` queues its handler.
* Message pumping happens in `x104_ui_run_loop` and, **inside the nested modal loops Windows runs for window move/size and menus, through the 10 ms `WM_TIMER` (`X104_TIMER_PUMP`) whose handler is the core's `x104_ui_pass()`** (queue + timers + the three unit ticks): live resize, the TabTip poll and the queue therefore keep running during a drag. A quit requested during a modal loop is handled by the window proc (`X104_WM_QUIT` -> `f_loop_exit` + `PostQuitMessage`).
* `Sleep`-free handshakes: when a unit needs to wait for another thread it uses events with timeouts and logs the timeout.

## 4. Ordering and protocol rules (R1 ... R17)
**R1 No synchronous delegate invocation from inside a fake.** Delegates are invoked only (a) by `x104_ui_run_loop` and the `x104_<unit>_ui_tick` hooks (through `x104_ui_pass()`),
(b) by items posted with `x104_ui_post`, (c) by the fake `Application::Start` itself for `cb->Invoke` and `OnLaunched`. Reason: the non-recursive AppMain lock and the game's
own re-entrancy assumptions (`facts/RECON` risk 3). Window messages that Windows can SEND synchronously while the host calls `ShowWindow/SetFocus/SetWindowPos/DestroyWindow`
(WM_SIZE, WM_MOVE, WM_ACTIVATE*, WM_SETFOCUS, WM_KILLFOCUS, WM_SHOWWINDOW, WM_WINDOWPOSCHANGED, WM_CAPTURECHANGED, WM_GETMINMAXINFO) may arrive nested inside a game call:
their handlers only update host state / set a pending flag; the game-visible effect is delivered from the unit's `x104_<unit>_ui_tick`. Pointer, key and char messages come from the
input queue (never synchronously nested) and may call the delegates directly.
**R2 First SizeChanged** (panel unit, from `x104_panel_ui_tick`) only when ALL hold: `x104.f_content_set` (the MainPage constructor returned: AppMain, the timer and every handler
exist), `x104_game_appmain() != NULL`, `x104_game_client_has_holder()` (`client+0xC != NULL`, written at the START of the init job; `SetSize` before it crashes at rva 0x58718),
client size > 0. It must be delivered BEFORE the first ready Tick (R3) and exactly once as "first". Actual size/scale values: `ActualWidth/Height >= 0` (the game's checker
`0x1DAED0` only rejects NEGATIVE values, `startup_flow.dr_set_panel` verdict), composition scale non-zero. `args->get_NewSize` is called straight on the args pointer (no QI):
the pointer passed as `args` MUST be the `ISizeChangedEventArgs` view (R10). The handler reads NewSize twice.
**R3 Timer gating** (async unit): the DispatcherTimer may be created/configured/started at any time, but its Tick is not delivered before `x104.f_size_changed_done`; the first Tick
is due one interval after that. The game's Tick (rva 0x1DF8A4) does nothing until `AppMain+0x3C == 1`; when ready it calls `IDispatcherTimer::Stop` (slot 12) (set `x104.f_ready_tick`,
log "READY tick"), collapses Logo, runs `AttachToPanel` and starts the render loop. `AttachToPanel` reads DR composition scale (+0x60/+0x64): SizeChanged
must have run before (m3 in `startup_flow.verdicts`).
**R4 Init job / ready flag**: nothing of the host sets `AppMain+0x3C`; the init job lambda (0x1DA2D8) does, after `MinecraftClient::init`. The host never calls `Minecraft::update`, never
calls `Present` itself, never touches `client+0x20/0x24` (friend failure modes). (The Present STUB wraps the game's own call; it does not make one.)
**R5 Exceptions in work items**: a pool worker guards every `Invoke` with `__try/__except` (`x104_call_guarded3`), logs code / rva / info (a C++/CX exception is SEH `0xE06D7363`; the
`_CxxThrowException` hook has already logged the thrown type) and completes the action as Error instead of killing the process silently.
**R6 Failure HRESULTs are fatal for the game**: the game's C++/CX wrappers turn any HRESULT < 0 into `Platform::COMException`. A fake returns S_OK for every call listed in the briefs.
The unknown-slot stubs deliberately return E_NOTIMPL: that is the "fail loudly" policy, and the boot's IAT diagnostics (`X104 *** game raises Platform::COMException(0x...) from game rva ...`)
point at the culprit.
**R7 Events**: `add_X(handler, EventRegistrationToken*)`: `x104_event_add` (AddRef the delegate, 64-bit token through the out pointer); `remove_X(token)` takes the token BY VALUE (8-byte
aligned, r2:r3); invoke = `handler->vtbl[3](handler, sender, args)`; no QI of the delegate. Handlers are called with the game's critical sections in an unknown state: never
hold a host lock (`x104_event_invoke` already runs outside its lock).
**R8 HSTRING**: real combase HSTRINGs. Arguments received are BORROWED. Returned HSTRINGs (get_Path, get_Text ...) are created with `x104_hstring_create` and owned by the game. Never
use the 1.2.8 fake HSTRING shims.
**R9 QI rules**: IUnknown/IInspectable always return the identity view `views[0]`; each interface returns its own view; `IAgileObject` succeeds (agile classes); unknown IIDs return
E_NOINTERFACE and are LOGGED (`X104 QI MISS`): that line is how missing interfaces are found on the device. Every returned interface is AddRef'd; objects are never freed.
**R10 Event-args and argument pointer rule**: the game calls some args methods directly on the pointer it receives, without QI: `ISizeChangedEventArgs.get_NewSize` (slot 7),
`IVisibilityChangedEventArgs.get_Visible` (slot 6), `IPointerEventArgs.get_CurrentPoint` (slot 6), `IKeyEventArgs.get_VirtualKey/get_KeyStatus` (6/7),
`IBackPressedEventArgs.put_Handled` (slot 7). So `args` MUST be the interface view of exactly that interface (pass `&obj->views[i]` of the matching `X104IfaceDesc`; if it is the primary
interface that is views[0]). The same holds for the arguments of the game's own overrides: `OnLaunched` receives the `ILaunchActivatedEventArgs` view. A factory's out parameters follow the DECLARED type: `IPageFactory::CreateInstance`'s third parameter is an `IPage*` (not views[0]).
`sender` is never used by the game's handlers but should be a valid IInspectable view (or NULL where the real runtime passes NULL: Tick args).
**R11 Dispatcher semantics** (async unit): `RunAsync(priority, DispatchedHandler, &op)`: FIFO (High before Normal/Low), executed on the UI thread inside the loop, handler `Invoke` is vtable
slot 3 with NO arguments; `op` may be returned NULL or a real action (DR tolerates NULL at 0x1DB47C); the host returns a real action that completes after the handler ran.
`HasThreadAccess` = on UI thread. `ProcessEvents(ProcessUntilQuit=2)` on a pool thread BLOCKS until `x104_async_shutdown` or the item's Cancel (the two input work items); it must
not run on the UI thread (there every option is a logged no-op: a fake never runs delegates inside a game call, 14b #6).
**R12 IAsyncInfo**: `get_Status` returns 0 (Started) until the work item returned (1 Completed) or `Cancel` was called (2 Canceled, immediately); after Cancel it stays != 0 for ever
(`StartRenderLoop` re-queries the stored action). The action passed to `Invoke(action)` must be non-NULL (the render loop dereferences it unconditionally) and answer
`QI(IAsyncInfo {00000036})`. **Completed handlers**: attaching `put_Completed` to a finished action invokes the handler (asynchronously via `x104_ui_post`, guarded, status Completed) - both for the async unit's actions and for the core's completed-action singleton (they used to disagree).
**R13 Pointers**: always send `PointerReleased` for every `PointerPressed` (focus loss, minimise, capture lost, device change); positions are client pixels = DIPs (scale 1.0);
the game keeps a `std::map` PointerId -> slot (12 slots max).
**R14 Text**: the TextBox text a game `put_Text` produces triggers EXACTLY ONE `TextChanged`, delivered asynchronously after the setter returned (the game sets `HID[0x58]` before
`put_Text` and only a TextChanged clears it); a key press is delivered as the new text; Enter is the adjacent pair `L"\r\n"` (a lone CR is plain text); show = `Focus(Programmatic=3)` on
the EditBox, hide = `Focus(Programmatic=3)` on the Button; TextChanged is fired from the UI thread only. The input unit keeps `text_input_active` in step with the focused element (R17).
**R15 Safe before init**: `CreateWindowExW` and the first `ShowWindow` SEND messages (WM_NCCREATE, WM_GETMINMAXINFO, WM_SIZE, WM_SHOWWINDOW ...) to `x104_window_proc` before any unit init ran. The boot therefore routes NOTHING to the units until `x104.f_units_ready` (set after the four `x104_<unit>_init` returned) and answers with `DefWindowProc` meanwhile; every unit's `ui_tick`/`window_message` must nevertheless be safe on zero-initialised state (the ticks run via `x104_ui_pass`, a no-op before the flag). The panel reads the real client size in `x104_panel_init`, so no early WM_SIZE is lost.
**R16 Early exit**: before the init job finished (`!x104_game_is_ready()`) the process must still be closable: `Suspending` is raised ONLY when the game is ready (its handler 0x1E3DA0 runs `DR::Trim`, `StopRenderLoop` and the client suspend helpers against `client+0x118`/`+0x11C`, NULL while the shaders compile); otherwise `x104_async_shutdown` + `x104_ui_quit` directly (log `exit before ready: Suspending skipped`). `x104_async_shutdown` waits at most its existing 2 s, it never waits for the init job.
**R17 Flag ownership** (each flag has exactly one owner and one meaning; try_0104.py lints the exit flags): `f_exit_requested` = app only ("an exit was requested", idempotency guard of `x104_app_request_exit`); `f_quit` = async only ("`x104_ui_quit` ran", `x104_ui_post` refuses from then on); `f_loop_exit` = async + the window proc ("the loop must return"); `f_units_ready` = boot; `f_swapchain_created` = panel; the others as in section 6. Shared win32_host.c globals the host owns in 0.10.4: `text_input_active` (input unit, through `x104_set_text_input_active`), `arm32_boot_frames_started/_done` (the Present stub, through `x104_present_begin/_end`).

## 5. Toolkit (win32_host_arm32_0104_core.inl) - what implementers use
* **Object / view / class**: `X104Object` (refs, serial, `views[]`), `X104View {vtbl, obj, desc}` (the pointer the game gets), `X104IfaceDesc` (IID, mutable vtable array, slot count, name),
  `X104Class` (name, runtime class name, interface table, flags, optional `qi_hook`). Define a vtable with the generated slot enums and the CHECKED slot macro:
  ```c
  X104_VTBL(x104_app_vt_statics, IApplicationStatics) = {
      X104_INSPECTABLE_SLOTS,                                   /* slots 0..5: QI, AddRef, Release, GetIids, GetRuntimeClassName, GetTrustLevel */
      X104_SLOT(IApplicationStatics, Start, x104_app_statics_start),     /* thunk type must equal X104_FN_IApplicationStatics_Start */
      X104_SLOT(IApplicationStatics, LoadComponentWithResourceLocation, x104_app_statics_load_component_loc)
  };                                                             /* every other slot becomes a named E_NOTIMPL stub */
  static const X104IfaceDesc x104_app_ifaces[] = { X104_IFACE(IApplicationStatics, x104_app_vt_statics), X104_IFACE(IApplicationFactory, x104_app_vt_factory) };
  X104_CLASS_DEF(x104_app_class, "ApplicationStatics", L"Windows.UI.Xaml.Application", x104_app_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
  X104_SINGLETON(x104_app_get_factory, x104_app_class, X104Object)      /* static X104Object *x104_app_get_factory(void) */
  ```
  A thunk gets `self` = the `X104View*`: `X104_THIS(MyStruct, self)` is the object (`typedef struct { X104Object base; ... } MyStruct;`, allocate with `x104_object_new(&cls, sizeof(MyStruct))`).
  Thunk signature = the generated typedef (`X104_FN_<Interface>_<Method>`, listed as the C signature in `units/SLOTS.md`), declared `HRESULT X104_API f(void *self, ...)` (X104_API is WINAPI = empty on ARM); the compiler does the AAPCS register
  assignment (TimeSpan/EventRegistrationToken `LONGLONG` in r2:r3, `double` in d0, output structs via pointers, input Size/Point/Rect by value in s0..). Enums are `int`, interface pointers `void *`.
  `X104_UNKNOWN_SLOTS` (3 slots) for IUnknown-only interfaces (ISwapChainPanelNative, IAgileObject-like), `X104_VTBL_N(name, count)` for hand-sized vtables.
  Holes: every NULL slot (also `X104_VTBL_SLACK`=4 extra slots behind the declared interface) is filled with a stub that logs `X104 UNIMPLEMENTED slot N of <iface> on <class> ... caller=<game rva>`
  (first call and every 2^n-th) and returns E_NOTIMPL. `x104_nop` is a ready S_OK slot (register it with `X104_SLOT_NOARGS`).
* **Aggregation**: `x104_aggregate_create(cls, size, outer, iid_instance, &inner, &instance, &obj, what)` implements the factory protocol of Application/Page/DispatcherTimer once: creates the object, stores `outer`, AddRefs `outer` exactly once (never released), returns `inner` = identity view and `instance` = the view of `iid_instance` (the declared default interface), logs everything. The units no longer re-implement it.
* **QI**: `x104_object_qi(obj, iid, &out)` (AddRef, logs `X104 QI MISS`); `x104_return_iface(obj, iid, out, what)` for getters; `X104Class.qi_hook` for dynamic answers.
* **Events**: `X104Event` (up to 8 handlers): `x104_event_init/add/remove/count/invoke` (R7). **Guarded calls**: `x104_call_guarded3(fn,a,b,c,&hr,&seh)`, `x104_invoke_delegate_guarded`.
* **Locks / once**: `X104Lock` (zero-initialised spin lock), `x104_lock/unlock`, `x104_once_begin/end`.
* **HSTRING**: `x104_hstring_create/_wide/_equals/_ascii/_free`, `x104_narrow` (wide -> printable ASCII for logs).
* **Logging**: `x104_logf` (wsprintfA formats, no floats, <= 1024 chars), `X104_LOG_LIMITED(&counter, max, ...)`, `X104_TRACE(...)` (only with `--trace-fakes`), `x104_describe_iid`, `x104_hr_name`,
  `x104_game_rva_of`.
* **Activation**: `X104FactoryDef {class name, get_factory}` + `x104_activate_from_table(defs, n, class_name, iid, out)`; the class -> unit owner table `x104_class_owners` lives in the core.
* **XAML element mixin**: `X104Element` + `X104_ELEMENT_IFACES` (IDependencyObject, IUIElement, IFrameworkElement) implementing `get_Dispatcher`, `Visibility`, `RenderSize`,
  `ActualWidth/Height`, `SizeChanged`, `FindName` (callback); `x104_element_new/_set_sizes`.
* **Completed action**: `x104_completed_action()` singleton (IAsyncAction + IAsyncInfo, status Completed); `put_Completed` queues an asynchronous guarded `Invoke(action, Completed)` through `x104_ui_post` (R12).
* **Game access**: `X104_CODE(rva)`/`X104_GAME_FN(type, rva)`, `x104_check_game_bytes`, `x104_game_mainpage/appmain/client/_client_has_holder/_is_ready`, `x104_read_u32/u8`; RVA constants `X104_RVA_*`,
  object layout constants `X104_MAINPAGE_*/APPMAIN_*/CLIENT_*`, geometry structs `X104Size/Point/Rect`.
* **TabTip / pointer source / text state**: `x104_osk_show/hide/poll/rect_client/note_pointer_up`, `x104_note_pointer_source(BOOL touch, why)`, `x104_touch_is_active_source()` - thin wrappers over the
  device-proven 1.2.8 controller (`arm32_osk_*`); **`x104_set_text_input_active(BOOL, why)`** sets the shared `text_input_active` the controller reads (input unit, R17).
* **Present accounting/pacing** (panel's Present stub): `x104_present_begin()`, `x104_present_sync(requested)` (`--vsync`), `x104_present_pace()` (`--max-fps=N`, automatic 1 ms sleep after 60 frames under 3 ms), `x104_present_end(hr, sync, flags)` (frame counters for the heartbeat/watchdog, first 20 results, every non-S_OK result, DEVICE LOST banner).
* **UI pass**: `x104_ui_pass()` = `x104_ui_pump_once()` + the three unit ticks + progress bump, re-entrancy guarded, no-op before `f_units_ready`; called by the loop and by the WM_TIMER pump.

### 5.1 On-device self test (`--selftest-fakes`, `--selftest-fakes=only`)
The x86 host-side test cannot verify ARM register passing, so without a device-side check the first ABI test would be the real game on the Surface 2. After the units are initialised and BEFORE the game's WinMain, `x104_selftest_fakes()` (win32_host_arm32_0104_selftest.inl) activates every implemented class through the same router the game uses and calls the slots the game uses THROUGH THE GENERATED TYPEDEFS, checking the returned values: ThreadPool item status machine and Cancel, `RunAsync` queues (R1), 64-bit `TimeSpan` round trip, late `Completed` handler, DisplayInformation floats, Window/CoreWindow + `IAgileObject`, panel double/float/Size getters + SizeChanged token by value, TextBox round trip, InputPane, Package/ApplicationData paths against `x104.game_root`/`data_root`, MemoryManager 64-bit outputs, Vibration TimeSpan, network level, plus the core services (HSTRING, events with 64-bit tokens, aggregation, completed action). Output: one `X104 selftest: [PASS|FAIL|SKIP] name - detail` line per check and a summary; a class whose unit is still a stub is a SKIP, every other activation failure a FAIL. `=only` exits afterwards (exit code 40 on failures, message box). The package ships `start_selftest.bat`. Run it first on a new device; the x86 core test runs the same file against the stubs (0 failures, all unit checks SKIP); the integration test (14b #2) runs it against the REAL units (43 PASS, 0 FAIL, 0 SKIP).

## 6. Global state (`x104`, struct `X104State`)
`image`, `instance`, `window`, `main_thread_id` (== `ui_thread_id`), `game_root` (exe dir == Package.InstalledLocation == cwd; must be ASCII), `data_root` (LocalFolder; platform unit sets),
options `opt_present/opt_half_res/opt_fullscreen/opt_trace/opt_width/opt_height/opt_window_explicit/opt_max_fps/opt_vsync/opt_selftest`, `original_activation`, `mainpage_outer` (MainPage = outer - 8), `exit_code`, and the ORDERING FLAGS (set once with
`x104_flag_set`, which logs): `f_mainpage_created` (app: IPageFactory.CreateInstance), `f_content_set` (app: Window.put_Content), `f_launched` (app: OnLaunched returned),
`f_size_changed_done` (panel: first SizeChanged returned), `f_timer_started` (async: timer Start), `f_ready_tick` (async: timer Stop after ready), **`f_exit_requested` (app only: exit requested, idempotency), `f_quit` (async only: x104_ui_quit ran), `f_loop_exit` (async + window proc: the loop must return), `f_units_ready` (boot: units initialised, routing enabled), `f_swapchain_created` (panel: the HWND stub created the chain / SetSwapChain attached it)**.
Command line options (boot parses them; the game also sees them): `--window=WxH` (explicit client size; default `min(1280x720, work area minus the frame)`, the window is centred and created HIDDEN), `--fullscreen`, `--present=hwnd|dcomp` (default hwnd = path B), `--half-res`
(keep the game's half-resolution GPU row; default is full resolution), `--max-fps=N`, `--vsync`, `--selftest-fakes[=only]`, `--trace-fakes`, `--trace-game-state` (game-state snapshots in the app unit + Direct3D draw tracer in the panel unit, parsed by those units themselves; see `HUD_INVESTIGATION.md`), plus the shared `--feature-level=`, `--input=`, `--touch-api=`, `--osk=`, `-maxfps`.

## 7. Bootstrap sequence (`x104_bootstrap`, win32_host_arm32_0104_boot.inl)
Before it: `Win32Bootstrap` sets `game_image`, `host_is_0104 = x104_image_matches(image)` (quiet PE check), makes the exe directory the cwd + `package_path`, opens the log
(`win32craft.log`, banner "Win32Craft Minecraft PE 0.10.4.40 ARM32 ..."), logs the OS version, starts the watchdog, `install_crash_trace()` (unhandled + vectored exception logging with game
rvas) and `arm32_install_abort_traps()`; then `if (host_is_0104) return x104_bootstrap(image);`.
1. `x104_identify_image` (log: base, SizeOfImage, sections 7, DllCharacteristics, patched call site bytes `09 f0 77 fc` at 0x200A72) - fail code 30.
2. `Win32CraftParseHostOptions()` (shared options) + `x104_parse_options()` + SUMMARY line.
3. HSTRING self-test, `RoInitialize(MTA)` (S_FALSE expected), `SetProcessDPIAware` (real pixels; the game is told 96 dpi / scale 1.0), `arm32_input_probe()` (input inventory, initial pointer source).
4. **`x104_platform_install(image)`** FIRST (paths: ASCII check of `x104.game_root`, data root, data-cell patches, XAudio2 probe) - fail code 31.
5. IAT hooks: activation router (cell 0x20B5FC -> `x104_activation_hook`, saves the vccorlib original), `_CxxThrowException` trace, D3DCompile logger, diagnostics (abort, terminate,
   _purecall, _exit_app, _exit, `__abi_WinRTraiseCOMException` and the 14 typed Platform exception raisers) - code 32. **Then the VC120 diagnostics: the loaded-module list (base, size) and a log-only forwarder in the `RoGetActivationFactory` import cell of every VC120 `_app` module that has one (msvcr120_app, vccorlib120_app; msvcp120_app has none - verified in the genuine DLL), logging class, IID, HRESULT and the calling module+offset of every activation the runtimes make on their own (ConcRT/PPL, tmpfile paths).**
6. `x104_panel_install(image)` (D3D11CreateDevice hook, profile patch, dxgi cells, float option) - code 33. Then `x104_async_install`, `x104_input_install`, `x104_app_install` - code 34.
7. Window: class `MCPE0104Win32`, `x104_window_proc`; **created HIDDEN (no WS_VISIBLE: shown by `Window.Activate`), centred, client = `--window` or the default clamped into the work area minus the frame (Surface RT 1366x768 with a 40 px taskbar: 1280x720 would put the bottom of the game off screen), effective size logged**; `x104.window = game_window = hwnd` - codes 35/36.
8. `x104_async_init(window)`, `x104_input_init(window)`, `x104_panel_init(window)`, `x104_app_init(window)` (this order: the CoreWindow/panel need the dispatcher; the app unit needs panel + input
   objects) - code 37. **Then `f_units_ready`: from here on window messages are routed to the units (R15).** 9. Verify the WinMain thunk bytes (ARM0104_STARTUP_FLOW_WINMAIN_THUNK_EXPECTED) - code 38.
9b. Optional `--selftest-fakes[=only]` (section 5.1) - code 40.
10. `winmain(hInstance, NULL, commandTail, SW_SHOWNORMAL)` inside `__try`: the game main runs, `Application::Start` (app unit) runs the UI loop inside it; code 39 on an escaped exception.
11. `x104_shutdown_units()` (clears `f_units_ready`, then input, panel, app, async, platform; all idempotent), watchdog stop, exit guard (30 s), return the game's result to the CRT.
**Window procedure routing** (`x104_window_proc`): step 1, valid at any time: `X104_WM_QUIT` (store exit code, `f_loop_exit`, `PostQuitMessage` - ends a nested modal loop), `X104_WM_WAKE`, WM_TIMER `X104_TIMER_PUMP` -> `x104_ui_pass()`, WM_ERASEBKGND -> 1, WM_SYSCOMMAND/SC_KEYMENU -> consumed (no Alt/F10 menu mode; the key reached the game). Step 2: before `f_units_ready` everything else is `DefWindowProc` (R15). Step 3: WM_SIZE/ACTIVATE/ACTIVATEAPP/SHOWWINDOW/SETFOCUS/KILLFOCUS/ENTERSIZEMOVE/EXITSIZEMOVE/WINDOWPOSCHANGED/DISPLAYCHANGE/DPICHANGED are
BROADCAST to panel, input, app (each hook returns TRUE if it fully handled the message; the first TRUE decides the result, all three see the message); every other message goes to input,
panel, app in that order and stops at the first TRUE. Step 4 defaults when no unit handled it: WM_PAINT validates, WM_SYSKEYDOWN/UP -> swallowed except Alt+F4, WM_SYSCHAR swallowed, WM_CLOSE -> `x104_app_request_exit(0)`, WM_DESTROY -> `PostQuitMessage(0)`, else DefWindowProcW.
Message ownership (review gap: nobody owned them): WM_SYSCOMMAND/SC_KEYMENU and the default SYSKEY swallowing = boot; WM_SYSKEYDOWN/UP delivery to the game = input; WM_DISPLAYCHANGE / WM_DPICHANGED = app (log + optional display events); WM_SIZE/WM_GETMINMAXINFO/ENTER/EXITSIZEMOVE = panel; WM_CLOSE/minimise/restore = app.

## 8. Cross-unit API (every function another unit may call)
All are `static` functions of the one TU, prototyped (with their exact C signatures) in `win32_host_arm32_0104_core.inl` section 14b, DEFINED by the named unit (the stubs define them all). "UI" = UI thread only; "any" = any thread.
| function | owner | thread | semantics |
|---|---|---|---|
| `BOOL x104_<unit>_install(BYTE *image)` (async, app, panel, platform, input) | each | UI (boot) | patch the game image / IAT / cells before the window exists; FALSE = fatal with a message box |
| `BOOL x104_<unit>_init(HWND)` (async, input, panel, app) | each | UI | create objects; window exists; order async, input, panel, app |
| `void x104_<unit>_shutdown(void)` (all five; async: `x104_async_shutdown`) | each | UI | idempotent cleanup at exit |
| `HRESULT x104_<unit>_activate(class, iid, out)` | each | any | WinRT activation of the classes the core table assigns (section 11); S_OK + AddRef'd interface, E_NOINTERFACE, REGDB_E_CLASSNOTREG = not implemented |
| `X104Object *x104_async_dispatcher_object(void)` | async | any | the CoreDispatcher (not AddRef'd); used by element `get_Dispatcher`, CoreWindow, input source |
| `int x104_ui_run_loop(void)` | async | UI | message/queue/timer loop; calls the core's `x104_ui_pass()` every iteration (<= 10 ms apart), leaves on `X104_WM_QUIT`/`WM_QUIT` or `f_loop_exit`; returns `x104.exit_code` |
| `void x104_ui_quit(int)` | async | any | makes the loop return (sets `exit_code`, `f_quit` (async's flag), `f_loop_exit`, wakes, posts `X104_WM_QUIT`) |
| `BOOL x104_ui_post(fn, arg)` | async | any | FIFO item run on the UI thread at loop top level; FALSE ONLY after `x104_ui_quit` (`f_quit`) |
| `void x104_ui_pump_once(void)` | async | UI | run queued items + due timers once (re-entrancy safe); only `x104_ui_pass()` calls it |
| `void x104_async_shutdown(void)` | async | UI | cancel actions (Canceled), wake ProcessEvents waiters, wait <= 2 s for workers |
| `void x104_app_ui_tick(void)` / `BOOL x104_app_window_message(...)` | app | UI | lifecycle bookkeeping / window messages (WM_CLOSE, minimise/restore, DISPLAYCHANGE/DPICHANGED) |
| `void x104_app_request_exit(int)` | app | any | graceful exit, idempotent on `f_exit_requested`: posts to the UI thread: Suspending handlers ONLY if the game is ready (guarded), `x104_async_shutdown`, `x104_ui_quit`; inline fallback on the UI thread if the post is refused |
| `BOOL x104_app_raise_back_pressed(void)` | app | UI | BackPressed to the game; TRUE = Handled |
| `void x104_display_raise_dpi_changed / _orientation_changed / _contents_invalidated(void)` | app | UI | DisplayInformation events |
| `X104Object *x104_panel_object(void)` | panel | any | SwapChainPanel (identity view; not AddRef'd) - the app's `FindName("swapChainPanel")` |
| `void x104_panel_raise_size_changed(void)` | panel | UI | SizeChanged with the current client size, honouring R2 |
| `void x104_panel_client_size(float*, float*)` | panel | any | client size in DIPs (== pixels) |
| `void x104_panel_ui_tick(void)` / `BOOL x104_panel_window_message(...)` | panel | UI | first SizeChanged, resize coalescing (live during drags), device-lost poll, swap-chain 3 s rule / WM_SIZE etc. |
| `X104Object *x104_input_core_window(void)` | input | any | the CoreWindow (`IWindow.get_CoreWindow`) |
| `X104Object *x104_input_element_logo/editbox/button(void)` | input | any | `FindName("Logo"/"EditBox"/"Button")` |
| `HRESULT x104_input_create_independent_source(UINT types, void **result)` | input | pool thread | panel `CreateCoreIndependentInputSource` slot 10 |
| `void x104_input_raise_visibility_changed(BOOL)` | input | UI | CoreWindow.VisibilityChanged (called by the app unit's lifecycle code) |
| `void x104_input_release_all_pointers(const char*)` | input | UI | Released for every pressed pointer |
| `void x104_inputpane_raise(BOOL showing)` | input | UI | InputPane Showing/Hiding; called by the shared TabTip controller through `arm32_input_pane_raise` |
| `void x104_input_ui_tick(void)` / `BOOL x104_input_window_message(...)` | input | UI | `x104_osk_poll()`, deferred input work / pointer, key (incl. SYSKEY), char, focus messages |
Completed async actions for fire-and-forget results (StatusBar.HideAsync): `x104_completed_action()` (core).

## 9. Existing win32_host.c code that is reused, and the guarded edits
**Reused** (all `static` in win32_host.c, defined above the include anchor): `log_line/log_hresult/log_pointer`, `Win32CraftLogPathW`; `arm32_patch_cell/_byte`, `arm32_check_bytes`, `arm32_write_thumb_jump`, `arm32_hex_dump`;
`arm32_iat_hook_import/_resolve_cell/_find_cell/_describe_address/_argument_value`, `host_next_argument`, `host_argument_equals`, `Win32CraftParseHostOptions` (+ `arm32_feature_level_cap`, `arm32_osk_policy`, `arm32_input_policy`);
`arm32_boot_step/_narrow/_start_exit_guard/_watchdog_stop/_progress/_frames_started/_frames_done/_note_frame/_start_tick`, `install_crash_trace`, `arm32_install_abort_traps`, `install_exception_trace`, `arm32_log_stack_scan/_log_context`,
`install_d3d_compile_trace` (D3DCompile logger), `arm32_iat_log_blob_text`; the TabTip controller `arm32_osk_show/hide/poll/rect_buffer/note_pointer_up` + `arm32_input_pane_raise` + the global `text_input_active`;
`arm32_input_probe/_note_source/_touch_active` and the resolved Win32 touch API table `arm32_api`; HSTRING via api_compat.c (combase). **Read, do not call** (they feed 1.2.8 structures):
`arm32_touch_pointer_message`, `arm32_touch_wm_touch`, `arm32_legacy_touch_message`, `arm32_legacy_mouse_filter` - they hold the device-proven WM_POINTER / WM_TOUCH / emulated-mouse decoding.
**Not used for 0.10.4**: `window_proc`, `initialize_game_d3d`, `initialize_game_app_128`, `render_game_target_128`, `drain_dispatcher_queue`, `install_arm32_iat_hooks`, `install_activation_redirect`,
`install_arm32_platform_fixes`, every `fake_*` 1.2.8 object, `create_user_data()` (it writes a 1.2.8 options.txt).
**Edits made to win32_host.c (all inside `#ifdef WIN32CRAFT_ARM32`; `tools/arm32/check_x86.py` = IDENTICAL; none was needed for the review fixes)**
1. `static BOOL host_is_0104;` next to `host_is_128`.
2. Forward declarations `x104_inputpane_raise` and `x104_activation_hook` next to the other ARM forward declarations (line ~339).
3. `arm32_input_pane_raise`: first statement `if (host_is_0104) { x104_inputpane_raise(showing); return; }`.
4. `win32_get_activation_factory`: first statement `if (host_is_0104) return x104_activation_hook(...)` (safety net: never hand out the 1.2.8 fakes).
5. Log banner of `arm32_boot_setup_paths_and_log` prints the version label from `host_is_0104`.
6. Before `int WINAPI Win32Bootstrap(void)` (ARM branch): `#include "win32_host_arm32_0104.inl"`; inside it: `host_is_0104 = x104_image_matches(image);` and, after `arm32_install_abort_traps();`, `if (host_is_0104) return x104_bootstrap(image);`.

## 10. Logging contract and the healthy log (M1 acceptance outline)
Prefix `X104` on every host line (the shared helpers log `ARM32 ...`). A healthy start shows, in this order (counts abbreviated): identification + options + platform banner (game root, data root,
patched cells) -> module list + VC120 forwarders -> window line (work area, effective client size, hidden) -> (optional selftest block) -> `activation #n Windows.UI.Xaml.Application IApplicationStatics -> unit app: S_OK` -> `Application::Start entered` -> App2 `aggregate: IApplicationFactory.CreateInstance created ...` -> `OnLaunched` ->
StatusBar.HideAsync -> Page `aggregate: IPageFactory.CreateInstance` -> `Application.LoadComponentWithResourceLocation(ms-appx:///MainPage.xaml)` -> `FindName swapChainPanel/Logo/EditBox/Button` ->
`CoreWindow.add_VisibilityChanged`, `DisplayInformation add_DpiChanged/add_OrientationChanged/add_DisplayContentsInvalidated`, panel `add_CompositionScaleChanged`, `add_SizeChanged`,
`HardwareButtons.add_BackPressed`, `InputPane add_Hiding/add_Showing` -> `D3D11CreateDevice request levels [B100 B000 A100 A000 9300] -> [9100] ... reported 9300` (+ adapter/diagnostics) ->
work item #1 (input source, High/TimeSliced) -> `Canvas.SetLeft` x2 -> Package.InstalledLocation.Path, ApplicationData.LocalFolder.Path, MemoryManager, work item #2 (init job) -> timer
created/interval 500000 ticks/add_Tick/Start -> `Window.put_Content` (`f_content_set`) -> `Window.Activate` (window shown) -> `OnLaunched returned` -> `UI loop entered` -> shader compiles (`D3DCompile #1..#3 ok`) ->
`first SizeChanged raised 1280x720` -> swap chain creation lines (path B stub, `IDXGISwapChain2 vtable[34]` + `vtable[8] Present` patched, `SetSwapChain ... path B ok`) -> `flag: f_size_changed_done` -> init job finished -> `READY tick` (Stop) -> HID::Initialize registrations
(`KeyDown/KeyUp/PointerPressed/...` added, work item #3 = input source thread) -> work item #4 = render loop -> `X104 present: FIRST Present returned 0x00000000 S_OK` -> `frame N complete` lines and a heartbeat `frames done=N (+M in the last 30 s)` that GROWS. No `UNIMPLEMENTED`, no `QI MISS`, no
`raises Platform::...Exception`, no `EXCEPTION escaped` line, no `dxgi vtable mismatch`, no `no swap chain was created`. Any such line is the next thing to fix.

## 11. WinRT classes the host fakes (23 classes / 36 call sites; owner = `x104_class_owners` in the core)
| class (activation string) | IIDs requested | game call sites | owner |
|---|---|---|---|
| Windows.UI.Xaml.Application | IApplicationStatics (Start 7, LoadComponentWithResourceLocation 9), IApplicationFactory (CreateInstance 6) | 0x1EECBC, 0x1EE424, 0x1E2CF0 | app |
| Windows.UI.Xaml.Controls.Page | IPageFactory | 0x1DD874 | app |
| Windows.UI.Xaml.Window | IWindowStatics | 0x1DAC80 | app |
| Windows.UI.Xaml.Controls.Canvas | ICanvasStatics | 0x1DFAB8 | app |
| Windows.UI.ViewManagement.StatusBar | IStatusBarStatics | 0x1E3394 | app |
| Windows.Phone.UI.Input.HardwareButtons | {594B8780-...} (manual) | 0x1E1798 | app |
| Windows.Graphics.Display.DisplayInformation | IDisplayInformationStatics | 0x4AF58, 0x1E183C | app |
| Windows.Foundation.Uri | IUriRuntimeClassFactory | 0x1088A0 | app |
| Windows.System.Threading.ThreadPool | IThreadPoolStatics | 0x1DA4A4, 0x114078 | async |
| Windows.UI.Xaml.DispatcherTimer | IDispatcherTimerFactory | 0x1DCE94 | async |
| Windows.UI.ViewManagement.InputPane | IInputPaneStatics | 0x1DE9CC | input |
| Windows.UI.Core.CoreCursor / CoreWindow | ICoreCursorFactory / ICoreWindowStatic | 0x1102E8 / 0x111420 | input |
| Windows.Storage.ApplicationData, Windows.ApplicationModel.Package, Windows.System.MemoryManager | statics | 0x1DAB28, 0x4E840, 0x4E69C/0x4E740 | platform |
| Windows.Phone.Devices.Notification.VibrationDevice, Windows.Networking.Connectivity.NetworkInformation, Windows.ApplicationModel.Store.CurrentAppSimulator (+CurrentApp), Windows.Foundation.Diagnostics.AsyncCausalityTracer, Windows.Web.Http.* | statics | 0x4AEB8, 0x4B108, 0x4B44C/0x4E93C, 0x4B708.., 0x108698.. | platform |
The panel, the CoreWindow and the three XAML elements are NOT activated: they are reached through `Page.FindName` / `Window.get_CoreWindow` / `ISwapChainPanel.CreateCoreIndependentInputSource`.
The VC120 `_app` runtimes (msvcr120_app / vccorlib120_app) also activate classes themselves through the REAL `RoGetActivationFactory` (PPL scheduling, tmpfile paths, ConcRT): **not routed (they get the real runtime) but now LOGGED by the boot's forwarders** (section 7 step 5); they are not on the boot path of the game's own code (`platform_services.verdicts` M4), the forwarders make a surprise visible instead of a silent hang.

## 12. Known risks and open questions (cannot be settled offline) and what the code does about each
1. RT 8.1 loadability: 8 unproven api-set contracts, VC120 `_app` DLLs with invalid Authenticode, 436 static ctors; `loadcheck` on the device decides (M0). *Mitigation in the package (section 13):* app-local api-set forwarder DLLs + UCRT overlay for every contract the shipped PEs import, `loadcheck.exe` with the 0.10.4 module list, `start_selftest.bat`; the host logs the loaded module list at boot.
2. Presentation path B: dxgi `.rdata` page patching, `CreateSwapChainForHwnd` with FLIP_SEQUENTIAL + STRETCH on Win8.1/RT is unverified (fallback chain in `units/panel.md`); `Present` from the render thread on an HWND chain while the UI thread is blocked on the AppMain lock (DXGI may need the window thread) is a theoretical deadlock. *Mitigation in code:* the factory stub records the chain it made; `SetSwapChain` compares and falls back to path A with an ERROR line on a vtable mismatch; the 3 s rule logs a black window; the Present stub brackets every frame (a never-returning Present shows in the watchdog line as "a game frame call has not returned"), logs every non-S_OK result and can pace (`--vsync`, `--max-fps`, automatic).
3. D3D level 9_1 on Tegra: shaders proven only on desktop/WARP; texture 2048 cap; 32-bit indices; device removal is unrecoverable in the game (fatal in the host). *Mitigation in code:* the panel unit logs the adapter/feature level/caps and wraps `CreateTexture2D`/`CreateBuffer` of the device vtable to log every failure with its description and every texture over 2048 / non-4-byte index buffer BEFORE the call (diagnostic only); the Present stub flags DEVICE LOST/RESET.
4. IApplicationFactory aggregation refcounts (m9): the host AddRefs `outer` once (now in `x104_aggregate_create`) and keeps it; if a double free shows up, look at App2/MainPage `+0x34` control block.
5. TabTip on RT 8.1 is unmeasured (see `arm32/OSK_NOTES.md`); the controller falls back through signals and logs which one it used (`ARM32 osk:` lines).
6. `SuspendingEventArgs` / `IApplicationData` Values container semantics: the handlers 0x1E3DA0/0x1E3C34 ignore the args and only need `LocalSettings` (slot 10) non-NULL
   and `Values` (slot 8) callable (may return S_OK + NULL); MainPage::SaveState (0x1DFA40) = lock, `DR::Trim`, StopRenderLoop, client suspend. Suspending is skipped before the game is ready (R16).
7. **ConcRT activation (new):** the game's `AppMain+0x10` is a ConcRT `critical_section` taken by the UI thread and the render thread every frame; how the `_app` flavour's contended slow path attaches a thread to the default scheduler (and whether that activates something through the real `RoGetActivationFactory`) is not verified. The recon only proved that no `Concurrency::task` runs on the boot path. *Mitigation in code:* the VC120 forwarders log any such activation with its caller module+offset; a failing one would otherwise be a silent hang/crash.

## 13. Packaging (owner: designer; tool `tools/make_0104_package.py`)
The 1.2.8 packaging tools (`make_arm32_package.py`, `make_arm32_device_package.py`) are 1.2.8-specific and `tools/loadcheck/loadcheck.c` had a hard-coded 1.2.8 module list; the first foundation had no owner for the 0.10.4 package. Now:
* `python tools\make_0104_package.py [--build] [--game-dir DIR] [--allow-stubs] [--no-apiset]` builds `dist-0104\Minecraft_PE_0.10.4.40_ARM32_Win32Craft\` + `.zip`: (1) refuses to run while any unit file still contains `*** STUB` (unless `--allow-stubs`, which names the package `...-FOUNDATION-ONLY` and says so in README/MANIFEST), (2) optionally runs `build_arm32.bat` (and `build_loadcheck_arm32.bat`), refuses a `win32craft.dll` older than its sources, (3) runs `tools/arm32-patcher.py patch --profile v0_10_4 <game folder> <out> --host-dll dist-arm32\win32craft.dll` (patched exe `Win32Craft.0.10.4-arm32.exe`, the three patched VC120 `*_app` DLLs from the genuine VCLibs appx, `patch_report.json`), (4) scans every PE of the folder for api-set imports and generates the app-local forwarder DLLs for the contracts Windows RT 8.1 lacks (`make_arm32_apiset_shims.py --out ... --pe ...`: the 8 unproven contracts of `pe_patch_plan` section 6 and everything else imported) plus the SDK UCRT (`ucrtbase.dll` + `api-ms-win-crt-*`), (5) copies `loadcheck.exe` (module list now includes `Win32Craft.0.10.4-arm32.exe`, `vccorlib120_app.dll`, `msvcp120_app.dll`, `msvcr120_app.dll`), (6) writes the start batch files (`start.bat`, `start_selftest.bat`, `start_window_1024.bat`/`--window=`, `start_dcomp.bat` = `--present=dcomp`, `start_halfres.bat`, `start_fl93.bat`/`start_fl100.bat` = `--feature-level=`, `start_osk_on.bat`/`start_osk_off.bat`, `start_vsync.bat`, `start_trace.bat`), `README_DEVICE.txt` (what to send back: `win32craft.log`, `loadcheck.txt`; the ASCII-path warning: the game folder AND the profile/data path must be ASCII-only, the host refuses otherwise) and `MANIFEST.txt` (SHA-256 of every file, input hashes, patcher report summary, build stamp), (7) dependency-checks every PE in the folder.
* The tool never writes into the game folder (the patcher copies it) and warns when the OUTPUT path is not ASCII-only.

## 14. Decisions after review
The independent critic reviewed the first foundation. Every finding was checked against the facts and the code; all were accepted, two with a correction noted below. "Fix" names what changed.
| # | severity | finding | verdict | fix |
|---|---|---|---|---|
| 1 | blocker | `X104_TIMER_PUMP` defined only in boot.inl, which is included last, but briefed to the async unit | confirmed (include order core, async, app, panel, platform, input, boot) | the macro moved to the core next to `X104_WM_QUIT/WAKE`; a lint in `try_0104.py` rejects any unit reference to a boot/selftest symbol or to another unit's private symbol; the umbrella documents "boot is last" |
| 2 | high | `f_quit` had two meanings (app exit guard vs async "loop quitting"); with the app setting it first `x104_ui_post(exit proc)` returned FALSE and X did nothing | confirmed | split: `f_exit_requested` (app only) and `f_quit` (async only, set in `x104_ui_quit`); `x104_ui_post` refuses only after `f_quit`; the app brief adds the inline fallback on the UI thread if the post fails; try_0104.py lints the ownership; `f_loop_exit` added for the loop |
| 3 | high | path B silently assumes the game's factory shares the patched vtable | confirmed (cannot be proven offline) | the stub records its chain/vtable/attempt (`created_by_stub`, `f_swapchain_created`); `SetSwapChain` compares and falls back to path A with an ERROR (`dxgi vtable mismatch`); a 3 s rule logs "no swap chain was created"; slot 34 and slot 8 patched per cell on every vtable of the chain (brief section 7 steps 1-5) |
| 4 | high | no frame counting, no Present HRESULT logging, pacing assumed | confirmed (the 30 s heartbeat uses `arm32_boot_frames_done`, fed only by the 1.2.8 loop) | the panel patches `IDXGISwapChain::Present` (slot 8) with a stub using the core helpers `x104_present_begin/_sync/_pace/_end` (frame counters + `arm32_boot_note_frame`, first 20 + every non-S_OK result, DEVICE LOST banner); `--vsync`, `--max-fps=N`, automatic 1 ms sleep after 60 frames under 3 ms. The "cheap alternative" (bump the counters from the render item's `get_Status` poll) was NOT taken: it would count polls, not frames, and would not see the HRESULT |
| 5 | medium | `text_input_active` never set in 0.10.4, so tap-to-reopen never works and hardware keys flip the source while the keyboard is up | confirmed (win32_host.c: `arm32_osk_poll` clears `arm32_osk_dismissed_by_user` when it is FALSE, `arm32_input_note_key` tests it) | core wrapper `x104_set_text_input_active` (logs transitions); input brief: set on every EditBox/Button Focus, FALSE at shutdown; acceptance check for `tap while the text box still has focus - opening the keyboard again`; OSK_NOTES updated |
| 6 | medium | Suspending raised after OnLaunched even before the init job finished (handler touches `client+0x118/+0x11C`) | confirmed | R16: Suspending only when `x104_game_is_ready()`, else `exit before ready: Suspending skipped`; async_shutdown never waits beyond its 2 s |
| 7 | medium | Page `CreateInstance` returned views[0] for the `IPage*` out parameter; LaunchActivatedEventArgs primary view was `IActivatedEventArgs` instead of `ILaunchActivatedEventArgs`; no aggregation helper | confirmed (SDK: `IPageFactory::CreateInstance(..., IPage **)`, `IApplicationOverrides::OnLaunched(ILaunchActivatedEventArgs*)`) | `x104_aggregate_create` in the core (outer AddRef'd once, inner = identity, instance = requested view); Page returns the IPage view; LaunchActivatedEventArgs views[0] = ILaunchActivatedEventArgs and OnLaunched gets that view; R10 extended |
| 8 | medium | the ABI comment said structs are never passed by value; vtable arrays are untyped so a wrong thunk signature compiles | confirmed (WinRT passes input structs by value: Measure/Arrange/put_RenderTransformOrigin/put_PointerPosition) | comments corrected; `gen_0104_iids.py` emits `X104_FN_<I>_<M>` typedefs (654 slots; enums = int, output structs via pointer, input Size/Point/Rect by value, unsupported types get no typedef); `X104_SLOT` / `X104_SLOT_NOARGS` refuse a mismatch at compile time (`__builtin_choose_expr` + `__builtin_types_compatible_p`); raw initializers are lint errors; the x86 test compiles 1 positive and 4 negative cases |
| 9 | medium | X104_WM_QUIT lost inside a modal loop; ticks do not run in modal loops so `in_size_move` was dead; Alt/F10 open the menu loop | confirmed | the window proc handles `X104_WM_QUIT` (`f_loop_exit` + `PostQuitMessage`); the loop tests `f_loop_exit` after every pass/message; the WM_TIMER pump runs the whole `x104_ui_pass()` (queue, timers, the three ticks) so the panel's live resize branch is live; SC_KEYMENU and unclaimed SYSKEY/SYSCHAR are consumed (Alt+F4 stays); input delivers F10/Alt as keys and consumes them |
| 10 | medium | window created visible and not pumped; default 1280x720 exceeds the Surface RT work area | confirmed (759 px outer height > 728 work area) | created hidden, shown by `Window.Activate` (+ 3 s safety net in the app tick); default size clamped to the work area minus the frame, centred; explicit `--window=` is honoured (warns if it does not fit); effective size logged. I chose clamping rather than a borderless default: a framed window keeps the close button and works with Alt+F4 |
| 11 | medium | VC120 runtimes call `RoGetActivationFactory` on their own and nothing routes/logs it | confirmed with a correction: only `msvcr120_app` and `vccorlib120_app` import it (checked in the genuine DLLs of the VCLibs appx); `msvcp120_app` does not | boot installs log-only forwarders in the import cells that exist, logs the module list (base, size) and adds ConcRT activation to the risk list (12.7). Not routed to the fakes: they must reach the real runtime |
| 12 | medium | completed action stored no handler and never invoked one | confirmed | `put_Completed` AddRefs the handler and queues a guarded `Invoke(action, Completed)` via `x104_ui_post` (never inline, R1), logs the first uses; the async unit's actions follow the same contract (R12); tested on x86 |
**Coverage gaps closed:** packaging (section 13, `tools/make_0104_package.py`, loadcheck module list, README/MANIFEST, ASCII-path warning); Present slot 8 owned by the panel (finding 4); `text_input_active` owned by the input unit (5); VC120 activations logged by the boot (11); flag ownership split (2) - the **shader profile byte** is written by the exe patcher (default ON) AND verified/idempotently applied by the panel unit, now documented as one value with two writers (the panel accepts both byte states and adapts the digit to the real device level); the **cwd** is set by `Win32Bootstrap` and re-checked by the panel unit (benign, documented); the two IAsyncAction implementations now share the Completed contract; window messages WM_SYSCOMMAND/SC_KEYMENU, WM_SYSKEYDOWN/Alt/F10 (boot default + input delivery), WM_DISPLAYCHANGE/WM_DPICHANGED (app) have owners (section 7); "safe before init" is rule R15 with the `f_units_ready` gate; the early-exit rule is R16; the on-device `--selftest-fakes` exists (5.1); the three open risks without code mitigation now have one (12.1-12.3).

## 14b. Integration pass (2026-10-18) - the five implemented units together
Scope: the combined tree (core + async + app + panel + platform + input + selftest + boot) built, linked and run on the PC through a mock game; contract requests of the implementers decided; the boot traced hop by hop. Everything below is verified by `python tools/arm32/try_0104.py --selftest --integration` (+ `tools/arm32/check_x86.py`, `build_arm32.bat`).
| # | topic | decision / fix |
|---|---|---|
| 1 | combined-tree compile/link | clean on the first try: 0 ARM errors, 0 warnings in 0104 files, `win32craft.dll` links (399872 bytes), x86 of `win32_host.c` / `api_compat.c` byte-identical, lint clean (no unit uses a boot/selftest/other-unit private symbol). No name clash, no prototype mismatch between units |
| 2 | whole-tree integration test (new) | `tools/arm32/test_0104_all.c` (run: `python tools/arm32/test_0104_core.py --src tools/arm32/test_0104_all.c`, or `try_0104.py --integration`): the REAL core + five units + selftest in one TU, a mock game that makes the calls of startup_flow table 2a in the same order through the same slots (activation router -> `Application::Start` -> App2 aggregation -> OnLaunched -> StatusBar -> Page aggregation -> FindName x4 + QI of every element -> all event registrations -> input work item -> Canvas -> Package/ApplicationData/Memory -> init job -> DispatcherTimer -> Window.Content/Activate -> UI loop -> first SizeChanged -> ready Tick -> HID::Initialize registrations -> StartRenderLoop) and a tester thread that drives the real window procedure (mouse, keys, Esc = Back, characters, InputPane, resize, minimise/restore, WM_CLOSE -> Suspending -> shutdown). 149 checks: ordering R2/R3 (`OnLaunched < content_set < first SizeChanged < first Tick < ready Tick < attach < render loop`), no Tick before `f_size_changed_done`, args pointers are the R10 views (`get_NewSize`, `get_CurrentPoint`, `get_VirtualKey`, `get_Visible`, `put_Handled` called straight on the pointer), pointers delivered exactly once to the HID source, exactly one TextChanged per modification (R14), all pool threads and the two blocked `ProcessEvents(2)` items end at shutdown, no `UNIMPLEMENTED` / `QI MISS` / `EXCEPTION escaped` / `ERROR` / refcount-underflow line. It also runs `x104_selftest_fakes()` against the real units: 43 PASS, 0 FAIL, **0 SKIP** (every check of section 5.1 now exercises a real implementation). It cannot prove ARM register passing, the game's own code, D3D/DXGI |
| 3 | `test_0104_core.py --src` (request of the async implementer) | the five `SLOT_TEST_*` compile tests run only for `test_0104_core.c`; the core harness now compiles against the pristine stubs (`-I arm32/v0_10_4/stubs` first), so it keeps testing the TOOLKIT while the units are real |
| 4 | async: `MsgWaitForMultipleObjectsEx` flags | fixed in the unit review: dwFlags was 1 (MWMO_WAITALL), now `MWMO_INPUTAVAILABLE` (4): a window message or a cross-thread post no longer waits up to 10-16 ms |
| 5 | core: `X104_QS_ALLINPUT` | 0x04FF -> 0x1CFF (adds QS_TOUCH 0x0800 and QS_POINTER 0x1000 of the Windows 8 SDK): a WM_POINTER / WM_TOUCH wakes the loop at once (ARM-only constant, x86 unchanged) |
| 6 | async: `ProcessEvents(0/1/3)` on the UI thread | no longer pumps the queue (R1: a fake called by the game must not run delegates, the caller may hold the AppMain lock); logged no-op like option 2. The game only calls `ProcessEvents(2)` on pool threads (facts input_text), so nothing changes in practice |
| 7 | async: shutdown log buffer | the "items still running" list used `used + 12/14 < 200`; a worst-case 13-character entry could overrun by 1-2 bytes: margins raised to 24/28 |
| 8 | app: `StopRenderLoop` NULL deref | fixed in the unit review: Suspending (exit proc) and the visibility change are delivered only when `AppMain+0xC != NULL` (the game's StopRenderLoop dereferences it without a check inside the AppMain scoped_lock; the SEH guard would leave the non-recursive lock held for ever). Held-back visibility is retried every pass; the exit proc logs `Suspending skipped: render loop never started` |
| 9 | exit path | `WM_CLOSE` -> `x104_app_request_exit` -> exit proc on the UI loop (Suspending if ready and the render loop exists, else logged skip R16) -> `x104_async_shutdown` (cancels every action, wakes the `ProcessEvents` waiters, <= 2 s) -> `x104_ui_quit` -> `Application::Start` returns S_OK -> the wrapper returns -> `x104_shutdown_units` -> exit guard (30 s) |
| 10 | image / hook sanity against the real patched exe | `dist-0104\...\Win32Craft.0.10.4-arm32.exe` (built by `make_0104_package.py`): machine 0x1C4, SizeOfImage 0x00E6F000, 7 sections, DllCharacteristics 0x8140, entry 0x20095D, call site 0x200A72 = `09 f0 77 fc`, profile string `_level_9_1`; all 25 IAT cells the boot hooks (activation router 0x20B5FC, D3D11CreateDevice 0x20B1A4, D3DCompile, abort/terminate/_purecall/_exit*/_CxxThrowException, COMException + 14 typed raisers) resolve BY NAME at the expected RVA; 183 expected-byte arrays of the header match (the 6 "mismatches" are exactly the bytes the patcher changes) |
Contract requests of the implementers: (a) skip the core-only compile tests for other harnesses -> done (#3); (b) async: no cross-unit API change; (c) nothing else was asked of the designer. The remaining LOW review items that are not code defects are listed in `INTEGRATION_NOTES.md` section 6.
