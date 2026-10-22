/* Host-side (x86, runs on the PC) logic test of win32_host_arm32_0104_async.inl: thread pool status machine, IAsyncInfo, Completed handlers, CoreDispatcher queue order
 * (R1/R11), ProcessEvents(ProcessUntilQuit), DispatcherTimer gating, UI loop and shutdown.  The REAL async unit is compiled in; the other four units come from the
 * pristine stubs so that the cross-unit prototypes are defined.  Build + run with:
 *     python tools/arm32/test_0104_core.py --src tools/arm32/test_0104_async.c
 * (x86 COM thunks are WINAPI, the mock delegates below are declared the same way.) */
#include "win32_compat.h"

extern void *WINAPI GetStdHandle(DWORD);
extern char *strstr(const char *, const char *);
extern int WINAPI wvsprintfA(LPSTR, LPCSTR, __builtin_va_list);
extern void WINAPI RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR *);

static volatile LONG g_failures;
static volatile LONG g_checks;
#define CHECK(cond) do { InterlockedIncrement(&g_checks); if (!(cond)) { InterlockedIncrement(&g_failures); test_print("CHECK FAILED line %d: %s\n", __LINE__, #cond); } } while (0)

static void test_print(const char *format, ...)
{
    char line[1100];
    DWORD written;
    __builtin_va_list arguments;
    __builtin_va_start(arguments, format);
    wvsprintfA(line, format, arguments);
    __builtin_va_end(arguments);
    WriteFile(GetStdHandle((DWORD)-11), line, (DWORD)strlen(line), &written, NULL);
}

/* ---- stand-ins for win32_host.c ---- */
static volatile LONG g_log_lines;
static volatile LONG g_throttle_logged;
static volatile LONG g_gate_logged;
static void log_line(const char *text)
{
    InterlockedIncrement(&g_log_lines);
    if (strstr(text, "throttle engaged")) InterlockedIncrement(&g_throttle_logged);
    if (strstr(text, "tick gate closed")) InterlockedIncrement(&g_gate_logged);
    test_print("  [log] %s\n", text);
}
static void log_hresult(const char *operation, HRESULT result) { test_print("  [hr] %s 0x%08lx\n", operation, (unsigned long)result); }
extern HRESULT WINAPI WindowsDeleteString(HSTRING);
extern BOOL WINAPI WindowsIsStringEmpty(HSTRING);
#define ARM32_SRC_MOUSEKB 1
#define ARM32_SRC_TOUCH 2
static BOOL arm32_check_bytes(const BYTE *address, const BYTE *expected, SIZE_T size, const char *what) { (void)what; return memcmp(address, expected, size) == 0; }
static void arm32_osk_show(const char *why) { (void)why; }
static void arm32_osk_hide(const char *why) { (void)why; }
static void arm32_osk_poll(void) { }
static BOOL arm32_osk_rect_buffer(float rect[4]) { (void)rect; return FALSE; }
static void arm32_osk_note_pointer_up(void) { }
static void arm32_input_note_source(int source, const char *why) { (void)source; (void)why; }
static BOOL arm32_input_touch_active(void) { return FALSE; }
static BOOL text_input_active;
static volatile LONG arm32_boot_frames_started, arm32_boot_frames_done, arm32_boot_progress, arm32_boot_loop_passes;
static DWORD arm32_boot_start_tick;
static void arm32_boot_note_frame(LONG frame, DWORD loop_start, DWORD frame_ms) { (void)frame; (void)loop_start; (void)frame_ms; }

#include "win32_host_arm32_0104_core.inl"
#include "win32_host_arm32_0104_async.inl"            /* THE REAL UNIT */
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_app.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_panel.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_platform.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_input.inl"

