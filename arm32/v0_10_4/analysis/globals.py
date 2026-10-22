import sys, os, bisect
sys.path.insert(0, r'D:\repo\Win32Craft-main\tools\arm32')
os.environ['ARM32_EXE'] = r'D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]\MCPE.Host.WindowsPhone.exe'
import arm32re
c = arm32re.Ctx()
idx = c.idx
fs = idx.fstarts
def fof(r):
    i = bisect.bisect_right(fs, r) - 1
    return fs[i] if i >= 0 else None
lo = int(sys.argv[1], 0); hi = int(sys.argv[2], 0)   # host range
dlo = 0x340000 + 0x400000
res = []
for va, sites in idx.mov32.items():
    if va < 0x400000 + 0x33f000: continue
    fl = set(fof(s) for s in sites)
    inside = [f for f in fl if f is not None and lo <= f < hi]
    outside = [f for f in fl if f is not None and not (lo <= f < hi)]
    if inside and outside:
        res.append((va, len(inside), len(outside), sorted(inside)[:3], sorted(outside)[:4]))
for r in sorted(res):
    print('VA %08x rva %08x in=%d out=%d  in:%s out:%s' % (r[0], r[0] - 0x400000, r[1], r[2], ['%x' % x for x in r[3]], ['%x' % x for x in r[4]]))
