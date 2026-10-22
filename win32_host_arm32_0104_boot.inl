/* win32_host_arm32_0104_boot.inl - bootstrap of the Minecraft PE 0.10.4.40 (WP8.1 XAML) ARM32 host.   OWNER: designer.
 *
 * Entry: the patched exe's CRT calls `bl 0x20A364` (instead of `bl 0x203EFC` at rva 0x200A72), a PIC trampoline that tail-jumps to
 * win32craft.dll!Win32Bootstrap with r0..r3 = the WinMain arguments (unused).  Win32Bootstrap (win32_host.c, ARM branch) has already
 * made the exe directory the working directory, opened the log, started the watchdog and installed the crash / abort traps; when
 * x104_image_matches(image) is TRUE it then calls x104_bootstrap(image) and returns its result to the CRT.
 *
 * x104_bootstrap order (HOST_DESIGN.md 7; every step is logged and every failure ends with a message box + a distinct exit code):
 *   identify -> options -> HSTRING/RoInitialize -> platform_install -> IAT hooks (activation router + diagnostics) -> VC120 diagnostics (module list, log-only
 *   RoGetActivationFactory forwarders) -> panel_install -> async/input/app install -> window (HIDDEN, fitted to the work area) -> async/input/panel/app init
 *   -> f_units_ready (window messages are routed to the units from here) -> optional --selftest-fakes -> call the game's WinMain wrapper (rva 0x203EFC) with the
 *   original-style arguments on THIS thread, which becomes the UI thread (the fake Application::Start runs x104_ui_run_loop inside it) -> shutdown -> exit guard
 *   -> return the game's result.
 *
 * Return codes: 0 or the game's result = normal end.  30 not a 0.10.4 image  31 platform_install  32 IAT hooks  33 panel_install
 *   34 async/input/app install  35 RegisterClassExW  36 CreateWindowExW  37 unit init  38 WinMain wrapper bytes differ
 *   39 the game raised an unhandled exception inside WinMain  40 --selftest-fakes found failures. */

#define X104_BOOT_BAD_IMAGE        30
#define X104_BOOT_PLATFORM_FAILED  31
#define X104_BOOT_IAT_FAILED       32
#define X104_BOOT_PANEL_FAILED     33
#define X104_BOOT_INSTALL_FAILED   34
#define X104_BOOT_CLASS_FAILED     35
#define X104_BOOT_WINDOW_FAILED    36
#define X104_BOOT_INIT_FAILED      37
#define X104_BOOT_WRAPPER_BYTES    38
#define X104_BOOT_GAME_EXCEPTION   39
#define X104_BOOT_SELFTEST_FAILED  40

#define X104_IMAGE_SIZE_PATCHED    0x00E6F000u    /* facts pe_patch_plan.host_identification */
#define X104_IMAGE_SIZE_STOCK      0x00E6E000u
/* X104_TIMER_PUMP, X104_WM_QUIT, X104_WM_WAKE live in the CORE (the async unit needs them and this file is included last) */

/* ---- image identification ---- */
static const BYTE x104_callsite_patched_bytes[4] = { 0x09, 0xf0, 0x77, 0xfc };     /* bl 0x20A364 (pe_patch_plan.patch_callsite) */

/* quiet check (no log): PE32 ARMNT with SizeOfImage == the patched 0.10.4 constant.  Used by Win32Bootstrap before anything is logged. */
static BOOL x104_image_matches(BYTE *image)
{
    DWORD pe_offset;
    const BYTE *pe;
    if (!image || IsBadReadPtr(image, 0x40) || image[0] != 'M' || image[1] != 'Z') return FALSE;
    pe_offset = *(const DWORD *)(image + 0x3c);
    if (pe_offset < 0x40 || pe_offset > 0x1000 || IsBadReadPtr(image + pe_offset, 0x100) || *(const DWORD *)(image + pe_offset) != 0x00004550u) return FALSE;
    pe = image + pe_offset;
    return *(const WORD *)(pe + 4) == 0x01c4u && *(const WORD *)(pe + 0x18) == 0x010bu && *(const DWORD *)(pe + 0x50) == X104_IMAGE_SIZE_PATCHED;
}

static int x104_identify_image(BYTE *image)
{
    DWORD pe_offset;
    const BYTE *pe;
    WORD sections;
    WORD dll_characteristics;
    DWORD size_of_image;
    DWORD entry;
    if (!x104_image_matches(image)) {
        if (image && !IsBadReadPtr(image, 0x40) && image[0] == 'M') {
            pe_offset = *(const DWORD *)(image + 0x3c);
            pe = image + pe_offset;
            x104_logf("X104 image identification FAILED: machine %04x SizeOfImage %08lx (expected ARMNT 01c4 and %08lx = patched 0.10.4 exe; %08lx is the stock exe)",
                      (unsigned)*(const WORD *)(pe + 4), (unsigned long)*(const DWORD *)(pe + 0x50), (unsigned long)X104_IMAGE_SIZE_PATCHED,
                      (unsigned long)X104_IMAGE_SIZE_STOCK);
        } else {
            x104_logf("X104 image identification FAILED: no PE image at %p", (void *)image);
        }
        return X104_BOOT_BAD_IMAGE;
    }
    pe_offset = *(const DWORD *)(image + 0x3c);
    pe = image + pe_offset;
    sections = *(const WORD *)(pe + 6);
    size_of_image = *(const DWORD *)(pe + 0x50);
    entry = *(const DWORD *)(pe + 0x28);
    dll_characteristics = *(const WORD *)(pe + 0x5e);
    x104_logf("X104 game image: base=%p SizeOfImage=%08lx sections=%u entry=%08lx DllCharacteristics=%04x (expected 7 sections, entry 0020095d, 8140)",
              (void *)image, (unsigned long)size_of_image, (unsigned)sections, (unsigned long)entry, (unsigned)dll_characteristics);
    if (sections != 7) x104_logf("X104 warning: %u sections, the patched 0.10.4 exe has 7 (6 + .d3imp)", (unsigned)sections);
    if (dll_characteristics & 0x1000u) x104_logf("X104 warning: IMAGE_DLLCHARACTERISTICS_APPCONTAINER still set (patcher step hdr.dllchars missing)");
    if (!arm32_check_bytes(image + X104_RVA_CALLSITE, x104_callsite_patched_bytes, sizeof(x104_callsite_patched_bytes), "X104 patched call site 0x200A72"))
        x104_logf("X104 warning: the CRT call site does not hold the patcher's bl 0x20A364 (we were entered some other way?)");
    return 0;
}

