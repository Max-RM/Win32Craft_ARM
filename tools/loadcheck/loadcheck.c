/* loadcheck.c - freestanding ARM32 console tool: checks, in ONE run, every DLL import of the Win32Craft files against the
 * real Windows loader of the machine it runs on (Windows RT 8.1 / Windows 10 ARM32).
 *
 *   loadcheck.exe [folder] [/nopause]     default folder = the folder of loadcheck.exe
 *
 * For each module (Win32Craft.1.2.8-arm32.exe, win32craft.dll, fmod_ARM.dll, the VC++ runtime, ucrtbase.dll) it parses the
 * PE import table, LoadLibrary()s every imported DLL (the folder is the DLL directory) and GetProcAddress()es every imported
 * function.  It prints which file actually served each DLL name (an api-set name served from the folder = our shim, served
 * from System32 = the OS knows the contract) and every function that cannot be resolved (this is what makes the Windows
 * "entry point not found" dialogs).  Output goes to the console and to loadcheck.txt in the folder.
 * Imports: KERNEL32 + USER32 (wsprintfA) only; no CRT. */

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HMODULE;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef void *LPVOID;
typedef unsigned int SIZE_T;

#define NULL ((void *)0)
#define INVALID_HANDLE_VALUE ((HANDLE)(int)-1)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1u
#define OPEN_EXISTING 3u
#define CREATE_ALWAYS 2u
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define MEM_RELEASE 0x8000u
#define PAGE_READWRITE 4u
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_INPUT_HANDLE ((DWORD)-10)
#define MAX_PATH 260

extern HANDLE GetStdHandle(DWORD);
extern BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
extern BOOL ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
extern HANDLE CreateFileA(LPCSTR, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
extern BOOL CloseHandle(HANDLE);
extern DWORD GetFileSize(HANDLE, DWORD *);
extern LPVOID VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
extern BOOL VirtualFree(LPVOID, SIZE_T, DWORD);
extern HMODULE LoadLibraryExA(LPCSTR, HANDLE, DWORD);
extern void *GetProcAddress(HMODULE, LPCSTR);
extern HMODULE GetModuleHandleA(LPCSTR);
extern DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD);
extern BOOL SetDllDirectoryA(LPCSTR);
extern DWORD GetLastError(void);
extern DWORD FormatMessageA(DWORD, const void *, DWORD, DWORD, LPSTR, DWORD, void *);
extern LPCSTR GetCommandLineA(void);
extern int lstrlenA(LPCSTR);
extern void ExitProcess(unsigned);
extern int wsprintfA(LPSTR, LPCSTR, ...);

static HANDLE g_out = INVALID_HANDLE_VALUE;
static HANDLE g_file = INVALID_HANDLE_VALUE;
static char g_line[2048];
static char g_folder[MAX_PATH];
static char g_path[MAX_PATH];
static char g_modpath[MAX_PATH];
static char g_msg[256];
static int g_total_fail;
static int g_pause = 1;

#define P(...) do { wsprintfA(g_line, __VA_ARGS__); out(g_line); } while (0)

static void out(const char *s)
{
    DWORD n;
    int len = lstrlenA(s);
    if (g_out != INVALID_HANDLE_VALUE) WriteFile(g_out, s, (DWORD)len, &n, NULL);
    if (g_file != INVALID_HANDLE_VALUE) WriteFile(g_file, s, (DWORD)len, &n, NULL);
}

static DWORD rd32(const BYTE *p) { return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24); }
static DWORD rd16(const BYTE *p) { return (DWORD)p[0] | ((DWORD)p[1] << 8); }

static void error_text(DWORD err)
{
    DWORD n = FormatMessageA(0x1000 | 0x200, NULL, err, 0, g_msg, (DWORD)sizeof(g_msg) - 1, NULL);
    while (n && (g_msg[n - 1] == '\r' || g_msg[n - 1] == '\n' || g_msg[n - 1] == ' ' || g_msg[n - 1] == '.')) --n;
    g_msg[n] = 0;
    if (!n) { g_msg[0] = '?'; g_msg[1] = 0; }
}

