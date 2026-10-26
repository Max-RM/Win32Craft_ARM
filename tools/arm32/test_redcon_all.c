/* Host-side (x86, runs on the PC) INTEGRATION test of the x104 host with the REDCON 1 profile: the REAL core + async + app + panel + platform + input
 * units in ONE translation unit (the boot is replaced by a few lines in main), x104_profile = &x104_profile_redcon.
 *
 * A mock REDCON 1 makes the SAME calls through the SAME vtable slots in the SAME order as Redcon.exe 1.3.0.0 (games_recon/redcon_redcon-startup.md F2-F8):
 * Application::Start -> App ctor (IApplicationFactory, no IApplication events) -> OnLaunched (ApplicationView, StatusBar, SwapChainPanel FACTORY with NULL/NULL,
 * alignments, Window.put_Content(panel), get_CoreWindow -> SetWindow block: 11 CoreWindow events, BackPressed, DisplayInformation (+2), VibrationDevice,
 * NetworkInformation, CoreApplication Suspending/Resuming, Package / ApplicationData / ApplicationLanguages / GetConnectionProfiles, RoamingSettings, the store
 * ownership test, CompositionTarget.add_Rendering) -> Window.Activate; then frames come ONLY from CompositionTarget.Rendering on the UI thread.
 * A tester thread drives the real window procedure: mouse, a key outside Minecraft's whitelist, WM_CHAR, Esc, the on-screen Back button, resize
 * (-> CoreWindow.SizeChanged), minimise / restore (the game pauses / resumes its Rendering handler), WM_CLOSE (-> CoreApplication.Suspending).
 * Not covered: D3D/DXGI (no device, no swap chain), ARM register passing (--selftest-fakes on the device), the real game code.
 * Build + run:  python tools/arm32/test_0104_core.py --src tools/arm32/test_redcon_all.c      Exit code 0 = every check passed.
 * Generated from tools/arm32/test_0104_all.c (stand-ins, window routing, mock delegates) + the REDCON part: keep the shared part in sync. */
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
static void **arm32_iat_find_cell(BYTE *image, BOOL delay, const char *name) { (void)image; (void)delay; (void)name; return NULL; }
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

/* ======================================================================================================================
 * the mock REDCON 1 (games_recon/redcon_redcon-startup.md F2-F8: the calls, slots and order of Redcon.exe 1.3.0.0)
 * ====================================================================================================================== */
extern HRESULT WINAPI RoGetActivationFactory(HSTRING, const GUID *, void **);

static HRESULT harness_original_activation(const wchar_t *class_name, const GUID *iid, void **out)        /* __cdecl, like vccorlib's function */
{
    /* what vccorlib's GetActivationFactoryByPCWSTR does on the device: the OS runtime (works here for desktop-capable classes such as PropertySet) */
    HSTRING name = NULL;
    HRESULT hr;
    *out = NULL;
    if (FAILED(WindowsCreateString(class_name, (UINT32)lstrlenW(class_name), &name))) return E_OUTOFMEMORY;
    hr = RoGetActivationFactory(name, iid, out);
    WindowsDeleteString(name);
    return hr;
}

static void *mock_activate(const wchar_t *class_name, const GUID *iid)
{
    void *out = NULL;
    HRESULT hr = x104_activation_hook(class_name, iid, &out);
    if (FAILED(hr)) test_print("  mock activation of %S failed: 0x%08lx\n", class_name, (unsigned long)hr);
    return out;
}

static void *mock_qi_of(void *object, const GUID *iid)
{
    void *out = NULL;
    if (!object) return NULL;
    X104_VFN(object, 0, HRESULT (WINAPI *)(void *, const GUID *, void **))(object, iid, &out);
    return out;
}

typedef struct MockOuter { const void **vtbl; volatile LONG refs; void *overrides; } MockOuter;
static MockOuter g_app_outer;
static void *g_inner_app, *g_instance_app;
static void *g_core_window, *g_window, *g_panel, *g_panel_native, *g_ct_statics;
static LONGLONG g_rendering_token;
static volatile LONG g_seq;
static volatile LONG g_seq_launch_begin, g_seq_launch_end, g_seq_first_frame, g_seq_activate;
static volatile LONG g_frames, g_frames_wrong_thread, g_paused, g_resumed, g_render_registrations;
static volatile LONG g_pressed, g_released, g_moved, g_press_x, g_press_y, g_press_id;
static volatile LONG g_keydown, g_keyup, g_key_vk, g_key_was_down, g_chars, g_char_code, g_char_handled_ok;
static volatile LONG g_back_events, g_vis_events, g_vis_last, g_size_events, g_size_w, g_size_h;
static volatile LONG g_core_susp_events, g_susp_chain_ok, g_onlaunched_calls, g_content_flag_after_put;
static volatile LONG g_product_active, g_vector_size, g_raw_pixels_ok, g_vibration_ok, g_languages_hr, g_roaming_ok, g_alignment_ok, g_suppress_ok;
static int g_button_visible_running, g_button_hidden_default;
static volatile LONG g_wrong_thread;
#define SEQ(name) do { name = InterlockedIncrement(&g_seq); } while (0)
#define UI_ONLY() do { if (GetCurrentThreadId() != x104.ui_thread_id) InterlockedIncrement(&g_wrong_thread); } while (0)

