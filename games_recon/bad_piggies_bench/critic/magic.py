import sys,os,struct
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import arm32re
c=arm32re.Ctx(); pe=c.pe
t=pe.text; buf=bytes(pe.img[t['va']:t['va']+t['vs']])
mags={'SHDR':0x52444853,'SHEX':0x58454853,'OSGN':0x4e47534f,'RDEF':0x46454452,'SFI0':0x30494653,'ISGN':0x4e475349,'Aon9':0x396e6f41,'ISG1':0x31475349,'OSG1':0x3147534f,'DXBC':0x43425844}
for n,v in mags.items():
    pat=struct.pack('<I',v); i=0; hits=[]
    while True:
        i=buf.find(pat,i)
        if i<0: break
        if (i&3)==0: hits.append(t['va']+i)
        i+=1
    fs=[]
    for h in hits:
        f=c.func_of(h)
        # literal pool: the function before it
        if not f:
            import bisect
            j=bisect.bisect_right(c.idx.fstarts,h)-1; f=c.idx.funcs[j]
        fs.append('%x(%s)'%(h,c.fname(f[0])))
    print(n, len(hits), ' '.join(fs[:12]))
