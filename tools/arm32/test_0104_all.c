/* Host-side (x86, runs on the PC) INTEGRATION test of the whole 0.10.4 host: the REAL core + async + app + panel + platform + input + selftest units in ONE
 * translation unit, exactly as win32_host_arm32_0104.inl includes them (only the boot file is replaced by a few lines below, because it needs the whole win32_host.c).
 *
 * A mock "game" plays App2 / MainPage / DeviceResources / AppMain / HID: it makes the SAME calls through the SAME vtable slots in the SAME order as the table 2a of
 * facts/startup_flow.md (activation router -> Application::Start -> App2 aggregation -> OnLaunched -> StatusBar -> MainPage ctor (Page aggregation, FindName x4, every
 * event registration, input work item, Canvas, Package/ApplicationData/Memory, init job, DispatcherTimer) -> Window.Content/Activate -> UI loop -> first SizeChanged ->
 * ready Tick -> AttachToPanel/HID::Initialize -> StartRenderLoop), then a scripted tester thread drives the real window procedure (mouse, keys, characters, resize,
 * minimise/restore, WM_CLOSE) and the mock handlers read every argument the way the game does (straight on the pointer they receive, R10).
 * What it proves: pointer/view agreement between the units, thread/ordering/flag agreement (R1, R2, R3, R12, R16, R17), no unit stub left behind.
 * What it cannot prove: ARM register passing (X104_SLOT + --selftest-fakes), the real game's own code, D3D/DXGI (no device, no swap chain is created here).
 * Build + run:  python tools/arm32/test_0104_core.py --src tools/arm32/test_0104_all.c      Exit code 0 = every check passed. */
#include "win32_compat.h"

extern void *WINAPI GetStdHandle(DWORD);
extern char *strstr(const char *, const char *);
extern int strcmp(const char *, const char *);
extern int WINAPI wvsprintfA(LPSTR, LPCSTR, __builtin_va_list);
extern void WINAPI RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR *);
extern LRESULT WINAPI SendMessageW(HWND, UINT, WPARAM, LPARAM);
extern BOOL WINAPI TerminateProcess(HANDLE, UINT);
extern BOOL WINAPI SetCurrentDirectoryW(LPCWSTR);
extern int WINAPI lstrcmpiW(LPCWSTR, LPCWSTR);
extern DWORD WINAPI GetEnvironmentVariableW(LPCWSTR, LPWSTR, DWORD);

static int g_failures;
static int g_checks;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; test_print("CHECK FAILED line %d: %s\n", __LINE__, #cond); } } while (0)
#define CHECK_MSG(cond, text) do { ++g_checks; if (!(cond)) { ++g_failures; test_print("CHECK FAILED line %d: %s  [%s]\n", __LINE__, #cond, text); } } while (0)

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

/* ---- the log: everything is kept (searchable), printed on request ---- */
static char g_log[1500000];
static volatile LONG g_log_len;
static volatile LONG g_log_lock;
static int g_verbose;
static void log_line(const char *text)
{
    int length = (int)strlen(text);
    while (__atomic_exchange_n(&g_log_lock, 1, __ATOMIC_ACQUIRE)) Sleep(0);
    if (g_log_len + length + 2 < (int)sizeof(g_log)) {
        memcpy(g_log + g_log_len, text, (size_t)length);
        g_log_len += length;
        g_log[g_log_len++] = '\n';
        g_log[g_log_len] = 0;
    }
    __atomic_store_n(&g_log_lock, 0, __ATOMIC_RELEASE);
    if (g_verbose) test_print("  [log] %s\n", text);
}
static int log_has(const char *needle) { return strstr(g_log, needle) != NULL; }
static int log_count(const char *needle)
{
    int count = 0;
    const char *at = g_log;
    while ((at = strstr(at, needle)) != NULL) { ++count; at += strlen(needle); }
    return count;
}
static void log_hresult(const char *operation, HRESULT result) { (void)operation; (void)result; }
extern HRESULT WINAPI WindowsDeleteString(HSTRING);
extern BOOL WINAPI WindowsIsStringEmpty(HSTRING);

/* ---- stand-ins for win32_host.c (the union of what the six harnesses of the units use) ---- */
#define ARM32_SRC_MOUSEKB 1
#define ARM32_SRC_TOUCH 2
#define ARM32_INPUT_AUTO 0
#define ARM32_INPUT_TOUCH 1
#define ARM32_INPUT_MOUSE 2
#define ARM32_TOUCHAPI_AUTO 0
#define ARM32_TOUCHAPI_POINTER 1
#define ARM32_TOUCHAPI_WMTOUCH 2
#define ARM32_TOUCHAPI_LEGACY 3
#define ARM32_TOUCHEVENTF_MOVE 0x0001u
#define ARM32_TOUCHEVENTF_DOWN 0x0002u
#define ARM32_TOUCHEVENTF_UP 0x0004u
#define ARM32_TOUCHEVENTF_PALM 0x0080u
static BOOL arm32_check_bytes(const BYTE *address, const BYTE *expected, SIZE_T size, const char *what) { (void)what; return memcmp(address, expected, size) == 0; }
static BOOL arm32_patch_cell(void **cell, const void *expected, void *replacement, void **original_out, const char *what)
{
    (void)what;
    if (expected && *cell != expected) return FALSE;
    if (original_out) *original_out = *cell;
    *cell = replacement;
    return TRUE;
}
static BOOL arm32_patch_byte(BYTE *address, BYTE expected, BYTE replacement, const char *what) { (void)what; if (*address != expected) return FALSE; *address = replacement; return TRUE; }
static BOOL arm32_iat_hook_import(BYTE *image, BOOL delay, const char *name, DWORD rva, void *hook, void **original, const char *what)
{ (void)image; (void)delay; (void)name; (void)rva; (void)hook; (void)original; (void)what; return FALSE; }
static void arm32_iat_describe_address(const void *address, char *out, SIZE_T out_size)
{ (void)address; if (out && out_size) lstrcpynA(out, "(test stub)", (int)out_size); }
static UINT arm32_feature_level_cap = 0x9100u;
static int g_osk_show, g_osk_hide, g_osk_poll, g_osk_pointer_up, g_note_touch, g_note_mouse;
static void arm32_osk_show(const char *why) { (void)why; ++g_osk_show; }
static void arm32_osk_hide(const char *why) { (void)why; ++g_osk_hide; }
static void arm32_osk_poll(void) { ++g_osk_poll; }
static BOOL arm32_osk_rect_buffer(float rect[4]) { (void)rect; return FALSE; }
static void arm32_osk_note_pointer_up(void) { ++g_osk_pointer_up; }
static void arm32_input_note_source(int source, const char *why) { (void)why; if (source == ARM32_SRC_TOUCH) ++g_note_touch; else ++g_note_mouse; }
static BOOL arm32_input_touch_active(void) { return FALSE; }
static BOOL arm32_osk_visible_now(void) { return FALSE; }
static BOOL text_input_active;
static volatile LONG arm32_boot_frames_started, arm32_boot_frames_done, arm32_boot_progress, arm32_boot_loop_passes;
static DWORD arm32_boot_start_tick;
static volatile LONG g_note_frame_calls;
static void arm32_boot_note_frame(LONG frame, DWORD loop_start, DWORD frame_ms) { (void)frame; (void)loop_start; (void)frame_ms; ++g_note_frame_calls; }
static int arm32_input_policy = ARM32_INPUT_AUTO;
static int arm32_osk_policy;
static int arm32_touch_api_effective;
static BOOL arm32_wm_touch_registered;
static BOOL arm32_input_verbose;
static const char *const arm32_input_policy_names[] = { "auto", "touch", "mouse" };
static const char *const arm32_touch_api_names[] = { "auto", "pointer", "wmtouch", "legacy" };
static const char *const arm32_osk_policy_names[] = { "auto", "on", "off" };
typedef struct Arm32TouchInput { LONG x, y; HANDLE source; DWORD id, flags, mask, time; ULONG_PTR extra; DWORD contact_x, contact_y; } Arm32TouchInput;
typedef BOOL (WINAPI *Arm32GetTouchInputInfoFn)(HANDLE, UINT, void *, int);
typedef BOOL (WINAPI *Arm32CloseTouchInputHandleFn)(HANDLE);
typedef struct Arm32InputApi { Arm32GetTouchInputInfoFn get_touch_input_info; Arm32CloseTouchInputHandleFn close_touch_input_handle; } Arm32InputApi;
static Arm32InputApi arm32_api;
static void arm32_input_attach_window(HWND window) { (void)window; arm32_touch_api_effective = ARM32_TOUCHAPI_POINTER; }
static BOOL arm32_pointer_is_touch(UINT32 id, UINT32 *type_out) { if (type_out) *type_out = id < 100 ? 2u : 4u; return id < 100; }
static BOOL arm32_pointer_nontouch_known(UINT32 id) { return id >= 100; }
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
static const wchar_t *arm32_iat_argument_value(const wchar_t *argument, const wchar_t *prefix)
{
    while (*prefix) {
        if (host_ascii_lower_w(*argument) != host_ascii_lower_w(*prefix)) return NULL;
        ++argument;
        ++prefix;
    }
    return argument;
}
HRESULT WINAPI SHGetFolderPathW(HWND window, int folder, HANDLE token, DWORD flags, LPWSTR path)
{
    (void)window; (void)folder; (void)token; (void)flags;
    path[0] = 0;
    return E_FAIL;
}

/* ---- the real tree, in the order of win32_host_arm32_0104.inl (boot excluded) ---- */
#include "win32_host_arm32_0104_core.inl"
#include "win32_host_arm32_0104_async.inl"
#include "win32_host_arm32_0104_app.inl"
#include "win32_host_arm32_0104_panel.inl"
#include "win32_host_arm32_0104_platform.inl"
#include "win32_host_arm32_0104_input.inl"
#include "win32_host_arm32_0104_selftest.inl"

/* ---- the window procedure: a transcription of the routing of x104_window_proc (boot.inl section 7, "Window procedure routing") ---- */
static volatile LONG g_modal_passes;
static LRESULT CALLBACK harness_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    BOOL handled = FALSE;
    BOOL part;
    switch (message) {
    case X104_WM_QUIT:
        x104.exit_code = (int)wparam;
        InterlockedExchange(&x104.f_loop_exit, 1);
        PostQuitMessage((int)wparam);
        return 0;
    case X104_WM_WAKE:
        return 0;
    case WM_TIMER:
        if (wparam == X104_TIMER_PUMP) { ++g_modal_passes; x104_ui_pass(); return 0; }
        break;
    case WM_ERASEBKGND:
        return 1;
    case 0x0112u:
        if ((wparam & 0xFFF0u) == 0xF100u) return 0;
        break;
    default:
        break;
    }
    if (!x104.f_units_ready) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case 0x0005u: case 0x0006u: case 0x001Cu: case 0x0018u: case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENTERSIZEMOVE: case WM_EXITSIZEMOVE: case WM_WINDOWPOSCHANGED:
    case 0x007Eu: case 0x02E0u: {
        LRESULT part_result = 0;
        part = x104_panel_window_message(window, message, wparam, lparam, &part_result);
        if (part && !handled) { handled = TRUE; result = part_result; }
        part_result = 0;
        part = x104_input_window_message(window, message, wparam, lparam, &part_result);
        if (part && !handled) { handled = TRUE; result = part_result; }
        part_result = 0;
        part = x104_app_window_message(window, message, wparam, lparam, &part_result);
        if (part && !handled) { handled = TRUE; result = part_result; }
        if (handled) return result;
        break;
    }
    default:
        if (x104_input_window_message(window, message, wparam, lparam, &result)) return result;
        result = 0;
        if (x104_panel_window_message(window, message, wparam, lparam, &result)) return result;
        result = 0;
        if (x104_app_window_message(window, message, wparam, lparam, &result)) return result;
        result = 0;
        break;
    }
    switch (message) {
    case WM_CLOSE: x104_app_request_exit(0); return 0;
    case 0x0104u: case 0x0105u: case 0x0106u: return 0;
    default: return DefWindowProcW(window, message, wparam, lparam);
    }
}

