#include "win32_compat.h"
#ifdef WIN32CRAFT_ARM32
#include "win32_host_arm32.h"
#endif

#ifndef E_BOUNDS
#define E_BOUNDS ((HRESULT)0x8000000BL)
#endif
#ifndef DXGI_PRESENT_DO_NOT_WAIT
#define DXGI_PRESENT_DO_NOT_WAIT 0x00000008U
#endif
#ifndef DXGI_ERROR_WAS_STILL_DRAWING
#define DXGI_ERROR_WAS_STILL_DRAWING ((HRESULT)0x887A000AL)
#endif

/* Executable addresses are grouped by client ABI; unprefixed RVAs belong to 0.15.10. */
#define RVA_ACTIVATION_FACTORY_IAT       0x00952ab4
#define RVA_CXX_THROW_IAT                0x009525f4
#define RVA_D3D_COMPILE_IAT              0x009520b0
#define RVA_D3D11_CREATE_DEVICE_IAT      0x009529b0
#define RVA_PLATFORM_OBJECT_CTOR_IAT     0x009529bc
#define RVA_PLATFORM_ALLOCATE2_IAT       0x009529cc
#define RVA_GET_IBOX_ARRAY_VTABLE_IAT    0x00952a04
#define RVA_PLATFORM_ALLOCATE1_IAT       0x00952a78
#define RVA_GET_IBOX_VTABLE_IAT          0x00952a80
#define RVA_DISABLE_THREAD_LIBRARY_IAT   0x00952164
#define RVA_PROCESS_PENDING_GAME_UI_IAT  0x009529a4
#define RVA_SHOW_PROFILE_CARD_UI_IAT     0x009529a8
#define RVA_CREATE_FILE2_IAT             0x009521f4
#define RVA_WFOPEN_S_IAT                0x00952880
#define RVA_WFOPEN_IAT                  0x00952884
#define RVA_FOPEN_IAT                   0x009528a0
#define RVA_D3D_DEVICE_IID               0x00a0201c
#define RVA_D3D_CONTEXT_IID              0x00a01c4c
#define RVA_DISCARD_WINDOW_VIEWS         0x005ff7d0
#define RVA_OWNER_CTOR                   0x006dad60
#define RVA_INIT_DEVICE                  0x006db060
#define RVA_INIT_TARGETS                 0x006dae00
#define RVA_OWNER_GLOBAL                 0x00b63e4c
#define RVA_DEVICE_RESOURCES_GLOBAL      0x00b63e48
#define RVA_CREATE_DEVICE_RESOURCES      0x0062a9a0
#define RVA_CREATE_APP_MAIN              0x0062a870
#define RVA_APP_PLATFORM_GLOBAL          0x00c009f4
#define RVA_UPDATE_RENDER                0x005fb790
#define RVA_RELEASE_TARGETS              0x006d4f40
#define RVA_OPERATOR_NEW                 0x0087c113
#define RVA_ENQUEUE_INPUT                0x005ffe90
#define RVA_MOUSE_EVENT_FACTORY          0x0038dfb0
#define RVA_TEXT_EVENT_FACTORY           0x0038e270
#define RVA_KEY_DOWN_HANDLER             0x00389e30
#define RVA_KEY_UP_HANDLER               0x00389f30
#define RVA_CLIENT_RENDER                0x0003afd0
#define RVA_MATERIAL_TASK_CALL           0x002cca0b
#define RVA_SCHEDULE_TASK                0x00390670
#define RVA_MATERIAL_TASK_VTABLE         0x00b3a334
#define RVA_MATERIAL_TASK_INVOKE         0x002cd9b0
#define RVA_ENTITY_MATERIAL_GROUP        0x00c67cc8
#define RVA_TERRAIN_MATERIAL_GROUP       0x00c67d28
#define RVA_SCREEN_VIEW_VTABLE           0x00b35c58
#define RVA_CUBEMAP_SCREEN_VTABLE        0x00b32cf8
#define RVA_MINECRAFT_CALLBACK_VTABLE    0x00b279ac
/* Minecraft 0.15.10 ChatScreen.  The HWND host has no XAML TextBox
 * KeyDown callback, so Enter must invoke the native ChatScreen submit method
 * after queued TextItems have been consumed by the next game frame. */
#define RVA_01510_CHAT_SCREEN_VTABLE      0x00b2c604
#define RVA_01510_CHAT_SCREEN_FOCUS       0x000b8f10
#define RVA_01510_CHAT_SCREEN_UNFOCUS     0x000b8ed0
#define RVA_01510_CHAT_SCREEN_SUBMIT      0x000b9fa0

/* 0.15.10 routes Import/Export World through its older platform-level
 * FileBrowser dispatcher rather than Windows.Storage.Pickers.  Hook the
 * dispatcher itself so both buttons bypass the unavailable UWP/XAML path. */
#define RVA_01510_FILE_BROWSER             0x00043ce0
#define RVA_01510_FILE_BROWSER_SETTINGS_DTOR 0x00043ad0
#define RVA_01510_GAME_OPERATOR_DELETE     0x000245a0
#define RVA_01510_DEVELOPMENT_VERSION_GETTER 0x00126f60
#define RVA_115_ACTIVATION_FACTORY_IAT       0x00f95b38
#define RVA_115_D3D_COMPILE_IAT              0x00f950bc
#define RVA_115_D3D11_CREATE_DEVICE_IAT      0x00f95a28
#define RVA_115_PLATFORM_OBJECT_CTOR_IAT     0x00f95a88
#define RVA_115_PLATFORM_ALLOCATE2_IAT       0x00f95a7c
#define RVA_115_GET_IBOX_ARRAY_VTABLE_IAT    0x00f95a80
#define RVA_115_PLATFORM_ALLOCATE1_IAT       0x00f95a70
#define RVA_115_GET_IBOX_VTABLE_IAT          0x00f95b1c
#define RVA_115_PROCESS_PENDING_GAME_UI_IAT  0x00f95a1c
#define RVA_115_SHOW_PROFILE_CARD_UI_IAT     0x00f95a20
#define RVA_115_CREATE_FILE2_IAT             0x00f95160
#define RVA_115_CXX_THROW_IAT                0x00f95668
#define RVA_115_WFOPEN_S_IAT                 0x00f958e0
#define RVA_115_VFSCANF_IAT                  0x00f958f4
#define RVA_115_VSSCANF_IAT                  0x00f958f8
#define RVA_115_FOPEN_IAT                    0x00f958fc
#define RVA_115_WFOPEN_IAT                   0x00f95948
#define RVA_115_PURECALL_IAT                 0x00f95630
#define RVA_115_STD_TERMINATE_IAT            0x00f95634
#define RVA_115_INVALID_PARAMETER_IAT        0x00f95864
#define RVA_115_ABORT_IAT                    0x00f95868
#define RVA_115__EXIT_IAT                    0x00f9586c
#define RVA_115_EXIT_IAT                     0x00f95888
#define RVA_115_CRT_ATEXIT_IAT              0x00f9588c
#define RVA_115_INVALID_PARAMETER_NR_IAT     0x00f95890
#define RVA_115_TERMINATE_IAT                0x00f95894
#define RVA_115_D3D_DEVICE_IID               0x0105ca98
#define RVA_115_D3D_CONTEXT_IID              0x0105ca78
#define RVA_115_DXGI_DEVICE3_IID             0x0105c950
#define RVA_115_DISCARD_WINDOW_VIEWS          0x006d0da0
#define RVA_115_CREATE_DEVICE_RESOURCES      0x000adc90
#define RVA_115_CREATE_APP_MAIN_XAML         0x000ac6c0
#define RVA_115_APP_MAIN_FRAME               0x006cdee0
#define RVA_115_APP_MAIN_RESIZE              0x006cdfc0
#define RVA_115_PLATFORM_FRAME               0x006d1220
#define RVA_115_RELEASE_TARGETS               0x0070d510
#define RVA_115_ENQUEUE_INPUT                 0x006d58a0
#define RVA_115_MOUSE_EVENT_FACTORY           0x007afef0
#define RVA_115_TEXT_EVENT_FACTORY            0x007b0020
#define RVA_115_KEY_EVENT_FACTORY             0x007affd0
#define RVA_115_MAP_VIRTUAL_KEY                0x007ad7a0
#define RVA_115_MOUSE_TOGGLE                  0x006dfe20
#define RVA_115_MOUSE_ABSOLUTE                0x006dfe80
#define RVA_115_MOUSE_RELATIVE                0x006dfec0
#define RVA_115_SHOW_TEXT_KEYBOARD            0x006d4f20
#define RVA_115_HIDE_TEXT_KEYBOARD            0x006d5340
#define RVA_115_FULLSCREEN_COMMAND            0x006d55b0
#define RVA_115_PICK_FILE                     0x006d1740
#define RVA_115_DEVICE_RESOURCES_GLOBAL      0x0142edb0
#define RVA_115_APP_PLATFORM_GLOBAL          0x0142ede0
#define RVA_115_SOUND_JSON_LOAD_CALL          0x00664c82
#define RVA_128_ACTIVATION_FACTORY_IAT         0x011d2b54
#define RVA_128_D3D_COMPILE_IAT                0x011d20b4
#define RVA_128_D3D11_CREATE_DEVICE_IAT        0x011d2a44
#define RVA_128_PLATFORM_OBJECT_CTOR_IAT       0x011d2ae4
#define RVA_128_PLATFORM_ALLOCATE2_IAT         0x011d2aac
#define RVA_128_GET_IBOX_ARRAY_VTABLE_IAT      0x011d2acc
#define RVA_128_PLATFORM_ALLOCATE1_IAT         0x011d2aa8
#define RVA_128_GET_IBOX_VTABLE_IAT            0x011d2b1c
#define RVA_128_CREATE_FILE2_IAT               0x011d20d4
#define RVA_128_CXX_THROW_IAT                  0x011d2690
#define RVA_128_D3D_CONTEXT_IID                0x012d8c7c
#define RVA_128_D3D_ANNOTATION_IID             0x012d8c4c
#define RVA_128_DXGI_DEVICE3_IID               0x012d8b28
#define RVA_128_DISCARD_WINDOW_VIEWS           0x007d68e0
#define RVA_128_OWNER_CTOR                     0x0082cce0
#define RVA_128_INIT_DEVICE                    0x0082cff0
#define RVA_128_INIT_TARGETS                   0x0082cd90
#define RVA_128_RELEASE_TARGETS                0x0082c680
#define RVA_128_OWNER_GLOBAL                   0x017c50f4
#define RVA_128_CREATE_APP_MAIN                0x007f5f80
#define RVA_128_APP_MAIN_FRAME                 0x007cf360
#define RVA_128_APP_VISIBILITY                 0x007d07a0
#define RVA_128_ENQUEUE_INPUT                  0x007dd0c0
#define RVA_128_MOUSE_EVENT_FACTORY            0x008d6500
#define RVA_128_KEY_EVENT_FACTORY              0x008d65e0
#define RVA_128_TEXT_EVENT_FACTORY             0x008d6670
#define RVA_128_MAP_VIRTUAL_KEY                0x008d1720
#define RVA_128_MOUSE_TOGGLE                   0x007e8260
#define RVA_128_MOUSE_ABSOLUTE                 0x007e82c0
#define RVA_128_MOUSE_RELATIVE                 0x007e8300
#define RVA_128_PICK_FILE                      0x007d72e0
#define RVA_128_ZIP_GET_CONTENTS               0x0090bf40
#define RVA_128_ZIP_OWNING_GET_CONTENTS_SLOT   0x0157ba84
#define RVA_128_ZIP_GET_CONTENTS_SLOT          0x0157bad4
#define RVA_128_STRING_ASSIGN                  0x00085f20
#define RVA_128_DEBUG_SCREEN_RENDERER_VTABLE   0x0154b14c
#define RVA_128_DEBUG_SCREEN_RENDER             0x001b4800
#define RVA_128_HUD_DEBUG_RENDERER_VTABLE       0x0154b500
#define RVA_128_HUD_DEBUG_RENDER                0x0016cb10
#define RVA_128_DEBUG_SCREEN_OPTION_INIT        0x0056c230
#define RVA_128_FONT_VTABLE                     0x0154c6f8
#define RVA_128_FONT_DRAW                       0x001ff320
#define RVA_128_FONT_GET_TEXT_WIDTH             0x00200260
#define RVA_128_COLOR_WHITE                     0x01548468
#define RVA_128_DEVELOPMENT_VERSION_GETTER     0x00414df0
#define RVA_128_TCUI_PROCESS_PENDING_IAT       0x017243fc
#define RVA_128_TCUI_SHOW_PROFILE_IAT          0x01724400
#define RVA_128_TCUI_SHOW_PROFILE_USER_IAT     0x01724408
#define RVA_128_CLIENT_INSTANCE_VTABLE          0x01549354
#define RVA_128_LEVEL_RENDERER_VTABLE            0x01570574
#define IMAGE_SIZE_115_PATCHED               0x01587000
#define IMAGE_SIZE_128_PATCHED               0x01923000
#ifdef WIN32CRAFT_ARM32
/* ARM32 1.2.8 exe after the ARM patcher: SizeOfImage = align(.d3imp RVA 0x01730000 + 0x444, 0x1000) (facts winmain.pe.size_of_image).
 * The stock exe is 0x01730000 and a patch that had to add the .d3tr fallback section would give 0x01732000. */
#define IMAGE_SIZE_128_ARM_PATCHED           0x01731000
#define IMAGE_SIZE_128_ARM_STOCK             0x01730000
#define IMAGE_SIZE_128_ARM_FALLBACK_SECTION  0x01732000
#endif
#define IMAGE_SIZE_01510_PATCHED             0x00d2e000
#define IMAGE_SIZE_0132_PATCHED              0x008cc000
#define IMAGE_SIZE_0142_PATCHED              0x00abf000

/* The early-2016 host has its own version-specific object layouts. */
#ifndef WIN32CRAFT_ARM32
int WINAPI Win32BootstrapEarly2016(void);
#endif

#define OWNER_SIZE                       0x000000ac
#define OWNER_RENDERER_OFFSET            0x00000098
#define OWNER_DEVICE_OFFSET              0x000000a4
#define RENDERER_CONTEXT_OFFSET          0x00000100
#define RENDERER_CONTEXT2_OFFSET         0x00000130
#define RENDERER_SWAP_CHAIN_OFFSET       0x000000f4
#define RENDERER_RTV_OFFSET              0x00000108
#define RENDERER_DSV_OFFSET              0x00000110
#define APP_PLATFORM_HID_OFFSET          0x00000258
#define APPMAIN_FRAME_ENABLED_OFFSET     0x00000010

static IDXGISwapChain *game_swap_chain;
static ID3D11DeviceContext *game_context;
static ID3D11RenderTargetView *game_target;
static void *game_renderer;
static void *game_resource_owner;
static void *game_app_main;
static void *game_device_resources;
static HWND game_window;
static BOOL mouse_capture_active;
static BOOL host_relative_requested;
static BOOL initial_menu_cursor_guard;
static BOOL mouse_clip_dirty = TRUE;
static BYTE *client_instance_128;
static BYTE win32_key_down[256];
static WORD pending_high_surrogate;
static BOOL suppress_next_t_character;
static BOOL text_input_active;
static BOOL text_ctrl_down;
static WPARAM legacy_clipboard_handled_key;
static BOOL win32_fullscreen_active;
static LONG saved_window_style;
static LONG saved_window_exstyle;
static WINDOWPLACEMENT saved_window_placement;
static wchar_t package_path[MAX_PATH];
static wchar_t game_data_path[MAX_PATH];
static BOOL host_is_windows7;
static BOOL host_is_115;
static BOOL host_is_128;
static BOOL host_is_116;
#ifdef WIN32CRAFT_ARM32
static BOOL host_is_0104;   /* Minecraft PE 0.10.4.40 (Windows Phone 8.1 XAML) ARM32 image: win32_host_arm32_0104.inl */
#endif
static BOOL pending_main_menu_exit_confirmation;
static BOOL activation_116(LPCWSTR class_name, const GUID *iid, void **result);
static BOOL host_unlimited_fps;
static volatile LONG game_present_success_count;
static BOOL window_in_size_move;
static UINT pending_resize_width;
static UINT pending_resize_height;
static BOOL input_events_enabled;
static void *active_chat_screen_01510;
static volatile LONG pending_chat_submit_01510;
static BOOL suppress_return_keyup_01510;
static BOOL queue_character_event(DWORD codepoint);

#ifdef WIN32CRAFT_ARM32
/* facts appmain.current_app_platform / appmain.layout: AppMain+0x1c holds the AppPlatform_Winrt* on ARM too (ctor,
 * update_frame and dtor all use [AppMain+0x1c]); the offset does not depend on host_is_128 here. */
static void *current_app_platform(void)
{
    if (!game_app_main || IsBadReadPtr((BYTE *)game_app_main + 0x1c, sizeof(void *))) return NULL;
    return *(void **)((BYTE *)game_app_main + 0x1c);
}
#else
static void *current_app_platform(void)
{
    if (!game_app_main) return NULL;
    return *(void **)((BYTE *)game_app_main + (host_is_128 ? 0x1c : 4));
}
#endif

/* Minecraft 1.1.5 startup diagnostics for the AppMainXaml asset/parser path. */
typedef FILE *(__cdecl *Game115FopenFn)(const char *, const char *);
typedef FILE *(__cdecl *Game115WfopenFn)(const wchar_t *, const wchar_t *);
typedef int (__cdecl *Game115WfopenSFn)(FILE **, const wchar_t *, const wchar_t *);
typedef int (__cdecl *Game115VfscanfFn)(
    unsigned long long, FILE *, const char *, void *, void *);
typedef int (__cdecl *Game115VsscanfFn)(
    unsigned long long, const char *, size_t, const char *, void *, void *);

typedef struct Game115ScanTrace {
    LONG sequence;
    int result;
    const void *buffer;
    char sample[80];
    char format[32];
} Game115ScanTrace;

static Game115FopenFn original_115_fopen;
static Game115WfopenFn original_115_wfopen;
static Game115FopenFn original_01510_fopen;
static Game115WfopenFn original_01510_wfopen;
static Game115WfopenSFn original_01510_wfopen_s;
static Game115WfopenSFn original_115_wfopen_s;
static Game115VfscanfFn original_115_vfscanf;
static Game115VsscanfFn original_115_vsscanf;
static char game115_last_stdio_path[384];
static FILE *game115_last_stdio_stream;
static volatile LONG game115_scan_sequence;
static Game115ScanTrace game115_scan_ring[32];
static BOOL game115_stdio_trace_installed;
static volatile LONG game115_fopen_log_count;
static volatile LONG game115_wfopen_log_count;
static volatile LONG game115_stdio_failure_log_count;
static volatile LONG game115_appmain_factory_active;
static volatile LONG game115_watchdog_started;
static DWORD game115_main_thread_id;

typedef void (__cdecl *Game115VoidFn)(void);
typedef void (__cdecl *Game115ExitFn)(int);
typedef int (__cdecl *Game115PurecallFn)(void);
static Game115VoidFn original_115_abort;
static Game115ExitFn original_115__exit;
static Game115ExitFn original_115_exit;
static Game115VoidFn original_115_terminate;
static Game115VoidFn original_115_std_terminate;
static Game115VoidFn original_115_invalid_parameter;
static Game115VoidFn original_115_invalid_parameter_noreturn;
static Game115PurecallFn original_115_purecall;
static void log_line(const char *text);
static void log_hresult(const char *operation, HRESULT result);
static void log_pointer(const char *name, const void *value);
static BYTE *get_hid_controller(void *platform);
static LONG CALLBACK win32_vectored_exception(EXCEPTION_POINTERS *details);
static BOOL install_115_stdio_trace(BYTE *image);
static BOOL install_115_termination_trace(BYTE *image);
static void trace_115_asset_candidates(void);
static void dump_115_stdio_state(void);
static void log_115_termination_stack(const char *reason);
static void start_115_constructor_watchdog(void);

#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 shared helpers (unit 00_core).  Signatures are fixed by arm32/PORT_RULES.md.
 * ====================================================================================================== */
static BYTE *game_image;

/* ARM32 touch input (docs: arm32/facts/touch_game.md, touch_host.md, touch_impl.json; code: "ARM32 touch input" block in front
 * of window_proc).  Forward declarations for the fake InputPane, game_requests_relative_mouse and the bootstrap. */
static BOOL arm32_input_touch_active(void);                     /* TRUE while the last input came from a finger (or --input=touch) */
static BOOL arm32_relative_mouse_gate(BOOL requested);           /* requested && !touch_active, with transition logging */
static void arm32_osk_show(const char *why);                     /* game asked for the on-screen keyboard (InputPane.TryShow) */
static void arm32_osk_hide(const char *why);                     /* ... and to hide it (InputPane.TryHide) */
static BOOL arm32_osk_rect_buffer(float rect[4]);                /* touch keyboard rectangle in swap-chain buffer pixels, FALSE = hidden */
static BOOL arm32_osk_rect_for_game(float rect[4]);              /* the same for the 1.2.8 OccludedRect, held back while the opening finger is down */
static void arm32_osk_note_pointer_up(void);                     /* a finger was lifted (tap-to-reopen after the user closed the keyboard) */
static void arm32_input_pane_raise(BOOL showing);                /* raise the handlers the game registered with InputPane.Showing / Hiding */
/* 0.10.4 host (win32_host_arm32_0104_*.inl, included in front of Win32Bootstrap): entry points used by the shared code above it */
static void x104_inputpane_raise(BOOL showing);                  /* input unit: InputPane Showing/Hiding to the handlers MainPage registered */
static HRESULT WINAPI x104_activation_hook(LPCWSTR class_name, const GUID *iid, void **result);   /* core: WinRT activation router */
static const char *x104_profile_title(void);                        /* core: name of the WP8.1 game the x104 host runs (log banner) */

/* lower-case hex dump "de ad be ef " (at most (out_size-1)/3 bytes) */
static void arm32_hex_dump(char *out, SIZE_T out_size, const BYTE *bytes, SIZE_T count)
{
    static const char digits[] = "0123456789abcdef";
    SIZE_T index;
    SIZE_T used = 0;
    if (!out || !out_size) return;
    for (index = 0; index < count && used + 3 < out_size; ++index) {
        out[used++] = digits[bytes[index] >> 4];
        out[used++] = digits[bytes[index] & 15u];
        out[used++] = ' ';
    }
    out[used] = 0;
}

/* Patch a pointer-sized data cell (vtable / IAT / std::function cell).  When `expected` is not NULL the cell must still
 * hold it (so a wrong RVA or an already patched cell is reported instead of corrupted).  `original_out` receives the
 * previous value on success.  Protection is flipped to PAGE_READWRITE (never executable) and restored. */
static BOOL arm32_patch_cell(void **cell, const void *expected, void *replacement,
                             void **original_out, const char *what)
{
    DWORD old_protection = 0;
    DWORD ignored;
    void *original;
    char line[224];

    if (!what) what = "(unnamed)";
    if (!cell || ((ULONG_PTR)cell & 3u) || IsBadReadPtr(cell, sizeof(*cell))) {
        wsprintfA(line, "ARM32 patch %s: cell %p is null, misaligned or unreadable", what, (void *)cell);
        log_line(line);
        return FALSE;
    }
    original = *cell;
    if (expected && original != expected) {
        wsprintfA(line, "ARM32 patch %s: cell %p holds %p but expected %p - not patched",
                  what, (void *)cell, original, expected);
        log_line(line);
        return FALSE;
    }
    if (!VirtualProtect(cell, sizeof(*cell), PAGE_READWRITE, &old_protection)) {
        wsprintfA(line, "ARM32 patch %s: VirtualProtect(%p, RW) failed, error=%lu",
                  what, (void *)cell, (unsigned long)GetLastError());
        log_line(line);
        return FALSE;
    }
    *cell = replacement;
    VirtualProtect(cell, sizeof(*cell), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), cell, sizeof(*cell));
    if (original_out) *original_out = original;
    wsprintfA(line, "ARM32 patch %s: cell %p %p -> %p", what, (void *)cell, original, replacement);
    log_line(line);
    return TRUE;
}

/* Patch one byte of read-only data (e.g. a character of a string literal) after verifying its current value. */
static BOOL arm32_patch_byte(BYTE *address, BYTE expected, BYTE replacement, const char *what)
{
    DWORD old_protection = 0;
    DWORD ignored;
    BYTE original;
    char line[224];

    if (!what) what = "(unnamed)";
    if (!address || IsBadReadPtr(address, 1)) {
        wsprintfA(line, "ARM32 patch byte %s: address %p is null or unreadable", what, (void *)address);
        log_line(line);
        return FALSE;
    }
    original = *address;
    if (original != expected) {
        wsprintfA(line, "ARM32 patch byte %s: %p holds %02x but expected %02x - not patched",
                  what, (void *)address, (unsigned)original, (unsigned)expected);
        log_line(line);
        return FALSE;
    }
    if (!VirtualProtect(address, 1, PAGE_READWRITE, &old_protection)) {
        wsprintfA(line, "ARM32 patch byte %s: VirtualProtect(%p, RW) failed, error=%lu",
                  what, (void *)address, (unsigned long)GetLastError());
        log_line(line);
        return FALSE;
    }
    *address = replacement;
    VirtualProtect(address, 1, old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), address, 1);
    wsprintfA(line, "ARM32 patch byte %s: %p %02x -> %02x", what, (void *)address,
              (unsigned)original, (unsigned)replacement);
    log_line(line);
    return TRUE;
}

/* Compare image bytes with a table before hooking; on mismatch log the first bytes of both. */
static BOOL arm32_check_bytes(const BYTE *address, const BYTE *expected, SIZE_T size, const char *what)
{
    char got_text[80];
    char want_text[80];
    char line[320];
    SIZE_T shown;
    SIZE_T first_diff;

    if (!what) what = "(unnamed)";
    if (!address || !expected || IsBadReadPtr(address, size)) {
        wsprintfA(line, "ARM32 check %s: %p (%lu bytes) is null or unreadable", what, (const void *)address,
                  (unsigned long)size);
        log_line(line);
        return FALSE;
    }
    if (memcmp(address, expected, size) == 0) return TRUE;
    for (first_diff = 0; first_diff < size && address[first_diff] == expected[first_diff]; ++first_diff) {
    }
    shown = size < 24 ? size : 24;
    arm32_hex_dump(got_text, sizeof(got_text), address, shown);
    arm32_hex_dump(want_text, sizeof(want_text), expected, shown);
    wsprintfA(line, "ARM32 check %s: bytes at %p differ at +%lu: got [%s] expected [%s]", what, (const void *)address,
              (unsigned long)first_diff, got_text, want_text);
    log_line(line);
    return FALSE;
}

/* Fallback code hook: absolute jump written over a function entry.
 *   site % 4 == 0 : DF F8 00 F0 | literal                      (ldr.w pc,[pc,#0]; 8 bytes, literal at site+4)
 *   site % 4 == 2 : DF F8 04 F0 | 00 BF | literal              (ldr.w pc,[pc,#4]; 10 bytes, literal at site+6)
 * Thumb LDR(literal) computes Align(PC,4)+imm with PC = site+4, and a load into PC from a non-word-aligned address is
 * UNPREDICTABLE (ARM ARM A8.8.64).  For site%4==2 Align(site+4,4) = site+2, so the "#2" form from the brief would read
 * the literal at the unaligned address site+4; #4 reads site+6, which is word aligned.  The literal has bit 0 set so
 * the jump stays in Thumb state.  The two bytes at site+4 (nop) are never executed. */
static BOOL arm32_write_thumb_jump(BYTE *site, const void *target, const char *what)
{
    BYTE stub[10];
    BYTE before[10];
    char before_text[40];
    char line[256];
    DWORD literal;
    DWORD old_protection = 0;
    DWORD ignored;
    SIZE_T size;

    if (!what) what = "(unnamed)";
    if (!site || !target || ((ULONG_PTR)site & 1u) || IsBadReadPtr(site, sizeof(stub))) {
        wsprintfA(line, "ARM32 thumb jump %s: bad site %p (null, odd or unreadable) or target %p",
                  what, (void *)site, target);
        log_line(line);
        return FALSE;
    }
    literal = (DWORD)(ULONG_PTR)target | 1u;
    stub[0] = 0xdf; stub[1] = 0xf8; stub[3] = 0xf0;
    if (((ULONG_PTR)site & 3u) == 0) {
        stub[2] = 0x00;
        memcpy(stub + 4, &literal, 4);
        size = 8;
    } else {
        stub[2] = 0x04;
        stub[4] = 0x00; stub[5] = 0xbf;
        memcpy(stub + 6, &literal, 4);
        size = 10;
    }
    memcpy(before, site, sizeof(before));
    if (!VirtualProtect(site, size, PAGE_EXECUTE_READWRITE, &old_protection)) {
        wsprintfA(line, "ARM32 thumb jump %s: VirtualProtect(%p, RWX) failed, error=%lu",
                  what, (void *)site, (unsigned long)GetLastError());
        log_line(line);
        return FALSE;
    }
    memcpy(site, stub, size);
    VirtualProtect(site, size, old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, size);
    arm32_hex_dump(before_text, sizeof(before_text), before, size);
    wsprintfA(line, "ARM32 thumb jump %s: %p -> %08lx (%lu bytes, was [%s])", what, (void *)site,
              (unsigned long)literal, (unsigned long)size, before_text);
    log_line(line);
    return TRUE;
}

/* ---- ARM32 crash / stack diagnostics (shared by the exception handlers below) ---- */

/* game image range: the real module base (game_image, set by Win32Bootstrap) or the preferred 0x400000, size from the PE
 * header (SizeOfImage), falling back to IMAGE_SIZE_128_ARM_PATCHED when the header cannot be read */
static void arm32_game_image_range(ULONG_PTR *base_out, ULONG_PTR *size_out)
{
    ULONG_PTR base = game_image ? (ULONG_PTR)game_image : 0x00400000u;
    ULONG_PTR size = 0x01731000u;
    DWORD pe_offset;

    if (!IsBadReadPtr((const void *)base, 0x40)) {
        pe_offset = *(const DWORD *)(base + 0x3c);
        if (pe_offset >= 0x40 && pe_offset < 0x1000 &&
            !IsBadReadPtr((const void *)(base + pe_offset), 0x54) &&
            *(const DWORD *)(base + pe_offset) == 0x00004550u) {
            size = *(const DWORD *)(base + pe_offset + 0x50);
        }
    }
    *base_out = base;
    *size_out = size;
}

/* TRUE when `value` (Thumb bit tolerated) points into the game image; *rva_out = (value & ~1) - base */
static BOOL arm32_in_image(ULONG_PTR value, ULONG_PTR base, ULONG_PTR size, DWORD *rva_out)
{
    value &= ~(ULONG_PTR)1;
    if (value < base || value >= base + size) return FALSE;
    if (rva_out) *rva_out = (DWORD)(value - base);
    return TRUE;
}

/* " (game rva 00123456)" or "" */
static void arm32_rva_text(char *out, SIZE_T out_size, ULONG_PTR value, ULONG_PTR base, ULONG_PTR size)
{
    DWORD rva;
    out[0] = 0;
    if (out_size >= 24 && arm32_in_image(value, base, size, &rva))
        wsprintfA(out, " (game rva %08lx)", (unsigned long)rva);
}

static void arm32_log_context(const char *tag, const CONTEXT *context)
{
    ULONG_PTR base;
    ULONG_PTR size;
    char pc_text[32];
    char lr_text[32];
    char line[256];

    arm32_game_image_range(&base, &size);
    arm32_rva_text(pc_text, sizeof(pc_text), context->Pc, base, size);
    arm32_rva_text(lr_text, sizeof(lr_text), context->Lr, base, size);
    wsprintfA(line, "%s: pc=%08lx%s lr=%08lx%s sp=%08lx cpsr=%08lx", tag,
              (unsigned long)context->Pc, pc_text, (unsigned long)context->Lr, lr_text,
              (unsigned long)context->Sp, (unsigned long)context->Cpsr);
    log_line(line);
    wsprintfA(line, "  r0=%08lx r1=%08lx r2=%08lx r3=%08lx r4=%08lx r5=%08lx",
              (unsigned long)context->R0, (unsigned long)context->R1, (unsigned long)context->R2,
              (unsigned long)context->R3, (unsigned long)context->R4, (unsigned long)context->R5);
    log_line(line);
    wsprintfA(line, "  r6=%08lx r7=%08lx r8=%08lx r9=%08lx r10=%08lx r11=%08lx r12=%08lx",
              (unsigned long)context->R6, (unsigned long)context->R7, (unsigned long)context->R8,
              (unsigned long)context->R9, (unsigned long)context->R10, (unsigned long)context->R11,
              (unsigned long)context->R12);
    log_line(line);
}

/* There is no EBP chain on ARM: scan up to `max_words` stack words from `sp` and print every value that points into the
 * game image (Thumb bit tolerated) as a candidate return address, at most `max_hits` lines. */
static void arm32_log_stack_scan(const char *tag, ULONG_PTR sp, UINT max_words, UINT max_hits)
{
    const DWORD *stack = (const DWORD *)sp;
    ULONG_PTR base;
    ULONG_PTR size;
    UINT index;
    UINT hits = 0;
    DWORD rva;
    char line[160];

    if (!sp || (sp & 3u)) {
        wsprintfA(line, "  %s stack scan skipped: sp=%08lx", tag, (unsigned long)sp);
        log_line(line);
        return;
    }
    arm32_game_image_range(&base, &size);
    for (index = 0; index < max_words; ++index) {
        DWORD value;
        if (IsBadReadPtr(stack + index, sizeof(*stack))) break;
        value = stack[index];
        if (!arm32_in_image(value, base, size, &rva)) continue;
        wsprintfA(line, "  %s stack sp+%03x: %08lx -> game rva %08lx", tag,
                  (unsigned)(index * 4), (unsigned long)value, (unsigned long)rva);
        log_line(line);
        if (++hits >= max_hits) break;
    }
}
#endif

/* 0.15.10 imports four Windows 8 KernelBase exports through KERNEL32 names.
 * The import patch redirects those cells to these Windows 7 compatibility
 * exports. FMOD delay imports are resolved during bootstrap; fail safely
 * before then without loading UWP DLLs. */
void *WINAPI Win32CraftResolveDelayLoadedAPI(
    void *parent_module_base, void *delayload_descriptor,
    void *failure_dll_hook, void *failure_system_hook,
    void *thunk_address, DWORD flags)
{
    (void)parent_module_base;
    (void)delayload_descriptor;
    (void)failure_dll_hook;
    (void)failure_system_hook;
    (void)thunk_address;
    (void)flags;
    return NULL;
}

BOOL WINAPI Win32CraftResolveDelayLoadsFromDll(
    void *parent_module_base, LPCSTR target_dll_name, DWORD flags)
{
    (void)parent_module_base;
    (void)target_dll_name;
    (void)flags;
    return FALSE;
}

void *WINAPI Win32CraftDelayLoadFailureHook(
    LPCSTR dll_name, LPCSTR procedure_name)
{
    (void)dll_name;
    (void)procedure_name;
    return NULL;
}

typedef struct D3CRAFT_CREATEFILE2_EXTENDED_PARAMETERS {
    DWORD dwSize;
    DWORD dwFileAttributes;
    DWORD dwFileFlags;
    DWORD dwSecurityQosFlags;
    LPVOID lpSecurityAttributes;
    HANDLE hTemplateFile;
} D3CRAFT_CREATEFILE2_EXTENDED_PARAMETERS;

HANDLE WINAPI Win32CraftCreateFile2(
    LPCWSTR file_name, DWORD desired_access, DWORD share_mode,
    DWORD creation_disposition,
    const D3CRAFT_CREATEFILE2_EXTENDED_PARAMETERS *parameters)
{
    DWORD flags_and_attributes = 0;
    LPVOID security_attributes = NULL;
    HANDLE template_file = NULL;

    if (parameters) {
        flags_and_attributes =
            parameters->dwFileAttributes |
            parameters->dwFileFlags |
            parameters->dwSecurityQosFlags;
        security_attributes = parameters->lpSecurityAttributes;
        template_file = parameters->hTemplateFile;
    }

    return CreateFileW(
        file_name, desired_access, share_mode, security_attributes,
        creation_disposition, flags_and_attributes, template_file);
}

static BOOL install_create_file2_compat(BYTE *image, SIZE_T create_file2_rva)
{
#ifdef WIN32CRAFT_ARM32
    /* facts pe_imports.create_file2_iat: CreateFile2 is native on 8.1/10 and the x86 hook is not used; never write an
     * IAT cell on ARM through this x86-era helper. */
    (void)image;
    (void)create_file2_rva;
    log_line("ARM32: install_create_file2_compat refused (CreateFile2 is native)");
    return FALSE;
#endif
    void **slot = (void **)(image + create_file2_rva);
    DWORD old_protection;
    DWORD ignored;

    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protection)) {
        log_line("VirtualProtect CreateFile2 IAT failed");
        return FALSE;
    }
    *slot = Win32CraftCreateFile2;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    log_line("CreateFile2 Windows 7 compatibility installed");
    return TRUE;
}

#ifdef WIN32CRAFT_ARM32
static LONG WINAPI win32_unhandled_exception(EXCEPTION_POINTERS *pointers)
{
    CONTEXT *context;
    DWORD *stack;
    UINT index;
    char line[160];
    if (!pointers || !pointers->ExceptionRecord || !pointers->ContextRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    context = pointers->ContextRecord;
    wsprintfA(line, "unhandled exception 0x%08lx at %p",
        (unsigned long)pointers->ExceptionRecord->ExceptionCode,
        pointers->ExceptionRecord->ExceptionAddress);
    log_line(line);
    if (pointers->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        pointers->ExceptionRecord->NumberParameters >= 2) {
        wsprintfA(line, "  access violation: %s address %08lx",
            pointers->ExceptionRecord->ExceptionInformation[0] == 0 ? "read of"
            : (pointers->ExceptionRecord->ExceptionInformation[0] == 1 ? "write to" : "execute of"),
            (unsigned long)pointers->ExceptionRecord->ExceptionInformation[1]);
        log_line(line);
    }
    arm32_log_context("crash", context);
    stack = (DWORD *)(ULONG_PTR)context->Sp;
    if (!IsBadReadPtr(stack, 12 * sizeof(*stack))) {
        for (index = 0; index < 12; ++index) {
            wsprintfA(line, "  crash stack +%02x: %08lx",
                index * 4, (unsigned long)stack[index]);
            log_line(line);
        }
    }
    arm32_log_stack_scan("crash", context->Sp, 64, 64);
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
static LONG WINAPI win32_unhandled_exception(EXCEPTION_POINTERS *pointers)
{
    CONTEXT *context;
    DWORD *stack;
    DWORD *frame;
    UINT index;
    char line[128];
    if (!pointers || !pointers->ExceptionRecord || !pointers->ContextRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    context = pointers->ContextRecord;
    wsprintfA(line,
        "unhandled exception 0x%08lx at %p eip=%08lx esp=%08lx ebp=%08lx",
        (unsigned long)pointers->ExceptionRecord->ExceptionCode,
        pointers->ExceptionRecord->ExceptionAddress,
        (unsigned long)context->Eip, (unsigned long)context->Esp,
        (unsigned long)context->Ebp);
    log_line(line);
    wsprintfA(line,
        "  eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx",
        (unsigned long)context->Eax, (unsigned long)context->Ebx,
        (unsigned long)context->Ecx, (unsigned long)context->Edx,
        (unsigned long)context->Esi, (unsigned long)context->Edi);
    log_line(line);
    stack = (DWORD *)(ULONG_PTR)context->Esp;
    if (!IsBadReadPtr(stack, 12 * sizeof(*stack))) {
        for (index = 0; index < 12; ++index) {
            wsprintfA(line, "  crash stack +%02x: %08lx",
                index * 4, (unsigned long)stack[index]);
            log_line(line);
        }
    }
    frame = (DWORD *)(ULONG_PTR)context->Ebp;
    for (index = 0; index < 16 && frame &&
         !IsBadReadPtr(frame, 2 * sizeof(*frame)); ++index) {
        wsprintfA(line, "  crash frame %u: return=%08lx frame=%08lx",
            index, (unsigned long)frame[1], (unsigned long)(ULONG_PTR)frame);
        log_line(line);
        if (frame[0] <= (DWORD)(ULONG_PTR)frame) break;
        frame = (DWORD *)(ULONG_PTR)frame[0];
    }
    if (host_is_115 || host_is_116) dump_115_stdio_state();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

static void install_crash_trace(void)
{
    typedef void * (WINAPI *SetUnhandledExceptionFilterFn)(void *);
    typedef LONG (CALLBACK *VectoredExceptionHandlerFn)(
        EXCEPTION_POINTERS *);
    typedef void * (WINAPI *AddVectoredExceptionHandlerFn)(
        ULONG, VectoredExceptionHandlerFn);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    SetUnhandledExceptionFilterFn set_filter = kernel32
        ? (SetUnhandledExceptionFilterFn)GetProcAddress(
            kernel32, "SetUnhandledExceptionFilter")
        : NULL;
    AddVectoredExceptionHandlerFn add_handler = kernel32
        ? (AddVectoredExceptionHandlerFn)GetProcAddress(
            kernel32, "AddVectoredExceptionHandler")
        : NULL;
    if (set_filter) set_filter(win32_unhandled_exception);
    if (add_handler) add_handler(1, win32_vectored_exception);
}

typedef void *(__cdecl *GameMallocFn)(size_t);
typedef void *(__cdecl *GameOperatorNewFn)(size_t);
typedef void *(__attribute__((thiscall)) *GameOwnerCtorFn)(void *);
typedef void (__attribute__((thiscall)) *GameInitDeviceFn)(void *, void *);
typedef void (__attribute__((thiscall)) *GameInitTargetsFn)(void *, void *, void *);
typedef void (__attribute__((thiscall)) *GameReleaseTargetsFn)(void *);
typedef void *(__attribute__((thiscall)) *GameCreateDeviceResourcesFn)(void *);
typedef void *(__attribute__((fastcall)) *GameCreateAppMainFn)(void **, void *);
typedef void *GameCreateAppMainXamlFn;
typedef void (__attribute__((thiscall)) *GameUpdateRenderFn)(void *);
typedef void (__attribute__((thiscall)) *GameClientRenderFn)(
    void *, int, int);
typedef void (__attribute__((thiscall)) *GameScreenRenderFn)(
    void *, int, int);
typedef void (__attribute__((thiscall)) *GameScreenDrawFn)(
    void *, void *);
typedef void (__attribute__((thiscall)) *GameCallbackFn)(void *);
typedef void (__attribute__((thiscall)) *GameTickFn)(void *, int, int);
typedef void (__attribute__((thiscall)) *GameApplyRenderStateFn)(
    void *, void *, void *, void *, void *);
typedef void (__attribute__((thiscall)) *GameMaterialDrawFn)(
    void *, void *, void *, void *);
typedef void (__attribute__((thiscall)) *GameResizeFn)(
    void *, int, int, int);
typedef void (__attribute__((thiscall)) *GameWindowSizeChangedFn)(
    void *, int, int);
typedef void (__attribute__((thiscall)) *GameClientUpdateFn)(void *);
typedef void (__attribute__((thiscall)) *GamePlatformResumeFn)(void *);
typedef void (__attribute__((thiscall)) *GameChatScreenVoidFn)(void *);
typedef void (__attribute__((thiscall)) *GameLifecycleFn)(void *);
typedef void (__attribute__((thiscall)) *GameEnqueueInputFn)(void *, void *);
typedef void (__attribute__((fastcall)) *GameKeyHandlerFn)(void *, int);
typedef int (__attribute__((fastcall)) *Game115MapVirtualKeyFn)(int);
typedef BYTE (__attribute__((thiscall)) *Game115AppFrameFn)(void *);
typedef void (__attribute__((thiscall)) *Game115AppResizeFn)(
    void *, void *, void *);
typedef void (__attribute__((thiscall)) *Game115PlatformFrameFn)(void *);

#ifndef WIN32CRAFT_ARM32
/*
 * The 1.1.5 AppMainXaml helper puts its first two arguments in ECX/EDX,
 * but returns with a plain RET and leaves the remaining three arguments for
 * its caller.  This is neither clang fastcall (callee cleanup) nor regparm
 * (EAX/EDX), so keep the exact MSVC-generated ABI in a small thunk.
 */
__attribute__((naked, noinline))
static void *call_game_create_app_main_xaml(
    GameCreateAppMainXamlFn function,
    void **result, void *startup_context,
    void **core_window, void **xaml_panel, void **pointer_source)
{
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl 28(%ebp)\n\t"
        "pushl 24(%ebp)\n\t"
        "pushl 20(%ebp)\n\t"
        "movl 12(%ebp), %ecx\n\t"
        "movl 16(%ebp), %edx\n\t"
        "call *8(%ebp)\n\t"
        "addl $12, %esp\n\t"
        "leave\n\t"
        "retl\n\t");
}

/* 1.2.8 AppMain factory: ECX=result, EDX=startup context and one
 * caller-cleaned activation-holder pointer on the stack. */
__attribute__((naked, noinline))
static void *call_game_create_app_main_128(
    void *function, void **result, void *startup_context,
    void **activation_argument)
{
    __asm__ volatile(
        "pushl 16(%esp)\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %ecx\n\t"
        "movl 16(%esp), %edx\n\t"
        "call *%eax\n\t"
        "addl $4, %esp\n\t"
        "retl\n\t");
}

/*
 * MouseItem factory ABI in 0.15.10:
 *   ECX = output unique_ptr slot
 *   EDX = pointer to action byte
 *   stack = pressed byte*, payload dword*, x word*, y word*
 * It leaves all four stack arguments for its caller.
 */
__attribute__((naked, noinline))
static void **call_game_make_mouse_event(
    void *function, void **output, const BYTE *action, const BYTE *pressed,
    const DWORD *payload, const WORD *x, const WORD *y)
{
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl 32(%ebp)\n\t"
        "pushl 28(%ebp)\n\t"
        "pushl 24(%ebp)\n\t"
        "pushl 20(%ebp)\n\t"
        "movl 12(%ebp), %ecx\n\t"
        "movl 16(%ebp), %edx\n\t"
        "call *8(%ebp)\n\t"
        "addl $16, %esp\n\t"
        "leave\n\t"
        "retl\n\t");
}

/*
 * The 0.15.10 text-event factory uses a nonstandard x86 ABI:
 *   ECX = output unique_ptr slot
 *   EDX = UTF-8 C string
 *   [ESP+4] = pointer to the trailing flag byte
 * but it returns with plain RET instead of RET 4.  Clang's normal fastcall
 * assumes callee stack cleanup, which leaves ESP shifted and makes the caller
 * read a bogus event pointer.  This naked cdecl bridge performs the exact
 * register setup and explicitly removes the one stack argument afterwards.
 */
__attribute__((naked, noinline))
static void **call_game_make_text_event(
    void *function, void **output, const char *utf8, const BYTE *flag)
{
    __asm__ volatile(
        "pushl 16(%esp)\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %ecx\n\t"
        "movl 16(%esp), %edx\n\t"
        "call *%eax\n\t"
        "addl $4, %esp\n\t"
        "retl\n\t");
}

/*
 * Minecraft 1.1.5 changed TextItem construction in two ways:
 *   ECX = output unique_ptr slot
 *   EDX = pointer to an MSVC std::string object (not a C string)
 *   [ESP+4] = pointer to the primary flag byte
 *   [ESP+8] = pointer to the per-character sequence byte
 * The factory returns with plain RET, so both stack arguments are removed
 * explicitly by this bridge.
 */
__attribute__((naked, noinline))
static void **call_game_make_text_event_115(
    void *function, void **output, const void *text,
    const BYTE *primary_flag, const BYTE *sequence_flag)
{
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %ecx\n\t"
        "movl 12(%esp), %edx\n\t"
        "pushl 20(%esp)\n\t"
        "pushl 20(%esp)\n\t"
        "call *%eax\n\t"
        "addl $8, %esp\n\t"
        "retl\n\t");
}

/*
 * KeyItem factory ABI in 1.1.5:
 *   ECX = output unique_ptr slot
 *   EDX = pointer to the mapped key byte
 *   [ESP+4] = pointer to the pressed-state dword
 * Like the other item factories it leaves its stack argument to the caller.
 */
__attribute__((naked, noinline))
static void **call_game_make_key_event_115(
    void *function, void **output, const BYTE *key, const DWORD *state)
{
    __asm__ volatile(
        "pushl 16(%esp)\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %ecx\n\t"
        "movl 16(%esp), %edx\n\t"
        "call *%eax\n\t"
        "addl $4, %esp\n\t"
        "retl\n\t");
}

/*
 * Explicit bridge for MinecraftClient::setSize's x86 ABI:
 * ECX = MinecraftClient, followed by width, height, and scale/mode on stack.
 * The 0.15.10 implementation returns with RET 12.
 */
__attribute__((naked, noinline))
static void call_game_resize(
    void *function, void *game, int width, int height, int mode)
{
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %ecx\n\t"
        "pushl 20(%esp)\n\t"
        "pushl 20(%esp)\n\t"
        "pushl 20(%esp)\n\t"
        "call *%eax\n\t"
        "retl\n\t");
}
#else /* WIN32CRAFT_ARM32 */
/*
 * ARM32 (Thumb-2, AAPCS-VFP) has no thiscall/fastcall and needs no naked bridges: every game function is an
 * ordinary C call.  The prototypes below are the x86 ones so the existing callers compile unchanged; each bridge ORs
 * bit 0 into `function` (callers pass image+RVA or a vtable cell without/with the Thumb bit) and widens the x86
 * BYTE/WORD argument pointers into the 32-/16-bit locals the ARM factories actually load (facts appmain.*, input.*).
 */
#define ARM32_THUMB_FUNCTION(function) ((void *)((ULONG_PTR)(function) | 1u))

/* 1.1.5 AppMainXaml: not part of the 1.2.8 ARM path */
static void *call_game_create_app_main_xaml(
    GameCreateAppMainXamlFn function,
    void **result, void *startup_context,
    void **core_window, void **xaml_panel, void **pointer_source)
{
    (void)function; (void)result; (void)startup_context;
    (void)core_window; (void)xaml_panel; (void)pointer_source;
    log_line("ARM32: call_game_create_app_main_xaml is not available (1.1.5 path)");
    return NULL;
}

/* facts appmain.create_app_main: AppMain** f(AppMain** result (r0), StartupCtx* (r1), CoreWindow** holder (r2))
 * -> r0 (== result), no stack arguments.  The return value is the x86 "holder result" (== result). */
static void *call_game_create_app_main_128(
    void *function, void **result, void *startup_context,
    void **activation_argument)
{
    typedef void **(*AppMainFactoryFn)(void **, void *, void **);
    return ((AppMainFactoryFn)ARM32_THUMB_FUNCTION(function))(
        result, startup_context, activation_argument);
}

/* facts input.make_mouse_event: void** f(void** out (r0), const int* action (r1), const int* pressed (r2),
 * const int* payload (r3), const short* x ([sp+0]), const short* y ([sp+4])) -> r0.  The factory loads action,
 * pressed and payload with 32-bit LDR and x,y with LDRH; `pressed` carries the signed wheel delta byte. */
static void **call_game_make_mouse_event(
    void *function, void **output, const BYTE *action, const BYTE *pressed,
    const DWORD *payload, const WORD *x, const WORD *y)
{
    typedef void **(*MouseFactoryFn)(
        void **, const int *, const int *, const int *, const short *, const short *);
    int action32 = (int)*action;
    int pressed32 = (int)(signed char)*pressed;
    int payload32 = (int)*payload;
    short x16 = (short)*x;
    short y16 = (short)*y;
    return ((MouseFactoryFn)ARM32_THUMB_FUNCTION(function))(
        output, &action32, &pressed32, &payload32, &x16, &y16);
}

/* 0.15.10 text factory (C string + flag): not part of the 1.2.8 ARM path */
static void **call_game_make_text_event(
    void *function, void **output, const char *utf8, const BYTE *flag)
{
    (void)function; (void)output; (void)utf8; (void)flag;
    log_line("ARM32: call_game_make_text_event is not available (0.15.10 path)");
    return NULL;
}

/* facts input.make_text_event: void** f(void** out (r0), const MsvcSmallString* text (r1),
 * const BYTE* primary_flag (r2), const BYTE* sequence_flag (r3)) -> r0; no stack arguments */
static void **call_game_make_text_event_115(
    void *function, void **output, const void *text,
    const BYTE *primary_flag, const BYTE *sequence_flag)
{
    typedef void **(*TextFactoryFn)(void **, const void *, const BYTE *, const BYTE *);
    return ((TextFactoryFn)ARM32_THUMB_FUNCTION(function))(
        output, text, primary_flag, sequence_flag);
}

/* facts input.make_key_event: void** f(void** out (r0), const int* key (r1: low byte used),
 * const int* state (r2)) -> r0; the key is loaded with a 32-bit LDR so it is widened here */
static void **call_game_make_key_event_115(
    void *function, void **output, const BYTE *key, const DWORD *state)
{
    typedef void **(*KeyFactoryFn)(void **, const int *, const int *);
    int key32 = (int)*key;
    int state32 = (int)*state;
    return ((KeyFactoryFn)ARM32_THUMB_FUNCTION(function))(output, &key32, &state32);
}

/* facts appmain.game_vf23_setSize: void MinecraftGame::setSize(this (r0), int w (r1), int h (r2),
 * float scale (s0)) (hard-float: the third value is NOT in r3).  x86 `mode` 0 == 0.0f. */
static void call_game_resize(
    void *function, void *game, int width, int height, int mode)
{
    typedef void (*SetSizeFn)(void *, int, int, float);
    ((SetSizeFn)ARM32_THUMB_FUNCTION(function))(game, width, height, (float)mode);
}
#endif /* WIN32CRAFT_ARM32 */
typedef void (STDMETHODCALLTYPE *D3DSetScissorRectsFn)(
    ID3D11DeviceContext *, UINT, const D3D11_RECT *);
typedef void (STDMETHODCALLTYPE *D3DDrawIndexedFn)(
    ID3D11DeviceContext *, UINT, UINT, INT);
typedef void (STDMETHODCALLTYPE *D3DDrawFn)(
    ID3D11DeviceContext *, UINT, UINT);
typedef void (STDMETHODCALLTYPE *D3DSetShaderFn)(
    ID3D11DeviceContext *, void *, void *const *, UINT);
typedef void (STDMETHODCALLTYPE *D3DDrawIndexedInstancedFn)(
    ID3D11DeviceContext *, UINT, UINT, UINT, INT, UINT);
typedef void (STDMETHODCALLTYPE *D3DDrawInstancedFn)(
    ID3D11DeviceContext *, UINT, UINT, UINT, UINT);
typedef void (STDMETHODCALLTYPE *D3DOMSetRenderTargetsFn)(
    ID3D11DeviceContext *, UINT, void *const *, void *);
typedef void (STDMETHODCALLTYPE *D3DClearRenderTargetViewFn)(
    ID3D11DeviceContext *, void *, const float *);
typedef void (STDMETHODCALLTYPE *D3DClearDepthStencilViewFn)(
    ID3D11DeviceContext *, void *, UINT, float, BYTE);
typedef HRESULT (WINAPI *GameActivationFn)(
    LPCWSTR, const GUID *, void **);
typedef void (WINAPI *GameCxxThrowFn)(void *, const void *);
typedef HRESULT (WINAPI *GameD3DCompileFn)(
    LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *,
    LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);
typedef HRESULT (WINAPI *GameD3D11CreateDeviceFn)(
    void *, UINT, HMODULE, UINT, const UINT *, UINT, UINT,
    ID3D11Device **, UINT *, ID3D11DeviceContext **);
typedef void (__cdecl *GamePlatformObjectCtorFn)(void *);
typedef void *(__cdecl *GamePlatformAllocate1Fn)(UINT);
typedef void *(__cdecl *GamePlatformAllocate2Fn)(UINT, UINT);

typedef enum FakeKind {
    FAKE_PACKAGE_FACTORY,
    FAKE_APPDATA_FACTORY,
    FAKE_MEMORY_FACTORY,
    FAKE_CURRENTAPP_FACTORY,
    FAKE_CURRENTAPP_CONSUMABLES,
    FAKE_COREAPP_FACTORY,
    FAKE_COREAPP_EXIT,
    FAKE_PACKAGE,
    FAKE_APPDATA,
    FAKE_APPDATA2,
    FAKE_CURRENTAPP,
    FAKE_LICENSE,
    FAKE_LISTING_INFORMATION,
    FAKE_PRODUCT_LISTINGS,
    FAKE_CONNECTION_PROFILE,
    FAKE_CONNECTION_PROFILE2,
    FAKE_CONNECTION_COST,
    FAKE_NETWORK_NAMES,
    FAKE_NETWORK_NAMES_ITERABLE,
    FAKE_NETWORK_NAMES_ITERATOR,
    FAKE_HOST_NAMES,
    FAKE_HOST_NAMES_ITERABLE,
    FAKE_HOST_NAMES_ITERATOR,
    FAKE_HOST_NAME,
    FAKE_UNFULFILLED_VECTOR,
    FAKE_PACKAGE_FOLDER,
    FAKE_DATA_FOLDER,
    FAKE_NETWORK_FACTORY,
    FAKE_CRYPTO_FACTORY,
    FAKE_CRYPTO_BUFFER,
    FAKE_ASYNC_OPERATION,
    FAKE_ASYNC_STRING,
    FAKE_ASYNC_UNFULFILLED,
    FAKE_API_INFORMATION,
    FAKE_APPLICATION_LANGUAGES,
    FAKE_LANGUAGE_VECTOR,
    FAKE_GEOGRAPHIC_REGION_FACTORY,
    FAKE_GEOGRAPHIC_REGION,
    FAKE_CURRENCY_VECTOR,
    FAKE_XAML_APPLICATION_FACTORY,
    FAKE_XAML_APPLICATION,
    FAKE_XAML_WINDOW_FACTORY,
    FAKE_XAML_WINDOW,
    FAKE_DISPLAY_INFORMATION_FACTORY,
    FAKE_DISPLAY_INFORMATION,
    FAKE_SPEECH_ACTIVATION_FACTORY,
    FAKE_SPEECH_VOICES_FACTORY,
    FAKE_SPEECH_SYNTHESIZER,
    FAKE_SPEECH_CLOSABLE,
    FAKE_SPEECH_SYNTHESIZER2,
    FAKE_SPEECH_OPTIONS,
    FAKE_SPEECH_VOICE,
    FAKE_SPEECH_VOICE_VECTOR,
    FAKE_SPEECH_VOICE_ITERABLE,
    FAKE_SPEECH_VOICE_ITERATOR,
    FAKE_ASYNC_SPEECH,
    FAKE_AUDIO_GRAPH_SETTINGS_FACTORY,
    FAKE_AUDIO_GRAPH_SETTINGS,
    FAKE_AUDIO_GRAPH_FACTORY,
    FAKE_ASYNC_AUDIO_GRAPH,
    FAKE_AUDIO_GRAPH_CREATE_RESULT,
    FAKE_RESOURCE_CONTEXT_ACTIVATION_FACTORY,
    FAKE_RESOURCE_CONTEXT_STATICS,
    FAKE_RESOURCE_CONTEXT_STATICS2,
    FAKE_RESOURCE_CONTEXT_STATICS3,
    FAKE_RESOURCE_CONTEXT,
    FAKE_RESOURCE_QUALIFIER_MAP_OBSERVABLE,
    FAKE_RESOURCE_QUALIFIER_MAP,
    FAKE_RESOURCE_QUALIFIER_MAP_VIEW,
    FAKE_RESOURCE_MANAGER_STATICS,
    FAKE_RESOURCE_MANAGER,
    FAKE_RESOURCE_MAP,
    FAKE_ANALYTICS_INFO_FACTORY,
    FAKE_ANALYTICS_VERSION_INFO,
    FAKE_CORE_TEXT_FACTORY,
    FAKE_CORE_TEXT_MANAGER,
    FAKE_CORE_TEXT_EDIT_CONTEXT,
    FAKE_HARDWARE_ID_FACTORY,
    FAKE_HARDWARE_TOKEN,
    FAKE_DATA_READER_FACTORY,
    FAKE_DATA_READER,
    FAKE_APPLICATION_VIEW_FACTORY,
    FAKE_APPLICATION_VIEW,
    FAKE_APPLICATION_VIEW3,
    FAKE_LAUNCHER_FACTORY,
    FAKE_LAUNCHER_OPTIONS_FACTORY,
    FAKE_LAUNCHER_OPTIONS,
    FAKE_ASYNC_BOOL,
    FAKE_FILE_SAVE_PICKER_FACTORY,
    FAKE_FILE_SAVE_PICKER,
    FAKE_FILE_OPEN_PICKER_FACTORY,
    FAKE_FILE_OPEN_PICKER,
    FAKE_FILE_TYPE_CHOICES,
    FAKE_FILE_TYPE_FILTER,
    FAKE_ASYNC_STORAGE_FILE,
    FAKE_STORAGE_FILE,
    FAKE_STORAGE_ITEM,
    FAKE_CACHED_FILE_MANAGER,
    FAKE_ASYNC_UINT,
    FAKE_ASYNC_INFO
} FakeKind;

typedef struct FakeInspectable {
    const void **vtable;
    LONG references;
    FakeKind kind;
} FakeInspectable;

typedef struct FakeHostName {
    const void **vtable;
    LONG references;
    FakeKind kind;
    wchar_t text[16];
} FakeHostName;

/* AppMainXaml::OnSizeChanged treats its first argument as both an
 * IUIElement and a native object with an optional callback pointer at
 * +0x264.  Keep enough zero-filled storage for both views. */
typedef struct FakeXamlSizeSource {
    const void **vtable;
    LONG references;
    FakeKind kind;
    BYTE reserved_to_264[0x264 - sizeof(FakeInspectable)];
    void *callback_264;
} FakeXamlSizeSource;

typedef struct FakeCryptoBuffer FakeCryptoBuffer;
typedef struct FakeCryptoByteAccess {
    const void **vtable;
    FakeCryptoBuffer *owner;
} FakeCryptoByteAccess;

struct FakeCryptoBuffer {
    const void **vtable;
    LONG references;
    FakeKind kind;
    UINT32 capacity;
    UINT32 length;
    BYTE *bytes;
    FakeCryptoByteAccess byte_access;
};

typedef struct FakeDataReader {
    const void **vtable;
    LONG references;
    FakeKind kind;
    FakeCryptoBuffer *source;
    UINT32 position;
} FakeDataReader;

static GameActivationFn original_activation;
static GameCxxThrowFn original_cxx_throw;
static GameD3DCompileFn original_d3d_compile;
static GameD3D11CreateDeviceFn original_d3d11_create_device;
static GamePlatformObjectCtorFn original_platform_object_ctor;
static GamePlatformAllocate1Fn original_platform_allocate1;
static GamePlatformAllocate2Fn original_platform_allocate2;
/* Windows 7 exposes only the base ID3D11Device/Context interfaces.
 * The original UWP initializer immediately QueryInterfaces the successful
 * D3D11CreateDevice outputs to ID3D11Device2/ID3D11DeviceContext2. Those
 * interfaces begin with Windows 8.1, so both QI calls fail on Windows 7 and
 * the game leaves owner+0x10 and renderer+0x84 null even though creation
 * itself succeeded. Keep one reference to the base interfaces so
 * initialize_game_d3d can transfer them into the renderer as a compatible
 * fallback. All methods used by this 0.13.2 renderer are in the inherited
 * base vtables. */
static ID3D11Device *captured_base_device;
static ID3D11DeviceContext *captured_base_context;
static GameTickFn original_game_tick;
static GameClientUpdateFn original_client_update;
static GameScreenRenderFn original_screen_view_render;
static GameScreenRenderFn original_cubemap_screen_render;
static GameScreenDrawFn original_screen_view_draw;
static GameCallbackFn original_callback_prepare;
static GameCallbackFn original_callback_begin;
static GameClientRenderFn original_callback_render;
static GameCallbackFn original_callback_end;
static GameCallbackFn original_callback_idle;
static GameCallbackFn original_callback_cleanup;
static BOOL host_invoking_game_render;
static GameApplyRenderStateFn original_apply_render_state;
static GameMaterialDrawFn original_material_draw;
typedef void *(__attribute__((thiscall)) *GameScheduleTaskFn)(
    void *, void *, void *, void *, int);
typedef BYTE (__attribute__((thiscall)) *GameTaskInvokeFn)(void *);
static GameScheduleTaskFn original_schedule_task;
static BYTE *game_image;
static D3DSetScissorRectsFn original_set_scissor_rects;
static D3DDrawIndexedFn original_draw_indexed;
static D3DDrawFn original_draw;
static D3DSetShaderFn original_ps_set_shader;
static D3DSetShaderFn original_vs_set_shader;
static D3DDrawIndexedInstancedFn original_draw_indexed_instanced;
static D3DDrawInstancedFn original_draw_instanced;
static D3DOMSetRenderTargetsFn original_om_set_render_targets;
static D3DClearRenderTargetViewFn original_clear_render_target_view;
static D3DClearDepthStencilViewFn original_clear_depth_stencil_view;
static volatile LONG game_draw_indexed_count;
static volatile LONG game_draw_count;
typedef HRESULT (STDMETHODCALLTYPE *SwapChainPresentFn)(IDXGISwapChain *, UINT, UINT);
static SwapChainPresentFn original_swap_chain_present;
static FakeInspectable package_factory;
static FakeInspectable appdata_factory;
static FakeInspectable memory_factory;
static FakeInspectable currentapp_factory;
static FakeInspectable currentapp_consumables;
static FakeInspectable coreapp_factory;
static FakeInspectable coreapp_exit;
static FakeInspectable core_accelerator_keys_116;
static FakeInspectable core_window5_116;
static FakeInspectable core_dispatcher_priority_116;
static FakeInspectable core_window_activated_args_116;
static FakeInspectable core_window_visibility_args_116;
static void *core_window_activated_handler_116;
static void *core_window_visibility_handler_116;
static void *core_window_pointer_moved_handler_116;
static void *core_window_pointer_entered_handler_116;
static void *core_window_pointer_pressed_handler_116;
static void *core_window_pointer_released_handler_116;
static void *core_window_pointer_wheel_handler_116;
static void *core_window_character_handler_116;
static void *core_window_key_down_handler_116;
static void *core_window_key_up_handler_116;
static void *core_window_size_handler_116;
static void *mouse_moved_handlers_116[32];
static unsigned mouse_moved_handler_count_116;
static HRESULT WINAPI core_pointer_add_moved_116(void *, void *, LONGLONG *);
static HRESULT WINAPI core_pointer_add_entered_116(void *, void *, LONGLONG *);
static HRESULT WINAPI core_pointer_add_pressed_116(void *, void *, LONGLONG *);
static HRESULT WINAPI core_pointer_add_released_116(void *, void *, LONGLONG *);
static HRESULT WINAPI core_pointer_add_wheel_116(void *, void *, LONGLONG *);
static HRESULT WINAPI core_window_pointer_cursor_get_116(void *, void **);
static void refresh_scene_mouse_116(void);
static void install_clear_view_116(ID3D11DeviceContext *context);
static HRESULT WINAPI core_character_add_116(void *, void *, LONGLONG *);
static HRESULT WINAPI core_key_down_add_116(void *, void *, LONGLONG *);
static HRESULT WINAPI core_key_up_add_116(void *, void *, LONGLONG *);
static HRESULT WINAPI core_size_add_116(void *, void *, LONGLONG *);
static void __attribute__((thiscall)) win32_set_fullscreen_mode(void *, int);
static FakeInspectable package_object;
static FakeInspectable appdata_object;
static FakeInspectable appdata2_object;
static FakeInspectable currentapp_object;
static FakeInspectable license_object;
static FakeInspectable listing_information;
static FakeInspectable product_listings;
static FakeInspectable connection_profile;
static FakeInspectable connection_profile2;
static FakeInspectable connection_cost;
static FakeInspectable network_names;
static FakeInspectable network_names_iterable;
static FakeInspectable network_names_iterator;
static FakeInspectable host_names;
static FakeInspectable host_names_iterable;
static FakeInspectable host_names_iterator;
static FakeHostName host_name;
static FakeInspectable unfulfilled_vector;
static FakeInspectable package_folder;
static FakeInspectable data_folder;
static FakeInspectable network_factory;
static FakeInspectable gamepad_factory;
static FakeInspectable gamepad_vector;
static FakeInspectable crypto_factory;
static FakeInspectable async_operation;
static FakeInspectable receipt_async_operation;
static FakeInspectable unfulfilled_async_operation;
static FakeInspectable api_information;
static FakeInspectable application_languages;
static FakeInspectable language_vector;
static FakeInspectable geographic_region_factory;
static FakeInspectable geographic_region;
static FakeInspectable currency_vector;
static FakeInspectable xaml_application_factory;
static FakeInspectable xaml_application;
static FakeInspectable xaml_window_factory;
static FakeInspectable xaml_window;
static FakeXamlSizeSource xaml_size_source;
static FakeInspectable display_information_factory;
static FakeInspectable display_information;
static FakeInspectable speech_activation_factory;
static FakeInspectable speech_voices_factory;
static FakeInspectable speech_synthesizer;
static FakeInspectable speech_closable;
static FakeInspectable speech_synthesizer2;
static FakeInspectable speech_options;
static FakeInspectable speech_voice;
static FakeInspectable speech_voice_vector;
static FakeInspectable speech_voice_iterable;
static FakeInspectable speech_voice_iterator;
static FakeInspectable speech_async_operation;
static FakeInspectable audio_graph_settings_factory;
static FakeInspectable audio_graph_settings;
static FakeInspectable audio_graph_factory;
static FakeInspectable audio_graph_async_operation;
static FakeInspectable audio_graph_create_result;
static FakeInspectable resource_context_activation_factory;
static FakeInspectable resource_context_statics;
static FakeInspectable resource_context_statics2;
static FakeInspectable resource_context_statics3;
static FakeInspectable resource_context;
static FakeInspectable resource_qualifier_map_observable;
static FakeInspectable resource_qualifier_map;
static FakeInspectable resource_qualifier_map_view;
static FakeInspectable resource_manager_statics;
static FakeInspectable resource_manager;
static FakeInspectable resource_map;
static FakeInspectable analytics_info_factory;
static FakeInspectable analytics_version_info;
static FakeInspectable core_text_factory;
static FakeInspectable core_text_manager;
static FakeInspectable core_text_edit_context;
static INT core_text_document_length;
static INT core_text_selection_start;
static INT core_text_selection_end;
static FakeInspectable hardware_id_factory;
static FakeInspectable hardware_token;
static FakeInspectable data_reader_factory;
static FakeDataReader data_reader;
static FakeInspectable application_view_factory;
static FakeInspectable application_view;
static FakeInspectable application_view3;
static FakeInspectable launcher_factory;
static FakeInspectable launcher_options_factory;
static FakeInspectable launcher_options;
static FakeInspectable launcher_async_operation;
static FakeInspectable file_save_picker_factory;
static FakeInspectable file_save_picker;
static FakeInspectable file_open_picker_factory;
static FakeInspectable file_open_picker;
static FakeInspectable file_type_choices;
static FakeInspectable file_type_filter;
static FakeInspectable file_save_async_operation;
static FakeInspectable file_open_async_operation;
static FakeInspectable storage_file;
static FakeInspectable storage_item;
/* 0.15.10 keeps the picker-selected file alive while it creates/copies a
 * second local file.  Use a separate object and async operation so the
 * selected source/destination path is never overwritten by that local file. */
static FakeInspectable secondary_storage_file;
static FakeInspectable secondary_storage_item;
static FakeInspectable secondary_storage_async_operation;
static FakeInspectable cached_file_manager;
static FakeInspectable cached_file_async_operation;
/* 0.15.10's C++/CX task wrapper immediately queries every
 * IAsyncOperation<T> for IAsyncInfo and calls get_ErrorCode/Cancel/Close.
 * Keep that projection on a full, separate vtable; returning the generic
 * 9-slot async-operation table made slot 9 a null/out-of-bounds call. */
static FakeInspectable async_info;
static wchar_t export_file_path[MAX_PATH * 4];
static wchar_t secondary_storage_path[MAX_PATH * 4];
static wchar_t export_suggested_name[MAX_PATH];
static wchar_t export_default_extension[32];
static BOOL export_file_selected;
static BOOL secondary_storage_available;
static FakeInspectable core_window_factory;
static FakeInspectable win32_core_window;
static FakeInspectable win32_core_dispatcher;
static FakeInspectable mouse_device_factory;
static FakeInspectable mouse_device;
static FakeInspectable thread_pool_factory;

typedef struct Win32DispatcherItem {
    void *handler;
    BOOL idle;
} Win32DispatcherItem;

#define WIN32_DISPATCHER_QUEUE_CAPACITY 64
static Win32DispatcherItem dispatcher_queue[WIN32_DISPATCHER_QUEUE_CAPACITY];
static unsigned dispatcher_queue_read;
static unsigned dispatcher_queue_write;
static volatile LONG dispatcher_queue_lock;
static volatile LONG dispatcher_run_count;
static const void *crypto_buffer_vtable[9];
static const void *crypto_byte_access_vtable[4];
static BYTE *game_image;
static int fake_fmod_system;
static int fake_fmod_group;
static int fake_fmod_sound;
static int fake_fmod_channel;
static char shader_include_directory[MAX_PATH * 2];

static BOOL patch_hlsl_lod_intrinsic(BYTE *buffer, SIZE_T size)
{
    static const char unsupported[] =
        "tex.CalculateLevelOfDetailUnclamped(TextureSampler0, uv)";
    static const char compatible[] = "(0.0f)";
    SIZE_T offset;
    BOOL replaced = FALSE;

    if (!buffer) return FALSE;
    for (offset = 0;
         offset + sizeof(unsupported) - 1 <= size;
         ++offset) {
        if (memcmp(buffer + offset, unsupported,
                   sizeof(unsupported) - 1) != 0) {
            continue;
        }
        CopyMemory(buffer + offset, compatible, sizeof(compatible) - 1);
        FillMemory(buffer + offset + sizeof(compatible) - 1,
                   sizeof(unsupported) - sizeof(compatible), ' ');
        replaced = TRUE;
    }
    if (replaced) {
        log_line("disabled unsupported HLSL texture LOD probe");
    }
    return replaced;
}

static BOOL patch_hlsl_root_signature(BYTE *buffer, SIZE_T size)
{
    static const char unsupported[] =
        "#define ROOT_SIGNATURE [RootSignature(MinecraftRootSignature)]";
    static const char compatible[] = "#define ROOT_SIGNATURE";
    SIZE_T offset;
    BOOL replaced = FALSE;

    if (!buffer) return FALSE;
    for (offset = 0;
         offset + sizeof(unsupported) - 1 <= size;
         ++offset) {
        if (memcmp(buffer + offset, unsupported,
                   sizeof(unsupported) - 1) != 0) {
            continue;
        }
        CopyMemory(buffer + offset, compatible, sizeof(compatible) - 1);
        FillMemory(buffer + offset + sizeof(compatible) - 1,
                   sizeof(unsupported) - sizeof(compatible), ' ');
        replaced = TRUE;
    }
    if (replaced) {
        log_line("disabled unsupported HLSL root-signature attribute");
    }
    return replaced;
}

static HRESULT STDMETHODCALLTYPE win32_include_open(
    ID3DInclude *self, D3D_INCLUDE_TYPE include_type, const char *filename,
    const void *parent_data, const void **data, UINT *bytes)
{
    char path[MAX_PATH * 2];
    FILE *file;
    long size;
    void *buffer;

    (void)self;
    (void)include_type;
    (void)parent_data;
    if (!filename || !data || !bytes) return E_INVALIDARG;
    if (shader_include_directory[0]) {
        lstrcpynA(path, shader_include_directory, sizeof(path));
        lstrcatA(path, filename);
    } else {
        lstrcpynA(path, filename, sizeof(path));
    }
    file = fopen(path, "rb");
    if (!file) return E_FAIL;
    if (fseek(file, 0, SEEK_END) != 0 ||
        (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return E_FAIL;
    }
    buffer = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size + 1);
    if (!buffer) {
        fclose(file);
        return E_OUTOFMEMORY;
    }
    if (size && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        HeapFree(GetProcessHeap(), 0, buffer);
        fclose(file);
        return E_FAIL;
    }
    fclose(file);
    ((BYTE *)buffer)[size] = 0;

    patch_hlsl_lod_intrinsic((BYTE *)buffer, (SIZE_T)size);
    patch_hlsl_root_signature((BYTE *)buffer, (SIZE_T)size);
    *data = buffer;
    *bytes = (UINT)size;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE win32_include_close(
    ID3DInclude *self, const void *data)
{
    (void)self;
    if (data) HeapFree(GetProcessHeap(), 0, (void *)data);
    return S_OK;
}

static ID3DIncludeVtbl win32_include_vtable = {
    win32_include_open,
    win32_include_close
};
static ID3DInclude win32_include = { &win32_include_vtable };

typedef struct Win32IncludeAllocation Win32IncludeAllocation;

typedef struct Win32IncludeProxy {
    ID3DInclude interface;
    ID3DInclude *original;
    Win32IncludeAllocation *allocations;
} Win32IncludeProxy;

struct Win32IncludeAllocation {
    Win32IncludeAllocation *next;
    ID3DInclude *original;
    const void *original_data;
};

static HRESULT STDMETHODCALLTYPE win32_include_proxy_open(
    ID3DInclude *self, D3D_INCLUDE_TYPE include_type, const char *filename,
    const void *parent_data, const void **data, UINT *bytes)
{
    Win32IncludeProxy *proxy = (Win32IncludeProxy *)self;
    const void *original_parent = parent_data;
    const void *original_data = NULL;
    Win32IncludeAllocation *allocation;
    Win32IncludeAllocation *parent;
    HRESULT result;

    if (!proxy || !proxy->original || !data || !bytes) return E_INVALIDARG;
    for (parent = proxy->allocations; parent; parent = parent->next) {
        if ((const void *)(parent + 1) == parent_data) {
            original_parent = parent->original_data;
            break;
        }
    }
    result = proxy->original->lpVtbl->Open(
        proxy->original, include_type, filename, original_parent,
        &original_data, bytes);
    if (FAILED(result)) return result;
    allocation = (Win32IncludeAllocation *)HeapAlloc(
        GetProcessHeap(), 0, sizeof(*allocation) + (SIZE_T)*bytes + 1);
    if (!allocation) {
        proxy->original->lpVtbl->Close(proxy->original, original_data);
        return E_OUTOFMEMORY;
    }
    allocation->next = proxy->allocations;
    allocation->original = proxy->original;
    allocation->original_data = original_data;
    proxy->allocations = allocation;
    if (*bytes) CopyMemory(allocation + 1, original_data, *bytes);
    ((BYTE *)(allocation + 1))[*bytes] = 0;
    patch_hlsl_lod_intrinsic((BYTE *)(allocation + 1), *bytes);
    patch_hlsl_root_signature((BYTE *)(allocation + 1), *bytes);
    *data = allocation + 1;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE win32_include_proxy_close(
    ID3DInclude *self, const void *data)
{
    Win32IncludeProxy *proxy = (Win32IncludeProxy *)self;
    Win32IncludeAllocation *allocation;
    Win32IncludeAllocation **link;
    HRESULT result = S_OK;
    if (!data) return S_OK;
    if (!proxy) return E_INVALIDARG;
    for (link = &proxy->allocations; *link; link = &(*link)->next) {
        if ((const void *)((*link) + 1) == data) break;
    }
    if (!*link) return E_FAIL;
    allocation = *link;
    *link = allocation->next;
    if (allocation->original && allocation->original->lpVtbl->Close) {
        result = allocation->original->lpVtbl->Close(
            allocation->original, allocation->original_data);
    }
    HeapFree(GetProcessHeap(), 0, allocation);
    return result;
}

static ID3DIncludeVtbl win32_include_proxy_vtable = {
    win32_include_proxy_open,
    win32_include_proxy_close
};

static void log_line(const char *text);
#ifdef WIN32CRAFT_ARM32
static void arm32_gfx_get_backbuffer_size(UINT *width, UINT *height);   /* gfx_bootstrap helper, defined further down */
#endif

static void STDMETHODCALLTYPE win32_set_scissor_rects(
    ID3D11DeviceContext *context, UINT count, const D3D11_RECT *rects)
{
    D3D11_RECT adjusted[16];
    const D3D11_RECT *submitted = rects;
    static unsigned logged_corrections;
    BOOL corrected = FALSE;
    RECT client;
    UINT index;

    /*
     * The WinRT renderer's Y conversion retains one UWP CoreWindow-height
     * offset.  Its scroll panes consequently submit e.g. -599..-47 for a
     * 688-pixel HWND, clipping every Options row.  Translate that wholly
     * negative interval back into HWND client coordinates and clamp the
     * oversized UWP bounds to the real swap-chain extent.
     */
    if (rects && count && count <= ARRAYSIZE(adjusted) && game_window &&
        GetClientRect(game_window, &client)) {
        CopyMemory(adjusted, rects, count * sizeof(adjusted[0]));
#ifdef WIN32CRAFT_ARM32
        {
            /* the game renders into the (feature-level clamped) swap-chain buffer, which is smaller than the client area
             * when the window exceeds the texture limit: clamp to the buffer, not to the window */
            UINT buffer_width = 0;
            UINT buffer_height = 0;
            arm32_gfx_get_backbuffer_size(&buffer_width, &buffer_height);
            if (buffer_width && buffer_height) {
                client.right = (LONG)buffer_width;
                client.bottom = (LONG)buffer_height;
            }
        }
#endif
        for (index = 0; index < count; ++index) {
#ifdef WIN32CRAFT_ARM32
            if (logged_corrections < 8 &&
                ((adjusted[index].top < 0 && adjusted[index].bottom <= 0) || adjusted[index].left < 0 ||
                 adjusted[index].top < 0 || adjusted[index].right > client.right || adjusted[index].bottom > client.bottom)) {
                /* device evidence for facts audit "scissor offset on ARM": the ORIGINAL rectangle the game submitted */
                char scissor_line[200];
                wsprintfA(scissor_line, "ARM32 scissor: game submitted rect %u/%u = L%ld T%ld R%ld B%ld, limit (buffer) %ldx%ld%s",
                          index, count, adjusted[index].left, adjusted[index].top, adjusted[index].right, adjusted[index].bottom,
                          client.right, client.bottom,
                          (adjusted[index].top < 0 && adjusted[index].bottom <= 0) ? " -> wholly-negative Y: translated by +limit.bottom (x86 rule)" : "");
                log_line(scissor_line);
            }
#endif
            if (adjusted[index].top < 0 && adjusted[index].bottom <= 0) {
                adjusted[index].top += client.bottom;
                adjusted[index].bottom += client.bottom;
                corrected = TRUE;
            }
            if (adjusted[index].left < 0) {
                adjusted[index].left = 0;
                corrected = TRUE;
            }
            if (adjusted[index].top < 0) {
                adjusted[index].top = 0;
                corrected = TRUE;
            }
            if (adjusted[index].right > client.right) {
                adjusted[index].right = client.right;
                corrected = TRUE;
            }
            if (adjusted[index].bottom > client.bottom) {
                adjusted[index].bottom = client.bottom;
                corrected = TRUE;
            }
        }
        submitted = adjusted;
    }
    if (corrected && logged_corrections < 8) {
        log_line("corrected WinRT scissor rectangle for HWND");
        ++logged_corrections;
    }
    original_set_scissor_rects(context, count, submitted);
}

static BOOL install_scissor_fix(ID3D11DeviceContext *context)
{
    D3DSetScissorRectsFn *slot;
    DWORD old_protection;
    DWORD ignored;

    if (!context) return FALSE;
    slot = (D3DSetScissorRectsFn *)&context->lpVtbl[45];
#ifdef WIN32CRAFT_ARM32
    /* facts gfx_compat.scissor_hook: vtable slot 45 and 4-byte slots are the same on ARM32, hook kept as is; only add
     * logging and never chain the hook to itself when the context is hooked twice. */
    if (IsBadReadPtr(slot, sizeof(*slot))) {
        log_line("ARM32 scissor fix: RSSetScissorRects vtable slot is unreadable");
        return FALSE;
    }
    log_pointer("ARM32 scissor fix: RSSetScissorRects vtable slot", (const void *)slot);
    log_pointer("ARM32 scissor fix: RSSetScissorRects original", (const void *)*slot);
    if ((const void *)*slot == (const void *)win32_set_scissor_rects) {
        log_line("ARM32 scissor fix: already installed");
        return TRUE;
    }
#endif
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE,
                        &old_protection)) {
        return FALSE;
    }
    original_set_scissor_rects = *slot;
    *slot = win32_set_scissor_rects;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    log_line("installed D3D11 HWND scissor fix");
    return TRUE;
}

static void STDMETHODCALLTYPE win32_draw_indexed(
    ID3D11DeviceContext *context, UINT index_count,
    UINT start_index, INT base_vertex)
{
    LONG count = InterlockedIncrement((LONG *)&game_draw_indexed_count);
    if (count <= 10 || count == 60 || count == 600) {
        char line[128];
        wsprintfA(line,
            "D3D11 DrawIndexed %ld indices=%u start=%u base=%d",
            count, index_count, start_index, base_vertex);
        log_line(line);
    }
    original_draw_indexed(
        context, index_count, start_index, base_vertex);
}

static void STDMETHODCALLTYPE win32_draw(
    ID3D11DeviceContext *context, UINT vertex_count, UINT start_vertex)
{
    LONG count = InterlockedIncrement((LONG *)&game_draw_count);
    if (count <= 10 || count == 60 || count == 600) {
        char line[112];
        wsprintfA(line, "D3D11 Draw %ld vertices=%u start=%u",
                  count, vertex_count, start_vertex);
        log_line(line);
    }
    original_draw(context, vertex_count, start_vertex);
}

static void log_d3d_stage(const char *name, LONG count, const void *object)
{
    char line[128];
    if (count <= 10 || count == 60 || count == 600) {
        wsprintfA(line, "D3D11 %s %ld object=%p", name, count, object);
        log_line(line);
    }
}

static void STDMETHODCALLTYPE win32_ps_set_shader(
    ID3D11DeviceContext *context, void *shader,
    void *const *classes, UINT class_count)
{
    static LONG count;
    log_d3d_stage("PSSetShader", ++count, shader);
    original_ps_set_shader(context, shader, classes, class_count);
}

static void STDMETHODCALLTYPE win32_vs_set_shader(
    ID3D11DeviceContext *context, void *shader,
    void *const *classes, UINT class_count)
{
    static LONG count;
    log_d3d_stage("VSSetShader", ++count, shader);
    original_vs_set_shader(context, shader, classes, class_count);
}

static void STDMETHODCALLTYPE win32_draw_indexed_instanced(
    ID3D11DeviceContext *context, UINT indices_per_instance,
    UINT instance_count, UINT start_index, INT base_vertex,
    UINT start_instance)
{
    static LONG count;
    log_d3d_stage("DrawIndexedInstanced", ++count, NULL);
    original_draw_indexed_instanced(
        context, indices_per_instance, instance_count, start_index,
        base_vertex, start_instance);
}

static void STDMETHODCALLTYPE win32_draw_instanced(
    ID3D11DeviceContext *context, UINT vertices_per_instance,
    UINT instance_count, UINT start_vertex, UINT start_instance)
{
    static LONG count;
    log_d3d_stage("DrawInstanced", ++count, NULL);
    original_draw_instanced(
        context, vertices_per_instance, instance_count,
        start_vertex, start_instance);
}

static void STDMETHODCALLTYPE win32_om_set_render_targets(
    ID3D11DeviceContext *context, UINT count, void *const *targets,
    void *depth)
{
    static LONG call_count;
    log_d3d_stage("OMSetRenderTargets", ++call_count,
                  count && targets ? targets[0] : NULL);
    original_om_set_render_targets(context, count, targets, depth);
}

static void STDMETHODCALLTYPE win32_clear_render_target_view(
    ID3D11DeviceContext *context, void *target, const float *color)
{
    static LONG count;
    log_d3d_stage("ClearRenderTargetView", ++count, target);
    original_clear_render_target_view(context, target, color);
}

static void STDMETHODCALLTYPE win32_clear_depth_stencil_view(
    ID3D11DeviceContext *context, void *target, UINT flags,
    float depth, BYTE stencil)
{
    static LONG count;
    log_d3d_stage("ClearDepthStencilView", ++count, target);
    original_clear_depth_stencil_view(
        context, target, flags, depth, stencil);
}

static BOOL install_draw_trace(ID3D11DeviceContext *context)
{
    D3DDrawIndexedFn *indexed_slot;
    D3DDrawFn *draw_slot;
    void **vtable;
    DWORD old_protection;
    DWORD ignored;

    if (!context) return FALSE;
    vtable = (void **)context->lpVtbl;
    indexed_slot = (D3DDrawIndexedFn *)&vtable[12];
    draw_slot = (D3DDrawFn *)&vtable[13];
    if (!VirtualProtect(vtable + 9, (53 - 9 + 1) * sizeof(*vtable),
                        PAGE_READWRITE, &old_protection)) {
        log_line("D3D11 draw trace vtable protection failed");
        return FALSE;
    }
    original_draw_indexed = *indexed_slot;
    original_draw = *draw_slot;
    original_ps_set_shader = (D3DSetShaderFn)vtable[9];
    original_vs_set_shader = (D3DSetShaderFn)vtable[11];
    original_draw_indexed_instanced =
        (D3DDrawIndexedInstancedFn)vtable[20];
    original_draw_instanced = (D3DDrawInstancedFn)vtable[21];
    original_om_set_render_targets =
        (D3DOMSetRenderTargetsFn)vtable[33];
    original_clear_render_target_view =
        (D3DClearRenderTargetViewFn)vtable[50];
    original_clear_depth_stencil_view =
        (D3DClearDepthStencilViewFn)vtable[53];
    vtable[9] = win32_ps_set_shader;
    vtable[11] = win32_vs_set_shader;
    *indexed_slot = win32_draw_indexed;
    *draw_slot = win32_draw;
    vtable[20] = win32_draw_indexed_instanced;
    vtable[21] = win32_draw_instanced;
    vtable[33] = win32_om_set_render_targets;
    vtable[50] = win32_clear_render_target_view;
    vtable[53] = win32_clear_depth_stencil_view;
    VirtualProtect(vtable + 9, (53 - 9 + 1) * sizeof(*vtable),
                   old_protection, &ignored);
    FlushInstructionCache(
        GetCurrentProcess(), vtable + 9,
        (53 - 9 + 1) * sizeof(*vtable));
    log_line("installed D3D11 pipeline/draw trace");
    return TRUE;
}

static int WINAPI fake_fmod_system_create(void **result)
{
    log_line("FMOD shim: System_Create");
    if (result) *result = &fake_fmod_system;
    return 0;
}

static int WINAPI fake_fmod_get_version(
    void *self, unsigned *version)
{
    (void)self;
    log_line("FMOD shim: getVersion");
    if (version) *version = 0xffffffffU;
    return 0;
}

static int WINAPI fake_fmod_init(
    void *self, int channels, unsigned flags, void *extra)
{
    (void)self; (void)channels; (void)flags; (void)extra;
    log_line("FMOD shim: init");
    return 0;
}

static int WINAPI fake_fmod_three_floats(
    void *self, float a, float b, float c)
{
    (void)self; (void)a; (void)b; (void)c;
    log_line("FMOD shim: set3DSettings");
    return 0;
}

static int WINAPI fake_fmod_create_group(
    void *self, const char *name, void **result)
{
    (void)self; (void)name;
    log_line("FMOD shim: createChannelGroup");
    if (result) *result = &fake_fmod_group;
    return 0;
}

static int WINAPI fake_fmod_get_group(
    void *self, void **result)
{
    (void)self;
    log_line("FMOD shim: getMasterChannelGroup");
    if (result) *result = &fake_fmod_group;
    return 0;
}

static int WINAPI fake_fmod_add_group(
    void *self, void *group, int propagate, void **connection)
{
    (void)self; (void)group; (void)propagate;
    if (connection) *connection = NULL;
    return 0;
}

static int WINAPI fake_fmod_no_args(void *self)
{
    (void)self;
    return 0;
}

static int WINAPI fake_fmod_bool(
    void *self, int value)
{
    (void)self; (void)value;
    return 0;
}

static int WINAPI fake_fmod_float(
    void *self, float value)
{
    (void)self; (void)value;
    return 0;
}

static int WINAPI fake_fmod_create_sound(
    void *self, const char *name, unsigned mode, void *info, void **result)
{
    (void)self; (void)name; (void)mode; (void)info;
    if (result) *result = &fake_fmod_sound;
    return 0;
}

static int WINAPI fake_fmod_two_floats(
    void *self, float a, float b)
{
    (void)self; (void)a; (void)b;
    return 0;
}

static int WINAPI fake_fmod_get_count(
    void *self, int *result)
{
    (void)self;
    if (result) *result = 0;
    return 0;
}

static int WINAPI fake_fmod_get_subsound(
    void *self, int index, void **result)
{
    (void)self; (void)index;
    if (result) *result = &fake_fmod_sound;
    return 0;
}

static int WINAPI fake_fmod_play_sound(
    void *self, void *sound, void *group, int paused, void **result)
{
    (void)self; (void)sound; (void)group; (void)paused;
    if (result) *result = &fake_fmod_channel;
    return 0;
}

static int WINAPI fake_fmod_vectors(
    void *self, const void *a, const void *b, const void *c)
{
    (void)self; (void)a; (void)b; (void)c;
    return 0;
}

static int WINAPI fake_fmod_is_playing(
    void *self, BYTE *result)
{
    (void)self;
    if (result) *result = FALSE;
    return 0;
}

static int WINAPI fake_fmod_listener(
    void *self, int listener, const void *position, const void *velocity,
    const void *forward, const void *up)
{
    (void)self; (void)listener; (void)position; (void)velocity;
    (void)forward; (void)up;
    return 0;
}


/*
 * Native Win32 FMOD bridge.
 *
 * The game delay-imports the Store build name fmod_WSA82_X86.dll.  Patching
 * the 25 delay thunks to these wrappers bypasses the UWP loader and forwards
 * them to the supplied desktop x86 build renamed to fmod.dll.
 * The bridge logs all initialization and sound-loading failures.  It defaults
 * to the native desktop DLL.  the stable baseline patches the delay-IAT slots themselves,
 * avoiding rel32 range/layout dependence.  WASAPI is the default because a
 * Wine WINMM device can initialize successfully yet remain inaudible.
 * fmod_output.txt accepts "auto", "wasapi", "dsound", or "winmm".
 */
typedef int (WINAPI *FmodSystemCreateFn)(void **);
typedef int (WINAPI *FmodGetVersionFn)(void *, unsigned *);
typedef int (WINAPI *FmodInitFn)(void *, int, unsigned, void *);
typedef int (WINAPI *FmodThreeFloatsFn)(void *, float, float, float);
typedef int (WINAPI *FmodCreateGroupFn)(void *, const char *, void **);
typedef int (WINAPI *FmodGetGroupFn)(void *, void **);
typedef int (WINAPI *FmodAddGroupFn)(void *, void *, int, void **);
typedef int (WINAPI *FmodNoArgsFn)(void *);
typedef int (WINAPI *FmodBoolFn)(void *, int);
typedef int (WINAPI *FmodFloatFn)(void *, float);
typedef int (WINAPI *FmodCreateSoundFn)(void *, const char *, unsigned, void *, void **);
typedef int (WINAPI *FmodTwoFloatsFn)(void *, float, float);
typedef int (WINAPI *FmodGetCountFn)(void *, int *);
typedef int (WINAPI *FmodGetSubSoundFn)(void *, int, void **);
typedef int (WINAPI *FmodPlaySoundFn)(void *, void *, void *, int, void **);
typedef int (WINAPI *FmodVectorsFn)(void *, const void *, const void *, const void *);
typedef int (WINAPI *FmodIsPlayingFn)(void *, BYTE *);
typedef int (WINAPI *FmodListenerFn)(void *, int, const void *, const void *, const void *, const void *);
typedef int (WINAPI *FmodSetOutputFn)(void *, int);
typedef int (WINAPI *FmodGetOutputFn)(void *, int *);
typedef int (WINAPI *FmodSetDriverFn)(void *, int);
typedef int (WINAPI *FmodGetDriverFn)(void *, int *);
typedef const char *(WINAPI *FmodErrorStringFn)(int);
typedef int (WINAPI *FmodGetNumDriversFn)(void *, int *);
typedef int (WINAPI *FmodGetDriverInfoFn)(void *, int, char *, int, void *, int *, int *, int *);

static HMODULE fmod_real_module;
static BOOL fmod_real_attempted;
static BOOL fmod_real_ready;
static int fmod_output_preference = 8; /* FMOD_OUTPUTTYPE_WASAPI */
static BOOL fmod_output_applied;
static unsigned fmod_create_log_count;
static unsigned fmod_play_log_count;
static unsigned fmod_update_counter;
static void *fmod_master_group;
static void *fmod_sound_group;
static void *fmod_music_group;

static FmodSystemCreateFn real_fmod_system_create;
static FmodGetVersionFn real_fmod_get_version;
static FmodInitFn real_fmod_init;
static FmodThreeFloatsFn real_fmod_set_3d_settings;
static FmodCreateGroupFn real_fmod_create_group;
static FmodGetGroupFn real_fmod_get_group;
static FmodAddGroupFn real_fmod_add_group;
static FmodNoArgsFn real_fmod_sound_release;
static FmodNoArgsFn real_fmod_system_close;
static FmodNoArgsFn real_fmod_system_release;
static FmodBoolFn real_fmod_set_mute;
static FmodFloatFn real_fmod_set_volume;
static FmodCreateSoundFn real_fmod_create_stream;
static FmodCreateSoundFn real_fmod_create_sound;
static FmodTwoFloatsFn real_fmod_set_min_max;
static FmodGetCountFn real_fmod_get_sub_count;
static FmodGetSubSoundFn real_fmod_get_sub_sound;
static FmodPlaySoundFn real_fmod_play_sound;
static FmodVectorsFn real_fmod_set_3d_attributes;
static FmodFloatFn real_fmod_set_pitch;
static FmodBoolFn real_fmod_set_paused;
static FmodIsPlayingFn real_fmod_is_playing;
static FmodNoArgsFn real_fmod_stop;
static FmodNoArgsFn real_fmod_update;
static FmodListenerFn real_fmod_set_listener;
static FmodSetOutputFn real_fmod_set_output;
static FmodGetOutputFn real_fmod_get_output;
static FmodSetDriverFn real_fmod_set_driver;
static FmodGetDriverFn real_fmod_get_driver;
static FmodErrorStringFn real_fmod_error_string;
static FmodGetNumDriversFn real_fmod_get_num_drivers;
static FmodGetDriverInfoFn real_fmod_get_driver_info;
static FmodNoArgsFn real_fmod_mixer_resume;
static FmodNoArgsFn real_fmod_mixer_suspend;

static BOOL ascii_prefix(const char *text, const char *prefix)
{
    while (*prefix) {
        char a = *text++;
        char b = *prefix++;
        if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
        if (a != b) return FALSE;
    }
    return TRUE;
}

static int read_fmod_output_preference(void)
{
    FILE *file = fopen("fmod_output.txt", "rb");
    char value[32];
    size_t count;
    if (!file) return 8;
    count = fread(value, 1, sizeof(value) - 1, file);
    fclose(file);
    value[count] = 0;
    if (ascii_prefix(value, "auto")) return 0;
    if (ascii_prefix(value, "wasapi")) return 8;
    if (ascii_prefix(value, "dsound")) return 6;
    if (ascii_prefix(value, "winmm")) return 7;
    return 8;
}

static const char *fmod_error_text(int result)
{
    const char *text = NULL;
    if (real_fmod_error_string) text = real_fmod_error_string(result);
    return text ? text : "unknown";
}

static void log_fmod_call(const char *name, int result)
{
    char line[512];
    wsprintfA(line, "FMOD bridge: %s -> %d (%s)",
              name, result, fmod_error_text(result));
    log_line(line);
}

static void *resolve_fmod(const char *name)
{
    void *result = GetProcAddress(fmod_real_module, name);
    if (!result) {
        char line[512];
        wsprintfA(line, "FMOD bridge: missing export %s", name);
        log_line(line);
    }
    return result;
}

static BOOL load_real_fmod(void)
{
    if (fmod_real_attempted) return fmod_real_ready;
    fmod_real_attempted = TRUE;
    fmod_output_preference = read_fmod_output_preference();
    fmod_real_module = LoadLibraryW(L"fmod.dll");
    if (!fmod_real_module) {
        char line[256];
        wsprintfA(line, "FMOD bridge: LoadLibraryW(fmod.dll) failed, error=%u",
                  (unsigned)GetLastError());
        log_line(line);
        return FALSE;
    }

#define RESOLVE_REQUIRED(field, type, symbol) \
    field = (type)resolve_fmod(symbol); if (!field) return FALSE
    RESOLVE_REQUIRED(real_fmod_system_create, FmodSystemCreateFn, "FMOD_System_Create");
    RESOLVE_REQUIRED(real_fmod_get_version, FmodGetVersionFn, "?getVersion@System@FMOD@@QAG?AW4FMOD_RESULT@@PAI@Z");
    RESOLVE_REQUIRED(real_fmod_init, FmodInitFn, "?init@System@FMOD@@QAG?AW4FMOD_RESULT@@HIPAX@Z");
    RESOLVE_REQUIRED(real_fmod_set_3d_settings, FmodThreeFloatsFn, "?set3DSettings@System@FMOD@@QAG?AW4FMOD_RESULT@@MMM@Z");
    RESOLVE_REQUIRED(real_fmod_create_group, FmodCreateGroupFn, "?createChannelGroup@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDPAPAVChannelGroup@2@@Z");
    RESOLVE_REQUIRED(real_fmod_get_group, FmodGetGroupFn, "?getMasterChannelGroup@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVChannelGroup@2@@Z");
    RESOLVE_REQUIRED(real_fmod_add_group, FmodAddGroupFn, "?addGroup@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@PAV12@_NPAPAVDSPConnection@2@@Z");
    RESOLVE_REQUIRED(real_fmod_sound_release, FmodNoArgsFn, "?release@Sound@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
    RESOLVE_REQUIRED(real_fmod_system_close, FmodNoArgsFn, "?close@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
    RESOLVE_REQUIRED(real_fmod_system_release, FmodNoArgsFn, "?release@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
    RESOLVE_REQUIRED(real_fmod_mixer_resume, FmodNoArgsFn, "?mixerResume@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
    RESOLVE_REQUIRED(real_fmod_mixer_suspend, FmodNoArgsFn, "?mixerSuspend@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
    RESOLVE_REQUIRED(real_fmod_set_mute, FmodBoolFn, "?setMute@ChannelControl@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
    RESOLVE_REQUIRED(real_fmod_set_volume, FmodFloatFn, "?setVolume@ChannelControl@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
    RESOLVE_REQUIRED(real_fmod_create_stream, FmodCreateSoundFn, "?createStream@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDIPAUFMOD_CREATESOUNDEXINFO@@PAPAVSound@2@@Z");
    RESOLVE_REQUIRED(real_fmod_create_sound, FmodCreateSoundFn, "?createSound@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDIPAUFMOD_CREATESOUNDEXINFO@@PAPAVSound@2@@Z");
    RESOLVE_REQUIRED(real_fmod_set_min_max, FmodTwoFloatsFn, "?set3DMinMaxDistance@Sound@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z");
    RESOLVE_REQUIRED(real_fmod_get_sub_count, FmodGetCountFn, "?getNumSubSounds@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
    RESOLVE_REQUIRED(real_fmod_get_sub_sound, FmodGetSubSoundFn, "?getSubSound@Sound@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAV12@@Z");
    RESOLVE_REQUIRED(real_fmod_play_sound, FmodPlaySoundFn, "?playSound@System@FMOD@@QAG?AW4FMOD_RESULT@@PAVSound@2@PAVChannelGroup@2@_NPAPAVChannel@2@@Z");
    RESOLVE_REQUIRED(real_fmod_set_3d_attributes, FmodVectorsFn, "?set3DAttributes@ChannelControl@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@00@Z");
    RESOLVE_REQUIRED(real_fmod_set_pitch, FmodFloatFn, "?setPitch@ChannelControl@FMOD@@QAG?AW4FMOD_RESULT@@M@Z");
    RESOLVE_REQUIRED(real_fmod_set_paused, FmodBoolFn, "?setPaused@ChannelControl@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z");
    RESOLVE_REQUIRED(real_fmod_is_playing, FmodIsPlayingFn, "?isPlaying@ChannelControl@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z");
    RESOLVE_REQUIRED(real_fmod_stop, FmodNoArgsFn, "?stop@ChannelControl@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
    RESOLVE_REQUIRED(real_fmod_update, FmodNoArgsFn, "?update@System@FMOD@@QAG?AW4FMOD_RESULT@@XZ");
    RESOLVE_REQUIRED(real_fmod_set_listener, FmodListenerFn, "?set3DListenerAttributes@System@FMOD@@QAG?AW4FMOD_RESULT@@HPBUFMOD_VECTOR@@000@Z");
#undef RESOLVE_REQUIRED

    real_fmod_set_output = (FmodSetOutputFn)resolve_fmod("?setOutput@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_OUTPUTTYPE@@@Z");
    real_fmod_get_output = (FmodGetOutputFn)resolve_fmod("?getOutput@System@FMOD@@QAG?AW4FMOD_RESULT@@PAW4FMOD_OUTPUTTYPE@@@Z");
    real_fmod_set_driver = (FmodSetDriverFn)resolve_fmod("?setDriver@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z");
    real_fmod_get_driver = (FmodGetDriverFn)resolve_fmod("?getDriver@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
    real_fmod_error_string = (FmodErrorStringFn)resolve_fmod("FMOD_ErrorString");
    real_fmod_get_num_drivers = (FmodGetNumDriversFn)resolve_fmod("?getNumDrivers@System@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z");
    real_fmod_get_driver_info = (FmodGetDriverInfoFn)resolve_fmod("?getDriverInfo@System@FMOD@@QAG?AW4FMOD_RESULT@@HPADHPAUFMOD_GUID@@PAHPAW4FMOD_SPEAKERMODE@@2@Z");
    fmod_real_ready = TRUE;
    {
        char line[128];
        wsprintfA(line, "FMOD bridge: desktop DLL loaded, requested output=%d", fmod_output_preference);
        log_line(line);
    }
    return TRUE;
}

static const char *normalize_fmod_path(const char *input, char *output, int output_size)
{
    const char *source = input;
    int index = 0;
    if (!input || !output || output_size < 2) return input;
    if (ascii_prefix(source, "ms-appx:///")) source += 11;
    else if (ascii_prefix(source, "file:///")) source += 8;
    while (*source && index < output_size - 1) {
        char value = *source++;
        if (value == '/') value = '\\';
        output[index++] = value;
    }
    output[index] = 0;
    return output;
}

static void log_fmod_sound_result(const char *kind, const char *path,
                                  unsigned mode, int result, void *sound)
{
    char line[768];
    char clipped[420];
    lstrcpynA(clipped, path ? path : "(null)", sizeof(clipped));
    wsprintfA(line, "FMOD bridge: %s result=%d mode=0x%x sound=%p path=%s",
              kind, result, mode, sound, clipped);
    log_line(line);
}

static void log_fmod_driver_state(void *system)
{
    int output = -1;
    int count = -1;
    int result;
    char line[512];
    if (real_fmod_get_output) {
        result = real_fmod_get_output(system, &output);
        wsprintfA(line, "FMOD bridge: getOutput result=%d output=%d", result, output);
        log_line(line);
    }
    if (real_fmod_get_num_drivers) {
        result = real_fmod_get_num_drivers(system, &count);
        wsprintfA(line, "FMOD bridge: getNumDrivers result=%d count=%d", result, count);
        log_line(line);
        if (!result && count > 0 && real_fmod_get_driver_info) {
            int index;
            if (count > 8) count = 8;
            for (index = 0; index < count; ++index) {
                char name[256];
                BYTE guid[16];
                int rate = 0, speaker = 0, channels = 0;
                ZeroMemory(name, sizeof(name));
                ZeroMemory(guid, sizeof(guid));
                result = real_fmod_get_driver_info(system, index, name,
                    sizeof(name), guid, &rate, &speaker, &channels);
                wsprintfA(line,
                    "FMOD bridge: driver[%d] result=%d name=%s rate=%d speaker=%d channels=%d",
                    index, result, name, rate, speaker, channels);
                log_line(line);
            }
        }
    }
}

static int WINAPI bridge_fmod_system_create(void **result)
{
    int code;
    if (!load_real_fmod()) return fake_fmod_system_create(result);
    code = real_fmod_system_create(result);
    {
        char line[192];
        wsprintfA(line, "FMOD bridge: System_Create result=%d system=%p",
                  code, result ? *result : NULL);
        log_line(line);
    }
    if (code || !result || !*result) {
        log_line("FMOD bridge: System_Create failed; using silent fallback");
        return fake_fmod_system_create(result);
    }
    return code;
}

static int WINAPI bridge_fmod_get_num_drivers(void *self, int *count)
{
    if (self == &fake_fmod_system || !load_real_fmod()) {
        if (count) *count = 0;
        return 0;
    }
    return real_fmod_get_num_drivers(self, count);
}

static int WINAPI bridge_fmod_get_driver_info(
    void *self, int index, char *name, int name_size, void *guid,
    int *rate, int *speaker_mode, int *speaker_channels)
{
    if (self == &fake_fmod_system || !load_real_fmod()) {
        if (name && name_size > 0) name[0] = 0;
        if (guid) ZeroMemory(guid, 16);
        if (rate) *rate = 0;
        if (speaker_mode) *speaker_mode = 0;
        if (speaker_channels) *speaker_channels = 0;
        return 0;
    }
    return real_fmod_get_driver_info(
        self, index, name, name_size, guid, rate, speaker_mode,
        speaker_channels);
}

static int WINAPI bridge_fmod_set_driver(void *self, int driver)
{
    if (self == &fake_fmod_system || !load_real_fmod()) return 0;
    return real_fmod_set_driver(self, driver);
}

static int WINAPI bridge_fmod_set_output(void *self, int output)
{
    if (self == &fake_fmod_system || !load_real_fmod()) return 0;
    return real_fmod_set_output(self, output);
}

static int WINAPI bridge_fmod_get_version(void *self, unsigned *version)
{
    int code;
    if (self == &fake_fmod_system || !load_real_fmod()) return fake_fmod_get_version(self, version);
    code = real_fmod_get_version(self, version);
    {
        char line[192];
        wsprintfA(line, "FMOD bridge: getVersion result=%d version=0x%x",
                  code, version ? *version : 0);
        log_line(line);
    }
    return code;
}

static const char *fmod_output_name(int output)
{
    if (output == 0) return "AUTODETECT";
    if (output == 6) return "DSOUND";
    if (output == 7) return "WINMM";
    if (output == 8) return "WASAPI";
    return "OTHER";
}

static int try_fmod_init_output(void *self, int output, int channels,
                                unsigned flags, void *extra)
{
    int code;
    int drivers = -1;
    char label[96];
    if (output && real_fmod_set_output) {
        code = real_fmod_set_output(self, output);
        wsprintfA(label, "setOutput(%s)", fmod_output_name(output));
        log_fmod_call(label, code);
        if (code) return code;
    }
    if (real_fmod_get_num_drivers) {
        code = real_fmod_get_num_drivers(self, &drivers);
        {
            char line[160];
            wsprintfA(line, "FMOD bridge: pre-init drivers output=%s result=%d count=%d",
                      fmod_output_name(output), code, drivers);
            log_line(line);
        }
        if (!code && drivers > 0 && real_fmod_set_driver) {
            code = real_fmod_set_driver(self, 0);
            log_fmod_call("setDriver(0)", code);
        }
    }
    code = real_fmod_init(self, channels, flags, extra);
    {
        char line[320];
        wsprintfA(line, "FMOD bridge: init output=%s channels=%d flags=0x%x result=%d (%s)",
                  fmod_output_name(output), channels, flags, code,
                  fmod_error_text(code));
        log_line(line);
    }
    return code;
}

static int WINAPI bridge_fmod_init(void *self, int channels, unsigned flags, void *extra)
{
    int code;
    int sequence[4];
    int count = 0;
    int index;
    if (self == &fake_fmod_system || !load_real_fmod()) return fake_fmod_init(self, channels, flags, extra);

    /* Prefer the requested backend, then try the other desktop backends. */
    sequence[count++] = fmod_output_preference;
    if (fmod_output_preference != 8) sequence[count++] = 8;
    if (fmod_output_preference != 6) sequence[count++] = 6;
    if (fmod_output_preference != 7 && count < 4) sequence[count++] = 7;

    code = 1;
    for (index = 0; index < count; ++index) {
        code = try_fmod_init_output(self, sequence[index], channels, flags, extra);
        if (!code) break;
    }
    fmod_output_applied = TRUE;
    if (!code) log_fmod_driver_state(self);
    return code;
}

static void force_fmod_group_audible(void *group, const char *reason);

static int WINAPI bridge_fmod_set_3d_settings(void *s,float a,float b,float c)
{ int r; if (s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_three_floats(s,a,b,c); r=real_fmod_set_3d_settings(s,a,b,c); if(r) log_fmod_call("set3DSettings",r); return r; }
static int WINAPI bridge_fmod_create_group(void *s,const char *n,void **o)
{
    int r;
    if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_create_group(s,n,o);
    r=real_fmod_create_group(s,n,o);
    if(r) log_fmod_call("createChannelGroup",r);
    if (!r && o && *o) {
        if (n && ascii_prefix(n, "Sound")) fmod_sound_group = *o;
        if (n && ascii_prefix(n, "Music")) fmod_music_group = *o;
        force_fmod_group_audible(*o, n ? n : "channel group");
    }
    return r;
}
static void force_fmod_group_audible(void *group, const char *reason)
{
    int mute_result = 0;
    int volume_result = 0;
    char line[256];
    if (!group || !load_real_fmod()) return;
    if (real_fmod_set_mute) mute_result = real_fmod_set_mute(group, FALSE);
    if (real_fmod_set_volume) volume_result = real_fmod_set_volume(group, 1.0f);
    wsprintfA(line, "FMOD bridge: force audible (%s) group=%p mute=%d volume=%d",
              reason, group, mute_result, volume_result);
    log_line(line);
}

static int WINAPI bridge_fmod_get_group(void *s,void **o)
{
    int r;
    if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_get_group(s,o);
    r=real_fmod_get_group(s,o);
    if(r) log_fmod_call("getMasterChannelGroup",r);
    if (!r && o && *o) {
        fmod_master_group = *o;
        force_fmod_group_audible(fmod_master_group, "master acquired");
    }
    return r;
}
static int WINAPI bridge_fmod_add_group(void *s,void *g,int p,void **c)
{ int r; if(s==&fake_fmod_group||!load_real_fmod()) return fake_fmod_add_group(s,g,p,c); r=real_fmod_add_group(s,g,p,c); if(r) log_fmod_call("addGroup",r); return r; }
static int WINAPI bridge_fmod_sound_release(void *s)
{ if(s==&fake_fmod_sound||!load_real_fmod()) return fake_fmod_no_args(s); return real_fmod_sound_release(s); }
static int WINAPI bridge_fmod_system_close(void *s)
{ if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_no_args(s); return real_fmod_system_close(s); }
static int WINAPI bridge_fmod_system_release(void *s)
{ if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_no_args(s); return real_fmod_system_release(s); }
static int WINAPI bridge_fmod_mixer_resume(void *s)
{ if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_no_args(s); return real_fmod_mixer_resume(s); }
static int WINAPI bridge_fmod_mixer_suspend(void *s)
{ if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_no_args(s); return real_fmod_mixer_suspend(s); }
static int WINAPI bridge_fmod_set_mute(void *s,int v)
{
    int requested = v;
    int result;
    char line[192];
    if(s==&fake_fmod_group||s==&fake_fmod_channel||!load_real_fmod()) return fake_fmod_bool(s,v);
    /* UWP lifecycle can leave the master group muted in an HWND host. */
    if (v && game_window && GetForegroundWindow() == game_window) v = FALSE;
    result = real_fmod_set_mute(s,v);
    wsprintfA(line, "FMOD bridge: setMute group=%p requested=%d applied=%d result=%d",
              s, requested, v, result);
    log_line(line);
    return result;
}
static int WINAPI bridge_fmod_set_volume(void *s,float v)
{
    int r;
    char line[192];
    if(s==&fake_fmod_group||s==&fake_fmod_channel||!load_real_fmod()) return fake_fmod_float(s,v);
    r=real_fmod_set_volume(s,v);
    wsprintfA(line, "FMOD bridge: setVolume object=%p value_milli=%d result=%d",
              s, (int)(v * 1000.0f), r);
    log_line(line);
    return r;
}

static int bridge_fmod_create_common(FmodCreateSoundFn function, const char *kind,
                                     void *s,const char *name,unsigned mode,
                                     void *info,void **out)
{
    char normalized[MAX_PATH * 3];
    const char *used;
    const char *logged;
    int result;
    if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_create_sound(s,name,mode,info,out);
    /* FMOD_OPENMEMORY receives encoded audio/FSB bytes in the `name`
     * parameter.  Treating those bytes as an ms-appx path both corrupts the
     * bank and replaces the stable resource-pack buffer with stack storage. */
    if (mode & 0x00000800u) {
        used = name;
        logged = "<memory sound bank>";
    } else {
        used = normalize_fmod_path(name, normalized, sizeof(normalized));
        logged = used;
    }
    result = function(s, used, mode, info, out);
    if (result || fmod_create_log_count < 48) {
        log_fmod_sound_result(kind, logged, mode, result, out ? *out : NULL);
        ++fmod_create_log_count;
    }
    return result;
}
static int WINAPI bridge_fmod_create_stream(void *s,const char *n,unsigned m,void *i,void **o)
{ return bridge_fmod_create_common(real_fmod_create_stream,"createStream",s,n,m,i,o); }
static int WINAPI bridge_fmod_create_sound(void *s,const char *n,unsigned m,void *i,void **o)
{ return bridge_fmod_create_common(real_fmod_create_sound,"createSound",s,n,m,i,o); }
static int WINAPI bridge_fmod_set_min_max(void *s,float a,float b)
{ if(s==&fake_fmod_sound||!load_real_fmod()) return fake_fmod_two_floats(s,a,b); return real_fmod_set_min_max(s,a,b); }
static int WINAPI bridge_fmod_get_sub_count(void *s,int *o)
{ if(s==&fake_fmod_sound||!load_real_fmod()) return fake_fmod_get_count(s,o); return real_fmod_get_sub_count(s,o); }
static int WINAPI bridge_fmod_get_sub_sound(void *s,int i,void **o)
{ if(s==&fake_fmod_sound||!load_real_fmod()) return fake_fmod_get_subsound(s,i,o); return real_fmod_get_sub_sound(s,i,o); }
static int WINAPI bridge_fmod_play_sound(void *s,void *sound,void *group,int paused,void **out)
{
    int r;
    if(s==&fake_fmod_system||sound==&fake_fmod_sound||!load_real_fmod()) return fake_fmod_play_sound(s,sound,group,paused,out);
    r=real_fmod_play_sound(s,sound,group,paused,out);
    if(r || fmod_play_log_count < 32) {
        char line[256];
        wsprintfA(line,"FMOD bridge: playSound result=%d paused=%d sound=%p group=%p channel=%p",
                  r,paused,sound,group,out?*out:NULL);
        log_line(line);
        ++fmod_play_log_count;
    }
    return r;
}
static int WINAPI bridge_fmod_set_attributes(void *s,const void *a,const void *b,const void *c)
{ if(s==&fake_fmod_channel||!load_real_fmod()) return fake_fmod_vectors(s,a,b,c); return real_fmod_set_3d_attributes(s,a,b,c); }
static int WINAPI bridge_fmod_set_pitch(void *s,float v)
{ if(s==&fake_fmod_channel||!load_real_fmod()) return fake_fmod_float(s,v); return real_fmod_set_pitch(s,v); }
static int WINAPI bridge_fmod_set_paused(void *s,int v)
{ if(s==&fake_fmod_channel||!load_real_fmod()) return fake_fmod_bool(s,v); return real_fmod_set_paused(s,v); }
static int WINAPI bridge_fmod_is_playing(void *s,BYTE *o)
{ if(s==&fake_fmod_channel||!load_real_fmod()) return fake_fmod_is_playing(s,o); return real_fmod_is_playing(s,o); }
static int WINAPI bridge_fmod_stop(void *s)
{ if(s==&fake_fmod_channel||!load_real_fmod()) return fake_fmod_no_args(s); return real_fmod_stop(s); }
static int WINAPI bridge_fmod_update(void *s)
{
    int r;
    if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_no_args(s);
    r=real_fmod_update(s);
    if(r) log_fmod_call("update",r);
    if (!r && (++fmod_update_counter % 120u) == 0u &&
        game_window && GetForegroundWindow() == game_window) {
        if (fmod_master_group && real_fmod_set_mute)
            real_fmod_set_mute(fmod_master_group, FALSE);
        if (fmod_sound_group && real_fmod_set_mute)
            real_fmod_set_mute(fmod_sound_group, FALSE);
        if (fmod_music_group && real_fmod_set_mute)
            real_fmod_set_mute(fmod_music_group, FALSE);
    }
    return r;
}
static int WINAPI bridge_fmod_set_listener(void *s,int l,const void *p,const void *v,const void *f,const void *u)
{ if(s==&fake_fmod_system||!load_real_fmod()) return fake_fmod_listener(s,l,p,v,f,u); return real_fmod_set_listener(s,l,p,v,f,u); }

#ifdef WIN32CRAFT_ARM32
/* ARM32 log (the ONLY feedback channel of a device run).  Differences to the x86 log_line below:
 *  - always on (the x86 build needs the WIN32CRAFT_DEBUG environment variable); WIN32CRAFT_DEBUG=0 turns it off;
 *  - the file is opened once under a spin lock (log_line is called from game threads through the hooks), with
 *    CreateFileW(FILE_APPEND_DATA) so every line is one WriteFile: no CRT buffering, nothing is lost on a crash;
 *  - the file is "win32craft.log" in the CURRENT directory at the first call (Win32Bootstrap makes that the exe directory),
 *    else %TEMP%\win32craft.log; the resulting absolute path is published in Win32CraftLogPathW so api_compat.c appends to
 *    the very same file;
 *  - opening is attempted at most 3 times (the x86 code retried fopen on every line). */
extern DWORD WINAPI GetFullPathNameW(LPCWSTR, DWORD, LPWSTR, LPWSTR *);
extern DWORD WINAPI GetTickCount(void);
extern DWORD WINAPI GetCurrentProcessId(void);
extern BOOL WINAPI TerminateProcess(HANDLE, UINT);
extern BOOL WINAPI MoveFileExW(LPCWSTR, LPCWSTR, DWORD);
extern DWORD WINAPI SetFilePointer(HANDLE, LONG, LONG *, DWORD);
extern int WINAPI lstrlenA(LPCSTR);
wchar_t Win32CraftLogPathW[MAX_PATH];
/* Tick base of the log prefix "[  12345 ms T0a3c] " (milliseconds since the first log line; thread id of the writer).
 * Shared with api_compat.c compat_log so every line of the file carries the same clock. */
static volatile LONG arm32_log_tick_base;
UINT Win32CraftLogPrefix(char *out, UINT capacity)
{
    DWORD now = GetTickCount();
    DWORD base = (DWORD)arm32_log_tick_base;
    if (!base) {
        /* first line of the process: publish the base (0 is reserved for "unset") */
        base = now ? now : 1;
        if (!__sync_bool_compare_and_swap(&arm32_log_tick_base, 0, (LONG)base)) base = (DWORD)arm32_log_tick_base;
    }
    if (!out || capacity < 32) return 0;
    return (UINT)wsprintfA(out, "[%7lu ms T%04lx] ", (unsigned long)(now - base), (unsigned long)GetCurrentThreadId());
}

/* The file is append-only (several runs follow each other), but it never grows past ARM32_LOG_MAX_BYTES (1 MiB):
 *  - at start an existing file above the limit is emptied (and a leftover win32craft.log.old of older builds is deleted);
 *  - while the game runs, log_line checks the size every ARM32_LOG_CHECK_LINES lines; above the limit the file is emptied and
 *    restarted with a note plus the first ARM32_LOG_HEAD_BYTES of this run (the start-up lines: Windows version, options, devices),
 *    so the file still starts with the facts that identify the machine.  Lines written between the check and the restart are lost. */
#define ARM32_LOG_MAX_BYTES (1024u * 1024u)
#define ARM32_LOG_CHECK_LINES 64
#define ARM32_LOG_HEAD_BYTES 8192u
static DWORD arm32_log_session_start;      /* file size when this run opened it (start of this run's lines) */

static void arm32_log_rotate(const wchar_t *path)
{
    WIN32_FILE_ATTRIBUTE_DATA info;
    wchar_t old_path[MAX_PATH + 8];
    if (lstrlenW(path) + 5 >= MAX_PATH + 8) return;
    lstrcpyW(old_path, path);
    lstrcatW(old_path, L".old");
    DeleteFileW(old_path);
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &info)) return;
    if (info.nFileSizeHigh == 0 && info.nFileSizeLow <= ARM32_LOG_MAX_BYTES) return;
    /* CREATE_ALWAYS truncates; the other writers (api_compat.c) open with share read|write, so this is allowed */
    {
        HANDLE empty = CreateFileW(path, 2 /* FILE_WRITE_DATA */, 3, NULL, 2 /* CREATE_ALWAYS */, 0x80, NULL);
        if (empty != INVALID_HANDLE_VALUE) CloseHandle(empty);
    }
}

/* the file reached the limit: keep the head of this run, drop everything else */
static void arm32_log_restart(HANDLE file, const wchar_t *path)
{
    static char head[ARM32_LOG_HEAD_BYTES + 1];
    static DWORD head_length;
    static BOOL head_taken;
    DWORD written;
    HANDLE reader;
    HANDLE empty;
    char note[160];

    if (!path || !path[0]) return;
    if (!head_taken) {
        /* the first restart of this run: the head is still in the file, at the offset where this run began */
        head_taken = TRUE;
        reader = CreateFileW(path, 0x80000000u /* GENERIC_READ */, 3, NULL, 3 /* OPEN_EXISTING */, 0x80, NULL);
        if (reader != INVALID_HANDLE_VALUE) {
            if (arm32_log_session_start < ARM32_LOG_MAX_BYTES - ARM32_LOG_HEAD_BYTES &&
                SetFilePointer(reader, (LONG)arm32_log_session_start, NULL, 0 /* FILE_BEGIN */) != 0xFFFFFFFFu) {
                if (!ReadFile(reader, head, ARM32_LOG_HEAD_BYTES, &head_length, NULL)) head_length = 0;
            }
            CloseHandle(reader);
        }
        while (head_length && head[head_length - 1] != '\n') --head_length;      /* whole lines only */
    }
    empty = CreateFileW(path, 2 /* FILE_WRITE_DATA */, 3, NULL, 2 /* CREATE_ALWAYS */, 0x80, NULL);
    if (empty == INVALID_HANDLE_VALUE) return;
    CloseHandle(empty);
    wsprintfA(note, "==== win32craft.log restarted: it reached %lu KB; the start of this run is kept below, the lines in between were dropped ====\r\n",
              (unsigned long)(ARM32_LOG_MAX_BYTES / 1024u));
    WriteFile(file, note, (DWORD)lstrlenA(note), &written, NULL);
    if (head_length) WriteFile(file, head, head_length, &written, NULL);
    WriteFile(file, "==== ... ====\r\n", 15, &written, NULL);
}

static HANDLE arm32_log_open_file(void)
{
    wchar_t path[MAX_PATH];
    wchar_t temp[MAX_PATH];
    DWORD length;
    HANDLE file;

    length = GetFullPathNameW(L"win32craft.log", MAX_PATH, path, NULL);
    if (length && length < MAX_PATH) {
        arm32_log_rotate(path);
        file = CreateFileW(path, 4 /* FILE_APPEND_DATA */, 3 /* share read|write */, NULL, 4 /* OPEN_ALWAYS */,
                           0x80 /* FILE_ATTRIBUTE_NORMAL */, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            lstrcpyW(Win32CraftLogPathW, path);
            arm32_log_session_start = GetFileSize(file, NULL);
            return file;
        }
    }
    length = GetTempPathW(MAX_PATH, temp);
    if (length && length + 16 < MAX_PATH) {
        lstrcpyW(path, temp);
        lstrcatW(path, L"win32craft.log");
        arm32_log_rotate(path);
        file = CreateFileW(path, 4, 3, NULL, 4, 0x80, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            lstrcpyW(Win32CraftLogPathW, path);
            arm32_log_session_start = GetFileSize(file, NULL);
            return file;
        }
    }
    return INVALID_HANDLE_VALUE;
}

static void log_line(const char *text)
{
    static volatile LONG diagnostics = -1;
    static HANDLE volatile log_file = INVALID_HANDLE_VALUE;
    static volatile LONG open_lock;
    static volatile LONG open_attempts;
    static volatile LONG line_counter;
    static volatile LONG restart_lock;
    char buffer[1024];
    DWORD length = 0;
    DWORD written;
    HANDLE file;

    if (!text) return;
    if (diagnostics < 0) {
        char value[8];
        DWORD value_length = GetEnvironmentVariableA("WIN32CRAFT_DEBUG", value, sizeof(value));
        diagnostics = (value_length == 1 && value[0] == '0') ? 0 : 1;
    }
    if (!diagnostics) return;
    file = log_file;
    if (file == INVALID_HANDLE_VALUE && open_attempts < 3) {
        while (__atomic_exchange_n(&open_lock, 1, __ATOMIC_ACQUIRE)) Sleep(0);
        file = log_file;
        if (file == INVALID_HANDLE_VALUE && open_attempts < 3) {
            ++open_attempts;
            file = arm32_log_open_file();
            if (file != INVALID_HANDLE_VALUE) {
                char banner[96];
                SYSTEMTIME now;
                GetLocalTime(&now);
                (void)Win32CraftLogPrefix(buffer, sizeof(buffer));   /* fixes the tick base at the moment of opening */
                wsprintfA(banner, "==== win32craft.log opened %04u-%02u-%02u %02u:%02u:%02u (pid %lu) ====\r\n",
                          now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
                          (unsigned long)GetCurrentProcessId());
                WriteFile(file, banner, (DWORD)lstrlenA(banner), &written, NULL);
                log_file = file;
            }
        }
        __atomic_store_n(&open_lock, 0, __ATOMIC_RELEASE);
    }
    if (file != INVALID_HANDLE_VALUE) {
        UINT prefix = Win32CraftLogPrefix(buffer, sizeof(buffer));
        UINT copied = 0;
        length = prefix;
        while (text[copied] && length < sizeof(buffer) - 3) {
            buffer[length++] = text[copied++];
        }
        if (text[copied]) {
            /* longer than the buffer: write the prefix and the text in pieces, then the line end */
            WriteFile(file, buffer, prefix, &written, NULL);
            WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
            WriteFile(file, "\r\n", 2, &written, NULL);
        } else {
            buffer[length++] = '\r';
            buffer[length++] = '\n';
            WriteFile(file, buffer, length, &written, NULL);
        }
        /* size cap: every ARM32_LOG_CHECK_LINES lines look at the file size (counts the api_compat.c lines too) */
        if ((__atomic_add_fetch(&line_counter, 1, __ATOMIC_RELAXED) % ARM32_LOG_CHECK_LINES) == 0 &&
            !__atomic_exchange_n(&restart_lock, 1, __ATOMIC_ACQUIRE)) {
            DWORD size = GetFileSize(file, NULL);
            if (size != 0xFFFFFFFFu && size > ARM32_LOG_MAX_BYTES) arm32_log_restart(file, Win32CraftLogPathW);
            __atomic_store_n(&restart_lock, 0, __ATOMIC_RELEASE);
        }
    }
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
}
#else
static void log_line(const char *text)
{
    static LONG diagnostics = -1;
    static FILE *diagnostic_file;

    if (!text) return;
    if (diagnostics < 0) {
        diagnostics =
            GetEnvironmentVariableA("WIN32CRAFT_DEBUG", NULL, 0) ? 1 : 0;
    }
    if (diagnostics) {
        if (!diagnostic_file)
            diagnostic_file = fopen("win32craft.log", "a");
        if (diagnostic_file) {
            fprintf(diagnostic_file, "%s\n", text);
            fflush(diagnostic_file);
        }
        OutputDebugStringA(text);
        OutputDebugStringA("\n");
    }
}
#endif /* WIN32CRAFT_ARM32 log_line */

static wchar_t host_ascii_lower_w(wchar_t value)
{
    if (value >= L'A' && value <= L'Z') return value + (L'a' - L'A');
    return value;
}

static BOOL host_argument_equals(const wchar_t *left, const wchar_t *right)
{
    while (*left && *right) {
        if (host_ascii_lower_w(*left) != host_ascii_lower_w(*right))
            return FALSE;
        ++left;
        ++right;
    }
    return *left == 0 && *right == 0;
}

static const wchar_t *host_next_argument(
    const wchar_t *cursor, wchar_t *argument, UINT capacity)
{
    UINT length = 0;
    BOOL quoted = FALSE;
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    while (*cursor) {
        if (*cursor == L'"') {
            quoted = !quoted;
            ++cursor;
            continue;
        }
        if (!quoted && (*cursor == L' ' || *cursor == L'\t')) break;
        if (length + 1 < capacity) argument[length++] = *cursor;
        ++cursor;
    }
    argument[length] = 0;
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    return cursor;
}

#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 (unit iat_hooks): IAT hook helpers and host options.  Facts: pe_imports, gfx_compat, fl91, chat_xbox.
 * ====================================================================================================== */

/* Feature-level ceiling applied by the D3D11CreateDevice hook (facts fl91.hook_force_fl91).  Default 0x9100 = 9_1,
 * 0 = "game" (keep the game's own list).  Host option --feature-level=..., environment WIN32CRAFT_FEATURE_LEVEL. */
static UINT arm32_feature_level_cap = 0x9100u;
static BOOL arm32_feature_level_resolved;

typedef struct Arm32FeatureLevelName {
    const wchar_t *text;
    const char *name;
    UINT level;
} Arm32FeatureLevelName;

static const Arm32FeatureLevelName arm32_feature_level_names[] = {
    { L"9_1", "9_1", 0x9100u }, { L"9_2", "9_2", 0x9200u }, { L"9_3", "9_3", 0x9300u },
    { L"10_0", "10_0", 0xa000u }, { L"10_1", "10_1", 0xa100u },
    { L"11_0", "11_0", 0xb000u }, { L"11_1", "11_1", 0xb100u },
    { L"game", "game list (no clamp)", 0u }
};

static BOOL arm32_iat_streq(const char *left, const char *right)
{
    while (*left && *right) {
        if (*left != *right) return FALSE;
        ++left;
        ++right;
    }
    return *left == 0 && *right == 0;
}

/* "9_1", "9.3", "11_0", "game" (case-insensitive) -> D3D_FEATURE_LEVEL value or 0 for "game" */
static BOOL arm32_parse_feature_level(const wchar_t *text, UINT *level_out)
{
    wchar_t normalized[16];
    UINT length = 0;
    UINT index;

    if (!text) return FALSE;
    while (text[length] && length + 1 < ARRAYSIZE(normalized)) {
        normalized[length] = text[length] == L'.' ? L'_' : text[length];
        ++length;
    }
    if (text[length]) return FALSE;
    normalized[length] = 0;
    for (index = 0; index < ARRAYSIZE(arm32_feature_level_names); ++index) {
        if (host_argument_equals(normalized, arm32_feature_level_names[index].text)) {
            *level_out = arm32_feature_level_names[index].level;
            return TRUE;
        }
    }
    return FALSE;
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

static void arm32_iat_narrow_for_log(char *out, SIZE_T out_size, const wchar_t *text)
{
    SIZE_T index = 0;
    while (text[index] && index + 1 < out_size) {
        out[index] = text[index] < 0x80 ? (char)text[index] : '?';
        ++index;
    }
    out[index] = 0;
}

/* ---- ARM32 touch input options (docs: ARM32.md "Touch input").  Command line beats environment beats default.
 *   --input=auto|touch|mouse   WIN32CRAFT_INPUT       auto (default): the last used device wins (touch -> touch HUD, mouse/keyboard ->
 *                                                     mouse mode); touch: pinned touch; mouse: pointer messages untouched (old behaviour)
 *   --touch-api=auto|pointer|wmtouch|legacy  WIN32CRAFT_TOUCH_API   which Windows touch API feeds the game (auto = pointer messages)
 *   --osk=auto|on|off          WIN32CRAFT_OSK         Windows touch keyboard for text boxes (auto: only while touch is the active input)
 *   --input-log                WIN32CRAFT_INPUT_LOG=1 log every touch event / source decision */
#define ARM32_INPUT_AUTO        0
#define ARM32_INPUT_TOUCH       1
#define ARM32_INPUT_MOUSE       2
#define ARM32_TOUCHAPI_AUTO     0
#define ARM32_TOUCHAPI_POINTER  1
#define ARM32_TOUCHAPI_WMTOUCH  2
#define ARM32_TOUCHAPI_LEGACY   3
#define ARM32_OSK_AUTO          0
#define ARM32_OSK_ON            1
#define ARM32_OSK_OFF           2

static int arm32_input_policy = ARM32_INPUT_AUTO;
static int arm32_touch_api_requested = ARM32_TOUCHAPI_AUTO;
static int arm32_osk_policy = ARM32_OSK_AUTO;
static BOOL arm32_input_verbose;
static const char *const arm32_input_policy_names[] = { "auto", "touch", "mouse" };
static const char *const arm32_touch_api_names[] = { "auto", "pointer", "wmtouch", "legacy" };
static const char *const arm32_osk_policy_names[] = { "auto", "on", "off" };

typedef struct Arm32InputOption {
    const wchar_t *flag;                 /* "--input"   (value is the next argument) */
    const wchar_t *flag_equals;          /* "--input="  (value follows the '=') */
    const char *environment;
    const char *const *names;            /* value names, index == constant */
    UINT count;
    int *target;
    const char *label;
    const char *source;                  /* where the effective value came from (for the log) */
} Arm32InputOption;

static int arm32_input_match_name(const wchar_t *text, const Arm32InputOption *option)
{
    UINT index;
    UINT length;
    wchar_t wide[16];

    for (index = 0; index < option->count; ++index) {
        const char *name = option->names[index];
        for (length = 0; name[length] && length + 1 < ARRAYSIZE(wide); ++length) wide[length] = (wchar_t)(unsigned char)name[length];
        wide[length] = 0;
        if (host_argument_equals(text, wide)) return (int)index;
    }
    return -1;
}

static void arm32_parse_input_options(void)
{
    Arm32InputOption options[3];
    const wchar_t *cursor = GetCommandLineW();
    wchar_t argument[128];
    wchar_t wide[32];
    char environment[32];
    char narrow[40];
    char line[320];
    DWORD environment_length;
    UINT index;
    UINT position;
    int pending = -1;

    options[0].flag = L"--input";
    options[0].flag_equals = L"--input=";
    options[0].environment = "WIN32CRAFT_INPUT";
    options[0].names = arm32_input_policy_names;
    options[0].count = 3;
    options[0].target = &arm32_input_policy;
    options[0].label = "input policy";
    options[1].flag = L"--touch-api";
    options[1].flag_equals = L"--touch-api=";
    options[1].environment = "WIN32CRAFT_TOUCH_API";
    options[1].names = arm32_touch_api_names;
    options[1].count = 4;
    options[1].target = &arm32_touch_api_requested;
    options[1].label = "touch api";
    options[2].flag = L"--osk";
    options[2].flag_equals = L"--osk=";
    options[2].environment = "WIN32CRAFT_OSK";
    options[2].names = arm32_osk_policy_names;
    options[2].count = 3;
    options[2].target = &arm32_osk_policy;
    options[2].label = "on-screen keyboard";

    arm32_input_policy = ARM32_INPUT_AUTO;
    arm32_touch_api_requested = ARM32_TOUCHAPI_AUTO;
    arm32_osk_policy = ARM32_OSK_AUTO;
    arm32_input_verbose = FALSE;
    for (index = 0; index < 3; ++index) options[index].source = "default";

    for (index = 0; index < 3; ++index) {
        environment_length = GetEnvironmentVariableA(options[index].environment, environment, sizeof(environment));
        if (!environment_length) continue;
        if (environment_length >= sizeof(environment)) {
            wsprintfA(line, "ARM32 input: %s is longer than 31 characters - ignored", options[index].environment);
            log_line(line);
            continue;
        }
        for (position = 0; position <= environment_length; ++position) wide[position] = (wchar_t)(unsigned char)environment[position];
        position = (UINT)arm32_input_match_name(wide, &options[index]);
        if ((int)position >= 0) {
            *options[index].target = (int)position;
            options[index].source = "environment";
        } else {
            wsprintfA(line, "ARM32 input: %s='%s' is not a valid value - ignored", options[index].environment, environment);
            log_line(line);
        }
    }
    environment_length = GetEnvironmentVariableA("WIN32CRAFT_INPUT_LOG", environment, sizeof(environment));
    if (environment_length && environment_length < sizeof(environment) &&
        !(environment_length == 1 && environment[0] == '0')) {
        arm32_input_verbose = TRUE;
    }

    cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
    while (*cursor) {
        const wchar_t *value = NULL;
        int which = -1;

        cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
        if (pending >= 0) {
            which = pending;
            value = argument;
            pending = -1;
        } else if (host_argument_equals(argument, L"--input-log")) {
            arm32_input_verbose = TRUE;
            continue;
        } else {
            for (index = 0; index < 3; ++index) {
                if (host_argument_equals(argument, options[index].flag)) {
                    pending = (int)index;
                    break;
                }
                value = arm32_iat_argument_value(argument, options[index].flag_equals);
                if (value) {
                    which = (int)index;
                    break;
                }
            }
            if (!value) continue;
        }
        position = (UINT)arm32_input_match_name(value, &options[which]);
        if ((int)position >= 0) {
            *options[which].target = (int)position;
            options[which].source = "command line";
        } else {
            arm32_iat_narrow_for_log(narrow, sizeof(narrow), value);
            wsprintfA(line, "ARM32 input: %s '%s' is not a valid value (%s) - ignored", options[which].label, narrow,
                      options[which].count == 4 ? "auto pointer wmtouch legacy" : (which == 0 ? "auto touch mouse" : "auto on off"));
            log_line(line);
        }
    }
    if (pending >= 0) {
        log_line("ARM32 input: an input option (--input / --touch-api / --osk) was the last argument and has no value - ignored");
    }
    wsprintfA(line, "ARM32 effective input options: input=%s (%s), touch-api=%s (%s), osk=%s (%s), input-log=%d",
              arm32_input_policy_names[arm32_input_policy], options[0].source,
              arm32_touch_api_names[arm32_touch_api_requested], options[1].source,
              arm32_osk_policy_names[arm32_osk_policy], options[2].source, (int)arm32_input_verbose);
    log_line(line);
}

void Win32CraftParseHostOptions(void)
{
    const wchar_t *cursor = GetCommandLineW();
    wchar_t argument[128];
    wchar_t wide[32];
    char environment[32];
    char narrow[40];
    char line[224];
    DWORD environment_length;
    UINT level = 0;
    UINT index;
    BOOL expect_level = FALSE;
    const wchar_t *value;
    const char *source = "default";
    const char *name = "9_1";

    host_unlimited_fps = FALSE;
    arm32_feature_level_cap = 0x9100u;
    environment_length = GetEnvironmentVariableA("WIN32CRAFT_FEATURE_LEVEL", environment, sizeof(environment));
    if (environment_length && environment_length < sizeof(environment)) {
        for (index = 0; index <= environment_length; ++index)
            wide[index] = (wchar_t)(unsigned char)environment[index];
        if (arm32_parse_feature_level(wide, &level)) {
            arm32_feature_level_cap = level;
            source = "environment WIN32CRAFT_FEATURE_LEVEL";
        } else {
            wsprintfA(line, "WIN32CRAFT_FEATURE_LEVEL='%s' is not one of 9_1 9_2 9_3 10_0 10_1 11_0 11_1 game - ignored",
                      environment);
            log_line(line);
        }
    } else if (environment_length) {
        log_line("WIN32CRAFT_FEATURE_LEVEL is longer than 31 characters - ignored");
    }

    cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
    while (*cursor) {
        cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
        value = NULL;
        if (expect_level) {
            expect_level = FALSE;
            value = argument;
        } else if (host_argument_equals(argument, L"-maxfps")) {
            host_unlimited_fps = TRUE;
        } else if (host_argument_equals(argument, L"--feature-level")) {
            expect_level = TRUE;
        } else {
            value = arm32_iat_argument_value(argument, L"--feature-level=");
        }
        if (value) {
            if (arm32_parse_feature_level(value, &level)) {
                arm32_feature_level_cap = level;
                source = "command line --feature-level";
            } else {
                arm32_iat_narrow_for_log(narrow, sizeof(narrow), value);
                wsprintfA(line, "--feature-level='%s' is not one of 9_1 9_2 9_3 10_0 10_1 11_0 11_1 game - ignored",
                          narrow);
                log_line(line);
            }
        }
    }
    if (expect_level) log_line("--feature-level was the last argument and has no value - ignored");
    log_line(host_unlimited_fps
        ? "Win32 frame limit: unlimited (VSync disabled)"
        : "Win32 frame limit: display VSync");

    for (index = 0; index < ARRAYSIZE(arm32_feature_level_names); ++index) {
        if (arm32_feature_level_names[index].level == arm32_feature_level_cap)
            name = arm32_feature_level_names[index].name;
    }
    wsprintfA(line, "ARM32 effective feature level cap: %s (0x%04X), source: %s",
              name, (unsigned)arm32_feature_level_cap, source);
    log_line(line);
    arm32_parse_input_options();
    arm32_feature_level_resolved = TRUE;
}

/* printable-ASCII copy of a possibly unreadable C string (copy_ascii_sample is defined further down the file) */
static void arm32_iat_copy_ascii(char *out, SIZE_T out_size, const char *text)
{
    SIZE_T index = 0;
    if (!out || !out_size) return;
    if (text && !IsBadReadPtr(text, 1)) {
        while (index + 1 < out_size && !IsBadReadPtr(text + index, 1) && text[index]) {
            unsigned char value = (unsigned char)text[index];
            out[index] = (value >= 0x20 && value < 0x7f) ? (char)value : '.';
            ++index;
        }
    }
    out[index] = 0;
}

/* "module.dll+0xOFFSET (base ptr)" for a code/data address, from the export-directory name of the owning image.
 * Only VirtualQuery/IsBadReadPtr are used so it is safe on any pointer. */
static void arm32_iat_describe_address(const void *address, char *out, SIZE_T out_size)
{
    MEMORY_BASIC_INFORMATION info;
    const BYTE *base;
    const char *module_name = NULL;
    char name_copy[48];
    DWORD pe_offset;
    DWORD export_rva;
    DWORD name_rva;

    if (!out || !out_size) return;
    out[0] = 0;
    if (!address) {
        wsprintfA(out, "null");
        return;
    }
    if (!VirtualQuery(address, &info, sizeof(info)) || !info.AllocationBase || info.State != MEM_COMMIT) {
        wsprintfA(out, "%p (not mapped)", address);
        return;
    }
    base = (const BYTE *)info.AllocationBase;
    if (!IsBadReadPtr(base, 0x40) && base[0] == 'M' && base[1] == 'Z') {
        pe_offset = *(const DWORD *)(base + 0x3c);
        if (pe_offset >= 0x40 && pe_offset < 0x1000 && !IsBadReadPtr(base + pe_offset, 0x80) &&
            *(const DWORD *)(base + pe_offset) == 0x00004550u) {
            export_rva = *(const DWORD *)(base + pe_offset + 0x78);
            if (export_rva && !IsBadReadPtr(base + export_rva, 0x28)) {
                name_rva = *(const DWORD *)(base + export_rva + 0xc);
                if (name_rva) module_name = (const char *)(base + name_rva);
            }
        }
    }
    if (module_name) {
        arm32_iat_copy_ascii(name_copy, sizeof(name_copy), module_name);
    } else {
        name_copy[0] = '?';
        name_copy[1] = 0;
    }
    wsprintfA(out, "%s+0x%lx (image base %p)", name_copy,
              (unsigned long)((ULONG_PTR)address - (ULONG_PTR)base), (const void *)base);
}

/* Walk `lookup_rva` (hint/name table) in parallel with `iat_rva` and return the IAT cell of the import called
 * `function_name`; NULL when absent. */
static void **arm32_iat_walk_names(BYTE *image, DWORD image_size, DWORD lookup_rva, DWORD iat_rva,
                                      const char *function_name)
{
    UINT index;
    for (index = 0; index < 8192; ++index) {
        const DWORD *entry_address = (const DWORD *)(image + lookup_rva + 4 * index);
        DWORD entry;
        if (IsBadReadPtr(entry_address, sizeof(*entry_address))) return NULL;
        entry = *entry_address;
        if (!entry) return NULL;
        if (entry & 0x80000000u) continue;
        if (entry + 2 >= image_size) continue;
        if (arm32_iat_streq((const char *)(image + entry + 2), function_name))
            return (void **)(image + iat_rva + 4 * index);
    }
    return NULL;
}

/* Find the IAT cell of a function imported BY NAME in the live PE image: the normal import directory, or the delay
 * import directory when `delay` (RVA-based descriptors, attributes bit 0).  The DLL name is not compared because the
 * patcher renames several of them (VCRUNTIME140_APP.dll -> VCRUNTIME140.dll, ...).  PE32 layout: data directory
 * array at nt+0x78, import = [1] (nt+0x80), delay import = [13] (nt+0xe0). */
static void **arm32_iat_find_cell(BYTE *image, BOOL delay, const char *function_name)
{
    const BYTE *nt;
    const DWORD *descriptor;
    DWORD pe_offset;
    DWORD image_size;
    DWORD directory_rva;
    UINT count;

    if (!image || IsBadReadPtr(image, 0x40)) return NULL;
    pe_offset = *(const DWORD *)(image + 0x3c);
    nt = image + pe_offset;
    if (pe_offset < 0x40 || pe_offset > 0x1000 || IsBadReadPtr(nt, 0x108) || *(const DWORD *)nt != 0x00004550u)
        return NULL;
    image_size = *(const DWORD *)(nt + 0x50);
    directory_rva = *(const DWORD *)(nt + (delay ? 0xe0 : 0x80));
    if (!directory_rva || directory_rva >= image_size) return NULL;
    descriptor = (const DWORD *)(image + directory_rva);
    for (count = 0; count < 256; ++count) {
        void **cell;
        if (!delay) {
            /* OriginalFirstThunk, TimeDateStamp, ForwarderChain, Name, FirstThunk */
            if (IsBadReadPtr(descriptor, 20)) return NULL;
            if (!descriptor[3] && !descriptor[4]) return NULL;
            cell = arm32_iat_walk_names(image, image_size,
                                           descriptor[0] ? descriptor[0] : descriptor[4], descriptor[4],
                                           function_name);
            descriptor += 5;
        } else {
            /* grAttrs, rvaDLLName, rvaHmod, rvaIAT, rvaINT, rvaBoundIAT, rvaUnloadIAT, dwTimeStamp */
            if (IsBadReadPtr(descriptor, 32)) return NULL;
            if (!descriptor[1]) return NULL;
            cell = (descriptor[0] & 1u)
                ? arm32_iat_walk_names(image, image_size, descriptor[4], descriptor[3], function_name)
                : NULL;
            descriptor += 8;
        }
        if (cell) return cell;
    }
    return NULL;
}

/* IAT cell of `function_name`: the live import table wins over the fact RVA (a disagreement is logged). */
static void **arm32_iat_resolve_cell(BYTE *image, BOOL delay, const char *function_name, DWORD fact_rva)
{
    void **by_name = arm32_iat_find_cell(image, delay, function_name);
    void **by_fact = (void **)(image + fact_rva);
    char line[256];

    if (!by_name) {
        wsprintfA(line, "ARM32 import %.80s not found by name in the %s import directory; using fact rva %08lx",
                  function_name, delay ? "delay" : "normal", (unsigned long)fact_rva);
        log_line(line);
        return by_fact;
    }
    if (by_name != by_fact) {
        wsprintfA(line, "ARM32 WARNING import %.80s: live import table cell rva %08lx != fact rva %08lx - using the live cell",
                  function_name, (unsigned long)((ULONG_PTR)by_name - (ULONG_PTR)image), (unsigned long)fact_rva);
        log_line(line);
    }
    return by_name;
}

/* Function pointers stored in data must carry the Thumb bit (PORT_RULES 5); ours already do, OR-ing is idempotent. */
static void *arm32_iat_thumb_pointer(void *function)
{
    return (void *)((ULONG_PTR)function | 1u);
}

/* Hook one regular IAT cell: resolve by name, require the loader-resolved value (non-null), log where it points,
 * remember it in *original_out BEFORE the hook becomes live, then patch with arm32_patch_cell (RW only). */
static BOOL arm32_iat_hook_import(BYTE *image, BOOL delay, const char *function_name, DWORD fact_rva,
                              void *hook, void **original_out, const char *what)
{
    void **cell = arm32_iat_resolve_cell(image, delay, function_name, fact_rva);
    void *original;
    char where[160];
    char line[400];

    if (!cell || ((ULONG_PTR)cell & 3u) || IsBadReadPtr(cell, sizeof(*cell))) {
        wsprintfA(line, "ARM32 hook %s: IAT cell %p is null, misaligned or unreadable", what, (void *)cell);
        log_line(line);
        return FALSE;
    }
    original = *cell;
    arm32_iat_describe_address(original, where, sizeof(where));
    wsprintfA(line, "ARM32 hook %s: cell rva %08lx currently %p = %s", what,
              (unsigned long)((ULONG_PTR)cell - (ULONG_PTR)image), original, where);
    log_line(line);
    if (!original) {
        wsprintfA(line, "ARM32 hook %s: the cell is null, the loader did not resolve this import - not hooked", what);
        log_line(line);
        return FALSE;
    }
    if (original == arm32_iat_thumb_pointer(hook)) {
        /* second installation: remembering our own hook as "original" would make the hook call itself forever */
        wsprintfA(line, "ARM32 hook %s: the cell already holds this hook - the saved original is kept", what);
        log_line(line);
        return !original_out || *original_out != NULL;
    }
    if (original_out) *original_out = original;
    if (!arm32_patch_cell(cell, original, arm32_iat_thumb_pointer(hook), NULL, what)) {
        if (original_out) *original_out = NULL;
        return FALSE;
    }
    return TRUE;
}

#else
void Win32CraftParseHostOptions(void)
{
    const wchar_t *cursor = GetCommandLineW();
    wchar_t argument[128];
    host_unlimited_fps = FALSE;
    cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
    while (*cursor) {
        cursor = host_next_argument(cursor, argument, ARRAYSIZE(argument));
        if (host_argument_equals(argument, L"-maxfps"))
            host_unlimited_fps = TRUE;
    }
    log_line(host_unlimited_fps
        ? "Win32 frame limit: unlimited (VSync disabled)"
        : "Win32 frame limit: display VSync");
}

#endif

UINT Win32CraftPresentSyncInterval(void)
{
    return host_unlimited_fps ? 0 : 1;
}

#ifdef WIN32CRAFT_ARM32
/* abi_audit: private FakeKind for the "completed IAsyncOperation<HSTRING> yielding an empty string" fake below. */
#define ARM32_FAKE_KIND_EMPTY_STRING_ASYNC ((FakeKind)0x32a01)
/* The x86 host relied on TSO ordering when it published data to other threads.  ARMv7 is weakly ordered, so every
 * "write data, then set a flag/pointer" pattern in the shim goes through a full barrier (dmb ish). */
#define ARM32_MEMORY_BARRIER() __sync_synchronize()
/* The ARM kernel32.lib does NOT export InterlockedIncrement/Decrement/Exchange (they are compiler intrinsics on ARM; verified:
 * lld-link "undefined symbol: InterlockedIncrement" against um/arm/kernel32.lib), but win32_compat.h declares them as imports.
 * Use the GCC/clang atomic builtins (seq_cst == the full barrier Windows' Interlocked* give).  The guards make this a no-op if
 * a header-level definition (win32_compat.h / 00_core) is added before this point. */
#ifndef InterlockedIncrement
#define InterlockedIncrement(p)    __atomic_add_fetch((volatile LONG *)(p), 1, __ATOMIC_SEQ_CST)
#endif
#ifndef InterlockedDecrement
#define InterlockedDecrement(p)    __atomic_sub_fetch((volatile LONG *)(p), 1, __ATOMIC_SEQ_CST)
#endif
#ifndef InterlockedExchange
#define InterlockedExchange(p, v)  __atomic_exchange_n((volatile LONG *)(p), (LONG)(v), __ATOMIC_SEQ_CST)
#endif
/* ABI assumptions the ARM32 shim relies on (see abi_audit notes): */
_Static_assert(sizeof(FakeInspectable) == 12, "FakeInspectable layout: vtable, refcount, kind");
_Static_assert(sizeof(wchar_t) == 2, "WinRT HSTRING / UTF-16 literals need a 16-bit wchar_t");
_Static_assert(sizeof(LONGLONG) == 8 && sizeof(ULONG_PTR) == 4, "EventRegistrationToken is 64-bit, pointers are 32-bit");

/* fake_query_interface answers every IID it has no special case for with the object itself.  That is only right when the
 * game really wants the same vtable; log the IIDs so a wrong projection (e.g. IDisplayInformation2, 4DCD0021-FAD1-4B8E-
 * 8EDF-775887B8BF19, which wants get_RawPixelsPerViewPixel(double*) at slot 6) shows up in the device log. */
static void arm32_log_default_query_interface(const FakeInspectable *object, REFIID iid)
{
    static volatile LONG count;
    static const GUID known[] = {
        { 0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46} }, /* IUnknown */
        { 0xaf86e2e0, 0xb12d, 0x4c6a, {0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90} }, /* IInspectable */
        { 0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90} }, /* IAgileObject */
        { 0x00000035, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46} }  /* IActivationFactory */
    };
    char line[200];
    unsigned index;
    if (!iid || !object) return;
    for (index = 0; index < ARRAYSIZE(known); ++index) {
        if (memcmp(iid, &known[index], sizeof(GUID)) == 0) return;
    }
    if (InterlockedIncrement(&count) > 300) return;
    wsprintfA(line,
        "ARM32 QI on fake kind=%d object=%p IID=%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X answered with the object itself",
        (int)object->kind, object, (unsigned long)iid->Data1, (unsigned)iid->Data2,
        (unsigned)iid->Data3, (unsigned)iid->Data4[0], (unsigned)iid->Data4[1],
        (unsigned)iid->Data4[2], (unsigned)iid->Data4[3], (unsigned)iid->Data4[4],
        (unsigned)iid->Data4[5], (unsigned)iid->Data4[6], (unsigned)iid->Data4[7]);
    log_line(line);
}
#endif

static ULONG WINAPI fake_add_ref(FakeInspectable *object)
{
    return (ULONG)InterlockedIncrement(&object->references);
}

static ULONG WINAPI fake_release(FakeInspectable *object)
{
    LONG value = InterlockedDecrement(&object->references);
    if (value < 1) {
        InterlockedExchange(&object->references, 1);
        value = 1;
    }
    return (ULONG)value;
}

static BOOL fake_kind_is_async(FakeKind kind)
{
#ifdef WIN32CRAFT_ARM32
    /* the CurrentApp2 empty-string operation answers QueryInterface(IAsyncInfo) like every other fake IAsyncOperation */
    if (kind == ARM32_FAKE_KIND_EMPTY_STRING_ASYNC) return TRUE;
#endif
    switch (kind) {
    case FAKE_ASYNC_OPERATION:
    case FAKE_ASYNC_STRING:
    case FAKE_ASYNC_UNFULFILLED:
    case FAKE_ASYNC_SPEECH:
    case FAKE_ASYNC_AUDIO_GRAPH:
    case FAKE_ASYNC_BOOL:
    case FAKE_ASYNC_STORAGE_FILE:
    case FAKE_ASYNC_UINT:
        return TRUE;
    default:
        return FALSE;
    }
}

static HRESULT WINAPI fake_query_interface(
    FakeInspectable *object, REFIID iid, void **result)
{
    static const GUID async_info_iid = {
        0x00000036, 0x0000, 0x0000,
        {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    static const GUID connection_profile2_iid = {
        0xe2045145, 0x4c9f, 0x400c,
        {0x91, 0x50, 0x7e, 0xc7, 0xd6, 0xe2, 0x88, 0x8a}
    };
    static const GUID appdata2_iid = {
        0x9e65cd69, 0x0ba3, 0x4e32,
        {0xbe, 0x29, 0xb0, 0x2d, 0xe6, 0x60, 0x76, 0x38}
    };
    static const GUID application_view3_iid = {
        0x903c9ce5, 0x793a, 0x4fdf,
        {0xa2, 0xb2, 0xaf, 0x1a, 0xc2, 0x1e, 0x31, 0x08}
    };
    static const GUID core_accelerator_keys_iid = {
        0x9ffdf7f5, 0xb8c9, 0x4ef0,
        {0xb7, 0xd2, 0x1d, 0xe6, 0x26, 0x56, 0x1f, 0xc8}
    };
    static const GUID core_window5_iid = {
        0x4b4ae1e1, 0x2e6d, 0x4eaa,
        {0xbd, 0xa1, 0x1c, 0x5c, 0xc1, 0xbe, 0xe1, 0x41}
    };
    static const GUID core_dispatcher_priority_iid = {
        0xbafaecad, 0x484d, 0x41be,
        {0xba, 0x80, 0x1d, 0x58, 0xc6, 0x52, 0x63, 0xea}
    };
    if ((host_is_128 || host_is_116) &&
        (object == &win32_core_window || object == &win32_core_dispatcher) &&
        iid) {
        char line[96];
        wsprintfA(line, "CoreWindow QI IID=%08lX-%04X-%04X",
                  (unsigned long)iid->Data1, (unsigned)iid->Data2,
                  (unsigned)iid->Data3);
        log_line(line);
    }
    if (!result) {
        return E_POINTER;
    }
    /* The picker/storage IAsyncInfo projection is required only by the
     * working 1.1.5 desktop picker bridge. 0.15.10 is deliberately left on
     * its original UWP path in v24, so preserve its pre-picker QueryInterface
     * behaviour as well. */
    if (host_is_116 && object == &win32_core_dispatcher && iid &&
        memcmp(iid, &core_dispatcher_priority_iid,
               sizeof(core_dispatcher_priority_iid)) == 0) {
        *result = &core_dispatcher_priority_116;
        fake_add_ref(&core_dispatcher_priority_116);
        log_line("CoreDispatcher QI returned ICoreDispatcherWithTaskPriority");
    } else if (host_is_116 && object == &win32_core_dispatcher && iid &&
        memcmp(iid, &core_accelerator_keys_iid,
               sizeof(core_accelerator_keys_iid)) == 0) {
        *result = &core_accelerator_keys_116;
        fake_add_ref(&core_accelerator_keys_116);
        log_line("CoreDispatcher QI returned ICoreAcceleratorKeys");
    } else if (host_is_116 && object == &win32_core_window && iid &&
        memcmp(iid, &core_window5_iid, sizeof(core_window5_iid)) == 0) {
        *result = &core_window5_116;
        fake_add_ref(&core_window5_116);
        log_line("CoreWindow QI returned ICoreWindow5");
    } else if ((host_is_115 || host_is_128 || host_is_116) &&
        fake_kind_is_async(object->kind) && iid &&
        memcmp(iid, &async_info_iid, sizeof(async_info_iid)) == 0) {
        *result = &async_info;
        fake_add_ref(&async_info);
        log_line("IAsyncOperation QI returned full IAsyncInfo projection");
    } else if (object->kind == FAKE_APPLICATION_VIEW && iid &&
        memcmp(iid, &application_view3_iid,
               sizeof(application_view3_iid)) == 0) {
        *result = &application_view3;
        fake_add_ref(&application_view3);
        log_line("ApplicationView QI returned IApplicationView3");
    } else if (object->kind == FAKE_APPDATA && iid &&
        memcmp(iid, &appdata2_iid, sizeof(appdata2_iid)) == 0) {
        *result = &appdata2_object;
        fake_add_ref(&appdata2_object);
        log_line("ApplicationData QI returned IApplicationData2");
    } else if (object->kind == FAKE_CONNECTION_PROFILE && iid &&
        memcmp(iid, &connection_profile2_iid, sizeof(connection_profile2_iid))
            == 0) {
        *result = &connection_profile2;
        fake_add_ref(&connection_profile2);
        log_line("ConnectionProfile QI returned IConnectionProfile2");
    } else if (object->kind == FAKE_HOST_NAMES) {
        *result = &host_names_iterable;
        fake_add_ref(&host_names_iterable);
        log_line("HostNames QI returned IIterable");
    } else if (object->kind == FAKE_NETWORK_NAMES ||
        object->kind == FAKE_UNFULFILLED_VECTOR ||
        object->kind == FAKE_PRODUCT_LISTINGS) {
        *result = &network_names_iterable;
        fake_add_ref(&network_names_iterable);
        if (object->kind == FAKE_PRODUCT_LISTINGS) {
            log_line("ProductListings QI returned empty IIterable");
        } else if (object->kind == FAKE_UNFULFILLED_VECTOR) {
            log_line("UnfulfilledConsumables QI returned empty IIterable");
        } else {
            log_line("NetworkNames QI returned IIterable");
        }
    } else {
#ifdef WIN32CRAFT_ARM32
        arm32_log_default_query_interface(object, iid);
#endif
        *result = object;
        fake_add_ref(object);
    }
    return S_OK;
}

/* Windows 7 has no Windows.Media.SpeechSynthesis runtime class.  Unlike
 * the older generic fake QueryInterface, this bridge returns the correct
 * interface pointer for each SpeechSynthesizer projection.  That matters on
 * x86 because IClosable::Close and ISpeechSynthesizer methods have different
 * stack signatures. */
static BOOL fake_guid_equal(REFIID left, const GUID *right)
{
    return left && right && memcmp(left, right, sizeof(GUID)) == 0;
}

static HRESULT WINAPI fake_speech_query_interface(
    FakeInspectable *object, REFIID iid, void **result)
{
    static const GUID iid_iunknown = {
        0x00000000, 0x0000, 0x0000,
        {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    static const GUID iid_iinspectable = {
        0xaf86e2e0, 0xb12d, 0x4c6a,
        {0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90}
    };
    static const GUID iid_iagile = {
        0x94ea2b94, 0xe9cc, 0x49e0,
        {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}
    };
    static const GUID iid_activation_factory = {
        0x00000035, 0x0000, 0x0000,
        {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    static const GUID iid_installed_voices = {
        0x7d526ecc, 0x7533, 0x4c3f,
        {0x85, 0xbe, 0x88, 0x8c, 0x2b, 0xae, 0xeb, 0xdc}
    };
    static const GUID iid_synthesizer = {
        0xce9f7c76, 0x97f4, 0x4ced,
        {0xad, 0x68, 0xd5, 0x1c, 0x45, 0x8e, 0x45, 0xc6}
    };
    static const GUID iid_closable = {
        0x30d5a829, 0x7fa4, 0x4026,
        {0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e}
    };
    static const GUID iid_synthesizer2 = {
        0xa7c5ecb2, 0x4339, 0x4d6a,
        {0xbb, 0xf8, 0xc7, 0xa4, 0xf1, 0x54, 0x4c, 0x2e}
    };
    static const GUID iid_options = {
        0xa0e23871, 0xcc3d, 0x43c9,
        {0x91, 0xb1, 0xee, 0x18, 0x53, 0x24, 0xd8, 0x3d}
    };
    static const GUID iid_voice = {
        0xb127d6a4, 0x1291, 0x4604,
        {0xaa, 0x9c, 0x83, 0x13, 0x40, 0x83, 0x35, 0x2c}
    };
    static const GUID iid_voice_vector = {
        0xee8d63ce, 0x51ac, 0x5984,
        {0x89, 0x1b, 0xd2, 0x32, 0xfa, 0x7f, 0x64, 0x53}
    };
    static const GUID iid_voice_iterable = {
        0x3c33bb52, 0xbd98, 0x5c8c,
        {0xad, 0xee, 0xee, 0x8d, 0xa0, 0x62, 0x8e, 0xfc}
    };
    static const GUID iid_voice_iterator = {
        0x12d40a27, 0xae8d, 0x5fb0,
        {0x8f, 0xed, 0x00, 0x16, 0x5d, 0x59, 0xc6, 0xab}
    };
    FakeInspectable *selected = NULL;

    if (!result) return E_POINTER;
    *result = NULL;

    if (object->kind == FAKE_SPEECH_ACTIVATION_FACTORY ||
        object->kind == FAKE_SPEECH_VOICES_FACTORY) {
        if (fake_guid_equal(iid, &iid_installed_voices))
            selected = &speech_voices_factory;
        else if (fake_guid_equal(iid, &iid_activation_factory) ||
                 fake_guid_equal(iid, &iid_iunknown) ||
                 fake_guid_equal(iid, &iid_iinspectable) ||
                 fake_guid_equal(iid, &iid_iagile))
            selected = &speech_activation_factory;
    } else if (object->kind == FAKE_SPEECH_SYNTHESIZER ||
               object->kind == FAKE_SPEECH_CLOSABLE ||
               object->kind == FAKE_SPEECH_SYNTHESIZER2) {
        if (fake_guid_equal(iid, &iid_closable))
            selected = &speech_closable;
        else if (fake_guid_equal(iid, &iid_synthesizer2))
            selected = &speech_synthesizer2;
        else if (fake_guid_equal(iid, &iid_synthesizer) ||
                 fake_guid_equal(iid, &iid_iunknown) ||
                 fake_guid_equal(iid, &iid_iinspectable) ||
                 fake_guid_equal(iid, &iid_iagile))
            selected = &speech_synthesizer;
    } else if (object->kind == FAKE_SPEECH_OPTIONS) {
        if (fake_guid_equal(iid, &iid_options) ||
            fake_guid_equal(iid, &iid_iunknown) ||
            fake_guid_equal(iid, &iid_iinspectable) ||
            fake_guid_equal(iid, &iid_iagile))
            selected = &speech_options;
    } else if (object->kind == FAKE_SPEECH_VOICE) {
        if (fake_guid_equal(iid, &iid_voice) ||
            fake_guid_equal(iid, &iid_iunknown) ||
            fake_guid_equal(iid, &iid_iinspectable) ||
            fake_guid_equal(iid, &iid_iagile))
            selected = &speech_voice;
    } else if (object->kind == FAKE_SPEECH_VOICE_VECTOR ||
               object->kind == FAKE_SPEECH_VOICE_ITERABLE ||
               object->kind == FAKE_SPEECH_VOICE_ITERATOR) {
        if (fake_guid_equal(iid, &iid_voice_iterable))
            selected = &speech_voice_iterable;
        else if (fake_guid_equal(iid, &iid_voice_iterator))
            selected = &speech_voice_iterator;
        else if (fake_guid_equal(iid, &iid_voice_vector) ||
                 fake_guid_equal(iid, &iid_iunknown) ||
                 fake_guid_equal(iid, &iid_iinspectable) ||
                 fake_guid_equal(iid, &iid_iagile))
            selected = &speech_voice_vector;
    } else if (object->kind == FAKE_ASYNC_SPEECH) {
        selected = object;
    }

    if (!selected) return E_NOINTERFACE;
    *result = selected;
    fake_add_ref(selected);
    return S_OK;
}


/* The stock ResourceContext/ResourceManager implementation is package/MRT
 * backed.  In this unpackaged Win32 host it is unavailable on Windows 7 and
 * enters mrmcore with no valid package manifest on Windows 8.1+, where modern
 * versions
 * can crash in DllCanUnloadNow/GetInternalReferenceBlobForManifestValue.
 * Keep the local projection active for 1.1.5 and 1.2.8 on every supported Windows
 * version. ResourceContext uses several unrelated WinRT statics interfaces
 * and its QualifierValues property returns IObservableMap whose IMap/IMapView
 * projections have separate vtables, so keep those projections distinct for
 * correct x86 stdcall cleanup. */
static HRESULT WINAPI fake_resource_query_interface(
    FakeInspectable *object, REFIID iid, void **result)
{
    static const GUID iid_iunknown = {
        0x00000000, 0x0000, 0x0000,
        {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    static const GUID iid_iinspectable = {
        0xaf86e2e0, 0xb12d, 0x4c6a,
        {0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90}
    };
    static const GUID iid_iagile = {
        0x94ea2b94, 0xe9cc, 0x49e0,
        {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}
    };
    static const GUID iid_activation_factory = {
        0x00000035, 0x0000, 0x0000,
        {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
    };
    static const GUID iid_resource_context = {
        0x2fa22f4b, 0x707e, 0x4b27,
        {0xad, 0x0d, 0xd0, 0xd8, 0xcd, 0x46, 0x8f, 0xd2}
    };
    static const GUID iid_resource_context_statics = {
        0x98be9d6c, 0x6338, 0x4b31,
        {0x99, 0xdf, 0xb2, 0xb4, 0x42, 0xf1, 0x71, 0x49}
    };
    static const GUID iid_resource_context_statics2 = {
        0x41f752ef, 0x12af, 0x41b9,
        {0xab, 0x36, 0xb1, 0xeb, 0x4b, 0x51, 0x24, 0x60}
    };
    static const GUID iid_resource_context_statics3 = {
        0x20cf492c, 0xaf0f, 0x450b,
        {0x9d, 0xa6, 0x10, 0x6d, 0xd0, 0xc2, 0x9a, 0x39}
    };
    /* Parameterized-interface IIDs generated from WinRT signatures. */
    static const GUID iid_observable_string_map = {
        0x1e036276, 0x2f60, 0x55f6,
        {0xb7, 0xf3, 0xf8, 0x60, 0x79, 0xe6, 0x90, 0x0b}
    };
    static const GUID iid_string_map = {
        0xf6d1f700, 0x49c2, 0x52ae,
        {0x81, 0x54, 0x82, 0x6f, 0x99, 0x08, 0x77, 0x3c}
    };
    static const GUID iid_string_map_view = {
        0xac7f26f2, 0xfeb7, 0x5b2a,
        {0x8a, 0xc4, 0x34, 0x5b, 0xc6, 0x2c, 0xae, 0xde}
    };
    static const GUID iid_resource_manager_statics = {
        0x1cc0fdfc, 0x69ee, 0x4e43,
        {0x99, 0x01, 0x47, 0xf1, 0x26, 0x87, 0xba, 0xf7}
    };
    static const GUID iid_resource_manager = {
        0xf744d97b, 0x9988, 0x44fb,
        {0xab, 0xd6, 0x53, 0x78, 0x84, 0x4c, 0xfa, 0x8b}
    };
    static const GUID iid_resource_map = {
        0x72284824, 0xdb8c, 0x42f8,
        {0xb0, 0x8c, 0x53, 0xff, 0x35, 0x7d, 0xad, 0x82}
    };
    FakeInspectable *selected = NULL;

    if (!result) return E_POINTER;
    *result = NULL;

    switch (object->kind) {
    case FAKE_RESOURCE_CONTEXT_ACTIVATION_FACTORY:
    case FAKE_RESOURCE_CONTEXT_STATICS:
    case FAKE_RESOURCE_CONTEXT_STATICS2:
    case FAKE_RESOURCE_CONTEXT_STATICS3:
        if (fake_guid_equal(iid, &iid_resource_context_statics))
            selected = &resource_context_statics;
        else if (fake_guid_equal(iid, &iid_resource_context_statics2))
            selected = &resource_context_statics2;
        else if (fake_guid_equal(iid, &iid_resource_context_statics3))
            selected = &resource_context_statics3;
        else if (fake_guid_equal(iid, &iid_activation_factory) ||
                 fake_guid_equal(iid, &iid_iunknown) ||
                 fake_guid_equal(iid, &iid_iinspectable) ||
                 fake_guid_equal(iid, &iid_iagile))
            selected = &resource_context_activation_factory;
        break;
    case FAKE_RESOURCE_CONTEXT:
        if (fake_guid_equal(iid, &iid_resource_context) ||
            fake_guid_equal(iid, &iid_iunknown) ||
            fake_guid_equal(iid, &iid_iinspectable) ||
            fake_guid_equal(iid, &iid_iagile))
            selected = &resource_context;
        break;
    case FAKE_RESOURCE_QUALIFIER_MAP_OBSERVABLE:
    case FAKE_RESOURCE_QUALIFIER_MAP:
    case FAKE_RESOURCE_QUALIFIER_MAP_VIEW:
        if (fake_guid_equal(iid, &iid_observable_string_map))
            selected = &resource_qualifier_map_observable;
        else if (fake_guid_equal(iid, &iid_string_map))
            selected = &resource_qualifier_map;
        else if (fake_guid_equal(iid, &iid_string_map_view))
            selected = &resource_qualifier_map_view;
        else if (fake_guid_equal(iid, &iid_iunknown) ||
                 fake_guid_equal(iid, &iid_iinspectable) ||
                 fake_guid_equal(iid, &iid_iagile))
            selected = &resource_qualifier_map_observable;
        break;
    case FAKE_RESOURCE_MANAGER_STATICS:
        if (fake_guid_equal(iid, &iid_resource_manager_statics) ||
            fake_guid_equal(iid, &iid_iunknown) ||
            fake_guid_equal(iid, &iid_iinspectable) ||
            fake_guid_equal(iid, &iid_iagile))
            selected = &resource_manager_statics;
        break;
    case FAKE_RESOURCE_MANAGER:
        if (fake_guid_equal(iid, &iid_resource_manager) ||
            fake_guid_equal(iid, &iid_iunknown) ||
            fake_guid_equal(iid, &iid_iinspectable) ||
            fake_guid_equal(iid, &iid_iagile))
            selected = &resource_manager;
        break;
    case FAKE_RESOURCE_MAP:
        if (fake_guid_equal(iid, &iid_resource_map) ||
            fake_guid_equal(iid, &iid_iunknown) ||
            fake_guid_equal(iid, &iid_iinspectable) ||
            fake_guid_equal(iid, &iid_iagile))
            selected = &resource_map;
        break;
    default:
        break;
    }

    if (!selected) return E_NOINTERFACE;
    *result = selected;
    fake_add_ref(selected);
    return S_OK;
}

static HRESULT WINAPI fake_get_iids(
    FakeInspectable *object, ULONG *count, IID **iids)
{
    (void)object;
    if (count) *count = 0;
    if (iids) *iids = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_get_runtime_class_name(
    FakeInspectable *object, HSTRING *name)
{
    static const wchar_t runtime_name[] = L"Win32Craft.Win32.RuntimeObject";
    (void)object;
    if (!name) return E_POINTER;
    return WindowsCreateString(
        runtime_name, ARRAYSIZE(runtime_name) - 1, name);
}

static HRESULT WINAPI fake_get_trust_level(
    FakeInspectable *object, INT *level)
{
    (void)object;
    if (!level) return E_POINTER;
    *level = 0;
    return S_OK;
}

static HRESULT WINAPI fake_factory_current(
    FakeInspectable *factory, void **result)
{
    FakeInspectable *object;
    if (!result) return E_POINTER;
    if (factory->kind == FAKE_PACKAGE_FACTORY) {
        object = &package_object;
    } else if (factory->kind == FAKE_APPDATA_FACTORY) {
        object = &appdata_object;
    } else if (factory->kind == FAKE_FILE_SAVE_PICKER_FACTORY) {
        object = &file_save_picker;
    } else if (factory->kind == FAKE_FILE_OPEN_PICKER_FACTORY) {
        object = &file_open_picker;
    } else if (factory->kind == FAKE_LAUNCHER_OPTIONS_FACTORY) {
        object = &launcher_options;
    } else {
        object = &currentapp_object;
    }
    *result = object;
    fake_add_ref(object);
    return S_OK;
}

static HRESULT WINAPI fake_package_installed_location(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &package_folder;
    fake_add_ref(&package_folder);
    return S_OK;
}

static HRESULT WINAPI fake_appdata_local_folder(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &data_folder;
    fake_add_ref(&data_folder);
    return S_OK;
}

static HRESULT WINAPI fake_folder_path(
    FakeInspectable *folder, HSTRING *result)
{
    LPCWSTR path;
    if (!result) return E_POINTER;
    path = folder->kind == FAKE_PACKAGE_FOLDER
        ? package_path : game_data_path;
    return WindowsCreateString(path, (UINT32)lstrlenW(path), result);
}

static BOOL is_buffer_byte_access_iid(REFIID iid)
{
    static const GUID buffer_byte_access_iid = {
        0x905a0fef, 0xbc53, 0x11df,
        {0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda}
    };
    return iid && memcmp(iid, &buffer_byte_access_iid,
                         sizeof(buffer_byte_access_iid)) == 0;
}

static ULONG WINAPI fake_crypto_buffer_add_ref(FakeCryptoBuffer *buffer)
{
    return (ULONG)InterlockedIncrement(&buffer->references);
}

static ULONG WINAPI fake_crypto_buffer_release(FakeCryptoBuffer *buffer)
{
    LONG references = InterlockedDecrement(&buffer->references);
    if (!references) {
        HeapFree(GetProcessHeap(), 0, buffer->bytes);
        HeapFree(GetProcessHeap(), 0, buffer);
    }
    return (ULONG)references;
}

static HRESULT WINAPI fake_crypto_buffer_query_interface(
    FakeCryptoBuffer *buffer, REFIID iid, void **result)
{
    if (!result) return E_POINTER;
    if (is_buffer_byte_access_iid(iid)) {
        *result = &buffer->byte_access;
    } else {
        *result = buffer;
    }
    fake_crypto_buffer_add_ref(buffer);
    return S_OK;
}

static HRESULT WINAPI fake_crypto_buffer_get_iids(
    FakeCryptoBuffer *buffer, ULONG *count, IID **iids)
{
    (void)buffer;
    if (count) *count = 0;
    if (iids) *iids = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_crypto_buffer_class_name(
    FakeCryptoBuffer *buffer, HSTRING *name)
{
    (void)buffer;
    if (!name) return E_POINTER;
    return WindowsCreateString(
        L"Windows.Storage.Streams.Buffer", 30, name);
}

static HRESULT WINAPI fake_crypto_buffer_trust(
    FakeCryptoBuffer *buffer, INT *level)
{
    (void)buffer;
    if (!level) return E_POINTER;
    *level = 0;
    return S_OK;
}

static HRESULT WINAPI fake_crypto_buffer_capacity(
    FakeCryptoBuffer *buffer, UINT32 *capacity)
{
    if (!capacity) return E_POINTER;
    *capacity = buffer->capacity;
    return S_OK;
}

static HRESULT WINAPI fake_crypto_buffer_length(
    FakeCryptoBuffer *buffer, UINT32 *length)
{
    if (!length) return E_POINTER;
    *length = buffer->length;
    return S_OK;
}

static HRESULT WINAPI fake_crypto_buffer_set_length(
    FakeCryptoBuffer *buffer, UINT32 length)
{
    if (length > buffer->capacity) return E_INVALIDARG;
    buffer->length = length;
    return S_OK;
}

static HRESULT WINAPI fake_crypto_byte_query_interface(
    FakeCryptoByteAccess *access, REFIID iid, void **result)
{
    return fake_crypto_buffer_query_interface(access->owner, iid, result);
}

static ULONG WINAPI fake_crypto_byte_add_ref(FakeCryptoByteAccess *access)
{
    return fake_crypto_buffer_add_ref(access->owner);
}

static ULONG WINAPI fake_crypto_byte_release(FakeCryptoByteAccess *access)
{
    return fake_crypto_buffer_release(access->owner);
}

static HRESULT WINAPI fake_crypto_byte_buffer(
    FakeCryptoByteAccess *access, BYTE **bytes)
{
    if (!bytes) return E_POINTER;
    *bytes = access->owner->bytes;
    return S_OK;
}

static FakeCryptoBuffer *create_crypto_buffer(UINT32 length, const BYTE *bytes)
{
    FakeCryptoBuffer *buffer;
    UINT32 index;
    typedef BOOL (WINAPI *RtlGenRandomFn)(void *, ULONG);
    static RtlGenRandomFn rtl_gen_random;
    static BOOL random_resolved;

    buffer = (FakeCryptoBuffer *)HeapAlloc(
        GetProcessHeap(), 0, sizeof(*buffer));
    if (!buffer) return NULL;
    ZeroMemory(buffer, sizeof(*buffer));
    buffer->bytes = (BYTE *)HeapAlloc(
        GetProcessHeap(), 0, length ? length : 1);
    if (!buffer->bytes) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return NULL;
    }
    buffer->vtable = crypto_buffer_vtable;
    buffer->references = 1;
    buffer->kind = FAKE_CRYPTO_BUFFER;
    buffer->capacity = length;
    buffer->length = length;
    buffer->byte_access.vtable = crypto_byte_access_vtable;
    buffer->byte_access.owner = buffer;
    if (bytes) {
        CopyMemory(buffer->bytes, bytes, length);
    } else {
        if (!random_resolved) {
            HMODULE advapi = LoadLibraryW(L"advapi32.dll");
            if (advapi) {
                rtl_gen_random = (RtlGenRandomFn)GetProcAddress(
                    advapi, "SystemFunction036");
            }
#ifdef WIN32CRAFT_ARM32
            ARM32_MEMORY_BARRIER(); /* publish rtl_gen_random before the "resolved" flag */
#endif
            random_resolved = TRUE;
        }
#ifdef WIN32CRAFT_ARM32
        ARM32_MEMORY_BARRIER(); /* a thread that saw the flag set must also see rtl_gen_random */
#endif
        if (!rtl_gen_random ||
            !rtl_gen_random(buffer->bytes, (ULONG)length)) {
            SYSTEMTIME now;
            DWORD state;
            GetLocalTime(&now);
            state = ((DWORD)now.wMilliseconds << 16) ^
                ((DWORD)now.wSecond << 8) ^ (DWORD)(ULONG_PTR)buffer;
            for (index = 0; index < length; ++index) {
                state = state * 1664525u + 1013904223u;
                buffer->bytes[index] = (BYTE)(state >> 24);
            }
        }
    }
    return buffer;
}

static HRESULT WINAPI fake_crypto_compare(
    FakeInspectable *factory, FakeCryptoBuffer *left,
    FakeCryptoBuffer *right, BYTE *equal)
{
    (void)factory;
    if (!left || !right || !equal) return E_POINTER;
    *equal = left->length == right->length &&
        memcmp(left->bytes, right->bytes, left->length) == 0;
    return S_OK;
}

static HRESULT WINAPI fake_crypto_generate_random(
    FakeInspectable *factory, UINT32 length, FakeCryptoBuffer **result)
{
    (void)factory;
    if (!result) return E_POINTER;
    *result = create_crypto_buffer(length, NULL);
    if (!*result) return E_OUTOFMEMORY;
    log_line("redirected CryptographicBuffer.GenerateRandom");
    return S_OK;
}

static HRESULT WINAPI fake_crypto_generate_random_number(
    FakeInspectable *factory, UINT32 *result)
{
    FakeCryptoBuffer *buffer;
    (void)factory;
    if (!result) return E_POINTER;
    buffer = create_crypto_buffer(sizeof(*result), NULL);
    if (!buffer) return E_OUTOFMEMORY;
    CopyMemory(result, buffer->bytes, sizeof(*result));
    fake_crypto_buffer_release(buffer);
    return S_OK;
}

static HRESULT WINAPI fake_crypto_create_from_array(
    FakeInspectable *factory, UINT32 length, BYTE *bytes,
    FakeCryptoBuffer **result)
{
    (void)factory;
    if (!result || (length && !bytes)) return E_POINTER;
    *result = create_crypto_buffer(length, bytes);
    return *result ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI fake_crypto_copy_to_array(
    FakeInspectable *factory, FakeCryptoBuffer *buffer,
    UINT32 *length, BYTE **bytes)
{
    typedef void * (WINAPI *CoTaskMemAllocFn)(SIZE_T);
    static CoTaskMemAllocFn task_alloc;
    HMODULE ole32;
    (void)factory;
    if (!buffer || !length || !bytes) return E_POINTER;
    if (!task_alloc) {
        ole32 = LoadLibraryW(L"ole32.dll");
        if (ole32) {
            task_alloc = (CoTaskMemAllocFn)GetProcAddress(
                ole32, "CoTaskMemAlloc");
        }
    }
    if (!task_alloc) return E_OUTOFMEMORY;
    *bytes = (BYTE *)task_alloc(buffer->length);
    if (!*bytes && buffer->length) return E_OUTOFMEMORY;
    *length = buffer->length;
    if (buffer->length) {
        CopyMemory(*bytes, buffer->bytes, buffer->length);
    }
    return S_OK;
}

static HRESULT WINAPI fake_hardware_get_package_token(
    FakeInspectable *factory, FakeCryptoBuffer *nonce, void **result)
{
    (void)factory;
    (void)nonce;
    if (!result) return E_POINTER;
    *result = &hardware_token;
    fake_add_ref(&hardware_token);
    log_line("HardwareIdentification returned local package token");
    return S_OK;
}

static HRESULT WINAPI fake_hardware_token_id(
    FakeInspectable *object, FakeCryptoBuffer **result)
{
    static const BYTE identifier[16] = {
        'W', 'i', 'n', '3', '2', 'C', 'r', 'a', 'f', 't',
        '-', 'W', '3', '2', 0, 1
    };
    (void)object;
    if (!result) return E_POINTER;
    *result = create_crypto_buffer(sizeof(identifier), identifier);
    return *result ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI fake_hardware_token_signature(
    FakeInspectable *object, FakeCryptoBuffer **result)
{
    static const BYTE signature[16] = {
        0x57, 0x49, 0x4e, 0x33, 0x32, 0x43, 0x52, 0x41,
        0x46, 0x54, 0x57, 0x33, 0x32, 0x01, 0x28, 0x00
    };
    (void)object;
    if (!result) return E_POINTER;
    *result = create_crypto_buffer(sizeof(signature), signature);
    return *result ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI fake_hardware_token_certificate(
    FakeInspectable *object, FakeCryptoBuffer **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = create_crypto_buffer(0, NULL);
    return *result ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI fake_data_reader_from_buffer(
    FakeInspectable *factory, FakeCryptoBuffer *buffer, void **result)
{
    (void)factory;
    if (!buffer || !result) return E_POINTER;
    if (data_reader.source) fake_crypto_buffer_release(data_reader.source);
    data_reader.source = buffer;
    data_reader.position = 0;
    fake_crypto_buffer_add_ref(buffer);
    *result = &data_reader;
    fake_add_ref((FakeInspectable *)&data_reader);
    log_line("DataReader.FromBuffer returned local reader");
    return S_OK;
}

static HRESULT WINAPI fake_data_reader_get_value(
    FakeDataReader *reader, UINT32 *result)
{
    (void)reader;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_data_reader_put_value(
    FakeDataReader *reader, UINT32 value)
{
    (void)reader;
    (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_data_reader_unconsumed(
    FakeDataReader *reader, UINT32 *result)
{
    if (!result) return E_POINTER;
    *result = reader->source && reader->position < reader->source->length
        ? reader->source->length - reader->position : 0;
    return S_OK;
}

static HRESULT WINAPI fake_data_reader_read_byte(
    FakeDataReader *reader, BYTE *result)
{
    if (!result) return E_POINTER;
    if (!reader->source || reader->position >= reader->source->length)
        return E_BOUNDS;
    *result = reader->source->bytes[reader->position++];
    return S_OK;
}

static HRESULT WINAPI fake_data_reader_read_bytes(
    FakeDataReader *reader, UINT32 length, BYTE *bytes)
{
    UINT32 available;
    UINT32 copied;
    if (length && !bytes) return E_POINTER;
    available = reader->source && reader->position < reader->source->length
        ? reader->source->length - reader->position : 0;
    copied = length < available ? length : available;
    if (copied) {
        CopyMemory(bytes, reader->source->bytes + reader->position, copied);
        reader->position += copied;
    }
    if (copied < length) ZeroMemory(bytes + copied, length - copied);
    return S_OK;
}

static HRESULT WINAPI fake_memory_limit(
    FakeInspectable *object, ULONGLONG *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0x100000000ULL;
    return S_OK;
}

static HRESULT WINAPI fake_memory_usage(
    FakeInspectable *object, ULONGLONG *result)
{
    typedef struct Win32CraftProcessMemoryCounters {
        DWORD cb;
        DWORD page_fault_count;
        SIZE_T peak_working_set_size;
        SIZE_T working_set_size;
        SIZE_T quota_peak_paged_pool_usage;
        SIZE_T quota_paged_pool_usage;
        SIZE_T quota_peak_nonpaged_pool_usage;
        SIZE_T quota_nonpaged_pool_usage;
        SIZE_T pagefile_usage;
        SIZE_T peak_pagefile_usage;
    } Win32CraftProcessMemoryCounters;
    typedef BOOL (WINAPI *GetProcessMemoryInfoFn)(
        HANDLE, Win32CraftProcessMemoryCounters *, DWORD);
    static GetProcessMemoryInfoFn get_process_memory_info;
    static BOOL resolved;
    Win32CraftProcessMemoryCounters counters;
    HMODULE module;
    (void)object;
    if (!result) return E_POINTER;
    if (!resolved) {
        module = LoadLibraryW(L"psapi.dll");
        if (module) get_process_memory_info =
            (GetProcessMemoryInfoFn)GetProcAddress(
                module, "GetProcessMemoryInfo");
        resolved = TRUE;
    }
    ZeroMemory(&counters, sizeof(counters));
    counters.cb = sizeof(counters);
    if (get_process_memory_info && get_process_memory_info(
            GetCurrentProcess(), &counters, sizeof(counters))) {
        *result = (ULONGLONG)counters.working_set_size;
    } else {
        *result = 0x10000000ULL;
    }
    return S_OK;
}

static HRESULT WINAPI fake_coreapp_id(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t app_id[] = L"Minecraft.Win32";
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(app_id, ARRAYSIZE(app_id) - 1, result);
}

static HRESULT WINAPI fake_currentapp_get_license(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &license_object;
    fake_add_ref(&license_object);
    log_line("CurrentApp LicenseInformation returned");
    return S_OK;
}

static HRESULT WINAPI fake_currentapp_load_listing(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &async_operation;
    fake_add_ref(&async_operation);
    log_line("CurrentApp LoadListingInformationAsync returned");
    return S_OK;
}

static HRESULT WINAPI fake_currentapp_get_receipt(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &receipt_async_operation;
    fake_add_ref(&receipt_async_operation);
    log_line("CurrentApp receipt async operation returned");
    if (host_is_116) log_115_termination_stack("1.16 CurrentApp receipt caller");
    return S_OK;
}

static HRESULT WINAPI fake_currentapp_get_unfulfilled(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &unfulfilled_async_operation;
    fake_add_ref(&unfulfilled_async_operation);
    log_line("CurrentApp GetUnfulfilledConsumablesAsync returned");
    return S_OK;
}

static HRESULT WINAPI fake_currentapp_app_id(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t app_id[] = L"Minecraft.Win32";
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(app_id, ARRAYSIZE(app_id) - 1, result);
}

static HRESULT WINAPI fake_currentapp_link_uri(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}


#ifdef WIN32CRAFT_ARM32
extern HRESULT WINAPI WindowsDeleteString(HSTRING);   /* defined in api_compat.c (forwards to combase) */
extern BOOL WINAPI WindowsIsStringEmpty(HSTRING);
#endif
static HRESULT win32_shell_open_hstring(HSTRING uri)
{
    typedef HINSTANCE (WINAPI *ShellExecuteWLocalFn)(
        HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
    HMODULE shell32;
    ShellExecuteWLocalFn shell_execute;
    LPCWSTR text;
    UINT32 length = 0;
    HINSTANCE result;

    if (!uri) return E_INVALIDARG;
    text = WindowsGetStringRawBuffer(uri, &length);
    if (!text || !length) return E_INVALIDARG;
    shell32 = LoadLibraryW(L"shell32.dll");
    if (!shell32) return E_FAIL;
    shell_execute = (ShellExecuteWLocalFn)GetProcAddress(shell32, "ShellExecuteW");
    if (!shell_execute) return E_FAIL;
    result = shell_execute(game_window, L"open", text, NULL, NULL, 1);
    return (INT_PTR)result > 32 ? S_OK : E_FAIL;
}

static HRESULT WINAPI fake_launcher_launch_uri(
    FakeInspectable *object, void *uri, void **operation)
{
    typedef HRESULT (WINAPI *UriGetAbsoluteFn)(void *, HSTRING *);
    HSTRING text = NULL;
    HRESULT result = E_FAIL;
    (void)object;
    if (!operation) return E_POINTER;
    *operation = NULL;
    if (uri && !IsBadReadPtr(uri, sizeof(void *)) &&
        !IsBadReadPtr(*(void ***)uri, 7 * sizeof(void *)) &&
        (*(void ***)uri)[6]) {
        result = ((UriGetAbsoluteFn)(*(void ***)uri)[6])(uri, &text);
        if (SUCCEEDED(result) && text) result = win32_shell_open_hstring(text);
#ifdef WIN32CRAFT_ARM32
        /* get_AbsoluteUri returns a REAL HSTRING (native combase) that the caller owns; the x86 shim string needed no free */
        if (text) WindowsDeleteString(text);
#endif
    }
    *operation = &launcher_async_operation;
    fake_add_ref(&launcher_async_operation);
    log_line(SUCCEEDED(result)
        ? "Windows.System.Launcher URL opened through ShellExecuteW"
        : "Windows.System.Launcher URL fallback returned completed operation");
    return S_OK;
}

static HRESULT WINAPI fake_launcher_launch_uri_options(
    FakeInspectable *object, void *uri, void *options, void **operation)
{
    (void)options;
    return fake_launcher_launch_uri(object, uri, operation);
}


static HRESULT WINAPI fake_launcher_option_get(
    FakeInspectable *object, void *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *(BYTE *)value = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_launcher_option_put(
    FakeInspectable *object, ULONG_PTR value)
{
    (void)object; (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_picker_get_hstring(
    FakeInspectable *object, HSTRING *result)
{
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(L"", 0, result);
}

static HRESULT WINAPI fake_picker_put_hstring(
    FakeInspectable *object, HSTRING value)
{
    (void)object; (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_picker_get_int(
    FakeInspectable *object, INT *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT WINAPI fake_picker_put_int(
    FakeInspectable *object, INT value)
{
    (void)object; (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_picker_put_default_extension(
    FakeInspectable *object, HSTRING value)
{
    LPCWSTR text;
    UINT32 length = 0;
    (void)object;
    text = value ? WindowsGetStringRawBuffer(value, &length) : NULL;
    if (!text) {
        export_default_extension[0] = 0;
    } else {
        UINT32 copy = length < ARRAYSIZE(export_default_extension) - 1
            ? length : ARRAYSIZE(export_default_extension) - 1;
        memcpy(export_default_extension, text, copy * sizeof(wchar_t));
        export_default_extension[copy] = 0;
    }
    return S_OK;
}

static HRESULT WINAPI fake_picker_put_suggested_name(
    FakeInspectable *object, HSTRING value)
{
    LPCWSTR text;
    UINT32 length = 0;
    (void)object;
    text = value ? WindowsGetStringRawBuffer(value, &length) : NULL;
    if (!text) {
        export_suggested_name[0] = 0;
    } else {
        UINT32 copy = length < ARRAYSIZE(export_suggested_name) - 1
            ? length : ARRAYSIZE(export_suggested_name) - 1;
        memcpy(export_suggested_name, text, copy * sizeof(wchar_t));
        export_suggested_name[copy] = 0;
    }
    return S_OK;
}

static HRESULT WINAPI fake_picker_get_file_type_choices(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &file_type_choices;
    fake_add_ref(&file_type_choices);
    return S_OK;
}

static HRESULT WINAPI fake_map_insert(
    FakeInspectable *object, HSTRING key, void *value, BYTE *replaced)
{
    (void)object; (void)key; (void)value;
    if (replaced) *replaced = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_picker_pick_save_file(
    FakeInspectable *object, void **operation)
{
    static const wchar_t filter[] =
        L"Minecraft World (*.mcworld)\0*.mcworld\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog;
    wchar_t filename[MAX_PATH * 4];
    const wchar_t *extension = export_default_extension;
    size_t name_length;
    (void)object;
    if (!operation) return E_POINTER;
    *operation = NULL;
    ZeroMemory(&dialog, sizeof(dialog));
    ZeroMemory(filename, sizeof(filename));
    if (export_suggested_name[0])
        lstrcpynW(filename, export_suggested_name, ARRAYSIZE(filename));
    if (!extension[0]) extension = L"mcworld";
    name_length = lstrlenW(filename);
    {
        const wchar_t *dot_scan = filename;
        BOOL has_dot = FALSE;
        while (*dot_scan) { if (*dot_scan++ == L'.') has_dot = TRUE; }
        if (name_length && !has_dot &&
        name_length + 9 < ARRAYSIZE(filename)) {
            lstrcatW(filename, L".mcworld");
        }
    }
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = game_window;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = filename;
    dialog.nMaxFile = ARRAYSIZE(filename);
    dialog.lpstrDefExt = extension[0] == L'.' ? extension + 1 : extension;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
    export_file_selected = GetSaveFileNameW(&dialog);
    if (export_file_selected) {
        HANDLE created;
        lstrcpynW(export_file_path, filename, ARRAYSIZE(export_file_path));
        created = CreateFileW(export_file_path, GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (created != INVALID_HANDLE_VALUE) CloseHandle(created);
    } else {
        export_file_path[0] = 0;
    }
    *operation = &file_save_async_operation;
    fake_add_ref(&file_save_async_operation);
    log_line(export_file_selected
        ? "FileSavePicker redirected to Win32 save dialog"
        : "FileSavePicker Win32 save dialog cancelled");
    return S_OK;
}

/* FileOpenPicker is used by Import World in 0.15.10 and 1.1.5.
 * Keep it separate from FileSavePicker because the two game builds use
 * different historical WinRT interface revisions and therefore different
 * vtable slots. */
static HRESULT WINAPI fake_picker_get_file_type_filter(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &file_type_filter;
    fake_add_ref(&file_type_filter);
    return S_OK;
}

static HRESULT WINAPI fake_vector_append_hstring(
    FakeInspectable *object, HSTRING value)
{
    (void)object; (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_vector_get_size(
    FakeInspectable *object, UINT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_vector_get_hstring_at(
    FakeInspectable *object, UINT index, HSTRING *result)
{
    (void)object; (void)index;
    if (!result) return E_POINTER;
    *result = NULL;
    return E_BOUNDS;
}

static HRESULT WINAPI fake_vector_get_view(
    FakeInspectable *object, void **result)
{
    if (!result) return E_POINTER;
    *result = object;
    fake_add_ref(object);
    return S_OK;
}

static HRESULT WINAPI fake_vector_index_of(
    FakeInspectable *object, HSTRING value, UINT *index, BYTE *found)
{
    (void)object; (void)value;
    if (index) *index = 0;
    if (found) *found = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_vector_set_hstring(
    FakeInspectable *object, UINT index, HSTRING value)
{
    (void)object; (void)index; (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_vector_insert_hstring(
    FakeInspectable *object, UINT index, HSTRING value)
{
    (void)object; (void)index; (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_vector_remove_at(
    FakeInspectable *object, UINT index)
{
    (void)object; (void)index;
    return S_OK;
}

static HRESULT WINAPI fake_vector_no_args(FakeInspectable *object)
{
    (void)object;
    return S_OK;
}

/* Some C++/CX builds call a projected Append thunk at slot 16 rather than
 * the canonical IVector<HSTRING> slot 13.  Both slots are populated below. */
static HRESULT WINAPI fake_picker_pick_open_file(
    FakeInspectable *object, void **operation)
{
    static const wchar_t filter[] =
        L"Minecraft World (*.mcworld)\0*.mcworld\0All files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog;
    wchar_t filename[MAX_PATH * 4];
    (void)object;
    if (!operation) return E_POINTER;
    *operation = NULL;
    ZeroMemory(&dialog, sizeof(dialog));
    ZeroMemory(filename, sizeof(filename));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = game_window;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = filename;
    dialog.nMaxFile = ARRAYSIZE(filename);
    dialog.lpstrDefExt = L"mcworld";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |
                   OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    export_file_selected = GetOpenFileNameW(&dialog);
    if (export_file_selected)
        lstrcpynW(export_file_path, filename, ARRAYSIZE(export_file_path));
    else
        export_file_path[0] = 0;
    *operation = &file_open_async_operation;
    fake_add_ref(&file_open_async_operation);
    log_line(export_file_selected
        ? "FileOpenPicker redirected to Win32 open dialog"
        : "FileOpenPicker Win32 open dialog cancelled");
    return S_OK;
}

static BOOL fake_is_secondary_storage_object(FakeInspectable *object)
{
    return object == &secondary_storage_file ||
        object == &secondary_storage_item;
}

static const wchar_t *fake_storage_object_path(FakeInspectable *object)
{
    return fake_is_secondary_storage_object(object)
        ? secondary_storage_path : export_file_path;
}

static HRESULT WINAPI fake_storage_query_interface(
    FakeInspectable *object, REFIID iid, void **result)
{
    static const GUID storage_item_iid = {
        0x4207a996, 0xca2f, 0x42f7,
        {0xbd,0xe8,0x8b,0x10,0x45,0x7a,0x7f,0x30}
    };
    BOOL secondary = fake_is_secondary_storage_object(object);
    FakeInspectable *answer;
    if (!result) return E_POINTER;
    if (iid && memcmp(iid, &storage_item_iid, sizeof(GUID)) == 0)
        answer = secondary ? &secondary_storage_item : &storage_item;
    else
        answer = secondary ? &secondary_storage_file : &storage_file;
    *result = answer;
    fake_add_ref(answer);
    return S_OK;
}

static HRESULT WINAPI fake_storage_get_path(
    FakeInspectable *object, HSTRING *result)
{
    const wchar_t *path = fake_storage_object_path(object);
    if (!result) return E_POINTER;
    return WindowsCreateString(path, (UINT32)lstrlenW(path), result);
}

static HRESULT WINAPI fake_storage_get_name(
    FakeInspectable *object, HSTRING *result)
{
    const wchar_t *path = fake_storage_object_path(object);
    const wchar_t *name = path;
    const wchar_t *scan;
    if (!result) return E_POINTER;
    for (scan = path; *scan; ++scan)
        if (*scan == L'\\' || *scan == L'/') name = scan + 1;
    return WindowsCreateString(name, (UINT32)lstrlenW(name), result);
}

static HRESULT WINAPI fake_storage_get_file_type(
    FakeInspectable *object, HSTRING *result)
{
    const wchar_t *path = fake_storage_object_path(object);
    const wchar_t *dot = NULL;
    const wchar_t *scan;
    if (!result) return E_POINTER;
    for (scan = path; *scan; ++scan) if (*scan == L'.') dot = scan;
    if (!dot) dot = L".mcworld";
    return WindowsCreateString(dot, (UINT32)lstrlenW(dot), result);
}

static HRESULT WINAPI fake_storage_get_content_type(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t type[] = L"application/octet-stream";
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(type, ARRAYSIZE(type) - 1, result);
}

static HRESULT WINAPI fake_storage_get_attributes(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_storage_get_date_created(
    FakeInspectable *object, LONGLONG *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_storage_is_of_type(
    FakeInspectable *object, INT type, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = type == 0;
    return S_OK;
}

static BOOL fake_build_local_storage_path(
    HSTRING name, wchar_t *path, UINT capacity)
{
    UINT32 length = 0;
    const wchar_t *text;
    UINT base_length;
    if (!path || capacity < 2 || !name) return FALSE;
    text = WindowsGetStringRawBuffer(name, &length);
    if (!text || !length) return FALSE;
    base_length = (UINT)lstrlenW(game_data_path);
    if (base_length + 1u + length + 1u > capacity) return FALSE;
    lstrcpyW(path, game_data_path);
    if (base_length && path[base_length - 1] != L'\\' &&
        path[base_length - 1] != L'/') {
        path[base_length++] = L'\\';
        path[base_length] = 0;
    }
    CopyMemory(path + base_length, text, length * sizeof(wchar_t));
    path[base_length + length] = 0;
    return TRUE;
}

static HRESULT fake_return_secondary_storage_operation(void **operation)
{
    if (!operation) return E_POINTER;
    *operation = &secondary_storage_async_operation;
    fake_add_ref(&secondary_storage_async_operation);
    return S_OK;
}

static BOOL fake_copy_file_replace(const wchar_t *source,
    const wchar_t *destination)
{
    BYTE buffer[2048];
    HANDLE input;
    HANDLE output;
    BOOL success = TRUE;
    if (!source || !destination) return FALSE;
    input = CreateFileW(source, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input == INVALID_HANDLE_VALUE) return FALSE;
    output = CreateFileW(destination, GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) {
        CloseHandle(input);
        return FALSE;
    }
    for (;;) {
        DWORD received = 0;
        DWORD written = 0;
        if (!ReadFile(input, buffer, sizeof(buffer), &received, NULL)) {
            success = FALSE;
            break;
        }
        if (!received) break;
        while (written < received) {
            DWORD part = 0;
            if (!WriteFile(output, buffer + written, received - written,
                &part, NULL) || !part) {
                success = FALSE;
                break;
            }
            written += part;
        }
        if (!success) break;
    }
    CloseHandle(output);
    CloseHandle(input);
    if (!success) DeleteFileW(destination);
    return success;
}

/* IStorageFolder::CreateFileAsync(name, CreationCollisionOption), slot 7.
 * 0.15.10 Export World creates "tempFileRead" in ApplicationData's local
 * folder before its native archive writer starts.  v22 left this slot NULL,
 * which explains the observed jump to EIP 00000000. */
static HRESULT WINAPI fake_folder_create_file_with_collision(
    FakeInspectable *folder, HSTRING name, INT collision, void **operation)
{
    HANDLE file;
    (void)folder;
    (void)collision;
    if (!operation) return E_POINTER;
    *operation = NULL;
    secondary_storage_available = FALSE;
    secondary_storage_path[0] = 0;
    if (!fake_build_local_storage_path(name, secondary_storage_path,
        ARRAYSIZE(secondary_storage_path))) {
        return E_FAIL;
    }
    file = CreateFileW(secondary_storage_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        secondary_storage_path[0] = 0;
        return E_FAIL;
    }
    CloseHandle(file);
    secondary_storage_available = TRUE;
    log_line("0.15.10 IStorageFolder::CreateFileAsync created local temp file");
    return fake_return_secondary_storage_operation(operation);
}

/* IStorageFile::CopyAsync(folder, desiredName, NameCollisionOption), slot 12.
 * 0.15.10 Import World first copies the selected .mcworld into LocalFolder,
 * then asks the returned StorageFile for its path.  Returning E_NOTIMPL
 * without an operation left its continuation with a null method target. */
static HRESULT WINAPI fake_storage_copy_async(
    FakeInspectable *file, void *folder, HSTRING desired_name,
    INT collision, void **operation)
{
    const wchar_t *source = fake_storage_object_path(file);
    (void)folder;
    (void)collision;
    if (!operation) return E_POINTER;
    *operation = NULL;
    secondary_storage_available = FALSE;
    secondary_storage_path[0] = 0;
    if (!source || !source[0] ||
        !fake_build_local_storage_path(desired_name,
            secondary_storage_path, ARRAYSIZE(secondary_storage_path))) {
        return E_FAIL;
    }
    if (lstrcmpW(source, secondary_storage_path) != 0 &&
        !fake_copy_file_replace(source, secondary_storage_path)) {
        secondary_storage_path[0] = 0;
        return E_FAIL;
    }
    secondary_storage_available = TRUE;
    log_line("0.15.10 IStorageFile::CopyAsync copied import into LocalFolder");
    return fake_return_secondary_storage_operation(operation);
}

static HRESULT WINAPI fake_not_implemented(void)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI fake_cached_defer_updates(
    FakeInspectable *object, void *file)
{
    (void)object; (void)file;
    return S_OK;
}

static HRESULT WINAPI fake_cached_complete_updates(
    FakeInspectable *object, void *file, void **operation)
{
    (void)object; (void)file;
    if (!operation) return E_POINTER;
    *operation = &cached_file_async_operation;
    fake_add_ref(&cached_file_async_operation);
    return S_OK;
}

static HRESULT WINAPI fake_async_info_get_id(
    FakeInspectable *object, UINT *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *value = 1;
    return S_OK;
}

static HRESULT WINAPI fake_async_info_get_status(
    FakeInspectable *object, INT *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *value = 1; /* AsyncStatus::Completed */
    return S_OK;
}

static HRESULT WINAPI fake_async_info_get_error_code(
    FakeInspectable *object, HRESULT *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *value = S_OK;
    return S_OK;
}

static HRESULT WINAPI fake_async_info_cancel(FakeInspectable *object)
{
    (void)object;
    return S_OK;
}

static HRESULT WINAPI fake_async_info_close(FakeInspectable *object)
{
    (void)object;
    return S_OK;
}

static HRESULT WINAPI fake_async_put_completed(
    FakeInspectable *object, void *handler)
{
    typedef HRESULT (WINAPI *AsyncCompletedInvokeFn)(
        void *, void *, INT);
    HRESULT result;

    if (host_is_116 && object && object->kind == FAKE_ASYNC_AUDIO_GRAPH) {
        log_line("Win32 AudioGraph completion suppressed for 1.16 FMOD fallback");
        return S_OK;
    }

    if (!handler || IsBadReadPtr(handler, sizeof(void *)) ||
        IsBadReadPtr(*(void ***)handler, 4 * sizeof(void *)) ||
        !(*(void ***)handler)[3]) {
        return E_POINTER;
    }
    /*
     * This operation is already complete when C++/CX installs its handler.
     * WinRT permits invoking the delegate inline. Status 1 is Completed;
     * GetResults below supplies a real ListingInformation object so the PPL
     * continuation can retain and inspect it safely.
     */
    result = ((AsyncCompletedInvokeFn)(*(void ***)handler)[3])(
        handler, object, 1);
    log_hresult("CurrentApp listing completion callback", result);
    /* A WinRT put_Completed implementation reports whether registration
     * succeeded; an inline handler failure belongs to the async task. */
    return result;
}

static HRESULT WINAPI fake_async_get_completed(
    FakeInspectable *object, void **handler)
{
    (void)object;
    if (!handler) return E_POINTER;
    *handler = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_async_get_results(
    FakeInspectable *object, void **result)
{
    if (!result) return E_POINTER;
#ifdef WIN32CRAFT_ARM32
    if (object && object->kind == ARM32_FAKE_KIND_EMPTY_STRING_ASYNC) {
        log_line("ARM32 CurrentApp2 async GetResults returned an empty HSTRING");
        return WindowsCreateString(L"", 0, (HSTRING *)result);
    }
#endif
    if (object && object->kind == FAKE_ASYNC_STRING) {
        static const wchar_t receipt[] =
            L"<Receipt Version=\"1.0\" ReceiptDate=\"2020-08-11T00:00:00Z\" "
            L"CertificateId=\"00000000-0000-0000-0000-000000000000\" "
            L"ReceiptDeviceId=\"00000000-0000-0000-0000-000000000000\" "
            L"xmlns=\"http://schemas.microsoft.com/windows/2012/store/receipt\">"
            L"<AppReceipt Id=\"00000000-0000-0000-0000-000000000000\" "
            L"AppId=\"Microsoft.MinecraftUWP_8wekyb3d8bbwe\" "
            L"LicenseType=\"Full\" PurchaseDate=\"2020-08-11T00:00:00Z\" />"
            L"</Receipt>";
        log_line("CurrentApp receipt GetResults returned non-null HSTRING");
        return WindowsCreateString(
            receipt, ARRAYSIZE(receipt) - 1, (HSTRING *)result);
    }
    if (object && object->kind == FAKE_ASYNC_UNFULFILLED) {
        *result = &unfulfilled_vector;
        fake_add_ref(&unfulfilled_vector);
        log_line("CurrentApp unfulfilled GetResults returned empty vector");
        return S_OK;
    }
    if (object && object->kind == FAKE_ASYNC_SPEECH) {
        *result = NULL;
        log_line("Win7 SpeechSynthesizer async GetResults returned null stream");
        return S_OK;
    }
    if (object && object->kind == FAKE_ASYNC_AUDIO_GRAPH) {
        *result = &audio_graph_create_result;
        fake_add_ref(&audio_graph_create_result);
        log_line("Win32 AudioGraph async result reports unavailable audio");
        return S_OK;
    }
    if (object && object->kind == FAKE_ASYNC_BOOL) {
        *(BYTE *)result = TRUE;
        return S_OK;
    }
    if (object && object->kind == FAKE_ASYNC_UINT) {
        *(UINT *)result = 0;
        return S_OK;
    }
    if (object && object->kind == FAKE_ASYNC_STORAGE_FILE) {
        if (object == &secondary_storage_async_operation) {
            if (!secondary_storage_available) {
                *result = NULL;
            } else {
                *result = &secondary_storage_file;
                fake_add_ref(&secondary_storage_file);
            }
        } else if (!export_file_selected) {
            *result = NULL;
        } else {
            *result = &storage_file;
            fake_add_ref(&storage_file);
        }
        return S_OK;
    }
    *result = &listing_information;
    fake_add_ref(&listing_information);
    log_line("CurrentApp listing GetResults returned ListingInformation");
    return S_OK;
}

static HRESULT WINAPI fake_listing_string(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t value[] = L"Minecraft";
    (void)object;
    if (!result) return E_POINTER;
    log_line("ListingInformation returned non-null string");
    return WindowsCreateString(value, ARRAYSIZE(value) - 1, result);
}

static HRESULT WINAPI fake_listing_age_rating(
    FakeInspectable *object, UINT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_listing_products(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &product_listings;
    fake_add_ref(&product_listings);
    log_line(object && object->kind == FAKE_LICENSE
        ? "LicenseInformation ProductLicenses returned empty map"
        : "ListingInformation ProductListings returned empty map");
    return S_OK;
}

static HRESULT WINAPI fake_map_lookup(
    FakeInspectable *object, HSTRING key, void **result)
{
    (void)object;
    (void)key;
    if (result) *result = NULL;
    return E_BOUNDS;
}

static HRESULT WINAPI fake_map_size(
    FakeInspectable *object, UINT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_map_has_key(
    FakeInspectable *object, HSTRING key, BYTE *result)
{
    (void)object;
    (void)key;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_map_split(
    FakeInspectable *object, void **first, void **second)
{
    (void)object;
    if (first) *first = NULL;
    if (second) *second = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_api_type_present(
    FakeInspectable *object, HSTRING type_name, BYTE *result)
{
    (void)object;
    (void)type_name;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_api_member_present(
    FakeInspectable *object, HSTRING type_name, HSTRING member_name,
    BYTE *result)
{
    (void)object;
    (void)type_name;
    (void)member_name;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_api_method_present_with_arity(
    FakeInspectable *object, HSTRING type_name, HSTRING method_name,
    UINT32 arity, BYTE *result)
{
    (void)object;
    (void)type_name;
    (void)method_name;
    (void)arity;
    if (!result) return E_POINTER;
    *result = FALSE;
    log_line("ApiInformation method query answered false");
    return S_OK;
}

static HRESULT WINAPI fake_api_contract_major(
    FakeInspectable *object, HSTRING contract_name, WORD major,
    BYTE *result)
{
    (void)object;
    (void)contract_name;
    (void)major;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_api_contract_minor(
    FakeInspectable *object, HSTRING contract_name, WORD major, WORD minor,
    BYTE *result)
{
    (void)object;
    (void)contract_name;
    (void)major;
    (void)minor;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_language_get_at(
    FakeInspectable *object, UINT32 index, HSTRING *result)
{
    static const wchar_t language[] = L"en-US";
    static const wchar_t currency[] = L"USD";
    const wchar_t *value;
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    if (index != 0) return E_BOUNDS;
    value = object && object->kind == FAKE_CURRENCY_VECTOR
        ? currency : language;
    return WindowsCreateString(
        value, (UINT32)lstrlenW(value), result);
}

static HRESULT WINAPI fake_language_get_size(
    FakeInspectable *object, UINT32 *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 1;
    return S_OK;
}

static HRESULT WINAPI fake_language_index_of(
    FakeInspectable *object, HSTRING value, UINT32 *index, BYTE *found)
{
    (void)object;
    (void)value;
    if (!index || !found) return E_POINTER;
    *index = 0;
    *found = TRUE;
    return S_OK;
}

static HRESULT WINAPI fake_language_get_many(
    FakeInspectable *object, UINT32 start, UINT32 capacity,
    HSTRING *values, UINT32 *actual)
{
    HRESULT result;
    (void)object;
    if (!actual || (capacity && !values)) return E_POINTER;
    *actual = 0;
    if (start != 0 || !capacity) return S_OK;
    result = fake_language_get_at(&language_vector, 0, values);
    if (SUCCEEDED(result)) *actual = 1;
    return result;
}

static HRESULT WINAPI fake_application_get_languages(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &language_vector;
    fake_add_ref(&language_vector);
    log_line("ApplicationLanguages returned en-US vector");
    return S_OK;
}

static HRESULT WINAPI fake_application_get_primary(
    FakeInspectable *object, HSTRING *result)
{
    return fake_language_get_at(object, 0, result);
}

static HRESULT WINAPI fake_application_put_primary(
    FakeInspectable *object, HSTRING value)
{
    (void)object;
    (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_geographic_activate(
    FakeInspectable *factory, void **result)
{
    (void)factory;
    if (!result) return E_POINTER;
    *result = &geographic_region;
    fake_add_ref(&geographic_region);
    log_line("GeographicRegion activated as en-US/US");
    return S_OK;
}

static HRESULT fake_geographic_string(
    const wchar_t *value, HSTRING *result)
{
    if (!result) return E_POINTER;
    return WindowsCreateString(value, (UINT32)lstrlenW(value), result);
}

static HRESULT WINAPI fake_geographic_code(
    FakeInspectable *object, HSTRING *result)
{
    (void)object;
    return fake_geographic_string(L"US", result);
}

static HRESULT WINAPI fake_geographic_code_three_letter(
    FakeInspectable *object, HSTRING *result)
{
    (void)object;
    return fake_geographic_string(L"USA", result);
}

static HRESULT WINAPI fake_geographic_code_three_digit(
    FakeInspectable *object, HSTRING *result)
{
    (void)object;
    return fake_geographic_string(L"840", result);
}

static HRESULT WINAPI fake_geographic_display_name(
    FakeInspectable *object, HSTRING *result)
{
    (void)object;
    return fake_geographic_string(L"United States", result);
}

static HRESULT WINAPI fake_geographic_currencies(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &currency_vector;
    fake_add_ref(&currency_vector);
    return S_OK;
}

static HRESULT WINAPI fake_xaml_get_current(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &xaml_application;
    fake_add_ref(&xaml_application);
    log_line("XAML Application.Current redirected to HWND host");
    return S_OK;
}

typedef struct Win32Float2 {
    float x;
    float y;
} Win32Float2;

static HRESULT WINAPI fake_xaml_window_current(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &xaml_window;
    fake_add_ref(&xaml_window);
    log_line("XAML Window.Current redirected to HWND window");
    return S_OK;
}

static HRESULT WINAPI fake_xaml_window_content(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &xaml_size_source;
    fake_add_ref((FakeInspectable *)&xaml_size_source);
    log_line("XAML Window.Content redirected to HWND size source");
    return S_OK;
}

static HRESULT WINAPI fake_xaml_window_actual_size(
    FakeInspectable *object, Win32Float2 *result)
{
    static LONG log_count;
    RECT client;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float logical_width;
    float logical_height;
    void *platform = NULL;
    char line[256];

    (void)object;
    if (!result) return E_POINTER;
    if (!game_window || !GetClientRect(game_window, &client)) {
        result->x = 1280.0f;
        result->y = 720.0f;
        log_line("XAML UIElement.ActualSize used 1280x720 fallback");
        return S_OK;
    }

    if (game_app_main &&
        !IsBadReadPtr((BYTE *)game_app_main + 8, sizeof(void *))) {
        platform = *(void **)((BYTE *)game_app_main + 4);
    }
    if (platform && !IsBadReadPtr((BYTE *)platform + 0x20c, 1)) {
        float candidate_x = *(float *)((BYTE *)platform + 0x204);
        float candidate_y = *(float *)((BYTE *)platform + 0x208);
        if (candidate_x > 0.0001f && candidate_x < 100.0f) {
            scale_x = candidate_x;
        }
        if (candidate_y > 0.0001f && candidate_y < 100.0f) {
            scale_y = candidate_y;
        }
    }

    logical_width = (float)(client.right - client.left) / scale_x;
    logical_height = (float)(client.bottom - client.top) / scale_y;
    result->x = logical_width;
    result->y = logical_height;

    if (InterlockedIncrement(&log_count) <= 4) {
        wsprintfA(line,
                  "XAML UIElement.ActualSize returned client=%dx%d logical=%d/%d scale=%d/%d",
                  client.right - client.left,
                  client.bottom - client.top,
                  (int)logical_width,
                  (int)logical_height,
                  (int)(scale_x * 1000.0f),
                  (int)(scale_y * 1000.0f));
        log_line(line);
    }
    return S_OK;
}

static HRESULT WINAPI fake_xaml_rasterization_scale(
    FakeInspectable *object, float *result)
{
    static LONG log_count;
    (void)object;
    if (!result) return E_POINTER;
    *result = 1.0f;
    if (InterlockedIncrement(&log_count) <= 4) {
        log_line("XAML size-source rasterization scale returned 1.0");
    }
    return S_OK;
}

static HRESULT WINAPI fake_display_get_for_current_view(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &display_information;
    fake_add_ref(&display_information);
#ifdef WIN32CRAFT_ARM32
    {
        static volatile LONG display_log_count;
        if (InterlockedIncrement(&display_log_count) <= 4)
            log_line("DisplayInformation.GetForCurrentView redirected to HWND display");
    }
#else
    log_line("DisplayInformation.GetForCurrentView redirected to HWND display");
#endif
    return S_OK;
}

static HRESULT WINAPI fake_display_orientation(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 1; /* DisplayOrientations::Landscape */
    return S_OK;
}

static HRESULT WINAPI fake_display_resolution_scale(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 100;
    return S_OK;
}

static HRESULT WINAPI fake_display_stereo_enabled(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_display_add_event(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    (void)handler;
    if (!token) return E_POINTER;
    *token = 0;
    return S_OK;
}

static HRESULT WINAPI fake_display_remove_event(
    FakeInspectable *object, LONGLONG token)
{
    (void)object;
    (void)token;
    return S_OK;
}

static HRESULT WINAPI fake_display_null_async(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_current(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &application_view;
    fake_add_ref(&application_view);
#ifdef WIN32CRAFT_ARM32
    {
        static volatile LONG view_log_count;
        if (InterlockedIncrement(&view_log_count) <= 4)
            log_line("ApplicationView.GetForCurrentView returned HWND view");
    }
#else
    log_line("ApplicationView.GetForCurrentView returned HWND view");
#endif
    return S_OK;
}

static HRESULT WINAPI fake_application_view_get_bool(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_get_fullscreen(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = win32_fullscreen_active ? TRUE : FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_get_int(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_put_value(
    FakeInspectable *object, ULONG_PTR value)
{
    (void)object;
    (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_get_title(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t title[] = L"Win32Craft";
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(title, ARRAYSIZE(title) - 1, result);
}

static HRESULT WINAPI fake_application_view_add_event(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    (void)handler;
    if (!token) return E_POINTER;
    *token = 0;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_remove_event(
    FakeInspectable *object, LONGLONG token)
{
    (void)object;
    (void)token;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_null_object(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_try(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = TRUE;
    return S_OK;
}

static HRESULT WINAPI application_view_enter_fullscreen_116(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    win32_set_fullscreen_mode(NULL, 1);
    *result = win32_fullscreen_active ? TRUE : FALSE;
    return S_OK;
}

static HRESULT WINAPI application_view_exit_fullscreen_116(
    FakeInspectable *object)
{
    (void)object;
    win32_set_fullscreen_mode(NULL, 0);
    return S_OK;
}

static HRESULT WINAPI fake_application_view_noop(FakeInspectable *object)
{
    (void)object;
    return S_OK;
}

#ifdef WIN32CRAFT_ARM32
/* ARM32 AAPCS-VFP: Windows.Foundation.Size is {float Width, float Height}, a homogeneous floating-point aggregate that
 * the game passes in s0/s1.  The x86 declaration (a 64-bit integer) would be read from r2:r3, and the BYTE* result
 * that follows would be fetched from the stack instead of r1 (a write through a garbage pointer).  Verified on the
 * ARM exe: IApplicationView3 slot 13 (0x34) is called as `vmov s0,s17; vmov s1,s16; r1=&bool; blx` (0x587458). */
static HRESULT WINAPI fake_application_view_try_resize(
    FakeInspectable *object, Win32Float2 size, BYTE *result)
{
    static volatile LONG logged;
    (void)object;
    if (!result) return E_POINTER;
    if (InterlockedIncrement(&logged) <= 8) {
        char line[96];
        wsprintfA(line, "ApplicationView.TryResizeView(%d x %d) accepted (host window unchanged)",
                  (int)size.x, (int)size.y);
        log_line(line);
    }
    *result = TRUE;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_set_size(
    FakeInspectable *object, Win32Float2 size)
{
    (void)object;
    (void)size;
    return S_OK;
}
#else
static HRESULT WINAPI fake_application_view_try_resize(
    FakeInspectable *object, LONGLONG size, BYTE *result)
{
    (void)object;
    (void)size;
    if (!result) return E_POINTER;
    *result = TRUE;
    return S_OK;
}

static HRESULT WINAPI fake_application_view_set_size(
    FakeInspectable *object, LONGLONG size)
{
    (void)object;
    (void)size;
    return S_OK;
}
#endif

static HRESULT WINAPI fake_display_dpi_x(
    FakeInspectable *object, float *result)
{
    static LONG log_count;
    (void)object;
    if (!result) return E_POINTER;
    *result = 96.0f;
    if (InterlockedIncrement(&log_count) <= 4) {
        log_line("DisplayInformation RawDpiX returned 96.0");
    }
    return S_OK;
}

static HRESULT WINAPI fake_display_dpi_y(
    FakeInspectable *object, float *result)
{
    static LONG log_count;
    (void)object;
    if (!result) return E_POINTER;
    *result = 96.0f;
    if (InterlockedIncrement(&log_count) <= 4) {
        log_line("DisplayInformation RawDpiY returned 96.0");
    }
    return S_OK;
}

static HRESULT WINAPI fake_xaml_noop(FakeInspectable *object)
{
    (void)object;
    log_line("XAML Application lifecycle call ignored by HWND host");
    return S_OK;
}

static HRESULT WINAPI fake_win32_application_exit(FakeInspectable *object)
{
    (void)object;
    if (!game_window) return S_OK;
    if (!PostMessageW(game_window, WM_CLOSE, 0, 0)) {
        DWORD error = GetLastError();
        char line[96];
        wsprintfA(line, "WinRT Application.Exit PostMessage failed: %lu",
                  (unsigned long)error);
        log_line(line);
        return error ? (HRESULT)(0x80070000u | (error & 0xffffu)) : E_FAIL;
    }
    log_line("WinRT Application.Exit redirected to HWND WM_CLOSE");
    return S_OK;
}

static HRESULT WINAPI fake_core_window_dispatcher(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &win32_core_dispatcher;
    fake_add_ref(&win32_core_dispatcher);
#ifdef WIN32CRAFT_ARM32
    {
        /* the game calls get_Dispatcher for every RunAsync wrapper (helper 0x5841d8): log only the first calls */
        static volatile LONG dispatcher_log_count;
        LONG dispatcher_calls = InterlockedIncrement(&dispatcher_log_count);
        if (dispatcher_calls <= 4) {
            log_line("CoreWindow.Dispatcher redirected to HWND dispatcher");
        } else if (dispatcher_calls == 5) {
            log_line("CoreWindow.Dispatcher redirected to HWND dispatcher (further calls are not logged)");
        }
    }
#else
    log_line("CoreWindow.Dispatcher redirected to HWND dispatcher");
#endif
    return S_OK;
}

#ifdef WIN32CRAFT_ARM32
static void arm32_gfx_get_backbuffer_size(UINT *width, UINT *height);   /* gfx_bootstrap helper, defined further down */
#endif
static HRESULT WINAPI fake_core_window_bounds(
    FakeInspectable *object, float *result)
{
    RECT client;
    (void)object;
    if (!result) return E_POINTER;
    result[0] = 0.0f;
    result[1] = 0.0f;
    result[2] = 1280.0f;
    result[3] = 720.0f;
    if (game_window && GetClientRect(game_window, &client)) {
        result[2] = (float)(client.right - client.left);
        result[3] = (float)(client.bottom - client.top);
    }
#ifdef WIN32CRAFT_ARM32
    {
        /* AppMain::init sizes the game from Bounds: it must equal the (feature-level clamped) swap-chain buffer, which
         * is also what activate_game_app_128 / resize_game_window write to platform+0x330/+0x334 */
        UINT buffer_width = 0;
        UINT buffer_height = 0;
        arm32_gfx_get_backbuffer_size(&buffer_width, &buffer_height);
        if (buffer_width && buffer_height) {
            result[2] = (float)buffer_width;
            result[3] = (float)buffer_height;
        }
    }
#endif
    return S_OK;
}

static HRESULT WINAPI fake_core_window_null_object(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_core_window_get_int(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_core_window_put_int(
    FakeInspectable *object, ULONG_PTR value)
{
    (void)object;
    (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_core_window_get_bool(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = TRUE;
    return S_OK;
}

static HRESULT WINAPI fake_core_window_pointer_position(
    FakeInspectable *object, float *result)
{
    (void)object;
    if (!result) return E_POINTER;
    result[0] = 0.0f;
    result[1] = 0.0f;
    return S_OK;
}

static HRESULT WINAPI core_window_pointer_cursor_put_116(
    FakeInspectable *object, void *value);

static HRESULT WINAPI fake_core_window_noop(FakeInspectable *object)
{
    (void)object;
    return S_OK;
}

static HRESULT WINAPI fake_core_window_close(FakeInspectable *object)
{
    (void)object;
    if (!game_window) return S_OK;
    if (!PostMessageW(game_window, WM_CLOSE, 0, 0)) {
        DWORD error = GetLastError();
        char line[96];
        wsprintfA(line, "CoreWindow.Close PostMessage failed: %lu",
                  (unsigned long)error);
        log_line(line);
        return error ? (HRESULT)(0x80070000u | (error & 0xffffu)) : E_FAIL;
    }
    log_line("CoreWindow.Close redirected to HWND WM_CLOSE");
    return S_OK;
}

static HRESULT WINAPI fake_core_window_event_add_116(
    void **storage, void *handler, LONGLONG *token, const char *label)
{
    void **vtable;
    if (!token) return E_POINTER;
    *token = 1;
    *storage = handler;
    vtable = handler ? *(void ***)handler : NULL;
    if (vtable && vtable[1]) ((ULONG (WINAPI *)(void *))vtable[1])(handler);
    log_pointer(label, handler);
    if (vtable) log_pointer("1.16 event invoke", vtable[3]);
    return S_OK;
}

static HRESULT WINAPI fake_core_window_add_activated_116(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    return fake_core_window_event_add_116(
        &core_window_activated_handler_116, handler, token,
        "1.16 CoreWindow.Activated handler");
}

static HRESULT WINAPI fake_core_window_add_visibility_116(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    return fake_core_window_event_add_116(
        &core_window_visibility_handler_116, handler, token,
        "1.16 CoreWindow.VisibilityChanged handler");
}

#ifdef WIN32CRAFT_ARM32
/*
 * ARM32: win32_host_116.inl is not compiled, but its functions are still referenced from shared code and from the
 * fake CoreWindow ICoreWindow vtable (win32_core_window_vtable).  That vtable is the LIVE CoreWindow of the 1.2.8 path:
 * the game's HID constructor calls add_CharacterReceived/KeyDown/KeyUp/PointerMoved/Pressed/Released/WheelChanged
 * (slots 28/34/36/44/46/48/52) and put_PointerCursor(0) (slot 15) - see facts appmain.corewindow_slots_used.  A failing
 * HRESULT there would be rethrown by C++/CX as Platform::COMException, so the event adders keep the x86 1.2.8
 * behaviour (AddRef + remember the handler + token 1, via fake_core_window_event_add_116) and the cursor accessors
 * succeed without side effects.  The remaining 1.16-only helpers are inert.
 */
static HRESULT WINAPI core_pointer_add_moved_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_pointer_moved_handler_116, handler, token, "CoreWindow.PointerMoved handler"); }
static HRESULT WINAPI core_pointer_add_entered_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_pointer_entered_handler_116, handler, token, "CoreWindow.PointerEntered handler"); }
static HRESULT WINAPI core_pointer_add_pressed_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_pointer_pressed_handler_116, handler, token, "CoreWindow.PointerPressed handler"); }
static HRESULT WINAPI core_pointer_add_released_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_pointer_released_handler_116, handler, token, "CoreWindow.PointerReleased handler"); }
static HRESULT WINAPI core_pointer_add_wheel_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_pointer_wheel_handler_116, handler, token, "CoreWindow.PointerWheelChanged handler"); }
static HRESULT WINAPI core_character_add_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_character_handler_116, handler, token, "CoreWindow.CharacterReceived handler"); }
static HRESULT WINAPI core_key_down_add_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_key_down_handler_116, handler, token, "CoreWindow.KeyDown handler"); }
static HRESULT WINAPI core_key_up_add_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_key_up_handler_116, handler, token, "CoreWindow.KeyUp handler"); }
static HRESULT WINAPI core_size_add_116(void *object, void *handler, LONGLONG *token)
{ (void)object; return fake_core_window_event_add_116(&core_window_size_handler_116, handler, token, "CoreWindow.SizeChanged handler"); }

static HRESULT WINAPI core_window_pointer_cursor_get_116(void *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI core_window_pointer_cursor_put_116(FakeInspectable *object, void *value)
{
    char line[96];
    (void)object;
    wsprintfA(line, "CoreWindow.PointerCursor put %p ignored", value);
    log_line(line);
    return S_OK;
}

static BOOL activation_116(LPCWSTR class_name, const GUID *iid, void **result)
{ (void)class_name; (void)iid; (void)result; return FALSE; }
static void refresh_scene_mouse_116(void)
{
}
static void install_clear_view_116(ID3D11DeviceContext *context)
{ (void)context; }
#endif

static HRESULT WINAPI fake_core_window_event_state_116(
    FakeInspectable *object, INT *result)
{ (void)object; if (!result) return E_POINTER; *result = 0; return S_OK; }

static HRESULT WINAPI fake_core_window_event_visible_116(
    FakeInspectable *object, BYTE *result)
{ (void)object; if (!result) return E_POINTER; *result = TRUE; return S_OK; }

static HRESULT WINAPI fake_core_window_event_handled_116(
    FakeInspectable *object, BYTE value)
{ (void)object; (void)value; return S_OK; }

static HRESULT WINAPI fake_core_window_activate_116(FakeInspectable *object)
{
    typedef HRESULT (WINAPI *InvokeFn)(void *, void *, void *);
    void **table;
    HRESULT result = S_OK;
    (void)object;
    log_line("1.16 CoreWindow.Activate");
    if (core_window_activated_handler_116) {
        table = *(void ***)core_window_activated_handler_116;
        result = ((InvokeFn)table[3])(
            core_window_activated_handler_116, &win32_core_window,
            &core_window_activated_args_116);
        log_hresult("1.16 CoreWindow.Activated event", result);
    }
    if (SUCCEEDED(result) && core_window_visibility_handler_116) {
        table = *(void ***)core_window_visibility_handler_116;
        result = ((InvokeFn)table[3])(
            core_window_visibility_handler_116, &win32_core_window,
            &core_window_visibility_args_116);
        log_hresult("1.16 CoreWindow.VisibilityChanged event", result);
    }
    return result;
}

static const void *core_window_activated_args_table_116[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_core_window_event_state_116, fake_core_window_event_handled_116
};

static const void *core_window_visibility_args_table_116[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_core_window_event_visible_116, fake_core_window_event_handled_116
};

static HRESULT WINAPI fake_core_window_key_state(
    FakeInspectable *object, INT key, INT *result)
{
    (void)object;
    (void)key;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_core_window_current(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &win32_core_window;
    fake_add_ref(&win32_core_window);
    log_line("CoreWindow.GetForCurrentThread redirected to HWND window");
    return S_OK;
}

static HRESULT WINAPI fake_core_dispatcher_thread_access(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = TRUE;
    return S_OK;
}

static void drain_dispatcher_queue(void);

static HRESULT WINAPI fake_core_dispatcher_process_events(
    FakeInspectable *object, INT options)
{
    MSG message;
    unsigned messages = 0;
    static unsigned logged_messages_116;
    (void)object;
    (void)options;
    /* FrameworkView::Run relies on CoreDispatcher to consume the native
     * window queue.  The former desktop stub only ran async callbacks, so
     * every keyboard and mouse message remained undispatched forever. */
    while (messages++ < (host_is_116 ? 256U : 1U) &&
           PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        if (logged_messages_116 < 32) {
            char line[96];
            wsprintfA(line, "1.16 Win32 message: hwnd=%p id=0x%04X",
                      message.hwnd, (unsigned)message.message);
            log_line(line);
            ++logged_messages_116;
        }
        if (message.message != WM_QUIT) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    drain_dispatcher_queue();
    /* Presentation belongs after AppMain's update/render, not in the
     * input pump. The 1.16 frame callsite supplies that boundary. */
    return S_OK;
}

typedef HRESULT (WINAPI *DispatcherInvokeFn)(void *);
typedef HRESULT (WINAPI *IdleDispatcherInvokeFn)(void *, void *);
typedef ULONG (WINAPI *DispatcherAddRefFn)(void *);
typedef ULONG (WINAPI *DispatcherReleaseFn)(void *);

static void lock_dispatcher_queue(void)
{
    while (InterlockedExchange(&dispatcher_queue_lock, 1) != 0) {
        Sleep(0);
    }
}

static void unlock_dispatcher_queue(void)
{
    InterlockedExchange(&dispatcher_queue_lock, 0);
}

static HRESULT queue_dispatcher_handler(
    void *handler, BOOL idle, void **operation)
{
    void **vtable;
    unsigned next;
    char line[192];

    if (!operation) return E_POINTER;
    *operation = NULL;
    if (!handler || IsBadReadPtr(handler, sizeof(void *))) return E_POINTER;
    vtable = *(void ***)handler;
    if (!vtable || IsBadReadPtr(vtable, 4 * sizeof(void *)) ||
        !vtable[1] || !vtable[2] || !vtable[3]) {
        return E_POINTER;
    }

    ((DispatcherAddRefFn)vtable[1])(handler);
    lock_dispatcher_queue();
    next = (dispatcher_queue_write + 1) % WIN32_DISPATCHER_QUEUE_CAPACITY;
    if (next == dispatcher_queue_read) {
        unlock_dispatcher_queue();
        ((DispatcherReleaseFn)vtable[2])(handler);
        log_line("CoreDispatcher queue full; handler rejected");
        return E_FAIL;
    }
    dispatcher_queue[dispatcher_queue_write].handler = handler;
    dispatcher_queue[dispatcher_queue_write].idle = idle;
    dispatcher_queue_write = next;
    unlock_dispatcher_queue();

    /* The existing 1.1.5 C++/CX path already tolerated S_OK with a NULL
     * operation from the old stub.  Preserve that ABI behavior while now
     * actually queueing the delegate; returning the Store async-operation
     * shim here would expose the wrong IAsyncAction GetResults signature. */
    *operation = NULL;
    if (InterlockedIncrement(&dispatcher_run_count) <= 48) {
        wsprintfA(line,
            "CoreDispatcher.%s queued handler=%p invoke=%p depth=%u",
            idle ? "RunIdleAsync" : "RunAsync",
            handler, vtable[3],
            (dispatcher_queue_write + WIN32_DISPATCHER_QUEUE_CAPACITY -
             dispatcher_queue_read) % WIN32_DISPATCHER_QUEUE_CAPACITY);
        log_line(line);
    }
    return S_OK;
}

static HRESULT WINAPI fake_core_dispatcher_run_async(
    FakeInspectable *object, INT priority, void *handler, void **operation)
{
    (void)object;
    (void)priority;
    return queue_dispatcher_handler(handler, FALSE, operation);
}

static HRESULT WINAPI fake_core_dispatcher_run_idle(
    FakeInspectable *object, void *handler, void **operation)
{
    (void)object;
    return queue_dispatcher_handler(handler, TRUE, operation);
}

static void drain_dispatcher_queue(void)
{
    static unsigned logged;
    Win32DispatcherItem item;
    void **vtable;
    HRESULT result;
    char line[192];

    for (;;) {
        ZeroMemory(&item, sizeof(item));
        lock_dispatcher_queue();
        if (dispatcher_queue_read == dispatcher_queue_write) {
            unlock_dispatcher_queue();
            break;
        }
        item = dispatcher_queue[dispatcher_queue_read];
        dispatcher_queue[dispatcher_queue_read].handler = NULL;
        dispatcher_queue_read =
            (dispatcher_queue_read + 1) % WIN32_DISPATCHER_QUEUE_CAPACITY;
        unlock_dispatcher_queue();

        vtable = item.handler ? *(void ***)item.handler : NULL;
        result = E_POINTER;
        if (vtable && !IsBadReadPtr(vtable, 4 * sizeof(void *)) &&
            vtable[3]) {
            if (item.idle) {
                result = ((IdleDispatcherInvokeFn)vtable[3])(
                    item.handler, NULL);
            } else {
                result = ((DispatcherInvokeFn)vtable[3])(item.handler);
            }
        }
        if (logged < 64 || FAILED(result)) {
            wsprintfA(line,
                "CoreDispatcher.%s invoked handler=%p invoke=%p result=0x%08lX relative=%d guard=%d",
                item.idle ? "RunIdleAsync" : "RunAsync",
                item.handler, vtable ? vtable[3] : NULL,
                (unsigned long)result,
                host_relative_requested, initial_menu_cursor_guard);
            log_line(line);
            ++logged;
        }
        if (vtable && vtable[2]) {
            ((DispatcherReleaseFn)vtable[2])(item.handler);
        }
    }
}

static HRESULT WINAPI fake_mouse_device_current(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &mouse_device;
    fake_add_ref(&mouse_device);
    log_line("MouseDevice.GetForCurrentView redirected to HWND mouse");
    return S_OK;
}

static HRESULT WINAPI fake_mouse_device_add_moved(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    if (!token) return E_POINTER;
    *token = 1;
    if (host_is_116 && object == &mouse_device) {
        void **vtable = handler ? *(void ***)handler : NULL;
        if (mouse_moved_handler_count_116 < ARRAYSIZE(mouse_moved_handlers_116))
            mouse_moved_handlers_116[mouse_moved_handler_count_116++] = handler;
        if (vtable && vtable[1])
            ((ULONG (WINAPI *)(void *))vtable[1])(handler);
        log_pointer("1.16 MouseDevice.MouseMoved handler", handler);
    }
    return S_OK;
}

static HRESULT WINAPI fake_mouse_device_remove_moved(
    FakeInspectable *object, LONGLONG token)
{
    (void)object;
    (void)token;
    return S_OK;
}

static HRESULT fake_thread_pool_result(void **operation)
{
    if (!operation) return E_POINTER;
    *operation = &async_operation;
    fake_add_ref(&async_operation);
    return S_OK;
}

typedef struct Win32WorkItem {
    void *handler;
} Win32WorkItem;
typedef HRESULT (WINAPI *WorkItemInvokeFn)(void *, void *);
typedef ULONG (WINAPI *DelegateAddRefFn)(void *);
typedef ULONG (WINAPI *DelegateReleaseFn)(void *);

static DWORD WINAPI win32_work_item_thread(void *parameter)
{
    Win32WorkItem *item = (Win32WorkItem *)parameter;
    void *handler = item ? item->handler : NULL;
    void **vtable = handler ? *(void ***)handler : NULL;

    if (item) HeapFree(GetProcessHeap(), 0, item);
    /*
     * Run after the factory call has returned.  Several MCPE work items
     * capture members which are installed immediately after RunAsync.
     */
    Sleep(10);
    if (vtable && vtable[3]) {
        ((WorkItemInvokeFn)vtable[3])(handler, &async_operation);
    }
    if (vtable && vtable[2]) {
        ((DispatcherReleaseFn)vtable[2])(handler);
    }
    return 0;
}

static HRESULT fake_thread_pool_queue(void *handler, void **operation)
{
    typedef HANDLE (WINAPI *CreateThreadFn)(
        void *, SIZE_T, DWORD (WINAPI *)(void *), void *, DWORD, DWORD *);
    static CreateThreadFn create_thread;
    Win32WorkItem *item;
    HMODULE kernel32;
    HANDLE thread;

    if (!handler || IsBadReadPtr(handler, sizeof(void *)) ||
        IsBadReadPtr(*(void ***)handler, 4 * sizeof(void *))) {
        return E_POINTER;
    }
    if (!create_thread) {
        kernel32 = GetModuleHandleW(L"kernel32.dll");
        create_thread = kernel32
            ? (CreateThreadFn)GetProcAddress(kernel32, "CreateThread")
            : NULL;
    }
    if (!create_thread) return E_FAIL;
    item = (Win32WorkItem *)HeapAlloc(
        GetProcessHeap(), 0, sizeof(*item));
    if (!item) return E_OUTOFMEMORY;
    item->handler = handler;
    ((DelegateAddRefFn)(*(void ***)handler)[1])(handler);
    thread = create_thread(
        NULL, 0, win32_work_item_thread, item, 0, NULL);
    if (!thread) {
        ((DelegateReleaseFn)(*(void ***)handler)[2])(handler);
        HeapFree(GetProcessHeap(), 0, item);
        return E_FAIL;
    }
    CloseHandle(thread);
    return fake_thread_pool_result(operation);
}

static HRESULT WINAPI fake_thread_pool_run(
    FakeInspectable *object, void *handler, void **operation)
{
    (void)object;
    return fake_thread_pool_queue(handler, operation);
}

static HRESULT WINAPI fake_thread_pool_run_priority(
    FakeInspectable *object, void *handler, INT priority, void **operation)
{
    (void)object;
    (void)priority;
    return fake_thread_pool_queue(handler, operation);
}

static HRESULT WINAPI fake_thread_pool_run_options(
    FakeInspectable *object, void *handler, INT priority, INT options,
    void **operation)
{
    (void)object;
    (void)priority;
    (void)options;
    return fake_thread_pool_queue(handler, operation);
}

/* Full UniversalApiContract 1.0 ICoreWindow layout. The dispatcher is backed
 * by the HWND queue; CoreWindow events are accepted as inert registrations
 * because the Win32 message loop feeds the game input/lifecycle paths. */
static const void *win32_core_window_vtable[58] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_core_window_null_object,
    fake_core_window_bounds,
    fake_core_window_null_object,
    fake_core_window_dispatcher,
    fake_core_window_get_int, fake_core_window_put_int,
    fake_core_window_get_bool, fake_core_window_put_int,
    core_window_pointer_cursor_get_116, core_window_pointer_cursor_put_116,
    fake_core_window_pointer_position,
    fake_core_window_get_bool,
    fake_core_window_activate_116, fake_core_window_close,
    fake_core_window_key_state, fake_core_window_key_state,
    fake_core_window_noop, fake_core_window_noop,
    fake_core_window_add_activated_116, fake_mouse_device_remove_moved, /* 24 */
    fake_mouse_device_add_moved, fake_mouse_device_remove_moved,       /* 26 */
    core_character_add_116, fake_mouse_device_remove_moved,            /* 28 */
    fake_mouse_device_add_moved, fake_mouse_device_remove_moved,       /* 30 */
    fake_mouse_device_add_moved, fake_mouse_device_remove_moved,       /* 32 */
    core_key_down_add_116, fake_mouse_device_remove_moved,             /* 34 */
    core_key_up_add_116, fake_mouse_device_remove_moved,               /* 36 */
    fake_mouse_device_add_moved, fake_mouse_device_remove_moved,       /* 38 */
    core_pointer_add_entered_116, fake_mouse_device_remove_moved,      /* 40 */
    fake_mouse_device_add_moved, fake_mouse_device_remove_moved,       /* 42 */
    core_pointer_add_moved_116, fake_mouse_device_remove_moved,        /* 44 */
    core_pointer_add_pressed_116, fake_mouse_device_remove_moved,      /* 46 */
    core_pointer_add_released_116, fake_mouse_device_remove_moved,     /* 48 */
    fake_mouse_device_add_moved, fake_mouse_device_remove_moved,       /* 50 */
    core_pointer_add_wheel_116, fake_mouse_device_remove_moved,        /* 52 */
    core_size_add_116, fake_mouse_device_remove_moved,                 /* 54 */
    fake_core_window_add_visibility_116, fake_mouse_device_remove_moved/* 56 */
};

static const void *core_window_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_core_window_current
};

static const void *win32_core_dispatcher_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_core_dispatcher_thread_access,
    fake_core_dispatcher_process_events,
    fake_core_dispatcher_run_async,
    fake_core_dispatcher_run_idle
};

static const void *mouse_device_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_mouse_device_current
};

static const void *mouse_device_vtable[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_mouse_device_add_moved, fake_mouse_device_remove_moved
};

static const void *thread_pool_factory_vtable[9] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_thread_pool_run,
    fake_thread_pool_run_priority,
    fake_thread_pool_run_options
};

/* Wine currently exposes CoreTextServicesManager but CreateEditContext is a
 * stub. Minecraft treats that failure as a C++/CX exception during HID setup,
 * before the desktop input bridge can take over. The HWND host only needs an
 * inert edit context: keyboard and text events are queued directly below. */
static HRESULT WINAPI fake_core_text_get_for_current_view(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &core_text_manager;
    fake_add_ref(&core_text_manager);
    log_line("CoreTextServicesManager.GetForCurrentView returned HWND manager");
    return S_OK;
}

static HRESULT WINAPI fake_core_text_create_edit_context(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &core_text_edit_context;
    fake_add_ref(&core_text_edit_context);
    log_line("CoreTextServicesManager.CreateEditContext returned HWND context");
    return S_OK;
}

static HRESULT WINAPI fake_core_text_null_object(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_core_text_get_name(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t empty[] = L"";
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(empty, 0, result);
}

static HRESULT WINAPI fake_core_text_get_value(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_core_text_put_value(
    FakeInspectable *object, ULONG_PTR value)
{
    (void)object;
    (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_core_text_add_event(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    static volatile LONG next_token;
    (void)object;
    (void)handler;
    if (!token) return E_POINTER;
    *token = (LONGLONG)InterlockedIncrement(&next_token);
    return S_OK;
}

static HRESULT WINAPI fake_core_text_remove_event(
    FakeInspectable *object, LONGLONG token)
{
    (void)object;
    (void)token;
    return S_OK;
}

typedef struct Win32CoreTextRange {
    INT start;
    INT end;
} Win32CoreTextRange;

static void *core_text_text_updating_handler;
static UINT core_text_update_character;
static wchar_t core_text_update_text[2048];
static UINT core_text_update_text_length;
static Win32CoreTextRange core_text_update_range;
static Win32CoreTextRange core_text_update_new_selection;
static FakeInspectable core_text_text_updating_args;

static HRESULT WINAPI core_text_update_range_get(
    void *object, Win32CoreTextRange *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = core_text_update_range;
    return S_OK;
}

static HRESULT WINAPI core_text_update_text_get(void *object, HSTRING *result)
{
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(core_text_update_text,
        core_text_update_text_length, result);
}

static HRESULT WINAPI core_text_update_selection_get(
    void *object, Win32CoreTextRange *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = core_text_update_new_selection;
    return S_OK;
}

static HRESULT WINAPI core_text_update_language_get(void *object, void **result)
{ (void)object; if (!result) return E_POINTER; *result = NULL; return S_OK; }

static HRESULT WINAPI core_text_update_result_get(void *object, INT *result)
{ (void)object; if (!result) return E_POINTER; *result = 0; return S_OK; }

static HRESULT WINAPI core_text_update_result_put(void *object, INT result)
{ (void)object; (void)result; return S_OK; }

static HRESULT WINAPI core_text_update_canceled_get(void *object, BYTE *result)
{ (void)object; if (!result) return E_POINTER; *result = FALSE; return S_OK; }

static HRESULT WINAPI core_text_update_deferral(void *object, void **result)
{ (void)object; if (!result) return E_POINTER; *result = NULL; return S_OK; }

static const void *core_text_text_updating_args_vtable[14] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    core_text_update_range_get, core_text_update_text_get,
    core_text_update_selection_get, core_text_update_language_get,
    core_text_update_result_get, core_text_update_result_put,
    core_text_update_canceled_get, core_text_update_deferral
};

#define WIN32CRAFT_CF_UNICODETEXT 13u
#define WIN32CRAFT_GMEM_MOVEABLE 0x0002u
static wchar_t core_text_paste_buffer[2048];

static HRESULT dispatch_core_text_update_text(
    const wchar_t *text, UINT text_length,
    INT selection_start, INT selection_end);

static BOOL paste_core_text_selection(void)
{
    HANDLE memory;
    const wchar_t *input;
    UINT length = 0;
    INT start = core_text_selection_start;
    INT end = core_text_selection_end;
    BOOL success = FALSE;
    if (start < 0 || end < start || end > core_text_document_length ||
        !OpenClipboard(game_window)) return FALSE;
    memory = GetClipboardData(WIN32CRAFT_CF_UNICODETEXT);
    input = memory ? (const wchar_t *)GlobalLock(memory) : NULL;
    if (input) {
        while (length < ARRAYSIZE(core_text_paste_buffer) - 1 && input[length]) {
            core_text_paste_buffer[length] = input[length];
            ++length;
        }
        core_text_paste_buffer[length] = 0;
        GlobalUnlock(memory);
        success = SUCCEEDED(dispatch_core_text_update_text(
            core_text_paste_buffer, length, start, end));
    }
    CloseClipboard();
    return success;
}

/* The 1.16 client consumes CoreText TextUpdating callbacks, so paste there
 * replaces the reported selection. Older clients consume queued TextItems
 * directly; feed clipboard characters through that same native input path. */
static BOOL paste_clipboard_as_game_text(void)
{
    HANDLE memory;
    const wchar_t *input;
    UINT length = 0, index = 0;
    BOOL success = FALSE;
    if (!text_input_active || !OpenClipboard(game_window)) return FALSE;
    memory = GetClipboardData(WIN32CRAFT_CF_UNICODETEXT);
    input = memory ? (const wchar_t *)GlobalLock(memory) : NULL;
    if (input) {
        while (length < ARRAYSIZE(core_text_paste_buffer) - 1 && input[length]) {
            core_text_paste_buffer[length] = input[length];
            ++length;
        }
        core_text_paste_buffer[length] = 0;
        GlobalUnlock(memory);
        success = TRUE;
    }
    CloseClipboard();
    if (!success) return FALSE;

    while (index < length) {
        DWORD codepoint = core_text_paste_buffer[index++];
        if (codepoint >= 0xd800 && codepoint <= 0xdbff && index < length) {
            DWORD low = core_text_paste_buffer[index];
            if (low >= 0xdc00 && low <= 0xdfff) {
                ++index;
                codepoint = 0x10000u + ((codepoint - 0xd800u) << 10) +
                    (low - 0xdc00u);
            }
        }
        if (codepoint < 0x20) continue;
        if (!queue_character_event(codepoint)) return FALSE;
    }
    return TRUE;
}

static HRESULT WINAPI core_text_text_updating_add(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    if (!token) return E_POINTER;
    core_text_text_updating_handler = handler;
    if (handler) {
        void **table = *(void ***)handler;
        if (table && table[1]) ((ULONG (WINAPI *)(void *))table[1])(handler);
    }
    *token = 1;
    log_pointer("CoreText TextUpdating handler", handler);
    return S_OK;
}

static HRESULT WINAPI core_text_text_updating_remove(
    FakeInspectable *object, LONGLONG token)
{
    void *handler = core_text_text_updating_handler;
    void **table = handler ? *(void ***)handler : NULL;
    (void)object;
    (void)token;
    core_text_text_updating_handler = NULL;
    if (table && table[2]) ((ULONG (WINAPI *)(void *))table[2])(handler);
    return S_OK;
}

static HRESULT dispatch_core_text_update_text(
    const wchar_t *text, UINT text_length,
    INT selection_start, INT selection_end)
{
    typedef HRESULT (WINAPI *InvokeFn)(void *, void *, void *);
    void *handler = core_text_text_updating_handler;
    void **table = handler ? *(void ***)handler : NULL;
    INT start = selection_start;
    INT end = selection_end;
    if (!table || !table[3] || text_length >= ARRAYSIZE(core_text_update_text))
        return (HRESULT)1;
    if (start < 0 || end < start || end > core_text_document_length) {
        start = end = core_text_document_length;
    }
    if (text_length) memcpy(core_text_update_text, text,
                            text_length * sizeof(wchar_t));
    core_text_update_text[text_length] = 0;
    core_text_update_text_length = text_length;
    core_text_update_range.start = start;
    core_text_update_range.end = end;
    core_text_update_new_selection.start =
        core_text_update_new_selection.end = start + 1;
    core_text_text_updating_args.vtable = core_text_text_updating_args_vtable;
    core_text_text_updating_args.references = 1;
    {
        HRESULT hr = ((InvokeFn)table[3])(
            handler, &core_text_edit_context,
            &core_text_text_updating_args);
        if (SUCCEEDED(hr)) {
            core_text_document_length += (INT)text_length - (end - start);
            core_text_selection_start = core_text_selection_end =
                start + (INT)text_length;
        }
        return hr;
    }
}

static HRESULT dispatch_core_text_character(
    UINT character, INT selection_start, INT selection_end)
{
    wchar_t text[1] = {(wchar_t)character};
    core_text_update_character = character;
    return dispatch_core_text_update_text(
        text, 1, selection_start, selection_end);
}

static HRESULT WINAPI fake_core_text_notify(FakeInspectable *object)
{
    (void)object;
    return S_OK;
}

static HRESULT WINAPI fake_core_text_focus_enter(FakeInspectable *object)
{
    (void)object;
    text_input_active = TRUE;
    pending_high_surrogate = 0;
    log_line("CoreText focus entered: Win32 text input activated");
    return S_OK;
}

static HRESULT WINAPI fake_core_text_focus_leave(FakeInspectable *object)
{
    (void)object;
    text_input_active = FALSE;
    pending_high_surrogate = 0;
    suppress_next_t_character = FALSE;
    log_line("CoreText focus left: Win32 text input deactivated");
    return S_OK;
}

#ifdef WIN32CRAFT_ARM32
/* ARM32 AAPCS: CoreTextRange is {int StartCaretPosition, int EndCaretPosition} (alignment 4, not an HFA), so it is passed
 * in consecutive core registers WITHOUT the even-register alignment a 64-bit integer gets:
 *   NotifyTextChanged(this r0, modifiedRange r1:r2, newLength r3, newSelection [sp+0]:[sp+4])
 *   NotifySelectionChanged(this r0, selection r1:r2)
 * (verified at 0x626c94/0x626cc6 in the ARM exe).  The x86 LONGLONG declaration would read r2:r3 / sp+8. */
static HRESULT WINAPI fake_core_text_notify_text_changed(
    FakeInspectable *object, Win32CoreTextRange modified_range, INT new_length,
    Win32CoreTextRange new_selection)
{
    char line[160];
    (void)object;
    core_text_document_length = new_length;
    core_text_selection_start = new_selection.start;
    core_text_selection_end = new_selection.end;
    wsprintfA(line,
        "CoreText NotifyTextChanged: range=%ld..%ld length=%d selection=%ld..%ld",
        (LONG)modified_range.start, (LONG)modified_range.end, new_length,
        (LONG)core_text_selection_start, (LONG)core_text_selection_end);
    log_line(line);
    return S_OK;
}

static HRESULT WINAPI fake_core_text_notify_selection_changed(
    FakeInspectable *object, Win32CoreTextRange selection)
{
    char line[112];
    (void)object;
    core_text_selection_start = selection.start;
    core_text_selection_end = selection.end;
    wsprintfA(line, "CoreText NotifySelectionChanged: %ld..%ld",
        (LONG)core_text_selection_start, (LONG)core_text_selection_end);
    log_line(line);
    return S_OK;
}
#else
static HRESULT WINAPI fake_core_text_notify_text_changed(
    FakeInspectable *object, LONGLONG modified_range, INT new_length,
    LONGLONG new_selection)
{
    char line[160];
    (void)object;
    core_text_document_length = new_length;
    core_text_selection_start = (INT)(DWORD)new_selection;
    core_text_selection_end = (INT)(DWORD)((ULONGLONG)new_selection >> 32);
    wsprintfA(line,
        "CoreText NotifyTextChanged: range=%ld..%ld length=%d selection=%ld..%ld",
        (LONG)(DWORD)modified_range,
        (LONG)(DWORD)((ULONGLONG)modified_range >> 32), new_length,
        (LONG)core_text_selection_start, (LONG)core_text_selection_end);
    log_line(line);
    return S_OK;
}

static HRESULT WINAPI fake_core_text_notify_selection_changed(
    FakeInspectable *object, LONGLONG selection)
{
    char line[112];
    (void)object;
    core_text_selection_start = (INT)(DWORD)selection;
    core_text_selection_end = (INT)(DWORD)((ULONGLONG)selection >> 32);
    wsprintfA(line, "CoreText NotifySelectionChanged: %ld..%ld",
        (LONG)core_text_selection_start, (LONG)core_text_selection_end);
    log_line(line);
    return S_OK;
}
#endif

/* ICoreTextServicesManagerStatics::GetForCurrentView. */
static const void *core_text_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_core_text_get_for_current_view
};

/* ICoreTextServicesManager: InputLanguage, language-change event, then
 * CreateEditContext. A null InputLanguage is sufficient for this host. */
static const void *core_text_manager_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_core_text_null_object,
    fake_core_text_add_event, fake_core_text_remove_event,
    fake_core_text_create_edit_context
};

/* ICoreTextEditContext ABI from UniversalApiContract 1.0: four property
 * pairs, nine event pairs, and five Notify methods. */
static const void *core_text_edit_context_vtable[37] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_core_text_get_name, fake_core_text_put_value,
    fake_core_text_get_value, fake_core_text_put_value,
    fake_core_text_get_value, fake_core_text_put_value,
    fake_core_text_get_value, fake_core_text_put_value,
    fake_core_text_add_event, fake_core_text_remove_event,
    fake_core_text_add_event, fake_core_text_remove_event,
    fake_core_text_add_event, fake_core_text_remove_event,
    core_text_text_updating_add, core_text_text_updating_remove,
    fake_core_text_add_event, fake_core_text_remove_event,
    fake_core_text_add_event, fake_core_text_remove_event,
    fake_core_text_add_event, fake_core_text_remove_event,
    fake_core_text_add_event, fake_core_text_remove_event,
    fake_core_text_add_event, fake_core_text_remove_event,
    fake_core_text_focus_enter, fake_core_text_focus_leave,
    fake_core_text_notify_text_changed,
    fake_core_text_notify_selection_changed,
    fake_core_text_notify
};

static HRESULT WINAPI fake_license_active(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = TRUE;
    return S_OK;
}

static HRESULT WINAPI fake_license_trial(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_license_add_changed(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object; (void)handler;
    if (!token) return E_POINTER;
    *token = 0;
    return S_OK;
}


/* Older MCPE networking asks NetworkInformation.GetHostNames before it
 * opens the native RakNet/WinSock sockets.  Returning an empty WinRT vector
 * makes 0.13.2/0.15.10 conclude that there is no usable adapter, while 1.1.5
 * no longer depends on that enumeration.  Expose one real local IPv4
 * HostName object, obtained through desktop Winsock, without requiring the
 * Windows 8 Networking WinRT implementation. */
typedef struct Win32CraftHostEnt {
    char *name;
    char **aliases;
    short address_type;
    short address_length;
    char **address_list;
} Win32CraftHostEnt;
typedef int (WINAPI *Win32CraftWSAStartupFn)(WORD, void *);
typedef int (WINAPI *Win32CraftGetHostNameFn)(char *, int);
typedef Win32CraftHostEnt *(WINAPI *Win32CraftGetHostByNameFn)(const char *);

static BOOL local_ipv4_resolved;
static LONG host_names_iterator_position;

static wchar_t *append_ipv4_octet(wchar_t *output, BYTE value)
{
    if (value >= 100) {
        *output++ = (wchar_t)(L'0' + value / 100);
        value = (BYTE)(value % 100);
        *output++ = (wchar_t)(L'0' + value / 10);
        *output++ = (wchar_t)(L'0' + value % 10);
    } else if (value >= 10) {
        *output++ = (wchar_t)(L'0' + value / 10);
        *output++ = (wchar_t)(L'0' + value % 10);
    } else {
        *output++ = (wchar_t)(L'0' + value);
    }
    return output;
}

static void set_host_ipv4_text(const BYTE *address)
{
    wchar_t *output = host_name.text;
    UINT index;
    for (index = 0; index < 4; ++index) {
        output = append_ipv4_octet(output, address[index]);
        if (index != 3) *output++ = L'.';
    }
    *output = 0;
}

#ifdef WIN32CRAFT_ARM32
/* The original body below becomes arm32_resolve_local_ipv4_once(); a thread-safe wrapper with the original name follows it. */
#define resolve_local_ipv4 arm32_resolve_local_ipv4_once
#endif
static void resolve_local_ipv4(void)
{
    HMODULE winsock;
    Win32CraftWSAStartupFn startup;
    Win32CraftGetHostNameFn get_host_name;
    Win32CraftGetHostByNameFn get_host_by_name;
    BYTE startup_data[512];
    char machine_name[256];
    Win32CraftHostEnt *entry;
    char **cursor;
    const BYTE *selected = NULL;
    static const BYTE loopback[4] = {127, 0, 0, 1};

    if (local_ipv4_resolved) return;
    set_host_ipv4_text(loopback);
    winsock = LoadLibraryW(L"ws2_32.dll");
    if (!winsock) {
        local_ipv4_resolved = TRUE;
        return;
    }
    startup = (Win32CraftWSAStartupFn)GetProcAddress(winsock, "WSAStartup");
    get_host_name = (Win32CraftGetHostNameFn)GetProcAddress(winsock, "gethostname");
    get_host_by_name = (Win32CraftGetHostByNameFn)GetProcAddress(winsock, "gethostbyname");
    ZeroMemory(startup_data, sizeof(startup_data));
    ZeroMemory(machine_name, sizeof(machine_name));
    if (!startup || !get_host_name || !get_host_by_name ||
        startup(0x0202, startup_data) != 0 ||
        get_host_name(machine_name, sizeof(machine_name) - 1) != 0) {
        local_ipv4_resolved = TRUE;
        return;
    }
    entry = get_host_by_name(machine_name);
    if (entry && entry->address_type == 2 && entry->address_length == 4) {
        for (cursor = entry->address_list; cursor && *cursor; ++cursor) {
            const BYTE *candidate = (const BYTE *)*cursor;
            if (!selected) selected = candidate;
            if (candidate[0] != 127 &&
                !(candidate[0] == 0 && candidate[1] == 0 &&
                  candidate[2] == 0 && candidate[3] == 0)) {
                selected = candidate;
                break;
            }
        }
    }
    if (selected) set_host_ipv4_text(selected);
    local_ipv4_resolved = TRUE;
}
#ifdef WIN32CRAFT_ARM32
#undef resolve_local_ipv4
/* NetworkInformation.GetHostNames can be asked from several game threads.  host_name.text is written first and the
 * "resolved" flag last; on ARM the stores can become visible in the other order, so a second thread could return a
 * half-written IPv4 string.  State 0 = idle, 1 = one thread resolving, 2 = text published (release/acquire). */
static volatile LONG arm32_local_ipv4_state;

static void resolve_local_ipv4(void)
{
    if (__atomic_load_n(&arm32_local_ipv4_state, __ATOMIC_ACQUIRE) == 2) return;
    if (__sync_val_compare_and_swap(&arm32_local_ipv4_state, 0, 1) == 0) {
        arm32_resolve_local_ipv4_once();
        __atomic_store_n(&arm32_local_ipv4_state, 2, __ATOMIC_RELEASE);
        return;
    }
    while (__atomic_load_n(&arm32_local_ipv4_state, __ATOMIC_ACQUIRE) != 2) Sleep(0);
}
#endif

static HRESULT WINAPI fake_network_get_host_names(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    resolve_local_ipv4();
    *result = &host_names;
    fake_add_ref(&host_names);
    log_line("Win32 NetworkInformation: local IPv4 HostName returned");
    return S_OK;
}

static HRESULT WINAPI fake_host_vector_get_at(
    FakeInspectable *object, UINT32 index, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    resolve_local_ipv4();
    if (index != 0) {
        *result = NULL;
        return E_BOUNDS;
    }
    *result = &host_name;
    fake_add_ref((FakeInspectable *)&host_name);
    return S_OK;
}

static HRESULT WINAPI fake_host_vector_size(
    FakeInspectable *object, UINT32 *result)
{
    (void)object;
    if (!result) return E_POINTER;
    resolve_local_ipv4();
    *result = 1;
    return S_OK;
}

static HRESULT WINAPI fake_host_vector_index_of(
    FakeInspectable *object, void *value, UINT32 *index, BYTE *found)
{
    (void)object;
    if (!index || !found) return E_POINTER;
    *index = 0;
    *found = value == &host_name;
    return S_OK;
}

static HRESULT WINAPI fake_host_vector_get_many(
    FakeInspectable *object, UINT32 start, UINT32 capacity,
    void **values, UINT32 *actual)
{
    (void)object;
    if (!actual) return E_POINTER;
    *actual = 0;
    if (start != 0 || capacity == 0) return S_OK;
    if (!values) return E_POINTER;
    values[0] = &host_name;
    fake_add_ref((FakeInspectable *)&host_name);
    *actual = 1;
    return S_OK;
}

static HRESULT WINAPI fake_host_iterable_first(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    host_names_iterator_position = 0;
    *result = &host_names_iterator;
    fake_add_ref(&host_names_iterator);
    return S_OK;
}

static HRESULT WINAPI fake_host_iterator_current(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    if (host_names_iterator_position != 0) {
        *result = NULL;
        return E_BOUNDS;
    }
    *result = &host_name;
    fake_add_ref((FakeInspectable *)&host_name);
    return S_OK;
}

static HRESULT WINAPI fake_host_iterator_has_current(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = host_names_iterator_position == 0;
    return S_OK;
}

static HRESULT WINAPI fake_host_iterator_move_next(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    host_names_iterator_position = 1;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_host_iterator_get_many(
    FakeInspectable *object, UINT32 capacity, void **values, UINT32 *actual)
{
    (void)object;
    if (!actual) return E_POINTER;
    *actual = 0;
    if (host_names_iterator_position != 0 || capacity == 0) return S_OK;
    if (!values) return E_POINTER;
    values[0] = &host_name;
    fake_add_ref((FakeInspectable *)&host_name);
    host_names_iterator_position = 1;
    *actual = 1;
    return S_OK;
}

static HRESULT WINAPI fake_host_name_ip_information(
    FakeHostName *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_host_name_text(
    FakeHostName *object, HSTRING *result)
{
    if (!result) return E_POINTER;
    resolve_local_ipv4();
    return WindowsCreateString(object->text, (UINT32)lstrlenW(object->text), result);
}

static HRESULT WINAPI fake_host_name_type(
    FakeHostName *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 1; /* HostNameType::Ipv4 */
    return S_OK;
}

static HRESULT WINAPI fake_host_name_is_equal(
    FakeHostName *object, void *other, BYTE *result)
{
    if (!result) return E_POINTER;
    *result = other == object;
    return S_OK;
}

/* Windows 7 has no Windows.Networking.Connectivity WinRT class.  The
 * Minecraft startup path only asks NetworkInformation for the current
 * InternetConnectionProfile.  A successful call with a null profile is the
 * documented "no active connection" result and is already handled by the
 * game.  Supplying the complete statics vtable also prevents later status
 * registration calls from throwing REGDB_E_CLASSNOTREG. */
static HRESULT WINAPI fake_network_get_collection(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &network_names;
    fake_add_ref(&network_names);
    log_line("Win32 NetworkInformation: empty collection returned");
    return S_OK;
}

static HRESULT WINAPI fake_network_get_internet_profile(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &connection_profile;
    fake_add_ref(&connection_profile);
    log_line("Win32 NetworkInformation: InternetConnectionProfile returned");
    return S_OK;
}

static HRESULT WINAPI fake_network_profile_name(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t name[] = L"Win32";
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(name, ARRAYSIZE(name) - 1, result);
}

static HRESULT WINAPI fake_network_connectivity_level(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 3; /* NetworkConnectivityLevel::InternetAccess */
    return S_OK;
}

static HRESULT WINAPI fake_network_profile_boolean(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_network_profile_null_object(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_network_get_connection_cost(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &connection_cost;
    fake_add_ref(&connection_cost);
    log_line("ConnectionProfile ConnectionCost returned");
    return S_OK;
}

static HRESULT WINAPI fake_network_cost_type(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 1; /* NetworkCostType::Unrestricted */
    return S_OK;
}

static HRESULT WINAPI fake_network_get_names(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &network_names;
    fake_add_ref(&network_names);
    log_line("ConnectionProfile NetworkNames returned");
    return S_OK;
}

static HRESULT WINAPI fake_empty_vector_get_at(
    FakeInspectable *object, UINT32 index, HSTRING *result)
{
    (void)object;
    (void)index;
    if (result) *result = NULL;
    return E_BOUNDS;
}

static HRESULT WINAPI fake_empty_vector_size(
    FakeInspectable *object, UINT32 *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 0;
    return S_OK;
}

static HRESULT WINAPI fake_empty_vector_index_of(
    FakeInspectable *object, HSTRING value, UINT32 *index, BYTE *found)
{
    (void)object;
    (void)value;
    if (!index || !found) return E_POINTER;
    *index = 0;
    *found = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_empty_vector_get_many(
    FakeInspectable *object, UINT32 start, UINT32 capacity,
    HSTRING *values, UINT32 *actual)
{
    (void)object;
    (void)start;
    (void)capacity;
    (void)values;
    if (!actual) return E_POINTER;
    *actual = 0;
    return S_OK;
}

static HRESULT WINAPI fake_empty_iterable_first(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &network_names_iterator;
    fake_add_ref(&network_names_iterator);
    log_line("NetworkNames IIterable::First returned empty iterator");
    return S_OK;
}

static HRESULT WINAPI fake_empty_iterator_current(
    FakeInspectable *object, HSTRING *result)
{
    (void)object;
    if (result) *result = NULL;
    return E_BOUNDS;
}

static HRESULT WINAPI fake_empty_iterator_has_current(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_empty_iterator_move_next(
    FakeInspectable *object, BYTE *result)
{
    return fake_empty_iterator_has_current(object, result);
}

static HRESULT WINAPI fake_empty_iterator_get_many(
    FakeInspectable *object, UINT32 capacity,
    HSTRING *values, UINT32 *actual)
{
    (void)object;
    (void)capacity;
    (void)values;
    if (!actual) return E_POINTER;
    *actual = 0;
    return S_OK;
}

static HRESULT WINAPI fake_network_get_proxy_async(
    FakeInspectable *object, void *uri, void **operation)
{
    (void)object;
    (void)uri;
    if (!operation) return E_POINTER;
    *operation = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI fake_network_get_sorted_pairs(
    FakeInspectable *object, void *destinations, INT options, void **result)
{
    (void)object;
    (void)destinations;
    (void)options;
    if (!result) return E_POINTER;
    *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI fake_network_add_changed(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    (void)handler;
    if (!token) return E_POINTER;
    *token = 0;
    return S_OK;
}

static HRESULT WINAPI fake_network_remove_changed(
    FakeInspectable *object, LONGLONG token)
{
    (void)object;
    (void)token;
    return S_OK;
}

static HRESULT WINAPI fake_gamepad_get_all(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &gamepad_vector;
    fake_add_ref(&gamepad_vector);
    log_line("Win32 Gamepad returned empty controller list");
    return S_OK;
}

static HRESULT WINAPI fake_gamepad_add_changed(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    (void)handler;
    if (!token) return E_POINTER;
    *token = 0;
    log_line("Win32 Gamepad event handler registered");
    return S_OK;
}

static HRESULT WINAPI fake_gamepad_remove_changed(
    FakeInspectable *object, LONGLONG token)
{
    (void)object;
    (void)token;
    log_line("Win32 Gamepad event handler removed");
    return S_OK;
}

/* IInspectable (0..5), then INetworkInformationStatics methods:
 * 6 GetConnectionProfiles, 7 GetInternetConnectionProfile,
 * 8 GetLanIdentifiers, 9 GetHostNames, 10 GetProxyConfigurationAsync,
 * 11 GetSortedEndpointPairs, 12/13 NetworkStatusChanged add/remove. */
static const void *network_factory_vtable[14] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_network_get_collection,
    fake_network_get_internet_profile,
    fake_network_get_collection,
    fake_network_get_host_names,
    fake_network_get_proxy_async,
    fake_network_get_sorted_pairs,
    fake_network_add_changed,
    fake_network_remove_changed
};

static const void *gamepad_factory_vtable[11] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_gamepad_add_changed, fake_gamepad_remove_changed,
    fake_gamepad_add_changed, fake_gamepad_remove_changed,
    fake_gamepad_get_all
};

static const void *gamepad_vector_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_empty_vector_get_at,
    fake_empty_vector_size,
    fake_empty_vector_index_of,
    fake_empty_vector_get_many
};

static const void *connection_profile_vtable[15] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_network_profile_name,
    fake_network_connectivity_level,
    fake_network_get_names,
    fake_network_get_connection_cost,
    fake_network_profile_null_object,
    fake_network_profile_null_object,
    NULL,
    NULL,
    fake_network_profile_null_object
};

/* IConnectionProfile2 is a separate WinRT interface, not an extension of
 * IConnectionProfile's vtable. The desktop host reports neither WWAN nor
 * WLAN and supplies null optional detail/reference objects. */
static const void *connection_profile2_vtable[15] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_network_profile_boolean,
    fake_network_profile_boolean,
    fake_network_profile_null_object,
    fake_network_profile_null_object,
    fake_network_profile_null_object,
    fake_network_profile_null_object,
    fake_network_connectivity_level,
    NULL,
    NULL
};

static const void *connection_cost_vtable[11] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_network_cost_type,
    fake_network_profile_boolean,
    fake_network_profile_boolean,
    fake_network_profile_boolean,
    fake_network_profile_boolean
};

static const void *network_names_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_empty_vector_get_at,
    fake_empty_vector_size,
    fake_empty_vector_index_of,
    fake_empty_vector_get_many
};

static const void *network_names_iterable_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_empty_iterable_first
};

static const void *network_names_iterator_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_empty_iterator_current,
    fake_empty_iterator_has_current,
    fake_empty_iterator_move_next,
    fake_empty_iterator_get_many
};

static const void *host_names_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_host_vector_get_at,
    fake_host_vector_size,
    fake_host_vector_index_of,
    fake_host_vector_get_many
};

static const void *host_names_iterable_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_host_iterable_first
};

static const void *host_names_iterator_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_host_iterator_current,
    fake_host_iterator_has_current,
    fake_host_iterator_move_next,
    fake_host_iterator_get_many
};

static const void *host_name_vtable[12] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_host_name_ip_information,
    fake_host_name_text,
    fake_host_name_text,
    fake_host_name_text,
    fake_host_name_type,
    fake_host_name_is_equal
};

static const void *crypto_buffer_vtable[9] = {
    fake_crypto_buffer_query_interface,
    fake_crypto_buffer_add_ref,
    fake_crypto_buffer_release,
    fake_crypto_buffer_get_iids,
    fake_crypto_buffer_class_name,
    fake_crypto_buffer_trust,
    fake_crypto_buffer_capacity,
    fake_crypto_buffer_length,
    fake_crypto_buffer_set_length
};

static const void *crypto_byte_access_vtable[4] = {
    fake_crypto_byte_query_interface,
    fake_crypto_byte_add_ref,
    fake_crypto_byte_release,
    fake_crypto_byte_buffer
};

/* ICryptographicBufferStatics: the game currently exercises random
 * generation and the two byte-array conversion methods. */
static const void *crypto_factory_vtable[17] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_crypto_compare,
    fake_crypto_generate_random,
    fake_crypto_generate_random_number,
    fake_crypto_create_from_array,
    fake_crypto_copy_to_array,
    NULL, NULL, NULL, NULL, NULL, NULL
};

/* IHardwareIdentificationStatics and IHardwareToken. The package-specific
 * identifier only needs to be stable within this unpackaged desktop host. */
static const void *hardware_id_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_hardware_get_package_token
};

static const void *hardware_token_vtable[9] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_hardware_token_id,
    fake_hardware_token_signature,
    fake_hardware_token_certificate
};

static const void *data_reader_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_data_reader_from_buffer
};

/* IDataReader slots through ReadBytes. Later conversion helpers are not used
 * while Minecraft turns the hardware token into its desktop device ID. */
static const void *data_reader_vtable[15] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_data_reader_get_value, fake_data_reader_put_value,
    fake_data_reader_get_value, fake_data_reader_put_value,
    fake_data_reader_unconsumed,
    fake_data_reader_get_value, fake_data_reader_put_value,
    fake_data_reader_read_byte,
    fake_data_reader_read_bytes
};

static const void *factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_factory_current
};

static const void *currentapp_factory_vtable[14] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_currentapp_get_license,
    fake_currentapp_link_uri,
    fake_currentapp_app_id,
    fake_currentapp_get_receipt,
    fake_currentapp_get_receipt,
    fake_currentapp_load_listing,
    fake_currentapp_get_receipt,
    fake_currentapp_get_receipt
};

/* ICurrentAppWithConsumables is a separate statics interface. Its final
 * slot returns IAsyncOperation<IVectorView<UnfulfilledConsumable>>. */
static const void *currentapp_consumables_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    NULL,
    NULL,
    NULL,
    fake_currentapp_get_unfulfilled
};

static const void *async_operation_vtable[9] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_async_put_completed,
    fake_async_get_completed,
    fake_async_get_results
};

/* Windows.Foundation.IAsyncInfo ABI:
 * 6 get_Id, 7 get_Status, 8 get_ErrorCode, 9 Cancel, 10 Close. */
static const void *async_info_vtable[11] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_async_info_get_id,
    fake_async_info_get_status,
    fake_async_info_get_error_code,
    fake_async_info_cancel,
    fake_async_info_close
};


static const void *launcher_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    NULL, NULL,
    fake_launcher_launch_uri,
    fake_launcher_launch_uri_options
};


/* LauncherOptions consists of property get/put pairs.  The desktop bridge
 * accepts them all as no-op state because ShellExecuteW does not require
 * UWP view preferences. */
static const void *launcher_options_vtable[30] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put,
    fake_launcher_option_get, fake_launcher_option_put
};

/* Windows 10 0.15.10 and Minecraft 1.1.5 both query the default
 * IFileSavePicker interface (IID 3286ffcb-617f-4cc5-af6a-b3fdf29ad145).
 * Its ABI is stable: PickSaveFileAsync is slot 19.  The old v20 0.15.10
 * table ended at slot 18, so the game called one pointer past the table and
 * eventually jumped to address 0. */
static const void *file_save_picker_vtable[20] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_picker_get_hstring, fake_picker_put_hstring,
    fake_picker_get_int, fake_picker_put_int,
    fake_picker_get_hstring, fake_picker_put_hstring,
    fake_picker_get_file_type_choices,
    fake_picker_get_hstring, fake_picker_put_default_extension,
    fake_not_implemented, fake_not_implemented,
    fake_picker_get_hstring, fake_picker_put_suggested_name,
    fake_picker_pick_save_file
};

/* IFileOpenPicker default interface ABI:
 *   6/7   ViewMode get/put
 *   8/9   SettingsIdentifier get/put
 *   10/11 SuggestedStartLocation get/put
 *   12/13 CommitButtonText get/put
 *   14    FileTypeFilter get
 *   15    PickSingleFileAsync
 * v20 incorrectly placed the filter getter in 12/13 and a picker call in
 * slot 14.  0.15.10 configures FileTypeFilter before showing the dialog, so
 * it received an async-operation object where it expected IVector<HSTRING>
 * and later called a null vtable entry. */
static const void *file_open_picker_vtable[17] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_picker_get_int, fake_picker_put_int,
    fake_picker_get_hstring, fake_picker_put_hstring,
    fake_picker_get_int, fake_picker_put_int,
    fake_picker_get_hstring, fake_picker_put_hstring,
    fake_picker_get_file_type_filter,
    fake_picker_pick_open_file,
    fake_not_implemented
};

static const void *file_type_filter_vtable[18] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_vector_get_hstring_at, fake_vector_get_size, fake_vector_get_view,
    fake_vector_index_of, fake_vector_set_hstring, fake_vector_insert_hstring,
    fake_vector_remove_at, fake_vector_append_hstring,
    fake_vector_no_args, fake_vector_no_args,
    fake_vector_append_hstring, fake_vector_no_args
};

static const void *file_type_choices_vtable[12] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_not_implemented, fake_not_implemented,
    fake_not_implemented, fake_not_implemented,
    fake_map_insert, fake_not_implemented
};

static const void *storage_file_vtable[18] = {
    fake_storage_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_storage_get_file_type, fake_storage_get_content_type,
    fake_not_implemented, fake_not_implemented,
    fake_not_implemented, fake_not_implemented, fake_storage_copy_async,
    fake_not_implemented, fake_not_implemented, fake_not_implemented,
    fake_not_implemented, fake_not_implemented
};

static const void *storage_item_vtable[16] = {
    fake_storage_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_not_implemented, fake_not_implemented,
    fake_not_implemented, fake_not_implemented,
    fake_not_implemented,
    fake_storage_get_name, fake_storage_get_path,
    fake_storage_get_attributes, fake_storage_get_date_created,
    fake_storage_is_of_type
};

static const void *cached_file_manager_vtable[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_cached_defer_updates, fake_cached_complete_updates
};

static const void *listing_information_vtable[12] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_listing_string,
    fake_listing_string,
    fake_listing_products,
    fake_listing_string,
    fake_listing_string,
    fake_listing_age_rating
};

static const void *product_listings_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_map_lookup,
    fake_map_size,
    fake_map_has_key,
    fake_map_split
};

static const void *api_information_vtable[16] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_api_type_present,
    fake_api_member_present,
    fake_api_method_present_with_arity,
    fake_api_member_present,
    fake_api_member_present,
    fake_api_member_present,
    fake_api_member_present,
    fake_api_member_present,
    fake_api_contract_major,
    fake_api_contract_minor
};

/* IApplicationLanguagesStatics and IVectorView<HSTRING>. */
static const void *application_languages_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_application_get_primary,
    fake_application_put_primary,
    fake_application_get_languages,
    fake_application_get_languages
};

static const void *language_vector_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_language_get_at,
    fake_language_get_size,
    fake_language_index_of,
    fake_language_get_many
};

/* Windows 7 has no WinRT GeographicRegion implementation. Minecraft only
 * needs the current territory while preparing locale/store state, so expose
 * a stable US region and a one-item USD currency vector. */
static const void *geographic_region_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_geographic_activate
};

static const void *geographic_region_vtable[13] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_geographic_code,
    fake_geographic_code,
    fake_geographic_code_three_letter,
    fake_geographic_code_three_digit,
    fake_geographic_display_name,
    fake_geographic_display_name,
    fake_geographic_currencies
};

static const void *xaml_application_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_xaml_get_current
};

/* Route Application::Exit to the HWND; other lifecycle methods stay inert. */
static const void *xaml_application_vtable[18] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_xaml_noop, fake_xaml_noop, fake_xaml_noop, fake_xaml_noop,
    fake_xaml_noop, fake_xaml_noop, fake_xaml_noop, fake_xaml_noop,
    fake_xaml_noop, fake_xaml_noop, fake_xaml_noop,
    fake_win32_application_exit
};

/* RoGetActivationFactory is asked directly for IWindowStatics, so
 * Current is the first method after IInspectable (slot 6).  The returned
 * IWindow is then queried for Content through slot 10. */
static const void *xaml_window_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_xaml_window_current
};

static const void *xaml_window_vtable[11] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    NULL, NULL, NULL, NULL,
    fake_xaml_window_content
};

/* AppMainXaml obtains IUIElement from its resize source and calls
 * get_ActualSize through slot 23. */
static const void *xaml_size_source_vtable[24] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_xaml_rasterization_scale,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    fake_xaml_window_actual_size
};

/* Windows.Media.SpeechSynthesis is absent on Windows 7. Minecraft 1.1.5
 * constructs a SpeechSynthesizer while AppPlatform_Winrt is being created,
 * even when narration is not enabled. Returning REGDB_E_CLASSNOTREG enters
 * vccorlib's WinRT exception path and crashes before AppMainXaml finishes.
 * This local projection supplies the constructor, IClosable, the base
 * synthesizer interfaces and one inert desktop voice. */
static HRESULT WINAPI fake_speech_activate(
    FakeInspectable *factory, void **result)
{
    (void)factory;
    if (!result) return E_POINTER;
    *result = &speech_synthesizer;
    fake_add_ref(&speech_synthesizer);
    log_line("Win7 SpeechSynthesizer activated local object");
    return S_OK;
}

static HRESULT WINAPI fake_speech_get_all_voices(
    FakeInspectable *factory, void **result)
{
    (void)factory;
    if (!result) return E_POINTER;
    *result = &speech_voice_vector;
    fake_add_ref(&speech_voice_vector);
    log_line("Win7 SpeechSynthesizer AllVoices returned one local voice");
    return S_OK;
}

static HRESULT WINAPI fake_speech_get_default_voice(
    FakeInspectable *factory, void **result)
{
    (void)factory;
    if (!result) return E_POINTER;
    *result = &speech_voice;
    fake_add_ref(&speech_voice);
    return S_OK;
}

static HRESULT WINAPI fake_speech_synthesize(
    FakeInspectable *object, HSTRING text, void **operation)
{
    (void)object;
    (void)text;
    if (!operation) return E_POINTER;
    *operation = &speech_async_operation;
    fake_add_ref(&speech_async_operation);
    log_line("Win7 SpeechSynthesizer synthesis request completed silently");
    return S_OK;
}

static HRESULT WINAPI fake_speech_put_voice(
    FakeInspectable *object, void *voice)
{
    (void)object;
    (void)voice;
    return S_OK;
}

static HRESULT WINAPI fake_speech_get_voice(
    FakeInspectable *object, void **voice)
{
    (void)object;
    if (!voice) return E_POINTER;
    *voice = &speech_voice;
    fake_add_ref(&speech_voice);
    return S_OK;
}

static HRESULT WINAPI fake_speech_close(FakeInspectable *object)
{
    (void)object;
    log_line("Win7 SpeechSynthesizer closed local object");
    return S_OK;
}

static HRESULT WINAPI fake_speech_get_options(
    FakeInspectable *object, void **options)
{
    (void)object;
    if (!options) return E_POINTER;
    *options = &speech_options;
    fake_add_ref(&speech_options);
    return S_OK;
}

static HRESULT WINAPI fake_speech_option_get(
    FakeInspectable *object, BYTE *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *value = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_speech_option_put(
    FakeInspectable *object, BYTE value)
{
    (void)object;
    (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_speech_voice_string(
    FakeInspectable *object, HSTRING *value)
{
    static const wchar_t text[] = L"Win32Craft Win32 Voice";
    (void)object;
    if (!value) return E_POINTER;
    return WindowsCreateString(text, ARRAYSIZE(text) - 1, value);
}

static HRESULT WINAPI fake_speech_voice_language(
    FakeInspectable *object, HSTRING *value)
{
    static const wchar_t text[] = L"en-US";
    (void)object;
    if (!value) return E_POINTER;
    return WindowsCreateString(text, ARRAYSIZE(text) - 1, value);
}

static HRESULT WINAPI fake_speech_voice_gender(
    FakeInspectable *object, INT *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

static HRESULT WINAPI fake_speech_vector_get_at(
    FakeInspectable *object, UINT32 index, void **value)
{
    (void)object;
    if (!value) return E_POINTER;
    if (index != 0) {
        *value = NULL;
        return E_BOUNDS;
    }
    *value = &speech_voice;
    fake_add_ref(&speech_voice);
    return S_OK;
}

static HRESULT WINAPI fake_speech_vector_size(
    FakeInspectable *object, UINT32 *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *value = 1;
    return S_OK;
}

static HRESULT WINAPI fake_speech_vector_index_of(
    FakeInspectable *object, void *value, UINT32 *index, BYTE *found)
{
    (void)object;
    if (!index || !found) return E_POINTER;
    *index = 0;
    *found = value == &speech_voice;
    return S_OK;
}

static HRESULT WINAPI fake_speech_vector_get_many(
    FakeInspectable *object, UINT32 start, UINT32 capacity,
    void **values, UINT32 *actual)
{
    (void)object;
    if (!actual) return E_POINTER;
    *actual = 0;
    if (start == 0 && capacity && values) {
        values[0] = &speech_voice;
        fake_add_ref(&speech_voice);
        *actual = 1;
    }
    return S_OK;
}

static HRESULT WINAPI fake_speech_iterable_first(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &speech_voice_iterator;
    fake_add_ref(&speech_voice_iterator);
    return S_OK;
}

static HRESULT WINAPI fake_speech_iterator_current(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &speech_voice;
    fake_add_ref(&speech_voice);
    return S_OK;
}

static HRESULT WINAPI fake_speech_iterator_has_current(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = TRUE;
    return S_OK;
}

static HRESULT WINAPI fake_speech_iterator_move_next(
    FakeInspectable *object, BYTE *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_speech_iterator_get_many(
    FakeInspectable *object, UINT32 capacity, void **values, UINT32 *actual)
{
    return fake_speech_vector_get_many(object, 0, capacity, values, actual);
}

static const void *speech_activation_factory_vtable[7] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_activate
};

static const void *speech_voices_factory_vtable[8] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_get_all_voices, fake_speech_get_default_voice
};

static const void *speech_synthesizer_vtable[10] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_synthesize, fake_speech_synthesize,
    fake_speech_put_voice, fake_speech_get_voice
};

static const void *speech_closable_vtable[7] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_close
};

static const void *speech_synthesizer2_vtable[7] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_get_options
};

static const void *speech_options_vtable[10] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_option_get, fake_speech_option_put,
    fake_speech_option_get, fake_speech_option_put
};

static const void *speech_voice_vtable[11] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_voice_string,
    fake_speech_voice_string,
    fake_speech_voice_language,
    fake_speech_voice_string,
    fake_speech_voice_gender
};

static const void *speech_voice_vector_vtable[10] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_vector_get_at, fake_speech_vector_size,
    fake_speech_vector_index_of, fake_speech_vector_get_many
};

static const void *speech_voice_iterable_vtable[7] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_iterable_first
};

static const void *speech_voice_iterator_vtable[10] = {
    fake_speech_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_speech_iterator_current, fake_speech_iterator_has_current,
    fake_speech_iterator_move_next, fake_speech_iterator_get_many
};

/* IAudioGraphSettingsFactory::Create(AudioRenderCategory).  Minecraft 1.2.8
 * constructs the settings object and immediately passes it to
 * AudioGraph::CreateAsync; no settings members are read on the startup path.
 * Keeping this projection local prevents the C++/CX constructor from raising
 * ClassNotRegisteredException in an unpackaged Win32 process. */
static HRESULT WINAPI fake_audio_graph_settings_create(
    FakeInspectable *factory, INT category, void **result)
{
    (void)factory;
    (void)category;
    if (!result) return E_POINTER;
    *result = &audio_graph_settings;
    fake_add_ref(&audio_graph_settings);
    log_line("Win32 AudioGraphSettings created");
    return S_OK;
}

static const void *audio_graph_settings_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_audio_graph_settings_create
};

static const void *audio_graph_settings_vtable[6] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level
};

/* The UWP AudioGraph backend is optional: 1.2.8 falls back to its FMOD path
 * when CreateAudioGraphResult.Status is not Success.  Complete CreateAsync
 * normally with DeviceNotAvailable instead of throwing from activation. */
static HRESULT WINAPI fake_audio_graph_create_async(
    FakeInspectable *factory, FakeInspectable *settings, void **result)
{
    (void)factory;
    (void)settings;
    if (!result) return E_POINTER;
    *result = &audio_graph_async_operation;
    fake_add_ref(&audio_graph_async_operation);
    log_line("Win32 AudioGraph CreateAsync completed without UWP device");
    return S_OK;
}

static HRESULT WINAPI fake_audio_graph_result_status(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 1; /* AudioGraphCreationStatus::DeviceNotAvailable */
    return S_OK;
}

static HRESULT WINAPI fake_audio_graph_result_graph(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static const void *audio_graph_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_audio_graph_create_async
};

static const void *audio_graph_create_result_vtable[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_audio_graph_result_status, fake_audio_graph_result_graph
};


/* Minimal unpackaged MRT projection. Minecraft 1.1.5 only needs a stable
 * context during AppPlatform construction; qualifier mutation is accepted
 * and kept process-local as no-ops. */
static HRESULT WINAPI fake_resource_context_activate(
    FakeInspectable *factory, void **result)
{
    (void)factory;
    if (!result) return E_POINTER;
    *result = &resource_context;
    fake_add_ref(&resource_context);
    log_line("Win7 ResourceContext activated local context");
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_get(
    FakeInspectable *factory, void **result)
{
    (void)factory;
    if (!result) return E_POINTER;
    *result = &resource_context;
    fake_add_ref(&resource_context);
    log_line("Win7 ResourceContext.GetForCurrentView returned local context");
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_create_matching(
    FakeInspectable *factory, void *qualifiers, void **result)
{
    (void)qualifiers;
    return fake_resource_context_get(factory, result);
}

static HRESULT WINAPI fake_resource_context_global_set(
    FakeInspectable *factory, HSTRING key, HSTRING value)
{
    (void)factory; (void)key; (void)value;
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_global_reset(
    FakeInspectable *factory)
{
    (void)factory;
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_global_reset_names(
    FakeInspectable *factory, void *names)
{
    (void)factory; (void)names;
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_global_set_persistent(
    FakeInspectable *factory, HSTRING key, HSTRING value, INT persistence)
{
    (void)factory; (void)key; (void)value; (void)persistence;
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_qualifiers(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &resource_qualifier_map_observable;
    fake_add_ref(&resource_qualifier_map_observable);
    log_line("Win7 ResourceContext.QualifierValues returned local map");
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_reset(FakeInspectable *object)
{
    (void)object;
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_reset_names(
    FakeInspectable *object, void *names)
{
    (void)object; (void)names;
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_override(
    FakeInspectable *object, void *qualifiers)
{
    (void)object; (void)qualifiers;
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_clone(
    FakeInspectable *object, void **result)
{
    return fake_resource_context_get(object, result);
}

static HRESULT WINAPI fake_resource_context_languages(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &language_vector;
    fake_add_ref(&language_vector);
    log_line("Win7 ResourceContext.Languages returned en-US vector");
    return S_OK;
}

static HRESULT WINAPI fake_resource_context_set_languages(
    FakeInspectable *object, void *languages)
{
    (void)object; (void)languages;
    return S_OK;
}

static HRESULT WINAPI fake_resource_map_changed_add(
    FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object; (void)handler;
    if (!token) return E_POINTER;
    *token = 0;
    return S_OK;
}

static HRESULT WINAPI fake_resource_map_changed_remove(
    FakeInspectable *object, LONGLONG token)
{
    (void)object; (void)token;
    return S_OK;
}

static HRESULT WINAPI fake_resource_string_map_lookup(
    FakeInspectable *object, HSTRING key, HSTRING *result)
{
    static const wchar_t language[] = L"en-US";
    static const wchar_t scale[] = L"100";
    UINT32 length = 0;
    const wchar_t *name;
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    name = WindowsGetStringRawBuffer(key, &length);
    if (name && lstrcmpW(name, L"Language") == 0)
        return WindowsCreateString(language, ARRAYSIZE(language) - 1, result);
    if (name && lstrcmpW(name, L"Scale") == 0)
        return WindowsCreateString(scale, ARRAYSIZE(scale) - 1, result);
    return E_BOUNDS;
}

static HRESULT WINAPI fake_resource_string_map_size(
    FakeInspectable *object, UINT32 *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = 2;
    return S_OK;
}

static HRESULT WINAPI fake_resource_string_map_has_key(
    FakeInspectable *object, HSTRING key, BYTE *result)
{
    UINT32 length = 0;
    const wchar_t *name;
    (void)object;
    if (!result) return E_POINTER;
    name = WindowsGetStringRawBuffer(key, &length);
    *result = name && (lstrcmpW(name, L"Language") == 0 ||
                       lstrcmpW(name, L"Scale") == 0);
    return S_OK;
}

static HRESULT WINAPI fake_resource_string_map_get_view(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &resource_qualifier_map_view;
    fake_add_ref(&resource_qualifier_map_view);
    return S_OK;
}

static HRESULT WINAPI fake_resource_string_map_insert(
    FakeInspectable *object, HSTRING key, HSTRING value, BYTE *replaced)
{
    (void)object; (void)key; (void)value;
    if (!replaced) return E_POINTER;
    *replaced = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_resource_string_map_remove(
    FakeInspectable *object, HSTRING key)
{
    (void)object; (void)key;
    return S_OK;
}

static HRESULT WINAPI fake_resource_string_map_clear(FakeInspectable *object)
{
    (void)object;
    return S_OK;
}

static HRESULT WINAPI fake_resource_string_map_split(
    FakeInspectable *object, void **first, void **second)
{
    (void)object;
    if (first) *first = NULL;
    if (second) *second = NULL;
    return S_OK;
}

/* ResourceManager is included proactively because 1.1.5's resource setup
 * commonly asks for it immediately after ResourceContext. The map is inert;
 * Minecraft's own JSON localization remains the source of UI strings. */
static HRESULT WINAPI fake_resource_manager_current(
    FakeInspectable *factory, void **result)
{
    (void)factory;
    if (!result) return E_POINTER;
    *result = &resource_manager;
    fake_add_ref(&resource_manager);
    log_line("Win7 ResourceManager.Current returned local manager");
    return S_OK;
}

static HRESULT WINAPI fake_resource_manager_is_reference(
    FakeInspectable *factory, HSTRING value, BYTE *result)
{
    (void)factory; (void)value;
    if (!result) return E_POINTER;
    *result = FALSE;
    return S_OK;
}

static HRESULT WINAPI fake_resource_manager_main_map(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &resource_map;
    fake_add_ref(&resource_map);
    return S_OK;
}

static HRESULT WINAPI fake_resource_manager_all_maps(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_resource_manager_default_context(
    FakeInspectable *object, void **result)
{
    return fake_resource_context_get(object, result);
}

static HRESULT WINAPI fake_resource_manager_pri_noop(
    FakeInspectable *object, void *files)
{
    (void)object; (void)files;
    return S_OK;
}

static HRESULT WINAPI fake_resource_map_uri(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = NULL;
    return S_OK;
}

static HRESULT WINAPI fake_resource_map_value(
    FakeInspectable *object, HSTRING name, void **result)
{
    (void)object; (void)name;
    if (!result) return E_POINTER;
    *result = NULL;
    return E_BOUNDS;
}

static HRESULT WINAPI fake_resource_map_value_for_context(
    FakeInspectable *object, HSTRING name, void *context, void **result)
{
    (void)context;
    return fake_resource_map_value(object, name, result);
}

static HRESULT WINAPI fake_resource_map_subtree(
    FakeInspectable *object, HSTRING name, void **result)
{
    (void)object; (void)name;
    if (!result) return E_POINTER;
    *result = &resource_map;
    fake_add_ref(&resource_map);
    return S_OK;
}

static const void *resource_context_activation_factory_vtable[7] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_context_activate
};

static const void *resource_context_statics_vtable[7] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_context_create_matching
};

static const void *resource_context_statics2_vtable[11] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_context_get,
    fake_resource_context_global_set,
    fake_resource_context_global_reset,
    fake_resource_context_global_reset_names,
    fake_resource_context_get
};

static const void *resource_context_statics3_vtable[7] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_context_global_set_persistent
};

static const void *resource_context_vtable[13] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_context_qualifiers,
    fake_resource_context_reset,
    fake_resource_context_reset_names,
    fake_resource_context_override,
    fake_resource_context_clone,
    fake_resource_context_languages,
    fake_resource_context_set_languages
};

static const void *resource_qualifier_map_observable_vtable[8] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_map_changed_add,
    fake_resource_map_changed_remove
};

static const void *resource_qualifier_map_vtable[13] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_string_map_lookup,
    fake_resource_string_map_size,
    fake_resource_string_map_has_key,
    fake_resource_string_map_get_view,
    fake_resource_string_map_insert,
    fake_resource_string_map_remove,
    fake_resource_string_map_clear
};

static const void *resource_qualifier_map_view_vtable[10] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_string_map_lookup,
    fake_resource_string_map_size,
    fake_resource_string_map_has_key,
    fake_resource_string_map_split
};

static const void *resource_manager_statics_vtable[8] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_manager_current,
    fake_resource_manager_is_reference
};

static const void *resource_manager_vtable[11] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_manager_main_map,
    fake_resource_manager_all_maps,
    fake_resource_manager_default_context,
    fake_resource_manager_pri_noop,
    fake_resource_manager_pri_noop
};

static const void *resource_map_vtable[10] = {
    fake_resource_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_resource_map_uri,
    fake_resource_map_value,
    fake_resource_map_value_for_context,
    fake_resource_map_subtree
};

/* Windows.System.Profile.AnalyticsInfo does not exist on Windows 7.
 * Minecraft 1.1.5 asks IAnalyticsInfoStatics::VersionInfo through slot 6,
 * then reads IAnalyticsVersionInfo::DeviceFamily through slot 6.  Returning
 * Windows.Desktop keeps the original desktop/Xbox branch selection intact
 * and avoids vccorlib raising REGDB_E_CLASSNOTREG during AppMainXaml setup. */
static HRESULT WINAPI fake_analytics_version_info_get(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &analytics_version_info;
    fake_add_ref(&analytics_version_info);
    log_line("AnalyticsInfo.VersionInfo returned Win32 version object");
    return S_OK;
}

static HRESULT WINAPI fake_analytics_device_family(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t family[] = L"Windows.Desktop";
    (void)object;
    if (!result) return E_POINTER;
    log_line("AnalyticsVersionInfo.DeviceFamily returned Windows.Desktop");
    return WindowsCreateString(family, ARRAYSIZE(family) - 1, result);
}

static HRESULT WINAPI fake_analytics_device_family_version(
    FakeInspectable *object, HSTRING *result)
{
    /* Encoded 10.0.14393.0, matching the Win10 1607-era desktop contract
     * that the original 1.1.5 binary targets. */
    static const wchar_t version[] = L"2814750710366208";
    (void)object;
    if (!result) return E_POINTER;
    return WindowsCreateString(version, ARRAYSIZE(version) - 1, result);
}

static HRESULT WINAPI fake_analytics_device_form(
    FakeInspectable *object, HSTRING *result)
{
    static const wchar_t form[] = L"Desktop";
    (void)object;
    if (!result) return E_POINTER;
    log_line("AnalyticsInfo.DeviceForm returned Desktop");
    return WindowsCreateString(form, ARRAYSIZE(form) - 1, result);
}

static const void *analytics_info_factory_vtable[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_analytics_version_info_get,
    fake_analytics_device_form
};

static const void *analytics_version_info_vtable[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_analytics_device_family,
    fake_analytics_device_family_version
};

/* 1.1.5 asks IDisplayInformationStatics::GetForCurrentView through slot 6.
 * The returned display object is then sampled through slots 12 and 13 while
 * AppPlatform_Winrt updates its pixel-density state. Wine's activation
 * factory exists but those calls fail, so keep this small HWND-backed ABI
 * bridge deterministic at the conventional 96-DPI desktop baseline. */
static const void *display_information_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_display_get_for_current_view
};

static const void *display_information_vtable[22] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_display_orientation,
    fake_display_orientation,
    fake_display_add_event, fake_display_remove_event,
    fake_display_resolution_scale,
    fake_display_dpi_x,
    fake_display_dpi_x,
    fake_display_dpi_y,
    fake_display_add_event, fake_display_remove_event,
    fake_display_stereo_enabled,
    fake_display_add_event, fake_display_remove_event,
    fake_display_null_async,
    fake_display_add_event, fake_display_remove_event
};

static const void *application_view_factory_vtable[9] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_application_view_current,
    fake_application_view_get_bool,
    fake_application_view_put_value
};

static const void *application_view_vtable[18] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_application_view_get_int,
    fake_application_view_get_bool,
    fake_application_view_get_bool,
    fake_application_view_get_fullscreen,
    fake_application_view_get_bool,
    fake_application_view_get_bool,
    fake_application_view_put_value,
    fake_application_view_put_value,
    fake_application_view_get_title,
    fake_application_view_get_int,
    fake_application_view_add_event,
    fake_application_view_remove_event
};

static const void *application_view3_vtable[15] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_application_view_null_object,
    fake_application_view_get_int,
    fake_application_view_put_value,
    fake_application_view_get_fullscreen,
    application_view_enter_fullscreen_116,
    application_view_exit_fullscreen_116,
    fake_application_view_noop,
    fake_application_view_try_resize,
    fake_application_view_set_size
};

static const void *package_vtable[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    NULL, fake_package_installed_location
};

/* IApplicationData continues after LocalFolder with RoamingFolder and
 * TemporaryFolder. 0.15.10 reads all three paths while constructing
 * AppPlatform; the desktop host intentionally maps them to the same
 * writable MinecraftPE root. */
static const void *appdata_vtable[15] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    NULL, NULL, NULL, NULL, NULL, NULL,
    fake_appdata_local_folder,
    fake_appdata_local_folder,
    fake_appdata_local_folder
};

static const void *appdata2_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_appdata_local_folder
};

static const void *currentapp_vtable[16] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    NULL, fake_license_active, fake_license_trial, NULL, fake_license_add_changed,
    NULL, NULL, NULL, NULL, NULL
};

static const void *license_vtable[12] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_listing_products, fake_license_active, fake_license_trial, NULL,
    fake_license_add_changed, NULL
};

/* 0.15.10 uses the original pre-picker StorageFolder projection.  Keep the
 * CreateFileAsync slot null there so v19-v23 Import/Export experiments cannot
 * affect any of its normal ApplicationData folder traffic. */
static const void *folder_vtable_01510[13] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    NULL, NULL, NULL, NULL, NULL, NULL, fake_folder_path
};

/* 1.1.5 keeps the working Win32 Export/Import World bridge. */
static const void *folder_vtable_115[13] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    NULL, fake_folder_create_file_with_collision,
    NULL, NULL, NULL, NULL, fake_folder_path
};

static const void *memory_vtable[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_memory_usage, fake_memory_limit
};

#ifdef WIN32CRAFT_ARM32
/* ICoreApplication continues past slot 11: 12 GetCurrentView, 13 Run, 14 RunWithActivationFactories, ...  The ARM exe calls
 * slot 12 (XSAPI 0xa758a8) and slot 13 (CoreApplication::Run 0x596328); the 12-entry x86 table made those reads run off the end of
 * the array.  24 entries so every NULL hole is filled with the logging E_NOTIMPL stub (arm32_fill_unimplemented_slots_once). */
static const void *coreapp_vtable[24] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_coreapp_id, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
#else
static const void *coreapp_vtable[12] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_coreapp_id, NULL, NULL, NULL, NULL, NULL
};
#endif

static const void *coreapp_exit_vtable[9] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    fake_win32_application_exit, fake_display_add_event,
    fake_display_remove_event
};

#ifdef WIN32CRAFT_ARM32
/*
 * ARM32 abi_audit additions to the fake WinRT shim.
 *
 * (1) Unimplemented-slot trap.  Many fake vtables leave slots NULL (x86 never reached them).  A NULL slot would crash
 *     at pc=0 with no hint which interface was called; every NULL hole of the tables below is filled with a stub that
 *     logs slot/object kind/caller and returns E_NOTIMPL (C++/CX turns that into a catchable exception).
 * (2) IID-specific factories the ARM exe asks for that the x86 dispatcher answered with the wrong table:
 *       Windows.UI.ViewManagement.ApplicationView  IID A28D7594-8C41-4E13-9719-5164796FE4C7
 *           (IApplicationViewStatics3: slot 7 = put_PreferredLaunchWindowingMode(enum), called by the game's
 *            AppPlatformWinRTDispatcher::setFullscreenMode lambda 0x5870b4 through 0x589af4 with r1 = 0 or 2; the
 *            shared application_view_factory table treated r1 as a BYTE* result -> E_POINTER / write to address 2)
 *       Windows.ApplicationModel.Store.CurrentApp  IID DF4E6E2D-3171-4AD3-8614-2C61244373CB
 *           (ICurrentApp2Statics: slot 6 GetCustomerPurchaseIdAsync(token, ticket, &op) called by 0x574ccc; the shared
 *            currentapp_factory table wrote a LicenseInformation pointer through the HSTRING argument)
 * (3) A bounded log line for every activation request (class + IID) so the device log shows exactly which classes the
 *     game asked for and which fell through to the real runtime.
 */
static void arm32_narrow_class_name(LPCWSTR class_name, char *out, UINT capacity)
{
    UINT index = 0;
    if (!capacity) return;
    if (class_name) {
        while (index + 1 < capacity && class_name[index]) {
            out[index] = class_name[index] < 0x80 ? (char)class_name[index] : '?';
            ++index;
        }
    }
    out[index] = 0;
}

static void arm32_log_activation_request(LPCWSTR class_name, const GUID *iid)
{
    static volatile LONG count;
    char name[160];
    char line[320];
    LONG sequence = InterlockedIncrement(&count);
    if (sequence > 400) return;
    arm32_narrow_class_name(class_name, name, sizeof(name));
    if (iid) {
        wsprintfA(line,
            "ARM32 activation #%ld %s IID=%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            (long)sequence, name[0] ? name : "(null)", (unsigned long)iid->Data1,
            (unsigned)iid->Data2, (unsigned)iid->Data3, (unsigned)iid->Data4[0],
            (unsigned)iid->Data4[1], (unsigned)iid->Data4[2], (unsigned)iid->Data4[3],
            (unsigned)iid->Data4[4], (unsigned)iid->Data4[5], (unsigned)iid->Data4[6],
            (unsigned)iid->Data4[7]);
    } else {
        wsprintfA(line, "ARM32 activation #%ld %s (no IID)", (long)sequence,
                  name[0] ? name : "(null)");
    }
    log_line(line);
}

static HRESULT __attribute__((noinline)) arm32_unimplemented_slot(
    FakeInspectable *object, unsigned slot, ULONG_PTR caller)
{
    static volatile LONG count;
    char line[256];
    ULONG_PTR image = (ULONG_PTR)game_image;
    ULONG_PTR rva = (image && (caller & ~(ULONG_PTR)1) >= image &&
                     (caller & ~(ULONG_PTR)1) - image < 0x02000000u)
        ? (caller & ~(ULONG_PTR)1) - image : 0;
    if (InterlockedIncrement(&count) <= 64) {
        wsprintfA(line,
            "ARM32 WinRT fake: UNIMPLEMENTED slot %u called on object=%p kind=%d "
            "caller=%p (game RVA 0x%08lX) -> E_NOTIMPL",
            slot, object, object ? (int)object->kind : -1, (void *)caller,
            (unsigned long)rva);
        log_line(line);
    }
    return E_NOTIMPL;
}

#define ARM32_UNIMPLEMENTED_SLOT(n) \
    static HRESULT WINAPI arm32_unimplemented_slot_##n(FakeInspectable *object) \
    { return arm32_unimplemented_slot(object, (n), (ULONG_PTR)__builtin_return_address(0)); }
ARM32_UNIMPLEMENTED_SLOT(0)  ARM32_UNIMPLEMENTED_SLOT(1)  ARM32_UNIMPLEMENTED_SLOT(2)  ARM32_UNIMPLEMENTED_SLOT(3)
ARM32_UNIMPLEMENTED_SLOT(4)  ARM32_UNIMPLEMENTED_SLOT(5)  ARM32_UNIMPLEMENTED_SLOT(6)  ARM32_UNIMPLEMENTED_SLOT(7)
ARM32_UNIMPLEMENTED_SLOT(8)  ARM32_UNIMPLEMENTED_SLOT(9)  ARM32_UNIMPLEMENTED_SLOT(10) ARM32_UNIMPLEMENTED_SLOT(11)
ARM32_UNIMPLEMENTED_SLOT(12) ARM32_UNIMPLEMENTED_SLOT(13) ARM32_UNIMPLEMENTED_SLOT(14) ARM32_UNIMPLEMENTED_SLOT(15)
ARM32_UNIMPLEMENTED_SLOT(16) ARM32_UNIMPLEMENTED_SLOT(17) ARM32_UNIMPLEMENTED_SLOT(18) ARM32_UNIMPLEMENTED_SLOT(19)
ARM32_UNIMPLEMENTED_SLOT(20) ARM32_UNIMPLEMENTED_SLOT(21) ARM32_UNIMPLEMENTED_SLOT(22) ARM32_UNIMPLEMENTED_SLOT(23)

static HRESULT (WINAPI *const arm32_unimplemented_slots[24])(FakeInspectable *) = {
    arm32_unimplemented_slot_0,  arm32_unimplemented_slot_1,  arm32_unimplemented_slot_2,  arm32_unimplemented_slot_3,
    arm32_unimplemented_slot_4,  arm32_unimplemented_slot_5,  arm32_unimplemented_slot_6,  arm32_unimplemented_slot_7,
    arm32_unimplemented_slot_8,  arm32_unimplemented_slot_9,  arm32_unimplemented_slot_10, arm32_unimplemented_slot_11,
    arm32_unimplemented_slot_12, arm32_unimplemented_slot_13, arm32_unimplemented_slot_14, arm32_unimplemented_slot_15,
    arm32_unimplemented_slot_16, arm32_unimplemented_slot_17, arm32_unimplemented_slot_18, arm32_unimplemented_slot_19,
    arm32_unimplemented_slot_20, arm32_unimplemented_slot_21, arm32_unimplemented_slot_22, arm32_unimplemented_slot_23
};

/* ICurrentApp (D52DC065) slots whose argument lists differ from the shared x86 fake_currentapp_get_receipt(object, void **result):
 *   9  RequestAppPurchaseAsync(bool includeReceipt, IAsyncOperation<HSTRING>**)                  -> r1 = bool, r2 = out
 *   10 RequestProductPurchaseAsync(HSTRING productId, bool includeReceipt, IAsyncOperation**)    -> r1, r2 = bool, r3 = out
 *   13 GetProductReceiptAsync(HSTRING productId, IAsyncOperation**)                              -> r1, r2 = out
 * The ARM exe calls slot 9 (0x575d20: `adds r2,r7,#8; ldr r3,[r3,#0x24]; blx` with the bool still in r1).  With the shared stub the
 * bool (0/1) was used as the result pointer: E_POINTER, or a store to address 1. */
static HRESULT WINAPI arm32_currentapp_request_app_purchase(
    FakeInspectable *object, BYTE include_receipt, void **operation)
{
    (void)include_receipt;
    return fake_currentapp_get_receipt(object, operation);
}

static HRESULT WINAPI arm32_currentapp_request_product_purchase(
    FakeInspectable *object, HSTRING product_id, BYTE include_receipt,
    void **operation)
{
    (void)product_id;
    (void)include_receipt;
    return fake_currentapp_get_receipt(object, operation);
}

static HRESULT WINAPI arm32_currentapp_get_product_receipt(
    FakeInspectable *object, HSTRING product_id, void **operation)
{
    (void)product_id;
    return fake_currentapp_get_receipt(object, operation);
}

typedef struct Arm32VtableHoles {
    const char *name;
    const void **table;
    unsigned count;
} Arm32VtableHoles;

static void arm32_fill_unimplemented_slots_once(void)
{
    static volatile LONG done;
    static const Arm32VtableHoles tables[] = {
        { "connection_profile",    connection_profile_vtable,    ARRAYSIZE(connection_profile_vtable) },
        { "connection_profile2",   connection_profile2_vtable,   ARRAYSIZE(connection_profile2_vtable) },
        { "crypto_factory",        crypto_factory_vtable,        ARRAYSIZE(crypto_factory_vtable) },
        { "currentapp_consumables", currentapp_consumables_vtable, ARRAYSIZE(currentapp_consumables_vtable) },
        { "launcher",              launcher_vtable,              ARRAYSIZE(launcher_vtable) },
        { "xaml_window",           xaml_window_vtable,           ARRAYSIZE(xaml_window_vtable) },
        { "xaml_size_source",      xaml_size_source_vtable,      ARRAYSIZE(xaml_size_source_vtable) },
        { "package",               package_vtable,               ARRAYSIZE(package_vtable) },
        { "appdata",               appdata_vtable,               ARRAYSIZE(appdata_vtable) },
        { "currentapp",            currentapp_vtable,            ARRAYSIZE(currentapp_vtable) },
        { "license",               license_vtable,               ARRAYSIZE(license_vtable) },
        { "folder_01510",          folder_vtable_01510,          ARRAYSIZE(folder_vtable_01510) },
        { "folder_115",            folder_vtable_115,            ARRAYSIZE(folder_vtable_115) },
        { "coreapp",               coreapp_vtable,               ARRAYSIZE(coreapp_vtable) }
    };
    unsigned filled = 0;
    unsigned table_index;
    unsigned slot;
    char line[96];

    /* 0 = idle, 1 = one thread filling, 2 = every table complete.  A second activating thread must not return (and then call a
     * still-NULL slot) before the first one has finished. */
    if (__atomic_load_n(&done, __ATOMIC_ACQUIRE) == 2) return;
    if (__sync_val_compare_and_swap(&done, 0, 1) != 0) {
        while (__atomic_load_n(&done, __ATOMIC_ACQUIRE) != 2) Sleep(0);
        return;
    }
    for (table_index = 0; table_index < ARRAYSIZE(tables); ++table_index) {
        const Arm32VtableHoles *entry = &tables[table_index];
        for (slot = 0; slot < entry->count; ++slot) {
            if (!entry->table[slot] && slot < ARRAYSIZE(arm32_unimplemented_slots)) {
                entry->table[slot] = (const void *)arm32_unimplemented_slots[slot];
                ++filled;
            }
        }
    }
    currentapp_factory_vtable[9] = (const void *)arm32_currentapp_request_app_purchase;
    currentapp_factory_vtable[10] = (const void *)arm32_currentapp_request_product_purchase;
    currentapp_factory_vtable[13] = (const void *)arm32_currentapp_get_product_receipt;
    wsprintfA(line, "ARM32 WinRT fakes: %u NULL vtable slots now log and return E_NOTIMPL", filled);
    log_line(line);
    __atomic_store_n(&done, 2, __ATOMIC_RELEASE);
}

/* --- IApplicationViewStatics3 (slots 6/7 PreferredLaunchWindowingMode get/put, 8/9 PreferredLaunchViewSize get/put) --- */
static volatile LONG arm32_preferred_windowing_mode;

static HRESULT WINAPI arm32_app_view_statics3_get_mode(
    FakeInspectable *object, INT *result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = (INT)arm32_preferred_windowing_mode;
    return S_OK;
}

static HRESULT WINAPI arm32_app_view_statics3_put_mode(
    FakeInspectable *object, INT mode)
{
    char line[96];
    (void)object;
    arm32_preferred_windowing_mode = mode;
    wsprintfA(line, "ApplicationView.PreferredLaunchWindowingMode = %d accepted", (int)mode);
    log_line(line);
    return S_OK;
}

static HRESULT WINAPI arm32_app_view_statics3_get_size(
    FakeInspectable *object, Win32Float2 *result)
{
    (void)object;
    if (!result) return E_POINTER;
    result->x = 0.0f;
    result->y = 0.0f;
    return S_OK;
}

static HRESULT WINAPI arm32_app_view_statics3_put_size(
    FakeInspectable *object, Win32Float2 size)
{
    (void)object;
    (void)size;
    return S_OK;
}

static const void *arm32_app_view_statics3_vtable[10] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    arm32_app_view_statics3_get_mode, arm32_app_view_statics3_put_mode,
    arm32_app_view_statics3_get_size, arm32_app_view_statics3_put_size
};
static FakeInspectable arm32_app_view_statics3 = {
    arm32_app_view_statics3_vtable, 1, FAKE_APPLICATION_VIEW_FACTORY
};

/* --- ICurrentApp2Statics (slot 6 GetCustomerPurchaseIdAsync, slot 7 GetCustomerCollectionsIdAsync) --- */
static FakeInspectable arm32_empty_string_async = {
    async_operation_vtable, 1, ARM32_FAKE_KIND_EMPTY_STRING_ASYNC
};

static HRESULT WINAPI arm32_currentapp2_customer_id(
    FakeInspectable *object, HSTRING token, HSTRING ticket, void **operation)
{
    (void)object;
    (void)token;
    (void)ticket;
    if (!operation) return E_POINTER;
    *operation = &arm32_empty_string_async;
    fake_add_ref(&arm32_empty_string_async);
    log_line("ARM32 CurrentApp2 GetCustomerPurchaseId/CollectionsId answered with a completed empty-string operation");
    return S_OK;
}

static const void *arm32_currentapp2_vtable[8] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    arm32_currentapp2_customer_id, arm32_currentapp2_customer_id
};
static FakeInspectable arm32_currentapp2_statics = {
    arm32_currentapp2_vtable, 1, FAKE_CURRENTAPP_FACTORY
};

/* --- Windows.UI.ViewManagement.InputPane ---
 * The ARM AppPlatform_Winrt constructor (0x581828 -> InputAdapter hideKeyboard 0x586ed0) UNCONDITIONALLY posts a
 * RunAsync lambda (0x586f8c) that calls InputPane::GetForCurrentView() + IInputPane2::TryHide() and, only afterwards, clears a
 * flag.  It therefore runs on the first drain_dispatcher_queue() before the first frame; the show-keyboard twin (0x586cc4,
 * IInputPane2::TryShow) and the occluded-rect query (0x57e2e8, IInputPane slot 10) run whenever a text box gets focus.  The real
 * InputPane has no view on a plain HWND thread, so GetForCurrentView fails and the game turns the failure into a C++/CX
 * COMException thrown inside the host's dispatcher drain.  This fake answers all of it harmlessly.
 *   IInputPaneStatics 95F4AF3A: slot 6 GetForCurrentView(InputPane**)
 *   IInputPane  : 6 add_Showing(handler, token*), 7 remove_Showing(token), 8 add_Hiding, 9 remove_Hiding, 10 get_OccludedRect(Rect*)
 *   IInputPane2 8A6B3F26-7090-4793-944C-C3F2CDE26276: 6 TryShow(bool*), 7 TryHide(bool*) */
static HRESULT WINAPI arm32_input_pane_query_interface(
    FakeInspectable *object, REFIID iid, void **result);

static HRESULT WINAPI arm32_input_pane_get_for_current_view(
    FakeInspectable *object, void **result);

/* The game registers one TypedEventHandler<InputPane, InputPaneVisibilityEventArgs> for Showing (add_Showing, slot 6; body 0x57e2e8:
 * args->put_EnsuredFocusedElementInView(true), InputPane::OccludedRect, keyboard height lambda posted to the ClientInstance) and one for
 * Hiding (add_Hiding, slot 8; body 0x57e398: put_EnsuredFocusedElementInView(true), height 0).  They are stored here (AddRef'd delegates)
 * and raised by arm32_input_pane_raise when the real keyboard appears / disappears, so the chat box moves up and back down. */
#define ARM32_INPUT_PANE_MAX_HANDLERS 4
static void *arm32_input_pane_handlers[2][ARM32_INPUT_PANE_MAX_HANDLERS];   /* [0] Showing, [1] Hiding */

static HRESULT arm32_input_pane_add_handler(unsigned hiding, void *handler, LONGLONG *token)
{
    unsigned index;
    char line[160];

    if (!token) return E_POINTER;
    *token = 0;
    if (!handler) return S_OK;
    for (index = 0; index < ARM32_INPUT_PANE_MAX_HANDLERS; ++index) {
        if (!arm32_input_pane_handlers[hiding][index]) {
            ((ULONG (WINAPI *)(void *))((void **)*(void **)handler)[1])(handler);
            arm32_input_pane_handlers[hiding][index] = handler;
            *token = (LONGLONG)(index + 1);
            wsprintfA(line, "ARM32 InputPane.add_%s: game handler %p stored (token %u)", hiding ? "Hiding" : "Showing", handler, index + 1);
            log_line(line);
            return S_OK;
        }
    }
    *token = 100;
    log_line("ARM32 InputPane: too many Showing/Hiding handlers - extra handler ignored");
    return S_OK;
}

static HRESULT arm32_input_pane_remove_handler(unsigned hiding, LONGLONG token)
{
    if (token >= 1 && token <= ARM32_INPUT_PANE_MAX_HANDLERS) {
        void *handler = arm32_input_pane_handlers[hiding][token - 1];
        if (handler) {
            arm32_input_pane_handlers[hiding][token - 1] = NULL;
            ((ULONG (WINAPI *)(void *))((void **)*(void **)handler)[2])(handler);
        }
    }
    return S_OK;
}

static HRESULT WINAPI arm32_input_pane_add_showing(FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    return arm32_input_pane_add_handler(0, handler, token);
}

static HRESULT WINAPI arm32_input_pane_remove_showing(FakeInspectable *object, LONGLONG token)
{
    (void)object;
    return arm32_input_pane_remove_handler(0, token);
}

static HRESULT WINAPI arm32_input_pane_add_hiding(FakeInspectable *object, void *handler, LONGLONG *token)
{
    (void)object;
    return arm32_input_pane_add_handler(1, handler, token);
}

static HRESULT WINAPI arm32_input_pane_remove_hiding(FakeInspectable *object, LONGLONG token)
{
    (void)object;
    return arm32_input_pane_remove_handler(1, token);
}

static HRESULT WINAPI arm32_input_pane_occluded_rect(
    FakeInspectable *object, float *rect)
{
    (void)object;
    if (!rect) return E_POINTER;
    rect[0] = 0.0f;
    rect[1] = 0.0f;
    rect[2] = 0.0f;
    rect[3] = 0.0f;
    /* the Windows touch keyboard (when the host opened it) occludes the bottom of the window: report it in buffer pixels - but not while the finger
     * that opened it is still down (the game would move the chat box under the finger, read the lift as a tap outside it and close it again) */
    arm32_osk_rect_for_game(rect);
    return S_OK;
}

/* TryShow / TryHide (IInputPane2 slots 6 / 7).  They stay "accepted" for the game; the host additionally drives the Windows touch
 * keyboard (TabTip) when touch is the active input device (arm32_osk_show / arm32_osk_hide, "ARM32 osk:" log lines). */
static HRESULT WINAPI arm32_input_pane_try_show(
    FakeInspectable *object, BYTE *result)
{
    static volatile LONG count;
    (void)object;
    if (!result) return E_POINTER;
    *result = TRUE;
    if (InterlockedIncrement(&count) <= 8) {
        log_line("ARM32 InputPane.TryShow accepted (fake input pane; the Windows touch keyboard is driven by the host, see ARM32 osk lines)");
    }
    arm32_osk_show("InputPane.TryShow");
    return S_OK;
}

static HRESULT WINAPI arm32_input_pane_try_hide(
    FakeInspectable *object, BYTE *result)
{
    static volatile LONG count;
    (void)object;
    if (!result) return E_POINTER;
    *result = TRUE;
    if (InterlockedIncrement(&count) <= 8) {
        log_line("ARM32 InputPane.TryHide accepted (fake input pane)");
    }
    arm32_osk_hide("InputPane.TryHide");
    return S_OK;
}

static const void *arm32_input_pane_factory_vtable[7] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    arm32_input_pane_get_for_current_view
};

static const void *arm32_input_pane_vtable[11] = {
    arm32_input_pane_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    arm32_input_pane_add_showing, arm32_input_pane_remove_showing,
    arm32_input_pane_add_hiding, arm32_input_pane_remove_hiding,
    arm32_input_pane_occluded_rect
};

static const void *arm32_input_pane2_vtable[8] = {
    arm32_input_pane_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    arm32_input_pane_try_show, arm32_input_pane_try_hide
};

static FakeInspectable arm32_input_pane_factory = {
    arm32_input_pane_factory_vtable, 1, FAKE_XAML_APPLICATION_FACTORY
};
static FakeInspectable arm32_input_pane = {
    arm32_input_pane_vtable, 1, FAKE_XAML_APPLICATION
};
static FakeInspectable arm32_input_pane2 = {
    arm32_input_pane2_vtable, 1, FAKE_XAML_APPLICATION
};

/* IInputPaneVisibilityEventArgs: 6 get_OccludedRect(Rect*), 7 put_EnsuredFocusedElementInView(bool), 8 get_EnsuredFocusedElementInView(bool*) */
static BYTE arm32_input_pane_ensured;

static HRESULT WINAPI arm32_input_pane_args_occluded_rect(FakeInspectable *object, float *rect)
{
    return arm32_input_pane_occluded_rect(object, rect);
}

static HRESULT WINAPI arm32_input_pane_args_put_ensured(FakeInspectable *object, BYTE value)
{
    (void)object;
    arm32_input_pane_ensured = value;
    return S_OK;
}

static HRESULT WINAPI arm32_input_pane_args_get_ensured(FakeInspectable *object, BYTE *value)
{
    (void)object;
    if (!value) return E_POINTER;
    *value = arm32_input_pane_ensured;
    return S_OK;
}

static const void *arm32_input_pane_args_vtable[9] = {
    fake_query_interface, fake_add_ref, fake_release,
    fake_get_iids, fake_get_runtime_class_name, fake_get_trust_level,
    arm32_input_pane_args_occluded_rect, arm32_input_pane_args_put_ensured, arm32_input_pane_args_get_ensured
};
static FakeInspectable arm32_input_pane_args = {
    arm32_input_pane_args_vtable, 1, FAKE_XAML_APPLICATION
};

/* invoke the delegates the game registered for Showing (TRUE) / Hiding (FALSE): TypedEventHandler::Invoke is slot 3 (sender, args) */
static void arm32_input_pane_raise(BOOL showing)
{
    unsigned slot = showing ? 0u : 1u;
    if (host_is_0104) {   /* 0.10.4: the input unit owns the InputPane objects and the handlers MainPage registered */
        x104_inputpane_raise(showing);
        return;
    }
    unsigned index;
    unsigned called = 0;
    char line[160];

    for (index = 0; index < ARM32_INPUT_PANE_MAX_HANDLERS; ++index) {
        void *handler = arm32_input_pane_handlers[slot][index];
        HRESULT result;
        if (!handler) continue;
        result = ((HRESULT (WINAPI *)(void *, void *, void *))((void **)*(void **)handler)[3])(handler, &arm32_input_pane, &arm32_input_pane_args);
        ++called;
        wsprintfA(line, "ARM32 InputPane %s event raised to the game handler %p hr=0x%08lx", showing ? "Showing" : "Hiding", handler,
                  (unsigned long)result);
        log_line(line);
    }
    if (!called) {
        wsprintfA(line, "ARM32 InputPane %s event: the game registered no handler", showing ? "Showing" : "Hiding");
        log_line(line);
    }
}

static HRESULT WINAPI arm32_input_pane_query_interface(
    FakeInspectable *object, REFIID iid, void **result)
{
    static const GUID input_pane2_iid = {
        0x8a6b3f26, 0x7090, 0x4793,
        {0x94, 0x4c, 0xc3, 0xf2, 0xcd, 0xe2, 0x62, 0x76}
    };
    FakeInspectable *target = &arm32_input_pane;
    (void)object;
    if (!result) return E_POINTER;
    if (iid && memcmp(iid, &input_pane2_iid, sizeof(GUID)) == 0) {
        target = &arm32_input_pane2;
    }
    *result = target;
    fake_add_ref(target);
    return S_OK;
}

static HRESULT WINAPI arm32_input_pane_get_for_current_view(
    FakeInspectable *object, void **result)
{
    (void)object;
    if (!result) return E_POINTER;
    *result = &arm32_input_pane;
    fake_add_ref(&arm32_input_pane);
    return S_OK;
}

static BOOL arm32_activation_override(
    LPCWSTR class_name, const GUID *iid, void **result)
{
    static const GUID application_view_statics3_iid = {
        0xa28d7594, 0x8c41, 0x4e13,
        {0x97, 0x19, 0x51, 0x64, 0x79, 0x6f, 0xe4, 0xc7}
    };
    static const GUID currentapp2_statics_iid = {
        0xdf4e6e2d, 0x3171, 0x4ad3,
        {0x86, 0x14, 0x2c, 0x61, 0x24, 0x43, 0x73, 0xcb}
    };

    static volatile LONG warned_not_128;

    arm32_fill_unimplemented_slots_once();
    if (!host_is_128 && !InterlockedExchange(&warned_not_128, 1)) {
        /* every 1.2.8-only fake (CoreText, AudioGraph, HardwareIdentification, DataReader, ApplicationView, Speech...) is
         * gated on host_is_128; if the image identification failed they silently fall through to the real runtime */
        log_line("ARM32 WARNING: host_is_128 is FALSE during WinRT activation - the 1.2.8 fake WinRT classes are NOT active");
    }
    arm32_log_activation_request(class_name, iid);
    if (!class_name || !iid || !result) return FALSE;
    if (lstrcmpW(class_name, L"Windows.UI.ViewManagement.InputPane") == 0) {
        *result = &arm32_input_pane_factory;
        fake_add_ref(&arm32_input_pane_factory);
        log_line("ARM32 redirected InputPane factory to its fake (no on-screen keyboard on the HWND host)");
        return TRUE;
    }
    if (lstrcmpW(class_name, L"Windows.UI.ViewManagement.ApplicationView") == 0 &&
        memcmp(iid, &application_view_statics3_iid, sizeof(GUID)) == 0) {
        *result = &arm32_app_view_statics3;
        fake_add_ref(&arm32_app_view_statics3);
        log_line("ARM32 redirected ApplicationView IApplicationViewStatics3 to its own fake");
        return TRUE;
    }
    if (lstrcmpW(class_name, L"Windows.ApplicationModel.Store.CurrentApp") == 0 &&
        memcmp(iid, &currentapp2_statics_iid, sizeof(GUID)) == 0) {
        *result = &arm32_currentapp2_statics;
        fake_add_ref(&arm32_currentapp2_statics);
        log_line("ARM32 redirected CurrentApp ICurrentApp2Statics to its own fake");
        return TRUE;
    }
    return FALSE;
}
#endif

#ifdef WIN32CRAFT_ARM32
/* The game asks for the same factories over and over (no caching of the statics): every "redirected ..." line of the
 * function below would be written on every request.  Log the first requests completely, then only the "WinRT fallback"
 * lines (classes the host does NOT fake: the evidence for what the game still needs from the real runtime). */
static volatile LONG arm32_activation_calls;
static void arm32_activation_log_line(const char *text)
{
    if (arm32_activation_calls <= 400) {
        log_line(text);
    } else if (text && (memcmp(text, "WinRT fallback ", 15) == 0 || memcmp(text, "ARM32: ", 7) == 0)) {
        log_line(text);
    }
}
#define log_line arm32_activation_log_line
#endif
static HRESULT WINAPI win32_get_activation_factory(
    LPCWSTR class_name, const GUID *iid, void **result)
{
#ifdef WIN32CRAFT_ARM32
    if (host_is_0104) return x104_activation_hook(class_name, iid, result);   /* never hand out the 1.2.8 fakes to the 0.10.4 exe */
    if (InterlockedIncrement(&arm32_activation_calls) == 401)
        log_line("ARM32: 400 WinRT activation requests logged; from now on only 'WinRT fallback' lines are logged");
#endif
    if (host_is_116 && activation_116(class_name, iid, result)) return S_OK;
    static const GUID currentapp_consumables_iid = {
        0x844e0071, 0x9e4f, 0x4f79,
        {0x99, 0x5a, 0x5f, 0x91, 0x17, 0x2e, 0x6c, 0xef}
    };
    HRESULT activation_result;
    char class_utf8[256];
    char line[512];

    if (!result) return E_POINTER;
    *result = NULL;
#ifdef WIN32CRAFT_ARM32
    if (arm32_activation_override(class_name, iid, result)) return S_OK;
#endif
    if (class_name &&
        lstrcmpW(class_name, L"Windows.ApplicationModel.Package") == 0) {
        log_line("redirected Windows.ApplicationModel.Package");
        *result = &package_factory;
        fake_add_ref(&package_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.Storage.ApplicationData") == 0) {
        log_line("redirected Windows.Storage.ApplicationData");
        *result = &appdata_factory;
        fake_add_ref(&appdata_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.System.Profile.AnalyticsInfo") == 0) {
        log_line("redirected Windows.System.Profile.AnalyticsInfo");
        *result = &analytics_info_factory;
        fake_add_ref(&analytics_info_factory);
        return S_OK;
    }
    if ((host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name,
                 L"Windows.UI.Text.Core.CoreTextServicesManager") == 0) {
        log_line("redirected Windows.UI.Text.Core.CoreTextServicesManager");
        *result = &core_text_factory;
        fake_add_ref(&core_text_factory);
        return S_OK;
    }
    if ((host_is_115 || host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name,
                 L"Windows.ApplicationModel.Resources.Core.ResourceContext") == 0) {
        HRESULT query_result = fake_resource_query_interface(
            &resource_context_activation_factory, iid, result);
        if (FAILED(query_result)) {
            *result = &resource_context_statics2;
            fake_add_ref(&resource_context_statics2);
        }
        log_line("redirected unpackaged ResourceContext factory");
        return S_OK;
    }
    if ((host_is_115 || host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name,
                 L"Windows.ApplicationModel.Resources.Core.ResourceManager") == 0) {
        HRESULT query_result = fake_resource_query_interface(
            &resource_manager_statics, iid, result);
        if (FAILED(query_result)) {
            *result = &resource_manager_statics;
            fake_add_ref(&resource_manager_statics);
        }
        log_line("redirected unpackaged ResourceManager factory");
        return S_OK;
    }
    if ((host_is_128 || (host_is_windows7 && host_is_115)) && class_name &&
        lstrcmpW(class_name,
                 L"Windows.Media.SpeechSynthesis.SpeechSynthesizer") == 0) {
        static const GUID installed_voices_iid = {
            0x7d526ecc, 0x7533, 0x4c3f,
            {0x85, 0xbe, 0x88, 0x8c, 0x2b, 0xae, 0xeb, 0xdc}
        };
        if (iid && memcmp(iid, &installed_voices_iid, sizeof(GUID)) == 0) {
            *result = &speech_voices_factory;
            fake_add_ref(&speech_voices_factory);
            log_line("redirected Win7 SpeechSynthesizer installed voices");
        } else {
            *result = &speech_activation_factory;
            fake_add_ref(&speech_activation_factory);
            log_line("redirected Win7 SpeechSynthesizer activation factory");
        }
        return S_OK;
    }
    if ((host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name,
                 L"Windows.Media.Audio.AudioGraphSettings") == 0) {
        *result = &audio_graph_settings_factory;
        fake_add_ref(&audio_graph_settings_factory);
        log_line("redirected unpackaged AudioGraphSettings factory");
        return S_OK;
    }
    if ((host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name, L"Windows.Media.Audio.AudioGraph") == 0) {
        *result = &audio_graph_factory;
        fake_add_ref(&audio_graph_factory);
        log_line("redirected unpackaged AudioGraph factory");
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.System.MemoryManager") == 0) {
        log_line("redirected Windows.System.MemoryManager");
        *result = &memory_factory;
        fake_add_ref(&memory_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name,
                 L"Windows.ApplicationModel.Store.CurrentApp") == 0) {
        if (iid && memcmp(iid, &currentapp_consumables_iid,
                          sizeof(currentapp_consumables_iid)) == 0) {
            log_line("redirected CurrentApp ICurrentAppWithConsumables");
            *result = &currentapp_consumables;
            fake_add_ref(&currentapp_consumables);
        } else {
            log_line("redirected Windows.ApplicationModel.Store.CurrentApp");
            *result = &currentapp_factory;
            fake_add_ref(&currentapp_factory);
        }
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name,
                 L"Windows.ApplicationModel.Core.CoreApplication") == 0) {
        static const GUID coreapp_exit_iid = {
            0xcf86461d, 0x261e, 0x4b72,
            {0x9a, 0xcd, 0x44, 0xed, 0x2a, 0xce, 0x6a, 0x29}
        };
        if (iid && memcmp(iid, &coreapp_exit_iid,
                          sizeof(coreapp_exit_iid)) == 0) {
            *result = &coreapp_exit;
            fake_add_ref(&coreapp_exit);
            log_line("redirected ICoreApplicationExit to HWND host");
            return S_OK;
        }
        log_line("redirected Windows.ApplicationModel.Core.CoreApplication");
        *result = &coreapp_factory;
        fake_add_ref(&coreapp_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name,
                 L"Windows.Networking.Connectivity.NetworkInformation") == 0) {
        log_line("redirected Windows.Networking.Connectivity.NetworkInformation");
        *result = &network_factory;
        fake_add_ref(&network_factory);
        return S_OK;
    }
    if (host_is_116 && class_name &&
        lstrcmpW(class_name, L"Windows.Gaming.Input.Gamepad") == 0) {
        wsprintfA(line, "redirected Windows.Gaming.Input.Gamepad factory=%p table=%p get=%p",
                  &gamepad_factory, gamepad_factory.vtable,
                  gamepad_factory.vtable ? gamepad_factory.vtable[10] : NULL);
        log_line(line);
        *result = &gamepad_factory;
        fake_add_ref(&gamepad_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name,
                 L"Windows.Security.Cryptography.CryptographicBuffer") == 0) {
        log_line("redirected Windows.Security.Cryptography.CryptographicBuffer");
        *result = &crypto_factory;
        fake_add_ref(&crypto_factory);
        return S_OK;
    }
    if ((host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name,
                 L"Windows.System.Profile.HardwareIdentification") == 0) {
        log_line("redirected Windows.System.Profile.HardwareIdentification");
        *result = &hardware_id_factory;
        fake_add_ref(&hardware_id_factory);
        return S_OK;
    }
    if ((host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name, L"Windows.Storage.Streams.DataReader") == 0) {
        log_line("redirected Windows.Storage.Streams.DataReader");
        *result = &data_reader_factory;
        fake_add_ref(&data_reader_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name,
                 L"Windows.Foundation.Metadata.ApiInformation") == 0) {
        log_line("redirected Windows.Foundation.Metadata.ApiInformation");
        *result = &api_information;
        fake_add_ref(&api_information);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name,
                 L"Windows.Globalization.ApplicationLanguages") == 0) {
        log_line("redirected Windows.Globalization.ApplicationLanguages");
        *result = &application_languages;
        fake_add_ref(&application_languages);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name,
                 L"Windows.Globalization.GeographicRegion") == 0) {
        log_line("redirected Windows.Globalization.GeographicRegion");
        *result = &geographic_region_factory;
        fake_add_ref(&geographic_region_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.UI.Xaml.Application") == 0) {
        log_line("redirected Windows.UI.Xaml.Application");
        *result = &xaml_application_factory;
        fake_add_ref(&xaml_application_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.UI.Xaml.Window") == 0) {
        log_line("redirected Windows.UI.Xaml.Window");
        *result = &xaml_window_factory;
        fake_add_ref(&xaml_window_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name,
                 L"Windows.Graphics.Display.DisplayInformation") == 0) {
        log_line("redirected Windows.Graphics.Display.DisplayInformation");
        *result = &display_information_factory;
        fake_add_ref(&display_information_factory);
        return S_OK;
    }
    if ((host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name,
                 L"Windows.UI.ViewManagement.ApplicationView") == 0) {
        log_line("redirected Windows.UI.ViewManagement.ApplicationView");
        *result = &application_view_factory;
        fake_add_ref(&application_view_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.UI.Core.CoreWindow") == 0) {
        log_line("redirected Windows.UI.Core.CoreWindow");
        *result = &core_window_factory;
        fake_add_ref(&core_window_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.Devices.Input.MouseDevice") == 0) {
        log_line("redirected Windows.Devices.Input.MouseDevice");
        *result = &mouse_device_factory;
        fake_add_ref(&mouse_device_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.System.Threading.ThreadPool") == 0) {
        log_line("redirected Windows.System.Threading.ThreadPool");
        *result = &thread_pool_factory;
        fake_add_ref(&thread_pool_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.System.Launcher") == 0) {
        log_line("redirected Windows.System.Launcher to ShellExecuteW");
        *result = &launcher_factory;
        fake_add_ref(&launcher_factory);
        return S_OK;
    }
    if (class_name &&
        lstrcmpW(class_name, L"Windows.System.LauncherOptions") == 0) {
        log_line("redirected Windows.System.LauncherOptions");
        *result = &launcher_options_factory;
        fake_add_ref(&launcher_options_factory);
        return S_OK;
    }
    if (host_is_115 && class_name &&
        lstrcmpW(class_name, L"Windows.Storage.Pickers.FileSavePicker") == 0) {
        log_line("redirected Windows.Storage.Pickers.FileSavePicker");
        *result = &file_save_picker_factory;
        fake_add_ref(&file_save_picker_factory);
        return S_OK;
    }
    if ((host_is_115 || host_is_128 || host_is_116) && class_name &&
        lstrcmpW(class_name, L"Windows.Storage.Pickers.FileOpenPicker") == 0) {
        log_line("redirected Windows.Storage.Pickers.FileOpenPicker");
        *result = &file_open_picker_factory;
        fake_add_ref(&file_open_picker_factory);
        return S_OK;
    }
    if (host_is_115 && class_name &&
        lstrcmpW(class_name, L"Windows.Storage.CachedFileManager") == 0) {
        log_line("redirected Windows.Storage.CachedFileManager");
        *result = &cached_file_manager;
        fake_add_ref(&cached_file_manager);
        return S_OK;
    }
    if (original_activation) {
        activation_result = original_activation(class_name, iid, result);
        class_utf8[0] = '\0';
        if (class_name) {
            WideCharToMultiByte(CP_UTF8, 0, class_name, -1, class_utf8,
                                sizeof(class_utf8), NULL, NULL);
        }
        if (iid) {
            wsprintfA(line,
                "WinRT fallback %s IID=%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x: HRESULT 0x%08lx",
                class_utf8[0] ? class_utf8 : "(null)",
                (unsigned long)iid->Data1, (unsigned)iid->Data2,
                (unsigned)iid->Data3, (unsigned)iid->Data4[0],
                (unsigned)iid->Data4[1], (unsigned)iid->Data4[2],
                (unsigned)iid->Data4[3], (unsigned)iid->Data4[4],
                (unsigned)iid->Data4[5], (unsigned)iid->Data4[6],
                (unsigned)iid->Data4[7], (unsigned long)activation_result);
        } else {
            wsprintfA(line, "WinRT fallback %s: HRESULT 0x%08lx",
                      class_utf8[0] ? class_utf8 : "(null)",
                      (unsigned long)activation_result);
        }
        log_line(line);
        return activation_result;
    }
    return REGDB_E_CLASSNOTREG;
}
#ifdef WIN32CRAFT_ARM32
#undef log_line
#endif

static BOOL install_activation_redirect(
    BYTE *image, SIZE_T activation_factory_rva)
{
    GameActivationFn *slot =
        (GameActivationFn *)(image + activation_factory_rva);
    DWORD old_protection;
    DWORD ignored;

    package_factory.vtable = factory_vtable;
    package_factory.references = 1;
    package_factory.kind = FAKE_PACKAGE_FACTORY;
    appdata_factory.vtable = factory_vtable;
    appdata_factory.references = 1;
    appdata_factory.kind = FAKE_APPDATA_FACTORY;
    memory_factory.vtable = memory_vtable;
    memory_factory.references = 1;
    memory_factory.kind = FAKE_MEMORY_FACTORY;
    currentapp_factory.vtable = currentapp_factory_vtable;
    currentapp_factory.references = 1;
    currentapp_factory.kind = FAKE_CURRENTAPP_FACTORY;
    currentapp_consumables.vtable = currentapp_consumables_vtable;
    currentapp_consumables.references = 1;
    currentapp_consumables.kind = FAKE_CURRENTAPP_CONSUMABLES;
    coreapp_factory.vtable = coreapp_vtable;
    coreapp_factory.references = 1;
    coreapp_factory.kind = FAKE_COREAPP_FACTORY;
    coreapp_exit.vtable = coreapp_exit_vtable;
    coreapp_exit.references = 1;
    coreapp_exit.kind = FAKE_COREAPP_EXIT;
    package_object.vtable = package_vtable;
    package_object.references = 1;
    package_object.kind = FAKE_PACKAGE;
    appdata_object.vtable = appdata_vtable;
    appdata_object.references = 1;
    appdata_object.kind = FAKE_APPDATA;
    appdata2_object.vtable = appdata2_vtable;
    appdata2_object.references = 1;
    appdata2_object.kind = FAKE_APPDATA2;
    currentapp_object.vtable = currentapp_vtable;
    currentapp_object.references = 1;
    currentapp_object.kind = FAKE_CURRENTAPP;
    license_object.vtable = license_vtable;
    license_object.references = 1;
    license_object.kind = FAKE_LICENSE;
    listing_information.vtable = listing_information_vtable;
    listing_information.references = 1;
    listing_information.kind = FAKE_LISTING_INFORMATION;
    product_listings.vtable = product_listings_vtable;
    product_listings.references = 1;
    product_listings.kind = FAKE_PRODUCT_LISTINGS;
    package_folder.vtable = host_is_115
        ? folder_vtable_115 : folder_vtable_01510;
    package_folder.references = 1;
    package_folder.kind = FAKE_PACKAGE_FOLDER;
    data_folder.vtable = host_is_115
        ? folder_vtable_115 : folder_vtable_01510;
    data_folder.references = 1;
    data_folder.kind = FAKE_DATA_FOLDER;
    network_factory.vtable = network_factory_vtable;
    network_factory.references = 1;
    network_factory.kind = FAKE_NETWORK_FACTORY;
    gamepad_factory.vtable = gamepad_factory_vtable;
    gamepad_factory.references = 1;
    gamepad_factory.kind = FAKE_NETWORK_FACTORY;
    gamepad_vector.vtable = gamepad_vector_vtable;
    gamepad_vector.references = 1;
    gamepad_vector.kind = FAKE_NETWORK_NAMES;
    connection_profile.vtable = connection_profile_vtable;
    connection_profile.references = 1;
    connection_profile.kind = FAKE_CONNECTION_PROFILE;
    connection_profile2.vtable = connection_profile2_vtable;
    connection_profile2.references = 1;
    connection_profile2.kind = FAKE_CONNECTION_PROFILE2;
    connection_cost.vtable = connection_cost_vtable;
    connection_cost.references = 1;
    connection_cost.kind = FAKE_CONNECTION_COST;
    network_names.vtable = network_names_vtable;
    network_names.references = 1;
    network_names.kind = FAKE_NETWORK_NAMES;
    network_names_iterable.vtable = network_names_iterable_vtable;
    network_names_iterable.references = 1;
    network_names_iterable.kind = FAKE_NETWORK_NAMES_ITERABLE;
    network_names_iterator.vtable = network_names_iterator_vtable;
    network_names_iterator.references = 1;
    network_names_iterator.kind = FAKE_NETWORK_NAMES_ITERATOR;
    host_names.vtable = host_names_vtable;
    host_names.references = 1;
    host_names.kind = FAKE_HOST_NAMES;
    host_names_iterable.vtable = host_names_iterable_vtable;
    host_names_iterable.references = 1;
    host_names_iterable.kind = FAKE_HOST_NAMES_ITERABLE;
    host_names_iterator.vtable = host_names_iterator_vtable;
    host_names_iterator.references = 1;
    host_names_iterator.kind = FAKE_HOST_NAMES_ITERATOR;
    host_name.vtable = host_name_vtable;
    host_name.references = 1;
    host_name.kind = FAKE_HOST_NAME;
    lstrcpyW(host_name.text, L"127.0.0.1");
    unfulfilled_vector.vtable = network_names_vtable;
    unfulfilled_vector.references = 1;
    unfulfilled_vector.kind = FAKE_UNFULFILLED_VECTOR;
    crypto_factory.vtable = crypto_factory_vtable;
    crypto_factory.references = 1;
    crypto_factory.kind = FAKE_CRYPTO_FACTORY;
    async_operation.vtable = async_operation_vtable;
    async_operation.references = 1;
    async_operation.kind = FAKE_ASYNC_OPERATION;
    receipt_async_operation.vtable = async_operation_vtable;
    receipt_async_operation.references = 1;
    receipt_async_operation.kind = FAKE_ASYNC_STRING;
    unfulfilled_async_operation.vtable = async_operation_vtable;
    unfulfilled_async_operation.references = 1;
    unfulfilled_async_operation.kind = FAKE_ASYNC_UNFULFILLED;
    api_information.vtable = api_information_vtable;
    api_information.references = 1;
    api_information.kind = FAKE_API_INFORMATION;
    application_languages.vtable = application_languages_vtable;
    application_languages.references = 1;
    application_languages.kind = FAKE_APPLICATION_LANGUAGES;
    language_vector.vtable = language_vector_vtable;
    language_vector.references = 1;
    language_vector.kind = FAKE_LANGUAGE_VECTOR;
    geographic_region_factory.vtable = geographic_region_factory_vtable;
    geographic_region_factory.references = 1;
    geographic_region_factory.kind = FAKE_GEOGRAPHIC_REGION_FACTORY;
    geographic_region.vtable = geographic_region_vtable;
    geographic_region.references = 1;
    geographic_region.kind = FAKE_GEOGRAPHIC_REGION;
    currency_vector.vtable = language_vector_vtable;
    currency_vector.references = 1;
    currency_vector.kind = FAKE_CURRENCY_VECTOR;
    xaml_application_factory.vtable = xaml_application_factory_vtable;
    xaml_application_factory.references = 1;
    xaml_application_factory.kind = FAKE_XAML_APPLICATION_FACTORY;
    xaml_application.vtable = xaml_application_vtable;
    xaml_application.references = 1;
    xaml_application.kind = FAKE_XAML_APPLICATION;
    xaml_window_factory.vtable = xaml_window_factory_vtable;
    xaml_window_factory.references = 1;
    xaml_window_factory.kind = FAKE_XAML_WINDOW_FACTORY;
    xaml_window.vtable = xaml_window_vtable;
    xaml_window.references = 1;
    xaml_window.kind = FAKE_XAML_WINDOW;
    xaml_size_source.vtable = xaml_size_source_vtable;
    xaml_size_source.references = 1;
    xaml_size_source.kind = FAKE_XAML_WINDOW;
    xaml_size_source.callback_264 = NULL;
    display_information_factory.vtable =
        display_information_factory_vtable;
    display_information_factory.references = 1;
    display_information_factory.kind = FAKE_DISPLAY_INFORMATION_FACTORY;
    display_information.vtable = display_information_vtable;
    display_information.references = 1;
    display_information.kind = FAKE_DISPLAY_INFORMATION;
    speech_activation_factory.vtable = speech_activation_factory_vtable;
    speech_activation_factory.references = 1;
    speech_activation_factory.kind = FAKE_SPEECH_ACTIVATION_FACTORY;
    speech_voices_factory.vtable = speech_voices_factory_vtable;
    speech_voices_factory.references = 1;
    speech_voices_factory.kind = FAKE_SPEECH_VOICES_FACTORY;
    speech_synthesizer.vtable = speech_synthesizer_vtable;
    speech_synthesizer.references = 1;
    speech_synthesizer.kind = FAKE_SPEECH_SYNTHESIZER;
    speech_closable.vtable = speech_closable_vtable;
    speech_closable.references = 1;
    speech_closable.kind = FAKE_SPEECH_CLOSABLE;
    speech_synthesizer2.vtable = speech_synthesizer2_vtable;
    speech_synthesizer2.references = 1;
    speech_synthesizer2.kind = FAKE_SPEECH_SYNTHESIZER2;
    speech_options.vtable = speech_options_vtable;
    speech_options.references = 1;
    speech_options.kind = FAKE_SPEECH_OPTIONS;
    speech_voice.vtable = speech_voice_vtable;
    speech_voice.references = 1;
    speech_voice.kind = FAKE_SPEECH_VOICE;
    speech_voice_vector.vtable = speech_voice_vector_vtable;
    speech_voice_vector.references = 1;
    speech_voice_vector.kind = FAKE_SPEECH_VOICE_VECTOR;
    speech_voice_iterable.vtable = speech_voice_iterable_vtable;
    speech_voice_iterable.references = 1;
    speech_voice_iterable.kind = FAKE_SPEECH_VOICE_ITERABLE;
    speech_voice_iterator.vtable = speech_voice_iterator_vtable;
    speech_voice_iterator.references = 1;
    speech_voice_iterator.kind = FAKE_SPEECH_VOICE_ITERATOR;
    speech_async_operation.vtable = async_operation_vtable;
    speech_async_operation.references = 1;
    speech_async_operation.kind = FAKE_ASYNC_SPEECH;
    audio_graph_settings_factory.vtable = audio_graph_settings_factory_vtable;
    audio_graph_settings_factory.references = 1;
    audio_graph_settings_factory.kind = FAKE_AUDIO_GRAPH_SETTINGS_FACTORY;
    audio_graph_settings.vtable = audio_graph_settings_vtable;
    audio_graph_settings.references = 1;
    audio_graph_settings.kind = FAKE_AUDIO_GRAPH_SETTINGS;
    audio_graph_factory.vtable = audio_graph_factory_vtable;
    audio_graph_factory.references = 1;
    audio_graph_factory.kind = FAKE_AUDIO_GRAPH_FACTORY;
    audio_graph_async_operation.vtable = async_operation_vtable;
    audio_graph_async_operation.references = 1;
    audio_graph_async_operation.kind = FAKE_ASYNC_AUDIO_GRAPH;
    audio_graph_create_result.vtable = audio_graph_create_result_vtable;
    audio_graph_create_result.references = 1;
    audio_graph_create_result.kind = FAKE_AUDIO_GRAPH_CREATE_RESULT;
    resource_context_activation_factory.vtable = resource_context_activation_factory_vtable;
    resource_context_activation_factory.references = 1;
    resource_context_activation_factory.kind = FAKE_RESOURCE_CONTEXT_ACTIVATION_FACTORY;
    resource_context_statics.vtable = resource_context_statics_vtable;
    resource_context_statics.references = 1;
    resource_context_statics.kind = FAKE_RESOURCE_CONTEXT_STATICS;
    resource_context_statics2.vtable = resource_context_statics2_vtable;
    resource_context_statics2.references = 1;
    resource_context_statics2.kind = FAKE_RESOURCE_CONTEXT_STATICS2;
    resource_context_statics3.vtable = resource_context_statics3_vtable;
    resource_context_statics3.references = 1;
    resource_context_statics3.kind = FAKE_RESOURCE_CONTEXT_STATICS3;
    resource_context.vtable = resource_context_vtable;
    resource_context.references = 1;
    resource_context.kind = FAKE_RESOURCE_CONTEXT;
    resource_qualifier_map_observable.vtable = resource_qualifier_map_observable_vtable;
    resource_qualifier_map_observable.references = 1;
    resource_qualifier_map_observable.kind = FAKE_RESOURCE_QUALIFIER_MAP_OBSERVABLE;
    resource_qualifier_map.vtable = resource_qualifier_map_vtable;
    resource_qualifier_map.references = 1;
    resource_qualifier_map.kind = FAKE_RESOURCE_QUALIFIER_MAP;
    resource_qualifier_map_view.vtable = resource_qualifier_map_view_vtable;
    resource_qualifier_map_view.references = 1;
    resource_qualifier_map_view.kind = FAKE_RESOURCE_QUALIFIER_MAP_VIEW;
    resource_manager_statics.vtable = resource_manager_statics_vtable;
    resource_manager_statics.references = 1;
    resource_manager_statics.kind = FAKE_RESOURCE_MANAGER_STATICS;
    resource_manager.vtable = resource_manager_vtable;
    resource_manager.references = 1;
    resource_manager.kind = FAKE_RESOURCE_MANAGER;
    resource_map.vtable = resource_map_vtable;
    resource_map.references = 1;
    resource_map.kind = FAKE_RESOURCE_MAP;
    analytics_info_factory.vtable = analytics_info_factory_vtable;
    analytics_info_factory.references = 1;
    analytics_info_factory.kind = FAKE_ANALYTICS_INFO_FACTORY;
    analytics_version_info.vtable = analytics_version_info_vtable;
    analytics_version_info.references = 1;
    analytics_version_info.kind = FAKE_ANALYTICS_VERSION_INFO;
    core_text_factory.vtable = core_text_factory_vtable;
    core_text_factory.references = 1;
    core_text_factory.kind = FAKE_CORE_TEXT_FACTORY;
    core_text_manager.vtable = core_text_manager_vtable;
    core_text_manager.references = 1;
    core_text_manager.kind = FAKE_CORE_TEXT_MANAGER;
    core_text_edit_context.vtable = core_text_edit_context_vtable;
    core_text_edit_context.references = 1;
    core_text_edit_context.kind = FAKE_CORE_TEXT_EDIT_CONTEXT;
    hardware_id_factory.vtable = hardware_id_factory_vtable;
    hardware_id_factory.references = 1;
    hardware_id_factory.kind = FAKE_HARDWARE_ID_FACTORY;
    hardware_token.vtable = hardware_token_vtable;
    hardware_token.references = 1;
    hardware_token.kind = FAKE_HARDWARE_TOKEN;
    data_reader_factory.vtable = data_reader_factory_vtable;
    data_reader_factory.references = 1;
    data_reader_factory.kind = FAKE_DATA_READER_FACTORY;
    data_reader.vtable = data_reader_vtable;
    data_reader.references = 1;
    data_reader.kind = FAKE_DATA_READER;
    data_reader.source = NULL;
    data_reader.position = 0;
    application_view_factory.vtable = application_view_factory_vtable;
    application_view_factory.references = 1;
    application_view_factory.kind = FAKE_APPLICATION_VIEW_FACTORY;
    application_view.vtable = application_view_vtable;
    application_view.references = 1;
    application_view.kind = FAKE_APPLICATION_VIEW;
    application_view3.vtable = application_view3_vtable;
    application_view3.references = 1;
    application_view3.kind = FAKE_APPLICATION_VIEW3;
    launcher_factory.vtable = launcher_vtable;
    launcher_factory.references = 1;
    launcher_factory.kind = FAKE_LAUNCHER_FACTORY;
    launcher_options_factory.vtable = factory_vtable;
    launcher_options_factory.references = 1;
    launcher_options_factory.kind = FAKE_LAUNCHER_OPTIONS_FACTORY;
    launcher_options.vtable = launcher_options_vtable;
    launcher_options.references = 1;
    launcher_options.kind = FAKE_LAUNCHER_OPTIONS;
    launcher_async_operation.vtable = async_operation_vtable;
    launcher_async_operation.references = 1;
    launcher_async_operation.kind = FAKE_ASYNC_BOOL;
    file_save_picker_factory.vtable = factory_vtable;
    file_save_picker_factory.references = 1;
    file_save_picker_factory.kind = FAKE_FILE_SAVE_PICKER_FACTORY;
    file_save_picker.vtable = file_save_picker_vtable;
    file_save_picker.references = 1;
    file_save_picker.kind = FAKE_FILE_SAVE_PICKER;
    file_open_picker_factory.vtable = factory_vtable;
    file_open_picker_factory.references = 1;
    file_open_picker_factory.kind = FAKE_FILE_OPEN_PICKER_FACTORY;
    file_open_picker.vtable = file_open_picker_vtable;
    file_open_picker.references = 1;
    file_open_picker.kind = FAKE_FILE_OPEN_PICKER;
    file_type_choices.vtable = file_type_choices_vtable;
    file_type_choices.references = 1;
    file_type_choices.kind = FAKE_FILE_TYPE_CHOICES;
    file_type_filter.vtable = file_type_filter_vtable;
    file_type_filter.references = 1;
    file_type_filter.kind = FAKE_FILE_TYPE_FILTER;
    file_save_async_operation.vtable = async_operation_vtable;
    file_save_async_operation.references = 1;
    file_save_async_operation.kind = FAKE_ASYNC_STORAGE_FILE;
    file_open_async_operation.vtable = async_operation_vtable;
    file_open_async_operation.references = 1;
    file_open_async_operation.kind = FAKE_ASYNC_STORAGE_FILE;
    storage_file.vtable = storage_file_vtable;
    storage_file.references = 1;
    storage_file.kind = FAKE_STORAGE_FILE;
    storage_item.vtable = storage_item_vtable;
    storage_item.references = 1;
    storage_item.kind = FAKE_STORAGE_ITEM;
    secondary_storage_file.vtable = storage_file_vtable;
    secondary_storage_file.references = 1;
    secondary_storage_file.kind = FAKE_STORAGE_FILE;
    secondary_storage_item.vtable = storage_item_vtable;
    secondary_storage_item.references = 1;
    secondary_storage_item.kind = FAKE_STORAGE_ITEM;
    secondary_storage_async_operation.vtable = async_operation_vtable;
    secondary_storage_async_operation.references = 1;
    secondary_storage_async_operation.kind = FAKE_ASYNC_STORAGE_FILE;
    cached_file_manager.vtable = cached_file_manager_vtable;
    cached_file_manager.references = 1;
    cached_file_manager.kind = FAKE_CACHED_FILE_MANAGER;
    cached_file_async_operation.vtable = async_operation_vtable;
    cached_file_async_operation.references = 1;
    cached_file_async_operation.kind = FAKE_ASYNC_UINT;
    async_info.vtable = async_info_vtable;
    async_info.references = 1;
    async_info.kind = FAKE_ASYNC_INFO;
    export_file_path[0] = 0;
    secondary_storage_path[0] = 0;
    export_suggested_name[0] = 0;
    lstrcpyW(export_default_extension, L"mcworld");
    export_file_selected = FALSE;
    secondary_storage_available = FALSE;
    core_window_factory.vtable = core_window_factory_vtable;
    core_window_factory.references = 1;
    core_window_factory.kind = FAKE_XAML_APPLICATION_FACTORY;
    win32_core_window.vtable = win32_core_window_vtable;
    win32_core_window.references = 1;
    win32_core_window.kind = FAKE_XAML_APPLICATION;
    win32_core_dispatcher.vtable = win32_core_dispatcher_vtable;
    win32_core_dispatcher.references = 1;
    win32_core_dispatcher.kind = FAKE_XAML_APPLICATION;
    mouse_device_factory.vtable = mouse_device_factory_vtable;
    mouse_device_factory.references = 1;
    mouse_device_factory.kind = FAKE_XAML_APPLICATION_FACTORY;
    mouse_device.vtable = mouse_device_vtable;
    mouse_device.references = 1;
    mouse_device.kind = FAKE_XAML_APPLICATION;
    thread_pool_factory.vtable = thread_pool_factory_vtable;
    thread_pool_factory.references = 1;
    thread_pool_factory.kind = FAKE_XAML_APPLICATION_FACTORY;

#ifdef WIN32CRAFT_ARM32
    /* facts pe_imports.activation_factory_iat: vccorlib GetActivationFactoryByPCWSTR (LPCWSTR, GUID&, void**) is the
     * ONLY WinRT activation path of the ARM exe; same host dispatcher as x86, every other class goes to the original. */
    (void)slot;
    (void)old_protection;
    (void)ignored;
    if (!arm32_iat_hook_import(image, FALSE, "?GetActivationFactoryByPCWSTR@@YAJPAXAAVGuid@Platform@@PAPAX@Z",
                           (DWORD)activation_factory_rva, (void *)win32_get_activation_factory,
                           (void **)&original_activation, "GetActivationFactoryByPCWSTR")) {
        log_line("ARM32: WinRT activation redirect NOT installed");
        return FALSE;
    }
    log_line("ARM32: WinRT activation redirect installed (host fakes first, vccorlib for every other class)");
    return TRUE;
}
#else
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protection)) {
        log_line("VirtualProtect activation IAT failed");
        return FALSE;
    }
    original_activation = *slot;
    *slot = win32_get_activation_factory;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    log_line("WinRT Package/ApplicationData redirect installed");
    return TRUE;
}
#endif

static void __cdecl win32_platform_object_ctor(void *object)
{
    /*
     * Wine's vccorlib140 implementation exports this C++/CX constructor as
     * an aborting stub.  The generated callers immediately install all
     * object/interface vtables themselves; the runtime constructor has no
     * state that these native objects need.
     */
    (void)object;
}

static void *WINAPI win32_get_ibox_vtable(void *object)
{
    /*
     * The compiler-generated boxed value/array constructors place their
     * concrete IBox vtable at object+4 before calling vccorlib. Wine's
     * helper aborts, while the native helper returns that projection.
     */
    return object ? ((void **)object)[1] : NULL;
}

static void *__cdecl win32_platform_allocate1(UINT size)
{
    void *memory = original_platform_allocate1(size);
    if (memory) ZeroMemory(memory, size);
    return memory;
}

static void *__cdecl win32_platform_allocate2(UINT object_size, UINT allocation_size)
{
    void *memory = original_platform_allocate2(object_size, allocation_size);
    char line[128];
    /*
     * Native vccorlib returns clean C++/CX object storage. Wine's allocator
     * currently forwards to malloc and leaves stale pointers in members that
     * generated constructors assume are null.
     */
    if (memory) {
        /*
         * The final allocation_size-object_size bytes belong to vccorlib's
         * reference-tracker cookie.  Clearing them destroys the hidden
         * control-block pointer.  Only initialise the public object storage.
         */
        ZeroMemory(memory, object_size);
    }
    wsprintfA(line,
        "Platform::Heap::Allocate(%u, %u) -> %p",
        object_size, allocation_size, memory);
    log_line(line);
    return memory;
}

static BOOL install_vccorlib_compat(
    BYTE *image, SIZE_T object_ctor_rva, SIZE_T allocate2_rva,
    SIZE_T allocate1_rva, SIZE_T ibox_array_rva, SIZE_T ibox_rva)
{
    GamePlatformObjectCtorFn *slot =
        (GamePlatformObjectCtorFn *)(image + object_ctor_rva);
    GamePlatformAllocate2Fn *allocate2_slot =
        (GamePlatformAllocate2Fn *)(image + allocate2_rva);
    GamePlatformAllocate1Fn *allocate1_slot =
        (GamePlatformAllocate1Fn *)(image + allocate1_rva);
    void **array_slot = (void **)(image + ibox_array_rva);
    void **box_slot = (void **)(image + ibox_rva);
    SIZE_T first_rva = object_ctor_rva;
    SIZE_T last_rva = object_ctor_rva;
    DWORD old_protection;
    DWORD ignored;
    SIZE_T protected_size;

#define INCLUDE_VCCORLIB_RVA(value) \
    do { \
        if ((value) < first_rva) first_rva = (value); \
        if ((value) > last_rva) last_rva = (value); \
    } while (0)
    INCLUDE_VCCORLIB_RVA(allocate2_rva);
    INCLUDE_VCCORLIB_RVA(allocate1_rva);
    INCLUDE_VCCORLIB_RVA(ibox_array_rva);
    INCLUDE_VCCORLIB_RVA(ibox_rva);
#undef INCLUDE_VCCORLIB_RVA
    protected_size = last_rva - first_rva + sizeof(void *);
#ifdef WIN32CRAFT_ARM32
    /* facts pe_imports.vccorlib_hooks_necessity (+ verdict): the desktop ARM vccorlib140.dll implements all five imports.
     * The two IBox hooks would REPLACE the native vtable result that generated code stores to obj+4 and the Object ctor
     * no-op skips a native vtable store, so none of them may be installed on ARM.  This function is not called there. */
    (void)slot;
    (void)allocate2_slot;
    (void)allocate1_slot;
    (void)array_slot;
    (void)box_slot;
    (void)old_protection;
    (void)ignored;
    (void)protected_size;
    log_line("ARM32: install_vccorlib_compat refused (native vccorlib140 serves all five imports)");
    return FALSE;
#endif

    if (!VirtualProtect(
            image + first_rva, protected_size, PAGE_READWRITE,
            &old_protection)) {
        log_line("vccorlib compatibility IAT protection failed");
        return FALSE;
    }
    original_platform_object_ctor = *slot;
    original_platform_allocate2 = *allocate2_slot;
    original_platform_allocate1 = *allocate1_slot;
    *slot = win32_platform_object_ctor;
    *allocate2_slot = win32_platform_allocate2;
    *allocate1_slot = win32_platform_allocate1;
    *array_slot = win32_get_ibox_vtable;
    *box_slot = win32_get_ibox_vtable;
    VirtualProtect(
        image + first_rva, protected_size, old_protection, &ignored);
    FlushInstructionCache(
        GetCurrentProcess(), image + first_rva, protected_size);
    log_line("installed native Platform::Object/IBox/allocator compatibility shims");
    return TRUE;
}

static void copy_ascii_sample(char *destination, UINT capacity,
                              const char *source)
{
    UINT index = 0;
    if (!destination || !capacity) return;
    if (!source || IsBadReadPtr(source, 1)) {
        destination[0] = 0;
        return;
    }
    while (index + 1 < capacity && !IsBadReadPtr(source + index, 1) &&
           source[index]) {
        unsigned char value = (unsigned char)source[index];
        destination[index] = (value >= 0x20 && value < 0x7f)
            ? (char)value : '.';
        ++index;
    }
    destination[index] = 0;
}


#define COPYRIGHT_OVERRIDE_TEXT_A "Not affiliated with Mojang AB"
#define COPYRIGHT_OVERRIDE_LINE_A "menu.copyright=" COPYRIGHT_OVERRIDE_TEXT_A
#define DEVELOPMENT_OVERRIDE_TEXT_A "github.com/deltaxur/Win32Craft"
#define DEVELOPMENT_OVERRIDE_LINE_A "development_version=" DEVELOPMENT_OVERRIDE_TEXT_A
#define COPYRIGHT_TEMP_FILE_LIMIT 16
#define COPYRIGHT_MAX_LANG_BYTES (32U * 1024U * 1024U)

static wchar_t copyright_temp_files[COPYRIGHT_TEMP_FILE_LIMIT][MAX_PATH];
static volatile LONG copyright_temp_sequence;

static WCHAR copyright_ascii_lower(WCHAR value)
{
    if (value >= L'A' && value <= L'Z') return (WCHAR)(value + (L'a' - L'A'));
    return value;
}

static BOOL copyright_is_lang_path_w(const wchar_t *path)
{
    int length;
    if (!path || IsBadReadPtr(path, sizeof(*path))) return FALSE;
    length = lstrlenW(path);
    if (length < 5) return FALSE;
    return path[length - 5] == L'.' &&
           copyright_ascii_lower(path[length - 4]) == L'l' &&
           copyright_ascii_lower(path[length - 3]) == L'a' &&
           copyright_ascii_lower(path[length - 2]) == L'n' &&
           copyright_ascii_lower(path[length - 1]) == L'g';
}

static BOOL copyright_mode_reads_a(const char *mode)
{
    return mode && !IsBadReadPtr(mode, 1) && mode[0] == 'r';
}

static BOOL copyright_mode_reads_w(const wchar_t *mode)
{
    return mode && !IsBadReadPtr(mode, sizeof(*mode)) && mode[0] == L'r';
}

static void copyright_remember_temp(const wchar_t *path)
{
    LONG sequence = InterlockedIncrement(&copyright_temp_sequence) - 1;
    UINT slot = (UINT)sequence % COPYRIGHT_TEMP_FILE_LIMIT;
    if (copyright_temp_files[slot][0])
        DeleteFileW(copyright_temp_files[slot]);
    lstrcpyW(copyright_temp_files[slot], path);
}

void Win32CraftCleanupCopyrightTemps(void)
{
    UINT index;
    for (index = 0; index < COPYRIGHT_TEMP_FILE_LIMIT; ++index) {
        if (copyright_temp_files[index][0]) {
            DeleteFileW(copyright_temp_files[index]);
            copyright_temp_files[index][0] = 0;
        }
    }
}

static BOOL lang_apply_line_override(
    const BYTE *input, DWORD size,
    const BYTE *key, DWORD key_size,
    const BYTE *replacement, DWORD replacement_size,
    BYTE **output, DWORD *output_size)
{
    DWORD line_start = 0xffffffffU;
    DWORD line_end = 0;
    DWORD index;
    DWORD prefix_size;
    DWORD suffix_size;
    BOOL need_newline;
    BYTE *result;
    DWORD result_size;

    if (!output || !output_size || !key || !key_size ||
        !replacement || !replacement_size)
        return FALSE;
    *output = NULL;
    *output_size = 0;

    for (index = 0; index + key_size <= size; ++index) {
        BOOL at_line_start = index == 0 || input[index - 1] == '\n';
        if (index == 3 && size >= 3 && input[0] == 0xef &&
            input[1] == 0xbb && input[2] == 0xbf)
            at_line_start = TRUE;
        if (at_line_start && memcmp(input + index, key, key_size) == 0) {
            line_start = index;
            line_end = index + key_size;
            while (line_end < size && input[line_end] != '\r' &&
                   input[line_end] != '\n')
                ++line_end;
            break;
        }
    }

    if (line_start != 0xffffffffU) {
        prefix_size = line_start;
        suffix_size = size - line_end;
        result_size = prefix_size + replacement_size + suffix_size;
        result = (BYTE *)HeapAlloc(
            GetProcessHeap(), 0, result_size ? result_size : 1);
        if (!result) return FALSE;
        if (prefix_size) memcpy(result, input, prefix_size);
        memcpy(result + prefix_size, replacement, replacement_size);
        if (suffix_size)
            memcpy(result + prefix_size + replacement_size,
                   input + line_end, suffix_size);
    } else {
        need_newline = size && input[size - 1] != '\n' &&
                       input[size - 1] != '\r';
        result_size = size + (need_newline ? 2U : 0U) +
                      replacement_size + 2U;
        result = (BYTE *)HeapAlloc(GetProcessHeap(), 0, result_size);
        if (!result) return FALSE;
        if (size) memcpy(result, input, size);
        index = size;
        if (need_newline) {
            result[index++] = '\r';
            result[index++] = '\n';
        }
        memcpy(result + index, replacement, replacement_size);
        index += replacement_size;
        result[index++] = '\r';
        result[index++] = '\n';
        result_size = index;
    }

    *output = result;
    *output_size = result_size;
    return TRUE;
}

typedef struct GameStringX86 {
    union {
        char small[16];
        char *heap;
    } storage;
    DWORD length;
    DWORD capacity;
} GameStringX86;

typedef BOOL (__attribute__((thiscall)) *Game128ZipGetContentsFn)(
    void *, const GameStringX86 *, GameStringX86 *, BOOL);
typedef void *(__attribute__((thiscall)) *Game128StringAssignFn)(
    GameStringX86 *, const char *, size_t);

static Game128ZipGetContentsFn original_128_zip_get_contents;
static BOOL start_screen_force_development_control(
    const BYTE *input, DWORD size, BYTE **output, DWORD *output_size);

static const char *game_string_data(const GameStringX86 *value)
{
    if (!value || IsBadReadPtr(value, sizeof(*value))) return NULL;
    return value->capacity < 16 ? value->storage.small : value->storage.heap;
}

static BOOL resource_path_has_suffix(const GameStringX86 *path,
                                     const char *suffix)
{
    const char *text = game_string_data(path);
    size_t suffix_length = 0;
    size_t start;
    size_t index;
    while (suffix[suffix_length]) ++suffix_length;
    if (!text || path->length < suffix_length ||
        IsBadReadPtr(text, path->length)) return FALSE;
    start = path->length - suffix_length;
    for (index = 0; index < suffix_length; ++index) {
        char left = text[start + index];
        char right = suffix[index];
        if (left == '\\') left = '/';
        if (right == '\\') right = '/';
        if (left >= 'A' && left <= 'Z') left += 'a' - 'A';
        if (right >= 'A' && right <= 'Z') right += 'a' - 'A';
        if (left != right) return FALSE;
    }
    return TRUE;
}

#ifdef WIN32CRAFT_ARM32
/* ARM32 resource overrides (facts platform_fixes.zip_get_contents_fn / directory_get_contents_slot / string_assign_fn).
 * PackAccessStrategy::getAsset (vtable slot 11) is a plain AAPCS call:
 *   BOOL getAsset(void *this /r0/, const std::string *path /r1/, std::string *out /r2/, BOOL requireValidPack /r3/)
 * The ARM package ships loose files, so DirectoryPackAccessStrategy (original 0x6314BC, cell 0x0136DF2C) is the likely live
 * path; the two zip vtables (original 0x64E6B8, cells 0x0136E598 / 0x0136E5E8) are kept for parity with x86.  The strategies
 * have DIFFERENT originals, so there is one thin wrapper per original.  The direct bl callers of 0x64E6B8 (0x9389A inside
 * sub_936B0 and 0x93BA6 inside sub_939B4) belong to MinecraftUnitTest code (FixedKeyProvider/MockFileAccess, "validfile.txt",
 * "stuff/more_stuff.json") that only ever runs from the test runner, so the cell patches cover every real path.
 * DirectoryPackWithEncryptionAccessStrategy[11] (cell 0x0136E004, original 0x6323F0) is deliberately not hooked: the vanilla
 * pack is not encrypted (plain UTF-8 en_US.lang / start_screen.json in the package); only its cell value is logged. */
typedef BOOL (*Arm32GetAssetFn)(void *, const GameStringX86 *, GameStringX86 *, BOOL);
typedef void *(*Arm32StringAssignFn)(GameStringX86 *, const char *, size_t);

static Arm32GetAssetFn arm32_original_zip_get_asset;
static Arm32GetAssetFn arm32_original_directory_get_asset;
static Arm32StringAssignFn arm32_string_assign;
static int arm32_get_asset_probe_budget = 16;     /* first calls are logged: shows which strategy serves which path */
static int arm32_get_asset_override_budget = 24;  /* rewritten .lang/start_screen.json results are logged */

static BOOL arm32_get_asset_override(Arm32GetAssetFn original, const char *kind, void *self,
                                     const GameStringX86 *path, GameStringX86 *output,
                                     BOOL require_valid_pack)
{
    static const BYTE copyright_key[] = "menu.copyright=";
    static const BYTE copyright_replacement[] = COPYRIGHT_OVERRIDE_LINE_A;
    static const BYTE development_key[] = "development_version=";
    static const BYTE development_replacement[] = DEVELOPMENT_OVERRIDE_LINE_A;
    BYTE *copyright_output = NULL;
    BYTE *final_output = NULL;
    BYTE *start_screen_output = NULL;
    DWORD copyright_size = 0;
    DWORD final_size = 0;
    DWORD start_screen_size = 0;
    DWORD original_size;
    const char *contents;
    BOOL result;
    BOOL is_start_screen;
    BOOL is_lang;
    char sample[96];
    char line[256];

    if (!original) return FALSE;
    result = original(self, path, output, require_valid_pack);
    if (arm32_get_asset_probe_budget > 0) {
        --arm32_get_asset_probe_budget;
        copy_ascii_sample(sample, sizeof(sample), game_string_data(path));
        wsprintfA(line, "ARM32 getAsset[%s] this=%p path='%s' result=%d out_length=%lu", kind, self, sample,
                  (int)result,
                  (result && output && !IsBadReadPtr(output, sizeof(*output))) ? (unsigned long)output->length : 0ul);
        log_line(line);
    }
    if (!result || !output || !arm32_string_assign) return result;

    is_start_screen = resource_path_has_suffix(path, "start_screen.json");
    is_lang = !is_start_screen && resource_path_has_suffix(path, ".lang");
    if (!is_start_screen && !is_lang) return result;

    contents = game_string_data(output);
    if (!contents || !output->length || IsBadReadPtr(contents, output->length)) {
        log_line("ARM32 getAsset override skipped: output string empty or unreadable");
        return result;
    }
    original_size = output->length;

    if (is_start_screen) {
        if (start_screen_force_development_control((const BYTE *)contents, original_size,
                                                   &start_screen_output, &start_screen_size)) {
            arm32_string_assign(output, (const char *)start_screen_output, start_screen_size);
            HeapFree(GetProcessHeap(), 0, start_screen_output);
            if (arm32_get_asset_override_budget > 0) {
                --arm32_get_asset_override_budget;
                copy_ascii_sample(sample, sizeof(sample), game_string_data(path));
                wsprintfA(line, "ARM32 getAsset[%s] start_screen override applied to '%s': %lu -> %lu bytes",
                          kind, sample, (unsigned long)original_size, (unsigned long)output->length);
                log_line(line);
            }
        } else if (arm32_get_asset_override_budget > 0) {
            --arm32_get_asset_override_budget;
            copy_ascii_sample(sample, sizeof(sample), game_string_data(path));
            wsprintfA(line, "ARM32 getAsset[%s] start_screen rewrite found nothing to change in '%s' (%lu bytes)",
                      kind, sample, (unsigned long)original_size);
            log_line(line);
        }
        return result;
    }

    if (!lang_apply_line_override((const BYTE *)contents, original_size,
                                  copyright_key, sizeof(copyright_key) - 1,
                                  copyright_replacement, sizeof(copyright_replacement) - 1,
                                  &copyright_output, &copyright_size)) {
        log_line("ARM32 getAsset lang override: menu.copyright rewrite failed (out of memory?)");
        return result;
    }
    if (lang_apply_line_override(copyright_output, copyright_size,
                                 development_key, sizeof(development_key) - 1,
                                 development_replacement, sizeof(development_replacement) - 1,
                                 &final_output, &final_size)) {
        arm32_string_assign(output, (const char *)final_output, final_size);
        HeapFree(GetProcessHeap(), 0, final_output);
    } else {
        /* x86 dropped the copyright change here; keeping it costs nothing */
        arm32_string_assign(output, (const char *)copyright_output, copyright_size);
    }
    HeapFree(GetProcessHeap(), 0, copyright_output);
    if (arm32_get_asset_override_budget > 0) {
        --arm32_get_asset_override_budget;
        copy_ascii_sample(sample, sizeof(sample), game_string_data(path));
        wsprintfA(line, "ARM32 getAsset[%s] .lang override applied to '%s': %lu -> %lu bytes",
                  kind, sample, (unsigned long)original_size, (unsigned long)output->length);
        log_line(line);
    }
    return result;
}

static BOOL win32_128_zip_get_contents(void *self, const GameStringX86 *path, GameStringX86 *output,
                                       BOOL require_valid_pack)
{
    return arm32_get_asset_override(arm32_original_zip_get_asset, "zip", self, path, output, require_valid_pack);
}

static BOOL win32_128_directory_get_contents(void *self, const GameStringX86 *path, GameStringX86 *output,
                                             BOOL require_valid_pack)
{
    return arm32_get_asset_override(arm32_original_directory_get_asset, "dir", self, path, output,
                                    require_valid_pack);
}

/* Swap one strategy's slot-11 cell.  The original is recorded BEFORE the cell is written so a call arriving right after
 * the store already finds it.  A cell that already holds the hook counts as installed. */
static BOOL patch_128_zip_slot(BYTE *image, SIZE_T slot_rva, SIZE_T function_rva, const BYTE *function_bytes,
                               SIZE_T function_bytes_size, void *hook, Arm32GetAssetFn *original_slot,
                               const char *what)
{
    void **slot = (void **)(image + slot_rva);
    void *expected = ARM_CODE(image, function_rva);
    char line[200];

    if (IsBadReadPtr(slot, sizeof(*slot))) {
        wsprintfA(line, "ARM32 %s: cell %p is unreadable", what, (void *)slot);
        log_line(line);
        return FALSE;
    }
    if (*slot == hook) {
        wsprintfA(line, "ARM32 %s: cell %p already holds the override", what, (void *)slot);
        log_line(line);
        return TRUE;
    }
    if (!arm32_check_bytes(image + function_rva, function_bytes, function_bytes_size, what)) return FALSE;
    *original_slot = (Arm32GetAssetFn)expected;
    return arm32_patch_cell(slot, expected, hook, NULL, what);
}

static BOOL install_128_zip_resource_overrides(BYTE *image)
{
    BOOL directory_ok;
    BOOL zip_plain_ok;
    BOOL zip_owning_ok;
    void **encrypted_cell;
    char line[200];

    if (!image) return FALSE;
    encrypted_cell = (void **)(image + ARM_PLATFORM_FIXES_DIRECTORY_ENCRYPTED_GET_CONTENTS_SLOT);
    arm32_string_assign = (Arm32StringAssignFn)ARM_CODE(image, ARM_PLATFORM_FIXES_STRING_ASSIGN_FN);
    if (!arm32_check_bytes(image + ARM_PLATFORM_FIXES_STRING_ASSIGN_FN,
                           ARM_PLATFORM_FIXES_STRING_ASSIGN_FN_EXPECTED,
                           sizeof(ARM_PLATFORM_FIXES_STRING_ASSIGN_FN_EXPECTED), "std::string::assign(ptr,len)")) {
        arm32_string_assign = NULL;
        log_line("Minecraft 1.2.8 ARM32 getAsset overrides not installed: std::string::assign signature mismatch");
        return FALSE;
    }
    directory_ok = patch_128_zip_slot(image, ARM_PLATFORM_FIXES_DIRECTORY_GET_CONTENTS_SLOT,
                                      0x006314bc /* DirectoryPackAccessStrategy::getAsset (facts) */,
                                      ARM_PLATFORM_FIXES_DIRECTORY_GET_CONTENTS_SLOT_EXPECTED,
                                      sizeof(ARM_PLATFORM_FIXES_DIRECTORY_GET_CONTENTS_SLOT_EXPECTED),
                                      (void *)win32_128_directory_get_contents,
                                      &arm32_original_directory_get_asset,
                                      "DirectoryPackAccessStrategy::getAsset vtable[11]");
    zip_plain_ok = patch_128_zip_slot(image, ARM_PLATFORM_FIXES_ZIP_PLAIN_SLOT_CELL,
                                      ARM_PLATFORM_FIXES_ZIP_GET_CONTENTS_FN,
                                      ARM_PLATFORM_FIXES_ZIP_GET_CONTENTS_FN_EXPECTED,
                                      sizeof(ARM_PLATFORM_FIXES_ZIP_GET_CONTENTS_FN_EXPECTED),
                                      (void *)win32_128_zip_get_contents, &arm32_original_zip_get_asset,
                                      "ZipPackAccessStrategy::getAsset vtable[11]");
    zip_owning_ok = patch_128_zip_slot(image, ARM_PLATFORM_FIXES_ZIP_OWNING_SLOT_CELL,
                                       ARM_PLATFORM_FIXES_ZIP_GET_CONTENTS_FN,
                                       ARM_PLATFORM_FIXES_ZIP_GET_CONTENTS_FN_EXPECTED,
                                       sizeof(ARM_PLATFORM_FIXES_ZIP_GET_CONTENTS_FN_EXPECTED),
                                       (void *)win32_128_zip_get_contents, &arm32_original_zip_get_asset,
                                       "ZipPackAccessStrategyOwningFileAcccess::getAsset vtable[11]");
    if (!IsBadReadPtr(encrypted_cell, sizeof(*encrypted_cell))) {
        wsprintfA(line, "ARM32 DirectoryPackWithEncryptionAccessStrategy getAsset cell %p = %p (left unhooked)",
                  (void *)encrypted_cell, *encrypted_cell);
        log_line(line);
    }
    wsprintfA(line, "Minecraft 1.2.8 ARM32 getAsset overrides: directory=%s zip=%s zip-owning=%s",
              directory_ok ? "ok" : "FAILED", zip_plain_ok ? "ok" : "FAILED", zip_owning_ok ? "ok" : "FAILED");
    log_line(line);
    return directory_ok || zip_plain_ok || zip_owning_ok;
}
#else
static BOOL __attribute__((thiscall)) win32_128_zip_get_contents(
    void *self, const GameStringX86 *path, GameStringX86 *output,
    BOOL require_valid_pack)
{
    static const BYTE copyright_key[] = "menu.copyright=";
    static const BYTE copyright_replacement[] = COPYRIGHT_OVERRIDE_LINE_A;
    static const BYTE development_key[] = "development_version=";
    static const BYTE development_replacement[] = DEVELOPMENT_OVERRIDE_LINE_A;
    Game128StringAssignFn assign = (Game128StringAssignFn)(
        game_image + RVA_128_STRING_ASSIGN);
    const char *contents;
    BYTE *copyright_output = NULL;
    BYTE *final_output = NULL;
    BYTE *start_screen_output = NULL;
    DWORD copyright_size = 0;
    DWORD final_size = 0;
    DWORD start_screen_size = 0;
    BOOL result;

    result = original_128_zip_get_contents(
        self, path, output, require_valid_pack);
    if (!result) return result;
    contents = game_string_data(output);
    if (!contents || IsBadReadPtr(contents, output->length)) return result;
    if (resource_path_has_suffix(path, "start_screen.json")) {
        if (start_screen_force_development_control(
                (const BYTE *)contents, output->length,
                &start_screen_output, &start_screen_size)) {
            assign(output, (const char *)start_screen_output,
                   start_screen_size);
            HeapFree(GetProcessHeap(), 0, start_screen_output);
        }
        return result;
    }
    if (!resource_path_has_suffix(path, ".lang")) return result;
    if (!lang_apply_line_override(
            (const BYTE *)contents, output->length,
            copyright_key, sizeof(copyright_key) - 1,
            copyright_replacement, sizeof(copyright_replacement) - 1,
            &copyright_output, &copyright_size))
        return result;
    if (lang_apply_line_override(
            copyright_output, copyright_size,
            development_key, sizeof(development_key) - 1,
            development_replacement, sizeof(development_replacement) - 1,
            &final_output, &final_size)) {
        assign(output, (const char *)final_output, final_size);
        HeapFree(GetProcessHeap(), 0, final_output);
    }
    HeapFree(GetProcessHeap(), 0, copyright_output);
    return result;
}

static BOOL patch_128_zip_slot(BYTE *image, SIZE_T slot_rva)
{
    void **slot = (void **)(image + slot_rva);
    DWORD old_protection;
    DWORD ignored;
    if (*slot != image + RVA_128_ZIP_GET_CONTENTS &&
        *slot != win32_128_zip_get_contents) return FALSE;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE,
                        &old_protection)) return FALSE;
    *slot = win32_128_zip_get_contents;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    return TRUE;
}

static BOOL install_128_zip_resource_overrides(BYTE *image)
{
    original_128_zip_get_contents = (Game128ZipGetContentsFn)(
        image + RVA_128_ZIP_GET_CONTENTS);
    if (!patch_128_zip_slot(image, RVA_128_ZIP_OWNING_GET_CONTENTS_SLOT) ||
        !patch_128_zip_slot(image, RVA_128_ZIP_GET_CONTENTS_SLOT)) {
        log_line("Minecraft 1.2.8 ZIP resource override installation failed");
        return FALSE;
    }
    log_line("Minecraft 1.2.8 ZIP screen/language overrides installed");
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 */

static BOOL copyright_write_override_file(const wchar_t *source_path,
                                          wchar_t *temporary_path)
{
    static const BYTE copyright_key[] = "menu.copyright=";
    static const BYTE copyright_replacement[] = COPYRIGHT_OVERRIDE_LINE_A;
    static const BYTE development_key[] = "development_version=";
    static const BYTE development_replacement[] = DEVELOPMENT_OVERRIDE_LINE_A;
    HANDLE source = INVALID_HANDLE_VALUE;
    HANDLE target = INVALID_HANDLE_VALUE;
    BYTE *input = NULL;
    BYTE *copyright_output = NULL;
    BYTE *output = NULL;
    DWORD high = 0;
    DWORD size;
    DWORD read_count = 0;
    DWORD write_count = 0;
    DWORD copyright_output_size = 0;
    DWORD output_size = 0;
    DWORD temp_length;
    wchar_t temp_directory[MAX_PATH];
    BOOL success = FALSE;

    if (!copyright_is_lang_path_w(source_path) || !temporary_path)
        return FALSE;
    source = CreateFileW(source_path, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (source == INVALID_HANDLE_VALUE) goto cleanup;
    size = GetFileSize(source, &high);
    if ((size == INVALID_FILE_SIZE && GetLastError() != 0) ||
        high != 0 || size > COPYRIGHT_MAX_LANG_BYTES)
        goto cleanup;
    input = (BYTE *)HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
    if (!input) goto cleanup;
    if (size && (!ReadFile(source, input, size, &read_count, NULL) ||
                 read_count != size))
        goto cleanup;
    CloseHandle(source);
    source = INVALID_HANDLE_VALUE;

    if (!lang_apply_line_override(
            input, size,
            copyright_key, (DWORD)(sizeof(copyright_key) - 1),
            copyright_replacement,
            (DWORD)(sizeof(copyright_replacement) - 1),
            &copyright_output, &copyright_output_size))
        goto cleanup;
    if (!lang_apply_line_override(
            copyright_output, copyright_output_size,
            development_key, (DWORD)(sizeof(development_key) - 1),
            development_replacement,
            (DWORD)(sizeof(development_replacement) - 1),
            &output, &output_size))
        goto cleanup;

    temp_length = GetTempPathW(MAX_PATH, temp_directory);
    if (!temp_length || temp_length >= MAX_PATH) goto cleanup;
    if (!GetTempFileNameW(temp_directory, L"D3C", 0, temporary_path))
        goto cleanup;
    target = CreateFileW(temporary_path, GENERIC_WRITE,
                         FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (target == INVALID_HANDLE_VALUE) {
        DeleteFileW(temporary_path);
        temporary_path[0] = 0;
        goto cleanup;
    }
    if (output_size &&
        (!WriteFile(target, output, output_size, &write_count, NULL) ||
         write_count != output_size)) {
        CloseHandle(target);
        target = INVALID_HANDLE_VALUE;
        DeleteFileW(temporary_path);
        temporary_path[0] = 0;
        goto cleanup;
    }
    CloseHandle(target);
    target = INVALID_HANDLE_VALUE;
    copyright_remember_temp(temporary_path);
    success = TRUE;

cleanup:
    if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
    if (target != INVALID_HANDLE_VALUE) CloseHandle(target);
    if (input) HeapFree(GetProcessHeap(), 0, input);
    if (copyright_output)
        HeapFree(GetProcessHeap(), 0, copyright_output);
    if (output) HeapFree(GetProcessHeap(), 0, output);
    return success;
}

BOOL Win32CraftPrepareCopyrightLangW(const wchar_t *path,
                                   wchar_t *temporary_path)
{
    if (!temporary_path) return FALSE;
    temporary_path[0] = 0;
    return copyright_write_override_file(path, temporary_path);
}

BOOL Win32CraftPrepareCopyrightLangA(const char *path, char *temporary_path,
                                   UINT temporary_capacity)
{
    wchar_t wide_path[MAX_PATH * 3];
    wchar_t wide_temporary[MAX_PATH];
    int converted;
    if (!path || !temporary_path || temporary_capacity == 0) return FALSE;
    temporary_path[0] = 0;
    converted = MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path,
                                    ARRAYSIZE(wide_path));
    if (!converted) return FALSE;
    if (!copyright_write_override_file(wide_path, wide_temporary))
        return FALSE;
    converted = WideCharToMultiByte(CP_ACP, 0, wide_temporary, -1,
                                    temporary_path, temporary_capacity,
                                    NULL, NULL);
    if (!converted) {
        DeleteFileW(wide_temporary);
        temporary_path[0] = 0;
        return FALSE;
    }
    return TRUE;
}


#define START_SCREEN_MAX_JSON_BYTES (8U * 1024U * 1024U)

static BOOL start_screen_is_path_w(const wchar_t *path)
{
    static const wchar_t suffix[] = L"ui\\start_screen.json";
    int path_length;
    int suffix_length = (int)ARRAYSIZE(suffix) - 1;
    int index;

    if (!path || IsBadReadPtr(path, sizeof(*path))) return FALSE;
    path_length = lstrlenW(path);
    if (path_length < suffix_length) return FALSE;
    for (index = 0; index < suffix_length; ++index) {
        WCHAR actual = path[path_length - suffix_length + index];
        WCHAR expected = suffix[index];
        if (actual == L'/') actual = L'\\';
        if (copyright_ascii_lower(actual) !=
            copyright_ascii_lower(expected))
            return FALSE;
    }
    if (path_length == suffix_length) return TRUE;
    return path[path_length - suffix_length - 1] == L'\\' ||
           path[path_length - suffix_length - 1] == L'/';
}

static BYTE *find_bytes_in_range(BYTE *begin, BYTE *end,
                                 const char *needle, DWORD needle_size)
{
    BYTE *cursor;
    if (!begin || !end || end < begin || !needle || !needle_size)
        return NULL;
    for (cursor = begin; cursor + needle_size <= end; ++cursor) {
        if (memcmp(cursor, needle, needle_size) == 0) return cursor;
    }
    return NULL;
}

static BYTE *json_find_matching(BYTE *open, BYTE *end,
                                BYTE open_char, BYTE close_char)
{
    LONG depth = 0;
    BOOL in_string = FALSE;
    BOOL escaped = FALSE;
    BOOL line_comment = FALSE;
    BOOL block_comment = FALSE;
    BYTE *cursor;

    if (!open || !end || open >= end || *open != open_char) return NULL;
    for (cursor = open; cursor < end; ++cursor) {
        BYTE ch = *cursor;
        BYTE next = cursor + 1 < end ? cursor[1] : 0;
        if (line_comment) {
            if (ch == '\n' || ch == '\r') line_comment = FALSE;
            continue;
        }
        if (block_comment) {
            if (ch == '*' && next == '/') {
                block_comment = FALSE;
                ++cursor;
            }
            continue;
        }
        if (in_string) {
            if (escaped) escaped = FALSE;
            else if (ch == '\\') escaped = TRUE;
            else if (ch == '"') in_string = FALSE;
            continue;
        }
        if (ch == '/' && next == '/') {
            line_comment = TRUE;
            ++cursor;
            continue;
        }
        if (ch == '/' && next == '*') {
            block_comment = TRUE;
            ++cursor;
            continue;
        }
        if (ch == '"') {
            in_string = TRUE;
            continue;
        }
        if (ch == open_char) ++depth;
        else if (ch == close_char) {
            --depth;
            if (depth == 0) return cursor;
        }
    }
    return NULL;
}

static BOOL json_replace_owned_range(BYTE **buffer, DWORD *size,
                                     DWORD begin, DWORD end,
                                     const char *replacement,
                                     DWORD replacement_size)
{
    BYTE *next;
    DWORD next_size;
    if (!buffer || !*buffer || !size || begin > end || end > *size ||
        !replacement)
        return FALSE;
    next_size = begin + replacement_size + (*size - end);
    next = (BYTE *)HeapAlloc(GetProcessHeap(), 0, next_size ? next_size : 1);
    if (!next) return FALSE;
    if (begin) memcpy(next, *buffer, begin);
    if (replacement_size) memcpy(next + begin, replacement, replacement_size);
    if (end < *size)
        memcpy(next + begin + replacement_size, *buffer + end, *size - end);
    HeapFree(GetProcessHeap(), 0, *buffer);
    *buffer = next;
    *size = next_size;
    return TRUE;
}

static BYTE *json_find_exact_key(BYTE *begin, BYTE *end,
                                 const char *key, DWORD key_size)
{
    BYTE *cursor = begin;
    while (cursor && cursor < end) {
        BYTE *found = find_bytes_in_range(cursor, end, key, key_size);
        BYTE *after;
        if (!found) return NULL;
        after = found + key_size;
        while (after < end && (*after == ' ' || *after == '\t' ||
               *after == '\r' || *after == '\n')) ++after;
        if (after < end && *after == ':') return found;
        cursor = found + 1;
    }
    return NULL;
}

static BOOL start_screen_force_development_control(
    const BYTE *input, DWORD size, BYTE **output, DWORD *output_size)
{
    static const char definition_key[] = "\"development_version\"";
    static const char text_key[] = "\"text\"";
    static const char bindings_key[] = "\"bindings\"";
    static const char control_name[] =
        "development_version@start.development_version";
    static const char control_fallback[] = "development_version@";
    static const char exact_text[] =
        "\"github.com/deltaxur/Win32Craft\"";
    static const char empty_bindings[] = "[]";
    static const char instance_override[] =
        "{\"ignored\":false,\"text\":\"github.com/deltaxur/Win32Craft\","
        "\"bindings\":[]}";
    BYTE *current;
    DWORD current_size;
    BOOL changed = FALSE;
    BYTE *key;
    BYTE *colon;
    BYTE *object_open;
    BYTE *object_close;
    BYTE *range_end;
    BYTE *value;
    BYTE *value_end;
    DWORD begin_offset;
    DWORD end_offset;

    if (!input || !output || !output_size) return FALSE;
    *output = NULL;
    *output_size = 0;
    current = (BYTE *)HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
    if (!current) return FALSE;
    if (size) memcpy(current, input, size);
    current_size = size;

    /* First make the base label literal and remove its dynamic binding. */
    key = json_find_exact_key(current, current + current_size,
                              definition_key,
                              (DWORD)(sizeof(definition_key) - 1));
    if (key) {
        colon = key + sizeof(definition_key) - 1;
        while (colon < current + current_size && *colon != ':') ++colon;
        object_open = colon < current + current_size ? colon + 1 : NULL;
        while (object_open && object_open < current + current_size &&
               (*object_open == ' ' || *object_open == '\t' ||
                *object_open == '\r' || *object_open == '\n')) ++object_open;
        object_close = object_open && *object_open == '{'
            ? json_find_matching(object_open, current + current_size, '{', '}')
            : NULL;
        if (object_close) {
            key = json_find_exact_key(object_open, object_close,
                                      text_key,
                                      (DWORD)(sizeof(text_key) - 1));
            if (key) {
                colon = key + sizeof(text_key) - 1;
                while (colon < object_close && *colon != ':') ++colon;
                value = colon < object_close ? colon + 1 : NULL;
                while (value && value < object_close &&
                       (*value == ' ' || *value == '\t' ||
                        *value == '\r' || *value == '\n')) ++value;
                if (value && value < object_close && *value == '"') {
                    value_end = value + 1;
                    while (value_end < object_close) {
                        if (*value_end == '\\' && value_end + 1 < object_close) {
                            value_end += 2;
                            continue;
                        }
                        if (*value_end == '"') { ++value_end; break; }
                        ++value_end;
                    }
                    begin_offset = (DWORD)(value - current);
                    end_offset = (DWORD)(value_end - current);
                    if (json_replace_owned_range(
                            &current, &current_size, begin_offset, end_offset,
                            exact_text, (DWORD)(sizeof(exact_text) - 1)))
                        changed = TRUE;
                }
            }
        }
    }

    /* Re-find the definition after the possible allocation above, then clear
     * its bindings array so a native build number cannot overwrite the text. */
    key = json_find_exact_key(current, current + current_size,
                              definition_key,
                              (DWORD)(sizeof(definition_key) - 1));
    if (key) {
        colon = key + sizeof(definition_key) - 1;
        while (colon < current + current_size && *colon != ':') ++colon;
        object_open = colon < current + current_size ? colon + 1 : NULL;
        while (object_open && object_open < current + current_size &&
               (*object_open == ' ' || *object_open == '\t' ||
                *object_open == '\r' || *object_open == '\n')) ++object_open;
        object_close = object_open && *object_open == '{'
            ? json_find_matching(object_open, current + current_size, '{', '}')
            : NULL;
        if (object_close) {
            key = json_find_exact_key(object_open, object_close,
                                      bindings_key,
                                      (DWORD)(sizeof(bindings_key) - 1));
            if (key) {
                colon = key + sizeof(bindings_key) - 1;
                while (colon < object_close && *colon != ':') ++colon;
                value = colon < object_close ? colon + 1 : NULL;
                while (value && value < object_close &&
                       (*value == ' ' || *value == '\t' ||
                        *value == '\r' || *value == '\n')) ++value;
                value_end = value && *value == '['
                    ? json_find_matching(value, current + current_size, '[', ']')
                    : NULL;
                if (value_end) {
                    begin_offset = (DWORD)(value - current);
                    end_offset = (DWORD)(value_end + 1 - current);
                    if (json_replace_owned_range(
                            &current, &current_size, begin_offset, end_offset,
                            empty_bindings,
                            (DWORD)(sizeof(empty_bindings) - 1)))
                        changed = TRUE;
                }
            }
        }
    }

    /* Finally force the inherited instance visible and self-contained. */
    key = find_bytes_in_range(
        current, current + current_size,
        control_name, (DWORD)(sizeof(control_name) - 1));
    if (!key) {
        key = find_bytes_in_range(
            current, current + current_size,
            control_fallback, (DWORD)(sizeof(control_fallback) - 1));
    }
    if (key) {
        range_end = key + 2048;
        if (range_end > current + current_size)
            range_end = current + current_size;
        colon = key;
        while (colon < range_end && *colon != ':') ++colon;
        object_open = colon < range_end ? colon + 1 : NULL;
        while (object_open && object_open < range_end &&
               (*object_open == ' ' || *object_open == '\t' ||
                *object_open == '\r' || *object_open == '\n')) ++object_open;
        object_close = object_open && *object_open == '{'
            ? json_find_matching(object_open, current + current_size, '{', '}')
            : NULL;
        if (object_close) {
            begin_offset = (DWORD)(object_open - current);
            end_offset = (DWORD)(object_close + 1 - current);
            if (json_replace_owned_range(
                    &current, &current_size, begin_offset, end_offset,
                    instance_override,
                    (DWORD)(sizeof(instance_override) - 1)))
                changed = TRUE;
        }
    }

    if (!changed) {
        HeapFree(GetProcessHeap(), 0, current);
        return FALSE;
    }
    *output = current;
    *output_size = current_size;
    return TRUE;
}

static BOOL start_screen_write_override_file(const wchar_t *source_path,
                                             wchar_t *temporary_path)
{
    HANDLE source = INVALID_HANDLE_VALUE;
    HANDLE target = INVALID_HANDLE_VALUE;
    BYTE *input = NULL;
    BYTE *output = NULL;
    DWORD high = 0;
    DWORD size;
    DWORD read_count = 0;
    DWORD write_count = 0;
    DWORD output_size = 0;
    DWORD temp_length;
    wchar_t temp_directory[MAX_PATH];
    BOOL success = FALSE;

    if (!start_screen_is_path_w(source_path) || !temporary_path)
        return FALSE;
    source = CreateFileW(source_path, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (source == INVALID_HANDLE_VALUE) goto cleanup;
    size = GetFileSize(source, &high);
    if ((size == INVALID_FILE_SIZE && GetLastError() != 0) ||
        high != 0 || size > START_SCREEN_MAX_JSON_BYTES)
        goto cleanup;
    input = (BYTE *)HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
    if (!input) goto cleanup;
    if (size && (!ReadFile(source, input, size, &read_count, NULL) ||
                 read_count != size))
        goto cleanup;
    CloseHandle(source);
    source = INVALID_HANDLE_VALUE;

    if (!start_screen_force_development_control(
            input, size, &output, &output_size))
        goto cleanup;

    temp_length = GetTempPathW(MAX_PATH, temp_directory);
    if (!temp_length || temp_length >= MAX_PATH) goto cleanup;
    if (!GetTempFileNameW(temp_directory, L"D3U", 0, temporary_path))
        goto cleanup;
    target = CreateFileW(temporary_path, GENERIC_WRITE,
                         FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (target == INVALID_HANDLE_VALUE) {
        DeleteFileW(temporary_path);
        temporary_path[0] = 0;
        goto cleanup;
    }
    if (output_size &&
        (!WriteFile(target, output, output_size, &write_count, NULL) ||
         write_count != output_size)) {
        CloseHandle(target);
        target = INVALID_HANDLE_VALUE;
        DeleteFileW(temporary_path);
        temporary_path[0] = 0;
        goto cleanup;
    }
    CloseHandle(target);
    target = INVALID_HANDLE_VALUE;
    copyright_remember_temp(temporary_path);
    success = TRUE;

cleanup:
    if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
    if (target != INVALID_HANDLE_VALUE) CloseHandle(target);
    if (input) HeapFree(GetProcessHeap(), 0, input);
    if (output) HeapFree(GetProcessHeap(), 0, output);
    return success;
}

BOOL Win32CraftPrepareStartScreenJsonW(const wchar_t *path,
                                    wchar_t *temporary_path)
{
    if (!temporary_path) return FALSE;
    temporary_path[0] = 0;
    return start_screen_write_override_file(path, temporary_path);
}

BOOL Win32CraftPrepareStartScreenJsonA(const char *path,
                                    char *temporary_path,
                                    UINT temporary_capacity)
{
    wchar_t wide_path[MAX_PATH * 3];
    wchar_t wide_temporary[MAX_PATH];
    int converted;
    if (!path || !temporary_path || temporary_capacity == 0) return FALSE;
    temporary_path[0] = 0;
    converted = MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path,
                                    ARRAYSIZE(wide_path));
    if (!converted) return FALSE;
    if (!start_screen_write_override_file(wide_path, wide_temporary))
        return FALSE;
    converted = WideCharToMultiByte(CP_ACP, 0, wide_temporary, -1,
                                    temporary_path, temporary_capacity,
                                    NULL, NULL);
    if (!converted) {
        DeleteFileW(wide_temporary);
        temporary_path[0] = 0;
        return FALSE;
    }
    return TRUE;
}

static void remember_115_stdio_path(const char *path, FILE *stream)
{
    copy_ascii_sample(game115_last_stdio_path,
                      (UINT)sizeof(game115_last_stdio_path), path);
    game115_last_stdio_stream = stream;
}


static FILE *__cdecl win32_01510_fopen(const char *path, const char *mode)
{
    char temporary[MAX_PATH * 3];
    if (original_01510_fopen && copyright_mode_reads_a(mode)) {
        if (Win32CraftPrepareCopyrightLangA(
                path, temporary, sizeof(temporary))) {
            FILE *stream = original_01510_fopen(temporary, mode);
            if (stream) return stream;
        }
        if (Win32CraftPrepareStartScreenJsonA(
                path, temporary, sizeof(temporary))) {
            FILE *stream = original_01510_fopen(temporary, mode);
            if (stream) return stream;
        }
    }
    return original_01510_fopen ? original_01510_fopen(path, mode) : NULL;
}

static FILE *__cdecl win32_01510_wfopen(const wchar_t *path,
                                        const wchar_t *mode)
{
    wchar_t temporary[MAX_PATH];
    if (original_01510_wfopen && copyright_mode_reads_w(mode)) {
        if (Win32CraftPrepareCopyrightLangW(path, temporary)) {
            FILE *stream = original_01510_wfopen(temporary, mode);
            if (stream) return stream;
        }
        if (Win32CraftPrepareStartScreenJsonW(path, temporary)) {
            FILE *stream = original_01510_wfopen(temporary, mode);
            if (stream) return stream;
        }
    }
    return original_01510_wfopen ? original_01510_wfopen(path, mode) : NULL;
}

static int __cdecl win32_01510_wfopen_s(FILE **output,
                                        const wchar_t *path,
                                        const wchar_t *mode)
{
    wchar_t temporary[MAX_PATH];
    int result;
    if (original_01510_wfopen_s && copyright_mode_reads_w(mode)) {
        if (Win32CraftPrepareCopyrightLangW(path, temporary)) {
            result = original_01510_wfopen_s(output, temporary, mode);
            if (!result && output && *output) return result;
        }
        if (Win32CraftPrepareStartScreenJsonW(path, temporary)) {
            result = original_01510_wfopen_s(output, temporary, mode);
            if (!result && output && *output) return result;
        }
    }
    return original_01510_wfopen_s
        ? original_01510_wfopen_s(output, path, mode) : 22;
}

static FILE *__cdecl win32_115_fopen(const char *path, const char *mode)
{
    char temporary[MAX_PATH * 3];
    const char *open_path = path;
    FILE *stream;
    if (original_115_fopen && copyright_mode_reads_a(mode)) {
        if (Win32CraftPrepareCopyrightLangA(
                path, temporary, sizeof(temporary)) ||
            Win32CraftPrepareStartScreenJsonA(
                path, temporary, sizeof(temporary)))
            open_path = temporary;
    }
    stream = original_115_fopen
        ? original_115_fopen(open_path, mode) : NULL;
    char safe_path[300];
    char safe_mode[32];
    char line[420];
    copy_ascii_sample(safe_path, sizeof(safe_path), path);
    copy_ascii_sample(safe_mode, sizeof(safe_mode), mode);
    LONG count;
    LONG failures = 0;
    remember_115_stdio_path(path, stream);
    count = InterlockedIncrement(&game115_fopen_log_count);
    if (!stream) failures = InterlockedIncrement(&game115_stdio_failure_log_count);
    if (count <= 48 || (!stream && failures <= 48)) {
        wsprintfA(line, "1.1.5 fopen #%ld path='%s' mode='%s' -> %p%s",
                  count, safe_path, safe_mode, stream,
                  stream ? "" : " FAILED");
        log_line(line);
    } else if (count == 49) {
        log_line("1.1.5 successful fopen trace suppressed after 48 calls");
    }
    return stream;
}

static int __cdecl win32_115_wfopen_s(FILE **output,
                                      const wchar_t *path,
                                      const wchar_t *mode)
{
    int result;
    char safe_path[300];
    char safe_mode[32];
    char line[420];
    FILE *stream = NULL;
    safe_path[0] = 0;
    safe_mode[0] = 0;
    if (path && !IsBadReadPtr(path, sizeof(*path))) {
        WideCharToMultiByte(CP_UTF8, 0, path, -1, safe_path,
                            sizeof(safe_path), NULL, NULL);
        safe_path[sizeof(safe_path) - 1] = 0;
    }
    if (mode && !IsBadReadPtr(mode, sizeof(*mode))) {
        WideCharToMultiByte(CP_UTF8, 0, mode, -1, safe_mode,
                            sizeof(safe_mode), NULL, NULL);
        safe_mode[sizeof(safe_mode) - 1] = 0;
    }
    {
        wchar_t temporary[MAX_PATH];
        const wchar_t *open_path = path;
        if (original_115_wfopen_s && copyright_mode_reads_w(mode) &&
            (Win32CraftPrepareCopyrightLangW(path, temporary) ||
             Win32CraftPrepareStartScreenJsonW(path, temporary)))
            open_path = temporary;
        result = original_115_wfopen_s
            ? original_115_wfopen_s(output, open_path, mode) : 22;
    }
    if (output && !IsBadReadPtr(output, sizeof(*output))) stream = *output;
    LONG count;
    LONG failures = 0;
    remember_115_stdio_path(safe_path, stream);
    count = InterlockedIncrement(&game115_wfopen_log_count);
    if (result || !stream)
        failures = InterlockedIncrement(&game115_stdio_failure_log_count);
    if (count <= 96 || ((result || !stream) && failures <= 48)) {
        wsprintfA(line, "1.1.5 _wfopen_s #%ld path='%s' mode='%s' -> %d stream=%p%s",
                  count, safe_path, safe_mode, result, stream,
                  (!result && stream) ? "" : " FAILED");
        log_line(line);
    } else if (count == 97) {
        log_line("1.1.5 successful _wfopen_s trace suppressed after 96 calls");
    }
    return result;
}


static FILE *__cdecl win32_115_wfopen(const wchar_t *path,
                                      const wchar_t *mode)
{
    wchar_t temporary[MAX_PATH];
    const wchar_t *open_path = path;
    if (original_115_wfopen && copyright_mode_reads_w(mode) &&
        (Win32CraftPrepareCopyrightLangW(path, temporary) ||
         Win32CraftPrepareStartScreenJsonW(path, temporary)))
        open_path = temporary;
    return original_115_wfopen ? original_115_wfopen(open_path, mode) : NULL;
}

static void remember_115_scan(const char *buffer, const char *format,
                              int result)
{
    LONG sequence = InterlockedIncrement(&game115_scan_sequence);
    Game115ScanTrace *trace =
        &game115_scan_ring[(unsigned long)sequence & 31u];
    trace->sequence = sequence;
    trace->result = result;
    trace->buffer = buffer;
    copy_ascii_sample(trace->sample, sizeof(trace->sample), buffer);
    copy_ascii_sample(trace->format, sizeof(trace->format), format);
    if (sequence <= 8 || result <= 0) {
        char line[240];
        wsprintfA(line,
                  "1.1.5 sscanf #%ld result=%d input='%s' format='%s'",
                  sequence, result, trace->sample, trace->format);
        log_line(line);
    }
}

static int __cdecl win32_115_vsscanf(
    unsigned long long options, const char *buffer, size_t buffer_count,
    const char *format, void *locale, void *arguments)
{
    int result = original_115_vsscanf
        ? original_115_vsscanf(options, buffer, buffer_count,
                               format, locale, arguments)
        : -1;
    remember_115_scan(buffer, format, result);
    return result;
}

static int __cdecl win32_115_vfscanf(
    unsigned long long options, FILE *stream, const char *format,
    void *locale, void *arguments)
{
    int result = original_115_vfscanf
        ? original_115_vfscanf(options, stream, format, locale, arguments)
        : -1;
    if (result <= 0) {
        char safe_format[48];
        char line[180];
        copy_ascii_sample(safe_format, sizeof(safe_format), format);
        wsprintfA(line,
                  "1.1.5 vfscanf stream=%p result=%d format='%s' last='%s'",
                  stream, result, safe_format, game115_last_stdio_path);
        log_line(line);
    }
    return result;
}

static BOOL patch_115_iat_slot(void **slot, void *replacement,
                               void **original, const char *name)
{
    DWORD old_protection;
    DWORD ignored;
    char line[160];
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE,
                        &old_protection)) {
        wsprintfA(line, "1.1.5 stdio hook %s: VirtualProtect failed", name);
        log_line(line);
        return FALSE;
    }
    *original = *slot;
    *slot = replacement;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    wsprintfA(line, "1.1.5 stdio hook installed: %s original=%p",
              name, *original);
    log_line(line);
    return TRUE;
}

static void __cdecl win32_115_abort(void)
{
    log_line("Minecraft 1.1.5 CRT abort() called");
    dump_115_stdio_state();
    if (original_115_abort) original_115_abort();
    for (;;) Sleep(1000);
}

static void __cdecl win32_115__exit(int code)
{
    char line[128];
    wsprintfA(line, "Minecraft 1.1.5 CRT _exit(%d) called", code);
    log_line(line);
    dump_115_stdio_state();
    if (original_115__exit) original_115__exit(code);
    for (;;) Sleep(1000);
}

static void __cdecl win32_115_exit(int code)
{
    char line[128];
    wsprintfA(line, "Minecraft 1.1.5 CRT exit(%d) called", code);
    log_line(line);
    dump_115_stdio_state();
    if (original_115_exit) original_115_exit(code);
    for (;;) Sleep(1000);
}

static void __cdecl win32_115_terminate(void)
{
    log_line("Minecraft 1.1.5 CRT terminate() called");
    dump_115_stdio_state();
    if (original_115_terminate) original_115_terminate();
    for (;;) Sleep(1000);
}

static void __cdecl win32_115_std_terminate(void)
{
    log_line("Minecraft 1.1.5 VCRUNTIME __std_terminate() called");
    dump_115_stdio_state();
    if (original_115_std_terminate) original_115_std_terminate();
    for (;;) Sleep(1000);
}

#ifdef WIN32CRAFT_ARM32
static void log_115_termination_stack(const char *reason)
{
    void *frames[24];
    USHORT count;
    USHORT index;
    ULONG_PTR image_base;
    ULONG_PTR image_size;
    DWORD rva;
    char line[128];
    wsprintfA(line, "Termination stack: %s", reason);
    log_line(line);
    arm32_game_image_range(&image_base, &image_size);
    count = RtlCaptureStackBackTrace(1, ARRAYSIZE(frames), frames, NULL);
    for (index = 0; index < count; ++index) {
        if (arm32_in_image((ULONG_PTR)frames[index], image_base, image_size, &rva)) {
            wsprintfA(line, "  termination frame %u: %p (game rva %08lx)",
                      (unsigned)index, frames[index], (unsigned long)rva);
        } else {
            wsprintfA(line, "  termination frame %u: %p", (unsigned)index, frames[index]);
        }
        log_line(line);
    }
}
#else
static void log_115_termination_stack(const char *reason)
{
    void *frames[24];
    USHORT count;
    USHORT index;
    char line[128];
    wsprintfA(line, "Minecraft 1.1.5 termination stack: %s", reason);
    log_line(line);
    count = RtlCaptureStackBackTrace(1, ARRAYSIZE(frames), frames, NULL);
    for (index = 0; index < count; ++index) {
        BYTE *candidate = (BYTE *)frames[index];
        wsprintfA(line, "  termination frame %u: %p%s",
                  (unsigned)index, candidate,
                  candidate >= game_image &&
                  candidate < game_image + 0x01500000 ? " (game)" : "");
        log_line(line);
    }
}
#endif

static void __cdecl win32_115_invalid_parameter(void)
{
    log_line("Minecraft 1.1.5 CRT _invalid_parameter_noinfo() called");
    log_115_termination_stack("_invalid_parameter_noinfo");
    dump_115_stdio_state();
    if (original_115_invalid_parameter) original_115_invalid_parameter();
}

static void __cdecl win32_115_invalid_parameter_noreturn(void)
{
    log_line("Minecraft 1.1.5 CRT _invalid_parameter_noinfo_noreturn() called");
    log_115_termination_stack("_invalid_parameter_noinfo_noreturn");
    dump_115_stdio_state();
    if (original_115_invalid_parameter_noreturn)
        original_115_invalid_parameter_noreturn();
    for (;;) Sleep(1000);
}

static int __cdecl win32_115_purecall(void)
{
    log_line("Minecraft 1.1.5 VCRUNTIME _purecall() called");
    dump_115_stdio_state();
    return original_115_purecall ? original_115_purecall() : 0;
}

static BOOL install_115_termination_trace(BYTE *image)
{
    BOOL result = TRUE;
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_ABORT_IAT), win32_115_abort,
        (void **)&original_115_abort, "abort");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115__EXIT_IAT), win32_115__exit,
        (void **)&original_115__exit, "_exit");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_EXIT_IAT), win32_115_exit,
        (void **)&original_115_exit, "exit");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_TERMINATE_IAT), win32_115_terminate,
        (void **)&original_115_terminate, "terminate");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_STD_TERMINATE_IAT), win32_115_std_terminate,
        (void **)&original_115_std_terminate, "__std_terminate");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_INVALID_PARAMETER_IAT),
        win32_115_invalid_parameter,
        (void **)&original_115_invalid_parameter,
        "_invalid_parameter_noinfo");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_INVALID_PARAMETER_NR_IAT),
        win32_115_invalid_parameter_noreturn,
        (void **)&original_115_invalid_parameter_noreturn,
        "_invalid_parameter_noinfo_noreturn");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_PURECALL_IAT), win32_115_purecall,
        (void **)&original_115_purecall, "_purecall");
    log_line(result
        ? "Minecraft 1.1.5 CRT termination trace installed with corrected IAT mapping"
        : "Minecraft 1.1.5 CRT termination trace incomplete");
    if (result) {
        log_line("1.1.5 CRT IAT verified: _crt_atexit=0x00F9588C left untouched");
        log_line("1.1.5 CRT IAT verified: invalid_parameter_noreturn=0x00F95890 terminate=0x00F95894");
    }
    return result;
}


static BOOL install_01510_copyright_override(BYTE *image)
{
    BOOL result = TRUE;
    result &= patch_115_iat_slot(
        (void **)(image + RVA_FOPEN_IAT), win32_01510_fopen,
        (void **)&original_01510_fopen, "0.15.10 fopen");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_WFOPEN_IAT), win32_01510_wfopen,
        (void **)&original_01510_wfopen, "0.15.10 _wfopen");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_WFOPEN_S_IAT), win32_01510_wfopen_s,
        (void **)&original_01510_wfopen_s, "0.15.10 _wfopen_s");
    log_line(result
        ? "Minecraft 0.15.10 menu.copyright override installed"
        : "Minecraft 0.15.10 menu.copyright override incomplete");
    return result;
}

static BOOL install_115_stdio_trace(BYTE *image)
{
    BOOL result = TRUE;
    if (game115_stdio_trace_installed) return TRUE;
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_FOPEN_IAT), win32_115_fopen,
        (void **)&original_115_fopen, "fopen");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_WFOPEN_S_IAT), win32_115_wfopen_s,
        (void **)&original_115_wfopen_s, "_wfopen_s");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_WFOPEN_IAT), win32_115_wfopen,
        (void **)&original_115_wfopen, "_wfopen");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_VFSCANF_IAT), win32_115_vfscanf,
        (void **)&original_115_vfscanf, "__stdio_common_vfscanf");
    result &= patch_115_iat_slot(
        (void **)(image + RVA_115_VSSCANF_IAT), win32_115_vsscanf,
        (void **)&original_115_vsscanf, "__stdio_common_vsscanf");
    game115_stdio_trace_installed = result;
    log_line(result
        ? "Minecraft 1.1.5 CRT stdio/JSON trace installed"
        : "Minecraft 1.1.5 CRT stdio/JSON trace incomplete");
    return result;
}

static void trace_115_one_asset(const wchar_t *relative)
{
    wchar_t full[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    char relative_utf8[360];
    char line[500];
    DWORD high;
    unsigned long long size;
    lstrcpyW(full, package_path);
    if (lstrlenW(full) && full[lstrlenW(full) - 1] != L'\\')
        lstrcatW(full, L"\\");
    lstrcatW(full, relative);
    relative_utf8[0] = 0;
    WideCharToMultiByte(CP_UTF8, 0, relative, -1, relative_utf8,
                        sizeof(relative_utf8), NULL, NULL);
    if (!GetFileAttributesExW(full, 0, &attributes)) {
        wsprintfA(line, "Minecraft 1.1.5 asset preflight: MISSING %s",
                  relative_utf8);
        log_line(line);
        return;
    }
    high = attributes.nFileSizeHigh;
    size = ((unsigned long long)high << 32) | attributes.nFileSizeLow;
    wsprintfA(line, "Minecraft 1.1.5 asset preflight: %s size=%lu:%lu",
              relative_utf8, high,
              (unsigned long)attributes.nFileSizeLow);
    log_line(line);
    (void)size;
}

static void trace_115_asset_candidates(void)
{
    trace_115_one_asset(L"data\\sounds.json");
    trace_115_one_asset(L"data\\sounds\\sound_definitions.json");
    trace_115_one_asset(L"data\\resource_packs\\vanilla\\sounds.json");
    trace_115_one_asset(
        L"data\\resource_packs\\vanilla\\sounds\\sound_definitions.json");
}

static void dump_115_stdio_state(void)
{
    LONG latest;
    LONG first;
    LONG sequence;
    char line[520];
    if (!game115_stdio_trace_installed) return;
    wsprintfA(line, "1.1.5 last stdio path='%s' stream=%p scan_seq=%ld",
              game115_last_stdio_path, game115_last_stdio_stream,
              game115_scan_sequence);
    log_line(line);
    latest = game115_scan_sequence;
    first = latest > 12 ? latest - 11 : 1;
    for (sequence = first; sequence <= latest; ++sequence) {
        Game115ScanTrace *trace =
            &game115_scan_ring[(unsigned long)sequence & 31u];
        if (trace->sequence != sequence) continue;
        wsprintfA(line,
                  "  scan #%ld result=%d ptr=%p input='%s' format='%s'",
                  trace->sequence, trace->result, trace->buffer,
                  trace->sample, trace->format);
        log_line(line);
    }
}

#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 (unit iat_hooks): _CxxThrowException trace, D3DCompile/D3DPreprocess loggers, D3D11CreateDevice hook.
 * All hook functions are plain AAPCS C functions (WINAPI is empty on ARM); the values written into the IAT cells are
 * Thumb function pointers (bit 0 set), the originals are read back from the cells with their bit 0.
 * ====================================================================================================== */

/* The ThrowInfo of the ARM MSVC runtime holds image-relative offsets (verified on the ARM exe: ThrowInfo+12 =
 * CatchableTypeArray rva {count, CatchableType rva[]}, CatchableType+4 = type_info rva, type_info+8 = decorated name),
 * so the exception type can be named without touching the runtime. */
static void arm32_iat_throw_type_name(const void *throw_info, char *out, SIZE_T out_size)
{
    ULONG_PTR base;
    ULONG_PTR size;
    const BYTE *info = (const BYTE *)throw_info;
    DWORD array_rva;
    DWORD catchable_rva;
    DWORD descriptor_rva;
    DWORD ignored;
    LONG count;

    out[0] = 0;
    arm32_game_image_range(&base, &size);
    if (!arm32_in_image((ULONG_PTR)info, base, size, &ignored) || IsBadReadPtr(info, 16)) return;
    array_rva = *(const DWORD *)(info + 12);
    if (!array_rva || array_rva >= size || IsBadReadPtr((const void *)(base + array_rva), 8)) return;
    count = *(const LONG *)(base + array_rva);
    if (count <= 0 || count > 64) return;
    catchable_rva = *(const DWORD *)(base + array_rva + 4);
    if (!catchable_rva || catchable_rva >= size || IsBadReadPtr((const void *)(base + catchable_rva), 8)) return;
    descriptor_rva = *(const DWORD *)(base + catchable_rva + 4);
    if (!descriptor_rva || descriptor_rva >= size) return;
    arm32_iat_copy_ascii(out, out_size, (const char *)(base + descriptor_rva + 8));
}

/* the most recent game C++ throw (for the abort/terminate report: an escaping exception ends the process via abort()) */
static const void *volatile arm32_last_throw_info;
static void *volatile arm32_last_throw_object;
static volatile LONG arm32_last_throw_number;
static volatile DWORD arm32_last_throw_thread;

static void __attribute__((noinline)) arm32_iat_log_cxx_throw(void *exception, const void *throw_info)
{
    static volatile LONG throw_count;
    void *frames[24];
    char type_name[96];
    char where[40];
    char line[256];
    ULONG_PTR base;
    ULONG_PTR size;
    USHORT count;
    USHORT index;
    LONG number = InterlockedIncrement(&throw_count);

    arm32_last_throw_info = throw_info;
    arm32_last_throw_object = exception;
    arm32_last_throw_thread = GetCurrentThreadId();
    arm32_last_throw_number = number;
    /* stacks for the first 64; then one short line per throw up to #3000 (so the last throw before a silent exit is
     * always visible), afterwards every 64th */
    if (number > 3000 && (number % 64) != 0) return;
    arm32_iat_throw_type_name(throw_info, type_name, sizeof(type_name));
    wsprintfA(line, "game raised C++ exception #%ld: type=%s object=%p throw_info=%p%s", (long)number,
              type_name[0] ? type_name : "?", exception, throw_info,
              number > 64 ? " (stack suppressed)" : "; stack follows");
    log_line(line);
    if (number > 64) return;
    arm32_game_image_range(&base, &size);
    count = RtlCaptureStackBackTrace(0, 24, frames, NULL);
    for (index = 0; index < count; ++index) {
        arm32_rva_text(where, sizeof(where), (ULONG_PTR)frames[index], base, size);
        wsprintfA(line, "  frame %u: %p%s", (unsigned)index, frames[index], where);
        log_line(line);
    }
}

/* The real call must stay a tail call: no host frame may sit between the throwing game function and
 * _CxxThrowException while the C++ runtime unwinds (checked in the disassembly: the last instruction is a bx). */
static void win32_cxx_throw(void *exception, const void *throw_info)
{
    arm32_iat_log_cxx_throw(exception, throw_info);
    original_cxx_throw(exception, throw_info);
}

static BOOL install_exception_trace(BYTE *image, SIZE_T cxx_throw_rva)
{
    if (!arm32_iat_hook_import(image, FALSE, "_CxxThrowException", (DWORD)cxx_throw_rva, (void *)win32_cxx_throw,
                           (void **)&original_cxx_throw, "_CxxThrowException")) {
        log_line("ARM32: C++ exception trace NOT installed");
        return FALSE;
    }
    log_line("ARM32: C++ exception trace installed");
    return TRUE;
}

/* ---- abort / terminate / invalid-parameter / pure-call trace ----
 * std::terminate() (an exception escaping a worker thread or a RunAsync lambda), abort(), a CRT invalid parameter and a pure
 * virtual call all end the process WITHOUT reaching the unhandled-exception filter (fast-fail).  abort() raises SIGABRT first and
 * the UCRT SIGABRT action is process-wide, so one handler covers std::terminate on every thread (set_terminate would be
 * per-thread).  The log gets the reason, thread, stack and the last C++ throw before the process dies. */
static void arm32_report_fatal_exit(const char *reason)
{
    char line[320];
    char type_name[96];
    char where[40];
    void *frames[24];
    ULONG_PTR base;
    ULONG_PTR size;
    USHORT count;
    USHORT index;
    int marker = 0;

    wsprintfA(line, "ARM32 FATAL EXIT: %s on thread T%04lx - the process ends now (no exception filter will run)", reason,
              (unsigned long)GetCurrentThreadId());
    log_line(line);
    if (arm32_last_throw_number) {
        type_name[0] = 0;
        arm32_iat_throw_type_name((const void *)arm32_last_throw_info, type_name, sizeof(type_name));
        wsprintfA(line, "  last game C++ throw: #%ld on thread T%04lx type=%s object=%p", (long)arm32_last_throw_number,
                  (unsigned long)arm32_last_throw_thread, type_name[0] ? type_name : "?", (void *)arm32_last_throw_object);
        log_line(line);
    } else {
        log_line("  no game C++ throw was recorded before this exit");
    }
    arm32_game_image_range(&base, &size);
    count = RtlCaptureStackBackTrace(0, ARRAYSIZE(frames), frames, NULL);
    for (index = 0; index < count; ++index) {
        arm32_rva_text(where, sizeof(where), (ULONG_PTR)frames[index], base, size);
        wsprintfA(line, "  frame %u: %p%s", (unsigned)index, frames[index], where);
        log_line(line);
    }
    arm32_log_stack_scan("fatal", ((ULONG_PTR)&marker) & ~(ULONG_PTR)3u, 512, 16);
}

static void __cdecl arm32_sigabrt_handler(int signal_number)
{
    (void)signal_number;
    arm32_report_fatal_exit("abort() / std::terminate() (SIGABRT)");
    /* returning lets the CRT finish abort() exactly as without the handler */
}

static void __cdecl arm32_invalid_parameter_handler(const wchar_t *expression, const wchar_t *function, const wchar_t *file,
                                                    unsigned line_number, ULONG_PTR reserved)
{
    char text[160];
    (void)reserved;
    wsprintfA(text, "CRT invalid parameter (function=%s file=%s line=%u expression=%s)",
              (function && !IsBadReadPtr(function, 2)) ? "present" : "none", (file && !IsBadReadPtr(file, 2)) ? "present" : "none",
              line_number, (expression && !IsBadReadPtr(expression, 2)) ? "present" : "none");
    arm32_report_fatal_exit(text);
    /* the default (no handler) is a fast-fail termination: keep that outcome */
    TerminateProcess(GetCurrentProcess(), 0xC0000417u /* STATUS_INVALID_CRUNTIME_PARAMETER */);
}

static void __cdecl arm32_purecall_handler(void)
{
    arm32_report_fatal_exit("pure virtual function call");
    TerminateProcess(GetCurrentProcess(), 0xC0000409u);
}

static void arm32_install_abort_traps(void)
{
    typedef void (__cdecl *SignalHandlerFn)(int);
    typedef SignalHandlerFn (__cdecl *SignalFn)(int, SignalHandlerFn);
    typedef void *(__cdecl *SetHandlerFn)(void *);
    static const wchar_t *const candidates[] = { L"api-ms-win-crt-runtime-l1-1-0.dll", L"ucrtbase.dll" };
    HMODULE crt = NULL;
    SignalFn signal_function;
    SetHandlerFn set_invalid;
    SetHandlerFn set_purecall;
    char line[200];
    UINT index;

    for (index = 0; index < ARRAYSIZE(candidates) && !crt; ++index) crt = GetModuleHandleW(candidates[index]);
    for (index = 0; index < ARRAYSIZE(candidates) && !crt; ++index) crt = LoadLibraryW(candidates[index]);
    if (!crt) {
        log_line("ARM32 abort traps NOT installed: neither api-ms-win-crt-runtime-l1-1-0.dll nor ucrtbase.dll is loadable");
        return;
    }
    signal_function = (SignalFn)GetProcAddress(crt, "signal");
    set_invalid = (SetHandlerFn)GetProcAddress(crt, "_set_invalid_parameter_handler");
    set_purecall = (SetHandlerFn)GetProcAddress(crt, "_set_purecall_handler");
    wsprintfA(line, "ARM32 abort traps: crt=%p signal=%p set_invalid_parameter_handler=%p set_purecall_handler=%p", (void *)crt,
              (void *)signal_function, (void *)set_invalid, (void *)set_purecall);
    log_line(line);
    if (signal_function) {
        SignalHandlerFn previous = signal_function(22 /* SIGABRT */, arm32_sigabrt_handler);
        log_line(previous == (SignalHandlerFn)(ULONG_PTR)-1 ? "ARM32: signal(SIGABRT) FAILED" : "ARM32: SIGABRT handler installed");
    }
    if (set_invalid) {
        set_invalid((void *)arm32_invalid_parameter_handler);
        log_line("ARM32: CRT invalid-parameter handler installed");
    }
    if (set_purecall) {
        set_purecall((void *)arm32_purecall_handler);
        log_line("ARM32: CRT pure-call handler installed");
    }
}

/* ---- D3DCompile / D3DPreprocess: pass-through with failure logging (facts gfx_compat, fl91) ---- */

typedef HRESULT (*GameD3DPreprocessFn)(
    LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, ID3DBlob **, ID3DBlob **);
static GameD3DPreprocessFn original_d3d_preprocess;

/* log a compiler message blob line by line (HLSL errors are multi-line) */
static void arm32_iat_log_blob_text(const char *label, ID3DBlob *blob)
{
    const char *text;
    SIZE_T size;
    SIZE_T offset = 0;
    char line[440];

    if (!blob) return;
    text = (const char *)ID3D10Blob_GetBufferPointer(blob);
    size = ID3D10Blob_GetBufferSize(blob);
    if (!text || !size || IsBadReadPtr(text, size)) return;
    if (size > 6000) size = 6000;
    while (offset < size) {
        SIZE_T length = 0;
        SIZE_T prefix = (SIZE_T)wsprintfA(line, "  %s: ", label);
        while (offset + length < size && length < 380 && text[offset + length] != '\n' && text[offset + length] != 0)
            ++length;
        if (length) {
            SIZE_T index;
            for (index = 0; index < length; ++index) {
                char value = text[offset + index];
                line[prefix + index] = (value == '\r' || value == '\t') ? ' ' : value;
            }
            line[prefix + length] = 0;
            log_line(line);
        }
        offset += length;
        if (offset < size && text[offset] == 0) break;
        if (offset < size && text[offset] == '\n') ++offset;
    }
}

/* "NAME=VALUE NAME=VALUE" of a D3D_SHADER_MACRO array (the game passes only VERSION=<feature level>) */
static void arm32_iat_format_macros(char *out, SIZE_T out_size, const D3D_SHADER_MACRO *defines)
{
    SIZE_T used = 0;
    UINT index;

    out[0] = 0;
    if (!defines) {
        wsprintfA(out, "(none)");
        return;
    }
    for (index = 0; index < 8 && used + 48 < out_size; ++index) {
        if (IsBadReadPtr(defines + index, sizeof(*defines)) || !defines[index].Name) break;
        used += (SIZE_T)wsprintfA(out + used, "%.20s=%.20s ", defines[index].Name,
                                  defines[index].Definition ? defines[index].Definition : "");
    }
}

static HRESULT win32_d3d_compile(
    LPCVOID source, SIZE_T source_size, LPCSTR source_name,
    const D3D_SHADER_MACRO *defines, ID3DInclude *include_handler,
    LPCSTR entry_point, LPCSTR target, UINT flags1, UINT flags2,
    ID3DBlob **code, ID3DBlob **errors)
{
    static volatile LONG compile_count;
    static volatile LONG failure_count;
    static volatile LONG warning_count;
    HRESULT result;
    LONG number;
    char macros[128];
    char line[480];

    /* include_handler (the game's HeaderInclude) and every other argument are forwarded unchanged */
    result = original_d3d_compile(
        source, source_size, source_name, defines, include_handler,
        entry_point, target, flags1, flags2, code, errors);
    number = InterlockedIncrement(&compile_count);
    if (FAILED(result)) {
        if (InterlockedIncrement(&failure_count) <= 64) {
            arm32_iat_format_macros(macros, sizeof(macros), defines);
            wsprintfA(line,
                "D3DCompile #%ld FAILED source=%.100s entry=%.20s target=%.40s bytes=%u flags=0x%X/0x%X macros=[%s] include=%p: HRESULT 0x%08lX",
                (long)number, source_name ? source_name : "<null>", entry_point ? entry_point : "<null>",
                target ? target : "<null>", (unsigned)source_size, flags1, flags2, macros, (void *)include_handler,
                (unsigned long)result);
            log_line(line);
            if (errors) arm32_iat_log_blob_text("D3DCompile error", *errors);
        }
    } else if (number <= 3) {
        arm32_iat_format_macros(macros, sizeof(macros), defines);
        wsprintfA(line, "D3DCompile #%ld ok: source=%.100s entry=%.20s target=%.40s bytes=%u macros=[%s]",
                  (long)number, source_name ? source_name : "<null>", entry_point ? entry_point : "<null>",
                  target ? target : "<null>", (unsigned)source_size, macros);
        log_line(line);
    } else if (errors && *errors && InterlockedIncrement(&warning_count) <= 8) {
        wsprintfA(line, "D3DCompile #%ld produced compiler messages (target=%.40s):", (long)number,
                  target ? target : "<null>");
        log_line(line);
        arm32_iat_log_blob_text("D3DCompile message", *errors);
    }
    return result;
}

static HRESULT win32_d3d_preprocess(
    LPCVOID source, SIZE_T source_size, LPCSTR source_name,
    const D3D_SHADER_MACRO *defines, ID3DInclude *include_handler,
    ID3DBlob **code_text, ID3DBlob **errors)
{
    static volatile LONG failure_count;
    HRESULT result = original_d3d_preprocess(
        source, source_size, source_name, defines, include_handler, code_text, errors);

    /* facts gfx_compat verdict: when D3DPreprocess fails the game silently abandons the stage without ever
     * reaching D3DCompile, so this is the only place where an include/preprocess error becomes visible. */
    if (FAILED(result) && InterlockedIncrement(&failure_count) <= 64) {
        char macros[128];
        char line[400];
        arm32_iat_format_macros(macros, sizeof(macros), defines);
        wsprintfA(line,
            "D3DPreprocess FAILED source=%.100s bytes=%u macros=[%s] include=%p: HRESULT 0x%08lX",
            source_name ? source_name : "<null>", (unsigned)source_size, macros, (void *)include_handler,
            (unsigned long)result);
        log_line(line);
        if (errors) arm32_iat_log_blob_text("D3DPreprocess error", *errors);
    }
    return result;
}

static BOOL install_d3d_compile_trace(BYTE *image, SIZE_T d3d_compile_rva)
{
    if (!arm32_iat_hook_import(image, FALSE, "D3DCompile", (DWORD)d3d_compile_rva, (void *)win32_d3d_compile,
                           (void **)&original_d3d_compile, "D3DCompile")) {
        log_line("ARM32: D3DCompile logger NOT installed (the game keeps using the system compiler directly)");
        return FALSE;
    }
    log_line("ARM32: D3DCompile pass-through logger installed");
    return TRUE;
}

static BOOL install_d3d_preprocess_trace(BYTE *image, SIZE_T d3d_preprocess_rva)
{
    if (!arm32_iat_hook_import(image, FALSE, "D3DPreprocess", (DWORD)d3d_preprocess_rva, (void *)win32_d3d_preprocess,
                           (void **)&original_d3d_preprocess, "D3DPreprocess")) {
        log_line("ARM32: D3DPreprocess logger NOT installed");
        return FALSE;
    }
    log_line("ARM32: D3DPreprocess pass-through logger installed");
    return TRUE;
}

/* ---- D3D11CreateDevice: BGRA flag + feature-level clamp (facts fl91.hook_force_fl91) ---- */

static void arm32_iat_format_levels(char *out, SIZE_T out_size, const UINT *levels, UINT count)
{
    SIZE_T used = 0;
    UINT index;

    out[0] = 0;
    if (!levels || !count) {
        wsprintfA(out, "(runtime default list)");
        return;
    }
    if (IsBadReadPtr(levels, (UINT_PTR)count * sizeof(*levels))) {
        wsprintfA(out, "(unreadable list %p)", (const void *)levels);
        return;
    }
    for (index = 0; index < count && index < 12 && used + 8 < out_size; ++index)
        used += (SIZE_T)wsprintfA(out + used, "%04X ", levels[index]);
}

/* No WARP retry here: the game retries once with D3D_DRIVER_TYPE_WARP itself (InitDevice 0x5BC3EC) and calls this hook
 * again, so the clamp applies to the retry as well.  No captured device either (the game's own QIs work natively). */
static HRESULT win32_d3d11_create_device(
    void *adapter, UINT driver_type, HMODULE software, UINT flags,
    const UINT *feature_levels, UINT feature_level_count, UINT sdk_version,
    ID3D11Device **device, UINT *selected_feature_level,
    ID3D11DeviceContext **immediate_context)
{
    static const UINT ladder[] = { 0xb100u, 0xb000u, 0xa100u, 0xa000u, 0x9300u, 0x9200u, 0x9100u };
    UINT clamped[16];
    UINT clamped_count = 0;
    UINT index;
    UINT adjusted_flags = flags | 0x20U; /* D3D11_CREATE_DEVICE_BGRA_SUPPORT */
    UINT cap = arm32_feature_level_cap;
    const UINT *levels = feature_levels;
    UINT level_count = feature_level_count;
    HRESULT result;
    char requested[96];
    char effective[96];
    char line[400];

    arm32_iat_format_levels(requested, sizeof(requested), feature_levels, feature_level_count);
    if (cap) {
        if (feature_levels && feature_level_count && feature_level_count <= 64 &&
            !IsBadReadPtr(feature_levels, (UINT_PTR)feature_level_count * sizeof(*feature_levels))) {
            for (index = 0; index < feature_level_count; ++index) {
                if (feature_levels[index] <= cap && clamped_count < ARRAYSIZE(clamped))
                    clamped[clamped_count++] = feature_levels[index];
            }
        } else if (!feature_levels || !feature_level_count) {
            for (index = 0; index < ARRAYSIZE(ladder); ++index) {
                if (ladder[index] <= cap) clamped[clamped_count++] = ladder[index];
            }
        }
        if (!clamped_count) clamped[clamped_count++] = cap;
        levels = clamped;
        level_count = clamped_count;
    }
    arm32_iat_format_levels(effective, sizeof(effective), levels, level_count);
    wsprintfA(line,
        "D3D11CreateDevice request: adapter=%p driver=%u flags=0x%X sdk=%u levels(%u)=[%s] -> cap=0x%04X flags=0x%X levels(%u)=[%s]",
        adapter, driver_type, flags, sdk_version, feature_level_count, requested, cap, adjusted_flags,
        level_count, effective);
    log_line(line);

    result = original_d3d11_create_device(
        adapter, driver_type, software, adjusted_flags,
        levels, level_count, sdk_version,
        device, selected_feature_level, immediate_context);

    if (SUCCEEDED(result)) {
        wsprintfA(line,
            "D3D11CreateDevice driver=%u OK: HRESULT 0x%08lX selected feature level=0x%04X device=%p context=%p",
            driver_type, (unsigned long)result, selected_feature_level ? *selected_feature_level : 0,
            device ? (void *)*device : NULL, immediate_context ? (void *)*immediate_context : NULL);
    } else {
        wsprintfA(line, "D3D11CreateDevice driver=%u FAILED: HRESULT 0x%08lX%s", driver_type,
                  (unsigned long)result,
                  driver_type == 1U ? " (hardware; the game retries once with WARP)"
                  : (driver_type == 5U ? " (WARP also failed: the game dereferences the null context next)" : ""));
    }
    log_line(line);
    return result;
}
#else
static void WINAPI win32_cxx_throw(void *exception, const void *throw_info)
{
    void *frames[24];
    USHORT count;
    USHORT index;
    char line[96];

    log_line("game raised a C++ exception; stack follows");
    if (host_is_115 || host_is_116) dump_115_stdio_state();
    count = RtlCaptureStackBackTrace(0, 24, frames, NULL);
    for (index = 0; index < count; ++index) {
        wsprintfA(line, "  frame %u: %p", (unsigned)index, frames[index]);
        log_line(line);
    }
    original_cxx_throw(exception, throw_info);
}

static BOOL install_exception_trace(BYTE *image, SIZE_T cxx_throw_rva)
{
    GameCxxThrowFn *slot =
        (GameCxxThrowFn *)(image + cxx_throw_rva);
    DWORD old_protection;
    DWORD ignored;

    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protection)) {
        return FALSE;
    }
    original_cxx_throw = *slot;
    *slot = win32_cxx_throw;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    log_line("C++ exception trace installed");
    return TRUE;
}

static HRESULT WINAPI win32_d3d_compile(
    LPCVOID source, SIZE_T source_size, LPCSTR source_name,
    const D3D_SHADER_MACRO *defines, ID3DInclude *include_handler,
    LPCSTR entry_point, LPCSTR target, UINT flags1, UINT flags2,
    ID3DBlob **code, ID3DBlob **errors)
{
    ID3DInclude *actual_include = include_handler;
    Win32IncludeProxy include_proxy;
    HRESULT result;
    const char *slash;
    const char *backslash;
    SIZE_T directory_length;

    if (include_handler == D3D_COMPILE_STANDARD_FILE_INCLUDE) {
        shader_include_directory[0] = 0;
        slash = source_name ? strrchr(source_name, '/') : NULL;
        backslash = source_name ? strrchr(source_name, '\\') : NULL;
        if (!slash || (backslash && backslash > slash)) slash = backslash;
        if (slash) {
            directory_length = (SIZE_T)(slash - source_name) + 1;
            if (directory_length < sizeof(shader_include_directory)) {
                CopyMemory(shader_include_directory, source_name,
                           directory_length);
                shader_include_directory[directory_length] = 0;
            }
        }
        actual_include = &win32_include;
    } else if (include_handler) {
        include_proxy.interface.lpVtbl = &win32_include_proxy_vtable;
        include_proxy.original = include_handler;
        include_proxy.allocations = NULL;
        actual_include = &include_proxy.interface;
    }
    result = original_d3d_compile(
        source, source_size, source_name, defines, actual_include,
        entry_point, target, flags1, flags2, code, errors);
    if (FAILED(result)) {
        char line[512];
        const char *error_text = NULL;
        SIZE_T error_size = 0;
        SIZE_T copy_size;
        wsprintfA(
            line,
            "D3DCompile failed source=%s entry=%s target=%s bytes=%u: HRESULT 0x%08lX",
            source_name ? source_name : "<null>",
            entry_point ? entry_point : "<null>",
            target ? target : "<null>", (unsigned)source_size,
            (unsigned long)result);
        log_line(line);
        if (errors && *errors) {
            error_text = (const char *)ID3D10Blob_GetBufferPointer(*errors);
            error_size = ID3D10Blob_GetBufferSize(*errors);
        }
        if (error_text && error_size) {
            copy_size = error_size < sizeof(line) - 1
                ? error_size : sizeof(line) - 1;
            CopyMemory(line, error_text, copy_size);
            line[copy_size] = 0;
            log_line(line);
        }
    }
    if (include_handler &&
        include_handler != D3D_COMPILE_STANDARD_FILE_INCLUDE) {
        while (include_proxy.allocations) {
            win32_include_proxy_close(
                &include_proxy.interface,
                (const void *)(include_proxy.allocations + 1));
        }
    }
    return result;
}

static BOOL install_d3d_compile_trace(BYTE *image, SIZE_T d3d_compile_rva)
{
    GameD3DCompileFn *slot =
        (GameD3DCompileFn *)(image + d3d_compile_rva);
    DWORD old_protection;
    DWORD ignored;

    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protection)) {
        log_line("VirtualProtect D3DCompile IAT failed");
        return FALSE;
    }
    original_d3d_compile = *slot;
    *slot = win32_d3d_compile;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    log_line("D3DCompile Win32 include compatibility installed");
    return TRUE;
}

static HRESULT WINAPI win32_d3d11_create_device(
    void *adapter, UINT driver_type, HMODULE software, UINT flags,
    const UINT *feature_levels, UINT feature_level_count, UINT sdk_version,
    ID3D11Device **device, UINT *selected_feature_level,
    ID3D11DeviceContext **immediate_context)
{
    UINT compatible_levels[16];
    UINT compatible_count = 0;
    UINT index;
    UINT adjusted_flags = flags | 0x20U; /* D3D11_CREATE_DEVICE_BGRA_SUPPORT */
    const UINT *levels = feature_levels;
    UINT level_count = feature_level_count;
    HRESULT result;
    char line[192];

    if (host_is_windows7 && feature_levels && feature_level_count) {
        for (index = 0; index < feature_level_count && compatible_count < 16; ++index) {
            /* The Windows 7 D3D11 runtime rejects a list containing 11_1
             * with E_INVALIDARG, even when lower levels are available. */
            if (feature_levels[index] != 0xb100U) {
                compatible_levels[compatible_count++] = feature_levels[index];
            }
        }
        if (compatible_count) {
            levels = compatible_levels;
            level_count = compatible_count;
        }
    }

    result = original_d3d11_create_device(
        adapter, driver_type, software, adjusted_flags,
        levels, level_count, sdk_version,
        device, selected_feature_level, immediate_context);
    wsprintfA(line,
        "D3D11CreateDevice compat: driver=%u flags=0x%X levels=%u result=0x%08lX selected=0x%X device=%p context=%p",
        driver_type, adjusted_flags, level_count, (unsigned long)result,
        selected_feature_level ? *selected_feature_level : 0,
        device ? *device : NULL,
        immediate_context ? *immediate_context : NULL);
    log_line(line);
    if (host_is_116) {
        log_pointer("1.16 D3D11 device output slot", device);
        log_pointer("1.16 D3D11 context output slot", immediate_context);
    }

    if (FAILED(result) && driver_type == 1U) {
        if (device) *device = NULL;
        if (immediate_context) *immediate_context = NULL;
        if (selected_feature_level) *selected_feature_level = 0;
        result = original_d3d11_create_device(
            NULL, 5U, NULL, adjusted_flags,
            levels, level_count, sdk_version,
            device, selected_feature_level, immediate_context);
        wsprintfA(line,
            "D3D11CreateDevice WARP fallback: result=0x%08lX selected=0x%X device=%p context=%p",
            (unsigned long)result,
            selected_feature_level ? *selected_feature_level : 0,
            device ? *device : NULL,
            immediate_context ? *immediate_context : NULL);
        log_line(line);
    }

    if (SUCCEEDED(result) && device && *device &&
        immediate_context && *immediate_context) {
        if (host_is_116) {
            install_clear_view_116(*immediate_context);
            IDXGISwapChain **renderer_swap_chain = (IDXGISwapChain **)(
                (BYTE *)immediate_context - 0x3c);
            IDXGIDevice *dxgi_device = NULL;
            IDXGIAdapter *adapter = NULL;
            IDXGIFactory *factory = NULL;
            DXGI_SWAP_CHAIN_DESC description;
            RECT client;
            HRESULT swap_result;
            if (!*renderer_swap_chain && game_window &&
                SUCCEEDED(ID3D11Device_QueryInterface(
                    *device, &IID_IDXGIDevice, (void **)&dxgi_device)) &&
                SUCCEEDED(IDXGIDevice_GetAdapter(dxgi_device, &adapter)) &&
                SUCCEEDED(IDXGIAdapter_GetParent(
                    adapter, &IID_IDXGIFactory, (void **)&factory))) {
                GetClientRect(game_window, &client);
                ZeroMemory(&description, sizeof(description));
                description.BufferDesc.Width = client.right - client.left;
                description.BufferDesc.Height = client.bottom - client.top;
                description.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                description.SampleDesc.Count = 1;
                description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                description.BufferCount = 1;
                description.OutputWindow = game_window;
                description.Windowed = TRUE;
                description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
                swap_result = IDXGIFactory_CreateSwapChain(
                    factory, (IUnknown *)*device, &description,
                    renderer_swap_chain);
                log_hresult("1.16 IDXGIFactory::CreateSwapChain(HWND)",
                            swap_result);
                if (SUCCEEDED(swap_result)) {
                    game_swap_chain = *renderer_swap_chain;
                    IDXGISwapChain_AddRef(game_swap_chain);
                    log_pointer("1.16 HWND swap chain installed at renderer+0x134",
                                renderer_swap_chain);
                }
            }
            if (factory) IDXGIFactory_Release(factory);
            if (adapter) IDXGIAdapter_Release(adapter);
            if (dxgi_device) IDXGIDevice_Release(dxgi_device);
        }
        if (captured_base_device) {
            ID3D11Device_Release(captured_base_device);
        }
        if (captured_base_context) {
            ID3D11DeviceContext_Release(captured_base_context);
        }
        captured_base_device = *device;
        captured_base_context = *immediate_context;
        ID3D11Device_AddRef(captured_base_device);
        ID3D11DeviceContext_AddRef(captured_base_context);
        log_line("captured base D3D11 device/context before UWP Device2 QI");
    }
    return result;
}

#endif

static BOOL patch_win7_device2_queries_to_base(
    BYTE *image, SIZE_T device_iid_rva, SIZE_T context_iid_rva)
{
    /* The original UWP DeviceResources code queries these two unique
     * constants immediately after D3D11CreateDevice:
     *   image+0x67F56C = IID_ID3D11Device2
     *   image+0x67F55C = IID_ID3D11DeviceContext2
     * Both interfaces require Windows 8.1.  Replacing only the IIDs with
     * their inherited base interfaces lets the original assignment and
     * AddRef/Release code run unchanged on Windows 7. */
    GUID *device_iid = (GUID *)(image + device_iid_rva);
    GUID *context_iid = (GUID *)(image + context_iid_rva);
    DWORD old_device = 0, old_context = 0, ignored = 0;

    if (!host_is_windows7) {
        return TRUE;
    }
    if (!VirtualProtect(device_iid, sizeof(*device_iid), PAGE_READWRITE,
                        &old_device) ||
        !VirtualProtect(context_iid, sizeof(*context_iid), PAGE_READWRITE,
                        &old_context)) {
        log_line("Windows 7 D3D base-IID patch protection failed; retained-pointer fallback remains active");
        return FALSE;
    }
    memcpy(device_iid, &IID_ID3D11Device, sizeof(*device_iid));
    memcpy(context_iid, &IID_ID3D11DeviceContext, sizeof(*context_iid));
    VirtualProtect(device_iid, sizeof(*device_iid), old_device, &ignored);
    VirtualProtect(context_iid, sizeof(*context_iid), old_context, &ignored);
    FlushInstructionCache(GetCurrentProcess(), device_iid, sizeof(*device_iid));
    FlushInstructionCache(GetCurrentProcess(), context_iid, sizeof(*context_iid));
    log_line("Windows 7 D3D: Device2/Context2 QueryInterface IIDs replaced with base interfaces");
    return TRUE;
}

static BOOL patch_win7_dxgi_device3_query_to_base(
    BYTE *image, SIZE_T dxgi_device3_iid_rva)
{
    /*
     * Minecraft 1.1.5 performs an optional video-memory-budget probe during
     * DeviceResources initialization:
     *
     *   ID3D11Device -> IDXGIDevice3 -> GetAdapter -> IDXGIAdapter3
     *
     * Windows 7 has only IDXGIDevice. The game logs the failed QI but then
     * unconditionally dereferences the null IDXGIDevice3 pointer at
     * VA 0x00B0DFC5. Downgrading only the first IID to inherited
     * IDXGIDevice keeps GetAdapter valid. The following IDXGIAdapter3 query
     * is intentionally left unchanged; its null result is checked by the
     * original code, so the optional budget/reservation calls are skipped.
     */
    static const GUID iid_idxgi_device3 = {
        0x6007896cU, 0x3244, 0x4afd,
        {0xbf,0x18,0xa6,0xd3,0xbe,0xda,0x50,0x23}
    };
    GUID *iid;
    DWORD old_protection = 0;
    DWORD ignored = 0;

    if (!host_is_windows7 || (!host_is_115 && !host_is_128)) {
        return TRUE;
    }

    iid = (GUID *)(image + dxgi_device3_iid_rva);
    if (memcmp(iid, &iid_idxgi_device3, sizeof(*iid)) != 0 &&
        memcmp(iid, &IID_IDXGIDevice, sizeof(*iid)) != 0) {
        log_line("Windows 7 DXGI Device3 IID patch refused: unexpected 1.1.5 bytes");
        return FALSE;
    }

    if (!VirtualProtect(iid, sizeof(*iid), PAGE_READWRITE, &old_protection)) {
        log_line("Windows 7 DXGI Device3 IID patch protection failed");
        return FALSE;
    }
    memcpy(iid, &IID_IDXGIDevice, sizeof(*iid));
    VirtualProtect(iid, sizeof(*iid), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), iid, sizeof(*iid));
    log_line("Windows 7 DXGI: IDXGIDevice3 query downgraded to IDXGIDevice");
    return TRUE;
}

static BOOL patch_win7_discard_views(BYTE *image)
{
    SIZE_T function_rva = host_is_128
        ? RVA_128_DISCARD_WINDOW_VIEWS
        : (host_is_115 ? RVA_115_DISCARD_WINDOW_VIEWS
                       : RVA_DISCARD_WINDOW_VIEWS);
    BYTE *function = image + function_rva;
    DWORD old_protection;
    DWORD ignored;

    if (!host_is_windows7) {
        return TRUE;
    }

    /*
     * Renderer::discardWindowSizeDependentResources calls a D3D11.1-only
     * DiscardView method for the color and depth views.  In Minecraft 1.1.5
     * the ID3D11DeviceContext2 QI output is renderer+0x110.  Windows 7 leaves
     * that field null, but VA 0x00AD0DA0 dereferences it without a guard at
     * VA 0x00AD0DB9.  DiscardView is only a bandwidth hint, so returning from
     * this tiny helper preserves the actual render/tick path and avoids
     * emulating the full D3D11.1/11.2 context vtable.
     */
    if ((host_is_115 || host_is_128) &&
        function[0] != 0xa1 && function[0] != 0xc3) {
        log_line("Minecraft 1.1.5 Windows 7 DiscardView patch refused: unexpected function prologue");
        return FALSE;
    }
    if (!VirtualProtect(function, 1, PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        log_line("Windows 7 DiscardView compatibility patch failed");
        return FALSE;
    }
    *function = 0xc3; /* ret */
    VirtualProtect(function, 1, old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), function, 1);
    log_line(host_is_128
        ? "Minecraft 1.2.8 Windows 7 DiscardView calls disabled"
        : (host_is_115
            ? "Minecraft 1.1.5 Windows 7 D3D11.1 DiscardView calls disabled"
            : "Windows 7 D3D11.1 DiscardView calls disabled"));
    return TRUE;
}

#ifdef WIN32CRAFT_ARM32
static BOOL install_d3d11_device_compat(
    BYTE *image, SIZE_T d3d11_create_device_rva)
{
    char line[160];

    if (!arm32_iat_hook_import(image, FALSE, "D3D11CreateDevice", (DWORD)d3d11_create_device_rva,
                           (void *)win32_d3d11_create_device, (void **)&original_d3d11_create_device,
                           "D3D11CreateDevice")) {
        log_line("ARM32: D3D11CreateDevice hook NOT installed");
        return FALSE;
    }
    wsprintfA(line, "ARM32: D3D11CreateDevice hook installed (BGRA flag, feature level cap 0x%04X)",
              (unsigned)arm32_feature_level_cap);
    log_line(line);
    return TRUE;
}
#else
static BOOL install_d3d11_device_compat(
    BYTE *image, SIZE_T d3d11_create_device_rva)
{
    GameD3D11CreateDeviceFn *slot =
        (GameD3D11CreateDeviceFn *)(image + d3d11_create_device_rva);
    DWORD old_protection;
    DWORD ignored;

    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protection)) {
        log_line("D3D11CreateDevice IAT hook protection failed");
        return FALSE;
    }
    original_d3d11_create_device = *slot;
    *slot = win32_d3d11_create_device;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    log_line("installed Windows 7 D3D11CreateDevice compatibility hook");
    return TRUE;
}
#endif

#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 gfx_bootstrap (unit gfx_bootstrap).  Facts: gfx_device.*, fl91.* (sections 1, 3, 5, 6), gfx_compat (present/scissor).
 * Contents: host-created swap chain helpers, FL9_x size clamp, owner+0x94/+0x98 mirroring, per-frame target binding,
 * Present (vtable slot 8) and Present1 (slot 22) hooks with the DEVICE_REMOVED/RESET policy.
 * Helpers other units may call (all defined before initialize_game_d3d / resize_game_window):
 *   arm32_gfx_mirror_client_size(w, h)      keep owner+0x94/+0x98 (and the renderer's embedded display w/h) equal to the size
 *                                           that is written to platform+0x330/+0x334
 *   arm32_gfx_get_backbuffer_size(&w, &h)   size of the swap-chain buffers actually created (clamped for FL9_x)
 *   arm32_gfx_bind_frame_targets()          per-frame RSSetViewports + OMSetRenderTargets from the renderer's live views
 *   arm32_gfx_device_is_lost()              TRUE once the game's Present reported DEVICE_REMOVED/RESET
 * ====================================================================================================== */
#define ARM32_GFX_PRESENT_ALLOW_TEARING    0x00000200U
#define ARM32_GFX_HR_DEVICE_REMOVED        ((HRESULT)0x887A0005L)
#define ARM32_GFX_HR_DEVICE_HUNG           ((HRESULT)0x887A0006L)
#define ARM32_GFX_HR_DEVICE_RESET          ((HRESULT)0x887A0007L)
#define ARM32_GFX_OWNER_CACHED_WIDTH       0x94u   /* facts gfx_device.owner_other_fields: int, copied to platform+0x330 by vf143 */
#define ARM32_GFX_OWNER_CACHED_HEIGHT      0x98u   /* int, copied to platform+0x334 */
#define ARM32_GFX_RENDERER_DISPLAY_WIDTH   0xe8u   /* embedded display descriptor (renderer+0xe8): u32 width */
#define ARM32_GFX_RENDERER_DISPLAY_HEIGHT  0xecu   /* u32 height */
#define ARM32_GFX_FALLBACK_WIDTH           1280u
#define ARM32_GFX_FALLBACK_HEIGHT          720u

/* Plain AAPCS prototypes of the game functions (facts gfx_device.calling_convention); callers add the Thumb bit via ARM_CODE */
typedef void *(*Arm32GfxOperatorNewFn)(size_t);
typedef void *(*Arm32GfxOwnerCtorFn)(void *owner);
typedef void (*Arm32GfxInitDeviceFn)(void *owner, void *renderer);
typedef void (*Arm32GfxInitTargetsFn)(void *owner, const void *display, void *renderer);
typedef void (*Arm32GfxReleaseTargetsFn)(void *renderer);
typedef HRESULT (STDMETHODCALLTYPE *Arm32GfxPresent1Fn)(IDXGISwapChain *, UINT, UINT, const void *);

/* facts gfx_device.display_descriptor: identical to the x86 host struct (sizeof 0x20, stereo @0x19 must stay 0) */
typedef struct Arm32GfxDisplay {
    UINT width;
    UINT height;
    BYTE reserved_08[0x11];
    BYTE stereo;
    BYTE reserved_1a[2];
    IDXGISwapChain *swap_chain;
} Arm32GfxDisplay;
typedef char arm32_gfx_display_layout_check[
    (sizeof(Arm32GfxDisplay) == 0x20 &&
     __builtin_offsetof(Arm32GfxDisplay, stereo) == 0x19 &&
     __builtin_offsetof(Arm32GfxDisplay, swap_chain) == 0x1c) ? 1 : -1];

/* D3D11_TEXTURE2D_DESC (only used by the failure diagnostics) */
typedef struct Arm32GfxTex2DDesc {
    UINT Width, Height, MipLevels, ArraySize, Format;
    UINT SampleCount, SampleQuality;
    UINT Usage, BindFlags, CPUAccessFlags, MiscFlags;
} Arm32GfxTex2DDesc;

/* IID_IDXGISwapChain1 {790a45f7-0d42-4876-983a-0a55cfe6f4aa} */
static const GUID arm32_gfx_iid_swap_chain1 = {
    0x790a45f7U, 0x0d42, 0x4876, {0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa}
};

static Arm32GfxPresent1Fn arm32_original_present1;
static volatile LONG arm32_gfx_device_lost;
static UINT arm32_gfx_width;             /* size of the swap-chain buffers / render targets actually created */
static UINT arm32_gfx_height;
static UINT arm32_gfx_texture_limit = 2048u; /* FL9_1 until InitDevice reports the real level */
static BOOL arm32_gfx_resizing;
static BOOL arm32_gfx_scissor_installed;

static BYTE *arm32_gfx_image(void)
{
    return game_image ? game_image : (BYTE *)GetModuleHandleW(NULL);
}

/* Largest texture / render-target dimension of a D3D feature level (fl91 sections 3 and 6: 2048 on 9_1/9_2, 4096 on 9_3) */
static UINT arm32_gfx_texture_limit_for_level(UINT feature_level)
{
    if (feature_level >= 0xB000u) return 16384u;
    if (feature_level >= 0xA000u) return 8192u;
    if (feature_level >= 0x9300u) return 4096u;
    return 2048u;
}

/* Keep the aspect ratio when a requested backbuffer exceeds the limit (float math: no 32-bit divide helper on Cortex-A9) */
static void arm32_gfx_clamp_size(UINT *width, UINT *height, const char *who)
{
    UINT w = *width;
    UINT h = *height;
    UINT limit = arm32_gfx_texture_limit;
    UINT larger;
    float scale;
    char line[192];

    if (w <= limit && h <= limit) return;
    larger = w > h ? w : h;
    scale = (float)limit / (float)larger;
    w = (UINT)((float)w * scale + 0.5f);
    h = (UINT)((float)h * scale + 0.5f);
    if (w < 1u) w = 1u;
    if (h < 1u) h = 1u;
    if (w > limit) w = limit;
    if (h > limit) h = limit;
    wsprintfA(line, "%s: requested %ux%u exceeds the %u px texture limit of this feature level; clamped to %ux%u",
              who, *width, *height, limit, w, h);
    log_line(line);
    *width = w;
    *height = h;
}

/* audit.json gap: AppPlatform_Winrt::vf143 (device re-init) copies owner+0x94/+0x98 back into platform+0x330/+0x334, so they
 * must equal the size the host writes there.  The renderer's embedded display descriptor (renderer+0xe8/+0xec) is what the
 * game itself updates after every swap-chain resize, so keep it in step too. */
static void arm32_gfx_mirror_client_size(int width, int height)
{
    BYTE *owner = (BYTE *)game_resource_owner;
    BYTE *renderer = (BYTE *)game_renderer;

    if (owner) {
        *(int *)(owner + ARM32_GFX_OWNER_CACHED_WIDTH) = width;
        *(int *)(owner + ARM32_GFX_OWNER_CACHED_HEIGHT) = height;
    }
    if (renderer) {
        *(UINT *)(renderer + ARM32_GFX_RENDERER_DISPLAY_WIDTH) = (UINT)width;
        *(UINT *)(renderer + ARM32_GFX_RENDERER_DISPLAY_HEIGHT) = (UINT)height;
    }
}

static void arm32_gfx_get_backbuffer_size(UINT *width, UINT *height)
{
    if (width) *width = arm32_gfx_width;
    if (height) *height = arm32_gfx_height;
}

static BOOL arm32_gfx_device_is_lost(void)
{
    return arm32_gfx_device_lost != 0;
}

/* Device removed/reset policy (audit.json "present result handling", facts gfx_device.present_path): the game maps these
 * HRESULTs to GameOwner::releaseDevice, which Releases the owner device (+0xc0) and the renderer swap chain (+0x104) that
 * the host created and keeps using.  The host cannot rebuild the HWND swap chain behind the game's back, so: log the
 * reason once, tell the game S_OK (it never sees the error), stop touching the dead device and close the window for a
 * clean exit instead of letting the game tear down host-owned objects. */
static void arm32_gfx_note_device_lost(HRESULT result, const char *where)
{
    char line[256];
    HRESULT reason = S_OK;
    UINT feature_level = 0;
    BYTE *owner = (BYTE *)game_resource_owner;
    ID3D11Device *device = owner
        ? *(ID3D11Device **)(owner + ARM_GFX_DEVICE_OWNER_DEVICE_OFFSET) : NULL;

    if (InterlockedExchange(&arm32_gfx_device_lost, 1)) return;
    if (device && !IsBadReadPtr(device, sizeof(void *)) &&
        device->lpVtbl && !IsBadReadPtr(device->lpVtbl, 40 * sizeof(void *))) {
        /* ID3D11Device vtable: GetFeatureLevel = 37, GetDeviceRemovedReason = 39 */
        feature_level = COMCALL(device, 37, UINT, STDMETHODCALLTYPE)(device);
        reason = COMCALL(device, 39, HRESULT, STDMETHODCALLTYPE)(device);
    }
    wsprintfA(line, "%s returned HRESULT 0x%08lX (device removed/reset); GetDeviceRemovedReason=0x%08lX feature level=0x%04X",
              where, (unsigned long)result, (unsigned long)reason, feature_level);
    log_line(line);
    log_line("gfx: GPU device lost; reporting S_OK to the game so GameOwner::releaseDevice does not free host-owned objects; closing the window");
    if (game_window) PostMessageW(game_window, WM_CLOSE, 0, 0);
}

/* Per-frame binding used instead of the x86 inline code in render_game_target_128 (reads the renderer's LIVE views so a
 * game-side target recreation cannot leave the host bound to stale views; viewport = real swap-chain buffer size). */
static BOOL arm32_gfx_bind_frame_targets(void)
{
    static unsigned target_change_logs;
    BYTE *renderer = (BYTE *)game_renderer;
    ID3D11RenderTargetView *rtv;
    ID3D11DepthStencilView *dsv;
    D3D11_VIEWPORT viewport;
    RECT client;
    UINT width = arm32_gfx_width;
    UINT height = arm32_gfx_height;

    if (!renderer || !game_context || arm32_gfx_device_lost) return FALSE;
    rtv = *(ID3D11RenderTargetView **)(renderer + ARM_GFX_DEVICE_RENDERER_RTV);
    dsv = *(ID3D11DepthStencilView **)(renderer + ARM_GFX_DEVICE_RENDERER_DSV);
    if (!rtv) return FALSE;
    if (rtv != game_target) {
        ID3D11RenderTargetView *old = game_target;
        if (target_change_logs < 8) {
            char line[160];
            ++target_change_logs;
            wsprintfA(line, "gfx: renderer render-target view changed %p -> %p (dsv %p)", (void *)old, (void *)rtv, (void *)dsv);
            log_line(line);
        }
        ID3D11RenderTargetView_AddRef(rtv);
        game_target = rtv;
        if (old) ID3D11RenderTargetView_Release(old);
    }
    if ((!width || !height) && game_window && GetClientRect(game_window, &client)) {
        width = (UINT)(client.right - client.left);
        height = (UINT)(client.bottom - client.top);
    }
    ZeroMemory(&viewport, sizeof(viewport));
    viewport.Width = (float)width;
    viewport.Height = (float)height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    ID3D11DeviceContext_RSSetViewports(game_context, 1, &viewport);
    ID3D11DeviceContext_OMSetRenderTargets(game_context, 1, &game_target, dsv);
    return TRUE;
}

/* Failure diagnostics for InitTargets (it only logs HRESULTs through the game's own log, which the user does not have) */
static void arm32_gfx_diagnose_targets(ID3D11Device *device, IDXGISwapChain *swap_chain, UINT width, UINT height)
{
    ID3D11Texture2D *backbuffer = NULL;
    ID3D11RenderTargetView *view = NULL;
    ID3D11Texture2D *depth = NULL;
    Arm32GfxTex2DDesc desc;
    HRESULT result;

    if (!device || !swap_chain) return;
    result = IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void **)&backbuffer);
    log_hresult("diag IDXGISwapChain::GetBuffer(0, ID3D11Texture2D)", result);
    if (SUCCEEDED(result) && backbuffer) {
        result = ID3D11Device_CreateRenderTargetView(device, backbuffer, NULL, &view);
        log_hresult("diag ID3D11Device::CreateRenderTargetView(backbuffer)", result);
        if (view) ID3D11RenderTargetView_Release(view);
        ID3D11Texture2D_Release(backbuffer);
    }
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = 0x2d; /* DXGI_FORMAT_D24_UNORM_S8_UINT, the only depth format valid on FL9_1 besides D16 */
    desc.SampleCount = 1;
    desc.BindFlags = 0x40; /* D3D11_BIND_DEPTH_STENCIL */
    result = COMCALL(device, 5, HRESULT, STDMETHODCALLTYPE, const void *, const void *, ID3D11Texture2D **)(
        device, &desc, NULL, &depth);
    log_hresult("diag ID3D11Device::CreateTexture2D(D24S8 depth)", result);
    if (depth) ID3D11Texture2D_Release(depth);
}

/* Runs the game's InitTargets (RTV renderer+0x118, DSV +0x120, back buffer +0x124 from GetBuffer; the depth texture is
 * transient) for the current swap chain and takes the host's own reference on the RTV (same as the x86 host). */
static BOOL arm32_gfx_recreate_targets(UINT width, UINT height, const char *why)
{
    Arm32GfxInitTargetsFn init_targets =
        (Arm32GfxInitTargetsFn)ARM_CODE(arm32_gfx_image(), ARM_GFX_DEVICE_INIT_TARGETS);
    BYTE *renderer = (BYTE *)game_renderer;
    Arm32GfxDisplay display;
    ID3D11RenderTargetView *rtv;
    void *dsv;
    void *backbuffer;
    char line[200];

    if (!renderer || !game_resource_owner || !game_swap_chain) return FALSE;
    if (game_target) {
        ID3D11RenderTargetView_Release(game_target);
        game_target = NULL;
    }
    ZeroMemory(&display, sizeof(display)); /* stereo (+0x19) stays 0: stereo needs Texture2DArray views (invalid on FL9_x) */
    display.width = width;
    display.height = height;
    display.swap_chain = game_swap_chain;
    wsprintfA(line, "gfx: calling game InitTargets (%s) %ux%u", why, width, height);
    log_line(line);
    init_targets(game_resource_owner, &display, game_renderer);
    rtv = *(ID3D11RenderTargetView **)(renderer + ARM_GFX_DEVICE_RENDERER_RTV);
    dsv = *(void **)(renderer + ARM_GFX_DEVICE_RENDERER_DSV);
    backbuffer = *(void **)(renderer + ARM_GFX_DEVICE_RENDERER_BACKBUFFER);
    log_pointer("game render target (+0x118)", rtv);
    log_pointer("game depth target (+0x120)", dsv);
    log_pointer("game backbuffer texture (+0x124)", backbuffer);
    if (!rtv || !dsv || !backbuffer) {
        log_line("gfx: InitTargets did not produce a complete render target set");
        arm32_gfx_diagnose_targets(*(ID3D11Device **)((BYTE *)game_resource_owner + ARM_GFX_DEVICE_OWNER_DEVICE_OFFSET),
                                   game_swap_chain, width, height);
        return FALSE;
    }
    ID3D11RenderTargetView_AddRef(rtv);
    game_target = rtv;
    arm32_gfx_width = width;
    arm32_gfx_height = height;
    arm32_gfx_mirror_client_size((int)width, (int)height);
    return TRUE;
}

/* One Present/Present1 policy for both hooks: synchronized presentation (Win32CraftPresentSyncInterval), no DO_NOT_WAIT,
 * ALLOW_TEARING masked (the host swap chain is created without DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING and owner+0x90 stays 0),
 * successful presents counted for render_game_target_128's fallback, DEVICE_REMOVED/RESET never forwarded to the game. */
static HRESULT arm32_gfx_present_common(
    IDXGISwapChain *swap_chain, UINT slot, UINT flags, const void *parameters)
{
    static unsigned present_count;
    static unsigned busy_drop_count;
    static unsigned failure_log_count;
    UINT actual_sync = Win32CraftPresentSyncInterval();
    UINT actual_flags = flags & ~(DXGI_PRESENT_DO_NOT_WAIT | ARM32_GFX_PRESENT_ALLOW_TEARING);
    HRESULT result;

    ++present_count;
    if (present_count == 1) {
        char line[192];
        wsprintfA(line, "game Present(slot %u) %u begin swap=%p sync=%u flags=0x%X (requested flags 0x%X)",
                  slot, present_count, swap_chain, actual_sync, actual_flags, flags);
        log_line(line);
    }
    if (arm32_gfx_device_lost) {
        InterlockedIncrement((LONG *)&game_present_success_count);
        return S_OK;
    }
    if (slot == 22) {
        result = arm32_original_present1
            ? arm32_original_present1(swap_chain, actual_sync, actual_flags, parameters) : E_FAIL;
    } else {
        result = original_swap_chain_present
            ? original_swap_chain_present(swap_chain, actual_sync, actual_flags) : E_FAIL;
    }
    if (result == ARM32_GFX_HR_DEVICE_REMOVED || result == ARM32_GFX_HR_DEVICE_RESET) {
        arm32_gfx_note_device_lost(result, slot == 22 ? "game Present1" : "game Present");
        InterlockedIncrement((LONG *)&game_present_success_count);
        return S_OK;
    }
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) {
        ++busy_drop_count;
        InterlockedIncrement((LONG *)&game_present_success_count);
        if (busy_drop_count == 1) log_line("VSync Present unexpectedly reported compositor busy");
        return S_OK;
    }
    if (SUCCEEDED(result)) {
        InterlockedIncrement((LONG *)&game_present_success_count);
    } else if (failure_log_count < 16) {
        ++failure_log_count;
        log_hresult(result == ARM32_GFX_HR_DEVICE_HUNG ? "game Present failed (DEVICE_HUNG, forwarded)"
                                                       : "game Present failed (forwarded)", result);
    }
    if (present_count == 1) {
        char line[96];
        wsprintfA(line, "game Present %u end: HRESULT 0x%08lX", present_count, (unsigned long)result);
        log_line(line);
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE win32_present(
    IDXGISwapChain *swap_chain, UINT sync_interval, UINT flags)
{
    (void)sync_interval; /* overridden by Win32CraftPresentSyncInterval */
    return arm32_gfx_present_common(swap_chain, 8, flags, NULL);
}

static HRESULT STDMETHODCALLTYPE win32_present1(
    IDXGISwapChain *swap_chain, UINT sync_interval, UINT flags, const void *parameters)
{
    (void)sync_interval;
    return arm32_gfx_present_common(swap_chain, 22, flags, parameters);
}

/* Hooks IDXGISwapChain::Present (slot 8, the game's default path) and, when the object is an IDXGISwapChain1 with the same
 * pointer, IDXGISwapChain1::Present1 (slot 22, used when [g_displaySettings+0x30] != 1).  The stored function pointers are
 * Thumb pointers (the compiler sets bit 0 on &function); the vtable is data, so PAGE_READWRITE (arm32_patch_cell). */
static BOOL install_swap_chain_present_hook(IDXGISwapChain *swap_chain)
{
    void **vtable;
    void *original = NULL;
    IDXGISwapChain *swap_chain1 = NULL;
    HRESULT result;
    BOOL present_ok;

    if (!swap_chain || !swap_chain->lpVtbl) return FALSE;
    vtable = swap_chain->lpVtbl;
    if (vtable[8] == (void *)win32_present) {
        present_ok = TRUE;
    } else {
        present_ok = arm32_patch_cell(&vtable[8], NULL, (void *)win32_present, &original,
                                      "IDXGISwapChain::Present (vtable slot 8)");
        if (present_ok) original_swap_chain_present = (SwapChainPresentFn)original;
    }
    if (!present_ok) {
        log_line("swap-chain Present vtable hook failed");
        return FALSE;
    }
    result = ID3D11Device_QueryInterface(swap_chain, &arm32_gfx_iid_swap_chain1, &swap_chain1);
    if (SUCCEEDED(result) && swap_chain1) {
        if (swap_chain1 == swap_chain) {
            if (vtable[22] == (void *)win32_present1) {
                /* already hooked */
            } else {
                original = NULL;
                if (arm32_patch_cell(&vtable[22], NULL, (void *)win32_present1, &original,
                                     "IDXGISwapChain1::Present1 (vtable slot 22)")) {
                    arm32_original_present1 = (Arm32GfxPresent1Fn)original;
                } else {
                    log_line("swap-chain Present1 vtable hook failed (Present hook is active)");
                }
            }
        } else {
            log_pointer("IDXGISwapChain1 interface pointer differs from the swap chain; Present1 not hooked", swap_chain1);
        }
        IDXGISwapChain_Release(swap_chain1);
    } else {
        log_hresult("swap chain QueryInterface(IDXGISwapChain1) failed; only Present is hooked", result);
    }
    log_line("gfx: swap-chain Present/Present1 hooks active");
    return TRUE;
}
#else /* !WIN32CRAFT_ARM32 */
static HRESULT STDMETHODCALLTYPE win32_present(
    IDXGISwapChain *swap_chain, UINT sync_interval, UINT flags)
{
    static unsigned present_count;
    HRESULT result;

    static unsigned busy_drop_count;
    UINT actual_sync = sync_interval;
    UINT actual_flags = flags;

    ++present_count;
    /* -maxfps disables VSync; there is no software frame limiter. */
    actual_sync = Win32CraftPresentSyncInterval();
    actual_flags &= ~DXGI_PRESENT_DO_NOT_WAIT;
    if (present_count == 1) {
        char line[160];
        wsprintfA(line,
                  "game Present %u begin swap=%p sync=%u flags=0x%X",
                  present_count, swap_chain, actual_sync, actual_flags);
        log_line(line);
    }
    result = original_swap_chain_present
        ? original_swap_chain_present(swap_chain, actual_sync, actual_flags)
        : E_FAIL;
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) {
        ++busy_drop_count;
        InterlockedIncrement((LONG *)&game_present_success_count);
        if (busy_drop_count == 1) {
            log_line("VSync Present unexpectedly reported compositor busy");
        }
        return S_OK;
    }
    if (SUCCEEDED(result)) {
        InterlockedIncrement((LONG *)&game_present_success_count);
    }
    if (present_count == 1 || FAILED(result)) {
        char line[128];
        wsprintfA(line, "game Present %u end: HRESULT 0x%08lX",
                  present_count, (unsigned long)result);
        log_line(line);
    }
    return result;
}

static BOOL install_swap_chain_present_hook(IDXGISwapChain *swap_chain)
{
    SwapChainPresentFn *slot;
    DWORD old_protection;
    DWORD ignored;

    if (!swap_chain || !swap_chain->lpVtbl) return FALSE;
    slot = (SwapChainPresentFn *)&swap_chain->lpVtbl[8];
    if (*slot == win32_present) return TRUE;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        log_line("swap-chain Present vtable hook protection failed");
        return FALSE;
    }
    original_swap_chain_present = *slot;
    *slot = win32_present;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    log_line("swap-chain Present vtable hook installed");
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 gfx_bootstrap present hooks */

static void __attribute__((thiscall)) win32_game_tick(
    void *minecraft_game, int tick_index, int last_tick_index)
{
    static unsigned tick_count;
    char line[128];

    ++tick_count;
    if (tick_count <= 10 || tick_count == 60) {
        wsprintfA(line, "MinecraftGame tick %u begin (%d/%d)",
                  tick_count, tick_index, last_tick_index);
        log_line(line);
    }
    original_game_tick(minecraft_game, tick_index, last_tick_index);
    if (tick_count <= 10 || tick_count == 60) {
        wsprintfA(line, "MinecraftGame tick %u end", tick_count);
        log_line(line);
    }
}

static BOOL install_game_tick_trace(BYTE *image)
{
    BYTE *site = image + 0x003b33a4;
    static const BYTE expected[5] = {0xe8, 0x97, 0x00, 0x00, 0x00};
    DWORD old_protection;
    DWORD ignored;
    INT_PTR displacement = (const BYTE *)win32_game_tick - (site + 5);

    original_game_tick = (GameTickFn)(image + 0x003b3440);
    if (memcmp(site, expected, sizeof(expected)) != 0 ||
        displacement < (INT_PTR)INT_MIN ||
        displacement > (INT_PTR)INT_MAX ||
        !VirtualProtect(site, sizeof(expected), PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        log_line("MinecraftGame tick trace patch failed");
        return FALSE;
    }
    site[0] = 0xe8;
    *(LONG *)(site + 1) = (LONG)displacement;
    VirtualProtect(site, sizeof(expected), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(expected));
    log_line("MinecraftGame tick trace installed");
    return TRUE;
}

static void __attribute__((thiscall)) win32_client_update(void *client)
{
    static unsigned update_count;
    char line[256];
    BYTE *screen_it;
    BYTE *screen_end;

    ++update_count;
    if (update_count <= 10 || update_count == 60 ||
        update_count == 600) {
        wsprintfA(line,
            "MinecraftClient update %u client=%p state_8=%u quit_9=%u "
            "state_10=%08lX screens=%p..%p suspended_230=%u "
            "render_all_2fe=%u state_1b4=%p",
            update_count, client,
            (unsigned)*((BYTE *)client + 8),
            (unsigned)*((BYTE *)client + 9),
            (unsigned long)*(DWORD *)((BYTE *)client + 0x10),
            *(void **)((BYTE *)client + 0xd0),
            *(void **)((BYTE *)client + 0xd4),
            (unsigned)*((BYTE *)client + 0x230),
            (unsigned)*((BYTE *)client + 0x2fe),
            *(void **)((BYTE *)client + 0x1b4));
        log_line(line);
    }
    if (update_count == 1) {
        unsigned index = 0;
        void *minecraft_game = *(void **)((BYTE *)client + 0x1b4);
        void **minecraft_game_vtable =
            minecraft_game && !IsBadReadPtr(minecraft_game, sizeof(void *))
            ? *(void ***)minecraft_game : NULL;
        if (minecraft_game_vtable &&
            !IsBadReadPtr(minecraft_game_vtable, 0x120)) {
            wsprintfA(line,
                "MinecraftGame object=%p vtable=%p "
                "slot+110=%p slot+114=%p slot+11c=%p",
                minecraft_game, minecraft_game_vtable,
                minecraft_game_vtable[0x110 / sizeof(void *)],
                minecraft_game_vtable[0x114 / sizeof(void *)],
                minecraft_game_vtable[0x11c / sizeof(void *)]);
            log_line(line);
        }
        {
            BYTE *level_renderer =
                *(BYTE **)((BYTE *)client + 0xa8);
            if (level_renderer &&
                !IsBadReadPtr(level_renderer, 0x20c)) {
                BYTE *pass_begin = *(BYTE **)(level_renderer + 0x14);
                BYTE *pass_end = *(BYTE **)(level_renderer + 0x18);
                wsprintfA(line,
                    "LevelRenderer object=%p client=%p passes=%p..%p "
                    "count=%u state_20b=%u size=%d/%d",
                    level_renderer,
                    *(void **)(level_renderer + 8),
                    pass_begin, pass_end,
                    pass_begin && pass_end >= pass_begin
                        ? (unsigned)((pass_end - pass_begin) / 4) : 0,
                    (unsigned)*(BYTE *)(level_renderer + 0x20b),
                    (int)*(short *)(level_renderer + 0x28),
                    (int)*(short *)(level_renderer + 0x2a));
                log_line(line);
            }
        }
        screen_it = *(BYTE **)((BYTE *)client + 0xd0);
        screen_end = *(BYTE **)((BYTE *)client + 0xd4);
        while (screen_it && screen_it + 8 <= screen_end && index < 16) {
            void *screen = *(void **)screen_it;
            void *vtable = screen && !IsBadReadPtr(screen, sizeof(void *))
                ? *(void **)screen : NULL;
            void *render_context =
                screen && !IsBadReadPtr((BYTE *)screen + 0x7c, 4)
                ? *(void **)((BYTE *)screen + 0x70) : NULL;
            void *render_vtable =
                render_context &&
                !IsBadReadPtr(render_context, sizeof(void *))
                ? *(void **)render_context : NULL;
            void *render_command =
                render_vtable && !IsBadReadPtr(
                    (BYTE *)render_vtable + 0x18, sizeof(void *))
                ? *(void **)((BYTE *)render_vtable + 0x14) : NULL;
            wsprintfA(line,
                "MinecraftClient screen %u object=%p vtable=%p "
                "render=%p render_vtable=%p command=%p tree=%p",
                index, screen, vtable, render_context, render_vtable,
                render_command,
                screen && !IsBadReadPtr((BYTE *)screen + 0x7c, 4)
                    ? *(void **)((BYTE *)screen + 0x78) : NULL);
            log_line(line);
            if (render_context &&
                !IsBadReadPtr((BYTE *)render_context + 0x114, 4)) {
                BYTE *child_it =
                    *(BYTE **)((BYTE *)render_context + 0x10c);
                BYTE *child_end =
                    *(BYTE **)((BYTE *)render_context + 0x110);
                unsigned child_index = 0;
                while (child_it && child_it + 8 <= child_end &&
                       child_index < 16) {
                    void *child = *(void **)child_it;
                    void *child_vtable =
                        child && !IsBadReadPtr(child, sizeof(void *))
                        ? *(void **)child : NULL;
                    void *child_command =
                        child_vtable && !IsBadReadPtr(
                            (BYTE *)child_vtable + 0x18, sizeof(void *))
                        ? *(void **)((BYTE *)child_vtable + 0x14) : NULL;
                    wsprintfA(line,
                        "MinecraftClient screen %u renderer %u "
                        "object=%p vtable=%p command=%p",
                        index, child_index, child, child_vtable,
                        child_command);
                    log_line(line);
                    child_it += 8;
                    ++child_index;
                }
            }
            screen_it += 8;
            ++index;
        }
    }
    if (update_count <= 3) {
        screen_it = *(BYTE **)((BYTE *)client + 0xd0);
        screen_end = *(BYTE **)((BYTE *)client + 0xd4);
        while (screen_it && screen_it + 8 <= screen_end) {
            void *screen = *(void **)screen_it;
            void *state =
                screen && !IsBadReadPtr((BYTE *)screen + 0x7c, 4)
                ? *(void **)((BYTE *)screen + 0x78) : NULL;
            if (state && !IsBadReadPtr((BYTE *)state + 8, 4)) {
                *(DWORD *)((BYTE *)state + 8) |= 0x0f;
            }
            screen_it += 8;
        }
        log_line("diagnostic: active ScreenViews invalidated");
    }
    original_client_update(client);
    if (update_count <= 3) {
        BYTE *level_renderer = *(BYTE **)((BYTE *)client + 0xa8);
        if (level_renderer && !IsBadReadPtr(level_renderer, 0x20c)) {
            BYTE *pass_begin = *(BYTE **)(level_renderer + 0x14);
            BYTE *pass_end = *(BYTE **)(level_renderer + 0x18);
            wsprintfA(line,
                "LevelRenderer after update %u passes=%p..%p count=%u "
                "size=%d/%d",
                update_count, pass_begin, pass_end,
                pass_begin && pass_end >= pass_begin
                    ? (unsigned)((pass_end - pass_begin) / 4) : 0,
                (int)*(short *)(level_renderer + 0x28),
                (int)*(short *)(level_renderer + 0x2a));
            log_line(line);
        }
    }
}

static BOOL install_client_update_trace(void *client)
{
    GameClientUpdateFn *slot;
    DWORD old_protection;
    DWORD ignored;

    if (!client || IsBadReadPtr(client, sizeof(void *)) ||
        IsBadReadPtr(*(void ***)client, 0x50)) {
        return FALSE;
    }
    slot = (GameClientUpdateFn *)&(*(void ***)client)[0x4c / sizeof(void *)];
    if (*slot == win32_client_update) return TRUE;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        log_line("MinecraftClient update vtable trace protection failed");
        return FALSE;
    }
    original_client_update = *slot;
    *slot = win32_client_update;
    VirtualProtect(slot, sizeof(*slot), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    log_pointer("MinecraftClient original update", original_client_update);
    log_line("MinecraftClient update vtable trace installed");
    return TRUE;
}

static unsigned screen_vector_count(void *screen, SIZE_T offset)
{
    BYTE *begin = *(BYTE **)((BYTE *)screen + offset);
    BYTE *end = *(BYTE **)((BYTE *)screen + offset + 4);
    return begin && end >= begin ? (unsigned)((end - begin) / 8) : 0;
}

static void __attribute__((thiscall)) win32_screen_view_render(
    void *screen, int render_token, int frame_token)
{
    static unsigned render_count;
    char line[224];
    void *state = *(void **)((BYTE *)screen + 0x78);
    DWORD dirty_before = state ? *(DWORD *)((BYTE *)state + 8) : 0;
    DWORD queue_before = *(DWORD *)((BYTE *)screen + 0x1c);

    ++render_count;
    if (render_count <= 10 || render_count == 60 ||
        render_count == 600) {
        wsprintfA(line,
            "ScreenView render %u screen=%p tokens=%08lx/%08lx "
            "dirty=%08lx queue=%lu",
            render_count, screen, (unsigned long)render_token,
            (unsigned long)frame_token, (unsigned long)dirty_before,
            (unsigned long)queue_before);
        log_line(line);
    }
    original_screen_view_render(screen, render_token, frame_token);
    if (render_count <= 10 || render_count == 60 ||
        render_count == 600) {
        DWORD dirty_after = state ? *(DWORD *)((BYTE *)state + 8) : 0;
        wsprintfA(line,
            "ScreenView render %u returned dirty=%08lx queue=%lu",
            render_count, (unsigned long)dirty_after,
            (unsigned long)*(DWORD *)((BYTE *)screen + 0x1c));
        log_line(line);
        if (render_count <= 3) {
            wsprintfA(line,
                "ScreenView vectors: 7c=%u 88=%u 94=%u a0=%u "
                "ac=%u b8=%u c4=%u d0=%u",
                screen_vector_count(screen, 0x7c),
                screen_vector_count(screen, 0x88),
                screen_vector_count(screen, 0x94),
                screen_vector_count(screen, 0xa0),
                screen_vector_count(screen, 0xac),
                screen_vector_count(screen, 0xb8),
                screen_vector_count(screen, 0xc4),
                screen_vector_count(screen, 0xd0));
            log_line(line);
        }
    }
}

static void __attribute__((thiscall)) win32_cubemap_screen_render(
    void *screen, int render_token, int frame_token)
{
    static unsigned render_count;
    char line[160];

    ++render_count;
    if (render_count <= 10 || render_count == 60 ||
        render_count == 600) {
        wsprintfA(line,
            "CubemapBackgroundScreen render %u screen=%p tokens=%08lx/%08lx",
            render_count, screen, (unsigned long)render_token,
            (unsigned long)frame_token);
        log_line(line);
    }
    original_cubemap_screen_render(screen, render_token, frame_token);
}

static void __attribute__((thiscall)) win32_screen_view_draw(
    void *screen, void *render_context_holder)
{
    static unsigned draw_count;
    void *render_context = render_context_holder &&
        !IsBadReadPtr(render_context_holder, sizeof(void *))
        ? *(void **)render_context_holder : NULL;
    void *vtable = render_context &&
        !IsBadReadPtr(render_context, sizeof(void *))
        ? *(void **)render_context : NULL;

    ++draw_count;
    if (draw_count <= 12 || draw_count == 60 || draw_count == 600) {
        char line[192];
        wsprintfA(line,
            "ScreenView draw %u screen=%p holder=%p context=%p vtable=%p",
            draw_count, screen, render_context_holder, render_context,
            vtable);
        log_line(line);
    }
    original_screen_view_draw(screen, render_context_holder);
}

static BOOL install_screen_render_trace(BYTE *image)
{
    GameScreenRenderFn *screen_view_slot =
        (GameScreenRenderFn *)(image + RVA_SCREEN_VIEW_VTABLE + 0x2c);
    GameScreenRenderFn *cubemap_slot =
        (GameScreenRenderFn *)(image + RVA_CUBEMAP_SCREEN_VTABLE + 0x2c);
    GameScreenDrawFn *screen_view_draw_slot =
        (GameScreenDrawFn *)(image + RVA_SCREEN_VIEW_VTABLE + 0x38);
    DWORD old_protection;
    DWORD ignored;

    if (!VirtualProtect(screen_view_slot, sizeof(*screen_view_slot),
                        PAGE_EXECUTE_READWRITE, &old_protection)) {
        log_line("ScreenView render trace protection failed");
        return FALSE;
    }
    original_screen_view_render = *screen_view_slot;
    *screen_view_slot = win32_screen_view_render;
    VirtualProtect(screen_view_slot, sizeof(*screen_view_slot),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), screen_view_slot,
                          sizeof(*screen_view_slot));

    if (!VirtualProtect(screen_view_draw_slot,
                        sizeof(*screen_view_draw_slot),
                        PAGE_EXECUTE_READWRITE, &old_protection)) {
        log_line("ScreenView draw trace protection failed");
        return FALSE;
    }
    original_screen_view_draw = *screen_view_draw_slot;
    *screen_view_draw_slot = win32_screen_view_draw;
    VirtualProtect(screen_view_draw_slot, sizeof(*screen_view_draw_slot),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), screen_view_draw_slot,
                          sizeof(*screen_view_draw_slot));

    if (!VirtualProtect(cubemap_slot, sizeof(*cubemap_slot),
                        PAGE_EXECUTE_READWRITE, &old_protection)) {
        log_line("Cubemap screen render trace protection failed");
        return FALSE;
    }
    original_cubemap_screen_render = *cubemap_slot;
    *cubemap_slot = win32_cubemap_screen_render;
    VirtualProtect(cubemap_slot, sizeof(*cubemap_slot),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), cubemap_slot,
                          sizeof(*cubemap_slot));
    log_line("screen update/draw vtable traces installed");
    return TRUE;
}

static void log_game_callback(const char *name, unsigned count, void *self)
{
    char line[160];
    if (count <= 12 || count == 60 || count == 600) {
        wsprintfA(line, "GameCallbacks %s %u self=%p source=%s",
                  name, count, self,
                  host_invoking_game_render ? "Win32 host" : "game");
        log_line(line);
    }
}

static void __attribute__((thiscall)) win32_callback_prepare(void *self)
{
    static unsigned count;
    log_game_callback("slot+04", ++count, self);
    original_callback_prepare(self);
}

static void __attribute__((thiscall)) win32_callback_begin(void *self)
{
    static unsigned count;
    log_game_callback("slot+08", ++count, self);
    original_callback_begin(self);
}

static void __attribute__((thiscall)) win32_callback_render(
    void *self, int render_token, int frame_token)
{
    static unsigned count;
    char line[192];
    ++count;
    if (count <= 12 || count == 60 || count == 600) {
        wsprintfA(line,
            "GameCallbacks slot+0C %u self=%p tokens=%08lx/%08lx source=%s",
            count, self, (unsigned long)render_token,
            (unsigned long)frame_token,
            host_invoking_game_render ? "Win32 host" : "game");
        log_line(line);
    }
    original_callback_render(self, render_token, frame_token);
}

static void __attribute__((thiscall)) win32_callback_end(void *self)
{
    static unsigned count;
    log_game_callback("slot+10", ++count, self);
    original_callback_end(self);
}

static void __attribute__((thiscall)) win32_callback_idle(void *self)
{
    static unsigned count;
    log_game_callback("slot+14", ++count, self);
    original_callback_idle(self);
}

static void __attribute__((thiscall)) win32_callback_cleanup(void *self)
{
    static unsigned count;
    log_game_callback("slot+18", ++count, self);
    original_callback_cleanup(self);
}

static BOOL install_game_callback_trace(BYTE *image)
{
    void **slots = (void **)(image + RVA_MINECRAFT_CALLBACK_VTABLE);
    DWORD old_protection;
    DWORD ignored;

    if (!VirtualProtect(slots + 1, 6 * sizeof(*slots),
                        PAGE_EXECUTE_READWRITE, &old_protection)) {
        log_line("GameCallbacks vtable trace protection failed");
        return FALSE;
    }
    original_callback_prepare = (GameCallbackFn)slots[1];
    original_callback_begin = (GameCallbackFn)slots[2];
    original_callback_render = (GameClientRenderFn)slots[3];
    original_callback_end = (GameCallbackFn)slots[4];
    original_callback_idle = (GameCallbackFn)slots[5];
    original_callback_cleanup = (GameCallbackFn)slots[6];
    slots[1] = win32_callback_prepare;
    slots[2] = win32_callback_begin;
    slots[3] = win32_callback_render;
    slots[4] = win32_callback_end;
    slots[5] = win32_callback_idle;
    slots[6] = win32_callback_cleanup;
    VirtualProtect(slots + 1, 6 * sizeof(*slots),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slots + 1,
                          6 * sizeof(*slots));
    log_line("MinecraftClient GameCallbacks vtable traces installed");
    return TRUE;
}

static void __attribute__((thiscall)) win32_apply_render_state(
    void *state, void *draw_context, void *render_context,
    void *reserved_1, void *reserved_2)
{
    static unsigned null_state_count;
    static unsigned valid_state_count;

    if (!state) {
        ++null_state_count;
        if (null_state_count <= 10 || null_state_count == 60 ||
            null_state_count == 600) {
            char line[96];
            wsprintfA(line, "skipped null render state %u",
                      null_state_count);
            log_line(line);
        }
        return;
    }
    ++valid_state_count;
    if (valid_state_count <= 3 || valid_state_count == 60 ||
        valid_state_count == 600) {
        char line[96];
        wsprintfA(line, "applying valid render state %u state=%p",
                  valid_state_count, state);
        log_line(line);
    }
    original_apply_render_state(
        state, draw_context, render_context, reserved_1, reserved_2);
}

static const char *material_string(const BYTE *material, size_t offset)
{
    return *(const DWORD *)(material + offset + 0x14) < 0x10
        ? (const char *)(material + offset)
        : *(const char * const *)(material + offset);
}

static void __attribute__((thiscall)) win32_material_draw(
    void *material, void *draw_context, void *render_context,
    void *reserved)
{
    static unsigned null_material_count;
    const BYTE *value = (const BYTE *)material;

    if (value && !IsBadReadPtr(value, 0xc4) &&
        !*(void * const *)(value + 0x58)) {
        ++null_material_count;
        if (null_material_count <= 10 || null_material_count == 60 ||
            null_material_count == 600) {
            char line[384];
            wsprintfA(line,
                      "null material %u object=%p flags=%08lX "
                      "vs='%s' ps='%s' gs='%s'",
                      null_material_count, material,
                      (unsigned long)*(const DWORD *)(value + 8),
                      material_string(value, 0x0c),
                      material_string(value, 0x24),
                      material_string(value, 0x3c));
            log_line(line);
        }
    }
    original_material_draw(
        material, draw_context, render_context, reserved);
}

static BOOL install_material_trace(BYTE *image)
{
    BYTE *site = image + 0x00535256;
    static const BYTE expected[5] = {0xe8, 0x95, 0xe2, 0xff, 0xff};
    DWORD old_protection;
    DWORD ignored;
    INT_PTR displacement = (const BYTE *)win32_material_draw - (site + 5);

    original_material_draw = (GameMaterialDrawFn)(image + 0x005334f0);
    if (memcmp(site, expected, sizeof(expected)) != 0 ||
        displacement < (INT_PTR)INT_MIN ||
        displacement > (INT_PTR)INT_MAX ||
        !VirtualProtect(site, sizeof(expected), PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        log_line("material trace patch failed");
        return FALSE;
    }
    site[0] = 0xe8;
    *(LONG *)(site + 1) = (LONG)displacement;
    VirtualProtect(site, sizeof(expected), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(expected));
    log_line("material trace installed");
    return TRUE;
}

static BOOL install_render_state_guard(BYTE *image)
{
    BYTE *site = image + 0x005335f4;
    static const BYTE expected[5] = {0xe8, 0x97, 0xa2, 0x00, 0x00};
    DWORD old_protection;
    DWORD ignored;
    INT_PTR displacement =
        (const BYTE *)win32_apply_render_state - (site + 5);

    original_apply_render_state =
        (GameApplyRenderStateFn)(image + 0x0053d890);
    if (memcmp(site, expected, sizeof(expected)) != 0 ||
        displacement < (INT_PTR)INT_MIN ||
        displacement > (INT_PTR)INT_MAX ||
        !VirtualProtect(site, sizeof(expected), PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        log_line("render-state guard patch failed");
        return FALSE;
    }
    site[0] = 0xe8;
    *(LONG *)(site + 1) = (LONG)displacement;
    VirtualProtect(site, sizeof(expected), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(expected));
    log_line("render-state guard installed");
    return TRUE;
}

/*
 * MaterialGroup::compileAsync submits its iterator callback through the
 * executable's private ConcRT scheduler.  Wine accepts the task but never
 * dispatches it for this UWP build, leaving every material without a D3D
 * shader.  Execute this one idempotent iterator synchronously, then submit
 * the now-complete iterator normally so its task<bool> shared state retains
 * the exact ownership/lifetime expected by Minecraft.
 */
static void *__attribute__((thiscall)) win32_schedule_material_task(
    void *scheduler, void *future, void *callable, void *token, int flags)
{
    static LONG invocation_count;
    void **vtable = callable && !IsBadReadPtr(callable, sizeof(void *))
        ? *(void ***)callable : NULL;

    if (vtable == (void **)(game_image + RVA_MATERIAL_TASK_VTABLE) &&
        !IsBadReadPtr(vtable, 3 * sizeof(void *)) && vtable[2]) {
        GameTaskInvokeFn invoke = (GameTaskInvokeFn)vtable[2];
        BYTE complete;
        LONG count;
        do {
            complete = invoke(callable);
        } while (!complete);
        count = InterlockedIncrement(&invocation_count);
        if (count <= 4) {
            log_line("material shader task executed synchronously");
        }
    } else {
        log_line("unexpected callback at material scheduler call");
    }
    return original_schedule_task(
        scheduler, future, callable, token, flags);
}

static BOOL install_material_task_compat(BYTE *image)
{
    BYTE *site = image + RVA_MATERIAL_TASK_CALL;
    static const BYTE expected[5] = {0xe8, 0x60, 0x3c, 0x0c, 0x00};
    DWORD old_protection;
    DWORD ignored;
    INT_PTR displacement =
        (const BYTE *)win32_schedule_material_task - (site + 5);

    original_schedule_task =
        (GameScheduleTaskFn)(image + RVA_SCHEDULE_TASK);
    if (memcmp(site, expected, sizeof(expected)) != 0 ||
        displacement < (INT_PTR)INT_MIN ||
        displacement > (INT_PTR)INT_MAX ||
        !VirtualProtect(site, sizeof(expected), PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        log_line("material scheduler compatibility patch failed");
        return FALSE;
    }
    site[0] = 0xe8;
    *(LONG *)(site + 1) = (LONG)displacement;
    VirtualProtect(site, sizeof(expected), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(expected));
    log_line("material scheduler compatibility patch installed");
    return TRUE;
}

static BOOL compile_material_group_now(BYTE *image, SIZE_T group_rva,
                                       const char *name)
{
    BYTE *group = image + group_rva;
    GameTaskInvokeFn invoke =
        (GameTaskInvokeFn)(image + RVA_MATERIAL_TASK_INVOKE);
    struct {
        void **vtable;
        BYTE *group;
        void *current;
    } task;
    UINT steps = 0;
    BYTE complete = FALSE;
    char line[128];

    if (IsBadReadPtr(group + 0x0c, sizeof(void *)) ||
        IsBadReadPtr(*(void **)(group + 0x0c), sizeof(void *))) {
        wsprintfA(line, "%s material group is unavailable", name);
        log_line(line);
        return FALSE;
    }
    task.vtable = (void **)(image + RVA_MATERIAL_TASK_VTABLE);
    task.group = group;
    task.current = **(void ***)(group + 0x0c);
    do {
        complete = invoke(&task);
        ++steps;
    } while (!complete && steps < 8192);
    wsprintfA(line, "%s material group compiled in %u steps%s",
              name, steps, complete ? "" : " (iteration limit reached)");
    log_line(line);
    return complete;
}

static BOOL write_relative_jump(BYTE *site, const void *target);

typedef struct DevelopmentVersionStringX86 {
    union {
        char small[16];
        char *heap;
    } storage;
    DWORD length;
    DWORD capacity;
} DevelopmentVersionStringX86;

static BOOL write_relative_jump(BYTE *site, const void *target);

/* 0.15.10's release getter returns an empty #development_version string.
 * Return the requested final text directly as a fallback for builds whose
 * start_screen.json is opened through a path not covered by the stdio hooks. */
static void *__attribute__((thiscall, noinline))
win32_development_version_text_01510(
    void *self, DevelopmentVersionStringX86 *output)
{
    static const char text[] = "github.com/deltaxur/Win32Craft";
    GameMallocFn allocate;
    char *buffer;
    (void)self;
    if (!output) return NULL;
    ZeroMemory(output, sizeof(*output));
    allocate = *(GameMallocFn *)(game_image + 0x00552768);
    buffer = allocate ? (char *)allocate(sizeof(text)) : NULL;
    if (!buffer) return output;
    CopyMemory(buffer, text, sizeof(text));
    output->storage.heap = buffer;
    output->length = (DWORD)(sizeof(text) - 1);
    output->capacity = (DWORD)(sizeof(text) - 1);
    return output;
}

static BOOL install_development_version_text_01510(BYTE *image)
{
    static const BYTE expected[10] = {
        0x55, 0x8b, 0xec, 0x51, 0xff,
        0x75, 0x08, 0xc7, 0x45, 0xfc
    };
    BYTE *site;

    if (!image) return FALSE;
    site = image + RVA_01510_DEVELOPMENT_VERSION_GETTER;
    if (memcmp(site, expected, sizeof(expected)) != 0) {
        log_line("Minecraft 0.15.10 development-version getter signature mismatch");
        return FALSE;
    }
    if (!write_relative_jump(site, win32_development_version_text_01510)) {
        log_line("Minecraft 0.15.10 development-version text hook failed");
        return FALSE;
    }
    log_line("Minecraft 0.15.10 development-version text overridden");
    return TRUE;
}

static BOOL write_relative_jump(BYTE *site, const void *target)
{
    DWORD old_protection;
    DWORD ignored;
    INT_PTR displacement = (const BYTE *)target - (site + 5);

    if (displacement < (INT_PTR)INT_MIN ||
        displacement > (INT_PTR)INT_MAX) {
        return FALSE;
    }
    if (!VirtualProtect(site, 10, PAGE_EXECUTE_READWRITE, &old_protection)) {
        return FALSE;
    }
    site[0] = 0xe9;
    *(LONG *)(site + 1) = (LONG)displacement;
    FillMemory(site + 5, 5, 0x90);
    VirtualProtect(site, 10, old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, 10);
    return TRUE;
}

static BOOL verify_patched_audio_exe(BYTE *image)
{
    static const char desktop_name[] = "fmod.dll";
    const char *delay_name = (const char *)(image + 0x00989a20);

    if (memcmp(delay_name, desktop_name, sizeof(desktop_name)) != 0) {
        log_line("audio EXE mismatch: FMOD delay-import name is not fmod.dll");
        return FALSE;
    }
    log_line("audio EXE verified: sound-event table enabled and FMOD name=fmod.dll");
    return TRUE;
}

static BOOL resolve_fmod_delay_imports_115(BYTE *image)
{
    typedef struct DelayDescriptor {
        DWORD attributes;
        DWORD name_rva;
        DWORD module_handle_rva;
        DWORD iat_rva;
        DWORD int_rva;
        DWORD bound_iat_rva;
        DWORD unload_iat_rva;
        DWORD timestamp;
    } DelayDescriptor;
    DelayDescriptor *descriptor =
        (DelayDescriptor *)(image + 0x01241040);
    DWORD *name_table;
    void **address_table;
    HMODULE module;
    DWORD old_protection;
    DWORD ignored;
    unsigned count = 0;
    unsigned index;

    if (descriptor->attributes != 1 ||
        descriptor->iat_rva != 0x013bf18c ||
        descriptor->int_rva != 0x01241080 ||
        memcmp(image + descriptor->name_rva, "fmod.dll", 9) != 0) {
        log_line("Minecraft 1.1.5 FMOD delay descriptor mismatch");
        return FALSE;
    }
    name_table = (DWORD *)(image + descriptor->int_rva);
    address_table = (void **)(image + descriptor->iat_rva);
    while (name_table[count]) ++count;
    if (count != 37) {
        log_line("Minecraft 1.1.5 FMOD delay import count mismatch");
        return FALSE;
    }
    module = LoadLibraryW(L"fmod.dll");
    if (!module ||
        !VirtualProtect(
            address_table, count * sizeof(*address_table),
            PAGE_READWRITE, &old_protection)) {
        log_line("Minecraft 1.1.5 desktop FMOD load failed");
        return FALSE;
    }
    for (index = 0; index < count; ++index) {
        DWORD thunk = name_table[index];
        void *target;
        if (thunk & 0x80000000u) {
            target = GetProcAddress(
                module, (LPCSTR)(ULONG_PTR)(thunk & 0xffffu));
        } else {
            target = GetProcAddress(
                module, (LPCSTR)(image + thunk + 2));
        }
        if (!target) {
            VirtualProtect(
                address_table, count * sizeof(*address_table),
                old_protection, &ignored);
            log_line("Minecraft 1.1.5 desktop FMOD export missing");
            return FALSE;
        }
        address_table[index] = (void *)target;
    }
    VirtualProtect(
        address_table, count * sizeof(*address_table),
        old_protection, &ignored);
    FlushInstructionCache(
        GetCurrentProcess(), address_table,
        count * sizeof(*address_table));
    log_line("Minecraft 1.1.5 desktop FMOD delay imports resolved");
    return TRUE;
}

static BOOL resolve_fmod_delay_imports_128(BYTE *image)
{
    typedef struct DelayDescriptor {
        DWORD attributes;
        DWORD name_rva;
        DWORD module_handle_rva;
        DWORD iat_rva;
        DWORD int_rva;
        DWORD bound_iat_rva;
        DWORD unload_iat_rva;
        DWORD timestamp;
    } DelayDescriptor;
    DWORD pe_offset = *(DWORD *)(image + 0x3c);
    BYTE *optional = image + pe_offset + 24;
    DWORD delay_rva = *(DWORD *)(optional + 0xc8);
    DWORD delay_size = *(DWORD *)(optional + 0xcc);
    DelayDescriptor *descriptor;
    DWORD *name_table;
    void **address_table;
    HMODULE module;
    DWORD old_protection;
    DWORD ignored;
    unsigned count = 0;
    unsigned index;

    if (!delay_rva || delay_size < sizeof(*descriptor)) {
        log_line("Minecraft 1.2.8 delay-import directory is unavailable");
        return FALSE;
    }
    descriptor = (DelayDescriptor *)(image + delay_rva);
    while ((BYTE *)(descriptor + 1) <= image + delay_rva + delay_size &&
           descriptor->name_rva) {
        const char *name = (const char *)(image + descriptor->name_rva);
        if (memcmp(name, "fmod.dll", 9) == 0) break;
        ++descriptor;
    }
    if ((BYTE *)(descriptor + 1) > image + delay_rva + delay_size ||
        !descriptor->name_rva || descriptor->attributes != 1 ||
        !descriptor->iat_rva || !descriptor->int_rva) {
        log_line("Minecraft 1.2.8 FMOD delay descriptor mismatch");
        return FALSE;
    }

    name_table = (DWORD *)(image + descriptor->int_rva);
    address_table = (void **)(image + descriptor->iat_rva);
    while (name_table[count] && count < 128) ++count;
    if (!count || count == 128) {
        log_line("Minecraft 1.2.8 FMOD delay import count is invalid");
        return FALSE;
    }
    module = LoadLibraryW(L"fmod.dll");
    if (!module || !VirtualProtect(
            address_table, count * sizeof(*address_table), PAGE_READWRITE,
            &old_protection)) {
        log_line("Minecraft 1.2.8 desktop FMOD load failed");
        return FALSE;
    }
    for (index = 0; index < count; ++index) {
        DWORD thunk = name_table[index];
        void *target = thunk & 0x80000000u
            ? GetProcAddress(module, (LPCSTR)(ULONG_PTR)(thunk & 0xffffu))
            : GetProcAddress(module, (LPCSTR)(image + thunk + 2));
        if (!target) {
            VirtualProtect(address_table, count * sizeof(*address_table),
                           old_protection, &ignored);
            log_line("Minecraft 1.2.8 desktop FMOD export missing");
            return FALSE;
        }
        address_table[index] = target;
    }
    VirtualProtect(address_table, count * sizeof(*address_table),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), address_table,
                          count * sizeof(*address_table));
    log_line("Minecraft 1.2.8 desktop FMOD delay imports resolved");
    return TRUE;
}

/*
 * 1.16 has its own 48-entry FMOD delay-import table.  Its bootstrap returns
 * before the legacy host path (and its older 31-entry bridge) is reached, so
 * relying on the normal delay helper leaves the Store DLL unresolved in the
 * unpackaged process.  Resolve the verified table eagerly against the x86
 * desktop FMOD DLL.  The imported C++ names are also an ABI check: an older or
 * otherwise incompatible FMOD build is rejected before the game can call it.
 */
static BOOL resolve_fmod_delay_imports_116(BYTE *image)
{
    typedef struct DelayDescriptor {
        DWORD attributes;
        DWORD name_rva;
        DWORD module_handle_rva;
        DWORD iat_rva;
        DWORD int_rva;
        DWORD bound_iat_rva;
        DWORD unload_iat_rva;
        DWORD timestamp;
    } DelayDescriptor;
    DelayDescriptor *descriptor = (DelayDescriptor *)(image + 0x024b25a4);
    DWORD *name_table;
    void **address_table;
    HMODULE module;
    DWORD old_protection;
    DWORD ignored;
    unsigned count = 0;
    unsigned index;

    if (!image || descriptor->attributes != 1 ||
        descriptor->name_rva != 0x01eaa230 ||
        descriptor->module_handle_rva != 0x02ada400 ||
        descriptor->iat_rva != 0x02ada320 ||
        descriptor->int_rva != 0x024b25e4 ||
        memcmp(image + descriptor->name_rva, "fmod.dll", 9) != 0) {
        log_line("Minecraft 1.16 FMOD delay descriptor mismatch");
        return FALSE;
    }

    name_table = (DWORD *)(image + descriptor->int_rva);
    address_table = (void **)(image + descriptor->iat_rva);
    while (name_table[count] && count < 64) ++count;
    if (count != 48) {
        log_line("Minecraft 1.16 FMOD delay import count mismatch");
        return FALSE;
    }

    module = LoadLibraryW(L"fmod.dll");
    if (!module) {
        char line[160];
        wsprintfA(line, "Minecraft 1.16 desktop FMOD load failed, error=%u",
                  (unsigned)GetLastError());
        log_line(line);
        return FALSE;
    }
    if (!VirtualProtect(address_table, count * sizeof(*address_table),
                        PAGE_READWRITE, &old_protection)) {
        log_line("Minecraft 1.16 FMOD delay-IAT protection change failed");
        return FALSE;
    }

    for (index = 0; index < count; ++index) {
        DWORD thunk = name_table[index];
        const char *name;
        void *target;
        if (thunk & 0x80000000u) {
            name = (LPCSTR)(ULONG_PTR)(thunk & 0xffffu);
        } else {
            name = (const char *)(image + thunk + 2);
        }
        target = GetProcAddress(module, name);
        if (!target) {
            char line[256];
            if (thunk & 0x80000000u)
                wsprintfA(line, "Minecraft 1.16 FMOD ordinal %u is missing",
                          (unsigned)(thunk & 0xffffu));
            else
                wsprintfA(line, "Minecraft 1.16 FMOD export is missing: %.180s",
                          name);
            log_line(line);
            VirtualProtect(address_table, count * sizeof(*address_table),
                           old_protection, &ignored);
            return FALSE;
        }
        /* Keep uncommon 1.16 calls native, but route the common audio path
         * through the diagnostic bridge.  Besides selecting a usable desktop
         * output, createSound/createStream translate ms-appx:/// paths. */
        switch (index) {
        case 2: target = bridge_fmod_get_version; break;
        case 3: target = bridge_fmod_get_num_drivers; break;
        case 4: target = bridge_fmod_get_driver_info; break;
        case 5: target = bridge_fmod_set_driver; break;
        case 6: target = bridge_fmod_set_output; break;
        case 7: target = bridge_fmod_set_3d_settings; break;
        case 8: target = bridge_fmod_create_group; break;
        case 9: target = bridge_fmod_get_group; break;
        case 10: target = bridge_fmod_add_group; break;
        case 11: target = bridge_fmod_mixer_resume; break;
        case 12: target = bridge_fmod_mixer_suspend; break;
        case 14: target = bridge_fmod_set_mute; break;
        case 15: target = bridge_fmod_create_stream; break;
        case 16: target = bridge_fmod_stop; break;
        case 17: target = bridge_fmod_create_sound; break;
        case 18: target = bridge_fmod_set_attributes; break;
        case 25: target = bridge_fmod_init; break;
        case 26: target = bridge_fmod_update; break;
        case 27: target = bridge_fmod_system_release; break;
        case 32: target = bridge_fmod_is_playing; break;
        case 33: target = bridge_fmod_set_listener; break;
        case 34: target = bridge_fmod_set_paused; break;
        case 35: target = bridge_fmod_set_pitch; break;
        case 36: target = bridge_fmod_set_volume; break;
        case 37: target = bridge_fmod_play_sound; break;
        case 40: target = bridge_fmod_set_min_max; break;
        case 41: target = bridge_fmod_get_sub_sound; break;
        case 43: target = bridge_fmod_get_sub_count; break;
        case 44: target = bridge_fmod_system_create; break;
        case 46: target = bridge_fmod_sound_release; break;
        default: break;
        }
        address_table[index] = target;
    }
    *(HMODULE *)(image + descriptor->module_handle_rva) = module;
    VirtualProtect(address_table, count * sizeof(*address_table),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), address_table,
                          count * sizeof(*address_table));
    log_line("Minecraft 1.16 desktop FMOD: 48 delay imports resolved");
    return TRUE;
}

static BOOL install_native_fmod_bridge(BYTE *image)
{
    static const void *targets[] = {
        bridge_fmod_system_create,
        bridge_fmod_get_num_drivers, bridge_fmod_get_driver_info,
        bridge_fmod_set_driver, bridge_fmod_set_output,
        bridge_fmod_get_version, bridge_fmod_init,
        bridge_fmod_set_3d_settings,
        bridge_fmod_create_group, bridge_fmod_get_group,
        bridge_fmod_add_group, bridge_fmod_sound_release,
        bridge_fmod_system_close, bridge_fmod_system_release,
        bridge_fmod_mixer_resume, bridge_fmod_mixer_suspend,
        bridge_fmod_set_mute, bridge_fmod_set_volume,
        bridge_fmod_create_stream, bridge_fmod_create_sound,
        bridge_fmod_set_min_max, bridge_fmod_get_sub_count,
        bridge_fmod_get_sub_sound, bridge_fmod_play_sound,
        bridge_fmod_set_pitch, bridge_fmod_set_paused,
        bridge_fmod_set_attributes, bridge_fmod_is_playing,
        bridge_fmod_stop, bridge_fmod_update,
        bridge_fmod_set_listener
    };
    void **delay_iat = (void **)(image + 0x00bfd1ec);
    DWORD old_protection;
    DWORD ignored;
    unsigned index;

    if (!VirtualProtect(delay_iat, sizeof(targets), PAGE_READWRITE,
                        &old_protection)) {
        log_line("native FMOD delay-IAT protection change failed");
        return FALSE;
    }
    for (index = 0; index < sizeof(targets) / sizeof(targets[0]); ++index)
        delay_iat[index] = (void *)targets[index];
    VirtualProtect(delay_iat, sizeof(targets), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), delay_iat, sizeof(targets));
    log_line("31 FMOD delay-IAT slots replaced with native Win32 bridge");
    return TRUE;
}

#ifdef WIN32CRAFT_ARM32
static LONG CALLBACK win32_vectored_exception(EXCEPTION_POINTERS *details)
{
    DWORD code;
    void *frames[64];
    USHORT frame_count;
    unsigned index;
    ULONG_PTR image_base;
    ULONG_PTR image_size;
    DWORD rva;
    char line[160];

    if (!details || !details->ExceptionRecord || !details->ContextRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    code = details->ExceptionRecord->ExceptionCode;
    /* Same filter as x86: only C++ throws and fail-fast; first-chance access violations are produced on purpose by
     * IsBadReadPtr, logging them here would recurse. */
    if (code == 0x80000001u && fmod_real_module) {
        BYTE *module = (BYTE *)fmod_real_module;
        BYTE *address = (BYTE *)details->ExceptionRecord->ExceptionAddress;
        DWORD pe_offset = *(DWORD *)(module + 0x3c);
        DWORD module_size = *(DWORD *)(module + pe_offset + 24 + 0x38);
        if (address >= module && address < module + module_size) {
            log_line("FMOD guard-page notification resumed");
            return -1; /* EXCEPTION_CONTINUE_EXECUTION */
        }
    }
    if (code != 0xe06d7363 && code != 0xc0000409) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (code == 0xe06d7363) {
        /* C++ throws are routine in a C++/CX game (caught and handled).  Full register/stack dumps for the first 32 only;
         * arm32_iat_log_cxx_throw (the _CxxThrowException hook) keeps its own sampled count for the rest. */
        static volatile LONG cxx_exception_count;
        if (InterlockedIncrement(&cxx_exception_count) > 32) return EXCEPTION_CONTINUE_SEARCH;
    }
    wsprintfA(line, "VEH exception 0x%08lx at %p", (unsigned long)code,
              details->ExceptionRecord->ExceptionAddress);
    log_line(line);
    if (code == 0xe06d7363 && details->ExceptionRecord->NumberParameters >= 3) {
        wsprintfA(line, "  C++ throw: object=%08lx throwinfo=%08lx",
                  (unsigned long)details->ExceptionRecord->ExceptionInformation[1],
                  (unsigned long)details->ExceptionRecord->ExceptionInformation[2]);
        log_line(line);
    }
    arm32_log_context("VEH", details->ContextRecord);

    arm32_game_image_range(&image_base, &image_size);
    frame_count = RtlCaptureStackBackTrace(0, ARRAYSIZE(frames), frames, NULL);
    for (index = 0; index < frame_count; ++index) {
        if (arm32_in_image((ULONG_PTR)frames[index], image_base, image_size, &rva)) {
            wsprintfA(line, "  VEH frame[%u] = %p (game rva %08lx)",
                      index, frames[index], (unsigned long)rva);
        } else {
            wsprintfA(line, "  VEH frame[%u] = %p", index, frames[index]);
        }
        log_line(line);
    }
    arm32_log_stack_scan("VEH", details->ContextRecord->Sp, 1024, 64);
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
static LONG CALLBACK win32_vectored_exception(EXCEPTION_POINTERS *details)
{
    DWORD code;
    DWORD *stack;
    void *frames[64];
    USHORT frame_count;
    unsigned index;
    char line[128];

    if (!details || !details->ExceptionRecord || !details->ContextRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    code = details->ExceptionRecord->ExceptionCode;
    /* FMOD 1.09.04 uses a guarded page in its asynchronous stream worker.
     * Native Windows clears PAGE_GUARD and resumes the faulting instruction;
     * Wine 11 currently lets this first-chance notification escape as an
     * unhandled 0x80000001 exception.  Resume only when the instruction is
     * inside the verified desktop FMOD image. */
    if (code == 0x80000001u && fmod_real_module) {
        BYTE *module = (BYTE *)fmod_real_module;
        BYTE *address = (BYTE *)details->ExceptionRecord->ExceptionAddress;
        DWORD pe_offset = *(DWORD *)(module + 0x3c);
        DWORD image_size = *(DWORD *)(module + pe_offset + 24 + 0x38);
        if (address >= module && address < module + image_size) {
            log_line("FMOD guard-page notification resumed for Wine");
            return -1; /* EXCEPTION_CONTINUE_EXECUTION */
        }
    }
    /*
     * Do not trace first-chance access violations here. IsBadReadPtr and
     * Wine's guarded probes intentionally raise them; logging one while
     * inspecting the stack recursively re-enters this handler.
     */
    if (code != 0xe06d7363 && code != 0xc0000409) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    wsprintfA(line, "VEH exception 0x%08lx at %p EAX=%p ECX=%p "
              "EDX=%p EDI=%p EBP=%p ESP=%p",
              (unsigned long)code,
              details->ExceptionRecord->ExceptionAddress,
              (void *)details->ContextRecord->Eax,
              (void *)details->ContextRecord->Ecx,
              (void *)details->ContextRecord->Edx,
              (void *)details->ContextRecord->Edi,
              (void *)details->ContextRecord->Ebp,
              (void *)details->ContextRecord->Esp);
    log_line(line);

    frame_count = RtlCaptureStackBackTrace(0, ARRAYSIZE(frames), frames, NULL);
    for (index = 0; index < frame_count; ++index) {
        BYTE *candidate = (BYTE *)frames[index];
        wsprintfA(line, "  VEH frame[%u] = %p%s",
                  index, candidate,
                  candidate >= game_image &&
                  candidate < game_image + 0x00c00000
                      ? " (game)" : "");
        log_line(line);
    }

    stack = (DWORD *)details->ContextRecord->Esp;
    for (index = 0; index < 2048; ++index) {
        BYTE *candidate;
        if (IsBadReadPtr(stack + index, sizeof(*stack))) break;
        candidate = (BYTE *)(ULONG_PTR)stack[index];
        if (candidate >= game_image &&
            candidate < game_image + 0x00c00000) {
            wsprintfA(line, "  game stack[%u] = %p",
                      index, candidate);
            log_line(line);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

static void create_user_data(void)
{
    wchar_t roaming[MAX_PATH];
    wchar_t game_dir[MAX_PATH];
    wchar_t options_path[MAX_PATH];
    HANDLE file;
#ifdef WIN32CRAFT_ARM32
    /* ARM32 1.2.8 options.txt (unit bootstrap_main).  Every key below was checked against the option table of the ARM exe
     * (strings 0xDAC000-0xDAE200).  Dropped because the ARM exe has no such key: game_flatworldlayers, game_limitworldsize,
     * ctrl_sensitivity (the key is ctrl_sensitivity2 now: the game default is kept), ctrl_usetouchscreen, ctrl_keyboardlayout,
     * ctrl_gamePadMap, gfx_renderdistance_new (gfx_viewdistance now: game default kept), gfx_animatetextures, gfx_hidegui,
     * dev_disablefilesystem.  old_game_version_* is omitted on purpose: absent keys give the fresh-install state (0.0.0) the
     * stock game starts from, whereas the old 0.15.10 values would make 1.2.8 run its "upgrade from 0.15.10" path.
     * gfx_msaa / gfx_texel_aa_2 are forced off: the host runs the D3D11 device at feature level 9_1 (facts fl91: no MSAA, no
     * TexelAA at 9_x) and 0 means "off" whatever the option's value domain is. */
    static const char defaults[] =
        "mp_username:Steve\r\n"
        "game_difficulty_new:1\r\n"
        "game_thirdperson:0\r\n"
        "gfx_dpadscale:0.5\r\n"
        "mp_server_visible:1\r\n"
        "mp_xboxlive_visible:0\r\n"
        "game_language:en_US\r\n"
        "game_skintypefull:Standard_Steve\r\n"
        "game_lastcustomskinnew:\r\n"
        "ctrl_sensitivity2:0.33\r\n"
        "ctrl_invertmouse:0\r\n"
        "ctrl_islefthanded:0\r\n"
        "ctrl_usetouchjoypad:0\r\n"
        "ctrl_swapjumpandsneak:0\r\n"
        "feedback_vibration:1\r\n"
        "ctrl_autojump:0\r\n"
        "gfx_viewbobbing:1\r\n"
        "gfx_fancygraphics:1\r\n"
        "gfx_fancyskies:1\r\n"
        "gfx_field_of_view:70\r\n"
        "gfx_gamma:0\r\n"
        "gfx_fullscreen:0\r\n"
        "gfx_msaa:0\r\n"
        "gfx_texel_aa_2:0\r\n"
        "audio_sound:1\r\n"
        "audio_music:1\r\n"
        "dev_autoloadlevel:0\r\n"
        "dev_showchunkmap:0\r\n";
#else
    static const char defaults[] =
        "mp_username:Steve\r\n"
        "game_difficulty_new:1\r\n"
        "game_thirdperson:0\r\n"
        "gfx_dpadscale:0.5\r\n"
        "mp_server_visible:1\r\n"
        "mp_xboxlive_visible:0\r\n"
        "game_flatworldlayers:[7,3,3,2]\r\n"
        "game_limitworldsize:0\r\n"
        "game_language:en_US\r\n"
        "game_skintypefull:Standard_Steve\r\n"
        "game_lastcustomskinnew:\r\n"
        "ctrl_sensitivity:0.33\r\n"
        "ctrl_invertmouse:0\r\n"
        "ctrl_islefthanded:0\r\n"
        "ctrl_usetouchscreen:0\r\n"
        "ctrl_usetouchjoypad:0\r\n"
        "ctrl_swapjumpandsneak:0\r\n"
        "feedback_vibration:1\r\n"
        "ctrl_autojump:0\r\n"
        "ctrl_keyboardlayout:0\r\n"
        "ctrl_gamePadMap:[0,1,t:1,t:0,2,3,10,11,8,9,13,4,5,6,12,X,X,X,X]\r\n"
        "gfx_renderdistance_new:96\r\n"
        "gfx_viewbobbing:1\r\n"
        "gfx_fancygraphics:1\r\n"
        "gfx_fancyskies:1\r\n"
        "gfx_animatetextures:1\r\n"
        "gfx_hidegui:0\r\n"
        "gfx_field_of_view:70\r\n"
        "gfx_gamma:0\r\n"
        "gfx_fullscreen:0\r\n"
        "audio_sound:1\r\n"
        "audio_music:1\r\n"
        "dev_autoloadlevel:0\r\n"
        "dev_showchunkmap:0\r\n"
        "dev_disablefilesystem:0\r\n"
        "old_game_version_major:0\r\n"
        "old_game_version_minor:15\r\n"
        "old_game_version_patch:10\r\n"
        "old_game_version_beta:0\r\n";
#endif
    DWORD written;

    if (SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL,
                         SHGFP_TYPE_CURRENT, roaming) != S_OK) {
        log_line("SHGetFolderPathW(CSIDL_APPDATA) failed");
        return;
    }
    if (lstrlenW(roaming) + 64 >= MAX_PATH) {
        log_line("APPDATA path is too long");
        return;
    }
    lstrcpyW(game_dir, roaming);
    lstrcatW(game_dir, L"\\MinecraftPE");
    lstrcpyW(game_data_path, game_dir);
    CreateDirectoryW(game_dir, NULL);

    lstrcpyW(options_path, game_dir);
    lstrcatW(options_path, L"\\games");
    CreateDirectoryW(options_path, NULL);
    lstrcatW(options_path, L"\\com.mojang");
    CreateDirectoryW(options_path, NULL);
    lstrcatW(options_path, L"\\minecraftpe");
    CreateDirectoryW(options_path, NULL);
    lstrcatW(options_path, L"\\options.txt");
    file = CreateFileW(options_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_EXISTS) {
            WIN32_FILE_ATTRIBUTE_DATA attributes;

            if (GetFileAttributesExW(
                    options_path, GetFileExInfoStandard, &attributes) &&
                (attributes.nFileSizeHigh || attributes.nFileSizeLow)) {
                log_line("roaming MinecraftPE options.txt already exists");
                return;
            }
            file = CreateFileW(
                options_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file == INVALID_HANDLE_VALUE) {
                log_line("could not regenerate empty options.txt");
                return;
            }
            log_line("regenerating empty roaming MinecraftPE options.txt");
        } else {
            log_line("CreateFileW(options.txt) failed");
            return;
        }
    }
    WriteFile(file, defaults, (DWORD)(sizeof(defaults) - 1), &written, NULL);
    CloseHandle(file);
    log_line("created roaming MinecraftPE/games/com.mojang/minecraftpe/options.txt");
}

static void log_hresult(const char *operation, HRESULT result)
{
    char line[160];
    wsprintfA(line, "%s: HRESULT 0x%08lx", operation, (unsigned long)result);
    log_line(line);
}

static void log_pointer(const char *name, const void *value)
{
    char line[160];
    wsprintfA(line, "%s: %p", name, value);
    log_line(line);
}


static BOOL safe_read_pointer(const void *address, void **value)
{
    SIZE_T bytes_read = 0;
    *value = NULL;
    return ReadProcessMemory(
        GetCurrentProcess(), address, value, sizeof(*value), &bytes_read) &&
        bytes_read == sizeof(*value);
}

#ifdef WIN32CRAFT_ARM32
/* ARM32 initialize_game_d3d (facts gfx_device.* / fl91.*): game owner via the game's operator new(0xd0) + owner ctor,
 * game InitDevice (which calls D3D11CreateDevice itself through the hooked IAT), host-created HWND swap chain
 * (B8G8R8A8, 2 buffers, DISCARD, flags 0), game InitTargets, swap chain stored at renderer+0x104, Present hooks. */
static BOOL initialize_game_d3d(HWND window)
{
    BYTE *image = arm32_gfx_image();
    Arm32GfxOperatorNewFn game_new =
        (Arm32GfxOperatorNewFn)ARM_CODE(image, ARM_GFX_DEVICE_OPERATOR_NEW);
    Arm32GfxOwnerCtorFn owner_ctor =
        (Arm32GfxOwnerCtorFn)ARM_CODE(image, ARM_GFX_DEVICE_OWNER_CTOR);
    Arm32GfxInitDeviceFn init_device =
        (Arm32GfxInitDeviceFn)ARM_CODE(image, ARM_GFX_DEVICE_INIT_DEVICE);
    void **owner_cell = (void **)ARM_DATA(image, ARM_GFX_DEVICE_OWNER_GLOBAL);
    BYTE *owner;
    BYTE *renderer;
    ID3D11Device *device;
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory *factory = NULL;
    DXGI_SWAP_CHAIN_DESC description;
    RECT client;
    HRESULT result;
    UINT width;
    UINT height;
    UINT owner_feature_level;
    char line[256];
    BOOL code_ok;

    /* The facts hold for the patched 1.2.8 ARM32 image only: verify every function we are about to call. */
    code_ok = arm32_check_bytes(image + ARM_GFX_DEVICE_OPERATOR_NEW, ARM_GFX_DEVICE_OPERATOR_NEW_EXPECTED,
                                sizeof(ARM_GFX_DEVICE_OPERATOR_NEW_EXPECTED), "gfx operator new");
    code_ok &= arm32_check_bytes(image + ARM_GFX_DEVICE_OWNER_CTOR, ARM_GFX_DEVICE_OWNER_CTOR_EXPECTED,
                                 sizeof(ARM_GFX_DEVICE_OWNER_CTOR_EXPECTED), "gfx owner ctor");
    code_ok &= arm32_check_bytes(image + ARM_GFX_DEVICE_INIT_DEVICE, ARM_GFX_DEVICE_INIT_DEVICE_EXPECTED,
                                 sizeof(ARM_GFX_DEVICE_INIT_DEVICE_EXPECTED), "gfx InitDevice");
    code_ok &= arm32_check_bytes(image + ARM_GFX_DEVICE_INIT_TARGETS, ARM_GFX_DEVICE_INIT_TARGETS_EXPECTED,
                                 sizeof(ARM_GFX_DEVICE_INIT_TARGETS_EXPECTED), "gfx InitTargets");
    code_ok &= arm32_check_bytes(image + ARM_GFX_DEVICE_RELEASE_TARGETS, ARM_GFX_DEVICE_RELEASE_TARGETS_EXPECTED,
                                 sizeof(ARM_GFX_DEVICE_RELEASE_TARGETS_EXPECTED), "gfx ReleaseTargets");
    if (!code_ok) {
        log_line("initialize_game_d3d: game image does not match the 1.2.8 ARM32 facts; aborting");
        return FALSE;
    }

    log_line("calling game resource constructor");
    log_pointer("game operator new", (const void *)game_new);
    log_pointer("game owner global cell (before)", *owner_cell);
    /* Mirrors the game's own make_owner (0x13f2a0): new(0xd0), memset 0, ctor, owner+0xb8 = 0x9100 (D3D_FEATURE_LEVEL_9_1),
     * +0xbc/+0xc0/+0xc4 stay 0.  Publishing the pointer in the global cell makes the game skip its own make_owner +
     * InitDevice + queryAdapterInfo + AppPlatform vf143. */
    owner = (BYTE *)game_new(ARM_GFX_DEVICE_OWNER_SIZE);
    log_pointer("game owner allocation", owner);
    if (!owner) {
        log_line("game resource owner creation returned null");
        return FALSE;
    }
    ZeroMemory(owner, ARM_GFX_DEVICE_OWNER_SIZE);
    owner = (BYTE *)owner_ctor(owner);
    if (!owner) {
        log_line("game owner constructor returned null");
        return FALSE;
    }
    *(UINT *)(owner + 0xb8) = 0x9100u;
    *owner_cell = owner;
    log_line("game resource constructor returned");
    log_pointer("game resource owner", owner);
    renderer = *(BYTE **)(owner + ARM_GFX_DEVICE_OWNER_RENDERER_OFFSET);
    game_resource_owner = owner;
    game_renderer = renderer;
    log_pointer("game renderer", renderer);
    if (!renderer) {
        log_line("game renderer creation returned null");
        return FALSE;
    }
    if (*(void **)(renderer + 0x108) != owner) {
        log_pointer("game renderer owner back-pointer (+0x108) differs from the owner", *(void **)(renderer + 0x108));
    }

    log_line("calling game D3D11 device initializer");
    init_device(owner, renderer);
    log_line("game D3D11 device initializer returned");
    /* InitDevice does not check its WARP retry: a second failure would already have crashed inside the game
     * (0x5BC406).  If it returned, verify that it really produced a device and an immediate context. */
    device = *(ID3D11Device **)(owner + ARM_GFX_DEVICE_OWNER_DEVICE_OFFSET);
    game_context = *(ID3D11DeviceContext **)(renderer + ARM_GFX_DEVICE_RENDERER_CONTEXT);
    owner_feature_level = *(UINT *)(owner + 0xb8);
    log_pointer("game D3D11 device (owner+0xc0)", device);
    log_pointer("game D3D11 base context (renderer+0x110)", game_context);
    log_pointer("game D3D11 Context2 (renderer+0x114)", *(void **)(renderer + ARM_GFX_DEVICE_RENDERER_CONTEXT2));
    log_pointer("game D3D annotation (renderer+0x140)", *(void **)(renderer + 0x140));
    wsprintfA(line, "game owner feature level (owner+0xb8) = 0x%04X", owner_feature_level);
    log_line(line);

    /* D3D11CreateDevice keeps its own references only through the game; release anything an IAT hook retained */
    if (captured_base_device) {
        ID3D11Device_Release(captured_base_device);
        captured_base_device = NULL;
    }
    if (captured_base_context) {
        ID3D11DeviceContext_Release(captured_base_context);
        captured_base_context = NULL;
    }
    if (!device || !game_context || IsBadReadPtr(device, sizeof(void *)) || IsBadReadPtr(game_context, sizeof(void *)) ||
        !device->lpVtbl || !game_context->lpVtbl) {
        log_line("game InitDevice produced no usable ID3D11Device / ID3D11DeviceContext "
                 "(D3D11CreateDevice failed for hardware and WARP at the selected feature level); aborting");
        game_context = NULL;
        return FALSE;
    }
    if (!IsBadReadPtr(device->lpVtbl, 40 * sizeof(void *))) {
        UINT actual_level = COMCALL(device, 37, UINT, STDMETHODCALLTYPE)(device); /* ID3D11Device::GetFeatureLevel */
        wsprintfA(line, "ID3D11Device::GetFeatureLevel = 0x%04X", actual_level);
        log_line(line);
        if (actual_level && actual_level != owner_feature_level) {
            log_line("warning: owner+0xb8 and the device feature level differ");
        }
        if (actual_level) owner_feature_level = actual_level;
    }
    arm32_gfx_texture_limit = arm32_gfx_texture_limit_for_level(owner_feature_level);
    wsprintfA(line, "texture/backbuffer size limit for this feature level: %u", arm32_gfx_texture_limit);
    log_line(line);

    /* Only the first call may hook the context vtable: a second hook would capture our own hook as the original */
    if (!arm32_gfx_scissor_installed) {
        arm32_gfx_scissor_installed = install_scissor_fix(game_context);
    }

    result = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);
    if (FAILED(result)) {
        log_hresult("ID3D11Device::QueryInterface(IDXGIDevice)", result);
        return FALSE;
    }
    result = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    if (FAILED(result)) {
        log_hresult("IDXGIDevice::GetAdapter", result);
        goto fail;
    }
    result = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory, (void **)&factory);
    if (FAILED(result)) {
        log_hresult("IDXGIAdapter::GetParent(IDXGIFactory)", result);
        goto fail;
    }

    ZeroMemory(&client, sizeof(client));
    GetClientRect(window, &client);
    width = (UINT)(client.right - client.left);
    height = (UINT)(client.bottom - client.top);
    wsprintfA(line, "HWND client size %ux%u", width, height);
    log_line(line);
    if (!width || !height || (client.right - client.left) < 0 || (client.bottom - client.top) < 0) {
        width = ARM32_GFX_FALLBACK_WIDTH;
        height = ARM32_GFX_FALLBACK_HEIGHT;
        log_line("HWND client area is empty; using 1280x720 for the first swap chain");
    }
    arm32_gfx_clamp_size(&width, &height, "initial swap chain");

    ZeroMemory(&description, sizeof(description));
    description.BufferDesc.Width = width;
    description.BufferDesc.Height = height;
    description.BufferDesc.RefreshRate.Numerator = 0;
    description.BufferDesc.RefreshRate.Denominator = 0;
    description.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1; /* no MSAA (fl91.msaa_gate) */
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.OutputWindow = window;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    description.Flags = 0; /* no ALLOW_TEARING: owner+0x90 stays 0 so the game never asks for tearing presents */
    log_line("creating HWND swap chain for game device");
    game_swap_chain = NULL;
    result = IDXGIFactory_CreateSwapChain(
        factory, (IUnknown *)device, &description, &game_swap_chain);
    log_hresult("IDXGIFactory::CreateSwapChain(HWND BGRA)", result);
    if (FAILED(result) || !game_swap_chain) {
        game_swap_chain = NULL;
        goto fail;
    }
    /* The host has its own fullscreen toggle; keep DXGI from switching to exclusive fullscreen on Alt+Enter
     * (IDXGIFactory::MakeWindowAssociation, vtable slot 8, DXGI_MWA_NO_ALT_ENTER = 2). */
    result = COMCALL(factory, 8, HRESULT, STDMETHODCALLTYPE, HWND, UINT)(factory, window, 2u);
    log_hresult("IDXGIFactory::MakeWindowAssociation(NO_ALT_ENTER)", result);

    /* The game's Present1 path (used when [g_displaySettings+0x30] != 1) calls vtable slot 22 of the pointer stored at
     * renderer+0x104, so prefer the IDXGISwapChain1 interface pointer when the runtime offers it. */
    {
        IDXGISwapChain *swap_chain1 = NULL;
        result = ID3D11Device_QueryInterface(game_swap_chain, &arm32_gfx_iid_swap_chain1, &swap_chain1);
        if (SUCCEEDED(result) && swap_chain1) {
            if (swap_chain1 != game_swap_chain) {
                IDXGISwapChain_Release(game_swap_chain);
                game_swap_chain = swap_chain1;
            } else {
                IDXGISwapChain_Release(swap_chain1);
            }
            log_pointer("HWND swap chain (IDXGISwapChain1)", game_swap_chain);
        } else {
            log_hresult("QueryInterface(IDXGISwapChain1) failed; the game's Present1 path will be unavailable", result);
        }
    }

    if (!arm32_gfx_recreate_targets(width, height, "startup")) {
        goto fail;
    }
    /* InitTargets does not store the swap chain (facts gfx_device.renderer_swapchain): Release the old value, AddRef ours */
    if (*(IDXGISwapChain **)(renderer + ARM_GFX_DEVICE_RENDERER_SWAPCHAIN) != game_swap_chain) {
        IDXGISwapChain *old = *(IDXGISwapChain **)(renderer + ARM_GFX_DEVICE_RENDERER_SWAPCHAIN);
        if (old) IDXGISwapChain_Release(old);
        IDXGISwapChain_AddRef(game_swap_chain);
        *(IDXGISwapChain **)(renderer + ARM_GFX_DEVICE_RENDERER_SWAPCHAIN) = game_swap_chain;
        log_line("HWND swap chain stored at renderer+0x104");
    }
    ID3D11DeviceContext_AddRef(game_context);
    if (!install_swap_chain_present_hook(game_swap_chain)) {
        /* without the hook DEVICE_REMOVED/RESET would reach the game, whose releaseDevice frees host-owned objects, and the
         * game_present_success_count bookkeeping of render_game_target_128 would double-present every frame */
        log_line("initialize_game_d3d: the swap-chain Present hook could not be installed; aborting");
        goto fail;
    }
    {
        HRESULT startup_present;
        startup_present = IDXGISwapChain_Present(game_swap_chain, 0, 0);
        log_hresult("initial HWND backbuffer present", startup_present);
    }
    log_line("game D3D11 device and HWND swap chain initialized");

    if (factory) IDXGIFactory_Release(factory);
    if (adapter) IDXGIAdapter_Release(adapter);
    if (dxgi_device) IDXGIDevice_Release(dxgi_device);
    return TRUE;

fail:
    if (game_swap_chain) {
        IDXGISwapChain_Release(game_swap_chain);
        game_swap_chain = NULL;
    }
    if (factory) IDXGIFactory_Release(factory);
    if (adapter) IDXGIAdapter_Release(adapter);
    if (dxgi_device) IDXGIDevice_Release(dxgi_device);
    return FALSE;
}
#else /* !WIN32CRAFT_ARM32 */
static BOOL initialize_game_d3d(HWND window)
{
    BYTE *image = (BYTE *)GetModuleHandleW(NULL);
    SIZE_T operator_new_rva = host_is_128
        ? 0x010296d1 : (host_is_115 ? 0x00e1f4f8 : RVA_OPERATOR_NEW);
    SIZE_T owner_ctor_rva = host_is_128
        ? RVA_128_OWNER_CTOR : (host_is_115 ? 0x0070db60 : RVA_OWNER_CTOR);
    SIZE_T init_device_rva = host_is_128
        ? RVA_128_INIT_DEVICE : (host_is_115 ? 0x0070de60 : RVA_INIT_DEVICE);
    SIZE_T init_targets_rva = host_is_128
        ? RVA_128_INIT_TARGETS : (host_is_115 ? 0x0070dc00 : RVA_INIT_TARGETS);
    SIZE_T owner_global_rva = host_is_128
        ? RVA_128_OWNER_GLOBAL : (host_is_115 ? 0x0142edd8 : RVA_OWNER_GLOBAL);
    SIZE_T owner_size = host_is_128
        ? 0x000000d0 : (host_is_115 ? 0x000000c8 : OWNER_SIZE);
    SIZE_T owner_renderer_offset = host_is_128
        ? 0x000000ac : (host_is_115 ? 0x000000ac : OWNER_RENDERER_OFFSET);
    SIZE_T owner_device_offset = host_is_128
        ? 0x000000c0 : (host_is_115 ? 0x000000bc : OWNER_DEVICE_OFFSET);
    SIZE_T renderer_context_offset = host_is_128
        ? 0x00000110 : (host_is_115 ? 0x0000010c : RENDERER_CONTEXT_OFFSET);
    SIZE_T renderer_context2_offset = host_is_128
        ? 0x00000114 : (host_is_115 ? 0x0000013c : RENDERER_CONTEXT2_OFFSET);
    SIZE_T renderer_swap_chain_offset = host_is_128
        ? 0x00000104 : (host_is_115 ? 0x00000100 : RENDERER_SWAP_CHAIN_OFFSET);
    SIZE_T renderer_rtv_offset = host_is_128
        ? 0x00000118 : (host_is_115 ? 0x00000114 : RENDERER_RTV_OFFSET);
    SIZE_T renderer_dsv_offset = host_is_128
        ? 0x00000120 : (host_is_115 ? 0x0000011c : RENDERER_DSV_OFFSET);
    SIZE_T renderer_backbuffer_offset = host_is_128
        ? 0x00000124 : (host_is_115 ? 0x00000120 : 0x00000114);
    GameOperatorNewFn game_new =
        (GameOperatorNewFn)(image + operator_new_rva);
    GameOwnerCtorFn owner_ctor =
        (GameOwnerCtorFn)(image + owner_ctor_rva);
    GameInitDeviceFn init_device =
        (GameInitDeviceFn)(image + init_device_rva);
    GameInitTargetsFn init_targets =
        (GameInitTargetsFn)(image + init_targets_rva);
    void *owner;
    void *renderer;
    ID3D11Device *device;
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory *factory = NULL;
    DXGI_SWAP_CHAIN_DESC description;
    RECT client;
    HRESULT result;
    struct {
        UINT width;
        UINT height;
        BYTE reserved_08[0x11];
        BYTE stereo;
        BYTE reserved_1a[2];
        IDXGISwapChain *swap_chain;
    } display;

    log_line("calling game resource constructor");
    log_pointer("game operator new", (const void *)game_new);
    owner = game_new(owner_size);
    log_pointer("game owner allocation", owner);
    if (owner) {
        ZeroMemory(owner, owner_size);
        owner = owner_ctor(owner);
        *(void **)(image + owner_global_rva) = owner;
    }
    log_line("game resource constructor returned");
    log_pointer("game resource owner", owner);
    if (!owner) {
        log_line("game resource owner creation returned null");
        return FALSE;
    }
    renderer = *(void **)((BYTE *)owner + owner_renderer_offset);
    game_resource_owner = owner;
    game_renderer = renderer;
    log_pointer("game renderer", renderer);
    if (!renderer) {
        log_line("game renderer creation returned null");
        return FALSE;
    }
    log_line("calling game D3D11 device initializer");
    init_device(owner, renderer);
    log_line("game D3D11 device initializer returned");
    device = *(ID3D11Device **)((BYTE *)owner + owner_device_offset);
    /*
     * D3D11CreateDevice stores the base immediate context at +0x100 and
     * then QIs ID3D11DeviceContext2 into +0x130.  Minecraft's frame setup,
     * clear and draw paths use the base pointer.  Wine exposes distinct
     * vtables for the two interface pointers, so hooking +0x130 misses the
     * entire render stream even though both refer to the same device
     * context.
     */
    game_context = *(ID3D11DeviceContext **)(
        (BYTE *)renderer + renderer_context_offset);
    log_pointer("game D3D11 base context (+0x100)", game_context);
    log_pointer("game D3D11 Context2 (+0x130)",
        *(void **)((BYTE *)renderer + renderer_context2_offset));

    if (host_is_windows7 && (!device || !game_context) &&
        captured_base_device && captured_base_context) {
        /* The UWP binary requested ID3D11Device2 and ID3D11DeviceContext2.
         * Windows 7 returns E_NOINTERFACE, but the base interfaces are fully
         * sufficient for the renderer paths used here. Transfer our retained
         * references into the exact fields that the failed QI left empty. */
        if (!device) {
            *(ID3D11Device **)((BYTE *)owner + owner_device_offset) =
                captured_base_device;
            device = captured_base_device;
            captured_base_device = NULL;
            log_line("Windows 7 fallback: base ID3D11Device stored at owner+0xA4");
        }
        if (!game_context) {
            *(ID3D11DeviceContext **)(
                (BYTE *)renderer + renderer_context_offset) =
                captured_base_context;
            game_context = captured_base_context;
            captured_base_context = NULL;
            log_line("Windows 7 fallback: base ID3D11DeviceContext stored at renderer+0x100");
        }
    }

    /* A successful Device2 QI on newer Windows makes the retained base
     * references unnecessary. */
    if (captured_base_device) {
        ID3D11Device_Release(captured_base_device);
        captured_base_device = NULL;
    }
    if (captured_base_context) {
        ID3D11DeviceContext_Release(captured_base_context);
        captured_base_context = NULL;
    }

    log_pointer("game D3D11 device", device);
    log_pointer("game D3D11 context", game_context);
    if (!device || !game_context) {
        log_line("game D3D device/context is null after Windows 7 fallback");
        return FALSE;
    }
    install_scissor_fix(game_context);

    result = ID3D11Device_QueryInterface(
        device, &IID_IDXGIDevice, (void **)&dxgi_device);
    if (FAILED(result)) {
        log_hresult("ID3D11Device::QueryInterface(IDXGIDevice)", result);
        return FALSE;
    }
    result = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    if (FAILED(result)) {
        log_hresult("IDXGIDevice::GetAdapter", result);
        goto fail;
    }
    result = IDXGIAdapter_GetParent(
        adapter, &IID_IDXGIFactory, (void **)&factory);
    if (FAILED(result)) {
        log_hresult("IDXGIAdapter::GetParent(IDXGIFactory)", result);
        goto fail;
    }

    GetClientRect(window, &client);
    ZeroMemory(&description, sizeof(description));
    description.BufferDesc.Width = (UINT)(client.right - client.left);
    description.BufferDesc.Height = (UINT)(client.bottom - client.top);
    description.BufferDesc.RefreshRate.Numerator = 0;
    description.BufferDesc.RefreshRate.Denominator = 0;
    description.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = host_is_windows7 ? 1 : 2;
    description.OutputWindow = window;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    log_line("creating HWND swap chain for game device");
    result = IDXGIFactory_CreateSwapChain(
        factory, (IUnknown *)device, &description, &game_swap_chain);
    log_hresult("IDXGIFactory::CreateSwapChain(HWND BGRA)", result);
    if (FAILED(result) && host_is_windows7) {
        description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        game_swap_chain = NULL;
        result = IDXGIFactory_CreateSwapChain(
            factory, (IUnknown *)device, &description, &game_swap_chain);
        log_hresult("IDXGIFactory::CreateSwapChain(HWND RGBA fallback)", result);
    }
    if (FAILED(result)) {
        goto fail;
    }

    ZeroMemory(&display, sizeof(display));
    display.width = description.BufferDesc.Width;
    display.height = description.BufferDesc.Height;
    display.swap_chain = game_swap_chain;
    log_line("calling game render-target initializer");
    init_targets(owner, &display, renderer);
    log_line("game render-target initializer returned");
    /*
     * The UWP CoreWindow path normally stores its IDXGISwapChain on the
     * renderer's frame-present helper before target creation. Our desktop
     * HWND path calls target creation directly, so supply that retained
     * reference explicitly for the first in-game Present.
     */
    if (*(IDXGISwapChain **)((BYTE *)renderer +
                            renderer_swap_chain_offset) != game_swap_chain) {
        IDXGISwapChain *old = *(IDXGISwapChain **)(
            (BYTE *)renderer + renderer_swap_chain_offset);
        if (old) IDXGISwapChain_Release(old);
        IDXGISwapChain_AddRef(game_swap_chain);
        *(IDXGISwapChain **)((BYTE *)renderer +
                            renderer_swap_chain_offset) = game_swap_chain;
        log_line("HWND swap chain stored at renderer+0xF4");
    }
    game_target =
        *(ID3D11RenderTargetView **)((BYTE *)renderer + renderer_rtv_offset);
    log_pointer("game render target", game_target);
    log_pointer(
        "game depth target",
        *(void **)((BYTE *)renderer + renderer_dsv_offset));
    log_pointer(
        "game backbuffer texture",
        *(void **)((BYTE *)renderer + renderer_backbuffer_offset));
    if (!game_target) {
        ID3D11Texture2D *backbuffer = NULL;
        log_line("game render target creation returned null; trying direct RTV fallback");
        result = IDXGISwapChain_GetBuffer(
            game_swap_chain, 0, &IID_ID3D11Texture2D, (void **)&backbuffer);
        log_hresult("IDXGISwapChain::GetBuffer direct fallback", result);
        if (SUCCEEDED(result) && backbuffer) {
            result = ID3D11Device_CreateRenderTargetView(
                device, backbuffer, NULL, &game_target);
            ID3D11Texture2D_Release(backbuffer);
            log_hresult("ID3D11Device::CreateRenderTargetView direct fallback", result);
            if (SUCCEEDED(result) && game_target) {
                *(ID3D11RenderTargetView **)(
                    (BYTE *)renderer + renderer_rtv_offset) = game_target;
            }
        }
        if (!game_target) {
            goto fail;
        }
    }
    ID3D11DeviceContext_AddRef(game_context);
    ID3D11RenderTargetView_AddRef(game_target);
    install_swap_chain_present_hook(game_swap_chain);
    {
        HRESULT startup_present;
        startup_present = IDXGISwapChain_Present(game_swap_chain, 0, 0);
        log_hresult("initial HWND backbuffer present", startup_present);
    }
    log_line("game D3D11 device and HWND swap chain initialized");

    if (factory) IDXGIFactory_Release(factory);
    if (adapter) IDXGIAdapter_Release(adapter);
    if (dxgi_device) IDXGIDevice_Release(dxgi_device);
    return TRUE;

fail:
    if (game_swap_chain) {
        IDXGISwapChain_Release(game_swap_chain);
        game_swap_chain = NULL;
    }
    if (factory) IDXGIFactory_Release(factory);
    if (adapter) IDXGIAdapter_Release(adapter);
    if (dxgi_device) IDXGIDevice_Release(dxgi_device);
    return FALSE;
}
#endif /* WIN32CRAFT_ARM32 initialize_game_d3d */

static BOOL initialize_game_app(BYTE *image)
{
    GameCreateDeviceResourcesFn create_device_resources =
        (GameCreateDeviceResourcesFn)(image + RVA_CREATE_DEVICE_RESOURCES);
    GameCreateAppMainFn create_app_main =
        (GameCreateAppMainFn)(image + RVA_CREATE_APP_MAIN);
    void *device_resources;
    void *holder_result;
    struct {
        int mode;
        char text[16];
        UINT length;
        UINT capacity;
    } startup_context;

    game_app_main = NULL;
    ZeroMemory(&startup_context, sizeof(startup_context));
    startup_context.capacity = 15;

    /*
     * MainPage 0.15.10 constructs AppMain while the DeviceResources global
     * is still null, then publishes DeviceResources.  MinecraftClient uses
     * that distinction while creating its renderer/pass lists, so preserve
     * the version's native order even though 0.13.2 used the reverse order.
     */
    log_line("calling MCPE_Host::AppMain constructor");
    holder_result = create_app_main(&game_app_main, &startup_context);
    log_pointer("AppMain holder result", holder_result);
    log_pointer("MCPE_Host::AppMain", game_app_main);
    if (!game_app_main) {
        log_line("MCPE_Host::AppMain construction returned null");
        return FALSE;
    }
    log_pointer("AppMain platform", *(void **)((BYTE *)game_app_main + 4));
    log_pointer("AppMain game", *(void **)((BYTE *)game_app_main + 8));
    log_pointer("AppMain services", *(void **)((BYTE *)game_app_main + 12));
    log_pointer("AppMain frame enabled",
                (void *)(ULONG_PTR)*(BYTE *)(
                    (BYTE *)game_app_main + APPMAIN_FRAME_ENABLED_OFFSET));
    log_line("MCPE_Host::AppMain constructed");

    log_line("calling DX::DeviceResources constructor");
    device_resources = NULL;
    holder_result = create_device_resources(&device_resources);
    *(void **)(image + RVA_DEVICE_RESOURCES_GLOBAL) = device_resources;
    game_device_resources = device_resources;
    log_pointer("DeviceResources holder result", holder_result);
    log_pointer("DX::DeviceResources", device_resources);
    if (!device_resources) {
        log_line("DX::DeviceResources construction returned null");
        return FALSE;
    }

    /*
     * The UWP progress-screen path conditionally starts these two jobs.
     * The HWND host has no XAML progress screen, so compile the populated
     * material registries before the first MinecraftClient frame.
     */
    compile_material_group_now(
        image, RVA_ENTITY_MATERIAL_GROUP, "entity/UI");
    compile_material_group_now(
        image, RVA_TERRAIN_MATERIAL_GROUP, "terrain");
    return TRUE;
}

static DWORD get_current_thread_id_dynamic(void)
{
    typedef DWORD (WINAPI *GetCurrentThreadIdFn)(void);
    static GetCurrentThreadIdFn function;
    if (!function) {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        function = kernel32
            ? (GetCurrentThreadIdFn)GetProcAddress(kernel32, "GetCurrentThreadId")
            : NULL;
    }
    return function ? function() : 0;
}

#ifdef WIN32CRAFT_ARM32
static void log_115_main_thread_context(const char *reason)
{
    typedef HANDLE (WINAPI *OpenThreadFn)(DWORD, BOOL, DWORD);
    typedef DWORD (WINAPI *SuspendThreadFn)(HANDLE);
    typedef BOOL (WINAPI *GetThreadContextFn)(HANDLE, CONTEXT *);
    typedef DWORD (WINAPI *ResumeThreadFn)(HANDLE);
    static OpenThreadFn open_thread;
    static SuspendThreadFn suspend_thread;
    static GetThreadContextFn get_thread_context;
    static ResumeThreadFn resume_thread;
    HMODULE kernel32;
    HANDLE thread;
    CONTEXT context;
    char line[192];

    if (!game115_main_thread_id) {
        log_line("ARM32 thread context: main thread id is unavailable");
        return;
    }
    kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!open_thread && kernel32) {
        open_thread = (OpenThreadFn)GetProcAddress(kernel32, "OpenThread");
        suspend_thread = (SuspendThreadFn)GetProcAddress(kernel32, "SuspendThread");
        get_thread_context = (GetThreadContextFn)GetProcAddress(kernel32, "GetThreadContext");
        resume_thread = (ResumeThreadFn)GetProcAddress(kernel32, "ResumeThread");
    }
    if (!open_thread || !suspend_thread || !get_thread_context || !resume_thread) {
        log_line("ARM32 thread context: thread-context APIs unavailable");
        return;
    }
    thread = open_thread(0x0002u | 0x0008u | 0x0040u, FALSE, game115_main_thread_id);
    if (!thread) {
        log_line("ARM32 thread context: OpenThread failed");
        return;
    }
    if (suspend_thread(thread) == 0xffffffffu) {
        log_line("ARM32 thread context: SuspendThread failed");
        CloseHandle(thread);
        return;
    }
    ZeroMemory(&context, sizeof(context));
    context.ContextFlags = 0x00200003u; /* CONTEXT_ARM | CONTEXT_CONTROL | CONTEXT_INTEGER */
    if (get_thread_context(thread, &context)) {
        wsprintfA(line, "ARM32 main thread context (%s)", reason ? reason : "snapshot");
        arm32_log_context(line, &context);
        arm32_log_stack_scan("main thread", context.Sp, 64, 64);
    } else {
        log_line("ARM32 thread context: GetThreadContext failed");
    }
    resume_thread(thread);
    CloseHandle(thread);
}
#else
static void log_115_main_thread_context(const char *reason)
{
    typedef HANDLE (WINAPI *OpenThreadFn)(DWORD, BOOL, DWORD);
    typedef DWORD (WINAPI *SuspendThreadFn)(HANDLE);
    typedef BOOL (WINAPI *GetThreadContextFn)(HANDLE, CONTEXT *);
    typedef DWORD (WINAPI *ResumeThreadFn)(HANDLE);
    static OpenThreadFn open_thread;
    static SuspendThreadFn suspend_thread;
    static GetThreadContextFn get_thread_context;
    static ResumeThreadFn resume_thread;
    HMODULE kernel32;
    HANDLE thread;
    CONTEXT context;
    DWORD *frame;
    unsigned index;
    char line[192];

    if (!game115_main_thread_id) {
        log_line("Minecraft 1.1.5 watchdog: main thread id is unavailable");
        return;
    }
    kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!open_thread && kernel32) {
        open_thread = (OpenThreadFn)GetProcAddress(kernel32, "OpenThread");
        suspend_thread = (SuspendThreadFn)GetProcAddress(kernel32, "SuspendThread");
        get_thread_context = (GetThreadContextFn)GetProcAddress(kernel32, "GetThreadContext");
        resume_thread = (ResumeThreadFn)GetProcAddress(kernel32, "ResumeThread");
    }
    if (!open_thread || !suspend_thread || !get_thread_context || !resume_thread) {
        log_line("Minecraft 1.1.5 watchdog: thread-context APIs unavailable");
        return;
    }
    thread = open_thread(0x0002u | 0x0008u | 0x0040u, FALSE,
                         game115_main_thread_id);
    if (!thread) {
        log_line("Minecraft 1.1.5 watchdog: OpenThread failed");
        return;
    }
    if (suspend_thread(thread) == 0xffffffffu) {
        log_line("Minecraft 1.1.5 watchdog: SuspendThread failed");
        CloseHandle(thread);
        return;
    }
    ZeroMemory(&context, sizeof(context));
    context.ContextFlags = 0x00010007u;
    if (get_thread_context(thread, &context)) {
        wsprintfA(line,
            "Minecraft 1.1.5 watchdog %s: EIP=%08X ESP=%08X EBP=%08X EAX=%08X ECX=%08X EDX=%08X",
            reason ? reason : "snapshot",
            context.Eip, context.Esp, context.Ebp,
            context.Eax, context.Ecx, context.Edx);
        log_line(line);
        frame = (DWORD *)(ULONG_PTR)context.Ebp;
        for (index = 0; index < 20; ++index) {
            DWORD next;
            DWORD ret;
            if (!frame || IsBadReadPtr(frame, 8)) break;
            next = frame[0];
            ret = frame[1];
            wsprintfA(line,
                "  watchdog frame %u: return=%08X frame=%08X%s",
                index, ret, (DWORD)(ULONG_PTR)frame,
                ret >= 0x00400000u && ret < 0x01900000u ? " (game)" : "");
            log_line(line);
            if (next <= (DWORD)(ULONG_PTR)frame || next - (DWORD)(ULONG_PTR)frame > 0x100000u)
                break;
            frame = (DWORD *)(ULONG_PTR)next;
        }
    } else {
        log_line("Minecraft 1.1.5 watchdog: GetThreadContext failed");
    }
    resume_thread(thread);
    CloseHandle(thread);
}
#endif

static DWORD WINAPI win32_115_constructor_watchdog_thread(void *parameter)
{
    LONG delay;
    char line[256];
    (void)parameter;
    for (delay = 0; delay < 3; ++delay) Sleep(1000);
    if (game115_appmain_factory_active) {
        wsprintfA(line,
            "Minecraft 1.1.5 watchdog: AppMainXaml still active after 3s; fopen=%ld wfopen=%ld scans=%ld",
            game115_fopen_log_count, game115_wfopen_log_count,
            game115_scan_sequence);
        log_line(line);
        dump_115_stdio_state();
        log_115_main_thread_context("3-second constructor stall");
    }
    for (delay = 0; delay < 7; ++delay) Sleep(1000);
    if (game115_appmain_factory_active) {
        wsprintfA(line,
            "Minecraft 1.1.5 watchdog: AppMainXaml still active after 10s; fopen=%ld wfopen=%ld scans=%ld",
            game115_fopen_log_count, game115_wfopen_log_count,
            game115_scan_sequence);
        log_line(line);
        dump_115_stdio_state();
        log_115_main_thread_context("10-second constructor stall");
    }
    return 0;
}

static void start_115_constructor_watchdog(void)
{
    typedef HANDLE (WINAPI *CreateThreadFn)(
        void *, SIZE_T, DWORD (WINAPI *)(void *), void *, DWORD, DWORD *);
    static CreateThreadFn create_thread;
    HMODULE kernel32;
    HANDLE thread;

    if (InterlockedExchange(&game115_watchdog_started, 1) != 0)
        return;
    if (!game115_main_thread_id)
        game115_main_thread_id = get_current_thread_id_dynamic();
    kernel32 = GetModuleHandleW(L"kernel32.dll");
    create_thread = kernel32
        ? (CreateThreadFn)GetProcAddress(kernel32, "CreateThread") : NULL;
    if (!create_thread) {
        log_line("Minecraft 1.1.5 constructor watchdog could not start");
        return;
    }
    thread = create_thread(NULL, 0, win32_115_constructor_watchdog_thread,
                           NULL, 0, NULL);
    if (!thread) {
        log_line("Minecraft 1.1.5 constructor watchdog CreateThread failed");
        return;
    }
    CloseHandle(thread);
    log_line("Minecraft 1.1.5 constructor watchdog armed for 3s/10s snapshots");
}

static BOOL initialize_game_app_115(BYTE *image)
{
    GameCreateDeviceResourcesFn create_device_resources =
        (GameCreateDeviceResourcesFn)(
            image + RVA_115_CREATE_DEVICE_RESOURCES);
    GameCreateAppMainXamlFn create_app_main =
        (GameCreateAppMainXamlFn)(
            image + RVA_115_CREATE_APP_MAIN_XAML);
    void *device_resources = NULL;
    void *holder_result;
    void *core_window = &win32_core_window;
    void *xaml_panel = NULL;
    void *pointer_source = NULL;
    struct {
        int mode;
        BYTE flag;
        BYTE reserved_05[3];
        char text[16];
        UINT length;
        UINT capacity;
    } startup_context;

    /*
     * MainPage 1.1.5 publishes DeviceResources before it constructs
     * AppMainXaml.  Preserve that order: AppPlatform_Winrt consults the
     * singleton while building MinecraftGame.
     */
    log_line("Minecraft 1.1.5 DeviceResources constructor begin");
    holder_result = create_device_resources(&device_resources);
    log_pointer("Minecraft 1.1.5 DeviceResources holder result", holder_result);
    log_pointer("Minecraft 1.1.5 DeviceResources pointer", device_resources);
    *(void **)(image + RVA_115_DEVICE_RESOURCES_GLOBAL) =
        device_resources;
    game_device_resources = device_resources;
    if (!device_resources) {
        log_line("Minecraft 1.1.5 DeviceResources construction failed");
        return FALSE;
    }
    log_line("Minecraft 1.1.5 DeviceResources constructed");

    /*
     * The startup context is { int mode, bool flag, std::string text }.
     * The three interface holders normally come from MainPage's XAML
     * controls.  Passing empty holders lets the desktop activation and D3D
     * redirects provide the non-XAML services while we retain an HWND swap
     * chain.
     */
    ZeroMemory(&startup_context, sizeof(startup_context));
    startup_context.capacity = 15;
    game_app_main = NULL;
    log_pointer("Minecraft 1.1.5 CoreWindow holder value", core_window);
    log_pointer("Minecraft 1.1.5 XAML panel holder value", xaml_panel);
    log_pointer("Minecraft 1.1.5 pointer source holder value", pointer_source);
    log_line("Minecraft 1.1.5 AppMainXaml factory begin");
    game115_main_thread_id = get_current_thread_id_dynamic();
    InterlockedExchange(&game115_appmain_factory_active, 1);
    start_115_constructor_watchdog();
    holder_result = call_game_create_app_main_xaml(
        create_app_main,
        &game_app_main, &startup_context,
        &core_window, &xaml_panel, &pointer_source);
    InterlockedExchange(&game115_appmain_factory_active, 0);
    log_line("Minecraft 1.1.5 AppMainXaml factory returned");
    log_pointer("Minecraft 1.1.5 AppMainXaml holder result", holder_result);
    log_pointer("Minecraft 1.1.5 AppMainXaml pointer", game_app_main);
    if (!game_app_main) {
        log_line("Minecraft 1.1.5 AppMainXaml construction failed");
        return FALSE;
    }
    log_line("Minecraft 1.1.5 AppMainXaml constructed");
    log_pointer("Minecraft 1.1.5 AppPlatform",
                *(void **)((BYTE *)game_app_main + 4));
    log_pointer("Minecraft 1.1.5 MinecraftGame",
                *(void **)((BYTE *)game_app_main + 8));
    return TRUE;
}

#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 AppMain bring-up (unit appmain; facts appmain.*, input.controller_input_handler, gfx_device.renderer_dsv).
 *
 *   AppMain (0x28 bytes): +0 vtable, +4 restart flag, +8 StartupCtx*, +0xc Agile<CoreWindow>, +0x1c AppPlatform_Winrt*,
 *   +0x20 MinecraftGame*, +0x24 helper.  There is NO frame-enable byte (AppMain+0x10 is Agile storage: never write it).
 *   AppMain::vf1 (restart_game) destroys and recreates MinecraftGame, so the game pointer is re-read from
 *   AppMain+0x20 every time and never cached.
 * ====================================================================================================== */
#define ARM32_AM_PLATFORM_OFFSET 0x1cu          /* facts appmain.layout */
#define ARM32_AM_GAME_OFFSET 0x20u
#define ARM32_AM_SIZE 0x28u                     /* sizeof(AppMain), facts appmain.size_check_0x24 */
#define ARM32_AM_PLATFORM_PROBE 0x3b8u          /* covers +0x330/+0x334 and the +0x3b4 InputAdapter* */
#define ARM32_AM_GAME_VTABLE_PROBE 0x64u        /* slots 0..24; the real vtable has 29 slots (0x74 bytes) */
/* AppPlatform_Winrt vtable 0x013695a4 (facts appmain.app_visibility): slot 27 = onActivated 0x583cf8,
 * slot 21 = setVisibleFlag 0x621620 (== AppView::OnVisibilityChanged(visible=true) inlined) */
#define ARM32_AM_PLATFORM_ON_ACTIVATED_SLOT 27
#define ARM32_AM_PLATFORM_SET_VISIBLE_SLOT 21
#define ARM32_AM_PLATFORM_ON_ACTIVATED_RVA 0x00583cf8u
#define ARM32_AM_PLATFORM_SET_VISIBLE_RVA 0x00621620u
/* MinecraftGame vtable 0x0133bacc (facts appmain.game_vf23_setSize / game_vf24_windowSizeChanged) */
#define ARM32_AM_GAME_SET_SIZE_SLOT 23
#define ARM32_AM_GAME_WINDOW_SIZE_CHANGED_SLOT 24
#define ARM32_AM_HID_HANDLER_OFFSET 0x10u       /* HIDController+0x10 = InputAdapter* (facts input.controller_input_handler) */

/* An ARM code pointer stored in data must equal image+rva+1 (Thumb bit set). */
static BOOL arm32_appmain_code_cell_is(const void *actual, BYTE *image, DWORD rva, const char *what)
{
    char line[200];
    const void *expected = ARM_CODE(image, rva);
    if (actual == expected) return TRUE;
    wsprintfA(line, "ARM32 AppMain: %s is %p, expected image+0x%08lx+1 = %p", what, actual, (unsigned long)rva,
              expected);
    log_line(line);
    return FALSE;
}

static BOOL initialize_game_app_128(BYTE *image)
{
    /* facts appmain.startup_ctx: StartupCtx is 0x20 bytes {int mode; byte flag; std::string at +8 (SSO buffer +8,
     * size +0x18, capacity +0x1c)}.  AppMain keeps the POINTER (AppMain+8) and re-reads it on restart, so it is static;
     * it is 8-byte aligned because the game's string::assign may use ldrd/strd. */
    static DWORD startup_context[0x40 / sizeof(DWORD)] __attribute__((aligned(8)));
    /* factory argument 3 is a pointer to a CoreWindow*; the holder itself is only dereferenced once but keep it static */
    static void *activation_argument;
    void *holder_result;
    void *platform;
    void *game;
    char line[256];

    if (!image) {
        log_line("Minecraft 1.2.8 AppMain: game image base is NULL");
        return FALSE;
    }
    /* sanity: the code we are about to call must be the ARM 1.2.8 bytes recorded in the facts */
    if (!arm32_check_bytes(image + ARM_APPMAIN_CREATE_APP_MAIN, ARM_APPMAIN_CREATE_APP_MAIN_EXPECTED,
                           sizeof(ARM_APPMAIN_CREATE_APP_MAIN_EXPECTED), "AppMain factory 0x597b4c") ||
        !arm32_check_bytes(image + ARM_APPMAIN_APP_MAIN_FRAME, ARM_APPMAIN_APP_MAIN_FRAME_EXPECTED,
                           sizeof(ARM_APPMAIN_APP_MAIN_FRAME_EXPECTED), "AppMain::update_frame 0x57d3a8")) {
        log_line("Minecraft 1.2.8 AppMain: game code bytes do not match the ARM32 facts; not starting");
        return FALSE;
    }

    /* The factory dereferences *holder as the CoreWindow and QIs/AddRefs it: the fakes are set up by
     * install_activation_redirect (install_arm32_iat_hooks), which must have run before this point. */
    if (!win32_core_window.vtable) {
        log_line("Minecraft 1.2.8 AppMain: the fake CoreWindow is not initialised (install_activation_redirect has not "
                 "run); not starting");
        return FALSE;
    }
    {
        /* AppMain::init / the game's present path use the GameOwner that initialize_game_d3d publishes in this global
         * (facts gfx_device.owner_global); log it so a missing owner shows up before the factory call hangs/crashes */
        void **owner_cell = (void **)ARM_DATA(image, ARM_GFX_DEVICE_OWNER_GLOBAL);
        void *owner_value = IsBadReadPtr(owner_cell, sizeof(void *)) ? NULL : *owner_cell;
        log_pointer("Minecraft 1.2.8 GameOwner global before AppMain construction", owner_value);
        if (!owner_value) log_line("warning: GameOwner global is NULL; initialize_game_d3d did not publish the owner");
    }
    ZeroMemory(startup_context, sizeof(startup_context));
    startup_context[0x1c / sizeof(DWORD)] = 0xf;    /* empty SSO std::string: size 0 (+0x18), capacity 0xF (+0x1c) */
    activation_argument = &win32_core_window;
    game_app_main = NULL;
    wsprintfA(line, "Minecraft 1.2.8 AppMain startup ctx %p holder %p CoreWindow %p; calling factory %p",
              (void *)startup_context, (void *)&activation_argument, activation_argument,
              ARM_CODE(image, ARM_APPMAIN_CREATE_APP_MAIN));
    log_line(line);
    /* AppMain** f(AppMain** result r0, StartupCtx* r1, CoreWindow** holder r2) -> r0; constructs AppPlatform_Winrt and
     * MinecraftGame, runs AppMain::init (waits for the platform worker thread) and sizes the game from the fake
     * CoreWindow.Bounds * DisplayInformation.LogicalDpi/96 */
    holder_result = call_game_create_app_main_128(
        image + ARM_APPMAIN_CREATE_APP_MAIN, &game_app_main,
        startup_context, &activation_argument);
    log_pointer("Minecraft 1.2.8 AppMain holder result", holder_result);
    log_pointer("Minecraft 1.2.8 AppMain", game_app_main);
    if (!game_app_main || IsBadReadPtr(game_app_main, ARM32_AM_SIZE)) {
        log_line("Minecraft 1.2.8 AppMain construction failed");
        return FALSE;
    }
    if (holder_result != (void *)&game_app_main) {
        log_line("Minecraft 1.2.8 AppMain: factory did not return its result pointer (continuing)");
    }
    if (*(void **)game_app_main != (void *)(image + ARM_APPMAIN_APPMAIN_VTABLE)) {
        wsprintfA(line, "Minecraft 1.2.8 AppMain vtable is %p, expected %p (continuing)",
                  *(void **)game_app_main, (void *)(image + ARM_APPMAIN_APPMAIN_VTABLE));
        log_line(line);
    }
    platform = *(void **)((BYTE *)game_app_main + ARM32_AM_PLATFORM_OFFSET);
    game = *(void **)((BYTE *)game_app_main + ARM32_AM_GAME_OFFSET);
    log_pointer("Minecraft 1.2.8 AppPlatform", platform);
    log_pointer("Minecraft 1.2.8 MinecraftGame", game);
    if (!platform || !game) {
        log_line("Minecraft 1.2.8 AppMain: AppPlatform or MinecraftGame missing after AppMain::init");
        return FALSE;
    }
    if (!IsBadReadPtr(platform, ARM32_AM_PLATFORM_PROBE)) {
        /* what AppMain::init derived from the fake CoreWindow/DisplayInformation (facts appmain.platform_dpi_scale):
         * LogicalDpi 96 must give scale 1.0 and the size must be the client rect */
        wsprintfA(line, "Minecraft 1.2.8 AppMain init: platform size %dx%d, dpi scale x%d y%d (per mille), "
                  "state %lu, AppMain+4 restart=%u",
                  *(int *)((BYTE *)platform + ARM_APPMAIN_PLATFORM_SCREEN_WIDTH),
                  *(int *)((BYTE *)platform + ARM_APPMAIN_PLATFORM_SCREEN_HEIGHT),
                  (int)(*(float *)((BYTE *)platform + ARM_APPMAIN_PLATFORM_DPI_SCALE) * 1000.0f),
                  (int)(*(float *)((BYTE *)platform + ARM_APPMAIN_PLATFORM_DPI_SCALE + 4) * 1000.0f),
                  (unsigned long)*(DWORD *)((BYTE *)platform + 0x3b0),
                  (unsigned)*((BYTE *)game_app_main + 4));
        log_line(line);
    }
    return TRUE;
}

static BOOL activate_game_app_128(HWND window)
{
    typedef void (*PlatformOnActivatedFn)(void *);
    typedef void (*PlatformSetVisibleFn)(void *, int);
    typedef void (*GameSetSizeFn)(void *, int, int, float);
    typedef void (*GameWindowSizeChangedFn128)(void *, int, int);
    BYTE *image = game_image;
    void *platform;
    void *game;
    void **platform_vtable;
    void **game_vtable;
    RECT client;
    int width;
    int height;
    char line[256];

    if (!game_app_main || !image || !GetClientRect(window, &client) ||
        IsBadReadPtr(game_app_main, ARM32_AM_SIZE)) {
        wsprintfA(line, "Minecraft 1.2.8 activate: AppMain %p, image %p, window %p, GetLastError %lu: "
                  "AppMain missing/unreadable or GetClientRect failed", game_app_main, (void *)image, (void *)window,
                  (unsigned long)GetLastError());
        log_line(line);
        return FALSE;
    }
    platform = *(void **)((BYTE *)game_app_main + ARM32_AM_PLATFORM_OFFSET);
    game = *(void **)((BYTE *)game_app_main + ARM32_AM_GAME_OFFSET);
    if (!platform || !game || IsBadReadPtr(game, sizeof(void *)) ||
        IsBadReadPtr(platform, ARM32_AM_PLATFORM_PROBE)) {
        wsprintfA(line, "Minecraft 1.2.8 activate: AppPlatform %p or MinecraftGame %p missing/unreadable", platform, game);
        log_line(line);
        return FALSE;
    }
    width = client.right - client.left;
    height = client.bottom - client.top;
    {
        /* gfx_bootstrap clamps the swap-chain buffers to the feature-level texture limit; the game must be told the size of
         * the buffers it renders into (platform+0x330/+0x334, setSize), not the window's client size */
        UINT buffer_width = 0;
        UINT buffer_height = 0;

        arm32_gfx_get_backbuffer_size(&buffer_width, &buffer_height);
        if (buffer_width && buffer_height) {
            if ((UINT)width != buffer_width || (UINT)height != buffer_height) {
                wsprintfA(line, "Minecraft 1.2.8 activate: client area %dx%d, swap-chain buffers %ux%u: using the buffer size",
                          width, height, buffer_width, buffer_height);
                log_line(line);
            }
            width = (int)buffer_width;
            height = (int)buffer_height;
        } else {
            log_line("Minecraft 1.2.8 activate: swap-chain buffer size unknown, using the client size");
        }
    }

    /* (a) visibility == AppView::OnVisibilityChanged(visible=TRUE), inlined on ARM (facts appmain.app_visibility):
     *     p = AppMain->platform; vt = *p; vt[27](p) (state 2 -> 1); vt[21](p, 1) (platform+9 = visible).
     *     The cells hold Thumb pointers already, so they are called as plain functions. */
    platform_vtable = *(void ***)platform;
    if (!platform_vtable || IsBadReadPtr(platform_vtable, (ARM32_AM_PLATFORM_ON_ACTIVATED_SLOT + 1) * sizeof(void *))) {
        log_pointer("Minecraft 1.2.8 AppPlatform vtable unreadable", platform_vtable);
        return FALSE;
    }
    if (!arm32_appmain_code_cell_is(platform_vtable[ARM32_AM_PLATFORM_ON_ACTIVATED_SLOT], image,
                                    ARM32_AM_PLATFORM_ON_ACTIVATED_RVA, "AppPlatform vtable[27] onActivated") ||
        !arm32_appmain_code_cell_is(platform_vtable[ARM32_AM_PLATFORM_SET_VISIBLE_SLOT], image,
                                    ARM32_AM_PLATFORM_SET_VISIBLE_RVA, "AppPlatform vtable[21] setVisibleFlag")) {
        return FALSE;
    }
    ((PlatformOnActivatedFn)platform_vtable[ARM32_AM_PLATFORM_ON_ACTIVATED_SLOT])(platform);
    ((PlatformSetVisibleFn)platform_vtable[ARM32_AM_PLATFORM_SET_VISIBLE_SLOT])(platform, 1);
    wsprintfA(line, "Minecraft 1.2.8 AppPlatform made visible: state %lu, visible byte %u",
              (unsigned long)*(DWORD *)((BYTE *)platform + 0x3b0), (unsigned)*((BYTE *)platform + 9));
    log_line(line);

    /* (b) size: platform+0x330/+0x334 then game vtable[23] setSize(game, w, h, float 0.0f in s0) and vtable[24]
     *     windowSizeChanged(game, w, h).  Swap-chain buffer size (== client size unless clamped for the feature level;
     *     strategy A of facts appmain.onWindowSizeChanged): AppMain::init already did the same from CoreWindow.Bounds.
     *     The game pointer is re-read: vf1 restart recreates it. */
    *(int *)((BYTE *)platform + ARM_APPMAIN_PLATFORM_SCREEN_WIDTH) = width;
    *(int *)((BYTE *)platform + ARM_APPMAIN_PLATFORM_SCREEN_HEIGHT) = height;
    arm32_gfx_mirror_client_size(width, height);    /* owner+0x94/+0x98 (vf143 copies them back) and renderer+0xe8/+0xec */
    game = *(void **)((BYTE *)game_app_main + ARM32_AM_GAME_OFFSET);
    if (game && !IsBadReadPtr(game, sizeof(void *)) && !IsBadReadPtr(*(void ***)game, ARM32_AM_GAME_VTABLE_PROBE)) {
        game_vtable = *(void ***)game;
        if (arm32_appmain_code_cell_is(game_vtable[ARM32_AM_GAME_SET_SIZE_SLOT], image,
                                       ARM_APPMAIN_GAME_VF23_SETSIZE, "MinecraftGame vtable[23] setSize") &&
            arm32_appmain_code_cell_is(game_vtable[ARM32_AM_GAME_WINDOW_SIZE_CHANGED_SLOT], image,
                                       ARM_APPMAIN_GAME_VF24_WINDOWSIZECHANGED,
                                       "MinecraftGame vtable[24] windowSizeChanged")) {
            ((GameSetSizeFn)game_vtable[ARM32_AM_GAME_SET_SIZE_SLOT])(game, width, height, 0.0f);
            ((GameWindowSizeChangedFn128)game_vtable[ARM32_AM_GAME_WINDOW_SIZE_CHANGED_SLOT])(game, width, height);
            wsprintfA(line, "Minecraft 1.2.8 game resized to %dx%d", width, height);
            log_line(line);
        } else {
            log_line("Minecraft 1.2.8 explicit resize skipped (game vtable differs from the facts); AppMain::init size kept");
        }
    } else {
        log_pointer("Minecraft 1.2.8 MinecraftGame vtable unreadable, resize skipped", game);
    }

    /* (c) input: controller = AppPlatform+0x3b4 -> InputAdapter, +4 -> HIDController; its +0x10 is the InputAdapter that
     *     enqueueInput takes as `this`.  ARM sizeof(InputAdapter) is 0x170 (x86 probed 0x1d0). */
    {
        BYTE *controller = get_hid_controller(platform);
        void *input_handler = (controller && !IsBadReadPtr(controller + ARM32_AM_HID_HANDLER_OFFSET, sizeof(void *)))
            ? *(void **)(controller + ARM32_AM_HID_HANDLER_OFFSET) : NULL;
        input_events_enabled = controller && input_handler &&
            !IsBadReadPtr(input_handler, ARM_APPMAIN_HID_INPUT_HANDLER_PROBE_0X1D0);
        wsprintfA(line, "Minecraft 1.2.8 HID controller %p, input handler %p, input events %s",
                  (void *)controller, input_handler, input_events_enabled ? "enabled" : "DISABLED");
        log_line(line);
    }
    log_line("Minecraft 1.2.8 HWND application activated");
    return TRUE;
}

/* One host frame == the body of AppView::Run's loop (facts appmain.app_main_frame): AppMain::update_frame(AppMain*)
 * -> platform tick, HID update (polls CoreWindow::GetAsyncKeyState), main-thread task queue, MinecraftGame::update
 * (vtable[22]), optional restart_game when AppMain+4 is set, and finally returns MinecraftGame::vf26 = the byte at
 * MinecraftGame+9 ("quit requested", set by MinecraftGame::vf25 requestQuit).  AppView::Run leaves its loop when this is
 * non-zero; the host maps that to WM_CLOSE.
 *
 * Returns nonzero when the game asked to quit.  The caller (main loop in Win32Bootstrap) must stop calling the frame
 * once it sees a nonzero result: leave the loop and return, or just keep pumping messages until WM_QUIT (the WM_CLOSE this
 * function posts, once, ends in DestroyWindow -> PostQuitMessage via window_proc).
 *
 * The game presents itself through the hooked swap chain (AppPlatform_Winrt::vf32 -> Display::present -> win32_present,
 * which bumps game_present_success_count); the host Present below is only the fallback of the x86 host.  The game's
 * pre-frame work in AppView::Run (CoreDispatcher::ProcessEvents) is done by the main loop's drain_dispatcher_queue(). */
static int render_game_target_128(void)
{
    typedef unsigned (*Game128FrameFn)(void *);
    static unsigned frame_count;
    static BOOL quit_posted;
    static BOOL platform_flag_logged;
    Game128FrameFn frame;
    LONG presents_before;
    HRESULT result;
    RECT client;
    unsigned frame_result;
    char line[256];

    /* game_target is not required here: arm32_gfx_bind_frame_targets re-reads the renderer's live view every frame */
    if (!game_app_main || !game_image || !game_renderer || !game_context || !game_swap_chain) {
        static BOOL missing_logged;
        if (!missing_logged) {
            missing_logged = TRUE;
            wsprintfA(line, "Minecraft 1.2.8 frame skipped: AppMain %p image %p renderer %p context %p target %p "
                      "swap chain %p (logged once)", game_app_main, (void *)game_image, game_renderer,
                      (void *)game_context, (void *)game_target, (void *)game_swap_chain);
            log_line(line);
        }
        return 0;
    }
    if (!game_window || !GetClientRect(game_window, &client)) {   /* window already destroyed (WM_CLOSE in flight) */
        static BOOL window_gone_logged;
        if (!window_gone_logged) {
            window_gone_logged = TRUE;
            log_line("Minecraft 1.2.8 frame skipped: game window is gone (logged once)");
        }
        return 0;
    }
    if (arm32_gfx_device_is_lost()) {
        /* the Present hook already logged the reason and posted WM_CLOSE; do not run the game on a dead device */
        static BOOL lost_logged;
        if (!lost_logged) {
            lost_logged = TRUE;
            log_line("Minecraft 1.2.8 frame skipped: the D3D device was lost (window is closing; logged once)");
        }
        return 0;
    }
    /* viewport = swap-chain buffer size (clamped for the feature level), RTV/DSV = renderer+0x118/+0x120 as they are NOW
     * (a resize or a game-side target recreation replaces them); facts appmain.render_dsv_offset */
    if (!arm32_gfx_bind_frame_targets()) {
        static BOOL bind_failed_logged;
        if (!bind_failed_logged) {
            bind_failed_logged = TRUE;
            wsprintfA(line, "Minecraft 1.2.8 frame skipped: the renderer has no render-target view (renderer+0x118 = %p, "
                      "buffer %ux%u; logged once)",
                      *(void **)((BYTE *)game_renderer + ARM_GFX_DEVICE_RENDERER_RTV), arm32_gfx_width, arm32_gfx_height);
            log_line(line);
        }
        return 0;
    }
    frame = (Game128FrameFn)ARM_CODE(game_image, ARM_APPMAIN_APP_MAIN_FRAME);
    presents_before = game_present_success_count;
    frame_result = frame(game_app_main);
    if (frame_result & 0xffu) {
        /* MinecraftGame+9 != 0: the game requested quit (e.g. through requestQuit); AppView::Run would exit here */
        if (!quit_posted) {
            quit_posted = TRUE;
            wsprintfA(line, "Minecraft 1.2.8 frame %u returned quit (MinecraftGame+9 set); posting WM_CLOSE",
                      frame_count);
            log_line(line);
            PostMessageW(game_window, WM_CLOSE, 0, 0);
        }
        ++frame_count;
        return 1;
    }
    if (game_present_success_count == presents_before) {
        result = IDXGISwapChain_Present(game_swap_chain, 0, 0);
        if (!frame_count || FAILED(result))
            log_hresult("Minecraft 1.2.8 host fallback Present", result);
    }
    if (!platform_flag_logged) {
        /* AppView::Run additionally reacts to AppPlatform+8 (set after a ClientInstance was removed): game+0x2f0 ?
         * per-client sub_1314cc(game,1,0) : AppMain::vf1 restart.  The x86 host does not emulate it either; only report. */
        void *platform = current_app_platform();
        if (platform && !IsBadReadPtr((BYTE *)platform + 8, 1) && *((BYTE *)platform + 8)) {
            void *game = *(void **)((BYTE *)game_app_main + ARM32_AM_GAME_OFFSET);
            platform_flag_logged = TRUE;
            wsprintfA(line, "Minecraft 1.2.8 AppPlatform+8 became set after frame %u (AppView::Run would restart/"
                      "refresh clients; not emulated): game+0x2f0=%u AppMain+4=%u", frame_count,
                      (game && !IsBadReadPtr((BYTE *)game + 0x2f0, 1)) ? (unsigned)*((BYTE *)game + 0x2f0) : 0xffu,
                      (unsigned)*((BYTE *)game_app_main + 4));
            log_line(line);
        }
    }
    ++frame_count;
    return 0;
}
#else
static BOOL initialize_game_app_128(BYTE *image)
{
    static BYTE startup_context[0x40];
    void *activation_argument = &win32_core_window;
    void *holder_result;

    ZeroMemory(startup_context, sizeof(startup_context));
    game_app_main = NULL;
    holder_result = call_game_create_app_main_128(
        image + RVA_128_CREATE_APP_MAIN, &game_app_main,
        startup_context, &activation_argument);
    log_pointer("Minecraft 1.2.8 AppMain holder result", holder_result);
    log_pointer("Minecraft 1.2.8 AppMain", game_app_main);
    if (!game_app_main || IsBadReadPtr(game_app_main, 0x24)) {
        log_line("Minecraft 1.2.8 AppMain construction failed");
        return FALSE;
    }
    log_pointer("Minecraft 1.2.8 AppPlatform",
                *(void **)((BYTE *)game_app_main + 0x1c));
    log_pointer("Minecraft 1.2.8 MinecraftGame",
                *(void **)((BYTE *)game_app_main + 0x20));
    return TRUE;
}

static BOOL activate_game_app_128(HWND window)
{
    typedef void (__attribute__((thiscall)) *Game128VisibilityFn)(void *, BYTE);
    Game128VisibilityFn visibility = (Game128VisibilityFn)(
        game_image + RVA_128_APP_VISIBILITY);
    void *platform;
    void *game;
    RECT client;

    if (!game_app_main || !GetClientRect(window, &client)) return FALSE;
    platform = *(void **)((BYTE *)game_app_main + 0x1c);
    game = *(void **)((BYTE *)game_app_main + 0x20);
    if (!platform || !game || IsBadReadPtr(game, sizeof(void *))) return FALSE;

    visibility(game_app_main, TRUE);
    *(int *)((BYTE *)platform + 0x330) = client.right - client.left;
    *(int *)((BYTE *)platform + 0x334) = client.bottom - client.top;
    if (!IsBadReadPtr(*(void ***)game, 0x64)) {
        GameResizeFn resize = (GameResizeFn)(
            *(void ***)game)[0x5c / sizeof(void *)];
        GameWindowSizeChangedFn changed = (GameWindowSizeChangedFn)(
            *(void ***)game)[0x60 / sizeof(void *)];
        if (resize) call_game_resize(
            (void *)resize, game, client.right - client.left,
            client.bottom - client.top, 0);
        if (changed) changed(
            game, client.right - client.left, client.bottom - client.top);
    }
    {
        BYTE *controller = get_hid_controller(platform);
        void *input_handler = controller
            ? *(void **)(controller + 0x10) : NULL;
        input_events_enabled = controller && input_handler &&
            !IsBadReadPtr(input_handler, 0x1d0);
    }
    log_line("Minecraft 1.2.8 HWND application activated");
    return TRUE;
}

static void render_game_target_128(void)
{
    typedef void (__attribute__((thiscall)) *Game128FrameFn)(void *);
    static unsigned frame_count;
    Game128FrameFn frame;
    ID3D11DepthStencilView *depth_target;
    LONG presents_before;
    HRESULT result;
    RECT client;
    D3D11_VIEWPORT viewport;

    if (!game_app_main || !game_renderer || !game_context || !game_target ||
        !game_swap_chain) return;
    GetClientRect(game_window, &client);
    ZeroMemory(&viewport, sizeof(viewport));
    viewport.Width = (float)(client.right - client.left);
    viewport.Height = (float)(client.bottom - client.top);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    depth_target = *(ID3D11DepthStencilView **)(
        (BYTE *)game_renderer + 0x120);
    ID3D11DeviceContext_RSSetViewports(game_context, 1, &viewport);
    ID3D11DeviceContext_OMSetRenderTargets(
        game_context, 1, &game_target, depth_target);
    frame = (Game128FrameFn)(game_image + RVA_128_APP_MAIN_FRAME);
    presents_before = game_present_success_count;
    frame(game_app_main);
    if (game_present_success_count == presents_before) {
        result = IDXGISwapChain_Present(game_swap_chain, 0, 0);
        if (!frame_count || FAILED(result))
            log_hresult("Minecraft 1.2.8 host fallback Present", result);
    }
    ++frame_count;
}

#endif /* WIN32CRAFT_ARM32 */

static BOOL activate_game_app_115(HWND window)
{
    Game115AppResizeFn resize_app =
        (Game115AppResizeFn)(game_image + RVA_115_APP_MAIN_RESIZE);
    void *platform;
    RECT client;

    if (!game_app_main ||
        IsBadReadPtr((BYTE *)game_app_main + 0x10, 1) ||
        !GetClientRect(window, &client)) {
        return FALSE;
    }
    platform = *(void **)((BYTE *)game_app_main + 4);
    if (!platform) return FALSE;

    log_pointer("Minecraft 1.1.5 AppMain resize entry", (const void *)resize_app);
    if (IsBadReadPtr((const void *)resize_app, 3) ||
        ((const BYTE *)resize_app)[0] != 0x55 ||
        ((const BYTE *)resize_app)[1] != 0x8b ||
        ((const BYTE *)resize_app)[2] != 0xec) {
        log_line("Minecraft 1.1.5 AppMain resize entry has invalid prologue");
        return FALSE;
    }

    /*
     * MainPage enables its CompositionTarget callback only after the
     * activation/visibility handlers have completed.  The HWND frame loop
     * replaces that callback, so publish the same ready state directly.
     */
    *((BYTE *)platform + 8) = TRUE;
    *((BYTE *)game_app_main + 0x10) = TRUE;
    log_line("Minecraft 1.1.5 initial AppMain resize begin");
    resize_app(
        game_app_main,
        &xaml_size_source,
        NULL);
    log_line("Minecraft 1.1.5 initial AppMain resize returned");
    {
        BYTE *controller = get_hid_controller(platform);
        void *input_handler = controller
            ? *(void **)(controller + 0x10) : NULL;
        input_events_enabled =
            controller && input_handler &&
            !IsBadReadPtr(input_handler, 0x1d0);
        log_line(input_events_enabled
            ? "Win32 input injection enabled for Minecraft 1.1.5 ABI"
            : "Minecraft 1.1.5 HID/InputHandler is unavailable");
    }
    log_line("Minecraft 1.1.5 HWND frame callback enabled");
    return TRUE;
}

static void render_game_target_115(void)
{
    static unsigned frame_count;
    Game115PlatformFrameFn platform_frame;
    Game115AppFrameFn app_frame;
    ID3D11DepthStencilView *depth_target;
    void *platform;
    LONG presents_before;
    HRESULT result;
    RECT client;
    D3D11_VIEWPORT viewport;

    if (!game_app_main || !game_renderer || !game_context ||
        !game_target || !game_swap_chain ||
        !*((BYTE *)game_app_main + 0x10)) {
        return;
    }
    platform = *(void **)((BYTE *)game_app_main + 4);
    if (!platform) return;

    GetClientRect(game_window, &client);
    ZeroMemory(&viewport, sizeof(viewport));
    viewport.Width = (float)(client.right - client.left);
    viewport.Height = (float)(client.bottom - client.top);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    depth_target = *(ID3D11DepthStencilView **)(
        (BYTE *)game_renderer + 0x11c);
    ID3D11DeviceContext_RSSetViewports(game_context, 1, &viewport);
    ID3D11DeviceContext_OMSetRenderTargets(
        game_context, 1, &game_target, depth_target);

    platform_frame =
        (Game115PlatformFrameFn)(game_image + RVA_115_PLATFORM_FRAME);
    app_frame =
        (Game115AppFrameFn)(game_image + RVA_115_APP_MAIN_FRAME);
    presents_before = game_present_success_count;
    if (!frame_count) {
        log_pointer("Minecraft 1.1.5 platform frame entry", (const void *)platform_frame);
        log_pointer("Minecraft 1.1.5 AppMain frame entry", (const void *)app_frame);
        if (IsBadReadPtr((const void *)platform_frame, 3) ||
            IsBadReadPtr((const void *)app_frame, 3) ||
            ((const BYTE *)platform_frame)[0] != 0x55 ||
            ((const BYTE *)app_frame)[0] != 0x55) {
            log_line("Minecraft 1.1.5 frame entry validation failed");
            return;
        }
        log_line("Minecraft 1.1.5 first frame begin");
    }
    platform_frame(platform);
    app_frame(game_app_main);
    if (!frame_count) log_line("Minecraft 1.1.5 first frame returned");
    if (game_present_success_count == presents_before) {
        /*
         * On Windows 7 the UWP CompositionTarget path does not reach the
         * original swap-chain Present, so the HWND host owns presentation.
         * win32_present enforces the same synchronized presentation policy as
         * the game-owned path.
         */
        result = IDXGISwapChain_Present(game_swap_chain, 1, 0);
        if (!frame_count) {
            log_line("Minecraft 1.1.5 synchronized fallback Present enabled");
        }
        if (!frame_count || FAILED(result)) {
            log_hresult("Minecraft 1.1.5 host fallback Present", result);
        }
    }
    ++frame_count;
}

/*
 * Wine's C++/CX reference-tracker helpers used while the social adapter is
 * constructed do not preserve EBX/EDI.  The MSVC caller keeps the adapter in
 * those non-volatile registers and subsequently dereferences the corrupted
 * values.  Recover both from the constructor's saved object pointer, perform
 * the original AddRef and return after the replaced instruction sequence.
 */
#ifdef WIN32CRAFT_ARM32
/* x86-only register recovery thunk of the 1.1.5 Wine patch; patch_115_xaml_self_references (never run on ARM) still
 * takes its address, so keep a harmless symbol. */
static void win32_115_restore_adapter_references(void)
{
}
#else
__attribute__((naked)) static void win32_115_restore_adapter_references(void)
{
    __asm__ __volatile__(
        "popl %eax\n\t"
        "addl $8, %esp\n\t"
        "movl -0x10(%ebp), %edi\n\t"
        "movl 0x10(%edi), %ebx\n\t"
        "movl (%edi), %ecx\n\t"
        "pushl %eax\n\t"
        "pushl %edi\n\t"
        "call *0x4(%ecx)\n\t"
        "ret\n\t");
}
#endif

static BOOL patch_115_xaml_self_references(BYTE *image)
{
    BYTE *site = image + 0x007ab00a;
    BYTE *optional_interface_site = image + 0x007b3069;
    BYTE *xbox_work_item_site = image + 0x007b34c0;
    BYTE *xbox_wait_site = image + 0x006cdc9b;
    static const BYTE expected[9] = {
        0x8b, 0x07, 0x83, 0xc4, 0x08, 0x57, 0xff, 0x50, 0x04
    };
    static const BYTE optional_interface_expected[10] = {
        0x8b, 0x4d, 0x08, 0xc7, 0x45, 0xec, 0x00, 0x00, 0x00, 0x00
    };
    static const BYTE optional_interface_replacement[10] = {
        0x8b, 0x4d, 0x08, /* mov ecx,[ebp+8] */
        0x31, 0xf6,       /* xor esi,esi */
        0x85, 0xc9,       /* test ecx,ecx */
        0x74, 0x42,       /* jz return-null epilogue */
        0x90
    };
    static const BYTE xbox_work_item_expected[8] = {
        0x55, 0x8b, 0xec, 0x51, 0x8b, 0x4d, 0x08, 0x8d
    };
    static const BYTE xbox_work_item_replacement[8] = {
        0x31, 0xc0,       /* xor eax,eax (S_OK) */
        0xc2, 0x08, 0x00, /* ret 8 */
        0x90, 0x90, 0x90
    };
    static const BYTE xbox_wait_expected[5] = {
        0x0f, 0x1f, 0x44, 0x00, 0x00
    };
    static const BYTE xbox_wait_replacement[5] = {
        0xe9, 0x0a, 0x00, 0x00, 0x00
    };
    BYTE replacement[9] = {
        0xe8, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90
    };
    INT_PTR displacement;
    DWORD old_protection;
    DWORD ignored;

    if (memcmp(site, expected, sizeof(expected)) != 0) {
        log_line("Minecraft 1.1.5 adapter register patch signature mismatch");
        return FALSE;
    }
    displacement =
        (BYTE *)win32_115_restore_adapter_references - (site + 5);
    *(LONG *)(replacement + 1) = (LONG)displacement;
    if (!VirtualProtect(site, sizeof(replacement),
                        PAGE_EXECUTE_READWRITE, &old_protection)) {
        log_line("Minecraft 1.1.5 adapter register patch protection failed");
        return FALSE;
    }
    CopyMemory(site, replacement, sizeof(replacement));
    VirtualProtect(site, sizeof(replacement), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(replacement));
    if (memcmp(optional_interface_site, optional_interface_expected,
               sizeof(optional_interface_expected)) != 0 ||
        !VirtualProtect(optional_interface_site,
                        sizeof(optional_interface_replacement),
                        PAGE_EXECUTE_READWRITE, &old_protection)) {
        log_line("Minecraft 1.1.5 optional Xbox interface patch failed");
        return FALSE;
    }
    CopyMemory(optional_interface_site, optional_interface_replacement,
               sizeof(optional_interface_replacement));
    VirtualProtect(optional_interface_site,
                   sizeof(optional_interface_replacement),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), optional_interface_site,
                          sizeof(optional_interface_replacement));
    if (memcmp(xbox_work_item_site, xbox_work_item_expected,
               sizeof(xbox_work_item_expected)) != 0 ||
        !VirtualProtect(xbox_work_item_site,
                        sizeof(xbox_work_item_replacement),
                        PAGE_EXECUTE_READWRITE, &old_protection)) {
        log_line("Minecraft 1.1.5 Xbox work-item no-op patch failed");
        return FALSE;
    }
    CopyMemory(xbox_work_item_site, xbox_work_item_replacement,
               sizeof(xbox_work_item_replacement));
    VirtualProtect(xbox_work_item_site,
                   sizeof(xbox_work_item_replacement),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), xbox_work_item_site,
                          sizeof(xbox_work_item_replacement));
    if (memcmp(xbox_wait_site, xbox_wait_expected,
               sizeof(xbox_wait_expected)) != 0 ||
        !VirtualProtect(xbox_wait_site, sizeof(xbox_wait_replacement),
                        PAGE_EXECUTE_READWRITE, &old_protection)) {
        log_line("Minecraft 1.1.5 Xbox initialization wait patch failed");
        return FALSE;
    }
    CopyMemory(xbox_wait_site, xbox_wait_replacement,
               sizeof(xbox_wait_replacement));
    VirtualProtect(xbox_wait_site, sizeof(xbox_wait_replacement),
                   old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), xbox_wait_site,
                          sizeof(xbox_wait_replacement));
    log_line("Minecraft 1.1.5 C++/CX adapter register fix installed");
    return TRUE;
}


typedef struct RuntimeBytePatch {
    SIZE_T rva;
    const BYTE *expected;
    const BYTE *replacement;
    SIZE_T size;
    const char *failure;
} RuntimeBytePatch;

#ifdef WIN32CRAFT_ARM32
/* ARM32: only data bytes are ever patched through this table (facts platform_fixes.protection_note), so the pages are
 * flipped to PAGE_READWRITE, never executable (Windows RT may refuse RWX).  A mismatch logs what the bytes really were. */
static BOOL apply_runtime_patches(BYTE *image,
                                  const RuntimeBytePatch *patches,
                                  SIZE_T count)
{
    SIZE_T index;

    for (index = 0; index < count; ++index) {
        BYTE *site = image + patches[index].rva;
        DWORD old_protection;
        DWORD ignored;

        if (!arm32_check_bytes(site, patches[index].expected, patches[index].size, patches[index].failure) ||
            !VirtualProtect(site, patches[index].size, PAGE_READWRITE, &old_protection)) {
            log_line(patches[index].failure);
            return FALSE;
        }
        CopyMemory(site, patches[index].replacement, patches[index].size);
        VirtualProtect(site, patches[index].size, old_protection, &ignored);
        FlushInstructionCache(GetCurrentProcess(), site, patches[index].size);
    }
    return TRUE;
}
#else
static BOOL apply_runtime_patches(BYTE *image,
                                  const RuntimeBytePatch *patches,
                                  SIZE_T count)
{
    SIZE_T index;

    for (index = 0; index < count; ++index) {
        BYTE *site = image + patches[index].rva;
        DWORD old_protection;
        DWORD ignored;

        if (memcmp(site, patches[index].expected, patches[index].size) != 0 ||
            !VirtualProtect(site, patches[index].size,
                            PAGE_EXECUTE_READWRITE, &old_protection)) {
            log_line(patches[index].failure);
            return FALSE;
        }
        CopyMemory(site, patches[index].replacement, patches[index].size);
        VirtualProtect(site, patches[index].size, old_protection, &ignored);
        FlushInstructionCache(GetCurrentProcess(), site, patches[index].size);
    }
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 */

#ifdef WIN32CRAFT_ARM32
/* Xbox sign-in neutralisation (facts platform_fixes.xbox_signin_event_str / xbox_xbl_signin_event_str and
 * chat_xbox.string_button_signin / string_button_xbl_signin).  Like x86 1.2.8, the first character of the two shared
 * event-name literals is changed from 'b' (0x62) to '!' (0x21): the five ScreenController registrations then register
 * "!utton.signin" / "!utton.xbl_signin" and JSON button presses never resolve an action.  The literals live in read-only
 * .rdata (one copy each, lengths 13/17 are hard-coded at the registration sites), so arm32_patch_byte flips the page to
 * PAGE_READWRITE only.  Must run before the game constructs its screen controllers.  Idempotent: an already renamed
 * string counts as success. */
static BOOL arm32_disable_event_string(BYTE *image, SIZE_T rva, const BYTE *expected, SIZE_T expected_size,
                                       const char *what)
{
    BYTE *address = image + rva;
    char line[160];

    if (IsBadReadPtr(address, expected_size)) {
        wsprintfA(line, "ARM32 %s: string at %p is unreadable", what, (void *)address);
        log_line(line);
        return FALSE;
    }
    if (address[0] == 0x21 && memcmp(address + 1, expected + 1, expected_size - 1) == 0) {
        wsprintfA(line, "ARM32 %s: string at %p already disabled", what, (void *)address);
        log_line(line);
        return TRUE;
    }
    if (!arm32_check_bytes(address, expected, expected_size, what)) return FALSE;
    return arm32_patch_byte(address, 0x62, 0x21, what);
}

static BOOL patch_xbox_actions_noop(BYTE *image)
{
    BOOL signin;
    BOOL xbl_signin;

    if (!image) return FALSE;
    signin = arm32_disable_event_string(image, ARM_PLATFORM_FIXES_XBOX_SIGNIN_EVENT_STR,
                                        ARM_PLATFORM_FIXES_XBOX_SIGNIN_EVENT_STR_EXPECTED,
                                        sizeof(ARM_PLATFORM_FIXES_XBOX_SIGNIN_EVENT_STR_EXPECTED),
                                        "Xbox event string 'button.signin'");
    xbl_signin = arm32_disable_event_string(image, ARM_PLATFORM_FIXES_XBOX_XBL_SIGNIN_EVENT_STR,
                                            ARM_PLATFORM_FIXES_XBOX_XBL_SIGNIN_EVENT_STR_EXPECTED,
                                            sizeof(ARM_PLATFORM_FIXES_XBOX_XBL_SIGNIN_EVENT_STR_EXPECTED),
                                            "Xbox event string 'button.xbl_signin'");
    if (!signin || !xbl_signin) {
        log_line("Minecraft 1.2.8 ARM32 Xbox sign-in events NOT fully disabled (see lines above)");
        return FALSE;
    }
    log_line("Minecraft 1.2.8 ARM32 Xbox sign-in events disabled");
    return TRUE;
}
#else
static BOOL patch_xbox_actions_noop(BYTE *image)
{
    /*
     * Keep the stock Xbox buttons and their JSON visibility rules intact, but
     * replace the action-thunk used by each Xbox control with the normal
     * ScreenController callback result (8).  This is safer than allowing the
     * callback to enter WebAuthenticationCoreManager/Xbox TCUI on an HWND
     * process, where the UWP runtime class is absent and C++/CX raises
     * Platform::ClassNotRegisteredException.
     *
     * The wrappers below are the vtable invoke slots registered for:
     *   0.15.10: button.menu_xbox_signin and all three button.xbl_signin paths
     *   1.1.5 : button.save_to_xbl, StoreScreen button.signin, and all
     *             three button.xbl_signin paths
     * Achievements, Invite Player, profile-card TCUI and the 1.1.5 background
     * Xbox work item remain covered by their existing import/internal no-ops.
     */
    static const BYTE return_handled[8] = {
        0xb8, 0x08, 0x00, 0x00, 0x00, /* mov eax,8 */
        0xc2, 0x04, 0x00              /* ret 4 */
    };
    static const BYTE event_name_b = 'b';
    static const BYTE event_name_disabled = '!';
    static const BYTE expected_01510_menu_signin[8] = {
        0x55, 0x8b, 0xec, 0x51, 0x83, 0xec, 0x28, 0x8b
    };
    static const BYTE expected_01510_signin_a[8] = {
        0x55, 0x8b, 0xec, 0x51, 0x83, 0xec, 0x28, 0x8b
    };
    static const BYTE expected_01510_signin_b[8] = {
        0x83, 0xc1, 0x04, 0xe9, 0x48, 0xfb, 0xff, 0xff
    };
    static const BYTE expected_01510_signin_c[8] = {
        0x83, 0xc1, 0x08, 0xe9, 0xd8, 0xf9, 0xff, 0xff
    };
    static const BYTE expected_115_save_to_xbl[8] = {
        0x83, 0xc1, 0x04, 0xe9, 0x18, 0xf1, 0xff, 0xff
    };
    static const BYTE expected_115_store_signin[8] = {
        0x83, 0xc1, 0x04, 0xe9, 0xb8, 0xe7, 0xff, 0xff
    };
    static const BYTE expected_115_signin_a[8] = {
        0x83, 0xc1, 0x04, 0xe9, 0x08, 0xee, 0xff, 0xff
    };
    static const BYTE expected_115_signin_b[8] = {
        0x55, 0x8b, 0xec, 0x8b, 0x41, 0x04, 0x83, 0xc1
    };
    static const BYTE expected_115_signin_c[8] = {
        0x83, 0xc1, 0x08, 0xe9, 0x38, 0xf8, 0xff, 0xff
    };
    static const RuntimeBytePatch patches_01510[] = {
        { 0x000f3520, expected_01510_menu_signin, return_handled, 8,
          "Minecraft 0.15.10 menu Xbox sign-in no-op patch failed" },
        { 0x00127a70, expected_01510_signin_a, return_handled, 8,
          "Minecraft 0.15.10 Xbox sign-in callback A no-op patch failed" },
        { 0x001316c0, expected_01510_signin_b, return_handled, 8,
          "Minecraft 0.15.10 Xbox sign-in callback B no-op patch failed" },
        { 0x001331c0, expected_01510_signin_c, return_handled, 8,
          "Minecraft 0.15.10 Xbox sign-in callback C no-op patch failed" }
    };
    static const RuntimeBytePatch patches_115[] = {
        { 0x001e1c70, expected_115_save_to_xbl, return_handled, 8,
          "Minecraft 1.1.5 save-to-Xbox callback no-op patch failed" },
        { 0x002fca30, expected_115_store_signin, return_handled, 8,
          "Minecraft 1.1.5 Store Sign In callback no-op patch failed" },
        { 0x002ea600, expected_115_signin_a, return_handled, 8,
          "Minecraft 1.1.5 Xbox sign-in callback A no-op patch failed" },
        { 0x0033f090, expected_115_signin_b, return_handled, 8,
          "Minecraft 1.1.5 Xbox sign-in callback B no-op patch failed" },
        { 0x00341970, expected_115_signin_c, return_handled, 8,
          "Minecraft 1.1.5 Xbox sign-in callback C no-op patch failed" }
    };
    /* 1.2.8 shares each event-name string between all of its controller
     * registrations.  Rename only the two Xbox sign-in keys so JSON button
     * presses cannot resolve an action at all; this avoids touching the
     * adjacent visibility/property lambdas that use different ABIs. */
    static const RuntimeBytePatch patches_128[] = {
        { 0x012b64b4, &event_name_b, &event_name_disabled, 1,
          "Minecraft 1.2.8 button.signin no-op patch failed" },
        { 0x012bfbcc, &event_name_b, &event_name_disabled, 1,
          "Minecraft 1.2.8 button.xbl_signin no-op patch failed" }
    };
    const RuntimeBytePatch *patches = host_is_128
        ? patches_128 : (host_is_115 ? patches_115 : patches_01510);
    SIZE_T count = host_is_128
        ? ARRAYSIZE(patches_128)
        : (host_is_115 ? ARRAYSIZE(patches_115)
                       : ARRAYSIZE(patches_01510));

    if (!apply_runtime_patches(image, patches, count)) return FALSE;
    log_line(host_is_128
        ? "Minecraft 1.2.8 Xbox sign-in events disabled"
        : (host_is_115
            ? "Minecraft 1.1.5 Xbox buttons retained with no-op actions"
            : "Minecraft 0.15.10 Xbox buttons retained with no-op actions"));
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 */

#ifdef WIN32CRAFT_ARM32
/* Not needed on ARM: the 1.2.8 ARM exe has no XAML TextBox classes at all (facts platform_fixes.chat_textbox_compat,
 * chat_xbox.patch_chat_textbox_compat); the x86 patch was 1.1.5-only anyway. */
static BOOL patch_chat_textbox_compat(BYTE *image)
{
    (void)image;
    return TRUE;
}
#else
static BOOL patch_chat_textbox_compat(BYTE *image)
{
    static const BYTE expected_set_text[20] = {
        0x8b, 0x06, 0x8b, 0x7d, 0xd0, 0x57, 0x56, 0xff,
        0x50, 0x1c, 0x85, 0xc0, 0x79, 0x06, 0x50, 0xe8,
        0x6b, 0xda, 0x8d, 0xff
    };
    static const BYTE replace_set_text[20] = {
        0x8b, 0x7d, 0xd0, 0x85, 0xf6, 0x74, 0x1a, 0x8b,
        0x06, 0x57, 0x56, 0xff, 0x50, 0x1c, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    static const BYTE expected_set_selection[28] = {
        0x89, 0x75, 0xcc, 0x57, 0xc6, 0x45, 0xfc, 0x02,
        0x8b, 0x06, 0x57, 0x56, 0xff, 0x90, 0x90, 0x00,
        0x00, 0x00, 0x85, 0xc0, 0x79, 0x06, 0x50, 0xe8,
        0xa5, 0xd9, 0x8d, 0xff
    };
    static const BYTE replace_set_selection[28] = {
        0x89, 0x75, 0xcc, 0x85, 0xf6, 0x74, 0x1f, 0x57,
        0xc6, 0x45, 0xfc, 0x02, 0x8b, 0x06, 0x57, 0x56,
        0xff, 0x90, 0x90, 0x00, 0x00, 0x00, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90
    };
    static const BYTE expected_set_caret[24] = {
        0x89, 0x75, 0xcc, 0xc6, 0x45, 0xfc, 0x03, 0x8b,
        0x06, 0x57, 0x56, 0xff, 0x50, 0x34, 0x85, 0xc0,
        0x79, 0x06, 0x50, 0xe8, 0x5c, 0xd9, 0x8d, 0xff
    };
    static const BYTE replace_set_caret[24] = {
        0x89, 0x75, 0xcc, 0x85, 0xf6, 0x74, 0x1b, 0xc6,
        0x45, 0xfc, 0x03, 0x8b, 0x06, 0x57, 0x56, 0xff,
        0x50, 0x34, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    static const RuntimeBytePatch patches_115[] = {
        {
            0x007b1021,
            expected_set_text,
            replace_set_text,
            sizeof(expected_set_text),
            "Minecraft 1.1.5 HWND text-box SetText guard patch failed"
        },
        {
            0x007b10df,
            expected_set_selection,
            replace_set_selection,
            sizeof(expected_set_selection),
            "Minecraft 1.1.5 HWND text-box selection guard patch failed"
        },
        {
            0x007b112c,
            expected_set_caret,
            replace_set_caret,
            sizeof(expected_set_caret),
            "Minecraft 1.1.5 HWND text-box caret guard patch failed"
        }
    };

    if (!host_is_115) return TRUE;
    if (!apply_runtime_patches(image, patches_115,
                               ARRAYSIZE(patches_115))) {
        return FALSE;
    }
    log_line("Minecraft 1.1.5 HWND chat text-box null-interface guards installed");
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 */

static BOOL activate_game_app(HWND window)
{
    void *platform;
    void *game;
    BYTE *app_platform;
    GamePlatformResumeFn resume_platform;
    RECT client;
    char line[96];

    if (!game_app_main) return FALSE;
    platform = *(void **)((BYTE *)game_app_main + 4);
    game = *(void **)((BYTE *)game_app_main + 8);
    if (!platform || !game || IsBadReadPtr(game, sizeof(void *))) {
        log_line("cannot activate AppMain: platform/game unavailable");
        return FALSE;
    }

    app_platform = *(BYTE **)(game_image + RVA_APP_PLATFORM_GLOBAL);
    log_pointer("AppPlatform", app_platform);

    /*
     * MinecraftClient byte +9 is the quit request. Its listener slot 0x58
     * sets that byte, and AppMain::Update calls Application::Exit when it is
     * set. Do not synthesize that listener notification on desktop startup.
     * AppMain byte +0x10 is the actual CompositionTarget::Rendering enable
     * flag; the HWND frame loop substitutes for that XAML event below.
     */
    wsprintfA(line, "MinecraftClient quit flag at startup: %u",
              (unsigned)*((BYTE *)game + 9));
    log_line(line);

    GetClientRect(window, &client);
    if (!IsBadReadPtr(*(void ***)game, 0x54)) {
        GameResizeFn resize =
            (GameResizeFn)(*(void ***)game)[0x50 / sizeof(void *)];
        if (resize) {
            call_game_resize((void *)resize, game,
                             client.right - client.left,
                             client.bottom - client.top, 0);
            log_line("MinecraftClient Win32 size initialized");
        }
    }
    *(int *)((BYTE *)platform + 0x25c) = client.right - client.left;
    *(int *)((BYTE *)platform + 0x260) = client.bottom - client.top;

    /*
     * MainPage's initial VisibilityChanged/Activated callbacks resume the
     * WinRT platform before CompositionTarget starts producing frames.
     * AppMain construction alone leaves AppPlatform_Winrt in its suspended
     * state, so materials are parsed but their queued shader work is never
     * submitted to D3D11.
     */
    resume_platform = !IsBadReadPtr(*(void ***)platform, 0x4c)
        ? (GamePlatformResumeFn)(*(void ***)platform)[0x48 / sizeof(void *)]
        : NULL;
    if (resume_platform) {
        wsprintfA(line,
            "AppPlatform lifecycle before resume: visible=%u "
            "suspend_134=%lu state_3c4=%lu",
            (unsigned)*((BYTE *)platform + 8),
            (unsigned long)*(DWORD *)((BYTE *)platform + 0x134),
            (unsigned long)*(DWORD *)((BYTE *)platform + 0x3c4));
        log_line(line);
        resume_platform(platform);
        *((BYTE *)platform + 8) = TRUE;
        wsprintfA(line,
            "AppPlatform lifecycle after resume: visible=%u "
            "suspend_134=%lu state_3c4=%lu",
            (unsigned)*((BYTE *)platform + 8),
            (unsigned long)*(DWORD *)((BYTE *)platform + 0x134),
            (unsigned long)*(DWORD *)((BYTE *)platform + 0x3c4));
        log_line(line);
    } else {
        log_line("AppPlatform lifecycle resume method unavailable");
    }

    /*
     * MainPage activation also notifies every AppPlatformListener.  The
     * MinecraftClient is the listener published by AppMain; slot +0x14 is
     * its activation/start callback in 0.15.10.
     */
    if (!IsBadReadPtr(*(void ***)game, 0x18)) {
        GameLifecycleFn start_listener =
            (GameLifecycleFn)(*(void ***)game)[0x14 / sizeof(void *)];
        log_pointer("AppMain listener", game);
        log_pointer("AppMain listener start", start_listener);
        if (start_listener) {
            start_listener(game);
            log_line("AppMain MinecraftClient listener started");
        }
    }

    /*
     * Listener startup may replace the initial screen stack.  Repeat only
     * MinecraftClient's platform-independent size notification here so the
     * newly active ScreenViews receive the HWND dimensions.  Do not invoke
     * Renderer::setWindowMetrics: that routine owns a SwapChainPanel and its
     * WinRT resize path is invalid for our HWND swap chain.
     */
    if (!IsBadReadPtr(*(void ***)game, 0x54)) {
        GameResizeFn resize =
            (GameResizeFn)(*(void ***)game)[0x50 / sizeof(void *)];
        GameWindowSizeChangedFn window_size_changed =
            (GameWindowSizeChangedFn)(
                *(void ***)game)[0x54 / sizeof(void *)];
        if (resize) {
            call_game_resize((void *)resize, game,
                             client.right - client.left,
                             client.bottom - client.top, 0);
            log_line("MinecraftClient post-activation size synchronized");
        }
        /*
         * MinecraftClient::setSize (+0x50) updates the screen/UI metrics,
         * but 0.15.10 split the renderer notification into the adjacent
         * +0x54 callback.  The native CoreWindow size path invokes it to
         * rebuild GameRenderer's render-pass vector.  Without this second
         * notification the vector at GameRenderer+0x14 stays empty and no
         * ScreenView draw callback can ever be reached.
         */
        if (window_size_changed) {
            window_size_changed(
                game, client.right - client.left,
                client.bottom - client.top);
            log_line("MinecraftClient renderer pass list synchronized");
        }
    }

    *(BYTE *)((BYTE *)game_app_main + APPMAIN_FRAME_ENABLED_OFFSET) = TRUE;
    log_line("AppMain HWND frame callback enabled");
    {
        BYTE *controller = get_hid_controller(platform);
        void *input_handler = controller
            ? *(void **)(controller + 0x10) : NULL;
        input_events_enabled =
            controller && input_handler &&
            !IsBadReadPtr(input_handler, 0x2ac);
        log_line(input_events_enabled
            ? "Win32 input injection enabled for 0.15.10 ABI"
            : "Win32 input injection unavailable: HID/InputHandler missing");
    }
    return TRUE;
}

static void render_game_target(void)
{
    static float phase;
    static unsigned frame_count;
    float color[4] = {0.055f, 0.085f, 0.12f, 1.0f};
    GameUpdateRenderFn update_render;

    if (!game_context || !game_target || !game_swap_chain) {
        return;
    }
    if (game_app_main && game_renderer &&
        *(BYTE *)(
            (BYTE *)game_app_main + APPMAIN_FRAME_ENABLED_OFFSET)) {
        BYTE *game = *(BYTE **)((BYTE *)game_app_main + 8);
        ID3D11DepthStencilView *depth_target =
            *(ID3D11DepthStencilView **)((BYTE *)game_renderer +
                                         RENDERER_DSV_OFFSET);
        RECT client;
        D3D11_VIEWPORT viewport;

        /*
         * The XAML resize path normally binds the DeviceResources targets
         * and publishes its viewport before CompositionTarget::Rendering.
         * An HWND swap chain cannot execute that SwapChainPanel path, so do
         * the platform-independent D3D11 part here using the views created
         * by Renderer::createWindowSizeDependentResources.
         */
        GetClientRect(game_window, &client);
        ZeroMemory(&viewport, sizeof(viewport));
        viewport.Width = (float)(client.right - client.left);
        viewport.Height = (float)(client.bottom - client.top);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        ID3D11DeviceContext_RSSetViewports(
            game_context, 1, &viewport);
        ID3D11DeviceContext_OMSetRenderTargets(
            game_context, 1, &game_target, depth_target);

        /*
         * MinecraftClient +0x230 is a render-suppression flag in 0.15.10.
         * Unlike the superficially similar +0x1d4 field in 0.13.2, setting
         * it skips the main renderer.  Keep the normal rendering path open.
         */
        *(BYTE *)(game + 0x230) = FALSE;
        LONG presents_before;
        LONG presents_after;
        HRESULT fallback_result;
        static unsigned fallback_present_count;
        update_render =
            (GameUpdateRenderFn)(game_image + RVA_UPDATE_RENDER);
        /*
         * The UWP build has an independent CompositionTarget callback that
         * drains a XAML task queue.  Calling that callback from the HWND loop
         * corrupts the UI material commands.  AppMain::Update owns the real
         * game tick and D3D render path and is the only per-frame callback
         * required by this Win32 host.
         *
         * On Windows 7 the UWP frame path can render into the HWND backbuffer
         * without reaching its original CompositionTarget-driven Present.
         * Observe the real swap-chain vtable: only present manually when the
         * game did not successfully present during this update.
         */
        presents_before = game_present_success_count;
        if (!frame_count) log_line("first AppMain update/render begin");
        update_render(game_app_main);
        if (!frame_count) log_line("first AppMain update/render returned");
        presents_after = game_present_success_count;
        if (presents_after == presents_before) {
            /*
             * Present(0) made the Windows 7 fallback loop run essentially
             * uncapped, causing high CPU use and uneven input.  D3D11 on
             * Windows 7 supports the normal one-vblank interval.
             */
            fallback_result = IDXGISwapChain_Present(
                game_swap_chain, 1, 0);
            ++fallback_present_count;
            if (fallback_present_count <= 10 ||
                fallback_present_count == 60 || FAILED(fallback_result)) {
                char line[160];
                wsprintfA(line,
                    "host fallback Present %u: HRESULT 0x%08lX",
                    fallback_present_count, (unsigned long)fallback_result);
                log_line(line);
            }
        }
        ++frame_count;
        return;
    }
    phase += 0.0025f;
    if (phase > 0.06f) {
        phase = 0.0f;
    }
    color[2] += phase;
    ID3D11DeviceContext_OMSetRenderTargets(
        game_context, 1, &game_target, NULL);
    ID3D11DeviceContext_ClearRenderTargetView(
        game_context, game_target, color);
    IDXGISwapChain_Present(game_swap_chain, 1, 0);
}


#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 input path (unit input_hid; facts input.*).  1.2.8 only.
 *
 *   AppMain+0x1c -> AppPlatform_Winrt, +0x3b4 -> InputAdapter (0x170 bytes), adapter+4 -> HIDController,
 *   controller+0x10 -> adapter again (the `this` of EnqueueInput), controller +0x28 int pointer-mode command,
 *   +0xa0 byte "actual", +0xc9..+0x1c8 held-key table, +0x1f0 text sequence byte.
 * ====================================================================================================== */
typedef void (*Arm32EnqueueInputFn)(void *adapter, void **item_slot);
typedef unsigned (*Arm32MapVirtualKeyFn)(unsigned vk_minus_8);

static BYTE *get_hid_controller(void *platform)
{
    BYTE *adapter;
    if (!platform ||
        IsBadReadPtr((BYTE *)platform + ARM_INPUT_PLATFORM_ADAPTER_OFFSET, sizeof(void *))) {
        return NULL;
    }
    adapter = *(BYTE **)((BYTE *)platform + ARM_INPUT_PLATFORM_ADAPTER_OFFSET);
    if (!adapter || IsBadReadPtr(adapter + ARM_INPUT_ADAPTER_TO_CONTROLLER, sizeof(void *))) {
        return NULL;
    }
    return *(BYTE **)(adapter + ARM_INPUT_ADAPTER_TO_CONTROLLER);
}

/* In the three std::function<void()> lambda bodies `this` is the _Func_impl object: +0 vtable, +4 the captured
 * InputAdapter*, adapter+4 the HIDController (facts input.mouse_mode_this_object). */
static BYTE *get_mouse_mode_controller(void *lambda_self)
{
    BYTE *adapter;
    if (!lambda_self || IsBadReadPtr((BYTE *)lambda_self + 4, sizeof(void *))) return NULL;
    adapter = *(BYTE **)((BYTE *)lambda_self + 4);
    if (!adapter || IsBadReadPtr(adapter + ARM_INPUT_ADAPTER_TO_CONTROLLER, sizeof(void *))) return NULL;
    return *(BYTE **)(adapter + ARM_INPUT_ADAPTER_TO_CONTROLLER);
}

static SIZE_T mouse_actual_offset(void)
{
    return ARM_INPUT_MOUSE_ACTUAL_OFFSET;
}

static SIZE_T mouse_command_offset(void)
{
    return ARM_INPUT_MOUSE_COMMAND_OFFSET;
}

/* HIDController plus the EnqueueInput `this`, both probed.  The 0x170 size is the real allocation of the ARM adapter
 * (the x86 0x1d0 probe would be larger than the object).  Logs the object graph once and any failure reason a few
 * times, because the log is the only feedback from the device. */
static BYTE *arm32_input_target(void **input_handler_out)
{
    static unsigned logged_failures;
    static BOOL logged_ready;
    void *platform;
    BYTE *controller;
    void *handler = NULL;
    const char *why = NULL;
    char line[256];

    *input_handler_out = NULL;
    if (!game_image || !game_app_main) return NULL;
    platform = current_app_platform();
    controller = platform ? get_hid_controller(platform) : NULL;
    if (!controller) {
        why = "no HIDController (AppMain+0x1c -> platform+0x3b4 -> adapter+4)";
    } else if (IsBadReadPtr(controller, ARM_INPUT_HID_TEXT_SEQUENCE_BYTE + 1)) {
        why = "HIDController is not readable up to +0x1f0";
    } else {
        handler = *(void **)(controller + ARM_INPUT_CONTROLLER_INPUT_HANDLER);
        if (!handler) why = "HIDController+0x10 (EnqueueInput this) is null";
        else if (IsBadReadPtr(handler, ARM_INPUT_INPUT_HANDLER_READABLE_SIZE))
            why = "EnqueueInput this is not readable (0x170 bytes)";
    }
    if (why) {
        if (logged_failures < 8) {
            wsprintfA(line, "ARM32 input target unavailable: %s (platform=%p controller=%p handler=%p)",
                      why, platform, (void *)controller, handler);
            log_line(line);
            ++logged_failures;
        }
        return NULL;
    }
    if (!logged_ready) {
        wsprintfA(line,
            "ARM32 input target: platform=%p controller=%p handler=%p actual=%u command=%d hold25=%u",
            platform, (void *)controller, handler,
            (unsigned)controller[ARM_INPUT_MOUSE_ACTUAL_OFFSET],
            *(int *)(controller + ARM_INPUT_MOUSE_COMMAND_OFFSET), (unsigned)controller[0x25]);
        log_line(line);
        logged_ready = TRUE;
    }
    *input_handler_out = handler;
    return controller;
}

/* facts input.enqueue_input: void(void *adapter r0, void **slot r1).  `slot` is the ADDRESS of the pointer variable
 * that owns the item (unique_ptr by reference); the callee moves it into the queue and zeroes the variable. */
static void arm32_enqueue_input(void *input_handler, void *event)
{
    Arm32EnqueueInputFn enqueue_input =
        (Arm32EnqueueInputFn)ARM_CODE(game_image, ARM_INPUT_ENQUEUE_INPUT);
    void *slot = event;
    enqueue_input(input_handler, &slot);
}

/*
 * The three AppPlatform_Winrt mouse-mode entry points (vf18 relative, vf19 absolute, vf22 toggle) wrap a tiny lambda
 * into a std::function and run it through CoreDispatcher.RunAsync, i.e. the host's drain_dispatcher_queue calls the
 * lambda's _Do_call on the host thread.  The stock lambdas only write a one-frame command to HIDController+0x28; we
 * replace them (facts input.mouse_*_hook) so the DLL retains the requested state.  Plain AAPCS, `this` in r0.
 */
static void win32_request_relative_mouse(void *lambda_self)
{
    static unsigned logged;
    BYTE *controller = get_mouse_mode_controller(lambda_self);
    if (logged < 32) {
        char line[192];
        wsprintfA(line,
            "Minecraft 1.2.8 relative mouse delegate invoked: this=%p controller=%p guard=%d actual=%d",
            lambda_self, (void *)controller, initial_menu_cursor_guard,
            controller ? controller[mouse_actual_offset()] : -1);
        log_line(line);
        ++logged;
    }
    if (initial_menu_cursor_guard) {
        /* App activation emits one relative-mode request before the first main-menu screen owns the pointer.
         * Ignore only that startup phase; the first real menu click or an explicit absolute request removes the
         * guard, so entering a world can capture normally. */
        if (controller) *(int *)(controller + mouse_command_offset()) = 0;
        host_relative_requested = FALSE;
        mouse_clip_dirty = TRUE;
        return;
    }
    if (controller && controller[mouse_actual_offset()] == 0) {
        *(int *)(controller + mouse_command_offset()) = 1;
    }
    /* The "actual" byte is owned by the host under an HWND, so it must not suppress the request. */
    host_relative_requested = TRUE;
    mouse_clip_dirty = TRUE;
}

static void win32_request_absolute_mouse(void *lambda_self)
{
    static unsigned logged;
    BYTE *controller = get_mouse_mode_controller(lambda_self);
    if (logged < 32) {
        char line[192];
        wsprintfA(line,
            "Minecraft 1.2.8 absolute mouse delegate invoked: this=%p controller=%p",
            lambda_self, (void *)controller);
        log_line(line);
        ++logged;
    }
    if (controller) {
        *(int *)(controller + mouse_command_offset()) = 2;
    }
    initial_menu_cursor_guard = FALSE;
    host_relative_requested = FALSE;
    mouse_clip_dirty = TRUE;
}

static void win32_toggle_mouse_mode(void *lambda_self)
{
    static unsigned logged;
    BYTE *controller = get_mouse_mode_controller(lambda_self);
    if (logged < 32) {
        char line[192];
        wsprintfA(line,
            "Minecraft 1.2.8 toggle mouse delegate invoked: this=%p controller=%p guard=%d",
            lambda_self, (void *)controller, initial_menu_cursor_guard);
        log_line(line);
        ++logged;
    }
    if (!controller) return;
    if (initial_menu_cursor_guard) {
        *(int *)(controller + mouse_command_offset()) = 0;
        host_relative_requested = FALSE;
        mouse_clip_dirty = TRUE;
        return;
    }
    controller[mouse_actual_offset()] =
        controller[mouse_actual_offset()] == 0;
    *(int *)(controller + mouse_command_offset()) =
        controller[mouse_actual_offset()] ? 2 : 1;
    host_relative_requested = controller[mouse_actual_offset()] == 0;
    mouse_clip_dirty = TRUE;
}

/* facts input.mouse_*_hook: the three lambdas are the slot-2 (_Do_call) cells of their std::function _Func_impl
 * vtables in .data.  Plan A overwrites the cell (data only, no code patching).  Plan B (fallback, only when the cell no
 * longer holds image+fn+1 but the function bytes still match) writes the 8-byte absolute jump over the entry.  Every
 * site is verified before anything is patched. */
static BOOL install_arm32_input_hooks(BYTE *image)
{
    typedef struct Arm32MouseHookSite {
        const char *name;
        DWORD function_rva;
        DWORD cell_rva;
        const BYTE *expected_bytes;
        void *hook;
    } Arm32MouseHookSite;
    static BOOL installed;
    Arm32MouseHookSite sites[3];
    BOOL use_cell[3];
    unsigned index;
    char line[256];

    if (installed) return TRUE;
    if (!image) return FALSE;
    /* Review addition: sanity-check the game functions the queue_* builders call (log only: input injection is gated
     * elsewhere, but a mismatch here is the first thing to look for in a device log). */
    {
        static const struct { const char *name; DWORD rva; const BYTE *bytes; } game_functions[] = {
            { "EnqueueInput", ARM_INPUT_ENQUEUE_INPUT, ARM_INPUT_ENQUEUE_INPUT_EXPECTED },
            { "MouseItem factory", ARM_INPUT_MAKE_MOUSE_EVENT, ARM_INPUT_MAKE_MOUSE_EVENT_EXPECTED },
            { "KeyCodeItem factory", ARM_INPUT_MAKE_KEY_EVENT, ARM_INPUT_MAKE_KEY_EVENT_EXPECTED },
            { "TextItem factory", ARM_INPUT_MAKE_TEXT_EVENT, ARM_INPUT_MAKE_TEXT_EVENT_EXPECTED },
            { "HID virtual-key mapper", ARM_INPUT_MAP_VIRTUAL_KEY, ARM_INPUT_MAP_VIRTUAL_KEY_EXPECTED },
            { "MultitouchItem factory (touch)", ARM_TOUCH_HOST_MAKE_MULTITOUCH_ITEM, ARM_TOUCH_HOST_MAKE_MULTITOUCH_ITEM_EXPECTED },
            { "Multitouch::feed (touch)", ARM_TOUCH_HOST_MULTITOUCH_FEED, ARM_TOUCH_HOST_MULTITOUCH_FEED_EXPECTED },
            { "InputHandler::tick (touch mode switch)", ARM_TOUCH_GAME_INPUT_HANDLER_TICK, ARM_TOUCH_GAME_INPUT_HANDLER_TICK_EXPECTED }
        };
        unsigned function_index;
        for (function_index = 0; function_index < sizeof(game_functions) / sizeof(game_functions[0]); ++function_index) {
            if (!arm32_check_bytes(image + game_functions[function_index].rva, game_functions[function_index].bytes, 12,
                                   game_functions[function_index].name)) {
                log_line("ARM32 input: game function bytes differ from the facts, input injection may misbehave");
            }
        }
    }
    sites[0].name = "relative mouse hook";
    sites[0].function_rva = ARM_INPUT_MOUSE_RELATIVE_HOOK;
    sites[0].cell_rva = 0x01369c30u;
    sites[0].expected_bytes = ARM_INPUT_MOUSE_RELATIVE_HOOK_EXPECTED;
    sites[0].hook = (void *)win32_request_relative_mouse;
    sites[1].name = "absolute mouse hook";
    sites[1].function_rva = ARM_INPUT_MOUSE_ABSOLUTE_HOOK;
    sites[1].cell_rva = 0x01369c14u;
    sites[1].expected_bytes = ARM_INPUT_MOUSE_ABSOLUTE_HOOK_EXPECTED;
    sites[1].hook = (void *)win32_request_absolute_mouse;
    sites[2].name = "toggle mouse hook";
    sites[2].function_rva = ARM_INPUT_MOUSE_TOGGLE_HOOK;
    sites[2].cell_rva = 0x01369c68u;
    sites[2].expected_bytes = ARM_INPUT_MOUSE_TOGGLE_HOOK_EXPECTED;
    sites[2].hook = (void *)win32_toggle_mouse_mode;

    for (index = 0; index < 3; ++index) {
        void **cell = (void **)(image + sites[index].cell_rva);
        void *wanted = ARM_CODE(image, sites[index].function_rva);
        BOOL bytes_ok = arm32_check_bytes(image + sites[index].function_rva,
                                          sites[index].expected_bytes, 16, sites[index].name);
        use_cell[index] = !IsBadReadPtr(cell, sizeof(*cell)) && *cell == wanted;
        if (!use_cell[index]) {
            wsprintfA(line, "ARM32 %s: vtable cell %p holds %p, expected %p", sites[index].name,
                      (void *)cell, IsBadReadPtr(cell, sizeof(*cell)) ? NULL : *cell, wanted);
            log_line(line);
            if (!bytes_ok) {
                log_line("mouse-mode hook signature mismatch");
                return FALSE;
            }
        }
    }
    for (index = 0; index < 3; ++index) {
        void **cell = (void **)(image + sites[index].cell_rva);
        void *hook = (void *)((ULONG_PTR)sites[index].hook | 1u);
        BOOL ok;
        if (use_cell[index]) {
            ok = arm32_patch_cell(cell, ARM_CODE(image, sites[index].function_rva), hook, NULL,
                                  sites[index].name);
        } else {
            ok = arm32_write_thumb_jump(image + sites[index].function_rva, hook, sites[index].name);
        }
        if (!ok) {
            log_line("mouse-mode hook installation failed");
            return FALSE;
        }
    }
    installed = TRUE;
    log_line("installed persistent HWND relative-mouse hooks (ARM32 std::function cells)");
    return TRUE;
}

static BOOL install_mouse_mode_hooks(BYTE *image)
{
    return install_arm32_input_hooks(image);
}

static BOOL game_requests_relative_mouse(void)
{
    static int previous_requested = -1;
    static int previous_actual = -1;
    static int previous_command = -1;
    void *platform;
    BYTE *controller;
    int actual = 0;
    int command = 0;
    char line[128];

    if (!game_app_main) return arm32_relative_mouse_gate(host_relative_requested);
    platform = current_app_platform();
    controller = get_hid_controller(platform);
    if (controller) {
        actual = controller[mouse_actual_offset()] != 0;
        command = *(int *)(controller + mouse_command_offset());
        /* The command is the hooks' one-frame request.  The stock per-frame HID update (0x626570, run inside the game
         * frame right after this) would turn a pending 1 (or a 2 while +0x25 is set) into dispatcher delegates
         * (0x627c14 / 0x627d28) that call CoreWindow capture/cursor APIs and create a CoreCursor through the real
         * WinRT factory.  The host owns the pointer under an HWND, so the request is consumed here, after it was logged
         * (facts input.mouse_command_offset, audit residual "command consumer byte +0x25"). */
        if (command) *(int *)(controller + mouse_command_offset()) = 0;
    }
    if ((int)host_relative_requested != previous_requested ||
        actual != previous_actual || command != previous_command) {
        wsprintfA(line,
                  "HID mouse mode: requested=%d actual=%d command=%d",
                  host_relative_requested, actual, command);
        log_line(line);
        previous_requested = host_relative_requested;
        previous_actual = actual;
        previous_command = command;
    }
    /* touch is the active input device: never capture / clip / hide the cursor and keep HIDController+0xa0 at 0 (the single
     * gate for "relative mouse", touch_host.md 4.4) */
    return arm32_relative_mouse_gate(host_relative_requested);
}
#else /* WIN32CRAFT_ARM32 */
static BYTE *get_hid_controller(void *platform)
{
    if (host_is_115 || host_is_128) {
        BYTE *adapter;
        SIZE_T adapter_offset = host_is_128 ? 0x3b4 : 0x278;
        if (!platform ||
            IsBadReadPtr((BYTE *)platform + adapter_offset, sizeof(void *))) {
            return NULL;
        }
        adapter = *(BYTE **)((BYTE *)platform + adapter_offset);
        if (!adapter || IsBadReadPtr(adapter + 4, sizeof(void *))) {
            return NULL;
        }
        return *(BYTE **)(adapter + 4);
    }
    if (!platform ||
        IsBadReadPtr((BYTE *)platform + APP_PLATFORM_HID_OFFSET,
                     sizeof(void *))) {
        return NULL;
    }
    return *(BYTE **)((BYTE *)platform + APP_PLATFORM_HID_OFFSET);
}

/* 1.1.5 moved the three mouse-mode methods onto a small adapter object.
 * Their this pointer stores the HID adapter at +4, and the controller is
 * still the pointer at adapter+4. */
static BYTE *get_mouse_mode_controller(void *object)
{
    BYTE *adapter;
    if (!host_is_115 && !host_is_128) return get_hid_controller(object);
    if (!object || IsBadReadPtr((BYTE *)object + 4, sizeof(void *)))
        return NULL;
    adapter = *(BYTE **)((BYTE *)object + 4);
    if (!adapter || IsBadReadPtr(adapter + 4, sizeof(void *)))
        return NULL;
    return *(BYTE **)(adapter + 4);
}

static SIZE_T mouse_actual_offset(void)
{
    return host_is_128 ? 0xa0 : (host_is_115 ? 0xb8 : 0x88);
}

static SIZE_T mouse_command_offset(void)
{
    return host_is_128 ? 0x28 : 0x44;
}

/*
 * These are the three AppPlatform_Winrt mouse-mode entry points used by
 * MinecraftClient.  The stock UWP implementation stores a one-frame command
 * at HIDController+0x48 and immediately consumes it while trying to talk to
 * CoreWindow.  An HWND host therefore cannot reliably observe the request by
 * polling after the frame.  Replace the tiny entry points and retain the
 * requested state in the DLL instead.
 */
static void __attribute__((thiscall)) win32_request_relative_mouse(
    void *platform)
{
    static unsigned logged;
    BYTE *controller = get_mouse_mode_controller(platform);
    if (logged < 32) {
        char line[192];
        wsprintfA(line,
            "Minecraft 1.1.5 relative mouse delegate invoked: this=%p controller=%p guard=%d actual=%d",
            platform, controller, initial_menu_cursor_guard,
            controller ? controller[mouse_actual_offset()] : -1);
        log_line(line);
        ++logged;
    }
    if (initial_menu_cursor_guard) {
        /* App activation emits one relative-mode request before the first
         * main-menu screen owns the pointer.  Ignore only that startup phase;
         * the first real menu click or an explicit absolute request removes
         * the guard, so entering a world can capture normally. */
        if (controller) *(int *)(controller + mouse_command_offset()) = 0;
        host_relative_requested = FALSE;
        mouse_clip_dirty = TRUE;
        return;
    }
    if (controller && controller[mouse_actual_offset()] == 0) {
        *(int *)(controller + mouse_command_offset()) = 1;
    }
    /* The game's "actual" byte is updated asynchronously by CoreWindow in
     * the UWP build.  Under an HWND it may remain stale at 1, so it must not
     * suppress the host-side request when a world asks for relative mode. */
    host_relative_requested = TRUE;
    mouse_clip_dirty = TRUE;
}

static void __attribute__((thiscall)) win32_request_absolute_mouse(
    void *platform)
{
    static unsigned logged;
    BYTE *controller = get_mouse_mode_controller(platform);
    if (logged < 32) {
        char line[192];
        wsprintfA(line,
            "Minecraft 1.1.5 absolute mouse delegate invoked: this=%p controller=%p",
            platform, controller);
        log_line(line);
        ++logged;
    }
    if (controller) {
        *(int *)(controller + mouse_command_offset()) = 2;
    }
    initial_menu_cursor_guard = FALSE;
    host_relative_requested = FALSE;
    mouse_clip_dirty = TRUE;
}

static void __attribute__((thiscall)) win32_toggle_mouse_mode(
    void *platform)
{
    static unsigned logged;
    BYTE *controller = get_mouse_mode_controller(platform);
    if (logged < 32) {
        char line[192];
        wsprintfA(line,
            "Minecraft 1.1.5 toggle mouse delegate invoked: this=%p controller=%p guard=%d",
            platform, controller, initial_menu_cursor_guard);
        log_line(line);
        ++logged;
    }
    if (!controller) return;
    if (initial_menu_cursor_guard) {
        *(int *)(controller + mouse_command_offset()) = 0;
        host_relative_requested = FALSE;
        mouse_clip_dirty = TRUE;
        return;
    }
    controller[mouse_actual_offset()] =
        controller[mouse_actual_offset()] == 0;
    *(int *)(controller + mouse_command_offset()) =
        controller[mouse_actual_offset()] ? 2 : 1;
    host_relative_requested = controller[mouse_actual_offset()] == 0;
    mouse_clip_dirty = TRUE;
}

static BOOL install_mouse_mode_hooks(BYTE *image)
{
    static const BYTE relative_01510[10] = {
        0x8b, 0x81, 0x58, 0x02, 0x00, 0x00, 0x80, 0xb8, 0x88, 0x00
    };
    static const BYTE absolute_01510[10] = {
        0x8b, 0x81, 0x58, 0x02, 0x00, 0x00, 0xc7, 0x40, 0x44, 0x02
    };
    static const BYTE toggle_01510[10] = {
        0x8b, 0x91, 0x58, 0x02, 0x00, 0x00, 0x80, 0xba, 0x88, 0x00
    };
    static const BYTE relative_115[10] = {
        0x8b, 0x41, 0x04, 0x8b, 0x40, 0x04, 0x80, 0xb8, 0xb8, 0x00
    };
    static const BYTE absolute_115[10] = {
        0x8b, 0x41, 0x04, 0x8b, 0x40, 0x04, 0xc7, 0x40, 0x44, 0x02
    };
    static const BYTE toggle_115[10] = {
        0x8b, 0x41, 0x04, 0x8b, 0x50, 0x04, 0x80, 0xba, 0xb8, 0x00
    };
    BYTE *relative_site = image +
        (host_is_128 ? RVA_128_MOUSE_RELATIVE :
         (host_is_115 ? RVA_115_MOUSE_RELATIVE : 0x005ffb10));
    BYTE *absolute_site = image +
        (host_is_128 ? RVA_128_MOUSE_ABSOLUTE :
         (host_is_115 ? RVA_115_MOUSE_ABSOLUTE : 0x005ffb30));
    BYTE *toggle_site = image +
        (host_is_128 ? RVA_128_MOUSE_TOGGLE :
         (host_is_115 ? RVA_115_MOUSE_TOGGLE : 0x005ffb40));
    const BYTE *relative_expected =
        (host_is_115 || host_is_128) ? relative_115 : relative_01510;
    const BYTE *absolute_expected =
        (host_is_115 || host_is_128) ? absolute_115 : absolute_01510;
    const BYTE *toggle_expected =
        (host_is_115 || host_is_128) ? toggle_115 : toggle_01510;

    if (host_is_128) {
        static const BYTE relative_128[10] = {
            0x8b, 0x41, 0x04, 0x8b, 0x40, 0x04, 0x80, 0xb8, 0xa0, 0x00
        };
        static const BYTE absolute_128[10] = {
            0x8b, 0x41, 0x04, 0x8b, 0x40, 0x04, 0xc7, 0x40, 0x28, 0x02
        };
        static const BYTE toggle_128[10] = {
            0x8b, 0x41, 0x04, 0x8b, 0x50, 0x04, 0x80, 0xba, 0xa0, 0x00
        };
        relative_expected = relative_128;
        absolute_expected = absolute_128;
        toggle_expected = toggle_128;
    }

    if (memcmp(relative_site, relative_expected, 10) ||
        memcmp(absolute_site, absolute_expected, 10) ||
        memcmp(toggle_site, toggle_expected, 10)) {
        log_line("mouse-mode hook signature mismatch");
        return FALSE;
    }
    if (!write_relative_jump(relative_site, win32_request_relative_mouse) ||
        !write_relative_jump(absolute_site, win32_request_absolute_mouse) ||
        !write_relative_jump(toggle_site, win32_toggle_mouse_mode)) {
        log_line("mouse-mode hook installation failed");
        return FALSE;
    }
    log_line(host_is_115
        ? "installed Minecraft 1.1.5 persistent HWND relative-mouse hooks"
        : "installed persistent HWND relative-mouse hooks");
    return TRUE;
}

static BOOL game_requests_relative_mouse(void)
{
    static int previous_requested = -1;
    static int previous_actual = -1;
    static int previous_command = -1;
    void *platform;
    BYTE *controller;
    int actual = 0;
    int command = 0;
    char line[128];

    if (!game_app_main) return host_relative_requested;
    platform = current_app_platform();
    controller = get_hid_controller(platform);
    if (controller) {
        actual = controller[mouse_actual_offset()] != 0;
        command = *(int *)(controller + mouse_command_offset());
        /*
         * Do not infer the requested mode from +0x48 here.  That field is a
         * transient command slot and is initialized/stale as 1 in menu paths.
         * The three direct hooks above are the authoritative state source.
         */
    }
    if ((int)host_relative_requested != previous_requested ||
        actual != previous_actual || command != previous_command) {
        wsprintfA(line,
                  "HID mouse mode: requested=%d actual=%d command=%d",
                  host_relative_requested, actual, command);
        log_line(line);
        previous_requested = host_relative_requested;
        previous_actual = actual;
        previous_command = command;
    }
    return host_relative_requested;
}
#endif /* WIN32CRAFT_ARM32 (input_hid: get_hid_controller .. game_requests_relative_mouse) */

#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 client-state unit (facts client_state.*).  Same object model as x86 (4-byte pointers):
 * ClientInstance +4 second vtable / +0x18 MinecraftGame* / +0x28 LevelRenderer* / +0x44 keyboard manager /
 * +0x2c0 SceneStack*, MinecraftGame +0x6c SceneStack*, SceneStack begin@0 end@4 stride 8.
 * Vtable constants below are plain data addresses (no Thumb bit); only CODE pointers stored in vtables have
 * bit 0 set (image + rva + 1), which is why ARM_CODE() is used for every function compare/call.
 * ====================================================================================================== */
typedef void (*ClientRenderFn)(void *self, ULONG_PTR r1, ULONG_PTR r2, ULONG_PTR r3);
typedef DWORD (*SceneFlagFn)(void *scene);

static ClientRenderFn client_render_original_arm32;
static volatile LONG client_render_calls_arm32;
static BOOL client_hook_installed_arm32;
static DWORD client_hook_install_tick_arm32;

/* Milliseconds since the first call, for log intervals only.  The compat header declares no tick API, so use
 * QueryPerformanceCounter with double math (no 64-bit division helper is needed/linked on ARM32). */
static DWORD arm32_client_ms(void)
{
    static double base_ticks;
    static BOOL have_base;
    LARGE_INTEGER counter, frequency;
    double ticks, freq;

    if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) return 0;
    ticks = (double)(DWORD)(counter.QuadPart >> 32) * 4294967296.0 + (double)(DWORD)counter.QuadPart;
    freq = (double)(DWORD)(frequency.QuadPart >> 32) * 4294967296.0 + (double)(DWORD)frequency.QuadPart;
    if (freq < 1.0) return 0;
    if (!have_base) {
        have_base = TRUE;
        base_ticks = ticks;
    }
    return (DWORD)((ticks - base_ticks) * 1000.0 / freq);
}

/* Log (at most 12 distinct values in total) an object whose vptr is not the one the host expects, so a wrong
 * vtable constant shows up in the log instead of silently disabling text focus / gameplay mouse capture. */
static void arm32_note_unexpected_vtable(const char *what, const void *object, const void *actual,
                                         const void *expected)
{
    static const void *seen[12];
    static int seen_count;
    char line[320];
    int index;

    for (index = 0; index < seen_count; ++index)
        if (seen[index] == actual) return;
    if (seen_count >= (int)(sizeof(seen) / sizeof(seen[0]))) return;
    seen[seen_count++] = actual;
    wsprintfA(line,
              "Minecraft 1.2.8 client-state: %s %p has vtable %p (image+0x%08lx) but the host expects %p (image+0x%08lx)",
              what, object, actual, (unsigned long)((ULONG_PTR)actual - (ULONG_PTR)game_image),
              expected, (unsigned long)((ULONG_PTR)expected - (ULONG_PTR)game_image));
    log_line(line);
}

static BOOL plausible_client_instance_128(BYTE *client, void *vtable)
{
    static BYTE *last_reported;
    static LONG reports;
    char line[320];
    void *second_vtable = game_image + ARM_CLIENT_STATE_CLIENTINSTANCE_VTABLE_SECONDARY;

    if (!client) return FALSE;
    if (!IsBadReadPtr(client, ARM_CLIENT_STATE_CLIENTINSTANCE_SIZE) &&
        *(void **)client == vtable &&
        *(void **)(client + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X04_VTABLE2) == second_vtable) {
        return TRUE;
    }
    /* the captured pointer is rejected: say why (once per distinct pointer, at most 4 times) */
    if (client != last_reported && reports < 4) {
        last_reported = client;
        ++reports;
        if (IsBadReadPtr(client, ARM_CLIENT_STATE_CLIENTINSTANCE_SIZE)) {
            wsprintfA(line,
                      "Minecraft 1.2.8 client-state: captured ClientInstance %p is not readable for 0x%lx bytes",
                      client, (unsigned long)ARM_CLIENT_STATE_CLIENTINSTANCE_SIZE);
        } else {
            wsprintfA(line,
                      "Minecraft 1.2.8 client-state: captured ClientInstance %p rejected: [0]=%p (expected %p) "
                      "[4]=%p (expected %p) image=%p",
                      client, *(void **)client, vtable,
                      *(void **)(client + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X04_VTABLE2), second_vtable,
                      game_image);
        }
        log_line(line);
    }
    return FALSE;
}

/* Capture the live ClientInstance at ClientInstance::vf3 (facts client_state.render_slot3_function).  The game
 * function consumes r0=this and r2/r3 (r1 is never read); all four registers are forwarded unchanged and the
 * original reads no stack arguments, so a plain 4-register C call through the Thumb pointer is complete.  The
 * caller of slot 3 is not known (it may be another thread), hence no assumptions about frequency or thread and
 * the first calls are logged.  No process-memory searches belong in the frame loop. */
static void win32_client_render_128(void *self, ULONG_PTR r1, ULONG_PTR r2, ULONG_PTR r3)
{
    LONG call = InterlockedIncrement(&client_render_calls_arm32);
    char line[320];

    if (client_instance_128 != (BYTE *)self)
        log_pointer("Minecraft 1.2.8 live render client", self);
    if (call <= 3 || call == 100 || call == 1000 || call == 10000 || call == 100000 || call == 1000000) {
        wsprintfA(line,
                  "Minecraft 1.2.8 ClientInstance::vf3 hook call #%ld: self=%p thread=%lu +%lu ms since hook "
                  "install r1=%08lx r2=%08lx r3=%08lx",
                  call, self, (unsigned long)GetCurrentThreadId(),
                  (unsigned long)(arm32_client_ms() - client_hook_install_tick_arm32),
                  (unsigned long)r1, (unsigned long)r2, (unsigned long)r3);
        log_line(line);
    }
    client_instance_128 = (BYTE *)self;
    if (client_render_original_arm32)
        client_render_original_arm32(self, r1, r2, r3);
}

/* Replace the single vtable cell (ClientInstance primary vtable slot 3, shared by every ClientInstance).
 * Returns FALSE (with a precise log) when the cell does not hold image+0x11a280+1 or the function entry bytes
 * differ; the caller decides whether that is fatal. */
static BOOL install_client_state_hook_128(BYTE *image)
{
    void **slot;
    void *expected;
    void *hook = (void *)win32_client_render_128;
    void **vtable;
    char line[400];

    if (!image) {
        log_line("Minecraft 1.2.8 client-state hook: image base is NULL");
        return FALSE;
    }
    slot = (void **)(image + ARM_CLIENT_STATE_RENDER_HOOK_SLOT);
    expected = ARM_CODE(image, ARM_CLIENT_STATE_RENDER_SLOT3_FUNCTION);
    vtable = (void **)(image + ARM_CLIENT_STATE_CLIENTINSTANCE_VTABLE_PRIMARY);
    if (((ULONG_PTR)hook & 1u) == 0) {
        wsprintfA(line, "Minecraft 1.2.8 client-state hook: hook function %p has no Thumb bit; refusing to patch", hook);
        log_line(line);
        return FALSE;
    }
    if (slot != vtable + 3) {
        wsprintfA(line, "Minecraft 1.2.8 client-state hook: internal constant mismatch slot=%p vtable+3=%p",
                  (void *)slot, (void *)(vtable + 3));
        log_line(line);
        return FALSE;
    }
    if (IsBadReadPtr(slot, sizeof(*slot))) {
        wsprintfA(line, "Minecraft 1.2.8 client-state hook: vtable cell %p (image+0x%08lx) is unreadable",
                  (void *)slot, (unsigned long)ARM_CLIENT_STATE_RENDER_HOOK_SLOT);
        log_line(line);
        return FALSE;
    }
    if (*slot == hook && client_render_original_arm32) {
        log_line("Minecraft 1.2.8 client-state hook: already installed");
        client_hook_installed_arm32 = TRUE;
        return TRUE;
    }
    if (*slot != expected) {
        wsprintfA(line,
                  "Minecraft 1.2.8 client-state hook: vtable cell %p (image+0x%08lx) holds %p (image+0x%08lx) but "
                  "expected %p (image+0x%08lx = ClientInstance::vf3 | Thumb bit); not patched",
                  (void *)slot, (unsigned long)ARM_CLIENT_STATE_RENDER_HOOK_SLOT, *slot,
                  (unsigned long)((ULONG_PTR)*slot - (ULONG_PTR)image), expected,
                  (unsigned long)(ARM_CLIENT_STATE_RENDER_SLOT3_FUNCTION + 1u));
        log_line(line);
        if (!IsBadReadPtr(vtable, 7 * sizeof(void *))) {
            wsprintfA(line,
                      "Minecraft 1.2.8 client-state hook: ClientInstance vtable %p slots 0..6 = %p %p %p %p %p %p %p "
                      "(image=%p)",
                      (void *)vtable, vtable[0], vtable[1], vtable[2], vtable[3], vtable[4], vtable[5], vtable[6],
                      (void *)image);
            log_line(line);
        }
        return FALSE;
    }
    if (!arm32_check_bytes(image + ARM_CLIENT_STATE_RENDER_SLOT3_FUNCTION,
                           ARM_CLIENT_STATE_RENDER_SLOT3_FUNCTION_EXPECTED,
                           sizeof(ARM_CLIENT_STATE_RENDER_SLOT3_FUNCTION_EXPECTED),
                           "ClientInstance::vf3 entry (0x0011a280)")) {
        log_line("Minecraft 1.2.8 client-state hook: ClientInstance::vf3 entry bytes differ; refusing to hook");
        return FALSE;
    }
    client_render_original_arm32 = (ClientRenderFn)expected;
    client_hook_install_tick_arm32 = arm32_client_ms();
    if (!arm32_patch_cell(slot, expected, hook, NULL, "ClientInstance vtable slot 3 (vf3 render/tick hook)")) {
        client_render_original_arm32 = NULL;
        log_line("Minecraft 1.2.8 client-state hook: vtable cell patch failed");
        return FALSE;
    }
    if (*slot != hook) {
        wsprintfA(line, "Minecraft 1.2.8 client-state hook: cell %p reads back %p after patching (expected %p)",
                  (void *)slot, *slot, hook);
        log_line(line);
        return FALSE;
    }
    client_hook_installed_arm32 = TRUE;
    wsprintfA(line,
              "Minecraft 1.2.8 client-state hook installed: cell %p original %p hook %p; waiting for the game to "
              "call ClientInstance::vf3",
              (void *)slot, expected, hook);
    log_line(line);
    return TRUE;
}

static BOOL client_world_active_128(void)
{
    BYTE *level_renderer;
    void *vptr;
    if (!plausible_client_instance_128(client_instance_128,
            game_image + ARM_CLIENT_STATE_CLIENTINSTANCE_VTABLE_PRIMARY)) return FALSE;
    level_renderer = *(BYTE **)(client_instance_128 + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X28_LEVELRENDERER);
    if (!level_renderer || IsBadReadPtr(level_renderer, sizeof(void *))) return FALSE;
    vptr = *(void **)level_renderer;
    if (vptr == game_image + ARM_CLIENT_STATE_LEVELRENDERER_VTABLE) return TRUE;
    arm32_note_unexpected_vtable("LevelRenderer", level_renderer, vptr,
                                 game_image + ARM_CLIENT_STATE_LEVELRENDERER_VTABLE);
    return FALSE;
}

/* SceneStack holds a vector<shared_ptr<AbstractScene>> at +0/+4 (4-byte pointers, stride 8, identical to x86).
 * ClientInstance::getTopScene (ARM RVA 0x11be24) uses the local stack at +0x2c0, falling back to MinecraftGame's
 * stack at +0x6c.  UIScene vtable slot 41 (+0xa4) is is_showing_menu, slot 42 (+0xa8) is should_steal_mouse.
 * These are the JSON screen flags, not a list of controller types or keys. */
static BYTE *top_scene_128(BYTE *stack)
{
    BYTE *begin, *end;
    if (!stack || IsBadReadPtr(stack, 8)) return NULL;
    begin = *(BYTE **)stack;
    end = *(BYTE **)(stack + 4);
    if (!begin || (ULONG_PTR)end <= (ULONG_PTR)begin ||
        ((ULONG_PTR)end - (ULONG_PTR)begin) % 8 ||
        IsBadReadPtr(end - 8, 8)) return NULL;
    return *(BYTE **)(end - 8);
}

/* Progress/diagnostic log for the capture hook: it is the ONLY place the ClientInstance is captured and its caller
 * is unknown, so tell the user (log) when nothing arrived after 5/15/30/60 s and every minute afterwards. */
static void arm32_client_wait_diagnostics(void)
{
    static BOOL captured_logged;
    static DWORD start_tick, frames, next_report_ms = 5000;
    static LONG reports;
    char line[400];
    DWORD now, elapsed;

    if (captured_logged) return;
    now = arm32_client_ms();
    if (client_instance_128) {
        if (frames) {
            wsprintfA(line,
                      "Minecraft 1.2.8 client-state: ClientInstance %p captured after %lu frames / %lu ms of waiting",
                      client_instance_128, (unsigned long)frames, (unsigned long)(now - start_tick));
            log_line(line);
        }
        captured_logged = TRUE;
        return;
    }
    if (!frames) start_tick = now;
    ++frames;
    elapsed = now - start_tick;
    if (elapsed < next_report_ms || reports >= 12) return;
    ++reports;
    next_report_ms = next_report_ms == 5000 ? 15000 : next_report_ms == 15000 ? 30000 :
                     next_report_ms == 30000 ? 60000 : next_report_ms + 60000;
    wsprintfA(line,
              "Minecraft 1.2.8 client-state: no ClientInstance captured after %lu frames / %lu ms (slot-3 hook "
              "installed=%d, hook calls=%ld); text-focus and gameplay mouse-capture detection stay inactive until "
              "the game calls ClientInstance::vf3 (RVA 0x%08lx)",
              (unsigned long)frames, (unsigned long)elapsed, (int)client_hook_installed_arm32,
              (long)client_render_calls_arm32, (unsigned long)ARM_CLIENT_STATE_RENDER_SLOT3_FUNCTION);
    log_line(line);
}

static void refresh_relative_mouse_from_client_128(void)
{
    BYTE *scene, *owner;
    BOOL gameplay = FALSE;
    static int previous_gameplay = -1;
    static BOOL announced_client, reported_no_scene, reported_bad_slots, reported_no_image;
    char line[400];

    if (!host_is_128 || !game_app_main) return;
    if (!game_image) {
        if (!reported_no_image) {
            reported_no_image = TRUE;
            log_line("Minecraft 1.2.8 client-state: game_image is NULL, client-state refresh skipped");
        }
        return;
    }
    arm32_client_wait_diagnostics();
    /* MinecraftKeyboardManager::show/hide (ARM RVAs 0x395504/0x395620) maintain bit 1 of byte +5 even when no
     * on-screen keyboard is requested (facts client_state.keyboardmanager_focus_byte). */
    if (plausible_client_instance_128(client_instance_128,
            game_image + ARM_CLIENT_STATE_CLIENTINSTANCE_VTABLE_PRIMARY)) {
        BYTE *keyboard = *(BYTE **)(client_instance_128 + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X44_KEYBOARD);
        BOOL focused = FALSE;
        if (!announced_client) {
            announced_client = TRUE;
            wsprintfA(line,
                      "Minecraft 1.2.8 client-state: ClientInstance %p accepted (owner=%p keyboard=%p "
                      "level_renderer=%p local_scene_stack=%p)",
                      client_instance_128,
                      *(void **)(client_instance_128 + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X18_OWNER),
                      keyboard,
                      *(void **)(client_instance_128 + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X28_LEVELRENDERER),
                      *(void **)(client_instance_128 + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X2C0_SCENESTACK));
            log_line(line);
        }
        if (keyboard && !IsBadReadPtr(keyboard, 6)) {
            if (*(void **)keyboard == game_image + ARM_CLIENT_STATE_KEYBOARDMANAGER_VTABLE)
                focused = (keyboard[ARM_CLIENT_STATE_KEYBOARDMANAGER_FOCUS_BYTE] & 2) != 0;
            else
                arm32_note_unexpected_vtable("MinecraftKeyboardManager", keyboard, *(void **)keyboard,
                                             game_image + ARM_CLIENT_STATE_KEYBOARDMANAGER_VTABLE);
        }
        if (text_input_active != focused) {
            text_input_active = focused;
            pending_high_surrogate = 0;
            if (!focused) suppress_next_t_character = FALSE;
            log_line(focused ? "Minecraft 1.2.8 text focus entered"
                             : "Minecraft 1.2.8 text focus left");
        }
    }
    if (client_world_active_128()) {
        scene = top_scene_128(*(BYTE **)(client_instance_128 + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X2C0_SCENESTACK));
        if (!scene) {
            owner = *(BYTE **)(client_instance_128 + ARM_CLIENT_STATE_CLIENTINSTANCE_OFF_0X18_OWNER);
            if (owner && !IsBadReadPtr(owner + ARM_CLIENT_STATE_MINECRAFTGAME_OFF_0X6C_SCENESTACK, sizeof(void *)))
                scene = top_scene_128(*(BYTE **)(owner + ARM_CLIENT_STATE_MINECRAFTGAME_OFF_0X6C_SCENESTACK));
        }
        if (scene && !IsBadReadPtr(scene, sizeof(void *))) {
            void **vtable = *(void ***)scene;
            if (vtable == (void **)(game_image + ARM_CLIENT_STATE_UISCENE_VTABLE)) {
                void *flag41 = ARM_CODE(game_image, ARM_CLIENT_STATE_UISCENE_SLOT41_IS_SHOWING_MENU);
                void *flag42 = ARM_CODE(game_image, ARM_CLIENT_STATE_UISCENE_SLOT42_SHOULD_STEAL_MOUSE);
                if (vtable[0xa4 / 4] != flag41 || vtable[0xa8 / 4] != flag42) {
                    /* never call an unverified function through a vtable cell */
                    if (!reported_bad_slots) {
                        reported_bad_slots = TRUE;
                        wsprintfA(line,
                                  "Minecraft 1.2.8 client-state: UIScene vtable %p slot41=%p (expected %p) "
                                  "slot42=%p (expected %p); gameplay detection disabled",
                                  (void *)vtable, vtable[0xa4 / 4], flag41, vtable[0xa8 / 4], flag42);
                        log_line(line);
                    }
                } else {
                    /* hud_screen.json sets should_steal_mouse=true: this means
                     * capture the desktop pointer for gameplay, not release it. */
                    gameplay = !((SceneFlagFn)vtable[0xa4 / 4])(scene) &&
                               ((SceneFlagFn)vtable[0xa8 / 4])(scene) &&
                               !text_input_active;
                }
            } else {
                arm32_note_unexpected_vtable("top scene (not UIScene)", scene, (void *)vtable,
                                             game_image + ARM_CLIENT_STATE_UISCENE_VTABLE);
            }
        } else if (!reported_no_scene) {
            reported_no_scene = TRUE;
            log_line("Minecraft 1.2.8 client-state: world is active but no top scene was found "
                     "(local stack and MinecraftGame stack empty or unreadable)");
        }
    }
    if ((int)gameplay != previous_gameplay) {
        log_line(gameplay ? "Minecraft 1.2.8 scene: gameplay capture"
                          : "Minecraft 1.2.8 scene: UI cursor");
        previous_gameplay = gameplay;
    }
    if (host_relative_requested != gameplay) {
        host_relative_requested = gameplay;
        mouse_clip_dirty = TRUE;
    }
}
#else /* !WIN32CRAFT_ARM32: x86 client-state code below is the unmodified original */
static BOOL plausible_client_instance_128(BYTE *client, void *vtable)
{
    if (!client || IsBadReadPtr(client, 0x378) ||
        *(void **)client != vtable ||
        *(void **)(client + 4) !=
            game_image + RVA_128_CLIENT_INSTANCE_VTABLE + 0x20) {
        return FALSE;
    }
    return TRUE;
}

/* Capture the live ClientInstance at its render callback, preserving its
 * two stack arguments. No process-memory searches belong in the frame loop. */
static void __attribute__((thiscall)) win32_client_render_128(
    void *self, void *argument1, void *argument2)
{
    typedef void (__attribute__((thiscall)) *RenderFn)(void *, void *, void *);
    if (client_instance_128 != (BYTE *)self)
        log_pointer("Minecraft 1.2.8 live render client", self);
    client_instance_128 = (BYTE *)self;
    ((RenderFn)(game_image + 0x0015a080))(self, argument1, argument2);
}

static BOOL install_client_state_hook_128(BYTE *image)
{
    void **slot = (void **)(image + RVA_128_CLIENT_INSTANCE_VTABLE) + 3;
    DWORD old_protection;
    if (*slot != image + 0x0015a080 ||
        !VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protection))
        return FALSE;
    *slot = win32_client_render_128;
    VirtualProtect(slot, sizeof(*slot), old_protection, &old_protection);
    return TRUE;
}

static BOOL client_world_active_128(void)
{
    BYTE *level_renderer;
    if (!plausible_client_instance_128(client_instance_128,
            game_image + RVA_128_CLIENT_INSTANCE_VTABLE)) return FALSE;
    level_renderer = *(BYTE **)(client_instance_128 + 0x28);
    return level_renderer &&
        !IsBadReadPtr(level_renderer, sizeof(void *)) &&
        *(void **)level_renderer ==
            game_image + RVA_128_LEVEL_RENDERER_VTABLE;
}

/* SceneStack holds a vector<shared_ptr<AbstractScene>> at +0/+4.
 * ClientInstance::getTopScene (RVA 0x15c740) uses the local stack at
 * +0x2c0, falling back to MinecraftGame's stack at +0x6c.
 * UIScene +0xa4 is is_showing_menu; +0xa8 is should_steal_mouse.
 * These are the JSON screen flags, not a list of controller types or keys. */
static BYTE *top_scene_128(BYTE *stack)
{
    BYTE *begin, *end;
    if (!stack || IsBadReadPtr(stack, 8)) return NULL;
    begin = *(BYTE **)stack;
    end = *(BYTE **)(stack + 4);
    if (!begin || (ULONG_PTR)end <= (ULONG_PTR)begin ||
        ((ULONG_PTR)end - (ULONG_PTR)begin) % 8 ||
        IsBadReadPtr(end - 8, 8)) return NULL;
    return *(BYTE **)(end - 8);
}

static void refresh_relative_mouse_from_client_128(void)
{
    typedef BYTE (__attribute__((thiscall)) *SceneFlagFn)(void *);
    BYTE *scene, *owner;
    BOOL gameplay = FALSE;
    static int previous_gameplay = -1;

    if (!host_is_128 || !game_app_main) return;
    /* MinecraftKeyboardManager::show/hide (RVAs 0x4e06d0/0x4e07d0)
     * maintain bit 1 even when no on-screen keyboard is requested. */
    if (plausible_client_instance_128(client_instance_128,
            game_image + RVA_128_CLIENT_INSTANCE_VTABLE)) {
        BYTE *keyboard = *(BYTE **)(client_instance_128 + 0x44);
        BOOL focused = keyboard && !IsBadReadPtr(keyboard, 6) &&
            *(void **)keyboard == game_image + 0x01569eac &&
            (keyboard[5] & 2);
        if (text_input_active != focused) {
            text_input_active = focused;
            pending_high_surrogate = 0;
            if (!focused) suppress_next_t_character = FALSE;
            log_line(focused ? "Minecraft 1.2.8 text focus entered"
                             : "Minecraft 1.2.8 text focus left");
        }
    }
    if (client_world_active_128()) {
        scene = top_scene_128(*(BYTE **)(client_instance_128 + 0x2c0));
        if (!scene) {
            owner = *(BYTE **)(client_instance_128 + 0x18);
            if (owner && !IsBadReadPtr(owner + 0x6c, sizeof(void *)))
                scene = top_scene_128(*(BYTE **)(owner + 0x6c));
        }
        if (scene && !IsBadReadPtr(scene, sizeof(void *)) &&
            *(void **)scene == game_image + 0x015684b8) {
            void **vtable = *(void ***)scene;
            /* hud_screen.json sets should_steal_mouse=true: this means
             * capture the desktop pointer for gameplay, not release it. */
            gameplay = !((SceneFlagFn)vtable[0xa4 / 4])(scene) &&
                       ((SceneFlagFn)vtable[0xa8 / 4])(scene) &&
                       !text_input_active;
        }
    }
    if ((int)gameplay != previous_gameplay) {
        log_line(gameplay ? "Minecraft 1.2.8 scene: gameplay capture"
                          : "Minecraft 1.2.8 scene: UI cursor");
        previous_gameplay = gameplay;
    }
    if (host_relative_requested != gameplay) {
        host_relative_requested = gameplay;
        mouse_clip_dirty = TRUE;
    }
}

#endif /* WIN32CRAFT_ARM32 client_state */

static void center_mouse_cursor(HWND window)
{
    RECT client;
    POINT center;

    if (!GetClientRect(window, &client)) return;
    center.x = (client.right - client.left) / 2;
    center.y = (client.bottom - client.top) / 2;
    if (ClientToScreen(window, &center)) {
        SetCursorPos(center.x, center.y);
    }
}

static void set_game_relative_actual(BOOL active)
{
    void *platform;
    BYTE *controller;

    if (!game_app_main) return;
    platform = current_app_platform();
    controller = get_hid_controller(platform);
    if (controller) {
        controller[mouse_actual_offset()] = active ? 1 : 0;
        *(int *)(controller + mouse_command_offset()) = 0;
    }
}

static BOOL apply_mouse_clip(HWND window, BOOL recenter)
{
    RECT clip;
    POINT top_left;
    POINT bottom_right;

    if (!GetClientRect(window, &clip)) return FALSE;
    top_left.x = clip.left;
    top_left.y = clip.top;
    bottom_right.x = clip.right;
    bottom_right.y = clip.bottom;
    if (!ClientToScreen(window, &top_left) ||
        !ClientToScreen(window, &bottom_right)) {
        return FALSE;
    }
    clip.left = top_left.x;
    clip.top = top_left.y;
    clip.right = bottom_right.x;
    clip.bottom = bottom_right.y;
    if (!ClipCursor(&clip)) return FALSE;
    if (recenter) center_mouse_cursor(window);
    mouse_clip_dirty = FALSE;
    return TRUE;
}

static void update_mouse_capture(HWND window)
{
    BOOL requested = game_requests_relative_mouse();
    BOOL focused = GetForegroundWindow() == window;
    BOOL enable = requested && focused && !IsIconic(window);

    if (enable) {
        if (!mouse_capture_active) {
            mouse_capture_active = TRUE;
            SetCapture(window);
            while (ShowCursor(FALSE) >= 0) {}
            set_game_relative_actual(TRUE);
            mouse_clip_dirty = TRUE;
            apply_mouse_clip(window, TRUE);
            log_line("relative mouse captured");
        } else {
            set_game_relative_actual(TRUE);
            if (mouse_clip_dirty || GetCapture() != window) {
                SetCapture(window);
                apply_mouse_clip(window, TRUE);
                log_line("relative mouse clip refreshed");
            }
        }
        return;
    }

    if (mouse_capture_active) {
        mouse_capture_active = FALSE;
        set_game_relative_actual(FALSE);
        ClipCursor(NULL);
        if (GetCapture() == window) ReleaseCapture();
        while (ShowCursor(TRUE) < 0) {}
        SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(32512)));
        log_line("relative mouse released");
    }
}


static BOOL append_wide_path(wchar_t *buffer, LPCWSTR suffix)
{
    int current = lstrlenW(buffer);
    int extra = lstrlenW(suffix);
    if (current < 0 || extra < 0 || current + extra >= MAX_PATH) return FALSE;
    lstrcatW(buffer, suffix);
    return TRUE;
}

static BOOL copy_valid_png(LPCWSTR source, LPCWSTR destination)
{
    static const BYTE png_signature[8] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
    };
    BYTE buffer[1024];
    HANDLE input;
    HANDLE output;
    DWORD read_count;
    DWORD written;
    BOOL ok = FALSE;

    input = CreateFileW(source, GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input == INVALID_HANDLE_VALUE) return FALSE;
    if (!ReadFile(input, buffer, sizeof(png_signature), &read_count, NULL) ||
        read_count != sizeof(png_signature) ||
        memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        CloseHandle(input);
        log_line("custom skin selection is not a PNG file");
        return FALSE;
    }

    output = CreateFileW(destination, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) {
        CloseHandle(input);
        return FALSE;
    }
    if (!WriteFile(output, buffer, read_count, &written, NULL) ||
        written != read_count) {
        goto done;
    }
    for (;;) {
        if (!ReadFile(input, buffer, sizeof(buffer), &read_count, NULL)) {
            goto done;
        }
        if (!read_count) {
            ok = TRUE;
            break;
        }
        if (!WriteFile(output, buffer, read_count, &written, NULL) ||
            written != read_count) {
            goto done;
        }
    }

done:
    CloseHandle(output);
    CloseHandle(input);
    return ok;
}

static BOOL line_has_key(const BYTE *line, DWORD length, const char *key)
{
    DWORD index = 0;
    while (key[index]) {
        if (index >= length || line[index] != (BYTE)key[index]) return FALSE;
        ++index;
    }
    return index < length && line[index] == ':';
}

static void copy_bytes(BYTE *destination, DWORD *position,
                       const BYTE *source, DWORD count)
{
    if (count) CopyMemory(destination + *position, source, count);
    *position += count;
}

static BOOL update_custom_skin_options(void)
{
    static const char type_key[] = "game_skintypefull";
    static const char last_key[] = "game_lastcustomskinnew";
    static const BYTE type_line[] =
        "game_skintypefull:Standard_Custom\r\n";
    static const BYTE last_line[] =
        "game_lastcustomskinnew:custom.png\r\n";
    wchar_t path[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    HANDLE file;
    BYTE *input = NULL;
    BYTE *output = NULL;
    DWORD size = 0;
    DWORD read_count = 0;
    DWORD output_position = 0;
    DWORD position = 0;
    DWORD written = 0;
    BOOL wrote_type = FALSE;
    BOOL wrote_last = FALSE;
    BOOL ok = FALSE;

    lstrcpyW(path, game_data_path);
    if (!append_wide_path(path,
            L"\\games\\com.mojang\\minecraftpe\\options.txt")) {
        return FALSE;
    }
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &attributes)) {
        size = attributes.nFileSizeLow;
    }
    if (size > 1024 * 1024) return FALSE;

    input = (BYTE *)HeapAlloc(GetProcessHeap(), 0, size + 1);
    output = (BYTE *)HeapAlloc(GetProcessHeap(), 0, size + 512);
    if (!input || !output) goto done;
    if (size) {
        file = CreateFileW(path, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) goto done;
        if (!ReadFile(file, input, size, &read_count, NULL)) {
            CloseHandle(file);
            goto done;
        }
        CloseHandle(file);
        size = read_count;
    }

    while (position < size) {
        DWORD line_start = position;
        DWORD line_end;
        while (position < size && input[position] != '\n') ++position;
        if (position < size) ++position;
        line_end = position;
        if (line_has_key(input + line_start, line_end - line_start, type_key)) {
            if (!wrote_type) {
                copy_bytes(output, &output_position, type_line,
                           sizeof(type_line) - 1);
                wrote_type = TRUE;
            }
        } else if (line_has_key(input + line_start,
                                line_end - line_start, last_key)) {
            if (!wrote_last) {
                copy_bytes(output, &output_position, last_line,
                           sizeof(last_line) - 1);
                wrote_last = TRUE;
            }
        } else {
            copy_bytes(output, &output_position, input + line_start,
                       line_end - line_start);
        }
    }
    if (output_position && output[output_position - 1] != '\n') {
        static const BYTE newline[] = "\r\n";
        copy_bytes(output, &output_position, newline, 2);
    }
    if (!wrote_type) {
        copy_bytes(output, &output_position, type_line, sizeof(type_line) - 1);
    }
    if (!wrote_last) {
        copy_bytes(output, &output_position, last_line, sizeof(last_line) - 1);
    }

    file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    ok = WriteFile(file, output, output_position, &written, NULL) &&
         written == output_position;
    CloseHandle(file);

done:
    if (input) HeapFree(GetProcessHeap(), 0, input);
    if (output) HeapFree(GetProcessHeap(), 0, output);
    return ok;
}

static void invoke_custom_skin_callback(void *callback)
{
    void **vtable;
    void (__attribute__((thiscall)) *invoke)(void *);

    if (!callback) return;
    vtable = *(void ***)callback;
    if (!vtable || !vtable[2]) return;
    invoke = (void (__attribute__((thiscall)) *)(void *))vtable[2];
    invoke(callback);
}

/* Exact x86 argument layout of AppPlatform's 0.15.10 FileBrowser call.
 * The game passes a 0xC0-byte settings object and a 24-byte MSVC std::string
 * by value, then returns with RET 0xD8.  Declaring the replacement thiscall
 * with the same by-value types makes clang preserve that ABI exactly. */
typedef struct GameFileBrowserSettings01510 {
    BYTE bytes[0xc0];
} GameFileBrowserSettings01510;

typedef struct GameMsvcString01510 {
    union {
        char small[16];
        char *heap;
    } storage;
    DWORD length;
    DWORD capacity;
} GameMsvcString01510;

typedef void (__attribute__((thiscall)) *GameFileBrowser01510Fn)(
    void *, GameFileBrowserSettings01510, GameMsvcString01510);
typedef void (__attribute__((thiscall)) *GameFileBrowserSettingsDtor01510Fn)(
    void *);
typedef void (__cdecl *GameOperatorDelete01510Fn)(void *, SIZE_T, int);
typedef void (__attribute__((thiscall)) *GameFileBrowserCallback01510Fn)(
    void *, void *, const GameMsvcString01510 *);

static GameFileBrowser01510Fn original_file_browser_01510;

static const char *game_msvc_string_data_01510(
    const GameMsvcString01510 *text)
{
    if (!text || text->length > 0x10000u) return NULL;
    if (text->capacity < 16u) return text->storage.small;
    return text->storage.heap;
}

static BOOL game_msvc_string_equals_01510(
    const GameMsvcString01510 *text, const char *expected)
{
    const char *data;
    SIZE_T expected_length;

    if (!expected) return FALSE;
    expected_length = 0;
    while (expected[expected_length]) ++expected_length;
    if (!text || text->length != expected_length) return FALSE;
    data = game_msvc_string_data_01510(text);
    if (!data || IsBadReadPtr(data, expected_length)) return FALSE;
    return memcmp(data, expected, expected_length) == 0;
}

static void destroy_file_browser_arguments_01510(
    GameFileBrowserSettings01510 *settings, GameMsvcString01510 *identifier)
{
    GameFileBrowserSettingsDtor01510Fn destroy_settings;
    GameOperatorDelete01510Fn game_delete;

    if (!game_image) return;
    destroy_settings = (GameFileBrowserSettingsDtor01510Fn)(
        game_image + RVA_01510_FILE_BROWSER_SETTINGS_DTOR);
    game_delete = (GameOperatorDelete01510Fn)(
        game_image + RVA_01510_GAME_OPERATOR_DELETE);

    if (settings) destroy_settings(settings);
    if (identifier && identifier->capacity >= 16u &&
        identifier->storage.heap &&
        !IsBadReadPtr(identifier->storage.heap, 1)) {
        game_delete(identifier->storage.heap,
                    (SIZE_T)identifier->capacity + 1u, 1);
        identifier->storage.heap = NULL;
    }
    if (identifier) {
        identifier->length = 0;
        identifier->capacity = 15;
        identifier->storage.small[0] = 0;
    }
}

static BOOL make_callback_path_string_01510(
    LPCWSTR path, GameMsvcString01510 *text, char **allocated)
{
    int required;
    char *buffer;

    if (!path || !text || !allocated) return FALSE;
    *allocated = NULL;
    ZeroMemory(text, sizeof(*text));
    required = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0,
                                   NULL, NULL);
    if (required <= 1) return FALSE;
    text->length = (DWORD)(required - 1);
    if (text->length < 16u) {
        if (!WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                 text->storage.small, 16, NULL, NULL)) {
            ZeroMemory(text, sizeof(*text));
            return FALSE;
        }
        text->capacity = 15;
        return TRUE;
    }
    buffer = (char *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)required);
    if (!buffer) {
        ZeroMemory(text, sizeof(*text));
        return FALSE;
    }
    if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, buffer, required,
                             NULL, NULL)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        ZeroMemory(text, sizeof(*text));
        return FALSE;
    }
    text->storage.heap = buffer;
    text->capacity = text->length;
    *allocated = buffer;
    return TRUE;
}

static BOOL invoke_file_browser_callback_01510(
    GameFileBrowserSettings01510 *settings, LPCWSTR path)
{
    BYTE *callback;
    void *target;
    void **vtable;
    GameFileBrowserCallback01510Fn invoke;
    GameMsvcString01510 path_string;
    char *allocated = NULL;

    if (!settings || !path || !make_callback_path_string_01510(
            path, &path_string, &allocated)) {
        return FALSE;
    }
    callback = settings->bytes + 0x50;
    if (IsBadReadPtr(callback + 0x24, sizeof(void *))) goto done;
    target = *(void **)(callback + 0x24);
    if (!target || IsBadReadPtr(target, sizeof(void *))) goto done;
    vtable = *(void ***)target;
    if (!vtable || IsBadReadPtr(vtable + 2, sizeof(void *)) || !vtable[2]) {
        goto done;
    }
    invoke = (GameFileBrowserCallback01510Fn)vtable[2];
    invoke(target, settings, &path_string);
    if (allocated) HeapFree(GetProcessHeap(), 0, allocated);
    return TRUE;

done:
    if (allocated) HeapFree(GetProcessHeap(), 0, allocated);
    return FALSE;
}

static BOOL show_world_file_dialog_01510(HWND owner, BOOL save, wchar_t *selected,
                                         DWORD selected_capacity)
{
    OPENFILENAMEW dialog;

    if (!selected || selected_capacity < 2) return FALSE;
    ZeroMemory(selected, selected_capacity * sizeof(wchar_t));
    if (save) lstrcpyW(selected, L"world.mcworld");
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter =
        L"Minecraft World (*.mcworld)\0*.mcworld\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = selected;
    dialog.nMaxFile = selected_capacity;
    dialog.lpstrDefExt = L"mcworld";
    dialog.lpstrTitle = save
        ? L"Export Minecraft World" : L"Import Minecraft World";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                   OFN_HIDEREADONLY |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    return save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
}

/* Early-2016 FilePickerSettings uses the same std::function and string ABI. */
BOOL Win32WorldFileDialogEarly2016(HWND owner, void *settings, BOOL save)
{
    wchar_t selected[MAX_PATH];
    if (!show_world_file_dialog_01510(owner, save, selected, ARRAYSIZE(selected))) {
        log_line("early-2016 world file dialog cancelled");
        return FALSE;
    }
    return invoke_file_browser_callback_01510(settings, selected);
}

static void __attribute__((thiscall, noinline)) win32_file_browser_01510(
    void *self, GameFileBrowserSettings01510 settings,
    GameMsvcString01510 identifier)
{
    static const char import_id[] = "FileBrowser.Rift.Import";
    static const char export_id[] = "FileBrowser.Rift.Export";
    wchar_t selected[MAX_PATH];
    BOOL is_import = game_msvc_string_equals_01510(&identifier, import_id);
    BOOL is_export = game_msvc_string_equals_01510(&identifier, export_id);

    if (!is_import && !is_export) {
        if (original_file_browser_01510) {
            original_file_browser_01510(self, settings, identifier);
        } else {
            destroy_file_browser_arguments_01510(&settings, &identifier);
        }
        return;
    }

    host_relative_requested = FALSE;
    mouse_clip_dirty = TRUE;
    update_mouse_capture(game_window);
    if (show_world_file_dialog_01510(game_window, is_export, selected,
                                     ARRAYSIZE(selected))) {
        if (invoke_file_browser_callback_01510(&settings, selected)) {
            log_line(is_export
                ? "0.15.10 Export World Win32 callback invoked"
                : "0.15.10 Import World Win32 callback invoked");
        } else {
            log_line(is_export
                ? "0.15.10 Export World callback unavailable"
                : "0.15.10 Import World callback unavailable");
        }
    } else {
        log_line(is_export
            ? "0.15.10 Export World Win32 dialog cancelled"
            : "0.15.10 Import World Win32 dialog cancelled");
    }
    destroy_file_browser_arguments_01510(&settings, &identifier);
}

static BOOL install_file_browser_compat_01510(BYTE *image)
{
    static const BYTE expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff,
        0x68, 0x41, 0x48, 0xc9, 0x00
    };
    BYTE *site;
    BYTE *trampoline;
    DWORD old_protection;

    if (!image) return FALSE;
    site = image + RVA_01510_FILE_BROWSER;
    if (memcmp(site, expected, sizeof(expected)) != 0) {
        log_line("0.15.10 FileBrowser hook signature mismatch");
        return FALSE;
    }
    trampoline = (BYTE *)VirtualAlloc(
        NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline) {
        log_line("0.15.10 FileBrowser trampoline allocation failed");
        return FALSE;
    }
    memcpy(trampoline, expected, sizeof(expected));
    trampoline[10] = 0xb8; /* mov eax, site+10 */
    *(DWORD *)(trampoline + 11) = (DWORD)(ULONG_PTR)(site + 10);
    trampoline[15] = 0xff; /* jmp eax */
    trampoline[16] = 0xe0;
    FlushInstructionCache(GetCurrentProcess(), trampoline, 17);
    original_file_browser_01510 = (GameFileBrowser01510Fn)trampoline;
    if (!write_relative_jump(site, win32_file_browser_01510)) {
        original_file_browser_01510 = NULL;
        VirtualFree(trampoline, 0, MEM_RELEASE);
        log_line("0.15.10 FileBrowser hook installation failed");
        return FALSE;
    }
    if (VirtualProtect(trampoline, 32, PAGE_EXECUTE_READ, &old_protection)) {
        FlushInstructionCache(GetCurrentProcess(), trampoline, 32);
    }
    log_line("0.15.10 Import/Export World direct Win32 hook installed");
    return TRUE;
}

/* Replace AppPlatform::pickFile itself rather than its WinRT UI lambda.
 * The original stores the completion object at platform+0x1B0 and invokes
 * callback->vtable[2]() after selection or cancellation. */
static void __attribute__((thiscall)) win32_pick_custom_skin(
    void *platform, void *callback)
{
    wchar_t selected[MAX_PATH];
    wchar_t primary[MAX_PATH];
    wchar_t fallback[MAX_PATH];
    OPENFILENAMEW dialog;
    BOOL copied_primary = FALSE;
    BOOL copied_fallback = FALSE;

    if (platform) {
        *(void **)((BYTE *)platform + 0x1b0) = callback;
    }
    ZeroMemory(selected, sizeof(selected));
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = game_window;
    dialog.lpstrFilter = L"PNG skin (*.png)\0*.png\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = selected;
    dialog.nMaxFile = ARRAYSIZE(selected);
    dialog.lpstrTitle = L"Choose a Minecraft skin PNG";
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |
                   OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    host_relative_requested = FALSE;
    mouse_clip_dirty = TRUE;
    update_mouse_capture(game_window);
    if (!GetOpenFileNameW(&dialog)) {
        log_line("Win32 custom-skin picker cancelled");
        invoke_custom_skin_callback(callback);
        return;
    }

    lstrcpyW(primary, game_data_path);
    lstrcpyW(fallback, game_data_path);
    if (!append_wide_path(primary,
            L"\\games\\com.mojang\\minecraftpe\\custom.png") ||
        !append_wide_path(fallback, L"\\custom.png")) {
        log_line("custom skin destination path too long");
        invoke_custom_skin_callback(callback);
        return;
    }
    copied_primary = copy_valid_png(selected, primary);
    copied_fallback = copy_valid_png(selected, fallback);
    if (!copied_primary && !copied_fallback) {
        log_line("failed to copy selected custom skin PNG");
        invoke_custom_skin_callback(callback);
        return;
    }
    if (!update_custom_skin_options()) {
        log_line("custom skin copied, but options update failed");
        invoke_custom_skin_callback(callback);
        return;
    }
    log_line("Win32 custom skin PNG installed as custom.png");
    invoke_custom_skin_callback(callback);
}

/* Minecraft 1.1.5 moved AppPlatform::pickFile to a different class layout.
 * The outer platform owns the WinRT adapter at +0x278 and the original method
 * stores its completion callback at adapter+0xA8 before scheduling the picker.
 * Keep that observable state, then reuse the synchronous Win32 dialog path. */
static void __attribute__((thiscall)) win32_pick_custom_skin_115(
    void *platform, void *callback)
{
    void *adapter = NULL;
    if (platform && !IsBadReadPtr((BYTE *)platform + 0x278, sizeof(void *))) {
        adapter = *(void **)((BYTE *)platform + 0x278);
    }
    if (adapter && !IsBadReadPtr((BYTE *)adapter + 0xa8, sizeof(void *))) {
        *(void **)((BYTE *)adapter + 0xa8) = callback;
    }
    win32_pick_custom_skin(NULL, callback);
}

/* 1.2.8 keeps the same completion adapter ABI as 1.1.5, but moved the
 * AppPlatform_Winrt adapter from outer+0x278 to outer+0x3B4. */
#ifdef WIN32CRAFT_ARM32
/* ARM32 AppPlatform_Winrt::pickFile replacement (facts platform_fixes.pick_file_fn): plain AAPCS
 * void pickFile(AppPlatform_Winrt *this /r0/, Callback *cb /r1/), reached only through vtable slot 35.
 * The original stores cb at (this+0x3B4)->+0xA8 and queues a WinRT FileOpenPicker lambda that later calls
 * cb->vtable[2](cb); keep the observable state, then run the synchronous Win32 dialog (which invokes the callback). */
static void win32_pick_custom_skin_128(void *platform, void *callback)
{
    void *adapter = NULL;
    char line[160];

    if (platform &&
        !IsBadReadPtr((BYTE *)platform + ARM_PLATFORM_FIXES_PLATFORM_ADAPTER_OFFSET, sizeof(void *))) {
        adapter = *(void **)((BYTE *)platform + ARM_PLATFORM_FIXES_PLATFORM_ADAPTER_OFFSET);
    }
    if (adapter &&
        !IsBadReadPtr((BYTE *)adapter + ARM_PLATFORM_FIXES_ADAPTER_CALLBACK_OFFSET, sizeof(void *))) {
        *(void **)((BYTE *)adapter + ARM_PLATFORM_FIXES_ADAPTER_CALLBACK_OFFSET) = callback;
    }
    wsprintfA(line, "ARM32 pickFile hook called: platform=%p adapter=%p callback=%p", platform, adapter, callback);
    log_line(line);
    win32_pick_custom_skin(NULL, callback);
}
#else
static void __attribute__((thiscall)) win32_pick_custom_skin_128(
    void *platform, void *callback)
{
    void *adapter = NULL;
    if (platform && !IsBadReadPtr((BYTE *)platform + 0x3b4, sizeof(void *))) {
        adapter = *(void **)((BYTE *)platform + 0x3b4);
    }
    if (adapter && !IsBadReadPtr((BYTE *)adapter + 0xa8, sizeof(void *))) {
        *(void **)((BYTE *)adapter + 0xa8) = callback;
    }
    win32_pick_custom_skin(NULL, callback);
}
#endif /* WIN32CRAFT_ARM32 */

static void __attribute__((thiscall)) win32_set_fullscreen_mode(
    void *platform, int mode)
{
    MONITORINFO monitor;
    HMONITOR handle;
    LONG style;

    (void)platform;
    if (!game_window) return;
    if (mode == 1 && !win32_fullscreen_active) {
        saved_window_placement.length = sizeof(saved_window_placement);
        if (!GetWindowPlacement(game_window, &saved_window_placement)) return;
        saved_window_style = GetWindowLongW(game_window, GWL_STYLE);
        saved_window_exstyle = GetWindowLongW(game_window, GWL_EXSTYLE);
        handle = MonitorFromWindow(game_window, MONITOR_DEFAULTTONEAREST);
        ZeroMemory(&monitor, sizeof(monitor));
        monitor.cbSize = sizeof(monitor);
        if (!handle || !GetMonitorInfoW(handle, &monitor)) return;
        style = (saved_window_style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP;
        SetWindowLongW(game_window, GWL_STYLE, style);
        SetWindowPos(game_window, NULL,
                     monitor.rcMonitor.left, monitor.rcMonitor.top,
                     monitor.rcMonitor.right - monitor.rcMonitor.left,
                     monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED |
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        win32_fullscreen_active = TRUE;
        mouse_clip_dirty = TRUE;
        log_line("entered Win32 borderless fullscreen");
    } else if (mode == 0 && win32_fullscreen_active) {
        SetWindowLongW(game_window, GWL_STYLE, saved_window_style);
        SetWindowLongW(game_window, GWL_EXSTYLE, saved_window_exstyle);
        SetWindowPlacement(game_window, &saved_window_placement);
        SetWindowPos(game_window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        win32_fullscreen_active = FALSE;
        mouse_clip_dirty = TRUE;
        log_line("left Win32 borderless fullscreen");
    }
    update_mouse_capture(game_window);
}



typedef HINSTANCE (WINAPI *ShellExecuteWFn)(
    HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, int);

static void __attribute__((thiscall)) win32_open_help_url(void *self)
{
    HMODULE shell32;
    ShellExecuteWFn shell_execute;
    HINSTANCE result;

    (void)self;
    shell32 = LoadLibraryW(L"shell32.dll");
    if (!shell32) {
        log_line("Help URL failed: shell32.dll unavailable");
        return;
    }
    shell_execute = (ShellExecuteWFn)GetProcAddress(shell32, "ShellExecuteW");
    if (!shell_execute) {
        log_line("Help URL failed: ShellExecuteW unavailable");
        return;
    }
    result = shell_execute(game_window, L"open",
                           L"http://aka.ms/minecraftfb",
                           NULL, NULL, 1);
    if ((UINT_PTR)result <= 32)
        log_line("Help URL ShellExecuteW failed");
    else
        log_line("Help URL opened through Win32 ShellExecuteW");
}

static HRESULT WINAPI win32_show_achievements_noop(
    void *user, void *callback, void *state)
{
    (void)user; (void)callback; (void)state;
    log_line("Achievements ignored by Win32 host");
    return E_NOTIMPL;
}

static HRESULT WINAPI win32_show_game_invite_noop(
    void *service_configuration, void *session_template,
    void *session_id, void *invitation_text,
    void *callback, void *state)
{
    (void)service_configuration; (void)session_template;
    (void)session_id; (void)invitation_text;
    (void)callback; (void)state;
    log_line("Invite Player ignored by Win32 host");
    return E_NOTIMPL;
}

static HRESULT WINAPI win32_process_pending_game_ui_noop(BOOL wait)
{
    (void)wait;
    log_line("Xbox TCUI pending request ignored by Win32 host");
    return S_OK;
}

static HRESULT WINAPI win32_show_profile_card_noop(
    HSTRING target_user_xuid, void *context, void *callback)
{
    (void)target_user_xuid;
    (void)context;
    (void)callback;
    log_line("Xbox profile-card request ignored by Win32 host");
    /*
     * Returning success would make the stock wrapper wait forever for a UWP
     * completion callback.  E_NOTIMPL takes its ordinary non-pending path.
     */
    return E_NOTIMPL;
}

static HRESULT WINAPI win32_show_profile_card_for_user_noop(
    void *user, HSTRING target_user_xuid, void *context, void *callback)
{
    (void)user;
    (void)target_user_xuid;
    (void)context;
    (void)callback;
    log_line("Xbox user profile-card request ignored by Win32 host");
    return E_NOTIMPL;
}

#ifdef WIN32CRAFT_ARM32
/* Facts pe_imports.tcui_* / chat_xbox.tcui_*: the three delay-IAT cells are patched BY NAME (ARM order differs from
 * x86: ShowProfileCardUI 0x015121FC, ProcessPendingGameUI 0x01512200, [0x01512204 = 0 terminator, never written],
 * ShowProfileCardUIForUser 0x01512208).  Each cell must still hold its lazy delay-load stub (image + stub rva, bit 0
 * set; the cells are covered by base relocations, so the value is correct at any load address).  The wrapper
 * 0xA3E014 only waits when a Show* result is >= 0 or E_PENDING, hence E_NOTIMPL/E_NOTIMPL/S_OK as on x86. */
static BOOL install_xbox_tcui_noops_128(BYTE *image)
{
    struct Arm32TcuiCell {
        const char *name;
        DWORD cell_rva;
        DWORD stub_value_rva;
        void *hook;
    } cells[3];
    UINT index;
    UINT installed = 0;
    char line[160];

    cells[0].name = "ShowProfileCardUI";
    cells[0].cell_rva = ARM_CHAT_XBOX_TCUI_CELL_SHOW_PROFILE;
    cells[0].stub_value_rva = 0x00b4a2cdu;
    cells[0].hook = arm32_iat_thumb_pointer((void *)win32_show_profile_card_noop);
    cells[1].name = "ProcessPendingGameUI";
    cells[1].cell_rva = ARM_CHAT_XBOX_TCUI_CELL_PROCESS_PENDING;
    cells[1].stub_value_rva = 0x00b4a2e9u;
    cells[1].hook = arm32_iat_thumb_pointer((void *)win32_process_pending_game_ui_noop);
    cells[2].name = "ShowProfileCardUIForUser";
    cells[2].cell_rva = ARM_CHAT_XBOX_TCUI_CELL_SHOW_PROFILE_FOR_USER;
    cells[2].stub_value_rva = 0x00b4a32du;
    cells[2].hook = arm32_iat_thumb_pointer((void *)win32_show_profile_card_for_user_noop);

    if (!IsBadReadPtr(image + ARM_CHAT_XBOX_TCUI_NULL_CELL, sizeof(DWORD)) &&
        *(const DWORD *)(image + ARM_CHAT_XBOX_TCUI_NULL_CELL) != 0) {
        wsprintfA(line, "ARM32 TCUI: terminator cell %08lx holds %08lx instead of 0 (left untouched)",
                  (unsigned long)ARM_CHAT_XBOX_TCUI_NULL_CELL,
                  (unsigned long)*(const DWORD *)(image + ARM_CHAT_XBOX_TCUI_NULL_CELL));
        log_line(line);
    }
    for (index = 0; index < 3; ++index) {
        void **cell = arm32_iat_resolve_cell(image, TRUE, cells[index].name, cells[index].cell_rva);
        if (!cell) continue;
        if (*cell == cells[index].hook) {
            wsprintfA(line, "ARM32 TCUI %s: already hooked", cells[index].name);
            log_line(line);
            ++installed;
        } else if (arm32_patch_cell(cell, image + cells[index].stub_value_rva, cells[index].hook, NULL,
                                    cells[index].name)) {
            ++installed;
        }
    }
    wsprintfA(line, "Minecraft 1.2.8 ARM32 Xbox TCUI no-ops installed: %u of 3 cells", installed);
    log_line(line);
    return installed == 3;
}
#else
static BOOL install_xbox_tcui_noops_128(BYTE *image)
{
    void **slots = (void **)(image + RVA_128_TCUI_PROCESS_PENDING_IAT);
    DWORD old_protection;
    DWORD ignored;
    if (!VirtualProtect(slots, 4 * sizeof(*slots), PAGE_READWRITE,
                        &old_protection)) return FALSE;
    slots[0] = win32_process_pending_game_ui_noop;
    slots[1] = win32_show_profile_card_noop;
    slots[3] = win32_show_profile_card_for_user_noop;
    VirtualProtect(slots, 4 * sizeof(*slots), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), slots, 4 * sizeof(*slots));
    log_line("Minecraft 1.2.8 Xbox TCUI hooks disabled");
    return TRUE;
}
#endif

#ifdef WIN32CRAFT_ARM32
/* Cross-unit hook (defined in api_compat.c by unit hstring_native): lets the FMOD/HSTRING side be served by the same
 * fake WinRT objects as the game's own activation calls. */
typedef HRESULT (*Win32CraftActivationFn)(LPCWSTR class_name, const GUID *iid, void **result);
extern Win32CraftActivationFn Win32CraftActivationDispatcher;

/* Installs every by-name IAT / delay-IAT hook of the ARM32 host (PORT_RULES contract).  Returns FALSE when a mandatory
 * hook (activation redirect, D3D11CreateDevice) could not be installed.  Not installed on ARM on purpose:
 * install_vccorlib_compat (the desktop vccorlib140.dll has real bodies; the IBox hooks would break it) and
 * install_create_file2_compat (CreateFile2 is native on 8.1/10). */
static BOOL install_arm32_iat_hooks(BYTE *image)
{
    BOOL activation_ok;
    BOOL device_ok;
    BOOL optional_ok = TRUE;
    char line[200];
    static BOOL installed_once;
    static BOOL installed_result;

    if (installed_once) {
        /* install_activation_redirect re-initialises the fake objects: a second pass must not touch anything */
        log_line("ARM32 IAT hooks: already installed - repeating the previous result");
        return installed_result;
    }
    if (!image) {
        log_line("ARM32 IAT hooks: image base is null");
        return FALSE;
    }
    if (!game_image) game_image = image;
    if (!host_is_128 || host_is_115 || host_is_116 || host_is_windows7) {
        log_line("ARM32 IAT hooks: host flags were not set for 1.2.8 - forcing host_is_128 only");
        host_is_128 = TRUE;
        host_is_115 = FALSE;
        host_is_116 = FALSE;
        host_is_windows7 = FALSE;
    }
    if (!arm32_feature_level_resolved) Win32CraftParseHostOptions();
    log_pointer("ARM32 IAT hooks: game image base", image);

    if (!install_exception_trace(image, ARM_CHAT_XBOX_EXCEPTION_TRACE_IAT)) optional_ok = FALSE;
    activation_ok = install_activation_redirect(image, ARM_PE_IMPORTS_ACTIVATION_FACTORY_IAT);
    if (activation_ok) {
        Win32CraftActivationDispatcher = win32_get_activation_factory;
        log_line("ARM32: Win32CraftActivationDispatcher set to the host activation factory");
    }
    if (!install_d3d_compile_trace(image, ARM_PE_IMPORTS_D3D_COMPILE_IAT)) optional_ok = FALSE;
    if (!install_d3d_preprocess_trace(image, ARM_GFX_COMPAT_IAT_D3D_PREPROCESS_REFLECT)) optional_ok = FALSE;
    device_ok = install_d3d11_device_compat(image, ARM_PE_IMPORTS_D3D11_CREATE_DEVICE_IAT);
    if (!install_xbox_tcui_noops_128(image)) optional_ok = FALSE;
    log_line("ARM32: vccorlib compat and CreateFile2 hooks are intentionally not installed");

    wsprintfA(line, "ARM32 IAT hooks: activation=%s d3d11=%s optional=%s",
              activation_ok ? "ok" : "FAILED", device_ok ? "ok" : "FAILED", optional_ok ? "ok" : "some missing");
    log_line(line);
    installed_result = activation_ok && device_ok;
    installed_once = TRUE;
    return installed_result;
}
#endif

static BOOL install_xbox_tcui_noops(
    BYTE *image, SIZE_T process_pending_rva, SIZE_T show_profile_rva)
{
    void **process_pending =
        (void **)(image + process_pending_rva);
    void **show_profile =
        (void **)(image + show_profile_rva);
    DWORD old_protection;
    DWORD ignored;

    if (!VirtualProtect(
            process_pending, 2 * sizeof(*process_pending),
            PAGE_READWRITE, &old_protection)) {
        log_line("VirtualProtect Xbox TCUI IAT failed");
        return FALSE;
    }
    *process_pending = win32_process_pending_game_ui_noop;
    *show_profile = win32_show_profile_card_noop;
    VirtualProtect(
        process_pending, 2 * sizeof(*process_pending),
        old_protection, &ignored);
    FlushInstructionCache(
        GetCurrentProcess(), process_pending, 2 * sizeof(*process_pending));
    log_line("Xbox TCUI profile-card hooks disabled");
    return TRUE;
}


/*
 * UWP edit boxes call AppPlatform_Winrt::showKeyboard/hideKeyboard before
 * receiving CharacterReceived events.  In an HWND process the original
 * implementation tries to obtain Windows.UI.ViewManagement.InputPane and
 * crashes because there is no CoreWindow view.  Keep Minecraft's own
 * "keyboard visible" byte in sync, but replace only that WinRT boundary.
 */
static void __attribute__((thiscall)) win32_show_text_keyboard(
    void *self, void *text, int max_length, int multiline,
    int secure, const void *position)
{
    (void)text;
    (void)max_length;
    (void)multiline;
    (void)secure;
    (void)position;
    if (self) *((BYTE *)self + 9) = TRUE;
    text_input_active = TRUE;
    pending_high_surrogate = 0;
    log_line("Win32 text input activated (InputPane show bypassed)");
}

static void __attribute__((thiscall)) win32_hide_text_keyboard(void *self)
{
    if (self) *((BYTE *)self + 9) = FALSE;
    text_input_active = FALSE;
    pending_high_surrogate = 0;
    suppress_next_t_character = FALSE;
    log_line("Win32 text input deactivated (InputPane hide bypassed)");
}

/* 1.1.5 wraps InputPane and fullscreen requests in command objects rather
 * than invoking the old AppPlatform methods directly.  The show/hide
 * commands have no stack arguments; the fullscreen command captures its
 * requested mode at this+4. */
static void __attribute__((thiscall)) win32_show_text_keyboard_115(void *command)
{
    (void)command;
    text_input_active = TRUE;
    pending_high_surrogate = 0;
    log_line("Minecraft 1.1.5 Win32 text input activated");
}

static void __attribute__((thiscall)) win32_hide_text_keyboard_115(void *command)
{
    (void)command;
    text_input_active = FALSE;
    pending_high_surrogate = 0;
    suppress_next_t_character = FALSE;
    log_line("Minecraft 1.1.5 Win32 text input deactivated");
}

static void __attribute__((thiscall)) win32_set_fullscreen_command_115(
    void *command)
{
    int mode = 0;
    if (command && !IsBadReadPtr((BYTE *)command + 4, sizeof(mode)))
        mode = *(int *)((BYTE *)command + 4);
    win32_set_fullscreen_mode(NULL, mode);
}

static BOOL install_win32_platform_fixes_115(BYTE *image)
{
    static const BYTE picker_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xd8, 0xf2, 0x2a, 0x01
    };
    static const BYTE show_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x45, 0x0b, 0x2c, 0x01
    };
    static const BYTE hide_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xe1, 0x0b, 0x2c, 0x01
    };
    static const BYTE fullscreen_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xef, 0x0c, 0x2c, 0x01
    };
    BYTE *picker_site = image + RVA_115_PICK_FILE;
    BYTE *show_site = image + RVA_115_SHOW_TEXT_KEYBOARD;
    BYTE *hide_site = image + RVA_115_HIDE_TEXT_KEYBOARD;
    BYTE *fullscreen_site = image + RVA_115_FULLSCREEN_COMMAND;

    if (memcmp(picker_site, picker_expected, 10) ||
        !write_relative_jump(picker_site, win32_pick_custom_skin_115)) {
        log_line("Minecraft 1.1.5 custom-skin picker hook failed");
        return FALSE;
    }
    if (memcmp(show_site, show_expected, 10) ||
        !write_relative_jump(show_site, win32_show_text_keyboard_115)) {
        log_line("Minecraft 1.1.5 text keyboard show hook failed");
        return FALSE;
    }
    if (memcmp(hide_site, hide_expected, 10) ||
        !write_relative_jump(hide_site, win32_hide_text_keyboard_115)) {
        log_line("Minecraft 1.1.5 text keyboard hide hook failed");
        return FALSE;
    }
    if (memcmp(fullscreen_site, fullscreen_expected, 10) ||
        !write_relative_jump(
            fullscreen_site, win32_set_fullscreen_command_115)) {
        log_line("Minecraft 1.1.5 fullscreen command hook failed");
        return FALSE;
    }
    log_line("installed Minecraft 1.1.5 Win32 picker/fullscreen/text-input fixes");
    return TRUE;
}

#ifdef WIN32CRAFT_ARM32
/* The only 1.2.8 code hook: AppPlatform_Winrt vtable (0x013695A4) slot 35 = pickFile.  Pure data-cell patch, no code is
 * rewritten (facts platform_fixes.pick_file_vtable_cell).  The cell must still hold real_base + 0x5830F8 + 1. */
static BOOL install_win32_platform_fixes_128(BYTE *image)
{
    void **cell;

    if (!image) return FALSE;
    cell = (void **)(image + ARM_PLATFORM_FIXES_PICK_FILE_VTABLE_CELL);
    if (!IsBadReadPtr(cell, sizeof(*cell)) && *cell == (void *)win32_pick_custom_skin_128) {
        log_line("Minecraft 1.2.8 ARM32 custom-skin picker already installed");
        return TRUE;
    }
    if (!arm32_check_bytes(image + ARM_PLATFORM_FIXES_PICK_FILE_HOOK_SITE_BYTES,
                           ARM_PLATFORM_FIXES_PICK_FILE_HOOK_SITE_BYTES_EXPECTED,
                           sizeof(ARM_PLATFORM_FIXES_PICK_FILE_HOOK_SITE_BYTES_EXPECTED),
                           "AppPlatform_Winrt::pickFile entry") ||
        !arm32_patch_cell(cell, ARM_CODE(image, ARM_PLATFORM_FIXES_PICK_FILE_FN),
                          (void *)win32_pick_custom_skin_128, NULL,
                          "AppPlatform_Winrt::pickFile vtable[35]")) {
        log_line("Minecraft 1.2.8 ARM32 custom-skin picker hook failed");
        return FALSE;
    }
    log_line("Minecraft 1.2.8 ARM32 Win32 custom-skin picker installed");
    return TRUE;
}

/* Contract entry point (arm32/PORT_RULES.md): pickFile cell, getAsset overrides (directory + zip strategies) and the Xbox
 * sign-in string patches.  Call it once, before the game's AppMain / screen controllers are constructed.  Every piece is
 * optional (the game runs without any of them), so each failure is logged with the cell/bytes it found and the function
 * only reports FALSE for an unusable image pointer.  The TCUI delay-IAT no-ops are installed by install_arm32_iat_hooks. */
static BOOL install_arm32_platform_fixes(BYTE *image)
{
    BOOL picker;
    BOOL overrides;
    BOOL xbox;
    char line[200];

    if (!image) {
        log_line("ARM32 platform fixes: image pointer is NULL");
        return FALSE;
    }
    wsprintfA(line, "ARM32 platform fixes: installing on image %p", (void *)image);
    log_line(line);
    picker = install_win32_platform_fixes_128(image);
    overrides = install_128_zip_resource_overrides(image);
    xbox = patch_xbox_actions_noop(image);
    patch_chat_textbox_compat(image);
    wsprintfA(line, "ARM32 platform fixes: pickFile=%s getAsset-overrides=%s xbox-signin=%s",
              picker ? "ok" : "FAILED", overrides ? "ok" : "FAILED", xbox ? "ok" : "FAILED");
    log_line(line);
    return TRUE;
}
#else
static BOOL install_win32_platform_fixes_128(BYTE *image)
{
    static const BYTE picker_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xf8, 0xcf, 0x4c, 0x01
    };
    BYTE *picker_site = image + RVA_128_PICK_FILE;

    if (memcmp(picker_site, picker_expected, sizeof(picker_expected)) != 0 ||
        !write_relative_jump(picker_site, win32_pick_custom_skin_128)) {
        log_line("Minecraft 1.2.8 custom-skin picker hook failed");
        return FALSE;
    }
    log_line("Minecraft 1.2.8 Win32 custom-skin picker installed");
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 */

static BOOL install_win32_platform_fixes(BYTE *image)
{
    static const BYTE picker_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xfb, 0x54, 0xd0, 0x00
    };
    static const BYTE fullscreen_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x3f, 0x59, 0xd0, 0x00
    };
    static const BYTE show_keyboard_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x55, 0x53, 0xd0, 0x00
    };
    static const BYTE hide_keyboard_expected[10] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xa1, 0x53, 0xd0, 0x00
    };

    if (memcmp(image + 0x00600110, picker_expected,
               sizeof(picker_expected)) != 0 ||
        !write_relative_jump(image + 0x00600110,
                             win32_pick_custom_skin)) {
        log_line("Win32 custom-skin hook installation failed");
        return FALSE;
    }
    if (memcmp(image + 0x00601310, fullscreen_expected,
               sizeof(fullscreen_expected)) != 0 ||
        !write_relative_jump(image + 0x00601310,
                             win32_set_fullscreen_mode)) {
        log_line("Win32 fullscreen hook installation failed");
        return FALSE;
    }
    if (memcmp(image + 0x005ff810, show_keyboard_expected,
               sizeof(show_keyboard_expected)) != 0 ||
        !write_relative_jump(image + 0x005ff810,
                             win32_show_text_keyboard)) {
        log_line("Win32 text keyboard show hook installation failed");
        return FALSE;
    }
    if (memcmp(image + 0x005ff980, hide_keyboard_expected,
               sizeof(hide_keyboard_expected)) != 0 ||
        !write_relative_jump(image + 0x005ff980,
                             win32_hide_text_keyboard)) {
        log_line("Win32 text keyboard hide hook installation failed");
        return FALSE;
    }
    log_line("installed Win32 picker/fullscreen/text-input fixes");
    return TRUE;
}

static void sync_hid_pointer_scale(float scale_x, float scale_y)
{
    void *platform;
    BYTE *controller;
    char line[128];

    if (!game_app_main) return;
    platform = current_app_platform();
    controller = get_hid_controller(platform);
    if (!controller) return;
    /*
     * 0.15.10 moved the pointer-transform fields.  HWND coordinates are
     * already physical pixels, so leave the game's initialized transform
     * intact until the remaining HID layout is mapped.
     */
    wsprintfA(line, "HID pointer scale synchronized: %d/%d",
              (int)(scale_x * 1000.0f), (int)(scale_y * 1000.0f));
    log_line(line);
}

#ifdef WIN32CRAFT_ARM32
/* DXGI part of the ARM32 resize: facts gfx_device.resize_sequence.  OMSetRenderTargets(0) + Flush, drop the host's RTV
 * reference, game ReleaseTargets (does NOT release the back buffer at renderer+0x124), release + NULL renderer+0x124 (DXGI
 * rejects ResizeBuffers while it is alive), ResizeBuffers(0, w, h, UNKNOWN, 0) (flags must be 0 on the host chain),
 * game InitTargets.  On a failed ResizeBuffers the old-size targets are rebuilt so the renderer is never left without views. */
static BOOL arm32_gfx_resize_targets(UINT width, UINT height)
{
    Arm32GfxReleaseTargetsFn release_targets =
        (Arm32GfxReleaseTargetsFn)ARM_CODE(arm32_gfx_image(), ARM_GFX_DEVICE_RELEASE_TARGETS);
    BYTE *renderer = (BYTE *)game_renderer;
    ID3D11Texture2D *backbuffer;
    HRESULT result;
    UINT old_width = arm32_gfx_width;
    UINT old_height = arm32_gfx_height;

    ID3D11DeviceContext_OMSetRenderTargets(game_context, 0, NULL, NULL);
    ID3D11DeviceContext_Flush(game_context);
    if (game_target) {
        ID3D11RenderTargetView_Release(game_target);
        game_target = NULL;
    }
    release_targets(game_renderer);
    backbuffer = *(ID3D11Texture2D **)(renderer + ARM_GFX_DEVICE_RENDERER_BACKBUFFER);
    if (backbuffer) {
        *(ID3D11Texture2D **)(renderer + ARM_GFX_DEVICE_RENDERER_BACKBUFFER) = NULL;
        ID3D11Texture2D_Release(backbuffer);
    }

    result = IDXGISwapChain_ResizeBuffers(game_swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    log_hresult("IDXGISwapChain::ResizeBuffers(HWND)", result);
    if (FAILED(result)) {
        if (result == ARM32_GFX_HR_DEVICE_REMOVED || result == ARM32_GFX_HR_DEVICE_RESET) {
            arm32_gfx_note_device_lost(result, "IDXGISwapChain::ResizeBuffers");
            return FALSE;
        }
        if (old_width && old_height) {
            log_line("gfx: ResizeBuffers failed; rebuilding the render targets at the previous size");
            arm32_gfx_recreate_targets(old_width, old_height, "resize recovery");
        }
        return FALSE;
    }
    if (!arm32_gfx_recreate_targets(width, height, "resize")) {
        log_line("HWND resize: target recreation failed");
        return FALSE;
    }
    return TRUE;
}

/* ARM32 resize_game_window (facts gfx_device.resize_sequence / appmain port_instructions 4): FL9_x size clamp, DXGI resize,
 * platform+0x330/+0x334 + owner+0x94/+0x98 mirroring, MinecraftGame vf23 resize(w, h, float scale in s0) and vf24
 * windowSizeChanged(w, h). */
static BOOL resize_game_window(HWND window, UINT width, UINT height)
{
    BYTE *image = arm32_gfx_image();
    char line[192];
    BOOL resized;

    (void)window;
    if (!width || !height || !game_swap_chain || !game_renderer || !game_context || !game_resource_owner) {
        return FALSE;
    }
    if (arm32_gfx_device_lost) return FALSE;
    if (arm32_gfx_resizing) {
        log_line("resize ignored: a resize is already in progress");
        return FALSE;
    }
    wsprintfA(line, "resize requested: %ux%u", width, height);
    log_line(line);
    arm32_gfx_clamp_size(&width, &height, "resize");
    if (width == arm32_gfx_width && height == arm32_gfx_height && game_target) {
        log_line("resize: swap-chain size unchanged");
        return TRUE;
    }

    arm32_gfx_resizing = TRUE;
    resized = arm32_gfx_resize_targets(width, height);
    arm32_gfx_resizing = FALSE;
    if (!resized) {
        SetWindowTextW(game_window, L"Win32Craft 1.2.8");
        return FALSE;
    }
    {
        HRESULT result = IDXGISwapChain_Present(game_swap_chain, 0, 0);
        log_hresult("HWND resize validation Present", result);
        if (FAILED(result)) return FALSE;
    }
    log_line("HWND swap chain targets resized and rebound");

    if (game_app_main) {
        BYTE *app = (BYTE *)game_app_main;
        void *platform = *(void **)(app + ARM_APPMAIN_LAYOUT); /* AppMain+0x1c */
        void *game = *(void **)(app + 0x20);                   /* AppMain+0x20 */

        if (platform) {
            *(int *)((BYTE *)platform + ARM_APPMAIN_PLATFORM_SCREEN_WIDTH) = (int)width;
            *(int *)((BYTE *)platform + ARM_APPMAIN_PLATFORM_SCREEN_HEIGHT) = (int)height;
        }
        arm32_gfx_mirror_client_size((int)width, (int)height);
        if (game && !IsBadReadPtr(game, sizeof(void *)) && *(void ***)game &&
            !IsBadReadPtr(*(void ***)game, 0x64)) {
            void **vtable = *(void ***)game;
            void *resize = vtable[0x5c / sizeof(void *)];
            void *changed = vtable[0x60 / sizeof(void *)];

            if (resize == ARM_CODE(image, ARM_APPMAIN_GAME_VF23_SETSIZE) &&
                changed == ARM_CODE(image, ARM_APPMAIN_GAME_VF24_WINDOWSIZECHANGED)) {
                /* facts gfx_device.game_resize_vslots: the 4th argument is a float in s0 (hard-float), not an int in r3 */
                ((void (*)(void *, int, int, float))resize)(game, (int)width, (int)height, 0.0f);
                ((void (*)(void *, int, int))changed)(game, (int)width, (int)height);
            } else {
                log_pointer("resize: MinecraftGame vtable slot 23 is not MinecraftGame::resize, skipped", resize);
                log_pointer("resize: MinecraftGame vtable slot 24", changed);
            }
        }
    }
    mouse_clip_dirty = TRUE;
    log_line("Minecraft 1.2.8 HWND target and UI size synchronized");
    return TRUE;
}
#else /* !WIN32CRAFT_ARM32 */
static BOOL resize_game_window(HWND window, UINT width, UINT height)
{
    BYTE *image = game_image ? game_image : (BYTE *)GetModuleHandleW(NULL);
    GameReleaseTargetsFn release_targets = image
        ? (GameReleaseTargetsFn)(
            image + (host_is_128
                ? RVA_128_RELEASE_TARGETS
                : (host_is_115 ? RVA_115_RELEASE_TARGETS
                               : RVA_RELEASE_TARGETS))) : NULL;
    GameInitTargetsFn init_targets = image
        ? (GameInitTargetsFn)(
            image + (host_is_128
                ? RVA_128_INIT_TARGETS
                : (host_is_115 ? 0x0070dc00
                               : RVA_INIT_TARGETS))) : NULL;
    void *platform;
    void *game;
    HRESULT result;
    char line[192];

    struct {
        UINT width;
        UINT height;
        BYTE reserved_08[0x11];
        BYTE stereo;
        BYTE reserved_1a[2];
        IDXGISwapChain *swap_chain;
    } display;

    (void)window;
    if (!width || !height || !game_swap_chain || !game_renderer ||
        !game_context) {
        return FALSE;
    }

    wsprintfA(line, "resize requested: %ux%u", width, height);
    log_line(line);

    /*
     * Renderer::setWindowMetrics owns the UWP SwapChainPanel path and cannot
     * size an HWND swap chain reliably.  Release the 0.15.10 renderer views,
     * resize the existing DXGI buffers, and invoke the game's platform-neutral
     * target constructor with the same display descriptor used at startup.
     */
    if (!game_resource_owner || !release_targets || !init_targets) {
        log_line("HWND resize: owner/target helpers unavailable");
        return FALSE;
    }
    ID3D11DeviceContext_OMSetRenderTargets(game_context, 0, NULL, NULL);
    ID3D11DeviceContext_Flush(game_context);
    if (game_target) {
        ID3D11RenderTargetView_Release(game_target);
        game_target = NULL;
    }
    release_targets(game_renderer);
    /*
     * Renderer::releaseWindowSizeDependentResources releases the three
     * views at +0x108/+0x10C/+0x110, while the texture obtained from
     * IDXGISwapChain::GetBuffer is owned separately at +0x114.  DXGI rejects
     * ResizeBuffers with DXGI_ERROR_INVALID_CALL until that last reference is
     * dropped as well.
     */
    {
        SIZE_T backbuffer_offset = host_is_128
            ? 0x124 : (host_is_115 ? 0x120 : 0x114);
        ID3D11Texture2D *backbuffer = *(ID3D11Texture2D **)(
            (BYTE *)game_renderer + backbuffer_offset);
        if (backbuffer) {
            *(ID3D11Texture2D **)(
                (BYTE *)game_renderer + backbuffer_offset) = NULL;
            ID3D11Texture2D_Release(backbuffer);
        }
    }

    result = IDXGISwapChain_ResizeBuffers(
        game_swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    log_hresult("IDXGISwapChain::ResizeBuffers(HWND)", result);
    if (FAILED(result)) {
        SetWindowTextW(game_window, host_is_128
            ? L"Win32Craft 1.2.8" : L"Win32Craft 1.1.5");
        return FALSE;
    }

    ZeroMemory(&display, sizeof(display));
    display.width = width;
    display.height = height;
    display.swap_chain = game_swap_chain;
    init_targets(game_resource_owner, &display, game_renderer);

    game_target = *(ID3D11RenderTargetView **)(
        (BYTE *)game_renderer +
        (host_is_128 ? 0x118
                     : (host_is_115 ? 0x114 : RENDERER_RTV_OFFSET)));
    if (!game_target) {
        log_line("HWND resize: target recreation returned null RTV");
        SetWindowTextW(game_window, host_is_128
            ? L"Win32Craft 1.2.8" : L"Win32Craft 1.1.5");
        return FALSE;
    }
    ID3D11RenderTargetView_AddRef(game_target);

    result = IDXGISwapChain_Present(game_swap_chain, 0, 0);
    log_hresult("HWND resize validation Present", result);
    if (FAILED(result)) return FALSE;
    log_line("HWND swap chain targets resized and rebound");

    /* HWND mouse coordinates are already physical client pixels. */
    if (!host_is_115 && !host_is_128) {
        sync_hid_pointer_scale(1.0f, 1.0f);
    }

    if (game_app_main) {
        if (host_is_128) {
            platform = *(void **)((BYTE *)game_app_main + 0x1c);
            game = *(void **)((BYTE *)game_app_main + 0x20);
            if (platform) {
                *(int *)((BYTE *)platform + 0x330) = (int)width;
                *(int *)((BYTE *)platform + 0x334) = (int)height;
            }
            if (game && !IsBadReadPtr(*(void ***)game, 0x64)) {
                GameResizeFn resize = (GameResizeFn)(
                    *(void ***)game)[0x5c / sizeof(void *)];
                GameWindowSizeChangedFn changed = (GameWindowSizeChangedFn)(
                    *(void ***)game)[0x60 / sizeof(void *)];
                if (resize) call_game_resize(
                    (void *)resize, game, width, height, 0);
                if (changed) changed(game, width, height);
            }
            mouse_clip_dirty = TRUE;
            log_line("Minecraft 1.2.8 HWND target and UI size synchronized");
            return TRUE;
        } else if (host_is_115) {
            Game115AppResizeFn resize_app =
                (Game115AppResizeFn)(
                    image + RVA_115_APP_MAIN_RESIZE);
            resize_app(game_app_main, &xaml_size_source, NULL);
            mouse_clip_dirty = TRUE;
            log_line(
                "Minecraft 1.1.5 HWND target and UI size synchronized");
            return TRUE;
        }
        platform = *(void **)((BYTE *)game_app_main + 4);
        game = *(void **)((BYTE *)game_app_main + 8);
        if (platform) {
            *(int *)((BYTE *)platform + 0x25c) = (int)width;
            *(int *)((BYTE *)platform + 0x260) = (int)height;
        }
        if (game && !IsBadReadPtr(*(void ***)game, 0x58)) {
            GameResizeFn resize =
                (GameResizeFn)(*(void ***)game)[0x50 / sizeof(void *)];
            GameWindowSizeChangedFn window_size_changed =
                (GameWindowSizeChangedFn)(
                    *(void ***)game)[0x54 / sizeof(void *)];
            if (resize) {
                call_game_resize(
                    (void *)resize, game, (int)width, (int)height, 0);
            }
            if (window_size_changed) {
                window_size_changed(
                    game, (int)width, (int)height);
            }
        }
    }
    mouse_clip_dirty = TRUE;
    log_line("HWND renderer targets, viewport, UI and HID size synchronized");
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 resize_game_window */

/*
 * AppPlatform_Winrt normally turns CoreWindow pointer callbacks into this
 * polymorphic MouseItem and moves it into Minecraft's input queue. The UWP
 * poller is disabled in the EXE because an HWND process has no CoreWindow, so
 * reproduce that final, platform-independent queue operation here.
 *
 * The 0.15.10 factory constructs the MouseItem vtable and layout, so the HWND
 * bridge only supplies the five platform-independent fields.
 */
#ifdef WIN32CRAFT_ARM32
/* facts input.make_mouse_event / input.enqueue_input: MouseItem factory 0x62a128 through the 00_core bridge (32-bit
 * action/pressed/payload, 16-bit x/y locals), then EnqueueInput(adapter, &slot).  Wheel = action 4 with the signed
 * delta byte in `pressed`, payload 0, x = y = 0 (same as the stock PointerWheelChanged handler 0x625a50). */
/* The game works in swap-chain-buffer pixels (platform+0x330/+0x334 = arm32_gfx_get_backbuffer_size).  The buffer is smaller
 * than the client area when the window exceeds the feature-level texture limit (2048 on FL9_1/9_2), and DXGI then stretches
 * it to the window: absolute pointer coordinates (client pixels) must be scaled by buffer/client.  Identity otherwise. */
static void arm32_scale_pointer_to_buffer(int *x, int *y)
{
    static BOOL logged_scale;
    UINT buffer_width = 0;
    UINT buffer_height = 0;
    RECT client;
    LONG client_width;
    LONG client_height;

    arm32_gfx_get_backbuffer_size(&buffer_width, &buffer_height);
    if (!buffer_width || !buffer_height || !game_window || !GetClientRect(game_window, &client)) return;
    client_width = client.right - client.left;
    client_height = client.bottom - client.top;
    if (client_width <= 0 || client_height <= 0) return;
    if ((UINT)client_width == buffer_width && (UINT)client_height == buffer_height) return;
    if (!logged_scale) {
        char line[160];
        logged_scale = TRUE;
        wsprintfA(line, "mouse: client area %ldx%ld differs from the %ux%u swap-chain buffer; pointer coordinates are scaled",
                  (long)client_width, (long)client_height, buffer_width, buffer_height);
        log_line(line);
    }
    *x = (int)((float)*x * (float)buffer_width / (float)client_width + 0.5f);
    *y = (int)((float)*y * (float)buffer_height / (float)client_height + 0.5f);
}

static BOOL queue_mouse_event(int x, int y, DWORD payload,
                              BYTE action, BYTE pressed)
{
    static unsigned logged_events;
    static BOOL logged_disabled;
    void *event = NULL;
    BYTE *controller;
    void *input_handler;
    WORD event_x;
    WORD event_y;
    char line[128];

    if (!input_events_enabled) {
        if (!logged_disabled) {
            log_line("Win32 input injection is not available");
            logged_disabled = TRUE;
        }
        return FALSE;
    }
    controller = arm32_input_target(&input_handler);
    if (!controller) return FALSE;

    if (!payload) arm32_scale_pointer_to_buffer(&x, &y);   /* payload 0 = absolute client coordinates (wheel passes 0,0) */
    if (payload) {
        if (x < SHRT_MIN) x = SHRT_MIN;
        if (y < SHRT_MIN) y = SHRT_MIN;
        if (x > SHRT_MAX) x = SHRT_MAX;
        if (y > SHRT_MAX) y = SHRT_MAX;
    } else {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > 0xffff) x = 0xffff;
        if (y > 0xffff) y = 0xffff;
    }

    event_x = (WORD)x;
    event_y = (WORD)y;
    call_game_make_mouse_event(game_image + ARM_INPUT_MAKE_MOUSE_EVENT, &event, &action, &pressed,
                               &payload, &event_x, &event_y);
    if (!event || IsBadReadPtr(event, ARM_INPUT_MOUSE_EVENT_LAYOUT) ||
        *((BYTE *)event + 4) != 0) {
        log_line("mouse event factory returned an invalid event");
        return FALSE;
    }
    arm32_enqueue_input(input_handler, event);

    if (logged_events < 12) {
        wsprintfA(line, "mouse event: x=%d y=%d action=%u pressed=%u payload=%u",
                  x, y, (unsigned)action, (unsigned)pressed, (unsigned)payload);
        log_line(line);
        ++logged_events;
    }
    return TRUE;
}

/* facts touch_host.make_multitouch_item: void** f(void** out r0, const int* buttonId r1, const int* data r2, const short* x r3,
 * const short* y [sp], const BYTE* pointerId [sp+4]) -> r0.  Plain AAPCS (two stack arguments).  The item is 0x10 bytes, type byte 1. */
static void **call_game_make_multitouch_event(
    void *function, void **output, const int *button_id, const int *data,
    const short *x, const short *y, const BYTE *pointer_id)
{
    typedef void **(*MultitouchFactoryFn)(
        void **, const int *, const int *, const short *, const short *, const BYTE *);
    return ((MultitouchFactoryFn)ARM32_THUMB_FUNCTION(function))(output, button_id, data, x, y, pointer_id);
}

/* Queue one MultitouchItem (HID item type 1) for pointer slot 0..11 - exactly what the stock WinRT pointer handlers
 * (0x624a64 pressed (1,1) / 0x625084 moved (0,0) / 0x625398 released (1,0)) produce for a touch contact.  x,y are CLIENT pixels:
 * they are scaled to the swap-chain buffer (same unit as the MouseItem path) and clamped to int16.  Processing the item happens in
 * the game's per-frame processHIDItem (Multitouch::feed); the first press makes TouchMapper::tick report and flips the game's input
 * mode to Touch (InputHandler::tick), which is what draws the on-screen touch controls.  Never used for mouse input. */
static BOOL queue_touch_event(int slot, int button_id, int data, int x, int y)
{
    static unsigned logged_events;
    static BOOL logged_disabled;
    void *event = NULL;
    BYTE *controller;
    void *input_handler;
    int client_x = x;
    int client_y = y;
    int button32 = button_id;
    int data32 = data;
    short event_x;
    short event_y;
    BYTE pointer_id;
    char line[192];

    if (!input_events_enabled) {
        if (!logged_disabled) {
            log_line("ARM32 touch: input injection is not available (input_events_enabled is FALSE), touch events are dropped");
            logged_disabled = TRUE;
        }
        return FALSE;
    }
    if (slot < 0 || slot >= ARM_TOUCH_HOST_MULTITOUCH_MAX_POINTERS) return FALSE;
    controller = arm32_input_target(&input_handler);
    if (!controller) return FALSE;

    arm32_scale_pointer_to_buffer(&x, &y);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > SHRT_MAX) x = SHRT_MAX;
    if (y > SHRT_MAX) y = SHRT_MAX;
    event_x = (short)x;
    event_y = (short)y;
    pointer_id = (BYTE)slot;
    call_game_make_multitouch_event(game_image + ARM_TOUCH_HOST_MAKE_MULTITOUCH_ITEM, &event, &button32, &data32,
                                    &event_x, &event_y, &pointer_id);
    if (!event || IsBadReadPtr(event, ARM_TOUCH_HOST_MULTITOUCH_ITEM_SIZE) ||
        *((BYTE *)event + 4) != ARM_TOUCH_HOST_MULTITOUCH_ITEM_TYPE) {
        log_line("ARM32 touch: multitouch event factory returned an invalid event (queue_touch_event)");
        return FALSE;
    }
    arm32_enqueue_input(input_handler, event);

    if (logged_events < 40 || arm32_input_verbose) {
        wsprintfA(line, "ARM32 touch: item queued slot=%d btn=%d data=%d client=(%d,%d) buffer=(%d,%d)",
                  slot, button_id, data, client_x, client_y, x, y);
        log_line(line);
        ++logged_events;
    }
    return TRUE;
}
#else /* WIN32CRAFT_ARM32 */
static BOOL queue_mouse_event(int x, int y, DWORD payload,
                              BYTE action, BYTE pressed)
{
    static unsigned logged_events;
    static BOOL logged_disabled;
    GameEnqueueInputFn enqueue_input;
    void *make_mouse_event;
    void *event = NULL;
    void *platform;
    BYTE *controller;
    void *input_handler;
    WORD event_x;
    WORD event_y;
    char line[128];

    if (!input_events_enabled) {
        if (!logged_disabled) {
            log_line("Win32 input injection is not available");
            logged_disabled = TRUE;
        }
        return FALSE;
    }
    if (!game_image || !game_app_main) return FALSE;
    platform = current_app_platform();
    if (!platform) return FALSE;
    controller = get_hid_controller(platform);
    input_handler = controller ? *(void **)(controller + 0x10) : NULL;
    if (!input_handler) return FALSE;

    if (payload) {
        if (x < SHRT_MIN) x = SHRT_MIN;
        if (y < SHRT_MIN) y = SHRT_MIN;
        if (x > SHRT_MAX) x = SHRT_MAX;
        if (y > SHRT_MAX) y = SHRT_MAX;
    } else {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > 0xffff) x = 0xffff;
        if (y > 0xffff) y = 0xffff;
    }

    event_x = (WORD)x;
    event_y = (WORD)y;
    make_mouse_event = game_image + (host_is_128
        ? RVA_128_MOUSE_EVENT_FACTORY
        : (host_is_115 ? RVA_115_MOUSE_EVENT_FACTORY
                       : RVA_MOUSE_EVENT_FACTORY));
    enqueue_input = (GameEnqueueInputFn)(game_image + (host_is_128
        ? RVA_128_ENQUEUE_INPUT
        : (host_is_115 ? RVA_115_ENQUEUE_INPUT : RVA_ENQUEUE_INPUT)));
    call_game_make_mouse_event(make_mouse_event, &event, &action, &pressed,
                               &payload, &event_x, &event_y);
    if (!event || IsBadReadPtr(event, 0x14) ||
        *((BYTE *)event + 4) != 0) {
        log_line("mouse event factory returned an invalid event");
        return FALSE;
    }
    enqueue_input(input_handler, event);

    if (logged_events < 12) {
        wsprintfA(line, "mouse event: x=%d y=%d action=%u pressed=%u",
                  x, y, (unsigned)action, (unsigned)pressed);
        log_line(line);
        ++logged_events;
    }
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 (input_hid: queue_mouse_event) */

/* 0.15.10 normally receives Enter from the XAML TextBox KeyDown callback.
 * A raw CoreWindow/HID KeyItem is not the same callback and therefore does
 * not submit chat in the HWND rehost.  Track the active native ChatScreen via
 * its focus/unfocus vtable entries, then invoke its own submit routine after
 * the frame has consumed all TextItems queued earlier in the Windows message
 * batch. */
static GameChatScreenVoidFn original_chat_focus_01510;
static GameChatScreenVoidFn original_chat_unfocus_01510;

static void __attribute__((thiscall)) chat_focus_hook_01510(void *screen)
{
    active_chat_screen_01510 = screen;
    if (original_chat_focus_01510) original_chat_focus_01510(screen);
}

static void __attribute__((thiscall)) chat_unfocus_hook_01510(void *screen)
{
    if (original_chat_unfocus_01510) original_chat_unfocus_01510(screen);
    if (active_chat_screen_01510 == screen) active_chat_screen_01510 = NULL;
    InterlockedExchange(&pending_chat_submit_01510, 0);
    suppress_return_keyup_01510 = FALSE;
}

static BOOL install_chat_submit_compat_01510(BYTE *image)
{
    void **vtable = (void **)(image + RVA_01510_CHAT_SCREEN_VTABLE);
    DWORD old_protection;
    DWORD ignored;

    if (!image ||
        vtable[6] != image + RVA_01510_CHAT_SCREEN_FOCUS ||
        vtable[7] != image + RVA_01510_CHAT_SCREEN_UNFOCUS) {
        log_line("Minecraft 0.15.10 ChatScreen vtable signature mismatch");
        return FALSE;
    }
    if (!VirtualProtect(vtable + 6, 2 * sizeof(void *), PAGE_READWRITE,
                        &old_protection)) {
        log_line("VirtualProtect Minecraft 0.15.10 ChatScreen vtable failed");
        return FALSE;
    }
    original_chat_focus_01510 = (GameChatScreenVoidFn)vtable[6];
    original_chat_unfocus_01510 = (GameChatScreenVoidFn)vtable[7];
    vtable[6] = chat_focus_hook_01510;
    vtable[7] = chat_unfocus_hook_01510;
    VirtualProtect(vtable + 6, 2 * sizeof(void *), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), vtable + 6,
                          2 * sizeof(void *));
    log_line("Minecraft 0.15.10 native ChatScreen Enter-submit hook installed");
    return TRUE;
}

static void request_chat_submit_01510(void)
{
    if (!active_chat_screen_01510 ||
        IsBadReadPtr(active_chat_screen_01510, sizeof(void *)) ||
        *(void **)active_chat_screen_01510 !=
            game_image + RVA_01510_CHAT_SCREEN_VTABLE) {
        log_line("Minecraft 0.15.10 Enter ignored: active ChatScreen unavailable");
        return;
    }
    InterlockedExchange(&pending_chat_submit_01510, 1);
}

static void process_chat_submit_01510(void)
{
    GameChatScreenVoidFn submit;
    void *screen;

    if (host_is_115 || host_is_128 ||
        InterlockedExchange(&pending_chat_submit_01510, 0) == 0) return;
    screen = active_chat_screen_01510;
    if (!screen || IsBadReadPtr(screen, sizeof(void *)) ||
        *(void **)screen != game_image + RVA_01510_CHAT_SCREEN_VTABLE) {
        log_line("Minecraft 0.15.10 deferred Enter lost its ChatScreen");
        return;
    }
    submit = (GameChatScreenVoidFn)(
        game_image + RVA_01510_CHAT_SCREEN_SUBMIT);
    submit(screen);
    log_line("Minecraft 0.15.10 chat submitted through native ChatScreen");
}

#ifdef WIN32CRAFT_ARM32
/* facts input.map_virtual_key / make_key_event / hid_key_held_table.  The stock WinRT KeyDown/KeyUp handlers
 * (0x625b10 / 0x625d64) do: mapped = mapVirtualKey(vk - 8) for vk 8..0xde, make KeyCodeItem(mapped, state), enqueue,
 * then controller[0xc9 + mapped] = state.  The host does the same.  Stock extras that are deliberately not repeated:
 *  - key-down is skipped when the held byte is already set (0x627188) and key-up is only emitted when it is set
 *    (0x625db2): the host's win32_key_down[] auto-repeat filter makes the two equivalent for single keys, and an
 *    unmatched extra key event is harmless to the game's keyboard state;
 *  - Backspace (key-down) and Enter (key-up) additionally queue a TextItem "\b" / "\n" while HIDController+0x24 == 0;
 *    the host gets those from WM_CHAR through queue_character_event. */
static BOOL queue_keyboard_event(BYTE virtual_key, DWORD state)
{
    static unsigned logged_events;
    Arm32MapVirtualKeyFn map_virtual_key;
    BYTE *controller;
    void *input_handler;
    BYTE mapped_key;
    DWORD state_value = state ? 1u : 0u;
    void *event = NULL;
    char line[112];

    if (!input_events_enabled || !virtual_key) return FALSE;
    controller = arm32_input_target(&input_handler);
    if (!controller) return FALSE;

    /* The mapper is a `tbh` table indexed by (VirtualKey - 8) with 0xd7 entries: anything else would jump out of it. */
    if (virtual_key < ARM_INPUT_VK_RANGE_GUARD || virtual_key > 0xde) return FALSE;
    map_virtual_key = (Arm32MapVirtualKeyFn)ARM_CODE(game_image, ARM_INPUT_MAP_VIRTUAL_KEY);
    mapped_key = (BYTE)map_virtual_key((unsigned)virtual_key - ARM_INPUT_VK_RANGE_GUARD);
    if (!mapped_key) return FALSE;
    call_game_make_key_event_115(game_image + ARM_INPUT_MAKE_KEY_EVENT, &event, &mapped_key,
                                 &state_value);
    if (!event || IsBadReadPtr(event, ARM_INPUT_KEY_EVENT_LAYOUT) ||
        ((BYTE *)event)[4] != 2) {
        log_line("Minecraft 1.2.8 key event factory returned an invalid event");
        return FALSE;
    }
    arm32_enqueue_input(input_handler, event);
    controller[ARM_INPUT_HID_KEY_HELD_TABLE + mapped_key] = state_value ? 1 : 0;

    if (logged_events < 24) {
        wsprintfA(line, "keyboard event: key=%u mapped=%u state=%u",
                  (unsigned)virtual_key, (unsigned)mapped_key, (unsigned)state_value);
        log_line(line);
        ++logged_events;
    }
    return TRUE;
}

/* facts input.make_text_event / hid_text_sequence_byte: TextItem(std::string text, primary 0, sequence).  The
 * 16-byte-SSO std::string layout is unchanged on ARM (size +0x10, capacity +0x14).  Every UTF-8 sequence of one code
 * point fits the inline buffer; the factory copies it, so a stack temporary is fine. */
static BOOL queue_character_event(DWORD codepoint)
{
    typedef struct MsvcSmallString115 {
        char storage[16];
        DWORD length;
        DWORD capacity;
    } MsvcSmallString115;

    static unsigned logged_text_events;
    MsvcSmallString115 text_115;
    char utf8[5];
    unsigned length;
    BYTE primary_flag = 0;
    BYTE sequence_flag;
    void *event = NULL;
    BYTE *controller;
    void *input_handler;
    char line[160];

    if ((codepoint < 0x20 && codepoint != 0x08 && codepoint != 0x0d) ||
        codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
        !game_image || !game_app_main) {
        return FALSE;
    }
    /* The stock client never produces a CR text item: its Enter key queues the string "\n" (static std::string at
     * 0x1581a10, initialised to one 0x0a byte by 0x52128) and Backspace the string "\b" (0xd8b030), see
     * HIDController::onKeyDown 0x625b10 / onKeyUp 0x625d64.  WM_CHAR delivers 0x0d for Enter, so send what the game
     * itself would have queued. */
    if (codepoint == 0x0d) codepoint = 0x0a;
    controller = arm32_input_target(&input_handler);
    if (!controller) return FALSE;

    if (codepoint <= 0x7f) {
        utf8[0] = (char)codepoint;
        length = 1;
    } else if (codepoint <= 0x7ff) {
        utf8[0] = (char)(0xc0 | (codepoint >> 6));
        utf8[1] = (char)(0x80 | (codepoint & 0x3f));
        length = 2;
    } else if (codepoint <= 0xffff) {
        utf8[0] = (char)(0xe0 | (codepoint >> 12));
        utf8[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        utf8[2] = (char)(0x80 | (codepoint & 0x3f));
        length = 3;
    } else {
        utf8[0] = (char)(0xf0 | (codepoint >> 18));
        utf8[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        utf8[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        utf8[3] = (char)(0x80 | (codepoint & 0x3f));
        length = 4;
    }
    utf8[length] = 0;

    ZeroMemory(&text_115, sizeof(text_115));
    memcpy(text_115.storage, utf8, length);
    text_115.length = length;
    text_115.capacity = 15;
    /* Match HIDController::queueTextItem (0x627108): byte +0x1f0 is copied into TextItem+0x21 and incremented. */
    sequence_flag = controller[ARM_INPUT_HID_TEXT_SEQUENCE_BYTE];
    controller[ARM_INPUT_HID_TEXT_SEQUENCE_BYTE] = (BYTE)(sequence_flag + 1);
    call_game_make_text_event_115(game_image + ARM_INPUT_MAKE_TEXT_EVENT, &event, &text_115,
                                  &primary_flag, &sequence_flag);
    if (!event || IsBadReadPtr(event, ARM_INPUT_TEXT_EVENT_LAYOUT) || ((BYTE *)event)[4] != 3) {
        log_line("text event factory returned an invalid event");
        return FALSE;
    }
    arm32_enqueue_input(input_handler, event);
    if (logged_text_events < 32) {
        wsprintfA(line,
            "Minecraft 1.2.8 text event queued: codepoint=U+%04lX bytes=%u sequence=%u",
            (unsigned long)codepoint, length, (unsigned)sequence_flag);
        log_line(line);
        ++logged_text_events;
    }
    return TRUE;
}
#else /* WIN32CRAFT_ARM32 */
/* Reuse the 0.15.10 HIDController key handlers. They perform the exact
 * VirtualKey mapping, construct KeyItem, enqueue it, and update held state. */
static BOOL queue_keyboard_event(BYTE virtual_key, DWORD state)
{
    static unsigned logged_events;
    GameKeyHandlerFn key_handler;
    GameEnqueueInputFn enqueue_input;
    void *platform;
    BYTE *controller;
    char line[112];

    if (!input_events_enabled || !virtual_key ||
        !game_image || !game_app_main) return FALSE;
    platform = current_app_platform();
    if (!platform) return FALSE;
    controller = get_hid_controller(platform);
    if (!controller || !*(void **)(controller + 0x10)) return FALSE;
    if (host_is_115 || host_is_128) {
        Game115MapVirtualKeyFn map_virtual_key =
            (Game115MapVirtualKeyFn)(
                game_image + (host_is_128
                    ? RVA_128_MAP_VIRTUAL_KEY : RVA_115_MAP_VIRTUAL_KEY));
        BYTE mapped_key;
        void *event = NULL;

        /*
         * HIDController subtracts the first Windows::System::VirtualKey
         * value before entering this jump table.  Win32 VK values use the
         * same numeric assignments for the supported keyboard range.
         */
        if (virtual_key < 8 || virtual_key > 0xde) return FALSE;
        mapped_key = (BYTE)map_virtual_key((int)virtual_key - 8);
        if (!mapped_key) return FALSE;
        call_game_make_key_event_115(
            game_image + (host_is_128
                ? RVA_128_KEY_EVENT_FACTORY : RVA_115_KEY_EVENT_FACTORY),
            &event, &mapped_key, &state);
        if (!event || IsBadReadPtr(event, 0x10) ||
            ((BYTE *)event)[4] != 2) {
            log_line(
                "Minecraft 1.1.5 key event factory returned an invalid event");
            return FALSE;
        }
        enqueue_input =
            (GameEnqueueInputFn)(
                game_image + (host_is_128
                    ? RVA_128_ENQUEUE_INPUT : RVA_115_ENQUEUE_INPUT));
        enqueue_input(*(void **)(controller + 0x10), event);
        /*
         * Match HIDController's held-key suppression table.  The native
         * callback indexes it with the mapped key at object offset +0xbf.
         */
        controller[(host_is_128 ? 0xc9 : 0xbf) + mapped_key] = state ? 1 : 0;
    } else {
        key_handler = (GameKeyHandlerFn)(
            game_image + (state
                ? RVA_KEY_DOWN_HANDLER : RVA_KEY_UP_HANDLER));
        key_handler(controller, (int)virtual_key);
    }

    if (logged_events < 24) {
        wsprintfA(line, "keyboard event: key=%u state=%u",
                  (unsigned)virtual_key, (unsigned)state);
        log_line(line);
        ++logged_events;
    }
    return TRUE;
}

/* The real CharacterReceived path converts text to UTF-8 and calls the
 * 0.15.10 factory. It allocates a 0x24-byte TextItem of type 3
 * and constructs the embedded MSVC std::string used by the UI controls. */
static BOOL queue_character_event(DWORD codepoint)
{
    typedef struct MsvcSmallString115 {
        char storage[16];
        DWORD length;
        DWORD capacity;
    } MsvcSmallString115;

    static unsigned logged_text_events;
    GameEnqueueInputFn enqueue_input;
    MsvcSmallString115 text_115;
    char utf8[5];
    unsigned length;
    BYTE primary_flag = 0;
    BYTE sequence_flag;
    void *event = NULL;
    void *platform;
    BYTE *controller;
    void *input_handler;
    char line[160];

    if ((!host_is_115 && !host_is_128 && codepoint == 0x0d) ||
        (codepoint < 0x20 && codepoint != 0x08 &&
         codepoint != 0x0d) ||
        codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
        !game_image || !game_app_main) {
        return FALSE;
    }
    platform = current_app_platform();
    if (!platform) return FALSE;
    controller = get_hid_controller(platform);
    input_handler = controller ? *(void **)(controller + 0x10) : NULL;
    if (!input_handler) return FALSE;

    if (codepoint <= 0x7f) {
        utf8[0] = (char)codepoint;
        length = 1;
    } else if (codepoint <= 0x7ff) {
        utf8[0] = (char)(0xc0 | (codepoint >> 6));
        utf8[1] = (char)(0x80 | (codepoint & 0x3f));
        length = 2;
    } else if (codepoint <= 0xffff) {
        utf8[0] = (char)(0xe0 | (codepoint >> 12));
        utf8[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        utf8[2] = (char)(0x80 | (codepoint & 0x3f));
        length = 3;
    } else {
        utf8[0] = (char)(0xf0 | (codepoint >> 18));
        utf8[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        utf8[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        utf8[3] = (char)(0x80 | (codepoint & 0x3f));
        length = 4;
    }
    utf8[length] = 0;

    enqueue_input = (GameEnqueueInputFn)(game_image + (host_is_128
        ? RVA_128_ENQUEUE_INPUT
        : (host_is_115 ? RVA_115_ENQUEUE_INPUT : RVA_ENQUEUE_INPUT)));
    if (host_is_115 || host_is_128) {
        /* MSVC 19.x x86 std::string uses a 16-byte small-string buffer,
         * followed by size and capacity. Every UTF-8 character emitted from
         * WM_CHAR fits in that inline representation. */
        ZeroMemory(&text_115, sizeof(text_115));
        memcpy(text_115.storage, utf8, length);
        text_115.length = length;
        text_115.capacity = 15;
        /* Match HIDController::onCharacterReceived exactly: byte +0x1e0
         * is copied into TextItem+0x21 and incremented for the next event. */
        sequence_flag = controller[host_is_128 ? 0x1f0 : 0x1e0];
        controller[host_is_128 ? 0x1f0 : 0x1e0] =
            (BYTE)(sequence_flag + 1);
        call_game_make_text_event_115(
            game_image + (host_is_128
                ? RVA_128_TEXT_EVENT_FACTORY : RVA_115_TEXT_EVENT_FACTORY),
            &event, &text_115, &primary_flag, &sequence_flag);
    } else {
        call_game_make_text_event(
            game_image + RVA_TEXT_EVENT_FACTORY,
            &event, utf8, &primary_flag);
    }
    if (!event || IsBadReadPtr(event, 0x24) || ((BYTE *)event)[4] != 3) {
        log_line("text event factory returned an invalid event");
        return FALSE;
    }
    enqueue_input(input_handler, event);
    if ((host_is_115 || host_is_128) && logged_text_events < 32) {
        wsprintfA(line,
            "Minecraft 1.1.5 text event queued: codepoint=U+%04lX bytes=%u sequence=%u",
            (unsigned long)codepoint, length, (unsigned)sequence_flag);
        log_line(line);
        ++logged_text_events;
    }
    return TRUE;
}
#endif /* WIN32CRAFT_ARM32 (input_hid: queue_keyboard_event, queue_character_event) */

static void handle_win32_key(WPARAM virtual_key, BOOL pressed)
{
    BYTE key;
    if (!input_events_enabled || virtual_key == 0 || virtual_key > 0xff) {
        return;
    }
    key = (BYTE)virtual_key;

    if (pressed) {
        /* Match the UWP path: suppress Windows autorepeat while held. */
        if (win32_key_down[key]) return;
        win32_key_down[key] = 1;
        queue_keyboard_event(key, 1);
    } else {
        win32_key_down[key] = 0;
        queue_keyboard_event(key, 0);
    }
}

static void release_all_win32_keys(void)
{
    unsigned key;
    for (key = 1; key < ARRAYSIZE(win32_key_down); ++key) {
        if (win32_key_down[key]) {
            win32_key_down[key] = 0;
            queue_keyboard_event((BYTE)key, 0);
        }
    }
    pending_high_surrogate = 0;
    text_ctrl_down = FALSE;
    legacy_clipboard_handled_key = 0;
    suppress_next_t_character = FALSE;
}

static void handle_win32_character(WPARAM value)
{
    WORD unit = (WORD)value;

    /*
     * Minecraft 0.15.10 uses a separate XAML TextBox KeyDown callback for
     * submission.  The HWND replacement invokes ChatScreen directly, while
     * the WM_CHAR carriage return must remain out of the TextItem stream or it
     * is rendered as a replacement/control glyph.  1.1.5 has a newer ABI.
     */
    if (!host_is_115 && !host_is_128 && unit == 0x0d) {
        pending_high_surrogate = 0;
        return;
    }

    if (suppress_next_t_character) {
        /* TranslateMessage produces one WM_CHAR for the same physical T key.
         * The key event opens chat; forwarding that character would also type
         * the layout-dependent letter into the freshly opened text box. */
        suppress_next_t_character = FALSE;
        return;
    }
    if (!text_input_active) {
        pending_high_surrogate = 0;
        return;
    }

    if (unit >= 0xd800 && unit <= 0xdbff) {
        pending_high_surrogate = unit;
        return;
    }
    if (unit >= 0xdc00 && unit <= 0xdfff && pending_high_surrogate) {
        DWORD codepoint = 0x10000u +
            (((DWORD)pending_high_surrogate - 0xd800u) << 10) +
            ((DWORD)unit - 0xdc00u);
        pending_high_surrogate = 0;
        queue_character_event(codepoint);
        return;
    }
    pending_high_surrogate = 0;
    queue_character_event((DWORD)unit);
}

static void queue_pointer_position(LPARAM lparam)
{
    int x = (short)LOWORD(lparam);
    int y = (short)HIWORD(lparam);

    if (mouse_capture_active) {
        RECT client;
        int center_x;
        int center_y;

        GetClientRect(game_window, &client);
        center_x = (client.right - client.left) / 2;
        center_y = (client.bottom - client.top) / 2;
        x -= center_x;
        y -= center_y;
        if (x || y) {
            queue_mouse_event(x, y, 1, 0, 0);
            center_mouse_cursor(game_window);
        }
        return;
    }

    /*
     * payload=0 selects Mouse::feed's absolute-coordinate path.  The UWP
     * direct-pointer callback uses payload=1 because its x/y fields are
     * relative deltas paired with a separately maintained cursor position.
     */
    queue_mouse_event(x, y, 0, 0, 0);
}

#ifdef WIN32CRAFT_ARM32
/* ARM32 (unit bootstrap_main): the D3D11 device runs at feature level 9_1 by default (facts fl91), whose texture / render
 * target limit is 2048 (4096 at 9_3, 8192 at 10_x, 16384 at 11_x).  The swap chain is also clamped by resize_game_window,
 * but a window the user can stretch past the limit would only show a letterboxed/scaled image, so WM_GETMINMAXINFO caps the
 * maximum tracking / maximized window size at "client area == limit".  The limit comes from the feature level the game's
 * InitDevice stored in owner+0xb8 (facts gfx_device owner layout); before the device exists the 9_1 limit is assumed. */
#ifndef WM_GETMINMAXINFO
#define WM_GETMINMAXINFO 0x0024U
#endif
typedef struct Arm32MinMaxInfo {
    POINT ptReserved;
    POINT ptMaxSize;
    POINT ptMaxPosition;
    POINT ptMinTrackSize;
    POINT ptMaxTrackSize;
} Arm32MinMaxInfo;
extern BOOL WINAPI AdjustWindowRectEx(RECT *, DWORD, BOOL, DWORD);

static UINT arm32_window_texture_limit(void)
{
    UINT level = 0;

    if (game_resource_owner && !IsBadReadPtr((BYTE *)game_resource_owner + 0xb8, sizeof(UINT)))
        level = *(UINT *)((BYTE *)game_resource_owner + 0xb8);
    if (level >= 0xb000u) return 16384u;
    if (level >= 0xa000u) return 8192u;
    if (level >= 0x9300u) return 4096u;
    return 2048u;
}

static void arm32_clamp_min_max_info(HWND window, Arm32MinMaxInfo *info)
{
    static UINT logged_limit;
    UINT limit = arm32_window_texture_limit();
    LONG style = window ? GetWindowLongW(window, GWL_STYLE) : 0;
    LONG exstyle = window ? GetWindowLongW(window, GWL_EXSTYLE) : 0;
    RECT frame;
    LONG max_width;
    LONG max_height;
    char line[160];

    if (!info) return;
    /* WM_GETMINMAXINFO is sent before WM_NCCREATE: the style may not be stored yet */
    if (!style) style = (LONG)WS_OVERLAPPEDWINDOW;
    frame.left = 0;
    frame.top = 0;
    frame.right = (LONG)limit;
    frame.bottom = (LONG)limit;
    if (!AdjustWindowRectEx(&frame, (DWORD)style, FALSE, (DWORD)exstyle)) {
        frame.left = 0;
        frame.top = 0;
        frame.right = (LONG)limit;
        frame.bottom = (LONG)limit;
    }
    max_width = frame.right - frame.left;
    max_height = frame.bottom - frame.top;
    if (info->ptMaxTrackSize.x > max_width) info->ptMaxTrackSize.x = max_width;
    if (info->ptMaxTrackSize.y > max_height) info->ptMaxTrackSize.y = max_height;
    if (info->ptMaxSize.x > max_width) info->ptMaxSize.x = max_width;
    if (info->ptMaxSize.y > max_height) info->ptMaxSize.y = max_height;
    if (logged_limit != limit) {
        logged_limit = limit;
        wsprintfA(line, "window size limit: client area capped at %ux%u (outer %ldx%ld) for the current feature level",
                  limit, limit, (long)max_width, (long)max_height);
        log_line(line);
    }
}
#endif

#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 touch input.  Facts: touch_game.* (InputHandler::tick picks the input mode from the kind of HID item it receives),
 * touch_host.* (item factory ABI, slots 0..11), touch_impl.* (mapper list / default-mode cell).  Design in arm32/facts/touch_host.md.
 *
 * Problem: the host only ever produced MouseItems, so the game stayed in Mouse mode and never drew the on-screen touch controls
 * (TouchMapper::render runs only while the input mode is Touch).  Windows turned touch into emulated mouse messages.
 * Fix: WM_POINTER* messages of PT_TOUCH contacts are consumed (no DefWindowProc, so no mouse emulation) and queued as
 * MultitouchItems through the game's own factory; the game then switches to Touch mode by itself.  Real mouse/keyboard input
 * switches back ("last used wins").  Every decision is logged with the prefixes "ARM32 input:", "ARM32 touch:", "ARM32 osk:".
 * ====================================================================================================== */
extern HWND WINAPI GetFocus(void);
extern BOOL WINAPI ScreenToClient(HWND, POINT *);
extern int WINAPI GetSystemMetrics(int);
extern LPARAM WINAPI GetMessageExtraInfo(void);
extern HWND WINAPI FindWindowW(LPCWSTR, LPCWSTR);
extern BOOL WINAPI IsWindowVisible(HWND);
extern BOOL WINAPI GetWindowRect(HWND, RECT *);
extern HWND WINAPI GetDesktopWindow(void);
extern BOOL WINAPI SetPropW(HWND, LPCWSTR, HANDLE);
extern BOOL WINAPI CreateProcessW(LPCWSTR, LPWSTR, void *, void *, BOOL, DWORD, void *, LPCWSTR, void *, void *);

#define ARM32_WM_TOUCH                0x0240u
#define ARM32_WM_CANCELMODE           0x001Fu
#define ARM32_WM_POINTERUPDATE        0x0245u
#define ARM32_WM_POINTERDOWN          0x0246u
#define ARM32_WM_POINTERUP            0x0247u
#define ARM32_WM_POINTERCAPTURECHANGED 0x024Cu
#define ARM32_WM_SYSCOMMAND           0x0112u
#define ARM32_SC_CLOSE                0xF060u
#define ARM32_PT_TOUCH                2u
#define ARM32_PT_PEN                  3u
#define ARM32_PT_MOUSE                4u
#define ARM32_PT_TOUCHPAD             5u
#define ARM32_POINTER_FLAG_INCONTACT  0x0004u      /* HIWORD(wParam) of WM_POINTER* */
#define ARM32_POINTER_FLAG_CANCELED   0x8000u
#define ARM32_TOUCHEVENTF_MOVE        0x0001u
#define ARM32_TOUCHEVENTF_DOWN        0x0002u
#define ARM32_TOUCHEVENTF_UP          0x0004u
#define ARM32_TOUCHEVENTF_PALM        0x0080u
#define ARM32_SM_CMOUSEBUTTONS        43
#define ARM32_SM_MOUSEPRESENT         19
#define ARM32_SM_DIGITIZER            94
#define ARM32_SM_MAXIMUMTOUCHES       95
#define ARM32_SM_CONVERTIBLESLATEMODE 0x2003
#define ARM32_SM_REMOTESESSION        0x1000
#define ARM32_SRC_NONE                0
#define ARM32_SRC_MOUSEKB             1
#define ARM32_SRC_TOUCH               2
#define ARM32_LEGACY_TOUCH_ID         0xFFFFFFF0u
#define ARM32_TOUCH_SLOTS             ARM_TOUCH_HOST_MULTITOUCH_MAX_POINTERS

typedef BOOL (WINAPI *Arm32GetPointerTypeFn)(UINT32, UINT32 *);
typedef BOOL (WINAPI *Arm32GetPointerInfoFn)(UINT32, void *);
typedef BOOL (WINAPI *Arm32RegisterTouchWindowFn)(HWND, ULONG);
typedef BOOL (WINAPI *Arm32GetTouchInputInfoFn)(HANDLE, UINT, void *, int);
typedef BOOL (WINAPI *Arm32CloseTouchInputHandleFn)(HANDLE);
typedef BOOL (WINAPI *Arm32SetWindowFeedbackSettingFn)(HWND, UINT, DWORD, UINT32, const void *);
typedef UINT (WINAPI *Arm32GetRawInputDeviceListFn)(void *, UINT *, UINT);
typedef UINT (WINAPI *Arm32GetRawInputDeviceInfoWFn)(HANDLE, UINT, void *, UINT *);
typedef BOOL (WINAPI *Arm32IsMouseInPointerEnabledFn)(void);

typedef struct Arm32InputApi {
    Arm32GetPointerTypeFn get_pointer_type;
    Arm32GetPointerInfoFn get_pointer_info;
    Arm32RegisterTouchWindowFn register_touch_window;
    Arm32GetTouchInputInfoFn get_touch_input_info;
    Arm32CloseTouchInputHandleFn close_touch_input_handle;
    Arm32SetWindowFeedbackSettingFn set_window_feedback_setting;
    Arm32GetRawInputDeviceListFn get_raw_input_device_list;
    Arm32GetRawInputDeviceInfoWFn get_raw_input_device_info;
    Arm32IsMouseInPointerEnabledFn is_mouse_in_pointer_enabled;
} Arm32InputApi;

typedef struct Arm32TouchSlot {
    UINT32 id;          /* Windows pointer id (or WM_TOUCH dwID) */
    BOOL active;
    BOOL dirty;         /* a newer position than sent_x/sent_y is waiting for arm32_touch_flush_moves */
    int x, y;           /* last known position, CLIENT pixels */
    int sent_x, sent_y; /* last position queued for the game */
    DWORD down_tick;
} Arm32TouchSlot;

typedef struct Arm32TouchStats {
    unsigned down, up, move_messages, moves_flushed, slots_full, canceled, capture_lost, queue_failed;
    unsigned legacy_from_touch, legacy_mouse_messages, pointer_mouse, pointer_pen, pointer_other, wm_touch_messages;
    unsigned source_switches;
} Arm32TouchStats;

static Arm32InputApi arm32_api;
static BOOL arm32_api_resolved;
static BOOL arm32_input_probed;
static int arm32_touch_api_effective;          /* 0 = touch handling inactive, else ARM32_TOUCHAPI_POINTER / WMTOUCH / LEGACY */
static int arm32_input_source = ARM32_SRC_MOUSEKB;
static int arm32_input_initial_source = ARM32_SRC_MOUSEKB;
static BOOL arm32_pointer_touch_seen;          /* a PT_TOUCH WM_POINTERDOWN reached window_proc at least once */
static BOOL arm32_legacy_touch_used;           /* a touch-sourced mouse message was converted to a contact before any WM_POINTER finger was seen */
static BOOL arm32_wm_touch_registered;         /* auto mode also called RegisterTouchWindow: WM_TOUCH is the multi-touch fallback when no WM_POINTER arrives */
static Arm32TouchSlot arm32_touch_slots[ARM32_TOUCH_SLOTS];
static Arm32TouchStats arm32_stats;
static Arm32TouchStats arm32_stats_logged;
static DWORD arm32_last_touch_tick;
static BOOL arm32_have_mouse_position;
static int arm32_last_mouse_screen_x;
static int arm32_last_mouse_screen_y;
static UINT32 arm32_nontouch_ids[4];
static unsigned arm32_nontouch_count;
static unsigned arm32_input_pass;
static unsigned arm32_mapper_reset_pass;       /* != 0: TouchMapper reset due when arm32_input_pass reaches it */
static BOOL arm32_input_game_ready_logged;

static BOOL arm32_touch_any_active(void);
static void arm32_touch_release_all(const char *why);
static BOOL arm32_osk_visible_now(void);

/* case-insensitive substring test on a UTF-16 string */
static BOOL arm32_wcs_contains_ci(const wchar_t *text, const wchar_t *needle)
{
    UINT start;
    UINT index;

    for (start = 0; text[start]; ++start) {
        for (index = 0; needle[index] && text[start + index] &&
                        host_ascii_lower_w(text[start + index]) == host_ascii_lower_w(needle[index]); ++index) {
        }
        if (!needle[index]) return TRUE;
    }
    return FALSE;
}

static void arm32_input_resolve_apis(void)
{
    HMODULE user32;
    char line[400];

    if (arm32_api_resolved) return;
    arm32_api_resolved = TRUE;
    user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) user32 = LoadLibraryW(L"user32.dll");
    if (!user32) {
        log_line("ARM32 input: user32.dll could not be loaded - no touch API can be resolved");
        return;
    }
    /* everything newer than Windows XP is resolved dynamically (the SDK ARM import library also lists post-8.1 functions, so a static
     * import would stop win32craft.dll from loading on Windows RT 8.1) */
    arm32_api.get_pointer_type = (Arm32GetPointerTypeFn)GetProcAddress(user32, "GetPointerType");
    arm32_api.get_pointer_info = (Arm32GetPointerInfoFn)GetProcAddress(user32, "GetPointerInfo");
    arm32_api.register_touch_window = (Arm32RegisterTouchWindowFn)GetProcAddress(user32, "RegisterTouchWindow");
    arm32_api.get_touch_input_info = (Arm32GetTouchInputInfoFn)GetProcAddress(user32, "GetTouchInputInfo");
    arm32_api.close_touch_input_handle = (Arm32CloseTouchInputHandleFn)GetProcAddress(user32, "CloseTouchInputHandle");
    arm32_api.set_window_feedback_setting = (Arm32SetWindowFeedbackSettingFn)GetProcAddress(user32, "SetWindowFeedbackSetting");
    arm32_api.get_raw_input_device_list = (Arm32GetRawInputDeviceListFn)GetProcAddress(user32, "GetRawInputDeviceList");
    arm32_api.get_raw_input_device_info = (Arm32GetRawInputDeviceInfoWFn)GetProcAddress(user32, "GetRawInputDeviceInfoW");
    arm32_api.is_mouse_in_pointer_enabled = (Arm32IsMouseInPointerEnabledFn)GetProcAddress(user32, "IsMouseInPointerEnabled");
    wsprintfA(line, "ARM32 input: user32 touch API: GetPointerType=%s GetPointerInfo=%s RegisterTouchWindow=%s GetTouchInputInfo=%s "
                    "CloseTouchInputHandle=%s SetWindowFeedbackSetting=%s GetRawInputDeviceList=%s IsMouseInPointerEnabled=%s",
              arm32_api.get_pointer_type ? "yes" : "NO", arm32_api.get_pointer_info ? "yes" : "NO",
              arm32_api.register_touch_window ? "yes" : "NO", arm32_api.get_touch_input_info ? "yes" : "NO",
              arm32_api.close_touch_input_handle ? "yes" : "NO", arm32_api.set_window_feedback_setting ? "yes" : "NO",
              arm32_api.get_raw_input_device_list ? "yes" : "NO", arm32_api.is_mouse_in_pointer_enabled ? "yes" : "NO");
    log_line(line);
}

/* Device inventory (RAWINPUTDEVICELIST is {HANDLE, DWORD}, 8 bytes on ARM32).  Virtual devices (Remote Desktop, "Root#" software
 * devices) do not count.  Only for the log and the initial guess; the runtime decision is always "last used wins". */
static BOOL arm32_input_scan_raw_devices(int *mice, int *keyboards, int *virtual_devices)
{
    typedef struct RawDevice { HANDLE device; DWORD type; } RawDevice;
    RawDevice *list;
    UINT count = 0;
    UINT got;
    UINT index;
    unsigned logged = 0;
    char line[400];
    char narrow[200];

    *mice = 0;
    *keyboards = 0;
    *virtual_devices = 0;
    if (!arm32_api.get_raw_input_device_list || !arm32_api.get_raw_input_device_info) return FALSE;
    if (arm32_api.get_raw_input_device_list(NULL, &count, sizeof(RawDevice)) == (UINT)-1) {
        wsprintfA(line, "ARM32 input: GetRawInputDeviceList(count) failed, error=%lu", (unsigned long)GetLastError());
        log_line(line);
        return FALSE;
    }
    if (!count) return TRUE;
    if (count > 64) count = 64;
    list = (RawDevice *)HeapAlloc(GetProcessHeap(), 0, count * sizeof(RawDevice));
    if (!list) return FALSE;
    got = arm32_api.get_raw_input_device_list(list, &count, sizeof(RawDevice));
    if (got == (UINT)-1) {
        wsprintfA(line, "ARM32 input: GetRawInputDeviceList failed, error=%lu", (unsigned long)GetLastError());
        log_line(line);
        HeapFree(GetProcessHeap(), 0, list);
        return FALSE;
    }
    for (index = 0; index < got; ++index) {
        wchar_t name[260];
        UINT length = ARRAYSIZE(name) - 1;
        BOOL is_virtual;
        const char *kind = list[index].type == 0 ? "mouse" : (list[index].type == 1 ? "keyboard" : "hid");

        name[0] = 0;
        if (arm32_api.get_raw_input_device_info(list[index].device, 0x20000007u, name, &length) == (UINT)-1) name[0] = 0;
        name[ARRAYSIZE(name) - 1] = 0;
        is_virtual = !name[0] || arm32_wcs_contains_ci(name, L"Root#") || arm32_wcs_contains_ci(name, L"RDP_");
        if (is_virtual) ++*virtual_devices;
        else if (list[index].type == 0) ++*mice;
        else if (list[index].type == 1) ++*keyboards;
        if (logged < 16) {
            arm32_iat_narrow_for_log(narrow, sizeof(narrow), name);
            wsprintfA(line, "ARM32 input: raw device #%u type=%s virtual=%d name=%s", index, kind, (int)is_virtual, narrow);
            log_line(line);
            ++logged;
        }
    }
    HeapFree(GetProcessHeap(), 0, list);
    return TRUE;
}

/* Once, before the window exists: metrics, device inventory and the initial guess of the active input source. */
static void arm32_input_probe(void)
{
    int mouse_present;
    int mouse_buttons;
    int digitizer;
    int max_touches;
    int slate;
    int remote;
    int mice = 0;
    int keyboards = 0;
    int virtual_devices = 0;
    BOOL raw_ok;
    BOOL touch_only;
    BOOL mouse_in_pointer = FALSE;
    char line[500];

    if (arm32_input_probed) return;
    arm32_input_probed = TRUE;
    arm32_input_resolve_apis();
    mouse_present = GetSystemMetrics(ARM32_SM_MOUSEPRESENT);
    mouse_buttons = GetSystemMetrics(ARM32_SM_CMOUSEBUTTONS);
    digitizer = GetSystemMetrics(ARM32_SM_DIGITIZER);
    max_touches = GetSystemMetrics(ARM32_SM_MAXIMUMTOUCHES);
    slate = GetSystemMetrics(ARM32_SM_CONVERTIBLESLATEMODE);
    remote = GetSystemMetrics(ARM32_SM_REMOTESESSION);
    if (arm32_api.is_mouse_in_pointer_enabled) mouse_in_pointer = arm32_api.is_mouse_in_pointer_enabled();
    raw_ok = arm32_input_scan_raw_devices(&mice, &keyboards, &virtual_devices);
    /* SM_MOUSEPRESENT is NOT trusted (Windows keeps a logical mouse on touch tablets); a touch-only machine has a ready digitizer
     * (NID_READY 0x80 and integrated/external touch 0x3), touch points and no real mouse or keyboard device */
    touch_only = max_touches > 0 && (digitizer & 0x80) && (digitizer & 0x3) && raw_ok && mice == 0 && keyboards == 0;
    if (arm32_input_policy == ARM32_INPUT_TOUCH) arm32_input_initial_source = ARM32_SRC_TOUCH;
    else if (arm32_input_policy == ARM32_INPUT_MOUSE) arm32_input_initial_source = ARM32_SRC_MOUSEKB;
    else arm32_input_initial_source = touch_only ? ARM32_SRC_TOUCH : ARM32_SRC_MOUSEKB;
    arm32_input_source = arm32_input_initial_source;
    wsprintfA(line, "ARM32 input: policy=%s touch-api=%s osk=%s digitizer=0x%08lx maxTouches=%d mousePresent=%d mouseButtons=%d "
                    "convertibleSlate=%d remoteSession=%d mouseInPointer=%d rawScan=%s rawMice=%d rawKeyboards=%d rawVirtual=%d "
                    "touchOnlyMachine=%d initialSource=%s",
              arm32_input_policy_names[arm32_input_policy], arm32_touch_api_names[arm32_touch_api_requested],
              arm32_osk_policy_names[arm32_osk_policy], (unsigned long)digitizer, max_touches, mouse_present, mouse_buttons, slate,
              remote, (int)mouse_in_pointer, raw_ok ? "ok" : "FAILED", mice, keyboards, virtual_devices, (int)touch_only,
              arm32_input_initial_source == ARM32_SRC_TOUCH ? "TOUCH" : "MOUSEKB");
    log_line(line);
}

static BOOL arm32_input_touch_active(void)
{
    if (arm32_input_policy == ARM32_INPUT_TOUCH) return TRUE;
    return arm32_input_policy == ARM32_INPUT_AUTO && arm32_input_source == ARM32_SRC_TOUCH;
}

/* touch -> mouse/keyboard: the stock TouchMapper keeps "released" flags until its reset (TouchMapper::vf2) and, being later in the mapper
 * list than MouseMapper, would win InputHandler::tick for ever.  The reset is deferred by a few passes so that the touch releases queued
 * just before are consumed by the game first.  Implemented in the diagnostics part below. */
static void arm32_touch_mapper_reset_now(const char *why);

static void arm32_input_note_source(int source, const char *why)
{
    static unsigned logged;
    int previous = arm32_input_source;
    char line[200];

    if (arm32_input_policy != ARM32_INPUT_AUTO || source == previous) return;
    arm32_input_source = source;
    ++arm32_stats.source_switches;
    if (logged < 300) {
        ++logged;
        wsprintfA(line, "ARM32 input: source %s -> %s (%s)", previous == ARM32_SRC_TOUCH ? "TOUCH" : "MOUSEKB",
                  source == ARM32_SRC_TOUCH ? "TOUCH" : "MOUSEKB", why ? why : "?");
        log_line(line);
    }
    if (source == ARM32_SRC_MOUSEKB) {
        arm32_touch_release_all("source -> mouse/keyboard");
        arm32_mapper_reset_pass = arm32_input_pass + 4;
    } else {
        arm32_mapper_reset_pass = 0;
    }
    if (game_window) update_mouse_capture(game_window);   /* release (touch) / re-acquire (mouse) the relative mouse right away */
}

/* single gate for the relative mouse: while a finger is the active input nothing may capture, clip or hide the cursor */
static BOOL arm32_relative_mouse_gate(BOOL requested)
{
    static int suppressed = -1;
    static unsigned logged;
    BOOL result = requested && !arm32_input_touch_active();
    int now_suppressed = requested && !result;

    if (now_suppressed != suppressed && logged < 200) {
        ++logged;
        suppressed = now_suppressed;
        log_line(now_suppressed ? "ARM32 input: relative mouse suppressed (touch is the active input device)"
                                : "ARM32 input: relative mouse allowed again");
    } else {
        suppressed = now_suppressed;
    }
    return result;
}

/* ---- touch contacts ---- */
static int arm32_touch_find_slot(UINT32 id)
{
    int index;

    for (index = 0; index < ARM32_TOUCH_SLOTS; ++index) {
        if (arm32_touch_slots[index].active && arm32_touch_slots[index].id == id) return index;
    }
    return -1;
}

/* lowest free slot, like the stock allocPointerSlot (0x627af0) */
static int arm32_touch_alloc_slot(UINT32 id)
{
    int index;

    for (index = 0; index < ARM32_TOUCH_SLOTS; ++index) {
        if (!arm32_touch_slots[index].active) {
            memset(&arm32_touch_slots[index], 0, sizeof(arm32_touch_slots[index]));
            arm32_touch_slots[index].active = TRUE;
            arm32_touch_slots[index].id = id;
            return index;
        }
    }
    return -1;
}

static BOOL arm32_touch_any_active(void)
{
    int index;

    for (index = 0; index < ARM32_TOUCH_SLOTS; ++index) {
        if (arm32_touch_slots[index].active) return TRUE;
    }
    return FALSE;
}

static int arm32_touch_active_count(void)
{
    int index;
    int count = 0;

    for (index = 0; index < ARM32_TOUCH_SLOTS; ++index) {
        if (arm32_touch_slots[index].active) ++count;
    }
    return count;
}

static unsigned arm32_touch_log_budget(void)
{
    return arm32_input_verbose ? 0xffffffffu : 300u;
}

/* screen -> client pixels, clamped to the client area (a drag may leave the window; the clamp keeps a joystick alive) */
static void arm32_touch_screen_to_client(HWND window, int screen_x, int screen_y, int *client_x, int *client_y)
{
    POINT point;
    RECT client;

    point.x = screen_x;
    point.y = screen_y;
    if (!ScreenToClient(window, &point)) {
        point.x = screen_x;
        point.y = screen_y;
    }
    if (GetClientRect(window, &client)) {
        if (point.x >= client.right) point.x = client.right - 1;
        if (point.y >= client.bottom) point.y = client.bottom - 1;
    }
    if (point.x < 0) point.x = 0;
    if (point.y < 0) point.y = 0;
    *client_x = point.x;
    *client_y = point.y;
}

static void arm32_touch_clamp_client(HWND window, int *x, int *y)
{
    RECT client;

    if (GetClientRect(window, &client)) {
        if (*x >= client.right) *x = client.right - 1;
        if (*y >= client.bottom) *y = client.bottom - 1;
    }
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
}

/* one-time snapshot of what the game will see when the first finger lands */
static void arm32_touch_log_first_contact(void)
{
    static BOOL logged;
    void *input_handler = NULL;
    BYTE *controller;
    char line[320];

    if (logged) return;
    logged = TRUE;
    controller = arm32_input_target(&input_handler);
    wsprintfA(line, "ARM32 touch: FIRST touch contact. HID state: controller=%p +0xa0(relative actual)=%u +0x25=%u command(+0x28)=%d "
                    "input_events_enabled=%d text_input_active=%d mouse_capture_active=%d relative_requested=%d initial_menu_cursor_guard=%d",
              (void *)controller,
              controller ? (unsigned)controller[ARM_INPUT_MOUSE_ACTUAL_OFFSET] : 0u,
              controller ? (unsigned)controller[0x25] : 0u,
              controller ? *(int *)(controller + ARM_INPUT_MOUSE_COMMAND_OFFSET) : -1,
              (int)input_events_enabled, (int)text_input_active, (int)mouse_capture_active, (int)host_relative_requested,
              (int)initial_menu_cursor_guard);
    log_line(line);
}

static void arm32_touch_move(int slot, int client_x, int client_y)
{
    Arm32TouchSlot *s = &arm32_touch_slots[slot];

    if (!s->active) return;
    ++arm32_stats.move_messages;
    s->x = client_x;
    s->y = client_y;
    s->dirty = (client_x != s->sent_x || client_y != s->sent_y);
}

static int arm32_touch_begin(HWND window, UINT32 id, int client_x, int client_y, const char *via)
{
    static unsigned full_logged;
    int slot = arm32_touch_find_slot(id);
    Arm32TouchSlot *s;
    char line[240];

    if (slot >= 0) {
        arm32_touch_move(slot, client_x, client_y);
        return slot;
    }
    slot = arm32_touch_alloc_slot(id);
    if (slot < 0) {
        ++arm32_stats.slots_full;
        if (full_logged < 5) {
            ++full_logged;
            wsprintfA(line, "ARM32 touch: all %d slots are busy, contact id=%lu ignored", ARM32_TOUCH_SLOTS, (unsigned long)id);
            log_line(line);
        }
        return -1;
    }
    s = &arm32_touch_slots[slot];
    arm32_touch_log_first_contact();
    arm32_last_touch_tick = GetTickCount();
    arm32_input_note_source(ARM32_SRC_TOUCH, via);
    if (window && GetFocus() != window) SetFocus(window);
    if (initial_menu_cursor_guard) {
        /* a first touch proves the menu is live, like the first mouse click does */
        initial_menu_cursor_guard = FALSE;
        host_relative_requested = FALSE;
        mouse_clip_dirty = TRUE;
        log_line("ARM32 touch: initial main-menu cursor guard released by touch");
    }
    s->x = s->sent_x = client_x;
    s->y = s->sent_y = client_y;
    s->down_tick = GetTickCount();
    if (!queue_touch_event(slot, 1, 1, client_x, client_y)) {
        s->active = FALSE;
        ++arm32_stats.queue_failed;
        wsprintfA(line, "ARM32 touch: down id=%lu could not be queued (game input target not ready?)", (unsigned long)id);
        log_line(line);
        return -1;
    }
    ++arm32_stats.down;
    if (arm32_stats.down <= arm32_touch_log_budget()) {
        wsprintfA(line, "ARM32 touch: down id=%lu slot=%d client=(%d,%d) via %s (active contacts: %d)", (unsigned long)id, slot,
                  client_x, client_y, via, arm32_touch_active_count());
        log_line(line);
    }
    return slot;
}

static void arm32_touch_end(int slot, int client_x, int client_y, const char *why)
{
    Arm32TouchSlot *s = &arm32_touch_slots[slot];
    char line[200];

    if (!s->active) return;
    s->active = FALSE;
    s->dirty = FALSE;
    arm32_last_touch_tick = GetTickCount();
    arm32_osk_note_pointer_up();
    if (!queue_touch_event(slot, 1, 0, client_x, client_y)) ++arm32_stats.queue_failed;
    ++arm32_stats.up;
    if (arm32_stats.up <= arm32_touch_log_budget()) {
        wsprintfA(line, "ARM32 touch: up id=%lu slot=%d client=(%d,%d) held %lu ms (%s)", (unsigned long)s->id, slot, client_x,
                  client_y, (unsigned long)(GetTickCount() - s->down_tick), why);
        log_line(line);
    }
}

/* at most one move per contact and loop pass: the game reads the queue once per frame anyway */
static void arm32_touch_flush_moves(void)
{
    int slot;

    for (slot = 0; slot < ARM32_TOUCH_SLOTS; ++slot) {
        Arm32TouchSlot *s = &arm32_touch_slots[slot];
        if (!s->active || !s->dirty) continue;
        s->dirty = FALSE;
        s->sent_x = s->x;
        s->sent_y = s->y;
        if (queue_touch_event(slot, 0, 0, s->x, s->y)) ++arm32_stats.moves_flushed;
        else ++arm32_stats.queue_failed;
    }
}

static void arm32_touch_release_all(const char *why)
{
    int slot;
    unsigned released = 0;
    char line[160];

    for (slot = 0; slot < ARM32_TOUCH_SLOTS; ++slot) {
        Arm32TouchSlot *s = &arm32_touch_slots[slot];
        if (!s->active) continue;
        arm32_touch_end(slot, s->x, s->y, why);
        ++released;
    }
    if (released) {
        wsprintfA(line, "ARM32 touch: released %u contact(s): %s", released, why);
        log_line(line);
    }
}

/* ---- WM_POINTER* (primary), WM_TOUCH (fallback) and touch-sourced mouse messages (fallback / filter) ---- */
static BOOL arm32_pointer_nontouch_known(UINT32 id)
{
    unsigned index;

    for (index = 0; index < ARRAYSIZE(arm32_nontouch_ids) && index < arm32_nontouch_count; ++index) {
        if (arm32_nontouch_ids[index] == id) return TRUE;
    }
    return FALSE;
}

static BOOL arm32_pointer_is_touch(UINT32 id, UINT32 *type_out)
{
    static BOOL logged_failure;
    UINT32 type = 0;
    char line[160];

    if (type_out) *type_out = 0;
    if (!arm32_api.get_pointer_type) return FALSE;
    if (!arm32_api.get_pointer_type(id, &type)) {
        if (!logged_failure) {
            logged_failure = TRUE;
            wsprintfA(line, "ARM32 touch: GetPointerType(%lu) failed, error=%lu - treated as non-touch", (unsigned long)id,
                      (unsigned long)GetLastError());
            log_line(line);
        }
        return FALSE;
    }
    if (type_out) *type_out = type;
    if (type != ARM32_PT_TOUCH) {
        if (!arm32_pointer_nontouch_known(id)) {
            arm32_nontouch_ids[arm32_nontouch_count % ARRAYSIZE(arm32_nontouch_ids)] = id;
            ++arm32_nontouch_count;
        }
        return FALSE;
    }
    return TRUE;
}

static void arm32_touch_from_pointer_lparam(HWND window, LPARAM lparam, int *client_x, int *client_y)
{
    arm32_touch_screen_to_client(window, (short)LOWORD(lparam), (short)HIWORD(lparam), client_x, client_y);
}

/* WM_POINTERUPDATE / DOWN / UP / CAPTURECHANGED.  TRUE = consumed (window_proc returns 0 WITHOUT DefWindowProc, which is what stops
 * Windows from synthesising mouse messages for the contact).  Mouse, pen and touchpad pointers are never consumed. */
static BOOL arm32_touch_pointer_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    UINT32 id = (UINT32)LOWORD(wparam);
    UINT flags = (UINT)HIWORD(wparam);
    UINT32 type = 0;
    static unsigned info_logged;
    static unsigned nontouch_logged;
    int slot;
    int client_x;
    int client_y;
    char line[260];

    if (arm32_input_policy == ARM32_INPUT_MOUSE || arm32_touch_api_effective != ARM32_TOUCHAPI_POINTER) return FALSE;
    slot = arm32_touch_find_slot(id);

    if (message == ARM32_WM_POINTERCAPTURECHANGED) {
        /* lParam is the HWND that took the capture, not a position.  Capture moving to ourselves is harmless. */
        if (slot < 0 || (HWND)lparam == window) return FALSE;
        ++arm32_stats.capture_lost;
        wsprintfA(line, "ARM32 touch: pointer capture of id=%lu moved to window %p - releasing the contact", (unsigned long)id,
                  (void *)lparam);
        log_line(line);
        arm32_touch_end(slot, arm32_touch_slots[slot].x, arm32_touch_slots[slot].y, "WM_POINTERCAPTURECHANGED");
        return TRUE;
    }

    if (message == ARM32_WM_POINTERDOWN && slot < 0) {
        if (!arm32_pointer_is_touch(id, &type)) {
            if (type == ARM32_PT_MOUSE) ++arm32_stats.pointer_mouse;
            else if (type == ARM32_PT_PEN) ++arm32_stats.pointer_pen;
            else ++arm32_stats.pointer_other;
            if (nontouch_logged < 6) {
                ++nontouch_logged;
                wsprintfA(line, "ARM32 touch: WM_POINTERDOWN id=%lu is not touch (GetPointerType=%lu: 2 touch, 3 pen, 4 mouse, 5 touchpad) "
                                "- left to DefWindowProc / the mouse path", (unsigned long)id, (unsigned long)type);
                log_line(line);
            }
            return FALSE;
        }
        if (!arm32_pointer_touch_seen) {
            int legacy_slot = arm32_touch_find_slot(ARM32_LEGACY_TOUCH_ID);
            arm32_pointer_touch_seen = TRUE;
            if (arm32_legacy_touch_used) {
                log_line("ARM32 touch: first PT_TOUCH WM_POINTERDOWN received after contacts that arrived as emulated mouse messages - "
                         "the pointer API works on this device, multi-touch is active");
            } else {
                log_line("ARM32 touch: first PT_TOUCH WM_POINTERDOWN received - the pointer API works on this device");
            }
            /* a finger that began as a converted mouse contact must not stay down: its button-up is dropped from now on */
            if (legacy_slot >= 0) {
                arm32_touch_end(legacy_slot, arm32_touch_slots[legacy_slot].x, arm32_touch_slots[legacy_slot].y, "pointer API took over");
            }
        }
        arm32_touch_from_pointer_lparam(window, lparam, &client_x, &client_y);
        if (info_logged < 3 && arm32_api.get_pointer_info) {
            /* POINTER_INFO (ARM32): +0 type, +4 id, +8 frame, +12 flags, +16 sourceDevice, +20 hwndTarget, +24 ptPixelLocation(x,y) */
            unsigned long long info[16];
            memset(info, 0, sizeof(info));
            ++info_logged;
            if (arm32_api.get_pointer_info(id, info)) {
                const int *words = (const int *)info;
                wsprintfA(line, "ARM32 touch: GetPointerInfo id=%lu type=%d flags=0x%08x ptPixelLocation=(%d,%d) lParam screen=(%d,%d) client=(%d,%d)",
                          (unsigned long)id, words[0], (unsigned)words[3], words[6], words[7], (int)(short)LOWORD(lparam),
                          (int)(short)HIWORD(lparam), client_x, client_y);
            } else {
                wsprintfA(line, "ARM32 touch: GetPointerInfo(%lu) failed, error=%lu", (unsigned long)id, (unsigned long)GetLastError());
            }
            log_line(line);
        }
        arm32_touch_begin(window, id, client_x, client_y, "WM_POINTERDOWN");
        return TRUE;   /* consumed even when all slots are busy */
    }

    if (message == ARM32_WM_POINTERDOWN || message == ARM32_WM_POINTERUPDATE) {
        if (slot >= 0) {
            arm32_touch_from_pointer_lparam(window, lparam, &client_x, &client_y);
            if (flags & ARM32_POINTER_FLAG_CANCELED) {
                ++arm32_stats.canceled;
                arm32_touch_end(slot, client_x, client_y, "WM_POINTERUPDATE canceled");
            } else {
                arm32_touch_move(slot, client_x, client_y);
            }
            return TRUE;
        }
        /* an unknown pointer that is in contact: its DOWN was missed (message sent before the window was ready) */
        if (message == ARM32_WM_POINTERUPDATE && (flags & ARM32_POINTER_FLAG_INCONTACT) && !arm32_pointer_nontouch_known(id) &&
            arm32_pointer_is_touch(id, &type)) {
            arm32_pointer_touch_seen = TRUE;
            arm32_touch_from_pointer_lparam(window, lparam, &client_x, &client_y);
            arm32_touch_begin(window, id, client_x, client_y, "WM_POINTERUPDATE (missed down)");
            return TRUE;
        }
        return FALSE;
    }

    if (message == ARM32_WM_POINTERUP) {
        if (slot < 0) return FALSE;
        arm32_touch_from_pointer_lparam(window, lparam, &client_x, &client_y);
        if (flags & ARM32_POINTER_FLAG_CANCELED) ++arm32_stats.canceled;
        arm32_touch_end(slot, client_x, client_y, (flags & ARM32_POINTER_FLAG_CANCELED) ? "WM_POINTERUP canceled" : "WM_POINTERUP");
        return TRUE;
    }
    return FALSE;
}

/* WM_TOUCH (--touch-api=wmtouch).  TOUCHINPUT on ARM32 is 40 bytes {LONG x,y; HANDLE hSource; DWORD dwID,dwFlags,dwMask,dwTime;
 * ULONG_PTR dwExtraInfo; DWORD cxContact,cyContact}; x,y are 1/100 pixel in screen coordinates. */
typedef struct Arm32TouchInput {
    LONG x, y;
    HANDLE source;
    DWORD id, flags, mask, time;
    ULONG_PTR extra;
    DWORD contact_x, contact_y;
} Arm32TouchInput;

static BOOL arm32_touch_wm_touch(HWND window, WPARAM wparam, LPARAM lparam)
{
    Arm32TouchInput inputs[16];
    UINT count = LOWORD(wparam);
    UINT index;
    static BOOL logged_first;

    if (arm32_input_policy == ARM32_INPUT_MOUSE) return FALSE;
    if (arm32_touch_api_effective != ARM32_TOUCHAPI_WMTOUCH) {
        /* auto mode registered the window for WM_TOUCH up front.  A WM_TOUCH message while no WM_POINTER touch message was ever seen proves the
         * pointer stack is not delivered here (Windows RT 8.1 Surface RT log: pointer*=0, all touch as emulated mouse): use real multi-touch. */
        if (!(arm32_touch_api_effective == ARM32_TOUCHAPI_POINTER && arm32_wm_touch_registered && !arm32_pointer_touch_seen &&
              !arm32_legacy_touch_used)) return FALSE;   /* after a converted mouse contact the device is single touch, as before: no late takeover */
        arm32_touch_api_effective = ARM32_TOUCHAPI_WMTOUCH;
        log_line("ARM32 input: WM_TOUCH arrived but no WM_POINTER touch message was ever seen - switching to --touch-api=wmtouch (multi-touch)");
    }
    if (!arm32_api.get_touch_input_info || !arm32_api.close_touch_input_handle) return FALSE;
    if (count > ARRAYSIZE(inputs)) count = ARRAYSIZE(inputs);
    if (!arm32_api.get_touch_input_info((HANDLE)lparam, count, inputs, (int)sizeof(Arm32TouchInput))) return FALSE;
    ++arm32_stats.wm_touch_messages;
    if (!logged_first) {
        logged_first = TRUE;
        log_line("ARM32 touch: first WM_TOUCH received - the WM_TOUCH API works on this device");
    }
    for (index = 0; index < count; ++index) {
        int client_x;
        int client_y;
        int slot;

        if (inputs[index].flags & ARM32_TOUCHEVENTF_PALM) continue;
        arm32_touch_screen_to_client(window, inputs[index].x / 100, inputs[index].y / 100, &client_x, &client_y);
        slot = arm32_touch_find_slot(inputs[index].id);
        if (inputs[index].flags & ARM32_TOUCHEVENTF_DOWN) {
            arm32_touch_begin(window, inputs[index].id, client_x, client_y, "WM_TOUCH down");
        } else if (inputs[index].flags & ARM32_TOUCHEVENTF_UP) {
            if (slot >= 0) arm32_touch_end(slot, client_x, client_y, "WM_TOUCH up");
        } else if (inputs[index].flags & ARM32_TOUCHEVENTF_MOVE) {
            if (slot >= 0) arm32_touch_move(slot, client_x, client_y);
            else arm32_touch_begin(window, inputs[index].id, client_x, client_y, "WM_TOUCH move (missed down)");
        }
    }
    arm32_api.close_touch_input_handle((HANDLE)lparam);
    return TRUE;
}

/* single touch from the emulated mouse messages (--touch-api=legacy, or the automatic fallback when no pointer message ever arrived) */
static BOOL arm32_legacy_touch_message(HWND window, UINT message, LPARAM lparam)
{
    int client_x = (short)LOWORD(lparam);
    int client_y = (short)HIWORD(lparam);
    int slot = arm32_touch_find_slot(ARM32_LEGACY_TOUCH_ID);

    arm32_touch_clamp_client(window, &client_x, &client_y);
    if (message == WM_LBUTTONDOWN) {
        arm32_touch_begin(window, ARM32_LEGACY_TOUCH_ID, client_x, client_y, "legacy WM_LBUTTONDOWN (touch-sourced)");
    } else if (message == WM_MOUSEMOVE) {
        if (slot >= 0) arm32_touch_move(slot, client_x, client_y);
    } else if (message == WM_LBUTTONUP) {
        if (slot >= 0) arm32_touch_end(slot, client_x, client_y, "legacy WM_LBUTTONUP (touch-sourced)");
    }
    /* right / middle buttons and wheel produced by gestures (press-and-hold, pan) are dropped */
    return TRUE;
}

/* Top of every mouse message.  TRUE = consumed.  A mouse message carrying the touch signature (GetMessageExtraInfo
 * & 0xFFFFFF80 == 0xFF515780) is Windows' emulation of a finger: it is never given to the game as a MouseItem.  Everything else is a
 * real mouse (or pen) and, in auto mode, makes mouse/keyboard the active input source. */
static BOOL arm32_legacy_mouse_filter(HWND window, UINT message, LPARAM lparam)
{
    static unsigned logged;
    DWORD extra;
    BOOL from_touch;
    BOOL real_move = FALSE;
    char line[240];

    if (arm32_input_policy == ARM32_INPUT_MOUSE || !arm32_touch_api_effective) return FALSE;
    extra = (DWORD)GetMessageExtraInfo();
    from_touch = (extra & 0xFFFFFF80u) == 0xFF515780u;
    if (from_touch) {
        /* No PT_TOUCH WM_POINTERDOWN has arrived yet: convert the emulated mouse messages to a (single) contact, but stay in pointer mode.
         * Windows also sends a lone emulated tap (the tap that activates the window ...) on machines whose pointer stack works, so
         * the first such message proves nothing; switching to legacy for good made Windows 11 ARM64 play with ONE finger (no walking and
         * looking at the same time).  The first real WM_POINTERDOWN of a finger takes over; where none ever comes (Windows RT 8.1 Surface RT
         * log: pointer*=0) every contact stays a converted single touch, as before. */
        BOOL provisional = arm32_touch_api_effective == ARM32_TOUCHAPI_POINTER && !arm32_pointer_touch_seen;
        if (provisional) arm32_legacy_touch_used = TRUE;
        ++arm32_stats.legacy_from_touch;
        if (logged < 8) {
            ++logged;
            wsprintfA(line, "ARM32 input: touch-sourced mouse message 0x%04x extra=0x%08lx touchApi=%s pointerTouchSeen=%d -> %s",
                      message, (unsigned long)extra, arm32_touch_api_names[arm32_touch_api_effective],
                      (int)arm32_pointer_touch_seen,
                      arm32_touch_api_effective == ARM32_TOUCHAPI_LEGACY ? "converted to touch"
                      : (provisional ? "converted to touch (provisional: the pointer API stays active and takes over with the first WM_POINTER finger)"
                                     : "dropped"));
            log_line(line);
        }
        if (provisional && message == WM_LBUTTONDOWN && arm32_stats.legacy_from_touch >= 12 && !arm32_pointer_touch_seen) {
            static BOOL noted;
            if (!noted) {
                noted = TRUE;
                log_line("ARM32 input: NOTE many touch contacts arrived only as emulated mouse messages and no WM_POINTER finger was ever seen - "
                         "this device delivers single touch only (send this log if multi-touch is expected)");
            }
        }
        if (arm32_touch_api_effective == ARM32_TOUCHAPI_LEGACY || provisional) return arm32_legacy_touch_message(window, message, lparam);
        return TRUE;   /* pointer / wmtouch modes: the contact is already delivered, drop the duplicate */
    }

    ++arm32_stats.legacy_mouse_messages;
    if (message == WM_MOUSEMOVE) {
        /* only a real displacement counts: Windows re-posts WM_MOUSEMOVE for a stationary cursor when windows appear / move, and the
         * synthetic moves that follow a touch must not flip the source (400 ms guard) */
        POINT screen;
        real_move = FALSE;
        screen.x = (short)LOWORD(lparam);
        screen.y = (short)HIWORD(lparam);
        if (!ClientToScreen(window, &screen)) {
            screen.x = (short)LOWORD(lparam);
            screen.y = (short)HIWORD(lparam);
        }
        if (arm32_have_mouse_position) {
            int dx = screen.x - arm32_last_mouse_screen_x;
            int dy = screen.y - arm32_last_mouse_screen_y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            real_move = (dx + dy) >= 2 && !arm32_touch_any_active() && (GetTickCount() - arm32_last_touch_tick) > 400u;
            if (real_move && arm32_input_policy == ARM32_INPUT_AUTO && arm32_input_source != ARM32_SRC_MOUSEKB) {
                wsprintfA(line, "mouse move %dpx", dx + dy);
                arm32_input_note_source(ARM32_SRC_MOUSEKB, line);
            }
        }
        arm32_have_mouse_position = TRUE;
        arm32_last_mouse_screen_x = screen.x;
        arm32_last_mouse_screen_y = screen.y;
    } else if (arm32_input_policy == ARM32_INPUT_AUTO && arm32_input_source != ARM32_SRC_MOUSEKB &&
               (GetTickCount() - arm32_last_touch_tick) > 400u) {
        wsprintfA(line, "mouse message 0x%04x", message);
        arm32_input_note_source(ARM32_SRC_MOUSEKB, line);
    }
    /* touch is the active source and this move was not a real displacement (a stationary cursor re-posted by Windows when the
     * keyboard window appears, the window activates ...): it must not become a MouseItem, which would flip the game back to Mouse mode */
    if (message == WM_MOUSEMOVE && !real_move && arm32_input_touch_active()) {
        static unsigned dropped_logged;
        if (dropped_logged < 5) {
            ++dropped_logged;
            log_line("ARM32 input: WM_MOUSEMOVE without a real displacement ignored while touch is the active source");
        }
        return TRUE;
    }
    return FALSE;
}

/* a key press means a keyboard is in use, except keys injected by the touch keyboard while text is being typed */
static void arm32_input_note_key(WPARAM virtual_key)
{
    char line[64];

    if (arm32_input_policy != ARM32_INPUT_AUTO || virtual_key == 0xE7) return;   /* VK_PACKET: injected by the touch keyboard */
    if (arm32_touch_any_active()) return;                                        /* a finger is down: a stray key must not drop it */
    if (text_input_active && arm32_osk_visible_now()) return;
    wsprintfA(line, "key VK=0x%02x", (unsigned)virtual_key);
    arm32_input_note_source(ARM32_SRC_MOUSEKB, line);
}

/* ---- Windows touch keyboard (TabTip) for text boxes ----
 * The game asks for the keyboard through InputPane.TryShow / TryHide (fake InputPane above) and reacts to the InputPane Showing /
 * Hiding events (keyboard height -> chat box position).  Device facts (kbdprobe run, Windows 10 build 15035 ARM32, Surface 2):
 *   - IsWindowVisible(IPTip_Main_Window) is ALWAYS true and the window rectangle never changes, so neither says anything;
 *   - the truth is IFrameworkInputPane::Location (empty rect = hidden), the WinRT InputPane OccludedRect (height 0 = hidden) and the
 *     DWM cloak flag of the TabTip window (cloaked = hidden) - all three agreed with what the user saw;
 *   - launching TabTip.exe while it already runs does NOTHING; ITipInvocation::Toggle shows AND hides (state blind);
 *   - hiding works with IInputPane2::TryHide and with SC_CLOSE to the TabTip window; ShowWindow(SW_HIDE) does nothing;
 *   - killing TabTip.exe leaves IFrameworkInputPane stale and ITipInvocation unregistered until TabTip.exe is launched again: never kill it;
 *   - IInputPane2::TryShow does not show a keyboard for a window without a text control (so Toggle is the way to show);
 *   - the user can close it with the keyboard's X at any time; nothing tells the game, so the host raises the game's Hiding handler.
 * The previous controller trusted IsWindowVisible: it never toggled a running TabTip, reported a keyboard rectangle forever
 * ("won't go down") and never told the game about Showing/Hiding.  Windows RT 8.1 is not measured yet (TelegArm: InputPane interop is
 * missing there, the cloak flag is unreliable): the log line "ARM32 osk: signals ..." names what was found and every action and every
 * state change is logged ("ARM32 osk:" lines). */
#define ARM32_OSK_SIG_NONE      0
#define ARM32_OSK_SIG_FIP       1
#define ARM32_OSK_SIG_PANE      2
#define ARM32_OSK_SIG_CLOAK     3
#define ARM32_OSK_HIDE_TRYHIDE  1
#define ARM32_OSK_HIDE_CLOSE    2
#define ARM32_OSK_HIDE_TOGGLE   3
#define ARM32_OSK_E_ABORT      ((HRESULT)0x80004004L)   /* a Toggle the host refused to send (the previous one was less than 2 s ago) */

static const char *const arm32_osk_signal_names[] = { "none", "IFrameworkInputPane", "InputPane", "dwm-cloak" };
static const char *const arm32_osk_hide_names[] = { "?", "InputPane2::TryHide", "SC_CLOSE", "ITipInvocation::Toggle" };

static BOOL arm32_osk_want;                    /* the game asked for the keyboard and it is not up yet */
static BOOL arm32_osk_ours;                    /* the keyboard on screen was shown on the game's request */
static BOOL arm32_osk_launched;
static DWORD arm32_osk_last_attempt;
static DWORD arm32_osk_want_since;
static unsigned arm32_osk_attempts;
static BOOL arm32_osk_signals_ready;
static int arm32_osk_signal;
static void **arm32_osk_fip;                   /* IFrameworkInputPane of the shell (Location + Showing/Hiding callbacks) */
static DWORD arm32_osk_fip_cookie;
static void **arm32_osk_pane;                  /* real WinRT InputPane of the game window (IInputPaneInterop), Windows 10 */
static void **arm32_osk_pane2;                 /* ... as IInputPane2 (TryShow / TryHide) */
static volatile LONG arm32_osk_fip_callbacks;  /* bumped by the shell callbacks (any thread), logged by the main loop */
static LONG arm32_osk_fip_callbacks_seen;
static int arm32_osk_state = -1;               /* last verdict: 1 shown, 0 hidden, -1 unknown */
static DWORD arm32_osk_last_detect;
static float arm32_osk_raised_rect[4];
static BOOL arm32_osk_dismissed_by_user;       /* the user closed the keyboard with its X while the game kept the text focus */
static BOOL arm32_osk_tap_since_dismiss;
static DWORD arm32_osk_pointer_up_tick;
static int arm32_osk_hide_methods[3];
static unsigned arm32_osk_hide_count;
static unsigned arm32_osk_hide_stage;          /* 0 idle, else 1-based index into arm32_osk_hide_methods */
static DWORD arm32_osk_hide_deadline;
static DWORD arm32_osk_last_toggle_tick;       /* last SUCCESSFUL ITipInvocation::Toggle (Toggle is state blind: never twice in a row) */
static BOOL arm32_osk_last_toggle_ok;
static BOOL arm32_osk_last_was_launch;         /* the last show attempt launched TabTip.exe instead of toggling */
static BOOL arm32_osk_last_show_suppressed;    /* the last show attempt's Toggle was held back by the 2 s guard (nothing was sent) */
static BOOL arm32_osk_hide_pending;            /* a TryHide waits ARM32_OSK_HIDE_DELAY_MS: a TryShow inside that time cancels it */
static DWORD arm32_osk_hide_pending_deadline;
static const char *arm32_osk_hide_pending_why;
static DWORD arm32_osk_visible_since;          /* tick when the state last became "visible" */
#define ARM32_OSK_HIDE_DELAY_MS 300u
static BOOL arm32_osk_signal_proven;           /* the state signal was seen to change at least once (or the shell called back) */
static BOOL arm32_osk_state_assumed;           /* arm32_osk_state is the host's own belief (blind / missing signal), not a measurement */
static BOOL arm32_osk_show_after_hide;         /* TryShow arrived while a hide was in flight: show when the hide is done */
static BOOL arm32_osk_hide_when_visible;       /* TryHide arrived while a show was in flight: hide as soon as the keyboard is up */
static DWORD arm32_osk_hide_when_visible_deadline;
static DWORD arm32_osk_last_hide_tick;         /* last TryHide / host hide (a hide landing soon after is not a user dismissal) */
/* the shell's own Showing / Hiding callbacks are the state signal that works on Windows 11 (22631 log: the callback fires ~350 ms after every
 * Toggle while IPTip_Main_Window stays invisible and gave no usable rectangle, so the Win10 window tests read "hidden" for ever and the
 * controller kept Toggling the keyboard open and shut) */
static volatile LONG arm32_osk_cb_kind;        /* last callback: 0 none yet, 1 Showing, 2 Hiding */
static volatile LONG arm32_osk_cb_rect[4];     /* rectangle of the last Showing callback (screen pixels) */
static volatile DWORD arm32_osk_cb_tick;
static BOOL arm32_osk_loc_works;               /* IFrameworkInputPane::Location was seen non-empty: its "empty" can be believed */
static LONG arm32_osk_assume_cb_seq;           /* callback count when the host started to assume a state */
static int arm32_osk_win11 = -1;               /* 1 = Windows 11 (build >= 22000): IPTip_Main_Window is never visible there */

typedef HRESULT (WINAPI *Arm32DwmGetWindowAttributeFn)(HWND, DWORD, void *, DWORD);
static Arm32DwmGetWindowAttributeFn arm32_osk_dwm_get;
static BOOL arm32_osk_dwm_resolved;

static float arm32_osk_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static HWND arm32_osk_find_window(void)
{
    return FindWindowW(L"IPTip_Main_Window", NULL);
}

/* 1 = TabTip window exists, is not cloaked and is "visible"; 0 = no window / cloaked / invisible; -1 = DWM not available */
static int arm32_osk_cloak_verdict(void)
{
    HWND tip = arm32_osk_find_window();
    DWORD cloaked = 0;

    if (!arm32_osk_dwm_resolved) {
        HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
        arm32_osk_dwm_resolved = TRUE;
        arm32_osk_dwm_get = dwmapi ? (Arm32DwmGetWindowAttributeFn)GetProcAddress(dwmapi, "DwmGetWindowAttribute") : NULL;
    }
    if (!tip) return 0;
    if (!arm32_osk_dwm_get) return -1;
    if (arm32_osk_dwm_get(tip, 14 /* DWMWA_CLOAKED */, &cloaked, sizeof(cloaked)) != 0) return -1;
    return (cloaked == 0 && IsWindowVisible(tip)) ? 1 : 0;
}

/* Windows 11 (build >= 22000): the touch keyboard lives in TextInputHost.exe; the IPTip_Main_Window of TabTip exists but is never "visible" */
static BOOL arm32_osk_is_win11(void)
{
    if (arm32_osk_win11 < 0) {
        typedef struct { ULONG size, major, minor, build, platform; WCHAR csd[128]; } OsVersion;
        typedef LONG (WINAPI *RtlGetVersionFn)(void *);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        RtlGetVersionFn function = ntdll ? (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
        OsVersion info;

        arm32_osk_win11 = 0;
        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (function && function(&info) >= 0 && info.major >= 10 && info.build >= 22000u) arm32_osk_win11 = 1;
    }
    return arm32_osk_win11 == 1;
}

/* is the touch keyboard on screen?  1 / 0 / -1 (no usable signal).  Strong signals first; the weak cloak flag only when nothing else answers. */
static int arm32_osk_detect(void)
{
    int strong = -1;
    HWND tip = arm32_osk_find_window();
    LONG callback_kind = arm32_osk_cb_kind;

    /* a TabTip window that does not exist / is not visible cannot show a keyboard: IFrameworkInputPane and InputPane stay "visible" (stale)
     * after TabTip.exe was killed or restarted (probe log: tip=0/vis=0 while fip kept its rectangle).  Windows 11 never shows that window,
     * so there only its existence counts. */
    if (!tip) return 0;
    if (!IsWindowVisible(tip) && !arm32_osk_is_win11()) return 0;

    if (arm32_osk_fip) {
        RECT rc;
        memset(&rc, 0, sizeof(rc));
        if (SUCCEEDED(((HRESULT (WINAPI *)(void *, RECT *))((void **)arm32_osk_fip[0])[6])(arm32_osk_fip, &rc))) {
            strong = (rc.bottom > rc.top && rc.right > rc.left) ? 1 : 0;
            if (strong) arm32_osk_loc_works = TRUE;
        }
    }
    if (arm32_osk_pane) {
        float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (SUCCEEDED(((HRESULT (WINAPI *)(void *, float *))((void **)arm32_osk_pane[0])[10])(arm32_osk_pane, rect))) {
            if (rect[3] > 0.5f) return 1;
            if (strong < 0) strong = 0;
        }
    }
    if (callback_kind && arm32_osk_is_win11()) {
        /* Windows 11 only (Windows 8.1 / 10 keep exactly the probe-verified rectangle logic below): the shell told us itself (Showing = 1,
         * Hiding = 2) and the callback is the truth, except that a rectangle that has proven reliable overrules a callback older than the
         * keyboard's animation (the user closed it some way that sent no Hiding) */
        int from_callback = callback_kind == 1 ? 1 : 0;
        DWORD age = GetTickCount() - arm32_osk_cb_tick;

        if (from_callback == 1 && strong == 0 && arm32_osk_loc_works && age > 1500u) return 0;
        return from_callback;
    }
    if (strong >= 0) return strong;
    return arm32_osk_cloak_verdict();
}

/* one log line with every signal at once: what the next device log needs to show how a Windows version reports its keyboard */
static void arm32_osk_snapshot(const char *tag)
{
    HWND tip = arm32_osk_find_window();
    HWND host = FindWindowW(L"Windows.UI.Core.CoreWindow", L"Microsoft Text Input Application");
    RECT tip_rect;
    RECT fip_rect;
    float pane_rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    HRESULT fip_hr = E_FAIL;
    HRESULT pane_hr = E_FAIL;
    char line[420];

    memset(&tip_rect, 0, sizeof(tip_rect));
    memset(&fip_rect, 0, sizeof(fip_rect));
    if (tip) GetWindowRect(tip, &tip_rect);
    if (arm32_osk_fip) fip_hr = ((HRESULT (WINAPI *)(void *, RECT *))((void **)arm32_osk_fip[0])[6])(arm32_osk_fip, &fip_rect);
    if (arm32_osk_pane) pane_hr = ((HRESULT (WINAPI *)(void *, float *))((void **)arm32_osk_pane[0])[10])(arm32_osk_pane, pane_rect);
    wsprintfA(line, "ARM32 osk: snapshot (%s): win11=%d tipWindow=%d visible=%d cloak=%d rect=(%ld,%ld,%ld,%ld) | fip hr=0x%08lx rect=(%ld,%ld,%ld,%ld) | "
                    "pane hr=0x%08lx bottom=%d | textInputHost=%d visible=%d | callbacks kind=%ld rect=(%ld,%ld,%ld,%ld) | state=%d assumed=%d detect=%d",
              tag, (int)arm32_osk_is_win11(), tip != NULL, tip ? (int)IsWindowVisible(tip) : 0, arm32_osk_cloak_verdict(),
              tip_rect.left, tip_rect.top, tip_rect.right, tip_rect.bottom,
              (unsigned long)fip_hr, fip_rect.left, fip_rect.top, fip_rect.right, fip_rect.bottom,
              (unsigned long)pane_hr, (int)pane_rect[3], host != NULL, host ? (int)IsWindowVisible(host) : 0,
              (long)arm32_osk_cb_kind, (long)arm32_osk_cb_rect[0], (long)arm32_osk_cb_rect[1], (long)arm32_osk_cb_rect[2], (long)arm32_osk_cb_rect[3],
              arm32_osk_state, (int)arm32_osk_state_assumed, arm32_osk_detect());
    log_line(line);
}

/* IFrameworkInputPaneHandler: the shell calls it on a thread of its choice; it only counts, the main loop logs and acts */
static HRESULT WINAPI arm32_osk_fip_handler_query(void *self, const GUID *iid, void **out)
{
    static const GUID iid_unknown = { 0x00000000U, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
    static const GUID iid_handler = { 0x226C537BU, 0x1E76, 0x4D9E, { 0xA7, 0x60, 0x33, 0xDB, 0x29, 0x92, 0x2F, 0x18 } };

    if (!out) return E_POINTER;
    if (iid && (memcmp(iid, &iid_unknown, sizeof(GUID)) == 0 || memcmp(iid, &iid_handler, sizeof(GUID)) == 0)) {
        *out = self;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI arm32_osk_fip_handler_add_ref(void *self)
{
    (void)self;
    return 2;
}

static ULONG WINAPI arm32_osk_fip_handler_release(void *self)
{
    (void)self;
    return 1;
}

static HRESULT WINAPI arm32_osk_fip_handler_showing(void *self, RECT *rect, BOOL ensure)
{
    (void)self;
    (void)ensure;
    arm32_osk_cb_rect[0] = rect ? rect->left : 0;
    arm32_osk_cb_rect[1] = rect ? rect->top : 0;
    arm32_osk_cb_rect[2] = rect ? rect->right : 0;
    arm32_osk_cb_rect[3] = rect ? rect->bottom : 0;
    arm32_osk_cb_tick = GetTickCount();
    arm32_osk_cb_kind = 1;
    InterlockedIncrement(&arm32_osk_fip_callbacks);
    return S_OK;
}

static HRESULT WINAPI arm32_osk_fip_handler_hiding(void *self, BOOL ensure)
{
    (void)self;
    (void)ensure;
    arm32_osk_cb_tick = GetTickCount();
    arm32_osk_cb_kind = 2;
    InterlockedIncrement(&arm32_osk_fip_callbacks);
    return S_OK;
}

static const void *arm32_osk_fip_handler_vtable[5] = {
    arm32_osk_fip_handler_query, arm32_osk_fip_handler_add_ref, arm32_osk_fip_handler_release,
    arm32_osk_fip_handler_showing, arm32_osk_fip_handler_hiding
};
static const void *arm32_osk_fip_handler_object = arm32_osk_fip_handler_vtable;

/* once, when the game window exists: find the best keyboard-state signal and log what each one answered */
static void arm32_osk_init_signals(void)
{
    typedef HRESULT (WINAPI *CoCreateInstanceFn)(const GUID *, void *, DWORD, const GUID *, void **);
    typedef HRESULT (WINAPI *RoGetActivationFactoryFn)(void *, const GUID *, void **);
    typedef HRESULT (WINAPI *WindowsCreateStringFn)(const WCHAR *, UINT32, void **);
    typedef HRESULT (WINAPI *WindowsDeleteStringFn)(void *);
    static const GUID clsid_fip = { 0xD5120AA3U, 0x46BA, 0x44C5, { 0x82, 0x2D, 0xCA, 0x80, 0x92, 0xC1, 0xFC, 0x72 } };
    static const GUID iid_fip = { 0x5752238BU, 0x24F0, 0x495A, { 0x82, 0xF1, 0x2F, 0xD5, 0x93, 0x05, 0x67, 0x96 } };
    static const GUID iid_interop = { 0x75CF2C57U, 0x9195, 0x4931, { 0x83, 0x32, 0xF0, 0xB4, 0x09, 0xE9, 0x16, 0xAF } };
    static const GUID iid_pane = { 0x640ada70U, 0x06f3, 0x4c87, { 0xa6, 0x78, 0x98, 0x29, 0xc9, 0x12, 0x7c, 0x28 } };
    static const GUID iid_pane2 = { 0x8a6b3f26U, 0x7090, 0x4793, { 0x94, 0x4c, 0xc3, 0xf2, 0xcd, 0xe2, 0x62, 0x76 } };
    static const WCHAR class_name[] = L"Windows.UI.ViewManagement.InputPane";
    HMODULE ole32;
    HMODULE combase;
    CoCreateInstanceFn create_instance;
    HRESULT hr_fip = E_FAIL, hr_advise = E_FAIL, hr_interop = E_FAIL, hr_pane = E_FAIL;
    int cloak_state;
    char line[320];

    if (arm32_osk_signals_ready || !game_window) return;
    arm32_osk_signals_ready = TRUE;
    ole32 = LoadLibraryW(L"ole32.dll");
    create_instance = ole32 ? (CoCreateInstanceFn)GetProcAddress(ole32, "CoCreateInstance") : NULL;
    if (create_instance) {
        void **object = NULL;
        hr_fip = create_instance(&clsid_fip, NULL, 1u /* CLSCTX_INPROC_SERVER */, &iid_fip, (void **)&object);
        if (SUCCEEDED(hr_fip) && object) {
            arm32_osk_fip = object;
            hr_advise = ((HRESULT (WINAPI *)(void *, HWND, void *, DWORD *))((void **)object[0])[4])(
                object, game_window, (void *)&arm32_osk_fip_handler_object, &arm32_osk_fip_cookie);
            {
                RECT probe_rect;
                memset(&probe_rect, 0, sizeof(probe_rect));
                if (FAILED(((HRESULT (WINAPI *)(void *, RECT *))((void **)object[0])[6])(object, &probe_rect))) {
                    log_line("ARM32 osk: IFrameworkInputPane::Location fails in this thread - the shell signal is not used");
                    ((ULONG (WINAPI *)(void *))((void **)object[0])[2])(object);
                    arm32_osk_fip = NULL;
                }
            }
        }
    }
    combase = LoadLibraryW(L"combase.dll");
    if (combase) {
        RoGetActivationFactoryFn get_factory = (RoGetActivationFactoryFn)GetProcAddress(combase, "RoGetActivationFactory");
        WindowsCreateStringFn create_string = (WindowsCreateStringFn)GetProcAddress(combase, "WindowsCreateString");
        WindowsDeleteStringFn delete_string = (WindowsDeleteStringFn)GetProcAddress(combase, "WindowsDeleteString");
        void *name = NULL;
        void **factory = NULL;
        void **pane = NULL;
        if (get_factory && create_string && delete_string && SUCCEEDED(create_string(class_name, (UINT32)(ARRAYSIZE(class_name) - 1), &name))) {
            hr_interop = get_factory(name, &iid_interop, (void **)&factory);
            delete_string(name);
            if (SUCCEEDED(hr_interop) && factory) {
                hr_pane = ((HRESULT (WINAPI *)(void *, HWND, const GUID *, void **))((void **)factory[0])[6])(factory, game_window, &iid_pane, (void **)&pane);
                if (SUCCEEDED(hr_pane) && pane) {
                    void **pane2 = NULL;
                    arm32_osk_pane = pane;
                    if (SUCCEEDED(((HRESULT (WINAPI *)(void *, const GUID *, void **))((void **)pane[0])[0])(pane, &iid_pane2, (void **)&pane2))) {
                        arm32_osk_pane2 = pane2;
                    }
                }
            }
        }
    }
    cloak_state = arm32_osk_cloak_verdict();
    arm32_osk_signal = arm32_osk_fip ? ARM32_OSK_SIG_FIP : (arm32_osk_pane ? ARM32_OSK_SIG_PANE : (cloak_state >= 0 ? ARM32_OSK_SIG_CLOAK : ARM32_OSK_SIG_NONE));
    wsprintfA(line, "ARM32 osk: signals: IFrameworkInputPane create=0x%08lx advise=0x%08lx | InputPane interop=0x%08lx getForWindow=0x%08lx "
                    "TryHide=%s | dwm-cloak verdict=%d | primary=%s",
              (unsigned long)hr_fip, (unsigned long)hr_advise, (unsigned long)hr_interop, (unsigned long)hr_pane,
              arm32_osk_pane2 ? "yes" : "no", cloak_state, arm32_osk_signal_names[arm32_osk_signal]);
    log_line(line);
}

static BOOL arm32_osk_visible_now(void)
{
    return arm32_osk_state == 1;
}

static BOOL arm32_osk_eligible(void)
{
    if (arm32_osk_policy == ARM32_OSK_OFF) return FALSE;
    if (arm32_osk_policy == ARM32_OSK_ON) return TRUE;
    return arm32_input_touch_active();
}

typedef struct Arm32StartupInfo {
    DWORD cb;
    LPWSTR reserved, desktop, title;
    DWORD x, y, x_size, y_size, x_chars, y_chars, fill_attribute, flags;
    WORD show_window, reserved2_size;
    BYTE *reserved2;
    HANDLE std_input, std_output, std_error;
} Arm32StartupInfo;

typedef struct Arm32ProcessInformation {
    HANDLE process, thread;
    DWORD process_id, thread_id;
} Arm32ProcessInformation;

static BOOL arm32_osk_launch_tabtip(void)
{
    wchar_t path[MAX_PATH];
    Arm32StartupInfo startup;
    Arm32ProcessInformation process;
    BOOL ok;
    char narrow[MAX_PATH];
    char line[MAX_PATH + 80];

    path[0] = 0;
    if (FAILED(SHGetFolderPathW(NULL, 0x2b /* CSIDL_PROGRAM_FILES_COMMON */, NULL, 0, path)) || !path[0] ||
        !append_wide_path(path, L"\\microsoft shared\\ink\\TabTip.exe")) {
        log_line("ARM32 osk: could not build the TabTip.exe path (SHGetFolderPathW CSIDL_PROGRAM_FILES_COMMON)");
        return FALSE;
    }
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    ok = CreateProcessW(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process);
    arm32_iat_narrow_for_log(narrow, sizeof(narrow), path);
    wsprintfA(line, "ARM32 osk: launched TabTip.exe path=%s result=%d error=%lu", narrow, (int)ok, ok ? 0ul : (unsigned long)GetLastError());
    log_line(line);
    if (ok) {
        if (process.thread) CloseHandle(process.thread);
        if (process.process) CloseHandle(process.process);
    }
    return ok;
}

/* Toggle is state blind (it shows AND hides), so a second one right after the first could undo it: no two Toggles within 2 s - unless the shell
 * has reported a change since the last Toggle (then the first one has taken effect and the next one is safe) */
static BOOL arm32_osk_toggle_guard_active(void)
{
    if (!arm32_osk_last_toggle_ok || (GetTickCount() - arm32_osk_last_toggle_tick) >= 2000u) return FALSE;
    if (arm32_osk_cb_kind && (int)(arm32_osk_cb_tick - arm32_osk_last_toggle_tick) > 0) return FALSE;
    return TRUE;
}

/* ITipInvocation::Toggle(GetDesktopWindow()) on CLSID_UIHostNoLaunch: SHOWS a hidden keyboard and HIDES a shown one (state blind) */
static HRESULT arm32_osk_toggle_com(const char *why)
{
    typedef HRESULT (WINAPI *CoCreateInstanceFn)(const GUID *, void *, DWORD, const GUID *, void **);
    static const GUID clsid_ui_host_no_launch = { 0x4ce576faU, 0x83dc, 0x4f88, { 0x95, 0x1c, 0x9d, 0x07, 0x82, 0xb4, 0xe3, 0x76 } };
    static const GUID iid_tip_invocation = { 0x37c994e7U, 0x432b, 0x4834, { 0xa2, 0xf7, 0xdc, 0xe1, 0xf1, 0x3b, 0x83, 0x4b } };
    HMODULE ole32 = LoadLibraryW(L"ole32.dll");
    CoCreateInstanceFn create = ole32 ? (CoCreateInstanceFn)GetProcAddress(ole32, "CoCreateInstance") : NULL;
    void **object = NULL;
    HRESULT result;
    char line[160];

    if (!create) {
        log_line("ARM32 osk: CoCreateInstance is not available - ITipInvocation::Toggle skipped");
        return E_FAIL;
    }
    if (arm32_osk_toggle_guard_active()) {
        /* Toggle shows AND hides: a second one right after the first would undo it (the keyboard takes up to 400 ms to appear) */
        log_line("ARM32 osk: Toggle suppressed - the previous Toggle was less than 2 s ago");
        return ARM32_OSK_E_ABORT;
    }
    result = create(&clsid_ui_host_no_launch, NULL, 0x2u | 0x4u /* INPROC_HANDLER | LOCAL_SERVER */, &iid_tip_invocation,
                    (void **)&object);
    if (FAILED(result) || !object) {
        wsprintfA(line, "ARM32 osk: %s: CoCreateInstance(UIHostNoLaunch / ITipInvocation) failed, hr=0x%08lx", why, (unsigned long)result);
        log_line(line);
        return FAILED(result) ? result : E_FAIL;
    }
    result = ((HRESULT (WINAPI *)(void *, HWND))((void **)*object)[3])(object, GetDesktopWindow());
    ((ULONG (WINAPI *)(void *))((void **)*object)[2])(object);
    arm32_osk_last_toggle_ok = SUCCEEDED(result);
    if (SUCCEEDED(result)) arm32_osk_last_toggle_tick = GetTickCount();
    wsprintfA(line, "ARM32 osk: %s: ITipInvocation::Toggle hr=0x%08lx", why, (unsigned long)result);
    log_line(line);
    return result;
}

static void arm32_osk_assume_state(int state, const char *why)
{
    char line[160];

    arm32_osk_state_assumed = state ? TRUE : FALSE;
    arm32_osk_assume_cb_seq = arm32_osk_fip_callbacks;
    if (arm32_osk_state == state) return;
    if (state == 1) arm32_osk_visible_since = GetTickCount();
    arm32_osk_state = state;
    wsprintfA(line, "ARM32 osk: no state signal - assuming the keyboard is %s (%s)", state ? "visible" : "hidden", why);
    log_line(line);
    arm32_input_pane_raise(state ? TRUE : FALSE);
}

/* compare the keyboard state with the last verdict; raises the game's Showing / Hiding handlers on a change (main loop only) */
static void arm32_osk_update_state(void)
{
    int detected = arm32_osk_detect();
    int old = arm32_osk_state;
    char line[240];

    if (detected < 0) return;
    if (arm32_osk_state_assumed && arm32_osk_is_win11() && arm32_osk_fip_callbacks != arm32_osk_assume_cb_seq) {
        /* the shell called back after the assumption was made: it is a real signal, measure again */
        arm32_osk_state_assumed = FALSE;
        log_line("ARM32 osk: a shell Showing/Hiding callback arrived - the assumed keyboard state is replaced by the measured one");
    }
    if (arm32_osk_state_assumed) {
        /* the host believes the keyboard is up without a trustworthy signal: keep the belief until a hide / show request replaces it */
        if (detected == 1) arm32_osk_state_assumed = FALSE;
        return;
    }
    if (detected == old) {
        if (detected == 1 && (arm32_osk_want || arm32_osk_ours)) {
            float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            if (arm32_osk_rect_buffer(rect) &&
                (arm32_osk_absf(rect[1] - arm32_osk_raised_rect[1]) > 2.0f || arm32_osk_absf(rect[3] - arm32_osk_raised_rect[3]) > 2.0f)) {
                arm32_osk_raised_rect[0] = rect[0];
                arm32_osk_raised_rect[1] = rect[1];
                arm32_osk_raised_rect[2] = rect[2];
                arm32_osk_raised_rect[3] = rect[3];
                log_line("ARM32 osk: keyboard rectangle changed - raising the game's Showing handler again");
                arm32_input_pane_raise(TRUE);
            }
        }
        return;
    }
    arm32_osk_state = detected;
    if (detected == 1) arm32_osk_visible_since = GetTickCount();
    if (old >= 0) arm32_osk_signal_proven = TRUE;
    wsprintfA(line, "ARM32 osk: keyboard %s (signal %s, was %d)", detected ? "VISIBLE" : "hidden", arm32_osk_signal_names[arm32_osk_signal], old);
    log_line(line);
    if (detected == 1) {
        if (arm32_osk_want || arm32_osk_ours) {
            float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            if (arm32_osk_rect_buffer(rect)) memcpy(arm32_osk_raised_rect, rect, sizeof(rect));
            arm32_input_pane_raise(TRUE);
        }
    } else if (old == 1) {
        if (arm32_osk_ours && !arm32_osk_hide_stage && !arm32_osk_hide_when_visible &&
            (!arm32_osk_last_hide_tick || (GetTickCount() - arm32_osk_last_hide_tick) > 1500u)) {
            arm32_osk_dismissed_by_user = TRUE;
            arm32_osk_tap_since_dismiss = FALSE;
            arm32_osk_ours = FALSE;
            log_line("ARM32 osk: the keyboard was closed by the user (its X); the game keeps the text focus - a tap on it opens the keyboard again");
        }
        arm32_input_pane_raise(FALSE);
    }
}

static void arm32_osk_show_attempt(void)
{
    HWND tip = arm32_osk_find_window();
    char line[200];
    HRESULT result;

    ++arm32_osk_attempts;
    arm32_osk_last_attempt = GetTickCount();
    arm32_osk_last_show_suppressed = FALSE;
    wsprintfA(line, "ARM32 osk: show attempt %u: TabTip window %s", arm32_osk_attempts, tip ? "exists -> ITipInvocation::Toggle" : "not found -> launch TabTip.exe");
    log_line(line);
    if (!tip) {
        arm32_osk_launched = TRUE;
        arm32_osk_last_was_launch = TRUE;
        arm32_osk_launch_tabtip();
        return;
    }
    arm32_osk_last_was_launch = FALSE;
    result = arm32_osk_toggle_com("show");
    arm32_osk_last_show_suppressed = result == ARM32_OSK_E_ABORT;
    if (FAILED(result) && result != ARM32_OSK_E_ABORT && (!arm32_osk_launched || arm32_osk_attempts >= 3)) {
        arm32_osk_launched = TRUE;
        arm32_osk_last_was_launch = TRUE;
        arm32_osk_launch_tabtip();
    }
}

/* Toggle is state blind (it also HIDES), so a second Toggle is only sent when the previous action had time to work, the state signal
 * says "hidden" and (before the signal was ever seen to change) the Toggle did not already report success */
static void arm32_osk_show_progress(DWORD now)
{
    char line[220];

    if (arm32_osk_hide_stage) return;     /* a hide is in flight: arm32_osk_show remembered the request */
    if (arm32_osk_state == 1) {
        static volatile LONG logged;
        arm32_osk_want = FALSE;
        arm32_osk_ours = TRUE;
        if (InterlockedIncrement(&logged) <= 60) {
            wsprintfA(line, "ARM32 osk: the keyboard is visible (%lu ms after the request, %u attempt(s))", (unsigned long)(now - arm32_osk_want_since),
                      arm32_osk_attempts);
            log_line(line);
        }
        return;
    }
    if (arm32_osk_attempts && arm32_osk_last_show_suppressed) {
        /* the last attempt sent nothing (the 2 s Toggle guard held it back): retry right after the guard, and draw no conclusion from a Toggle that
         * never ran (Windows 11 1.2.8 log: a re-tap 1 s after the keyboard went down only got the keyboard 2.9 s later) */
        if ((int)(now - (arm32_osk_last_toggle_tick + 2100u)) < 0 && arm32_osk_toggle_guard_active()) return;
    } else if (arm32_osk_attempts) {
        DWORD wait = arm32_osk_last_toggle_ok ? 2500u : (arm32_osk_last_was_launch ? (arm32_osk_state == 0 && arm32_osk_find_window() ? 300u : 1600u) : 400u);

        if ((now - arm32_osk_last_attempt) < wait) return;
        if (arm32_osk_state != 0 && (arm32_osk_last_toggle_ok || arm32_osk_last_was_launch)) {
            /* no trustworthy signal: one action per request (a second Toggle could hide what the first one showed) */
            arm32_osk_want = FALSE;
            arm32_osk_ours = TRUE;
            arm32_osk_assume_state(1, "one show action done");
            return;
        }
        if (arm32_osk_state == 0 && arm32_osk_last_toggle_ok && !arm32_osk_signal_proven) {
            /* Toggle reported success but the (never proven) signal still says hidden: it may be blind - believe the Toggle, do not toggle again */
            arm32_osk_want = FALSE;
            arm32_osk_ours = TRUE;
            arm32_osk_assume_state(1, "Toggle succeeded but the state signal never changed - treating the signal as blind");
            return;
        }
        if (arm32_osk_state == 0 && arm32_osk_cloak_verdict() == 1) {
            arm32_osk_want = FALSE;
            arm32_osk_ours = TRUE;
            arm32_osk_assume_state(1, "the signals disagree (the TabTip window is not cloaked)");
            return;
        }
    }
    /* a state signal that has been seen to work says "hidden": every further Toggle that does not show the keyboard is a failure to report,
     * not something to repeat ten times (Toggle also HIDES - the 22631 log shows the keyboard flickering open and shut under 10 attempts) */
    if (arm32_osk_attempts >= ((arm32_osk_is_win11() && arm32_osk_signal_proven) ? 4u : 10u)) {
        char message[200];
        arm32_osk_want = FALSE;
        wsprintfA(message, "ARM32 osk: the touch keyboard did NOT appear after %u attempts - giving up; text can still be typed on a hardware keyboard (send this log)",
                  arm32_osk_attempts);
        log_line(message);
        arm32_osk_snapshot("gave up");
        return;
    }
    arm32_osk_show_attempt();
}

static BOOL arm32_osk_hide_retry_pending;      /* the Toggle of the current hide method was held back (previous Toggle < 2 s ago): run it again */
static unsigned arm32_osk_hide_retries;

/* FALSE = this method could not run yet (Toggle held back by the 2 s guard) and must be tried again, not skipped */
static BOOL arm32_osk_hide_run_method(int method)
{
    char line[200];
    HWND tip = arm32_osk_find_window();
    HRESULT result = S_OK;
    BYTE accepted = 0;

    if (method == ARM32_OSK_HIDE_TRYHIDE) {
        result = arm32_osk_pane2 ? ((HRESULT (WINAPI *)(void *, BYTE *))((void **)arm32_osk_pane2[0])[7])(arm32_osk_pane2, &accepted) : E_FAIL;
        wsprintfA(line, "ARM32 osk: hide via InputPane2::TryHide hr=0x%08lx accepted=%d", (unsigned long)result, (int)accepted);
        log_line(line);
    } else if (method == ARM32_OSK_HIDE_CLOSE) {
        BOOL posted = tip ? PostMessageW(tip, ARM32_WM_SYSCOMMAND, ARM32_SC_CLOSE, 0) : FALSE;
        wsprintfA(line, "ARM32 osk: hide via SC_CLOSE to the TabTip window %p posted=%d", (void *)tip, (int)posted);
        log_line(line);
    } else {
        /* Toggle SHOWS a hidden keyboard: only toggle one that is believed to be up */
        if (arm32_osk_state != 1) {
            log_line("ARM32 osk: hide via Toggle skipped - the keyboard is not believed to be visible (a Toggle would open it)");
            return TRUE;
        }
        if (arm32_osk_toggle_com("hide") == ARM32_OSK_E_ABORT) return FALSE;
    }
    return TRUE;
}

/* run a hide method; a held-back Toggle is retried after the guard interval instead of moving on to the next method */
static void arm32_osk_hide_start_method(int method, DWORD now)
{
    if (!arm32_osk_hide_run_method(method) && arm32_osk_hide_retries < 3) {
        ++arm32_osk_hide_retries;
        arm32_osk_hide_retry_pending = TRUE;
        arm32_osk_hide_deadline = arm32_osk_last_toggle_tick + 2100u;
        if ((int)(arm32_osk_hide_deadline - now) <= 0) arm32_osk_hide_deadline = now + 100u;
        return;
    }
    arm32_osk_hide_retry_pending = FALSE;
    arm32_osk_hide_deadline = now + (method == ARM32_OSK_HIDE_TOGGLE ? 2200u : 550u);
}

static void arm32_osk_begin_hide(const char *why);

static void arm32_osk_hide_done(void)
{
    arm32_osk_hide_stage = 0;
    arm32_osk_hide_retry_pending = FALSE;
    arm32_osk_ours = FALSE;
    if (arm32_osk_show_after_hide) {
        arm32_osk_show_after_hide = FALSE;
        arm32_osk_dismissed_by_user = FALSE;
        arm32_osk_tap_since_dismiss = FALSE;
        arm32_osk_want = TRUE;
        arm32_osk_attempts = 0;
        arm32_osk_want_since = GetTickCount();
        log_line("ARM32 osk: the hide is done - now doing the show the game asked for meanwhile");
    }
}

static void arm32_osk_hide_progress(DWORD now)
{
    char line[200];

    if (arm32_osk_state <= 0 || (arm32_osk_state_assumed && !arm32_osk_hide_retry_pending)) {
        if (arm32_osk_state < 0 || arm32_osk_state_assumed) arm32_osk_assume_state(0, "one hide action done");
        log_line("ARM32 osk: the keyboard is hidden");
        arm32_osk_hide_done();
        return;
    }
    if ((int)(now - arm32_osk_hide_deadline) < 0 && !(arm32_osk_hide_retry_pending && !arm32_osk_toggle_guard_active())) return;
    if (arm32_osk_hide_retry_pending) {
        arm32_osk_hide_start_method(arm32_osk_hide_methods[arm32_osk_hide_stage - 1], now);
        return;
    }
    if (arm32_osk_hide_stage >= arm32_osk_hide_count) {
        wsprintfA(line, "ARM32 osk: the keyboard is STILL visible after %u hide method(s) - giving up (send this log)", arm32_osk_hide_count);
        log_line(line);
        arm32_osk_snapshot("hide gave up");
        arm32_osk_hide_done();
        return;
    }
    ++arm32_osk_hide_stage;
    arm32_osk_hide_retries = 0;
    arm32_osk_hide_start_method(arm32_osk_hide_methods[arm32_osk_hide_stage - 1], now);
}

/* once per main-loop pass (arm32_input_frame): detect state changes, drive show / hide, tap-to-reopen */
static void arm32_osk_poll(void)
{
    DWORD now;
    BOOL busy;
    BOOL idle = !(arm32_osk_want || arm32_osk_ours || arm32_osk_hide_stage || arm32_osk_dismissed_by_user || arm32_osk_hide_when_visible ||
                  arm32_osk_hide_pending);

    if (idle) {
        /* Windows 11: the shell also shows / hides the keyboard on its own (a tap on a text field, Enter, a focus change) - keep the state current
         * and log it even when no request is pending (1.2.8 log 2026-10-21: four callbacks arrived while idle and were invisible, so the state was
         * stale, typed keys flipped the game to Mouse mode and a later show request could have toggled a visible keyboard shut) */
        if (!(arm32_osk_signals_ready && arm32_osk_is_win11() && arm32_osk_fip_callbacks != arm32_osk_fip_callbacks_seen)) return;
    }
    arm32_osk_init_signals();
    now = GetTickCount();
    busy = arm32_osk_want || arm32_osk_hide_stage || arm32_osk_hide_when_visible;
    if (arm32_osk_fip_callbacks != arm32_osk_fip_callbacks_seen) {
        char line[240];
        LONG skipped = arm32_osk_fip_callbacks - arm32_osk_fip_callbacks_seen - 1;
        arm32_osk_fip_callbacks_seen = arm32_osk_fip_callbacks;
        /* Windows 11 reads the callback as the state itself (arm32_osk_detect) and "signal proven" is set when the measured state really
         * changes; Windows 8.1 / 10 count a callback as proof as they always did */
        if (!arm32_osk_is_win11()) arm32_osk_signal_proven = TRUE;
        if (arm32_osk_cb_kind == 1) {
            wsprintfA(line, "ARM32 osk: shell Showing callback #%ld rect=(%ld,%ld,%ld,%ld)%s, arrived %lu ms ago", (long)arm32_osk_fip_callbacks_seen,
                      (long)arm32_osk_cb_rect[0], (long)arm32_osk_cb_rect[1], (long)arm32_osk_cb_rect[2], (long)arm32_osk_cb_rect[3],
                      idle ? " while no request was pending" : "", (unsigned long)(now - arm32_osk_cb_tick));
        } else {
            wsprintfA(line, "ARM32 osk: shell Hiding callback #%ld%s, arrived %lu ms ago", (long)arm32_osk_fip_callbacks_seen,
                      idle ? " while no request was pending" : "", (unsigned long)(now - arm32_osk_cb_tick));
        }
        log_line(line);
        if (skipped > 0) {
            wsprintfA(line, "ARM32 osk: (%ld earlier shell callback(s) since the last logged one were not seen individually)", (long)skipped);
            log_line(line);
        }
        if (arm32_osk_fip_callbacks_seen <= 6) arm32_osk_snapshot("after the shell callback");
        arm32_osk_last_detect = 0;
    }
    if (arm32_osk_last_detect == 0 || (now - arm32_osk_last_detect) >= (busy ? 60u : 150u)) {
        arm32_osk_last_detect = now ? now : 1u;
        arm32_osk_update_state();
    }
    if (arm32_osk_hide_when_visible) {
        if (arm32_osk_state == 1) {
            arm32_osk_hide_when_visible = FALSE;
            arm32_osk_ours = TRUE;
            log_line("ARM32 osk: the keyboard came up after the game already asked to hide it - hiding it now");
            arm32_osk_begin_hide("deferred TryHide");
        } else if ((int)(now - arm32_osk_hide_when_visible_deadline) >= 0) {
            arm32_osk_hide_when_visible = FALSE;
        }
    }
    if (arm32_osk_hide_pending && (int)(now - arm32_osk_hide_pending_deadline) >= 0) {
        arm32_osk_hide_pending = FALSE;
        if (arm32_osk_state == 1) {
            arm32_osk_begin_hide(arm32_osk_hide_pending_why);
        } else {
            arm32_osk_ours = FALSE;
            log_line("ARM32 osk: the keyboard was already gone when the delayed hide was due - nothing to do");
        }
    }
    if (arm32_osk_hide_stage) arm32_osk_hide_progress(now);
    if (arm32_osk_want) arm32_osk_show_progress(now);
    if (arm32_osk_dismissed_by_user) {
        if (!text_input_active) {
            arm32_osk_dismissed_by_user = FALSE;
            arm32_osk_tap_since_dismiss = FALSE;
            log_line("ARM32 osk: the game's text focus ended after the user closed the keyboard");
        } else if (arm32_osk_tap_since_dismiss && arm32_osk_pointer_up_tick && (now - arm32_osk_pointer_up_tick) >= 300u) {
            arm32_osk_pointer_up_tick = 0;
            arm32_osk_tap_since_dismiss = FALSE;
            arm32_osk_dismissed_by_user = FALSE;
            if (arm32_osk_eligible() && arm32_osk_state != 1 && !arm32_osk_want) {
                log_line("ARM32 osk: tap while the text box still has focus - opening the keyboard again");
                arm32_osk_want = TRUE;
                arm32_osk_attempts = 0;
                arm32_osk_want_since = now;
            }
        }
    }
}

/* a finger was lifted: after a user dismissal this is the "tap on the text box again" signal */
static void arm32_osk_note_pointer_up(void)
{
    if (!arm32_osk_dismissed_by_user) return;
    arm32_osk_tap_since_dismiss = TRUE;
    arm32_osk_pointer_up_tick = GetTickCount();
}

static void arm32_osk_show(const char *why)
{
    static volatile LONG logged;
    char line[220];
    BOOL eligible = arm32_osk_eligible();

    if (InterlockedIncrement(&logged) <= 80) {
        wsprintfA(line, "ARM32 osk: show requested (%s) eligible=%d policy=%s touchActive=%d state=%d", why ? why : "?", (int)eligible,
                  arm32_osk_policy_names[arm32_osk_policy], (int)arm32_input_touch_active(), arm32_osk_state);
        log_line(line);
    }
    if (!eligible) return;
    if (arm32_osk_hide_pending) {
        arm32_osk_hide_pending = FALSE;
        log_line("ARM32 osk: delayed hide cancelled - the game asked for the keyboard again right away (it stays up)");
    }
    if (arm32_osk_want) return;        /* a request is already in flight: restarting it could send a second (state blind) Toggle */
    if (arm32_osk_dismissed_by_user && !arm32_osk_tap_since_dismiss) {
        log_line("ARM32 osk: show ignored - the user closed the keyboard and has not tapped since");
        return;
    }
    if (arm32_osk_hide_stage) {
        arm32_osk_show_after_hide = TRUE;
        log_line("ARM32 osk: show requested while a hide is in flight - it will run when the hide is done");
        return;
    }
    arm32_osk_hide_when_visible = FALSE;
    arm32_osk_dismissed_by_user = FALSE;
    arm32_osk_tap_since_dismiss = FALSE;
    arm32_osk_want = TRUE;
    arm32_osk_attempts = 0;
    arm32_osk_want_since = GetTickCount();
    arm32_osk_last_detect = 0;
    /* the state check, the actions and the game's Showing event happen in arm32_osk_poll on the next loop pass (never inside the game's call) */
}

static void arm32_osk_begin_hide(const char *why)
{
    char line[200];
    unsigned count = 0;

    arm32_osk_init_signals();
    if (arm32_osk_pane2) arm32_osk_hide_methods[count++] = ARM32_OSK_HIDE_TRYHIDE;
    if (arm32_osk_is_win11() && (arm32_osk_signal == ARM32_OSK_SIG_FIP || arm32_osk_signal == ARM32_OSK_SIG_PANE)) {
        /* Windows 11: SC_CLOSE to the invisible IPTip_Main_Window is not what closes the keyboard of TextInputHost, and the shell reports the
         * result of a Toggle itself (Hiding callback) - Toggle first, SC_CLOSE only as the fallback */
        arm32_osk_hide_methods[count++] = ARM32_OSK_HIDE_TOGGLE;
        if (arm32_osk_find_window()) arm32_osk_hide_methods[count++] = ARM32_OSK_HIDE_CLOSE;
    } else {
        if (arm32_osk_find_window()) arm32_osk_hide_methods[count++] = ARM32_OSK_HIDE_CLOSE;
        if (arm32_osk_signal == ARM32_OSK_SIG_FIP || arm32_osk_signal == ARM32_OSK_SIG_PANE) arm32_osk_hide_methods[count++] = ARM32_OSK_HIDE_TOGGLE;
    }
    if (!count) {
        arm32_osk_ours = FALSE;
        log_line("ARM32 osk: hide requested but there is no way to hide the keyboard (no InputPane, no TabTip window)");
        return;
    }
    arm32_osk_hide_count = count;
    arm32_osk_hide_stage = 1;
    arm32_osk_hide_retries = 0;
    arm32_osk_hide_retry_pending = FALSE;
    wsprintfA(line, "ARM32 osk: hide requested (%s): %u method(s), first %s", why ? why : "?", count, arm32_osk_hide_names[arm32_osk_hide_methods[0]]);
    log_line(line);
    arm32_osk_hide_start_method(arm32_osk_hide_methods[0], GetTickCount());
    arm32_osk_last_detect = 0;
}

static void arm32_osk_hide(const char *why)
{
    BOOL show_in_flight = arm32_osk_want && arm32_osk_attempts > 0;

    arm32_osk_last_hide_tick = GetTickCount();
    arm32_osk_want = FALSE;
    arm32_osk_show_after_hide = FALSE;
    arm32_osk_dismissed_by_user = FALSE;
    arm32_osk_tap_since_dismiss = FALSE;
    if (arm32_osk_hide_stage) return;
    if (!arm32_osk_ours && show_in_flight) {
        /* the Toggle is on its way and the keyboard is not visible yet: hide it as soon as it shows up (otherwise it stays unowned) */
        arm32_osk_hide_when_visible = TRUE;
        arm32_osk_hide_when_visible_deadline = GetTickCount() + 4000u;
        arm32_osk_last_detect = 0;
        log_line("ARM32 osk: hide requested while the keyboard is still coming up - it will be hidden as soon as it is visible");
        return;
    }
    /* only close a keyboard this host opened (TryHide also runs once at startup, and the user may use the keyboard on his own) */
    if (!arm32_osk_ours) return;
    arm32_osk_hide_when_visible = FALSE;
    /* games hide and re-show in one go (1.2.8 after Enter: TryHide, then TryShow 62 ms later for the same chat box - Surface 2 log 2026-10-21):
     * wait ARM32_OSK_HIDE_DELAY_MS so such a pair leaves the keyboard up instead of closing and reopening it; arm32_osk_poll runs the hide */
    if (!arm32_osk_hide_pending) {
        arm32_osk_hide_pending = TRUE;
        arm32_osk_hide_pending_deadline = GetTickCount() + ARM32_OSK_HIDE_DELAY_MS;
        arm32_osk_hide_pending_why = why;
    }
}

/* rectangle of the visible touch keyboard in swap-chain buffer pixels (what CoreWindow.Bounds / OccludedRect use) */
static BOOL arm32_osk_rect_buffer(float rect[4])
{
    static float last[4];
    HWND tip;
    RECT window_rect;
    RECT client;
    POINT top_left;
    POINT bottom_right;
    UINT buffer_width = 0;
    UINT buffer_height = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    LONG client_width;
    LONG client_height;
    BOOL changed;

    /* IsWindowVisible and the window rectangle say nothing about a hidden keyboard: only report it when the state signal says it is up */
    if (arm32_osk_state != 1 || !game_window) return FALSE;
    if (arm32_osk_is_win11()) {
        /* Windows 11: IPTip_Main_Window is a 0x0 window (22631 log: rect=(0,0,0,0)), the keyboard of TextInputHost is reported by the shell:
         * IFrameworkInputPane::Location (accurate there, (0,540,2400,1080) while shown), else the rectangle of the last Showing callback */
        BOOL have = FALSE;
        memset(&window_rect, 0, sizeof(window_rect));
        if (arm32_osk_fip &&
            SUCCEEDED(((HRESULT (WINAPI *)(void *, RECT *))((void **)arm32_osk_fip[0])[6])(arm32_osk_fip, &window_rect)) &&
            window_rect.right > window_rect.left && window_rect.bottom > window_rect.top) {
            have = TRUE;
        }
        if (!have && arm32_osk_cb_kind == 1 && arm32_osk_cb_rect[2] > arm32_osk_cb_rect[0] && arm32_osk_cb_rect[3] > arm32_osk_cb_rect[1]) {
            window_rect.left = arm32_osk_cb_rect[0];
            window_rect.top = arm32_osk_cb_rect[1];
            window_rect.right = arm32_osk_cb_rect[2];
            window_rect.bottom = arm32_osk_cb_rect[3];
            have = TRUE;
        }
        if (!have || !GetClientRect(game_window, &client)) return FALSE;
    } else {
        tip = arm32_osk_find_window();
        if (!tip || !GetWindowRect(tip, &window_rect) || !GetClientRect(game_window, &client)) {
            return FALSE;
        }
    }
    top_left.x = window_rect.left;
    top_left.y = window_rect.top;
    bottom_right.x = window_rect.right;
    bottom_right.y = window_rect.bottom;
    ScreenToClient(game_window, &top_left);
    ScreenToClient(game_window, &bottom_right);
    client_width = client.right - client.left;
    client_height = client.bottom - client.top;
    if (top_left.x < 0) top_left.x = 0;
    if (top_left.y < 0) top_left.y = 0;
    if (bottom_right.x > client_width) bottom_right.x = client_width;
    if (bottom_right.y > client_height) bottom_right.y = client_height;
    if (bottom_right.x <= top_left.x || bottom_right.y <= top_left.y) return FALSE;
    arm32_gfx_get_backbuffer_size(&buffer_width, &buffer_height);
    if (buffer_width && buffer_height && client_width > 0 && client_height > 0) {
        scale_x = (float)buffer_width / (float)client_width;
        scale_y = (float)buffer_height / (float)client_height;
    }
    rect[0] = (float)top_left.x * scale_x;
    rect[1] = (float)top_left.y * scale_y;
    rect[2] = (float)(bottom_right.x - top_left.x) * scale_x;
    rect[3] = (float)(bottom_right.y - top_left.y) * scale_y;
    changed = rect[0] != last[0] || rect[1] != last[1] || rect[2] != last[2] || rect[3] != last[3];
    if (changed) {
        char line[160];
        last[0] = rect[0];
        last[1] = rect[1];
        last[2] = rect[2];
        last[3] = rect[3];
        wsprintfA(line, "ARM32 osk: OccludedRect reported to the game (buffer px) x=%d y=%d w=%d h=%d", (int)rect[0], (int)rect[1],
                  (int)rect[2], (int)rect[3]);
        log_line(line);
    }
    return TRUE;
}

/* the 1.2.8 OccludedRect: the keyboard rectangle, but held back while a finger is still down that touched before the keyboard appeared - the
 * game moves the chat box above the keyboard at once, the lift then lands outside the text box and the game closes the keyboard again (Surface 2
 * log 2026-10-21: TryShow on the press, keyboard up 94 ms later, TryHide 31 ms after that).  Capped at 3 s after the keyboard appeared. */
static BOOL arm32_osk_rect_for_game(float rect[4])
{
    static DWORD logged_for;

    if (arm32_osk_state == 1 && arm32_touch_any_active() && (GetTickCount() - arm32_osk_visible_since) < 3000u) {
        if (logged_for != arm32_osk_visible_since) {
            logged_for = arm32_osk_visible_since;
            log_line("ARM32 osk: keyboard rectangle held back from the game until the finger is lifted (the chat box must not move under the finger)");
        }
        return FALSE;
    }
    return arm32_osk_rect_buffer(rect);
}

/* ---- diagnostics: which input mappers exist and which mode the game is in ---- */
typedef struct Arm32MapperName {
    const char *name;
    DWORD vtable_rva;
} Arm32MapperName;

static const Arm32MapperName arm32_mapper_names[] = {
    { "Gaze", ARM_TOUCH_IMPL_MAPPER_VTABLE_GAZE },
    { "Keyboard", ARM_TOUCH_IMPL_MAPPER_VTABLE_KEYBOARD },
    { "Mouse", ARM_TOUCH_IMPL_MAPPER_VTABLE_MOUSE },
    { "GamePad", ARM_TOUCH_IMPL_MAPPER_VTABLE_GAMEPAD },
    { "Touch", ARM_TOUCH_IMPL_MAPPER_VTABLE_TOUCH },
    { "Motion", ARM_TOUCH_IMPL_MAPPER_VTABLE_MOTION },
    { "DeviceButton", ARM_TOUCH_IMPL_MAPPER_VTABLE_DEVICEBUTTON },
    { "Behavior", ARM_TOUCH_IMPL_MAPPER_VTABLE_BEHAVIOR }
};

/* InputHandler of the captured ClientInstance: *( *(client+0x40) + 4 ), or NULL while it is not ready / not plausible */
static BYTE *arm32_input_handler(void)
{
    static BOOL logged_bad_owner;
    BYTE *client = client_instance_128;
    BYTE **owner;
    BYTE *handler;
    char line[200];

    if (!game_image || !client) return NULL;
    if (!plausible_client_instance_128(client, game_image + ARM_CLIENT_STATE_CLIENTINSTANCE_VTABLE_PRIMARY)) return NULL;
    owner = *(BYTE ***)(client + ARM_TOUCH_IMPL_CLIENT_INPUT_OWNER);
    if (!owner || IsBadReadPtr(owner, 2 * sizeof(void *))) return NULL;
    if ((BYTE *)owner[0] != client) {
        if (!logged_bad_owner) {
            logged_bad_owner = TRUE;
            wsprintfA(line, "ARM32 input: ClientInstance+0x40 owner=%p but owner[0]=%p != client %p - input handler lookup skipped",
                      (void *)owner, (void *)owner[0], (void *)client);
            log_line(line);
        }
        return NULL;
    }
    handler = owner[1];
    if (!handler || IsBadReadPtr(handler, 0x140)) return NULL;
    return handler;
}

static void arm32_input_log_mappers(BYTE *handler)
{
    BYTE **begin = *(BYTE ***)(handler + ARM_TOUCH_IMPL_INPUT_HANDLER_MAPPERS_BEGIN);
    BYTE **end = *(BYTE ***)(handler + ARM_TOUCH_IMPL_INPUT_HANDLER_MAPPERS_END);
    BYTE **item;
    char line[500];
    char names[360];
    unsigned count = 0;
    BOOL touch_present = FALSE;

    names[0] = 0;
    if (!begin || end < begin || (SIZE_T)((BYTE *)end - (BYTE *)begin) > 64 * sizeof(void *) || IsBadReadPtr(begin, (SIZE_T)((BYTE *)end - (BYTE *)begin))) {
        wsprintfA(line, "ARM32 input: InputHandler %p mapper vector is not plausible (begin=%p end=%p)", (void *)handler, (void *)begin,
                  (void *)end);
        log_line(line);
        return;
    }
    for (item = begin; item < end; ++item) {
        const char *name = "?";
        unsigned index;
        void *vtable = (*item && !IsBadReadPtr(*item, sizeof(void *))) ? *(void **)*item : NULL;

        for (index = 0; index < ARRAYSIZE(arm32_mapper_names); ++index) {
            if (vtable == (void *)(game_image + arm32_mapper_names[index].vtable_rva)) {
                name = arm32_mapper_names[index].name;
                if (arm32_mapper_names[index].vtable_rva == ARM_TOUCH_IMPL_MAPPER_VTABLE_TOUCH) touch_present = TRUE;
            }
        }
        if (lstrlenA(names) + 24 < (int)sizeof(names)) {
            if (count) lstrcatA(names, ",");
            lstrcatA(names, name);
        }
        ++count;
    }
    wsprintfA(line, "ARM32 input: InputHandler %p has %u mapper(s): %s", (void *)handler, count, names);
    log_line(line);
    if (!touch_present) {
        log_line("ARM32 input: WARNING TouchMapper is MISSING from the input mapper list - the game created no touch controls "
                 "(isHolographic?), the on-screen touch HUD can never appear. Send this log.");
    }
}

static const char *arm32_input_mode_name(int mode)
{
    switch (mode) {
    case 0: return "Undefined";
    case 1: return "Mouse";
    case 2: return "Touch";
    case 3: return "GamePad";
    case 4: return "MotionController";
    default: return "invalid";
    }
}

/* current InputMode of the primary client: InputHandler::getInputMode(handler, [handler+0x13c]) -> int* (NULL = unknown) */
static int arm32_input_read_mode(BYTE *handler)
{
    typedef int *(*GetInputModeFn)(void *, int);
    static int bytes_state;   /* 0 unchecked, 1 ok, -1 mismatch */
    int client_id;
    int *mode;

    if (bytes_state == 0) {
        bytes_state = arm32_check_bytes(game_image + ARM_TOUCH_GAME_GET_INPUT_MODE, ARM_TOUCH_GAME_GET_INPUT_MODE_EXPECTED, 12,
                                        "InputHandler::getInputMode") ? 1 : -1;
    }
    if (bytes_state < 0) return -1;
    client_id = *(int *)(handler + ARM_TOUCH_IMPL_INPUT_HANDLER_CLIENT_ID);
    mode = ((GetInputModeFn)ARM_CODE(game_image, ARM_TOUCH_GAME_GET_INPUT_MODE))(handler, client_id);
    if (!mode || IsBadReadPtr(mode, sizeof(int))) return -1;
    return *mode;
}

static void arm32_touch_mapper_reset_now(const char *why)
{
    BYTE *handler = arm32_input_handler();
    BYTE **begin;
    BYTE **end;
    BYTE **item;
    char line[200];

    if (!handler) return;
    begin = *(BYTE ***)(handler + ARM_TOUCH_IMPL_INPUT_HANDLER_MAPPERS_BEGIN);
    end = *(BYTE ***)(handler + ARM_TOUCH_IMPL_INPUT_HANDLER_MAPPERS_END);
    if (!begin || end < begin || (SIZE_T)((BYTE *)end - (BYTE *)begin) > 64 * sizeof(void *) || IsBadReadPtr(begin, (SIZE_T)((BYTE *)end - (BYTE *)begin))) return;
    for (item = begin; item < end; ++item) {
        void **vtable = (*item && !IsBadReadPtr(*item, sizeof(void *))) ? *(void ***)*item : NULL;
        void *wanted = ARM_CODE(game_image, ARM_TOUCH_IMPL_TOUCH_MAPPER_RESET);

        if (vtable != (void **)(game_image + ARM_TOUCH_IMPL_MAPPER_VTABLE_TOUCH)) continue;
        if (IsBadReadPtr(vtable, 3 * sizeof(void *)) || vtable[2] != wanted) {
            log_line("ARM32 input: TouchMapper vtable[2] is not the expected reset function - reset skipped");
            return;
        }
        ((void (*)(void *))wanted)(*item);
        wsprintfA(line, "ARM32 input: TouchMapper reset called (%s)", why ? why : "?");
        log_line(line);
        return;
    }
}

/* once per few loop passes: log mapper list (once), the game's input mode and every change of it */
static void arm32_input_poll_mode(void)
{
    static int previous_mode = -2;
    static BYTE *listed_handler;
    BYTE *handler = arm32_input_handler();
    int mode;
    char line[260];

    if (!handler) return;
    if (handler != listed_handler) {
        listed_handler = handler;
        arm32_input_log_mappers(handler);
        previous_mode = -2;
    }
    mode = arm32_input_read_mode(handler);
    if (mode == previous_mode) return;
    wsprintfA(line, "ARM32 input: game input mode %s -> %s (host source=%s, policy=%s, active touch contacts=%d)",
              previous_mode == -2 ? "(start)" : arm32_input_mode_name(previous_mode), mode < 0 ? "unknown" : arm32_input_mode_name(mode),
              arm32_input_source == ARM32_SRC_TOUCH ? "TOUCH" : "MOUSEKB", arm32_input_policy_names[arm32_input_policy],
              (int)arm32_touch_any_active());
    log_line(line);
    if (mode == 2) {
        log_line("ARM32 input: the game is in Touch mode - the on-screen touch controls (joystick / jump / sneak) are drawn by TouchMapper::render "
                 "once a world is open");
    }
    previous_mode = mode;
}

/* ---- statistics, window attachment, initial mode, per-pass hook ---- */
static void arm32_input_log_stats(const char *when)
{
    char line[560];

    if (memcmp(&arm32_stats, &arm32_stats_logged, sizeof(arm32_stats)) == 0 && when[0] == 'p') return;   /* "periodic" with no news */
    arm32_stats_logged = arm32_stats;
    wsprintfA(line, "ARM32 touch stats (%s): down=%u up=%u moveMsgs=%u movesQueued=%u slotsFull=%u canceled=%u captureLost=%u queueFailed=%u "
                    "wmTouch=%u legacyFromTouch=%u legacyMouse=%u pointerMouse=%u pointerPen=%u pointerOther=%u sourceSwitches=%u "
                    "source=%s touchApi=%s",
              when, arm32_stats.down, arm32_stats.up, arm32_stats.move_messages, arm32_stats.moves_flushed, arm32_stats.slots_full,
              arm32_stats.canceled, arm32_stats.capture_lost, arm32_stats.queue_failed, arm32_stats.wm_touch_messages,
              arm32_stats.legacy_from_touch, arm32_stats.legacy_mouse_messages, arm32_stats.pointer_mouse, arm32_stats.pointer_pen,
              arm32_stats.pointer_other, arm32_stats.source_switches, arm32_input_source == ARM32_SRC_TOUCH ? "TOUCH" : "MOUSEKB",
              arm32_touch_api_effective ? arm32_touch_api_names[arm32_touch_api_effective] : "off");
    log_line(line);
}

/* getDefaultInputMode (AppPlatform_Winrt vtable slot 154) for touch-only machines: start in Touch mode */
static int arm32_default_input_mode_touch(void *self)
{
    (void)self;
    return 2;
}

static void arm32_install_default_touch_mode(BYTE *image)
{
    void **cell;
    void *wanted;

    arm32_input_probe();
    if (arm32_input_initial_source != ARM32_SRC_TOUCH) {
        log_line("ARM32 input: default input mode left at the game's choice (Mouse for DeviceFamily Windows.Desktop)");
        return;
    }
    if (!image) return;
    cell = (void **)(image + ARM_TOUCH_IMPL_APPPLATFORM_VF154_CELL);
    wanted = ARM_CODE(image, ARM_TOUCH_GAME_DEFAULT_MODE_CHAIN);
    if (!arm32_check_bytes(image + ARM_TOUCH_GAME_DEFAULT_MODE_CHAIN, ARM_TOUCH_GAME_DEFAULT_MODE_CHAIN_EXPECTED, 12,
                           "getDefaultInputMode")) {
        log_line("ARM32 input: getDefaultInputMode bytes differ - the game starts in its own default mode (touch will still switch it)");
        return;
    }
    if (arm32_patch_cell(cell, wanted, (void *)((ULONG_PTR)arm32_default_input_mode_touch | 1u), NULL,
                         "AppPlatform_Winrt::getDefaultInputMode (start in Touch mode)")) {
        log_line("ARM32 input: touch-only machine - the game starts in Touch input mode");
    }
}

/* once, right after the window exists */
static void arm32_input_attach_window(HWND window)
{
    static const UINT feedback_types[] = { 1, 7, 8, 9, 10, 11 };   /* contact visualisation, tap, double tap, press and hold, right tap, press and tap */
    BOOL off = FALSE;
    unsigned index;
    unsigned feedback_ok = 0;
    int api = 0;
    char line[300];

    arm32_input_probe();
    if (arm32_input_policy == ARM32_INPUT_MOUSE) {
        log_line("ARM32 input: --input=mouse: pointer / touch messages are left to DefWindowProc (touch arrives as emulated mouse, no touch HUD)");
        return;
    }
    {
        BOOL have_pointer = arm32_api.get_pointer_type != NULL;
        BOOL have_wm_touch = arm32_api.register_touch_window && arm32_api.get_touch_input_info && arm32_api.close_touch_input_handle;

        if (arm32_touch_api_requested == ARM32_TOUCHAPI_LEGACY) api = ARM32_TOUCHAPI_LEGACY;
        else if (arm32_touch_api_requested == ARM32_TOUCHAPI_WMTOUCH) api = have_wm_touch ? ARM32_TOUCHAPI_WMTOUCH : ARM32_TOUCHAPI_LEGACY;
        else if (have_pointer) api = ARM32_TOUCHAPI_POINTER;
        else api = have_wm_touch ? ARM32_TOUCHAPI_WMTOUCH : ARM32_TOUCHAPI_LEGACY;
        if (arm32_touch_api_requested != ARM32_TOUCHAPI_AUTO && api != arm32_touch_api_requested) {
            wsprintfA(line, "ARM32 input: requested touch api '%s' is not available on this system, using '%s'",
                      arm32_touch_api_names[arm32_touch_api_requested], arm32_touch_api_names[api]);
            log_line(line);
        }
    }
    if (api == ARM32_TOUCHAPI_POINTER && arm32_touch_api_requested == ARM32_TOUCHAPI_AUTO &&
        arm32_api.register_touch_window && arm32_api.get_touch_input_info && arm32_api.close_touch_input_handle) {
        /* WM_POINTER stays the primary path (it consumes the contact, so no WM_TOUCH follows); registering only makes WM_TOUCH the
         * delivery path on systems that never send WM_POINTER to this window */
        if (arm32_api.register_touch_window(window, 0)) {
            arm32_wm_touch_registered = TRUE;
            log_line("ARM32 input: window also registered for WM_TOUCH (multi-touch fallback used automatically when no WM_POINTER arrives)");
        } else {
            wsprintfA(line, "ARM32 input: RegisterTouchWindow failed, error=%lu - no WM_TOUCH fallback (single-touch legacy fallback only)",
                      (unsigned long)GetLastError());
            log_line(line);
        }
    }
    if (api == ARM32_TOUCHAPI_WMTOUCH) {
        if (!arm32_api.register_touch_window(window, 0)) {
            wsprintfA(line, "ARM32 input: RegisterTouchWindow failed, error=%lu - falling back to legacy touch", (unsigned long)GetLastError());
            log_line(line);
            api = ARM32_TOUCHAPI_LEGACY;
        }
    }
    /* no contact dot / tap / press-and-hold feedback and no pen flicks: the game draws its own UI */
    if (arm32_api.set_window_feedback_setting) {
        for (index = 0; index < ARRAYSIZE(feedback_types); ++index) {
            if (arm32_api.set_window_feedback_setting(window, feedback_types[index], 0, sizeof(off), &off)) ++feedback_ok;
        }
    }
    SetPropW(window, L"MicrosoftTabletPenServiceProperty", (HANDLE)(ULONG_PTR)(0x1u | 0x8u | 0x10u | 0x10000u | 0x100000u));
    arm32_touch_api_effective = api;
    wsprintfA(line, "ARM32 input: touch handling active: api=%s (requested %s) window=%p feedbackDisabled=%u/%u initialSource=%s "
                    "(WM_POINTER touch contacts are consumed without DefWindowProc, mouse/pen pointers are not)",
              arm32_touch_api_names[api], arm32_touch_api_names[arm32_touch_api_requested], (void *)window, feedback_ok,
              (unsigned)ARRAYSIZE(feedback_types), arm32_input_source == ARM32_SRC_TOUCH ? "TOUCH" : "MOUSEKB");
    log_line(line);
}

/* once per main-loop pass, before the dispatcher drain and the game frame */
static void arm32_input_frame(void)
{
    ++arm32_input_pass;
    arm32_touch_flush_moves();
    arm32_osk_poll();
    if (arm32_mapper_reset_pass && arm32_input_pass >= arm32_mapper_reset_pass) {
        arm32_mapper_reset_pass = 0;
        if (arm32_input_source == ARM32_SRC_MOUSEKB && !arm32_touch_any_active()) {
            arm32_touch_mapper_reset_now("input source switched to mouse/keyboard");
        }
    }
    if ((arm32_input_pass & 3u) == 0) arm32_input_poll_mode();
    if ((arm32_input_pass % 3600u) == 0) arm32_input_log_stats("periodic");
    if (!arm32_input_game_ready_logged && arm32_input_pass == 600) {
        arm32_input_game_ready_logged = TRUE;
        if (!arm32_input_handler()) {
            log_line("ARM32 input: after 600 loop passes the ClientInstance / InputHandler is still not available - the input mode cannot be "
                     "logged (the vf3 client hook may not have fired)");
        }
    }
}
#endif /* WIN32CRAFT_ARM32 touch input */

static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    switch (message) {
#ifdef WIN32CRAFT_ARM32
    case ARM32_WM_POINTERUPDATE:
    case ARM32_WM_POINTERDOWN:
    case ARM32_WM_POINTERUP:
    case ARM32_WM_POINTERCAPTURECHANGED:
        /* touch contacts become MultitouchItems and are NOT passed to DefWindowProc (so Windows emits no emulated mouse messages);
         * mouse / pen pointers fall through to DefWindowProc and keep using the WM_MOUSE* path below */
        if (arm32_touch_pointer_message(window, message, wparam, lparam)) return 0;
        return DefWindowProcW(window, message, wparam, lparam);
    case ARM32_WM_TOUCH:
        if (arm32_touch_wm_touch(window, wparam, lparam)) return 0;
        return DefWindowProcW(window, message, wparam, lparam);
    case ARM32_WM_CANCELMODE:
        arm32_touch_release_all("WM_CANCELMODE");
        return DefWindowProcW(window, message, wparam, lparam);
#endif
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        /* Preserve the normal Windows Alt+F4 close command. */
        if (message == WM_SYSKEYDOWN && wparam == VK_F4 &&
            ((DWORD)lparam & (1u << 29))) {
            return DefWindowProcW(window, message, wparam, lparam);
        }
#ifdef WIN32CRAFT_ARM32
        if (!((DWORD)lparam & (1u << 30))) arm32_input_note_key(wparam);   /* first press only: a keyboard is in use */
#endif
        if (wparam == 0x1b && !((DWORD)lparam & (1u << 30))) {
            pending_main_menu_exit_confirmation =
                host_is_128 ? !client_world_active_128()
                            : (host_is_115 && !host_relative_requested);
        }
        if (wparam == 0x11 || wparam == 0xa2 || wparam == 0xa3)
            text_ctrl_down = TRUE;
        if (!host_is_116 && text_input_active && text_ctrl_down &&
            wparam == 'V' && !((DWORD)lparam & (1u << 30))) {
            BOOL pasted = paste_clipboard_as_game_text();
            legacy_clipboard_handled_key = wparam;
            log_line(pasted ? "Win32 clipboard paste queued as game text"
                            : "Win32 clipboard paste unavailable");
            return 0;
        }
        if (!host_is_115 && !host_is_128 &&
            text_input_active && wparam == VK_RETURN) {
            if (!((DWORD)lparam & (1u << 30))) {
                request_chat_submit_01510();
                suppress_return_keyup_01510 = TRUE;
            }
            return 0;
        }
        if (wparam == VK_T && !text_input_active &&
            !((DWORD)lparam & (1u << 30))) {
            suppress_next_t_character = TRUE;
        }
        handle_win32_key(wparam, TRUE);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wparam == legacy_clipboard_handled_key) {
            legacy_clipboard_handled_key = 0;
            return 0;
        }
        if (!host_is_115 && !host_is_128 && wparam == VK_RETURN &&
            suppress_return_keyup_01510) {
            suppress_return_keyup_01510 = FALSE;
            return 0;
        }
        handle_win32_key(wparam, FALSE);
        if (wparam == 0x11 || wparam == 0xa2 || wparam == 0xa3)
            text_ctrl_down = FALSE;
        return 0;
    case WM_CHAR:
        handle_win32_character(wparam);
        return 0;
    case WM_UNICHAR:
        if (wparam == UNICODE_NOCHAR) return TRUE;
        if (text_input_active) queue_character_event((DWORD)wparam);
        return 0;
    case WM_MOUSEWHEEL: {
        int delta;
#ifdef WIN32CRAFT_ARM32
        if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;
#endif
        delta = (SHORT)HIWORD(wparam);
        if (delta > 127) delta = 127;
        if (delta < -128) delta = -128;
        queue_mouse_event(0, 0, 0, 4, (BYTE)(CHAR)delta);
        return 0;
    }
    case WM_KILLFOCUS:
        release_all_win32_keys();
#ifdef WIN32CRAFT_ARM32
        arm32_touch_release_all("WM_KILLFOCUS");
#endif
        update_mouse_capture(window);
        return 0;
    case WM_SETFOCUS:
        update_mouse_capture(window);
        return 0;
    case WM_ENTERSIZEMOVE:
        window_in_size_move = TRUE;
        pending_resize_width = 0;
        pending_resize_height = 0;
        log_line("interactive window resize entered; DXGI resize deferred");
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_SIZE:
#ifdef WIN32CRAFT_ARM32
        if (wparam == SIZE_MINIMIZED) arm32_touch_release_all("window minimized");
#endif
        if (wparam != SIZE_MINIMIZED) {
            UINT width = (UINT)LOWORD(lparam);
            UINT height = (UINT)HIWORD(lparam);
            mouse_clip_dirty = TRUE;
            if (window_in_size_move && host_is_windows7) {
                pending_resize_width = width;
                pending_resize_height = height;
            } else {
                resize_game_window(window, width, height);
            }
            update_mouse_capture(window);
        }
        return 0;
    case WM_EXITSIZEMOVE:
        window_in_size_move = FALSE;
        if (host_is_windows7) {
            RECT client;
            UINT width = pending_resize_width;
            UINT height = pending_resize_height;
            if ((!width || !height) && GetClientRect(window, &client)) {
                width = (UINT)(client.right - client.left);
                height = (UINT)(client.bottom - client.top);
            }
            pending_resize_width = 0;
            pending_resize_height = 0;
            if (width && height) resize_game_window(window, width, height);
            log_line("interactive window resize exited; deferred DXGI resize applied");
        }
        mouse_clip_dirty = TRUE;
        update_mouse_capture(window);
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_MOVE:
    case WM_WINDOWPOSCHANGED:
        mouse_clip_dirty = TRUE;
        update_mouse_capture(window);
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_ACTIVATEAPP:
        if (!wparam) release_all_win32_keys();
#ifdef WIN32CRAFT_ARM32
        if (!wparam) arm32_touch_release_all("WM_ACTIVATEAPP(FALSE)");
#endif
        update_mouse_capture(window);
        return 0;
    case WM_SETCURSOR:
        if (mouse_capture_active && LOWORD(lparam) == HTCLIENT) {
            SetCursor(NULL);
            return TRUE;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_MOUSEMOVE:
#ifdef WIN32CRAFT_ARM32
        if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;
#endif
        queue_pointer_position(lparam);
        return 0;
    case WM_LBUTTONDOWN:
#ifdef WIN32CRAFT_ARM32
        if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;
#endif
        if (initial_menu_cursor_guard) {
            /* A real click proves that the Win32 menu is active.  Subsequent
             * relative requests (for example after Play enters a world) are
             * now authoritative. */
            initial_menu_cursor_guard = FALSE;
            host_relative_requested = FALSE;
            mouse_clip_dirty = TRUE;
            log_line("initial main-menu cursor guard released by click");
        }
        SetFocus(window);
        SetCapture(window);
        queue_pointer_position(lparam);
        queue_mouse_event((short)LOWORD(lparam), (short)HIWORD(lparam),
                          0, 1, 1);
        return 0;
    case WM_LBUTTONUP:
#ifdef WIN32CRAFT_ARM32
        if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;
#endif
        if (pending_main_menu_exit_confirmation) {
            RECT client;
            int x = (short)LOWORD(lparam);
            int y = (short)HIWORD(lparam);
            BOOL confirm = FALSE;
            if (GetClientRect(window, &client)) {
                int width = client.right - client.left;
                int height = client.bottom - client.top;
                if (host_is_128) {
                    confirm = x >= width * 35 / 100 &&
                        x <= width * 65 / 100 &&
                        y >= height * 54 / 100 &&
                        y <= height * 65 / 100;
                } else if (host_is_115) {
                    confirm = x >= width * 35 / 100 &&
                        x <= width * 50 / 100 &&
                        y >= height * 62 / 100 &&
                        y <= height * 72 / 100;
                }
            }
            pending_main_menu_exit_confirmation = FALSE;
            if (confirm) {
                log_line(host_is_128
                    ? "1.2.8 main-menu exit confirmed; closing HWND"
                    : "1.1.5 main-menu exit confirmed; closing HWND");
                PostMessageW(window, WM_CLOSE, 0, 0);
                return 0;
            }
        }
        queue_pointer_position(lparam);
        queue_mouse_event((short)LOWORD(lparam), (short)HIWORD(lparam),
                          0, 1, 0);
        if (!mouse_capture_active && GetCapture() == window) ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
#ifdef WIN32CRAFT_ARM32
        if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;
#endif
        SetFocus(window);
        SetCapture(window);
        queue_pointer_position(lparam);
        queue_mouse_event((short)LOWORD(lparam), (short)HIWORD(lparam),
                          0, 2, 1);
        return 0;
    case WM_RBUTTONUP:
#ifdef WIN32CRAFT_ARM32
        if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;
#endif
        queue_pointer_position(lparam);
        queue_mouse_event((short)LOWORD(lparam), (short)HIWORD(lparam),
                          0, 2, 0);
        if (!mouse_capture_active && GetCapture() == window) ReleaseCapture();
        return 0;
    case WM_MBUTTONDOWN:
#ifdef WIN32CRAFT_ARM32
        if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;
#endif
        SetFocus(window);
        SetCapture(window);
        queue_pointer_position(lparam);
        queue_mouse_event((short)LOWORD(lparam), (short)HIWORD(lparam),
                          0, 3, 1);
        return 0;
    case WM_MBUTTONUP:
#ifdef WIN32CRAFT_ARM32
        if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;
#endif
        queue_pointer_position(lparam);
        queue_mouse_event((short)LOWORD(lparam), (short)HIWORD(lparam),
                          0, 3, 0);
        if (!mouse_capture_active && GetCapture() == window) ReleaseCapture();
        return 0;
#ifdef WIN32CRAFT_ARM32
    case WM_GETMINMAXINFO:
        arm32_clamp_min_max_info(window, (Arm32MinMaxInfo *)lparam);
        return 0;
#endif
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

#ifndef WIN32CRAFT_ARM32
#include "win32_host_116.inl"
#endif

#ifdef WIN32CRAFT_ARM32
/* ======================================================================================================
 * ARM32 Win32Bootstrap (unit bootstrap_main) - the 1.2.8 ARM32 path only.  Facts: winmain.*, pe_imports.*,
 * appmain.app_main_frame, fl91.*; contract: arm32/PORT_RULES.md.
 *
 * Entry: the patched exe's __scrt_common_main_seh `bl sub_b4a424` (RVA 0xB49ACA) is redirected through a PIC trampoline
 * (RVA 0xC36EA0) to this export.  r0-r3 are not read; the return value becomes exit(r4).
 *
 * Return codes (0 or the WM_QUIT wParam = normal exit; every other value is a startup failure whose reason is the last
 * "ARM32 bootstrap FAILED" line of win32craft.log).  A startup failure shows a message box and ends the process with
 * TerminateProcess(code) (no CRT exit() over a half-initialised game); the normal exit returns into the CRT, guarded by a
 * 30 s terminate timer:
 *    3  SizeOfImage != IMAGE_SIZE_128_ARM_PATCHED   4  not a PE32 / ARMNT (0x01C4) image   10 install_arm32_iat_hooks
 *   11  install_arm32_platform_fixes   (12 unused)   13 RegisterClassExW   14 CreateWindowExW
 *   15  initialize_game_d3d   16 initialize_game_app_128   17 activate_game_app_128
 * (install_arm32_input_hooks and install_client_state_hook_128 are only needed for mouse-mode tracking / mouse capture /
 * text focus: their failure is logged as a warning, the run continues.)
 * ====================================================================================================== */
#define ARM32_BOOT_BAD_IMAGE_SIZE          3
#define ARM32_BOOT_BAD_PE_HEADER           4
#define ARM32_BOOT_IAT_HOOKS_FAILED       10
#define ARM32_BOOT_PLATFORM_FIXES_FAILED  11
#define ARM32_BOOT_INPUT_HOOKS_FAILED     12
#define ARM32_BOOT_REGISTER_CLASS_FAILED  13
#define ARM32_BOOT_CREATE_WINDOW_FAILED   14
#define ARM32_BOOT_D3D_FAILED             15
#define ARM32_BOOT_APPMAIN_FAILED         16
#define ARM32_BOOT_ACTIVATE_FAILED        17
#define ARM32_PE_MACHINE_ARMNT            0x01c4u
#define ARM32_PE32_MAGIC                  0x010bu
#define ARM32_BOOT_OPEN_ALWAYS            4U

/* Win32 calls the shared compat header does not declare (identical to the SDK prototypes) */
extern DWORD WINAPI GetTickCount(void);
extern DWORD WINAPI GetModuleFileNameW(HMODULE, LPWSTR, DWORD);
extern BOOL WINAPI SetCurrentDirectoryW(LPCWSTR);
extern BOOL WINAPI SetEnvironmentVariableA(LPCSTR, LPCSTR);
extern BOOL WINAPI TerminateProcess(HANDLE, UINT);
extern DWORD WINAPI GetCurrentProcessId(void);
extern int WINAPI MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);

/* contract entry points defined by the other ARM units (PORT_RULES "Cross-unit contract"); render_game_target_128 returns
 * int (nonzero = the game asked to quit) once the appmain unit is applied */
static BOOL install_arm32_iat_hooks(BYTE *image);
static BOOL install_arm32_platform_fixes(BYTE *image);
static BOOL install_arm32_input_hooks(BYTE *image);

static const char *volatile arm32_boot_step_name = "not started";
static volatile LONG arm32_boot_progress;       /* bumped on every step change and every main-loop pass */
static volatile LONG arm32_boot_loop_passes;
static volatile LONG arm32_boot_frames_started;
static volatile LONG arm32_boot_frames_done;
static volatile LONG arm32_boot_watchdog_stop;
static DWORD arm32_boot_start_tick;

static void arm32_boot_narrow(char *out, SIZE_T out_size, const wchar_t *text)
{
    SIZE_T index = 0;

    if (!out || !out_size) return;
    while (text && text[index] && index + 1 < out_size) {
        out[index] = text[index] < 0x80 ? (char)text[index] : '?';
        ++index;
    }
    out[index] = 0;
}

/* loaded-module inventory: ties a device log to a package variant (patched real FMOD vs silent stub, VC runtime versions) */
static void arm32_log_module_inventory(void)
{
    static const wchar_t *const names[] = {
        L"win32craft.dll", L"fmod_ARM.dll", L"vccorlib140.dll", L"msvcp140.dll", L"concrt140.dll", L"vcruntime140.dll",
        L"ucrtbase.dll", L"d3d11.dll", L"dxgi.dll", L"d3dcompiler_47.dll", L"combase.dll", L"wintypes.dll"
    };
    UINT index;
    char line[2 * MAX_PATH + 160];

    for (index = 0; index < ARRAYSIZE(names); ++index) {
        HMODULE module = GetModuleHandleW(names[index]);
        wchar_t path[MAX_PATH];
        char narrow_name[48];
        char narrow_path[MAX_PATH];
        WIN32_FILE_ATTRIBUTE_DATA info;
        DWORD stamp = 0;
        DWORD image_size = 0;
        DWORD file_size = 0;
        BOOL have_info = FALSE;
        const BYTE *pe = (const BYTE *)module;

        arm32_boot_narrow(narrow_name, sizeof(narrow_name), names[index]);
        if (!module) {
            wsprintfA(line, "module %s: not loaded", narrow_name);
            log_line(line);
            continue;
        }
        path[0] = 0;
        if (!GetModuleFileNameW(module, path, ARRAYSIZE(path))) path[0] = 0;
        arm32_boot_narrow(narrow_path, sizeof(narrow_path), path);
        if (path[0] && GetFileAttributesExW(path, GetFileExInfoStandard, &info)) {
            file_size = info.nFileSizeLow;
            have_info = TRUE;
        }
        if (!IsBadReadPtr(pe, 0x40)) {
            DWORD pe_offset = *(const DWORD *)(pe + 0x3c);
            if (pe_offset && pe_offset < 0x1000 && !IsBadReadPtr(pe + pe_offset, 0x60) &&
                *(const DWORD *)(pe + pe_offset) == 0x00004550u) {
                stamp = *(const DWORD *)(pe + pe_offset + 8);
                image_size = *(const DWORD *)(pe + pe_offset + 24 + 0x38);
            }
        }
        wsprintfA(line, "module %s: base=%p file_size=%s%lu timestamp=%08lx image_size=%08lx path=%s", narrow_name, (void *)module,
                  have_info ? "" : "?", (unsigned long)file_size, (unsigned long)stamp, (unsigned long)image_size,
                  narrow_path[0] ? narrow_path : "(unknown)");
        log_line(line);
    }
    log_line("module note: the silent FMOD stub fmod_ARM.dll is 6144 bytes, the patched real one 1118720 bytes");
}

static void arm32_boot_step(const char *name)
{
    char line[160];

    arm32_boot_step_name = name;
    InterlockedIncrement(&arm32_boot_progress);
    wsprintfA(line, "ARM32 bootstrap step: %s", name);
    log_line(line);
}

/* logs the failure, stops the watchdog and returns `code` for `return arm32_boot_fail(...)` */
static int arm32_boot_fail(int code, const char *what)
{
    char line[400];
    char log_path[MAX_PATH];
    char message[900];

    wsprintfA(line, "ARM32 bootstrap FAILED at step '%s': %s -> process exit code %d",
              arm32_boot_step_name, what, code);
    log_line(line);
    arm32_boot_watchdog_stop = 1;
    /* A failed start must not return into the CRT: exit() would run the static destructors of a half-initialised game
     * (a hang or a crash there leaves a dead window behind and hides the real reason).  Tell the user, then end the
     * process with the failure code; every log line is already written (WriteFile, no buffering). */
    arm32_boot_narrow(log_path, sizeof(log_path), Win32CraftLogPathW);
    wsprintfA(message, "Win32Craft could not start Minecraft 1.2.8 (error %d at step '%s').\n\n%s\n\nLog file: %s",
              code, arm32_boot_step_name, what, log_path[0] ? log_path : "(none)");
    MessageBoxA(NULL, message, "Win32Craft", 0x00000010u /* MB_ICONERROR */ | 0x00010000u /* MB_SETFOREGROUND */);
    log_line("ARM32 bootstrap: terminating the process after the failure");
    TerminateProcess(GetCurrentProcess(), (UINT)code);
    return code;
}

/* log_115_main_thread_context() logs (log_line -> stdio lock) while the main thread is suspended.  A main thread that was
 * suspended inside log_line, fprintf or the heap (the D3DCompile / shader hooks log from the main thread during a long first
 * frame, which is exactly when this watchdog fires) would then deadlock the whole process: the watchdog blocks on the lock and
 * never reaches ResumeThread.  So: capture the registers with the thread suspended for as short as possible, no logging and
 * no CRT call in between, resume it, and only then log. */
static void arm32_boot_snapshot_main_thread(const char *reason)
{
    typedef HANDLE (WINAPI *OpenThreadFn)(DWORD, BOOL, DWORD);
    typedef DWORD (WINAPI *SuspendThreadFn)(HANDLE);
    typedef BOOL (WINAPI *GetThreadContextFn)(HANDLE, CONTEXT *);
    typedef DWORD (WINAPI *ResumeThreadFn)(HANDLE);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    OpenThreadFn open_thread = kernel32 ? (OpenThreadFn)GetProcAddress(kernel32, "OpenThread") : NULL;
    SuspendThreadFn suspend_thread = kernel32 ? (SuspendThreadFn)GetProcAddress(kernel32, "SuspendThread") : NULL;
    GetThreadContextFn get_thread_context = kernel32
        ? (GetThreadContextFn)GetProcAddress(kernel32, "GetThreadContext") : NULL;
    ResumeThreadFn resume_thread = kernel32 ? (ResumeThreadFn)GetProcAddress(kernel32, "ResumeThread") : NULL;
    HANDLE thread;
    CONTEXT context;
    BOOL suspended;
    BOOL have_context = FALSE;
    char line[192];

    if (!game115_main_thread_id) {
        log_line("ARM32 thread context: main thread id is unavailable");
        return;
    }
    if (!open_thread || !suspend_thread || !get_thread_context || !resume_thread) {
        log_line("ARM32 thread context: thread-context APIs unavailable");
        return;
    }
    thread = open_thread(0x0002u | 0x0008u | 0x0040u, FALSE, game115_main_thread_id);
    if (!thread) {
        log_line("ARM32 thread context: OpenThread failed");
        return;
    }
    ZeroMemory(&context, sizeof(context));
    context.ContextFlags = 0x00200003u;   /* CONTEXT_ARM | CONTEXT_CONTROL | CONTEXT_INTEGER */
    suspended = suspend_thread(thread) != 0xffffffffu;
    if (suspended) {
        have_context = get_thread_context(thread, &context);
        resume_thread(thread);
    }
    CloseHandle(thread);
    if (!suspended) {
        log_line("ARM32 thread context: SuspendThread failed");
    } else if (!have_context) {
        log_line("ARM32 thread context: GetThreadContext failed");
    } else {
        wsprintfA(line, "ARM32 main thread context (%s)", reason ? reason : "snapshot");
        arm32_log_context(line, &context);
        /* the stalled thread's stack is quiescent, reading it after the resume is fine (IsBadReadPtr guarded) */
        arm32_log_stack_scan("main thread", context.Sp, 64, 64);
    }
}

/* A hang is distinguishable from a crash by the log alone: while the main thread makes no progress (a game call that never
 * returns) this thread reports it every 5/10/20/40/80... seconds and snapshots the main thread (Pc/Lr/Sp, game RVAs) three
 * times; while it progresses it writes a heartbeat every 30 s.  It only reads volatile counters. */
static DWORD WINAPI arm32_boot_watchdog_thread(void *parameter)
{
    LONG last_progress = -1;
    LONG last_frames = 0;
    DWORD last_change = GetTickCount();
    DWORD last_heartbeat = last_change;
    DWORD next_report = 5000;
    unsigned dumps = 0;
    BOOL stall_reported = FALSE;
    char line[360];

    (void)parameter;
    while (!arm32_boot_watchdog_stop) {
        LONG progress;
        DWORD now;
        DWORD stalled;

        Sleep(500);
        if (arm32_boot_watchdog_stop) break;
        now = GetTickCount();
        progress = arm32_boot_progress;
        if (progress != last_progress) {
            if (stall_reported) {
                wsprintfA(line, "ARM32 watchdog: main thread is progressing again after %lu s (step '%s')",
                          (unsigned long)((now - last_change) / 1000u), arm32_boot_step_name);
                log_line(line);
            }
            last_progress = progress;
            last_change = now;
            next_report = 5000;
            stall_reported = FALSE;
        } else if (now - last_change >= next_report) {
            stalled = now - last_change;
            stall_reported = TRUE;
            wsprintfA(line, "ARM32 watchdog: NO PROGRESS for %lu s in step '%s' (loop passes=%ld frames started=%ld done=%ld)%s",
                      (unsigned long)(stalled / 1000u), arm32_boot_step_name, (long)arm32_boot_loop_passes,
                      (long)arm32_boot_frames_started, (long)arm32_boot_frames_done,
                      arm32_boot_frames_started != arm32_boot_frames_done
                          ? " - a game frame call has not returned" : "");
            log_line(line);
            if ((dumps == 0 && stalled >= 10000u) || (dumps == 1 && stalled >= 80000u) ||
                (dumps == 2 && stalled >= 320000u)) {
                ++dumps;
                arm32_boot_snapshot_main_thread("bootstrap watchdog: stalled main thread");
            }
            next_report = next_report < 80000u ? next_report * 2u : next_report + 80000u;
        }
        if (now - last_heartbeat >= 30000u) {
            LONG frames = arm32_boot_frames_done;

            wsprintfA(line, "ARM32 heartbeat: up %lu s, step '%s', loop passes=%ld, frames done=%ld (+%ld in the last 30 s)",
                      (unsigned long)((now - arm32_boot_start_tick) / 1000u), arm32_boot_step_name,
                      (long)arm32_boot_loop_passes, (long)frames, (long)(frames - last_frames));
            log_line(line);
            last_frames = frames;
            last_heartbeat = now;
        }
    }
    return 0;
}

static void arm32_boot_start_watchdog(void)
{
    typedef HANDLE (WINAPI *CreateThreadFn)(
        void *, SIZE_T, DWORD (WINAPI *)(void *), void *, DWORD, DWORD *);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    CreateThreadFn create_thread = kernel32
        ? (CreateThreadFn)GetProcAddress(kernel32, "CreateThread") : NULL;
    HANDLE thread;

    if (!create_thread) {
        log_line("ARM32 watchdog: CreateThread is unavailable, no stall reports");
        return;
    }
    thread = create_thread(NULL, 0, arm32_boot_watchdog_thread, NULL, 0, NULL);
    if (!thread) {
        log_line("ARM32 watchdog: CreateThread failed, no stall reports");
        return;
    }
    CloseHandle(thread);
    log_line("ARM32 watchdog started (stall reports from 5 s, heartbeat every 30 s)");
}

static BOOL arm32_boot_log_file_writable(DWORD *error_out)
{
    HANDLE file = CreateFileW(L"win32craft.log", GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                              ARM32_BOOT_OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE) {
        *error_out = GetLastError();
        return FALSE;
    }
    CloseHandle(file);
    *error_out = 0;
    return TRUE;
}

/* The only feedback channel is win32craft.log, and log_line() writes it only when WIN32CRAFT_DEBUG exists in the
 * environment (checked once, on the first log_line call).  So, before anything logs: enable logging by default, make
 * package_path the exe directory (data/ and resources.pri live next to the exe; the x86 host used the working directory),
 * make sure the log can actually be created in the working directory and fall back to %TEMP% when it cannot. */
static void arm32_boot_setup_paths_and_log(void)
{
    wchar_t original_cwd[MAX_PATH];
    wchar_t exe_dir[MAX_PATH];
    wchar_t temp_dir[MAX_PATH];
    wchar_t current[MAX_PATH];
    char narrow[MAX_PATH];
    char line[2 * MAX_PATH + 160];
    BOOL debug_forced = FALSE;
    BOOL exe_dir_ok = FALSE;
    BOOL moved_to_exe_dir = FALSE;
    BOOL move_to_exe_dir_failed = FALSE;
    BOOL moved_to_temp = FALSE;
    BOOL log_ok;
    DWORD probe_error = 0;
    DWORD length;
    DWORD temp_length;

    if (!GetEnvironmentVariableA("WIN32CRAFT_DEBUG", NULL, 0)) {
        debug_forced = SetEnvironmentVariableA("WIN32CRAFT_DEBUG", "1") != 0;
    }
    original_cwd[0] = 0;
    if (!GetCurrentDirectoryW(MAX_PATH, original_cwd)) original_cwd[0] = 0;
    length = GetModuleFileNameW(NULL, exe_dir, MAX_PATH);
    if (length && length < MAX_PATH) {
        while (length && exe_dir[length - 1] != L'\\' && exe_dir[length - 1] != L'/') --length;
        if (length == 3 && exe_dir[1] == L':') {
            exe_dir[3] = 0;                 /* "C:\" keeps its slash: "C:" would mean the current directory of drive C */
            exe_dir_ok = TRUE;
        } else if (length > 1) {
            exe_dir[length - 1] = 0;
            exe_dir_ok = TRUE;
        }
    }
    if (exe_dir_ok) {
        lstrcpyW(package_path, exe_dir);
        if (!host_argument_equals(original_cwd, exe_dir)) {
            if (SetCurrentDirectoryW(exe_dir)) moved_to_exe_dir = TRUE;
            else move_to_exe_dir_failed = TRUE;
        }
    } else if (original_cwd[0]) {
        lstrcpyW(package_path, original_cwd);
    } else {
        lstrcpyW(package_path, L".");
    }
    log_ok = arm32_boot_log_file_writable(&probe_error);
    if (!log_ok) {
        temp_length = GetTempPathW(MAX_PATH, temp_dir);
        if (temp_length && temp_length < MAX_PATH && SetCurrentDirectoryW(temp_dir)) {
            moved_to_temp = TRUE;
            log_ok = arm32_boot_log_file_writable(&probe_error);
        }
    }

    /* first log_line() call of the process happens below */
    wsprintfA(line, "Win32Craft %s ARM32 Win32Bootstrap entered (host build %s %s)", host_is_0104 ? x104_profile_title() : "1.2.8",
              __DATE__, __TIME__);
    log_line(line);
    arm32_boot_narrow(narrow, sizeof(narrow), Win32CraftLogPathW);
    wsprintfA(line, "THIS LOG FILE: %s", Win32CraftLogPathW[0] ? narrow : "(not opened: win32craft.log cannot be created)");
    log_line(line);
    if (debug_forced) log_line("WIN32CRAFT_DEBUG was not set: logging enabled by default on ARM32");
    arm32_boot_narrow(narrow, sizeof(narrow), original_cwd);
    wsprintfA(line, "working directory at start: %s", narrow);
    log_line(line);
    if (exe_dir_ok) {
        arm32_boot_narrow(narrow, sizeof(narrow), exe_dir);
        wsprintfA(line, "exe directory (package_path): %s%s", narrow,
                  moved_to_exe_dir ? " - working directory changed to it"
                                   : (move_to_exe_dir_failed ? " - SetCurrentDirectoryW failed, working directory kept"
                                                             : ""));
    } else {
        arm32_boot_narrow(narrow, sizeof(narrow), package_path);
        wsprintfA(line, "exe directory unavailable, package_path = working directory: %s", narrow);
    }
    log_line(line);
    if (moved_to_temp) log_line("win32craft.log is not writable in the exe directory: working directory moved to %TEMP%");
    if (GetCurrentDirectoryW(MAX_PATH, current)) {
        arm32_boot_narrow(narrow, sizeof(narrow), current);
        wsprintfA(line, "log file: %s\\win32craft.log (%s)", narrow,
                  log_ok ? "writable" : "NOT WRITABLE - this log will be lost");
        log_line(line);
    }
    if (!log_ok) {
        wsprintfA(line, "win32craft.log cannot be created, GetLastError=%lu", (unsigned long)probe_error);
        log_line(line);
        OutputDebugStringA(line);
    }
    if (moved_to_temp && log_ok) {
        /* log_line() fopen()s win32craft.log once and keeps the FILE: the %TEMP% working directory only mattered for that first
         * open (done above).  Do not leave the game running with %TEMP% as its working directory. */
        const wchar_t *restore = exe_dir_ok ? exe_dir : original_cwd;

        if (restore[0] && SetCurrentDirectoryW(restore)) {
            log_line("working directory restored after opening the log in %TEMP%");
        } else {
            log_line("warning: could not restore the working directory after opening the log in %TEMP%");
        }
    }
}

static void arm32_boot_log_os_version(void)
{
    typedef LONG (WINAPI *RtlGetVersionFn)(void *);
    struct {
        DWORD size;
        DWORD major;
        DWORD minor;
        DWORD build;
        DWORD platform;
        wchar_t service_pack[128];
    } info;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn function = ntdll ? (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    char line[160];

    ZeroMemory(&info, sizeof(info));
    info.size = sizeof(info);
    if (function && function(&info) >= 0) {
        wsprintfA(line, "Windows version (RtlGetVersion): %lu.%lu build %lu", (unsigned long)info.major,
                  (unsigned long)info.minor, (unsigned long)info.build);
    } else {
        wsprintfA(line, "Windows version: RtlGetVersion unavailable");
    }
    log_line(line);
}

/* facts winmain.host.bootstrap_signature / winmain.pe.*: PE machine 0x1C4 and SizeOfImage == IMAGE_SIZE_128_ARM_PATCHED.
 * Returns 0 when the image is the patched 1.2.8 ARM32 exe, otherwise the code Win32Bootstrap must return. */
static int arm32_boot_identify_image(BYTE *image)
{
    const BYTE *pe;
    DWORD pe_offset;
    WORD machine;
    WORD sections;
    WORD magic;
    WORD dll_characteristics;
    DWORD size_of_image;
    DWORD entry_point;
    DWORD preferred_base;
    DWORD timestamp;
    char line[320];

    if (!image || IsBadReadPtr(image, 0x40) || image[0] != 'M' || image[1] != 'Z') {
        log_pointer("image identification: no MZ header at the exe module base", image);
        return ARM32_BOOT_BAD_PE_HEADER;
    }
    pe_offset = *(const DWORD *)(image + 0x3c);
    if (pe_offset < 0x40 || pe_offset > 0x1000 || IsBadReadPtr(image + pe_offset, 0x100) ||
        *(const DWORD *)(image + pe_offset) != 0x00004550u) {
        wsprintfA(line, "image identification: e_lfanew=%08lx does not point to a PE signature",
                  (unsigned long)pe_offset);
        log_line(line);
        return ARM32_BOOT_BAD_PE_HEADER;
    }
    pe = image + pe_offset;
    machine = *(const WORD *)(pe + 4);
    sections = *(const WORD *)(pe + 6);
    timestamp = *(const DWORD *)(pe + 8);
    magic = *(const WORD *)(pe + 0x18);
    entry_point = *(const DWORD *)(pe + 0x28);
    preferred_base = *(const DWORD *)(pe + 0x34);
    size_of_image = *(const DWORD *)(pe + 0x50);
    dll_characteristics = *(const WORD *)(pe + 0x5e);
    wsprintfA(line, "game image: base=%p preferred=%08lx machine=%04x magic=%04x sections=%u SizeOfImage=%08lx entry=%08lx "
                    "DllCharacteristics=%04x timestamp=%08lx",
              (void *)image, (unsigned long)preferred_base, (unsigned)machine, (unsigned)magic, (unsigned)sections,
              (unsigned long)size_of_image, (unsigned long)entry_point, (unsigned)dll_characteristics,
              (unsigned long)timestamp);
    log_line(line);
    if (machine != ARM32_PE_MACHINE_ARMNT || magic != ARM32_PE32_MAGIC) {
        wsprintfA(line, "image identification FAILED: PE machine %04x / magic %04x, expected %04x (ARMNT) / %04x (PE32): "
                        "this win32craft.dll build is only for the ARM32 Minecraft 1.2.8 exe",
                  (unsigned)machine, (unsigned)magic, (unsigned)ARM32_PE_MACHINE_ARMNT, (unsigned)ARM32_PE32_MAGIC);
        log_line(line);
        return ARM32_BOOT_BAD_PE_HEADER;
    }
    if (size_of_image != IMAGE_SIZE_128_ARM_PATCHED) {
        wsprintfA(line, "image identification FAILED: SizeOfImage %08lx, expected %08lx (patched ARM32 1.2.8 exe).%s",
                  (unsigned long)size_of_image, (unsigned long)IMAGE_SIZE_128_ARM_PATCHED,
                  size_of_image == IMAGE_SIZE_128_ARM_STOCK
                      ? " 0x01730000 is the stock exe: the ARM patcher was not applied."
                      : (size_of_image == IMAGE_SIZE_128_ARM_FALLBACK_SECTION
                             ? " 0x01732000 is the patcher's .d3tr fallback layout: add it to the gate."
                             : " Unknown Minecraft build."));
        log_line(line);
        return ARM32_BOOT_BAD_IMAGE_SIZE;
    }
    if (dll_characteristics & 0x1000u) {
        log_line("warning: DllCharacteristics still has IMAGE_DLLCHARACTERISTICS_APPCONTAINER (patcher step missing)");
    }
    if (sections != 8) {
        wsprintfA(line, "warning: %u sections, the patched exe has 8 (7 + .d3imp)", (unsigned)sections);
        log_line(line);
    }
    if ((DWORD)(ULONG_PTR)image != preferred_base) {
        wsprintfA(line, "image was rebased: %p instead of %08lx (all host RVAs are image-relative, nothing to do)",
                  (void *)image, (unsigned long)preferred_base);
        log_line(line);
    }
    log_line("image identification: patched Minecraft 1.2.8 ARM32 exe (machine 0x01C4, SizeOfImage 0x01731000)");
    return 0;
}

/* one log line at the first frames, then every 600th frame and at a few fixed counts (frame 1 is usually by far the
 * slowest: shader compilation, pack loading) */
static void arm32_boot_note_frame(LONG frame, DWORD loop_start, DWORD frame_ms)
{
    static LONG next_periodic = 600;
    static LONG slow_logged;
    BOOL milestone = frame <= 3 || frame == 5 || frame == 10 || frame == 30 || frame == 60 || frame == 120 ||
                     frame == 300 || frame >= next_periodic;
    char line[200];

    if (frame >= next_periodic) next_periodic += 600;
    if (milestone) {
        wsprintfA(line, "frame %ld complete: %lu ms since the loop started, this frame took %lu ms", (long)frame,
                  (unsigned long)(GetTickCount() - loop_start), (unsigned long)frame_ms);
        log_line(line);
    } else if (frame_ms >= 1000u && slow_logged < 20) {
        ++slow_logged;
        wsprintfA(line, "slow frame %ld: %lu ms", (long)frame, (unsigned long)frame_ms);
        log_line(line);
    }
}

/* PeekMessage pump + game frame, until WM_QUIT.  Returns the WM_QUIT wParam (0 for a normal window close / game quit). */
static int arm32_boot_run_loop(HWND window)
{
    MSG message;
    DWORD loop_start = GetTickCount();
    BOOL quit_requested = FALSE;
    char line[200];

    log_line("Win32 message/frame loop entered");
    arm32_boot_step("main loop");
    for (;;) {
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                wsprintfA(line, "message loop ended (WM_QUIT, wParam=%d) after %ld frames, %lu ms",
                          (int)message.wParam, (long)arm32_boot_frames_done,
                          (unsigned long)(GetTickCount() - loop_start));
                log_line(line);
                return (int)message.wParam;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        arm32_input_frame();   /* touch moves (one per contact), touch keyboard, input-mode log */
        drain_dispatcher_queue();
        refresh_relative_mouse_from_client_128();
        update_mouse_capture(window);
        if (!quit_requested) {
            LONG frame = InterlockedIncrement(&arm32_boot_frames_started);
            DWORD frame_start = GetTickCount();
            int quit;

            if (frame <= 3) {
                wsprintfA(line, "frame %ld: entering render_game_target_128", (long)frame);
                log_line(line);
            }
            quit = render_game_target_128();    /* appmain contract: nonzero = the game asked to quit */
            InterlockedIncrement(&arm32_boot_frames_done);
            arm32_boot_note_frame(frame, loop_start, GetTickCount() - frame_start);
            if (quit) {
                /* appmain facts: AppMain::update_frame returned MinecraftGame+9 (quit requested).  The frame function
                 * also posts WM_CLOSE once; WM_QUIT makes the exit independent of the window state. */
                quit_requested = TRUE;
                wsprintfA(line, "game requested quit after frame %ld: posting WM_QUIT", (long)frame);
                log_line(line);
                PostQuitMessage(0);
            }
        }
        InterlockedIncrement(&arm32_boot_loop_passes);
        InterlockedIncrement(&arm32_boot_progress);
        /* Yield without adding a 1 ms software frame cap. */
        Sleep(0);
    }
}

/* Returning from Win32Bootstrap enters the CRT's exit(): the game's static destructors run while its worker threads (FMOD,
 * asset/level save threads) may still be alive.  If that hangs the user is left with an invisible process, so arm a
 * last-resort timer: after 30 s the process is terminated with the same exit code (the log says so). */
static DWORD WINAPI arm32_boot_exit_guard_thread(void *parameter)
{
    UINT code = (UINT)(ULONG_PTR)parameter;

    Sleep(30000);
    log_line("ARM32 exit guard: the process did not finish exiting within 30 s (static destructors or worker threads hung): "
             "terminating it");
    TerminateProcess(GetCurrentProcess(), code);
    return 0;
}

static void arm32_boot_start_exit_guard(int code)
{
    typedef HANDLE (WINAPI *CreateThreadFn)(
        void *, SIZE_T, DWORD (WINAPI *)(void *), void *, DWORD, DWORD *);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    CreateThreadFn create_thread = kernel32
        ? (CreateThreadFn)GetProcAddress(kernel32, "CreateThread") : NULL;
    HANDLE thread = create_thread
        ? create_thread(NULL, 0, arm32_boot_exit_guard_thread, (void *)(ULONG_PTR)(UINT)code, 0, NULL) : NULL;

    if (!thread) {
        log_line("ARM32 exit guard: CreateThread failed, process exit is not guarded");
        return;
    }
    CloseHandle(thread);
    log_line("ARM32 exit guard armed (30 s): returning into the CRT exit()");
}

/* One identifying line right after option parsing: which build, which image, which feature-level cap, which options, which
 * log file.  (The lines before it are the file banner, paths, OS version and the image identification.) */
static void arm32_boot_log_startup_summary(BYTE *image)
{
    char command_line[240];
    char log_path[MAX_PATH];
    char line[800];

    arm32_boot_narrow(command_line, sizeof(command_line), GetCommandLineW());
    arm32_boot_narrow(log_path, sizeof(log_path), Win32CraftLogPathW);
    wsprintfA(line, "SUMMARY: Win32Craft 1.2.8 ARM32 host built %s %s | pid %lu | game image %p (SizeOfImage %08lx) | "
                    "feature level cap 0x%04X (0 = game list) | frame limit %s | log %s | command line [%s]",
              __DATE__, __TIME__, (unsigned long)GetCurrentProcessId(), (void *)image,
              (unsigned long)IMAGE_SIZE_128_ARM_PATCHED, (unsigned)arm32_feature_level_cap,
              host_unlimited_fps ? "unlimited" : "VSync", log_path[0] ? log_path : "(none)", command_line);
    log_line(line);
}

/* Minecraft PE 0.10.4.40 (WP8.1 XAML) host: toolkit, five units and x104_bootstrap (arm32/v0_10_4/HOST_DESIGN.md) */
#include "win32_host_arm32_0104.inl"

int WINAPI Win32Bootstrap(void)
{
    WNDCLASSEXW klass;
    HWND window;
    HINSTANCE instance = GetModuleHandleW(NULL);
    BYTE *image = (BYTE *)instance;
    HRESULT ro_result;
    RECT client;
    int code;
    char line[320];

    game_image = image;
    game_window = NULL;
    host_is_0104 = x104_image_matches(image);   /* quiet: selects the 0.10.4 host (PE machine 0x1C4 + SizeOfImage 0x00E6F000) before anything is logged */
    arm32_boot_start_tick = GetTickCount();
    game115_main_thread_id = GetCurrentThreadId();   /* log_115_main_thread_context() (watchdog) snapshots this thread */
    arm32_boot_setup_paths_and_log();
    arm32_boot_log_os_version();
    arm32_boot_start_watchdog();

    /* first, so that a fault in the PE parsing / option code below is traced too (needs game_image, set above) */
    arm32_boot_step("crash trace");
    install_crash_trace();
    log_line("crash trace installed (unhandled + vectored exception handlers)");
    arm32_install_abort_traps();

    if (host_is_0104) return x104_bootstrap(image);   /* Minecraft PE 0.10.4.40: everything else below is the 1.2.8 path */

    arm32_boot_step("identify image");
    code = arm32_boot_identify_image(image);
    if (code) return arm32_boot_fail(code, "image identification");

    host_is_128 = TRUE;
    host_is_115 = FALSE;
    host_is_116 = FALSE;
    host_is_windows7 = FALSE;   /* GetVersion is never 6.1 for the 6.2 ARM exe; no Windows 7 branch is used on ARM */
    log_line("host flags: host_is_128=1 host_is_115=0 host_is_116=0 host_is_windows7=0");

    arm32_boot_step("host options");
    Win32CraftParseHostOptions();
    arm32_boot_log_startup_summary(image);

    /* The first HSTRING call resolves combase/wintypes lazily and runs the "HSTRING self-test" (api_compat.c): do it now, on
     * the main thread outside any loader lock and before a game or FMOD thread can race for it; its PASSED/FAILED line is
     * the evidence that the genuine combase HSTRINGs are in use. */
    arm32_boot_step("HSTRING init");
    (void)WindowsIsStringEmpty(NULL);
    arm32_boot_step("RoInitialize");
    ro_result = RoInitialize(RO_INIT_MULTITHREADED);
    log_hresult("RoInitialize(RO_INIT_MULTITHREADED) [native combase]", ro_result);
    if (FAILED(ro_result) && ro_result != (HRESULT)0x80010106) {
        log_line("warning: RoInitialize failed - real WinRT activations of the game will fail");
    }

    arm32_boot_step("user data");
    create_user_data();
    if (!game_data_path[0]) {
        /* SHGetFolderPathW(CSIDL_APPDATA) failed (logged by create_user_data): keep the game away from an empty path */
        lstrcpyW(game_data_path, package_path);
        lstrcatW(game_data_path, L"\\MinecraftPE");
        CreateDirectoryW(game_data_path, NULL);
        log_line("warning: user data folder fallback next to the exe");
    }
    arm32_boot_narrow(line, sizeof(line), game_data_path);
    {
        char text[2 * MAX_PATH + 64];
        wsprintfA(text, "game data root (fake ApplicationData.LocalFolder): %s", line);
        log_line(text);
        arm32_boot_narrow(line, sizeof(line), package_path);
        wsprintfA(text, "package root (fake Package.InstalledLocation): %s", line);
        log_line(text);
    }

    arm32_boot_step("IAT hooks");
    if (!install_arm32_iat_hooks(image)) {
        return arm32_boot_fail(ARM32_BOOT_IAT_HOOKS_FAILED, "install_arm32_iat_hooks returned FALSE (mandatory hook missing)");
    }
    log_line("IAT hooks installed");
    arm32_log_module_inventory();
    arm32_boot_step("platform fixes");
    if (!install_arm32_platform_fixes(image)) {
        return arm32_boot_fail(ARM32_BOOT_PLATFORM_FIXES_FAILED, "install_arm32_platform_fixes returned FALSE");
    }
    log_line("platform fixes installed");
    arm32_boot_step("input hooks");
    if (!install_arm32_input_hooks(image)) {
        /* the x86 host ignored this result too: without the three mouse-mode hooks only the game's own relative-mouse
         * requests are not tracked (refresh_relative_mouse_from_client_128 still drives the capture); keep starting */
        log_line("ARM32 WARNING: install_arm32_input_hooks returned FALSE (see the lines above): continuing without the "
                 "mouse-mode hooks");
    } else {
        log_line("input hooks installed");
    }
    /* touch: device inventory + initial input source; on a touch-only machine the game starts in Touch input mode (must be in place before
     * MinecraftGame::vf27 creates the InputHandler, i.e. before initialize_game_app_128) */
    arm32_boot_step("touch input probe");
    arm32_input_probe();
    arm32_install_default_touch_mode(image);
    arm32_boot_step("client-state hook");
    if (!install_client_state_hook_128(image)) {
        log_line("Minecraft 1.2.8 client-state hook validation failed: continuing without it "
                 "(mouse capture / text-input focus detection will not work)");
    } else {
        log_line("client-state hook installed");
    }
    log_line("Win32Craft 1.2.8 ARM32 mode active");

    arm32_boot_step("window class");
    ZeroMemory(&klass, sizeof(klass));
    klass.cbSize = sizeof(klass);
    klass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    klass.lpfnWndProc = window_proc;
    klass.hInstance = instance;
    klass.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    klass.hbrBackground = (HBRUSH)(ULONG_PTR)(COLOR_WINDOWTEXT + 1);
    klass.lpszClassName = L"MCPE128Win32";
    if (!RegisterClassExW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        wsprintfA(line, "RegisterClassExW failed, GetLastError=%lu", (unsigned long)GetLastError());
        return arm32_boot_fail(ARM32_BOOT_REGISTER_CLASS_FAILED, line);
    }
    arm32_boot_step("create window");
    window = CreateWindowExW(
        0, klass.lpszClassName, L"Win32Craft 1.2.8",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        NULL, NULL, instance, NULL);
    if (!window) {
        wsprintfA(line, "CreateWindowExW failed, GetLastError=%lu", (unsigned long)GetLastError());
        return arm32_boot_fail(ARM32_BOOT_CREATE_WINDOW_FAILED, line);
    }
    game_window = window;
    initial_menu_cursor_guard = TRUE;
    if (GetClientRect(window, &client)) {
        wsprintfA(line, "HWND created: %p, client area %ldx%ld", (void *)window,
                  (long)(client.right - client.left), (long)(client.bottom - client.top));
    } else {
        wsprintfA(line, "HWND created: %p", (void *)window);
    }
    log_line(line);
    arm32_boot_step("touch input attach");
    arm32_input_attach_window(window);

    arm32_boot_step("initialize_game_d3d");
    if (!initialize_game_d3d(window)) {
        log_line("Minecraft 1.2.8 D3D bootstrap failed");
        return arm32_boot_fail(ARM32_BOOT_D3D_FAILED, "initialize_game_d3d returned FALSE");
    }
    log_line("Minecraft 1.2.8 D3D bootstrap ok");
    arm32_boot_step("initialize_game_app_128");
    if (!initialize_game_app_128(image)) {
        log_line("Minecraft 1.2.8 AppMain bootstrap failed");
        return arm32_boot_fail(ARM32_BOOT_APPMAIN_FAILED, "initialize_game_app_128 returned FALSE");
    }
    log_line("Minecraft 1.2.8 AppMain bootstrap ok");
    arm32_boot_step("activate_game_app_128");
    if (!activate_game_app_128(window)) {
        log_line("Minecraft 1.2.8 activation bootstrap failed");
        return arm32_boot_fail(ARM32_BOOT_ACTIVATE_FAILED, "activate_game_app_128 returned FALSE");
    }
    log_line("Minecraft 1.2.8 activation bootstrap ok");

    /* initial mouse mode exactly like x86: the startup listeners leave one stale relative request although the first
     * visible screen is the main menu */
    host_relative_requested = FALSE;
    mouse_clip_dirty = TRUE;
    set_game_relative_actual(FALSE);
    update_mouse_capture(window);
    log_line("Minecraft 1.2.8 D3D/AppMain bootstrap ready");

    code = arm32_boot_run_loop(window);
    arm32_touch_release_all("shutdown");
    arm32_input_log_stats("exit");
    arm32_boot_watchdog_stop = 1;
    wsprintfA(line, "Win32Bootstrap returning %d", code);
    log_line(line);
    arm32_boot_start_exit_guard(code);
    return code;
}
#else
int WINAPI Win32Bootstrap(void)
{
    WNDCLASSEXW klass;
    HWND window;
    MSG message;
    HINSTANCE instance = GetModuleHandleW(NULL);
    BYTE *image = (BYTE *)instance;
    HRESULT ro_result;
    DWORD pe_offset;
    DWORD image_size;

    game_image = image;
    game_window = NULL;
    Win32CraftParseHostOptions();
    pe_offset = *(DWORD *)(image + 0x3c);
    image_size = *(DWORD *)(image + pe_offset + 0x50);
#ifndef WIN32CRAFT_ARM32
    if (image_size == 0x02db1000) return bootstrap_116(image, instance);
    if (image_size == IMAGE_SIZE_0132_PATCHED ||
        image_size == IMAGE_SIZE_0142_PATCHED) {
        if (image_size == IMAGE_SIZE_0142_PATCHED) install_crash_trace();
        return Win32BootstrapEarly2016();
    }
#endif
    if (image_size != IMAGE_SIZE_01510_PATCHED &&
        image_size != IMAGE_SIZE_115_PATCHED &&
        image_size != IMAGE_SIZE_128_PATCHED) {
        return 3;
    }
    host_is_128 = image_size == IMAGE_SIZE_128_PATCHED;
    host_is_115 = image_size == IMAGE_SIZE_115_PATCHED;
    {
        typedef DWORD (WINAPI *GetVersionFn)(void);
        GetVersionFn get_version = (GetVersionFn)GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "GetVersion");
        DWORD version = get_version ? get_version() : 0;
        host_is_windows7 =
            (version & 0xffu) == 6 && ((version >> 8) & 0xffu) == 1;
    }
    log_line(host_is_128
        ? "Win32Craft 1.2.8 Win32Bootstrap entered"
        : (host_is_115
            ? "Win32Craft 1.1.5 universal v31 Win32Bootstrap entered"
            : "Win32Craft 0.15.10 universal v31 Win32Bootstrap entered"));
    install_crash_trace();
    install_exception_trace(
        image, host_is_128 ? RVA_128_CXX_THROW_IAT
                           : (host_is_115 ? RVA_115_CXX_THROW_IAT
                                          : RVA_CXX_THROW_IAT));
    install_create_file2_compat(
        image, host_is_128 ? RVA_128_CREATE_FILE2_IAT
                           : (host_is_115 ? RVA_115_CREATE_FILE2_IAT
                                          : RVA_CREATE_FILE2_IAT));
    log_line(host_is_windows7
        ? "Windows 7 DXGI present compatibility enabled"
        : "standard DXGI present path enabled");
    ro_result = RoInitialize(RO_INIT_MULTITHREADED);
    log_hresult("RoInitialize(RO_INIT_MULTITHREADED)", ro_result);
    if (!GetCurrentDirectoryW(MAX_PATH, package_path)) {
        lstrcpyW(package_path, L".");
    }
    create_user_data();
    install_activation_redirect(
        image, host_is_128 ? RVA_128_ACTIVATION_FACTORY_IAT
                           : (host_is_115 ? RVA_115_ACTIVATION_FACTORY_IAT
                                          : RVA_ACTIVATION_FACTORY_IAT));
    install_vccorlib_compat(
        image,
        host_is_128 ? RVA_128_PLATFORM_OBJECT_CTOR_IAT
                    : (host_is_115 ? RVA_115_PLATFORM_OBJECT_CTOR_IAT
                                   : RVA_PLATFORM_OBJECT_CTOR_IAT),
        host_is_128 ? RVA_128_PLATFORM_ALLOCATE2_IAT
                    : (host_is_115 ? RVA_115_PLATFORM_ALLOCATE2_IAT
                                   : RVA_PLATFORM_ALLOCATE2_IAT),
        host_is_128 ? RVA_128_PLATFORM_ALLOCATE1_IAT
                    : (host_is_115 ? RVA_115_PLATFORM_ALLOCATE1_IAT
                                   : RVA_PLATFORM_ALLOCATE1_IAT),
        host_is_128 ? RVA_128_GET_IBOX_ARRAY_VTABLE_IAT
                    : (host_is_115 ? RVA_115_GET_IBOX_ARRAY_VTABLE_IAT
                                   : RVA_GET_IBOX_ARRAY_VTABLE_IAT),
        host_is_128 ? RVA_128_GET_IBOX_VTABLE_IAT
                    : (host_is_115 ? RVA_115_GET_IBOX_VTABLE_IAT
                                   : RVA_GET_IBOX_VTABLE_IAT));
    install_d3d_compile_trace(
        image, host_is_128 ? RVA_128_D3D_COMPILE_IAT
                           : (host_is_115 ? RVA_115_D3D_COMPILE_IAT
                                          : RVA_D3D_COMPILE_IAT));
    if (!host_is_128) {
        patch_win7_device2_queries_to_base(
            image,
            host_is_115 ? RVA_115_D3D_DEVICE_IID : RVA_D3D_DEVICE_IID,
            host_is_115 ? RVA_115_D3D_CONTEXT_IID : RVA_D3D_CONTEXT_IID);
    }
    if (host_is_115 || host_is_128) {
        patch_win7_dxgi_device3_query_to_base(
            image, host_is_128
                ? RVA_128_DXGI_DEVICE3_IID : RVA_115_DXGI_DEVICE3_IID);
        patch_win7_discard_views(image);
    }
    install_d3d11_device_compat(
        image, host_is_128 ? RVA_128_D3D11_CREATE_DEVICE_IAT
                           : (host_is_115 ? RVA_115_D3D11_CREATE_DEVICE_IAT
                                          : RVA_D3D11_CREATE_DEVICE_IAT));
    if (host_is_128) {
        install_xbox_tcui_noops_128(image);
        patch_xbox_actions_noop(image);
    } else {
        install_xbox_tcui_noops(
            image,
            host_is_115
                ? RVA_115_PROCESS_PENDING_GAME_UI_IAT
                : RVA_PROCESS_PENDING_GAME_UI_IAT,
            host_is_115
                ? RVA_115_SHOW_PROFILE_CARD_UI_IAT
                : RVA_SHOW_PROFILE_CARD_UI_IAT);
        patch_xbox_actions_noop(image);
    }
    patch_chat_textbox_compat(image);
    if (host_is_128) {
        install_128_zip_resource_overrides(image);
        if (!install_client_state_hook_128(image)) {
            log_line("Minecraft 1.2.8 client-state hook validation failed");
            return 1;
        }
        resolve_fmod_delay_imports_128(image);
        install_mouse_mode_hooks(image);
        install_win32_platform_fixes_128(image);
        log_line("Win32Craft 1.2.8 mode active");
    } else if (host_is_115) {
        install_115_stdio_trace(image);
        log_line("Minecraft 1.1.5 CRT termination IAT hooks remain disabled in universal v31 build");
        trace_115_asset_candidates();
        if (memcmp(image + RVA_115_SOUND_JSON_LOAD_CALL,
                   "\x83\xc4\x04\x90\x90", 5) == 0) {
            log_line("Minecraft 1.1.5 temporary sounds.json loader bypass is active");
        } else {
            log_line("Minecraft 1.1.5 sounds.json loader remains enabled");
        }
        resolve_fmod_delay_imports_115(image);
        install_mouse_mode_hooks(image);
        install_win32_platform_fixes_115(image);
        log_line("Win32Craft 1.1.5 universal v31 mode active");
    } else {
        install_01510_copyright_override(image);
        install_development_version_text_01510(image);
        install_chat_submit_compat_01510(image);
        patch_win7_discard_views(image);
        verify_patched_audio_exe(image);
        install_native_fmod_bridge(image);
        install_mouse_mode_hooks(image);
        install_win32_platform_fixes(image);
        install_file_browser_compat_01510(image);
        install_material_task_compat(image);
    }

    ZeroMemory(&klass, sizeof(klass));
    klass.cbSize = sizeof(klass);
    klass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    klass.lpfnWndProc = window_proc;
    klass.hInstance = instance;
    klass.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    klass.hbrBackground = (HBRUSH)(ULONG_PTR)(COLOR_WINDOWTEXT + 1);
    klass.lpszClassName = host_is_128
        ? L"MCPE128Win32"
        : (host_is_115 ? L"MCPE115Win32" : L"MCPE01510Win32");
    if (!RegisterClassExW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        log_line("RegisterClassExW failed");
        return 1;
    }

    window = CreateWindowExW(
        0, klass.lpszClassName,
        host_is_128 ? L"Win32Craft 1.2.8"
                    : (host_is_115 ? L"Win32Craft 1.1.5"
                                   : L"Win32Craft 0.15.10"),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        NULL, NULL, instance, NULL);
    if (!window) {
        log_line("CreateWindowExW failed");
        return 2;
    }
    game_window = window;
    initial_menu_cursor_guard = TRUE;
    log_line("HWND created");
    if (host_is_128) {
        if (!initialize_game_d3d(window)) {
            log_line("Minecraft 1.2.8 D3D bootstrap failed");
        } else if (!initialize_game_app_128(image)) {
            log_line("Minecraft 1.2.8 AppMain bootstrap failed");
        } else if (!activate_game_app_128(window)) {
            log_line("Minecraft 1.2.8 activation bootstrap failed");
        } else {
            host_relative_requested = FALSE;
            mouse_clip_dirty = TRUE;
            set_game_relative_actual(FALSE);
            update_mouse_capture(window);
            log_line("Minecraft 1.2.8 D3D/AppMain bootstrap ready");
        }
    } else if (host_is_115) {
        /*
         * The 1.1.5 D3D resource layout is mapped independently from the
         * changed AppMainXaml constructor.  Bring up the real game device and
         * HWND swap chain while the application/frame ABI is still isolated.
         */
        patch_115_xaml_self_references(image);
        if (!initialize_game_d3d(window)) {
            log_line("Minecraft 1.1.5 D3D bootstrap failed");
        } else if (!initialize_game_app_115(image)) {
            log_line("Minecraft 1.1.5 AppMainXaml bootstrap failed");
        } else if (!activate_game_app_115(window)) {
            log_line("Minecraft 1.1.5 activation bootstrap failed");
        } else {
            log_line("Minecraft 1.1.5 D3D/AppMainXaml bootstrap ready");
        }
    } else if (!initialize_game_d3d(window)) {
        log_line("startup stopped: initialize_game_d3d failed");
    } else if (!initialize_game_app(image)) {
        log_line("startup stopped: initialize_game_app failed");
    } else if (!activate_game_app(window)) {
        log_line("startup stopped: activate_game_app failed");
    } else {
        /* Startup listeners leave one stale relative request even
         * though the first visible screen is the main menu. */
        host_relative_requested = FALSE;
        mouse_clip_dirty = TRUE;
        set_game_relative_actual(FALSE);
        update_mouse_capture(window);
        log_line("initial main-menu cursor forced to absolute mode");
    }

    log_line("Win32 message/frame loop entered");
    for (;;) {
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                log_line("message loop ended");
                return (int)message.wParam;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (host_is_115 || host_is_128) drain_dispatcher_queue();
        refresh_relative_mouse_from_client_128();
        update_mouse_capture(window);
        if (host_is_128) {
            render_game_target_128();
        } else if (host_is_115) {
            render_game_target_115();
        } else {
            render_game_target();
            process_chat_submit_01510();
        }
        /* Yield without adding a 1 ms software frame cap. */
        Sleep(0);
    }
}




#endif /* WIN32CRAFT_ARM32: Win32Bootstrap */

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    (void)instance;
    (void)reserved;
    if (reason == 0) Win32CraftCleanupCopyrightTemps();
    /* The patched startup site calls Win32Bootstrap directly. Leave the CRT's
     * DisableThreadLibraryCalls call alone to avoid starting a second host loop. */
    return TRUE;
}

HRESULT WINAPI D3D11CreateDevice(
    void *adapter, UINT driver_type, HMODULE software, UINT flags,
    const UINT *feature_levels, UINT feature_level_count, UINT sdk_version,
    ID3D11Device **device, UINT *selected_feature_level,
    ID3D11DeviceContext **immediate_context)
{
    static GameD3D11CreateDeviceFn system_create_device;
    HMODULE module;

    if (!system_create_device) {
        module = LoadLibraryW(L"d3d11.dll");
        if (module) {
            system_create_device =
                (GameD3D11CreateDeviceFn)GetProcAddress(
                    module, "D3D11CreateDevice");
        }
    }
    if (!system_create_device) return E_FAIL;
    return system_create_device(
        adapter, driver_type, software, flags,
        feature_levels, feature_level_count, sdk_version,
        device, selected_feature_level, immediate_context);
}

HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(
    void *dxgi_device, void **graphics_device)
{
    typedef HRESULT (WINAPI *CreateDirect3D11DeviceFn)(void *, void **);
    static CreateDirect3D11DeviceFn function;
    HMODULE module;

    if (!function) {
        module = LoadLibraryW(L"d3d11.dll");
        if (module) {
            function = (CreateDirect3D11DeviceFn)GetProcAddress(
                module, "CreateDirect3D11DeviceFromDXGIDevice");
        }
    }
    return function ? function(dxgi_device, graphics_device) : E_FAIL;
}

HRESULT WINAPI CreateDXGIFactory2(
    UINT flags, const GUID *iid, void **factory)
{
    typedef HRESULT (WINAPI *CreateDXGIFactory2Fn)(
        UINT, const GUID *, void **);
    static CreateDXGIFactory2Fn function;
    HMODULE module;

    if (!function) {
        module = LoadLibraryW(L"dxgi.dll");
        if (module) {
            function = (CreateDXGIFactory2Fn)GetProcAddress(
                module, "CreateDXGIFactory2");
        }
    }
    return function ? function(flags, iid, factory) : E_FAIL;
}
