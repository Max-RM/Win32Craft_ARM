/* win32_host_arm32_0104_app.inl - UNIT "app" of the Minecraft PE 0.10.4.40 ARM32 host.               *** STUB - TODO(app) ***
 *
 * Owner: the app implementer.  Brief: arm32/v0_10_4/units/app.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit app
 *
 * This file owns: Application (statics, factory/aggregation, inner IApplication), IActivatedEventArgs, Page (factory + inner base object with
 * FindName), Uri, Window (statics + IWindow), StatusBar, Canvas, HardwareButtons (+ BackPressedEventArgs), DisplayInformation, the lifecycle
 * (Suspending/Resuming/VisibilityChanged raising from window events) and the application exit.  The bodies below are placeholders that only
 * keep the tree compiling; replace them (keep every signature and the public function names). */
#ifndef WIN32_HOST_ARM32_0104_APP_INL
#define WIN32_HOST_ARM32_0104_APP_INL

/* TODO(app): nothing to patch in the game image; return TRUE. */
static BOOL x104_app_install(BYTE *image)
{
    (void)image;
    return TRUE;
}

/* TODO(app): UI thread, window exists, async/input/panel already initialised. */
static BOOL x104_app_init(HWND window)
{
    (void)window;
    return TRUE;
}

static void x104_app_shutdown(void)
{
}

/* TODO(app): Windows.UI.Xaml.{Application,Controls.Page,Window,Controls.Canvas}, Windows.UI.ViewManagement.StatusBar,
 * Windows.Phone.UI.Input.HardwareButtons, Windows.Graphics.Display.DisplayInformation, Windows.Foundation.Uri (x104_activate_from_table). */
static HRESULT x104_app_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    (void)class_name;
    (void)iid;
    (void)out;
    return REGDB_E_CLASSNOTREG;
}

/* TODO(app): UI thread, every loop pass (<= 10 ms): lifecycle bookkeeping (minimise/restore -> x104_input_raise_visibility_changed ...). */
static void x104_app_ui_tick(void)
{
}

/* TODO(app): WM_CLOSE (-> x104_app_request_exit), WM_SIZE minimise/restore, WM_ACTIVATE/WM_SHOWWINDOW (visibility), WM_DISPLAYCHANGE / WM_DPICHANGED (log; optional display events),
 * Alt+Left/Esc -> back is the input unit's job.
 * Return TRUE + *result when the message is fully handled.  The placeholder leaves everything to the boot's default handling. */
static BOOL x104_app_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result)
{
    (void)window;
    (void)message;
    (void)wparam;
    (void)lparam;
    (void)result;
    return FALSE;
}

/* TODO(app): any thread; graceful shutdown, idempotent on x104.f_exit_requested (the APP unit's flag; x104.f_quit belongs to the async unit):
 * post the exit proc with x104_ui_post (if that fails and this is the UI thread, run the exit proc inline); the exit proc raises Suspending ONLY when
 * x104_game_is_ready(), then x104_async_shutdown(), then x104_ui_quit(exit_code). */
static void x104_app_request_exit(int exit_code)
{
    if (__sync_val_compare_and_swap(&x104.f_exit_requested, 0, 1) != 0) return;
    x104_ui_quit(exit_code);
}

/* TODO(app): UI thread: invoke the BackPressed handler(s) the game registered with BackPressedEventArgs; TRUE when the game set Handled. */
static BOOL x104_app_raise_back_pressed(void)
{
    return FALSE;
}

/* TODO(app): UI thread: raise DisplayInformation.DpiChanged / OrientationChanged / DisplayContentsInvalidated to the registered handlers. */
static void x104_display_raise_dpi_changed(void) { }
static void x104_display_raise_orientation_changed(void) { }
static void x104_display_raise_contents_invalidated(void) { }

#endif /* WIN32_HOST_ARM32_0104_APP_INL */
