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
    typedef HRESULT (WINAPI *XAudio2CreateFn)(void **engine, ULONG flags, ULONG processor);
    typedef HRESULT (X104_API *CreateMasteringVoiceFn)(void *self, void **voice, ULONG channels, ULONG rate, ULONG flags, const wchar_t *device,
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

/* ---- completed IAsyncOperation<T> for the StorageFolder file APIs (GetFileAsync and friends) ----
 * HCR loads its save from ApplicationData.RoamingFolder\fsusda.dat; on a first run the file is absent, which the platform reports as a faulted
 * operation (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)), exactly like a device would.  The parameterised IAsyncOperation<T> IID is not known
 * up front, so the class initially exposes only IAsyncInfo and the first QI the game makes is logged by the generic QI-miss reporter. */
typedef struct X104PlatformFileOp {
    X104Object base;
    HRESULT error;                  /* S_OK = completed successfully, otherwise the faulted operation's error code */
    void *result;                   /* IStorageFile view returned by GetResults (AddRef'd), may be NULL */
    unsigned long id;
    void *handler;                  /* AsyncOperationCompletedHandler stored by put_Completed (AddRef'd) */
} X104PlatformFileOp;

static void x104_platform_fileop_invoke(void *argument)
{
    X104PlatformFileOp *op = argument;
    void *handler = op->handler;
    int status = SUCCEEDED(op->error) ? X104_ASYNC_COMPLETED : X104_ASYNC_ERROR;
    HRESULT hr = S_OK;
    X104Seh seh;
    if (handler && x104_call_guarded3(((void **)*(void ***)handler)[3], handler, x104_object_iface0(&op->base), (void *)(ULONG_PTR)status, &hr, &seh))
    {
        if (FAILED(hr)) x104_logf("X104 platform: file operation %p: Completed handler %p returned 0x%08lx", op, handler, (unsigned long)hr);
    }
    else
    {
        x104_logf("X104 platform: EXCEPTION escaped from a Completed handler of file operation %p (handler %p)", op, handler);
    }
    if (handler) x104_raw_release(handler);
    x104_raw_release(x104_object_iface0(&op->base));    /* the reference taken for the posted callback */
}

static HRESULT X104_API x104_platform_fileop_put_completed(void *self, void *handler)
{
    X104PlatformFileOp *op = X104_THIS(X104PlatformFileOp, self);
    int status = SUCCEEDED(op->error) ? X104_ASYNC_COMPLETED : X104_ASYNC_ERROR;
    if (!handler) { if (op->handler) { x104_raw_release(op->handler); op->handler = NULL; } return S_OK; }
    if (op->handler) x104_raw_release(op->handler);
    op->handler = handler;
    x104_raw_addref(handler);
    x104_raw_addref(x104_object_iface0(&op->base));
    if (!x104_ui_post(x104_platform_fileop_invoke, op)) {
        x104_raw_release(handler);
        x104_raw_release(x104_object_iface0(&op->base));
        op->handler = NULL;
        x104_logf("X104 platform: file operation %p: the UI queue is closed - Completed handler NOT invoked", op);
    }
    x104_logf("X104 platform: file operation %p: put_Completed(handler %p), status will be %d", op, handler, status);
    return S_OK;
}

static HRESULT X104_API x104_platform_fileop_get_completed(void *self, void **handler)
{
    X104PlatformFileOp *op = X104_THIS(X104PlatformFileOp, self);
    if (!handler) return E_POINTER;
    *handler = op->handler;
    if (op->handler) x104_raw_addref(op->handler);
    return S_OK;
}

static HRESULT X104_API x104_platform_fileop_get_results(void *self, void **results)
{
    X104PlatformFileOp *op = X104_THIS(X104PlatformFileOp, self);
    if (!results) return E_POINTER;
    *results = NULL;
    if (FAILED(op->error)) return op->error;
    if (op->result) {
        x104_raw_addref(op->result);
        *results = op->result;
    }
    return S_OK;
}

static HRESULT X104_API x104_platform_fileop_info_get_id(void *self, ULONG *id)
{
    X104PlatformFileOp *op = X104_THIS(X104PlatformFileOp, self);
    if (!id) return E_POINTER;
    *id = (ULONG)op->id;
    return S_OK;
}

static HRESULT X104_API x104_platform_fileop_info_get_status(void *self, int *status)
{
    X104PlatformFileOp *op = X104_THIS(X104PlatformFileOp, self);
    if (!status) return E_POINTER;
    *status = SUCCEEDED(op->error) ? X104_ASYNC_COMPLETED : X104_ASYNC_ERROR;
    return S_OK;
}

static HRESULT X104_API x104_platform_fileop_info_get_error(void *self, HRESULT *error)
{
    X104PlatformFileOp *op = X104_THIS(X104PlatformFileOp, self);
    if (!error) return E_POINTER;
    *error = op->error;
    return S_OK;
}

static HRESULT X104_API x104_platform_fileop_info_cancel(void *self) { (void)self; return S_OK; }
static HRESULT X104_API x104_platform_fileop_info_close(void *self) { (void)self; return S_OK; }

X104_VTBL_N(x104_platform_vt_fileop, 9) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_fileop_put_completed,
    [7] = (const void *)x104_platform_fileop_get_completed,
    [8] = (const void *)x104_platform_fileop_get_results
};
X104_VTBL(x104_platform_vt_fileop_info, IAsyncInfo) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IAsyncInfo, get_Id, x104_platform_fileop_info_get_id),
    X104_SLOT(IAsyncInfo, get_Status, x104_platform_fileop_info_get_status),
    X104_SLOT(IAsyncInfo, get_ErrorCode, x104_platform_fileop_info_get_error),
    X104_SLOT(IAsyncInfo, Cancel, x104_platform_fileop_info_cancel),
    X104_SLOT(IAsyncInfo, Close, x104_platform_fileop_info_close)
};
/* the parameterised IAsyncOperation<T> IID is not recovered from the game: HCR uses the pointer GetFileAsync returns directly and never QIs
 * it, so the host publishes its own IID for that view (a real QI for the generated IID would show up as a QI MISS line) */
static const GUID x104_platform_iid_asyncop_storagefile = {0x9a2b7d10,0x3c11,0x5b48,{0x8d,0x2e,0x6f,0x41,0x51,0x77,0x0c,0x19}};
static const X104IfaceDesc x104_platform_fileop_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_asyncop_storagefile, "IAsyncOperation<StorageFile>", x104_platform_vt_fileop),
    X104_IFACE(IAsyncInfo, x104_platform_vt_fileop_info)
};
X104_CLASS_DEF(x104_platform_fileop_class, "StorageFileOperation", L"Windows.Foundation.IAsyncOperation`1<Windows.Storage.StorageFile>",
               x104_platform_fileop_ifaces, X104_CLASS_AGILE, NULL);

/* ======================================================================================================================
 * StorageFile + read stream.  HCR loads <RoamingFolder>\fsusda.data through
 *     IStorageFolder.GetFileAsync -> StorageFile -> QI IRandomAccessStreamReference -> OpenReadAsync -> IRandomAccessStreamWithContentType
 * The bytes are the ones in the host's data root; the stream is an in-memory copy of the file, so the WinRT surface stays small.
 * ====================================================================================================================== */
static const GUID x104_platform_iid_istoragefile = {0xfa3f6186,0x4214,0x428c,{0xa6,0x4c,0x14,0xc9,0xac,0x73,0x15,0xea}};
static const GUID x104_platform_iid_istreamref = {0x33ee3134,0x1dd6,0x4e3a,{0x80,0x67,0xd1,0xc1,0x62,0xe8,0x64,0x2b}};
static const GUID x104_platform_iid_iseqstreamref = {0x43929d18,0x5ec9,0x4b5a,{0x91,0x9c,0x42,0x05,0xb0,0xc8,0x04,0xb6}};
static const GUID x104_platform_iid_irastream = {0x905a0fe1,0xbc53,0x11df,{0x8c,0x49,0x00,0x1e,0x4f,0xc6,0x86,0xda}};
static const GUID x104_platform_iid_iinputstream = {0x905a0fe2,0xbc53,0x11df,{0x8c,0x49,0x00,0x1e,0x4f,0xc6,0x86,0xda}};
static const GUID x104_platform_iid_ioutputstream = {0x905a0fe6,0xbc53,0x11df,{0x8c,0x49,0x00,0x1e,0x4f,0xc6,0x86,0xda}};
static const GUID x104_platform_iid_irastream_content = {0xcc254827,0x4b3d,0x438f,{0x92,0x32,0x10,0xc7,0x6b,0xc7,0xe0,0x38}};
static const GUID x104_platform_iid_iclosable = {0x30d5a829,0x7fa4,0x4026,{0x83,0xbb,0xd7,0x5b,0xae,0x4e,0xa9,0x9e}};
static const GUID x104_platform_iid_icontenttype = {0x97d098a5,0x3b99,0x4de9,{0x88,0xa5,0xe1,0x1d,0x2f,0x50,0xc7,0x95}};
static const GUID x104_platform_iid_ibufferbyteaccess = {0x905a0fef,0xbc53,0x11df,{0x8c,0x49,0x00,0x1e,0x4f,0xc6,0x86,0xda}};
/* IAsyncOperationWithProgress<IBuffer, UINT32> is parameterised: the host publishes its own IID (HCR uses the pointer ReadAsync returns
 * directly and never QIs it - a real QI would show up as a QI MISS line) */
static const GUID x104_platform_iid_asyncop_progress_buffer = {0x5b8a3e21,0x0c74,0x5f19,{0xb2,0x66,0x8a,0x31,0x74,0x1f,0x0d,0x52}};

/* ---- IBuffer access: the game allocates the buffer, the host fills it (IBufferByteAccess is the only way to reach the bytes) ---- */
static HRESULT x104_platform_buffer_bytes(void *buffer, BYTE **bytes)
{
    void *access = NULL;
    HRESULT hr;
    if (!buffer) return E_POINTER;
    hr = ((HRESULT (X104_API *)(void *, const GUID *, void **))((void **)*(void ***)buffer)[0])(buffer, &x104_platform_iid_ibufferbyteaccess, &access);
    if (FAILED(hr) || !access) return FAILED(hr) ? hr : (HRESULT)0x80004002u /* E_NOINTERFACE */;
    hr = ((HRESULT (X104_API *)(void *, BYTE **))((void **)*(void ***)access)[3])(access, bytes);
    ((ULONG (X104_API *)(void *))((void **)*(void ***)access)[2])(access);
    return hr;
}

static HRESULT x104_platform_buffer_set_length(void *buffer, UINT32 length)
{
    if (!buffer) return E_POINTER;
    return ((HRESULT (X104_API *)(void *, UINT32))((void **)*(void ***)buffer)[8])(buffer, length);
}

/* IBuffer.get_Length is slot 7 (0..5 IInspectable, 6 get_Capacity, 7 get_Length) */
static HRESULT x104_platform_buffer_get_length(void *buffer, UINT32 *length)
{
    if (!buffer || !length) return E_POINTER;
    return ((HRESULT (X104_API *)(void *, UINT32 *))((void **)*(void ***)buffer)[7])(buffer, length);
}

/* ---- the already-completed IAsyncOperationWithProgress<IBuffer, UINT32> ReadAsync returns ---- */
typedef struct X104PlatformReadOp {
    X104Object base;
    void *buffer;                   /* AddRef'd */
    unsigned long id;
    void *handler;
} X104PlatformReadOp;

static void x104_platform_readop_invoke(void *argument)
{
    X104PlatformReadOp *op = argument;
    void *handler = op->handler;
    HRESULT hr = S_OK;
    X104Seh seh;
    if (handler && x104_call_guarded3(((void **)*(void ***)handler)[3], handler, x104_object_iface0(&op->base),
                                      (void *)(ULONG_PTR)X104_ASYNC_COMPLETED, &hr, &seh)) {
        if (FAILED(hr)) x104_logf("X104 platform: read operation %p: Completed handler %p returned 0x%08lx", op, handler, (unsigned long)hr);
    } else if (handler) {
        x104_logf("X104 platform: EXCEPTION escaped from a Completed handler of read operation %p", op);
    }
    if (handler) x104_raw_release(handler);
    x104_raw_release(x104_object_iface0(&op->base));
}

static HRESULT X104_API x104_platform_readop_put_completed(void *self, void *handler)
{
    X104PlatformReadOp *op = X104_THIS(X104PlatformReadOp, self);
    if (!handler) { if (op->handler) { x104_raw_release(op->handler); op->handler = NULL; } return S_OK; }
    if (op->handler) x104_raw_release(op->handler);
    op->handler = handler;
    x104_raw_addref(handler);
    x104_raw_addref(x104_object_iface0(&op->base));
    if (!x104_ui_post(x104_platform_readop_invoke, op)) {
        x104_raw_release(handler);
        x104_raw_release(x104_object_iface0(&op->base));
        op->handler = NULL;
        x104_logf("X104 platform: read operation %p: the UI queue is closed - Completed handler NOT invoked", op);
    }
    return S_OK;
}

static HRESULT X104_API x104_platform_readop_get_completed(void *self, void **handler)
{
    X104PlatformReadOp *op = X104_THIS(X104PlatformReadOp, self);
    if (!handler) return E_POINTER;
    *handler = op->handler;
    if (op->handler) x104_raw_addref(op->handler);
    return S_OK;
}

static HRESULT X104_API x104_platform_readop_get_results(void *self, void **results)
{
    X104PlatformReadOp *op = X104_THIS(X104PlatformReadOp, self);
    if (!results) return E_POINTER;
    *results = op->buffer;
    if (op->buffer) x104_raw_addref(op->buffer);
    return S_OK;
}

static HRESULT X104_API x104_platform_readop_noop_progress(void *self, void *handler) { (void)self; (void)handler; return S_OK; }
static HRESULT X104_API x104_platform_readop_get_progress(void *self, void **handler) { (void)self; if (handler) *handler = NULL; return S_OK; }
static HRESULT X104_API x104_platform_readop_info_get_id(void *self, ULONG *id)
{
    X104PlatformReadOp *op = X104_THIS(X104PlatformReadOp, self);
    if (!id) return E_POINTER;
    *id = (ULONG)op->id;
    return S_OK;
}
static HRESULT X104_API x104_platform_readop_info_get_status(void *self, int *status) { (void)self; if (!status) return E_POINTER; *status = X104_ASYNC_COMPLETED; return S_OK; }
static HRESULT X104_API x104_platform_readop_info_get_error(void *self, HRESULT *error) { (void)self; if (!error) return E_POINTER; *error = S_OK; return S_OK; }
static HRESULT X104_API x104_platform_readop_info_noop(void *self) { (void)self; return S_OK; }

X104_VTBL_N(x104_platform_vt_readop, 11) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_readop_noop_progress,     /* put_Progress */
    [7] = (const void *)x104_platform_readop_get_progress,      /* get_Progress */
    [8] = (const void *)x104_platform_readop_put_completed,
    [9] = (const void *)x104_platform_readop_get_completed,
    [10] = (const void *)x104_platform_readop_get_results
};
X104_VTBL(x104_platform_vt_readop_info, IAsyncInfo) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IAsyncInfo, get_Id, x104_platform_readop_info_get_id),
    X104_SLOT(IAsyncInfo, get_Status, x104_platform_readop_info_get_status),
    X104_SLOT(IAsyncInfo, get_ErrorCode, x104_platform_readop_info_get_error),
    X104_SLOT(IAsyncInfo, Cancel, x104_platform_readop_info_noop),
    X104_SLOT(IAsyncInfo, Close, x104_platform_readop_info_noop)
};
static const X104IfaceDesc x104_platform_readop_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_asyncop_progress_buffer, "IAsyncOperationWithProgress<IBuffer,UInt32>", x104_platform_vt_readop),
    X104_IFACE(IAsyncInfo, x104_platform_vt_readop_info)
};
X104_CLASS_DEF(x104_platform_readop_class, "StreamReadOperation", L"Windows.Foundation.IAsyncOperationWithProgress`2<Windows.Storage.Streams.IBuffer,UINT32>",
               x104_platform_readop_ifaces, X104_CLASS_AGILE, NULL);

/* ---- already-completed operations the write path hands back ----
 * IOutputStream.WriteAsync returns IAsyncOperationWithProgress<UINT32,UINT32>, IOutputStream.FlushAsync and
 * ICompressor.FinishAsync return IAsyncOperation<Boolean>, and DataWriter.StoreAsync returns an IAsyncOperation<UINT32>
 * (the DataWriterStoreOperation class).  One object carries a single UINT32 result and the status; the three classes differ
 * only in the vtable and the GetResults signature.  The parameterised IAsyncOperation IIDs are not in the facts, so the host
 * publishes private ones (the game uses the returned pointer directly, a real QI would show up as a QI MISS line). */
static const GUID x104_platform_iid_asyncop_progress_uint32 = {0x5b8a3e31,0x0c74,0x5f19,{0xb2,0x66,0x8a,0x31,0x74,0x1f,0x0d,0x52}};
static const GUID x104_platform_iid_asyncop_bool = {0x5b8a3e32,0x0c74,0x5f19,{0xb2,0x66,0x8a,0x31,0x74,0x1f,0x0d,0x52}};
static const GUID x104_platform_iid_asyncop_uint32 = {0x5b8a3e33,0x0c74,0x5f19,{0xb2,0x66,0x8a,0x31,0x74,0x1f,0x0d,0x52}};

typedef struct X104PlatformDoneOp {
    X104Object base;
    UINT32 value;                   /* GetResults */
    HRESULT error;
    void *handler;                  /* AsyncOperationCompletedHandler / ...WithProgress (AddRef'd) */
    unsigned long id;
} X104PlatformDoneOp;

static void x104_platform_doneop_invoke(void *argument)
{
    X104PlatformDoneOp *op = argument;
    void *handler = op->handler;
    int status = SUCCEEDED(op->error) ? X104_ASYNC_COMPLETED : X104_ASYNC_ERROR;
    HRESULT hr = S_OK;
    X104Seh seh;
    if (handler && x104_call_guarded3(((void **)*(void ***)handler)[3], handler, x104_object_iface0(&op->base), (void *)(ULONG_PTR)status, &hr, &seh)) {
        if (FAILED(hr)) x104_logf("X104 platform: completed operation %p: Completed handler %p returned 0x%08lx", op, handler, (unsigned long)hr);
    } else if (handler) {
        x104_logf("X104 platform: EXCEPTION escaped from a Completed handler of completed operation %p", op);
    }
    if (handler) x104_raw_release(handler);
    x104_raw_release(x104_object_iface0(&op->base));
}

static HRESULT X104_API x104_platform_doneop_put_completed(void *self, void *handler)
{
    X104PlatformDoneOp *op = X104_THIS(X104PlatformDoneOp, self);
    int status = SUCCEEDED(op->error) ? X104_ASYNC_COMPLETED : X104_ASYNC_ERROR;
    if (!handler) { if (op->handler) { x104_raw_release(op->handler); op->handler = NULL; } return S_OK; }
    if (op->handler) x104_raw_release(op->handler);
    op->handler = handler;
    x104_raw_addref(handler);
    x104_raw_addref(x104_object_iface0(&op->base));
    if (!x104_ui_post(x104_platform_doneop_invoke, op)) {
        x104_raw_release(handler);
        x104_raw_release(x104_object_iface0(&op->base));
        op->handler = NULL;
        x104_logf("X104 platform: completed operation %p: the UI queue is closed - Completed handler NOT invoked", op);
    }
    x104_logf("X104 platform: completed operation %p: put_Completed(handler %p), status will be %d", op, handler, status);
    return S_OK;
}

static HRESULT X104_API x104_platform_doneop_get_completed(void *self, void **handler)
{
    X104PlatformDoneOp *op = X104_THIS(X104PlatformDoneOp, self);
    if (!handler) return E_POINTER;
    *handler = op->handler;
    if (op->handler) x104_raw_addref(op->handler);
    return S_OK;
}

static HRESULT X104_API x104_platform_doneop_get_results32(void *self, UINT32 *value)
{
    X104PlatformDoneOp *op = X104_THIS(X104PlatformDoneOp, self);
    if (!value) return E_POINTER;
    if (FAILED(op->error)) return op->error;
    *value = op->value;
    return S_OK;
}

static HRESULT X104_API x104_platform_doneop_get_results_bool(void *self, BYTE *value)
{
    X104PlatformDoneOp *op = X104_THIS(X104PlatformDoneOp, self);
    if (!value) return E_POINTER;
    if (FAILED(op->error)) return op->error;
    *value = op->value ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_doneop_noop_progress(void *self, void *handler) { (void)self; (void)handler; return S_OK; }
static HRESULT X104_API x104_platform_doneop_get_progress(void *self, void **handler) { (void)self; if (handler) *handler = NULL; return S_OK; }
static HRESULT X104_API x104_platform_doneop_get_id(void *self, ULONG *id)
{
    X104PlatformDoneOp *op = X104_THIS(X104PlatformDoneOp, self);
    if (!id) return E_POINTER;
    *id = (ULONG)op->id;
    return S_OK;
}
static HRESULT X104_API x104_platform_doneop_get_status(void *self, int *status)
{
    X104PlatformDoneOp *op = X104_THIS(X104PlatformDoneOp, self);
    if (!status) return E_POINTER;
    *status = SUCCEEDED(op->error) ? X104_ASYNC_COMPLETED : X104_ASYNC_ERROR;
    return S_OK;
}
static HRESULT X104_API x104_platform_doneop_get_error(void *self, HRESULT *error)
{
    X104PlatformDoneOp *op = X104_THIS(X104PlatformDoneOp, self);
    if (!error) return E_POINTER;
    *error = op->error;
    return S_OK;
}
static HRESULT X104_API x104_platform_doneop_info_noop(void *self) { (void)self; return S_OK; }

X104_VTBL_N(x104_platform_vt_op32_progress, 11) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_doneop_noop_progress,
    [7] = (const void *)x104_platform_doneop_get_progress,
    [8] = (const void *)x104_platform_doneop_put_completed,
    [9] = (const void *)x104_platform_doneop_get_completed,
    [10] = (const void *)x104_platform_doneop_get_results32
};
X104_VTBL_N(x104_platform_vt_opbool, 9) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_doneop_put_completed,
    [7] = (const void *)x104_platform_doneop_get_completed,
    [8] = (const void *)x104_platform_doneop_get_results_bool
};
X104_VTBL_N(x104_platform_vt_op32, 9) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_doneop_put_completed,
    [7] = (const void *)x104_platform_doneop_get_completed,
    [8] = (const void *)x104_platform_doneop_get_results32
};
X104_VTBL(x104_platform_vt_doneop_info, IAsyncInfo) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IAsyncInfo, get_Id, x104_platform_doneop_get_id),
    X104_SLOT(IAsyncInfo, get_Status, x104_platform_doneop_get_status),
    X104_SLOT(IAsyncInfo, get_ErrorCode, x104_platform_doneop_get_error),
    X104_SLOT(IAsyncInfo, Cancel, x104_platform_doneop_info_noop),
    X104_SLOT(IAsyncInfo, Close, x104_platform_doneop_info_noop)
};
static const X104IfaceDesc x104_platform_op32_progress_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_asyncop_progress_uint32, "IAsyncOperationWithProgress<UInt32,UInt32>", x104_platform_vt_op32_progress),
    X104_IFACE(IAsyncInfo, x104_platform_vt_doneop_info)
};
static const X104IfaceDesc x104_platform_opbool_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_asyncop_bool, "IAsyncOperation<Boolean>", x104_platform_vt_opbool),
    X104_IFACE(IAsyncInfo, x104_platform_vt_doneop_info)
};
static const X104IfaceDesc x104_platform_op32_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_asyncop_uint32, "IAsyncOperation<UInt32>", x104_platform_vt_op32),
    X104_IFACE(IAsyncInfo, x104_platform_vt_doneop_info)
};
X104_CLASS_DEF(x104_platform_op32_progress_class, "CompletedOperationUInt32Progress",
               L"Windows.Foundation.IAsyncOperationWithProgress`2<UINT32,UINT32>", x104_platform_op32_progress_ifaces, X104_CLASS_AGILE, NULL);
X104_CLASS_DEF(x104_platform_opbool_class, "CompletedOperationBoolean", L"Windows.Foundation.IAsyncOperation`1<Boolean>",
               x104_platform_opbool_ifaces, X104_CLASS_AGILE, NULL);
