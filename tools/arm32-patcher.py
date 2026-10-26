#!/usr/bin/env python3
"""arm32-patcher - table-driven PE patcher for the Win32Craft ARM32 ports.

One engine, many versions.  A *profile* (plain data, see PROFILES below, or a
JSON file passed with --profile-file) says which bytes of which game version to
change; the engine does the mechanics and refuses to write anything it cannot
verify.  Everything the engine writes is checked against the expected old bytes
first, and every edit is reported (file offset, old bytes, new bytes, reason).

Commands
  detect <exe|folder>                  say which version this is and which profile applies
  patch  --profile auto|NAME <game-folder> <out-folder> [options]
                                       copy the folder, patch exe + companion DLLs, write
                                       patch_report.json, re-parse and verify the result
  probe  <exe>                         auto-discovery for UNKNOWN versions (facts the exe itself
                                       can give; a new profile still needs host facts)
  verify <exe|folder> [--profile NAME] re-parse an already patched exe/folder and run the checks
  profiles [--dump NAME]               list profiles / print one as JSON (template for new ones)
  dll    <in.dll> <out.dll> [--profile NAME]
                                       patch one companion DLL (APPCONTAINER cleared, renames...)

Engine capabilities (all driven by profile data)
  * add a section with explicit characteristics (W+X is refused), extend the import
    directory with `<dll>!<export>` (new IAT), optional descriptor redirects
  * position independent Thumb-2 trampoline in .text padding + BL/B.W call site rewrite
    with range check and round-trip decode
  * byte patches with expected-old-bytes (and context) verification, optional groups
  * header edits (DllCharacteristics, NumberOfSections, SizeOfImage, SizeOfInitializedData,
    checksum keep|recompute, base relocations keep|drop), in-place import renames
  * companion DLL patching (APPCONTAINER 0x1140 -> 0x0140, renames, byte patches) from a
    folder, a --companion-dir or straight out of a VCLibs .appx
  * JSON report of every edit; confinement check (the output differs from the input ONLY in
    the recorded edits)

Python 3.9+, standard library only (capstone is used for self-checks when installed).
Examples
  python tools/arm32-patcher.py detect "D:/games/MCPE 0.10.4.40"
  python tools/arm32-patcher.py patch --profile auto "D:/games/MCPE 0.10.4.40" out/mcpe0104
  python tools/arm32-patcher.py probe  "D:/games/unknown/Game.exe"
"""

from __future__ import annotations

import argparse
import array
import bisect
import copy
import hashlib
import json
import os
import re
import shutil
import struct
import sys
import time
import zipfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

try:  # optional, self-checks only
    import capstone as _capstone
except Exception:  # pragma: no cover
    _capstone = None

TOOL = "arm32-patcher"
TOOL_VERSION = "1.0"

MACHINE_ARMNT = 0x01C4
SCN_CNT_CODE = 0x00000020
SCN_MEM_DISCARDABLE = 0x02000000
SCN_MEM_EXECUTE = 0x20000000
SCN_MEM_READ = 0x40000000
SCN_MEM_WRITE = 0x80000000
DLLCHAR_DYNAMIC_BASE = 0x0040
DLLCHAR_APPCONTAINER = 0x1000
FILE_RELOCS_STRIPPED = 0x0001
IMAGE_DIR_EXPORT, IMAGE_DIR_IMPORT, IMAGE_DIR_RESOURCE, IMAGE_DIR_EXCEPTION = 0, 1, 2, 3
IMAGE_DIR_SECURITY, IMAGE_DIR_BASERELOC, IMAGE_DIR_DELAY_IMPORT = 4, 5, 13


class PatchError(Exception):
    """A verification failed or the input is not what the profile expects."""


# ---------------------------------------------------------------------------
# small helpers
# ---------------------------------------------------------------------------

def align_up(value: int, boundary: int) -> int:
    return (value + boundary - 1) & ~(boundary - 1)


def sha256_hex(data: bytes | bytearray) -> str:
    return hashlib.sha256(bytes(data)).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def hexstr(data: bytes | bytearray, limit: int = 0) -> str:
    text = bytes(data).hex()
    if limit and len(text) > limit * 2:
        return text[:limit * 2] + "..."
    return text


def h(value: int | None) -> str:
    return "None" if value is None else "0x%X" % value


def say(text: str = "") -> None:
    print(text)


def flags_text(flags: int) -> str:
    return "".join([
        "R" if flags & SCN_MEM_READ else "-",
        "W" if flags & SCN_MEM_WRITE else "-",
        "X" if flags & SCN_MEM_EXECUTE else "-",
    ])


# ---------------------------------------------------------------------------
# PE32 parser (read only; patching happens on a bytearray, then re-parsed)
# ---------------------------------------------------------------------------

_FILE = {"machine": (0, "H"), "number_of_sections": (2, "H"), "timestamp": (4, "I"),
         "size_of_optional_header": (16, "H"), "characteristics": (18, "H")}
_OPT = {"magic": (0, "H"), "size_of_code": (4, "I"), "size_of_init_data": (8, "I"),
        "size_of_uninit_data": (12, "I"), "entry": (16, "I"), "base_of_code": (20, "I"),
        "base_of_data": (24, "I"), "image_base": (28, "I"), "section_alignment": (32, "I"),
        "file_alignment": (36, "I"), "os_version": (40, "2H"), "image_version": (44, "2H"),
        "subsystem_version": (48, "2H"), "win32_version": (52, "I"), "size_of_image": (56, "I"),
        "size_of_headers": (60, "I"), "checksum": (64, "I"), "subsystem": (68, "H"),
        "dll_characteristics": (70, "H"), "num_rva_sizes": (92, "I")}
_RELOC_WIDTH = {1: 2, 2: 2, 3: 4, 4: 4, 5: 8, 7: 8, 10: 8}


class Section:
    def __init__(self, index: int, hdr_off: int, raw: bytes) -> None:
        self.index = index
        self.hdr_off = hdr_off
        self.name = raw[:8].rstrip(b"\0").decode("latin-1")
        (self.vsize, self.va, self.rsize, self.roff, self.reloc_ptr, self.line_ptr,
         self.nreloc, self.nline, self.flags) = struct.unpack_from("<6I2HI", raw, 8)

    @property
    def extent(self) -> int:
        return max(self.vsize, self.rsize)

    @property
    def writable(self) -> bool:
        return bool(self.flags & SCN_MEM_WRITE)

    @property
    def executable(self) -> bool:
        return bool(self.flags & SCN_MEM_EXECUTE)

    def as_list(self) -> list:
        return [self.name, self.va, self.vsize, self.roff, self.rsize, self.flags]


@dataclass
class ImportFunc:
    name: Optional[str]
    ordinal: Optional[int]
    hint: int
    iat_rva: int


@dataclass
class ImportDesc:
    index: int
    desc_rva: int
    desc_off: int
    raw: bytes
    oft: int
    ts: int
    fwd: int
    name_rva: int
    name_off: Optional[int]
    iat: int
    dll: str
    funcs: list = field(default_factory=list)


