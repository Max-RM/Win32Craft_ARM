/* win32_host_arm32_0104_core.inl - toolkit of the Minecraft PE 0.10.4.40 (Windows Phone 8.1 XAML) ARM32 host.
 *
 * Included by win32_host_arm32_0104.inl (which win32_host.c includes under WIN32CRAFT_ARM32, right before Win32Bootstrap).
 * THIS FILE IS OWNED BY THE DESIGNER.  The five unit files (async, app, panel, platform, input) only USE it.
 * The single source of truth for all rules, threads, ordering and the cross-unit API is arm32/v0_10_4/HOST_DESIGN.md.
 *
 * Contents:   1 basics / externs / constants       2 logging            3 HSTRING helpers      4 locks, once
 *             5 object / view / class model + aggregation + X104_SLOT   6 hole-filling stubs   7 event lists   8 guarded calls
 *             9 activation registry and the IAT hook   10 game access helpers   11 XAML element mixin
 *             12 completed IAsyncAction singleton  13 on-screen keyboard / text-input / Present helpers   14 cross-unit prototypes
 *
 * RULE (checked by tools/arm32/try_0104.py): the five unit files may use ONLY symbols of this file, of the generated iids/header files and of
 * win32_host.c.  Nothing defined in win32_host_arm32_0104_boot.inl (it is included LAST) may be referenced by a unit - shared constants
 * (X104_TIMER_PUMP, X104_WM_*) and shared helpers live HERE.
 *
 * ABI reminder (AAPCS-VFP, Thumb-2, hard float): `this` = r0; args r1..r3 then stack; int64/double arguments are 8-byte aligned
 * (TimeSpan lands in r2:r3, r1 skipped; a double goes to d0 in the VFP convention).  Value structs: as OUTPUT parameters (`Size* value`) they
 * travel through POINTERS; as INPUT parameters WinRT passes Size/Point/Rect BY VALUE (Measure(Size), Arrange(Rect), put_RenderTransformOrigin(Point),
 * put_PointerPosition(Point)): an all-float struct is a homogeneous aggregate and goes in s0..s3 - declare the C parameter as X104Size/X104Point/X104Rect
 * and the compiler does the rest.  No slot the game needs today takes a struct by value, but the typedefs are generated that way.
 * SIGNATURES ARE CHECKED: every vtable slot is registered with X104_SLOT(Interface, Method, thunk), which refuses (compile error) a thunk whose C
 * signature differs from the generated typedef X104_FN_<Interface>_<Method> (units/SLOTS.md).  The vtable arrays hold untyped pointers, so nothing
 * else would catch a missing LONGLONG or a float where a double is expected - only a crash on the device.
 * Every COM thunk in this toolkit is declared WINAPI (empty on ARM). */
#ifndef WIN32_HOST_ARM32_0104_CORE_INL
#define WIN32_HOST_ARM32_0104_CORE_INL

#include "win32_host_arm32_0104.h"          /* ARM0104_CODE / ARM0104_DATA + generated verified constants */
#include "win32_host_arm32_0104_iids.inl"   /* GENERATED: X104_IID_*, X104_SLOT_*, X104_NSLOTS_* */

/* ======================================================================================================================
 * 1. basics, Win32 externs the compat header does not declare, verified RVAs
 * ====================================================================================================================== */
typedef __builtin_va_list X104VaList;
#ifndef X104_API
#define X104_API WINAPI
#endif

/* kernel32 / user32 prototypes identical to the SDK (win32_host.c declares more of them; identical redeclarations are legal C) */
extern LPSTR WINAPI GetCommandLineA(void);
extern HANDLE WINAPI CreateThread(void *, SIZE_T, DWORD (WINAPI *)(void *), void *, DWORD, DWORD *);
extern DWORD WINAPI WaitForSingleObject(HANDLE, DWORD);
extern HANDLE WINAPI CreateEventW(void *, BOOL, BOOL, LPCWSTR);
extern BOOL WINAPI SetEvent(HANDLE);
extern BOOL WINAPI ResetEvent(HANDLE);
extern HANDLE WINAPI GetCurrentThread(void);
extern BOOL WINAPI SetThreadPriority(HANDLE, int);
extern BOOL WINAPI SwitchToThread(void);
extern ULONGLONG WINAPI GetTickCount64(void);
extern DWORD WINAPI GetTickCount(void);
extern DWORD WINAPI MsgWaitForMultipleObjectsEx(DWORD, const HANDLE *, DWORD, DWORD, DWORD);
extern UINT_PTR WINAPI SetTimer(HWND, UINT_PTR, UINT, void *);
extern BOOL WINAPI KillTimer(HWND, UINT_PTR);
extern int WINAPI wvsprintfA(LPSTR, LPCSTR, X104VaList);
extern BOOL WINAPI ShowWindow(HWND, int);
extern BOOL WINAPI UpdateWindow(HWND);
extern BOOL WINAPI SetForegroundWindow(HWND);
extern BOOL WINAPI IsWindow(HWND);
extern DWORD WINAPI GetWindowThreadProcessId(HWND, DWORD *);
extern int WINAPI MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);

#ifndef WM_TIMER
#define WM_TIMER 0x0113u
#endif
#define X104_HEAP_ZERO_MEMORY   8
#define X104_WAIT_TIMEOUT       0x00000102u
#define X104_INFINITE           0xFFFFFFFFu
#define X104_QS_ALLINPUT        0x1CFFu    /* QS_ALLINPUT of the Windows 8 SDK: 0x04FF plus QS_TOUCH (0x0800) and QS_POINTER (0x1000): a WM_POINTER/WM_TOUCH wakes the wait at once */
#define X104_THREAD_PRIORITY_BELOW_NORMAL  (-1)
#define X104_THREAD_PRIORITY_NORMAL        0
#define X104_THREAD_PRIORITY_ABOVE_NORMAL  1
#define X104_E_ILLEGAL_METHOD_CALL         ((HRESULT)0x8000000EL)
#define X104_E_CHANGED_STATE               ((HRESULT)0x8000000CL)
#define X104_E_NOT_IMPL                    ((HRESULT)0x80004001L)
#define X104_E_BOUNDS                      ((HRESULT)0x8000000BL)
/* private window messages (posted to x104.window from any thread, consumed by x104_ui_run_loop on the UI thread BEFORE DispatchMessage) */
#define X104_WM_QUIT   (0x0400u + 0x104u)    /* wParam = exit code: leave x104_ui_run_loop.  Handled in x104_window_proc too (a nested modal loop dispatches it there) */
#define X104_WM_WAKE   (0x0400u + 0x105u)    /* wake the loop: new queue item or timer change */
#define X104_TIMER_PUMP 0x104u               /* SetTimer id of the async unit's modal-loop pump: the window proc forwards WM_TIMER to x104_ui_modal_pass() */

/* Verified RVAs used by the toolkit and the units (facts: pe_patch_plan, startup_flow, platform_services, frame_render, client_init).
 * Code RVAs are EVEN; use X104_CODE(rva) to call/store them (sets the Thumb bit), X104_DATA(rva) for data cells. */
enum {
    /* IAT cells of MCPE.Host.WindowsPhone.exe (pe_patch_plan.import_table) */
    X104_RVA_IAT_D3DCOMPILE        = 0x0020B000,
    X104_RVA_IAT_D3DREFLECT        = 0x0020B004,
    X104_RVA_IAT_XAUDIO2CREATE     = 0x0020B05C,   /* XAudio2_8.dll ordinal 1 */
    X104_RVA_IAT_D3D11CREATEDEVICE = 0x0020B1A4,
    X104_RVA_IAT_CREATEDXGIFACTORY1 = 0x0020B1AC,
    X104_RVA_IAT_TERMINATE         = 0x0020B4F0,   /* msvcr120_app ?terminate@@YAXXZ */
    X104_RVA_IAT_ABORT             = 0x0020B530,
    X104_RVA_IAT_EXIT_APP          = 0x0020B588,   /* _exit_app */
    X104_RVA_IAT_EXIT              = 0x0020B590,   /* _exit */
    X104_RVA_IAT_PURECALL          = 0x0020B5C8,
    X104_RVA_IAT_CXXTHROW          = 0x0020B5D4,
    X104_RVA_IAT_OPERATOR_NEW      = 0x0020B5E0,
    X104_RVA_IAT_GETACTIVATIONFACTORY = 0x0020B5FC, /* vccorlib120_app ?GetActivationFactoryByPCWSTR@@YAJPAXAAVGuid@Platform@@PAPAX@Z */
    X104_RVA_IAT_RAISE_COM         = 0x0020B680,   /* ?__abi_WinRTraiseCOMException@@YAXJ@Z */
    /* game code (even = Thumb function RVA) */
    X104_RVA_WINMAIN_WRAPPER       = 0x00203EFC,   /* WinMain thunk -> 0x20405C -> user main 0x1F42A4 -> Application::Start 0x1EECBC */
    X104_RVA_CALLSITE              = 0x00200A72,   /* patched by the exe patcher: bl 0x203EFC -> trampoline 0x20A364 -> win32craft.dll!Win32Bootstrap */
    X104_RVA_APPMAIN_ISREADY       = 0x001DA310,
    X104_RVA_APPMAIN_ONSIZE        = 0x001DA32C,
    X104_RVA_APPMAIN_ATTACH        = 0x001DA634,
    X104_RVA_APPMAIN_STARTRENDER   = 0x001DA8A8,
    X104_RVA_RENDER_LOOP_BODY      = 0x001DA160,
    X104_RVA_MAINPAGE_ONSIZE       = 0x001DF808,
    X104_RVA_MAINPAGE_ONTICK       = 0x001DF8A4,
    X104_RVA_CLIENT_INIT           = 0x00057B84,
    X104_RVA_NOOP_FUNCTION         = 0x0000BF60,   /* shared no-op (use +1 for the Thumb pointer stored in vtable cells) */
    /* data */
    X104_RVA_PROFILE_STRING        = 0x002351F0,   /* "_level_9_3\0" ; byte at +9 (RVA 0x2351F9) '3' -> '1' */
    X104_RVA_PROFILE_DIGIT         = 0x002351F9,
    X104_RVA_RESOLUTION_FLOAT      = 0x00DC8854,   /* Default GPU row scale 0.5f (bytes 00 00 00 3f) -> 1.0f */
    X104_RVA_DR_GLOBAL             = 0x00E2AFC4,
    X104_RVA_PLATFORM_GLOBAL       = 0x00DDD7E0,
    X104_RVA_GUI_SCALE             = 0x00DDE814,
    X104_RVA_GUI_SCALE_INV         = 0x00340D40,
    X104_RVA_CELL_NETWORK          = 0x0033FB6C,   /* AppPlatform_Winrt vtable slot 19 isNetworkEnabled */
    X104_RVA_CELL_TOUCHONLY        = 0x0033FB88,   /* slot 26: returns true (touch only) - leave alone */
    X104_RVA_CELL_BUY              = 0x0033FB7C,   /* slot 23 */
    X104_RVA_CELL_VIBRATE          = 0x0033FB94,   /* slot 29 */
    X104_RVA_CELL_FREEMEMORY       = 0x0033FBC0,   /* slot 40 */
    X104_RVA_CELL_SOUND_PLAY       = 0x00DB804C,   /* SoundSystemRT vtable 0xDB8028 slot 9 */
    X104_RVA_CELL_SOUND_VF10       = 0x00DB8050    /* slot 10 */
};
#define X104_CODE(rva)  ARM0104_CODE(x104.image, (rva))
#define X104_DATA(rva)  ARM0104_DATA(x104.image, (rva))

/* Game object layouts (facts startup_flow 3, client_init 2): host code only ever PEEKS at these, always behind IsBadReadPtr */
enum {
    X104_MAINPAGE_PANEL = 0x5C, X104_MAINPAGE_LOGO = 0x60, X104_MAINPAGE_EDITBOX = 0x64, X104_MAINPAGE_BUTTON = 0x68,
    X104_MAINPAGE_APPMAIN = 0x7C, X104_MAINPAGE_TIMER = 0x84, X104_MAINPAGE_OUTER_OFFSET = 8,
    X104_APPMAIN_DR = 0x04, X104_APPMAIN_CRITSEC = 0x10, X104_APPMAIN_CLIENT = 0x34, X104_APPMAIN_HOLDER = 0x38, X104_APPMAIN_READY = 0x3C,
    X104_CLIENT_INITDONE = 0x08, X104_CLIENT_HOLDER = 0x0C, X104_CLIENT_WIDTH = 0x20, X104_CLIENT_HEIGHT = 0x24,
    X104_CLIENT_TEXTURES = 0x118, X104_CLIENT_FONT = 0x11C, X104_CLIENT_KEYBOARD_HEIGHT = 0x164
};

/* X104Size / X104Point / X104Rect (Windows.Foundation.Size / Point / Rect) are defined in the generated iids file (the slot typedefs need them) */

