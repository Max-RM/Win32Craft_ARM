/* l9shim.c - Direct3D 11 compatibility shim for Bad Piggies Rebooted (Unity 2018.4, IL2CPP) on feature level 9_1 GPUs (Surface RT / Surface 2).
 *
 * Unity 2018.4 dropped the feature-level 9.x render path: UnityPlayer.dll asks d3d11.dll for a device with the level list [11_1 .. 10_0] and refuses
 * to start below 10.0 ("At least DirectX 11 feature level 10.0 is required").  On a Tegra (9_1 max, proven by tools/m0probe on a Surface 2, 2026-10-22)
 * that fails and Unity falls back to WARP (CPU).  This shim is loaded in UnityPlayer's place of d3d11.dll (its import descriptor name is rewritten to
 * this DLL by tools/badpiggies/make_badpiggies_package.py); it forwards every d3d11 export to the genuine system d3d11.dll EXCEPT D3D11CreateDevice,
 * which it makes succeed on a 9_1 GPU and then wraps so Unity's own SM4/SM5 shaders, states and formats become 9_1-legal:
 *   - device policy: try Unity's call unchanged; only when it yields a sub-FL10 device (or none) do we create a 9_1 device and install the wrappers.
 *     On a real FL10+ GPU (a newer device) the shim does nothing - Unity runs untouched (the project rule "never force a feature level").
 *   - the one device object gets a private, full-length copy of its vtable (measured at run time; critic F2: a short copy crashes inside DXGI).
 *   - CreateVertexShader / CreatePixelShader: the SM4 bytecode is swapped for a prebuilt vs/ps_4_0_level_9_1 replacement looked up by the DXBC
 *     checksum (bytes 4..19), from data\bp\l9_replacements.bin (built by the recon; proven to create on a Surface 2 Tegra: 113/113).  NOT CreateInputLayout
 *     (critic F1: Unity passes a signature-only blob there, never a full shader - a checksum swap would miss and kill every layout).
 *   - CreateTexture2D / the three view creators / sampler / rasterizer / blend(+1) / depth-stencil: the descriptors are sanitised to the 10Level9 rules.
 *   - CheckFeatureSupport / CheckFormatSupport / GetFeatureLevel: answered so Unity's caps init (GfxDeviceD3D11, UnityPlayer sub_1007434) reads 9_1-safe
 *     limits.  The matching in-memory correction of Unity's caps object is tools/badpiggies/l9caps.inc (anchored on verified RVAs; a no-op if they differ).
 *
 * Builds for x64 (PC test, tools/badpiggies/build_l9shim_x64.bat) and ARM32 Thumb-2 (device, build_l9shim_arm32.bat) from this one file.  No CRT: only
 * kernel32 + a dynamically loaded d3d11.dll, like tools/m0probe.  Design: games_recon/bad_piggies_hw-design.md (route "S+"), corrections in -hw-critic.md. */

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11_1.h>
#include <stdarg.h>
/* MSVC's <stdarg.h> (pulled in by windows.h) defines va_start as "address of the last named parameter + 4", which assumes MSVC's argument homing.  clang
 * on ARM does not home r0-r3 that way, so under clang-ARM every variadic argument is garbage - exactly the m0probe Surface 2 crash (2026-10-22), and it hit
 * this shim's logger too (the device log printed "build (null)", "1 replacements", garbage feature level, and a misread %s dereference crashed the game).
 * Use the compiler's own implementation, which is correct on every target. */
#if defined(__clang__)
#undef va_start
#undef va_arg
#undef va_end
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
typedef __builtin_va_list L9_VA_LIST;
#else
typedef va_list L9_VA_LIST;
#endif

#if defined(_M_ARM) || defined(__arm__)
#define L9_ARM 1
#endif

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

