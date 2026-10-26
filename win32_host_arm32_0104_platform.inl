/* win32_host_arm32_0104_platform.inl - UNIT "platform" of the Minecraft PE 0.10.4.40 ARM32 host.
 *
 * Owner: the platform implementer.  Brief: arm32/v0_10_4/units/platform.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit platform
 *
 * This file owns the "phone services" the game's AppPlatform_Winrt uses and the file-system roots:
 *   Package (InstalledLocation.Path = game folder, ASCII only), ApplicationData (LocalFolder.Path = data root, LocalSettings container),
 *   StorageFolder (IStorageFolder + IStorageItem), MemoryManager, VibrationDevice, NetworkInformation (+ a minimal connection profile),
 *   CurrentAppSimulator / CurrentApp (+ LicenseInformation), AsyncCausalityTracer, the Windows.Web.Http decision (left to the real runtime),
 *   the data folder selection, the AppPlatform vtable data-cell patches (buy 0x33FB7C; optional --bypass-network 0x33FB6C / --bypass-vibrate 0x33FB94)
 *   and the XAudio2 probe with its SoundSystemRT fallback cells (0xDB804C / 0xDB8050).
 *
 * Everything the game needs from this unit happens BEFORE the main menu: AppMain::createApp (0x1D9DD4) reads Package + ApplicationData in the MainPage
 * constructor, the init job needs the folders, MemoryManager and the assets.  File / network / audio traffic afterwards is native Win32/CRT/Winsock/XAudio2.
 *
 * Threading: install/shutdown run on the UI thread; every fake below is a leaf (no host lock is held, no game delegate is invoked: R1) and may be
 * called from any thread (the init job runs on a pool thread).  All state is zero-initialised-safe (R15).  HRESULTs are S_OK everywhere the brief lists
 * a call (R6); returned HSTRINGs are real combase HSTRINGs owned by the game (R8). */
#ifndef WIN32_HOST_ARM32_0104_PLATFORM_INL
#define WIN32_HOST_ARM32_0104_PLATFORM_INL

/* ======================================================================================================================
 * 1. state, small helpers
 * ====================================================================================================================== */
typedef struct X104PlatformState {
    BOOL installed;                 /* x104_platform_install ran to the end */
    BOOL shut_down;                 /* x104_platform_shutdown ran */
    BOOL offline;                   /* --offline: NetworkInformation.GetInternetConnectionProfile -> S_OK + NULL */
    BOOL bypass_network;            /* --bypass-network: AppPlatform vf19 (isNetworkEnabled) cell -> "return 0" */
    BOOL bypass_vibrate;            /* --bypass-vibrate: AppPlatform vf29 (vibrate) cell -> shared no-op */
    BOOL data_dir_given;            /* --data-dir=<path> */
    BOOL xaudio2_ok;                /* the XAudio2 probe succeeded */
    BOOL phys_known;                /* GlobalMemoryStatusEx worked */
    DWORD phys_mib;                 /* physical RAM in MiB (when phys_known) */
    ULONGLONG mem_usage;            /* MemoryManager.AppMemoryUsage the fake reports */
    ULONGLONG mem_limit;            /* MemoryManager.AppMemoryUsageLimit the fake reports */
    wchar_t data_dir[MAX_PATH];     /* the --data-dir value */
    char data_root_source[96];      /* where the data root came from (for the log) */
} X104PlatformState;

static X104PlatformState x104_platform_state;
static volatile LONG x104_platform_memory_state;          /* x104_once state of x104_platform_memory_decide */
static volatile LONG x104_platform_tracer_calls;
static volatile LONG x104_platform_vibrate_calls;
static volatile LONG x104_platform_cancel_calls;
static volatile LONG x104_platform_profile_calls;
static volatile LONG x104_platform_memory_calls;

/* only 7-bit characters survive the game's UTF-16 -> std::string narrowing (sub_480D8 truncates every unit to its low byte) */
static BOOL x104_platform_is_ascii(const wchar_t *text)
{
    if (!text) return TRUE;
    for (; *text; ++text) {
        if (*text >= 0x80) return FALSE;
    }
    return TRUE;
}

static BOOL x104_platform_starts_with(const wchar_t *text, const wchar_t *prefix)
{
    while (*prefix) {
        if (*text != *prefix) return FALSE;
        ++text;
        ++prefix;
    }
    return TRUE;
}

static void *x104_platform_kernel32_proc(const char *name)
{
    HMODULE module = GetModuleHandleW(L"kernel32.dll");
    return module ? GetProcAddress(module, name) : NULL;
}

/* GetShortPathNameW (resolved dynamically: win32_compat.h does not declare it); TRUE = *out holds the 8.3 form */
static BOOL x104_platform_short_path(const wchar_t *path, wchar_t *out, DWORD capacity)
{
    typedef DWORD (WINAPI *ShortPathFn)(LPCWSTR, LPWSTR, DWORD);
    ShortPathFn function = (ShortPathFn)x104_platform_kernel32_proc("GetShortPathNameW");
    DWORD length;
    out[0] = 0;
    if (!function) return FALSE;
    length = function(path, out, capacity);
    if (!length || length >= capacity) {
        out[0] = 0;
        return FALSE;
    }
    return TRUE;
}

/* number of characters of the root prefix: "C:\" = 3, "C:" = 2, "\\server\share\" = whole prefix, relative path = 0 */
static unsigned x104_platform_root_length(const wchar_t *path)
{
    unsigned index;
    unsigned separators;
    if (path[0] && path[1] == L':') return (path[2] == L'\\' || path[2] == L'/') ? 3u : 2u;
    if ((path[0] == L'\\' || path[0] == L'/') && (path[1] == L'\\' || path[1] == L'/')) {
        separators = 0;
        for (index = 2; path[index]; ++index) {
            if (path[index] == L'\\' || path[index] == L'/') {
                if (++separators == 2) return index + 1;
            }
        }
        return index;
    }
    return 0;
}

static BOOL x104_platform_is_absolute(const wchar_t *path)
{
    if (path[0] && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) return TRUE;
    return (path[0] == L'\\' || path[0] == L'/') && (path[1] == L'\\' || path[1] == L'/');
}

/* '/' -> '\', drop trailing separators (but never the one of a root such as "C:\") */
static void x104_platform_normalize(wchar_t *path)
{
    unsigned index;
    unsigned length;
    unsigned root = x104_platform_root_length(path);
    for (index = 0; path[index]; ++index) {
        if (path[index] == L'/') path[index] = L'\\';
    }
    length = (unsigned)lstrlenW(path);
    while (length > root && length > 1 && path[length - 1] == L'\\') path[--length] = 0;
}

/* dest = base + "\" + tail (no doubled separator); FALSE when it does not fit */
static BOOL x104_platform_join(wchar_t *dest, unsigned capacity, const wchar_t *base, const wchar_t *tail)
{
    unsigned length = (unsigned)lstrlenW(base);
    unsigned tail_length = (unsigned)lstrlenW(tail);
    if (length + tail_length + 2 > capacity) {
        dest[0] = 0;
        return FALSE;
    }
    lstrcpyW(dest, base);
    if (length && dest[length - 1] != L'\\' && dest[length - 1] != L'/') lstrcatW(dest, L"\\");
    lstrcatW(dest, tail);
    return TRUE;
}

static BOOL x104_platform_dir_exists(const wchar_t *path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path, 0 /* GetFileExInfoStandard */, &data)) return FALSE;
    return (data.dwFileAttributes & 0x10u /* FILE_ATTRIBUTE_DIRECTORY */) != 0;
}

/* CreateDirectoryW level by level (the parent chain may not exist: %APPDATA%\MinecraftPE\0.10.4) */
static BOOL x104_platform_make_dirs(const wchar_t *path, DWORD *error_out)
{
    wchar_t buffer[MAX_PATH];
    unsigned length = (unsigned)lstrlenW(path);
    unsigned root;
    unsigned index;
    if (error_out) *error_out = 0;
    if (!length || length >= MAX_PATH) {
        if (error_out) *error_out = 206;           /* ERROR_FILENAME_EXCED_RANGE */
        return FALSE;
    }
    lstrcpyW(buffer, path);
    root = x104_platform_root_length(buffer);
    for (index = root;; ++index) {
        wchar_t saved = buffer[index];
        if (saved != L'\\' && saved != 0) continue;
        if (index > root && buffer[index - 1] != L'\\') {
            buffer[index] = 0;
            if (!x104_platform_dir_exists(buffer) && !CreateDirectoryW(buffer, NULL) && !x104_platform_dir_exists(buffer)) {
                if (error_out) *error_out = GetLastError();
                return FALSE;
            }
            buffer[index] = saved;
        }
        if (!saved) break;
    }
    return TRUE;
}

/* create + write + delete a temporary file below `dir` */
static BOOL x104_platform_write_test(const wchar_t *dir, DWORD *error_out)
{
    static const char payload[] = "x104 platform write test";
    wchar_t file[MAX_PATH];
    HANDLE handle;
    DWORD written = 0;
    BOOL ok;
    if (error_out) *error_out = 0;
    if (!x104_platform_join(file, MAX_PATH, dir, L"x104_write_test.tmp")) {
        if (error_out) *error_out = 206;
        return FALSE;
    }
    handle = CreateFileW(file, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        if (error_out) *error_out = GetLastError();
        return FALSE;
    }
    ok = WriteFile(handle, payload, (DWORD)(sizeof(payload) - 1), &written, NULL) && written == (DWORD)(sizeof(payload) - 1);
    if (!ok && error_out) *error_out = GetLastError();
    CloseHandle(handle);
    if (!DeleteFileW(file)) {
        char shown[MAX_PATH];
        x104_narrow(shown, sizeof(shown), file);
        x104_logf("X104 platform: WARNING could not delete the write-test file %s (error %lu)", shown, (unsigned long)GetLastError());
    }
    return ok;
}

