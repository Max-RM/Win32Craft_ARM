#!/usr/bin/env python3
"""Build the Windows RT 8.1 / Win10 app-local runtime for the ARM32 Win32Craft folder:

  * the SDK app-local Universal CRT (ucrtbase.dll + api-ms-win-crt-*/api-ms-win-core-* shims),
  * generated forwarder DLLs for every other api-set contract our binaries import (e.g. api-ms-win-core-heap-l2-1-0.dll),
    each function forwarded to KERNEL32 / OLE32 / ADVAPI32 / COMBASE exactly like Microsoft's own shim DLLs.

On Windows 8.1 an api-set name that the OS schema does not know is searched like a normal DLL, so these files (placed next
to the exe) fix "api-ms-win-... is missing" errors.  Where the OS schema knows the contract (Windows 10) the files are ignored.

    python tools\\make_arm32_apiset_shims.py [--out DIR] [--pe FILE ...]
Default PE set = every exe/dll in dist-arm32\\package (+ the SDK ucrtbase.dll).  Output default: dist-arm32\\win81-runtime\\."""
import argparse, os, re, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(HERE / "arm32"))
import arm32re
import apiset_scan

LLVM = Path(os.environ.get("LLVM_BIN", r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"))
SDKROOT = Path(os.environ.get("SDKROOT", r"D:\Windows Kits\10"))
SDKVER = os.environ.get("SDKVER", "10.0.19041.0")
UCRT_REDIST = SDKROOT / "Redist" / "ucrt" / "DLLs" / "arm"
UM_LIB = SDKROOT / "Lib" / SDKVER / "um" / "arm"
SKIP_PREFIX = ("api-ms-win-gaming-tcui",)   # Win10-only, delay-loaded cells are overwritten by the host before use
CFLAGS = ["--target=thumbv7-pc-windows-msvc", "-march=armv7-a", "-mfloat-abi=hard", "-O2", "-ffreestanding", "-fno-builtin"]


def lib_exports(lib):
    r = subprocess.run([str(LLVM / "llvm-nm.exe"), "--defined-only", str(lib)], capture_output=True, text=True)
    return set(m.group(1) for m in re.finditer(r"__imp_(\S+)", r.stdout))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(REPO / "dist-arm32" / "win81-runtime"))
    ap.add_argument("--pe", nargs="*")
    ap.add_argument("--work", default=str(REPO / "build-arm32" / "apiset-shims"))
    a = ap.parse_args()
    out, work = Path(a.out), Path(a.work)
    pes = [Path(p) for p in a.pe] if a.pe else apiset_scan.collect([REPO / "dist-arm32" / "package"])
    pes = [p for p in pes if p.suffix.lower() in (".exe", ".dll") and "fmod-silent" not in str(p)]
    pes.append(UCRT_REDIST / "ucrtbase.dll")
    print("scanning %d PE files" % len(pes))
    scan = apiset_scan.scan(pes)
    sdk_shims = {p.name.lower(): p for p in UCRT_REDIST.glob("*.dll")}
    k32, ole32, adv = (lib_exports(UM_LIB / n) for n in ("kernel32.lib", "ole32.lib", "advapi32.lib"))

    out.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)
    for old in list(out.glob("*.dll")) + list(out.glob("*.lib")):
        old.unlink()          # only our own generated output folder
    # 1. SDK app-local UCRT
    for name, p in sdk_shims.items():
        shutil.copy2(p, out / name)
    print("copied %d SDK UCRT files" % len(sdk_shims))

    # 2. generated forwarders for everything else (+ completion of SDK shims that miss imported functions)
    empty_c = work / "empty.c"
    empty_c.write_text("int win32craft_apiset_shim_marker;\n")
    empty_o = work / "empty.obj"
    subprocess.run([str(LLVM / "clang.exe")] + CFLAGS + ["-c", str(empty_c), "-o", str(empty_o)], check=True)
    generated, completed, problems = [], [], []
    for contract in sorted(scan):
        if contract.startswith(SKIP_PREFIX):
            print("skip (Win10-only, host-patched):", contract)
            continue
        needed = sorted(set().union(*scan[contract].values()))
        if any(n.startswith("#") for n in needed):
            problems.append("%s: ordinal imports not supported: %s" % (contract, [n for n in needed if n.startswith("#")]))
            continue
        present = set()
        if contract in sdk_shims:
            present = {n for n, _ in arm32re.Pe(str(sdk_shims[contract])).exports()}
            missing = [n for n in needed if n not in present]
            if not missing:
                continue            # SDK shim already exports everything we import
            completed.append((contract, missing))
        if contract == "api-ms-win-core-heap-l2-1-0.dll" and needed == ["LocalFree"]:
            # Real code instead of a forwarder: does not depend on how the OS's KERNEL32 exports LocalFree (Windows RT 8.1 report:
            # "procedure entry point LocalFree could not be located"); LocalFree of a LMEM_FIXED block == HeapFree on the process heap.
            src = work / "heap_l2_shim.c"
            src.write_text(
                "typedef unsigned int DWORD; typedef int BOOL; typedef void *HANDLE;\n"
                "extern HANDLE GetProcessHeap(void);\n"
                "extern BOOL HeapFree(HANDLE, DWORD, void *);\n"
                "void *win32craft_LocalFree(void *memory)\n"
                "{\n"
                "    if (!memory) return (void *)0;\n"
                "    return HeapFree(GetProcessHeap(), 0, memory) ? (void *)0 : memory;\n"
                "}\n")
            obj = work / "heap_l2_shim.obj"
            r = subprocess.run([str(LLVM / "clang.exe")] + CFLAGS + ["-c", str(src), "-o", str(obj)], capture_output=True, text=True)
            deffile = work / "api-ms-win-core-heap-l2-1-0.def"
            deffile.write_text("LIBRARY api-ms-win-core-heap-l2-1-0\r\nEXPORTS\r\nLocalFree=win32craft_LocalFree\r\n")
            r2 = subprocess.run([str(LLVM / "lld-link.exe"), "/dll", "/machine:arm", "/noentry", "/def:" + str(deffile),
                                 "/out:" + str(out / contract), "/implib:" + str(work / "api-ms-win-core-heap-l2-1-0.lib"),
                                 "/nodefaultlib", "/subsystem:windows,6.02", "/brepro", str(obj), str(UM_LIB / "kernel32.lib")],
                                capture_output=True, text=True)
            if r.returncode or r2.returncode:
                problems.append("heap-l2 code shim failed: %s %s" % (r.stderr[:200], r2.stderr[:200]))
            else:
                generated.append((contract + " (code shim: LocalFree -> HeapFree)", 1))
            continue
        entries = {}
        for fn in sorted(set(needed) | present):
            if fn in present and contract in sdk_shims:
                entries[fn] = None  # keep the SDK mapping (kernel32.<fn>)
            elif contract.startswith("api-ms-win-core-winrt"):
                entries[fn] = "COMBASE"
            elif fn in k32:
                entries[fn] = "KERNEL32"
            elif fn in ole32:
                entries[fn] = "OLE32"
            elif fn in adv:
                entries[fn] = "ADVAPI32"
            else:
                problems.append("%s: no forward target for %s" % (contract, fn))
        lines = ["LIBRARY %s" % contract, "EXPORTS"]
        for fn, tgt in sorted(entries.items()):
            lines.append("%s=%s.%s" % (fn, tgt or "KERNEL32", fn))
        base = contract[:-4]
        deffile = work / (base + ".def")
        deffile.write_text("\r\n".join(lines) + "\r\n")
        dll = out / contract
        r = subprocess.run([str(LLVM / "lld-link.exe"), "/dll", "/machine:arm", "/noentry", "/def:" + str(deffile),
                            "/out:" + str(dll), "/implib:" + str(work / (base + ".lib")), "/nodefaultlib", "/subsystem:windows,6.02", "/brepro", str(empty_o)],
                           capture_output=True, text=True)
        if r.returncode:
            problems.append("%s: link failed: %s" % (contract, r.stderr[:300]))
            continue
        generated.append((contract, len(entries)))

    # 3. verification: every api-set import of every PE must be satisfied by a file in `out` (except skipped contracts)
    shipped = {p.name.lower(): p for p in out.glob("*.dll")}
    unsat = []
    for contract, mods in scan.items():
        if contract.startswith(SKIP_PREFIX):
            continue
        if contract not in shipped:
            unsat.append("%s: no file shipped" % contract)
            continue
        exp = {n for n, _ in arm32re.Pe(str(shipped[contract])).exports()}
        for fn in sorted(set().union(*mods.values())):
            if fn not in exp:
                unsat.append("%s!%s not exported by the shipped shim" % (contract, fn))
    print("\ngenerated %d forwarder DLLs:" % len(generated))
    for c, n in generated:
        print("   %-48s %d exports" % (c, n))
    for c, miss in completed:
        print("   completed SDK shim %s with %s" % (c, ", ".join(miss)))
    for p in problems:
        print("PROBLEM:", p)
    for u in unsat:
        print("UNSATISFIED:", u)
    total = sum(f.stat().st_size for f in out.glob("*.dll"))
    print("\n%d files, %.2f MB in %s" % (len(list(out.glob("*.dll"))), total / 1e6, out))
    sys.exit(1 if (problems or unsat) else 0)


if __name__ == "__main__":
    main()
