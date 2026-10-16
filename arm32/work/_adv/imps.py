import struct,sys
d=open(r'D:\repo\Win32Craft-main\dist-arm32\Win32Craft.1.2.8-arm32.exe','rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]
opt=pe+24; sec=opt+struct.unpack_from('<H',d,pe+20)[0]
secs=[struct.unpack_from('<8sIIII',d,sec+40*i) for i in range(nsec)]
def r2o(r):
    for n,vs,va,rs,rp in secs:
        if va<=r<va+max(vs,rs): return r-va+rp
    raise Exception(hex(r))
def cstr(r):
    o=r2o(r); return d[o:d.index(b'\0',o)].decode('latin1')
dd=opt+96
imp,_=struct.unpack_from('<II',d,dd+8); dly,_=struct.unpack_from('<II',d,dd+8*13)
out={}
o=r2o(imp)
while True:
    oft,ts,fc,nm,ft=struct.unpack_from('<IIIII',d,o); o+=20
    if not nm and not ft: break
    dll=cstr(nm); i=0
    t=oft or ft
    while True:
        e,=struct.unpack_from('<I',d,r2o(t)+4*i)
        if not e: break
        if not e&0x80000000: out.setdefault(cstr(e+2),[]).append((dll,ft+4*i))
        else: out.setdefault('#%d'%(e&0xffff),[]).append((dll,ft+4*i))
        i+=1
o=r2o(dly)
while True:
    at,nm,hm,iat,intr,bi,ui,ts=struct.unpack_from('<IIIIIIII',d,o); o+=32
    if not nm: break
    dll=cstr(nm); i=0
    while True:
        e,=struct.unpack_from('<I',d,r2o(intr)+4*i)
        if not e: break
        out.setdefault(cstr(e+2),[]).append((dll+' (delay)',iat+4*i)); i+=1
for k in sys.argv[1:]:
    for kk,v in out.items():
        if k in kk: print(kk,[(a,hex(b)) for a,b in v])
