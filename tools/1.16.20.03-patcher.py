#!/usr/bin/env python3
"""Create the Win32-hosted Minecraft 1.16.20.03 executable.

The executable-side patch sites are version-specific.  The matching Win32
host profile lives in ``win32_host_116.inl`` (``game_profile_116_02003``,
selected by ``SizeOfImage == 0x02DB1000``).  Two retail x86 revisions are
accepted (see ``SUPPORTED_SHA256``); they patch identically.

The default build keeps the Windows inbox ``chakra.dll`` import, matching the
retail UWP package and the known-good D3DCraft conversion.  The diagnostic
``--chakra-core`` mode redirects that import to the package
``ChakraCore.dll`` for comparison; it breaks experimental-gameplay scripts.

Use ``--package`` to assemble a ready-to-run folder from an extracted game
folder or an ``.appx``: it copies/extracts the game, patches the exe to
``Win32Craft.1.16.20.03.exe``, rewrites the package-only ``*_APP`` CRT
imports, copies the host DLL as ``win32craft.dll``, creates ``fmod.dll`` from
``fmod_X86.dll``, removes ``*.p7x`` files and writes the ``.Win32Craft``
marker (``Minecraft Bedrock Edition``).
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


# Two retail revisions of the same x86 build are in circulation.  Every patch
# site is identical, so both are accepted.
SUPPORTED_SHA256 = {
    "9ab7e10904d5acf1afe83ab0b666c1426aa61d71f6b15d782d1ae7062703ec63":
        "retail x86 revision A",
    "330d3898aec87fad0b9a47da317418cfb1431673438cf60dc7dee5ed35161b9f":
        "retail x86 revision B (Minecraft_1.16.20.03_x86-Cracked_by_MaxRM)",
}

PACKAGE_EXE_NAME = "Win32Craft.1.16.20.03.exe"

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

# The legacy D3DCraft 1.16.20.03 build is the known-good x86 conversion.  Its
# 60-export host and the support DLLs are runtime inputs: the patcher still
# rebuilds the game exe from the original Minecraft.Windows.exe, but it copies
# this exact runtime set so the result matches the tested conversion.  The
# hashes are validated to keep a rebuilt package from silently mixing versions.
LEGACY_HOST_NAME = "d3dcraft.dll"
LEGACY_HOST_SHA256 = \
    "995139d3b591206c3c48025baa422eb71659100c0627fee821a2e523e0d07bcd"
LEGACY_RUNTIME_SHA256 = {
    "ChakraCore.dll":
        "3d4940c34ba6a1f584a281cac06e46c807af57837ba95d06ae36080ec536ad7d",
    "cohtml.UWP.dll":
        "5e970ab7ecef972be7091e1d3f49b8bedc33d88a6a7660fcea5ad255267c2fa9",
    "RenoirCore.UWP.dll":
        "d99581dcc7b26b47c24c85ddc404e04268c944d84579c19e7698e2d89a83e695",
    "fmod.dll":
        "773b9cdc62cc3f50591020ec0866e8ce8dcc6d14f0f390b964acca458503f7d1",
    "api-ms-win-core-com-l1-1-1.dll":
        "f8960cef6afb60aec6527e9546d8bafdf2a08a945f9465b5bd0d719e1a778336",
    "api-ms-win-core-winrt-error-l1-1-0.dll":
        "d2f010dd7580c31e52e2021d108774cc5f40c95ba0739cc1ea0dd4f88c58d796",
    "api-ms-win-core-winrt-error-l1-1-1.dll":
        "62c785fa31b273a185352e33aa0f1e86f2d3379902342f5e21c2a793c7b0fd0d",
    "api-ms-win-core-winrt-l1-1-0.dll":
        "87b6936a8be5fb9cd464916371c75be2f3155bef709de18f03c34afaa9ce9c83",
    "api-ms-win-core-winrt-string-l1-1-0.dll":
        "5cac07da8791c6246b67dee65014c6eb28d10380377c8693a3780f053bb82f7f",
    "api-ms-win-eventing-provider-l1-1-0.dll":
        "f44e5d10c566e35dae1e1d1e15909c1e615945297380877a6f0fce1282071c02",
    "api-ms-win-gaming-tcui-l1-1-0.dll":
        "d84dbab1e3fd5b673625bed0427706cc78c6c4bda95e3b2ac5841c29e78d1e50",
    "XINPUTUAP.dll":
        "c4b01eaeb42c68d02da24675dcdd4a37ab8cc3dbe24849be7718b8f30f69e1e9",
}
# Files present in the retail package that the legacy conversion does not use.
LEGACY_REMOVED_FILES = (
    "fmod_X86.dll",
    "HttpServer.UWP.dll",
    "MediaDecoders.UWP.dll",
)


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


def patch(source: Path, output: Path, legacy_host: bool = False,
          chakra_core: bool = False) -> None:
    """Patch a retail 1.16.20.03 exe.

    ``legacy_host`` reproduces the original D3DCraft 1.16.20.03 conversion:
    the game keeps the system ``chakra.dll`` and the package shims, the
    ``dxgi``/``d3d11`` imports are redirected to ``d3dcraft.dll`` and the
    legacy 60-export host is bootstrapped from the added ``.d3imp`` section.
    The default builds the current shared ``win32craft.dll`` host.

    The shared host keeps the retail ``chakra.dll`` import by default, so the
    game uses the Windows inbox Chakra engine.  ``chakra_core`` is a
    diagnostic mode that redirects the import to the package
    ``ChakraCore.dll`` instead; ChakraCore lacks ``JsStartDebugging`` and its
    behavioral differences break experimental-gameplay scripts.  The legacy
    host always uses the system engine and ignores this flag.
    """
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
    image = bytearray(original)
    pe = Pe32(image)

    # Desktop loader header.  The UWP image already has the 32-bit
    # large-address-aware flag; keep it and only verify it.  Claim Windows 7
    # compatibility for the OS/subsystem minor version.  Clear APPCONTAINER
    # (0x1000) so a normal desktop process can start the exe, and clear
    # DYNAMIC_BASE (0x40) because the patch writes absolute 0x400000-based
    # addresses; NX_COMPAT and TERMINAL_SERVER_AWARE stay set.
    checked_write(
        image, pe.file_header + 18, struct.pack("<H", 0x0122),
        struct.pack("<H", 0x0122), "large-address-aware"
    )
    checked_write(image, pe.optional + 0x2A, b"\x02\x00", b"\x01\x00", "OS minor")
    checked_write(
        image, pe.optional + 0x32, b"\x02\x00", b"\x01\x00",
        "subsystem minor"
    )
    checked_write(
        image, pe.optional + 0x46, struct.pack("<H", 0x9140),
        struct.pack("<H", 0x8100), "desktop DLL characteristics"
    )

    # The Win32 PnP fallback intentionally supplies no WinRT hardware object.
    # This template specialization otherwise dereferences that null operation
    # and its synchronous caller subsequently waits forever.  Mark its task
    # implementation Completed (state 2); the continuation consumes the null
    # PnpObject as the supported "metadata unavailable" result.
    checked_write(
        image, pe.rva_to_offset(0x016edd4f),
        bytes.fromhex("8b0b5653ff511885c0782b"),
        bytes.fromhex("8b07c74064020000009090"),
        "complete unavailable PnP task"
    )
    # The matching PnP metadata wait would never complete once the null object
    # is accepted; skip the wait and continue with the default metadata.
    checked_write(
        image, pe.rva_to_offset(0x016e30d7),
        bytes.fromhex("e8544fb8fe"), b"\x90" * 5,
        "skip unavailable startup metadata wait"
    )
    # PointerVisualizationSettings only disables touch/contact feedback.  It
    # has no desktop equivalent and its C++/CX projection relies on an agile
    # WinRT factory identity that is unavailable in the unpackaged process.
    # Skip this self-contained optional block and continue with CoreWindow
    # event registration at 0x01AE3FAD.  Keep the temporary delegate alive:
    # the function's common epilogue releases EDI unconditionally.
    checked_write(
        image, pe.rva_to_offset(0x016e3f5d),
        bytes.fromhex("ff5008"), b"\x90" * 3,
        "retain skipped pointer visualization delegate"
    )
    checked_write(
        image, pe.rva_to_offset(0x016e3f60),
        bytes.fromhex("e8db340000"), bytes.fromhex("e948000000"),
        "skip pointer visualization setup"
    )
    # A late optional startup notification can have an empty std::function
    # in the unpackaged host.  The UWP event source normally installs this
    # callback; calling it empty enters std::_Xbad_function_call and Wine's
    # unavailable _invoke_watson implementation.  Treat an empty callback as
    # the intended no-op and return from the two-argument wrapper.
    checked_write(
        image, pe.rva_to_offset(0x016d3002),
        bytes.fromhex("ff1528642a02"), bytes.fromhex("33c05dc20800"),
        "ignore absent optional startup callback"
    )
    # The same optional notification is invoked from the loading-state
    # transition as well.  When its std::function is empty, skip the invoke
    # sequence and continue at the existing local cleanup path.
    checked_write(
        image, pe.rva_to_offset(0x016d42fb),
        bytes.fromhex("ff1528642a02"), bytes.fromhex("e92a00000090"),
        "skip absent loading-state callback"
    )
    # The "Did you like Minecraft?" rating prompt is gated by the persisted
    # shown_ratings_prompt option (ID 0x174) and a launch counter (ID 0x175).
    # When the prompt fires it reaches the UWP Store projection, which is
    # unavailable in the unpackaged host and crashes after the save-on-exit
    # path.  The trigger at 0x567830 returns false when the option is already
    # set; force that "already shown" branch so the prompt is never built.
    checked_write(
        image, pe.rva_to_offset(0x0167906),
        bytes.fromhex("7585"), bytes.fromhex("eb85"),
        "suppress rating prompt"
    )
    # Activation and window-state diagnostics stringify boxed WinRT enums.
    # Wine has an aborting Enum::ToString export. A null HSTRING is the
    # supported empty diagnostic value; retain the surrounding COM releases.
    for rva in (0x016e57bf, 0x016e5bae):
        checked_write(image, pe.rva_to_offset(rva),
                      bytes.fromhex("ff15e06b2a02"),
                      bytes.fromhex("33c090909090"),
                      "omit optional WinRT enum diagnostic")
    # The legacy D3DCraft conversion was validated without this workaround and
    # keeps the original destroy path.  The shared host keeps the listener
    # patch: server re-initialization frees the previous engine while its
    # ServerInstanceEventListener may still be in the dispatcher vector, so a
    # later dispatch crashes on the stale listener.
    if not legacy_host:
        # Server re-initialization replaces the MinecraftServerScriptEngine
        # stored at ServerInstance+0x144 and deletes the previous engine, but
        # the engine's ServerInstanceEventListener stays in the dispatcher
        # vector.  A later event dispatch calls the freed listener and crashes
        # on world entry (deterministic 0xc0000005 at the stale listener's
        # vtable slot).  Keep the previous engine alive: it is only 0x250
        # bytes per server re-initialization, and its listener remains valid
        # for dispatch.
        checked_write(
            image, pe.rva_to_offset(0x01023a0d),
            bytes.fromhex("7410"), bytes.fromhex("eb10"),
            "retain previous script engine listener"
        )
    # The unpackaged process cannot load the package-only app-local CRT DLLs
    # (vccorlib140_app, MSVCP140_APP, ...).  Rename those imports to the
    # desktop VC++ runtime names; the host supplies the few vccorlib helpers.
    replace_all(image, b"vccorlib140_app.DLL", b"vccorlib140.DLL", 1,
                "desktop vccorlib")
    replace_all(image, b"MSVCP140_APP.dll", b"MSVCP140.dll", 1,
                "desktop MSVCP")
    replace_all(image, b"CONCRT140_APP.dll", b"CONCRT140.dll", 1,
                "desktop CONCRT")
    replace_all(image, b"VCRUNTIME140_APP.dll", b"VCRUNTIME140.dll", 1,
                "desktop VCRUNTIME")
    # The retail package calls the 32-bit UWP FMOD DLL fmod_X86.dll; the
    # ready-to-run folder copies it to fmod.dll, which is the name both the
    # game exe and the host's delay-import descriptor expect.
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
    if len(descriptors) != 32 or import_size != 0x294:
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

    # Windows 7 does not export the three delay-load helper functions.  Point
    # their ILT entries at GetLastError (a harmless existing export) and add
    # host descriptors later that reuse the original IAT cells, so the image
    # resolves and never calls the missing helpers.
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

    # The legacy host and the default shared host keep the retail chakra.dll
    # import intact.  The diagnostic ChakraCore mode redirects it to the
    # package ChakraCore.dll and stubs the one export ChakraCore does not
    # provide.
    debug_iat_rva = None
    if not legacy_host and chakra_core:
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

    # Build the replacement import directory in a new read/write .d3imp
    # section.  The original descriptors are copied, host-facing DLL names are
    # redirected to win32craft.dll, and new descriptors are appended for
    # Win32Bootstrap and the Windows 7 delay-load compatibility shims.  The
    # bootstrap IAT is called from the patched WinMain below.
    last = pe.sections[-1]
    new_raw_offset = align(len(image), pe.file_alignment)
    new_rva = align(
        last.virtual_address + max(last.virtual_size, last.raw_size),
        pe.section_alignment,
    )
    if legacy_host:
        # Original D3DCraft layout: 32 retail descriptors with dxgi/d3d11
        # renamed to d3dcraft.dll, then four d3dcraft.dll descriptors for the
        # bootstrap and the KERNEL32 delay-load helpers.  The section is
        # deliberately kept at VirtualSize 0x378 / RawSize 0x400.
        legacy_names = ("Win32Bootstrap",) + compatibility_names
        new_descriptor_count = len(descriptors) + len(legacy_names) + 1
        section = bytearray(new_descriptor_count * 20)

        def append_legacy(data: bytes, alignment: int = 1) -> int:
            section.extend(b"\0" * ((-len(section)) % alignment))
            offset = len(section)
            section.extend(data)
            return offset

        dll_name_offset = append_legacy(b"d3dcraft.dll\0")
        hint_offsets: dict[str, int] = {}
        for name in legacy_names:
            hint_offsets[name] = append_legacy(
                b"\0\0" + name.encode("ascii") + b"\0", 2
            )
        ilt_offsets: dict[str, int] = {}
        for name in legacy_names:
            ilt_offsets[name] = append_legacy(
                struct.pack("<2I", new_rva + hint_offsets[name], 0), 4
            )
        bootstrap_iat_offset = append_legacy(
            struct.pack("<2I", new_rva + hint_offsets["Win32Bootstrap"], 0), 4
        )

        relocated: list[bytes] = []
        for dll_name, descriptor in descriptors:
            values = list(struct.unpack("<5I", descriptor))
            if dll_name in (b"dxgi.dll", b"d3d11.dll"):
                values[3] = new_rva + dll_name_offset
            relocated.append(struct.pack("<5I", *values))
        relocated.append(struct.pack(
            "<5I", new_rva + ilt_offsets["Win32Bootstrap"], 0, 0,
            new_rva + dll_name_offset, new_rva + bootstrap_iat_offset
        ))
        for name in compatibility_names:
            relocated.append(struct.pack(
                "<5I", new_rva + ilt_offsets[name], 0, 0,
                new_rva + dll_name_offset, compatibility_iats[name]
            ))
        relocated.append(b"\0" * 20)
        if len(relocated) != new_descriptor_count:
            raise ValueError(
                f"legacy import table has {len(relocated)} descriptors, "
                f"expected {new_descriptor_count}"
            )
        section[:new_descriptor_count * 20] = b"".join(relocated)
    else:
        extra_descriptors = 2 if not chakra_core else 3
        new_descriptor_count = (
            len(descriptors) + len(compatibility_names) + extra_descriptors
        )
        section = bytearray(new_descriptor_count * 20)

        def append(data: bytes, alignment: int = 1) -> int:
            section.extend(b"\0" * ((-len(section)) % alignment))
            offset = len(section)
            section.extend(data)
            return offset

        dll_name_offset = append(b"win32craft.dll\0")
        chakra_name_offset = None if not chakra_core else append(b"ChakraCore.dll\0")
        hint_offsets: dict[str, int] = {}
        host_names = ("Win32Bootstrap",) + compatibility_names
        if chakra_core:
            host_names = ("Win32Bootstrap", "JsStartDebugging") + \
                compatibility_names
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
            elif dll_name == b"chakra.dll" and chakra_name_offset is not None:
                values[3] = new_rva + chakra_name_offset
            relocated.append(struct.pack("<5I", *values))
        relocated.append(struct.pack(
            "<5I", new_rva + ilt_offsets["Win32Bootstrap"], 0, 0,
            new_rva + dll_name_offset, new_rva + bootstrap_iat_offset
        ))
        if chakra_core:
            if debug_iat_rva is None:
                raise ValueError("missing JsStartDebugging IAT slot")
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
            new_raw_offset, 0, 0, 0, 0, 0xC0000040
        ), "new .d3imp section header"
    )
    checked_write(
        image, pe.file_header + 2, struct.pack("<H", pe.section_count),
        struct.pack("<H", pe.section_count + 1), "section count"
    )
    # The host identifies the game build by SizeOfImage.  The appended section
    # must make it exactly 0x02DB1000 so bootstrap_116 selects
    # game_profile_116_02003; keep the size fields consistent.
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
    if new_image_size != 0x02DB1000:
        raise ValueError(
            f"patched SizeOfImage is 0x{new_image_size:08X}, expected "
            "0x02DB1000 for the 1.16.20.03 host profile"
        )
    checked_write(
        image, pe.optional + 0x68, struct.pack("<2I", import_rva, import_size),
        struct.pack("<2I", new_rva, new_descriptor_count * 20),
        "relocated import directory"
    )

    # Redirect the CRT's WinMain call through the Win32Bootstrap IAT.  The
    # absolute VA is safe because DYNAMIC_BASE was cleared in the header.
    bootstrap_iat_va = 0x400000 + new_rva + bootstrap_iat_offset
    checked_write(
        image, pe.rva_to_offset(0x1c471af), bytes.fromhex("50576800004000e83e1b0000"),
        b"\xff\x15" + struct.pack("<I", bootstrap_iat_va) + b"\x90" * 6,
        "WinMain bootstrap call"
    )
    output.write_bytes(image)
    print(f"wrote {output} ({len(image)} bytes)")
    print(f"SHA-256 {hashlib.sha256(image).hexdigest()}")


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
    VC++ runtime names or the loader stops with STATUS_DLL_NOT_FOUND before
    the host can log anything.
    """
    original = source.read_bytes()
    image = bytearray(original)
    pe = Pe32(image)
    import_rva, import_size = struct.unpack_from("<2I", image, pe.optional + 0x68)
    if not import_rva or import_size < 20:
        # The WinRT API-set shims are forwarding stubs without imports; there
        # is nothing to redirect in them.
        print(f"{source.name}: no import directory; skipped")
        return
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
                      input_digest: str, legacy: bool = False) -> None:
    exe_digest = sha256_file(exe)
    host_digest = sha256_file(host_dll)
    size_of_image = pe_size_of_image(exe)
    if legacy:
        runtime_lines = []
        for name in LEGACY_RUNTIME_SHA256:
            path = output / name
            runtime_lines.append(
                f"* `{name}`\n"
                f"  * SHA-256: `{sha256_file(path)}`\n"
                f"  * size: {path.stat().st_size} bytes"
            )
        runtime_list = "\n".join(runtime_lines)
        text = f"""# Minecraft 1.16.20.03 x86 - Win32Craft (legacy D3DCraft host)

Ready-to-run folder assembled by `tools\\1.16.20.03-patcher.py --package
--legacy-runtime ...`.

## Files

* `{exe.name}` - patched game executable
  * input SHA-256: `{input_digest}`
  * output SHA-256: `{exe_digest}`
  * size: {exe.stat().st_size} bytes
  * `SizeOfImage`: `0x{size_of_image:08X}`
* `d3dcraft.dll` - legacy 60-export D3DCraft x86 host
  * SHA-256: `{host_digest}`
  * size: {host_dll.stat().st_size} bytes

## Legacy runtime

The game keeps the retail `chakra.dll` import.  These support DLLs are copied
from the validated D3DCraft runtime folder:

{runtime_list}

`fmod_X86.dll`, `HttpServer.UWP.dll` and `MediaDecoders.UWP.dll` are not part
of the legacy conversion and are removed.

The exe keeps the original engine destroy path (no new-host listener leak
patch) and does not redirect `chakra.dll` to ChakraCore.  The Store rating
prompt is still suppressed by the `0x166d06` patch.  The `dxgi.dll` and
`d3d11.dll` imports are redirected to `d3dcraft.dll`, which supplies
`CreateDXGIFactory2`, `D3D11CreateDevice` and
`CreateDirect3D11DeviceFromDXGIDevice` at the original IAT slots.

Every `*.p7x` file and `AppxMetadata\\` were removed: the game runs as an
unpackaged Win32 process.  The `.Win32Craft` file contains the game type
(`Minecraft Bedrock Edition`) so repack tools can identify the folder.

Run `{exe.name}` with `WIN32CRAFT_DEBUG=1` to write `win32craft.log` next to
the executable.  User data lives under
`%APPDATA%\\MinecraftPE\\games\\com.mojang`.
"""
    else:
        fmod = output / "fmod.dll"
        fmod_digest = sha256_file(fmod)
        text = f"""# Minecraft 1.16.20.03 x86 - Win32Craft

Ready-to-run folder assembled by `tools\\1.16.20.03-patcher.py --package`.

## Files

* `{exe.name}` - patched game executable
  * input SHA-256: `{input_digest}`
  * output SHA-256: `{exe_digest}`
  * size: {exe.stat().st_size} bytes
  * `SizeOfImage`: `0x{size_of_image:08X}`
* `win32craft.dll` - Win32Craft x86 host (includes the shared 1.16
  `FileOpenPicker.FileTypeFilter` skin-picker fix)
  * SHA-256: `{host_digest}`
  * size: {host_dll.stat().st_size} bytes
* `fmod.dll` - desktop FMOD, copied from `fmod_X86.dll`
  * SHA-256: `{fmod_digest}`
  * size: {fmod.stat().st_size} bytes

The game executable keeps the Windows inbox `chakra.dll` import, matching the
retail package and the known-good D3DCraft conversion.  The package still
ships `ChakraCore.dll` because `cohtml.UWP.dll` imports it.

The shared-host build also suppresses the Store rating prompt (`jne` -> `jmp`
at file offset `0x166d06`) so a normal exit does not call the unavailable UWP
Store API, and keeps the previous script engine alive across server
re-initialization (file offset `0x1022e0d`).

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
            exe_name: str = PACKAGE_EXE_NAME,
            legacy_runtime: Path | None = None,
            chakra_core: bool = False) -> None:
    """Copy or extract a game folder and turn it into a Win32Craft package."""
    legacy = legacy_runtime is not None
    exe_name = validate_exe_name(exe_name)
    if not source.exists():
        raise ValueError(f"package source does not exist: {source}")
    if not host_dll.is_file():
        raise ValueError(f"host DLL does not exist: {host_dll}")
    validate_host_dll(host_dll)
    if legacy:
        if not legacy_runtime.is_dir():
            raise ValueError(
                f"legacy runtime folder does not exist: {legacy_runtime}"
            )
        host_digest = sha256_file(host_dll)
        if host_digest != LEGACY_HOST_SHA256:
            raise ValueError(
                f"{host_dll}: SHA-256 {host_digest} does not match the tested "
                f"legacy host ({LEGACY_HOST_SHA256})"
            )

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

    original_exe = output / "Minecraft.Windows.exe"
    if not original_exe.is_file():
        raise ValueError(f"Minecraft.Windows.exe not found in {output}")
    input_digest = sha256_file(original_exe)
    exe = output / exe_name
    patch(original_exe, exe, legacy_host=legacy, chakra_core=chakra_core)
    if exe != original_exe:
        original_exe.unlink()
        print(f"renamed patched exe to {exe.name}")

    if legacy:
        # The legacy conversion uses an older, validated runtime set.  Copy it
        # after extraction so a retail package cannot mix ChakraCore versions
        # with the legacy d3dcraft.dll host.
        for name, expected in LEGACY_RUNTIME_SHA256.items():
            source_path = legacy_runtime / name
            if not source_path.is_file():
                raise ValueError(f"legacy runtime file is missing: {source_path}")
            digest = sha256_file(source_path)
            if digest != expected:
                raise ValueError(
                    f"{source_path}: SHA-256 {digest} does not match the "
                    f"tested legacy runtime ({expected})"
                )
            shutil.copyfile(source_path, output / name)
            print(f"{name} <- {source_path}")
        for name in LEGACY_REMOVED_FILES:
            path = output / name
            if path.exists():
                path.unlink()
                print(f"removed {name}")
        stale_host = output / "win32craft.dll"
        if stale_host.exists():
            stale_host.unlink()
            print("removed win32craft.dll")
        shutil.copyfile(host_dll, output / LEGACY_HOST_NAME)
        print(f"{LEGACY_HOST_NAME} <- {host_dll}")
        # The legacy DLLs already use desktop CRT names, but keep the check so
        # a different runtime folder cannot leave package-only imports behind.
        for name in LEGACY_RUNTIME_SHA256:
            patch_crt_imports(output / name, output / name)
    else:
        # The retail game DLLs import the package-only *_APP CRT names.  The
        # loader fails with STATUS_DLL_NOT_FOUND before the host can log
        # anything unless these are redirected to the desktop runtime names.
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
    write_patch_notes(output, exe, host_dll, input_digest, legacy=legacy)
    print(f"package ready: {output}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path)
    parser.add_argument("output", nargs="?", type=Path)
    parser.add_argument("--package", action="store_true",
                        help="assemble a ready-to-run folder from a game "
                             "folder or .appx")
    parser.add_argument("--host", type=Path,
                        help="path to the host DLL (win32craft.dll, or "
                             "d3dcraft.dll with --legacy-runtime)")
    parser.add_argument("--legacy-runtime", type=Path,
                        help="folder with the tested d3dcraft.dll host "
                             "runtime DLLs; enables the legacy D3DCraft host")
    parser.add_argument("--exe-name", default=PACKAGE_EXE_NAME,
                        help="patched exe name inside the package "
                             f"(default: {PACKAGE_EXE_NAME})")
    parser.add_argument("--force", action="store_true",
                        help="replace an existing package output folder")
    parser.add_argument("--legacy-host", action="store_true",
                        help="reproduce the original D3DCraft host layout "
                             "(d3dcraft.dll, system chakra.dll, no leak patch)")
    parser.add_argument("--chakra-core", action="store_true",
                        help="diagnostic: redirect the game's chakra.dll "
                             "import to the package ChakraCore.dll (old "
                             "behavior; breaks experimental gameplay)")
    parser.add_argument("--system-chakra", action="store_true",
                        help="deprecated; the Windows inbox chakra.dll is "
                             "now the default")
    args = parser.parse_args()

    legacy_host = args.legacy_host or args.legacy_runtime is not None
    if args.chakra_core and legacy_host:
        parser.error("--chakra-core cannot be combined with the legacy host")
    if args.system_chakra and args.chakra_core:
        parser.error("--system-chakra and --chakra-core are mutually exclusive")
    if args.package:
        if args.source is None or args.output is None:
            parser.error("--package requires SOURCE and OUTPUT")
        if args.legacy_host and args.legacy_runtime is None:
            parser.error(
                "--package --legacy-host requires --legacy-runtime DIR"
            )
        host = args.host
        if host is None and args.legacy_runtime is not None:
            host = args.legacy_runtime / LEGACY_HOST_NAME
        if host is None:
            parser.error("--package requires --host PATH")
        package(args.source, args.output, host, args.force,
                args.exe_name, legacy_runtime=args.legacy_runtime,
                chakra_core=args.chakra_core)
        return

    patch(args.source or Path("Minecraft.Windows.exe"),
          args.output or Path(PACKAGE_EXE_NAME),
          legacy_host=legacy_host, chakra_core=args.chakra_core)


if __name__ == "__main__":
    main()