class PE:
    def __init__(self, data: bytes | bytearray, label: str = "<buffer>") -> None:
        self.data = data
        self.label = label
        try:
            self._parse()
        except (struct.error, IndexError) as error:
            raise PatchError(f"{label}: truncated or malformed PE ({error})")
        self._reloc_cache: Optional[list] = None
        self._reloc_keys: Optional[list] = None
        self._pdata_cache: Optional[list] = None
        self._pdata_keys: Optional[list] = None

    def _parse(self) -> None:
        data = self.data
        if len(data) < 0x100 or data[:2] != b"MZ":
            raise PatchError(f"{self.label}: not an MZ/PE file")
        self.pe_off = struct.unpack_from("<I", data, 0x3C)[0]
        if self.pe_off + 0x78 > len(data) or data[self.pe_off:self.pe_off + 4] != b"PE\0\0":
            raise PatchError(f"{self.label}: bad PE signature")
        self.fh = self.pe_off + 4
        self.oh = self.fh + 20
        if struct.unpack_from("<H", data, self.oh)[0] != 0x10B:
            raise PatchError(f"{self.label}: not PE32 (ARM32 images are PE32)")
        self.nsec = struct.unpack_from("<H", data, self.fh + 2)[0]
        self.opt_size = struct.unpack_from("<H", data, self.fh + 16)[0]
        self.sec_table = self.oh + self.opt_size
        if self.sec_table + 40 * self.nsec > len(data):
            raise PatchError(f"{self.label}: section table runs past EOF")
        self.sections = [Section(i, self.sec_table + 40 * i,
                                 bytes(data[self.sec_table + 40 * i:self.sec_table + 40 * i + 40]))
                         for i in range(self.nsec)]

    # --- header fields -----------------------------------------------------
    def _spec(self, name: str) -> tuple[int, str]:
        if name in _FILE:
            return self.fh + _FILE[name][0], _FILE[name][1]
        if name in _OPT:
            return self.oh + _OPT[name][0], _OPT[name][1]
        raise KeyError(name)

    def off(self, name: str) -> int:
        return self._spec(name)[0]

    def get(self, name: str):
        off, fmt = self._spec(name)
        value = struct.unpack_from("<" + fmt, self.data, off)
        return value[0] if len(value) == 1 else value

    def dd(self, index: int) -> tuple[int, int]:
        if index >= self.get("num_rva_sizes") or 96 + 8 * (index + 1) > self.opt_size:
            return (0, 0)
        return struct.unpack_from("<2I", self.data, self.dd_off(index))

    def dd_off(self, index: int) -> int:
        return self.oh + 96 + 8 * index

    @property
    def headers_limit(self) -> int:
        """Highest file offset the header area (section table) may grow to."""
        raws = [s.roff for s in self.sections if s.rsize and s.roff]
        limit = self.get("size_of_headers")
        return min([limit] + raws) if raws else limit

    # --- address translation -------------------------------------------------
    def section_by_name(self, name: str) -> Section:
        for section in self.sections:
            if section.name == name:
                return section
        raise PatchError(f"{self.label}: section {name!r} not found")

    def section_of_rva(self, rva: int) -> Optional[Section]:
        for section in self.sections:
            if section.va <= rva < section.va + section.extent:
                return section
        return None

    def rva_to_off(self, rva: int, raw_backed: bool = False) -> Optional[int]:
        section = self.section_of_rva(rva)
        if section is not None:
            delta = rva - section.va
            if section.rsize == 0 or (raw_backed and delta >= section.rsize):
                return None
            return section.roff + delta
        if rva < self.get("size_of_headers"):
            return rva
        return None

    def off_to_rva(self, off: int) -> Optional[int]:
        for section in self.sections:
            if section.roff <= off < section.roff + section.rsize:
                return section.va + off - section.roff
        if off < self.get("size_of_headers"):
            return off
        return None

    def read_rva(self, rva: int, count: int) -> bytes:
        section = self.section_of_rva(rva)
        if section is None:
            if rva + count <= self.get("size_of_headers"):
                return bytes(self.data[rva:rva + count])
            raise PatchError(f"{self.label}: RVA 0x{rva:X} is outside the image")
        delta = rva - section.va
        have = max(0, min(count, section.rsize - delta))
        chunk = bytes(self.data[section.roff + delta:section.roff + delta + have])
        return chunk + b"\0" * (count - have)

    def cstr(self, rva: int, limit: int = 4096) -> bytes:
        off = self.rva_to_off(rva)
        if off is None:
            raise PatchError(f"{self.label}: string RVA 0x{rva:X} is outside the image")
        end = self.data.find(b"\0", off, off + limit)
        if end < 0:
            raise PatchError(f"{self.label}: unterminated string at RVA 0x{rva:X}")
        return bytes(self.data[off:end])

    def u32_rva(self, rva: int) -> int:
        return struct.unpack("<I", self.read_rva(rva, 4))[0]

    # --- tables ----------------------------------------------------------------
    def imports(self) -> list[ImportDesc]:
        rva, _ = self.dd(IMAGE_DIR_IMPORT)
        out: list[ImportDesc] = []
        if not rva:
            return out
        off = self.rva_to_off(rva)
        if off is None:
            raise PatchError(f"{self.label}: import directory RVA 0x{rva:X} outside the image")
        index = 0
        while True:
            at = off + 20 * index
            if at + 20 > len(self.data):
                raise PatchError(f"{self.label}: import directory runs past EOF")
            raw = bytes(self.data[at:at + 20])
            oft, ts, fwd, name, ft = struct.unpack("<5I", raw)
            if not any((oft, ts, fwd, name, ft)):
                break
            desc = ImportDesc(index, rva + 20 * index, at, raw, oft, ts, fwd, name,
                              self.rva_to_off(name), ft, self.cstr(name).decode("latin-1"))
            thunk = oft or ft
            if thunk:
                for j in range(65536):
                    entry = self.u32_rva(thunk + 4 * j)
                    if entry == 0:
                        break
                    if entry & 0x80000000:
                        desc.funcs.append(ImportFunc(None, entry & 0xFFFF, 0, ft + 4 * j))
                    else:
                        hint = struct.unpack("<H", self.read_rva(entry, 2))[0]
                        desc.funcs.append(ImportFunc(self.cstr(entry + 2).decode("latin-1"),
                                                     None, hint, ft + 4 * j))
            out.append(desc)
            index += 1
        return out

    def delay_imports(self) -> list[tuple[str, list[ImportFunc]]]:
        rva, _ = self.dd(IMAGE_DIR_DELAY_IMPORT)
        out = []
        if not rva:
            return out
        for index in range(256):
            attrs, name, hmod, iat, int_, bound, unload, ts = struct.unpack(
                "<8I", self.read_rva(rva + 32 * index, 32))
            if not name:
                break
            if not attrs & 1:  # pre-VC6 absolute form; not seen on ARM
                break
            funcs = []
            for j in range(65536):
                entry = self.u32_rva(int_ + 4 * j)
                if entry == 0:
                    break
                if entry & 0x80000000:
                    funcs.append(ImportFunc(None, entry & 0xFFFF, 0, iat + 4 * j))
                else:
                    funcs.append(ImportFunc(self.cstr(entry + 2).decode("latin-1"), None,
                                            0, iat + 4 * j))
            out.append((self.cstr(name).decode("latin-1"), funcs))
        return out

    def reloc_entries(self) -> list[tuple[int, int]]:
        if self._reloc_cache is not None:
            return self._reloc_cache
        rva, size = self.dd(IMAGE_DIR_BASERELOC)
        entries: list[tuple[int, int]] = []
        off = self.rva_to_off(rva) if rva else None
        if off is not None:
            pos, end = off, min(off + size, len(self.data))
            while pos + 8 <= end:
                page, block = struct.unpack_from("<2I", self.data, pos)
                if block < 8 or pos + block > end:
                    break
                for i in range((block - 8) // 2):
                    value = struct.unpack_from("<H", self.data, pos + 8 + 2 * i)[0]
                    if value >> 12:
                        entries.append((page + (value & 0xFFF), value >> 12))
                pos += block
        entries.sort()
        self._reloc_cache = entries
        return entries

    def relocs_overlapping(self, rva: int, count: int) -> list[tuple[int, int]]:
        entries = self.reloc_entries()
        if self._reloc_keys is None:
            self._reloc_keys = [e[0] for e in entries]
        start = bisect.bisect_left(self._reloc_keys, rva - 7)
        hits = []
        for position, kind in entries[start:]:
            if position >= rva + count:
                break
            if position + _RELOC_WIDTH.get(kind, 4) > rva:
                hits.append((position, kind))
        return hits

    def pdata_functions(self) -> list[tuple[int, int]]:
        if self._pdata_cache is not None:
            return self._pdata_cache
        rva, size = self.dd(IMAGE_DIR_EXCEPTION)
        out: list[tuple[int, int]] = []
        off = self.rva_to_off(rva) if rva else None
        if off is not None:
            for i in range(min(size, len(self.data) - off) // 8):
                begin, unwind = struct.unpack_from("<2I", self.data, off + 8 * i)
                if not begin:
                    continue
                flag = unwind & 3
                if flag == 0:
                    try:
                        length = (self.u32_rva(unwind) & 0x3FFFF) * 2
                    except PatchError:
                        continue
                elif flag in (1, 2):
                    length = ((unwind >> 2) & 0x7FF) * 2
                else:
                    continue
                out.append((begin & ~1, length))
        out.sort()
        self._pdata_cache = out
        return out

    def function_covering(self, rva: int, count: int = 1) -> Optional[tuple[int, int]]:
        """The .pdata function overlapping [rva, rva+count), if any."""
        funcs = self.pdata_functions()
        if self._pdata_keys is None:
            self._pdata_keys = [f[0] for f in funcs]
        i = bisect.bisect_right(self._pdata_keys, rva + count - 1) - 1
        for j in (i, i - 1):          # functions do not overlap: the last two candidates suffice
            if j >= 0:
                begin, length = funcs[j]
                if begin < rva + count and rva < begin + length:
                    return funcs[j]
        return None

    def describe(self) -> dict:
        return {
            "machine": self.get("machine"), "image_base": self.get("image_base"),
            "size_of_image": self.get("size_of_image"), "entry": self.get("entry"),
            "number_of_sections": self.nsec,
            "dll_characteristics": self.get("dll_characteristics"),
            "checksum": self.get("checksum"),
            "sections": [s.as_list() for s in self.sections],
        }


def pe_checksum(data: bytes | bytearray) -> int:
    """Standard PE checksum (same as imagehlp!CheckSumMappedFile)."""
    pe = PE(data)
    ck = pe.off("checksum")
    n = len(data)
    even = n - (n & 1)
    words = array.array("H")
    words.frombytes(bytes(data[:even]))
    if sys.byteorder != "little":
        words.byteswap()
    total = sum(words) - words[ck // 2] - words[ck // 2 + 1]
    if n & 1:
        total += data[-1]
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (total + n) & 0xFFFFFFFF


# ---------------------------------------------------------------------------
# Thumb-2 helpers: branch encode/decode, a tiny length-aware decoder (probe)
# ---------------------------------------------------------------------------

def thumb_len(hw: int) -> int:
    return 4 if (hw & 0xF800) in (0xE800, 0xF000, 0xF800) else 2


def _sext(value: int, bits: int) -> int:
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def decode_branch(code: bytes, site_rva: int) -> Optional[tuple[str, int]]:
    """Decode a 32-bit Thumb-2 branch at site_rva -> (kind, target RVA).

    kinds: 'bl' (BL T1), 'b.w' (B T4), 'blx' (BLX imm T2, ARM-state target),
    'b.cond' (B<c>.W T3).  None if the bytes are not a branch."""
    if len(code) < 4:
        return None
    hw1, hw2 = struct.unpack("<2H", code[:4])
    if hw1 & 0xF800 != 0xF000 or not hw2 & 0x8000:
        return None
    s = (hw1 >> 10) & 1
    j1 = (hw2 >> 13) & 1
    j2 = (hw2 >> 11) & 1
    if hw2 & 0xD000 == 0x8000:
        value = (s << 20) | (j2 << 19) | (j1 << 18) | ((hw1 & 0x3F) << 12) | ((hw2 & 0x7FF) << 1)
        return ("b.cond", site_rva + 4 + _sext(value, 21))
    i1 = (~(j1 ^ s)) & 1
    i2 = (~(j2 ^ s)) & 1
    value = (s << 24) | (i1 << 23) | (i2 << 22) | ((hw1 & 0x3FF) << 12) | ((hw2 & 0x7FF) << 1)
    disp = _sext(value, 25)
    form = hw2 & 0xD000
    if form == 0xD000:
        return ("bl", site_rva + 4 + disp)
    if form == 0x9000:
        return ("b.w", site_rva + 4 + disp)
    if form == 0xC000 and not hw2 & 1:
        return ("blx", ((site_rva + 4) & ~3) + disp)
    return None


def encode_branch(kind: str, site_rva: int, target_rva: int) -> bytes:
    """Encode BL (T1) or B.W (T4) at site_rva -> target_rva (Thumb state, +-16 MiB)."""
    if kind not in ("bl", "b.w"):
        raise PatchError(f"cannot encode branch kind {kind!r} (only bl / b.w)")
    disp = target_rva - (site_rva + 4)
    if disp & 1:
        raise PatchError(f"branch displacement {disp} is odd (target 0x{target_rva:X} not halfword aligned)")
    if not -(1 << 24) <= disp < (1 << 24):
        raise PatchError(f"branch displacement {disp:+d} is outside +-16 MiB "
                         f"(site 0x{site_rva:X} -> 0x{target_rva:X})")
    value = disp & 0x1FFFFFF
    s = (value >> 24) & 1
    i1 = (value >> 23) & 1
    i2 = (value >> 22) & 1
    j1 = (~(i1 ^ s)) & 1
    j2 = (~(i2 ^ s)) & 1
    first = 0xF000 | (s << 10) | ((value >> 12) & 0x3FF)
    second = (0xD000 if kind == "bl" else 0x9000) | (j1 << 13) | (j2 << 11) | ((value >> 1) & 0x7FF)
    return struct.pack("<2H", first, second)


# Trampoline templates.  Position independent: the literal is a *displacement*
# (IAT cell RVA - (trampoline RVA + pc_bias)), no relocation, valid at any base.
TRAMPOLINES = {
    "pic_ldr_ip": {
        "code": bytes.fromhex(
            "dff808c0"   # ldr.w ip, [pc, #8]    ; ip = literal at +12 (trampoline is 4-aligned)
            "fc44"       # add   ip, pc          ; pc = tramp+8 -> ip = IAT cell address
            "dcf800c0"   # ldr.w ip, [ip]        ; ip = export address (Thumb bit set by the loader)
            "6047"),     # bx    ip              ; tail jump, LR untouched
        "length": 16, "pc_bias": 8, "literal_offset": 12, "align": 4,
        "asm": ["ldr.w ip, [pc, #8]", "add ip, pc", "ldr.w ip, [ip]", "bx ip"],
    },
}


def build_trampoline(kind: str, tramp_rva: int, cell_rva: int) -> bytes:
    t = TRAMPOLINES[kind]
    literal = (cell_rva - (tramp_rva + t["pc_bias"])) & 0xFFFFFFFF
    return t["code"] + struct.pack("<I", literal)


@dataclass
class Ins:
    rva: int
    size: int
    hw1: int
    hw2: int
    op: str = "other"
    target: Optional[int] = None
    rd: Optional[int] = None
    rn: Optional[int] = None
    imm: Optional[int] = None
    addr: Optional[int] = None   # literal pool address for ldr_lit


def decode_ins(pe: PE, rva: int) -> Ins:
    raw = pe.read_rva(rva, 4)
    hw1, hw2 = struct.unpack("<2H", raw)
    size = thumb_len(hw1)
    ins = Ins(rva, size, hw1, hw2 if size == 4 else 0)
    if size == 4:
        branch = decode_branch(raw, rva)
        if branch:
            ins.op, ins.target = branch
        elif hw1 & 0xFBF0 == 0xF240 and not hw2 & 0x8000:
            ins.op, ins.rd = "movw", (hw2 >> 8) & 0xF
            ins.imm = ((hw1 & 0xF) << 12) | (((hw1 >> 10) & 1) << 11) | (((hw2 >> 12) & 7) << 8) | (hw2 & 0xFF)
        elif hw1 & 0xFBF0 == 0xF2C0 and not hw2 & 0x8000:
            ins.op, ins.rd = "movt", (hw2 >> 8) & 0xF
            ins.imm = ((hw1 & 0xF) << 12) | (((hw1 >> 10) & 1) << 11) | (((hw2 >> 12) & 7) << 8) | (hw2 & 0xFF)
        elif hw1 & 0xFFF0 == 0xF8D0:
            ins.op, ins.rn, ins.rd, ins.imm = "ldr_imm_w", hw1 & 0xF, (hw2 >> 12) & 0xF, hw2 & 0xFFF
        return ins
    if hw1 & 0xF800 == 0x4800:
        ins.op, ins.rd, ins.imm = "ldr_lit", (hw1 >> 8) & 7, (hw1 & 0xFF) * 4
        ins.addr = ((rva + 4) & ~3) + ins.imm
    elif hw1 & 0xF800 == 0x2000:
        ins.op, ins.rd, ins.imm = "movs_imm", (hw1 >> 8) & 7, hw1 & 0xFF
    elif hw1 & 0xFF00 == 0x4600:
        ins.op, ins.rd, ins.rn = "mov", ((hw1 >> 4) & 8) | (hw1 & 7), (hw1 >> 3) & 0xF
    elif hw1 & 0xF800 == 0x6800:
        ins.op, ins.rd, ins.rn, ins.imm = "ldr_imm", hw1 & 7, (hw1 >> 3) & 7, ((hw1 >> 6) & 0x1F) * 4
    elif hw1 & 0xF800 == 0x6000:
        ins.op, ins.rd, ins.rn, ins.imm = "str_imm", hw1 & 7, (hw1 >> 3) & 7, ((hw1 >> 6) & 0x1F) * 4
    elif hw1 & 0xFF87 == 0x4700:
        ins.op, ins.rn = "bx", (hw1 >> 3) & 0xF
    elif hw1 & 0xFF87 == 0x4780:
        ins.op, ins.rn = "blx_reg", (hw1 >> 3) & 0xF
    elif hw1 & 0xF800 == 0xE000:
        ins.op, ins.target = "b.n", rva + 4 + _sext(hw1 & 0x7FF, 11) * 2
    elif hw1 & 0xF000 == 0xD000 and (hw1 >> 8) & 0xF < 0xE:
        ins.op, ins.target = "b.cond", rva + 4 + _sext(hw1 & 0xFF, 8) * 2
    elif hw1 & 0xF500 == 0xB100:
        ins.op = "cbz"
        ins.target = rva + 4 + ((((hw1 >> 9) & 1) << 6) | (((hw1 >> 3) & 0x1F) << 1))
    elif hw1 & 0xFF00 == 0xBD00:
        ins.op = "pop_pc"
    return ins


def sweep(pe: PE, start: int, end: int):
    rva = start & ~1
    while rva < end:
        ins = decode_ins(pe, rva)
        yield ins
        rva += ins.size


def disasm_text(pe: PE, rva: int, size: int) -> str:
    """Capstone text of one instruction when capstone is installed, else raw halfwords."""
    raw = pe.read_rva(rva, size)
    if _capstone is not None:
        md = _capstone.Cs(_capstone.CS_ARCH_ARM, _capstone.CS_MODE_THUMB)
        for ins in md.disasm(raw, rva):
            return f"{ins.mnemonic} {ins.op_str}".strip()
    return raw.hex()


def thunk_import_target(pe: PE, rva: int, cells: dict[int, str]) -> Optional[str]:
    """If the code at rva is `movw ip,lo; movt ip,hi; ldr.w pc,[ip]` (or ldr ip + bx ip),
    return the import name behind the IAT cell it jumps through."""
    try:
        a, b, c = decode_ins(pe, rva), None, None
        if a.op != "movw":
            return None
        b = decode_ins(pe, rva + 4)
        if b.op != "movt" or b.rd != a.rd:
            return None
        c = decode_ins(pe, rva + 8)
        if c.op != "ldr_imm_w" or c.rn != a.rd or c.imm != 0:
            return None
        value = (b.imm << 16) | a.imm
        return cells.get(value - pe.get("image_base"))
    except PatchError:
        return None


# ---------------------------------------------------------------------------
# PROFILES (data).  Everything version specific lives here, nothing in the engine.
# ---------------------------------------------------------------------------
# exe.input            what the stock exe must look like (all checked; sha256/size/sections
#                      can be bypassed together with --allow-hash-mismatch, the rest cannot)
# exe.byte_patches     [{id, rva|file_offset, old, new, reason, group?, context?}] old bytes verified
# exe.new_section      section appended at EOF (characteristics explicit, W+X refused)
# exe.import_add       dll + exports added to the import directory (IAT cells in the new section)
# exe.bootstrap        call_site (BL/B.W rewritten) + trampoline (PIC thunk in .text padding)
# exe.expected_outputs known-good SHA-256 of the output for given option variants
# companions           DLLs patched next to the exe (APPCONTAINER cleared, renames, byte patches)

PLACEHOLDER = "placeholder"
COMPLETE = "complete"

PROFILE_0104 = {
    "schema": 1,
    "name": "v0_10_4",
    "status": COMPLETE,
    "title": "Minecraft PE 0.10.4.40 (Windows Phone 8.1 XAML app, ARM32 Thumb-2)",
    "source": "arm32/v0_10_4/facts/pe_patch_plan.json (patch_profile, verified) + RECON_SUMMARY.md "
              "sections 4 and 5 (FL 9_1 profile byte); friend patcher NOT used (its .d3imp is RWX)",
    "identify": {
        "exe_names": ["MCPE.Host.WindowsPhone.exe"],
        "patched_exe_names": ["Win32Craft.0.10.4-arm32.exe"],
        "manifest": {"name": "Microsoft.Minecraft", "version": "0.10.4.40", "arch": "arm",
                     "executable": "MCPE.Host.WindowsPhone.exe",
                     "dependency": "Microsoft.VCLibs.120.00.Phone"},
        "class_markers": ["Windows.UI.Xaml.Application"],
    },
    "exe": {
        "file": "MCPE.Host.WindowsPhone.exe",
        "output_name": "Win32Craft.0.10.4-arm32.exe",
        "input": {
            "sha256": "6c0a8adce0141323b8a43937043ffd0806dea71e9d11b74f614b39a2046b29f3",
            "size": 14759424, "machine": 0x1C4, "number_of_sections": 6,
            "file_alignment": 0x200, "section_alignment": 0x1000, "image_base": 0x400000,
            "size_of_image": 0xE6E000, "entry": 0x20095D, "characteristics": 0x0122,
            "os_version": [6, 2], "subsystem_version": [6, 2], "dll_characteristics": 0x9140,
            "checksum": 0, "import_directory": [0x33AC88, 0x1F4], "import_descriptors": 24,
            "sections": [
                [".text", 0x1000, 0x209364, 0x400, 0x209400, 0x60000020],
                [".rdata", 0x20B000, 0x1339A0, 0x209800, 0x133A00, 0x40000040],
                [".data", 0x33F000, 0xAF52D8, 0x33D200, 0xA9E800, 0xC0000040],
                [".pdata", 0xE35000, 0x17C58, 0xDDBA00, 0x17E00, 0x40000040],
                [".tls", 0xE4D000, 0x9, 0xDF3800, 0x200, 0xC0000040],
                [".reloc", 0xE4E000, 0x1FB10, 0xDF3A00, 0x1FC00, 0x42000040],
            ],
        },
        "dll_characteristics": {"expect": 0x9140, "clear": 0x1000},
        "renames": [],
        "redirects": {"required": [], "optional": []},
        "byte_patches": [
            {"id": "fl91.profile_string", "group": "fl91", "rva": 0x2351F9, "old": "33", "new": "31",
             "context": {"rva": 0x2351F0, "hex": "5f6c6576656c5f395f3300"},
             "reason": "shader profile suffix _level_9_3 -> _level_9_1 (getShaderModelSuffix "
                       "throws for 9_1 otherwise); string has exactly one xref"},
        ],
        "new_section": {
            "name": ".d3imp", "characteristics": 0xC0000040,   # initialized data, R+W, NOT executable
            "expect_rva": 0xE6E000, "expect_raw_offset": 0xE13600, "expect_raw_size": 0x400,
            "expect_vsize": 0x23C, "expect_size_of_image": 0xE6F000,
        },
        "import_add": {"dll": "win32craft.dll", "exports": ["Win32Bootstrap"]},
        "bootstrap": {
            "export": "Win32Bootstrap",
            "call_site": {"rva": 0x200A72, "kind": "bl", "old": "03f043fa", "old_target": 0x203EFC,
                          "new": "09f077fc"},
            "trampoline": {"kind": "pic_ldr_ip", "section": ".text", "rva": 0x20A364, "slack": 0x9C,
                           "expect_literal": 0xC63EC8},
        },
        "checksum": "keep",
        "base_relocs": "keep",
        "expected_outputs": [
            {"variant": "plan table only (no FL 9_1 byte)", "skip": ["fl91.profile_string"],
             "sha256": "453ce59a2100dd7def909ac4871fd58308ec0052572ec73af07c9ac50b15db31"},
            {"variant": "default (with FL 9_1 byte)", "skip": [], "sha256": "cb44c09a1af48c266570dfe1b4764b803c0cdc89d0e3327add756da9aeea78e2"},
        ],
    },
    "companions": [],   # filled below (three VC120 Phone DLLs)
    "companion_sources": {
        "appx": {
            "identity": "Microsoft.VCLibs.120.00.Phone", "version": "12.0.30113.0", "arch": "arm",
            "search": ["D:/repo/armdeps/Microsoft.VCLibs.120.00.Phone_12.0.30113.0_arm__8wekyb3d8bbwe.Appx"],
            "env": "WIN32CRAFT_VCLIBS120_APPX",
        },
    },
    "ship_notes": [
        "win32craft.dll (shared host, exports Win32Bootstrap) next to the exe",
        "game data\\ next to the exe; ASCII-only install path",
        "Windows RT 8.1: UCRT overlay (dist-arm32/win81-runtime) for win32craft.dll; run loadcheck first",
    ],
}

for _name, _orig, _patched, _size, _off in (
    ("msvcr120_app.dll", "778800c2bd73d7aa8a3c1c90c3a7062130114b2b127e07c9a0d2549b419d765f",
     "c4cbf6e89da092aa33828a66a61b74ca06af5c739b053b1cacb4cf7f9728b2b5", 611768, 0x14E),
    ("msvcp120_app.dll", "29e62fd2baee67acecfdbefa226a88354215b46bb767c0962f7ef922d2f15074",
     "b82dd0fd70dd53a83cb6f72376c7f95123a9aeacac4c72e351863f22cbcf942c", 545208, 0x14E),
    ("vccorlib120_app.dll", "66170fed91616007332921762e848159af293847d5174dd1d60b26ad3dade3ac",
     "002d1f3999cb87342aad42f3dca8c6527c862029bc845fe1ed83eeee208db668", 307128, 0x15E),
):
    PROFILE_0104["companions"].append({
        "name": _name, "required": True, "kind": "vc120_phone",
        "size": _size, "sha256_original": _orig, "sha256_patched": _patched,
        "dll_characteristics": {"offset": _off, "expect": 0x1140, "clear": 0x1000},
        "renames": [], "byte_patches": [], "checksum": "keep",
        "appx_member": _name,
        "provenance": "Microsoft.VCLibs.120.00.Phone 12.0.30113.0 (framework package, not inside the game appx)",
    })

PROFILE_REDCON = {
    "schema": 1,
    "name": "redcon",
    "status": COMPLETE,
    "title": "REDCON 1 1.3.0.0 by Hexage (Windows Phone 8.1 C++/CX app, code-built SwapChainPanel, ARM32 Thumb-2)",
    "source": "games_recon/redcon_redcon-reuse.md F3 (stock values measured, output layout predicted read-only) + "
              "redcon_redcon-startup.md (call site 0x13A55A); same take-over as v0_10_4 (WinMain thunk bytes identical)",
    "identify": {
        "exe_names": ["Redcon.exe"],
        "patched_exe_names": ["Win32Craft.Redcon-arm32.exe"],
        "manifest": {"name": "Hexage.REDCON1", "version": "1.3.0.0", "arch": "arm",
                     "executable": "Redcon.exe", "dependency": "Microsoft.VCLibs.120.00.Phone"},
        "class_markers": ["Windows.UI.Xaml.Application"],
    },
    "exe": {
        "file": "Redcon.exe",
        "output_name": "Win32Craft.Redcon-arm32.exe",
        "input": {
            "sha256": "3aca126692e56183e2e70eb35a0b66018ee14ccef0c42c853e056bb4d17b5f74",
            "size": 1941504, "machine": 0x1C4, "number_of_sections": 5,
            "file_alignment": 0x200, "section_alignment": 0x1000, "image_base": 0x400000,
            "size_of_image": 0x1E0000, "entry": 0x13A445, "characteristics": 0x0122,
            "os_version": [6, 2], "subsystem_version": [6, 2], "dll_characteristics": 0x9140,
            "checksum": 0, "import_directory": [0x1B816C, 0x1A4], "import_descriptors": 20,
            "sections": [
                [".text", 0x1000, 0x1404B0, 0x400, 0x140600, 0x60000020],
                [".rdata", 0x142000, 0x781F8, 0x140A00, 0x78200, 0x40000040],
                [".data", 0x1BB000, 0xDE1C, 0x1B8C00, 0xBA00, 0xC0000040],
                [".pdata", 0x1C9000, 0xC3E8, 0x1C4600, 0xC400, 0x40000040],
                [".reloc", 0x1D6000, 0x94B0, 0x1D0A00, 0x9600, 0x42000040],
            ],
        },
        "dll_characteristics": {"expect": 0x9140, "clear": 0x1000},
        "renames": [],
        "redirects": {"required": [], "optional": []},
        "byte_patches": [],   # no shader-profile string: the game's own level list ends in 9_1, shaders are level_9
        "new_section": {
            "name": ".d3imp", "characteristics": 0xC0000040,   # initialized data, R+W, NOT executable
            "expect_rva": 0x1E0000, "expect_raw_offset": 0x1DA000, "expect_raw_size": 0x200,
            "expect_vsize": 0x1EC, "expect_size_of_image": 0x1E1000,
        },
        "import_add": {"dll": "win32craft.dll", "exports": ["Win32Bootstrap"]},
        "bootstrap": {
            "export": "Win32Bootstrap",
            "call_site": {"rva": 0x13A55A, "kind": "bl", "old": "00f0dff9", "old_target": 0x13A91C,
                          "new": "06f0a9ff"},
            "trampoline": {"kind": "pic_ldr_ip", "section": ".text", "rva": 0x1414B0, "slack": 0x150,
                           "expect_literal": 0x9ED2C},
        },
        "checksum": "keep",
        "base_relocs": "keep",
        "expected_outputs": [],   # record after the first device run proves the patched exe
    },
    "companions": [dict(c) for c in PROFILE_0104["companions"]],   # the same three VC120 Phone DLLs
    "companion_sources": PROFILE_0104["companion_sources"],
    "ship_notes": [
        "win32craft.dll (shared host, exports Win32Bootstrap) next to the exe",
        "res\\ (game data) next to the exe; ASCII-only, short install path (the game copies it into a 256-byte buffer)",
        "Windows RT 8.1: UCRT overlay + api-set shims as for v0_10_4; run loadcheck first",
    ],
}

PROFILE_128 = {
    "schema": 1,
    "name": "v1_2_8",
    "status": COMPLETE,
    "title": "Minecraft 1.2.8.0 UWP (CoreWindow app, ARM32 Thumb-2)",
    "source": "tools/1.2.8-arm32-patcher.py (device-proven); byte identity with it is a self-test",
    "identify": {
        "exe_names": ["Minecraft.Windows.exe"],
        "patched_exe_names": ["Win32Craft.1.2.8-arm32.exe"],
        "manifest": {"name": "Microsoft.MinecraftUWP", "version": "1.2.800.0", "arch": "arm",
                     "executable": "Minecraft.Windows.exe", "dependency": "Microsoft.VCLibs.140.00"},
        "class_markers": ["Windows.ApplicationModel.Core.CoreApplication"],
    },
    "exe": {
        "file": "Minecraft.Windows.exe",
        "output_name": "Win32Craft.1.2.8-arm32.exe",
        "input": {
            "sha256": "b01d5f1237d2daad8cc076d7218f0788751b55a9576c779ccdba08eef6fcfcf8",
            "size": 23595520, "machine": 0x1C4, "number_of_sections": 7,
            "file_alignment": 0x200, "section_alignment": 0x1000, "image_base": 0x400000,
            "size_of_image": 0x1730000, "entry": 0xB49B29, "characteristics": 0x0122,
            "os_version": [6, 2], "subsystem_version": [6, 2], "dll_characteristics": 0x9140,
            "checksum": 0, "import_directory": [0x13269A8, 0x3FC], "import_descriptors": 50,
            "sections": [
                [".text", 0x1000, 0xC35E9C, 0x400, 0xC36000, 0x60000020],
                [".rdata", 0xC37000, 0x6F7904, 0xC36400, 0x6F7A00, 0x40000040],
                [".data", 0x132F000, 0x28E580, 0x132DE00, 0x1E3400, 0xC0000040],
                [".pdata", 0x15BE000, 0xB3250, 0x1511200, 0xB3400, 0x40000040],
                [".tls", 0x1672000, 0x9, 0x15C4600, 0x200, 0xC0000040],
                [".rsrc", 0x1673000, 0x2E8, 0x15C4800, 0x400, 0x40000040],
                [".reloc", 0x1674000, 0xBBD28, 0x15C4C00, 0xBBE00, 0x42000040],
            ],
        },
        "dll_characteristics": {"expect": 0x9140, "clear": 0x1000},
        "renames": [
            {"id": "import.rename.vccorlib", "from": "vccorlib140_app.DLL", "to": "vccorlib140.DLL"},
            {"id": "import.rename.msvcp", "from": "MSVCP140_APP.dll", "to": "MSVCP140.dll"},
            {"id": "import.rename.concrt", "from": "CONCRT140_APP.dll", "to": "CONCRT140.dll"},
            {"id": "import.rename.vcruntime", "from": "VCRUNTIME140_APP.dll", "to": "VCRUNTIME140.dll"},
        ],
        "redirects": {
            "required": ["xinputuap.dll"],
            "optional": ["api-ms-win-core-com-l1-1-0.dll", "api-ms-win-core-winrt-error-l1-1-0.dll",
                         "api-ms-win-core-winrt-error-l1-1-1.dll", "api-ms-win-core-winrt-l1-1-0.dll",
                         "api-ms-win-core-winrt-string-l1-1-0.dll",
                         "api-ms-win-eventing-provider-l1-1-0.dll"],
        },
        "byte_patches": [],
        "new_section": {
            "name": ".d3imp", "characteristics": 0xC0000040,
            "expect_rva": 0x1730000, "expect_raw_offset": 0x1680A00, "expect_raw_size": 0x600,
            "expect_vsize": 0x444, "expect_size_of_image": 0x1731000,
        },
        "import_add": {"dll": "win32craft.dll", "exports": ["Win32Bootstrap"]},
        "bootstrap": {
            "export": "Win32Bootstrap",
            "call_site": {"rva": 0xB49ACA, "kind": "bl", "old": "00f0abfc", "old_target": 0xB4A424,
                          "new": "edf0e9f9"},
            "trampoline": {"kind": "pic_ldr_ip", "section": ".text", "rva": 0xC36EA0, "slack": 0x160},
        },
        "checksum": "keep",
        "base_relocs": "keep",
        "expected_outputs": [
            {"variant": "default", "skip": [], "redirect_api_sets": False,
             "sha256": "0f30d9cd8474c5959975711b6b3d9c4421dc593d5bf3021b141652dc250fa36f"},
            {"variant": "--redirect-api-sets", "skip": [], "redirect_api_sets": True,
             "sha256": "775a59883a9d09684cc8951ca90aec94006b941c85075b8ef9f9d928d68f322f"},
        ],
    },
    "companions": [
        {
            "name": "fmod_ARM.dll", "required": True, "kind": "fmod",
            "size": 1118720,
            "sha256_original": "5f92f5ac9317c7a5d61ab6b43a2e799ae90eb7eee5c992c85d0325ba1d09fc38",
            "sha256_patched": "cf226ecb83f478a67c6dab205dbd2a5ed0a469ac9fa27f2ed4bda12680d5d1f6",
            "dll_characteristics": {"offset": 0x17E, "expect": 0x1140, "clear": 0x1000},
            "renames": [{"id": "fmod.rename.vcruntime", "from": "VCRUNTIME140_APP.dll",
                         "to": "VCRUNTIME140.dll", "expect_offset": 0xF989A}],
            "byte_patches": [
                {"id": "fmod.skip_winrt_init", "group": "skip_winrt_init", "file_offset": 0x78A4E,
                 "old": "0023bb65", "new": "002486e1",
                 "reason": "skip Windows.ApplicationModel.Package/ApplicationData steps of sub_79300 "
                           "(movs r3,#0; str r3,[r7,#0x58] -> movs r4,#0; b.n 0x79960); fails "
                           "without package identity otherwise (arm32/facts/fmod.json option D)"},
            ],
            "checksum": "keep",
            "structure": {"sections": [".giats"], "imports": ["MMDevAPI.DLL"]},
            "provenance": "Minecraft_1.2.8.0_ARM32\\fmod_ARM.dll",
        },
    ],
    "ship_notes": [
        "desktop VC++ onecore ARM redistributables (vccorlib140/msvcp140/concrt140/vcruntime140.dll) "
        "are NOT patched: tools/make_arm32_package.py copies them from the VS Redist folder "
        "(use --redist-dir to have this tool copy them too)",
    ],
    "redist_files": ["vccorlib140.dll", "msvcp140.dll", "concrt140.dll", "vcruntime140.dll"],
}

# --- SLOT FOR A FUTURE PROFILE ------------------------------------------------
# Copy `arm32-patcher.py profiles --dump v0_10_4`, fill the values (all of them come from reverse
# engineering the new exe with probe + arm32re), set "status": "complete" and either paste the dict
# here or pass it with --profile-file FILE.json.  Nothing in the engine has to change.
PROFILE_1_16_40 = {
    "schema": 1,
    "name": "v1_16_40",
    "status": PLACEHOLDER,
    "title": "Minecraft 1.16.40.02 UWP (ARM32) - PLACEHOLDER, not implemented",
    "source": "arm32/v1_16_40/README_V116.md (RE round 1 paused; no verified facts yet)",
    "identify": {
        "exe_names": ["Minecraft.Windows.exe"], "patched_exe_names": [],
        "size_of_image": 0x2BD9000,   # from README_V116.md (stock); unverified until the exe is probed
        "manifest": {"name": "Microsoft.MinecraftUWP", "version_prefix": "1.16.40",
                     "arch": "arm", "executable": "Minecraft.Windows.exe"},
        "class_markers": ["Windows.ApplicationModel.Core.CoreApplication"],
    },
    "exe": None,
    "companions": [],
    "todo": [
        "exe.input: sha256/size/SizeOfImage/section table of the stock exe (run: probe)",
        "exe.bootstrap.call_site: RVA/old bytes/old callee of the WinMain-style call (probe lists "
        "candidates; confirm with arm32re)",
        "exe.bootstrap.trampoline: 4-aligned zero slot in .text padding (probe lists them)",
        "exe.renames / exe.redirects: *_app.dll -> desktop VC140 names, XINPUTUAP, api sets (host facts)",
        "exe.new_section.expect_*: RVA/raw offset/SizeOfImage the host keys the build on",
        "companions: fmod_ARM.dll (patch points), chakra.dll (absent on RT 8.1: host decision)",
        "expected_outputs: record the SHA-256 once a device run proved the patched exe",
    ],
}

PROFILES: dict[str, dict] = {
    "v0_10_4": PROFILE_0104,
    "redcon": PROFILE_REDCON,
    "v1_2_8": PROFILE_128,
    "v1_16_40": PROFILE_1_16_40,
}


def load_profiles(extra_files: list[str] | None = None) -> dict[str, dict]:
    profiles = copy.deepcopy(PROFILES)
    for path in extra_files or []:
        try:
            loaded = json.loads(Path(path).read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise PatchError(f"cannot load profile file {path}: {error}")
        for item in loaded if isinstance(loaded, list) else [loaded]:
            if "name" not in item:
                raise PatchError(f"{path}: profile without a name")
            profiles[item["name"]] = item
    return profiles


def profile_missing(profile: dict) -> list[str]:
    """Names of the required keys a profile still lacks (placeholder detection)."""
    missing = []
    if profile.get("status") != COMPLETE:
        missing.append("status != complete")
    exe = profile.get("exe")
    if not exe:
        return missing + ["exe"]
    for key in ("file", "output_name", "input", "dll_characteristics"):
        if not exe.get(key):
            missing.append("exe." + key)
    return missing


# ---------------------------------------------------------------------------
# options
# ---------------------------------------------------------------------------

@dataclass
class Options:
    dll_name: Optional[str] = None            # default: profile import_add.dll
    export: Optional[str] = None              # default: profile bootstrap.export
    skip: set = field(default_factory=set)    # edit ids / group names to leave out
    redirect_api_sets: bool = False
    checksum: Optional[str] = None            # keep | recompute (default: profile)
    base_relocs: Optional[str] = None         # keep | drop (default: profile)
    allow_hash_mismatch: bool = False
    allow_unknown_companion: bool = False


# ---------------------------------------------------------------------------
# generic edit recorder
# ---------------------------------------------------------------------------

class Recorder:
    """Applies verified edits to a bytearray and keeps the report."""

    def __init__(self, original: bytes, label: str) -> None:
        self.label = label
        self.orig = bytes(original)
        self.buf = bytearray(original)
        self.pe0 = PE(self.orig, label)
        self.edits: list[dict] = []
        self.ranges: list[tuple[int, int]] = []
        self.warnings: list[str] = []
        self.skipped: list[str] = []

    def edit(self, eid: str, off: int, old: bytes, new: bytes, reason: str,
             group: Optional[str] = None) -> dict:
        old, new = bytes(old), bytes(new)
        if len(old) != len(new):
            raise PatchError(f"{eid}: old/new length mismatch")
        current = bytes(self.buf[off:off + len(old)])
        if current != old:
            raise PatchError(f"{eid}: file+0x{off:X} contains {current.hex()}, expected {old.hex()}")
        self.buf[off:off + len(new)] = new
        rva = self.pe0.off_to_rva(off)
        record = {"id": eid, "file_offset": off, "rva": rva, "size": len(new),
                  "old_hex": old.hex(), "new_hex": new.hex(), "reason": reason}
        if group:
            record["group"] = group
        self.edits.append(record)
        self.ranges.append((off, off + len(new)))
        return record

    def append(self, eid: str, off: int, blob: bytes, reason: str, rva: Optional[int] = None) -> dict:
        if len(self.buf) > off:
            raise PatchError(f"{eid}: append offset 0x{off:X} is inside the file")
        gap = off - len(self.buf)
        start = len(self.buf)
        self.buf.extend(b"\0" * gap)
        self.buf.extend(blob)
        record = {"id": eid, "file_offset": off, "rva": rva, "size": len(blob), "old_hex": "",
                  "new_hex": hexstr(blob, 64), "new_sha256": sha256_hex(blob), "reason": reason,
                  "appended": True}
        self.edits.append(record)
        self.ranges.append((start, off + len(blob)))
        return record

    def skipped_by(self, opts: Options, eid: str, group: Optional[str]) -> bool:
        if eid in opts.skip or (group and group in opts.skip):
            self.skipped.append(eid)
            return True
        return False

    def check_confined(self) -> dict:
        """The output may differ from the input only inside the recorded edits."""
        out, orig = self.buf, self.orig
        merged: list[list[int]] = []
        for start, end in sorted(self.ranges):
            if merged and start <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], end)
            else:
                merged.append([start, end])
        limit = min(len(orig), len(out))
        stray: list[int] = []

        def scan(lo: int, hi: int) -> None:
            if lo < hi and orig[lo:hi] != out[lo:hi]:
                a, b = orig[lo:hi], bytes(out[lo:hi])
                for i in range(len(a)):
                    if a[i] != b[i]:
                        stray.append(lo + i)
                        if len(stray) >= 8:
                            return

        pos, inside = 0, 0
        for start, end in merged:
            scan(pos, min(start, limit))
            hi = min(end, limit)
            inside += sum(1 for i in range(min(start, limit), hi) if orig[i] != out[i])
            pos = max(pos, hi)
        scan(pos, limit)
        end_of_edits = max((r[1] for r in merged), default=0)
        if len(out) != max(len(orig), end_of_edits):
            stray.append(len(out))
        return {"ok": not stray, "changed_bytes_inside_edits": inside,
                "stray_offsets": [hex(x) for x in stray[:8]], "ranges": len(merged)}


# ---------------------------------------------------------------------------
# EXE patcher (generic, driven by profile["exe"])
# ---------------------------------------------------------------------------

class ExePatcher(Recorder):
    def __init__(self, data: bytes, profile: dict, opts: Optional[Options] = None,
                 label: str = "exe") -> None:
        missing = profile_missing(profile)
        if missing:
            raise PatchError(f"profile {profile.get('name')} is not usable (placeholder profile): "
                             f"missing/invalid {', '.join(missing)}")
        super().__init__(data, label)
        self.profile = profile
        self.spec = profile["exe"]
        self.opts = opts or Options()
        self.layout: dict = {}
        self.notes: list[str] = []

    # --- steps ------------------------------------------------------------------
    def check_input(self) -> None:
        pe, exp = self.pe0, self.spec["input"]
        digest = sha256_hex(self.orig)
        if exp.get("sha256") and (digest != exp["sha256"] or len(self.orig) != exp["size"]):
            message = (f"unsupported input SHA-256 {digest} ({len(self.orig)} bytes); expected "
                       f"{exp['sha256']} ({exp['size']} bytes)")
            if not self.opts.allow_hash_mismatch:
                raise PatchError(message)
            self.warnings.append("hash mismatch allowed: " + message)
        simple = {"machine": "machine", "file_alignment": "file_alignment",
                  "section_alignment": "section_alignment", "image_base": "image_base",
                  "size_of_image": "size_of_image", "entry": "entry",
                  "characteristics": "characteristics", "dll_characteristics": "dll_characteristics",
                  "checksum": "checksum", "number_of_sections": "number_of_sections"}
        for key, field_name in simple.items():
            if key in exp and pe.get(field_name) != exp[key]:
                raise PatchError(f"input {key} is {h(pe.get(field_name))}, profile expects {h(exp[key])}")
        for key in ("os_version", "subsystem_version"):
            if key in exp and list(pe.get(key)) != exp[key]:
                raise PatchError(f"input {key} is {pe.get(key)}, profile expects {exp[key]}")
        if exp.get("sections") and not self.opts.allow_hash_mismatch:
            have = [s.as_list() for s in pe.sections]
            if have != exp["sections"]:
                raise PatchError("section table differs from the profile "
                                 f"(first mismatch: {next((a for a, b in zip(have, exp['sections']) if a != b), have[:1])})")
        if "import_directory" in exp and list(pe.dd(IMAGE_DIR_IMPORT)) != exp["import_directory"]:
            raise PatchError(f"import directory {pe.dd(1)} != profile {exp['import_directory']}")
        if "import_descriptors" in exp:
            count = len(pe.imports())
            if count != exp["import_descriptors"]:
                raise PatchError(f"{count} import descriptors, profile expects {exp['import_descriptors']}")
        if pe.get("machine") != MACHINE_ARMNT:
            raise PatchError("not an ARMNT image")
        if len(self.orig) > max(s.roff + s.rsize for s in pe.sections):
            raise PatchError("input has an overlay after the last section (signed or packed?); "
                             "refusing to append a section behind it")
        if pe.dd(IMAGE_DIR_SECURITY)[0]:
            raise PatchError("input carries an Authenticode signature; remove it first")

    def check_not_patched(self) -> None:
        ns = self.spec.get("new_section")
        dll = (self.opts.dll_name or (self.spec.get("import_add") or {}).get("dll", "win32craft.dll")).lower()
        if ns and any(s.name == ns["name"] for s in self.pe0.sections):
            raise PatchError(f"input already has a {ns['name']} section: already patched; use the original exe")
        if ns and self.pe0.get("size_of_image") == ns.get("expect_size_of_image"):
            raise PatchError("input SizeOfImage equals the patched value: already patched")
        if any(d.dll.lower() == dll for d in self.pe0.imports()):
            raise PatchError(f"input already imports {dll}: already patched")

    def edit_dllchars(self) -> None:
        spec = self.spec.get("dll_characteristics")
        if not spec:
            return
        current = self.pe0.get("dll_characteristics")
        if current != spec["expect"]:
            raise PatchError(f"DllCharacteristics 0x{current:04X}, profile expects 0x{spec['expect']:04X}")
        new = (current & ~spec.get("clear", 0)) | spec.get("set", 0)
        self.edit("hdr.dllchars", self.pe0.off("dll_characteristics"), struct.pack("<H", current),
                  struct.pack("<H", new),
                  f"DllCharacteristics 0x{current:04X} -> 0x{new:04X} "
                  f"(APPCONTAINER cleared; OS/subsystem version stay {'.'.join(map(str, self.pe0.get('os_version')))})")

    def edit_renames(self) -> None:
        imports = self.pe0.imports()
        for item in self.spec.get("renames", []):
            if self.skipped_by(self.opts, item["id"], item.get("group")):
                continue
            old, new = item["from"].encode("ascii"), item["to"].encode("ascii")
            if len(new) > len(old):
                raise PatchError(f"{item['id']}: replacement longer than the original name")
            hits = [d for d in imports if d.dll.encode("latin-1") == old]
            expect = item.get("expect_count", 1)
            if len(hits) != expect:
                raise PatchError(f"{item['id']}: {len(hits)} import descriptors named {item['from']}, expected {expect}")
            total = self.orig.count(old)
            if total != item.get("expect_file_count", len(hits)):
                raise PatchError(f"{item['id']}: the name occurs {total}x in the file (other references?)")
            seen = set()
            for desc in hits:
                if desc.name_off in seen:
                    continue
                seen.add(desc.name_off)
                self.edit(item["id"], desc.name_off, old, new.ljust(len(old), b"\0"),
                          f"in-place import rename {item['from']} -> {item['to']}")

    def edit_byte_patches(self) -> None:
        pe = self.pe0
        for patch in self.spec.get("byte_patches", []):
            if self.skipped_by(self.opts, patch["id"], patch.get("group")):
                continue
            old, new = bytes.fromhex(patch["old"]), bytes.fromhex(patch["new"])
            if "rva" in patch:
                off = pe.rva_to_off(patch["rva"], raw_backed=True)
                if off is None:
                    raise PatchError(f"{patch['id']}: RVA 0x{patch['rva']:X} is not file backed")
            else:
                off = patch["file_offset"]
            ctx = patch.get("context")
            if ctx:
                have = pe.read_rva(ctx["rva"], len(bytes.fromhex(ctx["hex"])))
                if have != bytes.fromhex(ctx["hex"]):
                    raise PatchError(f"{patch['id']}: context at RVA 0x{ctx['rva']:X} is {have.hex()}, "
                                     f"expected {ctx['hex']}")
            rva = pe.off_to_rva(off)
            if rva is not None and pe.relocs_overlapping(rva, len(new)) and not patch.get("allow_reloc"):
                raise PatchError(f"{patch['id']}: edit at RVA 0x{rva:X} overlaps a base relocation")
            self.edit(patch["id"], off, old, new, patch["reason"], patch.get("group"))

    def add_import_section(self) -> None:
        spec, pe, opts = self.spec, self.pe0, self.opts
        ns, add = spec.get("new_section"), spec.get("import_add")
        if not ns or not add:
            return
        dll_name = opts.dll_name or add["dll"]
        exports = [opts.export] if opts.export else list(add["exports"])
        boot_export = opts.export or spec["bootstrap"]["export"]
        if boot_export not in exports:
            raise PatchError(f"bootstrap export {boot_export} is not in the export list {exports}")
        flags = ns["characteristics"]
        if flags & SCN_MEM_EXECUTE and flags & SCN_MEM_WRITE:
            raise PatchError(f"refusing a W+X section (characteristics 0x{flags:08X}); "
                             "code goes into .text padding, data into an RW section")
        imports = pe.imports()
        sa, fa = pe.get("section_alignment"), pe.get("file_alignment")
        last = max(pe.sections, key=lambda s: s.va)
        new_rva = align_up(last.va + last.extent, sa)
        new_raw = align_up(len(self.orig), fa)
        for key, value in (("expect_rva", new_rva), ("expect_raw_offset", new_raw)):
            if key in ns and ns[key] != value:
                raise PatchError(f"new section {key[7:]} would be 0x{value:X}, profile expects 0x{ns[key]:X}")

        # redirects (descriptor name repointed to the host dll; ILT/IAT stay)
        req = {n.lower() for n in spec.get("redirects", {}).get("required", [])}
        optional = {n.lower() for n in spec.get("redirects", {}).get("optional", [])}
        present = {d.dll.lower() for d in imports}
        if req - present:
            raise PatchError(f"required redirect import(s) not found: {sorted(req - present)}")
        if opts.redirect_api_sets and optional - present:
            raise PatchError(f"api sets not imported: {sorted(optional - present)}")
        redirect = req | (optional if opts.redirect_api_sets else set())

        n_desc = len(imports) + 2
        blob = bytearray(n_desc * 20)

        def put(data: bytes, alignment: int = 1) -> int:
            blob.extend(b"\0" * ((-len(blob)) % alignment))
            at = len(blob)
            blob.extend(data)
            return at

        name_at = put(dll_name.encode("ascii") + b"\0")
        hint_at = {e: put(b"\0\0" + e.encode("ascii") + b"\0", 2) for e in exports}
        thunks = struct.pack(f"<{len(exports) + 1}I", *([new_rva + hint_at[e] for e in exports] + [0]))
        ilt_at = put(thunks, 4)
        iat_at = put(thunks, 4)
        descriptors = []
        redirected = []
        for desc in imports:
            values = list(struct.unpack("<5I", desc.raw))
            if desc.dll.lower() in redirect:
                values[3] = new_rva + name_at
                redirected.append(desc.dll)
            descriptors.append(struct.pack("<5I", *values))
        descriptors.append(struct.pack("<5I", new_rva + ilt_at, 0, 0, new_rva + name_at, new_rva + iat_at))
        descriptors.append(b"\0" * 20)
        blob[:n_desc * 20] = b"".join(descriptors)
        vsize = len(blob)
        rsize = align_up(vsize, fa)
        blob.extend(b"\0" * (rsize - vsize))
        new_size_of_image = align_up(new_rva + vsize, sa)
        for key, value in (("expect_vsize", vsize), ("expect_raw_size", rsize),
                           ("expect_size_of_image", new_size_of_image)):
            if key in ns and ns[key] != value:
                if key == "expect_size_of_image" or not (opts.dll_name or opts.export):
                    raise PatchError(f"new section {key[7:]} is 0x{value:X}, profile expects 0x{ns[key]:X}")
                self.notes.append(f"{key[7:]} 0x{value:X} differs from the profile (custom dll/export names)")

        hdr_off = pe.sec_table + pe.nsec * 40
        if hdr_off + 40 > pe.headers_limit:
            raise PatchError("no room for an additional section header")
        name8 = ns["name"].encode("ascii").ljust(8, b"\0")
        self.edit("sec.header", hdr_off, b"\0" * 40,
                  struct.pack("<8s6I2HI", name8, vsize, new_rva, rsize, new_raw, 0, 0, 0, 0, flags),
                  f"new section {ns['name']} RVA 0x{new_rva:X} (flags 0x{flags:08X} = "
                  f"{flags_text(flags)}, not executable)")
        self.edit("hdr.numsections", pe.off("number_of_sections"), struct.pack("<H", pe.nsec),
                  struct.pack("<H", pe.nsec + 1), f"NumberOfSections {pe.nsec} -> {pe.nsec + 1}")
        old_init = pe.get("size_of_init_data")
        self.edit("hdr.sizeofinitdata", pe.off("size_of_init_data"), struct.pack("<I", old_init),
                  struct.pack("<I", old_init + rsize), f"SizeOfInitializedData += 0x{rsize:X}")
        self.edit("hdr.sizeofimage", pe.off("size_of_image"), struct.pack("<I", pe.get("size_of_image")),
                  struct.pack("<I", new_size_of_image),
                  f"SizeOfImage 0x{pe.get('size_of_image'):X} -> 0x{new_size_of_image:X} "
                  "(align(new section end); the host identifies the build with it)")
        old_dir = pe.dd(IMAGE_DIR_IMPORT)
        self.edit("hdr.importdir", pe.dd_off(IMAGE_DIR_IMPORT), struct.pack("<2I", *old_dir),
                  struct.pack("<2I", new_rva, n_desc * 20),
                  f"import directory -> ({h(new_rva)}, {h(n_desc * 20)}): {len(imports)} copied "
                  f"descriptors + {dll_name}!{','.join(exports)} + terminator")
        self.append("append." + ns["name"].lstrip("."), new_raw, bytes(blob),
                    f"{ns['name']} section data: rebuilt import directory, name, hints, ILT, IAT",
                    new_rva)
        self.layout = {"rva": new_rva, "vsize": vsize, "raw_offset": new_raw, "raw_size": rsize,
                       "size_of_image": new_size_of_image, "dll": dll_name, "exports": exports,
                       "cells": {e: new_rva + iat_at + 4 * i for i, e in enumerate(exports)},
                       "boot_export": boot_export, "redirected": redirected,
                       "descriptors": n_desc, "sections_after": pe.nsec + 1}

    def edit_bootstrap(self) -> None:
        spec, pe = self.spec, self.pe0
        boot = spec.get("bootstrap")
        if not boot or not self.layout:
            return
        cs, tr = boot["call_site"], boot["trampoline"]
        tmpl = TRAMPOLINES[tr["kind"]]
        text = pe.section_by_name(tr.get("section", ".text"))
        if not text.executable:
            raise PatchError(f"trampoline section {text.name} is not executable")
        rva, length = tr["rva"], tmpl["length"]
        if rva % tmpl["align"]:
            raise PatchError(f"trampoline RVA 0x{rva:X} is not {tmpl['align']}-aligned")
        if rva < text.va + text.vsize:
            raise PatchError(f"trampoline RVA 0x{rva:X} lies inside the declared {text.name} content "
                             f"(ends 0x{text.va + text.vsize:X})")
        slack = text.va + text.rsize - rva
        if slack < length:
            raise PatchError(f"only {slack} file-backed bytes after the trampoline slot (need {length})")
        if tr.get("slack") is not None and tr["slack"] != slack:
            raise PatchError(f"slack is 0x{slack:X} bytes, profile expects 0x{tr['slack']:X}")
        off = pe.rva_to_off(rva, raw_backed=True)
        if any(self.orig[off:off + slack]):
            raise PatchError("trampoline slot / slack is not all zero")
        if pe.relocs_overlapping(rva, slack):
            raise PatchError("a base relocation points into the trampoline slack")
        if pe.function_covering(rva, length):
            raise PatchError("a .pdata function covers the trampoline slot")
        cell = self.layout["cells"][self.layout["boot_export"]]
        code = build_trampoline(tr["kind"], rva, cell)
        literal = struct.unpack_from("<I", code, tmpl["literal_offset"])[0]
        if tr.get("expect_literal") is not None and tr["expect_literal"] != literal                 and not (self.opts.dll_name or self.opts.export):
            raise PatchError(f"trampoline literal 0x{literal:X}, profile expects 0x{tr['expect_literal']:X}")

        site = cs["rva"]
        site_off = pe.rva_to_off(site, raw_backed=True)
        old = bytes.fromhex(cs["old"])
        decoded = decode_branch(old, site)
        if decoded is None or decoded[0] != cs["kind"]:
            raise PatchError(f"call site bytes {old.hex()} are not a Thumb-2 {cs['kind']}")
        if cs.get("old_target") is not None and decoded[1] != cs["old_target"]:
            raise PatchError(f"call site targets 0x{decoded[1]:X}, profile expects 0x{cs['old_target']:X}")
        new = encode_branch(cs["kind"], site, rva)
        if cs.get("new") and bytes.fromhex(cs["new"]) != new:
            raise PatchError(f"computed call site {new.hex()} differs from the verified {cs['new']}")
        if decode_branch(new, site) != (cs["kind"], rva):
            raise PatchError("branch encode/decode round trip failed")
        if pe.relocs_overlapping(site, 4):
            raise PatchError("a base relocation covers the call site")
        self.edit("code.trampoline", off, b"\0" * length, code,
                  f"PIC tail jump through IAT cell 0x{cell:X} ({self.layout['dll']}!"
                  f"{self.layout['boot_export']}); literal = cell - (tramp+{tmpl['pc_bias']}) = 0x{literal:X}")
        self.edit("code.callsite", site_off, old, new,
                  f"{cs['kind']} 0x{decoded[1]:X} -> {cs['kind']} 0x{rva:X} (trampoline); "
                  f"displacement {rva - (site + 4):+d}")
        wanted = rva + length - text.va
        if wanted > text.vsize:
            if wanted > text.rsize:
                raise PatchError("trampoline does not fit the raw size of the section")
            self.edit("sec.text.virtualsize", text.hdr_off + 8, struct.pack("<I", text.vsize),
                      struct.pack("<I", wanted),
                      f"{text.name} VirtualSize 0x{text.vsize:X} -> 0x{wanted:X} (cover the trampoline)")
        self.layout.update({"trampoline_rva": rva, "trampoline_file_offset": off,
                            "trampoline_slack": slack, "call_site_rva": site,
                            "literal": literal})

    def edit_base_relocs(self) -> None:
        policy = self.opts.base_relocs or self.spec.get("base_relocs", "keep")
        if policy == "keep":
            return
        if policy != "drop":
            raise PatchError(f"unknown base_relocs policy {policy!r}")
        pe = self.pe0
        self.edit("hdr.relocdir", pe.dd_off(IMAGE_DIR_BASERELOC), struct.pack("<2I", *pe.dd(5)),
                  b"\0" * 8, "drop the base relocation directory (image only loads at its preferred base)")
        chars = pe.get("characteristics")
        self.edit("hdr.relocs_stripped", pe.off("characteristics"), struct.pack("<H", chars),
                  struct.pack("<H", chars | FILE_RELOCS_STRIPPED), "IMAGE_FILE_RELOCS_STRIPPED")
        dllc = struct.unpack_from("<H", self.buf, pe.off("dll_characteristics"))[0]
        if dllc & DLLCHAR_DYNAMIC_BASE:
            self.edit("hdr.dynamicbase", pe.off("dll_characteristics"), struct.pack("<H", dllc),
                      struct.pack("<H", dllc & ~DLLCHAR_DYNAMIC_BASE), "clear DYNAMIC_BASE (no relocations)")

    def edit_checksum(self) -> None:
        policy = self.opts.checksum or self.spec.get("checksum", "keep")
        if policy == "keep":
            return
        if policy != "recompute":
            raise PatchError(f"unknown checksum policy {policy!r}")
        value = pe_checksum(self.buf)
        old = struct.unpack_from("<I", self.buf, self.pe0.off("checksum"))[0]
        if old != value:
            self.edit("hdr.checksum", self.pe0.off("checksum"), struct.pack("<I", old),
                      struct.pack("<I", value), "PE header checksum recomputed")

    def expected_output(self, digest: str) -> dict:
        wanted_skip = set(self.skipped)
        info = {"sha256": digest, "known_variant": None, "match": None}
        if self.opts.dll_name or self.opts.export:
            info["note"] = "custom dll/export name: no recorded hash applies"
            return info
        for item in self.spec.get("expected_outputs", []):
            if set(item.get("skip", [])) != wanted_skip:
                continue
            if bool(item.get("redirect_api_sets", False)) != self.opts.redirect_api_sets:
                continue
            if (self.opts.checksum or "keep") != "keep" or self.opts.base_relocs not in (None, "keep"):
                continue
            info["known_variant"] = item["variant"]
            expected = item["sha256"]
            if expected.startswith("@"):
                info["match"] = None
                info["note"] = "variant hash not recorded yet"
            else:
                info["match"] = expected == digest
                info["expected_sha256"] = expected
        return info

    def run(self) -> tuple[bytes, dict]:
        self.check_not_patched()
        self.check_input()
        self.edit_dllchars()
        self.edit_renames()
        self.edit_byte_patches()
        self.add_import_section()
        self.edit_bootstrap()
        self.edit_base_relocs()
        self.edit_checksum()
        confined = self.check_confined()
        if not confined["ok"]:
            raise PatchError(f"internal error: output differs outside the recorded edits at {confined['stray_offsets']}")
        out = bytes(self.buf)
        checks = verify_patched_exe(out, self.profile, self.orig, self.opts, self.layout)
        failed = [c for c in checks if not c["ok"]]
        if failed:
            raise PatchError("post-patch verification failed: " +
                             "; ".join(f"{c['name']}: {c['detail']}" for c in failed))
        digest = sha256_hex(out)
        report = {
            "kind": "exe", "profile": self.profile["name"],
            "input": {"sha256": sha256_hex(self.orig), "size": len(self.orig)},
            "output": {"sha256": digest, "size": len(out), **PE(out).describe()},
            "options": {"dll_name": self.opts.dll_name, "export": self.opts.export,
                        "skip": sorted(self.opts.skip), "redirect_api_sets": self.opts.redirect_api_sets,
                        "checksum": self.opts.checksum or self.spec.get("checksum", "keep")},
            "layout": self.layout, "skipped_edits": self.skipped, "edits": self.edits,
            "confinement": confined, "checks": checks, "expected_output": self.expected_output(digest),
            "warnings": self.warnings + self.notes,
        }
        return out, report


# ---------------------------------------------------------------------------
# verification of a patched exe (works on any output, also on the old patcher's)
# ---------------------------------------------------------------------------

def verify_patched_exe(data: bytes, profile: dict, orig: Optional[bytes] = None,
                       opts: Optional[Options] = None, layout: Optional[dict] = None) -> list[dict]:
    checks: list[dict] = []

    def chk(name: str, ok: bool, detail: str = "") -> bool:
        checks.append({"name": name, "ok": bool(ok), "detail": detail})
        return bool(ok)

    spec = profile["exe"]
    opts = opts or Options()
    try:
        pe = PE(data, "patched exe")
    except PatchError as error:
        chk("parse", False, str(error))
        return checks
    chk("parse", pe.get("machine") == MACHINE_ARMNT, f"machine 0x{pe.get('machine'):X}, {pe.nsec} sections")
    sa, fa = pe.get("section_alignment"), pe.get("file_alignment")

    # sections
    problems = []
    prev_end = pe.get("size_of_headers")
    for s in pe.sections:
        if s.va % sa:
            problems.append(f"{s.name}: VA not aligned")
        if s.va < prev_end:
            problems.append(f"{s.name}: overlaps/not ascending")
        prev_end = s.va + align_up(s.extent, sa)
        if s.rsize and (s.roff % fa or s.roff + s.rsize > len(data)):
            problems.append(f"{s.name}: raw data misaligned or past EOF")
    chk("sections ascending, aligned, file-backed", not problems, "; ".join(problems))
    wx = [s.name for s in pe.sections if s.executable and s.writable]
    chk("no section is both writable and executable", not wx, ", ".join(wx))
    soi = pe.get("size_of_image")
    chk("SizeOfImage multiple of SectionAlignment and covers all sections",
        soi % sa == 0 and soi >= prev_end, f"0x{soi:X} (sections end 0x{prev_end:X})")
    dllc = pe.get("dll_characteristics")
    want = spec.get("dll_characteristics")
    if want:
        chk("APPCONTAINER cleared", not dllc & DLLCHAR_APPCONTAINER, f"DllCharacteristics 0x{dllc:04X}")

    ns, add, boot = spec.get("new_section"), spec.get("import_add"), spec.get("bootstrap")
    if ns and add and boot:
        dll_name = (opts.dll_name or add["dll"]).lower()
        export = opts.export or boot["export"]
        try:
            imports = pe.imports()
        except PatchError as error:
            chk("imports parse", False, str(error))
            return checks
        chk("imports parse", True, f"{len(imports)} descriptors")
        section = next((s for s in pe.sections if s.name == ns["name"]), None)
        chk(f"new section {ns['name']} present, not executable",
            section is not None and not section.executable and not (section.executable and section.writable),
            "missing" if section is None else f"flags 0x{section.flags:08X} ({flags_text(section.flags)})")
        if "expect_size_of_image" in ns and not (opts.dll_name or opts.export):
            chk("SizeOfImage is the host identification constant", soi == ns["expect_size_of_image"],
                f"0x{soi:X}")
        dir_rva, dir_size = pe.dd(IMAGE_DIR_IMPORT)
        chk("import directory size = (descriptors + terminator) * 20", dir_size == (len(imports) + 1) * 20,
            f"size 0x{dir_size:X}, {len(imports)} descriptors")
        hosts = [d for d in imports if d.dll.lower() == dll_name]
        chk(f"{dll_name} is the last import descriptor",
            bool(hosts) and imports[-1].dll.lower() == dll_name, f"{len(hosts)} descriptor(s) named {dll_name}")
        cell_rva = None
        if hosts:
            last = imports[-1]
            names = [f.name for f in last.funcs]
            chk(f"{dll_name} imports {export}", export in names, str(names))
            if export in names:
                func = last.funcs[names.index(export)]
                cell_rva = func.iat_rva
                value = pe.u32_rva(cell_rva)
                chk("IAT cell holds the hint/name RVA (unbound)", pe.cstr(value + 2).decode() == export,
                    f"cell 0x{cell_rva:X} = 0x{value:X}")
                chk("IAT cell lies in the new section",
                    section is not None and section.va <= cell_rva < section.va + section.vsize,
                    f"0x{cell_rva:X}")
        try:
            for d in imports:
                for rva in (d.name_rva, d.oft or d.iat, d.iat):
                    if pe.rva_to_off(rva) is None:
                        raise PatchError(f"{d.dll}: RVA 0x{rva:X} outside the image")
            chk("all descriptor name/ILT/IAT RVAs resolve", True)
        except PatchError as error:
            chk("all descriptor name/ILT/IAT RVAs resolve", False, str(error))

        # call site -> trampoline -> cell
        cs, tr = boot["call_site"], boot["trampoline"]
        tmpl = TRAMPOLINES[tr["kind"]]
        decoded = decode_branch(pe.read_rva(cs["rva"], 4), cs["rva"])
        chk("call site decodes to the trampoline", decoded == (cs["kind"], tr["rva"]),
            f"decoded {decoded}, expected {(cs['kind'], tr['rva'])}")
        tramp = pe.read_rva(tr["rva"], tmpl["length"])
        chk("trampoline code = template", tramp[:tmpl["literal_offset"]] == tmpl["code"], tramp.hex())
        literal = struct.unpack_from("<i", tramp, tmpl["literal_offset"])[0]
        if cell_rva is not None:
            chk("trampoline literal + (tramp + bias) == IAT cell",
                (tr["rva"] + tmpl["pc_bias"] + literal) & 0xFFFFFFFF == cell_rva,
                f"literal {literal:+#x}, cell 0x{cell_rva:X}")
        text = pe.section_by_name(tr.get("section", ".text"))
        chk("trampoline inside executable section (declared VirtualSize)",
            text.executable and text.va <= tr["rva"] and tr["rva"] + tmpl["length"] <= text.va + text.vsize,
            f"{text.name} ends 0x{text.va + text.vsize:X}")
        chk("trampoline 4-aligned", tr["rva"] % tmpl["align"] == 0)
        chk("no base relocation inside trampoline or call site",
            not pe.relocs_overlapping(tr["rva"], tmpl["length"]) and not pe.relocs_overlapping(cs["rva"], 4))
        chk("no .pdata function covers the trampoline", pe.function_covering(tr["rva"], tmpl["length"]) is None)
        if _capstone is not None:
            md = _capstone.Cs(_capstone.CS_ARCH_ARM, _capstone.CS_MODE_THUMB)
            text_ops = [f"{i.mnemonic} {i.op_str}".strip() for i in md.disasm(tramp[:tmpl["literal_offset"]], tr["rva"])]
            chk("trampoline capstone decode", text_ops == tmpl["asm"], "; ".join(text_ops))

    if orig is not None:
        po = PE(orig)
        chk("entry point unchanged", po.get("entry") == pe.get("entry"), f"0x{pe.get('entry'):X}")
        chk("image base unchanged", po.get("image_base") == pe.get("image_base"))
        io, ip = po.imports(), pe.imports()
        same = 0
        diff = []
        for a, b in zip(io, ip):
            if a.raw == b.raw:
                same += 1
            else:
                va, vb = struct.unpack("<5I", a.raw), struct.unpack("<5I", b.raw)
                if va[0] == vb[0] and va[4] == vb[4] and va[1:3] == vb[1:3]:
                    diff.append(a.dll)      # only the name field moved: a redirect
                else:
                    diff.append(a.dll + "!")
        chk("original import descriptors preserved (only redirects may change the name field)",
            len(ip) >= len(io) and not any(d.endswith("!") for d in diff),
            f"{same} identical, redirected: {diff}")
    policy = opts.checksum or spec.get("checksum", "keep")
    if policy == "recompute":
        chk("header checksum matches", pe.get("checksum") == pe_checksum(data), f"0x{pe.get('checksum'):X}")
    elif orig is not None:
        chk("header checksum kept", pe.get("checksum") == PE(orig).get("checksum"), f"0x{pe.get('checksum'):X}")
    return checks


# ---------------------------------------------------------------------------
# companion DLLs
# ---------------------------------------------------------------------------

KNOWN_SYSTEM_DLL = re.compile(
    r"^(api-ms-win-[a-z0-9\-]+|ext-ms-win-[a-z0-9\-]+|kernel32|kernelbase|ntdll|user32|gdi32|advapi32|"
    r"ole32|oleaut32|combase|ws2_32|d3d11|dxgi|d3dcompiler_\d+|xaudio2_\d|ucrtbase|bcrypt|shell32|shlwapi|"
    r"rpcrt4|msvcrt|winmm|version|imm32|ncrypt|crypt32|mf|mfplat|mfreadwrite|xinput\d_\d|"
    r"msvc[pr]\d+(_app)?|vccorlib\d+(_app)?|vcruntime\d+(_app)?|concrt\d+(_app)?|vcomp\d+(_app)?)\.dll$", re.I)


class DllPatcher(Recorder):
    """Patch one companion DLL according to a profile companion spec."""

    def __init__(self, data: bytes, spec: dict, opts: Optional[Options] = None, label: str = "") -> None:
        super().__init__(data, label or spec["name"])
        self.spec = spec
        self.opts = opts or Options()
        self.state = "unknown"

    def classify(self) -> str:
        digest = sha256_hex(self.orig)
        if digest == self.spec.get("sha256_original"):
            return "original"
        if digest == self.spec.get("sha256_patched"):
            return "patched_known"
        return "unknown"

    def run(self) -> tuple[bytes, dict]:
        spec, pe = self.spec, self.pe0
        self.state = self.classify()
        if pe.get("machine") != MACHINE_ARMNT:
            raise PatchError(f"{self.label}: not an ARMNT image")
        if spec.get("size") and self.state != "unknown" and len(self.orig) != spec["size"]:
            raise PatchError(f"{self.label}: size {len(self.orig)} != profile {spec['size']}")
        if self.state == "unknown" and not self.opts.allow_unknown_companion:
            raise PatchError(
                f"{self.label}: SHA-256 {sha256_hex(self.orig)} is neither the original "
                f"({spec.get('sha256_original')}) nor the known patched copy "
                f"({spec.get('sha256_patched')}); use --allow-unknown-companion to patch it anyway")
        dc = spec["dll_characteristics"]
        off = pe.off("dll_characteristics")
        if dc.get("offset") is not None and dc["offset"] != off:
            raise PatchError(f"{self.label}: DllCharacteristics at 0x{off:X}, profile says 0x{dc['offset']:X}")
        current = pe.get("dll_characteristics")
        if self.state == "patched_known":
            self.warnings.append("already patched copy (known SHA-256): left unchanged")
            if current & dc.get("clear", 0):
                raise PatchError(f"{self.label}: known patched copy still has APPCONTAINER set")
        else:
            if current & dc.get("clear", 0) == 0:
                if self.state == "original":
                    raise PatchError(f"{self.label}: original does not have the bit set (0x{current:04X})")
                self.warnings.append(f"APPCONTAINER already clear (0x{current:04X}); only other edits apply")
            elif current != dc["expect"]:
                raise PatchError(f"{self.label}: DllCharacteristics 0x{current:04X}, profile expects 0x{dc['expect']:04X}")
            else:
                new = current & ~dc["clear"] | dc.get("set", 0)
                self.edit("dll.dllchars", off, struct.pack("<H", current), struct.pack("<H", new),
                          f"DllCharacteristics 0x{current:04X} -> 0x{new:04X} (APPCONTAINER cleared)")
            imports = pe.imports()
            for item in spec.get("renames", []):
                if self.skipped_by(self.opts, item["id"], item.get("group")):
                    continue
                old, new_name = item["from"].encode(), item["to"].encode()
                hits = [d for d in imports if d.dll.encode("latin-1") == old]
                if len(hits) != item.get("expect_count", 1) or self.orig.count(old) != len(hits):
                    raise PatchError(f"{item['id']}: import {item['from']} found {len(hits)}x "
                                     f"(file occurrences {self.orig.count(old)})")
                for d in hits:
                    if item.get("expect_offset") is not None and d.name_off != item["expect_offset"]:
                        raise PatchError(f"{item['id']}: name at 0x{d.name_off:X}, profile expects "
                                         f"0x{item['expect_offset']:X}")
                    self.edit(item["id"], d.name_off, old, new_name.ljust(len(old), b"\0"),
                              f"in-place import rename {item['from']} -> {item['to']}")
            for patch in spec.get("byte_patches", []):
                if self.skipped_by(self.opts, patch["id"], patch.get("group")):
                    continue
                at = patch["file_offset"] if "file_offset" in patch else pe.rva_to_off(patch["rva"], True)
                rva = pe.off_to_rva(at)
                new = bytes.fromhex(patch["new"])
                if rva is not None and pe.relocs_overlapping(rva, len(new)) and not patch.get("allow_reloc"):
                    raise PatchError(f"{patch['id']}: overlaps a base relocation")
                self.edit(patch["id"], at, bytes.fromhex(patch["old"]), new, patch["reason"], patch.get("group"))
            policy = self.opts.checksum or spec.get("checksum", "keep")
            if policy == "recompute":
                value = pe_checksum(self.buf)
                old_sum = struct.unpack_from("<I", self.buf, pe.off("checksum"))[0]
                if old_sum != value:
                    self.edit("dll.checksum", pe.off("checksum"), struct.pack("<I", old_sum),
                              struct.pack("<I", value), "PE header checksum recomputed")
        confined = self.check_confined()
        if not confined["ok"]:
            raise PatchError(f"{self.label}: output differs outside the recorded edits")
        out = bytes(self.buf)
        digest = sha256_hex(out)
        checks = verify_companion(out, spec)
        failed = [c for c in checks if not c["ok"]]
        if failed:
            raise PatchError(f"{self.label}: verification failed: " +
                             "; ".join(f"{c['name']}: {c['detail']}" for c in failed))
        expected = spec.get("sha256_patched")
        match = None if not expected or self.skipped or self.opts.checksum == "recompute" else digest == expected
        report = {"kind": "companion", "name": spec["name"], "state_in": self.state,
                  "input": {"sha256": sha256_hex(self.orig), "size": len(self.orig)},
                  "output": {"sha256": digest, "size": len(out), "dll_characteristics": PE(out).get("dll_characteristics")},
                  "expected_patched_sha256": expected, "matches_expected": match,
                  "edits": self.edits, "skipped_edits": self.skipped, "confinement": confined,
                  "checks": checks, "warnings": self.warnings}
        return out, report


def verify_companion(data: bytes, spec: dict) -> list[dict]:
    """Structural checks of a (patched) companion DLL."""
    checks: list[dict] = []

    def chk(name: str, ok: bool, detail: str = "") -> None:
        checks.append({"name": name, "ok": bool(ok), "detail": detail})

    try:
        pe = PE(data, spec["name"])
    except PatchError as error:
        chk("parse", False, str(error))
        return checks
    chk("ARMNT PE32 DLL", pe.get("machine") == MACHINE_ARMNT and bool(pe.get("characteristics") & 0x2000),
        f"machine 0x{pe.get('machine'):X} characteristics 0x{pe.get('characteristics'):X}")
    dllc = pe.get("dll_characteristics")
    chk("APPCONTAINER cleared (desktop loadable)", not dllc & DLLCHAR_APPCONTAINER, f"DllCharacteristics 0x{dllc:04X}")
    wx = [s.name for s in pe.sections if s.executable and s.writable]
    chk("no W+X section", not wx, ",".join(wx))
    names = [s.name for s in pe.sections]
    extra = spec.get("structure", {})
    ordinary = (".text", ".rdata", ".data", ".pdata", ".tls", ".rsrc", ".reloc", ".didat", ".gfids",
                ".idata", ".edata", ".crt", ".bss", ".CRT", "minATL") + tuple(extra.get("sections", []))
    odd = [n for n in names if n not in ordinary]
    chk("only ordinary sections", not odd, f"{names}" + (f" odd: {odd}" if odd else ""))
    try:
        imports = pe.imports()
        unknown = [d.dll for d in imports if not KNOWN_SYSTEM_DLL.match(d.dll)
                   and d.dll.lower() not in [x.lower() for x in extra.get("imports", [])]]
        chk("imports parse and name system/ARM runtime DLLs only", not unknown,
            f"{len(imports)} DLLs" + (f"; unrecognised: {unknown}" if unknown else ""))
    except PatchError as error:
        chk("imports parse", False, str(error))
    chk("export directory present", bool(pe.dd(IMAGE_DIR_EXPORT)[0]))
    chk("base relocations present (ASLR)", bool(pe.dd(IMAGE_DIR_BASERELOC)[0]))
    sec_rva, sec_size = pe.dd(IMAGE_DIR_SECURITY)       # file offset for this directory
    if sec_rva:
        chk("Authenticode blob inside the file (kept, now invalid)", sec_rva + sec_size <= len(data),
            f"offset 0x{sec_rva:X} size 0x{sec_size:X}")
    return checks


# ---------------------------------------------------------------------------
# companion lookup (folder, --companion-dir, VCLibs appx)
# ---------------------------------------------------------------------------

def read_appx_member(appx: Path, member: str) -> Optional[bytes]:
    try:
        with zipfile.ZipFile(appx) as z:
            names = {n.lower(): n for n in z.namelist()}
            if member.lower() in names:
                return z.read(names[member.lower()])
    except (OSError, zipfile.BadZipFile):
        return None
    return None


def appx_identity(appx: Path) -> Optional[dict]:
    raw = read_appx_member(appx, "AppxManifest.xml")
    if raw is None:
        return None
    try:
        root = ET.fromstring(raw)
    except ET.ParseError:
        return None
    for el in root.iter():
        if el.tag.endswith("Identity"):
            return dict(el.attrib)
    return None


def find_companion(profile: dict, spec: dict, game_folder: Optional[Path], dirs: list[Path],
                   appx_files: list[Path]) -> tuple[Optional[bytes], str, list[str]]:
    """Return (bytes, source description, searched places)."""
    searched: list[str] = []
    candidates: list[tuple[str, Any]] = []
    for d in dirs:
        candidates.append((f"dir {d}", d / spec["name"]))
    if game_folder is not None:
        candidates.append((f"game folder {game_folder}", game_folder / spec["name"]))
    for label, path in candidates:
        searched.append(str(path))
        if path.is_file():
            return path.read_bytes(), f"file {path}", searched
    src = (profile.get("companion_sources") or {}).get("appx")
    if src and spec.get("appx_member"):
        paths = list(appx_files)
        if src.get("env") and os.environ.get(src["env"]):
            paths.append(Path(os.environ[src["env"]]))
        paths += [Path(p) for p in src.get("search", [])]
        for path in paths:
            searched.append(f"{path}!{spec['appx_member']}")
            if not path.is_file():
                continue
            ident = appx_identity(path) or {}
            if ident.get("Name") != src["identity"] or ident.get("Version") != src["version"] \
                    or ident.get("ProcessorArchitecture", "").lower() != src["arch"]:
                searched[-1] += f" (identity {ident.get('Name')} {ident.get('Version')} {ident.get('ProcessorArchitecture')} rejected)"
                continue
            data = read_appx_member(path, spec["appx_member"])
            if data is not None:
                return data, f"appx {path} ({ident['Name']} {ident['Version']}) member {spec['appx_member']}", searched
    return None, "", searched


# ---------------------------------------------------------------------------
# identification (detect)
# ---------------------------------------------------------------------------

def parse_manifest(path: Path) -> Optional[dict]:
    try:
        root = ET.fromstring(path.read_bytes())
    except (OSError, ET.ParseError):
        return None
    info: dict = {"dependencies": []}
    for el in root.iter():
        tag = el.tag.rsplit("}", 1)[-1]
        if tag == "Identity" and "name" not in info:
            info.update(name=el.attrib.get("Name"), version=el.attrib.get("Version"),
                        arch=el.attrib.get("ProcessorArchitecture"))
        elif tag == "Application" and "executable" not in info:
            info.update(executable=el.attrib.get("Executable"), entry_point=el.attrib.get("EntryPoint"))
        elif tag == "PackageDependency":
            info["dependencies"].append(f"{el.attrib.get('Name')} {el.attrib.get('MinVersion', '')}".strip())
    return info


def find_strings(data: bytes, text: str) -> dict:
    ascii_hits = data.count(text.encode("ascii"))
    wide_hits = data.count(text.encode("utf-16le"))
    return {"ascii": ascii_hits, "utf16": wide_hits}


def exe_state(pe: PE, data: bytes) -> dict:
    imports = pe.imports()
    host = [d.dll for d in imports if d.dll.lower().startswith("win32craft")]
    wx = [s.name for s in pe.sections if s.executable and s.writable]
    return {
        "has_host_import": host,
        "has_d3imp_section": any(s.name == ".d3imp" for s in pe.sections),
        "appcontainer": bool(pe.get("dll_characteristics") & DLLCHAR_APPCONTAINER),
        "wx_sections": wx,
    }


def score_profile(profile: dict, pe: PE, digest: str, size: int, exe_name: str,
                  manifest: Optional[dict]) -> tuple[int, list[str], str]:
    """Return (score, reasons, state) of how well an exe matches a profile."""
    score, reasons, state = 0, [], "unknown"
    ident = profile.get("identify", {})
    exe = profile.get("exe")
    if exe:
        inp = exe["input"]
        if digest == inp.get("sha256"):
            return 200, ["SHA-256 equals the stock exe of this profile"], "stock"
        for item in exe.get("expected_outputs", []):
            if digest == item["sha256"]:
                return 200, [f"SHA-256 equals this tool's output ({item['variant']})"], "patched-by-arm32-patcher"
        if pe.get("machine") == inp.get("machine"):
            score += 5
        soi = pe.get("size_of_image")
        if soi == inp.get("size_of_image"):
            score += 25
            reasons.append(f"SizeOfImage 0x{soi:X} = stock")
            state = "stock"
        elif exe.get("new_section") and soi == exe["new_section"].get("expect_size_of_image"):
            score += 25
            reasons.append(f"SizeOfImage 0x{soi:X} = patched value")
            state = "patched"
        if [s.as_list() for s in pe.sections][:len(inp.get("sections", []))] == inp.get("sections"):
            score += 20
            reasons.append("original section table identical")
        if pe.get("entry") == inp.get("entry"):
            score += 10
            reasons.append(f"entry 0x{pe.get('entry'):X}")
    elif ident.get("size_of_image") and pe.get("size_of_image") == ident["size_of_image"]:
        score += 25
        reasons.append(f"SizeOfImage 0x{pe.get('size_of_image'):X} (from README, unverified)")
    if exe_name in ident.get("exe_names", []) or exe_name in ident.get("patched_exe_names", []):
        score += 5
        reasons.append(f"file name {exe_name}")
    mf = ident.get("manifest")
    if mf and manifest:
        if manifest.get("name") == mf.get("name"):
            score += 5
            version = manifest.get("version") or ""
            if mf.get("version") and version == mf["version"] or \
                    mf.get("version_prefix") and version.startswith(mf["version_prefix"]):
                score += 20
                reasons.append(f"AppxManifest {manifest.get('name')} {version}")
            else:
                reasons.append(f"AppxManifest name matches but version is {version}")
    return score, reasons, state


def locate_exe(path: Path, profiles: dict) -> tuple[Path, Optional[dict]]:
    if path.is_file():
        manifest = parse_manifest(path.parent / "AppxManifest.xml") if (path.parent / "AppxManifest.xml").is_file() else None
        return path, manifest
    if not path.is_dir():
        raise PatchError(f"{path} does not exist")
    manifest = parse_manifest(path / "AppxManifest.xml") if (path / "AppxManifest.xml").is_file() else None
    names: list[str] = []
    if manifest and manifest.get("executable"):
        names.append(manifest["executable"])
    for p in profiles.values():
        names += p.get("identify", {}).get("exe_names", []) + p.get("identify", {}).get("patched_exe_names", [])
    for name in names:
        if (path / name).is_file():
            return path / name, manifest
    exes = sorted(path.glob("*.exe"))
    if len(exes) == 1:
        return exes[0], manifest
    raise PatchError(f"cannot decide which exe in {path} is the game ({len(exes)} candidates); pass the exe path")


def identify(path: Path, profiles: dict) -> dict:
    exe_path, manifest = locate_exe(path, profiles)
    data = exe_path.read_bytes()
    pe = PE(data, str(exe_path))
    digest = sha256_hex(data)
    scored = []
    for name, profile in profiles.items():
        score, reasons, state = score_profile(profile, pe, digest, len(data), exe_path.name, manifest)
        scored.append((score, name, reasons, state))
    scored.sort(reverse=True)
    best = scored[0]
    confidence = "high" if best[0] >= 70 else "medium" if best[0] >= 45 else "low" if best[0] >= 25 else "none"
    st = exe_state(pe, data)
    state = best[3]
    if st["has_host_import"] or st["has_d3imp_section"]:
        if state != "patched-by-arm32-patcher":
            state = "patched (not by this tool)"
    elif state == "unknown":
        state = "stock?" if confidence != "none" else "unknown"
    profile_name = best[1] if confidence != "none" else None
    return {
        "exe_path": str(exe_path), "sha256": digest, "size": len(data), "manifest": manifest,
        "machine": pe.get("machine"), "image_base": pe.get("image_base"),
        "size_of_image": pe.get("size_of_image"), "entry": pe.get("entry"),
        "dll_characteristics": pe.get("dll_characteristics"),
        "sections": [s.as_list() for s in pe.sections],
        "profile": profile_name, "confidence": confidence, "state": state, "score": best[0],
        "reasons": best[2], "status": profiles[best[1]].get("status") if profile_name else None,
        "ranking": [(s, n) for s, n, _, _ in scored[:3]], "exe_state": st,
    }


def print_identify(info: dict, profiles: dict) -> None:
    say(f"file         : {info['exe_path']}")
    say(f"sha256/size  : {info['sha256']}  {info['size']} bytes")
    machine = "ARMNT (Thumb-2)" if info["machine"] == MACHINE_ARMNT else "NOT ARM32"
    say(f"machine      : 0x{info['machine']:X} {machine}   ImageBase 0x{info['image_base']:X}   "
        f"entry 0x{info['entry']:X}")
    say(f"SizeOfImage  : 0x{info['size_of_image']:X}   DllCharacteristics 0x{info['dll_characteristics']:04X}"
        f" ({'APPCONTAINER' if info['exe_state']['appcontainer'] else 'desktop'})")
    say("sections     : " + "  ".join(f"{s[0]}@{s[1]:X}({flags_text(s[5])})" for s in info["sections"]))
    mf = info["manifest"]
    if mf:
        say(f"AppxManifest : {mf.get('name')} {mf.get('version')} {mf.get('arch')} exe={mf.get('executable')} "
            f"entry={mf.get('entry_point')} deps={mf.get('dependencies')}")
    st = info["exe_state"]
    say(f"state        : {info['state']}" + (f"  (host import {st['has_host_import']})" if st["has_host_import"] else "")
        + (f"  WARNING W+X sections {st['wx_sections']}" if st["wx_sections"] else ""))
    if info["profile"]:
        p = profiles[info["profile"]]
        say(f"profile      : {info['profile']}  [{p.get('status')}]  confidence {info['confidence']} (score {info['score']})")
        for reason in info["reasons"]:
            say(f"               - {reason}")
        if p.get("status") != COMPLETE:
            say("               this profile is a PLACEHOLDER: run `probe` and fill it in; patch is refused")
        elif info["state"].startswith("patched"):
            say("               already patched: patch needs the ORIGINAL exe")
        else:
            say("               apply with: patch --profile auto <game-folder> <out-folder>")
    else:
        say("profile      : none matches (unknown version)")
        say("               run: arm32-patcher.py probe <exe>   (discovery only; a new profile still needs host facts)")


# ---------------------------------------------------------------------------
# probe (unknown versions)
# ---------------------------------------------------------------------------

INTERESTING_IMPORTS = [
    "GetActivationFactoryByPCWSTR", "RoGetActivationFactory", "RoInitialize", "D3DCompile", "D3DReflect",
    "D3D11CreateDevice", "CreateDXGIFactory1", "XAudio2Create", "_CxxThrowException", "??2@YAPAXI@Z",
    "??3@YAXPAX@Z", "__getmainargs", "_initterm", "_initterm_e", "_cexit", "_exit", "exit", "_amsg_exit",
    "__set_app_type", "JsCreateRuntime",
]
CRT_MARKERS = re.compile(r"^(_initterm(_e)?|__getmainargs|__wgetmainargs|_cexit|_exit|exit|_amsg_exit|"
                         r"__set_app_type|_set_app_type|_configthreadlocale|_set_fmode|_get_initial_.*|"
                         r"_register_onexit_function|_crt_atexit|__scrt.*|_exit_app|_cexit_app)$")
STRING_PROBES = ["Windows.UI.Xaml.Application", "Windows.UI.Xaml.Window", "Windows.UI.Xaml.Controls.Page",
                 "Windows.ApplicationModel.Core.CoreApplication", "Windows.UI.Core.CoreWindow",
                 "Windows.Foundation.Collections", "_level_9_1", "_level_9_3", "ps_4_0", "vs_4_0",
                 "ps_5_0", "vs_5_0"]


def import_cells(pe: PE) -> dict[int, str]:
    cells: dict[int, str] = {}
    for d in pe.imports():
        for f in d.funcs:
            cells[f.iat_rva] = f"{d.dll}!{f.name if f.name else '#%d' % f.ordinal}"
    for dll, funcs in pe.delay_imports():
        for f in funcs:
            cells[f.iat_rva] = f"{dll}!{f.name if f.name else '#%d' % f.ordinal} (delay)"
    return cells


def probe_startup(pe: PE, cells: dict[int, str]) -> dict:
    """Follow entry -> (tail b.w) -> CRT main and rank BL call-site candidates."""
    base = pe.get("image_base")
    entry = pe.get("entry") & ~1
    text = next((s for s in pe.sections if s.executable), None)
    result: dict = {"entry": entry, "chain": [], "candidates": [], "crt_calls": []}
    if text is None:
        return result
    in_text = lambda r: text.va <= r < text.va + text.vsize

    def extent(rva: int, fallback: int) -> tuple[int, int]:
        f = pe.function_covering(rva, 1)
        return (f[0], f[0] + f[1]) if f and f[0] <= rva else (rva, rva + fallback)

    cur = entry
    chain = [cur]
    for _ in range(3):
        lo, hi = extent(cur, 0x40)
        ins = list(sweep(pe, lo, hi))
        if hi - lo >= 0x80:
            break
        tails = [i for i in ins if i.op in ("b.w", "b.n") and i.target is not None and not lo <= i.target < hi]
        if not tails:
            break
        cur = tails[-1].target & ~1
        chain.append(cur)
    result["chain"] = chain
    lo, hi = extent(chain[-1], 0x800)
    result["crt_main"] = (lo, hi)
    ins = list(sweep(pe, lo, min(hi, lo + 0x2000)))
    result["crt_main_instructions"] = len(ins)

    regs: dict[int, tuple] = {}
    for k, i in enumerate(ins):
        if i.op == "ldr_lit":
            try:
                regs[i.rd] = ("lit", pe.u32_rva(i.addr))
            except PatchError:
                pass
        elif i.op == "ldr_imm" and i.imm == 0 and regs.get(i.rn, ("",))[0] == "lit":
            cell = regs[i.rn][1] - base
            if cell in cells:
                regs[i.rd] = ("cell", cells[cell])
        elif i.op == "blx_reg" and regs.get(i.rn, ("",))[0] == "cell":
            result["crt_calls"].append((i.rva, regs[i.rn][1]))
        elif i.op in ("bl", "b.w") and i.target is not None:
            name = thunk_import_target(pe, i.target & ~1, cells)
            if name:
                result["crt_calls"].append((i.rva, name))

    candidates = []
    for k, i in enumerate(ins):
        if i.op != "bl" or i.target is None:
            continue
        target = i.target
        thunk = thunk_import_target(pe, target, cells)
        score, why = 0, []
        if thunk or not in_text(target):
            score -= 6
            why.append("callee is an import thunk / outside .text" if thunk else "callee outside .text")
        for j in range(max(0, k - 10), k):
            p = ins[j]
            if p.op == "ldr_lit" and p.rd == 0:
                try:
                    lit = pe.u32_rva(p.addr)
                except PatchError:
                    lit = None
                if lit == base:
                    score += 6
                    why.append("r0 = literal equal to ImageBase (hInstance)")
                    if pe.relocs_overlapping(p.addr, 4):
                        score += 1
                        why.append("literal is relocated")
            elif p.op == "movs_imm" and p.rd == 1 and p.imm == 0:
                score += 2
                why.append("movs r1,#0")
            elif p.op == "movs_imm" and p.rd == 3 and p.imm == 0:
                score += 1
                why.append("movs r3,#0")
        if k + 1 < len(ins):
            nxt = ins[k + 1]
            if (nxt.op == "str_imm" and nxt.rd == 0) or (nxt.op == "mov" and nxt.rn == 0):
                score += 2
                why.append("result (r0) stored right after")
        candidates.append({"site": i.rva, "target": target, "score": score, "why": why,
                           "bytes": pe.read_rva(i.rva, 4).hex(), "import": thunk})
    candidates.sort(key=lambda c: (-c["score"], -c["site"]))
    result["candidates"] = candidates[:6]
    return result


def find_padding(pe: PE, minimum: int = 16) -> list[dict]:
    regions = []
    funcs = pe.pdata_functions()
    for s in pe.sections:
        if not s.executable:
            continue
        tail = align_up(s.va + s.vsize, 4)
        avail = s.va + s.rsize - tail
        if avail >= minimum:
            off = pe.rva_to_off(tail, raw_backed=True)
            zero = off is not None and not any(pe.data[off:off + avail])
            regions.append({"kind": f"{s.name} tail slack (file backed, beyond VirtualSize)", "rva": tail,
                            "file_offset": off, "size": avail, "all_zero": zero,
                            "reloc_free": not pe.relocs_overlapping(tail, avail),
                            "pdata_free": pe.function_covering(tail, avail) is None,
                            "needs_vsize_bump": True})
        gaps = []
        for (b1, l1), (b2, _) in zip(funcs, funcs[1:]):
            if s.va <= b1 < s.va + s.vsize:
                start, end = align_up(b1 + l1, 4), b2
                if end - start >= minimum:
                    off = pe.rva_to_off(start, raw_backed=True)
                    if off is not None and not any(pe.data[off:off + (end - start)]):
                        gaps.append((end - start, start))
        gaps.sort(reverse=True)
        for size, start in gaps[:3]:
            regions.append({"kind": f"{s.name} zero gap between functions", "rva": start,
                            "file_offset": pe.rva_to_off(start, True), "size": size, "all_zero": True,
                            "reloc_free": not pe.relocs_overlapping(start, size), "pdata_free": True,
                            "needs_vsize_bump": False})
    return regions


def cmd_probe(args: argparse.Namespace) -> int:
    profiles = load_profiles(args.profile_file)
    path = Path(args.exe)
    exe_path, manifest = locate_exe(path, profiles)
    data = exe_path.read_bytes()
    pe = PE(data, str(exe_path))
    cells = import_cells(pe)
    info = identify(exe_path, profiles)
    out: dict[str, Any] = {"file": str(exe_path), "sha256": sha256_hex(data)}
    say(f"== probe {exe_path}")
    if pe.get("machine") != MACHINE_ARMNT:
        say(f"WARNING: machine 0x{pe.get('machine'):X} is not ARMNT (0x1C4): this tool only patches ARM32 Thumb-2 images")
    say(f"machine 0x{pe.get('machine'):X}  ImageBase 0x{pe.get('image_base'):X}  SizeOfImage 0x{pe.get('size_of_image'):X}  "
        f"entry 0x{pe.get('entry'):X} ({'Thumb' if pe.get('entry') & 1 else 'ARM!'})  "
        f"DllCharacteristics 0x{pe.get('dll_characteristics'):04X}  checksum 0x{pe.get('checksum'):X}")
    say(f"known profile: {info['profile'] or 'NONE'} ({info['confidence']}, state {info['state']})")
    say("sections:")
    for s in pe.sections:
        say(f"  {s.name:8s} VA 0x{s.va:08X} VS 0x{s.vsize:08X} raw 0x{s.roff:08X} RS 0x{s.rsize:08X} "
            f"{flags_text(s.flags)} 0x{s.flags:08X}")
    say(f"header slots free for new section headers: {(pe.headers_limit - (pe.sec_table + 40 * pe.nsec)) // 40}")
    say(f"overlay after last section: {len(data) - max(s.roff + s.rsize for s in pe.sections)} bytes; "
        f"Authenticode: {'YES' if pe.dd(IMAGE_DIR_SECURITY)[0] else 'no'}; base relocs: {pe.dd(5)[1]} bytes; "
        f".pdata functions: {len(pe.pdata_functions())}")

    imports = pe.imports()
    say(f"imports: {len(imports)} DLLs, {sum(len(d.funcs) for d in imports)} functions")
    app_dlls = [d.dll for d in imports if re.search(r"_app\.dll$|xinputuap|^vccorlib", d.dll, re.I)]
    say("  " + ", ".join(f"{d.dll}({len(d.funcs)})" for d in imports))
    if app_dlls:
        say(f"  AppContainer/UWP-runtime DLLs (need rename or a patched copy next to the exe): {app_dlls}")
    delay = pe.delay_imports()
    if delay:
        say("  delay imports: " + ", ".join(f"{n}({len(f)})" for n, f in delay))
    say("interesting IAT cells:")
    found = 0
    for d in imports:
        for f in d.funcs:
            if f.name and any(f.name == w or w in f.name for w in INTERESTING_IMPORTS if len(w) > 6 or f.name == w):
                say(f"  0x{f.iat_rva:08X}  {d.dll}!{f.name}")
                found += 1
    if not found:
        say("  (none of the watched imports is present)")

    say("app model:")
    markers = {s: find_strings(data, s) for s in STRING_PROBES}
    for s, c in markers.items():
        if c["ascii"] or c["utf16"]:
            say(f"  {s}: ascii {c['ascii']}, utf-16 {c['utf16']}")
    xaml = markers["Windows.UI.Xaml.Application"]["utf16"] + markers["Windows.UI.Xaml.Application"]["ascii"]
    core = markers["Windows.ApplicationModel.Core.CoreApplication"]["utf16"] + \
        markers["Windows.ApplicationModel.Core.CoreApplication"]["ascii"]
    kind = "XAML app (Application::Start; host needs XAML fakes)" if xaml else \
        "CoreWindow app (CoreApplication::Run / IFrameworkView)" if core else "unknown (no class-name string found)"
    say(f"  => {kind}")
    if manifest:
        say(f"  AppxManifest: {manifest.get('name')} {manifest.get('version')} entry={manifest.get('entry_point')} "
            f"deps={manifest.get('dependencies')}")

    say("startup (entry -> CRT main -> WinMain-style call):")
    sp = probe_startup(pe, cells)
    say("  entry chain: " + " -> ".join(f"0x{r:X}" for r in sp["chain"]))
    if "crt_main" in sp:
        say(f"  CRT main 0x{sp['crt_main'][0]:X}..0x{sp['crt_main'][1]:X} ({sp.get('crt_main_instructions')} instructions swept)")
    markers_seen = [(r, n) for r, n in sp["crt_calls"] if CRT_MARKERS.match(n.split('!')[-1].split(' ')[0])]
    if markers_seen:
        say("  CRT calls seen: " + ", ".join(f"0x{r:X} {n}" for r, n in markers_seen[:8]))
    if not sp["candidates"]:
        say("  no BL candidates (is the entry a normal CRT stub?)")
    elif sp["candidates"][0]["score"] < 6:
        say("  WARNING: no candidate shows the hInstance (ImageBase literal) argument setup; this exe may not use the")
        say("           VS CRT startup shape (entry -> cookie init -> tail b.w -> CRT main). Find the call by hand.")
    for n, c in enumerate(sp["candidates"][:5]):
        mark = ""
        exe = (profiles.get(info["profile"]) or {}).get("exe") if info["profile"] else None
        if exe and exe.get("bootstrap") and exe["bootstrap"]["call_site"]["rva"] == c["site"]:
            mark = "   <== equals the profile's call site"
        say(f"  #{n + 1} score {c['score']:+d}  bl at 0x{c['site']:X} -> 0x{c['target']:X}  bytes {c['bytes']}{mark}")
        say(f"       {'; '.join(c['why']) or 'no argument-setup evidence'}")
    out["startup"] = sp

    say("trampoline slots (zero, 4-aligned, >= 16 bytes):")
    pads = find_padding(pe)
    for p in pads:
        say(f"  RVA 0x{p['rva']:X} file 0x{p['file_offset']:X} size 0x{p['size']:X}  {p['kind']}  "
            f"zero={p['all_zero']} reloc-free={p['reloc_free']} pdata-free={p['pdata_free']}"
            + ("  (needs .text VirtualSize bump)" if p["needs_vsize_bump"] else ""))
    if not pads:
        say("  none found")
    out["padding"] = pads
    first_fit = next((p for p in pads if p["all_zero"] and p["reloc_free"] and p["pdata_free"]), None)
    if first_fit and sp["candidates"]:
        c0 = sp["candidates"][0]
        try:
            encode_branch("bl", c0["site"], first_fit["rva"])
            say(f"  BL range to the first slot from candidate #1: OK ({first_fit['rva'] - (c0['site'] + 4):+d} bytes)")
        except PatchError as error:
            say(f"  BL range problem: {error}")

    siblings = []
    folder = exe_path.parent
    for dll in sorted(folder.glob("*.dll")):
        try:
            other = PE(dll.read_bytes(), dll.name)
        except PatchError:
            continue
        if other.get("machine") == MACHINE_ARMNT and other.get("dll_characteristics") & DLLCHAR_APPCONTAINER:
            siblings.append(dll.name)
    if siblings:
        say(f"companion DLL candidates in the folder (ARM, APPCONTAINER set): {siblings}")

    say("")
    say("A new profile for this version needs (probe only supplies leads, none of it is verified):")
    needs = [
        "exe.input: sha256/size/SizeOfImage/section table above (copy from `profiles --dump`)",
        "exe.bootstrap.call_site: the candidate above CONFIRMED by reverse engineering (arm32re: callers/func)",
        "exe.bootstrap.trampoline: one of the zero slots above, 4-aligned, not under .pdata/.reloc",
        "exe.renames/redirects: the *_app.dll / XINPUTUAP / api-set imports that must change (host facts)",
        "exe.new_section.expect_*: the SizeOfImage the host will identify the build with",
        "byte_patches: e.g. shader profile strings (_level_9_x), each with a verified single xref",
        "companions: every ARM DLL next to the exe that is loaded in-process (fmod, chakra, VC runtime copies)",
        "expected_outputs: SHA-256 of the output once a device run proves it",
    ]
    for item in needs:
        say("  - " + item)
    say("")
    say("NOTE: this is an UNKNOWN version unless 'known profile' says otherwise. The patcher can only apply")
    say("      edits whose old bytes it can verify; WHAT to change (call site meaning, host expectations,")
    say("      imports to redirect) still needs host facts from reverse engineering. Nothing here is applied.")
    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1, default=str), encoding="utf-8")
    return 0


