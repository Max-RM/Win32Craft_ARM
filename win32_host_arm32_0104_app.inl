/* win32_host_arm32_0104_app.inl - UNIT "app" of the Minecraft PE 0.10.4.40 ARM32 host.
 *
 * Owner: the app implementer.  Brief: arm32/v0_10_4/units/app.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit app
 *
 * This file owns: Application (statics + factory/aggregation + the inner IApplication, and the Start that drives the whole game), the launch event args,
 * Page (factory + the inner base object with FindName), Uri, Window (statics + IWindow), StatusBar, Canvas, HardwareButtons (+ BackPressedEventArgs),
 * DisplayInformation, the window lifecycle (close, minimise / restore, display change) and the application exit.
 *
 * Contents:  1 private state and helpers      2 launch args, suspending args, back args      3 Uri
 *            4 Application (inner) and the exit path      5 Window      6 Page
 *            7 StatusBar / Canvas / HardwareButtons / DisplayInformation      8 Application statics + factory (Start)
 *            9 activation table and the contract functions
 *
 * Threads: everything the game calls runs on the UI thread (OnLaunched, the MainPage constructor) except the statics' getters, which are agile and lock free.
 * RULE R1: no fake in this file ever invokes a game delegate; delegates are raised only by x104_app_ui_tick (visibility, display change), by the exit proc (Suspending,
 * posted through x104_ui_post), by x104_app_raise_back_pressed / x104_display_raise_* (called by the input unit / the tick at the loop's top level) and by Start itself
 * (the initialisation callback and OnLaunched, both from the fake Application::Start which the UI thread runs). */
#ifndef WIN32_HOST_ARM32_0104_APP_INL
#define WIN32_HOST_ARM32_0104_APP_INL

/* user32 / kernel32 prototypes identical to the SDK (identical redeclarations are legal C) */
extern BOOL WINAPI IsWindowVisible(HWND);
extern int WINAPI GetSystemMetrics(int);
typedef struct X104AppFileTime { DWORD low, high; } X104AppFileTime;      /* FILETIME (win32_compat.h does not define it) */
extern void WINAPI GetSystemTimeAsFileTime(X104AppFileTime *);

#define X104_APP_SW_HIDE            0
#define X104_APP_SW_SHOW            5
#define X104_APP_WM_SHOWWINDOW      0x0018u
#define X104_APP_WM_ENDSESSION      0x0016u
#define X104_APP_WM_DISPLAYCHANGE   0x007Eu
#define X104_APP_WM_DPICHANGED      0x02E0u
#define X104_APP_SIZE_MINIMIZED     1u
#define X104_APP_SIZE_RESTORED      0u
#define X104_APP_SIZE_MAXIMIZED     2u
#define X104_APP_MB_ICONERROR       0x00000010u
#define X104_APP_ORIENTATION_LANDSCAPE 1          /* Windows.Graphics.Display.DisplayOrientations.Landscape */
#define X104_APP_SCALE_100          100           /* ResolutionScale.Scale100Percent */
#define X104_APP_SM_CXSCREEN        0
#define X104_APP_SM_CYSCREEN        1
#define X104_APP_ACTIVATE_GRACE_MS  3000u         /* Window.Activate must follow OnLaunched within this time, else the tick shows the window itself */
#define X104_APP_APPMAIN_RENDER_ACTION 0x0Cu      /* AppMain+0x0C: the IAsyncAction StartRenderLoop (rva 0x1DA8A8) stores; StopRenderLoop (0x1DA9D4) dereferences it WITHOUT a NULL check */

/* ======================================================================================================================
 * 1. private state and helpers
 * ====================================================================================================================== */
typedef struct X104AppApplication {            /* the object IApplicationFactory.CreateInstance returns (inner + IApplication instance) */
    X104Object base;
    X104Event suspending;                       /* SuspendingEventHandler  -> App2 lambda 0x1E3DA0 */
    X104Event resuming;                         /* EventHandler<Object>    -> App2 lambda 0x1E3C34 */
    X104Event unhandled;                        /* UnhandledExceptionEventHandler: accepted, never raised */
} X104AppApplication;

typedef struct X104AppWindow {
    X104Object base;
    X104Event activated, closed, size_changed, visibility_changed;     /* accepted and stored, never raised (the game does not use them) */
    void *content;                              /* IUIElement of the page (AddRef'd) */
    X104Lock content_lock;
} X104AppWindow;

typedef struct X104AppDisplay {
    X104Object base;
    X104Event orientation_changed;              /* handler 0x1DF76C reads sender->get_CurrentOrientation (slot 6) straight from the sender (R10) */
    X104Event dpi_changed;                      /* handler 0x1DF67C reads sender->get_LogicalDpi (slot 11) straight from the sender (R10) */
} X104AppDisplay;

typedef struct X104AppBackArgs {
    X104Object base;
    volatile LONG handled;
} X104AppBackArgs;

typedef struct X104AppUri {
    X104Object base;
    wchar_t *text;                              /* heap copy, NUL terminated, never freed */
    UINT32 length;
} X104AppUri;

typedef struct X104AppState {
    HWND window;
    volatile LONG initialized;
    volatile LONG shut_down;
    volatile LONG started;                      /* Application::Start entered */
    volatile LONG exit_proc_ran;
    volatile LONG activate_called;              /* Window.Activate ran */
    volatile LONG safety_fired;                 /* the 3 s safety net showed the window */
    volatile LONG pending_hidden;               /* window state as the window messages report it: 1 = minimised / hidden (zero state = visible) */
    volatile LONG delivered_hidden;             /* what the game was last told (zero state = visible: the game starts with MainPage+0x80 = 1) */
    volatile LONG display_changed_pending;
    volatile LONG visibility_held_logged;       /* the "visibility change held back until the render loop exists" line was logged for the current pending change */
    volatile LONG display_w, display_h;         /* last known monitor size */
    DWORD launched_tick;
    float page_w, page_h;                       /* last size pushed into the Page */
    int auto_rotation;                          /* DisplayInformation.AutoRotationPreferences (stored, ignored) */
    X104AppApplication *application;            /* the object IApplicationFactory.CreateInstance made (NULL before) */
    X104Element *page;                          /* the object IPageFactory.CreateInstance made (NULL before) */
    X104Event back_pressed;                     /* HardwareButtons.BackPressed (a static event) */
    X104Event contents_invalidated;             /* DisplayInformation.DisplayContentsInvalidated (a static event) */
    /* games without the XAML-page template (REDCON 1) */
    X104Event core_suspending;                  /* CoreApplication.Suspending (static): raised by the exit proc together with Application.Suspending */
    X104Event core_resuming;                    /* CoreApplication.Resuming (static): never raised (the host never suspends a running game) */
    X104Event rendering;                        /* CompositionTarget.Rendering (static): the whole game frame; raised by x104_app_rendering_tick */
    volatile LONG in_rendering;                 /* a Rendering handler is running (a modal loop inside a frame must not start another frame) */
    volatile LONG rendering_frames;             /* Rendering ticks delivered */
    volatile LONG rendering_exceptions;         /* consecutive ticks that ended in an escaped exception */
} X104AppState;

static X104AppState x104_app_g;

static void x104_app_log_exception(const char *what, const X104Seh *seh)
{
    x104_logf("X104 app: EXCEPTION escaped from %s: code=%08lx at %p (game rva %08lx) info=%08lx %08lx%s", what, (unsigned long)seh->code, seh->address,
              (unsigned long)seh->rva, (unsigned long)seh->info0, (unsigned long)seh->info1, seh->code == 0xE06D7363u ? " [C++ exception]" : "");
}

/* client size in DIPs (== pixels, scale 1.0): the panel unit's value, else the real client rect of the window */
static void x104_app_client_size(float *width, float *height)
{
    float w = 0.0f;
    float h = 0.0f;
    HWND window = x104_app_g.window ? x104_app_g.window : x104.window;
    RECT rect;
    x104_panel_client_size(&w, &h);
    if (!(w > 0.0f) || !(h > 0.0f)) {
        if (window && GetClientRect(window, &rect)) {
            w = (float)(rect.right - rect.left);
            h = (float)(rect.bottom - rect.top);
        }
    }
    if (width) *width = w;
    if (height) *height = h;
}

/* window state -> pending visibility for the tick (R1: the window messages only record) */
static void x104_app_set_pending_hidden(LONG hidden, const char *why)
{
    LONG previous = InterlockedExchange(&x104_app_g.pending_hidden, hidden);
    if (previous != hidden) x104_logf("X104 app: window state -> %s (%s)", hidden ? "minimised / hidden" : "visible", why);
}

/* AppMain+0x0C != NULL: the game's StartRenderLoop ran at least once.  StopRenderLoop (0x1DA9D4, reached from the Suspending handler 0x1DFA40 and from the visibility
 * handler 0x1DF9AC) does `cbz [AppMain+0xC]` -> r4 = 0 -> `ldr r3,[r4]` -> access violation INSIDE the AppMain critical_section scoped_lock; the SEH guard unwinds without the game's
 * destructor, so the non-recursive lock would stay held for ever.  Both events are therefore only delivered once this cell is set (the READY tick 0x1DF8A4 / the first
 * visible=true runs StartRenderLoop, after attachUi, which is also the ordering R3 wants). */
static BOOL x104_app_render_loop_exists(void)
{
    DWORD value = 0;
    BYTE *app;
    if (!x104_feat(X104_FEAT_XAML_PAGE)) return TRUE;       /* no AppMain: frames come from CompositionTarget.Rendering, nothing to guard */
    app = x104_game_appmain();
    return app && x104_read_u32(app + X104_APP_APPMAIN_RENDER_ACTION, &value) && value != 0;
}

/* HSTRING out parameter from a (pointer, length) slice; length 0 -> NULL HSTRING (== empty string) */
static HRESULT x104_app_return_hstring(const wchar_t *text, UINT32 length, HSTRING *value)
{
    if (!value) return E_POINTER;
    *value = NULL;
    if (!text || !length) return S_OK;
    return WindowsCreateString(text, length, value);
}

/* ======================================================================================================================
 * 2. launch args, suspending args / operation / deferral, back-pressed args
 * ====================================================================================================================== */
/* ---- Windows.ApplicationModel.Activation.LaunchActivatedEventArgs: views[0] = ILaunchActivatedEventArgs (what OnLaunched receives, R10), second view IActivatedEventArgs ---- */
static HRESULT X104_API x104_app_launch_get_arguments(void *self, HSTRING *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;                                           /* empty launch arguments */
    return S_OK;
}

static HRESULT X104_API x104_app_launch_get_tile_id(void *self, HSTRING *value)
{
    (void)self;
    return x104_hstring_create(L"App", value);
}

static HRESULT X104_API x104_app_activated_get_kind(void *self, int *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;                                              /* ActivationKind.Launch */
    return S_OK;
}

static HRESULT X104_API x104_app_activated_get_previous_state(void *self, int *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;                                              /* ApplicationExecutionState.NotRunning (3 = Terminated would run MainPage::vf11 LoadState) */
    X104_LOG_LIMITED(&logged, 3, "X104 app: IActivatedEventArgs.get_PreviousExecutionState -> 0 (NotRunning)");
    return S_OK;
}

static HRESULT X104_API x104_app_activated_get_splash_screen(void *self, void **value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

X104_VTBL(x104_app_vt_launch, ILaunchActivatedEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ILaunchActivatedEventArgs, get_Arguments, x104_app_launch_get_arguments),
    X104_SLOT(ILaunchActivatedEventArgs, get_TileId, x104_app_launch_get_tile_id)
};
X104_VTBL(x104_app_vt_activated, IActivatedEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IActivatedEventArgs, get_Kind, x104_app_activated_get_kind),
    X104_SLOT(IActivatedEventArgs, get_PreviousExecutionState, x104_app_activated_get_previous_state),
    X104_SLOT(IActivatedEventArgs, get_SplashScreen, x104_app_activated_get_splash_screen)
};
static const X104IfaceDesc x104_app_launch_ifaces[] = {
    X104_IFACE(ILaunchActivatedEventArgs, x104_app_vt_launch),
    X104_IFACE(IActivatedEventArgs, x104_app_vt_activated)
};
X104_CLASS_DEF(x104_app_launch_class, "LaunchActivatedEventArgs", L"Windows.ApplicationModel.Activation.LaunchActivatedEventArgs", x104_app_launch_ifaces, X104_CLASS_AGILE, NULL);

/* ---- Windows.ApplicationModel.SuspendingEventArgs / SuspendingOperation / SuspendingDeferral (the game's handler ignores them, but they are never NULL) ---- */
static HRESULT X104_API x104_app_susp_args_get_operation(void *self, void **value)
{
    X104Object *obj = X104_OBJ(self);
    if (!value) return E_POINTER;
    *value = NULL;
    return x104_return_iface((X104Object *)obj->user[0], &X104_IID_ISuspendingOperation, value, "SuspendingEventArgs.SuspendingOperation");
}

static HRESULT X104_API x104_app_deferral_complete(void *self)
{
    (void)self;
    x104_logf("X104 app: SuspendingDeferral.Complete()");
    return S_OK;
}

X104_VTBL(x104_app_vt_deferral, ISuspendingDeferral) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ISuspendingDeferral, Complete, x104_app_deferral_complete)
};
static const X104IfaceDesc x104_app_deferral_ifaces[] = { X104_IFACE(ISuspendingDeferral, x104_app_vt_deferral) };
X104_CLASS_DEF(x104_app_deferral_class, "SuspendingDeferral", L"Windows.ApplicationModel.SuspendingDeferral", x104_app_deferral_ifaces, X104_CLASS_AGILE, NULL);

static HRESULT X104_API x104_app_susp_op_get_deferral(void *self, void **deferral)
{
    X104Object *obj;
    (void)self;
    if (!deferral) return E_POINTER;
    *deferral = NULL;
    obj = x104_object_new(&x104_app_deferral_class, sizeof(X104Object));
    if (!obj) return E_OUTOFMEMORY;
    InterlockedIncrement(&obj->refs);                        /* refs = 2: one owned by the caller, one kept by the host (REDCON 1 releases it twice) */
    *deferral = &obj->views[0];
    x104_logf("X104 app: SuspendingOperation.GetDeferral() -> %p", *deferral);
    return S_OK;
}

static HRESULT X104_API x104_app_susp_op_get_deadline(void *self, LONGLONG *value)
{
    X104AppFileTime now;
    LONGLONG ticks;
    (void)self;
    if (!value) return E_POINTER;
    GetSystemTimeAsFileTime(&now);
    ticks = ((LONGLONG)now.high << 32) | (LONGLONG)now.low;
    *value = ticks + 50000000LL;                             /* DateTime = FILETIME ticks (100 ns since 1601): now + 5 s */
    return S_OK;
}