/* ======================================================================================================================
 * the mock game
 * ====================================================================================================================== */
#define G(object, iface, method)  X104_VFN((object), X104_SLOT_##iface##_##method, X104_FN_##iface##_##method)

typedef struct MockDelegate MockDelegate;
typedef HRESULT (*MockHandlerFn)(MockDelegate *self, void *a, void *b);
struct MockDelegate { const void **vtbl; volatile LONG refs; volatile LONG invoked; MockHandlerFn fn; };
static HRESULT WINAPI mock_qi(void *self, const GUID *iid, void **out) { (void)iid; *out = self; InterlockedIncrement(&((MockDelegate *)self)->refs); return S_OK; }
static ULONG WINAPI mock_addref(void *self) { return (ULONG)InterlockedIncrement(&((MockDelegate *)self)->refs); }
static ULONG WINAPI mock_release(void *self) { return (ULONG)InterlockedDecrement(&((MockDelegate *)self)->refs); }
static HRESULT WINAPI mock_invoke(void *self, void *a, void *b)
{
    MockDelegate *d = (MockDelegate *)self;
    InterlockedIncrement(&d->invoked);
    return d->fn ? d->fn(d, a, b) : S_OK;
}
static const void *mock_vtbl[4] = { mock_qi, mock_addref, mock_release, mock_invoke };
#define MOCK(name, handler) static MockDelegate name = { mock_vtbl, 1, 0, handler }

static BOOL wcscmp_ok(const wchar_t *a, const wchar_t *b) { return lstrcmpW(a, b) == 0; }
#ifndef MAKELONG
#define MAKELONG(lo, hi) ((LONG)(((WORD)((ULONG_PTR)(lo) & 0xffff)) | ((DWORD)((WORD)((ULONG_PTR)(hi) & 0xffff))) << 16))
#endif
static HSTRING mock_hs(const wchar_t *text) { HSTRING h = NULL; x104_hstring_create(text, &h); return h; }

/* the game's activation helper (sub_2001E4): through the REAL router, as the IAT hook would */
static void *mock_activate(const wchar_t *class_name, const GUID *iid)
{
    void *out = NULL;
    HRESULT hr = x104_activation_hook(class_name, iid, &out);
    if (FAILED(hr) || !out) test_print("  ACTIVATION of %ls FAILED 0x%08lx\n", class_name, (unsigned long)hr);
    return SUCCEEDED(hr) ? out : NULL;
}
static void *mock_qi_of(void *object, const GUID *iid)
{
    void *out = NULL;
    if (!object || FAILED(x104_raw_qi(object, iid, &out))) return NULL;
    return out;
}

/* game objects: MainPage (0x94 bytes, outer at +8), AppMain (0x40), MinecraftClient (0x168) */
typedef struct MockOuter { const void **vtbl; volatile LONG refs; void *overrides; } MockOuter;
static MockOuter g_app_outer;
static BYTE g_mainpage[0x100] __attribute__((aligned(16)));
static BYTE g_appmain[0x80] __attribute__((aligned(16)));
static BYTE g_client[0x170] __attribute__((aligned(16)));
static void *g_inner_app, *g_instance_app, *g_inner_page, *g_instance_page;
static X104Lock g_game_lock;           /* AppMain+0x10 critical_section: non-recursive */
#define APPMAIN_RENDER_ACTION(app)  (*(void *volatile *)((app) + 0x0C))
#define APPMAIN_READY(app)          (*(volatile BYTE *)((app) + 0x3C))

/* game-visible state recorded by the mock handlers (the checks read these) */
static void *g_panel, *g_panel_fe, *g_panel_scp, *g_panel_ui, *g_panel_dep, *g_logo_ui, *g_editbox, *g_editbox_ui, *g_editbox_ctl, *g_editbox_tb, *g_button, *g_button_ctl;
static void *g_core_window, *g_timer, *g_window;
static volatile LONG g_seq;
static volatile LONG g_seq_launch, g_seq_content, g_seq_size1, g_seq_tick_first, g_seq_tick_ready, g_seq_attach, g_seq_render;
static volatile LONG g_ticks, g_tick_gate_violation, g_ready_ticks;
static volatile LONG g_size_events, g_last_size_w, g_last_size_h, g_first_size_w, g_first_size_h, g_native_calls;
static volatile LONG g_frames, g_render_starts, g_render_threads_running, g_render_stopped_frames;
static volatile LONG g_pressed, g_released, g_moved, g_press_x, g_press_y, g_release_x, g_release_y, g_press_id, g_press_device, g_release_left, g_press_left, g_pressed_src2, g_pressed_src1, g_pressed_cw;
static volatile LONG g_keydown, g_keyup, g_key_vk, g_key_repeat;
static volatile LONG g_vis_events, g_vis_last, g_vis_unsafe;
static volatile LONG g_text_events, g_back_events, g_pane_showing, g_pane_hiding, g_pane_height;
static int g_button_created, g_button_visible, g_button_top_right, g_button_hidden_without_world;
extern BOOL WINAPI GetWindowRect(HWND, RECT *);
static volatile LONG g_susp_events, g_susp_chain_ok, g_onlaunched_calls, g_page_qi_ok, g_findname_ok, g_prev_state;
static volatile LONG g_input_items_done, g_workitem_threads;
static volatile LONG g_ctor_thread, g_launch_thread;
static wchar_t g_text_seen[64];
static int g_error_count;
#define SEQ(name) do { name = InterlockedIncrement(&g_seq); } while (0)
static volatile LONG g_wrong_thread;
/* R1/threading model section 3: every game delegate runs on the UI thread */
#define UI_ONLY() do { if (GetCurrentThreadId() != x104.ui_thread_id) InterlockedIncrement(&g_wrong_thread); } while (0)

/* ---- delegates ---- */
static HRESULT mock_resume(MockDelegate *d, void *a, void *b) { (void)d; (void)a; (void)b; return S_OK; }
MOCK(g_h_resume, mock_resume);
static HRESULT mock_noop(MockDelegate *d, void *a, void *b) { (void)d; (void)a; (void)b; return S_OK; }
MOCK(g_h_noop, mock_noop);

