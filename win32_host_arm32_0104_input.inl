/* win32_host_arm32_0104_input.inl - UNIT "input" of the Minecraft PE 0.10.4.40 ARM32 host.
 *
 * Owner: the input implementer.  Brief: arm32/v0_10_4/units/input.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit input
 *
 * This file owns everything between the Windows window and the game's HIDController:
 *   - the fake CoreWindow (+ its statics), CoreCursor (+ factory), CoreIndependentInputSource (one per CreateCoreIndependentInputSource call),
 *   - the pointer / key / visibility / text-changed event-args objects (PointerEventArgs -> PointerPoint -> PointerDevice / PointerPointProperties),
 *   - the three XAML elements the game gets from Page.FindName: Logo (Image), EditBox (TextBox with a real text model), Button,
 *   - the InputPane (statics, instance, event args, x104_inputpane_raise) and the bridge to the shared TabTip controller (x104_osk_* wrappers),
 *   - the translation of Win32 messages (WM_POINTER*, WM_TOUCH, emulated/real mouse, keys, characters, focus, cursor) into delegate calls.
 *
 * Threads: window messages, the ui tick and every delegate invocation run on the UI thread (R1: pointer, key and char messages come from the message
 * loop and never from inside a fake called by the game).  The fakes themselves may be called from any thread: event lists lock internally, the text
 * model has its own leaf lock (never held across a call into the game), the independent-source table has a leaf lock.  Pointer / key state is touched
 * by the UI thread only.
 *
 * Game facts used (facts/input_text.md + verdicts): HID::Initialize 0x111C1C registers KeyDown/KeyUp on the CoreWindow (always) and the three pointer
 * events either on a CoreIndependentInputSource created by the pool work item 0x110940 (the shipped path, the panel is passed) or on the CoreWindow;
 * the handlers call the args slots straight on the pointer they receive (no QI, R10), read x = (int16)(Position.X * scale), IsLeftButtonPressed of
 * the mouse Released must be FALSE, TextChanged must follow every game put_Text exactly once and after the setter returned (HID[0x58] flag), Enter is the
 * adjacent pair CR LF, the keyboard is shown by EditBox.Focus(Programmatic) and hidden by Button.Focus(Programmatic). */
#ifndef WIN32_HOST_ARM32_0104_INPUT_INL
#define WIN32_HOST_ARM32_0104_INPUT_INL

/* ======================================================================================================================
 * 1. constants, types, state
 * ====================================================================================================================== */
#define X104_INPUT_MAX_POINTERS   12            /* the game has 12 touch slots (Multitouch::feed clamps 0..11) */
#define X104_INPUT_MAX_SOURCES    8             /* independent input sources (the game creates 2) */
#define X104_INPUT_MAX_CURSORS    8
#define X104_INPUT_TEXT_CAP       50004         /* wide characters incl. NUL: the game's limit is 50000 (MaxLength n+1 or 0xC350) */
#define X104_INPUT_MOUSE_ID       0xFFFFFF01u   /* PointerId of the real mouse (never collides with Windows touch ids) */
#define X104_INPUT_LEGACY_TOUCH_ID 0xFFFFFFF0u  /* PointerId of the single emulated touch (same value as the 1.2.8 host) */
#define X104_INPUT_EV_MOVED       0
#define X104_INPUT_EV_PRESSED     1
#define X104_INPUT_EV_RELEASED    2
#define X104_INPUT_DEV_TOUCH      0             /* Windows.Devices.Input.PointerDeviceType */
#define X104_INPUT_DEV_PEN        1
#define X104_INPUT_DEV_MOUSE      2
#define X104_INPUT_FOCUS_NONE     0
#define X104_INPUT_FOCUS_EDITBOX  1
#define X104_INPUT_FOCUS_BUTTON   2

#define X104_INPUT_WM_ACTIVATE        0x0006u
#define X104_INPUT_WM_SETFOCUS        0x0007u
#define X104_INPUT_WM_KILLFOCUS       0x0008u
#define X104_INPUT_WM_SIZE            0x0005u
#define X104_INPUT_WM_SETCURSOR       0x0020u
#define X104_INPUT_WM_CANCELMODE      0x001Fu
#define X104_INPUT_WM_ACTIVATEAPP     0x001Cu
#define X104_INPUT_WM_KEYDOWN         0x0100u
#define X104_INPUT_WM_KEYUP           0x0101u
#define X104_INPUT_WM_CHAR            0x0102u
#define X104_INPUT_WM_SYSKEYDOWN      0x0104u
#define X104_INPUT_WM_SYSKEYUP        0x0105u
#define X104_INPUT_WM_MOUSEMOVE       0x0200u
#define X104_INPUT_WM_LBUTTONDOWN     0x0201u
#define X104_INPUT_WM_LBUTTONUP       0x0202u
#define X104_INPUT_WM_RBUTTONDOWN     0x0204u
#define X104_INPUT_WM_RBUTTONUP       0x0205u
#define X104_INPUT_WM_MBUTTONDOWN     0x0207u
#define X104_INPUT_WM_MBUTTONUP       0x0208u
#define X104_INPUT_WM_MOUSEWHEEL      0x020Au
#define X104_INPUT_WM_XBUTTONDOWN     0x020Bu
#define X104_INPUT_WM_XBUTTONUP       0x020Cu
#define X104_INPUT_WM_CAPTURECHANGED  0x0215u
#define X104_INPUT_WM_TOUCH           0x0240u
#define X104_INPUT_WM_POINTERUPDATE   0x0245u
#define X104_INPUT_WM_POINTERDOWN     0x0246u
#define X104_INPUT_WM_POINTERUP       0x0247u
#define X104_INPUT_WM_POINTERCAPTURECHANGED 0x024Cu

/* one tracked pointer (a pressed contact the game has seen a Pressed for) */
typedef struct x104_input_pointer_t {
    BOOL used;
    BOOL dirty;                 /* a newer position than sent_x/sent_y is waiting for x104_input_flush_moves */
    UINT32 id;
    int device;                 /* X104_INPUT_DEV_* */
    int x, y;                   /* last known position, client pixels */
    int sent_x, sent_y;         /* last position the game was told */
    DWORD down_tick;
} x104_input_pointer_t;

/* a CoreIndependentInputSource: three pointer event lists (index X104_INPUT_EV_*) */
typedef struct x104_input_source_t {
    X104Object base;
    int number;
    UINT32 types;
    X104Event ptr[3];
} x104_input_source_t;

typedef struct x104_input_cursor_slot_t {
    int type;
    UINT32 id;
    X104Object *object;
} x104_input_cursor_slot_t;

/* CorePhysicalKeyStatus (12 bytes; the game reads RepeatCount only, KeyUp has a 16-byte buffer) */
typedef struct x104_input_key_status_t {
    UINT32 repeat_count;
    UINT32 scan_code;
    BYTE is_extended, is_menu_down, was_key_down, is_key_released;
} x104_input_key_status_t;

typedef struct x104_input_state_t {
    volatile LONG initialized;
    HWND window;
    /* touch path decisions (own copies; the 1.2.8 variables are only READ once, after arm32_input_attach_window) */
    int touch_api;                      /* ARM32_TOUCHAPI_POINTER / WMTOUCH / LEGACY, 0 = touch handling inactive (--input=mouse) */
    BOOL pointer_touch_seen;            /* a PT_TOUCH WM_POINTERDOWN reached us */
    BOOL wm_touch_registered;           /* the window is also registered for WM_TOUCH (auto mode) */
    BOOL mouse_down;                    /* the real mouse left button is pressed and its Pressed was delivered */
    DWORD last_touch_tick;
    BOOL have_mouse_position;
    int last_mouse_x, last_mouse_y;     /* screen */
    /* objects that are not lazy singletons */
    X104Object *logo, *editbox, *button;
    X104Object *devices[3];
    /* event lists */
    X104Event cw_key_down, cw_key_up, cw_ptr[3], cw_visibility;
    X104Event text_changed, pane_showing, pane_hiding;
    /* independent input sources */
    X104Lock source_lock;
    x104_input_source_t *sources[X104_INPUT_MAX_SOURCES];
    volatile LONG source_count;
    /* pointer cursor */
    X104Lock cursor_lock;
    void *cursor;                       /* ICoreCursor the game set (AddRef'd), NULL = hidden */
    BOOL cursor_visible;
    int cursor_type;
    x104_input_cursor_slot_t cursors[X104_INPUT_MAX_CURSORS];
    /* pointers (UI thread only) */
    x104_input_pointer_t pointers[X104_INPUT_MAX_POINTERS];
    volatile LONG pending_release;      /* set by broadcast window messages, handled by the tick (R1) */
    const char * volatile pending_why;
    volatile LONG pending_release_mouse;
    unsigned dropped_events;
    /* the event being delivered (read by the args / point / device / properties fakes) */
    UINT32 cur_id;
    int cur_device;
    float cur_x, cur_y;
    BYTE cur_left, cur_contact, cur_canceled;
    ULONGLONG cur_timestamp;
    int cur_vk;
    x104_input_key_status_t cur_key;
    BYTE cur_visible;
    /* keys (UI thread only) */
    BYTE key_down[256];
    wchar_t high_surrogate;
    /* CoreWindow state */
    BYTE window_visible;
    BYTE input_enabled;
    /* the TextBox */
    X104Lock text_lock;
    volatile LONG text_pending;
    int focused;                        /* X104_INPUT_FOCUS_* */
    int text_len, sel_start, sel_len, max_length;
    BYTE accepts_return, prevent_osk;
    wchar_t text[X104_INPUT_TEXT_CAP];
    /* InputPane */
    X104Rect pane_rect;
    BYTE pane_ensured;
} x104_input_state_t;

static x104_input_state_t x104_input_st;

static const char *const x104_input_ptr_event_names[3] = { "CoreWindow.PointerMoved", "CoreWindow.PointerPressed", "CoreWindow.PointerReleased" };
static const char *const x104_input_src_event_names[3] = { "CoreIndependentInputSource.PointerMoved", "CoreIndependentInputSource.PointerPressed",
                                                           "CoreIndependentInputSource.PointerReleased" };
static const char *const x104_input_device_names[3] = { "touch", "pen", "mouse" };

/* Win32 calls that win32_host.c / win32_compat.h might not declare (identical redeclarations are legal) */
extern HWND WINAPI GetCapture(void);
extern HWND WINAPI SetCapture(HWND);
extern BOOL WINAPI ReleaseCapture(void);
extern HWND WINAPI GetFocus(void);
extern HWND WINAPI SetFocus(HWND);
extern BOOL WINAPI GetCursorPos(POINT *);
extern BOOL WINAPI ScreenToClient(HWND, POINT *);
extern BOOL WINAPI ClientToScreen(HWND, POINT *);
extern BOOL WINAPI GetClientRect(HWND, RECT *);
extern HCURSOR WINAPI SetCursor(HCURSOR);
extern HCURSOR WINAPI LoadCursorW(HINSTANCE, LPCWSTR);
extern LPARAM WINAPI GetMessageExtraInfo(void);
extern SHORT WINAPI GetKeyState(int);
extern DWORD WINAPI GetCurrentThreadId(void);
extern BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *);
extern BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *);

/* ======================================================================================================================
 * 2. small helpers
 * ====================================================================================================================== */
static void x104_input_client_size(float *width, float *height)
{
    RECT client;
    *width = 0.0f;
    *height = 0.0f;
    x104_panel_client_size(width, height);
    if ((*width < 1.0f || *height < 1.0f) && x104_input_st.window && GetClientRect(x104_input_st.window, &client)) {
        *width = (float)(client.right - client.left);
        *height = (float)(client.bottom - client.top);
    }
}

/* screen -> client pixels, clamped to the client area (a drag may leave the window; the clamp keeps a joystick alive) */
static void x104_input_clamp_client(HWND window, int *x, int *y)
{
    RECT client;
    if (window && GetClientRect(window, &client)) {
        if (*x >= client.right) *x = client.right - 1;
        if (*y >= client.bottom) *y = client.bottom - 1;
    }
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
}

static void x104_input_screen_to_client(HWND window, int screen_x, int screen_y, int *client_x, int *client_y)
{
    POINT point;
    point.x = screen_x;
    point.y = screen_y;
    if (!ScreenToClient(window, &point)) {
        point.x = screen_x;
        point.y = screen_y;
    }
    *client_x = point.x;
    *client_y = point.y;
    x104_input_clamp_client(window, client_x, client_y);
}

/* PointerPoint.Timestamp: microseconds of the performance counter (nobody in the game reads it) */
static ULONGLONG x104_input_timestamp_us(void)
{
    LARGE_INTEGER now, frequency;
    if (!QueryPerformanceCounter(&now) || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) return (ULONGLONG)GetTickCount() * 1000u;
    return ((ULONGLONG)now.QuadPart / (ULONGLONG)frequency.QuadPart) * 1000000u +
           (((ULONGLONG)now.QuadPart % (ULONGLONG)frequency.QuadPart) * 1000000u) / (ULONGLONG)frequency.QuadPart;
}

static BOOL x104_input_any_touch_down(void)
{
    unsigned index;
    for (index = 0; index < X104_INPUT_MAX_POINTERS; ++index) {
        if (x104_input_st.pointers[index].used && x104_input_st.pointers[index].device != X104_INPUT_DEV_MOUSE) return TRUE;
    }
    return FALSE;
}

static BOOL x104_input_any_pointer_down(void)
{
    unsigned index;
    for (index = 0; index < X104_INPUT_MAX_POINTERS; ++index) {
        if (x104_input_st.pointers[index].used) return TRUE;
    }
    return FALSE;
}

/* ======================================================================================================================
 * 3. event-args objects.  One reusable instance of each, never freed (deliveries are serialised on the UI thread).
 *    The pointer the game receives MUST be the view of exactly the interface it calls (R10): pointer args -> IPointerEventArgs (views[0]).
 * ====================================================================================================================== */
/* ---- ICoreWindowEventArgs (Handled), shared by pointer / key / visibility args: the flag lives in obj->user[1] ---- */
static HRESULT X104_API x104_input_cwargs_get_handled(void *self, BYTE *value)
{
    if (!value) return E_POINTER;
    *value = (BYTE)(ULONG_PTR)X104_OBJ(self)->user[1];
    return S_OK;
}

static HRESULT X104_API x104_input_cwargs_put_handled(void *self, BYTE value)
{
    X104_OBJ(self)->user[1] = (void *)(ULONG_PTR)(value ? 1 : 0);
    return S_OK;
}

X104_VTBL(x104_input_vt_cwargs, ICoreWindowEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreWindowEventArgs, get_Handled, x104_input_cwargs_get_handled),
    X104_SLOT(ICoreWindowEventArgs, put_Handled, x104_input_cwargs_put_handled)
};