/* call a game function by its verified rva with a typed plain C function pointer (Thumb bit set by X104_CODE):
 *     HRESULT hr = X104_GAME_FN(HRESULT (*)(void *, void *), 0x1DA634)(app, panel); */
#define X104_GAME_FN(type, rva)  ((type)X104_CODE(rva))

/* ======================================================================================================================
 * 1b. global state (one instance, `x104`; the fields are documented in HOST_DESIGN.md 6)
 * ====================================================================================================================== */
#define X104_PRESENT_HWND   0      /* path B (default): patched dxgi cells, CreateSwapChainForHwnd on the game window */
#define X104_PRESENT_DCOMP  1      /* path A (optional): DirectComposition */

typedef struct X104State {
    BYTE *image;                         /* game module base (== game_image) */
    HINSTANCE instance;
    HWND window;                         /* the game window (== game_window); created by the boot on the UI thread */
    DWORD main_thread_id;                /* thread of Win32Bootstrap (MTA); the UI thread is the SAME thread */
    DWORD ui_thread_id;
    wchar_t game_root[MAX_PATH];         /* exe directory == Package.InstalledLocation == current directory (ASCII only) */
    wchar_t data_root[MAX_PATH];         /* ApplicationData.LocalFolder (the platform unit fills it in x104_platform_install) */
    /* options (x104_parse_options) */
    int opt_present;                     /* --present=hwnd|dcomp */
    BOOL opt_half_res;                   /* --half-res: keep the game's own half-resolution GPU-table row (default: full resolution) */
    BOOL opt_fullscreen;                 /* --fullscreen: borderless window on the monitor */
    BOOL opt_trace;                      /* --trace-fakes: units log every call of the chatty fakes */
    int opt_width, opt_height;           /* effective client size: --window=WxH, else min(1280x720, work area minus the frame) */
    BOOL opt_window_explicit;            /* --window=WxH was given (no clamping to the work area) */
    int opt_max_fps;                     /* --max-fps=N: cap of the game's Present rate (render thread sleeps in the Present stub); 0 = automatic (x104_present_pace) */
    BOOL opt_vsync;                      /* --vsync: the Present stub changes SyncInterval 0 to 1 */
    int opt_selftest;                    /* --selftest-fakes: 1 = run the on-device fake self test before the game, 2 = run it and exit (--selftest-fakes=only) */
    /* hook bookkeeping */
    void *original_activation;           /* vccorlib120_app GetActivationFactoryByPCWSTR before our hook */
    /* ordering flags (HOST_DESIGN.md 4): each is set exactly once, by the owner named in the comment, with x104_flag_set */
    volatile LONG f_mainpage_created;    /* app: IPageFactory.CreateInstance seen, mainpage_outer recorded */
    volatile LONG f_content_set;         /* app: Window.put_Content(page) done (the MainPage constructor has returned) */
    volatile LONG f_launched;            /* app: OnLaunched returned */
    volatile LONG f_size_changed_done;   /* panel: the FIRST SizeChanged returned from the game handler */
    volatile LONG f_timer_started;       /* async: DispatcherTimer.Start called */
    volatile LONG f_ready_tick;          /* async: the game called DispatcherTimer.Stop after its ready tick (AppMain+0x3C was 1) */
    volatile LONG f_exit_requested;      /* app ONLY: x104_app_request_exit ran once (idempotency guard of the exit request; never consulted by async) */
    volatile LONG f_quit;                /* async ONLY: x104_ui_quit ran - the loop is leaving; x104_ui_post refuses to queue from now on */
    volatile LONG f_loop_exit;           /* the loop must return: set by x104_ui_quit (any thread) or by the window proc on X104_WM_QUIT; async tests it after every pass */
    volatile LONG f_units_ready;         /* boot: all four unit init functions returned.  Until then the window proc only does default handling (CreateWindowEx sends WM_GETMINMAXINFO/WM_SIZE/... early) */
    volatile LONG f_swapchain_created;   /* panel: the game's CreateSwapChainForComposition stub created the HWND chain (path B) */
    void *mainpage_outer;                /* MainPage+8 sub-object passed as `outer` to IPageFactory.CreateInstance (MainPage = outer - 8) */
    int exit_code;
} X104State;

static X104State x104;

/* ======================================================================================================================
 * 1c. game profile: the x104 host serves more than one Windows Phone 8.1 C++/CX game.  Everything that names ONE game (identification, take-over call
 *     site, window texts, which game-code patches exist, which XAML shape the game uses) lives in an X104Profile; the units ask x104_feat() instead of
 *     assuming Minecraft.  Selected by x104_image_matches() (quiet, before the log exists) and kept outside `x104` (the bootstrap clears that struct).
 * ====================================================================================================================== */
#define X104_FEAT_MCPE_PATCHES   0x00000001u   /* Minecraft code/data patches by verified RVA: shader profile string, resolution float, AppPlatform/SoundSystemRT cells */
#define X104_FEAT_IAT_FACTS      0x00000002u   /* the X104_RVA_IAT_* fallbacks are this exe's cells (other games: hook by name only, skip absent imports) */
#define X104_FEAT_XAML_PAGE      0x00000004u   /* "DirectX App (XAML)" template: MainPage via IPageFactory, AppMain + ready flag, thread-pool render loop, the first SizeChanged
                                                  raised by the host (R2), DispatcherTimer gate.  Without it (REDCON 1): SwapChainPanel built in code, frames from
                                                  CompositionTarget.Rendering on the UI thread, resizes through CoreWindow.SizeChanged, "ready" = OnLaunched returned */

typedef struct X104Profile {
    const char *key;                     /* short id in log lines */
    const char *title;                   /* "Minecraft PE 0.10.4.40" - log banner, failure message box */
    const char *box_caption;             /* caption of the host's error message boxes */
    const wchar_t *window_class;
    const wchar_t *window_title;
    DWORD size_of_image_patched;         /* the value the exe patcher writes (identification key) */
    DWORD size_of_image_stock;           /* only for the "this exe was not patched" log line */
    DWORD entry_rva;                     /* expected AddressOfEntryPoint (log check) */
    WORD sections_patched;               /* expected section count of the patched exe (log check) */
    DWORD callsite_rva;                  /* the CRT call the patcher rerouted to win32craft.dll!Win32Bootstrap */
    BYTE callsite_bytes[4];              /* its patched encoding */
    DWORD trampoline_rva;                /* where the patched call now goes (PIC thunk in .text padding; log text) */
    DWORD winmain_wrapper_rva;           /* the game's WinMain thunk (12 bytes = ARM0104_STARTUP_FLOW_WINMAIN_THUNK_EXPECTED in every supported game) */
    DWORD features;                      /* X104_FEAT_* */
    const wchar_t *data_subdir;          /* default data root = %APPDATA%\<data_subdir> (ApplicationData.LocalFolder) */
    const char *data_subdir_text;        /* the same for log lines */
} X104Profile;

static const X104Profile x104_profile_mcpe0104 = {
    "mcpe0104", "Minecraft PE 0.10.4.40", "Minecraft PE 0.10.4 - Win32Craft", L"MCPE0104Win32", L"Win32Craft - Minecraft PE 0.10.4",
    0x00E6F000u, 0x00E6E000u, 0x0020095Du, 7, 0x00200A72u, { 0x09, 0xf0, 0x77, 0xfc }, 0x0020A364u, 0x00203EFCu,
    X104_FEAT_MCPE_PATCHES | X104_FEAT_IAT_FACTS | X104_FEAT_XAML_PAGE,
    L"MinecraftPE\\0.10.4", "%APPDATA%\\MinecraftPE\\0.10.4"
};

/* REDCON 1 1.3.0.0 (Hexage): games_recon/redcon_*.md; values confirmed by tools/arm32-patcher.py --profile redcon */
static const X104Profile x104_profile_redcon = {
    "redcon", "REDCON 1 1.3.0.0", "REDCON 1 - Win32Craft", L"Win32CraftRedcon", L"Win32Craft - REDCON 1",
    0x001E1000u, 0x001E0000u, 0x0013A445u, 6, 0x0013A55Au, { 0x06, 0xf0, 0xa9, 0xff }, 0x001414B0u, 0x0013A91Cu,
    0,
    L"Win32Craft\\Redcon", "%APPDATA%\\Win32Craft\\Redcon"
};

static const X104Profile *const x104_profiles[] = { &x104_profile_mcpe0104, &x104_profile_redcon };
static const X104Profile *x104_profile = &x104_profile_mcpe0104;   /* the active game (set by x104_image_matches; the MCPE default keeps the PC harnesses unchanged) */

#define x104_feat(flag)  ((x104_profile->features & (flag)) != 0)

static const char *x104_profile_title(void)
{
    return x104_profile->title;
}

/* caption of the host's error message boxes */
static const char *x104_profile_box_caption(void)
{
    return x104_profile->box_caption;
}

/* fallback rva for a by-name IAT hook: the MCPE fact when the profile owns those facts, else 0 = "hook by name only" */
static DWORD x104_iat_fact(DWORD mcpe_rva)
{
    return x104_feat(X104_FEAT_IAT_FACTS) ? mcpe_rva : 0;
}

/* ======================================================================================================================
 * 2. logging  (the ONLY feedback channel of a device run: log generously, every failure path must name what failed)
 * ====================================================================================================================== */
/* printf-style log line (wsprintfA rules: %s %d %u %x %lu %08lx %p %c, NO floats; at most 1024 characters) */
static void x104_logf(const char *format, ...)
{
    char line[1100];                                   /* wvsprintfA writes at most 1024 characters */
    X104VaList arguments;
    __builtin_va_start(arguments, format);
    wvsprintfA(line, format, arguments);
    __builtin_va_end(arguments);
    log_line(line);
}

/* at most `limit` lines per call site: `static volatile LONG n; X104_LOG_LIMITED(&n, 20, "...", ...)` */
#define X104_LOG_LIMITED(counter, limit, ...) \
    do { if (InterlockedIncrement((volatile LONG *)(counter)) <= (LONG)(limit)) x104_logf(__VA_ARGS__); } while (0)

/* chatty per-call tracing, only with --trace-fakes (x104.opt_trace) */
#define X104_TRACE(...)  do { if (x104.opt_trace) x104_logf(__VA_ARGS__); } while (0)

static void x104_format_guid(char *out, const GUID *iid)
{
    if (!iid) {
        wsprintfA(out, "(null IID)");
        return;
    }
    wsprintfA(out, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", (unsigned long)iid->Data1, (unsigned)iid->Data2,
              (unsigned)iid->Data3, (unsigned)iid->Data4[0], (unsigned)iid->Data4[1], (unsigned)iid->Data4[2],
              (unsigned)iid->Data4[3], (unsigned)iid->Data4[4], (unsigned)iid->Data4[5], (unsigned)iid->Data4[6],
              (unsigned)iid->Data4[7]);
}

/* "IFrameworkElement" for a known IID, else NULL */
static const char *x104_iid_name(const GUID *iid)
{
    unsigned index;
    if (!iid) return NULL;
    for (index = 0; index < ARRAYSIZE(X104_KNOWN_IIDS); ++index) {
        if (memcmp(X104_KNOWN_IIDS[index].iid, iid, sizeof(GUID)) == 0) return X104_KNOWN_IIDS[index].name;
    }
    return NULL;
}

/* "IFrameworkElement {A391D09B-...}" or just the GUID text for an unknown IID (buffer >= 96 bytes) */
static void x104_describe_iid(char *out, const GUID *iid)
{
    char text[48];
    const char *name = x104_iid_name(iid);
    x104_format_guid(text, iid);
    if (name) wsprintfA(out, "%s {%s}", name, text);
    else wsprintfA(out, "{%s}", text);
}

static const char *x104_hr_name(HRESULT hr)
{
    switch ((ULONG)hr) {
    case 0x00000000u: return "S_OK";
    case 0x00000001u: return "S_FALSE";
    case 0x80004001u: return "E_NOTIMPL";
    case 0x80004002u: return "E_NOINTERFACE";
    case 0x80004003u: return "E_POINTER";
    case 0x80004004u: return "E_ABORT";
    case 0x80004005u: return "E_FAIL";
    case 0x8000000Bu: return "E_BOUNDS";
    case 0x8000000Cu: return "E_CHANGED_STATE";
    case 0x8000000Eu: return "E_ILLEGAL_METHOD_CALL";
    case 0x8007000Eu: return "E_OUTOFMEMORY";
    case 0x80070057u: return "E_INVALIDARG";
    case 0x80040154u: return "REGDB_E_CLASSNOTREG";
    case 0x887A0005u: return "DXGI_ERROR_DEVICE_REMOVED";
    case 0x887A0007u: return "DXGI_ERROR_DEVICE_RESET";
    case 0x887A0001u: return "DXGI_ERROR_INVALID_CALL";
    default: return "";
    }
}

