"""Build the per-shader / per-pass / per-variant table with level_9_1 verdicts."""
import os, sys, json, pickle, collections, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unityparse import *

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
rows = json.load(open(os.path.join(OUT, 'analysis.json')))
res = json.load(open(os.path.join(OUT, 'batch.json')))
refs = json.load(open(os.path.join(OUT, 'refs.json')))
shaders = pickle.load(open(os.path.join(OUT, 'shaders.pkl'), 'rb'))

mats = collections.defaultdict(list)
for m in refs['mats']:
    mats[m['shader']].append(m['keywords'])

CODE_FIND = {'GUI/Text Shader', 'Spine/Skeleton', 'Sprites/Default', 'TextMeshPro/Mobile/Distance Field', 'TextMeshPro/Sprite',
             '_Custom/Unlit_Alpha8Bit_Color', '_Custom/Unlit_Color_Geometry', 'e2d/Curve'}
SCRIPT_REF = {'INShaders/Unlit_ColorTransparent_SolidColor', 'INShaders/Unlit_Color', 'INShaders/Unlit_ColorTransparent_GrayOverlay',
              'INShaders/Unlit_ColorTransparent', 'Unlit/Cyber', 'Hidden/SnapshotEffect', '_Custom/SilhouetteShader', 'e2d/Fill'}
ENGINE_CORE = {'Hidden/InternalErrorShader', 'Hidden/InternalClear', 'Hidden/Internal-Colored', 'Hidden/BlitCopy', 'UI/Default',
               'UI/Default Font', 'Sprites/Mask', 'Hidden/Internal-StencilWrite', 'Hidden/Internal-GUITexture', 'Hidden/Internal-GUITextureClip',
               'Hidden/Internal-GUITextureClipText', 'Hidden/Internal-GUITextureBlit', 'Hidden/Internal-GUIRoundedRect'}
def usage(r):
    key = r['name'] + ' [%s#%d]' % (r['file'], r['pid'])
    if r['name'] == 'Hidden/FrameDebuggerRenderTargetDisplay':
        return 'D:unused'   # editor frame-debugger material inside unity default resources; never drawn by a player
    if mats.get(key):
        return 'A:material'
    if r['name'] in CODE_FIND and r['file'] != 'Resources/unity_builtin_extra':
        return 'A:code'
    if r['name'] in SCRIPT_REF:
        return 'B:script'
    if r['name'] in ENGINE_CORE:
        return 'C:engine'
    if r['name'] == 'Hidden/BlitCopyHDRTonemap' and r['file'] == 'globalgamemanagers.assets':
        return 'B:script'
    return 'D:unused'

def lightmode(sh, ssi, pi):
    p = sh['parsed']['subshaders'][ssi]['passes'][pi]
    for k, v in p['state']['tags']:
        if k.upper() == 'LIGHTMODE':
            return v.upper()
    return ''

def needed(r, sh, ssi, pi, kws):
    lm = lightmode(sh, ssi, pi)
    kw = set(kws)
    if lm in ('DEFERRED', 'PREPASSBASE', 'PREPASSFINAL', 'META', 'SHADOWCASTER', 'MOTIONVECTORS'):
        return False
    if lm == 'FORWARDADD':
        return False            # the game has 0 Light components
    if kw & {'SHADOWS_SCREEN', 'SHADOWS_DEPTH', 'SHADOWS_CUBE', 'VERTEXLIGHT_ON', 'INSTANCING_ON', 'PROCEDURAL_INSTANCING_ON',
             'LIGHTMAP_ON', 'DIRLIGHTMAP_COMBINED', 'DYNAMICLIGHTMAP_ON', 'LIGHTMAP_SHADOW_MIXING', 'SHADOWS_SHADOWMASK', 'UNITY_HDR_ON',
             'STEREO_INSTANCING_ON', 'UNITY_SINGLE_PASS_STEREO', 'STEREO_MULTIVIEW_ON'}:
        return False
    return True