# ---------------------------------------------------------------------------
# folder level driver
# ---------------------------------------------------------------------------

def resolve_profile(name: str, target: Path, profiles: dict) -> tuple[dict, dict]:
    info = identify(target, profiles)
    if name == "auto":
        if not info["profile"] or info["confidence"] in ("none", "low"):
            raise PatchError("cannot identify this version automatically; run `probe` on the exe "
                             "(an unknown version still needs a hand-built profile)")
        name = info["profile"]
        if info["confidence"] == "medium":
            say(f"WARNING: only medium confidence match for {name}: {info['reasons']}")
    if name not in profiles:
        raise PatchError(f"unknown profile {name!r}; known: {', '.join(profiles)}")
    profile = profiles[name]
    missing = profile_missing(profile)
    if missing:
        todo = "\n  - ".join(profile.get("todo", []))
        raise PatchError(f"profile {name} is a placeholder/incomplete ({', '.join(missing)}); not implemented yet."
                         + (f" It still needs:\n  - {todo}" if todo else ""))
    return profile, info


def make_options(args: argparse.Namespace) -> Options:
    skip = set(getattr(args, "skip", None) or [])
    if getattr(args, "no_fl91", False):
        skip.add("fl91")
    if getattr(args, "keep_winrt_init", False):
        skip.add("skip_winrt_init")
    return Options(dll_name=getattr(args, "dll_name", None) if getattr(args, "dll_name", None) != "win32craft.dll" else None,
                   export=getattr(args, "export", None) if getattr(args, "export", None) != "Win32Bootstrap" else None,
                   skip=skip, redirect_api_sets=getattr(args, "redirect_api_sets", False),
                   checksum=getattr(args, "checksum", None), base_relocs=getattr(args, "base_relocs", None),
                   allow_hash_mismatch=getattr(args, "allow_hash_mismatch", False),
                   allow_unknown_companion=getattr(args, "allow_unknown_companion", False))


