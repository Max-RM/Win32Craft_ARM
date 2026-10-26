/* m0probe.c - M0 device probe: one small console program, run ONCE on each ARM32 tablet (Surface RT = Windows RT 8.1 / Tegra 3,
 * Surface 2 = Windows 10 build 15035 ARM32 / Tegra 4), that answers the Direct3D questions for three game ports in ONE log file
 * (m0probe.log next to the exe):  Redcon (WP8.1 XAML), Geometry Dash (WP8.0 Silverlight/ANGLE), Bad Piggies Rebooted (Unity 2018.4 UWP).
 * See tools/m0probe/README.md for what every test answers and how to read the log.
 *
 * One source file, two builds: x64 (cl, the PC proof run) and ARM32 (clang, thumbv7-pc-windows-msvc).  Freestanding on both: no CRT,
 * own memset/memcpy/formatter; imports KERNEL32 + USER32 only (d3d11.dll, dxgi.dll, d3dcompiler_47.dll, version.dll, advapi32.dll,
 * psapi are loaded with LoadLibrary/GetProcAddress, so the exe always starts and can say what is missing).
 *
 * Process model: the parent runs T1 itself, then re-launches this exe as "m0probe.exe --test <name> [--case <c>]" for every test that
 * talks to a graphics driver, waits (timeout), logs the exit code and goes on - a driver crash or hang costs one test, not the log.
 * Children append to the same m0probe.log and write "key=value" result lines to m0probe.res; the parent builds the plain-words
 * summary from that file.
 *
 * Usage:  m0probe.exe                  full run (what run_probe.bat does)
 *         m0probe.exe --quick          skip the speed tests
 *         m0probe.exe --test idx32     explicit only: IASetIndexBuffer(R32_UINT) at 9_1 (removed the Intel device on the PC)
 *         m0probe.exe --test vtshort   explicit only: 54-slot (short) device vtable copy (crashes inside DXGI on the PC)
 *         m0probe.exe --test <name> [--case <c>]   run one child test by hand (sys adapters fl shaders render vt speed)
 *
 * Heredoc note: this file was written with a file editor; keep it that way (backslashes and NULs). */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_3.h>
#include <stdarg.h>
#if defined(__clang__)
/* MSVC's <stdarg.h> comes first on the include path.  On ARM it defines va_start as "address of the last named parameter + 4", which relies on MSVC
 * homing r0-r3 next to each other; clang does not, so every variadic argument was garbage in the ARM32 build (the first %ls of the log crashed the
 * probe on the Surface 2, device run 2026-10-22).  The compiler's own implementation is right on every target. */
#undef va_start
#undef va_arg
#undef va_end
#define M0_VA_LIST __builtin_va_list
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#else
#define M0_VA_LIST va_list
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4100 4127 4189 4201 4204 4210 4221 4244 4245 4267 4305 4306 4310 4706 4996)
#endif

#if defined(_M_ARM) || defined(__arm__)
#define BUILD_NAME "ARM32"
#elif defined(_M_X64) || defined(__x86_64__)
#define BUILD_NAME "x64"
#else
#define BUILD_NAME "other"
#endif
#define PROBE_VERSION "m0probe 1.0"

/* ======================================================================================================================
 * no-CRT helpers (the compiler may emit calls to these)
 * ==================================================================================================================== */
#if defined(_M_X64) || defined(_M_IX86)
int _fltused = 0;   /* x86/x64 cl builds only; ARM does not reference it */
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define M0_CL 1                     /* real cl.exe: keep the optimiser from turning these loops into calls to themselves */
#pragma function(memset, memcpy, memcmp, memmove)
#pragma optimize("", off)
#endif
void *memset(void *d, int c, size_t n) { unsigned char *p = (unsigned char *)d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *p = (unsigned char *)d; const unsigned char *q = (const unsigned char *)s; while (n--) *p++ = *q++; return d; }
void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *p = (unsigned char *)d; const unsigned char *q = (const unsigned char *)s;
    if (p == q || !n) return d;
    if (p < q) { while (n--) *p++ = *q++; } else { p += n; q += n; while (n--) *--p = *--q; }
    return d;
}
int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    while (n--) { if (*p != *q) return *p < *q ? -1 : 1; ++p; ++q; }
    return 0;
}
#ifdef M0_CL
#pragma optimize("", on)
#endif

#if defined(_M_ARM) || defined(__arm__)
/* armrt.lib supplies the 32/64-bit integer division helpers (__rt_udiv, __rt_udiv64 ...); their divide-by-zero path references three CRT
 * security symbols that do not exist without a CRT.  Never reached here; defined only so the link succeeds. */
unsigned int __security_cookie = 0xBB40E64Eu;
void __cdecl __security_check_cookie(unsigned int cookie) { (void)cookie; }
int __cdecl __GSHandlerCheck(void *a, void *b, void *c, void *d) { (void)a; (void)b; (void)c; (void)d; return 1; /* ExceptionContinueSearch */ }
#endif

typedef unsigned long long u64;
typedef long long i64;
typedef unsigned int u32;
typedef unsigned char u8;

static int s_len(const char *s) { int n = 0; while (s[n]) ++n; return n; }
static int s_cmp(const char *a, const char *b) { while (*a && *a == *b) { ++a; ++b; } return (int)(u8)*a - (int)(u8)*b; }
static int s_eq(const char *a, const char *b) { return s_cmp(a, b) == 0; }
static int s_prefix(const char *s, const char *p) { while (*p) { if (*s != *p) return 0; ++s; ++p; } return 1; }
static char s_lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static int s_icontains(const char *h, const char *needle)
{
    int n = s_len(needle), i;
    if (!n) return 1;
    for (; *h; ++h) {
        for (i = 0; i < n; ++i) { if (!h[i] || s_lower(h[i]) != s_lower(needle[i])) break; }
        if (i == n) return 1;
    }
    return 0;
}
static void s_copy(char *d, int cap, const char *s) { int i = 0; if (cap <= 0) return; while (s[i] && i < cap - 1) { d[i] = s[i]; ++i; } d[i] = 0; }
static int w_len(const WCHAR *s) { int n = 0; while (s[n]) ++n; return n; }
static void w_copy(WCHAR *d, int cap, const WCHAR *s) { int i = 0; if (cap <= 0) return; while (s[i] && i < cap - 1) { d[i] = s[i]; ++i; } d[i] = 0; }
static void w_cat(WCHAR *d, int cap, const WCHAR *s) { int n = w_len(d); w_copy(d + n, cap - n, s); }
static int w_icontains_a(const WCHAR *h, const char *needle)
{
    int n = s_len(needle), i;
    if (!n) return 1;
    for (; *h; ++h) {
        for (i = 0; i < n; ++i) { WCHAR c = h[i]; if (!c || c >= 128 || s_lower((char)c) != s_lower(needle[i])) break; }
        if (i == n) return 1;
    }
    return 0;
}
static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }

/* ---------------------------------------------------------------------------------------------------------------------
 * tiny printf: %s %ls %c %d %i %u %x %X %p %%  with flags - 0, width (number or *), .precision (strings; %.Nf is not supported:
 * doubles are pre-formatted with F()), length l (= int on Windows), ll / I64 / z (64-bit, size_t).  Returns the full length.
 * ------------------------------------------------------------------------------------------------------------------- */
typedef struct { char *p; int cap, n; } Out;
static void o_ch(Out *o, char c) { if (o->n + 1 < o->cap) o->p[o->n] = c; o->n++; }
static void o_str(Out *o, const char *s, int len, int width, int left)
{
    int i, pad = width > len ? width - len : 0;
    if (!left) for (i = 0; i < pad; ++i) o_ch(o, ' ');
    for (i = 0; i < len; ++i) o_ch(o, s[i]);
    if (left) for (i = 0; i < pad; ++i) o_ch(o, ' ');
}
static int m_vsnprintf(char *buf, int cap, const char *fmt, M0_VA_LIST ap)
{
    Out o; o.p = buf; o.cap = cap; o.n = 0;
    while (*fmt) {
        int left = 0, zero = 0, width = 0, prec = -1, lng = 0;
        char c;
        if (*fmt != '%') { o_ch(&o, *fmt++); continue; }
        ++fmt;
        for (;; ++fmt) { if (*fmt == '-') left = 1; else if (*fmt == '0') zero = 1; else if (*fmt == '+' || *fmt == ' ' || *fmt == '#') {} else break; }
        if (*fmt == '*') { width = va_arg(ap, int); if (width < 0) { left = 1; width = -width; } ++fmt; }
        else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') { ++fmt; prec = 0; if (*fmt == '*') { prec = va_arg(ap, int); ++fmt; } else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0'); }
        for (;;) {
            if (*fmt == 'l') { ++fmt; if (*fmt == 'l') { lng = 2; ++fmt; } else if (lng < 1) lng = 1; }
            else if (*fmt == 'z' || *fmt == 'j' || *fmt == 't') { lng = (sizeof(void *) == 8) ? 2 : 0; ++fmt; }
            else if (*fmt == 'h') { ++fmt; }
            else if (fmt[0] == 'I' && fmt[1] == '6' && fmt[2] == '4') { lng = 2; fmt += 3; }
            else break;
        }
        c = *fmt ? *fmt++ : 0;
        if (c == '%') { o_ch(&o, '%'); }
        else if (c == 'c') { o_ch(&o, (char)va_arg(ap, int)); }
        else if (c == 's' && lng != 1) {
            const char *s = va_arg(ap, const char *); int len;
            if (!s) s = "(null)";
            len = s_len(s); if (prec >= 0 && prec < len) len = prec;
            o_str(&o, s, len, width, left);
        } else if (c == 's' || c == 'S') {   /* %ls: UTF-16 -> UTF-8 */
            const WCHAR *w = va_arg(ap, const WCHAR *); char tmp[768]; int len;
            if (!w) { o_str(&o, "(null)", 6, width, left); continue; }
            len = WideCharToMultiByte(CP_UTF8, 0, w, -1, tmp, (int)sizeof tmp - 1, NULL, NULL);
            len = len > 0 ? len - 1 : 0; tmp[len] = 0;
            if (prec >= 0 && prec < len) len = prec;
            o_str(&o, tmp, len, width, left);
        } else if (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X' || c == 'p') {
            char tmp[24]; int nt = 0, neg = 0, i; u64 v; unsigned base = (c == 'x' || c == 'X' || c == 'p') ? 16 : 10;
            const char *dig = (c == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
            if (c == 'p') { v = (u64)(size_t)va_arg(ap, void *); zero = 1; width = (int)sizeof(void *) * 2; }
            else if (c == 'd' || c == 'i') {
                i64 sv = (lng == 2) ? va_arg(ap, i64) : (i64)va_arg(ap, int);
                if (sv < 0) { neg = 1; v = (u64)(-sv); } else v = (u64)sv;
            } else v = (lng == 2) ? va_arg(ap, u64) : (u64)va_arg(ap, unsigned);
            if (v == 0) tmp[nt++] = '0';
            else if (base == 16) { while (v) { tmp[nt++] = dig[v & 15]; v >>= 4; } }
            else if ((v >> 32) == 0) { u32 v32 = (u32)v; while (v32) { tmp[nt++] = dig[v32 % 10u]; v32 /= 10u; } }
            else {   /* 64-bit value: divide by 10 with 32-bit steps only (no __rt_udiv64 from armrt.lib) */
                while (v) {
                    u32 hi = (u32)(v >> 32), lo = (u32)v, qh = hi / 10u, rh = hi % 10u, t, q1, r1, q0, r0;
                    t = (rh << 16) | (lo >> 16); q1 = t / 10u; r1 = t % 10u;
                    t = (r1 << 16) | (lo & 0xffffu); q0 = t / 10u; r0 = t % 10u;
                    tmp[nt++] = dig[r0];
                    v = ((u64)qh << 32) | (u64)((q1 << 16) | q0);
                }
            }
            while (prec > nt && nt < 22) tmp[nt++] = '0';
            { int total = nt + neg; int pad = width > total ? width - total : 0;
              if (!left && !zero) for (i = 0; i < pad; ++i) o_ch(&o, ' ');
              if (neg) o_ch(&o, '-');
              if (!left && zero) for (i = 0; i < pad; ++i) o_ch(&o, '0');
              while (nt) o_ch(&o, tmp[--nt]);
              if (left) for (i = 0; i < pad; ++i) o_ch(&o, ' '); }
        } else { o_ch(&o, '%'); if (c) o_ch(&o, c); }
    }
    if (cap > 0) o.p[o.n < cap ? o.n : cap - 1] = 0;
    return o.n;
}
static int m_snprintf(char *buf, int cap, const char *fmt, ...)
{
    M0_VA_LIST ap; int n;
    va_start(ap, fmt); n = m_vsnprintf(buf, cap, fmt, ap); va_end(ap);
    return n;
}

/* F(v, prec): double -> text without 64-bit conversions (32-bit unsigned only); ring of 16 buffers so several can appear in one printf */
static const char *F(double v, int prec)
{
    static char ring[16][32]; static int idx;
    char *b = ring[idx++ & 15]; int n = 0, i;
    double scale = 1.0, ip; unsigned ipart, fpart, iscale = 1;
    if (v != v) { s_copy(b, 32, "nan"); return b; }
    if (v < 0) { b[n++] = '-'; v = -v; }
    if (prec < 0) prec = 0;
    if (prec > 6) prec = 6;
    for (i = 0; i < prec; ++i) { scale *= 10.0; iscale *= 10u; }
    if (v >= 4.0e9) { s_copy(b + n, 32 - n, "huge"); return b; }
    ip = (double)(unsigned)v;               /* floor for non-negative values */
    fpart = (unsigned)((v - ip) * scale + 0.5);
    ipart = (unsigned)ip;
    if (fpart >= iscale) { fpart -= iscale; ipart += 1; }
    n += m_snprintf(b + n, 32 - n, "%u", ipart);
    if (prec) { n += m_snprintf(b + n, 32 - n, ".%0*u", prec, fpart); }
    return b;
}

/* ======================================================================================================================
 * time, paths, files
 * ==================================================================================================================== */
static i64 g_qpf;
static double i64_to_d(i64 v) { u32 hi = (u32)((u64)v >> 32), lo = (u32)v; return (double)hi * 4294967296.0 + (double)lo; }
static i64 qpc(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
static double qpc_ms(i64 t0, i64 t1) { return i64_to_d(t1 - t0) * 1000.0 / i64_to_d(g_qpf); }
static double ms_since(i64 t0) { return qpc_ms(t0, qpc()); }

static WCHAR g_exe[MAX_PATH], g_dir[MAX_PATH];   /* g_dir = folder of the exe, with trailing backslash */
static void init_paths(void)
{
    int n;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f); g_qpf = f.QuadPart ? f.QuadPart : 1;
    GetModuleFileNameW(NULL, g_exe, MAX_PATH);
    w_copy(g_dir, MAX_PATH, g_exe);
    n = w_len(g_dir);
    while (n > 0 && g_dir[n - 1] != L'\\') --n;
    g_dir[n] = 0;
}
static void wpath(WCHAR *dst, const WCHAR *rel) { w_copy(dst, MAX_PATH, g_dir); w_cat(dst, MAX_PATH, rel); }
static void wpath_a(WCHAR *dst, const char *rel)
{
    int n = w_len(g_dir), i = 0;
    w_copy(dst, MAX_PATH, g_dir);
    while (rel[i] && n + i < MAX_PATH - 1) { dst[n + i] = (WCHAR)(u8)rel[i]; ++i; }
    dst[n + i] = 0;
}
static void a2w(WCHAR *dst, int cap, const char *s) { int i = 0; while (s[i] && i < cap - 1) { dst[i] = (WCHAR)(u8)s[i]; ++i; } dst[i] = 0; }

static u8 *read_file(const WCHAR *path, unsigned *size)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD sz, got = 0; u8 *b;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL);
    if (sz == INVALID_FILE_SIZE || sz > 64u * 1024u * 1024u) { CloseHandle(h); return NULL; }
    b = (u8 *)HeapAlloc(GetProcessHeap(), 0, sz ? sz : 1);
    if (!b) { CloseHandle(h); return NULL; }
    if (!ReadFile(h, b, sz, &got, NULL) || got != sz) { HeapFree(GetProcessHeap(), 0, b); CloseHandle(h); return NULL; }
    CloseHandle(h);
    if (size) *size = sz;
    return b;
}
static void free_file(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }
static int file_exists(const WCHAR *path) { DWORD a = GetFileAttributesW(path); return a != INVALID_FILE_ATTRIBUTES; }

/* sorted directory listing: names are stored in g_names (max 256 x 80 chars); returns the count */
static WCHAR g_names[256][80];
static int list_dir(const WCHAR *pattern, int want_dirs)
{
    WIN32_FIND_DATAW fd; HANDLE h; int n = 0, i, j;
    h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        int isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isdir != want_dirs) continue;
        if (fd.cFileName[0] == L'.' && (!fd.cFileName[1] || (fd.cFileName[1] == L'.' && !fd.cFileName[2]))) continue;
        if (n < 256) { w_copy(g_names[n], 80, fd.cFileName); ++n; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (i = 1; i < n; ++i) {   /* insertion sort, case-insensitive ASCII */
        WCHAR tmp[80]; w_copy(tmp, 80, g_names[i]);
        for (j = i - 1; j >= 0; --j) {
            int k = 0; WCHAR a, b;
            for (;; ++k) { a = g_names[j][k]; b = tmp[k]; if (a >= 'A' && a <= 'Z') a += 32; if (b >= 'A' && b <= 'Z') b += 32; if (a != b || !a) break; }
            if (a > b) w_copy(g_names[j + 1], 80, g_names[j]); else break;
        }
        w_copy(g_names[j + 1], 80, tmp);
    }
    return n;
}

/* ======================================================================================================================
 * logging: console + m0probe.log (every process appends; the parent truncates it at start), results file m0probe.res
 * ==================================================================================================================== */
static HANDLE g_con = INVALID_HANDLE_VALUE, g_log = INVALID_HANDLE_VALUE, g_res = INVALID_HANDLE_VALUE;
static DWORD g_log_err;
static char g_tkey[64];            /* "shaders.hw" - prefix of the status record */
static char g_line[3072];

static void write_all(HANDLE h, const char *s, int n)
{
    DWORD w;
    if (h == INVALID_HANDLE_VALUE || h == NULL || n <= 0) return;
    WriteFile(h, s, (DWORD)n, &w, NULL);
}
static void out_line(const char *s, int n)
{
    write_all(g_con, s, n);
    write_all(g_log, s, n);
    write_all(g_con, "\r\n", 2);
    write_all(g_log, "\r\n", 2);
}
static void lg(const char *fmt, ...)
{
    M0_VA_LIST ap; int n;
    va_start(ap, fmt); n = m_vsnprintf(g_line, (int)sizeof g_line, fmt, ap); va_end(ap);
    if (n >= (int)sizeof g_line) n = (int)sizeof g_line - 1;
    out_line(g_line, n);
}
static HANDLE open_append(const WCHAR *path)
{
    return CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}
static void truncate_file(const WCHAR *path)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
static void open_log(int truncate)
{
    WCHAR p[MAX_PATH];
    if (truncate) { wpath(p, L"m0probe.log"); truncate_file(p); wpath(p, L"m0probe.res"); truncate_file(p); }
    wpath(p, L"m0probe.log"); g_log = open_append(p);
    if (g_log == INVALID_HANDLE_VALUE) g_log_err = GetLastError();
    wpath(p, L"m0probe.res"); g_res = open_append(p);
}
/* result record: key=value */
static void rec(const char *key, const char *fmt, ...)
{
    char buf[1024]; int n; M0_VA_LIST ap;
    n = m_snprintf(buf, (int)sizeof buf, "%s=", key);
    va_start(ap, fmt); n += m_vsnprintf(buf + n, (int)sizeof buf - n - 2, fmt, ap); va_end(ap);
    if (n > (int)sizeof buf - 3) n = (int)sizeof buf - 3;
    buf[n++] = '\n';
    write_all(g_res, buf, n);
}
/* key under the current test: rect("fl.hw.best") -> stored as written; helper for formatted keys */
static const char *K(const char *fmt, ...)
{
    static char ring[8][96]; static int idx; char *b = ring[idx++ & 7]; M0_VA_LIST ap;
    va_start(ap, fmt); m_vsnprintf(b, 96, fmt, ap); va_end(ap);
    return b;
}

/* crash reporting inside children: log the exception, the faulting module and offset */
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
    MEMORY_BASIC_INFORMATION mbi; WCHAR name[MAX_PATH]; void *addr = ep->ExceptionRecord->ExceptionAddress;
    name[0] = 0; memset(&mbi, 0, sizeof mbi);
    if (VirtualQuery(addr, &mbi, sizeof mbi) && mbi.AllocationBase) GetModuleFileNameW((HMODULE)mbi.AllocationBase, name, MAX_PATH);
    lg("!!! UNHANDLED EXCEPTION 0x%08x at %p in %ls + 0x%x (test %s)", (unsigned)ep->ExceptionRecord->ExceptionCode, addr, name[0] ? name : L"?",
         (unsigned)((size_t)addr - (size_t)mbi.AllocationBase), g_tkey);
    if (ep->ExceptionRecord->ExceptionCode == 0xC0000005u && ep->ExceptionRecord->NumberParameters >= 2)
        lg("!!! access violation: %s address %p", ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "reading" : (ep->ExceptionRecord->ExceptionInformation[0] == 8 ? "executing" : "writing"),
             (void *)ep->ExceptionRecord->ExceptionInformation[1]);
    return EXCEPTION_EXECUTE_HANDLER;
}

/* ======================================================================================================================
 * expectation bookkeeping (child side): chk()/rep() lines like the PC bench, counters for the child's PASS/FAIL
 * ==================================================================================================================== */
static int g_pass, g_fail, g_info, g_diff;   /* g_diff: table rows that differ from the PC measurement (informational) */
/* expect: 1 = should succeed, 0 = should fail, -1 = just report */
static void rep(const char *name, HRESULT hr, int expect)
{
    const char *v;
    if (expect < 0) { v = "info"; g_info++; }
    else if ((SUCCEEDED(hr) ? 1 : 0) == expect) { v = "as-expected"; g_pass++; }
    else { v = "UNEXPECTED"; g_fail++; }
    lg("  %-72s hr=0x%08x %s", name, (unsigned)hr, v);
}
/* pc: what the PC (Intel UHD 620 and WARP, identical) returned: 1 ok, 0 failed, -1 not measured; prints a loud marker when this device differs */
static void trep(const char *name, HRESULT hr, int pc)
{
    const char *v = "";
    int ok = SUCCEEDED(hr) ? 1 : 0;
    if (pc >= 0) { if (ok == pc) v = "[same as PC]"; else v = "<<<< DIFFERS FROM PC"; }
    if (pc >= 0 && ok != pc) g_diff++;
    g_info++;
    lg("  %-66s %-4s hr=0x%08x %s", name, ok ? "OK" : "FAIL", (unsigned)hr, v);
}

/* ======================================================================================================================
 * dynamic loading of optional DLLs
 * ==================================================================================================================== */
typedef HRESULT (WINAPI *PFN_D3D11CreateDevice_t)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
                                                  ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
typedef HRESULT (WINAPI *PFN_CreateDXGIFactory1_t)(REFIID, void **);
static PFN_D3D11CreateDevice_t pD3D11CreateDevice;
static PFN_CreateDXGIFactory1_t pCreateDXGIFactory1;

