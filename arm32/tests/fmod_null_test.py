#!/usr/bin/env python3
"""
fmod_null_test.py - Unicorn ARM32 harness for the SILENT fmod_ARM.dll replacement (fmod_null/fmod_null.c).

Usage:  python arm32/tests/fmod_null_test.py [path\\to\\fmod_ARM.dll]
        (default: dist-fmod-null-arm32\\fmod_ARM.dll under the repo root; env FMOD_NULL_DLL also works)
Optional environment: MC_EXE (game exe, default Minecraft_1.2.8.0_ARM32/Minecraft.Windows.exe),
                      LLVM_BIN (for llvm-readobj, default D:\\repo\\llvm-rt\\scratch\\RT23P\\build-rt21\\bin)

What it proves
  1. static PE checks: ARM32 DLL, DYNAMIC_BASE, windows subsystem, EMPTY import table, base relocations present,
     exactly the 39 exports the game imports (compared three ways: arm32/facts/imports_table.json, the real import
     directory of Minecraft.Windows.exe when present, and `llvm-readobj --coff-exports`), every export a Thumb
     function inside .text;
  2. dynamic: the DLL is mapped like the Windows loader would (sections + base relocations, ARM_MOV32T and HIGHLOW)
     at two different bases (the preferred base and a relocated one) and EVERY export is called through the
     AAPCS-VFP convention with the exact register/stack/VFP layout the game uses (decoded from the real call sites
     in SoundSystemFMOD, see LAYOUTS below) -> r0 == FMOD_OK, benign out-parameters, callee-saved registers and sp
     preserved, no write outside the declared out-buffers / the DLL's own data / the stack;
  3. a realistic game sequence System_Create -> getVersion -> init -> setFileSystem -> channel groups -> createStream
     -> playSound -> Channel getters/setters -> release -> close;
  4. a NULL-tolerance sweep: every export called with NULL out-pointers and unmapped garbage for every other pointer
     (including `this`) must still return OK without touching memory.
"""
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_THUMB, UC_PROT_ALL, UC_HOOK_MEM_WRITE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R4, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
                               UC_ARM_REG_S0, UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC)

REPO = Path(__file__).resolve().parents[2]
FACTS = REPO / "arm32" / "facts" / "imports_table.json"
DEFAULT_DLL = REPO / "dist-fmod-null-arm32" / "fmod_ARM.dll"
DEFAULT_EXE = REPO / "Minecraft_1.2.8.0_ARM32" / "Minecraft.Windows.exe"
DEFAULT_LLVM_BIN = r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"

FMOD_OK = 0
NULL_OBJ_MAGIC = 0x4C4C554E
KIND = {"system": 1, "group": 2, "sound": 3, "channel": 4}
FMOD_VERSION_MIN = 0x00010904
GARBAGE = 0x7EAD0000          # never mapped: any dereference raises a Unicorn fault