/* ======================================================================================================================
 * 2. options (command line), roots
 * ====================================================================================================================== */
static void x104_platform_parse_options(void)
{
    const wchar_t *cursor = GetCommandLineW();
    wchar_t argument[MAX_PATH];
    const wchar_t *value;
    char shown[MAX_PATH];
    if (!cursor) return;
    cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));           /* argv[0] */
    while (*cursor) {
        cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
        if (host_argument_equals(argument, L"--offline")) {
            x104_platform_state.offline = TRUE;
        } else if (host_argument_equals(argument, L"--bypass-network")) {
            x104_platform_state.bypass_network = TRUE;
        } else if (host_argument_equals(argument, L"--bypass-vibrate")) {
            x104_platform_state.bypass_vibrate = TRUE;
        } else if ((value = arm32_iat_argument_value(argument, L"--data-dir=")) != NULL) {
            if (lstrlenW(argument) >= MAX_PATH - 1) {
                x104_logf("X104 platform: option --data-dir= is %d characters or longer (limit %d): it would be truncated - ignored", (int)lstrlenW(argument),
                          (int)(MAX_PATH - 1));
            } else if (value[0]) {
                lstrcpynW(x104_platform_state.data_dir, value, MAX_PATH);
                x104_platform_state.data_dir_given = TRUE;
            } else {
                x104_narrow(shown, sizeof(shown), argument);
                x104_logf("X104 platform: option %s has an empty value - ignored", shown);
            }
        }
    }
    x104_logf("X104 platform: options: --offline=%d --bypass-network=%d --bypass-vibrate=%d --data-dir %s", (int)x104_platform_state.offline,
              (int)x104_platform_state.bypass_network, (int)x104_platform_state.bypass_vibrate, x104_platform_state.data_dir_given ? "given" : "not given");
}

/* Package.InstalledLocation.Path must survive the game's low-byte narrowing: ASCII only, else the 8.3 name, else FATAL.  Sets x104.game_root to what the fake returns. */
static BOOL x104_platform_check_game_root(void)
{
    wchar_t short_path[MAX_PATH];
    char shown[MAX_PATH];
    char shown_short[MAX_PATH];
    if (!x104.game_root[0]) {
        x104_logf("X104 platform: FATAL the game root (exe directory) is empty - cannot build Package.InstalledLocation");
        return FALSE;
    }
    if (x104_platform_is_ascii(x104.game_root)) return TRUE;
    x104_narrow(shown, sizeof(shown), x104.game_root);
    if (x104_platform_short_path(x104.game_root, short_path, MAX_PATH) && x104_platform_is_ascii(short_path)) {
        x104_narrow(shown_short, sizeof(shown_short), short_path);
        x104_logf("X104 platform: WARNING the game folder path contains non-ASCII characters: %s - using its 8.3 short path %s for Package.InstalledLocation "
                  "(the game narrows UTF-16 to bytes; moving the game to an ASCII-only path is still recommended)", shown, shown_short);
        lstrcpyW(x104.game_root, short_path);
        x104_platform_normalize(x104.game_root);
        return TRUE;
    }
    x104_logf("X104 platform: FATAL the game folder path contains non-ASCII characters: %s - move the game to an ASCII-only path", shown);
    x104_logf("X104 platform: (GetShortPathNameW gave no ASCII 8.3 name either: 8.3 name generation is probably disabled on that volume)");
    return FALSE;
}

/* validate one data-root candidate: absolute, ASCII (or its 8.3 name), creatable, writable.  TRUE = *final_path is usable. */
static BOOL x104_platform_try_root(const wchar_t *candidate, const char *source, wchar_t *final_path)
{
    wchar_t path[MAX_PATH];
    wchar_t short_path[MAX_PATH];
    char shown[MAX_PATH];
    char shown_short[MAX_PATH];
    DWORD error = 0;
    DWORD length;

    final_path[0] = 0;
    if (!candidate || !candidate[0]) {
        x104_logf("X104 platform: data root candidate (%s) is empty - skipped", source);
        return FALSE;
    }
    if (x104_platform_is_absolute(candidate)) {
        if (lstrlenW(candidate) >= MAX_PATH - 8) {
            x104_logf("X104 platform: data root candidate (%s) is too long (%d characters) - skipped", source, (int)lstrlenW(candidate));
            return FALSE;
        }
        lstrcpyW(path, candidate);
    } else {
        typedef DWORD (WINAPI *FullPathFn)(LPCWSTR, DWORD, LPWSTR, LPWSTR *);
        FullPathFn full_path = (FullPathFn)x104_platform_kernel32_proc("GetFullPathNameW");
        length = full_path ? full_path(candidate, MAX_PATH - 8, path, NULL) : 0;
        if (!length || length >= MAX_PATH - 8) {
            x104_narrow(shown, sizeof(shown), candidate);
            x104_logf("X104 platform: data root candidate (%s) '%s' is not absolute and GetFullPathNameW failed (length %lu, error %lu) - skipped", source,
                      shown, (unsigned long)length, (unsigned long)GetLastError());
            return FALSE;
        }
    }
    x104_platform_normalize(path);
    x104_narrow(shown, sizeof(shown), path);
    x104_logf("X104 platform: data root candidate (%s): %s", source, shown);
    if (!x104_platform_make_dirs(path, &error)) {
        x104_logf("X104 platform: data root candidate (%s) %s cannot be created: Win32 error %lu - skipped", source, shown, (unsigned long)error);
        return FALSE;
    }
    if (!x104_platform_is_ascii(path)) {
        if (x104_platform_short_path(path, short_path, MAX_PATH) && x104_platform_is_ascii(short_path)) {
            x104_narrow(shown_short, sizeof(shown_short), short_path);
            x104_logf("X104 platform: WARNING data root candidate (%s) contains non-ASCII characters (%s): using its 8.3 short path %s", source, shown,
                      shown_short);
            lstrcpyW(path, short_path);
            x104_platform_normalize(path);
            x104_narrow(shown, sizeof(shown), path);
        } else {
            x104_logf("X104 platform: data root candidate (%s) %s contains non-ASCII characters and has no ASCII 8.3 name - skipped (the game would corrupt every "
                      "path under it)", source, shown);
            return FALSE;
        }
    }
    if (!x104_platform_write_test(path, &error)) {
        x104_logf("X104 platform: data root candidate (%s) %s is not writable (create/write/delete test failed, Win32 error %lu) - skipped", source, shown,
                  (unsigned long)error);
        return FALSE;
    }
    if (lstrlenW(path) > 150) {
        x104_logf("X104 platform: WARNING data root %s is %d characters long: world paths below it (\\games\\com.mojang\\minecraftpe\\minecraftWorlds\\...) "
                  "may exceed MAX_PATH", shown, (int)lstrlenW(path));
    }
    lstrcpyW(final_path, path);
    return TRUE;
}

/* (a) --data-dir=, (b) %APPDATA%\<profile data_subdir> (MinecraftPE\0.10.4, Win32Craft\Redcon), (c) <game_root>\LocalState: the first usable one becomes x104.data_root */
static BOOL x104_platform_choose_data_root(void)
{
    wchar_t appdata[MAX_PATH];
    wchar_t short_path[MAX_PATH];
    wchar_t candidate[MAX_PATH];
    wchar_t chosen[MAX_PATH];
    char shown[MAX_PATH];
    HRESULT hr;

    x104.data_root[0] = 0;
    if (x104_platform_state.data_dir_given) {
        if (x104_platform_try_root(x104_platform_state.data_dir, "command line --data-dir", chosen)) {
            lstrcpyW(x104.data_root, chosen);
            lstrcpynA(x104_platform_state.data_root_source, "command line --data-dir", (int)sizeof(x104_platform_state.data_root_source));
            return TRUE;
        }
        x104_logf("X104 platform: WARNING --data-dir was given but is unusable - falling back to the default data folders");
    }

    appdata[0] = 0;
    hr = SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, appdata);
    if (FAILED(hr) || !appdata[0]) {
        x104_logf("X104 platform: SHGetFolderPathW(CSIDL_APPDATA) failed: 0x%08lx %s - <AppData> candidate skipped", (unsigned long)hr, x104_hr_name(hr));
    } else {
        if (!x104_platform_is_ascii(appdata)) {
            x104_narrow(shown, sizeof(shown), appdata);
            if (x104_platform_short_path(appdata, short_path, MAX_PATH) && x104_platform_is_ascii(short_path)) {
                x104_logf("X104 platform: WARNING <AppData> (%s) contains non-ASCII characters (user name?): using its 8.3 short path", shown);
                lstrcpyW(appdata, short_path);
            } else {
                x104_logf("X104 platform: <AppData> (%s) contains non-ASCII characters and has no ASCII 8.3 name", shown);
            }
        }
        if (x104_platform_join(candidate, MAX_PATH, appdata, x104_profile->data_subdir) &&
            x104_platform_try_root(candidate, x104_profile->data_subdir_text, chosen)) {
            lstrcpyW(x104.data_root, chosen);
            lstrcpynA(x104_platform_state.data_root_source, x104_profile->data_subdir_text, (int)sizeof(x104_platform_state.data_root_source));
            return TRUE;
        }
    }

    if (x104_platform_join(candidate, MAX_PATH, x104.game_root, L"LocalState") &&
        x104_platform_try_root(candidate, "<game root>\\LocalState", chosen)) {
        lstrcpyW(x104.data_root, chosen);
        lstrcpynA(x104_platform_state.data_root_source, "<game root>\\LocalState", (int)sizeof(x104_platform_state.data_root_source));
        return TRUE;
    }
    x104_logf("X104 platform: FATAL no usable data folder: tried --data-dir, %s and <game root>\\LocalState (each reason is logged above). "
              "Pass --data-dir=<ASCII path of a writable folder>", x104_profile->data_subdir_text);
    return FALSE;
}