static HRESULT mock_noop(MockDelegate *d, void *a, void *b) { (void)d; (void)a; (void)b; return S_OK; }
MOCK(g_h_noop, mock_noop);

/* CompositionTarget.Rendering = the frame (sub_6EE50): sender / args unused */
static HRESULT mock_on_rendering(MockDelegate *d, void *sender, void *args)
{
    LONG frame;
    (void)d; (void)sender; (void)args;
    if (GetCurrentThreadId() != x104.ui_thread_id) InterlockedIncrement(&g_frames_wrong_thread);
    frame = InterlockedIncrement(&g_frames);
    if (frame == 1) SEQ(g_seq_first_frame);
    Sleep(2);                                                                    /* "Present(1,0)" */
    return S_OK;
}
MOCK(g_h_rendering, mock_on_rendering);

static void mock_register_rendering(void)
{
    CHECK(SUCCEEDED(G(g_ct_statics, ICompositionTargetStatics, add_Rendering)(g_ct_statics, &g_h_rendering, &g_rendering_token)) && g_rendering_token != 0);
    InterlockedIncrement(&g_render_registrations);
}

/* VisibilityChanged (0x6D7F4): hidden -> pause (remove_Rendering), visible -> resume (add_Rendering) when paused */
static HRESULT mock_on_visibility(MockDelegate *d, void *sender, void *args)
{
    BYTE visible = 0xFF;
    (void)d; (void)sender;
    UI_ONLY();
    CHECK(SUCCEEDED(G(args, IVisibilityChangedEventArgs, get_Visible)(args, &visible)) && visible <= 1);
    InterlockedIncrement(&g_vis_events);
    g_vis_last = visible;
    if (!visible && g_rendering_token) {
        CHECK(SUCCEEDED(G(g_ct_statics, ICompositionTargetStatics, remove_Rendering)(g_ct_statics, g_rendering_token)));
        g_rendering_token = 0;
        InterlockedIncrement(&g_paused);
    } else if (visible && !g_rendering_token) {
        mock_register_rendering();
        InterlockedIncrement(&g_resumed);
    }
    return S_OK;
}
MOCK(g_h_visibility, mock_on_visibility);

/* SizeChanged (0x6D770): args ignored, CoreWindow.get_Bounds re-read */
static HRESULT mock_on_size(MockDelegate *d, void *sender, void *args)
{
    X104Rect bounds;
    (void)d; (void)sender; (void)args;
    UI_ONLY();
    memset(&bounds, 0, sizeof(bounds));
    CHECK(SUCCEEDED(G(g_core_window, ICoreWindow, get_Bounds)(g_core_window, &bounds)));
    g_size_w = (LONG)bounds.Width;
    g_size_h = (LONG)bounds.Height;
    InterlockedIncrement(&g_size_events);
    return S_OK;
}
MOCK(g_h_size, mock_on_size);

static void mock_read_pointer(void *args, volatile LONG *counter)
{
    void *point = NULL;
    X104Point position;
    UINT32 id = 0;
    UI_ONLY();
    CHECK(SUCCEEDED(G(args, IPointerEventArgs, get_CurrentPoint)(args, &point)) && point);
    if (!point) return;
    CHECK(SUCCEEDED(G(point, IPointerPoint, get_Position)(point, &position)));
    CHECK(SUCCEEDED(G(point, IPointerPoint, get_PointerId)(point, &id)));
    if (counter == &g_pressed) { g_press_x = (LONG)position.X; g_press_y = (LONG)position.Y; g_press_id = (LONG)id; }
    InterlockedIncrement(counter);
    x104_raw_release(point);
}
static HRESULT mock_ptr_pressed(MockDelegate *d, void *s, void *a) { (void)d; (void)s; mock_read_pointer(a, &g_pressed); return S_OK; }
static HRESULT mock_ptr_moved(MockDelegate *d, void *s, void *a) { (void)d; (void)s; mock_read_pointer(a, &g_moved); return S_OK; }
static HRESULT mock_ptr_released(MockDelegate *d, void *s, void *a) { (void)d; (void)s; mock_read_pointer(a, &g_released); return S_OK; }
MOCK(g_h_pressed, mock_ptr_pressed);
MOCK(g_h_moved, mock_ptr_moved);
MOCK(g_h_released, mock_ptr_released);

