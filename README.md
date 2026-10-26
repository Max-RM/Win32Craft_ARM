# Win32Craft

Win32Craft runs UWP and Windows-Phone **ARM32** games and apps as native Win32 programs on ARM Windows — no Store, no
AppContainer, no original shell. A small native host plus a handful of shims replace the app's launcher, so the app's own
ARM32 code (engine, D3D, audio) runs directly on the device.

Targets: **Windows RT 8.1** (Surface RT, Tegra 3), **Windows 10 ARM32** build 15035 (Surface 2, Tegra 4) and
**Windows 10/11 ARM64** via WOW (e.g. POCO X3 Pro). Devices of this class top out at Direct3D feature level **9_1**, so
the host and the per-game shims negotiate levels 11_1 → 9_1 instead of assuming a modern GPU.

## Supported apps

| App | Version | State | Route |
|---|---|---|---|
| Minecraft: Windows 10 Edition | 1.2.8 (x86 UWP, Experimental) | runs | `tools/1.2.8-patcher.py` + host from `build_windows_mingw.bat` |
| Minecraft: Windows 10 Edition | 1.2.8 (ARM32) | runs | `tools/1.2.8-arm32-patcher.py` + `win32craft.dll` |
| Minecraft: Pocket Edition | 0.10.4.40 | runs | `tools/arm32-patcher.py` profile `v0_10_4` |
| REDCON 1 (Hexage) | 1.3.0.0 | runs — ~30 fps, sound + music | `tools/arm32-patcher.py` profile `redcon`, shared `win32craft.dll` |
| Geometry Dash | 2.0.1.0 | playable — menu, levels, music, touch | standalone `gd_host.exe` + `PhoneAppModelHost.dll` / `PhoneAudioSes.dll` |
| Bad Piggies Rebooted | 1.0.0.0 | renders and runs at FL 9_1; not complete | `l9shim.dll` D3D11 feature-level shim (Unity IL2CPP UWP) |

Minecraft 1.2.8 runs on Windows RT 8.1 / Windows 10 ARM32:

<img width="1920" height="1080" alt="Minecraft 1.2.8 running on Windows RT 8.1" src="https://github.com/user-attachments/assets/9eba6997-24c7-47db-9494-64d9cbbe2ed5" />

Status details, device logs, known issues and the continuation plan: [`HANDOFF_FOR_FRIEND.md`](HANDOFF_FOR_FRIEND.md).
Per-game recon and hand-off notes: [`games_recon/`](games_recon/).

## Build

### x86 host (Minecraft 1.2.8 UWP path)

Platform | Script
---|---
Windows | `build_windows_mingw.bat`
Linux (cross-compilation to run in Wine) | `./build_mingw.sh`

### ARM32 host — `win32craft.dll`

Thumb-2 / hard-float build with the Windows RT LLVM toolchain plus the MSVC ARM libraries and the Windows SDK. Run from
`cmd.exe` in the repository root; every path is overridable through environment variables:

```
set LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin
set VCLIB=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\arm
set SDKROOT=D:\Windows Kits\10
set SDKVER=10.0.19041.0
build_arm32.bat
```

Output: `dist-arm32\win32craft.dll` (ARMNT). The same DLL serves Minecraft 0.10.4 and the REDCON profile; the x86 build
output is unchanged by the ARM32 work.

### Per-game ARM32 builds

Target | Script
---|---
Geometry Dash host | `tools\gd\build_gd_host_arm32.bat`
Geometry Dash shims | `tools\gd\build_gd_shims_arm32.bat`
Bad Piggies `l9shim.dll` | `tools\badpiggies\build_l9shim_arm32.bat`
Diagnostics (`loadcheck`, `kbdprobe`, `m0probe`) | `build_loadcheck_arm32.bat`, `build_kbdprobe_arm32.bat`, `tools\m0probe\build_m0probe_arm32.bat`

A full per-game build log lives in [`tools/gd/GD_BUILD.md`](tools/gd/GD_BUILD.md) (Geometry Dash) and
[`HANDOFF_BADPIGGIES_MODS.md`](games_recon/HANDOFF_BADPIGGIES_MODS.md) (Bad Piggies).

## Patch example

```
cd tools
python 1.2.8-patcher.py Minecraft.Windows.exe
python 1.2.8-arm32-patcher.py Minecraft.Windows.exe
python arm32-patcher.py detect "D:\games\MCPE 0.10.4.40"
python arm32-patcher.py patch --profile auto "D:\games\REDCON" out\redcon
```

`arm32-patcher.py profiles` lists the available profiles (`--dump NAME` prints one as JSON, the template for a new
version).

## Requirements

Requirement | Link
---|---
The original data folder and `.exe` from the game you want to run (MCPE / REDCON / GD / BPRE) | https://www.mcappx.com/bedrock
Win32 FMOD archive (for the Minecraft 1.2.8 path) | https://www.mediafire.com/file/36ylad9xcz8s7xg/fmod-archive.7z/file

Game files are **not** included in this repository; you must supply your own copies. The ARM32 host additionally needs
the desktop VC++ ARM redistributables next to the exe (`vccorlib140.dll`, `msvcp140.dll`, `concrt140.dll`,
`vcruntime140.dll`); see [`ARM32.md`](ARM32.md) for the per-device details and the `loadcheck.exe` diagnostics.

## FMOD

FMOD | Versions
---|---

## Documentation

- [`HANDOFF_FOR_FRIEND.md`](HANDOFF_FOR_FRIEND.md) — status, how to run and continue every game
- [`ARM32.md`](ARM32.md) — Minecraft 1.2.8 ARM32 port: requirements, patching, device setup, diagnostics
- [`HANDOFF_ARM32.md`](HANDOFF_ARM32.md) — ARM32 host internals and per-unit notes
- [`games_recon/GAMES_PLAN.md`](games_recon/GAMES_PLAN.md) — per-game status and continuation plan

## Donate to the original Author (Deltaxur)

BTC: `bc1qkkckr4d8f9l4me5efnd3e5sf4q6akfhe26lpjy`
