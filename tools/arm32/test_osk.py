#!/usr/bin/env python3
"""Host-side (x86) simulation test of the touch-keyboard (TabTip) controller in win32_host.c:  python tools/arm32/test_osk.py [-v]
Extracts the REAL controller (from "#define ARM32_OSK_SIG_NONE" through arm32_osk_rect_buffer(), up to the "diagnostics: which input mappers" comment), compiles it against a
fake shell (fake clock, fake ITipInvocation::Toggle that shows/hides after 350 ms and fires the IFrameworkInputPane Showing/Hiding callbacks,
fake IFrameworkInputPane::Location, fake window class and SC_CLOSE) and runs scenarios, one process per scenario (the controller is all statics):
  win11_*   Windows 11 as seen in the 22631 log: IPTip_Main_Window exists but is never visible, Location empty, callbacks fire
  win10_*   Windows 10 build 15035 (Surface 2): window always visible, Location accurate - must behave exactly as the probe-verified controller
Exit code 0 = all scenarios passed."""
import os, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
LLVM = Path(os.environ.get("RT_LLVM_BIN", r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"))
VC = Path(os.environ.get("VCLIB_X86", r"D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\x86"))
SDKROOT = Path(os.environ.get("SDKROOT", r"D:\Windows Kits\10"))
SDKVER = os.environ.get("SDKVER", "10.0.19041.0")
OUT = REPO / "arm32" / "work" / "_test_osk"
OUT.mkdir(parents=True, exist_ok=True)
VERBOSE = "-v" in sys.argv

host = Path(os.environ.get("WIN32CRAFT_HOST_C", str(REPO / "win32_host.c"))).read_text(encoding="utf-8", errors="replace")
start = host.index("#define ARM32_OSK_SIG_NONE")
end = host.index("/* ---- diagnostics: which input mappers exist")
block = host[start:end]
if "static BOOL arm32_osk_rect_for_game(float rect[4])\n{" not in block:
    # an older controller (before 2026-10-21 evening) has no finger hold-back: the game got arm32_osk_rect_buffer() directly
    block += "\nstatic BOOL arm32_osk_rect_for_game(float rect[4]) { return arm32_osk_rect_buffer(rect); }\n"

prologue = r'''
#include <windows.h>
#include <stdio.h>
#include <string.h>
#define ARM32_OSK_AUTO 0
#define ARM32_OSK_ON 1
#define ARM32_OSK_OFF 2
static int arm32_osk_policy = ARM32_OSK_AUTO;
static const char *const arm32_osk_policy_names[] = { "auto", "on", "off" };
#define ARM32_WM_SYSCOMMAND 0x0112u
#define ARM32_SC_CLOSE 0xF060u

/* ---------------- the fake shell ---------------- */
static DWORD g_now = 100000;
static int g_verbose, g_win11, g_tip_exists = 1, g_callbacks = 1, g_location_accurate, g_toggle_noop;
static int g_kb_visible, g_toggles, g_closes, g_launched, g_raise_show, g_raise_hide;
static DWORD g_toggle_due, g_close_due, g_last_toggle_at;
static int g_toggle_pending, g_close_pending, g_min_toggle_gap = 100000;
static int g_confirmed = 1, g_unsafe_toggles;   /* a Toggle within 2 s of an UNCONFIRMED Toggle (no shell callback since) could undo it */
static HWND game_window = (HWND)0x1234;
static BOOL text_input_active = TRUE;

static void say(const char *text) { if (g_verbose) { char line[600]; wsprintfA(line, "  [%6lu] %s\n", (unsigned long)(g_now - 100000), text); fputs(line, stdout); } }
static void log_line(const char *text) { say(text); }
static BOOL arm32_input_touch_active(void) { return TRUE; }
static void arm32_input_pane_raise(BOOL showing) { if (showing) ++g_raise_show; else ++g_raise_hide; say(showing ? ">> game: Showing" : ">> game: Hiding"); }
static BOOL arm32_osk_rect_buffer(float rect[4]);
static BOOL arm32_osk_rect_for_game(float rect[4]);
static int g_touch_down;
static BOOL arm32_touch_any_active(void) { return g_touch_down; }
static void arm32_gfx_get_backbuffer_size(UINT *w, UINT *h) { *w = 1920; *h = 1080; }
static BOOL h_GetClientRect(HWND h, RECT *r) { (void)h; r->left = 0; r->top = 0; r->right = 1920; r->bottom = 1080; return TRUE; }
static BOOL h_ScreenToClient(HWND h, POINT *p) { (void)h; (void)p; return TRUE; }
static BOOL append_wide_path(wchar_t *path, const wchar_t *tail) { lstrcatW(path, tail); return TRUE; }
static void arm32_iat_narrow_for_log(char *out, SIZE_T size, const wchar_t *text) { (void)text; if (out && size) lstrcpynA(out, "path", (int)size); }

static void fire_callback(int showing);
static HWND h_FindWindowW(LPCWSTR c, LPCWSTR t) { (void)t; return (c && lstrcmpW(c, L"IPTip_Main_Window") == 0 && g_tip_exists) ? (HWND)0x77 : NULL; }
static BOOL h_IsWindowVisible(HWND h) { (void)h; return g_win11 ? FALSE : TRUE; }
static DWORD h_GetTickCount(void) { return g_now; }
static HMODULE h_LoadLibraryW(LPCWSTR n) { (void)n; return (HMODULE)0x5000; }
static HMODULE h_GetModuleHandleW(LPCWSTR n) { (void)n; return (HMODULE)0x5001; }
static HWND h_GetDesktopWindow(void) { return (HWND)0x10; }
static BOOL h_GetWindowRect(HWND h, RECT *r) { memset(r, 0, sizeof(*r)); if (h == (HWND)0x77 && !g_win11) { r->top = 540; r->right = 1920; r->bottom = 1080; } return TRUE; }
static BOOL h_PostMessageW(HWND h, UINT m, WPARAM w, LPARAM l)
{ (void)h; (void)l; if (m == 0x0112u && w == 0xF060u) { ++g_closes; if (!g_win11) { g_close_pending = 1; g_close_due = g_now + 100; } } return TRUE; }
static BOOL h_CreateProcessW(LPCWSTR a, LPWSTR b, void *c, void *d, BOOL e, DWORD f, void *g, LPCWSTR h, void *i, void *j)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; ((HANDLE *)j)[0] = NULL; ((HANDLE *)j)[1] = NULL; ++g_launched; return TRUE; }
static HRESULT h_SHGetFolderPathW(HWND a, int b, HANDLE c, DWORD d, LPWSTR path) { (void)a; (void)b; (void)c; (void)d; lstrcpyW(path, L"C:\\Common"); return S_OK; }

static HRESULT WINAPI fake_unknown_query(void *self, const GUID *iid, void **out) { (void)self; (void)iid; *out = NULL; return E_NOINTERFACE; }
static ULONG WINAPI fake_unknown_ref(void *self) { (void)self; return 1; }
static HRESULT WINAPI fake_tip_toggle(void *self, HWND w)
{
    (void)self; (void)w;
    ++g_toggles;
    if (g_last_toggle_at && (g_now - g_last_toggle_at) < (DWORD)g_min_toggle_gap) g_min_toggle_gap = (int)(g_now - g_last_toggle_at);
    if (g_last_toggle_at && !g_confirmed && (g_now - g_last_toggle_at) < 2000) { ++g_unsafe_toggles; say("!! UNSAFE Toggle: the previous one is not confirmed yet"); }
    g_confirmed = 0;
    g_last_toggle_at = g_now;
    if (!g_toggle_noop) { g_toggle_pending = 1; g_toggle_due = g_now + 350; }
    return S_OK;
}
static const void *fake_tip_vtbl[4] = { fake_unknown_query, fake_unknown_ref, fake_unknown_ref, fake_tip_toggle };
static const void *fake_tip_object = fake_tip_vtbl;
static HRESULT WINAPI fake_CoCreateInstance(const GUID *a, void *b, DWORD c, const GUID *d, void **out) { (void)a; (void)b; (void)c; (void)d; *out = (void *)&fake_tip_object; return S_OK; }
static LONG WINAPI fake_RtlGetVersion(void *p) { ULONG *v = (ULONG *)p; v[1] = 10; v[2] = 0; v[3] = g_win11 ? 22631u : 15035u; return 0; }
static HRESULT WINAPI fake_Dwm(HWND h, DWORD attr, void *value, DWORD size) { (void)h; (void)attr; (void)size; *(DWORD *)value = g_win11 ? 0u : (g_kb_visible ? 0u : 1u); return S_OK; }
static HRESULT WINAPI fake_fip_location(void *self, RECT *rc)
{
    (void)self;
    memset(rc, 0, sizeof(*rc));
    if (g_kb_visible && g_location_accurate) { rc->left = 0; rc->top = 540; rc->right = 1920; rc->bottom = 1080; }
    return S_OK;
}
static const void *fake_fip_vtbl[7] = { fake_unknown_query, fake_unknown_ref, fake_unknown_ref, fake_unknown_ref, fake_unknown_ref, fake_unknown_ref, fake_fip_location };
static const void *fake_fip_object = fake_fip_vtbl;
static FARPROC h_GetProcAddress(HMODULE m, LPCSTR name)
{
    (void)m;
    if (!lstrcmpA(name, "CoCreateInstance")) return (FARPROC)fake_CoCreateInstance;
    if (!lstrcmpA(name, "RtlGetVersion")) return (FARPROC)fake_RtlGetVersion;
    if (!lstrcmpA(name, "DwmGetWindowAttribute")) return (FARPROC)fake_Dwm;
    return NULL;
}
#define FindWindowW h_FindWindowW
#define IsWindowVisible h_IsWindowVisible
#define GetTickCount h_GetTickCount
#define LoadLibraryW h_LoadLibraryW
#define GetModuleHandleW h_GetModuleHandleW
#define GetProcAddress h_GetProcAddress
#define GetDesktopWindow h_GetDesktopWindow
#define GetWindowRect h_GetWindowRect
#define GetClientRect h_GetClientRect
#define ScreenToClient h_ScreenToClient
#define PostMessageW h_PostMessageW
#define CreateProcessW h_CreateProcessW
#define SHGetFolderPathW h_SHGetFolderPathW
'''

epilogue = r'''
static int failures;
static const char *scenario_name = "?";
static void check_fail(int line, const char *what) { char m[300]; wsprintfA(m, "FAIL [%s] line %d: %s\n", scenario_name, line, what); fputs(m, stdout); ++failures; }
#define CHECK(c) do { if (!(c)) check_fail(__LINE__, #c); } while (0)

static void fire_callback(int showing)
{
    RECT rc;
    if (!g_callbacks) return;
    g_confirmed = 1;
    rc.left = 0; rc.top = 540; rc.right = 1920; rc.bottom = 1080;
    if (showing) arm32_osk_fip_handler_showing(NULL, &rc, TRUE); else arm32_osk_fip_handler_hiding(NULL, TRUE);
}

static void step(void)
{
    g_now += 16;
    if (g_toggle_pending && (int)(g_now - g_toggle_due) >= 0) {
        g_toggle_pending = 0;
        g_kb_visible = !g_kb_visible;
        say(g_kb_visible ? "** shell: keyboard shown" : "** shell: keyboard hidden");
        fire_callback(g_kb_visible);
    }
    if (g_close_pending && (int)(g_now - g_close_due) >= 0) {
        g_close_pending = 0;
        if (g_kb_visible) { g_kb_visible = 0; say("** shell: keyboard closed by SC_CLOSE"); fire_callback(0); }
    }
    arm32_osk_poll();
}
static void run_ms(DWORD ms) { DWORD end = g_now + ms; while ((int)(end - g_now) > 0) step(); }

int main(int argc, char **argv)
{
    const char *name = argc > 1 ? argv[1] : "";
    int i;
    for (i = 2; i < argc; ++i) if (!lstrcmpA(argv[i], "-v")) g_verbose = 1;
    scenario_name = name;
    arm32_osk_signals_ready = TRUE;
    arm32_osk_fip = (void **)&fake_fip_object;
    arm32_osk_signal = ARM32_OSK_SIG_FIP;

    if (!lstrcmpA(name, "win11_show_hide")) {
        g_win11 = 1; g_location_accurate = 0;
        arm32_osk_show("t"); run_ms(5000);
        CHECK(g_toggles == 1); CHECK(g_kb_visible == 1); CHECK(arm32_osk_state == 1); CHECK(g_raise_show >= 1); CHECK(arm32_osk_ours);
        arm32_osk_hide("t"); run_ms(6000);
        CHECK(g_toggles == 2); CHECK(g_kb_visible == 0); CHECK(arm32_osk_state == 0); CHECK(g_raise_hide >= 1); CHECK(g_closes == 0); CHECK(!arm32_osk_ours);
        CHECK(g_unsafe_toggles == 0);
    } else if (!lstrcmpA(name, "win11_location_accurate")) {
        g_win11 = 1; g_location_accurate = 1;
        arm32_osk_show("t"); run_ms(5000);
        CHECK(g_toggles == 1); CHECK(g_kb_visible == 1); CHECK(arm32_osk_state == 1);
        arm32_osk_hide("t"); run_ms(6000);
        CHECK(g_toggles == 2); CHECK(g_kb_visible == 0); CHECK(arm32_osk_state == 0);
    } else if (!lstrcmpA(name, "win11_hide_while_coming_up")) {
        g_win11 = 1;
        arm32_osk_show("t"); run_ms(200); arm32_osk_hide("t"); run_ms(9000);
        CHECK(g_kb_visible == 0); CHECK(g_toggles == 2); CHECK(arm32_osk_state == 0); CHECK(g_unsafe_toggles == 0);
    } else if (!lstrcmpA(name, "win11_hide_right_after_show")) {
        g_win11 = 1;
        arm32_osk_show("t"); run_ms(700);                       /* visible by now (callback at ~350 ms) */
        CHECK(g_kb_visible == 1 && arm32_osk_ours);
        arm32_osk_hide("t"); run_ms(6000);                      /* the first Toggle is < 2 s old: the hide must wait, not give up */
        CHECK(g_toggles == 2); CHECK(g_kb_visible == 0); CHECK(arm32_osk_state == 0); CHECK(g_unsafe_toggles == 0);
    } else if (!lstrcmpA(name, "win11_no_callbacks")) {
        g_win11 = 1; g_callbacks = 0; g_location_accurate = 0;
        arm32_osk_show("t"); run_ms(6000);
        CHECK(g_toggles == 1); CHECK(g_kb_visible == 1); CHECK(arm32_osk_state == 1 && arm32_osk_state_assumed);
        arm32_osk_hide("t"); run_ms(5000);
        CHECK(g_toggles == 2); CHECK(g_kb_visible == 0); CHECK(arm32_osk_state == 0); CHECK(g_closes == 0);
    } else if (!lstrcmpA(name, "win11_user_closes")) {
        g_win11 = 1;
        arm32_osk_show("t"); run_ms(5000);
        CHECK(g_kb_visible == 1 && arm32_osk_ours);
        g_kb_visible = 0; fire_callback(0);                     /* the user taps the X of the keyboard */
        run_ms(2500);
        CHECK(arm32_osk_dismissed_by_user); CHECK(g_raise_hide >= 1); CHECK(g_toggles == 1);
        arm32_osk_show("again"); run_ms(500); CHECK(g_toggles == 1);          /* ignored: closed by the user and no tap since */
        arm32_osk_note_pointer_up(); run_ms(2500);                             /* tap -> reopen */
        CHECK(g_toggles == 2); CHECK(g_kb_visible == 1); CHECK(arm32_osk_state == 1);
    } else if (!lstrcmpA(name, "win11_already_visible")) {
        g_win11 = 1;
        g_kb_visible = 1; fire_callback(1); run_ms(100);        /* the user opened it himself, then the game asks */
        arm32_osk_show("t"); run_ms(3000);
        CHECK(g_toggles == 0); CHECK(arm32_osk_ours); CHECK(arm32_osk_state == 1);
    } else if (!lstrcmpA(name, "win11_never_appears")) {
        g_win11 = 1; g_toggle_noop = 1;
        arm32_osk_show("t"); run_ms(30000);
        CHECK(g_toggles <= 4); CHECK(!arm32_osk_want); CHECK(g_kb_visible == 0);
        CHECK(!arm32_osk_ours || arm32_osk_state == 0 || arm32_osk_state_assumed);
    } else if (!lstrcmpA(name, "win11_repeated_chat")) {
        g_win11 = 1;
        for (i = 0; i < 4; ++i) {
            arm32_osk_show("t"); run_ms(4000);
            CHECK(g_kb_visible == 1);
            arm32_osk_hide("t"); run_ms(4000);
            CHECK(g_kb_visible == 0);
        }
        CHECK(g_toggles == 8);
    } else if (!lstrcmpA(name, "win10_show_hide")) {
        g_win11 = 0; g_location_accurate = 1;
        arm32_osk_show("t"); run_ms(5000);
        CHECK(g_toggles == 1); CHECK(g_kb_visible == 1); CHECK(arm32_osk_state == 1); CHECK(g_raise_show >= 1);
        arm32_osk_hide("t"); run_ms(3000);
        CHECK(g_closes == 1); CHECK(g_toggles == 1); CHECK(g_kb_visible == 0); CHECK(arm32_osk_state == 0); CHECK(g_raise_hide >= 1);
    } else if (!lstrcmpA(name, "win11_rect") || !lstrcmpA(name, "win11_rect_location") || !lstrcmpA(name, "win10_rect")) {
        /* the rectangle the game gets (OccludedRect): Windows 10 = TabTip window, Windows 11 = shell (Location, else the Showing callback) */
        float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_win11 = name[4] == '1'; g_location_accurate = !g_win11 || !lstrcmpA(name, "win11_rect_location");
        arm32_osk_show("t"); run_ms(3000);
        CHECK(g_kb_visible == 1 && arm32_osk_state == 1);
        CHECK(arm32_osk_rect_buffer(rect));
        CHECK(rect[0] == 0.0f && rect[1] == 540.0f && rect[2] == 1920.0f && rect[3] == 540.0f);
        arm32_osk_hide("t"); run_ms(3000);
        CHECK(g_kb_visible == 0); CHECK(!arm32_osk_rect_buffer(rect));
    } else if (!lstrcmpA(name, "win11_show_right_after_hide") || !lstrcmpA(name, "win10_show_right_after_hide")) {
        /* the game hides the keyboard and asks for it again 0.7 s later (1.2.8 Windows 11 log: a re-tap waited 2.9 s for the keyboard) */
        DWORD asked, shown = 0;
        g_win11 = name[4] == '1'; g_location_accurate = !g_win11;
        /* Windows 11 hides with a Toggle, so the hide itself waits for the 2 s guard: let the keyboard stay up 2.6 s there (the log case:
         * hide, then a re-tap 0.7-1 s later that hits the guard of the HIDE Toggle); Windows 10 hides with SC_CLOSE, so 0.6 s exercises the
         * guard of the SHOW Toggle */
        arm32_osk_show("t"); run_ms(g_win11 ? 2600 : 600);
        CHECK(g_kb_visible == 1);
        arm32_osk_hide("t"); run_ms(700);
        CHECK(g_kb_visible == 0);
        arm32_osk_show("again"); asked = g_now;
        while ((g_now - asked) < 6000) { step(); if (!shown && g_kb_visible) shown = g_now; }
        {
            char m[120];
            wsprintfA(m, "  keyboard visible %lu ms after the second request\n", (unsigned long)(shown ? shown - asked : 0));
            fputs(m, stdout);
        }
        CHECK(shown != 0); CHECK(shown - asked <= 2000); CHECK(g_kb_visible == 1 && arm32_osk_state == 1);
        CHECK(g_unsafe_toggles == 0);
    } else if (!lstrcmpA(name, "win11_shell_acts_while_idle") || !lstrcmpA(name, "win10_shell_acts_while_idle")) {
        /* the shell shows the keyboard on its own while no request is pending, then the game asks for it, then the shell hides it on its own */
        g_win11 = name[4] == '1'; g_location_accurate = 1;
        run_ms(500);
        g_kb_visible = 1; fire_callback(1); run_ms(200);
        if (g_win11) CHECK(arm32_osk_state == 1);      /* Windows 11: the state follows the shell even when idle (new) */
        else CHECK(arm32_osk_state == -1);             /* Windows 10: idle callbacks are not processed (unchanged) */
        arm32_osk_show("t"); run_ms(2000);
        CHECK(g_toggles == 0); CHECK(arm32_osk_state == 1 && arm32_osk_ours);   /* adopted, never toggled shut */
        arm32_osk_hide("t"); run_ms(3000);
        CHECK(g_kb_visible == 0 && arm32_osk_state == 0);
        g_kb_visible = 1; fire_callback(1); run_ms(200);
        if (g_win11) CHECK(arm32_osk_state == 1); else CHECK(arm32_osk_state == 0);
        g_kb_visible = 0; fire_callback(0); run_ms(200);
        CHECK(arm32_osk_state == 0);
    } else if (!lstrcmpA(name, "win10_hide_then_show_quickly") || !lstrcmpA(name, "win11_hide_then_show_quickly")) {
        /* Surface 2 1.2.8 log 2026-10-21: Enter -> TryHide, TryShow 62 ms later for the same chat box: the keyboard must simply stay up */
        int ever_hidden = 0;
        g_win11 = name[4] == '1'; g_location_accurate = !g_win11;
        arm32_osk_show("t"); run_ms(3000);
        CHECK(g_kb_visible == 1);
        arm32_osk_hide("enter"); run_ms(62);
        if (!g_kb_visible) ever_hidden = 1;
        arm32_osk_show("refocus");
        { DWORD until = g_now + 3000; while ((int)(until - g_now) > 0) { step(); if (!g_kb_visible) ever_hidden = 1; } }
        CHECK(!ever_hidden); CHECK(g_kb_visible == 1 && arm32_osk_state == 1 && arm32_osk_ours);
        CHECK(g_toggles == 1); CHECK(g_closes == 0);
        arm32_osk_hide("done"); run_ms(3000);
        CHECK(g_kb_visible == 0 && arm32_osk_state == 0);
    } else if (!lstrcmpA(name, "win10_rect_held_while_finger_down") || !lstrcmpA(name, "win11_rect_held_while_finger_down")) {
        /* Surface 2 1.2.8 log 2026-10-21: the press opens the keyboard, which is up before the finger lifts - the game must not get the rectangle
         * (and move the chat box under the finger) until the lift */
        float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_win11 = name[4] == '1'; g_location_accurate = 1;
        g_touch_down = 1;
        arm32_osk_show("press"); run_ms(500);
        CHECK(g_kb_visible == 1 && arm32_osk_state == 1);
        CHECK(!arm32_osk_rect_for_game(rect));
        g_touch_down = 0;
        CHECK(arm32_osk_rect_for_game(rect)); CHECK(rect[3] == 540.0f);
        g_touch_down = 1; run_ms(4000);                       /* a finger held for long: the hold-back ends after 3 s */
        CHECK(arm32_osk_rect_for_game(rect));
    } else if (!lstrcmpA(name, "win10_blind_signal")) {
        /* a signal that never changes (Location empty, window "visible"): Toggle once, assume visible, never toggle a second time */
        g_win11 = 0; g_location_accurate = 0; g_callbacks = 0;
        arm32_osk_signal = ARM32_OSK_SIG_CLOAK; arm32_osk_fip = NULL;
        arm32_osk_show("t"); run_ms(8000);
        CHECK(g_toggles == 1); CHECK(g_kb_visible == 1);
    } else {
        fputs("unknown scenario\n", stdout);
        return 2;
    }
    {
        char m[200];
        wsprintfA(m, "%s: %s (toggles=%d closes=%d launched=%d raise=%d/%d min gap=%d ms)\n", name, failures ? "FAILED" : "ok", g_toggles, g_closes, g_launched,
                  g_raise_show, g_raise_hide, g_min_toggle_gap);
        fputs(m, stdout);
    }
    return failures ? 1 : 0;
}
int _fltused;
'''
src = OUT / "test_osk.c"
src.write_text(prologue + "\n" + block + "\n" + epilogue, encoding="utf-8")

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, (r.stdout or "") + (r.stderr or "")

obj = OUT / "test_osk.obj"
exe = OUT / "test_osk.exe"
rc, out = run([str(LLVM / "clang.exe"), "--target=i686-pc-windows-msvc", "-O1", "-g0", "-fno-stack-protector", "-I" + str(REPO),
               "-isystem", str(SDKROOT / "Include" / SDKVER / "um"), "-isystem", str(SDKROOT / "Include" / SDKVER / "shared"),
               "-isystem", str(SDKROOT / "Include" / SDKVER / "ucrt"), "-isystem", str(VC.parent.parent / "include"),
               "-Wno-incompatible-pointer-types", "-Wno-microsoft-cast", "-Wno-ignored-attributes", "-Wno-comment", "-Wno-pointer-sign",
               "-Wno-int-conversion", "-c", str(src), "-o", str(obj)])
errors = [l for l in out.splitlines() if "error" in l]
print("compile rc=%d, %d error lines" % (rc, len(errors)))
for l in errors[:30]:
    print("  ", l[:260])
if rc:
    sys.exit(2)
um = SDKROOT / "Lib" / SDKVER / "um" / "x86"
ucrt = SDKROOT / "Lib" / SDKVER / "ucrt" / "x86"
rc, out = run([str(LLVM / "lld-link.exe"), "/subsystem:console", "/machine:x86", "/entry:mainCRTStartup", "/out:" + str(exe), str(obj),
               "/libpath:" + str(VC), "/libpath:" + str(um), "/libpath:" + str(ucrt), "libcmt.lib", "kernel32.lib", "user32.lib",
               "oldnames.lib", "legacy_stdio_definitions.lib"])
print("link rc=%d" % rc)
if rc:
    print(out[:3000])
    sys.exit(3)

scenarios = ["win11_show_hide", "win11_location_accurate", "win11_hide_while_coming_up", "win11_hide_right_after_show", "win11_no_callbacks",
             "win11_user_closes", "win11_already_visible", "win11_never_appears", "win11_repeated_chat",
             "win10_show_hide", "win10_blind_signal",
             "win11_rect", "win11_rect_location", "win10_rect", "win11_show_right_after_hide", "win10_show_right_after_hide",
             "win11_shell_acts_while_idle", "win10_shell_acts_while_idle",
             "win10_hide_then_show_quickly", "win11_hide_then_show_quickly", "win10_rect_held_while_finger_down", "win11_rect_held_while_finger_down"]
only = [a for a in sys.argv[1:] if not a.startswith("-")]
bad = 0
for name in (only or scenarios):
    r = subprocess.run([str(exe), name] + (["-v"] if VERBOSE else []), capture_output=True, text=True, timeout=60)
    sys.stdout.write(r.stdout)
    sys.stdout.write(r.stderr)
    if r.returncode:
        bad += 1
print("OSK TEST %s (%d scenario(s) failed)" % ("FAILED" if bad else "PASSED", bad))
sys.exit(1 if bad else 0)