static int load_d3d(void)
{
    HMODULE d3d = LoadLibraryW(L"d3d11.dll"), dxgi = LoadLibraryW(L"dxgi.dll");
    if (!d3d) lg("LoadLibrary(d3d11.dll) FAILED, error %u", (unsigned)GetLastError());
    if (!dxgi) lg("LoadLibrary(dxgi.dll) FAILED, error %u", (unsigned)GetLastError());
    if (d3d) pD3D11CreateDevice = (PFN_D3D11CreateDevice_t)GetProcAddress(d3d, "D3D11CreateDevice");
    if (dxgi) pCreateDXGIFactory1 = (PFN_CreateDXGIFactory1_t)GetProcAddress(dxgi, "CreateDXGIFactory1");
    if (d3d && !pD3D11CreateDevice) lg("d3d11.dll has no D3D11CreateDevice export");
    if (dxgi && !pCreateDXGIFactory1) lg("dxgi.dll has no CreateDXGIFactory1 export");
    return pD3D11CreateDevice && pCreateDXGIFactory1;
}

/* advapi32 registry (read-only, optional) */
typedef LONG (WINAPI *PFN_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
typedef LONG (WINAPI *PFN_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LONG (WINAPI *PFN_RegCloseKey)(HKEY);
static PFN_RegOpenKeyExW pRegOpen; static PFN_RegQueryValueExW pRegQuery; static PFN_RegCloseKey pRegClose;
static int reg_ready(void)
{
    static int tried;
    if (!tried) {
        HMODULE a = LoadLibraryW(L"advapi32.dll");
        tried = 1;
        if (a) {
            pRegOpen = (PFN_RegOpenKeyExW)GetProcAddress(a, "RegOpenKeyExW");
            pRegQuery = (PFN_RegQueryValueExW)GetProcAddress(a, "RegQueryValueExW");
            pRegClose = (PFN_RegCloseKey)GetProcAddress(a, "RegCloseKey");
        }
    }
    return pRegOpen && pRegQuery && pRegClose;
}
/* reads a REG_SZ / REG_MULTI_SZ / REG_DWORD value as UTF-8 text (multi-sz joined with " | "); returns 1 when found */
static int reg_text(const WCHAR *key, const WCHAR *val, char *out, int cap)
{
    HKEY k; DWORD type = 0, size = 0; static WCHAR buf[1024]; int ok = 0;
    out[0] = 0;
    if (!reg_ready()) return 0;
    if (pRegOpen(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    size = sizeof buf - 4;
    memset(buf, 0, sizeof buf);
    if (pRegQuery(k, val, NULL, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
        ok = 1;
        if (type == REG_DWORD) m_snprintf(out, cap, "%u", *(DWORD *)buf);
        else if (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ) {
            int i, n = (int)(size / 2), w = 0; char tmp[600];
            for (i = 0; i < n && w < (int)sizeof tmp - 8; ++i) {
                if (!buf[i]) { if (type != REG_MULTI_SZ || i + 1 >= n || !buf[i + 1]) break; tmp[w++] = ' '; tmp[w++] = '|'; tmp[w++] = ' '; }
                else tmp[w++] = buf[i] < 128 ? (char)buf[i] : '?';
            }
            tmp[w] = 0; s_copy(out, cap, tmp);
        } else m_snprintf(out, cap, "(type %u, %u bytes)", (unsigned)type, (unsigned)size);
    }
    pRegClose(k);
    return ok;
}

/* version.dll file version */
typedef DWORD (WINAPI *PFN_GFVISize)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *PFN_GFVI)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *PFN_VQV)(LPCVOID, LPCWSTR, LPVOID *, PUINT);
static const char *file_version(const WCHAR *path)
{
    static char out[96]; static PFN_GFVISize psz; static PFN_GFVI pget; static PFN_VQV pq; static int tried;
    DWORD dummy = 0, sz; void *buf; VS_FIXEDFILEINFO *ffi = NULL; UINT len = 0;
    if (!tried) {
        HMODULE v = LoadLibraryW(L"version.dll"); tried = 1;
        if (v) { psz = (PFN_GFVISize)GetProcAddress(v, "GetFileVersionInfoSizeW"); pget = (PFN_GFVI)GetProcAddress(v, "GetFileVersionInfoW"); pq = (PFN_VQV)GetProcAddress(v, "VerQueryValueW"); }
    }
    if (!psz || !pget || !pq) { s_copy(out, sizeof out, "(version.dll unavailable)"); return out; }
    sz = psz(path, &dummy);
    if (!sz) { s_copy(out, sizeof out, "(no version resource)"); return out; }
    buf = HeapAlloc(GetProcessHeap(), 0, sz);
    if (!buf) { s_copy(out, sizeof out, "(out of memory)"); return out; }
    if (pget(path, 0, sz, buf) && pq(buf, L"\\", (LPVOID *)&ffi, &len) && ffi && len >= sizeof(VS_FIXEDFILEINFO))
        m_snprintf(out, sizeof out, "%u.%u.%u.%u", (unsigned)(ffi->dwFileVersionMS >> 16), (unsigned)(ffi->dwFileVersionMS & 0xffff),
                   (unsigned)(ffi->dwFileVersionLS >> 16), (unsigned)(ffi->dwFileVersionLS & 0xffff));
    else s_copy(out, sizeof out, "(unreadable)");
    HeapFree(GetProcessHeap(), 0, buf);
    return out;
}

/* ---------------------------------------------------------------------------------------------------------------------
 * loaded modules: baseline at the start of a test, then "what did device creation load" (d3d10warp.dll? a vendor user-mode driver?)
 * ------------------------------------------------------------------------------------------------------------------- */
static HMODULE g_seen[512]; static int g_nseen;
static int enum_modules(HMODULE *out, int max)
{
    typedef BOOL (WINAPI *PFN)(HANDLE, HMODULE *, DWORD, LPDWORD);
    static PFN fn; static int tried;
    DWORD needed = 0; int n;
    if (!tried) {
        HMODULE k = GetModuleHandleW(L"kernel32.dll"); tried = 1;
        fn = k ? (PFN)GetProcAddress(k, "K32EnumProcessModules") : NULL;
        if (!fn) { HMODULE p = LoadLibraryW(L"psapi.dll"); if (p) fn = (PFN)GetProcAddress(p, "EnumProcessModules"); }
    }
    if (!fn) return -1;
    if (!fn(GetCurrentProcess(), out, (DWORD)(max * (int)sizeof(HMODULE)), &needed)) return -1;
    n = (int)(needed / sizeof(HMODULE));
    return n > max ? max : n;
}
static void mods_baseline(void) { int n = enum_modules(g_seen, 512); g_nseen = n < 0 ? 0 : n; }
static void w2a(char *d, int cap, const WCHAR *w) { int i = 0; while (w[i] && i < cap - 1) { d[i] = w[i] < 128 ? (char)w[i] : '?'; ++i; } d[i] = 0; }
/* prints "prefix text..." wrapped at about 100 columns (continuation lines indented) */
static void print_wrapped(const char *prefix, const char *text)
{
    char line[160]; int n, i = 0, first = 1;
    while (text[i]) {
        n = 0;
        while (text[i] && n < 100) line[n++] = text[i++];
        while (text[i] && text[i - 1] != ' ' && n > 0 && line[n - 1] != ' ') { line[n++] = text[i++]; if (n >= 150) break; }
        line[n] = 0;
        lg("%s %s", first ? prefix : "        ", line);
        first = 0;
    }
}
static const WCHAR *base_name(const WCHAR *p) { const WCHAR *b = p, *q; for (q = p; *q; ++q) if (*q == L'\\') b = q + 1; return b; }
/* prints the modules that appeared since the last call; sets *warp / *vendor when d3d10warp.dll / a GPU driver module showed up */
static void mods_report(const char *tag, int full_list)
{
    static HMODULE now[512]; static WCHAR path[MAX_PATH]; static char names[2048];
    int n = enum_modules(now, 512), i, j, newc = 0, warp = 0, vendor = 0, nl = 0;
    if (n < 0) { lg("    (cannot enumerate modules: no K32EnumProcessModules / psapi)"); return; }
    if (full_list) {
        nl = 0; names[0] = 0;
        for (i = 0; i < n; ++i) { char b[80]; path[0] = 0; GetModuleFileNameW(now[i], path, MAX_PATH); w2a(b, sizeof b, base_name(path)); if (nl + 2 + s_len(b) < (int)sizeof names) nl += m_snprintf(names + nl, (int)sizeof names - nl, "%s ", b); }
        print_wrapped("    all modules loaded now:", names);
    }
    nl = 0; names[0] = 0;
    for (i = 0; i < n; ++i) {
        int seen = 0;
        for (j = 0; j < g_nseen; ++j) if (g_seen[j] == now[i]) { seen = 1; break; }
        if (seen) continue;
        path[0] = 0; GetModuleFileNameW(now[i], path, MAX_PATH);
        { const WCHAR *bn = base_name(path); const char *note = NULL;
          if (w_icontains_a(bn, "d3d10warp") || w_icontains_a(bn, "d3d11warp")) { warp = 1; note = "WARP software rasterizer"; }
          else if ((bn[0] == L'n' || bn[0] == L'N') && (bn[1] == L'v' || bn[1] == L'V')) { vendor = 1; note = "NVIDIA user-mode driver?"; }
          else if (w_icontains_a(path, "driverstore") || w_icontains_a(path, "filerepository")) { vendor = 1; note = "GPU vendor driver (DriverStore)"; }
          else if (w_icontains_a(bn, "tegra") || w_icontains_a(bn, "adreno") || w_icontains_a(bn, "mali") || w_icontains_a(bn, "igd") || w_icontains_a(bn, "ig9") || w_icontains_a(bn, "amd") || w_icontains_a(bn, "ati")) { vendor = 1; note = "GPU vendor driver?"; }
          if (note) lg("    >>> new module after device creation: %ls   <-- %s", path, note);
          else { char b[80]; w2a(b, sizeof b, bn); if (nl + 2 + s_len(b) < (int)sizeof names) nl += m_snprintf(names + nl, (int)sizeof names - nl, "%s ", b); }
          newc++; }
        if (g_nseen < 512) g_seen[g_nseen++] = now[i];
    }
    if (!newc) lg("    (no new modules loaded)");
    else if (nl) print_wrapped("    other modules loaded by device creation:", names);
    rec(K("mods.%s.warp", tag), "%d", warp);
    rec(K("mods.%s.vendor", tag), "%d", vendor);
}

/* ---------------------------------------------------------------------------------------------------------------------
 * D3D helpers
 * ------------------------------------------------------------------------------------------------------------------- */
#define REL(p) do { if (p) { IUnknown_Release((IUnknown *)(p)); (p) = NULL; } } while (0)
static ID3D11Device *dev; static ID3D11DeviceContext *ctx; static D3D_FEATURE_LEVEL g_fl;

static const char *fl_name(unsigned fl)
{
    switch (fl) {
    case 0xb100: return "11_1"; case 0xb000: return "11_0"; case 0xa100: return "10_1"; case 0xa000: return "10_0";
    case 0x9300: return "9_3"; case 0x9200: return "9_2"; case 0x9100: return "9_1"; default: return "?";
    }
}
static const D3D_FEATURE_LEVEL k_lv_all[7] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
                                               D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
static const D3D_FEATURE_LEVEL k_lv_91[1] = { D3D_FEATURE_LEVEL_9_1 };

static HRESULT make_dev(IDXGIAdapter *ad, D3D_DRIVER_TYPE dt, UINT flags, const D3D_FEATURE_LEVEL *lv, UINT n)
{
    HRESULT hr;
    dev = NULL; ctx = NULL; g_fl = (D3D_FEATURE_LEVEL)0;
    hr = pD3D11CreateDevice(ad, dt, NULL, flags, lv, n, D3D11_SDK_VERSION, &dev, &g_fl, &ctx);
    if (FAILED(hr)) { dev = NULL; ctx = NULL; }
    return hr;
}
static void drop_dev(void)
{
    if (ctx) { ID3D11DeviceContext_ClearState(ctx); ID3D11DeviceContext_Flush(ctx); }
    REL(ctx); REL(dev);
}

enum { AC_SOFT = 1, AC_HW = 2, AC_NVIDIA = 3, AC_L9 = 4 };
static int adapter_class(unsigned vendor, unsigned flags, const char *name)
{
    if (flags & 2u) return AC_SOFT;                       /* DXGI_ADAPTER_FLAG_SOFTWARE */
    if (vendor == 0x1414u) return AC_SOFT;                /* Microsoft: Basic Render Driver / WARP */
    /* a feature level 9_x device of a D3D9-class (WDDM 1.x) driver goes through Microsoft's D3D10Level9.dll, whose DXGI adapter is called "Software
     * Adapter" with vendor 0 and NO software flag - it is the GPU (Surface 2 run 2026-10-22: nvd3dum.dll loaded, 28x the WARP fill rate) */
    if (vendor == 0 && s_icontains(name, "software adapter")) return AC_L9;
    if (s_icontains(name, "software") || s_icontains(name, "basic render") || s_icontains(name, "warp")) return AC_SOFT;
    if (vendor == 0x10DEu) return AC_NVIDIA;
    if (vendor == 0) return AC_SOFT;
    return AC_HW;
}
static const char *ac_text(int c)
{
    return c == AC_NVIDIA ? "NVIDIA hardware" : (c == AC_HW ? "hardware" : (c == AC_L9 ? "D3D10Level9 layer over the GPU driver (see the module lines)" : "SOFTWARE"));
}

/* description of the adapter a device runs on; returns the adapter class */
static int dev_adapter(ID3D11Device *d, char *name, int cap, unsigned *vendor, unsigned *devid, unsigned *flags)
{
    IDXGIDevice *dd = NULL; IDXGIAdapter *ad = NULL; IDXGIAdapter1 *a1 = NULL; DXGI_ADAPTER_DESC1 d1; DXGI_ADAPTER_DESC d0; int got = 0;
    name[0] = 0; *vendor = *devid = *flags = 0;
    memset(&d1, 0, sizeof d1); memset(&d0, 0, sizeof d0);
    if (SUCCEEDED(ID3D11Device_QueryInterface(d, &IID_IDXGIDevice, (void **)&dd)) && dd) {
        if (SUCCEEDED(IDXGIDevice_GetAdapter(dd, &ad)) && ad) {
            if (SUCCEEDED(IDXGIAdapter_QueryInterface(ad, &IID_IDXGIAdapter1, (void **)&a1)) && a1 && SUCCEEDED(IDXGIAdapter1_GetDesc1(a1, &d1))) {
                WideCharToMultiByte(CP_UTF8, 0, d1.Description, -1, name, cap - 1, NULL, NULL); name[cap - 1] = 0;
                *vendor = d1.VendorId; *devid = d1.DeviceId; *flags = d1.Flags; got = 1;
            } else if (SUCCEEDED(IDXGIAdapter_GetDesc(ad, &d0))) {
                WideCharToMultiByte(CP_UTF8, 0, d0.Description, -1, name, cap - 1, NULL, NULL); name[cap - 1] = 0;
                *vendor = d0.VendorId; *devid = d0.DeviceId; got = 1;
            }
        }
    }
    REL(a1); REL(ad); REL(dd);
    return got ? adapter_class(*vendor, *flags, name) : 0;
}

/* opens a device for the tests: warp = WARP else HARDWARE; only91 = request {9_1} (our host), else the full list.  Logs hr / level / adapter. */
static int open_dev(int warp, int only91, const char *tag)
{
    HRESULT hr; char name[160]; unsigned v, d, f; int cls;
    D3D_DRIVER_TYPE dt = warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    hr = make_dev(NULL, dt, flags, only91 ? k_lv_91 : k_lv_all, only91 ? 1 : 7);
    if (hr == E_INVALIDARG && !only91) hr = make_dev(NULL, dt, flags, k_lv_all + 1, 6);
    lg("  device %s %s: hr=0x%08x FL=%s (0x%x)", warp ? "WARP" : "HARDWARE", only91 ? "{9_1 only}" : "{11_1..9_1}", (unsigned)hr, fl_name((unsigned)g_fl), (unsigned)g_fl);
    if (FAILED(hr)) { rec(K("dev.%s.hr", tag), "0x%08x", (unsigned)hr); return 0; }
    cls = dev_adapter(dev, name, (int)sizeof name, &v, &d, &f);
    lg("  adapter behind it: \"%s\" vendor=%04x device=%04x flags=0x%x -> %s", name, v, d, f, cls ? ac_text(cls) : "?");
    rec(K("dev.%s.adapter", tag), "%s", name);
    rec(K("dev.%s.class", tag), "%d", cls);
    rec(K("dev.%s.fl", tag), "0x%x", (unsigned)g_fl);
    return 1;
}

/* all the CheckFeatureSupport answers the ports care about (drives the caps fix-ups) */
static void dev_features(const char *tag)
{
    HRESULT hr;
    D3D11_FEATURE_DATA_THREADING th; D3D11_FEATURE_DATA_DOUBLES db; D3D11_FEATURE_DATA_D3D9_OPTIONS o9; D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT sh;
    D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT si; D3D11_FEATURE_DATA_D3D11_OPTIONS o11; D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS x10;
    D3D11_FEATURE_DATA_ARCHITECTURE_INFO ai;
    memset(&th, 0, sizeof th); memset(&db, 0, sizeof db); memset(&o9, 0, sizeof o9); memset(&sh, 0, sizeof sh); memset(&si, 0, sizeof si);
    memset(&o11, 0, sizeof o11); memset(&x10, 0, sizeof x10); memset(&ai, 0, sizeof ai);
    hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_THREADING, &th, sizeof th);
    lg("    THREADING            hr=0x%08x DriverConcurrentCreates=%d DriverCommandLists=%d", (unsigned)hr, th.DriverConcurrentCreates, th.DriverCommandLists);
    hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_DOUBLES, &db, sizeof db);
    lg("    DOUBLES              hr=0x%08x DoublePrecisionFloatShaderOps=%d", (unsigned)hr, db.DoublePrecisionFloatShaderOps);
    hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D9_OPTIONS, &o9, sizeof o9);
    lg("    D3D9_OPTIONS         hr=0x%08x FullNonPow2TextureSupport=%d", (unsigned)hr, o9.FullNonPow2TextureSupport);
    rec(K("feat.%s.fullnpot", tag), "%d", SUCCEEDED(hr) ? o9.FullNonPow2TextureSupport : -1);
    hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D9_SHADOW_SUPPORT, &sh, sizeof sh);
    lg("    D3D9_SHADOW_SUPPORT  hr=0x%08x SupportsDepthAsTextureWithLessEqualComparisonFilter=%d", (unsigned)hr, sh.SupportsDepthAsTextureWithLessEqualComparisonFilter);
    hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT, &si, sizeof si);
    lg("    D3D9_SIMPLE_INSTANCING hr=0x%08x SimpleInstancingSupported=%d", (unsigned)hr, si.SimpleInstancingSupported);
    hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D11_OPTIONS, &o11, sizeof o11);
    lg("    D3D11_OPTIONS        hr=0x%08x OutputMergerLogicOp=%d ConstantBufferOffsetting=%d ConstantBufferPartialUpdate=%d", (unsigned)hr,
       o11.OutputMergerLogicOp, o11.ConstantBufferOffsetting, o11.ConstantBufferPartialUpdate);
    lg("                         MapNoOverwriteOnDynamicConstantBuffer=%d MapNoOverwriteOnDynamicBufferSRV=%d ClearView=%d CopyWithOverlap=%d DiscardAPIsSeenByDriver=%d",
       o11.MapNoOverwriteOnDynamicConstantBuffer, o11.MapNoOverwriteOnDynamicBufferSRV, o11.ClearView, o11.CopyWithOverlap, o11.DiscardAPIsSeenByDriver);
    rec(K("feat.%s.cboffset", tag), "%d", SUCCEEDED(hr) ? o11.ConstantBufferOffsetting : -1);
    rec(K("feat.%s.logicop", tag), "%d", SUCCEEDED(hr) ? o11.OutputMergerLogicOp : -1);
    hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &x10, sizeof x10);
    lg("    D3D10_X_HARDWARE_OPTIONS hr=0x%08x ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x=%d", (unsigned)hr, x10.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x);
    hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_ARCHITECTURE_INFO, &ai, sizeof ai);
    lg("    ARCHITECTURE_INFO    hr=0x%08x TileBasedDeferredRenderer=%d", (unsigned)hr, ai.TileBasedDeferredRenderer);
    rec(K("feat.%s.tbdr", tag), "%d", SUCCEEDED(hr) ? ai.TileBasedDeferredRenderer : -1);
}

/* ======================================================================================================================
 * DXBC helpers
 * ==================================================================================================================== */
static const u32 md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
static const u8 md5_s[64] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
static void md5_block(u32 st[4], const u8 *blk)
{
    u32 M[16], a, b, c, d, f, g, i, t;
    for (i = 0; i < 16; ++i) M[i] = rd32(blk + 4 * i);
    a = st[0]; b = st[1]; c = st[2]; d = st[3];
    for (i = 0; i < 64; ++i) {
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | ~d); g = (7 * i) & 15; }
        f = f + a + md5_k[i] + M[g];
        a = d; d = c; c = b;
        t = md5_s[i];
        b = b + ((f << t) | (f >> (32 - t)));
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
}
/* the DXBC container checksum: MD5 over bytes 20.., with the length folded into the padding block (verified on 78 real containers) */
static void dx_checksum(const u8 *dxbc, unsigned total, u8 *out16)
{
    const u8 *d = dxbc + 20; unsigned size = total - 20, nfull = size / 64, r = size % 64, i; u8 blk[64];
    u32 st[4]; st[0] = 0x67452301u; st[1] = 0xefcdab89u; st[2] = 0x98badcfeu; st[3] = 0x10325476u;
    for (i = 0; i < nfull; ++i) md5_block(st, d + 64 * i);
    d += 64 * nfull;
    if (r >= 56) {
        memset(blk, 0, 64); memcpy(blk, d, r); blk[r] = 0x80; md5_block(st, blk);
        memset(blk, 0, 64); wr32(blk, size * 8); wr32(blk + 60, size * 2 | 1); md5_block(st, blk);
    } else {
        memset(blk, 0, 64); wr32(blk, size * 8); memcpy(blk + 4, d, r); blk[4 + r] = 0x80; wr32(blk + 60, size * 2 | 1); md5_block(st, blk);
    }
    wr32(out16, st[0]); wr32(out16 + 4, st[1]); wr32(out16 + 8, st[2]); wr32(out16 + 12, st[3]);
}
static const u8 *dx_chunk(const u8 *b, unsigned n, const char *fourcc, unsigned *clen)
{
    unsigned cnt, i;
    if (!b || n < 36 || memcmp(b, "DXBC", 4)) return NULL;
    cnt = rd32(b + 28);
    if (cnt > 64 || 32 + 4 * cnt > n) return NULL;
    for (i = 0; i < cnt; ++i) {
        unsigned o = rd32(b + 32 + 4 * i);
        if (o + 8 <= n && !memcmp(b + o, fourcc, 4)) {
            unsigned l = rd32(b + o + 4);
            if (o + 8 + l > n) return NULL;
            if (clen) *clen = l;
            return b + o + 8;
        }
    }
    return NULL;
}
/* 0 = pixel, 1 = vertex, 2 = geometry ..., -1 = no shader chunk */
static int dx_type(const u8 *b, unsigned n)
{
    unsigned l = 0; const u8 *s = dx_chunk(b, n, "SHDR", &l);
    if (!s) s = dx_chunk(b, n, "SHEX", &l);
    if (!s || l < 4) return -1;
    return (int)(rd32(s) >> 16);
}
/* Direct3D 9 payload level of a DXBC: 0 = no Aon9 chunk (plain SM4), 1 = level_9_1 (token xxxx0200), 3 = level_9_3 (xxxx0201), 9 = other */
static int dx_level9(const u8 *b, unsigned n)
{
    unsigned l = 0, off, tok; const u8 *a = dx_chunk(b, n, "Aon9", &l);
    if (!a) return 0;
    if (l < 16) return 9;
    off = rd32(a + 12);
    if (off + 4 > l) return 9;
    tok = rd32(a + off);
    return (tok & 0xffffu) == 0x0200u ? 1 : ((tok & 0xffffu) == 0x0201u ? 3 : 9);
}
static const char *dx_typename(int t) { return t == 1 ? "VS" : (t == 0 ? "PS" : (t == 2 ? "GS" : "??")); }
static const char *lvl_text(int l) { return l == 0 ? "SM4 only" : (l == 1 ? "level_9_1" : (l == 3 ? "level_9_3" : "level_9_?")); }
/* a signature-only DXBC (header + one ISGN chunk + fresh checksum), exactly what Unity's CreateInputLayout path builds; returns the size or 0 */
static unsigned build_sig_dxbc(const u8 *vs, unsigned vsn, u8 *out, unsigned cap)
{
    unsigned l = 0, total; const u8 *isgn = dx_chunk(vs, vsn, "ISGN", &l);
    if (!isgn) return 0;
    total = 36 + 8 + l;
    if (total > cap) return 0;
    memcpy(out, "DXBC", 4); memset(out + 4, 0, 16);
    wr32(out + 20, 1); wr32(out + 24, total); wr32(out + 28, 1); wr32(out + 32, 36);
    memcpy(out + 36, "ISGN", 4); wr32(out + 40, l); memcpy(out + 44, isgn, l);
    dx_checksum(out, total, out + 4);
    return total;
}

