/* gd_host.c - Win32Craft Geometry Dash host, milestone M1 (HEADLESS probe).
 *
 * Loads GeometryJumpComponent.dll and drives the decoded WP8 DrawingSurface contract (games_recon/
 * geometry_dash_gd-component.md F11): create the native Direct3DInterop, hand it the host's fake COM objects
 * (ICallback, 3 delegates, a runtime-host, a manipulation-host), create a feature-level-9_1 D3D11 device + a
 * portrait render target, then run PrepareResources + Draw a few times.  One Draw() runs a whole cocos2d frame
 * into the render target; after the first Draw the texture is read back and written to gd_frame.bmp.
 * Every method the component calls on a host object is logged (to trap anything the recon did not predict).
 *
 * This is a console .exe for the Surface 2 / Surface RT probe - it needs no window (M2 adds the window + present).
 * Freestanding ARM32 (clang thumbv7 /nodefaultlib); build with build_gd_host_arm32.bat.  NEVER run on the x64 PC.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include "gd_quad_shaders.h"   /* k_quad_vs / k_quad_ps (vs/ps_4_0_level_9_1 full-screen textured quad) */

/* DXGI/D3D IIDs defined inline (freestanding + /nodefaultlib pulls no dxguid.lib) */
static const GUID x_IID_IDXGIDevice    = {0x54ec77fa,0x1377,0x44e6,{0x8c,0x32,0x88,0xfd,0x5f,0x44,0xc8,0x4c}};
static const GUID x_IID_IDXGIFactory2  = {0x50c83a1c,0xe072,0x4c48,{0x87,0xb0,0x36,0x30,0xfa,0x36,0xa6,0xd0}};
static const GUID x_IID_ID3D11Texture2D= {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static const GUID x_IID_IContextCallback={0x000001da,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

/* MSVC <stdarg.h> va_start is wrong under clang-ARM (it assumes MSVC argument homing) -> garbage varargs.
 * Use the builtins, as in tools/m0probe/m0probe.c and tools/badpiggies/l9shim.c. */
#if defined(__clang__)
typedef __builtin_va_list GD_VA;
#define GD_VA_START(ap, last) __builtin_va_start(ap, last)
#define GD_VA_ARG(ap, type)   __builtin_va_arg(ap, type)
#define GD_VA_END(ap)         __builtin_va_end(ap)
#else
#include <stdarg.h>
typedef va_list GD_VA;
#define GD_VA_START(ap, last) va_start(ap, last)
#define GD_VA_ARG(ap, type)   va_arg(ap, type)
#define GD_VA_END(ap)         va_end(ap)
#endif

typedef struct HSTRING__ *HSTRING;   /* opaque WinRT string handle (not pulled in under WIN32_LEAN_AND_MEAN) */

static UINT32 wlen(const wchar_t *s) { UINT32 n = 0; while (s[n]) ++n; return n; }

/* ------------------------------------------------------------------ logging ------------------------------------------------------------------ */
static HANDLE g_log = INVALID_HANDLE_VALUE;
static char g_lbuf[1024];
static CRITICAL_SECTION g_log_cs;   /* lg() now runs on pool threads too (ctx/fopen/CUTC hooks); serialize so the trace stays readable */
static int g_log_cs_ready;          /* CRITICAL_SECTION is re-entrant per thread, so the crash handlers' lg() is still safe */

static int l_fmtu(char *b, unsigned v, int base, int up) {
    char t[32]; int n = 0, i; const char *d = up ? "0123456789ABCDEF" : "0123456789abcdef";
    if (!v) t[n++] = '0'; else while (v) { t[n++] = d[v % base]; v /= base; }
    for (i = 0; i < n; ++i) b[i] = t[n - 1 - i];
    return n;
}
/* tiny logger: %s %d %u %x %p %S(wide); every line is prefixed with [tid] so pool/worker threads can be told from the
 * render thread.  lg() now runs on pool threads too, so it serializes on g_log_cs -- but it acquires the lock with a
 * BOUNDED TryEnter: if some thread ever faulted while holding it (and the game's SEH swallowed the fault), later lg()
 * calls degrade to unlocked (possibly garbled) output instead of hanging the whole process and wasting a device run. */
static void lg(const char *f, ...) {
    GD_VA ap; char *b = g_lbuf; int cap = (int)sizeof g_lbuf - 3; int n = 0; DWORD w; int locked = 0;
    if (g_log_cs_ready) { int t; for (t = 0; t < 100; ++t) { if (TryEnterCriticalSection(&g_log_cs)) { locked = 1; break; } Sleep(1); } }
    { DWORD tid = GetCurrentThreadId(); b[n++] = '['; n += l_fmtu(b + n, (unsigned)tid, 10, 0); b[n++] = ']'; b[n++] = ' '; }
    GD_VA_START(ap, f);
    for (; *f && n < cap - 32; ++f) {
        if (*f != '%') { b[n++] = *f; continue; }
        ++f;
        if (*f == '0') ++f;                   /* zero flag (e.g. %08x) */
        while (*f >= '0' && *f <= '9') ++f;    /* field width: skip it, but STILL consume the arg (prevents a vararg-misalignment crash) */
        if (*f == 'l') ++f;                    /* length modifier: long == 32-bit on Win32-ARM, so %ld/%lu/%lx read one int-sized arg */
        if (*f == 's') { const char *s = GD_VA_ARG(ap, const char *); if (!s) s = "(null)"; while (*s && n < cap) b[n++] = *s++; }
        else if (*f == 'S') { const wchar_t *s = GD_VA_ARG(ap, const wchar_t *); if (!s) s = L"(null)"; while (*s && n < cap) { wchar_t c = *s++; b[n++] = (c < 128) ? (char)c : '?'; } }
        else if (*f == 'd') { int v = GD_VA_ARG(ap, int); if (v < 0) { b[n++] = '-'; v = -v; } n += l_fmtu(b + n, (unsigned)v, 10, 0); }
        else if (*f == 'u') { n += l_fmtu(b + n, GD_VA_ARG(ap, unsigned), 10, 0); }
        else if (*f == 'x') { n += l_fmtu(b + n, GD_VA_ARG(ap, unsigned), 16, 0); }
        else if (*f == 'p') { b[n++] = '0'; b[n++] = 'x'; n += l_fmtu(b + n, (unsigned)(size_t)GD_VA_ARG(ap, void *), 16, 0); }
        else b[n++] = *f;
    }
    GD_VA_END(ap);
    b[n++] = '\r'; b[n++] = '\n'; b[n] = 0;   /* NUL for OutputDebugStringA (file/stdout use the explicit length n) */
    OutputDebugStringA(b);
    { HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); if (o != INVALID_HANDLE_VALUE) WriteFile(o, b, (DWORD)n, &w, NULL); }
    if (g_log != INVALID_HANDLE_VALUE) { WriteFile(g_log, b, (DWORD)n, &w, NULL); FlushFileBuffers(g_log); }   /* survive a hard crash in the first Draw */
    if (locked) LeaveCriticalSection(&g_log_cs);
}
static void log_open(void) {
    wchar_t dir[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    InitializeCriticalSection(&g_log_cs); g_log_cs_ready = 1;
    wchar_t path[MAX_PATH]; int i = 0; const wchar_t *s;
    if (!n || n >= MAX_PATH) { n = GetModuleFileNameW(NULL, dir, MAX_PATH); while (n && dir[n-1] != L'\\') --n; dir[n ? n-1 : 0] = 0; }
    for (s = dir; *s && i < MAX_PATH - 16; ++s) path[i++] = *s;
    if (i && path[i-1] != L'\\') path[i++] = L'\\';
    for (s = L"gd_host.log"; *s; ++s) path[i++] = *s; path[i] = 0;
    g_log = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

/* ------------------------------------------------------------------ GUIDs ------------------------------------------------------------------ */
static const GUID IID_IUnknown2        = {0x00000000,0x0000,0x0000,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID IID_IInspectable2    = {0xAF86E2E0,0xB12D,0x4C6A,{0x9C,0x5A,0xD7,0xAA,0x65,0x10,0x1E,0x90}};
static const GUID IID_IAgileObject2    = {0x94EA2B94,0xE9CC,0x49E0,{0xC0,0xFF,0xEE,0x64,0xCA,0x8F,0x5B,0x90}};
static const GUID IID_InteropFactory   = {0xE8D96887,0xB552,0x373B,{0xAF,0xBF,0x83,0x8B,0x91,0x95,0xD4,0xFD}};
static const GUID IID_InteropPublic    = {0xD8485BA8,0x71AD,0x35C3,{0x83,0xDF,0x5B,0x24,0x4D,0xA3,0x4A,0xA6}};
static const GUID IID_ManipHandler     = {0x171B7EF7,0xA96F,0x4E37,{0xB9,0x36,0x7A,0x0B,0x10,0xF3,0x51,0x63}};
static const GUID IID_BgContentNative  = {0x262C1892,0x975A,0x45AA,{0x8F,0xE7,0xF2,0x5C,0x2C,0x40,0x88,0xE3}};
static const GUID IID_BroswerStatics   = {0x6AF73B9D,0x6C05,0x39B0,{0x90,0xFC,0x1C,0xA5,0x23,0x82,0x60,0xB1}};
static const GUID IID_ICallback        = {0x009BC214,0x16B9,0x37D2,{0x8A,0x4F,0xDD,0x76,0x28,0xD0,0xB7,0xCB}};
static const GUID IID_EventDelegate    = {0x0ED04E82,0xBBC6,0x36F7,{0xA2,0xF7,0x48,0xD8,0x3F,0x08,0x72,0xCA}};
static const GUID IID_MsgBoxDelegate   = {0x4DBE8C04,0xCE90,0x3B9D,{0x93,0xF1,0x43,0x7B,0x2A,0x8E,0xCC,0x77}};
static const GUID IID_EditBoxDelegate  = {0x3C7D6A8A,0x738B,0x3608,{0xB1,0x6B,0xE5,0x3A,0x47,0x70,0x8F,0xBA}};

static int guid_eq(const GUID *a, const GUID *b) {
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b; int i;
    for (i = 0; i < 16; ++i) if (x[i] != y[i]) return 0;
    return 1;
}

/* ------------------------------------------------------------------ WinRT / D3D entry points (loaded at run time) ------------------------------------------------------------------ */
typedef HRESULT (WINAPI *RoInitialize_t)(int);
typedef HRESULT (WINAPI *WindowsCreateString_t)(const wchar_t *, UINT32, HSTRING *);
typedef HRESULT (WINAPI *WindowsDeleteString_t)(HSTRING);
typedef HRESULT (WINAPI *DllGetActivationFactory_t)(HSTRING, void **);
typedef HRESULT (WINAPI *D3D11CreateDevice_t)(void *, int, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
                                              ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
static RoInitialize_t          pRoInitialize;
static WindowsCreateString_t   pWindowsCreateString;
static WindowsDeleteString_t   pWindowsDeleteString;

/* a generic COM vtable call by slot (this-call: first arg is the interface pointer) */
#define VCALL(iface, slot, type) ((type)(*(void ***)(iface))[(slot)])

/* ------------------------------------------------------------------ host-provided COM objects ------------------------------------------------------------------
 * Each is a singleton: AddRef/Release are no-ops (the host owns them for the whole process).  Every method logs. */

typedef struct { const void **vtbl; } Obj;   /* all fakes start with a vtable pointer */

static ULONG   STDMETHODCALLTYPE o_AddRef(void *t)  { (void)t; return 2; }
static ULONG   STDMETHODCALLTYPE o_Release(void *t) { (void)t; return 1; }
static HRESULT STDMETHODCALLTYPE insp_GetIids(void *t, ULONG *n, GUID **p) { (void)t; if (n) *n = 0; if (p) *p = NULL; return S_OK; }
static HRESULT STDMETHODCALLTYPE insp_GetRuntimeClassName(void *t, HSTRING *s) { (void)t; if (s) *s = NULL; return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE insp_GetTrustLevel(void *t, int *tl) { (void)t; if (tl) *tl = 0; return S_OK; }

/* ---- ICallback (IInspectable + 5 methods) ---- */
static HRESULT STDMETHODCALLTYPE cb_QI(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid, &IID_IUnknown2) || guid_eq(iid, &IID_IInspectable2) || guid_eq(iid, &IID_IAgileObject2) || guid_eq(iid, &IID_ICallback)) { *o = t; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static HRESULT STDMETHODCALLTYPE cb_getUniqueUserID(void *t, HSTRING *out) {
    (void)t; lg("  ICallback.getUniqueUserID");
    if (out) pWindowsCreateString(L"0000000000000000000000000000000000000000", 40, out);
    return S_OK;
}
/* Report NO network: GD's loading does a PlayFab/account step near the end; with network "available" it blocks waiting for
 * servers it cannot reach on this device (loading freezes ~80%).  Offline -> it skips that and finishes to the menu. */
static HRESULT STDMETHODCALLTYPE cb_isNetworkAvailable(void *t, unsigned char *out) { (void)t; lg("  ICallback.isNetworkAvailable -> FALSE (offline, so loading does not wait on PlayFab)"); if (out) *out = 0; return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_openEmail(void *t, HSTRING a, HSTRING b, HSTRING c) { (void)t;(void)a;(void)b;(void)c; lg("  ICallback.openEmail (ignored)"); return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_isLowMemoryDevice(void *t, unsigned char *out) { (void)t; lg("  ICallback.isLowMemoryDevice -> FALSE"); if (out) *out = 0; return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_rateApp(void *t) { (void)t; lg("  ICallback.rateApp (ignored)"); return S_OK; }
static const void *cb_vt[] = { (void*)cb_QI,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)cb_getUniqueUserID,(void*)cb_isNetworkAvailable,(void*)cb_openEmail,(void*)cb_isLowMemoryDevice,(void*)cb_rateApp };
static Obj g_callback = { cb_vt };

/* ---- the three Cocos2d* delegates (IUnknown + Invoke = slot 3) ---- */
static volatile int g_want_quit;
static HRESULT STDMETHODCALLTYPE dg_QI_make(void *t, const GUID *iid, void **o, const GUID *self) {
    if (guid_eq(iid, &IID_IUnknown2) || guid_eq(iid, &IID_IAgileObject2) || guid_eq(iid, self)) { *o = t; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static HRESULT STDMETHODCALLTYPE evt_QI(void *t, const GUID *iid, void **o) { return dg_QI_make(t, iid, o, &IID_EventDelegate); }
static HRESULT STDMETHODCALLTYPE evt_Invoke(void *t, int ev) { (void)t; lg("  Cocos2dEventDelegate.Invoke(%d) [0=ShowKbd 1=HideKbd 2=Terminate]", ev); if (ev == 2) g_want_quit = 1; return S_OK; }
static const void *evt_vt[] = { (void*)evt_QI,(void*)o_AddRef,(void*)o_Release,(void*)evt_Invoke };
static Obj g_evt_delegate = { evt_vt };

static HRESULT STDMETHODCALLTYPE mbx_QI(void *t, const GUID *iid, void **o) { return dg_QI_make(t, iid, o, &IID_MsgBoxDelegate); }
static HRESULT STDMETHODCALLTYPE mbx_Invoke(void *t, HSTRING title, HSTRING text) { (void)t;(void)title;(void)text; lg("  Cocos2dMessageBoxDelegate.Invoke (ignored)"); return S_OK; }
static const void *mbx_vt[] = { (void*)mbx_QI,(void*)o_AddRef,(void*)o_Release,(void*)mbx_Invoke };
static Obj g_mbx_delegate = { mbx_vt };

static HRESULT STDMETHODCALLTYPE edt_QI(void *t, const GUID *iid, void **o) { return dg_QI_make(t, iid, o, &IID_EditBoxDelegate); }
static HRESULT STDMETHODCALLTYPE edt_Invoke(void *t, HSTRING ph, HSTRING tx, int maxLen, int inputMode, int inputFlag, void *handler) {
    (void)t;(void)ph;(void)tx;(void)maxLen;(void)inputMode;(void)inputFlag;(void)handler; lg("  Cocos2dEditBoxDelegate.Invoke (ignored)"); return S_OK;
}
static const void *edt_vt[] = { (void*)edt_QI,(void*)o_AddRef,(void*)o_Release,(void*)edt_Invoke };
static Obj g_edt_delegate = { edt_vt };

/* ---- IDrawingSurfaceRuntimeHostNative (IUnknown + slot 3 RequestAdditionalFrame; extra slots are safe stubs, gd-component F7) ---- */
static volatile int g_additional_frame;
static HRESULT STDMETHODCALLTYPE rh_QI(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid, &IID_IUnknown2) || guid_eq(iid, &IID_IAgileObject2)) { *o = t; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static HRESULT STDMETHODCALLTYPE rh_RequestAdditionalFrame(void *t) { (void)t; g_additional_frame = 1; return S_OK; }
static HRESULT STDMETHODCALLTYPE rh_stub(void *t) { (void)t; lg("  runtime-host: UNEXPECTED extra slot called"); return E_NOTIMPL; }
static const void *rh_vt[] = { (void*)rh_QI,(void*)o_AddRef,(void*)o_Release,(void*)rh_RequestAdditionalFrame,
    (void*)rh_stub,(void*)rh_stub,(void*)rh_stub,(void*)rh_stub,(void*)rh_stub };
static Obj g_runtime_host = { rh_vt };

/* ---- DrawingSurfaceManipulationHost (IInspectable + add/remove Pointer Moved/Pressed/Released at slots 6..11, gd-component F8) ---- */
static void *g_ptr_moved, *g_ptr_pressed, *g_ptr_released;   /* the component's TypedEventHandlers, stored for M3 */
static HRESULT STDMETHODCALLTYPE mh_QI(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid, &IID_IUnknown2) || guid_eq(iid, &IID_IInspectable2) || guid_eq(iid, &IID_IAgileObject2)) { *o = t; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static long long g_tok;
/* the event source owns the delegate's lifetime: AddRef on add_, or the component frees it and the first touch
 * dereferences freed memory (observed: AV through a garbage vtable on the first tap). */
static void dg_hold(void *d) { if (d) VCALL(d, 1, ULONG(STDMETHODCALLTYPE*)(void*))(d); }
static HRESULT STDMETHODCALLTYPE mh_add_moved(void *t, void *d, long long *tok)   { (void)t; g_ptr_moved = d;   dg_hold(d); lg("  ManipulationHost.add_PointerMoved");   if (tok) *tok = ++g_tok; return S_OK; }
static HRESULT STDMETHODCALLTYPE mh_rm_moved(void *t, long long tok)              { (void)t;(void)tok; return S_OK; }
static HRESULT STDMETHODCALLTYPE mh_add_pressed(void *t, void *d, long long *tok) { (void)t; g_ptr_pressed = d; dg_hold(d); lg("  ManipulationHost.add_PointerPressed"); if (tok) *tok = ++g_tok; return S_OK; }
static HRESULT STDMETHODCALLTYPE mh_rm_pressed(void *t, long long tok)            { (void)t;(void)tok; return S_OK; }
static HRESULT STDMETHODCALLTYPE mh_add_released(void *t, void *d, long long *tok){ (void)t; g_ptr_released = d; dg_hold(d); lg("  ManipulationHost.add_PointerReleased"); if (tok) *tok = ++g_tok; return S_OK; }
static HRESULT STDMETHODCALLTYPE mh_rm_released(void *t, long long tok)           { (void)t;(void)tok; return S_OK; }
static const void *mh_vt[] = { (void*)mh_QI,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)mh_add_moved,(void*)mh_rm_moved,(void*)mh_add_pressed,(void*)mh_rm_pressed,(void*)mh_add_released,(void*)mh_rm_released };
static Obj g_manip_host = { mh_vt };

/* ------------------------------------------------------------------ BMP dump ------------------------------------------------------------------ */
static void write_bmp(const wchar_t *path, const unsigned char *bgra, int w, int h, int rowpitch) {
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD wr; int y, x; unsigned char hdr[54]; int row = (w * 3 + 3) & ~3; unsigned sz = 54 + (unsigned)row * h;
    unsigned char *line;
    if (f == INVALID_HANDLE_VALUE) return;
    for (y = 0; y < 54; ++y) hdr[y] = 0;
    hdr[0]='B'; hdr[1]='M'; *(unsigned*)(hdr+2)=sz; *(unsigned*)(hdr+10)=54;
    *(unsigned*)(hdr+14)=40; *(int*)(hdr+18)=w; *(int*)(hdr+22)=h; *(unsigned short*)(hdr+26)=1; *(unsigned short*)(hdr+28)=24;
    *(unsigned*)(hdr+34)=(unsigned)row*h;
    WriteFile(f, hdr, 54, &wr, NULL);
    line = (unsigned char *)VirtualAlloc(NULL, (SIZE_T)row, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    for (y = h - 1; y >= 0; --y) {                       /* BMP is bottom-up */
        const unsigned char *src = bgra + (SIZE_T)y * rowpitch;
        for (x = 0; x < w; ++x) { line[x*3+0]=src[x*4+0]; line[x*3+1]=src[x*4+1]; line[x*3+2]=src[x*4+2]; }
        for (x = w*3; x < row; ++x) line[x] = 0;
        WriteFile(f, line, (DWORD)row, &wr, NULL);
    }
    if (line) VirtualFree(line, 0, MEM_RELEASE);
    CloseHandle(f);
}

/* ------------------------------------------------------------------ WinRT fakes: Package + ApplicationData (asset / save paths) ------------------------------------------------------------------
 * cocos2d (CCFileUtilsWinRT) finds its assets via Windows.ApplicationModel.Package.Current.InstalledLocation.Path and its
 * writable/save root via Windows.Storage.ApplicationData.Current.LocalFolder.Path (gd-platform F2/F4).  Both throw in a
 * plain Win32 process (no package identity).  We fake just these two chains; every other class is forwarded to the OS. */
static const GUID IID_IActivationFactory      = {0x00000035,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID IID_IPackageStatics         = {0x4E534BDF,0x2960,0x4878,{0x97,0xA4,0x96,0x24,0xDE,0xB7,0x2F,0x2D}};
static const GUID IID_IStorageItem            = {0x4207A996,0xCA2F,0x42F7,{0xBD,0xE8,0x8B,0x10,0x45,0x7A,0x7F,0x30}};
static const GUID IID_IApplicationDataStatics = {0x5612147B,0xE843,0x45E3,{0x94,0xD8,0x06,0x16,0x9E,0x3C,0x8E,0x17}};

static wchar_t g_install_path[MAX_PATH];   /* folder that holds Assets\ (the exe dir) */
static wchar_t g_local_path[MAX_PATH];     /* a real writable folder for save data */

/* a StorageFolder/StorageItem carrying a path; IStorageItem.get_Path is slot 12 (offset 0x30) */
typedef struct { const void **vtbl; const wchar_t *path; } StorageItem;
static HRESULT STDMETHODCALLTYPE si_QI(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid,&IID_IUnknown2)||guid_eq(iid,&IID_IInspectable2)||guid_eq(iid,&IID_IAgileObject2)||guid_eq(iid,&IID_IStorageItem)) { *o=t; return S_OK; }
    *o=NULL; return E_NOINTERFACE;
}
static HRESULT STDMETHODCALLTYPE si_stub(void *t) { (void)t; return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE si_get_Path(void *t, HSTRING *out) {
    StorageItem *s = (StorageItem *)t;
    lg("  StorageItem.get_Path -> %S", s->path);
    return out ? pWindowsCreateString(s->path, wlen(s->path), out) : E_POINTER;
}
static const void *si_vt[] = { (void*)si_QI,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)si_stub,(void*)si_stub,(void*)si_stub,(void*)si_stub,(void*)si_stub,(void*)si_stub,   /* 6..11 (Rename/Delete/GetBasicProperties/get_Name) */
    (void*)si_get_Path,                                                                          /* 12 get_Path */
    (void*)si_stub,(void*)si_stub,(void*)si_stub };                                              /* 13..15 */
static StorageItem g_install_item = { si_vt, g_install_path };
static StorageItem g_local_item   = { si_vt, g_local_path };

static HRESULT STDMETHODCALLTYPE insp_QI_agile(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid,&IID_IUnknown2)||guid_eq(iid,&IID_IInspectable2)||guid_eq(iid,&IID_IAgileObject2)) { *o=t; return S_OK; }
    *o=NULL; return E_NOINTERFACE;
}
/* IPackage: get_InstalledLocation is slot 7 */
static HRESULT STDMETHODCALLTYPE pkg_get_InstalledLocation(void *t, void **o) { (void)t; lg("  IPackage.get_InstalledLocation"); if (o) *o = &g_install_item; return S_OK; }
static const void *pkg_vt[] = { (void*)insp_QI_agile,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)si_stub,(void*)pkg_get_InstalledLocation,(void*)si_stub };   /* 6 get_Id, 7 get_InstalledLocation, 8 get_Dependencies */
static Obj g_pkg = { pkg_vt };
/* IPackageStatics: get_Current is slot 6 */
static HRESULT STDMETHODCALLTYPE pkgstat_QI(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid,&IID_IUnknown2)||guid_eq(iid,&IID_IInspectable2)||guid_eq(iid,&IID_IAgileObject2)||guid_eq(iid,&IID_IActivationFactory)||guid_eq(iid,&IID_IPackageStatics)) { *o=t; return S_OK; }
    *o=NULL; return E_NOINTERFACE;
}
static HRESULT STDMETHODCALLTYPE pkgstat_get_Current(void *t, void **o) { (void)t; lg("  IPackageStatics.get_Current"); if (o) *o = &g_pkg; return S_OK; }
static const void *pkgstat_vt[] = { (void*)pkgstat_QI,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)pkgstat_get_Current };   /* 6 get_Current */
static Obj g_package_statics = { pkgstat_vt };

/* IApplicationData: get_LocalFolder is slot 12 */
static HRESULT STDMETHODCALLTYPE ad_get_LocalFolder(void *t, void **o) { (void)t; lg("  IApplicationData.get_LocalFolder"); if (o) *o = &g_local_item; return S_OK; }
static const void *ad_vt[] = { (void*)insp_QI_agile,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)si_stub,(void*)si_stub,(void*)si_stub,(void*)si_stub,(void*)si_stub,(void*)si_stub,   /* 6..11 (get_Version/.../get_RoamingSettings) */
    (void*)ad_get_LocalFolder,                                                                   /* 12 get_LocalFolder */
    (void*)si_stub,(void*)si_stub,(void*)si_stub };
static Obj g_appdata = { ad_vt };
/* IApplicationDataStatics: get_Current is slot 6 */
static HRESULT STDMETHODCALLTYPE adstat_QI(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid,&IID_IUnknown2)||guid_eq(iid,&IID_IInspectable2)||guid_eq(iid,&IID_IAgileObject2)||guid_eq(iid,&IID_IActivationFactory)||guid_eq(iid,&IID_IApplicationDataStatics)) { *o=t; return S_OK; }
    *o=NULL; return E_NOINTERFACE;
}
static HRESULT STDMETHODCALLTYPE adstat_get_Current(void *t, void **o) { (void)t; lg("  IApplicationDataStatics.get_Current"); if (o) *o = &g_appdata; return S_OK; }
static const void *adstat_vt[] = { (void*)adstat_QI,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)adstat_get_Current };
static Obj g_appdata_statics = { adstat_vt };

