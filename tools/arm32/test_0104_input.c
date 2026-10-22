/* Host-side (x86, runs on the PC) logic test of win32_host_arm32_0104_input.inl: object/QI matrix, event registration, pointer paths (WM_POINTER / mouse),
 * coalesced moves, R13 releases, key filter + Back, the TextBox model (TextChanged exactly once, Enter = CR LF, MaxLength), InputPane raise, cursor.
 * Build + run:  python tools/arm32/test_0104_core.py --src tools/arm32/test_0104_input.c
 * The test supplies stand-ins for the win32_host.c helpers the unit uses; the real core and the pristine stubs of the other units are compiled in.
 * COM thunks are WINAPI, so the vtable calls through mock delegates work on x86 as they do on ARM. */
#include "win32_compat.h"

extern void *WINAPI GetStdHandle(DWORD);
extern char *strstr(const char *, const char *);
extern int strcmp(const char *, const char *);
extern int WINAPI wvsprintfA(LPSTR, LPCSTR, __builtin_va_list);
extern void WINAPI RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR *);

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

/* ---- the log: a ring of the last 96 lines so a test can ask "was X logged" ---- */
static char g_ring[96][400];
static int g_ring_count;
static void log_line(const char *text)
{
    lstrcpynA(g_ring[g_ring_count % 96], text, sizeof(g_ring[0]));
    ++g_ring_count;
}
static BOOL log_has(const char *needle)
{
    int index;
    int first = g_ring_count > 96 ? g_ring_count - 96 : 0;
    for (index = first; index < g_ring_count; ++index) {
        if (strstr(g_ring[index % 96], needle)) return TRUE;
    }
    return FALSE;
}
static void log_hresult(const char *operation, HRESULT result) { (void)operation; (void)result; }
extern HRESULT WINAPI WindowsDeleteString(HSTRING);
extern BOOL WINAPI WindowsIsStringEmpty(HSTRING);

/* ---- stand-ins for win32_host.c ---- */
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
static int g_osk_show, g_osk_hide, g_osk_poll, g_osk_pointer_up, g_note_touch, g_note_mouse;
static BOOL g_osk_rect_ok;
static float g_osk_rect[4];
static BOOL g_osk_visible;
static void arm32_osk_show(const char *why) { (void)why; ++g_osk_show; }
static void arm32_osk_hide(const char *why) { (void)why; ++g_osk_hide; }
static void arm32_osk_poll(void) { ++g_osk_poll; }
static BOOL arm32_osk_rect_buffer(float rect[4]) { if (!g_osk_rect_ok) return FALSE; memcpy(rect, g_osk_rect, sizeof(g_osk_rect)); return TRUE; }
static void arm32_osk_note_pointer_up(void) { ++g_osk_pointer_up; }
static void arm32_input_note_source(int source, const char *why) { (void)why; if (source == ARM32_SRC_TOUCH) ++g_note_touch; else ++g_note_mouse; }
static BOOL arm32_input_touch_active(void) { return FALSE; }
static BOOL arm32_osk_visible_now(void) { return g_osk_visible; }
static BOOL text_input_active;
static volatile LONG arm32_boot_frames_started, arm32_boot_frames_done, arm32_boot_progress, arm32_boot_loop_passes;
static DWORD arm32_boot_start_tick;
static void arm32_boot_note_frame(LONG frame, DWORD loop_start, DWORD frame_ms) { (void)frame; (void)loop_start; (void)frame_ms; }
/* the input-specific shared state of win32_host.c */
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
static int g_mock_api = ARM32_TOUCHAPI_POINTER;
static int g_attach_calls;
static void arm32_input_attach_window(HWND window) { (void)window; ++g_attach_calls; arm32_touch_api_effective = g_mock_api; }
static BOOL arm32_pointer_is_touch(UINT32 id, UINT32 *type_out) { if (type_out) *type_out = id < 100 ? 2u : 4u; return id < 100; }
static BOOL arm32_pointer_nontouch_known(UINT32 id) { return id >= 100; }

