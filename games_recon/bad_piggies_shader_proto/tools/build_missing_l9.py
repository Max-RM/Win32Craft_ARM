"""build_missing_l9.py - build level_9_1 replacements for specific Bad Piggies DXBC programs (by original DXBC checksum) straight from the game's Data/,
using the existing tools in games_recon/bad_piggies_shader_proto/tools (sm4tohlsl translator + sm4emu interpreter + d3dcompiler_47).
Read-only on the game; x64 python; nothing ARM is executed.  Writes ONLY under --out.

  python -B -I build_missing_l9.py --data  <...>\\BadPiggiesRebooted_1.0.0.0_ARM\\Data ^
                                   --tools <...>\\games_recon\\bad_piggies_shader_proto\\tools ^
                                   --out   <empty dir> ^
                                   [--preset misses|shadowcasters|all-mechanical] [--ck <32 hex> ...] ^
                                   [--table <...>\\auto\\l9_replacements.bin --merge-out <new .bin>]
Per program it: translates the SM4 text to register-level HLSL, recompiles it for SM4 and proves it equal to Unity's original on 24 random input sets
(sm4emu), compiles it for vs/ps_4_0_level_9_1, checks that the input/output signatures are byte-identical to the original, and writes
<ck>.l9.cso / <ck>.hlsl / <ck>.orig.txt / <ck>.l9.asm.  --merge-out writes a new 'L9SW' table = --table + the new entries (the old table is untouched).
"""
import sys, os, re, struct, argparse, collections
sys.dont_write_bytecode = True
ap = argparse.ArgumentParser()
ap.add_argument('--data', required=True); ap.add_argument('--tools', required=True); ap.add_argument('--out', required=True)
ap.add_argument('--preset', choices=['misses', 'shadowcasters', 'all-mechanical'], default='misses')
ap.add_argument('--ck', nargs='*', default=[]); ap.add_argument('--table'); ap.add_argument('--merge-out')
a = ap.parse_args()
sys.path.insert(0, a.tools)
from unityparse import read_serialized, parse_shader, shader_platform_blobs
from sm4tohlsl import translate, Unsupported
from d3dc import compile, disassemble
from sm4emu import equivalent

MISSES = ['7eda7bd3a50b1aea45191dd3db67f4af',   # VS 1408 Legacy Shaders/VertexLit  SHADOWCASTER SHADOWS_DEPTH
          '2149265952758e8e2aa16f30616fa85e',   # PS  212 Legacy Shaders/VertexLit  SHADOWCASTER (SHADOWS_DEPTH and SHADOWS_CUBE)
          'e8e7fc89f0811cee3a03ce9369229eaa',   # VS 1508 Particles/Standard Unlit  SHADOWCASTER SHADOWS_CUBE _ALPHABLEND_ON
          'a8e80925af66290d2f3da26d61e3adc2']   # PS  408 Particles/Standard Unlit  SHADOWCASTER (+_ALPHABLEND_ON, DEPTH and CUBE)
SIBLINGS = ['699b506753c526a3c56b52f1f901d88c',  # VS 1292 VertexLit SHADOWS_CUBE
            'c350ad9b364ec53a4326f0ffab814a61',  # VS 1624 Particles SHADOWS_DEPTH _ALPHABLEND_ON
            'f7e32a0910040df455b1a7d4a63b1265',  # VS 1320 Particles SHADOWS_CUBE
            'ec874e4202b5a9e78480ffa113e9ec23']  # VS 1436 Particles SHADOWS_DEPTH

def sigs(dx):
    nch = struct.unpack_from('<I', dx, 28)[0]; out = {}
    for k in range(nch):
        off = struct.unpack_from('<I', dx, 32 + 4 * k)[0]; tag = dx[off:off + 4].decode('latin1')
        if tag in ('ISGN', 'OSGN'):
            n = struct.unpack_from('<I', dx, off + 8)[0]; els = []
            for e in range(n):
                nameoff, sidx, sysv, ctype, reg, mask = struct.unpack_from('<IIIIII', dx, off + 16 + e * 24)
                els.append((dx[off + 8 + nameoff: dx.index(b'\0', off + 8 + nameoff)], sidx, sysv, ctype, reg, mask))
            out[tag] = els
    return out

# ---- collect every D3D11 program of every Shader object: checksum -> (dxbc, [owner strings])
files = []
for f in sorted(os.listdir(a.data)):
    p = os.path.join(a.data, f)
    if os.path.isfile(p) and not f.endswith(('.resS', '.resource', '.config')):
        files.append((f, open(p, 'rb').read()))
