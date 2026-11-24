# PortingInfo - reference notes for porting UWP / Windows Phone ARM32 apps to Win32Craft

This folder exists so that **another AI agent (or a human) can continue a port without redoing the reverse
engineering**. Every ported app gets two documents here:

```
PortingInfo/
  README.md                        this file: what exists, for which game and which version
  REMOTE-AI-VIA-PS.md              how an AI drives a Surface RT over PowerShell remoting (test device access)
  -HillClimbRacing-Short-Explain.md
  -HillClimbRacing-Full-Explain.md
  ...
```

The file names start with a dash so the game name lines up in a plain directory listing (and so a
`grep -l '^# '` over the folder returns only the real documents).

## Naming

`<Game>` is the **full game name without spaces** - `HillClimbRacing`, `MinecraftPE`, `Redcon`, `GeometryDash`.
Do not invent abbreviations. The same rule applies to the per-game data folder under `%APPDATA%\Win32Craft\`
(the host profile field `data_subdir`) and to the host profile `key`: `Win32Craft\HillClimbRacing`, not `Win32Craft\HCR`.

The **version belongs in the document header, not in the file name**, because one game can have several ports that
share almost nothing (Minecraft 1.2.8 is a different port from Minecraft PE 0.10.4). If a second version of a game
is ported, add `-<Game>-<Version>-Short-Explain.md` / `-<Game>-<Version>-Full-Explain.md` and list it in the table
below. While only one version exists, the unsuffixed names are used.

## What each document contains

Short (`-<Game>-Short-Explain.md`): full name, version, package/engine, status, what works, what does not, the
one or two things to look at first. Meant to be read in 30 seconds.

Full (`-<Game>-Full-Explain.md`): the complete porting record - the WinRT holes the app actually hits, the fakes
the host implements for it, the profile flags and ini keys, the save/state layout, the reverse-engineered addresses
and the traps that cost time. Meant to let an agent act without guessing.

## Index

| Game | Version | Platform | Status | Documents |
|---|---|---|---|---|
| Hill Climb Racing (Fingersoft) | 1.27.0.32 | Windows Phone 8.1 XAML, ARM32 | runs in Wine 11 ARM32 on Android (VNC); Surface RT 8.1 creates a 9_1 hardware device but the driver drops it after ~25 frames; Windows 11 ARM64 pending | [`-HillClimbRacing-Short-Explain.md`](-HillClimbRacing-Short-Explain.md), [`-HillClimbRacing-Full-Explain.md`](-HillClimbRacing-Full-Explain.md) |
| Minecraft Bedrock (Mojang / Microsoft) | 1.14.60.5 | x86 UWP APPX, unpackaged Win32 | runs: startup, menu, world load, mouse capture and survival block breaking; package uses `Win32Craft.1.14.60.5.exe`; world entry uses the `0xEEFEF5` Chakra-predicate skip; audio output not initialized by the UWP FMOD 1.09.04 build | [`-Minecraft-1.14.60.5-Short-Explain.md`](-Minecraft-1.14.60.5-Short-Explain.md), [`-Minecraft-1.14.60.5-Full-Explain.md`](-Minecraft-1.14.60.5-Full-Explain.md) |
| Minecraft Bedrock (Mojang / Microsoft) | 1.14.30.2 | x86 UWP APPX, unpackaged Win32 | runs: startup, menu, world load, mouse capture and exit -> world re-entry (world load, `Save & Quit`, second load, no crash); package uses `Win32Craft.1.14.30.2.exe`; world entry uses the `0xE956A5` Chakra-predicate skip; audio not re-measured, expected to hit the same UWP FMOD limitation | [`-Minecraft-1.14.30.2-Short-Explain.md`](-Minecraft-1.14.30.2-Short-Explain.md), [`-Minecraft-1.14.30.2-Full-Explain.md`](-Minecraft-1.14.30.2-Full-Explain.md) |
| Minecraft Bedrock (Mojang / Microsoft) | 1.16.20.03 | x86 UWP APPX, unpackaged Win32 | runs: startup, menu, `Win32Craft 1.16.20.03` title and skin import (shared skin-picker fix); package uses `Win32Craft.1.16.20.03.exe`; audio output not fixed | [`-Minecraft-1.16.20.03-Short-Explain.md`](-Minecraft-1.16.20.03-Short-Explain.md) |
| Minecraft Bedrock (Mojang / Microsoft) | 1.16.40.02 | x86 UWP APPX, unpackaged Win32 | runs: menu, world load, skin import, resize and `Win32Craft 1.16.40.02` title; package uses `Win32Craft.1.16.40.02.exe`; audio output not initialized by the UWP FMOD 1.09.04 build on the test machine | [`-Minecraft-1.16.40.02-Short-Explain.md`](-Minecraft-1.16.40.02-Short-Explain.md), [`-Minecraft-1.16.40.02-Full-Explain.md`](-Minecraft-1.16.40.02-Full-Explain.md) |

| Reference | What it is |
|---|---|
| [`REMOTE-AI-VIA-PS.md`](REMOTE-AI-VIA-PS.md) | driving a Surface RT (Windows RT 8.1) from an AI over WinRM/PowerShell Remoting: push files, run tools, pull logs |

`games_recon/` still holds the older, much larger per-game research dumps (Bad Piggies, Geometry Dash, REDCON).
Those are raw notes; this folder is the curated, structured version. A `-<Game>-Full-Explain.md` may link into
`games_recon/` for the long tables (shader lists, import tables, ...) instead of copying them.

## Adding a new port to this folder

1. Copy `-HillClimbRacing-Short-Explain.md` as a skeleton and replace the facts; keep the headings.
2. In the full document, always fill in: the WinRT holes that had to be faked, the host profile (`X104Profile`)
   values, the ini keys, the data-root layout, and the "traps" section - that is what saves the next agent days.
3. Add a row to the index table above.
4. Keep the documents in English, plain Markdown, no binaries, no real IP addresses or personal data.
