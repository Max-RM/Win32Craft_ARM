# arm32-patcher - table-driven ARM32 PE patcher (notes, MCPE 0.10.4.40 + Minecraft 1.2.8)

Files (all new, nothing existing was touched):
* `tools/arm32-patcher.py` - the patcher (one file, Python 3.9+, standard library only; capstone is used for self-checks when present)
* `tools/arm32-patcher-selftest.py` - self test (198 checks, prints PASS/FAIL/SKIP)
* this file

Inputs were only read: the game folders, `D:\repo\win32craft\0.10.4.40\...`, the friend folder, `D:\repo\armdeps\...Appx`.

## 1. Usage

```
python tools\arm32-patcher.py detect <exe|folder> [--json F]
python tools\arm32-patcher.py patch  --profile auto|v0_10_4|v1_2_8 <game-folder> <out-folder> [options]
python tools\arm32-patcher.py probe  <exe> [--json F]          # unknown versions: leads only
python tools\arm32-patcher.py verify <patched exe|out folder> [--profile NAME]
python tools\arm32-patcher.py profiles [--dump NAME]           # list / print a profile as JSON
python tools\arm32-patcher.py dll    <in.dll> <out.dll>        # one companion DLL
python tools\arm32-patcher-selftest.py                         # exit 0 = all PASS
```

`patch` options: `--dll-name win32craft.dll` `--export Win32Bootstrap` (host DLL/export the call site is routed to),
`--no-fl91` / `--skip ID_OR_GROUP` (leave edits out), `--redirect-api-sets` (1.2.8), `--keep-winrt-init` (1.2.8 fmod),
`--checksum keep|recompute`, `--base-relocs keep|drop`, `--companion-dir DIR` (repeatable), `--companion-appx APPX`,
`--allow-hash-mismatch` (accept another exe SHA-256; every old-bytes check still applies), `--allow-unknown-companion`,
`--skip-companions`, `--host-dll FILE` (copy win32craft.dll into the output), `--redist-dir DIR` (1.2.8 VC140 DLLs),
`--exe-name`, `--keep-original-exe`, `--no-copy-folder` (write only patched files), `--force`, `--profile-file JSON` (global option, before the sub-command).

`patch` copies the game folder to `<out-folder>` (never into/over the input; refuses a non-empty out folder without `--force`), leaves out the stock exe (and the companion DLLs it replaces),
writes the patched exe as `Win32Craft.0.10.4-arm32.exe` plus the three VC120 DLLs, writes `patch_report.json` (every edit: id, file offset, RVA, size, old hex, new hex, reason; confinement check; all verification checks; companion provenance) and re-parses what it wrote from disk (50 checks for 0.10.4).
Exit code 0 = everything verified, 2 = patched but a companion is missing / a check failed, 1 = refused.

Typical 0.10.4 run (finds the genuine VC120 DLLs in `D:\repo\armdeps` on this machine; elsewhere pass `--companion-appx` or `--companion-dir`):
```
python tools\arm32-patcher.py patch --profile auto "D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]" out\mcpe0104 --host-dll dist-arm32\win32craft.dll
```

## 2. Engine and profile format

Everything version specific is data (`PROFILES` in the script, or a JSON file: `profiles --dump v0_10_4 > my.json`, edit, `--profile-file my.json`). The engine never contains an RVA.

