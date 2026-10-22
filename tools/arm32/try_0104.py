#!/usr/bin/env python3
"""Compile check of the Minecraft PE 0.10.4.40 ARM32 host (win32_host_arm32_0104_*.inl).

    python tools/arm32/try_0104.py                     # the repo tree as it is: compile win32_host.c + api_compat.c for ARM32 (+ link, + x86 identity)
    python tools/arm32/try_0104.py --unit async        # unit check: the repo's async.inl + the PRISTINE STUBS of the other four units
    python tools/arm32/try_0104.py --unit input --no-link --no-x86
    python tools/arm32/try_0104.py --selftest          # also build + run the x86 host-side self test of the toolkit core (tools/arm32/test_0104_core.py)

Unit keys: async, app, panel, platform, input.  --unit builds a temporary copy of the tree in arm32/work/0104_<unit>/ where only the named
unit file comes from the repo; every other unit file comes from the snapshot arm32/v0_10_4/stubs/ (the stubs as designed), so parallel
implementers cannot break each other's compile check.  The compile uses exactly the flags of build_arm32.bat; the link step uses its
library list.  The output lists every ERROR, and every WARNING that mentions a win32_host_arm32_0104 file (other warnings only counted).
The x86 identity check compiles the (temp) win32_host.c/api_compat.c for x86 and compares with arm32/orig: any DIFFERENT! is a defect.
A LINT step runs first on every checked unit file (it is part of the pass/fail result):
  * a unit may use ONLY symbols of the core, the generated iids/header and win32_host.c - nothing defined in the boot (included last) or the selftest, and no private
    `x104_<other unit>_` symbol of another unit (only the cross-unit functions prototyped in core section 14b);
  * every vtable slot must be registered with X104_SLOT / X104_SLOT_NOARGS (signature-checked against the generated X104_FN_ typedef), never `[X104_SLOT_x_y] = fn`;
  * flag ownership: x104.f_quit / f_loop_exit only in the async unit, x104.f_exit_requested only in the app unit.
Exit code 0 = no ARM errors, lint clean, link ok, x86 identical."""
import argparse, os, re, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(HERE))
import try_unit as T