/* ---- a flexible mock COM delegate: vtbl = { QI, AddRef, Release, Invoke(self, a, b) } ---- */
typedef struct Mock {
    const void **vtbl;
    volatile LONG refs;
    volatile LONG invoked;
    void *a;
    void *b;
    volatile LONG thread;
    HANDLE gate;                 /* Invoke waits for it (keeps a pool item Started) */
    int mode;                    /* 0 normal, 1 raise a C++-like exception, 2 return E_FAIL-like failure, 3 call ProcessEvents(2) on the dispatcher */
    volatile LONG returned;      /* mode 3: ProcessEvents returned */
    int id;
    void (*hook)(struct Mock *);
} Mock;
static int g_order[512];
static volatile LONG g_order_count;
static void order_add(int id) { LONG n = InterlockedIncrement(&g_order_count) - 1; if (n < 512) g_order[n] = id; }

static HRESULT WINAPI mock_qi(void *self, const GUID *iid, void **out) { (void)iid; *out = self; InterlockedIncrement(&((Mock *)self)->refs); return S_OK; }
static ULONG WINAPI mock_addref(void *self) { return (ULONG)InterlockedIncrement(&((Mock *)self)->refs); }
static ULONG WINAPI mock_release(void *self) { return (ULONG)InterlockedDecrement(&((Mock *)self)->refs); }
static HRESULT WINAPI mock_invoke(void *self, void *a, void *b)
{
    Mock *m = (Mock *)self;
    m->a = a;
    m->b = b;
    m->thread = (LONG)GetCurrentThreadId();
    if (m->mode == 3) {
        X104Object *disp = x104_async_dispatcher_object();
        void *iface = NULL;
        InterlockedIncrement(&m->invoked);
        x104_object_qi(disp, &X104_IID_ICoreDispatcher, &iface);
        X104_VFN(iface, X104_SLOT_ICoreDispatcher_ProcessEvents, X104_FN_ICoreDispatcher_ProcessEvents)(iface, 2);
        InterlockedIncrement(&m->returned);
        return S_OK;
    }
    if (m->mode == 1) {
        InterlockedIncrement(&m->invoked);
        RaiseException(0xE06D7363u, 0, 0, NULL);
    }
    if (m->hook) m->hook(m);
    InterlockedIncrement(&m->invoked);
    if (m->gate) WaitForSingleObject(m->gate, 8000);
    return m->mode == 2 ? (HRESULT)0x80070057L : S_OK;
}
static const void *mock_vtbl[4] = { mock_qi, mock_addref, mock_release, mock_invoke };
static void mock_init(Mock *m) { memset(m, 0, sizeof(*m)); m->vtbl = mock_vtbl; m->refs = 1; }

static BOOL wait_for(volatile LONG *value, LONG at_least, DWORD timeout_ms, BOOL pump)
{
    DWORD start = GetTickCount();
    while (*value < at_least) {
        if (pump) x104_ui_pump_once();
        if (GetTickCount() - start > timeout_ms) return FALSE;
        Sleep(2);
    }
    return TRUE;
}

static int status_of(void *action)
{
    void *info = NULL;
    int status = -1;
    if (SUCCEEDED(x104_raw_qi(action, &X104_IID_IAsyncInfo, &info)) && info) {
        if (FAILED(X104_VFN(info, X104_SLOT_IAsyncInfo_get_Status, X104_FN_IAsyncInfo_get_Status)(info, &status))) status = -2;
        x104_raw_release(info);
    }
    return status;
}
static HRESULT error_of(void *action)
{
    void *info = NULL;
    HRESULT hr = 0x12345;
    if (SUCCEEDED(x104_raw_qi(action, &X104_IID_IAsyncInfo, &info)) && info) {
        X104_VFN(info, X104_SLOT_IAsyncInfo_get_ErrorCode, X104_FN_IAsyncInfo_get_ErrorCode)(info, &hr);
        x104_raw_release(info);
    }
    return hr;
}
static void cancel(void *action)
{
    void *info = NULL;
    if (SUCCEEDED(x104_raw_qi(action, &X104_IID_IAsyncInfo, &info)) && info) {
        X104_VFN(info, X104_SLOT_IAsyncInfo_Cancel, X104_FN_IAsyncInfo_Cancel)(info);
        x104_raw_release(info);
    }
}