# ----------------------------------------------------------------------------------------------- PE parsing
class PE:
    def __init__(self, data):
        self.data = data
        if data[:2] != b"MZ":
            raise ValueError("not an MZ file")
        e = struct.unpack_from("<I", data, 0x3C)[0]
        if data[e:e + 4] != b"PE\0\0":
            raise ValueError("no PE signature")
        (self.machine, nsec, _ts, _sym, _nsym, optsz, self.characteristics) = struct.unpack_from("<HHIIIHH", data, e + 4)
        o = e + 24
        self.magic = struct.unpack_from("<H", data, o)[0]
        if self.magic != 0x10B:
            raise ValueError("not PE32")
        self.entry = struct.unpack_from("<I", data, o + 16)[0]
        self.image_base = struct.unpack_from("<I", data, o + 28)[0]
        self.size_of_image = struct.unpack_from("<I", data, o + 56)[0]
        self.size_of_headers = struct.unpack_from("<I", data, o + 60)[0]
        self.subsystem = struct.unpack_from("<H", data, o + 68)[0]
        self.dll_characteristics = struct.unpack_from("<H", data, o + 70)[0]
        ndirs = struct.unpack_from("<I", data, o + 92)[0]
        self.dirs = [struct.unpack_from("<II", data, o + 96 + 8 * i) for i in range(min(ndirs, 16))]
        self.sections = []
        so = o + optsz
        for i in range(nsec):
            name, vsize, va, rawsize, rawptr = struct.unpack_from("<8sIIII", data, so + 40 * i)
            self.sections.append((name.rstrip(b"\0").decode("ascii", "replace"), va, vsize, rawptr, rawsize))

    def directory(self, idx):
        return self.dirs[idx] if idx < len(self.dirs) else (0, 0)

    def rva2off(self, rva):
        if rva < self.size_of_headers:
            return rva
        for _n, va, vsize, rawptr, rawsize in self.sections:
            if va <= rva < va + max(vsize, rawsize):
                return rawptr + (rva - va)
        raise ValueError("RVA 0x%X is not in any section" % rva)

    def cstr(self, rva):
        off = self.rva2off(rva)
        end = self.data.index(b"\0", off)
        return self.data[off:end].decode("ascii")

    def u32(self, rva):
        return struct.unpack_from("<I", self.data, self.rva2off(rva))[0]

    def section_of(self, rva):
        for s in self.sections:
            if s[1] <= rva < s[1] + s[2]:
                return s[0]
        return None

    def exports(self):
        """name -> raw export RVA (bit 0 = Thumb)"""
        erva, esize = self.directory(0)
        if not erva:
            return {}, (0, 0)
        off = self.rva2off(erva)
        (_c, _t, _a, _b, _name, _base, nfunc, nnames, aof, aon, aoo) = struct.unpack_from("<IIHHIIIIIII", self.data, off)
        out = {}
        for i in range(nnames):
            name = self.cstr(self.u32(aon + 4 * i))
            ordinal = struct.unpack_from("<H", self.data, self.rva2off(aoo + 2 * i))[0]
            if name in out:
                raise ValueError("duplicate export " + name)
            out[name] = self.u32(aof + 4 * ordinal)
        return out, (erva, esize)

    def imports(self, dll_lower=None):
        """list of (dll, [names]) from the import directory"""
        irva, isize = self.directory(1)
        res = []
        if not irva:
            return res
        off = self.rva2off(irva)
        while True:
            oft, _ts, _fc, name_rva, ft = struct.unpack_from("<IIIII", self.data, off)
            if not (oft or name_rva or ft):
                break
            dll = self.cstr(name_rva)
            names = []
            p = oft or ft
            while True:
                thunk = self.u32(p)
                if not thunk:
                    break
                names.append("#%d" % (thunk & 0xFFFF) if thunk & 0x80000000 else self.cstr((thunk & 0x7FFFFFFF) + 2))
                p += 4
            res.append((dll, names))
            off += 20
        return [r for r in res if dll_lower is None or r[0].lower() == dll_lower]

    def base_relocs(self):
        rva, size = self.directory(5)
        out = []
        if not rva:
            return out
        off = self.rva2off(rva)
        end = off + size
        while off < end:
            page, blk = struct.unpack_from("<II", self.data, off)
            if blk < 8:
                break
            for i in range((blk - 8) // 2):
                v = struct.unpack_from("<H", self.data, off + 8 + 2 * i)[0]
                out.append((v >> 12, page + (v & 0xFFF)))
            off += blk
        return out


# ------------------------------------------------------------------------------------- ARM AAPCS-VFP calls
def f2bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def bits2f(b):
    return struct.unpack("<f", struct.pack("<I", b & 0xFFFFFFFF))[0]


def assign(args):
    """AAPCS-VFP argument placement.  args: list of (kind, value), kind in
       'i' (32-bit int / pointer / bool), 'q' (64-bit int), 'f' (float).
       returns (core_regs{idx:val}, sregs{idx:bits}, stack_words[list])."""
    ncrn = 0
    regs, sregs, stack = {}, {}, []
    ns = 0
    for kind, v in args:
        if kind == "f":
            if ns >= 16:
                raise ValueError("too many float args for the harness")
            sregs[ns] = f2bits(v)
            ns += 1
        elif kind == "i":
            v &= 0xFFFFFFFF
            if ncrn < 4:
                regs[ncrn] = v
                ncrn += 1
            else:
                stack.append(v)
        elif kind == "q":
            v &= 0xFFFFFFFFFFFFFFFF
            lo, hi = v & 0xFFFFFFFF, v >> 32
            if ncrn < 4 and ncrn % 2:
                ncrn += 1
            if ncrn <= 2:
                regs[ncrn], regs[ncrn + 1] = lo, hi
                ncrn += 2
            else:
                ncrn = 4
                if len(stack) % 2:
                    stack.append(0)          # padding to an 8-byte aligned slot
                stack += [lo, hi]
        else:
            raise ValueError(kind)
    return regs, sregs, stack


# Register/stack layouts of the REAL game call sites (SoundSystemFMOD, decoded with arm32re.py); the harness asserts
# that assign() reproduces them, so the tests below exercise exactly what the exe does.
# name -> (args, expected core regs, expected stack words, expected s regs)
S = lambda x: f2bits(x)
LAYOUTS = {
    # 5a39e8 setFileSystem: r1=open r2=close r3=read, [sp]=seek [sp+4]=0 [sp+8]=0 [sp+12]=-1
    "setFileSystem": ([("i", 0x100), ("i", 0x201), ("i", 0x302), ("i", 0x403), ("i", 0x504), ("i", 0), ("i", 0), ("i", -1)],
                      {0: 0x100, 1: 0x201, 2: 0x302, 3: 0x403}, [0x504, 0, 0, 0xFFFFFFFF], {}),
    # 5a39fa set3DSettings: s0,s1,s2 (this in r0)
    "set3DSettings": ([("i", 0x100), ("f", 1.0), ("f", 2.0), ("f", 3.0)], {0: 0x100}, [], {0: S(1.0), 1: S(2.0), 2: S(3.0)}),
    # 5a3a6c addGroup: r0=master r1=group r2=1(bool) r3=NULL
    "addGroup": ([("i", 0x100), ("i", 0x200), ("i", 1), ("i", 0)], {0: 0x100, 1: 0x200, 2: 1, 3: 0}, [], {}),
    # 5a40d6 createSound / 5a3eb6 createStream: r1=name r2=mode r3=exinfo [sp]=Sound**
    "createSound": ([("i", 0x100), ("i", 0x200), ("i", 0x300), ("i", 0), ("i", 0x500)],
                    {0: 0x100, 1: 0x200, 2: 0x300, 3: 0}, [0x500], {}),
    # 5a327e/5a48b4 playSound: r1=sound r2=group r3=paused(1) [sp]=Channel**
    "playSound": ([("i", 0x100), ("i", 0x200), ("i", 0x300), ("i", 1), ("i", 0x500)],
                  {0: 0x100, 1: 0x200, 2: 0x300, 3: 1}, [0x500], {}),
    # 5a3224 getFormat: r1=&type r2=NULL r3=NULL [sp]=NULL
    "getFormat": ([("i", 0x100), ("i", 0x200), ("i", 0), ("i", 0), ("i", 0)], {0: 0x100, 1: 0x200, 2: 0, 3: 0}, [0], {}),
    # 5a38d2 getDriverInfo: r1=id r2=name r3=0x80, [sp]=guid [sp+4]=NULL [sp+8]=NULL [sp+12]=NULL
    "getDriverInfo": ([("i", 0x100), ("i", 0), ("i", 0x200), ("i", 0x80), ("i", 0x400), ("i", 0), ("i", 0), ("i", 0)],
                      {0: 0x100, 1: 0, 2: 0x200, 3: 0x80}, [0x400, 0, 0, 0], {}),
    # 5a4d10 set3DListenerAttributes: r1=listener r2=pos r3=vel [sp]=forward [sp+4]=up
    "set3DListenerAttributes": ([("i", 0x100), ("i", 0), ("i", 0x200), ("i", 0x300), ("i", 0x400), ("i", 0x500)],
                                {0: 0x100, 1: 0, 2: 0x200, 3: 0x300}, [0x400, 0x500], {}),
    # 5a499c addFadePoint: clock in r2:r3 (r1 skipped), volume s0
    "addFadePoint": ([("i", 0x100), ("q", 0x1122334455667788), ("f", 0.25)],
                     {0: 0x100, 2: 0x55667788, 3: 0x11223344}, [], {0: S(0.25)}),
    # 5a49fc setDelay: start r2:r3, end [sp],[sp+4], stopchannels [sp+8]
    "setDelay": ([("i", 0x100), ("q", 0xAABBCCDD11223344), ("q", 0x0102030405060708), ("i", 1)],
                 {0: 0x100, 2: 0x11223344, 3: 0xAABBCCDD}, [0x05060708, 0x01020304, 1], {}),
}


def check_layouts():
    for name, (args, regs, stack, sregs) in LAYOUTS.items():
        r, s, st = assign(args)
        assert r == regs, "layout %s: regs %r != %r" % (name, r, regs)
        assert st == stack, "layout %s: stack %r != %r" % (name, st, stack)
        assert s == sregs, "layout %s: sregs %r != %r" % (name, s, sregs)
    print("  [ok] AAPCS-VFP placement helper reproduces the %d decoded game call-site layouts" % len(LAYOUTS))


# ----------------------------------------------------------------------------------------------- emulator
STACK_BASE, STACK_SIZE = 0x70000000, 0x100000
HEAP_BASE, HEAP_SIZE = 0x60000000, 0x100000
RET_ADDR = 0x71000000


class Emu:
    def __init__(self, pe, base):
        self.pe, self.base = pe, base
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
        uc = self.uc
        # enable VFP/NEON (CPACR cp10/cp11 full access + FPEXC.EN) - getDriverInfo zeroes the GUID with a NEON store
        uc.reg_write(UC_ARM_REG_C1_C0_2, uc.reg_read(UC_ARM_REG_C1_C0_2) | (0xF << 20))
        uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        # pristine CPU context restored before every call (see Emu.call for why)
        self.ctx0 = uc.context_save()
        size = (pe.size_of_image + 0xFFF) & ~0xFFF
        uc.mem_map(base, size, UC_PROT_ALL)
        uc.mem_write(base, pe.data[:pe.size_of_headers])
        for _n, va, vsize, rawptr, rawsize in pe.sections:
            n = min(vsize, rawsize)
            if n:
                uc.mem_write(base + va, pe.data[rawptr:rawptr + n])
        self.apply_relocs()
        uc.mem_map(STACK_BASE, STACK_SIZE, UC_PROT_ALL)
        uc.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_ALL)
        uc.mem_map(RET_ADDR, 0x1000, UC_PROT_ALL)
        uc.mem_write(RET_ADDR, b"\x00\xbf" * 8)          # thumb nops: landing pad of every call
        self.heap_top = HEAP_BASE
        self.writes = []
        uc.hook_add(UC_HOOK_MEM_WRITE, self._on_write)
        self.names = {}                                    # entry address -> export name
        self.called = set()
        self.data_ranges = [(base + va, base + va + vsize) for n, va, vsize, _p, _s in pe.sections if n in (".data", ".bss")]
        self.insn_total = 0

    # -- loader: base relocations, as ntdll's LdrProcessRelocationBlock does for ARM
    def apply_relocs(self):
        uc, delta = self.uc, (self.base - self.pe.image_base) & 0xFFFFFFFF
        stats = {}
        for typ, rva in self.pe.base_relocs():
            stats[typ] = stats.get(typ, 0) + 1
            addr = self.base + rva
            if typ == 0:
                continue
            if typ == 3:                                    # IMAGE_REL_BASED_HIGHLOW
                v = struct.unpack("<I", uc.mem_read(addr, 4))[0]
                uc.mem_write(addr, struct.pack("<I", (v + delta) & 0xFFFFFFFF))
            elif typ == 7:                                  # IMAGE_REL_BASED_ARM_MOV32T: movw+movt (Thumb-2)
                raw = bytes(uc.mem_read(addr, 8))
                w0, w1, t0, t1 = struct.unpack("<4H", raw)
                lo = self._imm16(w0, w1)
                hi = self._imm16(t0, t1)
                val = (((hi << 16) | lo) + delta) & 0xFFFFFFFF
                w0, w1 = self._set16(w0, w1, val & 0xFFFF)
                t0, t1 = self._set16(t0, t1, val >> 16)
                uc.mem_write(addr, struct.pack("<4H", w0, w1, t0, t1))
            else:
                raise AssertionError("unsupported base relocation type %d at RVA 0x%X" % (typ, rva))
        self.reloc_stats = stats

    @staticmethod
    def _imm16(hw1, hw2):
        # T3 MOVW/MOVT: imm4 = hw1[3:0], i = hw1[10], imm3 = hw2[14:12], imm8 = hw2[7:0]
        assert (hw1 & 0xFBF0) in (0xF240, 0xF2C0), "relocation does not point at movw/movt: %04x %04x" % (hw1, hw2)
        return ((hw1 & 0xF) << 12) | (((hw1 >> 10) & 1) << 11) | (((hw2 >> 12) & 7) << 8) | (hw2 & 0xFF)

    @staticmethod
    def _set16(hw1, hw2, imm):
        hw1 = (hw1 & 0xFBF0) | ((imm >> 12) & 0xF) | (((imm >> 11) & 1) << 10)
        hw2 = (hw2 & 0x8F00) | (((imm >> 8) & 7) << 12) | (imm & 0xFF)
        return hw1, hw2

    def _on_write(self, uc, access, address, size, value, user):
        self.writes.append((address, size))

    # -- memory helpers
    def alloc(self, nbytes, fill=0xAA, align=8):
        self.heap_top = (self.heap_top + align - 1) & ~(align - 1)
        a = self.heap_top
        self.heap_top += nbytes
        assert self.heap_top < HEAP_BASE + HEAP_SIZE, "harness heap exhausted"
        self.uc.mem_write(a, bytes([fill]) * nbytes)
        return a

    def r32(self, a):
        return struct.unpack("<I", self.uc.mem_read(a, 4))[0]

    def r64(self, a):
        return struct.unpack("<Q", self.uc.mem_read(a, 8))[0]

    def rb(self, a, n):
        return bytes(self.uc.mem_read(a, n))

    def w32(self, a, v):
        self.uc.mem_write(a, struct.pack("<I", v & 0xFFFFFFFF))

    def cstring(self, s):
        b = s.encode() + b"\0"
        a = self.alloc(len(b))
        self.uc.mem_write(a, b)
        return a

    def vec3(self, x, y, z):
        a = self.alloc(12)
        self.uc.mem_write(a, struct.pack("<3f", x, y, z))
        return a

    # -- calling convention
    def export_addr(self, name):
        return self.base + self.pe_exports[name]

    def call(self, name, args, out_ranges=(), expect=FMOD_OK):
        """Call export `name` with (kind,value) args placed per AAPCS-VFP.  Returns r0.  out_ranges = [(addr,size)]
        buffers the callee may write.  Checks FMOD result, callee-saved regs, sp and the memory writes."""
        uc = self.uc
        entry = self.export_addr(name)
        assert entry & 1, "%s: export RVA has no Thumb bit" % name
        self.called.add(name)
        regs, sregs, stack = assign(args)
        # Unicorn quirk: after a run that stopped at `until`, the next emu_start() silently skips the first
        # instruction.  Restoring the pristine CPU context (registers/mode only, memory is untouched) avoids it.
        uc.context_restore(self.ctx0)
        sp = (STACK_BASE + STACK_SIZE - 0x1000 - 4 * len(stack)) & ~7
        for i, w in enumerate(stack):
            self.w32(sp + 4 * i, w)
        for i in range(13):
            uc.reg_write(UC_ARM_REG_R0 + i, 0xDEAD0000 + i)           # poison unused regs
        for i, v in regs.items():
            uc.reg_write(UC_ARM_REG_R0 + i, v)
        canaries = {}
        for i in range(4, 12):                                         # callee-saved r4-r11
            canaries[i] = 0xC4A00000 + i
            uc.reg_write(UC_ARM_REG_R0 + i, canaries[i])
        for i, bits in sregs.items():
            uc.reg_write(UC_ARM_REG_S0 + i, bits)
        uc.reg_write(UC_ARM_REG_SP, sp)
        uc.reg_write(UC_ARM_REG_LR, RET_ADDR | 1)
        self.writes = []
        try:
            uc.emu_start(entry, RET_ADDR, count=100000)
        except UcError as e:
            raise AssertionError("%s: emulation fault: %s (pc=0x%08X)" % (name, e, uc.reg_read(UC_ARM_REG_PC)))
        pc = uc.reg_read(UC_ARM_REG_PC)
        assert (pc & ~1) == RET_ADDR, "%s: did not return to the caller (pc=0x%08X)" % (name, pc)
        assert uc.reg_read(UC_ARM_REG_SP) == sp, "%s: sp not restored" % name
        for i, v in canaries.items():
            assert uc.reg_read(UC_ARM_REG_R0 + i) == v, "%s: callee-saved r%d clobbered" % (name, i)
        r0 = uc.reg_read(UC_ARM_REG_R0)
        assert r0 == expect, "%s: returned 0x%X, expected 0x%X" % (name, r0, expect)
        for a, sz in self.writes:
            ok = (STACK_BASE <= a < STACK_BASE + STACK_SIZE) or any(lo <= a and a + sz <= hi for lo, hi in self.data_ranges)
            ok = ok or any(lo <= a and a + sz <= lo + n for lo, n in out_ranges)
            assert ok, "%s: unexpected write of %d bytes at 0x%08X" % (name, sz, a)
        return r0


