"""Compile the hand-written prototype HLSL for SM4 and level_9_1 and diff it against Unity's original programs.
Usage:  python -I tools/verify_proto.py          (run from the bad_piggies_shader_proto folder or anywhere)
Needs:  d3dcompiler_47.dll (path in tools/d3dc.py). Writes build/*.cso, build/*.asm and build/report.txt."""
import os, sys, re, json
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from d3dc import compile, disassemble
from sm4emu import equivalent
from sm4tohlsl import parse_sig, translate

ORIG = os.path.join(ROOT, 'orig'); HL = os.path.join(ROOT, 'hlsl'); BUILD = os.path.join(ROOT, 'build')
os.makedirs(BUILD, exist_ok=True)
binds = json.load(open(os.path.join(ORIG, 'unity_bindings.json')))

# (label, hlsl file, entry, stage, defines, original key)
JOBS = [
    ('Sprites/Default VS',                       'sprites_default.hlsl', 'SpriteVert', 'vs', [], '00_b000'),
    ('Sprites/Default VS ETC1',                  'sprites_default.hlsl', 'SpriteVert', 'vs', ['ETC1_EXTERNAL_ALPHA'], '00_b001'),
    ('Sprites/Default VS PIXELSNAP',             'sprites_default.hlsl', 'SpriteVert', 'vs', ['PIXELSNAP_ON'], '00_b002'),
    ('Sprites/Default VS ETC1 PIXELSNAP',        'sprites_default.hlsl', 'SpriteVert', 'vs', ['ETC1_EXTERNAL_ALPHA', 'PIXELSNAP_ON'], '00_b003'),
    ('Sprites/Default PS',                       'sprites_default.hlsl', 'SpriteFrag', 'ps', [], '00_b004'),
    ('Sprites/Default PS ETC1',                  'sprites_default.hlsl', 'SpriteFrag', 'ps', ['ETC1_EXTERNAL_ALPHA'], '00_b005'),
    ('Unlit/Transparent VS',                     'unlit_transparent.hlsl', 'vert', 'vs', [], '32_b000'),
    ('Unlit/Transparent PS',                     'unlit_transparent.hlsl', 'frag', 'ps', [], '32_b001'),
    ('_Custom/Unlit_ColorTransparent_Geometry VS', 'unlit_transparent.hlsl', 'vert', 'vs', ['COLOR_TINT'], '37_b000'),
    ('_Custom/Unlit_ColorTransparent_Geometry PS', 'unlit_transparent.hlsl', 'frag', 'ps', ['COLOR_TINT'], '37_b001'),
    ('UI/Default VS',                            'ui_default.hlsl', 'vert', 'vs', [], '71_b000'),
    ('UI/Default PS',                            'ui_default.hlsl', 'frag', 'ps', [], '71_b004'),
    ('UI/Default PS ALPHACLIP',                  'ui_default.hlsl', 'frag', 'ps', ['UNITY_UI_ALPHACLIP'], '71_b005'),
    ('UI/Default PS CLIP_RECT',                  'ui_default.hlsl', 'frag', 'ps', ['UNITY_UI_CLIP_RECT'], '71_b006'),
    ('UI/Default PS ALPHACLIP CLIP_RECT',        'ui_default.hlsl', 'frag', 'ps', ['UNITY_UI_ALPHACLIP', 'UNITY_UI_CLIP_RECT'], '71_b007'),
    # trimmed tier-1/2 replacement; compared with the original under game conditions: no LPPV (cb4[0].x=0),
    # no box projection (cb3[2].w=0), no probe blend (cb3[1].w=1). Interpreter ignores LOD/bias.
    ('Dummy lit FORWARDBASE PS tier1 DIRECTIONAL (trimmed, FULL_BRDF)',  'dummy_lit_forwardbase_tier1.hlsl', 'main', 'ps', ['FULL_BRDF'], '05_b013',
     {(4, 0, 0): 0.0, (3, 2, 3): 0.0, (3, 1, 3): 1.0}),
    ('Dummy lit FORWARDBASE PS tier1 DIRECTIONAL (trimmed, simple; compared with no light)',  'dummy_lit_forwardbase_tier1.hlsl', 'main', 'ps', [], '05_b013',
     {(4, 0, 0): 0.0, (3, 2, 3): 0.0, (3, 1, 3): 1.0, (0, 2, 0): 0.0, (0, 2, 1): 0.0, (0, 2, 2): 0.0}),
    ('Dummy lit FORWARDBASE PS tier1 DIRECTIONAL LIGHTPROBE_SH (trimmed, simple; compared with no light)', 'dummy_lit_forwardbase_tier1.hlsl', 'main', 'ps', ['LIGHTPROBE_SH'], '05_b015',
     {(4, 0, 0): 0.0, (3, 2, 3): 0.0, (3, 1, 3): 1.0, (0, 2, 0): 0.0, (0, 2, 1): 0.0, (0, 2, 2): 0.0}),
    ('Tinted dummy lit FORWARDBASE PS tier1 DIRECTIONAL (trimmed, simple; no light)', 'dummy_lit_forwardbase_tier1.hlsl', 'main', 'ps', ['COLOR_TINT'], '28_b013',
     {(4, 0, 0): 0.0, (3, 2, 3): 0.0, (3, 1, 3): 1.0, (0, 2, 0): 0.0, (0, 2, 1): 0.0, (0, 2, 2): 0.0}),
    ('Tinted dummy lit FORWARDBASE PS tier1 DIRECTIONAL LIGHTPROBE_SH (trimmed, simple; no light)', 'dummy_lit_forwardbase_tier1.hlsl', 'main', 'ps', ['COLOR_TINT', 'LIGHTPROBE_SH'], '28_b015',
     {(4, 0, 0): 0.0, (3, 2, 3): 0.0, (3, 1, 3): 1.0, (0, 2, 0): 0.0, (0, 2, 1): 0.0, (0, 2, 2): 0.0}),
]
AUTO = [  # mechanical translation (sm4tohlsl.py) of a complex program
    ('TMP/Mobile/Distance Field VS OUTLINE_ON UNDERLAY_ON (auto)', 'vs', '29_b003'),
    ('TMP/Mobile/Distance Field PS OUTLINE_ON UNDERLAY_ON (auto)', 'ps', '29_b019'),
]

