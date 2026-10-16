# Win32Craft ARM32 (Minecraft 1.2.8 on Windows RT 8.1 / Windows 10 ARM32)

> **Status: nothing here has been run on a device.** The port was built on an x64 machine that cannot execute ARM32 code.
> Everything was checked statically only (compile, link, disassembly, byte-level patch verification, emulation of
> individual routines). Expect the first run to fail somewhere; the host writes a detailed `win32craft.log` precisely so a
> failure can be diagnosed from that file. See "Reporting a failure".

## What this is

The Minecraft 1.2.8 ARM32 build (`Minecraft.Windows.exe`, a UWP/C++/CX app) cannot be started outside its app container.
Win32Craft turns it into an ordinary desktop process:

* `tools\1.2.8-arm32-patcher.py` patches the exe (clears the AppContainer flag, renames the `*_APP.dll` runtime imports to
  the desktop VC++ redistributables, redirects `XINPUTUAP.dll` to `win32craft.dll`, adds an import for
  `win32craft.dll!Win32Bootstrap` and redirects the exe's startup call to it) and patches `fmod_ARM.dll` the same way.
* `win32craft.dll` (ARM32 Thumb-2, built from `win32_host.c` + `api_compat.c`, ARM code under `#ifdef WIN32CRAFT_ARM32`)
  is the host. It creates the window, a D3D11 device at **feature level 9_1**, fakes the WinRT classes the game needs
  (CoreWindow, Package, ApplicationData, input pane, CurrentApp, ...), hooks input, and runs the game's main loop.
* The x86 build output is unchanged by the ARM32 work (checked with `tools\arm32\check_x86.py`).

Supported game build: exactly the ARM32 1.2.8 exe with SHA-256
`b01d5f1237d2daad8cc076d7218f0788751b55a9576c779ccdba08eef6fcfcf8` (23,595,520 bytes). Any other exe is refused.
You must supply the game files yourself; none are included.

## Requirements

* Windows RT 8.1 or Windows 10 ARM32 (desktop). ARM64 Windows is not a target of this build.
* Universal C Runtime: built in on Windows 10. On Windows RT 8.1 the device folder ships an app-local copy (ucrtbase.dll and the
  `api-ms-win-*.dll` forwarders, built by `tools\make_arm32_apiset_shims.py`), so KB2999226 is not required; Windows 8.1 lacks
  contracts such as `api-ms-win-core-heap-l2-1-0` and the loader then takes these files from the exe's folder.
  Confirmed on a real Windows RT 8.1 device (6.3 build 9600): only `heap-l2-1-0`, `libraryloader-l2-1-0` and the `api-ms-win-crt-*` set come from the
  folder, every other contract is native. For any "missing DLL / entry point not found" error run `loadcheck.exe` (in the device folder) and send `loadcheck.txt`.
* Direct3D 11.2 interfaces (present on RT 8.1 and Windows 10; they are queried without a fallback). The device is created at
  feature level **9_1** by default (devices of this class top out there). All shader variants were verified to compile
  for `*_4_0_level_9_1` offline; textures and the back buffer must stay at or below 2048 px.
* The desktop VC++ ARM redistributable DLLs, copied next to the exe: `vccorlib140.dll`, `msvcp140.dll`, `concrt140.dll`,
  `vcruntime140.dll` (from `VC\Redist\MSVC\<ver>\onecore\arm\Microsoft.VC143.CRT`). `msvcp140` and `concrt140`
  import only `vcruntime140`/`msvcp140` among the VC family, so these four are the complete set (verified with
  `llvm-readobj --coff-imports` on the patched exe, `win32craft.dll`, `fmod_ARM.dll` and the redistributables).
  `D3DCOMPILER_47.dll` is loaded from the system.

## Build (win32craft.dll)

Run from `cmd.exe` in the repository root:

```
build_arm32.bat
```

Output: `dist-arm32\win32craft.dll` (Machine ARMNT). Needs the Windows RT LLVM toolchain (clang + lld-link, target
`thumbv7-pc-windows-msvc`) and the ARM import libraries. Defaults point at the developer machine; override with
environment variables:

| variable | default |
|---|---|
| `LLVM_BIN` | `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin` |
| `VCLIB` | `D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\arm` |
| `SDKROOT` / `SDKVER` | `D:\Windows Kits\10` / `10.0.19041.0` |
| `BUILD` / `DIST` | `build-arm32` / `dist-arm32` |
| `CC` / `LINKER` | `%LLVM_BIN%\clang.exe` / `%LLVM_BIN%\lld-link.exe` |

The DLL exports (see `win32craft_arm32.def`) are undecorated, including `Win32Bootstrap` and `XInputGetState`.

Regression checks used during the port (run from the repository root): `python tools/arm32/check_x86.py` (x86 build
identical to pristine) and `python tools/arm32/verify_constants.py`.

## Patching

```
python tools\1.2.8-arm32-patcher.py <Minecraft.Windows.exe> <output.exe> [--redirect-api-sets]
python tools\1.2.8-arm32-patcher.py --fmod-dll <fmod_ARM.dll> --fmod-out <patched fmod_ARM.dll> [--fmod-keep-winrt-init]
```

Both can be combined in one command. Defaults: source `Minecraft.Windows.exe` in the current directory, output
`Win32Craft.1.2.8-arm32.exe`.

* Exe input must have SHA-256 `b01d5f12...fcfcf8` (see above). Output: patched exe with SizeOfImage `0x01731000`; the host
  uses that value to recognise the patched build and refuses any other image (return code 3).
* `--redirect-api-sets` additionally points six api-set imports (`api-ms-win-core-com-l1-1-0`, `...winrt-l1-1-0`,
  `...winrt-string-l1-1-0`, `...winrt-error-l1-1-0`/`-1-1`, `api-ms-win-eventing-provider-l1-1-0`) at `win32craft.dll`.
  Off by default: those api sets exist natively on Windows 8.1/10 and the host intercepts WinRT activation through IAT hooks
  instead. Use it only if a log shows the native WinRT/COM calls going wrong.
* `fmod_ARM.dll` input must have SHA-256
  `5f92f5ac9317c7a5d61ab6b43a2e799ae90eb7eee5c992c85d0325ba1d09fc38` (1,118,720 bytes, FMOD 1.09.04). The fmod patch renames
  `VCRUNTIME140_APP.dll` to `VCRUNTIME140.dll`, clears the AppContainer flag, and skips FMOD's `Windows.ApplicationModel.Package`/
  `ApplicationData` initialisation (it fails without package identity and would make `System::init` return
  `FMOD_ERR_INTERNAL`). `--fmod-keep-winrt-init` leaves that code alone (only for an experimental WinRT hook; not needed
  normally).
* Windows RT 8.1 api-set availability for the fmod DLL imports (e.g. `processthreads-l1-1-2`, `file-l1-2-1`,
  `sysinfo-l1-2-1`) could not be checked offline. If the loader reports a missing api set for `fmod_ARM.dll`, use the silent
  fallback below.

## Deployment layout and package script

`python tools\make_arm32_package.py` runs the patcher on the original game folder (default
`Minecraft_1.2.8.0_ARM32`, override with `--game-dir` or `WIN32CRAFT_GAME_DIR`) and assembles
`dist-arm32\package\` as an **overlay** (it never modifies the original folder). Options: `--use-existing`,
`--build`, `--redist`/`WIN32CRAFT_REDIST`, `--llvm-bin`/`LLVM_BIN`, `--redirect-api-sets`, `--force`. The package holds:

```
Win32Craft.1.2.8-arm32.exe     patched exe
fmod_ARM.dll                   patched real FMOD (replaces the game's original of the same name)
win32craft.dll                 host
vccorlib140.dll msvcp140.dll concrt140.dll vcruntime140.dll
fmod-silent\fmod_ARM.dll       silent fallback FMOD (do not deploy both)
MANIFEST.txt                   SHA-256 of every file and of the input exe/fmod
INSTALL.txt
```

Install: copy the game folder (the one with `Minecraft.Windows.exe`, `data\`, `resources.pri`) to a writable place outside
`WindowsApps`, **rename its `fmod_ARM.dll` to `fmod_ARM.dll.orig` (backup)**, then copy the package files (except
`MANIFEST.txt`, `INSTALL.txt`, `fmod-silent`) into it. The original `Minecraft.Windows.exe` stays untouched next to the
patched one. Start `Win32Craft.1.2.8-arm32.exe`. The host takes the exe's folder as the package root (assets are read from
there) and keeps user data in `%APPDATA%\MinecraftPE` (fake `ApplicationData.LocalFolder`; a default `options.txt` with MSAA/TexelAA
forced off is created if missing).

## Silent FMOD fallback

If the patched real FMOD fails to load or hangs (its init can block on WinRT device events), use a stub that implements the
39 FMOD functions the game uses with no output and no imports:

```
build_fmod_null_arm32.bat          -> dist-fmod-null-arm32\fmod_ARM.dll   (RUN_TESTS=1 also runs arm32\tests\fmod_null_test.py)
```

Put it in the game folder as `fmod_ARM.dll` instead of the patched one (the package script ships it as
`fmod-silent\fmod_ARM.dll`). The game then runs without sound. Source: `fmod_null\`.

## Runtime options

Command line (after the exe name) and environment, parsed by `Win32CraftParseHostOptions`:

| option | effect |
|---|---|
| `--feature-level <L>` or `--feature-level=<L>` | cap for the D3D11 device feature level; `L` is one of `9_1 9_2 9_3 10_0 10_1 11_0 11_1 game` (`game` = do not clamp). Default `9_1`. |
| `WIN32CRAFT_FEATURE_LEVEL=<L>` (environment) | same; a command-line value overrides it. Invalid values are logged and ignored. |
| `-maxfps` | disable VSync (present interval 0). Default: display VSync. |
| `--input=auto\|touch\|mouse` (also `--input auto`), env `WIN32CRAFT_INPUT` | which device drives the game. `auto` (default): the last used device wins - a finger switches the game to Touch mode (on-screen touch controls), a real mouse/keyboard switches it back. `touch`: pinned to touch (no mouse capture). `mouse`: touch is left to Windows' mouse emulation (the pre-touch behaviour, no touch HUD). |
| `--touch-api=auto\|pointer\|wmtouch\|legacy`, env `WIN32CRAFT_TOUCH_API` | which Windows API delivers the fingers. `auto`/`pointer`: `WM_POINTER*` (Windows 8+, multi-touch). `wmtouch`: `WM_TOUCH`. `legacy`: single finger from the emulated mouse messages. Only change this if the log says touch does not work. |
| `--osk=auto\|on\|off`, env `WIN32CRAFT_OSK` | Windows touch keyboard (TabTip) for chat / text boxes. `auto` (default): only while touch is the active input; `on`: always; `off`: never. |
| `--input-log`, env `WIN32CRAFT_INPUT_LOG=1` | log every touch event and source decision (default: the first 300 down/up lines and all mode switches). |
| `WIN32CRAFT_DEBUG` | logging is enabled by default on ARM32 (the host sets it itself); only the presence of the variable matters. |

Higher levels than the GPU supports make device creation fall back as described in the log (`D3D11CreateDevice ... FAILED`; the game
retries once with WARP). Note that textures above 2048 px only work above 9_x.

### Log file

`win32craft.log` is appended to the **working directory, which the host first sets to the exe's folder**. If that folder is not
writable, the log goes to `%TEMP%` (the line `log file: <path>\win32craft.log (...)` near the top says exactly where). Start
a new log by deleting the file beforehand; it is never truncated. A hung start is reported by a watchdog thread (`ARM32 watchdog`
lines) and a heartbeat every 30 s.

## Touch input (no keyboard / mouse attached)

**Symptom that this fixes:** on a touch-only device (Surface RT without a cover) the menus worked by touch, but the on-screen game controls
(joystick, jump, sneak ...) never appeared. There is **no game setting** for them and the game does **not** ask Windows which devices are attached:
it chooses its input mode (Mouse / Touch / GamePad) from the *kind of input items it receives* (`InputHandler::tick`), and the host used to feed it
mouse items only (Windows turns a finger into emulated mouse messages when a window does not handle touch itself). The touch controls are drawn natively by
`TouchMapper::render` and only while the input mode is Touch.

**What the host does now** (all ARM32-only code in `win32_host.c`, "ARM32 touch input" block in front of `window_proc`; facts in
`arm32/facts/touch_game.*`, `touch_host.*`, `touch_impl.json`):

* `WM_POINTERDOWN/UPDATE/UP/CAPTURECHANGED` of **touch** pointers are consumed (no `DefWindowProc`, so Windows sends no emulated mouse messages) and queued as the game's own
  `MultitouchItem` (factory 0x62a174, pointer slots 0..11, press `(1,1)`, move `(0,0)`, release `(1,0)`, coordinates scaled to the swap-chain buffer exactly like the
  mouse). The first press switches the game to Touch mode by itself. Mouse and pen pointers are left to `DefWindowProc`, so mouse/keyboard play is unchanged.
* "Last used wins": a real mouse movement/click or a key press makes mouse/keyboard the active *host* source again (touches are released, relative mouse capture is allowed
  again, the game's TouchMapper is reset so it does not keep winning the mode); the next touch switches back. The *game* leaves Touch mode only when it receives mouse items
  (the game's KeyboardMapper never changes the input mode), so after a key press the touch controls stay on screen until the mouse/trackpad is moved or clicked.
  Plugging a keyboard cover therefore needs no notification.
* While touch is the active source the host never captures/clips/hides the cursor (`relative mouse suppressed (touch...)`), so the touch drag is not fought by the mouse-look code.
* A touch-only machine (ready digitizer, touch points, no real raw mouse or keyboard) starts the game directly in Touch mode (the `getDefaultInputMode` vtable cell is patched).
* Text boxes (chat, world name ...): when touch is active, `InputPane.TryShow` starts / toggles the Windows touch keyboard (`TabTip.exe`, window class `IPTip_Main_Window`)
  and `TryHide` closes it; the typed characters arrive as ordinary `WM_KEYDOWN/WM_CHAR`. The keyboard rectangle is reported as `OccludedRect`.
* Touch feedback (contact dot, press-and-hold ring, pen flicks) is switched off for the window. `EnableMouseInPointer` is never called.

**Fallbacks and switches** (command line, or `start_touch.bat` / `start_mouse.bat`): `--touch-api=wmtouch` (WM_TOUCH), `--touch-api=legacy` (single finger from emulated mouse; also
selected automatically when touch-sourced mouse messages arrive but no `PT_TOUCH` pointer message ever did), `--input=mouse` (exactly the old behaviour), `--osk=on|off`.

**Log lines** (grep `ARM32 input:`, `ARM32 touch:`, `ARM32 osk:`):

| line | meaning |
|---|---|
| `ARM32 effective input options: ...` | the options in effect and where they came from |
| `ARM32 input: user32 touch API: GetPointerType=yes ...` | which touch APIs this Windows has |
| `ARM32 input: raw device #N type=... name=...` and `ARM32 input: policy=... digitizer=0x... maxTouches=N mousePresent=N rawMice=N rawKeyboards=N touchOnlyMachine=N initialSource=...` | device inventory and the initial guess |
| `ARM32 input: touch handling active: api=pointer ...` | touch handling is installed (`api=` is the effective API) |
| `ARM32 touch: FIRST touch contact. HID state: ...` and `first PT_TOUCH WM_POINTERDOWN received` | the first finger reached the host through the pointer API |
| `ARM32 touch: down/up id=N slot=N client=(x,y) ...`, `item queued slot=N btn= data= client=() buffer=()` | every contact and the item handed to the game |
| `ARM32 input: InputHandler ... has 8 mapper(s): Gaze,Keyboard,Mouse,GamePad,Touch,...` | the game's input mappers; `TouchMapper is MISSING` means the game created no touch controls |
| `ARM32 input: game input mode Mouse -> Touch (host source=TOUCH ...)` | **the key line**: the game switched to Touch mode, the touch HUD can be drawn |
| `ARM32 input: source MOUSEKB -> TOUCH (WM_POINTERDOWN)` / `TOUCH -> MOUSEKB (key VK=0x41)` | the host's last-used-device decisions |
| `ARM32 input: touch-sourced mouse message 0x0201 extra=0xFF515780 ... dropped` | Windows still emulated a mouse from a finger although the pointer message was consumed (counted in `legacyFromTouch`) |
| `ARM32 touch stats (...)` | counters: down/up/moves/slotsFull/captureLost/legacyFromTouch/sourceSwitches |
| `ARM32 osk: show requested ...`, `launched TabTip.exe ... result=`, `touch keyboard window ... is visible`, `did NOT appear` | the touch-keyboard handling |

## Known limitations and risks

* Unverified on hardware. Windows RT 8.1 api-set/UCRT availability for the game and fmod imports is assumed, not tested.
* Feature level 9_1 limits: no MSAA / TexelAA, textures and back buffer at most 2048 px.
* The on-screen keyboard is the Windows touch keyboard (TabTip) driven by the host, unverified on RT 8.1 (see *Touch input*); mouse capture and
  text-input focus depend on a client-state hook whose failure is logged but not fatal.
* About 40 of 67 WinRT classes the game may ask for are not faked; a missing one will show up in the log as an activation
  failure.
* Real FMOD initialisation can block on WinRT events; use the silent fallback if the log stops after FMOD lines or the
  watchdog reports `NO PROGRESS`.
* The device-loss path is untested; the D3D11CreateDevice hook must stay installed for the life of the process.
* Only the exact exe and fmod hashes above are supported; no multiplayer/Xbox Live features (TCUI calls are no-ops).

## Troubleshooting (keyed to log lines)

Return codes of `Win32Bootstrap` (visible as the process exit code): 0 normal exit, 3 wrong SizeOfImage, 4 not an ARM32 PE,
10 IAT hooks, 11 platform fixes, 12 input hooks, 13 RegisterClass, 14 CreateWindow, 15 D3D, 16 AppMain, 17 activation.
The last `ARM32 bootstrap FAILED at step '...'` line names the step.

| symptom / log line | meaning and action |
|---|---|
| process dies before any window, no log, error 0xC0000135 / "missing DLL" | a redistributable or api-set DLL is missing: check the four VC DLLs sit next to the exe; on RT 8.1 install KB2999226. If the missing name is an api set of `fmod_ARM.dll`, use the silent FMOD. |
| no log at all | host not reached: patched exe not used, `win32craft.dll` missing/not ARM32. Run the patched exe from the game folder. |
| `image identification FAILED: SizeOfImage ... 0x01730000 is the stock exe` | the unpatched exe was started; run `Win32Craft.1.2.8-arm32.exe`. |
| `image identification FAILED: PE machine ...` / `Unknown Minecraft build` | wrong exe/architecture or other game version (return 4 / 3). |
| `warning: DllCharacteristics still has IMAGE_DLLCHARACTERISTICS_APPCONTAINER` | patcher step missing; re-patch. |
| `WIN32CRAFT_FEATURE_LEVEL='...' is not one of ...` / `--feature-level='...' is not one of ...` | bad option value, ignored. `ARM32 effective feature level cap: ...` shows what is used and its source. |
| `ARM32 hook ...: IAT cell ... is null` / `the loader did not resolve this import - not hooked` | an import is missing or the exe differs from the verified one; send the log. |
| `ARM32: D3D11CreateDevice hook NOT installed` / `ARM32 bootstrap FAILED ... mandatory hook missing` (code 10) | mandatory hook failed; send the log. |
| `ARM32: WinRT activation redirect NOT installed` | fake WinRT classes inactive; startup will fail later; send the log. |
| `ARM32 WARNING: host_is_128 is FALSE during WinRT activation` | fakes inactive (flag order bug); send the log. |
| `D3D11CreateDevice driver=1 FAILED: HRESULT 0x...` | no hardware device at the requested level; the game retries with WARP (`driver=5`). If WARP also fails the game crashes next. Try `--feature-level game`. |
| `D3D11CreateDevice driver=... OK ... selected feature level=0x9100` | healthy; `0x9100`=9_1. |
| `D3DCompile #N FAILED ...` / `D3DCompile failed source=...` | a shader failed to compile at the chosen level; send the log line (contains HRESULT and source name). |
| `FMOD bridge: ... missing export` / `LoadLibraryW(fmod.dll) failed` | x86-bridge lines; on ARM the game imports `fmod_ARM.dll` directly. If audio fails, use the silent FMOD. |
| `ARM32 watchdog: NO PROGRESS for N s in step '...'` | stall; `a game frame call has not returned` means the game itself is stuck. A thread snapshot follows. If the step is startup and FMOD is involved, use the silent FMOD. |
| `unhandled exception 0x... at ...` + `access violation` + `crash` register lines | crash; the context dump and addresses are what the developer needs. |
| `win32craft.log is not writable in the exe directory: working directory moved to %TEMP%` | read-only game folder; the log is under `%TEMP%`. |
| `win32craft.log cannot be created, GetLastError=N` | no log possible anywhere; run from a writable folder. |
| `RegisterClassExW failed` / `CreateWindowExW failed` (codes 13/14) | desktop/window problem; GetLastError is in the line. |
| `Minecraft 1.2.8 D3D bootstrap failed` (15), `AppMain bootstrap failed` (16), `activation bootstrap failed` (17) | startup step failed; send the log. |
| `Minecraft 1.2.8 client-state hook validation failed: continuing without it` | game keeps running, but mouse capture and text focus will not work. |
| `ARM32 input: game function bytes differ from the facts` | input injection may misbehave; send the log. |
| `ARM32 input: WARNING TouchMapper is MISSING from the input mapper list` | the game built no touch controls (it believed the device is holographic); send the log. |
| `ARM32 input: WARNING no PT_TOUCH WM_POINTER message was seen but touch-sourced mouse messages arrive` | pointer messages do not reach the window on this Windows; the host switched itself to `--touch-api=legacy` (single finger). Send the log. |
| touch menus work but `game input mode` never becomes `Touch` | send the log: look at `ARM32 touch: item queued` (are items queued?), `could not be queued`, the `mapper(s)` line and `ARM32 input target unavailable`. |
| `ARM32 osk: ... did NOT appear after 5 attempts` | the Windows touch keyboard could not be shown (class name / TabTip path differ on this Windows); type with a hardware keyboard and send the log. |
| `ARM32 getAsset overrides not installed` / `ZIP resource override installation failed` | resource overrides (language/screens) missing; cosmetic. |
| `ARM32 QI on fake kind=... IID=...` | a game query was answered with the generic fake; useful to the developer if a feature misbehaves. |

## Reporting a failure

1. Delete `win32craft.log`, start the game once, reproduce, close it.
2. Send the whole `win32craft.log` (from the exe folder, or `%TEMP%` if the log says so), plus: device model, Windows RT 8.1 or
   Windows 10 ARM32 and the build number (the log prints the OS version near the top), whether KB2999226 is installed, and
   which `fmod_ARM.dll` (patched or silent) was used.
3. If there was no log, say which message Windows showed (missing DLL name or error code).