/* KeyDown (0x6DC5C): KeyStatus (WasKeyDown at +0xA), VirtualKey, QI ICoreWindowEventArgs -> put_Handled(true) */
static HRESULT mock_on_key(MockDelegate *d, void *sender, void *args, BOOL down)
{
    BYTE status[16];
    int vk = 0;
    void *cwargs;
    (void)d; (void)sender;
    UI_ONLY();
    memset(status, 0, sizeof(status));
    if (down) {
        CHECK(SUCCEEDED(G(args, IKeyEventArgs, get_KeyStatus)(args, status)));
        g_key_was_down = status[0x0A];
    }
    CHECK(SUCCEEDED(G(args, IKeyEventArgs, get_VirtualKey)(args, &vk)));
    g_key_vk = vk;
    cwargs = mock_qi_of(args, &X104_IID_ICoreWindowEventArgs);
    CHECK(cwargs != NULL);
    if (cwargs) { G(cwargs, ICoreWindowEventArgs, put_Handled)(cwargs, 1); x104_raw_release(cwargs); }
    InterlockedIncrement(down ? &g_keydown : &g_keyup);
    return S_OK;
}
static HRESULT mock_on_key_down(MockDelegate *d, void *s, void *a) { return mock_on_key(d, s, a, TRUE); }
static HRESULT mock_on_key_up(MockDelegate *d, void *s, void *a) { return mock_on_key(d, s, a, FALSE); }
MOCK(g_h_key_down, mock_on_key_down);
MOCK(g_h_key_up, mock_on_key_up);

/* CharacterReceived (0x6DEB0): QI ICoreWindowEventArgs -> put_Handled, ICharacterReceivedEventArgs.get_KeyCode */
static HRESULT mock_on_char(MockDelegate *d, void *sender, void *args)
{
    void *cwargs, *cargs;
    UINT32 code = 0;
    (void)d; (void)sender;
    UI_ONLY();
    cwargs = mock_qi_of(args, &X104_IID_ICoreWindowEventArgs);
    cargs = mock_qi_of(args, &X104_IID_ICharacterReceivedEventArgs);
    g_char_handled_ok = cwargs && cargs && SUCCEEDED(G(cwargs, ICoreWindowEventArgs, put_Handled)(cwargs, 1));
    if (cargs) CHECK(SUCCEEDED(G(cargs, ICharacterReceivedEventArgs, get_KeyCode)(cargs, &code)));
    g_char_code = (LONG)code;
    InterlockedIncrement(&g_chars);
    if (cwargs) x104_raw_release(cwargs);
    if (cargs) x104_raw_release(cargs);
    return S_OK;
}
MOCK(g_h_char, mock_on_char);

/* BackPressed (0x6DB48): push key 2 into the ring, ALWAYS Handled = true */
static HRESULT mock_on_back(MockDelegate *d, void *sender, void *args)
{
    (void)d; (void)sender;
    UI_ONLY();
    CHECK(SUCCEEDED(G(args, IBackPressedEventArgs, put_Handled)(args, 1)));
    InterlockedIncrement(&g_back_events);
    return S_OK;
}
MOCK(g_h_back, mock_on_back);

/* CoreApplication.Suspending (0x6D580): args -> SuspendingOperation -> GetDeferral -> Complete; pause rendering */
static HRESULT mock_on_suspending(MockDelegate *d, void *sender, void *args)
{
    void *operation = NULL, *deferral = NULL;
    (void)d; (void)sender;
    UI_ONLY();
    InterlockedIncrement(&g_core_susp_events);
    if (g_rendering_token) {
        G(g_ct_statics, ICompositionTargetStatics, remove_Rendering)(g_ct_statics, g_rendering_token);
        g_rendering_token = 0;
    }
    g_susp_chain_ok = SUCCEEDED(G(args, ISuspendingEventArgs, get_SuspendingOperation)(args, &operation)) && operation &&
                      SUCCEEDED(G(operation, ISuspendingOperation, GetDeferral)(operation, &deferral)) && deferral &&
                      SUCCEEDED(G(deferral, ISuspendingDeferral, Complete)(deferral));
    return S_OK;
}
MOCK(g_h_suspending, mock_on_suspending);

