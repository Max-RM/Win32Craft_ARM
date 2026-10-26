p = r'C:\Users\hamed\AppData\Local\Temp\claude\D--repo-Win32Craft-main\7ed8744f-0d72-482c-82d8-cd88a334a2ff\scratchpad\design\l9bench.c'
s = open(p).read()
old = '    printf("\\n[B] feature data at 9_1\\n");'
assert old in s
new = r'''    { void *d1 = NULL, *d2 = NULL; ID3D11Device_QueryInterface(dev, &IID_ID3D11Device1, &d1); ID3D11Device_QueryInterface(dev, &IID_ID3D11Device2, &d2);
      printf("  identity: device=%p vtbl=%p | Device1=%p vtbl=%p | Device2=%p vtbl=%p\n", (void *)dev, *(void **)dev, d1, d1 ? *(void **)d1 : NULL, d2, d2 ? *(void **)d2 : NULL);
      MEMORY_BASIC_INFORMATION mb; VirtualQuery(*(void **)dev, &mb, sizeof mb); printf("  device vtable page protect=0x%lx type=0x%lx\n", mb.Protect, mb.Type);
      void *cv = *(void **)ctx; VirtualQuery(cv, &mb, sizeof mb); printf("  context vtable=%p page protect=0x%lx type=0x%lx\n", cv, mb.Protect, mb.Type);
      if (d1) IUnknown_Release((IUnknown *)d1); if (d2) IUnknown_Release((IUnknown *)d2); }
''' + old
s = s.replace(old, new)
open(p, 'w').write(s)
print('patched3')