X104_CLASS_DEF(x104_platform_op32_class, "CompletedOperationUInt32", L"Windows.Foundation.IAsyncOperation`1<UINT32>",
               x104_platform_op32_ifaces, X104_CLASS_AGILE, NULL);

static X104PlatformDoneOp *x104_platform_doneop_new(const X104Class *cls, UINT32 value, HRESULT error)
{
    static volatile LONG serial;
    X104PlatformDoneOp *op = (X104PlatformDoneOp *)x104_object_new(cls, sizeof(*op));
    if (!op) return NULL;
    op->value = value;
    op->error = error;
    op->id = (unsigned long)InterlockedIncrement(&serial);
    return op;
}

static void *x104_platform_op32_progress_new(UINT32 value)
{
    X104PlatformDoneOp *op = x104_platform_doneop_new(&x104_platform_op32_progress_class, value, S_OK);
    return op ? x104_object_iface0(&op->base) : NULL;
}

static void *x104_platform_opbool_new(BYTE value)
{
    X104PlatformDoneOp *op = x104_platform_doneop_new(&x104_platform_opbool_class, value ? 1 : 0, S_OK);
    return op ? x104_object_iface0(&op->base) : NULL;
}

static void *x104_platform_op32_new(UINT32 value)
{
    X104PlatformDoneOp *op = x104_platform_doneop_new(&x104_platform_op32_class, value, S_OK);
    return op ? x104_object_iface0(&op->base) : NULL;
}

/* ---- the stream object: an in-memory copy of the file behind IRandomAccessStreamWithContentType ---- */
typedef struct X104PlatformStream {
    X104Object base;
    BYTE *data;
    UINT32 size;
    UINT32 capacity;
    UINT64 position;
    BOOL dirty;                     /* written to since the last Flush/Close */
    volatile LONG writes;           /* WriteAsync log limiter */
    wchar_t path[MAX_PATH];
} X104PlatformStream;

static void x104_platform_stream_read_file(X104PlatformStream *stream);
static X104PlatformStream *x104_platform_stream_new(const wchar_t *path, int load);
static X104PlatformStream *x104_platform_stream_clone(X104PlatformStream *source, UINT64 position);
static HRESULT x104_platform_stream_write_bytes(X104PlatformStream *stream, const BYTE *bytes, UINT32 length);
static void x104_platform_stream_save_file(X104PlatformStream *stream);

static HRESULT X104_API x104_platform_stream_get_size(void *self, UINT64 *value)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    if (!value) return E_POINTER;
    *value = stream->size;
    return S_OK;
}

/* the interface pointer of `obj` that implements `iid` (identity view when it is missing): GetInputStreamAt has to hand out the
 * IInputStream view, not view 0 - view 0 of the stream is IRandomAccessStreamWithContentType, whose vtable has no methods, so a
 * caller that gets it and calls slot 6 would land in the filler (E_NOTIMPL) */
static void *x104_platform_object_view(X104Object *object, const GUID *iid)
{
    X104View *view = x104_object_find_view(object, iid);
    return view ? (void *)view : x104_object_iface0(object);
}

static HRESULT X104_API x104_platform_stream_get_input_at(void *self, UINT64 position, void **value)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    X104PlatformStream *copy;
    static volatile LONG logged;
    if (!value) return E_POINTER;
    *value = NULL;
    copy = x104_platform_stream_clone(stream, position);
    if (!copy) return E_OUTOFMEMORY;
    *value = x104_platform_object_view(&copy->base, &x104_platform_iid_iinputstream);
    X104_LOG_LIMITED(&logged, 4, "X104 platform: stream.GetInputStreamAt(%lu) -> a copy of the stream (IInputStream view %p)", (unsigned long)position, *value);
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_get_output_at(void *self, UINT64 position, void **value)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    X104PlatformStream *copy;
    static volatile LONG logged;
    if (!value) return E_POINTER;
    *value = NULL;
    copy = x104_platform_stream_clone(stream, position);
    if (!copy) return E_OUTOFMEMORY;
    *value = x104_platform_object_view(&copy->base, &x104_platform_iid_ioutputstream);
    X104_LOG_LIMITED(&logged, 4, "X104 platform: stream.GetOutputStreamAt(%lu) -> a copy of the stream (IOutputStream view %p)", (unsigned long)position, *value);
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_get_position(void *self, UINT64 *value)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    if (!value) return E_POINTER;
    *value = stream->position;
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_seek(void *self, UINT64 position)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    stream->position = position;
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_clone_stream(void *self, void **value)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    X104PlatformStream *copy;
    if (!value) return E_POINTER;
    *value = NULL;
    copy = x104_platform_stream_clone(stream, stream->position);
    if (!copy) return E_OUTOFMEMORY;
    *value = x104_object_iface0(&copy->base);
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_get_can_read(void *self, BYTE *value) { (void)self; if (!value) return E_POINTER; *value = 1; return S_OK; }
static HRESULT X104_API x104_platform_stream_get_can_write(void *self, BYTE *value) { (void)self; if (!value) return E_POINTER; *value = 1; return S_OK; }

/* Grow a heap buffer that may not exist yet.  HeapReAlloc() is not HeapAlloc(): its contract (and Wine's RtlReAllocateHeap)
 * answers NULL for a NULL pointer, so the first growth of every buffer here has to start with HeapAlloc. */
static BYTE *x104_platform_grow(BYTE *data, UINT32 size)
{
    HANDLE heap = GetProcessHeap();
    return data ? (BYTE *)HeapReAlloc(heap, 0, data, size) : (BYTE *)HeapAlloc(heap, 0, size);
}

static HRESULT X104_API x104_platform_stream_read_async(void *self, void *buffer, UINT32 count, int options, void **operation)
{
    static volatile LONG serial;
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    X104PlatformReadOp *op;
    BYTE *bytes = NULL;
    UINT32 available, wanted;
    HRESULT hr;
    (void)options;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!buffer) return E_POINTER;
    available = stream->position < stream->size ? (UINT32)(stream->size - stream->position) : 0;
    wanted = count < available ? count : available;
    hr = x104_platform_buffer_bytes(buffer, &bytes);
    if (FAILED(hr)) {
        x104_logf("X104 platform: stream.ReadAsync: the game's IBuffer does not expose IBufferByteAccess (0x%08lx %s)", (unsigned long)hr, x104_hr_name(hr));
        return hr;
    }
    if (wanted && bytes) memcpy(bytes, stream->data + (size_t)stream->position, wanted);
    x104_platform_buffer_set_length(buffer, wanted);
    stream->position += wanted;
    op = (X104PlatformReadOp *)x104_object_new(&x104_platform_readop_class, sizeof(*op));
    if (!op) return E_OUTOFMEMORY;
    op->id = (unsigned long)InterlockedIncrement(&serial);
    op->buffer = buffer;
    x104_raw_addref(buffer);
    *operation = x104_object_iface0(&op->base);
    x104_logf("X104 platform: stream.ReadAsync(count %lu, position %lu of %lu) -> completed operation %p: %lu bytes",
              (unsigned long)count, (unsigned long)(stream->position - wanted), (unsigned long)stream->size, *operation, (unsigned long)wanted);
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_write_async(void *self, void *buffer, void **operation)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    BYTE *bytes = NULL;
    UINT32 length = 0;
    void *result;
    HRESULT hr;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!buffer) return E_POINTER;
    hr = x104_platform_buffer_bytes(buffer, &bytes);
    if (FAILED(hr)) {
        x104_logf("X104 platform: stream.WriteAsync: the game's IBuffer does not expose IBufferByteAccess (0x%08lx %s)", (unsigned long)hr, x104_hr_name(hr));
        return hr;
    }
    x104_platform_buffer_get_length(buffer, &length);
    hr = x104_platform_stream_write_bytes(stream, bytes, length);
    if (FAILED(hr)) return hr;
    result = x104_platform_op32_progress_new(length);
    if (!result) return E_OUTOFMEMORY;
    *operation = result;
    X104_LOG_LIMITED(&stream->writes, 8, "X104 platform: stream.WriteAsync(%lu bytes at %lu, size now %lu) -> completed operation %p",
                     (unsigned long)length, (unsigned long)(stream->position - length), (unsigned long)stream->size, *operation);
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_flush_async(void *self, void **operation)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    void *result;
    if (!operation) return E_POINTER;
    *operation = NULL;
    x104_platform_stream_save_file(stream);
    result = x104_platform_opbool_new(1);
    if (!result) return E_OUTOFMEMORY;
    *operation = result;
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_put_size(void *self, UINT64 value)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    UINT32 wanted = (UINT32)value;
    x104_logf("X104 platform: stream.put_Size(%lu) (current size %lu)", (unsigned long)value, (unsigned long)stream->size);
    if (wanted > stream->capacity) {
        BYTE *grown = x104_platform_grow(stream->data, wanted);
        if (!grown) return E_OUTOFMEMORY;
        stream->data = grown;
        stream->capacity = wanted;
    }
    if (stream->data && wanted > stream->size) memset(stream->data + stream->size, 0, wanted - stream->size);
    stream->size = wanted;
    if (stream->position > wanted) stream->position = wanted;
    stream->dirty = TRUE;
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_close(void *self)
{
    X104PlatformStream *stream = X104_THIS(X104PlatformStream, self);
    static volatile LONG logged;
    x104_platform_stream_save_file(stream);
    X104_LOG_LIMITED(&logged, 8, "X104 platform: stream.Close (%lu bytes)", (unsigned long)stream->size);
    return S_OK;
}

static HRESULT X104_API x104_platform_stream_get_content_type(void *self, HSTRING *value)
{
    (void)self;
    if (!value) return E_POINTER;
    return x104_hstring_create(L"application/octet-stream", value);
}

/* IRandomAccessStreamWithContentType: in the real ABI this IID's vtable is the flattened inheritance chain - the nine
 * IRandomAccessStream methods (6..14), IClosable::Close (15) and IContentTypeProvider::get_ContentType (16).  The 1.27 save
 * code QIs exactly this IID and then calls GetOutputStreamAt (slot 9) through it, so a "no methods of its own" view only
 * answers E_NOTIMPL (the game turns that into Platform::NotImplementedException and aborts the save). */
X104_VTBL_N(x104_platform_vt_stream_content, 17) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_stream_get_size,
    [7] = (const void *)x104_platform_stream_put_size,
    [8] = (const void *)x104_platform_stream_get_input_at,
    [9] = (const void *)x104_platform_stream_get_output_at,
    [10] = (const void *)x104_platform_stream_get_position,
    [11] = (const void *)x104_platform_stream_seek,
    [12] = (const void *)x104_platform_stream_clone_stream,
    [13] = (const void *)x104_platform_stream_get_can_read,
    [14] = (const void *)x104_platform_stream_get_can_write,
    [15] = (const void *)x104_platform_stream_close,
    [16] = (const void *)x104_platform_stream_get_content_type
};
X104_VTBL_N(x104_platform_vt_rastream, 15) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_stream_get_size,
    [7] = (const void *)x104_platform_stream_put_size,
    [8] = (const void *)x104_platform_stream_get_input_at,
    [9] = (const void *)x104_platform_stream_get_output_at,
    [10] = (const void *)x104_platform_stream_get_position,
    [11] = (const void *)x104_platform_stream_seek,
    [12] = (const void *)x104_platform_stream_clone_stream,
    [13] = (const void *)x104_platform_stream_get_can_read,
    [14] = (const void *)x104_platform_stream_get_can_write
};
X104_VTBL_N(x104_platform_vt_iinputstream, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_stream_read_async
};
X104_VTBL_N(x104_platform_vt_ioutputstream, 8) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_stream_write_async,
    [7] = (const void *)x104_platform_stream_flush_async
};
X104_VTBL_N(x104_platform_vt_iclosable, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_stream_close
};
X104_VTBL_N(x104_platform_vt_icontenttype, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_stream_get_content_type
};
static const X104IfaceDesc x104_platform_stream_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_irastream_content, "IRandomAccessStreamWithContentType", x104_platform_vt_stream_content),
    X104_IFACE_IID(&x104_platform_iid_irastream, "IRandomAccessStream", x104_platform_vt_rastream),
    X104_IFACE_IID(&x104_platform_iid_iinputstream, "IInputStream", x104_platform_vt_iinputstream),
    X104_IFACE_IID(&x104_platform_iid_ioutputstream, "IOutputStream", x104_platform_vt_ioutputstream),
    X104_IFACE_IID(&x104_platform_iid_iclosable, "IClosable", x104_platform_vt_iclosable),
    X104_IFACE_IID(&x104_platform_iid_icontenttype, "IContentTypeProvider", x104_platform_vt_icontenttype)
};
X104_CLASS_DEF(x104_platform_stream_class, "FileStream", L"Windows.Storage.Streams.IRandomAccessStreamWithContentType",
               x104_platform_stream_ifaces, X104_CLASS_AGILE, NULL);

/* read the whole file into memory once; the stream is then a plain byte array with a cursor */
static void x104_platform_stream_read_file(X104PlatformStream *stream)
{
    HANDLE handle;
    DWORD size, read = 0;
    stream->size = 0;
    if (!stream->path[0]) return;
    handle = CreateFileW(stream->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(handle, NULL);
    if (size && size != 0xFFFFFFFFu) {
        stream->data = (BYTE *)HeapAlloc(GetProcessHeap(), 0, size);
        if (stream->data && ReadFile(handle, stream->data, size, &read, NULL)) {
            stream->size = read;
            stream->capacity = size;
        }
    }
    CloseHandle(handle);
}

/* write `length` bytes at the stream cursor, growing the in-memory copy as needed */
static HRESULT x104_platform_stream_write_bytes(X104PlatformStream *stream, const BYTE *bytes, UINT32 length)
{
    UINT64 end;
    if (!stream) return E_POINTER;
    if (!bytes && length) return E_POINTER;
    end = stream->position + length;
    if (end > 0xFFFFFFFFu) return E_OUTOFMEMORY;        /* the whole save lives in one 32-bit buffer */
    if ((UINT32)end > stream->capacity) {
        UINT32 wanted = (UINT32)end + 4096;
        BYTE *grown = x104_platform_grow(stream->data, wanted);
        if (!grown) return E_OUTOFMEMORY;
        stream->data = grown;
        stream->capacity = wanted;
    }
    if (length) memcpy(stream->data + (size_t)stream->position, bytes, length);
    stream->position += length;
    if (stream->position > stream->size) stream->size = (UINT32)stream->position;
    stream->dirty = TRUE;
    return S_OK;
}

/* FlushAsync/Close: the game writes its save into an in-memory copy, so that copy is what lands on disk */
static void x104_platform_stream_save_file(X104PlatformStream *stream)
{
    HANDLE handle;
    DWORD written = 0;
    char shown[MAX_PATH];
    if (!stream || !stream->dirty || !stream->path[0]) return;
    x104_narrow(shown, sizeof(shown), stream->path);
    handle = CreateFileW(stream->path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        x104_logf("X104 platform: ERROR cannot open %s for writing (GetLastError %lu) - the save is NOT written",
                  shown, (unsigned long)GetLastError());
        return;
    }
    if (stream->size && !WriteFile(handle, stream->data, stream->size, &written, NULL)) {
        x104_logf("X104 platform: ERROR WriteFile(%s) failed (GetLastError %lu) - wrote %lu of %lu bytes",
                  shown, (unsigned long)GetLastError(), (unsigned long)written, (unsigned long)stream->size);
    } else {
        x104_logf("X104 platform: saved %lu bytes to %s", (unsigned long)stream->size, shown);
    }
    CloseHandle(handle);
    stream->dirty = FALSE;
}

static X104PlatformStream *x104_platform_stream_new(const wchar_t *path, int load)
{
    X104PlatformStream *stream = (X104PlatformStream *)x104_object_new(&x104_platform_stream_class, sizeof(X104PlatformStream));
    if (!stream) return NULL;
    if (path) lstrcpynW(stream->path, path, MAX_PATH);
    if (load) x104_platform_stream_read_file(stream);
    return stream;
}

static X104PlatformStream *x104_platform_stream_clone(X104PlatformStream *source, UINT64 position)
{
    X104PlatformStream *copy = x104_platform_stream_new(source->path, 0);
    if (!copy) return NULL;
    if (source->size) {
        copy->data = (BYTE *)HeapAlloc(GetProcessHeap(), 0, source->size);
        if (!copy->data) { x104_raw_release(x104_object_iface0(&copy->base)); return NULL; }
        memcpy(copy->data, source->data, source->size);
        copy->size = source->size;
        copy->capacity = source->size;
    }
    copy->position = position > copy->size ? copy->size : position;
    return copy;
}

/* ---- the StorageFile object: name/path plus the two ways to open it ---- */
typedef struct X104PlatformStorageFile {
    X104Object base;
    wchar_t path[MAX_PATH];
    wchar_t name[128];
} X104PlatformStorageFile;

static HRESULT x104_platform_stream_open_operation(X104PlatformStorageFile *file, void **operation, const char *why)
{
    static volatile LONG serial;
    X104PlatformFileOp *op;
    X104PlatformStream *stream;
    if (!operation) return E_POINTER;
    *operation = NULL;
    stream = x104_platform_stream_new(file->path, 1);
    if (!stream) return E_OUTOFMEMORY;
    op = (X104PlatformFileOp *)x104_object_new(&x104_platform_fileop_class, sizeof(*op));
    if (!op) { x104_raw_release(x104_object_iface0(&stream->base)); return E_OUTOFMEMORY; }
    op->id = (unsigned long)InterlockedIncrement(&serial);
    op->error = S_OK;
    op->result = x104_object_iface0(&stream->base);
    *operation = x104_object_iface0(&op->base);
    x104_logf("X104 platform: StorageFile.%s -> completed operation %p (stream %p, %lu bytes)", why, *operation, op->result,
              (unsigned long)stream->size);
    return S_OK;
}

static HRESULT X104_API x104_platform_file_get_name(void *self, HSTRING *value)
{
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    if (!value) return E_POINTER;
    return x104_hstring_create(file->name, value);
}

static HRESULT X104_API x104_platform_file_get_path(void *self, HSTRING *value)
{
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    if (!value) return E_POINTER;
    return x104_hstring_create(file->path, value);
}

static HRESULT X104_API x104_platform_file_get_attributes(void *self, int *value)
{
    WIN32_FILE_ATTRIBUTE_DATA info;
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    if (!value) return E_POINTER;
    *value = 0;
    if (GetFileAttributesExW(file->path, 0, &info)) *value = (int)info.dwFileAttributes;
    return S_OK;
}

static HRESULT X104_API x104_platform_file_get_date_created(void *self, LONGLONG *value)
{
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    if (!value) return E_POINTER;
    *value = 0;   /* the game only compares this against zero to detect a fresh save */
    (void)file;
    return S_OK;
}

static HRESULT X104_API x104_platform_file_is_of_type(void *self, int type, BYTE *value)
{
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    if (!value) return E_POINTER;
    *value = 0;
    if (type == 0 /* StorageItemTypes::File */) *value = 1;
    (void)file;
    return S_OK;
}

static HRESULT X104_API x104_platform_file_get_file_type(void *self, HSTRING *value)
{
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    const wchar_t *dot = NULL;
    const wchar_t *walk;
    if (!value) return E_POINTER;
    for (walk = file->name; *walk; ++walk) if (*walk == L'.') dot = walk;
    return x104_hstring_create(dot ? dot : L"", value);
}

static HRESULT X104_API x104_platform_file_get_content_type(void *self, HSTRING *value)
{
    (void)self;
    if (!value) return E_POINTER;
    return x104_hstring_create(L"application/octet-stream", value);
}

static HRESULT X104_API x104_platform_file_open_async(void *self, int mode, void **operation)
{
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    char shown[MAX_PATH];
    x104_narrow(shown, sizeof(shown), file->path);
    if (mode != 0 /* FileAccessMode::Read */ && mode != 1 /* FileAccessMode::ReadWrite */) {
        static volatile LONG logged;
        X104_LOG_LIMITED(&logged, 4, "X104 platform: StorageFile.OpenAsync(%d) on %s -> E_NOTIMPL (only read and read/write are implemented)", mode, shown);
        if (operation) *operation = NULL;
        return E_NOTIMPL;
    }
    x104_logf("X104 platform: StorageFile.OpenAsync(%s) on %s", mode ? "ReadWrite" : "Read", shown);
    return x104_platform_stream_open_operation(file, operation, mode ? "OpenAsync(ReadWrite)" : "OpenAsync(Read)");
}

static HRESULT X104_API x104_platform_file_open_read_async(void *self, void **operation)
{
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    char shown[MAX_PATH];
    x104_narrow(shown, sizeof(shown), file->path);
    x104_logf("X104 platform: IRandomAccessStreamReference.OpenReadAsync on %s", shown);
    return x104_platform_stream_open_operation(file, operation, "OpenReadAsync");
}

static HRESULT X104_API x104_platform_file_open_sequential_async(void *self, void **operation)
{
    X104PlatformStorageFile *file = X104_THIS(X104PlatformStorageFile, self);
    char shown[MAX_PATH];
    x104_narrow(shown, sizeof(shown), file->path);
    x104_logf("X104 platform: IInputStreamReference.OpenSequentialReadAsync on %s", shown);
    return x104_platform_stream_open_operation(file, operation, "OpenSequentialReadAsync");
}

X104_VTBL_N(x104_platform_vt_storagefile, 18) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_file_get_file_type,
    [7] = (const void *)x104_platform_file_get_content_type,
    [8] = (const void *)x104_platform_file_open_async
};
X104_VTBL(x104_platform_vt_fileitem, IStorageItem) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IStorageItem, get_Name, x104_platform_file_get_name),
    X104_SLOT(IStorageItem, get_Path, x104_platform_file_get_path),
    X104_SLOT(IStorageItem, get_Attributes, x104_platform_file_get_attributes),
    X104_SLOT(IStorageItem, get_DateCreated, x104_platform_file_get_date_created),
    X104_SLOT(IStorageItem, IsOfType, x104_platform_file_is_of_type)
};
X104_VTBL_N(x104_platform_vt_istreamref, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_file_open_read_async
};
X104_VTBL_N(x104_platform_vt_iseqstreamref, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_file_open_sequential_async
};
static const X104IfaceDesc x104_platform_storagefile_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_istoragefile, "IStorageFile", x104_platform_vt_storagefile),
    X104_IFACE(IStorageItem, x104_platform_vt_fileitem),
    X104_IFACE_IID(&x104_platform_iid_istreamref, "IRandomAccessStreamReference", x104_platform_vt_istreamref),
    X104_IFACE_IID(&x104_platform_iid_iseqstreamref, "IInputStreamReference", x104_platform_vt_iseqstreamref)
};
X104_CLASS_DEF(x104_platform_storagefile_class, "StorageFile", L"Windows.Storage.StorageFile", x104_platform_storagefile_ifaces,
               X104_CLASS_AGILE, NULL);

static X104Object *x104_platform_file_object(const wchar_t *path)
{
    X104PlatformStorageFile *file = (X104PlatformStorageFile *)x104_object_new(&x104_platform_storagefile_class, sizeof(X104PlatformStorageFile));
    const wchar_t *walk;
    const wchar_t *name;
    if (!file) return NULL;
    lstrcpynW(file->path, path ? path : L"", MAX_PATH);
    name = file->path;
    for (walk = file->path; *walk; ++walk) if (*walk == L'\\' || *walk == L'/') name = walk + 1;
    lstrcpynW(file->name, name, 128);
    return &file->base;
}

/* IStorageFolder.GetFileAsync(name): resolve the name inside the folder and complete the operation.  Windows itself fails this with
 * "file not found" when the save does not exist yet; the game treats that as the "first run" case, so the host creates an empty file and
 * reports success - the empty stream then reads as zero bytes, which is exactly what a fresh install looks like. */