/* ======================================================================================================================
 * 3. data-cell patches (AppPlatform_Winrt vtable) and the XAudio2 probe with its fallback cells
 * ====================================================================================================================== */
static const BYTE x104_platform_noop_bytes[6] = { 0x2d, 0xe9, 0x00, 0x48, 0xeb, 0x46 };                      /* push.w {fp,lr}; mov fp,sp : shared no-op 0xBF60 */
static const BYTE x104_platform_ret0_bytes[8] = { 0x2d, 0xe9, 0x00, 0x48, 0xeb, 0x46, 0x00, 0x20 };          /* ... movs r0,#0 : shared "return 0" 0x29BA0 */

static BOOL x104_platform_replacement_ok(DWORD rva, const BYTE *expected, SIZE_T size, const char *what)
{
    return x104_check_game_bytes(rva, expected, size, what);
}

/* one cell: verify the function the cell points at (bytes) and the cell itself, then patch it */
static BOOL x104_platform_patch_one(DWORD cell_rva, DWORD expected_function_rva, const BYTE *expected_bytes, DWORD replacement_rva, const char *what)
{
    char label[128];
    BOOL ok;
    wsprintfA(label, "%s original function %08lx", what, (unsigned long)expected_function_rva);
    if (expected_bytes && !x104_check_game_bytes(expected_function_rva, expected_bytes, 12, label)) {
        x104_logf("X104 platform: ERROR cell %08lx (%s) NOT patched: the function it should point at has unexpected bytes", (unsigned long)cell_rva, what);
        return FALSE;
    }
    ok = arm32_patch_cell((void **)X104_DATA(cell_rva), X104_CODE(expected_function_rva), X104_CODE(replacement_rva), NULL, what);
    if (ok) x104_logf("X104 platform: patched AppPlatform cell %08lx (%s): function %08lx -> shared stub %08lx", (unsigned long)cell_rva, what,
                      (unsigned long)expected_function_rva, (unsigned long)replacement_rva);
    else x104_logf("X104 platform: ERROR cell %08lx (%s) could not be patched (see the ARM32 patch line above)", (unsigned long)cell_rva, what);
    return ok;
}

static void x104_platform_patch_cells(void)
{
    BOOL stubs_ok = x104_platform_replacement_ok(0x29BA0, x104_platform_ret0_bytes, sizeof(x104_platform_ret0_bytes), "shared return-0 function 0x29BA0") &&
                    x104_platform_replacement_ok(X104_RVA_NOOP_FUNCTION, x104_platform_noop_bytes, sizeof(x104_platform_noop_bytes),
                                                 "shared no-op function 0xBF60");
    if (!stubs_ok) {
        x104_logf("X104 platform: ERROR the shared stub functions 0x29BA0 / 0xBF60 do not have the expected bytes: NO AppPlatform cell was patched "
                  "(the Store 'buy' path would run PPL tasks on the real runtime)");
        return;
    }
    /* vf23 buy: the store would run PPL tasks that activate the REAL runtime (facts M4) -> "return 0" */
    x104_platform_patch_one(X104_RVA_CELL_BUY, 0x4E350, ARM0104_PLATFORM_SERVICES_STORE_BUY_EXPECTED, 0x29BA0, "AppPlatform_Winrt vf23 buy -> return 0");
    if (x104_platform_state.bypass_network)
        x104_platform_patch_one(X104_RVA_CELL_NETWORK, 0x4EF14, ARM0104_PLATFORM_SERVICES_NETWORK_IS_ENABLED_EXPECTED, 0x29BA0,
                                "AppPlatform_Winrt vf19 isNetworkEnabled -> return 0 (--bypass-network)");
    else
        x104_logf("X104 platform: cell %08lx (vf19 isNetworkEnabled) not patched: served by the NetworkInformation fake", (unsigned long)X104_RVA_CELL_NETWORK);
    if (x104_platform_state.bypass_vibrate)
        x104_platform_patch_one(X104_RVA_CELL_VIBRATE, 0x50FF0, ARM0104_PLATFORM_SERVICES_VIBRATION_VIBRATE_EXPECTED, X104_RVA_NOOP_FUNCTION,
                                "AppPlatform_Winrt vf29 vibrate -> no-op (--bypass-vibrate)");
    else
        x104_logf("X104 platform: cell %08lx (vf29 vibrate) not patched: served by the VibrationDevice fake", (unsigned long)X104_RVA_CELL_VIBRATE);
    x104_logf("X104 platform: cell %08lx (vf40 getFreeMemory) not patched: served by the MemoryManager fake; cell %08lx (vf26 touch-only) left alone",
              (unsigned long)X104_RVA_CELL_FREEMEMORY, (unsigned long)X104_RVA_CELL_TOUCHONLY);
}

/* XAudio2 probe: the same two calls the game's SoundSystemRT ctor makes.  Returns S_OK or the failing HRESULT; *stage names the failing step. */
static HRESULT x104_platform_xaudio2_probe_body(const char **stage)
{
    typedef HRESULT (WINAPI *XAudio2CreateFn)(void **engine, UINT32 flags, UINT32 processor);
    typedef HRESULT (X104_API *CreateMasteringVoiceFn)(void *self, void **voice, UINT32 channels, UINT32 rate, UINT32 flags, const wchar_t *device,
                                                       const void *chain, int category);
    typedef void (X104_API *DestroyVoiceFn)(void *voice);
    HMODULE module;
    XAudio2CreateFn create;
    void *engine = NULL;
    void *voice = NULL;
    HRESULT hr;
    DWORD error;

    /* XAudio2 needs an initialised apartment on the calling thread (CO_E_NOTINITIALIZED otherwise, found by the host-side test).  The boot already ran
     * RoInitialize(MTA) on this thread (S_FALSE here); RPC_E_CHANGED_MODE would mean an STA thread, which XAudio2 also accepts. */
    hr = RoInitialize(1 /* RO_INIT_MULTITHREADED */);
    x104_logf("X104 platform: XAudio2 probe: RoInitialize(MTA) on the install thread = 0x%08lx %s (S_FALSE = already initialised by the boot)", (unsigned long)hr,
              x104_hr_name(hr));
    *stage = "LoadLibraryW(XAudio2_8.dll)";
    module = LoadLibraryW(L"XAudio2_8.dll");
    if (!module) {
        error = GetLastError();
        return (HRESULT)((error & 0xFFFFu) | 0x80070000u);
    }
    *stage = "GetProcAddress(XAudio2Create)";
    create = (XAudio2CreateFn)GetProcAddress(module, (LPCSTR)(ULONG_PTR)1);              /* ordinal 1, as the game's import */
    if (!create) create = (XAudio2CreateFn)GetProcAddress(module, "XAudio2Create");
    if (!create) {
        error = GetLastError();
        return (HRESULT)((error & 0xFFFFu) | 0x80070000u);
    }
    *stage = "XAudio2Create(flags 0, processor 1)";
    hr = create(&engine, 0, 1);
    if (FAILED(hr) || !engine) return FAILED(hr) ? hr : E_FAIL;
    *stage = "IXAudio2::CreateMasteringVoice(2 ch, 44100 Hz, category 6)";
    hr = X104_VFN(engine, 7, CreateMasteringVoiceFn)(engine, &voice, 2, 44100, 0, NULL, NULL, 6);
    if (FAILED(hr) || !voice) {
        x104_raw_release(engine);
        return FAILED(hr) ? hr : E_FAIL;
    }
    *stage = "DestroyVoice / Release";
    X104_VFN(voice, 18, DestroyVoiceFn)(voice);
    x104_raw_release(engine);
    return S_OK;
}