/* ------------------------------------------------------------------ RoGetActivationFactory hook + crash handler (diagnostic) ------------------------------------------------------------------
 * The component (via vccorlib110 / libEGL / fmod) activates WinRT classes during Draw (asset path via
 * Windows.ApplicationModel.Package, storage, display properties ...).  We IAT-patch RoGetActivationFactory in the
 * loaded game DLLs to LOG every class requested (then forward to the real OS), and install a crash filter, so the
 * next device run shows the exact activation sequence and where it stops.  (The real shims replace the forward later.) */
typedef HRESULT (WINAPI *RoGAF_t)(HSTRING, const GUID *, void **);
typedef const wchar_t * (WINAPI *WindowsGetStringRawBuffer_t)(HSTRING, UINT32 *);
static RoGAF_t g_real_RoGAF;
static WindowsGetStringRawBuffer_t pWindowsGetStringRawBuffer;

/* mirror the game's own OutputDebugString (cocos2d CCLog: asset loads, errors, scene changes) into gd_host.log */
typedef void (WINAPI *ODSW_t)(const wchar_t *);
typedef void (WINAPI *ODSA_t)(const char *);
static ODSW_t g_real_ODSW;
static ODSA_t g_real_ODSA;
static volatile long g_ods_count;
static void WINAPI hook_ODSW(const wchar_t *s) { long n = g_ods_count++; if (n < 2000 && s) lg("[cc] %S", s); if (g_real_ODSW) g_real_ODSW(s); }
static void WINAPI hook_ODSA(const char *s)    { long n = g_ods_count++; if (n < 2000 && s) lg("[cc] %s", s); if (g_real_ODSA) g_real_ODSA(s); }