/* RVA -> offset using the section table; returns 0xFFFFFFFF when outside the image */
static DWORD rva_to_off(const BYTE *b, DWORD size, DWORD rva)
{
    DWORD pe = rd32(b + 0x3c);
    DWORD nsec = rd16(b + pe + 6);
    DWORD optsz = rd16(b + pe + 20);
    const BYTE *sec = b + pe + 24 + optsz;
    DWORD i;
    for (i = 0; i < nsec; ++i, sec += 40) {
        DWORD va = rd32(sec + 12), vs = rd32(sec + 8), rs = rd32(sec + 16), ro = rd32(sec + 20);
        DWORD ext = vs > rs ? vs : rs;
        if (rva >= va && rva < va + ext) {
            DWORD off = ro + (rva - va);
            return off < size ? off : 0xFFFFFFFFu;
        }
    }
    if (rva < rd32(b + pe + 24 + 60)) return rva < size ? rva : 0xFFFFFFFFu;  /* headers */
    return 0xFFFFFFFFu;
}

static int is_apiset(const char *n)
{
    return (n[0] | 32) == 'a' && (n[1] | 32) == 'p' && (n[2] | 32) == 'i' && n[3] == '-' && (n[4] | 32) == 'm' && (n[5] | 32) == 's' && n[6] == '-';
}

static void check_dll_import(const BYTE *b, DWORD size, DWORD name_off, DWORD thunk_rva, const char *module)
{
    const char *dll = (const char *)(b + name_off);
    HMODULE h;
    DWORD err;
    int fails = 0;
    DWORD off;
    int idx;

    h = LoadLibraryExA(dll, NULL, 0);
    if (!h) {
        err = GetLastError();
        error_text(err);
        P("  [FAIL] %s: LoadLibrary(\"%s\") error %lu: %s\r\n", module, dll, err, g_msg);
        ++g_total_fail;
        return;
    }
    g_modpath[0] = 0;
    GetModuleFileNameA(h, g_modpath, MAX_PATH);
    off = rva_to_off(b, size, thunk_rva);
    if (off == 0xFFFFFFFFu) { P("  [FAIL] %s: bad thunk table for %s\r\n", module, dll); ++g_total_fail; return; }
    for (idx = 0;; ++idx) {
        DWORD ent = rd32(b + off + 4 * (DWORD)idx);
        void *p;
        if (!ent) break;
        if (ent & 0x80000000u) {
            p = GetProcAddress(h, (LPCSTR)(ent & 0xFFFF));
            if (!p) {
                err = GetLastError();
                if (fails < 25) P("  [MISSING] %s imports %s ordinal %lu - not exported by %s (error %lu)\r\n", module, dll, ent & 0xFFFF, g_modpath, err);
                ++fails;
            }
        } else {
            DWORD no = rva_to_off(b, size, (ent & 0x7FFFFFFFu) + 2);
            const char *fn = no == 0xFFFFFFFFu ? "?" : (const char *)(b + no);
            p = GetProcAddress(h, fn);
            if (!p) {
                err = GetLastError();
                if (fails < 25) P("  [MISSING] %s imports %s!%s - not exported by %s (error %lu)\r\n", module, dll, fn, g_modpath, err);
                ++fails;
            }
        }
    }
    if (fails) {
        if (fails > 25) P("  ... %s: %d more missing functions in %s not listed\r\n", module, fails - 25, dll);
        g_total_fail += fails;
    } else if (is_apiset(dll)) {
        P("  [ok]   %-44s served by %s\r\n", dll, g_modpath);
    }
}

