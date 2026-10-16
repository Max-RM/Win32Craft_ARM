#!/usr/bin/env python3
"""
shader_probe.py - offline FL9_1 feasibility probe for the Minecraft 1.2.8 ARM32 shader set.

What it does (all evidence-driven, mirrors compileShaderProgram 0x5BDECC in the ARM exe):
  * parses data/resource_packs/vanilla/materials/*.material (+ the index files common.json / fancy.json /
    sad.json that add per-set defines) and resolves "child:parent" inheritance;
  * builds each (vertexShader, fragmentShader, defines) variant the way the game does:
        source = "#define X\n" for every define + data/shaders/hlsl/<name>.hlsl   (includes inlined)
        D3DCompile(src, name, {VERSION=<decimal FL>, 0}, include, "main", "<vs|ps>_4_0_level_9_x", 0x8000, 0)
  * compiles vs/ps for 9_1 and 9_3 (and 10_0 = plain 4_0 as a sanity baseline) with d3dcompiler_47.dll (ctypes),
    reports instruction counts through D3DReflect (the same reflection the game does);
  * optionally (--device) creates a real D3D11 device restricted to {0x9100} and (a) calls
    CreateVertexShader/CreatePixelShader on every compiled blob, (b) dumps CheckFormatSupport for the
    formats the game uses (so you see what is legal at 9_1 on this machine's driver/WARP);
  * optionally (--fxc) cross-checks a few shaders with the SDK fxc.exe.

Usage (repo root):  python arm32/tests/shader_probe.py [--device] [--fxc] [--json out.json] [--only substr] [-v]
"""
import argparse, ctypes, json, os, re, subprocess, sys, tempfile
from ctypes import (c_void_p, c_char_p, c_uint, c_size_t, POINTER, byref, Structure, WINFUNCTYPE, c_int, c_ubyte,
                    c_uint32, c_wchar_p, create_string_buffer, cast, string_at)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DATA = os.path.join(ROOT, os.environ.get('SHADER_PROBE_GAME', 'Minecraft_1.2.8.0_ARM32'), 'data')   # e.g. SHADER_PROBE_GAME=Minecraft_1.16.40.02
HLSL = os.path.join(DATA, 'shaders', 'hlsl')
MATS = os.path.join(DATA, 'resource_packs', 'vanilla', 'materials')
FXC = r'D:\Windows Kits\10\bin\10.0.19041.0\x64\fxc.exe'

FL_VALUES = {'9_1': 0x9100, '9_3': 0x9300, '10_0': 0xA000}
TARGET_SUFFIX = {'9_1': '4_0_level_9_1', '9_3': '4_0_level_9_3', '10_0': '4_0'}   # game: 5BDD0C + 5BDECC logic

# ----------------------------------------------------------------------------- COM helpers
class GUID(Structure):
    _fields_ = [('a', c_uint32), ('b', ctypes.c_uint16), ('c', ctypes.c_uint16), ('d', c_ubyte * 8)]

def guid(s):
    import uuid
    u = uuid.UUID(s)
    g = GUID()
    g.a, g.b, g.c = u.time_low, u.time_mid, u.time_hi_version
    for i, v in enumerate(u.bytes[8:]): g.d[i] = v
    return g

def vfn(obj, idx, restype, *argtypes):
    vt = cast(obj, POINTER(c_void_p))[0]
    fn = cast(vt, POINTER(c_void_p))[idx]
    return WINFUNCTYPE(restype, c_void_p, *argtypes)(fn)

def com_release(obj):
    if obj: vfn(obj, 2, c_uint)(obj)

def blob_bytes(blob):
    p = vfn(blob, 3, c_void_p)(blob)
    n = vfn(blob, 4, c_size_t)(blob)
    return string_at(p, n)

class Macro(Structure):
    _fields_ = [('Name', c_char_p), ('Definition', c_char_p)]

d3dc = ctypes.WinDLL('d3dcompiler_47.dll')
d3dc.D3DCompile.restype = ctypes.HRESULT if False else c_int
d3dc.D3DCompile.argtypes = [c_void_p, c_size_t, c_char_p, c_void_p, c_void_p, c_char_p, c_char_p, c_uint, c_uint,
                            POINTER(c_void_p), POINTER(c_void_p)]