static void x104_platform_xaudio2_probe(void)
{
    const char *stage = "start";
    HRESULT hr = E_FAIL;
    X104Seh seh;
    BOOL clean = TRUE;
    BOOL patched_play, patched_init;
    memset(&seh, 0, sizeof(seh));
    __try {
        hr = x104_platform_xaudio2_probe_body(&stage);
    } __except (x104_seh_filter((EXCEPTION_POINTERS *)_exception_info(), &seh)) {
        clean = FALSE;
        hr = E_FAIL;
    }
    if (!clean) {
        x104_logf("X104 platform: XAudio2 probe raised an exception during '%s': code=%08lx at %p (game rva %08lx)", stage, (unsigned long)seh.code, seh.address,
                  (unsigned long)seh.rva);
    }
    if (SUCCEEDED(hr)) {
        x104_platform_state.xaudio2_ok = TRUE;
        x104_logf("X104 platform: XAudio2 probe ok (XAudio2Create + CreateMasteringVoice 2ch/44100 succeeded and were released; the game creates its own engine in the init job)");
        return;
    }
    x104_logf("X104 platform: XAudio2 probe FAILED hr=%08lx - sound disabled", (unsigned long)hr);
    if (!x104_feat(X104_FEAT_MCPE_PATCHES)) {
        x104_logf("X104 platform:   failing step: %s (%s) - no audio endpoint / audio service down?  (%s handles a failing XAudio2Create itself: it runs silently)", stage,
                  x104_hr_name(hr), x104_profile->title);
        return;
    }
    x104_logf("X104 platform:   failing step: %s (%s) - no audio endpoint / audio service down?  SoundSystemRT::play and ::init become no-ops", stage, x104_hr_name(hr));
    patched_play = arm32_patch_cell((void **)X104_DATA(X104_RVA_CELL_SOUND_PLAY), X104_CODE(0x106688), X104_CODE(X104_RVA_NOOP_FUNCTION), NULL,
                                    "SoundSystemRT::play");
    patched_init = arm32_patch_cell((void **)X104_DATA(X104_RVA_CELL_SOUND_VF10), X104_CODE(0x106630), X104_CODE(X104_RVA_NOOP_FUNCTION), NULL,
                                    "SoundSystemRT::init");
    if (!patched_play || !patched_init)
        x104_logf("X104 platform: ERROR the SoundSystemRT fallback cells were not both patched (play=%d init=%d): the first sound will crash the game (NULL engine)",
                  (int)patched_play, (int)patched_init);
}

/* ======================================================================================================================
 * 4. memory figures (MemoryManager)
 * ====================================================================================================================== */
typedef struct X104PlatformMemStatus {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    ULONGLONG ullTotalPhys;
    ULONGLONG ullAvailPhys;
    ULONGLONG ullTotalPageFile;
    ULONGLONG ullAvailPageFile;
    ULONGLONG ullTotalVirtual;
    ULONGLONG ullAvailVirtual;
    ULONGLONG ullAvailExtendedVirtual;
} X104PlatformMemStatus;

static const char *x104_platform_tier_name(ULONGLONG free_bytes)
{
    if (free_bytes > 0x1EA00000ULL) return "BEST (free memory > 0x1EA00000, ~490 MiB)";
    if (free_bytes > 0x0E600000ULL) return "MIDDLE (free memory > 0xE600000, ~230 MiB)";
    return "LOWEST (free memory <= 0xE600000)";
}

/* decide once: usage = 256 MiB constant, limit = 1 GiB when the machine has >= 1.5 GiB physical RAM, else 512 MiB.  Safe on zero state, any thread. */
static void x104_platform_memory_decide(void)
{
    typedef BOOL (WINAPI *MemoryStatusFn)(void *);
    MemoryStatusFn function;
    X104PlatformMemStatus status;
    ULONGLONG free_bytes;
    if (!x104_once_begin(&x104_platform_memory_state)) return;
    memset(&status, 0, sizeof(status));
    status.dwLength = (DWORD)sizeof(status);
    function = (MemoryStatusFn)x104_platform_kernel32_proc("GlobalMemoryStatusEx");
    x104_platform_state.mem_usage = 0x10000000ULL;                            /* 256 MiB */
    if (function && function(&status)) {
        x104_platform_state.phys_known = TRUE;
        x104_platform_state.phys_mib = (DWORD)(status.ullTotalPhys >> 20);
        x104_platform_state.mem_limit = status.ullTotalPhys >= 0x60000000ULL ? 0x40000000ULL : 0x20000000ULL;
    } else {
        x104_platform_state.phys_known = FALSE;
        x104_platform_state.mem_limit = 0x20000000ULL;                        /* unknown machine: the conservative figure */
        x104_logf("X104 platform: GlobalMemoryStatusEx %s (error %lu): assuming a small machine, limit 512 MiB", function ? "failed" : "is not available",
                  (unsigned long)GetLastError());
    }
    free_bytes = x104_platform_state.mem_limit - x104_platform_state.mem_usage;
    x104_logf("X104 platform: MemoryManager decision: physical RAM %lu MiB%s -> usage=%08lx%08lx limit=%08lx%08lx (hex hi/lo), free for the game = %lu MiB -> "
              "Options tier %s", (unsigned long)x104_platform_state.phys_mib, x104_platform_state.phys_known ? "" : " (unknown)",
              (unsigned long)(x104_platform_state.mem_usage >> 32), (unsigned long)(DWORD)x104_platform_state.mem_usage,
              (unsigned long)(x104_platform_state.mem_limit >> 32), (unsigned long)(DWORD)x104_platform_state.mem_limit,
              (unsigned long)(DWORD)(free_bytes >> 20), x104_platform_tier_name(free_bytes));
    x104_once_end(&x104_platform_memory_state);
}

/* ======================================================================================================================
 * 5. StorageFolder (IStorageFolder + IStorageItem): the Package folder and the LocalFolder
 * ====================================================================================================================== */
typedef struct X104PlatformFolder {
    X104Object base;
    const wchar_t *path;            /* points at x104.game_root / x104.data_root: the fake returns exactly what those fields hold */
    const char *label;              /* "InstalledLocation" / "LocalFolder" */
    volatile LONG path_calls;
} X104PlatformFolder;

static HRESULT X104_API x104_platform_item_get_path(void *self, HSTRING *value)
{
    X104PlatformFolder *folder = X104_THIS(X104PlatformFolder, self);
    char shown[MAX_PATH];
    HRESULT hr;
    if (!value) return E_POINTER;
    hr = x104_hstring_create(folder->path ? folder->path : L"", value);
    if (FAILED(hr)) {
        x104_logf("X104 platform: ERROR IStorageItem.get_Path (%s) could not create the HSTRING: 0x%08lx %s", folder->label ? folder->label : "folder", (unsigned long)hr,
                  x104_hr_name(hr));
        return hr;
    }
    x104_narrow(shown, sizeof(shown), folder->path);
    X104_LOG_LIMITED(&folder->path_calls, 6, "X104 platform: %s.Path -> %s", folder->label ? folder->label : "StorageFolder", shown);
    return S_OK;
}

static HRESULT X104_API x104_platform_item_get_name(void *self, HSTRING *value)
{
    X104PlatformFolder *folder = X104_THIS(X104PlatformFolder, self);
    const wchar_t *path = folder->path ? folder->path : L"";
    const wchar_t *name = path;
    unsigned index;
    if (!value) return E_POINTER;
    for (index = 0; path[index]; ++index) {
        if (path[index] == L'\\' || path[index] == L'/') name = path + index + 1;
    }
    return x104_hstring_create(name, value);
}

X104_VTBL(x104_platform_vt_folder, IStorageFolder) = {
    X104_INSPECTABLE_SLOTS                      /* every IStorageFolder method (async file APIs) stays a logging hole: the game uses none of them */
};
X104_VTBL(x104_platform_vt_item, IStorageItem) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IStorageItem, get_Name, x104_platform_item_get_name),
    X104_SLOT(IStorageItem, get_Path, x104_platform_item_get_path)
};
static const X104IfaceDesc x104_platform_folder_ifaces[] = {
    X104_IFACE(IStorageFolder, x104_platform_vt_folder),
    X104_IFACE(IStorageItem, x104_platform_vt_item)
};
X104_CLASS_DEF(x104_platform_folder_class, "StorageFolder", L"Windows.Storage.StorageFolder", x104_platform_folder_ifaces, X104_CLASS_AGILE, NULL);

/* which: 0 = Package.InstalledLocation (game root), 1 = ApplicationData.LocalFolder (data root).  Created lazily (the roots are final after install). */
static X104Object *x104_platform_folder_object(int which)
{
    static volatile LONG states[2];
    static X104Object *objects[2];
    if (which < 0 || which > 1) return NULL;
    if (x104_once_begin(&states[which])) {
        X104PlatformFolder *folder = (X104PlatformFolder *)x104_object_new(&x104_platform_folder_class, sizeof(X104PlatformFolder));
        if (folder) {
            folder->path = which ? x104.data_root : x104.game_root;
            folder->label = which ? "ApplicationData.Current.LocalFolder" : "Package.Current.InstalledLocation";
            objects[which] = &folder->base;
        }
        x104_once_end(&states[which]);
    }
    return objects[which];
}

/* ======================================================================================================================
 * 6. Package   (Windows.ApplicationModel.Package: IPackageStatics -> IPackage -> InstalledLocation)
 * ====================================================================================================================== */
static HRESULT X104_API x104_platform_package_get_installed_location(void *self, void **value)
{
    static volatile LONG logged;
    char shown[MAX_PATH];
    HRESULT hr;
    (void)self;
    hr = x104_return_iface(x104_platform_folder_object(0), &X104_IID_IStorageFolder, value, "Package.InstalledLocation");
    x104_narrow(shown, sizeof(shown), x104.game_root);
    X104_LOG_LIMITED(&logged, 4, "X104 platform: Package.Current.InstalledLocation.Path -> %s (hr 0x%08lx)", shown, (unsigned long)hr);
    return hr;
}

X104_VTBL(x104_platform_vt_package, IPackage) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IPackage, get_InstalledLocation, x104_platform_package_get_installed_location)
};
static const X104IfaceDesc x104_platform_package_ifaces[] = { X104_IFACE(IPackage, x104_platform_vt_package) };
X104_CLASS_DEF(x104_platform_package_class, "Package", L"Windows.ApplicationModel.Package", x104_platform_package_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_package_object, x104_platform_package_class, X104Object)