static void mock_stop_render_loop(void)
{
    /* AppMain::StopRenderLoop 0x1DA9D4: under the lock, `action = [AppMain+0xC]; action->QI(IAsyncInfo)->Cancel()` with NO NULL check */
    void *action;
    void *info = NULL;
    x104_lock(&g_game_lock);
    action = APPMAIN_RENDER_ACTION(g_appmain);
    x104_raw_qi(action, &X104_IID_IAsyncInfo, &info);           /* a NULL action = access violation inside the lock, like the game */
    if (info) { G(info, IAsyncInfo, Cancel)(info); x104_raw_release(info); }
    x104_unlock(&g_game_lock);
}

static HRESULT mock_on_suspending(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    void *operation = NULL, *deferral = NULL;
    LONGLONG deadline = 0;
    (void)d; (void)sender;
    if (SUCCEEDED(G(args, ISuspendingEventArgs, get_SuspendingOperation)(args, &operation)) && operation &&
        SUCCEEDED(G(operation, ISuspendingOperation, GetDeferral)(operation, &deferral)) && deferral &&
        SUCCEEDED(G(operation, ISuspendingOperation, get_Deadline)(operation, &deadline)) && deadline > 0 &&
        SUCCEEDED(G(deferral, ISuspendingDeferral, Complete)(deferral)))
        g_susp_chain_ok = 1;
    InterlockedIncrement(&g_susp_events);
    mock_stop_render_loop();                                    /* MainPage::SaveState 0x1DFA40 */
    return S_OK;
}
MOCK(g_h_suspending, mock_on_suspending);

static void mock_start_render_loop(void);
static HRESULT mock_on_visibility(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    BYTE visible = 2;
    (void)d; (void)sender;
    G(args, IVisibilityChangedEventArgs, get_Visible)(args, &visible);       /* straight on the pointer (0x1DF9CC) */
    g_vis_last = visible;
    InterlockedIncrement(&g_vis_events);
    if (!APPMAIN_READY(g_appmain)) return S_OK;
    if (!visible) {
        if (!APPMAIN_RENDER_ACTION(g_appmain)) InterlockedIncrement(&g_vis_unsafe);   /* the host delivered it before a render loop existed */
        mock_stop_render_loop();
    } else {
        mock_start_render_loop();
    }
    return S_OK;
}
MOCK(g_h_visibility, mock_on_visibility);

static HRESULT mock_on_back(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    (void)d; (void)sender;
    InterlockedIncrement(&g_back_events);
    return G(args, IBackPressedEventArgs, put_Handled)(args, 1);
}
MOCK(g_h_back, mock_on_back);

static HRESULT mock_on_pane_showing(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    void *statics = mock_activate(L"Windows.UI.ViewManagement.InputPane", &X104_IID_IInputPaneStatics);
    void *pane = NULL;
    X104Rect rect;
    (void)d; (void)sender; (void)args;
    memset(&rect, 0, sizeof(rect));
    if (statics && SUCCEEDED(G(statics, IInputPaneStatics, GetForCurrentView)(statics, &pane)) && pane && SUCCEEDED(G(pane, IInputPane, get_OccludedRect)(pane, &rect)))
        g_pane_height = (LONG)rect.Height;
    InterlockedIncrement(&g_pane_showing);
    return S_OK;
}
MOCK(g_h_pane_showing, mock_on_pane_showing);
static HRESULT mock_on_pane_hiding(MockDelegate *d, void *sender, void *args) { UI_ONLY(); (void)d; (void)sender; (void)args; InterlockedIncrement(&g_pane_hiding); return S_OK; }
MOCK(g_h_pane_hiding, mock_on_pane_hiding);

static HRESULT mock_on_dispatched_native(MockDelegate *d, void *a, void *b)
{
    UI_ONLY();
    /* the DR lambda 0x1DAF04: QI(panel, ISwapChainPanelNative)->SetSwapChain(chain).  No chain exists in this test: NULL is "detach" and must be harmless. */
    void *native = mock_qi_of(g_panel, &X104_IID_ISwapChainPanelNative);
    (void)d; (void)a; (void)b;
    if (native) { G(native, ISwapChainPanelNative, SetSwapChain)(native, NULL); x104_raw_release(native); InterlockedIncrement(&g_native_calls); }
    return S_OK;
}
MOCK(g_h_native, mock_on_dispatched_native);

static HRESULT mock_on_size(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    X104Size size;
    double w = 0.0, h = 0.0;
    float sx = 0.0f, sy = 0.0f;
    int orientation = -1, native_orientation = -1;
    float dpi = 0.0f;
    void *display_statics, *display = NULL;
    (void)d;
    x104_lock(&g_game_lock);                                    /* OnSizeChanged: lock(AppMain+0x10) */
    memset(&size, 0, sizeof(size));
    G(g_panel_fe, IFrameworkElement, get_ActualWidth)(g_panel_fe, &w);
    G(g_panel_fe, IFrameworkElement, get_ActualHeight)(g_panel_fe, &h);
    G(g_panel_scp, ISwapChainPanel, get_CompositionScaleX)(g_panel_scp, &sx);
    G(g_panel_scp, ISwapChainPanel, get_CompositionScaleY)(g_panel_scp, &sy);
    display_statics = mock_activate(L"Windows.Graphics.Display.DisplayInformation", &X104_IID_IDisplayInformationStatics);
    if (display_statics && SUCCEEDED(G(display_statics, IDisplayInformationStatics, GetForCurrentView)(display_statics, &display)) && display) {
        G(display, IDisplayInformation, get_NativeOrientation)(display, &native_orientation);
        G(display, IDisplayInformation, get_CurrentOrientation)(display, &orientation);
        G(display, IDisplayInformation, get_LogicalDpi)(display, &dpi);
    }
    if (!g_size_events) {
        void *dep = g_panel_dep, *dispatcher = NULL, *op = NULL;
        SEQ(g_seq_size1);
        if (dep && SUCCEEDED(G(dep, IDependencyObject, get_Dispatcher)(dep, &dispatcher)) && dispatcher) {
            HRESULT hr = G(dispatcher, ICoreDispatcher, RunAsync)(dispatcher, 1 /* High */, &g_h_native, &op);
            CHECK(SUCCEEDED(hr) && op != NULL);
            CHECK(g_native_calls == 0);                          /* R1/R11: RunAsync queued, did not run inside the call */
        }
    }
    G(args, ISizeChangedEventArgs, get_NewSize)(args, &size);    /* straight on the pointer, no QI (R10) */
    CHECK(sender == (void *)&x104_panel_object()->views[0]);
    CHECK(w == (double)size.Width && h == (double)size.Height && sx == 1.0f && sy == 1.0f && orientation == 1 && native_orientation == 1 && dpi == 96.0f);
    if (!g_size_events) { g_first_size_w = (LONG)size.Width; g_first_size_h = (LONG)size.Height; }
    g_last_size_w = (LONG)size.Width;
    g_last_size_h = (LONG)size.Height;
    InterlockedIncrement(&g_size_events);
    x104_unlock(&g_game_lock);
    return S_OK;
}
MOCK(g_h_size, mock_on_size);

static HRESULT mock_on_key_down(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    int vk = -1;
    BYTE status[16];
    (void)d; (void)sender;
    memset(status, 0xEE, sizeof(status));
    G(args, IKeyEventArgs, get_VirtualKey)(args, &vk);
    G(args, IKeyEventArgs, get_KeyStatus)(args, status);
    g_key_vk = vk;
    g_key_repeat = *(UINT32 *)status;
    InterlockedIncrement(&g_keydown);
    return S_OK;
}
MOCK(g_h_key_down, mock_on_key_down);
static HRESULT mock_on_key_up(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    BYTE status[16];
    (void)d; (void)sender;
    G(args, IKeyEventArgs, get_KeyStatus)(args, status);
    InterlockedIncrement(&g_keyup);
    return S_OK;
}
MOCK(g_h_key_up, mock_on_key_up);

