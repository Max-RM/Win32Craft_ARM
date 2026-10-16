import sys, os, re, subprocess
# summarize WinRT static helper functions: class string, IID, vtable offsets
ARM=r'D:\repo\Win32Craft-main\arm32\v1_16_40\analysis\a.py'
def run(args):
    return subprocess.run([sys.executable,ARM]+args,capture_output=True,text=True).stdout
def guid(ws):
    w1,w2,w3,w4=ws
    d4=[w3&0xff,(w3>>8)&0xff,(w3>>16)&0xff,(w3>>24)&0xff,w4&0xff,(w4>>8)&0xff,(w4>>16)&0xff,(w4>>24)&0xff]
    return "%08x-%04x-%04x-%02x%02x-%s"%(w1,w2&0xffff,w2>>16,d4[0],d4[1],''.join('%02x'%b for b in d4[2:]))
for f in sys.argv[1:]:
    out=run(['func',f,'-n','300'])
    pools=[int(m.group(1),16) for m in re.finditer(r'pool\[[0-9a-f]+\]=0x([0-9a-f]{8})',out)]
    # take first 4 pool words if they look like a guid
    cls=re.findall(r'wstr L"([^"]+)"',out)
    offs=re.findall(r'ldr(?:\.w)? +\w+, \[\w+, #0x([0-9a-f]+)\]\s*\n[^\n]*blx',out)
    offs2=re.findall(r'ldr(?:\.w)? +r[34], \[r3, #0x([0-9a-f]+)\]',out)
    g=guid(pools[:4]) if len(pools)>=4 else None
    print(f, cls[:1], g, 'slots', sorted(set(int(o,16)//4 for o in offs2)))
