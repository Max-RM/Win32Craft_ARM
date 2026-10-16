# ARM32 port rules (shared by every porting unit)

Read `arm32/RE_GUIDE.md` first (toolkit + ABI notes). This file defines **how the port is organised**.

## Goal
Make `win32_host.c` build and run for **Minecraft 1.2.8 ARM32** (Thumb-2, hard-float, Windows RT 8.1 / Windows 10 ARM32
desktop) without changing the x86 build. The ARM32 game exe is `Minecraft_1.2.8.0_ARM32/Minecraft.Windows.exe`.
The verified reverse-engineering facts are in `arm32/facts/<area>.json` (+ `.md`, + `.verdicts.json`); the generated
header `win32_host_arm32.h` (repo root, **do not edit**, regenerate with `python tools/arm32/gen_constants.py`)
exposes every numeric fact as `ARM_<AREA>_<ID>` (see the comment on each line; some "constants" are offsets/sizes -
read the fact JSON `summary`, `arm_signature`, `x86_to_arm_notes`, `port_instructions` for the real meaning).

## Decisions already made (do not re-litigate; flag a contradiction in your report instead)
1. **1.2.8 only.** All other versions (0.13/0.14/0.15.10/1.1.5/1.16, `win32_host_early2016.c`, `win32_host_116.inl`) are out of scope
   for ARM. `host_is_128` is TRUE, `host_is_115`/`host_is_116`/`host_is_windows7` are FALSE on ARM.
2. **x86 must stay byte-identical.** Every change is inside `#ifdef WIN32CRAFT_ARM32` (defined by `win32_compat.h` for
   `__arm__`/`_M_ARM`). Replace a function by wrapping: `#ifdef WIN32CRAFT_ARM32 <arm version> #else <original untouched> #endif`
   (two small anchored insertion edits: one before the signature, one after the closing brace). `try_unit.py` proves x86
   output identity; "DIFFERENT!" is a defect.
3. **Calling game code**: `typedef R (*Fn)(args)` plain C function pointers (AAPCS), obtained with
   `ARM_CODE(image, ARM_...)` (sets the Thumb bit). No `thiscall`/`fastcall`/naked asm on ARM. Pass `float` where the ARM code takes a float.
   ARM ABI of every callee comes from the facts (`arm_signature`), never from the x86 code.
4. **Prefer patching data cells** (vtable cells, IAT/delay-IAT cells, string bytes) with `PAGE_READWRITE`
   (VirtualProtect), verify the cell still holds the expected original value first, log and fail cleanly if not,
   `FlushInstructionCache` afterwards. Never use `PAGE_EXECUTE_READWRITE` unless a fact says code must be patched.
5. **Pointers stored in data have the Thumb bit set** (`image + rva + 1`); our hook function pointers (Thumb) already have it.
6. **Feature level 9_1**: the ARM D3D11CreateDevice hook clamps the feature-level list to `{0x9100}` by default
   (host option `--feature-level=9_1|9_2|9_3|10_0|10_1|11_0|11_1|game`, env `WIN32CRAFT_FEATURE_LEVEL`) - see `facts/fl91.md`.
7. Windows-7-only code (DXGI Device3/Device2 IID downgrades, DiscardWindowViews patch, 11_1 filter, WARP retry,
   `GetVersion`==6.1 branches), the Wine-era vccorlib shims (`install_vccorlib_compat`), the CreateFile2 hook and the fmod delay-import
   resolver are **not used on ARM** (facts explain why). Keep them compiled-out or uncalled, never delete x86 code.
8. **HSTRING**: on ARM the genuine desktop vccorlib140/combase use REAL HSTRINGs, so every HSTRING the host creates or
   consumes must go through native combase (`WindowsCreateString` etc. forward to combase on ARM).
9. Log generously (`log_line`, `log_pointer`, `log_hresult`, `wsprintfA`): the only feedback loop is the user running on a device
   and sending `win32craft.log`. Every failure path must log what failed and what the cell/bytes actually contained.
10. Image identification on ARM: `PE machine == 0x01C4` and `SizeOfImage == IMAGE_SIZE_128_ARM_PATCHED (0x01731000)`.

