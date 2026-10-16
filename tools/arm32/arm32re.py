#!/usr/bin/env python3
"""arm32re - reverse-engineering helper for the Minecraft 1.2.8 ARM32 (Thumb-2) UWP exe.

Usage (all RVAs/VAs hex; code RVAs are even, Thumb bit is stripped for you):

  arm32re.py info                      PE summary
  arm32re.py func  <rva> [-n N]        disassemble the function containing <rva> (pdata bounds), annotated
  arm32re.py dis   <rva> [count]       disassemble `count` instructions (default 40) starting at <rva>
  arm32re.py callers <rva>             direct BL/BLX/B(tail) sites that target <rva>
  arm32re.py calls <rva>               annotated list of direct callees (and constants used) in function
  arm32re.py xref  <rva>               every reference to the address <rva> (movw/movt pairs, literal-pool
                                       loads, data pointers).  Code sites are mapped to their function.
  arm32re.py str   <text> [-w] [-i]    search ASCII (default) or UTF-16 (-w) strings, prints RVA + xref count
  arm32re.py strx  <text> [-w]         like `str` but also lists the functions that reference each hit
  arm32re.py rtti  <class-substring>   RTTI lookup: type descriptor, COL, vtable RVA(s) + slots
  arm32re.py vtable <rva> [n]          dump n (default 40) pointer slots with annotations
  arm32re.py ptrs  <rva> [n]           dump n dwords as pointers/ints
  arm32re.py bytes <rva> [n]           hex dump
  arm32re.py imports [substr]          import + delay-import thunks with IAT RVAs
  arm32re.py impcallers <substr>       who calls an import (via the movw/movt/ldr pc thunks) - e.g. D3DCompile
  arm32re.py exports [substr]
  arm32re.py find  <hex bytes>         binary pattern search (e.g. "00 b5 01 46")
  arm32re.py guid  <xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx>   search a GUID in memory layout
  arm32re.py imm   <value> [--range A B]  functions containing an immediate operand equal to value
  arm32re.py funcs <rva-lo> <rva-hi>   list pdata functions in a range
  arm32re.py name  <rva> <label>       record a label (appended to names.jsonl; shown in later output)
  arm32re.py names                     dump recorded labels

Environment: ARM32_EXE overrides the exe path.  Caches live in tools/arm32/.cache (rebuilt on demand).
"""
import sys, os, re, struct, bisect, pickle, json, argparse, time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
EXE = os.environ.get("ARM32_EXE", os.path.join(REPO, "Minecraft_1.2.8.0_ARM32", "Minecraft.Windows.exe"))
CACHE = os.path.join(HERE, ".cache")
# labels (`name` command) are per analysed image: RVAs of different game versions / DLLs must never mix
_NAMES_DIR = os.path.join(HERE, "names")
os.makedirs(_NAMES_DIR, exist_ok=True)
NAMES = os.path.join(_NAMES_DIR, os.path.basename(os.path.dirname(os.path.abspath(EXE))) + "__" + os.path.basename(EXE) + ".jsonl")


def hx(s):
    return int(s, 16) if isinstance(s, str) else s


