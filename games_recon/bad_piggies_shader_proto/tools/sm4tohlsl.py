"""Mechanical SM4 (D3DDisassemble text) -> HLSL translator for simple Unity shaders.
Keeps: cbuffer slots + register offsets, texture/sampler slots, input/output signature (semantics, order, masks).
Unsupported constructs raise Unsupported(reason)."""
import re, struct

class Unsupported(Exception):
    pass

COMP = 'xyzw'

def split_top(s):
    out = []; depth = 0; cur = ''
    for ch in s:
        if ch in '([': depth += 1
        elif ch in ')]': depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out

def hexf(tok):
    tok = tok.strip()
    if tok.startswith('0x') or tok.startswith('-0x'):
        v = int(tok, 16) & 0xffffffff
        return struct.unpack('<f', struct.pack('<I', v))[0], v
    if re.match(r'^-?\d+$', tok):
        # integer literal: keep both readings
        iv = int(tok)
        return float(iv), iv & 0xffffffff
    f = float(tok)
    return f, struct.unpack('<I', struct.pack('<f', f))[0]

def fmt(f):
    if f != f:
        raise Unsupported('NaN literal')
    if f in (float('inf'), float('-inf')):
        return '(%s1e38*1e38)' % ('-' if f < 0 else '')
    r = repr(float(f))
    if 'e' in r or 'E' in r:
        return r
    return r

def parse_sig(lines, title):
    """parse '// Input signature:' table -> list of dict(name, index, mask, reg, sysval, fmt, used)"""
    rows = []; inside = False
    for l in lines:
        s = l.strip()
        if s.startswith('// ' + title):
            inside = True; continue
        if inside:
            c = s[2:].strip() if s.startswith('//') else None
            if c is None:
                break
            if not c or c.startswith('Name') or c.startswith('---'):
                if rows and not c:
                    break
                continue
            parts = c.split()
            if len(parts) < 6:
                break
            name, idx, mask, reg, sysv, fm = parts[:6]
            used = parts[6] if len(parts) > 6 else ''
            rows.append({'name': name, 'index': int(idx), 'mask': mask, 'reg': int(reg) if reg.isdigit() else reg, 'sysval': sysv, 'fmt': fm, 'used': used})
    return rows

class Op:
    def __init__(self, text):
        self.text = text
        m = re.match(r'^(-?)(\|?)(.*?)(\|?)$', text)
        self.neg = text.startswith('-')
        t = text[1:] if self.neg else text
        self.abs = t.startswith('|') and t.endswith('|')
        if self.abs:
            t = t[1:-1]
        self.core = t
        self.kind = None; self.swz = None
        mm = re.match(r'^l\((.*)\)$', t)
        if mm:
            self.kind = 'lit'
            self.vals = [hexf(x) for x in split_top(mm.group(1))]
            return
        mm = re.match(r'^(cb)(\d+)\[(.+?)\](?:\.([xyzw]+))?$', t)
        if mm:
            self.kind = 'cb'; self.cbn = int(mm.group(2)); idx = mm.group(3).strip()
            self.swz = mm.group(4) or 'xyzw'
            if re.match(r'^\d+$', idx):
                self.cbi = int(idx); self.dyn = None
            else:
                self.cbi = None; self.dyn = idx
            return
        mm = re.match(r'^icb\[(.+?)\](?:\.([xyzw]+))?$', t)
        if mm:
            raise Unsupported('immediate constant buffer')
        mm = re.match(r'^([rvotsx])(\d+)(?:\.([xyzw]+))?$', t)
        if mm:
            self.kind = mm.group(1); self.n = int(mm.group(2)); self.swz = mm.group(3) or 'xyzw'
            return
        if t == 'oDepth':
            self.kind = 'odepth'; self.swz = 'x'; return
        if t == 'null':
            self.kind = 'null'; return
        raise Unsupported('operand %r' % text)