/* CreateVertexShader / CreatePixelShader by program type; type -1 => E_FAIL */
static HRESULT create_shader(const u8 *b, unsigned n, int type)
{
    HRESULT hr;
    if (type == 1) { ID3D11VertexShader *v = NULL; hr = ID3D11Device_CreateVertexShader(dev, b, n, NULL, &v); REL(v); return hr; }
    if (type == 0) { ID3D11PixelShader *p = NULL; hr = ID3D11Device_CreatePixelShader(dev, b, n, NULL, &p); REL(p); return hr; }
    return E_FAIL;
}

/* ---------------------------------------------------------------------------------------------------------------------
 * data folder (shipped next to the exe by make_m0probe_package.py)
 * ------------------------------------------------------------------------------------------------------------------- */
typedef struct { u8 *data; unsigned size, count; } BpTable;
static BpTable g_bp;
static int bp_load(void)
{
    WCHAR p[MAX_PATH];
    if (g_bp.data) return 1;
    wpath(p, L"data\\bp\\l9_replacements.bin");
    g_bp.data = read_file(p, &g_bp.size);
    if (!g_bp.data) { lg("  cannot read %ls", p); return 0; }
    if (g_bp.size < 12 || memcmp(g_bp.data, "L9SW", 4)) { lg("  l9_replacements.bin: bad magic"); free_file(g_bp.data); g_bp.data = NULL; return 0; }
    g_bp.count = rd32(g_bp.data + 8);
    if (12u + 24u * g_bp.count > g_bp.size) { lg("  l9_replacements.bin: truncated table"); free_file(g_bp.data); g_bp.data = NULL; return 0; }
    return 1;
}
static const u8 *bp_entry(unsigned i, unsigned *sz)
{
    const u8 *e = g_bp.data + 12 + 24 * i; unsigned off = rd32(e + 16), size = rd32(e + 20);
    if (off > g_bp.size || size > g_bp.size - off) return NULL;
    *sz = size;
    return g_bp.data + off;
}
static const u8 *bp_lookup(const u8 *ck16, unsigned *sz)
{
    unsigned i;
    for (i = 0; i < g_bp.count; ++i) if (!memcmp(g_bp.data + 12 + 24 * i, ck16, 16)) return bp_entry(i, sz);
    return NULL;
}

/* ======================================================================================================================
 * child test bookkeeping
 * ==================================================================================================================== */
static void test_begin(const char *title, const char *tkey)
{
    s_copy(g_tkey, sizeof g_tkey, tkey);
    g_pass = g_fail = g_info = g_diff = 0;
    lg("");
    lg("---- %s ----", title);
}
static void test_end(void)
{
    const char *st = g_fail ? "FAIL" : (g_pass ? "PASS" : "INFO");
    if (g_diff) lg("  => %s (%d as expected, %d unexpected, %d info; %d value(s) DIFFER from the PC measurement)", st, g_pass, g_fail, g_info, g_diff);
    else lg("  => %s (%d as expected, %d unexpected, %d info)", st, g_pass, g_fail, g_info);
    rec(K("status.%s", g_tkey), "%s", st);
    if (g_diff) rec(K("diffs.%s", g_tkey), "%d", g_diff);
}

/* ======================================================================================================================
 * T1 system (runs in the parent)
 * ==================================================================================================================== */
static const char *arch_text(unsigned a)
{
    switch (a) { case 0: return "x86"; case 5: return "ARM (32-bit)"; case 9: return "x64"; case 12: return "ARM64"; default: return "other"; }
}
static void t_sys(void)
{
    typedef LONG (WINAPI *PFN_RtlGetVersion)(OSVERSIONINFOEXW *);
    typedef void (WINAPI *PFN_GetNativeSystemInfo)(SYSTEM_INFO *);
    OSVERSIONINFOEXW v; SYSTEM_INFO si; MEMORYSTATUSEX ms; SYSTEMTIME st; char b1[200], b2[200], b3[200]; WCHAR wsys[MAX_PATH];
    HMODULE nt = GetModuleHandleW(L"ntdll.dll"), k32 = GetModuleHandleW(L"kernel32.dll");
    PFN_RtlGetVersion rgv = nt ? (PFN_RtlGetVersion)GetProcAddress(nt, "RtlGetVersion") : NULL;
    PFN_GetNativeSystemInfo gnsi = k32 ? (PFN_GetNativeSystemInfo)GetProcAddress(k32, "GetNativeSystemInfo") : NULL;
    lg("---- T1 system ----");
    lg("  %s (%s build, %s %s)", PROBE_VERSION, BUILD_NAME, __DATE__, __TIME__);
    lg("  exe: %ls", g_exe);
    GetLocalTime(&st);
    lg("  started: %04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    if (rgv && rgv(&v) == 0) {
        lg("  OS (RtlGetVersion): %u.%u build %u, platform %u, product type %u, suite 0x%x, service pack \"%ls\"", (unsigned)v.dwMajorVersion, (unsigned)v.dwMinorVersion,
           (unsigned)v.dwBuildNumber, (unsigned)v.dwPlatformId, (unsigned)v.wProductType, (unsigned)v.wSuiteMask, v.szCSDVersion);
        rec("sys.os", "%u.%u.%u", (unsigned)v.dwMajorVersion, (unsigned)v.dwMinorVersion, (unsigned)v.dwBuildNumber);
    } else lg("  OS: RtlGetVersion unavailable");
    memset(&si, 0, sizeof si);
    if (gnsi) gnsi(&si); else GetSystemInfo(&si);
    lg("  processor architecture: %s (%u), processors: %u, level %u, revision 0x%04x, page size %u",
       arch_text(si.wProcessorArchitecture), (unsigned)si.wProcessorArchitecture, (unsigned)si.dwNumberOfProcessors, (unsigned)si.wProcessorLevel,
       (unsigned)si.wProcessorRevision, (unsigned)si.dwPageSize);
    rec("sys.arch", "%s", arch_text(si.wProcessorArchitecture));
    rec("sys.cpus", "%u", (unsigned)si.dwNumberOfProcessors);
    memset(&ms, 0, sizeof ms); ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) lg("  RAM: %u MiB total, %u MiB free; address space %u MiB", (unsigned)(ms.ullTotalPhys >> 20), (unsigned)(ms.ullAvailPhys >> 20), (unsigned)(ms.ullTotalVirtual >> 20));
    if (reg_text(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString", b1, sizeof b1)) {
        reg_text(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"Identifier", b2, sizeof b2);
        reg_text(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz", b3, sizeof b3);
        lg("  CPU: \"%s\" identifier \"%s\" %s MHz", b1, b2, b3);
    }
    if (reg_text(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemManufacturer", b1, sizeof b1)) {
        reg_text(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemProductName", b2, sizeof b2);
        lg("  machine: %s / %s", b1, b2);
    }
    GetSystemDirectoryW(wsys, MAX_PATH);
    lg("  system directory: %ls", wsys);
    lg("  console/log files: m0probe.log and m0probe.res next to the exe");
}

/* ======================================================================================================================
 * T2 adapters
 * ==================================================================================================================== */
static void list_sys_files(const WCHAR *sysdir, const WCHAR *sub, const WCHAR *pattern, int want_dirs, const char *filter)
{
    WCHAR p[MAX_PATH]; int n, i, shown = 0;
    w_copy(p, MAX_PATH, sysdir); w_cat(p, MAX_PATH, sub); w_cat(p, MAX_PATH, pattern);
    n = list_dir(p, want_dirs);
    for (i = 0; i < n && shown < 30; ++i) {
        if (filter && !w_icontains_a(g_names[i], filter)) continue;
        lg("      %ls%ls%ls", sub, g_names[i], want_dirs ? L"\\" : L"");
        ++shown;
    }
    if (!shown) lg("      (none)");
}
static void report_display_drivers(void)
{
    static const char *const vals[] = { "DriverDesc", "ProviderName", "DriverVersion", "DriverDate", "InstalledDisplayDrivers", "UserModeDriverName", "UserModeDriverNameWow", "MatchingDeviceId" };
    int i, j, found = 0;
    for (i = 0; i < 8; ++i) {
        char k8[200], val[600]; WCHAR key[200], wv[64];
        m_snprintf(k8, sizeof k8, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\%04u", i);
        a2w(key, 200, k8);
        if (!reg_text(key, L"DriverDesc", val, sizeof val) && !reg_text(key, L"ProviderName", val, sizeof val)) continue;
        found++;
        lg("    display class key %04u:", i);
        for (j = 0; j < (int)(sizeof vals / sizeof vals[0]); ++j) {
            a2w(wv, 64, vals[j]);
            if (reg_text(key, wv, val, sizeof val)) lg("      %-24s %.230s%s", vals[j], val, s_len(val) > 230 ? " ..." : "");
        }
    }
    if (!found) lg("    (no display class keys readable)");
}
static void t_adapters(void)
{
    IDXGIFactory1 *fac = NULL; HRESULT hr; unsigned i, nhw = 0, nsoft = 0; WCHAR sysdir[MAX_PATH], p[MAX_PATH]; int firsthw = -1;
    char firstname[160]; firstname[0] = 0;
    test_begin("T2 adapters (DXGI EnumAdapters1) + driver files", "adapters");
    if (!load_d3d()) { g_fail++; lg("  cannot load d3d11/dxgi"); return; }
    hr = pCreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&fac);
    lg("  CreateDXGIFactory1 hr=0x%08x", (unsigned)hr);
    if (FAILED(hr) || !fac) { g_fail++; return; }
    for (i = 0; i < 16; ++i) {
        IDXGIAdapter1 *a = NULL; DXGI_ADAPTER_DESC1 d; LARGE_INTEGER umd; IDXGIOutput *o = NULL; unsigned outs = 0; int cls; char name[200]; HRESULT h2;
        hr = IDXGIFactory1_EnumAdapters1(fac, i, &a);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr) || !a) { lg("  EnumAdapters1(%u) hr=0x%08x", i, (unsigned)hr); break; }
        memset(&d, 0, sizeof d);
        hr = IDXGIAdapter1_GetDesc1(a, &d);
        if (FAILED(hr)) { lg("  adapter %u: GetDesc1 hr=0x%08x", i, (unsigned)hr); REL(a); continue; }
        WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, (int)sizeof name - 1, NULL, NULL); name[sizeof name - 1] = 0;
        cls = adapter_class(d.VendorId, d.Flags, name);
        lg("  adapter %u: \"%s\"", i, name);
        lg("      VendorId=0x%04x DeviceId=0x%04x SubSysId=0x%08x Revision=%u Flags=0x%x%s LUID=%08x-%08x", d.VendorId, d.DeviceId, d.SubSysId, d.Revision, d.Flags,
           (d.Flags & 2u) ? " (DXGI_ADAPTER_FLAG_SOFTWARE)" : "", (unsigned)d.AdapterLuid.HighPart, (unsigned)d.AdapterLuid.LowPart);
        lg("      DedicatedVideoMemory=%u MiB DedicatedSystemMemory=%u MiB SharedSystemMemory=%u MiB  => class: %s", (unsigned)((u64)d.DedicatedVideoMemory >> 20),
           (unsigned)((u64)d.DedicatedSystemMemory >> 20), (unsigned)((u64)d.SharedSystemMemory >> 20), ac_text(cls));
        umd.QuadPart = 0;
        h2 = IDXGIAdapter1_CheckInterfaceSupport(a, &IID_IDXGIDevice, &umd);
        lg("      CheckInterfaceSupport(IDXGIDevice) hr=0x%08x user-mode driver version %u.%u.%u.%u", (unsigned)h2, (unsigned)((u32)umd.HighPart >> 16), (unsigned)((u32)umd.HighPart & 0xffff),
           (unsigned)((u32)umd.LowPart >> 16), (unsigned)((u32)umd.LowPart & 0xffff));
        while (outs < 8 && SUCCEEDED(IDXGIAdapter1_EnumOutputs(a, outs, &o))) { REL(o); ++outs; }
        lg("      outputs (monitors): %u", outs);
        rec(K("adapter.%u.name", i), "%s", name);
        rec(K("adapter.%u.vendor", i), "0x%04x", d.VendorId);
        rec(K("adapter.%u.class", i), "%d", cls);
        rec(K("adapter.%u.umd", i), "%d", SUCCEEDED(h2));
        if (cls == AC_SOFT) nsoft++; else { nhw++; if (firsthw < 0) { firsthw = (int)i; s_copy(firstname, sizeof firstname, name); } }
        REL(a);
    }
    REL(fac);
    lg("  adapters found: %u (%u hardware, %u software)", i, nhw, nsoft);
    rec("adapters.count", "%u", i);
    rec("adapters.hw", "%u", nhw);
    rec("adapters.soft", "%u", nsoft);
    rec("adapters.firsthw", "%s", firstname);
    if (i) g_pass++; else g_fail++;

    lg("  -- software rasteriser and driver files --");
    GetSystemDirectoryW(sysdir, MAX_PATH);
    {
        static const WCHAR *const files[] = { L"\\d3d10warp.dll", L"\\d3d11.dll", L"\\dxgi.dll", L"\\d3d9.dll", L"\\d3dcompiler_47.dll", L"\\d3dcompiler_43.dll", L"\\version.dll", L"\\ntdll.dll" };
        int k;
        for (k = 0; k < (int)(sizeof files / sizeof files[0]); ++k) {
            w_copy(p, MAX_PATH, sysdir); w_cat(p, MAX_PATH, files[k]);
            if (file_exists(p)) lg("    %-24ls present, file version %s", files[k] + 1, file_version(p));
            else lg("    %-24ls MISSING", files[k] + 1);
            if (k == 0) rec("warpdll", "%d", file_exists(p) ? 1 : 0);
        }
    }
    lg("    NVIDIA-looking files in the system folder:");
    list_sys_files(sysdir, L"\\", L"nv*.dll", 0, NULL);
    list_sys_files(sysdir, L"\\drivers\\", L"nv*.sys", 0, NULL);
    lg("    DriverStore\\FileRepository folders that look like a GPU driver (nv / tegra / display / gfx / graphics / video):");
    {
        WCHAR pat[MAX_PATH]; int n, k, shown = 0;
        w_copy(pat, MAX_PATH, sysdir); w_cat(pat, MAX_PATH, L"\\DriverStore\\FileRepository\\*");
        n = list_dir(pat, 1);
        for (k = 0; k < n && shown < 30; ++k)
            if (w_icontains_a(g_names[k], "nv") || w_icontains_a(g_names[k], "tegra") || w_icontains_a(g_names[k], "display") || w_icontains_a(g_names[k], "gfx") ||
                w_icontains_a(g_names[k], "graphics") || w_icontains_a(g_names[k], "video")) { lg("      %ls\\", g_names[k]); ++shown; }
        if (!shown) lg("      (none%s)", n ? "" : " - folder not readable");
    }
    lg("    registry display class (Control\\Class\\{4d36e968-...}):");
    report_display_drivers();
    test_end();
}

/* ======================================================================================================================
 * T3 feature levels
 * ==================================================================================================================== */
static void fl_try(const char *label, const char *tag, IDXGIAdapter *ad, D3D_DRIVER_TYPE dt, UINT flags, const D3D_FEATURE_LEVEL *lv, UINT n, int retry_without_111, int first_report, int expect_ok)
{
    HRESULT hr; int retried = 0;
    hr = make_dev(ad, dt, flags, lv, n);
    if (hr == E_INVALIDARG && retry_without_111 && lv[0] == D3D_FEATURE_LEVEL_11_1) { retried = 1; hr = make_dev(ad, dt, flags, lv + 1, n - 1); }
    lg("  %-52s hr=0x%08x%s", label, (unsigned)hr, retried ? " (E_INVALIDARG with 11_1 -> retried without it)" : "");
    rec(K("fl.%s.hr", tag), "0x%08x", (unsigned)hr);
    if (expect_ok) { if (SUCCEEDED(hr)) g_pass++; else g_fail++; } else g_info++;
    if (FAILED(hr)) { rec(K("fl.%s.level", tag), "0"); return; }
    {
        char name[160]; unsigned v, d, f; int cls = dev_adapter(dev, name, (int)sizeof name, &v, &d, &f);
        lg("      chosen feature level: %s (0x%x); ID3D11Device::GetFeatureLevel = 0x%x; creation flags 0x%x; device removed reason 0x%08x", fl_name((unsigned)g_fl), (unsigned)g_fl,
           (unsigned)ID3D11Device_GetFeatureLevel(dev), (unsigned)ID3D11Device_GetCreationFlags(dev), (unsigned)ID3D11Device_GetDeviceRemovedReason(dev));
        lg("      runs on adapter \"%s\" vendor=%04x device=%04x flags=0x%x -> %s", name, v, d, f, cls ? ac_text(cls) : "?");
        rec(K("fl.%s.level", tag), "0x%x", (unsigned)g_fl);
        rec(K("fl.%s.adapter", tag), "%s", name);
        rec(K("fl.%s.class", tag), "%d", cls);
    }
    dev_features(tag);
    mods_report(tag, first_report);
    drop_dev();
}
static IDXGIFactory1 *get_factory(void)
{
    IDXGIFactory1 *f = NULL; HRESULT hr = pCreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&f);
    if (FAILED(hr)) { lg("  CreateDXGIFactory1 hr=0x%08x", (unsigned)hr); return NULL; }
    return f;
}
static void t_fl(const char *cas)
{
    UINT bgra = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    char title[96], key[48];
    m_snprintf(title, sizeof title, "T3 feature levels [%s]", cas); m_snprintf(key, sizeof key, "fl.%s", cas);
    test_begin(title, key);
    if (!load_d3d()) { g_fail++; lg("  cannot load d3d11/dxgi"); return; }
    mods_baseline();
    if (s_eq(cas, "hw")) {
        fl_try("(a) HARDWARE, adapter NULL, [11_1 .. 9_1]", "hw", NULL, D3D_DRIVER_TYPE_HARDWARE, bgra, k_lv_all, 7, 1, 1, 1);
    } else if (s_eq(cas, "warp")) {
        fl_try("(c) WARP, adapter NULL, [11_1 .. 9_1]", "warp", NULL, D3D_DRIVER_TYPE_WARP, bgra, k_lv_all, 7, 1, 1, 1);
    } else if (cas[0] == 'a' && cas[1] >= '0' && cas[1] <= '9') {
        unsigned idx = (unsigned)(cas[1] - '0'); IDXGIFactory1 *f = get_factory(); IDXGIAdapter1 *a = NULL; DXGI_ADAPTER_DESC1 d; char name[160], label[200];
        if (!f) { g_fail++; return; }
        if (FAILED(IDXGIFactory1_EnumAdapters1(f, idx, &a)) || !a) { lg("  adapter %u does not exist", idx); REL(f); return; }
        memset(&d, 0, sizeof d); IDXGIAdapter1_GetDesc1(a, &d);
        WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, (int)sizeof name - 1, NULL, NULL); name[sizeof name - 1] = 0;
        m_snprintf(label, sizeof label, "(b) adapter %u \"%.30s\", UNKNOWN, [11_1 .. 9_1]", idx, name);
        fl_try(label, cas, (IDXGIAdapter *)a, D3D_DRIVER_TYPE_UNKNOWN, bgra, k_lv_all, 7, 1, 0, 1);
        REL(a); REL(f);
    } else if (s_eq(cas, "unity")) {
        /* Unity 2018.4 sub_ffdc74: IDXGIFactory::EnumAdapters(0), DriverType UNKNOWN, levels [11_1,11_0,10_1,10_0], flags 0x21 (SINGLETHREADED|BGRA) */
        IDXGIFactory1 *f = get_factory(); IDXGIAdapter *a = NULL;
        if (!f) { g_fail++; return; }
        if (FAILED(IDXGIFactory1_EnumAdapters(f, 0, &a)) || !a) { lg("  EnumAdapters(0) failed"); REL(f); g_fail++; return; }
        fl_try("Unity's first call: EnumAdapters(0), UNKNOWN, [11_1..10_0], 0x21", "unity", a, D3D_DRIVER_TYPE_UNKNOWN, 0x21, k_lv_all, 4, 0, 0, 0);
        REL(a); REL(f);
    } else if (s_eq(cas, "hwsingle") || s_eq(cas, "warpsingle")) {
        int warp = s_eq(cas, "warpsingle"), k;
        for (k = 0; k < 7; ++k) {
            HRESULT hr = make_dev(NULL, warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, bgra, &k_lv_all[k], 1);
            lg("  %s with ONLY feature level %s requested: hr=0x%08x chosen=%s", warp ? "WARP    " : "HARDWARE", fl_name((unsigned)k_lv_all[k]), (unsigned)hr, SUCCEEDED(hr) ? fl_name((unsigned)g_fl) : "-");
            rec(K("fl.%s.only_%s", warp ? "warp" : "hw", fl_name((unsigned)k_lv_all[k])), "%d", SUCCEEDED(hr));
            if (SUCCEEDED(hr)) drop_dev();
        }
    } else { lg("  unknown case %s", cas); g_fail++; }
    test_end();
}

/* ======================================================================================================================
 * T4 shaders at FL 9_1 (device created with ONLY {9_1}, exactly what the Win32Craft host does)
 * ==================================================================================================================== */
typedef struct { char name[72]; int used; char level[8]; char label[56]; } GdIdx;
static GdIdx g_gdidx[96]; static int g_ngdidx;
static void gd_index_load(void)
{
    WCHAR p[MAX_PATH]; unsigned n = 0; u8 *b; unsigned i = 0;
    g_ngdidx = 0;
    wpath(p, L"data\\gd\\index.txt");
    b = read_file(p, &n);
    if (!b) return;
    while (i < n && g_ngdidx < 96) {
        char line[200]; unsigned l = 0; GdIdx *e; char *t1, *t2, *t3;
        while (i < n && b[i] != '\n' && l < sizeof line - 1) { if (b[i] != '\r') line[l++] = (char)b[i]; ++i; }
        while (i < n && b[i] != '\n') ++i;
        if (i < n) ++i;
        line[l] = 0;
        if (!l || line[0] == '#') continue;
        e = &g_gdidx[g_ngdidx];
        t1 = line; while (*t1 && *t1 != ' ') ++t1; if (!*t1) continue; *t1++ = 0;
        t2 = t1; while (*t2 && *t2 != ' ') ++t2; if (!*t2) continue; *t2++ = 0;
        t3 = t2; while (*t3 && *t3 != ' ') ++t3; if (*t3) *t3++ = 0;
        s_copy(e->name, sizeof e->name, line); e->used = (t1[0] == '1'); s_copy(e->level, sizeof e->level, t2); s_copy(e->label, sizeof e->label, t3);
        g_ngdidx++;
    }
    free_file(b);
}
static GdIdx *gd_find(const char *name)
{
    int i;
    for (i = 0; i < g_ngdidx; ++i) if (s_eq(g_gdidx[i].name, name)) return &g_gdidx[i];
    return NULL;
}

