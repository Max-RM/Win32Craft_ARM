#include "../../win32_compat.h"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    (void)instance; (void)reason; (void)reserved;
    return TRUE;
}

int WINAPI Win32Bootstrap(void)
{
    char line[64];
    wsprintfA(line, "hello %d", 42);
    OutputDebugStringA(line);
    return 0;
}

HRESULT WINAPI D3D11CreateDevice(
    void *adapter, UINT driver_type, HMODULE software, UINT flags,
    const UINT *feature_levels, UINT feature_level_count, UINT sdk_version,
    ID3D11Device **device, UINT *selected_feature_level,
    ID3D11DeviceContext **immediate_context)
{
    (void)adapter; (void)driver_type; (void)software; (void)flags;
    (void)feature_levels; (void)feature_level_count; (void)sdk_version;
    (void)device; (void)selected_feature_level; (void)immediate_context;
    return E_FAIL;
}

HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(void *dxgi_device, void **graphics_device)
{ (void)dxgi_device; (void)graphics_device; return E_FAIL; }

HRESULT WINAPI CreateDXGIFactory2(UINT flags, const GUID *iid, void **factory)
{ (void)flags; (void)iid; (void)factory; return E_FAIL; }

/* float + 64-bit division to see which runtime helpers are required */
int test_div(int a, int b, long long c, long long d)
{
    return (a / b) + (a % b) + (int)(c / d) + (int)((unsigned long long)c % (unsigned long long)d);
}