/* game RVA of a code address inside the game image, 0 when outside */
static DWORD x104_game_rva_of(ULONG_PTR address)
{
    ULONG_PTR base = (ULONG_PTR)x104.image;
    address &= ~(ULONG_PTR)1;
    if (!base || address < base || address - base >= 0x01000000u) return 0;
    return (DWORD)(address - base);
}

static void x104_flag_set(volatile LONG *flag, const char *what)
{
    if (__sync_val_compare_and_swap(flag, 0, 1) == 0) x104_logf("X104 flag: %s", what);
}

static BOOL x104_on_ui_thread(void)
{
    return x104.ui_thread_id && GetCurrentThreadId() == x104.ui_thread_id;
}

/* ======================================================================================================================
 * 3. HSTRING helpers.  On ARM every HSTRING is a REAL combase HSTRING (WindowsCreateString etc. forward to combase in api_compat.c).
 *    Rules: an HSTRING the host RECEIVES as an argument is borrowed (never delete it); an HSTRING the host RETURNS through an out
 *    pointer is created with x104_hstring_create and owned by the game afterwards (the game deletes it).
 * ====================================================================================================================== */
static HRESULT x104_hstring_create(const wchar_t *text, HSTRING *out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!text || !text[0]) return S_OK;                    /* NULL HSTRING == empty string */
    return WindowsCreateString(text, (UINT32)lstrlenW(text), out);
}

/* copies at most capacity-1 characters (always NUL terminated); returns the full length of the HSTRING */
static UINT32 x104_hstring_wide(HSTRING string, wchar_t *buffer, UINT32 capacity)
{
    UINT32 length = 0;
    const wchar_t *raw = string ? WindowsGetStringRawBuffer(string, &length) : NULL;
    UINT32 copy;
    if (!buffer || !capacity) return length;
    copy = raw ? (length < capacity - 1 ? length : capacity - 1) : 0;
    if (copy) memcpy(buffer, raw, copy * sizeof(wchar_t));
    buffer[copy] = 0;
    return length;
}

static BOOL x104_hstring_equals(HSTRING string, const wchar_t *text)
{
    UINT32 length = 0;
    const wchar_t *raw = string ? WindowsGetStringRawBuffer(string, &length) : NULL;
    UINT32 index;
    if (!text) text = L"";
    if (!raw) return text[0] == 0;
    for (index = 0; index < length; ++index) {
        if (text[index] != raw[index]) return FALSE;
    }
    return text[length] == 0;
}

/* printable ASCII copy for logs ('?' for anything else) */
static void x104_hstring_ascii(HSTRING string, char *buffer, UINT32 capacity)
{
    UINT32 length = 0;
    const wchar_t *raw = string ? WindowsGetStringRawBuffer(string, &length) : NULL;
    UINT32 index = 0;
    if (!buffer || !capacity) return;
    while (raw && index < length && index + 1 < capacity) {
        buffer[index] = (raw[index] >= 0x20 && raw[index] < 0x7f) ? (char)raw[index] : '?';
        ++index;
    }
    buffer[index] = 0;
}

static void x104_hstring_free(HSTRING string)
{
    if (string) WindowsDeleteString(string);
}

/* wide -> ASCII (printable), for logs of wchar_t paths */
static void x104_narrow(char *out, SIZE_T capacity, const wchar_t *text)
{
    SIZE_T index = 0;
    if (!out || !capacity) return;
    while (text && text[index] && index + 1 < capacity) {
        out[index] = (text[index] >= 0x20 && text[index] < 0x7f) ? (char)text[index] : '?';
        ++index;
    }
    out[index] = 0;
}

/* ======================================================================================================================
 * 4. locks and once-init.  Leaf locks only: NEVER call into the game (or another unit) while holding one.
 *    A zero-initialised `volatile LONG` is a valid unlocked lock (usable in static storage without init).
 * ====================================================================================================================== */
typedef volatile LONG X104Lock;

static void x104_lock(X104Lock *lock)
{
    unsigned spins = 0;
    while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE)) {
        ++spins;
        if (spins > 400) Sleep(1);
        else if (spins > 40) Sleep(0);
    }
}

static void x104_unlock(X104Lock *lock)
{
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

/* run-once: `if (x104_once_begin(&state)) { ...init...; x104_once_end(&state); }` - other threads wait until the init finished */
static BOOL x104_once_begin(volatile LONG *state)
{
    if (__atomic_load_n(state, __ATOMIC_ACQUIRE) == 2) return FALSE;
    if (__sync_val_compare_and_swap(state, 0, 1) == 0) return TRUE;
    while (__atomic_load_n(state, __ATOMIC_ACQUIRE) != 2) Sleep(0);
    return FALSE;
}

static void x104_once_end(volatile LONG *state)
{
    __atomic_store_n(state, 2, __ATOMIC_RELEASE);
}

/* ======================================================================================================================
 * 5. object / view / class model
 *
 *    A host object is ONE X104Object; every COM interface it implements is a separate X104View {vtbl, obj, desc} stored inside it.
 *    The pointer handed to the game for an interface IS the address of that view, so a thunk receives `self` = X104View* and
 *    reaches the object with X104_OBJ(self) and its own struct with X104_THIS(MyStruct, self).
 *    - IUnknown / IInspectable identity = views[0] (the first interface of the class descriptor, which must be IInspectable-derived).
 *    - QueryInterface looks the IID up in the class's interface table, then IAgileObject (class flag), then the optional qi_hook.
 *    - Objects are NEVER freed.  Release only decrements (clamped at 1, underflow is logged).  Static singletons are allowed.
 *    - No COM aggregation delegation: when a game object aggregates a host inner object (Application, Page) the host creates it with
 *      x104_aggregate_create(), which AddRefs the outer once and keeps it for ever (HOST_DESIGN.md 5.2).
 * ====================================================================================================================== */
#define X104_MAX_VIEWS      12
#define X104_VTBL_SLACK     4          /* extra stub slots behind the declared interface so an out-of-range call is reported, not executed */
#define X104_CLASS_AGILE    0x0001u    /* QI(IAgileObject) succeeds */
#define X104_CLASS_FACTORY  0x0002u    /* informational (activation factory / statics object) */

struct X104Object;
typedef struct X104IfaceDesc {
    const GUID *iid;
    const void **vtbl;          /* mutable array: NSLOTS + X104_VTBL_SLACK entries; NULL entries are filled with logging stubs */
    unsigned slots;             /* number of entries of vtbl */
    const char *name;
} X104IfaceDesc;

typedef struct X104View {
    const void **vtbl;          /* MUST be the first member (this is the COM object pointer) */
    struct X104Object *obj;
    const X104IfaceDesc *desc;
} X104View;

/* optional QI extension: return the view to hand out for `iid` (or NULL for E_NOINTERFACE) */
typedef X104View *(*X104QiHook)(struct X104Object *obj, const GUID *iid);

typedef struct X104Class {
    const char *name;                   /* short name for logs */
    const wchar_t *runtime_name;        /* IInspectable::GetRuntimeClassName, e.g. L"Windows.UI.Xaml.Controls.SwapChainPanel" */
    const X104IfaceDesc *ifaces;        /* ifaces[0] = primary (identity) interface */
    unsigned iface_count;
    unsigned flags;
    X104QiHook qi_hook;
} X104Class;

typedef struct X104Object {
    const X104Class *cls;
    volatile LONG refs;
    LONG serial;                        /* unique per object, in the logs */
    unsigned view_count;
    void *outer;                        /* free for the units: controlling unknown of an aggregated game object */
    void *user[2];                      /* free for the units */
    X104View views[X104_MAX_VIEWS];
} X104Object;

#define X104_OBJ(self)         (((X104View *)(self))->obj)
#define X104_THIS(type, self)  ((type *)(((X104View *)(self))->obj))

/* vtable declaration: `X104_VTBL(vt, IFrameworkElement) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IFrameworkElement, get_ActualWidth, f), ... };` */
#define X104_VTBL(name, iface)  static const void *name[X104_NSLOTS_##iface + X104_VTBL_SLACK]
#define X104_VTBL_N(name, slots) static const void *name[(slots) + X104_VTBL_SLACK]
/* Register a thunk in a vtable initializer: `X104_VTBL(vt, IFoo) = { X104_INSPECTABLE_SLOTS, X104_SLOT(IFoo, Bar, my_bar_thunk), ... };`
 * The thunk's C type must be EXACTLY the generated typedef X104_FN_IFoo_Bar (units/SLOTS.md); otherwise this is a compile error ("initializer element is not a
 * compile-time constant" pointing at the X104_SLOT line): __builtin_choose_expr picks a non-constant call for a mismatch.  A slot without a typedef (unsupported
 * parameter type) cannot be registered either - extend tools/arm32/gen_0104_iids.py.  Direct `[X104_SLOT_IFoo_Bar] = fn` initializers are refused by try_0104.py. */
extern const void *x104_slot_signature_mismatch(const char *interface_and_method);
#define X104_SLOT(iface, method, thunk)     [X104_SLOT_##iface##_##method] = __builtin_choose_expr(__builtin_types_compatible_p(__typeof__(&(thunk)), X104_FN_##iface##_##method),                                                           (const void *)(thunk), x104_slot_signature_mismatch(#iface "." #method))
/* a slot that deliberately ignores ALL its arguments (x104_nop, or a thunk `HRESULT f(void *self)`): checked to have exactly that shape.  Safe on ARM (the caller
 * pops stack arguments); on the x86 host-side test build (stdcall) only use it for slots the test does not call with arguments. */
#define X104_SLOT_NOARGS(iface, method, thunk)     [X104_SLOT_##iface##_##method] = __builtin_choose_expr(__builtin_types_compatible_p(__typeof__(&(thunk)), HRESULT (X104_API *)(void *)),                                                           (const void *)(thunk), x104_slot_signature_mismatch(#iface "." #method " (NOARGS)"))

#define X104_IFACE(iface, vtbl_array)  { &X104_IID_##iface, (vtbl_array), (unsigned)ARRAYSIZE(vtbl_array), #iface }
#define X104_IFACE_IID(iid_ptr, label, vtbl_array)  { (iid_ptr), (vtbl_array), (unsigned)ARRAYSIZE(vtbl_array), (label) }
#define X104_CLASS_DEF(var, short_name, runtime, iface_array, class_flags, hook) \
    static const X104Class var = { (short_name), (runtime), (iface_array), (unsigned)ARRAYSIZE(iface_array), (class_flags), (hook) }

/* lazily created, thread-safe singleton accessor for a statics/factory object:
 *     X104_SINGLETON(my_factory, my_class, MyFactoryStruct)      -> static X104Object *my_factory(void)   (struct starts with X104Object base;) */
#define X104_SINGLETON(getter, cls, type)     static X104Object *getter(void)     {         static volatile LONG state_;         static X104Object *object_;         if (x104_once_begin(&state_)) { object_ = x104_object_new(&(cls), sizeof(type)); x104_once_end(&state_); }         return object_;     }

/* count live objects only for the final statistics */
static volatile LONG x104_object_serial;
static volatile LONG x104_qi_miss_count;

static HRESULT X104_API x104_qi(void *self, const GUID *iid, void **out);
static ULONG X104_API x104_addref(void *self);
static ULONG X104_API x104_release(void *self);
static HRESULT X104_API x104_get_iids(void *self, ULONG *count, GUID **iids);
static HRESULT X104_API x104_get_runtime_class_name(void *self, HSTRING *name);
static HRESULT X104_API x104_get_trust_level(void *self, int *level);
/* first slots of every IInspectable-derived vtable (slots 0..5) and of every IUnknown-only vtable (slots 0..2) */
#define X104_INSPECTABLE_SLOTS  x104_qi, x104_addref, x104_release, x104_get_iids, x104_get_runtime_class_name, x104_get_trust_level
#define X104_UNKNOWN_SLOTS      x104_qi, x104_addref, x104_release

static void x104_vtable_fill_holes(const X104IfaceDesc *desc);

/* initialise storage (static singleton or the start of a bigger struct) as an object of `cls`, refs = 1 */
static void x104_object_init(X104Object *obj, const X104Class *cls)
{
    unsigned index;
    obj->cls = cls;
    obj->refs = 1;
    obj->serial = InterlockedIncrement(&x104_object_serial);
    if (cls->iface_count > X104_MAX_VIEWS) {
        x104_logf("X104 FATAL: class %s declares %u interfaces, the limit is %u", cls->name, cls->iface_count, (unsigned)X104_MAX_VIEWS);
        obj->view_count = X104_MAX_VIEWS;
    } else {
        obj->view_count = cls->iface_count;
    }
    for (index = 0; index < obj->view_count; ++index) {
        x104_vtable_fill_holes(&cls->ifaces[index]);
        obj->views[index].vtbl = cls->ifaces[index].vtbl;
        obj->views[index].obj = obj;
        obj->views[index].desc = &cls->ifaces[index];
    }
}

/* heap object of `size` bytes (size >= sizeof(X104Object); a unit struct starts with `X104Object base;`), zero-filled, refs = 1 */
static X104Object *x104_object_new(const X104Class *cls, SIZE_T size)
{
    X104Object *obj;
    if (size < sizeof(X104Object)) size = sizeof(X104Object);
    obj = (X104Object *)HeapAlloc(GetProcessHeap(), X104_HEAP_ZERO_MEMORY, size);
    if (!obj) {
        x104_logf("X104 FATAL: out of memory creating a %s object (%lu bytes)", cls->name, (unsigned long)size);
        return NULL;
    }
    x104_object_init(obj, cls);
    return obj;
}

/* interface pointer of the identity view (IInspectable); NOT AddRef'd */
static void *x104_object_iface0(X104Object *obj)
{
    return obj ? (void *)&obj->views[0] : NULL;
}

/* the view of `obj` that implements `iid`, NULL if none (no AddRef, no logging) */
static X104View *x104_object_find_view(X104Object *obj, const GUID *iid)
{
    unsigned index;
    X104View *hooked;
    if (!obj || !iid) return NULL;
    if (memcmp(iid, &X104_IID_IUnknown, sizeof(GUID)) == 0 || memcmp(iid, &X104_IID_IInspectable, sizeof(GUID)) == 0) return &obj->views[0];
    for (index = 0; index < obj->view_count; ++index) {
        if (memcmp(obj->cls->ifaces[index].iid, iid, sizeof(GUID)) == 0) return &obj->views[index];
    }
    if ((obj->cls->flags & X104_CLASS_AGILE) && memcmp(iid, &X104_IID_IAgileObject, sizeof(GUID)) == 0) return &obj->views[0];
    if (obj->cls->qi_hook) {
        hooked = obj->cls->qi_hook(obj, iid);
        if (hooked) return hooked;
    }
    return NULL;
}

/* QueryInterface on a host object: AddRefs and stores the view; logs every miss (these are the "the game wants an interface we
 * do not implement" lines of the device log) */
static HRESULT x104_object_qi(X104Object *obj, const GUID *iid, void **out)
{
    X104View *view;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!obj || !iid) return E_POINTER;
    view = x104_object_find_view(obj, iid);
    if (!view) {
        if (InterlockedIncrement(&x104_qi_miss_count) <= 300) {
            char text[96];
            x104_describe_iid(text, iid);
            x104_logf("X104 QI MISS: %s obj=%p (serial %ld) does not implement %s -> E_NOINTERFACE", obj->cls->name, (void *)obj,
                      (long)obj->serial, text);
        }
        return E_NOINTERFACE;
    }
    InterlockedIncrement(&obj->refs);
    *out = view;
    return S_OK;
}