/* HID::onPointerPressed/Moved/Released: args->get_CurrentPoint (slot 6, no QI), point->get_Position/get_PointerId/get_Properties/get_PointerDevice */
static void mock_read_pointer(void *args, int kind, int source)
{
    void *point = NULL, *properties = NULL, *device = NULL;
    X104Point position;
    UINT32 id = 0;
    BYTE left = 0xFF;
    int type = -1;
    memset(&position, 0, sizeof(position));
    if (FAILED(G(args, IPointerEventArgs, get_CurrentPoint)(args, &point)) || !point) { InterlockedIncrement((volatile LONG *)&g_error_count); return; }
    G(point, IPointerPoint, get_Position)(point, &position);
    G(point, IPointerPoint, get_PointerId)(point, &id);
    if (SUCCEEDED(G(point, IPointerPoint, get_Properties)(point, &properties)) && properties) G(properties, IPointerPointProperties, get_IsLeftButtonPressed)(properties, &left);
    if (SUCCEEDED(G(point, IPointerPoint, get_PointerDevice)(point, &device)) && device) G(device, IPointerDevice, get_PointerDeviceType)(device, &type);
    if (kind == 1) {
        g_press_x = (LONG)position.X; g_press_y = (LONG)position.Y; g_press_id = (LONG)id; g_press_device = type; g_press_left = left;
        InterlockedIncrement(&g_pressed);
        if (source == 2) InterlockedIncrement(&g_pressed_src2); else if (source == 1) InterlockedIncrement(&g_pressed_src1); else InterlockedIncrement(&g_pressed_cw);
    } else if (kind == 2) {
        g_release_x = (LONG)position.X; g_release_y = (LONG)position.Y; g_release_left = left;
        InterlockedIncrement(&g_released);
    } else {
        InterlockedIncrement(&g_moved);
    }
    x104_raw_release(point);
    if (properties) x104_raw_release(properties);
    if (device) x104_raw_release(device);
}
static HRESULT mock_ptr_pressed(MockDelegate *d, void *s, void *a) { UI_ONLY(); (void)d; (void)s; mock_read_pointer(a, 1, 2); return S_OK; }
static HRESULT mock_ptr_moved(MockDelegate *d, void *s, void *a) { UI_ONLY(); (void)d; (void)s; mock_read_pointer(a, 0, 2); return S_OK; }
static HRESULT mock_ptr_released(MockDelegate *d, void *s, void *a) { UI_ONLY(); (void)d; (void)s; mock_read_pointer(a, 2, 2); return S_OK; }
MOCK(g_h_pressed, mock_ptr_pressed);
MOCK(g_h_moved, mock_ptr_moved);
MOCK(g_h_released, mock_ptr_released);

static HRESULT mock_on_text_changed(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    HSTRING text = NULL;
    (void)d; (void)sender; (void)args;
    if (g_editbox_tb && SUCCEEDED(G(g_editbox_tb, ITextBox, get_Text)(g_editbox_tb, &text))) {
        x104_hstring_wide(text, g_text_seen, ARRAYSIZE(g_text_seen));
        x104_hstring_free(text);
    }
    InterlockedIncrement(&g_text_events);
    return S_OK;
}
MOCK(g_h_text_changed, mock_on_text_changed);

/* ---- thread-pool work items ---- */
static void mock_input_item_body(BOOL game_hid)
{
    /* MainPage ctor work item 0x1DE0BC / HID's pool item 0x110940: panel->CreateCoreIndependentInputSource(7), ICorePointerInputSource.add_Pointer*, source->get_Dispatcher().ProcessEvents(ProcessUntilQuit) */
    void *src = NULL, *ptr = NULL, *dispatcher = NULL;
    LONGLONG token = 0;
    HRESULT hr;
    InterlockedIncrement(&g_workitem_threads);
    hr = G(g_panel_scp, ISwapChainPanel, CreateCoreIndependentInputSource)(g_panel_scp, 7, &src);
    CHECK(SUCCEEDED(hr) && src);
    if (!src) return;
    ptr = mock_qi_of(src, &X104_IID_ICorePointerInputSource);
    CHECK(ptr != NULL);
    if (ptr) {
        CHECK(SUCCEEDED(G(ptr, ICorePointerInputSource, add_PointerPressed)(ptr, game_hid ? (void *)&g_h_pressed : (void *)&g_h_noop, &token)) && token != 0);
        CHECK(SUCCEEDED(G(ptr, ICorePointerInputSource, add_PointerMoved)(ptr, game_hid ? (void *)&g_h_moved : (void *)&g_h_noop, &token)));
        CHECK(SUCCEEDED(G(ptr, ICorePointerInputSource, add_PointerReleased)(ptr, game_hid ? (void *)&g_h_released : (void *)&g_h_noop, &token)));
    }
    hr = G(src, ICoreInputSourceBase, get_Dispatcher)(src, &dispatcher);          /* straight on the pointer CreateCoreIndependentInputSource returned (R10) */
    CHECK(SUCCEEDED(hr) && dispatcher);
    if (dispatcher) G(dispatcher, ICoreDispatcher, ProcessEvents)(dispatcher, 2 /* ProcessUntilQuit */);
    InterlockedIncrement(&g_input_items_done);
}
static HRESULT mock_item_ctor_input(MockDelegate *d, void *action, void *b) { (void)d; (void)action; (void)b; mock_input_item_body(FALSE); return S_OK; }
static HRESULT mock_item_hid_input(MockDelegate *d, void *action, void *b) { (void)d; (void)action; (void)b; mock_input_item_body(TRUE); return S_OK; }
MOCK(g_h_item_ctor_input, mock_item_ctor_input);
MOCK(g_h_item_hid_input, mock_item_hid_input);

static HRESULT mock_item_init_job(MockDelegate *d, void *action, void *b)
{
    /* lambda 0x1DA2D8: client[+0xC] = holder; MinecraftClient::init (seconds of shader compiles); client[+8]=1; AppMain[+0x3C]=1 */
    (void)d; (void)b;
    CHECK(action != NULL);
    *(DWORD *)(g_client + 0x0C) = (DWORD)(ULONG_PTR)&g_client;       /* holder != NULL */
    Sleep(250);
    *(DWORD *)(g_client + 0x08) = 1;
    __sync_synchronize();
    APPMAIN_READY(g_appmain) = 1;
    return S_OK;
}
MOCK(g_h_item_init, mock_item_init_job);

static HRESULT mock_item_render(MockDelegate *d, void *action, void *b)
{
    /* the render loop body 0x1DA160: while (action->QI(IAsyncInfo)->get_Status == Started) { lock; if ready { update + present }; unlock } */
    void *info = NULL;
    int status = 0;
    (void)d; (void)b;
    CHECK(action != NULL);
    if (FAILED(x104_raw_qi(action, &X104_IID_IAsyncInfo, &info)) || !info) { CHECK(FALSE); return S_OK; }
    InterlockedIncrement(&g_render_threads_running);
    for (;;) {
        if (FAILED(G(info, IAsyncInfo, get_Status)(info, &status)) || status != 0) break;
        x104_lock(&g_game_lock);
        if (APPMAIN_READY(g_appmain)) {
            InterlockedIncrement(&g_frames);
            InterlockedIncrement(&arm32_boot_frames_started);          /* what the Present stub does */
            InterlockedIncrement(&arm32_boot_frames_done);
        }
        x104_unlock(&g_game_lock);
    }
    InterlockedDecrement(&g_render_threads_running);
    x104_raw_release(info);
    return S_OK;
}
MOCK(g_h_item_render, mock_item_render);

static void mock_start_render_loop(void)
{
    /* AppMain::StartRenderLoop 0x1DA8A8: ThreadPool.RunWithPriorityAndOptionsAsync(handler, High, TimeSliced) -> [AppMain+0xC] */
    void *statics = mock_activate(L"Windows.System.Threading.ThreadPool", &X104_IID_IThreadPoolStatics);
    void *op = NULL;
    HRESULT hr;
    x104_lock(&g_game_lock);
    hr = G(statics, IThreadPoolStatics, RunWithPriorityAndOptionsAsync)(statics, &g_h_item_render, 1, 1, &op);
    CHECK(SUCCEEDED(hr) && op);
    APPMAIN_RENDER_ACTION(g_appmain) = op;
    x104_unlock(&g_game_lock);
    InterlockedIncrement(&g_render_starts);
    SEQ(g_seq_render);
}

/* ---- the DispatcherTimer tick: MainPage::OnTick 0x1DF8A4 ---- */
static HRESULT mock_on_tick(MockDelegate *d, void *sender, void *args)
{
    UI_ONLY();
    void *window_statics, *window = NULL, *core = NULL, *statics;
    LONGLONG token = 0;
    X104Size render_size;
    void *op = NULL;
    (void)d; (void)args;
    InterlockedIncrement(&g_ticks);
    if (!g_seq_tick_first) SEQ(g_seq_tick_first);
    if (!x104.f_size_changed_done) InterlockedIncrement(&g_tick_gate_violation);     /* R3: no Tick before the first SizeChanged returned */
    CHECK(sender == (void *)&((X104View *)g_timer)->obj->views[0]);                  /* sender = the timer's IInspectable identity view */
    if (!APPMAIN_READY(g_appmain)) return S_OK;                                       /* IsAppInitialized 0x1DA310 */
    if (g_ready_ticks++) return S_OK;
    SEQ(g_seq_tick_ready);
    CHECK(SUCCEEDED(G(g_timer, IDispatcherTimer, Stop)(g_timer)));
    CHECK(SUCCEEDED(G(g_logo_ui, IUIElement, put_Visibility)(g_logo_ui, 1 /* Collapsed */)));
    /* AppMain::AttachToPanel 0x1DA634 -> AppPlatform_Winrt::initialize -> HID::Initialize */
    x104_lock(&g_game_lock);
    window_statics = mock_activate(L"Windows.UI.Xaml.Window", &X104_IID_IWindowStatics);
    CHECK(SUCCEEDED(G(window_statics, IWindowStatics, get_Current)(window_statics, &window)) && window);
    CHECK(SUCCEEDED(G(window, IWindow, get_CoreWindow)(window, &core)) && core == g_core_window);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_KeyDown)(core, &g_h_key_down, &token)) && token != 0);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_KeyUp)(core, &g_h_key_up, &token)) && token != 0);
    CHECK(SUCCEEDED(G(g_editbox_tb, ITextBox, add_TextChanged)(g_editbox_tb, &g_h_text_changed, &token)) && token != 0);
    {
        void *tb2 = mock_qi_of(g_editbox, &X104_IID_ITextBox2);
        CHECK(tb2 != NULL);
        if (tb2) CHECK(SUCCEEDED(G(tb2, ITextBox2, put_PreventKeyboardDisplayOnProgrammaticFocus)(tb2, 0)));
    }
    CHECK(SUCCEEDED(G(g_editbox_tb, ITextBox, put_AcceptsReturn)(g_editbox_tb, 1)));
    statics = mock_activate(L"Windows.System.Threading.ThreadPool", &X104_IID_IThreadPoolStatics);
    CHECK(SUCCEEDED(G(statics, IThreadPoolStatics, RunWithPriorityAndOptionsAsync)(statics, &g_h_item_hid_input, 1, 1, &op)) && op);
    memset(&render_size, 0, sizeof(render_size));
    CHECK(SUCCEEDED(G(g_panel_ui, IUIElement, get_RenderSize)(g_panel_ui, &render_size)) && render_size.Width > 0.0f && render_size.Height > 0.0f);
    x104_unlock(&g_game_lock);
    SEQ(g_seq_attach);
    mock_start_render_loop();                                                          /* StartRenderLoop 0x1DA8A8 */
    return S_OK;
}
MOCK(g_h_tick, mock_on_tick);

