/* win32_host_arm32_0104_selftest.inl - on-device self test of the host fakes (command line `--selftest-fakes`, `--selftest-fakes=only`).   OWNER: designer.
 *
 * WHY: the x86 host-side tests (tools/arm32/test_0104_core.py) cannot check ARM register passing (LONGLONG in r2:r3, double in d0, float, struct pointers) of the
 * fakes, so without this the FIRST ABI test would be the real game on the Surface 2.  With `--selftest-fakes` the bootstrap runs this file after the units are
 * initialised and BEFORE the game's WinMain: it activates every fake class through the same router the game uses (x104_activation_override), calls the slots
 * the game uses THROUGH THE GENERATED TYPEDEFS (X104_FN_<Interface>_<Method>, i.e. exactly the C signature of units/SLOTS.md, compiled by the same ARM compiler the
 * game's C++/CX code is not, but with the same AAPCS rules) and checks the values that come back.  `=only` stops after the test (exit code 40 on failures).
 *
 * Every check is logged as `X104 selftest: [PASS|FAIL|SKIP] <name> <detail>`; a class that the unit does not implement yet (REGDB_E_CLASSNOTREG) is a SKIP, any
 * other activation failure is a FAIL.  The test has side effects only on host objects: a few thread-pool items (the numbering of "item #n" shifts), one timer slot,
 * one scheduled TextChanged; it never calls into the game and never starts the timer or the keyboard.
 *
 * Contract used from the units (all documented in the briefs): ThreadPool + IAsyncInfo, CoreDispatcher.RunAsync queues (R1), DispatcherTimer 64-bit TimeSpan,
 * completed actions invoke late Completed handlers, DisplayInformation floats, Window/CoreWindow/IAgileObject, panel double/float/Size getters, TextBox text round
 * trip, Package/ApplicationData paths, MemoryManager 64-bit outputs, Vibration TimeSpan, network profile level.
 *
 * Included by win32_host_arm32_0104.inl right before the boot (it uses only the core, the iids and the unit contract functions). */
#ifndef WIN32_HOST_ARM32_0104_SELFTEST_INL
#define WIN32_HOST_ARM32_0104_SELFTEST_INL

static int x104_st_pass, x104_st_fail, x104_st_skip;

static void x104_st_result(const char *name, int outcome, const char *detail)
{
    if (outcome > 0) ++x104_st_pass;
    else if (outcome < 0) ++x104_st_fail;
    else ++x104_st_skip;
    x104_logf("X104 selftest: [%s] %s%s%s", outcome > 0 ? "PASS" : (outcome < 0 ? "FAIL" : "SKIP"), name, detail && detail[0] ? " - " : "", detail ? detail : "");
}
#define X104_ST_CHECK(name, condition, detail) x104_st_result((name), (condition) ? 1 : -1, (detail))

/* a mock C++/CX delegate: vtbl = { QI, AddRef, Release, Invoke }; Invoke(self, a, b) records its arguments (extra arguments are harmless on ARM; a delegate with fewer
 * is called with fewer: the callee ignores them), optionally blocks on `gate` (to keep a pool-thread item in the Started state) */
typedef struct X104StMock {
    const void **vtbl;
    volatile LONG refs;
    volatile LONG invoked;
    void *a;
    void *b;
    HANDLE gate;
    volatile LONG thread;
} X104StMock;

static HRESULT X104_API x104_st_mock_qi(void *self, const GUID *iid, void **out) { (void)iid; if (!out) return E_POINTER; *out = self; InterlockedIncrement(&((X104StMock *)self)->refs); return S_OK; }
static ULONG X104_API x104_st_mock_addref(void *self) { return (ULONG)InterlockedIncrement(&((X104StMock *)self)->refs); }
static ULONG X104_API x104_st_mock_release(void *self) { return (ULONG)InterlockedDecrement(&((X104StMock *)self)->refs); }
static HRESULT X104_API x104_st_mock_invoke(void *self, void *a, void *b)
{
    X104StMock *mock = (X104StMock *)self;
    mock->a = a;
    mock->b = b;
    mock->thread = (LONG)GetCurrentThreadId();
    InterlockedIncrement(&mock->invoked);
    if (mock->gate) WaitForSingleObject(mock->gate, 8000);
    return S_OK;
}
static const void *x104_st_mock_vtbl[4] = { x104_st_mock_qi, x104_st_mock_addref, x104_st_mock_release, x104_st_mock_invoke };

