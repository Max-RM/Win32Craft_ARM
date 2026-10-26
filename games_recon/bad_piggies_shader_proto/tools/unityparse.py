"""Minimal Unity 2018.4 serialized-file / UnityFS / Shader parser (pure Python).
Read-only. Written for the Bad Piggies Rebooted FL9_1 shader study."""
import struct, io, os, lzma

# ---------------------------------------------------------------- LZ4 block
def lz4_block_decompress(src, out_size):
    dst = bytearray()
    i = 0
    n = len(src)
    while i < n:
        token = src[i]; i += 1
        lit = token >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1
                lit += b
                if b != 255:
                    break
        dst += src[i:i + lit]; i += lit
        if i >= n:
            break
        off = src[i] | (src[i + 1] << 8); i += 2
        ml = token & 15
        if ml == 15:
            while True:
                b = src[i]; i += 1
                ml += b
                if b != 255:
                    break
        ml += 4
        start = len(dst) - off
        if off >= ml:
            dst += dst[start:start + ml]
        else:
            for k in range(ml):
                dst.append(dst[start + k])
    if out_size is not None and len(dst) != out_size:
        raise ValueError('lz4 size mismatch %d != %d' % (len(dst), out_size))
    return bytes(dst)

def lzma_unity(src, out_size):
    props = src[0]
    dict_size = struct.unpack_from('<I', src, 1)[0]
    lc = props % 9; rest = props // 9; lp = rest % 5; pb = rest // 5
    filt = [{'id': lzma.FILTER_LZMA1, 'dict_size': dict_size, 'lc': lc, 'lp': lp, 'pb': pb}]
    d = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filt)
    out = d.decompress(src[5:], out_size)
    return out[:out_size]

def decompress(flags, data, usize):
    c = flags & 0x3f
    if c == 0:
        return data
    if c == 1:
        return lzma_unity(data, usize)
    if c in (2, 3):
        return lz4_block_decompress(data, usize)
    raise ValueError('compression %d' % c)

# ---------------------------------------------------------------- readers
class R:
    def __init__(self, data, pos=0, be=False):
        self.d = data; self.p = pos; self.e = '>' if be else '<'
    def u8(self):
        v = self.d[self.p]; self.p += 1; return v
    def s8(self):
        v = struct.unpack_from('b', self.d, self.p)[0]; self.p += 1; return v
    def b(self):
        return self.u8() != 0
    def u16(self):
        v = struct.unpack_from(self.e + 'H', self.d, self.p)[0]; self.p += 2; return v
    def i16(self):
        v = struct.unpack_from(self.e + 'h', self.d, self.p)[0]; self.p += 2; return v
    def u32(self):
        v = struct.unpack_from(self.e + 'I', self.d, self.p)[0]; self.p += 4; return v
    def i32(self):
        v = struct.unpack_from(self.e + 'i', self.d, self.p)[0]; self.p += 4; return v
    def i64(self):
        v = struct.unpack_from(self.e + 'q', self.d, self.p)[0]; self.p += 8; return v
    def u64(self):
        v = struct.unpack_from(self.e + 'Q', self.d, self.p)[0]; self.p += 8; return v
    def f32(self):
        v = struct.unpack_from(self.e + 'f', self.d, self.p)[0]; self.p += 4; return v
    def raw(self, n):
        v = self.d[self.p:self.p + n]; self.p += n; return v
    def cstr(self):
        e = self.d.index(b'\0', self.p)
        v = self.d[self.p:e].decode('utf-8', 'replace'); self.p = e + 1; return v
    def align(self, a=4):
        self.p = (self.p + a - 1) & ~(a - 1)
    def astr(self):
        n = self.i32()
        if n < 0 or n > 10_000_000:
            raise ValueError('bad string len %d at %d' % (n, self.p - 4))
        v = self.d[self.p:self.p + n].decode('utf-8', 'replace'); self.p += n; self.align(); return v
    def arr(self, fn):
        n = self.i32()
        if n < 0 or n > 5_000_000:
            raise ValueError('bad array len %d at %d' % (n, self.p - 4))
        return [fn() for _ in range(n)]

