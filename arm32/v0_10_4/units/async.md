# Unit brief: ASYNC  (file `win32_host_arm32_0104_async.inl`)

Read first: `../HOST_DESIGN.md` (rules R1-R14, toolkit, threading), `SLOTS.md` (signatures), then this brief. Your compile check:
`python tools/arm32/try_0104.py --unit async`. You own the thread pool, the async operations, the CoreDispatcher, the DispatcherTimer and the UI-thread loop that every other
unit depends on. The two properties that matter most: **(1) the render loop must see `IAsyncInfo.Status == Started` until it is cancelled**, **(2) no game delegate may ever run
inside a fake method (RunAsync always queues)**.

## 1. Contract functions you must define (exact signatures, see the stub)
| function | thread | semantics |
|---|---|---|
| `BOOL x104_async_install(BYTE *image)` | UI (boot) | nothing to patch; return TRUE (log "X104 async: install ok") |
| `BOOL x104_async_init(HWND window)` | UI | create the dispatcher object, the UI queue + wake event, state locks; `x104.ui_thread_id` is already set by the boot; `SetTimer(window, X104_TIMER_PUMP, 10, NULL)` (**`X104_TIMER_PUMP` is defined in the CORE** next to `X104_WM_QUIT`/`X104_WM_WAKE` - the boot is included last, so a unit can never use a boot-only name; try_0104.py lints this) so that queue, timers AND the unit ticks keep running inside the modal loops Windows runs for window move/size and menus: the boot's window proc answers that WM_TIMER with the core's `x104_ui_pass()` (= your `x104_ui_pump_once()` + `x104_panel_ui_tick/x104_input_ui_tick/x104_app_ui_tick`, guarded against re-entry and a no-op before `x104.f_units_ready`) |
| `void x104_async_shutdown(void)` | UI | idempotent: set quit, Cancel every running action (status 2), wake `ProcessEvents(ProcessUntilQuit)` waiters, wait up to 2 s TOTAL for the worker threads, log which ones did not finish |
| `HRESULT x104_async_activate(class, iid, out)` | any | classes `Windows.System.Threading.ThreadPool` (IThreadPoolStatics) and `Windows.UI.Xaml.DispatcherTimer` (IDispatcherTimerFactory) via `x104_activate_from_table` |
| `X104Object *x104_async_dispatcher_object(void)` | any | the single CoreDispatcher object (not AddRef'd), NULL before init |
| `int x104_ui_run_loop(void)` | UI | the loop (section 4); returns the exit code of `x104_ui_quit` |
| `void x104_ui_quit(int code)` | any | `x104.exit_code = code; x104_flag_set(&x104.f_quit, ...)` (**`f_quit` is OWNED BY YOU and means exactly "x104_ui_quit ran, the loop is leaving"**; the app unit's exit guard is the separate `x104.f_exit_requested`), `InterlockedExchange(&x104.f_loop_exit, 1)`, signal the wake event, `PostMessageW(x104.window, X104_WM_QUIT, code, 0)` |
| `BOOL x104_ui_post(void (*fn)(void *), void *arg)` | any | FIFO item for the UI thread (same queue as dispatcher items). **Refuses (FALSE) ONLY after `x104_ui_quit` set `f_quit`** - never because an exit was merely requested: the app unit posts its exit proc through this function AFTER it set its own `f_exit_requested`, and that item must be accepted (the review's "clicking X does nothing" bug was one flag with two meanings) |
| `void x104_ui_pump_once(void)` | UI | run queued items (<= 64) + due timers once; re-entrancy guard. Called only by the core's `x104_ui_pass()` (the loop and the WM_TIMER pump), never directly by other units |
Per-pass hooks: you do NOT call `x104_panel_ui_tick()` / `x104_input_ui_tick()` / `x104_app_ui_tick()` yourself any more - the core's `x104_ui_pass()` does (it also bumps `arm32_boot_progress` for the watchdog). Your loop calls `x104_ui_pass()` once per iteration.

## 2. Objects and every vtable slot
All objects are agile (`X104_CLASS_AGILE`), IInspectable-based, built with the toolkit. Unlisted slots stay holes (named E_NOTIMPL stubs).
### 2.1 ThreadPool statics - class `Windows.System.Threading.ThreadPool`, `IThreadPoolStatics {B6BF67DD-84BD-44F8-AC1C-93EBCB9DBA91}` (`X104_IID_IThreadPoolStatics`)
| slot | signature | behaviour | game caller |
|---|---|---|---|
| 6 `RunAsync` | `(void *self, void *handler, void **operation)` | start an action at Normal priority; the handler is an `IWorkItemHandler` (C++/CX delegate object, vptr 0xDB892C; `Invoke` = vtable slot 3 `(handler, IAsyncAction*)`) | init job: helper 0x1DA4A4, from `createApp` 0x1D9FA4 (lambda 0x1DA2D8) |
| 7 `RunWithPriorityAsync` | `(self, handler, int priority, void **operation)` | same with priority | not seen; implement anyway |
| 8 `RunWithPriorityAndOptionsAsync` | `(self, handler, int priority, int options, void **operation)` | priority High=1 (WorkItemPriority Low -1, Normal 0, High 1), options TimeSliced=1 | helper 0x114078: render loop (`StartRenderLoop` 0x1DA8A8), the MainPage-ctor input item (call 0x1DD40E, body 0x1DE0BC), `HID::Initialize` input item (call 0x111E68, body 0x110940) |
Behaviour of every Run*: create an `AsyncAction` (2.2); AddRef the handler; start a REAL thread (`CreateThread`, 2 MiB stack, `RoInitialize(RO_INIT_MULTITHREADED)` inside, ignore S_FALSE and RPC_E_CHANGED_MODE) and return the action AddRef'd in `*operation` BEFORE the thread runs `Invoke` (the game releases `operation` and the handler immediately after the call, `createApp` 0x1D9FAE/0x1D9FB8). The worker: `x104_call_guarded3(handler->vtbl[3], handler, action_iunknown_view, NULL, &hr, &seh)` (R5). The `IAsyncAction*` argument MUST be the real action (never NULL; the render loop dereferences it). After Invoke returns: if the action was not cancelled -> status Completed (1); if an exception escaped -> status Error (3), ErrorCode = E_FAIL, log `X104 async #n item THREW code=%08lx at rva ...`. Release the handler. Call the `Completed` handler if one was set (guarded; see 2.2). Thread priority: High -> `THREAD_PRIORITY_BELOW_NORMAL` (the render loop spins; the UI thread must stay responsive on a 2-core Surface RT), Normal -> NORMAL, Low -> LOWEST.
Item numbering: a global counter `#1, #2, ...`; log creation (`kind`, priority, options, handler, caller rva = `__builtin_return_address(0)` mapped with `x104_game_rva_of`), thread start, finish (status, hr, elapsed ms).
Expected items on a healthy run (log them clearly): #1 input source thread (MainPage ctor), #2 init job (createApp), #3 HID input source thread (first ready Tick), #4 render loop (first ready Tick). The two input items never return (see 2.3 ProcessEvents).

### 2.2 AsyncAction - runtime class `Windows.Foundation.IAsyncAction`, interfaces `IAsyncAction` (views[0]) and `IAsyncInfo {00000036-0000-0000-C000-000000000046}`
`IAsyncAction`: 6 `put_Completed(void *handler)` (store, AddRef; if already finished call it - **asynchronously through the queue, never inline (R1)**, same contract as the core's completed-action singleton `x104_done_put_completed`, which the review made invoke late handlers via `x104_ui_post` so that PPL/C++-CX `.then` continuations on host-returned actions run; here you may call it from the worker/UI top level, guarded, with `(info = this IAsyncAction view, status)`; the handler is `AsyncActionCompletedHandler`, Invoke slot 3 `(IAsyncAction *, AsyncStatus)`), 7 `get_Completed(void **handler)`, 8 `GetResults()` (S_OK).
`IAsyncInfo`: 6 `get_Id(UINT32*)` (the item number), **7 `get_Status(int*)`** , 8 `get_ErrorCode(HRESULT*)`, **9 `Cancel()`**, 10 `Close()`. Status semantics (R12): 0 Started until the worker returned (1 Completed / 3 Error) or `Cancel` (2 Canceled immediately and sticky). `Cancel` also signals the item's cancel event (wakes a blocked `ProcessEvents`), sets nothing else (the game's loop sees Status != 0 and returns by itself). `Close` after completion is a no-op. Thread-safe: `get_Status` is polled continuously by the render thread (use `__atomic_load_n`).
**Render-loop throttle (behavioural)**: the loop body 0x1DA160 polls `get_Status` in a tight loop (lock; if ready: update + Present; unlock) and spins even before the game is ready (no sleep!). Implement a per-thread poll counter in `get_Status`: if the same thread polls an unfinished action more than ~200 times within 10 ms AND `!x104_game_is_ready()`, `Sleep(1)`; once ready never sleep more than 1 ms per call and prefer `SwitchToThread()` every ~64 polls so the UI thread is never starved of its AppMain-lock hand-over. Never sleep when the poll is rare (normal short work items). Log once per item when the throttle first engages.

### 2.3 CoreDispatcher - runtime class `Windows.UI.Core.CoreDispatcher`, `ICoreDispatcher {60DB2FA8-B705-4FDE-A7D6-EBBB1891D39E}`
| slot | signature | behaviour | game caller |
|---|---|---|---|
| 6 `get_HasThreadAccess` | `(void *self, BYTE *value)` | `x104_on_ui_thread()` | - |
| 7 `ProcessEvents` | `(void *self, int options)` | `CoreProcessEventsOption`: ProcessOneAndAllPending 0, ProcessOneIfPresent 1, **ProcessUntilQuit 2**, ProcessAllIfPresent 3. On the UI thread: 0/1/3 run queued items once, 2 is an error (log, return S_OK). On a pool thread: option 2 BLOCKS until `x104_async_shutdown` or that item's Cancel (wait on an event with a 250 ms poll; the calling item's action is found with a thread-local set by the worker); options 0/1/3 return S_OK immediately | input items, rva 0x1DE0BC / 0x110940 (`ISwapChainPanel.CreateCoreIndependentInputSource(7)`, three pointer `add_`, then `get_Dispatcher().ProcessEvents(2)` on `ICoreInputSourceBase`) |
| 8 `RunAsync` | `(void *self, int priority, void *handler, void **asyncAction)` | **ALWAYS queue** (R1/R11), never inline, from any thread including the UI thread. `CoreDispatcherPriority`: Idle -2, Low -1, Normal 0, High 1; execute High first, else FIFO. AddRef the handler. Return an action (2.2 style, Started until the handler ran, then Completed) AddRef'd in `*asyncAction` (NULL is tolerated by DR but not by every caller: return the object). On the UI thread the handler `Invoke` = vtable slot 3 with NO arguments (`DispatchedHandler`), guarded, then Release | `DeviceResources::SetSwapChain` lambda 0x1DAF04 via IDependencyObject.Dispatcher (0x1DC3FC, helper 0x11401C) at the first SizeChanged: it QIs the panel for ISwapChainPanelNative and calls SetSwapChain; HID: `showKeyboard/hideKeyboard/updateText/cursor` lambdas 0x110BE8, 0x1108D4, 0x11079C, 0x110868, 0x112250 via `CoreWindow.Dispatcher` (these focus the TextBox / button -> the input unit's TabTip trigger) |
| 9 `RunIdleAsync` | `(self, handler, void **op)` | like RunAsync with Idle priority | - |
Items are consumed by `x104_ui_pump_once`. A handler that calls RunAsync again appends to the queue (processed in the same or the next pass). Cap one pass at 64 items so the message pump stays responsive; log if a handler takes > 500 ms (`X104 async: dispatched handler took %lu ms`).

### 2.4 DispatcherTimer factory + timer
Factory: class `Windows.UI.Xaml.DispatcherTimer`, `IDispatcherTimerFactory {E9961E6E-3626-403A-AFE0-040D58165632}` slot 6 `CreateInstance(void *self, void *outer, void **inner, void **timer)`: outer is NULL in the game (still handle it), `inner` may be NULL pointer or receive NULL, `*timer` = AddRef'd `IDispatcherTimer` view of a new timer (game call: helper 0x1DCE94 from the MainPage ctor, result stored at MainPage+0x84).
Timer: `IDispatcherTimer {D160CE46-CD22-4F5F-8C97-40E61DA3E2DC}`: 6 `get_Interval(LONGLONG*)`, **7 `put_Interval(LONGLONG)`** (TimeSpan = 100 ns ticks, 500000 = 50 ms; the 64-bit value arrives in r2:r3 - a `LONGLONG` parameter in C does that), 8 `get_IsEnabled(BYTE*)`, **9 `add_Tick(handler, LONGLONG *token)`** (`EventHandler<Object>`, Invoke slot 3 `(sender, args)`), 10 `remove_Tick(LONGLONG token)`, **11 `Start()`**, **12 `Stop()`**. The game's Tick handler (rva 0x1DF8A4) QIs the timer from `MainPage+0x84` for IDispatcherTimer and calls **slot 12 Stop** only after `AppMain+0x3C == 1`.
Behaviour: up to 8 timers in a static table. `Start`: enabled, `next_due = now + interval` (`GetTickCount`), set `x104.f_timer_started`, log. `Stop`: disabled; if the caller is the game's tick (`x104_game_is_ready()` is TRUE) -> `x104_flag_set(&x104.f_ready_tick, "READY tick: the game stopped its timer (AppMain+0x3C==1)")`. Ticks are delivered ONLY by the UI loop (never inside a fake), only while enabled, and **not before `x104.f_size_changed_done`** (R3); after the gate opens the first tick is due `interval` later. If the UI thread is late, do not burst: `next_due = now + interval`. Delivery: `x104_event_invoke(&timer->tick, sender = timer's IInspectable view, args = NULL)`; the handler may call Stop/Start/remove inside the call (state only). Log: created, interval, add_Tick, Start, Stop, first Tick delivered, then every 200th Tick.

## 3. Cross-unit
You CALL: `x104_game_*` accessors, the core's `x104_ui_pass()` once per loop iteration (it runs your `x104_ui_pump_once`, the three unit ticks and bumps `arm32_boot_progress`; nothing else of yours calls the ticks).
You are CALLED: everything in the contract table; `x104_ui_post` by input (TextChanged after put_Text), panel, app (exit); `x104_async_dispatcher_object` by the element mixin (`IDependencyObject.get_Dispatcher`), the CoreWindow and the input source.

## 4. The UI loop (`x104_ui_run_loop`)
```
log "UI loop entered (thread %04x)"
for (;;) {
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        if (m.message == X104_WM_QUIT) -> leave with (int)m.wParam;  if (m.message == WM_QUIT) -> leave with wParam
        if (m.message == X104_WM_WAKE) continue;
        TranslateMessage; DispatchMessageW;           /* window proc -> units (R1: they only update state / post) */
        if (x104.f_loop_exit) goto leave;             /* the window proc consumed an X104_WM_QUIT dispatched by a nested modal loop */
    }
    x104_ui_pass();                                   /* core: x104_ui_pump_once() (queue <= 64 items + due timers), then the panel/input/app ticks, progress bump */
    if (x104.f_loop_exit) goto leave;                 /* after EVERY pass (x104_ui_quit sets it from any thread) */
    MsgWaitForMultipleObjectsEx(1, &wake_event, wait_ms, X104_QS_ALLINPUT, 1 /* MWMO_INPUTAVAILABLE */);   /* wait_ms = min(next timer due - now, 10), at least 1 */
}
leave: log "UI loop left (code %d)"; return x104.exit_code;
```
**Modal loops (review finding):** when Windows runs a nested modal loop (window move/size by caption drag, the system menu) it dispatches posted messages to the window proc, not to your `PeekMessage`. Therefore (a) `X104_WM_QUIT` is ALSO handled by the boot's window proc (it stores the exit code, sets `f_loop_exit` and calls `PostQuitMessage`, which ends the modal loop and re-posts WM_QUIT to your loop) - you test `f_loop_exit` after every pass and after every dispatched message as shown, so a quit requested during a drag is never lost; (b) the 10 ms `X104_TIMER_PUMP` WM_TIMER runs `x104_ui_pass()` inside the modal loop, so queued items, timers, the panel's live-resize SizeChanged, the TabTip poll and the visibility bookkeeping continue (the panel's `in_size_move` branch is live); (c) Alt / F10 menu mode (SC_KEYMENU) is consumed by the boot, the key reaches the game.
Keep the wake event signalled by `x104_ui_post`/dispatcher RunAsync/`x104_ui_quit`. The loop runs on the host main thread INSIDE the game's `Application::Start` call (the app unit calls it); nothing may block it for long.

## 5. Ordering constraints and traps (facts)
* **Safe before init (HOST_DESIGN R15):** `x104_ui_post`, `x104_ui_quit`, `x104_ui_pump_once`, `x104_async_dispatcher_object` and `x104_async_shutdown` must behave on zero-initialised state before `x104_async_init` ran (post -> queue or FALSE, quit -> flags only, pump -> no-op, dispatcher -> NULL, shutdown -> no-op); the boot's `--selftest-fakes` and a failing bootstrap step can reach them early.
* `AppMain` critical section: your fakes are called from handlers that hold it (R1). `RunAsync`/`put_Interval`/`add_Tick`/`Start`/`Stop`/`Cancel` only touch your own state under a leaf lock.
* The init job runs seconds (76 D3DCompile calls + textures): it needs a REAL thread and must START before the first SizeChanged (`client+0xC` is written at its start) - start the thread immediately inside `RunAsync`, do not delay.
* Do NOT wait for the init job to finish, do not set `AppMain+0x3C`.
* The Tick gate uses `x104.f_size_changed_done`, set by the panel unit; if it never opens the log must say so: print `X104 async: timer tick gate closed (no SizeChanged yet)` once per 2 s while a started timer is waiting.
* Exceptions from the init job (e.g. `getShaderModelSuffix` "Unkonwn feature level" if the panel unit's patch is missing) must show up as `X104 async #2 item THREW` plus the already logged `_CxxThrowException` line.
* `facts`: `startup_flow` 1b.12/1b.15/1d/2a, `platform_services.threadpool.*` + verdicts M1-M3, `frame_render.frame.*`, `input_text` verdict `ProcessEvents`, `startup_flow.verdicts` m4/m7.

