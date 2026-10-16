#!/usr/bin/env python3
"""Validate edit units without touching the repo sources.

    python tools/arm32/try_unit.py <unit> [<unit> ...] [--tag NAME] [--no-x86]

1. applies the units (in order) onto the PRISTINE sources (arm32/orig/*) into arm32/work/<tag>/
2. compiles win32_host.c and api_compat.c for ARM32 (-c, so inline-asm errors show up) and prints errors/warnings counts
3. compiles the edited files for x86 and compares their disassembly with the pristine x86 build: any difference means
   the unit changed the x86 build (it must not).
Use the unit name 00_core first if the unit depends on the compile-clean baseline."""
import sys, os, subprocess, hashlib, re, argparse
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import apply_edits

REPO = HERE.parent.parent
LLVM = Path(os.environ.get("RT_LLVM_BIN", r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"))
CLANG = str(LLVM / "clang.exe")
OBJDUMP = str(LLVM / "llvm-objdump.exe")

ARM_FLAGS = ["--target=thumbv7-pc-windows-msvc", "-march=armv7-a", "-mfpu=neon", "-mfloat-abi=hard", "-O2",
             "-fno-stack-protector", "-fno-builtin", "-ffunction-sections", "-ferror-limit=0",
             "-Wno-incompatible-pointer-types", "-Wno-microsoft-cast", "-Wno-ignored-attributes"]
X86_FLAGS = ["-target", "i686-w64-windows-gnu", "-O2", "-fno-stack-protector", "-fno-builtin", "-ferror-limit=0",
             "-Wno-incompatible-pointer-types", "-Wno-microsoft-cast", "-Wno-ignored-attributes"]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def compile_one(src, obj, flags, incs):
    cmd = [CLANG] + flags + ["-I" + i for i in incs] + ["-c", str(src), "-o", str(obj)]
    r = run(cmd)
    return r.returncode, r.stderr


def disasm_digest(obj):
    r = run([OBJDUMP, "-d", "-r", "-s", "-j", ".text", "-j", ".data", "-j", ".rdata", "--no-show-raw-insn", str(obj)])
    lines = [l for l in r.stdout.splitlines() if not l.startswith(str(obj)) and "file format" not in l]
    return hashlib.sha256("\n".join(lines).encode()).hexdigest(), len(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("units", nargs="+")
    ap.add_argument("--tag")
    ap.add_argument("--no-x86", action="store_true")
    ap.add_argument("--show", type=int, default=40, help="max error lines to print")
    a = ap.parse_args()
    tag = a.tag or "+".join(Path(u).name for u in a.units)[:60]
    wd = REPO / "arm32" / "work" / tag
    state, failures, applied = apply_edits.apply_units(a.units, workdir=str(wd))
    for t, e in failures:
        print("EDIT-FAIL", t, e)
    print("applied %d edits, %d failures" % (applied, len(failures)))
    wd.mkdir(parents=True, exist_ok=True)
    for name in ("win32_host.c", "api_compat.c"):
        src = apply_edits.pristine_path(name)
        if name not in state:
            state[name] = src.read_bytes().decode("utf-8")
    for t, txt in state.items():
        (wd / t).parent.mkdir(parents=True, exist_ok=True)
        (wd / t).write_bytes(txt.encode("utf-8"))
    incs = [str(wd), str(REPO)]
    ok_arm = True
    for name in ("win32_host.c", "api_compat.c"):
        rc, err = compile_one(wd / name, wd / (name + ".arm.obj"), ARM_FLAGS, incs)
        errs = [l for l in err.splitlines() if " error: " in l or " error:" in l]
        warns = [l for l in err.splitlines() if " warning: " in l]
        print("ARM  %-14s rc=%d errors=%d warnings=%d" % (name, rc, len(errs), len(warns)))
        for l in errs[:a.show]:
            print("   ", re.sub(r"^.*?(win32_host\.c|api_compat\.c)", r"\1", l)[:230])
        if rc != 0:
            ok_arm = False
    if not a.no_x86:
        xo = REPO / "arm32" / "work" / "_x86_pristine"
        xo.mkdir(parents=True, exist_ok=True)
        pristine_dir = REPO / "arm32" / "orig"
        for name in ("win32_host.c", "api_compat.c"):
            pobj = xo / (name + ".x86.obj")
            if not pobj.exists():
                rc, err = compile_one(pristine_dir / name, pobj, X86_FLAGS, [str(REPO)])
                if rc != 0:
                    print("pristine x86 compile failed:", err[:300])
            eobj = wd / (name + ".x86.obj")
            rc, err = compile_one(wd / name, eobj, X86_FLAGS, incs)
            if rc != 0:
                print("X86  %-14s COMPILE FAILED" % name)
                for l in [l for l in err.splitlines() if " error:" in l][:10]:
                    print("   ", l[:230])
                continue
            d1, n1 = disasm_digest(pobj)
            d2, n2 = disasm_digest(eobj)
            print("X86  %-14s %s (pristine %s.. edited %s..)" % (
                name, "IDENTICAL" if d1 == d2 else "DIFFERENT!", d1[:10], d2[:10]))
    sys.exit(0 if (ok_arm and not failures) else 1)


if __name__ == "__main__":
    main()