d3dc.D3DReflect.restype = c_int
d3dc.D3DReflect.argtypes = [c_void_p, c_size_t, POINTER(GUID), POINTER(c_void_p)]
IID_REFL = guid('8d536ca1-0cca-4956-a837-786963755584')

class ShaderDesc(Structure):
    _fields_ = [('Version', c_uint), ('Creator', c_void_p), ('Flags', c_uint), ('ConstantBuffers', c_uint),
                ('BoundResources', c_uint), ('InputParameters', c_uint), ('OutputParameters', c_uint),
                ('InstructionCount', c_uint), ('TempRegisterCount', c_uint), ('TempArrayCount', c_uint),
                ('DefCount', c_uint), ('DclCount', c_uint), ('TextureNormalInstructions', c_uint),
                ('TextureLoadInstructions', c_uint), ('TextureCompInstructions', c_uint),
                ('TextureBiasInstructions', c_uint), ('TextureGradientInstructions', c_uint),
                ('FloatInstructionCount', c_uint), ('IntInstructionCount', c_uint), ('UintInstructionCount', c_uint),
                ('StaticFlowControlCount', c_uint), ('DynamicFlowControlCount', c_uint),
                ('MacroInstructionCount', c_uint), ('ArrayInstructionCount', c_uint), ('CutInstructionCount', c_uint),
                ('EmitInstructionCount', c_uint), ('GSOutputTopology', c_uint), ('GSMaxOutputVertexCount', c_uint),
                ('InputPrimitive', c_uint), ('PatchConstantParameters', c_uint), ('cGSInstanceCount', c_uint),
                ('cControlPoints', c_uint), ('HSOutputPrimitive', c_uint), ('HSPartitioning', c_uint),
                ('TessellatorDomain', c_uint), ('cBarrierInstructions', c_uint), ('cInterlockedInstructions', c_uint),
                ('cTextureStoreInstructions', c_uint)]

def reflect(bytecode):
    buf = create_string_buffer(bytecode, len(bytecode))
    out = c_void_p()
    hr = d3dc.D3DReflect(buf, len(bytecode), byref(IID_REFL), byref(out))
    if hr < 0 or not out: return None
    desc = ShaderDesc()
    vfn(out, 3, c_int, c_void_p)(out, byref(desc))
    r = {'instr': desc.InstructionCount, 'cb': desc.ConstantBuffers, 'res': desc.BoundResources,
         'in': desc.InputParameters, 'out': desc.OutputParameters, 'temps': desc.TempRegisterCount,
         'tex': desc.TextureNormalInstructions + desc.TextureLoadInstructions + desc.TextureBiasInstructions +
                desc.TextureGradientInstructions + desc.TextureCompInstructions}
    com_release(out)
    return r

# ----------------------------------------------------------------------------- source assembly
_file_cache = {}
def find_hlsl(name, base=HLSL):
    key = name.lower()
    if key in _file_cache: return _file_cache[key]
    for fn in os.listdir(base):
        if fn.lower() == key:
            _file_cache[key] = os.path.join(base, fn)
            return _file_cache[key]
    _file_cache[key] = None
    return None

INC = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
def expand(text, depth=0):
    def rep(m):
        p = find_hlsl(m.group(1))
        if not p or depth > 8: return '/* missing include %s */' % m.group(1)
        return expand(open(p, 'r', encoding='utf-8', errors='replace').read(), depth + 1)
    return INC.sub(rep, text)

def build_source(shader_path, defines):
    # shader_path like "shaders/renderchunk.vertex" -> data/shaders/hlsl/renderchunk.vertex.hlsl  (code at 0x5C0CA8: "/hlsl" + ".hlsl")
    fn = shader_path.split('/')[-1] + '.hlsl'
    p = find_hlsl(fn)
    if not p: return None
    pre = ''.join('#define %s\n' % d for d in sorted(defines))      # string "#define " at 0x00DBC8E4
    return pre + expand(open(p, 'r', encoding='utf-8', errors='replace').read())

