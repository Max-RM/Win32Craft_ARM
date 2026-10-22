#!/usr/bin/env python3
"""
shader_probe_0104.py - offline FL9_1 feasibility probe for the Minecraft PE 0.10.4.40 (WP8.1) shader set.
Adapted from arm32/tests/shader_probe.py (1.2.8).  Mirrors what the 0.10.4 exe does (re-derived, see frame_render.md):

  * materials: data/materials/{common,fancy,sad}.json index files list *.material files (+ extra "defines", "tag");
    "child:parent" inheritance; vertexShader/fragmentShader names like "shaders/renderchunk.vertex".      [sub_c9cd4 / sub_cb16c]
  * source = for every define (std::set order = sorted):  "#define NAME\n"   +  contents of
    data/shaders/dx11/<name>.hlsl  (name = file part of the shader path + ".hlsl"; "/dx11" is inserted after "shaders")
    and, when the feature level is < 10_0 (0xA000), the extra define R8G8B8A8_SNORM_UNSUPPORTED is added.  [sub_cb16c +0x70..]
  * D3DCompile(src, srcLen, "data/<path>", pDefines=NULL, pInclude=(ID3DInclude*)1 (STANDARD_FILE_INCLUDE),
                "main", "<vs|ps>_4_0<suffix>", Flags1=0x8000 (OPTIMIZATION_LEVEL3), Flags2=0, ...)          [sub_d6a3c +0x258]
    suffix = "_level_9_3" for FL 9_3 (string @RVA 0x2351f0), "" for 10_0/10_1/11_x; 9_1/9_2 throw in the game.
    major/minor model: 4.0 for FL 9_3/10_0, 4.1 for 10_1, 5.0 for 11_x                                         [sub_d7230/d7314/d73fc]

Usage:  python shader_probe_0104.py [--device] [--json out.json] [-v]
"""
import argparse, ctypes, json, os, re, sys
from ctypes import c_void_p, c_uint, c_size_t, c_int, POINTER, byref, create_string_buffer
sys.path.insert(0, r'D:\repo\Win32Craft-main\arm32\tests')
import shader_probe as sp     # low level helpers only (COM vfn, reflect, GUID); module-level code just loads d3dcompiler_47

GAME = r'D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]'
DATA = os.path.join(GAME, 'data')
HLSL = os.path.join(DATA, 'shaders', 'dx11')
MATS = os.path.join(DATA, 'materials')

FLS = {'9_1': ('4_0_level_9_1', 0x9100), '9_3': ('4_0_level_9_3', 0x9300), '10_0': ('4_0', 0xA000)}

INC = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
def expand(text, depth=0):
    def rep(m):
        p = os.path.join(HLSL, m.group(1))
        if not os.path.exists(p) or depth > 8: return '/* missing include %s */' % m.group(1)
        return expand(open(p, 'r', encoding='utf-8', errors='replace').read(), depth + 1)
    return INC.sub(rep, text)

def build_source(shader_path, defines):
    fn = shader_path.split('/')[-1] + '.hlsl'
    p = os.path.join(HLSL, fn)
    if not os.path.exists(p): return None, fn
    pre = ''.join('#define %s\n' % d for d in sorted(defines))
    return pre + expand(open(p, 'r', encoding='utf-8', errors='replace').read()), fn

def load_json_lenient(path):
    t = open(path, 'r', encoding='utf-8', errors='replace').read()
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    t = re.sub(r'(^|[^:"\\])//[^\n]*', r'\1', t)
    t = re.sub(r',\s*([}\]])', r'\1', t)
    return json.loads(t)