static HRESULT X104_API x104_platform_folder_get_file_async(void *self, HSTRING name, void **operation)
{
    static volatile LONG serial;
    X104PlatformFolder *folder = X104_THIS(X104PlatformFolder, self);
    X104PlatformFileOp *op;
    X104Object *file;
    wchar_t full[MAX_PATH];
    char shown[MAX_PATH];
    char name_shown[128];
    UINT32 length = 0;
    const wchar_t *raw;
    DWORD attributes = 0xFFFFFFFFu;
    WIN32_FILE_ATTRIBUTE_DATA info;
    HANDLE handle;
    if (!operation) return E_POINTER;
    *operation = NULL;
    raw = name ? WindowsGetStringRawBuffer(name, &length) : NULL;
    x104_hstring_ascii(name, name_shown, sizeof(name_shown));
    lstrcpynW(full, folder->path ? folder->path : L"", MAX_PATH);
    if (raw && length) {
        lstrcatW(full, L"\\");
        lstrcatW(full, raw);
    }
    if (GetFileAttributesExW(full, 0 /* GetFileExInfoStandard */, &info)) attributes = info.dwFileAttributes;
    if (attributes == 0xFFFFFFFFu) {
        handle = CreateFileW(full, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        x104_logf("X104 platform: %s.GetFileAsync(%s): the file did not exist - created an empty one at %s",
                  folder->label ? folder->label : "StorageFolder", name_shown, full);
    }
    file = x104_platform_file_object(full);
    op = (X104PlatformFileOp *)x104_object_new(&x104_platform_fileop_class, sizeof(*op));
    if (!op) { if (file) x104_raw_release(file); return E_OUTOFMEMORY; }
    op->id = (unsigned long)InterlockedIncrement(&serial);
    op->error = S_OK;
    /* get_Results hands out an INTERFACE pointer: the game QIs IRandomAccessStreamReference on it straight away */
    op->result = file ? x104_object_iface0(file) : NULL;
    x104_narrow(shown, sizeof(shown), full);
    *operation = x104_object_iface0(&op->base);
    x104_logf("X104 platform: %s.GetFileAsync(%s) -> completed IAsyncOperation %p (id %lu): StorageFile %p for %s [%s]",
              folder->label ? folder->label : "StorageFolder", name_shown, *operation, op->id, file, shown,
              attributes == 0xFFFFFFFFu ? "created empty" : "file exists");
    return S_OK;
}


/* "name (n).ext": the GenerateUniqueName collision option appends the counter before the extension */
static void x104_platform_unique_path(wchar_t *path, unsigned attempt)
{
    wchar_t stem[MAX_PATH];
    wchar_t ext[64];
    wchar_t *walk, *last_dot = NULL;
    size_t length, index;
    if (!attempt) return;
    for (walk = path; *walk; ++walk) {
        if (*walk == L'\\' || *walk == L'/') last_dot = NULL;
        else if (*walk == L'.') last_dot = walk;
    }
    length = (size_t)((last_dot ? last_dot : path + lstrlenW(path)) - path);
    if (length > MAX_PATH - 32) length = MAX_PATH - 32;
    for (index = 0; index < length; ++index) stem[index] = path[index];
    stem[length] = 0;
    ext[0] = 0;
    if (last_dot) lstrcpynW(ext, last_dot, 64);
    wsprintfW(path, L"%s (%u)%s", stem, attempt, ext);
}

/* IStorageFolder.CreateFileAsync(name, CreationCollisionOption): the save-write path creates the file before opening it
 * ReadWrite.  options: 0 GenerateUniqueName, 1 ReplaceExisting, 2 FailIfExists, 3 OpenIfExists. */
static HRESULT x104_platform_folder_create_file_async(void *self, HSTRING name, int options, void **operation)
{
    static volatile LONG serial;
    X104PlatformFolder *folder = X104_THIS(X104PlatformFolder, self);
    X104PlatformFileOp *op;
    X104Object *file;
    wchar_t full[MAX_PATH];
    wchar_t base[MAX_PATH];
    char shown[MAX_PATH];
    char name_shown[128];
    UINT32 length = 0;
    const wchar_t *raw;
    HANDLE handle;
    DWORD disposition;
    DWORD last_error = 0;
    HRESULT error = S_OK;
    unsigned attempt = 0;
    if (!operation) return E_POINTER;
    *operation = NULL;
    raw = name ? WindowsGetStringRawBuffer(name, &length) : NULL;
    x104_hstring_ascii(name, name_shown, sizeof(name_shown));
    lstrcpynW(full, folder->path ? folder->path : L"", MAX_PATH);
    if (raw && length) {
        lstrcatW(full, L"\\");
        lstrcatW(full, raw);
    }
    lstrcpynW(base, full, MAX_PATH);
    switch (options) {
    case 1:  disposition = CREATE_ALWAYS; break;    /* ReplaceExisting */
    case 2:  disposition = CREATE_NEW;    break;    /* FailIfExists */
    case 3:  disposition = OPEN_EXISTING; break;    /* OpenIfExists */
    default: disposition = CREATE_NEW;    break;    /* GenerateUniqueName: CREATE_NEW + retry with " (n)" */
    }
    for (;;) {
        handle = CreateFileW(full, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, disposition,
                             FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); break; }
        last_error = GetLastError();
        if (options == 0 && last_error == ERROR_FILE_EXISTS && attempt < 999) {
            ++attempt;
            lstrcpynW(full, base, MAX_PATH);
            x104_platform_unique_path(full, attempt);
            continue;
        }
        error = HRESULT_FROM_WIN32(last_error);
        break;
    }
    file = x104_platform_file_object(full);
    op = (X104PlatformFileOp *)x104_object_new(&x104_platform_fileop_class, sizeof(*op));
    if (!op) { if (file) x104_raw_release(file); return E_OUTOFMEMORY; }
    op->id = (unsigned long)InterlockedIncrement(&serial);
    op->error = file ? error : E_OUTOFMEMORY;
    op->result = file ? x104_object_iface0(file) : NULL;
    *operation = x104_object_iface0(&op->base);
    x104_narrow(shown, sizeof(shown), full);
    x104_logf("X104 platform: %s.CreateFileAsync(%s, option %d) -> completed IAsyncOperation %p (id %lu, 0x%08lx %s): StorageFile %p for %s%s",
              folder->label ? folder->label : "StorageFolder", name_shown, options, *operation, op->id, (unsigned long)op->error,
              x104_hr_name(op->error), file, shown, attempt ? " (unique name)" : "");
    return S_OK;
}

static HRESULT X104_API x104_platform_folder_create_file_defaults(void *self, HSTRING name, void **operation)
{
    /* CreateFileAsync(name) defaults to CreationCollisionOption::FailIfExists */
    return x104_platform_folder_create_file_async(self, name, 2, operation);
}

X104_VTBL(x104_platform_vt_item, IStorageItem) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IStorageItem, get_Name, x104_platform_item_get_name),
    X104_SLOT(IStorageItem, get_Path, x104_platform_item_get_path)
};
X104_VTBL(x104_platform_vt_folder, IStorageFolder) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IStorageFolder, CreateFileAsyncOverloadDefaultOptions, x104_platform_folder_create_file_defaults),
    X104_SLOT(IStorageFolder, CreateFileAsync, x104_platform_folder_create_file_async),
    X104_SLOT(IStorageFolder, GetFileAsync, x104_platform_folder_get_file_async)
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
/* ---- logging IPropertySet wrapper ----
 * The host hands out its own IPropertySet (same IMap<String,Object> ABI layout as the OS PropertySet) whose methods log the key and forward to a real
 * Windows.Foundation.Collections.PropertySet activated from the runtime.  cocos2d's WP8 CCUserDefault keeps its keys in
 * LocalSettings.CreateContainer("UserDefaults", Always).Values, so this is where its reads/writes become visible. */
typedef struct X104PlatformSetting X104PlatformSetting;   /* one persisted key, defined with the persistence helpers below */

typedef struct X104PlatformPropertySet {
    X104Object base;
    void *inner;                    /* IInspectable of the real OS PropertySet */
    const char *label;              /* "LocalSettings.Values" / "LocalSettings.Children[...].Values" */
    BYTE persist;                   /* mirror this map in a file under the data root */
    BYTE persist_ready;             /* the file has been read: Insert/Remove/Clear write it back */
    X104PlatformSetting *settings;  /* the shadow list (= the map's contents on disk) */
    UINT32 settings_count;
    UINT32 settings_capacity;
    wchar_t file[96];               /* leaf name under the data root, e.g. "localsettings.dat" */
} X104PlatformPropertySet;

/* Windows.Foundation.Collections.IMap<String,Object> (the parameterised IID of IPropertySet's base interface) */
static const GUID x104_platform_iid_imap_string_object = {0x1b0d3570,0x0877,0x5ec2,{0x8a,0x2c,0x3b,0x95,0x39,0x50,0x6a,0xca}};
/* Windows.Foundation.IPropertyValueStatics (Windows.Foundation.PropertyValue) */
static const GUID x104_platform_iid_property_value_statics = {0x629bdbc8,0xd932,0x4ff4,{0x96,0xb9,0x8d,0x96,0xc5,0xc1,0xe8,0x58}};
enum { X104_SLOT_IPropertyValueStatics_CreateInt32 = 10,   /* CreateEmpty 6, CreateUInt8 7, CreateInt16 8, CreateUInt16 9, CreateInt32 10, ... */
       X104_SLOT_IPropertyValueStatics_CreateBoolean = 17,  /* ..., CreateChar16 16, CreateBoolean 17, CreateString 18 */
       X104_SLOT_IPropertyValueStatics_CreateString = 18 };
typedef HRESULT (X104_API *X104_FN_IPropertyValueStatics_CreateInt32)(void *, LONG, void **);
typedef HRESULT (X104_API *X104_FN_IPropertyValueStatics_CreateBoolean)(void *, BYTE, void **);
typedef HRESULT (X104_API *X104_FN_IPropertyValueStatics_CreateString)(void *, HSTRING, void **);

#define X104_SET(self)  ((X104PlatformPropertySet *)X104_OBJ(self))

static void *x104_platform_boxed_value(int is_bool, LONG value);   /* defined with the factory below */
static HRESULT X104_API x104_platform_propertyset_insert(void *self, HSTRING key, void *value, BYTE *replaced);
static void x104_platform_settings_observe(X104PlatformPropertySet *set, HSTRING key, void *value);   /* LocalSettings persistence */
static void x104_platform_settings_forget(X104PlatformPropertySet *set, HSTRING key);
static void x104_platform_settings_reset(X104PlatformPropertySet *set);
static void x104_platform_settings_load(X104PlatformPropertySet *set);

static int x104_platform_ends_with(const char *text, const char *suffix)
{
    size_t text_length = lstrlenA(text);
    size_t suffix_length = lstrlenA(suffix);
    size_t index;
    if (text_length < suffix_length) return 0;
    for (index = 0; index < suffix_length; ++index)
        if (text[text_length - suffix_length + index] != suffix[index]) return 0;
    return 1;
}

static HRESULT X104_API x104_platform_propertyset_lookup(void *self, HSTRING key, void **value)
{
    X104PlatformPropertySet *set = X104_SET(self);
    char shown[128];
    HRESULT hr;
    x104_hstring_ascii(key, shown, sizeof(shown));
    if (value) *value = NULL;
    hr = X104_VFN(set->inner, X104_SLOT_IMap_Lookup, X104_FN_IMap_Lookup)(set->inner, key, value);
    x104_logf("X104 platform: %s.Lookup(%s) -> 0x%08lx %s", set->label, shown, (unsigned long)hr, x104_hr_name(hr));
    if (hr == 0x8000000Bu /* E_BOUNDS: the key is not in the map */) {
        /* CCUserDefault reads with a bare Lookup (no HasKey) and a miss becomes a fatal C++/CX OutOfBoundsException.  On a device every key it asks
         * for exists because the game wrote it at some point; the host therefore materialises the getter's own default on first read.  The type is
         * the only thing Lookup cannot know, so the per-vehicle flags that are booleans ("<vehicle>_unlocked") are recognised by name and everything
         * else is an int32 upgrade level. */
        int is_bool = x104_platform_ends_with(shown, "_unlocked") || x104_platform_ends_with(shown, "_should_ask");
        void *fallback = x104_platform_boxed_value(is_bool, 0);
        if (fallback && value) {
            BYTE replaced = 0;
            HRESULT ihr = x104_platform_propertyset_insert(x104_object_iface0(&set->base), key, fallback, &replaced);
            if (SUCCEEDED(ihr)) {
                x104_raw_addref(fallback);
                *value = fallback;
                x104_logf("X104 platform: %s.Lookup(%s): missing key materialised as %s -> S_OK", set->label, shown,
                          is_bool ? "bool FALSE" : "int32 0");
                return S_OK;
            }
        }
    }
    return hr;
}

static HRESULT X104_API x104_platform_propertyset_get_size(void *self, ULONG *size)
{
    X104PlatformPropertySet *set = X104_SET(self);
    HRESULT hr = X104_VFN(set->inner, X104_SLOT_IMap_get_Size, X104_FN_IMap_get_Size)(set->inner, size);
    x104_logf("X104 platform: %s.get_Size -> %lu (hr 0x%08lx)", set->label, size ? (unsigned long)*size : 0UL, (unsigned long)hr);
    return hr;
}

static HRESULT X104_API x104_platform_propertyset_has_key(void *self, HSTRING key, BYTE *found)
{
    X104PlatformPropertySet *set = X104_SET(self);
    char shown[128];
    HRESULT hr;
    x104_hstring_ascii(key, shown, sizeof(shown));
    hr = X104_VFN(set->inner, X104_SLOT_IMap_HasKey, X104_FN_IMap_HasKey)(set->inner, key, found);
    x104_logf("X104 platform: %s.HasKey(%s) -> %s (hr 0x%08lx)", set->label, shown, (found && *found) ? "TRUE" : "FALSE", (unsigned long)hr);
    return hr;
}

static HRESULT X104_API x104_platform_propertyset_get_view(void *self, void **view)
{
    X104PlatformPropertySet *set = X104_SET(self);
    HRESULT hr = X104_VFN(set->inner, X104_SLOT_IMap_GetView, X104_FN_IMap_GetView)(set->inner, view);
    x104_logf("X104 platform: %s.GetView -> 0x%08lx %s", set->label, (unsigned long)hr, x104_hr_name(hr));
    return hr;
}

static HRESULT X104_API x104_platform_propertyset_insert(void *self, HSTRING key, void *value, BYTE *replaced)
{
    X104PlatformPropertySet *set = X104_SET(self);
    char shown[128];
    HRESULT hr;
    x104_hstring_ascii(key, shown, sizeof(shown));
    hr = X104_VFN(set->inner, X104_SLOT_IMap_Insert, X104_FN_IMap_Insert)(set->inner, key, value, replaced);
    x104_logf("X104 platform: %s.Insert(%s) -> 0x%08lx %s", set->label, shown, (unsigned long)hr, x104_hr_name(hr));
    if (SUCCEEDED(hr)) x104_platform_settings_observe(set, key, value);
    return hr;
}

static HRESULT X104_API x104_platform_propertyset_remove(void *self, HSTRING key)
{
    X104PlatformPropertySet *set = X104_SET(self);
    char shown[128];
    HRESULT hr;
    x104_hstring_ascii(key, shown, sizeof(shown));
    hr = X104_VFN(set->inner, X104_SLOT_IMap_Remove, X104_FN_IMap_Remove)(set->inner, key);
    x104_logf("X104 platform: %s.Remove(%s) -> 0x%08lx %s", set->label, shown, (unsigned long)hr, x104_hr_name(hr));
    if (SUCCEEDED(hr)) x104_platform_settings_forget(set, key);
    return hr;
}

static HRESULT X104_API x104_platform_propertyset_clear(void *self)
{
    X104PlatformPropertySet *set = X104_SET(self);
    HRESULT hr = X104_VFN(set->inner, X104_SLOT_IMap_Clear, X104_FN_IMap_Clear)(set->inner);
    x104_logf("X104 platform: %s.Clear -> 0x%08lx %s", set->label, (unsigned long)hr, x104_hr_name(hr));
    if (SUCCEEDED(hr)) x104_platform_settings_reset(set);
    return hr;
}

static HRESULT X104_API x104_platform_propertyset_add_map_changed(void *self, void *handler, LONGLONG *token)
{
    X104PlatformPropertySet *set = X104_SET(self);
    (void)handler;
    if (token) *token = 0;                          /* nobody in these games subscribes; a zero token is never removed */
    x104_logf("X104 platform: %s.add_MapChanged -> S_OK (token 0)", set->label);
    return S_OK;
}

static HRESULT X104_API x104_platform_propertyset_remove_map_changed(void *self, LONGLONG token)
{
    X104PlatformPropertySet *set = X104_SET(self);
    (void)token;
    return S_OK;
}

X104_VTBL(x104_platform_vt_propertyset, IMap) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IMap, Lookup, x104_platform_propertyset_lookup),
    X104_SLOT(IMap, get_Size, x104_platform_propertyset_get_size),
    X104_SLOT(IMap, HasKey, x104_platform_propertyset_has_key),
    X104_SLOT(IMap, GetView, x104_platform_propertyset_get_view),
    X104_SLOT(IMap, Insert, x104_platform_propertyset_insert),
    X104_SLOT(IMap, Remove, x104_platform_propertyset_remove),
    X104_SLOT(IMap, Clear, x104_platform_propertyset_clear),
    X104_SLOT(IObservableMap, add_MapChanged, x104_platform_propertyset_add_map_changed),
    X104_SLOT(IObservableMap, remove_MapChanged, x104_platform_propertyset_remove_map_changed)
};
static const X104IfaceDesc x104_platform_propertyset_ifaces[] = {
    X104_IFACE_IID(&X104_IID_IInspectable, "IPropertySet", x104_platform_vt_propertyset),
    /* C++/CX projects IPropertySet as IMap<String,Object> and QIs for the parameterised IID before the first call (seen in the device log) */
    X104_IFACE_IID(&x104_platform_iid_imap_string_object, "IMap<String,Object>", x104_platform_vt_propertyset)
};
X104_CLASS_DEF(x104_platform_propertyset_class, "PropertySet", L"Windows.Foundation.Collections.PropertySet", x104_platform_propertyset_ifaces,
               X104_CLASS_AGILE, NULL);

/* lazily created boxed Windows.Foundation.PropertyValue (int32 or boolean), cached per distinct value: the settings defaults only ever use 0 and 1 */
static void *x104_platform_boxed_value(int is_bool, LONG value)
{
    static void *cache[4];                          /* [is_bool * 2 + (value != 0)] */
    static volatile LONG state[4];
    unsigned index = (is_bool ? 2u : 0u) + (value ? 1u : 0u);
    if (x104_once_begin(&state[index])) {
        void *factory = NULL;
        void *boxed = NULL;
        HRESULT hr = x104.original_activation
            ? ((X104ActivationFn)x104.original_activation)(L"Windows.Foundation.PropertyValue", &x104_platform_iid_property_value_statics, &factory)
            : E_FAIL;
        if (SUCCEEDED(hr) && factory) {
            hr = is_bool
                ? X104_VFN(factory, X104_SLOT_IPropertyValueStatics_CreateBoolean, X104_FN_IPropertyValueStatics_CreateBoolean)(factory, value ? 1 : 0, &boxed)
                : X104_VFN(factory, X104_SLOT_IPropertyValueStatics_CreateInt32, X104_FN_IPropertyValueStatics_CreateInt32)(factory, value, &boxed);
            x104_raw_release(factory);
        }
        cache[index] = boxed;
        x104_logf("X104 platform: boxed PropertyValue %s %ld = %p (hr 0x%08lx %s)", is_bool ? "bool" : "int32", (long)value, boxed,
                  (unsigned long)hr, x104_hr_name(hr));
        x104_once_end(&state[index]);
    }
    return cache[index];
}

/* ---- win32craft.ini: the host's own settings file ----
 * The launcher drops a small ini next to the exe so that a setting the game would otherwise take from the system can be chosen per
 * install without rebuilding anything.  Today it carries the language:
 *     [platform]
 *     language=ru
 * The value feeds both the LocalSettings "language" key the game reads with getStringForKey (cocos2d CCUserDefault) and the locale
 * Windows.Globalization.ApplicationLanguages reports.  A missing file, a missing key or a language the game has no translation for
 * keeps the previous behaviour (English), so an install without the ini behaves exactly as before.  The file is read once, lazily,
 * from the directory the exe was started from. */
static wchar_t x104_config_lang_code[8] = L"en";
static wchar_t x104_config_lang_locale[16] = L"en-US";
static volatile LONG x104_config_state;

typedef struct X104ConfigLanguage {
    const wchar_t *match;           /* lower-case form accepted in the ini */
    const wchar_t *code;            /* the spelling the game uses */
    const wchar_t *locale;          /* the WinRT locale tag */
} X104ConfigLanguage;

static const X104ConfigLanguage x104_config_languages[] = {
    { L"en",    L"en",    L"en-US" },
    { L"de",    L"de",    L"de-DE" },
    { L"fr",    L"fr",    L"fr-FR" },
    { L"it",    L"it",    L"it-IT" },
    { L"es",    L"es",    L"es-ES" },
    { L"ru",    L"ru",    L"ru-RU" },
    { L"pt",    L"pt",    L"pt-BR" },
    { L"ja",    L"ja",    L"ja-JP" },
    { L"ko",    L"ko",    L"ko-KR" },
    { L"zh",    L"zh",    L"zh-CN" },
    { L"zh-cn", L"zh",    L"zh-CN" },
    { L"zh-tw", L"zh-TW", L"zh-TW" }
};

static void x104_config_copy(wchar_t *out, int capacity, const wchar_t *text)
{
    int index = 0;
    if (capacity <= 0) return;
    while (text[index] && index < capacity - 1) { out[index] = text[index]; ++index; }
    out[index] = 0;
}

static void x104_config_set_language(const X104ConfigLanguage *entry)
{
    x104_config_copy(x104_config_lang_code, (int)ARRAYSIZE(x104_config_lang_code), entry->code);
    x104_config_copy(x104_config_lang_locale, (int)ARRAYSIZE(x104_config_lang_locale), entry->locale);
}

/* value is the raw ini value: matched case-insensitively, first whole ("zh-tw"), then by its primary subtag ("ru-RU" -> "ru"), so
 * both the short and the full tag work.  An unknown tag keeps English and says so in the log. */
static void x104_config_apply_language(const wchar_t *value)
{
    wchar_t lowered[16];
    wchar_t primary[8];
    int index = 0;
    int primary_length = 0;
    unsigned entry;

    if (!value || !value[0]) return;
    while (value[index] && index < (int)ARRAYSIZE(lowered) - 1) {
        wchar_t character = value[index];
        if (character >= L'A' && character <= L'Z') character = (wchar_t)(character - L'A' + L'a');
        lowered[index] = character;
        ++index;
    }
    lowered[index] = 0;
    for (entry = 0; entry < ARRAYSIZE(x104_config_languages); ++entry)
        if (!lstrcmpW(lowered, x104_config_languages[entry].match)) { x104_config_set_language(&x104_config_languages[entry]); return; }
    while (lowered[primary_length] && lowered[primary_length] != L'-' && primary_length < (int)ARRAYSIZE(primary) - 1) {
        primary[primary_length] = lowered[primary_length];
        ++primary_length;
    }
    primary[primary_length] = 0;
    for (entry = 0; entry < ARRAYSIZE(x104_config_languages); ++entry) {
        if (lstrlenW(x104_config_languages[entry].match) != primary_length) continue;
        if (lstrcmpW(primary, x104_config_languages[entry].match)) continue;
        x104_config_set_language(&x104_config_languages[entry]);
        x104_config_copy(x104_config_lang_locale, (int)ARRAYSIZE(x104_config_lang_locale), value);   /* keep the caller's exact tag for WinRT */
        return;
    }
    x104_logf("X104 config: language \"%ls\" has no translation in the game; keeping en", value);
}

static int x104_config_key_is(const char *line, int length, const char *key)
{
    int index = 0;
    while (key[index]) {
        char character;
        if (index >= length) return 0;
        character = line[index];
        if (character >= 'A' && character <= 'Z') character = (char)(character - 'A' + 'a');
        if (character != key[index]) return 0;
        ++index;
    }
    return index == length;
}

/* tiny ini reader: [section] headers, key=value lines, ';'/'#' comments; only [platform] language is looked at (keys before the first
 * header count as platform too, so a one-line file works).  ASCII by design: language tags are ASCII. */
