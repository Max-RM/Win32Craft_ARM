/* l9bench.c - x64 test bench: what a real D3D11 device created with ONLY D3D_FEATURE_LEVEL_9_1
   accepts from Unity 2018.4 (Bad Piggies Rebooted), and whether the level_9_1 replacement table works.
   Usage: l9bench.exe <l9_replacements.bin> <dir with Unity *.dxbc subdirs> <needed_sha1_list.txt> [warp] */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11_2.h>
#include <d3d11sdklayers.h>
#include <dxgi1_3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

static ID3D11Device *dev; static ID3D11DeviceContext *ctx; static ID3D11InfoQueue *iq;
static int g_pass, g_fail; static ID3D11Buffer *g_ib;

static void drain(const char *tag) {
    if (!iq) return;
    UINT64 n = ID3D11InfoQueue_GetNumStoredMessages(iq);
    for (UINT64 i = 0; i < n; ++i) {
        SIZE_T len = 0; ID3D11InfoQueue_GetMessage(iq, i, NULL, &len);
        D3D11_MESSAGE *m = (D3D11_MESSAGE *)malloc(len);
        if (m && SUCCEEDED(ID3D11InfoQueue_GetMessage(iq, i, m, &len)) && m->Severity <= D3D11_MESSAGE_SEVERITY_WARNING)
            printf("      [debug %s] %.*s\n", tag, (int)m->DescriptionByteLength, m->pDescription);
        free(m);
    }
    ID3D11InfoQueue_ClearStoredMessages(iq);
}
/* expect: 1 = should succeed, 0 = should fail, -1 = just report */
static void rep(const char *name, HRESULT hr, int expect) {
    const char *v = expect < 0 ? "info" : ((SUCCEEDED(hr) ? 1 : 0) == expect ? "as-expected" : "UNEXPECTED");
    printf("  %-72s hr=0x%08lx %s\n", name, (unsigned long)hr, v);
    if (expect >= 0) { if (!strcmp(v, "as-expected")) g_pass++; else g_fail++; }
    drain(name);
}
static HRESULT tex2d(UINT w, UINT h, UINT mips, DXGI_FORMAT f, UINT bind, ID3D11Texture2D **out) {
    D3D11_TEXTURE2D_DESC d = {0}; d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = 1; d.Format = f;
    d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    ID3D11Texture2D *t = NULL; HRESULT hr = ID3D11Device_CreateTexture2D(dev, &d, NULL, &t);
    if (out) *out = t; else if (t) ID3D11Texture2D_Release(t);
    return hr;
}
/* ---- shader table ---- */
typedef struct { unsigned char ck[16]; unsigned off, size; } Ent;
static unsigned char *tbl; static Ent *ents; static unsigned nents;
static const unsigned char *lookup(const unsigned char *ck, unsigned *size) {
    for (unsigned i = 0; i < nents; ++i) if (!memcmp(ents[i].ck, ck, 16)) { *size = ents[i].size; return tbl + ents[i].off; }
    return NULL;
}
static unsigned char *readall(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char *)malloc(l); fread(b, 1, l, f); fclose(f); *n = (size_t)l; return b;
}
static const unsigned char *chunk(const unsigned char *b, size_t n, const char *fourcc, unsigned *clen) {
    unsigned cnt = *(const unsigned *)(b + 28);
    for (unsigned i = 0; i < cnt; ++i) {
        unsigned o = *(const unsigned *)(b + 32 + 4 * i);
        if (o + 8 <= n && !memcmp(b + o, fourcc, 4)) { *clen = *(const unsigned *)(b + o + 4); return b + o + 8; }
    }
    return NULL;
}
static int prog_type(const unsigned char *b, size_t n) { /* 0 PS, 1 VS */
    unsigned l; const unsigned char *s = chunk(b, n, "SHDR", &l); if (!s) s = chunk(b, n, "SHEX", &l);
    if (!s) return -1; return (int)((*(const unsigned *)s) >> 16);
}
/* build an input layout from the ISGN of a VS (float formats by mask width) */
static HRESULT make_layout(const unsigned char *vs, size_t n, const void *bytecode, size_t bclen) {
    unsigned l; const unsigned char *s = chunk(vs, n, "ISGN", &l); if (!s) return E_FAIL;
    unsigned cnt = *(const unsigned *)s; D3D11_INPUT_ELEMENT_DESC el[16]; unsigned k = 0;
    for (unsigned i = 0; i < cnt && k < 16; ++i) {
        const unsigned *e = (const unsigned *)(s + 8 + 24 * i);
        const char *name = (const char *)(s + e[0]);
        unsigned sysval = e[2], comptype = e[3], mask = e[5] & 0xff;
        if (sysval != 0) continue; /* SV_VertexID etc. */
        int w = (mask & 8) ? 4 : (mask & 4) ? 3 : (mask & 2) ? 2 : 1;
        DXGI_FORMAT f = comptype == 3 ? (w == 4 ? DXGI_FORMAT_R32G32B32A32_FLOAT : w == 3 ? DXGI_FORMAT_R32G32B32_FLOAT : w == 2 ? DXGI_FORMAT_R32G32_FLOAT : DXGI_FORMAT_R32_FLOAT)
                                       : DXGI_FORMAT_R8G8B8A8_UNORM;
        if (!strcmp(name, "COLOR")) f = DXGI_FORMAT_R8G8B8A8_UNORM; /* Unity sends Color32 */
        el[k].SemanticName = name; el[k].SemanticIndex = e[1]; el[k].Format = f; el[k].InputSlot = 0;
        el[k].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT; el[k].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA; el[k].InstanceDataStepRate = 0; k++;
    }
    ID3D11InputLayout *il = NULL; HRESULT hr = ID3D11Device_CreateInputLayout(dev, el, k, bytecode, bclen, &il);
    if (il) ID3D11InputLayout_Release(il);
    return hr;
}
static int is_needed(const char *fname, char needed[][13], int nneeded) {
    const char *u = strrchr(fname, '_'); if (!u) return 0; u++;
    for (int i = 0; i < nneeded; ++i) if (!strncmp(u, needed[i], 12)) return 1;
    return 0;
}