/* ---- options (command line only; environment is not used for 0.10.4) ---- */
static void x104_parse_options(void)
{
    const wchar_t *cursor = GetCommandLineW();
    wchar_t argument[128];
    const wchar_t *value;
    char shown[64];
    int width, height;

    x104.opt_present = X104_PRESENT_HWND;
    x104.opt_half_res = FALSE;
    x104.opt_fullscreen = FALSE;
    x104.opt_trace = FALSE;
    x104.opt_width = 1280;
    x104.opt_height = 720;
    x104.opt_window_explicit = FALSE;
    x104.opt_max_fps = 0;
    x104.opt_vsync = FALSE;
    x104.opt_selftest = 0;
    cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));          /* skip argv[0] */
    while (*cursor) {
        cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
        if (host_argument_equals(argument, L"--half-res")) {
            x104.opt_half_res = TRUE;
        } else if (host_argument_equals(argument, L"--full-res")) {
            x104.opt_half_res = FALSE;
        } else if (host_argument_equals(argument, L"--fullscreen")) {
            x104.opt_fullscreen = TRUE;
        } else if (host_argument_equals(argument, L"--trace-fakes")) {
            x104.opt_trace = TRUE;
        } else if (host_argument_equals(argument, L"--vsync")) {
            x104.opt_vsync = TRUE;
        } else if (host_argument_equals(argument, L"--selftest-fakes")) {
            x104.opt_selftest = 1;
        } else if (host_argument_equals(argument, L"--selftest-fakes=only")) {
            x104.opt_selftest = 2;
        } else if ((value = arm32_iat_argument_value(argument, L"--max-fps=")) != NULL) {
            width = 0;
            while (*value >= L'0' && *value <= L'9') width = width * 10 + (*value++ - L'0');
            if (width >= 5 && width <= 1000) x104.opt_max_fps = width;
            else x104_logf("X104 option --max-fps=N needs 5..1000 - ignored");
        } else if ((value = arm32_iat_argument_value(argument, L"--present=")) != NULL) {
            if (host_argument_equals(value, L"hwnd")) x104.opt_present = X104_PRESENT_HWND;
            else if (host_argument_equals(value, L"dcomp")) x104.opt_present = X104_PRESENT_DCOMP;
            else x104_logf("X104 option --present=%s is not hwnd|dcomp - ignored", (x104_narrow(shown, sizeof(shown), value), shown));
        } else if ((value = arm32_iat_argument_value(argument, L"--window=")) != NULL) {
            width = 0;
            height = 0;
            while (*value >= L'0' && *value <= L'9') width = width * 10 + (*value++ - L'0');
            if (*value == L'x' || *value == L'X') ++value;
            while (*value >= L'0' && *value <= L'9') height = height * 10 + (*value++ - L'0');
            if (width >= 320 && height >= 240 && width <= 8192 && height <= 8192) {
                x104.opt_width = width;
                x104.opt_height = height;
                x104.opt_window_explicit = TRUE;
            } else {
                x104_logf("X104 option --window=WxH needs 320x240..8192x8192 - ignored");
            }
        }
    }
    x104_logf("X104 options: present=%s full-res=%d fullscreen=%d trace-fakes=%d vsync=%d max-fps=%d selftest-fakes=%d window=%dx%d%s (feature level cap 0x%04X, osk/input options as in ARM32.md)",
              x104.opt_present == X104_PRESENT_DCOMP ? "dcomp" : "hwnd", (int)!x104.opt_half_res, (int)x104.opt_fullscreen, (int)x104.opt_trace,
              (int)x104.opt_vsync, x104.opt_max_fps, x104.opt_selftest, x104.opt_width, x104.opt_height,
              x104.opt_window_explicit ? " (explicit)" : " (default, clamped to the work area when the window is created)", (unsigned)arm32_feature_level_cap);
}

/* ---- failure / diagnostics ---- */
static int x104_boot_fail(int code, const char *what)
{
    char log_path[MAX_PATH];
    char message[900];
    x104_logf("X104 bootstrap FAILED at step '%s': %s -> process exit code %d", arm32_boot_step_name, what, code);
    arm32_boot_watchdog_stop = 1;
    arm32_boot_narrow(log_path, sizeof(log_path), Win32CraftLogPathW);
    wsprintfA(message, "Win32Craft could not start Minecraft PE 0.10.4 (error %d at step '%s').\n\n%s\n\nLog file: %s", code, arm32_boot_step_name, what,
              log_path[0] ? log_path : "(none)");
    MessageBoxA(NULL, message, "Win32Craft", 0x00000010u | 0x00010000u);
    log_line("X104 bootstrap: terminating the process after the failure");
    TerminateProcess(GetCurrentProcess(), (UINT)code);
    return code;
}