def load_dll(path, base):
    data = Path(path).read_bytes()
    pe = PE(data)
    emu = Emu(pe, base)
    exps, _ = pe.exports()
    emu.pe_exports = exps
    return emu


# ------------------------------------------------------------------------------------- export name tables
def expected_imports():
    import json
    d = json.loads(FACTS.read_text(encoding="utf-8"))
    ent = [e for e in d["normal"] if e["dll"].lower() == "fmod_arm.dll"]
    assert len(ent) == 1
    return [f["name"] for f in ent[0]["funcs"]]


def cpp(name):
    """short key of a mangled export: 'Class::method' or the C name"""
    m = re.match(r"\?(\w+)@(\w+)@FMOD@@", name)
    return "%s::%s" % (m.group(2), m.group(1)) if m else name


# argument kinds for the sweep / signature table: this, i, f, q, in (non-out pointer), out (out pointer)
SIGS = {
    "System::getVersion": ["this", "out"],
    "System::init": ["this", "i", "i", "in"],
    "System::set3DSettings": ["this", "f", "f", "f"],
    "System::createChannelGroup": ["this", "in", "out"],
    "System::getMasterChannelGroup": ["this", "out"],
    "ChannelGroup::addGroup": ["this", "in", "i", "out"],
    "System::close": ["this"],
    "System::release": ["this"],
    "System::mixerResume": ["this"],
    "System::mixerSuspend": ["this"],
    "System::set3DNumListeners": ["this", "i"],
    "ChannelControl::setMute": ["this", "i"],
    "System::setOutput": ["this", "i"],
    "Sound::set3DMinMaxDistance": ["this", "f", "f"],
    "Sound::release": ["this"],
    "System::createSound": ["this", "in", "i", "in", "out"],
    "ChannelControl::stop": ["this"],
    "ChannelControl::set3DAttributes": ["this", "in", "in", "in"],
    "System::getSoftwareFormat": ["this", "out", "out", "out"],
    "ChannelControl::getDSPClock": ["this", "out", "out"],
    "ChannelControl::addFadePoint": ["this", "q", "f"],
    "ChannelControl::setDelay": ["this", "q", "q", "i"],
    "ChannelControl::isPlaying": ["this", "out"],
    "System::update": ["this"],
    "System::set3DListenerAttributes": ["this", "i", "in", "in", "in", "in"],
    "System::getDriverInfo": ["this", "i", "out", "i", "out", "out", "out", "out"],
    "System::setDriver": ["this", "i"],
    "System::createStream": ["this", "in", "i", "in", "out"],
    "System::getNumDrivers": ["this", "out"],
    "FMOD_System_Create": ["out"],
    "Sound::getFormat": ["this", "out", "out", "out", "out"],
    "Sound::setMode": ["this", "i"],
    "System::setFileSystem": ["this", "in", "in", "in", "in", "in", "in", "i"],
    "System::playSound": ["this", "in", "in", "i", "out"],
    "Sound::getSubSound": ["this", "i", "out"],
    "Sound::getNumSubSounds": ["this", "out"],
    "ChannelControl::setVolume": ["this", "f"],
    "ChannelControl::setPaused": ["this", "i"],
    "ChannelControl::setPitch": ["this", "f"],
}


