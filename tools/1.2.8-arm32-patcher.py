#!/usr/bin/env python3
"""Create the Win32-hosted Minecraft 1.2.8 ARM32 executable.

ARM32 (Thumb-2) equivalent of tools/1.2.8-patcher.py.  Input is
Minecraft_1.2.8.0_ARM32\\Minecraft.Windows.exe; the plan (offsets, RVAs, byte
patterns) is the verified one from arm32/facts/winmain.md and pe_imports.md:

  * DllCharacteristics 0x9140 -> 0x8140 (clear APPCONTAINER only; OS and
    subsystem version stay 6.2, DYNAMIC_BASE stays because the design is
    base independent)
  * the four *_APP.dll runtime imports are renamed in place to the desktop
    redistributables
  * a new .d3imp section (RVA 0x01730000) holds the rebuilt import directory:
    the 50 original descriptors (XINPUTUAP.dll redirected to win32craft.dll)
    plus win32craft.dll!Win32Bootstrap
  * the `bl` to the C++/CX main wrapper in __scrt_common_main_seh is redirected
    to a 16-byte position independent trampoline placed in the zero padding at
    the end of .text, which tail-jumps through the Win32Bootstrap IAT cell
  * SizeOfImage becomes 0x01731000 (IMAGE_SIZE_128_ARM_PATCHED), which is how
    win32craft.dll identifies this build

A second mode (--fmod-dll IN --fmod-out OUT) patches fmod_ARM.dll so it loads
in a desktop process (arm32/facts/fmod.md, option D).
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from dataclasses import dataclass
from pathlib import Path


# ---------------------------------------------------------------------------
# Verified inputs (arm32/facts/winmain.json, pe_imports.json, fmod.json)
# ---------------------------------------------------------------------------

EXPECTED_SHA256 = "b01d5f1237d2daad8cc076d7218f0788751b55a9576c779ccdba08eef6fcfcf8"
EXPECTED_SIZE = 23_595_520
EXPECTED_FMOD_SHA256 = "5f92f5ac9317c7a5d61ab6b43a2e799ae90eb7eee5c992c85d0325ba1d09fc38"
EXPECTED_FMOD_SIZE = 1_118_720

IMAGE_FILE_MACHINE_ARMNT = 0x01C4
IMAGE_BASE = 0x400000
EXPECTED_DESCRIPTORS = 50
EXPECTED_IMPORT_DIRECTORY = (0x13269A8, 0x3FC)

DLL_CHARACTERISTICS_ORIGINAL = 0x9140
DLL_CHARACTERISTICS_PATCHED = 0x8140       # APPCONTAINER (0x1000) cleared
IMAGE_DLLCHARACTERISTICS_APPCONTAINER = 0x1000

# New section: appended at EOF, directly after .reloc.
NEW_SECTION_NAME = b".d3imp\0\0"
NEW_SECTION_RVA = 0x01730000
NEW_SECTION_RAW_OFFSET = 0x1680A00
NEW_SECTION_FLAGS = 0xC0000040             # initialized data, read, write
IMAGE_SIZE_128_ARM_PATCHED = 0x01731000    # Win32Bootstrap keys the build on it

# Call site inside __scrt_common_main_seh (sub_b49a24): `bl sub_b4a424`, the
# C++/CX main wrapper.  r0-r3 are not read by the callee, LR must reach the
# replacement unchanged (the trampoline tail-jumps with `bx ip`).
CALL_SITE_RVA = 0x00B49ACA
CALL_SITE_FILE_OFFSET = 0xB48ECA
CALL_SITE_ORIGINAL = bytes.fromhex("00f0abfc")
CALL_SITE_PATCHED = bytes.fromhex("edf0e9f9")   # bl 0x00C36EA0

# Trampoline: zero padding at the end of .text (no reloc, no .pdata, no
# pointers reach it).  Leaf tail jump, never on a stack: needs no unwind info.
TRAMPOLINE_RVA = 0x00C36EA0
TRAMPOLINE_FILE_OFFSET = 0xC362A0
TRAMPOLINE_SLACK = 0x160                   # bytes of zero padding available
TRAMPOLINE_CODE = bytes.fromhex(
    "dff808c0"   # ldr.w ip, [pc, #8]     ; ip = literal at +12
    "fc44"       # add   ip, pc           ; pc = tramp+8 -> ip = cell address
    "dcf800c0"   # ldr.w ip, [ip]         ; ip = Win32Bootstrap (Thumb bit set by the loader's export)
    "6047"       # bx    ip               ; tail jump, LR = call site + 4 untouched
)
TRAMPOLINE_PC_BIAS = 8                     # literal = cell_rva - (TRAMPOLINE_RVA + 8)
TEXT_SECTION_OLD_VIRTUAL_SIZE = 0xC35E9C
TEXT_SECTION_NEW_VIRTUAL_SIZE = 0xC35EB0   # covers the 16-byte trampoline

# In-place renames to the desktop VC redistributables (count 1 each).
DLL_RENAMES = (
    (b"vccorlib140_app.DLL", b"vccorlib140.DLL", "desktop vccorlib"),
    (b"MSVCP140_APP.dll", b"MSVCP140.dll", "desktop MSVCP"),
    (b"CONCRT140_APP.dll", b"CONCRT140.dll", "desktop CONCRT"),
    (b"VCRUNTIME140_APP.dll", b"VCRUNTIME140.dll", "desktop VCRUNTIME"),
)

# XINPUTUAP.dll is UWP only (its 14 byte name slot cannot hold win32craft.dll,
# so the descriptor Name RVA is repointed).  The six api sets below are present
# natively on Windows 8.1/10; redirecting them is optional parity with the x86
# build (win32craft.dll then serves the fake WinRT/COM/eventing exports).
WIN32CRAFT_REQUIRED_REDIRECTS = {
    b"xinputuap.dll",
}
WIN32CRAFT_OPTIONAL_API_SETS = {
    b"api-ms-win-core-com-l1-1-0.dll",
    b"api-ms-win-core-winrt-error-l1-1-0.dll",
    b"api-ms-win-core-winrt-error-l1-1-1.dll",
    b"api-ms-win-core-winrt-l1-1-0.dll",
    b"api-ms-win-core-winrt-string-l1-1-0.dll",
    b"api-ms-win-eventing-provider-l1-1-0.dll",
}

# fmod_ARM.dll (arm32/facts/fmod.json, option D)
FMOD_DLL_CHARACTERISTICS_OFFSET = 0x17E
FMOD_VCRUNTIME_NAME_OFFSET = 0xF989A
FMOD_SKIP_WINRT_PACKAGE_OFFSET = 0x78A4E   # RVA 0x7964E in sub_79300 (System init)
# movs r3,#0 ; str r3,[r7,#0x58]   ->   movs r4,#0 ; b.n 0x79960
# 0x79960 is the common cleanup epilogue of sub_79300 (r4 = FMOD result = 0).
# Reloc-safe 4-byte form (verifier correction, arm32/facts/fmod.verdicts.json): the original 6-byte patch at
# 0x78A50 straddled the type-7 base relocation at RVA 0x79652.
FMOD_SKIP_WINRT_PACKAGE_OLD = bytes.fromhex("0023bb65")
FMOD_SKIP_WINRT_PACKAGE_NEW = bytes.fromhex("002486e1")


def align(value: int, boundary: int) -> int:
    return (value + boundary - 1) & ~(boundary - 1)


def checked_write(image: bytearray, offset: int, expected: bytes,
                  replacement: bytes, label: str) -> None:
    actual = bytes(image[offset:offset + len(expected)])
    if actual != expected:
        raise ValueError(
            f"{label}: file+0x{offset:x} contains {actual.hex()}, "
            f"expected {expected.hex()}"
        )
    if len(expected) != len(replacement):
        raise ValueError(f"{label}: replacement length mismatch")
    image[offset:offset + len(replacement)] = replacement
    print(f"{label}: file+0x{offset:x}")


def replace_all(image: bytearray, old: bytes, new: bytes,
                expected_count: int, label: str) -> None:
    positions: list[int] = []
    start = 0
    while True:
        position = image.find(old, start)
        if position < 0:
            break
        positions.append(position)
        start = position + len(old)
    if len(positions) != expected_count:
        raise ValueError(
            f"{label}: expected {expected_count} occurrences, "
            f"found {len(positions)}"
        )
    if len(new) > len(old):
        raise ValueError(f"{label}: replacement is too long")
    for position in positions:
        image[position:position + len(old)] = new.ljust(len(old), b"\0")
        print(f"{label}: file+0x{position:x}")


def encode_thumb2_bl(site_rva: int, target_rva: int) -> bytes:
    """Thumb-2 BL (encoding T1) at site_rva branching to target_rva."""
    displacement = target_rva - (site_rva + 4)
    if displacement & 1 or not -(1 << 24) <= displacement < (1 << 24):
        raise ValueError(
            f"BL displacement {displacement} is odd or outside +-16 MiB"
        )
    value = displacement & 0x1FFFFFF
    sign = (value >> 24) & 1
    i1 = (value >> 23) & 1
    i2 = (value >> 22) & 1
    imm10 = (value >> 12) & 0x3FF
    imm11 = (value >> 1) & 0x7FF
    j1 = (~(i1 ^ sign)) & 1
    j2 = (~(i2 ^ sign)) & 1
    first = 0xF000 | (sign << 10) | imm10
    second = 0xD000 | (j1 << 13) | (j2 << 11) | imm11
    return struct.pack("<2H", first, second)


def decode_thumb2_bl(code: bytes, site_rva: int) -> int:
    """Return the RVA a Thumb-2 BL (T1) at site_rva branches to."""
    first, second = struct.unpack("<2H", code[:4])
    if first & 0xF800 != 0xF000 or second & 0xD000 != 0xD000:
        raise ValueError(f"not a Thumb-2 BL: {code[:4].hex()}")
    sign = (first >> 10) & 1
    j1 = (second >> 13) & 1
    j2 = (second >> 11) & 1
    i1 = (~(j1 ^ sign)) & 1
    i2 = (~(j2 ^ sign)) & 1
    value = (sign << 24) | (i1 << 23) | (i2 << 22) | \
        ((first & 0x3FF) << 12) | ((second & 0x7FF) << 1)
    if sign:
        value -= 1 << 25
    return site_rva + 4 + value


def build_trampoline(trampoline_rva: int, cell_rva: int) -> bytes:
    """16 bytes: PIC code + a displacement literal (valid at any image base)."""
    literal = (cell_rva - (trampoline_rva + TRAMPOLINE_PC_BIAS)) & 0xFFFFFFFF
    return TRAMPOLINE_CODE + struct.pack("<I", literal)


@dataclass
class Section:
    name: bytes
    virtual_address: int
    virtual_size: int
    raw_offset: int
    raw_size: int


class Pe32:
    def __init__(self, image: bytearray) -> None:
        self.image = image
        self.pe_offset = struct.unpack_from("<I", image, 0x3C)[0]
        if image[self.pe_offset:self.pe_offset + 4] != b"PE\0\0":
            raise ValueError("invalid PE signature")
        self.file_header = self.pe_offset + 4
        self.optional = self.file_header + 20
        if struct.unpack_from("<H", image, self.optional)[0] != 0x10B:
            raise ValueError("expected PE32")
        self.machine = struct.unpack_from("<H", image, self.file_header)[0]
        self.section_count = struct.unpack_from(
            "<H", image, self.file_header + 2
        )[0]
        self.optional_size = struct.unpack_from(
            "<H", image, self.file_header + 16
        )[0]
        self.section_table = self.optional + self.optional_size
        self.file_alignment = struct.unpack_from(
            "<I", image, self.optional + 0x24
        )[0]
        self.section_alignment = struct.unpack_from(
            "<I", image, self.optional + 0x20
        )[0]
        self.size_of_image = struct.unpack_from(
            "<I", image, self.optional + 0x38
        )[0]
        self.size_of_headers = struct.unpack_from(
            "<I", image, self.optional + 0x3C
        )[0]
        self.sections: list[Section] = []
        self.section_header_offsets: list[int] = []
        for index in range(self.section_count):
            header = self.section_table + index * 40
            name = bytes(image[header:header + 8])
            virtual_size, virtual_address, raw_size, raw_offset = \
                struct.unpack_from("<4I", image, header + 8)
            self.sections.append(
                Section(name, virtual_address, virtual_size,
                        raw_offset, raw_size)
            )
            self.section_header_offsets.append(header)

    def section_named(self, name: bytes) -> tuple[Section, int]:
        for section, header in zip(self.sections, self.section_header_offsets):
            if section.name.rstrip(b"\0") == name:
                return section, header
        raise ValueError(f"section {name!r} not found")

    def rva_to_offset(self, rva: int) -> int:
        for section in self.sections:
            extent = max(section.virtual_size, section.raw_size)
            if section.virtual_address <= rva < section.virtual_address + extent:
                return section.raw_offset + rva - section.virtual_address
        if rva < self.size_of_headers:
            return rva
        raise ValueError(f"RVA 0x{rva:x} is outside the image")

    def c_string_at_rva(self, rva: int) -> bytes:
        offset = self.rva_to_offset(rva)
        end = self.image.index(0, offset)
        return bytes(self.image[offset:end])


def read_import_descriptors(pe: Pe32, rva: int) -> list[tuple[bytes, bytes]]:
    cursor = pe.rva_to_offset(rva)
    descriptors: list[tuple[bytes, bytes]] = []
    while True:
        descriptor = bytes(pe.image[cursor:cursor + 20])
        values = struct.unpack("<5I", descriptor)
        if not any(values):
            return descriptors
        descriptors.append((pe.c_string_at_rva(values[3]).lower(), descriptor))
        cursor += 20


def refuse_if_already_patched(pe: Pe32) -> None:
    names = [section.name.rstrip(b"\0") for section in pe.sections]
    if NEW_SECTION_NAME.rstrip(b"\0") in names or \
            pe.size_of_image == IMAGE_SIZE_128_ARM_PATCHED:
        raise ValueError(
            "input is already patched (.d3imp section / patched SizeOfImage "
            "present); use the original Minecraft.Windows.exe"
        )


def refuse_same_file(source: Path, output: Path) -> None:
    if output.exists() and source.resolve() == output.resolve():
        raise ValueError("output must not overwrite the input file")


# ---------------------------------------------------------------------------
# Minecraft.Windows.exe
# ---------------------------------------------------------------------------

def patch(source: Path, output: Path, redirect_api_sets: bool) -> None:
    refuse_same_file(source, output)
    original = source.read_bytes()
    try:
        pe_probe = Pe32(bytearray(original))
    except (ValueError, struct.error) as error:
        raise ValueError(f"{source} is not a usable PE32 file: {error}")
    refuse_if_already_patched(pe_probe)
    digest = hashlib.sha256(original).hexdigest()
    if digest != EXPECTED_SHA256 or len(original) != EXPECTED_SIZE:
        raise ValueError(
            f"unsupported input SHA-256 {digest} ({len(original)} bytes); "
            f"expected {EXPECTED_SHA256} ({EXPECTED_SIZE} bytes)"
        )
    image = bytearray(original)
    pe = Pe32(image)
    if pe.machine != IMAGE_FILE_MACHINE_ARMNT:
        raise ValueError(f"machine 0x{pe.machine:x}, expected ARMNT 0x1c4")
    if (pe.file_alignment, pe.section_alignment) != (0x200, 0x1000):
        raise ValueError("unexpected file/section alignment")

    # --- header fields ------------------------------------------------------
    # Characteristics is already 0x0122 (large address aware); OS and
    # subsystem stay 6.2 (the x86 minor 2->1 writes are NOT ported).
    if struct.unpack_from("<H", image, pe.file_header + 18)[0] != 0x0122:
        raise ValueError("unexpected Characteristics (expected 0x0122)")
    if struct.unpack_from("<2H", image, pe.optional + 0x28) != (6, 2):
        raise ValueError("unexpected OS version (expected 6.2)")
    if struct.unpack_from("<2H", image, pe.optional + 0x30) != (6, 2):
        raise ValueError("unexpected subsystem version (expected 6.2)")
    checked_write(
        image, pe.optional + 0x46, struct.pack("<H", DLL_CHARACTERISTICS_ORIGINAL),
        struct.pack("<H", DLL_CHARACTERISTICS_PATCHED),
        "desktop DLL characteristics"
    )

    # --- in-place DLL renames (each name occurs exactly once) --------------
    for old, new, label in DLL_RENAMES:
        replace_all(image, old, new, 1, label)

    # --- import directory ---------------------------------------------------
    import_rva, import_size = struct.unpack_from("<2I", image, pe.optional + 0x68)
    if (import_rva, import_size) != EXPECTED_IMPORT_DIRECTORY:
        raise ValueError(
            f"unexpected import directory 0x{import_rva:x}/0x{import_size:x}"
        )
    descriptors = read_import_descriptors(pe, import_rva)
    if len(descriptors) != EXPECTED_DESCRIPTORS:
        raise ValueError(
            f"unexpected import table: {len(descriptors)} descriptors, "
            f"size 0x{import_size:x}"
        )
    present = {name for name, _ in descriptors}
    for name in WIN32CRAFT_REQUIRED_REDIRECTS:
        if name not in present:
            raise ValueError(f"required import {name!r} not found")
    redirects = set(WIN32CRAFT_REQUIRED_REDIRECTS)
    if redirect_api_sets:
        missing = WIN32CRAFT_OPTIONAL_API_SETS - present
        if missing:
            raise ValueError(f"api sets not imported: {sorted(missing)}")
        redirects |= WIN32CRAFT_OPTIONAL_API_SETS

    last = pe.sections[-1]
    new_raw_offset = align(len(image), pe.file_alignment)
    new_rva = align(
        last.virtual_address + max(last.virtual_size, last.raw_size),
        pe.section_alignment,
    )
    if new_raw_offset != NEW_SECTION_RAW_OFFSET or new_rva != NEW_SECTION_RVA:
        raise ValueError(
            f"new section would be at RVA 0x{new_rva:x} / file "
            f"0x{new_raw_offset:x}; the verified plan is 0x{NEW_SECTION_RVA:x}"
            f" / 0x{NEW_SECTION_RAW_OFFSET:x}"
        )
    new_descriptor_count = len(descriptors) + 2     # win32craft + terminator
    section = bytearray(new_descriptor_count * 20)

    def append(data: bytes, alignment: int = 1) -> int:
        section.extend(b"\0" * ((-len(section)) % alignment))
        offset = len(section)
        section.extend(data)
        return offset

    dll_name_offset = append(b"win32craft.dll\0")
    hint_offset = append(b"\0\0Win32Bootstrap\0", 2)
    ilt_offset = append(struct.pack("<2I", new_rva + hint_offset, 0), 4)
    bootstrap_iat_offset = append(
        struct.pack("<2I", new_rva + hint_offset, 0), 4
    )

    relocated: list[bytes] = []
    for dll_name, descriptor in descriptors:
        values = list(struct.unpack("<5I", descriptor))
        if dll_name in redirects:
            values[3] = new_rva + dll_name_offset
            print(f"redirect {dll_name.decode('ascii')} -> win32craft.dll")
        relocated.append(struct.pack("<5I", *values))
    relocated.append(struct.pack(
        "<5I", new_rva + ilt_offset, 0, 0,
        new_rva + dll_name_offset, new_rva + bootstrap_iat_offset
    ))
    relocated.append(b"\0" * 20)
    section[:new_descriptor_count * 20] = b"".join(relocated)

    virtual_size = len(section)
    raw_size = align(virtual_size, pe.file_alignment)
    section.extend(b"\0" * (raw_size - virtual_size))
    image.extend(b"\0" * (new_raw_offset - len(image)))
    image.extend(section)

    new_header = pe.section_table + pe.section_count * 40
    if new_header + 40 > pe.size_of_headers:
        raise ValueError("no room for an additional section header")
    checked_write(
        image, new_header, b"\0" * 40,
        struct.pack(
            "<8s6I2HI", NEW_SECTION_NAME, virtual_size, new_rva, raw_size,
            new_raw_offset, 0, 0, 0, 0, NEW_SECTION_FLAGS
        ), "new .d3imp section header"
    )
    checked_write(
        image, pe.file_header + 2, struct.pack("<H", pe.section_count),
        struct.pack("<H", pe.section_count + 1), "section count"
    )
    old_initialized = struct.unpack_from("<I", image, pe.optional + 8)[0]
    checked_write(
        image, pe.optional + 8, struct.pack("<I", old_initialized),
        struct.pack("<I", old_initialized + raw_size), "initialized data size"
    )
    new_image_size = align(new_rva + virtual_size, pe.section_alignment)
    if new_image_size != IMAGE_SIZE_128_ARM_PATCHED:
        raise ValueError(
            f"SizeOfImage would be 0x{new_image_size:x}, the host expects "
            f"0x{IMAGE_SIZE_128_ARM_PATCHED:x} (.d3imp grew past 4 KiB?)"
        )
    checked_write(
        image, pe.optional + 0x38, struct.pack("<I", pe.size_of_image),
        struct.pack("<I", new_image_size), "image size"
    )
    checked_write(
        image, pe.optional + 0x68, struct.pack("<2I", import_rva, import_size),
        struct.pack("<2I", new_rva, new_descriptor_count * 20),
        "relocated import directory"
    )

    # --- Win32Bootstrap call: bl -> trampoline -> IAT cell ------------------
    bootstrap_cell_rva = new_rva + bootstrap_iat_offset
    text, text_header = pe.section_named(b".text")
    if text.virtual_size != TEXT_SECTION_OLD_VIRTUAL_SIZE:
        raise ValueError(f".text VirtualSize 0x{text.virtual_size:x} unexpected")
    if pe.rva_to_offset(TRAMPOLINE_RVA) != TRAMPOLINE_FILE_OFFSET or \
            pe.rva_to_offset(CALL_SITE_RVA) != CALL_SITE_FILE_OFFSET:
        raise ValueError("call site / trampoline RVA -> file offset mismatch")
    trampoline = build_trampoline(TRAMPOLINE_RVA, bootstrap_cell_rva)
    call_site_new = encode_thumb2_bl(CALL_SITE_RVA, TRAMPOLINE_RVA)
    if call_site_new != CALL_SITE_PATCHED:
        raise ValueError(
            f"computed call site {call_site_new.hex()} differs from the "
            f"verified {CALL_SITE_PATCHED.hex()}"
        )
    if decode_thumb2_bl(CALL_SITE_ORIGINAL, CALL_SITE_RVA) != 0x00B4A424:
        raise ValueError("original call site does not target sub_b4a424")
    checked_write(
        image, TRAMPOLINE_FILE_OFFSET, b"\0" * TRAMPOLINE_SLACK,
        trampoline + b"\0" * (TRAMPOLINE_SLACK - len(trampoline)),
        "Win32Bootstrap trampoline"
    )
    checked_write(
        image, CALL_SITE_FILE_OFFSET, CALL_SITE_ORIGINAL, call_site_new,
        "WinMain bootstrap call"
    )
    # Declare the trampoline inside .text (the loader maps it either way, but
    # disassemblers only show what the section header covers).
    checked_write(
        image, text_header + 8, struct.pack("<I", text.virtual_size),
        struct.pack("<I", TEXT_SECTION_NEW_VIRTUAL_SIZE),
        ".text virtual size"
    )

    verify_patched_exe(image, redirect_api_sets)
    output.write_bytes(image)
    print(f"wrote {output} ({len(image)} bytes)")
    print(f"SHA-256 {hashlib.sha256(image).hexdigest()}")


def verify_patched_exe(image: bytearray, redirect_api_sets: bool) -> None:
    """Re-parse the result and check the invariants the host depends on."""
    pe = Pe32(image)
    optional = pe.optional
    if struct.unpack_from("<H", image, optional + 0x46)[0] != \
            DLL_CHARACTERISTICS_PATCHED:
        raise ValueError("verify: DllCharacteristics")
    if pe.size_of_image != IMAGE_SIZE_128_ARM_PATCHED:
        raise ValueError("verify: SizeOfImage")
    import_rva, import_size = struct.unpack_from("<2I", image, optional + 0x68)
    descriptors = read_import_descriptors(pe, import_rva)
    if len(descriptors) != EXPECTED_DESCRIPTORS + 1 or \
            import_size != (EXPECTED_DESCRIPTORS + 2) * 20:
        raise ValueError("verify: import directory size")
    names = [name for name, _ in descriptors]
    if names[-1] != b"win32craft.dll" or names.count(b"win32craft.dll") != \
            1 + len(WIN32CRAFT_REQUIRED_REDIRECTS) + \
            (len(WIN32CRAFT_OPTIONAL_API_SETS) if redirect_api_sets else 0):
        raise ValueError("verify: win32craft.dll descriptors")
    for _, descriptor in descriptors:
        ilt, _, _, name_rva, iat = struct.unpack("<5I", descriptor)
        pe.rva_to_offset(name_rva)
        pe.rva_to_offset(ilt or iat)
    ilt, _, _, _, cell_rva = struct.unpack("<5I", descriptors[-1][1])
    hint_rva = struct.unpack_from("<I", image, pe.rva_to_offset(ilt))[0]
    if pe.c_string_at_rva(hint_rva + 2) != b"Win32Bootstrap" or \
            struct.unpack_from("<I", image, pe.rva_to_offset(cell_rva))[0] != hint_rva:
        raise ValueError("verify: Win32Bootstrap ILT/IAT")
    tramp = bytes(image[TRAMPOLINE_FILE_OFFSET:TRAMPOLINE_FILE_OFFSET + 16])
    literal = struct.unpack_from("<i", tramp, 12)[0]
    if TRAMPOLINE_RVA + TRAMPOLINE_PC_BIAS + literal != cell_rva or \
            tramp[:12] != TRAMPOLINE_CODE:
        raise ValueError("verify: trampoline literal")
    target = decode_thumb2_bl(
        bytes(image[CALL_SITE_FILE_OFFSET:CALL_SITE_FILE_OFFSET + 4]),
        CALL_SITE_RVA,
    )
    if target != TRAMPOLINE_RVA:
        raise ValueError("verify: call site target")
    text, _ = pe.section_named(b".text")
    if not (text.virtual_address <= TRAMPOLINE_RVA and
            TRAMPOLINE_RVA + 16 <= text.virtual_address + text.virtual_size):
        raise ValueError("verify: trampoline outside .text")
    print("self-check: import table, trampoline and call site are consistent")


# ---------------------------------------------------------------------------
# fmod_ARM.dll
# ---------------------------------------------------------------------------

def patch_fmod(source: Path, output: Path, keep_winrt_init: bool) -> None:
    """Make fmod_ARM.dll loadable in a desktop (non-AppContainer) process.

    1. VCRUNTIME140_APP.dll (VCLibs only) -> VCRUNTIME140.dll
    2. clear APPCONTAINER in DllCharacteristics (0x1140 -> 0x0140)
    3. skip sub_79300's Windows.ApplicationModel.Package / ApplicationData
       steps (they fail without package identity and would make System::init
       return FMOD_ERR_INTERNAL); the four path globals stay NULL, the game
       installs its own file system callbacks.  --fmod-keep-winrt-init leaves
       this code alone (needed if the host hooks the DLL's
       RoGetActivationFactory IAT cell at RVA 0xB118C instead, option B').

    HOOK (later WinRT-import retarget): if fmod's api-ms-win-core-winrt-l1-1-0
    descriptor (name field at file 0xF929C, 34 bytes) ever has to point at
    win32craft.dll, or api-set names have to move to KERNEL32.dll for Windows
    RT 8.1 (fmod.json port_instructions), add the checked_write calls in
    `fmod_winrt_import_retarget` below; none is applied by default.
    """
    refuse_same_file(source, output)
    original = source.read_bytes()
    if b"VCRUNTIME140_APP.dll" not in original and \
            b"VCRUNTIME140.dll\0\0\0\0\0" in original:
        raise ValueError("fmod_ARM.dll looks already patched")
    digest = hashlib.sha256(original).hexdigest()
    if digest != EXPECTED_FMOD_SHA256 or len(original) != EXPECTED_FMOD_SIZE:
        raise ValueError(
            f"unsupported fmod input SHA-256 {digest} ({len(original)} "
            f"bytes); expected {EXPECTED_FMOD_SHA256} ({EXPECTED_FMOD_SIZE})"
        )
    image = bytearray(original)
    pe = Pe32(image)
    if pe.machine != IMAGE_FILE_MACHINE_ARMNT:
        raise ValueError(f"machine 0x{pe.machine:x}, expected ARMNT 0x1c4")
    if pe.optional + 0x46 != FMOD_DLL_CHARACTERISTICS_OFFSET:
        raise ValueError("fmod DllCharacteristics offset mismatch")

    replace_all(image, b"VCRUNTIME140_APP.dll", b"VCRUNTIME140.dll", 1,
                "fmod desktop VCRUNTIME")
    if image.find(b"VCRUNTIME140.dll\0\0\0\0\0") != FMOD_VCRUNTIME_NAME_OFFSET:
        raise ValueError("fmod VCRUNTIME name is not at the verified offset")
    characteristics = struct.unpack_from(
        "<H", image, FMOD_DLL_CHARACTERISTICS_OFFSET
    )[0]
    if not characteristics & IMAGE_DLLCHARACTERISTICS_APPCONTAINER:
        raise ValueError("fmod APPCONTAINER flag is not set")
    checked_write(
        image, FMOD_DLL_CHARACTERISTICS_OFFSET,
        struct.pack("<H", characteristics),
        struct.pack("<H", characteristics & ~IMAGE_DLLCHARACTERISTICS_APPCONTAINER),
        "fmod desktop DLL characteristics"
    )
    if keep_winrt_init:
        print("fmod: keeping the WinRT Package/ApplicationData init code")
    else:
        checked_write(
            image, FMOD_SKIP_WINRT_PACKAGE_OFFSET,
            FMOD_SKIP_WINRT_PACKAGE_OLD, FMOD_SKIP_WINRT_PACKAGE_NEW,
            "fmod skip WinRT package/appdata init"
        )
    fmod_winrt_import_retarget(image, pe)
    output.write_bytes(image)
    print(f"wrote {output} ({len(image)} bytes)")
    print(f"SHA-256 {hashlib.sha256(image).hexdigest()}")


def fmod_winrt_import_retarget(image: bytearray, pe: Pe32) -> None:
    """Placeholder for a later WinRT-import retarget of fmod_ARM.dll.

    Intentionally empty: the recommended plan (fmod.json option D) needs no
    import retarget.  Candidates, all in-place name rewrites (NUL padded):
      * file 0xF929C  api-ms-win-core-winrt-l1-1-0.dll -> win32craft.dll
        (option B: win32craft.dll must export RoGetActivationFactory,
        RoInitialize, RoUninitialize)
      * file 0xF9710 / 0xF965E / 0xF978E / 0xF9764 / 0xF9C92  api sets missing
        on Windows RT 8.1 -> KERNEL32.dll
    """


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Patch the Minecraft 1.2.8 ARM32 executable (and "
                    "optionally fmod_ARM.dll) for the Win32 host."
    )
    parser.add_argument("source", nargs="?", type=Path, default=None,
                        help="original Minecraft.Windows.exe "
                             "(default: Minecraft.Windows.exe unless only "
                             "--fmod-dll is requested)")
    parser.add_argument("output", nargs="?", type=Path,
                        default=Path("Win32Craft.1.2.8-arm32.exe"))
    parser.add_argument("--redirect-api-sets", action="store_true",
                        help="also redirect the six optional api-set DLLs "
                             "(com, winrt, winrt-string, winrt-error x2, "
                             "eventing) to win32craft.dll (default: only "
                             "XINPUTUAP.dll)")
    parser.add_argument("--fmod-dll", type=Path, metavar="IN",
                        help="patch this fmod_ARM.dll for desktop use")
    parser.add_argument("--fmod-out", type=Path, metavar="OUT",
                        help="output path for the patched fmod_ARM.dll")
    parser.add_argument("--fmod-keep-winrt-init", action="store_true",
                        help="do not skip the WinRT Package/ApplicationData "
                             "init in fmod_ARM.dll (option B' hook)")
    args = parser.parse_args()
    if bool(args.fmod_dll) != bool(args.fmod_out):
        parser.error("--fmod-dll and --fmod-out must be given together")
    if args.fmod_dll:
        patch_fmod(args.fmod_dll, args.fmod_out, args.fmod_keep_winrt_init)
        if args.source is None:
            return
    patch(args.source or Path("Minecraft.Windows.exe"), args.output,
          args.redirect_api_sets)


if __name__ == "__main__":
    main()
