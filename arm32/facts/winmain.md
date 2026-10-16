# winmain: bootstrap call site + PE patch plan (Minecraft 1.2.8 ARM32 -> ARM patcher)

Machine-readable: `winmain.json` (36 facts: 29 high / 6 medium / 1 low). Reference implementation + proof harness:
`arm32/facts/winmain_verify.py` (builds the patched image in memory, asserts every original byte, capstone round-trip,
unicorn smoke test at two image bases). All RVAs even code addresses; file offsets given where bytes are written.

## 1. The call site
```
entry 0xB49B29 -> sub_b49b28 (bl __security_init_cookie 0xb49cf8; b.w 0xb49a24)
sub_b49a24 = __scrt_common_main_seh: _initterm_e / _initterm, then
  0xB49AC2 mov r2,r0          ; r0 = _get_narrow_winmain_command_line()
  0xB49AC4 ldr r0,[pc]        ; 0x00400000 (HIGHLOW-relocated __ImageBase)
  0xB49AC6 mov r3,r4          ; r4 = show mode (0)
  0xB49AC8 movs r1,#0
  0xB49ACA bl  #0xB4A424      <<< REPLACE (bytes 00 f0 ab fc, file offset 0xB48ECA)
  0xB49ACE mov r4,r0          ; exit code
  0xB49AD0 bl 0x8b980 (return-0 stub); cbnz; mov r0,r4; bl exit
```
* Only the 4-byte `bl` changes (the x86 patch had to replace 12 bytes because the 3 pushes were part of the sequence).
* Replaced callee: `0xB4A424` (forwarder) -> `0xB4A574` (C++/CX `main(Array<String^>^)` wrapper: GetCmdArguments, Array<String^>) -> `0xFF6A0`
  (user main, builds the IFrameworkViewSource) -> `0x596328` = `CoreApplication::Run` (GetActivationFactoryByPCWSTR
  "Windows.ApplicationModel.Core.CoreApplication", IID 0AACF7A4-5E1D-49DF-8034-FB6A68BC5ED1, vtable slot 13 / offset 0x34).
  Win32Bootstrap replaces the whole chain; none of it is called by the host.
* ABI: r0..r3 (hInstance, 0, cmdline, 0) are **not read** by the callee, so `int Win32Bootstrap(void)` is fine. No stack args to clean.
  Return value -> `mov r4,r0` -> `exit(r4)`; LR (= 0xB49ACF) must reach Win32Bootstrap unchanged (tail-jump trampoline does that).
* CRT state: the C++/CX `InitializeData` runs as the last `_initterm_e` entry (0xB48C08) before this call, exactly like x86.

## 2. Replacement design (verified)
BL reaches +-16 MiB, but the target is a DLL function behind an IAT cell in a data section, so a code trampoline is required.
Verified-unused slack at the end of `.text`: RVA **0xC36EA0..0xC37000** (file 0xC362A0..0xC36400, 0x160 zero bytes; no reloc, no .pdata, no pointers).

| item | value |
|---|---|
| call site (file 0xB48ECA) | `00 f0 ab fc` -> **`ed f0 e9 f9`** = `bl 0x00C36EA0` (displacement +971730) |
| trampoline (file 0xC362A0) | **`df f8 08 c0  fc 44  dc f8 00 c0  60 47  94 95 af 00`** |
| decoded | `ldr.w ip,[pc,#8]` / `add ip,pc` / `ldr.w ip,[ip]` / `bx ip` / `.word 0x00AF9594` |
| literal | `cell_rva - (0xC36EA0 + 8)` = 0x0173043C - 0xC36EA8 (a displacement: correct at any base, **no reloc, DYNAMIC_BASE can stay**) |
| IAT cell | RVA 0x0173043C in the new `.d3imp` section (loader fills it with `win32craft.dll!Win32Bootstrap`) |

Proofs: keystone assembled, capstone disassembled back (identical), unicorn ran `bl -> trampoline -> cell -> stub -> mov r4,r0` at image base
0x400000 and 0x1C3A0000 (r4 = stub result, LR = site+5 both times). Alternatives recorded in the JSON: absolute-literal variant
(`... 00 00 3c 04 b3 01`, only valid at base 0x400000, fails when rebased) and a `.d3tr` RX-section fallback (SizeOfImage 0x01732000).

