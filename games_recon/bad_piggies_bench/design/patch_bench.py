p = r'C:\Users\hamed\AppData\Local\Temp\claude\D--repo-Win32Craft-main\7ed8744f-0d72-482c-82d8-cd88a334a2ff\scratchpad\design\l9bench.c'
s = open(p).read()
old = '''      if (b) { ID3D11DeviceContext_IASetIndexBuffer(ctx, b, DXGI_FORMAT_R32_UINT, 0); rep("IASetIndexBuffer R32_UINT (void call, debug layer decides)", S_OK, -1);
               ID3D11DeviceContext_IASetIndexBuffer(ctx, b, DXGI_FORMAT_R16_UINT, 0); rep("IASetIndexBuffer R16_UINT", S_OK, -1); ID3D11Buffer_Release(b); b = NULL; }'''
new = '''      if (b) { ID3D11DeviceContext_IASetIndexBuffer(ctx, b, DXGI_FORMAT_R16_UINT, 0); rep("IASetIndexBuffer R16_UINT", S_OK, -1); g_ib = b; b = NULL; }'''
assert old in s; s = s.replace(old, new)
s = s.replace('static int g_pass, g_fail;', 'static int g_pass, g_fail; static ID3D11Buffer *g_ib;')
old2 = r'''    printf("\nSUMMARY: %d as-expected, %d unexpected\n", g_pass, g_fail);'''
new2 = r'''    printf("\n[G] LAST: IASetIndexBuffer with R32_UINT (Unity Supports32bitsIndexBuffer=1)\n");
    rep("GetDeviceRemovedReason before", ID3D11Device_GetDeviceRemovedReason(dev), 1);
    if (g_ib) { ID3D11DeviceContext_IASetIndexBuffer(ctx, g_ib, DXGI_FORMAT_R32_UINT, 0); rep("IASetIndexBuffer R32_UINT (void call)", S_OK, -1); ID3D11DeviceContext_Flush(ctx); }
    rep("GetDeviceRemovedReason after", ID3D11Device_GetDeviceRemovedReason(dev), -1);
    printf("\nSUMMARY: %d as-expected, %d unexpected\n", g_pass, g_fail);'''
assert old2 in s, 'old2'; s = s.replace(old2, new2)
for a in ["\"tex 124x124 RGBA8 7 mips ('loading', NPOT+mips)\", tex2d(124, 124, 7, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), 0)",
          "\"tex 1024x652 RGBA8 11 mips ('Comic_DailyChallenge')\", tex2d(1024, 652, 11, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, NULL), 0)"]:
    assert a in s, a; s = s.replace(a, a[:-2] + "-1)")
a = 'orig++; if (SUCCEEDED(h)) origok++; drain("orig");'
assert a in s; s = s.replace(a, 'orig++; if (SUCCEEDED(h)) origok++; if (iq) ID3D11InfoQueue_ClearStoredMessages(iq);')
open(p, 'w').write(s)
print('patched')
