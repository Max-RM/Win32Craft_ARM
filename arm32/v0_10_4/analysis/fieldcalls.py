import sys, re
sys.path.insert(0, r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis')
from q import D, starts, mem
# usage: fieldcalls.py <fieldoff> [lo hi]  : after "ldr rX,[rY,#field]" find vtable call on rX (ldr rA,[rX]; ldr rB,[rA,#slot]; blx rB)
field = int(sys.argv[1], 0)
lo = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0
hi = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0xffffff
for b in starts:
    if b < lo or b > hi: continue
    l, ins = D[b]
    n = len(ins)
    for i, (ia, isz, mn, ops) in enumerate(ins):
        if mn.startswith('ldr') and not mn.startswith(('ldrb', 'ldrh', 'ldrs')):
            mm = mem(ops)
            if mm and mm[1] == field and mm[0] not in ('sp', 'pc', 'r7'):
                d = ops.split(',')[0].strip()
                alias = {d}
                for k in range(i + 1, min(i + 10, n)):
                    a, s, m2, o2 = ins[k]
                    mm2 = re.match(r'(\w+), (\w+)$', o2.strip())
                    if m2 == 'mov' and mm2 and mm2.group(2) in alias:
                        alias.add(mm2.group(1))
                    if m2.startswith('ldr') and not m2.startswith(('ldrb', 'ldrh')):
                        x = mem(o2)
                        d2 = o2.split(',')[0].strip()
                        if x and x[0] in alias and x[1] == 0:
                            for k2 in range(k + 1, min(k + 5, n)):
                                a3, s3, m3, o3 = ins[k2]
                                if m3.startswith('ldr'):
                                    x3 = mem(o3)
                                    if x3 and x3[0] == d2 and x3[1] != 0:
                                        print('%08x f=%08x slot 0x%x (%d)' % (ia, b, x3[1], x3[1] // 4))
                                        break
                            break