typedef struct { int tot, ok, used_tot, used_ok, a_tot, a_ok, b_tot, b_ok; } ShStat;
/* creates every *.dxbc of data\<sub> as VS or PS and prints one line each; is_gd adds the used-by-the-game flag (index.txt) and
 * splits the totals into libcocos2d (a) and libGLESv2_phone (b) */
static void shader_folder(const char *sub, int is_gd, ShStat *st, int verbose)
{
    WCHAR pat[MAX_PATH], fp[MAX_PATH]; int n, i; char rel[96];
    static WCHAR names[256][80];
    memset(st, 0, sizeof *st);
    m_snprintf(rel, sizeof rel, "data\\%s\\*.dxbc", sub);
    wpath_a(pat, rel);
    n = list_dir(pat, 0);
    if (!n) { lg("  (no files in data\\%s - run make_m0probe_package.py)", sub); g_fail++; return; }
    for (i = 0; i < n; ++i) w_copy(names[i], 80, g_names[i]);
    for (i = 0; i < n; ++i) {
        unsigned sz = 0; u8 *b; HRESULT hr; int type, lvl, used = 0; char an[96]; GdIdx *gi = NULL;
        m_snprintf(rel, sizeof rel, "data\\%s\\", sub);
        wpath_a(fp, rel); w_cat(fp, MAX_PATH, names[i]);
        w2a(an, sizeof an, names[i]);
        b = read_file(fp, &sz);
        if (!b) { lg("  [FAIL] %-40s cannot read file", an); st->tot++; continue; }
        type = dx_type(b, sz); lvl = dx_level9(b, sz);
        if (is_gd) { gi = gd_find(an); used = gi ? gi->used : 0; }
        hr = create_shader(b, sz, type);
        st->tot++; if (SUCCEEDED(hr)) st->ok++;
        if (is_gd) {
            if (used) { st->used_tot++; if (SUCCEEDED(hr)) st->used_ok++; }
            if (s_prefix(an, "libcocos2d")) { st->a_tot++; if (SUCCEEDED(hr)) st->a_ok++; } else { st->b_tot++; if (SUCCEEDED(hr)) st->b_ok++; }
        }
        if (verbose || FAILED(hr))
        lg("  [%s] %-34s %s %-9s %5u B  hr=0x%08x%s%s%s", SUCCEEDED(hr) ? " ok " : "FAIL", an, dx_typename(type), lvl_text(lvl), sz, (unsigned)hr,
           (is_gd && used) ? "  USED by the game: " : "", (is_gd && used && gi) ? gi->label : "", (is_gd && !used) ? "  (not used)" : "");
        free_file(b);
    }
}
static void t_shaders(const char *cas)
{
    int warp = s_eq(cas, "warp"); char title[96], key[48], tag[8]; ShStat rs, gs;
    unsigned i, bp_ok = 0, bp_tot = 0, bpo_tot = 0, bpo_fail = 0, nfail_print = 0;
    m_snprintf(title, sizeof title, "T4 shaders at FL 9_1 on %s {9_1 only}", warp ? "WARP" : "HARDWARE");
    m_snprintf(key, sizeof key, "shaders.%s", cas); s_copy(tag, sizeof tag, cas);
    test_begin(title, key);
    if (!load_d3d()) { g_fail++; lg("  cannot load d3d11/dxgi"); return; }
    mods_baseline();
    if (!open_dev(warp, 1, tag)) { g_fail++; test_end(); return; }
    mods_report(tag, 0);
    gd_index_load();

    lg("  -- Redcon: data\\redcon (10 blobs with level-9 payloads; expected ALL ok)%s --", warp ? " [WARP run: only failures are listed]" : "");
    shader_folder("redcon", 0, &rs, !warp);
    lg("  Redcon shaders created at 9_1: %d/%d", rs.ok, rs.tot);
    rec(K("sh.%s.redcon", tag), "%d/%d", rs.ok, rs.tot);
    if (rs.tot && rs.ok == rs.tot) g_pass++; else g_fail++;

    lg("  -- Geometry Dash: data\\gd (libcocos2d.dll: 30 level_9_3 containers = 15 programs, the game uses 9 programs = 18 containers; libGLESv2_phone.dll: 10 ANGLE internals) --");
    shader_folder("gd", 1, &gs, !warp);
    lg("  Geometry Dash level_9_3 containers (libcocos2d.dll) created at 9_1: %d/%d", gs.a_ok, gs.a_tot);
    lg("  ... of which the 18 containers of the 9 programs the game USES:      %d/%d", gs.used_ok, gs.used_tot);
    lg("  ANGLE internal shaders (libGLESv2_phone.dll) created at 9_1:         %d/%d", gs.b_ok, gs.b_tot);
    rec(K("sh.%s.gd", tag), "%d/%d", gs.a_ok, gs.a_tot);
    rec(K("sh.%s.gdused", tag), "%d/%d", gs.used_ok, gs.used_tot);
    rec(K("sh.%s.gdangle", tag), "%d/%d", gs.b_ok, gs.b_tot);
    g_info++;

    lg("  -- Bad Piggies: data\\bp\\l9_replacements.bin (113 level_9_1 replacements; expected ALL ok) --");
    if (bp_load()) {
        for (i = 0; i < g_bp.count; ++i) {
            unsigned sz = 0; const u8 *e = bp_entry(i, &sz); HRESULT hr; int type;
            bp_tot++;
            if (!e) { if (nfail_print++ < 40) lg("  [FAIL] entry %u: bad offset in table", i); continue; }
            type = dx_type(e, sz); hr = create_shader(e, sz, type);
            if (SUCCEEDED(hr)) bp_ok++;
            else if (nfail_print++ < 40) lg("  [FAIL] entry %u %s %u B hr=0x%08x", i, dx_typename(type), sz, (unsigned)hr);
        }
        lg("  Bad Piggies replacements created at 9_1: %u/%u", bp_ok, bp_tot);
        rec(K("sh.%s.bp", tag), "%u/%u", bp_ok, bp_tot);
        if (bp_tot && bp_ok == bp_tot) g_pass++; else g_fail++;
        {   /* the Unity originals (plain vs_4_0/ps_4_0, no Aon9 payload) must be REFUSED by a 9_1-only device */
            WCHAR pat[MAX_PATH]; static WCHAR names[64][80]; int n, k;
            wpath(pat, L"data\\bp\\orig\\*.dxbc"); n = list_dir(pat, 0); if (n > 64) n = 64;
            for (k = 0; k < n; ++k) w_copy(names[k], 80, g_names[k]);
            for (k = 0; k < n; ++k) {
                WCHAR fp[MAX_PATH]; unsigned sz = 0; u8 *b; char an[96]; int type, lvl; HRESULT hr;
                wpath(fp, L"data\\bp\\orig\\"); w_cat(fp, MAX_PATH, names[k]); w2a(an, sizeof an, names[k]);
                b = read_file(fp, &sz); if (!b) continue;
                type = dx_type(b, sz); lvl = dx_level9(b, sz); hr = create_shader(b, sz, type);
                bpo_tot++; if (FAILED(hr)) bpo_fail++;
                lg("  [%s] Unity original %-46s %s %-9s hr=0x%08x (expected: refused)", FAILED(hr) ? " ok " : "FAIL", an, dx_typename(type), lvl_text(lvl), (unsigned)hr);
                free_file(b);
            }
            lg("  Bad Piggies Unity originals refused at 9_1: %u/%u", bpo_fail, bpo_tot);
            rec(K("sh.%s.bporig", tag), "%u/%u", bpo_fail, bpo_tot);
            if (bpo_tot && bpo_fail == bpo_tot) g_pass++; else g_fail++;
        }
    } else g_fail++;
    rep("GetDeviceRemovedReason after all shader creations", ID3D11Device_GetDeviceRemovedReason(dev), 1);
    drop_dev();
    test_end();
}

/* ======================================================================================================================
 * embedded test shader pair (HLSL, compiled with fxc to vs_4_0_level_9_1 / ps_4_0_level_9_1; used by the blend and speed tests):
 *
 *   cbuffer CB : register(b0) { float4 xform; float4 color; };            // xform = scale.xy, offset.zw in clip space
 *   Texture2D<float4> t0 : register(t0);  SamplerState s0 : register(s0);
 *   struct VI { float2 p : POSITION; float2 uv : TEXCOORD0; };
 *   struct VO { float4 p : SV_Position; float2 uv : TEXCOORD0; float4 c : TEXCOORD1; };
 *   VO vs(VI i) { VO o; o.p = float4(i.p * xform.xy + xform.zw, 0.5, 1.0); o.uv = i.uv; o.c = color; return o; }
 *   float4 ps(VO i) : SV_Target { return t0.Sample(s0, i.uv) * i.c; }
 * ==================================================================================================================== */
static const unsigned char k_quad_vs[1000] = {
    0x44,0x58,0x42,0x43,0xb8,0x4c,0xaf,0xd6,0x33,0x55,0xe5,0xf8,0x16,0x5e,0x0d,0x44,
    0x0d,0x86,0x4d,0x38,0x01,0x00,0x00,0x00,0xe8,0x03,0x00,0x00,0x06,0x00,0x00,0x00,
    0x38,0x00,0x00,0x00,0xf4,0x00,0x00,0x00,0xd0,0x01,0x00,0x00,0x4c,0x02,0x00,0x00,
    0x24,0x03,0x00,0x00,0x78,0x03,0x00,0x00,0x41,0x6f,0x6e,0x39,0xb4,0x00,0x00,0x00,
    0xb4,0x00,0x00,0x00,0x00,0x02,0xfe,0xff,0x80,0x00,0x00,0x00,0x34,0x00,0x00,0x00,
    0x01,0x00,0x24,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x24,0x00,
    0x01,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x02,0xfe,0xff,0x51,0x00,0x00,0x05,0x03,0x00,0x0f,0xa0,
    0x00,0x00,0x00,0x3f,0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x1f,0x00,0x00,0x02,0x05,0x00,0x00,0x80,0x00,0x00,0x0f,0x90,0x1f,0x00,0x00,0x02,
    0x05,0x00,0x01,0x80,0x01,0x00,0x0f,0x90,0x04,0x00,0x00,0x04,0x00,0x00,0x03,0x80,
    0x00,0x00,0xe4,0x90,0x01,0x00,0xe4,0xa0,0x01,0x00,0xee,0xa0,0x02,0x00,0x00,0x03,
    0x00,0x00,0x03,0xc0,0x00,0x00,0xe4,0x80,0x00,0x00,0xe4,0xa0,0x01,0x00,0x00,0x02,
    0x00,0x00,0x0c,0xc0,0x03,0x00,0x44,0xa0,0x01,0x00,0x00,0x02,0x00,0x00,0x03,0xe0,
    0x01,0x00,0xe4,0x90,0x01,0x00,0x00,0x02,0x01,0x00,0x0f,0xe0,0x02,0x00,0xe4,0xa0,
    0xff,0xff,0x00,0x00,0x53,0x48,0x44,0x52,0xd4,0x00,0x00,0x00,0x40,0x00,0x01,0x00,
    0x35,0x00,0x00,0x00,0x59,0x00,0x00,0x04,0x46,0x8e,0x20,0x00,0x00,0x00,0x00,0x00,
    0x02,0x00,0x00,0x00,0x5f,0x00,0x00,0x03,0x32,0x10,0x10,0x00,0x00,0x00,0x00,0x00,
    0x5f,0x00,0x00,0x03,0x32,0x10,0x10,0x00,0x01,0x00,0x00,0x00,0x67,0x00,0x00,0x04,
    0xf2,0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x65,0x00,0x00,0x03,
    0x32,0x20,0x10,0x00,0x01,0x00,0x00,0x00,0x65,0x00,0x00,0x03,0xf2,0x20,0x10,0x00,
    0x02,0x00,0x00,0x00,0x32,0x00,0x00,0x0b,0x32,0x20,0x10,0x00,0x00,0x00,0x00,0x00,
    0x46,0x10,0x10,0x00,0x00,0x00,0x00,0x00,0x46,0x80,0x20,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xe6,0x8a,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x36,0x00,0x00,0x08,0xc2,0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x02,0x40,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0x00,0x00,0x80,0x3f,
    0x36,0x00,0x00,0x05,0x32,0x20,0x10,0x00,0x01,0x00,0x00,0x00,0x46,0x10,0x10,0x00,
    0x01,0x00,0x00,0x00,0x36,0x00,0x00,0x06,0xf2,0x20,0x10,0x00,0x02,0x00,0x00,0x00,
    0x46,0x8e,0x20,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x3e,0x00,0x00,0x01,
    0x53,0x54,0x41,0x54,0x74,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x52,0x44,0x45,0x46,
    0xd0,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x1c,0x00,0x00,0x00,0x00,0x04,0xfe,0xff,0x00,0x01,0x00,0x00,0xa6,0x00,0x00,0x00,
    0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x43,0x42,0x00,0xab,0x3c,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x58,0x00,0x00,0x00,
    0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x90,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xa0,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x10,0x00,0x00,0x00,
    0x02,0x00,0x00,0x00,0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0x66,0x6f,0x72,
    0x6d,0x00,0xab,0xab,0x01,0x00,0x03,0x00,0x01,0x00,0x04,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x63,0x6f,0x6c,0x6f,0x72,0x00,0x4d,0x69,0x63,0x72,0x6f,0x73,
    0x6f,0x66,0x74,0x20,0x28,0x52,0x29,0x20,0x48,0x4c,0x53,0x4c,0x20,0x53,0x68,0x61,
    0x64,0x65,0x72,0x20,0x43,0x6f,0x6d,0x70,0x69,0x6c,0x65,0x72,0x20,0x31,0x30,0x2e,
    0x31,0x00,0xab,0xab,0x49,0x53,0x47,0x4e,0x4c,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
    0x08,0x00,0x00,0x00,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x00,0x00,0x41,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x03,0x03,0x00,0x00,0x50,0x4f,0x53,0x49,0x54,0x49,0x4f,0x4e,0x00,0x54,0x45,0x58,
    0x43,0x4f,0x4f,0x52,0x44,0x00,0xab,0xab,0x4f,0x53,0x47,0x4e,0x68,0x00,0x00,0x00,
    0x03,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x00,0x00,0x00,
    0x5c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
    0x01,0x00,0x00,0x00,0x03,0x0c,0x00,0x00,0x5c,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x0f,0x00,0x00,0x00,
    0x53,0x56,0x5f,0x50,0x6f,0x73,0x69,0x74,0x69,0x6f,0x6e,0x00,0x54,0x45,0x58,0x43,
    0x4f,0x4f,0x52,0x44,0x00,0xab,0xab,0xab,
};

static const unsigned char k_quad_ps[784] = {
    0x44,0x58,0x42,0x43,0xa1,0xac,0xde,0x34,0x76,0x2e,0xe9,0x3a,0x7b,0x35,0x21,0xde,
    0x3c,0x0f,0x5a,0xb9,0x01,0x00,0x00,0x00,0x10,0x03,0x00,0x00,0x06,0x00,0x00,0x00,
    0x38,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0x5c,0x01,0x00,0x00,0xd8,0x01,0x00,0x00,
    0x6c,0x02,0x00,0x00,0xdc,0x02,0x00,0x00,0x41,0x6f,0x6e,0x39,0x80,0x00,0x00,0x00,
    0x80,0x00,0x00,0x00,0x00,0x02,0xff,0xff,0x58,0x00,0x00,0x00,0x28,0x00,0x00,0x00,
    0x00,0x00,0x28,0x00,0x00,0x00,0x28,0x00,0x00,0x00,0x28,0x00,0x01,0x00,0x24,0x00,
    0x00,0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0xff,0x1f,0x00,0x00,0x02,
    0x00,0x00,0x00,0x80,0x00,0x00,0x03,0xb0,0x1f,0x00,0x00,0x02,0x00,0x00,0x00,0x80,
    0x01,0x00,0x0f,0xb0,0x1f,0x00,0x00,0x02,0x00,0x00,0x00,0x90,0x00,0x08,0x0f,0xa0,
    0x42,0x00,0x00,0x03,0x00,0x00,0x0f,0x80,0x00,0x00,0xe4,0xb0,0x00,0x08,0xe4,0xa0,
    0x05,0x00,0x00,0x03,0x00,0x00,0x0f,0x80,0x00,0x00,0xe4,0x80,0x01,0x00,0xe4,0xb0,
    0x01,0x00,0x00,0x02,0x00,0x08,0x0f,0x80,0x00,0x00,0xe4,0x80,0xff,0xff,0x00,0x00,
    0x53,0x48,0x44,0x52,0x94,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x25,0x00,0x00,0x00,
    0x5a,0x00,0x00,0x03,0x00,0x60,0x10,0x00,0x00,0x00,0x00,0x00,0x58,0x18,0x00,0x04,
    0x00,0x70,0x10,0x00,0x00,0x00,0x00,0x00,0x55,0x55,0x00,0x00,0x62,0x10,0x00,0x03,
    0x32,0x10,0x10,0x00,0x01,0x00,0x00,0x00,0x62,0x10,0x00,0x03,0xf2,0x10,0x10,0x00,
    0x02,0x00,0x00,0x00,0x65,0x00,0x00,0x03,0xf2,0x20,0x10,0x00,0x00,0x00,0x00,0x00,
    0x68,0x00,0x00,0x02,0x01,0x00,0x00,0x00,0x45,0x00,0x00,0x09,0xf2,0x00,0x10,0x00,
    0x00,0x00,0x00,0x00,0x46,0x10,0x10,0x00,0x01,0x00,0x00,0x00,0x46,0x7e,0x10,0x00,
    0x00,0x00,0x00,0x00,0x00,0x60,0x10,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x00,0x07,
    0xf2,0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x46,0x0e,0x10,0x00,0x00,0x00,0x00,0x00,
    0x46,0x1e,0x10,0x00,0x02,0x00,0x00,0x00,0x3e,0x00,0x00,0x01,0x53,0x54,0x41,0x54,
    0x74,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x52,0x44,0x45,0x46,0x8c,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x1c,0x00,0x00,0x00,
    0x00,0x04,0xff,0xff,0x00,0x01,0x00,0x00,0x62,0x00,0x00,0x00,0x5c,0x00,0x00,0x00,
    0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x5f,0x00,0x00,0x00,
    0x02,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
    0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x73,0x30,0x00,0x74,
    0x30,0x00,0x4d,0x69,0x63,0x72,0x6f,0x73,0x6f,0x66,0x74,0x20,0x28,0x52,0x29,0x20,
    0x48,0x4c,0x53,0x4c,0x20,0x53,0x68,0x61,0x64,0x65,0x72,0x20,0x43,0x6f,0x6d,0x70,
    0x69,0x6c,0x65,0x72,0x20,0x31,0x30,0x2e,0x31,0x00,0xab,0xab,0x49,0x53,0x47,0x4e,
    0x68,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x50,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0f,0x00,0x00,0x00,0x5c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x03,0x03,0x00,0x00,0x5c,0x00,0x00,0x00,
    0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
    0x0f,0x0f,0x00,0x00,0x53,0x56,0x5f,0x50,0x6f,0x73,0x69,0x74,0x69,0x6f,0x6e,0x00,
    0x54,0x45,0x58,0x43,0x4f,0x4f,0x52,0x44,0x00,0xab,0xab,0xab,0x4f,0x53,0x47,0x4e,
    0x2c,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x20,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0f,0x00,0x00,0x00,0x53,0x56,0x5f,0x54,0x61,0x72,0x67,0x65,0x74,0x00,0xab,0xab,
};


#define M_FLT_MAX 3.402823466e+38f

/* ---------------------------------------------------------------------------------------------------------------------
 * shared render helpers
 * ------------------------------------------------------------------------------------------------------------------- */
static HRESULT tex2d(UINT w, UINT h, UINT mips, DXGI_FORMAT f, UINT bind, ID3D11Texture2D **out)
{
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D *t = NULL; HRESULT hr;
    memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = 1; d.Format = f; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    hr = ID3D11Device_CreateTexture2D(dev, &d, NULL, &t);
    if (out) *out = t; else REL(t);
    return hr;
}
static HRESULT make_buffer(UINT bytes, UINT bind, D3D11_USAGE usage, UINT cpu, const void *data, ID3D11Buffer **out)
{
    D3D11_BUFFER_DESC bd; D3D11_SUBRESOURCE_DATA sd;
    memset(&bd, 0, sizeof bd); memset(&sd, 0, sizeof sd);
    bd.ByteWidth = bytes; bd.Usage = usage; bd.BindFlags = bind; bd.CPUAccessFlags = cpu;
    sd.pSysMem = data;
    return ID3D11Device_CreateBuffer(dev, &bd, data ? &sd : NULL, out);
}
static HRESULT make_rt(UINT w, UINT h, ID3D11Texture2D **tex, ID3D11RenderTargetView **rtv, ID3D11Texture2D **staging)
{
    HRESULT hr = tex2d(w, h, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, tex);
    if (FAILED(hr)) return hr;
    hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)*tex, NULL, rtv);
    if (FAILED(hr)) return hr;
    if (staging) {
        D3D11_TEXTURE2D_DESC d;
        memset(&d, 0, sizeof d);
        d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        hr = ID3D11Device_CreateTexture2D(dev, &d, NULL, staging);
    }
    return hr;
}
static HRESULT read_px(ID3D11Texture2D *rt, ID3D11Texture2D *st, int x, int y, u32 *px)
{
    D3D11_MAPPED_SUBRESOURCE m; HRESULT hr;
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)st, (ID3D11Resource *)rt);
    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m);
    if (FAILED(hr)) return hr;
    *px = rd32((const u8 *)m.pData + (size_t)y * m.RowPitch + (size_t)x * 4);
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0);
    return S_OK;
}
static HRESULT make_blend(D3D11_BLEND sf, D3D11_BLEND df, D3D11_BLEND saf, D3D11_BLEND daf, ID3D11BlendState **out)
{
    D3D11_BLEND_DESC b;
    memset(&b, 0, sizeof b);
    b.RenderTarget[0].BlendEnable = TRUE; b.RenderTarget[0].SrcBlend = sf; b.RenderTarget[0].DestBlend = df; b.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    b.RenderTarget[0].SrcBlendAlpha = saf; b.RenderTarget[0].DestBlendAlpha = daf; b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    b.RenderTarget[0].RenderTargetWriteMask = 0xf;
    return ID3D11Device_CreateBlendState(dev, &b, out);
}
static HRESULT make_sampler(D3D11_FILTER f, D3D11_TEXTURE_ADDRESS_MODE am, UINT aniso, ID3D11SamplerState **out)
{
    D3D11_SAMPLER_DESC s;
    memset(&s, 0, sizeof s);
    s.Filter = f; s.AddressU = s.AddressV = s.AddressW = am; s.MaxAnisotropy = aniso; s.ComparisonFunc = D3D11_COMPARISON_NEVER; s.MinLOD = 0; s.MaxLOD = M_FLT_MAX;
    return ID3D11Device_CreateSamplerState(dev, &s, out);
}
static HRESULT make_raster(int scissor, ID3D11RasterizerState **out)
{
    D3D11_RASTERIZER_DESC r;
    memset(&r, 0, sizeof r);
    r.FillMode = D3D11_FILL_SOLID; r.CullMode = D3D11_CULL_NONE; r.DepthClipEnable = TRUE; r.ScissorEnable = scissor ? TRUE : FALSE;
    return ID3D11Device_CreateRasterizerState(dev, &r, out);
}