X104_VTBL(x104_app_vt_susp_op, ISuspendingOperation) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ISuspendingOperation, GetDeferral, x104_app_susp_op_get_deferral),
    X104_SLOT(ISuspendingOperation, get_Deadline, x104_app_susp_op_get_deadline)
};
static const X104IfaceDesc x104_app_susp_op_ifaces[] = { X104_IFACE(ISuspendingOperation, x104_app_vt_susp_op) };
X104_CLASS_DEF(x104_app_susp_op_class, "SuspendingOperation", L"Windows.ApplicationModel.SuspendingOperation", x104_app_susp_op_ifaces, X104_CLASS_AGILE, NULL);

X104_VTBL(x104_app_vt_susp_args, ISuspendingEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ISuspendingEventArgs, get_SuspendingOperation, x104_app_susp_args_get_operation)
};
static const X104IfaceDesc x104_app_susp_args_ifaces[] = { X104_IFACE(ISuspendingEventArgs, x104_app_vt_susp_args) };
X104_CLASS_DEF(x104_app_susp_args_class, "SuspendingEventArgs", L"Windows.ApplicationModel.SuspendingEventArgs", x104_app_susp_args_ifaces, X104_CLASS_AGILE, NULL);

/* ---- Windows.Phone.UI.Input.BackPressedEventArgs: the game never QIs it, it calls vtable slot 7 (put_Handled) straight on the pointer it receives (R10).  The IID of
 *      IBackPressedEventArgs is not in the recovered facts: the view carries a host-private IID, a QI for the genuine one would show up as a QI MISS line. ---- */
static const GUID x104_app_iid_back_args = {0x104BAC0Du, 0x0A9Eu, 0x4104u, {0x8B, 0xAC, 0x0D, 0x0A, 0x9E, 0x10, 0x4B, 0xAC}};

static HRESULT X104_API x104_app_back_get_handled(void *self, BYTE *value)
{
    if (!value) return E_POINTER;
    *value = (BYTE)(X104_THIS(X104AppBackArgs, self)->handled != 0);
    return S_OK;
}

static HRESULT X104_API x104_app_back_put_handled(void *self, BYTE value)
{
    InterlockedExchange(&X104_THIS(X104AppBackArgs, self)->handled, value ? 1 : 0);
    return S_OK;
}

X104_VTBL(x104_app_vt_back_args, IBackPressedEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IBackPressedEventArgs, get_Handled, x104_app_back_get_handled),
    X104_SLOT(IBackPressedEventArgs, put_Handled, x104_app_back_put_handled)
};
static const X104IfaceDesc x104_app_back_args_ifaces[] = { X104_IFACE_IID(&x104_app_iid_back_args, "IBackPressedEventArgs", x104_app_vt_back_args) };
X104_CLASS_DEF(x104_app_back_args_class, "BackPressedEventArgs", L"Windows.Phone.UI.Input.BackPressedEventArgs", x104_app_back_args_ifaces, X104_CLASS_AGILE, NULL);

/* the one args object, reused for every BackPressed (raised on the UI thread only, one at a time) */
static X104AppBackArgs *x104_app_back_args(void)
{
    static volatile LONG state;
    static X104AppBackArgs *args;
    if (x104_once_begin(&state)) {
        args = (X104AppBackArgs *)x104_object_new(&x104_app_back_args_class, sizeof(X104AppBackArgs));
        x104_once_end(&state);
    }
    return args;
}

/* ======================================================================================================================
 * 3. Windows.Foundation.Uri  (IUriRuntimeClassFactory 6 CreateUri / 7 CreateWithRelativeUri, IUriRuntimeClass)
 * ====================================================================================================================== */
typedef struct X104AppUriParts {
    UINT32 scheme_len;
    UINT32 host_off, host_len;
    UINT32 path_off, path_len;
    UINT32 query_off, query_len;               /* includes the leading '?' */
    UINT32 frag_off, frag_len;                 /* includes the leading '#' */
    INT port;
} X104AppUriParts;

static BOOL x104_app_is_scheme_char(wchar_t c, BOOL first)
{
    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) return TRUE;
    return !first && ((c >= L'0' && c <= L'9') || c == L'+' || c == L'-' || c == L'.');
}

static BOOL x104_app_scheme_is(const wchar_t *text, UINT32 length, const char *name)
{
    UINT32 index = 0;
    while (name[index]) {
        wchar_t c = index < length ? text[index] : 0;
        if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
        if (c != (wchar_t)(unsigned char)name[index]) return FALSE;
        ++index;
    }
    return index == length;
}

/* splits scheme://[userinfo@]host[:port]/path?query#fragment (no dot-segment handling, no percent decoding) */
static void x104_app_uri_parse(const wchar_t *text, UINT32 length, X104AppUriParts *parts)
{
    UINT32 index = 0;
    UINT32 scan = 0;
    memset(parts, 0, sizeof(*parts));
    while (scan < length && x104_app_is_scheme_char(text[scan], scan == 0)) ++scan;
    if (scan > 0 && scan < length && text[scan] == L':') {
        parts->scheme_len = scan;
        index = scan + 1;
    }
    if (index + 1 < length && text[index] == L'/' && text[index + 1] == L'/') {
        UINT32 start;
        UINT32 end;
        UINT32 at;
        UINT32 colon;
        index += 2;
        start = index;
        while (index < length && text[index] != L'/' && text[index] != L'?' && text[index] != L'#') ++index;
        end = index;
        for (at = end; at > start; --at) {
            if (text[at - 1] == L'@') break;
        }
        start = at;                                          /* skips "user:password@" */
        colon = end;
        while (colon > start && text[colon - 1] >= L'0' && text[colon - 1] <= L'9') --colon;
        if (colon > start && colon < end && text[colon - 1] == L':') {
            UINT32 digit;
            INT port = 0;
            for (digit = colon; digit < end && port < 100000; ++digit) port = port * 10 + (INT)(text[digit] - L'0');
            parts->port = port;
            end = colon - 1;
        }
        parts->host_off = start;
        parts->host_len = end - start;
    }
    parts->path_off = index;
    while (index < length && text[index] != L'?' && text[index] != L'#') ++index;
    parts->path_len = index - parts->path_off;
    if (index < length && text[index] == L'?') {
        parts->query_off = index;
        ++index;
        while (index < length && text[index] != L'#') ++index;
        parts->query_len = index - parts->query_off;
    }
    if (index < length && text[index] == L'#') {
        parts->frag_off = index;
        parts->frag_len = length - index;
    }
    if (!parts->port) {
        if (x104_app_scheme_is(text, parts->scheme_len, "http")) parts->port = 80;
        else if (x104_app_scheme_is(text, parts->scheme_len, "https")) parts->port = 443;
        else if (x104_app_scheme_is(text, parts->scheme_len, "ftp")) parts->port = 21;
    }
}

/* base + relative -> a new heap string (NUL terminated, never freed by the host); NULL on out of memory */
static wchar_t *x104_app_uri_combine(const wchar_t *base, UINT32 base_len, const wchar_t *relative, UINT32 relative_len, UINT32 *out_len)
{
    X104AppUriParts base_parts;
    X104AppUriParts rel_parts;
    UINT32 prefix = 0;                       /* characters of `base` kept */
    const wchar_t *extra = NULL;             /* text inserted between the kept prefix and `relative` */
    UINT32 extra_len = 0;
    UINT32 total;
    wchar_t *result;
    x104_app_uri_parse(base, base_len, &base_parts);
    x104_app_uri_parse(relative, relative_len, &rel_parts);
    if (rel_parts.scheme_len || !base_len) {
        prefix = 0;                                                   /* the relative part is already absolute (or there is no base) */
    } else if (relative_len >= 2 && relative[0] == L'/' && relative[1] == L'/') {
        prefix = base_parts.scheme_len ? base_parts.scheme_len + 1 : 0;       /* scheme-relative: "scheme:" + "//host/..." */
    } else if (relative_len >= 1 && relative[0] == L'/') {
        prefix = base_parts.path_off;                                 /* "scheme://authority" + "/path" */
    } else if (relative_len >= 1 && relative[0] == L'?') {
        prefix = base_parts.path_off + base_parts.path_len;           /* base without query and fragment */
    } else if (relative_len >= 1 && relative[0] == L'#') {
        prefix = base_parts.path_off + base_parts.path_len + base_parts.query_len;
    } else {
        UINT32 slash = base_parts.path_len;                           /* directory of the base path, including its last '/' */
        while (slash > 0 && base[base_parts.path_off + slash - 1] != L'/') --slash;
        if (slash) {
            prefix = base_parts.path_off + slash;
        } else {
            static const wchar_t slash_text[] = L"/";
            prefix = base_parts.path_off;
            if (base_parts.host_len) {                                /* "scheme://host" + "/" + relative */
                extra = slash_text;
                extra_len = 1;
            }
        }
    }
    total = prefix + extra_len + relative_len;
    result = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)(total + 1) * sizeof(wchar_t));
    if (!result) return NULL;
    if (prefix) memcpy(result, base, (SIZE_T)prefix * sizeof(wchar_t));
    if (extra_len) memcpy(result + prefix, extra, (SIZE_T)extra_len * sizeof(wchar_t));
    if (relative_len) memcpy(result + prefix + extra_len, relative, (SIZE_T)relative_len * sizeof(wchar_t));
    result[total] = 0;
    if (out_len) *out_len = total;
    return result;
}

static HRESULT X104_API x104_app_uri_get_absolute(void *self, HSTRING *value)
{
    X104AppUri *uri = X104_THIS(X104AppUri, self);
    return x104_app_return_hstring(uri->text, uri->length, value);
}

#define X104_APP_URI_PART_GETTER(name, off_field, len_field) \
    static HRESULT X104_API name(void *self, HSTRING *value) \
    { \
        X104AppUri *uri = X104_THIS(X104AppUri, self); \
        X104AppUriParts parts; \
        if (!value) return E_POINTER; \
        x104_app_uri_parse(uri->text, uri->length, &parts); \
        return x104_app_return_hstring(uri->text + parts.off_field, parts.len_field, value); \
    }
X104_APP_URI_PART_GETTER(x104_app_uri_get_host, host_off, host_len)
X104_APP_URI_PART_GETTER(x104_app_uri_get_path, path_off, path_len)
X104_APP_URI_PART_GETTER(x104_app_uri_get_query, query_off, query_len)
X104_APP_URI_PART_GETTER(x104_app_uri_get_fragment, frag_off, frag_len)

static HRESULT X104_API x104_app_uri_get_scheme(void *self, HSTRING *value)
{
    X104AppUri *uri = X104_THIS(X104AppUri, self);
    X104AppUriParts parts;
    if (!value) return E_POINTER;
    x104_app_uri_parse(uri->text, uri->length, &parts);
    return x104_app_return_hstring(uri->text, parts.scheme_len, value);
}

static HRESULT X104_API x104_app_uri_get_port(void *self, INT *value)
{
    X104AppUri *uri = X104_THIS(X104AppUri, self);
    X104AppUriParts parts;
    if (!value) return E_POINTER;
    x104_app_uri_parse(uri->text, uri->length, &parts);
    *value = parts.port;
    return S_OK;
}

static HRESULT X104_API x104_app_uri_combine_uri(void *self, HSTRING relative, void **instance);

X104_VTBL(x104_app_vt_uri, IUriRuntimeClass) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IUriRuntimeClass, get_AbsoluteUri, x104_app_uri_get_absolute),
    X104_SLOT(IUriRuntimeClass, get_DisplayUri, x104_app_uri_get_absolute),
    X104_SLOT(IUriRuntimeClass, get_Fragment, x104_app_uri_get_fragment),
    X104_SLOT(IUriRuntimeClass, get_Host, x104_app_uri_get_host),
    X104_SLOT(IUriRuntimeClass, get_Path, x104_app_uri_get_path),
    X104_SLOT(IUriRuntimeClass, get_Query, x104_app_uri_get_query),
    X104_SLOT(IUriRuntimeClass, get_RawUri, x104_app_uri_get_absolute),
    X104_SLOT(IUriRuntimeClass, get_SchemeName, x104_app_uri_get_scheme),
    X104_SLOT(IUriRuntimeClass, get_Port, x104_app_uri_get_port),
    X104_SLOT(IUriRuntimeClass, CombineUri, x104_app_uri_combine_uri)
};
static const X104IfaceDesc x104_app_uri_ifaces[] = { X104_IFACE(IUriRuntimeClass, x104_app_vt_uri) };
X104_CLASS_DEF(x104_app_uri_class, "Uri", L"Windows.Foundation.Uri", x104_app_uri_ifaces, X104_CLASS_AGILE, NULL);

/* a new Uri object holding a heap copy of text[0..length); *instance = its IUriRuntimeClass view (refs = 1, owned by the caller) */
static HRESULT x104_app_uri_create(const wchar_t *text, UINT32 length, void **instance)
{
    X104AppUri *uri;
    wchar_t *copy;
    if (!instance) return E_POINTER;
    *instance = NULL;
    uri = (X104AppUri *)x104_object_new(&x104_app_uri_class, sizeof(X104AppUri));
    copy = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, ((SIZE_T)length + 1) * sizeof(wchar_t));
    if (!uri || !copy) {
        x104_logf("X104 app: ERROR Uri: out of memory creating a Uri of %lu characters", (unsigned long)length);
        return E_OUTOFMEMORY;
    }
    if (length) memcpy(copy, text, (SIZE_T)length * sizeof(wchar_t));
    copy[length] = 0;
    uri->text = copy;
    uri->length = length;
    *instance = &uri->base.views[0];
    return S_OK;
}

static HRESULT X104_API x104_app_uri_combine_uri(void *self, HSTRING relative, void **instance)
{
    X104AppUri *uri = X104_THIS(X104AppUri, self);
    UINT32 relative_len = 0;
    const wchar_t *relative_text = relative ? WindowsGetStringRawBuffer(relative, &relative_len) : NULL;
    UINT32 total = 0;
    wchar_t *joined;
    HRESULT hr;
    if (!instance) return E_POINTER;
    *instance = NULL;
    joined = x104_app_uri_combine(uri->text, uri->length, relative_text ? relative_text : L"", relative_text ? relative_len : 0, &total);
    if (!joined) {
        x104_logf("X104 app: ERROR Uri.CombineUri: out of memory");
        return E_OUTOFMEMORY;
    }
    hr = x104_app_uri_create(joined, total, instance);
    HeapFree(GetProcessHeap(), 0, joined);
    return hr;
}

static HRESULT X104_API x104_app_urifactory_create_uri(void *self, HSTRING text, void **instance)
{
    static volatile LONG logged;
    UINT32 length = 0;
    const wchar_t *raw;
    HRESULT hr;
    char ascii[160];
    (void)self;
    if (!instance) return E_POINTER;
    *instance = NULL;
    raw = text ? WindowsGetStringRawBuffer(text, &length) : NULL;
    if (!raw || !length) {
        x104_logf("X104 app: ERROR IUriRuntimeClassFactory.CreateUri(empty string) -> E_INVALIDARG (a Windows.Foundation.Uri cannot be empty)");
        return E_INVALIDARG;
    }
    hr = x104_app_uri_create(raw, length, instance);
    x104_hstring_ascii(text, ascii, sizeof(ascii));
    X104_LOG_LIMITED(&logged, 8, "X104 app: Uri.CreateUri(\"%s\") -> %p hr=%08lx", ascii, *instance, (unsigned long)hr);
    return hr;
}

