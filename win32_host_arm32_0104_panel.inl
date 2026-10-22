/* win32_host_arm32_0104_panel.inl - UNIT "panel" of the Minecraft PE 0.10.4.40 ARM32 host.
 *
 * Owner: the panel implementer.  Brief: arm32/v0_10_4/units/panel.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit panel
 *
 * This file owns everything graphics-adjacent (the game owns the device, the swap chain, the RTV/DSV and Present(0,0); the host never presents and never clears):
 *   1  the fake Windows.UI.Xaml.Controls.SwapChainPanel (+ ISizeChangedEventArgs, ISwapChainPanelNative)
 *   2  the D3D11CreateDevice IAT hook (feature level 9_1 requested, 9_3 reported), device diagnostics, CreateBuffer / CreateTexture2D failure logging, device-loss poll
 *   3  the shader profile string ("_level_9_3" -> "_level_9_1"), the half-resolution GPU-table float, cwd + include check
 *   4  presentation path B (default): dxgi vtable DATA-cell patches (CreateSwapChainForComposition -> CreateSwapChainForHwnd on the game window, SetMatrixTransform -> S_OK,
 *      IDXGISwapChain::Present -> frame accounting bracket); SetSwapChain is the self-check; path A (DirectComposition) as fallback / option
 *   5  the window size -> SizeChanged pipeline (first SizeChanged under the ordering rules R2/R3, coalesced live resize), WM_SIZE / WM_GETMINMAXINFO.
 * Threading: x104_panel_ui_tick / x104_panel_window_message / x104_panel_raise_size_changed / the SwapChainPanel thunks (except CreateCoreIndependentInputSource, a pool
 * thread) run on the UI thread; the D3D11 hook runs on the UI thread (MainPage constructor) and may re-enter on the render thread (HandleDeviceLost); the Present stub and
 * the CreateBuffer / CreateTexture2D wrappers run on game threads.  Every table below is published entry-complete before the vtable cell that uses it is patched. */
#ifndef WIN32_HOST_ARM32_0104_PANEL_INL
#define WIN32_HOST_ARM32_0104_PANEL_INL

extern BOOL WINAPI SetCurrentDirectoryW(LPCWSTR);
extern BOOL WINAPI TerminateProcess(HANDLE, UINT);

/* ======================================================================================================================
 * 1. types, constants, state
 * ====================================================================================================================== */
#define X104_PANEL_MAX_VT          4          /* distinct vtables remembered per kind (factory / swap chain / device) */
#define X104_PANEL_DXGI_FACTORY_CREATE_SWAPCHAIN_FOR_HWND         15
#define X104_PANEL_DXGI_FACTORY_MAKE_WINDOW_ASSOCIATION            8
#define X104_PANEL_DXGI_FACTORY_CREATE_SWAPCHAIN_FOR_COMPOSITION  24
#define X104_PANEL_DXGI_SWAPCHAIN_PRESENT                          8
#define X104_PANEL_DXGI_SWAPCHAIN2_SET_MATRIX_TRANSFORM           34
#define X104_PANEL_D3D11_DEVICE_CREATE_BUFFER                      3
#define X104_PANEL_D3D11_DEVICE_CREATE_TEXTURE2D                   5
#define X104_PANEL_D3D11_DEVICE_CHECK_FEATURE_SUPPORT             33
#define X104_PANEL_D3D11_DEVICE_GET_DEVICE_REMOVED_REASON         39
#define X104_PANEL_LOSS_POLL_MS    500u
#define X104_PANEL_MIN_TRACK_W     320
#define X104_PANEL_MIN_TRACK_H     240

typedef struct x104_panel_swap_desc1_t {               /* DXGI_SWAP_CHAIN_DESC1 (48 bytes) */
    UINT Width, Height, Format;
    BOOL Stereo;
    UINT SampleCount, SampleQuality;
    UINT BufferUsage, BufferCount, Scaling, SwapEffect, AlphaMode, Flags;
} x104_panel_swap_desc1_t;

typedef struct x104_panel_buffer_desc_t {              /* D3D11_BUFFER_DESC */
    UINT ByteWidth, Usage, BindFlags, CPUAccessFlags, MiscFlags, StructureByteStride;
} x104_panel_buffer_desc_t;

typedef struct x104_panel_tex2d_desc_t {               /* D3D11_TEXTURE2D_DESC */
    UINT Width, Height, MipLevels, ArraySize, Format, SampleCount, SampleQuality, Usage, BindFlags, CPUAccessFlags, MiscFlags;
} x104_panel_tex2d_desc_t;

typedef struct x104_panel_adapter_desc_t {             /* DXGI_ADAPTER_DESC */
    WCHAR Description[128];
    UINT VendorId, DeviceId, SubSysId, Revision;
    SIZE_T DedicatedVideoMemory, DedicatedSystemMemory, SharedSystemMemory;
    BYTE AdapterLuid[8];
} x104_panel_adapter_desc_t;

typedef HRESULT (X104_API *x104_panel_create_device_fn)(void *adapter, UINT driver_type, HMODULE software, UINT flags, const UINT *levels, UINT level_count,
                                                       UINT sdk_version, void **device, UINT *selected_level, void **context);
typedef HRESULT (X104_API *x104_panel_create_hwnd_chain_fn)(void *factory, void *device, HWND window, const x104_panel_swap_desc1_t *desc,
                                                           const void *fullscreen_desc, void *restrict_to_output, void **swapchain);
typedef HRESULT (X104_API *x104_panel_create_comp_chain_fn)(void *factory, void *device, const x104_panel_swap_desc1_t *desc, void *restrict_to_output,
                                                           void **swapchain);
typedef HRESULT (X104_API *x104_panel_present_fn)(void *self, UINT sync, UINT flags);
typedef HRESULT (X104_API *x104_panel_create_buffer_fn)(void *self, const x104_panel_buffer_desc_t *desc, const void *initial, void **buffer);
typedef HRESULT (X104_API *x104_panel_create_texture2d_fn)(void *self, const x104_panel_tex2d_desc_t *desc, const void *initial, void **texture);
typedef HRESULT (X104_API *x104_panel_dcomp_create_fn)(void *dxgi_device, const GUID *iid, void **device);
typedef HRESULT (X104_API *x104_panel_create_factory1_fn)(const GUID *iid, void **factory);

static const GUID x104_panel_iid_dxgi_factory1  = {0x770aae78, 0xf26f, 0x4dba, {0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87}};
static const GUID x104_panel_iid_dxgi_factory2  = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
static const GUID x104_panel_iid_dxgi_swapchain = {0x310d36a0, 0xd2e7, 0x4c0a, {0xaa, 0x04, 0x6a, 0x9d, 0x23, 0xb8, 0x88, 0x6a}};
static const GUID x104_panel_iid_dxgi_swapchain2 = {0xa8be2ac4, 0x199f, 0x4946, {0xb3, 0x31, 0x79, 0x59, 0x9f, 0xb9, 0x8d, 0xe7}};
static const GUID x104_panel_iid_d3d11_device2  = {0x9d06dffa, 0xd1e5, 0x4d07, {0x83, 0xa8, 0x1b, 0xb1, 0x23, 0xf2, 0xf8, 0x41}};
static const GUID x104_panel_iid_dcomp_device   = {0xC37EA93A, 0xE7AA, 0x450D, {0xB1, 0x6F, 0x97, 0x46, 0xCB, 0x04, 0x07, 0xF3}};

typedef struct x104_panel_factory_vt_t { void **vtbl; void *orig_for_hwnd; void *orig_for_composition; } x104_panel_factory_vt_t;
typedef struct x104_panel_chain_vt_t   { void **vtbl; void *orig_present; void *orig_set_matrix; } x104_panel_chain_vt_t;
typedef struct x104_panel_device_vt_t  { void **vtbl; void *orig_create_buffer; void *orig_create_texture2d; } x104_panel_device_vt_t;

/* the SwapChainPanel host object (views: IDependencyObject [identity], IUIElement, IFrameworkElement, ISwapChainPanel, ISwapChainPanelNative) */
typedef struct x104_panel_object_t {
    X104Element base;
    X104Event scale_changed;           /* ISwapChainPanel.CompositionScaleChanged (never raised) */
    float scale_x, scale_y;            /* 1.0f */
    void *native_chain;                /* the last chain handed to SetSwapChain (AddRef'd) */
} x104_panel_object_t;

/* the SizeChangedEventArgs host object (views: ISizeChangedEventArgs) - one per panel, fields set before every raise (the handler runs synchronously) */
typedef struct x104_panel_args_t {
    X104Object base;
    X104Size previous, current;
} x104_panel_args_t;

typedef struct x104_panel_state_t {
    /* objects */
    x104_panel_object_t *panel;
    x104_panel_args_t *args;
    HWND window;
    /* size pipeline */
    volatile LONG client_w, client_h;          /* pixels == DIPs; written by WM_SIZE (UI thread) and the tick, read from any thread */
    volatile LONG pending_resize;
    volatile LONG minimized;
    volatile LONG in_size_move;
    volatile LONG shutdown;
    BOOL first_done;
    DWORD last_change_tick;                    /* last WM_SIZE that changed the size */
    DWORD last_raise_tick;
    DWORD first_wait_log_tick;
    DWORD deferred_log_tick;
    DWORD size_done_tick;                      /* when the first SizeChanged returned */
    DWORD loss_poll_tick;
    int raised_w, raised_h;                    /* size delivered by the last raise (== previous size of the next one) */
    LONG raise_count;
    BOOL rule_logged;                          /* the 3 s rule line was written */
    /* presentation */
    int path;                                  /* effective path: X104_PRESENT_HWND / X104_PRESENT_DCOMP */
    BOOL fallback_a;                           /* path B was selected but SetSwapChain found a chain the stub did not create */
    X104Lock chain_lock;
    void *stub_chain;                          /* AddRef'd IDXGISwapChain1* the HWND stub returned to the game (last one) */
    void *stub_identity;                       /* its IDXGISwapChain identity pointer (valid while stub_chain is held) */
    BOOL created_by_stub;
    char stub_attempt;                         /* 'a'..'d' (HWND) or 'z' (composition fallback of the stub) */
    HRESULT stub_last_hr;
    void **game_factory_vtbl;                  /* vtable of the factory behind the game's device (diagnostic) */
    void **patched_factory_vtbl;               /* first factory vtable patched */
    volatile LONG factory_count, chain_count, device_vt_count;
    x104_panel_factory_vt_t factories[X104_PANEL_MAX_VT];
    x104_panel_chain_vt_t chains[X104_PANEL_MAX_VT];
    x104_panel_device_vt_t devices[X104_PANEL_MAX_VT];
    volatile LONG setswapchain_calls;
    /* D3D device */
    UINT cap;                                  /* feature level cap in force (arm32_feature_level_cap at install) */
    void *device;                              /* AddRef'd ID3D11Device of the last creation */
    X104Lock device_lock;
    volatile LONG device_creations;
    volatile LONG lost_reported;
    BOOL diag_logged;
    UINT actual_level, reported_level, driver_type;
    /* path A */
    void *dcomp_device, *dcomp_target, *dcomp_visual;
    BOOL dcomp_failed;
} x104_panel_state_t;

static x104_panel_state_t x104_panel_state;
static void *x104_panel_orig_create_device;
static volatile LONG x104_panel_msg_busy;
static char x104_panel_msg_text[768];

/* ======================================================================================================================
 * 2. small helpers
 * ====================================================================================================================== */
static BOOL x104_panel_call_guarded(void (*function)(void *, void *), void *a, void *b, const char *what)
{
    BOOL clean = TRUE;
    X104Seh seh;
    memset(&seh, 0, sizeof(seh));
    __try {
        function(a, b);
    } __except (x104_seh_filter((EXCEPTION_POINTERS *)_exception_info(), &seh)) {
        clean = FALSE;
    }
    if (!clean) {
        x104_logf("X104 panel: ERROR exception 0x%08lx at %p (game rva %08lx) inside %s - the step is skipped, the game continues", (unsigned long)seh.code, seh.address,
                  (unsigned long)seh.rva, what ? what : "(unnamed)");
    }
    return clean;
}

static DWORD WINAPI x104_panel_msgbox_thread(void *argument)
{
    BOOL exit_after = argument != NULL;
    MessageBoxA(NULL, x104_panel_msg_text, "Minecraft PE 0.10.4 - Win32Craft", 0x10u | 0x10000u | 0x40000u);   /* MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST */
    if (exit_after) x104_app_request_exit(3);
    InterlockedExchange(&x104_panel_msg_busy, 0);
    return 0;
}

/* a message box on its own thread: never blocks the UI thread inside a game call / the pump.  exit_after: x104_app_request_exit(3) when the box is closed. */
static void x104_panel_show_message(const char *text, BOOL exit_after)
{
    HANDLE thread;
    if (InterlockedExchange(&x104_panel_msg_busy, 1)) {
        x104_logf("X104 panel: a message box is already open - not showing a second one (\"%s\")", text);
        if (exit_after) x104_app_request_exit(3);
        return;
    }
    lstrcpynA(x104_panel_msg_text, text, (int)sizeof(x104_panel_msg_text));
    thread = CreateThread(NULL, 0, x104_panel_msgbox_thread, exit_after ? (void *)1 : NULL, 0, NULL);
    if (!thread) {
        x104_logf("X104 panel: ERROR CreateThread for the message box failed, error=%lu (text: %s)", (unsigned long)GetLastError(), text);
        InterlockedExchange(&x104_panel_msg_busy, 0);
        if (exit_after) x104_app_request_exit(3);
        return;
    }
    CloseHandle(thread);
}

static void x104_panel_format_levels(char *out, SIZE_T capacity, const UINT *levels, UINT count)
{
    SIZE_T used = 0;
    UINT index;
    out[0] = 0;
    if (!levels || !count) {
        lstrcpynA(out, "(runtime default list)", (int)capacity);
        return;
    }
    if (IsBadReadPtr(levels, (UINT_PTR)count * sizeof(*levels))) {
        wsprintfA(out, "(unreadable list %p)", (const void *)levels);
        return;
    }
    for (index = 0; index < count && index < 12 && used + 8 < capacity; ++index) used += (SIZE_T)wsprintfA(out + used, "%04X ", levels[index]);
}

/* ASCII case-insensitive path comparison ignoring trailing backslashes */
static BOOL x104_panel_path_equals(const wchar_t *a, const wchar_t *b)
{
    SIZE_T la = (SIZE_T)lstrlenW(a);
    SIZE_T lb = (SIZE_T)lstrlenW(b);
    SIZE_T index;
    while (la && (a[la - 1] == L'\\' || a[la - 1] == L'/')) --la;
    while (lb && (b[lb - 1] == L'\\' || b[lb - 1] == L'/')) --lb;
    if (la != lb) return FALSE;
    for (index = 0; index < la; ++index) {
        wchar_t ca = a[index];
        wchar_t cb = b[index];
        if (ca >= L'A' && ca <= L'Z') ca = (wchar_t)(ca + 32);
        if (cb >= L'A' && cb <= L'Z') cb = (wchar_t)(cb + 32);
        if (ca == L'/') ca = L'\\';
        if (cb == L'/') cb = L'\\';
        if (ca != cb) return FALSE;
    }
    return TRUE;
}

/* ======================================================================================================================
 * 2b. --trace-game-state: Direct3D immediate-context tracer (diagnostic, OFF by default; the app unit logs the game-object half of the same option)
 *
 *     Why: on the first device run the world rendered but no hotbar / touch buttons / HUD appeared.  The game's own decision points are all consistent with "the HUD is drawn"
 *     (arm32/v0_10_4/HUD_INVESTIGATION.md), so this tracer answers the remaining question: are HUD-like draw calls issued to Direct3D at all, and with what state?
 *     It wraps 13 cells of the ID3D11DeviceContext vtable (PSSetShaderResources 8, PSSetShader 9, DrawIndexed 12, Draw 13, Map 14 (failures logged), OMSetRenderTargets 33, OMSetBlendState 35,
 *     OMSetDepthStencilState 36, RSSetState 43, RSSetViewports 44, RSSetScissorRects 45, ClearRenderTargetView 50, ClearDepthStencilView 53), forwards every call unchanged and
 *     remembers the state it saw.  GetDesc (slot 7) is read from the blend / depth-stencil / rasterizer state objects at the moment the game binds them.
 *     Every draw is classified from the CURRENT depth / blend state:   W = depth test on, no blend (world, opaque)      B = depth test on + blend (world, alpha)
 *                                                                        U = depth test OFF + blend (the game's ui_* materials)   u = depth test off, no blend
 *     Log lines (render thread, at Present; every 1 s for 90 s, then every 10 s; plus one line whenever "frame has U draws" flips, at most 60 times):
 *       X104 d3dtrace frame #N [periodic|U-appeared|U-vanished] draws=D indexed=I W=.. B=.. U=.. u=.. clearRT=.. clearDS=.. sets: rtv=.. vp=.. blend=.. depth=.. raster=.. scissor=..
 *       X104 d3dtrace   passes: C1 D1 W120 B8 W3 U52 ...   (run-length list of draw classes and clears, in call order; C = ClearRenderTargetView, D = ClearDepthStencilView)
 *       X104 d3dtrace   end state: rtv=.. (game DR.rtv=.. same|DIFFERENT) dsv=.. viewport=WxH
 *       X104 d3dtrace   draw[-k] ...   the last 8 draws of the frame with indexCount, viewport, scissor, blend, depth, cull, shader and texture-0 pointers
 * ====================================================================================================================== */
#define X104_PANEL_TR_SLOTS      54
#define X104_PANEL_TR_RING       24
#define X104_PANEL_TR_RUNS       30
#define X104_PANEL_TR_DENSE_MS   90000u
#define X104_PANEL_TR_SLOW_MS    10000u
#define X104_PANEL_TR_MAX_FLIPS  60u

typedef struct x104_panel_tr_blend_t {                 /* D3D11_BLEND_DESC (264 bytes) */
    UINT AlphaToCoverageEnable, IndependentBlendEnable;
    struct { UINT BlendEnable, SrcBlend, DestBlend, BlendOp, SrcBlendAlpha, DestBlendAlpha, BlendOpAlpha; BYTE WriteMask, pad[3]; } rt[8];
} x104_panel_tr_blend_t;
typedef struct x104_panel_tr_depth_t {                 /* D3D11_DEPTH_STENCIL_DESC (52 bytes) */
    UINT DepthEnable, DepthWriteMask, DepthFunc, StencilEnable;
    BYTE ReadMask, WriteMask, pad[2];
    UINT front[4], back[4];
} x104_panel_tr_depth_t;
typedef struct x104_panel_tr_raster_t {                /* D3D11_RASTERIZER_DESC (40 bytes) */
    UINT FillMode, CullMode;
    INT FrontCounterClockwise, DepthBias;
    float DepthBiasClamp, SlopeScaledDepthBias;
    INT DepthClipEnable, ScissorEnable, MultisampleEnable, AntialiasedLineEnable;
} x104_panel_tr_raster_t;
typedef struct x104_panel_tr_viewport_t { float x, y, w, h, z0, z1; } x104_panel_tr_viewport_t;     /* D3D11_VIEWPORT */
typedef struct x104_panel_tr_rect_t { LONG left, top, right, bottom; } x104_panel_tr_rect_t;          /* RECT */

typedef struct x104_panel_tr_draw_t {
    UINT count, start;
    BYTE kind, cls;
    BYTE bl_en, bl_src, bl_dst, bl_mask, ds_en, ds_wr, ds_fn, ds_st, rs_cull, rs_sc;
    short vp_w, vp_h, sc_w, sc_h;
    void *rtv, *dsv, *ps, *srv0;
    void *vscb[4], *pscb[4];                            /* constant buffers of slots 0..3 */
    void *vs, *layout, *vb0, *ib, *vscb0, *pscb0;       /* what the draw read (device log 5 follow-up: why do the world UI draws show nothing) */
    UINT vstride, voffset, ibfmt, iboffset;
    INT base;
    void *vb_caller, *ib_caller, *draw_caller;          /* return addresses into the game (image rva = value - image base) */
} x104_panel_tr_draw_t;

typedef struct x104_panel_tr_vt_t { void **vtbl; void *orig[X104_PANEL_TR_SLOTS]; } x104_panel_tr_vt_t;

typedef struct x104_panel_tr_t {
    BOOL enabled, installed;
    volatile LONG vt_count;
    volatile LONG heals;                 /* draw/clear cells the runtime re-wrote and the tracer put back (x104_panel_tr_heal) */
    x104_panel_tr_vt_t vts[2];
    /* the context state as the wrappers saw it */
    void *rtv, *dsv, *ps, *srv0, *bs, *dss, *rs;
    void *vs, *layout, *vb0, *ib, *vscb0, *pscb0;
    void *vscb[4], *pscb[4];
    UINT vstride, voffset, ibfmt, iboffset;
    void *vb_caller, *ib_caller;
    BYTE bl_en, bl_src, bl_dst, bl_mask, ds_en, ds_wr, ds_fn, ds_st, rs_cull, rs_sc;
    short vp_w, vp_h, sc_w, sc_h;
    /* the frame being recorded */
    UINT draws, indexed, cls_w, cls_b, cls_u_ui, cls_u_flat, clears_rt, clears_ds, n_rtv, n_vp, n_bs, n_dss, n_rs, n_sc;
    UINT maps, maps_discard, maps_nooverwrite, maps_failed;
    UINT ring_pos;
    x104_panel_tr_draw_t ring[X104_PANEL_TR_RING];
    BYTE run_cls[X104_PANEL_TR_RUNS];
    UINT run_n[X104_PANEL_TR_RUNS];
    UINT nruns, run_overflow;
    /* log control */
    DWORD start_tick, next_tick;
    LONG frames;
    BOOL last_ui;
    UINT flips;
    BOOL fix_vb;                         /* vertex-buffer stride fixer (default on, --no-vb-stride-fix turns it off) */
    UINT vb_count;                       /* NumBuffers of the last IASetVertexBuffers(StartSlot 0) */
    volatile LONG stride_fixes;
    BOOL dumped_menu, dumped_world;     /* the one-shot frame / texture dumps (x104_panel_tr_dump_frame) */
    UINT world_frames;
} x104_panel_tr_t;

static x104_panel_tr_t x104_panel_tr;