def resolve_materials():
    out = {}
    items = []
    for idx in ('common.json', 'fancy.json', 'sad.json'):
        for it in load_json_lenient(os.path.join(MATS, idx)):
            items.append((idx, it))
    # also every .material file stand-alone (terrain_switchable.material is not in the index files)
    listed = {it['path'].split('/')[-1] for _, it in items}
    for fn in sorted(os.listdir(MATS)):
        if fn.endswith('.material') and fn not in listed:
            items.append(('standalone', {'path': 'materials/' + fn}))
    for idx, item in items:
        path = os.path.join(DATA, item['path'])
        mats = load_json_lenient(path)
        extra = set(item.get('defines', []))
        tag = item.get('tag', '')
        for name in mats:
            chain = []
            cur = name.split(':')[0]
            seen = set()
            while cur and cur not in seen:
                seen.add(cur)
                key = next((k for k in mats if k.split(':')[0] == cur), None)
                if key is None: break
                chain.append(mats[key])
                parts = key.split(':')
                cur = parts[1] if len(parts) > 1 else None
            vs = fs = None
            defs = set()
            for m in reversed(chain):
                vs = m.get('vertexShader', vs)
                fs = m.get('fragmentShader', fs)
                defs |= set(m.get('defines', []))
            if vs and fs:
                out['%s|%s%s|%s' % (idx, name.split(':')[0], tag, item['path'].split('/')[-1])] = \
                    {'vs': vs, 'fs': fs, 'defines': defs | extra, 'idx': idx}
    return out

def d3d_compile(src, name, fl, stage):
    target = '%s_%s' % (stage, FLS[fl][0])
    sb = src.encode('utf-8')
    code = c_void_p(); err = c_void_p()
    hr = sp.d3dc.D3DCompile(sb, len(sb), name.encode(), None, None, b'main', target.encode(), 0x8000, 0, byref(code), byref(err))
    msg = ''
    if err:
        msg = sp.blob_bytes(err).decode('utf-8', 'replace').strip(); sp.com_release(err)
    bc = sp.blob_bytes(code) if code else None
    if code: sp.com_release(code)
    return hr, bc, msg, target

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--device', action='store_true'); ap.add_argument('--json'); ap.add_argument('-v', action='store_true')
    a = ap.parse_args()
    mats = resolve_materials()
    print('material variants: %d' % len(mats))
    # unique compile jobs: (shaderpath, stage, frozenset(defines), fl)
    jobs = {}
    for key, m in mats.items():
        for stage, sh in (('vs', m['vs']), ('ps', m['fs'])):
            jobs.setdefault((sh, stage, frozenset(m['defines'])), []).append(key)
    print('unique (shader, stage, defines) combos: %d ; distinct shader files: %d' %
          (len(jobs), len({(j[0], j[1]) for j in jobs})))
    res = {}
    fails = []
    for (sh, stage, defs), users in sorted(jobs.items(), key=lambda kv: (kv[0][0], kv[0][1], sorted(kv[0][2]))):
        for fl in FLS:
            d = set(defs)
            if FLS[fl][1] < 0xA000: d.add('R8G8B8A8_SNORM_UNSUPPORTED')
            src, fn = build_source(sh, d)
            k = '%s|%s|%s' % (sh, stage, ','.join(sorted(defs)))
            r = res.setdefault(k, {'shader': sh, 'stage': stage, 'defines': sorted(defs), 'users': users, 'fl': {}})
            if src is None:
                r['fl'][fl] = {'ok': False, 'err': 'source file missing: ' + fn}
                continue
            hr, bc, msg, target = d3d_compile(src, 'data/shaders/dx11/' + fn, fl, stage)
            ok = hr >= 0 and bc is not None
            e = {'ok': ok, 'target': target, 'hr': '0x%08X' % (hr & 0xffffffff)}
            if ok:
                rf = sp.reflect(bc)
                if rf: e['reflect'] = rf
                e['bytes'] = len(bc)
                e['_bc'] = bc
            if msg and (not ok or a.v): e['err'] = msg[:600]
            r['fl'][fl] = e
            if not ok and fl == '9_1': fails.append((k, msg.splitlines()[0] if msg else ''))
    # summary
    print('\n=== per (file, stage): pass/fail by feature level (over all define combos) ===')
    files = {}
    for k, r in res.items():
        files.setdefault((r['shader'], r['stage']), []).append(r)
    summary = {}
    for (sh, st), rs in sorted(files.items()):
        row = {}
        for fl in FLS:
            okn = sum(1 for r in rs if r['fl'][fl]['ok']); row[fl] = (okn, len(rs))
        mx = {fl: max([r['fl'][fl].get('reflect', {}).get('instr', 0) for r in rs if r['fl'][fl]['ok']] or [0]) for fl in FLS}
        summary['%s|%s' % (sh, st)] = {'combos': len(rs), **{fl: '%d/%d' % row[fl] for fl in FLS}, 'max_instr_sm4': mx}
        print('%-34s %s  combos=%2d  9_1=%d/%d 9_3=%d/%d 10_0=%d/%d  maxSM4instr(9_1)=%d' % (
            sh, st, len(rs), row['9_1'][0], row['9_1'][1], row['9_3'][0], row['9_3'][1], row['10_0'][0], row['10_0'][1], mx['9_1']))
    print('\n9_1 failures: %d' % len(fails))
    for k, m in fails[:40]: print('  ', k, '->', m[:200])
    out = {'summary': summary, 'results': {}}
    for k, r in res.items():
        rr = {kk: vv for kk, vv in r.items() if kk != 'fl'}
        rr['fl'] = {fl: {kk: vv for kk, vv in e.items() if kk != '_bc'} for fl, e in r['fl'].items()}
        out['results'][k] = rr
    if a.device:
        out['device'] = device_test(res)
        print(json.dumps(out['device'], indent=1)[:3000])
    if a.json:
        json.dump(out, open(a.json, 'w'), indent=1)
    return 0

