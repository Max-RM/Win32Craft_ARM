/* win32_host_arm32_0104_platform.inl - UNIT "platform" of the Minecraft PE 0.10.4.40 ARM32 host.     *** STUB - TODO(platform) ***
 *
 * Owner: the platform implementer.  Brief: arm32/v0_10_4/units/platform.md.  Rules / threads / ordering / cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.
 * Compile check: python tools/arm32/try_0104.py --unit platform
 *
 * This file owns: Package (InstalledLocation.Path = game folder, ASCII only), ApplicationData (LocalFolder.Path, LocalSettings.Values),
 * MemoryManager, VibrationDevice, NetworkInformation, CurrentAppSimulator/CurrentApp, AsyncCausalityTracer, the Windows.Web.Http stubs,
 * the data folder / cwd handling, the AppPlatform vtable data-cell patches (network 0x33FB6C, buy 0x33FB7C, vibrate 0x33FB94,
 * free memory 0x33FBC0) and the XAudio2 fallback cells (0xDB804C / 0xDB8050).
 * The bodies below are placeholders that only keep the tree compiling; replace them (keep every signature and the public function names). */
#ifndef WIN32_HOST_ARM32_0104_PLATFORM_INL
#define WIN32_HOST_ARM32_0104_PLATFORM_INL

/* TODO(platform): FIRST unit to run (before the window): verify x104.game_root is ASCII (else return FALSE with a clear log line), decide and
 * create x104.data_root (%APPDATA%\MinecraftPE\0.10.4 style, ASCII; fall back to <game_root>\LocalState), patch the cells above, probe XAudio2. */
static BOOL x104_platform_install(BYTE *image)
{
    (void)image;
    return TRUE;
}

static void x104_platform_shutdown(void)
{
}

/* TODO(platform): the 12 classes the core table assigns to the platform unit (x104_class_owners): Package, ApplicationData, MemoryManager,
 * VibrationDevice, NetworkInformation, CurrentAppSimulator, CurrentApp, AsyncCausalityTracer, Http{RequestMessage,Method,StringContent,Client}. */
static HRESULT x104_platform_activate(const wchar_t *class_name, const GUID *iid, void **out)
{
    (void)class_name;
    (void)iid;
    (void)out;
    return REGDB_E_CLASSNOTREG;
}

#endif /* WIN32_HOST_ARM32_0104_PLATFORM_INL */