/* IAT diagnostics: every abort / terminate / pure call / exit and every C++/CX exception the game raises from a failing HRESULT is
 * logged with the caller's game rva and a stack scan, then forwarded to the original.  A host fake that returns a failure HRESULT
 * shows up here as "WinRT raise COMException 0x80004001 from game rva ..." right before the game dies. */
static void *x104_orig_abort, *x104_orig_terminate, *x104_orig_purecall, *x104_orig_exit_app, *x104_orig_exit, *x104_orig_raise_com;

static void x104_diag_report(const char *what, ULONG_PTR caller, ULONG_PTR frame)
{
    x104_logf("X104 *** %s called from %08lx (game rva %08lx) ***", what, (unsigned long)caller, (unsigned long)x104_game_rva_of(caller));
    arm32_log_stack_scan(what, frame, 160, 14);
}

static void X104_API x104_hook_abort(void)
{
    x104_diag_report("abort()", (ULONG_PTR)__builtin_return_address(0), (ULONG_PTR)__builtin_frame_address(0));
    ((void (X104_API *)(void))x104_orig_abort)();
}

static void X104_API x104_hook_terminate(void)
{
    x104_diag_report("std::terminate()", (ULONG_PTR)__builtin_return_address(0), (ULONG_PTR)__builtin_frame_address(0));
    ((void (X104_API *)(void))x104_orig_terminate)();
}

static int X104_API x104_hook_purecall(void)
{
    x104_diag_report("_purecall()", (ULONG_PTR)__builtin_return_address(0), (ULONG_PTR)__builtin_frame_address(0));
    return ((int (X104_API *)(void))x104_orig_purecall)();
}

static void X104_API x104_hook_exit_app(UINT a, UINT b, UINT c, UINT d)
{
    x104_logf("X104 game called _exit_app(%08lx, %08lx) from game rva %08lx", (unsigned long)a, (unsigned long)b,
              (unsigned long)x104_game_rva_of((ULONG_PTR)__builtin_return_address(0)));
    ((void (X104_API *)(UINT, UINT, UINT, UINT))x104_orig_exit_app)(a, b, c, d);
}

static void X104_API x104_hook_exit(UINT a, UINT b, UINT c, UINT d)
{
    x104_logf("X104 game called _exit(%d) from game rva %08lx", (int)a, (unsigned long)x104_game_rva_of((ULONG_PTR)__builtin_return_address(0)));
    ((void (X104_API *)(UINT, UINT, UINT, UINT))x104_orig_exit)(a, b, c, d);
}

static void X104_API x104_hook_raise_com(HRESULT hr)
{
    static volatile LONG count;
    if (InterlockedIncrement(&count) <= 60) {
        x104_logf("X104 *** game raises Platform::COMException(0x%08lx %s) from %08lx (game rva %08lx) ***", (unsigned long)hr, x104_hr_name(hr),
                  (unsigned long)(ULONG_PTR)__builtin_return_address(0), (unsigned long)x104_game_rva_of((ULONG_PTR)__builtin_return_address(0)));
        if (count <= 8) arm32_log_stack_scan("raise COMException", (ULONG_PTR)__builtin_frame_address(0), 160, 14);
    }
    ((void (X104_API *)(HRESULT))x104_orig_raise_com)(hr);
}