/* SetWindow block sub_6CF3C + game init sub_86E0C + "start" sub_6C808 */
static void mock_set_window(void *core)
{
    void *hw, *display_statics, *display = NULL, *display2, *vib_statics, *vib = NULL, *net, *coreapp, *package_statics, *package = NULL, *folder = NULL, *item;
    void *appdata_statics, *appdata = NULL, *profiles = NULL, *languages = NULL, *roaming = NULL, *values = NULL, *store, *license = NULL, *licenses = NULL, *product = NULL;
    LONGLONG token = 0;
    float dpi = 0.0f;
    double raw = 0.0;
    UINT32 size = 99;
    BYTE active = 0xFF;
    HSTRING path = NULL, key;
    wchar_t wide[MAX_PATH];

    g_core_window = core;
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_Activated)(core, &g_h_noop, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_SizeChanged)(core, &g_h_size, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_VisibilityChanged)(core, &g_h_visibility, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_Closed)(core, &g_h_noop, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_PointerPressed)(core, &g_h_pressed, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_PointerReleased)(core, &g_h_released, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_PointerCaptureLost)(core, &g_h_released, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_PointerMoved)(core, &g_h_moved, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_KeyDown)(core, &g_h_key_down, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_KeyUp)(core, &g_h_key_up, &token)) && token);
    CHECK(SUCCEEDED(G(core, ICoreWindow, add_CharacterReceived)(core, &g_h_char, &token)) && token);
    hw = mock_activate(L"Windows.Phone.UI.Input.HardwareButtons", &X104_IID_IHardwareButtonsStatics);
    CHECK(hw && SUCCEEDED(G(hw, IHardwareButtonsStatics, add_BackPressed)(hw, &g_h_back, &token)) && token);
    display_statics = mock_activate(L"Windows.Graphics.Display.DisplayInformation", &X104_IID_IDisplayInformationStatics);
    CHECK(display_statics && SUCCEEDED(G(display_statics, IDisplayInformationStatics, GetForCurrentView)(display_statics, &display)) && display);
    CHECK(SUCCEEDED(G(display_statics, IDisplayInformationStatics, put_AutoRotationPreferences)(display_statics, 5)));
    CHECK(SUCCEEDED(G(display, IDisplayInformation, add_OrientationChanged)(display, &g_h_noop, &token)));
    CHECK(SUCCEEDED(G(display, IDisplayInformation, add_DpiChanged)(display, &g_h_noop, &token)));
    CHECK(SUCCEEDED(G(display, IDisplayInformation, get_LogicalDpi)(display, &dpi)) && dpi > 0.0f);
    vib_statics = mock_activate(L"Windows.Phone.Devices.Notification.VibrationDevice", &X104_IID_IVibrationDeviceStatics);
    g_vibration_ok = vib_statics && SUCCEEDED(G(vib_statics, IVibrationDeviceStatics, GetDefault)(vib_statics, &vib)) && vib != NULL;   /* stored, used later without a NULL check */
    net = mock_activate(L"Windows.Networking.Connectivity.NetworkInformation", &X104_IID_INetworkInformationStatics);
    CHECK(net && SUCCEEDED(G(net, INetworkInformationStatics, add_NetworkStatusChanged)(net, &g_h_noop, &token)) && token);
    coreapp = mock_activate(L"Windows.ApplicationModel.Core.CoreApplication", &X104_IID_ICoreApplication);
    CHECK(coreapp && SUCCEEDED(G(coreapp, ICoreApplication, add_Resuming)(coreapp, &g_h_noop, &token)) && token);
    CHECK(coreapp && SUCCEEDED(G(coreapp, ICoreApplication, add_Suspending)(coreapp, &g_h_suspending, &token)) && token);

    /* game init sub_86E0C: InstalledLocation, LocalFolder, Languages, first-run udid (GetConnectionProfiles) */
    package_statics = mock_activate(L"Windows.ApplicationModel.Package", &X104_IID_IPackageStatics);
    CHECK(package_statics && SUCCEEDED(G(package_statics, IPackageStatics, get_Current)(package_statics, &package)) && package);
    CHECK(SUCCEEDED(G(package, IPackage, get_InstalledLocation)(package, &folder)) && folder);
    item = mock_qi_of(folder, &X104_IID_IStorageItem);
    CHECK(item && SUCCEEDED(G(item, IStorageItem, get_Path)(item, &path)) && path);
    x104_hstring_wide(path, wide, ARRAYSIZE(wide));
    CHECK(lstrcmpiW(wide, x104.game_root) == 0);
    x104_hstring_free(path);
    appdata_statics = mock_activate(L"Windows.Storage.ApplicationData", &X104_IID_IApplicationDataStatics);
    CHECK(appdata_statics && SUCCEEDED(G(appdata_statics, IApplicationDataStatics, get_Current)(appdata_statics, &appdata)) && appdata);
    folder = NULL;
    CHECK(SUCCEEDED(G(appdata, IApplicationData, get_LocalFolder)(appdata, &folder)) && folder);
    item = mock_qi_of(folder, &X104_IID_IStorageItem);
    path = NULL;
    CHECK(item && SUCCEEDED(G(item, IStorageItem, get_Path)(item, &path)));
    x104_hstring_wide(path, wide, ARRAYSIZE(wide));
    CHECK(lstrcmpiW(wide, x104.data_root) == 0);
    x104_hstring_free(path);
    {
        /* ApplicationLanguages: left to the OS runtime (a desktop process may use it); here through harness_original_activation */
        void *statics = NULL;
        g_languages_hr = (LONG)x104_activation_hook(L"Windows.Globalization.ApplicationLanguages", &X104_IID_IInspectable, &statics);
        (void)languages;
        if (statics) x104_raw_release(statics);
    }
    CHECK(SUCCEEDED(G(net, INetworkInformationStatics, GetConnectionProfiles)(net, &profiles)) && profiles);
    if (profiles) {
        CHECK(SUCCEEDED(X104_VFN(profiles, X104_SLOT_IVectorView_get_Size, X104_FN_IVectorView_get_Size)(profiles, &size)));
        g_vector_size = (LONG)size;
    }
    display2 = mock_qi_of(display, &X104_IID_IDisplayInformation2);
    g_raw_pixels_ok = display2 && SUCCEEDED(G(display2, IDisplayInformation2, get_RawPixelsPerViewPixel)(display2, &raw)) && raw == 1.0;

    /* lazy paths exercised here too: RoamingSettings.Values (map) and the store ownership test (ProductLicenses.Lookup(id).IsActive) */
    g_roaming_ok = SUCCEEDED(G(appdata, IApplicationData, get_RoamingSettings)(appdata, &roaming)) && roaming &&
                   SUCCEEDED(G(roaming, IApplicationDataContainer, get_Values)(roaming, &values)) && values != NULL;
    store = mock_activate(L"Windows.ApplicationModel.Store.CurrentApp", &X104_IID_ICurrentApp);
    CHECK(store && SUCCEEDED(G(store, ICurrentApp, get_LicenseInformation)(store, &license)) && license);
    CHECK(SUCCEEDED(G(license, ILicenseInformation, get_ProductLicenses)(license, &licenses)) && licenses);
    key = mock_hs(L"REDCON1");
    if (licenses) CHECK(SUCCEEDED(X104_VFN(licenses, X104_SLOT_IMapView_Lookup, X104_FN_IMapView_Lookup)(licenses, key, &product)) && product);
    x104_hstring_free(key);
    if (product) CHECK(SUCCEEDED(G(product, IProductLicense, get_IsActive)(product, &active)));
    g_product_active = active;

    /* "start" sub_6C808 -> resume rendering sub_6CA90: add_Rendering */
    g_ct_statics = mock_activate(L"Windows.UI.Xaml.Media.CompositionTarget", &X104_IID_ICompositionTargetStatics);
    CHECK(g_ct_statics != NULL);
    mock_register_rendering();
}

