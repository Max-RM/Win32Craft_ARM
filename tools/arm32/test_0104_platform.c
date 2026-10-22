/* Host-side (x86, runs on the PC) test of win32_host_arm32_0104_platform.inl: roots (ASCII check, data-root selection and fallbacks), data-cell patches on a fake game
 * image, MemoryManager decision, every fake object through the generated slot typedefs, activation, plus the real on-device selftest block of the platform unit
 * (x104_selftest_fakes: the other units are the pristine stubs and SKIP).
 * Build + run:  python tools/arm32/test_0104_platform.py     (the .py runs this exe several times with different command lines)
 * Environment variables set by the .py: X104T_GAME (ASCII game root), X104T_APPDATA (what the SHGetFolderPathW stand-in returns), X104T_MODE (see main). */
#include "win32_compat.h"

extern void *WINAPI GetStdHandle(DWORD);
extern char *strstr(const char *, const char *);
extern int strcmp(const char *, const char *);
extern int WINAPI wvsprintfA(LPSTR, LPCSTR, __builtin_va_list);
extern void WINAPI RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR *);
extern DWORD WINAPI GetEnvironmentVariableW(LPCWSTR, LPWSTR, DWORD);
extern BOOL WINAPI SetEnvironmentVariableW(LPCWSTR, LPCWSTR);
extern BOOL WINAPI RemoveDirectoryW(LPCWSTR);