/* the typed Platform::XException raisers (void f(void)) */
#define X104_RAISE_HOOK(short_name, mangled_part, rva) \
    static void *x104_orig_raise_##short_name; \
    static void X104_API x104_hook_raise_##short_name(void) \
    { \
        static volatile LONG count; \
        if (InterlockedIncrement(&count) <= 30) { \
            x104_logf("X104 *** game raises Platform::" #short_name "Exception from %08lx (game rva %08lx) ***", \
                      (unsigned long)(ULONG_PTR)__builtin_return_address(0), (unsigned long)x104_game_rva_of((ULONG_PTR)__builtin_return_address(0))); \
            if (count <= 6) arm32_log_stack_scan("raise " #short_name, (ULONG_PTR)__builtin_frame_address(0), 160, 14); \
        } \
        ((void (X104_API *)(void))x104_orig_raise_##short_name)(); \
    }
X104_RAISE_HOOK(ObjectDisposed, "ObjectDisposed", 0x20B684)
X104_RAISE_HOOK(Disconnected, "Disconnected", 0x20B688)
X104_RAISE_HOOK(WrongThread, "WrongThread", 0x20B68C)
X104_RAISE_HOOK(ClassNotRegistered, "ClassNotRegistered", 0x20B690)
X104_RAISE_HOOK(ChangedState, "ChangedState", 0x20B694)
X104_RAISE_HOOK(OutOfBounds, "OutOfBounds", 0x20B698)
X104_RAISE_HOOK(InvalidArgument, "InvalidArgument", 0x20B69C)
X104_RAISE_HOOK(OutOfMemory, "OutOfMemory", 0x20B6A0)
X104_RAISE_HOOK(AccessDenied, "AccessDenied", 0x20B6A4)
X104_RAISE_HOOK(Failure, "Failure", 0x20B6A8)
X104_RAISE_HOOK(OperationCanceled, "OperationCanceled", 0x20B6AC)
X104_RAISE_HOOK(NullReference, "NullReference", 0x20B6B0)
X104_RAISE_HOOK(InvalidCast, "InvalidCast", 0x20B6B4)
X104_RAISE_HOOK(NotImplemented, "NotImplemented", 0x20B6B8)

static BOOL x104_hook_one(BYTE *image, const char *name, DWORD rva, void *hook, void **original, const char *what)
{
    return arm32_iat_hook_import(image, FALSE, name, rva, hook, original, what);
}

static void x104_install_diagnostic_hooks(BYTE *image)
{
    unsigned installed = 0;
    unsigned wanted = 0;
#define X104_DIAG(name, rva, hook, original, what) do { ++wanted; if (x104_hook_one(image, (name), (rva), (void *)(hook), (void **)&(original), (what))) ++installed; } while (0)
#define X104_RAISE(short_name, rva) \
    X104_DIAG("?__abi_WinRTraise" #short_name "Exception@@YAXXZ", (rva), x104_hook_raise_##short_name, x104_orig_raise_##short_name, "raise" #short_name);
    X104_DIAG("abort", X104_RVA_IAT_ABORT, x104_hook_abort, x104_orig_abort, "abort");
    X104_DIAG("?terminate@@YAXXZ", X104_RVA_IAT_TERMINATE, x104_hook_terminate, x104_orig_terminate, "terminate");
    X104_DIAG("_purecall", X104_RVA_IAT_PURECALL, x104_hook_purecall, x104_orig_purecall, "_purecall");
    X104_DIAG("_exit_app", X104_RVA_IAT_EXIT_APP, x104_hook_exit_app, x104_orig_exit_app, "_exit_app");
    X104_DIAG("_exit", X104_RVA_IAT_EXIT, x104_hook_exit, x104_orig_exit, "_exit");
    X104_DIAG("?__abi_WinRTraiseCOMException@@YAXJ@Z", X104_RVA_IAT_RAISE_COM, x104_hook_raise_com, x104_orig_raise_com, "raiseCOMException");
    X104_RAISE(ObjectDisposed, 0x20B684) X104_RAISE(Disconnected, 0x20B688) X104_RAISE(WrongThread, 0x20B68C)
    X104_RAISE(ClassNotRegistered, 0x20B690) X104_RAISE(ChangedState, 0x20B694) X104_RAISE(OutOfBounds, 0x20B698)
    X104_RAISE(InvalidArgument, 0x20B69C) X104_RAISE(OutOfMemory, 0x20B6A0) X104_RAISE(AccessDenied, 0x20B6A4)
    X104_RAISE(Failure, 0x20B6A8) X104_RAISE(OperationCanceled, 0x20B6AC) X104_RAISE(NullReference, 0x20B6B0)
    X104_RAISE(InvalidCast, 0x20B6B4) X104_RAISE(NotImplemented, 0x20B6B8)
#undef X104_RAISE
#undef X104_DIAG
    x104_logf("X104 diagnostic IAT hooks: %u of %u installed (abort/terminate/_purecall/_exit*/Platform exception raisers)", installed, wanted);
}

static BOOL x104_install_iat_hooks(BYTE *image)
{
    BOOL activation_ok;
    activation_ok = arm32_iat_hook_import(image, FALSE, "?GetActivationFactoryByPCWSTR@@YAJPAXAAVGuid@Platform@@PAPAX@Z",
                                          X104_RVA_IAT_GETACTIVATIONFACTORY, (void *)x104_activation_hook, &x104.original_activation,
                                          "GetActivationFactoryByPCWSTR (X104 router)");
    if (!activation_ok) {
        x104_logf("X104 FATAL: the WinRT activation router could not be installed");
        return FALSE;
    }
    x104_logf("X104: WinRT activation router installed; original vccorlib function %p", x104.original_activation);
    if (!install_exception_trace(image, X104_RVA_IAT_CXXTHROW)) x104_logf("X104 warning: _CxxThrowException trace not installed");
    if (!install_d3d_compile_trace(image, X104_RVA_IAT_D3DCOMPILE)) x104_logf("X104 warning: D3DCompile logger not installed");
    x104_install_diagnostic_hooks(image);
    return TRUE;
}

/* ---- VC120 runtime diagnostics (review finding 12) ----
 * msvcr120_app and vccorlib120_app import RoGetActivationFactory from api-ms-win-core-winrt-l1-1-0 and call it on their own (ConcRT / PPL scheduling, vccorlib's
 * activation helpers, tmpfile paths) - NOT through the game's single choke point that the activation router covers.  A real activation failure there would show
 * up as a hang or a crash with nothing in the log, so the import cell of every VC120 module that has one is patched with a log-only forwarder (class name, IID,
 * HRESULT, caller module+offset), and the loaded module list (base, size) is logged once at boot.  (msvcp120_app has no such import: checked in the genuine DLL.) */
static void *x104_orig_ro_get_activation_factory;
static volatile LONG x104_ro_calls;
static volatile LONG x104_ro_failures;

static HRESULT X104_API x104_hook_ro_get_activation_factory(HSTRING class_id, const GUID *iid, void **factory)
{
    HRESULT hr = ((HRESULT (X104_API *)(HSTRING, const GUID *, void **))x104_orig_ro_get_activation_factory)(class_id, iid, factory);
    LONG number = InterlockedIncrement(&x104_ro_calls);
    if (number <= 120 || (FAILED(hr) && InterlockedIncrement(&x104_ro_failures) <= 200)) {
        char name[128];
        char iid_text[96];
        char where[160];
        x104_hstring_ascii(class_id, name, sizeof(name));
        x104_describe_iid(iid_text, iid);
        arm32_iat_describe_address(__builtin_return_address(0), where, sizeof(where));
        x104_logf("X104 VC120 RoGetActivationFactory #%ld %s %s from %s -> 0x%08lx %s", (long)number, name, iid_text, where, (unsigned long)hr, x104_hr_name(hr));
    }
    return hr;
}

static void x104_log_loaded_modules(void)
{
    typedef BOOL (WINAPI *EnumFn)(HANDLE, HMODULE *, DWORD, DWORD *);
    typedef BOOL (WINAPI *InfoFn)(HANDLE, HMODULE, void *, DWORD);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    EnumFn enumerate = kernel32 ? (EnumFn)GetProcAddress(kernel32, "K32EnumProcessModules") : NULL;
    InfoFn information = kernel32 ? (InfoFn)GetProcAddress(kernel32, "K32GetModuleInformation") : NULL;
    HMODULE modules[256];
    DWORD needed = 0;
    unsigned count, index;
    if (!enumerate || !enumerate(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
        x104_logf("X104 modules: K32EnumProcessModules not available (error %lu) - module list skipped", (unsigned long)GetLastError());
        return;
    }
    count = needed / sizeof(HMODULE);
    if (count > ARRAYSIZE(modules)) count = ARRAYSIZE(modules);
    x104_logf("X104 modules: %u loaded at the start of the bootstrap", count);
    for (index = 0; index < count; ++index) {
        wchar_t path[MAX_PATH];
        char shown[200];
        struct { void *base; DWORD size; void *entry; } info = { 0, 0, 0 };
        path[0] = 0;
        GetModuleFileNameW(modules[index], path, ARRAYSIZE(path));
        if (information) information(GetCurrentProcess(), modules[index], &info, sizeof(info));
        x104_narrow(shown, sizeof(shown), path);
        x104_logf("X104 module %p size %08lx %s", (void *)modules[index], (unsigned long)info.size, shown);
    }
}

static void x104_install_vc120_diagnostics(void)
{
    static const wchar_t *const names[] = { L"msvcr120_app.dll", L"msvcp120_app.dll", L"vccorlib120_app.dll" };
    unsigned index;
    unsigned installed = 0;
    x104_log_loaded_modules();
    for (index = 0; index < ARRAYSIZE(names); ++index) {
        HMODULE module = GetModuleHandleW(names[index]);
        char shown[64];
        void **cell;
        void *original;
        x104_narrow(shown, sizeof(shown), names[index]);
        if (!module) {
            x104_logf("X104 VC120: %s is NOT loaded (it is a load-time import of the game exe: the loader failed? check the loadcheck result)", shown);
            continue;
        }
        cell = arm32_iat_find_cell((BYTE *)module, FALSE, "RoGetActivationFactory");
        if (!cell) cell = arm32_iat_find_cell((BYTE *)module, TRUE, "RoGetActivationFactory");
        if (!cell) {
            x104_logf("X104 VC120: %s at %p has no RoGetActivationFactory import (it cannot activate WinRT classes itself)", shown, (void *)module);
            continue;
        }
        original = *cell;
        if (!original) {
            x104_logf("X104 VC120: %s RoGetActivationFactory cell %p is NULL (delay import not resolved yet) - not hooked", shown, (void *)cell);
            continue;
        }
        if (x104_orig_ro_get_activation_factory && original != x104_orig_ro_get_activation_factory) {
            x104_logf("X104 VC120: %s resolves RoGetActivationFactory to %p, another module to %p - only the first target is forwarded; this module is NOT hooked", shown, original,
                      x104_orig_ro_get_activation_factory);
            continue;
        }
        if (arm32_patch_cell(cell, original, (void *)((ULONG_PTR)x104_hook_ro_get_activation_factory | 1u), NULL, "VC120 RoGetActivationFactory log forwarder")) {
            x104_orig_ro_get_activation_factory = original;
            ++installed;
            x104_logf("X104 VC120: %s at %p: RoGetActivationFactory forwarder installed (cell %p, original %p)", shown, (void *)module, (void *)cell, original);
        }
    }
    x104_logf("X104 VC120 diagnostics: %u RoGetActivationFactory forwarder(s) installed (a real activation by msvcr120_app / vccorlib120_app now appears as 'X104 VC120 RoGetActivationFactory')",
              installed);
}

/* ---- window ---- */
static volatile LONG x104_keymenu_logged;

/* X104_WM_QUIT reached the window proc: either the main loop never saw it because Windows is inside a modal loop (system menu, window move/size) that dispatches it
 * here, or it was sent.  Record the code, tell the loop, and PostQuitMessage so that the modal loop itself ends (modal loops leave on WM_QUIT and re-post it). */
static void x104_boot_loop_exit(int exit_code)
{
    x104.exit_code = exit_code;
    InterlockedExchange(&x104.f_loop_exit, 1);
    PostQuitMessage(exit_code);
}

static LRESULT CALLBACK x104_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    BOOL handled = FALSE;
    BOOL broadcast = FALSE;
    BOOL part;

    /* 1. messages the boot owns, valid at any time (also before the units are initialised) */
    switch (message) {
    case X104_WM_QUIT:
        x104_boot_loop_exit((int)wparam);
        return 0;
    case X104_WM_WAKE:
        return 0;
    case WM_TIMER:
        if (wparam == X104_TIMER_PUMP) {
            x104_ui_pass();                      /* queue + timers + unit ticks, also inside the modal loops of window move/size and menus */
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;                                /* the game draws every pixel; never flash the class brush */
    case 0x0112u: /* WM_SYSCOMMAND */
        if ((wparam & 0xFFF0u) == 0xF100u /* SC_KEYMENU */) {
            /* Alt / F10 / Alt+key would start the menu-mode modal loop; this window has no menu and the keys already went to the game */
            if (InterlockedIncrement(&x104_keymenu_logged) <= 3) x104_logf("X104 window: SC_KEYMENU consumed (lparam %08lx) - no menu mode", (unsigned long)lparam);
            return 0;
        }
        break;
    default:
        break;
    }

    /* 2. NEVER route to the units before they are initialised: CreateWindowEx and the first ShowWindow send WM_GETMINMAXINFO / WM_NCCREATE / WM_SIZE ... synchronously
     *    (rule "safe before init", HOST_DESIGN.md 4).  The panel reads the real client size in x104_panel_init, so nothing is lost. */
    if (!x104.f_units_ready) {
        if (message == WM_PAINT) {
            PAINTSTRUCT paint;
            BeginPaint(window, &paint);
            EndPaint(window, &paint);
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    /* 3. messages several units care about are broadcast to all (panel, input, app); the first unit that claims a result decides it.
     * Every other message stops at the first unit that returns TRUE (input, panel, app). */
    switch (message) {
    case 0x0005u: /* WM_SIZE */ case 0x0006u: /* WM_ACTIVATE */ case 0x001Cu: /* WM_ACTIVATEAPP */ case 0x0018u: /* WM_SHOWWINDOW */
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENTERSIZEMOVE: case WM_EXITSIZEMOVE: case WM_WINDOWPOSCHANGED:
    case 0x007Eu: /* WM_DISPLAYCHANGE */ case 0x02E0u: /* WM_DPICHANGED */
        broadcast = TRUE;
        break;
    default:
        break;
    }
    if (broadcast) {
        LRESULT part_result = 0;
        part = x104_panel_window_message(window, message, wparam, lparam, &part_result);
        if (part && !handled) { handled = TRUE; result = part_result; }
        part_result = 0;
        part = x104_input_window_message(window, message, wparam, lparam, &part_result);
        if (part && !handled) { handled = TRUE; result = part_result; }
        part_result = 0;
        part = x104_app_window_message(window, message, wparam, lparam, &part_result);
        if (part && !handled) { handled = TRUE; result = part_result; }
        if (handled) return result;
    } else {
        if (x104_input_window_message(window, message, wparam, lparam, &result)) return result;
        result = 0;
        if (x104_panel_window_message(window, message, wparam, lparam, &result)) return result;
        result = 0;
        if (x104_app_window_message(window, message, wparam, lparam, &result)) return result;
        result = 0;
    }

    /* 4. defaults when no unit handled the message */
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_SYSKEYDOWN:
    case 0x0105u: /* WM_SYSKEYUP */
        /* the input unit delivers the key and consumes it; if it did not, still never let DefWindowProc turn Alt / F10 into menu mode.  Alt+F4 stays the way to close. */
        if (wparam == VK_F4 && (lparam & (1 << 29)) /* ALT down */) return DefWindowProcW(window, message, wparam, lparam);
        return 0;
    case 0x0106u: /* WM_SYSCHAR */
        return 0;
    case WM_CLOSE:
        /* default when the app unit does not take it over: leave through the normal path (app unit stub / real unit decide) */
        x104_logf("X104 WM_CLOSE reached the default handler (the app unit did not handle it) - requesting exit");
        x104_app_request_exit(0);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

extern BOOL WINAPI SystemParametersInfoW(UINT, UINT, void *, UINT);

static HWND x104_create_window(HINSTANCE instance)
{
    WNDCLASSEXW klass;
    HWND window;
    RECT rect;
    RECT work;
    DWORD style = WS_OVERLAPPEDWINDOW;          /* NOT WS_VISIBLE: the window is shown by Window.Activate (app unit), after OnLaunched built the UI; a visible window that
                                                   is not pumped during D3D11CreateDevice / AppMain creation would turn into a "Not Responding" ghost */
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width;
    int height;
    int frame_w, frame_h, client_w, client_h;

    ZeroMemory(&klass, sizeof(klass));
    klass.cbSize = sizeof(klass);
    klass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    klass.lpfnWndProc = x104_window_proc;
    klass.hInstance = instance;
    klass.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    klass.hbrBackground = (HBRUSH)(ULONG_PTR)(COLOR_WINDOWTEXT + 1);
    klass.lpszClassName = L"MCPE0104Win32";
    arm32_boot_step("window class");
    if (!RegisterClassExW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        x104_logf("X104 RegisterClassExW failed, GetLastError=%lu", (unsigned long)GetLastError());
        return NULL;
    }
    ZeroMemory(&work, sizeof(work));
    if (!SystemParametersInfoW(0x0030u /* SPI_GETWORKAREA */, 0, &work, 0) || work.right <= work.left || work.bottom <= work.top) {
        work.left = 0;
        work.top = 0;
        work.right = GetSystemMetrics(0 /* SM_CXSCREEN */);
        work.bottom = GetSystemMetrics(1 /* SM_CYSCREEN */);
    }
    client_w = x104.opt_width;
    client_h = x104.opt_height;
    if (x104.opt_fullscreen) {
        style = WS_POPUP;
        x = 0;
        y = 0;
        client_w = GetSystemMetrics(0 /* SM_CXSCREEN */);
        client_h = GetSystemMetrics(1 /* SM_CYSCREEN */);
        width = client_w;
        height = client_h;
        frame_w = frame_h = 0;
    } else {
        rect.left = 0;
        rect.top = 0;
        rect.right = 100;
        rect.bottom = 100;
        AdjustWindowRectEx(&rect, style, FALSE, 0);
        frame_w = (rect.right - rect.left) - 100;
        frame_h = (rect.bottom - rect.top) - 100;
        if (!x104.opt_window_explicit) {
            /* default 1280x720 would put the bottom of the game below the taskbar on a 1366x768 Surface RT: fit into the work area */
            int max_w = (work.right - work.left) - frame_w;
            int max_h = (work.bottom - work.top) - frame_h;
            if (client_w > max_w) client_w = max_w;
            if (client_h > max_h) client_h = max_h;
            if (client_w < 320) client_w = 320;
            if (client_h < 240) client_h = 240;
        } else if (client_w + frame_w > work.right - work.left || client_h + frame_h > work.bottom - work.top) {
            x104_logf("X104 warning: --window=%dx%d plus the window frame (%dx%d) is larger than the work area (%ldx%ld): part of the game will be off screen", client_w, client_h,
                      frame_w, frame_h, (long)(work.right - work.left), (long)(work.bottom - work.top));
        }
        width = client_w + frame_w;
        height = client_h + frame_h;
        x = work.left + ((work.right - work.left) - width) / 2;
        y = work.top + ((work.bottom - work.top) - height) / 2;
        if (x < work.left) x = work.left;
        if (y < work.top) y = work.top;
    }
    x104.opt_width = client_w;
    x104.opt_height = client_h;
    x104_logf("X104 window: work area %ldx%ld at (%ld,%ld), frame %dx%d, effective client %dx%d, outer %dx%d at (%d,%d)%s%s", (long)(work.right - work.left),
              (long)(work.bottom - work.top), (long)work.left, (long)work.top, frame_w, frame_h, client_w, client_h, width, height, x, y,
              x104.opt_window_explicit ? " [--window given]" : "", x104.opt_fullscreen ? " [fullscreen]" : "");
    arm32_boot_step("create window");
    window = CreateWindowExW(0, klass.lpszClassName, L"Win32Craft - Minecraft PE 0.10.4", style, x, y, width, height, NULL, NULL, instance, NULL);
    if (!window) {
        x104_logf("X104 CreateWindowExW failed, GetLastError=%lu", (unsigned long)GetLastError());
        return NULL;
    }
    if (GetClientRect(window, &rect))
        x104_logf("X104 HWND created (hidden until Window.Activate): %p, client area %ldx%ld (requested %dx%d%s)", (void *)window, (long)(rect.right - rect.left),
                  (long)(rect.bottom - rect.top), client_w, client_h, x104.opt_fullscreen ? ", fullscreen" : "");
    return window;
}

/* ---- the game call ---- */
/* command line tail as the CRT passes it to WinMain: GetCommandLineA() without the program name */
static char *x104_command_tail(void)
{
    char *cursor = GetCommandLineA();
    if (!cursor) return (char *)"";
    if (*cursor == '"') {
        ++cursor;
        while (*cursor && *cursor != '"') ++cursor;
        if (*cursor == '"') ++cursor;
    } else {
        while (*cursor && *cursor != ' ' && *cursor != '\t') ++cursor;
    }
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    return cursor;
}

static int x104_call_game_winmain(HINSTANCE instance, BOOL *crashed)
{
    typedef int (*X104WinMainFn)(HINSTANCE, HINSTANCE, char *, int);
    X104WinMainFn winmain = (X104WinMainFn)X104_CODE(X104_RVA_WINMAIN_WRAPPER);
    int result = -1;
    X104Seh seh;
    *crashed = FALSE;
    memset(&seh, 0, sizeof(seh));
    __try {
        result = winmain(instance, NULL, x104_command_tail(), 1 /* SW_SHOWNORMAL */);
    } __except (x104_seh_filter((EXCEPTION_POINTERS *)_exception_info(), &seh)) {
        *crashed = TRUE;
        x104_logf("X104 EXCEPTION escaped from the game's WinMain: code=%08lx at %p (game rva %08lx) info=%08lx %08lx", (unsigned long)seh.code,
                  seh.address, (unsigned long)seh.rva, (unsigned long)seh.info0, (unsigned long)seh.info1);
    }
    return result;
}

static void x104_shutdown_units(void)
{
    InterlockedExchange(&x104.f_units_ready, 0);       /* window messages from here on get default handling only */
    x104_input_shutdown();
    x104_panel_shutdown();
    x104_app_shutdown();
    x104_async_shutdown();
    x104_platform_shutdown();
}

/* ---- the bootstrap ---- */
static int x104_bootstrap(BYTE *image)
{
    HINSTANCE instance = (HINSTANCE)image;
    HRESULT ro_result;
    HWND window;
    BOOL crashed = FALSE;
    int code;
    int result;
    typedef BOOL (WINAPI *SetProcessDPIAwareFn)(void);
    SetProcessDPIAwareFn set_dpi_aware;

    memset(&x104, 0, sizeof(x104));
    x104.image = image;
    x104.instance = instance;
    x104.main_thread_id = GetCurrentThreadId();
    x104.ui_thread_id = x104.main_thread_id;
    lstrcpyW(x104.game_root, package_path);
    host_is_0104 = TRUE;
    game_image = image;
    x104_logf("X104: Win32Craft Minecraft PE 0.10.4.40 (Windows Phone 8.1 XAML build) ARM32 host, built %s %s, thread %04lx = UI thread", __DATE__, __TIME__,
              (unsigned long)x104.main_thread_id);

    arm32_boot_step("X104 identify image");
    code = x104_identify_image(image);
    if (code) return x104_boot_fail(code, "image identification: this is not the patched Minecraft PE 0.10.4.40 ARM32 exe (SizeOfImage 0x00E6F000)");

    arm32_boot_step("X104 host options");
    Win32CraftParseHostOptions();           /* feature-level cap (default 9_1), --input/--osk/--touch-api, -maxfps: shared with 1.2.8 */
    x104_parse_options();
    {
        char root[MAX_PATH];
        x104_narrow(root, sizeof(root), x104.game_root);
        x104_logf("X104 SUMMARY: pid %lu | game image %p | game root (= exe dir = cwd = Package.InstalledLocation) %s | log %s",
                  (unsigned long)GetCurrentProcessId(), (void *)image, root, "see THIS LOG FILE line above");
    }

    arm32_boot_step("X104 HSTRING / RoInitialize");
    (void)WindowsIsStringEmpty(NULL);       /* resolves combase lazily + the HSTRING self-test line of api_compat.c, outside any loader lock */
    ro_result = RoInitialize(RO_INIT_MULTITHREADED);
    log_hresult("X104 RoInitialize(MTA) [expected S_FALSE 0x00000001: vccorlib InitializeData(2) already initialised this thread]", ro_result);

    set_dpi_aware = (SetProcessDPIAwareFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDPIAware");
    x104_logf("X104: SetProcessDPIAware %s (real pixels, no DPI virtualisation; the host reports 96 dpi / scale 1.0 to the game)",
              set_dpi_aware ? (set_dpi_aware() ? "ok" : "failed") : "not available");

    arm32_boot_step("X104 touch probe");
    arm32_input_probe();                    /* logs the input inventory; sets the initial pointer source (touch-only machines start as TOUCH) */

    arm32_boot_step("X104 platform_install");
    if (!x104_platform_install(image)) return x104_boot_fail(X104_BOOT_PLATFORM_FAILED, "platform setup failed (see the lines above: non-ASCII install path, data folder, patched cells)");

    arm32_boot_step("X104 IAT hooks");
    if (!x104_install_iat_hooks(image)) return x104_boot_fail(X104_BOOT_IAT_FAILED, "the WinRT activation router could not be installed");

    arm32_boot_step("X104 VC120 diagnostics");
    x104_install_vc120_diagnostics();       /* module list + log-only RoGetActivationFactory forwarders in msvcr120_app / vccorlib120_app (ConcRT, PPL) */

    arm32_boot_step("X104 panel_install");
    if (!x104_panel_install(image)) return x104_boot_fail(X104_BOOT_PANEL_FAILED, "graphics setup failed (D3D11CreateDevice hook / shader profile patch / presentation path; see the lines above)");

    arm32_boot_step("X104 async/input/app install");
    if (!x104_async_install(image) || !x104_input_install(image) || !x104_app_install(image))
        return x104_boot_fail(X104_BOOT_INSTALL_FAILED, "a unit install step failed (async / input / app; see the lines above)");

    window = x104_create_window(instance);
    if (!window) return x104_boot_fail(X104_BOOT_WINDOW_FAILED, "the game window could not be created");
    x104.window = window;
    game_window = window;                   /* the OSK controller and the shared helpers use win32_host.c's game_window */

    arm32_boot_step("X104 unit init");
    if (!x104_async_init(window)) return x104_boot_fail(X104_BOOT_INIT_FAILED, "async unit init failed");
    if (!x104_input_init(window)) return x104_boot_fail(X104_BOOT_INIT_FAILED, "input unit init failed");
    if (!x104_panel_init(window)) return x104_boot_fail(X104_BOOT_INIT_FAILED, "panel unit init failed");
    if (!x104_app_init(window)) return x104_boot_fail(X104_BOOT_INIT_FAILED, "app unit init failed");
    x104_flag_set(&x104.f_units_ready, "f_units_ready: all units initialised, window messages are routed to them from now on");

    arm32_boot_step("X104 verify WinMain wrapper");
    if (!arm32_check_bytes(image + X104_RVA_WINMAIN_WRAPPER, ARM0104_STARTUP_FLOW_WINMAIN_THUNK_EXPECTED, 12, "X104 WinMain thunk 0x203EFC"))
        return x104_boot_fail(X104_BOOT_WRAPPER_BYTES, "the game's WinMain thunk (rva 0x203EFC) does not hold the expected bytes: wrong exe build");

    if (x104.opt_selftest) {
        int failures;
        arm32_boot_step("X104 selftest fakes");
        failures = x104_selftest_fakes();
        if (x104.opt_selftest == 2 || failures) {
            x104_logf("X104 selftest: %d failure(s)%s", failures, x104.opt_selftest == 2 ? " - --selftest-fakes=only: exiting without starting the game" : " - starting the game anyway");
            if (x104.opt_selftest == 2) {
                x104_shutdown_units();
                arm32_boot_watchdog_stop = 1;
                MessageBoxA(NULL, failures ? "Win32Craft fake self test: FAILURES - see win32craft.log" : "Win32Craft fake self test: all checks passed (see win32craft.log)",
                            "Win32Craft", failures ? 0x00000010u : 0x00000040u);
                arm32_boot_start_exit_guard(failures ? X104_BOOT_SELFTEST_FAILED : 0);
                return failures ? X104_BOOT_SELFTEST_FAILED : 0;
            }
        }
    }

    arm32_boot_step("X104 game WinMain");
    x104_logf("X104: calling the game's WinMain wrapper (rva 0x203EFC, original-style arguments hInstance=%p, 0, command tail \"%s\", 1) on the UI thread; "
              "Application::Start will run the fake UI loop inside it", (void *)instance, x104_command_tail());
    result = x104_call_game_winmain(instance, &crashed);
    x104_logf("X104: the game's WinMain returned %d%s", result, crashed ? " (after an unhandled exception)" : "");

    arm32_boot_step("X104 shutdown");
    x104_shutdown_units();
    arm32_boot_watchdog_stop = 1;
    if (crashed) return x104_boot_fail(X104_BOOT_GAME_EXCEPTION, "the game raised an unhandled exception (details in the log)");
    x104_logf("X104: Win32Bootstrap returning %d", result);
    arm32_boot_start_exit_guard(result);
    return result;
}