static void *g_statics;
static void *g_dispatcher;
static HRESULT run_async(Mock *m, void **action) { return X104_VFN(g_statics, X104_SLOT_IThreadPoolStatics_RunAsync, X104_FN_IThreadPoolStatics_RunAsync)(g_statics, m, action); }
static HRESULT run_prio(Mock *m, int prio, int opts, void **action)
{
    return X104_VFN(g_statics, X104_SLOT_IThreadPoolStatics_RunWithPriorityAndOptionsAsync, X104_FN_IThreadPoolStatics_RunWithPriorityAndOptionsAsync)(g_statics, m, prio, opts, action);
}
static HRESULT disp_run(int prio, Mock *m, void **action)
{
    return X104_VFN(g_dispatcher, X104_SLOT_ICoreDispatcher_RunAsync, X104_FN_ICoreDispatcher_RunAsync)(g_dispatcher, prio, m, action);
}

static void post_fn(void *arg) { order_add((int)(INT_PTR)arg); }
static void post_crash(void *arg) { (void)arg; RaiseException(0xC0000005u, 0, 0, NULL); }
static void order_hook(Mock *m) { order_add(m->id); }

static void quit_thread_post(void *arg) { (void)arg; }
static volatile LONG g_loop_item_ran;
static void loop_item(void *arg) { (void)arg; InterlockedIncrement(&g_loop_item_ran); }
static DWORD WINAPI quit_thread(void *arg)
{
    Sleep(120);
    x104_ui_post(loop_item, NULL);
    Sleep(80);
    x104_ui_quit((int)(INT_PTR)arg);
    return 0;
}

static void tick_hook(Mock *m)
{
    (void)m;
}

