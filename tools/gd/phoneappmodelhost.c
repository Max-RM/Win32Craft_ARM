/* PhoneAppModelHost.dll shim for Win32Craft (Geometry Dash and other WP8.0 ports).
 *
 * The real phone-only PhoneAppModelHost.dll does not exist on Windows RT 8.1 / Windows 10 ARM32 desktop.
 * It is imported by:
 *   - GeometryJumpComponent.dll  : CaptureUiThreadContext
 *   - libcocos2d.dll             : CaptureUiThreadContext
 *   - fmodex_80_arm.dll          : LoadPackagedLibrary
 * (LaunchEmergencyDialer is in the real DLL's export table but no GD binary imports it.)
 *
 * Ship this DLL named exactly "PhoneAppModelHost.dll" in the game folder; the loader finds the app-local
 * copy first, so no byte of the game DLLs has to be patched.  See games_recon/geometry_dash_gd-deps.md F4.
 *
 * Freestanding: no CRT, built with clang thumbv7-pc-windows-msvc /nodefaultlib, exports via the .def file.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* HRESULT CaptureUiThreadContext(IContextCallback** ppContext)
 *
 * Returning a FAILED HRESULT (NOT S_OK) is required.  The game's inlined PPL (_ContextCallback) code uses this
 * to decide whether there is a UI-thread apartment to marshal task continuations to.  On this process we are MTA,
 * so the real phone call would capture the UI context; here there is none.  _IsCurrentOriginSTA treats a result
 * >= 0 as "we are on the UI thread" and would then make continuations marshal to a context that does not exist
 * (hang / throw).  With a failure the caller stores NULL and PPL continuations simply run on the thread pool.
 * (games_recon/geometry_dash_gd-deps.md F4.) */
HRESULT CaptureUiThreadContext(void **ppContext)
{
    if (ppContext) *ppContext = NULL;
    return E_NOTIMPL;   /* 0x80004001 */
}

/* HMODULE LoadPackagedLibrary(LPCWSTR name, DWORD reserved)
 *
 * FMOD Ex's plugin loader (FMOD_OS_Library_Load).  KERNELBASE!LoadPackagedLibrary fails with
 * APPMODEL_ERROR_NO_PACKAGE in a plain (non-packaged) Win32 process, so do an ordinary library load instead.
 * GD never reaches this path (it imports no FMOD loadPlugin/setPluginPath), but keep it correct. */
HMODULE WINAPI shim_LoadPackagedLibrary(LPCWSTR name, DWORD reserved)   /* exported as LoadPackagedLibrary via the .def (avoids the winbase.h dllimport clash) */
{
    (void)reserved;
    return LoadLibraryExW(name, NULL, 0);
}

/* Present only so the export table matches the real DLL; never imported by a GD binary. */
HRESULT LaunchEmergencyDialer(void)
{
    return E_NOTIMPL;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(h);
    return TRUE;
}
