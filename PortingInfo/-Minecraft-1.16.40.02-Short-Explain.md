# Minecraft 1.16.40.02 (Bedrock, x86 UWP)

* Game: Minecraft Bedrock Edition by Mojang / Microsoft
* Version: 1.16.40.02, x86 UWP APPX (build 2020-09-03, package
  `Microsoft.MinecraftUWP_1.16.4002.0_x86__8wekyb3d8bbwe`)
* Engine: native C++ Bedrock engine with Windows inbox Chakra JavaScript
  (`ChakraCore.dll` remains for cohtml), cohtml UI; no .NET game logic
* Host: `win32craft.dll` (Win32Craft x86 host), profile `game_profile_116_04002`
* Patcher: `tools/1.16.40.02-patcher.py`
* Package exe: `Win32Craft.1.16.40.02.exe` (the stock `Minecraft.Windows.exe`
  is removed; `--exe-name` overrides the name)
* Package marker: `.Win32Craft` containing `Minecraft Bedrock Edition`;
  `*.p7x` and `*.i64` files are removed
* Status: runs as an unpackaged Win32 x86 process on Windows 10/11. Main menu,
  world loading, skin import, window resize and the `Win32Craft 1.16.40.02`
  title work. The game is silent on the test machine: FMOD loads and resolves
  all 48 delay imports, but the UWP FMOD output does not initialize
  (`FMOD_ERR_OUTPUT_INIT`) and the AudioGraph fallback is suppressed.

## What works

* Full startup: package/application-data redirection, PnP metadata, CoreApplication
  view creation, CoreWindow, menu, world creation/loading and in-game rendering.
* D3D11 through the host's HWND swap chain. The game's `ResizeBuffers` call uses
  `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` on a `DISCARD` chain; the host strips the
  flag and resizes from `GetBuffer(0)` after the game releases its views.
* Resize: 1264x681 -> 1920x1017 -> 1264x681 was verified without a lost device.
* Present/Present1 are forwarded once per frame, which removes the double-present
  flicker.
* Skin import: `FileOpenPicker.FileTypeFilter` is stored, `RemoveAtEnd`/`Clear`
  work, and the Win32 dialog is built with the game's filter and default
  extension (`PNG image (*.png)` was verified with a real PNG).
* FMOD delay imports: the profile selects the 1.16.40.02 descriptor
  (`0x024b01cc`), all 48 names resolve against the shipped `fmod_X86.dll`, and
  the common calls go through the diagnostic bridge.
* Survival input: the shared host answers the game's `GetAsyncKeyState` polling
  with the real mouse-button state for VK 1-6, so holding the attack button
  keeps mining/breaking blocks instead of looking like a one-frame tap.
* Exit: the "Did you like Minecraft?" Store rating prompt is suppressed
  (`jne` -> `jmp` at file offset `0x166d06`, option `0x174` /
  `RatingPromptController`). A normal exit no longer calls the unavailable UWP
  Store projection.
* JavaScript engine: the exe keeps the Windows inbox `chakra.dll` import. The
  diagnostic `--chakra-core` mode redirects it to `ChakraCore.dll` but breaks
  experimental-gameplay scripts; it is not used for the normal package.
* User data: `%APPDATA%\MinecraftPE\games\com.mojang`; `WIN32CRAFT_DEBUG=1`
  writes `win32craft.log` next to the exe.

## What does not work yet

* Audio output. `System_Create`, `getVersion` and `setOutput(WASAPI)` succeed,
  but `init` returns 28 (`FMOD_ERR_OUTPUT_INIT`); DirectSound and WinMM fail with
  54 (`FMOD_ERR_OUTPUT_CREATEBUFFER`). The shipped DLL is the UWP FMOD 1.09.04
  build, so this is likely a package-identity/audio-graph limitation rather than
  a missing import. The game falls back to the host's silent AudioGraph path.
* The 1.16.40.02 host profile is x86-only. The ARM32 1.16.40.02 work is separate
  (`arm32/v1_16_40/`).

## First things to look at

1. `win32_host_116.inl`: `game_profile_116_04002` and `bootstrap_116`. The profile
   is selected from `SizeOfImage == 0x02DAF000` after patching.
2. `tools/1.16.40.02-patcher.py`: the exe patch sites and the `--package` mode.
3. `win32_host.c`: `resolve_fmod_delay_imports_116` and the `bridge_fmod_*`
   functions for the remaining sound work.
4. Package: `dist-1.16.40.02\Minecraft_1.16.40.02_x86_Win32Craft` (ignored by
   git) or rebuild with
   `python tools\1.16.40.02-patcher.py SOURCE OUTPUT --package --host PATH`.
   The ready folder contains `Win32Craft.1.16.40.02.exe`, `win32craft.dll`,
   `fmod.dll`, `.Win32Craft` and `PATCH_NOTES.md`; the stock
   `Minecraft.Windows.exe` is not kept.
