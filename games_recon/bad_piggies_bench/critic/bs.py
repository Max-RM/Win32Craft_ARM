import struct,re
p=r'D:\repo\Win32Craft-main\BadPiggiesRebooted_1.0.0.0_ARM\Data\globalgamemanagers'
b=open(p,'rb').read()
for m in re.finditer(rb'2018\.4\.36f1',b):
    print('version string at',hex(m.start()))
i=b.find(b'.unity')
print('first .unity at',hex(i), b[i-60:i+8])
# dump region after last version string occurrence (BuildSettings m_Version)
ms=[m.start() for m in re.finditer(rb'2018\.4\.36f1',b)]
for v in ms[1:]:
    print('---- around', hex(v))
    o=v-4
    ln=struct.unpack_from('<I',b,o)[0]
    print('len',ln, b[o+4:o+4+ln])
    o=o+4+ln; o=(o+3)&~3
    print([hex(x) for x in struct.unpack_from('<24I',b,o)])
o=0xad70+11; o=(o+3)&~3
ln=struct.unpack_from('<I',b,o)[0]; o+=4+ln; o=(o+3)&~3
n=struct.unpack_from('<I',b,o)[0]; o+=4; print('runtimeClassHashes',n); o+=n*20
n=struct.unpack_from('<I',b,o)[0]; o+=4; print('scriptHashes',n); o+=n*32
n=struct.unpack_from('<I',b,o)[0]; o+=4; print('graphicsAPIs count',n, 'at', hex(o-4), struct.unpack_from('<%di'%min(n,10),b,o))
