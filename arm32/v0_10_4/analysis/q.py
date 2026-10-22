import sys, pickle, re, bisect
D = pickle.load(open(r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis\all_dis.pkl', 'rb'))
starts = sorted(D)

def fof(a):
    i = bisect.bisect_right(starts, a) - 1
    return starts[i] if i >= 0 else None

def mem(ops):
    m = re.search(r'\[(\w+), #(-?0x[0-9a-f]+|-?\d+)\]', ops)
    if m:
        return m.group(1), int(m.group(2), 0)
    m = re.search(r'\[(\w+)\]', ops)
    if m:
        return m.group(1), 0
    return None

def field(off, kinds=('str', 'ldr')):
    """list accesses [rX,#off]"""
    for b in starts:
        l, ins = D[b]
        for (ia, isz, mn, ops) in ins:
            if any(mn.startswith(k) for k in kinds):
                mm = mem(ops)
                if mm and mm[1] == off:
                    print('%08x f=%08x %s %s' % (ia, b, mn, ops))

def vcall(slotoff):
    """calls via ldr rA,[rB,#slotoff]; ... blx rA"""
    for b in starts:
        l, ins = D[b]
        for i, (ia, isz, mn, ops) in enumerate(ins):
            if mn.startswith('ldr') and not mn.startswith('ldrb') and not mn.startswith('ldrh'):
                mm = mem(ops)
                if mm and mm[1] == slotoff and not ops.startswith('pc') :
                    dst = ops.split(',')[0].strip()
                    for j in range(i + 1, min(i + 6, len(ins))):
                        m2 = ins[j]
                        if m2[2] == 'blx' and m2[3].strip() == dst:
                            print('%08x f=%08x' % (ia, b))
                            break
                        if m2[3].split(',')[0].strip() == dst and not m2[2].startswith('str'):
                            break

def callers(t):
    pat = '#0x%x' % t
    for b in starts:
        l, ins = D[b]
        for (ia, isz, mn, ops) in ins:
            if mn in ('bl', 'b.w', 'b') and ops.strip() == pat:
                print('%08x f=%08x %s' % (ia, b, mn))

def grep(rx):
    r = re.compile(rx)
    for b in starts:
        l, ins = D[b]
        for (ia, isz, mn, ops) in ins:
            if r.search(mn + ' ' + ops):
                print('%08x f=%08x %s %s' % (ia, b, mn, ops))

def calls(f):
    l, ins = D[f]
    for (ia, isz, mn, ops) in ins:
        if mn in ('bl', 'blx') or mn.startswith('b.w'):
            print('%08x %s %s' % (ia, mn, ops))

if __name__ == '__main__':
    cmd = sys.argv[1]
    args = sys.argv[2:]
    if cmd == 'field': field(int(args[0], 0), tuple(args[1].split(',')) if len(args) > 1 else ('str', 'ldr'))
    elif cmd == 'vcall': vcall(int(args[0], 0))
    elif cmd == 'callers': callers(int(args[0], 0))
    elif cmd == 'grep': grep(args[0])
    elif cmd == 'calls': calls(int(args[0], 0))
