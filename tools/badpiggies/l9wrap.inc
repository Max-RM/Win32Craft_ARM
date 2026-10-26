/* l9wrap.inc - included by l9shim.c: the D3D11CreateDevice policy and the per-method device-vtable wrappers.
 * Vtable slot numbers are the measured ones (games_recon/bad_piggies_bench/critic/crit_vt.c, confirmed on a Surface 2 Tegra):
 *   5 CreateTexture2D  7 CreateShaderResourceView  9 CreateRenderTargetView  10 CreateDepthStencilView
 *   12 CreateVertexShader  15 CreatePixelShader  20 CreateBlendState  21 CreateDepthStencilState  22 CreateRasterizerState
 *   23 CreateSamplerState  29 CheckFormatSupport  33 CheckFeatureSupport  37 GetFeatureLevel  45 CreateBlendState1 (ID3D11Device1, same object/vtable). */

enum {
    SLOT_CreateTexture2D = 5, SLOT_CreateSRV = 7, SLOT_CreateRTV = 9, SLOT_CreateDSV = 10,
    SLOT_CreateVS = 12, SLOT_CreatePS = 15, SLOT_CreateBlend = 20, SLOT_CreateDepthStencil = 21,
    SLOT_CreateRasterizer = 22, SLOT_CreateSampler = 23, SLOT_CheckFormatSupport = 29,
    SLOT_CheckFeatureSupport = 33, SLOT_GetFeatureLevel = 37, SLOT_CreateBlend1 = 45,
    VT_MAX = 512
};

static void *g_dev;                 /* the one wrapped device (Unity creates exactly one) */
static void **g_vt_orig;            /* its original vtable */
static void *g_vt_copy[VT_MAX];     /* our private copy, installed into the device */
static int g_vt_len;
static D3D_FEATURE_LEVEL g_real_level;
static int g_wrapped;
static int g_shadow_support;        /* D3D11_FEATURE_D3D9_OPTIONS.SupportsDepthAsTextureWithLessEqualComparisonFilter: 24-bit depth can keep SHADER_RESOURCE */
static volatile LONG g_vs_hit, g_vs_miss, g_ps_hit, g_ps_miss, g_tex_fix, g_state_fix, g_srv_fail;

/* ---- module address range (to measure the vtable length: entries that point into d3d11.dll) ---- */
static void mod_range(const wchar_t *name, ULONG_PTR *lo, ULONG_PTR *hi)
{
    HMODULE m = GetModuleHandleW(name);
    *lo = *hi = 0;
    if (!m) return;
    {
        const u8 *base = (const u8 *)m;
        DWORD pe = *(const DWORD *)(base + 0x3c);
        DWORD size = *(const DWORD *)(base + pe + 0x50);     /* OptionalHeader.SizeOfImage */
        *lo = (ULONG_PTR)base; *hi = (ULONG_PTR)base + size;
    }
}

/* original slot as a typed function pointer */
#define ORIG(slot, type) ((type)g_vt_orig[(slot)])
#define SELF_ORIG(self, slot, type) ((type)(((void ***)(self))[0] == g_vt_copy ? g_vt_orig[(slot)] : ((void ***)(self))[0][(slot)]))

/* ======================================================================================================================
 * shader swap: CreateVertexShader (12) / CreatePixelShader (15)
 * ==================================================================================================================== */
/* DIAGNOSTIC: trace every shader-create call (hit/miss, size, HRESULT) so the log shows whether Unity reaches shader
 * creation at all before the error-shader crash, and whether the driver accepts the swapped bytecode. */
