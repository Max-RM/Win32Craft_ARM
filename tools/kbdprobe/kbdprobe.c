/* kbdprobe.c - touch keyboard (TabTip) lab for Windows RT 8.1 / Windows 10 ARM32 (freestanding, no CRT).
 *
 * Question it answers (one device run, one log file): which signal tells the truth about "the touch keyboard is on screen",
 * and which way of hiding it actually works, for a window that has NO native text control (the game window of Win32Craft).
 * TelegArm proved on the same device that IsWindowVisible(IPTip_Main_Window) lies, the TabTip rectangle never changes, the DWM
 * cloak flag is session dependent and the WinRT InputPane interop is unavailable on 8.1.  The only untried candidate is the
 * desktop shell API IFrameworkInputPane (Location + Showing/Hiding notifications), which this tool logs next to the others.
 *
 * Usage on the device (touch only is fine - the buttons are big):
 *   1. start kbdprobe.exe (it opens maximised).  Log: kbdprobe.log next to the exe (also shown in the window).
 *   2. tap the grey SURFACE (it behaves like the game window: takes focus, no native EDIT), tap  SHOW: launch TabTip.exe,
 *      type "ab", then tap the keyboard's X.  Tap  "Keyboard IS visible"  or  "Keyboard is NOT visible"  after EVERY step -
 *      the log then stores your answer next to every signal at that moment.
 *   3. repeat with each HIDE button (after a SHOW), then with the native EDIT box (Windows shows the keyboard by itself there).
 * Send kbdprobe.log back. */

#include <windows.h>
#include <shobjidl.h>
#include <tlhelp32.h>

/* ---------- no-CRT helpers (clang may emit calls to these) ---------- */
#if defined(_M_X64) || defined(_M_IX86)
int _fltused = 0;   /* x86/x64 smoke-test builds only; ARM does not reference it */
#endif
void *memset(void *d, int c, size_t n) { unsigned char *p = (unsigned char *)d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *p = (unsigned char *)d; const unsigned char *q = (const unsigned char *)s; while (n--) *p++ = *q++; return d; }
int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    while (n--) { if (*p != *q) return *p < *q ? -1 : 1; ++p; ++q; }
    return 0;
}

enum {
    ID_SHOW_LAUNCH = 101, ID_SHOW_TOGGLE, ID_SHOW_IP, ID_HIDE_IP, ID_HIDE_CLOSE, ID_HIDE_TOGGLE, ID_HIDE_KILL, ID_HIDE_SWHIDE, ID_HIDE_BLUR,
    ID_YES, ID_NO, ID_SNAP, ID_EDIT, ID_LOG, ID_NEUTRAL
};

static HWND g_main, g_surface, g_edit, g_log, g_neutral, g_buttons[16];
static int g_button_count;
static HANDLE g_file = INVALID_HANDLE_VALUE;
static DWORD g_start;
static HFONT g_font;
static int g_typed;
static char g_typed_text[64];

typedef BOOL (WINAPI *GetCurrentInputMessageSourceFn)(void *);
typedef HRESULT (WINAPI *DwmGetWindowAttributeFn)(HWND, DWORD, void *, DWORD);
typedef struct { int deviceType; int originId; } InputSource;