static HRESULT X104_API x104_qi(void *self, const GUID *iid, void **out)
{
    if (!self) return E_POINTER;
    return x104_object_qi(X104_OBJ(self), iid, out);
}

static ULONG X104_API x104_addref(void *self)
{
    return self ? (ULONG)InterlockedIncrement(&X104_OBJ(self)->refs) : 0;
}

static ULONG X104_API x104_release(void *self)
{
    X104Object *obj;
    LONG value;
    if (!self) return 0;
    obj = X104_OBJ(self);
    value = InterlockedDecrement(&obj->refs);
    if (value < 1) {
        static volatile LONG underflow_count;
        InterlockedExchange(&obj->refs, 1);      /* objects are never freed: clamp */
        X104_LOG_LIMITED(&underflow_count, 40, "X104 WARNING: refcount underflow on %s obj=%p (serial %ld) - clamped to 1", obj->cls->name,
                         (void *)obj, (long)obj->serial);
        value = 1;
    }
    return (ULONG)value;
}

static HRESULT X104_API x104_get_iids(void *self, ULONG *count, GUID **iids)
{
    (void)self;
    if (count) *count = 0;
    if (iids) *iids = NULL;
    return S_OK;
}

static HRESULT X104_API x104_get_runtime_class_name(void *self, HSTRING *name)
{
    if (!name) return E_POINTER;
    *name = NULL;
    if (!self) return E_POINTER;
    return x104_hstring_create(X104_OBJ(self)->cls->runtime_name, name);
}

static HRESULT X104_API x104_get_trust_level(void *self, int *level)
{
    (void)self;
    if (!level) return E_POINTER;
    *level = 0;     /* BaseTrust */
    return S_OK;
}

/* a slot that simply succeeds: `X104_SLOT(IFoo, Bar, x104_nop)` (any argument list; the arguments are ignored) */
static HRESULT X104_API x104_nop(void *self) { (void)self; return S_OK; }

/* `*out = AddRef'd interface of a host object` for getters like get_CoreWindow / get_Dispatcher; a NULL object is a logged E_FAIL (never a silent NULL) */
static HRESULT x104_return_iface(X104Object *obj, const GUID *iid, void **out, const char *what)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!obj) {
        x104_logf("X104 ERROR: %s requested but the owning unit has no such object (not implemented / not initialised)", what ? what : "host object");
        return E_FAIL;
    }
    return x104_object_qi(obj, iid, out);
}

/* raw COM helpers for objects the GAME owns (delegates, event args, the inner objects we are handed) */
static ULONG x104_raw_addref(void *unknown)
{
    return unknown ? ((ULONG (X104_API *)(void *))((void **)*(void ***)unknown)[1])(unknown) : 0;
}

static ULONG x104_raw_release(void *unknown)
{
    return unknown ? ((ULONG (X104_API *)(void *))((void **)*(void ***)unknown)[2])(unknown) : 0;
}

static HRESULT x104_raw_qi(void *unknown, const GUID *iid, void **out)
{
    if (out) *out = NULL;
    if (!unknown || !out) return E_POINTER;
    return ((HRESULT (X104_API *)(void *, const GUID *, void **))((void **)*(void ***)unknown)[0])(unknown, iid, out);
}

/* vtable slot `slot` of a COM object as a typed function pointer: X104_VFN(obj, 3, HRESULT (X104_API *)(void*, void*, void*)) */
#define X104_VFN(obj, slot, type)  ((type)(((void **)*(void ***)(obj))[(slot)]))

/* Aggregation protocol (IApplicationFactory::CreateInstance(outer, &inner, &instance), IPageFactory::CreateInstance, IDispatcherTimerFactory::CreateInstance):
 * create a host object of class `cls` (struct size `size`, starting with X104Object), remember `outer` in obj->outer, AddRef `outer` ONCE and never release it (the real
 * runtime keeps the outer alive; startup_flow.verdicts m9), hand back
 *     *inner    = the identity view (views[0], IInspectable) AddRef'd   - what the game stores as its "inner" pointer,
 *     *instance = the view of `iid_instance` AddRef'd                   - the DEFAULT interface of the class (IApplication, IPage ...), what the factory returns as the instance;
 *                 iid_instance == NULL means views[0].
 * `inner` / `instance` may be NULL pointers (the caller does not want them).  *object_out (optional) receives the object (refs: 1 host + 1 per returned pointer).
 * Everything is logged (rule: a failed aggregation must name the factory).  Returns S_OK, E_OUTOFMEMORY or E_NOINTERFACE (class lacks iid_instance). */
static HRESULT x104_aggregate_create(const X104Class *cls, SIZE_T size, void *outer, const GUID *iid_instance, void **inner, void **instance,
                                     X104Object **object_out, const char *what)
{
    X104Object *obj;
    X104View *view;
    char iid_text[96];
    if (object_out) *object_out = NULL;
    if (inner) *inner = NULL;
    if (instance) *instance = NULL;
    obj = x104_object_new(cls, size);
    if (!obj) {
        x104_logf("X104 ERROR: %s: out of memory creating the aggregated %s object", what ? what : "aggregate", cls->name);
        return E_OUTOFMEMORY;
    }
    view = iid_instance ? x104_object_find_view(obj, iid_instance) : &obj->views[0];
    if (!view) {
        x104_describe_iid(iid_text, iid_instance);
        x104_logf("X104 ERROR: %s: class %s does not implement the instance interface %s -> E_NOINTERFACE", what ? what : "aggregate", cls->name, iid_text);
        return E_NOINTERFACE;
    }
    obj->outer = outer;
    if (outer) x104_raw_addref(outer);                      /* the runtime would keep `outer` alive for the lifetime of the process: so do we */
    if (inner) {
        *inner = &obj->views[0];
        InterlockedIncrement(&obj->refs);
    }
    if (instance) {
        *instance = view;
        InterlockedIncrement(&obj->refs);
    }
    if (object_out) *object_out = obj;
    x104_logf("X104 aggregate: %s created %s obj=%p (serial %ld): outer=%p -> inner=%p (%s) instance=%p (%s), outer AddRef'd once and kept", what ? what : "?", cls->name,
              (void *)obj, (long)obj->serial, outer, inner ? *inner : NULL, obj->views[0].desc->name, instance ? *instance : NULL, view->desc->name);
    return S_OK;
}

/* ======================================================================================================================
 * 6. hole-filling stubs: every NULL vtable slot answers E_NOTIMPL and names itself in the log (unknown-slot policy: fail loudly)
 * ====================================================================================================================== */
static struct { const void *desc; unsigned slot; LONG count; } x104_hole_seen[96];
static volatile LONG x104_hole_total;
static X104Lock x104_hole_lock;

static HRESULT __attribute__((noinline)) x104_hole_report(void *self, unsigned slot, ULONG_PTR caller)
{
    const X104View *view = (const X104View *)self;
    const X104Object *obj = NULL;
    const X104IfaceDesc *desc = NULL;
    unsigned index;
    LONG count = 0;
    BOOL first = FALSE;
    DWORD rva = x104_game_rva_of(caller);
    char iid_text[96];

    if (view && !IsBadReadPtr(view, sizeof(*view)) && view->obj && !IsBadReadPtr(view->obj, sizeof(X104Object)) &&
        view >= view->obj->views && view < view->obj->views + X104_MAX_VIEWS && view->desc) {
        obj = view->obj;
        desc = view->desc;
    }
    InterlockedIncrement(&x104_hole_total);
    x104_lock(&x104_hole_lock);
    for (index = 0; index < ARRAYSIZE(x104_hole_seen); ++index) {
        if (x104_hole_seen[index].desc == desc && x104_hole_seen[index].slot == slot && x104_hole_seen[index].count) {
            count = ++x104_hole_seen[index].count;
            break;
        }
        if (!x104_hole_seen[index].count) {
            x104_hole_seen[index].desc = desc;
            x104_hole_seen[index].slot = slot;
            x104_hole_seen[index].count = 1;
            count = 1;
            first = TRUE;
            break;
        }
    }
    x104_unlock(&x104_hole_lock);
    if (index == ARRAYSIZE(x104_hole_seen)) {
        first = x104_hole_total <= 400;          /* table full: plain limit */
        count = 1;
    }
    if (first || (count & (count - 1)) == 0 /* 2,4,8,16,... occurrences */) {
        iid_text[0] = 0;
        if (desc) x104_describe_iid(iid_text, desc->iid);
        x104_logf("X104 UNIMPLEMENTED slot %u%s of %s on %s obj=%p (serial %ld) caller=%08lx (game rva %08lx) call #%ld -> E_NOTIMPL",
                  slot, (desc && slot >= desc->slots - X104_VTBL_SLACK) ? " (BEYOND the declared interface!)" : "",
                  desc ? desc->name : "(not a host view)", obj ? obj->cls->name : "?", (void *)obj, obj ? (long)obj->serial : -1L,
                  (unsigned long)caller, (unsigned long)rva, (long)count);
        if (first && iid_text[0]) x104_logf("X104   interface %s", iid_text);
    }
    return X104_E_NOT_IMPL;
}

#define X104_HOLE(n) static HRESULT X104_API x104_hole_##n(void *self) \
    { return x104_hole_report(self, (n), (ULONG_PTR)__builtin_return_address(0)); }