static HRESULT X104_API x104_app_urifactory_create_relative(void *self, HSTRING base, HSTRING relative, void **instance)
{
    static volatile LONG logged;
    UINT32 base_len = 0;
    UINT32 relative_len = 0;
    UINT32 total = 0;
    const wchar_t *base_text;
    const wchar_t *relative_text;
    wchar_t *joined;
    HRESULT hr;
    (void)self;
    if (!instance) return E_POINTER;
    *instance = NULL;
    base_text = base ? WindowsGetStringRawBuffer(base, &base_len) : NULL;
    relative_text = relative ? WindowsGetStringRawBuffer(relative, &relative_len) : NULL;
    if (!base_text || !base_len) {
        x104_logf("X104 app: ERROR IUriRuntimeClassFactory.CreateWithRelativeUri(empty base) -> E_INVALIDARG");
        return E_INVALIDARG;
    }
    joined = x104_app_uri_combine(base_text, base_len, relative_text ? relative_text : L"", relative_text ? relative_len : 0, &total);
    if (!joined) {
        x104_logf("X104 app: ERROR IUriRuntimeClassFactory.CreateWithRelativeUri: out of memory");
        return E_OUTOFMEMORY;
    }
    hr = x104_app_uri_create(joined, total, instance);
    X104_LOG_LIMITED(&logged, 8, "X104 app: Uri.CreateWithRelativeUri(base %lu chars, relative %lu chars) -> %p hr=%08lx", (unsigned long)base_len,
                     (unsigned long)relative_len, *instance, (unsigned long)hr);
    HeapFree(GetProcessHeap(), 0, joined);
    return hr;
}

X104_VTBL(x104_app_vt_urifactory, IUriRuntimeClassFactory) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IUriRuntimeClassFactory, CreateUri, x104_app_urifactory_create_uri),
    X104_SLOT(IUriRuntimeClassFactory, CreateWithRelativeUri, x104_app_urifactory_create_relative)
};
static const X104IfaceDesc x104_app_urifactory_ifaces[] = { X104_IFACE(IUriRuntimeClassFactory, x104_app_vt_urifactory) };
X104_CLASS_DEF(x104_app_urifactory_class, "UriFactory", L"Windows.Foundation.Uri", x104_app_urifactory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_urifactory, x104_app_urifactory_class, X104Object)

/* text of a Uri object the game hands us (our own Uri in practice) for the logs; never fails */
static void x104_app_describe_uri(void *uri, char *out, SIZE_T capacity)
{
    HSTRING text = NULL;
    HRESULT hr;
    out[0] = 0;
    if (!uri) {
        lstrcpynA(out, "(NULL uri)", (int)capacity);
        return;
    }
    if (IsBadReadPtr(uri, sizeof(void *)) || IsBadReadPtr(*(void **)uri, 8 * sizeof(void *))) {
        wsprintfA(out, "(unreadable uri %p)", uri);
        return;
    }
    hr = X104_VFN(uri, X104_SLOT_IUriRuntimeClass_get_AbsoluteUri, HRESULT (X104_API *)(void *, HSTRING *))(uri, &text);
    if (SUCCEEDED(hr)) {
        x104_hstring_ascii(text, out, (UINT32)capacity);
        x104_hstring_free(text);
    } else {
        wsprintfA(out, "(Uri.get_AbsoluteUri failed 0x%08lx)", (unsigned long)hr);
    }
}

/* ======================================================================================================================
 * 4. Application (the inner object of the game's App2) and the exit path
 * ====================================================================================================================== */
#define X104_APP_EVENT_PAIR(add_name, remove_name, type, field) \
    static HRESULT X104_API add_name(void *self, void *handler, LONGLONG *token) \
    { \
        return x104_event_add(&X104_THIS(type, self)->field, handler, token); \
    } \
    static HRESULT X104_API remove_name(void *self, LONGLONG token) \
    { \
        return x104_event_remove(&X104_THIS(type, self)->field, token); \
    }

#define X104_APP_GLOBAL_EVENT_PAIR(add_name, remove_name, field) \
    static HRESULT X104_API add_name(void *self, void *handler, LONGLONG *token) \
    { \
        (void)self; \
        return x104_event_add(&x104_app_g.field, handler, token); \
    } \
    static HRESULT X104_API remove_name(void *self, LONGLONG token) \
    { \
        (void)self; \
        return x104_event_remove(&x104_app_g.field, token); \
    }

X104_APP_EVENT_PAIR(x104_app_application_add_unhandled, x104_app_application_remove_unhandled, X104AppApplication, unhandled)
X104_APP_EVENT_PAIR(x104_app_application_add_suspending, x104_app_application_remove_suspending, X104AppApplication, suspending)
X104_APP_EVENT_PAIR(x104_app_application_add_resuming, x104_app_application_remove_resuming, X104AppApplication, resuming)

static HRESULT X104_API x104_app_application_exit(void *self)
{
    (void)self;
    x104_logf("X104 app: Application.Exit() called by the game");
    x104_app_request_exit(0);
    return S_OK;
}

X104_VTBL(x104_app_vt_application, IApplication) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplication, add_UnhandledException, x104_app_application_add_unhandled),
    X104_SLOT(IApplication, remove_UnhandledException, x104_app_application_remove_unhandled),
    X104_SLOT(IApplication, add_Suspending, x104_app_application_add_suspending),
    X104_SLOT(IApplication, remove_Suspending, x104_app_application_remove_suspending),
    X104_SLOT(IApplication, add_Resuming, x104_app_application_add_resuming),
    X104_SLOT(IApplication, remove_Resuming, x104_app_application_remove_resuming),
    X104_SLOT(IApplication, Exit, x104_app_application_exit)
};
/* views[0] = IApplication: the inner pointer the game stores (identity, IInspectable) AND the IApplication instance are the same view */
static const X104IfaceDesc x104_app_application_ifaces[] = { X104_IFACE(IApplication, x104_app_vt_application) };
X104_CLASS_DEF(x104_app_application_class, "Application", L"Windows.UI.Xaml.Application", x104_app_application_ifaces, X104_CLASS_AGILE, NULL);

/* Suspending(sender = IApplication view, args = SuspendingEventArgs view) to the handlers the game registered; returns how many handlers ran.  Exit path only. */
static unsigned x104_app_raise_suspending(void)
{
    X104AppApplication *application = x104_app_g.application;
    X104Object *args;
    X104Object *operation;
    unsigned count;
    unsigned raised;
    unsigned core_count = x104_event_count(&x104_app_g.core_suspending);
    if (!application) {
        x104_logf("X104 app: Suspending not raised: the game never created its Application object");
        return 0;
    }
    count = x104_event_count(&application->suspending);
    if (!count && !core_count) {
        x104_logf("X104 app: Suspending not raised: no handler is registered");
        return 0;
    }
    args = x104_object_new(&x104_app_susp_args_class, sizeof(X104Object));
    operation = x104_object_new(&x104_app_susp_op_class, sizeof(X104Object));
    if (!args || !operation) {
        x104_logf("X104 app: ERROR Suspending not raised: out of memory creating the event args");
        return 0;
    }
    args->user[0] = operation;
    x104_logf("X104 app: raising Suspending (sender=%p args=%p, %u handler(s) registered)", &application->base.views[0], &args->views[0], count);
    raised = count ? x104_event_invoke_guarded(&application->suspending, &application->base.views[0], &args->views[0]) : 0;
    if (core_count) {
        unsigned core_raised;
        x104_logf("X104 app: raising CoreApplication.Suspending (args=%p, %u handler(s) registered)", &args->views[0], core_count);
        core_raised = x104_event_invoke_guarded(&x104_app_g.core_suspending, NULL, &args->views[0]);
        x104_logf("X104 app: CoreApplication.Suspending raised to %u handler(s)", core_raised);
        raised += core_raised;
    }
    x104_logf("X104 app: Suspending raised to %u handler(s)", raised);
    return raised;
}

/* The exit proc (UI thread, loop top level - or inline if the queue is closed): hide the window at once (the user sees the click work), Suspending only if the game is
 * ready (R16: its handler touches client+0x118/+0x11C, NULL while the init job still compiles shaders), async shutdown (<= 2 s), then leave the loop. */
static void x104_app_exit_proc(void *argument)
{
    int code = (int)(INT_PTR)argument;
    HWND window = x104_app_g.window ? x104_app_g.window : x104.window;
    BOOL ready;
    if (__sync_val_compare_and_swap(&x104_app_g.exit_proc_ran, 0, 1) != 0) {
        x104_logf("X104 app: exit proc already ran - this call is ignored");
        return;
    }
    ready = x104_game_is_ready();
    x104_logf("X104 app: exit proc started (code %d, thread %04lx, launched=%d, game ready=%d)", code, (unsigned long)GetCurrentThreadId(), (int)x104.f_launched, (int)ready);
    if (window && IsWindow(window)) ShowWindow(window, X104_APP_SW_HIDE);
    if (x104.f_launched && ready && !x104_app_render_loop_exists()) {
        x104_logf("X104 app: Suspending skipped: render loop never started (AppMain+0xC == NULL, StopRenderLoop would dereference NULL under the AppMain lock)");
    } else if (x104.f_launched && ready) {
        x104_app_raise_suspending();
    } else if (!ready) {
        x104_logf("X104 app: exit before ready: Suspending skipped (AppMain+0x3C == 0)");
    } else {
        x104_logf("X104 app: exit before OnLaunched returned: Suspending skipped");
    }
    x104_async_shutdown();
    x104_logf("X104 app: exit proc: workers stopped, leaving the UI loop (code %d)", code);
    x104_ui_quit(code);
    x104_logf("X104 app: exit proc done");
}

/* ======================================================================================================================
 * 5. Window  (IWindowStatics 6 get_Current, IWindow)
 * ====================================================================================================================== */
X104_APP_EVENT_PAIR(x104_app_window_add_activated, x104_app_window_remove_activated, X104AppWindow, activated)
X104_APP_EVENT_PAIR(x104_app_window_add_closed, x104_app_window_remove_closed, X104AppWindow, closed)
X104_APP_EVENT_PAIR(x104_app_window_add_size_changed, x104_app_window_remove_size_changed, X104AppWindow, size_changed)
X104_APP_EVENT_PAIR(x104_app_window_add_visibility_changed, x104_app_window_remove_visibility_changed, X104AppWindow, visibility_changed)

static HRESULT X104_API x104_app_window_get_bounds(void *self, X104Rect *value)
{
    float w = 0.0f;
    float h = 0.0f;
    (void)self;
    if (!value) return E_POINTER;
    x104_app_client_size(&w, &h);
    value->X = 0.0f;
    value->Y = 0.0f;
    value->Width = w;
    value->Height = h;
    return S_OK;
}

static HRESULT X104_API x104_app_window_get_visible(void *self, BYTE *value)
{
    HWND window = x104_app_g.window ? x104_app_g.window : x104.window;
    (void)self;
    if (!value) return E_POINTER;
    *value = (BYTE)(window && IsWindow(window) && IsWindowVisible(window) && !IsIconic(window));
    return S_OK;
}

static HRESULT X104_API x104_app_window_get_content(void *self, void **value)
{
    X104AppWindow *window = X104_THIS(X104AppWindow, self);
    if (!value) return E_POINTER;
    x104_lock(&window->content_lock);
    *value = window->content;
    if (*value) x104_raw_addref(*value);
    x104_unlock(&window->content_lock);
    return S_OK;
}

/* IWindow.put_Content(IUIElement*): the game's OnLaunched passes the page (QI(page, IUIElement) = the Page inner's IUIElement view).  Setting it is the signal that the
 * MainPage constructor has returned (AppMain, the timer and every handler exist): the panel unit may deliver the first SizeChanged. */
static HRESULT X104_API x104_app_window_put_content(void *self, void *content)
{
    X104AppWindow *window = X104_THIS(X104AppWindow, self);
    void *old;
    BOOL is_page = FALSE;
    if (content) x104_raw_addref(content);
    x104_lock(&window->content_lock);
    old = window->content;
    window->content = content;
    x104_unlock(&window->content_lock);
    if (content && x104_app_g.page && !IsBadReadPtr(content, sizeof(X104View))) is_page = ((X104View *)content)->obj == &x104_app_g.page->base;
    if (!x104_feat(X104_FEAT_XAML_PAGE)) {
        BOOL is_panel = content && !IsBadReadPtr(content, sizeof(X104View)) && ((X104View *)content)->obj == x104_panel_object();
        x104_logf("X104 app: Window.put_Content(%p) [%s]", content, content ? (is_panel ? "the code-built SwapChainPanel" : "NOT the host's SwapChainPanel") : "NULL: content cleared");
        if (old) x104_raw_release(old);
        return S_OK;
    }
    x104_logf("X104 app: Window.put_Content(%p) [%s]", content, content ? (is_page ? "the Page inner of IPageFactory.CreateInstance" : "NOT the Page inner this host created") : "NULL: content cleared");
    if (old) x104_raw_release(old);
    if (content) x104_flag_set(&x104.f_content_set, "f_content_set: Window.Content set: the MainPage constructor has returned");
    return S_OK;
}

static HRESULT X104_API x104_app_window_get_core_window(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_input_core_window(), &X104_IID_ICoreWindow, value, "Window.CoreWindow");
}

static HRESULT X104_API x104_app_window_get_dispatcher(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_async_dispatcher_object(), &X104_IID_ICoreDispatcher, value, "Window.Dispatcher");
}

/* show + activate the (until now hidden) game window; their synchronous window messages are harmless (R1: the units only record state) */
static BOOL x104_app_show_window(const char *why)
{
    HWND window = x104_app_g.window ? x104_app_g.window : x104.window;
    RECT rect;
    int w = 0;
    int h = 0;
    if (!window || !IsWindow(window)) {
        x104_logf("X104 app: ERROR cannot show the game window (%s): handle %p is not a window", why, (void *)window);
        return FALSE;
    }
    ShowWindow(window, X104_APP_SW_SHOW);
    UpdateWindow(window);
    SetForegroundWindow(window);
    SetFocus(window);
    if (GetClientRect(window, &rect)) {
        w = (int)(rect.right - rect.left);
        h = (int)(rect.bottom - rect.top);
    }
    x104_logf("X104 app: Window.Activate -> window shown (client %dx%d, visible=%d, iconic=%d) [%s]", w, h, (int)IsWindowVisible(window), (int)IsIconic(window), why);
    return TRUE;
}

static HRESULT X104_API x104_app_window_activate(void *self)
{
    (void)self;
    InterlockedExchange(&x104_app_g.activate_called, 1);
    x104_logf("X104 app: Window.Activate (thread %04lx)", (unsigned long)GetCurrentThreadId());
    return x104_app_show_window("Window.Activate") ? S_OK : E_FAIL;
}