def check_out_folder(src_folder: Path, out: Path, force: bool) -> None:
    s, o = src_folder.resolve(), out.resolve()
    if o == s or s in o.parents or o in s.parents:
        raise PatchError("the output folder must not be, contain or lie inside the game folder")
    if o.exists() and any(o.iterdir()) and not force:
        raise PatchError(f"output folder {out} is not empty (use --force to write into it)")


def cmd_patch(args: argparse.Namespace) -> int:
    profiles = load_profiles(args.profile_file)
    src = Path(args.game_folder)
    out = Path(args.out_folder)
    profile, info = resolve_profile(args.profile, src, profiles)
    opts = make_options(args)
    spec = profile["exe"]
    known = set()
    for item in spec.get("renames", []) + spec.get("byte_patches", []):
        known |= {item["id"], item.get("group")}
    for comp in profile["companions"]:
        for item in comp.get("renames", []) + comp.get("byte_patches", []):
            known |= {item["id"], item.get("group")}
    for token in sorted(opts.skip - known):
        say(f"WARNING: --skip {token} matches no edit id or group of profile {profile['name']}")
    exe_path = Path(info["exe_path"])
    if src.is_dir() and (src / spec["file"]).is_file():
        exe_path = src / spec["file"]
    elif src.is_file():
        exe_path = src
    folder = exe_path.parent
    if not exe_path.is_file():
        raise PatchError(f"{spec['file']} not found in {src}")
    check_out_folder(folder, out, args.force)
    say(f"profile {profile['name']}: {profile['title']}")
    say(f"input  {exe_path}")
    data = exe_path.read_bytes()
    patched, report = ExePatcher(data, profile, opts, exe_path.name).run()

    out.mkdir(parents=True, exist_ok=True)
    if not args.no_copy_folder and src.is_dir():
        omit = set() if args.keep_original_exe else {exe_path.name.lower()}
        omit |= {(c["name"].lower()) for c in profile["companions"]}

        def ignore(directory: str, names: list[str]) -> list[str]:
            return [n for n in names if Path(directory) == folder and n.lower() in omit]

        say(f"copying game folder to {out} ...")
        shutil.copytree(folder, out, ignore=ignore, dirs_exist_ok=True)
    out_name = args.exe_name or spec["output_name"]
    (out / out_name).write_bytes(patched)
    say(f"wrote {out / out_name}  sha256 {report['output']['sha256']}  {report['output']['size']} bytes  "
        f"SizeOfImage 0x{report['output']['size_of_image']:X}")
    eo = report["expected_output"]
    if eo.get("known_variant"):
        say(f"recorded output hash for variant '{eo['known_variant']}': "
            f"{'MATCH' if eo['match'] else 'NOT RECORDED' if eo['match'] is None else 'MISMATCH'}")
    full = {"tool": TOOL, "version": TOOL_VERSION, "profile": profile["name"],
            "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "source_folder": str(folder), "output_folder": str(out), "exe": {**report, "file": out_name,
                                                                          "source_file": exe_path.name},
            "companions": [], "problems": []}

    if not args.skip_companions:
        dirs = [Path(d) for d in args.companion_dir or []]
        appx = [Path(a) for a in args.companion_appx or []]
        for comp in profile["companions"]:
            data_c, source, searched = find_companion(profile, comp, folder if src.is_dir() or src.is_file() else None,
                                                      dirs, appx)
            if data_c is None:
                msg = f"companion {comp['name']} not found; searched: {searched}"
                if comp.get("required"):
                    full["problems"].append(msg)
                    say("ERROR: " + msg)
                else:
                    say("note: " + msg)
                continue
            patched_c, crep = DllPatcher(data_c, comp, opts, comp["name"]).run()
            (out / comp["name"]).write_bytes(patched_c)
            crep.update(source=source, file=comp["name"], provenance=comp.get("provenance"))
            full["companions"].append(crep)
            m = crep["matches_expected"]
            say(f"wrote {out / comp['name']}  [{crep['state_in']}] sha256 {crep['output']['sha256'][:16]}... "
                f"{'(= recorded patched hash)' if m else '(hash not comparable)' if m is None else '(DIFFERS from recorded hash)'}"
                f"  from {source}")
    if args.host_dll:
        host = Path(args.host_dll)
        target = out / (opts.dll_name or spec["import_add"]["dll"])
        shutil.copyfile(host, target)
        full["host_dll"] = {"source": str(host), "file": target.name, "sha256": sha256_file(target)}
        say(f"copied host dll -> {target}")
    if args.redist_dir and profile.get("redist_files"):
        copied = []
        for name in profile["redist_files"]:
            cand = Path(args.redist_dir) / name
            if cand.is_file():
                shutil.copyfile(cand, out / name)
                copied.append({"file": name, "sha256": sha256_file(out / name)})
        full["redist_copied"] = copied
        say(f"redistributables copied: {[c['file'] for c in copied]}")
    full["ship_notes"] = profile.get("ship_notes", [])

    # final re-parse of what was written
    reparsed = verify_folder(out, profile, opts, folder, not args.skip_companions, out_name)
    full["verify"] = reparsed
    bad = [c for c in reparsed["checks"] if not c["ok"]]
    (out / "patch_report.json").write_text(json.dumps(full, indent=1), encoding="utf-8")
    say(f"wrote {out / 'patch_report.json'}")
    say(f"verify (re-parsed from disk): {len(reparsed['checks']) - len(bad)}/{len(reparsed['checks'])} checks OK")
    for c in bad:
        say(f"  FAIL {c['name']}: {c['detail']}")
    if full["problems"]:
        say("PROBLEMS: " + "; ".join(full["problems"]))
    return 0 if not bad and not full["problems"] else 2