static HRESULT X104_API x104_platform_package_statics_get_current(void *self, void **value)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: Package.get_Current");
    return x104_return_iface(x104_platform_package_object(), &X104_IID_IPackage, value, "Package.Current");
}

X104_VTBL(x104_platform_vt_package_statics, IPackageStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IPackageStatics, get_Current, x104_platform_package_statics_get_current)
};
static const X104IfaceDesc x104_platform_package_statics_ifaces[] = { X104_IFACE(IPackageStatics, x104_platform_vt_package_statics) };
X104_CLASS_DEF(x104_platform_package_statics_class, "PackageStatics", L"Windows.ApplicationModel.Package", x104_platform_package_statics_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_package_statics, x104_platform_package_statics_class, X104Object)

/* ======================================================================================================================
 * 7. ApplicationData (IApplicationDataStatics -> IApplicationData -> LocalFolder / LocalSettings -> IApplicationDataContainer.Values)
 * ====================================================================================================================== */
static HRESULT X104_API x104_platform_container_get_name(void *self, HSTRING *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;                                  /* the LocalSettings container has an empty name */
    return S_OK;
}

static HRESULT X104_API x104_platform_container_get_values(void *self, void **value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;                                  /* a NULL IPropertySet is harmless: MainPage::vf10/vf11 ignore their argument */
    X104_LOG_LIMITED(&logged, 6, "X104 platform: ApplicationData.Current.LocalSettings.Values -> NULL (S_OK)");
    return S_OK;
}

X104_VTBL(x104_platform_vt_container, IApplicationDataContainer) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationDataContainer, get_Name, x104_platform_container_get_name),
    X104_SLOT(IApplicationDataContainer, get_Values, x104_platform_container_get_values)
};
static const X104IfaceDesc x104_platform_container_ifaces[] = { X104_IFACE(IApplicationDataContainer, x104_platform_vt_container) };
X104_CLASS_DEF(x104_platform_container_class, "ApplicationDataContainer", L"Windows.Storage.ApplicationDataContainer", x104_platform_container_ifaces,
               X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_container_object, x104_platform_container_class, X104Object)

static HRESULT X104_API x104_platform_appdata_get_local_settings(void *self, void **value)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 4, "X104 platform: ApplicationData.Current.LocalSettings -> container object (non-NULL: the Launched/Suspending handlers dereference it)");
    return x104_return_iface(x104_platform_container_object(), &X104_IID_IApplicationDataContainer, value, "ApplicationData.LocalSettings");
}

/* RoamingSettings (REDCON 1 keeps string key/values there: games_recon/redcon_redcon-platform.md F9).  Values = a real Windows.Foundation.Collections.PropertySet
 * from the OS runtime (IPropertySet -> IMap<String,Object>; works in a desktop process), created once; in memory only (not persisted between runs yet). */
static void *x104_platform_roaming_values;          /* IInspectable of the PropertySet (AddRef'd once, kept) */

static HRESULT X104_API x104_platform_roaming_get_values(void *self, void **value)
{
    static volatile LONG state;
    static volatile LONG logged;
    HRESULT hr = S_OK;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    if (x104_once_begin(&state)) {
        void *factory = NULL;
        if (!x104.original_activation) {
            hr = E_FAIL;
        } else {
            hr = ((X104ActivationFn)x104.original_activation)(L"Windows.Foundation.Collections.PropertySet", &X104_IID_IActivationFactory, &factory);
            if (SUCCEEDED(hr) && factory) {
                hr = X104_VFN(factory, 6, HRESULT (X104_API *)(void *, void **))(factory, &x104_platform_roaming_values);   /* IActivationFactory.ActivateInstance */
                x104_raw_release(factory);
            }
        }
        x104_logf("X104 platform: RoamingSettings.Values = OS PropertySet %p (hr 0x%08lx %s; in memory only, not persisted between runs)", x104_platform_roaming_values,
                  (unsigned long)hr, x104_hr_name(hr));
        x104_once_end(&state);
    }
    if (!x104_platform_roaming_values) {
        X104_LOG_LIMITED(&logged, 6, "X104 platform: ERROR RoamingSettings.Values unavailable (no PropertySet) -> E_FAIL");
        return E_FAIL;
    }
    x104_raw_addref(x104_platform_roaming_values);
    *value = x104_platform_roaming_values;
    return S_OK;
}

static HRESULT X104_API x104_platform_roaming_get_name(void *self, HSTRING *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

X104_VTBL(x104_platform_vt_roaming, IApplicationDataContainer) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationDataContainer, get_Name, x104_platform_roaming_get_name),
    X104_SLOT(IApplicationDataContainer, get_Values, x104_platform_roaming_get_values)
};
static const X104IfaceDesc x104_platform_roaming_ifaces[] = { X104_IFACE(IApplicationDataContainer, x104_platform_vt_roaming) };
X104_CLASS_DEF(x104_platform_roaming_class, "ApplicationDataContainer(Roaming)", L"Windows.Storage.ApplicationDataContainer", x104_platform_roaming_ifaces,
               X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_roaming_object, x104_platform_roaming_class, X104Object)

static HRESULT X104_API x104_platform_appdata_get_roaming_settings(void *self, void **value)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 4, "X104 platform: ApplicationData.Current.RoamingSettings -> container object");
    return x104_return_iface(x104_platform_roaming_object(), &X104_IID_IApplicationDataContainer, value, "ApplicationData.RoamingSettings");
}

static HRESULT X104_API x104_platform_appdata_get_local_folder(void *self, void **value)
{
    static volatile LONG logged;
    char shown[MAX_PATH];
    HRESULT hr;
    (void)self;
    hr = x104_return_iface(x104_platform_folder_object(1), &X104_IID_IStorageFolder, value, "ApplicationData.LocalFolder");
    x104_narrow(shown, sizeof(shown), x104.data_root);
    X104_LOG_LIMITED(&logged, 4, "X104 platform: ApplicationData.Current.LocalFolder.Path -> %s (hr 0x%08lx)", shown, (unsigned long)hr);
    return hr;
}

X104_VTBL(x104_platform_vt_appdata, IApplicationData) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationData, get_LocalSettings, x104_platform_appdata_get_local_settings),
    X104_SLOT(IApplicationData, get_RoamingSettings, x104_platform_appdata_get_roaming_settings),
    X104_SLOT(IApplicationData, get_LocalFolder, x104_platform_appdata_get_local_folder)
};
static const X104IfaceDesc x104_platform_appdata_ifaces[] = { X104_IFACE(IApplicationData, x104_platform_vt_appdata) };
X104_CLASS_DEF(x104_platform_appdata_class, "ApplicationData", L"Windows.Storage.ApplicationData", x104_platform_appdata_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_appdata_object, x104_platform_appdata_class, X104Object)

static HRESULT X104_API x104_platform_appdata_statics_get_current(void *self, void **value)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: ApplicationData.get_Current");
    return x104_return_iface(x104_platform_appdata_object(), &X104_IID_IApplicationData, value, "ApplicationData.Current");
}

X104_VTBL(x104_platform_vt_appdata_statics, IApplicationDataStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationDataStatics, get_Current, x104_platform_appdata_statics_get_current)
};
static const X104IfaceDesc x104_platform_appdata_statics_ifaces[] = { X104_IFACE(IApplicationDataStatics, x104_platform_vt_appdata_statics) };
X104_CLASS_DEF(x104_platform_appdata_statics_class, "ApplicationDataStatics", L"Windows.Storage.ApplicationData", x104_platform_appdata_statics_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_appdata_statics, x104_platform_appdata_statics_class, X104Object)

/* ======================================================================================================================
 * 8. MemoryManager (IMemoryManagerStatics slots 6 / 7: 64-bit outputs through ULONGLONG*)
 * ====================================================================================================================== */
static HRESULT X104_API x104_platform_memory_get_usage(void *self, ULONGLONG *value)
{
    (void)self;
    if (!value) return E_POINTER;
    x104_platform_memory_decide();
    *value = x104_platform_state.mem_usage;
    X104_LOG_LIMITED(&x104_platform_memory_calls, 8, "X104 platform: MemoryManager.AppMemoryUsage -> %08lx%08lx", (unsigned long)(*value >> 32), (unsigned long)(DWORD)*value);
    return S_OK;
}

static HRESULT X104_API x104_platform_memory_get_limit(void *self, ULONGLONG *value)
{
    (void)self;
    if (!value) return E_POINTER;
    x104_platform_memory_decide();
    *value = x104_platform_state.mem_limit;
    X104_LOG_LIMITED(&x104_platform_memory_calls, 8, "X104 platform: MemoryManager.AppMemoryUsageLimit -> %08lx%08lx", (unsigned long)(*value >> 32), (unsigned long)(DWORD)*value);
    return S_OK;
}

X104_VTBL(x104_platform_vt_memory, IMemoryManagerStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IMemoryManagerStatics, get_AppMemoryUsage, x104_platform_memory_get_usage),
    X104_SLOT(IMemoryManagerStatics, get_AppMemoryUsageLimit, x104_platform_memory_get_limit)
};
static const X104IfaceDesc x104_platform_memory_ifaces[] = { X104_IFACE(IMemoryManagerStatics, x104_platform_vt_memory) };
X104_CLASS_DEF(x104_platform_memory_class, "MemoryManagerStatics", L"Windows.System.MemoryManager", x104_platform_memory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_memory_statics, x104_platform_memory_class, X104Object)