static HRESULT X104_API x104_app_window_close(void *self)
{
    (void)self;
    x104_logf("X104 app: Window.Close() called by the game");
    x104_app_request_exit(0);
    return S_OK;
}

X104_VTBL(x104_app_vt_window, IWindow) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IWindow, get_Bounds, x104_app_window_get_bounds),
    X104_SLOT(IWindow, get_Visible, x104_app_window_get_visible),
    X104_SLOT(IWindow, get_Content, x104_app_window_get_content),
    X104_SLOT(IWindow, put_Content, x104_app_window_put_content),
    X104_SLOT(IWindow, get_CoreWindow, x104_app_window_get_core_window),
    X104_SLOT(IWindow, get_Dispatcher, x104_app_window_get_dispatcher),
    X104_SLOT(IWindow, add_Activated, x104_app_window_add_activated),
    X104_SLOT(IWindow, remove_Activated, x104_app_window_remove_activated),
    X104_SLOT(IWindow, add_Closed, x104_app_window_add_closed),
    X104_SLOT(IWindow, remove_Closed, x104_app_window_remove_closed),
    X104_SLOT(IWindow, add_SizeChanged, x104_app_window_add_size_changed),
    X104_SLOT(IWindow, remove_SizeChanged, x104_app_window_remove_size_changed),
    X104_SLOT(IWindow, add_VisibilityChanged, x104_app_window_add_visibility_changed),
    X104_SLOT(IWindow, remove_VisibilityChanged, x104_app_window_remove_visibility_changed),
    X104_SLOT(IWindow, Activate, x104_app_window_activate),
    X104_SLOT(IWindow, Close, x104_app_window_close)
};
static const X104IfaceDesc x104_app_window_ifaces[] = { X104_IFACE(IWindow, x104_app_vt_window) };
X104_CLASS_DEF(x104_app_window_class, "Window", L"Windows.UI.Xaml.Window", x104_app_window_ifaces, X104_CLASS_AGILE, NULL);

/* the one Window object (Window.Current) */
static X104AppWindow *x104_app_window_object(void)
{
    static volatile LONG state;
    static X104AppWindow *window;
    if (x104_once_begin(&state)) {
        window = (X104AppWindow *)x104_object_new(&x104_app_window_class, sizeof(X104AppWindow));
        if (window) {
            x104_event_init(&window->activated, "Window.Activated");
            x104_event_init(&window->closed, "Window.Closed");
            x104_event_init(&window->size_changed, "Window.SizeChanged");
            x104_event_init(&window->visibility_changed, "Window.VisibilityChanged");
        }
        x104_once_end(&state);
    }
    return window;
}

static HRESULT X104_API x104_app_windowstatics_get_current(void *self, void **value)
{
    static volatile LONG logged;
    X104AppWindow *window = x104_app_window_object();
    (void)self;
    X104_LOG_LIMITED(&logged, 6, "X104 app: Window.Current (thread %04lx) -> window object %p", (unsigned long)GetCurrentThreadId(), (void *)window);
    return x104_return_iface(window ? &window->base : NULL, &X104_IID_IWindow, value, "Window.Current");
}

X104_VTBL(x104_app_vt_windowstatics, IWindowStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IWindowStatics, get_Current, x104_app_windowstatics_get_current)
};
static const X104IfaceDesc x104_app_windowstatics_ifaces[] = { X104_IFACE(IWindowStatics, x104_app_vt_windowstatics) };
X104_CLASS_DEF(x104_app_windowstatics_class, "WindowStatics", L"Windows.UI.Xaml.Window", x104_app_windowstatics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_windowstatics, x104_app_windowstatics_class, X104Object)

/* ======================================================================================================================
 * 6. Page  (IPageFactory 6 CreateInstance; the inner object is an XAML element with FindName)
 * ====================================================================================================================== */
/* FindName on the Page (called by InitializeComponent 0x1EDDD8 through the MainPage's QI(IFrameworkElement), four times): the four elements of MainPage.xaml, which
 * the other units own.  Names are case sensitive.  The result is the identity (IInspectable) view; the game QIs it for ISwapChainPanel / IImage / ITextBox / IButton. */
static HRESULT x104_app_page_find_name(X104Element *element, HSTRING name, void **result)
{
    char text[96];
    X104Object *found = NULL;
    BOOL known = TRUE;
    HRESULT hr;
    (void)element;
    x104_hstring_ascii(name, text, sizeof(text));
    if (x104_hstring_equals(name, L"swapChainPanel")) found = x104_panel_object();
    else if (x104_hstring_equals(name, L"Logo")) found = x104_input_element_logo();
    else if (x104_hstring_equals(name, L"EditBox")) found = x104_input_element_editbox();
    else if (x104_hstring_equals(name, L"Button")) found = x104_input_element_button();
    else known = FALSE;
    if (!known) {
        *result = NULL;
        x104_logf("X104 app: FindName(\"%s\") -> NULL (unknown name; S_OK)", text);
        return S_OK;
    }
    hr = x104_return_iface(found, &X104_IID_IInspectable, result, text);
    x104_logf("X104 app: FindName(\"%s\") -> %p (hr=0x%08lx %s)", text, result ? *result : NULL, (unsigned long)hr, x104_hr_name(hr));
    return hr;
}

X104_VTBL(x104_app_vt_page, IPage) = {
    X104_INSPECTABLE_SLOTS
};
static const X104IfaceDesc x104_app_page_ifaces[] = {
    X104_ELEMENT_IFACES,
    X104_IFACE(IPage, x104_app_vt_page)
};
X104_CLASS_DEF(x104_app_page_class, "Page", L"Windows.UI.Xaml.Controls.Page", x104_app_page_ifaces, X104_CLASS_AGILE, NULL);

/* IPageFactory.CreateInstance(outer = MainPage+8, &inner (-> MainPage+0x24), &instance): inner = the identity view, instance = the IPage view (the declared type of the third
 * parameter).  outer is AddRef'd once and kept by x104_aggregate_create. */
static HRESULT X104_API x104_app_pagefactory_create_instance(void *self, void *outer, void **inner, void **instance)
{
    X104Object *obj = NULL;
    X104Element *element;
    HRESULT hr;
    float w = 0.0f;
    float h = 0.0f;
    (void)self;
    hr = x104_aggregate_create(&x104_app_page_class, sizeof(X104Element), outer, &X104_IID_IPage, inner, instance, &obj, "IPageFactory.CreateInstance");
    if (FAILED(hr)) {
        x104_logf("X104 app: ERROR IPageFactory.CreateInstance(outer=%p) failed: 0x%08lx %s", outer, (unsigned long)hr, x104_hr_name(hr));
        return hr;
    }
    element = (X104Element *)obj;
    x104_event_init(&element->size_changed, "Page.SizeChanged");
    element->label = "Page";
    element->find_name = x104_app_page_find_name;
    x104_app_client_size(&w, &h);
    x104_element_set_sizes(element, (double)w, (double)h);
    x104_app_g.page = element;
    x104_app_g.page_w = w;
    x104_app_g.page_h = h;
    if (outer) {
        x104.mainpage_outer = outer;
        x104_logf("X104 app: Page CreateInstance(outer=%p) mainpage=%p inner=%p instance=%p (page size %dx%d)", outer, x104_game_mainpage(), inner ? *inner : NULL,
                  instance ? *instance : NULL, (int)w, (int)h);
        x104_flag_set(&x104.f_mainpage_created, "f_mainpage_created: IPageFactory.CreateInstance seen, MainPage = outer - 8 recorded");
    } else {
        x104_logf("X104 app: WARNING Page CreateInstance with outer == NULL: the MainPage cannot be located (x104_game_mainpage() stays NULL, the first SizeChanged will wait for ever)");
    }
    return S_OK;
}

X104_VTBL(x104_app_vt_pagefactory, IPageFactory) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IPageFactory, CreateInstance, x104_app_pagefactory_create_instance)
};
static const X104IfaceDesc x104_app_pagefactory_ifaces[] = { X104_IFACE(IPageFactory, x104_app_vt_pagefactory) };
X104_CLASS_DEF(x104_app_pagefactory_class, "PageFactory", L"Windows.UI.Xaml.Controls.Page", x104_app_pagefactory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_pagefactory, x104_app_pagefactory_class, X104Object)

/* ======================================================================================================================
 * 7. StatusBar / Canvas / HardwareButtons / DisplayInformation
 * ====================================================================================================================== */
/* ---- Windows.UI.ViewManagement.StatusBar: HideAsync is the one call (OnLaunched, result released at once) ---- */
static HRESULT X104_API x104_app_statusbar_show_async(void *self, void **value)
{
    (void)self;
    x104_logf("X104 app: StatusBar.ShowAsync (no status bar on the desktop)");
    return x104_return_iface(x104_completed_action(), &X104_IID_IAsyncAction, value, "StatusBar.ShowAsync");
}

static HRESULT X104_API x104_app_statusbar_hide_async(void *self, void **value)
{
    (void)self;
    x104_logf("X104 app: StatusBar.HideAsync (no status bar on the desktop: completed action returned)");
    return x104_return_iface(x104_completed_action(), &X104_IID_IAsyncAction, value, "StatusBar.HideAsync");
}

X104_VTBL(x104_app_vt_statusbar, IStatusBar) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IStatusBar, ShowAsync, x104_app_statusbar_show_async),
    X104_SLOT(IStatusBar, HideAsync, x104_app_statusbar_hide_async)
};
static const X104IfaceDesc x104_app_statusbar_ifaces[] = { X104_IFACE(IStatusBar, x104_app_vt_statusbar) };
X104_CLASS_DEF(x104_app_statusbar_class, "StatusBar", L"Windows.UI.ViewManagement.StatusBar", x104_app_statusbar_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_app_get_statusbar, x104_app_statusbar_class, X104Object)

static HRESULT X104_API x104_app_statusbarstatics_get_for_current_view(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_app_get_statusbar(), &X104_IID_IStatusBar, value, "StatusBar.GetForCurrentView");
}

X104_VTBL(x104_app_vt_statusbarstatics, IStatusBarStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IStatusBarStatics, GetForCurrentView, x104_app_statusbarstatics_get_for_current_view)
};
static const X104IfaceDesc x104_app_statusbarstatics_ifaces[] = { X104_IFACE(IStatusBarStatics, x104_app_vt_statusbarstatics) };
X104_CLASS_DEF(x104_app_statusbarstatics_class, "StatusBarStatics", L"Windows.UI.ViewManagement.StatusBar", x104_app_statusbarstatics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_statusbarstatics, x104_app_statusbarstatics_class, X104Object)

/* ---- Windows.UI.Xaml.Controls.Canvas: SetLeft(element, 3000.0) twice from the MainPage constructor (EditBox / Button off screen); `length` arrives in d0 (a C double) ---- */
static HRESULT X104_API x104_app_canvas_set_left(void *self, void *element, double length)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 4, "X104 app: Canvas.SetLeft(%p, %d)", element, (int)length);
    return S_OK;
}

X104_VTBL(x104_app_vt_canvas, ICanvasStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICanvasStatics, SetLeft, x104_app_canvas_set_left)
};
static const X104IfaceDesc x104_app_canvas_ifaces[] = { X104_IFACE(ICanvasStatics, x104_app_vt_canvas) };
X104_CLASS_DEF(x104_app_canvas_class, "CanvasStatics", L"Windows.UI.Xaml.Controls.Canvas", x104_app_canvas_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_canvas, x104_app_canvas_class, X104Object)

/* ---- Windows.Phone.UI.Input.HardwareButtons: add_BackPressed (slot 6, medium confidence) from the MainPage constructor ---- */
static HRESULT X104_API x104_app_hwbuttons_add_back_pressed(void *self, void *handler, LONGLONG *token)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 4, "X104 app: HardwareButtons.add_BackPressed handler=%p", handler);
    return x104_event_add(&x104_app_g.back_pressed, handler, token);
}

static HRESULT X104_API x104_app_hwbuttons_remove_back_pressed(void *self, LONGLONG token)
{
    (void)self;
    return x104_event_remove(&x104_app_g.back_pressed, token);
}

X104_VTBL(x104_app_vt_hwbuttons, IHardwareButtonsStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IHardwareButtonsStatics, add_BackPressed, x104_app_hwbuttons_add_back_pressed),
    X104_SLOT(IHardwareButtonsStatics, remove_BackPressed, x104_app_hwbuttons_remove_back_pressed)
};
static const X104IfaceDesc x104_app_hwbuttons_ifaces[] = { X104_IFACE(IHardwareButtonsStatics, x104_app_vt_hwbuttons) };
X104_CLASS_DEF(x104_app_hwbuttons_class, "HardwareButtons", L"Windows.Phone.UI.Input.HardwareButtons", x104_app_hwbuttons_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_hwbuttons, x104_app_hwbuttons_class, X104Object)

/* ---- Windows.Graphics.Display.DisplayInformation: landscape, 96 dpi, scale 100%.  The handlers read the SENDER (the DisplayInformation object) with direct vtable calls, so
 *      views[0] is the IDisplayInformation view and that very view is passed as sender (R10). ---- */
X104_APP_EVENT_PAIR(x104_app_di_add_orientation_changed, x104_app_di_remove_orientation_changed, X104AppDisplay, orientation_changed)
X104_APP_EVENT_PAIR(x104_app_di_add_dpi_changed, x104_app_di_remove_dpi_changed, X104AppDisplay, dpi_changed)

static HRESULT X104_API x104_app_di_get_current_orientation(void *self, int *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = X104_APP_ORIENTATION_LANDSCAPE;
    X104_LOG_LIMITED(&logged, 1, "X104 app: DisplayInformation.get_CurrentOrientation -> %d (Landscape)", *value);
    return S_OK;
}

static HRESULT X104_API x104_app_di_get_native_orientation(void *self, int *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = X104_APP_ORIENTATION_LANDSCAPE;
    X104_LOG_LIMITED(&logged, 1, "X104 app: DisplayInformation.get_NativeOrientation -> %d (Landscape: no width/height swap)", *value);
    return S_OK;
}

static HRESULT X104_API x104_app_di_get_resolution_scale(void *self, int *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = X104_APP_SCALE_100;
    X104_LOG_LIMITED(&logged, 1, "X104 app: DisplayInformation.get_ResolutionScale -> %d", *value);
    return S_OK;
}

static HRESULT X104_API x104_app_di_get_logical_dpi(void *self, float *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = 96.0f;
    X104_LOG_LIMITED(&logged, 1, "X104 app: DisplayInformation.get_LogicalDpi -> 96");
    return S_OK;
}

static HRESULT X104_API x104_app_di_get_raw_dpi_x(void *self, float *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = 96.0f;
    X104_LOG_LIMITED(&logged, 1, "X104 app: DisplayInformation.get_RawDpiX -> 96");
    return S_OK;
}

static HRESULT X104_API x104_app_di_get_raw_dpi_y(void *self, float *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = 96.0f;
    X104_LOG_LIMITED(&logged, 1, "X104 app: DisplayInformation.get_RawDpiY -> 96");
    return S_OK;
}