#define L9_SHTRACE 1
static HRESULT STDMETHODCALLTYPE h_CreateVS(ID3D11Device *self, const void *code, SIZE_T len, ID3D11ClassLinkage *link, ID3D11VertexShader **out)
{
    u32 rsz; const void *rep = table_lookup(code, len, &rsz);
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, const void *, SIZE_T, ID3D11ClassLinkage *, ID3D11VertexShader **);
    HRESULT hr;
    if (rep) {
        long n = InterlockedIncrement(&g_vs_hit);
        hr = ORIG(SLOT_CreateVS, Fn)(self, rep, rsz, link, out);
#if L9_SHTRACE
        if (n <= 80) lgf("l9shim: CreateVertexShader HIT #%ld (%u -> %u bytes) hr=0x%08x", n, (unsigned)len, rsz, (unsigned)hr);
#endif
        return hr;
    }
    {
        long n = InterlockedIncrement(&g_vs_miss);
        hr = ORIG(SLOT_CreateVS, Fn)(self, code, len, link, out);   /* let the driver reject SM4 (0x80070057): logged, not crashed */
        if (n <= 8) dump_blob("l9_miss_vs_", n, code, (u32)len);
        if (n <= 40) lgf("l9shim: CreateVertexShader MISS #%ld (no level-9 replacement, %u bytes) hr=0x%08x - dumped l9_miss_vs_%ld.dxbc - the object will be invisible", n, (unsigned)len, (unsigned)hr, n);
        return hr;
    }
}
static HRESULT STDMETHODCALLTYPE h_CreatePS(ID3D11Device *self, const void *code, SIZE_T len, ID3D11ClassLinkage *link, ID3D11PixelShader **out)
{
    u32 rsz; const void *rep = table_lookup(code, len, &rsz);
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, const void *, SIZE_T, ID3D11ClassLinkage *, ID3D11PixelShader **);
    HRESULT hr;
    if (rep) {
        long n = InterlockedIncrement(&g_ps_hit);
        hr = ORIG(SLOT_CreatePS, Fn)(self, rep, rsz, link, out);
#if L9_SHTRACE
        if (n <= 80) lgf("l9shim: CreatePixelShader HIT #%ld (%u -> %u bytes) hr=0x%08x", n, (unsigned)len, rsz, (unsigned)hr);
#endif
        return hr;
    }
    {
        long n = InterlockedIncrement(&g_ps_miss);
        hr = ORIG(SLOT_CreatePS, Fn)(self, code, len, link, out);
        if (n <= 8) dump_blob("l9_miss_ps_", n, code, (u32)len);
        if (n <= 40) lgf("l9shim: CreatePixelShader MISS #%ld (no level-9 replacement, %u bytes) hr=0x%08x - dumped l9_miss_ps_%ld.dxbc - the object will be invisible", n, (unsigned)len, (unsigned)hr, n);
        return hr;
    }
}

/* ======================================================================================================================
 * texture + view format sanitising (10Level9 rules).  Insurance: Bad Piggies' own data has no 32-bit depth or typeless colour,
 * but Unity's internal back-buffer depth is R32G8X24_TYPELESS and some RTs are *_TYPELESS; those are illegal at 9_1.
 * ==================================================================================================================== */
static DXGI_FORMAT color_typeless_to_unorm(DXGI_FORMAT f)
{
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
    default: return f;
    }
}
/* 32-bit depth is unavailable at 9_1; 24-bit is.  Keep it typeless so a shader-resource view can still be made on shadow-capable drivers (the Tegra reports shadow support). */
static int is_depth32_typeless(DXGI_FORMAT f) { return f == DXGI_FORMAT_R32G8X24_TYPELESS || f == DXGI_FORMAT_R32_TYPELESS; }