/* ---- freestanding helpers (the compiler may emit memset/memcpy) ---- */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma function(memset, memcpy)
#endif
void *memset(void *d, int c, size_t n) { u8 *p = (u8 *)d; while (n--) *p++ = (u8)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { u8 *p = (u8 *)d; const u8 *q = (const u8 *)s; while (n--) *p++ = *q++; return d; }
int memcmp(const void *a, const void *b, size_t n) { const u8 *p = (const u8 *)a, *q = (const u8 *)b; while (n--) { if (*p != *q) return *p < *q ? -1 : 1; ++p; ++q; } return 0; }
#if defined(_M_X64) || defined(_M_IX86)
int _fltused = 0;
#endif
#if defined(L9_ARM)
/* armrt.lib division helpers reference these CRT security symbols on their divide-by-zero path; never reached (no division here), defined so the link succeeds */
unsigned int __security_cookie = 0xB3B3B3B3u;
void __cdecl __security_check_cookie(unsigned int c) { (void)c; }
int __cdecl __GSHandlerCheck(void *a, void *b, void *c, void *d) { (void)a; (void)b; (void)c; (void)d; return 1; }
#endif


/* ============================================================================================================
 * logging: one file next to the game (win32craft is the host's; this shim writes l9shim.log so it is visible even without the host)
 * ============================================================================================================ */
static HANDLE g_log = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_log_cs;
static int g_log_ready;
static wchar_t g_log_dir[MAX_PATH];     /* the directory the log actually opened in (so diagnostic blob dumps land beside it) */
static int g_have_log_dir;

static void log_raw(const char *s, int n)
{
    DWORD w;
    if (g_log == INVALID_HANDLE_VALUE || n <= 0) return;
    WriteFile(g_log, s, (DWORD)n, &w, NULL);
}

/* one writable-location attempt for the log: dir (NUL-terminated wide, with trailing slash or empty) + "l9shim.log" */
static HANDLE log_try(const wchar_t *dir)
{
    wchar_t path[MAX_PATH]; wchar_t *d = path; const wchar_t *s;
    if (!dir) return INVALID_HANDLE_VALUE;
    for (s = dir; *s && (d - path) < MAX_PATH - 12; ++s) *d++ = *s;
    if (d > path && d[-1] != L'\\' && d[-1] != L'/') *d++ = L'\\';
    for (s = L"l9shim.log"; *s; ++s) *d++ = *s; *d = 0;
    return CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

/* tiny printf: %s %c %d %u %x %X %p %ls %% , width (0/space pad), 'l'/'ll'/'I64' length.  Returns length written to buf. */
static int fmt(char *buf, int cap, const char *f, L9_VA_LIST ap)
{
    int n = 0, i;
#define PUT(ch) do { if (n + 1 < cap) buf[n] = (char)(ch); ++n; } while (0)
    for (; *f; ++f) {
        int width = 0, zero = 0, lng = 0;
        char t[24]; int nt;
        if (*f != '%') { PUT(*f); continue; }
        ++f;
        if (*f == '0') { zero = 1; ++f; }
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); ++f; }
        if (*f == 'l') { ++f; lng = 1; if (*f == 'l') { lng = 2; ++f; } }
        else if (f[0] == 'I' && f[1] == '6' && f[2] == '4') { lng = 2; f += 3; }
        if (*f == 's') {
            const char *s = va_arg(ap, const char *); if (!s) s = "(null)";
            for (; *s; ++s) PUT(*s);
        } else if (*f == 'S') {
            const wchar_t *w = va_arg(ap, const wchar_t *); if (!w) { const char *z = "(null)"; for (; *z; ++z) PUT(*z); }
            else for (; *w; ++w) PUT(*w < 128 ? (char)*w : '?');
        } else if (*f == 'c') {
            PUT((char)va_arg(ap, int));
        } else if (*f == 'd' || *f == 'u' || *f == 'x' || *f == 'X' || *f == 'p') {
            u64 v; int neg = 0; unsigned base = (*f == 'x' || *f == 'X' || *f == 'p') ? 16 : 10;
            const char *dig = (*f == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
            if (*f == 'p') { v = (u64)(size_t)va_arg(ap, void *); zero = 1; width = (int)sizeof(void *) * 2; }
            else if (*f == 'd') { long long s = (lng == 2) ? va_arg(ap, long long) : (long long)va_arg(ap, int); if (s < 0) { neg = 1; v = (u64)(-s); } else v = (u64)s; }
            else v = (lng == 2) ? va_arg(ap, u64) : (u64)va_arg(ap, unsigned);
            nt = 0;
            if (!v) t[nt++] = '0';
            else while (v) { t[nt++] = dig[v % base]; v /= base; }
            if (neg) t[nt++] = '-';
            for (i = nt; i < width; ++i) PUT(zero ? '0' : ' ');
            while (nt) PUT(t[--nt]);
        } else { PUT('%'); if (*f) PUT(*f); }
    }
#undef PUT
    if (n >= cap) n = cap - 1;
    return n;
}

static void lgf(const char *f, ...)
{
    char line[1024];
    int n;
    L9_VA_LIST ap;
    va_start(ap, f); n = fmt(line, (int)sizeof line - 2, f, ap); va_end(ap);
    line[n] = 0;
    OutputDebugStringA(line);                 /* always: visible in DebugView / the kernel debugger even if no file could be opened (AppContainer) */
    line[n++] = '\r'; line[n++] = '\n';
    if (g_log == INVALID_HANDLE_VALUE) return;
    if (g_log_ready) EnterCriticalSection(&g_log_cs);
    log_raw(line, n);
    if (g_log_ready) LeaveCriticalSection(&g_log_cs);
}

/* Open the log in the first writable place.  Inside a UWP AppContainer the game folder (WindowsApps) is read-only and the real %TEMP% is not writable, but
 * %LOCALAPPDATA% writes are redirected to the package's own store (findable at ...\AppData\Local\Packages\<family>\LocalCache\...\l9shim.log).  Every line
 * also goes to OutputDebugString regardless. */
static void remember_dir(const wchar_t *dir)
{
    wchar_t *d = g_log_dir; const wchar_t *s;
    for (s = dir; *s && (d - g_log_dir) < MAX_PATH - 1; ++s) *d++ = *s;
    *d = 0; g_have_log_dir = 1;
}
static void log_open(void)
{
    wchar_t dir[MAX_PATH], exedir[MAX_PATH], *p;
    DWORD n;
    InitializeCriticalSection(&g_log_cs); g_log_ready = 1;
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH)) { g_log = log_try(dir); if (g_log != INVALID_HANDLE_VALUE) { remember_dir(dir); lgf("l9shim: log at %%LOCALAPPDATA%%\\l9shim.log (redirected into this app's package store)"); return; } }
    if (GetTempPathW(MAX_PATH, dir)) { g_log = log_try(dir); if (g_log != INVALID_HANDLE_VALUE) { remember_dir(dir); lgf("l9shim: log at %%TEMP%%\\l9shim.log"); return; } }
    n = GetModuleFileNameW(NULL, exedir, MAX_PATH);             /* next to the game exe (works for the non-UWP test / host route) */
    if (n && n < MAX_PATH) { for (p = exedir + n; p > exedir && *p != L'\\' && *p != L'/'; --p) {} *p = 0; g_log = log_try(exedir); if (g_log != INVALID_HANDLE_VALUE) remember_dir(exedir); }
    /* if all failed, g_log stays invalid and only OutputDebugString carries the log */
}

