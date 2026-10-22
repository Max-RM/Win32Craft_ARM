#!/usr/bin/env python3
"""Build and run the host-side (x86) test of the 0.10.4 platform unit:  python tools/arm32/test_0104_platform.py
Compiles tools/arm32/test_0104_platform.c (the REAL win32_host_arm32_0104_platform.inl + the pristine stubs of the other units + the on-device selftest block)
and runs it several times with different command lines / environments (default, --offline --bypass-*, --data-dir, unusable %APPDATA%, non-ASCII game folder).
Needs the same environment as test_0104_core.py (RT_LLVM_BIN, VCLIB_X86, SDKROOT/SDKVER).  Exit code 0 = all runs passed."""
import os, shutil, subprocess, sys, tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
LLVM = Path(os.environ.get("RT_LLVM_BIN", r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"))
VC = Path(os.environ.get("VCLIB_X86", r"D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\x86"))
SDKROOT = Path(os.environ.get("SDKROOT", r"D:\Windows Kits\10"))
SDKVER = os.environ.get("SDKVER", "10.0.19041.0")
SRC = REPO / "tools" / "arm32" / "test_0104_platform.c"
OUT = REPO / "arm32" / "work" / "_test_0104_platform"
OUT.mkdir(parents=True, exist_ok=True)


def run(cmd, env=None):
    r = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=180)
    return r.returncode, (r.stdout or "") + (r.stderr or "")


obj = OUT / "test_0104_platform.obj"
exe = OUT / "test_0104_platform.exe"
rc, out = run([str(LLVM / "clang.exe"), "--target=i686-pc-windows-msvc", "-O1", "-g0", "-fno-stack-protector", "-I" + str(REPO), "-Wno-incompatible-pointer-types",
               "-Wno-microsoft-cast", "-Wno-ignored-attributes", "-Wno-comment", "-Wno-pointer-sign", "-c", str(SRC), "-o", str(obj)])
errors = [l for l in out.splitlines() if "error" in l]
print("compile rc=%d, %d error lines" % (rc, len(errors)))
for l in errors[:40]:
    print("  ", l[:260])
if rc:
    sys.exit(2)
um = SDKROOT / "Lib" / SDKVER / "um" / "x86"
ucrt = SDKROOT / "Lib" / SDKVER / "ucrt" / "x86"
rc, out = run([str(LLVM / "lld-link.exe"), "/subsystem:console", "/machine:x86", "/out:" + str(exe), str(obj), "/libpath:" + str(VC), "/libpath:" + str(um),
               "/libpath:" + str(ucrt), "libcmt.lib", "kernel32.lib", "user32.lib", "runtimeobject.lib", "ole32.lib", "oldnames.lib", "legacy_stdio_definitions.lib"])
print("link rc=%d" % rc)
if rc:
    print(out[:3000])
    sys.exit(3)

tmp = Path(tempfile.mkdtemp(prefix="x104plat_"))
game = tmp / "game"
game.mkdir()
failed = 0


def case(name, args, appdata, mode, expect="", expect_rc=0):
    global failed
    env = dict(os.environ)
    env["X104T_GAME"] = str(game)
    env["X104T_APPDATA"] = appdata
    env["X104T_MODE"] = mode
    env["X104T_EXPECT"] = expect
    env["X104T_EXPECT"] = expect
    # one pre-formed command line string: the arguments already carry the quotes the host's host_next_argument() understands
    r = subprocess.run('"%s" %s' % (exe, " ".join(args)), capture_output=True, text=True, env=env, timeout=180)
    text = r.stdout + r.stderr
    fails = [l for l in text.splitlines() if "CHECK FAILED" in l or "FAIL]" in l]
    ok = r.returncode == expect_rc and not fails
    print("== case %-34s rc=%d %s" % (name, r.returncode, "ok" if ok else "FAILED"))
    if not ok or "-v" in sys.argv:
        sys.stdout.write(text)
    if not ok:
        failed += 1


try:
    appdata = tmp / "appdata"
    appdata.mkdir()
    case("default (APPDATA)", [], str(appdata), "A")
    case("offline + bypass flags", ["--offline", "--bypass-network", "--bypass-vibrate"], str(appdata), "A")
    case("--data-dir", ["--data-dir=" + str(tmp / "custom" / "dd")], str(appdata), "D", str(tmp / "custom" / "dd"))
    case("--data-dir quoted with space", ['"--data-dir=' + str(tmp / "quoted dir" / "x") + '"'], str(appdata), "D", str(tmp / "quoted dir" / "x"))
    case("unusable APPDATA -> LocalState", [], "Z:/no/such/drive", "F")
    case("APPDATA unset -> LocalState", [], "", "F")
    case("non-ASCII game folder", [], str(appdata), "N")
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("RESULT:", "OK" if not failed else "%d case(s) FAILED" % failed)
sys.exit(1 if failed else 0)
