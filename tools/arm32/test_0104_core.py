#!/usr/bin/env python3
"""Build and run the host-side (x86) self test of the 0.10.4 toolkit core:  python tools/arm32/test_0104_core.py [--src tools/arm32/test_0104_<unit>.c]
--src builds another harness with the same environment: an implementer may copy test_0104_core.c to tools/arm32/test_0104_<unit>.c, replace the stub
#include of their unit by the real unit file and unit-test its PURE logic (text diff, queues, event ordering ...) on the PC without a device.
Needs the RT LLVM (RT_LLVM_BIN), the MSVC x86 libs (VCLIB_X86) and the Windows SDK x86 libs (SDKROOT/SDKVER).  Exit code 0 = all checks passed."""
import os, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
LLVM = Path(os.environ.get("RT_LLVM_BIN", r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"))
VC = Path(os.environ.get("VCLIB_X86", r"D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\x86"))
SDKROOT = Path(os.environ.get("SDKROOT", r"D:\Windows Kits\10"))
SDKVER = os.environ.get("SDKVER", "10.0.19041.0")
SRC = REPO / "tools" / "arm32" / "test_0104_core.c"
if "--src" in sys.argv:
    SRC = (REPO / sys.argv[sys.argv.index("--src") + 1]).resolve()
OUT = REPO / "arm32" / "work" / ("_test_0104_" + SRC.stem)
OUT.mkdir(parents=True, exist_ok=True)

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, (r.stdout or "") + (r.stderr or "")

# The core harness tests the TOOLKIT against the pristine unit stubs (it supplies its own stand-ins for the queue and win32_host.c helpers);
# the real units are tested by their own harnesses (test_0104_<unit>.c, --src) and by try_0104.py. Core-only checks apply only to test_0104_core.c.
IS_CORE = SRC.name == "test_0104_core.c"
STUB_INC = ["-I" + str(REPO / "arm32" / "v0_10_4" / "stubs")] if IS_CORE else []
obj = OUT / (SRC.stem + ".obj")
exe = OUT / (SRC.stem + ".exe")
rc, out = run([str(LLVM / "clang.exe"), "--target=i686-pc-windows-msvc", "-O1", "-g0", "-fno-stack-protector", *STUB_INC, "-I" + str(REPO),
               "-Wno-incompatible-pointer-types", "-Wno-microsoft-cast", "-Wno-ignored-attributes", "-Wno-comment", "-Wno-pointer-sign",
               "-c", str(SRC), "-o", str(obj)])
errors = [l for l in out.splitlines() if "error" in l]
print("compile rc=%d, %d error lines" % (rc, len(errors)))
for l in errors[:40]:
    print("  ", l[:220])
if rc:
    sys.exit(2)

# compile-time tests of X104_SLOT / X104_SLOT_NOARGS: the right signatures compile, every wrong one is refused
slot_failures = 0
SLOT_TESTS = (("SLOT_TEST_POS", True), ("SLOT_TEST_NEG_ARGS", False), ("SLOT_TEST_NEG_FLOAT", False), ("SLOT_TEST_NEG_NOARGS", False), ("SLOT_TEST_NEG_NOTYPEDEF", False)) if IS_CORE else ()
for define, must_compile in SLOT_TESTS:
    srcrc, srcout = run([str(LLVM / "clang.exe"), "--target=i686-pc-windows-msvc", "-O1", "-g0", "-fno-stack-protector", *STUB_INC, "-I" + str(REPO), "-D" + define,
                         "-Wno-incompatible-pointer-types", "-Wno-microsoft-cast", "-Wno-ignored-attributes", "-Wno-comment", "-Wno-pointer-sign",
                         "-c", str(SRC), "-o", str(OUT / (define + ".obj"))])
    ok = (srcrc == 0) == must_compile
    print("slot macro test %-24s expected %s -> %s" % (define, "compile" if must_compile else "REFUSED", "ok" if ok else "WRONG (rc=%d)" % srcrc))
    if not ok:
        slot_failures += 1
        for l in [l for l in srcout.splitlines() if "error" in l][:5]:
            print("   ", l[:200])
if slot_failures:
    sys.exit(4)
um = SDKROOT / "Lib" / SDKVER / "um" / "x86"
ucrt = SDKROOT / "Lib" / SDKVER / "ucrt" / "x86"
rc, out = run([str(LLVM / "lld-link.exe"), "/subsystem:console", "/machine:x86", "/out:" + str(exe), str(obj),
               "/libpath:" + str(VC), "/libpath:" + str(um), "/libpath:" + str(ucrt), "libcmt.lib", "kernel32.lib", "user32.lib", "runtimeobject.lib",
               "ole32.lib", "oldnames.lib", "legacy_stdio_definitions.lib"])
print("link rc=%d" % rc)
if rc:
    print(out[:3000])
    sys.exit(3)
r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
sys.stdout.write(r.stdout)
sys.stdout.write(r.stderr)
print("test exit code", r.returncode)
sys.exit(r.returncode)
