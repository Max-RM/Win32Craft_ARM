/* Host-side (x86, runs on the PC) self test of win32_host_arm32_0104_core.inl: object/view/class model, QI, holes, events, HSTRING,
 * element mixin, guarded calls, activation table.  Build + run with:  python tools/arm32/test_0104_core.py
 * The test supplies stand-ins for the few win32_host.c helpers the core uses; the real unit stubs are compiled in so that every
 * cross-unit prototype of the core is defined. */
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

/* ---- stand-ins for win32_host.c ---- */
static char g_last_log[1100];
static int g_log_lines;
static int g_seen_beyond;
static void log_line(const char *text)
{
    ++g_log_lines;
    if (strstr(text, "BEYOND")) g_seen_beyond = 1;
    lstrcpynA(g_last_log, text, sizeof(g_last_log));
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
/* win32_host.c globals/helpers the core's section 13/13b/15 uses */
static BOOL text_input_active;
static volatile LONG arm32_boot_frames_started, arm32_boot_frames_done, arm32_boot_progress, arm32_boot_loop_passes;
static DWORD arm32_boot_start_tick;
static LONG g_note_frame_calls;
static void arm32_boot_note_frame(LONG frame, DWORD loop_start, DWORD frame_ms) { (void)frame; (void)loop_start; (void)frame_ms; ++g_note_frame_calls; }

#include "win32_host_arm32_0104_core.inl"
#define x104_ui_post x104_stub_ui_post
#define x104_ui_pump_once x104_stub_ui_pump_once
#include "win32_host_arm32_0104_async.inl"
#undef x104_ui_post
#undef x104_ui_pump_once
/* harness queue standing in for the async unit's UI queue (single threaded) */
static void (*g_queue_fn[16])(void *);
static void *g_queue_arg[16];
static int g_queue_count;
static BOOL g_queue_closed;
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
#include "win32_host_arm32_0104_app.inl"
#include "win32_host_arm32_0104_panel.inl"
#include "win32_host_arm32_0104_platform.inl"
#include "win32_host_arm32_0104_input.inl"
#include "win32_host_arm32_0104_selftest.inl"

/* ---- a mock COM delegate: vtbl = { QI, AddRef, Release, Invoke } ---- */
typedef struct MockDelegate { const void **vtbl; LONG refs; LONG invoked; void *last_sender; void *last_args; HRESULT result; } MockDelegate;
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
static HRESULT WINAPI mock_invoke_crash(void *self, void *sender, void *args)
{
    (void)self; (void)sender; (void)args;
    RaiseException(0xE06D7363u, 0, 0, NULL);     /* what a C++ exception looks like to SEH */
    return S_OK;
}
static const void *mock_vtbl[4] = { mock_qi, mock_addref, mock_release, mock_invoke };
static const void *mock_vtbl_crash[4] = { mock_qi, mock_addref, mock_release, mock_invoke_crash };

/* ---- a test class with two interfaces and a deliberately incomplete vtable ---- */
typedef struct TestObject { X104Object base; int counter; } TestObject;
static int test_method_calls;
static HRESULT WINAPI test_get_dispatcher_like(void *self, int *value) { (void)self; ++test_method_calls; *value = X104_THIS(TestObject, self)->counter; return S_OK; }
X104_VTBL(vt_test_a, IApplicationStatics) = {
    X104_INSPECTABLE_SLOTS,
    [X104_SLOT_IApplicationStatics_Start] = test_get_dispatcher_like
};
X104_VTBL(vt_test_b, ICanvasStatics) = {
    X104_INSPECTABLE_SLOTS
};
static const X104IfaceDesc test_ifaces[] = { X104_IFACE(IApplicationStatics, vt_test_a), X104_IFACE(ICanvasStatics, vt_test_b) };
X104_CLASS_DEF(test_class, "TestObject", L"Test.Namespace.TestObject", test_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);

static X104Object *test_factory_singleton;
static X104Object *test_get_factory(void)
{
    if (!test_factory_singleton) test_factory_singleton = x104_object_new(&test_class, sizeof(TestObject));
    return test_factory_singleton;
}
static const X104FactoryDef test_factory_defs[] = { { L"Test.Namespace.TestObject", test_get_factory } };

typedef struct TestElement { X104Element base; } TestElement;
static HRESULT test_find_name(X104Element *element, HSTRING name, void **result)
{
    (void)element;
    if (x104_hstring_equals(name, L"Logo")) { *result = (void *)0x1234; return S_OK; }
    *result = NULL;
    return E_FAIL;
}
static const X104IfaceDesc test_element_ifaces[] = { X104_ELEMENT_IFACES };
X104_CLASS_DEF(test_element_class, "TestElement", L"Windows.UI.Xaml.Controls.Image", test_element_ifaces, X104_CLASS_AGILE, NULL);

/* the examples of HOST_DESIGN section 5 / the unit briefs, compiled for real: IUnknown-only interface + element mixin + extra interface + X104_SINGLETON */
static void *g_swapchain_seen;
static HRESULT WINAPI test_set_swap_chain(void *self, void *swapchain) { (void)self; g_swapchain_seen = swapchain; return S_OK; }
static HRESULT WINAPI test_get_scale(void *self, float *value) { (void)self; *value = 1.0f; return S_OK; }
X104_VTBL(vt_test_native, ISwapChainPanelNative) = {
    X104_UNKNOWN_SLOTS,
    [X104_SLOT_ISwapChainPanelNative_SetSwapChain] = test_set_swap_chain
};
X104_VTBL(vt_test_scp, ISwapChainPanel) = {
    X104_INSPECTABLE_SLOTS,
    [X104_SLOT_ISwapChainPanel_get_CompositionScaleX] = test_get_scale,
    [X104_SLOT_ISwapChainPanel_get_CompositionScaleY] = test_get_scale
};
static const X104IfaceDesc test_panel_ifaces[] = { X104_ELEMENT_IFACES, X104_IFACE(ISwapChainPanel, vt_test_scp), X104_IFACE(ISwapChainPanelNative, vt_test_native) };
X104_CLASS_DEF(test_panel_class, "TestPanel", L"Windows.UI.Xaml.Controls.SwapChainPanel", test_panel_ifaces, X104_CLASS_AGILE, NULL);
typedef struct TestPanel { X104Element base; int extra; } TestPanel;
X104_SINGLETON(test_panel_singleton, test_panel_class, TestPanel)
static const X104IfaceDesc test_plain_ifaces[] = { X104_IFACE(IApplicationStatics, vt_test_a) };
X104_CLASS_DEF(test_plain_class, "Plain", L"Windows.Test.Plain", test_plain_ifaces, 0, NULL);
X104_SINGLETON(test_plain_singleton, test_plain_class, X104Object)

int main(void)
{
    X104Object *obj;
    void *iface;
    void *iface2;
    HRESULT hr;
    int value;
    LONGLONG token1 = 0, token2 = 0;
    MockDelegate d1 = { mock_vtbl, 1, 0, NULL, NULL, S_OK };
    MockDelegate d2 = { mock_vtbl, 1, 0, NULL, NULL, S_OK };
    MockDelegate crash = { mock_vtbl_crash, 1, 0, NULL, NULL, S_OK };
    X104Event event;
    HSTRING hs = NULL;
    wchar_t wide[32];
    char ascii[32];

    test_print("== core self test ==\n");
    x104.image = (BYTE *)0x400000;

    /* 1. object model */
    obj = x104_object_new(&test_class, sizeof(TestObject));
    CHECK(obj != NULL);
    CHECK(obj->view_count == 2);
    CHECK(obj->refs == 1);
    ((TestObject *)obj)->counter = 42;
    CHECK(x104_object_iface0(obj) == (void *)&obj->views[0]);
    hr = x104_object_qi(obj, &X104_IID_IUnknown, &iface);
    CHECK(hr == S_OK && iface == (void *)&obj->views[0] && obj->refs == 2);
    hr = x104_object_qi(obj, &X104_IID_IInspectable, &iface2);
    CHECK(hr == S_OK && iface2 == iface);
    hr = x104_object_qi(obj, &X104_IID_ICanvasStatics, &iface2);
    CHECK(hr == S_OK && iface2 == (void *)&obj->views[1]);
    hr = x104_object_qi(obj, &X104_IID_IAgileObject, &iface2);
    CHECK(hr == S_OK && iface2 == (void *)&obj->views[0]);
    iface2 = (void *)1;
    hr = x104_object_qi(obj, &X104_IID_IWindow, &iface2);
    CHECK(hr == E_NOINTERFACE && iface2 == NULL);
    CHECK(strstr(g_last_log, "QI MISS") != NULL);
    /* through the vtable thunks, like the game does */
    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))X104_VFN(iface, 0, void *))(iface, &X104_IID_ICanvasStatics, &iface2);
    CHECK(hr == S_OK && iface2 == (void *)&obj->views[1]);
    CHECK(((ULONG (WINAPI *)(void *))X104_VFN(iface2, 1, void *))(iface2) == (ULONG)obj->refs);
    {
        LONG before = obj->refs;
        int i;
        for (i = 0; i < 20; ++i) ((ULONG (WINAPI *)(void *))X104_VFN(iface2, 2, void *))(iface2);   /* over-release */
        CHECK(obj->refs == 1 && before > 1);
        CHECK(strstr(g_last_log, "underflow") != NULL);
    }
    /* IInspectable */
    hr = ((HRESULT (WINAPI *)(void *, HSTRING *))X104_VFN(iface, 4, void *))(iface, &hs);
    CHECK(hr == S_OK && x104_hstring_equals(hs, L"Test.Namespace.TestObject"));
    x104_hstring_free(hs);
    hs = NULL;
    {
        int level = -1;
        ULONG count = 99;
        GUID *iids = (GUID *)1;
        CHECK(((HRESULT (WINAPI *)(void *, int *))X104_VFN(iface, 5, void *))(iface, &level) == S_OK && level == 0);
        CHECK(((HRESULT (WINAPI *)(void *, ULONG *, GUID **))X104_VFN(iface, 3, void *))(iface, &count, &iids) == S_OK && count == 0 && iids == NULL);
    }
    /* implemented slot works with X104_THIS */
    value = 0;
    hr = ((HRESULT (WINAPI *)(void *, int *))X104_VFN(&obj->views[0], X104_SLOT_IApplicationStatics_Start, void *))(&obj->views[0], &value);
    CHECK(hr == S_OK && value == 42 && test_method_calls == 1);

    /* 2. holes: unimplemented slots answer E_NOTIMPL and name themselves */
    g_log_lines = 0;
    hr = ((HRESULT (WINAPI *)(void *))X104_VFN(&obj->views[0], X104_SLOT_IApplicationStatics_LoadComponent, void *))(&obj->views[0]);
    CHECK(hr == (HRESULT)0x80004001);
    CHECK(g_log_lines >= 1 && strstr(g_last_log, "IApplicationStatics") != NULL);
    CHECK(strstr(g_last_log, "slot 8") != NULL || strstr(g_last_log, "interface") != NULL);
    test_print("  hole log: %s\n", g_last_log);
    /* beyond the declared interface (slack) */
    hr = ((HRESULT (WINAPI *)(void *))X104_VFN(&obj->views[0], X104_NSLOTS_IApplicationStatics + 1, void *))(&obj->views[0]);
    CHECK(hr == (HRESULT)0x80004001);
    CHECK(g_seen_beyond);
    /* a repeated hole call logs only on the 2^n-th call */
    g_log_lines = 0;
    ((HRESULT (WINAPI *)(void *))X104_VFN(&obj->views[0], X104_SLOT_IApplicationStatics_LoadComponent, void *))(&obj->views[0]);   /* 2nd: logs */
    ((HRESULT (WINAPI *)(void *))X104_VFN(&obj->views[0], X104_SLOT_IApplicationStatics_LoadComponent, void *))(&obj->views[0]);   /* 3rd: silent */
    CHECK(g_log_lines == 1);
    /* hole called with something that is not a host view must not crash */
    hr = ((HRESULT (WINAPI *)(void *))x104_hole_5)(NULL);
    CHECK(hr == (HRESULT)0x80004001);

    /* 3. events */
    x104_event_init(&event, "Test.Event");
    CHECK(x104_event_add(&event, &d1, &token1) == S_OK && token1 != 0 && d1.refs == 2);
    CHECK(x104_event_add(&event, &d2, &token2) == S_OK && token2 != 0 && token2 != token1);
    CHECK(x104_event_count(&event) == 2);
    CHECK(x104_event_invoke(&event, (void *)0xAA, (void *)0xBB) == 2);
    CHECK(d1.invoked == 1 && d2.invoked == 1 && d1.last_sender == (void *)0xAA && d1.last_args == (void *)0xBB);
    CHECK(d1.refs == 2);          /* snapshot AddRef/Release balanced */
    CHECK(x104_event_remove(&event, token1) == S_OK && d1.refs == 1 && x104_event_count(&event) == 1);
    CHECK(x104_event_invoke(&event, NULL, NULL) == 1 && d1.invoked == 1 && d2.invoked == 2);
    CHECK(x104_event_remove(&event, 12345) == S_OK);
    CHECK(x104_event_add(&event, NULL, &token1) == E_INVALIDARG);
    d2.result = (HRESULT)0x80004005;
    g_log_lines = 0;
    x104_event_invoke(&event, NULL, NULL);
    CHECK(g_log_lines == 1 && strstr(g_last_log, "0x80004005") != NULL);
    {
        MockDelegate extra[X104_EVENT_MAX + 2];
        int i;
        LONGLONG t;
        int accepted = 0;
        for (i = 0; i < X104_EVENT_MAX + 2; ++i) { extra[i].vtbl = mock_vtbl; extra[i].refs = 1; extra[i].invoked = 0; extra[i].result = S_OK; }
        for (i = 0; i < X104_EVENT_MAX + 2; ++i) if (x104_event_add(&event, &extra[i], &t) == S_OK) ++accepted;
        CHECK(accepted == X104_EVENT_MAX - 1);       /* d2 occupies one slot */
        CHECK(extra[X104_EVENT_MAX + 1].refs == 1);   /* a refused add leaves the refcount untouched */
    }

    /* 4. guarded calls */
    {
        HRESULT ghr = S_OK;
        X104Seh seh;
        BOOL clean = x104_call_guarded3(((void **)mock_vtbl)[3], &d2, (void *)1, (void *)2, &ghr, &seh);
        CHECK(clean && ghr == (HRESULT)0x80004005);
        clean = x104_call_guarded3(((void **)mock_vtbl_crash)[3], &crash, NULL, NULL, &ghr, &seh);
        CHECK(!clean && seh.code == 0xE06D7363u && ghr == E_FAIL);
        ghr = x104_invoke_delegate_guarded(&crash, NULL, NULL, "test delegate");
        CHECK(ghr == E_FAIL && strstr(g_last_log, "C++ exception") != NULL);
        d2.result = S_OK;
        CHECK(x104_invoke_delegate_guarded(&d2, NULL, NULL, "ok delegate") == S_OK);
    }

    /* 4b. guarded event invoke + helper getters + singleton */
    {
        X104Event guarded;
        MockDelegate ok = { mock_vtbl, 1, 0, NULL, NULL, S_OK };
        LONGLONG t1 = 0, t2 = 0;
        void *out = NULL;
        X104Object *single1;
        x104_event_init(&guarded, "Test.Guarded");
        CHECK(x104_event_add(&guarded, &crash, &t1) == S_OK && x104_event_add(&guarded, &ok, &t2) == S_OK);
        CHECK(x104_event_invoke_guarded(&guarded, (void *)7, (void *)8) == 2);
        CHECK(ok.invoked == 1 && ok.last_sender == (void *)7 && ok.last_args == (void *)8);
        CHECK(x104_return_iface(NULL, &X104_IID_IUnknown, &out, "nothing") == E_FAIL && out == NULL);
        CHECK(x104_return_iface(test_get_factory(), &X104_IID_ICanvasStatics, &out, "factory") == S_OK && out != NULL);
        single1 = test_get_factory();
        CHECK(single1 == test_get_factory());
    }

    /* 4c. documented examples */
    {
        X104Object *panel = test_panel_singleton();
        void *native = NULL, *scp = NULL, *fe = NULL, *agile = NULL;
        float scale = 0;
        CHECK(panel != NULL && panel == test_panel_singleton() && panel->view_count == 5);
        CHECK(x104_object_qi(panel, &X104_IID_ISwapChainPanelNative, &native) == S_OK);
        CHECK(((HRESULT (WINAPI *)(void *, void *))X104_VFN(native, X104_SLOT_ISwapChainPanelNative_SetSwapChain, void *))(native, (void *)0x55) == S_OK && g_swapchain_seen == (void *)0x55);
        CHECK(x104_object_qi(panel, &X104_IID_ISwapChainPanel, &scp) == S_OK);
        CHECK(((HRESULT (WINAPI *)(void *, float *))X104_VFN(scp, X104_SLOT_ISwapChainPanel_get_CompositionScaleY, void *))(scp, &scale) == S_OK && scale == 1.0f);
        CHECK(x104_object_qi(panel, &X104_IID_IFrameworkElement, &fe) == S_OK);
        CHECK(x104_object_qi(panel, &X104_IID_IAgileObject, &agile) == S_OK && agile == (void *)&panel->views[0]);
        /* through ANY view the identity QI returns views[0] */
        CHECK(x104_qi(scp, &X104_IID_IUnknown, &agile) == S_OK && agile == (void *)&panel->views[0]);
        /* the unknown-only interface still has hole stubs behind SetSwapChain */
        g_log_lines = 0;
        CHECK(((HRESULT (WINAPI *)(void *))X104_VFN(native, 5, void *))(native) == (HRESULT)0x80004001 && g_log_lines >= 1);
        CHECK(test_plain_singleton()->view_count == 1 && (test_plain_singleton()->cls->flags & X104_CLASS_AGILE) == 0);
        { void *o = NULL; CHECK(x104_object_qi(test_plain_singleton(), &X104_IID_IAgileObject, &o) == E_NOINTERFACE); }
    }

    /* 5. HSTRING helpers */
    CHECK(x104_hstring_create(L"Hello", &hs) == S_OK && hs != NULL);
    CHECK(x104_hstring_equals(hs, L"Hello") && !x104_hstring_equals(hs, L"Hell") && !x104_hstring_equals(hs, L"Hello!"));
    CHECK(x104_hstring_wide(hs, wide, 32) == 5 && wide[0] == 'H' && wide[5] == 0);
    CHECK(x104_hstring_wide(hs, wide, 3) == 5 && wide[2] == 0 && wide[1] == 'e');
    x104_hstring_ascii(hs, ascii, sizeof(ascii));
    CHECK(strcmp(ascii, "Hello") == 0);
    x104_hstring_free(hs);
    CHECK(x104_hstring_create(L"", &hs) == S_OK && hs == NULL && x104_hstring_equals(NULL, L""));

    /* 6. GUID / name helpers */
    {
        char text[96];
        x104_describe_iid(text, &X104_IID_IFrameworkElement);
        CHECK(strstr(text, "IFrameworkElement") != NULL && strstr(text, "A391D09B-4A99-4B7C-9D8D-6FA5D01F6FBF") != NULL);
    }

    /* 7. activation through the table */
    {
        void *out = NULL;
        hr = x104_activate_from_table(test_factory_defs, 1, L"Test.Namespace.TestObject", &X104_IID_ICanvasStatics, &out);
        CHECK(hr == S_OK && out != NULL);
        hr = x104_activate_from_table(test_factory_defs, 1, L"Test.Namespace.TestObject", &X104_IID_IWindow, &out);
        CHECK(hr == E_NOINTERFACE);
        hr = x104_activate_from_table(test_factory_defs, 1, L"Nope", &X104_IID_IWindow, &out);
        CHECK(hr == REGDB_E_CLASSNOTREG);
        /* the core router: a class in the owner table whose unit is a stub falls back with the "not implemented yet" note */
        hr = x104_activation_override(L"Windows.UI.Xaml.Application", &X104_IID_IApplicationStatics, &out);
        CHECK(hr == REGDB_E_CLASSNOTREG && strstr(g_last_log, "owned by the unit") != NULL);
        hr = x104_activation_override(L"Windows.Some.Unknown.Class", &X104_IID_IApplicationStatics, &out);
        CHECK(hr == REGDB_E_CLASSNOTREG);
    }

    /* 8. element mixin */
    {
        TestElement *element = (TestElement *)x104_element_new(&test_element_class, sizeof(TestElement), "Image");
        double w = 0, h = 0;
        float size[2] = {0, 0};
        int visibility = -1;
        HSTRING name = NULL;
        void *result = NULL;
        void *fe = NULL, *ui = NULL, *dep = NULL;
        CHECK(element != NULL && element->base.base.view_count == 3);
        x104_element_set_sizes(&element->base, 1280.0, 720.0);
        CHECK(x104_object_qi(&element->base.base, &X104_IID_IFrameworkElement, &fe) == S_OK);
        CHECK(x104_object_qi(&element->base.base, &X104_IID_IUIElement, &ui) == S_OK);
        CHECK(x104_object_qi(&element->base.base, &X104_IID_IDependencyObject, &dep) == S_OK);
        CHECK(((HRESULT (WINAPI *)(void *, double *))X104_VFN(fe, X104_SLOT_IFrameworkElement_get_ActualWidth, void *))(fe, &w) == S_OK && w == 1280.0);
        CHECK(((HRESULT (WINAPI *)(void *, double *))X104_VFN(fe, X104_SLOT_IFrameworkElement_get_ActualHeight, void *))(fe, &h) == S_OK && h == 720.0);
        CHECK(((HRESULT (WINAPI *)(void *, float *))X104_VFN(ui, X104_SLOT_IUIElement_get_RenderSize, void *))(ui, size) == S_OK && size[0] == 1280.0f && size[1] == 720.0f);
        CHECK(((HRESULT (WINAPI *)(void *, int))X104_VFN(ui, X104_SLOT_IUIElement_put_Visibility, void *))(ui, 1) == S_OK);
        CHECK(((HRESULT (WINAPI *)(void *, int *))X104_VFN(ui, X104_SLOT_IUIElement_get_Visibility, void *))(ui, &visibility) == S_OK && visibility == 1);
        token1 = 0;
        CHECK(((HRESULT (WINAPI *)(void *, void *, LONGLONG *))X104_VFN(fe, X104_SLOT_IFrameworkElement_add_SizeChanged, void *))(fe, &d1, &token1) == S_OK && token1);
        CHECK(x104_event_invoke(&element->base.size_changed, fe, NULL) == 1);
        /* remove_SizeChanged takes the 64-bit token by value: on ARM it is passed in r2:r3 - the compiler handles it for us here */
        CHECK(((HRESULT (WINAPI *)(void *, LONGLONG))X104_VFN(fe, X104_SLOT_IFrameworkElement_remove_SizeChanged, void *))(fe, token1) == S_OK);
        CHECK(x104_event_count(&element->base.size_changed) == 0);
        /* FindName without callback fails loudly, with callback works */
        x104_hstring_create(L"Logo", &name);
        CHECK(((HRESULT (WINAPI *)(void *, HSTRING, void **))X104_VFN(fe, X104_SLOT_IFrameworkElement_FindName, void *))(fe, name, &result) == E_FAIL);
        element->base.find_name = test_find_name;
        CHECK(((HRESULT (WINAPI *)(void *, HSTRING, void **))X104_VFN(fe, X104_SLOT_IFrameworkElement_FindName, void *))(fe, name, &result) == S_OK && result == (void *)0x1234);
        x104_hstring_free(name);
        /* get_Dispatcher with the async stub (no dispatcher yet) fails loudly */
        CHECK(((HRESULT (WINAPI *)(void *, void **))X104_VFN(dep, X104_SLOT_IDependencyObject_get_Dispatcher, void *))(dep, &result) == E_FAIL);
        CHECK(strstr(g_last_log, "no dispatcher") != NULL);
        /* an unimplemented IUIElement slot (Opacity setter) is a named hole */
        g_log_lines = 0;
        CHECK(((HRESULT (WINAPI *)(void *))X104_VFN(ui, X104_SLOT_IUIElement_put_Opacity, void *))(ui) == (HRESULT)0x80004001);
        CHECK(strstr(g_last_log, "IUIElement") != NULL);
    }

    /* 9. completed IAsyncAction */
    {
        void *action = NULL, *info = NULL;
        int status = -1;
        UINT32 id = 0;
        CHECK(x104_object_qi(x104_completed_action(), &X104_IID_IAsyncAction, &action) == S_OK);
        CHECK(x104_object_qi(x104_completed_action(), &X104_IID_IAsyncInfo, &info) == S_OK);
        CHECK(((HRESULT (WINAPI *)(void *, int *))X104_VFN(info, X104_SLOT_IAsyncInfo_get_Status, void *))(info, &status) == S_OK && status == X104_ASYNC_COMPLETED);
        CHECK(((HRESULT (WINAPI *)(void *, UINT32 *))X104_VFN(info, X104_SLOT_IAsyncInfo_get_Id, void *))(info, &id) == S_OK && id == 1);
        CHECK(((HRESULT (WINAPI *)(void *))X104_VFN(info, X104_SLOT_IAsyncInfo_Cancel, void *))(info) == S_OK);
        CHECK(((HRESULT (WINAPI *)(void *))X104_VFN(action, X104_SLOT_IAsyncAction_GetResults, void *))(action) == S_OK);
        CHECK(x104_completed_action() == x104_completed_action());
    }

    /* 10. game accessors without a game */
    CHECK(x104_game_mainpage() == NULL && x104_game_appmain() == NULL && x104_game_client() == NULL && !x104_game_client_has_holder() && !x104_game_is_ready());
    {
        /* fake game memory: MainPage at +0 with outer at +8; AppMain; client */
        static BYTE page[0x100] __attribute__((aligned(8)));
        static BYTE app[0x40] __attribute__((aligned(8)));
        static BYTE client[0x170] __attribute__((aligned(8)));
        x104.mainpage_outer = page + 8;
        *(DWORD *)(page + X104_MAINPAGE_APPMAIN) = (DWORD)(ULONG_PTR)app;
        *(DWORD *)(app + X104_APPMAIN_CLIENT) = (DWORD)(ULONG_PTR)client;
        CHECK(x104_game_mainpage() == page && x104_game_appmain() == app && x104_game_client() == client);
        CHECK(!x104_game_client_has_holder() && !x104_game_is_ready());
        *(DWORD *)(client + X104_CLIENT_HOLDER) = 0x1000;
        app[X104_APPMAIN_READY] = 1;
        CHECK(x104_game_client_has_holder() && x104_game_is_ready());
        x104.mainpage_outer = NULL;
    }

    /* 11. flags */
    {
        volatile LONG flag = 0;
        g_log_lines = 0;
        x104_flag_set(&flag, "test flag");
        x104_flag_set(&flag, "test flag");
        CHECK(flag == 1 && g_log_lines == 1);
    }

    /* 12. once */
    {
        static volatile LONG once;
        int runs = 0;
        if (x104_once_begin(&once)) { ++runs; x104_once_end(&once); }
        if (x104_once_begin(&once)) { ++runs; x104_once_end(&once); }
        CHECK(runs == 1);
    }


    /* 13. review fixes: aggregation helper, late Completed handler, text_input_active, Present accounting, X104_SLOT/NOARGS, generated typedefs */
    {
        MockDelegate outer = { mock_vtbl, 1, 0, NULL, NULL, S_OK };
        void *inner = NULL, *instance = NULL, *again = NULL;
        X104Object *created = NULL;
        HRESULT ahr = x104_aggregate_create(&test_class, sizeof(TestObject), &outer, &X104_IID_ICanvasStatics, &inner, &instance, &created, "test factory");
        CHECK(ahr == S_OK && created != NULL && inner == (void *)&created->views[0] && instance == (void *)&created->views[1]);
        CHECK(outer.refs == 2 && created->outer == &outer && created->refs == 3);          /* outer AddRef'd ONCE; host ref + inner + instance */
        CHECK(strstr(g_last_log, "outer AddRef'd once") != NULL);
        ahr = x104_aggregate_create(&test_class, sizeof(TestObject), &outer, NULL, &inner, NULL, NULL, "default iface");   /* iid NULL = views[0], instance may be NULL */
        CHECK(ahr == S_OK && outer.refs == 3 && inner != NULL);
        ahr = x104_aggregate_create(&test_class, sizeof(TestObject), &outer, &X104_IID_IWindow, &inner, &again, &created, "wrong iface");
        CHECK(ahr == E_NOINTERFACE && inner == NULL && again == NULL && created == NULL && outer.refs == 3);
        CHECK(strstr(g_last_log, "does not implement the instance interface") != NULL);
    }
    {
        /* a Completed handler attached to a finished action is invoked asynchronously (never inline), with the action and status Completed */
        MockDelegate handler = { mock_vtbl, 1, 0, NULL, NULL, S_OK };
        void *action = NULL;
        CHECK(x104_object_qi(x104_completed_action(), &X104_IID_IAsyncAction, &action) == S_OK);
        CHECK(((HRESULT (WINAPI *)(void *, void *))X104_VFN(action, X104_SLOT_IAsyncAction_put_Completed, void *))(action, &handler) == S_OK);
        CHECK(handler.invoked == 0 && handler.refs == 2 && g_queue_count == 1);
        x104_ui_pump_once();
        CHECK(handler.invoked == 1 && handler.last_args == (void *)(ULONG_PTR)X104_ASYNC_COMPLETED && handler.last_sender == x104_object_iface0(x104_completed_action()) && handler.refs == 1);
        g_queue_closed = TRUE;                                                                  /* after x104_ui_quit the queue refuses: no leak, no inline call */
        CHECK(((HRESULT (WINAPI *)(void *, void *))X104_VFN(action, X104_SLOT_IAsyncAction_put_Completed, void *))(action, &handler) == S_OK);
        CHECK(handler.invoked == 1 && handler.refs == 1 && strstr(g_last_log, "queue is closed") != NULL);
        g_queue_closed = FALSE;
        CHECK(((HRESULT (WINAPI *)(void *, void *))X104_VFN(action, X104_SLOT_IAsyncAction_put_Completed, void *))(action, NULL) == S_OK && g_queue_count == 0);
    }
    {
        g_log_lines = 0;
        text_input_active = FALSE;
        x104_set_text_input_active(TRUE, "test focus");
        x104_set_text_input_active(TRUE, "test focus again");
        CHECK(text_input_active && g_log_lines == 1 && strstr(g_last_log, "0 -> 1") != NULL);
        x104_set_text_input_active(FALSE, "test blur");
        CHECK(!text_input_active && strstr(g_last_log, "1 -> 0") != NULL);
    }
    {
        /* Present accounting: first 20 logged, frames counted, a failure and DXGI_STATUS_OCCLUDED are reported */
        LONG before = arm32_boot_frames_done;
        x104.opt_vsync = TRUE;
        CHECK(x104_present_sync(0) == 1 && x104_present_sync(1) == 1);
        x104.opt_vsync = FALSE;
        CHECK(x104_present_sync(0) == 0);
        x104_present_begin();
        x104_present_pace();
        x104_present_end(S_OK, 0, 0);
        CHECK(arm32_boot_frames_started == 1 && arm32_boot_frames_done == before + 1 && g_note_frame_calls == 1 && strstr(g_last_log, "FIRST Present") != NULL);
        x104_present_end((HRESULT)0x887A0005L, 0, 0);
        CHECK(strstr(g_last_log, "DEVICE LOST") != NULL);
        x104_present_end((HRESULT)0x087A0001L, 0, 0);
        CHECK(strstr(g_last_log, "OCCLUDED") != NULL);
        x104.opt_max_fps = 100;                      /* explicit cap: 10 ms between two paces */
        {
            DWORD t0 = GetTickCount();
            x104_present_pace();
            x104_present_pace();
            CHECK(GetTickCount() - t0 >= 8);
        }
        x104.opt_max_fps = 0;
    }
    {
        /* the pass is a no-op before f_units_ready */
        x104.f_units_ready = 0;
        x104_ui_pass();
        CHECK(arm32_boot_progress == 0);
        x104.f_units_ready = 1;
        x104_ui_pass();
        CHECK(arm32_boot_progress == 1 && arm32_boot_loop_passes == 1);
        x104.f_units_ready = 0;
    }
    {
        /* the checked slot macros put exactly the thunk into the vtable */
        CHECK(sizeof(X104_FN_ICanvasStatics_SetLeft) == sizeof(void *));
        CHECK(x104_vt_done_info[X104_SLOT_IAsyncInfo_get_Status] == (const void *)x104_done_get_status);
        CHECK(x104_vt_done_action[X104_SLOT_IAsyncAction_GetResults] == (const void *)x104_done_get_results);
    }
    {
        /* the on-device selftest runs against the stub units: core checks pass, everything unit-owned is a SKIP, nothing FAILS */
        int failures = x104_selftest_fakes();
        CHECK(failures == 0 && x104_st_pass >= 6 && x104_st_skip >= 8);
        test_print("  selftest against the stubs: %d pass, %d fail, %d skip\n", x104_st_pass, x104_st_fail, x104_st_skip);
    }

    test_print("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

#if defined(SLOT_TEST_NEG_ARGS)       /* wrong parameter type: LONG instead of LONGLONG (TimeSpan) -> must NOT compile */
static HRESULT WINAPI slot_test_put_interval(void *self, LONG value) { (void)self; (void)value; return S_OK; }
X104_VTBL(slot_test_vt, IDispatcherTimer) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IDispatcherTimer, put_Interval, slot_test_put_interval) };
#elif defined(SLOT_TEST_NEG_FLOAT)    /* float where the typedef says double (Canvas.SetLeft length) -> must NOT compile */
static HRESULT WINAPI slot_test_set_left(void *self, void *element, float length) { (void)self; (void)element; (void)length; return S_OK; }
X104_VTBL(slot_test_vt, ICanvasStatics) = { X104_INSPECTABLE_SLOTS, X104_SLOT(ICanvasStatics, SetLeft, slot_test_set_left) };
#elif defined(SLOT_TEST_NEG_NOARGS)   /* NOARGS with a thunk that takes an argument -> must NOT compile */
static HRESULT WINAPI slot_test_start(void *self, void *callback) { (void)self; (void)callback; return S_OK; }
X104_VTBL(slot_test_vt, IApplicationStatics) = { X104_INSPECTABLE_SLOTS, X104_SLOT_NOARGS(IApplicationStatics, Start, slot_test_start) };
#elif defined(SLOT_TEST_NEG_NOTYPEDEF)  /* a slot without a typedef (struct by value other than Size/Point/Rect) -> must NOT compile */
static HRESULT WINAPI slot_test_margin(void *self, int value) { (void)self; (void)value; return S_OK; }
X104_VTBL(slot_test_vt, IFrameworkElement) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IFrameworkElement, put_Margin, slot_test_margin) };
#elif defined(SLOT_TEST_POS)          /* the right signatures, including a double, a LONGLONG, a struct by value and NOARGS -> must compile */
static HRESULT WINAPI slot_test_put_interval(void *self, LONGLONG value) { (void)self; (void)value; return S_OK; }
static HRESULT WINAPI slot_test_set_left(void *self, void *element, double length) { (void)self; (void)element; (void)length; return S_OK; }
static HRESULT WINAPI slot_test_measure(void *self, X104Size size) { (void)self; (void)size; return S_OK; }
X104_VTBL(slot_test_vt1, IDispatcherTimer) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IDispatcherTimer, put_Interval, slot_test_put_interval), X104_SLOT_NOARGS(IDispatcherTimer, Start, x104_nop) };
X104_VTBL(slot_test_vt2, ICanvasStatics) = { X104_INSPECTABLE_SLOTS, X104_SLOT(ICanvasStatics, SetLeft, slot_test_set_left) };
X104_VTBL(slot_test_vt3, IUIElement) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IUIElement, Measure, slot_test_measure) };
#endif