/* the textured quad kit: embedded VS/PS, layout, VB (full-screen quad), IB, dynamic + default constant buffers, sampler, rasteriser, alpha blend */
typedef struct {
    ID3D11VertexShader *vs; ID3D11PixelShader *ps; ID3D11InputLayout *il; ID3D11Buffer *vb, *ib, *cb_dyn, *cb_def;
    ID3D11SamplerState *ss; ID3D11RasterizerState *rs; ID3D11BlendState *alpha;
} Quad;
static Quad Q;
static int quad_init(void)
{
    static const float verts[16] = { -1, -1, 0, 1,   -1, 1, 0, 0,   1, 1, 1, 0,   1, -1, 1, 1 };
    static const unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };
    D3D11_INPUT_ELEMENT_DESC el[2]; HRESULT hr; int ok = 1;
    memset(&Q, 0, sizeof Q); memset(el, 0, sizeof el);
    el[0].SemanticName = "POSITION"; el[0].Format = DXGI_FORMAT_R32G32_FLOAT; el[0].AlignedByteOffset = 0; el[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    el[1].SemanticName = "TEXCOORD"; el[1].Format = DXGI_FORMAT_R32G32_FLOAT; el[1].AlignedByteOffset = 8; el[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    hr = ID3D11Device_CreateVertexShader(dev, k_quad_vs, sizeof k_quad_vs, NULL, &Q.vs); if (FAILED(hr)) { lg("  quad VS create hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = ID3D11Device_CreatePixelShader(dev, k_quad_ps, sizeof k_quad_ps, NULL, &Q.ps); if (FAILED(hr)) { lg("  quad PS create hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = ID3D11Device_CreateInputLayout(dev, el, 2, k_quad_vs, sizeof k_quad_vs, &Q.il); if (FAILED(hr)) { lg("  quad layout create hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = make_buffer(sizeof verts, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT, 0, verts, &Q.vb); if (FAILED(hr)) { lg("  quad VB hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = make_buffer(sizeof idx, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DEFAULT, 0, idx, &Q.ib); if (FAILED(hr)) { lg("  quad IB hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = make_buffer(32, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL, &Q.cb_dyn); if (FAILED(hr)) { lg("  quad dynamic CB hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = make_buffer(32, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT, 0, NULL, &Q.cb_def); if (FAILED(hr)) { lg("  quad default CB hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = make_sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, 1, &Q.ss); if (FAILED(hr)) { lg("  quad sampler hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = make_raster(0, &Q.rs); if (FAILED(hr)) { lg("  quad rasterizer hr=0x%08x", (unsigned)hr); ok = 0; }
    hr = make_blend(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, &Q.alpha); if (FAILED(hr)) { lg("  quad blend hr=0x%08x", (unsigned)hr); ok = 0; }
    return ok;
}
static void quad_free(void)
{
    REL(Q.vs); REL(Q.ps); REL(Q.il); REL(Q.vb); REL(Q.ib); REL(Q.cb_dyn); REL(Q.cb_def); REL(Q.ss); REL(Q.rs); REL(Q.alpha);
}
static void quad_bind(ID3D11ShaderResourceView *srv, UINT w, UINT h, ID3D11RenderTargetView *rtv)
{
    UINT stride = 16, off = 0; D3D11_VIEWPORT vp;
    vp.TopLeftX = 0; vp.TopLeftY = 0; vp.Width = (float)w; vp.Height = (float)h; vp.MinDepth = 0; vp.MaxDepth = 1;
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
    ID3D11DeviceContext_RSSetState(ctx, Q.rs);
    ID3D11DeviceContext_IASetInputLayout(ctx, Q.il);
    ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &Q.vb, &stride, &off);
    ID3D11DeviceContext_IASetIndexBuffer(ctx, Q.ib, DXGI_FORMAT_R16_UINT, 0);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(ctx, Q.vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(ctx, Q.ps, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv);
    ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &Q.ss);
}
/* writes xform + colour into the dynamic constant buffer (WRITE_DISCARD) */
static HRESULT cb_map(float sx, float sy, float ox, float oy, float r, float g, float b, float a)
{
    D3D11_MAPPED_SUBRESOURCE m; HRESULT hr; float *f;
    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)Q.cb_dyn, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    if (FAILED(hr)) return hr;
    f = (float *)m.pData; f[0] = sx; f[1] = sy; f[2] = ox; f[3] = oy; f[4] = r; f[5] = g; f[6] = b; f[7] = a;
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)Q.cb_dyn, 0);
    return S_OK;
}
static ID3D11ShaderResourceView *make_texture_srv(UINT w, UINT h, const u32 *texels, ID3D11Texture2D **tex)
{
    D3D11_TEXTURE2D_DESC d; D3D11_SUBRESOURCE_DATA sd; ID3D11ShaderResourceView *srv = NULL; HRESULT hr;
    memset(&d, 0, sizeof d); memset(&sd, 0, sizeof sd);
    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sd.pSysMem = texels; sd.SysMemPitch = w * 4;
    hr = ID3D11Device_CreateTexture2D(dev, &d, &sd, tex);
    if (FAILED(hr)) { lg("  texture %ux%u hr=0x%08x", w, h, (unsigned)hr); return NULL; }
    hr = ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)*tex, NULL, &srv);
    if (FAILED(hr)) { lg("  SRV hr=0x%08x", (unsigned)hr); return NULL; }
    return srv;
}

/* ======================================================================================================================
 * T5a Bad Piggies sprite draw: Unity's Sprites/Default originals -> checksum lookup -> level_9_1 replacement -> draw -> read back
 *   mode 0 "draw"  : input layout from the replacement VS (l9bench.c [F2])
 *   mode 1 "il"    : input layout from a SIGNATURE-ONLY DXBC (what Unity's CreateInputLayout path passes, crit_il.c) + 3 variants
 *   mode 2 "cboff" : same, constant buffers bound through VSSetConstantBuffers1 windows into one 64 KB buffer (crit_il.c CBOFF)
 * ==================================================================================================================== */
static void sprite_test(int mode, const char *name)
{
    WCHAR p1[MAX_PATH], p2[MAX_PATH]; unsigned n1 = 0, n2 = 0, vsz = 0, psz = 0, i; u8 *ovs, *ops; const u8 *vs, *ps; HRESULT hr;
    ID3D11VertexShader *v = NULL; ID3D11PixelShader *f = NULL; ID3D11InputLayout *il = NULL, *ilsig = NULL;
    ID3D11Buffer *vb = NULL, *ib = NULL, *cb[4] = { NULL, NULL, NULL, NULL }; ID3D11Texture2D *tex = NULL, *rt = NULL, *ds = NULL, *st = NULL;
    ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11DepthStencilView *dsv = NULL;
    ID3D11SamplerState *ss = NULL; ID3D11DepthStencilState *dss = NULL; ID3D11RasterizerState *rs = NULL;
    static u8 sig[1024]; unsigned sigsz = 0;
    D3D11_INPUT_ELEMENT_DESC el[3];
    static float g[12], dr[44], fr[92], spr[8];
    static const unsigned sizes[4] = { 48, 176, 368, 32 };
    float *data[4]; u32 texel[16]; u32 px = 0;

    if (!bp_load()) { g_fail++; return; }
    wpath(p1, L"data\\bp\\orig\\00_Sprites_Default_b000_vs_756ae2904e92.dxbc");
    wpath(p2, L"data\\bp\\orig\\00_Sprites_Default_b004_ps_3aa3dabc65f1.dxbc");
    ovs = read_file(p1, &n1); ops = read_file(p2, &n2);
    if (!ovs || !ops) { lg("  Unity originals missing in data\\bp\\orig"); g_fail++; free_file(ovs); free_file(ops); return; }
    vs = bp_lookup(ovs + 4, &vsz); ps = bp_lookup(ops + 4, &psz);
    lg("  lookup Sprites/Default VS %s, PS %s (by the checksum in the DXBC header)", vs ? "hit" : "MISS", ps ? "hit" : "MISS");
    if (!vs || !ps) { g_fail++; free_file(ovs); free_file(ops); return; }

    memset(el, 0, sizeof el);
    el[0].SemanticName = "POSITION"; el[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; el[0].AlignedByteOffset = 0; el[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    el[1].SemanticName = "COLOR"; el[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM; el[1].AlignedByteOffset = 12; el[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    el[2].SemanticName = "TEXCOORD"; el[2].Format = DXGI_FORMAT_R32G32_FLOAT; el[2].AlignedByteOffset = 16; el[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

    rep("CreateVertexShader(replacement)", ID3D11Device_CreateVertexShader(dev, vs, vsz, NULL, &v), 1);
    rep("CreatePixelShader(replacement)", ID3D11Device_CreatePixelShader(dev, ps, psz, NULL, &f), 1);
    if (mode == 0) {
        rep("CreateInputLayout(float3/Color32/float2, replacement VS bytecode)", ID3D11Device_CreateInputLayout(dev, el, 3, vs, vsz, &il), 1);
    } else {
        u8 chk[16]; ID3D11InputLayout *t = NULL;
        dx_checksum(ovs, n1, chk);
        lg("  DXBC checksum self-check on Unity's VS: %s", memcmp(chk, ovs + 4, 16) ? "MISMATCH (checksum code is wrong!)" : "matches the file's checksum");
        sigsz = build_sig_dxbc(ovs, n1, sig, sizeof sig);
        lg("  signature-only DXBC built from the VS: %u bytes (the real Unity blob is 148 for this shader)", sigsz);
        if (sigsz) rep("CreateInputLayout(signature-only blob, like Unity sub_100fa88)", ID3D11Device_CreateInputLayout(dev, el, 3, sig, sigsz, &ilsig), 1);
        else { lg("  could not build the signature blob"); g_fail++; }
        if (mode == 1) {
            rep("CreateInputLayout(original Unity vs_4_0 bytecode)", ID3D11Device_CreateInputLayout(dev, el, 3, ovs, n1, &t), 1); REL(t);
            rep("CreateInputLayout(level-9 replacement VS)", ID3D11Device_CreateInputLayout(dev, el, 3, vs, vsz, &t), 1); REL(t);
            if (sigsz) { u8 z[1024]; memcpy(z, sig, sigsz); memset(z + 4, 0, 16);
                         rep("CreateInputLayout(signature-only blob with ZERO checksum)", ID3D11Device_CreateInputLayout(dev, el, 3, z, sigsz, &t), 0); REL(t); }
        }
        il = ilsig;
        if (!il) { ID3D11Device_CreateInputLayout(dev, el, 3, vs, vsz, &il); lg("  (draw falls back to the layout from the replacement VS)"); }
    }
    if (!il || !v || !f) { lg("  cannot draw: missing shader or layout"); g_fail++; goto done; }

    {   /* geometry, constants, resources */
        struct { float x, y, z; u32 c; float u, v; } q[4] = { { -1, -1, 0.5f, 0xffffffffu, 0, 1 }, { -1, 1, 0.5f, 0xffffffffu, 0, 0 }, { 1, 1, 0.5f, 0xffffffffu, 1, 0 }, { 1, -1, 0.5f, 0xffffffffu, 1, 1 } };
        unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };
        D3D11_TEXTURE2D_DESC td; D3D11_SUBRESOURCE_DATA sd; D3D11_DEPTH_STENCIL_DESC dsd; float black[4] = { 0, 0, 0, 0 }; D3D11_VIEWPORT vp; UINT stride = 24, off = 0;
        memset(&td, 0, sizeof td); memset(&sd, 0, sizeof sd); memset(&dsd, 0, sizeof dsd);
        make_buffer(sizeof q, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT, 0, q, &vb);
        make_buffer(sizeof idx, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DEFAULT, 0, idx, &ib);
        memset(g, 0, sizeof g); memset(dr, 0, sizeof dr); memset(fr, 0, sizeof fr); memset(spr, 0, sizeof spr);
        g[8] = g[9] = g[10] = g[11] = 1;                                 /* $Globals _Color @32 */
        dr[0] = dr[5] = dr[10] = dr[15] = 1;                             /* unity_ObjectToWorld = I */
        fr[68] = fr[73] = fr[78] = fr[83] = 1;                           /* unity_MatrixVP @272 = I */
        spr[0] = spr[1] = spr[2] = spr[3] = 1; spr[4] = spr[5] = 1;      /* _RendererColor, _Flip */
        data[0] = g; data[1] = dr; data[2] = fr; data[3] = spr;
        for (i = 0; i < 4; ++i) make_buffer(sizes[i], D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT, 0, data[i], &cb[i]);
        for (i = 0; i < 16; ++i) texel[i] = 0xff3264c8u;                 /* R=200 G=100 B=50 A=255 */
        td.Width = td.Height = 4; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        sd.pSysMem = texel; sd.SysMemPitch = 16;
        ID3D11Device_CreateTexture2D(dev, &td, &sd, &tex); if (tex) ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, NULL, &srv);
        make_rt(64, 64, &rt, &rtv, &st);
        hr = tex2d(64, 64, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, &ds);
        if (ds) ID3D11Device_CreateDepthStencilView(dev, (ID3D11Resource *)ds, NULL, &dsv); else lg("  D24S8 depth texture hr=0x%08x -> drawing without depth buffer", (unsigned)hr);
        make_sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, 1, &ss);
        dsd.DepthEnable = TRUE; dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dsd.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;   /* Unity reversed Z */
        ID3D11Device_CreateDepthStencilState(dev, &dsd, &dss);
        make_raster(0, &rs);
        if (!srv || !rtv || !st || !ss) { lg("  resource creation failed"); g_fail++; goto done; }
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, black);
        if (dsv) ID3D11DeviceContext_ClearDepthStencilView(ctx, dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, dsv);
        ID3D11DeviceContext_OMSetDepthStencilState(ctx, dss, 0);
        ID3D11DeviceContext_RSSetState(ctx, rs);
        vp.TopLeftX = 0; vp.TopLeftY = 0; vp.Width = 64; vp.Height = 64; vp.MinDepth = 0; vp.MaxDepth = 1;
        ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
        ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vb, &stride, &off);
        ID3D11DeviceContext_IASetIndexBuffer(ctx, ib, DXGI_FORMAT_R16_UINT, 0);
        ID3D11DeviceContext_IASetInputLayout(ctx, il);
        ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_VSSetShader(ctx, v, NULL, 0);
        ID3D11DeviceContext_PSSetShader(ctx, f, NULL, 0);
        if (mode == 2) {
            ID3D11DeviceContext1 *c1 = NULL; D3D11_FEATURE_DATA_D3D11_OPTIONS o; ID3D11Buffer *ring = NULL; D3D11_MAPPED_SUBRESOURCE mm;
            static const UINT first[4] = { 256, 512, 768, 1024 }, num[4] = { 32, 32, 32, 32 }; ID3D11Buffer *rr[4];
            memset(&o, 0, sizeof o);
            ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof o);
            lg("  D3D11_OPTIONS.ConstantBufferOffsetting=%d (Unity's caps+0x3f9)", o.ConstantBufferOffsetting);
            hr = ID3D11DeviceContext_QueryInterface(ctx, &IID_ID3D11DeviceContext1, (void **)&c1);
            lg("  QI ID3D11DeviceContext1 hr=0x%08x", (unsigned)hr);
            if (!o.ConstantBufferOffsetting || !c1) { lg("  SKIPPED: the device does not offer constant-buffer offsetting, so Unity would use plain VSSetConstantBuffers (see the 'draw' test)"); REL(c1); g_info++; goto done; }
            hr = make_buffer(65536, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL, &ring);
            rep("CreateBuffer 64 KB dynamic constant ring", hr, 1);
            if (!ring) { REL(c1); g_fail++; goto done; }
            hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)ring, 0, D3D11_MAP_WRITE_DISCARD, 0, &mm);
            rep("Map(WRITE_DISCARD) the ring", hr, 1);
            if (SUCCEEDED(hr)) {
                memset(mm.pData, 0x7f, 65536);   /* garbage everywhere else */
                for (i = 0; i < 4; ++i) memcpy((u8 *)mm.pData + 0x1000 * (i + 1), data[i], sizes[i]);
                ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)ring, 0);
            }
            rr[0] = rr[1] = rr[2] = rr[3] = ring;
            ID3D11DeviceContext1_VSSetConstantBuffers1(c1, 0, 4, rr, first, num);
            lg("  VSSetConstantBuffers1: 4 windows into one 64 KB buffer (first constants 256/512/768/1024, 32 constants each)");
            REL(c1); REL(ring);
        } else {
            ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 4, cb);
        }
        ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv);
        ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &ss);
        ID3D11DeviceContext_DrawIndexed(ctx, 6, 0, 0);
        hr = read_px(rt, st, 32, 32, &px);
        rep("Map staging read-back", hr, 1);
        if (SUCCEEDED(hr)) {
            int r = (int)(px & 0xff), gg = (int)((px >> 8) & 0xff), b = (int)((px >> 16) & 0xff), a = (int)(px >> 24); int good;
            lg("  read-back centre pixel RGBA = %d %d %d %d (expected 200 100 50 255)", r, gg, b, a);
            good = (r >= 198 && r <= 202 && gg >= 98 && gg <= 102 && b >= 48 && b <= 52 && a == 255);
            if (good) g_pass++; else g_fail++;
            rec(K("render.%s.pixel", name), "%d %d %d %d", r, gg, b, a);
            rec(K("render.%s.ok", name), "%d", good);
        }
        rep("GetDeviceRemovedReason after the draw", ID3D11Device_GetDeviceRemovedReason(dev), 1);
    }
done:
    REL(v); REL(f); REL(il); if (ilsig != il) REL(ilsig); REL(vb); REL(ib); for (i = 0; i < 4; ++i) REL(cb[i]);
    REL(srv); REL(tex); REL(rtv); REL(rt); REL(dsv); REL(ds); REL(st); REL(ss); REL(dss); REL(rs);
    free_file(ovs); free_file(ops);
}

/* ======================================================================================================================
 * T5b Redcon fixed render-state set (redcon-render F5/F7) + functional blend check with the embedded shaders
 * ==================================================================================================================== */
static int to8(float v) { if (v < 0) v = 0; if (v > 1) v = 1; return (int)(v * 255.0f + 0.5f); }
static void blend_expected(int mode, const float s[4], const float d[4], int out[4])
{
    int c; float sa = s[3];
    for (c = 0; c < 3; ++c) {
        float sf, df;
        switch (mode) {
        case 0: sf = 1; df = 0; break;               /* (ONE, ZERO)              opaque */
        case 1: sf = 1; df = 1; break;               /* (ONE, ONE)               additive */
        case 2: sf = sa; df = 1 - sa; break;         /* (SRC_ALPHA, INV_SRC_ALPHA) alpha */
        case 3: sf = 1; df = 1 - sa; break;          /* (ONE, INV_SRC_ALPHA)     premultiplied */
        default: sf = 0; df = s[c]; break;           /* (ZERO, SRC_COLOR)        multiply */
        }
        out[c] = to8(s[c] * sf + d[c] * df);
    }
    out[3] = to8(sa);                                /* alpha factors are fixed ONE/ZERO/ADD ("separate alpha") */
}
static void render_blend(void)
{
    static const D3D11_BLEND sfs[5] = { D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_ZERO };
    static const D3D11_BLEND dfs[5] = { D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_SRC_COLOR };
    static const char *const names[5] = { "(ONE,ZERO) opaque", "(ONE,ONE) additive", "(SRC_ALPHA,INV_SRC_ALPHA) alpha", "(ONE,INV_SRC_ALPHA) premultiplied", "(ZERO,SRC_COLOR) multiply" };
    ID3D11BlendState *bs[5]; ID3D11DepthStencilState *dss = NULL; ID3D11RasterizerState *rs1 = NULL, *rs2 = NULL; ID3D11SamplerState *ss = NULL;
    D3D11_DEPTH_STENCIL_DESC d; D3D11_SAMPLER_DESC s; char nm[128]; int i, fn_ok = 0;
    ID3D11Texture2D *tex = NULL, *rt = NULL, *st = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *rtv = NULL;
    u32 white[16]; static const float src[4] = { 0.5f, 0.25f, 1.0f, 0.5f }, dst[4] = { 0.2f, 0.4f, 0.6f, 0.8f };

    lg("  -- the fixed state set sub_765B8 creates once (redcon-render F5) --");
    memset(&d, 0, sizeof d);
    d.DepthEnable = FALSE; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D11_COMPARISON_LESS; d.StencilEnable = FALSE; d.StencilReadMask = 0xff; d.StencilWriteMask = 0xff;
    d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP; d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS; d.BackFace = d.FrontFace;
    rep("depth-stencil: DepthEnable=0 write ALL func LESS, stencil off", ID3D11Device_CreateDepthStencilState(dev, &d, &dss), 1);
    rep("rasterizer: SOLID, CullNone, DepthClip TRUE, ScissorEnable=1", make_raster(1, &rs1), 1);
    rep("rasterizer: SOLID, CullNone, DepthClip TRUE, ScissorEnable=0", make_raster(0, &rs2), 1);
    memset(&s, 0, sizeof s);
    s.Filter = (D3D11_FILTER)0x15; s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; s.MaxAnisotropy = 2; s.ComparisonFunc = D3D11_COMPARISON_NEVER; s.MinLOD = 0; s.MaxLOD = M_FLT_MAX;
    rep("sampler: MIN_MAG_MIP_LINEAR (0x15), CLAMP, MaxAnisotropy=2, LOD 0..FLT_MAX", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1);
    for (i = 0; i < 5; ++i) {
        bs[i] = NULL;
        m_snprintf(nm, sizeof nm, "blend %s, alpha factors ONE/ZERO/ADD (separate alpha)", names[i]);
        rep(nm, make_blend(sfs[i], dfs[i], D3D11_BLEND_ONE, D3D11_BLEND_ZERO, &bs[i]), 1);
    }

    lg("  -- functional check: draw src=(%s,%s,%s,%s) over dst=(%s,%s,%s,%s) with every state, compare the read-back pixel --", F(src[0], 2), F(src[1], 2), F(src[2], 2), F(src[3], 2),
       F(dst[0], 2), F(dst[1], 2), F(dst[2], 2), F(dst[3], 2));
    if (!quad_init()) { lg("  quad kit creation failed"); g_fail++; goto done; }
    for (i = 0; i < 16; ++i) white[i] = 0xffffffffu;
    srv = make_texture_srv(4, 4, white, &tex);
    if (FAILED(make_rt(64, 64, &rt, &rtv, &st)) || !srv) { lg("  render target creation failed"); g_fail++; goto done; }
    for (i = 0; i < 5; ++i) {
        float clr[4]; int exp[4]; u32 px = 0; HRESULT hr; int r, g, b, a, bad;
        if (!bs[i]) continue;
        clr[0] = dst[0]; clr[1] = dst[1]; clr[2] = dst[2]; clr[3] = dst[3];
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clr);
        quad_bind(srv, 64, 64, rtv);
        ID3D11DeviceContext_OMSetBlendState(ctx, bs[i], NULL, 0xffffffffu);
        hr = cb_map(1, 1, 0, 0, src[0], src[1], src[2], src[3]);
        if (FAILED(hr)) { lg("  constant buffer Map failed hr=0x%08x", (unsigned)hr); g_fail++; continue; }
        ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &Q.cb_dyn);
        ID3D11DeviceContext_DrawIndexed(ctx, 6, 0, 0);
        hr = read_px(rt, st, 32, 32, &px);
        if (FAILED(hr)) { lg("  read-back failed hr=0x%08x", (unsigned)hr); g_fail++; continue; }
        blend_expected(i, src, dst, exp);
        r = (int)(px & 0xff); g = (int)((px >> 8) & 0xff); b = (int)((px >> 16) & 0xff); a = (int)(px >> 24);
        bad = (r < exp[0] - 3 || r > exp[0] + 3 || g < exp[1] - 3 || g > exp[1] + 3 || b < exp[2] - 3 || b > exp[2] + 3 || a < exp[3] - 3 || a > exp[3] + 3);
        lg("  [%s] %-36s got %3d %3d %3d %3d   expected %3d %3d %3d %3d", bad ? "FAIL" : " ok ", names[i], r, g, b, a, exp[0], exp[1], exp[2], exp[3]);
        if (bad) g_fail++; else { g_pass++; fn_ok++; }
    }
    rec("render.blend.functional", "%d/5", fn_ok);
