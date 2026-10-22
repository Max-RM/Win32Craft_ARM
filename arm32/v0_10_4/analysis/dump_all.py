import sys, os, pickle
sys.path.insert(0, r'D:\repo\Win32Craft-main\tools\arm32')
os.environ['ARM32_EXE'] = r'D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]\MCPE.Host.WindowsPhone.exe'
import arm32re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
c = arm32re.Ctx()
pe = c.pe
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
out = {}
for (b, l) in c.idx.funcs:
    ins = []
    for (ia, isz, mn, ops) in md.disasm_lite(bytes(pe.img[b:b + l]), b):
        ins.append((ia, isz, mn, ops))
    out[b] = (l, ins)
pickle.dump(out, open(r'D:\repo\Win32Craft-main\arm32\v0_10_4\analysis\all_dis.pkl', 'wb'))
print(len(out))