def d3d_compile(src, name, fl_key, stage, defines_extra=()):
    target = '%s_%s' % ('vs' if stage == 'vs' else 'ps', TARGET_SUFFIX[fl_key])
    macros = (Macro * 2)()
    macros[0].Name = b'VERSION'
    macros[0].Definition = str(FL_VALUES[fl_key]).encode()
    macros[1].Name = None; macros[1].Definition = None
    sb = src.encode('utf-8')
    code = c_void_p(); err = c_void_p()
    hr = d3dc.D3DCompile(sb, len(sb), name.encode(), byref(macros), None, b'main', target.encode(), 0x8000, 0,
                         byref(code), byref(err))
    msg = ''
    if err:
        msg = blob_bytes(err).decode('utf-8', 'replace').strip()
        com_release(err)
    bc = blob_bytes(code) if code else None
    if code: com_release(code)
    return hr, bc, msg, target

# ----------------------------------------------------------------------------- materials
def load_json_lenient(path):
    t = open(path, 'r', encoding='utf-8', errors='replace').read()
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    t = re.sub(r'(^|[^:"\\])//[^\n]*', r'\1', t)
    t = re.sub(r',\s*([}\]])', r'\1', t)
    return json.loads(t)

def resolve_materials():
    """returns dict key -> {'vs','fs','defines'(set)} for every material entry of every index file
    (packs: vanilla + vanilla_base; index files common.json / fancy.json / sad.json add the extra defines)."""
    out = {}
    sets = []
    for pack in ('vanilla', 'vanilla_base'):
        mdir = os.path.join(DATA, 'resource_packs', pack, 'materials')
        for idx in ('common.json', 'fancy.json', 'sad.json'):
            ip = os.path.join(mdir, idx)
            if os.path.exists(ip):
                for item in load_json_lenient(ip):
                    sets.append((pack, idx, item))
    for pack, idx, item in sets:
        path = os.path.join(DATA, 'resource_packs', pack, item['path'])
        if not os.path.exists(path): continue
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
            for m in reversed(chain):          # parent first, child overrides
                vs = m.get('vertexShader', vs)
                fs = m.get('fragmentShader', fs)
                defs |= set(m.get('defines', []))
            if vs and fs:
                out['%s/%s|%s%s|%s' % (pack, idx, name.split(':')[0], tag, item['path'].split('/')[-1])] =                     {'vs': vs, 'fs': fs, 'defines': defs | extra, 'idx': idx}
    return out

# ----------------------------------------------------------------------------- D3D11 device test
def make_fl91_device(driver=1):
    d3d11 = ctypes.WinDLL('d3d11.dll')
    d3d11.D3D11CreateDevice.restype = c_int
    levels = (c_uint * 1)(0x9100)
    dev = c_void_p(); ctx = c_void_p(); fl = c_uint(0)
    hr = d3d11.D3D11CreateDevice(None, driver, None, 0x20, levels, 1, 7, byref(dev), byref(fl), byref(ctx))
    return hr, dev, ctx, fl.value

FORMATS = {  # DXGI_FORMAT values used by the game (5BCF90 vertex formats, swap chain, textures)
    'B8G8R8A8_UNORM(87)': 87, 'R8G8B8A8_UNORM(28)': 28, 'R8G8B8A8_UNORM_SRGB(29)': 29, 'R8G8B8A8_SNORM(31)': 31,
    'R16G16B16A16_FLOAT(10)': 10, 'R32G32B32_FLOAT(6)': 6, 'R32G32B32A32_FLOAT(2)': 2, 'R32G32_FLOAT(16)': 16,
    'R16G16_UNORM(35)': 35, 'R16G16_SNORM(37)': 37, 'R16G16_FLOAT(34)': 34, 'R16G16B16A16_UNORM(11)': 11,
    'R10G10B10A2_UNORM(24)': 24, 'R32_FLOAT(41)': 41, 'R8_UNORM(61)': 61, 'R8G8_UNORM(49)': 49,
    'D24_UNORM_S8_UINT(45)': 45, 'D32_FLOAT(40)': 40, 'D16_UNORM(55)': 55, 'R24G8_TYPELESS(44)': 44,
    'R24_UNORM_X8_TYPELESS(46)': 46, 'B5G6R5_UNORM(85)': 85, 'R16_UNORM(56)': 56, 'BC1_UNORM(71)': 71,
    'BC3_UNORM(77)': 77, 'R11G11B10_FLOAT(26)': 26, 'R16G16B16A16_SNORM(13)': 13,
}
FLAGS = [(0x2, 'VB'), (0x4, 'IB'), (0x20, 'TEX2D'), (0x1000, 'MIP'), (0x200, 'SAMPLE'), (0x4000, 'RTV'),
         (0x8000, 'BLEND'), (0x10000, 'DSV'), (0x40000, 'MSAA_RT'), (0x80000, 'MSAA_LOAD')]

