# Minecraft PE 0.10.4.40 (Windows Phone 8.1, ARM32) -> Win32Craft - working notes

Read together with `../RE_GUIDE.md` (toolkit + output contract; its paths mention the 1.2.8 exe: use the paths below) and `../PORT_RULES.md`.

## The target
* Exe: `D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]\MCPE.Host.WindowsPhone.exe` (read-only; never edit anything under `D:\repo\win32craft\0.10.4.40`).
  ARM Thumb-2 (machine 0x1C4), ImageBase 0x400000, SizeOfImage 0xE6E000, entry RVA 0x20095D, 12171 pdata functions, built with VS2013 (cl 18.00.31101).
* It is a **Windows Phone 8.1 XAML app** (AppxManifest has `mp:PhoneIdentity`, dependency `Microsoft.VCLibs.120.00.Phone`, `MainPage.xbf`, `App.xbf`), NOT a CoreWindow/IFrameworkView app like 1.2.8.
  The ISA is the same ARMv7 Thumb-2 as Windows RT 8.1, so the exe can run on a Surface RT once the WinRT/XAML surface it calls is faked and its VC120 runtime DLLs sit next to it.
* Hardware constraint (user): the ARM32 devices (Surface RT, Tegra class) top out at D3D feature level **9_1** - the game's D3D11 device and every shader profile must work at `_level_9_1` (ps_2_0-class, no SM4 features).
* WinRT classes the exe activates / uses (string scan, 2026-10-17): `Windows.UI.Xaml.{Window,Application,Controls.Page,Controls.Canvas,Controls.SwapChainPanel,DispatcherTimer}`,
  `Windows.UI.Core.{CoreWindow,CoreCursor}` (pointer + key handlers), `Windows.UI.ViewManagement.InputPane`, `Windows.Phone.UI.Input.HardwareButtons` (BackPressed),
  `Windows.Graphics.Display.DisplayInformation`, `Windows.System.{Threading.ThreadPool,MemoryManager}`, `Windows.ApplicationModel.{Package,Store.CurrentAppSimulator}`, `Windows.Storage.ApplicationData`,
  `Windows.Web.Http.*`, `Windows.Networking.Connectivity.NetworkInformation`, `Windows.Phone.Devices.Notification.VibrationDevice`, `Windows.Foundation.Diagnostics.AsyncCausalityTracer`;
  imports D3DCOMPILER_47 (D3DCompile/D3DReflect), d3d11, WS2_32, msvcr120_app / msvcp120_app / vccorlib120_app (activation via `vccorlib120_app!GetActivationFactoryByPCWSTR`).

## How to run the toolkit for this exe (labels are stored per exe automatically)
```
PowerShell:  $env:ARM32_EXE = 'D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]\MCPE.Host.WindowsPhone.exe'
             $env:ARM32_FACTS_DIR = 'D:\repo\Win32Craft-main\arm32\v0_10_4\facts'     (only for facts_survey / gen_constants / verify scripts)
python tools/arm32/arm32re.py info | func | dis | xref | strx | rtti | vtable | impcallers | imports ... name <rva> <label>
```
Without `ARM32_EXE` the toolkit analyses the 1.2.8 exe: ALWAYS set it. The index builds in seconds (small exe).
Addresses: IDA VA = 0x400000 + RVA. The friend's notes mix `sub_5DCE2C` (VA) and RVAs (0x1DCE2C): always convert and always re-derive; the toolkit prints RVAs.

## The friend's attempt (hint material only - NOT trusted, do not copy code)
Their handoff `D:\repo\win32craft\0.10.4.40\WORK_SUMMARY.md` (+ `Win32Craft_ARM\arm32\facts_0104\notes.md`, patcher `Win32Craft_ARM\tools\0.10.4.40-arm32-patcher.py`, host block `host_is_0104`/`initialize_game_*_0104` in `Win32Craft_ARM\win32_host.c` ~line 22166+) says:
the game starts and compiles ~45 shaders, the window stays blank/white, `Draw=0` because they bypass the XAML `MainPage` and tick `AppMain` directly; the `StartMenuScreen` constructor crashes because
`MinecraftClient+0x118` (texture manager) and `+0x11C` (Font) are NULL - the client init that MainPage/App normally triggers is never run. Their claims are unverified (they were produced with a weaker model); the audit in `facts/friend_audit.*` grades each one.

