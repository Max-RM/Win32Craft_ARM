"""Who references each Shader: PPtr scan over every serialized file + Material decode."""
import os, sys, pickle, struct, collections, json, re, bisect
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unityparse import *

GAME = r'D:\repo\Win32Craft-main\BadPiggiesRebooted_1.0.0.0_ARM\Data'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
files = []
for f in sorted(os.listdir(GAME)):
    p = os.path.join(GAME, f)
    if os.path.isfile(p) and not f.endswith(('.resS', '.resource', '.config')):
        files.append((f, open(p, 'rb').read()))
for f in sorted(os.listdir(os.path.join(GAME, 'Resources'))):
    files.append(('Resources/' + f, open(os.path.join(GAME, 'Resources', f), 'rb').read()))
ab = os.path.join(GAME, 'StreamingAssets', 'AssetBundles')
for f in sorted(os.listdir(ab)):
    if f.endswith('.manifest') or f == 'StandaloneWindows':
        continue
    for name, d in read_unityfs(os.path.join(ab, f)).items():
        if not name.endswith(('.resS', '.resource')):
            files.append(('AB/' + f, d))

def canon(ext):
    e = ext.lower()
    if e == 'library/unity default resources': return 'resources/unity%20default%20resources'
    if e.startswith('library/'): e = e[8:]
    return e

sfs = {}
for fn, d in files:
    try:
        sfs[fn] = (read_serialized(d), d)
    except Exception as e:
        print('skip', fn, e)

rows = json.load(open(os.path.join(OUT, 'analysis.json')))
shader_keys = {}
for r in rows:
    shader_keys[(r['file'].lower(), r['pid'])] = r['name'] + ' [%s#%d]' % (r['file'], r['pid'])

# material decode
mats = []
for fn, (sf, d) in sfs.items():
    for (pid, start, size, cid) in sf['objects']:
        if cid != 21:
            continue
        rr = R(d, start)
        name = rr.astr(); fid = rr.i32(); spid = rr.i64(); kw = rr.astr()
        if fid == 0:
            tf = fn.lower()
        else:
            tf = canon(sf['externals'][fid - 1])
        sname = shader_keys.get((tf, spid), 'UNRESOLVED(%s,%d)' % (tf, spid))
        mats.append({'file': fn, 'pid': pid, 'name': name, 'shader': sname, 'keywords': kw})

# generic pptr scan
refs = collections.defaultdict(collections.Counter)
for (sfile, spid), sname in shader_keys.items():
    for fn, (sf, d) in sfs.items():
        if fn.lower() == sfile:
            fid = 0
        else:
            ex = [canon(e) for e in sf['externals']]
            if sfile not in ex:
                continue
            fid = ex.index(sfile) + 1
        pat = struct.pack('<iq', fid, spid)
        objs = sorted(sf['objects'], key=lambda o: o[1])
        starts = [o[1] for o in objs]
        i = d.find(pat)
        while i >= 0:
            k = bisect.bisect_right(starts, i) - 1
            if k >= 0 and objs[k][1] <= i < objs[k][1] + objs[k][2]:
                cid = objs[k][3]
                if not (cid == 48 and objs[k][0] == spid):
                    refs[sname]['%s:class%d' % (fn, cid)] += 1
            i = d.find(pat, i + 1)

json.dump({'mats': mats, 'refs': {k: dict(v) for k, v in refs.items()}}, open(os.path.join(OUT, 'refs.json'), 'w'), indent=1)
bys = collections.defaultdict(list)
for m in mats:
    bys[m['shader']].append(m)
for r in rows:
    key = r['name'] + ' [%s]' % r['file']
    ms = bys.get(key, [])
    kws = collections.Counter(m['keywords'] for m in ms)
    print('%-80s mats=%3d kw=%s refs=%s' % (key, len(ms), dict(kws), dict(refs.get(key, {}))))
for k in bys:
    if k.startswith('UNRESOLVED'):
        print(k, len(bys[k]), [m['name'] for m in bys[k]][:5])
