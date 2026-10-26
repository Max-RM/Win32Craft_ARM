"""Disassemble every unique D3D11 sub-program of every Shader and classify level_9_1 fitness."""
import os, sys, pickle, struct, collections, re, json, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unityparse import *
from d3dc import disassemble

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
DIS = os.path.join(OUT, 'dis'); os.makedirs(DIS, exist_ok=True)
shaders = pickle.load(open(os.path.join(OUT, 'shaders.pkl'), 'rb'))

FLOW = {'if_nz', 'if_z', 'else', 'endif', 'loop', 'endloop', 'break', 'breakc_nz', 'breakc_z', 'continue', 'continuec_nz', 'continuec_z',
        'switch', 'case', 'default', 'endswitch', 'call', 'callc_nz', 'callc_z', 'ret', 'retc_nz', 'retc_z', 'label'}
INTOPS = {'iadd', 'imul', 'imad', 'umad', 'umul', 'udiv', 'ishl', 'ishr', 'ushr', 'and', 'or', 'xor', 'not', 'ftoi', 'ftou', 'itof', 'utof',
          'ieq', 'ine', 'ige', 'ilt', 'uge', 'ult', 'imax', 'imin', 'umax', 'umin', 'ineg', 'bfi', 'ubfe', 'ibfe', 'countbits', 'firstbit_hi', 'firstbit_lo', 'bfrev', 'f32tof16', 'f16tof32'}
DERIV = {'deriv_rtx', 'deriv_rty', 'deriv_rtx_coarse', 'deriv_rty_coarse', 'deriv_rtx_fine', 'deriv_rty_fine'}
SAMPLE_OK_PS = {'sample', 'sample_b'}
SAMPLE_BAD = {'sample_l', 'sample_d', 'sample_c', 'sample_c_lz', 'gather4', 'gather4_c', 'gather4_po', 'ld', 'ld_ms', 'resinfo', 'sampleinfo', 'lod', 'bufinfo', 'ld_structured', 'ld_raw', 'store_raw', 'store_structured', 'store_uav_typed', 'ld_uav_typed', 'sync', 'emit', 'cut'}

def analyze(txt, stage):
    lines = [l.strip() for l in txt.splitlines()]
    a = {'version': None, 'ops': collections.Counter(), 'tex': [], 'samplers': [], 'cbs': [], 'inputs': [], 'outputs': [], 'sv_in': [], 'sv_out': [],
         'temps': 0, 'indexableTemp': False, 'dyn_cb': False, 'icb': False, 'slots': None, 'rdef': [], 'isgn': [], 'osgn': [], 'flags': ''}
    sec = None
    for l in lines:
        if l.startswith('//'):
            c = l[2:].strip()
            if c.startswith('Input signature'): sec = 'isgn'; continue
            if c.startswith('Output signature'): sec = 'osgn'; continue
            if c.startswith('Buffer Definitions'): sec = 'rdef'; continue
            if c.startswith('Resource Bindings'): sec = 'rb'; continue
            m = re.match(r'Approximately (\d+) instruction slots used', c)
            if m: a['slots'] = int(m.group(1))
            if sec in ('isgn', 'osgn', 'rdef', 'rb') and c and not c.startswith('---') and not c.startswith('Name '):
                a.setdefault(sec, []).append(c)
            continue
        if not l:
            continue
        tok = l.split()[0]
        if tok in ('vs_4_0', 'ps_4_0', 'vs_5_0', 'ps_5_0', 'vs_4_1', 'ps_4_1', 'gs_4_0', 'gs_5_0', 'hs_5_0', 'ds_5_0', 'cs_5_0', 'cs_4_0'):
            a['version'] = tok; sec = None; continue
        if tok == 'dcl_globalFlags': a['flags'] = l; continue
        if tok.startswith('dcl_'):
            if tok == 'dcl_constantbuffer':
                a['cbs'].append(l.split(None, 1)[1])
                if 'dynamicIndexed' in l: a['dyn_cb'] = True
            elif tok.startswith('dcl_resource'):
                a['tex'].append(l)
            elif tok == 'dcl_sampler':
                a['samplers'].append(l.split(None, 1)[1])
            elif tok in ('dcl_input', 'dcl_input_ps'):
                a['inputs'].append(l.split(None, 1)[1])
            elif tok in ('dcl_input_sgv', 'dcl_input_siv', 'dcl_input_ps_siv', 'dcl_input_ps_sgv'):
                a['sv_in'].append(l.split(None, 1)[1]); a['inputs'].append(l.split(None, 1)[1])
            elif tok == 'dcl_output':
                a['outputs'].append(l.split(None, 1)[1])
            elif tok in ('dcl_output_siv', 'dcl_output_sgv'):
                a['outputs'].append(l.split(None, 1)[1]); a['sv_out'].append(l.split(None, 1)[1])
            elif tok == 'dcl_temps':
                a['temps'] = int(l.split()[1])
            elif tok == 'dcl_indexableTemp':
                a['indexableTemp'] = True
            elif tok == 'dcl_immediateConstantBuffer':
                a['icb'] = True
            continue
        if tok in ('{', '}') or tok.startswith('{') or re.match(r'^[-0-9.]', tok):
            continue
        op = tok.split('(')[0]
        a['ops'][op] += 1
    # count non-ret instructions
    a['ninstr'] = sum(v for k, v in a['ops'].items() if k != 'ret')
    return a

