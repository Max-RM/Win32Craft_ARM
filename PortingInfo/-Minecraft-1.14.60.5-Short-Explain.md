# Minecraft 1.14.60.5 (Bedrock, x86 UWP)

* Game: Minecraft Bedrock Edition by Mojang / Microsoft
* Version: 1.14.60.5, x86 UWP APPX (package
  `Microsoft.MinecraftUWP_1.14.6005.0_x86__8wekyb3d8bbwe`)
* Engine: native C++ Bedrock engine with ChakraCore and cohtml UI; no .NET game logic
* Host: `win32craft.dll` (Win32Craft x86 host), profile `game_profile_114`,
  selected by `SizeOfImage == 0x028B8000`
* Patcher: `tools/1.14.6005.0-patcher.py`
* Package exe: `Win32Craft.1.14.60.5.exe` (the stock `Minecraft.Windows.exe`
  is removed; `--exe-name` overrides the name)
* Package marker: `.Win32Craft` containing `Minecraft Bedrock Edition`;
  `*.p7x` and `*.i64` files are removed
* Status: runs as an unpackaged Win32 x86 process on Windows 10/11. Main menu,
  world loading, mouse capture in gameplay and survival block breaking work.
  The game is silent on the test machine: FMOD resolves all 42 delay imports,
  but the UWP FMOD output does not initialize (`FMOD_ERR_OUTPUT_INIT`), the
  same limitation as the 1.16 ports.

## What works

* Full startup: package/application-data redirection, CoreApplication view,
  CoreWindow, menu and world loading. The window title is built by the host as
  `Win32Craft 1.14.60.5`.
* D3D11 through the host's HWND swap chain, with the shared 1.16 renderer
  (single present per frame, `ALLOW_TEARING` stripped from `ResizeBuffers`).
* World entry: the stock exe crashes in the first Chakra script-interface
  predicate call in `sub_12EFEB0`. The patcher replaces that call with
  `mov al,1` + NOPs, so the loading screen finishes and the world loads.
* Mouse capture: the host trusts the game's explicit
  `CoreWindow.SetPointerCapture` / `ReleasePointerCapture` calls and treats the
  base `UIScene` flags as a tri-state (`menu=1` release, `game=1` capture,
  `0/0` keep the current state). `put_PointerCursor(NULL)` is a relative-mouse
  request; a non-NULL cursor restores the arrow.
* Survival: `CoreWindow.GetAsyncKeyState`/`GetKeyState` now return the real
  mouse-button state for VK 1-6. Before this, holding the attack button looked
  like a one-frame tap and block breaking never progressed.
* FMOD delay imports: the profile selects the 1.14 descriptor (`0x0215C0F0`),
  all 42 names resolve against the shipped `fmod_X86.dll`, and the common calls
  go through the shared diagnostic bridge.
* User data: `%APPDATA%\MinecraftPE\games\com.mojang`; `WIN32CRAFT_DEBUG=1`
  writes `win32craft.log` next to the exe.

## What does not work yet

* Audio output. `System_Create`, `getVersion` and `setOutput(WASAPI)` succeed,
  but `init` returns 28 (`FMOD_ERR_OUTPUT_INIT`); DirectSound and WinMM fail
  with 54 (`FMOD_ERR_OUTPUT_CREATEBUFFER`). The shipped DLL is the UWP FMOD
  1.09.04 build, so this looks like a package-identity/audio-graph limitation
  rather than a missing import. The game continues silently.
* The world-entry fix is an exe-side workaround, not a host fake for the
  Chakra script interface. It skips the first virtual call of
  `sub_12EFEB0`; the call is not reimplemented.
* Window resize and the skin picker are inherited from the shared 1.16 host
  but were not re-measured in the 1.14 test session.

## First things to look at

1. `win32_host_116.inl`: `game_profile_114` and `bootstrap_116`. The profile is
   selected from `SizeOfImage == 0x028B8000` after patching.
2. `tools/1.14.6005.0-patcher.py`: the PE/import surgery, the `.d3imp`
   section, the `WinMain` bootstrap call and the `0xEEFEF5` world-entry skip.
3. `win32_host.c`: `fake_core_window_key_state`, the pointer-capture slots and
   `resolve_fmod_delay_imports_116`.
4. Package: rebuild with
   `python tools\1.14.6005.0-patcher.py SOURCE OUTPUT --package --host PATH`.
   The ready folder contains `Win32Craft.1.14.60.5.exe`, `win32craft.dll`,
   `fmod.dll`, `.Win32Craft` and `PATCH_NOTES.md`; the stock
   `Minecraft.Windows.exe` is not kept.
