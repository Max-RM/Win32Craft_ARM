# Minecraft 1.16.40.02 ARM32 port - working notes (read together with ../RE_GUIDE.md and ../PORT_RULES.md)

## STATUS (paused 2026-10-12 by the user; resume next week)
* Groundwork done: per-exe label files, version-aware toolkit (`ARM32_EXE`, `ARM32_FACTS_DIR`, `gen_constants.py` args), this README.
* RE round 1 (workflow `v116-re-facts`, run `wf_9ef5b840-759`) was STOPPED mid-run: the **Scope** phase finished and wrote `SCOPE.md`
  (11 areas: gfx_device, gfx_compat, appview, input, touch, client_state, platform_fixes, pe_imports, winmain_patcher, winrt_activation, js_audio).
  The Find agents for the first six were interrupted; NO fact file was completed (`facts/` is empty). Finder scratch left in `analysis/`
  (`a.py`, `appview_thunk.py`, `fsum.py`, `cache_appview/` = a 25 MB index cache that can be deleted) and a few labels in
  `tools/arm32/names/Minecraft_1.16.40.02__Minecraft.Windows.exe.jsonl` (unverified).
* To resume: re-run the workflow script (it caches the finished Scope agent) with
  `Workflow({scriptPath: "C:\\Users\\hamed\\.claude\\projects\\D--repo-Win32Craft-main\\7ed8744f-0d72-482c-82d8-cd88a334a2ff\\workflows\\scripts\\v116-re-facts-wf_9ef5b840-759.js", resumeFromRunId: "wf_9ef5b840-759"})`
  (the script file may be gone if the session data is cleaned: the briefs are in `SCOPE.md`, rebuild from it).
  Then: fold verifier corrections -> `gen_constants.py` (command in the table below) -> port-units workflow -> device package -> unified patcher.
* Also still open from 1.2.8: the user's two-finger multi-touch test result on the Surface RT.

Game under analysis: `Minecraft_1.16.40.02\Minecraft.Windows.exe` (ARM32 UWP, ProcessorArchitecture arm, built 2020-09-03, SizeOfImage 0x2BD9000,
~140k functions, ImageBase 0x400000). Cheap feasibility tests passed (see ../../HANDOFF_ARM32.md): runtime `D3DCompile` with `_level_9_1`
profiles (52/52 game shaders compile at `*_4_0_level_9_1`), d3d11 only, no RenderDragon, `fmod_ARM.dll`, `chakra.dll` (38 JSRT imports),
`vccorlib140_app.DLL`, JSON `.material` files + HLSL sources in `data\shaders\hlsl`.

## Layout (everything for this version lives apart from the 1.2.8 files, which keep working untouched)
| item | path |
|---|---|
| RE facts (per area: `<area>.json/.md/.verdicts.json`, `audit.json`) | `arm32/v1_16_40/facts/` |
| scratch analysis | `arm32/v1_16_40/analysis/` |
| generated constants header (prefix `ARM116_`, macros `ARM116_CODE/ARM116_DATA`) | `win32_host_arm32_v116.h` (repo root) via `python tools/arm32/gen_constants.py --facts arm32/v1_16_40/facts --out win32_host_arm32_v116.h --prefix ARM116 --guard WIN32_HOST_ARM32_V116_H --title "Minecraft 1.16.40.02 ARM32"` |
| ARM host code for this version | `win32_host_arm32_116.inl` (included from `win32_host.c` under `#ifdef WIN32CRAFT_ARM32`, like the x86 `win32_host_116.inl`) - ONE win32craft.dll serves both versions, chosen at runtime by PE machine + patched SizeOfImage |
| patcher | one table-driven `tools/arm32-patcher.py` (profiles for 1.2.8 and 1.16.40.02; generic PE edits; per-version call-site/rename/redirect tables) - built at the end |

## How to use the RE toolkit for this exe (labels are stored per exe automatically)
```
set ARM32_EXE=D:\repo\Win32Craft-main\Minecraft_1.16.40.02\Minecraft.Windows.exe      (PowerShell: $env:ARM32_EXE = "...")
set ARM32_FACTS_DIR=D:\repo\Win32Craft-main\arm32\v1_16_40\facts                         (only for facts_survey/gen_constants/verify scripts)
python tools/arm32/arm32re.py info | func | dis | xref | strx | rtti | vtable | impcallers ... name <rva> <label>
```
Without `ARM32_EXE` the toolkit analyses the 1.2.8 exe: ALWAYS set it for this version (first use builds the index, ~45 s).
The constants checker: `ARM32_EXE=... ARM32_HDR=win32_host_arm32_v116.h ARM32_PREFIX=ARM116 python tools/arm32/verify_constants.py`.

## Sources of knowledge (all are HINTS about semantics; NO RVA, offset or struct layout transfers between versions)
1. The x86 host for the x86 build of 1.16.20.03: `win32_host_116.inl` (1390 lines, fake WinRT objects `*_116`, `activation_116`, pointer/key/character event
   args that are fed to the handlers the game registers on CoreWindow, relative-mouse logic, scene/ClientInstance probing) plus every `host_is_116`
   branch in `win32_host.c` (grep `host_is_116`, `bootstrap_116`) and `tools/1.16.20.03-patcher.py`. They show WHAT the host must do for 1.16.
2. The finished 1.2.8 ARM32 port (facts in `arm32/facts/`, code in `win32_host.c` under `WIN32CRAFT_ARM32`, `HANDOFF_ARM32.md`, `ARM32.md`): proven ARM
   solutions (hook by data cell, Thumb trampoline, HSTRING native forwarding, FL9_1 clamp, touch via WM_TOUCH/MultitouchItem, loadcheck, app-local runtime).
   The 1.16.40 game is the same engine family 3 years later: class names (RTTI), strings and call shapes usually survive, numbers do not.

## Known differences already seen (1.16.40.02 vs 1.2.8 ARM)
* Shaders: `D3DCompile` caller `sub_1062384` (IAT 0x015A601C), `D3DReflect` caller `sub_105fec4`; strings `_level_9_1` (0x01795C6C), `_level_9_3` (0x01795C60), `#define VERSION ` (0x017A3B08).
* Imports: D3DCOMPILER_47, chakra.dll (JSRT; absent on RT 8.1), d3d11, fmod_ARM, vccorlib140_app, plus api-sets not yet proven on RT 8.1:
  processtopology-obsolete-l1-1-0, synch-ansi-l1-1-0, synch-l1-2-1, windowserrorreporting-l1-1-0, wow64-l1-1-0.
* The game folder has `data\renderer\hummingbird|overworld` JSON (not RenderDragon), 19 `.material` JSON files, `data\shaders\hlsl` (84 files, 14 geometry shaders are VR-only).
