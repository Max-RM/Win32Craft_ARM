# Win32Craft — hand-off

**What this is.** Win32Craft runs UWP and Windows-Phone **ARM32** apps and games as **native Win32** on ARM Windows —
no Store, no AppContainer, no original shell. For each app we replace its shell/launcher with a small native host and a
handful of shims, so the app's own ARM32 code (its engine, its D3D, its audio) runs directly on the device. It targets
old ARM tablets that are stuck at Direct3D **feature level 9_1**: a Surface RT (Windows RT 8.1, Tegra 3) and a Surface 2
(Windows 10 build 15035 ARM32, Tegra 4), and it also runs on your **POCO X3 Pro (Windows 11 ARM64)** via WOW.

This bundle is three games plus the full source to continue them.

## What's in this bundle

| | |
|---|---|
| `HANDOFF_FOR_FRIEND.md` | this document — status, how to run, how it works, how to continue, for every game |
| `runnable/` | the three built packages you can copy to a device and run (they contain extracted game DLLs/assets) |
| `source.zip` | the Win32Craft source + tooling to rebuild/continue (extracts to `Win32Craft_src/`; **no game folders / .appx / .xap** — bring your own game copies) |

Inside `runnable/`: `REDCON_1.3.0.0_ARM32_Win32Craft.zip`, `BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft.zip`,
`GeometryDash_M1_host.zip`, and `gd_frame.png` (proof: Geometry Dash's splash rendered through our host at FL 9_1).

## Status at a glance

| Game | State | Renders at FL 9_1 | Host type | What's left |
|---|---|---|---|---|
| **Redcon** 1.3.0.0 (WP8.1 XAML) | ✅ **runs** on Surface 2 — ~30 fps, sound + music, touch, Back, exit | native (the game asks 11_1..9_1 itself; shaders already level-9) | x104 "fake-object" host (shared `win32craft.dll`), `redcon` profile | minor polish (XAML dialog widgets, store/IAP stubs) |
| **Bad Piggies Rebooted** 1.0.0.0 (Unity 2018.4 IL2CPP UWP) | ✅ **renders + runs** on Surface 2's Tegra 4 — real GPU, menus + gameplay | yes, via the `l9shim` D3D11 shim (FL10→9_1) | `l9d3d11.dll` shim injected into UnityPlayer | optional: patch the mod's own C# bug; Tegra 3 / RT 8.1 test |
| **Geometry Dash** 2.0.1.0 (WP8.0 Silverlight + cocos2d/ANGLE) | 🟡 **M1 done** — the splash **renders** at FL 9_1 on Surface 2 (headless probe) | yes (confirmed: 8 frames drawn) | new standalone host `gd_host.exe` | **M2–M6**: window + present-with-rotate + loop (→ visible/upright/animated), input, audio, package |

The hard, uncertain parts are behind us. Redcon and Bad Piggies are finished ports. Geometry Dash's whole pipeline is
proven on-device (the host drives the game's native component, ANGLE runs on our device, cocos2d loads assets and draws
real frames); what remains for GD is the ordinary windowing/input/audio shell around a working renderer.

## Devices & how you fit in

- **Surface RT** — Windows RT 8.1, Tegra 3, FL 9_1. The oldest target; some things (WARP vs HW) still unmeasured there.
- **Surface 2** — Windows 10 build 15035 ARM32, Tegra 4, FL 9_1. The main test device for all three games.
- **POCO X3 Pro** — Windows 11 ARM64 (yours). Runs the ARM32 binaries under WOW; good for continuing GD (M2+) and for a
  second data point. Everything in `runnable/` should run there the same way it does on the Surface 2.

**Golden rules** (see *Methods & toolchain* below): build the ARM32 binaries on an x64 PC with clang, and **never run the
ARM binaries on the x64 PC** — only on an ARM device. The public source must never contain the game folders or their
`.appx`/`.xap`. Don't hard-wire feature level 9_1 as a permanent default — let a capable GPU pick its own.

## How to read the rest of this document

1. **Methods & toolchain** — the two host architectures, the reusable techniques, the exact build toolchain, the
   diagnostic tools (`loadcheck`, `m0probe`), the repo map. Read this first if you're going to build.
2. **Redcon** — a finished WP8.1 XAML port on the shared x104 host.
3. **Bad Piggies Rebooted** — a finished Unity/FL9_1 port via the D3D11 shim.
4. **Geometry Dash** — the in-progress port: M1 is done (renders), with a detailed M2–M6 continuation guide. This is where
   the open work is.

There is also a separate `games_recon/HANDOFF_FOR_BADPIGGIES_DEV.md` — a clean bug report for the Bad Piggies *mod author*
about three pre-existing C# bugs we found (not our port's fault). Pass it along if you're in touch with them.

---
