import struct,sys
p=sys.argv[1] if len(sys.argv)>1 else r'D:\repo\Win32Craft-main\Minecraft_1.2.8.0_ARM32\fmod_ARM.dll'
d=open(p,'rb').read()
u16=lambda o:struct.unpack_from('<H',d,o)[0]
u32=lambda o:struct.unpack_from('<I',d,o)[0]
pe=u32(0x3c)
nsec=u16(pe+6); osz=u16(pe+20)
opt=pe+24
print('pe',hex(pe),'opt',hex(opt),'DllChar file off',hex(opt+70),hex(u16(opt+70)))
secs=[]
so=opt+osz
for i in range(nsec):
    o=so+i*40
    name=d[o:o+8].rstrip(b'\0').decode()
    vs,va,rs,rp=struct.unpack_from('<IIII',d,o+8)
    secs.append((name,va,vs,rp,rs))
    print(name,hex(va),hex(vs),hex(rp),hex(rs))
def r2f(r):
    for n,va,vs,rp,rs in secs:
        if va<=r<va+max(vs,rs): return r-va+rp
    return r if r<0x400 else None
def cstr(o):
    e=d.index(b'\0',o);return d[o:e].decode('latin1')
imp=u32(opt+96+8)
print('imp dir rva',hex(imp),'file',hex(r2f(imp)))
o=r2f(imp);i=0
while True:
    oft,ts,fc,name,ft=struct.unpack_from('<IIIII',d,o)
    if not name: break
    nf=r2f(name)
    print('#%d desc@%x name rva %x file %x %s IAT rva %x'%(i,o,name,nf,cstr(nf),ft))
    t=r2f(oft or ft);k=0
    while True:
        v=u32(t)
        if v==0:break
        if v&0x80000000: print('    ord',v&0xffff,'cell rva %x'%(ft+4*k))
        else:
            hn=r2f(v+0)
            print('    %s  (hint-name file %x) cell rva %x'%(cstr(hn+2),hn+2,ft+4*k))
        t+=4;k+=1
    o+=20;i+=1