/* App::OnLaunched sub_6EBD0 */
static HRESULT WINAPI mock_on_launched(void *self, void *args, void *unused3)
{
    void *view_statics, *view = NULL, *view2, *statusbar_statics, *statusbar = NULL, *action = NULL, *panel_factory, *fe, *ui, *window_statics, *core = NULL;
    void *inner = NULL;
    (void)self; (void)args; (void)unused3;
    InterlockedIncrement(&g_onlaunched_calls);
    SEQ(g_seq_launch_begin);
    UI_ONLY();
    view_statics = mock_activate(L"Windows.UI.ViewManagement.ApplicationView", &X104_IID_IApplicationViewStatics2);
    CHECK(view_statics && SUCCEEDED(G(view_statics, IApplicationViewStatics2, GetForCurrentView)(view_statics, &view)) && view);
    view2 = mock_qi_of(view, &X104_IID_IApplicationView2);
    g_suppress_ok = view2 && SUCCEEDED(G(view2, IApplicationView2, put_SuppressSystemOverlays)(view2, 1));
    statusbar_statics = mock_activate(L"Windows.UI.ViewManagement.StatusBar", &X104_IID_IStatusBarStatics);
    CHECK(statusbar_statics && SUCCEEDED(G(statusbar_statics, IStatusBarStatics, GetForCurrentView)(statusbar_statics, &statusbar)) && statusbar);
    CHECK(SUCCEEDED(G(statusbar, IStatusBar, HideAsync)(statusbar, &action)) && action);
    x104_raw_release(action);
    panel_factory = mock_activate(L"Windows.UI.Xaml.Controls.SwapChainPanel", &X104_IID_ISwapChainPanelFactory);
    CHECK(panel_factory && SUCCEEDED(G(panel_factory, ISwapChainPanelFactory, CreateInstance)(panel_factory, NULL, NULL, &g_panel)) && g_panel);
    (void)inner;
    fe = mock_qi_of(g_panel, &X104_IID_IFrameworkElement);
    g_alignment_ok = fe && SUCCEEDED(G(fe, IFrameworkElement, put_HorizontalAlignment)(fe, 3)) && SUCCEEDED(G(fe, IFrameworkElement, put_VerticalAlignment)(fe, 3));
    g_panel_native = mock_qi_of(g_panel, &X104_IID_ISwapChainPanelNative);
    CHECK(g_panel_native != NULL && mock_qi_of(g_panel, &X104_IID_ISwapChainPanel) != NULL);
    ui = mock_qi_of(g_panel, &X104_IID_IUIElement);
    window_statics = mock_activate(L"Windows.UI.Xaml.Window", &X104_IID_IWindowStatics);
    CHECK(window_statics && SUCCEEDED(G(window_statics, IWindowStatics, get_Current)(window_statics, &g_window)) && g_window);
    CHECK(G(g_window, IWindow, put_Content)(g_window, ui) == S_OK);
    g_content_flag_after_put = x104.f_content_set;
    CHECK(SUCCEEDED(G(g_window, IWindow, get_CoreWindow)(g_window, &core)) && core);
    mock_set_window(core);
    SEQ(g_seq_activate);
    CHECK(G(g_window, IWindow, Activate)(g_window) == S_OK);
    SEQ(g_seq_launch_end);
    return S_OK;
}

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
    return X104_VFN(g_inner_app, 0, HRESULT (WINAPI *)(void *, const GUID *, void **))(g_inner_app, iid, out);
}
static ULONG WINAPI mock_outer_addref(void *self) { return (ULONG)InterlockedIncrement(&((MockOuter *)self)->refs); }
static ULONG WINAPI mock_outer_release(void *self) { return (ULONG)InterlockedDecrement(&((MockOuter *)self)->refs); }
static const void *mock_outer_vtbl[3] = { mock_outer_qi, mock_outer_addref, mock_outer_release };
static const void *mock_overrides_vtbl[16] = { mock_outer_qi, mock_outer_addref, mock_outer_release, NULL, NULL, NULL, NULL, mock_on_launched };
static MockOuter g_overrides_object = { mock_overrides_vtbl, 1, NULL };