/* log every file the game opens, to see where asset loading stalls (cdecl; on ARM one calling convention) */
typedef void * (*fopen_t)(const char *, const char *);
static fopen_t g_real_fopen;
static volatile long g_fopen_count;
static char g_last_asset[300];                    /* last file opened, for the device-removal pinpoint */
static void *hook_fopen(const char *path, const char *mode) {
    long n = g_fopen_count++;
    if (path) { int i = 0; while (path[i] && i < (int)sizeof g_last_asset - 1) { g_last_asset[i] = path[i]; ++i; } g_last_asset[i] = 0; }
    if (n < 3000 && path) lg("  [fopen #%ld] %s (mode %s)", n, path, mode ? mode : "?");
    return g_real_fopen ? g_real_fopen(path, mode) : NULL;
}

/* ------------------------------------------------------------------ UI-thread continuation context (the ~80% loading-stall fix) ------------------------------------------------------------------
 * GD loads its assets with C++/CX PPL `concurrency::task<byte>().then(lambda)`.  On this MTA process the continuation
 * captures the "UI thread" through _ContextCallback::_Capture -> CaptureUiThreadContext(&ctx)
 * (GeometryJumpComponent sub_2ce94 / libcocos2d sub_8c628, verified by static RE).  The old PhoneAppModelHost shim
 * returned E_NOTIMPL, so _Capture stored NULL and every continuation ran on the thread-pool thread that completed the
 * antecedent.  cocos2d/GL work is thread-affine, so those texture-load completions did nothing useful and the loading
 * counter froze at ~80%.
 *
 * Fix: return a real IContextCallback whose ContextCallback() marshals the continuation onto THIS render thread,
 * which drains the queue once per frame (between Draws).  Critical detail from the RE: _Capture passes a NON-NULL
 * ppContext, while _IsCurrentOriginSTA (sub_2d8f8) passes ppContext==NULL -- and if that STA test returns S_OK,
 * task::_Wait throws "Illegal to wait on a task in a Windows Runtime STA" (sub_2d324).  So hook_CUTC returns a
 * context ONLY for the capture (pp != NULL) and E_NOTIMPL for the STA test (pp == NULL): continuations marshal to the
 * render thread, yet task waits stay legal. */
typedef HRESULT (STDMETHODCALLTYPE *PFNCONTEXTCALL)(void *comCallData);
typedef struct { PFNCONTEXTCALL fn; void *param; HANDLE done; volatile HRESULT hr; volatile int state; } DispItem;  /* state: 0 free, 1 queued, 2 running */
#define DISP_MAX 64
static DispItem g_disp[DISP_MAX];
static CRITICAL_SECTION g_disp_cs;
static volatile int g_disp_ready;
static DWORD g_render_tid;
static volatile long g_ctx_calls;

