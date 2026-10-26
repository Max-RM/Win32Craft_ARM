"""Pack every level-9 replacement into one lookup file for a CreateVertexShader/CreatePixelShader/CreateInputLayout hook.
Format (little endian):  'L9SW' u32 version=1, u32 count, then count x { u8[16] dxbc_checksum_of_ORIGINAL, u32 offset, u32 size }
sorted by checksum, then the level-9 DXBC blobs.  The key is bytes 4..19 of the DXBC Unity passes in (its MD5-style checksum).
Usage: python -I tools/make_table.py  -> auto/l9_replacements.bin + auto/l9_replacements.txt"""
import os, csv, struct
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUTO = os.path.join(ROOT, 'auto')
ents = []
with open(os.path.join(AUTO, 'needed', 'index.csv')) as fh:
    for r in csv.DictReader(fh):
        if r['l9_cso']:
            ents.append((bytes.fromhex(r['dxbc_checksum']), open(os.path.join(AUTO, 'needed', r['l9_cso']), 'rb').read(), r['used_by'].split(' | ')[0], r['verdict']))
up = os.path.join(AUTO, 'unityplayer')
for nm in ('internal_blit_vs', 'internal_blit_ps'):
    first = open(os.path.join(up, nm + '.orig.txt')).readline()
    ck = first.split('checksum ')[1].strip()
    ents.append((bytes.fromhex(ck), open(os.path.join(up, nm + '.l9.cso'), 'rb').read(), 'UnityPlayer.dll ' + nm, 'mechanical'))
# hand-written trimmed tier-1/2 replacements (built by tools/verify_proto.py into build/)
HAND = [('05_dummy_lit_b013_ps_63f2a98727fb.dxbc', 'Dummy_lit_FORWARDBASE_PS_tier1_DIRECTIONAL_trimmed_simple_compared_with_no_light.l9.cso'),
        ('05_dummy_lit_b015_ps_136c06546f76.dxbc', 'Dummy_lit_FORWARDBASE_PS_tier1_DIRECTIONAL_LIGHTPROBE_SH_trimmed_simple_compared_with_no_light.l9.cso'),
        ('28_dummy_lit_tinted_b013_ps_a23f6435a863.dxbc', 'Tinted_dummy_lit_FORWARDBASE_PS_tier1_DIRECTIONAL_trimmed_simple_no_light.l9.cso'),
        ('28_dummy_lit_tinted_b015_ps_9bf8b6c52ba1.dxbc', 'Tinted_dummy_lit_FORWARDBASE_PS_tier1_DIRECTIONAL_LIGHTPROBE_SH_trimmed_simple_no_light.l9.cso')]
for orig, cso in HAND:
    ck = open(os.path.join(ROOT, 'orig', orig), 'rb').read()[4:20]
    ents.append((ck, open(os.path.join(ROOT, 'build', cso), 'rb').read(), 'hand (hlsl/dummy_lit_forwardbase_tier1.hlsl) for ' + orig, 'hand-done'))
# shadow-caster family (Legacy Shaders/VertexLit + Particles/Standard Unlit SHADOWCASTER passes).  The earlier needed() filter dropped
# SHADOWCASTER, but Unity creates these programs at scene load (the 4 runtime MISSes seen on the Surface 2, plus 4 keyword siblings as
# insurance).  Each <original-DXBC-checksum>.l9.cso here is built from the game's own SM4 by tools/build_missing_l9.py (sm4-equivalent,
# signatures-identical, creates on a WARP FL 9_1 device).  Filename stem = the original checksum = the table key.
SC = os.path.join(AUTO, 'shadowcasters')
if os.path.isdir(SC):
    for nm in sorted(os.listdir(SC)):
        if nm.endswith('.l9.cso'):
            ck = bytes.fromhex(nm[:-len('.l9.cso')])
            ents.append((ck, open(os.path.join(SC, nm), 'rb').read(), 'shadow-caster (tools/build_missing_l9.py)', 'mechanical'))
ents.sort(key=lambda e: e[0])
assert len(set(e[0] for e in ents)) == len(ents)
hdr = struct.pack('<4sII', b'L9SW', 1, len(ents))
table_size = len(ents) * 24
off = len(hdr) + table_size
tab = b''; data = b''
for ck, blob, _, _ in ents:
    tab += ck + struct.pack('<II', off + len(data), len(blob))
    data += blob
open(os.path.join(AUTO, 'l9_replacements.bin'), 'wb').write(hdr + tab + data)
with open(os.path.join(AUTO, 'l9_replacements.txt'), 'w') as fh:
    for ck, blob, who, v in ents:
        fh.write('%s %6d %-12s %s\n' % (ck.hex(), len(blob), v, who))
print(len(ents), 'entries,', len(hdr + tab + data), 'bytes')