/* draw test: Unity's Sprites/Default VS+PS go through the checksum lookup like the wrapper would, then a quad is drawn and read back */
static void draw_test(const char *origdir) {
    char p1[MAX_PATH], p2[MAX_PATH]; size_t n1, n2;
    sprintf(p1, "%s\\00_Sprites_Default_b000_vs_756ae2904e92.dxbc", origdir);
    sprintf(p2, "%s\\00_Sprites_Default_b004_ps_3aa3dabc65f1.dxbc", origdir);
    unsigned char *ovs = readall(p1, &n1), *ops = readall(p2, &n2);
    if (!ovs || !ops) { printf("  orig blobs missing\n"); return; }
    unsigned vsz, psz; const unsigned char *vs = lookup(ovs + 4, &vsz), *ps = lookup(ops + 4, &psz);
    printf("  lookup Sprites/Default VS %s, PS %s\n", vs ? "hit" : "MISS", ps ? "hit" : "MISS");
    if (!vs || !ps) return;
    ID3D11VertexShader *v = NULL; ID3D11PixelShader *f = NULL; ID3D11InputLayout *il = NULL;
    rep("CreateVertexShader(replacement)", ID3D11Device_CreateVertexShader(dev, vs, vsz, NULL, &v), 1);
    rep("CreatePixelShader(replacement)", ID3D11Device_CreatePixelShader(dev, ps, psz, NULL, &f), 1);
    D3D11_INPUT_ELEMENT_DESC el[3] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    rep("CreateInputLayout(Unity-style float3/Color32/float2, replacement VS)", ID3D11Device_CreateInputLayout(dev, el, 3, vs, vsz, &il), 1);
    struct { float x, y, z; unsigned c; float u, v; } q[4] = {
        { -1, -1, 0.5f, 0xffffffff, 0, 1 }, { -1, 1, 0.5f, 0xffffffff, 0, 0 }, { 1, 1, 0.5f, 0xffffffff, 1, 0 }, { 1, -1, 0.5f, 0xffffffff, 1, 1 } };
    unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };
    D3D11_BUFFER_DESC bd = {0}; D3D11_SUBRESOURCE_DATA sd = {0}; ID3D11Buffer *vb = NULL, *ib = NULL, *cb[4] = {0};
    bd.ByteWidth = sizeof q; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; sd.pSysMem = q; ID3D11Device_CreateBuffer(dev, &bd, &sd, &vb);
    bd.ByteWidth = sizeof idx; bd.BindFlags = D3D11_BIND_INDEX_BUFFER; sd.pSysMem = idx; ID3D11Device_CreateBuffer(dev, &bd, &sd, &ib);
    static float g[12], draw[44], frame[92], spr[8];
    g[8] = g[9] = g[10] = g[11] = 1;                                     /* $Globals _Color @32 */
    draw[0] = draw[5] = draw[10] = draw[15] = 1;                         /* unity_ObjectToWorld = I */
    frame[68] = frame[73] = frame[78] = frame[83] = 1;                   /* unity_MatrixVP @272 = I */
    spr[0] = spr[1] = spr[2] = spr[3] = 1; spr[4] = spr[5] = 1;          /* _RendererColor, _Flip */
    unsigned sizes[4] = { 48, 176, 368, 32 }; float *data[4] = { g, draw, frame, spr };
    for (int i = 0; i < 4; ++i) { bd.ByteWidth = sizes[i]; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; sd.pSysMem = data[i]; ID3D11Device_CreateBuffer(dev, &bd, &sd, &cb[i]); }
    unsigned texel[16]; for (int i = 0; i < 16; ++i) texel[i] = 0xff3264c8u;  /* R=200 G=100 B=50 A=255 */
    ID3D11Texture2D *tex = NULL, *rt = NULL, *ds = NULL, *st = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11DepthStencilView *dsv = NULL;
    D3D11_TEXTURE2D_DESC td = {0}; td.Width = td.Height = 4; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sd.pSysMem = texel; sd.SysMemPitch = 16; ID3D11Device_CreateTexture2D(dev, &td, &sd, &tex); ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, NULL, &srv);
    td.Width = td.Height = 64; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; ID3D11Device_CreateTexture2D(dev, &td, NULL, &rt); ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)rt, NULL, &rtv);
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL; ID3D11Device_CreateTexture2D(dev, &td, NULL, &ds); ID3D11Device_CreateDepthStencilView(dev, (ID3D11Resource *)ds, NULL, &dsv);
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ; ID3D11Device_CreateTexture2D(dev, &td, NULL, &st);
    D3D11_SAMPLER_DESC smp = {0}; smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; smp.MaxAnisotropy = 1; smp.MinLOD = -FLT_MAX; smp.MaxLOD = FLT_MAX; ID3D11SamplerState *ss = NULL; ID3D11Device_CreateSamplerState(dev, &smp, &ss);
    D3D11_DEPTH_STENCIL_DESC dsd = {0}; dsd.DepthEnable = TRUE; dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dsd.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; ID3D11DepthStencilState *dss = NULL; ID3D11Device_CreateDepthStencilState(dev, &dsd, &dss);
    D3D11_RASTERIZER_DESC rd = {0}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE; ID3D11RasterizerState *rs = NULL; ID3D11Device_CreateRasterizerState(dev, &rd, &rs);
    float black[4] = { 0, 0, 0, 0 }; ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, black); ID3D11DeviceContext_ClearDepthStencilView(ctx, dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0); /* reversed Z clear 0 */
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, dsv); ID3D11DeviceContext_OMSetDepthStencilState(ctx, dss, 0); ID3D11DeviceContext_RSSetState(ctx, rs);
    D3D11_VIEWPORT vp = { 0, 0, 64, 64, 0, 1 }; ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
    UINT stride = 24, off = 0; ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vb, &stride, &off); ID3D11DeviceContext_IASetIndexBuffer(ctx, ib, DXGI_FORMAT_R16_UINT, 0);
    ID3D11DeviceContext_IASetInputLayout(ctx, il); ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(ctx, v, NULL, 0); ID3D11DeviceContext_PSSetShader(ctx, f, NULL, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 4, cb); ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv); ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &ss);
    { ID3D11ShaderResourceView *nul[1] = { NULL }; ID3D11SamplerState *nuls[1] = { NULL };
      ID3D11DeviceContext_VSSetShaderResources(ctx, 0, 1, nul); rep("VSSetShaderResources(NULL) - unbinding, as engines do", S_OK, -1);
      ID3D11DeviceContext_VSSetSamplers(ctx, 0, 1, nuls); rep("VSSetSamplers(NULL)", S_OK, -1); }
    ID3D11DeviceContext_DrawIndexed(ctx, 6, 0, 0); rep("DrawIndexed(6) with replacement Sprites/Default", S_OK, -1);
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)st, (ID3D11Resource *)rt);
    D3D11_MAPPED_SUBRESOURCE m; HRESULT hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m);
    if (SUCCEEDED(hr)) { unsigned px = ((unsigned *)((unsigned char *)m.pData + 32 * m.RowPitch))[32]; ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0);
        int r = px & 0xff, gg = (px >> 8) & 0xff, b = (px >> 16) & 0xff, a = px >> 24;
        printf("  read-back centre pixel RGBA = %d %d %d %d (expected 200 100 50 255)\n", r, gg, b, a);
        if (abs(r - 200) <= 2 && abs(gg - 100) <= 2 && abs(b - 50) <= 2 && a == 255) g_pass++; else g_fail++; }
    else rep("Map staging", hr, 1);
    rep("GetDeviceRemovedReason after draw", ID3D11Device_GetDeviceRemovedReason(dev), 1);
}