# ----------------------------------------------------------------------------------------------- the checks
class Counter:
    n = 0

    @classmethod
    def ok(cls, msg):
        cls.n += 1
        print("  [ok] " + msg)


def static_checks(dll_path):
    data = Path(dll_path).read_bytes()
    pe = PE(data)
    assert pe.machine == 0x01C4, "machine 0x%X is not ARMNT" % pe.machine
    assert pe.characteristics & 0x2000, "not a DLL"
    assert pe.dll_characteristics & 0x0040, "DYNAMIC_BASE not set"
    assert pe.subsystem == 2, "subsystem %d is not WINDOWS_GUI" % pe.subsystem
    Counter.ok("PE32 ARMNT DLL, /subsystem:windows, DYNAMIC_BASE, DllCharacteristics=0x%04X" % pe.dll_characteristics)
    assert pe.imports() == [] and pe.directory(1)[0] == 0, "DLL has imports: %r" % pe.imports()
    Counter.ok("import directory is EMPTY (no kernel32, no CRT, nothing)")
    relocs = pe.base_relocs()
    assert any(t == 7 for t, _ in relocs), "no ARM_MOV32T relocations"
    Counter.ok("base relocations present: %d entries, types %s" % (len(relocs), sorted({t for t, _ in relocs})))

    exps, (erva, esize) = pe.exports()
    want = expected_imports()
    assert len(want) == 39 and len(set(want)) == 39
    assert set(exps) == set(want), "exports differ from imports_table.json: only-in-dll=%s only-in-exe=%s" % (
        sorted(set(exps) - set(want)), sorted(set(want) - set(exps)))
    assert len(exps) == 39
    Counter.ok("39 exports == the 39 imports of imports_table.json (byte-exact names, no extras)")

    text = [s for s in pe.sections if s[0] == ".text"][0]
    for name, rva in exps.items():
        assert rva & 1, "%s: RVA 0x%X has no Thumb bit" % (name, rva)
        assert text[1] <= (rva & ~1) < text[1] + text[2], "%s: not in .text" % name
        assert not (erva <= rva < erva + esize), "%s: is a forwarder" % name
    Counter.ok("every export is a Thumb function inside .text (RVA bit0 set), no forwarders")

    exe = Path(os.environ.get("MC_EXE", DEFAULT_EXE))
    if exe.exists():
        exe_pe = PE(exe.read_bytes())
        imps = exe_pe.imports("fmod_arm.dll")
        assert len(imps) == 1, "exe has %d fmod_ARM.dll descriptors" % len(imps)
        names = imps[0][1]
        assert len(names) == 39 and set(names) == set(exps), "exe import directory != DLL exports: %s / %s" % (
            sorted(set(names) - set(exps)), sorted(set(exps) - set(names)))
        assert names == want, "exe import order differs from imports_table.json"
        Counter.ok("real %s import directory: 39 names from fmod_ARM.dll == DLL exports (set and order)" % exe.name)
    else:
        print("  [skip] %s not found: exe import directory comparison skipped" % exe)

    llvm_bin = os.environ.get("LLVM_BIN", DEFAULT_LLVM_BIN)
    ro = Path(llvm_bin) / "llvm-readobj.exe"
    if ro.exists():
        out = subprocess.run([str(ro), "--coff-exports", str(dll_path)], capture_output=True, text=True, check=True).stdout
        ro_names = re.findall(r"^\s*Name: (.+)$", out, re.M)
        assert len(ro_names) == 39 and set(ro_names) == set(want), "llvm-readobj export list mismatch"
        Counter.ok("llvm-readobj --coff-exports lists the same 39 names as imports_table.json")
        imp_out = subprocess.run([str(ro), "--coff-imports", str(dll_path)], capture_output=True, text=True, check=True).stdout
        assert "Import {" not in imp_out and "Name:" not in imp_out, "llvm-readobj reports imports"
        Counter.ok("llvm-readobj --coff-imports reports no imports")
    else:
        print("  [skip] llvm-readobj not found: tool cross-check skipped")
    return pe