rd = os.path.join(a.data, 'Resources')
for f in sorted(os.listdir(rd)):
    files.append(('Resources/' + f, open(os.path.join(rd, f), 'rb').read()))
progs = {}
for fn, d in files:
    sf = read_serialized(d)
    for (pid, start, size, cid) in sf['objects']:
        if cid != 48:
            continue
        sh = parse_shader(d, start, size)
        for plat, subs, dec in shader_platform_blobs(sh):
            for bi, sp in enumerate(subs):
                c = sp['code']; i = c.find(b'DXBC'); dx = c[i:i + struct.unpack_from('<I', c, i + 24)[0]]
                progs.setdefault(dx[4:20].hex(), (dx, []))[1].append('%s [%s#%d] blob%d kw=%s' % (sh['parsed']['name'], fn, pid, bi, ' '.join(sp['keywords']) or '-'))

# ---- existing table
covered = {}
if a.table:
    t = open(a.table, 'rb').read(); _, _, cnt = struct.unpack_from('<4sII', t, 0)
    for i in range(cnt):
        ck = t[12 + i * 24:28 + i * 24]; off, sz = struct.unpack_from('<II', t, 28 + i * 24); covered[ck.hex()] = t[off:off + sz]

want = list(a.ck)
if a.preset == 'misses' and not want: want = MISSES
elif a.preset == 'shadowcasters': want = MISSES + SIBLINGS + want
elif a.preset == 'all-mechanical': want = [ck for ck in progs if ck not in covered] + want
os.makedirs(a.out, exist_ok=True)
ok = {}; res = []
for ck in want:
    if ck not in progs:
        res.append((ck, 'NOT FOUND in Data/')); continue
    dx, owners = progs[ck]
    dis = disassemble(dx)
    stage = 'vs' if re.search(r'^\s*vs_\d_\d', dis, re.M) else 'ps'
    open(os.path.join(a.out, ck + '.orig.txt'), 'w').write(dis)
    r = None
    for mode in ('keep', 'bias'):
        try:
            src, tr = translate(dis, stage, lod_mode=mode)
        except Unsupported as e:
            r = 'UNSUPPORTED: %s' % e; break
        except Exception as e:
            r = 'ERROR: %r' % e; break
        bc4, m4 = compile(src, stage + '_4_0')
        if not bc4:
            r = 'SM4 recompile failed: ' + m4.strip()[:160]; break
        try:
            nb, worst = equivalent(dis, disassemble(bc4), stage)
        except Exception as e:
            r = 'emulator error %r' % e; break
        if nb:
            r = 'NOT EQUAL to original in %d/24 cases' % nb; break
        bc9, m9 = compile(src, stage + '_4_0_level_9_1')
        if not bc9:
            r = 'level_9_1 FAIL: ' + m9.strip()[:160]
            if 'SampleLevel' in src and mode == 'keep': continue
            break
        s0, s9 = sigs(dx), sigs(bc9)
        if s0 != s9:
            r = 'SIGNATURE DIFFERS from original'; break
        d9 = disassemble(bc9)
        open(os.path.join(a.out, ck + '.hlsl'), 'w').write(src)
        open(os.path.join(a.out, ck + '.l9.cso'), 'wb').write(bc9)
        open(os.path.join(a.out, ck + '.l9.asm'), 'w').write(d9)
        m = re.search(r'approximately (\d+) instruction slots used', d9)
        ok[ck] = bc9
        r = 'OK %s mode=%s orig=%dB l9=%dB slots=%s sm4-equal(24/24) signatures-identical' % (stage, mode, len(dx), len(bc9), m.group(1) if m else '?')
        break
    res.append((ck, r))
cnt = collections.Counter(r.split()[0] for _, r in res)
for ck, r in res:
    print(ck, r, '|', progs[ck][1][0] if ck in progs else '')
print('summary', dict(cnt))

if a.merge_out:
    allent = dict(covered); allent.update(ok)
    ents = sorted(allent.items())
    hdr = struct.pack('<4sII', b'L9SW', 1, len(ents)); off = len(hdr) + 24 * len(ents); tab = b''; data = b''
    for bytes_ck, blob in ents:
        tab += bytes.fromhex(bytes_ck) + struct.pack('<II', off + len(data), len(blob)); data += blob
    open(a.merge_out, 'wb').write(hdr + tab + data)
    print('wrote', a.merge_out, len(ents), 'entries (%d old + %d new)' % (len(covered), len(ok)))