done:
    for (i = 0; i < 5; ++i) REL(bs[i]);
    REL(dss); REL(rs1); REL(rs2); REL(ss); REL(srv); REL(tex); REL(rtv); REL(rt); REL(st); quad_free();
}

/* ======================================================================================================================
 * T5c format / state table (l9bench [B][C][E] + the extras the ports need).  pc = what the PC returned (Intel UHD 620 and WARP agree):
 * 1 ok, 0 refused, -1 not measured.  Differences are flagged "<<<< DIFFERS FROM PC".
 * ==================================================================================================================== */
typedef struct { const char *name; DXGI_FORMAT fmt; const char *pc; } FmtRow;
static const FmtRow k_fmts[] = {
    { "R8G8B8A8_TYPELESS", DXGI_FORMAT_R8G8B8A8_TYPELESS, "FAILED" },
    { "R8G8B8A8_UNORM", DXGI_FORMAT_R8G8B8A8_UNORM, "tex2d=1 rt=1 ds=0 sample=1 mip=1 ib=0 vb=1" },
    { "B8G8R8A8_UNORM", DXGI_FORMAT_B8G8R8A8_UNORM, "tex2d=1 rt=1 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "B8G8R8A8_TYPELESS", DXGI_FORMAT_B8G8R8A8_TYPELESS, "FAILED" },
    { "R32G8X24_TYPELESS", DXGI_FORMAT_R32G8X24_TYPELESS, "FAILED" },
    { "D32_FLOAT_S8X24_UINT", DXGI_FORMAT_D32_FLOAT_S8X24_UINT, "FAILED" },
    { "D24_UNORM_S8_UINT", DXGI_FORMAT_D24_UNORM_S8_UINT, "tex2d=1 rt=0 ds=1 sample=0 mip=0 ib=0 vb=0" },
    { "R24G8_TYPELESS", DXGI_FORMAT_R24G8_TYPELESS, "FAILED" },
    { "D16_UNORM", DXGI_FORMAT_D16_UNORM, "tex2d=1 rt=0 ds=1 sample=0 mip=0 ib=0 vb=0" },
    { "R16_TYPELESS", DXGI_FORMAT_R16_TYPELESS, "FAILED" },
    { "A8_UNORM", DXGI_FORMAT_A8_UNORM, "tex2d=1 rt=1 ds=0 sample=1 mip=0 ib=0 vb=0" },
    { "B4G4R4A4_UNORM", DXGI_FORMAT_B4G4R4A4_UNORM, "tex2d=1 rt=0 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "BC1_UNORM", DXGI_FORMAT_BC1_UNORM, "tex2d=1 rt=0 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "BC3_UNORM", DXGI_FORMAT_BC3_UNORM, "tex2d=1 rt=0 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "R16G16B16A16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT, "FAILED" },
    { "R32_UINT", DXGI_FORMAT_R32_UINT, "FAILED" },
    { "R16_UINT", DXGI_FORMAT_R16_UINT, "tex2d=0 rt=0 ds=0 sample=0 mip=0 ib=1 vb=0" },
    { "B5G6R5_UNORM", DXGI_FORMAT_B5G6R5_UNORM, "tex2d=1 rt=1 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "B5G5R5A1_UNORM", DXGI_FORMAT_B5G5R5A1_UNORM, "tex2d=1 rt=0 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "R8_UNORM", DXGI_FORMAT_R8_UNORM, "tex2d=1 rt=1 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "R8G8_UNORM", DXGI_FORMAT_R8G8_UNORM, "tex2d=1 rt=1 ds=0 sample=1 mip=0 ib=0 vb=0" },
    { "BC2_UNORM", DXGI_FORMAT_BC2_UNORM, "tex2d=1 rt=0 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "R8G8B8A8_UNORM_SRGB", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, "tex2d=1 rt=1 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "D32_FLOAT", DXGI_FORMAT_D32_FLOAT, "FAILED" },
    { "B8G8R8X8_UNORM", DXGI_FORMAT_B8G8R8X8_UNORM, "tex2d=1 rt=1 ds=0 sample=1 mip=1 ib=0 vb=0" },
    { "R16_UNORM", DXGI_FORMAT_R16_UNORM, "FAILED" },
    { "R16_FLOAT", DXGI_FORMAT_R16_FLOAT, "FAILED" },
};
static void flags_text(UINT fs, char *out, int cap)
{
    m_snprintf(out, cap, "tex2d=%d rt=%d ds=%d sample=%d mip=%d ib=%d vb=%d", !!(fs & D3D11_FORMAT_SUPPORT_TEXTURE2D), !!(fs & D3D11_FORMAT_SUPPORT_RENDER_TARGET),
               !!(fs & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL), !!(fs & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE), !!(fs & D3D11_FORMAT_SUPPORT_MIP),
               !!(fs & D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER), !!(fs & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER));
}
#define T2D(nm, w, h, m, f, bind, pc) do { char n_[120]; m_snprintf(n_, sizeof n_, "%s", nm); trep(n_, tex2d(w, h, m, f, bind, NULL), pc); } while (0)
static void render_table(void)
{
    int i; HRESULT hr; UINT fs; char buf[160];
    const UINT fDS = D3D11_BIND_DEPTH_STENCIL, fSR = D3D11_BIND_SHADER_RESOURCE, fRT = D3D11_BIND_RENDER_TARGET;
    lg("  -- CheckFormatSupport at the 9_1 device (compare with games_recon/bad_piggies_bench/design/bench_hw.txt) --");
    for (i = 0; i < (int)(sizeof k_fmts / sizeof k_fmts[0]); ++i) {
        fs = 0; hr = ID3D11Device_CheckFormatSupport(dev, k_fmts[i].fmt, &fs);
        if (FAILED(hr)) s_copy(buf, sizeof buf, "FAILED"); else flags_text(fs, buf, sizeof buf);
        lg("  %-22s hr=0x%08x %-46s %s", k_fmts[i].name, (unsigned)hr, buf, !k_fmts[i].pc ? "" : (s_eq(buf, k_fmts[i].pc) ? "[same as PC]" : "<<<< DIFFERS FROM PC"));
        if (k_fmts[i].pc && !s_eq(buf, k_fmts[i].pc)) g_diff++;
        g_info++;
    }
    lg("  -- textures --");
    T2D("depth R32G8X24_TYPELESS DS|SR 1366x768 (Unity 24-bit depth)", 1366, 768, 1, DXGI_FORMAT_R32G8X24_TYPELESS, fDS | fSR, 0);
    T2D("depth R32G8X24_TYPELESS DS only", 1366, 768, 1, DXGI_FORMAT_R32G8X24_TYPELESS, fDS, 0);
    T2D("depth D24_UNORM_S8_UINT DS (the fix)", 1366, 768, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, fDS, 1);
    T2D("depth R24G8_TYPELESS DS|SR", 1366, 768, 1, DXGI_FORMAT_R24G8_TYPELESS, fDS | fSR, 1);
    T2D("depth D24_UNORM_S8_UINT DS|SR", 1366, 768, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, fDS | fSR, 0);
    T2D("depth R16_TYPELESS DS|SR (16-bit branch)", 1366, 768, 1, DXGI_FORMAT_R16_TYPELESS, fDS | fSR, 1);
    T2D("depth D16_UNORM DS", 1366, 768, 1, DXGI_FORMAT_D16_UNORM, fDS, 1);
    T2D("colour RT R8G8B8A8_TYPELESS RT|SR 1024x1024", 1024, 1024, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, fRT | fSR, 0);
    T2D("colour RT B8G8R8A8_TYPELESS RT|SR", 1024, 1024, 1, DXGI_FORMAT_B8G8R8A8_TYPELESS, fRT | fSR, 0);
    T2D("colour RT R8G8B8A8_UNORM RT|SR (the fix)", 1024, 1024, 1, DXGI_FORMAT_R8G8B8A8_UNORM, fRT | fSR, 1);
    T2D("tex 2049x16 RGBA8 1 mip (wider than 2048)", 2049, 16, 1, DXGI_FORMAT_R8G8B8A8_UNORM, fSR, 1);
    T2D("tex 2160x1080 RGBA8 1 mip", 2160, 1080, 1, DXGI_FORMAT_R8G8B8A8_UNORM, fSR, 1);
    T2D("tex 2048x2048 RGBA8 1 mip", 2048, 2048, 1, DXGI_FORMAT_R8G8B8A8_UNORM, fSR, 1);
    T2D("tex 2048x2048 BC3 1 mip (atlases)", 2048, 2048, 1, DXGI_FORMAT_BC3_UNORM, fSR, 1);
    T2D("tex 4096x2048 RGBA8 12 mips (NPOT round-up of 2160x1080)", 4096, 2048, 12, DXGI_FORMAT_R8G8B8A8_UNORM, fSR, 1);
    T2D("tex 124x124 RGBA8 7 mips (NPOT with mips)", 124, 124, 7, DXGI_FORMAT_R8G8B8A8_UNORM, fSR, 1);
    T2D("tex 124x124 RGBA8 1 mip", 124, 124, 1, DXGI_FORMAT_R8G8B8A8_UNORM, fSR, 1);
    T2D("tex 128x128 RGBA8 8 mips (POT round-up)", 128, 128, 8, DXGI_FORMAT_R8G8B8A8_UNORM, fSR, 1);
    T2D("tex 1024x652 RGBA8 11 mips", 1024, 652, 11, DXGI_FORMAT_R8G8B8A8_UNORM, fSR, 1);
    T2D("tex 36x16 BC3 1 mip (35x16 rounded to block size)", 36, 16, 1, DXGI_FORMAT_BC3_UNORM, fSR, 1);
    T2D("tex 200x200 BC3 1 mip (NPOT)", 200, 200, 1, DXGI_FORMAT_BC3_UNORM, fSR, 1);
    T2D("tex 200x200 BC1 1 mip (NPOT)", 200, 200, 1, DXGI_FORMAT_BC1_UNORM, fSR, 1);
    T2D("tex 100x60 BC1 1 mip (NPOT)", 100, 60, 1, DXGI_FORMAT_BC1_UNORM, fSR, 1);
    T2D("tex 200x200 BC3 8 mips (NPOT with mips)", 200, 200, 8, DXGI_FORMAT_BC3_UNORM, fSR, 1);
    T2D("tex 128x128 A8_UNORM 8 mips", 128, 128, 8, DXGI_FORMAT_A8_UNORM, fSR, 0);
    T2D("tex 128x128 A8_UNORM 1 mip", 128, 128, 1, DXGI_FORMAT_A8_UNORM, fSR, 1);
    T2D("tex 1024x1024 A8_UNORM 1 mip (SDF atlases)", 1024, 1024, 1, DXGI_FORMAT_A8_UNORM, fSR, 1);
    T2D("tex 64x64 B4G4R4A4_UNORM 1 mip", 64, 64, 1, DXGI_FORMAT_B4G4R4A4_UNORM, fSR, 1);
    T2D("tex 64x64 B4G4R4A4_UNORM 7 mips", 64, 64, 7, DXGI_FORMAT_B4G4R4A4_UNORM, fSR, 1);
    {   /* views on a typeless texture, only if the typeless texture exists */
        ID3D11Texture2D *t = NULL;
        if (SUCCEEDED(tex2d(256, 256, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, fRT | fSR, &t)) && t) {
            ID3D11RenderTargetView *rv = NULL; ID3D11ShaderResourceView *sv = NULL; D3D11_RENDER_TARGET_VIEW_DESC rd; D3D11_SHADER_RESOURCE_VIEW_DESC sd;
            memset(&rd, 0, sizeof rd); memset(&sd, 0, sizeof sd);
            rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            trep("RTV R8G8B8A8_UNORM on the typeless texture", ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)t, &rd, &rv), -1); REL(rv);
            sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
            trep("SRV R8G8B8A8_UNORM on the typeless texture", ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)t, &sd, &sv), -1); REL(sv);
            REL(t);
        }
    }
    lg("  -- sampler states --");
    {
        D3D11_SAMPLER_DESC s; ID3D11SamplerState *ss = NULL;
        memset(&s, 0, sizeof s);
        s.Filter = D3D11_FILTER_ANISOTROPIC; s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_WRAP; s.MaxAnisotropy = 16; s.MinLOD = -M_FLT_MAX; s.MaxLOD = M_FLT_MAX; s.ComparisonFunc = D3D11_COMPARISON_NEVER;
        trep("sampler ANISOTROPIC MaxAnisotropy=16", ID3D11Device_CreateSamplerState(dev, &s, &ss), 0); REL(ss);
        s.MaxAnisotropy = 2; trep("sampler ANISOTROPIC MaxAnisotropy=2", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1); REL(ss);
        s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; s.MaxAnisotropy = 16; trep("sampler LINEAR with MaxAnisotropy=16 (field ignored?)", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1); REL(ss);
        s.MaxAnisotropy = 1; trep("sampler LINEAR MinLOD=-FLT_MAX MaxLOD=FLT_MAX (Unity default)", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1); REL(ss);
        s.MipLODBias = -0.5f; trep("sampler LINEAR MipLODBias=-0.5", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1); REL(ss); s.MipLODBias = 0;
        s.MaxAnisotropy = 0; trep("sampler LINEAR MaxAnisotropy=0", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1); REL(ss); s.MaxAnisotropy = 1;
        s.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE; trep("sampler MIRROR_ONCE", ID3D11Device_CreateSamplerState(dev, &s, &ss), 0); REL(ss); s.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        s.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; s.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; trep("sampler comparison (shadow)", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1); REL(ss);
    }
    lg("  -- rasterizer / blend / depth-stencil --");
    {
        D3D11_RASTERIZER_DESC r; ID3D11RasterizerState *rs = NULL; D3D11_BLEND_DESC b; ID3D11BlendState *bs = NULL; D3D11_DEPTH_STENCIL_DESC d; ID3D11DepthStencilState *ds = NULL;
        memset(&r, 0, sizeof r); r.FillMode = D3D11_FILL_SOLID; r.CullMode = D3D11_CULL_BACK; r.DepthClipEnable = FALSE;
        trep("rasterizer DepthClipEnable=FALSE ('ZClip False')", ID3D11Device_CreateRasterizerState(dev, &r, &rs), 0); REL(rs);
        r.DepthClipEnable = TRUE; r.ScissorEnable = TRUE; r.SlopeScaledDepthBias = 1.0f; r.DepthBias = 1;
        trep("rasterizer DepthClip=TRUE scissor, depth bias", ID3D11Device_CreateRasterizerState(dev, &r, &rs), 1); REL(rs);
        memset(&b, 0, sizeof b);
        b.RenderTarget[0].BlendEnable = TRUE; b.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; b.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; b.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA; b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; b.RenderTarget[0].RenderTargetWriteMask = 0xf;
        trep("blend SrcAlpha/InvSrcAlpha, alpha = colour factors", ID3D11Device_CreateBlendState(dev, &b, &bs), 1); REL(bs);
        b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        trep("blend separate alpha factors (One,One)", ID3D11Device_CreateBlendState(dev, &b, &bs), 1); REL(bs);
        b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA; b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
        trep("blend separate alpha op (BlendOpAlpha=MAX)", ID3D11Device_CreateBlendState(dev, &b, &bs), 1); REL(bs); b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        b.AlphaToCoverageEnable = TRUE; trep("blend AlphaToCoverageEnable=TRUE", ID3D11Device_CreateBlendState(dev, &b, &bs), 0); REL(bs); b.AlphaToCoverageEnable = FALSE;
        b.RenderTarget[0].RenderTargetWriteMask = 0xe; trep("blend colour mask 14 (RGB)", ID3D11Device_CreateBlendState(dev, &b, &bs), 1); REL(bs);
        b.RenderTarget[0].RenderTargetWriteMask = 0; trep("blend colour mask 0", ID3D11Device_CreateBlendState(dev, &b, &bs), 1); REL(bs);
        memset(&d, 0, sizeof d); d.DepthEnable = TRUE; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; d.StencilEnable = TRUE; d.StencilReadMask = d.StencilWriteMask = 0xff;
        d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP; d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE; d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS; d.BackFace = d.FrontFace;
        trep("depth-stencil front == back", ID3D11Device_CreateDepthStencilState(dev, &d, &ds), 1); REL(ds);
        d.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO; trep("depth-stencil front != back (two-sided)", ID3D11Device_CreateDepthStencilState(dev, &d, &ds), 1); REL(ds);
    }
    lg("  -- queries and buffers --");
    {
        D3D11_QUERY_DESC q; ID3D11Query *qq = NULL; ID3D11Buffer *b = NULL;
        memset(&q, 0, sizeof q);
        q.Query = D3D11_QUERY_TIMESTAMP_DISJOINT; trep("query TIMESTAMP_DISJOINT", ID3D11Device_CreateQuery(dev, &q, &qq), 1); REL(qq);
        q.Query = D3D11_QUERY_TIMESTAMP; trep("query TIMESTAMP", ID3D11Device_CreateQuery(dev, &q, &qq), 1); REL(qq);
        q.Query = D3D11_QUERY_EVENT; trep("query EVENT", ID3D11Device_CreateQuery(dev, &q, &qq), 1); REL(qq);
        q.Query = D3D11_QUERY_OCCLUSION; trep("query OCCLUSION", ID3D11Device_CreateQuery(dev, &q, &qq), 0); REL(qq);
        q.Query = D3D11_QUERY_OCCLUSION_PREDICATE; trep("query OCCLUSION_PREDICATE", ID3D11Device_CreateQuery(dev, &q, &qq), 0); REL(qq);
        trep("dynamic index buffer 4096 B", make_buffer(4096, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL, &b), 1); REL(b);
        trep("buffer VB|IB combined", make_buffer(4096, D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL, &b), 0); REL(b);
        trep("constant buffer 368 bytes (UnityPerFrame)", make_buffer(368, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL, &b), 1); REL(b);
    }
}

static void t_render(const char *cas)
{
    char title[96], key[48];
    m_snprintf(title, sizeof title, "T5 render at FL 9_1 on HARDWARE {9_1 only} [%s]", cas); m_snprintf(key, sizeof key, "render.%s", cas);
    test_begin(title, key);
    if (!load_d3d()) { g_fail++; lg("  cannot load d3d11/dxgi"); return; }
    mods_baseline();
    if (!open_dev(0, 1, "render")) { g_fail++; test_end(); return; }
    if (s_eq(cas, "draw")) sprite_test(0, "draw");
    else if (s_eq(cas, "il")) sprite_test(1, "il");
    else if (s_eq(cas, "cboff")) sprite_test(2, "cboff");
    else if (s_eq(cas, "blend")) render_blend();
    else if (s_eq(cas, "table")) render_table();
    else { lg("  unknown case %s", cas); g_fail++; }
    drop_dev();
    test_end();
}

/* explicit only: Unity says Supports32bitsIndexBuffer=1; at 9_1 IASetIndexBuffer(R32_UINT) removed the Intel device on the PC */
static void t_idx32(void)
{
    ID3D11Buffer *ib = NULL; HRESULT hr;
    test_begin("T5x IASetIndexBuffer(R32_UINT) at FL 9_1 (explicit test, can hang or remove the device)", "idx32");
    if (!load_d3d() || !open_dev(0, 1, "idx32")) { g_fail++; test_end(); return; }
    hr = make_buffer(4096, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL, &ib);
    rep("dynamic index buffer", hr, 1);
    rep("GetDeviceRemovedReason before", ID3D11Device_GetDeviceRemovedReason(dev), 1);
    if (ib) {
        ID3D11DeviceContext_IASetIndexBuffer(ctx, ib, DXGI_FORMAT_R32_UINT, 0);
        lg("  IASetIndexBuffer(R32_UINT) returned (void call)");
        ID3D11DeviceContext_Flush(ctx);
    }
    rep("GetDeviceRemovedReason after", ID3D11Device_GetDeviceRemovedReason(dev), -1);
    REL(ib); drop_dev();
    test_end();
}

/* ======================================================================================================================
 * T6 device vtable copy (crit_vt.c): replace the device's vtable pointer by a private FULL-length copy, then make a swap chain like Unity's
 * (B8G8R8A8, 2 buffers, FLIP_SEQUENTIAL, usage 0x30 = RENDER_TARGET_OUTPUT|SHADER_INPUT, flags 0x40 = FRAME_LATENCY_WAITABLE_OBJECT), 30 x Present, ResizeBuffers
 * ==================================================================================================================== */
static void **g_orig_vt; static void *g_copy_vt[512]; static volatile LONG g_calls[64];
static HRESULT (STDMETHODCALLTYPE *o_CreateTexture2D)(ID3D11Device *, const D3D11_TEXTURE2D_DESC *, const D3D11_SUBRESOURCE_DATA *, ID3D11Texture2D **);
static HRESULT STDMETHODCALLTYPE h_CreateTexture2D(ID3D11Device *d, const D3D11_TEXTURE2D_DESC *a, const D3D11_SUBRESOURCE_DATA *b, ID3D11Texture2D **c) { g_calls[5]++; return o_CreateTexture2D(d, a, b, c); }
static HRESULT (STDMETHODCALLTYPE *o_CreateRTV)(ID3D11Device *, ID3D11Resource *, const D3D11_RENDER_TARGET_VIEW_DESC *, ID3D11RenderTargetView **);
static HRESULT STDMETHODCALLTYPE h_CreateRTV(ID3D11Device *d, ID3D11Resource *a, const D3D11_RENDER_TARGET_VIEW_DESC *b, ID3D11RenderTargetView **c) { g_calls[9]++; return o_CreateRTV(d, a, b, c); }
static D3D_FEATURE_LEVEL (STDMETHODCALLTYPE *o_GetFL)(ID3D11Device *);
static D3D_FEATURE_LEVEL STDMETHODCALLTYPE h_GetFL(ID3D11Device *d) { g_calls[37]++; return o_GetFL(d); }
static HRESULT (STDMETHODCALLTYPE *o_CFS)(ID3D11Device *, D3D11_FEATURE, void *, UINT);
static HRESULT STDMETHODCALLTYPE h_CFS(ID3D11Device *d, D3D11_FEATURE f, void *p, UINT s) { g_calls[33]++; return o_CFS(d, f, p, s); }
static HRESULT (STDMETHODCALLTYPE *o_QI)(ID3D11Device *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE h_QI(ID3D11Device *d, REFIID r, void **o) { g_calls[0]++; return o_QI(d, r, o); }

static HWND make_window(UINT w, UINT h)
{
    WNDCLASSW wc; HWND hw;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"m0probe_window";
    RegisterClassW(&wc);
    hw = CreateWindowExW(0, L"m0probe_window", L"m0probe (diagnostic window, closes by itself)", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, (int)w, (int)h, NULL, NULL, wc.hInstance, NULL);
    return hw;
}
static void pump(void) { MSG m; while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); } }