static void disp_init(void) {
    int i;
    InitializeCriticalSection(&g_disp_cs);
    for (i = 0; i < DISP_MAX; ++i) { g_disp[i].done = CreateEventW(NULL, FALSE, FALSE, NULL); g_disp[i].state = 0; }
    g_disp_ready = 1;
}

/* run on the render thread once per frame: execute every queued continuation, then release its waiter */
static void disp_drain(void) {
    int i;
    if (!g_disp_ready) return;
    for (;;) {
        DispItem *it = NULL;
        EnterCriticalSection(&g_disp_cs);
        for (i = 0; i < DISP_MAX; ++i) if (g_disp[i].state == 1) { g_disp[i].state = 2; it = &g_disp[i]; break; }
        LeaveCriticalSection(&g_disp_cs);
        if (!it) break;
        it->hr = it->fn ? it->fn(it->param) : S_OK;
        SetEvent(it->done);          /* the thread waiting in ctx_ContextCallback wakes, reads hr, frees the slot */
    }
}

static HRESULT STDMETHODCALLTYPE ctx_QI(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid, &IID_IUnknown2) || guid_eq(iid, &IID_IAgileObject2) || guid_eq(iid, &x_IID_IContextCallback)) { *o = t; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
/* IContextCallback::ContextCallback(pfn, pParam, riid, iMethod, pUnk) -- run pfn(pParam) on the render thread */
static HRESULT STDMETHODCALLTYPE ctx_ContextCallback(void *t, PFNCONTEXTCALL pfn, void *pParam, const GUID *riid, int iMethod, void *pUnk) {
    long n; int i; DispItem *it = NULL;
    (void)t; (void)riid; (void)iMethod; (void)pUnk;
    n = g_ctx_calls++;
    if (!pfn) return S_OK;
    if (GetCurrentThreadId() == g_render_tid) { if (n < 40) lg("  ctx #%ld: inline (already on render thread)", n); return pfn(pParam); }
    EnterCriticalSection(&g_disp_cs);
    for (i = 0; i < DISP_MAX; ++i) if (g_disp[i].state == 0) { it = &g_disp[i]; it->fn = pfn; it->param = pParam; ResetEvent(it->done); it->state = 1; break; }
    LeaveCriticalSection(&g_disp_cs);
    if (!it) { if (n < 40) lg("  ctx #%ld: queue FULL -> running inline (wrong thread)", n); return pfn(pParam); }
    if (n < 40) lg("  ctx #%ld: marshal to render thread (slot %d)", n, (int)(it - g_disp));
    { int timeouts = 0;
      for (;;) {
          DWORD w = WaitForSingleObject(it->done, 5000);
          if (w == WAIT_OBJECT_0) break;
          ++timeouts;
          lg("  ctx #%ld: WAIT >%ds for render thread (slot %d) -- render thread not draining", n, timeouts * 5, (int)(it - g_disp));
          if (timeouts >= 4) {   /* ~20s: take the item back if drain has not claimed it, and run inline rather than hang forever */
              int withdrawn = 0;
              EnterCriticalSection(&g_disp_cs);
              if (it->state == 1) { it->state = 0; withdrawn = 1; }   /* still queued -> safe to reclaim; state==2 means drain owns it, keep waiting */
              LeaveCriticalSection(&g_disp_cs);
              if (withdrawn) { lg("  ctx #%ld: withdrew after 20s, running inline (wrong thread, last resort)", n); return pfn(pParam); }
          }
      }
    }
    { HRESULT hr = it->hr; EnterCriticalSection(&g_disp_cs); it->state = 0; LeaveCriticalSection(&g_disp_cs); return hr; }
}
static const void *ctx_vt[] = { (void*)ctx_QI, (void*)o_AddRef, (void*)o_Release, (void*)ctx_ContextCallback };
static Obj g_ui_context = { ctx_vt };

/* CaptureUiThreadContext hook (overrides the PhoneAppModelHost shim via IAT patch).  See the block comment above. */
static volatile long g_cutc_cap, g_cutc_sta;
static HRESULT WINAPI hook_CUTC(void **ppContext) {
    if (ppContext) { *ppContext = &g_ui_context; if (g_cutc_cap++ < 20) lg("  CaptureUiThreadContext(capture) -> host UI context"); return S_OK; }
    if (g_cutc_sta++ < 4) lg("  CaptureUiThreadContext(STA test) -> E_NOTIMPL (we are MTA; task waits stay legal)");
    return E_NOTIMPL;
}

static int cls_is(const wchar_t *name, const wchar_t *lit) { return name && lstrcmpW(name, lit) == 0; }

static HRESULT WINAPI hook_RoGAF(HSTRING classId, const GUID *iid, void **out) {
    const wchar_t *name = (pWindowsGetStringRawBuffer && classId) ? pWindowsGetStringRawBuffer(classId, NULL) : NULL;
    HRESULT hr;
    unsigned iid0 = iid ? ((const unsigned *)iid)[0] : 0;               /* Data1 of the requested iid, for the log */
    if (cls_is(name, L"Windows.ApplicationModel.Package")) {
        if (out) *out = &g_package_statics;                            /* return our object; vccorlib then QIs it for IPackageStatics */
        lg("  RoGetActivationFactory(\"%S\", iid=%08x) -> host IPackageStatics", name, iid0);
        return S_OK;
    } else if (cls_is(name, L"Windows.Storage.ApplicationData")) {
        if (out) *out = &g_appdata_statics;
        lg("  RoGetActivationFactory(\"%S\", iid=%08x) -> host IApplicationDataStatics", name, iid0);
        return S_OK;
    }
    hr = g_real_RoGAF ? g_real_RoGAF(classId, iid, out) : E_NOTIMPL;
    lg("  RoGetActivationFactory(\"%S\") -> OS hr=0x%x factory=%p", name ? name : L"?", (unsigned)hr, out ? *out : NULL);
    return hr;
}

/* patch every IAT slot that imports function `fn` (by name) in the loaded module `mod`, to point at `newfn`. */
static int iat_patch(HMODULE mod, const char *fn, void *newfn) {
    unsigned char *base = (unsigned char *)mod;
    DWORD pe, impRva, i; IMAGE_IMPORT_DESCRIPTOR *imp; int patched = 0;
    if (!base) return 0;
    pe = *(DWORD *)(base + 0x3c);
    impRva = *(DWORD *)(base + pe + 24 + 96 + 8);     /* DataDirectory[1] (import) VA */
    if (!impRva) return 0;
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + impRva);
    for (; imp->Name; ++imp) {
        DWORD oftRva = imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk;
        DWORD *oft = (DWORD *)(base + oftRva);
        void **iat = (void **)(base + imp->FirstThunk);
        for (i = 0; oft[i]; ++i) {
            if (oft[i] & 0x80000000u) continue;          /* imported by ordinal */
            {
                const char *nm = (const char *)(base + (oft[i] & 0x7fffffffu) + 2);
                if (lstrcmpA(nm, fn) == 0) {
                    DWORD old;
                    if (VirtualProtect(&iat[i], sizeof(void *), PAGE_READWRITE, &old)) {
                        iat[i] = newfn; VirtualProtect(&iat[i], sizeof(void *), old, &old); ++patched;
                    }
                }
            }
        }
    }
    return patched;
}

/* patch one code byte at module+rva (verifies the expected current byte first), for the ANGLE feature-level clamp.
 * This edits the loaded image in memory only -- it never touches the shipped DLL on disk. */
static int code_patch(HMODULE mod, DWORD rva, unsigned char expect, unsigned char val) {
    unsigned char *p = (unsigned char *)mod + rva; DWORD old;
    if (!mod) return 0;
    if (*p != expect) { lg("  code_patch @0x%x: byte 0x%x != expected 0x%x -> SKIP", (unsigned)rva, (unsigned)*p, (unsigned)expect); return 0; }
    if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) return 0;
    *p = val; VirtualProtect(p, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 1);
    return 1;
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep) {
    void *addr = ep->ExceptionRecord->ExceptionAddress;
    HMODULE m = NULL; char path[MAX_PATH]; const char *bn = "?"; unsigned off = 0;
    path[0] = 0;
    if (GetModuleHandleExW(0x4 /*FROM_ADDRESS*/ | 0x2 /*UNCHANGED_REFCOUNT*/, (LPCWSTR)addr, &m) && m) {
        GetModuleFileNameA(m, path, MAX_PATH);
        { const char *s = path; bn = path; for (; *s; ++s) if (*s == '\\') bn = s + 1; }
        off = (unsigned)((unsigned char *)addr - (unsigned char *)m);
    }
    lg("!!! CRASH code=0x%x addr=%p in %s+0x%x", (unsigned)ep->ExceptionRecord->ExceptionCode, addr, bn, off);
    if (g_log != INVALID_HANDLE_VALUE) FlushFileBuffers(g_log);
    return EXCEPTION_EXECUTE_HANDLER;
}

/* first-chance handler: logs hard faults (AV / fail-fast / stack overflow) with module+offset even when a C++/CX
 * terminate->fail-fast would bypass the unhandled-exception filter.  Ignores ordinary C++ exceptions (0xE06D7363). */