LLVM = Path(os.environ.get("LLVM_BIN", os.environ.get("RT_LLVM_BIN", r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin")))
VCLIB = Path(os.environ.get("VCLIB", r"D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\arm"))
SDKROOT = Path(os.environ.get("SDKROOT", r"D:\Windows Kits\10"))
SDKVER = os.environ.get("SDKVER", "10.0.19041.0")
UNITS = ["async", "app", "panel", "platform", "input"]
STUBS = REPO / "arm32" / "v0_10_4" / "stubs"

# build_arm32.bat CFLAGS, verbatim (plus -ferror-limit=0 so every error is listed)
CFLAGS = ["--target=thumbv7-pc-windows-msvc", "-march=armv7-a", "-mfpu=neon", "-mfloat-abi=hard", "-O2", "-fno-stack-protector", "-fno-builtin",
          "-ffunction-sections", "-Wno-incompatible-pointer-types", "-Wno-microsoft-cast", "-Wno-ignored-attributes", "-Wno-comment", "-ferror-limit=0",
          # extra diagnostics (no effect on the generated code): only the ones that mention a 0104 file are printed
          "-Wall", "-Wno-unused-function", "-Wno-unused-const-variable", "-Wno-unused-variable", "-Wno-unused-but-set-variable", "-Wno-parentheses",
          "-Wno-switch", "-Wno-missing-braces", "-Wno-pointer-sign", "-Wno-int-conversion", "-Wno-comment"]

TREE_FILES = ["win32_host.c", "api_compat.c", "win32_compat.h", "win32_host_116.inl", "win32_host_early2016.c", "win32_host_arm32.h", "win32_host_arm32_0104.h", "win32_host_arm32_0104.inl",
              "win32_host_arm32_0104_core.inl", "win32_host_arm32_0104_iids.inl", "win32_host_arm32_0104_selftest.inl", "win32_host_arm32_0104_boot.inl", "win32craft_arm32.def"]


def unit_file(unit):
    return "win32_host_arm32_0104_%s.inl" % unit


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, (r.stdout or "") + (r.stderr or "")


def build_tree(unit):
    """returns the directory to compile in"""
    if not unit:
        return REPO
    work = REPO / "arm32" / "work" / ("0104_" + unit)
    if work.exists():
        for f in work.iterdir():
            if f.is_file():
                f.unlink()
    work.mkdir(parents=True, exist_ok=True)
    for name in TREE_FILES:
        shutil.copyfile(REPO / name, work / name)
    for u in UNITS:
        src = (REPO / unit_file(u)) if u == unit else (STUBS / unit_file(u))
        if not src.exists():
            print("MISSING unit source:", src)
            sys.exit(2)
        shutil.copyfile(src, work / unit_file(u))
    return work


# ---------------------------------------------------------------------------------------------------------------------------------------------
# lint of the unit files
# ---------------------------------------------------------------------------------------------------------------------------------------------
def strip_c(text):
    """remove comments and string/char literals (keeps line structure)"""
    text = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(?:\.|[^"\\n])*"', '""', text)
    text = re.sub(r"'(?:\.|[^'\\n])*'", "' '", text)
    return text


def defined_names(text):
    """identifiers a C source DEFINES at file scope (macros, static functions, static variables, enumerators of file-scope enums)"""
    text = strip_c(text)
    names = set(re.findall(r"^[ \t]*#[ \t]*define[ \t]+(\w+)", text, re.M))
    names |= set(re.findall(r"^static\s[^;{}()]*?\b(\w+)\s*\(", text, re.M))                 # static functions
    names |= set(re.findall(r"^static\s[^;{}()]*?\b(\w+)\s*(?:=|;|\[)", text, re.M))            # static variables
    names |= set(re.findall(r"^X104_VTBL(?:_N)?\(\s*(\w+)", text, re.M))
    names |= set(re.findall(r"^X104_(?:CLASS_DEF|SINGLETON)\(\s*(\w+)", text, re.M))
    return {n for n in names if n.startswith(("x104_", "X104_"))}


def lint_units(tree, units):
    errors = []
    core = (tree / "win32_host_arm32_0104_core.inl").read_text(encoding="utf-8", errors="replace")
    iids = (tree / "win32_host_arm32_0104_iids.inl").read_text(encoding="utf-8", errors="replace")
    boot = (tree / "win32_host_arm32_0104_boot.inl").read_text(encoding="utf-8", errors="replace")
    selftest = (tree / "win32_host_arm32_0104_selftest.inl").read_text(encoding="utf-8", errors="replace")
    shared = defined_names(core) | defined_names(iids)
    boot_only = (defined_names(boot) | defined_names(selftest)) - shared
    # cross-unit contract = the functions prototyped in core section 14b
    contract = set(re.findall(r"\b(x104_\w+)\s*\(", core[core.index("14b. CROSS-UNIT API"):]))
    for unit in units:
        name = unit_file(unit)
        raw = (tree / name).read_text(encoding="utf-8", errors="replace")
        text = strip_c(raw)
        lines = text.splitlines()
        used = {}
        for number, line in enumerate(lines, 1):
            for ident in re.findall(r"\b[xX]104_\w+", line):
                used.setdefault(ident, number)
        for ident, number in sorted(used.items(), key=lambda kv: kv[1]):
            if ident in boot_only:
                errors.append("%s:%d: uses '%s', which is defined in the boot/selftest file (included AFTER the units): move it to the core or define it in your unit" % (name, number, ident))
            m = re.match(r"x104_(async|app|panel|platform|input)_", ident)
            if m and m.group(1) != unit and ident not in contract and ident not in shared:
                errors.append("%s:%d: uses '%s', a private symbol of the %s unit (only the cross-unit functions of core section 14b may be called)" % (name, number, ident, m.group(1)))
        for number, line in enumerate(lines, 1):
            if re.search(r"\[\s*X104_SLOT_\w+\s*\]\s*=", line):
                errors.append("%s:%d: raw '[X104_SLOT_...] = fn' initializer: register the thunk with X104_SLOT(Interface, Method, fn) (signature-checked) or X104_SLOT_NOARGS" % (name, number))
            if unit != "async" and re.search(r"\bf_(quit|loop_exit)\b", line):
                errors.append("%s:%d: x104.f_quit / f_loop_exit belong to the async unit (x104_ui_quit); the app unit's idempotency flag is x104.f_exit_requested" % (name, number))
            if unit != "app" and re.search(r"\bf_exit_requested\b", line):
                errors.append("%s:%d: x104.f_exit_requested belongs to the app unit" % (name, number))
    print("LINT %-14s %s" % (",".join(units), "clean" if not errors else "%d problem(s)" % len(errors)))
    for e in errors[:60]:
        print("   LINT", e)
    return len(errors)


def report(tag, output, show):
    errors, mine, other = [], [], 0
    for line in output.splitlines():
        if " error:" in line or " error :" in line:
            errors.append(line)
        elif " warning:" in line:
            if "win32_host_arm32_0104" in line:
                mine.append(line)
            else:
                other += 1
    print("ARM  %-14s errors=%d  warnings-in-0104-files=%d  other-warnings=%d" % (tag, len(errors), len(mine), other))
    for line in errors[:show]:
        print("   ERROR", line.strip()[:260])
        if "initializer element is not a compile-time constant" in line and "win32_host_arm32_0104" in line:
            print("         (if this line is an X104_SLOT / X104_SLOT_NOARGS: the thunk's C signature differs from the generated typedef X104_FN_<Interface>_<Method> - see units/SLOTS.md)")
    for line in mine[:show]:
        print("   WARN ", line.strip()[:260])
    return len(errors)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", choices=UNITS, help="check one unit file against the pristine stubs of the others")
    ap.add_argument("--no-link", action="store_true")
    ap.add_argument("--no-x86", action="store_true")
    ap.add_argument("--selftest", action="store_true", help="also run tools/arm32/test_0104_core.py (x86 host-side toolkit self test)")
    ap.add_argument("--integration", action="store_true", help="also run the x86 INTEGRATION test of the real tree (tools/arm32/test_0104_all.c: mock game + all five real units + the device self test; whole-tree only)")
    ap.add_argument("--show", type=int, default=60, help="max lines per category")
    a = ap.parse_args()

    tree = build_tree(a.unit)
    failed = 0
    failed += 1 if lint_units(tree, [a.unit] if a.unit else UNITS) else 0
    print("tree:", tree, "(unit %s from the repo, the others from arm32/v0_10_4/stubs)" % a.unit if a.unit else "(repo, all units as they are)")
    out_dir = REPO / "arm32" / "work" / ("0104_obj_" + (a.unit or "repo"))
    out_dir.mkdir(parents=True, exist_ok=True)
    clang = str(LLVM / "clang.exe")
    objs = []
    for name in ("win32_host.c", "api_compat.c"):
        obj = out_dir / (name + ".obj")
        rc, out = run([clang] + CFLAGS + ["-I" + str(tree), "-c", str(tree / name), "-o", str(obj)])
        failed += report(name, out, a.show)
        if rc != 0:
            failed += 1 if not any(" error:" in l for l in out.splitlines()) else 0
        objs.append(obj)
    if not failed and not a.no_link:
        um = SDKROOT / "Lib" / SDKVER / "um" / "arm"
        ucrt = SDKROOT / "Lib" / SDKVER / "ucrt" / "arm"
        dll = out_dir / "win32craft.dll"
        cmd = [str(LLVM / "lld-link.exe"), "/dll", "/machine:arm", "/entry:DllMain", "/def:" + str(tree / "win32craft_arm32.def"), "/out:" + str(dll),
               "/implib:" + str(out_dir / "win32craft.lib")] + [str(o) for o in objs] + [
               str(um / "kernel32.lib"), str(um / "user32.lib"), str(um / "shell32.lib"), str(um / "comdlg32.lib"), str(um / "armrt.lib"),
               str(VCLIB / "libcmt.lib"), str(VCLIB / "legacy_stdio_definitions.lib"), str(VCLIB / "legacy_stdio_wide_specifiers.lib"),
               str(ucrt / "ucrt.lib"), str(VCLIB / "vcruntime.lib"), "/nodefaultlib", "/subsystem:windows,6.02", "/nxcompat", "/brepro",
               "/stack:0x200000,0x1000", "/opt:ref", "/opt:icf"]
        env = dict(os.environ)
        env.pop("LINK", None)
        r = subprocess.run(cmd, capture_output=True, text=True, env=env)
        text = (r.stdout or "") + (r.stderr or "")
        if r.returncode:
            print("LINK FAILED")
            for line in text.splitlines()[:a.show]:
                print("   ", line[:260])
            failed += 1
        else:
            print("LINK ok  (%s, %d bytes)" % (dll.name, dll.stat().st_size))
    if not a.no_x86:
        xo = REPO / "arm32" / "work" / "_x86_pristine"
        xo.mkdir(parents=True, exist_ok=True)
        for name in ("win32_host.c", "api_compat.c"):
            pobj = xo / (name + ".x86.obj")
            if not pobj.exists():
                rc, err = T.compile_one(REPO / "arm32" / "orig" / name, pobj, T.X86_FLAGS, [str(REPO)])
                if rc:
                    print("pristine x86 compile failed:", err[:300])
            eobj = out_dir / (name + ".x86.obj")
            rc, err = T.compile_one(tree / name, eobj, T.X86_FLAGS, [str(tree)])
            if rc:
                print("X86  %-14s COMPILE FAILED" % name)
                for line in [l for l in err.splitlines() if " error:" in l][:10]:
                    print("   ", line[:230])
                failed += 1
                continue
            d1, _ = T.disasm_digest(pobj)
            d2, _ = T.disasm_digest(eobj)
            same = d1 == d2
            print("X86  %-14s %s" % (name, "IDENTICAL" if same else "DIFFERENT!"))
            if not same:
                failed += 1
    if a.selftest:
        rc, out = run([sys.executable, str(HERE / "test_0104_core.py")])
        tail = [l for l in out.splitlines() if l.startswith("==") or "FAILED" in l or "error" in l]
        print("SELFTEST", "ok" if rc == 0 else "FAILED rc=%d" % rc)
        for line in tail[:20]:
            print("   ", line[:200])
        failed += 1 if rc else 0
    if a.integration and not a.unit:
        rc, out = run([sys.executable, str(HERE / "test_0104_core.py"), "--src", "tools/arm32/test_0104_all.c"])
        tail = [l for l in out.splitlines() if l.startswith("==") or "CHECK FAILED" in l or "TIMEOUT" in l or "error" in l or l.startswith("selftest against")]
        print("INTEGRATION", "ok" if rc == 0 else "FAILED rc=%d" % rc)
        for line in tail[:20]:
            print("   ", line[:200])
        failed += 1 if rc else 0
    print("RESULT:", "OK" if not failed else "FAILED (%d problem(s))" % failed)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