| key | meaning |
|---|---|
| `status` | `complete` or `placeholder` (a placeholder is refused and its `todo` list is printed) |
| `identify` | exe names, manifest identity (Name/Version/arch/Executable), class-name markers: used by `detect` |
| `exe.input` | what the stock exe must be: sha256, size, machine, alignments, entry, SizeOfImage, Characteristics, DllCharacteristics, import dir + descriptor count, full section table. All checked before anything is written (sha256/section table can be bypassed together with `--allow-hash-mismatch`) |
| `exe.dll_characteristics` | `{expect, clear, set}` (APPCONTAINER 0x1000 cleared) |
| `exe.renames` | in-place import renames `{id, from, to}` located through the import descriptors (name must occur exactly as often as descriptors reference it) |
| `exe.redirects` | descriptors (by DLL name) whose name field is repointed to the host DLL; `required` always, `optional` with `--redirect-api-sets` |
| `exe.byte_patches` | `{id, rva|file_offset, old, new, reason, group?, context?}`: old bytes (and optional wider context) verified, relocation overlap refused |
| `exe.new_section` | name, **explicit characteristics (W+X is refused)**, and `expect_*` values (RVA, raw offset/size, VirtualSize, SizeOfImage) that are asserted, never assumed |
| `exe.import_add` | host DLL + exports added to the import directory (new IAT in the new section; original descriptors are copied byte for byte) |
| `exe.bootstrap` | `call_site {rva, kind bl|b.w, old, old_target, new}` and `trampoline {kind, section, rva, slack, expect_literal}`; template `pic_ldr_ip` |
| `exe.checksum`, `exe.base_relocs` | `keep|recompute`, `keep|drop` |
| `exe.expected_outputs` | known-good output SHA-256 per option variant (checked after every run) |
| `companions[]` | DLLs next to the exe: name, `sha256_original`, `sha256_patched`, `dll_characteristics {offset, expect, clear}`, `renames`, `byte_patches`, `structure` (extra allowed sections/imports), `appx_member` |
| `companion_sources.appx` | identity (Name/Version/arch) a VCLibs `.appx` must have to be used as the source of the originals |

Engine steps (in this order, all verified before writing, each recorded): not-already-patched check -> input identity -> DllCharacteristics -> renames -> byte patches -> new section header, NumberOfSections, SizeOfInitializedData, SizeOfImage, import directory, appended section data -> trampoline in the `.text` tail slack (must be 4-aligned, zero, file backed, beyond the declared VirtualSize, not under `.pdata`/`.reloc`) -> BL/B.W rewrite (T1/T4 encoding, +-16 MiB range check, round-trip decode, callee must equal `old_target`) -> `.text` VirtualSize bump -> relocs/checksum policy -> confinement check (the output differs from the input only inside the recorded edits) -> full re-parse verification.

Trampoline (PIC, 16 bytes, same as the device-proven 1.2.8 one, tail-jump so LR stays the call site + 4):
```
ldr.w ip,[pc,#8]   ; dff8 08c0   literal at +12 (needs a 4-aligned slot)
add   ip,pc        ; fc44        pc = tramp+8
ldr.w ip,[ip]      ; dcf8 00c0   ip = loader-filled IAT cell = export address (Thumb bit set)
bx    ip           ; 6047
.word cell_rva - (tramp_rva + 8)         ; a displacement, no relocation, valid at any base
```

## 3. v0_10_4 edit table (every offset recomputed from the actual PE; matches `facts/pe_patch_plan.json`)

Input `MCPE.Host.WindowsPhone.exe` sha256 `6c0a8adce0141323b8a43937043ffd0806dea71e9d11b74f614b39a2046b29f3`, 14759424 bytes, SizeOfImage 0xE6E000, 6 sections.

