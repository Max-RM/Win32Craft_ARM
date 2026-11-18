# Hill Climb Racing 1.27.0.32 (Windows Phone 8.1 XAML, ARM32)

* Game: Hill Climb Racing by Fingersoft
* Version: 1.27.0.32 (Windows Phone 8.1 Silverlight-era XAML app shipped as APPX, ARM32)
* Engine: native C++ (Cocos2d-x 2.2.5) with ANGLE for rendering; no .NET game logic
* Host: `win32craft.dll` (Win32Craft_ARM ARM32 host), profile `x104_profile_hcr`
* Status: runs in Wine 11 ARM32 on Android (Debian 13 armhf chroot, VNC display `:1`).
  Surface RT (Windows RT 8.1, Tegra 3, D3D11 feature level 9_1 only) reaches the menu with the
  host's 9_1 shim, but the GPU driver drops the device after roughly 25 frames.
  Windows 11 ARM64 is not verified yet.

## What works

* Full game loop: splash, main menu, vehicle select, stage select, upgrade screen, races,
  pause, results, coins.
* Rendering: ANGLE D3D11 at feature level 10_0 through Wine's d3d11. Launch the exe with
  `--feature-level=10_0` (the host default cap is 9_1, and the game's shaders do not survive it).
  On a 9_1-only device the host retries the hardware at 9_1 and reports 9_3 to the game
  (`--fl-lie=1`, on by default in the HCR profile), see "What does not work".
* Input: mouse/touch through `PointerPoint`; arrow keys (Right/Up = gas, Left/Down = brake) on
  the keyboard, like the Windows 10 build.
* Window: normal Win32 window, close button and Alt+F4 both quit cleanly.
* Saves: coins, upgrades, unlocked vehicles, stage selection, highscores, `adfree` and
  achievements persist across restarts in `%APPDATA%\Win32Craft\HillClimbRacing\`.
* Language: `win32craft.ini` next to the exe, `[platform]` / `language` (`en`, `de`, `fr`, `it`,
  `es`, `ru`, `pt`, `ja`, `ko`, `zh`, `zh-TW`); missing file or key means English. The game's
  own language picker keeps working and overrides the ini once the player changes it.
* Coin packs: a packaged exe patch (AutoModificator switch) makes the GET COINS page grant the
  tapped pack without contacting the Store.

## What does not work

* Surface RT / anything with a feature level 9_1 GPU: the host can create the hardware device at
  9_1, report 9_3 back to the game and ask `D3DCompile` for the `_level_9_1` profile, and the game
  then renders frames - but the Tegra 3 driver drops the device (`DXGI_ERROR_DRIVER_INTERNAL_ERROR`,
  `0x887A0020`) after about 25 presents. WARP at 9_3 runs at 1-3 fps and does not fail, so the
  host side is complete and the remaining problem is inside the vendor UMD. Run
  `Run-HCR-HW-trace.bat` (`--trace-game-state`) to record the draws of the frame that kills it.
  A 9_1 device that tops out at 9_3 (Snapdragon / Adreno under Wine) is unaffected: it takes the
  normal 10_0 path.
* Real Store purchases and the real listing query (`CurrentApp.LoadListingInformationAsync`
  is not implemented by the host and returns `E_NOTIMPL`; the game catches the exception).
  This is exactly what the coin-pack patch replaces.
* Ads are disabled (the ad SDK surfaces are stubbed out), so no ad rewards exist.
* Xbox/achievement sync, cloud saves, tile/background-task notifications: stubbed, no effect.

## Look at this first

1. `--feature-level=10_0` - without it the game renders nothing usable.
2. The `UserDefaults` child container of `LocalSettings` - all progress lives there, not in
   `fsusda.data` (see the full document; the game treats a missing key as fatal).
3. The full document `-HillClimbRacing-Full-Explain.md` for the WinRT holes, profile flags and
   the reverse-engineered addresses used by the coin patch.