static void x104_config_parse(const char *text)
{
    const char *line = text;
    int in_platform = 1;

    if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) line += 3;   /* UTF-8 BOM */
    while (*line) {
        const char *end = line;
        int start = 0;
        int length;
        while (*end && *end != '\n' && *end != '\r') ++end;
        length = (int)(end - line);
        while (start < length && (line[start] == ' ' || line[start] == '\t')) ++start;
        while (length > start && (line[length - 1] == ' ' || line[length - 1] == '\t')) --length;
        if (length > start && (line[start] == ';' || line[start] == '#')) {
            /* comment */
        } else if (length > start && line[start] == '[') {
            int close = start + 1;
            while (close < length && line[close] != ']') ++close;
            in_platform = x104_config_key_is(line + start + 1, close - start - 1, "platform");
        } else if (in_platform) {
            int equals = start;
            while (equals < length && line[equals] != '=') ++equals;
            if (equals < length && x104_config_key_is(line + start, equals - start, "language")) {
                int value_start = equals + 1;
                int value_end = length;
                wchar_t value[24];
                int index = 0;
                while (value_start < value_end && (line[value_start] == ' ' || line[value_start] == '\t')) ++value_start;
                while (value_end > value_start && (line[value_end - 1] == ' ' || line[value_end - 1] == '\t' || line[value_end - 1] == '"')) --value_end;
                if (value_start < value_end && line[value_start] == '"') ++value_start;
                while (value_start < value_end && index < (int)ARRAYSIZE(value) - 1) value[index++] = (wchar_t)(unsigned char)line[value_start++];
                value[index] = 0;
                x104_config_apply_language(value);
            }
        }
        line = *end ? end + 1 : end;
    }
}

static void x104_config_load(void)
{
    wchar_t path[MAX_PATH];
    DWORD length;
    HANDLE file;
    char buffer[2048];
    DWORD read = 0;

    length = GetModuleFileNameW(NULL, path, (DWORD)ARRAYSIZE(path));
    if (!length || length >= ARRAYSIZE(path) - 16) return;
    while (length && path[length - 1] != L'\\' && path[length - 1] != L'/') --length;
    x104_config_copy(path + length, (int)ARRAYSIZE(path) - (int)length, L"win32craft.ini");
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        x104_logf("X104 config: no win32craft.ini next to the exe; language stays %ls", x104_config_lang_code);
        return;
    }
    if (ReadFile(file, buffer, sizeof(buffer) - 1, &read, NULL) && read) {
        buffer[read] = 0;
        x104_config_parse(buffer);
    }
    CloseHandle(file);
    x104_logf("X104 config: win32craft.ini: language %ls, locale %ls", x104_config_lang_code, x104_config_lang_locale);
}

static void x104_config_ensure(void)
{
    if (x104_once_begin(&x104_config_state)) {
        x104_config_load();
        x104_once_end(&x104_config_state);
    }
}

const wchar_t *x104_config_language_code(void)
{
    x104_config_ensure();
    return x104_config_lang_code;
}

const wchar_t *x104_config_language_locale(void)
{
    x104_config_ensure();
    return x104_config_lang_locale;
}

/* HCR reads its whole settings block with bare Lookup calls (no HasKey) inside CCUserDefault, so on a fresh profile every key it asks for must already be
 * there - the type and the default come from the getter arguments in the game code (getIntegerForKey/getBoolForKey/getStringForKey at .text:0x4A21D0).
 * The language codes are the ones the game ships translations for (en, de, zh, zh-TW, fr, it, ja, es, ru, pt, ko). */
enum { X104_SEED_INT32 = 0, X104_SEED_BOOL = 1, X104_SEED_STRING = 2 };

typedef struct X104PlatformSeed {
    const wchar_t *key;
    unsigned char kind;
    signed char value;              /* int32/bool value */
    const wchar_t *text;            /* string value */
} X104PlatformSeed;

static const X104PlatformSeed x104_platform_seeds[] = {
    { L"stage",                   X104_SEED_INT32,  0, NULL },
    { L"coins",                   X104_SEED_INT32,  0, NULL },
    { L"highscore",               X104_SEED_INT32,  0, NULL },
    { L"level_engine",            X104_SEED_INT32,  1, NULL },
    { L"level_suspension",        X104_SEED_INT32,  1, NULL },
    { L"level_awd",               X104_SEED_INT32,  1, NULL },
    { L"level_traction",          X104_SEED_INT32,  1, NULL },
    { L"level_body",              X104_SEED_INT32,  1, NULL },
    { L"stage_selection",         X104_SEED_INT32,  0, NULL },
    { L"car_selection",           X104_SEED_INT32,  0, NULL },
    { L"display_mode",            X104_SEED_INT32,  0, NULL },
    { L"facebook_like_rewarded",  X104_SEED_INT32,  0, NULL },
    { L"video_ads_enabled",       X104_SEED_INT32,  1, NULL },
    { L"everyplay_enabled",       X104_SEED_INT32,  1, NULL },
    { L"adfree",                  X104_SEED_BOOL,   0, NULL },
    { L"adFreeIAPPurchased",      X104_SEED_BOOL,   0, NULL },
    { L"auto_sign_in",            X104_SEED_BOOL,   0, NULL },
    { L"render_dirt",             X104_SEED_BOOL,   1, NULL },
    { L"scrolling_background",    X104_SEED_BOOL,   1, NULL },
    { L"halloween_content",       X104_SEED_BOOL,   1, NULL },
    { L"music",                   X104_SEED_BOOL,   1, NULL },
    { L"sound",                   X104_SEED_BOOL,   1, NULL },
    { L"moga_pro_guide",          X104_SEED_BOOL,   0, NULL },
    { L"moga_pocket_guide",       X104_SEED_BOOL,   0, NULL },
    /* the Store review prompt: read with getBoolForKey (a bare Lookup must not hand it an int32 - that raises TYPE_E_TYPEMISMATCH inside
     * vccorlib and kills the game's UI loop).  FALSE = the game never asks the Store for a rating. */
    { L"windows_store_review_should_ask", X104_SEED_BOOL, 0, NULL },
    { L"windows_store_review_counter",    X104_SEED_INT32, 0, NULL },
    /* read as a string with a bare Lookup; when it is present the branch that would read "stage" as a string is skipped */
    { L"language",                X104_SEED_STRING, 0, L"en" }
};

/* fresh boxed Windows.Foundation.PropertyValue string (the caller releases it) */
static void *x104_platform_boxed_string(const wchar_t *text)
{
    void *factory = NULL;
    void *boxed = NULL;
    HSTRING string = NULL;
    HRESULT hr = x104.original_activation
        ? ((X104ActivationFn)x104.original_activation)(L"Windows.Foundation.PropertyValue", &x104_platform_iid_property_value_statics, &factory)
        : E_FAIL;
    if (SUCCEEDED(hr) && factory) {
        hr = x104_hstring_create(text, &string);
        if (SUCCEEDED(hr)) {
            hr = X104_VFN(factory, X104_SLOT_IPropertyValueStatics_CreateString, X104_FN_IPropertyValueStatics_CreateString)(factory, string, &boxed);
            WindowsDeleteString(string);
        }
        x104_raw_release(factory);
    }
    if (FAILED(hr)) x104_logf("X104 platform: boxed PropertyValue string %ls failed (hr 0x%08lx %s)", text, (unsigned long)hr, x104_hr_name(hr));
    return boxed;
}

/* ---- settings persistence ----
 * On a phone ApplicationData.LocalSettings lives in the app profile, so CCUserDefault (coins, upgrade levels, achievements,
 * car_selection, ...) survives a restart.  The host wraps a plain in-memory Windows.Foundation.Collections.PropertySet, so every
 * run started from the seeds above and the progress was lost.  Every persisted map keeps a shadow list which is written to
 * <data root>\localsettings.dat (root map) or localsettings_<container>.dat (a named child) after every Insert/Remove/Clear
 * (never during seeding), then read back into the map before the game touches it.  One entry per line,
 * "type<TAB>key<TAB>value", type i (int32), b (boolean) or s (string); 'r' removes a key and 'c' clears the map.  Appending
 * instead of rewriting means a run killed during a save cannot truncate the profile; loading compacts the log. */
struct X104PlatformSetting {
    wchar_t key[96];
    wchar_t text[192];              /* string value ("s") */
    LONG number;                    /* int32/boolean value ("i"/"b") */
    char type;                      /* 'i', 'b', 's' */
};

static BOOL x104_platform_settings_leaf_path(const wchar_t *leaf, wchar_t *path, unsigned capacity)
{
    if (!leaf || !leaf[0] || !x104.data_root[0]) return FALSE;
    return x104_platform_join(path, capacity, x104.data_root, leaf);
}

static BOOL x104_platform_settings_path(X104PlatformPropertySet *set, wchar_t *path, unsigned capacity)
{
    return set && x104_platform_settings_leaf_path(set->file, path, capacity);
}

/* wide -> UTF-8 with the escape rules of the file (one entry per line) */
static UINT32 x104_platform_settings_encode(const wchar_t *text, char *out, UINT32 capacity)
{
    UINT32 used = 0;
    while (text && *text) {
        wchar_t ch = *text++;
        char unit[8];
        int length;
        if (ch == L'\\' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            if (used + 2 >= capacity) break;
            out[used++] = '\\';
            out[used++] = (char)(ch == L'\\' ? '\\' : ch == L'\t' ? 't' : ch == L'\r' ? 'r' : 'n');
            continue;
        }
        length = WideCharToMultiByte(CP_UTF8, 0, &ch, 1, unit, (int)sizeof(unit), NULL, NULL);
        if (length <= 0) continue;
        if (used + (UINT32)length >= capacity) break;
        memcpy(out + used, unit, (size_t)length);
        used += (UINT32)length;
    }
    out[used] = 0;
    return used;
}

static void x104_platform_settings_decode(const wchar_t *text, wchar_t *out, unsigned capacity)
{
    unsigned used = 0;
    while (text && *text && used + 1 < capacity) {
        wchar_t ch = *text++;
        if (ch == L'\\' && *text) {
            wchar_t next = *text++;
            ch = next == L't' ? L'\t' : next == L'n' ? L'\n' : next == L'r' ? L'\r' : next;
        }
        out[used++] = ch;
    }
    out[used] = 0;
}

static LONG x104_platform_settings_number(const wchar_t *text)
{
    LONG value = 0;
    int negative = 0;
    if (text && (*text == L'-' || *text == L'+')) negative = *text++ == L'-';
    while (text && *text >= L'0' && *text <= L'9') {
        value = value * 10 + (LONG)(*text - L'0');
        ++text;
    }
    return negative ? -value : value;
}

static X104PlatformSetting *x104_platform_settings_find(X104PlatformPropertySet *set, const wchar_t *key)
{
    UINT32 index;
    for (index = 0; index < set->settings_count; ++index)
        if (!lstrcmpW(set->settings[index].key, key)) return &set->settings[index];
    return NULL;
}

static X104PlatformSetting *x104_platform_settings_add(X104PlatformPropertySet *set, const wchar_t *key)
{
    X104PlatformSetting *entry = x104_platform_settings_find(set, key);
    if (entry) return entry;
    if (set->settings_count == set->settings_capacity) {
        UINT32 capacity = set->settings_capacity ? set->settings_capacity * 2 : 64;
        X104PlatformSetting *grown = (X104PlatformSetting *)x104_platform_grow((BYTE *)set->settings,
                                                                                capacity * (UINT32)sizeof(*grown));
        if (!grown) return NULL;
        set->settings = grown;
        set->settings_capacity = capacity;
    }
    entry = &set->settings[set->settings_count++];
    memset(entry, 0, sizeof(*entry));
    lstrcpynW(entry->key, key, (int)ARRAYSIZE(entry->key));
    return entry;
}

static void x104_platform_settings_drop(X104PlatformPropertySet *set, const wchar_t *key)
{
    UINT32 index;
    for (index = 0; index < set->settings_count; ++index) {
        if (lstrcmpW(set->settings[index].key, key)) continue;
        for (; index + 1 < set->settings_count; ++index)
            set->settings[index] = set->settings[index + 1];
        --set->settings_count;
        return;
    }
}

/* a fresh boxed Windows.Foundation.PropertyValue int32 for an arbitrary value (the cached helper only knows 0/1) */
static void *x104_platform_boxed_int32(LONG value)
{
    void *factory = NULL;
    void *boxed = NULL;
    HRESULT hr = x104.original_activation
        ? ((X104ActivationFn)x104.original_activation)(L"Windows.Foundation.PropertyValue", &x104_platform_iid_property_value_statics, &factory)
        : E_FAIL;
    if (SUCCEEDED(hr) && factory) {
        hr = X104_VFN(factory, X104_SLOT_IPropertyValueStatics_CreateInt32, X104_FN_IPropertyValueStatics_CreateInt32)(factory, value, &boxed);
        x104_raw_release(factory);
    }
    if (FAILED(hr)) x104_logf("X104 platform: boxed PropertyValue int32 %ld failed (hr 0x%08lx %s)", (long)value, (unsigned long)hr, x104_hr_name(hr));
    return boxed;
}

/* put one persisted entry back into the map (no shadow update: the entry is already the shadow) */
static void x104_platform_settings_push(X104PlatformPropertySet *set, const X104PlatformSetting *entry)
{
    void *boxed;
    HSTRING name = NULL;
    if (entry->type == 's') boxed = x104_platform_boxed_string(entry->text);
    else if (entry->type == 'b') boxed = x104_platform_boxed_value(1, entry->number != 0);
    else boxed = x104_platform_boxed_int32(entry->number);
    if (!boxed) return;
    if (SUCCEEDED(x104_hstring_create(entry->key, &name))) {
        BYTE replaced = 0;
        x104_platform_propertyset_insert(x104_object_iface0(&set->base), name, boxed, &replaced);
        WindowsDeleteString(name);
    }
    if (entry->type == 's') x104_raw_release(boxed);        /* the map holds its own reference; the int32/bool helpers are cached */
}

/* One "type<TAB>key<TAB>value" line.  The file is log structured: every Insert/Remove/Clear appends one line (so a save never
 * truncates the file the way a full rewrite did - a run killed during a rewrite left 0 bytes), the last line for a key wins,
 * and 'r' (remove) / 'c' (clear) are operations.  Loading compacts the file back to one line per key. */
static UINT32 x104_platform_settings_line(char type, const wchar_t *key, LONG number, const wchar_t *text, char *out, UINT32 capacity)
{
    UINT32 size = 0;
    UINT32 key_length = x104_platform_settings_encode(key, out + 2, capacity > 2 ? capacity - 2 : 0);
    if (2 + key_length + 2 > capacity) return 0;
    out[0] = type;
    out[1] = '\t';
    size = 2 + key_length;
    out[size++] = '\t';
    if (type == 's') {
        UINT32 value_length = x104_platform_settings_encode(text, out + size, capacity - size);
        size += value_length;
    } else if (type == 'r' || type == 'c') {
        /* no value */
    } else {
        char value[24];
        int value_length = wsprintfA(value, "%ld", (long)number);
        if (value_length < 0 || size + (UINT32)value_length + 2 > capacity) return 0;
        memcpy(out + size, value, (size_t)value_length);
        size += (UINT32)value_length;
    }
    if (size + 1 >= capacity) return 0;
    out[size++] = '\n';
    return size;
}

/* rewrite the whole map (startup compaction; Clear/Remove only append tombstones) */
static void x104_platform_settings_save(X104PlatformPropertySet *set)
{
    wchar_t path[MAX_PATH];
    char *data;
    UINT32 size = 0;
    UINT32 capacity = 4096;
    UINT32 index;
    DWORD written = 0;
    HANDLE file;

    if (!set || !set->persist) return;
    if (!x104_platform_settings_path(set, path, MAX_PATH)) return;
    data = (char *)HeapAlloc(GetProcessHeap(), 0, capacity);
    if (!data) return;
    for (index = 0; index < set->settings_count; ++index) {
        const X104PlatformSetting *entry = &set->settings[index];
        char line[1408];
        UINT32 line_length = x104_platform_settings_line(entry->type, entry->key, entry->number, entry->text, line, (UINT32)sizeof(line));
        if (!line_length) continue;
        if (size + line_length > capacity) {
            UINT32 wanted = capacity;
            char *grown;
            while (wanted < size + line_length) wanted *= 2;
            grown = (char *)x104_platform_grow((BYTE *)data, wanted);
            if (!grown) { HeapFree(GetProcessHeap(), 0, data); return; }
            data = grown;
            capacity = wanted;
        }
        memcpy(data + size, line, line_length);
        size += line_length;
    }
    file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        x104_logf("X104 platform: %s: cannot write localsettings.dat (Win32 error %lu)", set->label, (unsigned long)GetLastError());
    } else {
        if (WriteFile(file, data, size, &written, NULL) && written == size)
            x104_logf("X104 platform: %s: compacted %ls to %lu entries (%lu bytes)", set->label, set->file,
                      (unsigned long)set->settings_count, (unsigned long)size);
        else
            x104_logf("X104 platform: %s: localsettings.dat write failed (Win32 error %lu)", set->label, (unsigned long)GetLastError());
        CloseHandle(file);
    }
    HeapFree(GetProcessHeap(), 0, data);
}

/* append one operation; FILE_APPEND_DATA / OPEN_ALWAYS are not in win32_compat.h */
#ifndef FILE_APPEND_DATA
#define FILE_APPEND_DATA 0x00000004U
#endif
#ifndef OPEN_ALWAYS
#define OPEN_ALWAYS 4U
#endif
static void x104_platform_settings_append(X104PlatformPropertySet *set, char type, const wchar_t *key, LONG number, const wchar_t *text)
{
    wchar_t path[MAX_PATH];
    char line[1408];
    UINT32 line_length;
    DWORD written = 0;
    HANDLE file;

    if (!set || !set->persist || !set->persist_ready) return;
    if (!x104_platform_settings_path(set, path, MAX_PATH)) return;
    line_length = x104_platform_settings_line(type, key, number, text, line, (UINT32)sizeof(line));
    if (!line_length) return;
    file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        x104_logf("X104 platform: %s: cannot append to %ls (Win32 error %lu)", set->label, set->file, (unsigned long)GetLastError());
        return;
    }
    if (!WriteFile(file, line, line_length, &written, NULL) || written != line_length)
        x104_logf("X104 platform: %s: %ls append failed (Win32 error %lu)", set->label, set->file, (unsigned long)GetLastError());
    CloseHandle(file);
}

static void x104_platform_settings_load(X104PlatformPropertySet *set)
{
    wchar_t path[MAX_PATH];
    HANDLE file;
    DWORD size, read = 0;
    char *bytes = NULL;
    wchar_t *text = NULL;
    wchar_t *line;
    int wide = 0;
    UINT32 loaded = 0;
    UINT32 lines = 0;
    UINT32 index;

    if (!set || !set->persist) return;
    if (!x104_platform_settings_path(set, path, MAX_PATH)) return;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        x104_logf("X104 platform: %s: no %ls yet (first run), the defaults stay", set->label, set->file);
        return;
    }
    size = GetFileSize(file, NULL);
    if (size != 0xFFFFFFFFu && size && size < (1u << 20)) {
        bytes = (char *)HeapAlloc(GetProcessHeap(), 0, size + 1);
        if (bytes && ReadFile(file, bytes, size, &read, NULL)) bytes[read] = 0;
    }
    CloseHandle(file);
    if (bytes && read) {
        wide = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, NULL, 0);
        if (wide > 0) {
            text = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, ((size_t)wide + 1) * sizeof(wchar_t));
            if (text) {
                MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, text, wide);
                text[wide] = 0;
            }
        }
    }
    line = text;
    while (line && *line) {
        wchar_t *end = line;
        wchar_t *key, *value;
        wchar_t type;
        wchar_t saved;
        while (*end && *end != L'\n' && *end != L'\r') ++end;
        saved = *end;
        *end = 0;
        type = *line;
        key = line + 1;
        value = NULL;
        if (type && *key == L'\t') {
            for (++key, value = key; *value && *value != L'\t'; ++value) { }
            if (*value == L'\t') {
                ++lines;
                *value++ = 0;
                if (type == 'c') {
                    set->settings_count = 0;
                } else if (type == 'r') {
                    x104_platform_settings_drop(set, key);
                } else {
                    X104PlatformSetting *entry = x104_platform_settings_add(set, key);
                    if (entry) {
                        entry->type = (char)type;
                        entry->text[0] = 0;
                        if (type == 's') {
                            x104_platform_settings_decode(value, entry->text, (unsigned)ARRAYSIZE(entry->text));
                            entry->number = 0;
                        } else if (type == 'b') {
                            entry->number = value[0] == L'1' ? 1 : 0;
                        } else {
                            entry->number = x104_platform_settings_number(value);
                        }
                    }
                }
            }
        }
        *end = saved;
        line = *end ? end + 1 : end;
    }
    if (text) HeapFree(GetProcessHeap(), 0, text);
    if (bytes) HeapFree(GetProcessHeap(), 0, bytes);
    /* the shadow list is the compacted state: put it into the map, then collapse the log back to one line per key */
    for (index = 0; index < set->settings_count; ++index)
        x104_platform_settings_push(set, &set->settings[index]);
    loaded = set->settings_count;
    if (lines != loaded) x104_platform_settings_save(set);
    x104_logf("X104 platform: %s: loaded %lu persisted settings from %ls (%lu lines)", set->label, (unsigned long)loaded, set->file, (unsigned long)lines);
}

/* Insert: read the boxed value through IPropertyValue and mirror it in the shadow list + file */
static void x104_platform_settings_observe(X104PlatformPropertySet *set, HSTRING key, void *value)
{
    static const GUID iid_iproperty_value = {0x4bd682dd,0x7554,0x40e9,{0x9a,0x9b,0x82,0x65,0x4e,0xde,0x7e,0x62}};
    LPCWSTR raw;
    void *pv = NULL;
    UINT32 type = 0;
    int stored = 0;

    if (!set || !set->persist || !set->persist_ready || !value) return;
    raw = WindowsGetStringRawBuffer(key, NULL);
    if (!raw) return;
    if (FAILED(x104_raw_qi(value, &iid_iproperty_value, &pv)) || !pv) {
        x104_logf("X104 platform: %s: %ls is not an IPropertyValue, it is not persisted", set->label, raw);
        return;
    }
    if (SUCCEEDED(X104_VFN(pv, 6, HRESULT (X104_API *)(void *, UINT32 *))(pv, &type))) {
        switch (type) {
        case 4: {                                       /* Int32 */
            LONG number = 0;
            if (SUCCEEDED(X104_VFN(pv, 11, HRESULT (X104_API *)(void *, LONG *))(pv, &number))) {
                X104PlatformSetting *entry = x104_platform_settings_add(set, raw);
                if (entry) { entry->type = 'i'; entry->number = number; entry->text[0] = 0; stored = 1; }
            }
            break;
        }
        case 5: {                                       /* UInt32 */
            UINT32 number = 0;
            if (SUCCEEDED(X104_VFN(pv, 12, HRESULT (X104_API *)(void *, UINT32 *))(pv, &number))) {
                X104PlatformSetting *entry = x104_platform_settings_add(set, raw);
                if (entry) { entry->type = 'i'; entry->number = (LONG)number; entry->text[0] = 0; stored = 1; }
            }
            break;
        }
        case 11: {                                      /* Boolean */
            BYTE flag = 0;
            if (SUCCEEDED(X104_VFN(pv, 18, HRESULT (X104_API *)(void *, BYTE *))(pv, &flag))) {
                X104PlatformSetting *entry = x104_platform_settings_add(set, raw);
                if (entry) { entry->type = 'b'; entry->number = flag ? 1 : 0; entry->text[0] = 0; stored = 1; }
            }
            break;
        }
        case 12: {                                      /* String */
            HSTRING string = NULL;
            if (SUCCEEDED(X104_VFN(pv, 19, HRESULT (X104_API *)(void *, HSTRING *))(pv, &string)) && string) {
                LPCWSTR content = WindowsGetStringRawBuffer(string, NULL);
                X104PlatformSetting *entry = x104_platform_settings_add(set, raw);
                if (entry) {
                    entry->type = 's';
                    entry->number = 0;
                    lstrcpynW(entry->text, content ? content : L"", (int)ARRAYSIZE(entry->text));
                    stored = 1;
                }
                WindowsDeleteString(string);
            }
            break;
        }
        default:
            x104_logf("X104 platform: %s: %ls has PropertyType %lu, it is not persisted", set->label, raw, (unsigned long)type);
            break;
        }
    }
    x104_raw_release(pv);
    if (stored) {
        X104PlatformSetting *entry = x104_platform_settings_find(set, raw);
        if (entry) x104_platform_settings_append(set, entry->type, entry->key, entry->number, entry->text);
    }
}

