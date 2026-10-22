#!/usr/bin/env python3
"""Host-side (x86) test of the 1 MiB log cap in win32_host.c:  python tools/arm32/test_logcap.py
Extracts the REAL arm32_log_rotate / arm32_log_restart code (the block between the two marker comments in win32_host.c), compiles it with a tiny main()
and checks: an oversized file is emptied at start, a leftover .old is deleted, and a run that reaches the cap is restarted with the head of the run kept.
Same toolchain variables as test_0104_core.py.  Exit code 0 = all checks passed."""
import os, re, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
LLVM = Path(os.environ.get("RT_LLVM_BIN", r"D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"))
VC = Path(os.environ.get("VCLIB_X86", r"D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\x86"))
SDKROOT = Path(os.environ.get("SDKROOT", r"D:\Windows Kits\10"))
SDKVER = os.environ.get("SDKVER", "10.0.19041.0")
OUT = REPO / "arm32" / "work" / "_test_logcap"
OUT.mkdir(parents=True, exist_ok=True)

host = (REPO / "win32_host.c").read_text(encoding="utf-8", errors="replace")
start = host.index("/* The file is append-only (several runs follow each other)")
end = host.index("static HANDLE arm32_log_open_file(void)")
block = host[start:end]

main_c = r'''
#include <windows.h>
#include <stdio.h>
#include <string.h>
''' + block + r'''
static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static DWORD size_of(const wchar_t *p) { WIN32_FILE_ATTRIBUTE_DATA d; return GetFileAttributesExW(p, GetFileExInfoStandard, &d) ? d.nFileSizeLow : 0xFFFFFFFFu; }
static void write_bytes(HANDLE f, DWORD n) { char buf[1024]; DWORD w, i; memset(buf, 'x', sizeof(buf)); buf[1022] = '\r'; buf[1023] = '\n';
    for (i = 0; i < n / 1024; ++i) WriteFile(f, buf, 1024, &w, NULL); }

int main(int argc, char **argv)
{
    wchar_t path[MAX_PATH], old_path[MAX_PATH];
    HANDLE f;
    DWORD w;
    char text[16384];
    DWORD got;
    (void)argc; (void)argv;
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"win32craft_logcap_test.log");
    lstrcpyW(old_path, path); lstrcatW(old_path, L".old");

    /* 1. an oversized file (2 MiB) and a stale .old are cleaned at start */
    DeleteFileW(path); DeleteFileW(old_path);
    f = CreateFileW(path, GENERIC_WRITE, 3, NULL, CREATE_ALWAYS, 0x80, NULL); write_bytes(f, 2u * 1024u * 1024u); CloseHandle(f);
    f = CreateFileW(old_path, GENERIC_WRITE, 3, NULL, CREATE_ALWAYS, 0x80, NULL); write_bytes(f, 4096); CloseHandle(f);
    CHECK(size_of(path) == 2u * 1024u * 1024u);
    arm32_log_rotate(path);
    CHECK(size_of(path) == 0);
    CHECK(size_of(old_path) == 0xFFFFFFFFu);

    /* 2. a file below the cap is kept (appending continues) */
    f = CreateFileW(path, GENERIC_WRITE, 3, NULL, CREATE_ALWAYS, 0x80, NULL); write_bytes(f, 300u * 1024u); CloseHandle(f);
    arm32_log_rotate(path);
    CHECK(size_of(path) == 300u * 1024u);

    /* 3. a run that reaches the cap: the head of the run survives, the rest is dropped */
    f = CreateFileW(path, 4 /* FILE_APPEND_DATA */, 3, NULL, OPEN_ALWAYS, 0x80, NULL);
    arm32_log_session_start = GetFileSize(f, NULL);
    CHECK(arm32_log_session_start == 300u * 1024u);
    WriteFile(f, "RUN-START line one\r\n", 20, &w, NULL);
    WriteFile(f, "RUN-START line two\r\n", 20, &w, NULL);
    write_bytes(f, 900u * 1024u);
    CHECK(GetFileSize(f, NULL) > ARM32_LOG_MAX_BYTES);
    arm32_log_restart(f, path);
    {
        DWORD after = GetFileSize(f, NULL);
        char message[120];
        wsprintfA(message, "size after restart: %lu (by attributes %lu)", (unsigned long)after, (unsigned long)size_of(path));
        printf("%s\n", message);
        CHECK(after > 0 && after < 16384u);
    }
    WriteFile(f, "AFTER restart\r\n", 15, &w, NULL);
    CloseHandle(f);
    {
        HANDLE r = CreateFileW(path, GENERIC_READ, 3, NULL, OPEN_EXISTING, 0x80, NULL);
        memset(text, 0, sizeof(text));
        ReadFile(r, text, sizeof(text) - 1, &got, NULL);
        CloseHandle(r);
    }
    CHECK(strstr(text, "restarted") != NULL);
    CHECK(strstr(text, "RUN-START line one") != NULL);
    CHECK(strstr(text, "RUN-START line two") != NULL);
    CHECK(strstr(text, "AFTER restart") != NULL);
    CHECK(strstr(text, "RUN-START line one") < strstr(text, "AFTER restart"));

    /* 4. a second restart keeps the same head and does not stack notes */
    f = CreateFileW(path, 4, 3, NULL, OPEN_ALWAYS, 0x80, NULL);
    write_bytes(f, 1100u * 1024u);
    arm32_log_restart(f, path);
    CloseHandle(f);
    {
        HANDLE r = CreateFileW(path, GENERIC_READ, 3, NULL, OPEN_EXISTING, 0x80, NULL);
        const char *p;
        int notes = 0;
        memset(text, 0, sizeof(text));
        ReadFile(r, text, sizeof(text) - 1, &got, NULL);
        CloseHandle(r);
        for (p = text; (p = strstr(p, "win32craft.log restarted")) != NULL; ++p) ++notes;
        CHECK(notes == 1);
        CHECK(strstr(text, "RUN-START line one") != NULL);
    }
    DeleteFileW(path);
    printf("%s\n", failures ? "LOGCAP TEST FAILED" : "LOGCAP TEST PASSED");
    return failures ? 1 : 0;
}
int _fltused;
'''
src = OUT / "test_logcap.c"
src.write_text(main_c, encoding="utf-8")

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, (r.stdout or "") + (r.stderr or "")