static LONG WINAPI gd_veh(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == 0xC0000005u || code == 0xC0000409u || code == 0xC00000FDu) {
        void *addr = ep->ExceptionRecord->ExceptionAddress;
        HMODULE m = NULL; char path[MAX_PATH]; const char *bn = "?"; unsigned off = 0; path[0] = 0;
        if (GetModuleHandleExW(0x4 | 0x2, (LPCWSTR)addr, &m) && m) {
            GetModuleFileNameA(m, path, MAX_PATH);
            { const char *s = path; bn = path; for (; *s; ++s) if (*s == '\\') bn = s + 1; }
            off = (unsigned)((unsigned char *)addr - (unsigned char *)m);
        }
        lg("!!! VEH fault code=0x%x addr=%p in %s+0x%x", (unsigned)code, addr, bn, off);
        if (g_log != INVALID_HANDLE_VALUE) FlushFileBuffers(g_log);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ------------------------------------------------------------------ M2 window + rotated present, M3 input ------------------------------------------------------------------ */
#define RTW 480     /* portrait render target the game draws into (gd-component F9); the window is landscape RTHxRTW */
#define RTH 800
static int g_win_w = RTH, g_win_h = RTW;    /* landscape window client size (800x480) */

static ID3D11Device         *g_dev;
static ID3D11DeviceContext  *g_ctx;
static void                 *g_interop;     /* Direct3DInterop default iface (keyboard) */
static volatile int          g_quit;

/* --- fake Windows.UI.Core.PointerEventArgs / Windows.UI.Input.PointerPoint ---
 * The component reads these through raw vtable calls (no QI on them); the ARGS is QI'd for IAgileObject (gd-component F8),
 * so it must succeed there or the component captures a COM context.  A ring keeps queued events' coords distinct until Draw drains them. */
typedef struct { const void **vtbl; float x, y; unsigned id; } PtrPoint;
typedef struct { const void **vtbl; PtrPoint *pt; } PtrArgs;
typedef struct { float X, Y; } WF_Point;

static HRESULT STDMETHODCALLTYPE pp_QI(void *t, const GUID *iid, void **o) {
    if (guid_eq(iid,&IID_IUnknown2)||guid_eq(iid,&IID_IInspectable2)||guid_eq(iid,&IID_IAgileObject2)) { *o=t; return S_OK; }
    *o=NULL; return E_NOINTERFACE;
}
static HRESULT STDMETHODCALLTYPE pp_stub(void *t) { (void)t; return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE pp_get_Position(void *t, WF_Point *o) { PtrPoint *p=(PtrPoint*)t; if (o){o->X=p->x;o->Y=p->y;} return S_OK; }
static HRESULT STDMETHODCALLTYPE pp_get_PointerId(void *t, unsigned *o) { PtrPoint *p=(PtrPoint*)t; if (o)*o=p->id; return S_OK; }
static const void *pp_vt[] = { (void*)pp_QI,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)pp_stub,          /* 6 get_PointerDevice */
    (void*)pp_get_Position,  /* 7 get_Position */
    (void*)pp_stub,          /* 8 get_RawPosition */
    (void*)pp_get_PointerId  /* 9 get_PointerId */ };
static HRESULT STDMETHODCALLTYPE pa_get_CurrentPoint(void *t, void **o) { PtrArgs *a=(PtrArgs*)t; if (o)*o=a->pt; return S_OK; }
static const void *pa_vt[] = { (void*)pp_QI,(void*)o_AddRef,(void*)o_Release,(void*)insp_GetIids,(void*)insp_GetRuntimeClassName,(void*)insp_GetTrustLevel,
    (void*)pa_get_CurrentPoint /* 6 get_CurrentPoint */ };

#define PTR_RING 64
static PtrPoint g_pp_ring[PTR_RING];
static PtrArgs  g_pa_ring[PTR_RING];
static volatile long g_ring_i;
static int g_ring_ready;
static void ptr_ring_init(void) { int i; for (i=0;i<PTR_RING;++i){ g_pp_ring[i].vtbl=pp_vt; g_pa_ring[i].vtbl=pa_vt; g_pa_ring[i].pt=&g_pp_ring[i]; } g_ring_ready=1; }

/* kind: 0 pressed, 1 moved, 2 released.  (mx,my) = window client pixels.  Maps to the portrait RT pixel under the cursor
 * (present is a 90-deg CCW rotation, so window(u,v) shows RT uv (v,1-u); RT pixel = (v*RTW, (1-u)*RTH)). */
static volatile long g_ptr_count;
static void send_pointer(int kind, int mx, int my, unsigned id) {
    void *h = kind==0 ? g_ptr_pressed : kind==1 ? g_ptr_moved : g_ptr_released;
    long i, n;
    HRESULT hr;
    if (!h || !g_ring_ready) return;
    n = g_ptr_count++;
    i = (g_ring_i++) & (PTR_RING - 1);
    /* matches the present rotation (window(u,v) -> RT uv (1-v, u)); RT pixel = ((1-v)*RTW, u*RTH) */
    g_pp_ring[i].x = (1.0f - (float)my / (float)g_win_h) * (float)RTW;
    g_pp_ring[i].y = ((float)mx / (float)g_win_w) * (float)RTH;
    g_pp_ring[i].id = id;
    if (n < 8) lg("  pointer kind=%d win=(%d,%d) rt=(%d,%d) handler=%p invoking...", kind, mx, my, (int)g_pp_ring[i].x, (int)g_pp_ring[i].y, h);
    hr = VCALL(h, 3, HRESULT(STDMETHODCALLTYPE*)(void*,void*,void*))(h, &g_manip_host, &g_pa_ring[i]);   /* TypedEventHandler::Invoke(sender, args) */
    if (n < 8) lg("  pointer invoke returned hr=0x%x", (unsigned)hr);
}

/* Touch latency: a touch tablet (Surface 2) by default feeds this window MOUSE messages synthesized from touch, but only
 * AFTER gesture recognition (press-and-hold detection adds ~300-500 ms before the click is promoted) -- that is the "jump
 * delay" in GD.  We instead take raw WM_TOUCH (delivered immediately), disable press-and-hold, and ignore the late
 * promoted mouse clicks that follow a real touch.  The mouse handlers stay as a fallback for a mouse/trackpad. */
static volatile DWORD g_last_touch;   /* GetTickCount of the last real WM_TOUCH; promoted mouse within the window is a dup */

static LRESULT CALLBACK gd_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int mx = (int)(short)(l & 0xffff), my = (int)(short)((l >> 16) & 0xffff);
    switch (m) {
    case WM_CLOSE: case WM_DESTROY: g_quit = 1; PostQuitMessage(0); return 0;
    case 0x0240: {   /* WM_TOUCH: raw touch, no promotion/press-and-hold delay */
        UINT n = (UINT)(w & 0xffff), i; TOUCHINPUT ti[10];
        if (n > 10) n = 10;
        if (GetTouchInputInfo((HTOUCHINPUT)l, n, ti, (int)sizeof(TOUCHINPUT))) {
            g_last_touch = GetTickCount();
            for (i = 0; i < n; ++i) {
                POINT p; p.x = ti[i].x / 100; p.y = ti[i].y / 100; ScreenToClient(h, &p);
                if      (ti[i].dwFlags & 0x0002) send_pointer(0, p.x, p.y, ti[i].dwID + 2);   /* TOUCHEVENTF_DOWN */
                else if (ti[i].dwFlags & 0x0004) send_pointer(2, p.x, p.y, ti[i].dwID + 2);   /* TOUCHEVENTF_UP   */
                else if (ti[i].dwFlags & 0x0001) send_pointer(1, p.x, p.y, ti[i].dwID + 2);   /* TOUCHEVENTF_MOVE */
            }
            CloseTouchInputHandle((HTOUCHINPUT)l);
            return 0;
        }
        break;
    }
    case 0x02CC:   /* WM_TABLETQUERYSYSTEMGESTURESTATUS: disable press-and-hold (the touch-down delay) + flicks */
        return 0x00000001 /*TABLET_DISABLE_PRESSANDHOLD*/ | 0x00010000 /*TABLET_DISABLE_FLICKS*/;
    case WM_LBUTTONDOWN: if (GetTickCount() - g_last_touch < 1000) return 0; SetCapture(h); send_pointer(0, mx, my, 1); return 0;
    case WM_MOUSEMOVE:   if (GetTickCount() - g_last_touch < 1000) return 0; if (w & MK_LBUTTON) send_pointer(1, mx, my, 1); return 0;
    case WM_LBUTTONUP:   if (GetTickCount() - g_last_touch < 1000) return 0; ReleaseCapture(); send_pointer(2, mx, my, 1); return 0;
    case WM_KEYDOWN:
        if (g_interop) {
            if (w == VK_ESCAPE)      VCALL(g_interop, 12, HRESULT(STDMETHODCALLTYPE*)(void*))(g_interop);           /* OnBackKeyPress */
            else if (w == VK_RETURN) VCALL(g_interop, 13, HRESULT(STDMETHODCALLTYPE*)(void*,int))(g_interop, 3);    /* Enter */
            else if (w == VK_BACK)   VCALL(g_interop, 13, HRESULT(STDMETHODCALLTYPE*)(void*,int))(g_interop, 2);    /* Back */
        }
        return 0;
    case WM_CHAR:
        if (g_interop && w >= 32 && w < 0xd800) {
            HSTRING s = NULL; wchar_t c[2] = { (wchar_t)w, 0 };
            if (pWindowsCreateString && SUCCEEDED(pWindowsCreateString(c, 1, &s))) {
                VCALL(g_interop, 14, HRESULT(STDMETHODCALLTYPE*)(void*,int,HSTRING))(g_interop, 0 /*Text*/, s);     /* OnCocos2dKeyEvent(Text,str) */
                if (s) pWindowsDeleteString(s);
            }
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static HWND make_window(void) {
    WNDCLASSW wc; RECT r; HWND hw;
    { int i; unsigned char *z=(unsigned char*)&wc; for (i=0;i<(int)sizeof wc;++i) z[i]=0; }
    wc.lpfnWndProc = gd_wndproc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"gd_host_window";
    wc.hCursor = LoadCursorW(NULL, (const wchar_t *)0x7f00 /*IDC_ARROW*/);
    RegisterClassW(&wc);
    r.left = 0; r.top = 0; r.right = g_win_w; r.bottom = g_win_h;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    hw = CreateWindowExW(0, L"gd_host_window", L"Geometry Dash (Win32Craft)", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         0x80000000 /*CW_USEDEFAULT*/, 0x80000000, r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (hw) RegisterTouchWindow(hw, 0);   /* get raw WM_TOUCH instead of delayed promoted-mouse (touch jump latency) */
    return hw;
}

/* --- the present pass: a landscape swap chain + a quad that samples the portrait RT rotated 90 deg CCW --- */
static IDXGISwapChain1        *g_sc;
static ID3D11RenderTargetView *g_bbrtv;
static ID3D11ShaderResourceView *g_rtsrv;
static struct { ID3D11VertexShader *vs; ID3D11PixelShader *ps; ID3D11InputLayout *il; ID3D11Buffer *vb, *cb; ID3D11SamplerState *ss; ID3D11RasterizerState *rs; } g_q;

static int present_init(HWND hwnd, ID3D11Texture2D *portrait_rt) {
    /* triangle-strip (TL,BL,TR,BR); UVs rotate the portrait RT upright for the landscape window.  (The device run showed
     * the earlier mapping was 180 deg off, so these are the flipped UVs: window(u,v) samples RT uv (1-v, u).) */
    static const float verts[16] = { -1.f, 1.f, 1.f,0.f,   -1.f,-1.f, 0.f,0.f,   1.f,1.f, 1.f,1.f,   1.f,-1.f, 0.f,1.f };
    static const float cbdata[8] = { 1.f,1.f,0.f,0.f,  1.f,1.f,1.f,1.f };   /* xform={scale 1, offset 0}, color=white */
    D3D11_INPUT_ELEMENT_DESC el[2]; D3D11_BUFFER_DESC bd; D3D11_SUBRESOURCE_DATA sd; D3D11_SAMPLER_DESC sm; D3D11_RASTERIZER_DESC rs;
    DXGI_SWAP_CHAIN_DESC1 scd; IDXGIDevice *dd=NULL; IDXGIAdapter *ad=NULL; IDXGIFactory2 *fa=NULL; ID3D11Texture2D *bb=NULL; HRESULT hr;
    int z; unsigned char *zp;
    for (z=0,zp=(unsigned char*)&el; z<(int)sizeof el; ++z) zp[z]=0;
    el[0].SemanticName="POSITION"; el[0].Format=DXGI_FORMAT_R32G32_FLOAT; el[0].AlignedByteOffset=0; el[0].InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;
    el[1].SemanticName="TEXCOORD"; el[1].Format=DXGI_FORMAT_R32G32_FLOAT; el[1].AlignedByteOffset=8; el[1].InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;
    if (FAILED(ID3D11Device_CreateVertexShader(g_dev, k_quad_vs, sizeof k_quad_vs, NULL, &g_q.vs))) { lg("present: quad VS failed"); return 0; }
    if (FAILED(ID3D11Device_CreatePixelShader(g_dev, k_quad_ps, sizeof k_quad_ps, NULL, &g_q.ps))) { lg("present: quad PS failed"); return 0; }
    if (FAILED(ID3D11Device_CreateInputLayout(g_dev, el, 2, k_quad_vs, sizeof k_quad_vs, &g_q.il))) { lg("present: quad layout failed"); return 0; }
    for (z=0,zp=(unsigned char*)&bd; z<(int)sizeof bd; ++z) zp[z]=0;
    bd.ByteWidth=sizeof verts; bd.Usage=D3D11_USAGE_DEFAULT; bd.BindFlags=D3D11_BIND_VERTEX_BUFFER; sd.pSysMem=verts; sd.SysMemPitch=0; sd.SysMemSlicePitch=0;
    if (FAILED(ID3D11Device_CreateBuffer(g_dev, &bd, &sd, &g_q.vb))) { lg("present: VB failed"); return 0; }
    bd.ByteWidth=sizeof cbdata; bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER; sd.pSysMem=cbdata;
    if (FAILED(ID3D11Device_CreateBuffer(g_dev, &bd, &sd, &g_q.cb))) { lg("present: CB failed"); return 0; }
    for (z=0,zp=(unsigned char*)&sm; z<(int)sizeof sm; ++z) zp[z]=0;
    sm.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR; sm.AddressU=sm.AddressV=sm.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP; sm.MaxLOD=3.402823466e38f;
    if (FAILED(ID3D11Device_CreateSamplerState(g_dev, &sm, &g_q.ss))) { lg("present: sampler failed"); return 0; }
    for (z=0,zp=(unsigned char*)&rs; z<(int)sizeof rs; ++z) zp[z]=0;
    rs.FillMode=D3D11_FILL_SOLID; rs.CullMode=D3D11_CULL_NONE; rs.DepthClipEnable=TRUE;
    if (FAILED(ID3D11Device_CreateRasterizerState(g_dev, &rs, &g_q.rs))) { lg("present: raster failed"); return 0; }
    if (FAILED(ID3D11Device_CreateShaderResourceView(g_dev, (ID3D11Resource *)portrait_rt, NULL, &g_rtsrv))) { lg("present: RT SRV failed"); return 0; }

    if (FAILED(ID3D11Device_QueryInterface(g_dev, &x_IID_IDXGIDevice, (void**)&dd)) || !dd) { lg("present: no IDXGIDevice"); return 0; }
    VCALL(dd, 12, HRESULT(STDMETHODCALLTYPE*)(void*,UINT))(dd, 1);   /* IDXGIDevice1::SetMaximumFrameLatency(1): cut the present-queue input lag (DXGI defaults to 3 frames) */
    IDXGIDevice_GetAdapter(dd, &ad);
    if (ad) IDXGIAdapter_GetParent(ad, &x_IID_IDXGIFactory2, (void**)&fa);
    if (!fa) { lg("present: no IDXGIFactory2"); return 0; }
    for (z=0,zp=(unsigned char*)&scd; z<(int)sizeof scd; ++z) zp[z]=0;
    scd.Width=g_win_w; scd.Height=g_win_h; scd.Format=DXGI_FORMAT_B8G8R8A8_UNORM; scd.SampleDesc.Count=1;
    /* blt (DISCARD) model: the back-buffer RTV stays valid across Present (the FLIP model rotates buffers and invalidates
     * it, which crashed the windowed run after a few frames). */
    scd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; scd.BufferCount=2; scd.Scaling=DXGI_SCALING_STRETCH; scd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    hr = IDXGIFactory2_CreateSwapChainForHwnd(fa, (IUnknown*)g_dev, hwnd, &scd, NULL, NULL, &g_sc);
    if (FAILED(hr)) { lg("present: 2-buffer DISCARD hr=0x%x; trying 1 buffer", (unsigned)hr);
        scd.BufferCount=1; hr = IDXGIFactory2_CreateSwapChainForHwnd(fa, (IUnknown*)g_dev, hwnd, &scd, NULL, NULL, &g_sc); }
    if (FAILED(hr) || !g_sc) { lg("present: CreateSwapChainForHwnd hr=0x%x", (unsigned)hr); return 0; }
    if (FAILED(IDXGISwapChain1_GetBuffer(g_sc, 0, &x_IID_ID3D11Texture2D, (void**)&bb)) || !bb) { lg("present: GetBuffer failed"); return 0; }
    hr = ID3D11Device_CreateRenderTargetView(g_dev, (ID3D11Resource *)bb, NULL, &g_bbrtv);
    ID3D11Texture2D_Release(bb);
    if (FAILED(hr)) { lg("present: back-buffer RTV hr=0x%x", (unsigned)hr); return 0; }
    lg("present ready: landscape %dx%d swap chain + rotate-present quad", g_win_w, g_win_h);
    return 1;
}

/* copy the portrait RT to a staging texture and write it as a BMP next to the log (debug: see the loading screen over time) */
static void dump_rt_bmp(ID3D11Texture2D *rt, const wchar_t *fname) {
    D3D11_TEXTURE2D_DESC td2; D3D11_MAPPED_SUBRESOURCE map; ID3D11Texture2D *stg = NULL;
    ID3D11Texture2D_GetDesc(rt, &td2);
    td2.Usage = D3D11_USAGE_STAGING; td2.BindFlags = 0; td2.CPUAccessFlags = D3D11_CPU_ACCESS_READ; td2.MiscFlags = 0;
    if (FAILED(ID3D11Device_CreateTexture2D(g_dev, &td2, NULL, &stg)) || !stg) return;
    ID3D11DeviceContext_CopyResource(g_ctx, (ID3D11Resource *)stg, (ID3D11Resource *)rt);
    if (SUCCEEDED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)stg, 0, D3D11_MAP_READ, 0, &map))) {
        wchar_t path[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH); const wchar_t *s; int i = 0;
        if (!n || n >= MAX_PATH) { path[0] = L'.'; i = 1; } else { for (s = path; *s; ++s) i = (int)(s - path) + 1; }
        path[i] = 0; { wchar_t *d = path + i; if (d > path && d[-1] != L'\\') *d++ = L'\\'; for (s = fname; *s; ++s) *d++ = *s; *d = 0; }
        write_bmp(path, (const unsigned char *)map.pData, td2.Width, td2.Height, (int)map.RowPitch);
        lg("  wrote %S (%ux%u)", fname, td2.Width, td2.Height);
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)stg, 0);
    }
    ID3D11Texture2D_Release(stg);
}