/* ---- PointerPointProperties ---- */
static HRESULT X104_API x104_input_props_get_pressure(void *self, float *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.cur_contact ? 0.5f : 0.0f;
    return S_OK;
}

static HRESULT X104_API x104_input_props_get_left(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.cur_left ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_props_get_false(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_input_props_get_true(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 1;
    return S_OK;
}

static HRESULT X104_API x104_input_props_get_canceled(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.cur_canceled ? 1 : 0;
    return S_OK;
}

X104_VTBL(x104_input_vt_props, IPointerPointProperties) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IPointerPointProperties, get_Pressure, x104_input_props_get_pressure),
    X104_SLOT(IPointerPointProperties, get_IsInverted, x104_input_props_get_false),
    X104_SLOT(IPointerPointProperties, get_IsEraser, x104_input_props_get_false),
    X104_SLOT(IPointerPointProperties, get_IsLeftButtonPressed, x104_input_props_get_left),
    X104_SLOT(IPointerPointProperties, get_IsRightButtonPressed, x104_input_props_get_false),
    X104_SLOT(IPointerPointProperties, get_IsMiddleButtonPressed, x104_input_props_get_false),
    X104_SLOT(IPointerPointProperties, get_IsPrimary, x104_input_props_get_true),
    X104_SLOT(IPointerPointProperties, get_IsInRange, x104_input_props_get_true),
    X104_SLOT(IPointerPointProperties, get_IsCanceled, x104_input_props_get_canceled),
    X104_SLOT(IPointerPointProperties, get_IsBarrelButtonPressed, x104_input_props_get_false)
};
static const X104IfaceDesc x104_input_props_ifaces[] = { X104_IFACE(IPointerPointProperties, x104_input_vt_props) };
X104_CLASS_DEF(x104_input_props_class, "PointerPointProperties", L"Windows.UI.Input.PointerPointProperties", x104_input_props_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_props, x104_input_props_class, X104Object)

/* ---- PointerDevice (three objects: user[0] = device type) ---- */
static HRESULT X104_API x104_input_dev_get_type(void *self, int *value)
{
    if (!value) return E_POINTER;
    *value = (int)(ULONG_PTR)X104_OBJ(self)->user[0];
    return S_OK;
}

static HRESULT X104_API x104_input_dev_get_integrated(void *self, BYTE *value)
{
    if (!value) return E_POINTER;
    *value = ((int)(ULONG_PTR)X104_OBJ(self)->user[0] == X104_INPUT_DEV_TOUCH) ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_dev_get_max_contacts(void *self, UINT32 *value)
{
    if (!value) return E_POINTER;
    *value = ((int)(ULONG_PTR)X104_OBJ(self)->user[0] == X104_INPUT_DEV_TOUCH) ? X104_INPUT_MAX_POINTERS : 1;
    return S_OK;
}

X104_VTBL(x104_input_vt_device, IPointerDevice) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IPointerDevice, get_PointerDeviceType, x104_input_dev_get_type),
    X104_SLOT(IPointerDevice, get_IsIntegrated, x104_input_dev_get_integrated),
    X104_SLOT(IPointerDevice, get_MaxContacts, x104_input_dev_get_max_contacts)
};
static const X104IfaceDesc x104_input_device_ifaces[] = { X104_IFACE(IPointerDevice, x104_input_vt_device) };
X104_CLASS_DEF(x104_input_device_class, "PointerDevice", L"Windows.Devices.Input.PointerDevice", x104_input_device_ifaces, X104_CLASS_AGILE, NULL);

/* ---- PointerPoint (reads the event being delivered) ---- */
static HRESULT X104_API x104_input_pt_get_device(void *self, void **value)
{
    int device = x104_input_st.cur_device;
    (void)self;
    if (device < 0 || device > 2) device = X104_INPUT_DEV_TOUCH;
    return x104_return_iface(x104_input_st.devices[device], &X104_IID_IPointerDevice, value, "PointerPoint.get_PointerDevice");
}

static HRESULT X104_API x104_input_pt_get_position(void *self, X104Point *value)
{
    (void)self;
    if (!value) return E_POINTER;
    value->X = x104_input_st.cur_x;
    value->Y = x104_input_st.cur_y;
    return S_OK;
}

static HRESULT X104_API x104_input_pt_get_id(void *self, UINT32 *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.cur_id;
    return S_OK;
}

static HRESULT X104_API x104_input_pt_get_frame(void *self, UINT32 *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_input_pt_get_timestamp(void *self, ULONGLONG *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.cur_timestamp;
    return S_OK;
}

static HRESULT X104_API x104_input_pt_get_in_contact(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.cur_contact ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_pt_get_properties(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_input_get_props(), &X104_IID_IPointerPointProperties, value, "PointerPoint.get_Properties");
}

X104_VTBL(x104_input_vt_point, IPointerPoint) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IPointerPoint, get_PointerDevice, x104_input_pt_get_device),
    X104_SLOT(IPointerPoint, get_Position, x104_input_pt_get_position),
    X104_SLOT(IPointerPoint, get_RawPosition, x104_input_pt_get_position),
    X104_SLOT(IPointerPoint, get_PointerId, x104_input_pt_get_id),
    X104_SLOT(IPointerPoint, get_FrameId, x104_input_pt_get_frame),
    X104_SLOT(IPointerPoint, get_Timestamp, x104_input_pt_get_timestamp),
    X104_SLOT(IPointerPoint, get_IsInContact, x104_input_pt_get_in_contact),
    X104_SLOT(IPointerPoint, get_Properties, x104_input_pt_get_properties)
};
static const X104IfaceDesc x104_input_point_ifaces[] = { X104_IFACE(IPointerPoint, x104_input_vt_point) };
X104_CLASS_DEF(x104_input_point_class, "PointerPoint", L"Windows.UI.Input.PointerPoint", x104_input_point_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_point, x104_input_point_class, X104Object)

/* ---- PointerEventArgs: views[0] = IPointerEventArgs (the game calls get_CurrentPoint straight on the pointer it receives), + ICoreWindowEventArgs ---- */
static HRESULT X104_API x104_input_pargs_get_current_point(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_input_get_point(), &X104_IID_IPointerPoint, value, "PointerEventArgs.get_CurrentPoint");
}

static HRESULT X104_API x104_input_pargs_get_modifiers(void *self, int *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

X104_VTBL(x104_input_vt_pargs, IPointerEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IPointerEventArgs, get_CurrentPoint, x104_input_pargs_get_current_point),
    X104_SLOT(IPointerEventArgs, get_KeyModifiers, x104_input_pargs_get_modifiers)
};
static const X104IfaceDesc x104_input_pargs_ifaces[] = { X104_IFACE(IPointerEventArgs, x104_input_vt_pargs), X104_IFACE(ICoreWindowEventArgs, x104_input_vt_cwargs) };
X104_CLASS_DEF(x104_input_pargs_class, "PointerEventArgs", L"Windows.UI.Core.PointerEventArgs", x104_input_pargs_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_pointer_args, x104_input_pargs_class, X104Object)

/* ---- KeyEventArgs: views[0] = IKeyEventArgs (get_VirtualKey / get_KeyStatus are called straight on the pointer) ---- */
static HRESULT X104_API x104_input_kargs_get_vk(void *self, int *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.cur_vk;
    return S_OK;
}

static HRESULT X104_API x104_input_kargs_get_status(void *self, void *value)
{
    (void)self;
    if (!value) return E_POINTER;
    memcpy(value, &x104_input_st.cur_key, sizeof(x104_input_st.cur_key));      /* exactly 12 bytes (CorePhysicalKeyStatus) */
    return S_OK;
}

X104_VTBL(x104_input_vt_kargs, IKeyEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IKeyEventArgs, get_VirtualKey, x104_input_kargs_get_vk),
    X104_SLOT(IKeyEventArgs, get_KeyStatus, x104_input_kargs_get_status)
};
static const X104IfaceDesc x104_input_kargs_ifaces[] = { X104_IFACE(IKeyEventArgs, x104_input_vt_kargs), X104_IFACE(ICoreWindowEventArgs, x104_input_vt_cwargs) };
X104_CLASS_DEF(x104_input_kargs_class, "KeyEventArgs", L"Windows.UI.Core.KeyEventArgs", x104_input_kargs_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_key_args, x104_input_kargs_class, X104Object)

/* ---- VisibilityChangedEventArgs: views[0] = IVisibilityChangedEventArgs (get_Visible is called straight on the pointer at 0x1DF9CC) ---- */
static HRESULT X104_API x104_input_vargs_get_visible(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.cur_visible ? 1 : 0;
    return S_OK;
}

X104_VTBL(x104_input_vt_vargs, IVisibilityChangedEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IVisibilityChangedEventArgs, get_Visible, x104_input_vargs_get_visible)
};
static const X104IfaceDesc x104_input_vargs_ifaces[] = { X104_IFACE(IVisibilityChangedEventArgs, x104_input_vt_vargs), X104_IFACE(ICoreWindowEventArgs, x104_input_vt_cwargs) };
X104_CLASS_DEF(x104_input_vargs_class, "VisibilityChangedEventArgs", L"Windows.UI.Core.VisibilityChangedEventArgs", x104_input_vargs_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_vis_args, x104_input_vargs_class, X104Object)

/* ---- TextChangedEventArgs (the game's handler ignores it; holes are fine) ---- */
X104_VTBL(x104_input_vt_targs, ITextChangedEventArgs) = { X104_INSPECTABLE_SLOTS };
static const X104IfaceDesc x104_input_targs_ifaces[] = { X104_IFACE(ITextChangedEventArgs, x104_input_vt_targs) };
X104_CLASS_DEF(x104_input_targs_class, "TextChangedEventArgs", L"Windows.UI.Xaml.Controls.TextChangedEventArgs", x104_input_targs_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_text_args, x104_input_targs_class, X104Object)

/* ======================================================================================================================
 * 4. event registration thunks (shared logic).  Registrations come from game threads (the independent-source ones from a pool thread): X104Event locks.
 * ====================================================================================================================== */
static HRESULT x104_input_event_add(X104Event *event, const char *what, void *handler, LONGLONG *token)
{
    static volatile LONG logged;
    HRESULT hr = x104_event_add(event, handler, token);
    X104_LOG_LIMITED(&logged, 120, "X104 input: %s handler=%p -> 0x%08lx %s", what, handler, (unsigned long)hr, x104_hr_name(hr));
    return hr;
}

#define X104_INPUT_EVENT_PAIR(name, label, event_expr) \
    static HRESULT X104_API name##_add(void *self, void *handler, LONGLONG *token) { (void)self; return x104_input_event_add(&(event_expr), label, handler, token); } \
    static HRESULT X104_API name##_remove(void *self, LONGLONG token) { (void)self; return x104_event_remove(&(event_expr), token); }

/* ======================================================================================================================
 * 5. CoreWindow (+ statics), CoreCursor (+ factory), CoreIndependentInputSource
 * ====================================================================================================================== */
X104_INPUT_EVENT_PAIR(x104_input_cw_key_down, "CoreWindow.add_KeyDown", x104_input_st.cw_key_down)
X104_INPUT_EVENT_PAIR(x104_input_cw_key_up, "CoreWindow.add_KeyUp", x104_input_st.cw_key_up)
X104_INPUT_EVENT_PAIR(x104_input_cw_moved, "CoreWindow.add_PointerMoved", x104_input_st.cw_ptr[X104_INPUT_EV_MOVED])
X104_INPUT_EVENT_PAIR(x104_input_cw_pressed, "CoreWindow.add_PointerPressed", x104_input_st.cw_ptr[X104_INPUT_EV_PRESSED])
X104_INPUT_EVENT_PAIR(x104_input_cw_released, "CoreWindow.add_PointerReleased", x104_input_st.cw_ptr[X104_INPUT_EV_RELEASED])
X104_INPUT_EVENT_PAIR(x104_input_cw_visibility, "CoreWindow.add_VisibilityChanged", x104_input_st.cw_visibility)

static HRESULT X104_API x104_input_cw_get_bounds(void *self, X104Rect *value)
{
    float width, height;
    (void)self;
    if (!value) return E_POINTER;
    x104_input_client_size(&width, &height);
    value->X = 0.0f;
    value->Y = 0.0f;
    value->Width = width;
    value->Height = height;
    return S_OK;
}

static HRESULT X104_API x104_input_cw_get_dispatcher(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_async_dispatcher_object(), &X104_IID_ICoreDispatcher, value, "CoreWindow.get_Dispatcher");
}

