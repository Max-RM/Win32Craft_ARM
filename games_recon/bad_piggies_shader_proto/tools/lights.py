import os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unityparse import *
GAME = r'D:\repo\Win32Craft-main\BadPiggiesRebooted_1.0.0.0_ARM\Data'
files = []
for f in sorted(os.listdir(GAME)):
    p = os.path.join(GAME, f)
    if os.path.isfile(p) and not f.endswith(('.resS', '.resource', '.config')):
        files.append((f, open(p, 'rb').read()))
ab = os.path.join(GAME, 'StreamingAssets', 'AssetBundles')
for f in sorted(os.listdir(ab)):
    if f.endswith('.manifest') or f == 'StandaloneWindows':
        continue
    for name, d in read_unityfs(os.path.join(ab, f)).items():
        if not name.endswith(('.resS', '.resource')):
            files.append(('AB/' + f, d))
tot = collections.Counter()
for fn, d in files:
    sf = read_serialized(d)
    for (pid, start, size, cid) in sf['objects']:
        if cid == 108:
            r = R(d, start)
            r.i32(); r.i64(); en = r.u8(); r.align()
            ty = r.i32(); col = [r.f32() for _ in range(4)]; inten = r.f32(); rng = r.f32(); spot = r.f32(); cook = r.f32()
            sh = r.i32()
            print(fn, pid, 'enabled', en, 'type', ['Spot','Directional','Point','Area'][ty] if ty < 4 else ty, 'int %.2f' % inten, 'shadows', ['None','Hard','Soft'][sh] if sh < 3 else sh)
            tot[(ty, sh)] += 1
        if cid == 20:
            tot['camera'] += 1
print(tot)