def find_patched_exe(out: Path, spec: dict) -> Optional[Path]:
    wanted = out / spec["output_name"]
    if wanted.is_file():
        return wanted
    section = (spec.get("new_section") or {}).get("name")
    found = []
    for cand in sorted(out.glob("*.exe")):
        try:
            if section and any(s.name == section for s in PE(cand.read_bytes(), cand.name).sections):
                found.append(cand)
        except PatchError:
            continue
    return found[0] if len(found) == 1 else None


def verify_folder(out: Path, profile: dict, opts: Options, source_folder: Optional[Path],
                  require_companions: bool = True, exe_name: Optional[str] = None) -> dict:
    spec = profile["exe"]
    checks: list[dict] = []
    exe = out / exe_name if exe_name else find_patched_exe(out, spec)
    orig = None
    if source_folder is not None and (source_folder / spec["file"]).is_file():
        orig = (source_folder / spec["file"]).read_bytes()
    if exe is not None and exe.is_file():
        for c in verify_patched_exe(exe.read_bytes(), profile, orig, opts):
            checks.append({**c, "name": f"exe: {c['name']}"})
    else:
        checks.append({"name": "patched exe present", "ok": False, "detail": f"not found in {out}"})
    for comp in profile["companions"]:
        path = out / comp["name"]
        if not path.is_file():
            if comp.get("required") and require_companions:
                checks.append({"name": f"{comp['name']} present", "ok": False, "detail": "missing"})
            continue
        for c in verify_companion(path.read_bytes(), comp):
            checks.append({**c, "name": f"{comp['name']}: {c['name']}"})
    return {"exe": str(exe), "checks": checks}


