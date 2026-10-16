# ARM32 port — reverse-engineering guide (shared by all analysis agents)

## What this project is
`win32craft.dll` is a shim DLL. A patched `Minecraft.Windows.exe` (a UWP/C++/CX Bedrock client) imports it
instead of the WinRT/UWP pieces it cannot use on a plain desktop. At startup the patched exe calls
`Win32Bootstrap()` which creates an HWND, a D3D11 device + swap chain, fakes just enough WinRT
(CoreWindow, StorageFolder, pickers, ...) in C, constructs the game's `AppMain` through the game's own
factory, then runs its own message/frame loop calling game functions directly.

The existing implementation (`win32_host.c`, x86/i686) hard-codes, per Minecraft version, **RVAs of game
functions/vtables/IAT cells/strings, struct field offsets, x86 calling conventions and x86 machine-code
patches**. We are porting the **1.2.8** path to **ARM32 (Thumb-2, Windows RT 8.1 / Windows 10 ARM32,
desktop)**. The only ARM32 binary available is `Minecraft_1.2.8.0_ARM32/Minecraft.Windows.exe`
(same Minecraft version 1.2.8.0, built Dec 2017, MSVC 14.10, `thumbv7-windows-msvc`).

> **Never reuse an x86 RVA or an x86 struct offset for ARM.** They are hypotheses only. The x86 *semantics*
> (what a function does, which class it belongs to, which field is which) transfer; the numbers do not.
> The x86 binary is NOT available, only the x86 host source `win32_host.c` (+ comments in it).

## Where things are
* Repo root: `D:\repo\Win32Craft-main`
* x86 host source (read-only for you): `win32_host.c` (~15k lines; the 1.2.8 path is selected by
  `host_is_128`, constants `RVA_128_*`). `arm32/analysis/func_index.txt` lists all functions with line spans.
* ARM32 exe: `Minecraft_1.2.8.0_ARM32/Minecraft.Windows.exe` (PE32, machine 0x1C4, ImageBase 0x400000,
  SizeOfImage 0x1730000, .text 0x1000..0xC36E9C, .rdata 0xC37000, .data 0x132F000, .pdata 0x15BE000,
  .reloc 0x1674000; ASLR flag set but base relocs present).