/* ---- App2 / OnLaunched / MainPage constructor ---- */
static HRESULT WINAPI mock_outer_qi(void *self, const GUID *iid, void **out)
{
    MockOuter *outer = (MockOuter *)self;
    *out = NULL;
    if (memcmp(iid, &X104_IID_IApplicationOverrides, sizeof(GUID)) == 0 && outer->overrides) {
        *out = outer->overrides;
        InterlockedIncrement(&outer->refs);
        return S_OK;
    }
    if (memcmp(iid, &X104_IID_IUnknown, sizeof(GUID)) == 0 || memcmp(iid, &X104_IID_IInspectable, sizeof(GUID)) == 0) {
        *out = self;
        InterlockedIncrement(&outer->refs);
        return S_OK;
    }
    {
        void *inner = (outer == &g_app_outer) ? g_inner_app : g_inner_page;       /* unknown IIDs go to the inner (App2::vf0 / MainPage::QI) */
        return X104_VFN(inner, 0, HRESULT (WINAPI *)(void *, const GUID *, void **))(inner, iid, out);
    }
}
static ULONG WINAPI mock_outer_addref(void *self) { return (ULONG)InterlockedIncrement(&((MockOuter *)self)->refs); }
static ULONG WINAPI mock_outer_release(void *self) { return (ULONG)InterlockedDecrement(&((MockOuter *)self)->refs); }
static const void *mock_outer_vtbl[3] = { mock_outer_qi, mock_outer_addref, mock_outer_release };

static void mock_mainpage_ctor(void)
{
    void *page_factory, *uri_factory, *uri = NULL, *app_statics, *fe = NULL, *window_statics, *window = NULL, *core = NULL, *display_statics, *display = NULL;
    void *hw, *pane_statics, *pane = NULL, *dispatcher_statics, *threadpool, *op = NULL, *canvas, *package_statics, *package = NULL, *folder = NULL, *item = NULL;
    void *appdata_statics, *appdata = NULL, *memory, *timer_factory, *package_item;
    LONGLONG token = 0;
    HRESULT hr;
    HSTRING name, path = NULL;
    wchar_t wide[MAX_PATH];
    int index;
    ULONGLONG usage = 0, limit = 0;
    (void)dispatcher_statics;
    g_ctor_thread = (LONG)GetCurrentThreadId();

    /* Page base: IPageFactory.CreateInstance(outer = MainPage+8, &inner, &instance) */
    memset(g_mainpage, 0, sizeof(g_mainpage));
    ((MockOuter *)(g_mainpage + 8))->vtbl = mock_outer_vtbl;
    ((MockOuter *)(g_mainpage + 8))->refs = 1;
    page_factory = mock_activate(L"Windows.UI.Xaml.Controls.Page", &X104_IID_IPageFactory);
    hr = G(page_factory, IPageFactory, CreateInstance)(page_factory, g_mainpage + 8, &g_inner_page, &g_instance_page);
    CHECK(SUCCEEDED(hr) && g_inner_page && g_instance_page);
    CHECK(x104.f_mainpage_created && x104.mainpage_outer == (void *)(g_mainpage + 8) && x104_game_mainpage() == g_mainpage);
    CHECK(((MockOuter *)(g_mainpage + 8))->refs == 2);                 /* the host AddRef'd outer exactly once */

    /* InitializeComponent: Uri, LoadComponentWithResourceLocation, QI(this, IFrameworkElement) -> FindName x4 */
    uri_factory = mock_activate(L"Windows.Foundation.Uri", &X104_IID_IUriRuntimeClassFactory);
    name = mock_hs(L"ms-appx:///MainPage.xaml");
    CHECK(SUCCEEDED(G(uri_factory, IUriRuntimeClassFactory, CreateUri)(uri_factory, name, &uri)) && uri);
    x104_hstring_free(name);
    app_statics = mock_activate(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationStatics);
    CHECK(G(app_statics, IApplicationStatics, LoadComponentWithResourceLocation)(app_statics, g_mainpage, uri, 0) == S_OK);
    CHECK(SUCCEEDED(mock_outer_qi(g_mainpage + 8, &X104_IID_IFrameworkElement, &fe)) && fe);
    {
        static const wchar_t *names[4] = { L"swapChainPanel", L"Logo", L"EditBox", L"Button" };
        void *found[4] = { NULL, NULL, NULL, NULL };
        g_findname_ok = 1;
        for (index = 0; index < 4; ++index) {
            name = mock_hs(names[index]);
            hr = G(fe, IFrameworkElement, FindName)(fe, name, &found[index]);
            if (FAILED(hr) || !found[index]) g_findname_ok = 0;
            x104_hstring_free(name);
        }
        if (g_findname_ok) {
            g_panel = found[0];
            g_panel_scp = mock_qi_of(g_panel, &X104_IID_ISwapChainPanel);
            g_panel_fe = mock_qi_of(g_panel, &X104_IID_IFrameworkElement);
            g_panel_ui = mock_qi_of(g_panel, &X104_IID_IUIElement);
            g_panel_dep = mock_qi_of(g_panel, &X104_IID_IDependencyObject);
            g_logo_ui = mock_qi_of(found[1], &X104_IID_IUIElement);
            CHECK(mock_qi_of(found[1], &X104_IID_IImage) != NULL);
            g_editbox = found[2];
            g_editbox_tb = mock_qi_of(found[2], &X104_IID_ITextBox);
            g_editbox_ctl = mock_qi_of(found[2], &X104_IID_IControl);
            g_editbox_ui = mock_qi_of(found[2], &X104_IID_IUIElement);
            g_button = found[3];
            g_button_ctl = mock_qi_of(found[3], &X104_IID_IControl);
            CHECK(mock_qi_of(found[3], &X104_IID_IButton) != NULL);
            g_findname_ok = g_panel_scp && g_panel_fe && g_panel_ui && g_panel_dep && g_logo_ui && g_editbox_tb && g_editbox_ctl && g_editbox_ui && g_button_ctl;
        }
    }
    CHECK_MSG(g_findname_ok, "FindName / QI of the four elements");

    /* Window.Current.CoreWindow.add_VisibilityChanged */
    window_statics = mock_activate(L"Windows.UI.Xaml.Window", &X104_IID_IWindowStatics);
    CHECK(SUCCEEDED(G(window_statics, IWindowStatics, get_Current)(window_statics, &window)) && window);
    g_window = window;
    CHECK(SUCCEEDED(G(window, IWindow, get_CoreWindow)(window, &core)) && core);
    g_core_window = core;
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_VisibilityChanged)(core, &g_h_visibility, &token)) && token != 0);

    /* DisplayInformation events */
    display_statics = mock_activate(L"Windows.Graphics.Display.DisplayInformation", &X104_IID_IDisplayInformationStatics);
    CHECK(SUCCEEDED(G(display_statics, IDisplayInformationStatics, GetForCurrentView)(display_statics, &display)) && display);
    CHECK(SUCCEEDED(G(display, IDisplayInformation, add_DpiChanged)(display, &g_h_noop, &token)));
    CHECK(SUCCEEDED(G(display, IDisplayInformation, add_OrientationChanged)(display, &g_h_noop, &token)));
    CHECK(SUCCEEDED(G(display_statics, IDisplayInformationStatics, add_DisplayContentsInvalidated)(display_statics, &g_h_noop, &token)));
    /* panel events */
    CHECK(SUCCEEDED(G(g_panel_scp, ISwapChainPanel, add_CompositionScaleChanged)(g_panel_scp, &g_h_noop, &token)));
    CHECK(SUCCEEDED(G(g_panel_fe, IFrameworkElement, add_SizeChanged)(g_panel_fe, &g_h_size, &token)) && token != 0);
    hw = mock_activate(L"Windows.Phone.UI.Input.HardwareButtons", &X104_IID_IHardwareButtonsStatics);
    CHECK(SUCCEEDED(G(hw, IHardwareButtonsStatics, add_BackPressed)(hw, &g_h_back, &token)) && token != 0);
    pane_statics = mock_activate(L"Windows.UI.ViewManagement.InputPane", &X104_IID_IInputPaneStatics);
    CHECK(SUCCEEDED(G(pane_statics, IInputPaneStatics, GetForCurrentView)(pane_statics, &pane)) && pane);
    CHECK(SUCCEEDED(G(pane, IInputPane, add_Hiding)(pane, &g_h_pane_hiding, &token)));
    CHECK(SUCCEEDED(G(pane, IInputPane, add_Showing)(pane, &g_h_pane_showing, &token)));

    /* (DeviceResources: D3D11CreateDevice - not part of this test)  the input work item */
    threadpool = mock_activate(L"Windows.System.Threading.ThreadPool", &X104_IID_IThreadPoolStatics);
    CHECK(SUCCEEDED(G(threadpool, IThreadPoolStatics, RunWithPriorityAndOptionsAsync)(threadpool, &g_h_item_ctor_input, 1, 1, &op)) && op);
    canvas = mock_activate(L"Windows.UI.Xaml.Controls.Canvas", &X104_IID_ICanvasStatics);
    CHECK(G(canvas, ICanvasStatics, SetLeft)(canvas, g_editbox_ui, 3000.0) == S_OK);
    CHECK(G(canvas, ICanvasStatics, SetLeft)(canvas, g_editbox_ui, 3000.0) == S_OK);

    /* AppMain + createApp: Package.InstalledLocation.Path, ApplicationData.LocalFolder.Path, MemoryManager, init job */
    memset(g_appmain, 0, sizeof(g_appmain));
    memset(g_client, 0, sizeof(g_client));
    *(DWORD *)(g_appmain + 0x34) = (DWORD)(ULONG_PTR)g_client;
    *(DWORD *)(g_mainpage + X104_MAINPAGE_APPMAIN) = (DWORD)(ULONG_PTR)g_appmain;     /* [MainPage+0x7C] = AppMain*: stored at the END of the AppMain step */
    package_statics = mock_activate(L"Windows.ApplicationModel.Package", &X104_IID_IPackageStatics);
    CHECK(SUCCEEDED(G(package_statics, IPackageStatics, get_Current)(package_statics, &package)) && package);
    CHECK(SUCCEEDED(G(package, IPackage, get_InstalledLocation)(package, &folder)) && folder);
    package_item = mock_qi_of(folder, &X104_IID_IStorageItem);
    CHECK(package_item && SUCCEEDED(G(package_item, IStorageItem, get_Path)(package_item, &path)) && path);
    x104_hstring_wide(path, wide, ARRAYSIZE(wide));
    CHECK(lstrcmpiW(wide, x104.game_root) == 0);
    x104_hstring_free(path);
    appdata_statics = mock_activate(L"Windows.Storage.ApplicationData", &X104_IID_IApplicationDataStatics);
    CHECK(SUCCEEDED(G(appdata_statics, IApplicationDataStatics, get_Current)(appdata_statics, &appdata)) && appdata);
    folder = NULL;
    CHECK(SUCCEEDED(G(appdata, IApplicationData, get_LocalFolder)(appdata, &folder)) && folder);
    item = mock_qi_of(folder, &X104_IID_IStorageItem);
    path = NULL;
    CHECK(item && SUCCEEDED(G(item, IStorageItem, get_Path)(item, &path)));
    x104_hstring_wide(path, wide, ARRAYSIZE(wide));
    CHECK(lstrcmpiW(wide, x104.data_root) == 0);
    x104_hstring_free(path);
    memory = mock_activate(L"Windows.System.MemoryManager", &X104_IID_IMemoryManagerStatics);
    CHECK(SUCCEEDED(G(memory, IMemoryManagerStatics, get_AppMemoryUsage)(memory, &usage)) && SUCCEEDED(G(memory, IMemoryManagerStatics, get_AppMemoryUsageLimit)(memory, &limit)) && limit > usage);
    op = NULL;
    CHECK(SUCCEEDED(G(threadpool, IThreadPoolStatics, RunAsync)(threadpool, &g_h_item_init, &op)) && op);       /* the init job starts running at once */
    x104_raw_release(op);                                                                                       /* createApp releases the action right after the call */

    /* DispatcherTimer: CreateInstance(NULL, NULL, &t), put_Interval(50 ms), add_Tick, Start */
    timer_factory = mock_activate(L"Windows.UI.Xaml.DispatcherTimer", &X104_IID_IDispatcherTimerFactory);
    {
        void *created = NULL;
        CHECK(SUCCEEDED(G(timer_factory, IDispatcherTimerFactory, CreateInstance)(timer_factory, NULL, NULL, &created)) && created);
        g_timer = mock_qi_of(created, &X104_IID_IDispatcherTimer);
        CHECK(g_timer != NULL);
    }
    CHECK(SUCCEEDED(G(g_timer, IDispatcherTimer, put_Interval)(g_timer, 500000LL)));
    CHECK(SUCCEEDED(G(g_timer, IDispatcherTimer, add_Tick)(g_timer, &g_h_tick, &token)) && token != 0);
    CHECK(SUCCEEDED(G(g_timer, IDispatcherTimer, Start)(g_timer)));
    CHECK(x104.f_timer_started);
    (void)threadpool;
}

