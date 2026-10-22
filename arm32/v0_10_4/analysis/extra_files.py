import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shader_probe_0104 as p
used = set()
for m in p.resolve_materials().values(): used.add(m['vs']); used.add(m['fs'])
usedfiles = {u.split('/')[-1] + '.hlsl' for u in used}
for fn in sorted(os.listdir(p.HLSL)):
    if not fn.endswith('.hlsl') or fn in usedfiles: continue
    stage = 'vs' if '.vertex.' in fn else 'ps'
    for defs in ([], ['COLOR_BASED', 'USE_OVERLAY', 'FANCY'], ['ALPHA_TEST','USE_EMISSIVE']):
        row = []
        for fl in p.FLS:
            d = set(defs)
            if p.FLS[fl][1] < 0xA000: d.add('R8G8B8A8_SNORM_UNSUPPORTED')
            src, _ = p.build_source('shaders/' + fn[:-5], d)
            hr, bc, msg, t = p.d3d_compile(src, 'data/shaders/dx11/' + fn, fl, stage)
            row.append('%s=%s' % (fl, 'OK' if hr >= 0 else 'FAIL ' + msg.splitlines()[0][:80]))
        print(fn, defs, ' '.join(row))
