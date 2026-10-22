import sys, re, collections
sys.path.insert(0, r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis')
from q import D, starts, mem
res = collections.defaultdict(list)
for b in starts:
    l, ins = D[b]
    n = len(ins)
    for i, (ia, isz, mn, ops) in enumerate(ins):
        if mn == 'movw' and re.match(r'\w+, #0xd7e0$', ops.strip()):
            reg = ops.split(',')[0].strip()
            alias = set()
            # find ldr X,[reg] within 6
            for k in range(i + 1, min(i + 8, n)):
                a, s, m2, o2 = ins[k]
                if m2.startswith('ldr') and not m2.startswith(('ldrb', 'ldrh')):
                    x = mem(o2)
                    if x and x[0] == reg and x[1] == 0:
                        pr = o2.split(',')[0].strip()
                        # now find: ldr V,[pr]  ; ldr T,[V,#slot]
                        for k2 in range(k + 1, min(k + 14, n)):
                            a2, s2, m3, o3 = ins[k2]
                            if m3.startswith('ldr') and not m3.startswith(('ldrb', 'ldrh')):
                                x2 = mem(o3)
                                if x2 and x2[0] == pr and x2[1] == 0:
                                    v = o3.split(',')[0].strip()
                                    for k3 in range(k2 + 1, min(k2 + 5, n)):
                                        a3, s3, m4, o4 = ins[k3]
                                        if m4.startswith('ldr'):
                                            x3 = mem(o4)
                                            if x3 and x3[0] == v and x3[1] != 0:
                                                res[x3[1]].append((b, ia))
                                                break
                                    break
                        break
for off in sorted(res):
    print('0x%x slot %d : %d sites : %s' % (off, off // 4, len(res[off]), ' '.join('%x' % x[0] for x in res[off][:8])))