* Your output directory: `arm32/facts/` (create files there; never edit `win32_host.c`).
* LLVM/Clang for ARM: `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin\` (clang, lld-link, llvm-objdump, llvm-readobj ...).
  MSVC/SDK ARM libs: `D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\arm`,
  `D:\Windows Kits\10\Lib\10.0.19041.0\{um,ucrt}\arm`.
* Python 3.13 with `capstone`, `keystone`, `unicorn` is installed (ARM/Thumb disassemble / assemble / emulate).

## The RE toolkit: `python tools/arm32/arm32re.py <cmd>` (run from the repo root)
All numbers are hex RVAs (not VAs). Thumb bit is stripped for you. First call builds a cache (~10 s).
```
info | func <rva> [-n N] | dis <rva> [count] | calls <rva> | callers <rva> | xref <rva>
str <text> [-w] [-i] | strx <text> [-w]      # strings (ASCII default, -w UTF-16); strx lists referencing functions
rtti <class-substring>                         # RTTI: vtable RVA(s) of a class (mangled names, e.g. ".?AVClientInstance@@")
vtable <rva> [n]  | ptrs <rva> [n] | bytes <rva> [n] | find "<hex bytes>" | guid <GUID> | imm <value>
imports [substr] | impcallers <substr> | exports [substr] | funcs <lo> <hi> | name <rva> <label> | names
```
* `func`/`dis` annotate `bl` targets, `movw/movt` pairs (absolute VA loads, from relocation records),
  literal-pool loads, strings, import cells (`IAT dll!name`), and vtables (`vtable Class`).
  Virtual functions get automatic names `Class::vfN` (N = slot index) from RTTI vtables.
* Functions come from `.pdata` (exact start + length). Import calls go through 12-byte thunks
  (`movw ip / movt ip / ldr.w pc,[ip]`), shown as `thunk[dll!name]`; `impcallers` finds callers by import name.
* **When you identify a function or object, run `arm32re.py name <rva> <label>`** so later
  disassembly (yours and other agents') shows it. Labels: `Class::method` or `area:purpose`.
* Raw output is long: always limit with `-n`, `| head`, or `Select-Object -First`. Never dump big ranges.
* Use Python (`import sys; sys.path.insert(0,'tools/arm32'); import arm32re`) for custom scans:
  `c = arm32re.Ctx(); c.pe` (sections, `u32`, `bytes`, `cstr`), `c.idx` (`callers`, `mov32`, `ptr`, `rtti`,
  `thunks`, `funcs`), `arm32re.xrefs_to(c, rva)`, `arm32re.thumb_dis(...)`.
  When writing Python to a file, avoid doubled backslashes in shell heredocs (they get collapsed);
  prefer the Write tool for script files.

## Facts already established (verified by the lead)
* Vtable layout and virtual slot numbering match x86 (same source): e.g. x86 `RVA_128_CLIENT_INSTANCE_VTABLE`
  (0x01549354, second vtable at +0x20) ↔ ARM `.?AVClientInstance@@` vtables at **0x0133A758** (offset 0) and
  **0x0133A778** (+0x20), slot 3 (`ClientInstance::vf3`, the render callback hooked on x86) = fn **0x0011A280**.
  Its field accesses (`this+0x28` level renderer, `+0x1c`, `+0x18`, `+0x2c0` scene stack) mirror x86 offsets, so
  for pointer-only structures offsets often coincide — but verify each one in ARM code.
* ARM function starts are 4-aligned; `ldr.w pc,[pc,#imm]`-style trampolines are the usual absolute jump.
* Imports are called through thunks; the host patches **IAT cells** by RVA (cells live in `.rdata` 0xC37000..0xC37BF8
  plus delay-import cells in `.data`/.rdata). IAT slot **order differs from x86** — always resolve by import name
  (`imports <substr>`), never by x86 slot arithmetic. E.g. TCUI delay cells: ShowProfileCardUI 0x015121FC,
  ProcessPendingGameUI 0x01512200, ShowProfileCardUIForUser 0x01512208 (x86 had Process before Show).
* `fmod_ARM.dll` is a regular (non-delay) import in the ARM exe. D3DCompile IAT cell 0x00C370B4,
  `_CxxThrowException` 0x00C374E4 (VCRUNTIME140_APP), `D3D11CreateDevice` thunk exists (use `imports`).
* All code pointers stored in data have bit 0 set (Thumb). RVA values in your facts are **even code addresses**
  (without the Thumb bit); say so when a pointer value matters.

## MSVC-ARM32 (AAPCS-VFP, hard-float) conventions you will need
* Args: r0–r3 then stack (8-byte aligned for 64-bit); floats/doubles in s0–s15/d0–d7 (hard-float); result in
  r0 (r0:r1 for 64-bit, s0/d0 for float/double). C++ `this` = r0. MSVC puts a hidden aggregate-return pointer in
  r0 for free functions; for member functions determine whether `this` is r0 and the sret pointer r1 (MSVC order)
  by reading real code — do not assume. Functions with `push {r0-r3}` in the prologue spill register arguments.
* **No callee-cleanup / thiscall / fastcall quirks exist on ARM**: the x86 hand-written asm thunks
  (`call_game_*`) turn into ordinary C calls. What matters is the *argument order and count* — re-derive them
  from the ARM code (how many register args are consumed, which stack args are read).
* Prologues: `push {r4-r11,lr}`/`push.w`, frame pointer r11 (`fp`) often used; `bl 0x11fc` = stack-probe helper.
* Thumb-2: 16-bit and 32-bit encodings; branch range BL/B.W ±16 MB; `movw/movt` pairs carry absolute addresses
  (type-7 relocation); literal pools hold absolute VAs.
* Exceptions: `.pdata` unwind info per function; C++ EH through `__CxxFrameHandler3`; C++/CX `Platform::` helpers
  come from `vccorlib140_app.dll` (imports).

## Method tips
1. Anchor on something unambiguous: RTTI vtable (`rtti`), distinctive strings (`strx`, ASCII and `-w`), import
   names (`impcallers`), GUIDs (`guid`), constants (`imm`), or an existing fact. Then walk callers/callees.
2. Mirror the x86 host's comments: they describe the semantics ("ClientInstance::getTopScene uses the local stack
   at +0x2c0 ..."). Use them to pick the right candidate, then confirm with ≥2 independent indicators
   (e.g. string xref + callsite shape + vtable slot + field offsets in neighbours).
3. For struct offsets, find 2+ ARM functions that access the same field consistently (e.g. via vtable-slot
   methods whose x86 counterparts are described in comments), and decode `ldr/str [rN,#off]` forms
   (`movw rX,#off` + register offset is used for offsets > 0xFFF; Thumb-2 `ldr.w` imm12 ≤ 0xFFF).
4. Record **expected bytes** (first 8–12 bytes at the function entry, little-endian hex) for every function the
   host will hook or patch, so the ARM host can sanity-check the site at runtime like the x86 host does.
5. If you cannot find something, say so with what you tried. A confident wrong RVA is worse than "not found".
   Confidence: `high` = ≥2 independent confirmations; `medium` = one strong + consistent; `low` = plausible.

## Output contract
Write `arm32/facts/<area>.json` (machine-readable) and `arm32/facts/<area>.md` (short human summary).
JSON shape:
```json
{"area":"...","facts":[{
  "id":"area.short_name",
  "x86_symbol":"RVA_128_X | literal | field description",
  "x86_value":"0x0082cce0",
  "kind":"function|iat|vtable|vtable_slot|data|offset|iid|string|hook_site|abi|pe",
  "arm_rva":"0x00xxxxxx or null (even code address; for offsets give the offset)",
  "summary":"what it is (class::method), one or two lines",
  "arm_signature":"e.g. void* fn(void** result /*r0*/, void* ctx /*r1*/, void** holder /*r2*/) -> r0; callee-saved r4-r11; no stack args",
  "x86_to_arm_notes":"every difference the porter must handle (ABI, arg order, struct layout, bytes to patch, ...)",
  "expected_bytes":"first bytes at arm_rva as hex, or null",
  "evidence":["tool-command -> observation", "..."],
  "confidence":"high|medium|low"}],
 "port_instructions":["precise, code-level instructions for the person porting the x86 source functions that use these facts"],
 "open_questions":["..."]}
```
Every x86 constant/offset/ABI/byte-pattern assumption in your assigned source regions must be accounted
for by a fact (even if the fact says "unchanged: same offset, confirmed by ..." or "no longer needed because ...").