def device_probe(results):
    out = {}
    for name, driver in (('hardware', 1), ('warp', 5)):
        hr, dev, ctx, fl = make_fl91_device(driver)
        if hr < 0 or not dev:
            out[name] = {'create_hr': '0x%08X' % (hr & 0xffffffff)}
            continue
        d = {'create_hr': 'S_OK', 'selected_fl': hex(fl)}
        gfl = vfn(dev, 37, c_uint)(dev)
        d['GetFeatureLevel_slot37'] = hex(gfl)
        fmts = {}
        for fname, fv in FORMATS.items():
            sup = c_uint(0)
            h = vfn(dev, 29, c_int, c_uint, POINTER(c_uint))(dev, fv, byref(sup))
            fmts[fname] = ('ERR 0x%08X' % (h & 0xffffffff)) if h < 0 else '+'.join(n for b, n in FLAGS if sup.value & b) or '-'
        d['formats'] = fmts
        # feature support probes
        feats = {}
        def cfs(slot_feature, size, label):
            buf = create_string_buffer(size)
            h = vfn(dev, 33, c_int, c_uint, c_void_p, c_uint)(dev, slot_feature, buf, size)
            feats[label] = ('ERR 0x%08X' % (h & 0xffffffff)) if h < 0 else buf.raw.hex()
        cfs(7, 4, 'D3D9_OPTIONS(7)'); cfs(9, 4, 'D3D9_SHADOW_SUPPORT(9)'); cfs(11, 4, 'D3D9_SIMPLE_INSTANCING(11)')
        cfs(5, 4, 'D3D11_OPTIONS(5)'); cfs(15, 4, 'D3D11_OPTIONS3(15)'); cfs(13, 8, 'D3D9_OPTIONS1(13)')
        d['feature_support'] = feats
        # shader creation test
        shader_fail = []
        n_ok = 0
        for key, r in results.items():
            for st, slot in (('vs', 12), ('ps', 15)):
                bc = r.get(st, {}).get('9_1', {}).get('bytecode')
                if not bc: continue
                buf = create_string_buffer(bc, len(bc))
                sh = c_void_p()
                h = vfn(dev, slot, c_int, c_void_p, c_size_t, c_void_p, POINTER(c_void_p))(dev, buf, len(bc), None, byref(sh))
                if h < 0: shader_fail.append((key, st, '0x%08X' % (h & 0xffffffff)))
                else:
                    n_ok += 1; com_release(sh)
        d['create_shader_ok_9_1'] = n_ok
        d['create_shader_fail_9_1'] = shader_fail
        out[name] = d
        com_release(ctx); com_release(dev)
    return out

# ----------------------------------------------------------------------------- fxc cross-check
def fxc_check(src, target, name):
    if not os.path.exists(FXC): return 'fxc missing'
    with tempfile.TemporaryDirectory() as td:
        p = os.path.join(td, 'x.hlsl')
        open(p, 'w').write(src)
        cmd = [FXC, '/nologo', '/T', target, '/E', 'main', '/O3', '/DVERSION=%d' % (0x9100 if 'level_9_1' in target else 0x9300), p]
        r = subprocess.run(cmd, capture_output=True, text=True)
        return 'OK' if r.returncode == 0 else ('FAIL: ' + (r.stdout + r.stderr).strip().splitlines()[-1][:200])