| id | file offset | RVA | old | new | what |
|---|---|---|---|---|---|
| hdr.dllchars | 0x17E | - | `4091` | `4081` | DllCharacteristics 0x9140 -> 0x8140 (APPCONTAINER cleared; TS-aware, NX, DYNAMIC_BASE stay) |
| fl91.profile_string | 0x2339F9 | 0x2351F9 | `33` | `31` | `_level_9_3` -> `_level_9_1` (context `5f6c6576656c5f395f3300` @ RVA 0x2351F0 verified). **Optional, group `fl91`, default ON** |
| sec.header | 0x308 | - | 40 x `00` | `.d3imp` VS 0x23C RVA 0xE6E000 RS 0x400 raw 0xE13600 flags **0xC0000040** | new section header (RW initialized data, not executable) |
| hdr.numsections | 0x126 | - | `0600` | `0700` | |
| hdr.sizeofinitdata | 0x140 | - | `000ac600` | `000ec600` | += 0x400 |
| hdr.sizeofimage | 0x170 | - | `00e0e600` | `00f0e600` | 0xE6E000 -> **0xE6F000** (host identification constant) |
| hdr.importdir | 0x1A0 | - | `88ac3300 f4010000` | `00e0e600 08020000` | dir -> (0xE6E000, 0x208) = 24 copied descriptors + `win32craft.dll!Win32Bootstrap` + terminator |
| append.d3imp | 0xE13600 | 0xE6E000 | (EOF) | 0x400 bytes | descriptors 0x000..0x208, name 0x208, hint 0x218, ILT 0x22C, **IAT cell 0x234 (RVA 0xE6E234)**; 24 original descriptors byte-identical; no RWX, no code |
| code.trampoline | 0x209764 | 0x20A364 | 16 x `00` | `dff808c0 fc44 dcf800c0 6047 c83ec600` | literal 0xC63EC8 = 0xE6E234 - (0x20A364+8); 0x9C zero bytes of `.text` tail slack |
| code.callsite | 0x1FFE72 | 0x200A72 | `03f043fa` (`bl 0x203EFC`) | `09f077fc` (`bl 0x20A364`) | the WinMain call of the CRT startup, displacement +39150 |
| sec.text.virtualsize | 0x220 | - | `64932000` | `74932000` | `.text` VS 0x209364 -> 0x209374 so the loader maps the trampoline |

