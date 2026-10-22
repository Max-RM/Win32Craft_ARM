/* win32_host_arm32_0104_input.inl - UNIT "input" of the Minecraft PE 0.10.4.40 ARM32 host.            *** STUB - TODO(input) ***
 *
 * Owner: the input implementer.  Brief: arm32/v0_10_4/units/input.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit input
 *
 * This file owns: the fake CoreWindow (+statics), CoreCursor, CoreIndependentInputSource, PointerEventArgs/PointerPoint/PointerDevice/
 * PointerPointProperties, KeyEventArgs, VisibilityChangedEventArgs, the XAML elements Logo (Image) / EditBox (TextBox) / Button, the
 * InputPane (statics, instance, event args, x104_inputpane_raise), the TabTip integration (x104_osk_* wrappers of the core) and the Win32
 * message translation x104_input_window_message.
 * The bodies below are placeholders that only keep the tree compiling; replace them (keep every signature and the public function names). */
#ifndef WIN32_HOST_ARM32_0104_INPUT_INL
#define WIN32_HOST_ARM32_0104_INPUT_INL

/* TODO(input): nothing to patch in the game image unless the facts require it (the touch-only cell 0x33FB88 stays as is). */
static BOOL x104_input_install(BYTE *image)
{
    (void)image;
    return TRUE;
}

/* TODO(input): UI thread, window exists, async unit initialised (the CoreWindow's get_Dispatcher needs the dispatcher object).  Keep win32_host.c's `text_input_active`
 * in step with the focused element through x104_set_text_input_active() (EditBox.Focus -> TRUE, Button.Focus -> FALSE, shutdown -> FALSE): the TabTip controller reads it. */
static BOOL x104_input_init(HWND window)
{
    (void)window;
    return TRUE;
}

static void x104_input_shutdown(void)
{
}

/* TODO(input): Windows.UI.ViewManagement.InputPane, Windows.UI.Core.CoreCursor, Windows.UI.Core.CoreWindow (x104_activate_from_table). */
static HRESULT x104_input_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    (void)class_name;
    (void)iid;
    (void)out;
    return REGDB_E_CLASSNOTREG;
}

/* TODO(input): UI thread, every loop pass (<= 10 ms): x104_osk_poll(). */
static void x104_input_ui_tick(void)
{
}

/* TODO(input): WM_POINTERxxx, WM_TOUCH, WM_xBUTTONxx, WM_MOUSEMOVE, WM_KEYxx, WM_SYSKEYxx (deliver, then return TRUE/0 - except Alt+F4, which must reach DefWindowProc), WM_CHAR,
 * WM_SETFOCUS, WM_KILLFOCUS, WM_CAPTURECHANGED, ESC; return TRUE + *result when consumed.  MUST be safe before x104_input_init ran (the boot gates it with f_units_ready). */
static BOOL x104_input_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result)
{
    (void)window;
    (void)message;
    (void)wparam;
    (void)lparam;
    (void)result;
    return FALSE;
}

/* TODO(input): the CoreWindow and the three XAML elements (IInspectable identity); NULL until x104_input_init. */
static X104Object *x104_input_core_window(void) { return NULL; }
static X104Object *x104_input_element_logo(void) { return NULL; }
static X104Object *x104_input_element_editbox(void) { return NULL; }
static X104Object *x104_input_element_button(void) { return NULL; }

/* TODO(input): UI thread: CoreWindow.VisibilityChanged(visible) to the registered handlers (VisibilityChangedEventArgs.get_Visible slot 6). */
static void x104_input_raise_visibility_changed(BOOL visible)
{
    (void)visible;
}

/* TODO(input): UI thread: Released for every pressed pointer (so the game never keeps a stuck contact). */
static void x104_input_release_all_pointers(const char *why)
{
    (void)why;
}

/* TODO(input): any thread (called from the input work item's pool thread by the panel's CreateCoreIndependentInputSource): create a
 * CoreIndependentInputSource (ICoreInputSourceBase + ICorePointerInputSource), AddRef'd interface for ICoreInputSourceBase in *result. */
static HRESULT x104_input_create_independent_source(UINT device_types, void **result)
{
    (void)device_types;
    if (result) *result = NULL;
    return E_NOTIMPL;
}

/* TODO(input): UI thread: invoke the InputPane Showing (TRUE) / Hiding (FALSE) handlers the game registered; OccludedRect must already be
 * published (height first).  Called by arm32_input_pane_raise() of win32_host.c when host_is_0104. */
static void x104_inputpane_raise(BOOL showing)
{
    x104_logf("X104 inputpane_raise(%s): input unit not implemented yet", showing ? "Showing" : "Hiding");
}

#endif /* WIN32_HOST_ARM32_0104_INPUT_INL */