class Translator:
    def __init__(self, dis, stage):
        self.dis = dis
        self.stage = stage
        self.lines = dis.splitlines()
        self.isig = parse_sig(self.lines, 'Input signature')
        self.osig = parse_sig(self.lines, 'Output signature')
        self.cbs = {}      # n -> (size, dyn)
        self.tex = {}      # n -> type
        self.samp = {}     # n -> mode
        self.temps = 0
        self.body = []
        self.ind = 1
        self.boolc = set()  # (reg_name, comp) known bool masks
        self.approx = []
        self.cbused = {}   # n -> set(idx)

    # ------------------------------------------------ operand read
    def reg_name(self, op):
        if op.kind == 'r': return 'r%d' % op.n
        if op.kind == 'v': return 'v%d' % op.n
        if op.kind == 'o': return 'o%d' % op.n
        if op.kind == 'odepth': return 'oDepth'
        if op.kind == 'cb':
            if op.dyn is None:
                self.cbused.setdefault(op.cbn, set()).add(op.cbi)
                return 'cb%d_%d' % (op.cbn, op.cbi)
            m = re.match(r'^([rv])(\d+)\.([xyzw])\s*\+\s*(\d+)$', op.dyn)
            if not m:
                m2 = re.match(r'^([rv])(\d+)\.([xyzw])$', op.dyn)
                if not m2:
                    raise Unsupported('dyn index %r' % op.dyn)
                return 'cb%d[(int)%s%s.%s]' % (op.cbn, m2.group(1), m2.group(2), m2.group(3))
            return 'cb%d[(int)%s%s.%s + %s]' % (op.cbn, m.group(1), m.group(2), m.group(3), m.group(4))
        raise Unsupported('read kind %s' % op.kind)

    def read(self, op, comps):
        """comps: list of positions 0..3 selecting from the operand's swizzle; returns HLSL expr of len(comps)"""
        n = len(comps)
        if op.kind == 'lit':
            vals = op.vals
            sel = [vals[c] if len(vals) > 1 else vals[0] for c in comps]
            if n == 1:
                e = fmt(sel[0][0])
            else:
                e = 'float%d(%s)' % (n, ', '.join(fmt(v[0]) for v in sel))
        else:
            swz = op.swz
            chars = ''.join(swz[c] if len(swz) > 1 else swz[0] for c in comps)
            e = '%s.%s' % (self.reg_name(op), chars)
        if op.abs:
            e = 'abs(%s)' % e
        if op.neg:
            e = '(-%s)' % e
        return e

    def isbool(self, op, comps):
        if op.kind == 'lit':
            return False
        if op.kind not in ('r',):
            return False
        swz = op.swz
        return all(('r%d' % op.n, swz[c] if len(swz) > 1 else swz[0]) in self.boolc for c in comps)

    def lit_all(self, op, comps, bits):
        if op.kind != 'lit' or op.neg or op.abs:
            return False
        vals = op.vals
        return all((vals[c] if len(vals) > 1 else vals[0])[1] == bits for c in comps)

    def dst(self, op):
        if op.kind == 'null':
            return None, []
        mask = op.swz if op.kind != 'odepth' else 'x'
        pos = [COMP.index(c) for c in mask]
        if op.kind == 'odepth':
            return 'oDepth', [0]
        return '%s.%s' % (self.reg_name(op), mask), pos

    def emit(self, s):
        self.body.append('    ' * self.ind + s)

    def setbool(self, op, isb):
        if op.kind != 'r':
            return
        for c in op.swz:
            k = ('r%d' % op.n, c)
            if isb: self.boolc.add(k)
            else: self.boolc.discard(k)

    # ------------------------------------------------ main
    def run(self):
        started = False
        for raw in self.lines:
            l = raw.strip()
            if not l or l.startswith('//'):
                continue
            if re.match(r'^(vs|ps)_\d_\d$', l):
                started = True; continue
            if not started:
                continue
            self.inst(l)
        return self.build()

    def inst(self, l):
        m = re.match(r'^([A-Za-z_0-9]+)(\([^)]*\))*(?:\s+(.*))?$', l)
        if not m:
            raise Unsupported('line %r' % l)
        op = m.group(1)
        args = split_top(m.group(3)) if m.group(3) else []
        sat = False
        if op.endswith('_sat'):
            sat = True; op = op[:-4]
        if op == 'dcl_globalFlags':
            return
        if op == 'dcl_constantbuffer':
            mm = re.match(r'CB(\d+)\[(\d+)\]', args[0])
            self.cbs[int(mm.group(1))] = (int(mm.group(2)), 'dynamicIndexed' in l)
            return
        if op == 'dcl_sampler':
            mm = re.match(r's(\d+)', args[0])
            self.samp[int(mm.group(1))] = args[1]
            if 'comparison' in args[1]:
                raise Unsupported('comparison sampler')
            return
        if op.startswith('dcl_resource_'):
            ty = op[len('dcl_resource_'):]
            mm = re.search(r't(\d+)\s*$', l)
            self.tex[int(mm.group(1))] = ty
            if ty not in ('texture2d', 'texturecube', 'texture3d'):
                raise Unsupported('resource %s' % ty)
            return
        if op in ('dcl_input', 'dcl_input_ps', 'dcl_output', 'dcl_output_siv', 'dcl_input_sgv', 'dcl_input_siv', 'dcl_input_ps_siv', 'dcl_input_ps_sgv'):
            if 'instance_id' in l or 'vertex_id' in l:
                raise Unsupported('SV_InstanceID/VertexID')
            if 'noperspective' in l or 'constant' in l.split()[1:2]:
                pass
            if op == 'dcl_output_siv' and 'rendertarget_array_index' in l:
                raise Unsupported('rendertarget_array_index')
            if op == 'dcl_input_ps_siv' and 'position' in l:
                raise Unsupported('SV_Position read in PS (VPOS)')
            return
        if op == 'dcl_temps':
            self.temps = int(args[0]); return
        if op == 'dcl_indexableTemp':
            raise Unsupported('indexable temp')
        if op == 'dcl_immediateConstantBuffer':
            raise Unsupported('immediate constant buffer')
        if op.startswith('dcl_'):
            raise Unsupported(op)
        if op == 'ret':
            if self.ind > 1:
                self.emit('return;  /* early ret */')
                raise Unsupported('early return')
            return
        if op in ('loop', 'endloop', 'breakc_nz', 'breakc_z', 'break', 'continue', 'switch'):
            raise Unsupported('loop/switch flow control (%s)' % op)
        if op in ('if_nz', 'if_z'):
            a = Op(args[0])
            self.emit('if (%s %s 0) {' % (self.read(a, [0]), '!=' if op == 'if_nz' else '=='))
            self.ind += 1; return
        if op == 'else':
            self.ind -= 1; self.emit('} else {'); self.ind += 1; return
        if op == 'endif':
            self.ind -= 1; self.emit('}'); return
        if op in ('discard_nz', 'discard_z'):
            a = Op(args[0])
            self.emit('if (%s %s 0) discard;' % (self.read(a, [0]), '!=' if op == 'discard_nz' else '=='))
            return
        if op == 'sincos':
            ds, dc, a = Op(args[0]), Op(args[1]), Op(args[2])
            for d, fn in ((ds, 'sin'), (dc, 'cos')):
                if d.kind == 'null': continue
                dn, pos = self.dst(d)
                self.emit('%s = %s(%s);' % (dn, fn, self.read(a, pos)))
                self.setbool(d, False)
            return
        ops = [Op(a) for a in args]
        d = ops[0]; src = ops[1:]
        dn, pos = self.dst(d)
        n = len(pos)
        rd = lambda o: self.read(o, pos)
        isb = False
        if op == 'mov': e = rd(src[0])
        elif op == 'add': e = '%s + %s' % (rd(src[0]), rd(src[1]))
        elif op == 'mul': e = '%s * %s' % (rd(src[0]), rd(src[1]))
        elif op == 'mad': e = '%s * %s + %s' % (rd(src[0]), rd(src[1]), rd(src[2]))
        elif op == 'div': e = '%s / %s' % (rd(src[0]), rd(src[1]))
        elif op == 'max': e = 'max(%s, %s)' % (rd(src[0]), rd(src[1]))
        elif op == 'min': e = 'min(%s, %s)' % (rd(src[0]), rd(src[1]))
        elif op == 'rsq': e = 'rsqrt(%s)' % rd(src[0])
        elif op == 'sqrt': e = 'sqrt(%s)' % rd(src[0])
        elif op == 'rcp': e = '1.0 / %s' % rd(src[0])
        elif op == 'exp': e = 'exp2(%s)' % rd(src[0])
        elif op == 'log': e = 'log2(%s)' % rd(src[0])
        elif op == 'frc': e = 'frac(%s)' % rd(src[0])
        elif op == 'round_ni': e = 'floor(%s)' % rd(src[0])
        elif op == 'round_pi': e = 'ceil(%s)' % rd(src[0])
        elif op == 'round_z': e = 'trunc(%s)' % rd(src[0])
        elif op == 'round_ne': e = 'round(%s)' % rd(src[0])
        elif op in ('dp2', 'dp3', 'dp4'):
            k = int(op[2])
            e = 'dot(%s, %s)' % (self.read(src[0], list(range(k))), self.read(src[1], list(range(k))))
        elif op in ('lt', 'ge', 'eq', 'ne'):
            sym = {'lt': '<', 'ge': '>=', 'eq': '==', 'ne': '!='}[op]
            t = 'float' if n == 1 else 'float%d' % n
            e = '(%s)(%s %s %s)' % (t, rd(src[0]), sym, rd(src[1]))
            isb = True
        elif op == 'movc':
            e = '(%s != 0) ? %s : %s' % (rd(src[0]), rd(src[1]), rd(src[2]))
            if n > 1:
                t = 'float%d' % n
                e = '(%s != 0) ? (%s)(%s) : (%s)(%s)' % (rd(src[0]), t, rd(src[1]), t, rd(src[2]))
            isb = self.isbool(src[1], pos) and self.isbool(src[2], pos)
        elif op == 'and':
            a, b = src
            if self.lit_all(b, pos, 0x3f800000) or self.lit_all(a, pos, 0x3f800000):
                x = a if b.kind == 'lit' else b
                e = '(%s != 0) ? 1.0 : 0.0' % rd(x) if n == 1 else '(float%d)(%s != 0)' % (n, rd(x))
                isb = True
            elif self.lit_all(b, pos, 0x7fffffff):
                e = 'abs(%s)' % rd(a)
            elif self.isbool(a, pos) and self.isbool(b, pos):
                e = '%s * %s' % (rd(a), rd(b)); isb = True
            elif self.isbool(a, pos):
                e = '(%s != 0) ? %s : 0' % (rd(a), rd(b)) if n == 1 else '(%s != 0) ? (float%d)(%s) : (float%d)0' % (rd(a), n, rd(b), n)
            elif self.isbool(b, pos):
                e = '(%s != 0) ? %s : 0' % (rd(b), rd(a)) if n == 1 else '(%s != 0) ? (float%d)(%s) : (float%d)0' % (rd(b), n, rd(a), n)
            else:
                raise Unsupported('and on non-bool values: %s' % self.text_of(op, args))
        elif op == 'or':
            a, b = src
            if self.isbool(a, pos) and self.isbool(b, pos):
                e = 'max(%s, %s)' % (rd(a), rd(b)); isb = True
            else:
                raise Unsupported('or on non-bool')
        elif op == 'not':
            if self.isbool(src[0], pos):
                e = '1.0 - %s' % rd(src[0]); isb = True
            else:
                raise Unsupported('not on non-bool')
        elif op in ('sample', 'sample_b', 'sample_l', 'sample_indexable', 'sample_l_indexable', 'sample_b_indexable'):
            base = op.replace('_indexable', '')
            coord, t, s = src[0], src[1], src[2]
            ty = self.tex[t.n]
            k = 2 if ty == 'texture2d' else 3
            c = self.read(coord, list(range(k)))
            if base == 'sample':
                call = 't%d.Sample(s%d, %s)' % (t.n, s.n, c)
            elif base == 'sample_b':
                call = 't%d.SampleBias(s%d, %s, %s)' % (t.n, s.n, c, self.read(src[3], [0]))
            else:
                if self.stage == 'vs':
                    raise Unsupported('vertex texture fetch')
                if getattr(self, 'lod_mode', 'keep') == 'keep':
                    call = 't%d.SampleLevel(s%d, %s, %s)' % (t.n, s.n, c, self.read(src[3], [0]))
                else:
                    # approximation: SampleLevel(lod) -> SampleBias(lod) (texldb); exact when the texture has a single mip
                    call = 't%d.SampleBias(s%d, %s, %s)' % (t.n, s.n, c, self.read(src[3], [0]))
                    self.approx.append('sample_l -> SampleBias')
            tsw = t.swz
            e = '(%s).%s' % (call, ''.join(tsw[p] for p in pos))
        elif op in ('sample_c', 'sample_c_lz'):
            raise Unsupported('shadow compare sampling (%s)' % op)
        elif op in ('deriv_rtx', 'deriv_rty', 'deriv_rtx_coarse', 'deriv_rty_coarse'):
            raise Unsupported('derivatives (%s)' % op)
        else:
            raise Unsupported('opcode %s' % op)
        if sat:
            e = 'saturate(%s)' % e
        if dn is not None:
            self.emit('%s = %s;' % (dn, e))
        if d.kind == 'r':
            self.setbool(d, isb and not sat)

    def text_of(self, op, args):
        return op + ' ' + ', '.join(args)

    # ------------------------------------------------ assemble HLSL
    def build(self):
        out = []
        out.append('// generated by sm4tohlsl.py from Unity D3D11 bytecode; keep cbuffer slots/offsets, t#/s# slots, signatures')
        for n, (size, dyn) in sorted(self.cbs.items()):
            if dyn:
                out.append('cbuffer CB%d : register(b%d) { float4 cb%d[%d]; };' % (n, n, n, size))
                # alias names used for static reads
                self._dynarr = getattr(self, '_dynarr', set()) | {n}
            else:
                members = ' '.join('float4 cb%d_%d : packoffset(c%d);' % (n, i, i) for i in range(size))
                out.append('cbuffer CB%d : register(b%d) { %s };' % (n, n, members))
        for n, mode in sorted(self.samp.items()):
            out.append('SamplerState s%d : register(s%d);' % (n, n))
        for n, ty in sorted(self.tex.items()):
            tn = {'texture2d': 'Texture2D<float4>', 'texturecube': 'TextureCube<float4>', 'texture3d': 'Texture3D<float4>'}[ty]
            out.append('%s t%d : register(t%d);' % (tn, n, n))
        # signatures
        def semname(r):
            nm = r['name']; u = nm.upper()
            if u == 'SV_POSITION': return 'SV_POSITION'
            if u == 'SV_TARGET': return 'SV_Target%d' % r['index']
            if u == 'SV_DEPTH': return 'SV_Depth'
            if u.startswith('SV_'): raise Unsupported('system value %s' % nm)
            return '%s%d' % (nm, r['index'])
        def ctype(r):
            mask = r['mask']
            k = len(mask)
            ft = {'float': 'float', 'uint': 'uint', 'int': 'int'}.get(r['fmt'], 'float')
            if ft != 'float' and r['name'].upper() not in ('SV_POSITION',):
                raise Unsupported('integer signature element %s' % r['name'])
            return ft if k == 1 else '%s%d' % (ft, k)
        out.append('struct IN_T {')
        for i, r in enumerate(self.isig):
            out.append('    %s i%d : %s;' % (ctype(r), i, semname(r)))
        out.append('};')
        out.append('struct OUT_T {')
        for i, r in enumerate(self.osig):
            sn = semname(r)
            if r['name'].upper() == 'SV_DEPTH':
                sn = 'SV_Depth'
            out.append('    %s o%d : %s;' % (ctype(r) if r['mask'] != 'N/A' else 'float', i, sn))
        out.append('};')
        out.append('OUT_T main(IN_T IN) {')
        # registers
        vregs = sorted(set(r['reg'] for r in self.isig if isinstance(r['reg'], int)))
        oregs = sorted(set(r['reg'] for r in self.osig if isinstance(r['reg'], int)))
        for v in vregs:
            out.append('    float4 v%d = 0;' % v)
        for i, r in enumerate(self.isig):
            if not isinstance(r['reg'], int):
                continue
            if self.stage == 'ps' and r['name'].upper() == 'SV_POSITION':
                continue  # never read (VPOS unsupported); keeps the signature only
            out.append('    v%d.%s = IN.i%d;' % (r['reg'], r['mask'], i))
        for o in oregs:
            out.append('    float4 o%d = 0;' % o)
        if any(r['name'].upper() == 'SV_DEPTH' for r in self.osig):
            out.append('    float4 oDepth = 0;')
        for t in range(self.temps):
            out.append('    float4 r%d = 0;' % t)
        out.extend(self.body)
        out.append('    OUT_T OUT;')
        for i, r in enumerate(self.osig):
            if r['name'].upper() == 'SV_DEPTH':
                out.append('    OUT.o%d = oDepth.x;' % i)
            else:
                out.append('    OUT.o%d = o%d.%s;' % (i, r['reg'], r['mask']))
        out.append('    return OUT;')
        out.append('}')
        src = '\n'.join(out) + '\n'
        if getattr(self, '_dynarr', None):
            # static reads of a dynamically indexed cbuffer must use the array
            for n in self._dynarr:
                src = re.sub(r'\bcb%d_(\d+)\b' % n, lambda m: 'cb%d[%s]' % (n, m.group(1)), src)
        return src

def translate(dis, stage, lod_mode='keep'):
    t = Translator(dis, stage)
    t.lod_mode = lod_mode
    src = t.run()
    return src, t