static void check_module(const char *name)
{
    HANDLE f;
    DWORD size, got, pe, impdir, off;
    BYTE *b;
    int count = 0;

    wsprintfA(g_path, "%s\\%s", g_folder, name);
    f = CreateFileA(g_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        P("\r\n== %s: not found in %s (skipped)\r\n", name, g_folder);
        return;
    }
    size = GetFileSize(f, NULL);
    b = (BYTE *)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!b || !ReadFile(f, b, size, &got, NULL) || got != size || size < 0x200) {
        P("\r\n== %s: cannot read\r\n", name);
        CloseHandle(f);
        return;
    }
    CloseHandle(f);
    P("\r\n== %s (%lu bytes)\r\n", name, size);
    pe = rd32(b + 0x3c);
    if (b[0] != 'M' || b[1] != 'Z' || pe > size - 0x200 || rd32(b + pe) != 0x4550u) {
        P("  [FAIL] not a PE file\r\n");
        ++g_total_fail;
        VirtualFree(b, 0, MEM_RELEASE);
        return;
    }
    if (rd16(b + pe + 4) != 0x1C4) {
        P("  [FAIL] machine type 0x%X is not ARMNT (0x1C4)\r\n", rd16(b + pe + 4));
        ++g_total_fail;
    }
    impdir = rd32(b + pe + 24 + 96 + 8);
    off = rva_to_off(b, size, impdir);
    if (off == 0xFFFFFFFFu) { P("  (no import directory)\r\n"); VirtualFree(b, 0, MEM_RELEASE); return; }
    for (;; off += 20) {
        DWORD oft = rd32(b + off), nm = rd32(b + off + 12), ft = rd32(b + off + 16);
        DWORD no;
        if (!oft && !nm && !ft) break;
        no = rva_to_off(b, size, nm);
        if (no == 0xFFFFFFFFu) { P("  [FAIL] bad import name RVA\r\n"); ++g_total_fail; break; }
        check_dll_import(b, size, no, oft ? oft : ft, name);
        ++count;
    }
    P("  %d import DLLs checked\r\n", count);
    VirtualFree(b, 0, MEM_RELEASE);
}

static void parse_args(void)
{
    const char *c = GetCommandLineA();
    int n = 0;
    g_folder[0] = 0;
    if (*c == '"') { ++c; while (*c && *c != '"') ++c; if (*c) ++c; } else { while (*c && *c != ' ') ++c; }
    while (*c) {
        const char *s;
        while (*c == ' ') ++c;
        if (!*c) break;
        s = c;
        if (*c == '"') { ++s; ++c; while (*c && *c != '"') ++c; } else { while (*c && *c != ' ') ++c; }
        if (c - s == 8 && (s[0] == '/' || s[0] == '-') && (s[1] | 32) == 'n' && (s[2] | 32) == 'o') { g_pause = 0; }
        else { n = (int)(c - s); if (n > MAX_PATH - 2) n = MAX_PATH - 2; { int i; for (i = 0; i < n; ++i) g_folder[i] = s[i]; } g_folder[n] = 0; }
        if (*c == '"') ++c;
    }
    if (!g_folder[0]) {
        DWORD len = GetModuleFileNameA(NULL, g_folder, MAX_PATH);
        while (len && g_folder[len - 1] != '\\') --len;
        if (len) --len;
        g_folder[len] = 0;
    }
    while (lstrlenA(g_folder) > 3 && g_folder[lstrlenA(g_folder) - 1] == '\\') g_folder[lstrlenA(g_folder) - 1] = 0;
}

void mainCRTStartup(void)
{
    static const char *const modules[] = {
        "Win32Craft.1.2.8-arm32.exe", "win32craft.dll", "fmod_ARM.dll", "ucrtbase.dll",
        "vccorlib140.dll", "msvcp140.dll", "concrt140.dll", "vcruntime140.dll"
    };
    DWORD i;
    struct { DWORD size, major, minor, build, platform; unsigned short csd[128]; } ver;
    long (*rtl_get_version)(void *);

    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    parse_args();
    SetDllDirectoryA(g_folder);
    wsprintfA(g_path, "%s\\loadcheck.txt", g_folder);
    g_file = CreateFileA(g_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

    out("loadcheck - Win32Craft ARM32 dependency check\r\n");
    P("folder: %s\r\n", g_folder);
    rtl_get_version = (long (*)(void *))GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (rtl_get_version) {
        ver.size = (DWORD)sizeof(ver);
        if (rtl_get_version(&ver) == 0) P("Windows version %lu.%lu build %lu\r\n", ver.major, ver.minor, ver.build);
    }
    for (i = 0; i < sizeof(modules) / sizeof(modules[0]); ++i) check_module(modules[i]);

    P("\r\n==== RESULT: %d problem(s) ====\r\n", g_total_fail);
    if (g_total_fail) out("Send loadcheck.txt (it is in the same folder) to the developer.\r\n");
    else out("All imports resolve on this machine.\r\n");
    if (g_pause) {
        DWORD n;
        char c;
        out("\r\nPress Enter to close...\r\n");
        ReadFile(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &n, NULL);
    }
    if (g_file != INVALID_HANDLE_VALUE) CloseHandle(g_file);
    ExitProcess(g_total_fail ? 1u : 0u);
}