obj = OUT / "test_logcap.obj"
exe = OUT / "test_logcap.exe"
rc, out = run([str(LLVM / "clang.exe"), "--target=i686-pc-windows-msvc", "-O1", "-g0", "-fno-stack-protector", "-I" + str(REPO),
               "-isystem", str(SDKROOT / "Include" / SDKVER / "um"), "-isystem", str(SDKROOT / "Include" / SDKVER / "shared"),
               "-isystem", str(SDKROOT / "Include" / SDKVER / "ucrt"), "-isystem", str(VC.parent.parent / "include"),
               "-Wno-incompatible-pointer-types", "-Wno-microsoft-cast", "-Wno-ignored-attributes", "-Wno-comment", "-Wno-pointer-sign",
               "-c", str(src), "-o", str(obj)])
errors = [l for l in out.splitlines() if "error" in l]
print("compile rc=%d, %d error lines" % (rc, len(errors)))
for l in errors[:30]:
    print("  ", l[:240])
if rc:
    sys.exit(2)
um = SDKROOT / "Lib" / SDKVER / "um" / "x86"
ucrt = SDKROOT / "Lib" / SDKVER / "ucrt" / "x86"
rc, out = run([str(LLVM / "lld-link.exe"), "/subsystem:console", "/machine:x86", "/entry:mainCRTStartup", "/out:" + str(exe), str(obj),
               "/libpath:" + str(VC), "/libpath:" + str(um), "/libpath:" + str(ucrt), "libcmt.lib", "kernel32.lib", "user32.lib",
               "oldnames.lib", "legacy_stdio_definitions.lib"])
print("link rc=%d" % rc)
if rc:
    print(out[:3000])
    sys.exit(3)
r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
sys.stdout.write(r.stdout)
sys.stdout.write(r.stderr)
print("test exit code", r.returncode)
sys.exit(r.returncode)