/* ======================================================================================================================
 * 9. VibrationDevice (statics: GetDefault; device: Vibrate(TimeSpan) / Cancel, called by the game WITHOUT QI on the pointer GetDefault returned)
 * ====================================================================================================================== */
/* private IID of the device interface (the real IVibrationDevice IID is not in the facts; the game never QIs it): the statics IID with its first byte changed */
static const GUID x104_platform_iid_vibration_device = { 0x332fd20e, 0x1c69, 0x4c91, { 0x94, 0x9e, 0x4b, 0xb6, 0x7a, 0x85, 0xbd, 0xc7 } };

static HRESULT X104_API x104_platform_vibration_vibrate(void *self, LONGLONG duration)
{
    (void)self;
    X104_LOG_LIMITED(&x104_platform_vibrate_calls, 4, "X104 platform: VibrationDevice.Vibrate(TimeSpan %08lx%08lx ticks = %ld ms) - no-op, call #%ld",
                     (unsigned long)((ULONGLONG)duration >> 32), (unsigned long)(DWORD)duration,
                     (duration >= 0 && duration < 0x7FFFFFFF) ? (long)(LONG)duration / 10000L : -1L, (long)x104_platform_vibrate_calls);
    return S_OK;
}

static HRESULT X104_API x104_platform_vibration_cancel(void *self)
{
    (void)self;
    X104_LOG_LIMITED(&x104_platform_cancel_calls, 3, "X104 platform: VibrationDevice.Cancel - no-op, call #%ld", (long)x104_platform_cancel_calls);
    return S_OK;
}

X104_VTBL(x104_platform_vt_vibration_device, IVibrationDevice) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IVibrationDevice, Vibrate, x104_platform_vibration_vibrate),
    X104_SLOT(IVibrationDevice, Cancel, x104_platform_vibration_cancel)
};
static const X104IfaceDesc x104_platform_vibration_device_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_vibration_device, "IVibrationDevice", x104_platform_vt_vibration_device)
};
X104_CLASS_DEF(x104_platform_vibration_device_class, "VibrationDevice", L"Windows.Phone.Devices.Notification.VibrationDevice", x104_platform_vibration_device_ifaces,
               X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_vibration_device, x104_platform_vibration_device_class, X104Object)

static HRESULT X104_API x104_platform_vibration_get_default(void *self, void **device)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: VibrationDevice.GetDefault -> fake device (Vibrate/Cancel are called without QI on this pointer)");
    return x104_return_iface(x104_platform_vibration_device(), &x104_platform_iid_vibration_device, device, "VibrationDevice.GetDefault");
}

X104_VTBL(x104_platform_vt_vibration_statics, IVibrationDeviceStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IVibrationDeviceStatics, GetDefault, x104_platform_vibration_get_default)
};
static const X104IfaceDesc x104_platform_vibration_statics_ifaces[] = { X104_IFACE(IVibrationDeviceStatics, x104_platform_vt_vibration_statics) };
X104_CLASS_DEF(x104_platform_vibration_statics_class, "VibrationDeviceStatics", L"Windows.Phone.Devices.Notification.VibrationDevice", x104_platform_vibration_statics_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_vibration_statics, x104_platform_vibration_statics_class, X104Object)

/* ======================================================================================================================
 * 10. NetworkInformation (GetInternetConnectionProfile -> a minimal profile: level 3 = InternetAccess, wlan = TRUE; --offline = S_OK + NULL)
 * ====================================================================================================================== */
static HRESULT X104_API x104_platform_profile_get_level(void *self, int *level)
{
    static volatile LONG logged;
    (void)self;
    if (!level) return E_POINTER;
    *level = 3;                                     /* NetworkConnectivityLevel.InternetAccess */
    X104_LOG_LIMITED(&logged, 3, "X104 platform: ConnectionProfile.GetNetworkConnectivityLevel -> 3 (InternetAccess)");
    return S_OK;
}

static HRESULT X104_API x104_platform_profile2_get_is_wwan(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_profile2_get_is_wlan(void *self, BYTE *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = 1;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: ConnectionProfile2.get_IsWlanConnectionProfile -> 1");
    return S_OK;
}

X104_VTBL(x104_platform_vt_profile, IConnectionProfile) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IConnectionProfile, GetNetworkConnectivityLevel, x104_platform_profile_get_level)
};
X104_VTBL(x104_platform_vt_profile2, IConnectionProfile2) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IConnectionProfile2, get_IsWwanConnectionProfile, x104_platform_profile2_get_is_wwan),
    X104_SLOT(IConnectionProfile2, get_IsWlanConnectionProfile, x104_platform_profile2_get_is_wlan)
};
static const X104IfaceDesc x104_platform_profile_ifaces[] = {
    X104_IFACE(IConnectionProfile, x104_platform_vt_profile),
    X104_IFACE(IConnectionProfile2, x104_platform_vt_profile2)
};
X104_CLASS_DEF(x104_platform_profile_class, "ConnectionProfile", L"Windows.Networking.Connectivity.ConnectionProfile", x104_platform_profile_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_profile_object, x104_platform_profile_class, X104Object)

static HRESULT X104_API x104_platform_network_get_profile(void *self, void **profile)
{
    HRESULT hr;
    (void)self;
    if (!profile) return E_POINTER;
    *profile = NULL;
    if (x104_platform_state.offline) {
        X104_LOG_LIMITED(&x104_platform_profile_calls, 4, "X104 platform: NetworkInformation.GetInternetConnectionProfile -> NULL (--offline: the game treats it as no network)");
        return S_OK;
    }
    hr = x104_return_iface(x104_platform_profile_object(), &X104_IID_IConnectionProfile, profile, "NetworkInformation.GetInternetConnectionProfile");
    X104_LOG_LIMITED(&x104_platform_profile_calls, 4, "X104 platform: NetworkInformation.GetInternetConnectionProfile -> level 3 wlan 1 (hr 0x%08lx)", (unsigned long)hr);
    return hr;
}

static X104Event x104_platform_network_status_event = { 0, "NetworkInformation.NetworkStatusChanged" };

static HRESULT X104_API x104_platform_network_add_status_changed(void *self, void *handler, LONGLONG *token)
{
    (void)self;
    x104_logf("X104 platform: NetworkInformation.add_NetworkStatusChanged(%p) stored (never raised: the host network state does not change)", handler);
    return x104_event_add(&x104_platform_network_status_event, handler, token);
}

static HRESULT X104_API x104_platform_network_remove_status_changed(void *self, LONGLONG token)
{
    (void)self;
    return x104_event_remove(&x104_platform_network_status_event, token);
}

/* an EMPTY IVectorView (parameterised: no IID of its own is needed, the game calls the slots on the typed out pointer) */
static HRESULT X104_API x104_platform_emptyvec_get_at(void *self, UINT32 index, void **item)
{
    (void)self;
    (void)index;
    if (item) *item = NULL;
    return X104_E_BOUNDS;
}

static HRESULT X104_API x104_platform_emptyvec_get_size(void *self, UINT32 *size)
{
    (void)self;
    if (!size) return E_POINTER;
    *size = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_emptyvec_index_of(void *self, void *item, UINT32 *index, BYTE *found)
{
    (void)self;
    (void)item;
    if (index) *index = 0;
    if (found) *found = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_emptyvec_get_many(void *self, UINT32 start, UINT32 capacity, void **items, UINT32 *actual)
{
    (void)self;
    (void)start;
    (void)capacity;
    (void)items;
    if (actual) *actual = 0;
    return S_OK;
}

X104_VTBL(x104_platform_vt_emptyvec, IVectorView) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IVectorView, GetAt, x104_platform_emptyvec_get_at),
    X104_SLOT(IVectorView, get_Size, x104_platform_emptyvec_get_size),
    X104_SLOT(IVectorView, IndexOf, x104_platform_emptyvec_index_of),
    X104_SLOT(IVectorView, GetMany, x104_platform_emptyvec_get_many)
};
static const X104IfaceDesc x104_platform_emptyvec_ifaces[] = { X104_IFACE_IID(&X104_IID_IInspectable, "IVectorView(empty)", x104_platform_vt_emptyvec) };
X104_CLASS_DEF(x104_platform_emptyvec_class, "EmptyVectorView", L"Windows.Foundation.Collections.IVectorView`1", x104_platform_emptyvec_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_emptyvec_object, x104_platform_emptyvec_class, X104Object)

static HRESULT X104_API x104_platform_network_get_profiles(void *self, void **profiles)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: NetworkInformation.GetConnectionProfiles -> empty vector (no adapter GUID for the game's device id)");
    return x104_return_iface(x104_platform_emptyvec_object(), &X104_IID_IInspectable, profiles, "NetworkInformation.GetConnectionProfiles");
}

X104_VTBL(x104_platform_vt_network_statics, INetworkInformationStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(INetworkInformationStatics, GetConnectionProfiles, x104_platform_network_get_profiles),
    X104_SLOT(INetworkInformationStatics, GetInternetConnectionProfile, x104_platform_network_get_profile),
    X104_SLOT(INetworkInformationStatics, add_NetworkStatusChanged, x104_platform_network_add_status_changed),
    X104_SLOT(INetworkInformationStatics, remove_NetworkStatusChanged, x104_platform_network_remove_status_changed)
};
static const X104IfaceDesc x104_platform_network_statics_ifaces[] = { X104_IFACE(INetworkInformationStatics, x104_platform_vt_network_statics) };
X104_CLASS_DEF(x104_platform_network_statics_class, "NetworkInformationStatics", L"Windows.Networking.Connectivity.NetworkInformation", x104_platform_network_statics_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_network_statics, x104_platform_network_statics_class, X104Object)

