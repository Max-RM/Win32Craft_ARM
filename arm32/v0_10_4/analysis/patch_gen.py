p = r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis\gen_client_init.py'
t = open(p, encoding='utf-8-sig').read()
blk = open(r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis\vf7_block.txt', encoding='utf-8').read()
a = t.index("F('client_init.init.vf7_steps'")
b = t.index("F('client_init.init.textures_font_writers'")
t = t[:a] + blk + t[b:]

# platform vtable fact: auto classify remaining slots
old_start = t.index("for s, r, t in pslots:\n    cell = PLAT_VT + 4 * s")
old_end = t.index("F('client_init.platform.asset_access'")
new = '''for s, r, t in pslots:
    cell = PLAT_VT + 4 * s
    assert u32(cell) == 0x400000 + r + 1, (s, hex(u32(cell)), hex(r))
named = set(s for s, r, t in pslots)
stub_void, ret_false, ret_true, other = [], [], [], []
for s in range(43):
    if s in named: continue
    v = u32(PLAT_VT + 4 * s) - 0x400000 - 1
    if v == 0x0BF60: stub_void.append(s)
    elif v == 0x29BA0: ret_false.append(s)
    elif v == 0x17FD40: ret_true.append(s)
    else: other.append((s, v))
vt_text = ('AppPlatform_Winrt vtable (43 slots, RVA 0x33FB20; base AppPlatform 0x33F890; slot cell = RVA + 4*slot, value = 0x400000+fn+1). Overridden/implemented slots (slot: function RVA - behaviour): '
           + '; '.join('[%d] 0x%x %s' % (s, r, t) for s, r, t in pslots)
           + '. Empty void stubs (0x0BF60): %s. Return-false stubs (0x29BA0): %s. Return-true stubs (0x17FD40): %s. Other unanalysed: %s.'
           % (stub_void, ret_false, ret_true, ', '.join('[%d] 0x%x' % (s, v) for s, v in other)))
F('client_init.platform.vtable', 'vtable', PLAT_VT, vt_text,
  None,
  'Calls are plain C++ virtual calls: this=r0; an sret std::string is passed in r1 (this stays r0). Slots used on the init path via client->vf8()/g_platform: 14 (assets), 21, 26, 30, 40, 18 (setSize), 34 (focus lost), 25 (postUpdate), 2/3 (texture decode).',
  ['vtable dump arm32re.py vtable 0x33fb20 60', 'bodies read for every listed slot', 'independent labels by the platform/input agents agree on slots 1,2,9,12,14,19,23,29,31,32,33,34,40 (tools/arm32/names)'],
  'medium', bytes_n=None)
facts[-1]['expected_bytes'] = None

'''
t = t[:old_start] + new + t[old_end:]
# fix gplat evidence
t = t.replace("'gplat scan: movw #0xd7e0/movt #0x11d at 0x4726e, 0x47ade, 0x47c00, 0x4a2.. (g_platform readers)'", "'movw #0xd7e0 + movt #0x11d (VA 0x11dd7e0) sites: 0x4728e (writer, AppPlatform ctor), 0x47adc, 0x47bfe, 0xa0412, 0xa0bf2, 0xcd3ba, 0xd663a, 0xdc528, 0xdcd68, 0x1164b8, 0x1a684e'")
t = t.replace("Options::sync 0xA29B0", "0xA29B0(options)")
t = t.replace("KB/MOUSE UI: make AppPlatform_Winrt slot 26 (cell 0x33FB88? = platform vtbl+0x68)", "KB/MOUSE UI: make AppPlatform_Winrt slot 26 (cell RVA 0x33FB88 = platform vtbl+0x68)")
open(p, 'w', encoding='utf-8').write(t)
print('patched')
