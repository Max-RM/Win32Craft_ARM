import sys, os, re
sys.path.insert(0, r'D:\repo\Win32Craft-main\tools\arm32')
import arm32re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
c = arm32re.Ctx()
pe = c.pe
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
pat = re.compile(sys.argv[1])
want = sys.argv[2:]
for (b, l) in c.idx.funcs:
    code = bytes(pe.img[b:b+l])
    prev = []
    for (ia, isz, mn, ops) in md.disasm_lite(code, b):
        prev.append((ia, mn, ops))
        if pat.search(mn + ' ' + ops):
            ctx = prev[-3:]
            print('%08x in func %08x: %s' % (ia, b, ' | '.join('%s %s' % (m, o) for (_, m, o) in ctx)))