class Pe:
    def __init__(self, path=EXE):
        self.path = path
        self.raw = open(path, "rb").read()
        r = self.raw
        pe = struct.unpack_from("<I", r, 0x3C)[0]
        assert r[pe:pe + 4] == b"PE\0\0"
        self.machine = struct.unpack_from("<H", r, pe + 4)[0]
        nsec = struct.unpack_from("<H", r, pe + 6)[0]
        osz = struct.unpack_from("<H", r, pe + 20)[0]
        opt = pe + 24
        self.pe_off, self.opt_off = pe, opt
        self.base = struct.unpack_from("<I", r, opt + 28)[0]
        self.size_of_image = struct.unpack_from("<I", r, opt + 56)[0]
        self.entry = struct.unpack_from("<I", r, opt + 16)[0]
        self.dd = [struct.unpack_from("<2I", r, opt + 96 + 8 * i) for i in range(16)]
        st = opt + osz
        self.sections = []
        for i in range(nsec):
            h = st + 40 * i
            name = r[h:h + 8].rstrip(b"\0").decode()
            vs, va, rs, ro, = struct.unpack_from("<4I", r, h + 8)
            chars = struct.unpack_from("<I", r, h + 36)[0]
            self.sections.append(dict(name=name, va=va, vs=vs, ro=ro, rs=rs, ch=chars))
        img = bytearray(self.size_of_image)
        hdr = struct.unpack_from("<I", r, opt + 60)[0]
        img[:hdr] = r[:hdr]
        for s in self.sections:
            n = min(s["rs"], s["vs"]) if s["vs"] else s["rs"]
            img[s["va"]:s["va"] + n] = r[s["ro"]:s["ro"] + n]
        self.img = img
        self.text = next((s for s in self.sections if s["name"] == ".text"), None)
        # forwarder-only DLLs (api-set shims) have no code section
        self.text_lo = self.text["va"] if self.text else 0
        self.text_hi = (self.text["va"] + self.text["vs"]) if self.text else 0

    # --- access
    def sec_of(self, rva):
        for s in self.sections:
            if s["va"] <= rva < s["va"] + max(s["vs"], s["rs"]):
                return s
        return None

    def u8(self, rva): return self.img[rva]
    def u16(self, rva): return struct.unpack_from("<H", self.img, rva)[0]
    def u32(self, rva): return struct.unpack_from("<I", self.img, rva)[0]
    def bytes(self, rva, n): return bytes(self.img[rva:rva + n])

    def cstr(self, rva, maxn=512):
        e = self.img.find(b"\0", rva, rva + maxn)
        return bytes(self.img[rva:e if e >= 0 else rva + maxn])

    def wstr(self, rva, maxn=512):
        out = bytearray()
        for i in range(0, maxn, 2):
            c = self.img[rva + i:rva + i + 2]
            if len(c) < 2 or c == b"\0\0": break
            out += c
        return bytes(out).decode("utf-16le", "replace")

    def in_text(self, rva): return self.text_lo <= rva < self.text_hi

    # --- import tables
    def imports(self):
        res = []
        rva, size = self.dd[1]
        if not rva: return res
        p = rva
        while True:
            oft, ts, fc, name, ft = struct.unpack_from("<5I", self.img, p)
            if not (oft or name or ft): break
            dll = self.cstr(name).decode()
            thunk = oft or ft
            i = 0
            while True:
                e = self.u32(thunk + 4 * i)
                if not e: break
                if e & 0x80000000:
                    res.append(dict(dll=dll, name="#%d" % (e & 0xFFFF), iat=ft + 4 * i, delay=False))
                else:
                    res.append(dict(dll=dll, name=self.cstr(e + 2).decode(), iat=ft + 4 * i, delay=False))
                i += 1
            p += 20
        # delay imports
        rva, size = self.dd[13]
        if rva:
            p = rva
            while True:
                attrs, dllname, hmod, iat, int_, bound, unload, ts = struct.unpack_from("<8I", self.img, p)
                if not dllname: break
                dll = self.cstr(dllname).decode()
                i = 0
                while True:
                    e = self.u32(int_ + 4 * i)
                    if not e: break
                    nm = ("#%d" % (e & 0xFFFF)) if e & 0x80000000 else self.cstr(e + 2).decode()
                    res.append(dict(dll=dll, name=nm, iat=iat + 4 * i, delay=True))
                    i += 1
                p += 32
        return res

    def exports(self):
        rva, size = self.dd[0]
        if not rva: return []
        (_, _, _, _, _, base, nfunc, nnames, aof, aon, aoo) = struct.unpack_from("<IIHHIIIIIII", self.img, rva)
        res = []
        for i in range(nnames):
            nm = self.cstr(self.u32(aon + 4 * i)).decode()
            ordn = self.u16(aoo + 2 * i)
            res.append((nm, self.u32(aof + 4 * ordn)))
        return res

    def relocs(self):
        rva, size = self.dd[5]
        p, end = rva, rva + size
        out = []
        while p < end:
            page, blk = struct.unpack_from("<2I", self.img, p)
            if blk < 8: break
            for i in range((blk - 8) // 2):
                e = self.u16(p + 8 + 2 * i)
                t, o = e >> 12, e & 0xFFF
                if t: out.append((page + o, t))
            p += blk
        return out

    def pdata(self):
        rva, size = self.dd[3]
        out = []
        for i in range(size // 8):
            b, u = struct.unpack_from("<2I", self.img, rva + 8 * i)
            if not b: continue
            flag = u & 3
            if flag == 0:
                hdr = self.u32(u)
                ln = (hdr & 0x3FFFF) * 2
            else:
                ln = ((u >> 2) & 0x7FF) * 2
            out.append((b & ~1, ln, u))
        out.sort()
        return out


def decode_movw(w1, w2):
    imm4 = w1 & 0xF; i = (w1 >> 10) & 1; imm3 = (w2 >> 12) & 7; imm8 = w2 & 0xFF; rd = (w2 >> 8) & 0xF
    return rd, (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8


class Index:
    """Cross-reference index built from relocations + a capstone sweep (cached)."""
    VER = 4

    def __init__(self, pe):
        self.pe = pe
        os.makedirs(CACHE, exist_ok=True)
        path = os.path.join(CACHE, "index_v%d.pkl" % self.VER)
        st = os.stat(pe.path)
        key = (pe.path, st.st_size, int(st.st_mtime))
        if os.path.exists(path):
            try:
                d = pickle.load(open(path, "rb"))
                if d.get("key") == key:
                    self.__dict__.update(d["data"]); return
            except Exception:
                pass
        t0 = time.time()
        sys.stderr.write("[arm32re] building index (one-off, ~1-3 min)...\n")
        self.build()
        pickle.dump(dict(key=key, data={k: v for k, v in self.__dict__.items() if k != "pe"}), open(path, "wb"), protocol=4)
        sys.stderr.write("[arm32re] index built in %.0fs\n" % (time.time() - t0))

    def build(self):
        pe = self.pe
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
        md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
        self.funcs = [(b, l) for (b, l, u) in pe.pdata()]
        self.fstarts = [b for b, l in self.funcs]
        rel = pe.relocs()
        self.mov32 = {}     # VA -> [site rva]     (movw/movt pair, reloc type 7)
        self.mov32_reg = {}  # site -> reg
        self.ptr = {}       # VA -> [location rva] (dword pointers: data + literal pools)
        pool_locs = set()
        for rva, t in rel:
            if t == 7:
                w1, w2, w3, w4 = struct.unpack_from("<4H", pe.img, rva)
                rd, lo = decode_movw(w1, w2)
                rd2, hi = decode_movw(w3, w4)
                va = (hi << 16) | lo
                self.mov32.setdefault(va, []).append(rva)
                self.mov32_reg[rva] = rd
            elif t == 3:
                va = pe.u32(rva)
                self.ptr.setdefault(va, []).append(rva)
                if pe.in_text(rva): pool_locs.add(rva)
        self.pool_locs = pool_locs
        self.callers = {}   # target rva -> [(site, kind)]
        self.litref = {}    # pool dword rva -> [site rva]
        img = pe.img
        pl = sorted(pool_locs)
        for fb, fl in self.funcs:
            lo, hi = fb, fb + fl
            # split out relocated pool words
            i = bisect.bisect_left(pl, lo)
            segs = []; cur = lo
            while i < len(pl) and pl[i] < hi:
                if pl[i] > cur: segs.append((cur, pl[i]))
                cur = pl[i] + 4
                i += 1
            if cur < hi: segs.append((cur, hi))
            for (a, b) in segs:
                addr = a
                while addr < b:
                    got = False
                    for (ia, isz, mn, ops) in md.disasm_lite(bytes(img[addr:b]), addr):
                        got = True
                        addr = ia + isz
                        c = mn[0]
                        if c == 'b':
                            if ops.startswith('#0x') and mn in ('bl', 'b', 'b.w', 'blx'):
                                tgt = int(ops[1:], 16)
                                kind = 'call' if mn in ('bl', 'blx') else 'jmp'
                                if kind == 'call' or not (fb <= tgt < fb + fl):
                                    self.callers.setdefault(tgt & ~1, []).append((ia, kind))
                        elif c == 'l' and mn.startswith('ldr') and '[pc' in ops:
                            m = re.search(r'#(-?)0x([0-9a-f]+)\]', ops)
                            if m:
                                off = int(m.group(2), 16) * (-1 if m.group(1) else 1)
                            else:
                                off = 0
                            pool = ((ia + 4) & ~3) + off
                            self.litref.setdefault(pool, []).append(ia)
                        elif c == 'a' and mn.startswith('adr'):
                            m = re.search(r'#(-?)0x([0-9a-f]+)', ops)
                            if m:
                                tg = ((ia + 4) & ~3) + int(m.group(2), 16) * (-1 if m.group(1) else 1)
                                self.mov32.setdefault(pe.base + tg, []).append(ia)  # adr-style ref (pc-relative)
                    if not got:
                        addr += 2
                    elif addr < b:
                        # capstone stopped on undecodable bytes: skip them
                        addr += 2
        # RTTI
        self.rtti = {}
        self.vtable_at = {}
        td = []
        data = bytes(img)
        for m in re.finditer(rb"\.\?A[VU][ -~]{1,300}?@@\x00", data):
            td.append((m.start(), m.group(0)[:-1].decode()))
        for srva, name in td:
            tdva = pe.base + srva - 8
            for loc in self.ptr.get(tdva, []):
                if loc - 12 < 0: continue
                if pe.u32(loc - 12) != 0: continue  # COL.signature == 0
                colva = pe.base + loc - 12
                for vloc in self.ptr.get(colva, []):
                    self.rtti.setdefault(name, []).append((vloc + 4, pe.u32(loc - 8)))
        for name, lst in self.rtti.items():
            for (v, off) in lst:
                self.vtable_at[v] = name
        # import thunks: movw ip / movt ip / ldr.w pc,[ip]
        self.thunks = {}
        imp_slots = {i["iat"] for i in pe.imports()}
        for va, sites in self.mov32.items():
            r = va - pe.base
            if r in imp_slots:
                for site in sites:
                    if pe.in_text(site) and bytes(img[site + 8:site + 12]) == bytes((0xdc, 0xf8, 0x00, 0xf0)):
                        self.thunks[site] = r
        # automatic names for virtual functions: Class::vfN (first class wins; plain classes preferred)
        self.vfn_names = {}
        def simple(n):
            m = re.match(r"\.\?A[VU](.*)@@$", n)
            if not m: return n
            body = m.group(1)
            if body.startswith("?$"):
                return body[:60]
            return "::".join(reversed(body.split("@")))
        for name, lst in sorted(self.rtti.items(), key=lambda kv: ("?$" in kv[0], kv[0])):
            nm = simple(name)
            for (v, off) in lst:
                i = 0
                while True:
                    t = pe.u32(v + 4 * i)
                    if (t & 1) and pe.in_text((t & ~1) - pe.base) and (t & ~1) - pe.base in self_func_starts(self):
                        self.vfn_names.setdefault((t & ~1) - pe.base, "%s::vf%d" % (nm, i))
                        i += 1
                    else:
                        break
                    if i > 600: break


def self_func_starts(idx):
    fs = getattr(idx, "_fset", None)
    if fs is None:
        fs = idx._fset = set(idx.fstarts)
    return fs


def thumb_dis(pe, rva, count=40, end=None, idx=None, ann=None):
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = False
    lines = []
    n = 0
    addr = rva
    limit = end if end else pe.text_hi
    while addr < limit and n < count:
        got = False
        for (ia, isz, mn, ops) in md.disasm_lite(bytes(pe.img[addr:min(limit, addr + 4 * count + 64)]), addr):
            got = True
            raw = pe.bytes(ia, isz).hex()
            note = ann(ia, mn, ops) if ann else ""
            lines.append("%08x: %-8s %-8s %-28s%s" % (ia, raw, mn, ops, ("  ; " + note) if note else ""))
            addr = ia + isz
            n += 1
            if n >= count or addr >= limit: break
        if not got:
            lines.append("%08x: %-8s .word/data" % (addr, pe.bytes(addr, 2).hex()))
            addr += 2; n += 1
    return lines


class Ctx:
    def __init__(self):
        self.pe = Pe()
        self._idx = None
        self.names = {}
        if os.path.exists(NAMES):
            for ln in open(NAMES, encoding="utf-8"):
                ln = ln.strip()
                if not ln: continue
                try:
                    d = json.loads(ln); self.names[d["rva"]] = d["label"]
                except Exception:
                    pass
        self._imp = None

    @property
    def idx(self):
        if self._idx is None: self._idx = Index(self.pe)
        return self._idx

    @property
    def imp(self):
        if self._imp is None:
            self._imp = {i["iat"]: i for i in self.pe.imports()}
        return self._imp

    def func_of(self, rva):
        fs = self.idx.fstarts
        i = bisect.bisect_right(fs, rva) - 1
        if i < 0: return None
        b, l = self.idx.funcs[i]
        if rva < b + l: return (b, l)
        if rva in self.idx.thunks: return (rva, 12)
        return None

    def fname(self, rva):
        rva &= ~1
        if rva in self.names: return self.names[rva]
        t = self.idx.thunks.get(rva)
        if t is not None:
            i = self.imp.get(t)
            return "thunk[%s!%s]" % (i["dll"], i["name"]) if i else "thunk[%x]" % t
        return getattr(self.idx, "vfn_names", {}).get(rva) or "sub_%x" % rva

    def describe(self, va):
        pe = self.pe
        if va & 0xFFFF0000 == 0 and va < 0x10000: return ""
        rva = (va & ~1) - pe.base
        if not (0 <= rva < pe.size_of_image): return ""
        if rva in self.imp:
            i = self.imp[rva]; return "IAT %s!%s" % (i["dll"], i["name"])
        va_is_fn = (va & 1) and pe.in_text(rva)
        if pe.in_text(rva) and ((va & 1) or self.func_of(rva) and self.func_of(rva)[0] == rva):
            return "fn %s" % self.fname(rva)
        if pe.in_text(rva):
            return "code %s+0x%x" % (self.fname(self.func_of(rva)[0]) if self.func_of(rva) else "?", rva - (self.func_of(rva)[0] if self.func_of(rva) else rva))
        va0 = va - pe.base
        v = self.idx.vtable_at.get(rva)
        if v: return "vtable %s" % v
        # inside a vtable?
        s = self.pe.sec_of(rva)
        if s is None: return ""
        # strings
        b = pe.bytes(rva, 80)
        m = re.match(rb"[\x20-\x7e\t\r\n]{3,}", b)
        if m and (len(m.group(0)) >= 3 and (len(b) == len(m.group(0)) or b[len(m.group(0))] == 0)):
            return 'str "%s"' % m.group(0).decode()[:60].replace("\n", "\\n")
        w = pe.wstr(rva, 80)
        if len(w) >= 3 and all(32 <= ord(ch) < 127 for ch in w):
            return 'wstr L"%s"' % w[:60]
        # which vtable contains it
        return "%s+0x%x" % (s["name"], rva - s["va"])

    def annotator(self):
        pe = self.pe
        idx = self.idx

        def ann(ia, mn, ops):
            if mn in ("bl", "blx", "b", "b.w") and ops.startswith("#0x"):
                t = int(ops[1:], 16)
                return self.fname(t)
            if mn == "movw" and ia in idx.mov32_reg:
                w1, w2, w3, w4 = struct.unpack_from("<4H", pe.img, ia)
                _, lo = decode_movw(w1, w2); _, hi = decode_movw(w3, w4)
                va = (hi << 16) | lo
                return "-> 0x%08x %s" % (va, self.describe(va))
            if mn.startswith("ldr") and "[pc" in ops:
                m = re.search(r"#(-?)0x([0-9a-f]+)\]", ops)
                off = (int(m.group(2), 16) * (-1 if m.group(1) else 1)) if m else 0
                pool = ((ia + 4) & ~3) + off
                v = pe.u32(pool)
                d = self.describe(v) if pool in idx.pool_locs else ""
                return "pool[%08x]=0x%08x %s" % (pool, v, d)
            if mn.startswith("adr"):
                m = re.search(r"#(-?)0x([0-9a-f]+)", ops)
                if m:
                    tg = ((ia + 4) & ~3) + int(m.group(2), 16) * (-1 if m.group(1) else 1)
                    return "-> %08x %s" % (tg, self.describe(pe.base + tg))
            return ""
        return ann


def find_all(buf, pat):
    out = []; i = 0
    while True:
        i = buf.find(pat, i)
        if i < 0: break
        out.append(i); i += 1
    return out


def xrefs_to(ctx, rva):
    """all references to address rva (as VA = base+rva, also with Thumb bit)."""
    idx = ctx.idx; pe = ctx.pe
    va = pe.base + rva
    res = []
    for v in (va, va | 1):
        for site in idx.mov32.get(v, []):
            res.append(("mov32", site))
        for loc in idx.ptr.get(v, []):
            if loc in idx.pool_locs:
                for site in idx.litref.get(loc, []):
                    res.append(("litpool", site))
                if not idx.litref.get(loc): res.append(("pool-orphan", loc))
            else:
                res.append(("dataptr", loc))
    return sorted(set(res), key=lambda x: x[1])


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("cmd", nargs="?")
    ap.add_argument("args", nargs="*")
    ap.add_argument("-n", type=int, default=0)
    ap.add_argument("-w", action="store_true")
    ap.add_argument("-i", action="store_true")
    ap.add_argument("--range", nargs=2)
    a = ap.parse_args()
    if not a.cmd or a.cmd in ("-h", "--help", "help"):
        print(__doc__); return
    c = Ctx(); pe = c.pe
    cmd, args = a.cmd, a.args

    if cmd == "info":
        print("exe:", pe.path); print("machine: 0x%x base: 0x%x image: 0x%x entry: 0x%x" % (pe.machine, pe.base, pe.size_of_image, pe.entry))
        for s in pe.sections: print("  %-7s va=%08x vs=%08x ro=%08x rs=%08x" % (s["name"], s["va"], s["vs"], s["ro"], s["rs"]))
        for i, (r, s) in enumerate(pe.dd):
            if r: print("  dd[%d] rva=%08x size=%x" % (i, r, s))
        print("pdata functions:", len(pe.pdata()))
    elif cmd == "func":
        r = hx(args[0]) & ~1
        f = c.func_of(r)
        if not f: print("no pdata function contains it; use `dis`"); return
        b, l = f
        print("function %s  [%08x..%08x) len=%d" % (c.fname(b), b, b + l, l))
        for ln in thumb_dis(pe, b, count=a.n or 100000, end=b + l, ann=c.annotator()): print(ln)
    elif cmd == "dis":
        r = hx(args[0]) & ~1
        cnt = int(args[1]) if len(args) > 1 else 40
        for ln in thumb_dis(pe, r, count=cnt, ann=c.annotator()): print(ln)
    elif cmd == "callers":
        r = hx(args[0]) & ~1
        for site, kind in sorted(c.idx.callers.get(r, [])):
            f = c.func_of(site)
            print("%08x  %-4s in %s" % (site, kind, c.fname(f[0]) if f else "?"))
    elif cmd == "calls":
        r = hx(args[0]) & ~1
        f = c.func_of(r)
        b, l = f
        seen = []
        ann = c.annotator()
        for ln in thumb_dis(pe, b, count=100000, end=b + l, ann=ann):
            if "; " in ln and (" bl " in ln or " blx " in ln or "-> 0x" in ln or "pool[" in ln):
                print(ln)
    elif cmd == "xref":
        r = hx(args[0])
        for kind, site in xrefs_to(c, r):
            f = c.func_of(site) if pe.in_text(site) else None
            print("%-8s %08x  %s" % (kind, site, ("in " + c.fname(f[0]) + "+0x%x" % (site - f[0])) if f else pe.sec_of(site)["name"] if pe.sec_of(site) else ""))
    elif cmd in ("str", "strx"):
        text = args[0]
        pat = text.lower() if a.i else text
        hits = []
        for s in pe.sections:
            if s["name"] not in (".rdata", ".data", ".text"): continue
            buf = bytes(pe.img[s["va"]:s["va"] + s["vs"]])
            if a.w:
                for m in re.finditer(re.escape(pat.encode("utf-16le") if not a.i else pat.encode("utf-16le")), buf if not a.i else buf.lower() if False else buf):
                    hits.append(s["va"] + m.start())
            else:
                bb = buf.lower() if a.i else buf
                for m in re.finditer(re.escape(pat.encode()), bb):
                    hits.append(s["va"] + m.start())
        for h in hits[:200]:
            # extend to string start
            st = h
            if not a.w:
                while st > 0 and 32 <= pe.u8(st - 1) < 127: st -= 1
                txt = pe.cstr(st, 200).decode("latin1")
            else:
                while st >= 2 and 32 <= pe.u8(st - 2) < 127 and pe.u8(st - 1) == 0: st -= 2
                txt = pe.wstr(st, 200)
            xr = xrefs_to(c, st)
            print("%08x  refs=%d  %r" % (st, len(xr), txt[:100]))
            if cmd == "strx":
                for kind, site in xr[:12]:
                    f = c.func_of(site) if pe.in_text(site) else None
                    print("      %-8s %08x %s" % (kind, site, ("in " + c.fname(f[0]) + "+0x%x" % (site - f[0])) if f else ""))
    elif cmd == "rtti":
        sub = args[0]
        for name, lst in sorted(c.idx.rtti.items()):
            if sub.lower() in name.lower():
                print(name)
                for (v, off) in lst:
                    print("   vtable rva=%08x (this-adjust offset %d)" % (v, off))
    elif cmd == "vtable":
        r = hx(args[0]); n = int(args[1]) if len(args) > 1 else 40
        cls = c.idx.vtable_at.get(r)
        print("vtable at %08x %s" % (r, cls or ""))
        for i in range(n):
            v = pe.u32(r + 4 * i)
            if not (pe.base <= v < pe.base + pe.size_of_image): print("  [%d] +0x%x : 0x%08x (end?)" % (i, 4 * i, v)); break
            print("  [%d] +0x%x : 0x%08x  %s" % (i, 4 * i, v, c.describe(v)))
    elif cmd == "ptrs":
        r = hx(args[0]); n = int(args[1]) if len(args) > 1 else 16
        for i in range(n):
            v = pe.u32(r + 4 * i)
            print("%08x: %08x  %s" % (r + 4 * i, v, c.describe(v)))
    elif cmd == "bytes":
        r = hx(args[0]); n = int(args[1]) if len(args) > 1 else 64
        for o in range(0, n, 16):
            b = pe.bytes(r + o, min(16, n - o))
            print("%08x: %s  %s" % (r + o, " ".join("%02x" % x for x in b), "".join(chr(x) if 32 <= x < 127 else "." for x in b)))
    elif cmd == "imports":
        sub = (args[0] if args else "").lower()
        for i in sorted(pe.imports(), key=lambda x: x["iat"]):
            if sub in (i["dll"] + "!" + i["name"]).lower():
                print("%08x  %s%s!%s" % (i["iat"], "(delay) " if i["delay"] else "", i["dll"], i["name"]))
    elif cmd == "impcallers":
        sub = args[0].lower()
        slots = {i["iat"]: i for i in pe.imports() if sub in (i["dll"] + "!" + i["name"]).lower()}
        for site, iat in sorted(c.idx.thunks.items()):
            if iat in slots:
                i = slots[iat]
                print("import %s!%s  IAT=%08x thunk=%08x" % (i["dll"], i["name"], iat, site))
                for (cs, kind) in sorted(c.idx.callers.get(site, [])):
                    f = c.func_of(cs)
                    print("    %08x %-4s in %s" % (cs, kind, c.fname(f[0]) if f else "?"))
        for iat, i in slots.items():
            for kind, site in xrefs_to(c, iat):
                f = c.func_of(site) if pe.in_text(site) else None
                if kind == "mov32" and site in c.idx.thunks: continue
                print("    direct %-8s %08x %s" % (kind, site, ("in %s" % c.fname(f[0])) if f else ""))
    elif cmd == "exports":
        sub = (args[0] if args else "").lower()
        for nm, r in pe.exports():
            if sub in nm.lower(): print("%08x  %s" % (r, nm))
    elif cmd == "find":
        pat = bytes.fromhex("".join(args))
        for h in find_all(bytes(pe.img), pat)[:200]:
            f = c.func_of(h) if pe.in_text(h) else None
            print("%08x  %s" % (h, ("in %s+0x%x" % (c.fname(f[0]), h - f[0])) if f else (pe.sec_of(h) or {}).get("name", "")))
    elif cmd == "guid":
        g = args[0].replace("{", "").replace("}", "")
        p = g.split("-")
        pat = struct.pack("<IHH", int(p[0], 16), int(p[1], 16), int(p[2], 16)) + bytes.fromhex(p[3] + p[4])
        for h in find_all(bytes(pe.img), pat):
            xr = xrefs_to(c, h)
            print("%08x  refs=%d" % (h, len(xr)))
            for kind, site in xr[:10]:
                f = c.func_of(site) if pe.in_text(site) else None
                print("    %-8s %08x %s" % (kind, site, ("in %s+0x%x" % (c.fname(f[0]), site - f[0])) if f else ""))
    elif cmd == "imm":
        val = hx(args[0])
        lo, hi = (hx(a.range[0]), hx(a.range[1])) if a.range else (pe.text_lo, pe.text_hi)
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
        md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
        pat = re.compile(r"#(0x%x|%d)\b" % (val, val))
        hits = {}
        for (b, l) in c.idx.funcs:
            if b + l < lo or b > hi: continue
            for (ia, isz, mn, ops) in md.disasm_lite(bytes(pe.img[b:b + l]), b):
                if pat.search(ops):
                    hits.setdefault(b, []).append((ia, mn, ops))
        for b, lst in sorted(hits.items()):
            print("%s [%08x]" % (c.fname(b), b))
            for (ia, mn, ops) in lst[:6]: print("     %08x %s %s" % (ia, mn, ops))
    elif cmd == "funcs":
        lo, hi = hx(args[0]), hx(args[1])
        for (b, l) in c.idx.funcs:
            if lo <= b < hi: print("%08x  len=%-5d %s" % (b, l, c.fname(b)))
    elif cmd == "name":
        with open(NAMES, "a", encoding="utf-8") as f:
            f.write(json.dumps({"rva": hx(args[0]) & ~1, "label": args[1]}) + "\n")
        print("recorded")
    elif cmd == "names":
        for r, l in sorted(c.names.items()): print("%08x %s" % (r, l))
    else:
        print("unknown command; run with --help")


if __name__ == "__main__":
    main()