static HRESULT WINAPI mock_on_launched(void *self, void *args, void *unused3)
{
    void *statusbar_statics, *statusbar = NULL, *action = NULL, *activated = NULL, *ui = NULL, *window_statics, *window = NULL;
    int previous = -1;
    (void)self; (void)unused3;
    InterlockedIncrement(&g_onlaunched_calls);
    g_launch_thread = (LONG)GetCurrentThreadId();
    SEQ(g_seq_launch);
    statusbar_statics = mock_activate(L"Windows.UI.ViewManagement.StatusBar", &X104_IID_IStatusBarStatics);
    CHECK(SUCCEEDED(G(statusbar_statics, IStatusBarStatics, GetForCurrentView)(statusbar_statics, &statusbar)) && statusbar);
    CHECK(SUCCEEDED(G(statusbar, IStatusBar, HideAsync)(statusbar, &action)) && action);
    x104_raw_release(action);
    mock_mainpage_ctor();
    /* QI(args, IActivatedEventArgs)->get_PreviousExecutionState */
    activated = mock_qi_of(args, &X104_IID_IActivatedEventArgs);
    CHECK(activated != NULL);
    if (activated) G(activated, IActivatedEventArgs, get_PreviousExecutionState)(activated, &previous);
    g_prev_state = previous;
    ui = NULL;
    mock_outer_qi(g_mainpage + 8, &X104_IID_IUIElement, &ui);
    g_page_qi_ok = ui != NULL;
    window_statics = mock_activate(L"Windows.UI.Xaml.Window", &X104_IID_IWindowStatics);
    CHECK(SUCCEEDED(G(window_statics, IWindowStatics, get_Current)(window_statics, &window)) && window);
    CHECK(G(window, IWindow, put_Content)(window, ui) == S_OK);
    SEQ(g_seq_content);
    CHECK(x104.f_content_set);
    CHECK(G(window, IWindow, Activate)(window) == S_OK);
    return S_OK;
}
static const void *mock_overrides_vtbl[16] = { mock_outer_qi, mock_outer_addref, mock_outer_release, NULL, NULL, NULL, NULL, mock_on_launched };
static MockOuter g_overrides_object = { mock_overrides_vtbl, 1, NULL };

static HRESULT mock_callback_invoke(MockDelegate *self, void *sender, void *args)
{
    void *factory, *app = NULL;
    LONGLONG token = 0;
    (void)self; (void)sender; (void)args;
    factory = mock_activate(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationFactory);
    CHECK(factory != NULL);
    g_app_outer.vtbl = mock_outer_vtbl;
    g_app_outer.refs = 1;
    g_app_outer.overrides = &g_overrides_object;
    CHECK(SUCCEEDED(G(factory, IApplicationFactory, CreateInstance)(factory, &g_app_outer, &g_inner_app, &g_instance_app)) && g_inner_app && g_instance_app);
    CHECK(g_app_outer.refs == 2);
    app = mock_qi_of(g_inner_app, &X104_IID_IApplication);
    CHECK(app != NULL);
    if (app) {
        CHECK(SUCCEEDED(G(app, IApplication, add_Suspending)(app, &g_h_suspending, &token)) && token != 0);
        CHECK(SUCCEEDED(G(app, IApplication, add_Resuming)(app, &g_h_resume, &token)) && token != 0);
    }
    return S_OK;
}
MOCK(g_h_callback, mock_callback_invoke);

/* ======================================================================================================================
 * the tester thread: drives the real window procedure from outside, like a user would
 * ====================================================================================================================== */
static HWND g_hwnd;
static volatile LONG g_tester_done;
static volatile LONG g_stage;
static int g_wait_failed;