X104_VTBL(x104_app_vt_display, IDisplayInformation) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IDisplayInformation, get_CurrentOrientation, x104_app_di_get_current_orientation),
    X104_SLOT(IDisplayInformation, get_NativeOrientation, x104_app_di_get_native_orientation),
    X104_SLOT(IDisplayInformation, add_OrientationChanged, x104_app_di_add_orientation_changed),
    X104_SLOT(IDisplayInformation, remove_OrientationChanged, x104_app_di_remove_orientation_changed),
    X104_SLOT(IDisplayInformation, get_ResolutionScale, x104_app_di_get_resolution_scale),
    X104_SLOT(IDisplayInformation, get_LogicalDpi, x104_app_di_get_logical_dpi),
    X104_SLOT(IDisplayInformation, get_RawDpiX, x104_app_di_get_raw_dpi_x),
    X104_SLOT(IDisplayInformation, get_RawDpiY, x104_app_di_get_raw_dpi_y),
    X104_SLOT(IDisplayInformation, add_DpiChanged, x104_app_di_add_dpi_changed),
    X104_SLOT(IDisplayInformation, remove_DpiChanged, x104_app_di_remove_dpi_changed)
};
/* IDisplayInformation2.get_RawPixelsPerViewPixel: 1.0 (the host reports 96 dpi / scale 1.0 everywhere; REDCON 1 multiplies CoreWindow.Bounds by it) */
static HRESULT X104_API x104_app_di_get_raw_pixels_per_view_pixel(void *self, double *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = 1.0;
    X104_LOG_LIMITED(&logged, 1, "X104 app: DisplayInformation.get_RawPixelsPerViewPixel -> 1.0");
    return S_OK;
}

X104_VTBL(x104_app_vt_display2, IDisplayInformation2) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IDisplayInformation2, get_RawPixelsPerViewPixel, x104_app_di_get_raw_pixels_per_view_pixel)
};
static const X104IfaceDesc x104_app_display_ifaces[] = { X104_IFACE(IDisplayInformation, x104_app_vt_display), X104_IFACE(IDisplayInformation2, x104_app_vt_display2) };
X104_CLASS_DEF(x104_app_display_class, "DisplayInformation", L"Windows.Graphics.Display.DisplayInformation", x104_app_display_ifaces, X104_CLASS_AGILE, NULL);

/* the one DisplayInformation object (GetForCurrentView) */
static X104AppDisplay *x104_app_display_object(void)
{
    static volatile LONG state;
    static X104AppDisplay *display;
    if (x104_once_begin(&state)) {
        display = (X104AppDisplay *)x104_object_new(&x104_app_display_class, sizeof(X104AppDisplay));
        if (display) {
            x104_event_init(&display->orientation_changed, "DisplayInformation.OrientationChanged");
            x104_event_init(&display->dpi_changed, "DisplayInformation.DpiChanged");
        }
        x104_once_end(&state);
    }
    return display;
}

static HRESULT X104_API x104_app_displaystatics_get_for_current_view(void *self, void **current)
{
    static volatile LONG logged;
    X104AppDisplay *display = x104_app_display_object();
    (void)self;
    X104_LOG_LIMITED(&logged, 6, "X104 app: DisplayInformation.GetForCurrentView (thread %04lx) -> object %p", (unsigned long)GetCurrentThreadId(), (void *)display);
    return x104_return_iface(display ? &display->base : NULL, &X104_IID_IDisplayInformation, current, "DisplayInformation.GetForCurrentView");
}

static HRESULT X104_API x104_app_displaystatics_get_auto_rotation(void *self, int *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = x104_app_g.auto_rotation;
    return S_OK;
}

static HRESULT X104_API x104_app_displaystatics_put_auto_rotation(void *self, int value)
{
    static volatile LONG logged;
    (void)self;
    x104_app_g.auto_rotation = value;
    X104_LOG_LIMITED(&logged, 3, "X104 app: DisplayInformation.AutoRotationPreferences = %d (stored, ignored)", value);
    return S_OK;
}

X104_APP_GLOBAL_EVENT_PAIR(x104_app_displaystatics_add_contents_invalidated, x104_app_displaystatics_remove_contents_invalidated, contents_invalidated)

X104_VTBL(x104_app_vt_displaystatics, IDisplayInformationStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IDisplayInformationStatics, GetForCurrentView, x104_app_displaystatics_get_for_current_view),
    X104_SLOT(IDisplayInformationStatics, get_AutoRotationPreferences, x104_app_displaystatics_get_auto_rotation),
    X104_SLOT(IDisplayInformationStatics, put_AutoRotationPreferences, x104_app_displaystatics_put_auto_rotation),
    X104_SLOT(IDisplayInformationStatics, add_DisplayContentsInvalidated, x104_app_displaystatics_add_contents_invalidated),
    X104_SLOT(IDisplayInformationStatics, remove_DisplayContentsInvalidated, x104_app_displaystatics_remove_contents_invalidated)
};
static const X104IfaceDesc x104_app_displaystatics_ifaces[] = { X104_IFACE(IDisplayInformationStatics, x104_app_vt_displaystatics) };
X104_CLASS_DEF(x104_app_displaystatics_class, "DisplayInformationStatics", L"Windows.Graphics.Display.DisplayInformation", x104_app_displaystatics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_displaystatics, x104_app_displaystatics_class, X104Object)

/* ======================================================================================================================
 * 7b. classes of games without the XAML-page template (REDCON 1: games_recon/redcon_redcon-startup.md F3-F6, F9):
 *     CoreApplication (Suspending / Resuming), ApplicationView (SuppressSystemOverlays), CoreWindowResizeManager (NotifyLayoutCompleted) and
 *     CompositionTarget (Rendering = the game frame, raised every UI pass by x104_app_rendering_tick)
 * ====================================================================================================================== */
X104_APP_GLOBAL_EVENT_PAIR(x104_app_core_add_suspending, x104_app_core_remove_suspending, core_suspending)
X104_APP_GLOBAL_EVENT_PAIR(x104_app_core_add_resuming, x104_app_core_remove_resuming, core_resuming)

X104_VTBL(x104_app_vt_coreapp, ICoreApplication) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreApplication, add_Suspending, x104_app_core_add_suspending),
    X104_SLOT(ICoreApplication, remove_Suspending, x104_app_core_remove_suspending),
    X104_SLOT(ICoreApplication, add_Resuming, x104_app_core_add_resuming),
    X104_SLOT(ICoreApplication, remove_Resuming, x104_app_core_remove_resuming)
};
static const X104IfaceDesc x104_app_coreapp_ifaces[] = { X104_IFACE(ICoreApplication, x104_app_vt_coreapp) };
X104_CLASS_DEF(x104_app_coreapp_class, "CoreApplicationStatics", L"Windows.ApplicationModel.Core.CoreApplication", x104_app_coreapp_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_coreapp, x104_app_coreapp_class, X104Object)

/* ApplicationView.GetForCurrentView() -> an object the game QIs for IApplicationView2 (slot 7 put_SuppressSystemOverlays: no system overlays on a desktop) */
static HRESULT X104_API x104_app_view_get_suppress(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 1;
    return S_OK;
}

static HRESULT X104_API x104_app_view_put_suppress(void *self, BYTE value)
{
    (void)self;
    x104_logf("X104 app: ApplicationView.put_SuppressSystemOverlays(%u) accepted (no-op on a desktop window)", (unsigned)value);
    return S_OK;
}

X104_VTBL(x104_app_vt_view2, IApplicationView2) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationView2, get_SuppressSystemOverlays, x104_app_view_get_suppress),
    X104_SLOT(IApplicationView2, put_SuppressSystemOverlays, x104_app_view_put_suppress)
};
static const X104IfaceDesc x104_app_view_ifaces[] = { X104_IFACE(IApplicationView2, x104_app_vt_view2) };
X104_CLASS_DEF(x104_app_view_class, "ApplicationView", L"Windows.UI.ViewManagement.ApplicationView", x104_app_view_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_app_get_view, x104_app_view_class, X104Object)

static HRESULT X104_API x104_app_viewstatics_get_for_current_view(void *self, void **current)
{
    (void)self;
    return x104_return_iface(x104_app_get_view(), &X104_IID_IApplicationView2, current, "ApplicationView.GetForCurrentView");
}

X104_VTBL(x104_app_vt_viewstatics, IApplicationViewStatics2) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationViewStatics2, GetForCurrentView, x104_app_viewstatics_get_for_current_view)
};
static const X104IfaceDesc x104_app_viewstatics_ifaces[] = { X104_IFACE(IApplicationViewStatics2, x104_app_vt_viewstatics) };
X104_CLASS_DEF(x104_app_viewstatics_class, "ApplicationViewStatics", L"Windows.UI.ViewManagement.ApplicationView", x104_app_viewstatics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_viewstatics, x104_app_viewstatics_class, X104Object)

/* CoreWindowResizeManager.GetForCurrentView().NotifyLayoutCompleted(): the end of the game's rotation handler; nothing waits for it on a desktop */
static HRESULT X104_API x104_app_resize_notify_layout_completed(void *self)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 4, "X104 app: CoreWindowResizeManager.NotifyLayoutCompleted (no-op)");
    return S_OK;
}

X104_VTBL(x104_app_vt_resize, ICoreWindowResizeManager) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreWindowResizeManager, NotifyLayoutCompleted, x104_app_resize_notify_layout_completed)
};
static const X104IfaceDesc x104_app_resize_ifaces[] = { X104_IFACE(ICoreWindowResizeManager, x104_app_vt_resize) };
X104_CLASS_DEF(x104_app_resize_class, "CoreWindowResizeManager", L"Windows.UI.Core.CoreWindowResizeManager", x104_app_resize_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_app_get_resize, x104_app_resize_class, X104Object)

static HRESULT X104_API x104_app_resizestatics_get_for_current_view(void *self, void **current)
{
    (void)self;
    return x104_return_iface(x104_app_get_resize(), &X104_IID_ICoreWindowResizeManager, current, "CoreWindowResizeManager.GetForCurrentView");
}

X104_VTBL(x104_app_vt_resizestatics, ICoreWindowResizeManagerStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICoreWindowResizeManagerStatics, GetForCurrentView, x104_app_resizestatics_get_for_current_view)
};
static const X104IfaceDesc x104_app_resizestatics_ifaces[] = { X104_IFACE(ICoreWindowResizeManagerStatics, x104_app_vt_resizestatics) };
X104_CLASS_DEF(x104_app_resizestatics_class, "CoreWindowResizeManagerStatics", L"Windows.UI.Core.CoreWindowResizeManager", x104_app_resizestatics_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_resizestatics, x104_app_resizestatics_class, X104Object)

/* CompositionTarget statics: Rendering (EventHandler<Object>; the handler ignores sender and args) and SurfaceContentsLost (accepted, never raised) */
static HRESULT X104_API x104_app_ct_add_rendering(void *self, void *handler, LONGLONG *token)
{
    HRESULT hr;
    (void)self;
    hr = x104_event_add(&x104_app_g.rendering, handler, token);
    x104_logf("X104 app: CompositionTarget.add_Rendering(%p) -> 0x%08lx (%u handler(s); frames are raised every UI pass while the window is visible)", handler,
              (unsigned long)hr, x104_event_count(&x104_app_g.rendering));
    return hr;
}

static HRESULT X104_API x104_app_ct_remove_rendering(void *self, LONGLONG token)
{
    HRESULT hr;
    (void)self;
    hr = x104_event_remove(&x104_app_g.rendering, token);
    x104_logf("X104 app: CompositionTarget.remove_Rendering -> %u handler(s) left (the game paused its frames)", x104_event_count(&x104_app_g.rendering));
    return hr;
}

static HRESULT X104_API x104_app_ct_add_surface_lost(void *self, void *handler, LONGLONG *token)
{
    static LONGLONG next = 0x5C0000;
    (void)self;
    (void)handler;
    if (!token) return E_POINTER;
    *token = ++next;
    return S_OK;
}

static HRESULT X104_API x104_app_ct_remove_surface_lost(void *self, LONGLONG token)
{
    (void)self;
    (void)token;
    return S_OK;
}

X104_VTBL(x104_app_vt_compositiontarget, ICompositionTargetStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICompositionTargetStatics, add_Rendering, x104_app_ct_add_rendering),
    X104_SLOT(ICompositionTargetStatics, remove_Rendering, x104_app_ct_remove_rendering),
    X104_SLOT(ICompositionTargetStatics, add_SurfaceContentsLost, x104_app_ct_add_surface_lost),
    X104_SLOT(ICompositionTargetStatics, remove_SurfaceContentsLost, x104_app_ct_remove_surface_lost)
};
static const X104IfaceDesc x104_app_compositiontarget_ifaces[] = { X104_IFACE(ICompositionTargetStatics, x104_app_vt_compositiontarget) };
X104_CLASS_DEF(x104_app_compositiontarget_class, "CompositionTargetStatics", L"Windows.UI.Xaml.Media.CompositionTarget", x104_app_compositiontarget_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_compositiontarget, x104_app_compositiontarget_class, X104Object)

/* TRUE while a Rendering handler is registered and the window shows: the UI loop then waits at most 1 ms (the game's own limiter and Present(1,0) pace the frames) */
static BOOL x104_app_rendering_active(void)
{
    HWND window = x104_app_g.window ? x104_app_g.window : x104.window;
    return x104_event_count(&x104_app_g.rendering) && x104.f_launched && !x104.f_exit_requested && window && IsWindowVisible(window) && !IsIconic(window);
}

/* UI thread, every pass: one CompositionTarget.Rendering = one game frame (update + draw + Present(1,0)) */
static void x104_app_rendering_tick(void)
{
    void *snapshot[X104_EVENT_MAX];
    unsigned count = 0;
    unsigned index;
    LONG frame;
    X104Event *event = &x104_app_g.rendering;
    if (!x104_app_rendering_active() || x104_app_g.delivered_hidden) return;
    if (x104_app_g.rendering_exceptions >= 30) return;          /* the frame keeps throwing: stop (logged once below) instead of spinning on it */
    if (InterlockedExchange(&x104_app_g.in_rendering, 1)) return;
    x104_lock(&event->lock);
    for (index = 0; index < X104_EVENT_MAX; ++index) {
        if (event->handler[index]) snapshot[count++] = event->handler[index];
    }
    for (index = 0; index < count; ++index) x104_raw_addref(snapshot[index]);
    x104_unlock(&event->lock);
    frame = InterlockedIncrement(&x104_app_g.rendering_frames);
    if (frame == 1) x104_logf("X104 app: first CompositionTarget.Rendering tick (%u handler(s), %lu ms after OnLaunched returned)", count,
                              (unsigned long)(GetTickCount() - x104_app_g.launched_tick));
    for (index = 0; index < count; ++index) {
        HRESULT hr = S_OK;
        X104Seh seh;
        if (!x104_call_guarded3(((void **)*(void ***)snapshot[index])[X104_SLOT_DELEGATE_Invoke], snapshot[index], NULL, NULL, &hr, &seh)) {
            LONG failures = InterlockedIncrement(&x104_app_g.rendering_exceptions);
            if (failures <= 5 || failures == 30)
                x104_logf("X104 app: EXCEPTION escaped from the Rendering handler (frame %ld): code=%08lx at %p (game rva %08lx)%s%s", (long)frame, (unsigned long)seh.code,
                          seh.address, (unsigned long)seh.rva, seh.code == 0xE06D7363u ? " [C++ exception]" : "",
                          failures == 30 ? " - 30 in a row: frames are no longer raised" : "");
        } else {
            InterlockedExchange(&x104_app_g.rendering_exceptions, 0);
        }
        x104_raw_release(snapshot[index]);
    }
    InterlockedExchange(&x104_app_g.in_rendering, 0);
}