static HRESULT STDMETHODCALLTYPE h_CreateTexture2D(ID3D11Device *self, const D3D11_TEXTURE2D_DESC *desc, const D3D11_SUBRESOURCE_DATA *init, ID3D11Texture2D **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, const D3D11_TEXTURE2D_DESC *, const D3D11_SUBRESOURCE_DATA *, ID3D11Texture2D **);
    D3D11_TEXTURE2D_DESC d;
    if (!desc) return ORIG(SLOT_CreateTexture2D, Fn)(self, desc, init, out);
    d = *desc;
    if (d.Format != color_typeless_to_unorm(d.Format)) { d.Format = color_typeless_to_unorm(d.Format); InterlockedIncrement(&g_tex_fix); }
    if (is_depth32_typeless(d.Format)) { d.Format = DXGI_FORMAT_R24G8_TYPELESS; InterlockedIncrement(&g_tex_fix); }   /* 32-bit -> 24-bit depth */
    if (d.Format == DXGI_FORMAT_R24G8_TYPELESS && (d.BindFlags & D3D11_BIND_SHADER_RESOURCE) && !g_shadow_support) d.BindFlags &= ~(UINT)D3D11_BIND_SHADER_RESOURCE;
    if (d.Format == DXGI_FORMAT_A8_UNORM && d.MipLevels != 1) { d.MipLevels = 1; InterlockedIncrement(&g_tex_fix); }    /* A8 has no mip support at 9_1 */
    if ((d.MiscFlags & D3D11_RESOURCE_MISC_SHARED) && (d.Format == DXGI_FORMAT_R8G8B8A8_UNORM || d.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)) d.MiscFlags &= ~(UINT)D3D11_RESOURCE_MISC_SHARED;
    if ((d.Width > 2048 || d.Height > 2048) && (g_tex_fix) <= 40) lgf("l9shim: CreateTexture2D %ux%u exceeds the 9_1 2048 limit (format %u) - left as is, the driver may clamp", d.Width, d.Height, d.Format);
    return ORIG(SLOT_CreateTexture2D, Fn)(self, &d, init, out);
}

static HRESULT STDMETHODCALLTYPE h_CreateSRV(ID3D11Device *self, ID3D11Resource *res, const D3D11_SHADER_RESOURCE_VIEW_DESC *desc, ID3D11ShaderResourceView **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, ID3D11Resource *, const D3D11_SHADER_RESOURCE_VIEW_DESC *, ID3D11ShaderResourceView **);
    D3D11_SHADER_RESOURCE_VIEW_DESC d;
    HRESULT hr;
    if (!desc) return ORIG(SLOT_CreateSRV, Fn)(self, res, desc, out);
    d = *desc;
    if (d.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS) d.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    else if (d.Format != color_typeless_to_unorm(d.Format)) d.Format = color_typeless_to_unorm(d.Format);
    hr = ORIG(SLOT_CreateSRV, Fn)(self, res, &d, out);
    if (FAILED(hr)) {
        /* At 9_1 there is no typeless format: a typed SRV (e.g. an sRGB view, or a differently-typed view of what Unity made typeless) over a
         * resource the texture sanitiser collapsed to one concrete UNORM type is rejected with E_INVALIDARG.  Retry with a NULL desc so the view
         * inherits the resource's own, now fully-typed, format and dimension.  This recovers the "id=41" TexturesD3D11.cpp:388 failure. */
        HRESULT hr2 = ORIG(SLOT_CreateSRV, Fn)(self, res, NULL, out);
        long n = InterlockedIncrement(&g_srv_fail);
        if (n <= 40) lgf("l9shim: CreateSRV viewfmt=%u dim=%u failed hr=0x%08x; NULL-desc retry hr=0x%08x%s", (unsigned)d.Format, (unsigned)d.ViewDimension, (unsigned)hr, (unsigned)hr2, SUCCEEDED(hr2) ? " (recovered)" : "");
        if (SUCCEEDED(hr2)) return hr2;
    }
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_CreateRTV(ID3D11Device *self, ID3D11Resource *res, const D3D11_RENDER_TARGET_VIEW_DESC *desc, ID3D11RenderTargetView **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, ID3D11Resource *, const D3D11_RENDER_TARGET_VIEW_DESC *, ID3D11RenderTargetView **);
    D3D11_RENDER_TARGET_VIEW_DESC d;
    if (!desc) return ORIG(SLOT_CreateRTV, Fn)(self, res, desc, out);
    d = *desc;
    if (d.Format != color_typeless_to_unorm(d.Format)) d.Format = color_typeless_to_unorm(d.Format);
    return ORIG(SLOT_CreateRTV, Fn)(self, res, &d, out);
}
static HRESULT STDMETHODCALLTYPE h_CreateDSV(ID3D11Device *self, ID3D11Resource *res, const D3D11_DEPTH_STENCIL_VIEW_DESC *desc, ID3D11DepthStencilView **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, ID3D11Resource *, const D3D11_DEPTH_STENCIL_VIEW_DESC *, ID3D11DepthStencilView **);
    D3D11_DEPTH_STENCIL_VIEW_DESC d;
    if (!desc) return ORIG(SLOT_CreateDSV, Fn)(self, res, desc, out);
    d = *desc;
    if (d.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) d.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;   /* matches the 24-bit depth texture */
    else if (d.Format == DXGI_FORMAT_D32_FLOAT) d.Format = DXGI_FORMAT_D16_UNORM;
    return ORIG(SLOT_CreateDSV, Fn)(self, res, &d, out);
}