static HRESULT X104_API x104_input_cw_get_input_enabled(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.input_enabled ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_cw_put_input_enabled(void *self, BYTE value)
{
    (void)self;
    x104_input_st.input_enabled = value ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_cw_get_visible(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.window_visible ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_cw_get_pointer_position(void *self, X104Point *value)
{
    POINT point;
    (void)self;
    if (!value) return E_POINTER;
    value->X = 0.0f;
    value->Y = 0.0f;
    if (GetCursorPos(&point) && x104_input_st.window && ScreenToClient(x104_input_st.window, &point)) {
        value->X = (float)point.x;
        value->Y = (float)point.y;
    }
    return S_OK;
}

/* the cursor the game last set (NULL = hide the Windows cursor over the window).  Applied right away and by every WM_SETCURSOR. */
static HCURSOR x104_input_current_hcursor(void)
{
    /* CoreCursorType: Arrow 0, Cross 1, Custom 2, Hand 3, Help 4, IBeam 5, SizeAll 6, SizeNESW 7, SizeNS 8, SizeNWSE 9, SizeWE 10, UniversalNo 11, UpArrow 12, Wait 13 */
    static const unsigned short idc[14] = { 32512, 32515, 32512, 32649, 32651, 32513, 32646, 32643, 32645, 32642, 32644, 32648, 32516, 32514 };
    int type = x104_input_st.cursor_type;
    if (!x104_input_st.cursor_visible) return NULL;
    if (type < 0 || type > 13) type = 0;
    return LoadCursorW(NULL, MAKEINTRESOURCEW(idc[type]));
}

static HRESULT X104_API x104_input_cw_get_pointer_cursor(void *self, void **value)
{
    (void)self;
    if (!value) return E_POINTER;
    x104_lock(&x104_input_st.cursor_lock);
    *value = x104_input_st.cursor;
    if (*value) x104_raw_addref(*value);
    x104_unlock(&x104_input_st.cursor_lock);
    return S_OK;
}

static HRESULT X104_API x104_input_cw_put_pointer_cursor(void *self, void *cursor)
{
    static volatile LONG logged;
    void *previous;
    unsigned index;
    int type = 0;
    (void)self;
    if (cursor) x104_raw_addref(cursor);
    x104_lock(&x104_input_st.cursor_lock);
    previous = x104_input_st.cursor;
    x104_input_st.cursor = cursor;
    x104_input_st.cursor_visible = cursor ? TRUE : FALSE;
    for (index = 0; cursor && index < X104_INPUT_MAX_CURSORS; ++index) {
        if (x104_input_st.cursors[index].object && (void *)&x104_input_st.cursors[index].object->views[0] == cursor) {
            type = x104_input_st.cursors[index].type;
            break;
        }
    }
    x104_input_st.cursor_type = type;
    x104_unlock(&x104_input_st.cursor_lock);
    if (previous) x104_raw_release(previous);
    X104_LOG_LIMITED(&logged, 30, "X104 input: CoreWindow.put_PointerCursor(%p) -> Windows cursor %s (type %d)", cursor, cursor ? "shown" : "hidden", type);
    SetCursor(x104_input_current_hcursor());
    return S_OK;
}

static HRESULT X104_API x104_input_cw_get_key_state(void *self, int virtual_key, int *state)
{
    SHORT value;
    (void)self;
    if (!state) return E_POINTER;
    value = GetKeyState(virtual_key);
    *state = ((value & 0x8000) ? 1 : 0) | ((value & 1) ? 2 : 0);      /* CoreVirtualKeyStates: Down 1, Locked 2 */
    return S_OK;
}

X104_VTBL(x104_input_vt_corewindow, ICoreWindow) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreWindow, get_Bounds, x104_input_cw_get_bounds),
    X104_SLOT(ICoreWindow, get_Dispatcher, x104_input_cw_get_dispatcher),
    X104_SLOT(ICoreWindow, get_IsInputEnabled, x104_input_cw_get_input_enabled),
    X104_SLOT(ICoreWindow, put_IsInputEnabled, x104_input_cw_put_input_enabled),
    X104_SLOT(ICoreWindow, get_PointerCursor, x104_input_cw_get_pointer_cursor),
    X104_SLOT(ICoreWindow, put_PointerCursor, x104_input_cw_put_pointer_cursor),
    X104_SLOT(ICoreWindow, get_PointerPosition, x104_input_cw_get_pointer_position),
    X104_SLOT(ICoreWindow, get_Visible, x104_input_cw_get_visible),
    X104_SLOT(ICoreWindow, GetAsyncKeyState, x104_input_cw_get_key_state),
    X104_SLOT(ICoreWindow, GetKeyState, x104_input_cw_get_key_state),
    X104_SLOT_NOARGS(ICoreWindow, ReleasePointerCapture, x104_nop),
    X104_SLOT_NOARGS(ICoreWindow, SetPointerCapture, x104_nop),
    X104_SLOT(ICoreWindow, add_KeyDown, x104_input_cw_key_down_add),
    X104_SLOT(ICoreWindow, remove_KeyDown, x104_input_cw_key_down_remove),
    X104_SLOT(ICoreWindow, add_KeyUp, x104_input_cw_key_up_add),
    X104_SLOT(ICoreWindow, remove_KeyUp, x104_input_cw_key_up_remove),
    X104_SLOT(ICoreWindow, add_PointerMoved, x104_input_cw_moved_add),
    X104_SLOT(ICoreWindow, remove_PointerMoved, x104_input_cw_moved_remove),
    X104_SLOT(ICoreWindow, add_PointerPressed, x104_input_cw_pressed_add),
    X104_SLOT(ICoreWindow, remove_PointerPressed, x104_input_cw_pressed_remove),
    X104_SLOT(ICoreWindow, add_PointerReleased, x104_input_cw_released_add),
    X104_SLOT(ICoreWindow, remove_PointerReleased, x104_input_cw_released_remove),
    X104_SLOT(ICoreWindow, add_VisibilityChanged, x104_input_cw_visibility_add),
    X104_SLOT(ICoreWindow, remove_VisibilityChanged, x104_input_cw_visibility_remove)
};
static const X104IfaceDesc x104_input_cw_ifaces[] = { X104_IFACE(ICoreWindow, x104_input_vt_corewindow) };
X104_CLASS_DEF(x104_input_cw_class, "CoreWindow", L"Windows.UI.Core.CoreWindow", x104_input_cw_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_core_window, x104_input_cw_class, X104Object)

/* ---- CoreWindow statics: GetForCurrentThread (helper 0x111420, lazily, from the cursor lambdas on the UI thread) ---- */
static HRESULT X104_API x104_input_cwstatic_get_for_current_thread(void *self, void **window)
{
    (void)self;
    return x104_return_iface(x104_input_core_window(), &X104_IID_ICoreWindow, window, "CoreWindow.GetForCurrentThread");
}

X104_VTBL(x104_input_vt_cwstatic, ICoreWindowStatic) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreWindowStatic, GetForCurrentThread, x104_input_cwstatic_get_for_current_thread)
};
static const X104IfaceDesc x104_input_cwstatic_ifaces[] = { X104_IFACE(ICoreWindowStatic, x104_input_vt_cwstatic) };
X104_CLASS_DEF(x104_input_cwstatic_class, "CoreWindowStatics", L"Windows.UI.Core.CoreWindow", x104_input_cwstatic_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_input_get_cwstatic, x104_input_cwstatic_class, X104Object)

/* ---- CoreCursor + factory: the game makes CoreCursor(Arrow, 0) on every mouse release; cached by (type, id) so the never-freed objects stay bounded ---- */
static HRESULT X104_API x104_input_cursor_get_id(void *self, UINT32 *value)
{
    if (!value) return E_POINTER;
    *value = (UINT32)(ULONG_PTR)X104_OBJ(self)->user[1];
    return S_OK;
}

static HRESULT X104_API x104_input_cursor_get_type(void *self, int *value)
{
    if (!value) return E_POINTER;
    *value = (int)(ULONG_PTR)X104_OBJ(self)->user[0];
    return S_OK;
}

X104_VTBL(x104_input_vt_cursor, ICoreCursor) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreCursor, get_Id, x104_input_cursor_get_id),
    X104_SLOT(ICoreCursor, get_Type, x104_input_cursor_get_type)
};
static const X104IfaceDesc x104_input_cursor_ifaces[] = { X104_IFACE(ICoreCursor, x104_input_vt_cursor) };
X104_CLASS_DEF(x104_input_cursor_class, "CoreCursor", L"Windows.UI.Core.CoreCursor", x104_input_cursor_ifaces, X104_CLASS_AGILE, NULL);

static HRESULT X104_API x104_input_cursorfactory_create(void *self, int type, UINT32 id, void **cursor)
{
    static volatile LONG logged;
    X104Object *object = NULL;
    unsigned index;
    HRESULT hr;
    (void)self;
    if (!cursor) return E_POINTER;
    *cursor = NULL;
    x104_lock(&x104_input_st.cursor_lock);
    for (index = 0; index < X104_INPUT_MAX_CURSORS; ++index) {
        if (x104_input_st.cursors[index].object && x104_input_st.cursors[index].type == type && x104_input_st.cursors[index].id == id) {
            object = x104_input_st.cursors[index].object;
            break;
        }
    }
    if (!object) {
        for (index = 0; index < X104_INPUT_MAX_CURSORS; ++index) {
            if (!x104_input_st.cursors[index].object) {
                object = x104_object_new(&x104_input_cursor_class, sizeof(X104Object));
                if (object) {
                    object->user[0] = (void *)(ULONG_PTR)type;
                    object->user[1] = (void *)(ULONG_PTR)id;
                    x104_input_st.cursors[index].type = type;
                    x104_input_st.cursors[index].id = id;
                    x104_input_st.cursors[index].object = object;
                }
                break;
            }
        }
    }
    x104_unlock(&x104_input_st.cursor_lock);
    if (!object) {
        /* table full (the game uses one or two kinds): hand out an uncached object rather than failing the game */
        object = x104_object_new(&x104_input_cursor_class, sizeof(X104Object));
        if (!object) {
            x104_logf("X104 input: ERROR CoreCursor.CreateCursor(type=%d id=%u): out of memory -> E_OUTOFMEMORY", type, (unsigned)id);
            return E_OUTOFMEMORY;
        }
        object->user[0] = (void *)(ULONG_PTR)type;
        object->user[1] = (void *)(ULONG_PTR)id;
    }
    hr = x104_object_qi(object, &X104_IID_ICoreCursor, cursor);
    X104_LOG_LIMITED(&logged, 12, "X104 input: CoreCursor.CreateCursor(type=%d id=%u) -> %p hr=0x%08lx", type, (unsigned)id, *cursor, (unsigned long)hr);
    return hr;
}

X104_VTBL(x104_input_vt_cursorfactory, ICoreCursorFactory) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreCursorFactory, CreateCursor, x104_input_cursorfactory_create)
};
static const X104IfaceDesc x104_input_cursorfactory_ifaces[] = { X104_IFACE(ICoreCursorFactory, x104_input_vt_cursorfactory) };
X104_CLASS_DEF(x104_input_cursorfactory_class, "CoreCursorFactory", L"Windows.UI.Core.CoreCursor", x104_input_cursorfactory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_input_get_cursorfactory, x104_input_cursorfactory_class, X104Object)

/* ---- CoreIndependentInputSource: views[0] = ICoreInputSourceBase (the game calls get_Dispatcher straight on the pointer it gets back), + ICorePointerInputSource ---- */
static HRESULT X104_API x104_input_src_get_dispatcher(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_async_dispatcher_object(), &X104_IID_ICoreDispatcher, value, "CoreIndependentInputSource.get_Dispatcher");
}

static HRESULT X104_API x104_input_src_get_input_enabled(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 1;
    return S_OK;
}

static HRESULT X104_API x104_input_src_get_has_capture(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_input_src_get_pointer_cursor(void *self, void **value)
{
    return x104_input_cw_get_pointer_cursor(self, value);
}

X104_INPUT_EVENT_PAIR(x104_input_src_moved, "CoreIndependentInputSource.add_PointerMoved", X104_THIS(x104_input_source_t, self)->ptr[X104_INPUT_EV_MOVED])
X104_INPUT_EVENT_PAIR(x104_input_src_pressed, "CoreIndependentInputSource.add_PointerPressed", X104_THIS(x104_input_source_t, self)->ptr[X104_INPUT_EV_PRESSED])
X104_INPUT_EVENT_PAIR(x104_input_src_released, "CoreIndependentInputSource.add_PointerReleased", X104_THIS(x104_input_source_t, self)->ptr[X104_INPUT_EV_RELEASED])

X104_VTBL(x104_input_vt_srcbase, ICoreInputSourceBase) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreInputSourceBase, get_Dispatcher, x104_input_src_get_dispatcher),
    X104_SLOT(ICoreInputSourceBase, get_IsInputEnabled, x104_input_src_get_input_enabled),
    X104_SLOT_NOARGS(ICoreInputSourceBase, put_IsInputEnabled, x104_nop)
};
X104_VTBL(x104_input_vt_srcptr, ICorePointerInputSource) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT_NOARGS(ICorePointerInputSource, ReleasePointerCapture, x104_nop),
    X104_SLOT_NOARGS(ICorePointerInputSource, SetPointerCapture, x104_nop),
    X104_SLOT(ICorePointerInputSource, get_HasCapture, x104_input_src_get_has_capture),
    X104_SLOT(ICorePointerInputSource, get_PointerPosition, x104_input_cw_get_pointer_position),
    X104_SLOT(ICorePointerInputSource, get_PointerCursor, x104_input_src_get_pointer_cursor),
    X104_SLOT(ICorePointerInputSource, put_PointerCursor, x104_input_cw_put_pointer_cursor),
    X104_SLOT(ICorePointerInputSource, add_PointerMoved, x104_input_src_moved_add),
    X104_SLOT(ICorePointerInputSource, remove_PointerMoved, x104_input_src_moved_remove),
    X104_SLOT(ICorePointerInputSource, add_PointerPressed, x104_input_src_pressed_add),
    X104_SLOT(ICorePointerInputSource, remove_PointerPressed, x104_input_src_pressed_remove),
    X104_SLOT(ICorePointerInputSource, add_PointerReleased, x104_input_src_released_add),
    X104_SLOT(ICorePointerInputSource, remove_PointerReleased, x104_input_src_released_remove)
};
static const X104IfaceDesc x104_input_src_ifaces[] = { X104_IFACE(ICoreInputSourceBase, x104_input_vt_srcbase), X104_IFACE(ICorePointerInputSource, x104_input_vt_srcptr) };
X104_CLASS_DEF(x104_input_src_class, "CoreIndependentInputSource", L"Windows.UI.Core.CoreIndependentInputSource", x104_input_src_ifaces, X104_CLASS_AGILE, NULL);

/* called by the panel's ISwapChainPanel.CreateCoreIndependentInputSource(7, &source) on the input work item's POOL thread; thread-safe.
 * Returns the ICoreInputSourceBase view, AddRef'd (the game then QIs ICorePointerInputSource and calls get_Dispatcher on it). */
static HRESULT x104_input_create_independent_source(UINT device_types, void **result)
{
    x104_input_source_t *source;
    LONG slot;
    unsigned index;
    if (!result) return E_POINTER;
    *result = NULL;
    source = (x104_input_source_t *)x104_object_new(&x104_input_src_class, sizeof(x104_input_source_t));
    if (!source) {
        x104_logf("X104 input: ERROR independent input source (types=%u): out of memory -> E_OUTOFMEMORY", (unsigned)device_types);
        return E_OUTOFMEMORY;
    }
    for (index = 0; index < 3; ++index) x104_event_init(&source->ptr[index], x104_input_src_event_names[index]);
    source->types = device_types;
    x104_lock(&x104_input_st.source_lock);
    slot = x104_input_st.source_count;
    if (slot < X104_INPUT_MAX_SOURCES) {
        source->number = (int)slot + 1;
        x104_input_st.sources[slot] = source;
        __atomic_store_n(&x104_input_st.source_count, slot + 1, __ATOMIC_RELEASE);
    }
    x104_unlock(&x104_input_st.source_lock);
    if (slot >= X104_INPUT_MAX_SOURCES) {
        x104_logf("X104 input: ERROR independent input source (types=%u): the table of %d sources is full -> E_FAIL", (unsigned)device_types, X104_INPUT_MAX_SOURCES);
        return E_FAIL;
    }
    x104_logf("X104 input: independent input source #%d created (types=%u) on thread %04lx", source->number, (unsigned)device_types,
              (unsigned long)GetCurrentThreadId());
    return x104_object_qi(&source->base, &X104_IID_ICoreInputSourceBase, result);
}

/* ======================================================================================================================
 * 6. the TextBox model (EditBox) and the XAML elements Logo / EditBox / Button
 * ====================================================================================================================== */