X104_HOLE(0) X104_HOLE(1) X104_HOLE(2) X104_HOLE(3) X104_HOLE(4) X104_HOLE(5) X104_HOLE(6) X104_HOLE(7)
X104_HOLE(8) X104_HOLE(9) X104_HOLE(10) X104_HOLE(11) X104_HOLE(12) X104_HOLE(13) X104_HOLE(14) X104_HOLE(15)
X104_HOLE(16) X104_HOLE(17) X104_HOLE(18) X104_HOLE(19) X104_HOLE(20) X104_HOLE(21) X104_HOLE(22) X104_HOLE(23)
X104_HOLE(24) X104_HOLE(25) X104_HOLE(26) X104_HOLE(27) X104_HOLE(28) X104_HOLE(29) X104_HOLE(30) X104_HOLE(31)
X104_HOLE(32) X104_HOLE(33) X104_HOLE(34) X104_HOLE(35) X104_HOLE(36) X104_HOLE(37) X104_HOLE(38) X104_HOLE(39)
X104_HOLE(40) X104_HOLE(41) X104_HOLE(42) X104_HOLE(43) X104_HOLE(44) X104_HOLE(45) X104_HOLE(46) X104_HOLE(47)
X104_HOLE(48) X104_HOLE(49) X104_HOLE(50) X104_HOLE(51) X104_HOLE(52) X104_HOLE(53) X104_HOLE(54) X104_HOLE(55)
X104_HOLE(56) X104_HOLE(57) X104_HOLE(58) X104_HOLE(59) X104_HOLE(60) X104_HOLE(61) X104_HOLE(62) X104_HOLE(63)
X104_HOLE(64) X104_HOLE(65) X104_HOLE(66) X104_HOLE(67) X104_HOLE(68) X104_HOLE(69) X104_HOLE(70) X104_HOLE(71)
X104_HOLE(72) X104_HOLE(73) X104_HOLE(74) X104_HOLE(75) X104_HOLE(76) X104_HOLE(77) X104_HOLE(78) X104_HOLE(79)
X104_HOLE(80) X104_HOLE(81) X104_HOLE(82) X104_HOLE(83) X104_HOLE(84) X104_HOLE(85) X104_HOLE(86) X104_HOLE(87)
X104_HOLE(88) X104_HOLE(89) X104_HOLE(90) X104_HOLE(91) X104_HOLE(92) X104_HOLE(93) X104_HOLE(94) X104_HOLE(95)
X104_HOLE(96) X104_HOLE(97) X104_HOLE(98) X104_HOLE(99) X104_HOLE(100) X104_HOLE(101) X104_HOLE(102) X104_HOLE(103)
X104_HOLE(104) X104_HOLE(105) X104_HOLE(106) X104_HOLE(107) X104_HOLE(108) X104_HOLE(109) X104_HOLE(110) X104_HOLE(111)
X104_HOLE(112) X104_HOLE(113) X104_HOLE(114) X104_HOLE(115) X104_HOLE(116) X104_HOLE(117) X104_HOLE(118) X104_HOLE(119)
X104_HOLE(120) X104_HOLE(121) X104_HOLE(122) X104_HOLE(123) X104_HOLE(124) X104_HOLE(125) X104_HOLE(126) X104_HOLE(127)

static HRESULT X104_API x104_hole_overflow(void *self)
{
    return x104_hole_report(self, 999, (ULONG_PTR)__builtin_return_address(0));    /* slot >= 128 */
}

static const void *const x104_hole_table[128] = {
    x104_hole_0, x104_hole_1, x104_hole_2, x104_hole_3, x104_hole_4, x104_hole_5, x104_hole_6, x104_hole_7,
    x104_hole_8, x104_hole_9, x104_hole_10, x104_hole_11, x104_hole_12, x104_hole_13, x104_hole_14, x104_hole_15,
    x104_hole_16, x104_hole_17, x104_hole_18, x104_hole_19, x104_hole_20, x104_hole_21, x104_hole_22, x104_hole_23,
    x104_hole_24, x104_hole_25, x104_hole_26, x104_hole_27, x104_hole_28, x104_hole_29, x104_hole_30, x104_hole_31,
    x104_hole_32, x104_hole_33, x104_hole_34, x104_hole_35, x104_hole_36, x104_hole_37, x104_hole_38, x104_hole_39,
    x104_hole_40, x104_hole_41, x104_hole_42, x104_hole_43, x104_hole_44, x104_hole_45, x104_hole_46, x104_hole_47,
    x104_hole_48, x104_hole_49, x104_hole_50, x104_hole_51, x104_hole_52, x104_hole_53, x104_hole_54, x104_hole_55,
    x104_hole_56, x104_hole_57, x104_hole_58, x104_hole_59, x104_hole_60, x104_hole_61, x104_hole_62, x104_hole_63,
    x104_hole_64, x104_hole_65, x104_hole_66, x104_hole_67, x104_hole_68, x104_hole_69, x104_hole_70, x104_hole_71,
    x104_hole_72, x104_hole_73, x104_hole_74, x104_hole_75, x104_hole_76, x104_hole_77, x104_hole_78, x104_hole_79,
    x104_hole_80, x104_hole_81, x104_hole_82, x104_hole_83, x104_hole_84, x104_hole_85, x104_hole_86, x104_hole_87,
    x104_hole_88, x104_hole_89, x104_hole_90, x104_hole_91, x104_hole_92, x104_hole_93, x104_hole_94, x104_hole_95,
    x104_hole_96, x104_hole_97, x104_hole_98, x104_hole_99, x104_hole_100, x104_hole_101, x104_hole_102, x104_hole_103,
    x104_hole_104, x104_hole_105, x104_hole_106, x104_hole_107, x104_hole_108, x104_hole_109, x104_hole_110, x104_hole_111,
    x104_hole_112, x104_hole_113, x104_hole_114, x104_hole_115, x104_hole_116, x104_hole_117, x104_hole_118, x104_hole_119,
    x104_hole_120, x104_hole_121, x104_hole_122, x104_hole_123, x104_hole_124, x104_hole_125, x104_hole_126, x104_hole_127
};

/* every NULL entry of the interface's vtable becomes the stub of its slot number (idempotent, race-benign) */
static void x104_vtable_fill_holes(const X104IfaceDesc *desc)
{
    const void **vtbl = desc->vtbl;
    unsigned slot;
    for (slot = 0; slot < desc->slots; ++slot) {
        if (!vtbl[slot]) vtbl[slot] = slot < 128 ? x104_hole_table[slot] : (const void *)x104_hole_overflow;
    }
}

/* ======================================================================================================================
 * 7. event lists.  Protocol (verified: input_text.delegate_invoke, startup_flow 2a): the pointer given to add_X is the delegate object;
 *    it is invoked with vtbl[3](delegate, sender, args); the add_X out parameter is a 64-bit EventRegistrationToken; AddRef the
 *    delegate when storing it (vtbl[1]) and Release it on remove (vtbl[2]).
 *    Invocation snapshots the list under the lock, AddRefs each delegate, runs them OUTSIDE the lock (a handler may add/remove
 *    handlers or call back into the host) and Releases them.  RULE R1 (HOST_DESIGN.md 4): only call x104_event_invoke from the UI
 *    loop's top level (or from a pool thread for events that are defined as such), NEVER from inside a fake method the game called.
 * ====================================================================================================================== */
#define X104_EVENT_MAX 8

typedef struct X104Event {
    X104Lock lock;
    const char *name;               /* e.g. "SwapChainPanel.SizeChanged", set by the owner via x104_event_init or the static initialiser */
    unsigned count;
    void *handler[X104_EVENT_MAX];
    LONGLONG token[X104_EVENT_MAX];
    volatile LONG invocations;
} X104Event;

static volatile LONG x104_token_serial;

static void x104_event_init(X104Event *event, const char *name)
{
    memset(event, 0, sizeof(*event));
    event->name = name;
}

static HRESULT x104_event_add(X104Event *event, void *handler, LONGLONG *token_out)
{
    unsigned index;
    LONGLONG token;
    if (!token_out) return E_POINTER;
    *token_out = 0;
    if (!handler) return E_INVALIDARG;
    token = ((LONGLONG)0x104 << 32) | (UINT32)InterlockedIncrement(&x104_token_serial);
    x104_raw_addref(handler);
    x104_lock(&event->lock);
    for (index = 0; index < X104_EVENT_MAX; ++index) {
        if (!event->handler[index]) {
            event->handler[index] = handler;
            event->token[index] = token;
            ++event->count;
            break;
        }
    }
    x104_unlock(&event->lock);
    if (index == X104_EVENT_MAX) {
        x104_raw_release(handler);
        x104_logf("X104 ERROR: event %s has no free handler slot (%u max) - add_ refused with E_FAIL", event->name ? event->name : "?",
                  (unsigned)X104_EVENT_MAX);
        return E_FAIL;
    }
    *token_out = token;
    x104_logf("X104 event %s: handler %p added (token %08lx%08lx, %u registered)", event->name ? event->name : "?", handler,
              (unsigned long)(token >> 32), (unsigned long)(UINT32)token, event->count);
    return S_OK;
}

static HRESULT x104_event_remove(X104Event *event, LONGLONG token)
{
    unsigned index;
    void *removed = NULL;
    x104_lock(&event->lock);
    for (index = 0; index < X104_EVENT_MAX; ++index) {
        if (event->handler[index] && event->token[index] == token) {
            removed = event->handler[index];
            event->handler[index] = NULL;
            event->token[index] = 0;
            --event->count;
            break;
        }
    }
    x104_unlock(&event->lock);
    if (removed) x104_raw_release(removed);
    else x104_logf("X104 event %s: remove_ with unknown token %08lx%08lx ignored (S_OK)", event->name ? event->name : "?",
                   (unsigned long)(token >> 32), (unsigned long)(UINT32)token);
    return S_OK;
}

static unsigned x104_event_count(X104Event *event)
{
    return event->count;
}

/* invokes every registered delegate with (sender, args); returns how many were invoked.  Failing HRESULTs are logged. */
static unsigned x104_event_invoke(X104Event *event, void *sender, void *args)
{
    void *snapshot[X104_EVENT_MAX];
    unsigned count = 0;
    unsigned index;
    x104_lock(&event->lock);
    for (index = 0; index < X104_EVENT_MAX; ++index) {
        if (event->handler[index]) snapshot[count++] = event->handler[index];
    }
    for (index = 0; index < count; ++index) x104_raw_addref(snapshot[index]);
    x104_unlock(&event->lock);
    InterlockedIncrement(&event->invocations);
    for (index = 0; index < count; ++index) {
        HRESULT hr = X104_VFN(snapshot[index], 3, HRESULT (X104_API *)(void *, void *, void *))(snapshot[index], sender, args);
        if (FAILED(hr)) {
            x104_logf("X104 event %s: delegate %p Invoke returned 0x%08lx %s", event->name ? event->name : "?", snapshot[index],
                      (unsigned long)hr, x104_hr_name(hr));
        }
        x104_raw_release(snapshot[index]);
    }
    return count;
}

/* ======================================================================================================================
 * 8. guarded calls into game code (async work items must never let an exception kill a pool thread silently)
 * ====================================================================================================================== */
typedef struct X104Seh {
    DWORD code;
    void *address;
    ULONG_PTR info0;
    ULONG_PTR info1;
    DWORD rva;
} X104Seh;

static int x104_seh_filter(EXCEPTION_POINTERS *pointers, X104Seh *out)
{
    if (pointers && pointers->ExceptionRecord && out) {
        EXCEPTION_RECORD *record = pointers->ExceptionRecord;
        out->code = record->ExceptionCode;
        out->address = record->ExceptionAddress;
        out->info0 = record->NumberParameters > 0 ? record->ExceptionInformation[0] : 0;
        out->info1 = record->NumberParameters > 1 ? record->ExceptionInformation[1] : 0;
        out->rva = x104_game_rva_of((ULONG_PTR)record->ExceptionAddress);
    }
    return 1;   /* EXCEPTION_EXECUTE_HANDLER */
}

/* Invoke fn(a, b, c) (a COM delegate Invoke, a game lambda, ...) inside __try/__except.  Returns TRUE when no exception escaped;
 * *hr_out = the return value (E_FAIL after an exception).  A C++/CX exception that escaped is SEH code 0xE06D7363. */
static BOOL x104_call_guarded3(void *fn, void *a, void *b, void *c, HRESULT *hr_out, X104Seh *seh_out)
{
    BOOL clean = TRUE;
    HRESULT hr = S_OK;
    X104Seh local;
    memset(&local, 0, sizeof(local));
    __try {
        hr = ((HRESULT (X104_API *)(void *, void *, void *))fn)(a, b, c);
    } __except (x104_seh_filter((EXCEPTION_POINTERS *)_exception_info(), &local)) {
        clean = FALSE;
        hr = E_FAIL;
    }
    if (hr_out) *hr_out = hr;
    if (seh_out) *seh_out = local;
    return clean;
}

/* delegate->Invoke(sender, args) guarded; logs an escaped exception with `what` */
static HRESULT x104_invoke_delegate_guarded(void *delegate, void *sender, void *args, const char *what)
{
    HRESULT hr = S_OK;
    X104Seh seh;
    if (!delegate) return E_POINTER;
    if (!x104_call_guarded3(((void **)*(void ***)delegate)[3], delegate, sender, args, &hr, &seh)) {
        x104_logf("X104 EXCEPTION escaped from %s (delegate %p): code=%08lx at %p (game rva %08lx) info=%08lx %08lx%s", what ? what : "delegate",
                  delegate, (unsigned long)seh.code, seh.address, (unsigned long)seh.rva, (unsigned long)seh.info0,
                  (unsigned long)seh.info1, seh.code == 0xE06D7363u ? " [C++ exception]" : "");
    }
    return hr;
}