/* ======================================================================================================================
 * 11. CurrentAppSimulator / CurrentApp (+ LicenseInformation).  The AppPlatform "buy" cell is patched, so none of this runs on the boot path.
 * ====================================================================================================================== */
/* ProductLicense: IsActive FALSE for every product (policy: the Windows Phone Store is closed, purchases cannot happen, nothing is reported as owned) */
static HRESULT X104_API x104_platform_product_get_id(void *self, HSTRING *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    return S_OK;
}

static HRESULT X104_API x104_platform_product_get_is_active(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_product_get_expiration(void *self, LONGLONG *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

X104_VTBL(x104_platform_vt_product, IProductLicense) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IProductLicense, get_ProductId, x104_platform_product_get_id),
    X104_SLOT(IProductLicense, get_IsActive, x104_platform_product_get_is_active),
    X104_SLOT(IProductLicense, get_ExpirationDate, x104_platform_product_get_expiration)
};
static const X104IfaceDesc x104_platform_product_ifaces[] = { X104_IFACE(IProductLicense, x104_platform_vt_product) };
X104_CLASS_DEF(x104_platform_product_class, "ProductLicense", L"Windows.ApplicationModel.Store.ProductLicense", x104_platform_product_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_product_object, x104_platform_product_class, X104Object)

/* IMapView<String, ProductLicense>: Lookup(any id) -> the not-active license, HasKey TRUE, Size 0 */
static HRESULT X104_API x104_platform_licmap_lookup(void *self, HSTRING key, void **value)
{
    static volatile LONG logged;
    char shown[96];
    (void)self;
    x104_hstring_ascii(key, shown, sizeof(shown));
    X104_LOG_LIMITED(&logged, 20, "X104 platform: ProductLicenses.Lookup(\"%s\") -> not active (the Store is closed: nothing is owned)", shown);
    return x104_return_iface(x104_platform_product_object(), &X104_IID_IProductLicense, value, "ProductLicenses.Lookup");
}

static HRESULT X104_API x104_platform_licmap_get_size(void *self, UINT32 *size)
{
    (void)self;
    if (!size) return E_POINTER;
    *size = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_licmap_has_key(void *self, HSTRING key, BYTE *found)
{
    (void)self;
    (void)key;
    if (!found) return E_POINTER;
    *found = 1;
    return S_OK;
}

static HRESULT X104_API x104_platform_licmap_split(void *self, void **first, void **second)
{
    (void)self;
    if (first) *first = NULL;
    if (second) *second = NULL;
    return S_OK;
}

X104_VTBL(x104_platform_vt_licmap, IMapView) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IMapView, Lookup, x104_platform_licmap_lookup),
    X104_SLOT(IMapView, get_Size, x104_platform_licmap_get_size),
    X104_SLOT(IMapView, HasKey, x104_platform_licmap_has_key),
    X104_SLOT(IMapView, Split, x104_platform_licmap_split)
};
static const X104IfaceDesc x104_platform_licmap_ifaces[] = { X104_IFACE_IID(&X104_IID_IInspectable, "IMapView(ProductLicenses)", x104_platform_vt_licmap) };
X104_CLASS_DEF(x104_platform_licmap_class, "ProductLicenses", L"Windows.Foundation.Collections.IMapView`2", x104_platform_licmap_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_licmap_object, x104_platform_licmap_class, X104Object)

static HRESULT X104_API x104_platform_license_get_product_licenses(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_platform_licmap_object(), &X104_IID_IInspectable, value, "LicenseInformation.ProductLicenses");
}

static HRESULT X104_API x104_platform_license_get_is_active(void *self, BYTE *value)
{
    static volatile LONG logged;
    (void)self;
    if (!value) return E_POINTER;
    *value = 1;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: LicenseInformation.get_IsActive -> TRUE");
    return S_OK;
}

static HRESULT X104_API x104_platform_license_get_is_trial(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

X104_VTBL(x104_platform_vt_license, ILicenseInformation) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ILicenseInformation, get_ProductLicenses, x104_platform_license_get_product_licenses),
    X104_SLOT(ILicenseInformation, get_IsActive, x104_platform_license_get_is_active),
    X104_SLOT(ILicenseInformation, get_IsTrial, x104_platform_license_get_is_trial)
};
static const X104IfaceDesc x104_platform_license_ifaces[] = { X104_IFACE(ILicenseInformation, x104_platform_vt_license) };
X104_CLASS_DEF(x104_platform_license_class, "LicenseInformation", L"Windows.ApplicationModel.Store.LicenseInformation", x104_platform_license_ifaces, X104_CLASS_AGILE, NULL);
X104_SINGLETON(x104_platform_license_object, x104_platform_license_class, X104Object)

static HRESULT X104_API x104_platform_store_get_license(void *self, void **value)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: CurrentApp(Simulator).get_LicenseInformation -> fake license (IsActive TRUE, IsTrial FALSE)");
    return x104_return_iface(x104_platform_license_object(), &X104_IID_ILicenseInformation, value, "CurrentApp.LicenseInformation");
}

static HRESULT X104_API x104_platform_store_request_app_purchase(void *self, BYTE include_receipt, void **operation)
{
    (void)self;
    (void)include_receipt;
    if (operation) *operation = NULL;
    x104_logf("X104 platform: CurrentApp(Simulator).RequestAppPurchaseAsync - the Store is not available on this host -> E_NOTIMPL");
    return X104_E_NOT_IMPL;
}

static HRESULT X104_API x104_platform_store_request_product_purchase(void *self, HSTRING product, BYTE include_receipt, void **operation)
{
    char shown[96];
    (void)self;
    (void)include_receipt;
    if (operation) *operation = NULL;
    x104_hstring_ascii(product, shown, sizeof(shown));
    x104_logf("X104 platform: CurrentApp(Simulator).RequestProductPurchaseAsync(\"%s\") - the Store is not available on this host -> E_NOTIMPL", shown);
    return X104_E_NOT_IMPL;
}

static HRESULT X104_API x104_platform_store_get_product_receipt(void *self, HSTRING product, void **operation)
{
    char shown[96];
    (void)self;
    if (operation) *operation = NULL;
    x104_hstring_ascii(product, shown, sizeof(shown));
    x104_logf("X104 platform: CurrentApp(Simulator).GetProductReceiptAsync(\"%s\") - the Store is not available on this host -> E_NOTIMPL", shown);
    return X104_E_NOT_IMPL;
}

X104_VTBL(x104_platform_vt_simulator, ICurrentAppSimulator) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICurrentAppSimulator, get_LicenseInformation, x104_platform_store_get_license),
    X104_SLOT(ICurrentAppSimulator, RequestAppPurchaseAsync, x104_platform_store_request_app_purchase),
    X104_SLOT(ICurrentAppSimulator, RequestProductPurchaseAsync, x104_platform_store_request_product_purchase),
    X104_SLOT(ICurrentAppSimulator, GetProductReceiptAsync, x104_platform_store_get_product_receipt)
};
X104_VTBL(x104_platform_vt_simulator_consumables, ICurrentAppSimulatorWithConsumables) = {
    X104_INSPECTABLE_SLOTS                      /* holes: the consumables API is only reached through the purchase flow the buy cell patch bypasses */
};
static const X104IfaceDesc x104_platform_simulator_ifaces[] = {
    X104_IFACE(ICurrentAppSimulator, x104_platform_vt_simulator),
    X104_IFACE(ICurrentAppSimulatorWithConsumables, x104_platform_vt_simulator_consumables)
};
X104_CLASS_DEF(x104_platform_simulator_class, "CurrentAppSimulatorStatics", L"Windows.ApplicationModel.Store.CurrentAppSimulator", x104_platform_simulator_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_simulator_statics, x104_platform_simulator_class, X104Object)

static HRESULT X104_API x104_platform_store_load_listing(void *self, void **operation)
{
    (void)self;
    if (operation) *operation = NULL;
    x104_logf("X104 platform: CurrentApp.LoadListingInformationAsync - the Store is not available on this host -> E_NOTIMPL");
    return X104_E_NOT_IMPL;
}

static HRESULT X104_API x104_platform_store_get_app_receipt(void *self, void **operation)
{
    (void)self;
    if (operation) *operation = NULL;
    x104_logf("X104 platform: CurrentApp.GetAppReceiptAsync - the Store is not available on this host -> E_NOTIMPL");
    return X104_E_NOT_IMPL;
}