static void x104_input_textchanged_run(void *argument)
{
    static volatile LONG logged;
    unsigned handlers;
    int length;
    X104Object *editbox = x104_input_st.editbox;
    (void)argument;
    InterlockedExchange(&x104_input_st.text_pending, 0);          /* a put_Text made by the handler itself schedules the next one */
    if (!editbox) return;
    length = x104_input_st.text_len;
    handlers = x104_event_invoke_guarded(&x104_input_st.text_changed, &editbox->views[0], &x104_input_get_text_args()->views[0]);
    X104_LOG_LIMITED(&logged, 30, "X104 input: TextChanged raised (len=%d, handlers=%u)", length, handlers);
}

/* exactly one TextChanged after any number of modifications, delivered asynchronously on the UI thread (R14) */
static void x104_input_text_schedule(const char *why)
{
    static volatile LONG logged;
    if (__sync_val_compare_and_swap(&x104_input_st.text_pending, 0, 1) != 0) return;     /* one is already on its way and will see the newest text */
    if (!x104_ui_post(x104_input_textchanged_run, NULL)) {
        InterlockedExchange(&x104_input_st.text_pending, 0);
        X104_LOG_LIMITED(&logged, 5, "X104 input: WARNING TextChanged (%s) not scheduled: the UI queue is closed (shutdown)", why);
    }
}

static HRESULT X104_API x104_input_tb_get_text(void *self, HSTRING *value)
{
    HRESULT hr;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    x104_lock(&x104_input_st.text_lock);
    hr = WindowsCreateString(x104_input_st.text_len ? x104_input_st.text : NULL, (UINT32)x104_input_st.text_len, value);
    x104_unlock(&x104_input_st.text_lock);
    if (FAILED(hr)) x104_logf("X104 input: ERROR TextBox.get_Text: WindowsCreateString(len=%d) failed 0x%08lx", x104_input_st.text_len, (unsigned long)hr);
    return hr;
}

/* the game's put_Text (show keyboard, UpdateText, CRLF strip): the caret goes to the end (the game follows with Select()); ALWAYS one TextChanged afterwards */
static HRESULT X104_API x104_input_tb_put_text(void *self, HSTRING value)
{
    static volatile LONG logged;
    UINT32 length = 0;
    const wchar_t *raw = value ? WindowsGetStringRawBuffer(value, &length) : NULL;
    UINT32 copy = length;
    BOOL truncated = FALSE;
    (void)self;
    if (!raw) copy = 0;
    if (copy > X104_INPUT_TEXT_CAP - 1) {
        copy = X104_INPUT_TEXT_CAP - 1;
        truncated = TRUE;
    }
    x104_lock(&x104_input_st.text_lock);
    if (copy) memcpy(x104_input_st.text, raw, copy * sizeof(wchar_t));
    x104_input_st.text[copy] = 0;
    x104_input_st.text_len = (int)copy;
    x104_input_st.sel_start = (int)copy;
    x104_input_st.sel_len = 0;
    x104_unlock(&x104_input_st.text_lock);
    X104_LOG_LIMITED(&logged, 40, "X104 input: TextBox put_Text len=%d%s -> TextChanged scheduled", (int)copy, truncated ? " (TRUNCATED to the capacity)" : "");
    x104_input_text_schedule("put_Text");
    return S_OK;
}

static HRESULT X104_API x104_input_tb_get_sel_length(void *self, INT *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.sel_len;
    return S_OK;
}

static HRESULT X104_API x104_input_tb_get_sel_start(void *self, INT *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.sel_start;
    return S_OK;
}

static void x104_input_tb_select_locked(INT start, INT length)
{
    if (start < 0) start = 0;
    if (start > x104_input_st.text_len) start = x104_input_st.text_len;
    if (length < 0) length = 0;
    if (length > x104_input_st.text_len - start) length = x104_input_st.text_len - start;
    x104_input_st.sel_start = start;
    x104_input_st.sel_len = length;
}

static HRESULT X104_API x104_input_tb_select(void *self, INT start, INT length)
{
    (void)self;
    x104_lock(&x104_input_st.text_lock);
    x104_input_tb_select_locked(start, length);
    x104_unlock(&x104_input_st.text_lock);
    return S_OK;
}

static HRESULT X104_API x104_input_tb_select_all(void *self)
{
    return x104_input_tb_select(self, 0, x104_input_st.text_len);
}

static HRESULT X104_API x104_input_tb_put_sel_length(void *self, INT value)
{
    return x104_input_tb_select(self, x104_input_st.sel_start, value);
}

static HRESULT X104_API x104_input_tb_put_sel_start(void *self, INT value)
{
    return x104_input_tb_select(self, value, x104_input_st.sel_len);
}

static HRESULT X104_API x104_input_tb_get_max_length(void *self, INT *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.max_length;
    return S_OK;
}

static HRESULT X104_API x104_input_tb_put_max_length(void *self, INT value)
{
    static volatile LONG logged;
    (void)self;
    if (value < 0) {
        X104_LOG_LIMITED(&logged, 5, "X104 input: WARNING TextBox.put_MaxLength(%d) is negative -> E_INVALIDARG", (int)value);
        return E_INVALIDARG;
    }
    x104_input_st.max_length = value;           /* 0 = unlimited (XAML), a user edit is limited to X104_INPUT_TEXT_CAP-1 in any case */
    X104_TRACE("X104 input: TextBox.put_MaxLength(%d)", (int)value);
    return S_OK;
}

static HRESULT X104_API x104_input_tb_get_accepts_return(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.accepts_return ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_tb_put_accepts_return(void *self, BYTE value)
{
    (void)self;
    x104_input_st.accepts_return = value ? 1 : 0;
    x104_logf("X104 input: ITextBox.put_AcceptsReturn(%d)", (int)x104_input_st.accepts_return);
    return S_OK;
}

X104_INPUT_EVENT_PAIR(x104_input_tb_changed, "ITextBox.add_TextChanged", x104_input_st.text_changed)

static HRESULT X104_API x104_input_tb2_get_prevent(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.prevent_osk ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_tb2_put_prevent(void *self, BYTE value)
{
    (void)self;
    x104_input_st.prevent_osk = value ? 1 : 0;
    x104_logf("X104 input: PreventKeyboardDisplayOnProgrammaticFocus=%d", (int)x104_input_st.prevent_osk);
    return S_OK;
}

/* IControl.Focus(FocusState, bool *result).  The game only ever uses Programmatic (3): EditBox = show the keyboard, Button = hide it (R14). */
static HRESULT X104_API x104_input_edit_focus(void *self, int state, BYTE *result)
{
    static volatile LONG logged;
    (void)self;
    if (result) *result = 1;
    x104_input_st.focused = X104_INPUT_FOCUS_EDITBOX;
    X104_LOG_LIMITED(&logged, 200, "X104 input: EditBox.Focus(%d) -> osk show (text mode on, text len=%d)", state, x104_input_st.text_len);
    x104_set_text_input_active(TRUE, "EditBox.Focus");              /* on EVERY Focus, not only on a change (units/input.md 3.5) */
    x104_osk_show("EditBox.Focus");
    return S_OK;
}

static HRESULT X104_API x104_input_button_focus(void *self, int state, BYTE *result)
{
    static volatile LONG logged;
    (void)self;
    if (result) *result = 1;
    x104_input_st.focused = X104_INPUT_FOCUS_BUTTON;
    X104_LOG_LIMITED(&logged, 200, "X104 input: Button.Focus(%d) -> osk hide (text mode off)", state);
    x104_set_text_input_active(FALSE, "Button.Focus");
    x104_osk_hide("Button.Focus");
    return S_OK;
}

static HRESULT X104_API x104_input_edit_get_focus_state(void *self, int *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = (x104_input_st.focused == X104_INPUT_FOCUS_EDITBOX) ? 3 : 0;
    return S_OK;
}

static HRESULT X104_API x104_input_button_get_focus_state(void *self, int *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = (x104_input_st.focused == X104_INPUT_FOCUS_BUTTON) ? 3 : 0;
    return S_OK;
}

/* ---- Logo: Windows.UI.Xaml.Controls.Image (the game only sets Visibility through the mixin) ---- */
X104_VTBL(x104_input_vt_image, IImage) = { X104_INSPECTABLE_SLOTS };
static const X104IfaceDesc x104_input_logo_ifaces[] = { X104_ELEMENT_IFACES, X104_IFACE(IImage, x104_input_vt_image) };
X104_CLASS_DEF(x104_input_logo_class, "Image(Logo)", L"Windows.UI.Xaml.Controls.Image", x104_input_logo_ifaces, X104_CLASS_AGILE, NULL);

/* ---- EditBox: Windows.UI.Xaml.Controls.TextBox ---- */
X104_VTBL(x104_input_vt_edit_control, IControl) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IControl, get_FocusState, x104_input_edit_get_focus_state),
    X104_SLOT(IControl, Focus, x104_input_edit_focus)
};
X104_VTBL(x104_input_vt_textbox, ITextBox) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ITextBox, get_Text, x104_input_tb_get_text),
    X104_SLOT(ITextBox, put_Text, x104_input_tb_put_text),
    X104_SLOT(ITextBox, get_SelectionLength, x104_input_tb_get_sel_length),
    X104_SLOT(ITextBox, put_SelectionLength, x104_input_tb_put_sel_length),
    X104_SLOT(ITextBox, get_SelectionStart, x104_input_tb_get_sel_start),
    X104_SLOT(ITextBox, put_SelectionStart, x104_input_tb_put_sel_start),
    X104_SLOT(ITextBox, get_MaxLength, x104_input_tb_get_max_length),
    X104_SLOT(ITextBox, put_MaxLength, x104_input_tb_put_max_length),
    X104_SLOT(ITextBox, get_AcceptsReturn, x104_input_tb_get_accepts_return),
    X104_SLOT(ITextBox, put_AcceptsReturn, x104_input_tb_put_accepts_return),
    X104_SLOT(ITextBox, add_TextChanged, x104_input_tb_changed_add),
    X104_SLOT(ITextBox, remove_TextChanged, x104_input_tb_changed_remove),
    X104_SLOT(ITextBox, Select, x104_input_tb_select),
    X104_SLOT(ITextBox, SelectAll, x104_input_tb_select_all)
};
X104_VTBL(x104_input_vt_textbox2, ITextBox2) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ITextBox2, get_PreventKeyboardDisplayOnProgrammaticFocus, x104_input_tb2_get_prevent),
    X104_SLOT(ITextBox2, put_PreventKeyboardDisplayOnProgrammaticFocus, x104_input_tb2_put_prevent)
};
static const X104IfaceDesc x104_input_edit_ifaces[] = {
    X104_ELEMENT_IFACES,
    X104_IFACE(IControl, x104_input_vt_edit_control),
    X104_IFACE(ITextBox, x104_input_vt_textbox),
    X104_IFACE(ITextBox2, x104_input_vt_textbox2)
};
X104_CLASS_DEF(x104_input_edit_class, "TextBox(EditBox)", L"Windows.UI.Xaml.Controls.TextBox", x104_input_edit_ifaces, X104_CLASS_AGILE, NULL);

/* ---- Button: Windows.UI.Xaml.Controls.Button ---- */
X104_VTBL(x104_input_vt_button_control, IControl) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IControl, get_FocusState, x104_input_button_get_focus_state),
    X104_SLOT(IControl, Focus, x104_input_button_focus)
};
X104_VTBL(x104_input_vt_button, IButton) = { X104_INSPECTABLE_SLOTS };
static const X104IfaceDesc x104_input_button_ifaces[] = {
    X104_ELEMENT_IFACES,
    X104_IFACE(IControl, x104_input_vt_button_control),
    X104_IFACE(IButton, x104_input_vt_button)
};
X104_CLASS_DEF(x104_input_button_class, "Button", L"Windows.UI.Xaml.Controls.Button", x104_input_button_ifaces, X104_CLASS_AGILE, NULL);

/* overlapping move of wide characters (memmove is not declared by the compat header) */
static void x104_input_move_wchars(wchar_t *destination, const wchar_t *source, int count)
{
    int index;
    if (destination < source) {
        for (index = 0; index < count; ++index) destination[index] = source[index];
    } else if (destination > source) {
        for (index = count - 1; index >= 0; --index) destination[index] = source[index];
    }
}

/* ---- text editing from WM_CHAR (UI thread).  Returns TRUE when the text changed (TextChanged is scheduled by the caller) ---- */
static BOOL x104_input_text_insert(const wchar_t *units, int count)
{
    BOOL changed = FALSE;
    BOOL rejected = FALSE;
    int start, new_length, limit;
    x104_lock(&x104_input_st.text_lock);
    start = x104_input_st.sel_start;
    if (start < 0) start = 0;
    if (start > x104_input_st.text_len) start = x104_input_st.text_len;
    if (start + x104_input_st.sel_len > x104_input_st.text_len) x104_input_st.sel_len = x104_input_st.text_len - start;
    new_length = x104_input_st.text_len - x104_input_st.sel_len + count;
    limit = (x104_input_st.max_length > 0 && x104_input_st.max_length < X104_INPUT_TEXT_CAP - 1) ? x104_input_st.max_length : X104_INPUT_TEXT_CAP - 1;
    if (new_length > limit) {
        rejected = TRUE;                           /* never a partial CR LF pair: the whole insertion is refused */
    } else {
        x104_input_move_wchars(x104_input_st.text + start + count, x104_input_st.text + start + x104_input_st.sel_len,
                               x104_input_st.text_len - start - x104_input_st.sel_len + 1);
        memcpy(x104_input_st.text + start, units, count * sizeof(wchar_t));
        x104_input_st.text_len = new_length;
        x104_input_st.sel_start = start + count;
        x104_input_st.sel_len = 0;
        changed = TRUE;
    }
    x104_unlock(&x104_input_st.text_lock);
    if (rejected) {
        static volatile LONG logged;
        X104_LOG_LIMITED(&logged, 10, "X104 input: TextBox: insertion of %d unit(s) refused (length %d, MaxLength %d)", count, new_length - count, x104_input_st.max_length);
    }
    return changed;
}

static BOOL x104_input_text_backspace(void)
{
    BOOL changed = FALSE;
    int start;
    int remove = 0;
    x104_lock(&x104_input_st.text_lock);
    start = x104_input_st.sel_start;
    if (start < 0) start = 0;
    if (start > x104_input_st.text_len) start = x104_input_st.text_len;
    if (x104_input_st.sel_len > 0) {
        if (start + x104_input_st.sel_len > x104_input_st.text_len) x104_input_st.sel_len = x104_input_st.text_len - start;
        remove = x104_input_st.sel_len;
    } else if (start > 0) {
        remove = 1;
        if (start > 1 && x104_input_st.text[start - 1] >= 0xDC00 && x104_input_st.text[start - 1] <= 0xDFFF &&
            x104_input_st.text[start - 2] >= 0xD800 && x104_input_st.text[start - 2] <= 0xDBFF) remove = 2;        /* a whole surrogate pair */
        start -= remove;
    }
    if (remove > 0) {
        x104_input_move_wchars(x104_input_st.text + start, x104_input_st.text + start + remove, x104_input_st.text_len - start - remove + 1);
        x104_input_st.text_len -= remove;
        x104_input_st.sel_start = start;
        x104_input_st.sel_len = 0;
        changed = TRUE;
    }
    x104_unlock(&x104_input_st.text_lock);
    return changed;
}