/* same as x104_event_invoke but every delegate runs inside __try/__except (use for events raised from shutdown / error paths) */
static unsigned x104_event_invoke_guarded(X104Event *event, void *sender, void *args)
{
    void *snapshot[X104_EVENT_MAX];
    unsigned count = 0;
    unsigned index;
    x104_lock(&event->lock);
    for (index = 0; index < X104_EVENT_MAX; ++index) {
        if (event->handler[index]) snapshot[count++] = event->handler[index];
    }
    for (index = 0; index < count; ++index) x104_raw_addref(snapshot[index]);
    x104_unlock(&event->lock);
    InterlockedIncrement(&event->invocations);
    for (index = 0; index < count; ++index) {
        HRESULT hr = x104_invoke_delegate_guarded(snapshot[index], sender, args, event->name ? event->name : "event");
        if (FAILED(hr)) {
            x104_logf("X104 event %s: delegate %p Invoke returned 0x%08lx %s", event->name ? event->name : "?", snapshot[index], (unsigned long)hr,
                      x104_hr_name(hr));
        }
        x104_raw_release(snapshot[index]);
    }
    return count;
}

/* ======================================================================================================================
 * 9. activation registry.  The game activates every WinRT class through ONE choke point (IAT 0x20B5FC); the boot hooks it with
 *    x104_activation_hook.  The class -> owner-unit table below is the single, explicit list of the classes the host fakes
 *    (facts startup_flow.2 + platform_services.activation.inventory).  Anything else goes to the real runtime and is logged as
 *    "X104 WinRT FALLBACK".
 * ====================================================================================================================== */
typedef struct X104FactoryDef {
    const wchar_t *class_name;
    X104Object *(*get_factory)(void);      /* returns the (lazily created, thread-safe) singleton factory/statics object, or NULL on failure */
} X104FactoryDef;

enum { X104_UNIT_ASYNC = 1, X104_UNIT_APP, X104_UNIT_PANEL, X104_UNIT_PLATFORM, X104_UNIT_INPUT };

static const struct { const wchar_t *class_name; int unit; } x104_class_owners[] = {
    { L"Windows.System.Threading.ThreadPool",                       X104_UNIT_ASYNC },
    { L"Windows.UI.Xaml.DispatcherTimer",                           X104_UNIT_ASYNC },
    { L"Windows.UI.Xaml.Application",                               X104_UNIT_APP },
    { L"Windows.UI.Xaml.Controls.Page",                             X104_UNIT_APP },
    { L"Windows.UI.Xaml.Window",                                    X104_UNIT_APP },
    { L"Windows.UI.Xaml.Controls.Canvas",                           X104_UNIT_APP },
    { L"Windows.UI.ViewManagement.StatusBar",                       X104_UNIT_APP },
    { L"Windows.Phone.UI.Input.HardwareButtons",                    X104_UNIT_APP },
    { L"Windows.Graphics.Display.DisplayInformation",               X104_UNIT_APP },
    { L"Windows.Foundation.Uri",                                    X104_UNIT_APP },
    { L"Windows.UI.ViewManagement.InputPane",                       X104_UNIT_INPUT },
    { L"Windows.UI.Core.CoreCursor",                                X104_UNIT_INPUT },
    { L"Windows.UI.Core.CoreWindow",                                X104_UNIT_INPUT },
    { L"Windows.Storage.ApplicationData",                           X104_UNIT_PLATFORM },
    { L"Windows.ApplicationModel.Package",                          X104_UNIT_PLATFORM },
    { L"Windows.System.MemoryManager",                              X104_UNIT_PLATFORM },
    { L"Windows.Phone.Devices.Notification.VibrationDevice",        X104_UNIT_PLATFORM },
    { L"Windows.Networking.Connectivity.NetworkInformation",        X104_UNIT_PLATFORM },
    { L"Windows.ApplicationModel.Store.CurrentAppSimulator",        X104_UNIT_PLATFORM },
    { L"Windows.ApplicationModel.Store.CurrentApp",                 X104_UNIT_PLATFORM },
    { L"Windows.Foundation.Diagnostics.AsyncCausalityTracer",       X104_UNIT_PLATFORM },
    { L"Windows.Web.Http.HttpRequestMessage",                       X104_UNIT_PLATFORM },
    { L"Windows.Web.Http.HttpMethod",                               X104_UNIT_PLATFORM },
    { L"Windows.Web.Http.HttpStringContent",                        X104_UNIT_PLATFORM },
    { L"Windows.Web.Http.HttpClient",                               X104_UNIT_PLATFORM },
    /* REDCON 1 (never activated by Minecraft PE 0.10.4) */
    { L"Windows.UI.Xaml.Controls.SwapChainPanel",                   X104_UNIT_PANEL },
    { L"Windows.UI.Xaml.Media.CompositionTarget",                   X104_UNIT_APP },
    { L"Windows.ApplicationModel.Core.CoreApplication",             X104_UNIT_APP },
    { L"Windows.UI.ViewManagement.ApplicationView",                 X104_UNIT_APP },
    { L"Windows.UI.Core.CoreWindowResizeManager",                   X104_UNIT_APP }
    /* Windows.Globalization.ApplicationLanguages, Windows.Foundation.PropertyValue and Windows.System.Launcher are left to the real runtime on purpose:
     * they work in a desktop process (logged as "X104 WinRT FALLBACK") */
};

/* Each unit file defines its hook; it returns S_OK (an AddRef'd interface for `iid` in *out), E_NOINTERFACE (class known, IID not),
 * another failure, or REGDB_E_CLASSNOTREG = "not implemented (yet)": the core then falls back to the real runtime and says so. */
static HRESULT x104_async_activate(const wchar_t *class_name, const GUID *iid, void **out);
static HRESULT x104_app_activate(const wchar_t *class_name, const GUID *iid, void **out);
static HRESULT x104_panel_activate(const wchar_t *class_name, const GUID *iid, void **out);
static HRESULT x104_platform_activate(const wchar_t *class_name, const GUID *iid, void **out);
static HRESULT x104_input_activate(const wchar_t *class_name, const GUID *iid, void **out);

/* helper for the unit hooks: resolve `class_name` in a unit's X104FactoryDef table and QI the factory object for `iid` */
static HRESULT x104_activate_from_table(const X104FactoryDef *defs, unsigned count, const wchar_t *class_name, const GUID *iid, void **out)
{
    unsigned index;
    for (index = 0; index < count; ++index) {
        if (lstrcmpW(defs[index].class_name, class_name) == 0) {
            X104Object *factory = defs[index].get_factory();
            if (!factory) {
                x104_logf("X104 ERROR: the factory object for a host class could not be created");
                return E_FAIL;
            }
            return x104_object_qi(factory, iid, out);
        }
    }
    return REGDB_E_CLASSNOTREG;
}

static volatile LONG x104_activation_count;

/* HRESULT x104_activation_override(class, iid, out): REGDB_E_CLASSNOTREG = the host does not (yet) fake the class (caller falls back) */
static HRESULT x104_activation_override(const wchar_t *class_name, const GUID *iid, void **out)
{
    unsigned index;
    int unit = 0;
    HRESULT hr = REGDB_E_CLASSNOTREG;
    LONG number = InterlockedIncrement(&x104_activation_count);
    char name[128];
    char text[96];
    const char *unit_name = "none";

    if (!out) return E_POINTER;
    *out = NULL;
    x104_narrow(name, sizeof(name), class_name);
    for (index = 0; class_name && index < ARRAYSIZE(x104_class_owners); ++index) {
        if (lstrcmpW(x104_class_owners[index].class_name, class_name) == 0) {
            unit = x104_class_owners[index].unit;
            break;
        }
    }
    switch (unit) {
    case X104_UNIT_ASYNC:    unit_name = "async";    hr = x104_async_activate(class_name, iid, out); break;
    case X104_UNIT_APP:      unit_name = "app";      hr = x104_app_activate(class_name, iid, out); break;
    case X104_UNIT_PANEL:    unit_name = "panel";    hr = x104_panel_activate(class_name, iid, out); break;
    case X104_UNIT_PLATFORM: unit_name = "platform"; hr = x104_platform_activate(class_name, iid, out); break;
    case X104_UNIT_INPUT:    unit_name = "input";    hr = x104_input_activate(class_name, iid, out); break;
    default: break;
    }
    if (number <= 600 || FAILED(hr)) {
        x104_describe_iid(text, iid);
        x104_logf("X104 activation #%ld %s %s -> unit %s: 0x%08lx %s%s", (long)number, name[0] ? name : "(null)", text, unit_name,
                  (unsigned long)hr, x104_hr_name(hr),
                  (unit && hr == REGDB_E_CLASSNOTREG) ? "  (class is owned by the unit but not implemented yet: falling back to the real runtime)" : "");
    }
    return hr;
}

typedef HRESULT (*X104ActivationFn)(const wchar_t *, const GUID *, void **);

/* the function installed in the game's IAT cell 0x20B5FC (and called from win32_get_activation_factory when host_is_0104) */
static HRESULT X104_API x104_activation_hook(const wchar_t *class_name, const GUID *iid, void **result)
{
    HRESULT hr;
    char name[128];
    char text[96];
    if (!result) return E_POINTER;
    *result = NULL;
    hr = x104_activation_override(class_name, iid, result);
    if (hr != REGDB_E_CLASSNOTREG) return hr;
    if (!x104.original_activation) return REGDB_E_CLASSNOTREG;
    hr = ((X104ActivationFn)x104.original_activation)(class_name, iid, result);
    x104_narrow(name, sizeof(name), class_name);
    x104_describe_iid(text, iid);
    x104_logf("X104 WinRT FALLBACK %s %s -> real runtime: 0x%08lx %s", name[0] ? name : "(null)", text, (unsigned long)hr, x104_hr_name(hr));
    return hr;
}

/* ======================================================================================================================
 * 10. game access helpers (peeking at the game's own objects; every read is guarded)
 * ====================================================================================================================== */
static BOOL x104_read_u32(const void *address, DWORD *value)
{
    if (value) *value = 0;
    if (!address || ((ULONG_PTR)address & 3u) || IsBadReadPtr(address, 4)) return FALSE;
    if (value) *value = *(const volatile DWORD *)address;
    return TRUE;
}

static BOOL x104_read_u8(const void *address, BYTE *value)
{
    if (value) *value = 0;
    if (!address || IsBadReadPtr(address, 1)) return FALSE;
    if (value) *value = *(const volatile BYTE *)address;
    return TRUE;
}

/* MainPage object (outer - 8), NULL before IPageFactory.CreateInstance */
static BYTE *x104_game_mainpage(void)
{
    return x104.mainpage_outer ? (BYTE *)x104.mainpage_outer - X104_MAINPAGE_OUTER_OFFSET : NULL;
}

/* AppMain* = [MainPage+0x7C] (NULL until the MainPage constructor stored it) */
static BYTE *x104_game_appmain(void)
{
    DWORD value;
    BYTE *page = x104_game_mainpage();
    return (page && x104_read_u32(page + X104_MAINPAGE_APPMAIN, &value)) ? (BYTE *)(ULONG_PTR)value : NULL;
}

/* MinecraftClient* = [AppMain+0x34] */
static BYTE *x104_game_client(void)
{
    DWORD value;
    BYTE *app = x104_game_appmain();
    return (app && x104_read_u32(app + X104_APPMAIN_CLIENT, &value)) ? (BYTE *)(ULONG_PTR)value : NULL;
}

/* client+0xC (AppPlatform** holder) != NULL: written by runInit at the START of the init job.  SizeChanged before it crashes at rva 0x58718. */
static BOOL x104_game_client_has_holder(void)
{
    DWORD value;
    BYTE *client = x104_game_client();
    return client && x104_read_u32(client + X104_CLIENT_HOLDER, &value) && value != 0;
}

/* AppMain+0x3C (isReady): set by the init job after MinecraftClient::init returned.  A game without the XAML-page template has no init job: it is ready once
 * OnLaunched returned (REDCON 1 creates its engine, device and swap chain synchronously inside OnLaunched). */
static BOOL x104_game_is_ready(void)
{
    BYTE value;
    BYTE *app;
    if (!x104_feat(X104_FEAT_XAML_PAGE)) return x104.f_launched != 0;
    app = x104_game_appmain();
    return app && x104_read_u8(app + X104_APPMAIN_READY, &value) && value != 0;
}

/* verify the bytes at image+rva before patching/calling (logs got/expected on mismatch) */
static BOOL x104_check_game_bytes(DWORD rva, const BYTE *expected, SIZE_T size, const char *what)
{
    return arm32_check_bytes(x104.image + rva, expected, size, what);
}

