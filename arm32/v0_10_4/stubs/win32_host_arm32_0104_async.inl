/* win32_host_arm32_0104_async.inl - UNIT "async" of the Minecraft PE 0.10.4.40 ARM32 host.            *** STUB - TODO(async) ***
 *
 * Owner: the async implementer.  Brief: arm32/v0_10_4/units/async.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit async
 *
 * This file owns: ThreadPool statics + IAsyncAction/IAsyncInfo, the CoreDispatcher, DispatcherTimer, and the UI thread primitives
 * (x104_ui_post, x104_ui_run_loop, x104_ui_quit, x104_ui_pump_once).  The bodies below are placeholders that only keep the tree
 * compiling; replace them (keep every signature and the file's public function names). */
#ifndef WIN32_HOST_ARM32_0104_ASYNC_INL
#define WIN32_HOST_ARM32_0104_ASYNC_INL

/* TODO(async): nothing to patch in the game image; return TRUE. */
static BOOL x104_async_install(BYTE *image)
{
    (void)image;
    return TRUE;
}

/* TODO(async): UI thread, window exists.  Create the dispatcher object, the UI queue + wake event, SetTimer(window, X104_TIMER_PUMP, 10, NULL) (the boot's window proc
 * answers its WM_TIMER with x104_ui_pass(), which keeps the queue, the timers and the unit ticks alive inside the modal loops of window move/size and menus). */
static BOOL x104_async_init(HWND window)
{
    SetTimer(window, X104_TIMER_PUMP, 10, NULL);
    return TRUE;
}

/* TODO(async): cancel running actions (status Canceled), wake the ProcessEvents(ProcessUntilQuit) thread, wait <= 2 s for workers. */
static void x104_async_shutdown(void)
{
}

/* TODO(async): classes Windows.System.Threading.ThreadPool (IThreadPoolStatics) and Windows.UI.Xaml.DispatcherTimer (IDispatcherTimerFactory);
 * use x104_activate_from_table().  REGDB_E_CLASSNOTREG = "not implemented yet" (the core falls back to the real runtime and logs it). */
static HRESULT x104_async_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    (void)class_name;
    (void)iid;
    (void)out;
    return REGDB_E_CLASSNOTREG;
}

/* TODO(async): the one CoreDispatcher (ICoreDispatcher + IAgileObject) of the UI thread. */
static X104Object *x104_async_dispatcher_object(void)
{
    return NULL;
}

/* TODO(async): the real loop (messages + FIFO queue + DispatcherTimer + 10 ms idle hooks).  This placeholder only pumps messages so that a
 * window can open and close.  The final loop: drain the messages (leave on X104_WM_QUIT / WM_QUIT), call x104_ui_pass() (queue, timers and the three unit
 * ticks, shared with the WM_TIMER pump), then LEAVE if x104.f_loop_exit is set (x104_ui_quit sets it from any thread; the window proc sets it when a nested
 * modal loop dispatched X104_WM_QUIT), then wait <= 10 ms. */
static int x104_ui_run_loop(void)
{
    MSG message;
    for (;;) {
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == X104_WM_QUIT) return (int)message.wParam;
            if (message.message == WM_QUIT) return (int)message.wParam;
            if (message.message == X104_WM_WAKE) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        x104_ui_pass();
        if (x104.f_loop_exit) return x104.exit_code;
        MsgWaitForMultipleObjectsEx(0, NULL, 10, X104_QS_ALLINPUT, 0);
    }
}

/* TODO(async): any thread; make x104_ui_run_loop return exit_code: x104.exit_code, f_quit (x104_ui_post refuses from now on), f_loop_exit, wake event, X104_WM_QUIT. */
static void x104_ui_quit(int exit_code)
{
    x104.exit_code = exit_code;
    x104_flag_set(&x104.f_quit, "f_quit: x104_ui_quit - the UI loop is leaving");
    InterlockedExchange(&x104.f_loop_exit, 1);
    if (x104.window) PostMessageW(x104.window, X104_WM_QUIT, (WPARAM)exit_code, 0);
}

/* TODO(async): any thread; FIFO queue consumed on the UI thread; wake the loop with X104_WM_WAKE.  Refuses (FALSE) ONLY after x104_ui_quit (x104.f_quit). */
static BOOL x104_ui_post(void (*function)(void *), void *argument)
{
    (void)function;
    (void)argument;
    return FALSE;
}

/* TODO(async): UI thread; run queued work and due timers once; must be re-entrancy safe (only x104_ui_pass() calls it: the loop and the WM_TIMER pump of modal loops). */
static void x104_ui_pump_once(void)
{
}

#endif /* WIN32_HOST_ARM32_0104_ASYNC_INL */