## Source organisation / edit protocol (parallel, race-free)
* The pristine x86 sources are in `arm32/orig/` (win32_host.c, api_compat.c, ...). **Never edit the repo files directly.**
* Each unit writes **edit pairs** into `arm32/edits/<unit>/`:
  `NNN.old.txt` (exact text, must occur exactly once in the pristine file *as already edited by 00_core*), `NNN.new.txt` (replacement),
  optional `NNN.file.txt` (target file, default `win32_host.c`; e.g. `api_compat.c`), optional `NNN.note.txt`.
  Plain newlines are fine (applier converts to the CRLF of the source). Number edits 001, 002, ...; they apply in order.
  Insert code by making `old` a unique anchor line and `new` = new code + that anchor line (or anchor + new code).
* Validate: `python tools/arm32/try_unit.py 00_core <your_unit> --tag <your_unit>` applies `00_core` then your edits
  to the pristine sources in `arm32/work/<tag>/`, compiles `win32_host.c` + `api_compat.c` for ARM32 (`-c`) and for x86, prints ARM
  errors and whether the x86 object is IDENTICAL to pristine. Your unit must add **zero ARM errors** (the baseline `00_core` is
  compile-clean) and keep x86 IDENTICAL. Look at `arm32/work/<tag>/win32_host.c` to re-read your edited code.
* Stay inside the regions your unit owns (below). Do not change the signature of any function defined outside your region.
  Anything outside your region that must change: record it under `unowned` in your report (do not edit it).
* Do not touch another unit's `arm32/edits/<unit>/` directory (except a reviewer fixing the unit it reviews).
* Compile flags (what try_unit.py uses): `--target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2
  -fno-stack-protector -fno-builtin`, clang at `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin\clang.exe`.
* Style: match the surrounding C (C99, 4-space indent, existing helpers `log_line/log_pointer/log_hresult`, `wsprintfA` buffers,
  `VirtualProtect` idiom). Comments only where the reason is not obvious; cite the fact id (e.g. `facts input.mouse_hooks`).

