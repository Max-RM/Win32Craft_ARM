import os, sys, pickle, struct, collections, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unityparse import *

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
shaders = pickle.load(open(os.path.join(OUT, 'shaders.pkl'), 'rb'))
prefixes = collections.Counter()
tails = collections.Counter()
progtypes = collections.Counter()
nsub = 0
for sh in shaders[:3]:
    for (plat, subs, dec) in shader_platform_blobs(sh):
        print(sh['parsed']['name'], 'plat', plat, 'nsub', len(subs), 'declen', len(dec))
        for sp in subs[:4]:
            c = sp['code']
            i = c.find(b'DXBC')
            dsz = struct.unpack_from('<I', c, i + 24)[0] if i >= 0 else -1
            print('  ver', sp['version'], 'type', sp['progType'], 'stats', sp['stats'], 'kw', sp['keywords'], sp.get('localKeywords'), 'codelen', len(c), 'dxbc@', i, 'dxbcsize', dsz, 'prefix', c[:i].hex(), 'after', c[i + dsz:].hex()[:80], 'tail', sp['tail'].hex()[:80])
for sh in shaders:
    for (plat, subs, dec) in shader_platform_blobs(sh):
        for sp in subs:
            nsub += 1
            c = sp['code']
            i = c.find(b'DXBC')
            dsz = struct.unpack_from('<I', c, i + 24)[0]
            prefixes[(i, c[:i].hex())] += 1
            tails[len(c) - i - dsz] += 1
            progtypes[(sp['progType'], sp['version'])] += 1
print('nsub', nsub)
print('prefixes', prefixes.most_common(20))
print('after-dxbc lengths', tails.most_common(20))
print('progtypes', progtypes)