/* ======================================================================================================================
 * pipeline-state sanitising (only the fields a 9_1 device rejects at creation; Bad Piggies' own content mostly avoids them)
 * ==================================================================================================================== */
static HRESULT STDMETHODCALLTYPE h_CreateSampler(ID3D11Device *self, const D3D11_SAMPLER_DESC *desc, ID3D11SamplerState **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, const D3D11_SAMPLER_DESC *, ID3D11SamplerState **);
    D3D11_SAMPLER_DESC d;
    if (!desc) return ORIG(SLOT_CreateSampler, Fn)(self, desc, out);
    d = *desc;
    if ((d.Filter & 0xC0) == 0xC0 /* anisotropic */ && d.MaxAnisotropy > 2) { d.MaxAnisotropy = 2; InterlockedIncrement(&g_state_fix); }
    if (d.AddressU == D3D11_TEXTURE_ADDRESS_MIRROR_ONCE) d.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
    if (d.AddressV == D3D11_TEXTURE_ADDRESS_MIRROR_ONCE) d.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
    if (d.AddressW == D3D11_TEXTURE_ADDRESS_MIRROR_ONCE) d.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
    if ((d.Filter & 0x80) && !g_shadow_support) d.Filter = (D3D11_FILTER)(d.Filter & ~0x80);   /* comparison filter needs shadow support */
    return ORIG(SLOT_CreateSampler, Fn)(self, &d, out);
}
static HRESULT STDMETHODCALLTYPE h_CreateRasterizer(ID3D11Device *self, const D3D11_RASTERIZER_DESC *desc, ID3D11RasterizerState **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, const D3D11_RASTERIZER_DESC *, ID3D11RasterizerState **);
    D3D11_RASTERIZER_DESC d;
    if (!desc) return ORIG(SLOT_CreateRasterizer, Fn)(self, desc, out);
    d = *desc;
    if (!d.DepthClipEnable) { d.DepthClipEnable = TRUE; InterlockedIncrement(&g_state_fix); }    /* must be TRUE at 9_1 */
    d.DepthBiasClamp = 0.0f;
    return ORIG(SLOT_CreateRasterizer, Fn)(self, &d, out);
}
static void sanitize_blend_rt(D3D11_RENDER_TARGET_BLEND_DESC *rt)
{
    rt->SrcBlendAlpha = rt->SrcBlend; rt->DestBlendAlpha = rt->DestBlend; rt->BlendOpAlpha = rt->BlendOp;   /* no separate-alpha blend at 9_1 */
}
static HRESULT STDMETHODCALLTYPE h_CreateBlend(ID3D11Device *self, const D3D11_BLEND_DESC *desc, ID3D11BlendState **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, const D3D11_BLEND_DESC *, ID3D11BlendState **);
    D3D11_BLEND_DESC d; int i;
    if (!desc) return ORIG(SLOT_CreateBlend, Fn)(self, desc, out);
    d = *desc;
    d.AlphaToCoverageEnable = FALSE;
    d.IndependentBlendEnable = FALSE;
    sanitize_blend_rt(&d.RenderTarget[0]);
    for (i = 1; i < 8; ++i) d.RenderTarget[i] = d.RenderTarget[0];
    return ORIG(SLOT_CreateBlend, Fn)(self, &d, out);
}
static HRESULT STDMETHODCALLTYPE h_CreateBlend1(ID3D11Device1 *self, const D3D11_BLEND_DESC1 *desc, ID3D11BlendState1 **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device1 *, const D3D11_BLEND_DESC1 *, ID3D11BlendState1 **);
    D3D11_BLEND_DESC1 d; int i;
    if (!desc) return ORIG(SLOT_CreateBlend1, Fn)(self, desc, out);
    d = *desc;
    d.AlphaToCoverageEnable = FALSE;
    d.IndependentBlendEnable = FALSE;
    d.RenderTarget[0].LogicOpEnable = FALSE;
    d.RenderTarget[0].SrcBlendAlpha = d.RenderTarget[0].SrcBlend; d.RenderTarget[0].DestBlendAlpha = d.RenderTarget[0].DestBlend; d.RenderTarget[0].BlendOpAlpha = d.RenderTarget[0].BlendOp;
    for (i = 1; i < 8; ++i) d.RenderTarget[i] = d.RenderTarget[0];
    return ORIG(SLOT_CreateBlend1, Fn)(self, &d, out);
}
static HRESULT STDMETHODCALLTYPE h_CreateDepthStencil(ID3D11Device *self, const D3D11_DEPTH_STENCIL_DESC *desc, ID3D11DepthStencilState **out)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, const D3D11_DEPTH_STENCIL_DESC *, ID3D11DepthStencilState **);
    D3D11_DEPTH_STENCIL_DESC d;
    if (!desc) return ORIG(SLOT_CreateDepthStencil, Fn)(self, desc, out);
    d = *desc;
    d.BackFace = d.FrontFace;         /* no two-sided stencil at 9_1 */
    return ORIG(SLOT_CreateDepthStencil, Fn)(self, &d, out);
}