static int g_failures;
static int g_checks;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; test_print("CHECK FAILED line %d: %s\n", __LINE__, #cond); } } while (0)

static void test_print(const char *format, ...)
{
    char line[1100];
    DWORD written;
    __builtin_va_list arguments;
    __builtin_va_start(arguments, format);
    wvsprintfA(line, format, arguments);
    __builtin_va_end(arguments);
    WriteFile(GetStdHandle((DWORD)-11), line, (DWORD)strlen(line), &written, NULL);
}

/* ---- stand-ins for win32_host.c ---- */
static char g_log[64][300];
static int g_log_lines;
static int g_log_total;
static void log_line(const char *text)
{
    lstrcpynA(g_log[g_log_total % 64], text, 300);
    ++g_log_total;
    ++g_log_lines;
    test_print("  [log] %s\n", text);
}
static BOOL g_saw_fatal;
static int log_seen(const char *needle)      /* any of the logged lines contains needle (only the last 64 lines are kept) */
{
    int index;
    for (index = 0; index < 64 && index < g_log_total; ++index) {
        if (strstr(g_log[index], needle)) return 1;
    }
    return 0;
}
static void log_hresult(const char *operation, HRESULT result) { test_print("  [hr] %s 0x%08lx\n", operation, (unsigned long)result); }
extern HRESULT WINAPI WindowsDeleteString(HSTRING);
extern BOOL WINAPI WindowsIsStringEmpty(HSTRING);
#define ARM32_SRC_MOUSEKB 1
#define ARM32_SRC_TOUCH 2
static BOOL arm32_check_bytes(const BYTE *address, const BYTE *expected, SIZE_T size, const char *what)
{
    if (memcmp(address, expected, size) == 0) return TRUE;
    test_print("  [check] %s differs\n", what);
    return FALSE;
}
static BOOL arm32_patch_cell(void **cell, const void *expected, void *replacement, void **original_out, const char *what)
{
    if (expected && *cell != expected) {
        test_print("  [patch] %s: cell holds %p expected %p - not patched\n", what, *cell, expected);
        return FALSE;
    }
    if (original_out) *original_out = *cell;
    *cell = replacement;
    test_print("  [patch] %s: %p -> %p\n", what, *cell, replacement);
    return TRUE;
}
static void arm32_osk_show(const char *why) { (void)why; }
static void arm32_osk_hide(const char *why) { (void)why; }
static void arm32_osk_poll(void) { }
static BOOL arm32_osk_rect_buffer(float rect[4]) { (void)rect; return FALSE; }
static void arm32_osk_note_pointer_up(void) { }
static void arm32_input_note_source(int source, const char *why) { (void)source; (void)why; }
static BOOL arm32_input_touch_active(void) { return FALSE; }
static BOOL text_input_active;
static volatile LONG arm32_boot_frames_started, arm32_boot_frames_done, arm32_boot_progress, arm32_boot_loop_passes;
static DWORD arm32_boot_start_tick;
static void arm32_boot_note_frame(LONG frame, DWORD loop_start, DWORD frame_ms) { (void)frame; (void)loop_start; (void)frame_ms; }

/* the command line helpers of win32_host.c (identical logic) */
static wchar_t host_ascii_lower_w(wchar_t value) { return (value >= L'A' && value <= L'Z') ? (wchar_t)(value + (L'a' - L'A')) : value; }
static BOOL host_argument_equals(const wchar_t *left, const wchar_t *right)
{
    while (*left && *right) {
        if (host_ascii_lower_w(*left) != host_ascii_lower_w(*right)) return FALSE;
        ++left;
        ++right;
    }
    return *left == 0 && *right == 0;
}
static const wchar_t *host_next_argument(const wchar_t *cursor, wchar_t *argument, UINT capacity)
{
    UINT length = 0;
    BOOL quoted = FALSE;
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    while (*cursor) {
        if (*cursor == L'"') { quoted = !quoted; ++cursor; continue; }
        if (!quoted && (*cursor == L' ' || *cursor == L'\t')) break;
        if (length + 1 < capacity) argument[length++] = *cursor;
        ++cursor;
    }
    argument[length] = 0;
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    return cursor;
}
static const wchar_t *arm32_iat_argument_value(const wchar_t *argument, const wchar_t *prefix)
{
    while (*prefix) {
        if (host_ascii_lower_w(*argument) != host_ascii_lower_w(*prefix)) return NULL;
        ++argument;
        ++prefix;
    }
    return argument;
}

/* the SHGetFolderPathW stand-in (declared by win32_compat.h; no shell32 needed): returns %X104T_APPDATA% or fails */
HRESULT WINAPI SHGetFolderPathW(HWND window, int folder, HANDLE token, DWORD flags, LPWSTR path)
{
    DWORD length;
    (void)window; (void)folder; (void)token; (void)flags;
    length = GetEnvironmentVariableW(L"X104T_APPDATA", path, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        path[0] = 0;
        return E_FAIL;
    }
    return S_OK;
}

#include "win32_host_arm32_0104_core.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_async.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_app.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_panel.inl"
#include "win32_host_arm32_0104_platform.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_input.inl"
#include "win32_host_arm32_0104_selftest.inl"

#define FNV(object, iface, method) X104_VFN((object), X104_SLOT_##iface##_##method, X104_FN_##iface##_##method)

static BOOL wide_equals(const wchar_t *a, const wchar_t *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static BOOL dir_exists_test(const wchar_t *path)
{
    return x104_platform_dir_exists(path);
}

/* fake game image: the cells and functions of the platform brief at their RVAs */
static BYTE *make_image(void)
{
    BYTE *image = (BYTE *)VirtualAlloc(NULL, 0x00E6F000u, 0x3000 /* MEM_COMMIT|MEM_RESERVE */, 4 /* PAGE_READWRITE */);
    static const BYTE prologue_buy[12] = { 0x2d,0xe9,0xf0,0x4b,0x0d,0xf1,0x18,0x0b,0xb3,0xf7,0x7e,0xf8 };
    static const BYTE prologue_net[12] = { 0x2d,0xe9,0xf0,0x4f,0x0d,0xf1,0x1c,0x0b,0xb2,0xf7,0x9c,0xfa };
    static const BYTE prologue_vib[12] = { 0x2d,0xe9,0xf0,0x49,0x0d,0xf1,0x14,0x0b,0x85,0xb0,0x6f,0x46 };
    if (!image) return NULL;
    memcpy(image + 0xBF60, x104_platform_noop_bytes, sizeof(x104_platform_noop_bytes));
    memcpy(image + 0x29BA0, x104_platform_ret0_bytes, sizeof(x104_platform_ret0_bytes));
    memcpy(image + 0x4E350, prologue_buy, 12);
    memcpy(image + 0x4EF14, prologue_net, 12);
    memcpy(image + 0x50FF0, prologue_vib, 12);
    *(void **)(image + X104_RVA_CELL_BUY) = ARM0104_CODE(image, 0x4E350);
    *(void **)(image + X104_RVA_CELL_NETWORK) = ARM0104_CODE(image, 0x4EF14);
    *(void **)(image + X104_RVA_CELL_VIBRATE) = ARM0104_CODE(image, 0x50FF0);
    *(void **)(image + X104_RVA_CELL_FREEMEMORY) = ARM0104_CODE(image, 0x4EAF8);
    *(void **)(image + X104_RVA_CELL_SOUND_PLAY) = ARM0104_CODE(image, 0x106688);
    *(void **)(image + X104_RVA_CELL_SOUND_VF10) = ARM0104_CODE(image, 0x106630);
    return image;
}

static void test_objects(const wchar_t *expected_game, const wchar_t *expected_data, int offline)
{
    void *statics = NULL, *object = NULL, *folder = NULL, *item = NULL, *settings = NULL, *values = (void *)1, *profile = NULL, *iface = NULL, *license = NULL;
    HSTRING path = NULL, name = NULL;
    wchar_t wide[MAX_PATH];
    ULONGLONG usage = 0, limit = 0;
    int level = 0;
    BYTE flag = 0;
    LONGLONG token = 0;
    HRESULT hr;
    static const GUID bogus = { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } };

    /* Package */
    hr = x104_platform_activate(L"Windows.ApplicationModel.Package", &X104_IID_IPackageStatics, &statics);
    CHECK(hr == S_OK && statics);
    hr = FNV(statics, IPackageStatics, get_Current)(statics, &object);
    CHECK(hr == S_OK && object);
    CHECK(x104_raw_qi(object, &X104_IID_IPackage, &iface) == S_OK && iface == object);
    hr = FNV(object, IPackage, get_InstalledLocation)(object, &folder);
    CHECK(hr == S_OK && folder);
    CHECK(x104_raw_qi(folder, &X104_IID_IStorageItem, &item) == S_OK && item && item != folder);
    CHECK(x104_raw_qi(folder, &X104_IID_IAgileObject, &iface) == S_OK);
    hr = FNV(item, IStorageItem, get_Path)(item, &path);
    CHECK(hr == S_OK && path);
    x104_hstring_wide(path, wide, MAX_PATH);
    CHECK(wide_equals(wide, expected_game));
    x104_hstring_free(path);
    hr = FNV(item, IStorageItem, get_Name)(item, &name);
    CHECK(hr == S_OK);
    x104_hstring_wide(name, wide, MAX_PATH);
    test_print("  Package folder name = '%s'\n", (x104_narrow((char *)g_log[63], 100, wide), (char *)g_log[63]));
    CHECK(wide[0] != 0);
    x104_hstring_free(name);
    /* a bogus IID must be a QI miss, not a crash */
    CHECK(x104_raw_qi(statics, &bogus, &iface) == E_NOINTERFACE && iface == NULL);

    /* ApplicationData */
    hr = x104_platform_activate(L"Windows.Storage.ApplicationData", &X104_IID_IApplicationDataStatics, &statics);
    CHECK(hr == S_OK && statics);
    hr = FNV(statics, IApplicationDataStatics, get_Current)(statics, &object);
    CHECK(hr == S_OK && object);
    hr = FNV(object, IApplicationData, get_LocalFolder)(object, &folder);
    CHECK(hr == S_OK && folder);
    CHECK(x104_raw_qi(folder, &X104_IID_IStorageItem, &item) == S_OK);
    hr = FNV(item, IStorageItem, get_Path)(item, &path);
    x104_hstring_wide(path, wide, MAX_PATH);
    CHECK(hr == S_OK && wide_equals(wide, expected_data));
    x104_hstring_free(path);
    hr = FNV(object, IApplicationData, get_LocalSettings)(object, &settings);
    CHECK(hr == S_OK && settings);
    hr = FNV(settings, IApplicationDataContainer, get_Values)(settings, &values);
    CHECK(hr == S_OK && values == NULL);
    hr = FNV(settings, IApplicationDataContainer, get_Name)(settings, &name);
    CHECK(hr == S_OK && name == NULL);
    /* the folders are two distinct objects, both stable */
    {
        void *package_folder = NULL, *package = NULL, *local_folder = NULL;
        hr = x104_platform_activate(L"Windows.ApplicationModel.Package", &X104_IID_IPackageStatics, &statics);
        FNV(statics, IPackageStatics, get_Current)(statics, &package);
        FNV(package, IPackage, get_InstalledLocation)(package, &package_folder);
        FNV(object, IApplicationData, get_LocalFolder)(object, &local_folder);
        CHECK(package_folder != local_folder);
    }

    /* MemoryManager */
    hr = x104_platform_activate(L"Windows.System.MemoryManager", &X104_IID_IMemoryManagerStatics, &statics);
    CHECK(hr == S_OK && statics);
    hr = FNV(statics, IMemoryManagerStatics, get_AppMemoryUsage)(statics, &usage);
    CHECK(hr == S_OK && usage == 0x10000000ULL);
    hr = FNV(statics, IMemoryManagerStatics, get_AppMemoryUsageLimit)(statics, &limit);
    CHECK(hr == S_OK && (limit == 0x40000000ULL || limit == 0x20000000ULL));
    CHECK(limit - usage > 0xE600000ULL);
    test_print("  memory usage=%08lx%08lx limit=%08lx%08lx\n", (unsigned long)(usage >> 32), (unsigned long)(DWORD)usage, (unsigned long)(limit >> 32), (unsigned long)(DWORD)limit);

    /* VibrationDevice: Vibrate / Cancel are called WITHOUT QI on the pointer GetDefault returned */
    hr = x104_platform_activate(L"Windows.Phone.Devices.Notification.VibrationDevice", &X104_IID_IVibrationDeviceStatics, &statics);
    CHECK(hr == S_OK && statics);
    hr = FNV(statics, IVibrationDeviceStatics, GetDefault)(statics, &object);
    CHECK(hr == S_OK && object);
    CHECK(FNV(object, IVibrationDevice, Cancel)(object) == S_OK);
    CHECK(FNV(object, IVibrationDevice, Vibrate)(object, 2000000LL) == S_OK);
    CHECK(FNV(object, IVibrationDevice, Vibrate)(object, -1LL) == S_OK);

    /* NetworkInformation */
    hr = x104_platform_activate(L"Windows.Networking.Connectivity.NetworkInformation", &X104_IID_INetworkInformationStatics, &statics);
    CHECK(hr == S_OK && statics);
    profile = (void *)1;
    hr = FNV(statics, INetworkInformationStatics, GetInternetConnectionProfile)(statics, &profile);
    CHECK(hr == S_OK);
    if (offline) {
        CHECK(profile == NULL);
    } else {
        CHECK(profile != NULL);
        if (profile) {
            CHECK(FNV(profile, IConnectionProfile, GetNetworkConnectivityLevel)(profile, &level) == S_OK && level == 3);
            CHECK(x104_raw_qi(profile, &X104_IID_IConnectionProfile2, &iface) == S_OK && iface);
            flag = 0;
            CHECK(FNV(iface, IConnectionProfile2, get_IsWlanConnectionProfile)(iface, &flag) == S_OK && flag == 1);
            CHECK(FNV(iface, IConnectionProfile2, get_IsWwanConnectionProfile)(iface, &flag) == S_OK && flag == 0);
        }
    }

    /* Store */
    hr = x104_platform_activate(L"Windows.ApplicationModel.Store.CurrentAppSimulator", &X104_IID_ICurrentAppSimulator, &statics);
    CHECK(hr == S_OK && statics);
    CHECK(x104_raw_qi(statics, &X104_IID_ICurrentAppSimulatorWithConsumables, &iface) == S_OK);
    hr = FNV(statics, ICurrentAppSimulator, get_LicenseInformation)(statics, &license);
    CHECK(hr == S_OK && license);
    flag = 0;
    CHECK(FNV(license, ILicenseInformation, get_IsActive)(license, &flag) == S_OK && flag == 1);
    CHECK(FNV(license, ILicenseInformation, get_IsTrial)(license, &flag) == S_OK && flag == 0);
    values = (void *)1;
    CHECK(FNV(license, ILicenseInformation, get_ProductLicenses)(license, &values) == S_OK && values == NULL);
    CHECK(FNV(statics, ICurrentAppSimulator, RequestAppPurchaseAsync)(statics, 0, &object) == X104_E_NOT_IMPL);
    hr = x104_platform_activate(L"Windows.ApplicationModel.Store.CurrentApp", &X104_IID_ICurrentApp, &statics);
    CHECK(hr == S_OK && statics);

    /* AsyncCausalityTracer: slots 6..10 take arguments the host ignores (cdecl-like caller cleanup is NOT what x86 stdcall does, so only the 11/12 slots are called here) */
    hr = x104_platform_activate(L"Windows.Foundation.Diagnostics.AsyncCausalityTracer", &X104_IID_IAsyncCausalityTracerStatics, &statics);
    CHECK(hr == S_OK && statics);
    {
        static const void *vt_handler[4];
        static const void *const *dummy_handler = vt_handler;
        CHECK(FNV(statics, IAsyncCausalityTracerStatics, add_TracingStatusChanged)(statics, NULL, &token) == S_OK && token == 0);
        (void)dummy_handler;
        CHECK(FNV(statics, IAsyncCausalityTracerStatics, remove_TracingStatusChanged)(statics, 0) == S_OK);
    }

    /* Http stays with the real runtime */
    hr = x104_platform_activate(L"Windows.Web.Http.HttpClient", &X104_IID_IUnknown, &statics);
    CHECK(hr == REGDB_E_CLASSNOTREG && statics == NULL);
    hr = x104_platform_activate(L"Windows.Foo.Bar", &X104_IID_IUnknown, &statics);
    CHECK(hr == REGDB_E_CLASSNOTREG);
}

int main(void)
{
    wchar_t game[MAX_PATH];
    wchar_t appdata[MAX_PATH];
    wchar_t mode[32];
    BYTE *image;
    BOOL ok;
    int offline;
    int bypass;
    int failures;

    test_print("== platform unit host test ==\n");
    /* what the boot does before x104_platform_install: the MTA apartment XAudio2 needs (CO_E_NOTINITIALIZED otherwise) */
    test_print("  RoInitialize(MTA) = 0x%08lx\n", (unsigned long)RoInitialize(1));
    GetEnvironmentVariableW(L"X104T_GAME", game, MAX_PATH);
    GetEnvironmentVariableW(L"X104T_APPDATA", appdata, MAX_PATH);
    mode[0] = 0;
    GetEnvironmentVariableW(L"X104T_MODE", mode, 32);
    CHECK(game[0] != 0);

    image = make_image();
    CHECK(image != NULL);
    memset(&x104, 0, sizeof(x104));
    x104.image = image;
    lstrcpyW(x104.game_root, game);

    if (mode[0] == L'N') {
        /* mode N: non-ASCII game folder: either the 8.3 short path is used (and x104.game_root is rewritten) or install returns FALSE with the FATAL line */
        wchar_t unicode[MAX_PATH];
        lstrcpyW(unicode, game);
        lstrcatW(unicode, L"\\caf\x00e9");
        x104_platform_make_dirs(unicode, NULL);
        lstrcpyW(x104.game_root, unicode);
        ok = x104_platform_install(image);
        test_print("  install with a non-ASCII game folder returned %d, game_root ascii=%d\n", (int)ok, (int)x104_platform_is_ascii(x104.game_root));
        CHECK(ok ? x104_platform_is_ascii(x104.game_root) : log_seen("FATAL the game folder path contains non-ASCII characters"));
        RemoveDirectoryW(unicode);
        goto done;
    }

    ok = x104_platform_install(image);
    CHECK(ok);
    test_print("  install -> %d; data_root source '%s'\n", (int)ok, x104_platform_state.data_root_source);
    offline = x104_platform_state.offline;
    bypass = x104_platform_state.bypass_network;
    if (ok) {
        char shown[MAX_PATH];
        x104_narrow(shown, sizeof(shown), x104.data_root);
        test_print("  data root = %s\n", shown);
        CHECK(x104.data_root[0] != 0 && x104_platform_is_ascii(x104.data_root) && dir_exists_test(x104.data_root));
        CHECK(log_seen("writable=1"));
        /* buy cell patched, the others per option */
        CHECK(*(void **)(image + X104_RVA_CELL_BUY) == X104_CODE(0x29BA0));
        CHECK(*(void **)(image + X104_RVA_CELL_NETWORK) == (bypass ? X104_CODE(0x29BA0) : X104_CODE(0x4EF14)));
        CHECK(*(void **)(image + X104_RVA_CELL_VIBRATE) == (x104_platform_state.bypass_vibrate ? X104_CODE(X104_RVA_NOOP_FUNCTION) : X104_CODE(0x50FF0)));
        CHECK(*(void **)(image + X104_RVA_CELL_FREEMEMORY) == X104_CODE(0x4EAF8));
        /* the XAudio2 probe either passed (cells untouched) or failed (both cells -> no-op) */
        if (x104_platform_state.xaudio2_ok) {
            CHECK(*(void **)(image + X104_RVA_CELL_SOUND_PLAY) == X104_CODE(0x106688));
            CHECK(*(void **)(image + X104_RVA_CELL_SOUND_VF10) == X104_CODE(0x106630));
        } else {
            CHECK(*(void **)(image + X104_RVA_CELL_SOUND_PLAY) == X104_CODE(X104_RVA_NOOP_FUNCTION));
            CHECK(*(void **)(image + X104_RVA_CELL_SOUND_VF10) == X104_CODE(X104_RVA_NOOP_FUNCTION));
        }
        test_objects(x104.game_root, x104.data_root, offline);
        /* the real on-device self test block (other units SKIP) */
        failures = x104_selftest_fakes();
        CHECK(failures == 0);
        x104_platform_shutdown();
        x104_platform_shutdown();
    }
    if (mode[0] == L'F') {
        /* mode F (fallback): X104T_APPDATA points at an impossible drive -> the data root must be <game>\LocalState */
        wchar_t expect[MAX_PATH];
        x104_platform_join(expect, MAX_PATH, game, L"LocalState");
        CHECK(wide_equals(x104.data_root, expect));
    }
    if (mode[0] == L'D') {
        wchar_t expect[MAX_PATH];
        expect[0] = 0;
        GetEnvironmentVariableW(L"X104T_EXPECT", expect, MAX_PATH);
        CHECK(expect[0] != 0 && wide_equals(x104.data_root, expect));
        CHECK(x104_platform_state.data_dir_given);
    }
    if (mode[0] == L'A') {
        wchar_t expect[MAX_PATH];
        x104_platform_join(expect, MAX_PATH, appdata, L"MinecraftPE\\0.10.4");
        CHECK(wide_equals(x104.data_root, expect));
    }
done:
    test_print("== %d checks, %d FAILED ==\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
