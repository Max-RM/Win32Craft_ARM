/* Host-side (x86, runs on the PC) test of win32_host_arm32_0104_app.inl: the REAL app unit against the pristine stubs of the other units plus mocks of the few
 * cross-unit functions it calls.  A mock "game" plays App2 / MainPage: it makes the same calls through the same vtables (activation table, aggregation, FindName,
 * Window.Content/Activate, BackPressed, events, Suspending ...), so the thunk wiring, the ordering rules and the exit path are exercised without a device.
 * Build + run:  python tools/arm32/test_0104_app.py
 * (the ARM register passing of float/double/LONGLONG arguments is NOT covered here: that is what X104_SLOT checks at compile time and --selftest-fakes on the device). */
#include "win32_compat.h"
#define MAKELONG_FOR_TEST(lo, hi) ((WPARAM)(((hi) << 16) | (lo)))

extern void *WINAPI GetStdHandle(DWORD);
extern char *strstr(const char *, const char *);
extern int strcmp(const char *, const char *);
extern int WINAPI wvsprintfA(LPSTR, LPCSTR, __builtin_va_list);
extern void WINAPI RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR *);
extern HWND WINAPI CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, void *, HINSTANCE, void *);

static int g_failures;
static int g_checks;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; test_print("CHECK FAILED line %d: %s\n", __LINE__, #cond); } } while (0)

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
static char g_log[200000];
static int g_log_len;
static int g_verbose = 0;
static void log_line(const char *text)
{
    int length = (int)strlen(text);
    if (g_log_len + length + 2 < (int)sizeof(g_log)) {
        memcpy(g_log + g_log_len, text, (size_t)length);
        g_log_len += length;
        g_log[g_log_len++] = '\n';
        g_log[g_log_len] = 0;
    }
    if (g_verbose) test_print("  [log] %s\n", text);
}
static int log_has(const char *needle) { return strstr(g_log, needle) != NULL; }
static void log_hresult(const char *operation, HRESULT result) { test_print("  [hr] %s 0x%08lx\n", operation, (unsigned long)result); }
/* host_next_argument / host_argument_equals of win32_host.c (the --trace-game-state parser of the unit uses them) */
static wchar_t host_ascii_lower_w(wchar_t value) { return (value >= L'A' && value <= L'Z') ? (wchar_t)(value + (L'a' - L'A')) : value; }
static BOOL host_argument_equals(const wchar_t *left, const wchar_t *right)
{
    while (*left && *right) {
        if (host_ascii_lower_w(*left) != host_ascii_lower_w(*right)) return FALSE;
        ++left;
        ++right;
    }
    return *left == 0 && *right == 0;
}
static const wchar_t *host_next_argument(const wchar_t *cursor, wchar_t *argument, UINT capacity)
{
    UINT length = 0;
    BOOL quoted = FALSE;
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    while (*cursor) {
        if (*cursor == L'"') { quoted = !quoted; ++cursor; continue; }
        if (!quoted && (*cursor == L' ' || *cursor == L'\t')) break;
        if (length + 1 < capacity) argument[length++] = *cursor;
        ++cursor;
    }
    argument[length] = 0;
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    return cursor;
}
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

/* ---- the pristine stubs of the other units, with the functions this test mocks renamed out of the way ---- */
#define x104_ui_run_loop x104_stub_ui_run_loop
#define x104_ui_quit x104_stub_ui_quit
#define x104_ui_post x104_stub_ui_post
#define x104_ui_pump_once x104_stub_ui_pump_once
#define x104_async_shutdown x104_stub_async_shutdown
#define x104_async_dispatcher_object x104_stub_async_dispatcher_object
#define x104_panel_client_size x104_stub_panel_client_size
#define x104_panel_object x104_stub_panel_object
#define x104_input_core_window x104_stub_input_core_window
#define x104_input_element_logo x104_stub_input_element_logo
#define x104_input_element_editbox x104_stub_input_element_editbox
#define x104_input_element_button x104_stub_input_element_button
#define x104_input_raise_visibility_changed x104_stub_input_raise_visibility_changed
#define x104_input_release_all_pointers x104_stub_input_release_all_pointers
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_async.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_panel.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_input.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_platform.inl"
#undef x104_ui_run_loop
#undef x104_ui_quit
#undef x104_ui_post
#undef x104_ui_pump_once
#undef x104_async_shutdown
#undef x104_async_dispatcher_object
#undef x104_panel_client_size
#undef x104_panel_object
#undef x104_input_core_window
#undef x104_input_element_logo
#undef x104_input_element_editbox
#undef x104_input_element_button
#undef x104_input_raise_visibility_changed
#undef x104_input_release_all_pointers

/* ---- mocks of the cross-unit functions the app unit calls ---- */
typedef HRESULT (WINAPI *MockFn3)(void *, void *, void *);
static void (*g_queue_fn[16])(void *);
static void *g_queue_arg[16];
static int g_queue_count;
static BOOL g_queue_closed;
static int g_quit_code = -1;
static int g_quit_calls;
static int g_shutdown_calls;
static int g_release_pointers_calls;
static int g_visibility_calls;
static int g_visibility_last = -1;
static int g_loop_calls;
static void (*g_loop_hook)(void);
static float g_client_w = 1280.0f, g_client_h = 720.0f;

static BOOL x104_ui_post(void (*function)(void *), void *argument)
{
    if (g_queue_closed || g_queue_count >= 16) return FALSE;
    g_queue_fn[g_queue_count] = function;
    g_queue_arg[g_queue_count] = argument;
    ++g_queue_count;
    return TRUE;
}
static void x104_ui_pump_once(void)
{
    int index;
    for (index = 0; index < g_queue_count; ++index) g_queue_fn[index](g_queue_arg[index]);
    g_queue_count = 0;
}
static void x104_ui_quit(int code)
{
    ++g_quit_calls;
    g_quit_code = code;
    g_queue_closed = TRUE;
    x104.exit_code = code;
}
static int x104_ui_run_loop(void)
{
    ++g_loop_calls;
    if (g_loop_hook) g_loop_hook();
    x104_ui_pump_once();
    return x104.exit_code;
}
static void x104_async_shutdown(void) { ++g_shutdown_calls; }
static X104Object *x104_async_dispatcher_object(void) { return NULL; }
static void x104_panel_client_size(float *w, float *h) { *w = g_client_w; *h = g_client_h; }
static void x104_input_raise_visibility_changed(BOOL visible) { ++g_visibility_calls; g_visibility_last = visible ? 1 : 0; }
static void x104_input_release_all_pointers(const char *why) { (void)why; ++g_release_pointers_calls; }

/* mock XAML elements (the panel / logo / editbox / button objects of the other units): IInspectable-only objects */
X104_VTBL(vt_mock_element, ISwapChainPanel) = { X104_INSPECTABLE_SLOTS };
static const X104IfaceDesc mock_element_ifaces[] = { X104_IFACE(ISwapChainPanel, vt_mock_element) };
X104_CLASS_DEF(mock_element_class, "MockElement", L"Mock.Element", mock_element_ifaces, X104_CLASS_AGILE, NULL);
X104_VTBL(vt_mock_cw, ICoreWindow) = { X104_INSPECTABLE_SLOTS };
static const X104IfaceDesc mock_cw_ifaces[] = { X104_IFACE(ICoreWindow, vt_mock_cw) };
X104_CLASS_DEF(mock_cw_class, "MockCW", L"Mock.CW", mock_cw_ifaces, X104_CLASS_AGILE, NULL);
static X104Object *g_mock_panel, *g_mock_logo, *g_mock_editbox, *g_mock_button, *g_mock_core_window;
static X104Object *x104_panel_object(void) { return g_mock_panel; }
static X104Object *x104_input_core_window(void) { return g_mock_core_window; }
static X104Object *x104_input_element_logo(void) { return g_mock_logo; }
static X104Object *x104_input_element_editbox(void) { return g_mock_editbox; }
static X104Object *x104_input_element_button(void) { return g_mock_button; }

#include "win32_host_arm32_0104_app.inl"

/* ---- a mock COM delegate: vtbl = { QI, AddRef, Release, Invoke } ---- */
typedef struct MockDelegate { const void **vtbl; LONG refs; LONG invoked; void *last_sender; void *last_args; HRESULT result; int put_handled; } MockDelegate;
static HRESULT WINAPI mock_qi(void *self, const GUID *iid, void **out) { (void)iid; *out = self; ((MockDelegate *)self)->refs++; return S_OK; }
static ULONG WINAPI mock_addref(void *self) { return (ULONG)++((MockDelegate *)self)->refs; }
static ULONG WINAPI mock_release(void *self) { return (ULONG)--((MockDelegate *)self)->refs; }
static HRESULT WINAPI mock_invoke(void *self, void *sender, void *args)
{
    MockDelegate *d = (MockDelegate *)self;
    d->invoked++;
    d->last_sender = sender;
    d->last_args = args;
    return d->result;
}
static const void *mock_vtbl[4] = { mock_qi, mock_addref, mock_release, mock_invoke };

/* the BackPressed handler: calls args->put_Handled(d->put_handled) straight on the pointer it receives, like MainPage::OnBackPressed 0x1DF4E8 */
static HRESULT WINAPI mock_back_invoke(void *self, void *sender, void *args)
{
    MockDelegate *d = (MockDelegate *)self;
    d->invoked++;
    d->last_sender = sender;
    d->last_args = args;
    return X104_VFN(args, 7, HRESULT (WINAPI *)(void *, BYTE))(args, (BYTE)d->put_handled);
}
static const void *mock_back_vtbl[4] = { mock_qi, mock_addref, mock_release, mock_back_invoke };

/* the Suspending handler: walks args -> operation -> deferral -> deadline */
static int g_susp_chain_ok;
static HRESULT WINAPI mock_susp_invoke(void *self, void *sender, void *args)
{
    MockDelegate *d = (MockDelegate *)self;
    void *operation = NULL, *deferral = NULL;
    LONGLONG deadline = 0;
    d->invoked++;
    d->last_sender = sender;
    d->last_args = args;
    if (SUCCEEDED(X104_VFN(args, 6, HRESULT (WINAPI *)(void *, void **))(args, &operation)) && operation &&
        SUCCEEDED(X104_VFN(operation, 6, HRESULT (WINAPI *)(void *, void **))(operation, &deferral)) && deferral &&
        SUCCEEDED(X104_VFN(operation, 7, HRESULT (WINAPI *)(void *, LONGLONG *))(operation, &deadline)) && deadline > 0 &&
        SUCCEEDED(X104_VFN(deferral, 6, HRESULT (WINAPI *)(void *))(deferral)))
        g_susp_chain_ok = 1;
    return d->result;
}
static const void *mock_susp_vtbl[4] = { mock_qi, mock_addref, mock_release, mock_susp_invoke };

/* ---- the mock game: App2 (outer with QI), the ApplicationInitializationCallback, OnLaunched, MainPage memory ---- */
typedef struct MockOuter { const void **vtbl; LONG refs; void *overrides; } MockOuter;
static MockOuter g_app_outer;
static MockOuter g_page_outer;
static void *g_inner_app, *g_inner_page, *g_instance_app, *g_instance_page;
static BYTE g_mainpage_memory[0x100] __attribute__((aligned(16)));
static BYTE g_appmain_memory[0x80] __attribute__((aligned(16)));
static MockDelegate g_susp_handler = { mock_susp_vtbl, 1, 0, NULL, NULL, S_OK, 0 };
static MockDelegate g_resume_handler = { mock_vtbl, 1, 0, NULL, NULL, S_OK, 0 };
static MockDelegate g_back_handler = { mock_back_vtbl, 1, 0, NULL, NULL, S_OK, 1 };
static MockDelegate g_dpi_handler = { mock_vtbl, 1, 0, NULL, NULL, S_OK, 0 };
static MockDelegate g_orientation_handler = { mock_vtbl, 1, 0, NULL, NULL, S_OK, 0 };
static MockDelegate g_contents_handler = { mock_vtbl, 1, 0, NULL, NULL, S_OK, 0 };
static int g_onlaunched_calls;
static int g_onlaunched_state_seen = -1;
static void *g_overrides_args;
static HRESULT g_onlaunched_result = S_OK;
static int g_page_qi_ok;
static int g_findname_ok;
static void *g_window_content_seen;
static int g_activate_result = -1;

static HRESULT WINAPI mock_outer_qi(void *self, const GUID *iid, void **out)
{
    MockOuter *outer = (MockOuter *)self;
    *out = NULL;
    if (memcmp(iid, &X104_IID_IApplicationOverrides, sizeof(GUID)) == 0 && outer->overrides) {
        *out = outer->overrides;
        outer->refs++;
        return S_OK;
    }
    if (memcmp(iid, &X104_IID_IUnknown, sizeof(GUID)) == 0 || memcmp(iid, &X104_IID_IInspectable, sizeof(GUID)) == 0) {
        *out = self;
        outer->refs++;
        return S_OK;
    }
    /* like App2::vf0 / MainPage::QI: unknown IIDs go to the inner */
    {
        void *inner = (outer == &g_app_outer) ? g_inner_app : g_inner_page;
        return X104_VFN(inner, 0, HRESULT (WINAPI *)(void *, const GUID *, void **))(inner, iid, out);
    }
}
static ULONG WINAPI mock_outer_addref(void *self) { return (ULONG)++((MockOuter *)self)->refs; }
static ULONG WINAPI mock_outer_release(void *self) { return (ULONG)--((MockOuter *)self)->refs; }
static const void *mock_outer_vtbl[3] = { mock_outer_qi, mock_outer_addref, mock_outer_release };

/* get a statics/factory interface like the game's sub_2001E4 does (through the unit's activation hook) */
static void *mock_activate(const wchar_t *class_name, const GUID *iid)
{
    void *out = NULL;
    HRESULT hr = x104_app_activate(class_name, iid, &out);
    if (FAILED(hr)) test_print("  activation of %ls failed 0x%08lx\n", class_name, (unsigned long)hr);
    return SUCCEEDED(hr) ? out : NULL;
}

static HSTRING mock_hs(const wchar_t *text)
{
    HSTRING h = NULL;
    x104_hstring_create(text, &h);
    return h;
}

/* IApplicationOverrides.OnLaunched(args): what App2::OnLaunched 0x1E3A30 does, call for call */
static HRESULT WINAPI mock_on_launched(void *self, void *args, void *unused3)
{
    (void)unused3;
    void *statusbar_statics, *statusbar = NULL, *action = NULL;
    void *page_factory, *window_statics, *window = NULL, *canvas, *hw, *display_statics, *display = NULL, *uri_factory, *uri = NULL;
    void *activated = NULL, *core = NULL, *fe = NULL, *ui = NULL, *dispatcher = NULL, *panel = NULL, *named = NULL;
    HRESULT hr;
    int value = -1;
    float dpi = 0.0f;
    LONGLONG token = 0;
    HSTRING name;
    (void)self;
    ++g_onlaunched_calls;
    g_overrides_args = args;
    /* StatusBar.GetForCurrentView().HideAsync() */
    statusbar_statics = mock_activate(L"Windows.UI.ViewManagement.StatusBar", &X104_IID_IStatusBarStatics);
    CHECK(statusbar_statics != NULL);
    hr = X104_VFN(statusbar_statics, 6, HRESULT (WINAPI *)(void *, void **))(statusbar_statics, &statusbar);
    CHECK(SUCCEEDED(hr) && statusbar);
    hr = X104_VFN(statusbar, 7, HRESULT (WINAPI *)(void *, void **))(statusbar, &action);
    CHECK(SUCCEEDED(hr) && action);
    /* MainPage constructor: Page base via IPageFactory.CreateInstance */
    page_factory = mock_activate(L"Windows.UI.Xaml.Controls.Page", &X104_IID_IPageFactory);
    CHECK(page_factory != NULL);
    g_page_outer.vtbl = mock_outer_vtbl;
    g_page_outer.refs = 1;
    memset(g_mainpage_memory, 0, sizeof(g_mainpage_memory));
    memcpy(g_mainpage_memory + 8, &g_page_outer, sizeof(void *));
    ((MockOuter *)(g_mainpage_memory + 8))->vtbl = mock_outer_vtbl; ((MockOuter *)(g_mainpage_memory + 8))->refs = 1;
    hr = X104_VFN(page_factory, 6, HRESULT (WINAPI *)(void *, void *, void **, void **))(page_factory, g_mainpage_memory + 8, &g_inner_page, &g_instance_page);
    CHECK(SUCCEEDED(hr) && g_inner_page && g_instance_page);
    /* the MainPage outer sub-object is at MainPage+8: build the QI-forwarding outer there */
    memcpy(g_mainpage_memory + 8, &g_page_outer, sizeof(void *));
    ((MockOuter *)(g_mainpage_memory + 8))->vtbl = mock_outer_vtbl;
    /* InitializeComponent: Uri + LoadComponentWithResourceLocation + QI(this, IFrameworkElement) + FindName x4 */
    uri_factory = mock_activate(L"Windows.Foundation.Uri", &X104_IID_IUriRuntimeClassFactory);
    name = mock_hs(L"ms-appx:///MainPage.xaml");
    hr = X104_VFN(uri_factory, 6, HRESULT (WINAPI *)(void *, HSTRING, void **))(uri_factory, name, &uri);
    CHECK(SUCCEEDED(hr) && uri);
    x104_hstring_free(name);
    {
        void *app_statics = mock_activate(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationStatics);
        CHECK(app_statics != NULL);
        hr = X104_VFN(app_statics, 9, HRESULT (WINAPI *)(void *, void *, void *, int))(app_statics, g_mainpage_memory, uri, 0);
        CHECK(hr == S_OK);
        hr = X104_VFN(app_statics, 8, HRESULT (WINAPI *)(void *, void *, void *))(app_statics, g_mainpage_memory, uri);
        CHECK(hr == S_OK);
    }
    hr = mock_outer_qi(g_mainpage_memory + 8, &X104_IID_IFrameworkElement, &fe);
    CHECK(SUCCEEDED(hr) && fe);
    {
        static const wchar_t *names[4] = { L"swapChainPanel", L"Logo", L"EditBox", L"Button" };
        X104Object *expected[4];
        int index;
        expected[0] = g_mock_panel; expected[1] = g_mock_logo; expected[2] = g_mock_editbox; expected[3] = g_mock_button;
        g_findname_ok = 1;
        for (index = 0; index < 4; ++index) {
            named = NULL;
            name = mock_hs(names[index]);
            hr = X104_VFN(fe, 51, HRESULT (WINAPI *)(void *, HSTRING, void **))(fe, name, &named);
            if (FAILED(hr) || named != (void *)&expected[index]->views[0]) g_findname_ok = 0;
            x104_hstring_free(name);
        }
        named = (void *)1;
        name = mock_hs(L"swapchainpanel");
        hr = X104_VFN(fe, 51, HRESULT (WINAPI *)(void *, HSTRING, void **))(fe, name, &named);
        CHECK(hr == S_OK && named == NULL);        /* names are case sensitive: unknown -> S_OK + NULL */
        x104_hstring_free(name);
    }
    /* Window.Current.CoreWindow */
    window_statics = mock_activate(L"Windows.UI.Xaml.Window", &X104_IID_IWindowStatics);
    hr = X104_VFN(window_statics, 6, HRESULT (WINAPI *)(void *, void **))(window_statics, &window);
    CHECK(SUCCEEDED(hr) && window);
    hr = X104_VFN(window, 10, HRESULT (WINAPI *)(void *, void **))(window, &core);
    CHECK(SUCCEEDED(hr) && core == (void *)&g_mock_core_window->views[0]);
    /* DisplayInformation: GetForCurrentView, events, getters */
    display_statics = mock_activate(L"Windows.Graphics.Display.DisplayInformation", &X104_IID_IDisplayInformationStatics);
    hr = X104_VFN(display_statics, 6, HRESULT (WINAPI *)(void *, void **))(display_statics, &display);
    CHECK(SUCCEEDED(hr) && display);
    CHECK(SUCCEEDED(X104_VFN(display, 14, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(display, &g_dpi_handler, &token)) && token != 0);
    CHECK(SUCCEEDED(X104_VFN(display, 8, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(display, &g_orientation_handler, &token)));
    CHECK(SUCCEEDED(X104_VFN(display_statics, 9, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(display_statics, &g_contents_handler, &token)));
    CHECK(SUCCEEDED(X104_VFN(display, 11, HRESULT (WINAPI *)(void *, float *))(display, &dpi)) && dpi == 96.0f);
    dpi = 0;
    CHECK(SUCCEEDED(X104_VFN(display, 12, HRESULT (WINAPI *)(void *, float *))(display, &dpi)) && dpi == 96.0f);
    dpi = 0;
    CHECK(SUCCEEDED(X104_VFN(display, 13, HRESULT (WINAPI *)(void *, float *))(display, &dpi)) && dpi == 96.0f);
    CHECK(SUCCEEDED(X104_VFN(display, 6, HRESULT (WINAPI *)(void *, int *))(display, &value)) && value == 1);
    value = 0;
    CHECK(SUCCEEDED(X104_VFN(display, 7, HRESULT (WINAPI *)(void *, int *))(display, &value)) && value == 1);
    value = 0;
    CHECK(SUCCEEDED(X104_VFN(display, 10, HRESULT (WINAPI *)(void *, int *))(display, &value)) && value == 100);
    /* panel SizeChanged registration is the panel unit's; Canvas.SetLeft x2; HardwareButtons.add_BackPressed */
    canvas = mock_activate(L"Windows.UI.Xaml.Controls.Canvas", &X104_IID_ICanvasStatics);
    CHECK(canvas != NULL);
    CHECK(X104_VFN(canvas, 8, HRESULT (WINAPI *)(void *, void *, double))(canvas, g_mock_editbox, 3000.0) == S_OK);
    CHECK(X104_VFN(canvas, 8, HRESULT (WINAPI *)(void *, void *, double))(canvas, g_mock_button, 3000.0) == S_OK);
    hw = mock_activate(L"Windows.Phone.UI.Input.HardwareButtons", &X104_IID_IHardwareButtonsStatics);
    CHECK(hw != NULL);
    CHECK(SUCCEEDED(X104_VFN(hw, 6, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(hw, &g_back_handler, &token)) && token != 0);
    /* QI(args, IActivatedEventArgs)->get_PreviousExecutionState */
    hr = X104_VFN(args, 0, HRESULT (WINAPI *)(void *, const GUID *, void **))(args, &X104_IID_IActivatedEventArgs, &activated);
    CHECK(SUCCEEDED(hr) && activated);
    value = -1;
    CHECK(SUCCEEDED(X104_VFN(activated, 7, HRESULT (WINAPI *)(void *, int *))(activated, &value)));
    g_onlaunched_state_seen = value;
    /* QI(page, IUIElement) -> Window.put_Content -> Window.Activate */
    hr = mock_outer_qi(g_mainpage_memory + 8, &X104_IID_IUIElement, &ui);
    g_page_qi_ok = SUCCEEDED(hr) && ui != NULL;
    g_window_content_seen = ui;
    hr = X104_VFN(window, 9, HRESULT (WINAPI *)(void *, void *))(window, ui);
    CHECK(hr == S_OK);
    g_activate_result = (int)X104_VFN(window, 20, HRESULT (WINAPI *)(void *))(window);
    (void)dispatcher; (void)panel;
    return g_onlaunched_result;
}
static const void *mock_overrides_vtbl[16] = { mock_outer_qi, mock_outer_addref, mock_outer_release, NULL, NULL, NULL, NULL, mock_on_launched };
static MockOuter g_overrides_object = { mock_overrides_vtbl, 1, NULL };

/* the ApplicationInitializationCallback: App2::App2 -> IApplicationFactory.CreateInstance + add_Suspending + add_Resuming */
static int g_callback_calls;
static HRESULT WINAPI mock_callback_invoke(void *self, void *sender, void *args)
{
    void *factory, *app = NULL;
    LONGLONG token = 0;
    HRESULT hr;
    (void)self; (void)sender; (void)args;
    ++g_callback_calls;
    factory = mock_activate(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationFactory);
    CHECK(factory != NULL);
    g_app_outer.vtbl = mock_outer_vtbl;
    g_app_outer.refs = 1;
    g_app_outer.overrides = &g_overrides_object;
    hr = X104_VFN(factory, 6, HRESULT (WINAPI *)(void *, void *, void **, void **))(factory, &g_app_outer, &g_inner_app, &g_instance_app);
    CHECK(SUCCEEDED(hr) && g_inner_app && g_instance_app);
    CHECK(g_app_outer.refs == 2);                       /* AddRef'd exactly once */
    hr = X104_VFN(g_inner_app, 0, HRESULT (WINAPI *)(void *, const GUID *, void **))(g_inner_app, &X104_IID_IApplication, &app);
    CHECK(SUCCEEDED(hr) && app);
    CHECK(SUCCEEDED(X104_VFN(app, 13, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(app, &g_susp_handler, &token)) && token != 0);
    CHECK(SUCCEEDED(X104_VFN(app, 15, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(app, &g_resume_handler, &token)) && token != 0);
    CHECK(SUCCEEDED(X104_VFN(app, 11, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(app, &g_resume_handler, &token)));
    return S_OK;
}
static const void *mock_callback_vtbl[4] = { mock_qi, mock_addref, mock_release, mock_callback_invoke };
static MockDelegate g_callback = { mock_callback_vtbl, 1, 0, NULL, NULL, S_OK, 0 };

/* ---- tests ---- */
static void test_uri(void)
{
    X104AppUriParts p;
    static const wchar_t full[] = L"https://user:pw@Example.com:8443/a/b.txt?x=1&y=2#frag";
    void *factory = mock_activate(L"Windows.Foundation.Uri", &X104_IID_IUriRuntimeClassFactory);
    void *uri = NULL, *combined = NULL;
    HSTRING in, out = NULL;
    static const struct { const wchar_t *base; const wchar_t *relative; const wchar_t *expected; } cases[] = {
        { L"http://a.b/c/d.txt?x=1#f", L"e.png", L"http://a.b/c/e.png" },
        { L"http://a.b/c/d.txt?x=1#f", L"/z", L"http://a.b/z" },
        { L"http://a.b/c/d.txt?x=1#f", L"?q", L"http://a.b/c/d.txt?q" },
        { L"http://a.b/c/d.txt?x=1#f", L"#g", L"http://a.b/c/d.txt?x=1#g" },
        { L"http://a.b/c/d.txt?x=1#f", L"//h/i", L"http://h/i" },
        { L"http://a.b/c/d.txt", L"https://q/r", L"https://q/r" },
        { L"ms-appx:///MainPage.xaml", L"Assets/x.png", L"ms-appx:///Assets/x.png" },
        { L"http://host", L"x", L"http://host/x" },
    };
    unsigned index;
    CHECK(factory != NULL);
    x104_app_uri_parse(full, (UINT32)lstrlenW(full), &p);
    CHECK(p.scheme_len == 5 && p.host_len == 11 && p.port == 8443 && p.path_len == 8 && p.query_len == 8 && p.frag_len == 5);
    CHECK(memcmp(full + p.host_off, L"Example.com", 22) == 0);
    CHECK(memcmp(full + p.path_off, L"/a/b.txt", 16) == 0);
    in = mock_hs(L"ms-appx:///MainPage.xaml");
    CHECK(X104_VFN(factory, 6, HRESULT (WINAPI *)(void *, HSTRING, void **))(factory, in, &uri) == S_OK && uri);
    CHECK(SUCCEEDED(X104_VFN(uri, 6, HRESULT (WINAPI *)(void *, HSTRING *))(uri, &out)) && x104_hstring_equals(out, L"ms-appx:///MainPage.xaml"));
    x104_hstring_free(out); out = NULL;
    CHECK(SUCCEEDED(X104_VFN(uri, 16, HRESULT (WINAPI *)(void *, HSTRING *))(uri, &out)) && x104_hstring_equals(out, L"ms-appx:///MainPage.xaml"));
    x104_hstring_free(out); out = NULL;
    CHECK(SUCCEEDED(X104_VFN(uri, 17, HRESULT (WINAPI *)(void *, HSTRING *))(uri, &out)) && x104_hstring_equals(out, L"ms-appx"));
    x104_hstring_free(out); out = NULL;
    CHECK(SUCCEEDED(X104_VFN(uri, 13, HRESULT (WINAPI *)(void *, HSTRING *))(uri, &out)) && x104_hstring_equals(out, L"/MainPage.xaml"));
    x104_hstring_free(out); out = NULL;
    CHECK(SUCCEEDED(X104_VFN(uri, 11, HRESULT (WINAPI *)(void *, HSTRING *))(uri, &out)) && out == NULL);      /* empty host -> NULL HSTRING */
    CHECK(SUCCEEDED(X104_VFN(uri, 22, HRESULT (WINAPI *)(void *, HSTRING, void **))(uri, in, &combined)) && combined);
    x104_hstring_free(in);
    in = mock_hs(L"");
    CHECK(X104_VFN(factory, 6, HRESULT (WINAPI *)(void *, HSTRING, void **))(factory, in, &uri) == E_INVALIDARG);
    CHECK(X104_VFN(factory, 6, HRESULT (WINAPI *)(void *, HSTRING, void **))(factory, NULL, &uri) == E_INVALIDARG);
    for (index = 0; index < ARRAYSIZE(cases); ++index) {
        HSTRING base = mock_hs(cases[index].base);
        HSTRING relative = mock_hs(cases[index].relative);
        void *result = NULL;
        HRESULT hr = X104_VFN(factory, 7, HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void **))(factory, base, relative, &result);
        HSTRING text = NULL;
        int ok = SUCCEEDED(hr) && result && SUCCEEDED(X104_VFN(result, 6, HRESULT (WINAPI *)(void *, HSTRING *))(result, &text)) && x104_hstring_equals(text, cases[index].expected);
        if (!ok) test_print("  Uri combine case %u failed: %ls + %ls (expected %ls)\n", index, cases[index].base, cases[index].relative, cases[index].expected);
        CHECK(ok);
        x104_hstring_free(text);
        x104_hstring_free(base);
        x104_hstring_free(relative);
    }
    {
        INT port = 0;
        static const wchar_t http_text[] = L"http://host/p";
        HSTRING h = mock_hs(http_text);
        void *u = NULL;
        CHECK(X104_VFN(factory, 6, HRESULT (WINAPI *)(void *, HSTRING, void **))(factory, h, &u) == S_OK);
        CHECK(SUCCEEDED(X104_VFN(u, 19, HRESULT (WINAPI *)(void *, INT *))(u, &port)) && port == 80);
        x104_hstring_free(h);
    }
}

static void test_window_messages_and_tick(void)
{
    LRESULT result = 99;
    int before_vis;
    /* nothing is delivered before f_content_set */
    x104.f_content_set = 0;
    CHECK(x104_app_window_message(x104.window, WM_SIZE, 1 /* SIZE_MINIMIZED */, 0, &result) == FALSE);
    x104_app_ui_tick();
    CHECK(g_visibility_calls == 0);
    x104.f_content_set = 1;
    before_vis = g_visibility_calls;
    x104_app_ui_tick();
    CHECK(g_visibility_calls == before_vis + 1 && g_visibility_last == 0 && g_release_pointers_calls >= 1);       /* minimised -> Visible = FALSE, pointers released first */
    x104_app_ui_tick();
    CHECK(g_visibility_calls == before_vis + 1);                                                               /* not repeated */
    CHECK(x104_app_window_message(x104.window, WM_SIZE, 0 /* SIZE_RESTORED */, 0, &result) == FALSE);
    x104_app_ui_tick();
    CHECK(g_visibility_calls == before_vis + 2 && g_visibility_last == 1);
    x104_app_window_message(x104.window, WM_SIZE, 3 /* SIZE_MAXSHOW: ignored */, 0, &result);
    x104_app_ui_tick();
    CHECK(g_visibility_calls == before_vis + 2);
    x104_app_window_message(x104.window, 0x0018 /* WM_SHOWWINDOW */, 0, 0, &result);
    x104_app_ui_tick();
    CHECK(g_visibility_calls == before_vis + 3 && g_visibility_last == 0);
    x104_app_window_message(x104.window, 0x0018, 1, 0, &result);
    x104_app_ui_tick();
    CHECK(g_visibility_calls == before_vis + 4 && g_visibility_last == 1);

    /* WM_DPICHANGED: logged loudly, nothing else */
    CHECK(x104_app_window_message(x104.window, 0x02E0, MAKELONG_FOR_TEST(120, 120), 0, &result) == FALSE);
    CHECK(log_has("WM_DPICHANGED dpi 120"));
    CHECK(g_dpi_handler.invoked == 0);
    /* WM_DISPLAYCHANGE with a new monitor size -> OrientationChanged from the tick, sender = the DisplayInformation view; same size -> nothing */
    CHECK(x104_app_window_message(x104.window, 0x007E, 32, (LPARAM)((768 << 16) | 1366), &result) == FALSE);
    CHECK(log_has("WM_DISPLAYCHANGE 1366x768 bpp 32"));
    x104_app_ui_tick();
    CHECK(g_orientation_handler.invoked == 1 && g_orientation_handler.last_args == NULL && g_orientation_handler.last_sender != NULL);
    CHECK(g_dpi_handler.invoked == 0 && g_contents_handler.invoked == 0);
    {
        int orientation = 0;
        void *sender = g_orientation_handler.last_sender;
        CHECK(SUCCEEDED(X104_VFN(sender, 6, HRESULT (WINAPI *)(void *, int *))(sender, &orientation)) && orientation == 1);     /* the handler calls slot 6 straight on the sender */
    }
    x104_app_window_message(x104.window, 0x007E, 32, (LPARAM)((768 << 16) | 1366), &result);
    x104_app_ui_tick();
    CHECK(g_orientation_handler.invoked == 1);
    /* the explicit raise helpers */
    x104_display_raise_dpi_changed();
    CHECK(g_dpi_handler.invoked == 1);
    {
        float dpi = 0;
        void *sender = g_dpi_handler.last_sender;
        CHECK(SUCCEEDED(X104_VFN(sender, 11, HRESULT (WINAPI *)(void *, float *))(sender, &dpi)) && dpi == 96.0f);            /* DpiChanged handler: slot 11 on the sender */
    }
    x104_display_raise_contents_invalidated();
    CHECK(g_contents_handler.invoked == 1);
    /* the Page follows the client size */
    g_client_w = 1000.0f; g_client_h = 600.0f;
    x104_app_ui_tick();
    CHECK(x104_app_g.page && x104_app_g.page->actual_w == 1000.0 && x104_app_g.page->actual_h == 600.0);
}

static void loop_scenario_hook(void)
{
    /* inside the UI loop: back pressed (handled), then a close request */
    CHECK(x104.f_launched == 1);
    CHECK(x104_app_raise_back_pressed() == TRUE);
    g_back_handler.put_handled = 0;
    CHECK(x104_app_raise_back_pressed() == FALSE);
    g_back_handler.put_handled = 1;
    test_window_messages_and_tick();
    {
        LRESULT result = 77;
        CHECK(x104_app_window_message(x104.window, WM_CLOSE, 0, 0, &result) == TRUE && result == 0);
        CHECK(g_queue_count == 1);                                      /* the exit proc was queued, not run inline */
        CHECK(g_quit_calls == 0);
        x104_app_request_exit(5);                                       /* second request: ignored */
        x104_app_request_exit(6);
        CHECK(g_queue_count == 1);
    }
}


/* ---- --trace-game-state: the snapshot against a fake game graph (also with unreadable pointers: it must never fault) ---- */
static void fake_class(BYTE *image, DWORD vtable_offset, const char *raw_name)
{
    DWORD locator = (DWORD)(ULONG_PTR)(image + vtable_offset + 0x100);
    DWORD descriptor = (DWORD)(ULONG_PTR)(image + vtable_offset + 0x200);
    memcpy(image + vtable_offset - 4, &locator, 4);                           /* vtable[-1] = complete object locator */
    memcpy(image + vtable_offset + 0x100 + 12, &descriptor, 4);               /* locator+12 = type descriptor */
    memcpy(image + vtable_offset + 0x200 + 8, raw_name, strlen(raw_name) + 1);            /* descriptor+8 = ".?AVName@@" */
}

static void fake_put_dword(BYTE *base, DWORD offset, DWORD value) { memcpy(base + offset, &value, 4); }
static void fake_put_float(BYTE *base, DWORD offset, float value) { memcpy(base + offset, &value, 4); }
static void fake_object(BYTE *object, BYTE *image, DWORD vtable_offset) { fake_put_dword(object, 0, (DWORD)(ULONG_PTR)(image + vtable_offset)); }
#define FAKE_PTR(p) ((DWORD)(ULONG_PTR)(p))

static int log_longest_line(void)
{
    int longest = 0, run = 0, index;
    for (index = 0; index < g_log_len; ++index) {
        if (g_log[index] == '\n') { if (run > longest) longest = run; run = 0; } else ++run;
    }
    return longest;
}

static void test_game_state_tracer(void)
{
    BYTE *image = (BYTE *)VirtualAlloc(NULL, 0x1000000u, 0x3000 /* MEM_COMMIT | MEM_RESERVE */, 0x04 /* PAGE_READWRITE */);
    static BYTE client[0x170], options[0x2d0], gui[0xc40], player[0xe70], holder[0x340], timer[0x24], screen[16], gamemode[16], platform[0x88], dr[0x80], area[0x20], cell[8];
    BYTE *saved_image = x104.image;
    DWORD saved_client_ptr = 0;
    DWORD client_ptr = FAKE_PTR(client);
    int longest;
    CHECK(image != NULL);
    if (!image) return;
    memcpy(&saved_client_ptr, g_appmain_memory + X104_APPMAIN_CLIENT, 4);
    fake_put_dword(image, 0x3c, 0x80);
    fake_put_dword(image, 0x80 + 0x50, 0x1000000u);
    fake_class(image, 0x20000, ".?AVProgressScreen@@");
    fake_class(image, 0x21000, ".?AVTouchInputHolder@@");
    fake_class(image, 0x22000, ".?AVStartMenuScreen@Touch@@");
    fake_class(image, 0x23000, ".?AVLocalPlayer@@");
    fake_class(image, 0x24000, ".?AVGui@@");

    x104.image = image;
    /* the game graph: AppMain+0x34 = client, AppMain+0x38 = platform cell */
    memset(client, 0, sizeof(client)); memset(options, 0, sizeof(options)); memset(gui, 0, sizeof(gui)); memset(player, 0, sizeof(player)); memset(holder, 0, sizeof(holder));
    memset(timer, 0, sizeof(timer)); memset(platform, 0, sizeof(platform)); memset(dr, 0, sizeof(dr)); memset(area, 0, sizeof(area));
    fake_object(screen, image, 0x20000);
    fake_object(holder, image, 0x21000);
    fake_object(player, image, 0x23000);
    fake_object(gui, image, 0x24000);
    fake_object(gamemode, image, 0x22000);
    fake_put_dword(g_appmain_memory, X104_APPMAIN_CLIENT, client_ptr);
    fake_put_dword(g_appmain_memory, 0x38, FAKE_PTR(cell));
    fake_put_dword(cell, 0, FAKE_PTR(platform));
    fake_put_dword(client, 0x08, 1); client[0x13C] = 1; fake_put_dword(client, 0x20, 1280); fake_put_dword(client, 0x24, 720);
    fake_put_dword(client, 0x6C, FAKE_PTR(screen)); fake_put_dword(client, 0x68, 0x1000); fake_put_dword(client, 0x40, FAKE_PTR(gamemode));
    fake_put_dword(client, 0x12C, FAKE_PTR(player)); fake_put_dword(client, 0x124, FAKE_PTR(holder)); fake_put_dword(client, 0x130, FAKE_PTR(gui));
    fake_put_dword(client, 0x134, FAKE_PTR(options)); fake_put_dword(client, 0xA8, FAKE_PTR(timer));
    fake_put_float(timer, 0, 20.0f); fake_put_dword(timer, 4, 1); fake_put_float(timer, 8, 0.5f);
    fake_put_dword(gui, 0xAC4, client_ptr); fake_put_dword(gui, 0xB0C, 4);
    options[0x276] = 1; options[0x1A] = 1; fake_put_float(options, 0x28C, 3.0f);
    fake_put_dword(player, 0xDB4, 1); fake_put_float(player, 0xE0C, 0.25f); fake_put_dword(player, 0xDB8, client_ptr); fake_put_dword(player, 0x138, 20);
    fake_put_float(platform, 0x38, 3.779f); fake_put_float(platform, 0x3C, 1.0f); fake_put_float(platform, 0x40, 1.0f); fake_put_dword(platform, 0x30, 1280); fake_put_dword(platform, 0x34, 720);
    fake_put_float(dr, 0x20, 1280.0f); fake_put_float(dr, 0x24, 720.0f); fake_put_float(dr, 0x2C, 1.0f);
    fake_put_dword(image, ARM0104_FRAME_RENDER_DEVICE_RESOURCES_GLOBAL_INSTANCE, FAKE_PTR(dr));
    fake_put_float(image, 0xDDE814, 4.0f); fake_put_float(image, ARM0104_CLIENT_INIT_GLOBALS_INVGUISCALE, 0.25f);
    fake_put_dword(holder, 0x10 + 0xF4, FAKE_PTR(area));
    fake_put_float(area, 0x08, 10.0f); fake_put_float(area, 0x0C, 20.0f); fake_put_float(area, 0x10, 30.5f); fake_put_float(area, 0x14, 40.0f);

    g_log_len = 0; g_log[0] = 0;
    x104_app_gs_snapshot("test", GetTickCount());
    CHECK(log_has("X104 gstate #1 [test]"));
    CHECK(log_has("screen(+0x6C)=") && log_has("ProgressScreen"));
    CHECK(log_has("TouchInputHolder") && log_has("StartMenuScreen@Touch") && log_has("LocalPlayer") && log_has(" Gui "));
    CHECK(log_has("hideGui(+0x244)=0") && log_has("useTouchscreen(+0x276)=1") && log_has("pixelDensity(+0x28C)=3000m"));
    CHECK(log_has("loadState(+0xDB4)=1") && log_has("progress(+0xE0C)=250m") && log_has("input(+0xD8C)=00000000 (null)"));
    CHECK(log_has("size=1280x720") && log_has("tps=20000m") && log_has("guiScale=4000m invGuiScale=250m"));
    CHECK(log_has("ppmm(+0x38)=3779m") && log_has("viewport(+0x18)=0m,0m 1280000m x 720000m z=0m..1000m"));
    CHECK(log_has(":10000,20000,30500,40000"));
    longest = log_longest_line();
    CHECK(longest > 100 && longest < 1000);

    /* the periodic / change schedule */
    g_log_len = 0; g_log[0] = 0;
    x104.f_units_ready = 1;
    x104.f_exit_requested = 0;
    x104_app_gs.enabled = TRUE;
    x104_app_gs_tick();
    CHECK(log_has("--trace-game-state active") && log_has("[periodic]"));
    fake_put_dword(player, 0xDB4, 3); fake_put_dword(client, 0x6C, 0);
    x104_app_gs_tick();
    CHECK(log_has("[change]") && log_has("loadState(+0xDB4)=3") && log_has("screen(+0x6C)=00000000 (null)"));

    /* garbage everywhere: unreadable pointers and wild floats must not fault and are marked */
    g_log_len = 0; g_log[0] = 0;
    fake_put_dword(client, 0x6C, 0xDEADBEEFu); fake_put_dword(client, 0x12C, 0xDEAD0000u); fake_put_dword(client, 0x134, 0xCCCCCCCCu); fake_put_dword(client, 0x130, 0xFFFFFFFFu);
    fake_put_dword(client, 0x124, 0x00000010u); fake_put_dword(client, 0xA8, 0x7FFFFFF0u); fake_put_dword(dr, 0x20, 0x7F800000u); fake_put_dword(dr, 0x24, 0x7FC00000u);
    fake_put_dword(image, ARM0104_FRAME_RENDER_DEVICE_RESOURCES_GLOBAL_INSTANCE, 0x13579BDFu);
    x104_app_gs_snapshot("garbage", GetTickCount());
    CHECK(log_has("[garbage]") && log_has("(unreadable)"));
    fake_put_dword(image, ARM0104_FRAME_RENDER_DEVICE_RESOURCES_GLOBAL_INSTANCE, FAKE_PTR(dr));
    x104_app_gs_snapshot("garbage2", GetTickCount());
    CHECK(log_has("-999998m") || log_has("-999999m"));
    longest = log_longest_line();
    CHECK(longest < 1000);

    /* no client yet: silent */
    fake_put_dword(g_appmain_memory, X104_APPMAIN_CLIENT, 0);
    g_log_len = 0; g_log[0] = 0;
    x104_app_gs_snapshot("none", GetTickCount());
    CHECK(g_log_len == 0);
    x104_app_gs.enabled = FALSE;

    fake_put_dword(g_appmain_memory, X104_APPMAIN_CLIENT, saved_client_ptr);
    x104.image = saved_image;
    VirtualFree(image, 0, 0x8000 /* MEM_RELEASE */);
}

int main(void)
{
    void *statics;
    HRESULT hr;
    void *hw_statics;
    LONGLONG token;
    void *bad = NULL;
    int unit_tests_ok;

    test_print("== app unit test ==\n");
    x104.image = (BYTE *)0x400000;
    x104.ui_thread_id = GetCurrentThreadId();
    x104.main_thread_id = x104.ui_thread_id;
    x104.window = CreateWindowExA(0, "STATIC", "app test", 0x80000000u /* WS_POPUP */, 0, 0, 320, 200, NULL, NULL, NULL, NULL);
    CHECK(x104.window != NULL);
    g_mock_panel = x104_object_new(&mock_element_class, sizeof(X104Object));
    g_mock_logo = x104_object_new(&mock_element_class, sizeof(X104Object));
    g_mock_editbox = x104_object_new(&mock_element_class, sizeof(X104Object));
    g_mock_button = x104_object_new(&mock_element_class, sizeof(X104Object));
    g_mock_core_window = x104_object_new(&mock_cw_class, sizeof(X104Object));

    /* contract: zero-safe before init */
    x104_app_ui_tick();
    CHECK(x104_app_window_message(x104.window, 0x007E, 0, 0, NULL) == FALSE);
    CHECK(x104_app_install(NULL) == TRUE);
    CHECK(x104_app_init(x104.window) == TRUE);
    CHECK(x104_app_activate(L"Windows.Foo.Bar", &X104_IID_IApplicationStatics, &bad) == REGDB_E_CLASSNOTREG);
    CHECK(x104_app_raise_back_pressed() == FALSE);                       /* no handler yet: FALSE, no crash */
    CHECK(log_has("no handler is registered"));

    /* the eight classes of the table activate with the IIDs the game asks for */
    {
        static const struct { const wchar_t *name; const GUID *iid; } t[] = {
            { L"Windows.UI.Xaml.Application", &X104_IID_IApplicationStatics }, { L"Windows.UI.Xaml.Application", &X104_IID_IApplicationFactory },
            { L"Windows.UI.Xaml.Controls.Page", &X104_IID_IPageFactory }, { L"Windows.UI.Xaml.Window", &X104_IID_IWindowStatics },
            { L"Windows.UI.Xaml.Controls.Canvas", &X104_IID_ICanvasStatics }, { L"Windows.UI.ViewManagement.StatusBar", &X104_IID_IStatusBarStatics },
            { L"Windows.Phone.UI.Input.HardwareButtons", &X104_IID_IHardwareButtonsStatics }, { L"Windows.Graphics.Display.DisplayInformation", &X104_IID_IDisplayInformationStatics },
            { L"Windows.Foundation.Uri", &X104_IID_IUriRuntimeClassFactory } };
        unsigned i;
        for (i = 0; i < ARRAYSIZE(t); ++i) {
            void *p = NULL;
            hr = x104_app_activate(t[i].name, t[i].iid, &p);
            CHECK(hr == S_OK && p != NULL);
        }
    }
    /* wrong IID on a known class -> E_NOINTERFACE */
    CHECK(x104_app_activate(L"Windows.UI.Xaml.Window", &X104_IID_IApplicationStatics, &bad) == E_NOINTERFACE);

    test_uri();

    /* Start() with the mock game */
    statics = mock_activate(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationStatics);
    CHECK(statics != NULL);
    {
        void *current = (void *)1;
        CHECK(X104_VFN(statics, 6, HRESULT (WINAPI *)(void *, void **))(statics, &current) == E_FAIL && current == NULL);     /* get_Current before creation */
    }
    g_mock_panel->user[0] = NULL;
    g_loop_hook = loop_scenario_hook;
    /* the game is "not ready" (AppMain+0x3C == 0): the MainPage memory the mock page factory fills is wired after CreateInstance, so wire AppMain now through the outer */
    hr = X104_VFN(statics, 7, HRESULT (WINAPI *)(void *, void *))(statics, &g_callback);
    CHECK(hr == S_OK);
    CHECK(g_callback_calls == 1);
    CHECK(g_onlaunched_calls == 1);
    CHECK(g_overrides_args != NULL);
    {
        /* OnLaunched received the ILaunchActivatedEventArgs view (views[0]), and it answers IActivatedEventArgs */
        X104View *view = (X104View *)g_overrides_args;
        CHECK(view->desc != NULL && memcmp(view->desc->iid, &X104_IID_ILaunchActivatedEventArgs, sizeof(GUID)) == 0);
    }
    CHECK(g_onlaunched_state_seen == 0);
    CHECK(g_page_qi_ok && g_findname_ok);
    CHECK(g_activate_result == (int)S_OK);
    CHECK(x104.f_mainpage_created == 1 && x104.f_content_set == 1 && x104.f_launched == 1);
    CHECK(x104.mainpage_outer == g_mainpage_memory + 8);
    CHECK(g_inner_page == (void *)&((X104Object *)x104_app_g.page)->views[0]);                 /* inner = identity view of the page */
    CHECK(((X104View *)g_instance_page)->desc->iid == &X104_IID_IPage);                         /* instance = the IPage view */
    CHECK(((X104View *)g_instance_app)->desc->iid == &X104_IID_IApplication);
    CHECK(g_inner_app == g_instance_app);                                                       /* views[0] is IApplication for the Application */
    CHECK(g_window_content_seen != NULL && ((X104View *)g_window_content_seen)->obj == (X104Object *)x104_app_g.page);
    CHECK(g_loop_calls == 1);
    CHECK(g_susp_handler.invoked == 0);                                     /* AppMain does not exist in the mock memory: the game is NOT ready -> Suspending skipped */
    CHECK(log_has("exit before ready: Suspending skipped (AppMain+0x3C == 0)"));
    CHECK(g_shutdown_calls == 1 && g_quit_calls == 1 && g_quit_code == 0);
    CHECK(x104.f_exit_requested == 1);
    CHECK(log_has("Start: UI loop returned 0"));
    CHECK(log_has("request_exit(6) ignored"));
    CHECK(g_back_handler.invoked == 2);
    CHECK(g_back_handler.last_sender == NULL);
    CHECK(log_has("BackPressed handled=1") && log_has("BackPressed handled=0"));
    CHECK(!IsWindowVisible(x104.window));                                   /* the exit proc hid the window */
    {
        void *current = NULL;
        CHECK(X104_VFN(statics, 6, HRESULT (WINAPI *)(void *, void **))(statics, &current) == S_OK && current == g_instance_app);
    }
    /* a second Start is refused */
    CHECK(X104_VFN(statics, 7, HRESULT (WINAPI *)(void *, void *))(statics, &g_callback) == X104_E_ILLEGAL_METHOD_CALL);

    /* the exit path with a READY game: Suspending is raised, with a complete args -> operation -> deferral chain */
    {
        BYTE *appmain = g_appmain_memory;
        DWORD pointer = (DWORD)(ULONG_PTR)appmain;
        memcpy(g_mainpage_memory + X104_MAINPAGE_APPMAIN, &pointer, 4);
        appmain[X104_APPMAIN_READY] = 1;
        CHECK(x104_game_is_ready());
        CHECK(!x104_app_render_loop_exists());                              /* AppMain+0xC == NULL: StartRenderLoop never ran */
        /* ready but no render loop yet: VisibilityChanged is HELD (StopRenderLoop would dereference NULL under the AppMain lock) and retried every pass */
        {
            LRESULT result = 0;
            int calls = g_visibility_calls;
            x104.f_exit_requested = 0;
            x104_app_window_message(x104.window, WM_SIZE, 1 /* SIZE_MINIMIZED */, 0, &result);
            x104_app_ui_tick();
            x104_app_ui_tick();
            CHECK(g_visibility_calls == calls);
            CHECK(log_has("held back: game ready but its render loop does not exist yet"));
            {
                DWORD action = 0x12345678u;                                 /* StartRenderLoop stored its IAsyncAction */
                memcpy(appmain + X104_APP_APPMAIN_RENDER_ACTION, &action, 4);
            }
            CHECK(x104_app_render_loop_exists());
            x104_app_ui_tick();
            CHECK(g_visibility_calls == calls + 1 && g_visibility_last == 0);
            x104_app_window_message(x104.window, WM_SIZE, 0 /* SIZE_RESTORED */, 0, &result);
            x104_app_ui_tick();
            CHECK(g_visibility_calls == calls + 2 && g_visibility_last == 1);
            memset(appmain + X104_APP_APPMAIN_RENDER_ACTION, 0, 4);         /* back to "never started" for the exit checks */
        }
        /* ready but no render loop: Suspending is SKIPPED (the handler would call StopRenderLoop -> NULL dereference under the lock) */
        x104_app_g.exit_proc_ran = 0;
        x104.f_exit_requested = 0;
        g_queue_closed = FALSE;
        g_queue_count = 0;
        g_shutdown_calls = 0;
        g_quit_calls = 0;
        x104_app_request_exit(0);
        CHECK(g_queue_count == 1);
        x104_ui_pump_once();
        CHECK(g_susp_handler.invoked == 0);
        CHECK(log_has("Suspending skipped: render loop never started"));
        CHECK(g_shutdown_calls == 1 && g_quit_calls == 1 && g_quit_code == 0);
        /* with the render loop running: Suspending is raised */
        {
            DWORD action = 0x12345678u;
            memcpy(appmain + X104_APP_APPMAIN_RENDER_ACTION, &action, 4);
        }
        x104_app_g.exit_proc_ran = 0;
        x104.f_exit_requested = 0;
        g_queue_closed = FALSE;
        g_queue_count = 0;
        g_shutdown_calls = 0;
        g_quit_calls = 0;
        x104_app_request_exit(0);
        CHECK(g_queue_count == 1);
        x104_ui_pump_once();
        CHECK(g_susp_handler.invoked == 1 && g_susp_chain_ok);
        CHECK(g_susp_handler.last_sender == g_inner_app);                   /* sender = the IApplication view */
        CHECK(g_shutdown_calls == 1 && g_quit_calls == 1 && g_quit_code == 0);
        CHECK(log_has("Suspending raised to 1 handler(s)"));
        /* the queue is closed now (x104_ui_quit ran): a new request on the UI thread runs the exit proc inline - but it is idempotent on f_exit_requested */
        x104_app_request_exit(9);
        CHECK(g_quit_calls == 1);
    }
    /* the queue-closed fallback: exit proc inline on the UI thread */
    x104_app_g.exit_proc_ran = 0;
    x104.f_exit_requested = 0;
    g_susp_handler.invoked = 0;
    g_quit_calls = 0;
    x104_app_request_exit(3);                                              /* g_queue_closed is TRUE: post refused -> inline */
    CHECK(log_has("the UI queue is closed: running the exit proc inline"));
    CHECK(g_susp_handler.invoked == 1 && g_quit_calls == 1 && g_quit_code == 3);

    /* HardwareButtons remove, Application.Exit, Window.Close and the events with 64-bit tokens by value */
    hw_statics = mock_activate(L"Windows.Phone.UI.Input.HardwareButtons", &X104_IID_IHardwareButtonsStatics);
    token = 0;
    CHECK(SUCCEEDED(X104_VFN(hw_statics, 6, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(hw_statics, &g_resume_handler, &token)) && token != 0);
    CHECK(X104_VFN(hw_statics, 7, HRESULT (WINAPI *)(void *, LONGLONG))(hw_statics, token) == S_OK);
    {
        long refs_before = g_resume_handler.refs;
        CHECK(x104_event_count(&x104_app_g.back_pressed) == 1);            /* only the game's own handler is left */
        (void)refs_before;
    }
    x104_app_g.exit_proc_ran = 0;
    x104.f_exit_requested = 0;
    g_queue_closed = FALSE;
    g_queue_count = 0;
    CHECK(X104_VFN(g_instance_app, 17, HRESULT (WINAPI *)(void *))(g_instance_app) == S_OK);       /* Application.Exit */
    CHECK(g_queue_count == 1);
    g_queue_count = 0;

    /* Start failure paths are covered by reading the code; the unit shuts down cleanly and twice */
    x104_app_shutdown();
    x104_app_shutdown();
    x104_app_ui_tick();                                                    /* no-op after shutdown */
    test_game_state_tracer();
    unit_tests_ok = (g_failures == 0);
    test_print("== app unit test: %d checks, %d failures %s ==\n", g_checks, g_failures, unit_tests_ok ? "OK" : "FAILED");
    if (!unit_tests_ok) {
        g_verbose = 0;
        test_print("---- log ----\n%s\n", g_log);
    }
    return unit_tests_ok ? 0 : 1;
}