# ----------------------------------------------------------------------------- host-flow probe (swap chain etc. at FL9_1)
def host_probe():
    """Mimics initialize_game_d3d + InitTargets on a device restricted to {0x9100}."""
    out = {}
    user32 = ctypes.WinDLL('user32.dll'); user32.CreateWindowExW.restype = c_void_p
    user32.CreateWindowExW.argtypes = [c_uint, c_wchar_p, c_wchar_p, c_uint, c_int, c_int, c_int, c_int, c_void_p, c_void_p, c_void_p, c_void_p]
    hwnd = user32.CreateWindowExW(0, 'STATIC', 'fl91probe', 0x10CF0000, 0, 0, 800, 480, None, None, None, None)
    out['hwnd'] = bool(hwnd)
    for name, driver in (('hardware', 1), ('warp', 5)):
        r = {}
        hr, dev, ctx, fl = make_fl91_device(driver)
        r['create'] = '0x%08X sel=%s' % (hr & 0xffffffff, hex(fl))
        if hr < 0: out[name] = r; continue
        def qi(obj, iid_s, label, holder=r):
            o = c_void_p(); g = guid(iid_s)
            h = vfn(obj, 0, c_int, POINTER(GUID), POINTER(c_void_p))(obj, byref(g), byref(o))
            holder[label] = 'S_OK' if h >= 0 else '0x%08X' % (h & 0xffffffff)
            return o if h >= 0 else None
        d3 = qi(dev, '6007896c-3244-4afd-bf18-a6d3beda5023', 'QI IDXGIDevice3')          # 0xDBC2C4
        c2 = qi(ctx, '420d5b32-b90c-4da4-bef0-359f6a24a83a', 'QI ID3D11DeviceContext2')   # 0xDBC3FC
        c1 = qi(ctx, 'bb2c6faa-b5fb-4082-8e6b-388b8cfa90e1', 'QI ID3D11DeviceContext1')
        an = qi(ctx, 'b2daad8b-03d4-4dbf-95eb-32ab4b63d0ab', 'QI ID3DUserDefinedAnnotation')  # 0xDBC3DC
        dxdev = qi(dev, '54ec77fa-1377-44e6-8c32-88fd5f44c84c', 'QI IDXGIDevice')
        if dxdev:
            adapter = c_void_p(); vfn(dxdev, 7, c_int, POINTER(c_void_p))(dxdev, byref(adapter))
            a3 = qi(adapter, '645967a4-1392-4310-a798-8053ce3e93fd', 'QI IDXGIAdapter3')
            fac = c_void_p(); g = guid('7b7166ec-21c7-44ae-b21a-c9ae321ae369')
            vfn(adapter, 6, c_int, POINTER(GUID), POINTER(c_void_p))(adapter, byref(g), byref(fac))
            class Mode(Structure): _fields_ = [('W', c_uint), ('H', c_uint), ('N', c_uint), ('D', c_uint), ('F', c_uint), ('SO', c_uint), ('Sc', c_uint)]
            class SCD(Structure): _fields_ = [('Mode', Mode), ('SC', c_uint), ('SQ', c_uint), ('Usage', c_uint), ('Buffers', c_uint), ('Wnd', c_void_p), ('Windowed', c_int), ('Swap', c_uint), ('Flags', c_uint)]
            d = SCD(); d.Mode = Mode(800, 480, 0, 0, 87, 0, 0); d.SC = 1; d.SQ = 0; d.Usage = 0x20; d.Buffers = 2; d.Wnd = hwnd; d.Windowed = 1; d.Swap = 0; d.Flags = 0
            sc = c_void_p()
            h = vfn(fac, 10, c_int, c_void_p, POINTER(SCD), POINTER(c_void_p))(fac, dev, byref(d), byref(sc))   # IDXGIFactory::CreateSwapChain
            r['CreateSwapChain BGRA 2buf DISCARD'] = 'S_OK' if h >= 0 else '0x%08X' % (h & 0xffffffff)
            if h >= 0:
                tex = c_void_p(); g2 = guid('6f15aaf2-d208-4e89-9ab4-489535d34f9c')   # ID3D11Texture2D
                vfn(sc, 9, c_int, c_uint, POINTER(GUID), POINTER(c_void_p))(sc, 0, byref(g2), byref(tex))
                rtv = c_void_p()
                h = vfn(dev, 9, c_int, c_void_p, c_void_p, POINTER(c_void_p))(dev, tex, None, byref(rtv))
                r['CreateRenderTargetView(backbuffer)'] = 'S_OK' if h >= 0 else '0x%08X' % (h & 0xffffffff)
                class TD(Structure): _fields_ = [('W', c_uint), ('H', c_uint), ('Mip', c_uint), ('Arr', c_uint), ('Fmt', c_uint), ('SC', c_uint), ('SQ', c_uint), ('Usage', c_uint), ('Bind', c_uint), ('CPU', c_uint), ('Misc', c_uint)]
                for fmt, nm in ((45, 'D24_UNORM_S8_UINT'), (40, 'D32_FLOAT')):
                    td = TD(800, 480, 1, 1, fmt, 1, 0, 0, 0x40, 0, 0); dt = c_void_p()
                    h = vfn(dev, 5, c_int, c_void_p, c_void_p, POINTER(c_void_p))(dev, byref(td), None, byref(dt))
                    r['CreateTexture2D depth %s (InitTargets uses 0x2d)' % nm] = 'S_OK' if h >= 0 else '0x%08X' % (h & 0xffffffff)
                    if h >= 0:
                        dsv = c_void_p(); h = vfn(dev, 10, c_int, c_void_p, c_void_p, POINTER(c_void_p))(dev, dt, None, byref(dsv))
                        r['CreateDepthStencilView %s' % nm] = 'S_OK' if h >= 0 else '0x%08X' % (h & 0xffffffff)
                        com_release(dsv); com_release(dt)
                h = vfn(sc, 8, c_int, c_uint, c_uint)(sc, 1, 0)   # Present(1,0)
                r['Present(1,0)'] = 'S_OK' if h >= 0 else '0x%08X' % (h & 0xffffffff)
                com_release(rtv); com_release(tex); com_release(sc)
        out[name] = r
        for o in (d3, c2, c1, an): com_release(o)
        com_release(ctx); com_release(dev)
    return out