/* WM_CHAR (UTF-16 unit).  Only in text mode (focused == EditBox); outside it the key events already carried letters and digits. */
static void x104_input_on_char(wchar_t unit)
{
    static volatile LONG logged;
    static volatile LONG ignored_logged;
    wchar_t pair[2];
    BOOL changed = FALSE;
    if (x104_input_st.focused != X104_INPUT_FOCUS_EDITBOX) {
        if (unit >= 0x20) X104_LOG_LIMITED(&ignored_logged, 3, "X104 input: WM_CHAR U+%04X ignored (not in text mode, focused=%d)", (unsigned)unit, x104_input_st.focused);
        return;
    }
    if (unit >= 0xD800 && unit <= 0xDBFF) {              /* high surrogate: wait for the low half */
        x104_input_st.high_surrogate = unit;
        return;
    }
    if (unit >= 0xDC00 && unit <= 0xDFFF) {
        if (x104_input_st.high_surrogate) {
            pair[0] = x104_input_st.high_surrogate;
            pair[1] = unit;
            x104_input_st.high_surrogate = 0;
            changed = x104_input_text_insert(pair, 2);
        } else {
            return;                                      /* lone low surrogate */
        }
    } else {
        x104_input_st.high_surrogate = 0;
        if (unit == 0x08) {
            changed = x104_input_text_backspace();
        } else if (unit == 0x0D) {
            if (!x104_input_st.accepts_return) return;
            pair[0] = 0x0D;                              /* Enter is the ADJACENT pair CR LF (a lone CR is plain text for the game) */
            pair[1] = 0x0A;
            changed = x104_input_text_insert(pair, 2);
        } else if (unit >= 0x20 && unit != 0x7F) {
            pair[0] = unit;
            changed = x104_input_text_insert(pair, 1);
        } else {
            return;                                      /* other control characters */
        }
    }
    X104_LOG_LIMITED(&logged, 20, "X104 input: WM_CHAR in text mode: U+%04X -> %s, len=%d caret=%d", (unsigned)unit, changed ? "text changed" : "no change", x104_input_st.text_len,
                     x104_input_st.sel_start);
    if (changed) x104_input_text_schedule("WM_CHAR");
}

/* ======================================================================================================================
 * 7. InputPane (statics, instance, args) and x104_inputpane_raise
 * ====================================================================================================================== */
X104_INPUT_EVENT_PAIR(x104_input_pane_showing, "InputPane.add_Showing", x104_input_st.pane_showing)
X104_INPUT_EVENT_PAIR(x104_input_pane_hiding, "InputPane.add_Hiding", x104_input_st.pane_hiding)

static HRESULT X104_API x104_input_pane_get_occluded(void *self, X104Rect *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.pane_rect;
    return S_OK;
}

X104_VTBL(x104_input_vt_pane, IInputPane) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IInputPane, add_Showing, x104_input_pane_showing_add),
    X104_SLOT(IInputPane, remove_Showing, x104_input_pane_showing_remove),
    X104_SLOT(IInputPane, add_Hiding, x104_input_pane_hiding_add),
    X104_SLOT(IInputPane, remove_Hiding, x104_input_pane_hiding_remove),
    X104_SLOT(IInputPane, get_OccludedRect, x104_input_pane_get_occluded)
};
static const X104IfaceDesc x104_input_pane_ifaces[] = { X104_IFACE(IInputPane, x104_input_vt_pane) };
X104_CLASS_DEF(x104_input_pane_class, "InputPane", L"Windows.UI.ViewManagement.InputPane", x104_input_pane_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_pane, x104_input_pane_class, X104Object)

static HRESULT X104_API x104_input_pargs2_get_ensured(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_input_st.pane_ensured;
    return S_OK;
}

static HRESULT X104_API x104_input_pargs2_put_ensured(void *self, BYTE value)
{
    (void)self;
    x104_input_st.pane_ensured = value ? 1 : 0;
    return S_OK;
}

X104_VTBL(x104_input_vt_paneargs, IInputPaneVisibilityEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IInputPaneVisibilityEventArgs, get_OccludedRect, x104_input_pane_get_occluded),
    X104_SLOT(IInputPaneVisibilityEventArgs, put_EnsuredFocusedElementInView, x104_input_pargs2_put_ensured),
    X104_SLOT(IInputPaneVisibilityEventArgs, get_EnsuredFocusedElementInView, x104_input_pargs2_get_ensured)
};
static const X104IfaceDesc x104_input_paneargs_ifaces[] = { X104_IFACE(IInputPaneVisibilityEventArgs, x104_input_vt_paneargs) };
X104_CLASS_DEF(x104_input_paneargs_class, "InputPaneVisibilityEventArgs", L"Windows.UI.ViewManagement.InputPaneVisibilityEventArgs", x104_input_paneargs_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_input_get_pane_args, x104_input_paneargs_class, X104Object)

/* Windows.UI.ViewManagement.InputPane statics: GetForCurrentView returns the SAME object every time (OnInputPaneShowing fetches it again) */
static HRESULT X104_API x104_input_panestatics_get_for_current_view(void *self, void **pane)
{
    (void)self;
    return x104_return_iface(x104_input_get_pane(), &X104_IID_IInputPane, pane, "InputPane.GetForCurrentView");
}

X104_VTBL(x104_input_vt_panestatics, IInputPaneStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IInputPaneStatics, GetForCurrentView, x104_input_panestatics_get_for_current_view)
};
static const X104IfaceDesc x104_input_panestatics_ifaces[] = { X104_IFACE(IInputPaneStatics, x104_input_vt_panestatics) };
X104_CLASS_DEF(x104_input_panestatics_class, "InputPaneStatics", L"Windows.UI.ViewManagement.InputPane", x104_input_panestatics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_input_get_panestatics, x104_input_panestatics_class, X104Object)

/* UI thread, called from the TabTip controller (arm32_input_pane_raise -> here) at the top of the loop.  The rectangle is published FIRST: the game's Showing
 * handler (0x1DF714) reads InputPane.get_OccludedRect().Height straight after and passes it to AppMain::SetKeyboardHeight. */
static void x104_inputpane_raise(BOOL showing)
{
    float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    X104Object *pane = x104_input_st.initialized ? x104_input_get_pane() : NULL;
    X104Object *args = x104_input_st.initialized ? x104_input_get_pane_args() : NULL;
    unsigned handlers;
    const char *source = "hidden: empty rect";
    if (!pane || !args) {
        x104_logf("X104 input: InputPane %s requested but the input unit is not initialised - ignored", showing ? "Showing" : "Hiding");
        return;
    }
    if (showing) {
        if (x104_osk_rect_client(rect)) {
            source = "keyboard rectangle from the host (TabTip window on Windows 8.1/10, shell rectangle on Windows 11)";
        } else {
            float width, height;
            x104_input_client_size(&width, &height);
            rect[0] = 0.0f;
            rect[1] = height * 0.6f;
            rect[2] = width;
            rect[3] = height * 0.4f;
            source = "no keyboard rectangle available: default 40% of the client height";
        }
    }
    x104_input_st.pane_rect.X = rect[0];
    x104_input_st.pane_rect.Y = rect[1];
    x104_input_st.pane_rect.Width = rect[2];
    x104_input_st.pane_rect.Height = rect[3];
    x104_input_st.pane_ensured = 0;
    handlers = x104_event_invoke_guarded(showing ? &x104_input_st.pane_showing : &x104_input_st.pane_hiding, &pane->views[0], &args->views[0]);
    x104_logf("X104 input: InputPane %s raised, occluded h=%d (rect %d,%d %dx%d; %s), handlers=%u", showing ? "Showing" : "Hiding", (int)rect[3], (int)rect[0], (int)rect[1], (int)rect[2],
              (int)rect[3], source, handlers);
}

/* ======================================================================================================================
 * 8. pointer delivery: Pressed / Moved / Released to the CoreWindow lists AND every independent input source (UI thread)
 * ====================================================================================================================== */
static unsigned x104_input_dispatch_pointer(int kind)
{
    unsigned total = 0;
    LONG count, index;
    void *args = &x104_input_get_pointer_args()->views[0];
    total += x104_event_invoke_guarded(&x104_input_st.cw_ptr[kind], &x104_input_get_core_window()->views[0], args);
    count = __atomic_load_n(&x104_input_st.source_count, __ATOMIC_ACQUIRE);
    for (index = 0; index < count && index < X104_INPUT_MAX_SOURCES; ++index) {
        x104_input_source_t *source = x104_input_st.sources[index];
        if (source) total += x104_event_invoke_guarded(&source->ptr[kind], &source->base.views[0], args);
    }
    return total;
}

static unsigned x104_input_raise_pointer(int kind, const x104_input_pointer_t *pointer, BOOL left, BOOL contact, BOOL canceled)
{
    x104_input_st.cur_id = pointer->id;
    x104_input_st.cur_device = pointer->device;
    x104_input_st.cur_x = (float)pointer->x;
    x104_input_st.cur_y = (float)pointer->y;
    x104_input_st.cur_left = left ? 1 : 0;
    x104_input_st.cur_contact = contact ? 1 : 0;
    x104_input_st.cur_canceled = canceled ? 1 : 0;
    x104_input_st.cur_timestamp = x104_input_timestamp_us();
    X104_OBJ(&x104_input_get_pointer_args()->views[0])->user[1] = NULL;           /* Handled = false */
    return x104_input_dispatch_pointer(kind);
}

/* the pointer handlers exist only after HID::Initialize ran (first ready Tick): KeyDown is registered by Initialize itself, the CoreWindow pointer lists
 * (r2 == NULL path) synchronously as well; the source lists a moment later from the pool work item */
static BOOL x104_input_pointer_gate(void)
{
    return x104_event_count(&x104_input_st.cw_key_down) > 0 || x104_event_count(&x104_input_st.cw_ptr[X104_INPUT_EV_PRESSED]) > 0;
}

static void x104_input_note_dropped(const char *what)
{
    unsigned count = ++x104_input_st.dropped_events;
    if (count == 1 || count == 10 || count == 100 || count == 1000)
        x104_logf("X104 input: dropped input before HID::Initialize (%u events, last: %s): the game has registered no pointer / key handlers yet", count, what);
}

static int x104_input_ptr_find(UINT32 id)
{
    int index;
    for (index = 0; index < X104_INPUT_MAX_POINTERS; ++index) {
        if (x104_input_st.pointers[index].used && x104_input_st.pointers[index].id == id) return index;
    }
    return -1;
}

/* a new contact: allocates a tracked pointer and tells the game (Pressed).  Returns the table index or -1 (dropped / table full). */
static int x104_input_ptr_begin(UINT32 id, int device, int x, int y, const char *via)
{
    static volatile LONG full_logged;
    static volatile LONG down_logged;
    int index = x104_input_ptr_find(id);
    x104_input_pointer_t *pointer;
    unsigned handlers;
    if (index >= 0) {
        x104_input_pointer_t *known = &x104_input_st.pointers[index];
        if (known->x != x || known->y != y) {
            known->x = x;
            known->y = y;
            known->dirty = (x != known->sent_x || y != known->sent_y);
        }
        return index;
    }
    if (!x104_input_pointer_gate()) {
        x104_input_note_dropped(via);
        return -1;
    }
    for (index = 0; index < X104_INPUT_MAX_POINTERS; ++index) {
        if (!x104_input_st.pointers[index].used) break;
    }
    if (index >= X104_INPUT_MAX_POINTERS) {
        X104_LOG_LIMITED(&full_logged, 5, "X104 input: all %d pointer slots are busy, contact id=%u (%s) ignored", X104_INPUT_MAX_POINTERS, (unsigned)id, via);
        return -1;
    }
    pointer = &x104_input_st.pointers[index];
    memset(pointer, 0, sizeof(*pointer));
    pointer->used = TRUE;
    pointer->id = id;
    pointer->device = device;
    pointer->x = pointer->sent_x = x;
    pointer->y = pointer->sent_y = y;
    pointer->down_tick = GetTickCount();
    handlers = x104_input_raise_pointer(X104_INPUT_EV_PRESSED, pointer, TRUE, TRUE, FALSE);
    X104_LOG_LIMITED(&down_logged, arm32_input_verbose ? 100000 : 40, "X104 input: pointer down id=%u type=%d (%s) at (%d,%d) via %s handlers=%u", (unsigned)id, device,
                     x104_input_device_names[device], x, y, via, handlers);
    return index;
}

static void x104_input_ptr_move(int index, int x, int y)
{
    x104_input_pointer_t *pointer = &x104_input_st.pointers[index];
    if (!pointer->used) return;
    pointer->x = x;
    pointer->y = y;
    pointer->dirty = (x != pointer->sent_x || y != pointer->sent_y);
}

/* the contact ended (or was cancelled): Released with the final position, the table entry is freed BEFORE the game runs (R13) */
static void x104_input_ptr_end(int index, int x, int y, const char *why, BOOL canceled)
{
    static volatile LONG up_logged;
    x104_input_pointer_t copy = x104_input_st.pointers[index];
    unsigned handlers;
    if (!copy.used) return;
    memset(&x104_input_st.pointers[index], 0, sizeof(x104_input_st.pointers[index]));
    copy.x = x;
    copy.y = y;
    if (copy.device != X104_INPUT_DEV_MOUSE) {
        x104_input_st.last_touch_tick = GetTickCount();
        x104_osk_note_pointer_up();                      /* "a finger was lifted": tap-to-reopen after the user closed the keyboard */
    } else {
        x104_input_st.mouse_down = FALSE;
    }
    /* IsLeftButtonPressed is FALSE at release: the game restores the Arrow cursor only then (input_text.pointer_released) */
    handlers = x104_input_raise_pointer(X104_INPUT_EV_RELEASED, &copy, FALSE, FALSE, canceled);
    X104_LOG_LIMITED(&up_logged, arm32_input_verbose ? 100000 : 40, "X104 input: pointer up id=%u type=%d at (%d,%d) held %lu ms (%s) handlers=%u", (unsigned)copy.id, copy.device, x, y,
                     (unsigned long)(GetTickCount() - copy.down_tick), why, handlers);
}

