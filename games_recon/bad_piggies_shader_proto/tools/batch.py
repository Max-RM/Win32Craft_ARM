"""Translate + compile every unique DXBC blob: SM4 fidelity check and level_9_1 compile."""
import os, sys, json, re, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sm4tohlsl import translate, Unsupported
from d3dc import compile, disassemble
from sm4emu import equivalent

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
HL = os.path.join(OUT, 'hlsl'); os.makedirs(HL, exist_ok=True)
rows = json.load(open(os.path.join(OUT, 'analysis.json')))

COMMUT = {'mul', 'add', 'max', 'min', 'dp2', 'dp3', 'dp4', 'eq', 'ne', 'and', 'or', 'mul_sat', 'add_sat', 'dp3_sat', 'dp4_sat', 'dp2_sat', 'max_sat', 'min_sat'}
def norm(lines):
    out = []
    for l in lines:
        m = re.match(r'^(\S+)\s+(.*)$', l)
        if not m:
            out.append(l); continue
        op, rest = m.group(1), m.group(2)
        from sm4tohlsl import split_top
        a = split_top(rest)
        if op in COMMUT and len(a) == 3:
            a = [a[0]] + sorted(a[1:])
        elif op in ('mad', 'mad_sat') and len(a) == 4:
            a = [a[0]] + sorted(a[1:3]) + [a[3]]
        out.append(op + ' ' + ', '.join(a))
    return out

def code_lines(dis, stage):
    i = dis.index(stage + '_4_0') if dis.lstrip().startswith('//') is False else None
    m = re.search(r'^\s*%s_[45]_0\s*$' % stage, dis, re.M)
    body = dis[m.end():]
    return [l.strip() for l in body.splitlines() if l.strip() and not l.strip().startswith('//') and not l.strip().startswith('dcl_')]

def l9info(d9):
    slots = re.search(r'approximately (\d+) instruction slots used(?: \((\d+) texture, (\d+) arithmetic\))?', d9)
    regs = re.findall(r'^//\s+c(\d+)\s+cb(\d+)\s+(\d+)\s+(\d+)', d9, re.M)
    ncb = sum(int(r[3]) for r in regs)
    defs = len(re.findall(r'^\s+def c\d+', d9, re.M))
    return {'slots': int(slots.group(1)) if slots else None, 'tex': int(slots.group(2)) if slots and slots.group(2) else None,
            'alu': int(slots.group(3)) if slots and slots.group(3) else None, 'cbregs': ncb, 'defs': defs}

res = {}
done = set()
for r in rows:
    for bi, b in r['blobs'].items():
        h = b['sha1']
        if h in done:
            continue
        done.add(h)
        stage = b['stage']
        dis = open(b['file']).read()
        ent = {'stage': stage, 'shader': r['name'], 'blob': bi, 'keywords': b['keywords'], 'sm4_n': b['ninstr']}
        for mode in ('keep', 'bias'):
            try:
                src, t = translate(dis, stage, lod_mode=mode)
            except Unsupported as e:
                ent['translate'] = 'UNSUPPORTED: %s' % e
                break
            except Exception as e:
                ent['translate'] = 'ERROR: %r' % e
                break
            ent['translate'] = 'ok'
            ent['approx'] = sorted(set(t.approx))
            fn = os.path.join(HL, '%s_%s_%s.hlsl' % (h, stage, mode))
            open(fn, 'w').write(src)
            bc, msg = compile(src, stage + '_4_0')
            if not bc:
                ent['sm4'] = 'FAIL ' + msg.strip()[:300]
            else:
                d2 = disassemble(bc)
                try:
                    nb, worst = equivalent(dis, d2, stage)
                    ent['emu'] = 'EQUAL' if nb == 0 else 'MISMATCH %d/24 %r' % (nb, worst)
                except Exception as ex:
                    ent['emu'] = 'emu-error %r' % ex
                a = code_lines(dis, stage); bb = code_lines(d2, stage)
                if a == bb: ent['sm4'] = 'identical'
                elif norm(a) == norm(bb): ent['sm4'] = 'identical_mod_commutation'
                elif len(a) == len(bb): ent['sm4'] = 'same_count_differs'
                else: ent['sm4'] = 'differs %d->%d' % (len(a), len(bb))
            bc9, msg9 = compile(src, stage + '_4_0_level_9_1')
            if bc9:
                d9 = disassemble(bc9)
                ent['l9'] = 'OK'; ent['l9info'] = l9info(d9); ent['l9mode'] = mode
                open(fn[:-5] + '.l9.cso', 'wb').write(bc9)
                open(fn[:-5] + '.l9.asm', 'w').write(d9)
                break
            else:
                ent['l9'] = 'FAIL'; ent['l9err'] = msg9.strip()[:400]; ent['l9mode'] = mode
                if not any('sample_l' in x or 'SampleLevel' in msg9 or 'lod' in msg9.lower() for x in [msg9]):
                    if 'SampleLevel' not in src:
                        break
        res[h] = ent
json.dump(res, open(os.path.join(OUT, 'batch.json'), 'w'), indent=1)
c = collections.Counter()
for h, e in res.items():
    c[(e['stage'], e['translate'][:11], e.get('sm4', '-')[:25], e.get('l9', '-'), e.get('l9mode', '-'))] += 1
for k, v in sorted(c.items(), key=lambda x: -x[1]):
    print(v, k)