/* ---------- logging ---------- */
static void log_line(const char *text)
{
    char line[900];
    DWORD written;
    DWORD ms = GetTickCount() - g_start;
    int length = wsprintfA(line, "[%6lu] %s\r\n", (unsigned long)ms, text);
    if (g_file != INVALID_HANDLE_VALUE) WriteFile(g_file, line, (DWORD)length, &written, NULL);
    if (g_log) {
        int len = GetWindowTextLengthA(g_log);
        if (len > 28000) { SendMessageA(g_log, EM_SETSEL, 0, 12000); SendMessageA(g_log, EM_REPLACESEL, 0, (LPARAM)""); len = GetWindowTextLengthA(g_log); }
        SendMessageA(g_log, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(g_log, EM_REPLACESEL, 0, (LPARAM)line);
    }
}

static void logf1(const char *format, unsigned long a, unsigned long b, unsigned long c)
{
    char buffer[500];
    wsprintfA(buffer, format, a, b, c);
    log_line(buffer);
}

static const char *class_of(HWND window, char *buffer, int size)
{
    buffer[0] = 0;
    if (!window) { lstrcpyA(buffer, "none"); return buffer; }
    if (!GetClassNameA(window, buffer, size)) lstrcpyA(buffer, "?");
    return buffer;
}

static void input_source_text(char *out)
{
    static GetCurrentInputMessageSourceFn fn;
    static int resolved;
    InputSource source;
    if (!resolved) {
        HMODULE user32 = GetModuleHandleA("user32.dll");
        fn = user32 ? (GetCurrentInputMessageSourceFn)GetProcAddress(user32, "GetCurrentInputMessageSource") : NULL;
        resolved = 1;
    }
    if (fn && fn(&source)) wsprintfA(out, "dev%d/org%d", source.deviceType, source.originId);
    else lstrcpyA(out, "src?");
}

/* ---------- signals ---------- */
typedef struct Sig {
    int tip_exists, tip_visible, tip_cloaked, tip_procs;
    RECT tip_rect;
    DWORD tip_style, tip_exstyle;
    int fip_ok;
    RECT fip_rect;
    int ip_ok;
    float ip_rect[4];
    HWND foreground, focus;
} Sig;

static void **g_ip;     /* WinRT Windows.UI.ViewManagement.InputPane (IInputPane) for the main HWND, via IInputPaneInterop */
static void **g_ip2;    /* same object as IInputPane2 (TryShow / TryHide) */
static IFrameworkInputPane *g_fip;
static DWORD g_fip_cookie;
static int g_fip_events;
static Sig g_last_sig;
static int g_have_last;

static int count_tabtip_processes(int terminate, DWORD *error)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry;
    int count = 0;
    if (error) *error = 0;
    if (snapshot == INVALID_HANDLE_VALUE) { if (error) *error = GetLastError(); return -1; }
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (lstrcmpiW(entry.szExeFile, L"TabTip.exe") == 0) {
                ++count;
                if (terminate) {
                    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                    if (process) {
                        if (!TerminateProcess(process, 0) && error) *error = GetLastError();
                        CloseHandle(process);
                    } else if (error) {
                        *error = GetLastError();
                    }
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count;
}

static void sample(Sig *sig)
{
    static DwmGetWindowAttributeFn dwm;
    static int dwm_resolved;
    HWND tip = FindWindowW(L"IPTip_Main_Window", NULL);
    GUITHREADINFO info;

    memset(sig, 0, sizeof(*sig));
    if (!dwm_resolved) {
        HMODULE module = LoadLibraryW(L"dwmapi.dll");
        dwm = module ? (DwmGetWindowAttributeFn)GetProcAddress(module, "DwmGetWindowAttribute") : NULL;
        dwm_resolved = 1;
    }
    sig->tip_exists = tip != NULL;
    if (tip) {
        DWORD cloaked = 0;
        sig->tip_visible = IsWindowVisible(tip) ? 1 : 0;
        if (dwm && dwm(tip, 14 /* DWMWA_CLOAKED */, &cloaked, sizeof(cloaked)) == 0) sig->tip_cloaked = (int)cloaked; else sig->tip_cloaked = -1;
        GetWindowRect(tip, &sig->tip_rect);
        sig->tip_style = (DWORD)GetWindowLongW(tip, GWL_STYLE);
        sig->tip_exstyle = (DWORD)GetWindowLongW(tip, GWL_EXSTYLE);
    }
    sig->tip_procs = count_tabtip_processes(0, NULL);
    if (g_fip) {
        sig->fip_ok = SUCCEEDED(g_fip->lpVtbl->Location(g_fip, &sig->fip_rect)) ? 1 : -1;
    }
    if (g_ip) {
        float rect[4] = { 0, 0, 0, 0 };
        HRESULT hr = ((HRESULT(WINAPI *)(void *, float *))((void **)g_ip[0])[10])(g_ip, rect);
        sig->ip_ok = SUCCEEDED(hr) ? 1 : -1;
        if (SUCCEEDED(hr)) { sig->ip_rect[0] = rect[0]; sig->ip_rect[1] = rect[1]; sig->ip_rect[2] = rect[2]; sig->ip_rect[3] = rect[3]; }
    }
    sig->foreground = GetForegroundWindow();
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (GetGUIThreadInfo(0, &info)) sig->focus = info.hwndFocus;
}

static void sig_text(const Sig *sig, char *out)
{
    char fg[48], fo[48];
    wsprintfA(out, "tip=%d vis=%d cloak=%d procs=%d rect=(%ld,%ld,%ld,%ld) style=%08lx ex=%08lx | fip=%d(%ld,%ld,%ld,%ld) | ip=%d(%d,%d,%d,%d) | fg=%s focus=%s",
              sig->tip_exists, sig->tip_visible, sig->tip_cloaked, sig->tip_procs, (long)sig->tip_rect.left, (long)sig->tip_rect.top,
              (long)sig->tip_rect.right, (long)sig->tip_rect.bottom, (unsigned long)sig->tip_style, (unsigned long)sig->tip_exstyle,
              sig->fip_ok, (long)sig->fip_rect.left, (long)sig->fip_rect.top, (long)sig->fip_rect.right, (long)sig->fip_rect.bottom,
              sig->ip_ok, (int)sig->ip_rect[0], (int)sig->ip_rect[1], (int)sig->ip_rect[2], (int)sig->ip_rect[3],
              class_of(sig->foreground, fg, sizeof(fg)), class_of(sig->focus, fo, sizeof(fo)));
}

static void snapshot_log(const char *tag)
{
    Sig sig;
    char text[700], line[760];
    sample(&sig);
    sig_text(&sig, text);
    wsprintfA(line, "%s %s", tag, text);
    log_line(line);
}

static void sample_tick(void)
{
    Sig sig;
    sample(&sig);
    if (!g_have_last || memcmp(&sig, &g_last_sig, sizeof(sig)) != 0) {
        char text[700], line[760];
        sig_text(&sig, text);
        wsprintfA(line, "SIG %s", text);
        log_line(line);
        g_last_sig = sig;
        g_have_last = 1;
    }
}

/* ---------- IFrameworkInputPaneHandler ---------- */
static HRESULT STDMETHODCALLTYPE h_query(IFrameworkInputPaneHandler *self, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IFrameworkInputPaneHandler)) { *out = self; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE h_addref(IFrameworkInputPaneHandler *self) { (void)self; return 2; }
static ULONG STDMETHODCALLTYPE h_release(IFrameworkInputPaneHandler *self) { (void)self; return 1; }
static HRESULT STDMETHODCALLTYPE h_showing(IFrameworkInputPaneHandler *self, RECT *rect, BOOL ensure)
{
    char line[200];
    (void)self;
    ++g_fip_events;
    wsprintfA(line, "FIP.Showing rect=(%ld,%ld,%ld,%ld) ensure=%d", rect ? (long)rect->left : -1, rect ? (long)rect->top : -1,
              rect ? (long)rect->right : -1, rect ? (long)rect->bottom : -1, (int)ensure);
    log_line(line);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE h_hiding(IFrameworkInputPaneHandler *self, BOOL ensure)
{
    char line[80];
    (void)self;
    ++g_fip_events;
    wsprintfA(line, "FIP.Hiding ensure=%d", (int)ensure);
    log_line(line);
    return S_OK;
}
static IFrameworkInputPaneHandlerVtbl g_handler_vtbl = { h_query, h_addref, h_release, h_showing, h_hiding };
static IFrameworkInputPaneHandler g_handler = { &g_handler_vtbl };

static void fip_init(void)
{
    HRESULT hr = CoCreateInstance(&CLSID_FrameworkInputPane, NULL, CLSCTX_INPROC_SERVER, &IID_IFrameworkInputPane, (void **)&g_fip);
    char line[200];
    wsprintfA(line, "IFrameworkInputPane CoCreateInstance hr=0x%08lx obj=%p", (unsigned long)hr, (void *)g_fip);
    log_line(line);
    if (FAILED(hr) || !g_fip) { g_fip = NULL; return; }
    hr = g_fip->lpVtbl->AdviseWithHWND(g_fip, g_main, &g_handler, &g_fip_cookie);
    wsprintfA(line, "IFrameworkInputPane::AdviseWithHWND hr=0x%08lx cookie=%lu", (unsigned long)hr, (unsigned long)g_fip_cookie);
    log_line(line);
}

/* ---------- WinRT InputPane for an HWND (IInputPaneInterop): works on Windows 10, TelegArm saw it fail on RT 8.1 ---------- */
static void ip_init(void)
{
    typedef HRESULT (WINAPI *RoGetActivationFactoryFn)(void *, const GUID *, void **);
    typedef HRESULT (WINAPI *WindowsCreateStringFn)(const WCHAR *, UINT32, void **);
    typedef HRESULT (WINAPI *WindowsDeleteStringFn)(void *);
    static const GUID iid_interop = { 0x75CF2C57U, 0x9195, 0x4931, { 0x83, 0x32, 0xF0, 0xB4, 0x09, 0xE9, 0x16, 0xAF } };
    static const GUID iid_input_pane = { 0x640ada70U, 0x06f3, 0x4c87, { 0xa6, 0x78, 0x98, 0x29, 0xc9, 0x12, 0x7c, 0x28 } };
    static const GUID iid_input_pane2 = { 0x8a6b3f26U, 0x7090, 0x4793, { 0x94, 0x4c, 0xc3, 0xf2, 0xcd, 0xe2, 0x62, 0x76 } };
    static const WCHAR class_name[] = L"Windows.UI.ViewManagement.InputPane";
    HMODULE combase = LoadLibraryW(L"combase.dll");
    RoGetActivationFactoryFn get_factory = combase ? (RoGetActivationFactoryFn)GetProcAddress(combase, "RoGetActivationFactory") : NULL;
    WindowsCreateStringFn create = combase ? (WindowsCreateStringFn)GetProcAddress(combase, "WindowsCreateString") : NULL;
    WindowsDeleteStringFn destroy = combase ? (WindowsDeleteStringFn)GetProcAddress(combase, "WindowsDeleteString") : NULL;
    void *name = NULL;
    void **factory = NULL;
    void **pane = NULL;
    HRESULT hr;
    char line[220];

    if (!get_factory || !create || !destroy) { log_line("InputPane: combase.dll WinRT entry points missing"); return; }
    hr = create(class_name, (UINT32)(sizeof(class_name) / sizeof(WCHAR) - 1), &name);
    if (FAILED(hr)) { logf1("InputPane: WindowsCreateString hr=0x%08lx", (unsigned long)hr, 0, 0); return; }
    hr = get_factory(name, &iid_interop, (void **)&factory);
    destroy(name);
    wsprintfA(line, "InputPane: RoGetActivationFactory(IInputPaneInterop) hr=0x%08lx factory=%p", (unsigned long)hr, (void *)factory);
    log_line(line);
    if (FAILED(hr) || !factory) return;
    hr = ((HRESULT(WINAPI *)(void *, HWND, const GUID *, void **))((void **)factory[0])[6])(factory, g_main, &iid_input_pane, (void **)&pane);
    wsprintfA(line, "InputPane: IInputPaneInterop::GetForWindow hr=0x%08lx pane=%p", (unsigned long)hr, (void *)pane);
    log_line(line);
    if (FAILED(hr) || !pane) return;
    g_ip = pane;
    hr = ((HRESULT(WINAPI *)(void *, const GUID *, void **))((void **)pane[0])[0])(pane, &iid_input_pane2, (void **)&g_ip2);
    wsprintfA(line, "InputPane: QueryInterface(IInputPane2 TryShow/TryHide) hr=0x%08lx", (unsigned long)hr);
    log_line(line);
    if (FAILED(hr)) g_ip2 = NULL;
}

static void act_ip(int show)
{
    BYTE result = 0;
    HRESULT hr;
    char line[160];
    if (!g_ip2) { log_line(show ? "ACTION show/InputPane2::TryShow unavailable (no InputPane)" : "ACTION hide/InputPane2::TryHide unavailable (no InputPane)"); return; }
    hr = ((HRESULT(WINAPI *)(void *, BYTE *))((void **)g_ip2[0])[show ? 6 : 7])(g_ip2, &result);
    wsprintfA(line, "ACTION %s hr=0x%08lx result=%d", show ? "show/InputPane2::TryShow" : "hide/InputPane2::TryHide", (unsigned long)hr, (int)result);
    log_line(line);
}

/* ---------- TabTip actions ---------- */
static void act_launch(void)
{
    WCHAR path[MAX_PATH];
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    BOOL ok;
    char line[300];
    ExpandEnvironmentStringsW(L"%CommonProgramFiles%\\microsoft shared\\ink\\TabTip.exe", path, MAX_PATH);
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    ok = CreateProcessW(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process);
    wsprintfA(line, "ACTION show/launch TabTip.exe ok=%d error=%lu", (int)ok, ok ? 0ul : (unsigned long)GetLastError());
    log_line(line);
    if (ok) { CloseHandle(process.hThread); CloseHandle(process.hProcess); }
}

static void act_toggle(const char *why)
{
    static const GUID clsid = { 0x4ce576faU, 0x83dc, 0x4f88, { 0x95, 0x1c, 0x9d, 0x07, 0x82, 0xb4, 0xe3, 0x76 } };
    static const GUID iid = { 0x37c994e7U, 0x432b, 0x4834, { 0xa2, 0xf7, 0xdc, 0xe1, 0xf1, 0x3b, 0x83, 0x4b } };
    void **object = NULL;
    HRESULT hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER, &iid, (void **)&object);
    char line[200];
    if (FAILED(hr) || !object) {
        wsprintfA(line, "ACTION %s ITipInvocation CoCreateInstance failed hr=0x%08lx", why, (unsigned long)hr);
        log_line(line);
        return;
    }
    hr = ((HRESULT(WINAPI *)(void *, HWND))((void **)*object)[3])(object, GetDesktopWindow());
    ((ULONG(WINAPI *)(void *))((void **)*object)[2])(object);
    wsprintfA(line, "ACTION %s ITipInvocation::Toggle hr=0x%08lx", why, (unsigned long)hr);
    log_line(line);
}

static void act_close(void)
{
    HWND tip = FindWindowW(L"IPTip_Main_Window", NULL);
    char line[160];
    BOOL ok = tip ? PostMessageW(tip, WM_SYSCOMMAND, SC_CLOSE, 0) : FALSE;
    wsprintfA(line, "ACTION hide/SC_CLOSE tip=%p posted=%d", (void *)tip, (int)ok);
    log_line(line);
}

static void act_swhide(void)
{
    HWND tip = FindWindowW(L"IPTip_Main_Window", NULL);
    char line[160];
    BOOL was = tip ? ShowWindow(tip, SW_HIDE) : FALSE;
    wsprintfA(line, "ACTION hide/ShowWindow(SW_HIDE) tip=%p wasVisible=%d error=%lu", (void *)tip, (int)was, (unsigned long)GetLastError());
    log_line(line);
}

static void act_kill(void)
{
    DWORD error = 0;
    int count = count_tabtip_processes(1, &error);
    logf1("ACTION hide/terminate TabTip.exe processes=%lu lastError=%lu", (unsigned long)count, (unsigned long)error, 0);
}

static void act_blur(void)
{
    SetFocus(g_neutral);
    log_line("ACTION hide/blur: SetFocus(neutral button)");
}

/* ---------- windows ---------- */
static void log_pointer(const char *what, LPARAM lparam)
{
    char source[40], line[160];
    input_source_text(source);
    wsprintfA(line, "%s at (%d,%d) %s extra=%08lx", what, (int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam), source,
              (unsigned long)GetMessageExtraInfo());
    log_line(line);
}

static LRESULT CALLBACK surface_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    char line[200];
    switch (message) {
    case WM_LBUTTONDOWN:
        log_pointer("SURFACE WM_LBUTTONDOWN", lparam);
        SetFocus(window);
        return 0;
    case WM_POINTERDOWN:
        log_pointer("SURFACE WM_POINTERDOWN", lparam);
        break;
    case WM_TOUCH:
        logf1("SURFACE WM_TOUCH inputs=%lu", (unsigned long)LOWORD(wparam), 0, 0);
        CloseTouchInputHandle((HTOUCHINPUT)lparam);
        return 0;
    case WM_SETFOCUS: log_line("SURFACE WM_SETFOCUS"); InvalidateRect(window, NULL, TRUE); return 0;
    case WM_KILLFOCUS: log_line("SURFACE WM_KILLFOCUS"); InvalidateRect(window, NULL, TRUE); return 0;
    case WM_KEYDOWN:
        wsprintfA(line, "SURFACE WM_KEYDOWN vk=0x%02lx", (unsigned long)wparam);
        log_line(line);
        return 0;
    case WM_CHAR: {
        char source[40];
        input_source_text(source);
        wsprintfA(line, "SURFACE WM_CHAR 0x%04lx (%s)", (unsigned long)wparam, source);
        log_line(line);
        if (wparam >= 32 && wparam < 127 && g_typed < (int)sizeof(g_typed_text) - 1) { g_typed_text[g_typed++] = (char)wparam; g_typed_text[g_typed] = 0; }
        InvalidateRect(window, NULL, TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint;
        RECT rect;
        char text[200];
        HDC dc = BeginPaint(window, &paint);
        GetClientRect(window, &rect);
        FillRect(dc, &rect, (HBRUSH)GetStockObject(GetFocus() == window ? LTGRAY_BRUSH : GRAY_BRUSH));
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_font);
        wsprintfA(text, "SURFACE (game-like, no native EDIT)  focus=%d  typed: %s", GetFocus() == window ? 1 : 0, g_typed_text);
        DrawTextA(dc, text, -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK);
        EndPaint(window, &paint);
        return 0;
    }
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

static HWND add_button(HWND parent, const char *title, int id)
{
    HWND button = CreateWindowExA(0, "BUTTON", title, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_MULTILINE, 0, 0, 10, 10,
                                  parent, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
    SendMessageA(button, WM_SETFONT, (WPARAM)g_font, TRUE);
    if (g_button_count < 16) g_buttons[g_button_count++] = button;
    return button;
}

static void layout(void)
{
    RECT client;
    int width, height, column, row, bw, bh, top, log_height, i;
    GetClientRect(g_main, &client);
    width = client.right;
    height = client.bottom;
    bh = 56;
    bw = width / 3;
    top = 0;
    for (i = 0; i < g_button_count; ++i) {
        column = i % 3;
        row = i / 3;
        MoveWindow(g_buttons[i], column * bw, top + row * bh, bw - 2, bh - 2, TRUE);
    }
    top += ((g_button_count + 2) / 3) * bh + 4;
    log_height = height / 3;
    MoveWindow(g_surface, 0, top, width / 2 - 2, 90, TRUE);
    MoveWindow(g_edit, width / 2, top, width / 2, 90, TRUE);
    top += 96;
    MoveWindow(g_log, 0, top, width, height - top, TRUE);
    (void)log_height;
}

static LRESULT CALLBACK main_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    char line[260];
    switch (message) {
    case WM_CREATE:
        g_main = window;
        g_font = CreateFontA(-22, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        add_button(window, "SHOW: launch TabTip.exe", ID_SHOW_LAUNCH);
        add_button(window, "SHOW: ITipInvocation::Toggle", ID_SHOW_TOGGLE);
        add_button(window, "SHOW: InputPane2::TryShow", ID_SHOW_IP);
        add_button(window, "HIDE: InputPane2::TryHide", ID_HIDE_IP);
        g_neutral = add_button(window, "(neutral button - blur target)", ID_NEUTRAL);
        add_button(window, "HIDE: SC_CLOSE to tip window", ID_HIDE_CLOSE);
        add_button(window, "HIDE: ITipInvocation::Toggle", ID_HIDE_TOGGLE);
        add_button(window, "HIDE: terminate TabTip.exe", ID_HIDE_KILL);
        add_button(window, "HIDE: ShowWindow(SW_HIDE)", ID_HIDE_SWHIDE);
        add_button(window, "HIDE: blur (SetFocus neutral)", ID_HIDE_BLUR);
        add_button(window, "Snapshot signals", ID_SNAP);
        add_button(window, "Keyboard IS visible", ID_YES);
        add_button(window, "Keyboard is NOT visible", ID_NO);
        g_surface = CreateWindowExA(WS_EX_CLIENTEDGE, "KbdProbeSurface", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 10, 10, window, NULL, GetModuleHandleA(NULL), NULL);
        g_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "native EDIT: tap here", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, window, (HMENU)(INT_PTR)ID_EDIT, GetModuleHandleA(NULL), NULL);
        SendMessageA(g_edit, WM_SETFONT, (WPARAM)g_font, TRUE);
        g_log = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 10, 10, window, (HMENU)(INT_PTR)ID_LOG, GetModuleHandleA(NULL), NULL);
        SendMessageA(g_log, WM_SETFONT, (WPARAM)g_font, TRUE);
        RegisterTouchWindow(g_surface, 0);
        return 0;
    case WM_SIZE:
        if (g_surface) layout();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wparam);
        int code = HIWORD(wparam);
        if (id == ID_EDIT) {
            if (code == EN_SETFOCUS) log_line("EDIT EN_SETFOCUS");
            else if (code == EN_KILLFOCUS) log_line("EDIT EN_KILLFOCUS");
            return 0;
        }
        if (code != BN_CLICKED) break;
        switch (id) {
        case ID_SHOW_LAUNCH: act_launch(); break;
        case ID_SHOW_TOGGLE: act_toggle("show"); break;
        case ID_SHOW_IP: act_ip(1); break;
        case ID_HIDE_IP: act_ip(0); break;
        case ID_HIDE_CLOSE: act_close(); break;
        case ID_HIDE_TOGGLE: act_toggle("hide"); break;
        case ID_HIDE_KILL: act_kill(); break;
        case ID_HIDE_SWHIDE: act_swhide(); break;
        case ID_HIDE_BLUR: act_blur(); break;
        case ID_SNAP: snapshot_log("SNAPSHOT"); break;
        case ID_YES: snapshot_log("ORACLE keyboard=VISIBLE"); break;
        case ID_NO: snapshot_log("ORACLE keyboard=NOT-VISIBLE"); break;
        }
        return 0;
    }
    case WM_ACTIVATE: logf1("MAIN WM_ACTIVATE state=%lu minimized=%lu", (unsigned long)LOWORD(wparam), (unsigned long)HIWORD(wparam), 0); break;
    case WM_ACTIVATEAPP: logf1("MAIN WM_ACTIVATEAPP active=%lu", (unsigned long)wparam, 0, 0); break;
    case WM_SETTINGCHANGE:
        wsprintfA(line, "MAIN WM_SETTINGCHANGE wparam=0x%lx lparam=%s", (unsigned long)wparam, lparam && !IsBadReadPtr((void *)lparam, 2) ? "(str)" : "0");
        log_line(line);
        break;
    case WM_TIMER:
        sample_tick();
        return 0;
    case WM_DESTROY:
        if (g_fip && g_fip_cookie) g_fip->lpVtbl->Unadvise(g_fip, g_fip_cookie);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

static void header(void)
{
    char line[300];
    typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOW *);
    RtlGetVersionFn rtl = (RtlGetVersionFn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    OSVERSIONINFOW version;
    memset(&version, 0, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtl) rtl(&version);
    wsprintfA(line, "kbdprobe start: Windows %lu.%lu build %lu", (unsigned long)version.dwMajorVersion, (unsigned long)version.dwMinorVersion,
              (unsigned long)version.dwBuildNumber);
    log_line(line);
    wsprintfA(line, "metrics: convertibleSlate=%d tabletPC=%d digitizer=0x%lx maxTouches=%d mousePresent=%d", GetSystemMetrics(0x2003),
              GetSystemMetrics(86), (unsigned long)GetSystemMetrics(94), GetSystemMetrics(95), GetSystemMetrics(19));
    log_line(line);
    log_line("steps: SURFACE -> SHOW launch -> type ab -> X on keyboard -> answer visible/not visible after EVERY step; repeat with each HIDE; then the native EDIT");
}

int WINAPI probe_main(void)
{
    WNDCLASSA wc;
    MSG message;
    char path[MAX_PATH];
    char *slash;
    HINSTANCE instance = GetModuleHandleA(NULL);

    g_start = GetTickCount();
    GetModuleFileNameA(NULL, path, MAX_PATH);
    slash = path + lstrlenA(path);
    while (slash > path && slash[-1] != '\\') --slash;
    lstrcpyA(slash, "kbdprobe.log");
    g_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = surface_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "KbdProbeSurface";
    RegisterClassA(&wc);
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = main_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "KbdProbeMain";
    RegisterClassA(&wc);

    g_main = CreateWindowExA(0, "KbdProbeMain", "kbdprobe - touch keyboard lab", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 700,
                             NULL, NULL, instance, NULL);
    if (!g_main) return 1;
    header();
    fip_init();
    ip_init();
    snapshot_log("INITIAL");
    SetTimer(g_main, 1, 100, NULL);
    ShowWindow(g_main, SW_SHOWMAXIMIZED);
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    log_line("kbdprobe exit");
    if (g_file != INVALID_HANDLE_VALUE) CloseHandle(g_file);
    return (int)message.wParam;
}

void probe_start(void)
{
    ExitProcess((UINT)probe_main());
}