def device_test(res):
    d3d11 = ctypes.WinDLL('d3d11.dll'); d3d11.D3D11CreateDevice.restype = c_int
    out = {}
    for name, driver in (('hardware', 1), ('warp', 5)):
        levels = (c_uint * 1)(0x9100)
        dev = c_void_p(); ctx = c_void_p(); fl = c_uint(0)
        hr = d3d11.D3D11CreateDevice(None, driver, None, 0, levels, 1, 7, byref(dev), byref(fl), byref(ctx))
        if hr < 0 or not dev:
            out[name] = {'create_hr': '0x%08X' % (hr & 0xffffffff)}; continue
        d = {'create_hr': 'S_OK', 'fl': hex(fl.value)}
        nok = 0; bad = []
        for k, r in res.items():
            e = r['fl']['9_1']
            if not e.get('ok'): continue
            bc = e['_bc']; slot = 12 if r['stage'] == 'vs' else 15
            buf = create_string_buffer(bc, len(bc)); shp = c_void_p()
            h = sp.vfn(dev, slot, c_int, c_void_p, c_size_t, c_void_p, POINTER(c_void_p))(dev, buf, len(bc), None, byref(shp))
            if h < 0: bad.append((k, '0x%08X' % (h & 0xffffffff)))
            else: nok += 1; sp.com_release(shp)
        # also try creating 9_3 bytecode on the 9_1 device (shows whether the game's own profile would even load)
        n93ok = 0; n93bad = 0
        for k, r in res.items():
            e = r['fl']['9_3']
            if not e.get('ok'): continue
            bc = e['_bc']; slot = 12 if r['stage'] == 'vs' else 15
            buf = create_string_buffer(bc, len(bc)); shp = c_void_p()
            h = sp.vfn(dev, slot, c_int, c_void_p, c_size_t, c_void_p, POINTER(c_void_p))(dev, buf, len(bc), None, byref(shp))
            if h < 0: n93bad += 1
            else: n93ok += 1; sp.com_release(shp)
        d['create_shader_ok_with_9_1_bytecode'] = nok
        d['create_shader_fail_with_9_1_bytecode'] = bad[:20]
        d['9_3_bytecode_on_9_1_device'] = {'ok': n93ok, 'fail': n93bad}
        out[name] = d
        sp.com_release(ctx); sp.com_release(dev)
    return out

if __name__ == '__main__':
    sys.exit(main())