# ---------------------------------------------------------------- UnityFS
def read_unityfs(path_or_bytes):
    data = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else open(path_or_bytes, 'rb').read()
    r = R(data, be=True)
    sig = r.cstr()
    assert sig == 'UnityFS', sig
    ver = r.u32(); r.cstr(); r.cstr()
    size = r.i64(); csize = r.u32(); usize = r.u32(); flags = r.u32()
    if ver >= 7:
        r.align(16)
    if flags & 0x80:
        binfo = data[len(data) - csize:]
    else:
        binfo = data[r.p:r.p + csize]; r.p += csize
    binfo = decompress(flags, binfo, usize)
    b = R(binfo, be=True)
    b.raw(16)
    nblocks = b.i32()
    blocks = [(b.u32(), b.u32(), b.u16()) for _ in range(nblocks)]
    nodes = []
    for _ in range(b.i32()):
        off = b.i64(); sz = b.i64(); fl = b.u32(); name = b.cstr()
        nodes.append((name, off, sz, fl))
    out = bytearray()
    for (us, cs, fl) in blocks:
        out += decompress(fl, data[r.p:r.p + cs], us); r.p += cs
    return {name: bytes(out[off:off + sz]) for (name, off, sz, fl) in nodes}

# ---------------------------------------------------------------- SerializedFile
def read_serialized(data):
    h = R(data, be=True)
    msize = h.u32(); fsize = h.u32(); ver = h.u32(); doff = h.u32()
    endian = h.u8(); h.raw(3)
    r = R(data, h.p, be=(endian != 0))
    uver = r.cstr()
    platform = r.i32()
    tt = r.b()
    types = []
    for _ in range(r.i32()):
        cid = r.i32(); stripped = r.b(); sti = r.i16()
        script = None
        if cid == 114:
            script = r.raw(16)
        oldhash = r.raw(16)
        if tt:
            nn = r.i32(); sb = r.i32()
            r.raw(nn * 24 + sb)
        types.append(cid)
    objs = []
    for _ in range(r.i32()):
        r.align(4)
        pid = r.i64(); start = r.u32() + doff; size = r.u32(); tidx = r.i32()
        objs.append((pid, start, size, types[tidx]))
    # script types
    for _ in range(r.i32()):
        r.i32(); r.align(4); r.i64()
    externals = []
    for _ in range(r.i32()):
        r.cstr(); r.raw(16); r.i32(); externals.append(r.cstr())
    return {'version': ver, 'unity': uver, 'platform': platform, 'objects': objs, 'externals': externals}

# ---------------------------------------------------------------- Shader (2018.4 layout)
def sfv(r):
    return (r.f32(), r.astr())

def ser_property(r):
    return {'name': r.astr(), 'desc': r.astr(), 'attrs': r.arr(r.astr), 'type': r.i32(), 'flags': r.u32(),
            'def': [r.f32() for _ in range(4)], 'deftex': (r.astr(), r.i32())}

def tagmap(r):
    return r.arr(lambda: (r.astr(), r.astr()))

def rtblend(r):
    return [sfv(r) for _ in range(7)]

def stencilop(r):
    return [sfv(r) for _ in range(4)]

def shader_state(r):
    s = {'name': r.astr()}
    s['rtBlend'] = [rtblend(r) for _ in range(8)]
    s['rtSeparateBlend'] = r.b(); r.align()
    s['zClip'] = sfv(r)
    s['zTest'] = sfv(r); s['zWrite'] = sfv(r); s['culling'] = sfv(r)
    s['offsetFactor'] = sfv(r); s['offsetUnits'] = sfv(r); s['alphaToMask'] = sfv(r)
    s['stencilOp'] = stencilop(r); s['stencilOpFront'] = stencilop(r); s['stencilOpBack'] = stencilop(r)
    s['stencilReadMask'] = sfv(r); s['stencilWriteMask'] = sfv(r); s['stencilRef'] = sfv(r)
    s['fogStart'] = sfv(r); s['fogEnd'] = sfv(r); s['fogDensity'] = sfv(r)
    s['fogColor'] = ([sfv(r) for _ in range(4)], r.astr())
    s['fogMode'] = r.i32(); s['gpuProgramID'] = r.i32()
    s['tags'] = tagmap(r); s['lod'] = r.i32()
    s['lighting'] = r.b(); r.align()
    return s

def vecparam(r):
    v = {'nameIndex': r.i32(), 'index': r.i32(), 'arraySize': r.i32(), 'type': r.s8(), 'dim': r.s8()}; r.align(); return v

def matparam(r):
    v = {'nameIndex': r.i32(), 'index': r.i32(), 'arraySize': r.i32(), 'type': r.s8(), 'rows': r.s8()}; r.align(); return v

def texparam(r):
    v = {'nameIndex': r.i32(), 'index': r.i32(), 'samplerIndex': r.i32(), 'multisampled': r.b(), 'dim': r.s8()}; r.align(); return v

def bufbind(r):
    return {'nameIndex': r.i32(), 'index': r.i32()}