/* ======================================================================================================================
 * 8. Application statics + factory: the fake Application::Start that drives the whole game
 * ====================================================================================================================== */
static HRESULT X104_API x104_app_statics_get_current(void *self, void **value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!x104_app_g.application) {
        x104_logf("X104 app: Application.get_Current but no application object exists yet -> E_FAIL");
        return E_FAIL;
    }
    return x104_object_qi(&x104_app_g.application->base, &X104_IID_IApplication, value);
}

/* Start() cannot continue: say so on the screen and in the log, leave without a loop (the process ends) */
static HRESULT x104_app_start_failed(void *callback, int code, const char *message)
{
    x104_logf("X104 app: FATAL: %s - Application::Start returns WITHOUT a UI loop (exit code %d recorded)", message, code);
    x104.exit_code = code;
    MessageBoxA(NULL, message, "Win32Craft", X104_APP_MB_ICONERROR);
    x104_raw_release(callback);
    return S_OK;
}

/* IApplicationStatics.Start(ApplicationInitializationCallback): in the real runtime this blocks until the app ends.  The UI thread runs it (HOST_DESIGN 1, 3). */
static HRESULT X104_API x104_app_statics_start(void *self, void *callback)
{
    HRESULT hr = S_OK;
    X104Seh seh;
    void *overrides = NULL;
    void *outer;
    X104Object *args;
    BOOL clean;
    int rc;
    (void)self;
    if (__sync_val_compare_and_swap(&x104_app_g.started, 0, 1) != 0) {
        x104_logf("X104 app: ERROR Application::Start called a second time -> E_ILLEGAL_METHOD_CALL");
        return X104_E_ILLEGAL_METHOD_CALL;
    }
    x104_logf("X104 app: Application::Start entered (callback=%p, thread %04lx, UI thread %04lx)", callback, (unsigned long)GetCurrentThreadId(), (unsigned long)x104.ui_thread_id);
    if (!callback) {
        x104_logf("X104 app: ERROR Application::Start(NULL callback) -> E_INVALIDARG");
        return E_INVALIDARG;
    }
    if (!x104_on_ui_thread())
        x104_logf("X104 app: WARNING Application::Start runs on thread %04lx which is not the UI thread %04lx: the UI loop will pump the wrong thread", (unsigned long)GetCurrentThreadId(),
                  (unsigned long)x104.ui_thread_id);
    x104_raw_addref(callback);

    /* 1. the initialisation callback: the game's lambda 0x1EE380 builds App2, whose constructor calls IApplicationFactory.CreateInstance (below) and registers Suspending/Resuming */
    memset(&seh, 0, sizeof(seh));
    clean = x104_call_guarded3(((void **)*(void ***)callback)[X104_SLOT_DELEGATE_Invoke], callback, NULL, NULL, &hr, &seh);
    if (!clean) {
        x104_app_log_exception("the ApplicationInitializationCallback (App2 construction)", &seh);
        return x104_app_start_failed(callback, 41, "The game raised an exception while it created its Application (App2). See win32craft.log.");
    }
    x104_logf("X104 app: initialisation callback returned 0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
    if (FAILED(hr)) return x104_app_start_failed(callback, 41, "The game's ApplicationInitializationCallback failed. See win32craft.log.");

    /* 2. App2 must exist: its constructor called CreateInstance and the host kept `outer` */
    outer = x104_app_g.application ? x104_app_g.application->base.outer : NULL;
    if (!x104_app_g.application || !outer) {
        return x104_app_start_failed(callback, 42, "App2 was not created: IApplicationFactory.CreateInstance was never called with an outer object. See win32craft.log.");
    }

    /* 3. deliver the launch: QI(outer, IApplicationOverrides) -> slot 7 OnLaunched(ILaunchActivatedEventArgs*) */
    hr = x104_raw_qi(outer, &X104_IID_IApplicationOverrides, &overrides);
    if (FAILED(hr) || !overrides) {
        x104_logf("X104 app: ERROR QI(outer=%p, IApplicationOverrides) failed: 0x%08lx %s", outer, (unsigned long)hr, x104_hr_name(hr));
        return x104_app_start_failed(callback, 43, "App2 does not answer IApplicationOverrides. See win32craft.log.");
    }
    args = x104_object_new(&x104_app_launch_class, sizeof(X104Object));
    if (!args) {
        x104_raw_release(overrides);
        return x104_app_start_failed(callback, 44, "Out of memory creating the launch arguments.");
    }
    x104_logf("X104 app: calling IApplicationOverrides.OnLaunched(overrides=%p, args=%p = ILaunchActivatedEventArgs view)", overrides, &args->views[0]);
    memset(&seh, 0, sizeof(seh));
    hr = S_OK;
    clean = x104_call_guarded3(((void **)*(void ***)overrides)[X104_SLOT_IApplicationOverrides_OnLaunched], overrides, &args->views[0], NULL, &hr, &seh);
    x104_logf("X104 app: OnLaunched -> hr=%08lx %s%s", (unsigned long)hr, x104_hr_name(hr), clean ? "" : " (EXCEPTION)");
    x104_raw_release(overrides);
    if (!clean) {
        x104_app_log_exception("App2::OnLaunched (MainPage construction / Window.Activate)", &seh);
        return x104_app_start_failed(callback, 45, "The game raised an exception inside OnLaunched (MainPage construction). See win32craft.log.");
    }
    if (FAILED(hr)) return x104_app_start_failed(callback, 45, "App2::OnLaunched returned a failure. See win32craft.log.");
    x104_app_g.launched_tick = GetTickCount();
    x104_flag_set(&x104.f_launched, "f_launched: OnLaunched returned");
    if (!x104_app_g.activate_called)
        x104_logf("X104 app: WARNING OnLaunched returned without calling Window.Activate: the window stays hidden (the tick's safety net shows it in %lu ms)", (unsigned long)X104_APP_ACTIVATE_GRACE_MS);

    /* 4. the UI loop: returns after x104_ui_quit (the exit proc) */
    x104_logf("X104 app: entering the UI loop (f_content_set=%d, f_mainpage_created=%d)", (int)x104.f_content_set, (int)x104.f_mainpage_created);
    rc = x104_ui_run_loop();
    x104_logf("X104 app: Start: UI loop returned %d", rc);
    if (!x104_app_g.exit_proc_ran) {
        x104_logf("X104 app: the loop ended without the app exit proc (WM_QUIT / WM_DESTROY path): stopping the workers now");
        x104_async_shutdown();
    }
    x104_raw_release(callback);
    return S_OK;
}

/* Application.LoadComponent / LoadComponentWithResourceLocation(component, uri [, location]): the XAML is never parsed (FindName supplies the four elements) */
static void x104_app_log_load_component(void *component, void *uri, int location, const char *what)
{
    char text[200];
    x104_app_describe_uri(uri, text, sizeof(text));
    x104_logf("X104 app: %s uri=%s component=%p location=%d (no-op: MainPage.xbf is never parsed, Page.FindName supplies the elements)", what, text, component, location);
}

static HRESULT X104_API x104_app_statics_load_component(void *self, void *component, void *uri)
{
    (void)self;
    x104_app_log_load_component(component, uri, 0, "LoadComponent");
    return S_OK;
}

static HRESULT X104_API x104_app_statics_load_component_loc(void *self, void *component, void *uri, int location)
{
    (void)self;
    x104_app_log_load_component(component, uri, location, "LoadComponentWithResourceLocation");
    return S_OK;
}

/* IApplicationFactory.CreateInstance(outer = App2+8, &inner (-> App2+0x14), &instance): the Application object the aggregating App2 gets back */
static HRESULT X104_API x104_app_factory_create_instance(void *self, void *outer, void **inner, void **instance)
{
    X104Object *obj = NULL;
    X104AppApplication *application;
    HRESULT hr;
    (void)self;
    hr = x104_aggregate_create(&x104_app_application_class, sizeof(X104AppApplication), outer, &X104_IID_IApplication, inner, instance, &obj, "IApplicationFactory.CreateInstance");
    if (FAILED(hr)) {
        x104_logf("X104 app: ERROR IApplicationFactory.CreateInstance(outer=%p) failed: 0x%08lx %s", outer, (unsigned long)hr, x104_hr_name(hr));
        return hr;
    }
    application = (X104AppApplication *)obj;
    x104_event_init(&application->suspending, "Application.Suspending");
    x104_event_init(&application->resuming, "Application.Resuming");
    x104_event_init(&application->unhandled, "Application.UnhandledException");
    if (x104_app_g.application)
        x104_logf("X104 app: WARNING a second Application object was created (the first was %p): Suspending goes to the new one", (void *)x104_app_g.application);
    x104_app_g.application = application;
    x104_logf("X104 app: CreateInstance(outer=%p inner=%p instance=%p) for Application", outer, inner ? *inner : NULL, instance ? *instance : NULL);
    if (!outer) x104_logf("X104 app: WARNING Application CreateInstance with outer == NULL: OnLaunched cannot be delivered");
    return S_OK;
}

X104_VTBL(x104_app_vt_statics, IApplicationStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationStatics, get_Current, x104_app_statics_get_current),
    X104_SLOT(IApplicationStatics, Start, x104_app_statics_start),
    X104_SLOT(IApplicationStatics, LoadComponent, x104_app_statics_load_component),
    X104_SLOT(IApplicationStatics, LoadComponentWithResourceLocation, x104_app_statics_load_component_loc)
};
X104_VTBL(x104_app_vt_factory, IApplicationFactory) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationFactory, CreateInstance, x104_app_factory_create_instance)
};
static const X104IfaceDesc x104_app_statics_ifaces[] = {
    X104_IFACE(IApplicationStatics, x104_app_vt_statics),
    X104_IFACE(IApplicationFactory, x104_app_vt_factory)
};
X104_CLASS_DEF(x104_app_statics_class, "ApplicationStatics", L"Windows.UI.Xaml.Application", x104_app_statics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_app_get_statics, x104_app_statics_class, X104Object)

/* ======================================================================================================================
 * 8b. --trace-game-state: snapshots of the game's own state (host option, OFF by default; READ-ONLY, every read behind IsBadReadPtr)
 *
 *     Why: first device run: menu fine, world renders, but no hotbar and no touch controls in the world.  arm32/v0_10_4/HUD_INVESTIGATION.md has the proof of what decides
 *     that.  In short, the HUD is drawn by GameRenderer::render 0xB9E1C -> Gui::render 0x6E2C4 (needs: client+0x68 level, client+0x12C player, options hideGui byte +0x244 == 0,
 *     and for the hotbar client+0x6C Screen == NULL); the touch buttons are drawn by player+0xD8C->vf2 (TouchscreenInput::vf2 0x9B14C), and player+0xD8C is set by
 *     MinecraftClient::vf40 0x58D8C only when LocalPlayer::vf149 0xAA160 moves the player's load state (player+0xDB4) from 1 to 3.
 *
 *     Log lines (all start with "X104 gstate", `m` = thousandths because the log has no floats, ptr 00000000 = null, FFFFFFFF/-1 = unreadable):
 *       #N [periodic|change] t=S.mmm   one header line, then `screen`, `options`, `platform`, `player` and `touch` lines.
 *     Schedule: every 1 s for the first 60 s after the client exists, then every 10 s; plus an extra snapshot (`change`) whenever the signature (screen, player, load state,
 *     input object, holder, hideGui, client size) changes (at most 80, at least 150 ms apart).
 * ====================================================================================================================== */
#define X104_APP_GS_DENSE_MS     60000u
#define X104_APP_GS_SLOW_MS      10000u
#define X104_APP_GS_MAX_CHANGES  80u
#define X104_APP_GS_SIG_WORDS    9
#define X104_APP_GS_RVA_GUI_SCALE 0x00DDE814u   /* float guiScale (written by MinecraftClient::setSize 0x59738); 1/guiScale is ARM0104_CLIENT_INIT_GLOBALS_INVGUISCALE */

typedef struct x104_app_gs_t {
    BOOL enabled;
    BOOL started;
    DWORD start_tick, next_tick, last_change_tick;
    unsigned snapshots, changes;
    BOOL sig_valid;
    DWORD sig[X104_APP_GS_SIG_WORDS];
} x104_app_gs_t;

static x104_app_gs_t x104_app_gs;

/* command line only (--trace-game-state), parsed by the unit itself so the core / boot stay untouched */
static void x104_app_gs_parse(void)
{
    const wchar_t *cursor = GetCommandLineW();
    wchar_t argument[128];
    if (!cursor) return;
    cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));          /* argv[0] */
    while (*cursor) {
        cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
        if (host_argument_equals(argument, L"--trace-game-state")) x104_app_gs.enabled = TRUE;
    }
}

static DWORD x104_app_gs_u32(const BYTE *base, DWORD offset)
{
    DWORD value;
    return (base && x104_read_u32(base + offset, &value)) ? value : 0xFFFFFFFFu;
}

static int x104_app_gs_i32(const BYTE *base, DWORD offset)
{
    DWORD value;
    return (base && x104_read_u32(base + offset, &value)) ? (int)value : -1;
}

static int x104_app_gs_u8(const BYTE *base, DWORD offset)
{
    BYTE value;
    return (base && x104_read_u8(base + offset, &value)) ? (int)value : -1;
}