static HRESULT g_last_present_hr;                 /* diagnostics: is Present failing silently (device removed)? */
static unsigned g_rt_sum_prev;                    /* previous RT checksum, to detect a frozen (stale) displayed frame */
static int g_removed_frame = -1;                  /* first frame the D3D device went away (-1 = still alive) */

/* cheap stale-frame detector: sum the portrait RT's pixels.  If this value stops changing while the game claims to draw,
 * the displayed frame is stale (scene switched but GL/device is dead) rather than a live still screen. */
static unsigned rt_checksum(ID3D11Texture2D *rt) {
    D3D11_TEXTURE2D_DESC td; D3D11_MAPPED_SUBRESOURCE map; ID3D11Texture2D *stg = NULL; unsigned sum = 0;
    ID3D11Texture2D_GetDesc(rt, &td);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ; td.MiscFlags = 0;
    if (FAILED(ID3D11Device_CreateTexture2D(g_dev, &td, NULL, &stg)) || !stg) return 0;
    ID3D11DeviceContext_CopyResource(g_ctx, (ID3D11Resource *)stg, (ID3D11Resource *)rt);
    if (SUCCEEDED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)stg, 0, D3D11_MAP_READ, 0, &map))) {
        unsigned y, x; const unsigned char *p = (const unsigned char *)map.pData;
        for (y = 0; y < td.Height; y += 8) { const unsigned *row = (const unsigned *)(p + (size_t)y * map.RowPitch);
            for (x = 0; x < td.Width; x += 8) sum = sum * 1000003u + row[x]; }
        ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)stg, 0);
    }
    ID3D11Texture2D_Release(stg);
    return sum;
}