def classify(a, stage):
    """Return (verdict, reasons). Verdict: mechanical / needs_rewrite / impossible."""
    hard = []; soft = []
    ops = a['ops']
    if a['version'] and a['version'].endswith('5_0'):
        soft.append('SM5.0 program (%s)' % a['version'])
    for op in ops:
        if op in SAMPLE_BAD:
            if op in ('sample_l',) and stage == 'vs':
                hard.append('vertex texture fetch (sample_l in VS) - not in vs_2_0')
            elif op == 'sample_l':
                soft.append('sample_l (tex2Dlod) in PS - not in ps_2_0 (use sample/sample_b)')
            elif op in ('sample_c', 'sample_c_lz'):
                hard.append('%s (shadow compare) - no comparison sampling at 9_1' % op)
            elif op == 'sample_d':
                soft.append('sample_d (tex2Dgrad) - not in ps_2_0')
            elif op in ('ld', 'resinfo', 'ld_ms', 'gather4', 'sampleinfo', 'lod'):
                hard.append('%s - not expressible in SM2.0' % op)
            else:
                hard.append(op)
    if stage == 'vs' and any(op in ('sample', 'sample_b') for op in ops):
        hard.append('vertex texture fetch - not in vs_2_0')
    if any(op in DERIV for op in ops):
        hard.append('ddx/ddy (deriv_rt*) - not in ps_2_0 (ps_2_x/ps_3_0 only)')
    flow = [op for op in ops if op in FLOW and op != 'ret']
    if flow:
        if stage == 'ps':
            soft.append('flow control %s in PS - ps_2_0 has none (must flatten / unroll)' % sorted(set(flow)))
        else:
            soft.append('flow control %s in VS - vs_2_0 only static flow control (flatten/unroll)' % sorted(set(flow)))
    ints = sorted(op for op in ops if op in INTOPS)
    if ints:
        soft.append('integer ops %s (SM2 has no ints; usually from instancing/indexing code - rewrite as float)' % ints)
    if a['indexableTemp']:
        soft.append('indexable temp array (x#) - not in SM2')
    if a['dyn_cb']:
        if stage == 'ps':
            hard.append('dynamic cbuffer indexing in PS - ps_2_0 has no relative addressing')
        else:
            soft.append('dynamic cbuffer indexing in VS (vs_2_0 a0 relative addressing ok, but must fit 256 consts)')
    if a['icb']:
        soft.append('immediate constant buffer')
    for s in a['sv_in']:
        if 'vertex_id' in s or 'instance_id' in s:
            soft.append('uses %s (not available at 9_1; instancing not supported at 9_1)' % s)
        if stage == 'ps' and 'position' in s:
            soft.append('SV_Position read in PS (VPOS) - not in ps_2_0')
        if 'is_front_face' in s:
            soft.append('SV_IsFrontFace (VFACE) - ps_3_0 only')
    for s in a['outputs']:
        if stage == 'ps' and ('o1' in s.split(',')[0] or 'o2' in s.split(',')[0] or 'o3' in s.split(',')[0]):
            hard.append('multiple render targets (%s) - 9_1 has 1 RT' % s)
        if 'oDepth' in s:
            soft.append('writes depth (oDepth)')
    ntex = len(a['tex'])
    if ntex > 8:
        soft.append('%d textures' % ntex)
    for t in a['tex']:
        if 'texture3d' in t: soft.append('3D texture')
        if 'texturecube' in t: pass
        if 'texture2darray' in t or 'texturecubearray' in t: hard.append('texture array (%s)' % t)
        if 'texture2dms' in t: hard.append('MSAA texture (%s)' % t)
        if 'buffer' in t.split()[0]: hard.append('buffer resource (%s)' % t)
    for s in a['samplers']:
        if 'mode_comparison' in s: hard.append('comparison sampler')
    if stage == 'ps':
        nin = len([i for i in a['inputs'] if not 'position' in i])
        if nin > 10:
            soft.append('%d PS inputs (ps_2_0: 8 texcoord + 2 color)' % nin)
        if a['temps'] > 12:
            soft.append('%d temps (ps_2_0: 12)' % a['temps'])
        if a['ninstr'] > 64:
            soft.append('%d SM4 instructions (ps_2_0 budget 64 ALU + 32 tex)' % a['ninstr'])
    if hard:
        return 'impossible_as_is', hard + soft
    if soft:
        return 'needs_rewrite', soft
    return 'mechanical', []