/* write a raw blob beside the log as <prefix><idx>.dxbc.  Used to capture the exact DXBC of a shader that has no level-9 replacement,
 * so a precise replacement can be built offline from the authentic bytes (not a size-guessed asset match). */
static void dump_blob(const char *prefix, long idx, const void *data, u32 len)
{
    wchar_t path[MAX_PATH]; wchar_t *d = path; const wchar_t *s; const char *a;
    char num[16]; int ni = 0, k; long v = idx;
    HANDLE h; DWORD w;
    if (!g_have_log_dir) return;
    for (s = g_log_dir; *s && (d - path) < MAX_PATH - 48; ++s) *d++ = *s;
    if (d > path && d[-1] != L'\\' && d[-1] != L'/') *d++ = L'\\';
    for (a = prefix; *a; ++a) *d++ = (wchar_t)(unsigned char)*a;
    if (v <= 0) num[ni++] = '0'; else { char tmp[16]; int ti = 0; while (v) { tmp[ti++] = (char)('0' + v % 10); v /= 10; } while (ti) num[ni++] = tmp[--ti]; }
    for (k = 0; k < ni; ++k) *d++ = (wchar_t)num[k];
    for (a = ".dxbc"; *a; ++a) *d++ = (wchar_t)*a;
    *d = 0;
    h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, data, len, &w, NULL);
    CloseHandle(h);
}