def structparam(r):
    return {'nameIndex': r.i32(), 'index': r.i32(), 'arraySize': r.i32(), 'structSize': r.i32(),
            'vec': r.arr(lambda: vecparam(r)), 'mat': r.arr(lambda: matparam(r))}

def cbuffer(r):
    return {'nameIndex': r.i32(), 'mat': r.arr(lambda: matparam(r)), 'vec': r.arr(lambda: vecparam(r)),
            'struct': r.arr(lambda: structparam(r)), 'size': r.i32()}

def subprogram(r):
    sp = {'blobIndex': r.u32()}
    ch = r.arr(lambda: (r.s8(), r.s8())); r.align()
    sp['channels'] = ch; sp['sourceMap'] = r.u32()
    sp['keywordIndices'] = r.arr(r.u16); r.align()
    sp['tier'] = r.s8(); sp['gpuProgramType'] = r.s8(); r.align()
    sp['vec'] = r.arr(lambda: vecparam(r))
    sp['mat'] = r.arr(lambda: matparam(r))
    sp['tex'] = r.arr(lambda: texparam(r))
    sp['bufparams'] = r.arr(lambda: bufbind(r))
    sp['cbs'] = r.arr(lambda: cbuffer(r))
    sp['cbbind'] = r.arr(lambda: bufbind(r))
    sp['uav'] = r.arr(lambda: (r.i32(), r.i32(), r.i32()))
    sp['samplers'] = r.arr(lambda: (r.u32(), r.i32()))
    sp['requirements'] = r.i32()
    return sp

def program(r):
    return r.arr(lambda: subprogram(r))

def ser_pass(r):
    p = {'nameIndices': r.arr(lambda: (r.astr(), r.i32()))}
    p['type'] = r.i32()
    p['state'] = shader_state(r)
    p['programMask'] = r.u32()
    p['vs'] = program(r); p['ps'] = program(r); p['gs'] = program(r); p['hs'] = program(r); p['ds'] = program(r)
    p['hasInstancing'] = r.b(); p['hasProcInstancing'] = r.b(); r.align()
    p['useName'] = r.astr(); p['name'] = r.astr(); p['textureName'] = r.astr()
    p['tags'] = tagmap(r)
    return p

def subshader(r):
    return {'passes': r.arr(lambda: ser_pass(r)), 'tags': tagmap(r), 'lod': r.i32()}

def parse_shader(data, start, size):
    r = R(data, start)
    sh = {'objName': r.astr()}
    pf = {}
    pf['props'] = r.arr(lambda: ser_property(r))
    pf['subshaders'] = r.arr(lambda: subshader(r))
    pf['name'] = r.astr(); pf['customEditor'] = r.astr(); pf['fallback'] = r.astr()
    pf['deps'] = r.arr(lambda: (r.astr(), r.astr()))
    pf['disableNoSubshadersMessage'] = r.b(); r.align()
    sh['parsed'] = pf
    sh['platforms'] = r.arr(r.u32)
    sh['offsets'] = r.arr(r.u32)
    sh['clen'] = r.arr(r.u32)
    sh['dlen'] = r.arr(r.u32)
    n = r.i32(); sh['blob'] = r.raw(n); r.align()
    sh['deps'] = r.arr(lambda: (r.i32(), r.i64()))
    sh['nonModTex'] = r.arr(lambda: (r.astr(), r.i32(), r.i64()))
    sh['baked'] = r.b(); r.align()
    sh['consumed'] = r.p - start
    sh['size'] = size
    return sh

def shader_platform_blobs(sh):
    """returns list of (platform, [subprogram dicts])"""
    res = []
    for i, plat in enumerate(sh['platforms']):
        comp = sh['blob'][sh['offsets'][i]:sh['offsets'][i] + sh['clen'][i]]
        dec = lz4_block_decompress(comp, sh['dlen'][i])
        r = R(dec)
        cnt = r.i32()
        ents = [(r.u32(), r.u32()) for _ in range(cnt)]
        subs = []
        for (off, ln) in ents:
            s = R(dec, off)
            sp = {'version': s.i32(), 'progType': s.i32()}
            sp['stats'] = [s.i32() for _ in range(3)]
            if sp['version'] >= 201608170:
                sp['stats'].append(s.i32())
            sp['keywords'] = s.arr(s.astr)
            if sp['version'] >= 201806140:
                sp['localKeywords'] = s.arr(s.astr)
            n = s.i32(); sp['code'] = dec[s.p:s.p + n]; s.p += n; s.align()
            sp['tail'] = dec[s.p:off + ln]
            sp['off'] = off; sp['len'] = ln
            subs.append(sp)
        res.append((plat, subs, dec))
    return res

def name_of(data, start):
    r = R(data, start)
    return r.astr()
