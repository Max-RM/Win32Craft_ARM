"""Replays, on a real FL9_1-only D3D11 device, the calls DX::DeviceResources makes (RVAs 0x1dafc0 / 0x1db158) to see which
survive off the XAML path: QI ID3D11Device2 / IDXGIDevice3 / ID3D11DeviceContext2 / ID3DUserDefinedAnnotation, the game's own
5-entry feature level list, CreateSwapChainForComposition vs CreateSwapChainForHwnd with the exact DXGI_SWAP_CHAIN_DESC1
(FLIP_SEQUENTIAL, 2 buffers, B8G8R8A8, STRETCH, ALPHA_IGNORE), IDXGISwapChain2::SetMatrixTransform, SetMaximumFrameLatency(1),
GetBuffer/CreateRenderTargetView, D24S8 depth texture/DSV, Present(0,0).  Prints one line per step."""
import ctypes, sys, os
from ctypes import c_void_p, c_uint, c_int, c_float, byref, POINTER, Structure, sizeof
sys.path.insert(0, r'D:\repo\Win32Craft-main\arm32\tests')
import shader_probe as sp
vfn, guid, com_release = sp.vfn, sp.guid, sp.com_release
GUID = sp.GUID

def hrs(h): return 'S_OK' if h >= 0 else '0x%08X' % (h & 0xffffffff)

class SCD1(Structure):
    _fields_ = [('Width', c_uint), ('Height', c_uint), ('Format', c_uint), ('Stereo', c_int), ('SC', c_uint), ('SQ', c_uint),
                ('Usage', c_uint), ('Buffers', c_uint), ('Scaling', c_uint), ('SwapEffect', c_uint), ('AlphaMode', c_uint), ('Flags', c_uint)]
class M32(Structure):
    _fields_ = [('_11', c_float), ('_12', c_float), ('_21', c_float), ('_22', c_float), ('_31', c_float), ('_32', c_float)]
class TD(Structure):
    _fields_ = [('W', c_uint), ('H', c_uint), ('Mip', c_uint), ('Arr', c_uint), ('Fmt', c_uint), ('SC', c_uint), ('SQ', c_uint), ('Usage', c_uint), ('Bind', c_uint), ('CPU', c_uint), ('Misc', c_uint)]

def qi(obj, iid):
    o = c_void_p(); g = guid(iid)
    h = vfn(obj, 0, c_int, POINTER(GUID), POINTER(c_void_p))(obj, byref(g), byref(o))
    return h, (o if h >= 0 else None)