/* ======================================================================================================================
 * caps queries.  The matching in-memory correction of Unity's own caps object (which it fills from these) is l9caps.inc;
 * here the device is simply asked the truth, and the answers are logged so a device run shows what Unity saw.
 * ==================================================================================================================== */
static HRESULT STDMETHODCALLTYPE h_CheckFeatureSupport(ID3D11Device *self, D3D11_FEATURE feature, void *data, UINT size)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, D3D11_FEATURE, void *, UINT);
    HRESULT hr = ORIG(SLOT_CheckFeatureSupport, Fn)(self, feature, data, size);
    l9caps_on_check_feature((UINT)feature);     /* fires the 9_1 caps correction on Unity's last caps-init query (feature 0xf) */
    return hr;
}
static HRESULT STDMETHODCALLTYPE h_CheckFormatSupport(ID3D11Device *self, DXGI_FORMAT format, UINT *support)
{
    typedef HRESULT (STDMETHODCALLTYPE *Fn)(ID3D11Device *, DXGI_FORMAT, UINT *);
    return ORIG(SLOT_CheckFormatSupport, Fn)(self, format, support);
}
static D3D_FEATURE_LEVEL STDMETHODCALLTYPE h_GetFeatureLevel(ID3D11Device *self)
{
    typedef D3D_FEATURE_LEVEL (STDMETHODCALLTYPE *Fn)(ID3D11Device *);
    (void)self;
    g_fl_queried = 1;                            /* Unity's caps init (sub_1007434) is the only GetFeatureLevel caller; the real level is honest (9_1) */
    return ORIG(SLOT_GetFeatureLevel, Fn)(self);
}