typedef void (X104_API *x104_panel_tr_fn_di)(void *, UINT, UINT, INT);
/* ---- input layout stride registry + vertex-buffer stride fixer ----
 * Device log 7: every HUD / button draw with a TEXTURE binds its (dynamic) vertex buffer with stride 0 although the data in the buffer is a valid interleaved 20-byte
 * vertex stream (position float3, colour, packed uv: exactly what its input layout describes) and the GUI projection matrix is right.  A stride of 0 makes every vertex
 * read element 0 -> degenerate quads -> nothing is drawn (the menu draws, which use stride 16, are fine).  The fixer remembers the stride each input layout needs (the
 * largest AlignedByteOffset + element size of slot 0, computed in CreateInputLayout) and, right before a draw, re-binds slot 0 with that stride when the game left it at 0. */
#define X104_PANEL_LS_MAX 256
static struct { void *layout; UINT stride; } x104_panel_ls[X104_PANEL_LS_MAX];
static volatile LONG x104_panel_ls_count;

static UINT x104_panel_format_bytes(UINT format)
{
    switch (format) {
    case 2: case 3: case 4: return 16;
    case 6: case 7: case 8: return 12;
    case 10: case 11: case 12: case 13: case 14: case 16: case 17: case 18: return 8;
    case 24: case 25: case 28: case 30: case 31: case 32: case 34: case 35: case 36: case 37: case 38: case 41: case 42: case 43: case 87: return 4;
    case 49: case 50: case 51: case 52: case 54: case 56: case 57: case 58: case 59: return 2;
    case 61: case 62: case 63: case 64: return 1;
    default: return 0;
    }
}

static void x104_panel_ls_add(void *layout, const void *elements, UINT count)
{
    const DWORD *e = (const DWORD *)elements;
    UINT end = 0, i;
    LONG index;
    if (!layout || !elements || count == 0 || count > 16u || IsBadReadPtr(elements, count * 28u)) return;
    for (i = 0; i < count; ++i) {                       /* D3D11_INPUT_ELEMENT_DESC: name, index, format, slot, offset, class, step */
        UINT size = x104_panel_format_bytes(e[i * 7u + 2]);
        UINT offset = e[i * 7u + 4];
        if (e[i * 7u + 3] != 0u) continue;                    /* slot 0 only */
        if (size == 0) return;                                /* unknown format: do not guess */
        if (offset == 0xFFFFFFFFu) offset = end;              /* D3D11_APPEND_ALIGNED_ELEMENT */
        if (offset + size > end) end = offset + size;
    }
    if (end == 0) return;
    index = x104_panel_ls_count;
    if (index >= X104_PANEL_LS_MAX) return;
    x104_panel_ls[index].layout = layout;
    x104_panel_ls[index].stride = end;
    __sync_synchronize();
    x104_panel_ls_count = index + 1;
}

static UINT x104_panel_ls_find(void *layout)
{
    LONG count = x104_panel_ls_count;
    LONG i;
    for (i = count; i > 0; --i)
        if (x104_panel_ls[i - 1].layout == layout) return x104_panel_ls[i - 1].stride;
    return 0;
}

typedef void (X104_API *x104_panel_tr_fn_draw)(void *, UINT, UINT);
typedef void (X104_API *x104_panel_tr_fn_vs)(void *, void *, void *const *, UINT);
typedef void (X104_API *x104_panel_tr_fn_il)(void *, void *);
typedef void (X104_API *x104_panel_tr_fn_vb)(void *, UINT, UINT, void *const *, const UINT *, const UINT *);
typedef void (X104_API *x104_panel_tr_fn_ib)(void *, void *, UINT, UINT);
typedef void (X104_API *x104_panel_tr_fn_cb)(void *, UINT, UINT, void *const *);
typedef void (X104_API *x104_panel_tr_fn_dii)(void *, UINT, UINT, UINT, INT, UINT);
typedef void (X104_API *x104_panel_tr_fn_di2)(void *, UINT, UINT, UINT, UINT);
typedef void (X104_API *x104_panel_tr_fn_omrt)(void *, UINT, void *const *, void *);
typedef void (X104_API *x104_panel_tr_fn_bs)(void *, void *, const float *, UINT);
typedef void (X104_API *x104_panel_tr_fn_dss)(void *, void *, UINT);
typedef void (X104_API *x104_panel_tr_fn_rs)(void *, void *);
typedef void (X104_API *x104_panel_tr_fn_vp)(void *, UINT, const x104_panel_tr_viewport_t *);
typedef void (X104_API *x104_panel_tr_fn_sc)(void *, UINT, const x104_panel_tr_rect_t *);
typedef void (X104_API *x104_panel_tr_fn_ps)(void *, void *, void *const *, UINT);
typedef void (X104_API *x104_panel_tr_fn_srv)(void *, UINT, UINT, void *const *);
typedef void (X104_API *x104_panel_tr_fn_clr)(void *, void *, const float *);
typedef void (X104_API *x104_panel_tr_fn_cld)(void *, void *, UINT, float, BYTE);
typedef HRESULT (X104_API *x104_panel_tr_fn_map)(void *, void *, UINT, UINT, UINT, void *);

/* command line only; the unit parses its own copy so the core / boot stay untouched */
static void x104_panel_tr_parse(void)
{
    const wchar_t *cursor = GetCommandLineW();
    x104_panel_tr.fix_vb = TRUE;
    wchar_t argument[128];
    if (!cursor) return;
    cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));          /* argv[0] */
    while (*cursor) {
        cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
        if (host_argument_equals(argument, L"--trace-game-state")) x104_panel_tr.enabled = TRUE;
        if (host_argument_equals(argument, L"--no-vb-stride-fix")) x104_panel_tr.fix_vb = FALSE;
    }
}

static void x104_panel_tr_heal(void *self);
static void x104_panel_tr_fix_stride(void *self);

static void *x104_panel_tr_orig(void *self, int slot)
{
    void **vtbl = self ? *(void ***)self : NULL;
    LONG count = x104_panel_tr.vt_count;
    LONG index;
    if (slot < 0 || slot >= X104_PANEL_TR_SLOTS) return NULL;
    for (index = 0; index < count && index < 2; ++index)
        if (x104_panel_tr.vts[index].vtbl == vtbl) return x104_panel_tr.vts[index].orig[slot];
    return count > 0 ? x104_panel_tr.vts[0].orig[slot] : NULL;
}

/* ID3D11{Blend,DepthStencil,Rasterizer}State::GetDesc is slot 7 (IUnknown 3 + ID3D11DeviceChild 4); the pointer is valid while the game binds it */
static BOOL x104_panel_tr_getdesc(void *state, void *desc)
{
    BOOL clean = TRUE;
    X104Seh seh;
    memset(&seh, 0, sizeof(seh));
    if (!state || IsBadReadPtr(state, 4)) return FALSE;
    __try {
        X104_VFN(state, 7, void (X104_API *)(void *, void *))(state, desc);
    } __except (x104_seh_filter((EXCEPTION_POINTERS *)_exception_info(), &seh)) {
        clean = FALSE;
    }
    return clean;
}

static void x104_panel_tr_run(BYTE cls)
{
    x104_panel_tr_t *t = &x104_panel_tr;
    if (t->nruns && t->run_cls[t->nruns - 1] == cls) {
        ++t->run_n[t->nruns - 1];
    } else if (t->nruns < X104_PANEL_TR_RUNS) {
        t->run_cls[t->nruns] = cls;
        t->run_n[t->nruns] = 1;
        ++t->nruns;
    } else {
        ++t->run_overflow;
    }
}

static void x104_panel_tr_record(BYTE kind, UINT count, UINT start, INT base, void *caller)
{
    x104_panel_tr_t *t = &x104_panel_tr;
    x104_panel_tr_draw_t *d;
    BYTE cls;
    if (!t->ds_en && t->bl_en) { cls = 'U'; ++t->cls_u_ui; }
    else if (!t->ds_en) { cls = 'u'; ++t->cls_u_flat; }
    else if (t->bl_en) { cls = 'B'; ++t->cls_b; }
    else { cls = 'W'; ++t->cls_w; }
    ++t->draws;
    if (kind == 'I' || kind == 'J') ++t->indexed;
    x104_panel_tr_run(cls);
    d = &t->ring[t->ring_pos % X104_PANEL_TR_RING];
    ++t->ring_pos;
    d->count = count; d->start = start; d->kind = kind; d->cls = cls;
    d->bl_en = t->bl_en; d->bl_src = t->bl_src; d->bl_dst = t->bl_dst; d->bl_mask = t->bl_mask;
    d->ds_en = t->ds_en; d->ds_wr = t->ds_wr; d->ds_fn = t->ds_fn; d->ds_st = t->ds_st;
    d->rs_cull = t->rs_cull; d->rs_sc = t->rs_sc;
    d->vp_w = t->vp_w; d->vp_h = t->vp_h; d->sc_w = t->sc_w; d->sc_h = t->sc_h;
    d->rtv = t->rtv; d->dsv = t->dsv; d->ps = t->ps; d->srv0 = t->srv0;
    d->vs = t->vs; d->layout = t->layout; d->vb0 = t->vb0; d->ib = t->ib; d->vscb0 = t->vscb0; d->pscb0 = t->pscb0;
    { UINT k; for (k = 0; k < 4; ++k) { d->vscb[k] = t->vscb[k]; d->pscb[k] = t->pscb[k]; } }
    d->vstride = t->vstride; d->voffset = t->voffset; d->ibfmt = t->ibfmt; d->iboffset = t->iboffset; d->base = base;
    d->vb_caller = t->vb_caller; d->ib_caller = t->ib_caller; d->draw_caller = caller;
}

static void X104_API x104_panel_tr_draw_indexed(void *self, UINT count, UINT start, INT base)
{
    x104_panel_tr_fn_di original = (x104_panel_tr_fn_di)x104_panel_tr_orig(self, 12);
    x104_panel_tr_fix_stride(self);
    x104_panel_tr_record('I', count, start, base, __builtin_return_address(0));
    if (original) original(self, count, start, base);
}

static void X104_API x104_panel_tr_draw(void *self, UINT count, UINT start)
{
    x104_panel_tr_fn_draw original = (x104_panel_tr_fn_draw)x104_panel_tr_orig(self, 13);
    x104_panel_tr_fix_stride(self);
    x104_panel_tr_record('V', count, start, 0, __builtin_return_address(0));
    if (original) original(self, count, start);
}

