#!/usr/bin/env python3
"""Build and run the host-side (x86) test of the app unit:  python tools/arm32/test_0104_app.py
Compiles tools/arm32/test_0104_app.c (the REAL win32_host_arm32_0104_app.inl against the pristine stubs of the other units and a mock game) and runs it.
Same toolchain variables as test_0104_core.py.  Exit code 0 = all checks passed."""
import os, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
LLVM = Path(os.environ.get("RT_LLVM_BIN", r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"))
VC = Path(os.environ.get("VCLIB_X86", r"D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\x86"))
SDKROOT = Path(os.environ.get("SDKROOT", r"D:\Windows Kits\10"))
SDKVER = os.environ.get("SDKVER", "10.0.19041.0")
SRC = REPO / "tools" / "arm32" / "test_0104_app.c"
OUT = REPO / "arm32" / "work" / "_test_0104_app"
OUT.mkdir(parents=True, exist_ok=True)

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, (r.stdout or "") + (r.stderr or "")

obj = OUT / "test_0104_app.obj"
exe = OUT / "test_0104_app.exe"
rc, out = run([str(LLVM / "clang.exe"), "--target=i686-pc-windows-msvc", "-O1", "-g0", "-fno-stack-protector", "-I" + str(REPO),
               "-Wno-incompatible-pointer-types", "-Wno-microsoft-cast", "-Wno-ignored-attributes", "-Wno-comment", "-Wno-pointer-sign",
               "-c", str(SRC), "-o", str(obj)])
errors = [l for l in out.splitlines() if "error" in l]
print("compile rc=%d, %d error lines" % (rc, len(errors)))
for l in errors[:40]:
    print("  ", l[:240])
if rc:
    sys.exit(2)
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
