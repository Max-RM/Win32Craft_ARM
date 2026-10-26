"""Print Unity binding metadata (cbuffer members with offsets, textures, bind slots) for given shader idx + blob."""
import os, sys, pickle, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unityparse import *
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
shaders = pickle.load(open(os.path.join(OUT, 'shaders.pkl'), 'rb'))
idx = int(sys.argv[1]); want = set(int(x) for x in sys.argv[2:]) if len(sys.argv) > 2 else None
sh = shaders[idx]
print(sh['parsed']['name'], 'props:', [(p['name'], p['type']) for p in sh['parsed']['props']])
seen = set()
for ss in sh['parsed']['subshaders']:
    for p in ss['passes']:
        names = {i: n for n, i in p['nameIndices']}
        print('pass state:', p['state']['name'], 'tags', p['state']['tags'], 'zTest', p['state']['zTest'], 'zWrite', p['state']['zWrite'], 'cull', p['state']['culling'],
              'blend0', [x for x in p['state']['rtBlend'][0][:4]])
        for st in ('vs', 'ps'):
            for sp in p[st]:
                if sp['blobIndex'] in seen or (want and sp['blobIndex'] not in want):
                    continue
                seen.add(sp['blobIndex'])
                print(' %s blob %d tier %d kw %s channels %s' % (st, sp['blobIndex'], sp['tier'], [names.get(k) for k in sp['keywordIndices']], sp['channels']))
                binds = {c['nameIndex']: c['index'] for c in sp['cbbind']}
                for cb in sp['cbs']:
                    print('   cbuffer %s : register(b%d) size %d' % (names.get(cb['nameIndex']), binds.get(cb['nameIndex'], -1), cb['size']))
                    for v in cb['vec']:
                        print('      float%d %s @%d (c%d.%s) arr%d' % (v['dim'], names.get(v['nameIndex']), v['index'], v['index'] // 16, 'xyzw'[(v['index'] % 16) // 4], v['arraySize']))
                    for m in cb['mat']:
                        print('      float%dx4 %s @%d (c%d) arr%d' % (m['rows'], names.get(m['nameIndex']), m['index'], m['index'] // 16, m['arraySize']))
                for t in sp['tex']:
                    print('   texture %s t%d sampler s%d dim %d' % (names.get(t['nameIndex']), t['index'], t['samplerIndex'], t['dim']))
                for s in sp['samplers']:
                    print('   sampler', s)