rows = []
blobs_meta = {}
for si, sh in enumerate(shaders):
    pf = sh['parsed']
    name = pf['name']
    safe = re.sub(r'[^A-Za-z0-9_.-]+', '_', name)
    sdir = os.path.join(DIS, '%02d_%s' % (si, safe)); os.makedirs(sdir, exist_ok=True)
    plats = shader_platform_blobs(sh)
    assert len(plats) == 1 and plats[0][0] == 4
    subs = plats[0][1]
    # disassemble each blob
    blobinfo = {}
    for bi, sp in enumerate(subs):
        c = sp['code']; i = c.find(b'DXBC'); dsz = struct.unpack_from('<I', c, i + 24)[0]
        dx = c[i:i + dsz]
        txt = disassemble(dx)
        stage = 'vs' if txt.lstrip().find('vs_') >= 0 and re.search(r'^\s*vs_\d_\d', txt, re.M) else ('ps' if re.search(r'^\s*ps_\d_\d', txt, re.M) else 'other')
        a = analyze(txt, stage)
        verdict, reasons = classify(a, stage)
        h = hashlib.sha1(dx).hexdigest()[:12]
        fn = os.path.join(sdir, 'b%03d_%s_%s.txt' % (bi, stage, h))
        open(fn, 'w').write(txt)
        open(fn[:-4] + '.dxbc', 'wb').write(dx)
        blobinfo[bi] = {'stage': stage, 'keywords': sp['keywords'], 'version': a['version'], 'ninstr': a['ninstr'], 'slots': a['slots'], 'temps': a['temps'],
                        'tex': a['tex'], 'samplers': a['samplers'], 'cbs': a['cbs'], 'inputs': a['inputs'], 'outputs': a['outputs'],
                        'verdict': verdict, 'reasons': reasons, 'sha1': h, 'file': fn, 'prefix': c[:i].hex(), 'unity_stats': sp['stats'],
                        'ops': dict(a['ops'])}
    # map passes
    passes = []
    for ssi, ss in enumerate(pf['subshaders']):
        for pi, p in enumerate(ss['passes']):
            ent = {'subshader': ssi, 'pass': pi, 'name': p['name'], 'useName': p['useName'], 'type': p['type'], 'tags': p['tags'], 'sstags': ss['tags'], 'lod': ss['lod'], 'stages': {}}
            for st in ('vs', 'ps', 'gs', 'hs', 'ds'):
                bl = sorted(set(sp['blobIndex'] for sp in p[st]))
                if bl:
                    ent['stages'][st] = bl
            passes.append(ent)
    rows.append({'idx': si, 'name': name, 'file': sh['file'], 'pid': sh['pid'], 'fallback': pf['fallback'], 'deps': pf['deps'], 'passes': passes, 'blobs': blobinfo,
                 'props': [p['name'] for p in pf['props']]})
json.dump(rows, open(os.path.join(OUT, 'analysis.json'), 'w'), indent=1)

# summary print
tot = collections.Counter()
for r in rows:
    vc = collections.Counter(b['verdict'] for b in r['blobs'].values())
    tot.update(vc)
    vers = collections.Counter(b['version'] for b in r['blobs'].values())
    print('%2d %-55s %-22s passes=%d blobs=%d %s %s fb=%r' % (r['idx'], r['name'], r['file'][:22], len(r['passes']), len(r['blobs']), dict(vc), dict(vers), r['fallback']))
print('TOTAL', tot)