def cmd_detect(args: argparse.Namespace) -> int:
    profiles = load_profiles(args.profile_file)
    info = identify(Path(args.target), profiles)
    print_identify(info, profiles)
    if args.json:
        Path(args.json).write_text(json.dumps(info, indent=1, default=str), encoding="utf-8")
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    profiles = load_profiles(args.profile_file)
    target = Path(args.target)
    info = identify(target, profiles)
    name = args.profile if args.profile and args.profile != "auto" else info["profile"]
    if not name:
        raise PatchError("cannot identify the profile; pass --profile")
    profile = profiles[name]
    if profile_missing(profile):
        raise PatchError(f"profile {name} is a placeholder")
    opts = make_options(args)
    if target.is_dir():
        res = verify_folder(target, profile, opts, None)
        checks = res["checks"]
    else:
        checks = verify_patched_exe(target.read_bytes(), profile, None, opts)
    bad = 0
    for c in checks:
        say(f"{'PASS' if c['ok'] else 'FAIL'}  {c['name']}" + (f"  [{c['detail']}]" if c["detail"] else ""))
        bad += 0 if c["ok"] else 1
    return 1 if bad else 0


def cmd_profiles(args: argparse.Namespace) -> int:
    profiles = load_profiles(args.profile_file)
    if args.dump:
        if args.dump not in profiles:
            raise PatchError(f"unknown profile {args.dump}")
        print(json.dumps(profiles[args.dump], indent=1))
        return 0
    for name, p in profiles.items():
        missing = profile_missing(p)
        say(f"{name:10s} {p['status']:11s} {p['title']}")
        if missing:
            say(f"{'':10s} missing: {', '.join(missing)}")
    return 0


