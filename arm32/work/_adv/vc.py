import struct, capstone
p=r'C:\Windows\System32\vccorlib140.dll'
d=open(p,'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]
nsec=struct.unpack_from('<H',d,pe+6)[0]
opt=pe+24
magic=struct.unpack_from('<H',d,opt)[0]
dd=opt+(112 if magic==0x20b else 96)
exp_rva,exp_sz=struct.unpack_from('<II',d,dd)
sec=opt+struct.unpack_from('<H',d,pe+20)[0]
secs=[struct.unpack_from('<8sIIII',d,sec+40*i) for i in range(nsec)]
def r2o(r):
    for n,vs,va,rs,rp in secs:
        if va<=r<va+max(vs,rs): return r-va+rp
exp=r2o(exp_rva)
nnames,=struct.unpack_from('<I',d,exp+24); fn,no,ordt=struct.unpack_from('<III',d,exp+28)
fn=r2o(fn+0) if False else None
base_f,base_n,base_o=struct.unpack_from('<III',d,exp+28)
for i in range(nnames):
    nr,=struct.unpack_from('<I',d,r2o(base_n)+4*i)
    nm=d[r2o(nr):d.index(b'\0',r2o(nr))].decode()
    if 'InitializeData' in nm:
        o,=struct.unpack_from('<H',d,r2o(base_o)+2*i); f,=struct.unpack_from('<I',d,r2o(base_f)+4*o)
        print(nm,hex(f))
        md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
        code=d[r2o(f):r2o(f)+0x120]
        for ins in md.disasm(code,f):
            print(hex(ins.address),ins.mnemonic,ins.op_str)
        break