/* ApplicationInitializationCallback -> App ctor sub_6CDF0: IApplicationFactory.CreateInstance(outer = App+4, &inner, &instance); NO IApplication event here */
static HRESULT mock_callback_invoke(MockDelegate *self, void *sender, void *args)
{
    void *factory;
    (void)self; (void)sender; (void)args;
    factory = mock_activate(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationFactory);
    CHECK(factory != NULL);
    g_app_outer.vtbl = mock_outer_vtbl;
    g_app_outer.refs = 1;
    g_app_outer.overrides = &g_overrides_object;
    CHECK(SUCCEEDED(G(factory, IApplicationFactory, CreateInstance)(factory, &g_app_outer, &g_inner_app, &g_instance_app)) && g_inner_app && g_instance_app);
    return S_OK;
}
MOCK(g_h_callback, mock_callback_invoke);

/* ======================================================================================================================
 * the tester thread
 * ====================================================================================================================== */
static HWND g_hwnd;
static volatile LONG g_stage;
static int g_wait_failed;
static LONG g_frames_while_hidden = -1;

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

static DWORD WINAPI tester_thread(void *argument)
{
    RECT rect;
    int width, height;
    (void)argument;
    g_stage = 1;
    wait_until(&x104.f_launched, 1, 15000, "f_launched");
    wait_until(&g_frames, 30, 15000, "30 Rendering frames");

    g_stage = 2;                                                          /* mouse */
    SendMessageW(g_hwnd, 0x0201 /* WM_LBUTTONDOWN */, 1, MAKELONG(100, 200));
    wait_until(&g_pressed, 1, 2000, "PointerPressed");
    SendMessageW(g_hwnd, 0x0200 /* WM_MOUSEMOVE */, 1, MAKELONG(150, 220));
    wait_until(&g_moved, 1, 2000, "PointerMoved");
    SendMessageW(g_hwnd, 0x0202 /* WM_LBUTTONUP */, 0, MAKELONG(150, 220));
    wait_until(&g_released, 1, 2000, "PointerReleased");

    g_stage = 3;                                                          /* keys, characters, Back */
    SendMessageW(g_hwnd, 0x0100 /* WM_KEYDOWN */, 0x70 /* F1: not in Minecraft's whitelist */, 0x003B0001);
    wait_until(&g_keydown, 1, 2000, "KeyDown");
    SendMessageW(g_hwnd, 0x0101 /* WM_KEYUP */, 0x70, (LPARAM)0xC03B0001);
    wait_until(&g_keyup, 1, 2000, "KeyUp");
    SendMessageW(g_hwnd, 0x0102 /* WM_CHAR */, (WPARAM)L'x', 1);
    wait_until(&g_chars, 1, 2000, "CharacterReceived");
    SendMessageW(g_hwnd, 0x0100, 0x1B /* VK_ESCAPE = Back */, 0x00010001);
    wait_until(&g_back_events, 1, 2000, "BackPressed");

    g_stage = 4;                                                          /* the on-screen Back button: hidden by default (the game has its own back UI), --back-button-always shows it */
    Sleep(150);
    g_button_hidden_default = x104_backbtn.hwnd && !IsWindowVisible(x104_backbtn.hwnd) ? 1 : 0;
    x104_backbtn.always = TRUE;
    Sleep(150);
    if (x104_backbtn.hwnd) {
        g_button_visible_running = IsWindowVisible(x104_backbtn.hwnd) ? 1 : 0;
        SendMessageW(x104_backbtn.hwnd, 0x0201, 1, (LPARAM)((5 << 16) | 5));
        SendMessageW(x104_backbtn.hwnd, 0x0202, 0, (LPARAM)((5 << 16) | 5));
        wait_until(&g_back_events, 2, 2000, "BackPressed from the on-screen button");
    }

    g_stage = 5;                                                          /* resize -> CoreWindow.SizeChanged */
    GetClientRect(g_hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    SetWindowPos(g_hwnd, NULL, 0, 0, width + 200 + 16, height + 100 + 39, 0x0016);
    wait_until(&g_size_events, 1, 3000, "CoreWindow.SizeChanged after the resize");

    g_stage = 6;                                                          /* minimise / restore: the game pauses and resumes its frames */
    ShowWindow(g_hwnd, 6 /* SW_MINIMIZE */);
    wait_until(&g_paused, 1, 3000, "VisibilityChanged(false) -> remove_Rendering");
    Sleep(100);
    {
        LONG before = g_frames;
        Sleep(250);
        g_frames_while_hidden = g_frames - before;
    }
    ShowWindow(g_hwnd, 9 /* SW_RESTORE */);
    wait_until(&g_resumed, 1, 3000, "VisibilityChanged(true) -> add_Rendering");
    {
        LONG before = g_frames;
        wait_until(&g_frames, before + 20, 3000, "frames after restore");
    }

    g_stage = 7;                                                          /* close -> CoreApplication.Suspending -> loop leaves */
    SendMessageW(g_hwnd, WM_CLOSE, 0, 0);
    return 0;
}

static DWORD WINAPI watchdog_thread(void *argument)
{
    (void)argument;
    Sleep(60000);
    test_print("WATCHDOG: the REDCON integration test did not finish in 60 s (stage %ld) - last log lines follow\n", (long)g_stage);
    {
        int length = (int)g_log_len;
        int start = length > 6000 ? length - 6000 : 0;
        DWORD written;
        WriteFile(GetStdHandle((DWORD)-11), g_log + start, (DWORD)(length - start), &written, NULL);
    }
    TerminateProcess(GetCurrentProcess(), 99);
    return 0;
}

int main(int argc, char **argv)
{
    WNDCLASSEXW klass;
    HANDLE tester, watchdog;
    int failures;
    static wchar_t cwd[MAX_PATH];
    HRESULT start_hr;
    (void)argv;
    g_verbose = argc > 1;

    x104_profile = &x104_profile_redcon;                                  /* what x104_image_matches selects for the patched Redcon.exe */
    memset(&x104, 0, sizeof(x104));
    x104.main_thread_id = GetCurrentThreadId();
    x104.ui_thread_id = x104.main_thread_id;
    x104.opt_width = 800;
    x104.opt_height = 600;
    x104.original_activation = (void *)harness_original_activation;
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
    klass.lpszClassName = L"X104RedconIntegrationTest";
    CHECK(RegisterClassExW(&klass) != 0);
    g_hwnd = CreateWindowExW(0x00000080 | 0x08000000, klass.lpszClassName, L"x104 REDCON integration test", WS_OVERLAPPEDWINDOW, -20000, -20000, 816, 639, NULL, NULL,
                             klass.hInstance, NULL);
    CHECK(g_hwnd != NULL);
    x104.window = g_hwnd;

    CHECK(x104_async_init(g_hwnd));
    CHECK(x104_input_init(g_hwnd));
    CHECK(x104_panel_init(g_hwnd));
    CHECK(x104_app_install(x104.image));
    CHECK(x104_app_init(g_hwnd));
    x104_flag_set(&x104.f_units_ready, "f_units_ready");

    /* start_selftest.bat under the REDCON profile: nothing may fail or be skipped */
    {
        int selftest_failures = x104_selftest_fakes();
        CHECK(selftest_failures == 0);
        CHECK_MSG(x104_st_skip == 0, "a selftest check is a SKIP under the REDCON profile");
        test_print("selftest against the REAL units (REDCON profile): %d pass, %d fail, %d skip\n", x104_st_pass, x104_st_fail, x104_st_skip);
    }

    watchdog = CreateThread(NULL, 0, watchdog_thread, NULL, 0, NULL);
    (void)watchdog;
    tester = CreateThread(NULL, 0, tester_thread, NULL, 0, NULL);
    {
        void *statics = mock_activate(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationStatics);
        CHECK(statics != NULL);
        start_hr = G(statics, IApplicationStatics, Start)(statics, &g_h_callback);
        CHECK(start_hr == S_OK);
    }
    test_print("Application::Start returned (stage %ld)\n", (long)g_stage);
    WaitForSingleObject(tester, 5000);
    InterlockedExchange(&x104.f_units_ready, 0);
    x104_input_shutdown();
    x104_panel_shutdown();
    x104_app_shutdown();
    x104_async_shutdown();
    x104_platform_shutdown();

    /* ---------------- verdicts ---------------- */
    CHECK(g_wait_failed == 0);
    CHECK(g_onlaunched_calls == 1);
    CHECK_MSG(g_wrong_thread == 0 && g_frames_wrong_thread == 0, "a game delegate ran off the UI thread");
    CHECK_MSG(g_seq_launch_begin < g_seq_activate && g_seq_activate < g_seq_launch_end && g_seq_launch_end < g_seq_first_frame, "frames only after OnLaunched returned");
    CHECK_MSG(!g_content_flag_after_put && !x104.f_content_set && !x104.f_mainpage_created, "Window.put_Content of a panel is not the MainPage signal");
    CHECK(x104.f_launched && x104.f_size_changed_done && x104.f_exit_requested && x104.f_quit && x104.f_loop_exit);
    CHECK(g_suppress_ok && g_alignment_ok && g_raw_pixels_ok && g_vibration_ok);
    CHECK_MSG(g_vector_size == 0, "GetConnectionProfiles: empty vector");
    CHECK_MSG(SUCCEEDED((HRESULT)g_languages_hr), "ApplicationLanguages through the OS runtime");
    CHECK_MSG(g_roaming_ok, "RoamingSettings.Values = OS PropertySet");
    CHECK_MSG(g_product_active == 0, "ProductLicenses.Lookup(any id).IsActive == FALSE (nothing owned)");
    CHECK(g_pressed >= 1 && g_press_x == 100 && g_press_y == 200 && g_moved >= 1 && g_released >= 1);
    CHECK_MSG(g_keydown >= 1 && g_keyup >= 1 && g_key_was_down == 0 && g_key_vk == 0x70, "every key reaches a non-Minecraft game");
    CHECK_MSG(g_chars == 1 && g_char_code == (LONG)L'x' && g_char_handled_ok, "WM_CHAR -> CoreWindow.CharacterReceived");
    CHECK_MSG(g_button_hidden_default, "the on-screen Back button is hidden by default for REDCON");
    CHECK_MSG(g_back_events == 2 && g_button_visible_running, "Esc + one tap on the on-screen button (--back-button-always)");
    CHECK_MSG(g_size_events >= 1 && g_size_w > 800 && g_size_h > 600, "CoreWindow.SizeChanged with the new Bounds");
    CHECK_MSG(g_paused >= 1 && g_resumed >= 1 && g_frames_while_hidden == 0, "no frames while minimised");
    CHECK_MSG(g_render_registrations >= 2, "the game re-registered Rendering after the restore");
    CHECK_MSG(g_core_susp_events == 1 && g_susp_chain_ok, "CoreApplication.Suspending with a deferral on exit");
    CHECK_MSG(log_count("X104 UNIMPLEMENTED") == 0, "a vtable hole was called");
    CHECK_MSG(log_count("X104 QI MISS") == 0, "a QI miss");
    CHECK_MSG(log_count("EXCEPTION escaped") == 0, "an exception escaped a guarded call");
    CHECK_MSG(log_count("ERROR") - log_count("X104 panel: ERROR first SizeChanged returned") == 0, "an ERROR line");
    CHECK_MSG(log_count("dropped input before HID::Initialize") == 0, "input was dropped");
    CHECK_MSG(log_count("held back") == 0, "a lifecycle event was held back");
    CHECK_MSG(log_has("X104 app: first CompositionTarget.Rendering tick"), "the Rendering tick ran");
    CHECK_MSG(log_has("X104 async: UI loop left"), "the loop left");

    failures = g_failures;
    {
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
    test_print("== REDCON integration test: %d checks, %d failures; frames=%ld size events=%ld modal passes=%ld ==\n", g_checks, failures, (long)g_frames,
               (long)g_size_events, (long)g_modal_passes);
    return failures ? 1 : 0;
}
