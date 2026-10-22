/* win32_host_arm32_0104_panel.inl - UNIT "panel" of the Minecraft PE 0.10.4.40 ARM32 host.            *** STUB - TODO(panel) ***
 *
 * Owner: the panel implementer.  Brief: arm32/v0_10_4/units/panel.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit panel
 *
 * This file owns: the fake SwapChainPanel (+ ISizeChangedEventArgs, ISwapChainPanelNative), the D3D11CreateDevice hook (levels {9_1},
 * report 9_3), the 1-byte shader profile patch, D3DCompile cwd/include handling, the half-resolution GPU-table float, presentation
 * path B (dxgi vtable data-cell patches), WM_SIZE -> panel size -> SizeChanged (ordering rules!) and device-lost handling.
 * The bodies below are placeholders that only keep the tree compiling; replace them (keep every signature and the public function names). */
#ifndef WIN32_HOST_ARM32_0104_PANEL_INL
#define WIN32_HOST_ARM32_0104_PANEL_INL

/* TODO(panel): hooks that must be live before the game creates its device: IAT 0x20B1A4 D3D11CreateDevice, profile byte 0x2351F9 (idempotent: the exe patcher already
 * writes it by default), resolution float 0xDC8854 (unless x104.opt_half_res), dxgi cell patches for x104.opt_present == X104_PRESENT_HWND (CreateSwapChainForComposition ->
 * HWND stub that records the chain it created, SetMatrixTransform -> S_OK, Present slot 8 -> x104_present_begin/_sync/_pace/_end bracket).  FALSE = fatal (message box). */
static BOOL x104_panel_install(BYTE *image)
{
    (void)image;
    return TRUE;
}

/* TODO(panel): UI thread, window exists: create the SwapChainPanel object (x104_element_new), size it from the client rect. */
static BOOL x104_panel_init(HWND window)
{
    (void)window;
    return TRUE;
}

static void x104_panel_shutdown(void)
{
}

/* The panel owns no activated WinRT class (the game gets the panel from Page.FindName). */
static HRESULT x104_panel_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    (void)class_name;
    (void)iid;
    (void)out;
    return REGDB_E_CLASSNOTREG;
}

/* TODO(panel): UI thread, every loop pass: fire the FIRST SizeChanged when HOST_DESIGN.md 4 R2 allows it. */
static void x104_panel_ui_tick(void)
{
}

/* TODO(panel): WM_SIZE / WM_ENTERSIZEMOVE / WM_EXITSIZEMOVE / WM_GETMINMAXINFO; return TRUE + *result when fully handled.  Only called after x104.f_units_ready. */
static BOOL x104_panel_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result)
{
    (void)window;
    (void)message;
    (void)wparam;
    (void)lparam;
    (void)result;
    return FALSE;
}

/* TODO(panel): the SwapChainPanel host object (IInspectable identity); NULL until x104_panel_init. */
static X104Object *x104_panel_object(void)
{
    return NULL;
}

/* TODO(panel): UI thread: deliver SizeChanged(NewSize = current client size in DIPs) to the game's handlers, ordering rules R2. */
static void x104_panel_raise_size_changed(void)
{
}

/* TODO(panel): any thread. */
static void x104_panel_client_size(float *width_dips, float *height_dips)
{
    if (width_dips) *width_dips = 0.0f;
    if (height_dips) *height_dips = 0.0f;
}

#endif /* WIN32_HOST_ARM32_0104_PANEL_INL */