def functional(emu, tag):
    """call every export with synthetic ABI-correct arguments; realistic game sequence first"""
    c = emu.call
    ck = emu.cstring

    def handle(addr, kind, what):
        assert addr != 0, what + ": NULL handle"
        assert emu.base <= addr < emu.base + emu.pe.size_of_image, what + ": handle not inside the DLL's static pools"
        assert emu.r32(addr) == NULL_OBJ_MAGIC and emu.r32(addr + 4) == KIND[kind], what + ": wrong object kind"

    def out_ptr(nbytes=4):
        a = emu.alloc(nbytes + 16)          # 8 guard bytes either side (0xAA) so overruns are caught
        return a + 8

    def guard_ok(a, nbytes, what):
        assert emu.rb(a - 8, 8) == b"\xAA" * 8 and emu.rb(a + nbytes, 8) == b"\xAA" * 8, what + ": out buffer overrun"

    # --- System_Create (C export, one argument)
    pp = out_ptr()
    c("FMOD_System_Create", [("i", pp)], [(pp, 4)])
    system = emu.r32(pp)
    handle(system, "system", "System_Create")
    guard_ok(pp, 4, "System_Create")

    # --- version gate (SoundSystemFMOD::createSystem: version >= 0x00010904 or the engine skips init)
    vp = out_ptr()
    c("?getVersion@System@FMOD@@QAA?AW4FMOD_RESULT@@PAI@Z", [("i", system), ("i", vp)], [(vp, 4)])
    ver = emu.r32(vp)
    assert ver >= FMOD_VERSION_MIN and ver == 0x00010904, "version 0x%X" % ver
    guard_ok(vp, 4, "getVersion")

    # --- driver selection: getNumDrivers==0 so the game never loops, but the other entries must be safe anyway
    np_ = out_ptr()
    c("?getNumDrivers@System@FMOD@@QAA?AW4FMOD_RESULT@@PAH@Z", [("i", system), ("i", np_)], [(np_, 4)])
    assert emu.r32(np_) == 0
    guard_ok(np_, 4, "getNumDrivers")
    name_buf, guid_buf = out_ptr(0x80), out_ptr(16)
    rate_p, mode_p, ch_p = out_ptr(), out_ptr(), out_ptr()
    # exactly the stack layout of the game's call (5a38d2): [sp]=guid, others NULL in the game; here all provided
    c("?getDriverInfo@System@FMOD@@QAA?AW4FMOD_RESULT@@HPADHPAUFMOD_GUID@@PAHPAW4FMOD_SPEAKERMODE@@2@Z",
      [("i", system), ("i", 0), ("i", name_buf), ("i", 0x80), ("i", guid_buf), ("i", rate_p), ("i", mode_p), ("i", ch_p)],
      [(name_buf, 0x80), (guid_buf, 16), (rate_p, 4), (mode_p, 4), (ch_p, 4)])
    assert emu.rb(name_buf, 12) == b"Null Output\0", emu.rb(name_buf, 12)
    assert emu.rb(guid_buf, 16) == b"\0" * 16
    assert (emu.r32(rate_p), emu.r32(mode_p), emu.r32(ch_p)) == (48000, 3, 2)
    for a, n in ((name_buf, 0x80), (guid_buf, 16), (rate_p, 4), (mode_p, 4), (ch_p, 4)):
        guard_ok(a, n, "getDriverInfo")
    # tiny name buffer: must truncate and NUL-terminate, never overrun
    small = out_ptr(4)
    c("?getDriverInfo@System@FMOD@@QAA?AW4FMOD_RESULT@@HPADHPAUFMOD_GUID@@PAHPAW4FMOD_SPEAKERMODE@@2@Z",
      [("i", system), ("i", 0), ("i", small), ("i", 4), ("i", 0), ("i", 0), ("i", 0), ("i", 0)], [(small, 4)])
    assert emu.rb(small, 4) == b"Nul\0"
    guard_ok(small, 4, "getDriverInfo(small)")
    c("?setDriver@System@FMOD@@QAA?AW4FMOD_RESULT@@H@Z", [("i", system), ("i", 0)])
    c("?setOutput@System@FMOD@@QAA?AW4FMOD_RESULT@@W4FMOD_OUTPUTTYPE@@@Z", [("i", system), ("i", 0)])

    # --- init(0x20, 4, NULL)
    c("?init@System@FMOD@@QAA?AW4FMOD_RESULT@@HIPAX@Z", [("i", system), ("i", 0x20), ("i", 4), ("i", 0)])

    # --- setFileSystem: 4 callbacks (never called: unmapped addresses would fault), asyncread/cancel NULL, blockalign -1
    cb = [0x7BAD0000 + 0x100 * i + 1 for i in range(4)]
    c("?setFileSystem@System@FMOD@@QAA?AW4FMOD_RESULT@@P6A?AW43@PBDPAIPAPAXPAX@ZP6A?AW43@33@ZP6A?AW43@33I13@ZP6A?AW43@3I3@ZP6A?AW43@PAUFMOD_ASYNCREADINFO@@3@Z9H@Z",
      [("i", system), ("i", cb[0]), ("i", cb[1]), ("i", cb[2]), ("i", cb[3]), ("i", 0), ("i", 0), ("i", -1)])

    # --- set3DSettings(1.0, distance, 1.0)
    c("?set3DSettings@System@FMOD@@QAA?AW4FMOD_RESULT@@MMM@Z", [("i", system), ("f", 1.0), ("f", 2.0), ("f", 1.0)])

    # --- channel groups: "Sound Channel Group", "Music Channel Group", master; addGroup(master, group, true, NULL)
    g1p, g2p, mp = out_ptr(), out_ptr(), out_ptr()
    cg = "?createChannelGroup@System@FMOD@@QAA?AW4FMOD_RESULT@@PBDPAPAVChannelGroup@2@@Z"
    c(cg, [("i", system), ("i", ck("Sound Channel Group")), ("i", g1p)], [(g1p, 4)])
    c(cg, [("i", system), ("i", ck("Music Channel Group")), ("i", g2p)], [(g2p, 4)])
    c("?getMasterChannelGroup@System@FMOD@@QAA?AW4FMOD_RESULT@@PAPAVChannelGroup@2@@Z", [("i", system), ("i", mp)], [(mp, 4)])
    g1, g2, master = emu.r32(g1p), emu.r32(g2p), emu.r32(mp)
    for a, w in ((g1, "sound group"), (g2, "music group"), (master, "master group")):
        handle(a, "group", w)
    assert len({g1, g2, master}) == 3, "channel group handles must be distinct"
    for a, n in ((g1p, 4), (g2p, 4), (mp, 4)):
        guard_ok(a, n, "group out")
    ag = "?addGroup@ChannelGroup@FMOD@@QAA?AW4FMOD_RESULT@@PAV12@_NPAPAVDSPConnection@2@@Z"
    c(ag, [("i", master), ("i", g1), ("i", 1), ("i", 0)])
    c(ag, [("i", master), ("i", g2), ("i", 1), ("i", 0)])
    conn = out_ptr()
    emu.w32(conn, 0x12345678)
    c(ag, [("i", master), ("i", g1), ("i", 1), ("i", conn)], [(conn, 4)])
    assert emu.r32(conn) == 0, "addGroup must null the DSPConnection out value"

    # --- audio format the game uses to scale fades (getSoftwareFormat(&rate, NULL, NULL))
    sf = "?getSoftwareFormat@System@FMOD@@QAA?AW4FMOD_RESULT@@PAHPAW4FMOD_SPEAKERMODE@@0@Z"
    rp = out_ptr()
    c(sf, [("i", system), ("i", rp), ("i", 0), ("i", 0)], [(rp, 4)])
    rate = emu.r32(rp)
    assert rate == 48000
    guard_ok(rp, 4, "getSoftwareFormat")
    mp2, rp2, rawp = out_ptr(), out_ptr(), out_ptr()
    c(sf, [("i", system), ("i", rp2), ("i", mp2), ("i", rawp)], [(rp2, 4), (mp2, 4), (rawp, 4)])
    assert (emu.r32(rp2), emu.r32(mp2), emu.r32(rawp)) == (48000, 3, 2)

    # --- loadSound: createStream/createSound(name, mode, exinfo, &sound); the game accepts a sound unless
    #     getFormat's type is 9 (MPEG) or 0x12 (AUDIOQUEUE)
    sp1, sp2 = out_ptr(), out_ptr()
    c("?createStream@System@FMOD@@QAA?AW4FMOD_RESULT@@PBDIPAUFMOD_CREATESOUNDEXINFO@@PAPAVSound@2@@Z",
      [("i", system), ("i", ck("sounds/ambient/cave.ogg")), ("i", 0x08000008 | 0x10), ("i", 0), ("i", sp1)], [(sp1, 4)])
    exinfo = emu.alloc(0x94)
    c("?createSound@System@FMOD@@QAA?AW4FMOD_RESULT@@PBDIPAUFMOD_CREATESOUNDEXINFO@@PAPAVSound@2@@Z",
      [("i", system), ("i", ck("sounds/dig/stone1.ogg")), ("i", 0x08000A09), ("i", exinfo), ("i", sp2)], [(sp2, 4)])
    snd1, snd2 = emu.r32(sp1), emu.r32(sp2)
    handle(snd1, "sound", "createStream")
    handle(snd2, "sound", "createSound")
    assert snd1 != snd2
    guard_ok(sp1, 4, "createStream")
    guard_ok(sp2, 4, "createSound")
    c("?set3DMinMaxDistance@Sound@FMOD@@QAA?AW4FMOD_RESULT@@MM@Z", [("i", snd1), ("f", 1.0), ("f", 16.0)])
    nsub = out_ptr()
    c("?getNumSubSounds@Sound@FMOD@@QAA?AW4FMOD_RESULT@@PAH@Z", [("i", snd1), ("i", nsub)], [(nsub, 4)])
    assert emu.r32(nsub) == 0
    guard_ok(nsub, 4, "getNumSubSounds")
    sub = out_ptr()
    c("?getSubSound@Sound@FMOD@@QAA?AW4FMOD_RESULT@@HPAPAV12@@Z", [("i", snd1), ("i", 0), ("i", sub)], [(sub, 4)])
    handle(emu.r32(sub), "sound", "getSubSound")
    guard_ok(sub, 4, "getSubSound")
    gf = "?getFormat@Sound@FMOD@@QAA?AW4FMOD_RESULT@@PAW4FMOD_SOUND_TYPE@@PAW4FMOD_SOUND_FORMAT@@PAH2@Z"
    tp = out_ptr()
    c(gf, [("i", snd1), ("i", tp), ("i", 0), ("i", 0), ("i", 0)], [(tp, 4)])     # exactly the game's (&type, NULL, NULL, NULL)
    typ = emu.r32(tp)
    assert typ not in (9, 0x12), "getFormat type %d would make the game release the sound" % typ
    assert typ == 15
    guard_ok(tp, 4, "getFormat")
    fp_, chp, bp = out_ptr(), out_ptr(), out_ptr()
    c(gf, [("i", snd2), ("i", tp), ("i", fp_), ("i", chp), ("i", bp)], [(tp, 4), (fp_, 4), (chp, 4), (bp, 4)])
    assert (emu.r32(fp_), emu.r32(chp), emu.r32(bp)) == (2, 2, 16)
    c("?setMode@Sound@FMOD@@QAA?AW4FMOD_RESULT@@I@Z", [("i", snd1), ("i", 0x200000)])

    # --- play: playSound(sound, group, paused=1, &channel) then volume/pitch/3D/unpause
    chp1, chp2 = out_ptr(), out_ptr()
    ps = "?playSound@System@FMOD@@QAA?AW4FMOD_RESULT@@PAVSound@2@PAVChannelGroup@2@_NPAPAVChannel@2@@Z"
    c(ps, [("i", system), ("i", snd1), ("i", g1), ("i", 1), ("i", chp1)], [(chp1, 4)])
    c(ps, [("i", system), ("i", snd2), ("i", g2), ("i", 1), ("i", chp2)], [(chp2, 4)])
    ch1, ch2 = emu.r32(chp1), emu.r32(chp2)
    handle(ch1, "channel", "playSound#1")
    handle(ch2, "channel", "playSound#2")
    assert ch1 != ch2
    guard_ok(chp1, 4, "playSound")
    pos, vel = emu.vec3(1.0, 2.0, 3.0), emu.vec3(0.0, 0.0, 0.0)
    c("?set3DAttributes@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@00@Z", [("i", ch1), ("i", pos), ("i", vel), ("i", 0)])
    c("?setVolume@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@M@Z", [("i", ch1), ("f", 0.5)])
    c("?setPitch@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@M@Z", [("i", ch1), ("f", 1.0)])
    c("?setPaused@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@_N@Z", [("i", ch1), ("i", 0)])
    ip = emu.alloc(1 + 16) + 8
    c("?isPlaying@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@PA_N@Z", [("i", ch1), ("i", ip)], [(ip, 1)])
    assert emu.rb(ip, 1) == b"\x00", "isPlaying must report false (one byte)"
    guard_ok(ip, 1, "isPlaying")

    # --- fade-out (SoundSystemFMOD::vf27): getSoftwareFormat, getDSPClock(NULL,&clock), addFadePoint x2, setDelay
    clk, clk2 = out_ptr(8), out_ptr(8)
    emu.uc.mem_write(clk, b"\xFF" * 8)
    dsp = "?getDSPClock@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@PA_K0@Z"
    c(dsp, [("i", ch1), ("i", 0), ("i", clk)], [(clk, 8)])
    assert emu.r64(clk) == 0
    guard_ok(clk, 8, "getDSPClock")
    c(dsp, [("i", ch1), ("i", clk2), ("i", clk)], [(clk, 8), (clk2, 8)])
    assert emu.r64(clk2) == 0 and emu.r64(clk) == 0
    clock = emu.r64(clk)
    fade = "?addFadePoint@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@_KM@Z"
    c(fade, [("i", ch1), ("q", clock), ("f", 1.0)])
    c(fade, [("i", ch1), ("q", clock + rate // 2), ("f", 0.0)])
    c("?setDelay@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@_K0_N@Z", [("i", ch1), ("q", 0), ("q", clock + rate // 2), ("i", 1)])
    c("?setMute@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@_N@Z", [("i", master), ("i", 1)])
    c("?setMute@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@_N@Z", [("i", master), ("i", 0)])

    # --- listener
    fwd, up = emu.vec3(0, 0, -1), emu.vec3(0, 1, 0)
    c("?set3DNumListeners@System@FMOD@@QAA?AW4FMOD_RESULT@@H@Z", [("i", system), ("i", 1)])
    c("?set3DListenerAttributes@System@FMOD@@QAA?AW4FMOD_RESULT@@HPBUFMOD_VECTOR@@000@Z",
      [("i", system), ("i", 0), ("i", pos), ("i", vel), ("i", fwd), ("i", up)])

    # --- frame update, stop, release, suspend/resume, shutdown
    c("?update@System@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", system)])
    c("?stop@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", ch1)])
    c("?stop@ChannelControl@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", ch2)])
    c("?release@Sound@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", snd1)])
    c("?release@Sound@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", snd2)])
    c("?mixerSuspend@System@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", system)])
    c("?mixerResume@System@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", system)])
    c("?update@System@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", system)])
    c("?close@System@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", system)])
    c("?release@System@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", system)])
    # a second System_Create after release must work (same static handle is fine)
    pp2 = out_ptr()
    c("FMOD_System_Create", [("i", pp2)], [(pp2, 4)])
    handle(emu.r32(pp2), "system", "System_Create #2")
    Counter.ok("[%s] realistic sequence System_Create -> getVersion -> init -> setFileSystem -> groups -> createStream/"
               "createSound -> playSound -> channel getters/setters -> fade -> release -> close, all FMOD_OK" % tag)

    # --- stress: pool wrap-around (createSound from game worker threads is lock-free round-robin)
    seen = set()
    for i in range(5000):
        p = out_ptr()
        c("?createSound@System@FMOD@@QAA?AW4FMOD_RESULT@@PBDIPAUFMOD_CREATESOUNDEXINFO@@PAPAVSound@2@@Z",
          [("i", system), ("i", GARBAGE), ("i", 0x08000A09), ("i", GARBAGE), ("i", p)], [(p, 4)])
        s = emu.r32(p)
        handle(s, "sound", "createSound stress")
        seen.add(s)
        if i % 4 == 0:
            c("?release@Sound@FMOD@@QAA?AW4FMOD_RESULT@@XZ", [("i", s)])
    assert len(seen) >= 4096, "only %d distinct sound handles over 5000 loads" % len(seen)
    chs = set()
    for i in range(1500):
        p = out_ptr()
        c(ps, [("i", system), ("i", snd1), ("i", g1), ("i", 1), ("i", p)], [(p, 4)])
        handle(emu.r32(p), "channel", "playSound stress")
        chs.add(emu.r32(p))
    assert len(chs) >= 1024
    Counter.ok("[%s] 5000 createSound + 1500 playSound: handles valid, distinct up to the pool size (%d sounds / %d channels), "
               "wrap-around is safe" % (tag, len(seen), len(chs)))