#include "win32_host_arm32_0104_core.inl"
#define x104_ui_post x104_stub_ui_post
#define x104_ui_pump_once x104_stub_ui_pump_once
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_async.inl"
#undef x104_ui_post
#undef x104_ui_pump_once
/* harness queue standing in for the async unit's UI queue (single threaded) */
static void (*g_queue_fn[64])(void *);
static void *g_queue_arg[64];
static int g_queue_count;
static BOOL g_queue_closed;
static BOOL x104_ui_post(void (*function)(void *), void *argument)
{
    if (g_queue_closed || g_queue_count >= 64) return FALSE;
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
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_app.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_panel.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_platform.inl"
#include "win32_host_arm32_0104_input.inl"

/* the stub app unit's x104_app_raise_back_pressed returns FALSE and logs; the test counts through the log ring */

/* ---- mock delegates: they behave like the game's handlers (read the args through the vtable slots straight on the pointer) ---- */
typedef struct MockHandler { const void **vtbl; LONG refs; int kind; } MockHandler;
enum { H_POINTER = 1, H_KEY, H_VIS, H_TEXT, H_PANE };
typedef struct Recorded { int kind; int what; UINT32 id; int device; float x, y; BYTE left; BYTE contact; int vk; UINT32 repeat; BYTE visible; float pane_h; } Recorded;
static Recorded g_rec[256];
static int g_rec_count;
static int g_cur_event;             /* 0 moved, 1 pressed, 2 released: which add_ the handler was registered with, set in the handler itself */

static HRESULT WINAPI mh_qi(void *self, const GUID *iid, void **out) { (void)iid; *out = self; ((MockHandler *)self)->refs++; return S_OK; }
static ULONG WINAPI mh_addref(void *self) { return (ULONG)++((MockHandler *)self)->refs; }
static ULONG WINAPI mh_release(void *self) { return (ULONG)--((MockHandler *)self)->refs; }
#define VCALL(obj, slot, type) ((type)(((void **)*(void ***)(obj))[(slot)]))
static HRESULT WINAPI mh_invoke(void *self, void *sender, void *args)
{
    MockHandler *handler = (MockHandler *)self;
    Recorded *r;
    (void)sender;
    if (g_rec_count >= 256) return S_OK;
    r = &g_rec[g_rec_count++];
    memset(r, 0, sizeof(*r));
    r->kind = handler->kind;
    r->what = g_cur_event;
    if (handler->kind == H_POINTER) {
        void *point = NULL, *device = NULL, *props = NULL;
        X104Point position = { -1.0f, -1.0f };
        int dev = -1;
        BYTE left = 99;
        UINT32 id = 0xDEAD;
        BYTE contact = 99;
        /* exactly what HID::onPointerPressed does: straight calls, no QI */
        CHECK(VCALL(args, 6, HRESULT (WINAPI *)(void *, void **))(args, &point) == S_OK);
        CHECK(VCALL(point, 9, HRESULT (WINAPI *)(void *, UINT32 *))(point, &id) == S_OK);
        CHECK(VCALL(point, 7, HRESULT (WINAPI *)(void *, X104Point *))(point, &position) == S_OK);
        CHECK(VCALL(point, 6, HRESULT (WINAPI *)(void *, void **))(point, &device) == S_OK);
        CHECK(VCALL(device, 6, HRESULT (WINAPI *)(void *, int *))(device, &dev) == S_OK);
        CHECK(VCALL(point, 13, HRESULT (WINAPI *)(void *, void **))(point, &props) == S_OK);
        CHECK(VCALL(props, 16, HRESULT (WINAPI *)(void *, BYTE *))(props, &left) == S_OK);
        CHECK(VCALL(point, 12, HRESULT (WINAPI *)(void *, BYTE *))(point, &contact) == S_OK);
        r->id = id;
        r->x = position.X;
        r->y = position.Y;
        r->device = dev;
        r->left = left;
        r->contact = contact;
        VCALL(device, 2, ULONG (WINAPI *)(void *))(device);
        VCALL(props, 2, ULONG (WINAPI *)(void *))(props);
        VCALL(point, 2, ULONG (WINAPI *)(void *))(point);
    } else if (handler->kind == H_KEY) {
        int vk = -1;
        BYTE status[16];
        memset(status, 0xCC, sizeof(status));
        CHECK(VCALL(args, 6, HRESULT (WINAPI *)(void *, int *))(args, &vk) == S_OK);
        CHECK(VCALL(args, 7, HRESULT (WINAPI *)(void *, void *))(args, status) == S_OK);
        CHECK(status[12] == 0xCC && status[15] == 0xCC);                     /* at most 12 bytes written */
        r->vk = vk;
        r->repeat = *(UINT32 *)status;
        r->what = g_cur_event;
    } else if (handler->kind == H_VIS) {
        BYTE visible = 99;
        CHECK(VCALL(args, 6, HRESULT (WINAPI *)(void *, BYTE *))(args, &visible) == S_OK);
        r->visible = visible;
    } else if (handler->kind == H_PANE) {
        X104Rect rect;
        CHECK(VCALL(args, 7, HRESULT (WINAPI *)(void *, BYTE))(args, 1) == S_OK);         /* put_EnsuredFocusedElementInView(true), like the game */
        CHECK(VCALL(args, 6, HRESULT (WINAPI *)(void *, X104Rect *))(args, &rect) == S_OK);
        r->pane_h = rect.Height;
    }
    return S_OK;
}
static const void *mh_vtbl[4] = { mh_qi, mh_addref, mh_release, mh_invoke };
static MockHandler mh_ptr_cw[3] = { { mh_vtbl, 1, H_POINTER }, { mh_vtbl, 1, H_POINTER }, { mh_vtbl, 1, H_POINTER } };
static MockHandler mh_ptr_src[3] = { { mh_vtbl, 1, H_POINTER }, { mh_vtbl, 1, H_POINTER }, { mh_vtbl, 1, H_POINTER } };
static MockHandler mh_key_down = { mh_vtbl, 1, H_KEY };
static MockHandler mh_key_up = { mh_vtbl, 1, H_KEY };
static MockHandler mh_vis = { mh_vtbl, 1, H_VIS };
static MockHandler mh_text = { mh_vtbl, 1, H_TEXT };
static MockHandler mh_showing = { mh_vtbl, 1, H_PANE };
static MockHandler mh_hiding = { mh_vtbl, 1, H_PANE };

static int count_rec(int kind, int what)
{
    int index, n = 0;
    for (index = 0; index < g_rec_count; ++index) if (g_rec[index].kind == kind && (what < 0 || g_rec[index].what == what)) ++n;
    return n;
}

#define SLOT(obj, n, type) VCALL(obj, n, type)
static HWND g_window;
static LRESULT g_result;
static BOOL send_msg(UINT message, WPARAM wparam, LPARAM lparam)
{
    g_result = 0;
    return x104_input_window_message(g_window, message, wparam, lparam, &g_result);
}
#define MAKEWPARAM(l, h) ((WPARAM)(((DWORD)(l) & 0xFFFF) | ((DWORD)(h) << 16)))
#define MAKELPARAM(l, h) ((LPARAM)(((DWORD)(l) & 0xFFFF) | ((DWORD)(h) << 16)))
#define XY(x, y) ((LPARAM)(((y) & 0xFFFF) << 16 | ((x) & 0xFFFF)))
static void pump(void)
{
    x104_ui_pump_once();
    x104_input_ui_tick();
    x104_ui_pump_once();
}
static void get_text(void *textbox, wchar_t *out, int capacity)
{
    HSTRING hs = NULL;
    UINT32 length = 0;
    const wchar_t *raw;
    out[0] = 0;
    CHECK(SLOT(textbox, 6, HRESULT (WINAPI *)(void *, HSTRING *))(textbox, &hs) == S_OK);
    raw = hs ? WindowsGetStringRawBuffer(hs, &length) : NULL;
    if (raw) {
        if ((int)length > capacity - 1) length = (UINT32)capacity - 1;
        memcpy(out, raw, length * sizeof(wchar_t));
        out[length] = 0;
    }
    x104_hstring_free(hs);
}
static void put_text(void *textbox, const wchar_t *text)
{
    HSTRING hs = NULL;
    CHECK(x104_hstring_create(text, &hs) == S_OK);
    CHECK(SLOT(textbox, 7, HRESULT (WINAPI *)(void *, HSTRING))(textbox, hs) == S_OK);
    x104_hstring_free(hs);
}

int main(void)
{
    void *cw, *iface, *tb, *tb2, *ctl, *btn_ctl, *src1, *src2, *srcptr1, *srcptr2, *statics, *pane;
    HRESULT hr;
    LONGLONG token;
    int index;
    wchar_t buffer[64];
    X104Object *editbox, *button, *logo;
    WNDCLASSEXW klass;

    test_print("== input unit logic test ==\n");
    x104.image = (BYTE *)0x400000;
    x104.ui_thread_id = GetCurrentThreadId();
    memset(&klass, 0, sizeof(klass));
    klass.cbSize = sizeof(klass);
    klass.lpfnWndProc = (WNDPROC)DefWindowProcW;
    klass.lpszClassName = L"X104InputTest";
    RegisterClassExW(&klass);
    g_window = CreateWindowExW(0, L"X104InputTest", L"t", WS_POPUP, 0, 0, 800, 600, NULL, NULL, NULL, NULL);
    CHECK(g_window != NULL);
    x104.window = g_window;

    /* safe before init */
    CHECK(send_msg(0x0246u, 5, XY(100, 200)) == FALSE);
    CHECK(x104_input_core_window() == NULL && x104_input_element_editbox() == NULL);
    x104_input_ui_tick();
    x104_inputpane_raise(TRUE);
    CHECK(log_has("not initialised"));

    /* ---- init + the QI matrix ---- */
    CHECK(x104_input_install(NULL));
    CHECK(x104_input_init(g_window));
    CHECK(g_attach_calls == 1 && arm32_touch_api_effective == ARM32_TOUCHAPI_POINTER);
    CHECK(x104_input_init(g_window) && g_attach_calls == 1);
    editbox = x104_input_element_editbox();
    button = x104_input_element_button();
    logo = x104_input_element_logo();
    CHECK(editbox && button && logo && x104_input_core_window());
    CHECK(log_has("X104 input: init ok"));
    CHECK(x104_object_qi(x104_input_core_window(), &X104_IID_ICoreWindow, &cw) == S_OK);
    CHECK(x104_object_qi(x104_input_core_window(), &X104_IID_IAgileObject, &iface) == S_OK);       /* mandatory for HID's Platform::Agile */
    CHECK(x104_object_qi(editbox, &X104_IID_ITextBox, &tb) == S_OK);
    CHECK(x104_object_qi(editbox, &X104_IID_ITextBox2, &tb2) == S_OK);
    CHECK(x104_object_qi(editbox, &X104_IID_IControl, &ctl) == S_OK);
    CHECK(x104_object_qi(editbox, &X104_IID_IUIElement, &iface) == S_OK);
    CHECK(x104_object_qi(button, &X104_IID_IControl, &btn_ctl) == S_OK);
    CHECK(x104_object_qi(button, &X104_IID_IButton, &iface) == S_OK);
    CHECK(x104_object_qi(logo, &X104_IID_IImage, &iface) == S_OK);
    CHECK(x104_object_qi(logo, &X104_IID_IUIElement, &iface) == S_OK);
    CHECK(x104_object_qi(logo, &X104_IID_ITextBox, &iface) == E_NOINTERFACE);
    /* statics through the activation hook of the unit */
    CHECK(x104_input_activate(L"Windows.UI.Core.CoreWindow", &X104_IID_ICoreWindowStatic, &statics) == S_OK);
    CHECK(SLOT(statics, X104_SLOT_ICoreWindowStatic_GetForCurrentThread, HRESULT (WINAPI *)(void *, void **))(statics, &iface) == S_OK && iface == cw);
    CHECK(x104_input_activate(L"Windows.UI.ViewManagement.InputPane", &X104_IID_IInputPaneStatics, &statics) == S_OK);
    CHECK(SLOT(statics, X104_SLOT_IInputPaneStatics_GetForCurrentView, HRESULT (WINAPI *)(void *, void **))(statics, &pane) == S_OK);
    CHECK(SLOT(statics, X104_SLOT_IInputPaneStatics_GetForCurrentView, HRESULT (WINAPI *)(void *, void **))(statics, &iface) == S_OK && iface == pane);   /* the same object */
    CHECK(x104_input_activate(L"Windows.UI.Core.CoreCursor", &X104_IID_ICoreCursorFactory, &statics) == S_OK);
    CHECK(x104_input_activate(L"Windows.Nope", &X104_IID_ICoreCursorFactory, &iface) == REGDB_E_CLASSNOTREG);

    /* ---- input before HID::Initialize is dropped (no handlers yet) ---- */
    CHECK(send_msg(0x0246u, 5, XY(100, 200)) == TRUE);                  /* touch is still consumed */
    CHECK(x104_input_st.dropped_events == 1 && !x104_input_any_pointer_down());
    CHECK(send_msg(0x0041u + 0x100u - 0x41u + 0u, 0x41, 0) == TRUE);    /* WM_KEYDOWN 'A' with nobody registered: consumed + dropped */
    CHECK(x104_input_st.dropped_events == 2);

    /* ---- the registrations of HID::Initialize (CoreWindow keys) and of the work items (sources) ---- */
    token = 0;
    CHECK(SLOT(cw, X104_SLOT_ICoreWindow_add_KeyDown, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(cw, &mh_key_down, &token) == S_OK && token != 0);
    CHECK(SLOT(cw, X104_SLOT_ICoreWindow_add_KeyUp, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(cw, &mh_key_up, &token) == S_OK);
    CHECK(SLOT(cw, X104_SLOT_ICoreWindow_add_VisibilityChanged, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(cw, &mh_vis, &token) == S_OK);
    CHECK(log_has("CoreWindow.add_KeyDown handler="));
    CHECK(x104_input_create_independent_source(7, &src1) == S_OK && src1);
    CHECK(x104_input_create_independent_source(7, &src2) == S_OK && src2);
    CHECK(log_has("independent input source #2 created (types=7)"));
    CHECK(x104_raw_qi(src2, &X104_IID_ICorePointerInputSource, &srcptr2) == S_OK);
    CHECK(x104_raw_qi(src1, &X104_IID_ICorePointerInputSource, &srcptr1) == S_OK);
    {
        void *dispatcher = (void *)1;
        /* get_Dispatcher is called straight on the returned pointer; the stub async unit has no dispatcher object: E_FAIL + a log, never a crash */
        hr = SLOT(src2, X104_SLOT_ICoreInputSourceBase_get_Dispatcher, HRESULT (WINAPI *)(void *, void **))(src2, &dispatcher);
        CHECK(hr != S_OK && dispatcher == NULL);
    }
    /* source 1 = MainPage's empty handlers (here: also recording, to count), source 2 = the real ones */
    for (index = 0; index < 3; ++index) {
        static const int moved_pressed_released[3][2] = { { X104_SLOT_ICorePointerInputSource_add_PointerMoved, 0 }, { X104_SLOT_ICorePointerInputSource_add_PointerPressed, 1 },
                                                           { X104_SLOT_ICorePointerInputSource_add_PointerReleased, 2 } };
        CHECK(SLOT(srcptr2, moved_pressed_released[index][0], HRESULT (WINAPI *)(void *, void *, LONGLONG *))(srcptr2, &mh_ptr_src[index], &token) == S_OK && token != 0);
        CHECK(SLOT(cw, index == 0 ? X104_SLOT_ICoreWindow_add_PointerMoved : (index == 1 ? X104_SLOT_ICoreWindow_add_PointerPressed : X104_SLOT_ICoreWindow_add_PointerReleased),
                   HRESULT (WINAPI *)(void *, void *, LONGLONG *))(cw, &mh_ptr_cw[index], &token) == S_OK);
    }

    /* ---- WM_POINTER touch: down, three updates (coalesced into ONE Moved), up ---- */
    g_rec_count = 0;
    CHECK(send_msg(0x0246u, MAKEWPARAM(5, 0x0004), XY(100, 200)) == TRUE && g_result == 0);
    CHECK(x104_input_any_touch_down() && g_note_touch >= 1);
    CHECK(count_rec(H_POINTER, -1) == 2);                                 /* CoreWindow list + source #2 list (source #1 has no handler) */
    CHECK(g_rec[0].id == 5 && g_rec[0].device == 0 && g_rec[0].x == 100.0f && g_rec[0].y == 200.0f && g_rec[0].left == 1 && g_rec[0].contact == 1);
    CHECK(send_msg(0x0245u, MAKEWPARAM(5, 0x0004), XY(110, 210)) == TRUE);
    CHECK(send_msg(0x0245u, MAKEWPARAM(5, 0x0004), XY(120, 220)) == TRUE);
    CHECK(send_msg(0x0245u, MAKEWPARAM(5, 0x0004), XY(130, 230)) == TRUE);
    CHECK(count_rec(H_POINTER, -1) == 2);                                 /* nothing yet: coalesced */
    pump();
    CHECK(count_rec(H_POINTER, -1) == 4 && g_rec[2].x == 130.0f && g_rec[2].y == 230.0f);
    pump();
    CHECK(count_rec(H_POINTER, -1) == 4);                                 /* no new move without new positions */
    g_osk_pointer_up = 0;
    CHECK(send_msg(0x0247u, MAKEWPARAM(5, 0x0000), XY(131, 231)) == TRUE);
    CHECK(count_rec(H_POINTER, -1) == 6 && g_rec[4].id == 5 && g_rec[4].x == 131.0f && g_rec[4].left == 0 && g_rec[4].contact == 0);
    CHECK(g_osk_pointer_up == 1 && !x104_input_any_pointer_down());
    CHECK(send_msg(0x0247u, MAKEWPARAM(5, 0x0000), XY(131, 231)) == FALSE);        /* an up without a down is not ours */
    CHECK(log_has("first WM_POINTER touch"));

    /* two fingers; one is cancelled by a lost capture, the other by a focus loss (deferred to the tick, R1) */
    g_rec_count = 0;
    CHECK(send_msg(0x0246u, MAKEWPARAM(7, 0x0004), XY(10, 10)) == TRUE);
    CHECK(send_msg(0x0246u, MAKEWPARAM(8, 0x0004), XY(20, 20)) == TRUE);
    CHECK(x104_input_st.pointers[0].used && x104_input_st.pointers[1].used);
    CHECK(send_msg(0x024Cu, MAKEWPARAM(7, 0), (LPARAM)0x1234) == TRUE);          /* capture went to another window */
    CHECK(count_rec(H_POINTER, -1) == 6);
    CHECK(send_msg(0x0008u, 0, 0) == FALSE);                                      /* WM_KILLFOCUS: broadcast, only a flag */
    CHECK(x104_input_any_pointer_down());
    pump();
    CHECK(!x104_input_any_pointer_down() && count_rec(H_POINTER, -1) == 8);
    CHECK(log_has("released all pointers (1): focus lost"));
    /* a mouse pointer is not consumed by the touch path */
    CHECK(send_msg(0x0246u, MAKEWPARAM(200, 0x0004), XY(10, 10)) == FALSE);

    /* ---- the real mouse: Pressed(Mouse, left) / hover never delivered / Moved while pressed / Released with IsLeft FALSE ---- */
    g_rec_count = 0;
    CHECK(send_msg(0x0200u, 0, XY(50, 60)) == TRUE);                              /* hover */
    pump();
    CHECK(g_rec_count == 0);
    CHECK(send_msg(0x0201u, 1, XY(50, 60)) == TRUE);
    CHECK(count_rec(H_POINTER, -1) == 2 && g_rec[0].id == X104_INPUT_MOUSE_ID && g_rec[0].device == 2 && g_rec[0].left == 1);
    CHECK(x104_input_st.mouse_down);
    CHECK(send_msg(0x0200u, 1, XY(55, 66)) == TRUE);
    pump();
    CHECK(count_rec(H_POINTER, -1) == 4 && g_rec[2].x == 55.0f && g_rec[2].y == 66.0f);
    CHECK(send_msg(0x0202u, 0, XY(56, 67)) == TRUE);
    CHECK(count_rec(H_POINTER, -1) == 6 && g_rec[4].device == 2 && g_rec[4].left == 0 && g_rec[4].x == 56.0f);
    CHECK(!x104_input_st.mouse_down && !x104_input_any_pointer_down());
    CHECK(send_msg(0x0204u, 2, XY(50, 60)) == TRUE);                              /* right button: ignored */
    CHECK(count_rec(H_POINTER, -1) == 6);
    /* positions outside the client area are clamped */
    CHECK(send_msg(0x0201u, 1, XY(5000, 6000)) == TRUE);
    CHECK(g_rec[count_rec(H_POINTER, -1) - 1].x == 799.0f && g_rec[count_rec(H_POINTER, -1) - 1].y == 599.0f);
    x104_input_release_all_pointers("test");
    CHECK(!x104_input_any_pointer_down() && g_rec[g_rec_count - 1].left == 0);

    /* ---- the first touch-sourced mouse message while no WM_POINTER touch was seen would switch to the legacy path: tested with the flag ---- */
    x104_input_st.touch_api = ARM32_TOUCHAPI_LEGACY;
    {
        BOOL consumed = x104_input_legacy_touch(g_window, 0x0201u, XY(300, 310));       /* the conversion function itself (the extra-info signature needs a real message) */
        CHECK(consumed && x104_input_ptr_find(X104_INPUT_LEGACY_TOUCH_ID) >= 0);
        x104_input_legacy_touch(g_window, 0x0202u, XY(301, 311));
        CHECK(x104_input_ptr_find(X104_INPUT_LEGACY_TOUCH_ID) < 0);
    }
    x104_input_st.touch_api = ARM32_TOUCHAPI_POINTER;

    /* ---- keys ---- */
    g_rec_count = 0;
    g_cur_event = 0;
    CHECK(send_msg(0x0100u, 0x41, 0x00000001) == TRUE);                             /* 'A' down */
    CHECK(count_rec(H_KEY, -1) == 1 && g_rec[0].vk == 0x41 && g_rec[0].repeat == 1);
    CHECK(send_msg(0x0101u, 0x41, 0xC0000001) == TRUE);                             /* 'A' up */
    CHECK(count_rec(H_KEY, -1) == 2 && g_rec[1].vk == 0x41);
    CHECK(send_msg(0x0101u, 0x41, 0xC0000001) == TRUE && count_rec(H_KEY, -1) == 2);   /* an up without a down is not delivered */
    CHECK(send_msg(0x0100u, 0x11, 0x00000001) == TRUE && count_rec(H_KEY, -1) == 2);   /* Ctrl: dropped (garbage text event otherwise) */
    CHECK(send_msg(0x0100u, 0xE7, 0x00000001) == TRUE && count_rec(H_KEY, -1) == 2);   /* VK_PACKET */
    CHECK(send_msg(0x0100u, 0xBE, 0x00000001) == TRUE && count_rec(H_KEY, -1) == 2);   /* OEM period */
    CHECK(send_msg(0x0100u, 0x10, 0x00000001) == TRUE && count_rec(H_KEY, -1) == 3 && g_rec[2].vk == 0x10);   /* Shift is delivered */
    CHECK(send_msg(0x0100u, 0x25, 0x00000001) == TRUE && count_rec(H_KEY, -1) == 4);   /* left arrow */
    CHECK(send_msg(0x0104u, 0x79, 0x20000001) == TRUE && count_rec(H_KEY, -1) == 5 && g_rec[4].vk == 0x79);   /* F10 as SYSKEYDOWN: delivered, consumed (no menu mode) */
    CHECK(send_msg(0x0104u, 0x73, 0x20000001) == FALSE);                            /* Alt+F4: DefWindowProc turns it into WM_CLOSE */
    CHECK(count_rec(H_KEY, -1) == 5);
    x104_input_release_all_keys("test");
    CHECK(count_rec(H_KEY, -1) == 5 + 3);                                           /* Shift, Left, F10 were still down */
    g_rec_count = 0;
    CHECK(send_msg(0x0100u, 0x1B, 0x00000001) == TRUE && g_rec_count == 0);          /* Esc is Back, not a key */
    CHECK(log_has("BackPressed handled=0 (Esc)"));
    CHECK(send_msg(0x0101u, 0x1B, 0xC0000001) == TRUE && g_rec_count == 0);
    CHECK(send_msg(0x0104u, 0x25, 0x20000001) == TRUE && g_rec_count == 0);          /* Alt+Left */
    CHECK(log_has("(Alt+Left)"));
    g_osk_visible = FALSE;
    {
        int before = g_note_mouse;
        send_msg(0x0100u, 0x42, 0x00000001);
        CHECK(g_note_mouse == before + 1);                                          /* a hardware key switches the source to mouse/keyboard */
        send_msg(0x0101u, 0x42, 0xC0000001);
    }

    /* ---- the TextBox ---- */
    {
        BYTE focus_result = 0;
        g_osk_show = g_osk_hide = 0;
        CHECK(SLOT(tb2, X104_SLOT_ITextBox2_put_PreventKeyboardDisplayOnProgrammaticFocus, HRESULT (WINAPI *)(void *, BYTE))(tb2, 0) == S_OK);
        CHECK(log_has("PreventKeyboardDisplayOnProgrammaticFocus=0"));
        CHECK(SLOT(tb, X104_SLOT_ITextBox_put_AcceptsReturn, HRESULT (WINAPI *)(void *, BYTE))(tb, 1) == S_OK);
        CHECK(SLOT(tb, X104_SLOT_ITextBox_add_TextChanged, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(tb, &mh_text, &token) == S_OK && token != 0);
        CHECK(log_has("ITextBox.add_TextChanged handler="));
        /* the show-keyboard lambda: MaxLength, put_Text, Select(len,len), Focus(3) */
        CHECK(SLOT(tb, X104_SLOT_ITextBox_put_MaxLength, HRESULT (WINAPI *)(void *, INT))(tb, 0xC350) == S_OK);
        put_text(tb, L"hi");
        CHECK(SLOT(tb, X104_SLOT_ITextBox_Select, HRESULT (WINAPI *)(void *, INT, INT))(tb, 2, 2) == S_OK);
        text_input_active = FALSE;
        CHECK(SLOT(ctl, X104_SLOT_IControl_Focus, HRESULT (WINAPI *)(void *, int, BYTE *))(ctl, 3, &focus_result) == S_OK && focus_result == 1);
        CHECK(g_osk_show == 1 && text_input_active == TRUE && x104_input_st.focused == X104_INPUT_FOCUS_EDITBOX);
        CHECK(log_has("X104 text_input_active 0 -> 1 (EditBox.Focus)"));
        /* put_Text always yields exactly ONE TextChanged, after the setter returned, even for several setters before the pump */
        g_rec_count = 0;
        CHECK(g_queue_count == 1);                                                 /* the put_Text above scheduled it; the second setter coalesces */
        put_text(tb, L"hi");
        CHECK(g_queue_count == 1 && count_rec(H_TEXT, -1) == 0);
        x104_ui_pump_once();
        CHECK(count_rec(H_TEXT, -1) == 1 && g_queue_count == 0);
        get_text(tb, buffer, 64);
        CHECK(lstrcmpW(buffer, L"hi") == 0);
        /* typing: WM_CHAR appends at the caret, one TextChanged for several chars */
        g_rec_count = 0;
        send_msg(0x0102u, 'a', 1);
        send_msg(0x0102u, 'b', 1);
        CHECK(g_queue_count == 1);
        x104_ui_pump_once();
        CHECK(count_rec(H_TEXT, -1) == 1);
        get_text(tb, buffer, 64);
        CHECK(lstrcmpW(buffer, L"hiab") == 0);
        send_msg(0x0102u, 0x08, 1);                                                 /* backspace */
        x104_ui_pump_once();
        get_text(tb, buffer, 64);
        CHECK(lstrcmpW(buffer, L"hia") == 0);
        send_msg(0x0102u, 0x0D, 1);                                                 /* Enter = the adjacent pair CR LF */
        x104_ui_pump_once();
        get_text(tb, buffer, 64);
        CHECK(lstrcmpW(buffer, L"hia\r\n") == 0);
        /* the game strips the CR LF and writes the text back: ANOTHER one-shot TextChanged, then it stops */
        g_rec_count = 0;
        put_text(tb, L"hia");
        x104_ui_pump_once();
        CHECK(count_rec(H_TEXT, -1) == 1 && g_queue_count == 0);
        /* MaxLength: the whole insertion is refused (never a partial CR LF) */
        CHECK(SLOT(tb, X104_SLOT_ITextBox_put_MaxLength, HRESULT (WINAPI *)(void *, INT))(tb, 4) == S_OK);
        g_rec_count = 0;
        send_msg(0x0102u, 'x', 1);                                                  /* "hiax" = 4 */
        send_msg(0x0102u, 'y', 1);                                                  /* refused */
        send_msg(0x0102u, 0x0D, 1);                                                 /* refused */
        x104_ui_pump_once();
        get_text(tb, buffer, 64);
        CHECK(lstrcmpW(buffer, L"hiax") == 0 && count_rec(H_TEXT, -1) == 1);
        /* control characters and a surrogate pair */
        CHECK(SLOT(tb, X104_SLOT_ITextBox_put_MaxLength, HRESULT (WINAPI *)(void *, INT))(tb, 50000) == S_OK);
        put_text(tb, L"");
        x104_ui_pump_once();
        send_msg(0x0102u, 0x01, 1);
        send_msg(0x0102u, 0x7F, 1);
        send_msg(0x0102u, 0xD83D, 1);
        send_msg(0x0102u, 0xDE00, 1);
        x104_ui_pump_once();
        get_text(tb, buffer, 64);
        CHECK(lstrlenW(buffer) == 2 && buffer[0] == 0xD83D && buffer[1] == 0xDE00);
        send_msg(0x0102u, 0x08, 1);
        x104_ui_pump_once();
        get_text(tb, buffer, 64);
        CHECK(lstrlenW(buffer) == 0);                                              /* the pair is removed as a unit */
        /* hide: Button.Focus(3) leaves text mode, WM_CHAR is ignored */
        CHECK(SLOT(btn_ctl, X104_SLOT_IControl_Focus, HRESULT (WINAPI *)(void *, int, BYTE *))(btn_ctl, 3, &focus_result) == S_OK);
        CHECK(g_osk_hide == 1 && text_input_active == FALSE && x104_input_st.focused == X104_INPUT_FOCUS_BUTTON);
        CHECK(log_has("X104 text_input_active 1 -> 0 (Button.Focus)"));
        g_rec_count = 0;
        send_msg(0x0102u, 'z', 1);
        x104_ui_pump_once();
        get_text(tb, buffer, 64);
        CHECK(lstrlenW(buffer) == 0 && count_rec(H_TEXT, -1) == 0);
    }

    /* ---- InputPane: the rectangle is published FIRST, the handlers read it ---- */
    {
        void *pane_args = NULL;
        X104Rect rect;
        CHECK(SLOT(pane, X104_SLOT_IInputPane_add_Showing, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(pane, &mh_showing, &token) == S_OK);
        CHECK(SLOT(pane, X104_SLOT_IInputPane_add_Hiding, HRESULT (WINAPI *)(void *, void *, LONGLONG *))(pane, &mh_hiding, &token) == S_OK);
        (void)pane_args;
        g_rec_count = 0;
        g_osk_rect_ok = TRUE;
        g_osk_rect[0] = 0.0f; g_osk_rect[1] = 400.0f; g_osk_rect[2] = 800.0f; g_osk_rect[3] = 200.0f;
        x104_inputpane_raise(TRUE);
        CHECK(count_rec(H_PANE, -1) == 1 && g_rec[0].pane_h == 200.0f);
        CHECK(log_has("InputPane Showing raised, occluded h=200"));
        CHECK(SLOT(pane, X104_SLOT_IInputPane_get_OccludedRect, HRESULT (WINAPI *)(void *, X104Rect *))(pane, &rect) == S_OK && rect.Height == 200.0f && rect.Y == 400.0f);
        g_osk_rect_ok = FALSE;
        x104_inputpane_raise(TRUE);
        CHECK(count_rec(H_PANE, -1) == 2 && g_rec[1].pane_h > 100.0f && g_rec[1].pane_h < 300.0f);        /* default 40% of the client height (600 -> 240 or the panel size) */
        CHECK(log_has("default 40% of the client height"));
        x104_inputpane_raise(FALSE);
        CHECK(count_rec(H_PANE, -1) == 3 && g_rec[2].pane_h == 0.0f);
        CHECK(SLOT(pane, X104_SLOT_IInputPane_get_OccludedRect, HRESULT (WINAPI *)(void *, X104Rect *))(pane, &rect) == S_OK && rect.Height == 0.0f);
    }

    /* ---- visibility ---- */
    g_rec_count = 0;
    x104_input_raise_visibility_changed(FALSE);
    CHECK(count_rec(H_VIS, -1) == 1 && g_rec[0].visible == 0);
    x104_input_raise_visibility_changed(TRUE);
    CHECK(count_rec(H_VIS, -1) == 2 && g_rec[1].visible == 1);

    /* ---- cursor: factory caches, put_PointerCursor(NULL) hides, WM_SETCURSOR answers ---- */
    {
        void *c1 = NULL, *c2 = NULL;
        int type = -1;
        UINT32 id = 9;
        CHECK(x104_input_activate(L"Windows.UI.Core.CoreCursor", &X104_IID_ICoreCursorFactory, &statics) == S_OK);
        CHECK(SLOT(statics, X104_SLOT_ICoreCursorFactory_CreateCursor, HRESULT (WINAPI *)(void *, int, UINT32, void **))(statics, 0, 0, &c1) == S_OK && c1);
        CHECK(SLOT(statics, X104_SLOT_ICoreCursorFactory_CreateCursor, HRESULT (WINAPI *)(void *, int, UINT32, void **))(statics, 0, 0, &c2) == S_OK && c2 == c1);
        CHECK(SLOT(c1, X104_SLOT_ICoreCursor_get_Type, HRESULT (WINAPI *)(void *, int *))(c1, &type) == S_OK && type == 0);
        CHECK(SLOT(c1, X104_SLOT_ICoreCursor_get_Id, HRESULT (WINAPI *)(void *, UINT32 *))(c1, &id) == S_OK && id == 0);
        CHECK(SLOT(cw, X104_SLOT_ICoreWindow_put_PointerCursor, HRESULT (WINAPI *)(void *, void *))(cw, NULL) == S_OK && !x104_input_st.cursor_visible);
        CHECK(send_msg(0x0020u, (WPARAM)g_window, MAKELPARAM(1, 0x200)) == TRUE && g_result == TRUE);
        CHECK(SLOT(cw, X104_SLOT_ICoreWindow_put_PointerCursor, HRESULT (WINAPI *)(void *, void *))(cw, c1) == S_OK && x104_input_st.cursor_visible);
        CHECK(send_msg(0x0020u, (WPARAM)g_window, MAKELPARAM(2, 0x200)) == FALSE);        /* not the client area */
    }

    /* ---- CoreWindow bounds ---- */
    {
        X104Rect bounds;
        CHECK(SLOT(cw, X104_SLOT_ICoreWindow_get_Bounds, HRESULT (WINAPI *)(void *, X104Rect *))(cw, &bounds) == S_OK && bounds.Width > 0.0f && bounds.Height > 0.0f);
    }

    /* ---- shutdown is idempotent and clears text_input_active ---- */
    text_input_active = TRUE;
    x104_input_shutdown();
    CHECK(text_input_active == FALSE && !x104_input_st.initialized);
    x104_input_shutdown();
    CHECK(send_msg(0x0246u, 5, XY(1, 1)) == FALSE);

    test_print("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

#if defined(SLOT_TEST_NEG_ARGS)
static HRESULT WINAPI slot_test_put_interval(void *self, LONG value) { (void)self; (void)value; return S_OK; }
X104_VTBL(slot_test_vt, IDispatcherTimer) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IDispatcherTimer, put_Interval, slot_test_put_interval) };
#elif defined(SLOT_TEST_NEG_FLOAT)
static HRESULT WINAPI slot_test_set_left(void *self, void *element, float length) { (void)self; (void)element; (void)length; return S_OK; }
X104_VTBL(slot_test_vt, ICanvasStatics) = { X104_INSPECTABLE_SLOTS, X104_SLOT(ICanvasStatics, SetLeft, slot_test_set_left) };
#elif defined(SLOT_TEST_NEG_NOARGS)
static HRESULT WINAPI slot_test_start(void *self, void *callback) { (void)self; (void)callback; return S_OK; }
X104_VTBL(slot_test_vt, IApplicationStatics) = { X104_INSPECTABLE_SLOTS, X104_SLOT_NOARGS(IApplicationStatics, Start, slot_test_start) };
#elif defined(SLOT_TEST_NEG_NOTYPEDEF)
static HRESULT WINAPI slot_test_margin(void *self, int value) { (void)self; (void)value; return S_OK; }
X104_VTBL(slot_test_vt, IFrameworkElement) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IFrameworkElement, put_Margin, slot_test_margin) };
#elif defined(SLOT_TEST_POS)
static HRESULT WINAPI slot_test_put_interval(void *self, LONGLONG value) { (void)self; (void)value; return S_OK; }
X104_VTBL(slot_test_vt1, IDispatcherTimer) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IDispatcherTimer, put_Interval, slot_test_put_interval), X104_SLOT_NOARGS(IDispatcherTimer, Start, x104_nop) };
#endif