/* ======================================================================================================================
 * 11. XAML element mixin: IDependencyObject / IUIElement / IFrameworkElement shared by Page, SwapChainPanel, Image, TextBox, Button.
 *     A unit struct starts with `X104Element base;` and lists X104_ELEMENT_IFACES first in its class's interface table:
 *         static const X104IfaceDesc my_ifaces[] = { X104_ELEMENT_IFACES, X104_IFACE(ISwapChainPanel, vt_scp), ... };
 *     NOTE: views[0] must be IInspectable-derived: IDependencyObject is (it is the first of X104_ELEMENT_IFACES).
 *     Implemented slots (everything else is a logging hole):
 *       IDependencyObject  11 get_Dispatcher
 *       IUIElement         21 get_Visibility  22 put_Visibility  23 get_RenderSize(Size*)
 *       IFrameworkElement  13 get_ActualWidth(double*) 14 get_ActualHeight(double*) 47 add_SizeChanged 48 remove_SizeChanged
 *                          51 FindName -> x104_element.find_name (NULL = E_FAIL with a log)
 * ====================================================================================================================== */
typedef struct X104Element {
    X104Object base;
    int visibility;                     /* 0 Visible, 1 Collapsed (Windows.UI.Xaml.Visibility) */
    float render_w, render_h;           /* IUIElement.RenderSize */
    double actual_w, actual_h;          /* IFrameworkElement.ActualWidth/Height (DIPs) */
    X104Event size_changed;             /* IFrameworkElement.SizeChanged (delegate = SizeChangedEventHandler) */
    HRESULT (*find_name)(struct X104Element *element, HSTRING name, void **result);   /* optional (Page) */
    const char *label;                  /* "Page", "SwapChainPanel", ... for logs */
} X104Element;

#define X104_ELEMENT(self)  X104_THIS(X104Element, self)

/* async unit: the CoreDispatcher of the UI thread; returned as an object (QI it for ICoreDispatcher) */
static X104Object *x104_async_dispatcher_object(void);

static HRESULT X104_API x104_dep_get_dispatcher(void *self, void **result)
{
    X104Object *dispatcher;
    (void)self;
    if (!result) return E_POINTER;
    dispatcher = x104_async_dispatcher_object();
    if (!dispatcher) {
        *result = NULL;
        x104_logf("X104 ERROR: DependencyObject.get_Dispatcher but the async unit has no dispatcher object");
        return E_FAIL;
    }
    return x104_object_qi(dispatcher, &X104_IID_ICoreDispatcher, result);
}

static HRESULT X104_API x104_ui_get_visibility(void *self, int *value)
{
    if (!value) return E_POINTER;
    *value = X104_ELEMENT(self)->visibility;
    return S_OK;
}

static HRESULT X104_API x104_ui_put_visibility(void *self, int value)
{
    static volatile LONG logged;
    X104Element *element = X104_ELEMENT(self);
    element->visibility = value;
    X104_LOG_LIMITED(&logged, 40, "X104 %s.Visibility = %d (%s)", element->label ? element->label : "element", value,
                     value ? "Collapsed" : "Visible");
    return S_OK;
}

static HRESULT X104_API x104_ui_get_render_size(void *self, X104Size *size)
{
    X104Element *element = X104_ELEMENT(self);
    if (!size) return E_POINTER;
    size->Width = element->render_w;
    size->Height = element->render_h;
    return S_OK;
}

static HRESULT X104_API x104_fe_get_actual_width(void *self, double *value)
{
    if (!value) return E_POINTER;
    *value = X104_ELEMENT(self)->actual_w;
    return S_OK;
}

static HRESULT X104_API x104_fe_get_actual_height(void *self, double *value)
{
    if (!value) return E_POINTER;
    *value = X104_ELEMENT(self)->actual_h;
    return S_OK;
}

static HRESULT X104_API x104_fe_add_size_changed(void *self, void *handler, LONGLONG *token)
{
    return x104_event_add(&X104_ELEMENT(self)->size_changed, handler, token);
}

static HRESULT X104_API x104_fe_remove_size_changed(void *self, LONGLONG token)
{
    return x104_event_remove(&X104_ELEMENT(self)->size_changed, token);
}

static HRESULT X104_API x104_fe_find_name(void *self, HSTRING name, void **result)
{
    X104Element *element = X104_ELEMENT(self);
    char text[96];
    if (!result) return E_POINTER;
    *result = NULL;
    x104_hstring_ascii(name, text, sizeof(text));
    if (!element->find_name) {
        x104_logf("X104 ERROR: %s.FindName(\"%s\") but this element has no find_name callback", element->label ? element->label : "element", text);
        return E_FAIL;
    }
    return element->find_name(element, name, result);
}

X104_VTBL(x104_vt_dependencyobject, IDependencyObject) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IDependencyObject, get_Dispatcher, x104_dep_get_dispatcher)
};
X104_VTBL(x104_vt_uielement, IUIElement) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IUIElement, get_Visibility, x104_ui_get_visibility),
    X104_SLOT(IUIElement, put_Visibility, x104_ui_put_visibility),
    X104_SLOT(IUIElement, get_RenderSize, x104_ui_get_render_size)
};
/* put_HorizontalAlignment / put_VerticalAlignment: REDCON 1 stretches its code-built SwapChainPanel (3 = Stretch); the host element always fills the window */
static HRESULT X104_API x104_fe_put_alignment(void *self, int value)
{
    static volatile LONG logged;
    X104_LOG_LIMITED(&logged, 4, "X104 element %s: put_Horizontal/VerticalAlignment(%d) accepted (no-op: the element always fills the client area)",
                     X104_THIS(X104Element, self)->label ? X104_THIS(X104Element, self)->label : "?", value);
    return S_OK;
}

X104_VTBL(x104_vt_frameworkelement, IFrameworkElement) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IFrameworkElement, put_HorizontalAlignment, x104_fe_put_alignment),
    X104_SLOT(IFrameworkElement, put_VerticalAlignment, x104_fe_put_alignment),
    X104_SLOT(IFrameworkElement, get_ActualWidth, x104_fe_get_actual_width),
    X104_SLOT(IFrameworkElement, get_ActualHeight, x104_fe_get_actual_height),
    X104_SLOT(IFrameworkElement, add_SizeChanged, x104_fe_add_size_changed),
    X104_SLOT(IFrameworkElement, remove_SizeChanged, x104_fe_remove_size_changed),
    X104_SLOT(IFrameworkElement, FindName, x104_fe_find_name)
};
#define X104_ELEMENT_IFACES \
    X104_IFACE(IDependencyObject, x104_vt_dependencyobject), \
    X104_IFACE(IUIElement, x104_vt_uielement), \
    X104_IFACE(IFrameworkElement, x104_vt_frameworkelement)

static X104Element *x104_element_new(const X104Class *cls, SIZE_T size, const char *label)
{
    X104Element *element = (X104Element *)x104_object_new(cls, size < sizeof(X104Element) ? sizeof(X104Element) : size);
    if (!element) return NULL;
    element->label = label;
    x104_event_init(&element->size_changed, "FrameworkElement.SizeChanged");
    return element;
}

static void x104_element_set_sizes(X104Element *element, double width, double height)
{
    element->actual_w = width;
    element->actual_h = height;
    element->render_w = (float)width;
    element->render_h = (float)height;
}

/* ======================================================================================================================
 * 12. a ready-made COMPLETED IAsyncAction (IAsyncAction + IAsyncInfo), e.g. for StatusBar.HideAsync results that nobody waits for.
 *     put_Completed has the semantics of the real runtime for an action that is already finished: the handler IS invoked (AsyncActionCompletedHandler.Invoke
 *     (action, Completed)), asynchronously on the UI thread through x104_ui_post (never inline: rule R1), guarded.  PPL `create_task(IAsyncAction)` and C++/CX
 *     `.then` continuations attached to an action returned by a fake therefore run.  The async unit's own actions implement the same contract (units/async.md 2.2);
 *     the two implementations agree.  The first uses are logged so a surprise attach is visible.
 * ====================================================================================================================== */
static BOOL x104_ui_post(void (*function)(void *), void *argument);      /* async unit (also declared in section 14b) */
static X104Object *x104_completed_action(void);

static void x104_done_invoke_completed(void *argument)
{
    void *handler = argument;
    HRESULT hr = S_OK;
    X104Seh seh;
    void *action = x104_object_iface0(x104_completed_action());
    if (!x104_call_guarded3(((void **)*(void ***)handler)[3], handler, action, (void *)(ULONG_PTR)X104_ASYNC_COMPLETED, &hr, &seh)) {
        x104_logf("X104 EXCEPTION escaped from a Completed handler of the completed action (handler %p): code=%08lx at %p (game rva %08lx)", handler,
                  (unsigned long)seh.code, seh.address, (unsigned long)seh.rva);
    } else if (FAILED(hr)) {
        x104_logf("X104 completed action: Completed handler %p returned 0x%08lx %s", handler, (unsigned long)hr, x104_hr_name(hr));
    }
    x104_raw_release(handler);
}

static HRESULT X104_API x104_done_put_completed(void *self, void *handler)
{
    static volatile LONG logged;
    (void)self;
    if (!handler) return S_OK;
    X104_LOG_LIMITED(&logged, 20, "X104 NOTE: completed IAsyncAction.put_Completed(handler=%p): Completed will be invoked asynchronously on the UI thread (a late attach)", handler);
    x104_raw_addref(handler);
    if (!x104_ui_post(x104_done_invoke_completed, handler)) {
        x104_raw_release(handler);
        x104_logf("X104 WARNING: completed IAsyncAction.put_Completed(handler=%p): the UI queue is closed (shutdown) - handler NOT invoked", handler);
    }
    return S_OK;
}
static HRESULT X104_API x104_done_get_completed(void *self, void **handler) { (void)self; if (!handler) return E_POINTER; *handler = NULL; return S_OK; }
static HRESULT X104_API x104_done_get_results(void *self) { (void)self; return S_OK; }
static HRESULT X104_API x104_done_get_id(void *self, UINT32 *id) { (void)self; if (!id) return E_POINTER; *id = 1; return S_OK; }
static HRESULT X104_API x104_done_get_status(void *self, int *status) { (void)self; if (!status) return E_POINTER; *status = X104_ASYNC_COMPLETED; return S_OK; }
static HRESULT X104_API x104_done_get_error(void *self, HRESULT *hr) { (void)self; if (!hr) return E_POINTER; *hr = S_OK; return S_OK; }
static HRESULT X104_API x104_done_noop(void *self) { (void)self; return S_OK; }

X104_VTBL(x104_vt_done_action, IAsyncAction) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IAsyncAction, put_Completed, x104_done_put_completed),
    X104_SLOT(IAsyncAction, get_Completed, x104_done_get_completed),
    X104_SLOT(IAsyncAction, GetResults, x104_done_get_results)
};
X104_VTBL(x104_vt_done_info, IAsyncInfo) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IAsyncInfo, get_Id, x104_done_get_id),
    X104_SLOT(IAsyncInfo, get_Status, x104_done_get_status),
    X104_SLOT(IAsyncInfo, get_ErrorCode, x104_done_get_error),
    X104_SLOT(IAsyncInfo, Cancel, x104_done_noop),
    X104_SLOT(IAsyncInfo, Close, x104_done_noop)
};
static const X104IfaceDesc x104_done_ifaces[] = {
    X104_IFACE(IAsyncAction, x104_vt_done_action),
    X104_IFACE(IAsyncInfo, x104_vt_done_info)
};
X104_CLASS_DEF(x104_done_class, "CompletedAsyncAction", L"Windows.Foundation.IAsyncAction", x104_done_ifaces, X104_CLASS_AGILE, NULL);
static X104Object x104_done_object;
static volatile LONG x104_done_state;

/* the singleton completed action (identity interface = IAsyncAction); the pointer is NOT AddRef'd - QI it to hand it to the game */
static X104Object *x104_completed_action(void)
{
    if (x104_once_begin(&x104_done_state)) {
        x104_object_init(&x104_done_object, &x104_done_class);
        x104_once_end(&x104_done_state);
    }
    return &x104_done_object;
}

/* ======================================================================================================================
 * 13. on-screen keyboard (TabTip), text-input state and pointer-source wrappers around the device-proven 1.2.8 controller in win32_host.c
 *     ("Windows touch keyboard (TabTip) for text boxes", arm32/OSK_NOTES.md).  The controller is level-triggered and polled:
 *       x104_osk_show()/x104_osk_hide() only record the request; the real work and the call to arm32_input_pane_raise() (which the
 *       host edit forwards to x104_inputpane_raise() of the input unit) happen in x104_osk_poll() on the UI thread's tick.
 *     It is eligible when --osk=on, or --osk=auto AND the last pointer was a finger: call x104_note_pointer_source(TRUE/FALSE).
 *     The controller ALSO reads win32_host.c's global `text_input_active` (arm32_osk_poll clears arm32_osk_dismissed_by_user when it is FALSE, and the
 *     hardware-key source switch tests it).  In 0.10.4 nothing else sets it, so the INPUT UNIT must keep it in step with the focused element through
 *     x104_set_text_input_active() on every EditBox.Focus / Button.Focus (units/input.md 3.5) - otherwise the tap-to-reopen-after-user-dismissal behaviour that
 *     the Surface 2 test approved never works.
 * ====================================================================================================================== */