def orig_file(key, ext):
    idx, b = key.split('_')
    for f in os.listdir(ORIG):
        if f.startswith(idx + '_') and ('_%s_' % b) in f and f.endswith(ext):
            return os.path.join(ORIG, f)
    raise FileNotFoundError(key)

def decls(dis):
    out = []
    for l in dis.splitlines():
        s = l.strip()
        if s.startswith(('dcl_constantbuffer', 'dcl_sampler', 'dcl_resource')):
            out.append(s)
    return sorted(out)

def sigkey(rows):
    return [(r['name'].upper(), r['index'], r['mask'], r['reg'], r['sysval'], r['fmt']) for r in rows]

def rdef_members(dis):
    """'Buffer Definitions' of our own compile -> {cbuffer: {member: offset}}"""
    res = {}; cur = None
    for l in dis.splitlines():
        m = re.match(r'^// cbuffer (\S+)', l)
        if m: cur = m.group(1); res[cur] = {}; continue
        m = re.match(r'^//\s+(?:row_major\s+)?(\w+)\s+(\w+)(?:\[\d+\])?;\s+// Offset:\s+(\d+) Size:\s+(\d+)', l)
        if m and cur:
            res[cur][m.group(2)] = int(m.group(3))
    return res

def l9_summary(d9):
    slots = re.search(r'approximately (\d+) instruction slots used(?: \((\d+) texture, (\d+) arithmetic\))?', d9)
    maps = re.findall(r'^//\s+(c\d+)\s+(cb\d+)\s+(\d+)\s+(\d+)', d9, re.M)
    return slots.group(0) if slots else '?', ['%s<-%s[%s..+%s]' % m for m in maps]

lines = []
def say(s=''):
    print(s); lines.append(s)

