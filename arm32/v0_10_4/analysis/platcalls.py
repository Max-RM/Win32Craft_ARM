import sys, re, collections
sys.path.insert(0, r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis')
from q import D, starts, mem

# find "platform = client->vf8()" then platform->vfN calls
res = collections.defaultdict(list)
for b in starts:
    l, ins = D[b]
    n = len(ins)
    for i in range(n):
        ia, isz, mn, ops = ins[i]
        if mn.startswith('ldr') and re.match(r'\w+, \[\w+, #0x20\]$', ops.strip()):
            dst = ops.split(',')[0].strip()
            # find blx dst within 3
            j = None
            for k in range(i + 1, min(i + 4, n)):
                if ins[k][2] == 'blx' and ins[k][3].strip() == dst:
                    j = k; break
            if j is None: continue
            # now scan after for platform vcall
            alias = {'r0'}
            for k in range(j + 1, min(j + 14, n)):
                a, s, m2, o2 = ins[k]
                mm = re.match(r'(\w+), (\w+)$', o2.strip())
                if m2 == 'mov' and mm and mm.group(2) in alias:
                    alias.add(mm.group(1))
                if m2.startswith('ldr') and not m2.startswith('ldrb'):
                    mm2 = mem(o2)
                    d2 = o2.split(',')[0].strip()
                    if mm2 and mm2[0] in alias and mm2[1] == 0:
                        # vtable load into d2; next ldr d3,[d2,#off]
                        for k2 in range(k + 1, min(k + 6, n)):
                            a3, s3, m3, o3 = ins[k2]
                            if m3.startswith('ldr'):
                                mm3 = mem(o3)
                                if mm3 and mm3[0] == d2 and mm3[1] != 0:
                                    res[mm3[1]].append((b, ia))
                                    break
                        break
for off in sorted(res):
    print('0x%x slot %d : %d sites e.g. %s' % (off, off // 4, len(res[off]), ' '.join('%x' % x[1] for x in res[off][:5])))