static void t_vt(int shortcopy)
{
    static const IID *iids[6] = { &IID_ID3D11Device, &IID_ID3D11Device1, &IID_ID3D11Device2, &IID_ID3D11Device3, &IID_ID3D11Device4, &IID_ID3D11Device5 };
    HRESULT hr; int i, ncopy, glen; HMODULE d3d; MEMORY_BASIC_INFORMATION mbi; u8 *lo, *hi; LONG before[64]; int all_ok = 1;
    HWND wnd; IDXGIDevice *dd = NULL; IDXGIAdapter *ad = NULL; IDXGIFactory2 *fa = NULL; IDXGISwapChain1 *sc = NULL; DXGI_SWAP_CHAIN_DESC1 d; UINT flags_used = 0;
    static const struct { UINT usage; DXGI_SWAP_EFFECT eff; UINT bufs; UINT flags; const char *what; } ladder[4] = {
        { 0x30, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2, 0x40, "Unity's desc: usage 0x30 FLIP_SEQUENTIAL 2 buffers flags 0x40" },
        { 0x20, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2, 0x40, "usage 0x20 (no SHADER_INPUT) FLIP_SEQUENTIAL flags 0x40" },
        { 0x20, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2, 0x00, "usage 0x20 FLIP_SEQUENTIAL, no flags" },
        { 0x20, DXGI_SWAP_EFFECT_DISCARD,         1, 0x00, "usage 0x20 DISCARD (blt model), 1 buffer" } };
    int rung;

    test_begin(shortcopy ? "T6x SHORT (54-slot) device vtable copy (explicit crash test)" : "T6 full device vtable copy + Unity-style swap chain + Present + ResizeBuffers", shortcopy ? "vtshort" : "vt");
    if (!load_d3d()) { g_fail++; return; }
    mods_baseline();
    hr = make_dev(NULL, D3D_DRIVER_TYPE_HARDWARE, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED, k_lv_91, 1);
    lg("  device HARDWARE {9_1} BGRA|SINGLETHREADED (like Unity, no debug layer): hr=0x%08x FL=%s", (unsigned)hr, fl_name((unsigned)g_fl));
    if (FAILED(hr)) { g_fail++; rec("vt.ok", "0"); test_end(); return; }
    for (i = 0; i < 6; ++i) {
        void *p = NULL; HRESULT h = ID3D11Device_QueryInterface(dev, iids[i], &p);
        lg("  QI ID3D11Device%c hr=0x%08x ptr=%p same-object=%d same-vtbl=%d", i ? (char)('0' + i) : ' ', (unsigned)h, p, p == (void *)dev, p && *(void **)p == *(void **)dev);
        if (p) IUnknown_Release((IUnknown *)p);
    }
    g_orig_vt = *(void ***)dev;
    d3d = GetModuleHandleW(L"d3d11.dll");
    {   /* module range from the PE header (no psapi needed) */
        const IMAGE_DOS_HEADER *dh = (const IMAGE_DOS_HEADER *)d3d; const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const u8 *)d3d + dh->e_lfanew);
        lo = (u8 *)d3d; hi = lo + nt->OptionalHeader.SizeOfImage;
    }
    for (glen = 0; glen < 512; ++glen) { u8 *fn = (u8 *)g_orig_vt[glen]; if (fn < lo || fn >= hi) break; }
    memset(&mbi, 0, sizeof mbi); VirtualQuery(g_orig_vt, &mbi, sizeof mbi);
    lg("  device vtable at %p (page protect 0x%x): %d consecutive entries point into d3d11.dll (ID3D11Device=43, Device1=50, Device2=54, Device3=65, Device4=67, Device5=69)", g_orig_vt, (unsigned)mbi.Protect, glen);
    rec("vt.len", "%d", glen);
    ncopy = shortcopy ? 54 : glen;
    memcpy(g_copy_vt, g_orig_vt, sizeof(void *) * (size_t)ncopy);
    for (i = ncopy; i < 512; ++i) g_copy_vt[i] = NULL;   /* a short copy leaves the tail NULL -> crash if anyone calls Device3+ through it */
    o_QI = (void *)g_orig_vt[0]; g_copy_vt[0] = (void *)h_QI;
    o_CreateTexture2D = (void *)g_orig_vt[5]; g_copy_vt[5] = (void *)h_CreateTexture2D;
    o_CreateRTV = (void *)g_orig_vt[9]; g_copy_vt[9] = (void *)h_CreateRTV;
    o_CFS = (void *)g_orig_vt[33]; g_copy_vt[33] = (void *)h_CFS;
    o_GetFL = (void *)g_orig_vt[37]; g_copy_vt[37] = (void *)h_GetFL;
    *(void ***)dev = g_copy_vt;
    lg("  vtable swapped (%d slots copied; slots 0 QI, 5 CreateTexture2D, 9 CreateRenderTargetView, 33 CheckFeatureSupport, 37 GetFeatureLevel are hooked)", ncopy);

    wnd = make_window(640, 360);
    lg("  window created: %p", (void *)wnd);
    hr = ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd); rep("QI IDXGIDevice through the swapped vtable", hr, 1);
    if (dd) IDXGIDevice_GetAdapter(dd, &ad);
    if (ad) IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory2, (void **)&fa);
    if (!fa) { lg("  no IDXGIFactory2"); g_fail++; all_ok = 0; goto restore; }
    for (i = 0; i < 64; ++i) before[i] = g_calls[i];
    for (rung = 0; rung < 4 && !sc; ++rung) {
        memset(&d, 0, sizeof d);
        d.Width = 640; d.Height = 360; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1; d.BufferUsage = ladder[rung].usage;
        d.BufferCount = ladder[rung].bufs; d.Scaling = DXGI_SCALING_STRETCH; d.SwapEffect = ladder[rung].eff; d.Flags = ladder[rung].flags;
        hr = IDXGIFactory2_CreateSwapChainForHwnd(fa, (IUnknown *)dev, wnd, &d, NULL, NULL, &sc);
        lg("  CreateSwapChainForHwnd [%s]: hr=0x%08x", ladder[rung].what, (unsigned)hr);
        if (SUCCEEDED(hr) && sc) { flags_used = ladder[rung].flags; rec("vt.swapchain", "rung %d (%s)", rung, ladder[rung].what); if (rung == 0) g_pass++; else { lg("  NOTE: Unity's exact swap-chain description was refused; fallback %d worked", rung); g_info++; } }
        else { sc = NULL; if (rung == 0) all_ok = 0; }
    }
    lg("  calls made by DXGI into the hooked slots during swap-chain creation: QI %d, CreateTexture2D %d, CreateRTV %d, CheckFeatureSupport %d, GetFeatureLevel %d",
       (int)(g_calls[0] - before[0]), (int)(g_calls[5] - before[5]), (int)(g_calls[9] - before[9]), (int)(g_calls[33] - before[33]), (int)(g_calls[37] - before[37]));
    if (sc) {
        ID3D11Texture2D *bb = NULL; ID3D11RenderTargetView *rtv = NULL; float c[4] = { 0.2f, 0.4f, 0.6f, 1.0f }; i64 t0; int k;
        D3D11_TEXTURE2D_DESC bd; ID3D11ShaderResourceView *srv = NULL;
        IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
        if (bb) {
            memset(&bd, 0, sizeof bd); ID3D11Texture2D_GetDesc(bb, &bd);
            lg("  back buffer: %ux%u format %u bind flags 0x%x", bd.Width, bd.Height, (unsigned)bd.Format, bd.BindFlags);
            trep("CreateShaderResourceView(back buffer)", ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)bb, NULL, &srv), -1); REL(srv);
            hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
            rep("CreateRenderTargetView(back buffer) through the copy", hr, 1);
            lg("  (RTV hook count now %d)", (int)g_calls[9]);
        } else { lg("  GetBuffer failed"); g_fail++; all_ok = 0; }
        if (rtv) {
            t0 = qpc();
            for (k = 0; k < 30; ++k) {
                ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, c);
                hr = IDXGISwapChain1_Present(sc, 0, 0); pump();
                if (FAILED(hr)) break;
            }
            lg("  %d x Present(0,0) hr=0x%08x in %s ms", k, (unsigned)hr, F(ms_since(t0), 1));
            rep("30 x Present", hr, 1);
            if (FAILED(hr)) all_ok = 0;
            hr = IDXGISwapChain1_ResizeBuffers(sc, 0, 0, 0, DXGI_FORMAT_UNKNOWN, flags_used);
            lg("  ResizeBuffers while the RTV is alive: hr=0x%08x (the PC returned DXGI_ERROR_INVALID_CALL 0x887a0001 - expected)", (unsigned)hr);
            g_info++;
            REL(rtv); REL(bb);
            ID3D11DeviceContext_OMSetRenderTargets(ctx, 0, NULL, NULL); ID3D11DeviceContext_Flush(ctx);
            hr = IDXGISwapChain1_ResizeBuffers(sc, 0, 800, 450, DXGI_FORMAT_UNKNOWN, flags_used);
            rep("ResizeBuffers 800x450 after releasing the RTV", hr, 1);
            if (FAILED(hr)) all_ok = 0;
            if (SUCCEEDED(hr)) {
                IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
                if (bb) ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
                if (rtv) {
                    for (k = 0; k < 5; ++k) { ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, c); hr = IDXGISwapChain1_Present(sc, 0, 0); pump(); if (FAILED(hr)) break; }
                    rep("5 x Present after the resize", hr, 1);
                    if (FAILED(hr)) all_ok = 0;
                }
                REL(rtv); REL(bb);
            }
        } else all_ok = 0;
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 0, NULL, NULL);
        REL(sc);
    } else { g_fail++; all_ok = 0; }
    {   /* a caller that goes through a NEWER interface bypasses the slot hooks only when the vtable is complete */
        ID3D11Device3 *d3 = NULL;
        hr = ID3D11Device_QueryInterface(dev, &IID_ID3D11Device3, (void **)&d3);
        lg("  QI ID3D11Device3 through the copy: hr=0x%08x", (unsigned)hr);
        if (d3) {
            D3D11_TEXTURE2D_DESC1 t; ID3D11Texture2D1 *tt = NULL; LONG b5 = g_calls[5];
            memset(&t, 0, sizeof t);
            t.Width = t.Height = 16; t.MipLevels = 1; t.ArraySize = 1; t.Format = DXGI_FORMAT_R8G8B8A8_UNORM; t.SampleDesc.Count = 1; t.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            if (!shortcopy) {
                hr = ID3D11Device3_CreateTexture2D1(d3, &t, NULL, &tt);
                lg("  ID3D11Device3::CreateTexture2D1 through the copy hr=0x%08x; the CreateTexture2D hook fired %d times (a Device3 caller bypasses the slot-5 hook)", (unsigned)hr, (int)(g_calls[5] - b5));
            } else lg("  short copy: ID3D11Device3 methods would jump to NULL (not called)");
            REL(tt); ID3D11Device3_Release(d3);
        }
    }
restore:
    REL(fa); REL(ad); REL(dd);
    rep("GetDeviceRemovedReason", ID3D11Device_GetDeviceRemovedReason(dev), 1);
    *(void ***)dev = g_orig_vt;
    if (wnd) DestroyWindow(wnd);
    mods_report("vt", 0);
    drop_dev();
    rec("vt.ok", "%d", all_ok && !g_fail);
    test_end();
}

/* ======================================================================================================================
 * T7 speed: offscreen 1280x720 render target, N alpha-blended full-screen textured quads per frame, event-query synchronised;
 *           500 small draws/frame with a constant-buffer update per draw (Map(DISCARD), UpdateSubresource, VSSetConstantBuffers1 windows)
 * ==================================================================================================================== */
static ID3D11Query *g_evq; static int g_gpu_dead;
static void gpu_wait(void)
{
    BOOL done = FALSE; HRESULT hr; DWORD t0 = GetTickCount();
    ID3D11DeviceContext_End(ctx, (ID3D11Asynchronous *)g_evq);
    ID3D11DeviceContext_Flush(ctx);
    for (;;) {
        hr = ID3D11DeviceContext_GetData(ctx, (ID3D11Asynchronous *)g_evq, &done, sizeof done, 0);
        if (hr != S_FALSE) break;
        if (GetTickCount() - t0 > 10000) { lg("  !!! the GPU did not finish a frame within 10 s - giving up on this test (device removed reason 0x%08x)", (unsigned)ID3D11Device_GetDeviceRemovedReason(dev)); g_gpu_dead = 1; break; }
        Sleep(0);
    }
}
static double fill_frame(ID3D11RenderTargetView *rtv, int quads)
{
    static const float clr[4] = { 0, 0, 0, 0 }; i64 t0 = qpc(); int q;
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clr);
    for (q = 0; q < quads; ++q) ID3D11DeviceContext_DrawIndexed(ctx, 6, 0, 0);
    gpu_wait();
    return ms_since(t0);
}
/* variant: 0 = Map(DISCARD)+VSSetConstantBuffers per draw, 1 = UpdateSubresource+VSSetConstantBuffers per draw, 2 = VSSetConstantBuffers1 windows (Unity's path) */
static double cpu_frame(ID3D11RenderTargetView *rtv, int draws, int variant, ID3D11DeviceContext1 *c1, ID3D11Buffer *ring, double *submit_ms)
{
    static const float clr[4] = { 0, 0, 0, 0 }; i64 t0 = qpc(); int i;
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clr);
    if (variant == 2) {
        int base = 0;
        while (base < draws) {
            int chunk = draws - base > 256 ? 256 : draws - base; D3D11_MAPPED_SUBRESOURCE m;
            if (FAILED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)ring, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) break;
            for (i = 0; i < chunk; ++i) {
                int n = base + i; float *f = (float *)((u8 *)m.pData + 256 * i);
                f[0] = 0.03f; f[1] = 0.04f; f[2] = (float)(n % 25) * 0.075f - 0.9f; f[3] = (float)(n / 25) * 0.09f - 0.85f; f[4] = 1; f[5] = 0.5f; f[6] = 0.25f; f[7] = 0.5f;
            }
            ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)ring, 0);
            for (i = 0; i < chunk; ++i) {
                UINT first = (UINT)(16 * i), num = 16;
                ID3D11DeviceContext1_VSSetConstantBuffers1(c1, 0, 1, &ring, &first, &num);
                ID3D11DeviceContext_DrawIndexed(ctx, 6, 0, 0);
            }
            base += chunk;
        }
    } else {
        for (i = 0; i < draws; ++i) {
            float f[8]; f[0] = 0.03f; f[1] = 0.04f; f[2] = (float)(i % 25) * 0.075f - 0.9f; f[3] = (float)(i / 25) * 0.09f - 0.85f; f[4] = 1; f[5] = 0.5f; f[6] = 0.25f; f[7] = 0.5f;
            if (variant == 0) { cb_map(f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]); ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &Q.cb_dyn); }
            else { ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource *)Q.cb_def, 0, NULL, f, 0, 0); ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &Q.cb_def); }
            ID3D11DeviceContext_DrawIndexed(ctx, 6, 0, 0);
        }
    }
    *submit_ms = ms_since(t0);
    gpu_wait();
    return ms_since(t0);
}
static void t_speed(const char *cas)
{
    int warp = s_eq(cas, "warpbest"), only91 = s_eq(cas, "hw91"); char title[96], key[48];
    ID3D11Texture2D *rt = NULL, *tex = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11ShaderResourceView *srv = NULL; static u32 texels[256 * 256]; int x, y;
    D3D11_QUERY_DESC qd; double warm, avg, mn, sum; int quads = 200, frames, f;
    m_snprintf(title, sizeof title, "T7 speed [%s]: %s, %s", cas, warp ? "WARP" : "HARDWARE", only91 ? "FL {9_1 only}" : "best feature level");
    m_snprintf(key, sizeof key, "speed.%s", cas);
    test_begin(title, key);
    if (!load_d3d()) { g_fail++; return; }
    mods_baseline();
    if (!open_dev(warp, only91, cas)) { g_fail++; test_end(); return; }
    if (s_eq(cas, "hwbest") && g_fl == D3D_FEATURE_LEVEL_9_1) {
        lg("  the best HARDWARE level IS 9_1: identical to the hw91 run, nothing more to measure");
        rec("speed.hwbest.same_as_hw91", "1"); drop_dev(); g_info++; test_end(); return;
    }
    rec(K("speed.%s.level", cas), "%s", fl_name((unsigned)g_fl));
    if (!quad_init()) { g_fail++; quad_free(); drop_dev(); test_end(); return; }
    for (y = 0; y < 256; ++y) for (x = 0; x < 256; ++x) texels[y * 256 + x] = 0xff000000u | ((u32)x << 16) | ((u32)y << 8) | (u32)((x ^ y) & 0xff);
    srv = make_texture_srv(256, 256, texels, &tex);
    if (FAILED(make_rt(1280, 720, &rt, &rtv, NULL)) || !srv) { lg("  1280x720 render target / texture creation failed"); g_fail++; goto done; }
    memset(&qd, 0, sizeof qd); qd.Query = D3D11_QUERY_EVENT;
    if (FAILED(ID3D11Device_CreateQuery(dev, &qd, &g_evq))) { lg("  event query creation failed"); g_fail++; goto done; }
    quad_bind(srv, 1280, 720, rtv);
    ID3D11DeviceContext_OMSetBlendState(ctx, Q.alpha, NULL, 0xffffffffu);

    /* ---- fill rate ---- */
    cb_map(1, 1, 0, 0, 1, 1, 1, 0.1f);
    ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &Q.cb_dyn);
    {   /* the first frames also pay for driver / WARP warm-up: measure a small frame twice, then size the real frame for ~400 ms (200 quads on a fast device) */
        double t0 = fill_frame(rtv, 10), t1 = fill_frame(rtv, 10), per_quad = t1 / 10.0;
        if (per_quad < 0.001) per_quad = 0.001;
        quads = (int)(400.0 / per_quad); if (quads > 200) quads = 200; if (quads < 1) quads = 1;
        lg("  fill: warm-up frame (10 quads) %s ms, second frame %s ms -> %d quads per measured frame (target 400 ms, at most 200)", F(t0, 1), F(t1, 1), quads);
        warm = fill_frame(rtv, quads);
        lg("  fill: first frame at that size took %s ms", F(warm, 1));
    }
    frames = (int)(6000.0 / (warm > 1.0 ? warm : 1.0)); if (frames > 20) frames = 20; if (frames < 3) frames = 3;
    sum = 0; mn = 1e9;
    for (f = 0; f < frames && !g_gpu_dead; ++f) { double ms = fill_frame(rtv, quads); sum += ms; if (ms < mn) mn = ms; }
    if (g_gpu_dead) { g_fail++; goto done; }
    avg = sum / frames;
    {
        double mpix = (double)quads * 0.9216 * 1000.0 / (avg > 0.001 ? avg : 0.001);
        lg("  FILL RATE: %d alpha-blended full-screen quads (1280x720) x %d frames: %s ms/frame avg (best %s) = %s Mpixel/s", quads, frames, F(avg, 2), F(mn, 2), F(mpix, 1));
        rec(K("speed.%s.fill_ms", cas), "%s", F(avg, 2));
        rec(K("speed.%s.fill_mpix", cas), "%s", F(mpix, 1));
        g_info++;
    }

    /* ---- CPU draw-call cost ---- */
    {
        ID3D11DeviceContext1 *c1 = NULL; ID3D11Buffer *ring = NULL; D3D11_FEATURE_DATA_D3D11_OPTIONS o; int variant; static const char *const vname[3] = {
            "Map(WRITE_DISCARD) + VSSetConstantBuffers per draw", "UpdateSubresource + VSSetConstantBuffers per draw", "VSSetConstantBuffers1 windows into a dynamic ring (Unity's path)" };
        static const char *const vkey[3] = { "cpu_map_us", "cpu_upd_us", "cpu_off_us" };
        memset(&o, 0, sizeof o);
        ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof o);
        if (o.ConstantBufferOffsetting && SUCCEEDED(ID3D11DeviceContext_QueryInterface(ctx, &IID_ID3D11DeviceContext1, (void **)&c1)) && c1)
            make_buffer(65536, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, NULL, &ring);
        cb_map(0.03f, 0.04f, 0, 0, 1, 0.5f, 0.25f, 0.5f);
        for (variant = 0; variant < 3; ++variant) {
            int draws = 500; double submit = 0, sub_sum = 0, tot_sum = 0, tw, per_draw_submit, per_draw_total;
            if (variant == 2 && (!c1 || !ring)) { lg("  CPU test: %s - SKIPPED (device reports ConstantBufferOffsetting=%d)", vname[2], o.ConstantBufferOffsetting); rec(K("speed.%s.%s", cas, vkey[2]), "n/a"); continue; }
            ID3D11DeviceContext_OMSetBlendState(ctx, Q.alpha, NULL, 0xffffffffu);
            tw = cpu_frame(rtv, draws, variant, c1, ring, &submit);
            if (tw > 1000.0) { draws = (int)(500.0 * 400.0 / tw); if (draws < 10) draws = 10; lg("  frame slower than 1 s: scaled down to %d draws per frame", draws); tw = cpu_frame(rtv, draws, variant, c1, ring, &submit); }
            frames = (int)(2500.0 / (tw > 1.0 ? tw : 1.0)); if (frames > 20) frames = 20; if (frames < 3) frames = 3;
            for (f = 0; f < frames && !g_gpu_dead; ++f) { double t = cpu_frame(rtv, draws, variant, c1, ring, &submit); sub_sum += submit; tot_sum += t; }
            if (g_gpu_dead) { g_fail++; break; }
            per_draw_submit = sub_sum / frames / draws * 1000.0; per_draw_total = tot_sum / frames / draws * 1000.0;
            lg("  CPU draws [%s]:", vname[variant]);
            lg("      %d draws x %d frames: submit %s ms/frame, total %s ms/frame => %s us/draw submit, %s us/draw total", draws, frames, F(sub_sum / frames, 2), F(tot_sum / frames, 2), F(per_draw_submit, 1), F(per_draw_total, 1));
            rec(K("speed.%s.%s", cas, vkey[variant]), "%s", F(per_draw_total, 1));
            g_info++;
        }
        REL(c1); REL(ring);
    }
    rep("GetDeviceRemovedReason after the speed test", ID3D11Device_GetDeviceRemovedReason(dev), 1);
done:
    REL(g_evq); REL(srv); REL(tex); REL(rtv); REL(rt); quad_free();
    mods_report(cas, 0);
    drop_dev();
    test_end();
}

/* ======================================================================================================================
 * parent: result file access, child launcher, summary
 * ==================================================================================================================== */
static int res_get(const char *key, char *out, int cap)
{
    WCHAR p[MAX_PATH]; unsigned n = 0, i = 0; u8 *b; int kl = s_len(key), found = 0;
    out[0] = 0;
    wpath(p, L"m0probe.res");
    b = read_file(p, &n);
    if (!b) return 0;
    while (i < n) {
        unsigned ls = i, le = i;
        while (le < n && b[le] != '\n') ++le;
        if ((int)(le - ls) > kl && !memcmp(b + ls, key, (size_t)kl) && b[ls + kl] == '=') {
            unsigned l = le - ls - (unsigned)kl - 1; if (l && b[le - 1] == '\r') --l; if ((int)l > cap - 1) l = (unsigned)cap - 1;
            memcpy(out, b + ls + kl + 1, l); out[l] = 0; found = 1;
        }
        i = le + 1;
    }
    free_file(b);
    return found;
}
static int res_int(const char *key, int dflt) { char v[64]; int n = 0, neg = 0, i = 0; if (!res_get(key, v, sizeof v)) return dflt; if (v[0] == '-') { neg = 1; i = 1; } for (; v[i] >= '0' && v[i] <= '9'; ++i) n = n * 10 + (v[i] - '0'); return neg ? -n : n; }