/* at most one Moved per pointer and loop pass: the game reads its queue once per frame anyway */
static void x104_input_flush_moves(void)
{
    int index;
    for (index = 0; index < X104_INPUT_MAX_POINTERS; ++index) {
        x104_input_pointer_t *pointer = &x104_input_st.pointers[index];
        if (!pointer->used || !pointer->dirty) continue;
        pointer->dirty = FALSE;
        pointer->sent_x = pointer->x;
        pointer->sent_y = pointer->y;
        x104_input_raise_pointer(X104_INPUT_EV_MOVED, pointer, pointer->device == X104_INPUT_DEV_MOUSE, TRUE, FALSE);
    }
}

/* UI thread: Released for every pressed pointer (focus loss, minimise, capture lost, device change, shutdown) */
static void x104_input_release_all_pointers(const char *why)
{
    x104_input_pointer_t copy[X104_INPUT_MAX_POINTERS];
    int index;
    int released = 0;
    BOOL had_mouse = x104_input_st.mouse_down;
    memcpy(copy, x104_input_st.pointers, sizeof(copy));
    memset(x104_input_st.pointers, 0, sizeof(x104_input_st.pointers));
    x104_input_st.mouse_down = FALSE;
    for (index = 0; index < X104_INPUT_MAX_POINTERS; ++index) {
        if (!copy[index].used) continue;
        x104_input_raise_pointer(X104_INPUT_EV_RELEASED, &copy[index], FALSE, FALSE, TRUE);
        ++released;
    }
    if (had_mouse && x104_input_st.window && GetCapture() == x104_input_st.window) ReleaseCapture();
    if (released) x104_logf("X104 input: released all pointers (%d): %s", released, why ? why : "?");
}

/* ======================================================================================================================
 * 9. the three pointer paths: WM_POINTER (touch), WM_TOUCH, touch-sourced mouse messages (legacy single touch), and the real mouse
 * ====================================================================================================================== */
static BOOL x104_input_touch_begin(HWND window, UINT32 id, int x, int y, const char *via)
{
    x104_input_st.last_touch_tick = GetTickCount();
    x104_note_pointer_source(TRUE, via);
    if (window && GetFocus() != window) SetFocus(window);
    return x104_input_ptr_begin(id, X104_INPUT_DEV_TOUCH, x, y, via) >= 0;
}

/* WM_POINTERUPDATE / DOWN / UP / CAPTURECHANGED.  TRUE = consumed (return 0 WITHOUT DefWindowProc: that stops Windows' mouse emulation of the contact).
 * Mouse and pen pointers are never consumed: DefWindowProc turns them into WM_MOUSE* messages, handled by the mouse path below. */
static BOOL x104_input_on_pointer_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    static volatile LONG nontouch_logged;
    UINT32 id = (UINT32)LOWORD(wparam);
    UINT flags = (UINT)HIWORD(wparam);
    UINT32 type = 0;
    int index, x, y;
    if (arm32_input_policy == ARM32_INPUT_MOUSE || x104_input_st.touch_api != ARM32_TOUCHAPI_POINTER) return FALSE;
    index = x104_input_ptr_find(id);

    if (message == X104_INPUT_WM_POINTERCAPTURECHANGED) {
        /* lParam is the HWND that took the capture, not a position; the capture moving to ourselves is harmless */
        if (index < 0 || (HWND)lparam == window || x104_input_st.pointers[index].device != X104_INPUT_DEV_TOUCH) return FALSE;
        x104_logf("X104 input: pointer capture of id=%u moved to window %p - releasing the contact", (unsigned)id, (void *)lparam);
        x104_input_ptr_end(index, x104_input_st.pointers[index].x, x104_input_st.pointers[index].y, "WM_POINTERCAPTURECHANGED", TRUE);
        return TRUE;
    }

    if (message == X104_INPUT_WM_POINTERDOWN && index < 0) {
        if (!arm32_pointer_is_touch(id, &type)) {
            X104_LOG_LIMITED(&nontouch_logged, 6, "X104 input: WM_POINTERDOWN id=%u is not touch (GetPointerType=%u: 2 touch, 3 pen, 4 mouse, 5 touchpad) - left to DefWindowProc / the mouse path",
                             (unsigned)id, (unsigned)type);
            return FALSE;
        }
        if (!x104_input_st.pointer_touch_seen) {
            x104_input_st.pointer_touch_seen = TRUE;
            x104_logf("X104 input: first WM_POINTER touch (id=%u) - the pointer API works on this device", (unsigned)id);
        }
        x104_input_screen_to_client(window, (short)LOWORD(lparam), (short)HIWORD(lparam), &x, &y);
        x104_input_touch_begin(window, id, x, y, "WM_POINTERDOWN");
        return TRUE;                                          /* consumed even when dropped / all slots are busy */
    }

    if (message == X104_INPUT_WM_POINTERDOWN || message == X104_INPUT_WM_POINTERUPDATE) {
        if (index >= 0) {
            x104_input_screen_to_client(window, (short)LOWORD(lparam), (short)HIWORD(lparam), &x, &y);
            if (flags & 0x8000u) x104_input_ptr_end(index, x, y, "WM_POINTERUPDATE canceled", TRUE);
            else x104_input_ptr_move(index, x, y);
            return TRUE;
        }
        /* an unknown pointer that is in contact: its DOWN was missed (sent before the window was ready, or dropped by a focus release) */
        if (message == X104_INPUT_WM_POINTERUPDATE && (flags & 0x0004u) && !arm32_pointer_nontouch_known(id) && arm32_pointer_is_touch(id, &type)) {
            x104_input_st.pointer_touch_seen = TRUE;
            x104_input_screen_to_client(window, (short)LOWORD(lparam), (short)HIWORD(lparam), &x, &y);
            x104_input_touch_begin(window, id, x, y, "WM_POINTERUPDATE (missed down)");
            return TRUE;
        }
        return FALSE;
    }

    if (message == X104_INPUT_WM_POINTERUP) {
        if (index < 0) return FALSE;
        x104_input_screen_to_client(window, (short)LOWORD(lparam), (short)HIWORD(lparam), &x, &y);
        x104_input_ptr_end(index, x, y, (flags & 0x8000u) ? "WM_POINTERUP canceled" : "WM_POINTERUP", (flags & 0x8000u) ? TRUE : FALSE);
        return TRUE;
    }
    return FALSE;
}

/* WM_TOUCH.  Windows RT 8.1 never sends WM_POINTER to this window (device log): the first WM_TOUCH while no touch WM_POINTER was seen switches to this path. */
static BOOL x104_input_on_wm_touch(HWND window, WPARAM wparam, LPARAM lparam)
{
    Arm32TouchInput inputs[16];
    UINT count = LOWORD(wparam);
    UINT index;
    static volatile LONG first_logged;
    if (arm32_input_policy == ARM32_INPUT_MOUSE) return FALSE;
    if (x104_input_st.touch_api != ARM32_TOUCHAPI_WMTOUCH) {
        if (!(x104_input_st.touch_api == ARM32_TOUCHAPI_POINTER && x104_input_st.wm_touch_registered && !x104_input_st.pointer_touch_seen)) return FALSE;
        x104_input_st.touch_api = ARM32_TOUCHAPI_WMTOUCH;
        x104_logf("X104 input: WM_TOUCH arrived but no WM_POINTER touch message was ever seen - switching to the WM_TOUCH path (multi-touch)");
    }
    if (!arm32_api.get_touch_input_info || !arm32_api.close_touch_input_handle) return FALSE;
    if (count > ARRAYSIZE(inputs)) count = ARRAYSIZE(inputs);
    if (!arm32_api.get_touch_input_info((HANDLE)lparam, count, inputs, (int)sizeof(Arm32TouchInput))) {
        x104_logf("X104 input: GetTouchInputInfo(count=%u) failed, error=%lu", count, (unsigned long)GetLastError());
        return FALSE;
    }
    X104_LOG_LIMITED(&first_logged, 1, "X104 input: first WM_TOUCH (%u contact(s)) - the WM_TOUCH API works on this device", count);
    for (index = 0; index < count; ++index) {
        int x, y, slot;
        if (inputs[index].flags & ARM32_TOUCHEVENTF_PALM) continue;
        x104_input_screen_to_client(window, inputs[index].x / 100, inputs[index].y / 100, &x, &y);
        slot = x104_input_ptr_find(inputs[index].id);
        if (inputs[index].flags & ARM32_TOUCHEVENTF_DOWN) {
            x104_input_touch_begin(window, inputs[index].id, x, y, "WM_TOUCH down");
        } else if (inputs[index].flags & ARM32_TOUCHEVENTF_UP) {
            if (slot >= 0) x104_input_ptr_end(slot, x, y, "WM_TOUCH up", FALSE);
        } else if (inputs[index].flags & ARM32_TOUCHEVENTF_MOVE) {
            if (slot >= 0) x104_input_ptr_move(slot, x, y);
            else x104_input_touch_begin(window, inputs[index].id, x, y, "WM_TOUCH move (missed down)");
        }
    }
    arm32_api.close_touch_input_handle((HANDLE)lparam);
    return TRUE;
}

/* single touch from the emulated mouse messages (legacy mode: no WM_POINTER touch and no WM_TOUCH ever arrived) */
static BOOL x104_input_legacy_touch(HWND window, UINT message, LPARAM lparam)
{
    int x = (short)LOWORD(lparam);
    int y = (short)HIWORD(lparam);
    int index = x104_input_ptr_find(X104_INPUT_LEGACY_TOUCH_ID);
    x104_input_clamp_client(window, &x, &y);
    if (message == X104_INPUT_WM_LBUTTONDOWN) x104_input_touch_begin(window, X104_INPUT_LEGACY_TOUCH_ID, x, y, "legacy WM_LBUTTONDOWN (touch-sourced)");
    else if (message == X104_INPUT_WM_MOUSEMOVE) { if (index >= 0) x104_input_ptr_move(index, x, y); }
    else if (message == X104_INPUT_WM_LBUTTONUP) { if (index >= 0) x104_input_ptr_end(index, x, y, "legacy WM_LBUTTONUP (touch-sourced)", FALSE); }
    return TRUE;                                              /* right / middle buttons and wheel produced by gestures (press-and-hold, pan) are dropped */
}

/* Top of every mouse message.  TRUE = consumed.  A message carrying the touch signature (GetMessageExtraInfo & 0xFFFFFF80 == 0xFF515780) is Windows' emulation of a
 * finger: never a mouse for the game.  Everything else is a real mouse (or pen) and, in auto mode, makes mouse/keyboard the active input source. */
static BOOL x104_input_mouse_filter(HWND window, UINT message, LPARAM lparam)
{
    static volatile LONG logged;
    DWORD extra;
    if (!x104_input_st.touch_api) return FALSE;                  /* --input=mouse: everything is a mouse */
    extra = (DWORD)GetMessageExtraInfo();
    if ((extra & 0xFFFFFF80u) == 0xFF515780u) {
        X104_LOG_LIMITED(&logged, 8, "X104 input: touch-sourced mouse message 0x%04x extra=0x%08lx touchApi=%d pointerTouchSeen=%d -> %s", message, (unsigned long)extra,
                         x104_input_st.touch_api, (int)x104_input_st.pointer_touch_seen,
                         x104_input_st.touch_api == ARM32_TOUCHAPI_LEGACY ? "converted to touch"
                         : (!x104_input_st.pointer_touch_seen && x104_input_st.touch_api == ARM32_TOUCHAPI_POINTER ? "switching to legacy conversion" : "dropped"));
        if (x104_input_st.touch_api == ARM32_TOUCHAPI_POINTER && !x104_input_st.pointer_touch_seen) {
            /* the finger arrives only as emulated mouse messages: WM_POINTER touch handling does not work here */
            x104_input_st.touch_api = ARM32_TOUCHAPI_LEGACY;
            x104_logf("X104 input: WARNING no PT_TOUCH WM_POINTER message was seen but touch-sourced mouse messages arrive - switching to legacy (single touch) automatically");
        }
        if (x104_input_st.touch_api == ARM32_TOUCHAPI_LEGACY) return x104_input_legacy_touch(window, message, lparam);
        return TRUE;                                              /* pointer / wmtouch modes: the contact is already delivered, drop the duplicate */
    }
    if (message == X104_INPUT_WM_MOUSEMOVE) {
        /* only a real displacement counts: Windows re-posts WM_MOUSEMOVE for a stationary cursor when windows appear / move, and the synthetic moves that follow
         * a touch must not flip the source (400 ms guard) */
        POINT screen;
        screen.x = (short)LOWORD(lparam);
        screen.y = (short)HIWORD(lparam);
        if (!ClientToScreen(window, &screen)) {
            screen.x = (short)LOWORD(lparam);
            screen.y = (short)HIWORD(lparam);
        }
        if (x104_input_st.have_mouse_position) {
            int dx = screen.x - x104_input_st.last_mouse_x;
            int dy = screen.y - x104_input_st.last_mouse_y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if ((dx + dy) >= 2 && !x104_input_any_touch_down() && (GetTickCount() - x104_input_st.last_touch_tick) > 400u) x104_note_pointer_source(FALSE, "mouse move");
        }
        x104_input_st.have_mouse_position = TRUE;
        x104_input_st.last_mouse_x = screen.x;
        x104_input_st.last_mouse_y = screen.y;
    } else if ((GetTickCount() - x104_input_st.last_touch_tick) > 400u) {
        x104_note_pointer_source(FALSE, "mouse message");
    }
    return FALSE;
}

static BOOL x104_input_on_mouse(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    static volatile LONG other_logged;
    int x = (short)LOWORD(lparam);
    int y = (short)HIWORD(lparam);
    int index;
    if (x104_input_mouse_filter(window, message, lparam)) return TRUE;
    x104_input_clamp_client(window, &x, &y);
    index = x104_input_ptr_find(X104_INPUT_MOUSE_ID);
    switch (message) {
    case X104_INPUT_WM_MOUSEMOVE:
        if (index >= 0) x104_input_ptr_move(index, x, y);       /* a hover move of an unpressed mouse would insert a map node in the game: never delivered */
        return TRUE;
    case X104_INPUT_WM_LBUTTONDOWN:
        if (window && GetFocus() != window) SetFocus(window);
        if (index >= 0) return TRUE;
        if (x104_input_ptr_begin(X104_INPUT_MOUSE_ID, X104_INPUT_DEV_MOUSE, x, y, "mouse WM_LBUTTONDOWN") >= 0) {
            x104_input_st.mouse_down = TRUE;
            SetCapture(window);
        }
        return TRUE;
    case X104_INPUT_WM_LBUTTONUP:
        if (index >= 0) {
            x104_input_st.mouse_down = FALSE;                    /* before ReleaseCapture: the WM_CAPTURECHANGED it sends must not look like a lost capture */
            x104_input_ptr_end(index, x, y, "mouse WM_LBUTTONUP", FALSE);
        }
        if (GetCapture() == window) ReleaseCapture();
        return TRUE;
    case X104_INPUT_WM_RBUTTONDOWN:
    case X104_INPUT_WM_MBUTTONDOWN:
        X104_LOG_LIMITED(&other_logged, 3, "X104 input: mouse button message 0x%04x ignored (the game is touch-only: left button only)", message);
        return TRUE;
    case X104_INPUT_WM_RBUTTONUP:
    case X104_INPUT_WM_MBUTTONUP:
        return TRUE;
    default:
        break;
    }
    (void)wparam;
    return FALSE;
}

