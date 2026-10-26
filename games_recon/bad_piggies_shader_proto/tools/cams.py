import os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unityparse import *
exec(open('lights.py').read().split('tot = collections')[0].split('from unityparse import *')[1])
tot = collections.Counter()
for fn, d in files:
    sf = read_serialized(d)
    for (pid, start, size, cid) in sf['objects']:
        if cid == 20:
            r = R(d, start)
            r.i32(); r.i64(); en = r.u8(); r.align()
            clear = r.u32(); bg = [r.f32() for _ in range(4)]; pm = r.i32(); [r.f32() for _ in range(4)]; gf = r.i32(); fl = r.f32()
            rect = [r.f32() for _ in range(4)]; near = r.f32(); far = r.f32(); fov = r.f32(); ortho = r.u8(); r.align(); osz = r.f32(); depth = r.f32()
            cm = r.u32(); rp = r.i32(); r.i32(); r.i64(); td = r.i32(); te = r.i32(); hdr = r.u8(); msaa = r.u8(); dyn = r.u8(); force = r.u8(); occ = r.u8(); r.align()
            k = (en, clear, ortho, rp, hdr, msaa)
            tot[k] += 1
            if tot[k] == 1: print(fn, pid, 'enabled', en, 'clear', clear, 'ortho', ortho, 'renderingPath', rp, 'HDR', hdr, 'MSAA', msaa, 'fl', fl, 'gate', gf, 'near', near, 'far', far)
print(tot)