## Architecture question (answer comes from the recon facts)
(a) bypass XAML and call DisplayResources/AppMain directly (friend's approach; needs every init step the XAML path performs to be replayed by the host), or
(b) faithful: patch only `MainPage::InitializeComponent` (XAML loader) / `Application::Start`, fake the small WinRT/XAML surface the game code really calls (like the 1.2.8 fake CoreWindow), and let the game's own MainPage/App code run.
Decision rule: pick (b) if the game-side MainPage logic after InitializeComponent is small and only touches a short list of faked objects; otherwise a hybrid that replays the exact MainPage sequence from the binary.

## Layout
| item | path |
|---|---|
| recon / RE facts (`<area>.json/.md/.verdicts.json`, `friend_audit.*`, `audit.json`) | `arm32/v0_10_4/facts/` |
| scratch | `arm32/v0_10_4/analysis/` |
| per-exe labels | `tools/arm32/names/Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]__MCPE.Host.WindowsPhone.exe.jsonl` |
| host code (later) | `win32_host_arm32_0104.inl` in this repo, ONE `win32craft.dll` for all versions chosen at runtime by PE machine + SizeOfImage |


## Host foundation (2026-10-17) - read this first when you implement
* `HOST_DESIGN.md` is the single source of truth (architecture, threads, ordering rules R1-R14, toolkit, bootstrap, cross-unit API). `units/<async|app|panel|platform|input>.md` are the implementer briefs, `units/SLOTS.md` the generated slot/ABI tables.
* Code: `win32_host_arm32_0104.inl` (umbrella, included from win32_host.c in front of `Win32Bootstrap`), `_core.inl` (toolkit), `_boot.inl` (bootstrap), five unit files (IMPLEMENTED 2026-10-18; the pristine foundation stubs are kept in `stubs/` for the per-unit compile checks), `_iids.inl` (generated by `tools/arm32/gen_0104_iids.py`).
* Tools: `python tools/arm32/try_0104.py [--unit <key>] [--selftest]` (compile + link + x86 identity), `python tools/arm32/test_0104_core.py` (x86 host-side self test of the toolkit), `python tools/arm32/check_x86.py`.

## Foundation review fixes (2026-10-17) - what changed for implementers
The independent review of the foundation found 12 defects and 11 coverage gaps; everything is fixed in place and explained in `HOST_DESIGN.md` section 14 ("Decisions after review"). Short list of what you will notice:
* `X104_TIMER_PUMP`, `X104_WM_*` and the new helpers live in the CORE; a unit may use only core/iids/header/win32_host.c symbols (lint). The boot is included last.
* Register every slot with `X104_SLOT(Interface, Method, thunk)` (signature-checked against the generated `X104_FN_<Interface>_<Method>` typedefs; `X104_SLOT_NOARGS` for `x104_nop`-style thunks). Enums are `int`, interface pointers `void *`, output structs through pointers, input `Size/Point/Rect` by value.
* Exit flags are split: `f_exit_requested` (app only), `f_quit` (async only), `f_loop_exit` (async + window proc). Suspending only when `x104_game_is_ready()`.
* The UI loop calls the core's `x104_ui_pass()` once per iteration; the 10 ms WM_TIMER runs the same pass inside modal loops; the loop leaves on `f_loop_exit`.
* `x104_aggregate_create()` implements the outer/inner/instance protocol (Application, Page, DispatcherTimer). Page returns the IPage view; OnLaunched gets the ILaunchActivatedEventArgs view.
* The window is created hidden and shown by `Window.Activate`; default size fits the work area. Messages are routed to the units only after `f_units_ready`.
* Panel: path B records the chain it creates, `SetSwapChain` verifies it (fallback to path A with an ERROR), a 3 s rule logs a black window, `Present` (slot 8) is bracketed by `x104_present_begin/_sync/_pace/_end`, device diagnostics for Tegra.
* Input: keep `text_input_active` in step through `x104_set_text_input_active()`; deliver and consume WM_SYSKEYDOWN/UP (except Alt+F4).
* Tools: `try_0104.py` lints; `test_0104_core.py` has 130 checks + the X104_SLOT negative-compile tests; `--selftest-fakes` runs on the device (`start_selftest.bat`); `tools/make_0104_package.py` builds the device package (`--allow-stubs` for the foundation dry run).

## Integration (2026-10-18)
All five units are implemented and integrated: read `INTEGRATION_NOTES.md` (what exists, what only a device can prove, the log line per milestone, the device test plan) and `HOST_DESIGN.md` section 14b. Whole-tree check: `python tools/arm32/try_0104.py --selftest --integration` (x86 mock-game integration test `tools/arm32/test_0104_all.c`).

## STATUS (paused by the user 2026-10-18)
* DONE: recon (RECON_SUMMARY.md); host foundation + review (HOST_DESIGN.md, units/*.md, core/boot/iids/selftest .inl, tools/arm32/try_0104.py); patcher (tools/arm32-patcher.py, PATCHER_NOTES.md); the five unit files implemented (~7.6k lines) with per-unit independent reviews (fixes applied for async + app; the panel/platform/input review summaries were lost with a dropped connection - the integrator re-read those three files and found no defects); full-tree integration (builds, x86 identical, selftests: core 130 + async 313 + app 161 + input 440 + platform + whole-tree mock-game integration 149 checks) - see INTEGRATION_NOTES.md.
* NOT done: the two independent cross-unit reviews (ordering/threading lens, ABI/slot-coverage lens) and the fixes they would trigger; the 0.10.4 device package (`python tools/make_0104_package.py`); any device run.
* Resume (cached agents replay for free, only the missing cross-unit reviews + final-fix run): Workflow({scriptPath: "D:\repo\Win32Craft-main\arm32\v0_10_4\analysis\v0104-host-units.js", resumeFromRunId: "wf_1e3dbe62-6d8"}). Cheaper alternative: skip the workflow and go straight to packaging + a device test.
* Nothing has run on a device yet; first device step = `--selftest-fakes`, then the game.
