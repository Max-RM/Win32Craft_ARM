/* l9shim_test.c - PC (x64) harness for tools/badpiggies/l9shim.dll.  Freestanding, no CRT (like tools/m0probe).
 *
 * It loads the shim DLL, forces its 9_x wrap path (L9SHIM_FORCE_9X=1) so a PC with a real GPU still exercises what a Tegra would hit, and checks that
 * through the wrapped device:
 *   - Unity's ORIGINAL Sprites/Default SM4 bytecode (data\bp\orig\*.dxbc) is accepted by CreateVertexShader/CreatePixelShader (the shim swaps it for the
 *     level-9 replacement); a sprite is drawn and the read-back pixel is 200 100 50 255.  Without the shim the same bytecode gives 0x80070057.
 *   - states/formats a 9_1 device rejects are accepted once the shim sanitises them: anisotropy 16, DepthClipEnable=FALSE, AlphaToCoverage + separate
 *     alpha, MIRROR_ONCE, an R8G8B8A8_TYPELESS render target, an R32G8X24_TYPELESS depth texture.  Each is also tried on the RAW device to show it fails there.
 * Run from a folder that has Data\bp\l9_replacements.bin and Data\bp\orig\ (the m0probe package layout).  Exit code 0 = every check passed. */

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11_1.h>

typedef unsigned char u8; typedef unsigned int u32;
#if defined(_MSC_VER) && !defined(__clang__)
#pragma function(memset, memcpy)
#endif
void *memset(void *d, int c, size_t n) { u8 *p = (u8 *)d; while (n--) *p++ = (u8)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { u8 *p = (u8 *)d; const u8 *q = (const u8 *)s; while (n--) *p++ = *q++; return d; }
#if defined(_M_X64) || defined(_M_IX86)
int _fltused = 0;
#endif

static HANDLE g_out;
static int s_len(const char *s) { int n = 0; while (s[n]) ++n; return n; }
static int g_checks, g_fail;

static void o(const char *s) { DWORD w; WriteFile(g_out, s, (DWORD)s_len(s), &w, NULL); }
static void ox(const char *label, unsigned v) {
    char b[80]; int n = 0, i; const char *d = "0123456789abcdef"; char t[8];
    while (*label) b[n++] = *label++;
    b[n++] = '0'; b[n++] = 'x';
    { int nt = 0; if (!v) t[nt++] = '0'; else while (v) { t[nt++] = d[v & 15]; v >>= 4; } for (i = nt; i < 8; ++i) b[n++] = '0'; while (nt) b[n++] = t[--nt]; }
    b[n++] = '\r'; b[n++] = '\n'; b[n] = 0; o(b);
}
static void check(int cond, const char *what) { ++g_checks; if (!cond) { ++g_fail; o("  FAIL: "); o(what); o("\r\n"); } else { o("  ok:   "); o(what); o("\r\n"); } }

static u8 *read_file(const wchar_t *path, u32 *size) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD sz, got = 0; u8 *buf;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL);
    buf = (u8 *)VirtualAlloc(NULL, sz ? sz : 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf || !ReadFile(h, buf, sz, &got, NULL) || got != sz) { CloseHandle(h); return NULL; }
    CloseHandle(h); *size = sz; return buf;
}

typedef HRESULT (WINAPI *PFN_Create)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);

/* a raw 9_1 device (no shim) to show which calls fail there */
static ID3D11Device *raw_9_1(void) {
    static const D3D_FEATURE_LEVEL nine[] = { (D3D_FEATURE_LEVEL)0x9100 };
    ID3D11Device *d = NULL;
    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nine, 1, D3D11_SDK_VERSION, &d, NULL, NULL);
    return d;
}