static void x104_platform_settings_forget(X104PlatformPropertySet *set, HSTRING key)
{
    LPCWSTR raw;
    if (!set || !set->persist || !set->persist_ready) return;
    raw = WindowsGetStringRawBuffer(key, NULL);
    if (raw) x104_platform_settings_drop(set, raw);
    x104_platform_settings_append(set, 'r', raw ? raw : L"", 0, NULL);
}

static void x104_platform_settings_reset(X104PlatformPropertySet *set)
{
    if (!set || !set->persist || !set->persist_ready) return;
    set->settings_count = 0;
    x104_platform_settings_append(set, 'c', L"", 0, NULL);
}

static void x104_platform_seed_one(X104PlatformPropertySet *set, const wchar_t *key, void *boxed)
{
    HSTRING name = NULL;
    if (boxed && SUCCEEDED(x104_hstring_create(key, &name))) {
        BYTE replaced = 0;
        HRESULT hr = x104_platform_propertyset_insert(x104_object_iface0(&set->base), name, boxed, &replaced);
        if (FAILED(hr)) x104_logf("X104 platform: %s: seeding %ls failed (hr 0x%08lx %s)", set->label, key, (unsigned long)hr, x104_hr_name(hr));
        WindowsDeleteString(name);
    }
}

static X104Object *x104_platform_property_set_create(const char *label)
{
    X104PlatformPropertySet *set = (X104PlatformPropertySet *)x104_object_new(&x104_platform_propertyset_class, sizeof(*set));
    void *factory = NULL;
    HRESULT hr = E_FAIL;
    if (!set) return NULL;
    set->label = label;
    if (x104.original_activation) {
        hr = ((X104ActivationFn)x104.original_activation)(L"Windows.Foundation.Collections.PropertySet", &X104_IID_IActivationFactory, &factory);
        if (SUCCEEDED(hr) && factory) {
            void *instance = NULL;
            hr = X104_VFN(factory, 6, HRESULT (X104_API *)(void *, void **))(factory, &instance);   /* IActivationFactory.ActivateInstance */
            x104_raw_release(factory);
            if (SUCCEEDED(hr) && instance) {
                /* the map methods live on IMap<String,Object>, a separate vtable from the object's IInspectable identity */
                hr = x104_raw_qi(instance, &x104_platform_iid_imap_string_object, &set->inner);
                x104_raw_release(instance);
            }
        }
    }
    x104_logf("X104 platform: %s = logging wrapper around the OS PropertySet %p (hr 0x%08lx %s)", label,
              set->inner, (unsigned long)hr, x104_hr_name(hr));
    if (set->inner) {
        unsigned index;
        /* WP8->WinRT migration flags: an older app version wrote them, so a real device has them; TRUE = "already migrated", i.e. skip the migration. */
        static const wchar_t *const migration_keys[] = { L"WP_MIGRATION", L"WP_MIGRATION_ACHIEVEMENTS" };
        for (index = 0; index < ARRAYSIZE(migration_keys); ++index) {
            void *boxed = x104_platform_boxed_value(1, 1);
            x104_platform_seed_one(set, migration_keys[index], boxed);
        }
        for (index = 0; index < ARRAYSIZE(x104_platform_seeds); ++index) {
            const X104PlatformSeed *seed = &x104_platform_seeds[index];
            void *boxed = seed->kind == X104_SEED_STRING
                ? x104_platform_boxed_string(!lstrcmpW(seed->key, L"language") ? x104_config_language_code() : seed->text)
                : x104_platform_boxed_value(seed->kind == X104_SEED_BOOL, seed->value);
            x104_platform_seed_one(set, seed->key, boxed);
            if (seed->kind == X104_SEED_STRING && boxed) x104_raw_release(boxed);   /* the map holds its own reference */
        }
        x104_logf("X104 platform: %s: seeded %lu settings defaults (int32/bool/string)", label, (unsigned long)ARRAYSIZE(x104_platform_seeds));
    }
    return &set->base;
}

/* turn on the file mirror for an existing map and read what the last run left there */
static void x104_platform_property_set_persist(X104PlatformPropertySet *set, const wchar_t *leaf)
{
    if (!set || !set->inner || !leaf || !leaf[0]) return;
    lstrcpynW(set->file, leaf, (int)ARRAYSIZE(set->file));
    set->persist = 1;
    x104_platform_settings_load(set);
    set->persist_ready = 1;
}

/* lazily created, thread-safe wrapper per container; `leaf` != NULL turns the file mirror on */
static X104Object *x104_platform_property_set_cached(X104Object **slot, volatile LONG *state, const char *label, const wchar_t *leaf)
{
    if (x104_once_begin(state)) {
        X104Object *created = x104_platform_property_set_create(label);
        if (created && leaf) x104_platform_property_set_persist((X104PlatformPropertySet *)created, leaf);
        *slot = created;
        x104_once_end(state);
    }
    return *slot;
}

static X104Object *x104_platform_local_values;
static volatile LONG x104_platform_local_values_state;

static X104Object *x104_platform_container_object(void);   /* the root (LocalSettings) container, created lazily below */

static X104Object *x104_platform_local_property_set(void)
{
    return x104_platform_property_set_cached(&x104_platform_local_values, &x104_platform_local_values_state, "LocalSettings.Values",
                                             L"localsettings.dat");
}

/* One ApplicationDataContainer: LocalSettings itself (empty name, Values = the persisted map) or a named child created through
 * CreateContainer().  On a phone every child has its own Values map, and HCR uses that for its live tiles: it deletes and recreates
 * "TileNotifications" and calls Values.Clear() on it.  The old host returned the LocalSettings container for every name, so that
 * Clear() wiped the whole saved profile (coins, upgrades, achievements). */
typedef struct X104PlatformContainerObject {
    X104Object base;
    X104PlatformPropertySet *values;    /* lazily created; NULL until the first get_Values */
    wchar_t name[96];                   /* empty for LocalSettings */
    char label[192];                    /* property-set log label (must outlive the set) */
    wchar_t file[96];                   /* persistence file leaf for this container's Values */
} X104PlatformContainerObject;

static X104PlatformContainerObject *x104_platform_children[16];
static UINT32 x104_platform_children_count;

static X104PlatformContainerObject *x104_platform_container_new(const wchar_t *name);   /* defined after the class */

static HRESULT X104_API x104_platform_container_get_name(void *self, HSTRING *value)
{
    X104PlatformContainerObject *container = (X104PlatformContainerObject *)X104_OBJ(self);
    if (!value) return E_POINTER;
    *value = NULL;
    if (container && container->name[0]) return x104_hstring_create(container->name, value);
    return S_OK;                                    /* the LocalSettings container has an empty name */
}

static HRESULT X104_API x104_platform_container_get_values(void *self, void **value)
{
    X104PlatformContainerObject *container = (X104PlatformContainerObject *)X104_OBJ(self);
    if (!container) return E_POINTER;
    if (!container->values) {
        if (!container->name[0]) {
            container->values = (X104PlatformPropertySet *)x104_platform_local_property_set();
        } else {
            container->values = (X104PlatformPropertySet *)x104_platform_property_set_create(container->label);
            x104_platform_property_set_persist(container->values, container->file);
        }
    }
    if (!container->values) return E_OUTOFMEMORY;
    return x104_return_iface((X104Object *)container->values, &X104_IID_IInspectable, value,
                             container->name[0] ? container->label : "ApplicationData.LocalSettings.Values");
}

static HRESULT X104_API x104_platform_container_create_container(void *self, HSTRING name, int disposition, void **value)
{
    X104PlatformContainerObject *container;
    LPCWSTR raw;
    char shown[128];
    UINT32 index;
    (void)self;
    (void)disposition;                              /* Always / OpenIfExists / ReplaceExisting: the map is created on demand anyway */
    if (!value) return E_POINTER;
    *value = NULL;
    raw = WindowsGetStringRawBuffer(name, NULL);
    if (!raw) raw = L"";
    x104_narrow(shown, sizeof(shown), raw);
    for (index = 0; index < x104_platform_children_count; ++index) {
        if (lstrcmpW(x104_platform_children[index]->name, raw)) continue;
        container = x104_platform_children[index];
        x104_logf("X104 platform: ApplicationDataContainer.CreateContainer(%s) -> existing child container with its own Values", shown);
        return x104_return_iface(&container->base, &X104_IID_IApplicationDataContainer, value, "ApplicationDataContainer.CreateContainer");
    }
    container = x104_platform_container_new(raw);
    if (!container) return E_OUTOFMEMORY;
    x104_logf("X104 platform: ApplicationDataContainer.CreateContainer(%s) -> new child container with its own Values", shown);
    return x104_return_iface(&container->base, &X104_IID_IApplicationDataContainer, value, "ApplicationDataContainer.CreateContainer");
}

static HRESULT X104_API x104_platform_container_delete_container(void *self, HSTRING name)
{
    LPCWSTR raw = WindowsGetStringRawBuffer(name, NULL);
    char shown[128];
    UINT32 index;
    (void)self;
    if (!raw) return S_OK;
    x104_narrow(shown, sizeof(shown), raw);
    for (index = 0; index < x104_platform_children_count; ++index) {
        if (lstrcmpW(x104_platform_children[index]->name, raw)) continue;
        {
            wchar_t path[MAX_PATH];
            if (x104_platform_settings_leaf_path(x104_platform_children[index]->file, path, MAX_PATH))
                DeleteFileW(path);              /* a real DeleteContainer drops the values too */
        }
        for (; index + 1 < x104_platform_children_count; ++index)
            x104_platform_children[index] = x104_platform_children[index + 1];
        --x104_platform_children_count;
        x104_logf("X104 platform: ApplicationDataContainer.DeleteContainer(%s) -> dropped from the registry", shown);
        return S_OK;
    }
    x104_logf("X104 platform: ApplicationDataContainer.DeleteContainer(%s) -> S_OK (no such child container)", shown);
    return S_OK;
}

X104_VTBL(x104_platform_vt_container, IApplicationDataContainer) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationDataContainer, get_Name, x104_platform_container_get_name),
    X104_SLOT(IApplicationDataContainer, get_Values, x104_platform_container_get_values),
    X104_SLOT(IApplicationDataContainer, CreateContainer, x104_platform_container_create_container),
    X104_SLOT(IApplicationDataContainer, DeleteContainer, x104_platform_container_delete_container)
};
static const X104IfaceDesc x104_platform_container_ifaces[] = { X104_IFACE(IApplicationDataContainer, x104_platform_vt_container) };
X104_CLASS_DEF(x104_platform_container_class, "ApplicationDataContainer", L"Windows.Storage.ApplicationDataContainer", x104_platform_container_ifaces,
               X104_CLASS_AGILE, NULL);

static X104PlatformContainerObject *x104_platform_container_new(const wchar_t *name)
{
    X104PlatformContainerObject *container = (X104PlatformContainerObject *)x104_object_new(&x104_platform_container_class, sizeof(*container));
    char ascii[96];
    int index;
    if (!container) return NULL;
    lstrcpynW(container->name, name, (int)ARRAYSIZE(container->name));
    for (index = 0; name[index] && index < (int)sizeof(ascii) - 1; ++index)
        ascii[index] = (name[index] >= 0x20 && name[index] < 0x7f) ? (char)name[index] : '?';
    ascii[index] = 0;
    wsprintfA(container->label, "LocalSettings.Children[%s].Values", ascii);
    wsprintfW(container->file, L"localsettings_%s.dat", name);
    if (x104_platform_children_count < ARRAYSIZE(x104_platform_children))
        x104_platform_children[x104_platform_children_count++] = container;
    return container;
}

static X104PlatformContainerObject *x104_platform_root_container;
static volatile LONG x104_platform_root_state;

static X104Object *x104_platform_container_object(void)
{
    if (x104_once_begin(&x104_platform_root_state)) {
        x104_platform_root_container = (X104PlatformContainerObject *)x104_object_new(&x104_platform_container_class, sizeof(*x104_platform_root_container));
        x104_once_end(&x104_platform_root_state);
    }
    return x104_platform_root_container ? &x104_platform_root_container->base : NULL;
}

static HRESULT X104_API x104_platform_appdata_get_local_settings(void *self, void **value)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 4, "X104 platform: ApplicationData.Current.LocalSettings -> container object (non-NULL: the Launched/Suspending handlers dereference it)");
    return x104_return_iface(x104_platform_container_object(), &X104_IID_IApplicationDataContainer, value, "ApplicationData.LocalSettings");
}

/* RoamingSettings (REDCON 1 keeps string key/values there: games_recon/redcon_redcon-platform.md F9).  Values = the logging IPropertySet wrapper around a real
 * Windows.Foundation.Collections.PropertySet from the OS runtime, created once; in memory only (not persisted between runs yet). */
static X104Object *x104_platform_roaming_values;
static volatile LONG x104_platform_roaming_values_state;

static HRESULT X104_API x104_platform_roaming_get_values(void *self, void **value)
{
    (void)self;
    return x104_return_iface(x104_platform_property_set_cached(&x104_platform_roaming_values, &x104_platform_roaming_values_state,
                                                               "RoamingSettings.Values", NULL),
                             &X104_IID_IInspectable, value, "ApplicationData.RoamingSettings.Values");
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

/* WP8 keeps the roaming state in a separate folder; the host has one data root, so both folders point at it (the game only reads what it wrote) */
static HRESULT X104_API x104_platform_appdata_get_roaming_folder(void *self, void **value)
{
    static volatile LONG logged;
    char shown[MAX_PATH];
    HRESULT hr;
    (void)self;
    hr = x104_return_iface(x104_platform_folder_object(1), &X104_IID_IStorageFolder, value, "ApplicationData.RoamingFolder");
    x104_narrow(shown, sizeof(shown), x104.data_root);
    X104_LOG_LIMITED(&logged, 4, "X104 platform: ApplicationData.Current.RoamingFolder.Path -> %s (hr 0x%08lx)", shown, (unsigned long)hr);
    return hr;
}

X104_VTBL(x104_platform_vt_appdata, IApplicationData) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IApplicationData, get_LocalSettings, x104_platform_appdata_get_local_settings),
    X104_SLOT(IApplicationData, get_RoamingSettings, x104_platform_appdata_get_roaming_settings),
    X104_SLOT(IApplicationData, get_LocalFolder, x104_platform_appdata_get_local_folder),
    X104_SLOT(IApplicationData, get_RoamingFolder, x104_platform_appdata_get_roaming_folder)
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
static HRESULT X104_API x104_platform_emptyvec_get_at(void *self, ULONG index, void **item)
{
    (void)self;
    (void)index;
    if (item) *item = NULL;
    return X104_E_BOUNDS;
}

static HRESULT X104_API x104_platform_emptyvec_get_size(void *self, ULONG *size)
{
    (void)self;
    if (!size) return E_POINTER;
    *size = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_emptyvec_index_of(void *self, void *item, ULONG *index, BYTE *found)
{
    (void)self;
    (void)item;
    if (index) *index = 0;
    if (found) *found = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_emptyvec_get_many(void *self, ULONG start, ULONG capacity, void **items, ULONG *actual)
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

static HRESULT X104_API x104_platform_licmap_get_size(void *self, ULONG *size)
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
/* ---- pending IAsyncOperation<IVectorView<UnfulfilledConsumable>>: the result of GetUnfulfilledConsumablesAsync (slot 9 of
 *      ICurrentAppWithConsumables, called from the game's Page Loaded handler).  The game attaches a continuation and does not block on
 *      the operation, so it may stay Started for ever: put_Completed only logs (the handler is never fired), get_Status reports Started,
 *      GetResults refuses with E_ILLEGAL_METHOD_CALL (the WinRT rule for an operation that has not completed), Cancel/Close are no-ops.
 *      The parameterized IID is SHA-1("pinterface({9fc2b0bb-e446-44e2-aa61-9cab8f636af2};pinterface({bbe1fa4c-b0e3-4583-baef-1f1b2e483e56};rc(Windows.ApplicationModel.Store.UnfulfilledConsumable;{2df7fbbb-1cdd-4cb8-a014-7b9cf8986927})))")
 *      over the WinRT pinterface namespace GUID; the value was produced by C++/WinRT guid_of<> for that exact type. */
static const GUID x104_platform_iid_asyncop_unfulfilled = {0xf1cd679a,0x3a8f,0x5e1b,{0x82,0xf1,0xf2,0xfd,0x09,0x16,0xca,0x3f}};

typedef struct x104_platform_pending_op {
    X104Object base;                     /* views[0] = IAsyncOperation<...>, views[1] = IAsyncInfo */
    LONG id;
} x104_platform_pending_op;

static HRESULT X104_API x104_platform_pending_put_completed(void *self, void *handler)
{
    static volatile LONG logged;
    (void)self; (void)handler;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: GetUnfulfilledConsumablesAsync operation: put_Completed -> S_OK (pending for ever, the handler is never fired)");
    return S_OK;
}

static HRESULT X104_API x104_platform_pending_get_completed(void *self, void **handler)
{
    (void)self;
    if (!handler) return E_POINTER;
    *handler = NULL;
    return S_OK;
}

static HRESULT X104_API x104_platform_pending_get_results(void *self, void *results)
{
    static volatile LONG logged;
    (void)self;
    if (results) *(void **)results = NULL;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: GetUnfulfilledConsumablesAsync operation: GetResults while Started -> E_ILLEGAL_METHOD_CALL");
    return (HRESULT)0x8000000Eu;
}

static HRESULT X104_API x104_platform_pending_info_get_id(void *self, ULONG *id)
{
    x104_platform_pending_op *op = X104_THIS(x104_platform_pending_op, self);
    if (!id) return E_POINTER;
    *id = (ULONG)op->id;
    return S_OK;
}

static HRESULT X104_API x104_platform_pending_info_get_status(void *self, int *status)
{
    (void)self;
    if (!status) return E_POINTER;
    *status = 0;                                        /* AsyncStatus::Started */
    return S_OK;
}

static HRESULT X104_API x104_platform_pending_info_get_error(void *self, HRESULT *hr)
{
    (void)self;
    if (!hr) return E_POINTER;
    *hr = S_OK;
    return S_OK;
}

static HRESULT X104_API x104_platform_pending_info_cancel(void *self)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 3, "X104 platform: GetUnfulfilledConsumablesAsync operation: Cancel -> S_OK (stays Started)");
    return S_OK;
}

static HRESULT X104_API x104_platform_pending_info_close(void *self)
{
    (void)self;
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_pending_op, 9) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_pending_put_completed,
    [7] = (const void *)x104_platform_pending_get_completed,
    [8] = (const void *)x104_platform_pending_get_results
};
X104_VTBL(x104_platform_vt_pending_info, IAsyncInfo) = {
    X104_INSPECTABLE_SLOTS,
    X104_SLOT(IAsyncInfo, get_Id, x104_platform_pending_info_get_id),
    X104_SLOT(IAsyncInfo, get_Status, x104_platform_pending_info_get_status),
    X104_SLOT(IAsyncInfo, get_ErrorCode, x104_platform_pending_info_get_error),
    X104_SLOT(IAsyncInfo, Cancel, x104_platform_pending_info_cancel),
    X104_SLOT(IAsyncInfo, Close, x104_platform_pending_info_close)
};
static const X104IfaceDesc x104_platform_pending_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_asyncop_unfulfilled, "IAsyncOperation<IVectorView<UnfulfilledConsumable>>", x104_platform_vt_pending_op),
    X104_IFACE(IAsyncInfo, x104_platform_vt_pending_info)
};
X104_CLASS_DEF(x104_platform_pending_class, "UnfulfilledConsumablesOperation",
               L"Windows.Foundation.IAsyncOperation`1<Windows.Foundation.Collections.IVectorView`1<Windows.ApplicationModel.Store.UnfulfilledConsumable>>",
               x104_platform_pending_ifaces, X104_CLASS_AGILE, NULL);

/* ICurrentAppWithConsumables slot 9: hand the game a fresh, never-completing operation */
static HRESULT X104_API x104_platform_currentapp_get_unfulfilled(void *self, void **operation)
{
    static volatile LONG serial;
    x104_platform_pending_op *op;
    (void)self;
    if (!operation) return E_POINTER;
    *operation = NULL;
    op = (x104_platform_pending_op *)x104_object_new(&x104_platform_pending_class, sizeof(*op));
    if (!op) return E_OUTOFMEMORY;
    op->id = InterlockedIncrement(&serial);
    *operation = x104_object_iface0(&op->base);
    x104_logf("X104 platform: CurrentApp.GetUnfulfilledConsumablesAsync -> pending IAsyncOperation %p (id %ld, serial %ld; stays Started, the continuation never runs)",
              *operation, (long)op->id, (long)op->base.serial);
    return S_OK;
}

/* ICurrentAppWithConsumables {844E0071-9E4F-4F79-995A-5F91172E6CEF}: the game QIs CurrentApp for it right after the Page Loaded
 * handler (a C++/CX safe_cast, so the QI itself must succeed).  Only slot 9 (GetUnfulfilledConsumablesAsync) is reached on the boot path;
 * the other three slots stay holes. */
static const GUID x104_platform_iid_currentapp_consumables = {0x844e0071,0x9e4f,0x4f79,{0x99,0x5a,0x5f,0x91,0x17,0x2e,0x6c,0xef}};
X104_VTBL_N(x104_platform_vt_currentapp_consumables, 10) = {
    X104_INSPECTABLE_SLOTS,
    [9] = (const void *)x104_platform_currentapp_get_unfulfilled
};
static const X104IfaceDesc x104_platform_currentapp_ifaces[] = {
    X104_IFACE(ICurrentApp, x104_platform_vt_currentapp),
    X104_IFACE_IID(&x104_platform_iid_currentapp_consumables, "ICurrentAppWithConsumables", x104_platform_vt_currentapp_consumables)
};
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
 * 12b. Windows.Storage.Compression.Decompressor: HCR reads its save through it
 *      GetFileAsync -> OpenReadAsync -> GetInputStreamAt(0) -> CreateDecompressor -> ReadAsync.
 *      The host has no MS-XCA (XPRESS Huffman) codec, so the decompressor hands the bytes of the underlying stream
 *      through unchanged and the game's own Compressor does the same on the way back, so its saves round-trip inside
 *      the host.  A save file copied from a real device would not decode here - no M1 need for that.
 * ====================================================================================================================== */
static const GUID x104_platform_iid_idecompressor_factory = { 0x5337e252, 0x1da2, 0x42e1, { 0x88, 0x34, 0x03, 0x79, 0xd2, 0x8d, 0x74, 0x2f } };
/* private IID for the instance view: the genuine IDecompressor IID is not in the facts, so a QI for it shows up as a QI MISS line */
static const GUID x104_platform_iid_idecompressor = { 0x5337e253, 0x1da2, 0x42e1, { 0x88, 0x34, 0x03, 0x79, 0xd2, 0x8d, 0x74, 0x2f } };

typedef struct X104PlatformDecompressor {
    X104Object base;
    void *underlying;               /* IInputStream view of the game's stream (AddRef'd), NULL after DetachStream/Close */
    volatile LONG reads;
} X104PlatformDecompressor;

/* IInputStream::ReadAsync is slot 6 (IInputStream derives from IInspectable only); the operation the stream returns is already the
 * IAsyncOperationWithProgress<IBuffer, UINT32> the decompressor has to return, so it is passed straight through */
static HRESULT X104_API x104_platform_decompressor_read_async(void *self, void *buffer, UINT32 count, int options, void **operation)
{
    X104PlatformDecompressor *dec = X104_THIS(X104PlatformDecompressor, self);
    HRESULT hr;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!dec->underlying) {
        x104_logf("X104 platform: Decompressor.ReadAsync on a closed/detached decompressor -> E_ILLEGAL_METHOD_CALL");
        return 0x8000000Eu; /* E_ILLEGAL_METHOD_CALL */
    }
    hr = ((HRESULT (X104_API *)(void *, void *, UINT32, int, void **))((void **)*(void ***)dec->underlying)[6])(dec->underlying, buffer, count, options, operation);
    X104_LOG_LIMITED(&dec->reads, 6, "X104 platform: Decompressor.ReadAsync(count %lu, options %d) -> delegated to the underlying stream: 0x%08lx %s (pass-through, no codec)",
                     (unsigned long)count, options, (unsigned long)hr, x104_hr_name(hr));
    return hr;
}

