import sys, re, collections
sys.path.insert(0, r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis')
from q import D, starts, mem
cnt = collections.defaultdict(set)
for b in starts:
    l, ins = D[b]
    n = len(ins)
    for i, (ia, isz, mn, ops) in enumerate(ins):
        if mn == 'bl' and ops.strip() == '#0x4b054':
            alias = {'r0'}
            for k in range(i + 1, min(i + 10, n)):
                a, s, m2, o2 = ins[k]
                mm2 = re.match(r'(\w+), (\w+)$', o2.strip())
                if m2 == 'mov' and mm2 and mm2.group(2) in alias:
                    alias.add(mm2.group(1))
                if m2.startswith(('ldr', 'str', 'vldr', 'vstr', 'add', 'adds')):
                    x = mem(o2)
                    if x and x[0] in alias:
                        cnt[x[1]].add(b)
                    mm3 = re.match(r'(\w+), (\w+), #(0x[0-9a-f]+|\d+)$', o2.strip())
                    if m2.startswith('add') and mm3 and mm3.group(2) in alias:
                        cnt[int(mm3.group(3), 0)].add(b)
                if m2 == 'bl': break
for off in sorted(cnt):
    print('DR+0x%x : %s' % (off, ' '.join('%x' % x for x in sorted(cnt[off])[:8])))
