/* crit_il.c - critic test: Unity 2018.4 creates input layouts from a SIGNATURE-ONLY DXBC (sub_100fa88 -> sub_fff01c:
   new container 'DXBC' + fresh checksum + 1 chunk ISGN). Does a FL 9_1 device accept that, and does a draw work with it?
   Usage: crit_il.exe <l9_replacements.bin> <orig dir> [warp] */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11_2.h>
#include <d3d11sdklayers.h>
#include <d3dcommon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

static ID3D11Device *dev; static ID3D11DeviceContext *ctx; static ID3D11InfoQueue *iq;
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
static void rep(const char *name, HRESULT hr) { printf("  %-80s hr=0x%08lx\n", name, (unsigned long)hr); drain(name); }
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
typedef HRESULT (WINAPI *PFN_GETSIG)(LPCVOID, SIZE_T, ID3DBlob **);

int main(int argc, char **argv) {
    if (argc < 3) { printf("usage\n"); return 2; }
    int warp = argc > 3 && !strcmp(argv[3], "warp");
    D3D_FEATURE_LEVEL want = D3D_FEATURE_LEVEL_9_1, got = 0;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
    D3D_DRIVER_TYPE dt = warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    HRESULT hr = D3D11CreateDevice(NULL, dt, NULL, flags, &want, 1, D3D11_SDK_VERSION, &dev, &got, &ctx);
    if (FAILED(hr)) { flags &= ~D3D11_CREATE_DEVICE_DEBUG; hr = D3D11CreateDevice(NULL, dt, NULL, flags, &want, 1, D3D11_SDK_VERSION, &dev, &got, &ctx); }
    printf("device: %s hr=0x%08lx FL=0x%x debug=%d\n", warp ? "WARP" : "HARDWARE", (unsigned long)hr, got, !!(flags & D3D11_CREATE_DEVICE_DEBUG));
    if (FAILED(hr)) return 1;
    ID3D11Device_QueryInterface(dev, &IID_ID3D11InfoQueue, (void **)&iq);
    size_t n; tbl = readall(argv[1], &n); nents = *(unsigned *)(tbl + 8); ents = (Ent *)(tbl + 12);
    char p1[MAX_PATH], p2[MAX_PATH]; size_t n1, n2;
    sprintf(p1, "%s\\00_Sprites_Default_b000_vs_756ae2904e92.dxbc", argv[2]);
    sprintf(p2, "%s\\00_Sprites_Default_b004_ps_3aa3dabc65f1.dxbc", argv[2]);
    unsigned char *ovs = readall(p1, &n1), *ops = readall(p2, &n2);
    if (!ovs || !ops) { printf("orig missing\n"); return 1; }
    unsigned vsz, psz; const unsigned char *vs = lookup(ovs + 4, &vsz), *ps = lookup(ops + 4, &psz);
    printf("lookup VS %s PS %s\n", vs ? "hit" : "MISS", ps ? "hit" : "MISS");
    HMODULE dc = LoadLibraryA("d3dcompiler_47.dll"); PFN_GETSIG getsig = dc ? (PFN_GETSIG)GetProcAddress(dc, "D3DGetInputSignatureBlob") : NULL;
    ID3DBlob *sig = NULL; if (getsig) hr = getsig(ovs, n1, &sig); else hr = E_NOINTERFACE;
    printf("D3DGetInputSignatureBlob(orig Unity VS) hr=0x%08lx size=%u\n", (unsigned long)hr, sig ? (unsigned)ID3D10Blob_GetBufferSize(sig) : 0);
    if (sig) { const unsigned char *s = (const unsigned char *)ID3D10Blob_GetBufferPointer(sig);
        printf("  sig blob: magic %.4s chunks=%u first chunk %.4s; checksum %s table: %s\n", s, *(unsigned *)(s + 28), s + *(unsigned *)(s + 32),
               "in", lookup(s + 4, &psz) ? "YES" : "no (a checksum-keyed CreateInputLayout hook MISSES)"); }
    D3D11_INPUT_ELEMENT_DESC el[3] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11InputLayout *ilA = NULL, *ilB = NULL, *ilC = NULL;
    if (sig) rep("CreateInputLayout(signature-only blob, like Unity sub_100fa88)", ID3D11Device_CreateInputLayout(dev, el, 3, ID3D10Blob_GetBufferPointer(sig), ID3D10Blob_GetBufferSize(sig), &ilA));
    rep("CreateInputLayout(original Unity vs_4_0 bytecode)", ID3D11Device_CreateInputLayout(dev, el, 3, ovs, n1, &ilB));
    rep("CreateInputLayout(level-9 replacement VS)", ID3D11Device_CreateInputLayout(dev, el, 3, vs, vsz, &ilC));
    /* zero checksum variant of the signature-only blob */
    if (sig) { size_t sl = ID3D10Blob_GetBufferSize(sig); unsigned char *z = (unsigned char *)malloc(sl); memcpy(z, ID3D10Blob_GetBufferPointer(sig), sl); memset(z + 4, 0, 16);
        ID3D11InputLayout *ilZ = NULL; rep("CreateInputLayout(signature-only blob with ZERO checksum)", ID3D11Device_CreateInputLayout(dev, el, 3, z, sl, &ilZ)); if (ilZ) ID3D11InputLayout_Release(ilZ); }
    ID3D11InputLayout *il = ilA ? ilA : ilB;
    printf("draw uses layout from %s\n", ilA ? "signature-only blob" : (ilB ? "original VS" : "NONE"));
    if (!il || !vs || !ps) return 1;
    ID3D11VertexShader *v = NULL; ID3D11PixelShader *f = NULL;
    rep("CreateVertexShader(replacement)", ID3D11Device_CreateVertexShader(dev, vs, vsz, NULL, &v));
    rep("CreatePixelShader(replacement)", ID3D11Device_CreatePixelShader(dev, ps, psz, NULL, &f));
    struct { float x, y, z; unsigned c; float u, v; } q[4] = {
        { -1, -1, 0.5f, 0xffffffff, 0, 1 }, { -1, 1, 0.5f, 0xffffffff, 0, 0 }, { 1, 1, 0.5f, 0xffffffff, 1, 0 }, { 1, -1, 0.5f, 0xffffffff, 1, 1 } };
    unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };
    D3D11_BUFFER_DESC bd = {0}; D3D11_SUBRESOURCE_DATA sd = {0}; ID3D11Buffer *vb = NULL, *ib = NULL, *cb[4] = {0};
    bd.ByteWidth = sizeof q; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; sd.pSysMem = q; ID3D11Device_CreateBuffer(dev, &bd, &sd, &vb);
    bd.ByteWidth = sizeof idx; bd.BindFlags = D3D11_BIND_INDEX_BUFFER; sd.pSysMem = idx; ID3D11Device_CreateBuffer(dev, &bd, &sd, &ib);
    static float g[12], draw[44], frame[92], spr[8];
    g[8] = g[9] = g[10] = g[11] = 1; draw[0] = draw[5] = draw[10] = draw[15] = 1; frame[68] = frame[73] = frame[78] = frame[83] = 1;
    spr[0] = spr[1] = spr[2] = spr[3] = 1; spr[4] = spr[5] = 1;
    unsigned sizes[4] = { 48, 176, 368, 32 }; float *data[4] = { g, draw, frame, spr };
    for (int i = 0; i < 4; ++i) { bd.ByteWidth = sizes[i]; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; sd.pSysMem = data[i]; ID3D11Device_CreateBuffer(dev, &bd, &sd, &cb[i]); }
    unsigned texel[16]; for (int i = 0; i < 16; ++i) texel[i] = 0xff3264c8u;
    ID3D11Texture2D *tex = NULL, *rt = NULL, *st = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *rtv = NULL;
    D3D11_TEXTURE2D_DESC td = {0}; td.Width = td.Height = 4; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sd.pSysMem = texel; sd.SysMemPitch = 16; ID3D11Device_CreateTexture2D(dev, &td, &sd, &tex); ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, NULL, &srv);
    td.Width = td.Height = 64; td.BindFlags = D3D11_BIND_RENDER_TARGET; ID3D11Device_CreateTexture2D(dev, &td, NULL, &rt); ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)rt, NULL, &rtv);
    td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ; ID3D11Device_CreateTexture2D(dev, &td, NULL, &st);
    D3D11_SAMPLER_DESC smp = {0}; smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; smp.MaxAnisotropy = 1; smp.MinLOD = -FLT_MAX; smp.MaxLOD = FLT_MAX; ID3D11SamplerState *ss = NULL; ID3D11Device_CreateSamplerState(dev, &smp, &ss);
    D3D11_RASTERIZER_DESC rd = {0}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE; ID3D11RasterizerState *rs = NULL; ID3D11Device_CreateRasterizerState(dev, &rd, &rs);
    float black[4] = { 0, 0, 0, 0 }; ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, black);
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL); ID3D11DeviceContext_RSSetState(ctx, rs);
    D3D11_VIEWPORT vp = { 0, 0, 64, 64, 0, 1 }; ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
    UINT stride = 24, off = 0; ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vb, &stride, &off); ID3D11DeviceContext_IASetIndexBuffer(ctx, ib, DXGI_FORMAT_R16_UINT, 0);
    ID3D11DeviceContext_IASetInputLayout(ctx, il); ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(ctx, v, NULL, 0); ID3D11DeviceContext_PSSetShader(ctx, f, NULL, 0);
    if (getenv("CBOFF")) { /* Unity path when caps+0x3f9 (ConstantBufferOffsetting) is set: one big ring buffer + VSSetConstantBuffers1 windows */
        ID3D11DeviceContext1 *c1 = NULL; ID3D11DeviceContext_QueryInterface(ctx, &IID_ID3D11DeviceContext1, (void **)&c1);
        D3D11_FEATURE_DATA_D3D11_OPTIONS o = {0}; ID3D11Device_CheckFeatureSupport(dev, D3D11_FEATURE_D3D11_OPTIONS, &o, sizeof o);
        printf("  D3D11_OPTIONS.ConstantBufferOffsetting=%d (Unity caps+0x3f9)\n", o.ConstantBufferOffsetting);
        D3D11_BUFFER_DESC rb = {0}; rb.ByteWidth = 65536; rb.Usage = D3D11_USAGE_DYNAMIC; rb.BindFlags = D3D11_BIND_CONSTANT_BUFFER; rb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ID3D11Buffer *ring = NULL; rep("CreateBuffer 64 KB dynamic constant ring", ID3D11Device_CreateBuffer(dev, &rb, NULL, &ring));
        D3D11_MAPPED_SUBRESOURCE mm; ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)ring, 0, D3D11_MAP_WRITE_DISCARD, 0, &mm);
        memset(mm.pData, 0x7f, 65536); /* garbage everywhere else */
        for (int i = 0; i < 4; ++i) memcpy((unsigned char *)mm.pData + 0x1000 * (i + 1), data[i], sizes[i]);
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)ring, 0);
        ID3D11Buffer *rr[4] = { ring, ring, ring, ring }; UINT first[4] = { 256, 512, 768, 1024 }, num[4] = { 32, 32, 32, 32 };
        ID3D11DeviceContext1_VSSetConstantBuffers1(c1, 0, 4, rr, first, num); rep("VSSetConstantBuffers1 (4 windows into one 64 KB buffer)", S_OK);
    } else
    ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 4, cb); ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv); ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &ss);
    ID3D11DeviceContext_DrawIndexed(ctx, 6, 0, 0); rep("DrawIndexed(6) with that layout + level-9 Sprites/Default", S_OK);
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)st, (ID3D11Resource *)rt);
    D3D11_MAPPED_SUBRESOURCE m; hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m);
    if (SUCCEEDED(hr)) { unsigned px = ((unsigned *)((unsigned char *)m.pData + 32 * m.RowPitch))[32]; ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0);
        printf("  read-back centre pixel RGBA = %u %u %u %u (expected 200 100 50 255)\n", px & 0xff, (px >> 8) & 0xff, (px >> 16) & 0xff, px >> 24); }
    rep("GetDeviceRemovedReason", ID3D11Device_GetDeviceRemovedReason(dev));
    return 0;
}