static HRESULT X104_API x104_platform_decompressor_detach(void *self, void **stream)
{
    X104PlatformDecompressor *dec = X104_THIS(X104PlatformDecompressor, self);
    if (!stream) return E_POINTER;
    *stream = dec->underlying;
    dec->underlying = NULL;                       /* the reference moves to the caller */
    x104_logf("X104 platform: Decompressor.DetachStream -> %p", *stream);
    return S_OK;
}

static HRESULT X104_API x104_platform_decompressor_close(void *self)
{
    X104PlatformDecompressor *dec = X104_THIS(X104PlatformDecompressor, self);
    if (dec->underlying) {
        x104_raw_release(dec->underlying);
        dec->underlying = NULL;
    }
    return S_OK;
}

/* IDecompressor requires IClosable: slots 0..5 IInspectable, 6 Close, 7 ReadAsync, 8 DetachStream */
X104_VTBL_N(x104_platform_vt_decompressor, 9) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_decompressor_close,
    [7] = (const void *)x104_platform_decompressor_read_async,
    [8] = (const void *)x104_platform_decompressor_detach
};
X104_VTBL_N(x104_platform_vt_decompressor_closable, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_decompressor_close
};
/* HCR does not QI IDecompressor: it uses the object as an IInputStream (905A0FE2-...) and calls ReadAsync on it */
X104_VTBL_N(x104_platform_vt_decompressor_input, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_decompressor_read_async
};
static const X104IfaceDesc x104_platform_decompressor_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_idecompressor, "IDecompressor", x104_platform_vt_decompressor),
    X104_IFACE_IID(&x104_platform_iid_iinputstream, "IInputStream", x104_platform_vt_decompressor_input),
    X104_IFACE_IID(&x104_platform_iid_iclosable, "IClosable", x104_platform_vt_decompressor_closable)
};
X104_CLASS_DEF(x104_platform_decompressor_class, "Decompressor", L"Windows.Storage.Compression.Decompressor", x104_platform_decompressor_ifaces,
               X104_CLASS_AGILE, NULL);

/* IDecompressorFactory::CreateDecompressor(IInputStream underlyingStream) */
static HRESULT X104_API x104_platform_decompressor_create(void *self, void *underlying, void **value)
{
    X104PlatformDecompressor *dec;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!underlying) return E_POINTER;
    dec = (X104PlatformDecompressor *)x104_object_new(&x104_platform_decompressor_class, sizeof(*dec));
    if (!dec) return E_OUTOFMEMORY;
    dec->underlying = underlying;
    x104_raw_addref(underlying);
    *value = x104_object_iface0(&dec->base);
    x104_logf("X104 platform: CreateDecompressor(stream %p) -> %p (pass-through: the save bytes are handed to the game unchanged)", underlying, *value);
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_decompressor_factory, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_decompressor_create
};
static const X104IfaceDesc x104_platform_decompressor_factory_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_idecompressor_factory, "IDecompressorFactory", x104_platform_vt_decompressor_factory)
};
X104_CLASS_DEF(x104_platform_decompressor_factory_class, "DecompressorFactory", L"Windows.Storage.Compression.Decompressor",
               x104_platform_decompressor_factory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_decompressor_statics, x104_platform_decompressor_factory_class, X104Object)

/* ======================================================================================================================
 * 12b-2. Windows.Storage.Compression.Compressor: the save-write path
 *      CreateFileAsync -> OpenAsync(ReadWrite) -> GetOutputStreamAt(0) -> CreateCompressor -> DataWriter -> StoreAsync ->
 *      Compressor.FinishAsync -> stream.FlushAsync.  Wine has no MS-XCA (XPRESS Huffman) encoder either, so the compressor
 *      is a pass-through: the bytes the DataWriter stores go straight into the underlying stream and FinishAsync only
 *      flushes it.  The file therefore holds exactly what the decompressor hands back on the next run.
 * ====================================================================================================================== */
static const GUID x104_platform_iid_icompressor = { 0x0ac3645a, 0x57ac, 0x4ee1, { 0xb7, 0x02, 0x84, 0xd3, 0x9d, 0x54, 0x24, 0xe0 } };
static const GUID x104_platform_iid_icompressor_factory = { 0x5f3d96a4, 0x2cfb, 0x442c, { 0xa8, 0xba, 0xd7, 0xd1, 0x1b, 0x03, 0x9d, 0xa0 } };

typedef struct X104PlatformCompressor {
    X104Object base;
    void *underlying;               /* IOutputStream view (AddRef'd), NULL after DetachStream/Close */
    X104PlatformStream *target;     /* the host stream behind `underlying` (no extra reference: `underlying` owns it) */
    volatile LONG writes;
} X104PlatformCompressor;

/* map an IOutputStream view back to the host stream that really holds the bytes: the stream itself, or the stream a
 * pass-through compressor forwards to.  NULL for a foreign implementation (the host never gets one from HCR). */
static X104PlatformStream *x104_platform_output_target(void *view);

static HRESULT X104_API x104_platform_compressor_write_async(void *self, void *buffer, void **operation)
{
    X104PlatformCompressor *comp = X104_THIS(X104PlatformCompressor, self);
    HRESULT hr;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!comp->underlying) return 0x8000000Eu; /* E_ILLEGAL_METHOD_CALL */
    hr = ((HRESULT (X104_API *)(void *, void *, void **))((void **)*(void ***)comp->underlying)[6])(comp->underlying, buffer, operation);
    X104_LOG_LIMITED(&comp->writes, 6, "X104 platform: Compressor.WriteAsync -> delegated to the underlying stream: 0x%08lx %s", (unsigned long)hr,
                     x104_hr_name(hr));
    return hr;
}

static HRESULT X104_API x104_platform_compressor_flush_async(void *self, void **operation)
{
    X104PlatformCompressor *comp = X104_THIS(X104PlatformCompressor, self);
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!comp->underlying) return 0x8000000Eu;
    return ((HRESULT (X104_API *)(void *, void **))((void **)*(void ***)comp->underlying)[7])(comp->underlying, operation);
}

/* ICompressor.FinishAsync: there is no codec to flush, so the underlying stream is saved and TRUE is reported */
static HRESULT X104_API x104_platform_compressor_finish_async(void *self, void **operation)
{
    X104PlatformCompressor *comp = X104_THIS(X104PlatformCompressor, self);
    void *result;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (comp->target) x104_platform_stream_save_file(comp->target);
    result = x104_platform_opbool_new(1);
    if (!result) return E_OUTOFMEMORY;
    *operation = result;
    x104_logf("X104 platform: Compressor.FinishAsync -> completed operation %p (TRUE, pass-through: %lu bytes)",
              *operation, comp->target ? (unsigned long)comp->target->size : 0ul);
    return S_OK;
}

static HRESULT X104_API x104_platform_compressor_detach(void *self, void **stream)
{
    X104PlatformCompressor *comp = X104_THIS(X104PlatformCompressor, self);
    if (!stream) return E_POINTER;
    *stream = comp->underlying;
    comp->underlying = NULL;
    comp->target = NULL;
    x104_logf("X104 platform: Compressor.DetachStream -> %p", *stream);
    return S_OK;
}

static HRESULT X104_API x104_platform_compressor_close(void *self)
{
    X104PlatformCompressor *comp = X104_THIS(X104PlatformCompressor, self);
    if (comp->underlying) {
        x104_raw_release(comp->underlying);
        comp->underlying = NULL;
    }
    comp->target = NULL;
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_compressor, 8) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_compressor_finish_async,
    [7] = (const void *)x104_platform_compressor_detach
};
X104_VTBL_N(x104_platform_vt_compressor_output, 8) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_compressor_write_async,
    [7] = (const void *)x104_platform_compressor_flush_async
};
X104_VTBL_N(x104_platform_vt_compressor_closable, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_compressor_close
};
static const X104IfaceDesc x104_platform_compressor_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_icompressor, "ICompressor", x104_platform_vt_compressor),
    X104_IFACE_IID(&x104_platform_iid_ioutputstream, "IOutputStream", x104_platform_vt_compressor_output),
    X104_IFACE_IID(&x104_platform_iid_iclosable, "IClosable", x104_platform_vt_compressor_closable)
};
X104_CLASS_DEF(x104_platform_compressor_class, "Compressor", L"Windows.Storage.Compression.Compressor", x104_platform_compressor_ifaces,
               X104_CLASS_AGILE, NULL);

static X104PlatformStream *x104_platform_output_target(void *view)
{
    X104Object *object;
    if (!view) return NULL;
    object = X104_OBJ(view);
    if (object->cls == &x104_platform_stream_class) return (X104PlatformStream *)object;
    if (object->cls == &x104_platform_compressor_class) return ((X104PlatformCompressor *)object)->target;
    return NULL;
}

/* ICompressorFactory::CreateCompressor(IOutputStream createdCompressor) */
static HRESULT X104_API x104_platform_compressor_create(void *self, void *underlying, void **value)
{
    X104PlatformCompressor *comp;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!underlying) return E_POINTER;
    comp = (X104PlatformCompressor *)x104_object_new(&x104_platform_compressor_class, sizeof(*comp));
    if (!comp) return E_OUTOFMEMORY;
    comp->underlying = underlying;
    x104_raw_addref(underlying);
    comp->target = x104_platform_output_target(underlying);
    *value = x104_object_iface0(&comp->base);
    x104_logf("X104 platform: CreateCompressor(stream %p) -> %p (pass-through%s)", underlying, *value,
              comp->target ? "" : "; the stream is not a host stream, writes cannot be stored");
    return S_OK;
}

/* ICompressorFactory::CreateCompressorEx(IOutputStream, CompressAlgorithm, UINT32 blockSize, ICompressor**) - the C++/CX
 * constructor `ref new Compressor(stream, algorithm, blockSize)` lands here.  All four arguments must be kept: mapping the
 * slot onto the 3-argument CreateCompressor read the algorithm as the out pointer (access violation), and leaving blockSize
 * out of the signature made the block size double as the out pointer (E_POINTER, i.e. Platform::NullReferenceException). */
static HRESULT X104_API x104_platform_compressor_create_ex(void *self, void *underlying, UINT32 algorithm, UINT32 block_size, void **value)
{
    (void)algorithm;                /* no MS-XCA codec in the host: every algorithm is a pass-through */
    (void)block_size;
    return x104_platform_compressor_create(self, underlying, value);
}

X104_VTBL_N(x104_platform_vt_compressor_factory, 8) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_compressor_create,
    [7] = (const void *)x104_platform_compressor_create_ex
};
static const X104IfaceDesc x104_platform_compressor_factory_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_icompressor_factory, "ICompressorFactory", x104_platform_vt_compressor_factory)
};
X104_CLASS_DEF(x104_platform_compressor_factory_class, "CompressorFactory", L"Windows.Storage.Compression.Compressor",
               x104_platform_compressor_factory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_compressor_statics, x104_platform_compressor_factory_class, X104Object)

/* ======================================================================================================================
 * 12b-3. Windows.Storage.Streams.DataWriter: HCR serialises its save with one
 *      `DataWriter(compressor); writer->WriteInt32(...); writer->StoreAsync(); writer->FlushAsync();`
 *      The writer collects the bytes in memory and StoreAsync pushes them into the host stream behind the compressor in one
 *      go (no IBuffer is created, which keeps the host's IBuffer surface at "whatever the game passes in").
 *      The 1.27 save code instead creates the writer with its default constructor, fills it, DetachBuffer()s the result and
 *      hands that buffer to IOutputStream.WriteAsync, so the host also owns a throwaway IBuffer implementation.
 * ====================================================================================================================== */
static const GUID x104_platform_iid_ibuffer = { 0x905a0fe0, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };

typedef struct X104PlatformBuffer {
    X104Object base;
    BYTE *data;                     /* owned */
    UINT32 size;
    UINT32 capacity;
} X104PlatformBuffer;

static HRESULT X104_API x104_platform_ibuffer_get_capacity(void *self, UINT32 *value)
{
    X104PlatformBuffer *buffer = X104_THIS(X104PlatformBuffer, self);
    if (!value) return E_POINTER;
    *value = buffer->capacity;
    return S_OK;
}

static HRESULT X104_API x104_platform_ibuffer_get_length(void *self, UINT32 *value)
{
    X104PlatformBuffer *buffer = X104_THIS(X104PlatformBuffer, self);
    if (!value) return E_POINTER;
    *value = buffer->size;
    return S_OK;
}

static HRESULT X104_API x104_platform_ibuffer_put_length(void *self, UINT32 length)
{
    X104PlatformBuffer *buffer = X104_THIS(X104PlatformBuffer, self);
    if (length > buffer->capacity) {
        BYTE *grown = x104_platform_grow(buffer->data, length);
        if (!grown) return E_OUTOFMEMORY;
        buffer->data = grown;
        buffer->capacity = length;
    }
    if (buffer->data && length > buffer->size) memset(buffer->data + buffer->size, 0, length - buffer->size);
    buffer->size = length;
    return S_OK;
}

static HRESULT X104_API x104_platform_ibuffer_byte_access(void *self, BYTE **bytes)
{
    X104PlatformBuffer *buffer = X104_THIS(X104PlatformBuffer, self);
    if (!bytes) return E_POINTER;
    *bytes = buffer->data;
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_ibuffer, 9) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_ibuffer_get_capacity,
    [7] = (const void *)x104_platform_ibuffer_get_length,
    [8] = (const void *)x104_platform_ibuffer_put_length
};
X104_VTBL_N(x104_platform_vt_ibuffer_byte_access, 4) = {
    X104_INSPECTABLE_SLOTS,
    [3] = (const void *)x104_platform_ibuffer_byte_access
};
static const X104IfaceDesc x104_platform_ibuffer_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_ibuffer, "IBuffer", x104_platform_vt_ibuffer),
    X104_IFACE_IID(&x104_platform_iid_ibufferbyteaccess, "IBufferByteAccess", x104_platform_vt_ibuffer_byte_access)
};
X104_CLASS_DEF(x104_platform_buffer_class, "Buffer", L"Windows.Storage.Streams.Buffer", x104_platform_ibuffer_ifaces, X104_CLASS_AGILE, NULL);

static const GUID x104_platform_iid_idatawriter = { 0x64b89265, 0xd341, 0x4922, { 0xb3, 0x8a, 0xdd, 0x4a, 0xf8, 0x80, 0x8c, 0x4e } };
static const GUID x104_platform_iid_idatawriter_factory = { 0x338c67c2, 0x8b84, 0x4c2b, { 0x9c, 0x50, 0x7b, 0x87, 0x67, 0x84, 0x7a, 0x1f } };

typedef struct X104PlatformDataWriter {
    X104Object base;
    BYTE *data;                 /* owned; the bytes not stored yet */
    UINT32 size;
    UINT32 capacity;
    UINT32 unicode;             /* UnicodeEncoding: 0 = Utf8, 1 = Utf16LE, 2 = Utf16BE */
    UINT32 byte_order;          /* ByteOrder: 0 = LittleEndian, 1 = BigEndian */
    void *stream;               /* IOutputStream view (AddRef'd), NULL after DetachStream */
    X104PlatformStream *target; /* the host stream behind `stream` (no extra reference) */
    volatile LONG calls;
} X104PlatformDataWriter;

static HRESULT x104_platform_writer_reserve(X104PlatformDataWriter *writer, UINT32 extra)
{
    UINT32 wanted;
    BYTE *grown;
    if (extra > 0xFFFFFFFFu - writer->size) return E_OUTOFMEMORY;
    wanted = writer->size + extra;
    if (wanted <= writer->capacity) return S_OK;
    wanted = wanted + 4096;
    grown = x104_platform_grow(writer->data, wanted);
    if (!grown) return E_OUTOFMEMORY;
    writer->data = grown;
    writer->capacity = wanted;
    return S_OK;
}

static HRESULT x104_platform_writer_append(X104PlatformDataWriter *writer, const BYTE *bytes, UINT32 count)
{
    HRESULT hr;
    if (!count) return S_OK;
    if (!bytes) return E_POINTER;
    hr = x104_platform_writer_reserve(writer, count);
    if (FAILED(hr)) return hr;
    memcpy(writer->data + writer->size, bytes, count);
    writer->size += count;
    return S_OK;
}

/* store one integer of `count` bytes, reversed for ByteOrder::BigEndian */
static HRESULT x104_platform_writer_scalar(X104PlatformDataWriter *writer, const BYTE *bytes, UINT32 count)
{
    BYTE reversed[8];
    UINT32 index;
    if (writer->byte_order) {
        for (index = 0; index < count; ++index) reversed[index] = bytes[count - 1 - index];
        return x104_platform_writer_append(writer, reversed, count);
    }
    return x104_platform_writer_append(writer, bytes, count);
}

static HRESULT X104_API x104_platform_writer_get_unstored(void *self, UINT32 *value)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    if (!value) return E_POINTER;
    *value = writer->size;
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_get_unicode(void *self, UINT32 *value)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    if (!value) return E_POINTER;
    *value = writer->unicode;
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_put_unicode(void *self, UINT32 value)
{
    X104_THIS(X104PlatformDataWriter, self)->unicode = value;
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_get_byte_order(void *self, UINT32 *value)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    if (!value) return E_POINTER;
    *value = writer->byte_order;
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_put_byte_order(void *self, UINT32 value)
{
    X104_THIS(X104PlatformDataWriter, self)->byte_order = value;
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_write_byte(void *self, BYTE value)
{
    return x104_platform_writer_append(X104_THIS(X104PlatformDataWriter, self), &value, 1);
}

static HRESULT X104_API x104_platform_writer_write_bytes(void *self, BYTE *value, UINT32 count)
{
    return x104_platform_writer_append(X104_THIS(X104PlatformDataWriter, self), value, count);
}

static HRESULT X104_API x104_platform_writer_write_buffer(void *self, void *buffer)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    BYTE *bytes = NULL;
    UINT32 length = 0;
    HRESULT hr;
    if (!buffer) return E_POINTER;
    hr = x104_platform_buffer_get_length(buffer, &length);
    if (FAILED(hr)) return hr;
    if (!length) return S_OK;
    hr = x104_platform_buffer_bytes(buffer, &bytes);
    if (FAILED(hr)) return hr;
    return x104_platform_writer_append(writer, bytes, length);
}

static HRESULT X104_API x104_platform_writer_write_buffer_range(void *self, void *buffer, UINT32 start, UINT32 count)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    BYTE *bytes = NULL;
    UINT32 length = 0;
    HRESULT hr;
    if (!buffer) return E_POINTER;
    hr = x104_platform_buffer_get_length(buffer, &length);
    if (FAILED(hr)) return hr;
    if (!count) return S_OK;
    if (start > length || count > length - start) return 0x8000000Bu; /* E_BOUNDS */
    hr = x104_platform_buffer_bytes(buffer, &bytes);
    if (FAILED(hr)) return hr;
    return x104_platform_writer_append(writer, bytes + start, count);
}

#define X104_PLATFORM_WRITER_SCALAR(fn_name, ctype)                                                                     \
    static HRESULT X104_API x104_platform_writer_##fn_name(void *self, ctype value)                                     \
    {                                                                                                                   \
        return x104_platform_writer_scalar(X104_THIS(X104PlatformDataWriter, self), (const BYTE *)&value,                \
                                           (UINT32)sizeof(value));                                                      \
    }

X104_PLATFORM_WRITER_SCALAR(write_boolean, BYTE)
X104_PLATFORM_WRITER_SCALAR(write_int16, SHORT)
X104_PLATFORM_WRITER_SCALAR(write_int32, LONG)
X104_PLATFORM_WRITER_SCALAR(write_int64, LONGLONG)
X104_PLATFORM_WRITER_SCALAR(write_uint16, USHORT)
X104_PLATFORM_WRITER_SCALAR(write_uint32, UINT32)
X104_PLATFORM_WRITER_SCALAR(write_uint64, UINT64)
X104_PLATFORM_WRITER_SCALAR(write_single, float)
X104_PLATFORM_WRITER_SCALAR(write_double, double)
X104_PLATFORM_WRITER_SCALAR(write_datetime, LONGLONG)
X104_PLATFORM_WRITER_SCALAR(write_timespan, LONGLONG)

static HRESULT X104_API x104_platform_writer_write_guid(void *self, GUID value)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    BYTE bytes[16];
    const BYTE *raw = (const BYTE *)&value;
    UINT32 index;
    if (!writer->byte_order) return x104_platform_writer_append(writer, raw, 16);
    for (index = 0; index < 16; ++index) bytes[index] = raw[15 - index];
    return x104_platform_writer_append(writer, bytes, 16);
}

/* WriteString returns the number of bytes (not code units) stored */
static HRESULT X104_API x104_platform_writer_write_string(void *self, HSTRING value, UINT32 *written)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    UINT32 units = 0;
    const wchar_t *raw = value ? WindowsGetStringRawBuffer(value, &units) : NULL;
    HRESULT hr = S_OK;
    if (!written) return E_POINTER;
    *written = 0;
    if (!units || !raw) return S_OK;
    if (writer->unicode == 0) {                                    /* UTF-8 */
        int bytes = WideCharToMultiByte(CP_UTF8, 0, raw, (int)units, NULL, 0, NULL, NULL);
        if (bytes < 0) bytes = 0;
        if (bytes) {
            hr = x104_platform_writer_reserve(writer, (UINT32)bytes);
            if (SUCCEEDED(hr)) {
                WideCharToMultiByte(CP_UTF8, 0, raw, (int)units, (char *)writer->data + writer->size, bytes, NULL, NULL);
                writer->size += (UINT32)bytes;
            }
        }
        *written = (UINT32)(bytes > 0 ? bytes : 0);
    } else {                                                       /* UTF-16LE / UTF-16BE */
        UINT32 index;
        UINT32 bytes = units * 2u;
        hr = x104_platform_writer_reserve(writer, bytes);
        if (SUCCEEDED(hr)) {
            for (index = 0; index < units; ++index) {
                BYTE low = (BYTE)(raw[index] & 0xFF), high = (BYTE)((raw[index] >> 8) & 0xFF);
                writer->data[writer->size++] = writer->unicode == 1 ? low : high;
                writer->data[writer->size++] = writer->unicode == 1 ? high : low;
            }
        }
        *written = bytes;
    }
    X104_LOG_LIMITED(&writer->calls, 8, "X104 platform: DataWriter.WriteString(%lu code units, %s) -> %lu bytes",
                     (unsigned long)units, writer->unicode ? "UTF-16" : "UTF-8", (unsigned long)*written);
    if (FAILED(hr))
        x104_logf("X104 platform: ERROR DataWriter.WriteString -> 0x%08lx %s (unstored %lu bytes, capacity %lu)",
                  (unsigned long)hr, x104_hr_name(hr), (unsigned long)writer->size, (unsigned long)writer->capacity);
    return hr;
}

static HRESULT X104_API x104_platform_writer_measure_string(void *self, HSTRING value, UINT32 *bytes_out)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    UINT32 units = 0;
    const wchar_t *raw = value ? WindowsGetStringRawBuffer(value, &units) : NULL;
    if (!bytes_out) return E_POINTER;
    if (!units || !raw) { *bytes_out = 0; return S_OK; }
    if (writer->unicode == 0) {
        int bytes = WideCharToMultiByte(CP_UTF8, 0, raw, (int)units, NULL, 0, NULL, NULL);
        *bytes_out = (UINT32)(bytes > 0 ? bytes : 0);
    } else {
        *bytes_out = units * 2u;
    }
    return S_OK;
}

