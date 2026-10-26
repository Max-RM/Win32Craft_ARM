/* PhoneAudioSes.dll shim for Win32Craft (Geometry Dash / FMOD Ex on WP8.0 ports).
 *
 * The real phone-only PhoneAudioSes.dll (a thin WASAPI-activation layer) does not exist on Windows RT 8.1 /
 * Windows 10 ARM32 desktop.  fmodex_80_arm.dll's WASAPI output module (fmod_output_wasapi.cpp) imports three
 * functions; this shim implements them on the standard desktop MMDevice API (mmdevapi.dll, present on RT 8.1
 * and Windows 10).  Ship this DLL named exactly "PhoneAudioSes.dll" in the game folder.
 * See games_recon/geometry_dash_gd-deps.md F5.  Signatures are inferred from the FMOD call sites and are to be
 * confirmed by a device log on first audio bring-up (milestone M5).
 *
 * Freestanding: no CRT, clang thumbv7-pc-windows-msvc /nodefaultlib; exports via the .def file; links ole32.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmdeviceapi.h>

/* Defined inline (freestanding + /nodefaultlib does not pull the SDK's GUID objects or uuid.lib). */
static const GUID x_CLSID_MMDeviceEnumerator = {0xBCDE0395,0xE52F,0x467C,{0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}};
static const GUID x_IID_IMMDeviceEnumerator  = {0xA95664D2,0x9614,0x4F35,{0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}};

/* role argument the FMOD code passes: 0 = default.  Map 0 -> eConsole, anything else -> eCommunications. */
static ERole map_role(DWORD role) { return role == 0 ? eConsole : eCommunications; }

static HRESULT make_enumerator(IMMDeviceEnumerator **out)
{
    *out = NULL;
    /* FMOD's output thread may not have initialised COM; do it defensively and never uninitialise (the
     * returned interfaces must outlive this call).  S_FALSE / RPC_E_CHANGED_MODE are fine. */
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    return CoCreateInstance(&x_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &x_IID_IMMDeviceEnumerator, (void **)out);
}

/* Returns a CoTaskMemAlloc'd device-id string (the caller frees it with CoTaskMemFree), or NULL on failure. */
static LPWSTR default_endpoint_id(EDataFlow flow, DWORD role)
{
    IMMDeviceEnumerator *e = NULL;
    IMMDevice *dev = NULL;
    LPWSTR id = NULL;
    if (FAILED(make_enumerator(&e)) || !e) return NULL;
    if (SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(e, flow, map_role(role), &dev)) && dev) {
        if (FAILED(IMMDevice_GetId(dev, &id))) id = NULL;
        IMMDevice_Release(dev);
    }
    IMMDeviceEnumerator_Release(e);
    return id;
}

LPWSTR GetDefaultAudioRenderId(DWORD role)
{
    return default_endpoint_id(eRender, role);
}

LPWSTR GetDefaultAudioCaptureId(DWORD role)
{
    return default_endpoint_id(eCapture, role);
}

/* HRESULT ActivateAudioInterface(LPCWSTR deviceId, REFIID iid, void** ppv):
 * synchronous activation of an audio interface (IAudioClient / IAudioClient2) on the given endpoint id.
 * The phone API blocked until ready; GetDevice->Activate is the synchronous desktop equivalent. */
HRESULT ActivateAudioInterface(LPCWSTR deviceId, REFIID iid, void **ppv)
{
    IMMDeviceEnumerator *e = NULL;
    IMMDevice *dev = NULL;
    HRESULT hr;
    if (ppv) *ppv = NULL;
    if (!ppv || !iid) return E_POINTER;
    hr = make_enumerator(&e);
    if (FAILED(hr) || !e) return FAILED(hr) ? hr : E_FAIL;
    hr = IMMDeviceEnumerator_GetDevice(e, deviceId, &dev);
    if (SUCCEEDED(hr) && dev) {
        hr = IMMDevice_Activate(dev, iid, CLSCTX_ALL, NULL, ppv);
        IMMDevice_Release(dev);
    }
    IMMDeviceEnumerator_Release(e);
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(h);
    return TRUE;
}
