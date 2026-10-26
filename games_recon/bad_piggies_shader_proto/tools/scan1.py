import os, sys, pickle, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unityparse import *

GAME = r'D:\repo\Win32Craft-main\BadPiggiesRebooted_1.0.0.0_ARM\Data'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
os.makedirs(OUT, exist_ok=True)

files = []
for f in sorted(os.listdir(GAME)):
    p = os.path.join(GAME, f)
    if os.path.isfile(p) and not f.endswith(('.resS', '.resource', '.config')):
        files.append((f, open(p, 'rb').read()))
for f in sorted(os.listdir(os.path.join(GAME, 'Resources'))):
    files.append(('Resources/' + f, open(os.path.join(GAME, 'Resources', f), 'rb').read()))
ab = os.path.join(GAME, 'StreamingAssets', 'AssetBundles')
for f in sorted(os.listdir(ab)):
    if f.endswith('.manifest'):
        continue
    try:
        nodes = read_unityfs(os.path.join(ab, f))
    except Exception as e:
        print('BUNDLE FAIL', f, e); continue
    for name, d in nodes.items():
        print('bundle', f, 'node', name, len(d))
        if name.endswith(('.resS', '.resource')):
            continue
        files.append(('AB/' + f + '/' + name, d))

classcount = collections.Counter()
shaders = []
for (fn, d) in files:
    try:
        sf = read_serialized(d)
    except Exception as e:
        print('SF FAIL', fn, e); continue
    cc = collections.Counter(o[3] for o in sf['objects'])
    classcount.update(cc)
    n48 = cc.get(48, 0)
    print('%-60s v%d %s plat=%d objs=%d shaders=%d mats=%d ext=%s' % (fn, sf['version'], sf['unity'], sf['platform'], len(sf['objects']), n48, cc.get(21, 0), sf['externals'][:4]))
    for (pid, start, size, cid) in sf['objects']:
        if cid == 48:
            try:
                sh = parse_shader(d, start, size)
                ok = sh['consumed'] == size
                print('   SHADER pid=%d size=%d name=%r parsed=%r plats=%s consumed_ok=%s' % (pid, size, sh['objName'], sh['parsed']['name'], sh['platforms'], ok))
                sh['file'] = fn; sh['pid'] = pid
                shaders.append(sh)
            except Exception as e:
                print('   SHADER PARSE FAIL pid=%d size=%d name=%r: %s' % (pid, size, name_of(d, start), e))
pickle.dump(shaders, open(os.path.join(OUT, 'shaders.pkl'), 'wb'))
print('class counts top:', classcount.most_common(30))
print('total shaders', len(shaders))
