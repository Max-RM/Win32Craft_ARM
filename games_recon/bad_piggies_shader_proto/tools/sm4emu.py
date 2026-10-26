"""Tiny SM4 interpreter (D3DDisassemble text) used to check that a recompiled shader computes the same outputs
as Unity's original for random inputs/constants. Texture sampling is a deterministic function of (slot, coord)."""
import struct, math, re, random
from sm4tohlsl import Op, split_top, Unsupported

_pf = struct.Struct('<f'); _pI = struct.Struct('<I')
def f2b(x):
    try:
        return _pI.unpack(_pf.pack(x))[0]
    except OverflowError:
        return 0x7f800000 if x > 0 else 0xff800000
def b2f(b):
    return _pf.unpack(_pI.pack(b & 0xffffffff))[0]

C = 'xyzw'

def tex_fn(slot, coord):
    s = 0.0
    for i, c in enumerate(coord):
        if c != c or c in (float('inf'), float('-inf')):
            c = 0.0
        s += c * (12.9898 + 7.233 * i) + slot * 3.17
    return [abs(math.sin(s * (1.0 + 0.37 * k))) for k in range(4)]

class Emu:
    def __init__(self, dis, stage):
        self.stage = stage
        lines = [l.strip() for l in dis.splitlines()]
        start = [i for i, l in enumerate(lines) if re.match(r'^(vs|ps)_[45]_0$', l)][0]
        self.code = [l for l in lines[start + 1:] if l and not l.startswith('//')]
        self.tex = {}
        for l in self.code:
            m = re.match(r'^dcl_resource_(\w+).*t(\d+)$', l)
            if m:
                self.tex[int(m.group(2))] = m.group(1)

    def run(self, inputs, cbs):
        """inputs: {v#: [4 bits]}, cbs: {n: [[4 bits]...]} -> (outputs {o#: [4 bits]}, discarded)"""
        R = {}
        def get_reg(name):
            if name not in R:
                R[name] = [0, 0, 0, 0]
            return R[name]
        for k, v in inputs.items():
            R[k] = list(v)
        discarded = False
        mask_stack = [True]
        def active():
            return all(mask_stack)
        def read(op, comps):
            if op.kind == 'lit':
                vals = op.vals
                bits = [(vals[c] if len(vals) > 1 else vals[0])[1] for c in comps]
            else:
                if op.kind == 'cb':
                    if op.dyn is not None:
                        m = re.match(r'^([rv]\d+)\.([xyzw])(?:\s*\+\s*(\d+))?$', op.dyn)
                        idx = b2f(get_reg(m.group(1))[C.index(m.group(2))])
                        # Unity's dynamic indices are ints in registers; treat bits as int
                        ib = get_reg(m.group(1))[C.index(m.group(2))]
                        base = ib if ib < 4096 else int(idx)
                        i = base + int(m.group(3) or 0)
                        arr = cbs[op.cbn]
                        reg = arr[i] if 0 <= i < len(arr) else [0, 0, 0, 0]
                    else:
                        reg = cbs[op.cbn][op.cbi]
                else:
                    nm = {'r': 'r', 'v': 'v', 'o': 'o'}[op.kind] + str(op.n)
                    reg = get_reg(nm)
                sw = op.swz
                bits = [reg[C.index(sw[c] if len(sw) > 1 else sw[0])] for c in comps]
            out = []
            for b in bits:
                if op.abs: b &= 0x7fffffff
                if op.neg: b ^= 0x80000000
                out.append(b)
            return out
        def readf(op, comps):
            return [b2f(b) for b in read(op, comps)]
        def write(op, comps_vals, sat=False, isfloat=True):
            if not active():
                return
            if op.kind == 'null':
                return
            nm = 'oDepth' if op.kind == 'odepth' else (op.kind + str(op.n))
            reg = get_reg(nm)
            mask = op.swz if op.kind != 'odepth' else 'x'
            for ch, val in zip(mask, comps_vals):
                if isfloat:
                    if sat:
                        val = 0.0 if val != val else min(1.0, max(0.0, val))
                    reg[C.index(ch)] = f2b(val)
                else:
                    reg[C.index(ch)] = val & 0xffffffff
        for l in self.code:
            m = re.match(r'^([A-Za-z_0-9]+)(\([^)]*\))*(?:\s+(.*))?$', l)
            op = m.group(1); args = split_top(m.group(3)) if m.group(3) else []
            if op.startswith('dcl_'):
                continue
            sat = op.endswith('_sat')
            if sat: op = op[:-4]
            if op == 'ret':
                if len(mask_stack) == 1:
                    break
                continue
            if op in ('if_nz', 'if_z'):
                v = read(Op(args[0]), [0])[0]
                cond = (v != 0) if op == 'if_nz' else (v == 0)
                mask_stack.append(cond); continue
            if op == 'else':
                mask_stack[-1] = not mask_stack[-1]; continue
            if op == 'endif':
                mask_stack.pop(); continue
            if op in ('discard_nz', 'discard_z'):
                v = read(Op(args[0]), [0])[0]
                if active() and ((v != 0) if op == 'discard_nz' else (v == 0)):
                    discarded = True
                continue
            ops = [Op(a) for a in args]
            if op == 'sincos':
                ds, dc, a = ops
                for d, fn in ((ds, math.sin), (dc, math.cos)):
                    if d.kind == 'null': continue
                    pos = [C.index(c) for c in d.swz]
                    write(d, [fn(x) if math.isfinite(x) else float('nan') for x in readf(a, pos)], sat)
                continue
            d = ops[0]; src = ops[1:]
            pos = [C.index(c) for c in (d.swz if d.kind not in ('odepth', 'null') else 'x')]
            F = lambda o: readf(o, pos)
            B = lambda o: read(o, pos)
            def cw(fn, n):
                vals = [F(s) for s in src[:n]]
                res = []
                for i in range(len(pos)):
                    try:
                        res.append(fn(*[v[i] for v in vals]))
                    except (ValueError, ZeroDivisionError, OverflowError):
                        res.append(float('nan'))
                return res
            def smax(a, b):
                if a != a: return b
                if b != b: return a
                return max(a, b)
            def smin(a, b):
                if a != a: return b
                if b != b: return a
                return min(a, b)
            def sdiv(a, b):
                if b == 0:
                    if a == 0 or a != a: return float('nan')
                    return math.copysign(float('inf'), a) * math.copysign(1, b)
                return a / b
            def rsq(a):
                if a == 0: return float('inf')
                if a < 0 or a != a: return float('nan')
                return 1 / math.sqrt(a)
            def lg(a):
                if a == 0: return float('-inf')
                if a < 0 or a != a: return float('nan')
                if a == float('inf'): return a
                return math.log2(a)
            def ex(a):
                if a != a: return a
                if a > 128: return float('inf')
                return 2.0 ** a
            if op == 'mov': write(d, F(src[0]) if True else None, sat) if not sat else write(d, F(src[0]), sat)
            elif op == 'add': write(d, cw(lambda a, b: a + b, 2), sat)
            elif op == 'mul': write(d, cw(lambda a, b: a * b, 2), sat)
            elif op == 'mad': write(d, cw(lambda a, b, c: a * b + c, 3), sat)
            elif op == 'div': write(d, cw(sdiv, 2), sat)
            elif op == 'max': write(d, cw(smax, 2), sat)
            elif op == 'min': write(d, cw(smin, 2), sat)
            elif op == 'rsq': write(d, cw(rsq, 1), sat)
            elif op == 'sqrt': write(d, cw(lambda a: math.sqrt(a) if a >= 0 else float('nan'), 1), sat)
            elif op == 'rcp': write(d, cw(lambda a: sdiv(1.0, a), 1), sat)
            elif op == 'exp': write(d, cw(ex, 1), sat)
            elif op == 'log': write(d, cw(lg, 1), sat)
            elif op == 'frc': write(d, cw(lambda a: a - math.floor(a) if math.isfinite(a) else float('nan'), 1), sat)
            elif op == 'round_ni': write(d, cw(lambda a: float(math.floor(a)) if math.isfinite(a) else a, 1), sat)
            elif op == 'round_pi': write(d, cw(lambda a: float(math.ceil(a)) if math.isfinite(a) else a, 1), sat)
            elif op == 'round_z': write(d, cw(lambda a: float(math.trunc(a)) if math.isfinite(a) else a, 1), sat)
            elif op == 'round_ne': write(d, cw(lambda a: float(round(a)) if math.isfinite(a) else a, 1), sat)
            elif op in ('dp2', 'dp3', 'dp4'):
                k = int(op[2]); a = readf(src[0], list(range(k))); b = readf(src[1], list(range(k)))
                s = sum(x * y for x, y in zip(a, b))
                write(d, [s] * len(pos), sat)
            elif op in ('lt', 'ge', 'eq', 'ne'):
                fn = {'lt': lambda a, b: a < b, 'ge': lambda a, b: a >= b, 'eq': lambda a, b: a == b, 'ne': lambda a, b: a != b}[op]
                a = F(src[0]); b = F(src[1])
                write(d, [0xffffffff if fn(x, y) else 0 for x, y in zip(a, b)], isfloat=False)
            elif op == 'movc':
                c = B(src[0]); a = B(src[1]); b = B(src[2])
                res = [x if cc != 0 else y for cc, x, y in zip(c, a, b)]
                if sat:
                    write(d, [b2f(x) for x in res], sat)
                else:
                    write(d, res, isfloat=False)
            elif op == 'and':
                write(d, [x & y for x, y in zip(B(src[0]), B(src[1]))], isfloat=False)
            elif op == 'or':
                write(d, [x | y for x, y in zip(B(src[0]), B(src[1]))], isfloat=False)
            elif op == 'not':
                write(d, [~x & 0xffffffff for x in B(src[0])], isfloat=False)
            elif op in ('sample', 'sample_b', 'sample_l', 'sample_indexable', 'sample_l_indexable', 'sample_b_indexable'):
                coord, t = src[0], src[1]
                k = 2 if self.tex.get(t.n, 'texture2d') == 'texture2d' else 3
                cvals = readf(coord, list(range(k)))
                texel = tex_fn(t.n, cvals)
                write(d, [texel[C.index(t.swz[p])] for p in pos], sat)
            else:
                raise Unsupported('emu opcode %s' % op)
        outs = {k: v for k, v in R.items() if k.startswith('o')}
        return outs, discarded

