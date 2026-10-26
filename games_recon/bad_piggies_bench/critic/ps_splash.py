import struct,sys
p=r'D:\repo\Win32Craft-main\BadPiggiesRebooted_1.0.0.0_ARM\Data\globalgamemanagers'
b=open(p,'rb').read()
i=b.find(b'Bad Piggies::Rebooted')
print('productName at',hex(i))
o=i+len(b'Bad Piggies::Rebooted'); o=(o+3)&~3
fid,pid=struct.unpack_from('<iq',b,o); o+=12
hx,hy=struct.unpack_from('<ff',b,o); o+=8
col=struct.unpack_from('<4f',b,o); o+=16
show,logo=b[o],b[o+1]; o+=4
op,anim,style,mode,bz,lz,la,pa=struct.unpack_from('<fiiiffff',b,o); o+=32
luv=struct.unpack_from('<4f',b,o); o+=16; puv=struct.unpack_from('<4f',b,o); o+=16
n=struct.unpack_from('<i',b,o)[0]; o+=4
print('defaultCursor',fid,pid,'hotspot',hx,hy,'splashColor',col)
print('m_ShowUnitySplashScreen',show,'m_ShowUnitySplashLogo',logo,'overlay',op,'anim',anim,'logoStyle',style,'drawMode',mode,'zooms',bz,lz,'aspects',la,pa)
print('uvs',luv,puv,'logos count',n)
for k in range(n):
    f,pi=struct.unpack_from('<iq',b,o); d=struct.unpack_from('<f',b,o+12)[0]; o+=16
    print('  logo',f,pi,'duration',d)
vr=struct.unpack_from('<iq',b,o); o+=12; hl=struct.unpack_from('<iq',b,o); o+=12
print('VRsplash',vr,'holo',hl)
print('next ints',struct.unpack_from('<4i',b,o))