/* ======================================================================================================================
 * 10. keys and characters
 * ====================================================================================================================== */
/* the VKs the game maps (HID::mapVirtualKey 0x1127CC) plus letters and digits (they become TEXT events); every other VK would become a garbage TEXT event */
static BOOL x104_input_key_wanted(UINT vk)
{
    if (vk == 0x08 || vk == 0x0D || vk == 0x20 || vk == 0x21 || vk == 0x22 || vk == 0x10) return TRUE;
    if (vk >= 0x25 && vk <= 0x28) return TRUE;
    if (vk >= 0x70 && vk <= 0x7B) return TRUE;
    if (vk >= 0x30 && vk <= 0x39) return TRUE;
    if (vk >= 0x41 && vk <= 0x5A) return TRUE;
    return FALSE;
}

static unsigned x104_input_raise_key(BOOL down, UINT vk, UINT repeat, UINT scan, BOOL extended, BOOL menu, BOOL was_down)
{
    X104Object *args = x104_input_get_key_args();
    x104_input_st.cur_vk = (int)vk;
    x104_input_st.cur_key.repeat_count = repeat ? repeat : 1;
    x104_input_st.cur_key.scan_code = scan;
    x104_input_st.cur_key.is_extended = extended ? 1 : 0;
    x104_input_st.cur_key.is_menu_down = menu ? 1 : 0;
    x104_input_st.cur_key.was_key_down = was_down ? 1 : 0;
    x104_input_st.cur_key.is_key_released = down ? 0 : 1;
    args->user[1] = NULL;
    return x104_event_invoke_guarded(down ? &x104_input_st.cw_key_down : &x104_input_st.cw_key_up, &x104_input_get_core_window()->views[0], &args->views[0]);
}

static void x104_input_back(const char *why)
{
    static volatile LONG logged;
    BOOL handled = x104_app_raise_back_pressed();
    X104_LOG_LIMITED(&logged, 60, "X104 input: BackPressed handled=%d (%s)", (int)handled, why);
}

/* WM_KEYDOWN / WM_KEYUP / WM_SYSKEYDOWN / WM_SYSKEYUP.  TRUE = consumed; FALSE only for Alt+F4 (DefWindowProc turns it into WM_CLOSE). */
static BOOL x104_input_on_key(UINT message, WPARAM wparam, LPARAM lparam)
{
    static volatile LONG logged;
    static volatile LONG dropped_logged;
    BOOL sys = (message == X104_INPUT_WM_SYSKEYDOWN || message == X104_INPUT_WM_SYSKEYUP);
    BOOL down = (message == X104_INPUT_WM_KEYDOWN || message == X104_INPUT_WM_SYSKEYDOWN);
    UINT vk = (UINT)wparam;
    BOOL alt = ((DWORD)lparam & (1u << 29)) ? TRUE : FALSE;
    BOOL was_down = ((DWORD)lparam & (1u << 30)) ? TRUE : FALSE;
    UINT repeat = (UINT)((DWORD)lparam & 0xFFFFu);
    UINT scan = (UINT)(((DWORD)lparam >> 16) & 0xFFu);
    BOOL extended = ((DWORD)lparam & (1u << 24)) ? TRUE : FALSE;

    if (sys && vk == VK_F4 && alt) return FALSE;                 /* the ONE exception: Alt+F4 must reach DefWindowProc */
    if (down && !was_down && vk != 0xE7 /* VK_PACKET: injected by the touch keyboard */) {
        /* a hardware key means a keyboard is in use, unless a finger is down or the key was injected by our own touch keyboard */
        if (!x104_input_any_pointer_down() && !(text_input_active && arm32_osk_visible_now())) {
            char why[32];
            wsprintfA(why, "key VK=0x%02x", vk);
            x104_note_pointer_source(FALSE, why);
        }
    }
    /* Esc, Alt+Left and the browser Back key are Back (HardwareButtons.BackPressed), not keys */
    if (vk == 0x1B || vk == 0xA6 || (sys && alt && vk == 0x25)) {
        if (down && !was_down) x104_input_back(vk == 0x1B ? "Esc" : (vk == 0xA6 ? "Browser Back" : "Alt+Left"));
        return TRUE;
    }
    if (down) {
        if (!x104_input_key_wanted(vk)) {
            X104_LOG_LIMITED(&dropped_logged, 6, "X104 input: key VK=0x%02x dropped (the game would turn it into a garbage text event)", vk);
            return TRUE;
        }
        {
            unsigned handlers;
            if (!x104_input_pointer_gate()) {
                x104_input_note_dropped("key down");
                return TRUE;
            }
            x104_input_st.key_down[vk & 0xFF] = 1;
            handlers = x104_input_raise_key(TRUE, vk, repeat, scan, extended, alt, was_down);
            X104_LOG_LIMITED(&logged, arm32_input_verbose ? 100000 : 40, "X104 input: key down vk=%02x repeat=%u scan=%02x handlers=%u", vk, repeat, scan, handlers);
        }
        return TRUE;
    }
    if (vk < 256 && x104_input_st.key_down[vk]) {
        x104_input_st.key_down[vk] = 0;
        x104_input_raise_key(FALSE, vk, 1, scan, extended, alt, TRUE);
    }
    return TRUE;
}

/* KeyUp for every key the game saw go down (focus loss, shutdown): a stuck Shift would capitalise everything */
static void x104_input_release_all_keys(const char *why)
{
    unsigned vk;
    unsigned released = 0;
    for (vk = 0; vk < 256; ++vk) {
        if (!x104_input_st.key_down[vk]) continue;
        x104_input_st.key_down[vk] = 0;
        x104_input_raise_key(FALSE, vk, 1, 0, FALSE, FALSE, TRUE);
        ++released;
    }
    x104_input_st.high_surrogate = 0;
    if (released) x104_logf("X104 input: released %u pressed key(s): %s", released, why ? why : "?");
}

/* ======================================================================================================================
 * 11. contract functions
 * ====================================================================================================================== */
/* nothing to patch in the game image: the touch-only cell 0x33FB88 stays as it is (the game is touch-only on purpose) */
static BOOL x104_input_install(BYTE *image)
{
    (void)image;
    x104_logf("X104 input: install ok (no patches: the touch-only cell 0x%08lx is left alone)", (unsigned long)X104_RVA_CELL_TOUCHONLY);
    return TRUE;
}

/* ======================================================================================================================
 * 9b. on-screen Back / Pause button (host-drawn).  The Windows Phone build has NO pause button: "pause" in a world and "back" in menus are the
 *     phone's hardware Back key (MinecraftClient::tick opens the PauseScreen on game key 0xFF, rva 0x59EE6..0x59F16; the PauseScreen is built only
 *     there and from the controller options).  The host maps Esc / Alt+Left / Browser Back to BackPressed, but a touch-only user had no way to pause.
 *     A small owned, layered popup over the top-left corner of the game window raises the same BackPressed when tapped (in a world: pause menu; in
 *     menus: back; on the start menu the game leaves it unhandled and nothing happens).  user32 only (this DLL does not link gdi32).
 *     Shown only in a world with no screen open (client level + player set, screen NULL); --back-button-always shows it in menus too,
 *     --no-back-button turns it off.  UI thread only (created in x104_input_init, placed every pass in x104_input_ui_tick).
 * ====================================================================================================================== */
extern BOOL WINAPI SetLayeredWindowAttributes(HWND, DWORD, BYTE, DWORD);
extern HBRUSH WINAPI GetSysColorBrush(int);
extern int WINAPI FillRect(HDC, const RECT *, HBRUSH);
extern BOOL WINAPI InvalidateRect(HWND, const RECT *, BOOL);

#define X104_BB_WS_POPUP           0x80000000u
#define X104_BB_WS_EX_LAYERED      0x00080000u
#define X104_BB_WS_EX_TOOLWINDOW   0x00000080u
#define X104_BB_WS_EX_NOACTIVATE   0x08000000u
#define X104_BB_LWA_ALPHA          0x00000002u
#define X104_BB_WM_PAINT           0x000Fu
#define X104_BB_WM_ERASEBKGND      0x0014u
#define X104_BB_WM_MOUSEACTIVATE   0x0021u
#define X104_BB_WM_LBUTTONDOWN     0x0201u
#define X104_BB_WM_LBUTTONUP       0x0202u
#define X104_BB_WM_CAPTURECHANGED  0x0215u
#define X104_BB_MA_NOACTIVATE      3
#define X104_BB_SWP_NOSIZE         0x0001u
#define X104_BB_SWP_NOMOVE         0x0002u
#define X104_BB_SWP_NOZORDER       0x0004u
#define X104_BB_SWP_NOACTIVATE     0x0010u
#define X104_BB_SWP_SHOWWINDOW     0x0040u
#define X104_BB_SWP_HIDEWINDOW     0x0080u
#define X104_BB_COLOR_WINDOW       5
#define X104_BB_COLOR_WINDOWTEXT   8
#define X104_BB_COLOR_HIGHLIGHT    13
#define X104_BB_ALPHA              150

static struct {
    HWND hwnd;
    RECT placed;          /* screen rectangle last applied */
    BOOL shown;
    BOOL pressed;
    BOOL always;          /* --back-button-always: also in menus (default: only in a world with no screen open) */
    unsigned taps;
} x104_backbtn;

/* MinecraftClient fields the HUD itself depends on (GameRenderer::render -> Gui::render, see the app unit's game-state notes) */
#define X104_BB_CLIENT_LEVEL   0x68u
#define X104_BB_CLIENT_SCREEN  0x6Cu       /* Screen*: start menu, pause, chat, inventory, death and loading screens are all Screens */
#define X104_BB_CLIENT_PLAYER  0x12Cu

/* in a world and no screen open (user request 2026-10-21: the button only while playing, not in the menus) */
static BOOL x104_backbtn_in_world(void)
{
    BYTE *client = x104_game_client();
    DWORD level = 0, player = 0, screen = 1;
    if (!client) return FALSE;
    if (!x104_read_u32(client + X104_BB_CLIENT_LEVEL, &level) || !x104_read_u32(client + X104_BB_CLIENT_PLAYER, &player) ||
        !x104_read_u32(client + X104_BB_CLIENT_SCREEN, &screen)) return FALSE;
    return level != 0 && player != 0 && screen == 0;
}