static void present_frame(void) {
    float clear[4] = { 0.f, 0.f, 0.f, 1.f }; UINT stride = 16, off = 0; D3D11_VIEWPORT vp;
    ID3D11ShaderResourceView *nullsrv = NULL;
    ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 1, &g_bbrtv, NULL);     /* unbinds the portrait RT as a render target */
    ID3D11DeviceContext_ClearRenderTargetView(g_ctx, g_bbrtv, clear);
    vp.TopLeftX=0; vp.TopLeftY=0; vp.Width=(float)g_win_w; vp.Height=(float)g_win_h; vp.MinDepth=0; vp.MaxDepth=1;
    ID3D11DeviceContext_RSSetViewports(g_ctx, 1, &vp);
    ID3D11DeviceContext_RSSetState(g_ctx, g_q.rs);
    ID3D11DeviceContext_OMSetBlendState(g_ctx, NULL, NULL, 0xffffffff);
    ID3D11DeviceContext_OMSetDepthStencilState(g_ctx, NULL, 0);
    ID3D11DeviceContext_IASetInputLayout(g_ctx, g_q.il);
    ID3D11DeviceContext_IASetVertexBuffers(g_ctx, 0, 1, &g_q.vb, &stride, &off);
    ID3D11DeviceContext_IASetPrimitiveTopology(g_ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_VSSetShader(g_ctx, g_q.vs, NULL, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(g_ctx, 0, 1, &g_q.cb);
    ID3D11DeviceContext_PSSetShader(g_ctx, g_q.ps, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(g_ctx, 0, 1, &g_rtsrv);
    ID3D11DeviceContext_PSSetSamplers(g_ctx, 0, 1, &g_q.ss);
    ID3D11DeviceContext_Draw(g_ctx, 4, 0);
    ID3D11DeviceContext_PSSetShaderResources(g_ctx, 0, 1, &nullsrv);      /* free the portrait SRV so the next Draw can target it again */
    { ID3D11RenderTargetView *nullrtv = NULL; ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 1, &nullrtv, NULL); }  /* unbind before Present */
    g_last_present_hr = IDXGISwapChain1_Present(g_sc, 1, 0);
}

/* ------------------------------------------------------------------ main ------------------------------------------------------------------ */
typedef struct { float Width, Height; } WF_Size;   /* Windows.Foundation.Size / DrawingSurfaceSizeF (HFA: passed in s0,s1) */

void mainCRTStartup(void) {
    HMODULE hCombase, hD3D11, hComp;
    D3D11CreateDevice_t pD3D11CreateDevice;
    DllGetActivationFactory_t pDllGetActivationFactory;
    ID3D11Device *device = NULL; ID3D11DeviceContext *ctx = NULL; D3D_FEATURE_LEVEL fl = (D3D_FEATURE_LEVEL)0;
    ID3D11Texture2D *rt = NULL; ID3D11RenderTargetView *rtv = NULL;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                   D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    D3D11_TEXTURE2D_DESC td; HRESULT hr; HSTRING hsClass; void *factory = NULL;
    void *statics = NULL, *interopUnk = NULL, *interop = NULL, *provider = NULL, *native = NULL, *handler = NULL;
    WF_Size wb, desired; LARGE_INTEGER pt; int frame;

    log_open();
    SetUnhandledExceptionFilter(crash_filter);
    AddVectoredExceptionHandler(1, gd_veh);
    lg("==== gd_host M1 (headless) build %s %s ====", __DATE__, __TIME__);

    hCombase = LoadLibraryW(L"combase.dll");
    hD3D11   = LoadLibraryW(L"d3d11.dll");
    pRoInitialize             = (RoInitialize_t)             GetProcAddress(hCombase, "RoInitialize");
    pWindowsCreateString      = (WindowsCreateString_t)      GetProcAddress(hCombase, "WindowsCreateString");
    pWindowsDeleteString      = (WindowsDeleteString_t)      GetProcAddress(hCombase, "WindowsDeleteString");
    pWindowsGetStringRawBuffer= (WindowsGetStringRawBuffer_t)GetProcAddress(hCombase, "WindowsGetStringRawBuffer");
    g_real_RoGAF              = (RoGAF_t)                    GetProcAddress(hCombase, "RoGetActivationFactory");
    pD3D11CreateDevice   = (D3D11CreateDevice_t)  GetProcAddress(hD3D11, "D3D11CreateDevice");
    if (!pRoInitialize || !pWindowsCreateString || !pD3D11CreateDevice) { lg("FATAL: missing combase/d3d11 entry points"); ExitProcess(2); }

    hr = pRoInitialize(1 /* RO_INIT_MULTITHREADED */);
    lg("RoInitialize(MTA) hr=0x%x", (unsigned)hr);

    /* D3D11 device at the device's real max level (do not force 9_1 on a capable GPU), with BGRA for ANGLE. */
    hr = pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                            levels, (UINT)(sizeof levels / sizeof levels[0]), D3D11_SDK_VERSION, &device, &fl, &ctx);
    if (FAILED(hr)) { lg("D3D11CreateDevice(HARDWARE) hr=0x%x, trying WARP", (unsigned)hr);
        hr = pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                levels, (UINT)(sizeof levels / sizeof levels[0]), D3D11_SDK_VERSION, &device, &fl, &ctx); }
    if (FAILED(hr)) { lg("FATAL: D3D11CreateDevice hr=0x%x", (unsigned)hr); ExitProcess(3); }
    lg("D3D11 device created, feature level 0x%x", (unsigned)fl);
    g_dev = device; g_ctx = ctx; ptr_ring_init();

    /* portrait render target the game draws into (gd-component F9), BGRA8 RENDER_TARGET|SHADER_RESOURCE */
    { int i; unsigned char *z = (unsigned char *)&td; for (i = 0; i < (int)sizeof td; ++i) z[i] = 0; }
    td.Width = RTW; td.Height = RTH; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    hr = ID3D11Device_CreateTexture2D(device, &td, NULL, &rt);
    if (FAILED(hr)) { lg("FATAL: CreateTexture2D(rt) hr=0x%x", (unsigned)hr); ExitProcess(4); }
    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)rt, NULL, &rtv);
    if (FAILED(hr)) { lg("FATAL: CreateRenderTargetView hr=0x%x", (unsigned)hr); ExitProcess(4); }
    lg("render target %dx%d (portrait) + RTV created", RTW, RTH);

    /* load the native component and its activation factory entry point */
    hComp = LoadLibraryW(L"GeometryJumpComponent.dll");
    if (!hComp) { lg("FATAL: LoadLibrary(GeometryJumpComponent.dll) err=%u", (unsigned)GetLastError()); ExitProcess(5); }
    pDllGetActivationFactory = (DllGetActivationFactory_t)GetProcAddress(hComp, "DllGetActivationFactory");
    if (!pDllGetActivationFactory) { lg("FATAL: no DllGetActivationFactory export"); ExitProcess(5); }
    lg("GeometryJumpComponent.dll loaded; DllGetActivationFactory at %p", (void *)pDllGetActivationFactory);

    /* asset + save paths for the Package / ApplicationData fakes (getAppPath appends "\Assets\Resources\", gd-platform F4) */
    {
        DWORD n = GetModuleFileNameW(NULL, g_install_path, MAX_PATH);
        wchar_t *d; const wchar_t *s;
        while (n && g_install_path[n - 1] != L'\\') --n;
        if (n) g_install_path[n - 1] = 0;                      /* exe dir, no trailing backslash (holds Assets\) */
        n = GetEnvironmentVariableW(L"LOCALAPPDATA", g_local_path, MAX_PATH);
        if (n && n < MAX_PATH - 16) {
            d = g_local_path + n; if (n && g_local_path[n - 1] != L'\\') *d++ = L'\\';
            for (s = L"GeometryDash"; *s; ++s) *d++ = *s; *d = 0;
            CreateDirectoryW(g_local_path, NULL);
        } else { lstrcpyW(g_local_path, g_install_path); }
        lg("asset path (InstalledLocation) = %S", g_install_path);
        lg("save  path (LocalFolder)       = %S", g_local_path);
    }

    /* intercept Package/ApplicationData and log every other WinRT class (patch the game DLLs' RoGetActivationFactory IAT) */
    {
        static const wchar_t *const mods[] = { L"GeometryJumpComponent.dll", L"vccorlib110.dll", L"libcocos2d.dll", L"libEGL_phone.dll", L"libGLESv2_phone.dll", L"fmodex_80_arm.dll" };
        int k, p = 0;
        for (k = 0; k < (int)(sizeof mods / sizeof mods[0]); ++k) p += iat_patch(GetModuleHandleW(mods[k]), "RoGetActivationFactory", (void *)hook_RoGAF);
        lg("patched RoGetActivationFactory in %d IAT slot(s) across the game DLLs", p);
        /* capture the game's OutputDebugString (cocos2d CCLog) to see why it is stuck in the loading scene */
        g_real_ODSW = (ODSW_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "OutputDebugStringW");
        g_real_ODSA = (ODSA_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "OutputDebugStringA");
        p = 0;
        for (k = 0; k < (int)(sizeof mods / sizeof mods[0]); ++k) {
            HMODULE m = GetModuleHandleW(mods[k]);
            p += iat_patch(m, "OutputDebugStringW", (void *)hook_ODSW);
            p += iat_patch(m, "OutputDebugStringA", (void *)hook_ODSA);
        }
        lg("patched OutputDebugString in %d IAT slot(s) (cocos2d log capture)", p);
        /* trace file opens to find where asset loading stalls (~80%) */
        g_real_fopen = (fopen_t)GetProcAddress(GetModuleHandleW(L"msvcr110.dll"), "fopen");
        p = 0;
        for (k = 0; k < (int)(sizeof mods / sizeof mods[0]); ++k) p += iat_patch(GetModuleHandleW(mods[k]), "fopen", (void *)hook_fopen);
        lg("patched fopen in %d IAT slot(s) (real fopen=%p)", p, (void *)g_real_fopen);
        /* THE FIX: give PPL task continuations a working UI-thread context so async asset-load completions run on the
         * render thread instead of being lost on the thread pool (patch CaptureUiThreadContext in its two importers,
         * GeometryJumpComponent.dll + libcocos2d.dll; see the UI-thread-context block above). */
        disp_init();
        g_render_tid = GetCurrentThreadId();
        p  = iat_patch(hComp, "CaptureUiThreadContext", (void *)hook_CUTC);
        p += iat_patch(GetModuleHandleW(L"libcocos2d.dll"), "CaptureUiThreadContext", (void *)hook_CUTC);
        lg("patched CaptureUiThreadContext in %d IAT slot(s) -> host UI-thread context (render tid=%u)", p, (unsigned)g_render_tid);
        /* THE REAL FIX (confirmed by E:\gd_host.log + debug.txt + static RE): GeometryJumpComponent sub_36390 passes a
         * feature-level index (r6, default `movs r6,#3` = FL9_3 at RVA 0x363fc) to libGLESv2_phone!CreateWinrtEglWindow
         * (ordinal 181, at 0x36496).  Our device is real FL 9_1, so ANGLE was driving a 9_1 device as if it were 9_3 and
         * the first 9_3-only texture-upload path removed it with DRIVER_INTERNAL_ERROR (ANGLE debug.txt:
         * Image11::createStagingTexture failed).  Clamp that requested level to the device's ACTUAL max (per-device policy,
         * NOT a blanket forced 9_1): 9_1 device -> r6=1, 9_2 -> r6=2, 9_3+ -> leave the game's default. */
        { unsigned char idx = (fl == D3D_FEATURE_LEVEL_9_1) ? 1 : (fl == D3D_FEATURE_LEVEL_9_2) ? 2 : 3;
          if (idx < 3) {
              int a = code_patch(hComp, 0x363fc, 0x03, idx);                   /* default FL9_3 request -> device max */
              int b = (idx < 2) ? code_patch(hComp, 0x3641a, 0x02, idx) : 0;   /* the explicit-9_2 branch -> device max */
              lg("ANGLE FL clamp: device fl=0x%x -> idx %d, patched %d site(s)", (unsigned)fl, idx, a + b);
          } else lg("ANGLE FL clamp: device fl=0x%x >= 9_3, no clamp needed", (unsigned)fl);
        }
    }

    /* step 2: BroswerEventHelper.SetCallback(ICallback) */
    pWindowsCreateString(L"PhoneDirect3DXamlAppComponent.BroswerEventHelper", wlen(L"PhoneDirect3DXamlAppComponent.BroswerEventHelper"), &hsClass);
    hr = pDllGetActivationFactory(hsClass, &factory); pWindowsDeleteString(hsClass);
    lg("DllGetActivationFactory(BroswerEventHelper) hr=0x%x factory=%p", (unsigned)hr, factory);
    if (SUCCEEDED(hr) && factory) {
        hr = VCALL(factory, 0, HRESULT(STDMETHODCALLTYPE*)(void*,const GUID*,void**))(factory, &IID_BroswerStatics, &statics);
        lg("  QI __IBroswerEventHelperStatics hr=0x%x statics=%p", (unsigned)hr, statics);
        if (SUCCEEDED(hr) && statics) {
            hr = VCALL(statics, 6, HRESULT(STDMETHODCALLTYPE*)(void*,void*))(statics, &g_callback);  /* SetCallback */
            lg("  SetCallback(host ICallback) hr=0x%x", (unsigned)hr);
        }
    }

    /* step 3: Direct3DInterop factory -> CreateInstance(0) -> QI default interface */
    pWindowsCreateString(L"PhoneDirect3DXamlAppComponent.Direct3DInterop", wlen(L"PhoneDirect3DXamlAppComponent.Direct3DInterop"), &hsClass);
    hr = pDllGetActivationFactory(hsClass, &factory); pWindowsDeleteString(hsClass);
    lg("DllGetActivationFactory(Direct3DInterop) hr=0x%x factory=%p", (unsigned)hr, factory);
    if (FAILED(hr) || !factory) { lg("FATAL: no Direct3DInterop factory"); ExitProcess(6); }
    { void *ifac = NULL;
      hr = VCALL(factory, 0, HRESULT(STDMETHODCALLTYPE*)(void*,const GUID*,void**))(factory, &IID_InteropFactory, &ifac);
      lg("  QI __IDirect3DInteropFactory hr=0x%x ifac=%p", (unsigned)hr, ifac);
      if (FAILED(hr) || !ifac) { lg("FATAL"); ExitProcess(6); }
      hr = VCALL(ifac, 6, HRESULT(STDMETHODCALLTYPE*)(void*,int,void**))(ifac, 0 /*Landscape*/, &interopUnk);  /* CreateInstance */
      lg("  CreateInstance(0) hr=0x%x interop=%p", (unsigned)hr, interopUnk);
      if (FAILED(hr) || !interopUnk) { lg("FATAL"); ExitProcess(6); }
    }
    hr = VCALL(interopUnk, 0, HRESULT(STDMETHODCALLTYPE*)(void*,const GUID*,void**))(interopUnk, &IID_InteropPublic, &interop);
    lg("  QI __IDirect3DInteropPublicNonVirtuals hr=0x%x interop=%p", (unsigned)hr, interop);
    if (FAILED(hr) || !interop) { lg("FATAL"); ExitProcess(6); }
    g_interop = interop;   /* for keyboard forwarding from the window proc */

    /* step 4: set_WindowBounds(landscape DIP size) + the 3 delegates */
    wb.Width = (float)RTH; wb.Height = (float)RTW;   /* landscape 800x480 */
    hr = VCALL(interop, 19, HRESULT(STDMETHODCALLTYPE*)(void*,WF_Size))(interop, wb);
    lg("  set_WindowBounds(%d,%d) hr=0x%x", (int)wb.Width, (int)wb.Height, (unsigned)hr);
    VCALL(interop, 20, HRESULT(STDMETHODCALLTYPE*)(void*,void*))(interop, &g_evt_delegate);
    VCALL(interop, 21, HRESULT(STDMETHODCALLTYPE*)(void*,void*))(interop, &g_mbx_delegate);
    VCALL(interop, 22, HRESULT(STDMETHODCALLTYPE*)(void*,void*))(interop, &g_edt_delegate);
    lg("  SetCocos2d{Event,MessageBox,EditBox}Delegate done");

    /* step 5: CreateContentProvider -> QI native; QI handler -> SetManipulationHost */
    hr = VCALL(interop, 6, HRESULT(STDMETHODCALLTYPE*)(void*,void**))(interop, &provider);
    lg("  CreateContentProvider hr=0x%x provider=%p", (unsigned)hr, provider);
    if (FAILED(hr) || !provider) { lg("FATAL"); ExitProcess(7); }
    hr = VCALL(provider, 0, HRESULT(STDMETHODCALLTYPE*)(void*,const GUID*,void**))(provider, &IID_BgContentNative, &native);
    lg("  QI IDrawingSurfaceBackgroundContentProviderNative hr=0x%x native=%p", (unsigned)hr, native);
    if (FAILED(hr) || !native) { lg("FATAL"); ExitProcess(7); }
    hr = VCALL(interopUnk, 0, HRESULT(STDMETHODCALLTYPE*)(void*,const GUID*,void**))(interopUnk, &IID_ManipHandler, &handler);
    lg("  QI IDrawingSurfaceManipulationHandler hr=0x%x handler=%p", (unsigned)hr, handler);
    if (SUCCEEDED(hr) && handler)
        VCALL(handler, 6, HRESULT(STDMETHODCALLTYPE*)(void*,void*))(handler, &g_manip_host);   /* SetManipulationHost */

    /* step 6: Connect(runtime-host, device) */
    hr = VCALL(native, 3, HRESULT(STDMETHODCALLTYPE*)(void*,void*,void*))(native, &g_runtime_host, device);
    lg("  Connect(runtime-host, device) hr=0x%x", (unsigned)hr);

    /* step 7: create the window + landscape swap chain, then run the message + Draw loop (one thread owns the D3D11 context) */
    {
        HWND hwnd = make_window();
        MSG msg;
        if (!hwnd) { lg("FATAL: window creation failed err=%u", (unsigned)GetLastError()); ExitProcess(8); }
        lg("window created %p (%dx%d landscape)", (void *)hwnd, g_win_w, g_win_h);
        if (!present_init(hwnd, rt)) { lg("FATAL: present init failed"); ExitProcess(8); }
        { float c0[4] = { 0.f, 0.f, 0.f, 1.f };   /* show black while the first Draw boots ANGLE + loads assets (seconds) */
          ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 1, &g_bbrtv, NULL);
          ID3D11DeviceContext_ClearRenderTargetView(g_ctx, g_bbrtv, c0); IDXGISwapChain1_Present(g_sc, 1, 0); }

        for (frame = 0; !g_quit && !g_want_quit; ++frame) {
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); if (msg.message == WM_QUIT) g_quit = 1; }
            if (g_quit || g_want_quit) break;
            disp_drain();   /* run any PPL task continuations marshaled to this render thread (async asset-load completions) */
            pt.QuadPart = 0; desired.Width = desired.Height = 0;
            VCALL(native, 5, HRESULT(STDMETHODCALLTYPE*)(void*,const LARGE_INTEGER*,WF_Size*))(native, &pt, &desired);   /* PrepareResources */
            g_additional_frame = 0;
            hr = VCALL(native, 6, HRESULT(STDMETHODCALLTYPE*)(void*,void*,void*,void*))(native, device, ctx, rtv);        /* Draw -> the portrait RT */
            disp_drain();   /* again after Draw, so a completion that fired during Draw registers before the next frame */
            if (frame < 5 || (frame % 60) == 0) lg("  Draw #%d hr=0x%x", frame, (unsigned)hr);
            if (FAILED(hr)) { lg("  Draw #%d FAILED hr=0x%x removedReason=0x%x", frame, (unsigned)hr, (unsigned)ID3D11Device_GetDeviceRemovedReason(device)); }
            /* pinpoint: the FIRST frame the device goes away, with the last asset opened -> which texture upload killed it */
            if (g_removed_frame < 0) { HRESULT dr = ID3D11Device_GetDeviceRemovedReason(device);
                if (dr != 0) { g_removed_frame = frame;
                    lg("  !!! DEVICE REMOVED first seen at frame %d reason=0x%x (last asset: %s)", frame, (unsigned)dr, g_last_asset); } }
            /* the decisive line: did the PPL/UI-context path fire at all?  If cutc_cap / ctx_calls stay 0 while loading
             * stalls, the async-completion theory is dead and the stall is elsewhere (std::thread worker / HTTP queue). */
            if (frame == 300 || frame == 900)
                lg("  [counters @%d] fopen=%ld cutc_cap=%ld cutc_sta=%ld ctx_calls=%ld", frame,
                   g_fopen_count, g_cutc_cap, g_cutc_sta, g_ctx_calls);
            if (frame == 0)   dump_rt_bmp(rt, L"gd_frame.bmp");        /* first frame */
            if (frame == 900) dump_rt_bmp(rt, L"gd_frame_900.bmp");   /* ~15 s in: has the loading screen progressed? */
            /* per-second health probe: static RE says the loader is a synchronous 14-step chain that almost certainly
             * FINISHES (menu music = step 13), so the freeze is most likely a dead GL/device at the first menu frame while
             * the host keeps presenting the stale portrait RT.  These lines settle it: a changing RT checksum = live
             * rendering; a frozen checksum + removedReason/Present!=0 = device lost (likely the ANGLE FL9_3-vs-9_1 gap). */
            if (frame && (frame % 60) == 0) {
                unsigned sum = rt_checksum(rt); HRESULT dr = ID3D11Device_GetDeviceRemovedReason(device);
                lg("  [health @%d] rtsum=0x%x %s present=0x%x removed=0x%x", frame, sum,
                   sum == g_rt_sum_prev ? "(FROZEN)" : "(changing)", (unsigned)g_last_present_hr, (unsigned)dr);
                g_rt_sum_prev = sum;
            }
            present_frame();    /* sample the portrait RT into the landscape back buffer, 90-deg CCW, and Present */
        }
        lg("==== gd_host exiting after %d frames ====", frame);
    }
    if (g_log != INVALID_HANDLE_VALUE) CloseHandle(g_log);
    ExitProcess(0);
}