int main(void)
{
    Mock work, blocker, h1, h2, h3, h4, completed, tick;
    void *action = NULL, *action2 = NULL, *timer_factory = NULL, *timer = NULL, *inner = NULL;
    HRESULT hr;
    int index;
    LONGLONG token = 0, interval = 0;
    BYTE flag = 0xFF;

    test_print("== async unit logic test ==\n");
    x104.image = (BYTE *)0x400000;
    x104.ui_thread_id = GetCurrentThreadId();
    x104.main_thread_id = x104.ui_thread_id;

    /* safe before init (R15): nothing crashes, post queues, dispatcher NULL */
    CHECK(x104_async_dispatcher_object() == NULL);
    x104_ui_pump_once();
    CHECK(x104_ui_post(post_fn, (void *)(INT_PTR)900));
    CHECK(x104_async_install(NULL));
    CHECK(x104_async_init(NULL));
    CHECK(x104_async_dispatcher_object() != NULL);
    x104.f_units_ready = 1;
    g_order_count = 0;
    x104_ui_pump_once();
    CHECK(g_order_count == 1 && g_order[0] == 900);          /* the pre-init post ran */
    g_order_count = 0;

    /* activation */
    CHECK(SUCCEEDED(x104_async_activate(L"Windows.System.Threading.ThreadPool", &X104_IID_IThreadPoolStatics, &g_statics)) && g_statics);
    CHECK(SUCCEEDED(x104_async_activate(L"Windows.UI.Xaml.DispatcherTimer", &X104_IID_IDispatcherTimerFactory, &timer_factory)) && timer_factory);
    CHECK(x104_async_activate(L"Windows.Foo", &X104_IID_IThreadPoolStatics, &inner) == REGDB_E_CLASSNOTREG);
    CHECK(x104_async_activate(L"Windows.System.Threading.ThreadPool", &X104_IID_IDispatcherTimer, &inner) == E_NOINTERFACE);
    CHECK(SUCCEEDED(x104_object_qi(x104_async_dispatcher_object(), &X104_IID_ICoreDispatcher, &g_dispatcher)) && g_dispatcher);
    {
        void *agile = NULL;
        CHECK(SUCCEEDED(x104_raw_qi(g_dispatcher, &X104_IID_IAgileObject, &agile)));
    }

    /* 1. ThreadPool.RunAsync: real action, item runs on ANOTHER thread with the action as argument, status 0 -> 1 */
    mock_init(&work);
    hr = run_async(&work, &action);
    CHECK(SUCCEEDED(hr) && action != NULL);
    CHECK(wait_for(&work.invoked, 1, 3000, FALSE));
    CHECK((DWORD)work.thread != GetCurrentThreadId() && work.a != NULL);
    {
        void *info = NULL;
        CHECK(SUCCEEDED(x104_raw_qi(work.a, &X104_IID_IAsyncInfo, &info)));                 /* the argument answers IAsyncInfo */
    }
    for (index = 0; index < 500 && status_of(action) == 0; ++index) Sleep(2);
    CHECK(status_of(action) == 1);
    CHECK(error_of(action) == S_OK);
    Sleep(30);
    CHECK(work.refs == 1);                                                                   /* the pool released its reference to the handler */

    /* 2. blocked High/TimeSliced item: Started, Cancel -> Canceled at once, sticky after the item returned */
    mock_init(&blocker);
    blocker.gate = CreateEventW(NULL, TRUE, FALSE, NULL);
    action = NULL;
    hr = run_prio(&blocker, 1, 1, &action);
    CHECK(SUCCEEDED(hr) && action);
    CHECK(wait_for(&blocker.invoked, 1, 3000, FALSE));
    CHECK(status_of(action) == 0);
    cancel(action);
    CHECK(status_of(action) == 2);
    SetEvent(blocker.gate);
    Sleep(150);
    CHECK(status_of(action) == 2);

    /* 3. Completed handler: attached while running -> fired with Completed on the finishing thread; attached late -> queued for the UI thread (R1) */
    mock_init(&blocker);
    blocker.gate = CreateEventW(NULL, TRUE, FALSE, NULL);
    mock_init(&completed);
    action = NULL;
    CHECK(SUCCEEDED(run_async(&blocker, &action)));
    CHECK(wait_for(&blocker.invoked, 1, 3000, FALSE));
    CHECK(SUCCEEDED(X104_VFN(action, X104_SLOT_IAsyncAction_put_Completed, X104_FN_IAsyncAction_put_Completed)(action, &completed)));
    {
        void *got = NULL;
        CHECK(SUCCEEDED(X104_VFN(action, X104_SLOT_IAsyncAction_get_Completed, X104_FN_IAsyncAction_get_Completed)(action, &got)) && got == &completed);
        x104_raw_release(got);
    }
    CHECK(completed.invoked == 0);
    SetEvent(blocker.gate);
    CHECK(wait_for(&completed.invoked, 1, 3000, FALSE));
    CHECK(completed.a == action && completed.b == (void *)1 && (DWORD)completed.thread != GetCurrentThreadId());
    mock_init(&h1);
    CHECK(SUCCEEDED(X104_VFN(action, X104_SLOT_IAsyncAction_put_Completed, X104_FN_IAsyncAction_put_Completed)(action, &h1)));
    CHECK(h1.invoked == 0);                                                                  /* NOT inline */
    x104_ui_pump_once();
    CHECK(h1.invoked == 1 && h1.b == (void *)1 && (DWORD)h1.thread == GetCurrentThreadId());

    /* 4. exception in a work item -> Error/E_FAIL, process survives; failing HRESULT -> Error with that code */
    mock_init(&h2);
    h2.mode = 1;
    action = NULL;
    CHECK(SUCCEEDED(run_async(&h2, &action)));
    for (index = 0; index < 500 && status_of(action) == 0; ++index) Sleep(2);
    CHECK(status_of(action) == 3 && error_of(action) == E_FAIL);
    mock_init(&h3);
    h3.mode = 2;
    action = NULL;
    CHECK(SUCCEEDED(run_async(&h3, &action)));
    for (index = 0; index < 500 && status_of(action) == 0; ++index) Sleep(2);
    CHECK(status_of(action) == 3 && error_of(action) == (HRESULT)0x80070057L);
    CHECK(X104_VFN(action, X104_SLOT_IAsyncAction_GetResults, X104_FN_IAsyncAction_GetResults)(action) == (HRESULT)0x80070057L);

    /* bad arguments */
    CHECK(run_async(NULL, &action) == E_POINTER && action == NULL);
    CHECK(X104_VFN(g_statics, X104_SLOT_IThreadPoolStatics_RunAsync, X104_FN_IThreadPoolStatics_RunAsync)(g_statics, &work, NULL) == E_POINTER);

    /* 5. CoreDispatcher: HasThreadAccess, RunAsync always queues, strict priority High > Normal > Low > Idle, FIFO inside, shared with x104_ui_post */
    CHECK(SUCCEEDED(X104_VFN(g_dispatcher, X104_SLOT_ICoreDispatcher_get_HasThreadAccess, X104_FN_ICoreDispatcher_get_HasThreadAccess)(g_dispatcher, &flag)) && flag == 1);
    mock_init(&h1); mock_init(&h2); mock_init(&h3); mock_init(&h4);
    h1.id = 2; h2.id = 3; h3.id = 5; h4.id = 6;
    h1.hook = h2.hook = h3.hook = h4.hook = order_hook;
    g_order_count = 0;
    CHECK(x104_ui_post(post_fn, (void *)(INT_PTR)1));
    action = NULL;
    CHECK(SUCCEEDED(disp_run(1, &h1, &action)) && action);
    CHECK(SUCCEEDED(disp_run(0, &h2, &action2)) && action2);
    CHECK(x104_ui_post(post_fn, (void *)(INT_PTR)4));
    CHECK(SUCCEEDED(disp_run(-1, &h3, NULL)) == FALSE);                                      /* NULL out pointer -> E_POINTER */
    {
        void *a3 = NULL;
        CHECK(SUCCEEDED(disp_run(-1, &h3, &a3)));
        CHECK(SUCCEEDED(X104_VFN(g_dispatcher, X104_SLOT_ICoreDispatcher_RunIdleAsync, X104_FN_ICoreDispatcher_RunIdleAsync)(g_dispatcher, &h4, &inner)));
    }
    CHECK(h1.invoked == 0 && h2.invoked == 0 && g_order_count == 0);                         /* R1: nothing ran inside the calls */
    CHECK(status_of(action) == 0 && status_of(action2) == 0);
    x104_ui_pump_once();
    CHECK(g_order_count == 6 && g_order[0] == 2 && g_order[1] == 1 && g_order[2] == 3 && g_order[3] == 4 && g_order[4] == 5 && g_order[5] == 6);
    CHECK(status_of(action) == 1 && status_of(action2) == 1);
    CHECK((DWORD)h1.thread == GetCurrentThreadId() && h1.a == NULL);
    CHECK(h1.refs == 1);

    /* 5b. RunAsync from another thread, a cancelled action is skipped, a crashing queue item does not stop the pass */
    mock_init(&h1); mock_init(&h2);
    CHECK(SUCCEEDED(disp_run(0, &h1, &action)));
    cancel(action);
    CHECK(status_of(action) == 2);
    CHECK(x104_ui_post(post_crash, NULL));
    g_order_count = 0;
    CHECK(x104_ui_post(post_fn, (void *)(INT_PTR)77));
    x104_ui_pump_once();
    CHECK(h1.invoked == 0 && g_order_count == 1 && g_order[0] == 77 && status_of(action) == 2);
    h2.mode = 1;
    CHECK(SUCCEEDED(disp_run(0, &h2, &action)));
    x104_ui_pump_once();
    CHECK(status_of(action) == 3);

    /* 5c. per-pass cap */
    g_order_count = 0;
    for (index = 0; index < 100; ++index) CHECK(x104_ui_post(post_fn, (void *)(INT_PTR)index));
    x104_ui_pump_once();
    CHECK(g_order_count == X104_ASYNC_PUMP_CAP);
    x104_ui_pump_once();
    CHECK(g_order_count == 100);
    for (index = 0; index < 100; ++index) CHECK(g_order[index] == index);                   /* FIFO */

    /* 5d. ProcessEvents on the UI thread: 2 refused (returns), 0/1/3 are logged no-ops too: R1 (integration review) - a fake called by the game never runs queued
     *     handlers; only the UI pass does */
    g_order_count = 0;
    CHECK(x104_ui_post(post_fn, (void *)(INT_PTR)55));
    CHECK(X104_VFN(g_dispatcher, X104_SLOT_ICoreDispatcher_ProcessEvents, X104_FN_ICoreDispatcher_ProcessEvents)(g_dispatcher, 2) == S_OK);
    CHECK(g_order_count == 0);
    CHECK(X104_VFN(g_dispatcher, X104_SLOT_ICoreDispatcher_ProcessEvents, X104_FN_ICoreDispatcher_ProcessEvents)(g_dispatcher, 1) == S_OK);
    CHECK(g_order_count == 0);
    x104_ui_pump_once();
    CHECK(g_order_count == 1 && g_order[0] == 55);

    /* 6. ProcessEvents(ProcessUntilQuit) on a pool thread blocks until that item's Cancel */
    mock_init(&h1);
    h1.mode = 3;
    action = NULL;
    CHECK(SUCCEEDED(run_prio(&h1, 1, 1, &action)));
    CHECK(wait_for(&h1.invoked, 1, 3000, FALSE));
    Sleep(400);
    CHECK(h1.returned == 0 && status_of(action) == 0);
    cancel(action);
    CHECK(wait_for(&h1.returned, 1, 2000, FALSE));
    CHECK(status_of(action) == 2);
    /* a second blocked item is released by x104_async_shutdown (later) */
    mock_init(&h4);
    h4.mode = 3;
    action2 = NULL;
    CHECK(SUCCEEDED(run_prio(&h4, 1, 1, &action2)));
    CHECK(wait_for(&h4.invoked, 1, 3000, FALSE));

    /* 7. render-loop throttle: a thread that polls an unfinished action thousands of times while the game is not ready gets Sleep(1) */
    mock_init(&blocker);
    blocker.gate = CreateEventW(NULL, TRUE, FALSE, NULL);
    action = NULL;
    CHECK(SUCCEEDED(run_prio(&blocker, 1, 1, &action)));
    CHECK(wait_for(&blocker.invoked, 1, 3000, FALSE));
    {
        DWORD t0 = GetTickCount();
        int n;
        for (n = 0; n < 20000; ++n) status_of(action);
        CHECK(GetTickCount() - t0 >= 50);                                                    /* ~100 sleeps of >= 1 ms */
        CHECK(g_throttle_logged == 1);
    }
    SetEvent(blocker.gate);

    /* 8. DispatcherTimer: factory with NULL outer, 64-bit interval, token by value, gate, no burst */
    hr = X104_VFN(timer_factory, X104_SLOT_IDispatcherTimerFactory_CreateInstance, X104_FN_IDispatcherTimerFactory_CreateInstance)(timer_factory, NULL, NULL, &timer);
    CHECK(SUCCEEDED(hr) && timer);
    CHECK(SUCCEEDED(X104_VFN(timer, X104_SLOT_IDispatcherTimer_put_Interval, X104_FN_IDispatcherTimer_put_Interval)(timer, 300000LL)));
    CHECK(SUCCEEDED(X104_VFN(timer, X104_SLOT_IDispatcherTimer_get_Interval, X104_FN_IDispatcherTimer_get_Interval)(timer, &interval)) && interval == 300000LL);
    mock_init(&tick);
    tick.hook = tick_hook;
    CHECK(SUCCEEDED(X104_VFN(timer, X104_SLOT_IDispatcherTimer_add_Tick, X104_FN_IDispatcherTimer_add_Tick)(timer, &tick, &token)) && token != 0);
    CHECK(SUCCEEDED(X104_VFN(timer, X104_SLOT_IDispatcherTimer_get_IsEnabled, X104_FN_IDispatcherTimer_get_IsEnabled)(timer, &flag)) && flag == 0);
    CHECK(SUCCEEDED(X104_VFN(timer, X104_SLOT_IDispatcherTimer_Start, X104_FN_IDispatcherTimer_Start)(timer)));
    CHECK(x104.f_timer_started == 1);
    CHECK(SUCCEEDED(X104_VFN(timer, X104_SLOT_IDispatcherTimer_get_IsEnabled, X104_FN_IDispatcherTimer_get_IsEnabled)(timer, &flag)) && flag == 1);
    Sleep(120);
    x104_ui_pump_once();
    CHECK(tick.invoked == 0);                                                                /* gate closed (no SizeChanged yet) */
    CHECK(g_gate_logged == 1);
    x104.f_size_changed_done = 1;
    x104_ui_pump_once();
    CHECK(tick.invoked == 0);                                                                /* the first tick is due one interval after the gate opened */
    Sleep(70);
    x104_ui_pump_once();
    CHECK(tick.invoked == 1 && tick.a == timer && tick.b == NULL && (DWORD)tick.thread == GetCurrentThreadId());
    Sleep(300);                                                                              /* ~10 intervals late */
    x104_ui_pump_once();
    CHECK(tick.invoked == 2);                                                                /* never a burst */
    x104_ui_pump_once();
    CHECK(tick.invoked == 2);
    CHECK(SUCCEEDED(X104_VFN(timer, X104_SLOT_IDispatcherTimer_Stop, X104_FN_IDispatcherTimer_Stop)(timer)));
    CHECK(x104.f_ready_tick == 0);                                                           /* game not ready: not a READY tick */
    Sleep(100);
    x104_ui_pump_once();
    CHECK(tick.invoked == 2);
    CHECK(SUCCEEDED(X104_VFN(timer, X104_SLOT_IDispatcherTimer_remove_Tick, X104_FN_IDispatcherTimer_remove_Tick)(timer, token)) && tick.refs == 1);

    /* 9. the UI loop: runs posted items from another thread, returns the exit code of x104_ui_quit, post refuses afterwards */
    {
        HANDLE thread = CreateThread(NULL, 0, quit_thread, (void *)(INT_PTR)7, 0, NULL);
        int code;
        DWORD t0 = GetTickCount();
        CHECK(thread != NULL);
        code = x104_ui_run_loop();
        CHECK(code == 7);
        CHECK(g_loop_item_ran == 1);
        CHECK(GetTickCount() - t0 < 2000);
        CHECK(x104.f_quit == 1 && x104.f_loop_exit == 1);
        CHECK(x104_ui_post(post_fn, NULL) == FALSE);
        WaitForSingleObject(thread, 2000);
        CloseHandle(thread);
    }

    /* 10. shutdown: cancels the blocked ProcessEvents item, drops queued work, idempotent, waits for the workers */
    {
        DWORD t0 = GetTickCount();
        x104_async_shutdown();
        CHECK(GetTickCount() - t0 < 2500);
        CHECK(wait_for(&h4.returned, 1, 2000, FALSE));
        CHECK(status_of(action2) == 2);
        CHECK(x104_async_st.workers_running == 0);
        x104_async_shutdown();                                                               /* idempotent */
        mock_init(&h1);
        CHECK(SUCCEEDED(run_async(&h1, &action)));                                           /* after shutdown: Canceled, never started */
        CHECK(status_of(action) == 2);
        Sleep(50);
        CHECK(h1.invoked == 0);
        CHECK(SUCCEEDED(disp_run(0, &h1, &action)) && status_of(action) == 2);
    }

    test_print("checks %ld, failures %ld, log lines %ld\n", (long)g_checks, (long)g_failures, (long)g_log_lines);
    test_print(g_failures ? "== async unit test FAILED ==\n" : "== async unit test passed ==\n");
    return g_failures ? 1 : 0;
}
