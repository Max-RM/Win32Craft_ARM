import struct, hashlib, sys, os
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

SRC = r'D:\repo\Win32Craft-main\Minecraft_1.2.8.0_ARM32\fmod_ARM.dll'
OUT = r'D:\repo\Win32Craft-main\arm32\work\fmod_verify'
os.makedirs(OUT, exist_ok=True)
orig = open(SRC, 'rb').read()


def apply(d, edits):
    d = bytearray(d)
    for off, old, new in edits:
        o_, n_ = bytes.fromhex(old), bytes.fromhex(new)
        assert d[off:off + len(o_)] == o_, (hex(off), d[off:off + len(o_)].hex())
        assert len(o_) == len(n_)
        d[off:off + len(o_)] = n_
    return bytes(d)

common = [
    (0xF989A, '56435255' '4E54494D' '45313430' '5F415050' '2E646C6C' '00',
              '56435255' '4E54494D' '45313430' '2E646C6C' '00' '00000000'),
    (0x17E, '4011', '4001'),
]
finder = common + [(0x78A50, 'bb654df21871', '002400f085b9')]
alt = common + [(0x78A4E, '0023bb65', '002486e1')]
# finder-plus-timeout variant is not built (see verdict)

for name, ed in (('fmod_ARM_finder.dll', finder), ('fmod_ARM_alt.dll', alt)):
    d = apply(orig, ed)
    open(os.path.join(OUT, name), 'wb').write(d)
    print(name, len(d), hashlib.sha256(d).hexdigest())
    diff = [i for i in range(len(d)) if d[i] != orig[i]]
    print('  differing bytes:', len(diff), hex(diff[0]), hex(diff[-1]))


# simulate ntdll base relocation (IMAGE_REL_BASED_ARM_MOV32T) with a 64K-aligned delta and
# disassemble the patched site afterwards
def relocate(d, delta):
    d = bytearray(d)
    fo, rsz = 0x10D600, 0x3AB8
    o = fo
    n7 = 0
    while o < fo + rsz:
        page, bs = struct.unpack_from('<II', d, o)
        if bs == 0:
            break
        for i in range((bs - 8) // 2):
            e = struct.unpack_from('<H', d, o + 8 + 2 * i)[0]
            t, off = e >> 12, e & 0xFFF
            rva = page + off
            # file offset (only .text/.rdata/.data matter here)
            if t == 7:
                if 0x1000 <= rva < 0xB0000:
                    fp = rva - 0x1000 + 0x400
                elif 0xB1000 <= rva < 0xFC000:
                    fp = rva - 0xB1000 + 0xAFA00
                else:
                    fp = rva - 0xFC000 + 0xF9E00
                h1, h2, h3, h4 = struct.unpack_from('<4H', d, fp)
                lo = ((h1 & 0xF) << 12) | (((h1 >> 10) & 1) << 11) | (((h2 >> 12) & 7) << 8) | (h2 & 0xFF)
                hi = ((h3 & 0xF) << 12) | (((h3 >> 10) & 1) << 11) | (((h4 >> 12) & 7) << 8) | (h4 & 0xFF)
                v = ((hi << 16) | lo) + delta & 0xFFFFFFFF
                for k, (a, b) in enumerate(((h1, h2), (h3, h4))):
                    imm = v & 0xFFFF if k == 0 else v >> 16
                    a2 = (a & 0xFBF0) | ((imm >> 12) & 0xF) | (((imm >> 11) & 1) << 10)
                    b2 = (b & 0x8F00) | (((imm >> 8) & 7) << 12) | (imm & 0xFF)
                    struct.pack_into('<2H', d, fp + 4 * k, a2, b2)
                n7 += 1
            elif t == 3:
                if 0x1000 <= rva < 0xB0000:
                    fp = rva - 0x1000 + 0x400
                elif 0xB1000 <= rva < 0xFC000:
                    fp = rva - 0xB1000 + 0xAFA00
                else:
                    fp = rva - 0xFC000 + 0xF9E00
                v = struct.unpack_from('<I', d, fp)[0]
                struct.pack_into('<I', d, fp, (v + delta) & 0xFFFFFFFF)
        o += bs
    return bytes(d), n7

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
for name in ('fmod_ARM_finder.dll', 'fmod_ARM_alt.dll'):
    d = open(os.path.join(OUT, name), 'rb').read()
    for delta in (0x00000000, 0x23450000, 0xFFFF0000 & 0xFFFFFFFF):
        r, n7 = relocate(d, delta)
        site = r[0x78A4E:0x78A4E + 14]
        print(name, hex(delta), 'mov32t relocs', n7, site.hex())
        for i in md.disasm(site, 0x7964E):
            print('   ', hex(i.address), i.mnemonic, i.op_str)