No renames, no redirects (`*_app.dll` imports stay; the VC120 Phone DLLs ship beside the exe). Checksum stays 0 (as in stock and in the plan's hash); `--checksum recompute` is validated against `imagehlp!MapFileAndCheckSum`.

**Outputs** (both reproducible; `expected_outputs` in the profile):
* plan table only (`--no-fl91`): sha256 `453ce59a2100dd7def909ac4871fd58308ec0052572ec73af07c9ac50b15db31` - this IS the hash `pe_patch_plan.json` records for its table, reproduced exactly
* **default (with the FL 9_1 byte)**: sha256 `cb44c09a1af48c266570dfe1b4764b803c0cdc89d0e3327add756da9aeea78e2` (differs from the plan hash by exactly the one byte at file 0x2339F9)
* size 14760448, SizeOfImage 0xE6F000, DllCharacteristics 0x8140, NumberOfSections 7, entry 0x20095D unchanged

```
.text   VA 0x001000 VS 0x209374 raw 0x000400 RS 0x209400  R-X 0x60000020
.rdata  VA 0x20B000 VS 0x1339A0 raw 0x209800 RS 0x133A00  R-- 0x40000040
.data   VA 0x33F000 VS 0xAF52D8 raw 0x33D200 RS 0xA9E800  RW- 0xC0000040
.pdata  VA 0xE35000 VS 0x017C58 raw 0xDDBA00 RS 0x017E00  R-- 0x40000040
.tls    VA 0xE4D000 VS 0x000009 raw 0xDF3800 RS 0x000200  RW- 0xC0000040
.reloc  VA 0xE4E000 VS 0x01FB10 raw 0xDF3A00 RS 0x01FC00  R-- 0x42000040
.d3imp  VA 0xE6E000 VS 0x00023C raw 0xE13600 RS 0x000400  RW- 0xC0000040   (new; NOT executable)
```

### Decision for the owner: FL 9_1 byte default ON
RECON_SUMMARY lists the string patch as one of two alternatives (the other is rewriting `pTarget` in a `D3DCompile` hook); without one of them `getShaderModelSuffix` throws for 9_1. I made the byte patch part of the profile (default ON) because it is verified: the string has exactly one reference (re-derived by the self test from `.reloc`: the MOV32 pair at RVA 0xD742C, inside `getShaderModelSuffix` 0xD73FC) and it is the only `_level_9_` string in the file. If the host rewrites `pTarget` instead, patch with `--no-fl91` (hash 453ce59a...). Both variants are accepted by the tool, only the default is recorded as "default".

## 4. v1_2_8 identity proof (PROOF REQUIRED - done)

Old patcher run on `Minecraft_1.2.8.0_ARM32\Minecraft.Windows.exe` (default flags) -> sha256 `0f30d9cd8474c5959975711b6b3d9c4421dc593d5bf3021b141652dc250fa36f`, equal to `dist-arm32\Win32Craft.1.2.8-arm32.exe` (23597056 bytes). Engine with profile `v1_2_8` -> **byte-identical file, same SHA-256** (also asserted by the self test on every run, which re-runs the old patcher):
* exe default: `0f30d9cd...` identical
* exe `--redirect-api-sets`: `775a59883a9d09684cc8951ca90aec94006b941c85075b8ef9f9d928d68f322f` identical to the old patcher's
* `fmod_ARM.dll` (VCRUNTIME140 rename + APPCONTAINER + skip-WinRT-init): `cf226ecb83f478a67c6dab205dbd2a5ed0a469ac9fa27f2ed4bda12680d5d1f6` identical; `--keep-winrt-init` differs by exactly file 0x78A4F..0x78A51
* every edit offset equals the old patcher's log (0x19e, 0x1328cde, 0x132c3c8, 0x132cd0a, 0x132ce36, 0x350, 0x146, 0x160, 0x190, 0x1c0, 0xc362a0, 0xb48eca, 0x240)

There is no byte difference to explain. (Only the report/verification layer is new; the old patcher's exe checksum behaviour - untouched, 0 - is kept as profile policy `keep`.)

## 5. VC120 runtime DLLs (msvcr120_app / msvcp120_app / vccorlib120_app)

**Originals FOUND**: `D:\repo\armdeps\Microsoft.VCLibs.120.00.Phone_12.0.30113.0_arm__8wekyb3d8bbwe.Appx` (645282 bytes, sha256 `a9472ec802c64baf17161d8e611548df78fd29b8a758bb2e223979110452c892`; AppxManifest `Microsoft.VCLibs.120.00.Phone` 12.0.30113.0, ProcessorArchitecture arm, publisher Microsoft Corporation, framework package = exactly the manifest's `PackageDependency Name/MinVersion`). This resolves risk R5 of the plan ("clean appx not in the repo"). The tool reads the DLLs straight out of that zip (identity checked first) - nothing needs to be unpacked.

| DLL | size | original sha256 (from the appx; DllCharacteristics 0x1140) | patched sha256 (0x0140) | friend copy |
|---|---|---|---|---|
| msvcr120_app.dll | 611768 | `778800c2bd73d7aa8a3c1c90c3a7062130114b2b127e07c9a0d2549b419d765f` | `c4cbf6e89da092aa33828a66a61b74ca06af5c739b053b1cacb4cf7f9728b2b5` | identical to patched |
| msvcp120_app.dll | 545208 | `29e62fd2baee67acecfdbefa226a88354215b46bb767c0962f7ef922d2f15074` | `b82dd0fd70dd53a8cb6f72376c7f95123a9aeacac4c72e351863f22cbcf942c` | identical to patched |
| vccorlib120_app.dll | 307128 | `66170fed91616007332921762e848159af293847d5174dd1d60b26ad3dade3ac` | `002d1f3999cb87342aad42f3dca8c6527c862029bc845fe1ed83eeee208db668` | identical to patched |


What the friend's copies in `TEST\MCPE_0.10.4.40\` differ in, measured: **exactly one byte per file**, the high byte of DllCharacteristics (file 0x14F / 0x14F / 0x15F, `0x11 -> 0x01`, i.e. 0x1140 -> 0x0140). Not changed: imports (no renames), Authenticode blob (still there, now invalid), header checksum (still the original's, so stale), sections, size, version resources. The genuine originals' sha256 equal `sha256_original_reconstructed` of the plan (it had reconstructed them by flipping the bit back), and the friend copies equal our patch of the genuine files. The self test proves all of this on every run (original -> patched == friend copy; friend copy provenance: checksum recomputed with the bit set equals its header checksum; the checksum routine equals `imagehlp`).

DLL patch step (`DllPatcher`): given an **original** it clears APPCONTAINER (file offset recomputed from the PE and cross-checked with the profile 0x14E/0x15E); given the **known friend copy** (sha256 in `sha256_patched`) it recognises the state `patched_known` and writes it unchanged; an unknown hash is refused (`--allow-unknown-companion` patches the bit only). Both routes end in the same bytes. Header checksum is kept (policy `keep`, = checksum of the original; `recompute` available).

Structural verification of the result (all PASS for original-derived and friend copies): ARMNT PE32 DLL, APPCONTAINER clear, no W+X section, only ordinary sections (`.text .rdata .data .pdata minATL .rsrc .reloc`), imports parse and name only `api-ms-win-core-*` sets, `OLEAUT32.dll` and each other (`msvcr120_app <- msvcp120_app <- vccorlib120_app`), export directory and base relocations present (ASLR), Authenticode blob inside the file.

Other VCLibs packages seen and NOT used: `D:\arm\Dep\Microsoft.VCLibs.120.00_12.0.21005.1_arm...` (Store/desktop flavour, older), `Microsoft.VCLibs.120.00.Universal_12.0.30501.0`. The exe's manifest asks for the Phone package 12.0.30113.0.

## 6. What was verified, and how

| claim | evidence (self test `tools\arm32-patcher-selftest.py`, 198 PASS / 0 FAIL / 0 SKIP on this machine) |
|---|---|
| engine == device-proven 1.2.8 patcher | byte-identical exe, exe `--redirect-api-sets`, fmod_ARM.dll; SHA 0f30d9cd.. |
| engine reproduces the verified plan | 9/9 plan edits (offset, old, new), appended content hex + sha256, trampoline/callsite bytes, plan sha 453ce59a.., diff confined to plan V2 ranges (independent diff, not the engine's own list) |
| no W+X, alignment, ordering | every output: no section W+X, SizeOfImage multiple of 0x1000 == align(last end), sections ascending/non-overlapping, `.d3imp` flags 0xC0000040 |
| imports | own parser AND `tools\arm32\arm32re.py` parse the 411 (0.10.4) / 717 (1.2.8) imports identically; `win32craft.dll!Win32Bootstrap` IAT cell 0xE6E234 / 0x173043C |
| call site / trampoline | BL round trip on 4000 random sites (+-16 MiB, BL and B.W), keystone agrees (300 random + the trampoline template), capstone decodes `bl #0x20a364` and the 4 trampoline instructions, **Unicorn runs call site -> trampoline -> export at two image bases (0x400000, 0x2A30000) without relocation**: PC lands on the value read from the IAT cell, LR = call site + 4, r0/r3 preserved |
| no reloc / .pdata over patched code | checked in engine and re-checked on the output |
| checksum | `pe_checksum` == imagehlp on exe and DLLs; recompute only changes the checksum field |
| refusals | wrong old bytes, wrong context, non-zero slack, already patched (own output and the friend's), W+X request, wrong callee, BL out of range / odd, reloc overlap, unexpected SizeOfImage, unknown companion hash, placeholder profile, output inside input folder, non-empty out folder |
| CLI | detect (3 inputs + non-PE), patch end to end (input folder hashes unchanged), verify (incl. a corrupted trampoline literal -> FAIL), profile as JSON round trip, probe on both exes |

Not verified (cannot be offline): that Windows RT 8.1 loads the result (8 api sets, code integrity, VC120 DllMains - see RECON_SUMMARY section 6/9), and that the FL 9_1 shaders run on Tegra.

## 7. `probe` (unknown older versions) and the v1_16_40 slot

`probe <exe>` prints machine/ImageBase/SizeOfImage/entry, section table (flags), free section-header slots, overlay/Authenticode/relocs/.pdata counts, imports per DLL, AppContainer/UWP-runtime DLLs, IAT cells of `GetActivationFactoryByPCWSTR`, `D3DCompile`, `D3DReflect`, `D3D11CreateDevice`, `CreateDXGIFactory1`, `XAudio2Create`, `RoInitialize`, operator new/delete, `_CxxThrowException`, CRT imports; class-name strings (XAML vs CoreWindow app); the startup chain `entry -> (cookie init) -> tail b.w -> CRT main` and ranked BL call-site candidates (r0 = literal equal to ImageBase, `movs r1/r3,#0`, result stored; thunk callees penalised; CRT calls such as `_initterm_e`, `__getmainargs`, `_exit_app` named through their IAT cells); zero trampoline slots (`.text` tail slack, plus zero gaps between `.pdata` functions - prefer the tail slack) with reloc/pdata freedom and BL range; ARM companion DLLs with APPCONTAINER next to the exe; and the list of what a new profile needs. On both known exes the true call site (0x200A72 / 0xB49ACA) is candidate #1 (self-tested), and probing modified copies degrades to a medium-confidence guess. **An unknown version still needs host facts**: probe cannot tell what the call site means, which imports the host must redirect, which SizeOfImage the host keys on, or which companion patches are needed.

Slot for a future profile: `PROFILE_1_16_40` in the script is a `placeholder` (README_V116.md: SizeOfImage 0x2BD9000 unverified, `todo` list). `patch --profile v1_16_40` refuses and prints the todo list. To add it: `profiles --dump v0_10_4`, fill in values from `probe` + RE, set `"status": "complete"`, load with `--profile-file` or paste into `PROFILES`. No engine change is needed for: more renames (VC140 `_APP` names are the 1.2.8 example), redirects, byte patches, several companions, another new-section layout, custom host DLL/export.

## 8. Risks and notes

1. **Default hash `cb44c09a...` is tool-computed** (plan-table hash 453ce59a... is the independently recorded one). A device run of the default exe is the real proof; a mismatch of the FL byte would show up as the `getShaderModelSuffix` throw (see RECON_SUMMARY section 4).
2. **Code integrity on RT**: patched exe and the VC120 DLLs (Authenticode blob kept, now invalid; DllCharacteristics has no FORCE_INTEGRITY bit) need the same relaxed policy the 1.2.8 package already assumes. Removing the blob would not help a signature-enforcing policy.
3. **Header checksum**: exe stays 0 (as stock and as the plan hash); DLL checksums are the stale original values. Windows does not validate the checksum of ordinary user-mode DLLs; `--checksum recompute` exists if a tool complains.
4. `.text` VirtualSize grows by 16 bytes (required: the loader only maps VirtualSize bytes of the file-backed tail). SizeOfCode is untouched (plan).
5. The friend exe/`.d3imp` RWX layout is **not** reproduced (self test confirms its `.d3imp` is W+X and that the friend exe is refused as "already patched"). The friend VC120 copies are byte-equivalent to a patch of the genuine files, so they are safe to use; the tool prefers the genuine ones.
6. The appx path `D:\repo\armdeps\...` is machine specific; elsewhere pass `--companion-appx`, `--companion-dir`, or set `WIN32CRAFT_VCLIBS120_APPX`. Copying that appx into the repo (e.g. next to `dist-arm32`) would make the build reproducible; I did not copy files into the repo.
7. 8 api-set contracts, the VC120 DllMains and the 436 static ctors remain unproven on RT 8.1 (RECON_SUMMARY section 6): the loadcheck module list for 0.10.4 is `{Win32Craft.0.10.4-arm32.exe, win32craft.dll, vccorlib120_app.dll, msvcp120_app.dll, msvcr120_app.dll, ucrtbase.dll}`.
8. `probe` heuristics are tuned on two exes (VS2013 XAML and VS2017 CoreWindow CRT shape). For other CRT shapes the warning "no candidate shows the hInstance argument setup" is printed and the call site must be found by hand.
9. The tool refuses signed or overlay-carrying exes (the new section is appended at EOF); neither known target has one.
10. `selftest` needs the owner-machine paths above for the 0.10.4 / appx / friend inputs (override: `ARM32_EXE_0104`, `ARM32_DIR_0104`, `ARM32_VCLIBS120_APPX`, `ARM32_FRIEND_DIR`); a missing input gives SKIP, never PASS.
