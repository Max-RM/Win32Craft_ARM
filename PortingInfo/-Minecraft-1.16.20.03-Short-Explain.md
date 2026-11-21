# Minecraft 1.16.20.03 (Bedrock, x86 UWP)

* Game: Minecraft Bedrock Edition by Mojang / Microsoft
* Version: 1.16.20.03, x86 UWP APPX (package
  `Microsoft.MinecraftUWP_1.16.2003.0_x86__8wekyb3d8bbwe`)
* Engine: native C++ Bedrock engine with ChakraCore and cohtml UI; no .NET game logic
* Host: `win32craft.dll` (Win32Craft x86 host), profile selected by
  `SizeOfImage == 0x02DB1000`; shares `win32_host_116.inl` with 1.16.40.02
* Patcher: `tools/1.16.20.03-patcher.py`
* Package exe: `Win32Craft.1.16.20.03.exe` (the stock `Minecraft.Windows.exe`
  is removed; `--exe-name` overrides the name)
* Package marker: `.Win32Craft` containing `Minecraft Bedrock Edition`;
  `*.p7x` and `*.i64` files are removed
* Status: runs as an unpackaged Win32 x86 process. Startup, main menu, window
  title and the skin import path are verified. The shared host includes the
  `FileOpenPicker.FileTypeFilter` skin-picker fix.

## What works

* Full startup with package/application-data redirection, CoreApplication view,
  CoreWindow and the `Win32Craft 1.16.20.03` title.
* Skin import: Profile -> Create Character -> Classic Skins -> Choose New Skin
  opens the Win32 file dialog with the game's filter and default extension
  (`FileOpenPicker redirected to Win32 open dialog` in the host log).
* Package mode: `.appx` or extracted folder input, `*_APP` CRT import
  rewriting in the six game DLLs, `fmod.dll` creation, host copy, standard exe
  rename, `.Win32Craft` marker and `PATCH_NOTES.md`.
* `WIN32CRAFT_DEBUG=1` writes `win32craft.log` next to the exe.

## What does not work yet

* Audio output is not fixed. The shared host still has the UWP FMOD 1.09.04
  output-init limitation described in the 1.16.40.02 document; the skin-picker
  test did not re-test sound.
* This port is x86-only. The ARM32 1.16.40.02 work is separate
  (`arm32/v1_16_40/`).

## First things to look at

1. `tools/1.16.20.03-patcher.py`: the exe patch sites, the accepted input
   hashes and the `--package` mode.
2. `win32_host_116.inl`: the shared 1.16 profiles; the 1.16.20.03 profile is
   selected by `SizeOfImage == 0x02DB1000`.
3. `win32_host.c`: the shared `FileOpenPicker` redirect and the FMOD bridge.
4. Package: `dist-1.16.20.03\Minecraft_1.16.20.03_x86_Win32Craft` (ignored by
   git) or rebuild with
   `python tools\1.16.20.03-patcher.py SOURCE OUTPUT --package --host PATH`.
   The ready folder contains `Win32Craft.1.16.20.03.exe`, `win32craft.dll`,
   `fmod.dll`, `.Win32Craft` and `PATCH_NOTES.md`.