# ----------------------------------------------------------------------------- extras (controls / non-default game defines)
def compile_raw(src, name, target, fl_val):
    macros = (Macro * 2)()
    macros[0].Name = b'VERSION'; macros[0].Definition = str(fl_val).encode()
    sb = src.encode('utf-8'); code = c_void_p(); err = c_void_p()
    hr = d3dc.D3DCompile(sb, len(sb), name.encode(), byref(macros), None, b'main', target.encode(), 0x8000, 0, byref(code), byref(err))
    msg = blob_bytes(err).decode('utf-8', 'replace').strip() if err else ''
    if err: com_release(err)
    if code: com_release(code)
    return hr, msg

def run_extras():
    rows = []
    def add(what, hr, msg):
        rows.append((what, 'OK' if hr >= 0 else 'FAIL', msg.replace(chr(10), ' | ')[:220]))
    # negative control
    hr, m = compile_raw('float4 main(float4 p:POSITION):SV_Position{return undefinedSymbol;}', 'neg', 'vs_4_0_level_9_1', 0x9100)
    add('negative control (undefined symbol) vs_4_0_level_9_1', hr, m)
    # geometry shader targets
    for t in ('gs_4_0_level_9_1', 'gs_4_0', 'gs_5_0'):
        src = 'struct V{float4 p:SV_Position;}; [maxvertexcount(3)] void main(triangle V i[3], inout TriangleStream<V> o){o.Append(i[0]);}'
        hr, m = compile_raw(src, 'gs', t, 0x9100)
        add('trivial geometry shader target %s' % t, hr, m)
    # non-default game defines on representative shaders
    VR = ['VR_MODE', 'INSTANCEDSTEREO', 'GEOMETRY_INSTANCEDSTEREO']
    VR2 = ['VR_MODE', 'INSTANCEDSTEREO', 'VERTEXSHADER_INSTANCEDSTEREO']
    cases = [
        ('shaders/renderchunk.vertex', 'vs', ['LOW_PRECISION', 'TEXEL_AA', 'ATLAS_TEXTURE', 'TEXEL_AA_FEATURE'], 'TEXEL_AA_FEATURE (game: displaySettings+0x31)'),
        ('shaders/renderchunk.fragment', 'ps', ['LOW_PRECISION', 'TEXEL_AA', 'ATLAS_TEXTURE', 'TEXEL_AA_FEATURE'], 'TEXEL_AA_FEATURE'),
        ('shaders/renderchunk.fragment', 'ps', ['LOW_PRECISION', 'TEXEL_AA', 'ATLAS_TEXTURE', 'MSAA_FRAMEBUFFER_ENABLED'], 'MSAA_FRAMEBUFFER_ENABLED (game: hasFeature(1) && displaySettings+0xc>1)'),
        ('shaders/renderchunk.vertex', 'vs', ['LOW_PRECISION', 'TEXEL_AA', 'ATLAS_TEXTURE'] + VR, 'VR GEOMETRY_INSTANCEDSTEREO (vs side)'),
        ('shaders/renderchunk.vertex', 'vs', ['LOW_PRECISION', 'TEXEL_AA', 'ATLAS_TEXTURE'] + VR2, 'VR VERTEXSHADER_INSTANCEDSTEREO (vs side)'),
        ('shaders/renderchunk.fragment', 'ps', ['LOW_PRECISION', 'TEXEL_AA', 'ATLAS_TEXTURE'] + VR, 'VR (ps side)'),
    ]
    for shader, stage, defs, what in cases:
        for fl in ('9_1', '9_3'):
            d2 = set(defs) | {'R8G8B8A8_SNORM_UNSUPPORTED'}
            src = build_source(shader, d2)
            hr, bc, msg, tgt = d3d_compile(src, shader + '.hlsl', fl, stage)
            add('%s %s @%s' % (shader.split('/')[-1], what, fl), hr, msg)
    # GUID-dir overrides (data/shaders/<uuid>/hlsl/*.hlsl) paired with the vanilla vertex shader
    base = os.path.join(DATA, 'shaders')
    for d in sorted(os.listdir(base)):
        hd = os.path.join(base, d, 'hlsl')
        if re.match(r'[0-9a-f]{8}-', d) and os.path.isdir(hd):
            for fn in os.listdir(hd):
                txt = '#define R8G8B8A8_SNORM_UNSUPPORTED\n#define LOW_PRECISION\n' + expand(open(os.path.join(hd, fn), encoding='utf-8', errors='replace').read())
                for fl in ('9_1',):
                    hr, bc, msg, tgt = d3d_compile(txt, d + '/' + fn, fl, 'ps' if 'fragment' in fn else 'vs')
                    add('GUID override %s/%s @%s' % (d[:8], fn, fl), hr, msg)
    return rows

