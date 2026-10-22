import sys, os, ctypes
from ctypes import c_void_p, byref
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shader_probe_0104 as p
sp = p.sp
fn = 'renderchunk.fragment.hlsl'
src = '#define FOG\n' + open(os.path.join(p.HLSL, fn)).read()   # raw, with #include "ShaderConstants.fxh"
def go(label):
    sb = src.encode()
    code = c_void_p(); err = c_void_p()
    hr = sp.d3dc.D3DCompile(sb, len(sb), b'data/shaders/dx11/' + fn.encode(), None, ctypes.c_void_p(1), b'main', b'ps_4_0_level_9_1', 0x8000, 0, byref(code), byref(err))
    msg = sp.blob_bytes(err).decode(errors='replace').strip().splitlines()[0] if err else ''
    print(label, 'cwd=', os.getcwd(), 'hr=0x%08X' % (hr & 0xffffffff), msg[:140])
sp.d3dc.D3DCompile.argtypes = [c_void_p, ctypes.c_size_t, ctypes.c_char_p, c_void_p, c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(c_void_p), ctypes.POINTER(c_void_p)]
os.chdir(p.GAME); go('game dir   ')
os.chdir(os.environ.get('TEMP', '.')); go('other dir  ')