/* the float at base+offset in thousandths (rounded); -999999 = unreadable, -999998 = NaN / infinity */
static int x104_app_gs_milli(const BYTE *base, DWORD offset)
{
    DWORD bits;
    union { DWORD u; float f; } value;
    float scaled;
    if (!base || !x104_read_u32(base + offset, &bits)) return -999999;
    if ((bits & 0x7F800000u) == 0x7F800000u) return -999998;
    value.u = bits;
    scaled = value.f * 1000.0f;
    if (scaled > 2000000000.0f) return 2000000000;
    if (scaled < -2000000000.0f) return -2000000000;
    return (int)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

/* RTTI class name of a game object: vtable -> complete object locator (vtable[-1]) -> type descriptor (+12) -> ".?AVName@@" at +8 (32-bit MSVC layout, absolute pointers,
 * relocated by the loader).  Returns `out`; never throws. */
static const char *x104_app_gs_class_name(DWORD object, char *out, SIZE_T capacity)
{
    DWORD vtable = 0, locator = 0, descriptor = 0, image_size;
    const BYTE *image = x104.image;
    const BYTE *pe;
    DWORD base;
    SIZE_T length = 0;
    const char *raw;
    out[0] = 0;
    if (!object) { wsprintfA(out, "(null)"); return out; }
    if (object == 0xFFFFFFFFu || !x104_read_u32((const void *)(ULONG_PTR)object, &vtable)) { wsprintfA(out, "(unreadable)"); return out; }
    pe = image + *(const DWORD *)(image + 0x3c);
    image_size = *(const DWORD *)(pe + 0x50);
    base = (DWORD)(ULONG_PTR)image;
    if (vtable < base + 4u || vtable >= base + image_size) { wsprintfA(out, "(vtable %08lx outside the game image)", (unsigned long)vtable); return out; }
    if (!x104_read_u32((const void *)(ULONG_PTR)(vtable - 4u), &locator) || locator < base || locator >= base + image_size) { wsprintfA(out, "(no RTTI at %08lx)", (unsigned long)vtable); return out; }
    if (!x104_read_u32((const void *)(ULONG_PTR)(locator + 12u), &descriptor) || descriptor < base || descriptor >= base + image_size) { wsprintfA(out, "(no type descriptor)"); return out; }
    raw = (const char *)(ULONG_PTR)(descriptor + 8u);
    if (!IsBadReadPtr(raw, 4) && raw[0] == '.' && raw[1] == '?' && raw[2] == 'A' && (raw[3] == 'V' || raw[3] == 'U')) raw += 4;
    while (length + 1 < capacity && !IsBadReadPtr(raw + length, 1) && raw[length] >= 0x20 && raw[length] < 0x7F && !(raw[length] == '@' && raw[length + 1] == '@')) {
        out[length] = raw[length];
        ++length;
    }
    out[length] = 0;
    if (!length) wsprintfA(out, "(empty name)");
    return out;
}

/* the 16 bytes of a RectangleArea (vtable, +4 flag, +8..+0x14 four floats) behind a pointer, in thousandths */
static void x104_app_gs_area(char *out, DWORD area)
{
    if (!area) { wsprintfA(out, "null"); return; }
    if (area == 0xFFFFFFFFu) { wsprintfA(out, "?"); return; }
    wsprintfA(out, "%08lx:%d,%d,%d,%d", (unsigned long)area, x104_app_gs_milli((const BYTE *)(ULONG_PTR)area, 0x08), x104_app_gs_milli((const BYTE *)(ULONG_PTR)area, 0x0C),
              x104_app_gs_milli((const BYTE *)(ULONG_PTR)area, 0x10), x104_app_gs_milli((const BYTE *)(ULONG_PTR)area, 0x14));
}

static void x104_app_gs_snapshot(const char *why, DWORD now)
{
    BYTE *app = x104_game_appmain();
    BYTE *client = x104_game_client();
    BYTE *image = x104.image;
    DWORD screen, pending, holder, gui, options, player, input, gamemode, level, platform_cell, platform_ptr, dr, timer, touch;
    char name_a[80], name_b[80], name_c[80], name_d[80], name_e[80];
    char areas[11][64];
    char line[1000];
    unsigned index;
    DWORD offset;
    const BYTE *platform;
    const BYTE *drp;
    if (!client) return;
    ++x104_app_gs.snapshots;
    screen = x104_app_gs_u32(client, 0x6C);
    pending = x104_app_gs_u32(client, 0x14C);
    holder = x104_app_gs_u32(client, 0x124);
    gui = x104_app_gs_u32(client, 0x130);
    options = x104_app_gs_u32(client, 0x134);
    player = x104_app_gs_u32(client, 0x12C);
    gamemode = x104_app_gs_u32(client, 0x40);
    level = x104_app_gs_u32(client, 0x68);
    timer = x104_app_gs_u32(client, 0xA8);
    input = (player && player != 0xFFFFFFFFu) ? x104_app_gs_u32((const BYTE *)(ULONG_PTR)player, 0xD8C) : 0;
    platform_cell = x104_app_gs_u32(app, 0x38);
    platform_ptr = (platform_cell && platform_cell != 0xFFFFFFFFu) ? x104_app_gs_u32((const BYTE *)(ULONG_PTR)platform_cell, 0) : 0;
    platform = (platform_ptr && platform_ptr != 0xFFFFFFFFu) ? (const BYTE *)(ULONG_PTR)platform_ptr : NULL;
    dr = x104_app_gs_u32(image, ARM0104_FRAME_RENDER_DEVICE_RESOURCES_GLOBAL_INSTANCE);
    drp = (dr && dr != 0xFFFFFFFFu) ? (const BYTE *)(ULONG_PTR)dr : NULL;

    x104_logf("X104 gstate #%u [%s] t=%lu.%03lu ready=%d client=%08lx init=%d renderEnabled=%d paused=%d grabbed=%d size=%dx%d platformSlot26Flag(+0x150)=%d frames(+0xF0)=%d kbdH=%d timer: tps=%dm ticks=%d partial=%dm",
              x104_app_gs.snapshots, why, (unsigned long)((now - x104_app_gs.start_tick) / 1000u), (unsigned long)((now - x104_app_gs.start_tick) % 1000u), (int)x104_game_is_ready(),
              (unsigned long)(ULONG_PTR)client, x104_app_gs_u8(client, 0x08), x104_app_gs_u8(client, 0x13C), x104_app_gs_u8(client, 0x3C), x104_app_gs_u8(client, 0x74),
              x104_app_gs_i32(client, 0x20), x104_app_gs_i32(client, 0x24), x104_app_gs_u8(client, 0x150), x104_app_gs_i32(client, 0xF0), x104_app_gs_i32(client, 0x164),
              x104_app_gs_milli((const BYTE *)(ULONG_PTR)(timer == 0xFFFFFFFFu ? 0 : timer), 0), x104_app_gs_i32((const BYTE *)(ULONG_PTR)(timer == 0xFFFFFFFFu ? 0 : timer), 4),
              x104_app_gs_milli((const BYTE *)(ULONG_PTR)(timer == 0xFFFFFFFFu ? 0 : timer), 8));

    x104_logf("X104 gstate  screen(+0x6C)=%08lx %s | pending(+0x149)=%d->%08lx inSetScreen(+0x148)=%d | level(+0x68)=%08lx gameMode(+0x40)=%08lx %s | inputHolder(+0x124)=%08lx %s | gui(+0x130)=%08lx %s slots(+0xB0C)=%d guiOwner(+0xAC4)=%s",
              (unsigned long)screen, x104_app_gs_class_name(screen, name_a, sizeof(name_a)), x104_app_gs_u8(client, 0x149), (unsigned long)pending, x104_app_gs_u8(client, 0x148),
              (unsigned long)level, (unsigned long)gamemode, x104_app_gs_class_name(gamemode, name_e, sizeof(name_e)), (unsigned long)holder, x104_app_gs_class_name(holder, name_b, sizeof(name_b)), (unsigned long)gui,
              x104_app_gs_class_name(gui, name_c, sizeof(name_c)), (gui && gui != 0xFFFFFFFFu) ? x104_app_gs_i32((const BYTE *)(ULONG_PTR)gui, 0xB0C) : -1,
              (gui && gui != 0xFFFFFFFFu) ? (x104_app_gs_u32((const BYTE *)(ULONG_PTR)gui, 0xAC4) == (DWORD)(ULONG_PTR)client ? "client" : "OTHER") : "?");

    {
        const BYTE *o = (options && options != 0xFFFFFFFFu) ? (const BYTE *)(ULONG_PTR)options : NULL;
        x104_logf("X104 gstate  options=%08lx hideGui(+0x244)=%d useTouchscreen(+0x276)=%d useTouchJoypad(+0x275)=%d leftHanded(+0x1D)=%d fancyGraphics(+0x1A)=%d vibrate(+0x1E)=%d animTex(+0x278)=%d fancySkies(+0x277)=%d serverVisible(+0x274)=%d pixelDensity(+0x28C)=%dm difficulty(+0x240)=%d | guiScale=%dm invGuiScale=%dm",
                  (unsigned long)options, x104_app_gs_u8(o, 0x244), x104_app_gs_u8(o, 0x276), x104_app_gs_u8(o, 0x275), x104_app_gs_u8(o, 0x1D), x104_app_gs_u8(o, 0x1A),
                  x104_app_gs_u8(o, 0x1E), x104_app_gs_u8(o, 0x278), x104_app_gs_u8(o, 0x277), x104_app_gs_u8(o, 0x274), x104_app_gs_milli(o, 0x28C), x104_app_gs_i32(o, 0x240),
                  x104_app_gs_milli(image, X104_APP_GS_RVA_GUI_SCALE), x104_app_gs_milli(image, ARM0104_CLIENT_INIT_GLOBALS_INVGUISCALE));
    }

    x104_logf("X104 gstate  platform=%08lx width(+0x30)=%d height(+0x34)=%d ppmm(+0x38)=%dm scaleX(+0x3C)=%dm scaleY(+0x40)=%dm | DR=%08lx viewport(+0x18)=%dm,%dm %dm x %dm z=%dm..%dm rtSize(+0x3C)=%dm x %dm outSize(+0x44)=%dm x %dm logical(+0x4C)=%dm x %dm ovr(+0x34)=%d prof(+0x35)=%d dpi(+0x5C)=%dm comp(+0x60)=%dm,%dm rtv=%08lx dsv=%08lx",
              (unsigned long)platform_ptr, x104_app_gs_i32(platform, 0x30), x104_app_gs_i32(platform, 0x34), x104_app_gs_milli(platform, 0x38), x104_app_gs_milli(platform, 0x3C),
              x104_app_gs_milli(platform, 0x40), (unsigned long)dr, x104_app_gs_milli(drp, 0x18), x104_app_gs_milli(drp, 0x1C), x104_app_gs_milli(drp, 0x20), x104_app_gs_milli(drp, 0x24),
              x104_app_gs_milli(drp, 0x28), x104_app_gs_milli(drp, 0x2C), x104_app_gs_milli(drp, 0x3C), x104_app_gs_milli(drp, 0x40), x104_app_gs_milli(drp, 0x44),
              x104_app_gs_milli(drp, 0x48), x104_app_gs_milli(drp, 0x4C), x104_app_gs_milli(drp, 0x50), x104_app_gs_u8(drp, 0x34), x104_app_gs_u8(drp, 0x35),
              x104_app_gs_milli(drp, 0x5C), x104_app_gs_milli(drp, 0x60), x104_app_gs_milli(drp, 0x64), (unsigned long)x104_app_gs_u32(drp, 0x10), (unsigned long)x104_app_gs_u32(drp, 0x14));

    {
        const BYTE *p = (player && player != 0xFFFFFFFFu) ? (const BYTE *)(ULONG_PTR)player : NULL;
        x104_logf("X104 gstate  player(+0x12C)=%08lx %s loadState(+0xDB4)=%d (1 loading, 3 ready) progress(+0xE0C)=%dm input(+0xD8C)=%08lx %s health(+0x138)=%d clientId(+0xCC8)=%d pos(+0x10)=%dm,%dm,%dm mc(+0xDB8)=%s",
                  (unsigned long)player, x104_app_gs_class_name(player, name_d, sizeof(name_d)), x104_app_gs_i32(p, 0xDB4), x104_app_gs_milli(p, 0xE0C), (unsigned long)input,
                  x104_app_gs_class_name(input, name_a, sizeof(name_a)), x104_app_gs_i32(p, 0x138), x104_app_gs_i32(p, 0xCC8), x104_app_gs_milli(p, 0x10), x104_app_gs_milli(p, 0x14),
                  x104_app_gs_milli(p, 0x18), p ? (x104_app_gs_u32(p, 0xDB8) == (DWORD)(ULONG_PTR)client ? "client" : "OTHER") : "?");
    }

    /* the touch layout: TouchscreenInput is the sub-object at TouchInputHolder+0x10; its button areas (RectangleArea: floats at +8..+0x14) sit at sub-object +0xF4..+0x11C */
    touch = (holder && holder != 0xFFFFFFFFu) ? holder + 0x10u : 0;
    for (index = 0, offset = 0xF4u; index < 11u; ++index, offset += 4u)
        x104_app_gs_area(areas[index], touch ? x104_app_gs_u32((const BYTE *)(ULONG_PTR)touch, offset) : 0);
    wsprintfA(line, "X104 gstate  touch: TouchscreenInput=%08lx flags: hidden(+0x11)=%d f(+0xDD)=%d f(+0xDE)=%d f(+0xDF)=%d options(+0xD8)=%08lx mc(+0xF0)=%08lx | areas(+0xF4..+0x11C; x0,y0,x1,y1 in thousandths) %s %s %s %s %s %s %s %s %s %s %s",
              (unsigned long)touch, x104_app_gs_u8((const BYTE *)(ULONG_PTR)touch, 0x11), x104_app_gs_u8((const BYTE *)(ULONG_PTR)touch, 0xDD), x104_app_gs_u8((const BYTE *)(ULONG_PTR)touch, 0xDE),
              x104_app_gs_u8((const BYTE *)(ULONG_PTR)touch, 0xDF), (unsigned long)x104_app_gs_u32((const BYTE *)(ULONG_PTR)touch, 0xD8), (unsigned long)x104_app_gs_u32((const BYTE *)(ULONG_PTR)touch, 0xF0),
              areas[0], areas[1], areas[2], areas[3], areas[4], areas[5], areas[6], areas[7], areas[8], areas[9], areas[10]);
    x104_logf("%s", line);
}

/* UI thread, every pass: cheap (one GetTickCount and a few guarded reads while nothing changes) */
static void x104_app_gs_tick(void)
{
    DWORD now = GetTickCount();
    BYTE *client;
    DWORD sig[X104_APP_GS_SIG_WORDS];
    DWORD player, options, holder;
    BOOL changed = FALSE;
    unsigned index;
    if (!x104_app_gs.enabled || !x104.f_units_ready || x104.f_exit_requested) return;
    client = x104_game_client();
    if (!client) return;
    if (!x104_app_gs.started) {
        x104_app_gs.started = TRUE;
        x104_app_gs.start_tick = now;
        x104_app_gs.next_tick = now;
        x104_logf("X104 gstate: --trace-game-state active: the game client exists (%p); snapshots every 1 s for 60 s, then every 10 s, plus 'change' snapshots", (void *)client);
    }
    player = x104_app_gs_u32(client, 0x12C);
    holder = x104_app_gs_u32(client, 0x124);
    options = x104_app_gs_u32(client, 0x134);
    sig[0] = x104_app_gs_u32(client, 0x6C);
    sig[1] = player;
    sig[2] = (player && player != 0xFFFFFFFFu) ? x104_app_gs_u32((const BYTE *)(ULONG_PTR)player, 0xDB4) : 0;
    sig[3] = (player && player != 0xFFFFFFFFu) ? x104_app_gs_u32((const BYTE *)(ULONG_PTR)player, 0xD8C) : 0;
    sig[4] = holder;
    sig[5] = (options && options != 0xFFFFFFFFu) ? (DWORD)x104_app_gs_u8((const BYTE *)(ULONG_PTR)options, 0x244) : 0;
    sig[6] = x104_app_gs_u32(client, 0x20);
    sig[7] = x104_app_gs_u32(client, 0x24);
    sig[8] = x104_app_gs_u32(client, 0x68);
    if (!x104_app_gs.sig_valid) {
        x104_app_gs.sig_valid = TRUE;
        for (index = 0; index < X104_APP_GS_SIG_WORDS; ++index) x104_app_gs.sig[index] = sig[index];
    } else {
        for (index = 0; index < X104_APP_GS_SIG_WORDS; ++index) if (sig[index] != x104_app_gs.sig[index]) changed = TRUE;
    }
    if (changed && x104_app_gs.changes < X104_APP_GS_MAX_CHANGES && (DWORD)(now - x104_app_gs.last_change_tick) >= 150u) {
        for (index = 0; index < X104_APP_GS_SIG_WORDS; ++index) x104_app_gs.sig[index] = sig[index];
        x104_app_gs.last_change_tick = now;
        ++x104_app_gs.changes;
        x104_app_gs_snapshot("change", now);
    }
    if ((int)(now - x104_app_gs.next_tick) >= 0) {
        x104_app_gs.next_tick = now + ((DWORD)(now - x104_app_gs.start_tick) < X104_APP_GS_DENSE_MS ? 1000u : X104_APP_GS_SLOW_MS);
        x104_app_gs_snapshot("periodic", now);
    }
}

/* ======================================================================================================================
 * 9. activation table and the contract functions (HOST_DESIGN.md section 8)
 * ====================================================================================================================== */
static const X104FactoryDef x104_app_factories[] = {
    { L"Windows.UI.Xaml.Application",                  x104_app_get_statics },
    { L"Windows.UI.Xaml.Controls.Page",                x104_app_get_pagefactory },
    { L"Windows.UI.Xaml.Window",                       x104_app_get_windowstatics },
    { L"Windows.UI.Xaml.Controls.Canvas",              x104_app_get_canvas },
    { L"Windows.UI.ViewManagement.StatusBar",          x104_app_get_statusbarstatics },
    { L"Windows.Phone.UI.Input.HardwareButtons",       x104_app_get_hwbuttons },
    { L"Windows.Graphics.Display.DisplayInformation",  x104_app_get_displaystatics },
    { L"Windows.Foundation.Uri",                       x104_app_get_urifactory },
    { L"Windows.ApplicationModel.Core.CoreApplication", x104_app_get_coreapp },
    { L"Windows.UI.ViewManagement.ApplicationView",    x104_app_get_viewstatics },
    { L"Windows.UI.Core.CoreWindowResizeManager",      x104_app_get_resizestatics },
    { L"Windows.UI.Xaml.Media.CompositionTarget",      x104_app_get_compositiontarget }
};

/* nothing to patch in the game image; the static events get their names here (before anything can be activated) */
static BOOL x104_app_install(BYTE *image)
{
    (void)image;
    x104_event_init(&x104_app_g.back_pressed, "HardwareButtons.BackPressed");
    x104_event_init(&x104_app_g.contents_invalidated, "DisplayInformation.DisplayContentsInvalidated");
    x104_event_init(&x104_app_g.core_suspending, "CoreApplication.Suspending");
    x104_event_init(&x104_app_g.core_resuming, "CoreApplication.Resuming");
    x104_event_init(&x104_app_g.rendering, "CompositionTarget.Rendering");
    x104_app_gs_parse();
    x104_logf("X104 app: install ok (nothing to patch; --trace-game-state=%d)", (int)x104_app_gs.enabled);
    return TRUE;
}

/* UI thread; async, input and panel are initialised.  Objects are created lazily on first activation. */
static BOOL x104_app_init(HWND window)
{
    x104_app_g.window = window;
    x104_app_g.display_w = GetSystemMetrics(X104_APP_SM_CXSCREEN);
    x104_app_g.display_h = GetSystemMetrics(X104_APP_SM_CYSCREEN);
    InterlockedExchange(&x104_app_g.initialized, 1);
    x104_logf("X104 app: init ok (window %p, monitor %ldx%ld)", (void *)window, (long)x104_app_g.display_w, (long)x104_app_g.display_h);
    return TRUE;
}

static void x104_app_shutdown(void)
{
    if (__sync_val_compare_and_swap(&x104_app_g.shut_down, 0, 1) != 0) return;
    x104_logf("X104 app: shutdown (exit requested=%d, launched=%d, exit proc ran=%d)", (int)x104.f_exit_requested, (int)x104.f_launched, (int)x104_app_g.exit_proc_ran);
}

/* WinRT activation of the eight classes this unit fakes */
static HRESULT x104_app_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    return x104_activate_from_table(x104_app_factories, (unsigned)ARRAYSIZE(x104_app_factories), class_name, iid, out);
}