/* ============================================================================================================
 * the shader replacement table (data\bp\l9_replacements.bin, format L9SW; same loader as tools/m0probe bp_*)
 *   header: "L9SW"(4) version(4) count(4);  then count entries of 24 bytes: checksum(16) offset(4) size(4);  then the blobs.
 * ============================================================================================================ */
static u8 *g_tab;
static u32 g_tab_size, g_tab_count;

static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

static void table_load(void)
{
    wchar_t path[MAX_PATH], *p;
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    HANDLE h; DWORD sz, got = 0;
    const wchar_t *rel = L"Data\\bp\\l9_replacements.bin";     /* beside the game; the packager places the table here */
    wchar_t *d;
    if (!n || n >= MAX_PATH) return;
    for (p = path + n; p > path && *p != L'\\' && *p != L'/'; --p) {}
    if (p > path) ++p;
    d = p; { const wchar_t *s = rel; while (*s && (d - path) < MAX_PATH - 1) *d++ = *s++; *d = 0; }
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { lgf("l9shim: shader table not found: %S", path); return; }
    sz = GetFileSize(h, NULL);
    if (sz == INVALID_FILE_SIZE || sz < 12) { CloseHandle(h); return; }
    g_tab = (u8 *)VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_tab) { CloseHandle(h); return; }
    if (!ReadFile(h, g_tab, sz, &got, NULL) || got != sz) { CloseHandle(h); VirtualFree(g_tab, 0, MEM_RELEASE); g_tab = NULL; return; }
    CloseHandle(h);
    g_tab_size = sz;
    if (memcmp(g_tab, "L9SW", 4) != 0) { lgf("l9shim: shader table bad magic"); g_tab = NULL; return; }
    g_tab_count = rd32(g_tab + 8);
    if (12u + 24u * g_tab_count > g_tab_size) { lgf("l9shim: shader table truncated"); g_tab = NULL; return; }
    lgf("l9shim: shader table loaded: %u replacements (%u bytes)", g_tab_count, g_tab_size);
}

/* look up a replacement by the DXBC checksum at bytes 4..19 of the bytecode Unity passes */
static const void *table_lookup(const void *bytecode, SIZE_T length, u32 *out_size)
{
    const u8 *b = (const u8 *)bytecode;
    u32 i;
    if (!g_tab || length < 20 || memcmp(b, "DXBC", 4) != 0) return NULL;
    for (i = 0; i < g_tab_count; ++i) {
        const u8 *e = g_tab + 12 + 24 * i;
        if (memcmp(e, b + 4, 16) == 0) {
            u32 off = rd32(e + 16), size = rd32(e + 20);
            if (off <= g_tab_size && size <= g_tab_size - off) { *out_size = size; return g_tab + off; }
            return NULL;
        }
    }
    return NULL;
}

static HMODULE real_d3d11(void);   /* defined below; the wrapper's D3D11CreateDevice policy needs it */
#include "l9caps.inc"      /* the in-memory correction of Unity's capability record for 9_1 (used by the wrapper's caps-query hooks) */
#include "l9wrap.inc"      /* the device wrapper: vtable copy + the hooked methods + the D3D11CreateDevice policy */

/* ============================================================================================================
 * exports: everything forwards to the genuine system d3d11.dll except D3D11CreateDevice (l9wrap.inc)
 * ============================================================================================================ */
