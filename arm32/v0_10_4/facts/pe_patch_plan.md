# pe_patch_plan - PE structure, imports, patch points, table-driven patcher plan (MCPE 0.10.4.40 ARM32)

Machine-readable twin: `pe_patch_plan.json` (27 facts, `patch_profile` = the exact edit table, `import_table` = every import with its IAT RVA).
Everything below was re-derived from the stock exe with `arm32re.py` (ARM32_EXE set to the 0.10.4 exe) plus own PE parsers/capstone; the friend's numbers were only used as places to look.
A prototype applier of the table (scratch only, not in the repo) produced the patched exe and was byte-diffed against the friend's patched exe.

## 1. Verdict on the friend's PE claims
| claim | result |
|---|---|
| IAT `_CxxThrowException` 0x20B5D4, operator new 0x20B5E0, `GetActivationFactoryByPCWSTR` 0x20B5FC | CONFIRMED (also operator delete 0x20B5DC, thunks 0x200448 / 0x200430 / 0x200308) |
| call site RVA 0x200A72, `bl sub_203EFC`, bytes `03f043fa`, file 0x1FFE72 | CONFIRMED. It is the WinMain call of the VS2013 `__tmainCRTStartup`-like sub_200970, after all CRT/C++ init |
| trampoline 0xE6E23C, bytes `dff808c0 fc44 dcf800c0 6047` + literal | encoding CONFIRMED and correct, but it lives in an RWX section: replace (see 4) |
| `.d3imp` RVA 0xE6E000 / raw 0xE13600 / SizeOfImage 0xE6F000 / 24+1 descriptors | CONFIRMED (flags 0xE0000060 are the problem, not the geometry) |
| DllCharacteristics 0x9140 -> 0x8140 | CONFIRMED (file 0x17E) |
| no DLL renames, ship `*_app.dll` beside the exe | CONFIRMED and forced: no desktop ARM VC120 exists and `_exit_app` is not in desktop msvcr120 |
| friend's VC120 DLL copies are patched consistently | CONFIRMED, see 5 (only `0x1000` cleared; version 12.00.30113.0 = manifest MinVersion) |

## 2. Stock exe facts (all high confidence)
* PE32 ARMNT, EXE, ImageBase 0x400000, SizeOfImage 0xE6E000, entry 0x20095D (Thumb of 0x20095C), checksum 0, GUI, OS/subsys 6.2, Characteristics 0x0122, DllCharacteristics 0x9140 (TS-aware | APPCONTAINER | NX | DYNAMIC_BASE).
  e_lfanew 0x120, optional header 0x138, section table 0x218. No `.rsrc` (no manifest), no Authenticode, no delay/bound imports, load config 0x48 bytes (only SecurityCookie VA 0x11C9F60; no SafeSEH/CFG).