static void x104_st_mock_init(X104StMock *mock)
{
    memset(mock, 0, sizeof(*mock));
    mock->vtbl = x104_st_mock_vtbl;
    mock->refs = 1;
}

/* wait until mock->invoked >= count (pumping the UI queue when `pump`), at most timeout_ms */
static BOOL x104_st_wait_invoked(X104StMock *mock, LONG count, DWORD timeout_ms, BOOL pump)
{
    DWORD start = GetTickCount();
    while (mock->invoked < count) {
        if (pump) x104_ui_pump_once();
        if (GetTickCount() - start > timeout_ms) return FALSE;
        Sleep(5);
    }
    return TRUE;
}

#define X104_ST_FN(object, iface, method) X104_VFN((object), X104_SLOT_##iface##_##method, X104_FN_##iface##_##method)

/* activate a fake class like the game does; FALSE (+ SKIP or FAIL result) when no usable factory */
static BOOL x104_st_activate(const wchar_t *class_name, const GUID *iid, void **out, const char *label)
{
    HRESULT hr;
    char text[160];
    *out = NULL;
    hr = x104_activation_override(class_name, iid, out);
    if (SUCCEEDED(hr) && *out) return TRUE;
    wsprintfA(text, "activation hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
    x104_st_result(label, hr == REGDB_E_CLASSNOTREG ? 0 : -1, hr == REGDB_E_CLASSNOTREG ? "class not implemented by its unit yet" : text);
    return FALSE;
}

static int x104_st_status(void *async_object)
{
    void *info = NULL;
    int status = -1;
    if (SUCCEEDED(x104_raw_qi(async_object, &X104_IID_IAsyncInfo, &info)) && info) {
        if (FAILED(X104_ST_FN(info, IAsyncInfo, get_Status)(info, &status))) status = -2;
    }
    return status;
}

/* ---- core services (always run) ---- */
static void x104_st_core(void)
{
    HSTRING h = NULL;
    wchar_t buffer[40];
    X104StMock mock, outer;
    X104Event event;
    LONGLONG token = 0;
    void *inner = NULL, *instance = NULL;
    X104Object *object = NULL;
    HRESULT hr;

    hr = x104_hstring_create(L"ms-appx:///MainPage.xaml", &h);
    X104_ST_CHECK("core: HSTRING round trip", SUCCEEDED(hr) && x104_hstring_equals(h, L"ms-appx:///MainPage.xaml") && x104_hstring_wide(h, buffer, 40) == 24 && buffer[23] == L'l', "");
    x104_hstring_free(h);

    x104_st_mock_init(&mock);
    x104_event_init(&event, "selftest");
    hr = x104_event_add(&event, &mock, &token);
    X104_ST_CHECK("core: event add returns a 64-bit token", SUCCEEDED(hr) && token != 0 && (token >> 32) == 0x104 && mock.refs == 2 && x104_event_count(&event) == 1, "");
    x104_event_invoke(&event, (void *)0x1111, (void *)0x2222);
    X104_ST_CHECK("core: event invoke passes sender and args", mock.invoked == 1 && mock.a == (void *)0x1111 && mock.b == (void *)0x2222, "");
    hr = x104_event_remove(&event, token);
    x104_event_invoke(&event, NULL, NULL);
    X104_ST_CHECK("core: event remove takes the token BY VALUE", SUCCEEDED(hr) && mock.invoked == 1 && mock.refs == 1 && x104_event_count(&event) == 0, "");

    x104_st_mock_init(&outer);
    hr = x104_aggregate_create(&x104_done_class, sizeof(X104Object), &outer, &X104_IID_IAsyncInfo, &inner, &instance, &object, "selftest");
    X104_ST_CHECK("core: x104_aggregate_create (inner = views[0], instance = requested view, outer AddRef'd once)",
                  SUCCEEDED(hr) && object && inner == (void *)&object->views[0] && instance == (void *)&object->views[1] && outer.refs == 2 && object->outer == &outer, "");
    if (instance) {
        int status = -1;
        X104_ST_CHECK("core: aggregated object answers through its IAsyncInfo view", SUCCEEDED(X104_ST_FN(instance, IAsyncInfo, get_Status)(instance, &status)) && status == 1, "");
    }

    {
        void *done = x104_object_iface0(x104_completed_action());
        void *dummy = NULL;
        X104StMock handler;
        x104_st_mock_init(&handler);
        X104_ST_CHECK("core: completed action is IAsyncAction + IAsyncInfo(Completed) + IAgileObject",
                      x104_st_status(done) == 1 && SUCCEEDED(x104_raw_qi(done, &X104_IID_IAgileObject, &dummy)), "");
        hr = X104_ST_FN(done, IAsyncAction, put_Completed)(done, &handler);
        if (!x104_async_dispatcher_object()) {
            x104_st_result("core: completed action invokes a late Completed handler", 0, "async unit not implemented (no UI queue)");
        } else {
            BOOL got = x104_st_wait_invoked(&handler, 1, 2000, TRUE);
            X104_ST_CHECK("core: completed action invokes a late Completed handler (asynchronously, with status Completed)",
                          SUCCEEDED(hr) && got && handler.b == (void *)(ULONG_PTR)X104_ASYNC_COMPLETED && handler.a == done, "");
        }
    }
}

/* ---- async unit ---- */
static void x104_st_async(void)
{
    void *statics = NULL, *action = NULL, *timer_factory = NULL, *timer = NULL, *inner = NULL, *dispatcher = NULL;
    X104StMock work, blocker, queued, tick;
    HRESULT hr;
    LONGLONG interval = 0, token = 0;
    BYTE flag = 0xFF;

    if (x104_st_activate(L"Windows.System.Threading.ThreadPool", &X104_IID_IThreadPoolStatics, &statics, "async: ThreadPool activation")) {
        x104_st_mock_init(&work);
        hr = X104_ST_FN(statics, IThreadPoolStatics, RunAsync)(statics, &work, &action);
        X104_ST_CHECK("async: ThreadPool.RunAsync returns a real IAsyncAction before/while the item runs", SUCCEEDED(hr) && action != NULL, "");
        if (action) {
            BOOL ran = x104_st_wait_invoked(&work, 1, 3000, FALSE);
            int status = -1;
            DWORD start = GetTickCount();
            X104_ST_CHECK("async: the work item ran on a pool thread with the action as argument",
                          ran && (DWORD)work.thread != GetCurrentThreadId() && work.a != NULL, "");
            while ((status = x104_st_status(action)) == 0 && GetTickCount() - start < 3000) Sleep(5);
            X104_ST_CHECK("async: IAsyncInfo.get_Status becomes Completed (1) after the item returned", status == 1, "");
        }
        x104_st_mock_init(&blocker);
        blocker.gate = CreateEventW(NULL, TRUE, FALSE, NULL);
        action = NULL;
        hr = X104_ST_FN(statics, IThreadPoolStatics, RunWithPriorityAndOptionsAsync)(statics, &blocker, 1 /* High */, 1 /* TimeSliced */, &action);
        if (SUCCEEDED(hr) && action) {
            void *info = NULL;
            int before, after, later;
            x104_st_wait_invoked(&blocker, 1, 3000, FALSE);
            before = x104_st_status(action);
            if (SUCCEEDED(x104_raw_qi(action, &X104_IID_IAsyncInfo, &info)) && info) X104_ST_FN(info, IAsyncInfo, Cancel)(info);
            after = x104_st_status(action);
            SetEvent(blocker.gate);
            Sleep(100);
            later = x104_st_status(action);
            X104_ST_CHECK("async: a running item reports Started (0); Cancel makes the status Canceled (2) at once and it stays 2 after the item returned",
                          blocker.invoked == 1 && before == 0 && after == 2 && later == 2, "");
        } else {
            X104_ST_CHECK("async: RunWithPriorityAndOptionsAsync (priority, options, out in the right registers)", FALSE, "call failed");
        }
    }

    {
        X104Object *object = x104_async_dispatcher_object();
        if (!object) {
            x104_st_result("async: CoreDispatcher", 0, "async unit not implemented");
        } else if (SUCCEEDED(x104_object_qi(object, &X104_IID_ICoreDispatcher, &dispatcher)) && dispatcher) {
            void *op = NULL;
            int status;
            hr = X104_ST_FN(dispatcher, ICoreDispatcher, get_HasThreadAccess)(dispatcher, &flag);
            X104_ST_CHECK("async: CoreDispatcher.HasThreadAccess on the UI thread", SUCCEEDED(hr) && flag == 1, "");
            x104_st_mock_init(&queued);
            hr = X104_ST_FN(dispatcher, ICoreDispatcher, RunAsync)(dispatcher, 0 /* Normal */, &queued, &op);
            X104_ST_CHECK("async: RunAsync ALWAYS queues (rule R1: the handler did not run inside the call)", SUCCEEDED(hr) && queued.invoked == 0, "");
            x104_ui_pump_once();
            status = op ? x104_st_status(op) : -9;
            X104_ST_CHECK("async: the queued handler runs on the UI thread in x104_ui_pump_once and its action completes",
                          queued.invoked == 1 && (DWORD)queued.thread == GetCurrentThreadId() && status == 1, "");
        } else {
            x104_st_result("async: CoreDispatcher", -1, "QI(ICoreDispatcher) failed");
        }
    }

    if (x104_st_activate(L"Windows.UI.Xaml.DispatcherTimer", &X104_IID_IDispatcherTimerFactory, &timer_factory, "async: DispatcherTimer activation")) {
        hr = X104_ST_FN(timer_factory, IDispatcherTimerFactory, CreateInstance)(timer_factory, NULL, &inner, &timer);
        X104_ST_CHECK("async: DispatcherTimerFactory.CreateInstance(NULL outer)", SUCCEEDED(hr) && timer != NULL, "");
        if (timer) {
            hr = X104_ST_FN(timer, IDispatcherTimer, put_Interval)(timer, 500000LL);
            X104_ST_CHECK("async: put_Interval(500000) + get_Interval round trip (TimeSpan in r2:r3)",
                          SUCCEEDED(hr) && SUCCEEDED(X104_ST_FN(timer, IDispatcherTimer, get_Interval)(timer, &interval)) && interval == 500000LL, "");
            x104_st_mock_init(&tick);
            hr = X104_ST_FN(timer, IDispatcherTimer, add_Tick)(timer, &tick, &token);
            X104_ST_CHECK("async: add_Tick returns a token, the timer is not enabled", SUCCEEDED(hr) && token != 0 &&
                          SUCCEEDED(X104_ST_FN(timer, IDispatcherTimer, get_IsEnabled)(timer, &flag)) && flag == 0, "");
            hr = X104_ST_FN(timer, IDispatcherTimer, remove_Tick)(timer, token);
            X104_ST_CHECK("async: remove_Tick(token by value)", SUCCEEDED(hr) && tick.refs == 1, "");
        }
    }
}

/* ---- app unit ---- */
static void x104_st_app(void)
{
    void *statics = NULL, *object = NULL, *action = NULL, *uri = NULL, *core = NULL, *agile = NULL;
    HRESULT hr;
    int number = 0;
    float dpi = 0.0f;
    X104Rect bounds;
    HSTRING text = NULL;
    HSTRING back = NULL;

    if (x104_st_activate(L"Windows.Graphics.Display.DisplayInformation", &X104_IID_IDisplayInformationStatics, &statics, "app: DisplayInformation activation")) {
        hr = X104_ST_FN(statics, IDisplayInformationStatics, GetForCurrentView)(statics, &object);
        if (SUCCEEDED(hr) && object) {
            void *info = NULL;
            if (SUCCEEDED(x104_raw_qi(object, &X104_IID_IDisplayInformation, &info)) && info) {
                hr = X104_ST_FN(info, IDisplayInformation, get_LogicalDpi)(info, &dpi);
                X104_ST_CHECK("app: DisplayInformation.LogicalDpi == 96.0f (float through a pointer)", SUCCEEDED(hr) && dpi == 96.0f, "");
                dpi = 0.0f;
                hr = X104_ST_FN(info, IDisplayInformation, get_RawDpiX)(info, &dpi);
                X104_ST_CHECK("app: DisplayInformation.RawDpiX == 96.0f", SUCCEEDED(hr) && dpi == 96.0f, "");
                hr = X104_ST_FN(info, IDisplayInformation, get_CurrentOrientation)(info, &number);
                X104_ST_CHECK("app: DisplayInformation.CurrentOrientation == Landscape (1)", SUCCEEDED(hr) && number == 1, "");
            } else {
                x104_st_result("app: IDisplayInformation", -1, "QI failed");
            }
        } else {
            x104_st_result("app: DisplayInformation.GetForCurrentView", -1, "failed");
        }
    }

    if (x104_st_activate(L"Windows.Foundation.Uri", &X104_IID_IUriRuntimeClassFactory, &statics, "app: Uri activation")) {
        x104_hstring_create(L"ms-appx:///MainPage.xaml", &text);
        hr = X104_ST_FN(statics, IUriRuntimeClassFactory, CreateUri)(statics, text, &uri);
        if (SUCCEEDED(hr) && uri && SUCCEEDED(X104_ST_FN(uri, IUriRuntimeClass, get_AbsoluteUri)(uri, &back))) {
            X104_ST_CHECK("app: Uri round trip", x104_hstring_equals(back, L"ms-appx:///MainPage.xaml"), "");
            x104_hstring_free(back);
        } else {
            x104_st_result("app: Uri round trip", -1, "CreateUri / get_AbsoluteUri failed");
        }
        x104_hstring_free(text);
    }

    if (x104_st_activate(L"Windows.UI.ViewManagement.StatusBar", &X104_IID_IStatusBarStatics, &statics, "app: StatusBar activation")) {
        hr = X104_ST_FN(statics, IStatusBarStatics, GetForCurrentView)(statics, &object);
        if (SUCCEEDED(hr) && object) {
            hr = X104_ST_FN(object, IStatusBar, HideAsync)(object, &action);
            X104_ST_CHECK("app: StatusBar.HideAsync returns a completed IAsyncAction", SUCCEEDED(hr) && action && x104_st_status(action) == 1, "");
        } else {
            x104_st_result("app: StatusBar.GetForCurrentView", -1, "failed");
        }
    }

    if (x104_st_activate(L"Windows.UI.Xaml.Window", &X104_IID_IWindowStatics, &statics, "app: Window activation")) {
        hr = X104_ST_FN(statics, IWindowStatics, get_Current)(statics, &object);
        if (SUCCEEDED(hr) && object) {
            memset(&bounds, 0, sizeof(bounds));
            hr = X104_ST_FN(object, IWindow, get_Bounds)(object, &bounds);
            X104_ST_CHECK("app: Window.Bounds is the client rect (Rect through a pointer)", SUCCEEDED(hr) && bounds.Width > 0.0f && bounds.Height > 0.0f, "");
            hr = X104_ST_FN(object, IWindow, get_CoreWindow)(object, &core);
            X104_ST_CHECK("app: Window.CoreWindow is the input unit's CoreWindow and answers IAgileObject (mandatory)",
                          SUCCEEDED(hr) && core && SUCCEEDED(x104_raw_qi(core, &X104_IID_IAgileObject, &agile)), "");
        } else {
            x104_st_result("app: Window.Current", -1, "failed");
        }
    }
}

/* ---- panel unit ---- */
static void x104_st_panel(void)
{
    X104Object *panel = x104_panel_object();
    void *scp = NULL, *fe = NULL, *ui = NULL, *native = NULL;
    X104StMock handler;
    LONGLONG token = 0;
    float scale = 0.0f;
    double width = 0.0, height = 0.0;
    X104Size size;
    HRESULT hr;

    if (!panel) {
        x104_st_result("panel: SwapChainPanel", 0, "panel unit not implemented");
        return;
    }
    if (FAILED(x104_object_qi(panel, &X104_IID_ISwapChainPanel, &scp)) || FAILED(x104_object_qi(panel, &X104_IID_IFrameworkElement, &fe)) ||
        FAILED(x104_object_qi(panel, &X104_IID_IUIElement, &ui)) || FAILED(x104_object_qi(panel, &X104_IID_ISwapChainPanelNative, &native))) {
        x104_st_result("panel: QI matrix (ISwapChainPanel, IFrameworkElement, IUIElement, ISwapChainPanelNative)", -1, "a QI failed (see QI MISS above)");
        return;
    }
    hr = X104_ST_FN(scp, ISwapChainPanel, get_CompositionScaleX)(scp, &scale);
    X104_ST_CHECK("panel: CompositionScaleX == 1.0f", SUCCEEDED(hr) && scale == 1.0f, "");
    hr = X104_ST_FN(fe, IFrameworkElement, get_ActualWidth)(fe, &width);
    X104_ST_CHECK("panel: ActualWidth > 0 (double through a pointer)", SUCCEEDED(hr) && width > 0.0, "");
    hr = X104_ST_FN(fe, IFrameworkElement, get_ActualHeight)(fe, &height);
    X104_ST_CHECK("panel: ActualHeight > 0", SUCCEEDED(hr) && height > 0.0, "");
    memset(&size, 0, sizeof(size));
    hr = X104_ST_FN(ui, IUIElement, get_RenderSize)(ui, &size);
    X104_ST_CHECK("panel: RenderSize == ActualWidth/Height (Size through a pointer)", SUCCEEDED(hr) && (double)size.Width == width && (double)size.Height == height, "");
    x104_st_mock_init(&handler);
    hr = X104_ST_FN(fe, IFrameworkElement, add_SizeChanged)(fe, &handler, &token);
    X104_ST_CHECK("panel: add_SizeChanged + remove_SizeChanged(token by value)", SUCCEEDED(hr) && token != 0 &&
                  SUCCEEDED(X104_ST_FN(fe, IFrameworkElement, remove_SizeChanged)(fe, token)) && handler.refs == 1, "");
}

/* ---- input unit ---- */
static void x104_st_input(void)
{
    X104Object *core = x104_input_core_window();
    X104Object *editbox = x104_input_element_editbox();
    void *window = NULL, *box = NULL, *statics = NULL, *pane_object = NULL;
    X104StMock handler;
    LONGLONG token = 0;
    X104Rect bounds;
    HRESULT hr;
    HSTRING text = NULL, back = NULL;
    INT max_length = 0;

    if (!core) {
        x104_st_result("input: CoreWindow", 0, "input unit not implemented");
    } else if (SUCCEEDED(x104_object_qi(core, &X104_IID_ICoreWindow, &window))) {
        memset(&bounds, 0, sizeof(bounds));
        hr = X104_ST_FN(window, ICoreWindow, get_Bounds)(window, &bounds);
        X104_ST_CHECK("input: CoreWindow.Bounds", SUCCEEDED(hr) && bounds.Width > 0.0f, "");
        x104_st_mock_init(&handler);
        hr = X104_ST_FN(window, ICoreWindow, add_KeyDown)(window, &handler, &token);
        X104_ST_CHECK("input: CoreWindow.add_KeyDown + remove_KeyDown(token by value)", SUCCEEDED(hr) && token != 0 &&
                      SUCCEEDED(X104_ST_FN(window, ICoreWindow, remove_KeyDown)(window, token)) && handler.refs == 1, "");
    } else {
        x104_st_result("input: CoreWindow", -1, "QI(ICoreWindow) failed");
    }
    if (editbox && SUCCEEDED(x104_object_qi(editbox, &X104_IID_ITextBox, &box))) {
        x104_hstring_create(L"abc", &text);
        hr = X104_ST_FN(box, ITextBox, put_Text)(box, text);
        back = NULL;
        X104_ST_CHECK("input: EditBox.put_Text / get_Text round trip", SUCCEEDED(hr) && SUCCEEDED(X104_ST_FN(box, ITextBox, get_Text)(box, &back)) &&
                      x104_hstring_equals(back, L"abc"), "");
        x104_hstring_free(back);
        x104_hstring_free(text);
        hr = X104_ST_FN(box, ITextBox, put_MaxLength)(box, 123);
        X104_ST_CHECK("input: EditBox.MaxLength round trip", SUCCEEDED(hr) && SUCCEEDED(X104_ST_FN(box, ITextBox, get_MaxLength)(box, &max_length)) && max_length == 123, "");
        X104_ST_FN(box, ITextBox, put_MaxLength)(box, 50000);
        x104_hstring_create(L"", &text);
        X104_ST_FN(box, ITextBox, put_Text)(box, text);
        x104_hstring_free(text);
    } else {
        x104_st_result("input: EditBox TextBox", editbox ? -1 : 0, editbox ? "QI(ITextBox) failed" : "input unit not implemented");
    }
    if (x104_st_activate(L"Windows.UI.ViewManagement.InputPane", &X104_IID_IInputPaneStatics, &statics, "input: InputPane activation")) {
        hr = X104_ST_FN(statics, IInputPaneStatics, GetForCurrentView)(statics, &pane_object);
        if (SUCCEEDED(hr) && pane_object) {
            x104_st_mock_init(&handler);
            token = 0;
            hr = X104_ST_FN(pane_object, IInputPane, add_Showing)(pane_object, &handler, &token);
            X104_ST_CHECK("input: InputPane.add_Showing + remove_Showing", SUCCEEDED(hr) && token != 0 &&
                          SUCCEEDED(X104_ST_FN(pane_object, IInputPane, remove_Showing)(pane_object, token)), "");
            memset(&bounds, 0xAA, sizeof(bounds));
            hr = X104_ST_FN(pane_object, IInputPane, get_OccludedRect)(pane_object, &bounds);
            X104_ST_CHECK("input: InputPane.OccludedRect is {0,0,0,0} while no keyboard is up", SUCCEEDED(hr) && bounds.Height == 0.0f, "");
        } else {
            x104_st_result("input: InputPane.GetForCurrentView", -1, "failed");
        }
    }
}

/* ---- platform unit ---- */
static BOOL x104_st_wide_equals_nocase(const wchar_t *left, const wchar_t *right)
{
    while (*left && *right) {
        wchar_t a = *left++, b = *right++;
        if (a >= L'A' && a <= L'Z') a = (wchar_t)(a + 32);
        if (b >= L'A' && b <= L'Z') b = (wchar_t)(b + 32);
        if (a != b) return FALSE;
    }
    return *left == *right;
}

static BOOL x104_st_path_equals(void *storage_folder, const wchar_t *expected, const char *label)
{
    void *item = NULL;
    HSTRING path = NULL;
    wchar_t wide[MAX_PATH];
    BOOL ok = FALSE;
    if (SUCCEEDED(x104_raw_qi(storage_folder, &X104_IID_IStorageItem, &item)) && item && SUCCEEDED(X104_ST_FN(item, IStorageItem, get_Path)(item, &path)) && path) {
        x104_hstring_wide(path, wide, ARRAYSIZE(wide));
        ok = x104_st_wide_equals_nocase(wide, expected);
        if (!ok) {
            char got[160], want[160], line[420];
            x104_narrow(got, sizeof(got), wide);
            x104_narrow(want, sizeof(want), expected);
            wsprintfA(line, "%s: got '%s' expected '%s'", label, got, want);
            x104_logf("X104 selftest: path mismatch %s", line);
        }
        x104_hstring_free(path);
    }
    return ok;
}

static void x104_st_platform(void)
{
    void *statics = NULL, *object = NULL, *folder = NULL, *settings = NULL, *profile = NULL;
    HRESULT hr;
    ULONGLONG usage = 0, limit = 0;
    void *values = (void *)1;

    if (x104_st_activate(L"Windows.ApplicationModel.Package", &X104_IID_IPackageStatics, &statics, "platform: Package activation")) {
        hr = X104_ST_FN(statics, IPackageStatics, get_Current)(statics, &object);
        if (SUCCEEDED(hr) && object && SUCCEEDED(X104_ST_FN(object, IPackage, get_InstalledLocation)(object, &folder)) && folder)
            X104_ST_CHECK("platform: Package.Current.InstalledLocation.Path == game folder (via IStorageItem QI)", x104_st_path_equals(folder, x104.game_root, "InstalledLocation"), "");
        else
            x104_st_result("platform: Package.Current.InstalledLocation", -1, "call chain failed");
    }
    if (x104_st_activate(L"Windows.Storage.ApplicationData", &X104_IID_IApplicationDataStatics, &statics, "platform: ApplicationData activation")) {
        hr = X104_ST_FN(statics, IApplicationDataStatics, get_Current)(statics, &object);
        folder = NULL;
        if (SUCCEEDED(hr) && object && SUCCEEDED(X104_ST_FN(object, IApplicationData, get_LocalFolder)(object, &folder)) && folder)
            X104_ST_CHECK("platform: ApplicationData.LocalFolder.Path == data root", x104_st_path_equals(folder, x104.data_root, "LocalFolder"), "");
        else
            x104_st_result("platform: ApplicationData.LocalFolder", -1, "call chain failed");
        if (object && SUCCEEDED(X104_ST_FN(object, IApplicationData, get_LocalSettings)(object, &settings)) && settings) {
            hr = X104_ST_FN(settings, IApplicationDataContainer, get_Values)(settings, &values);
            X104_ST_CHECK("platform: LocalSettings is non-NULL and Values is callable (S_OK, may be NULL)", SUCCEEDED(hr), "");
        } else {
            x104_st_result("platform: ApplicationData.LocalSettings", -1, "NULL or failed (the Suspending handler dereferences it)");
        }
    }
    if (x104_st_activate(L"Windows.System.MemoryManager", &X104_IID_IMemoryManagerStatics, &statics, "platform: MemoryManager activation")) {
        hr = X104_ST_FN(statics, IMemoryManagerStatics, get_AppMemoryUsage)(statics, &usage);
        X104_ST_CHECK("platform: MemoryManager usage/limit (64-bit outputs): limit > usage + 0xE600000", SUCCEEDED(hr) &&
                      SUCCEEDED(X104_ST_FN(statics, IMemoryManagerStatics, get_AppMemoryUsageLimit)(statics, &limit)) && limit > usage + 0x0E600000ULL, "");
    }
    if (x104_st_activate(L"Windows.Phone.Devices.Notification.VibrationDevice", &X104_IID_IVibrationDeviceStatics, &statics, "platform: VibrationDevice activation")) {
        hr = X104_ST_FN(statics, IVibrationDeviceStatics, GetDefault)(statics, &object);
        if (SUCCEEDED(hr) && object) {
            X104_ST_CHECK("platform: VibrationDevice.Vibrate(TimeSpan 100 ms) + Cancel (the game calls them without QI)", SUCCEEDED(X104_ST_FN(object, IVibrationDevice, Vibrate)(object, 1000000LL)) &&
                          SUCCEEDED(X104_ST_FN(object, IVibrationDevice, Cancel)(object)), "");
        } else {
            x104_st_result("platform: VibrationDevice.GetDefault", -1, "failed");
        }
    }
    if (x104_st_activate(L"Windows.Networking.Connectivity.NetworkInformation", &X104_IID_INetworkInformationStatics, &statics, "platform: NetworkInformation activation")) {
        hr = X104_ST_FN(statics, INetworkInformationStatics, GetInternetConnectionProfile)(statics, &profile);
        if (SUCCEEDED(hr) && profile) {
            int level = -1;
            void *cp = NULL;
            X104_ST_CHECK("platform: connection profile reports InternetAccess (3)", SUCCEEDED(x104_raw_qi(profile, &X104_IID_IConnectionProfile, &cp)) && cp &&
                          SUCCEEDED(X104_ST_FN(cp, IConnectionProfile, GetNetworkConnectivityLevel)(cp, &level)) && level == 3, "");
        } else {
            x104_st_result("platform: NetworkInformation profile", SUCCEEDED(hr) ? 0 : -1, SUCCEEDED(hr) ? "NULL profile (--offline?)" : "call failed");
        }
    }
}

/* returns the number of FAILED checks (SKIPs are not failures) */
static int x104_selftest_fakes(void)
{
    x104_logf("X104 selftest: ===== begin (checks the ABI of every implemented fake through the generated slot typedefs; SKIP = the unit does not implement the class yet) =====");
    x104_st_pass = x104_st_fail = x104_st_skip = 0;
    x104_st_core();
    x104_st_async();
    x104_st_app();
    x104_st_panel();
    x104_st_input();
    x104_st_platform();
    x104_logf("X104 selftest: ===== %d passed, %d FAILED, %d skipped =====", x104_st_pass, x104_st_fail, x104_st_skip);
    return x104_st_fail;
}

#endif /* WIN32_HOST_ARM32_0104_SELFTEST_INL */
