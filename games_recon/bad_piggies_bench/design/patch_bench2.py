p = r'C:\Users\hamed\AppData\Local\Temp\claude\D--repo-Win32Craft-main\7ed8744f-0d72-482c-82d8-cd88a334a2ff\scratchpad\design\l9bench.c'
s = open(p).read()
func = r'''
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
'''
anchor = 'int main(int argc, char **argv) {'
assert anchor in s
s = s.replace(anchor, func + '\n' + anchor)
old = r'''    printf("\n[G] LAST: IASetIndexBuffer'''
new = r'''    printf("\n[F2] draw test through the lookup (Sprites/Default)\n");
    if (nents) { char od[MAX_PATH]; sprintf(od, "%s", argc > 5 ? argv[5] : "."); draw_test(od); }
    printf("\n[G] LAST: IASetIndexBuffer'''
assert old in s
s = s.replace(old, new)
open(p, 'w').write(s)
print('patched2')