* Sections: `.text` 0x1000/vs 0x209364/raw 0x400/rs 0x209400 - `.rdata` 0x20B000 - `.data` 0x33F000 - `.pdata` 0xE35000 (12171 functions) - `.tls` 0xE4D000 (vs 9) - `.reloc` 0xE4E000 (vs 0x1FB10). File ends at 0xE13600. Header area 0x308..0x400 is zero: 6 more section headers fit.
* ASLR works: DYNAMIC_BASE, 61923 relocs (HIGHLOW 41574, THUMB_MOV32 20349); the friend log shows the game at base 0x540000. Nothing the patcher writes may be base dependent (BL is PC-relative, trampoline literal is an RVA delta, descriptors are RVAs).
* TLS dir 0x300F40: template 0x124D000..0x124D008 (8 bytes), index VA 0x122B240, callbacks array RVA 0x20BDBC whose first entry is NULL: no TLS callbacks, nothing to patch.
* Imports (24 descriptors): dxgi, D3DCOMPILER_47 (D3DCompile 0x20B000, D3DReflect 0x20B004), d3d11 (D3D11CreateDevice 0x20B1A4), XAudio2_8 (#1 XAudio2Create 0x20B05C), WS2_32 (19, ordinals), 16 api sets, and the three VC120 Phone DLLs:
  `vccorlib120_app.DLL` 50 fns IAT 0x20B5FC..0x20B6C0, `msvcp120_app.dll` 123 fns 0x20B1B4..0x20B39C, `msvcr120_app.dll` 149 fns 0x20B3A4..0x20B5F4. Every one of the 322 names exists in the shipped DLL export tables (0 missing; also 160/38/7 cross imports).
* WinRT activation: all 36 call sites use wrapper sub_2001E4 -> thunk 0x200308 -> IAT 0x20B5FC. ABI `HRESULT f(PCWSTR cls /*r0*/, Platform::Guid& iid /*r1*/, void** out /*r2*/)`.
* No SDIV/UDIV anywhere in the 692,763 disassembled instructions (Tegra-3 safe), medium confidence.

## 3. CRT startup path (what has already happened when the patch point is reached)
`0x20095C` (/GS cookie init sub_200E6C, cookie RVA 0xDC9F60) -> `b.w 0x200970`:
1. spin lock [0xE342C4], state [0xE342C8];
2. `_initterm_e(0x20BDA0, 0x20BDB4)`: {sub_200878 (EncodePointer(-1) stores, _fmode/_commode, setusermatherr; RoInitialize only if [0xE2B03C]!=0, it is 0), sub_2004F8 (onexit table), **sub_1FFECC (vccorlib120_app!InitializeData([0xDC9F5C]=2))**, sub_29BA0 (returns 0)};
3. `_initterm(0x20B6C8, 0x20BD9C)`: 437 slots / 436 non-null: sub_200910 (`__getmainargs`) then all C++ static initialisers (no direct path from any of them to WinRT activation found);
4. ANSI command-line tail -> r2; **`bl sub_203EFC` at 0x200A72** with r0 = hInstance (relocated literal), r1 = 0, r3 = 0; r0 result -> [0xE2B248], then `_exit_app`/`_cexit`.
`InitializeData(2)` = `RoInitialize(RO_INIT_MULTITHREADED)` (disassembled in the shipped vccorlib120_app; arg 1 = STA, 0 = skip). So the main thread is already an MTA and the host's own RoInitialize returns **S_FALSE (1)** - the friend log shows exactly that; it is not an error.
`sub_203EFC` = empty frame + `b.w sub_20405C` (the C++/CX main wrapper: GetCmdArguments -> `Array<String^>` -> sub_1F42A4 user main -> sub_1EECBC `Application::Start`).
`Application::Start` = `GetActivationFactory(L"Windows.UI.Xaml.Application", {06499997-F7B4-45FE-B763-7577D1D3CB4A})` then vtable slot 7 (+0x1C) with the callback delegate (calls at 0x1EED1A / 0x1EED30).
**Architecture consequence:** one patch point serves both (a) bypass and (b) faithful: the host's IAT activation hook can supply a fake `Windows.UI.Xaml.Application` statics whose slot 7 runs the callback, and Win32Bootstrap may call `ARM_CODE(image,0x203EFC)` to enter the untouched wrapper. No second exe patch is needed for (b).

## 4. Patcher profile (table-driven; JSON key `patch_profile`)
Input: sha256 `6c0a8adc...b29f3`, 14759424 bytes. Output: 14760448 bytes, SizeOfImage **0xE6F000** (the shared host's identification constant, machine 0x1C4), 7 sections, sha256 `453ce59a...db31` for exactly this table.

| id | file offset | old | new | what / why |
|---|---|---|---|---|
| hdr.numsections | 0x126 | 0600 | 0700 | add `.d3imp` |
| hdr.sizeofinitdata | 0x140 | 000ac600 | 000ec600 | += raw size 0x400 |
| hdr.sizeofimage | 0x170 | 00e0e600 | 00f0e600 | 0xE6E000 -> 0xE6F000 |
| hdr.dllchars | 0x17E | 4091 | 4081 | clear APPCONTAINER (0x9140 -> 0x8140) |
| hdr.importdir | 0x1A0 | 88ac3300 f4010000 | 00e0e600 08020000 | dir -> (0xE6E000, 0x208) |
| sec.text.virtualsize | 0x220 | 64932000 | 74932000 | .text VS 0x209364 -> 0x209374 (covers trampoline) |
| sec.header | 0x308 | 40 x 00 | `.d3imp` VS 0x23C RVA 0xE6E000 RS 0x400 raw 0xE13600 flags **0xC0000040** | new section header (RW data, not executable) |
| code.trampoline | 0x209764 (RVA 0x20A364) | 16 x 00 | `dff808c0 fc44 dcf800c0 6047 c83ec600` | PIC tail-jump through IAT cell 0xE6E234 (literal 0xC63EC8 = cell - (tramp+8)) |
| code.callsite | 0x1FFE72 (RVA 0x200A72) | 03f043fa | 09f077fc | `bl 0x203efc` -> `bl 0x20A364` |
| append | 0xE13600 | - | 0x400 bytes | `.d3imp`: 26 descriptors (24 byte copies + win32craft.dll!Win32Bootstrap + terminator) 0x000..0x208, name 0x208, hint 0x218, ILT 0x22C, IAT cell 0x234, zero padding |

Trampoline home: the zero padding right after the last literal pool in `.text` (last function sub_20A328 ends 0x20A35E, pool 0x20A360, VS end 0x20A364): 0x9C zero file bytes + zero mapped page tail to 0x20B000, 4-aligned, no relocation, no pdata, no pointer into it anywhere. Same idea as the device-proven 1.2.8 patcher; BL distance 0x98EE. The friend put it in an RWX `.d3imp` (works on their OS, risky on RT code integrity).
No renames, no XINPUTUAP/api-set redirects (not imported / natively served; HSTRING goes to the OS combase).

## 5. VC120 Phone runtime DLLs (ship beside the exe)
msvcr120_app.dll / msvcp120_app.dll / vccorlib120_app.dll in `TEST\MCPE_0.10.4.40`: ARMNT, ImageBase 0x10000000, DYNAMIC_BASE with relocations, no TLS, FileVersion **12.00.30113.0 PHONEREL** = AppxManifest MinVersion 12.0.30113.0. DllCharacteristics now 0x0140.
Edit table (dll_profiles): file 0x14E / 0x14E / 0x15E, `4011 -> 4001`. Provenance proof: PE header checksum == checksum recomputed with 0x1000 set (algorithm validated on kernel32.dll), so nothing else checksum-visible changed; Authenticode blobs (Microsoft Corporation / Windows Phone) remain but are now invalid (fine for the loader; needs the same relaxed code-integrity as the patched exe/fmod on RT).
The appx does not contain these DLLs (framework package); reconstructed-original sha256 are in the JSON.

## 6. Windows RT 8.1 loadability
* 32 api-set names are imported by exe + 3 DLLs + host; 24 are proven served on 6.3.9600 by `arm32/device_logs/loadcheck_rt81_6.3.9600_OK.txt`.
* **UNPROVEN (8):** errorhandling-l1-1-2 (exe), localization-l1-2-1 (exe + msvcr/msvcp/vccorlib), debug-l1-1-1, fibers-l1-1-1, heap-l1-2-0, libraryloader-l1-1-1, processenvironment-l1-2-0, rtlsupport-l1-2-0. Probably native (same winblue schema as WP8.1), but only a loadcheck run decides. Fix = forwarder shims via `tools/make_arm32_apiset_shims.py` (KERNELBASE/ntdll targets, avoid targets that forward back into the missing contract).
* win32craft.dll (friend build) needs ucrtbase.dll + api-ms-win-crt-string app-local: reuse the existing `win81-runtime` overlay.
* **loadcheck.c coverage:** works as is for everything except the hard-coded `modules[]` (1.2.8 names). Use `{Win32Craft.0.10.4-arm32.exe, win32craft.dll, vccorlib120_app.dll, msvcp120_app.dll, msvcr120_app.dll, ucrtbase.dll}`. It already LoadLibraries every imported DLL (runs the VC120 DllMains; loader rejects bad/APPCONTAINER DLLs there), resolves names and ordinals (XAudio2_8 #1, WS2_32). Gaps: missing module = silent skip (make it a failure); the patched exe itself is never mapped (optional `LoadLibraryExW(exe, DONT_RESOLVE_DLL_REFERENCES)` check, untested); runtime GetProcAddress by host/game is invisible.

## 7. What the host must do (PE level, ordered)
1. `Win32Bootstrap` is entered from the CRT on the main thread (r0..r3 = WinMain args, LR = 0x200A76|1); identify the image by machine 0x1C4 + SizeOfImage 0xE6F000.
2. All addresses = `GetModuleHandle(NULL) + RVA`; patch IAT cells (0x20B5D4, 0x20B5FC, 0x20B1A4, 0x20B000 ...) only after checking the cell points into the expected module; VirtualProtect RW, FlushInstructionCache.
3. RoInitialize result S_FALSE is success; allocate game objects through the cell value of operator new (0x20B5E0) so new/delete stay on one heap.
4. Return the exit code in r0 (it becomes the process exit code via the CRT).

## 8. Verification checklist (also in JSON `verification_checklist`)
V1 input hash; V2 byte diff touches only 0x126/0x140/0x170/0x17E/0x1A0/0x220/0x308-0x32F/0x1FFE72/0x209764-73 + 0x400 appended; V3 re-parse (7 sections, SizeOfImage 0xE6F000, dd[1] (0xE6E000,0x208), 25 imports, last = win32craft.dll!Win32Bootstrap, cell == hint RVA 0xE6E218, descriptors 0..23 byte-identical); V4 section table sane and `.d3imp` not executable; V5 BL decodes to 0x20A364, capstone decode of trampoline, literal + 0x20A36C == 0xE6E234; V6 no reloc/pdata over the trampoline; V7 DLL checks (dllchars, version, checksum rule, 0 missing exports); V8 optional unicorn run to the BL; V9 loadcheck on the RT; V10 first-run log lines (`SizeOfImage=00e6f000`, three hook lines with correct module names, RoInitialize 0x00000001).

## 9. Risks
* R1 8 unproven api sets on RT 8.1 (above). R2 code integrity: patched exe and Authenticode-broken DLLs need the relaxed policy already assumed for 1.2.8. R3 friend's RWX section if its patcher output is reused. R4 CRT/static init and real vccorlib120_app init were only seen working on the friend's OS (10.0.22631), not on RT 8.1; fallback: install hooks from `win32craft.dll!DllMain`. R5 the clean Microsoft VCLibs 12.0.30113.0 Phone appx is not in the repo (copies proven genuine by checksum). R6 no manifest: default DPI/GetVersion behaviour, decide in the host.

## 10. Not found / uncertain
* RT 8.1 behaviour of the 8 contracts, the VC120 DllMains and the 436 static constructors (never run there).
* Meaning of flag [0xE2B03C] (0 on disk, only gates an extra RoInitialize in sub_200878) and of sub_29BA0 as an XIA entry (returns 0; it is ICF-merged with `AddExternalServerScreen::vf16`, toolkit auto-name).
* Whether Windows RT accepts a mapped-as-DLL exe in the optional loadcheck idea (untested).
* "No static initialiser reaches activation" covers direct calls only.

Labels added with `arm32re.py name`: 0x200A72 (call site), 0x20A364 (trampoline slot), 0x1FFECC/0x1FFF84 (vccorlib init/uninit), 0x200878, 0x2004F8, 0x200E6C, 0x200910, tables 0x20BDA0 / 0x20B6C8, TLS 0x300F40 / 0x20BDBC, load config 0x300EF8, thunk 0x200308.