/* StoreAsync: flush the accumulated bytes into the host stream in one write */
static HRESULT X104_API x104_platform_writer_store_async(void *self, void **operation)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    HRESULT hr;
    UINT32 stored = writer->size;
    void *result;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!writer->target) {
        X104_LOG_LIMITED(&writer->calls, 4, "X104 platform: DataWriter.StoreAsync -> E_NOTIMPL (the stream behind the writer is not a host stream)");
        return E_NOTIMPL;
    }
    hr = x104_platform_stream_write_bytes(writer->target, writer->data, stored);
    if (FAILED(hr)) return hr;
    writer->size = 0;
    result = x104_platform_op32_new(stored);
    if (!result) return E_OUTOFMEMORY;
    *operation = result;
    x104_logf("X104 platform: DataWriter.StoreAsync -> completed operation %p: %lu bytes written into the save (%lu bytes in the stream)",
              *operation, (unsigned long)stored, (unsigned long)writer->target->size);
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_flush_async(void *self, void **operation)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    void *result;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (writer->target) x104_platform_stream_save_file(writer->target);
    result = x104_platform_opbool_new(1);
    if (!result) return E_OUTOFMEMORY;
    *operation = result;
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_detach_buffer(void *self, void **value)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    X104PlatformBuffer *buffer;
    if (!value) return E_POINTER;
    *value = NULL;
    buffer = (X104PlatformBuffer *)x104_object_new(&x104_platform_buffer_class, sizeof(*buffer));
    if (!buffer) return E_OUTOFMEMORY;
    buffer->data = writer->data;            /* the bytes move to the buffer, the writer starts empty again */
    buffer->size = writer->size;
    buffer->capacity = writer->capacity;
    writer->data = NULL;
    writer->size = 0;
    writer->capacity = 0;
    *value = x104_object_iface0(&buffer->base);
    x104_logf("X104 platform: DataWriter.DetachBuffer -> IBuffer %p (%lu bytes)", *value, (unsigned long)buffer->size);
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_detach_stream(void *self, void **value)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    if (!value) return E_POINTER;
    *value = writer->stream;
    writer->stream = NULL;
    writer->target = NULL;
    return S_OK;
}

static HRESULT X104_API x104_platform_writer_close(void *self)
{
    X104PlatformDataWriter *writer = X104_THIS(X104PlatformDataWriter, self);
    if (writer->stream) {
        x104_raw_release(writer->stream);
        writer->stream = NULL;
    }
    writer->target = NULL;
    return S_OK;
}

/* IDataWriter: IInspectable (0..5) + the 27 members of the interface (winmd order) */
X104_VTBL_N(x104_platform_vt_datawriter, 33) = {
    X104_INSPECTABLE_SLOTS,
    [6]  = (const void *)x104_platform_writer_get_unstored,
    [7]  = (const void *)x104_platform_writer_get_unicode,
    [8]  = (const void *)x104_platform_writer_put_unicode,
    [9]  = (const void *)x104_platform_writer_get_byte_order,
    [10] = (const void *)x104_platform_writer_put_byte_order,
    [11] = (const void *)x104_platform_writer_write_byte,
    [12] = (const void *)x104_platform_writer_write_bytes,
    [13] = (const void *)x104_platform_writer_write_buffer,
    [14] = (const void *)x104_platform_writer_write_buffer_range,
    [15] = (const void *)x104_platform_writer_write_boolean,
    [16] = (const void *)x104_platform_writer_write_guid,
    [17] = (const void *)x104_platform_writer_write_int16,
    [18] = (const void *)x104_platform_writer_write_int32,
    [19] = (const void *)x104_platform_writer_write_int64,
    [20] = (const void *)x104_platform_writer_write_uint16,
    [21] = (const void *)x104_platform_writer_write_uint32,
    [22] = (const void *)x104_platform_writer_write_uint64,
    [23] = (const void *)x104_platform_writer_write_single,
    [24] = (const void *)x104_platform_writer_write_double,
    [25] = (const void *)x104_platform_writer_write_datetime,
    [26] = (const void *)x104_platform_writer_write_timespan,
    [27] = (const void *)x104_platform_writer_write_string,
    [28] = (const void *)x104_platform_writer_measure_string,
    [29] = (const void *)x104_platform_writer_store_async,
    [30] = (const void *)x104_platform_writer_flush_async,
    [31] = (const void *)x104_platform_writer_detach_buffer,
    [32] = (const void *)x104_platform_writer_detach_stream
};
X104_VTBL_N(x104_platform_vt_datawriter_closable, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_writer_close
};
static const X104IfaceDesc x104_platform_datawriter_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_idatawriter, "IDataWriter", x104_platform_vt_datawriter),
    X104_IFACE_IID(&x104_platform_iid_iclosable, "IClosable", x104_platform_vt_datawriter_closable)
};
static HRESULT x104_platform_datawriter_activate(void **instance);   /* the class it creates is defined right below */

X104_CLASS_DEF_NEW(x104_platform_datawriter_class, "DataWriter", L"Windows.Storage.Streams.DataWriter", x104_platform_datawriter_ifaces,
                   X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL, x104_platform_datawriter_activate);

static X104PlatformDataWriter *x104_platform_writer_new(void)
{
    X104PlatformDataWriter *writer = (X104PlatformDataWriter *)x104_object_new(&x104_platform_datawriter_class, sizeof(*writer));
    if (!writer) return NULL;
    writer->unicode = 0;             /* Utf8 */
    writer->byte_order = 0;          /* LittleEndian */
    return writer;
}

/* `ref new DataWriter()`: the 1.27 save code builds the bytes here, DetachBuffer()s them and hands the buffer to
 * IOutputStream.WriteAsync, so the writer is created without a stream */
static HRESULT x104_platform_datawriter_activate(void **instance)
{
    X104PlatformDataWriter *writer;
    if (!instance) return E_POINTER;
    *instance = NULL;
    writer = x104_platform_writer_new();
    if (!writer) return E_OUTOFMEMORY;
    *instance = x104_object_iface0(&writer->base);
    x104_logf("X104 platform: DataWriter created %p (no stream: DetachBuffer path)", *instance);
    return S_OK;
}

/* IDataWriterFactory::CreateDataWriter(IOutputStream stream) */
static HRESULT X104_API x104_platform_datawriter_create(void *self, void *stream, void **value)
{
    X104PlatformDataWriter *writer;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!stream) return E_POINTER;
    writer = x104_platform_writer_new();
    if (!writer) return E_OUTOFMEMORY;
    writer->stream = stream;
    x104_raw_addref(stream);
    writer->target = x104_platform_output_target(stream);
    *value = x104_object_iface0(&writer->base);
    x104_logf("X104 platform: CreateDataWriter(stream %p) -> %p (target %p)", stream, *value, writer->target);
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_datawriter_factory, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_datawriter_create
};
static const X104IfaceDesc x104_platform_datawriter_factory_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_idatawriter_factory, "IDataWriterFactory", x104_platform_vt_datawriter_factory)
};
X104_CLASS_DEF_NEW(x104_platform_datawriter_factory_class, "DataWriterFactory", L"Windows.Storage.Streams.DataWriter",
                   x104_platform_datawriter_factory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL, x104_platform_datawriter_activate);
X104_SINGLETON(x104_platform_datawriter_statics, x104_platform_datawriter_factory_class, X104Object)

/* ======================================================================================================================
 * 12c. Windows.Storage.Streams.DataReader: HCR reads its save with DataReader::FromBuffer
 *      (CreateDecompressor -> IInputStream.ReadAsync into a Buffer -> DataReader.FromBuffer -> Read*).
 *      The reader is a cursor over a byte array the host copies out of the game's IBuffer; a read past the end answers
 *      E_BOUNDS, which is what the real DataReader raises as OutOfBoundsException.
 * ====================================================================================================================== */
static const GUID x104_platform_iid_idatareader = { 0xe2b50029, 0xb4c1, 0x4314, { 0xa4, 0xb8, 0xfb, 0x81, 0x3a, 0x2f, 0x27, 0x5e } };
static const GUID x104_platform_iid_idatareader_statics = { 0x11fcbfc8, 0xf93a, 0x471b, { 0xb1, 0x21, 0xf3, 0x79, 0xe3, 0x49, 0x31, 0x3c } };
static const GUID x104_platform_iid_idatareader_factory = { 0xd7527847, 0x57da, 0x4e15, { 0x91, 0x4c, 0x06, 0x80, 0x66, 0x99, 0xa0, 0x98 } };

typedef struct X104PlatformDataReader {
    X104Object base;
    BYTE *data;                 /* owned copy of the bytes (FromBuffer) */
    UINT32 size;
    UINT32 offset;
    UINT32 unicode;             /* UnicodeEncoding: 0 = Utf8, 1 = Utf16LE, 2 = Utf16BE */
    UINT32 byte_order;          /* ByteOrder: 0 = LittleEndian, 1 = BigEndian */
    UINT32 options;             /* InputStreamOptions */
    void *stream;               /* IInputStream view when the reader was created from a stream (AddRef'd), else NULL */
    volatile LONG calls;
} X104PlatformDataReader;

static HRESULT x104_platform_reader_take(X104PlatformDataReader *reader, UINT32 count, BYTE **bytes)
{
    if (count > reader->size - reader->offset) {
        x104_logf("X104 platform: DataReader: read of %lu bytes past the end (%lu of %lu consumed) -> E_BOUNDS (the game raises OutOfBoundsException)",
                  (unsigned long)count, (unsigned long)reader->offset, (unsigned long)reader->size);
        return 0x8000000Bu; /* E_BOUNDS */
    }
    *bytes = reader->data ? reader->data + reader->offset : NULL;
    reader->offset += count;
    return S_OK;
}

/* copy `count` bytes out of the reader, honouring put_ByteOrder */
static HRESULT x104_platform_reader_copy(X104PlatformDataReader *reader, UINT32 count, void *value)
{
    BYTE *bytes;
    UINT32 index;
    HRESULT hr = x104_platform_reader_take(reader, count, &bytes);
    if (FAILED(hr)) return hr;
    if (reader->byte_order) {
        for (index = 0; index < count; ++index) ((BYTE *)value)[index] = bytes[count - 1 - index];
    } else if (count && bytes && value) {
        memcpy(value, bytes, count);
    }
    return S_OK;
}

#define X104_PLATFORM_READER_SCALAR(fn_name, ctype)                                                        \
    static HRESULT X104_API x104_platform_reader_##fn_name(void *self, ctype *value)                       \
    {                                                                                                      \
        X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);                          \
        if (!value) return E_POINTER;                                                                      \
        memset(value, 0, sizeof(*value));                                                                  \
        return x104_platform_reader_copy(reader, (UINT32)sizeof(ctype), value);                            \
    }

X104_PLATFORM_READER_SCALAR(read_byte, BYTE)
X104_PLATFORM_READER_SCALAR(read_boolean, BYTE)
X104_PLATFORM_READER_SCALAR(read_int16, SHORT)
X104_PLATFORM_READER_SCALAR(read_int32, LONG)
X104_PLATFORM_READER_SCALAR(read_int64, LONGLONG)
X104_PLATFORM_READER_SCALAR(read_uint16, USHORT)
X104_PLATFORM_READER_SCALAR(read_uint32, UINT32)
X104_PLATFORM_READER_SCALAR(read_uint64, UINT64)
X104_PLATFORM_READER_SCALAR(read_single, float)
X104_PLATFORM_READER_SCALAR(read_double, double)
X104_PLATFORM_READER_SCALAR(read_datetime, LONGLONG)
X104_PLATFORM_READER_SCALAR(read_timespan, LONGLONG)

static HRESULT X104_API x104_platform_reader_get_unconsumed(void *self, UINT32 *value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    if (!value) return E_POINTER;
    *value = reader->size - reader->offset;
    X104_LOG_LIMITED(&reader->calls, 12, "X104 platform: DataReader.UnconsumedBufferLength -> %lu of %lu bytes", (unsigned long)*value,
                     (unsigned long)reader->size);
    return S_OK;
}

static HRESULT X104_API x104_platform_reader_get_unicode(void *self, UINT32 *value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    if (!value) return E_POINTER;
    *value = reader->unicode;
    return S_OK;
}

static HRESULT X104_API x104_platform_reader_put_unicode(void *self, UINT32 value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    reader->unicode = value;
    X104_LOG_LIMITED(&reader->calls, 12, "X104 platform: DataReader.UnicodeEncoding <- %lu", (unsigned long)value);
    return S_OK;
}

static HRESULT X104_API x104_platform_reader_get_byte_order(void *self, UINT32 *value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    if (!value) return E_POINTER;
    *value = reader->byte_order;
    return S_OK;
}

static HRESULT X104_API x104_platform_reader_put_byte_order(void *self, UINT32 value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    reader->byte_order = value;
    X104_LOG_LIMITED(&reader->calls, 12, "X104 platform: DataReader.ByteOrder <- %lu", (unsigned long)value);
    return S_OK;
}

static HRESULT X104_API x104_platform_reader_get_options(void *self, UINT32 *value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    if (!value) return E_POINTER;
    *value = reader->options;
    return S_OK;
}

static HRESULT X104_API x104_platform_reader_put_options(void *self, UINT32 value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    reader->options = value;
    return S_OK;
}

/* void ReadBytes(Byte[] value): WinRT passes a WinRT array as (elements, count) */
static HRESULT X104_API x104_platform_reader_read_bytes(void *self, BYTE *value, UINT32 count)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    HRESULT hr;
    if (!value && count) return E_POINTER;
    hr = x104_platform_reader_copy(reader, count, value);
    X104_LOG_LIMITED(&reader->calls, 12, "X104 platform: DataReader.ReadBytes(%lu) -> 0x%08lx %s", (unsigned long)count, (unsigned long)hr, x104_hr_name(hr));
    return hr;
}

static HRESULT X104_API x104_platform_reader_read_guid(void *self, GUID *value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    BYTE *bytes;
    HRESULT hr;
    if (!value) return E_POINTER;
    memset(value, 0, sizeof(*value));
    hr = x104_platform_reader_take(reader, (UINT32)sizeof(GUID), &bytes);
    if (SUCCEEDED(hr) && bytes) memcpy(value, bytes, sizeof(GUID));
    return hr;
}

/* IBuffer ReadBuffer(UINT32 count): the host has no IBuffer implementation of its own (the real runtime provides Buffer) */
static HRESULT X104_API x104_platform_reader_read_buffer(void *self, UINT32 count, void **value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    (void)reader;
    if (value) *value = NULL;
    X104_LOG_LIMITED(&reader->calls, 6, "X104 platform: DataReader.ReadBuffer(%lu) -> E_NOTIMPL (the host never hands out an IBuffer here)",
                     (unsigned long)count);
    return E_NOTIMPL;
}

/* HSTRING ReadString(UINT32 codeUnitCount) */
static HRESULT X104_API x104_platform_reader_read_string(void *self, UINT32 code_units, HSTRING *value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    BYTE *bytes = NULL;
    HRESULT hr;
    wchar_t *wide;
    UINT32 index;
    if (!value) return E_POINTER;
    *value = NULL;
    if (reader->unicode) {                                   /* UTF-16 */
        hr = x104_platform_reader_take(reader, code_units * 2u, &bytes);
        if (FAILED(hr)) return hr;
        wide = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, ((size_t)code_units + 1) * sizeof(wchar_t));
        if (!wide) return E_OUTOFMEMORY;
        for (index = 0; index < code_units; ++index) {
            UINT32 at = index * 2u;
            wide[index] = (wchar_t)(reader->unicode == 1 ? (bytes[at] | (bytes[at + 1] << 8)) : ((bytes[at] << 8) | bytes[at + 1]));
        }
        wide[code_units] = 0;
        hr = x104_hstring_create(wide, value);
        HeapFree(GetProcessHeap(), 0, wide);
    } else {                                                 /* UTF-8 */
        int wide_length;
        hr = x104_platform_reader_take(reader, code_units, &bytes);
        if (FAILED(hr)) return hr;
        wide_length = MultiByteToWideChar(CP_UTF8, 0, (const char *)bytes, (int)code_units, NULL, 0);
        if (wide_length < 0) wide_length = 0;
        wide = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, ((size_t)wide_length + 1) * sizeof(wchar_t));
        if (!wide) return E_OUTOFMEMORY;
        if (wide_length > 0) MultiByteToWideChar(CP_UTF8, 0, (const char *)bytes, (int)code_units, wide, wide_length);
        wide[wide_length] = 0;
        hr = x104_hstring_create(wide, value);
        HeapFree(GetProcessHeap(), 0, wide);
    }
    X104_LOG_LIMITED(&reader->calls, 12, "X104 platform: DataReader.ReadString(%lu code units, %s) -> 0x%08lx %s", (unsigned long)code_units,
                     reader->unicode ? "UTF-16" : "UTF-8", (unsigned long)hr, x104_hr_name(hr));
    return hr;
}

static HRESULT X104_API x104_platform_reader_load_async(void *self, UINT32 count, void **operation)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    if (operation) *operation = NULL;
    X104_LOG_LIMITED(&reader->calls, 6, "X104 platform: DataReader.LoadAsync(%lu) -> E_NOTIMPL (the game uses FromBuffer, so the reader is already loaded)",
                     (unsigned long)count);
    return E_NOTIMPL;
}

static HRESULT X104_API x104_platform_reader_detach_buffer(void *self, void **value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    if (value) *value = NULL;
    X104_LOG_LIMITED(&reader->calls, 4, "X104 platform: DataReader.DetachBuffer -> E_NOTIMPL");
    return E_NOTIMPL;
}

static HRESULT X104_API x104_platform_reader_detach_stream(void *self, void **value)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    if (!value) return E_POINTER;
    *value = reader->stream;
    reader->stream = NULL;
    return S_OK;
}

static HRESULT X104_API x104_platform_reader_close(void *self)
{
    X104PlatformDataReader *reader = X104_THIS(X104PlatformDataReader, self);
    if (reader->stream) {
        x104_raw_release(reader->stream);
        reader->stream = NULL;
    }
    return S_OK;
}

/* IDataReader: IInspectable (0..5) + the 26 members of the interface (the winmd order is authoritative) */
X104_VTBL_N(x104_platform_vt_datareader, 32) = {
    X104_INSPECTABLE_SLOTS,
    [6]  = (const void *)x104_platform_reader_get_unconsumed,
    [7]  = (const void *)x104_platform_reader_get_unicode,
    [8]  = (const void *)x104_platform_reader_put_unicode,
    [9]  = (const void *)x104_platform_reader_get_byte_order,
    [10] = (const void *)x104_platform_reader_put_byte_order,
    [11] = (const void *)x104_platform_reader_get_options,
    [12] = (const void *)x104_platform_reader_put_options,
    [13] = (const void *)x104_platform_reader_read_byte,
    [14] = (const void *)x104_platform_reader_read_bytes,
    [15] = (const void *)x104_platform_reader_read_buffer,
    [16] = (const void *)x104_platform_reader_read_boolean,
    [17] = (const void *)x104_platform_reader_read_guid,
    [18] = (const void *)x104_platform_reader_read_int16,
    [19] = (const void *)x104_platform_reader_read_int32,
    [20] = (const void *)x104_platform_reader_read_int64,
    [21] = (const void *)x104_platform_reader_read_uint16,
    [22] = (const void *)x104_platform_reader_read_uint32,
    [23] = (const void *)x104_platform_reader_read_uint64,
    [24] = (const void *)x104_platform_reader_read_single,
    [25] = (const void *)x104_platform_reader_read_double,
    [26] = (const void *)x104_platform_reader_read_string,
    [27] = (const void *)x104_platform_reader_read_datetime,
    [28] = (const void *)x104_platform_reader_read_timespan,
    [29] = (const void *)x104_platform_reader_load_async,
    [30] = (const void *)x104_platform_reader_detach_buffer,
    [31] = (const void *)x104_platform_reader_detach_stream
};
static const X104IfaceDesc x104_platform_datareader_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_idatareader, "IDataReader", x104_platform_vt_datareader),
    X104_IFACE_IID(&x104_platform_iid_iclosable, "IClosable", x104_platform_vt_decompressor_closable)
};
X104_CLASS_DEF(x104_platform_datareader_class, "DataReader", L"Windows.Storage.Streams.DataReader", x104_platform_datareader_ifaces,
               X104_CLASS_AGILE, NULL);

static X104PlatformDataReader *x104_platform_reader_new(void)
{
    X104PlatformDataReader *reader = (X104PlatformDataReader *)x104_object_new(&x104_platform_datareader_class, sizeof(*reader));
    if (!reader) return NULL;
    reader->unicode = 0;             /* Utf8 */
    reader->byte_order = 0;          /* LittleEndian */
    return reader;
}

/* IDataReaderStatics::FromBuffer(IBuffer buffer) */
static HRESULT X104_API x104_platform_datareader_from_buffer(void *self, void *buffer, void **value)
{
    X104PlatformDataReader *reader;
    BYTE *bytes = NULL;
    UINT32 length = 0;
    HRESULT hr;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!buffer) return E_POINTER;
    hr = ((HRESULT (X104_API *)(void *, UINT32 *))((void **)*(void ***)buffer)[7])(buffer, &length);
    if (FAILED(hr)) {
        x104_logf("X104 platform: DataReader.FromBuffer: IBuffer.get_Length failed (0x%08lx %s)", (unsigned long)hr, x104_hr_name(hr));
        return hr;
    }
    reader = x104_platform_reader_new();
    if (!reader) return E_OUTOFMEMORY;
    if (length) {
        hr = x104_platform_buffer_bytes(buffer, &bytes);
        if (FAILED(hr)) {
            x104_logf("X104 platform: DataReader.FromBuffer: the game's IBuffer does not expose IBufferByteAccess (0x%08lx %s)", (unsigned long)hr,
                      x104_hr_name(hr));
            return hr;
        }
        reader->data = (BYTE *)HeapAlloc(GetProcessHeap(), 0, length);
        if (!reader->data) return E_OUTOFMEMORY;
        memcpy(reader->data, bytes, length);
        reader->size = length;
    }
    *value = x104_object_iface0(&reader->base);
    x104_logf("X104 platform: DataReader.FromBuffer(%lu bytes) -> reader %p", (unsigned long)length, *value);
    return S_OK;
}

/* IDataReaderFactory::CreateDataReader(IInputStream stream) */
static HRESULT X104_API x104_platform_datareader_create(void *self, void *stream, void **value)
{
    X104PlatformDataReader *reader;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    if (!stream) return E_POINTER;
    reader = x104_platform_reader_new();
    if (!reader) return E_OUTOFMEMORY;
    reader->stream = stream;
    x104_raw_addref(stream);
    *value = x104_object_iface0(&reader->base);
    x104_logf("X104 platform: CreateDataReader(stream %p) -> reader %p (LoadAsync is not implemented, only FromBuffer is)", stream, *value);
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_datareader_statics, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_datareader_from_buffer
};
X104_VTBL_N(x104_platform_vt_datareader_factory, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_datareader_create
};
static const X104IfaceDesc x104_platform_datareader_statics_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_idatareader_statics, "IDataReaderStatics", x104_platform_vt_datareader_statics),
    X104_IFACE_IID(&x104_platform_iid_idatareader_factory, "IDataReaderFactory", x104_platform_vt_datareader_factory)
};
X104_CLASS_DEF(x104_platform_datareader_statics_class, "DataReaderStatics", L"Windows.Storage.Streams.DataReader",
               x104_platform_datareader_statics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_datareader_statics, x104_platform_datareader_statics_class, X104Object)

/* ======================================================================================================================
 * 12e. Windows.ApplicationModel.Background: the tile-update maintenance task HCR registers after a race, and again
 *      whenever the language changes.  Wine's windows.applicationmodel has no Background namespace at all, so the
 *      registration dies with REGDB_E_CLASSNOTREG and the game turns that into Platform::ClassNotRegisteredException
 *      raised from its UI queue - the game loop never returns, which is exactly the "TOUCH TO CONTINUE" freeze and the
 *      "SELECT LANGUAGE" freeze.  A desktop process has no background-task broker either, so the three classes are
 *      fakes: the registration is accepted and reported back as a task object, but nothing is ever scheduled and the
 *      live tile simply does not update in the background (the same as the Windows build running unpackaged).
 * ====================================================================================================================== */
