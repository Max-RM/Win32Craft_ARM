# Minecraft 1.14.30.2 (Bedrock, x86 UWP)

* Game: Minecraft Bedrock Edition by Mojang / Microsoft
* Version: 1.14.30.2, x86 UWP APPX (package
  `Microsoft.MinecraftUWP_1.14.3002.0_x86__8wekyb3d8bbwe`)
* Engine: native C++ Bedrock engine with ChakraCore and cohtml UI; no .NET game logic
* Host: `win32craft.dll` (Win32Craft x86 host), profile
  `game_profile_114_3002`, selected by `SizeOfImage == 0x02857000`
* Patcher: `tools/1.14.3002.0-patcher.py`
* Package exe: `Win32Craft.1.14.30.2.exe` (the stock `Minecraft.Windows.exe`
  is removed; `--exe-name` overrides the name)
* Package marker: `.Win32Craft` containing `Minecraft Bedrock Edition`;
  `*.p7x` and `*.i64` files are removed
* Status: runs as an unpackaged Win32 x86 process on Windows 10/11. Main menu,
  world loading, mouse capture and exit -> world re-entry work. The world was
  loaded, exited with `Save & Quit` and loaded again with no exception. Audio
  was not re-measured in this session; the shipped UWP FMOD 1.09.04 build is
  expected to hit the same `FMOD_ERR_OUTPUT_INIT` limitation as 1.14.60.5.

## What works

* Full startup: package/application-data redirection, CoreApplication view,
  CoreWindow, menu and world loading. The window title is built by the host as
  `Win32Craft 1.14.30.2`.
* D3D11 through the host's HWND swap chain, with the shared 1.16 renderer
  (single present per frame, `ALLOW_TEARING` stripped from `ResizeBuffers`).
* World entry: the stock exe crashes in the first Chakra script-interface
  predicate call in the 1.14 world loader. The patcher replaces that call with
  `mov al,1` + NOPs at `0xE956A5`, so the loading screen finishes and the world
  loads.
* World re-entry: the client script engine creates four `Script*EventListener`
  objects per world and deletes the old ones without unregistering them; the
  server engine does the same for its `ServerInstanceEventListener`. The
  patcher unlinks the stale listeners before deletion (and keeps the server
  engine allocation), so `Save & Quit` -> load world works repeatedly. Verified
  with a world load, one `Save & Quit` cycle and a second load, no AV.
* Mouse capture: the host trusts the game's explicit
  `CoreWindow.SetPointerCapture` / `ReleasePointerCapture` calls and treats the
  base `UIScene` flags as a tri-state (`menu=1` release, `game=1` capture,
  `0/0` keep the current state). `put_PointerCursor(NULL)` is a relative-mouse
  request; a non-NULL cursor restores the arrow.
* FMOD delay imports: the profile selects the 1.14.30.2 descriptor
  (`0x020FB140`), all 42 names resolve against the shipped `fmod_X86.dll`, and
  the common calls go through the shared diagnostic bridge.
* User data: `%APPDATA%\MinecraftPE\games\com.mojang`; `WIN32CRAFT_DEBUG=1`
  writes `win32craft.log` next to the exe.

## What does not work yet

* Audio output. Not re-measured for 1.14.30.2, but the shipped DLL is the same
  UWP FMOD 1.09.04 build that fails with `FMOD_ERR_OUTPUT_INIT` (28) in
  1.14.60.5. Expect a silent game.
* The world-entry fix is an exe-side workaround, not a host fake for the
  Chakra script interface. It skips the first virtual call of the loader; the
  call is not reimplemented.
* Survival block breaking, window resize and the skin picker are inherited
  from the shared host but were not re-measured in the 1.14.30.2 session (the
  test world was Creative).

## First things to look at

1. `win32_host_116.inl`: `game_profile_114_3002` and the dispatch in
   `bootstrap_116`. The profile is selected from `SizeOfImage == 0x02857000`
   after patching.
2. `tools/1.14.3002.0-patcher.py`: the PE/import surgery, the `.d3imp`
   section, the `WinMain` bootstrap call, the `0xE956A5` world-entry skip and
   the listener-unlink stubs.
3. `win32_host.c`: the shared 1.16/1.14 bootstrap, the pointer-capture slots
   and `resolve_fmod_delay_imports_116`.
4. Package: rebuild with
   `python tools\1.14.3002.0-patcher.py SOURCE OUTPUT --package --host PATH`.
   The ready folder contains `Win32Craft.1.14.30.2.exe`, `win32craft.dll`,
   `fmod.dll`, `.Win32Craft` and `PATCH_NOTES.md`; the stock
   `Minecraft.Windows.exe` is not kept.
