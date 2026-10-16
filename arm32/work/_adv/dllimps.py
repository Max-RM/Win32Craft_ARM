import struct,sys
def imports(path):
    d=open(path,'rb').read()
    pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]
    opt=pe+24; sec=opt+struct.unpack_from('<H',d,pe+20)[0]
    secs=[struct.unpack_from('<8sIIII',d,sec+40*i) for i in range(nsec)]
    def r2o(r):
        for n,vs,va,rs,rp in secs:
            if va<=r<va+max(vs,rs): return r-va+rp
    def cstr(r):
        o=r2o(r); return d[o:d.index(b'\0',o)].decode('latin1')
    dd=opt+96
    res={}
    for idx,delay in ((1,False),(13,True)):
        rva,sz=struct.unpack_from('<II',d,dd+8*idx)
        if not rva: continue
        o=r2o(rva)
        while True:
            if not delay:
                oft,ts,fc,nm,ft=struct.unpack_from('<IIIII',d,o); o+=20
                if not nm: break
                n=oft or ft
            else:
                at,nm,hm,iat,intr,bi,ui,ts=struct.unpack_from('<IIIIIIII',d,o); o+=32
                if not nm: break
                n=intr
            names=[];i=0
            while True:
                e,=struct.unpack_from('<I',d,r2o(n)+4*i)
                if not e: break
                names.append(cstr(e+2) if not e&0x80000000 else '#%d'%(e&0xffff)); i+=1
            res[cstr(nm)+(' (delay)' if delay else '')]=names
    return res
if __name__=='__main__':
    for p in sys.argv[1:]:
        r=imports(p)
        print('==',p)
        for k in sorted(r): print('  ',k,len(r[k]))