int main(int argc, char **argv) {
    if (argc < 4) { printf("usage\n"); return 2; }
    int warp = argc > 4 && !strcmp(argv[4], "warp");
    D3D_FEATURE_LEVEL want = D3D_FEATURE_LEVEL_9_1, got = 0;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | (getenv("NODEBUG") ? 0 : D3D11_CREATE_DEVICE_DEBUG);
    D3D_DRIVER_TYPE dt = warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    HRESULT hr = D3D11CreateDevice(NULL, dt, NULL, flags, &want, 1, D3D11_SDK_VERSION, &dev, &got, &ctx);
    int debug = 1;
    if (FAILED(hr)) { debug = 0; flags &= ~D3D11_CREATE_DEVICE_DEBUG; hr = D3D11CreateDevice(NULL, dt, NULL, flags, &want, 1, D3D11_SDK_VERSION, &dev, &got, &ctx); }
    printf("device: driver=%s hr=0x%08lx FL=0x%x debug-layer=%d GetFeatureLevel=0x%x\n", warp ? "WARP" : "HARDWARE", (unsigned long)hr, got, debug, dev ? ID3D11Device_GetFeatureLevel(dev) : 0);
    if (FAILED(hr)) return 1;
    if (debug && SUCCEEDED(ID3D11Device_QueryInterface(dev, &IID_ID3D11InfoQueue, (void **)&iq))) {}
    { IDXGIDevice *dd; IDXGIAdapter *ad; DXGI_ADAPTER_DESC ds;
      if (SUCCEEDED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd)) && SUCCEEDED(IDXGIDevice_GetAdapter(dd, &ad)) && SUCCEEDED(IDXGIAdapter_GetDesc(ad, &ds)))
          printf("adapter: \"%ls\" vendor=%04x device=%04x dedicated=%llu MiB\n", ds.Description, ds.VendorId, ds.DeviceId, (unsigned long long)(ds.DedicatedVideoMemory >> 20)); }

    printf("\n[A] interfaces Unity requires (sub_ffdc74)\n");
    { void *p = NULL; rep("QI ID3D11Device1", ID3D11Device_QueryInterface(dev, &IID_ID3D11Device1, &p), 1); if (p) IUnknown_Release((IUnknown *)p);
      p = NULL; rep("QI ID3D11Device2", ID3D11Device_QueryInterface(dev, &IID_ID3D11Device2, &p), -1); if (p) IUnknown_Release((IUnknown *)p);
      p = NULL; rep("QI IDXGIDevice3", ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice3, &p), 1); if (p) IUnknown_Release((IUnknown *)p); }

    { void *d1 = NULL, *d2 = NULL; ID3D11Device_QueryInterface(dev, &IID_ID3D11Device1, &d1); ID3D11Device_QueryInterface(dev, &IID_ID3D11Device2, &d2);
      printf("  identity: device=%p vtbl=%p | Device1=%p vtbl=%p | Device2=%p vtbl=%p\n", (void *)dev, *(void **)dev, d1, d1 ? *(void **)d1 : NULL, d2, d2 ? *(void **)d2 : NULL);
      MEMORY_BASIC_INFORMATION mb; VirtualQuery(*(void **)dev, &mb, sizeof mb); printf("  device vtable page protect=0x%lx type=0x%lx\n", mb.Protect, mb.Type);
      void *cv = *(void **)ctx; VirtualQuery(cv, &mb, sizeof mb); printf("  context vtable=%p page protect=0x%lx type=0x%lx\n", cv, mb.Protect, mb.Type);
      if (d1) IUnknown_Release((IUnknown *)d1); if (d2) IUnknown_Release((IUnknown *)d2); }
    printf("\n[B] feature data at 9_1\n");
    { D3D11_FEATURE_DATA_D3D9_OPTIONS o = {0}; hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D9_OPTIONS, &o, sizeof o);
      printf("  D3D9_OPTIONS hr=0x%08lx FullNonPow2TextureSupport=%d\n", (unsigned long)hr, o.FullNonPow2TextureSupport);
      D3D11_FEATURE_DATA_D3D11_OPTIONS o1 = {0}; hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D11_OPTIONS, &o1, sizeof o1);
      printf("  D3D11_OPTIONS hr=0x%08lx OutputMergerLogicOp=%d ConstantBufferOffsetting=%d ConstantBufferPartialUpdate=%d MapNoOverwriteOnDynamicConstantBuffer=%d MapNoOverwriteOnDynamicBufferSRV=%d\n", (unsigned long)hr,
             o1.OutputMergerLogicOp, o1.ConstantBufferOffsetting, o1.ConstantBufferPartialUpdate, o1.MapNoOverwriteOnDynamicConstantBuffer, o1.MapNoOverwriteOnDynamicBufferSRV);
      D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT sh = {0}; hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D9_SHADOW_SUPPORT, &sh, sizeof sh);
      printf("  D3D9_SHADOW_SUPPORT hr=0x%08lx SupportsDepthAsTextureWithLessEqualComparisonFilter=%d\n", (unsigned long)hr, sh.SupportsDepthAsTextureWithLessEqualComparisonFilter);
      D3D11_FEATURE_DATA_D3D9_SIMPLE_INSTANCING_SUPPORT si = {0}; hr = ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT, &si, sizeof si);
      printf("  D3D9_SIMPLE_INSTANCING hr=0x%08lx SimpleInstancingSupported=%d\n", (unsigned long)hr, si.SimpleInstancingSupported);
      UINT fs = 0;
      DXGI_FORMAT probe[] = { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R16_UINT };
      const char *pn[] = { "R8G8B8A8_TYPELESS", "R8G8B8A8_UNORM", "B8G8R8A8_UNORM", "B8G8R8A8_TYPELESS", "R32G8X24_TYPELESS", "D32_FLOAT_S8X24_UINT", "D24_UNORM_S8_UINT", "R24G8_TYPELESS", "D16_UNORM", "R16_TYPELESS", "A8_UNORM", "B4G4R4A4_UNORM", "BC1_UNORM", "BC3_UNORM", "R16G16B16A16_FLOAT", "R32_UINT", "R16_UINT" };
      for (int i = 0; i < (int)(sizeof probe / sizeof probe[0]); ++i) { fs = 0; hr = ID3D11Device_CheckFormatSupport(dev, probe[i], &fs);
          printf("  CheckFormatSupport %-22s hr=0x%08lx tex2d=%d rt=%d ds=%d sample=%d mip=%d ib=%d vb=%d\n", pn[i], (unsigned long)hr, !!(fs & D3D11_FORMAT_SUPPORT_TEXTURE2D), !!(fs & D3D11_FORMAT_SUPPORT_RENDER_TARGET), !!(fs & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL), !!(fs & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE), !!(fs & D3D11_FORMAT_SUPPORT_MIP), !!(fs & D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER), !!(fs & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER)); }
      drain("format"); }

    printf("\n[C] textures Unity creates (hw-unitygfx F6/F7, hw-content F4/F5)\n");
    rep("depth R32G8X24_TYPELESS DS|SR (Unity 24-bit depth, sub_1006980)", tex2d(1366, 768, 1, DXGI_FORMAT_R32G8X24_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, NULL), 0);
    rep("depth R32G8X24_TYPELESS DS only", tex2d(1366, 768, 1, DXGI_FORMAT_R32G8X24_TYPELESS, D3D11_BIND_DEPTH_STENCIL, NULL), 0);
    rep("depth D24_UNORM_S8_UINT DS (fix)", tex2d(1366, 768, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL, NULL), 1);
    rep("depth R24G8_TYPELESS DS|SR", tex2d(1366, 768, 1, DXGI_FORMAT_R24G8_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("depth D24_UNORM_S8_UINT DS|SR", tex2d(1366, 768, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("depth R16_TYPELESS DS|SR (16-bit branch)", tex2d(1366, 768, 1, DXGI_FORMAT_R16_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("colour RT R8G8B8A8_TYPELESS RT|SR (sub_1007a58 typeless column)", tex2d(1024, 1024, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("colour RT B8G8R8A8_TYPELESS RT|SR", tex2d(1024, 1024, 1, DXGI_FORMAT_B8G8R8A8_TYPELESS, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("colour RT R8G8B8A8_UNORM RT|SR (fix)", tex2d(1024, 1024, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, NULL), 1);
    { ID3D11Texture2D *t = NULL; HRESULT h1 = tex2d(256, 256, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, &t);
      if (t) { ID3D11RenderTargetView *rtv = NULL; D3D11_RENDER_TARGET_VIEW_DESC rd = {0}; rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
               rep("  RTV R8G8B8A8_UNORM on the typeless texture", ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)t, &rd, &rtv), -1); if (rtv) ID3D11RenderTargetView_Release(rtv);
               ID3D11ShaderResourceView *srv = NULL; D3D11_SHADER_RESOURCE_VIEW_DESC sd = {0}; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
               rep("  SRV R8G8B8A8_UNORM on the typeless texture", ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)t, &sd, &srv), -1); if (srv) ID3D11ShaderResourceView_Release(srv);
               ID3D11Texture2D_Release(t); } (void)h1; }
    rep("tex 2160x1080 RGBA8 1 mip (Innovation_VersionSelector_Background)", tex2d(2160, 1080, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("tex 2048x2048 BC3 1 mip (atlases)", tex2d(2048, 2048, 1, DXGI_FORMAT_BC3_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), 1);
    rep("tex 4096x2048 RGBA8 12 mips (NPOT round-up of 2160x1080)", tex2d(4096, 2048, 12, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("tex 124x124 RGBA8 7 mips ('loading', NPOT+mips)", tex2d(124, 124, 7, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("tex 124x124 RGBA8 1 mip", tex2d(124, 124, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), 1);
    rep("tex 128x128 RGBA8 8 mips (POT round-up)", tex2d(128, 128, 8, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), 1);
    rep("tex 1024x652 RGBA8 11 mips ('Comic_DailyChallenge')", tex2d(1024, 652, 11, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("tex 36x16 BC3 1 mip (35x16 DXT5 rounded to block size)", tex2d(36, 16, 1, DXGI_FORMAT_BC3_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("tex 200x200 BC3 1 mip (UI buttons)", tex2d(200, 200, 1, DXGI_FORMAT_BC3_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("tex 128x128 A8_UNORM 8 mips (built-in 'Soft')", tex2d(128, 128, 8, DXGI_FORMAT_A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("tex 64x64 B4G4R4A4 1 mip (IMGUI skin)", tex2d(64, 64, 1, DXGI_FORMAT_B4G4R4A4_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);
    rep("tex 1024x1024 A8_UNORM 1 mip (SDF atlases)", tex2d(1024, 1024, 1, DXGI_FORMAT_A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), -1);

    printf("\n[D] swap chain like Unity's (sub_2f458c desc, but HWND)\n");
    { WNDCLASSA wc = {0}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "l9bench"; RegisterClassA(&wc);
      HWND w = CreateWindowA("l9bench", "l9bench", WS_OVERLAPPEDWINDOW, 0, 0, 640, 360, NULL, NULL, NULL, NULL);
      IDXGIDevice *dd = NULL; IDXGIAdapter *ad = NULL; IDXGIFactory2 *fa = NULL;
      ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dd); IDXGIDevice_GetAdapter(dd, &ad); IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory2, (void **)&fa);
      UINT usages[2] = { 0x30, 0x20 };
      for (int i = 0; i < 2; ++i) {
          DXGI_SWAP_CHAIN_DESC1 d = {0}; d.Width = 640; d.Height = 360; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1; d.BufferUsage = usages[i];
          d.BufferCount = 2; d.Scaling = DXGI_SCALING_STRETCH; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; d.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
          IDXGISwapChain1 *sc = NULL; char nm[128]; sprintf(nm, "CreateSwapChainForHwnd usage=0x%x FLIP_SEQUENTIAL flags=0x40", usages[i]);
          hr = IDXGIFactory2_CreateSwapChainForHwnd(fa, (IUnknown *)dev, w, &d, NULL, NULL, &sc); rep(nm, hr, -1);
          if (sc) { ID3D11Texture2D *bb = NULL; IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
                    if (bb) { D3D11_TEXTURE2D_DESC bd; ID3D11Texture2D_GetDesc(bb, &bd); printf("      back buffer bind=0x%x\n", bd.BindFlags);
                              ID3D11ShaderResourceView *srv = NULL; rep("      CreateShaderResourceView(back buffer)", ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)bb, NULL, &srv), -1); if (srv) ID3D11ShaderResourceView_Release(srv);
                              ID3D11RenderTargetView *rtv = NULL; rep("      CreateRenderTargetView(back buffer)", ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv), 1); if (rtv) ID3D11RenderTargetView_Release(rtv);
                              ID3D11Texture2D_Release(bb); }
                    rep("      Present(1,0)", IDXGISwapChain1_Present(sc, 1, 0), 1);
                    IDXGISwapChain1_Release(sc); }
      }
      DestroyWindow(w); }

    printf("\n[E] state objects (hw-unitygfx F9)\n");
    { D3D11_SAMPLER_DESC s = {0}; ID3D11SamplerState *ss = NULL;
      s.Filter = D3D11_FILTER_ANISOTROPIC; s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_WRAP; s.MaxAnisotropy = 16; s.MinLOD = -FLT_MAX; s.MaxLOD = FLT_MAX; s.ComparisonFunc = D3D11_COMPARISON_NEVER;
      rep("sampler ANISOTROPIC MaxAnisotropy=16", ID3D11Device_CreateSamplerState(dev, &s, &ss), 0); if (ss) ID3D11SamplerState_Release(ss), ss = NULL;
      s.MaxAnisotropy = 2; rep("sampler ANISOTROPIC MaxAnisotropy=2", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1); if (ss) ID3D11SamplerState_Release(ss), ss = NULL;
      s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; s.MaxAnisotropy = 16; rep("sampler LINEAR with MaxAnisotropy=16 (field ignored?)", ID3D11Device_CreateSamplerState(dev, &s, &ss), -1); if (ss) ID3D11SamplerState_Release(ss), ss = NULL;
      s.MaxAnisotropy = 1; rep("sampler LINEAR MinLOD=-FLT_MAX MaxLOD=FLT_MAX (Unity default)", ID3D11Device_CreateSamplerState(dev, &s, &ss), 1); if (ss) ID3D11SamplerState_Release(ss), ss = NULL;
      s.MipLODBias = -0.5f; rep("sampler LINEAR MipLODBias=-0.5", ID3D11Device_CreateSamplerState(dev, &s, &ss), -1); if (ss) ID3D11SamplerState_Release(ss), ss = NULL; s.MipLODBias = 0;
      s.MaxAnisotropy = 0; rep("sampler LINEAR MaxAnisotropy=0", ID3D11Device_CreateSamplerState(dev, &s, &ss), -1); if (ss) ID3D11SamplerState_Release(ss), ss = NULL; s.MaxAnisotropy = 1;
      s.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE; rep("sampler MIRROR_ONCE", ID3D11Device_CreateSamplerState(dev, &s, &ss), 0); if (ss) ID3D11SamplerState_Release(ss), ss = NULL; s.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
      s.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; s.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; rep("sampler comparison (shadow)", ID3D11Device_CreateSamplerState(dev, &s, &ss), -1); if (ss) ID3D11SamplerState_Release(ss), ss = NULL; }
    { D3D11_RASTERIZER_DESC r = {0}; ID3D11RasterizerState *rs = NULL; r.FillMode = D3D11_FILL_SOLID; r.CullMode = D3D11_CULL_BACK; r.DepthClipEnable = FALSE;
      rep("rasterizer DepthClipEnable=FALSE ('ZClip False')", ID3D11Device_CreateRasterizerState(dev, &r, &rs), 0); if (rs) ID3D11RasterizerState_Release(rs), rs = NULL;
      r.DepthClipEnable = TRUE; r.ScissorEnable = TRUE; r.SlopeScaledDepthBias = 1.0f; r.DepthBias = 1; rep("rasterizer DepthClip=TRUE scissor, depth bias", ID3D11Device_CreateRasterizerState(dev, &r, &rs), 1); if (rs) ID3D11RasterizerState_Release(rs), rs = NULL; }
    { D3D11_BLEND_DESC b = {0}; ID3D11BlendState *bs = NULL;
      b.RenderTarget[0].BlendEnable = TRUE; b.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; b.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; b.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
      b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA; b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; b.RenderTarget[0].RenderTargetWriteMask = 0xf;
      rep("blend SrcAlpha/InvSrcAlpha, alpha = colour factors", ID3D11Device_CreateBlendState(dev, &b, &bs), 1); if (bs) ID3D11BlendState_Release(bs), bs = NULL;
      b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
      rep("blend separate alpha factors (IMGUI: One,One)", ID3D11Device_CreateBlendState(dev, &b, &bs), -1); if (bs) ID3D11BlendState_Release(bs), bs = NULL;
      b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA; b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
      rep("blend separate alpha op (BlendOpAlpha=MAX)", ID3D11Device_CreateBlendState(dev, &b, &bs), -1); if (bs) ID3D11BlendState_Release(bs), bs = NULL; b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
      b.AlphaToCoverageEnable = TRUE; rep("blend AlphaToCoverageEnable=TRUE", ID3D11Device_CreateBlendState(dev, &b, &bs), 0); if (bs) ID3D11BlendState_Release(bs), bs = NULL; b.AlphaToCoverageEnable = FALSE;
      b.RenderTarget[0].RenderTargetWriteMask = 0xe; rep("blend colour mask 14 (RGB)", ID3D11Device_CreateBlendState(dev, &b, &bs), 1); if (bs) ID3D11BlendState_Release(bs), bs = NULL;
      b.RenderTarget[0].RenderTargetWriteMask = 0; rep("blend colour mask 0", ID3D11Device_CreateBlendState(dev, &b, &bs), 1); if (bs) ID3D11BlendState_Release(bs), bs = NULL;
      { ID3D11Device1 *d1 = NULL; ID3D11Device_QueryInterface(dev, &IID_ID3D11Device1, (void **)&d1);
        if (d1) { D3D11_BLEND_DESC1 b1 = {0}; ID3D11BlendState1 *bs1 = NULL; b1.RenderTarget[0].LogicOpEnable = FALSE; b1.RenderTarget[0].BlendEnable = TRUE;
                  b1.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; b1.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO; b1.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; b1.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; b1.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; b1.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; b1.RenderTarget[0].RenderTargetWriteMask = 0xf;
                  rep("CreateBlendState1 (no logic op)", ID3D11Device1_CreateBlendState1(d1, &b1, &bs1), -1); if (bs1) ID3D11BlendState1_Release(bs1);
                  ID3D11Device1_Release(d1); } } }
    { D3D11_DEPTH_STENCIL_DESC d = {0}; ID3D11DepthStencilState *ds = NULL; d.DepthEnable = TRUE; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
      d.StencilEnable = TRUE; d.StencilReadMask = d.StencilWriteMask = 0xff;
      d.FrontFace.StencilFailOp = d.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP; d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE; d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS; d.BackFace = d.FrontFace;
      rep("depth-stencil front == back", ID3D11Device_CreateDepthStencilState(dev, &d, &ds), 1); if (ds) ID3D11DepthStencilState_Release(ds), ds = NULL;
      d.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO; rep("depth-stencil front != back (two-sided)", ID3D11Device_CreateDepthStencilState(dev, &d, &ds), -1); if (ds) ID3D11DepthStencilState_Release(ds), ds = NULL; }
    { D3D11_QUERY_DESC q = {0}; ID3D11Query *qq = NULL;
      q.Query = D3D11_QUERY_TIMESTAMP_DISJOINT; rep("query TIMESTAMP_DISJOINT (optional at 9_1)", ID3D11Device_CreateQuery(dev, &q, &qq), -1); if (qq) ID3D11Query_Release(qq), qq = NULL;
      q.Query = D3D11_QUERY_EVENT; rep("query EVENT", ID3D11Device_CreateQuery(dev, &q, &qq), 1); if (qq) ID3D11Query_Release(qq), qq = NULL;
      q.Query = D3D11_QUERY_OCCLUSION; rep("query OCCLUSION", ID3D11Device_CreateQuery(dev, &q, &qq), -1); if (qq) ID3D11Query_Release(qq), qq = NULL; }
    { D3D11_BUFFER_DESC bd = {0}; ID3D11Buffer *b = NULL; bd.ByteWidth = 4096; bd.Usage = D3D11_USAGE_DYNAMIC; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
      rep("dynamic index buffer", ID3D11Device_CreateBuffer(dev, &bd, NULL, &b), 1);
      if (b) { ID3D11DeviceContext_IASetIndexBuffer(ctx, b, DXGI_FORMAT_R16_UINT, 0); rep("IASetIndexBuffer R16_UINT", S_OK, -1); g_ib = b; b = NULL; }
      bd.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER; rep("buffer VB|IB combined", ID3D11Device_CreateBuffer(dev, &bd, NULL, &b), 0); if (b) ID3D11Buffer_Release(b), b = NULL;
      bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; bd.ByteWidth = 368; rep("constant buffer 368 bytes (UnityPerFrame)", ID3D11Device_CreateBuffer(dev, &bd, NULL, &b), 1); if (b) ID3D11Buffer_Release(b), b = NULL; }

    printf("\n[F] shaders: Unity originals vs the level_9_1 table\n");
    { size_t n; tbl = readall(argv[1], &n); if (!tbl || memcmp(tbl, "L9SW", 4)) { printf("  table missing\n"); }
      else { nents = *(unsigned *)(tbl + 8); ents = (Ent *)(tbl + 12); printf("  table: %u entries\n", nents);
        int tvs = 0, tps = 0, tok = 0, tbad = 0;
        for (unsigned i = 0; i < nents; ++i) { const unsigned char *b = tbl + ents[i].off; int pt = prog_type(b, ents[i].size); HRESULT h;
            if (pt == 1) { ID3D11VertexShader *v = NULL; h = ID3D11Device_CreateVertexShader(dev, b, ents[i].size, NULL, &v); if (v) ID3D11VertexShader_Release(v); tvs++; }
            else { ID3D11PixelShader *p = NULL; h = ID3D11Device_CreatePixelShader(dev, b, ents[i].size, NULL, &p); if (p) ID3D11PixelShader_Release(p); tps++; }
            if (SUCCEEDED(h)) tok++; else { tbad++; printf("  table entry %u (type %d) FAILED hr=0x%08lx\n", i, pt, (unsigned long)h); }
            drain("table"); }
        printf("  table entries created directly: %d VS + %d PS, OK %d, failed %d\n", tvs, tps, tok, tbad);
        /* walk Unity originals */
        char needed[512][13]; int nneeded = 0; FILE *nf = fopen(argv[3], "r"); char line[64];
        while (nf && nneeded < 512 && fgets(line, sizeof line, nf)) { if (strlen(line) >= 12) { memcpy(needed[nneeded], line, 12); needed[nneeded][12] = 0; nneeded++; } }
        if (nf) fclose(nf);
        char pat[MAX_PATH]; sprintf(pat, "%s\\*", argv[2]); WIN32_FIND_DATAA fd; HANDLE hd = FindFirstFileA(pat, &fd);
        int orig = 0, origok = 0, hit = 0, hitok = 0, ilok = 0, ilbad = 0, nd = 0, ndhit = 0, ndok = 0; unsigned char seen[2048][16]; int nseen = 0;
        while (hd != INVALID_HANDLE_VALUE) {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.') {
                char p2[MAX_PATH]; sprintf(p2, "%s\\%s\\*.dxbc", argv[2], fd.cFileName); WIN32_FIND_DATAA f2; HANDLE h2 = FindFirstFileA(p2, &f2);
                while (h2 != INVALID_HANDLE_VALUE) {
                    char fp[MAX_PATH]; sprintf(fp, "%s\\%s\\%s", argv[2], fd.cFileName, f2.cFileName); size_t bn; unsigned char *b = readall(fp, &bn);
                    int dup = 0; for (int k = 0; k < nseen; ++k) if (!memcmp(seen[k], b + 4, 16)) { dup = 1; break; }
                    if (!dup && nseen < 2048) { memcpy(seen[nseen++], b + 4, 16);
                        int pt = prog_type(b, bn); HRESULT h; int need = is_needed(f2.cFileName, needed, nneeded);
                        if (pt == 1) { ID3D11VertexShader *v = NULL; h = ID3D11Device_CreateVertexShader(dev, b, bn, NULL, &v); if (v) ID3D11VertexShader_Release(v); }
                        else { ID3D11PixelShader *p = NULL; h = ID3D11Device_CreatePixelShader(dev, b, bn, NULL, &p); if (p) ID3D11PixelShader_Release(p); }
                        orig++; if (SUCCEEDED(h)) origok++; if (iq) ID3D11InfoQueue_ClearStoredMessages(iq);
                        if (need) nd++;
                        unsigned rs; const unsigned char *r = lookup(b + 4, &rs);
                        if (r) { hit++; if (need) ndhit++;
                            if (pt == 1) { ID3D11VertexShader *v = NULL; h = ID3D11Device_CreateVertexShader(dev, r, rs, NULL, &v); if (v) ID3D11VertexShader_Release(v);
                                           HRESULT hl = make_layout(b, bn, r, rs); if (SUCCEEDED(hl)) ilok++; else { ilbad++; printf("  input layout FAILED for %s hr=0x%08lx\n", f2.cFileName, (unsigned long)hl); } }
                            else { ID3D11PixelShader *p = NULL; h = ID3D11Device_CreatePixelShader(dev, r, rs, NULL, &p); if (p) ID3D11PixelShader_Release(p); }
                            if (SUCCEEDED(h)) { hitok++; if (need) ndok++; } else printf("  replacement FAILED for %s hr=0x%08lx\n", f2.cFileName, (unsigned long)h);
                            drain("repl"); }
                        else if (need) printf("  needed program without replacement: %s\\%s\n", fd.cFileName, f2.cFileName);
                    }
                    free(b);
                    if (!FindNextFileA(h2, &f2)) { FindClose(h2); break; }
                }
            }
            if (!FindNextFileA(hd, &fd)) { FindClose(hd); break; }
        }
        printf("  unique Unity programs: %d; created as-is on the 9_1 device: %d (expect 0)\n", orig, origok);
        printf("  table hits: %d, replacement created OK: %d; input layouts from Unity's ISGN + l9 VS: OK %d, failed %d\n", hit, hitok, ilok, ilbad);
        printf("  needed programs (index.csv): %d, with replacement: %d, created OK: %d\n", nd, ndhit, ndok);
        if (origok == 0) g_pass++; else g_fail++;
        if (nd == ndhit && ndhit == ndok) g_pass++; else g_fail++;
      } }
    printf("\n[F2] draw test through the lookup (Sprites/Default)\n");
    if (nents) { char od[MAX_PATH]; sprintf(od, "%s", argc > 5 ? argv[5] : "."); draw_test(od); }
    printf("\n[G] LAST: IASetIndexBuffer with R32_UINT (Unity Supports32bitsIndexBuffer=1)\n");
    rep("GetDeviceRemovedReason before", ID3D11Device_GetDeviceRemovedReason(dev), 1);
    if (g_ib) { ID3D11DeviceContext_IASetIndexBuffer(ctx, g_ib, DXGI_FORMAT_R32_UINT, 0); rep("IASetIndexBuffer R32_UINT (void call)", S_OK, -1); ID3D11DeviceContext_Flush(ctx); }
    rep("GetDeviceRemovedReason after", ID3D11Device_GetDeviceRemovedReason(dev), -1);
    printf("\nSUMMARY: %d as-expected, %d unexpected\n", g_pass, g_fail);
    return 0;
}