/* ======================================================================================================================
 * install the wrappers on the one device object
 * ==================================================================================================================== */
static void wrap_device(ID3D11Device *dev)
{
    ULONG_PTR lo, hi; int i;
    void **orig = *(void ***)dev;
    mod_range(L"d3d11.dll", &lo, &hi);
    if (!lo) { lgf("l9shim: cannot find d3d11.dll module range - device NOT wrapped"); return; }
    for (i = 0; i < VT_MAX; ++i) {
        ULONG_PTR p = (ULONG_PTR)orig[i];
        if (p < lo || p >= hi) break;
    }
    g_vt_len = i;
    if (g_vt_len <= SLOT_CreateBlend1) { lgf("l9shim: measured vtable length %d is too short - device NOT wrapped", g_vt_len); return; }
    for (i = 0; i < g_vt_len; ++i) g_vt_copy[i] = orig[i];
    g_vt_orig = orig;
    g_vt_copy[SLOT_CreateTexture2D]      = (void *)h_CreateTexture2D;
    g_vt_copy[SLOT_CreateSRV]            = (void *)h_CreateSRV;
    g_vt_copy[SLOT_CreateRTV]            = (void *)h_CreateRTV;
    g_vt_copy[SLOT_CreateDSV]            = (void *)h_CreateDSV;
    g_vt_copy[SLOT_CreateVS]             = (void *)h_CreateVS;
    g_vt_copy[SLOT_CreatePS]             = (void *)h_CreatePS;
    g_vt_copy[SLOT_CreateBlend]          = (void *)h_CreateBlend;
    g_vt_copy[SLOT_CreateDepthStencil]   = (void *)h_CreateDepthStencil;
    g_vt_copy[SLOT_CreateRasterizer]     = (void *)h_CreateRasterizer;
    g_vt_copy[SLOT_CreateSampler]        = (void *)h_CreateSampler;
    g_vt_copy[SLOT_CheckFormatSupport]   = (void *)h_CheckFormatSupport;
    g_vt_copy[SLOT_CheckFeatureSupport]  = (void *)h_CheckFeatureSupport;
    g_vt_copy[SLOT_GetFeatureLevel]      = (void *)h_GetFeatureLevel;
    if (g_vt_len > SLOT_CreateBlend1) g_vt_copy[SLOT_CreateBlend1] = (void *)h_CreateBlend1;
    *(void ***)dev = g_vt_copy;
    g_dev = dev;
    g_wrapped = 1;
    lgf("l9shim: device wrapped (vtable %d slots copied; VS/PS swap, texture/view/state sanitisers, caps queries)", g_vt_len);
    l9caps_arm();            /* arm the 9_1 caps correction of Unity's caps object (only in this 9_x path; a no-op on any non-Bad-Piggies build) */
}

static void query_shadow_support(ID3D11Device *dev)
{
    D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT s; memset(&s, 0, sizeof s);
    if (SUCCEEDED(ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D9_SHADOW_SUPPORT, &s, sizeof s)))
        g_shadow_support = s.SupportsDepthAsTextureWithLessEqualComparisonFilter ? 1 : 0;
    lgf("l9shim: D3D9_SHADOW_SUPPORT=%d (24-bit depth %s keep SHADER_RESOURCE)", g_shadow_support, g_shadow_support ? "can" : "cannot");
}

/* ======================================================================================================================
 * D3D11CreateDevice: the policy
 * ==================================================================================================================== */
typedef HRESULT (WINAPI *PFN_D3D11CreateDevice)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
static PFN_D3D11CreateDevice g_real_create;

static const char *fl_name(D3D_FEATURE_LEVEL l)
{
    switch ((unsigned)l) { case 0xb100: return "11_1"; case 0xb000: return "11_0"; case 0xa100: return "10_1"; case 0xa000: return "10_0";
        case 0x9300: return "9_3"; case 0x9200: return "9_2"; case 0x9100: return "9_1"; default: return "?"; }
}

HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driver, HMODULE software, UINT flags,
                                 const D3D_FEATURE_LEVEL *levels, UINT count, UINT sdk,
                                 ID3D11Device **devOut, D3D_FEATURE_LEVEL *levelOut, ID3D11DeviceContext **ctxOut)
{
    static const D3D_FEATURE_LEVEL nine[] = { (D3D_FEATURE_LEVEL)0x9300, (D3D_FEATURE_LEVEL)0x9200, (D3D_FEATURE_LEVEL)0x9100 };
    HRESULT hr;
    ID3D11Device *dev = NULL; D3D_FEATURE_LEVEL chosen = (D3D_FEATURE_LEVEL)0; ID3D11DeviceContext *ctx = NULL;
    if (!g_real_create) { HMODULE m = real_d3d11(); g_real_create = m ? (PFN_D3D11CreateDevice)GetProcAddress(m, "D3D11CreateDevice") : NULL; }
    if (!g_real_create) return E_FAIL;

    /* test hook (off in production): L9SHIM_FORCE_9X=1 skips the FL10 attempt so the 9_x wrap path can be exercised on a PC that has a real GPU */
    {
        char v[8]; DWORD fn = GetEnvironmentVariableA("L9SHIM_FORCE_9X", v, sizeof v);
        if (fn == 1 && v[0] == '1') { lgf("l9shim: L9SHIM_FORCE_9X=1 - skipping the FL10 attempt (test mode)"); goto force9x; }
    }

    /* 1. Unity's call unchanged.  If it gives a FL10+ device, the shim steps aside entirely (a newer GPU runs Unity untouched). */
    hr = g_real_create(adapter, driver, software, flags, levels, count, sdk, &dev, &chosen, &ctx);
    if (SUCCEEDED(hr) && dev && (unsigned)chosen >= 0xA000u) {
        lgf("l9shim: D3D11CreateDevice -> %s on the game's own request; FL10+, the shim does nothing", fl_name(chosen));
        if (devOut) *devOut = dev; else ID3D11Device_Release(dev);
        if (levelOut) *levelOut = chosen;
        if (ctxOut) *ctxOut = ctx; else if (ctx) ID3D11DeviceContext_Release(ctx);
        return hr;
    }
    if (dev) ID3D11Device_Release(dev);
    if (ctx) ID3D11DeviceContext_Release(ctx);
    dev = NULL; ctx = NULL; chosen = (D3D_FEATURE_LEVEL)0;

    /* 2. below FL10 (or no device): create a 9_x device and wrap it so Unity's FL10 content becomes 9_1-legal */
    lgf("l9shim: the game's request did not give a FL10+ device (hr=0x%08x); creating a 9_x device and wrapping it", (unsigned)hr);
force9x:
    hr = g_real_create(adapter, driver, software, flags, nine, 3, sdk, &dev, &chosen, &ctx);
    if (FAILED(hr) || !dev) { lgf("l9shim: 9_x device creation FAILED hr=0x%08x - returning it to the game (it will fall back to WARP as before)", (unsigned)hr);
        if (devOut) *devOut = dev; if (levelOut) *levelOut = chosen; if (ctxOut) *ctxOut = ctx; return hr; }
    g_real_level = chosen;
    lgf("l9shim: created a %s device (driver type %d); wrapping it", fl_name(chosen), driver);
    query_shadow_support(dev);
    wrap_device(dev);
    if (devOut) *devOut = dev; else ID3D11Device_Release(dev);
    if (levelOut) *levelOut = chosen;      /* the truth; l9caps.inc handles what Unity's caps init is told */
    if (ctxOut) *ctxOut = ctx; else if (ctx) ID3D11DeviceContext_Release(ctx);
    return S_OK;
}
