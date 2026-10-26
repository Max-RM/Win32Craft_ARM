import struct
up = open(r'D:\repo\Win32Craft-main\BadPiggiesRebooted_1.0.0.0_ARM\UnityPlayer.dll', 'rb').read()
t = open(r'D:\repo\Win32Craft-main\games_recon\bad_piggies_shader_proto\auto\l9_replacements.bin', 'rb').read()
assert t[:4] == b'L9SW'
n = struct.unpack_from('<I', t, 8)[0]
keys = {}
for i in range(n):
    ck = t[12 + 24 * i:12 + 24 * i + 16]
    off, size = struct.unpack_from('<II', t, 12 + 24 * i + 16)
    keys[ck] = (off, size)
print('entries', n)
for fo in (0x266ba38, 0x266bc70, 0x266bec8, 0x266c1b0):
    d = up[fo:fo + 4]
    size = struct.unpack_from('<I', up, fo + 24)[0]
    ck = up[fo + 4:fo + 20]
    ver = None
    print(hex(fo), d, 'size', size, 'checksum', ck.hex(), 'in table:', ck in keys)