def cmd_dll(args: argparse.Namespace) -> int:
    profiles = load_profiles(args.profile_file)
    src, dst = Path(args.input), Path(args.output)
    if src.resolve() == dst.resolve():
        raise PatchError("output must not overwrite the input")
    wanted = args.profile
    cands = []
    for name, p in profiles.items():
        if wanted not in (None, "auto") and name != wanted:
            continue
        for c in p.get("companions", []):
            if c["name"].lower() == src.name.lower():
                cands.append((p, c))
    if not cands:
        raise PatchError(f"no profile knows a companion named {src.name}")
    data = src.read_bytes()
    digest = sha256_hex(data)
    cands.sort(key=lambda pc: digest not in (pc[1].get("sha256_original"), pc[1].get("sha256_patched")))
    profile, comp = cands[0]
    patched, rep = DllPatcher(data, comp, make_options(args), src.name).run()
    dst.write_bytes(patched)
    say(f"{rep['name']} [{rep['state_in']}] -> {dst}  sha256 {rep['output']['sha256']}  "
        f"(profile {profile['name']}, expected {'MATCH' if rep['matches_expected'] else rep['matches_expected']})")
    for e in rep["edits"]:
        say(f"  {e['id']}: file+0x{e['file_offset']:X} {e['old_hex']} -> {e['new_hex']}")
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(prog="arm32-patcher.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile-file", action="append", metavar="JSON",
                    help="extra profile(s) as JSON (same schema as `profiles --dump`); repeatable")
    sub = ap.add_subparsers(dest="cmd", required=True)

    d = sub.add_parser("detect", help="identify the version of an exe or game folder")
    d.add_argument("target")
    d.add_argument("--json", metavar="FILE")
    d.set_defaults(func=cmd_detect)

    p = sub.add_parser("patch", help="patch a game folder into an output folder")
    p.add_argument("game_folder")
    p.add_argument("out_folder")
    p.add_argument("--profile", default="auto", help="auto (default) or a profile name")
    p.add_argument("--dll-name", default="win32craft.dll", help="host DLL the exe imports")
    p.add_argument("--export", default="Win32Bootstrap", help="host export the call site is routed to")
    p.add_argument("--skip", action="append", metavar="ID_OR_GROUP", help="leave an edit (id or group) out")
    p.add_argument("--no-fl91", action="store_true", help="0.10.4: do not patch the _level_9_3 profile string")
    p.add_argument("--keep-winrt-init", action="store_true",
                   help="1.2.8 fmod: keep the WinRT Package/ApplicationData init (hook it instead)")
    p.add_argument("--redirect-api-sets", action="store_true", help="1.2.8: also redirect the six optional api sets")
    p.add_argument("--checksum", choices=["keep", "recompute"], help="PE header checksum policy (default: profile)")
    p.add_argument("--base-relocs", choices=["keep", "drop"], help="base relocation policy (default: profile)")
    p.add_argument("--companion-dir", action="append", metavar="DIR", help="extra folder with companion DLLs")
    p.add_argument("--companion-appx", action="append", metavar="APPX", help="VCLibs .appx holding the originals")
    p.add_argument("--allow-hash-mismatch", action="store_true", help="accept an exe with another SHA-256 (offsets still verified)")
    p.add_argument("--allow-unknown-companion", action="store_true", help="patch companion DLLs with unknown SHA-256")
    p.add_argument("--skip-companions", action="store_true")
    p.add_argument("--host-dll", metavar="FILE", help="copy this win32craft.dll into the output folder")
    p.add_argument("--redist-dir", metavar="DIR", help="copy the profile's redistributable DLLs from here")
    p.add_argument("--exe-name", help="file name of the patched exe (default: profile output_name)")
    p.add_argument("--keep-original-exe", action="store_true", help="also copy the unpatched exe into the output")
    p.add_argument("--no-copy-folder", action="store_true", help="write only the patched files (no folder copy)")
    p.add_argument("--force", action="store_true", help="write into a non-empty output folder")
    p.set_defaults(func=cmd_patch)

    pr = sub.add_parser("probe", help="auto-discovery for unknown versions")
    pr.add_argument("exe")
    pr.add_argument("--json", metavar="FILE")
    pr.set_defaults(func=cmd_probe)

    v = sub.add_parser("verify", help="re-parse and check a patched exe/folder")
    v.add_argument("target")
    v.add_argument("--profile", default="auto")
    v.add_argument("--dll-name")
    v.add_argument("--export")
    v.set_defaults(func=cmd_verify)

    pf = sub.add_parser("profiles", help="list profiles or dump one as JSON")
    pf.add_argument("--dump", metavar="NAME")
    pf.set_defaults(func=cmd_profiles)

    dl = sub.add_parser("dll", help="patch one companion DLL")
    dl.add_argument("input")
    dl.add_argument("output")
    dl.add_argument("--profile", default="auto")
    dl.add_argument("--skip", action="append")
    dl.add_argument("--checksum", choices=["keep", "recompute"])
    dl.add_argument("--allow-unknown-companion", action="store_true")
    dl.set_defaults(func=cmd_dll)
    return ap


def main(argv: Optional[list[str]] = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(errors="replace")
        except Exception:
            pass
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except PatchError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
