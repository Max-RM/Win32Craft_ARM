import sys, struct
sys.path.insert(0,'D:/repo/Win32Craft-main/tools/arm32')
import os
os.environ['ARM32_EXE']=r'D:\repo\Win32Craft-main\Minecraft_1.16.40.02\Minecraft.Windows.exe'
import arm32re
c = arm32re.Ctx()
pe = c.pe
def rd(rva,n):
    return pe.bytes(rva,n)
def thunk(rva):
    """decode 'subs/adds r0,#N ; ldr.w ip,[pc,#4]; bx ip; .word target' thunks"""
    b = rd(rva,16)
    adj=None
    # subs r0,r0,#imm3 : 0x1f00 | ... ; subs r0,#imm8 : 0x38xx
    h = struct.unpack('<H',b[:2])[0]
    off=2
    if (h & 0xF800)==0x3800 and (h&0x0700)==0:
        adj=-(h&0xff)
    elif (h & 0xF800)==0x3000 and (h&0x0700)==0:
        adj=(h&0xff)
    elif (h & 0xFFC0)==0x1e00 or (h&0xFE00)==0x1e00:
        adj=-((h>>6)&7)
    elif (h & 0xFE00)==0x1c00:
        adj=((h>>6)&7)
    else:
        return None
    if b[2:4]==b'\xdf\xf8' or b[2:6]==b'\xdf\xf8\x04\xc0':
        pass
    if b[2:6]==bytes([0xdf,0xf8,0x04,0xc0]) and b[6:8]==b'\x60\x47':
        tgt = struct.unpack('<I',b[12:16])[0] if False else None
        # pool at rva+8? ldr.w ip,[pc,#4] at rva+2 : pc=(rva+2+4)&~3 =rva+4 aligned -> +4 => 
        pc=((rva+2)+4)&~3
        t=struct.unpack('<I',rd(pc+4,4))[0]
        return adj,(t-0x400000)&~1
    return ('?',b.hex())
if __name__=='__main__':
    for a in sys.argv[1:]:
        r=int(a,16)
        t=thunk(r); print(a, (t[0], hex(t[1])) if t and t[0]!="?" else t)