/* IA / constant-buffer / VS binding trackers (slots 7 VSSetConstantBuffers, 11 VSSetShader, 16 PSSetConstantBuffers, 17 IASetInputLayout, 18 IASetVertexBuffers, 19 IASetIndexBuffer) */
static void X104_API x104_panel_tr_set_vscb(void *self, UINT start, UINT count, void *const *buffers)
{
    x104_panel_tr_fn_cb original = (x104_panel_tr_fn_cb)x104_panel_tr_orig(self, 7);
    if (buffers) { UINT k; for (k = 0; k < count && start + k < 4u; ++k) x104_panel_tr.vscb[start + k] = buffers[k]; }
    if (start == 0 && count >= 1 && buffers) x104_panel_tr.vscb0 = buffers[0];
    if (original) original(self, start, count, buffers);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_pscb(void *self, UINT start, UINT count, void *const *buffers)
{
    x104_panel_tr_fn_cb original = (x104_panel_tr_fn_cb)x104_panel_tr_orig(self, 16);
    if (buffers) { UINT k; for (k = 0; k < count && start + k < 4u; ++k) x104_panel_tr.pscb[start + k] = buffers[k]; }
    if (start == 0 && count >= 1 && buffers) x104_panel_tr.pscb0 = buffers[0];
    if (original) original(self, start, count, buffers);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_vs(void *self, void *shader, void *const *instances, UINT count)
{
    x104_panel_tr_fn_vs original = (x104_panel_tr_fn_vs)x104_panel_tr_orig(self, 11);
    x104_panel_tr.vs = shader;
    if (original) original(self, shader, instances, count);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_layout(void *self, void *layout)
{
    x104_panel_tr_fn_il original = (x104_panel_tr_fn_il)x104_panel_tr_orig(self, 17);
    x104_panel_tr.layout = layout;
    if (original) original(self, layout);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_vb(void *self, UINT start, UINT count, void *const *buffers, const UINT *strides, const UINT *offsets)
{
    x104_panel_tr_fn_vb original = (x104_panel_tr_fn_vb)x104_panel_tr_orig(self, 18);
    if (start == 0 && count >= 1 && buffers) {
        x104_panel_tr.vb0 = buffers[0];
        x104_panel_tr.vb_count = count;
        x104_panel_tr.vstride = strides ? strides[0] : 0;
        x104_panel_tr.voffset = offsets ? offsets[0] : 0;
        x104_panel_tr.vb_caller = __builtin_return_address(0);
        if (x104_panel_tr.vstride == 0) {
            static volatile LONG logged;
            if ((LONG)__sync_add_and_fetch(&logged, 1) <= 12)
                x104_logf("X104 d3dtrace: IASetVertexBuffers STRIDE 0 from game rva 0x%lx: start=%u count=%u buffer0=%p strides=%p offsets=%p stride[0]=%u stride[1]=%u offset[0]=%u", (unsigned long)((BYTE *)__builtin_return_address(0) - x104.image),
                          start, count, buffers[0], (const void *)strides, (const void *)offsets, strides ? strides[0] : 0u, (strides && count > 1) ? strides[1] : 0u, offsets ? offsets[0] : 0u);
        }
    }
    if (original) original(self, start, count, buffers, strides, offsets);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_ib(void *self, void *buffer, UINT format, UINT offset)
{
    x104_panel_tr_fn_ib original = (x104_panel_tr_fn_ib)x104_panel_tr_orig(self, 19);
    x104_panel_tr.ib = buffer;
    x104_panel_tr.ibfmt = format;
    x104_panel_tr.iboffset = offset;
    x104_panel_tr.ib_caller = __builtin_return_address(0);
    if (original) original(self, buffer, format, offset);
    x104_panel_tr_heal(self);
}

/* before every draw: slot 0 of the vertex buffers bound with stride 0 although the bound input layout needs a stride -> re-bind it with the layout's stride */
static void x104_panel_tr_fix_stride(void *self)
{
    x104_panel_tr_t *t = &x104_panel_tr;
    UINT need;
    x104_panel_tr_fn_vb bind;
    UINT stride;
    void *buffer;
    if (!t->fix_vb || t->vstride != 0 || !t->vb0 || !t->layout || t->vb_count != 1) return;
    need = x104_panel_ls_find(t->layout);
    if (need == 0) return;
    bind = (x104_panel_tr_fn_vb)x104_panel_tr_orig(self, 18);
    if (!bind) return;
    stride = need;
    buffer = t->vb0;
    bind(self, 0, 1, &buffer, &stride, &t->voffset);
    t->vstride = need;
    if ((LONG)__sync_add_and_fetch(&t->stride_fixes, 1) <= 12)
        x104_logf("X104 panel: vertex-buffer STRIDE FIX #%ld: the game bound buffer %p with stride 0 (from game rva 0x%lx) but input layout %p needs %u bytes - re-bound with %u before the draw",
                  (long)t->stride_fixes, buffer, (unsigned long)((BYTE *)t->vb_caller - x104.image), t->layout, need, need);
}

/* DrawIndexedInstanced (slot 20) / DrawInstanced (slot 21): the first device log (HUD investigation) showed ZERO calls of slots 12/13 although the game draws every frame */
static void X104_API x104_panel_tr_draw_indexed_instanced(void *self, UINT index_count, UINT instances, UINT start, INT base, UINT start_instance)
{
    x104_panel_tr_fn_dii original = (x104_panel_tr_fn_dii)x104_panel_tr_orig(self, 20);
    x104_panel_tr_fix_stride(self);
    x104_panel_tr_record('J', index_count, start, base, __builtin_return_address(0));
    if (original) original(self, index_count, instances, start, base, start_instance);
}

static void X104_API x104_panel_tr_draw_instanced(void *self, UINT vertex_count, UINT instances, UINT start, UINT start_instance)
{
    x104_panel_tr_fn_di2 original = (x104_panel_tr_fn_di2)x104_panel_tr_orig(self, 21);
    x104_panel_tr_fix_stride(self);
    x104_panel_tr_record('W', vertex_count, start, 0, __builtin_return_address(0));
    if (original) original(self, vertex_count, instances, start, start_instance);
}

static void X104_API x104_panel_tr_set_rtv(void *self, UINT count, void *const *views, void *depth)
{
    x104_panel_tr_fn_omrt original = (x104_panel_tr_fn_omrt)x104_panel_tr_orig(self, 33);
    x104_panel_tr_t *t = &x104_panel_tr;
    t->rtv = (count && views && !IsBadReadPtr(views, sizeof(void *))) ? views[0] : NULL;
    t->dsv = depth;
    ++t->n_rtv;
    if (original) original(self, count, views, depth);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_blend(void *self, void *state, const float *factor, UINT mask)
{
    x104_panel_tr_fn_bs original = (x104_panel_tr_fn_bs)x104_panel_tr_orig(self, 35);
    x104_panel_tr_t *t = &x104_panel_tr;
    x104_panel_tr_blend_t desc;
    t->bs = state;
    ++t->n_bs;
    memset(&desc, 0, sizeof(desc));
    if (!state) {                                                    /* NULL = the default state: blending off */
        t->bl_en = 0; t->bl_src = 1; t->bl_dst = 0; t->bl_mask = 15;
    } else if (x104_panel_tr_getdesc(state, &desc)) {
        t->bl_en = desc.rt[0].BlendEnable ? 1 : 0; t->bl_src = (BYTE)desc.rt[0].SrcBlend; t->bl_dst = (BYTE)desc.rt[0].DestBlend; t->bl_mask = desc.rt[0].WriteMask;
    } else {
        t->bl_en = 0; t->bl_src = 255; t->bl_dst = 255; t->bl_mask = 0;
    }
    if (original) original(self, state, factor, mask);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_depth(void *self, void *state, UINT reference)
{
    x104_panel_tr_fn_dss original = (x104_panel_tr_fn_dss)x104_panel_tr_orig(self, 36);
    x104_panel_tr_t *t = &x104_panel_tr;
    x104_panel_tr_depth_t desc;
    t->dss = state;
    ++t->n_dss;
    memset(&desc, 0, sizeof(desc));
    if (!state) {                                                    /* NULL = the default state: depth test on, write all, LESS */
        t->ds_en = 1; t->ds_wr = 1; t->ds_fn = 2; t->ds_st = 0;
    } else if (x104_panel_tr_getdesc(state, &desc)) {
        t->ds_en = desc.DepthEnable ? 1 : 0; t->ds_wr = (BYTE)desc.DepthWriteMask; t->ds_fn = (BYTE)desc.DepthFunc; t->ds_st = desc.StencilEnable ? 1 : 0;
    } else {
        t->ds_en = 1; t->ds_wr = 255; t->ds_fn = 255; t->ds_st = 255;
    }
    if (original) original(self, state, reference);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_raster(void *self, void *state)
{
    x104_panel_tr_fn_rs original = (x104_panel_tr_fn_rs)x104_panel_tr_orig(self, 43);
    x104_panel_tr_t *t = &x104_panel_tr;
    x104_panel_tr_raster_t desc;
    t->rs = state;
    ++t->n_rs;
    memset(&desc, 0, sizeof(desc));
    if (!state) {                                                    /* NULL = the default state: cull back, no scissor */
        t->rs_cull = 3; t->rs_sc = 0;
    } else if (x104_panel_tr_getdesc(state, &desc)) {
        t->rs_cull = (BYTE)desc.CullMode; t->rs_sc = desc.ScissorEnable ? 1 : 0;
    } else {
        t->rs_cull = 255; t->rs_sc = 255;
    }
    if (original) original(self, state);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_viewports(void *self, UINT count, const x104_panel_tr_viewport_t *viewports)
{
    x104_panel_tr_fn_vp original = (x104_panel_tr_fn_vp)x104_panel_tr_orig(self, 44);
    x104_panel_tr_t *t = &x104_panel_tr;
    ++t->n_vp;
    if (count && viewports && !IsBadReadPtr(viewports, sizeof(*viewports))) {
        t->vp_w = (short)viewports[0].w;
        t->vp_h = (short)viewports[0].h;
    }
    if (original) original(self, count, viewports);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_scissors(void *self, UINT count, const x104_panel_tr_rect_t *rects)
{
    x104_panel_tr_fn_sc original = (x104_panel_tr_fn_sc)x104_panel_tr_orig(self, 45);
    x104_panel_tr_t *t = &x104_panel_tr;
    ++t->n_sc;
    if (count && rects && !IsBadReadPtr(rects, sizeof(*rects))) {
        t->sc_w = (short)(rects[0].right - rects[0].left);
        t->sc_h = (short)(rects[0].bottom - rects[0].top);
    }
    if (original) original(self, count, rects);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_ps(void *self, void *shader, void *const *instances, UINT count)
{
    x104_panel_tr_fn_ps original = (x104_panel_tr_fn_ps)x104_panel_tr_orig(self, 9);
    x104_panel_tr.ps = shader;
    if (original) original(self, shader, instances, count);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_set_srv(void *self, UINT start, UINT count, void *const *views)
{
    x104_panel_tr_fn_srv original = (x104_panel_tr_fn_srv)x104_panel_tr_orig(self, 8);
    if (start == 0) x104_panel_tr.srv0 = (count && views && !IsBadReadPtr(views, sizeof(void *))) ? views[0] : NULL;
    if (original) original(self, start, count, views);
    x104_panel_tr_heal(self);
}

static void X104_API x104_panel_tr_clear_rtv(void *self, void *view, const float *color)
{
    x104_panel_tr_fn_clr original = (x104_panel_tr_fn_clr)x104_panel_tr_orig(self, 50);
    ++x104_panel_tr.clears_rt;
    x104_panel_tr_run('C');
    if (original) original(self, view, color);
}

static void X104_API x104_panel_tr_clear_dsv(void *self, void *view, UINT flags, float depth, BYTE stencil)
{
    x104_panel_tr_fn_cld original = (x104_panel_tr_fn_cld)x104_panel_tr_orig(self, 53);
    ++x104_panel_tr.clears_ds;
    x104_panel_tr_run('D');
    if (original) original(self, view, flags, depth, stencil);
}

/* ID3D11DeviceContext::Map (slot 14): constant / vertex buffer updates.  A FAILED Map (feature level 9_1: e.g. WRITE_NO_OVERWRITE on a constant buffer is invalid) is ignored by the game
 * and leaves stale data in the buffer - exactly the kind of "swallowed render error" that would make a draw invisible */
static HRESULT X104_API x104_panel_tr_map(void *self, void *resource, UINT subresource, UINT type, UINT flags, void *mapped)
{
    static volatile LONG failed_lines;
    x104_panel_tr_fn_map original = (x104_panel_tr_fn_map)x104_panel_tr_orig(self, 14);
    x104_panel_tr_t *t = &x104_panel_tr;
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, resource, subresource, type, flags, mapped);
    ++t->maps;
    if (type == 4u) ++t->maps_discard;
    else if (type == 5u) ++t->maps_nooverwrite;
    if (FAILED(hr)) {
        ++t->maps_failed;
        X104_LOG_LIMITED(&failed_lines, 40, "X104 d3dtrace: Map FAILED hr=0x%08lx %s resource=%p subresource=%u type=%u (4 discard, 5 no-overwrite) flags=%08lx", (unsigned long)hr, x104_hr_name(hr), resource,
                         subresource, type, (unsigned long)flags);
    }
    return hr;
}

/* patch the 13 cells of one ID3D11DeviceContext vtable (data cells of d3d11.dll, PAGE_READWRITE via arm32_patch_cell); the originals are stored BEFORE a cell goes live */
/* the cells of the ID3D11DeviceContext vtable the tracer owns (also re-checked every 30 frames: the runtime may rewrite cells of its heap vtable) */
static const struct x104_panel_tr_cell_t { int slot; void *wrapper; const char *name; } x104_panel_tr_cells[] = {
    { 7,  (void *)x104_panel_tr_set_vscb,      "VSSetConstantBuffers" },
    { 8,  (void *)x104_panel_tr_set_srv,       "PSSetShaderResources" },
    { 9,  (void *)x104_panel_tr_set_ps,        "PSSetShader" },
    { 11, (void *)x104_panel_tr_set_vs,        "VSSetShader" },
    { 12, (void *)x104_panel_tr_draw_indexed,  "DrawIndexed" },
    { 13, (void *)x104_panel_tr_draw,          "Draw" },
    { 20, (void *)x104_panel_tr_draw_indexed_instanced, "DrawIndexedInstanced" },
    { 21, (void *)x104_panel_tr_draw_instanced, "DrawInstanced" },
    { 14, (void *)x104_panel_tr_map,           "Map" },
    { 16, (void *)x104_panel_tr_set_pscb,      "PSSetConstantBuffers" },
    { 17, (void *)x104_panel_tr_set_layout,    "IASetInputLayout" },
    { 18, (void *)x104_panel_tr_set_vb,        "IASetVertexBuffers" },
    { 19, (void *)x104_panel_tr_set_ib,        "IASetIndexBuffer" },
    { 33, (void *)x104_panel_tr_set_rtv,       "OMSetRenderTargets" },
    { 35, (void *)x104_panel_tr_set_blend,     "OMSetBlendState" },
    { 36, (void *)x104_panel_tr_set_depth,     "OMSetDepthStencilState" },
    { 43, (void *)x104_panel_tr_set_raster,    "RSSetState" },
    { 44, (void *)x104_panel_tr_set_viewports, "RSSetViewports" },
    { 45, (void *)x104_panel_tr_set_scissors,  "RSSetScissorRects" },
    { 50, (void *)x104_panel_tr_clear_rtv,     "ClearRenderTargetView" },
    { 53, (void *)x104_panel_tr_clear_dsv,     "ClearDepthStencilView" }
    };
#define X104_PANEL_TR_NCELLS ((unsigned)(sizeof(x104_panel_tr_cells) / sizeof(x104_panel_tr_cells[0])))

/* The Direct3D runtime REWRITES the draw / clear cells (DrawIndexed 12, Draw 13, DrawIndexedInstanced 20, DrawInstanced 21, ClearRenderTargetView 50,
 * ClearDepthStencilView 53) of its heap vtable back to its own functions (device log 3: 'CELL REPLACED by the runtime', most likely it specialises them after the
 * state calls that change the bound render target / states).  Putting our wrappers back right after every state call the runtime could have reacted to keeps the
 * draw tracer working; the vtable is runtime heap memory, so a plain interlocked store is enough. */
static void x104_panel_tr_heal(void *self)
{
    x104_panel_tr_t *t = &x104_panel_tr;
    void **vtbl = self ? *(void ***)self : NULL;
    LONG index;
    unsigned entry;
    for (index = 0; index < t->vt_count && index < 2; ++index) {
        if (t->vts[index].vtbl != vtbl) continue;
        for (entry = 0; entry < X104_PANEL_TR_NCELLS; ++entry) {
            int slot = x104_panel_tr_cells[entry].slot;
            void *current = vtbl[slot];
            if (current == x104_panel_tr_cells[entry].wrapper) continue;
            t->vts[index].orig[slot] = current;
            __atomic_store_n((void **)&vtbl[slot], x104_panel_tr_cells[entry].wrapper, __ATOMIC_RELEASE);
            ++t->heals;
        }
        return;
    }
}

static void x104_panel_tr_install_vtable(void *context, const char *which)
{
    const struct x104_panel_tr_cell_t *table = x104_panel_tr_cells;
    x104_panel_tr_t *t = &x104_panel_tr;
    void **vtbl;
    LONG index;
    unsigned entry;
    unsigned patched = 0;
    if (!context) return;
    vtbl = *(void ***)context;
    if (!vtbl || IsBadReadPtr(vtbl, X104_PANEL_TR_SLOTS * sizeof(void *))) {
        x104_logf("X104 d3dtrace: ERROR context vtable %p (%s) unreadable - the draw tracer is NOT installed for it", (void *)vtbl, which);
        return;
    }
    for (index = 0; index < t->vt_count && index < 2; ++index)
        if (t->vts[index].vtbl == vtbl) {
            x104_logf("X104 d3dtrace: context vtable %p (%s) is already traced", (void *)vtbl, which);
            return;
        }
    if (t->vt_count >= 2) {
        x104_logf("X104 d3dtrace: WARNING a third context vtable %p (%s) - not traced", (void *)vtbl, which);
        return;
    }
    index = t->vt_count;
    t->vts[index].vtbl = vtbl;
    for (entry = 0; entry < X104_PANEL_TR_NCELLS; ++entry) t->vts[index].orig[table[entry].slot] = vtbl[table[entry].slot];
    __sync_synchronize();
    t->vt_count = index + 1;
    if (!t->installed) {                                             /* the defaults the game's first state-set calls overwrite */
        t->ds_en = 1; t->ds_wr = 1; t->ds_fn = 2; t->bl_src = 1; t->bl_mask = 15; t->rs_cull = 3;
        t->installed = TRUE;
    }
    for (entry = 0; entry < X104_PANEL_TR_NCELLS; ++entry) {
        if (arm32_patch_cell((void **)&vtbl[table[entry].slot], t->vts[index].orig[table[entry].slot], table[entry].wrapper, NULL, table[entry].name)) ++patched;
        else x104_logf("X104 d3dtrace: WARNING context vtable %p cell %d (%s) could NOT be patched", (void *)vtbl, table[entry].slot, table[entry].name);
    }
    x104_logf("X104 d3dtrace: ID3D11DeviceContext vtable %p (%s): %u of %u cells wrapped (%s%s)", (void *)vtbl, which, patched, X104_PANEL_TR_NCELLS,
              t->enabled ? "draw / state tracer for --trace-game-state" : "quiet", t->fix_vb ? ", vertex-buffer stride fixer ON (--no-vb-stride-fix turns it off)" : ", stride fixer off");
    {
        char where_vtbl[160], where_draw[160];
        arm32_iat_describe_address((const void *)vtbl, where_vtbl, sizeof(where_vtbl));
        arm32_iat_describe_address(t->vts[index].orig[12], where_draw, sizeof(where_draw));
        x104_logf("X104 d3dtrace: vtable lives at %s; original DrawIndexed cell = %p (%s), Draw = %p, DrawIndexedInstanced = %p, ClearDepthStencilView = %p", where_vtbl, t->vts[index].orig[12], where_draw,
                  t->vts[index].orig[13], t->vts[index].orig[20], t->vts[index].orig[53]);
    }
}

/* the first device logs showed the state-setting cells being hit every frame but the draw / clear cells never (draws=0, clearDS=0 although the game draws and clears
 * every frame): check that every wrapped cell still holds our wrapper, log (and re-patch) the ones the runtime replaced */
static void x104_panel_tr_verify_cells(void)
{
    x104_panel_tr_t *t = &x104_panel_tr;
    static volatile LONG reported;
    LONG index;
    unsigned entry;
    for (index = 0; index < t->vt_count && index < 2; ++index) {
        void **vtbl = t->vts[index].vtbl;
        if (!vtbl || IsBadReadPtr(vtbl, X104_PANEL_TR_SLOTS * sizeof(void *))) continue;
        for (entry = 0; entry < X104_PANEL_TR_NCELLS; ++entry) {
            int slot = x104_panel_tr_cells[entry].slot;
            void *current = vtbl[slot];
            if (current == x104_panel_tr_cells[entry].wrapper) continue;
            if (InterlockedIncrement(&reported) <= 30) {
                char where[160];
                arm32_iat_describe_address(current, where, sizeof(where));
                x104_logf("X104 d3dtrace: CELL REPLACED by the runtime: vtable %p cell %d (%s) holds %p (%s), our wrapper is %p, original was %p - re-patching", (void *)vtbl, slot,
                          x104_panel_tr_cells[entry].name, current, where, x104_panel_tr_cells[entry].wrapper, t->vts[index].orig[slot]);
            }
            t->vts[index].orig[slot] = current;
            __sync_synchronize();
            arm32_patch_cell((void **)&vtbl[slot], current, x104_panel_tr_cells[entry].wrapper, NULL, x104_panel_tr_cells[entry].name);
        }
    }
}

static void x104_panel_tr_log_frame(const char *why, DWORD now)
{
    x104_panel_tr_t *t = &x104_panel_tr;
    char runs[420];
    size_t length = 0;
    unsigned index;
    DWORD dr = 0, dr_rtv = 0, dr_dsv = 0;
    UINT shown;
    UINT first;
    runs[0] = 0;
    for (index = 0; index < t->nruns && length + 24 < sizeof(runs); ++index)
        length += (size_t)wsprintfA(runs + length, "%c%u ", (char)t->run_cls[index], t->run_n[index]);
    if (t->run_overflow && length + 24 < sizeof(runs)) wsprintfA(runs + length, "(+%u more runs)", t->run_overflow);
    x104_logf("X104 d3dtrace frame #%ld [%s] t=%lu.%03lu draws=%u indexed=%u W=%u B=%u U=%u u=%u clearRT=%u clearDS=%u sets: rtv=%u vp=%u blend=%u depth=%u raster=%u scissor=%u maps=%u (discard %u, no-overwrite %u, FAILED %u)",
              (long)t->frames, why, (unsigned long)((now - t->start_tick) / 1000u), (unsigned long)((now - t->start_tick) % 1000u), t->draws, t->indexed, t->cls_w, t->cls_b, t->cls_u_ui,
              t->cls_u_flat, t->clears_rt, t->clears_ds, t->n_rtv, t->n_vp, t->n_bs, t->n_dss, t->n_rs, t->n_sc, t->maps, t->maps_discard, t->maps_nooverwrite, t->maps_failed);
    x104_logf("X104 d3dtrace   passes: %s   (cell heals so far: %ld, vertex-buffer stride fixes: %ld)", runs, (long)t->heals, (long)t->stride_fixes);
    if (x104.image && x104_read_u32(x104.image + ARM0104_FRAME_RENDER_DEVICE_RESOURCES_GLOBAL_INSTANCE, &dr) && dr) {
        x104_read_u32((const void *)(ULONG_PTR)(dr + 0x10u), &dr_rtv);
        x104_read_u32((const void *)(ULONG_PTR)(dr + 0x14u), &dr_dsv);
    }
    x104_logf("X104 d3dtrace   end state: rtv=%p (game DR.rtv=%08lx %s) dsv=%p (DR.dsv=%08lx %s) viewport=%dx%d scissor=%dx%d blend=%d depth=%d cull=%d scissorOn=%d",
              t->rtv, (unsigned long)dr_rtv, (DWORD)(ULONG_PTR)t->rtv == dr_rtv ? "same" : "DIFFERENT", t->dsv, (unsigned long)dr_dsv, (DWORD)(ULONG_PTR)t->dsv == dr_dsv ? "same" : "DIFFERENT",
              (int)t->vp_w, (int)t->vp_h, (int)t->sc_w, (int)t->sc_h, (int)t->bl_en, (int)t->ds_en, (int)t->rs_cull, (int)t->rs_sc);
    shown = t->ring_pos < 8u ? t->ring_pos : 8u;
    first = t->ring_pos - shown;
    for (index = 0; index < shown; ++index) {
        const x104_panel_tr_draw_t *d = &t->ring[(first + index) % X104_PANEL_TR_RING];
        x104_logf("X104 d3dtrace   draw[-%u] %c %c count=%u start=%u vp=%dx%d scissor=%dx%d(on=%d) blend=%d(src%d dst%d mask%x) depth=%d(write%d func%d stencil%d) cull=%d ps=%p tex0=%p rtv=%p dsv=%p",
                  shown - index, (char)d->kind, (char)d->cls, d->count, d->start, (int)d->vp_w, (int)d->vp_h, (int)d->sc_w, (int)d->sc_h, (int)d->rs_sc, (int)d->bl_en,
                  (int)d->bl_src, (int)d->bl_dst, (unsigned)d->bl_mask, (int)d->ds_en, (int)d->ds_wr, (int)d->ds_fn, (int)d->ds_st, (int)d->rs_cull, d->ps, d->srv0, d->rtv, d->dsv);
    }
}

/* ======================================================================================================================
 * 2d. One-shot frame / texture dump (device log 4: the world frames issue 12 UI draws after the 3D pass, with exactly the state of the menu UI draws, which ARE visible,
 *     yet nothing shows).  Once in a menu frame and once in a world frame, on the render thread right before the real Present: copy the back buffer (RTV of the game)
 *     and the textures bound by the last UI draws to staging textures, log their statistics and write them as 32-bit BMP files next to the log
 *     (x104_dump_<menu|world>_<backbuffer|tex>_<n>.bmp).  The UI textures of the world frame are compared with the menu ones: a blank texture (alpha 0) explains the invisible
 *     HUD, a visible HUD in the back-buffer dump with an invisible screen means the problem is after the game (presentation).
 * ====================================================================================================================== */
typedef struct x104_panel_dd_t { UINT Width, Height, MipLevels, ArraySize, Format, SampleCount, SampleQuality, Usage, BindFlags, CPUAccess, Misc; } x104_panel_dd_t;
typedef struct x104_panel_dm_t { void *data; UINT row_pitch, depth_pitch; } x104_panel_dm_t;

static void x104_panel_dd_release(void *object)
{
    if (object && !IsBadReadPtr(object, 4)) X104_VFN(object, 2, ULONG (X104_API *)(void *))(object);
}

static BOOL x104_panel_dd_write_bmp(const char *name, const BYTE *pixels, UINT width, UINT height, UINT row_pitch, BOOL bgra)
{
    static BYTE line[4096 * 4];
    wchar_t wide[96];
    HANDLE file;
    BYTE header[54];
    DWORD written = 0;
    UINT row, col, i;
    if (width == 0 || height == 0 || width > 4096) return FALSE;
    for (i = 0; i + 1 < ARRAYSIZE(wide) && name[i]; ++i) wide[i] = (wchar_t)(unsigned char)name[i];
    wide[i] = 0;
    file = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    memset(header, 0, sizeof(header));
    header[0] = 'B'; header[1] = 'M';
    *(DWORD *)(header + 2) = 54u + width * height * 4u;
    *(DWORD *)(header + 10) = 54u;
    *(DWORD *)(header + 14) = 40u;
    *(LONG *)(header + 18) = (LONG)width;
    *(LONG *)(header + 22) = (LONG)height;
    *(WORD *)(header + 26) = 1;
    *(WORD *)(header + 28) = 32;
    *(DWORD *)(header + 34) = width * height * 4u;
    WriteFile(file, header, 54, &written, NULL);
    for (row = height; row > 0; --row) {
        const BYTE *src = pixels + (row - 1) * row_pitch;
        for (col = 0; col < width; ++col) {
            if (bgra) { line[col * 4] = src[col * 4]; line[col * 4 + 1] = src[col * 4 + 1]; line[col * 4 + 2] = src[col * 4 + 2]; }
            else { line[col * 4] = src[col * 4 + 2]; line[col * 4 + 1] = src[col * 4 + 1]; line[col * 4 + 2] = src[col * 4]; }
            line[col * 4 + 3] = 255;                                                 /* viewers ignore alpha in BI_RGB; the alpha statistics are in the log */
        }
        WriteFile(file, line, width * 4u, &written, NULL);
    }
    CloseHandle(file);
    return TRUE;
}

/* view = an ID3D11RenderTargetView or ID3D11ShaderResourceView (both: ID3D11View::GetResource is slot 7) */
static void x104_panel_tr_dump_view(const char *tag, const char *what, void *view, void *device, void *context)
{
    static volatile LONG counter;
    void *resource = NULL;
    void *staging = NULL;
    UINT dimension = 0;
    x104_panel_dd_t desc, sdesc;
    x104_panel_dm_t mapped;
    HRESULT hr;
    X104Seh seh;
    LONG number = (LONG)__sync_add_and_fetch(&counter, 1);
    memset(&seh, 0, sizeof(seh));
    memset(&desc, 0, sizeof(desc));
    if (!view || IsBadReadPtr(view, 4) || !device || !context) {
        x104_logf("X104 dump[%s] %s: nothing to dump (view=%p device=%p context=%p)", tag, what, view, device, context);
        return;
    }
    __try {
        do {
            X104_VFN(view, 7, void (X104_API *)(void *, void **))(view, &resource);
            if (!resource) { x104_logf("X104 dump[%s] %s view=%p: GetResource returned NULL", tag, what, view); break; }
            X104_VFN(resource, 7, void (X104_API *)(void *, UINT *))(resource, &dimension);
            if (dimension != 3) { x104_logf("X104 dump[%s] %s view=%p resource=%p: dimension %u is not a Texture2D", tag, what, view, resource, dimension); break; }
            X104_VFN(resource, 10, void (X104_API *)(void *, x104_panel_dd_t *))(resource, &desc);
            x104_logf("X104 dump[%s] %s view=%p resource=%p: %ux%u format=%u mips=%u array=%u samples=%u usage=%u bind=0x%x", tag, what, view, resource, desc.Width, desc.Height, desc.Format,
                      desc.MipLevels, desc.ArraySize, desc.SampleCount, desc.Usage, desc.BindFlags);
            if (desc.SampleCount != 1 || (desc.Format != 28 && desc.Format != 87 && desc.Format != 29 && desc.Format != 91)) {
                x104_logf("X104 dump[%s] %s: format %u / samples %u not dumped (8-bit RGBA/BGRA only)", tag, what, desc.Format, desc.SampleCount);
                break;
            }
            sdesc = desc;
            sdesc.Usage = 3;                 /* D3D11_USAGE_STAGING */
            sdesc.BindFlags = 0;
            sdesc.CPUAccess = 0x20000u;      /* D3D11_CPU_ACCESS_READ */
            sdesc.Misc = 0;
            hr = X104_VFN(device, 5, HRESULT (X104_API *)(void *, const x104_panel_dd_t *, const void *, void **))(device, &sdesc, NULL, &staging);
            if (FAILED(hr) || !staging) { x104_logf("X104 dump[%s] %s: CreateTexture2D(staging) FAILED hr=0x%08lx", tag, what, (unsigned long)hr); break; }
            X104_VFN(context, 47, void (X104_API *)(void *, void *, void *))(context, staging, resource);
            memset(&mapped, 0, sizeof(mapped));
            hr = X104_VFN(context, 14, HRESULT (X104_API *)(void *, void *, UINT, UINT, UINT, x104_panel_dm_t *))(context, staging, 0, 1u /* MAP_READ */, 0, &mapped);
            if (FAILED(hr) || !mapped.data) { x104_logf("X104 dump[%s] %s: Map(staging) FAILED hr=0x%08lx", tag, what, (unsigned long)hr); break; }
            {
                UINT x, y, nonzero = 0, total = desc.Width * desc.Height;
                DWORD sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
                BOOL bgra = desc.Format == 87 || desc.Format == 91;
                char name[80];
                for (y = 0; y < desc.Height; ++y) {
                    const BYTE *row = (const BYTE *)mapped.data + y * mapped.row_pitch;
                    for (x = 0; x < desc.Width; ++x) {
                        BYTE r = bgra ? row[x * 4 + 2] : row[x * 4], g = row[x * 4 + 1], b = bgra ? row[x * 4] : row[x * 4 + 2], a = row[x * 4 + 3];
                        sum_r += r; sum_g += g; sum_b += b; sum_a += a;
                        if (a) ++nonzero;
                    }
                }
                if (total == 0) total = 1;
                x104_logf("X104 dump[%s] %s: pixels with alpha>0: %u of %u (%u%%); average RGBA = %u,%u,%u,%u", tag, what, nonzero, total, (nonzero * 100u) / total, sum_r / total, sum_g / total,
                          sum_b / total, sum_a / total);
                wsprintfA(name, "x104_dump_%s_%s_%ld.bmp", tag, what[0] == 'b' ? "backbuffer" : "tex", (long)number);
                x104_logf("X104 dump[%s] %s -> %s (%s)", tag, what, name, x104_panel_dd_write_bmp(name, (const BYTE *)mapped.data, desc.Width, desc.Height, mapped.row_pitch, bgra) ? "written" : "WRITE FAILED");
            }
            X104_VFN(context, 15, void (X104_API *)(void *, void *, UINT))(context, staging, 0);
        } while (0);
    } __except (x104_seh_filter((EXCEPTION_POINTERS *)_exception_info(), &seh)) {
        x104_logf("X104 dump[%s] %s: EXCEPTION while dumping (code 0x%08lx)", tag, what, (unsigned long)seh.code);
    }
    x104_panel_dd_release(staging);
    x104_panel_dd_release(resource);
}

typedef struct x104_panel_bd_t { UINT ByteWidth, Usage, BindFlags, CPUAccess, Misc, StructureByteStride; } x104_panel_bd_t;

static void x104_panel_dd_fmt(char *out, float value)               /* wsprintfA has no %f */
{
    BOOL negative = FALSE;
    UINT whole, frac;
    if (value != value) { out[0] = 'n'; out[1] = 'a'; out[2] = 'n'; out[3] = 0; return; }
    if (value > 1.0e9f || value < -1.0e9f) { out[0] = value > 0.0f ? '+' : '-'; out[1] = 'b'; out[2] = 'i'; out[3] = 'g'; out[4] = 0; return; }
    if (value < 0.0f) { negative = TRUE; value = -value; }
    whole = (UINT)value;
    frac = (UINT)((value - (float)whole) * 1000.0f + 0.5f);
    if (frac >= 1000u) { ++whole; frac -= 1000u; }
    wsprintfA(out, "%s%u.%03u", negative ? "-" : "", whole, frac);
}

/* log nbytes of a buffer starting at byte_offset through a staging copy; mode 'f' = floats (group per line), 'h' = 16-bit indices, 'w' = 32-bit indices */
static void x104_panel_tr_dump_buffer(const char *tag, const char *what, void *buffer, void *device, void *context, UINT byte_offset, UINT nbytes, char mode, UINT group)
{
    void *staging = NULL;
    x104_panel_bd_t desc, sdesc;
    x104_panel_dm_t mapped;
    HRESULT hr;
    X104Seh seh;
    memset(&seh, 0, sizeof(seh));
    memset(&desc, 0, sizeof(desc));
    if (!buffer || IsBadReadPtr(buffer, 4) || !device || !context) {
        x104_logf("X104 dump[%s] %s: no buffer (buffer=%p)", tag, what, buffer);
        return;
    }
    __try {
        do {
            X104_VFN(buffer, 10, void (X104_API *)(void *, x104_panel_bd_t *))(buffer, &desc);
            x104_logf("X104 dump[%s] %s buffer=%p: ByteWidth=%u usage=%u bind=0x%x cpu=0x%x; showing %u bytes from offset %u", tag, what, buffer, desc.ByteWidth, desc.Usage, desc.BindFlags,
                      desc.CPUAccess, nbytes, byte_offset);
            if (byte_offset >= desc.ByteWidth) { x104_logf("X104 dump[%s] %s: offset beyond the buffer", tag, what); break; }
            if (nbytes > desc.ByteWidth - byte_offset) nbytes = desc.ByteWidth - byte_offset;
            sdesc = desc;
            sdesc.Usage = 3;
            sdesc.BindFlags = 0;
            sdesc.CPUAccess = 0x20000u;
            sdesc.Misc = 0;
            hr = X104_VFN(device, 3, HRESULT (X104_API *)(void *, const x104_panel_bd_t *, const void *, void **))(device, &sdesc, NULL, &staging);
            if (FAILED(hr) || !staging) { x104_logf("X104 dump[%s] %s: CreateBuffer(staging) FAILED hr=0x%08lx", tag, what, (unsigned long)hr); break; }
            X104_VFN(context, 47, void (X104_API *)(void *, void *, void *))(context, staging, buffer);
            memset(&mapped, 0, sizeof(mapped));
            hr = X104_VFN(context, 14, HRESULT (X104_API *)(void *, void *, UINT, UINT, UINT, x104_panel_dm_t *))(context, staging, 0, 1u, 0, &mapped);
            if (FAILED(hr) || !mapped.data) { x104_logf("X104 dump[%s] %s: Map(staging) FAILED hr=0x%08lx", tag, what, (unsigned long)hr); break; }
            {
                const BYTE *data = (const BYTE *)mapped.data + byte_offset;
                char line[420];
                UINT i, n, per_line, used;
                if (mode == 'f') {
                    n = nbytes / 4u;
                    per_line = group ? group : 4u;
                    if (per_line > 12u) per_line = 12u;
                    for (i = 0; i < n; i += per_line) {
                        UINT j;
                        used = 0;
                        for (j = 0; j < per_line && i + j < n; ++j) {
                            char num[24];
                            x104_panel_dd_fmt(num, ((const float *)data)[i + j]);
                            used += (UINT)wsprintfA(line + used, "%s ", num);
                        }
                        x104_logf("X104 dump[%s] %s  [%03u] %s", tag, what, i * 4u, line);
                    }
                } else {
                    UINT size = mode == 'w' ? 4u : 2u;
                    n = nbytes / size;
                    per_line = 18u;
                    for (i = 0; i < n; i += per_line) {
                        UINT j;
                        used = 0;
                        for (j = 0; j < per_line && i + j < n; ++j)
                            used += (UINT)wsprintfA(line + used, "%u ", size == 4u ? ((const DWORD *)data)[i + j] : (DWORD)((const WORD *)data)[i + j]);
                        x104_logf("X104 dump[%s] %s  [%03u] %s", tag, what, i, line);
                    }
                }
            }
            X104_VFN(context, 15, void (X104_API *)(void *, void *, UINT))(context, staging, 0);
        } while (0);
    } __except (x104_seh_filter((EXCEPTION_POINTERS *)_exception_info(), &seh)) {
        x104_logf("X104 dump[%s] %s: EXCEPTION while dumping (code 0x%08lx)", tag, what, (unsigned long)seh.code);
    }
    x104_panel_dd_release(staging);
}

static void x104_panel_tr_dump_frame(const char *tag)
{
    x104_panel_tr_t *t = &x104_panel_tr;
    DWORD dr = 0, device = 0, context = 0, rtv = 0;
    void *seen[8];
    unsigned nseen = 0, index, k;
    UINT shown = t->ring_pos < X104_PANEL_TR_RING ? t->ring_pos : X104_PANEL_TR_RING;
    UINT first = t->ring_pos - shown;
    if (!x104.image || !x104_read_u32(x104.image + ARM0104_FRAME_RENDER_DEVICE_RESOURCES_GLOBAL_INSTANCE, &dr) || !dr) return;
    x104_read_u32((const void *)(ULONG_PTR)(dr + 4u), &device);
    x104_read_u32((const void *)(ULONG_PTR)(dr + 8u), &context);
    x104_read_u32((const void *)(ULONG_PTR)(dr + 0x10u), &rtv);
    x104_logf("X104 dump[%s]: begin (render thread, frame #%ld, %u UI draws, %u world draws): back buffer + the textures of the last UI draws", tag, (long)t->frames, t->cls_u_ui, t->cls_w);
    x104_panel_tr_dump_view(tag, "backbuffer", (void *)(ULONG_PTR)rtv, (void *)(ULONG_PTR)device, (void *)(ULONG_PTR)context);
    for (index = 0; index < shown; ++index) {
        const x104_panel_tr_draw_t *d = &t->ring[(first + index) % X104_PANEL_TR_RING];
        BOOL dup = FALSE;
        if (d->cls != 'U' || !d->srv0) continue;
        for (k = 0; k < nseen; ++k) if (seen[k] == d->srv0) dup = TRUE;
        if (dup || nseen >= ARRAYSIZE(seen)) continue;
        seen[nseen++] = d->srv0;
        x104_panel_tr_dump_view(tag, "tex", d->srv0, (void *)(ULONG_PTR)device, (void *)(ULONG_PTR)context);
    }
    for (index = 0; index < shown; ++index) {                      /* every draw of the ring: what did it read? (stride 0 = suspicious) */
        const x104_panel_tr_draw_t *d = &t->ring[(first + index) % X104_PANEL_TR_RING];
        x104_logf("X104 dump[%s] ring[%u] %c %c count=%u start=%u base=%d vs=%p layout=%p ps=%p tex0=%p | vb=%p stride=%u off=%u | ib=%p fmt=%u off=%u | vscb0=%p | callers(rva): vb=0x%lx ib=0x%lx draw=0x%lx", tag, index, (char)d->kind, (char)d->cls,
                  d->count, d->start, (int)d->base, d->vs, d->layout, d->ps, d->srv0, d->vb0, d->vstride, d->voffset, d->ib, d->ibfmt, d->iboffset, d->vscb0,
                  (unsigned long)((BYTE *)d->vb_caller - x104.image), (unsigned long)((BYTE *)d->ib_caller - x104.image), (unsigned long)((BYTE *)d->draw_caller - x104.image));
    }
    for (index = shown; index > 0; --index) {                     /* the last UI draw of the frame: what did it read? */
        const x104_panel_tr_draw_t *d = &t->ring[(first + index - 1) % X104_PANEL_TR_RING];
        UINT vsize, offset;
        if (d->cls != 'U') continue;
        x104_logf("X104 dump[%s] last UI draw: kind=%c count=%u start=%u base=%d vs=%p layout=%p ps=%p tex0=%p | vb=%p stride=%u offset=%u | ib=%p fmt=%u offset=%u | vscb0=%p pscb0=%p",
                  tag, (char)d->kind, d->count, d->start, (int)d->base, d->vs, d->layout, d->ps, d->srv0, d->vb0, d->vstride, d->voffset, d->ib, d->ibfmt, d->iboffset, d->vscb0, d->pscb0);
        {
            UINT k;
            for (k = 0; k < 4; ++k) {
                char label[16];
                if (d->vscb[k]) { wsprintfA(label, "vscb%u", k); x104_panel_tr_dump_buffer(tag, label, d->vscb[k], (void *)(ULONG_PTR)device, (void *)(ULONG_PTR)context, 0, 128, 'f', 4); }
                if (d->pscb[k]) { wsprintfA(label, "pscb%u", k); x104_panel_tr_dump_buffer(tag, label, d->pscb[k], (void *)(ULONG_PTR)device, (void *)(ULONG_PTR)context, 0, 64, 'f', 4); }
            }
        }
        if (d->vstride == 0u) {
            x104_panel_tr_dump_buffer(tag, "vertices(stride 0!)", d->vb0, (void *)(ULONG_PTR)device, (void *)(ULONG_PTR)context, d->voffset, 96, 'f', 6);
        } else if (d->vstride >= 4u && d->vstride <= 96u) {
            offset = d->voffset + (d->base > 0 ? (UINT)d->base * d->vstride : 0u);
            x104_panel_tr_dump_buffer(tag, "vertices", d->vb0, (void *)(ULONG_PTR)device, (void *)(ULONG_PTR)context, offset, d->vstride * 12u, 'f', d->vstride / 4u);
        }
        vsize = d->ibfmt == 42u ? 4u : 2u;
        x104_panel_tr_dump_buffer(tag, "indices", d->ib, (void *)(ULONG_PTR)device, (void *)(ULONG_PTR)context, d->iboffset + d->start * vsize, (d->count < 36u ? d->count : 36u) * vsize,
                                  vsize == 4u ? 'w' : 'h', 0);
        break;
    }
    x104_logf("X104 dump[%s]: end", tag);
}

/* render thread, from the Present stub (before the real Present): log (1 s / 10 s, and when the "frame has U draws" flag flips), then start a new frame record */
static void x104_panel_tr_frame_end(void)
{
    x104_panel_tr_t *t = &x104_panel_tr;
    DWORD now = GetTickCount();
    BOOL ui = t->cls_u_ui != 0;
    const char *why = NULL;
    ++t->frames;
    if (!t->start_tick) {
        t->start_tick = now;
        t->next_tick = now;
    }
    if (t->enabled && t->frames > 3 && ui != t->last_ui && t->flips < X104_PANEL_TR_MAX_FLIPS) {
        ++t->flips;
        why = ui ? "U-appeared" : "U-vanished";
    } else if (t->enabled && (int)(now - t->next_tick) >= 0) {
        t->next_tick = now + ((DWORD)(now - t->start_tick) < X104_PANEL_TR_DENSE_MS ? 1000u : X104_PANEL_TR_SLOW_MS);
        why = "periodic";
    }
    if (t->enabled && !t->dumped_menu && t->cls_w == 0 && t->cls_u_ui >= 15 && (DWORD)(now - t->start_tick) >= 2500u) {
        t->dumped_menu = TRUE;
        x104_panel_tr_dump_frame("menu");
    }
    if (t->cls_w >= 50 && t->cls_u_ui >= 8) ++t->world_frames;
    if (t->enabled && !t->dumped_world && t->world_frames >= 240u) {
        t->dumped_world = TRUE;
        x104_panel_tr_dump_frame("world");
    }
    if (why || (t->frames % 30) == 1) x104_panel_tr_verify_cells();
    if (why) x104_panel_tr_log_frame(why, now);
    t->last_ui = ui;
    t->draws = t->indexed = t->cls_w = t->cls_b = t->cls_u_ui = t->cls_u_flat = t->clears_rt = t->clears_ds = 0;
    t->n_rtv = t->n_vp = t->n_bs = t->n_dss = t->n_rs = t->n_sc = 0;
    t->maps = t->maps_discard = t->maps_nooverwrite = t->maps_failed = 0;
    t->ring_pos = 0;
    t->nruns = t->run_overflow = 0;
}

/* ======================================================================================================================
 * 2c. ID3D11Device::Create{ShaderResourceView,RenderTargetView,DepthStencilView,InputLayout,VertexShader,PixelShader,BlendState,DepthStencilState,RasterizerState,SamplerState}:
 *     log-only wrappers, ALWAYS ON (like the CreateBuffer / CreateTexture2D ones in section 4): a FAILED result is logged with the interesting fields of the descriptor and
 *     passed on unchanged.  Why: the game forces feature level 9_3 on a phone but runs at 9_1 here, and it ignores the HRESULT of these calls; a state / view that
 *     silently failed to exist is a classic reason for "drawn but invisible" (log: `X104 panel: CreateRasterizerState FAILED hr=... fill=3 cull=2 ...`, at most 20 per kind).
 *     Wrapped cells (ID3D11Device vtable): 7 CreateShaderResourceView, 9 CreateRenderTargetView, 10 CreateDepthStencilView, 11 CreateInputLayout, 12 CreateVertexShader,
 *     15 CreatePixelShader, 20 CreateBlendState, 21 CreateDepthStencilState, 22 CreateRasterizerState, 23 CreateSamplerState.
 * ====================================================================================================================== */
#define X104_PANEL_CF_SLOTS   24

typedef struct x104_panel_cf_sampler_t {                 /* D3D11_SAMPLER_DESC (52 bytes) */
    UINT Filter, AddressU, AddressV, AddressW;
    float MipLODBias;
    UINT MaxAnisotropy, ComparisonFunc;
    float BorderColor[4], MinLOD, MaxLOD;
} x104_panel_cf_sampler_t;
typedef struct x104_panel_cf_vt_t { void **vtbl; void *orig[X104_PANEL_CF_SLOTS]; } x104_panel_cf_vt_t;
typedef struct x104_panel_cf_t { volatile LONG count; x104_panel_cf_vt_t vts[2]; } x104_panel_cf_t;
static x104_panel_cf_t x104_panel_cf;

typedef HRESULT (X104_API *x104_panel_cf_fn_view)(void *, void *, const void *, void **);          /* (this, resource, desc, out) */
typedef HRESULT (X104_API *x104_panel_cf_fn_state)(void *, const void *, void **);                  /* (this, desc, out) */
typedef HRESULT (X104_API *x104_panel_cf_fn_shader)(void *, const void *, SIZE_T, void *, void **); /* (this, bytecode, length, linkage, out) */
typedef HRESULT (X104_API *x104_panel_cf_fn_layout)(void *, const void *, UINT, const void *, SIZE_T, void **);

static void *x104_panel_cf_orig(void *self, int slot)
{
    void **vtbl = self ? *(void ***)self : NULL;
    LONG count = x104_panel_cf.count;
    LONG index;
    if (slot < 0 || slot >= X104_PANEL_CF_SLOTS) return NULL;
    for (index = 0; index < count && index < 2; ++index)
        if (x104_panel_cf.vts[index].vtbl == vtbl) return x104_panel_cf.vts[index].orig[slot];
    return count > 0 ? x104_panel_cf.vts[0].orig[slot] : NULL;
}

static BOOL x104_panel_cf_readable(const void *pointer, SIZE_T size)
{
    return pointer && !IsBadReadPtr(pointer, size);
}

static HRESULT X104_API x104_panel_cf_create_srv(void *self, void *resource, const void *desc, void **view)
{
    static volatile LONG lines;
    x104_panel_cf_fn_view original = (x104_panel_cf_fn_view)x104_panel_cf_orig(self, 7);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, resource, desc, view);
    if (FAILED(hr)) {
        const UINT *d = x104_panel_cf_readable(desc, 8) ? (const UINT *)desc : NULL;
        X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateShaderResourceView FAILED hr=0x%08lx %s resource=%p format=%u dimension=%u", (unsigned long)hr, x104_hr_name(hr), resource, d ? d[0] : 0u, d ? d[1] : 0u);
    }
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_rtv(void *self, void *resource, const void *desc, void **view)
{
    static volatile LONG lines;
    x104_panel_cf_fn_view original = (x104_panel_cf_fn_view)x104_panel_cf_orig(self, 9);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, resource, desc, view);
    if (FAILED(hr)) {
        const UINT *d = x104_panel_cf_readable(desc, 8) ? (const UINT *)desc : NULL;
        X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateRenderTargetView FAILED hr=0x%08lx %s resource=%p format=%u dimension=%u", (unsigned long)hr, x104_hr_name(hr), resource, d ? d[0] : 0u, d ? d[1] : 0u);
    }
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_dsv(void *self, void *resource, const void *desc, void **view)
{
    static volatile LONG lines;
    x104_panel_cf_fn_view original = (x104_panel_cf_fn_view)x104_panel_cf_orig(self, 10);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, resource, desc, view);
    if (FAILED(hr)) {
        const UINT *d = x104_panel_cf_readable(desc, 12) ? (const UINT *)desc : NULL;
        X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateDepthStencilView FAILED hr=0x%08lx %s resource=%p format=%u dimension=%u flags=%u", (unsigned long)hr, x104_hr_name(hr), resource, d ? d[0] : 0u,
                         d ? d[1] : 0u, d ? d[2] : 0u);
    }
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_layout(void *self, const void *elements, UINT count, const void *code, SIZE_T length, void **layout)
{
    static volatile LONG lines;
    x104_panel_cf_fn_layout original = (x104_panel_cf_fn_layout)x104_panel_cf_orig(self, 11);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, elements, count, code, length, layout);
    if (FAILED(hr)) X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateInputLayout FAILED hr=0x%08lx %s elements=%u bytecode=%p (%lu bytes)", (unsigned long)hr, x104_hr_name(hr), count, code, (unsigned long)length);
    else if (elements && count && layout && *layout) {
        x104_panel_ls_add(*layout, elements, count);
    }
    if (SUCCEEDED(hr) && elements && count && count <= 12u && layout && *layout) {
        /* D3D11_INPUT_ELEMENT_DESC = { const char *name; UINT index, format, slot, offset, slotClass, stepRate } (28 bytes): one line per layout, to match the layout pointer of a draw */
        static volatile LONG logged;
        if ((LONG)__sync_add_and_fetch(&logged, 1) <= 80 && !IsBadReadPtr(elements, count * 28u)) {
            const DWORD *e = (const DWORD *)elements;
            char line[420];
            UINT i, used = 0;
            for (i = 0; i < count && used + 48u < sizeof(line); ++i) {
                const char *name = (const char *)(ULONG_PTR)e[i * 7u];
                used += (UINT)wsprintfA(line + used, "[%s %u fmt%u slot%u off%u cls%u] ", (name && !IsBadReadPtr(name, 2)) ? name : "?", e[i * 7u + 1], e[i * 7u + 2], e[i * 7u + 3], e[i * 7u + 4], e[i * 7u + 5]);
            }
            x104_logf("X104 panel: CreateInputLayout ok layout=%p vs-bytecode=%p (%lu bytes) %u elements: %s", *layout, code, (unsigned long)length, count, line);
        }
    }
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_vs(void *self, const void *code, SIZE_T length, void *linkage, void **shader)
{
    static volatile LONG lines;
    x104_panel_cf_fn_shader original = (x104_panel_cf_fn_shader)x104_panel_cf_orig(self, 12);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, code, length, linkage, shader);
    if (FAILED(hr)) X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateVertexShader FAILED hr=0x%08lx %s bytecode=%p (%lu bytes)", (unsigned long)hr, x104_hr_name(hr), code, (unsigned long)length);
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_ps(void *self, const void *code, SIZE_T length, void *linkage, void **shader)
{
    static volatile LONG lines;
    x104_panel_cf_fn_shader original = (x104_panel_cf_fn_shader)x104_panel_cf_orig(self, 15);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, code, length, linkage, shader);
    if (FAILED(hr)) X104_LOG_LIMITED(&lines, 20, "X104 panel: CreatePixelShader FAILED hr=0x%08lx %s bytecode=%p (%lu bytes)", (unsigned long)hr, x104_hr_name(hr), code, (unsigned long)length);
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_blend(void *self, const void *desc, void **state)
{
    static volatile LONG lines;
    x104_panel_cf_fn_state original = (x104_panel_cf_fn_state)x104_panel_cf_orig(self, 20);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, desc, state);
    if (FAILED(hr)) {
        const x104_panel_tr_blend_t *d = x104_panel_cf_readable(desc, sizeof(*d)) ? (const x104_panel_tr_blend_t *)desc : NULL;
        if (d)
            X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateBlendState FAILED hr=0x%08lx %s alphaToCoverage=%u independent=%u rt0: blend=%u src=%u dst=%u op=%u srcAlpha=%u dstAlpha=%u opAlpha=%u mask=%x",
                             (unsigned long)hr, x104_hr_name(hr), d->AlphaToCoverageEnable, d->IndependentBlendEnable, d->rt[0].BlendEnable, d->rt[0].SrcBlend, d->rt[0].DestBlend, d->rt[0].BlendOp,
                             d->rt[0].SrcBlendAlpha, d->rt[0].DestBlendAlpha, d->rt[0].BlendOpAlpha, (unsigned)d->rt[0].WriteMask);
        else
            X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateBlendState FAILED hr=0x%08lx %s (desc %p unreadable)", (unsigned long)hr, x104_hr_name(hr), desc);
    }
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_depth(void *self, const void *desc, void **state)
{
    static volatile LONG lines;
    x104_panel_cf_fn_state original = (x104_panel_cf_fn_state)x104_panel_cf_orig(self, 21);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, desc, state);
    if (FAILED(hr)) {
        const x104_panel_tr_depth_t *d = x104_panel_cf_readable(desc, sizeof(*d)) ? (const x104_panel_tr_depth_t *)desc : NULL;
        if (d)
            X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateDepthStencilState FAILED hr=0x%08lx %s depth=%u write=%u func=%u stencil=%u readMask=%x writeMask=%x front: fail=%u depthFail=%u pass=%u func=%u",
                             (unsigned long)hr, x104_hr_name(hr), d->DepthEnable, d->DepthWriteMask, d->DepthFunc, d->StencilEnable, (unsigned)d->ReadMask, (unsigned)d->WriteMask, d->front[0], d->front[1],
                             d->front[2], d->front[3]);
        else
            X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateDepthStencilState FAILED hr=0x%08lx %s (desc %p unreadable)", (unsigned long)hr, x104_hr_name(hr), desc);
    }
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_raster(void *self, const void *desc, void **state)
{
    static volatile LONG lines;
    x104_panel_cf_fn_state original = (x104_panel_cf_fn_state)x104_panel_cf_orig(self, 22);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, desc, state);
    if (FAILED(hr)) {
        const x104_panel_tr_raster_t *d = x104_panel_cf_readable(desc, sizeof(*d)) ? (const x104_panel_tr_raster_t *)desc : NULL;
        if (d)
            X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateRasterizerState FAILED hr=0x%08lx %s fill=%u cull=%u frontCCW=%d depthBias=%d slopeBias=%dm depthClip=%d scissor=%d multisample=%d aaLine=%d",
                             (unsigned long)hr, x104_hr_name(hr), d->FillMode, d->CullMode, (int)d->FrontCounterClockwise, (int)d->DepthBias, (int)(d->SlopeScaledDepthBias * 1000.0f),
                             (int)d->DepthClipEnable, (int)d->ScissorEnable, (int)d->MultisampleEnable, (int)d->AntialiasedLineEnable);
        else
            X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateRasterizerState FAILED hr=0x%08lx %s (desc %p unreadable)", (unsigned long)hr, x104_hr_name(hr), desc);
    }
    return hr;
}

static HRESULT X104_API x104_panel_cf_create_sampler(void *self, const void *desc, void **state)
{
    static volatile LONG lines;
    x104_panel_cf_fn_state original = (x104_panel_cf_fn_state)x104_panel_cf_orig(self, 23);
    HRESULT hr;
    if (!original) return E_FAIL;
    hr = original(self, desc, state);
    if (FAILED(hr)) {
        const x104_panel_cf_sampler_t *d = x104_panel_cf_readable(desc, sizeof(*d)) ? (const x104_panel_cf_sampler_t *)desc : NULL;
        if (d)
            X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateSamplerState FAILED hr=0x%08lx %s filter=%u address=%u/%u/%u mipBias=%dm maxAniso=%u comparison=%u minLod=%dm maxLod=%dm", (unsigned long)hr,
                             x104_hr_name(hr), d->Filter, d->AddressU, d->AddressV, d->AddressW, (int)(d->MipLODBias * 1000.0f), d->MaxAnisotropy, d->ComparisonFunc, (int)(d->MinLOD * 1000.0f),
                             (int)(d->MaxLOD * 1000.0f));
        else
            X104_LOG_LIMITED(&lines, 20, "X104 panel: CreateSamplerState FAILED hr=0x%08lx %s (desc %p unreadable)", (unsigned long)hr, x104_hr_name(hr), desc);
    }
    return hr;
}

/* patch the 10 cells of one ID3D11Device vtable (idempotent per vtable; the originals are stored before a cell goes live) */
static void x104_panel_cf_install(void *device_iface, const char *which)
{
    static const struct { int slot; void *wrapper; const char *name; } table[] = {
        { 7,  (void *)x104_panel_cf_create_srv,     "CreateShaderResourceView" },
        { 9,  (void *)x104_panel_cf_create_rtv,     "CreateRenderTargetView" },
        { 10, (void *)x104_panel_cf_create_dsv,     "CreateDepthStencilView" },
        { 11, (void *)x104_panel_cf_create_layout,  "CreateInputLayout" },
        { 12, (void *)x104_panel_cf_create_vs,      "CreateVertexShader" },
        { 15, (void *)x104_panel_cf_create_ps,      "CreatePixelShader" },
        { 20, (void *)x104_panel_cf_create_blend,   "CreateBlendState" },
        { 21, (void *)x104_panel_cf_create_depth,   "CreateDepthStencilState" },
        { 22, (void *)x104_panel_cf_create_raster,  "CreateRasterizerState" },
        { 23, (void *)x104_panel_cf_create_sampler, "CreateSamplerState" }
    };
    void **vtbl;
    LONG index;
    unsigned entry;
    unsigned patched = 0;
    if (!device_iface) return;
    vtbl = *(void ***)device_iface;
    if (!vtbl || IsBadReadPtr(vtbl, X104_PANEL_CF_SLOTS * sizeof(void *))) {
        x104_logf("X104 panel: ERROR d3d11 device vtable %p (%s) unreadable - the Create* failure wrappers are not installed", (void *)vtbl, which);
        return;
    }
    for (index = 0; index < x104_panel_cf.count && index < 2; ++index)
        if (x104_panel_cf.vts[index].vtbl == vtbl) return;
    if (x104_panel_cf.count >= 2) {
        x104_logf("X104 panel: d3d11 device vtable %p (%s): two vtables already carry the Create* failure wrappers - skipped", (void *)vtbl, which);
        return;
    }
    index = x104_panel_cf.count;
    x104_panel_cf.vts[index].vtbl = vtbl;
    for (entry = 0; entry < ARRAYSIZE(table); ++entry) x104_panel_cf.vts[index].orig[table[entry].slot] = vtbl[table[entry].slot];
    __sync_synchronize();
    x104_panel_cf.count = index + 1;
    for (entry = 0; entry < ARRAYSIZE(table); ++entry)
        if (arm32_patch_cell((void **)&vtbl[table[entry].slot], x104_panel_cf.vts[index].orig[table[entry].slot], table[entry].wrapper, NULL, table[entry].name)) ++patched;
    x104_logf("X104 panel: d3d11 device vtable %p (%s): %u of %u Create* failure-logging wrappers installed (views, shaders, input layout, blend / depth-stencil / rasterizer / sampler states)", (void *)vtbl,
              which, patched, (unsigned)ARRAYSIZE(table));
}

/* ======================================================================================================================
 * 3. game-image patches: shader profile string, resolution float, cwd / include
 * ====================================================================================================================== */
/* the digit of "_level_9_?" ('1'..'3'), 0 when the 11 bytes at rva 0x2351F0 are not "_level_9_<digit>\0" */
static char x104_panel_profile_digit(void)
{
    static const BYTE prefix[9] = { 0x5f, 0x6c, 0x65, 0x76, 0x65, 0x6c, 0x5f, 0x39, 0x5f };     /* "_level_9_" */
    const BYTE *text = (const BYTE *)X104_DATA(X104_RVA_PROFILE_STRING);
    if (IsBadReadPtr(text, 11)) return 0;
    if (memcmp(text, prefix, sizeof(prefix)) != 0 || text[10] != 0) return 0;
    return (text[9] >= '1' && text[9] <= '3') ? (char)text[9] : 0;
}

/* make the profile string end in `wanted` ('1' / '2' / '3').  Idempotent; also called from the D3D11CreateDevice hook when the real level differs from the cap. */
static BOOL x104_panel_set_profile_digit(char wanted, const char *why)
{
    BYTE *address = (BYTE *)X104_DATA(X104_RVA_PROFILE_DIGIT);
    char current = x104_panel_profile_digit();
    if (!current) {
        x104_check_game_bytes(X104_RVA_PROFILE_STRING, ARM0104_FRAME_RENDER_SHADER_FEATURE_LEVEL_LIE_EXPECTED, 11, "shader profile string (_level_9_x)");
        x104_logf("X104 panel: ERROR profile string at rva %08lx is not \"_level_9_<1|2|3>\" - cannot select the digit '%c' (%s)", (unsigned long)X104_RVA_PROFILE_STRING,
                  wanted, why ? why : "?");
        return FALSE;
    }
    if (current == wanted) {
        x104_logf("X104 panel: profile string already \"_level_9_%c\" (%02x, %s) - nothing to patch", wanted, (unsigned)(BYTE)wanted, why ? why : "?");
        return TRUE;
    }
    if (!arm32_patch_byte(address, (BYTE)current, (BYTE)wanted, "shader profile string digit (rva 0x2351F9)")) {
        x104_logf("X104 panel: ERROR profile digit patch %02x -> %02x failed (%s)", (unsigned)(BYTE)current, (unsigned)(BYTE)wanted, why ? why : "?");
        return FALSE;
    }
    x104_logf("X104 panel: profile string patched %02x -> %02x (\"_level_9_%c\", %s)", (unsigned)(BYTE)current, (unsigned)(BYTE)wanted, wanted, why ? why : "?");
    return TRUE;
}

static BOOL x104_panel_install_profile(UINT cap)
{
    char digit = x104_panel_profile_digit();
    char wanted = 0;
    if (!digit) {
        x104_check_game_bytes(X104_RVA_PROFILE_STRING, ARM0104_FRAME_RENDER_SHADER_FEATURE_LEVEL_LIE_EXPECTED, 11, "shader profile string (_level_9_x)");
        x104_logf("X104 panel: ERROR the shader profile string at rva %08lx is neither \"_level_9_3\" (stock) nor \"_level_9_1/2\" (patched): wrong exe? (exit code 33)",
                  (unsigned long)X104_RVA_PROFILE_STRING);
        return FALSE;
    }
    if (digit == '1') x104_logf("X104 panel: profile string already patched by the exe patcher (0x31) - \"_level_9_1\" verified");
    else if (digit == '3') x104_logf("X104 panel: profile string verified: stock \"_level_9_3\" (0x33)");
    else x104_logf("X104 panel: profile string verified: \"_level_9_%c\"", digit);
    if (cap == 0x9100u) wanted = '1';
    else if (cap == 0x9200u) wanted = '2';
    else if (cap == 0x9300u) wanted = '3';
    if (wanted) {
        if (!x104_panel_set_profile_digit(wanted, "feature level cap")) return FALSE;
    } else {
        x104_logf("X104 panel: feature level cap 0x%04X: the profile digit stays '%c' until the device level is known (the D3D11CreateDevice hook adapts it)", (unsigned)cap, digit);
    }
    /* getShaderModelSuffix (0xD73FC): the code that reads the string - warning only */
    if (!x104_check_game_bytes(ARM0104_FRAME_RENDER_SHADER_PROFILE_SELECTION, ARM0104_FRAME_RENDER_SHADER_PROFILE_SELECTION_EXPECTED, 12, "getShaderModelSuffix (0xD73FC)"))
        x104_logf("X104 panel: WARNING getShaderModelSuffix bytes differ from the verified ones (see the ARM32 check line above) - the profile patch may not match this exe");
    else
        x104_logf("X104 panel: getShaderModelSuffix bytes verified");
    return TRUE;
}

static void x104_panel_install_resolution(void)
{
    static const BYTE half[4] = { 0x00, 0x00, 0x00, 0x3f };      /* 0.5f */
    static const BYTE full[4] = { 0x00, 0x00, 0x80, 0x3f };      /* 1.0f */
    BYTE *address = (BYTE *)X104_DATA(X104_RVA_RESOLUTION_FLOAT);
    if (IsBadReadPtr(address, 4)) {
        x104_logf("X104 panel: ERROR resolution float at rva %08lx is unreadable", (unsigned long)X104_RVA_RESOLUTION_FLOAT);
        return;
    }
    if (x104.opt_half_res) {
        if (memcmp(address, half, 4) != 0)
            x104_logf("X104 panel: WARNING --half-res: the Default GPU row scale at rva %08lx is %02x %02x %02x %02x (expected 00 00 00 3f)", (unsigned long)X104_RVA_RESOLUTION_FLOAT,
                      (unsigned)address[0], (unsigned)address[1], (unsigned)address[2], (unsigned)address[3]);
        x104_logf("X104 panel: half-res kept (--half-res): the game renders at half size on >= 1280x720 and stretches (the swap chain is 1/2 of the client size)");
        return;
    }
    if (memcmp(address, half, 4) == 0) {
        if (arm32_patch_byte(address + 2, 0x00, 0x80, "Default GPU row resolution scale 0.5f -> 1.0f (rva 0xDC8856)"))
            x104_logf("X104 panel: resolution float 0.5 -> 1.0 (full resolution; --half-res keeps the game's half-resolution row)");
        else
            x104_logf("X104 panel: ERROR resolution float patch failed - the game will render at half resolution");
    } else if (memcmp(address, full, 4) == 0) {
        x104_logf("X104 panel: resolution float already 1.0 (full resolution)");
    } else {
        x104_logf("X104 panel: ERROR resolution float at rva %08lx holds %02x %02x %02x %02x (expected 00 00 00 3f) - not patched", (unsigned long)X104_RVA_RESOLUTION_FLOAT,
                  (unsigned)address[0], (unsigned)address[1], (unsigned)address[2], (unsigned)address[3]);
    }
}

/* D3DCompile resolves ShaderConstants.fxh relative to cwd + dirname(pSourceName) = <cwd>\data\shaders\dx11 */
static void x104_panel_install_cwd(void)
{
    wchar_t cwd[MAX_PATH];
    wchar_t path[MAX_PATH + 64];
    char shown[MAX_PATH];
    char shown2[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA info;
    DWORD length = GetCurrentDirectoryW(MAX_PATH, cwd);
    if (!length || length >= MAX_PATH) {
        x104_logf("X104 panel: WARNING GetCurrentDirectoryW failed (length %lu, error %lu) - forcing cwd to the game root", (unsigned long)length, (unsigned long)GetLastError());
        cwd[0] = 0;
    }
    x104_narrow(shown, sizeof(shown), cwd);
    x104_narrow(shown2, sizeof(shown2), x104.game_root);
    if (!x104.game_root[0]) {
        x104_logf("X104 panel: ERROR x104.game_root is empty (cwd is \"%s\") - the shader include directory cannot be verified", shown);
        return;
    }
    if (x104_panel_path_equals(cwd, x104.game_root)) {
        x104_logf("X104 panel: cwd ok (\"%s\")", shown);
    } else if (SetCurrentDirectoryW(x104.game_root)) {
        x104_logf("X104 panel: cwd was \"%s\", set to the game root \"%s\"", shown, shown2);
    } else {
        x104_logf("X104 panel: ERROR cwd is \"%s\" and SetCurrentDirectoryW(\"%s\") failed, error=%lu - D3DCompile will not find ShaderConstants.fxh", shown, shown2,
                  (unsigned long)GetLastError());
    }
    lstrcpynW(path, x104.game_root, MAX_PATH);
    if (lstrlenW(path) && path[lstrlenW(path) - 1] != L'\\' && path[lstrlenW(path) - 1] != L'/') lstrcatW(path, L"\\");
    lstrcatW(path, L"data\\shaders\\dx11\\ShaderConstants.fxh");
    x104_narrow(shown, sizeof(shown), path);
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &info) && !(info.dwFileAttributes & 0x10u))
        x104_logf("X104 panel: shader include file present (%s, %lu bytes)", shown, (unsigned long)info.nFileSizeLow);
    else
        x104_logf("X104 panel: ERROR the shader include file is MISSING: %s (error %lu) - every shader compile will fail with X1507", shown, (unsigned long)GetLastError());
}

/* ======================================================================================================================
 * 4. vtable-cell patches: dxgi factory, swap chain, d3d11 device
 * ====================================================================================================================== */
static x104_panel_factory_vt_t *x104_panel_find_factory_vt(void **vtbl)
{
    LONG count = x104_panel_state.factory_count;
    LONG index;
    for (index = 0; index < count && index < X104_PANEL_MAX_VT; ++index)
        if (x104_panel_state.factories[index].vtbl == vtbl) return &x104_panel_state.factories[index];
    return NULL;
}

static x104_panel_chain_vt_t *x104_panel_find_chain_vt(void **vtbl)
{
    LONG count = x104_panel_state.chain_count;
    LONG index;
    for (index = 0; index < count && index < X104_PANEL_MAX_VT; ++index)
        if (x104_panel_state.chains[index].vtbl == vtbl) return &x104_panel_state.chains[index];
    return NULL;
}

static x104_panel_device_vt_t *x104_panel_find_device_vt(void **vtbl)
{
    LONG count = x104_panel_state.device_vt_count;
    LONG index;
    for (index = 0; index < count && index < X104_PANEL_MAX_VT; ++index)
        if (x104_panel_state.devices[index].vtbl == vtbl) return &x104_panel_state.devices[index];
    return NULL;
}

/* ---- IDXGISwapChain::Present stub (slot 8) and SetMatrixTransform stub (slot 34) ---- */
static HRESULT X104_API x104_panel_present_stub(void *self, UINT sync, UINT flags)
{
    x104_panel_chain_vt_t *entry = self ? x104_panel_find_chain_vt(*(void ***)self) : NULL;
    x104_panel_present_fn original;
    HRESULT hr;
    if (!entry && x104_panel_state.chain_count > 0) entry = &x104_panel_state.chains[0];
    original = entry ? (x104_panel_present_fn)entry->orig_present : NULL;
    if (!original) {
        static volatile LONG logged;
        X104_LOG_LIMITED(&logged, 5, "X104 panel: ERROR Present stub called for %p but no original Present is known (vtable %p) - returning E_FAIL", self,
                         self ? (void *)*(void ***)self : NULL);
        return E_FAIL;
    }
    if (x104_panel_tr.installed) x104_panel_tr_frame_end();       /* --trace-game-state: the frame's draw record (off by default) */
    x104_present_begin();
    sync = x104_present_sync(sync);
    x104_present_pace();
    hr = original(self, sync, flags);
    x104_present_end(hr, sync, flags);
    return hr;
}

static HRESULT X104_API x104_panel_set_matrix_transform_ok(void *self, const void *matrix)
{
    static volatile LONG calls;
    (void)matrix;
    X104_LOG_LIMITED(&calls, 6, "X104 panel: IDXGISwapChain2::SetMatrixTransform(%p) on chain %p -> S_OK (HWND chain: the composition transform does not exist; the swap-chain buffer is stretched by DXGI)",
                     matrix, self);
    return S_OK;
}

/* patch Present on one vtable (and remember the original); returns TRUE when the cell now holds the stub */
static BOOL x104_panel_patch_present_vtable(void **vtbl, const char *why)
{
    void *original;
    LONG index;
    if (!vtbl || IsBadReadPtr(vtbl, 9 * sizeof(void *))) {
        x104_logf("X104 panel: ERROR swap chain vtable %p unreadable (%s) - Present not patched", (void *)vtbl, why);
        return FALSE;
    }
    original = vtbl[X104_PANEL_DXGI_SWAPCHAIN_PRESENT];
    if (original == (void *)x104_panel_present_stub) return x104_panel_find_chain_vt(vtbl) != NULL;
    x104_lock(&x104_panel_state.chain_lock);
    index = x104_panel_state.chain_count;
    if (index >= X104_PANEL_MAX_VT) {
        x104_unlock(&x104_panel_state.chain_lock);
        x104_logf("X104 panel: ERROR %d swap chain vtables are already patched - vtable %p (%s) NOT patched, its frames are not counted", (int)X104_PANEL_MAX_VT, (void *)vtbl, why);
        return FALSE;
    }
    x104_panel_state.chains[index].vtbl = vtbl;
    x104_panel_state.chains[index].orig_present = original;
    x104_panel_state.chains[index].orig_set_matrix = NULL;      /* slot 34 exists only on an IDXGISwapChain2 vtable: never read it here (the vtable may be shorter) */
    __sync_synchronize();
    x104_panel_state.chain_count = index + 1;
    x104_unlock(&x104_panel_state.chain_lock);
    if (!arm32_patch_cell((void **)&vtbl[X104_PANEL_DXGI_SWAPCHAIN_PRESENT], original, (void *)x104_panel_present_stub, NULL, "IDXGISwapChain vtable cell 8 (Present)")) {
        x104_logf("X104 panel: ERROR IDXGISwapChain vtable %p cell 8 (Present) could NOT be patched (%s) - frames are not counted", (void *)vtbl, why);
        return FALSE;
    }
    x104_logf("X104 panel: IDXGISwapChain vtable %p: vtable[8] (Present) %p -> stub (frame accounting / pacing / --vsync; %s)", (void *)vtbl, original, why);
    return TRUE;
}

/* patch the vtable(s) of a swap chain: Present on the IDXGISwapChain and IDXGISwapChain2 vtable, SetMatrixTransform (only when asked, HWND chains) on the IDXGISwapChain2 vtable */
static void x104_panel_patch_chain(void *chain, BOOL patch_matrix, const char *why)
{
    void *chain2 = NULL;
    void **vtbl1;
    void **vtbl2 = NULL;
    HRESULT hr;
    if (!chain) return;
    vtbl1 = *(void ***)chain;
    hr = x104_raw_qi(chain, &x104_panel_iid_dxgi_swapchain2, &chain2);
    if (SUCCEEDED(hr) && chain2) {
        vtbl2 = *(void ***)chain2;
        x104_logf("X104 panel: swap chain %p (%s): IDXGISwapChain vtable %p, IDXGISwapChain2 %p vtable %p%s", chain, why, (void *)vtbl1, chain2, (void *)vtbl2,
                  vtbl1 == vtbl2 ? " (the same vtable)" : " (different vtables)");
    } else {
        x104_logf("X104 panel: WARNING QI(IDXGISwapChain2) on %p failed, hr=0x%08lx %s - SetMatrixTransform (slot 34) cannot be patched; IDXGISwapChain vtable %p", chain,
                  (unsigned long)hr, x104_hr_name(hr), (void *)vtbl1);
    }
    x104_panel_patch_present_vtable(vtbl1, why);
    if (vtbl2 && vtbl2 != vtbl1) x104_panel_patch_present_vtable(vtbl2, why);
    if (patch_matrix && vtbl2) {
        void *old = vtbl2[X104_PANEL_DXGI_SWAPCHAIN2_SET_MATRIX_TRANSFORM];
        if (old == (void *)x104_panel_set_matrix_transform_ok) {
            x104_logf("X104 panel: IDXGISwapChain2 vtable %p: vtable[34] already the stub", (void *)vtbl2);
        } else if (arm32_patch_cell((void **)&vtbl2[X104_PANEL_DXGI_SWAPCHAIN2_SET_MATRIX_TRANSFORM], old, (void *)x104_panel_set_matrix_transform_ok, NULL,
                                    "IDXGISwapChain2 vtable cell 34 (SetMatrixTransform)")) {
            x104_logf("X104 panel: IDXGISwapChain2 vtable[34] %p -> stub (SetMatrixTransform returns S_OK; an HWND chain would answer 0x887A0001) on vtable %p", old, (void *)vtbl2);
        } else {
            x104_logf("X104 panel: ERROR IDXGISwapChain2 vtable %p cell 34 (SetMatrixTransform) could NOT be patched - the game will throw DXGI_ERROR_INVALID_CALL from its resize handler",
                      (void *)vtbl2);
        }
    }
    if (chain2) x104_raw_release(chain2);
}

/* ---- IDXGIFactory2::CreateSwapChainForComposition (slot 24) -> CreateSwapChainForHwnd on the game window ---- */
static void x104_panel_log_swap_desc(const char *what, const x104_panel_swap_desc1_t *desc)
{
    x104_logf("X104 panel: %s: %ux%u fmt=%u stereo=%d samples=%u/%u usage=%08lx buffers=%u scaling=%u effect=%u alpha=%u flags=%08lx", what, desc->Width, desc->Height, desc->Format,
              (int)desc->Stereo, desc->SampleCount, desc->SampleQuality, (unsigned long)desc->BufferUsage, desc->BufferCount, desc->Scaling, desc->SwapEffect, desc->AlphaMode,
              (unsigned long)desc->Flags);
}

static HRESULT X104_API x104_panel_create_swapchain_for_composition(void *factory, void *device, const x104_panel_swap_desc1_t *desc_in, void *restrict_to_output, void **swapchain)
{
    x104_panel_factory_vt_t *entry = factory ? x104_panel_find_factory_vt(*(void ***)factory) : NULL;
    x104_panel_create_hwnd_chain_fn create_hwnd;
    x104_panel_create_comp_chain_fn create_composition;
    x104_panel_swap_desc1_t desc;
    static const char labels[4] = { 'a', 'b', 'c', 'd' };
    HRESULT hr = E_FAIL;
    HRESULT first_hr = S_OK;
    void *chain = NULL;
    HWND window = x104.window;
    unsigned attempt;
    char used = 0;

    if (!entry && x104_panel_state.factory_count > 0) {
        entry = &x104_panel_state.factories[0];
        x104_logf("X104 panel: WARNING CreateSwapChainForComposition stub called for factory %p whose vtable %p is not in the patched list - using the first entry", factory,
                  factory ? (void *)*(void ***)factory : NULL);
    }
    if (!swapchain || !desc_in || !entry) {
        x104_logf("X104 panel: ERROR CreateSwapChainForComposition stub: swapchain=%p desc=%p vtable entry=%p - returning E_INVALIDARG", (void *)swapchain, (const void *)desc_in, (void *)entry);
        return E_INVALIDARG;
    }
    *swapchain = NULL;
    create_hwnd = (x104_panel_create_hwnd_chain_fn)entry->orig_for_hwnd;
    create_composition = (x104_panel_create_comp_chain_fn)entry->orig_for_composition;
    desc = *desc_in;
    x104_logf("X104 panel: CreateSwapChainForComposition intercepted (factory %p device %p output %p out %p, window %p)", factory, device, restrict_to_output, (void *)swapchain, (void *)window);
    x104_panel_log_swap_desc("requested desc", &desc);
    if (!window || !IsWindow(window)) {
        x104_logf("X104 panel: ERROR the game window %p does not exist - cannot create an HWND swap chain", (void *)window);
    } else {
        for (attempt = 0; attempt < 4; ++attempt) {
            x104_panel_swap_desc1_t try_desc = desc;
            if (attempt == 1) {
                try_desc.Scaling = 1;                              /* DXGI_SCALING_NONE */
                if (x104.opt_half_res)
                    x104_logf("X104 panel: ERROR attempt b uses DXGI_SCALING_NONE but --half-res makes the buffer (%ux%u) smaller than the client (%ld x %ld): the picture will be a corner",
                              try_desc.Width, try_desc.Height, (long)x104_panel_state.client_w, (long)x104_panel_state.client_h);
            } else if (attempt == 2) {
                try_desc.SwapEffect = 0;                           /* DXGI_SWAP_EFFECT_DISCARD (blt model), STRETCH */
            } else if (attempt == 3) {
                try_desc.SwapEffect = 0;
                try_desc.BufferCount = 1;
            }
            chain = NULL;
            hr = create_hwnd(factory, device, window, &try_desc, NULL, NULL, &chain);
            if (attempt == 0) first_hr = hr;
            x104_panel_state.stub_last_hr = hr;
            x104_logf("X104 panel: SwapChain created via CreateSwapChainForHwnd attempt %c hr=%08lx size=%ux%u (%s; effect=%u scaling=%u buffers=%u) chain=%p", labels[attempt],
                      (unsigned long)hr, try_desc.Width, try_desc.Height, SUCCEEDED(hr) ? "created" : x104_hr_name(hr), try_desc.SwapEffect, try_desc.Scaling, try_desc.BufferCount, chain);
            if (SUCCEEDED(hr) && chain) {
                used = labels[attempt];
                desc = try_desc;
                break;
            }
            if (SUCCEEDED(hr) && !chain) {
                x104_logf("X104 panel: ERROR CreateSwapChainForHwnd returned success with a NULL chain at attempt %c", labels[attempt]);
                hr = E_FAIL;
            }
            if (attempt == 0) x104_panel_log_swap_desc("attempt a failed - desc was", &try_desc);
        }
    }

    if (!used) {
        /* every HWND attempt failed: hand the game the composition chain it asked for; SetSwapChain will see that the stub did not create it and attach it with path A */
        x104_logf("X104 panel: ERROR all CreateSwapChainForHwnd attempts failed (first hr=0x%08lx %s, last hr=0x%08lx %s) - FALLING BACK to the original CreateSwapChainForComposition "
                  "(path A, DirectComposition, will be attached by SetSwapChain)", (unsigned long)first_hr, x104_hr_name(first_hr), (unsigned long)hr, x104_hr_name(hr));
        chain = NULL;
        hr = create_composition(factory, device, desc_in, restrict_to_output, &chain);
        x104_panel_state.stub_last_hr = hr;
        x104_logf("X104 panel: original CreateSwapChainForComposition hr=0x%08lx %s chain=%p", (unsigned long)hr, x104_hr_name(hr), chain);
        if (SUCCEEDED(hr)) {
            *swapchain = chain;
            x104_panel_state.stub_attempt = 'z';
        }
        return hr;
    }

    /* the chain exists: no Alt-Enter, no DXGI subclassing of our window proc */
    {
        HRESULT mwa = X104_VFN(factory, X104_PANEL_DXGI_FACTORY_MAKE_WINDOW_ASSOCIATION, HRESULT (X104_API *)(void *, HWND, UINT))(factory, window, 3u);
        x104_logf("X104 panel: IDXGIFactory::MakeWindowAssociation(%p, NO_WINDOW_CHANGES|NO_ALT_ENTER) hr=0x%08lx %s", (void *)window, (unsigned long)mwa, x104_hr_name(mwa));
    }
    {
        void *identity = NULL;
        HRESULT qhr = x104_raw_qi(chain, &x104_panel_iid_dxgi_swapchain, &identity);
        void *old_chain;
        if (FAILED(qhr) || !identity) {
            x104_logf("X104 panel: WARNING QI(IDXGISwapChain) on the new chain %p failed hr=0x%08lx - SetSwapChain will compare the raw pointer", chain, (unsigned long)qhr);
            identity = chain;
        } else {
            x104_raw_release(identity);                 /* the chain keeps the object alive; only the pointer value is needed */
        }
        x104_panel_patch_chain(chain, TRUE, "chain created by the HWND stub");
        x104_raw_addref(chain);
        x104_lock(&x104_panel_state.chain_lock);
        old_chain = x104_panel_state.stub_chain;
        x104_panel_state.stub_chain = chain;
        x104_panel_state.stub_identity = identity;
        x104_panel_state.created_by_stub = TRUE;
        x104_panel_state.stub_attempt = used;
        x104_unlock(&x104_panel_state.chain_lock);
        if (old_chain) x104_raw_release(old_chain);
        x104_logf("X104 panel: HWND stub done: chain=%p identity=%p vtable=%p attempt=%c size=%ux%u (client %ld x %ld)", chain, identity, (void *)*(void ***)chain, used, desc.Width,
                  desc.Height, (long)x104_panel_state.client_w, (long)x104_panel_state.client_h);
    }
    x104_flag_set(&x104.f_swapchain_created, "f_swapchain_created (the HWND stub created the swap chain)");
    *swapchain = chain;
    return S_OK;
}

/* patch cell 24 of a Factory2 vtable (idempotent); `factory2` is any IDXGIFactory2 pointer */
static BOOL x104_panel_patch_factory_vtable(void *factory2, const char *source)
{
    void **vtbl;
    void *orig_hwnd;
    void *orig_comp;
    LONG index;
    if (!factory2) return FALSE;
    vtbl = *(void ***)factory2;
    if (!vtbl || IsBadReadPtr(vtbl, 25 * sizeof(void *))) {
        x104_logf("X104 panel: ERROR dxgi Factory2 vtable %p (%s) is unreadable", (void *)vtbl, source);
        return FALSE;
    }
    if (x104_panel_find_factory_vt(vtbl)) {
        x104_logf("X104 panel: dxgi Factory2 vtable %p (%s) is already patched", (void *)vtbl, source);
        return TRUE;
    }
    orig_hwnd = vtbl[X104_PANEL_DXGI_FACTORY_CREATE_SWAPCHAIN_FOR_HWND];
    orig_comp = vtbl[X104_PANEL_DXGI_FACTORY_CREATE_SWAPCHAIN_FOR_COMPOSITION];
    if (orig_comp == (void *)x104_panel_create_swapchain_for_composition) {
        x104_logf("X104 panel: ERROR dxgi Factory2 vtable %p (%s) holds the stub but is not in the table - cannot find CreateSwapChainForHwnd", (void *)vtbl, source);
        return FALSE;
    }
    x104_lock(&x104_panel_state.chain_lock);
    index = x104_panel_state.factory_count;
    if (index >= X104_PANEL_MAX_VT) {
        x104_unlock(&x104_panel_state.chain_lock);
        x104_logf("X104 panel: ERROR %d dxgi factory vtables are already patched - vtable %p (%s) NOT patched", (int)X104_PANEL_MAX_VT, (void *)vtbl, source);
        return FALSE;
    }
    x104_panel_state.factories[index].vtbl = vtbl;
    x104_panel_state.factories[index].orig_for_hwnd = orig_hwnd;
    x104_panel_state.factories[index].orig_for_composition = orig_comp;
    __sync_synchronize();
    x104_panel_state.factory_count = index + 1;
    if (!x104_panel_state.patched_factory_vtbl) x104_panel_state.patched_factory_vtbl = vtbl;
    x104_unlock(&x104_panel_state.chain_lock);
    if (!arm32_patch_cell((void **)&vtbl[X104_PANEL_DXGI_FACTORY_CREATE_SWAPCHAIN_FOR_COMPOSITION], orig_comp, (void *)x104_panel_create_swapchain_for_composition, NULL,
                          "IDXGIFactory2 vtable cell 24 (CreateSwapChainForComposition)")) {
        x104_logf("X104 panel: ERROR dxgi Factory2 vtable %p cell 24 could NOT be patched (%s) - the game will create a composition chain", (void *)vtbl, source);
        return FALSE;
    }
    x104_logf("X104 panel: dxgi Factory2 vtable %p: cell 24 %p -> stub (%s); CreateSwapChainForHwnd (cell 15) = %p", (void *)vtbl, orig_comp, source, orig_hwnd);
    return TRUE;
}

/* ---- ID3D11Device::CreateBuffer (slot 3) / CreateTexture2D (slot 5): diagnostics only, never change an argument or a result ---- */
static HRESULT X104_API x104_panel_create_buffer_wrapper(void *self, const x104_panel_buffer_desc_t *desc, const void *initial, void **buffer)
{
    static volatile LONG failed_count, warned_count;
    x104_panel_device_vt_t *entry = self ? x104_panel_find_device_vt(*(void ***)self) : NULL;
    x104_panel_create_buffer_fn original;
    HRESULT hr;
    if (!entry && x104_panel_state.device_vt_count > 0) entry = &x104_panel_state.devices[0];
    original = entry ? (x104_panel_create_buffer_fn)entry->orig_create_buffer : NULL;
    if (!original) return E_FAIL;
    if (desc && !IsBadReadPtr(desc, sizeof(*desc)) && (desc->BindFlags & 0x2u /* D3D11_BIND_INDEX_BUFFER */) && (desc->ByteWidth & 3u)) {
        X104_LOG_LIMITED(&warned_count, 40, "X104 panel: WARNING CreateBuffer index buffer with ByteWidth=%u (not a multiple of 4: the game maps 32-bit indices to R32_UINT) bind=%08lx usage=%u",
                         desc->ByteWidth, (unsigned long)desc->BindFlags, desc->Usage);
    }
    hr = original(self, desc, initial, buffer);
    if (FAILED(hr)) {
        if (desc && !IsBadReadPtr(desc, sizeof(*desc)))
            X104_LOG_LIMITED(&failed_count, 40, "X104 panel: CreateBuffer FAILED hr=0x%08lx %s: ByteWidth=%u bind=%08lx usage=%u cpu=%08lx misc=%08lx stride=%u initial=%p", (unsigned long)hr,
                             x104_hr_name(hr), desc->ByteWidth, (unsigned long)desc->BindFlags, desc->Usage, (unsigned long)desc->CPUAccessFlags, (unsigned long)desc->MiscFlags,
                             desc->StructureByteStride, initial);
        else
            X104_LOG_LIMITED(&failed_count, 40, "X104 panel: CreateBuffer FAILED hr=0x%08lx %s (desc %p unreadable)", (unsigned long)hr, x104_hr_name(hr), (const void *)desc);
    }
    return hr;
}

static HRESULT X104_API x104_panel_create_texture2d_wrapper(void *self, const x104_panel_tex2d_desc_t *desc, const void *initial, void **texture)
{
    static volatile LONG failed_count, warned_count;
    x104_panel_device_vt_t *entry = self ? x104_panel_find_device_vt(*(void ***)self) : NULL;
    x104_panel_create_texture2d_fn original;
    HRESULT hr;
    BOOL readable = desc && !IsBadReadPtr(desc, sizeof(*desc));
    if (!entry && x104_panel_state.device_vt_count > 0) entry = &x104_panel_state.devices[0];
    original = entry ? (x104_panel_create_texture2d_fn)entry->orig_create_texture2d : NULL;
    if (!original) return E_FAIL;
    if (readable && (desc->Width > 2048u || desc->Height > 2048u)) {
        X104_LOG_LIMITED(&warned_count, 40, "X104 panel: WARNING CreateTexture2D %ux%u exceeds the 2048 limit of feature level 9_1 (fmt=%u mips=%u array=%u bind=%08lx usage=%u) - expect a failure",
                         desc->Width, desc->Height, desc->Format, desc->MipLevels, desc->ArraySize, (unsigned long)desc->BindFlags, desc->Usage);
    }
    hr = original(self, desc, initial, texture);
    if (FAILED(hr)) {
        if (readable)
            X104_LOG_LIMITED(&failed_count, 40, "X104 panel: CreateTexture2D FAILED hr=0x%08lx %s: %ux%u fmt=%u mips=%u array=%u samples=%u bind=%08lx usage=%u cpu=%08lx misc=%08lx initial=%p",
                             (unsigned long)hr, x104_hr_name(hr), desc->Width, desc->Height, desc->Format, desc->MipLevels, desc->ArraySize, desc->SampleCount,
                             (unsigned long)desc->BindFlags, desc->Usage, (unsigned long)desc->CPUAccessFlags, (unsigned long)desc->MiscFlags, initial);
        else
            X104_LOG_LIMITED(&failed_count, 40, "X104 panel: CreateTexture2D FAILED hr=0x%08lx %s (desc %p unreadable)", (unsigned long)hr, x104_hr_name(hr), (const void *)desc);
    }
    return hr;
}

static void x104_panel_patch_device_vtable(void *device_iface, const char *which)
{
    void **vtbl;
    void *orig_buffer;
    void *orig_texture;
    LONG index;
    if (!device_iface) return;
    vtbl = *(void ***)device_iface;
    if (!vtbl || IsBadReadPtr(vtbl, 6 * sizeof(void *))) {
        x104_logf("X104 panel: ERROR d3d11 device vtable %p (%s) is unreadable - diagnostics wrappers not installed", (void *)vtbl, which);
        return;
    }
    if (x104_panel_find_device_vt(vtbl)) {
        x104_logf("X104 panel: d3d11 device vtable %p (%s) already carries the CreateBuffer/CreateTexture2D wrappers", (void *)vtbl, which);
        return;
    }
    orig_buffer = vtbl[X104_PANEL_D3D11_DEVICE_CREATE_BUFFER];
    orig_texture = vtbl[X104_PANEL_D3D11_DEVICE_CREATE_TEXTURE2D];
    if (orig_buffer == (void *)x104_panel_create_buffer_wrapper || orig_texture == (void *)x104_panel_create_texture2d_wrapper) {
        x104_logf("X104 panel: WARNING d3d11 device vtable %p (%s) holds a wrapper but is not in the table - skipped", (void *)vtbl, which);
        return;
    }
    x104_lock(&x104_panel_state.chain_lock);
    index = x104_panel_state.device_vt_count;
    if (index >= X104_PANEL_MAX_VT) {
        x104_unlock(&x104_panel_state.chain_lock);
        x104_logf("X104 panel: %d d3d11 device vtables already carry wrappers - vtable %p (%s) skipped", (int)X104_PANEL_MAX_VT, (void *)vtbl, which);
        return;
    }
    x104_panel_state.devices[index].vtbl = vtbl;
    x104_panel_state.devices[index].orig_create_buffer = orig_buffer;
    x104_panel_state.devices[index].orig_create_texture2d = orig_texture;
    __sync_synchronize();
    x104_panel_state.device_vt_count = index + 1;
    x104_unlock(&x104_panel_state.chain_lock);
    if (arm32_patch_cell((void **)&vtbl[X104_PANEL_D3D11_DEVICE_CREATE_BUFFER], orig_buffer, (void *)x104_panel_create_buffer_wrapper, NULL, "ID3D11Device vtable cell 3 (CreateBuffer)"))
        x104_logf("X104 panel: d3d11 device vtable %p (%s): vtable[3] (CreateBuffer) %p -> logging wrapper", (void *)vtbl, which, orig_buffer);
    else
        x104_logf("X104 panel: WARNING d3d11 device vtable %p (%s): CreateBuffer wrapper NOT installed", (void *)vtbl, which);
    if (arm32_patch_cell((void **)&vtbl[X104_PANEL_D3D11_DEVICE_CREATE_TEXTURE2D], orig_texture, (void *)x104_panel_create_texture2d_wrapper, NULL,
                         "ID3D11Device vtable cell 5 (CreateTexture2D)"))
        x104_logf("X104 panel: d3d11 device vtable %p (%s): vtable[5] (CreateTexture2D) %p -> logging wrapper", (void *)vtbl, which, orig_texture);
    else
        x104_logf("X104 panel: WARNING d3d11 device vtable %p (%s): CreateTexture2D wrapper NOT installed", (void *)vtbl, which);
}

/* ======================================================================================================================
 * 5. device loss
 * ====================================================================================================================== */
static void x104_panel_report_device_lost(HRESULT reason, const char *where)
{
    char text[400];
    if (InterlockedExchange(&x104_panel_state.lost_reported, 1)) return;
    x104_logf("X104 panel: DEVICE LOST reason=%08lx %s (detected by %s) - unrecoverable: the game does not rebuild its GPU resources", (unsigned long)reason, x104_hr_name(reason), where);
    wsprintfA(text, "The graphics device was lost (reason 0x%08lX %s).\r\n\r\nMinecraft cannot recover from this. Restart the game.", (unsigned long)reason, x104_hr_name(reason));
    x104_panel_show_message(text, TRUE);
}

static HRESULT x104_panel_removed_reason(void *device)
{
    if (!device) return S_OK;
    return X104_VFN(device, X104_PANEL_D3D11_DEVICE_GET_DEVICE_REMOVED_REASON, HRESULT (X104_API *)(void *))(device);
}

static void x104_panel_poll_device(DWORD now)
{
    void *device;
    HRESULT reason;
    if (now - x104_panel_state.loss_poll_tick < X104_PANEL_LOSS_POLL_MS) return;
    x104_panel_state.loss_poll_tick = now;
    if (x104_panel_state.lost_reported) return;
    x104_lock(&x104_panel_state.device_lock);
    device = x104_panel_state.device;
    if (device) x104_raw_addref(device);
    x104_unlock(&x104_panel_state.device_lock);
    if (!device) return;
    reason = x104_panel_removed_reason(device);
    x104_raw_release(device);
    if (reason != S_OK) x104_panel_report_device_lost(reason, "the GetDeviceRemovedReason poll");
}

static void x104_panel_remember_device(void *device)
{
    void *old;
    if (device) x104_raw_addref(device);
    x104_lock(&x104_panel_state.device_lock);
    old = x104_panel_state.device;
    x104_panel_state.device = device;
    x104_unlock(&x104_panel_state.device_lock);
    if (old) x104_raw_release(old);
}

/* ======================================================================================================================
 * 6. the D3D11CreateDevice hook (IAT cell 0x20B1A4)
 * ====================================================================================================================== */
typedef struct x104_panel_created_t {
    void *device;
    void *context;                       /* the immediate context D3D11CreateDevice returned (for --trace-game-state), may be NULL */
    UINT driver;
    UINT actual;
    UINT reported;
    LONG number;
} x104_panel_created_t;

/* diagnostics after a successful creation; every call guarded by the caller (a crash here must never reach the game) */
static void x104_panel_post_create(void *argument, void *unused)
{
    x104_panel_created_t *created = (x104_panel_created_t *)argument;
    void *dxgi_device = NULL;
    void *adapter = NULL;
    void *factory = NULL;
    void *device2 = NULL;
    HRESULT hr;
    (void)unused;

    /* the factory behind the game's own device: this is how the game gets the factory it calls CreateSwapChainForComposition on (device -> IDXGIDevice -> GetAdapter -> GetParent) */
    hr = x104_raw_qi(created->device, &IID_IDXGIDevice, &dxgi_device);
    if (SUCCEEDED(hr) && dxgi_device) {
        hr = X104_VFN(dxgi_device, 7, HRESULT (X104_API *)(void *, void **))(dxgi_device, &adapter);
        if (SUCCEEDED(hr) && adapter) {
            if (!x104_panel_state.diag_logged) {
                x104_panel_adapter_desc_t desc;
                char name[130];
                memset(&desc, 0, sizeof(desc));
                hr = X104_VFN(adapter, 8, HRESULT (X104_API *)(void *, x104_panel_adapter_desc_t *))(adapter, &desc);
                if (SUCCEEDED(hr)) {
                    x104_narrow(name, sizeof(name), (const wchar_t *)desc.Description);
                    x104_logf("X104 panel: adapter \"%s\" VendorId=%04X DeviceId=%04X SubSys=%08lx Rev=%u DedicatedVideoMemory=%lu MiB SharedSystemMemory=%lu MiB DedicatedSystemMemory=%lu MiB",
                              name, desc.VendorId, desc.DeviceId, (unsigned long)desc.SubSysId, desc.Revision, (unsigned long)(desc.DedicatedVideoMemory >> 20),
                              (unsigned long)(desc.SharedSystemMemory >> 20), (unsigned long)(desc.DedicatedSystemMemory >> 20));
                } else {
                    x104_logf("X104 panel: WARNING IDXGIAdapter::GetDesc failed hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
                }
            }
            hr = X104_VFN(adapter, 6, HRESULT (X104_API *)(void *, const GUID *, void **))(adapter, &x104_panel_iid_dxgi_factory2, &factory);
            if (SUCCEEDED(hr) && factory) {
                void **game_vtbl = *(void ***)factory;
                x104_panel_state.game_factory_vtbl = game_vtbl;
                if (x104_panel_state.path == X104_PRESENT_HWND) {
                    if (x104_panel_find_factory_vt(game_vtbl)) {
                        x104_logf("X104 panel: the game's own IDXGIFactory2 %p has the vtable %p that is already patched (assumption confirmed: path B covers the game's factory)", factory,
                                  (void *)game_vtbl);
                    } else {
                        x104_logf("X104 panel: WARNING the game's own IDXGIFactory2 %p has vtable %p, which differs from the throw-away factory's vtable %p - patching it too", factory,
                                  (void *)game_vtbl, (void *)x104_panel_state.patched_factory_vtbl);
                        x104_panel_patch_factory_vtable(factory, "the factory behind the game's device");
                    }
                } else {
                    x104_logf("X104 panel: the game's own IDXGIFactory2 %p vtable %p (path A: not patched)", factory, (void *)game_vtbl);
                }
            } else {
                x104_logf("X104 panel: WARNING IDXGIAdapter::GetParent(IDXGIFactory2) failed hr=0x%08lx %s - the game's factory vtable is unknown", (unsigned long)hr, x104_hr_name(hr));
            }
        } else {
            x104_logf("X104 panel: WARNING IDXGIDevice::GetAdapter failed hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
        }
    } else {
        x104_logf("X104 panel: WARNING QI(IDXGIDevice) on the new device failed hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
    }
    if (factory) x104_raw_release(factory);
    if (adapter) x104_raw_release(adapter);
    if (dxgi_device) x104_raw_release(dxgi_device);

    if (!x104_panel_state.diag_logged) {
        struct { BOOL concurrent_creates, command_lists; } threading;
        struct { BOOL full_non_pow2; } d3d9;
        struct { BOOL tile_based; } arch;
        memset(&threading, 0, sizeof(threading));
        memset(&d3d9, 0, sizeof(d3d9));
        memset(&arch, 0, sizeof(arch));
        x104_logf("X104 panel: device diagnostics: driver=%s feature level actual=0x%04X reported to the game=0x%04X", created->driver == 5 ? "WARP (hardware creation failed)" :
                  (created->driver == 1 ? "HARDWARE" : "other"), created->actual, created->reported);
        hr = X104_VFN(created->device, X104_PANEL_D3D11_DEVICE_CHECK_FEATURE_SUPPORT, HRESULT (X104_API *)(void *, int, void *, UINT))(created->device, 0 /* THREADING */, &threading,
                                                                                                                                    (UINT)sizeof(threading));
        if (SUCCEEDED(hr)) x104_logf("X104 panel: D3D11_FEATURE_THREADING: DriverConcurrentCreates=%d DriverCommandLists=%d", (int)threading.concurrent_creates, (int)threading.command_lists);
        else x104_logf("X104 panel: CheckFeatureSupport(THREADING) hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
        hr = X104_VFN(created->device, X104_PANEL_D3D11_DEVICE_CHECK_FEATURE_SUPPORT, HRESULT (X104_API *)(void *, int, void *, UINT))(created->device, 7 /* D3D9_OPTIONS */, &d3d9,
                                                                                                                                    (UINT)sizeof(d3d9));
        if (SUCCEEDED(hr)) x104_logf("X104 panel: D3D11_FEATURE_D3D9_OPTIONS: FullNonPow2TextureSupport=%d", (int)d3d9.full_non_pow2);
        else x104_logf("X104 panel: CheckFeatureSupport(D3D9_OPTIONS) hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
        hr = X104_VFN(created->device, X104_PANEL_D3D11_DEVICE_CHECK_FEATURE_SUPPORT, HRESULT (X104_API *)(void *, int, void *, UINT))(created->device, 6 /* ARCHITECTURE_INFO */, &arch,
                                                                                                                                    (UINT)sizeof(arch));
        if (SUCCEEDED(hr)) x104_logf("X104 panel: D3D11_FEATURE_ARCHITECTURE_INFO: TileBasedDeferredRenderer=%d", (int)arch.tile_based);
        else x104_logf("X104 panel: CheckFeatureSupport(ARCHITECTURE_INFO) hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
        x104_panel_state.diag_logged = TRUE;
    }

    /* the diagnostics wrappers: the interface the game will really call is ID3D11Device2 (it QIs it right after the creation): patch its vtable too when it differs */
    x104_panel_patch_device_vtable(created->device, "ID3D11Device");
    x104_panel_cf_install(created->device, "ID3D11Device");
    hr = x104_raw_qi(created->device, &x104_panel_iid_d3d11_device2, &device2);
    if (SUCCEEDED(hr) && device2) {
        x104_panel_patch_device_vtable(device2, "ID3D11Device2");
        x104_panel_cf_install(device2, "ID3D11Device2");
        x104_raw_release(device2);
    } else {
        x104_logf("X104 panel: QI(ID3D11Device2) on the new device failed hr=0x%08lx %s (the game needs it: expect a crash right after this)", (unsigned long)hr, x104_hr_name(hr));
    }

    /* --trace-game-state: wrap the immediate context (the interface the game really calls is ID3D11DeviceContext2: patch its vtable too when it differs) */
    if ((x104_panel_tr.enabled || x104_panel_tr.fix_vb) && created->context) {
        static const GUID iid_context2 = {0x420d5b32, 0xb90c, 0x4da4, {0xbe, 0xf0, 0x35, 0x9f, 0x6a, 0x24, 0xa8, 0x3a}};
        void *context2 = NULL;
        x104_panel_tr_install_vtable(created->context, "ID3D11DeviceContext of D3D11CreateDevice");
        hr = x104_raw_qi(created->context, &iid_context2, &context2);
        if (SUCCEEDED(hr) && context2) {
            x104_panel_tr_install_vtable(context2, "ID3D11DeviceContext2");
            x104_raw_release(context2);
        } else {
            x104_logf("X104 d3dtrace: QI(ID3D11DeviceContext2) failed hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
        }
    } else if (x104_panel_tr.enabled || x104_panel_tr.fix_vb) {
        x104_logf("X104 d3dtrace: ERROR D3D11CreateDevice returned no immediate context pointer - the draw tracer is NOT installed");
    }
}

static HRESULT X104_API x104_panel_create_device(void *adapter, UINT driver_type, HMODULE software, UINT flags, const UINT *feature_levels, UINT feature_level_count, UINT sdk_version,
                                                 void **device, UINT *selected_level, void **context)
{
    static const UINT ladder[] = { 0xb100u, 0xb000u, 0xa100u, 0xa000u, 0x9300u, 0x9200u, 0x9100u };
    x104_panel_create_device_fn original = (x104_panel_create_device_fn)x104_panel_orig_create_device;
    UINT cap = arm32_feature_level_cap;
    UINT clamped[16];
    UINT clamped_count = 0;
    const UINT *levels = feature_levels;
    UINT level_count = feature_level_count;
    UINT use_flags = flags | 0x20u;                       /* D3D11_CREATE_DEVICE_BGRA_SUPPORT: harmless, needed by interop */
    UINT used_driver = driver_type;
    UINT index;
    HRESULT hr;
    HRESULT first_hr;
    LONG number = InterlockedIncrement(&x104_panel_state.device_creations);
    char requested[96];
    char effective[96];

    if (!original) {
        x104_logf("X104 panel: ERROR D3D11CreateDevice hook called but the original function pointer is NULL");
        return E_FAIL;
    }
    x104_panel_format_levels(requested, sizeof(requested), feature_levels, feature_level_count);
    if (cap) {
        if (feature_levels && feature_level_count && feature_level_count <= 64 && !IsBadReadPtr(feature_levels, (UINT_PTR)feature_level_count * sizeof(*feature_levels))) {
            for (index = 0; index < feature_level_count; ++index)
                if (feature_levels[index] <= cap && clamped_count < ARRAYSIZE(clamped)) clamped[clamped_count++] = feature_levels[index];
        } else if (!feature_levels || !feature_level_count) {
            for (index = 0; index < ARRAYSIZE(ladder); ++index)
                if (ladder[index] <= cap) clamped[clamped_count++] = ladder[index];
        }
        if (!clamped_count) clamped[clamped_count++] = cap;
        levels = clamped;
        level_count = clamped_count;
    }
    x104_panel_format_levels(effective, sizeof(effective), levels, level_count);
    if (number > 1)
        x104_logf("X104 panel: WARNING D3D11CreateDevice entered AGAIN (call #%ld, thread %lu): the game is re-creating its device (HandleDeviceLost / ValidateDevice)", (long)number,
                  (unsigned long)GetCurrentThreadId());
    x104_logf("X104 panel: D3D11CreateDevice request: adapter=%p driver=%u flags=0x%X sdk=%u levels(%u)=[%s] -> cap=0x%04X flags=0x%X levels(%u)=[%s]", adapter, driver_type, flags, sdk_version,
              feature_level_count, requested, cap, use_flags, level_count, effective);

    hr = original(adapter, driver_type, software, use_flags, levels, level_count, sdk_version, device, selected_level, context);
    first_hr = hr;
    if (SUCCEEDED(hr) && (!device || !*device)) {
        x104_logf("X104 panel: ERROR D3D11CreateDevice returned success but the device output is NULL (device=%p)", (void *)device);
        hr = E_FAIL;
    }
    if (FAILED(hr) && driver_type == 1) {
        x104_logf("X104 panel: ERROR D3D11CreateDevice (HARDWARE) FAILED hr=0x%08lx %s with levels [%s] - retrying once with D3D_DRIVER_TYPE_WARP (software rasteriser: SLOW)",
                  (unsigned long)hr, x104_hr_name(hr), effective);
        if (device) *device = NULL;
        if (context) *context = NULL;
        if (selected_level) *selected_level = 0;
        used_driver = 5;
        hr = original(NULL, 5u, NULL, use_flags, levels, level_count, sdk_version, device, selected_level, context);
        x104_logf("X104 panel: D3D11CreateDevice WARP retry hr=0x%08lx %s selected=0x%04X device=%p context=%p", (unsigned long)hr, x104_hr_name(hr), selected_level ? *selected_level : 0,
                  device ? *device : NULL, context ? *context : NULL);
        if (SUCCEEDED(hr) && (!device || !*device)) hr = E_FAIL;
    }
    if (FAILED(hr)) {
        char text[512];
        x104_logf("X104 panel: FATAL D3D11CreateDevice failed (hardware hr=0x%08lx %s, final hr=0x%08lx %s, driver %u): the game ignores the result and would crash at once - terminating (exit code 33)",
                  (unsigned long)first_hr, x104_hr_name(first_hr), (unsigned long)hr, x104_hr_name(hr), used_driver);
        wsprintfA(text, "Cannot create a Direct3D 11 device.\r\n\r\nHardware: 0x%08lX %s\r\nFinal: 0x%08lX %s (driver type %u, levels [%s])\r\n\r\nSee win32craft.log.", (unsigned long)first_hr,
                  x104_hr_name(first_hr), (unsigned long)hr, x104_hr_name(hr), used_driver, effective);
        MessageBoxA(NULL, text, "Minecraft PE 0.10.4 - Win32Craft", 0x10u | 0x10000u | 0x40000u);
        TerminateProcess(GetCurrentProcess(), 33);
        return hr;
    }

    {
        UINT actual = selected_level ? *selected_level : 0;
        UINT reported = actual;
        x104_panel_created_t created;
        if (actual == 0x9100u || actual == 0x9200u) {
            reported = 0x9300u;                                         /* the lie: the game's getShaderModelSuffix accepts only 0x9300 and >= 0xA000 */
            if (selected_level) *selected_level = reported;
            x104_panel_set_profile_digit(actual == 0x9100u ? '1' : '2', "device level differs from / confirms the cap");
        } else if (actual == 0x9300u) {
            x104_panel_set_profile_digit('3', "the device really is feature level 9_3");
        }
        x104_logf("X104 panel: selected feature level 0x%04X, reported to the game 0x%04X (driver %s, device=%p context=%p, HRESULT 0x%08lx)", actual, reported,
                  used_driver == 5 ? "WARP" : (used_driver == 1 ? "HARDWARE" : "other"), device ? *device : NULL, context ? *context : NULL, (unsigned long)hr);
        x104_panel_state.actual_level = actual;
        x104_panel_state.reported_level = reported;
        x104_panel_state.driver_type = used_driver;

        if (number > 1) {
            /* a re-creation: the previous device is the lost one (HandleDeviceLost runs from Present on the render thread) */
            void *old_device;
            x104_lock(&x104_panel_state.device_lock);
            old_device = x104_panel_state.device;
            if (old_device) x104_raw_addref(old_device);
            x104_unlock(&x104_panel_state.device_lock);
            if (old_device) {
                HRESULT reason = x104_panel_removed_reason(old_device);
                x104_raw_release(old_device);
                if (reason != S_OK) x104_panel_report_device_lost(reason, "the game re-creating its device (D3D11CreateDevice entered again)");
                else x104_logf("X104 panel: the previous device still reports S_OK - re-creation without a device loss (ValidateDevice?); keeping the new device");
            }
        }
        x104_panel_remember_device(device ? *device : NULL);
        created.device = device ? *device : NULL;
        created.context = context ? *context : NULL;
        created.driver = used_driver;
        created.actual = actual;
        created.reported = reported;
        created.number = number;
        if (created.device) x104_panel_call_guarded(x104_panel_post_create, &created, NULL, "device diagnostics / vtable patches");
    }
    return hr;
}

/* ======================================================================================================================
 * 7. path A: DirectComposition (optional --present=dcomp, and the fallback when SetSwapChain finds a chain the HWND stub did not create)
 * ====================================================================================================================== */
static BOOL x104_panel_dcomp_attach(void *chain)
{
    x104_panel_dcomp_create_fn create;
    HMODULE module;
    HRESULT hr;
    void *dev;
    if (x104_panel_state.dcomp_failed) return FALSE;
    if (!x104.window || !IsWindow(x104.window)) {
        x104_logf("X104 panel: ERROR path A: the game window %p does not exist", (void *)x104.window);
        return FALSE;
    }
    if (!x104_panel_state.dcomp_device) {
        module = LoadLibraryW(L"dcomp.dll");
        if (!module) {
            x104_logf("X104 panel: ERROR path A: LoadLibrary(dcomp.dll) failed, error=%lu - DirectComposition is not available on this system", (unsigned long)GetLastError());
            x104_panel_state.dcomp_failed = TRUE;
            return FALSE;
        }
        create = (x104_panel_dcomp_create_fn)GetProcAddress(module, "DCompositionCreateDevice");
        if (!create) {
            x104_logf("X104 panel: ERROR path A: dcomp.dll has no DCompositionCreateDevice (error %lu)", (unsigned long)GetLastError());
            x104_panel_state.dcomp_failed = TRUE;
            return FALSE;
        }
        dev = NULL;
        hr = create(NULL, &x104_panel_iid_dcomp_device, &dev);
        x104_logf("X104 panel: path A: DCompositionCreateDevice(NULL, IDCompositionDevice) hr=0x%08lx %s device=%p", (unsigned long)hr, x104_hr_name(hr), dev);
        if (FAILED(hr) || !dev) {
            x104_panel_state.dcomp_failed = TRUE;
            return FALSE;
        }
        x104_panel_state.dcomp_device = dev;
        hr = X104_VFN(dev, 6, HRESULT (X104_API *)(void *, HWND, BOOL, void **))(dev, x104.window, TRUE, &x104_panel_state.dcomp_target);          /* CreateTargetForHwnd */
        x104_logf("X104 panel: path A: IDCompositionDevice::CreateTargetForHwnd(%p, topmost) hr=0x%08lx %s target=%p", (void *)x104.window, (unsigned long)hr, x104_hr_name(hr),
                  x104_panel_state.dcomp_target);
        if (FAILED(hr) || !x104_panel_state.dcomp_target) {
            x104_panel_state.dcomp_failed = TRUE;
            return FALSE;
        }
        hr = X104_VFN(dev, 7, HRESULT (X104_API *)(void *, void **))(dev, &x104_panel_state.dcomp_visual);                                          /* CreateVisual */
        x104_logf("X104 panel: path A: IDCompositionDevice::CreateVisual hr=0x%08lx %s visual=%p", (unsigned long)hr, x104_hr_name(hr), x104_panel_state.dcomp_visual);
        if (FAILED(hr) || !x104_panel_state.dcomp_visual) {
            x104_panel_state.dcomp_failed = TRUE;
            return FALSE;
        }
        hr = X104_VFN(x104_panel_state.dcomp_target, 3, HRESULT (X104_API *)(void *, void *))(x104_panel_state.dcomp_target, x104_panel_state.dcomp_visual);   /* SetRoot */
        x104_logf("X104 panel: path A: IDCompositionTarget::SetRoot(visual) hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
        if (FAILED(hr)) {
            x104_panel_state.dcomp_failed = TRUE;
            return FALSE;
        }
    }
    dev = x104_panel_state.dcomp_device;
    hr = X104_VFN(x104_panel_state.dcomp_visual, 15, HRESULT (X104_API *)(void *, void *))(x104_panel_state.dcomp_visual, chain);                    /* SetContent */
    x104_logf("X104 panel: path A: IDCompositionVisual::SetContent(%p) hr=0x%08lx %s", chain, (unsigned long)hr, x104_hr_name(hr));
    if (FAILED(hr)) return FALSE;
    hr = X104_VFN(dev, 3, HRESULT (X104_API *)(void *))(dev);                                                                                         /* Commit */
    x104_logf("X104 panel: path A: IDCompositionDevice::Commit hr=0x%08lx %s", (unsigned long)hr, x104_hr_name(hr));
    return SUCCEEDED(hr);
}

/* ======================================================================================================================
 * 8. the fake SwapChainPanel and SizeChangedEventArgs
 * ====================================================================================================================== */
/* ISwapChainPanelNative::SetSwapChain: the path-B watchdog (runs on the UI thread right after the SizeChanged handler returned, queued by the game) */
static HRESULT X104_API x104_panel_native_set_swapchain(void *self, void *swapchain)
{
    x104_panel_object_t *panel = X104_THIS(x104_panel_object_t, self);
    void *identity = NULL;
    void *previous;
    HRESULT qhr;
    BOOL identity_owned = FALSE;
    BOOL attached = FALSE;
    LONG number = InterlockedIncrement(&x104_panel_state.setswapchain_calls);

    if (!swapchain) {
        x104_logf("X104 panel: SetSwapChain(NULL) #%ld: detach request ignored (S_OK)", (long)number);
        return S_OK;
    }
    qhr = x104_raw_qi(swapchain, &x104_panel_iid_dxgi_swapchain, &identity);
    if (FAILED(qhr) || !identity) {
        x104_logf("X104 panel: WARNING SetSwapChain(%p): QI(IDXGISwapChain) failed hr=0x%08lx %s - comparing the raw pointer", swapchain, (unsigned long)qhr, x104_hr_name(qhr));
        identity = swapchain;
    } else {
        identity_owned = TRUE;
    }
    x104_raw_addref(swapchain);
    previous = panel->native_chain;
    panel->native_chain = swapchain;
    if (previous) x104_raw_release(previous);
    x104_logf("X104 panel: SetSwapChain(%p) #%ld identity=%p vtable=%p path=%s", swapchain, (long)number, identity, (void *)*(void ***)swapchain,
              x104_panel_state.path == X104_PRESENT_HWND ? "B (HWND)" : "A (DirectComposition)");

    if (x104_panel_state.path == X104_PRESENT_HWND) {
        BOOL same;
        x104_lock(&x104_panel_state.chain_lock);
        same = x104_panel_state.created_by_stub && x104_panel_state.stub_chain && (x104_panel_state.stub_identity == identity || x104_panel_state.stub_chain == swapchain);
        x104_unlock(&x104_panel_state.chain_lock);
        if (same) {
            x104_logf("X104 panel: SetSwapChain(%p): the chain created by the HWND stub - nothing to attach (path B ok, attempt %c)", swapchain, x104_panel_state.stub_attempt);
            attached = TRUE;
        } else {
            x104_logf("X104 panel: ERROR dxgi vtable mismatch: the game's swap chain %p was NOT created by the HWND stub (stub ran=%d, stub chain=%p, game factory vtable=%p, patched vtable=%p) - "
                      "path B did not take effect, falling back to path A (DirectComposition)", swapchain, (int)x104_panel_state.created_by_stub, x104_panel_state.stub_chain,
                      (void *)x104_panel_state.game_factory_vtbl, (void *)x104_panel_state.patched_factory_vtbl);
            x104_panel_state.fallback_a = TRUE;
            x104_panel_patch_chain(identity, FALSE, "composition chain of the path-A fallback");
            attached = x104_panel_dcomp_attach(swapchain);
        }
    } else {
        x104_panel_patch_chain(identity, FALSE, "composition chain (path A)");
        attached = x104_panel_dcomp_attach(swapchain);
    }
    if (attached) {
        x104_flag_set(&x104.f_swapchain_created, "f_swapchain_created (SetSwapChain: the chain is attached)");
    } else {
        x104_logf("X104 panel: ERROR the swap chain %p could not be attached by any path - the window will stay black", swapchain);
        x104_panel_show_message("There is no way to present the game's swap chain on this system.\r\n\r\n"
                                "The HWND swap-chain patch did not take effect and DirectComposition is not available or failed. See win32craft.log (X104 panel: lines).", FALSE);
    }
    if (identity_owned) x104_raw_release(identity);
    return S_OK;       /* never fail: the game throws a Platform exception for HRESULT < 0 (attach lambda 0x1DAF04) */
}

static HRESULT X104_API x104_panel_scp_get_scale_x(void *self, float *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(x104_panel_object_t, self)->scale_x;
    return S_OK;
}

static HRESULT X104_API x104_panel_scp_get_scale_y(void *self, float *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(x104_panel_object_t, self)->scale_y;
    return S_OK;
}

static HRESULT X104_API x104_panel_scp_add_scale_changed(void *self, void *handler, LONGLONG *token)
{
    return x104_event_add(&X104_THIS(x104_panel_object_t, self)->scale_changed, handler, token);
}

static HRESULT X104_API x104_panel_scp_remove_scale_changed(void *self, LONGLONG token)
{
    return x104_event_remove(&X104_THIS(x104_panel_object_t, self)->scale_changed, token);
}

static HRESULT X104_API x104_panel_scp_create_input_source(void *self, int device_types, void **result)
{
    static volatile LONG logged;
    HRESULT hr;
    (void)self;
    hr = x104_input_create_independent_source((UINT)device_types, result);
    X104_LOG_LIMITED(&logged, 6, "X104 panel: ISwapChainPanel::CreateCoreIndependentInputSource(types=%d) on thread %lu -> 0x%08lx %s source=%p", device_types, (unsigned long)GetCurrentThreadId(),
                     (unsigned long)hr, x104_hr_name(hr), result ? *result : NULL);
    return hr;
}

static HRESULT X104_API x104_panel_args_get_previous(void *self, X104Size *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(x104_panel_args_t, self)->previous;
    return S_OK;
}

static HRESULT X104_API x104_panel_args_get_new(void *self, X104Size *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(x104_panel_args_t, self)->current;
    return S_OK;
}

X104_VTBL(x104_panel_vt_scp, ISwapChainPanel) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ISwapChainPanel, get_CompositionScaleX, x104_panel_scp_get_scale_x),
    X104_SLOT(ISwapChainPanel, get_CompositionScaleY, x104_panel_scp_get_scale_y),
    X104_SLOT(ISwapChainPanel, add_CompositionScaleChanged, x104_panel_scp_add_scale_changed),
    X104_SLOT(ISwapChainPanel, remove_CompositionScaleChanged, x104_panel_scp_remove_scale_changed),
    X104_SLOT(ISwapChainPanel, CreateCoreIndependentInputSource, x104_panel_scp_create_input_source)
};
X104_VTBL(x104_panel_vt_native, ISwapChainPanelNative) = {
    X104_UNKNOWN_SLOTS,
    X104_SLOT(ISwapChainPanelNative, SetSwapChain, x104_panel_native_set_swapchain)
};
X104_VTBL(x104_panel_vt_args, ISizeChangedEventArgs) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ISizeChangedEventArgs, get_PreviousSize, x104_panel_args_get_previous),
    X104_SLOT(ISizeChangedEventArgs, get_NewSize, x104_panel_args_get_new)
};

static const X104IfaceDesc x104_panel_ifaces[] = {
    X104_ELEMENT_IFACES,
    X104_IFACE(ISwapChainPanel, x104_panel_vt_scp),
    X104_IFACE(ISwapChainPanelNative, x104_panel_vt_native)
};
X104_CLASS_DEF(x104_panel_class, "SwapChainPanel", L"Windows.UI.Xaml.Controls.SwapChainPanel", x104_panel_ifaces, X104_CLASS_AGILE, NULL);

static const X104IfaceDesc x104_panel_args_ifaces[] = {
    X104_IFACE(ISizeChangedEventArgs, x104_panel_vt_args)
};
X104_CLASS_DEF(x104_panel_args_class, "SizeChangedEventArgs", L"Windows.UI.Xaml.SizeChangedEventArgs", x104_panel_args_ifaces, X104_CLASS_AGILE, NULL);

/* ======================================================================================================================
 * 9. size pipeline (R2 / R3)
 * ====================================================================================================================== */
/* copy the real client rectangle into client_w/h (not while minimised / 0x0) */
static void x104_panel_refresh_client(void)
{
    RECT rect;
    if (!x104_panel_state.window || x104_panel_state.minimized) return;
    if (!GetClientRect(x104_panel_state.window, &rect)) return;
    if (rect.right - rect.left > 0 && rect.bottom - rect.top > 0) {
        InterlockedExchange(&x104_panel_state.client_w, rect.right - rect.left);
        InterlockedExchange(&x104_panel_state.client_h, rect.bottom - rect.top);
    }
}

/* deliver SizeChanged(current client size) to the game's handlers.  UI thread, top level only (R1).  Returns the number of handlers invoked. */
static unsigned x104_panel_raise_now(BOOL first)
{
    x104_panel_object_t *panel = x104_panel_state.panel;
    x104_panel_args_t *args = x104_panel_state.args;
    int width = (int)x104_panel_state.client_w;
    int height = (int)x104_panel_state.client_h;
    int previous_w = x104_panel_state.raised_w;
    int previous_h = x104_panel_state.raised_h;
    unsigned handlers;
    unsigned invoked;
    DWORD started;
    LONG number;
    if (!panel || !args) return 0;
    handlers = x104_event_count(&panel->base.size_changed);
    if (!handlers) {
        x104_logf("X104 panel: SizeChanged %dx%d NOT raised: the panel has no SizeChanged handler registered (the MainPage constructor has not run add_SizeChanged)", width, height);
        return 0;
    }
    x104_element_set_sizes(&panel->base, (double)width, (double)height);
    args->previous.Width = (float)previous_w;
    args->previous.Height = (float)previous_h;
    args->current.Width = (float)width;
    args->current.Height = (float)height;
    number = InterlockedIncrement(&x104_panel_state.raise_count);
    if (first || number <= 20 || (number % 10) == 0)
        x104_logf("X104 panel: %sSizeChanged raising %dx%d (prev %dx%d) handlers=%u #%ld", first ? "first " : "", width, height, previous_w, previous_h, handlers, (long)number);
    started = GetTickCount();
    invoked = x104_event_invoke_guarded(&panel->base.size_changed, x104_object_iface0(&panel->base.base), &args->base.views[0]);
    x104_panel_state.raised_w = width;
    x104_panel_state.raised_h = height;
    x104_panel_state.last_raise_tick = GetTickCount();
    if (first || number <= 20 || (number % 10) == 0)
        x104_logf("X104 panel: %sSizeChanged raised %dx%d (prev %dx%d) handlers=%u invoked=%u, the game's handler took %lu ms", first ? "first " : "", width, height, previous_w, previous_h,
                  handlers, invoked, (unsigned long)(GetTickCount() - started));
    return invoked;
}

/* R2: the first SizeChanged only when the MainPage constructor returned, AppMain exists, the init job started (client+0xC) and the client size is known */
static void x104_panel_try_first(DWORD now)
{
    BYTE *appmain;
    BOOL content = x104.f_content_set != 0;
    BOOL holder;
    int width = (int)x104_panel_state.client_w;
    int height = (int)x104_panel_state.client_h;
    if (!x104_panel_state.panel) return;
    appmain = content ? x104_game_appmain() : NULL;
    holder = appmain ? x104_game_client_has_holder() : FALSE;
    if (!content || !appmain || !holder || width <= 0 || height <= 0) {
        if (now - x104_panel_state.first_wait_log_tick >= 2000u || !x104_panel_state.first_wait_log_tick) {
            x104_panel_state.first_wait_log_tick = now ? now : 1u;
            x104_logf("X104 panel: first SizeChanged waiting: content_set=%d appmain=%p holder=%d size=%dx%d", (int)content, (void *)appmain, (int)holder, width, height);
        }
        return;
    }
    x104_panel_refresh_client();
    InterlockedExchange(&x104_panel_state.pending_resize, 0);
    if (x104_panel_raise_now(TRUE) == 0) {
        static volatile LONG none_logged;
        X104_LOG_LIMITED(&none_logged, 10, "X104 panel: the first SizeChanged was delivered to no handler - will retry");
        return;
    }
    x104_panel_state.first_done = TRUE;
    x104_panel_state.size_done_tick = GetTickCount();
    x104_flag_set(&x104.f_size_changed_done, "f_size_changed_done (first SizeChanged returned)");
}

static void x104_panel_ui_tick(void)
{
    DWORD now;
    if (!x104_panel_state.panel || x104_panel_state.shutdown) return;
    now = GetTickCount();
    if (!x104_panel_state.first_done) {
        x104_panel_try_first(now);
    } else if (x104_panel_state.pending_resize && !x104_panel_state.minimized) {
        BOOL due = x104_panel_state.in_size_move ? (now - x104_panel_state.last_raise_tick >= 100u) : (now - x104_panel_state.last_change_tick >= 30u);
        if (due && !x104_game_is_ready()) {
            if (now - x104_panel_state.deferred_log_tick >= 3000u) {
                x104_panel_state.deferred_log_tick = now;
                x104_logf("X104 panel: resize to %ldx%ld deferred until the game finished its init job (a SizeChanged during MinecraftClient::init would race it)",
                          (long)x104_panel_state.client_w, (long)x104_panel_state.client_h);
            }
            due = FALSE;
        }
        if (due) {
            InterlockedExchange(&x104_panel_state.pending_resize, 0);
            x104_panel_refresh_client();
            if ((int)x104_panel_state.client_w != x104_panel_state.raised_w || (int)x104_panel_state.client_h != x104_panel_state.raised_h) x104_panel_raise_now(FALSE);
        }
    }
    x104_panel_poll_device(now);
    /* the 3 s rule: a black window must be explained by the log */
    if (x104_panel_state.first_done && !x104_panel_state.rule_logged && !x104.f_swapchain_created && now - x104_panel_state.size_done_tick >= 3000u) {
        x104_panel_state.rule_logged = TRUE;
        x104_logf("X104 panel: ERROR first SizeChanged returned %lu ms ago but no swap chain was created/attached (created_by_stub=%d, path %s, last stub HRESULT 0x%08lx %s, "
                  "SetSwapChain calls %ld) - the window will stay black; see the lines above (CreateSwapChainForComposition / SetSwapChain)", (unsigned long)(now - x104_panel_state.size_done_tick),
                  (int)x104_panel_state.created_by_stub, x104_panel_state.path == X104_PRESENT_HWND ? "B" : "A", (unsigned long)x104_panel_state.stub_last_hr,
                  x104_hr_name(x104_panel_state.stub_last_hr), (long)x104_panel_state.setswapchain_calls);
    }
}

static BOOL x104_panel_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result)
{
    (void)window;
    switch (message) {
    case WM_SIZE: {
        int width = (int)LOWORD(lparam);
        int height = (int)HIWORD(lparam);
        static volatile LONG logged;
        LONG number = InterlockedIncrement(&logged);
        if (number <= 30 || (number % 20) == 0)
            x104_logf("X104 panel: WM_SIZE %dx%d type=%u (client was %ldx%ld)", width, height, (unsigned)wparam, (long)x104_panel_state.client_w, (long)x104_panel_state.client_h);
        if (wparam == SIZE_MINIMIZED) {
            InterlockedExchange(&x104_panel_state.minimized, 1);
            return FALSE;
        }
        InterlockedExchange(&x104_panel_state.minimized, 0);
        if (width <= 0 || height <= 0) {
            static volatile LONG ignored_count;
            X104_LOG_LIMITED(&ignored_count, 10, "X104 panel: WM_SIZE %dx%d ignored (0x0 client area, type=%u)", width, height, (unsigned)wparam);
            return FALSE;
        }
        if (width != (int)x104_panel_state.client_w || height != (int)x104_panel_state.client_h) {
            InterlockedExchange(&x104_panel_state.client_w, width);
            InterlockedExchange(&x104_panel_state.client_h, height);
            x104_panel_state.last_change_tick = GetTickCount();
            InterlockedExchange(&x104_panel_state.pending_resize, 1);
        }
        return FALSE;                                    /* the other units see WM_SIZE too */
    }
    case WM_ENTERSIZEMOVE:
        InterlockedExchange(&x104_panel_state.in_size_move, 1);
        return FALSE;
    case WM_EXITSIZEMOVE:
        InterlockedExchange(&x104_panel_state.in_size_move, 0);
        x104_panel_state.last_change_tick = GetTickCount() - 30u;       /* deliver the settled size on the next tick */
        InterlockedExchange(&x104_panel_state.pending_resize, 1);
        return FALSE;
    case 0x0024u: /* WM_GETMINMAXINFO */ {
        LONG *info = (LONG *)lparam;
        if (!info || IsBadReadPtr(info, 10 * sizeof(LONG))) return FALSE;
        info[6] = X104_PANEL_MIN_TRACK_W;                /* MINMAXINFO.ptMinTrackSize */
        info[7] = X104_PANEL_MIN_TRACK_H;
        if (result) *result = 0;
        return TRUE;
    }
    default:
        return FALSE;
    }
}

/* ======================================================================================================================
 * 10. contract functions
 * ====================================================================================================================== */
/* hooks that must be live before the game creates its device */
static BOOL x104_panel_install(BYTE *image)
{
    UINT cap = arm32_feature_level_cap;
    char shown[96];
    x104_panel_state.cap = cap;
    x104_panel_tr_parse();
    x104_panel_state.path = x104.opt_present == X104_PRESENT_DCOMP ? X104_PRESENT_DCOMP : X104_PRESENT_HWND;
    if (image != x104.image) x104_logf("X104 panel: WARNING install image %p differs from x104.image %p", (void *)image, (void *)x104.image);
    x104_logf("X104 panel: install: feature level cap 0x%04X, presentation path %s, %s%s", cap, x104_panel_state.path == X104_PRESENT_HWND ? "B (HWND swap chain)" : "A (DirectComposition)",
              x104.opt_half_res ? "--half-res" : "full resolution", x104_panel_tr.enabled ? ", --trace-game-state (D3D draw tracer on)" : "");

    /* (a) the D3D11CreateDevice IAT hook - the only fatal step */
    if (!arm32_iat_hook_import(image, FALSE, "D3D11CreateDevice", X104_RVA_IAT_D3D11CREATEDEVICE, (void *)x104_panel_create_device, &x104_panel_orig_create_device, "D3D11CreateDevice")) {
        x104_logf("X104 panel: ERROR the D3D11CreateDevice hook could NOT be installed (IAT rva %08lx): the game would fail on a 9_1 GPU and crash at the next QI",
                  (unsigned long)X104_RVA_IAT_D3D11CREATEDEVICE);
        return FALSE;
    }
    wsprintfA(shown, "cell %p -> %p", (void *)(image + X104_RVA_IAT_D3D11CREATEDEVICE), (void *)x104_panel_create_device);
    x104_logf("X104 panel: install: D3D11CreateDevice hook installed (%s, original %p)", shown, x104_panel_orig_create_device);

    /* (b) shader profile string, (c) resolution float, (d) cwd + include */
    if (!x104_panel_install_profile(cap)) return FALSE;
    x104_panel_install_resolution();
    x104_panel_install_cwd();

    /* (e) presentation path */
    if (x104_panel_state.path == X104_PRESENT_DCOMP) {
        HMODULE module = LoadLibraryW(L"dcomp.dll");
        if (!module || !GetProcAddress(module, "DCompositionCreateDevice")) {
            x104_logf("X104 panel: WARNING --present=dcomp but dcomp.dll / DCompositionCreateDevice is not available (error %lu) - FALLING BACK to path B (HWND swap chain)",
                      (unsigned long)GetLastError());
            x104_panel_state.path = X104_PRESENT_HWND;
        } else {
            x104_logf("X104 panel: install: path A selected, dcomp.dll present - no dxgi cells are patched; SetSwapChain attaches the game's composition chain to the window");
        }
    }
    if (x104_panel_state.path == X104_PRESENT_HWND) {
        HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
        x104_panel_create_factory1_fn create = dxgi ? (x104_panel_create_factory1_fn)GetProcAddress(dxgi, "CreateDXGIFactory1") : NULL;
        void *factory1 = NULL;
        void *factory2 = NULL;
        HRESULT hr;
        if (!create) {
            x104_logf("X104 panel: ERROR dxgi.dll / CreateDXGIFactory1 not available (error %lu): the dxgi cells are patched later from the game's own device (if the game gets that far)",
                      (unsigned long)GetLastError());
        } else {
            hr = create(&x104_panel_iid_dxgi_factory1, &factory1);
            if (FAILED(hr) || !factory1) {
                x104_logf("X104 panel: ERROR CreateDXGIFactory1 failed hr=0x%08lx %s - the dxgi cells are patched later from the game's own device", (unsigned long)hr, x104_hr_name(hr));
            } else {
                hr = x104_raw_qi(factory1, &x104_panel_iid_dxgi_factory2, &factory2);
                if (FAILED(hr) || !factory2) {
                    x104_logf("X104 panel: ERROR QI(IDXGIFactory2) failed hr=0x%08lx %s - this system has no CreateSwapChainForComposition (Windows 7 without the platform update?); the game will fail",
                              (unsigned long)hr, x104_hr_name(hr));
                } else {
                    x104_panel_patch_factory_vtable(factory2, "throw-away factory of install");
                    x104_raw_release(factory2);
                }
                x104_raw_release(factory1);
            }
        }
    }
    return TRUE;
}

static BOOL x104_panel_init(HWND window)
{
    RECT rect;
    int width = 0;
    int height = 0;
    x104_panel_state.window = window;
    if (window && GetClientRect(window, &rect)) {
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }
    if (width <= 0 || height <= 0) {
        x104_logf("X104 panel: WARNING GetClientRect gave %dx%d (window %p, minimised=%d) - using the configured size %dx%d", width, height, (void *)window, window ? (int)IsIconic(window) : 0,
                  x104.opt_width, x104.opt_height);
        width = x104.opt_width > 0 ? x104.opt_width : 1280;
        height = x104.opt_height > 0 ? x104.opt_height : 720;
    }
    InterlockedExchange(&x104_panel_state.client_w, width);
    InterlockedExchange(&x104_panel_state.client_h, height);
    if (!x104_panel_state.panel) {
        x104_panel_object_t *panel = (x104_panel_object_t *)x104_element_new(&x104_panel_class, sizeof(x104_panel_object_t), "SwapChainPanel");
        x104_panel_args_t *args = (x104_panel_args_t *)x104_object_new(&x104_panel_args_class, sizeof(x104_panel_args_t));
        if (!panel || !args) {
            x104_logf("X104 panel: ERROR init: out of memory creating the SwapChainPanel (%p) / SizeChangedEventArgs (%p) objects", (void *)panel, (void *)args);
            return FALSE;
        }
        x104_event_init(&panel->scale_changed, "SwapChainPanel.CompositionScaleChanged");
        x104_event_init(&panel->base.size_changed, "SwapChainPanel.SizeChanged");
        panel->scale_x = 1.0f;
        panel->scale_y = 1.0f;
        panel->base.visibility = 0;
        x104_panel_state.panel = panel;
        x104_panel_state.args = args;
    }
    x104_element_set_sizes(&x104_panel_state.panel->base, (double)width, (double)height);
    x104_panel_state.loss_poll_tick = GetTickCount();
    x104_logf("X104 panel: init: SwapChainPanel obj=%p (serial %ld) client %dx%d (window %p), CompositionScale 1.0, SizeChangedEventArgs obj=%p", (void *)x104_panel_state.panel,
              (long)x104_panel_state.panel->base.base.serial, width, height, (void *)window, (void *)x104_panel_state.args);
    return TRUE;
}

static void x104_panel_shutdown(void)
{
    if (InterlockedExchange(&x104_panel_state.shutdown, 1)) return;
    x104_logf("X104 panel: shutdown: %ld SizeChanged raised, %ld SetSwapChain call(s), swap chain created=%d (stub=%d, attempt %c), device creations %ld, game threads still own the GPU objects",
              (long)x104_panel_state.raise_count, (long)x104_panel_state.setswapchain_calls, (int)x104.f_swapchain_created, (int)x104_panel_state.created_by_stub,
              x104_panel_state.stub_attempt ? x104_panel_state.stub_attempt : '-', (long)x104_panel_state.device_creations);
}

/* The panel owns no activated WinRT class (the game gets the panel from Page.FindName). */
static HRESULT x104_panel_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    (void)class_name;
    (void)iid;
    (void)out;
    return REGDB_E_CLASSNOTREG;
}

static X104Object *x104_panel_object(void)
{
    return x104_panel_state.panel ? &x104_panel_state.panel->base.base : NULL;
}

static void x104_panel_raise_size_changed(void)
{
    if (!x104_panel_state.panel || x104_panel_state.shutdown) return;
    if (!x104_on_ui_thread()) {
        static volatile LONG logged;
        X104_LOG_LIMITED(&logged, 5, "X104 panel: x104_panel_raise_size_changed called off the UI thread (thread %lu) - queued as a pending resize (R1)", (unsigned long)GetCurrentThreadId());
        x104_panel_state.last_change_tick = GetTickCount();
        InterlockedExchange(&x104_panel_state.pending_resize, 1);
        return;
    }
    if (!x104_panel_state.first_done) {
        x104_panel_try_first(GetTickCount());
        return;
    }
    if (x104_panel_state.minimized) return;
    InterlockedExchange(&x104_panel_state.pending_resize, 0);
    x104_panel_refresh_client();
    x104_panel_raise_now(FALSE);
}

static void x104_panel_client_size(float *width_dips, float *height_dips)
{
    if (width_dips) *width_dips = (float)x104_panel_state.client_w;
    if (height_dips) *height_dips = (float)x104_panel_state.client_h;
}

#endif /* WIN32_HOST_ARM32_0104_PANEL_INL */
