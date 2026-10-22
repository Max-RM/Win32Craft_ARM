import sys, re
sys.path.insert(0, r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis')
from q import D, starts, mem
slot = int(sys.argv[1], 0)
back = int(sys.argv[2]) if len(sys.argv) > 2 else 8
lo = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0
hi = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0xffffff
for b in starts:
    if b < lo or b > hi: continue
    l, ins = D[b]
    for i, (ia, isz, mn, ops) in enumerate(ins):
        if mn.startswith('ldr') and not mn.startswith(('ldrb', 'ldrh', 'ldrs')):
            mm = mem(ops)
            if mm and mm[1] == slot:
                dst = ops.split(',')[0].strip()
                for j in range(i + 1, min(i + 6, len(ins))):
                    if ins[j][2] == 'blx' and ins[j][3].strip() == dst:
                        ctx = ins[max(0, i - back):j + 1]
                        print('--- %08x f=%08x' % (ia, b))
                        for c in ctx:
                            print('   %08x %s %s' % (c[0], c[2], c[3]))
                        break