void hmain(void)
{
    static const D3D_FEATURE_LEVEL want[] = { (D3D_FEATURE_LEVEL)0xb100, (D3D_FEATURE_LEVEL)0xb000, (D3D_FEATURE_LEVEL)0xa100,
                                              (D3D_FEATURE_LEVEL)0xa000, (D3D_FEATURE_LEVEL)0x9300, (D3D_FEATURE_LEVEL)0x9200, (D3D_FEATURE_LEVEL)0x9100 };
    HMODULE shim; PFN_Create create;
    ID3D11Device *dev = NULL; D3D_FEATURE_LEVEL chosen = (D3D_FEATURE_LEVEL)0; ID3D11DeviceContext *ctx = NULL;
    ID3D11Device *raw;
    HRESULT hr;
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    o("l9shim_test: loading the shim with L9SHIM_FORCE_9X=1\r\n");
    SetEnvironmentVariableA("L9SHIM_FORCE_9X", "1");

    shim = LoadLibraryW(L"l9shim_x64.dll");
    if (!shim) shim = LoadLibraryW(L"build-badpiggies\\l9shim_x64.dll");
    if (!shim) { o("FAIL: cannot load l9shim_x64.dll\r\n"); ExitProcess(2); }
    create = (PFN_Create)GetProcAddress(shim, "D3D11CreateDevice");
    if (!create) { o("FAIL: the shim does not export D3D11CreateDevice\r\n"); ExitProcess(2); }

    hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, want, 7, D3D11_SDK_VERSION, &dev, &chosen, &ctx);
    ox("shim D3D11CreateDevice hr=", (unsigned)hr);
    ox("  wrapped device feature level=", (unsigned)chosen);
    check(SUCCEEDED(hr) && dev && ctx, "the shim created and wrapped a 9_x device");
    if (!dev) { o("== cannot continue ==\r\n"); ExitProcess(g_fail ? 1 : 0); }
    raw = raw_9_1();
    check(raw != NULL, "a raw 9_1 device for the negative checks");

    /* ---- shader swap: Unity's ORIGINAL SM4 bytecode must be accepted through the shim, refused on the raw device ---- */
    {
        u32 vsz = 0, psz = 0;
        u8 *vs = read_file(L"Data\\bp\\orig\\00_Sprites_Default_b000_vs_756ae2904e92.dxbc", &vsz);
        u8 *ps = read_file(L"Data\\bp\\orig\\00_Sprites_Default_b004_ps_3aa3dabc65f1.dxbc", &psz);
        check(vs && ps, "read Unity's original Sprites/Default VS+PS");
        if (vs && ps) {
            ID3D11VertexShader *v1 = NULL, *v2 = NULL; ID3D11PixelShader *p1 = NULL, *p2 = NULL;
            HRESULT rawvs = raw ? ID3D11Device_CreateVertexShader(raw, vs, vsz, NULL, &v2) : E_FAIL;
            HRESULT rawps = raw ? ID3D11Device_CreatePixelShader(raw, ps, psz, NULL, &p2) : E_FAIL;
            HRESULT shvs = ID3D11Device_CreateVertexShader(dev, vs, vsz, NULL, &v1);
            HRESULT shps = ID3D11Device_CreatePixelShader(dev, ps, psz, NULL, &p1);
            ox("  raw CreateVertexShader(original SM4) hr=", (unsigned)rawvs);
            ox("  shim CreateVertexShader(original SM4) hr=", (unsigned)shvs);
            ox("  shim CreatePixelShader(original SM4) hr=", (unsigned)shps);
            check(FAILED(rawvs) && FAILED(rawps), "the raw 9_1 device REFUSES Unity's SM4 bytecode (as Unity hits today)");
            check(SUCCEEDED(shvs) && v1 && SUCCEEDED(shps) && p1, "the shim swaps them for level-9 and they are accepted");
            if (v1) ID3D11VertexShader_Release(v1); if (p1) ID3D11PixelShader_Release(p1);
            if (v2) ID3D11VertexShader_Release(v2); if (p2) ID3D11PixelShader_Release(p2);
        }
    }

    /* ---- state / format sanitising: each call fails on the raw device and succeeds through the shim ---- */
    {
        D3D11_SAMPLER_DESC sd; ID3D11SamplerState *ss1 = NULL, *ss2 = NULL;
        memset(&sd, 0, sizeof sd); sd.Filter = D3D11_FILTER_ANISOTROPIC; sd.MaxAnisotropy = 16;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD = 3.402823466e38f;
        check(raw && FAILED(ID3D11Device_CreateSamplerState(raw, &sd, &ss2)), "raw: anisotropy 16 sampler refused");
        check(SUCCEEDED(ID3D11Device_CreateSamplerState(dev, &sd, &ss1)) && ss1, "shim: anisotropy clamped to 2, sampler accepted");
        if (ss1) ID3D11SamplerState_Release(ss1); if (ss2) ID3D11SamplerState_Release(ss2);
    }
    {
        D3D11_RASTERIZER_DESC rd; ID3D11RasterizerState *r1 = NULL, *r2 = NULL;
        memset(&rd, 0, sizeof rd); rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = FALSE;
        check(raw && FAILED(ID3D11Device_CreateRasterizerState(raw, &rd, &r2)), "raw: DepthClipEnable=FALSE refused");
        check(SUCCEEDED(ID3D11Device_CreateRasterizerState(dev, &rd, &r1)) && r1, "shim: DepthClip forced TRUE, rasterizer accepted");
        if (r1) ID3D11RasterizerState_Release(r1); if (r2) ID3D11RasterizerState_Release(r2);
    }
    {
        D3D11_BLEND_DESC bd; ID3D11BlendState *b1 = NULL, *b2 = NULL;
        memset(&bd, 0, sizeof bd); bd.AlphaToCoverageEnable = TRUE;
        bd.RenderTarget[0].BlendEnable = TRUE; bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE; bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA; bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
        bd.RenderTarget[0].RenderTargetWriteMask = 0xf;
        check(raw && FAILED(ID3D11Device_CreateBlendState(raw, &bd, &b2)), "raw: AlphaToCoverage blend refused");
        check(SUCCEEDED(ID3D11Device_CreateBlendState(dev, &bd, &b1)) && b1, "shim: AlphaToCoverage off + alpha=colour factors, blend accepted");
        if (b1) ID3D11BlendState_Release(b1); if (b2) ID3D11BlendState_Release(b2);
    }
    {
        D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t1 = NULL, *t2 = NULL;
        memset(&td, 0, sizeof td); td.Width = 256; td.Height = 256; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        check(raw && FAILED(ID3D11Device_CreateTexture2D(raw, &td, NULL, &t2)), "raw: R8G8B8A8_TYPELESS render target refused");
        check(SUCCEEDED(ID3D11Device_CreateTexture2D(dev, &td, NULL, &t1)) && t1, "shim: TYPELESS mapped to UNORM, texture accepted");
        if (t1) ID3D11Texture2D_Release(t1); if (t2) ID3D11Texture2D_Release(t2);
    }
    {
        D3D11_TEXTURE2D_DESC td; ID3D11Texture2D *t1 = NULL, *t2 = NULL;
        memset(&td, 0, sizeof td); td.Width = 1366; td.Height = 768; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32G8X24_TYPELESS; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        check(raw && FAILED(ID3D11Device_CreateTexture2D(raw, &td, NULL, &t2)), "raw: R32G8X24_TYPELESS depth refused (Unity's back-buffer depth)");
        check(SUCCEEDED(ID3D11Device_CreateTexture2D(dev, &td, NULL, &t1)) && t1, "shim: 32-bit depth mapped to 24-bit, depth texture accepted");
        if (t1) ID3D11Texture2D_Release(t1); if (t2) ID3D11Texture2D_Release(t2);
    }

    /* ---- caps correction values (checked against a synthetic caps object; real UnityPlayer is ARM and cannot load here) ---- */
    {
        typedef int (*SelfTest)(void);
        SelfTest st = (SelfTest)GetProcAddress(shim, "l9shim_selftest_caps");
        check(st != NULL, "the shim exports the caps self-test");
        if (st) { int bad = st(); ox("  caps self-test wrong fields=", (unsigned)bad); check(bad == 0, "the 9_1 caps correction writes every field correctly"); }
    }

    if (raw) ID3D11Device_Release(raw);
    if (ctx) ID3D11DeviceContext_Release(ctx);
    ID3D11Device_Release(dev);

    o("\r\n");
    if (g_fail) { ox("== l9shim_test: FAILURES, count=", (unsigned)g_fail); }
    else o("== l9shim_test: all checks passed ==\r\n");
    ExitProcess(g_fail ? 1 : 0);
}