def check(label, src, entry, stage, defines, key, name, fix=None):
    od = open(orig_file(key, '.txt')).read()
    defs = ''.join('#define %s 1\n' % d for d in defines)
    full = defs + src
    bc4, m4 = compile(full, stage + '_4_0', entry=entry, name=name)
    bc9, m9 = compile(full, stage + '_4_0_level_9_1', entry=entry, name=name)
    say('== %s   [%s, -E %s -T %s_4_0_level_9_1 %s]' % (label, name, entry, stage, ' '.join('-D' + d for d in defines)))
    if not bc4 or not bc9:
        say('   COMPILE FAILED: %s %s' % (m4.strip(), m9.strip())); return False
    tag = re.sub(r'[^A-Za-z0-9]+', '_', label).strip('_')
    open(os.path.join(BUILD, tag + '.l9.cso'), 'wb').write(bc9)
    d9 = disassemble(bc9); open(os.path.join(BUILD, tag + '.l9.asm'), 'w').write(d9)
    d4 = disassemble(bc4)
    ok = True
    # signatures (compare the SM4 part of the level-9 blob, which is what D3D11 validates/links)
    for t in ('Input signature', 'Output signature'):
        a = sigkey(parse_sig(od.splitlines(), t)); b = sigkey(parse_sig(d9.splitlines(), t))
        say('   %-17s %s' % (t + ':', 'same' if a == b else 'DIFF orig=%s new=%s' % (a, b)))
        ok &= a == b
    # declarations: cbuffer slots/sizes, t#/s# slots
    a = decls(od); b = decls(d9)
    if fix:   # trimmed program: may read fewer registers / fewer textures, but every slot it declares must exist with the same type
        def slot(x):
            return re.sub(r'\[\d+\]', '', x)
        extra = [x for x in b if slot(x) not in [slot(y) for y in a]]
        say('   cb/tex/sampler decls (trimmed, subset allowed): %s' % ('OK, new=%s' % b if not extra else 'EXTRA %s' % extra))
        ok &= not extra
    else:
        say('   cb/tex/sampler decls: %s' % ('same ' + str(a) if a == b else 'DIFF\n      orig=%s\n      new =%s' % (a, b)))
        ok &= a == b
    # member offsets vs Unity metadata
    meta = binds[key]; rd = rdef_members(d9)
    for cb in meta['cbuffers']:
        if fix:
            cb = dict(cb, members=[m for m in cb['members'] if any(m['name'] in mem for mem in rd.values())])
            if not cb['members']:
                say('   cbuffer %-18s b%-2s not read by the trimmed program' % (cb['name'], cb['slot'])); continue
        ours = None
        if ('CB%s' % cb['slot']) in rd:   # auto-translated program: members are named cb<slot>_<register>
            mem = rd['CB%s' % cb['slot']]
            ours = {m['name']: mem.get('cb%s_%d' % (cb['slot'], m['offset'] // 16), -1) + m['offset'] % 16 for m in cb['members']}
        for nm, mem in ([] if ours is not None else rd.items()):
            if all(m['name'] in mem for m in cb['members']):
                ours = mem; break
        if ours is None:
            say('   cbuffer %s (b%s): NOT FOUND in our RDEF %s' % (cb['name'], cb['slot'], list(rd))); ok = False; continue
        diffs = [(m['name'], m['offset'], ours[m['name']]) for m in cb['members'] if ours[m['name']] != m['offset']]
        say('   cbuffer %-18s b%-2s Unity members %s -> %s' % (cb['name'], cb['slot'], [(m['name'], m['offset']) for m in cb['members']], 'offsets same' if not diffs else 'OFFSET DIFF %s' % diffs))
        ok &= not diffs
    say('   textures (Unity): %s' % [(t['name'], 't%d' % t['t'], 's%d' % t['s']) for t in meta['textures']])
    # behaviour: original SM4 vs our SM4
    nb, worst = equivalent(od, d4, stage, fix=fix)
    say('   SM4 behaviour vs original (24 random cases, own interpreter): %s' % ('EQUAL' if nb == 0 else 'MISMATCH %d %r' % (nb, worst)))
    ok &= nb == 0
    s, maps = l9_summary(d9)
    say('   level_9_1: %s; D3D9 constant map %s' % (s, maps))
    say('   RESULT: %s' % ('OK' if ok else 'CHECK'))
    return ok

allok = True
for job in JOBS:
    label, f, entry, stage, defines, key = job[:6]
    allok &= check(label, open(os.path.join(HL, f)).read(), entry, stage, defines, key, f, job[6] if len(job) > 6 else None)
for label, stage, key in AUTO:
    od = open(orig_file(key, '.txt')).read()
    src, t = translate(od, stage)
    fn = os.path.join(ROOT, 'auto', re.sub(r'[^A-Za-z0-9]+', '_', label).strip('_') + '.hlsl')
    open(fn, 'w').write(src)
    allok &= check(label, src, 'main', stage, [], key, os.path.basename(fn))
say(); say('ALL OK' if allok else 'SOME CHECKS FAILED')
open(os.path.join(BUILD, 'report.txt'), 'w').write('\n'.join(lines) + '\n')