def sweep(emu, tag):
    """NULL-tolerance: NULL for every out pointer, unmapped garbage for every other pointer incl. `this`"""
    n = 0
    for mangled in sorted(emu.pe_exports):
        key = cpp(mangled)
        args = []
        for k in SIGS[key]:
            if k == "this":
                args.append(("i", GARBAGE))
            elif k == "in":
                args.append(("i", GARBAGE + 0x10))
            elif k == "out":
                args.append(("i", 0))
            elif k == "i":
                args.append(("i", 0x20))
            elif k == "f":
                args.append(("f", 0.5))
            elif k == "q":
                args.append(("q", 0x0000000100000002))
        emu.call(mangled, args)
        n += 1
    assert n == 39
    Counter.ok("[%s] NULL-tolerance sweep: all 39 exports OK with NULL out-pointers, garbage `this` and garbage in-pointers "
               "(no dereference, no stray writes)" % tag)


def run_dll(dll_path, base, tag):
    emu = load_dll(dll_path, base)
    types = emu.reloc_stats
    print("== %s: DLL mapped at 0x%08X (preferred 0x%08X, delta 0x%08X), relocation entries by type %s ==" % (
        tag, base, emu.pe.image_base, (base - emu.pe.image_base) & 0xFFFFFFFF, types))
    emu.addr2name = {emu.export_addr(n): n for n in emu.pe_exports}
    functional(emu, tag)
    sweep(emu, tag)
    missing = set(emu.pe_exports) - emu.called
    assert not missing, "exports never called: %s" % sorted(missing)
    Counter.ok("[%s] every one of the 39 exports was called (%d distinct)" % (tag, len(emu.called)))
    # statics really live in the relocated image (a missed relocation would have faulted above); report where
    sysp = emu.alloc(8)
    emu.call("FMOD_System_Create", [("i", sysp)], [(sysp, 4)])
    print("  System handle = 0x%08X (inside image 0x%08X..0x%08X)" % (emu.r32(sysp), base, base + emu.pe.size_of_image))
    return emu


def main(argv):
    dll = Path(argv[1]) if len(argv) > 1 else Path(os.environ.get("FMOD_NULL_DLL", DEFAULT_DLL))
    if not dll.exists():
        print("ERROR: %s not found - run build_fmod_null_arm32.bat first" % dll)
        return 2
    print("fmod_null_test: %s (%d bytes)" % (dll, dll.stat().st_size))
    print("-- static checks")
    pe = static_checks(dll)
    check_layouts()
    print("-- emulation (Unicorn ARM/Thumb-2, AAPCS-VFP)")
    run_dll(dll, pe.image_base, "preferred base")
    run_dll(dll, 0x5A230000, "relocated base")
    print("ALL %d CHECKS PASSED" % Counter.n)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except AssertionError as e:
        print("FAIL: %s" % e)
        sys.exit(1)