static void x104_osk_show(const char *why) { arm32_osk_show(why); }
static void x104_osk_hide(const char *why) { arm32_osk_hide(why); }
static void x104_osk_poll(void) { arm32_osk_poll(); }
static BOOL x104_osk_rect_client(float rect[4]) { return arm32_osk_rect_buffer(rect); }
static void x104_osk_note_pointer_up(void) { arm32_osk_note_pointer_up(); }
static void x104_note_pointer_source(BOOL touch, const char *why) { arm32_input_note_source(touch ? ARM32_SRC_TOUCH : ARM32_SRC_MOUSEKB, why); }
static BOOL x104_touch_is_active_source(void) { return arm32_input_touch_active(); }

/* the shared win32_host.c flag "a game text control has the focus" (read by the TabTip controller); the transition is logged */
static void x104_set_text_input_active(BOOL active, const char *why)
{
    BOOL previous = text_input_active;
    text_input_active = active;
    if (previous != active) x104_logf("X104 text_input_active %d -> %d (%s)", (int)previous, (int)active, why ? why : "?");
}

/* ======================================================================================================================
 * 13b. Present accounting and pacing.  The game's render thread calls IDXGISwapChain1::Present(0,0) itself; nothing in the host counted those frames (the 30 s heartbeat
 *      of win32_host.c printed `frames done=0` for the whole run), no Present HRESULT was ever logged and an HWND flip chain with SyncInterval 0 may run uncapped.
 *      The PANEL unit patches the swap chain's vtable slot 8 (Present) with a stub that brackets the real call:
 *          x104_present_begin();  sync = x104_present_sync(sync);  x104_present_pace();  hr = real(self, sync, flags);  x104_present_end(hr, sync, flags);
 *      Frames are counted in win32_host.c's arm32_boot_frames_started/_done (watchdog + heartbeat), the first 20 results and every non-S_OK result are logged.
 * ====================================================================================================================== */
static volatile LONG x104_present_bad;
static volatile LONG x104_present_occluded;
static DWORD x104_present_first_tick, x104_present_last_tick;
static LONGLONG x104_present_last_qpc;
static volatile LONG x104_present_fast_run;
static volatile LONG x104_present_throttled;

static void x104_present_begin(void)
{
    InterlockedIncrement(&arm32_boot_frames_started);
}

/* SyncInterval actually passed to the real Present (--vsync turns the game's 0 into 1) */
static UINT x104_present_sync(UINT requested)
{
    return (x104.opt_vsync && requested == 0) ? 1u : requested;
}

/* render thread, BEFORE the real Present: explicit cap (--max-fps=N) or the automatic guard against an uncapped spin (a 2-core Surface RT must keep the UI thread alive):
 * after 60 consecutive Present intervals under 3 ms (> 330 fps) the thread sleeps 1 ms per frame for the rest of the run (logged once). */
static void x104_present_pace(void)
{
    LARGE_INTEGER now, frequency;
    double scale;                               /* microseconds per tick (double math: no 64-bit division helper needed) */
    long interval_us;
    if (!QueryPerformanceCounter(&now) || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) return;
    scale = 1000000.0 / (double)frequency.QuadPart;
    interval_us = x104_present_last_qpc ? (long)((double)(now.QuadPart - x104_present_last_qpc) * scale) : 1000000L;
    if (x104.opt_max_fps > 0) {
        long period_us = 1000000L / x104.opt_max_fps;
        while (interval_us < period_us) {
            long remaining_us = period_us - interval_us;
            if (remaining_us >= 2000) Sleep((DWORD)(remaining_us / 1000 - 1));
            else SwitchToThread();
            QueryPerformanceCounter(&now);
            interval_us = (long)((double)(now.QuadPart - x104_present_last_qpc) * scale);
        }
    } else if (x104_present_throttled) {
        if (interval_us < 8000) Sleep(1);
    } else if (interval_us < 3000) {
        if (InterlockedIncrement(&x104_present_fast_run) >= 60) {
            x104_present_throttled = 1;
            x104_logf("X104 present: the game renders uncapped (Present interval %lu us for 60 frames in a row) - the render thread now sleeps 1 ms per frame "
                      "(use --vsync for SyncInterval 1 or --max-fps=N for an explicit cap)", (unsigned long)interval_us);
        }
    } else {
        x104_present_fast_run = 0;
    }
    QueryPerformanceCounter(&now);
    x104_present_last_qpc = now.QuadPart;
}

/* render thread, AFTER the real Present */
static void x104_present_end(HRESULT hr, UINT sync, UINT flags)
{
    LONG frame = InterlockedIncrement(&arm32_boot_frames_done);
    DWORD now = GetTickCount();
    DWORD frame_ms = x104_present_last_tick ? now - x104_present_last_tick : 0;
    if (frame == 1) {
        x104_present_first_tick = now;
        x104_logf("X104 present: FIRST Present returned 0x%08lx %s (sync=%u flags=%08lx) %lu ms after the process started", (unsigned long)hr, x104_hr_name(hr),
                  (unsigned)sync, (unsigned long)flags, (unsigned long)(now - arm32_boot_start_tick));
    } else if (frame <= 20) {
        x104_logf("X104 present: Present #%ld returned 0x%08lx %s (sync=%u, %lu ms since the previous one)", (long)frame, (unsigned long)hr, x104_hr_name(hr), (unsigned)sync,
                  (unsigned long)frame_ms);
    }
    if (hr != S_OK) {
        if (hr == (HRESULT)0x087A0001L) {          /* DXGI_STATUS_OCCLUDED: window minimised / covered; the app unit stops the render loop on minimise */
            if (InterlockedIncrement(&x104_present_occluded) <= 3 || (x104_present_occluded & 0x3FF) == 0)
                x104_logf("X104 present: DXGI_STATUS_OCCLUDED (count %ld)", (long)x104_present_occluded);
        } else if (InterlockedIncrement(&x104_present_bad) <= 200) {
            x104_logf("X104 present: Present #%ld returned 0x%08lx %s (sync=%u flags=%08lx)%s", (long)frame, (unsigned long)hr, x104_hr_name(hr), (unsigned)sync,
                      (unsigned long)flags, (hr == (HRESULT)0x887A0005L || hr == (HRESULT)0x887A0007L) ? "  *** DEVICE LOST/RESET: unrecoverable in this game ***" : "");
        }
    }
    arm32_boot_note_frame(frame, x104_present_first_tick, frame_ms);
    x104_present_last_tick = now;
}

/* ======================================================================================================================
 * 14b. CROSS-UNIT API.  Every function below is DEFINED by exactly one unit file (the stubs create them); any unit may CALL any of
 *      them as documented in arm32/v0_10_4/HOST_DESIGN.md section 8.  "UI" = must run on the UI thread, "any" = any thread.
 * ====================================================================================================================== */
/* ---- async unit (win32_host_arm32_0104_async.inl) ---- */
static BOOL x104_async_install(BYTE *image);                 /* boot, before the window exists */
static BOOL x104_async_init(HWND window);                    /* boot, UI thread, window exists: creates dispatcher + primitives */
static void x104_async_shutdown(void);                       /* app unit when Start is about to return: cancel actions, wake workers, wait <= 2 s */
static int  x104_ui_run_loop(void);                          /* UI: the message / dispatcher / timer loop of the fake Application::Start; returns the exit code */
static void x104_ui_quit(int exit_code);                     /* any: make x104_ui_run_loop return: x104.exit_code, f_quit (logged), f_loop_exit, wake event, PostMessage(X104_WM_QUIT) */
static BOOL x104_ui_post(void (*function)(void *), void *argument);   /* any: run function(argument) on the UI thread (FIFO); FALSE only after x104_ui_quit (f_quit) */
static void x104_ui_pump_once(void);                         /* UI: run queued work + due timers once (re-entrancy safe); called by x104_ui_pass() only */
/* x104_async_dispatcher_object() is declared in section 11 */
/* ---- app unit (win32_host_arm32_0104_app.inl) ---- */
static BOOL x104_app_install(BYTE *image);
static BOOL x104_app_init(HWND window);
static void x104_app_shutdown(void);
static void x104_app_ui_tick(void);                          /* UI: every loop pass; lifecycle bookkeeping */
static BOOL x104_app_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result);
static void x104_app_request_exit(int exit_code);            /* any: graceful shutdown (idempotent on f_exit_requested): Suspending only if the game is ready, x104_async_shutdown, x104_ui_quit */
static BOOL x104_app_raise_back_pressed(void);               /* UI: BackPressed to the game's handler; TRUE = the game set Handled */
static BOOL x104_app_rendering_active(void);                  /* any: a CompositionTarget.Rendering handler is registered and the window shows (REDCON 1) */
static unsigned x104_input_raise_core_size_changed(void);    /* UI: CoreWindow.SizeChanged to the game's handlers (REDCON 1 resizes through it); returns the count */
static void x104_display_raise_dpi_changed(void);            /* UI */
static void x104_display_raise_orientation_changed(void);    /* UI */
static void x104_display_raise_contents_invalidated(void);   /* UI */
/* ---- panel unit (win32_host_arm32_0104_panel.inl) ---- */
static BOOL x104_panel_install(BYTE *image);                 /* boot: D3D11CreateDevice hook, profile patch, dxgi cells, float option */
static BOOL x104_panel_init(HWND window);
static void x104_panel_shutdown(void);
static void x104_panel_ui_tick(void);                        /* UI: every loop pass; fires the FIRST SizeChanged when the ordering rules allow */
static BOOL x104_panel_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result);
static X104Object *x104_panel_object(void);                  /* the SwapChainPanel (not AddRef'd) */
static void x104_panel_raise_size_changed(void);             /* UI: SizeChanged with the current client size; honours the ordering rules (no-op while blocked) */
static void x104_panel_client_size(float *width_dips, float *height_dips);   /* any: current client size, == DIPs (scale 1.0) */
/* ---- platform unit (win32_host_arm32_0104_platform.inl) ---- */
static BOOL x104_platform_install(BYTE *image);              /* boot, FIRST: paths (ASCII check), data root, data-cell patches, XAudio2 probe */
static void x104_platform_shutdown(void);
/* ---- input unit (win32_host_arm32_0104_input.inl) ---- */
static BOOL x104_input_install(BYTE *image);
static BOOL x104_input_init(HWND window);
static void x104_input_shutdown(void);
static void x104_input_ui_tick(void);                        /* UI: every loop pass; x104_osk_poll() etc. */
static BOOL x104_input_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LRESULT *result);
static X104Object *x104_input_core_window(void);             /* the CoreWindow (not AddRef'd) */
static X104Object *x104_input_element_logo(void);            /* FindName("Logo") */
static X104Object *x104_input_element_editbox(void);         /* FindName("EditBox") */
static X104Object *x104_input_element_button(void);          /* FindName("Button") */
static void x104_input_raise_visibility_changed(BOOL visible);   /* UI: CoreWindow.VisibilityChanged (the app unit calls it from its lifecycle code) */
static void x104_input_release_all_pointers(const char *why);    /* UI: send Released for every pressed pointer (focus loss, minimise) */
static void x104_inputpane_raise(BOOL showing);              /* UI: Showing/Hiding to the handlers MainPage registered (called by arm32_input_pane_raise) */
static HRESULT x104_input_create_independent_source(UINT device_types, void **result);   /* any thread (the input work item): ISwapChainPanel.CreateCoreIndependentInputSource slot 10 */

/* ======================================================================================================================
 * 15. the UI-thread pass shared by the loop and the modal-loop pump
 * ====================================================================================================================== */
static volatile LONG x104_in_pass;

/* ONE pass of the UI thread's work: queued items + due timers (async), then the three unit ticks (panel: first/coalesced SizeChanged, device-loss poll; input: TabTip
 * poll, deferred pointer release; app: visibility), then a watchdog progress bump.  Called by x104_ui_run_loop after it drained the message queue, and by the window
 * proc on the WM_TIMER pump (X104_TIMER_PUMP, 10 ms) so that everything keeps running inside the modal loops Windows runs for window move/size and the system menu.
 * UI thread only, never re-entered (a nested call returns at once), a no-op until the boot finished initialising the units (x104.f_units_ready). */
static void x104_ui_pass(void)
{
    if (!x104.f_units_ready) return;
    if (__sync_val_compare_and_swap(&x104_in_pass, 0, 1) != 0) return;
    x104_ui_pump_once();
    x104_panel_ui_tick();
    x104_input_ui_tick();
    x104_app_ui_tick();
    InterlockedIncrement(&arm32_boot_progress);
    InterlockedIncrement(&arm32_boot_loop_passes);
    InterlockedExchange(&x104_in_pass, 0);
}


#endif /* WIN32_HOST_ARM32_0104_CORE_INL */