## Shared helpers provided by `00_core` (available to every unit)
```c
/* patch a pointer-sized data cell: verifies *cell == expected (when expected != NULL), stores replacement, optionally
 * returns the original value, flips protection to PAGE_READWRITE and back, flushes the icache, logs failures with `what` */
static BOOL arm32_patch_cell(void **cell, const void *expected, void *replacement, void **original_out, const char *what);
/* patch one byte in read-only data (e.g. a string char), verifies the expected byte first */
static BOOL arm32_patch_byte(BYTE *address, BYTE expected, BYTE replacement, const char *what);
/* fallback code hook: 8-byte `ldr.w pc,[pc,#imm]` + literal absolute jump at a 4-aligned function entry */
static BOOL arm32_write_thumb_jump(BYTE *site, const void *target, const char *what);
/* verify bytes at image+rva equal a table (sanity check before hooking); logs got/expected on mismatch */
static BOOL arm32_check_bytes(const BYTE *address, const BYTE *expected, SIZE_T size, const char *what);
```

## Cross-unit contract (exact names/signatures; ARM variants only exist under `#ifdef WIN32CRAFT_ARM32`)
| provided by | symbol | meaning |
|---|---|---|
| iat_hooks | `static BOOL install_arm32_iat_hooks(BYTE *image)` | installs by-name IAT/delay-IAT hooks: `_CxxThrowException` trace, activation-factory redirect, D3DCompile forwarder/logger, D3D11CreateDevice (BGRA flag + feature-level clamp), TCUI no-ops; FALSE if a mandatory hook fails (activation redirect, D3D11CreateDevice) |
| iat_hooks | `static void Win32CraftParseHostOptions(void)` | existing; ARM adds `--feature-level` / env |
| platform_fixes | `static BOOL install_arm32_platform_fixes(BYTE *image)` | pickFile cell, zip/directory getAsset overrides, Xbox sign-in string patches |
| input_hid | `static BOOL install_arm32_input_hooks(BYTE *image)` | the three mouse-mode lambda cells |
| input_hid | `queue_*`, `get_hid_controller`, ... | existing names kept (ARM bodies) |
| client_state | `static BOOL install_client_state_hook_128(BYTE *image)` | existing name, ARM body |
| client_state | `static void refresh_relative_mouse_from_client_128(void)` | existing name, ARM body |
| gfx_bootstrap | `static BOOL initialize_game_d3d(HWND window)` | existing name, ARM body (creates swap chain, owner, calls game InitDevice/InitTargets) |
| gfx_bootstrap | `static BOOL resize_game_window(HWND window, UINT width, UINT height)` | existing name, ARM body |
| appmain | `static BOOL initialize_game_app_128(BYTE *image)`, `static BOOL activate_game_app_128(HWND window)` | existing names, ARM bodies |
| appmain | `static int render_game_target_128(void)` | ARM body returns nonzero when the game asked to quit (see facts appmain frame return value) |
| bootstrap_main | `int WINAPI Win32Bootstrap(void)` | ARM variant: identification, then calls the contract above in order |
| hstring_native | api_compat.c exports | HSTRING forwarding to combase; `Win32CraftActivationDispatcher` hook (see unit) |

## Region ownership (names refer to functions/data in the PRISTINE `arm32/orig/win32_host.c`; grep for them)
* **00_core** (runs first, single agent): see its brief. Owns: includes of `win32_host_arm32.h`/`win32_host_116.inl`/`bootstrap_116`/early2016
  guards, all x86 inline-asm bridges (`call_game_*`, `win32_115_restore_adapter_references`), the shared helpers above, crash/diagnostic
  handlers (`win32_unhandled_exception`, `install_crash_trace`, `win32_vectored_exception`, `log_115_termination_stack` and any other
  `CONTEXT.E**` user), and stubs so `win32_host.c` + `api_compat.c` compile clean on ARM.
* **iat_hooks**: `install_exception_trace`/`win32_cxx_throw`, `install_activation_redirect`, `install_vccorlib_compat` (uncalled on ARM),
  `install_d3d_compile_trace`/`win32_d3d_compile` and the HLSL/include helpers they use, `install_d3d11_device_compat` + its hook function,
  `install_create_file2_compat`, `install_xbox_tcui_noops_128` + the `win32_*_noop` hook functions, `Win32CraftParseHostOptions`/`host_next_argument`,
  `install_scissor_fix`/`win32_set_scissor_rects` (review only: keep behaviour), new `install_arm32_iat_hooks`.
* **gfx_bootstrap**: `initialize_game_d3d` and the helper code that creates the swap chain/owner/renderer (~lines 11300-11880), `resize_game_window`,
  `win32_present`/`install_swap_chain_present_hook` (Present slot 8 + Present1 slot 22), owner+0x94/+0x98 size mirroring, viewport/RT binding
  helpers, backbuffer size clamp for FL9_x.
* **appmain**: `initialize_game_app_128`, `activate_game_app_128`, `render_game_target_128`, `current_app_platform`, visibility handling.
* **input_hid**: `get_hid_controller`, `mouse_actual_offset`, `mouse_command_offset`, `install_mouse_mode_hooks` + the hook bodies, `queue_*`
  event builders (mouse/keyboard/character/wheel), `game_requests_relative_mouse`, `set_game_relative_actual`, `sync_hid_pointer_scale`,
  hid key-held logic, new `install_arm32_input_hooks`.
* **client_state**: `plausible_client_instance_128`, `win32_client_render_128`, `install_client_state_hook_128`, `client_world_active_128`,
  `top_scene_128`, `refresh_relative_mouse_from_client_128`.
* **platform_fixes**: `win32_pick_custom_skin_128`, `install_win32_platform_fixes_128`, the ZIP/lang/start_screen override code
  (`install_128_zip_resource_overrides`, `patch_128_zip_slot`, `win32_128_zip_get_contents` and helpers, ~lines 8480-9115), `RuntimeBytePatch`/`apply_runtime_patches`,
  `patch_xbox_actions_noop`, `patch_chat_textbox_compat` (no-op on ARM), new `install_arm32_platform_fixes`.
* **bootstrap_main**: `Win32Bootstrap`, `DllMain`, `window_proc` (only additive ARM changes, e.g. WM_GETMINMAXINFO texture-size clamp), the
  main loop, `IMAGE_SIZE_*` constants for ARM, `create_user_data` defaults review, the exported `D3D11CreateDevice`/`CreateDirect3D11DeviceFromDXGIDevice`/`CreateDXGIFactory2` tail of the file.
* **hstring_native**: all of `api_compat.c` and any HSTRING handling in `win32_host.c` outside the regions above.
* **abi_audit**: read-only audit of the fake-WinRT shim (everything not owned above, roughly lines 1490-8200: `fake_*` objects, vtables, `win32_get_activation_factory`,
  fmod fakes) for ARM ABI problems; edits only inside that shim span.