# ----------------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--device', action='store_true')
    ap.add_argument('--fxc', action='store_true')
    ap.add_argument('--host', action='store_true')
    ap.add_argument('--json')
    ap.add_argument('--only')
    ap.add_argument('-v', action='store_true')
    a = ap.parse_args()

    mats = resolve_materials()
    variants = {}   # (vs, fs, frozenset(defs)) -> [material keys]
    for k, m in mats.items():
        variants.setdefault((m['vs'], m['fs'], frozenset(m['defines'])), []).append(k)
    print('materials: %d entries -> %d unique (vs,fs,defines) variants' % (len(mats), len(variants)))

    results = {}
    cache = {}
    for (vs, fs, defs), keys in sorted(variants.items(), key=lambda kv: (kv[0][0], kv[0][1], sorted(kv[0][2]))):
        label = '%s + %s [%s]' % (vs.split('/')[-1], fs.split('/')[-1], ','.join(sorted(defs)))
        if a.only and a.only not in label: continue
        entry = {'keys': keys, 'vs': {}, 'ps': {}}
        for stage, shader in (('vs', vs), ('ps', fs)):
            for fl in ('9_1', '9_3', '10_0'):
                d2 = set(defs)
                if fl != '10_0': d2.add('R8G8B8A8_SNORM_UNSUPPORTED')   # ShaderDefines_build: !hasFeature(6) (FL<10_0)
                ck = (shader, frozenset(d2), fl, stage)
                if ck in cache:
                    entry[stage][fl] = cache[ck]; continue
                src = build_source(shader, d2)
                if src is None:
                    res = {'ok': False, 'error': 'source not found'}
                else:
                    hr, bc, msg, tgt = d3d_compile(src, shader + '.hlsl', fl, stage)
                    res = {'ok': hr >= 0 and bc is not None, 'hr': '0x%08X' % (hr & 0xffffffff), 'target': tgt,
                           'error': '' if hr >= 0 else msg[:600], 'bytecode': bc, 'bytes': len(bc) if bc else 0}
                    if res['ok']:
                        res['refl'] = reflect(bc)
                    entry['_src_' + stage + fl] = src if (a.fxc and fl != '10_0') else None
                cache[ck] = res
                entry[stage][fl] = res
        results[label] = entry

    # ---- summary
    def stat(stage, fl):
        tot = ok = 0
        for r in results.values():
            tot += 1; ok += 1 if r[stage][fl]['ok'] else 0
        return ok, tot
    print('\nvariant compile matrix (variants, not unique files):')
    for stage in ('vs', 'ps'):
        for fl in ('9_1', '9_3', '10_0'):
            ok, tot = stat(stage, fl)
            print('  %s %-5s %d/%d OK' % (stage, fl, ok, tot))

    # unique (file, defines) failures at 9_1
    print('\nfailures at 9_1 (unique shader/defines):')
    seenf = set()
    for label, r in results.items():
        for stage in ('vs', 'ps'):
            x = r[stage]['9_1']
            if not x['ok']:
                sig = (label.split(' + ')[0 if stage == 'vs' else 1].split(' [')[0], x['error'][:120])
                if sig in seenf: continue
                seenf.add(sig)
                print('  [%s] %s -> %s' % (stage, label, x['error'].replace('\n', ' | ')[:300]))
    print('\nper-file 9_1 vs 9_3 vs 10_0 status (first variant of each file):')
    files = {}
    for label, r in results.items():
        vs, rest = label.split(' + ', 1)
        fsn = rest.split(' [')[0]
        for stage, nm in (('vs', vs), ('ps', fsn)):
            files.setdefault((stage, nm), []).append(r[stage])
    for (stage, nm), lst in sorted(files.items()):
        row = []
        for fl in ('9_1', '9_3', '10_0'):
            okc = sum(1 for x in lst if x[fl]['ok'])
            row.append('%s %d/%d' % (fl, okc, len(lst)))
        mx = max([x['9_1'].get('refl', {}) and x['9_1']['refl']['instr'] or 0 for x in lst] or [0])
        print('  %s %-34s %s  maxInstr@9_1=%s' % (stage, nm, ' | '.join(row), mx))

    print('\nextras / controls:')
    for r in run_extras():
        print('  %-70s %-4s %s' % r)

    if a.fxc:
        print('\nfxc cross-check (first variant per file):')
        done = set()
        for label, r in results.items():
            for stage in ('vs', 'ps'):
                nm = label.split(' + ')[0 if stage == 'vs' else 1].split(' [')[0]
                if (stage, nm) in done: continue
                done.add((stage, nm))
                for fl in ('9_1', '9_3'):
                    src = r.get('_src_' + stage + fl)
                    if not src: continue
                    t = '%s_%s' % (stage, TARGET_SUFFIX[fl])
                    print('  %-30s %s fxc: %s | d3dcompiler_47: %s' % (nm, t, fxc_check(src, t, nm),
                          'OK' if r[stage][fl]['ok'] else 'FAIL'))

    if a.device:
        print('\nD3D11 device restricted to {0x9100}:')
        dp = device_probe(results)
        print(json.dumps(dp, indent=1)[:6000])
        results_dev = dp
    else:
        results_dev = None

    if a.host:
        print('\nhost-flow probe (initialize_game_d3d at FL9_1):')
        print(json.dumps(host_probe(), indent=1))

    if a.json:
        slim = {}
        for label, r in results.items():
            slim[label] = {'materials': r['keys'][:6]}
            for stage in ('vs', 'ps'):
                slim[label][stage] = {fl: {k: v for k, v in x.items() if k != 'bytecode'} for fl, x in r[stage].items()}
        json.dump({'variants': slim, 'device': results_dev}, open(a.json, 'w'), indent=1)
        print('wrote', a.json)

if __name__ == '__main__':
    main()
