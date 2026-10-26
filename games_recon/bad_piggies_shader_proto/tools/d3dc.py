import ctypes, os
from ctypes import c_void_p, c_size_t, c_uint, c_char_p, POINTER, byref

_dll = ctypes.WinDLL(r'D:\Windows Kits\10\bin\10.0.19041.0\x64\d3dcompiler_47.dll')
_dis = _dll.D3DDisassemble
_dis.argtypes = [c_void_p, c_size_t, c_uint, c_char_p, POINTER(c_void_p)]
_dis.restype = ctypes.c_long
_comp = _dll.D3DCompile
_comp.argtypes = [c_void_p, c_size_t, c_char_p, c_void_p, c_void_p, c_char_p, c_char_p, c_uint, c_uint, POINTER(c_void_p), POINTER(c_void_p)]
_comp.restype = ctypes.c_long

def _blob_bytes(pblob):
    vt = ctypes.cast(pblob, POINTER(POINTER(c_void_p)))[0]
    GetBufferPointer = ctypes.WINFUNCTYPE(c_void_p, c_void_p)(vt[3])
    GetBufferSize = ctypes.WINFUNCTYPE(c_size_t, c_void_p)(vt[4])
    Release = ctypes.WINFUNCTYPE(c_uint, c_void_p)(vt[2])
    p = GetBufferPointer(pblob); n = GetBufferSize(pblob)
    data = ctypes.string_at(p, n)
    Release(pblob)
    return data

def disassemble(code, flags=0):
    buf = ctypes.create_string_buffer(bytes(code), len(code))
    out = c_void_p()
    hr = _dis(buf, len(code), flags, None, byref(out))
    if hr != 0:
        raise OSError('D3DDisassemble hr=0x%08x' % (hr & 0xffffffff))
    return _blob_bytes(out).decode('ascii', 'replace').rstrip('\0')

D3DCOMPILE_OPTIMIZATION_LEVEL3 = 1 << 15
def compile(src, target, entry='main', flags=D3DCOMPILE_OPTIMIZATION_LEVEL3, name='src.hlsl'):
    """returns (bytecode or None, messages str)"""
    b = src.encode() if isinstance(src, str) else src
    code = c_void_p(); err = c_void_p()
    hr = _comp(b, len(b), name.encode(), None, None, entry.encode(), target.encode(), flags, 0, byref(code), byref(err))
    msg = _blob_bytes(err).decode('ascii', 'replace').rstrip('\0') if err.value else ''
    bc = _blob_bytes(code) if (hr == 0 and code.value) else None
    return bc, msg