static LRESULT CALLBACK x104_backbtn_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == X104_BB_WM_MOUSEACTIVATE) return X104_BB_MA_NOACTIVATE;      /* never take the focus from the game window */
    if (message == X104_BB_WM_ERASEBKGND) return 1;
    if (message == X104_BB_WM_LBUTTONDOWN) {                                       /* a finger arrives as the promoted left button */
        x104_backbtn.pressed = TRUE;
        SetCapture(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    if (message == X104_BB_WM_LBUTTONUP) {
        if (x104_backbtn.pressed) {
            RECT client;
            int x = (short)LOWORD(lparam);
            int y = (short)HIWORD(lparam);
            x104_backbtn.pressed = FALSE;
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
            if (GetClientRect(hwnd, &client) && x >= 0 && y >= 0 && x < client.right && y < client.bottom) {
                ++x104_backbtn.taps;
                x104_input_back("on-screen Back/Pause button");
            }
        }
        return 0;
    }
    if (message == X104_BB_WM_CAPTURECHANGED) {
        if (x104_backbtn.pressed) {
            x104_backbtn.pressed = FALSE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    if (message == X104_BB_WM_PAINT) {
        PAINTSTRUCT paint;
        RECT area;
        HDC dc = BeginPaint(hwnd, &paint);
        if (dc && GetClientRect(hwnd, &area) && area.right > 8 && area.bottom > 8) {
            LONG w = area.right, h = area.bottom;
            LONG bar_width = w / 6, gap = w / 7;
            RECT bar;
            /* a dark square with the two white bars of a pause sign; brighter while pressed */
            FillRect(dc, &area, GetSysColorBrush(x104_backbtn.pressed ? X104_BB_COLOR_HIGHLIGHT : X104_BB_COLOR_WINDOWTEXT));
            bar.top = h / 4;
            bar.bottom = h - h / 4;
            bar.left = w / 2 - gap / 2 - bar_width;
            bar.right = bar.left + bar_width;
            FillRect(dc, &bar, GetSysColorBrush(X104_BB_COLOR_WINDOW));
            bar.left = w / 2 + gap / 2 + (gap & 1);
            bar.right = bar.left + bar_width;
            FillRect(dc, &bar, GetSysColorBrush(X104_BB_COLOR_WINDOW));
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static BOOL x104_backbtn_command_line_has(const wchar_t *option)
{
    const wchar_t *text = GetCommandLineW();
    unsigned length = 0;
    while (option[length]) ++length;
    if (!text) return FALSE;
    for (; *text; ++text) {
        unsigned index = 0;
        while (index < length && text[index] == option[index]) ++index;
        if (index == length) return TRUE;
    }
    return FALSE;
}

static void x104_backbtn_create(HWND owner)
{
    static const wchar_t class_name[] = L"Win32CraftBackButton";
    WNDCLASSEXW klass;
    HINSTANCE instance = (HINSTANCE)GetModuleHandleW(NULL);

    if (x104_backbtn.hwnd || !owner) return;
    x104_backbtn.always = x104_backbtn_command_line_has(L"--back-button-always");
    if (x104_backbtn_command_line_has(L"--no-back-button")) {
        x104_logf("X104 input: on-screen Back/Pause button off (--no-back-button); Back = Esc / Alt+Left / Browser Back");
        return;
    }
    memset(&klass, 0, sizeof(klass));
    klass.cbSize = sizeof(klass);
    klass.lpfnWndProc = x104_backbtn_proc;
    klass.hInstance = instance;
    klass.lpszClassName = class_name;
    RegisterClassExW(&klass);          /* ERROR_CLASS_ALREADY_EXISTS on a second init is fine: CreateWindowExW below still works */
    x104_backbtn.hwnd = CreateWindowExW(X104_BB_WS_EX_LAYERED | X104_BB_WS_EX_TOOLWINDOW | X104_BB_WS_EX_NOACTIVATE, class_name, L"Back", X104_BB_WS_POPUP,
                                        0, 0, 1, 1, owner, NULL, instance, NULL);
    if (!x104_backbtn.hwnd) {
        x104_logf("X104 input: WARNING the on-screen Back/Pause button could not be created (CreateWindowExW error %lu) - Back = Esc / Alt+Left only",
                  (unsigned long)GetLastError());
        return;
    }
    SetLayeredWindowAttributes(x104_backbtn.hwnd, 0, X104_BB_ALPHA, X104_BB_LWA_ALPHA);
    x104_logf("X104 input: on-screen Back/Pause button created (window %p, top-left of the game window, shown %s; a tap = the phone's Back key)",
              (void *)x104_backbtn.hwnd, x104_backbtn.always ? "always (--back-button-always)" : "only in a world with no menu open");
}

/* every UI pass: follow the game window (move / resize / maximise / minimise); SetWindowPos only when something changed */
static void x104_backbtn_place(HWND owner)
{
    RECT client;
    RECT want;
    POINT origin;
    LONG size;
    BOOL show;

    if (!x104_backbtn.hwnd) return;
    show = owner && IsWindowVisible(owner) && !IsIconic(owner) && GetClientRect(owner, &client) && client.right > 64 && client.bottom > 64 &&
           (x104_backbtn.always || x104_backbtn_in_world());
    if (!show) {
        if (x104_backbtn.shown) {
            SetWindowPos(x104_backbtn.hwnd, NULL, 0, 0, 0, 0, X104_BB_SWP_NOMOVE | X104_BB_SWP_NOSIZE | X104_BB_SWP_NOZORDER | X104_BB_SWP_NOACTIVATE | X104_BB_SWP_HIDEWINDOW);
            x104_backbtn.shown = FALSE;
        }
        return;
    }
    origin.x = 0;
    origin.y = 0;
    ClientToScreen(owner, &origin);
    size = client.bottom / 11;                          /* about a finger tip on a tablet */
    if (size < 40) size = 40;
    if (size > 120) size = 120;
    if (size > client.right / 6) size = client.right / 6;
    want.left = origin.x + size / 3;                    /* top-LEFT corner (user request 2026-10-21) */
    want.top = origin.y + size / 3;
    want.right = want.left + size;
    want.bottom = want.top + size;
    if (x104_backbtn.shown && memcmp(&want, &x104_backbtn.placed, sizeof(want)) == 0) return;
    SetWindowPos(x104_backbtn.hwnd, NULL, want.left, want.top, size, size, X104_BB_SWP_NOZORDER | X104_BB_SWP_NOACTIVATE | X104_BB_SWP_SHOWWINDOW);
    if (!x104_backbtn.shown) {
        static volatile LONG logged;
        X104_LOG_LIMITED(&logged, 40, "X104 input: on-screen Back/Pause button shown at (%ld,%ld) size %ld", want.left, want.top, size);
    }
    x104_backbtn.placed = want;
    x104_backbtn.shown = TRUE;
}

static BOOL x104_input_init(HWND window)
{
    unsigned index;
    X104Element *element;
    if (x104_input_st.initialized) return TRUE;
    x104_input_st.window = window;
    x104_event_init(&x104_input_st.cw_key_down, "CoreWindow.KeyDown");
    x104_event_init(&x104_input_st.cw_key_up, "CoreWindow.KeyUp");
    for (index = 0; index < 3; ++index) x104_event_init(&x104_input_st.cw_ptr[index], x104_input_ptr_event_names[index]);
    x104_event_init(&x104_input_st.cw_visibility, "CoreWindow.VisibilityChanged");
    x104_event_init(&x104_input_st.text_changed, "TextBox.TextChanged");
    x104_event_init(&x104_input_st.pane_showing, "InputPane.Showing");
    x104_event_init(&x104_input_st.pane_hiding, "InputPane.Hiding");
    x104_input_st.cursor_visible = TRUE;
    x104_input_st.window_visible = 1;
    x104_input_st.input_enabled = 1;
    x104_input_st.max_length = 0xC350;                    /* TextBox default of the game (50000) until put_MaxLength */
    x104_input_st.accepts_return = 0;
    x104_input_st.text[0] = 0;

    /* Windows registration: RegisterTouchWindow for the WM_TOUCH fallback, touch feedback off, pen service property.  Logs `ARM32 input: touch handling active ...` */
    arm32_input_attach_window(window);
    x104_input_st.touch_api = arm32_touch_api_effective;
    x104_input_st.wm_touch_registered = arm32_wm_touch_registered;
    x104_backbtn_create(window);

    for (index = 0; index < 3; ++index) {
        x104_input_st.devices[index] = x104_object_new(&x104_input_device_class, sizeof(X104Object));
        if (!x104_input_st.devices[index]) {
            x104_logf("X104 input: ERROR init: out of memory creating the PointerDevice objects");
            return FALSE;
        }
        x104_input_st.devices[index]->user[0] = (void *)(ULONG_PTR)index;
    }
    element = x104_element_new(&x104_input_logo_class, sizeof(X104Element), "Logo");
    x104_input_st.logo = element ? &element->base : NULL;
    element = x104_element_new(&x104_input_edit_class, sizeof(X104Element), "EditBox");
    x104_input_st.editbox = element ? &element->base : NULL;
    element = x104_element_new(&x104_input_button_class, sizeof(X104Element), "Button");
    x104_input_st.button = element ? &element->base : NULL;
    if (!x104_input_st.logo || !x104_input_st.editbox || !x104_input_st.button) {
        x104_logf("X104 input: ERROR init: out of memory creating the XAML elements (logo=%p editbox=%p button=%p)", (void *)x104_input_st.logo, (void *)x104_input_st.editbox,
                  (void *)x104_input_st.button);
        return FALSE;
    }
    /* force the lazily created singletons now so a failure shows up at init, not in the middle of a pointer event */
    if (!x104_input_get_core_window() || !x104_input_get_cwstatic() || !x104_input_get_cursorfactory() || !x104_input_get_panestatics() || !x104_input_get_pane() ||
        !x104_input_get_pane_args() || !x104_input_get_pointer_args() || !x104_input_get_point() || !x104_input_get_props() || !x104_input_get_key_args() ||
        !x104_input_get_vis_args() || !x104_input_get_text_args()) {
        x104_logf("X104 input: ERROR init: out of memory creating the input objects");
        return FALSE;
    }
    __atomic_store_n(&x104_input_st.initialized, 1, __ATOMIC_RELEASE);
    x104_logf("X104 input: init ok (touch api %s, input policy %s, osk policy %s, window %p, CoreWindow obj=%p, EditBox obj=%p, Button obj=%p, Logo obj=%p)",
              arm32_touch_api_names[x104_input_st.touch_api & 3], arm32_input_policy_names[arm32_input_policy % 3], arm32_osk_policy_names[arm32_osk_policy % 3], (void *)window,
              (void *)x104_input_get_core_window(), (void *)x104_input_st.editbox, (void *)x104_input_st.button, (void *)x104_input_st.logo);
    return TRUE;
}

/* idempotent */
static void x104_input_shutdown(void)
{
    if (x104_input_st.initialized) {
        if (x104_input_any_pointer_down()) x104_input_release_all_pointers("shutdown");
        x104_input_release_all_keys("shutdown");
        x104_osk_hide("shutdown");
        if (x104_backbtn.hwnd) {
            DestroyWindow(x104_backbtn.hwnd);
            x104_backbtn.hwnd = NULL;
            x104_backbtn.shown = FALSE;
        }
        x104_logf("X104 input: shutdown (dropped input events before HID::Initialize: %u, on-screen Back/Pause taps: %u)", x104_input_st.dropped_events, x104_backbtn.taps);
        __atomic_store_n(&x104_input_st.initialized, 0, __ATOMIC_RELEASE);
    }
    x104_set_text_input_active(FALSE, "shutdown");
}

static const X104FactoryDef x104_input_factories[] = {
    { L"Windows.UI.ViewManagement.InputPane", x104_input_get_panestatics },
    { L"Windows.UI.Core.CoreCursor", x104_input_get_cursorfactory },
    { L"Windows.UI.Core.CoreWindow", x104_input_get_cwstatic }
};

static HRESULT x104_input_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!class_name) return REGDB_E_CLASSNOTREG;
    return x104_activate_from_table(x104_input_factories, ARRAYSIZE(x104_input_factories), class_name, iid, out);
}

/* every loop pass (<= 10 ms, also from the WM_TIMER pump inside modal loops): deferred release, coalesced pointer moves, the TabTip controller (which calls
 * arm32_input_pane_raise -> x104_inputpane_raise on state changes) */
static void x104_input_ui_tick(void)
{
    if (!x104_input_st.initialized) return;
    if (InterlockedExchange(&x104_input_st.pending_release, 0)) {
        const char *why = x104_input_st.pending_why;
        InterlockedExchange(&x104_input_st.pending_release_mouse, 0);
        x104_input_release_all_pointers(why ? why : "focus lost");
        x104_input_release_all_keys(why ? why : "focus lost");
    } else if (InterlockedExchange(&x104_input_st.pending_release_mouse, 0)) {
        if (x104_input_st.mouse_down) {
            int index = x104_input_ptr_find(X104_INPUT_MOUSE_ID);
            if (index >= 0) x104_input_ptr_end(index, x104_input_st.pointers[index].x, x104_input_st.pointers[index].y, "mouse capture lost", TRUE);
            x104_input_st.mouse_down = FALSE;
            x104_logf("X104 input: mouse capture lost while the left button was down - released");
        }
    }
    x104_input_flush_moves();
    x104_osk_poll();
    x104_backbtn_place(x104_input_st.window);
}

static void x104_input_request_release(const char *why)
{
    x104_input_st.pending_why = why;
    InterlockedExchange(&x104_input_st.pending_release, 1);
}

/* Win32 messages.  TRUE + *result when consumed.  Broadcast messages (focus, activation, size) return FALSE and only set flags (R1). */
static BOOL x104_input_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result)
{
    BOOL consumed = FALSE;
    LRESULT value = 0;
    if (!x104_input_st.initialized) return FALSE;                  /* safe before init (R15) */
    switch (message) {
    case X104_INPUT_WM_POINTERUPDATE:
    case X104_INPUT_WM_POINTERDOWN:
    case X104_INPUT_WM_POINTERUP:
    case X104_INPUT_WM_POINTERCAPTURECHANGED:
        consumed = x104_input_on_pointer_message(window, message, wparam, lparam);
        break;
    case X104_INPUT_WM_TOUCH:
        consumed = x104_input_on_wm_touch(window, wparam, lparam);
        break;
    case X104_INPUT_WM_MOUSEMOVE:
    case X104_INPUT_WM_LBUTTONDOWN:
    case X104_INPUT_WM_LBUTTONUP:
    case X104_INPUT_WM_RBUTTONDOWN:
    case X104_INPUT_WM_RBUTTONUP:
    case X104_INPUT_WM_MBUTTONDOWN:
    case X104_INPUT_WM_MBUTTONUP:
        consumed = x104_input_on_mouse(window, message, wparam, lparam);
        break;
    case X104_INPUT_WM_MOUSEWHEEL:
        consumed = x104_input_mouse_filter(window, message, lparam);       /* a wheel produced by a touch gesture is dropped; a real wheel is not used by the game */
        break;
    case X104_INPUT_WM_XBUTTONDOWN:
        if (HIWORD(wparam) == 1) x104_input_back("mouse XBUTTON1 (Back)");
        consumed = TRUE;
        value = TRUE;
        break;
    case X104_INPUT_WM_XBUTTONUP:
        consumed = TRUE;
        value = TRUE;
        break;
    case X104_INPUT_WM_KEYDOWN:
    case X104_INPUT_WM_KEYUP:
    case X104_INPUT_WM_SYSKEYDOWN:
    case X104_INPUT_WM_SYSKEYUP:
        consumed = x104_input_on_key(message, wparam, lparam);
        break;
    case X104_INPUT_WM_CHAR:
        x104_input_on_char((wchar_t)wparam);
        consumed = TRUE;
        break;
    case X104_INPUT_WM_SETCURSOR:
        if (LOWORD(lparam) == 1 /* HTCLIENT */) {
            SetCursor(x104_input_current_hcursor());
            consumed = TRUE;
            value = TRUE;
        }
        break;
    case X104_INPUT_WM_ACTIVATE:
        if (LOWORD(wparam) == 0 /* WA_INACTIVE */) x104_input_request_release("window deactivated");
        break;
    case X104_INPUT_WM_ACTIVATEAPP:
        if (!wparam) x104_input_request_release("application deactivated");
        break;
    case X104_INPUT_WM_KILLFOCUS:
        x104_input_request_release("focus lost");
        break;
    case X104_INPUT_WM_CANCELMODE:
        x104_input_request_release("WM_CANCELMODE");
        break;
    case X104_INPUT_WM_CAPTURECHANGED:
        if ((HWND)lparam != window && x104_input_st.mouse_down) InterlockedExchange(&x104_input_st.pending_release_mouse, 1);
        break;
    case X104_INPUT_WM_SIZE:
        if (wparam == 1 /* SIZE_MINIMIZED */) x104_input_request_release("window minimised");
        break;
    default:
        break;
    }
    if (consumed && result) *result = value;
    return consumed;
}

static X104Object *x104_input_core_window(void) { return x104_input_st.initialized ? x104_input_get_core_window() : NULL; }
static X104Object *x104_input_element_logo(void) { return x104_input_st.logo; }
static X104Object *x104_input_element_editbox(void) { return x104_input_st.editbox; }
static X104Object *x104_input_element_button(void) { return x104_input_st.button; }

/* UI thread: CoreWindow.VisibilityChanged (VisibilityChangedEventArgs.get_Visible is read straight on the pointer at 0x1DF9CC) */
static void x104_input_raise_visibility_changed(BOOL visible)
{
    unsigned handlers;
    X104Object *args;
    if (!x104_input_st.initialized) {
        x104_logf("X104 input: VisibilityChanged(%d) requested but the input unit is not initialised - ignored", (int)visible);
        return;
    }
    args = x104_input_get_vis_args();
    x104_input_st.window_visible = visible ? 1 : 0;
    x104_input_st.cur_visible = visible ? 1 : 0;
    args->user[1] = NULL;
    handlers = x104_event_invoke_guarded(&x104_input_st.cw_visibility, &x104_input_get_core_window()->views[0], &args->views[0]);
    x104_logf("X104 input: CoreWindow.VisibilityChanged(%d) raised, handlers=%u", (int)visible, handlers);
}

#endif /* WIN32_HOST_ARM32_0104_INPUT_INL */