/* UI thread, every loop pass (<= 10 ms; also from the WM_TIMER pump inside modal loops): the delivery point of everything the window messages only recorded (R1) */
static void x104_app_ui_tick(void)
{
    X104Element *page;
    float w = 0.0f;
    float h = 0.0f;
    if (!x104_app_g.initialized || x104_app_g.shut_down) return;

    /* safety net: the window is created hidden and shown by Window.Activate */
    if (x104.f_launched && !x104_app_g.activate_called && !x104_app_g.safety_fired && !x104.f_exit_requested &&
        (DWORD)(GetTickCount() - x104_app_g.launched_tick) >= X104_APP_ACTIVATE_GRACE_MS) {
        HWND window = x104_app_g.window ? x104_app_g.window : x104.window;
        if (!window || !IsWindowVisible(window)) {
            x104_app_g.safety_fired = 1;
            x104_logf("X104 app: ERROR Window.Activate was never called (%lu ms after OnLaunched returned): showing the window anyway", (unsigned long)(GetTickCount() - x104_app_g.launched_tick));
            x104_app_show_window("safety net");
        }
    }

    /* minimise / restore -> CoreWindow.VisibilityChanged (the game stops / restarts its render loop).  XAML-page games: once MainPage exists; others
     * (REDCON 1, handlers registered inside OnLaunched): once the window was activated */
    if ((x104_feat(X104_FEAT_XAML_PAGE) ? x104.f_content_set != 0 : x104_app_g.activate_called != 0) && !x104.f_exit_requested) {
        LONG hidden = x104_app_g.pending_hidden;
        BOOL hold = FALSE;
        if (hidden != x104_app_g.delivered_hidden && x104_game_is_ready() && !x104_app_render_loop_exists()) {
            /* ready, but the READY tick has not run StartRenderLoop yet: the handler would call StopRenderLoop (NULL deref under the lock) or start the loop before attachUi.  Retry next pass. */
            hold = TRUE;
            if (!InterlockedExchange(&x104_app_g.visibility_held_logged, 1))
                x104_logf("X104 app: visibility change (-> %d) held back: game ready but its render loop does not exist yet (AppMain+0xC == NULL); retried every pass", hidden ? 0 : 1);
        }
        if (!hold && hidden != x104_app_g.delivered_hidden) {
            InterlockedExchange(&x104_app_g.visibility_held_logged, 0);
            x104_app_g.delivered_hidden = hidden;
            x104_logf("X104 app: visibility -> %d (%s)", hidden ? 0 : 1, hidden ? "minimised / hidden: pointers released, the game stops its render loop" : "restored: the game restarts its render loop");
            x104_input_release_all_pointers("visibility");
            x104_input_raise_visibility_changed(hidden ? FALSE : TRUE);
        }
        if (x104_app_g.display_changed_pending) {
            InterlockedExchange(&x104_app_g.display_changed_pending, 0);
            x104_logf("X104 app: monitor size changed to %ldx%ld -> DisplayInformation.OrientationChanged", (long)x104_app_g.display_w, (long)x104_app_g.display_h);
            x104_display_raise_orientation_changed();
        }
    }

    /* the Page follows the client size */
    page = x104_app_g.page;
    if (page) {
        x104_app_client_size(&w, &h);
        if (w > 0.0f && h > 0.0f && (w != x104_app_g.page_w || h != x104_app_g.page_h)) {
            x104_app_g.page_w = w;
            x104_app_g.page_h = h;
            x104_element_set_sizes(page, (double)w, (double)h);
            X104_TRACE("X104 app: Page size -> %dx%d", (int)w, (int)h);
        }
    }

    /* CompositionTarget.Rendering: the game frame of games that draw on the UI thread (REDCON 1); nothing registers it in 0.10.4 */
    x104_app_rendering_tick();

    /* --trace-game-state (off by default): snapshots of the game's own state, read-only */
    if (x104_app_gs.enabled) x104_app_gs_tick();
}

/* window messages (UI thread; only after f_units_ready).  R1: nothing here calls the game - state is recorded, x104_app_ui_tick delivers it.  TRUE only when fully consumed. */
static BOOL x104_app_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result)
{
    (void)window;
    switch (message) {
    case WM_CLOSE:
        x104_logf("X104 app: WM_CLOSE -> exit requested");
        x104_app_request_exit(0);
        if (result) *result = 0;
        return TRUE;
    case X104_APP_WM_ENDSESSION:
        if (wparam) {
            x104_logf("X104 app: WM_ENDSESSION (the session is ending, lparam %08lx) -> exit requested", (unsigned long)lparam);
            x104_app_request_exit(0);
        }
        return FALSE;
    case WM_SIZE:
        if (wparam == X104_APP_SIZE_MINIMIZED) x104_app_set_pending_hidden(1, "WM_SIZE SIZE_MINIMIZED");
        else if (wparam == X104_APP_SIZE_RESTORED || wparam == X104_APP_SIZE_MAXIMIZED) x104_app_set_pending_hidden(0, "WM_SIZE restored / maximised");
        return FALSE;                                   /* a broadcast message: the panel and DefWindowProc must see it too */
    case X104_APP_WM_SHOWWINDOW:
        if (!wparam) x104_app_set_pending_hidden(1, "WM_SHOWWINDOW hide");
        else if (!IsIconic(window)) x104_app_set_pending_hidden(0, "WM_SHOWWINDOW show");
        return FALSE;
    case X104_APP_WM_DISPLAYCHANGE: {
        LONG width = (LONG)(lparam & 0xFFFF);
        LONG height = (LONG)((lparam >> 16) & 0xFFFF);
        BOOL changed = (width != x104_app_g.display_w || height != x104_app_g.display_h);
        x104_logf("X104 app: WM_DISPLAYCHANGE %ldx%ld bpp %lu%s", (long)width, (long)height, (unsigned long)wparam, changed ? " (monitor size changed: OrientationChanged follows)" : "");
        if (changed) {
            x104_app_g.display_w = width;
            x104_app_g.display_h = height;
            InterlockedExchange(&x104_app_g.display_changed_pending, 1);
        }
        return FALSE;
    }
    case X104_APP_WM_DPICHANGED:
        x104_logf("X104 app: WARNING WM_DPICHANGED dpi %lu (x) %lu (y) - NOT expected: the process is system-DPI-aware and the game is told 96 dpi / scale 1.0; the new DPI is ignored",
                  (unsigned long)(wparam & 0xFFFF), (unsigned long)((wparam >> 16) & 0xFFFF));
        return FALSE;
    default:
        return FALSE;
    }
}

/* any thread; idempotent on x104.f_exit_requested (this unit's flag).  The exit proc runs at the loop's top level (x104_ui_post) or inline on the UI thread when the queue is closed. */
static void x104_app_request_exit(int exit_code)
{
    if (__sync_val_compare_and_swap(&x104.f_exit_requested, 0, 1) != 0) {
        x104_logf("X104 app: request_exit(%d) ignored: an exit was already requested", exit_code);
        return;
    }
    x104_logf("X104 app: request_exit(%d) from thread %04lx (ui=%d, game ready=%d)", exit_code, (unsigned long)GetCurrentThreadId(), (int)x104_on_ui_thread(),
              (int)x104_game_is_ready());
    if (x104_ui_post(x104_app_exit_proc, (void *)(INT_PTR)exit_code)) return;
    if (x104_on_ui_thread()) {
        x104_logf("X104 app: the UI queue is closed: running the exit proc inline");
        x104_app_exit_proc((void *)(INT_PTR)exit_code);
    } else {
        x104_logf("X104 app: exit request ignored: loop already quitting");
    }
}

/* UI thread (called by the input unit for Esc / Alt+Left): BackPressed(sender = NULL, args = BackPressedEventArgs view) to the game's handler.  Returns the Handled the game
 * set: TRUE = the game consumed it (BACK event queued, rva 0x1DA248); FALSE = "OS default = leave the app" (the game is on its start menu) - the host does NOT exit on that. */
static BOOL x104_app_raise_back_pressed(void)
{
    X104AppBackArgs *args = x104_app_back_args();
    unsigned count;
    BOOL handled;
    if (!args) {
        x104_logf("X104 app: ERROR BackPressed: cannot create the event args (out of memory) - handled=0");
        return FALSE;
    }
    if (!x104_event_count(&x104_app_g.back_pressed)) {
        x104_logf("X104 app: BackPressed requested but no handler is registered (the MainPage constructor has not run) - handled=0");
        return FALSE;
    }
    InterlockedExchange(&args->handled, 0);
    count = x104_event_invoke_guarded(&x104_app_g.back_pressed, NULL, &args->base.views[0]);
    handled = args->handled != 0;
    x104_logf("X104 app: BackPressed handled=%d (raised to %u handler(s)%s)", (int)handled, count, handled ? "" : "; not handled: the host does not exit on this, use the window's close button / Alt+F4");
    return handled;
}

/* DisplayInformation events: sender = the DisplayInformation view (R10), args = NULL, guarded.  UI thread. */
static void x104_app_display_raise(const char *name, int which)
{
    X104AppDisplay *display = x104_app_display_object();
    X104Event *event;
    unsigned count;
    if (!display) {
        x104_logf("X104 app: ERROR DisplayInformation.%s not raised: out of memory", name);
        return;
    }
    event = which == 0 ? &display->dpi_changed : (which == 1 ? &display->orientation_changed : &x104_app_g.contents_invalidated);
    count = x104_event_invoke_guarded(event, &display->base.views[0], NULL);
    x104_logf("X104 app: DisplayInformation.%s raised to %u handler(s)", name, count);
}

static void x104_display_raise_dpi_changed(void)
{
    x104_app_display_raise("DpiChanged", 0);
}

static void x104_display_raise_orientation_changed(void)
{
    x104_app_display_raise("OrientationChanged", 1);
}

/* the game's handler is DR::ValidateDevice, which re-creates the device: the host never raises this by itself */
static void x104_display_raise_contents_invalidated(void)
{
    x104_app_display_raise("DisplayContentsInvalidated", 2);
}

#endif /* WIN32_HOST_ARM32_0104_APP_INL */