static BOOL wait_until(volatile LONG *value, LONG minimum, DWORD timeout_ms, const char *what)
{
    DWORD start = GetTickCount();
    while (*value < minimum) {
        if (GetTickCount() - start > timeout_ms) {
            test_print("  TIMEOUT waiting for %s (value %ld, wanted %ld, stage %ld)\n", what, (long)*value, (long)minimum, (long)g_stage);
            ++g_wait_failed;
            return FALSE;
        }
        Sleep(5);
    }
    return TRUE;
}

static void tester_ui_show_keyboard(void *argument)
{
    /* HID's UI-thread lambda 0x110BE8: MaxLength, put_Text, Select, IControl::Focus(Programmatic) on the TextBox.  The setter must not raise TextChanged inline. */
    HSTRING text = mock_hs(L"hello");
    BYTE result = 0;
    LONG before = g_text_events;
    (void)argument;
    CHECK(SUCCEEDED(G(g_editbox_tb, ITextBox, put_MaxLength)(g_editbox_tb, 50000)));
    CHECK(SUCCEEDED(G(g_editbox_tb, ITextBox, put_Text)(g_editbox_tb, text)));
    CHECK(g_text_events == before);                              /* R14: asynchronous */
    CHECK(SUCCEEDED(G(g_editbox_tb, ITextBox, Select)(g_editbox_tb, 5, 5)));
    CHECK(SUCCEEDED(G(g_editbox_ctl, IControl, Focus)(g_editbox_ctl, 3, &result)));
    x104_hstring_free(text);
}
static void tester_ui_hide_keyboard(void *argument)
{
    BYTE result = 0;
    (void)argument;
    CHECK(SUCCEEDED(G(g_button_ctl, IControl, Focus)(g_button_ctl, 3, &result)));
}

static void tester_ui_pane(void *argument)
{
    (void)argument;
    x104_inputpane_raise(TRUE);
    x104_inputpane_raise(FALSE);
}