## 6. Required log lines (prefix `X104 async`)
`install ok`; `init ok (ui thread %04x, dispatcher %p)`; per work item: `item #%d created kind=%s prio=%d opts=%d handler=%p caller=rva %08lx`, `item #%d started on thread %04x`, `item #%d finished status=%d hr=%08lx after %lu ms`, `item #%d THREW ...`, `item #%d Cancel()`, `item #%d throttle engaged`; dispatcher: `RunAsync #%d prio=%d from thread %04x (ui=%d)` for the first 40, `handler took %lu ms`; timer: lines above; loop: `UI loop entered/left`; shutdown: `shutdown: %d items still running after 2000 ms: #...`.

## 7. Acceptance criteria
1. `try_0104.py --unit async` OK (0 errors, 0 warnings in 0104 files, link ok, x86 IDENTICAL).
2. `python tools/arm32/try_0104.py --selftest` and a device run with `--selftest-fakes` pass the async section (ThreadPool/IAsyncInfo status machine, RunAsync queues, 64-bit TimeSpan round trip). No `UNIMPLEMENTED` line for IThreadPoolStatics, IAsyncAction, IAsyncInfo, ICoreDispatcher, IDispatcherTimerFactory, IDispatcherTimer on a device run.
3. Device log: items #1/#2 start during OnLaunched, #2 finishes after the shader compiles, `first Tick delivered` only after the panel's first SizeChanged, `READY tick` follows, items #3/#4 start, render loop item #4 stays Started (heartbeat shows frames), CPU of the UI thread idle between events.
4. Closing the window: loop returns, `x104_async_shutdown` cancels #4, #1/#3 wake up, the process exits within the 30 s guard (preferably < 3 s).

## 8. Suggested structure
`x104_async_item` struct (base `X104Object` + id, kind, status, error, handler, completed handler, cancel event, thread handle/id, start tick); `x104_async_state` (lock, queue ring or linked list of {kind, fn/handler, arg, priority}, wake event, timers[8], items[32], quit event); `X104_SINGLETON` for the statics objects; keep thread-local "current item" in a TLS slot (`TlsAlloc` is not declared: use a small table keyed by `GetCurrentThreadId()` under the leaf lock).