## 3. PE patch plan (all offsets in the original file)
| field | file off | old -> new | note |
|---|---|---|---|
| Characteristics | 0x156 | 0x0122 (assert only) | LARGE_ADDRESS_AWARE already set |
| OS / subsystem version | 0x180 / 0x188 | 6.2 keep | do NOT port the x86 minor 2->1 writes |
| DllCharacteristics | 0x19E | **0x9140 -> 0x8140** | clear APPCONTAINER (required); keep DYNAMIC_BASE (design is base independent) and NX_COMPAT; never set GUARD_CF |
| NumberOfSections | 0x146 | 7 -> 8 | header slot 0x350..0x377 (SizeOfHeaders 0x400, zeros to 0x3FF) |
| SizeOfInitializedData | 0x160 | 0xAF5800 -> 0xAF5E00 | +0x600 |
| SizeOfCode / Checksum | 0x15C / 0x198 | unchanged (0xC36000 / 0) | trampoline lives in existing .text |
| **SizeOfImage** | 0x190 | 0x01730000 -> **0x01731000** | **IMAGE_SIZE_128_ARM_PATCHED** (stable for any .d3imp < 4 KiB) |
| import dir dd[1] | 0x1C0 | (0x13269A8, 0x3FC) -> (0x01730000, 0x410) | 50 descriptors copied verbatim + win32craft + null |
| LOAD_CONFIG dd[10] | - | untouched | 0x11859A8/0x68: cookie VA 0x1737F94, no SEH table, GuardFlags 0x100, CFG check cell 0xC37BF8 -> empty stub; no GUARD_CF in DllCharacteristics => CFG off |
| TLS dd[9], reloc dd[5], .pdata dd[3], delay dd[13], IAT dd[12], export dd[0] | - | untouched | trampoline needs no .pdata (leaf tail-jump, never on stack); no new relocs needed |

`.d3imp` (RVA 0x01730000, file 0x01680A00 = EOF, flags 0xC0000040, VirtualSize 0x444, raw 0x600):
descriptors @0 (0x410 bytes), `win32craft.dll` @0x410, hint/name `Win32Bootstrap` @0x420, ILT @0x434, **IAT cell @0x43C**.
The exe has no overlay and no signature (dd[4]=0), file length 0x1680A00.

**Not needed on ARM** (reason): KERNEL32 placeholder ILT rewrite and the four compat descriptors (ARM imports ResolveDelayLoadedAPI / DelayLoadFailureHook /
CreateFile2 from api-ms-win-core-delayload-l1-1-1/-1-0 and file-l1-2-0, native on Win8+); redirect of dxgi/d3d11/api-set descriptors to win32craft.dll
(no dxgi import; d3d11 cell 0xC37A50 is patched at runtime; sets exist natively); `fmod_X86.dll` rename; the x86 12-byte `ff 15` patch and file offset
0x10293A2; the 27-descriptor/0x230 import-size check (ARM: 50 / 0x3FC).

### In-place DLL name renames (every occurrence found; NUL-pad to old length)
| string | count | file off | import desc |
|---|---|---|---|
| `vccorlib140_app.DLL` -> `vccorlib140.DLL` | 1 | 0x1328CDE | #24, 63 imports (all exported by ARM redist vccorlib140.dll) |
| `MSVCP140_APP.dll` -> `MSVCP140.dll` | 1 | 0x132C3C8 | #25, 258 imports (0 missing) |
| `CONCRT140_APP.dll` -> `CONCRT140.dll` | 1 | 0x132CD0A | #26, 43 imports (0 missing) |
| `VCRUNTIME140_APP.dll` -> `VCRUNTIME140.dll` | 1 | 0x132CE36 | #27, 19 imports (0 missing) |
| `fmod_ARM.dll` | 1 (x86 had 2) | 0x13277EA | #1, 39 imports; **no rename needed** (shipped file has that name) |
| `XINPUTUAP.dll` | 1 | 0x1326DAE | #0; optional -> `XInput1_4.dll` if target lacks XInputUap.dll |
No UTF-16 copies of any of these exist.

Input validation: SHA-256 of the unmodified ARM exe = **b01d5f1237d2daad8cc076d7218f0788751b55a9576c779ccdba08eef6fcfcf8**, 23,595,520 bytes.
Reference output of `winmain_verify.py` (PIC variant, no optional renames): SHA-256 b84df02c69b8d4d115c29d1f7909d92e0b7514fa3626004d184220d936554b6f, 0x1681000 bytes.

## 4. Win32Bootstrap accounting (1.2.8 path)
* Signature: `int Win32Bootstrap(void)`, export undecorated `Win32Bootstrap`; image identification via `*(DWORD*)(image+e_lfanew+0x50)` (offsets valid on ARM PE32) == 0x01731000; drop all other IMAGE_SIZE_* branches.
* IAT cells (by name): CxxThrow 0xC374E4, CreateFile2 0xC3765C, GetActivationFactoryByPCWSTR 0xC37BF0, Platform::Object ctor 0xC37B90, Heap::Allocate(II) 0xC37B80, Heap::Allocate(I) 0xC37B7C,
  GetIBoxArrayVtable 0xC37B88, GetIBoxVtable 0xC37BC8, D3DCompile 0xC370B4, D3D11CreateDevice 0xC37A50. Windows-7-only shims (DXGI Device3 IID, DiscardView, device2 queries, GetVersion==6.1) are not needed.
* x86 CONTEXT-based crash dump must be rewritten for ARM (Pc/Sp/Lr/R0-R12; no clean fp chain); everything else in the bootstrap function is portable C.

## Open questions
UCRT/api-set availability on the target OS (Win10 ARM32 yes, RT 8.1 no UCRT); fmod_ARM.dll imports VCRUNTIME140_APP.dll and is APPCONTAINER-flagged (consider patching it too);
nothing could be loaded on real ARM Windows (validated structurally + by emulation); whether the Wine-era vccorlib shims are still required with the real ARM vccorlib140.dll.
