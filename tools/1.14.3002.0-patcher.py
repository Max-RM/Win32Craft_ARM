#!/usr/bin/env python3
"""Create the Win32-hosted Minecraft 1.14.30.2 executable.

The executable-side patch sites are version-specific.  The matching Win32
host profile lives in ``win32_host_116.inl`` (``game_profile_114_3002``,
selected by ``SizeOfImage == 0x02857000``); the patched exe grows to that
size.

Use ``--package`` to assemble a ready-to-run folder from an extracted game
folder or an ``.appx``: it copies/extracts the game, patches the exe to
``Win32Craft.1.14.30.2.exe``, copies the host DLL as ``win32craft.dll``,
creates ``fmod.dll`` from ``fmod_X86.dll``, removes ``*.p7x`` files and writes
the ``.Win32Craft`` marker (``Minecraft Bedrock Edition``).
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path

from mcbe_package import prepare_mcbe_package


PACKAGE_EXE_NAME = "Win32Craft.1.14.30.2.exe"

SUPPORTED_SHA256 = {
    "c306ba96a7ffc774ebd58f065db0f076fbc2aca6152a110bca0b3b24b6abc9f3":
        "retail x86 (Microsoft.MinecraftUWP_1.14.3002.0_x86)",
}

WIN32CRAFT_API_SETS = {
    b"xinputuap.dll",
    b"api-ms-win-core-com-l1-1-0.dll",
    b"api-ms-win-core-memory-l1-1-3.dll",
    b"api-ms-win-core-winrt-error-l1-1-0.dll",
    b"api-ms-win-core-winrt-error-l1-1-1.dll",
    b"api-ms-win-core-winrt-l1-1-0.dll",
    b"api-ms-win-core-winrt-string-l1-1-0.dll",
    b"api-ms-win-eventing-provider-l1-1-0.dll",
}

RENOIR_WINDOWS7_API_SETS = {
    b"api-ms-win-core-synch-l1-2-0.dll",
    b"api-ms-win-core-processthreads-l1-1-2.dll",
    b"api-ms-win-core-profile-l1-1-0.dll",
    b"api-ms-win-core-sysinfo-l1-2-1.dll",
    b"api-ms-win-core-libraryloader-l1-2-0.dll",
    b"api-ms-win-core-interlocked-l1-2-0.dll",
}


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


@dataclass
class Section:
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
        self.sections: list[Section] = []
        for index in range(self.section_count):
            header = self.section_table + index * 40
            virtual_size, virtual_address, raw_size, raw_offset = \
                struct.unpack_from("<4I", image, header + 8)
            self.sections.append(
                Section(virtual_address, virtual_size, raw_offset, raw_size)
            )

    def rva_to_offset(self, rva: int) -> int:
        for section in self.sections:
            extent = max(section.virtual_size, section.raw_size)
            if section.virtual_address <= rva < section.virtual_address + extent:
                return section.raw_offset + rva - section.virtual_address
        headers = struct.unpack_from("<I", self.image, self.optional + 0x3C)[0]
        if rva < headers:
            return rva
        raise ValueError(f"RVA 0x{rva:x} is outside the image")

    def c_string_at_rva(self, rva: int) -> bytes:
        offset = self.rva_to_offset(rva)
        end = self.image.index(0, offset)
        return bytes(self.image[offset:end])


def patch(source: Path, output: Path,
          host_dll: str = "win32craft.dll") -> None:
    original = source.read_bytes()
    digest = hashlib.sha256(original).hexdigest()
    if digest not in SUPPORTED_SHA256:
        supported = "\n  ".join(
            f"{value}  ({label})" for value, label in SUPPORTED_SHA256.items()
        )
        raise ValueError(
            f"unsupported input SHA-256 {digest}; supported inputs:\n  "
            f"{supported}"
        )
    print(f"input SHA-256 {digest} ({SUPPORTED_SHA256[digest]})")
    image = bytearray(original)
    pe = Pe32(image)

    # The 1.14.30.2 image already targets desktop Windows: these fields are
    # written here only to verify the expected stock values before the
    # version-specific patch sites below.
    checked_write(
        image, pe.file_header + 18, struct.pack("<H", 0x0122),
        struct.pack("<H", 0x0122), "large-address-aware"
    )
    checked_write(image, pe.optional + 0x2A, b"\x02\x00", b"\x02\x00",
                  "OS minor")
    checked_write(
        image, pe.optional + 0x32, b"\x02\x00", b"\x02\x00",
        "subsystem minor"
    )
    # Clear DYNAMIC_BASE and APPCONTAINER: the bootstrap site uses an
    # absolute VA for the host import, which is only valid at the preferred
    # image base, and an unpackaged process must not claim package identity.
    checked_write(
        image, pe.optional + 0x46, struct.pack("<H", 0x9140),
        struct.pack("<H", 0x8100), "desktop DLL characteristics"
    )

    replace_all(image, b"vccorlib140_app.DLL", b"vccorlib140.DLL", 1,
                "desktop vccorlib")
    replace_all(image, b"MSVCP140_APP.dll", b"MSVCP140.dll", 1,
                "desktop MSVCP")
    replace_all(image, b"CONCRT140_APP.dll", b"CONCRT140.dll", 1,
                "desktop CONCRT")
    replace_all(image, b"VCRUNTIME140_APP.dll", b"VCRUNTIME140.dll", 1,
                "desktop VCRUNTIME")
    replace_all(image, b"fmod_X86.dll", b"fmod.dll", 2, "desktop FMOD name")

    import_rva, import_size = struct.unpack_from("<2I", image, pe.optional + 0x68)
    cursor = pe.rva_to_offset(import_rva)
    descriptors: list[tuple[bytes, bytes]] = []
    kernel_descriptor: tuple[int, int] | None = None
    chakra_descriptor: tuple[int, int] | None = None
    while True:
        descriptor = bytes(image[cursor:cursor + 20])
        values = struct.unpack("<5I", descriptor)
        if not any(values):
            break
        name = pe.c_string_at_rva(values[3]).lower()
        descriptors.append((name, descriptor))
        if name == b"kernel32.dll":
            kernel_descriptor = (values[0] or values[4], values[4])
        elif name == b"chakra.dll":
            chakra_descriptor = (values[0] or values[4], values[4])
        cursor += 20
    if len(descriptors) != 31 or import_size != 0x280:
        raise ValueError(
            f"unexpected import table: {len(descriptors)} descriptors, "
            f"size 0x{import_size:x}"
        )
    if kernel_descriptor is None:
        raise ValueError("KERNEL32 import descriptor not found")
    if chakra_descriptor is None:
        raise ValueError("chakra.dll import descriptor not found")

    kernel_ilt_rva, kernel_iat_rva = kernel_descriptor
    kernel_ilt_offset = pe.rva_to_offset(kernel_ilt_rva)
    kernel_names: dict[str, tuple[int, int]] = {}
    index = 0
    while True:
        hint_rva = struct.unpack_from("<I", image, kernel_ilt_offset + index * 4)[0]
        if not hint_rva:
            break
        if hint_rva & 0x80000000:
            raise ValueError("unexpected ordinal KERNEL32 import")
        kernel_names[pe.c_string_at_rva(hint_rva + 2).decode("ascii")] = \
            (index, hint_rva)
        index += 1

    safe_hint_rva = kernel_names["GetLastError"][1]
    compatibility_names = (
        "ResolveDelayLoadedAPI", "ResolveDelayLoadsFromDll",
        "DelayLoadFailureHook",
    )
    compatibility_iats: dict[str, int] = {}
    for name in compatibility_names:
        entry_index, original_hint_rva = kernel_names[name]
        checked_write(
            image, kernel_ilt_offset + entry_index * 4,
            struct.pack("<I", original_hint_rva), struct.pack("<I", safe_hint_rva),
            f"{name} loader placeholder"
        )
        compatibility_iats[name] = kernel_iat_rva + entry_index * 4

    chakra_ilt_rva, chakra_iat_rva = chakra_descriptor
    chakra_ilt_offset = pe.rva_to_offset(chakra_ilt_rva)
    chakra_names: dict[str, tuple[int, int]] = {}
    index = 0
    while True:
        hint_rva = struct.unpack_from(
            "<I", image, chakra_ilt_offset + index * 4
        )[0]
        if not hint_rva:
            break
        if hint_rva & 0x80000000:
            raise ValueError("unexpected ordinal chakra.dll import")
        chakra_names[pe.c_string_at_rva(hint_rva + 2).decode("ascii")] = \
            (index, hint_rva)
        index += 1
    debug_index, debug_hint_rva = chakra_names["JsStartDebugging"]
    chakra_safe_hint_rva = chakra_names["JsAddRef"][1]
    checked_write(
        image, chakra_ilt_offset + debug_index * 4,
        struct.pack("<I", debug_hint_rva),
        struct.pack("<I", chakra_safe_hint_rva),
        "JsStartDebugging loader placeholder"
    )
    debug_iat_rva = chakra_iat_rva + debug_index * 4

    last = pe.sections[-1]
    new_raw_offset = align(len(image), pe.file_alignment)
    new_rva = align(
        last.virtual_address + max(last.virtual_size, last.raw_size),
        pe.section_alignment,
    )
    new_descriptor_count = len(descriptors) + len(compatibility_names) + 3
    section = bytearray(new_descriptor_count * 20)

    def append(data: bytes, alignment: int = 1) -> int:
        section.extend(b"\0" * ((-len(section)) % alignment))
        offset = len(section)
        section.extend(data)
        return offset

    host_dll_bytes = host_dll.encode("ascii")
    if not host_dll_bytes or b"\0" in host_dll_bytes:
        raise ValueError("host DLL name must be a non-empty ASCII string")
    dll_name_offset = append(host_dll_bytes + b"\0")
    chakra_name_offset = append(b"ChakraCore.dll\0")
    hint_offsets: dict[str, int] = {}
    host_names = ("Win32Bootstrap", "JsStartDebugging") + compatibility_names
    for name in host_names:
        hint_offsets[name] = append(b"\0\0" + name.encode("ascii") + b"\0", 2)
    ilt_offsets: dict[str, int] = {}
    for name in host_names:
        ilt_offsets[name] = append(
            struct.pack("<2I", new_rva + hint_offsets[name], 0), 4
        )
    bootstrap_iat_offset = append(
        struct.pack("<2I", new_rva + hint_offsets["Win32Bootstrap"], 0), 4
    )

    relocated: list[bytes] = []
    for dll_name, descriptor in descriptors:
        values = list(struct.unpack("<5I", descriptor))
        if dll_name in (b"dxgi.dll", b"d3d11.dll") or \
                dll_name in WIN32CRAFT_API_SETS:
            values[3] = new_rva + dll_name_offset
        elif dll_name == b"chakra.dll":
            values[3] = new_rva + chakra_name_offset
        relocated.append(struct.pack("<5I", *values))
    relocated.append(struct.pack(
        "<5I", new_rva + ilt_offsets["Win32Bootstrap"], 0, 0,
        new_rva + dll_name_offset, new_rva + bootstrap_iat_offset
    ))
    relocated.append(struct.pack(
        "<5I", new_rva + ilt_offsets["JsStartDebugging"], 0, 0,
        new_rva + dll_name_offset, debug_iat_rva
    ))
    for name in compatibility_names:
        relocated.append(struct.pack(
            "<5I", new_rva + ilt_offsets[name], 0, 0,
            new_rva + dll_name_offset, compatibility_iats[name]
        ))
    relocated.append(b"\0" * 20)
    section[:new_descriptor_count * 20] = b"".join(relocated)

    # The Minecraft client script-engine initializer runs again for every
    # world.  It creates four Script*EventListener objects, stores them in
    # engine fields +0x230/+0x234/+0x23c/+0x240 and deletes the previous
    # objects without unregistering them from their event-manager vectors:
    #
    #   +0x230 ScriptClientPacketEventListener -> [[ClientInstance]+0x1b8]
    #   +0x234 ScriptTelemetryEventListener    -> [engine+0xb0]
    #   +0x23c ScriptClientLevelEventListener  -> manager returned by vtbl+0x70
    #   +0x240 ScriptHitDetectEventListener    -> [[ClientInstance]+0x62c]
    #
    # After a world exit and re-entry every vector holds [stale, current], and
    # the next dispatch dereferences the recycled stale block as a vtable
    # (observed as an AV in the packet-dispatcher thunk).  Replace each
    # deleting-destructor call with a stub that unlinks the old pointer from
    # its manager vector first.  The shared helper and the stubs live in this
    # appended section, which is marked executable below.
    code = bytearray()
    code_relocations: list[tuple[int, int]] = []

    def code_call(label: int) -> None:
        code_relocations.append((len(code) + 1, label))
        code.extend(b"\xe8\0\0\0\0")

    helper_offset = len(code)
    code.extend(bytes.fromhex(
        # pushad; ebx=vector; ecx=begin; edi=end; empty -> delete
        "608bde8b0b8b7b043bcf740b3911740983c1043bcf72f5eb16"
        # found: shift every later entry left by one pointer
        "8bc18d68043bef73098b750089308bc5ebf0"
        # end -= 4
        "836b0404"
        # deleting destructor through vtable slot 0
        "8b028bca6a01ff1061c3"
    ))
    stub_packet_offset = len(code)
    code.extend(bytes.fromhex(
        "528b8b480200008b01ff90dc0300008bb0b80100005a"
    ))
    code_call(helper_offset)
    code.extend(bytes.fromhex("8db330020000c3"))
    stub_telemetry_offset = len(code)
    code.extend(bytes.fromhex("528bb3b00000005a"))
    code_call(helper_offset)
    code.extend(bytes.fromhex("8db334020000c3"))
    stub_level_offset = len(code)
    code_call(helper_offset)
    code.extend(b"\xc3")
    stub_hitdetect_offset = len(code)
    code.extend(bytes.fromhex("8bd18b83480200008bb02c060000"))
    code_call(helper_offset)
    code.extend(b"\xc3")
    cave_offset = append(bytes(code), 4)
    cave_va = 0x400000 + new_rva + cave_offset
    for position, label in code_relocations:
        struct.pack_into(
            "<i", section, cave_offset + position,
            (cave_va + label) - (cave_va + position + 4),
        )

    def listener_site(rva: int, expected: str, stub_offset: int,
                      label: str) -> None:
        original = bytes.fromhex(expected)
        site_va = 0x400000 + rva
        replacement = b"\xe8" + struct.pack(
            "<i", cave_va + stub_offset - (site_va + 5)
        ) + b"\x90" * (len(original) - 5)
        checked_write(
            image, pe.rva_to_offset(rva), original, replacement, label
        )

    listener_site(
        0x94D956, "8b028bca6a01ff10", stub_packet_offset,
        "unregister old packet listener on script re-init"
    )
    listener_site(
        0x94D9F6, "8bcA8b026a01ff10", stub_telemetry_offset,
        "unregister old telemetry listener on script re-init"
    )
    listener_site(
        0x94DA80, "8b028bca6a01ff10", stub_level_offset,
        "unregister old level listener on script re-init"
    )
    listener_site(
        0x94DAF2, "8b016a01ff10", stub_hitdetect_offset,
        "unregister old hit-detect listener on script re-init"
    )

    virtual_size = len(section)
    raw_size = align(virtual_size, pe.file_alignment)
    section.extend(b"\0" * (raw_size - virtual_size))
    image.extend(b"\0" * (new_raw_offset - len(image)))
    image.extend(section)

    new_header = pe.section_table + pe.section_count * 40
    size_of_headers = struct.unpack_from("<I", image, pe.optional + 0x3C)[0]
    if new_header + 40 > size_of_headers:
        raise ValueError("no room for an additional section header")
    checked_write(
        image, new_header, b"\0" * 40,
        struct.pack(
            "<8s6I2HI", b".d3imp\0\0", virtual_size, new_rva, raw_size,
            new_raw_offset, 0, 0, 0, 0, 0xE0000040
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
    old_image_size = struct.unpack_from("<I", image, pe.optional + 0x38)[0]
    new_image_size = align(new_rva + virtual_size, pe.section_alignment)
    checked_write(
        image, pe.optional + 0x38, struct.pack("<I", old_image_size),
        struct.pack("<I", new_image_size), "image size"
    )
    if new_image_size != 0x02857000:
        raise ValueError(
            f"patched SizeOfImage is 0x{new_image_size:08X}, expected "
            "0x02857000 for the 1.14.30.2 host profile"
        )
    checked_write(
        image, pe.optional + 0x68, struct.pack("<2I", import_rva, import_size),
        struct.pack("<2I", new_rva, new_descriptor_count * 20),
        "relocated import directory"
    )

    bootstrap_iat_va = 0x400000 + new_rva + bootstrap_iat_offset
    checked_write(
        image, pe.rva_to_offset(0x197353f), bytes.fromhex("50576800004000e82d100000"),
        b"\xff\x15" + struct.pack("<I", bootstrap_iat_va) + b"\x90" * 6,
        "WinMain bootstrap call"
    )

    # 1.14 world entry calls a Chakra script-interface predicate before that
    # interface is ready (sub_12EFEB0, first virtual call).  The call faults
    # and the loading screen never leaves.  Force the predicate result to
    # true and skip the broken call; the following checks then succeed and
    # the world loads.  This is a workaround, not a host-side fake.
    checked_write(
        image, pe.rva_to_offset(0xe956a5),
        bytes.fromhex("8b4e048b018b4004ffd0"),
        bytes.fromhex("b0019090909090909090"),
        "world-entry Chakra predicate skip"
    )
    # The Minecraft server script engine owns a ServerInstanceEventListener
    # subobject at engine+0x22c and registers it with the listener manager at
    # [ServerInstance+0x4c].  Server re-initialization creates a new engine,
    # stores it in ServerInstance+0xfc, deletes the old engine and then
    # registers the new listener.  The old engine's listener is never
    # unregistered, so the dispatch vector keeps a pointer to a freed engine;
    # after a world exit and re-entry the event loop dereferences the
    # recycled block as a vtable and faults.
    #
    # Replace the old-engine deletion at 0xf462f0-0xf462ff with a call to
    # removeListener([ServerInstance+0x4c], old_engine+0x22c).  The removal
    # helper is 0x5481f0, the same one the teardown path calls.  At this
    # point edi is the ServerInstance and edx is the old engine; the je at
    # 0xf462ee still skips the sequence when no old engine exists.  The
    # engine allocation itself is deliberately retained so a dispatch that
    # is already in flight cannot run through freed memory.  This is a small
    # per-world leak, matching the 1.16 listener workaround.
    reinit_rva = 0xF462F0
    remove_listener_va = 0x400000 + 0x5481F0
    checked_write(
        image, pe.rva_to_offset(reinit_rva),
        bytes.fromhex("8b028bcaeb0685c974068b016a01ff10"),
        b"\x8b\x4f\x4c"                      # mov ecx, [edi + 0x4c]
        b"\x8d\x82\x2c\x02\x00\x00"          # lea eax, [edx + 0x22c]
        b"\x50"                              # push eax
        b"\xe8" + struct.pack(
            "<i", remove_listener_va - (0x400000 + reinit_rva + 0x0F)
        ) +
        b"\x90",
        "unregister old script engine listener on server re-init"
    )
    # ServerInstance teardown unregisters the current listener from
    # [ServerInstance+0x4c], clears engine+0xfc, then deletes the engine.
    # Keep the allocation (0xf4756a je -> jmp) as a second safety net for
    # events queued before the listener was unregistered.
    checked_write(
        image, pe.rva_to_offset(0xf4756a),
        bytes.fromhex("7406"), bytes.fromhex("eb06"),
        "retain script engine on server teardown"
    )
    # The "Did you like Minecraft?" rating prompt is gated by the persisted
    # shown_ratings_prompt option and a launch counter.  When it fires it
    # reaches the UWP Store projection, which is unavailable in the
    # unpackaged host and crashes after the save-on-exit path.  The branch at
    # 0xf7b63 returns false when the option is already set; force that
    # "already shown" path so the prompt is never built.
    checked_write(
        image, pe.rva_to_offset(0xf7b63),
        bytes.fromhex("756b"), bytes.fromhex("eb6b"),
        "suppress rating prompt"
    )
    output.write_bytes(image)
    print(f"wrote {output} ({len(image)} bytes)")
    print(f"SHA-256 {hashlib.sha256(image).hexdigest()}")


CRT_IMPORT_REPLACEMENTS = {
    b"vccorlib140_app.dll": b"vccorlib140.dll",
    b"vcruntime140_app.dll": b"vcruntime140.dll",
    b"msvcp140_app.dll": b"msvcp140.dll",
    b"concrt140_app.dll": b"concrt140.dll",
}


def patch_crt_imports(source: Path, output: Path) -> None:
    """Redirect UWP app-local CRT imports in a game DLL to desktop names.

    The retail package ships ChakraCore, cohtml, HttpServer, MediaDecoders,
    RenoirCore and fmod with ``*_APP`` CRT imports.  Those DLLs only exist
    inside the package graph; an unpackaged process must use the regular
    VC++ runtime names.
    """
    original = source.read_bytes()
    image = bytearray(original)
    pe = Pe32(image)
    import_rva, import_size = struct.unpack_from("<2I", image, pe.optional + 0x68)
    if not import_rva or import_size < 20:
        raise ValueError(f"{source}: no import directory")
    cursor = pe.rva_to_offset(import_rva)
    patched = 0
    while True:
        values = struct.unpack_from("<5I", image, cursor)
        if not any(values):
            break
        name_offset = pe.rva_to_offset(values[3])
        end = image.index(0, name_offset)
        name = bytes(image[name_offset:end])
        replacement = CRT_IMPORT_REPLACEMENTS.get(name.lower())
        if replacement is not None:
            if len(replacement) > len(name):
                raise ValueError(
                    f"{source}: replacement {replacement!r} is longer than "
                    f"the import name {name!r}"
                )
            image[name_offset:end] = replacement.ljust(len(name), b"\0")
            patched += 1
        cursor += 20
    if patched:
        print(f"{source.name}: {patched} UWP CRT import(s) -> desktop")
    else:
        print(f"{source.name}: no UWP CRT imports")
    output.write_bytes(image)


def patch_renoir_windows7(source: Path, output: Path) -> None:
    """Redirect Renoir's unavailable Windows 8+ API sets to the host."""
    original = source.read_bytes()
    image = bytearray(original)
    pe = Pe32(image)
    import_rva, import_size = struct.unpack_from(
        "<2I", image, pe.optional + 0x68
    )
    if not import_rva or import_size < 20:
        raise ValueError("Renoir image has no import directory")
    found: set[bytes] = set()
    cursor = pe.rva_to_offset(import_rva)
    while True:
        descriptor = struct.unpack_from("<5I", image, cursor)
        if not any(descriptor):
            break
        name_offset = pe.rva_to_offset(descriptor[3])
        name = pe.c_string_at_rva(descriptor[3])
        normalized = name.lower()
        if normalized in RENOIR_WINDOWS7_API_SETS:
            if normalized in found:
                raise ValueError(f"duplicate Renoir import descriptor {name!r}")
            image[name_offset:name_offset + len(name)] = \
                b"win32craft.dll".ljust(len(name), b"\0")
            found.add(normalized)
            print(f"{name.decode()} -> win32craft.dll")
        cursor += 20
    if found != RENOIR_WINDOWS7_API_SETS:
        names = ", ".join(sorted(name.decode() for name in found)) or "none"
        raise ValueError(f"unexpected Renoir API-set group: {names}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(image)
    print(f"input SHA-256  {hashlib.sha256(original).hexdigest()}")
    print(f"output SHA-256 {hashlib.sha256(image).hexdigest()}")


def patch_cohtml_windows7(source: Path, output: Path) -> None:
    """Move cohtml's LoadPackagedLibrary import to win32craft.dll."""
    original = source.read_bytes()
    image = bytearray(original)
    pe = Pe32(image)
    import_rva, import_size = struct.unpack_from(
        "<2I", image, pe.optional + 0x68
    )
    cursor = pe.rva_to_offset(import_rva)
    descriptors: list[bytes] = []
    target_iat_rva = None
    while True:
        descriptor = bytes(image[cursor:cursor + 20])
        values = struct.unpack("<5I", descriptor)
        if not any(values):
            break
        descriptors.append(descriptor)
        if pe.c_string_at_rva(values[3]).lower() == b"kernel32.dll":
            ilt_rva = values[0] or values[4]
            ilt_offset = pe.rva_to_offset(ilt_rva)
            imports: dict[str, tuple[int, int]] = {}
            index = 0
            while True:
                hint_rva = struct.unpack_from(
                    "<I", image, ilt_offset + index * 4
                )[0]
                if not hint_rva:
                    break
                if hint_rva & 0x80000000:
                    raise ValueError("unexpected ordinal KERNEL32 import")
                imports[pe.c_string_at_rva(hint_rva + 2).decode("ascii")] = \
                    (index, hint_rva)
                index += 1
            target_index, target_hint = imports["LoadPackagedLibrary"]
            safe_hint = imports["FreeLibrary"][1]
            position = ilt_offset + target_index * 4
            if struct.unpack_from("<I", image, position)[0] != target_hint:
                raise ValueError("LoadPackagedLibrary thunk changed unexpectedly")
            struct.pack_into("<I", image, position, safe_hint)
            target_iat_rva = values[4] + target_index * 4
        cursor += 20
    if target_iat_rva is None:
        raise ValueError("LoadPackagedLibrary import was not found")

    last = pe.sections[-1]
    new_raw_offset = align(len(image), pe.file_alignment)
    new_rva = align(
        last.virtual_address + max(last.virtual_size, last.raw_size),
        pe.section_alignment,
    )
    descriptor_size = (len(descriptors) + 2) * 20
    section = bytearray(descriptor_size)

    def append(data: bytes, alignment: int = 1) -> int:
        section.extend(b"\0" * ((-len(section)) % alignment))
        offset = len(section)
        section.extend(data)
        return offset

    dll_offset = append(b"win32craft.dll\0")
    hint_offset = append(b"\0\0LoadPackagedLibrary\0", 2)
    ilt_offset = append(struct.pack("<2I", new_rva + hint_offset, 0), 4)
    relocated = descriptors + [struct.pack(
        "<5I", new_rva + ilt_offset, 0, 0,
        new_rva + dll_offset, target_iat_rva
    ), b"\0" * 20]
    section[:descriptor_size] = b"".join(relocated)
    virtual_size = len(section)
    raw_size = align(virtual_size, pe.file_alignment)
    section.extend(b"\0" * (raw_size - virtual_size))
    image.extend(b"\0" * (new_raw_offset - len(image)))
    image.extend(section)

    new_header = pe.section_table + pe.section_count * 40
    size_of_headers = struct.unpack_from("<I", image, pe.optional + 0x3C)[0]
    if new_header + 40 > size_of_headers:
        raise ValueError("no room for an additional section header")
    struct.pack_into(
        "<8s6I2HI", image, new_header, b".w7imp\0\0", virtual_size,
        new_rva, raw_size, new_raw_offset, 0, 0, 0, 0, 0xC0000040
    )
    struct.pack_into("<H", image, pe.file_header + 2, pe.section_count + 1)
    initialized = struct.unpack_from("<I", image, pe.optional + 8)[0]
    struct.pack_into("<I", image, pe.optional + 8, initialized + raw_size)
    struct.pack_into(
        "<I", image, pe.optional + 0x38,
        align(new_rva + virtual_size, pe.section_alignment)
    )
    struct.pack_into(
        "<2I", image, pe.optional + 0x68,
        new_rva, (len(descriptors) + 2) * 20
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(image)
    print("LoadPackagedLibrary -> win32craft.dll")
    print(f"input SHA-256  {hashlib.sha256(original).hexdigest()}")
    print(f"output SHA-256 {hashlib.sha256(image).hexdigest()}")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def pe_identity(path: Path) -> tuple[int, int, int]:
    """Return (machine, characteristics, optional-header magic)."""
    data = path.read_bytes()
    if len(data) < 0x40:
        raise ValueError(f"{path}: file is too short for a PE image")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError(f"{path}: invalid PE signature")
    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    characteristics = struct.unpack_from("<H", data, pe_offset + 4 + 18)[0]
    optional_magic = struct.unpack_from("<H", data, pe_offset + 24)[0]
    return machine, characteristics, optional_magic


def pe_size_of_image(path: Path) -> int:
    data = path.read_bytes()
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    return struct.unpack_from("<I", data, pe_offset + 0x50)[0]


def validate_host_dll(path: Path) -> None:
    machine, characteristics, optional_magic = pe_identity(path)
    if machine != 0x014C:
        raise ValueError(f"{path}: host DLL is not x86 (machine 0x{machine:04x})")
    if not characteristics & 0x2000:
        raise ValueError(f"{path}: host image is not a DLL")
    if optional_magic != 0x10B:
        raise ValueError(f"{path}: host DLL is not PE32")


def validate_exe_name(name: str) -> str:
    path = Path(name)
    if not name or path.name != name or path.suffix.lower() != ".exe":
        raise ValueError(
            f"exe name {name!r} must be a bare file name ending in .exe"
        )
    return name


def remove_analysis_files(folder: Path) -> None:
    """Drop IDA database files; they are large and useless in a package."""
    for path in sorted(folder.rglob("*")):
        if path.is_file() and path.suffix.lower() == ".i64":
            path.unlink()
            print(f"removed {path.relative_to(folder)}")


def write_patch_notes(output: Path, exe: Path, host_dll: Path,
                      input_digest: str) -> None:
    fmod = output / "fmod.dll"
    exe_digest = sha256_file(exe)
    host_digest = sha256_file(host_dll)
    fmod_digest = sha256_file(fmod)
    size_of_image = pe_size_of_image(exe)
    text = f"""# Minecraft 1.14.30.2 x86 - Win32Craft

Ready-to-run folder assembled by `tools\\1.14.3002.0-patcher.py --package`.

## Files

* `{exe.name}` - patched game executable
  * input SHA-256: `{input_digest}`
  * output SHA-256: `{exe_digest}`
  * size: {exe.stat().st_size} bytes
  * `SizeOfImage`: `0x{size_of_image:08X}`
* `win32craft.dll` - Win32Craft x86 host
  * SHA-256: `{host_digest}`
  * size: {host_dll.stat().st_size} bytes
* `fmod.dll` - desktop FMOD, copied from `fmod_X86.dll`
  * SHA-256: `{fmod_digest}`
  * size: {fmod.stat().st_size} bytes

The retail game DLLs (`ChakraCore.dll`, `cohtml.UWP.dll`,
`HttpServer.UWP.dll`, `MediaDecoders.UWP.dll`, `RenoirCore.UWP.dll` and
`fmod_X86.dll`) had their package-only `*_APP` CRT imports redirected to the
desktop VC++ runtime names.

Every `*.p7x` file and `AppxMetadata\\` were removed: the game runs as an
unpackaged Win32 process.  The `.Win32Craft` file contains the game type
(`Minecraft Bedrock Edition`) so repack tools can identify the folder.

Run `{exe.name}` with `WIN32CRAFT_DEBUG=1` to write `win32craft.log` next to
the executable.  User data lives under
`%APPDATA%\\MinecraftPE\\games\\com.mojang`.
"""
    (output / "PATCH_NOTES.md").write_text(text, encoding="utf-8",
                                            newline="\n")
    print("wrote PATCH_NOTES.md")


def package(source: Path, output: Path, host_dll: Path,
            force: bool = False,
            exe_name: str = PACKAGE_EXE_NAME) -> None:
    """Copy or extract a game folder and turn it into a Win32Craft package."""
    exe_name = validate_exe_name(exe_name)
    if not source.exists():
        raise ValueError(f"package source does not exist: {source}")
    if not host_dll.is_file():
        raise ValueError(f"host DLL does not exist: {host_dll}")
    validate_host_dll(host_dll)

    source_resolved = source.resolve()
    output_resolved = output.resolve()
    if source_resolved == output_resolved:
        raise ValueError("package output must differ from the source")
    if source.is_dir() and source_resolved in output_resolved.parents:
        raise ValueError("package output must not be inside the source folder")

    if output.exists():
        if not force:
            raise ValueError(
                f"package output already exists: {output} (use --force)"
            )
        if output.is_dir():
            shutil.rmtree(output)
        else:
            output.unlink()
    output.mkdir(parents=True)

    if source.is_dir():
        print(f"copying game folder {source} -> {output}")
        shutil.copytree(source, output, dirs_exist_ok=True)
    elif source.suffix.lower() == ".appx":
        print(f"extracting {source} -> {output}")
        with zipfile.ZipFile(source) as archive:
            archive.extractall(output)
    else:
        raise ValueError(
            "package source must be an extracted game folder or an .appx file"
        )

    # These belong to the signed package identity and are meaningless (and
    # confusing) in an unpackaged desktop folder.
    for name in ("AppxSignature.p7x", "AppxMetadata"):
        path = output / name
        if path.is_dir():
            shutil.rmtree(path)
            print(f"removed {name}/")
        elif path.exists():
            path.unlink()
            print(f"removed {name}")
    remove_analysis_files(output)

    # Some imported 1.14 folders keep a full stock backup next to the game
    # executable; it is not needed in the finished package.
    backup_exe = output / "Minecraft.Windows.exe.orig"
    if backup_exe.exists():
        backup_exe.unlink()
        print("removed Minecraft.Windows.exe.orig")

    original_exe = output / "Minecraft.Windows.exe"
    if not original_exe.is_file():
        raise ValueError(f"Minecraft.Windows.exe not found in {output}")
    input_digest = sha256_file(original_exe)
    exe = output / exe_name
    patch(original_exe, exe)
    if exe != original_exe:
        original_exe.unlink()
        print(f"renamed patched exe to {exe.name}")

    # The retail game DLLs import the package-only *_APP CRT names.  The
    # cracked package already has these redirected; a package assembled from
    # the retail folder must do the same or the loader fails with
    # STATUS_DLL_NOT_FOUND before the host can log anything.
    for name in (
        "ChakraCore.dll",
        "cohtml.UWP.dll",
        "HttpServer.UWP.dll",
        "MediaDecoders.UWP.dll",
        "RenoirCore.UWP.dll",
        "fmod_X86.dll",
    ):
        path = output / name
        if path.is_file():
            patch_crt_imports(path, path)
        else:
            print(f"warning: {name} not found; skipped")

    fmod_source = output / "fmod_X86.dll"
    if not fmod_source.is_file():
        raise ValueError(f"fmod_X86.dll not found in {output}")
    shutil.copyfile(fmod_source, output / "fmod.dll")
    print("fmod.dll <- fmod_X86.dll")

    shutil.copyfile(host_dll, output / "win32craft.dll")
    print(f"win32craft.dll <- {host_dll}")

    prepare_mcbe_package(output)
    write_patch_notes(output, exe, host_dll, input_digest)
    print(f"package ready: {output}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path)
    parser.add_argument("output", nargs="?", type=Path)
    parser.add_argument("--host-dll", default="win32craft.dll",
                        help="DLL name imported for the Win32 host")
    parser.add_argument("--package", action="store_true",
                        help="assemble a ready-to-run folder from a game "
                             "folder or .appx")
    parser.add_argument("--host", type=Path,
                        help="path to the Win32Craft x86 host DLL "
                             "(required with --package)")
    parser.add_argument("--exe-name", default=PACKAGE_EXE_NAME,
                        help="patched exe name inside the package "
                             f"(default: {PACKAGE_EXE_NAME})")
    parser.add_argument("--force", action="store_true",
                        help="replace an existing package output folder")
    args = parser.parse_args()

    if args.package:
        if args.source is None or args.output is None or args.host is None:
            parser.error(
                "--package requires SOURCE, OUTPUT and --host PATH"
            )
        package(args.source, args.output, args.host, args.force,
                args.exe_name)
        return

    source = args.source or Path("Minecraft.Windows.exe")
    output = args.output or Path("Win32Craft.1.14.30.2.exe")
    patch(source, output, args.host_dll)


if __name__ == "__main__":
    main()