X104_VTBL(x104_platform_vt_currentapp, ICurrentApp) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(ICurrentApp, LoadListingInformationAsync, x104_platform_store_load_listing),
    X104_SLOT(ICurrentApp, GetAppReceiptAsync, x104_platform_store_get_app_receipt),
    X104_SLOT(ICurrentApp, get_LicenseInformation, x104_platform_store_get_license),
    X104_SLOT(ICurrentApp, RequestAppPurchaseAsync, x104_platform_store_request_app_purchase),
    X104_SLOT(ICurrentApp, RequestProductPurchaseAsync, x104_platform_store_request_product_purchase),
    X104_SLOT(ICurrentApp, GetProductReceiptAsync, x104_platform_store_get_product_receipt)
};
static const X104IfaceDesc x104_platform_currentapp_ifaces[] = { X104_IFACE(ICurrentApp, x104_platform_vt_currentapp) };
X104_CLASS_DEF(x104_platform_currentapp_class, "CurrentAppStatics", L"Windows.ApplicationModel.Store.CurrentApp", x104_platform_currentapp_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_currentapp_statics, x104_platform_currentapp_class, X104Object)

/* ======================================================================================================================
 * 12. AsyncCausalityTracer (IAsyncCausalityTracerStatics): slots 6..10 are S_OK no-ops (the argument lists - a by-value GUID, 64-bit ids - are ignored,
 *     which is correct on AAPCS: the caller pops its own stack arguments); 11/12 store / drop the TracingStatusChanged handlers
 * ====================================================================================================================== */
static X104Event x104_platform_tracer_event = { 0, "AsyncCausalityTracer.TracingStatusChanged" };

static void x104_platform_tracer_note(const char *method)
{
    if (InterlockedIncrement(&x104_platform_tracer_calls) == 1)
        x104_logf("X104 platform: AsyncCausalityTracer first call (%s) from PPL task code - every Trace* call is a no-op that returns S_OK", method);
}

#define X104_PLATFORM_TRACER_NOP(function, method)  \
    static HRESULT X104_API function(void *self) { (void)self; x104_platform_tracer_note(method); return S_OK; }
X104_PLATFORM_TRACER_NOP(x104_platform_tracer_creation, "TraceOperationCreation")
X104_PLATFORM_TRACER_NOP(x104_platform_tracer_completion, "TraceOperationCompletion")
X104_PLATFORM_TRACER_NOP(x104_platform_tracer_relation, "TraceOperationRelation")
X104_PLATFORM_TRACER_NOP(x104_platform_tracer_work_start, "TraceSynchronousWorkStart")
X104_PLATFORM_TRACER_NOP(x104_platform_tracer_work_completion, "TraceSynchronousWorkCompletion")

static HRESULT X104_API x104_platform_tracer_add_status_changed(void *self, void *handler, LONGLONG *token)
{
    HRESULT hr;
    (void)self;
    if (!token) return E_POINTER;
    *token = 0;
    if (!handler) return S_OK;
    hr = x104_event_add(&x104_platform_tracer_event, handler, token);
    if (FAILED(hr)) {
        *token = 0;                                  /* the real tracer never fails here; the event is never raised anyway */
        x104_logf("X104 platform: AsyncCausalityTracer.add_TracingStatusChanged could not store the handler (0x%08lx) - ignored, S_OK", (unsigned long)hr);
    }
    return S_OK;
}

static HRESULT X104_API x104_platform_tracer_remove_status_changed(void *self, LONGLONG token)
{
    (void)self;
    if (token) x104_event_remove(&x104_platform_tracer_event, token);
    return S_OK;
}

X104_VTBL(x104_platform_vt_tracer, IAsyncCausalityTracerStatics) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT_NOARGS(IAsyncCausalityTracerStatics, TraceOperationCreation, x104_platform_tracer_creation),
    X104_SLOT_NOARGS(IAsyncCausalityTracerStatics, TraceOperationCompletion, x104_platform_tracer_completion),
    X104_SLOT_NOARGS(IAsyncCausalityTracerStatics, TraceOperationRelation, x104_platform_tracer_relation),
    X104_SLOT_NOARGS(IAsyncCausalityTracerStatics, TraceSynchronousWorkStart, x104_platform_tracer_work_start),
    X104_SLOT_NOARGS(IAsyncCausalityTracerStatics, TraceSynchronousWorkCompletion, x104_platform_tracer_work_completion),
    X104_SLOT(IAsyncCausalityTracerStatics, add_TracingStatusChanged, x104_platform_tracer_add_status_changed),
    X104_SLOT(IAsyncCausalityTracerStatics, remove_TracingStatusChanged, x104_platform_tracer_remove_status_changed)
};
static const X104IfaceDesc x104_platform_tracer_ifaces[] = { X104_IFACE(IAsyncCausalityTracerStatics, x104_platform_vt_tracer) };
X104_CLASS_DEF(x104_platform_tracer_class, "AsyncCausalityTracerStatics", L"Windows.Foundation.Diagnostics.AsyncCausalityTracer", x104_platform_tracer_ifaces,
               X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_tracer_statics, x104_platform_tracer_class, X104Object)

/* ======================================================================================================================
 * 13. activation table (Windows.Web.Http.* stays with the real runtime: section 3.8 of the brief)
 * ====================================================================================================================== */
static const X104FactoryDef x104_platform_factories[] = {
    { L"Windows.ApplicationModel.Package",                       x104_platform_package_statics },
    { L"Windows.Storage.ApplicationData",                        x104_platform_appdata_statics },
    { L"Windows.System.MemoryManager",                           x104_platform_memory_statics },
    { L"Windows.Phone.Devices.Notification.VibrationDevice",     x104_platform_vibration_statics },
    { L"Windows.Networking.Connectivity.NetworkInformation",     x104_platform_network_statics },
    { L"Windows.ApplicationModel.Store.CurrentAppSimulator",     x104_platform_simulator_statics },
    { L"Windows.ApplicationModel.Store.CurrentApp",              x104_platform_currentapp_statics },
    { L"Windows.Foundation.Diagnostics.AsyncCausalityTracer",    x104_platform_tracer_statics }
};

/* ======================================================================================================================
 * 14. contract functions
 * ====================================================================================================================== */
/* FIRST unit to run (before the window exists): roots, data-cell patches, XAudio2 probe.  FALSE = fatal (boot shows a message box, exit code 31). */
static BOOL x104_platform_install(BYTE *image)
{
    char game_shown[MAX_PATH];
    char data_shown[MAX_PATH];

    x104_logf("X104 platform: install begin (image %p)", (void *)image);
    x104_platform_parse_options();

    if (!x104_platform_check_game_root()) return FALSE;
    if (!x104_platform_choose_data_root()) return FALSE;
    x104_narrow(game_shown, sizeof(game_shown), x104.game_root);
    x104_narrow(data_shown, sizeof(data_shown), x104.data_root);
    x104_logf("X104 platform: game root=%s data root=%s (source %s) writable=1", game_shown, data_shown, x104_platform_state.data_root_source);
    if (x104_feat(X104_FEAT_MCPE_PATCHES))
        x104_logf("X104 platform: the game creates %s\\games\\com.mojang\\minecraftpe (options.txt, clientId.txt, minecraftWorlds) below the data root; assets are read from %s\\data",
                  data_shown, game_shown);
    else
        x104_logf("X104 platform: %s keeps its saves / settings files in %s (ApplicationData.LocalFolder); assets are read from %s", x104_profile->title, data_shown, game_shown);

    x104_platform_memory_decide();
    if (x104_feat(X104_FEAT_MCPE_PATCHES)) x104_platform_patch_cells();
    else x104_logf("X104 platform: no AppPlatform cell patches for %s (its store / network / vibration calls go through the WinRT fakes)", x104_profile->title);
    x104_platform_xaudio2_probe();

    if (x104_feat(X104_FEAT_MCPE_PATCHES))
        x104_logf("X104 platform: Windows.Web.Http.* is NOT faked (decision: dead code - IHttpClient at WinRTRestRequestJob+0xD8 is never assigned); the real runtime answers. "
                  "Visible effect: tapping 'Play on Realms' crashes at WinRTRestRequestJob::vf2 0x10E9D4 (load at 0x10ED56) - known, no patch for M1");
    x104_platform_state.installed = TRUE;
    x104_logf("X104 platform: install done (offline=%d, xaudio2=%s)", (int)x104_platform_state.offline, x104_platform_state.xaudio2_ok ? "ok" : "DISABLED");
    return TRUE;
}

static void x104_platform_shutdown(void)
{
    if (x104_platform_state.shut_down) return;
    x104_platform_state.shut_down = TRUE;
    x104_logf("X104 platform: shutdown (Vibrate calls %ld, Cancel calls %ld, network profile requests %ld, tracer calls %ld, memory queries %ld)",
              (long)x104_platform_vibrate_calls, (long)x104_platform_cancel_calls, (long)x104_platform_profile_calls, (long)x104_platform_tracer_calls,
              (long)x104_platform_memory_calls);
}

/* the 8 classes of the table; Windows.Web.Http.* answers REGDB_E_CLASSNOTREG on purpose (the core then falls back to the real runtime and logs it) */
static HRESULT x104_platform_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    static volatile LONG http_logged;
    if (out) *out = NULL;
    if (!class_name || !iid || !out) return E_POINTER;
    if (x104_platform_starts_with(class_name, L"Windows.Web.Http.")) {
        char shown[96];
        x104_narrow(shown, sizeof(shown), class_name);
        X104_LOG_LIMITED(&http_logged, 8, "X104 platform: %s requested - left to the real runtime on purpose (Realms/MCO REST code; IHttpClient is never assigned by the game)", shown);
        return REGDB_E_CLASSNOTREG;
    }
    return x104_activate_from_table(x104_platform_factories, (unsigned)ARRAYSIZE(x104_platform_factories), class_name, iid, out);
}

#endif /* WIN32_HOST_ARM32_0104_PLATFORM_INL */