typedef struct { char name[48]; int launched; DWORD code; int timeout; double ms; } Run;
static char g_arg_test[48], g_arg_case[48];
static void child_main(void);
static Run g_runs[48]; static int g_nruns;
static int g_arg_nospawn;   /* --no-spawn: run every test in this process (same code path as when CreateProcess fails) */
static int run_inprocess(const char *name, Run *r, const char *test, const char *cas)
{
    i64 t0 = qpc();
    r->launched = 1; r->code = 0;
    s_copy(g_arg_test, sizeof g_arg_test, test); s_copy(g_arg_case, sizeof g_arg_case, cas ? cas : "");
    child_main();
    r->ms = ms_since(t0);
    lg("[run] %s: finished in-process, %s s", name, F(r->ms / 1000.0, 1));
    return 1;
}
static int run_child(const char *test, const char *cas, int timeout_s)
{
    WCHAR cmd[MAX_PATH + 160], cmd0[MAX_PATH + 160], wt[64]; STARTUPINFOW si; PROCESS_INFORMATION pi; DWORD wr, code = 0; i64 t0; Run *r; char name[48];
    if (cas) m_snprintf(name, sizeof name, "%s %s", test, cas); else s_copy(name, sizeof name, test);
    r = &g_runs[g_nruns < 47 ? g_nruns++ : 47]; memset(r, 0, sizeof *r); s_copy(r->name, sizeof r->name, name);
    if (g_arg_nospawn) return run_inprocess(name, r, test, cas);
    w_copy(cmd, (int)(sizeof cmd / sizeof cmd[0]), L"\""); w_cat(cmd, (int)(sizeof cmd / sizeof cmd[0]), g_exe); w_cat(cmd, (int)(sizeof cmd / sizeof cmd[0]), L"\" --test ");
    a2w(wt, 64, test); w_cat(cmd, (int)(sizeof cmd / sizeof cmd[0]), wt);
    if (cas) { w_cat(cmd, (int)(sizeof cmd / sizeof cmd[0]), L" --case "); a2w(wt, 64, cas); w_cat(cmd, (int)(sizeof cmd / sizeof cmd[0]), wt); }
    memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); si.hStdOutput = (g_con == INVALID_HANDLE_VALUE) ? NULL : g_con; si.hStdError = si.hStdOutput;
    if (si.hStdOutput) SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    if (si.hStdInput && si.hStdInput != INVALID_HANDLE_VALUE) SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT); else si.hStdInput = NULL;
    memset(&pi, 0, sizeof pi);
    t0 = qpc();
    w_copy(cmd0, (int)(sizeof cmd0 / sizeof cmd0[0]), cmd);
    if (!CreateProcessW(g_exe, cmd, NULL, NULL, TRUE, 0, NULL, g_dir, &si, &pi)) {
        DWORD e1 = GetLastError(); STARTUPINFOW si2;
        memset(&si2, 0, sizeof si2); si2.cb = sizeof si2; memset(&pi, 0, sizeof pi);
        w_copy(cmd, (int)(sizeof cmd / sizeof cmd[0]), cmd0);
        if (!CreateProcessW(g_exe, cmd, NULL, NULL, FALSE, 0, NULL, g_dir, &si2, &pi)) {   /* second try: the child just inherits the console */
            lg("[run] %s: CreateProcess FAILED, errors %u / %u - running this test IN THIS PROCESS instead (a driver crash would end the whole run)", name, (unsigned)e1, (unsigned)GetLastError());
            return run_inprocess(name, r, test, cas);
        }
        lg("[run] %s: launched without explicit std handles (first attempt failed with error %u)", name, (unsigned)e1);
    }
    r->launched = 1;
    wr = WaitForSingleObject(pi.hProcess, (DWORD)timeout_s * 1000u);
    if (wr == WAIT_TIMEOUT) {
        r->timeout = 1;
        lg("[run] %s: TIMEOUT after %d s - terminating the child (driver hang?)", name, timeout_s);
        TerminateProcess(pi.hProcess, 0xDEAD0001u);
        WaitForSingleObject(pi.hProcess, 10000);
    }
    GetExitCodeProcess(pi.hProcess, &code);
    r->code = code; r->ms = ms_since(t0);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (!r->timeout) {
        if (code == 0) lg("[run] %s: finished, exit code 0, %s s", name, F(r->ms / 1000.0, 1));
        else lg("[run] %s: CRASHED / ABNORMAL EXIT, exit code 0x%08x (%s s) - the log above shows how far it got", name, (unsigned)code, F(r->ms / 1000.0, 1));
    }
    return code == 0 && !r->timeout;
}

static const char *fl_from_hex(const char *hex)   /* "0xb000" -> "11_0" */
{
    unsigned v = 0; const char *p = hex;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    for (; *p; ++p) { char c = s_lower(*p); v = v * 16 + (unsigned)((c >= 'a') ? (c - 'a' + 10) : (c - '0')); }
    return v ? fl_name(v) : "none";
}
static void print_summary(void)
{
    char v[256], v2[256], v3[256]; int i, n, hw_found = 0;
    lg("");
    lg("==================================================================================");
    lg("SUMMARY (%s %s build)", PROBE_VERSION, BUILD_NAME);
    lg("==================================================================================");
    if (res_get("sys.os", v, sizeof v)) { res_get("sys.arch", v2, sizeof v2); res_get("sys.cpus", v3, sizeof v3); lg("Machine: Windows %s, %s, %s CPU core(s)", v, v2, v3); }

    /* --- GPU --- */
    n = res_int("adapters.count", -1);
    if (n < 0) lg("GPU: UNKNOWN - the adapter test did not finish (see its [run] line above)");
    else if (n == 0) lg("GPU: NO ADAPTER AT ALL (DXGI lists none)");
    else {
        for (i = 0; i < n && !hw_found; ++i) {
            int cls = res_int(K("adapter.%d.class", i), 0);
            if (cls == AC_NVIDIA || cls == AC_HW) {
                res_get(K("adapter.%d.name", i), v, sizeof v); res_get(K("adapter.%d.vendor", i), v2, sizeof v2);
                lg("GPU: %s (%s adapter, vendor %s) - listed by DXGI", v, cls == AC_NVIDIA ? "NVIDIA hardware" : "hardware", v2); hw_found = 1;
            }
        }
        if (!hw_found) { res_get("adapter.0.name", v, sizeof v); lg("GPU: NO GPU DRIVER: software adapter only (\"%s\"%s)", v, n > 1 ? " and others, all software" : ""); }
    }
    if (res_get("fl.hw.adapter", v, sizeof v)) {
        int cls = res_int("fl.hw.class", 0);
        int warp = res_get("mods.hw.warp", v2, sizeof v2) && v2[0] == '1';
        int vendor = res_int("mods.hw.vendor", 0);
        lg("Default HARDWARE device (what our host and Unity get) runs on: \"%s\" -> %s", v, cls ? ac_text(cls) : "?");
        if (cls == AC_L9 && vendor && !warp) lg("  >>> GPU: YES - the GPU driver runs it through Microsoft's D3D10Level9 layer (that layer names its adapter \"Software Adapter\") <<<");
        else if ((cls == AC_SOFT || cls == AC_L9) && (warp || !vendor)) lg("  >>> the 'hardware' device is a SOFTWARE renderer: no GPU acceleration on this OS image <<<");
    }
    if (res_get("mods.hw.warp", v, sizeof v)) lg("d3d10warp.dll loaded by the HARDWARE device creation: %s; GPU vendor driver module loaded: %s", v[0] == '1' ? "YES (software rasteriser)" : "no", res_int("mods.hw.vendor", 0) ? "YES" : "no");
    if (res_get("warpdll", v, sizeof v)) lg("%%SystemRoot%%\\System32\\d3d10warp.dll present: %s", v[0] == '1' ? "yes" : "NO");

    /* --- feature levels --- */
    {
        char hw[64], wp[64], hr1[32], hr2[32];
        if (res_get("fl.hw.level", v, sizeof v) && !s_eq(v, "0")) s_copy(hw, sizeof hw, fl_from_hex(v)); else { res_get("fl.hw.hr", hr1, sizeof hr1); m_snprintf(hw, sizeof hw, "FAILED (hr=%s)", hr1[0] ? hr1 : "test did not run"); }
        if (res_get("fl.warp.level", v, sizeof v) && !s_eq(v, "0")) s_copy(wp, sizeof wp, fl_from_hex(v)); else { res_get("fl.warp.hr", hr2, sizeof hr2); m_snprintf(wp, sizeof wp, "FAILED (hr=%s)", hr2[0] ? hr2 : "test did not run"); }
        lg("True max feature level: HW %s, WARP %s", hw, wp);
        if (res_get("fl.unity.hr", v, sizeof v)) { res_get("fl.unity.level", v2, sizeof v2); lg("Unity's exact first call (EnumAdapters(0), [11_1..10_0], 0x21): hr=%s, level %s%s", v, s_eq(v2, "0") || !v2[0] ? "-" : fl_from_hex(v2), (s_eq(v, "0x00000000")) ? "  (stock game would get this device)" : "  (stock game would FAIL here)"); }
        n = res_int("adapters.count", 0);
        for (i = 0; i < n && i < 9; ++i) {
            if (res_get(K("fl.a%d.level", i), v, sizeof v)) { res_get(K("fl.a%d.hr", i), v2, sizeof v2); lg("  adapter %d with its own device: hr=%s level %s", i, v2, s_eq(v, "0") ? "-" : fl_from_hex(v)); }
        }
        if (res_get("fl.hw.only_9_3", v, sizeof v)) lg("HARDWARE accepts single requests: 9_3=%s 9_2=%s 9_1=%s 10_0=%s", res_int("fl.hw.only_9_3", -1) ? "yes" : "no", res_int("fl.hw.only_9_2", -1) ? "yes" : "no",
                                                    res_int("fl.hw.only_9_1", -1) ? "yes" : "no", res_int("fl.hw.only_10_0", -1) ? "yes" : "no");
    }
    /* --- shaders --- */
    {
        int k; static const char *const tg[2] = { "hw", "warp" };
        for (k = 0; k < 2; ++k) {
            char a[32], b[32], c[32], d[32], e[32], f[32];
            if (!res_get(K("sh.%s.redcon", tg[k]), a, sizeof a)) { lg("Shaders at FL 9_1 on %s: test did not finish", k ? "WARP" : "HARDWARE"); continue; }
            res_get(K("sh.%s.gd", tg[k]), b, sizeof b); res_get(K("sh.%s.gdused", tg[k]), c, sizeof c); res_get(K("sh.%s.bp", tg[k]), d, sizeof d); res_get(K("sh.%s.bporig", tg[k]), e, sizeof e); res_get(K("sh.%s.gdangle", tg[k]), f, sizeof f);
            if (s_eq(a, "0/0")) { lg("[%s {9_1}] NO SHADER DATA FOUND: the data folder is missing next to m0probe.exe - copy the WHOLE folder", k ? "WARP" : "HARDWARE"); continue; }
            lg("[%s {9_1}] Redcon shaders at 9_1: %s", k ? "WARP" : "HARDWARE", a);
            lg("[%s {9_1}] GD 9_3 shaders at 9_1: %s (the 18 containers of the 9 programs the game uses: %s); ANGLE internal level_9_1 shaders: %s", k ? "WARP" : "HARDWARE", b, c, f);
            lg("[%s {9_1}] Bad Piggies replacements at 9_1: %s; Unity originals refused (expected all): %s", k ? "WARP" : "HARDWARE", d, e);
        }
    }
    /* --- render --- */
    {
        char p[64];
        if (res_get("render.draw.pixel", p, sizeof p)) lg("Bad Piggies sprite draw + read-back (replacement layout): pixel %s -> %s", p, res_int("render.draw.ok", 0) ? "OK" : "BAD");
        else lg("Bad Piggies sprite draw: no result (test did not finish)");
        if (res_get("render.il.pixel", p, sizeof p)) lg("  signature-only input layout draw: pixel %s -> %s", p, res_int("render.il.ok", 0) ? "OK" : "BAD");
        if (res_get("render.cboff.pixel", p, sizeof p)) lg("  VSSetConstantBuffers1 (cbuffer offset) draw: pixel %s -> %s", p, res_int("render.cboff.ok", 0) ? "OK" : "BAD");
        else lg("  VSSetConstantBuffers1 draw: skipped or not finished (device reports ConstantBufferOffsetting=%d)", res_int("feat.hw.cboffset", -1));
        if (res_get("render.blend.functional", p, sizeof p)) lg("Redcon blend states drawn and read back correctly: %s", p);
        lg("D3D9_OPTIONS FullNonPow2TextureSupport (HW device): %d (PC: 1)", res_int("feat.hw.fullnpot", -1));
    }
    /* --- vtable --- */
    if (res_get("vt.len", v, sizeof v)) {
        int finished = res_get("status.vt", v2, sizeof v2);
        lg("Device vtable copy: %s, measured vtable length %s slots (PC: 174)", !finished ? "CRASHED OR HUNG before the end" : (res_int("vt.ok", 0) ? "OK" : "FAILED"), v);
        if (res_get("vt.swapchain", v3, sizeof v3)) lg("  swap chain accepted by the driver: %s", v3);
        else lg("  swap chain: NONE of the four descriptions was accepted");
    } else lg("Device vtable copy: no result (test crashed or did not run)");
    /* --- speed --- */
    {
        char h[32], w[32], hb[32];
        int have_h = res_get("speed.hw91.fill_mpix", h, sizeof h), have_w = res_get("speed.warpbest.fill_mpix", w, sizeof w), have_b = res_get("speed.hwbest.fill_mpix", hb, sizeof hb);
        lg("Fill rate (1280x720 alpha-blended quads): HARDWARE {9_1} %s Mpixel/s | HARDWARE best-level %s | WARP best-level %s",
           have_h ? h : "n/a", have_b ? hb : (res_int("speed.hwbest.same_as_hw91", 0) ? "(same device as {9_1})" : "n/a"), have_w ? w : "n/a");
        if (have_h && have_w) {
            double a = 0, b = 0; const char *p = h; unsigned iv = 0; while (*p >= '0' && *p <= '9') iv = iv * 10 + (unsigned)(*p++ - '0'); a = iv;
            p = w; iv = 0; while (*p >= '0' && *p <= '9') iv = iv * 10 + (unsigned)(*p++ - '0'); b = iv;
            if (b > 0) lg("  HARDWARE {9_1} / WARP = %sx %s", F(a / b, 2), (a / b < 1.5) ? "  >>> hardware is NOT clearly faster than WARP: it is probably software <<<" : "(hardware path is clearly faster)");
        }
        if (res_get("speed.hw91.cpu_map_us", v, sizeof v)) { res_get("speed.hw91.cpu_upd_us", v2, sizeof v2); res_get("speed.hw91.cpu_off_us", v3, sizeof v3);
            lg("CPU cost per draw with a constant-buffer update (HARDWARE {9_1}): Map %s us | UpdateSubresource %s us | VSSetConstantBuffers1 windows %s us", v, v2, v3); }
    }
    /* --- per test --- */
    lg("");
    lg("PER-TEST RESULT (PASS = every checked expectation held, FAIL = some did not, INFO = measurements only, CRASH/TIMEOUT = the test process died / hung)");
    for (i = 0; i < g_nruns; ++i) {
        char st[32]; Run *r = &g_runs[i]; char key[64];
        m_snprintf(key, sizeof key, "status.%s", r->name); { int j; for (j = 0; key[j]; ++j) if (key[j] == ' ') key[j] = '.'; }
        if (!r->launched) s_copy(st, sizeof st, "NOT STARTED");
        else if (r->timeout) s_copy(st, sizeof st, "TIMEOUT");
        else if (r->code != 0) m_snprintf(st, sizeof st, "CRASH 0x%08x", (unsigned)r->code);
        else if (!res_get(key, st, sizeof st)) s_copy(st, sizeof st, "no status");
        lg("  %-24s %s", r->name, st);
    }
}

/* ======================================================================================================================
 * entry
 * ==================================================================================================================== */
static int g_arg_quick, g_arg_summary;
static char g_extra[4][32]; static int g_nextra;   /* --extra <test>: run an explicit-only test (idx32, vtshort) after the normal plan */
static void parse_args(void)
{
    const WCHAR *c = GetCommandLineW(); int ntok = 0; char tok[96]; char prev[96]; prev[0] = 0;
    g_arg_test[0] = g_arg_case[0] = 0; g_arg_quick = 0; g_arg_summary = 0; g_arg_nospawn = 0; g_nextra = 0;
    for (;;) {
        int n = 0;
        while (*c == L' ') ++c;
        if (!*c) break;
        if (*c == L'"') { ++c; while (*c && *c != L'"') { if (n < 95) tok[n++] = *c < 128 ? (char)*c : '?'; ++c; } if (*c) ++c; }
        else { while (*c && *c != L' ') { if (n < 95) tok[n++] = *c < 128 ? (char)*c : '?'; ++c; } }
        tok[n] = 0;
        if (ntok > 0) {
            if (s_eq(prev, "--test")) s_copy(g_arg_test, sizeof g_arg_test, tok);
            else if (s_eq(prev, "--case")) s_copy(g_arg_case, sizeof g_arg_case, tok);
            else if (s_eq(prev, "--extra")) { if (g_nextra < 4) s_copy(g_extra[g_nextra++], sizeof g_extra[0], tok); }
            else if (s_eq(tok, "--quick")) g_arg_quick = 1;
            else if (s_eq(tok, "--no-spawn")) g_arg_nospawn = 1;
            else if (s_eq(tok, "--summary-only")) g_arg_summary = 1;   /* re-print the SUMMARY from an existing m0probe.res (for testing) */
        }
        s_copy(prev, sizeof prev, tok); ++ntok;
    }
}
/* PC/ARM proof of the own formatter and the DXBC checksum code (hidden: m0probe.exe --test selftest_fmt) */
static void t_selftest_fmt(void)
{
    char b[128]; int bad = 0, i;
    struct { const char *got; const char *want; } t[16];
    test_begin("selftest: formatter", "selftest_fmt");
    m_snprintf(b, sizeof b, "%llu", 0xFFFFFFFFFFFFFFFFull); t[0].got = K("%s", b); t[0].want = "18446744073709551615";
    m_snprintf(b, sizeof b, "%llu|%llu|%llu", 4294967296ull, 10000000000ull, 1234567890123456789ull); t[1].got = K("%s", b); t[1].want = "4294967296|10000000000|1234567890123456789";
    m_snprintf(b, sizeof b, "%lld|%d|%u", -9000000000ll, -42, 4000000000u); t[2].got = K("%s", b); t[2].want = "-9000000000|-42|4000000000";
    m_snprintf(b, sizeof b, "%08x|%X|%x|%#x", 0xBEEFu, 0xabcdefu, 0u, 255u); t[3].got = K("%s", b); t[3].want = "0000beef|ABCDEF|0|ff";
    m_snprintf(b, sizeof b, "[%5d][%-5d][%05d][%.3s][%8s][%-8s]", 42, 42, 42, "abcdef", "ab", "ab"); t[4].got = K("%s", b); t[4].want = "[   42][42   ][00042][abc][      ab][ab      ]";
    m_snprintf(b, sizeof b, "%s|%s|%s|%s", F(3.14159, 2), F(0.5, 0), F(2.999, 2), F(1234.5678, 1)); t[5].got = K("%s", b); t[5].want = "3.14|1|3.00|1234.6";
    m_snprintf(b, sizeof b, "%s|%s|%s", F(0.0, 3), F(-1.25, 1), F(99999.9996, 3)); t[6].got = K("%s", b); t[6].want = "0.000|-1.3|100000.000";
    m_snprintf(b, sizeof b, "%ls|%c|%%|%*d|%.*s", L"wide", 'x', 4, 7, 2, "xyz"); t[7].got = K("%s", b); t[7].want = "wide|x|%|   7|xy";
    for (i = 0; i < 8; ++i) {
        int ok = s_eq(t[i].got, t[i].want);
        lg("  [%s] got \"%s\" want \"%s\"", ok ? " ok " : "FAIL", t[i].got, t[i].want);
        if (!ok) bad++;
    }
    {   /* DXBC checksum: build the checksum of a container made from a known one and compare */
        static u8 blob[256]; u8 sum[16]; unsigned sz = 0; static const u8 isgn[8] = { 0, 0, 0, 0, 8, 0, 0, 0 };
        memcpy(blob, "DXBC", 4); memset(blob + 4, 0, 16); wr32(blob + 20, 1); wr32(blob + 24, 36 + 8 + 8); wr32(blob + 28, 1); wr32(blob + 32, 36);
        memcpy(blob + 36, "ISGN", 4); wr32(blob + 40, 8); memcpy(blob + 44, isgn, 8); sz = 52;
        int ok;
        dx_checksum(blob, sz, sum);
        /* reference value from the Python implementation that reproduced the checksum of 78 real DXBC files */
        ok = rd32(sum) == 0x77a3f937u && rd32(sum + 4) == 0xf7adc59eu && rd32(sum + 8) == 0x14328d51u && rd32(sum + 12) == 0x218ad709u;
        lg("  [%s] DXBC checksum of a made-up 52-byte container: %08x %08x %08x %08x (reference 77a3f937 f7adc59e 14328d51 218ad709)", ok ? " ok " : "FAIL",
           (unsigned)rd32(sum), (unsigned)rd32(sum + 4), (unsigned)rd32(sum + 8), (unsigned)rd32(sum + 12));
        if (!ok) bad++;
    }
    if (bad) g_fail += bad; else g_pass++;
    test_end();
}
static void child_main(void)
{
    const char *t = g_arg_test, *c = g_arg_case;
    SetUnhandledExceptionFilter(crash_filter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);   /* no WER / critical-error dialogs in the children */
    s_copy(g_tkey, sizeof g_tkey, t);
    if (s_eq(t, "adapters")) t_adapters();
    else if (s_eq(t, "fl")) t_fl(c[0] ? c : "hw");
    else if (s_eq(t, "shaders")) t_shaders(c[0] ? c : "hw");
    else if (s_eq(t, "render")) t_render(c[0] ? c : "draw");
    else if (s_eq(t, "vt")) t_vt(0);
    else if (s_eq(t, "vtshort")) t_vt(1);
    else if (s_eq(t, "speed")) t_speed(c[0] ? c : "hw91");
    else if (s_eq(t, "idx32")) t_idx32();
    else if (s_eq(t, "selftest_fmt")) t_selftest_fmt();
    else if (s_eq(t, "selftest_crash")) { lg("selftest: about to dereference NULL"); *(volatile int *)0 = 1; }   /* PC proof that the parent survives a crash */
    else if (s_eq(t, "selftest_hang")) { lg("selftest: hanging"); for (;;) Sleep(1000); }                          /* PC proof of the timeout path */
    else { lg("unknown test \"%s\"", t); }
}
static void parent_main(void)
{
    WCHAR p1[MAX_PATH], p2[MAX_PATH]; int n, i; char v[32];
    if (g_arg_summary) { open_log(0); print_summary(); return; }
    wpath(p1, L"m0probe.log"); wpath(p2, L"m0probe.log.old");
    MoveFileExW(p1, p2, MOVEFILE_REPLACE_EXISTING);
    open_log(1);
    lg("==================================================================================");
    lg("m0probe - Direct3D device probe for the Surface RT / Surface 2 game ports");
    lg("Log file: %ls (every line is also shown on the console; please send this file back)", p1);
    lg("==================================================================================");
    lg("");
    if (g_log == INVALID_HANDLE_VALUE) lg("WARNING: cannot create m0probe.log in %ls (error %u) - copy this folder to a writable place such as the Desktop and run it again", g_dir, (unsigned)g_log_err);
    t_sys();
    run_child("adapters", NULL, 60);
    run_child("fl", "hw", 60);
    n = res_int("adapters.count", 0); if (n > 6) n = 6;
    for (i = 0; i < n; ++i) { char cs[8]; m_snprintf(cs, sizeof cs, "a%d", i); run_child("fl", cs, 60); }
    run_child("fl", "warp", 60);
    run_child("fl", "unity", 60);
    run_child("fl", "hwsingle", 90);
    run_child("fl", "warpsingle", 90);
    run_child("shaders", "hw", 90);
    run_child("shaders", "warp", 90);
    run_child("render", "draw", 60);
    run_child("render", "il", 60);
    run_child("render", "cboff", 60);
    run_child("render", "blend", 60);
    run_child("render", "table", 60);
    run_child("vt", NULL, 60);
    if (!g_arg_quick) {
        run_child("speed", "hw91", 90);
        run_child("speed", "hwbest", 90);
        run_child("speed", "warpbest", 90);
    }
    for (i = 0; i < g_nextra; ++i) run_child(g_extra[i], NULL, s_eq(g_extra[i], "selftest_hang") ? 5 : 60);
    (void)v;
    print_summary();
    lg("");
    lg("Done. Please send back m0probe.log (and nothing else is needed).");
}
void mainCRTStartup(void)
{
    init_paths();
    g_con = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_con == NULL) g_con = INVALID_HANDLE_VALUE;
    parse_args();
    if (g_arg_test[0]) { open_log(0); child_main(); }
    else parent_main();
    ExitProcess(0);
}