static const GUID x104_platform_iid_imaintenance_trigger_factory = { 0x4b3ddb2e, 0x97dd, 0x4629, { 0x88, 0xb0, 0xb0, 0x6c, 0xf9, 0x48, 0x2a, 0xe5 } };
static const GUID x104_platform_iid_imaintenance_trigger = { 0x68184c83, 0xfc22, 0x4ce5, { 0x84, 0x1a, 0x72, 0x39, 0xa9, 0x81, 0x00, 0x47 } };
static const GUID x104_platform_iid_ibackground_trigger = { 0x84b3a058, 0x6027, 0x4b87, { 0x97, 0x90, 0xbd, 0xf3, 0xf7, 0x57, 0xdb, 0xd7 } };
static const GUID x104_platform_iid_ibackground_task_builder = { 0x0351550e, 0x3e64, 0x4572, { 0xa9, 0x3a, 0x84, 0x07, 0x5a, 0x37, 0xc9, 0x17 } };
static const GUID x104_platform_iid_ibackground_task_builder2 = { 0x6ae7cfb1, 0x104f, 0x406d, { 0x8d, 0xb6, 0x84, 0x4a, 0x57, 0x0f, 0x42, 0xbb } };
static const GUID x104_platform_iid_ibackground_task_builder3 = { 0x28c74f4a, 0x8ba9, 0x4c09, { 0xa2, 0x4f, 0x19, 0x68, 0x3e, 0x2c, 0x92, 0x4c } };
static const GUID x104_platform_iid_ibackground_task_registration = { 0x10654cc2, 0xa26e, 0x43bf, { 0x8c, 0x12, 0x1f, 0xb4, 0x0d, 0xbf, 0xbf, 0xa0 } };
static const GUID x104_platform_iid_ibackground_task_registration2 = { 0x6138c703, 0xbb86, 0x4112, { 0xaf, 0xc3, 0x7f, 0x93, 0x9b, 0x16, 0x6e, 0x3b } };
static const GUID x104_platform_iid_ibackground_task_registration_statics = { 0x4c542f69, 0xb000, 0x42ba, { 0xa0, 0x93, 0x6a, 0x56, 0x3c, 0x65, 0xe3, 0xf8 } };
/* the parameterised IIDs of IMapView<GUID, IBackgroundTaskRegistration> / IIterable<...> / IIterator<...> are hash-derived and not in the
 * facts; these private ones are only used for the log ("the game QI'd <this>") - the map is always handed out directly */
static const GUID x104_platform_iid_task_map_view = { 0x5337e260, 0x1da2, 0x42e1, { 0x88, 0x34, 0x03, 0x79, 0xd2, 0x8d, 0x74, 0x2f } };
static const GUID x104_platform_iid_task_map_iterable = { 0x5337e261, 0x1da2, 0x42e1, { 0x88, 0x34, 0x03, 0x79, 0xd2, 0x8d, 0x74, 0x2f } };
static const GUID x104_platform_iid_task_map_iterator = { 0x5337e262, 0x1da2, 0x42e1, { 0x88, 0x34, 0x03, 0x79, 0xd2, 0x8d, 0x74, 0x2f } };

/* HSTRING copy: WindowsCreateString always makes a string the callee owns, WindowsGetStringRawBuffer never AddRefs */
static HSTRING x104_platform_hstring_copy(HSTRING value)
{
    UINT32 length = 0;
    const wchar_t *raw = value ? WindowsGetStringRawBuffer(value, &length) : NULL;
    HSTRING copy = NULL;
    if (FAILED(WindowsCreateString(raw, raw ? length : 0, &copy))) return NULL;
    return copy;
}

/* ---- MaintenanceTrigger: `ref new MaintenanceTrigger(15, false)` ---- */
typedef struct X104PlatformTrigger {
    X104Object base;
    UINT32 freshness_time;      /* minutes between two runs */
    BYTE one_shot;
} X104PlatformTrigger;

static HRESULT X104_API x104_platform_trigger_get_freshness(void *self, UINT32 *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(X104PlatformTrigger, self)->freshness_time;
    return S_OK;
}

static HRESULT X104_API x104_platform_trigger_get_one_shot(void *self, BYTE *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(X104PlatformTrigger, self)->one_shot;
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_maintenance_trigger, 8) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_trigger_get_freshness,
    [7] = (const void *)x104_platform_trigger_get_one_shot
};
/* IBackgroundTrigger is empty: the type only exists so SetTrigger can take a trigger of any kind */
X104_VTBL_N(x104_platform_vt_background_trigger, 6) = {
    X104_INSPECTABLE_SLOTS
};
static const X104IfaceDesc x104_platform_trigger_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_imaintenance_trigger, "IMaintenanceTrigger", x104_platform_vt_maintenance_trigger),
    X104_IFACE_IID(&x104_platform_iid_ibackground_trigger, "IBackgroundTrigger", x104_platform_vt_background_trigger)
};
X104_CLASS_DEF(x104_platform_trigger_class, "MaintenanceTrigger", L"Windows.ApplicationModel.Background.MaintenanceTrigger",
               x104_platform_trigger_ifaces, X104_CLASS_AGILE, NULL);

static HRESULT X104_API x104_platform_maintenance_trigger_create(void *self, UINT32 freshness_time, BYTE one_shot, void **value)
{
    X104PlatformTrigger *trigger;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    trigger = (X104PlatformTrigger *)x104_object_new(&x104_platform_trigger_class, sizeof(*trigger));
    if (!trigger) return E_OUTOFMEMORY;
    trigger->freshness_time = freshness_time;
    trigger->one_shot = one_shot;
    *value = x104_object_iface0(&trigger->base);
    x104_logf("X104 platform: MaintenanceTrigger.Create(%lu min, %s) -> %p (accepted, never scheduled on a desktop host)",
              (unsigned long)freshness_time, one_shot ? "one-shot" : "repeating", *value);
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_maintenance_trigger_factory, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_maintenance_trigger_create
};
static const X104IfaceDesc x104_platform_maintenance_trigger_factory_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_imaintenance_trigger_factory, "IMaintenanceTriggerFactory", x104_platform_vt_maintenance_trigger_factory)
};
X104_CLASS_DEF(x104_platform_maintenance_trigger_factory_class, "MaintenanceTriggerFactory", L"Windows.ApplicationModel.Background.MaintenanceTrigger",
               x104_platform_maintenance_trigger_factory_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_maintenance_trigger_statics, x104_platform_maintenance_trigger_factory_class, X104Object)

/* ---- BackgroundTaskRegistration: the object Register() hands back ---- */
typedef struct X104PlatformTaskRegistration {
    X104Object base;
    GUID task_id;
    HSTRING name;               /* owned */
    void *trigger;              /* IBackgroundTrigger view (AddRef'd) or NULL */
} X104PlatformTaskRegistration;

static void x104_platform_task_id_new(GUID *id)
{
    static volatile LONG counter;
    LONG number = InterlockedIncrement(&counter);
    id->Data1 = 0x48435254u + (DWORD)number;     /* "HCRT" + n: only has to be unique inside this process */
    id->Data2 = 0x0001;
    id->Data3 = 0x4000;
    id->Data4[0] = 0x8a; id->Data4[1] = 0x2f; id->Data4[2] = 0x00; id->Data4[3] = 0x11;
    id->Data4[4] = 0x22; id->Data4[5] = 0x33; id->Data4[6] = 0x44; id->Data4[7] = 0x55;
}

static HRESULT X104_API x104_platform_registration_get_task_id(void *self, GUID *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(X104PlatformTaskRegistration, self)->task_id;
    return S_OK;
}

static HRESULT X104_API x104_platform_registration_get_name(void *self, HSTRING *value)
{
    X104PlatformTaskRegistration *registration = X104_THIS(X104PlatformTaskRegistration, self);
    if (!value) return E_POINTER;
    *value = x104_platform_hstring_copy(registration->name);
    return S_OK;
}

/* add_/remove_Progress and add_/remove_Completed: accepted and forgotten (nothing runs, so nothing ever reports) */
static HRESULT X104_API x104_platform_registration_add_handler(void *self, void *handler, LONGLONG *token)
{
    static volatile LONG logged;
    (void)self;
    if (!token) return E_POINTER;
    (void)handler;
    *token = 1;
    X104_LOG_LIMITED(&logged, 4, "X104 platform: BackgroundTaskRegistration add_Progress/add_Completed accepted (nothing runs)");
    return S_OK;
}

static HRESULT X104_API x104_platform_registration_remove_handler(void *self, LONGLONG token)
{
    (void)self;
    (void)token;
    return S_OK;
}

static HRESULT X104_API x104_platform_registration_unregister(void *self, BYTE *value)
{
    static volatile LONG logged;
    (void)self;
    if (value) *value = 1;
    X104_LOG_LIMITED(&logged, 8, "X104 platform: BackgroundTaskRegistration.Unregister -> TRUE");
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_registration, 13) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_registration_get_task_id,
    [7] = (const void *)x104_platform_registration_get_name,
    [8] = (const void *)x104_platform_registration_add_handler,
    [9] = (const void *)x104_platform_registration_remove_handler,
    [10] = (const void *)x104_platform_registration_add_handler,
    [11] = (const void *)x104_platform_registration_remove_handler,
    [12] = (const void *)x104_platform_registration_unregister
};

static HRESULT X104_API x104_platform_registration_get_trigger(void *self, void **value)
{
    X104PlatformTaskRegistration *registration = X104_THIS(X104PlatformTaskRegistration, self);
    if (!value) return E_POINTER;
    *value = registration->trigger;
    if (*value) x104_raw_addref(*value);
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_registration2, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_registration_get_trigger
};
static const X104IfaceDesc x104_platform_registration_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_ibackground_task_registration, "IBackgroundTaskRegistration", x104_platform_vt_registration),
    X104_IFACE_IID(&x104_platform_iid_ibackground_task_registration2, "IBackgroundTaskRegistration2", x104_platform_vt_registration2)
};
X104_CLASS_DEF(x104_platform_registration_class, "BackgroundTaskRegistration", L"Windows.ApplicationModel.Background.BackgroundTaskRegistration",
               x104_platform_registration_ifaces, X104_CLASS_AGILE, NULL);

/* ---- the empty AllTasks map: IMapView<GUID, IBackgroundTaskRegistration> + IIterable + IIterator ----
 * The game walks the existing registrations before adding its own.  There are none on a desktop host, so the map is empty:
 * Lookup/HasKey answer E_BOUNDS/FALSE, First returns an iterator whose HasCurrent is FALSE.  The parameterised IIDs are not in
 * the facts, so the map answers ANY QI with its IIterable view (the only other view anyone could want) and logs the IID. */
typedef struct X104PlatformTaskMap {
    X104Object base;
} X104PlatformTaskMap;

typedef struct X104PlatformTaskMapIterator {
    X104Object base;
} X104PlatformTaskMapIterator;

static HRESULT X104_API x104_platform_task_map_lookup(void *self, const GUID *key, void **value)
{
    (void)self;
    (void)key;
    if (value) *value = NULL;
    return E_BOUNDS;
}

static HRESULT X104_API x104_platform_task_map_get_size(void *self, UINT32 *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_task_map_has_key(void *self, const GUID *key, BYTE *value)
{
    (void)self;
    (void)key;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_task_map_split(void *self, void **first, void **second)
{
    if (!first || !second) return E_POINTER;
    x104_raw_addref(self);
    *first = self;
    x104_raw_addref(self);
    *second = self;
    return S_OK;
}

static HRESULT X104_API x104_platform_task_iterator_get_current(void *self, void **value)
{
    (void)self;
    if (value) *value = NULL;
    return E_BOUNDS;
}

static HRESULT X104_API x104_platform_task_iterator_get_has_current(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_task_iterator_move_next(void *self, BYTE *value)
{
    (void)self;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_task_iterator_get_many(void *self, UINT32 capacity, void **items, UINT32 *actual)
{
    (void)self;
    (void)items;
    if (!actual) return E_POINTER;
    (void)capacity;
    *actual = 0;
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_task_iterator, 10) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_task_iterator_get_current,
    [7] = (const void *)x104_platform_task_iterator_get_has_current,
    [8] = (const void *)x104_platform_task_iterator_move_next,
    [9] = (const void *)x104_platform_task_iterator_get_many
};
static const X104IfaceDesc x104_platform_task_iterator_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_task_map_iterator, "IIterator<KVP>", x104_platform_vt_task_iterator)
};
X104_CLASS_DEF(x104_platform_task_iterator_class, "BackgroundTaskMapIterator", L"Windows.Foundation.Collections.IIterator`2",
               x104_platform_task_iterator_ifaces, X104_CLASS_AGILE, NULL);

static HRESULT X104_API x104_platform_task_map_first(void *self, void **value)
{
    X104PlatformTaskMapIterator *iterator;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    iterator = (X104PlatformTaskMapIterator *)x104_object_new(&x104_platform_task_iterator_class, sizeof(*iterator));
    if (!iterator) return E_OUTOFMEMORY;
    *value = x104_object_iface0(&iterator->base);
    return S_OK;
}

static X104View *x104_platform_task_map_qi(struct X104Object *object, const GUID *iid)
{
    static volatile LONG logged;
    char text[96];
    x104_describe_iid(text, iid);
    X104_LOG_LIMITED(&logged, 8, "X104 platform: AllTasks map QI %s -> IIterable view (parameterised IID, not looked up)", text);
    return &object->views[1];                 /* views[0] = IMapView, views[1] = IIterable */
}

X104_VTBL_N(x104_platform_vt_task_map_view, 10) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_task_map_lookup,
    [7] = (const void *)x104_platform_task_map_get_size,
    [8] = (const void *)x104_platform_task_map_has_key,
    [9] = (const void *)x104_platform_task_map_split
};
X104_VTBL_N(x104_platform_vt_task_map_iterable, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_task_map_first
};
static const X104IfaceDesc x104_platform_task_map_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_task_map_view, "IMapView<GUID,IBackgroundTaskRegistration>", x104_platform_vt_task_map_view),
    X104_IFACE_IID(&x104_platform_iid_task_map_iterable, "IIterable<KVP>", x104_platform_vt_task_map_iterable)
};
X104_CLASS_DEF(x104_platform_task_map_class, "BackgroundTaskMap", L"Windows.Foundation.Collections.IMapView`2",
               x104_platform_task_map_ifaces, X104_CLASS_AGILE, x104_platform_task_map_qi);

static HRESULT X104_API x104_platform_registration_statics_get_all_tasks(void *self, void **value)
{
    static volatile LONG logged;
    X104PlatformTaskMap *map;
    (void)self;
    if (!value) return E_POINTER;
    *value = NULL;
    map = (X104PlatformTaskMap *)x104_object_new(&x104_platform_task_map_class, sizeof(*map));
    if (!map) return E_OUTOFMEMORY;
    *value = x104_object_iface0(&map->base);
    X104_LOG_LIMITED(&logged, 8, "X104 platform: BackgroundTaskRegistration.AllTasks -> empty map %p (no task was ever registered by another process)", *value);
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_registration_statics, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_registration_statics_get_all_tasks
};
static const X104IfaceDesc x104_platform_registration_statics_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_ibackground_task_registration_statics, "IBackgroundTaskRegistrationStatics", x104_platform_vt_registration_statics)
};
X104_CLASS_DEF(x104_platform_registration_statics_class, "BackgroundTaskRegistrationStatics", L"Windows.ApplicationModel.Background.BackgroundTaskRegistration",
               x104_platform_registration_statics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_registration_statics, x104_platform_registration_statics_class, X104Object)

/* ---- BackgroundTaskBuilder: `ref new BackgroundTaskBuilder()` (default constructor -> ActivateInstance) ---- */
typedef struct X104PlatformTaskBuilder {
    X104Object base;
    HSTRING entry_point;        /* owned */
    HSTRING name;               /* owned */
    void *trigger;              /* IBackgroundTrigger view (AddRef'd) or NULL */
    BYTE cancel_on_condition_loss;
    BYTE network_requested;
} X104PlatformTaskBuilder;

static HRESULT X104_API x104_platform_builder_put_entry_point(void *self, HSTRING value)
{
    X104PlatformTaskBuilder *builder = X104_THIS(X104PlatformTaskBuilder, self);
    char shown[128];
    if (builder->entry_point) WindowsDeleteString(builder->entry_point);
    builder->entry_point = x104_platform_hstring_copy(value);
    x104_hstring_ascii(builder->entry_point, shown, sizeof(shown));
    x104_logf("X104 platform: BackgroundTaskBuilder.TaskEntryPoint = %s", shown);
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_get_entry_point(void *self, HSTRING *value)
{
    X104PlatformTaskBuilder *builder = X104_THIS(X104PlatformTaskBuilder, self);
    if (!value) return E_POINTER;
    *value = x104_platform_hstring_copy(builder->entry_point);
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_set_trigger(void *self, void *trigger)
{
    X104PlatformTaskBuilder *builder = X104_THIS(X104PlatformTaskBuilder, self);
    if (builder->trigger) x104_raw_release(builder->trigger);
    builder->trigger = trigger;
    if (trigger) x104_raw_addref(trigger);
    x104_logf("X104 platform: BackgroundTaskBuilder.SetTrigger(%p) accepted", trigger);
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_add_condition(void *self, void *condition)
{
    static volatile LONG logged;
    (void)self;
    (void)condition;
    X104_LOG_LIMITED(&logged, 4, "X104 platform: BackgroundTaskBuilder.AddCondition accepted (no conditions are evaluated)");
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_put_name(void *self, HSTRING value)
{
    X104PlatformTaskBuilder *builder = X104_THIS(X104PlatformTaskBuilder, self);
    char shown[128];
    if (builder->name) WindowsDeleteString(builder->name);
    builder->name = x104_platform_hstring_copy(value);
    x104_hstring_ascii(builder->name, shown, sizeof(shown));
    x104_logf("X104 platform: BackgroundTaskBuilder.Name = %s", shown);
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_get_name(void *self, HSTRING *value)
{
    X104PlatformTaskBuilder *builder = X104_THIS(X104PlatformTaskBuilder, self);
    if (!value) return E_POINTER;
    *value = x104_platform_hstring_copy(builder->name);
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_register(void *self, void **value)
{
    X104PlatformTaskBuilder *builder = X104_THIS(X104PlatformTaskBuilder, self);
    X104PlatformTaskRegistration *registration;
    char shown[128];
    if (!value) return E_POINTER;
    *value = NULL;
    registration = (X104PlatformTaskRegistration *)x104_object_new(&x104_platform_registration_class, sizeof(*registration));
    if (!registration) return E_OUTOFMEMORY;
    registration->name = x104_platform_hstring_copy(builder->name);
    registration->trigger = builder->trigger;
    if (registration->trigger) x104_raw_addref(registration->trigger);
    x104_platform_task_id_new(&registration->task_id);
    *value = x104_object_iface0(&registration->base);
    x104_hstring_ascii(builder->name, shown, sizeof(shown));
    x104_logf("X104 platform: BackgroundTaskBuilder.Register() -> task \"%s\" %p (registration accepted; nothing will run)", shown, *value);
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_put_cancel_on_loss(void *self, BYTE value)
{
    X104_THIS(X104PlatformTaskBuilder, self)->cancel_on_condition_loss = value ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_get_cancel_on_loss(void *self, BYTE *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(X104PlatformTaskBuilder, self)->cancel_on_condition_loss;
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_put_network(void *self, BYTE value)
{
    X104_THIS(X104PlatformTaskBuilder, self)->network_requested = value ? 1 : 0;
    return S_OK;
}

static HRESULT X104_API x104_platform_builder_get_network(void *self, BYTE *value)
{
    if (!value) return E_POINTER;
    *value = X104_THIS(X104PlatformTaskBuilder, self)->network_requested;
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_task_builder, 13) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_builder_put_entry_point,
    [7] = (const void *)x104_platform_builder_get_entry_point,
    [8] = (const void *)x104_platform_builder_set_trigger,
    [9] = (const void *)x104_platform_builder_add_condition,
    [10] = (const void *)x104_platform_builder_put_name,
    [11] = (const void *)x104_platform_builder_get_name,
    [12] = (const void *)x104_platform_builder_register
};
X104_VTBL_N(x104_platform_vt_task_builder2, 8) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_builder_put_cancel_on_loss,
    [7] = (const void *)x104_platform_builder_get_cancel_on_loss
};
X104_VTBL_N(x104_platform_vt_task_builder3, 8) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_builder_put_network,
    [7] = (const void *)x104_platform_builder_get_network
};
static const X104IfaceDesc x104_platform_task_builder_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_ibackground_task_builder, "IBackgroundTaskBuilder", x104_platform_vt_task_builder),
    X104_IFACE_IID(&x104_platform_iid_ibackground_task_builder2, "IBackgroundTaskBuilder2", x104_platform_vt_task_builder2),
    X104_IFACE_IID(&x104_platform_iid_ibackground_task_builder3, "IBackgroundTaskBuilder3", x104_platform_vt_task_builder3)
};

static HRESULT x104_platform_task_builder_activate(void **instance);   /* the class it creates is defined right below */

X104_CLASS_DEF_NEW(x104_platform_task_builder_class, "BackgroundTaskBuilder", L"Windows.ApplicationModel.Background.BackgroundTaskBuilder",
                   x104_platform_task_builder_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL, x104_platform_task_builder_activate);
X104_SINGLETON(x104_platform_task_builder_statics, x104_platform_task_builder_class, X104Object)

/* ---- HillClimbRacing.BackgroundTasks.UpdateTileTask: the game's own tile updater ----
 * The entry point lives in a .NET component (CLRHost.dll in the manifest), and the game calls its static Update() right after
 * registering the maintenance task to refresh the live tile immediately.  There is no CLR host and no live tile on this build,
 * so the statics interface IUpdateTileTaskStatic accepts the call and does nothing. */
static const GUID x104_platform_iid_iupdatetiletask_static = { 0x5e0eab60, 0x445e, 0x5a45, { 0x70, 0xe3, 0x65, 0x6c, 0x5e, 0x30, 0xdf, 0x4c } };

static HRESULT X104_API x104_platform_tile_task_update(void *self)
{
    static volatile LONG logged;
    (void)self;
    X104_LOG_LIMITED(&logged, 8, "X104 platform: UpdateTileTask.Update() -> S_OK (the tile is not updated on a desktop host)");
    return S_OK;
}

X104_VTBL_N(x104_platform_vt_tile_task_statics, 7) = {
    X104_INSPECTABLE_SLOTS,
    [6] = (const void *)x104_platform_tile_task_update
};
static const X104IfaceDesc x104_platform_tile_task_statics_ifaces[] = {
    X104_IFACE_IID(&x104_platform_iid_iupdatetiletask_static, "IUpdateTileTaskStatic", x104_platform_vt_tile_task_statics)
};
X104_CLASS_DEF(x104_platform_tile_task_statics_class, "UpdateTileTask", L"HillClimbRacing.BackgroundTasks.UpdateTileTask",
               x104_platform_tile_task_statics_ifaces, X104_CLASS_AGILE | X104_CLASS_FACTORY, NULL);
X104_SINGLETON(x104_platform_tile_task_statics, x104_platform_tile_task_statics_class, X104Object)

static HRESULT x104_platform_task_builder_activate(void **instance)
{
    X104PlatformTaskBuilder *builder;
    if (!instance) return E_POINTER;
    *instance = NULL;
    builder = (X104PlatformTaskBuilder *)x104_object_new(&x104_platform_task_builder_class, sizeof(*builder));
    if (!builder) return E_OUTOFMEMORY;
    *instance = x104_object_iface0(&builder->base);
    x104_logf("X104 platform: BackgroundTaskBuilder created %p", *instance);
    return S_OK;
}

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
    { L"Windows.Foundation.Diagnostics.AsyncCausalityTracer",    x104_platform_tracer_statics },
    { L"Windows.Storage.Compression.Decompressor",               x104_platform_decompressor_statics },
    { L"Windows.Storage.Compression.Compressor",                 x104_platform_compressor_statics },
    { L"Windows.Storage.Streams.DataReader",                     x104_platform_datareader_statics },
    { L"Windows.Storage.Streams.DataWriter",                     x104_platform_datawriter_statics },
    { L"Windows.ApplicationModel.Background.MaintenanceTrigger",       x104_platform_maintenance_trigger_statics },
    { L"Windows.ApplicationModel.Background.BackgroundTaskBuilder",    x104_platform_task_builder_statics },
    { L"Windows.ApplicationModel.Background.BackgroundTaskRegistration", x104_platform_registration_statics },
    { L"HillClimbRacing.BackgroundTasks.UpdateTileTask",               x104_platform_tile_task_statics }
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
