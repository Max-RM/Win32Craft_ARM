/* crit_vt.c - critic test of the design's "private vtable copy" device hook at FL 9_1:
   (1) which ID3D11DeviceN interfaces share the object/vtable, (2) how long the real vtable is,
   (3) does a swapped vtable keep DXGI swap chain creation / Present / interop working,
   (4) which methods does DXGI itself call through the swapped vtable during CreateSwapChain/Present.
   Usage: crit_vt.exe [warp] [short]   (short = copy only the 54 ID3D11Device2 slots) */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_3.h>
#include <stdio.h>
#include <string.h>
#include <psapi.h>

static void **g_orig; static void *g_copy[256]; static volatile LONG g_calls[256]; static int g_n;
typedef HRESULT (STDMETHODCALLTYPE *GENFN)(void);
/* generic counting thunks are not portable; instead wrap a few interesting slots explicitly */
static HRESULT (STDMETHODCALLTYPE *o_CreateTexture2D)(ID3D11Device *, const D3D11_TEXTURE2D_DESC *, const D3D11_SUBRESOURCE_DATA *, ID3D11Texture2D **);
static HRESULT STDMETHODCALLTYPE h_CreateTexture2D(ID3D11Device *d, const D3D11_TEXTURE2D_DESC *a, const D3D11_SUBRESOURCE_DATA *b, ID3D11Texture2D **c) { InterlockedIncrement(&g_calls[5]); return o_CreateTexture2D(d, a, b, c); }
static HRESULT (STDMETHODCALLTYPE *o_CreateRTV)(ID3D11Device *, ID3D11Resource *, const D3D11_RENDER_TARGET_VIEW_DESC *, ID3D11RenderTargetView **);
static HRESULT STDMETHODCALLTYPE h_CreateRTV(ID3D11Device *d, ID3D11Resource *a, const D3D11_RENDER_TARGET_VIEW_DESC *b, ID3D11RenderTargetView **c) { InterlockedIncrement(&g_calls[9]); return o_CreateRTV(d, a, b, c); }
static D3D_FEATURE_LEVEL (STDMETHODCALLTYPE *o_GetFL)(ID3D11Device *);
static D3D_FEATURE_LEVEL STDMETHODCALLTYPE h_GetFL(ID3D11Device *d) { InterlockedIncrement(&g_calls[37]); return o_GetFL(d); }
static HRESULT (STDMETHODCALLTYPE *o_CFS)(ID3D11Device *, D3D11_FEATURE, void *, UINT);
static HRESULT STDMETHODCALLTYPE h_CFS(ID3D11Device *d, D3D11_FEATURE f, void *p, UINT s) { InterlockedIncrement(&g_calls[33]); return o_CFS(d, f, p, s); }
static HRESULT (STDMETHODCALLTYPE *o_QI)(ID3D11Device *, REFIID, void **);
static HRESULT STDMETHODCALLTYPE h_QI(ID3D11Device *d, REFIID r, void **o) { InterlockedIncrement(&g_calls[0]); return o_QI(d, r, o); }

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0); int warp = 0, shrt = 0; for (int i = 1; i < argc; ++i) { if (!strcmp(argv[i], "warp")) warp = 1; if (!strcmp(argv[i], "short")) shrt = 1; }
    D3D_FEATURE_LEVEL want = D3D_FEATURE_LEVEL_9_1, got = 0; ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL;
    HRESULT hr = D3D11CreateDevice(NULL, warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED, &want, 1, D3D11_SDK_VERSION, &dev, &got, &ctx);
    printf("device %s hr=0x%08lx FL=0x%x (no debug layer, SINGLETHREADED like Unity)\n", warp ? "WARP" : "HW", (unsigned long)hr, got);
    if (FAILED(hr)) return 1;
    const IID *iids[6] = { &IID_ID3D11Device, &IID_ID3D11Device1, &IID_ID3D11Device2, &IID_ID3D11Device3, &IID_ID3D11Device4, &IID_ID3D11Device5 };
    for (int i = 0; i < 6; ++i) { void *p = NULL; HRESULT h = ID3D11Device_QueryInterface(dev, iids[i], &p);
        printf("  QI ID3D11Device%c hr=0x%08lx ptr=%p same-object=%d same-vtbl=%d\n", i ? '0' + i : ' ', (unsigned long)h, p, p == (void *)dev, p && *(void **)p == *(void **)dev); if (p) IUnknown_Release((IUnknown *)p); }
    g_orig = *(void ***)dev;
    MODULEINFO mi; HMODULE d3d = GetModuleHandleA("d3d11.dll"); GetModuleInformation(GetCurrentProcess(), d3d, &mi, sizeof mi);
    unsigned char *lo = (unsigned char *)mi.lpBaseOfDll, *hi = lo + mi.SizeOfImage;
    for (g_n = 0; g_n < 256; ++g_n) { unsigned char *f = (unsigned char *)g_orig[g_n]; if (f < lo || f >= hi) break; }
    printf("  device vtable: %d consecutive entries point into d3d11.dll (ID3D11Device=43, Device1=50, Device2=54, Device3=65, Device4=67, Device5=69)\n", g_n);
    int ncopy = shrt ? 54 : g_n;
    memcpy(g_copy, g_orig, sizeof(void *) * ncopy);
    for (int i = ncopy; i < 256; ++i) g_copy[i] = NULL;  /* a short copy leaves the tail NULL -> crash if anyone calls Device3+ through it */
    o_QI = g_orig[0]; g_copy[0] = (void *)h_QI; o_CreateTexture2D = g_orig[5]; g_copy[5] = (void *)h_CreateTexture2D; o_CreateRTV = g_orig[9]; g_copy[9] = (void *)h_CreateRTV;
    o_CFS = g_orig[33]; g_copy[33] = (void *)h_CFS; o_GetFL = g_orig[37]; g_copy[37] = (void *)h_GetFL;
    *(void ***)dev = g_copy;
    printf("  vtable swapped (%d slots copied)\n", ncopy);
    WNDCLASSA wc = {0}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "critvt"; RegisterClassA(&wc);
    HWND w = CreateWindowA("critvt", "critvt", WS_OVERLAPPEDWINDOW, 0, 0, 640, 360, NULL, NULL, NULL, NULL);
    IDXGIDevice *dd = NULL; IDXGIAdapter *ad = NULL; IDXGIFactory2 *fa = NULL;
    hr = ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd); printf("  QI IDXGIDevice through swapped vtable hr=0x%08lx\n", (unsigned long)hr);
    IDXGIDevice_GetAdapter(dd, &ad); IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory2, (void **)&fa);
    LONG before[256]; memcpy(before, (void *)g_calls, sizeof before);
    DXGI_SWAP_CHAIN_DESC1 d = {0}; d.Width = 640; d.Height = 360; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1; d.BufferUsage = 0x30;
    d.BufferCount = 2; d.Scaling = DXGI_SCALING_STRETCH; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; d.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    IDXGISwapChain1 *sc = NULL; hr = IDXGIFactory2_CreateSwapChainForHwnd(fa, (IUnknown *)dev, w, &d, NULL, NULL, &sc);
    printf("  CreateSwapChainForHwnd(Unity desc) through swapped device hr=0x%08lx\n", (unsigned long)hr);
    printf("  calls made by DXGI into the hooked slots during swap-chain creation: QI %ld, CreateTexture2D %ld, CreateRTV %ld, CheckFeatureSupport %ld, GetFeatureLevel %ld\n",
           g_calls[0] - before[0], g_calls[5] - before[5], g_calls[9] - before[9], g_calls[33] - before[33], g_calls[37] - before[37]);
    if (sc) { ID3D11Texture2D *bb = NULL; IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
        ID3D11RenderTargetView *rtv = NULL; hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv); printf("  CreateRenderTargetView(back buffer) hr=0x%08lx (hook count %ld)\n", (unsigned long)hr, g_calls[9]);
        float c[4] = { 0.2f, 0.4f, 0.6f, 1 }; for (int i = 0; i < 30; ++i) { ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL); ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, c); hr = IDXGISwapChain1_Present(sc, 0, 0); if (FAILED(hr)) break; }
        printf("  30 x Present hr=0x%08lx\n", (unsigned long)hr);
        hr = IDXGISwapChain1_ResizeBuffers(sc, 0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT); printf("  ResizeBuffers while RTV alive hr=0x%08lx (expected failure DXGI_ERROR_INVALID_CALL)\n", (unsigned long)hr);
        ID3D11RenderTargetView_Release(rtv); ID3D11Texture2D_Release(bb); ID3D11DeviceContext_OMSetRenderTargets(ctx, 0, NULL, NULL); ID3D11DeviceContext_Flush(ctx);
        hr = IDXGISwapChain1_ResizeBuffers(sc, 0, 800, 450, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT); printf("  ResizeBuffers 800x450 hr=0x%08lx\n", (unsigned long)hr);
        IDXGISwapChain1_Release(sc); }
    { ID3D11Device3 *d3 = NULL; hr = ID3D11Device_QueryInterface(dev, &IID_ID3D11Device3, (void **)&d3);
      if (d3) { D3D11_TEXTURE2D_DESC1 t = {0}; t.Width = t.Height = 16; t.MipLevels = 1; t.ArraySize = 1; t.Format = DXGI_FORMAT_R8G8B8A8_UNORM; t.SampleDesc.Count = 1; t.BindFlags = D3D11_BIND_SHADER_RESOURCE;
          ID3D11Texture2D1 *tt = NULL; LONG b5 = g_calls[5];
          if (!shrt) { hr = ID3D11Device3_CreateTexture2D1(d3, &t, NULL, &tt); printf("  ID3D11Device3::CreateTexture2D1 through the copy hr=0x%08lx; CreateTexture2D hook fired %ld times (a Device3 caller BYPASSES the slot-5 hook)\n", (unsigned long)hr, g_calls[5] - b5); }
          else printf("  short copy: ID3D11Device3 methods would jump to NULL (not called)\n");
          if (tt) ID3D11Texture2D1_Release(tt); ID3D11Device3_Release(d3); } }
    printf("  GetDeviceRemovedReason hr=0x%08lx\n", (unsigned long)ID3D11Device_GetDeviceRemovedReason(dev));
    *(void ***)dev = g_orig;
    return 0;
}