static HMODULE g_real_d3d11;
static void *g_fwd_surface, *g_fwd_devfromdxgi;

static HMODULE real_d3d11(void)
{
    if (!g_real_d3d11) {
        wchar_t sysdir[MAX_PATH]; UINT n = GetSystemDirectoryW(sysdir, MAX_PATH);
        wchar_t full[MAX_PATH]; const wchar_t *s = L"\\d3d11.dll"; wchar_t *d;
        if (n && n < MAX_PATH - 10) { d = sysdir + n; while (*s) *d++ = *s++; *d = 0; g_real_d3d11 = LoadLibraryW(sysdir); }
        if (!g_real_d3d11) g_real_d3d11 = LoadLibraryW(L"d3d11.dll");   /* fallback; our DLL is not named d3d11.dll so no recursion */
        (void)full;
    }
    return g_real_d3d11;
}

/* CreateDirect3D11SurfaceFromDXGISurface / CreateDirect3D11DeviceFromDXGIDevice: pure forwards (Unity imports them; the WinRT interop is unchanged) */
HRESULT WINAPI CreateDirect3D11SurfaceFromDXGISurface(void *dxgiSurface, void **graphicsSurface)
{
    if (!g_fwd_surface) { HMODULE m = real_d3d11(); g_fwd_surface = m ? (void *)GetProcAddress(m, "CreateDirect3D11SurfaceFromDXGISurface") : NULL; }
    if (!g_fwd_surface) return E_NOTIMPL;
    return ((HRESULT (WINAPI *)(void *, void **))g_fwd_surface)(dxgiSurface, graphicsSurface);
}
HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(void *dxgiDevice, void **graphicsDevice)
{
    if (!g_fwd_devfromdxgi) { HMODULE m = real_d3d11(); g_fwd_devfromdxgi = m ? (void *)GetProcAddress(m, "CreateDirect3D11DeviceFromDXGIDevice") : NULL; }
    if (!g_fwd_devfromdxgi) return E_NOTIMPL;
    return ((HRESULT (WINAPI *)(void *, void **))g_fwd_devfromdxgi)(dxgiDevice, graphicsDevice);
}

/* test-only export (the PC harness cannot load the ARM UnityPlayer, so it checks the caps-correction values against a synthetic caps object).
 * Returns the number of fields that are wrong; 0 = ok.  Never called in production. */
__declspec(dllexport) int l9shim_selftest_caps(void)
{
    u8 *c = (u8 *)VirtualAlloc(NULL, 0x400, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    int bad = 0;
    int saved = g_caps_logonly;
    if (!c) return 99;
    memset(c, 0xFF, 0x400);                 /* start wrong so every write is observable */
    g_caps_logonly = 0;                     /* the self-test checks the real writes regardless of the diagnostic log-only default */
    l9caps_apply(c);
    g_caps_logonly = saved;
    if (*(u32 *)(c + 0xbc) != 0xE3) ++bad;
    if (*(u32 *)(c + 0xdc) != 2048) ++bad;
    if (*(u32 *)(c + 0xe4) != 2048) ++bad;
    if (*(u32 *)(c + 0xe0) != 512) ++bad;
    if (*(u32 *)(c + 0xec) != 1) ++bad;
    if (*(u32 *)(c + 0xf0) != 0) ++bad;
    if (*(u32 *)(c + 0x378) != 1) ++bad;
    if (*(u32 *)(c + 0x3f4) != 0) ++bad;
    if (c[0x104] != 0) ++bad;
    if (c[0x107] != 0) ++bad;
    if (c[0x36c] != 0) ++bad;
    if (c[0x3f8] != 0) ++bad;
    if (c[0x3d9] != 0) ++bad;
    if (c[0x101] != 0) ++bad;
    VirtualFree(c, 0, MEM_RELEASE);
    return bad;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        log_open();
        lgf("==== l9shim loaded (build %s %s) ====", __DATE__, __TIME__);
        table_load();
    }
    return TRUE;
}