def run(driver_name, driver, fl_list, label):
    print('=== %s / %s' % (driver_name, label))
    d3d11 = ctypes.WinDLL('d3d11.dll'); d3d11.D3D11CreateDevice.restype = c_int
    levels = (c_uint * len(fl_list))(*fl_list)
    dev = c_void_p(); ctx = c_void_p(); fl = c_uint(0)
    hr = d3d11.D3D11CreateDevice(None, driver, None, 0, levels, len(fl_list), 7, byref(dev), byref(fl), byref(ctx))
    print(' D3D11CreateDevice flags=0 levels=%s -> %s fl=%s' % ([hex(x) for x in fl_list], hrs(hr), hex(fl.value)))
    if hr < 0: return
    for nm, iid, o in (('ID3D11Device2', '9d06dffa-d1e5-4d07-83a8-1bb123f2f841', dev), ('ID3D11DeviceContext2', '420d5b32-b90c-4da4-bef0-359f6a24a83a', ctx),
                       ('ID3DUserDefinedAnnotation(ctx)', 'b2daad8b-03d4-4dbf-95eb-32ab4b63d0ab', ctx)):
        h, p = qi(o, iid); print(' QI %-32s %s' % (nm, hrs(h))); com_release(p)
    h, dxdev3 = qi(dev, '6007896c-3244-4afd-bf18-a6d3beda5023'); print(' QI IDXGIDevice3 %s' % hrs(h))
    h = vfn(dxdev3, 12, c_int, c_uint)(dxdev3, 1); print(' IDXGIDevice1::SetMaximumFrameLatency(1) [slot 12] %s' % hrs(h))
    ad = c_void_p(); h = vfn(dxdev3, 7, c_int, POINTER(c_void_p))(dxdev3, byref(ad)); print(' GetAdapter %s' % hrs(h))
    fac = c_void_p(); g = guid('50c83a1c-e072-4c48-87b0-3630fa36a6d0')
    h = vfn(ad, 6, c_int, POINTER(GUID), POINTER(c_void_p))(ad, byref(g), byref(fac)); print(' Adapter::GetParent(IDXGIFactory2) %s' % hrs(h))
    W, H = 1366, 768
    desc = SCD1(W, H, 87, 0, 1, 0, 0x20, 2, 0, 3, 3, 0)
    # (a) ForComposition (slot 24) - what the exe calls
    sc = c_void_p()
    h = vfn(fac, 24, c_int, c_void_p, POINTER(SCD1), c_void_p, POINTER(c_void_p))(fac, dev, byref(desc), None, byref(sc))
    print(' CreateSwapChainForComposition (slot 24, game desc) %s' % hrs(h))
    if h >= 0:
        h2, sc2 = qi(sc, 'a8be2ac4-199f-4946-b331-79599fb98de7'); print('  QI IDXGISwapChain2 %s' % hrs(h2))
        m = M32(1, 0, 0, 1, 0, 0)
        h3 = vfn(sc2, 34, c_int, POINTER(M32))(sc2, byref(m)); print('  SetMatrixTransform (slot 34 = 0x88) on composition SC %s' % hrs(h3))
        tex = c_void_p(); g2 = guid('6f15aaf2-d208-4e89-9ab4-489535d34f9c')
        h4 = vfn(sc, 9, c_int, c_uint, POINTER(GUID), POINTER(c_void_p))(sc, 0, byref(g2), byref(tex)); print('  GetBuffer(0, ID3D11Texture2D) %s' % hrs(h4))
        rtv = c_void_p(); h5 = vfn(dev, 9, c_int, c_void_p, c_void_p, POINTER(c_void_p))(dev, tex, None, byref(rtv)); print('  CreateRenderTargetView %s' % hrs(h5))
        h6 = vfn(sc, 8, c_int, c_uint, c_uint)(sc, 0, 0); print('  Present(0,0) with no visual attached %s' % hrs(h6))
        h7 = vfn(sc, 13, c_int, c_uint, c_uint, c_uint, c_uint, c_uint)(sc, 2, 1024, 600, 87, 0)
        print('  ResizeBuffers(2,1024,600) while RTV alive (expected failure) %s' % hrs(h7))
        com_release(rtv); com_release(tex); com_release(sc2); com_release(sc)
    # (b) HWND swap chain with the same desc
    user32 = ctypes.WinDLL('user32.dll'); user32.CreateWindowExW.restype = c_void_p
    user32.CreateWindowExW.argtypes = [c_uint, ctypes.c_wchar_p, ctypes.c_wchar_p, c_uint, c_int, c_int, c_int, c_int, c_void_p, c_void_p, c_void_p, c_void_p]
    hwnd = user32.CreateWindowExW(0, 'STATIC', 'probe', 0x10CF0000, 0, 0, W, H, None, None, None, None)
    for lab, scaling, eff in (('game desc (STRETCH, FLIP_SEQUENTIAL)', 0, 3), ('SCALING_NONE, FLIP_SEQUENTIAL', 1, 3), ('STRETCH, DISCARD(bitblt, 1 buf)', 0, 0)):
        d = SCD1(W, H, 87, 0, 1, 0, 0x20, 2 if eff else 1, scaling, eff, 0 if eff == 0 else 3, 0)
        sc = c_void_p()
        h = vfn(fac, 15, c_int, c_void_p, c_void_p, POINTER(SCD1), c_void_p, c_void_p, POINTER(c_void_p))(fac, dev, hwnd, byref(d), None, None, byref(sc))
        print(' CreateSwapChainForHwnd [%s] %s' % (lab, hrs(h)))
        if h >= 0:
            h2, sc2 = qi(sc, 'a8be2ac4-199f-4946-b331-79599fb98de7')
            m = M32(1, 0, 0, 1, 0, 0)
            h3 = vfn(sc2, 34, c_int, POINTER(M32))(sc2, byref(m)) if sc2 else -1
            print('   QI IDXGISwapChain2 %s ; SetMatrixTransform on HWND SC %s' % (hrs(h2), hrs(h3)))
            h6 = vfn(sc, 8, c_int, c_uint, c_uint)(sc, 0, 0); print('   Present(0,0) %s' % hrs(h6))
            com_release(sc2); com_release(sc)
    # depth texture exactly as 0x1db66a..0x1db6c8
    td = TD(W, H, 1, 1, 45, 1, 0, 0, 0x40, 0, 0); dt = c_void_p()
    h = vfn(dev, 5, c_int, c_void_p, c_void_p, POINTER(c_void_p))(dev, byref(td), None, byref(dt)); print(' CreateTexture2D D24_UNORM_S8_UINT DSV %dx%d %s' % (W, H, hrs(h)))
    td = TD(4096, 4096, 1, 1, 87, 1, 0, 0, 0x20, 0, 0); t2 = c_void_p()
    h = vfn(dev, 5, c_int, c_void_p, c_void_p, POINTER(c_void_p))(dev, byref(td), None, byref(t2)); print(' CreateTexture2D 4096x4096 (limit check; 9_1 max is 2048) %s' % hrs(h))
    com_release(dt); com_release(t2); com_release(fac); com_release(ad); com_release(dxdev3); com_release(ctx); com_release(dev)

GAME_LIST = [0xb100, 0xb000, 0xa100, 0xa000, 0x9300]
if __name__ == '__main__':
    for name, drv in (('hardware', 1), ('warp', 5)):
        run(name, drv, GAME_LIST, 'GAME LIST as in exe (0x1dafda..0x1daff2) - expect failure on a 9_1-only GPU / ok on this PC')
        run(name, drv, [0x9100], 'host-clamped {0x9100}')