def verdict(e):
    if e is None:
        return 'n/a', ''
    t = e['translate']
    if t != 'ok':
        reason = t.replace('UNSUPPORTED: ', '')
        if 'AttributeError' in t: reason = 'structured buffer (SM5 procedural instancing)'
        imp = any(x in reason for x in ('comparison sampler', 'texture2darray', 'texture2dms', 'SV_Ren', 'structured', 'VPOS', 'instance_id', 'SV_InstanceID'))
        return ('impossible' if imp else 'hand'), reason
    if e.get('emu') != 'EQUAL':
        return 'hand', 'translation not proven: %s' % e.get('emu')
    if e.get('l9') == 'OK':
        if e.get('l9mode') == 'bias':
            return 'mech-approx', 'sample_l->SampleBias (tex2Dlod not in ps_2_0)'
        return 'mechanical', ''
    err = e.get('l9err', '')
    m = re.search(r'error (X\d+): ([^\n]*)', err)
    return 'hand', (m.group(1) + ' ' + m.group(2)[:110]) if m else err[:120]

table = []
for r, sh in zip(rows, shaders):
    use = usage(r)
    for ssi, ss in enumerate(sh['parsed']['subshaders']):
        for pi, p in enumerate(ss['passes']):
            lm = lightmode(sh, ssi, pi)
            for st in ('vs', 'ps', 'gs', 'hs', 'ds'):
                byblob = collections.defaultdict(list)
                for sp in p[st]:
                    byblob[sp['blobIndex']].append(sp['tier'])
                for bi, tiers in sorted(byblob.items()):
                    b = r['blobs'][str(bi)]
                    e = res.get(b['sha1'])
                    v, why = verdict(e)
                    nd = use != 'D:unused' and needed(r, sh, ssi, pi, b['keywords'])
                    l9 = e.get('l9info') if e else None
                    table.append({'idx': r['idx'], 'shader': r['name'], 'file': r['file'], 'pid': r['pid'], 'use': use, 'ss': ssi, 'pass': pi, 'lightmode': lm,
                                  'stage': st, 'blob': bi, 'tiers': sorted(tiers), 'keywords': ' '.join(b['keywords']), 'sm': b['version'],
                                  'sm4_n': b['ninstr'], 'verdict': v, 'why': why, 'needed': nd, 'sha1': b['sha1'],
                                  'l9_slots': l9['slots'] if l9 else None, 'l9_alu': l9['alu'] if l9 else None, 'l9_tex': l9['tex'] if l9 else None,
                                  'l9_cbregs': l9['cbregs'] if l9 else None, 'l9_defs': l9['defs'] if l9 else None})
json.dump(table, open(os.path.join(OUT, 'table.json'), 'w'), indent=1)

# summaries
def summarize(filt, label):
    rowsf = [t for t in table if filt(t)]
    uniq = {}
    for t in rowsf:
        uniq[t['sha1']] = t
    c = collections.Counter(t['verdict'] for t in uniq.values())
    print('%-50s entries=%4d unique_programs=%4d %s' % (label, len(rowsf), len(uniq), dict(c)))
    return uniq
summarize(lambda t: True, 'ALL')
summarize(lambda t: t['use'] != 'D:unused', 'used shaders (A+B+C), all variants')
summarize(lambda t: t['use'].startswith('A'), 'A only, all variants')
u = summarize(lambda t: t['needed'], 'NEEDED variants (A+B+C, forward-base, no lights)')
summarize(lambda t: t['needed'] and t['use'].startswith('A'), 'NEEDED variants, A only')
summarize(lambda t: t['needed'] and 0 in t['tiers'], 'NEEDED, tier0 blobs only')
print()
for v in ('hand', 'impossible', 'mech-approx'):
    for t in sorted(u.values(), key=lambda t: t['idx']):
        if t['verdict'] == v:
            print(v, t['idx'], t['shader'], t['use'], t['lightmode'], t['stage'], 'b%d' % t['blob'], 'tiers', t['tiers'], repr(t['keywords']), t['sm4_n'], t['why'][:140])