def close(a, b):
    fa, fb = b2f(a), b2f(b)
    if fa != fa and fb != fb:
        return True
    if fa == fb:
        return True
    if math.isinf(fa) or math.isinf(fb):
        return False
    return abs(fa - fb) <= 2e-3 * max(1.0, abs(fa), abs(fb))

def equivalent(dis_a, dis_b, stage, trials=24, seed=1, fix=None):
    """fix: optional {(cb, reg, comp): float} constants forced in every trial (to test a trimmed shader under game conditions)"""
    ea = Emu(dis_a, stage); eb = Emu(dis_b, stage)
    rnd = random.Random(seed)
    vregs = set(); cbsz = {}
    for l in ea.code + eb.code:
        for m in re.finditer(r'\bv(\d+)\b', l): vregs.add(int(m.group(1)))
        m = re.match(r'^dcl_constantbuffer CB(\d+)\[(\d+)\]', l)
        if m: cbsz[int(m.group(1))] = max(cbsz.get(int(m.group(1)), 0), int(m.group(2)))
    bad = 0; worst = None
    for t in range(trials):
        inputs = {'v%d' % v: [f2b(rnd.uniform(-1.5, 1.5)) for _ in range(4)] for v in vregs}
        cbs = {n: [[f2b(rnd.uniform(-1.5, 1.5)) for _ in range(4)] for _ in range(sz)] for n, sz in cbsz.items()}
        for (n, i, c), val in (fix or {}).items():
            if n in cbs and i < len(cbs[n]):
                cbs[n][i][c] = f2b(val)
        oa, da = ea.run(inputs, cbs); ob, db = eb.run(inputs, cbs)
        if da != db:
            bad += 1; worst = ('discard', da, db); continue
        if da:
            continue
        for k in set(oa) | set(ob):
            va = oa.get(k, [0] * 4); vb = ob.get(k, [0] * 4)
            for i in range(4):
                if not close(va[i], vb[i]):
                    bad += 1; worst = (k, i, b2f(va[i]), b2f(vb[i])); break
            else:
                continue
            break
    return bad, worst