static DWORD WINAPI tester_thread(void *argument)
{
    RECT rect;
    int width, height;
    (void)argument;
    g_stage = 1;
    /* ---- boot milestones ---- */
    wait_until(&x104.f_launched, 1, 15000, "f_launched");
    wait_until(&x104.f_size_changed_done, 1, 15000, "first SizeChanged (f_size_changed_done)");
    wait_until(&x104.f_ready_tick, 1, 15000, "READY tick");
    wait_until(&g_render_starts, 1, 15000, "StartRenderLoop");
    wait_until(&g_frames, 50, 15000, "50 frames");
    g_stage = 2;
    wait_until(&g_input_items_done, 0, 1000, "-");
    {
        DWORD start = GetTickCount();
        while ((g_workitem_threads < 3 || x104_event_count(&x104_input_st.sources[1]->ptr[1]) < 1) && GetTickCount() - start < 5000) Sleep(5);
    }

    /* ---- mouse: press / move / release through the real window procedure ---- */
    g_stage = 3;
    SendMessageW(g_hwnd, 0x0201 /* WM_LBUTTONDOWN */, 1, MAKELONG(100, 200));
    wait_until(&g_pressed, 1, 2000, "PointerPressed");
    SendMessageW(g_hwnd, 0x0200 /* WM_MOUSEMOVE */, 1, MAKELONG(150, 220));
    wait_until(&g_moved, 1, 2000, "PointerMoved");
    SendMessageW(g_hwnd, 0x0202 /* WM_LBUTTONUP */, 0, MAKELONG(150, 220));
    wait_until(&g_released, 1, 2000, "PointerReleased");

    /* ---- keyboard: hardware keys ---- */
    g_stage = 4;
    SendMessageW(g_hwnd, 0x0100 /* WM_KEYDOWN */, 0x57 /* W */, 0x00110001);
    wait_until(&g_keydown, 1, 2000, "KeyDown");
    SendMessageW(g_hwnd, 0x0101 /* WM_KEYUP */, 0x57, (LPARAM)0xC0110001);
    wait_until(&g_keyup, 1, 2000, "KeyUp");
    SendMessageW(g_hwnd, 0x0100, 0x1B /* VK_ESCAPE = Back */, 0x00010001);
    wait_until(&g_back_events, 1, 2000, "BackPressed");

    /* ---- the on-screen Back/Pause button (host-drawn popup over the top-left of the game window): a tap = one more BackPressed ---- */
    Sleep(150);
    /* the mock game has no MinecraftClient: by default the button stays hidden (only in a world); then force it on like --back-button-always */
    g_button_hidden_without_world = x104_backbtn.hwnd && !IsWindowVisible(x104_backbtn.hwnd) ? 1 : 0;
    x104_backbtn.always = TRUE;
    Sleep(150);
    if (x104_backbtn.hwnd) {
        RECT button, client;
        POINT origin;
        origin.x = 0;
        origin.y = 0;
        g_button_created = 1;
        g_button_visible = IsWindowVisible(x104_backbtn.hwnd) ? 1 : 0;
        if (GetWindowRect(x104_backbtn.hwnd, &button) && GetClientRect(g_hwnd, &client) && ClientToScreen(g_hwnd, &origin)) {
            g_button_top_right = button.left >= origin.x && button.right < origin.x + client.right / 2 &&
                                 button.top >= origin.y && button.bottom <= origin.y + client.bottom / 3 && (button.right - button.left) >= 40 ? 1 : 0;
        }
        SendMessageW(x104_backbtn.hwnd, 0x0201 /* WM_LBUTTONDOWN */, 1, (LPARAM)((5 << 16) | 5));
        SendMessageW(x104_backbtn.hwnd, 0x0202 /* WM_LBUTTONUP */, 0, (LPARAM)((5 << 16) | 5));
        wait_until(&g_back_events, 2, 2000, "BackPressed from the on-screen button");
    }

    /* ---- text input: show keyboard (game side), type a character, hide ---- */
    g_stage = 5;
    x104_ui_post(tester_ui_show_keyboard, NULL);
    wait_until(&g_text_events, 1, 2000, "TextChanged after put_Text");
    Sleep(150);
    SendMessageW(g_hwnd, 0x0102 /* WM_CHAR */, (WPARAM)L'x', 1);
    wait_until(&g_text_events, 2, 2000, "TextChanged after WM_CHAR");
    Sleep(150);
    x104_ui_post(tester_ui_pane, NULL);
    wait_until(&g_pane_showing, 1, 2000, "InputPane.Showing");
    x104_ui_post(tester_ui_hide_keyboard, NULL);
    Sleep(100);

    /* ---- resize (game ready: the second SizeChanged) ---- */
    g_stage = 6;
    GetClientRect(g_hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    SetWindowPos(g_hwnd, NULL, 0, 0, width + 200 + 16, height + 100 + 39, 0x0016 /* SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE */);
    wait_until(&g_size_events, 2, 3000, "second SizeChanged after the resize");

    /* ---- minimise / restore: VisibilityChanged to the game, the render loop stops and restarts ---- */
    g_stage = 7;
    ShowWindow(g_hwnd, 6 /* SW_MINIMIZE */);
    wait_until(&g_vis_events, 1, 3000, "VisibilityChanged(false)");
    Sleep(200);
    {
        LONG frames_a = g_frames;
        Sleep(200);
        g_render_stopped_frames = g_frames - frames_a;             /* must be 0 once the loop was cancelled */
    }
    ShowWindow(g_hwnd, 9 /* SW_RESTORE */);
    wait_until(&g_vis_events, 2, 3000, "VisibilityChanged(true)");
    wait_until(&g_render_starts, 2, 3000, "render loop restarted");
    {
        LONG frames_b = g_frames;
        DWORD start = GetTickCount();
        while (g_frames < frames_b + 20 && GetTickCount() - start < 3000) Sleep(5);
    }

    /* ---- close: WM_CLOSE -> exit proc -> Suspending (game ready, render loop exists) -> workers stop -> loop leaves ---- */
    g_stage = 8;
    SendMessageW(g_hwnd, WM_CLOSE, 0, 0);
    g_tester_done = 1;
    return 0;
}

static DWORD WINAPI watchdog_thread(void *argument)
{
    (void)argument;
    Sleep(60000);
    test_print("WATCHDOG: the integration test did not finish in 60 s (stage %ld) - last log lines follow\n", (long)g_stage);
    {
        int length = (int)g_log_len;
        int start = length > 6000 ? length - 6000 : 0;
        DWORD written;
        WriteFile(GetStdHandle((DWORD)-11), g_log + start, (DWORD)(length - start), &written, NULL);
    }
    TerminateProcess(GetCurrentProcess(), 99);
    return 0;
}

/* ======================================================================================================================
 * main: what x104_bootstrap does (units init, window, f_units_ready, selftest), then the game's WinMain -> Application::Start
 * ====================================================================================================================== */
int main(int argc, char **argv)
{
    WNDCLASSEXW klass;
    HANDLE tester, watchdog;
    int failures;
    int selftest_failures;
    static wchar_t cwd[MAX_PATH];
    HRESULT start_hr;
    (void)argv;
    g_verbose = argc > 1;

    memset(&x104, 0, sizeof(x104));
    x104.main_thread_id = GetCurrentThreadId();
    x104.ui_thread_id = x104.main_thread_id;
    x104.opt_width = 800;
    x104.opt_height = 600;
    GetCurrentDirectoryW(MAX_PATH, cwd);
    lstrcpyW(x104.game_root, cwd);
    lstrcpyW(x104.data_root, cwd);
    arm32_boot_start_tick = GetTickCount();
    RoInitialize(1 /* MTA */);

    memset(&klass, 0, sizeof(klass));
    klass.cbSize = sizeof(klass);
    klass.style = 0x0003 | 0x0020;
    klass.lpfnWndProc = harness_window_proc;
    klass.hInstance = GetModuleHandleW(NULL);
    klass.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    klass.lpszClassName = L"X104IntegrationTest";
    CHECK(RegisterClassExW(&klass) != 0);
    /* hidden, off screen, a tool window: the test must not flash on the desktop */
    g_hwnd = CreateWindowExW(0x00000080 /* WS_EX_TOOLWINDOW */ | 0x08000000 /* WS_EX_NOACTIVATE */, klass.lpszClassName, L"x104 integration test", WS_OVERLAPPEDWINDOW, -20000, -20000, 816, 639, NULL, NULL, klass.hInstance, NULL);
    CHECK(g_hwnd != NULL);
    x104.window = g_hwnd;

    /* x104_bootstrap step 8: async, input, panel, app init (this order) */
    CHECK(x104_async_init(g_hwnd));
    CHECK(x104_input_init(g_hwnd));
    CHECK(x104_panel_init(g_hwnd));
    CHECK(x104_app_install(x104.image));
    CHECK(x104_app_init(g_hwnd));
    x104_flag_set(&x104.f_units_ready, "f_units_ready");

    /* the on-device selftest, against the REAL units: nothing may fail and NOTHING may be skipped any more */
    selftest_failures = x104_selftest_fakes();
    CHECK(selftest_failures == 0);
    CHECK_MSG(x104_st_skip == 0, "a selftest check is still a SKIP: a unit is a stub or an object is missing");
    test_print("selftest against the REAL units: %d pass, %d fail, %d skip\n", x104_st_pass, x104_st_fail, x104_st_skip);

    watchdog = CreateThread(NULL, 0, watchdog_thread, NULL, 0, NULL);
    (void)watchdog;
    tester = CreateThread(NULL, 0, tester_thread, NULL, 0, NULL);

    /* the game's WinMain wrapper -> main -> Application::Start(callback): the real router, the real fake Start */
    {
        void *statics = mock_activate(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationStatics);
        CHECK(statics != NULL);
        start_hr = G(statics, IApplicationStatics, Start)(statics, &g_h_callback);
        CHECK(start_hr == S_OK);
    }
    test_print("Application::Start returned (stage %ld)\n", (long)g_stage);
    WaitForSingleObject(tester, 5000);

    /* x104_shutdown_units */
    InterlockedExchange(&x104.f_units_ready, 0);
    x104_input_shutdown();
    x104_panel_shutdown();
    x104_app_shutdown();
    x104_async_shutdown();
    x104_platform_shutdown();

    /* ---------------- verdicts ---------------- */
    CHECK(g_wait_failed == 0);
    CHECK(g_onlaunched_calls == 1 && g_prev_state == 0 && g_page_qi_ok && g_findname_ok);
    CHECK_MSG(g_wrong_thread == 0, "a game delegate ran off the UI thread");
    CHECK(g_launch_thread == (LONG)x104.ui_thread_id && g_ctor_thread == g_launch_thread);                  /* OnLaunched + MainPage ctor on the UI thread */
    /* ordering: OnLaunched < content set < first SizeChanged < first Tick < READY tick < attach < render loop (R2, R3) */
    CHECK(g_seq_launch < g_seq_content && g_seq_content < g_seq_size1 && g_seq_size1 < g_seq_tick_first && g_seq_tick_first <= g_seq_tick_ready && g_seq_tick_ready < g_seq_attach && g_seq_attach < g_seq_render);
    CHECK(g_tick_gate_violation == 0);
    CHECK(x104.f_mainpage_created && x104.f_content_set && x104.f_launched && x104.f_size_changed_done && x104.f_timer_started && x104.f_ready_tick && x104.f_exit_requested && x104.f_quit && x104.f_loop_exit);
    CHECK(g_first_size_w > 0 && g_first_size_h > 0);
    CHECK(g_native_calls >= 1);                                                                              /* the DR dispatcher lambda ran on the UI queue */
    CHECK(g_ready_ticks == 1);                                                                               /* the timer was stopped by the game: exactly one ready Tick */
    /* input */
    CHECK(g_pressed >= 1 && g_press_x == 100 && g_press_y == 200 && g_press_device == 2 /* mouse */ && g_press_left == 1);
    CHECK(g_released >= 1 && g_release_left == 0);                                                           /* IsLeftButtonPressed FALSE on Released */
    CHECK(g_moved >= 1);
    CHECK(g_pressed == 1 && g_pressed_src2 == 1);                                                            /* delivered once to the HID source, never doubled */
    CHECK(g_keydown >= 1 && g_key_vk == 0x57 && g_key_repeat == 1 && g_keyup >= 1);
    CHECK(g_back_events == 2);                                                                              /* Esc + one tap on the on-screen button */
    CHECK(g_button_hidden_without_world);                                                                   /* no world -> no button */
    CHECK(g_button_created && g_button_visible && g_button_top_right);                                      /* the button exists, is shown, top-left */
    CHECK(g_text_events == 2 && wcscmp_ok(g_text_seen, L"hellox"));                                          /* EXACTLY one TextChanged per modification (R14) */
    CHECK(g_pane_showing >= 1 && g_pane_height > 0 && g_pane_hiding >= 1);
    CHECK(text_input_active == 0);
    /* resize, visibility, render loop */
    CHECK(g_size_events >= 2 && g_last_size_w != g_first_size_w);
    CHECK(g_vis_events == 2 && g_vis_last == 1 && g_vis_unsafe == 0);
    CHECK(g_render_stopped_frames == 0);
    CHECK(g_render_starts == 2);
    /* exit */
    CHECK(g_susp_events == 1 && g_susp_chain_ok);
    CHECK(g_render_threads_running == 0);
    CHECK(x104_async_st.workers_running == 0);
    CHECK(g_input_items_done == g_workitem_threads);                                                         /* ProcessEvents(ProcessUntilQuit) returned on every input item at shutdown */
    CHECK(x104_async_st.shutdown_state == 2);
    /* the log must be clean of the lines the design calls failures */
    CHECK_MSG(log_count("X104 UNIMPLEMENTED") == 0, "a vtable hole was called");
    CHECK_MSG(log_count("X104 QI MISS") == 0, "a QI miss");
    CHECK_MSG(log_count("EXCEPTION escaped") == 0, "an exception escaped a guarded call");
    /* the only ERROR line this test legitimately provokes: it creates no swap chain, so the panel's 3 s rule explains the black window */
    CHECK_MSG(log_count("ERROR") - log_count("X104 panel: ERROR first SizeChanged returned") == 0, "an ERROR line");
    CHECK_MSG(log_count("underflow") == 0, "a refcount underflow");
    CHECK_MSG(log_count("dropped input before HID::Initialize") == 0, "input was dropped");
    CHECK_MSG(log_count("held back") == 0, "a lifecycle event was held back");
    CHECK_MSG(log_has("X104 app: Suspending raised to 1 handler(s)"), "Suspending was raised to the game");
    CHECK_MSG(log_has("X104 async: UI loop left"), "the loop left");
    CHECK_MSG(!log_has("Suspending skipped"), "Suspending was skipped");
    CHECK_MSG(g_modal_passes > 0, "the WM_TIMER pump never ran");

    failures = g_failures;
    {
        /* print every line the verdicts above consider a failure */
        const char *needles[] = { "ERROR", "UNIMPLEMENTED", "QI MISS", "EXCEPTION escaped", "underflow", "WARNING" };
        unsigned n;
        for (n = 0; n < ARRAYSIZE(needles); ++n) {
            const char *at = g_log;
            while ((at = strstr(at, needles[n])) != NULL) {
                const char *begin = at;
                const char *end = at;
                char line[400];
                int length;
                while (begin > g_log && begin[-1] != 0x0A) --begin;
                while (*end && *end != 0x0A) ++end;
                length = (int)(end - begin);
                if (length > 390) length = 390;
                memcpy(line, begin, (size_t)length);
                line[length] = 0;
                test_print("  >> %s\n", line);
                at = end;
            }
        }
    }
    if (failures || g_verbose) {
        int length = (int)g_log_len;
        int start = length > 12000 ? length - 12000 : 0;
        DWORD written;
        test_print("---- tail of the host log ----\n");
        WriteFile(GetStdHandle((DWORD)-11), g_log + start, (DWORD)(length - start), &written, NULL);
    }
    test_print("== integration test: %d checks, %d failures; frames=%ld size events=%ld modal passes=%ld ==\n", g_checks, failures, (long)g_frames, (long)g_size_events, (long)g_modal_passes);
    return failures ? 1 : 0;
}
