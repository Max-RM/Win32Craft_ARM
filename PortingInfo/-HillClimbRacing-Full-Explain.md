# Hill Climb Racing 1.27.0.32 - full porting record

| | |
|---|---|
| Game | Hill Climb Racing (Fingersoft) |
| Version | 1.27.0.32, package for Windows Phone 8.1 (XAML app, ARM32), APPX |
| Engine | native C++ Cocos2d-x 2.2.5 + ANGLE (no .NET game code, one managed helper DLL) |
| Host | `win32craft.dll` (Win32Craft_ARM ARM32 host), profile `x104_profile_hcr` |
| Verified on | Wine 11 ARM32 on Android (Debian 13 armhf chroot), VNC display `:1` |
| Not verified yet | Windows 11 ARM64, Surface RT / Windows RT 8.1 |

Everything below was established on the device with the host log (`win32craft.log` next to
the exe), IDA 9.2 headless on the game exe, and the game's own WINMD/APPX metadata.

## 1. Where the pieces live

| Piece | Path |
|---|---|
| Host sources (one translation unit) | `work/hcrhost/win32_host.c` + `win32_host_arm32_0104*.inl` |
| Host build script | `work/hcrhost/build.ps1` |
| Host output | `work/hcrhost/win32craft.dll` (552960 bytes in the verified build) |
| Game exe in the repo | not packaged yet (no `tools/make_hcr_package.py`; see the open items) |
| Game folder on the test device | `/home/MaxRM/WinTest/HCR_1.27.0.32_ARM32_Win32Craft/` inside the chroot |
| Host log | `win32craft.log` in the game folder |

The host is one translation unit: `win32_host.c` includes `win32_host_arm32_0104.inl`, which
includes the units in a fixed order (core, async, app, panel, platform, input, selftest, boot).
Each unit owns its file; a new game only needs an `X104Profile` entry plus whatever fakes its
WinRT surface requires.

## 2. Package layout (what the port needs next to the exe)

* `Win32Craft.HCR-arm32.exe` - the game exe with the host bootstrap patch (section `.d3imp`
  added, `SizeOfImage` 0x1C9000; stock is 0x1C8000). The patch adds a rewritten import table
  (APIs that Windows RT 8.1 / Wine do not export get mapped to their base DLLs) and reroutes
  one CRT init call at RVA `0x11A28E` through a thunk at RVA `0x11AF50` into
  `win32craft.dll!Win32Bootstrap`.
* `win32craft.dll` - the host.
* `libGLESv2.dll`, `libEGL.dll` - ANGLE (D3D11 backend). The host installs its
  `D3D11CreateDevice` hook in the `libGLESv2.dll` import table (IAT), so the feature level and
  the device flags can be adjusted before the game sees them.
* `libcocos2d.dll` - the game's engine DLL, as shipped.
* VC120 Phone runtime: taken from the real `Microsoft.VCLibs.120.00.Phone` 12.0.30113.0 ARM
  APPX with the `APPCONTAINER` flag removed, so the DLLs can load in a normal Win32 process.
  VC11 (`msvcr110.dll` and friends) is copied from the Geometry Dash package, because parts of
  the binary chain still import it.
* `win32craft.ini` - optional, see section 9.
* The APPX-only files (AppxManifest, XBF, AdDuplex / AdMediator assemblies, winmd files) stay in
  the folder but are not used at run time.

Launch overrides used on the device (they keep the phone-named `*_app` runtime DLLs from being
preferred over the desktop-named ones in the folder):

```
WINEDLLOVERRIDES="mscoree,mshtml=;msvcr120_app=n;msvcp120_app=n;vccorlib120_app=n"
```

## 3. Host profile

`x104_profile_hcr` in `win32_host_arm32_0104_core.inl`:

| Field | Value | Meaning |
|---|---|---|
| `key` | `hillclimbracing` | short id in log lines (also why the data folder is not "HCR") |
| `title` | `Hill Climb Racing 1.27.0.32` | banner / failure box text |
| `box_caption` | `Hill Climb Racing - Win32Craft` | host message box caption |
| `window_class` / `window_title` | `HCRWin32` / `Win32Craft - Hill Climb Racing` | |
| `size_of_image_patched` | `0x001C9000` | identification key (must match the patched exe) |
| `size_of_image_stock` | `0x001C8000` | only used by the "this exe was not patched" log line |
| `entry_rva` | `0x0011A179` | expected `AddressOfEntryPoint` |
| `sections_patched` | `7` | expected section count (the patched exe has `.d3imp`) |
| `callsite_rva` / `callsite_bytes` | `0x0011A28E` / `00 f0 5f fe` | the CRT call the bootstrap patch rewrites |
| `trampoline_rva` | `0x0011AF50` | where the rewritten call lands (PIC thunk in `.text` padding) |
| `winmain_wrapper_rva` | `0x0011A664` | the game's WinMain thunk (same 12-byte shape in every supported XAML game) |
| `features` | `X104_FEAT_XAML_PAGE`, `X104_FEAT_SM4_SHADERS`, `X104_FEAT_KEYBOARD_PAD` | see section 4 |
| `data_subdir` | `Win32Craft\HillClimbRacing` | `%APPDATA%\Win32Craft\HillClimbRacing` |
| `data_subdir_text` | `%APPDATA%\Win32Craft\HillClimbRacing` | the same string for log lines |

Identification works on `SizeOfImage`, so the host refuses to run with a mismatched exe instead
of failing somewhere deep in the WinRT fakes.

## 4. Feature flags

`X104_FEAT_XAML_PAGE` - the game is the stock "DirectX App (XAML)" template: the host has to
provide `MainPage` through `IPageFactory`, an `AppMain` object plus its ready flag, the
thread-pool render loop, and the first `SizeChanged` (the game's `SwapChainPanel` never gets a
size from a real XAML layout engine, so the host synthesises it). Without this the game loads
but never starts rendering.

`X104_FEAT_SM4_SHADERS` - HCR compiles its shaders at run time through ANGLE and asks for a
shader profile; the host answers with the SM4 profile (`dx_4_0`-class) instead of letting a
default SM2 answer through. This is why `--feature-level=10_0` is required (section 5).

`X104_FEAT_KEYBOARD_PAD` - the game was built for a phone and reads only touch points, so the
host converts arrow keys into the game's gas/brake input (section 10).

## 5. Rendering

* The game creates its D3D11 device indirectly: ANGLE (`libGLESv2.dll`) calls
  `D3D11CreateDevice`. The host hooks that import in ANGLE's IAT and adjusts the requested
  feature level / flags.
* The host's default cap on ARM32 is feature level `0x9100` (9_1). HCR's shaders do not survive
  that: the scene draws nothing. Launching the exe with `--feature-level=10_0` raises the cap
  and everything renders.
* **Feature level 9_1 hardware (Surface RT, Tegra 3).** HCR hands `D3D11CreateDevice` a single-level
  list `[9300]`, so a 9_1-only UMD fails the call with `DXGI_ERROR_UNSUPPORTED` (0x887A0004) and the
  host used to fall back to WARP. The HCR profile now sets `X104_FEAT_FL9_LIE` (0x20), which makes
  the panel hook (section "4b" of `win32_host_arm32_0104_panel.inl`):
  1. retry the *hardware* device with `[0x9100]` when the first request fails, instead of WARP;
  2. answer `ID3D11Device::GetFeatureLevel` with `0x9300` on a wrapped vtable (cell 37) while the
     real device stays at 9_1;
  3. patch the single `_level_9_3` literal inside `libGLESv2.dll` to `_level_9_1`, so ANGLE asks
     `D3DCompile` for the 9_1 shader model that matches the real device.
  Options: `--fl-lie=0|1` (default from the profile), `--angle-profile=9_3` (keep ANGLE's own
  suffix; A/B test only), `--warp` (force the software rasteriser), `--feature-level=<level>`
  (cap). State on RT: the device is created at 9_1 and the game renders at 15-30 fps, but the
  driver returns `Present -> DXGI_ERROR_DEVICE_REMOVED` with
  `GetDeviceRemovedReason = DXGI_ERROR_DRIVER_INTERNAL_ERROR` (0x887A0020) after ~25 presents, with
  both the 9_1 and the 9_3 ANGLE profile. The game then re-creates the device and the cycle
  repeats. WARP at 9_3 renders the same content and never dies (1-3 fps), so the host/ANGLE side is
  not the problem; suspect the vendor UMD choking on a draw or resource of that frame.
  The full run is `arm32/device_logs/hcr_rt_9_1_shim_device_lost_2026-11-18.log`.
* Window client area under openbox is 888x444 at `+4+30` on a 896x504 display, so
  `screen = client + (4, 30)`; this matters for the xdotool coordinates in section 14.
* Presentation is a normal `IDXGISwapChain` present inside `WM_PAINT`/the render loop owned by
  the game; Wine's d3d11 does the rest. No custom swapchain code is involved.

## 6. The WinRT surface the game actually touches

The host implements the Windows Phone 8.1 XAML surface broadly and gains per-game behaviour
from the profile. HCR specifically exercises:

| Surface | Host behaviour |
|---|---|
| `Windows.ApplicationModel.Core.CoreApplication`, `Application::Current` | aggregated game `App2` object; lifecycle events (`Launching`, `Activated`, `Suspending`, `Resuming`) are delivered from the host's window messages |
| `Windows.UI.Xaml` `Application`, `Page`, `Frame`, `SwapChainPanel`, `Grid`, `Canvas` | minimal XAML object model: enough for the template page, `Loaded`, `SizeChanged`, `ActualWidth/Height`, `Focus`, `Visibility`, `Opacity`, `RenderTransform` |
| `Windows.UI.Core.CoreWindow` | pointer events, key events, `PointerCursor`, `Activated`; the game only reads `PointerPoint.PointerId` and `PointerPoint.Position` |
| `Windows.Graphics.Display.DisplayInformation` | DPI / orientation answers matching the 888x444 client area |
| `Windows.Storage.ApplicationData` | real gap in Wine (`dlls/windows.storage.applicationdata` is `E_NOTIMPL`), fully faked by the host, including `LocalSettings` and child containers (section 7) |
| `Windows.Storage` files/folders | `StorageFile`, `StorageFolder`, `GetFileAsync`, `CreateFileAsync`, `OpenAsync`, `OpenStreamForWriteAsync`, `GetBasicPropertiesAsync`; the data root is `%APPDATA%\Win32Craft\HillClimbRacing` |
| Streams | `IRandomAccessStream` / `IRandomAccessStreamWithContentType` (ABI-flattened), `DataReader`, `DataWriter`, host `IBuffer` + `IBufferByteAccess` |
| `Windows.Storage.Compression` | `Compressor` / `Decompressor` are pass-through (no MS-XCA); the game's own `CompressorAlgorithm = 5` (XPRESS Huffman) is accepted but the bytes are stored raw |
| `Windows.ApplicationModel.Store.CurrentApp`, `CurrentAppSimulator` | license object (no trials), `LoadListingInformationAsync` returns `E_NOTIMPL`, `RequestProductPurchaseAsync` returns `E_NOTIMPL`; `ICurrentAppWithConsumables::GetUnfulfilledConsumablesAsync` returns an `IAsyncOperation` that is `Started` for ever (the game attaches a continuation and never blocks) |
| `Windows.ApplicationModel.Background` | absent in Wine; the host fakes the namespace and the game's `HillClimbRacing.BackgroundTasks.UpdateTileTask` statics (the real class is .NET, loaded from `CLRHost.dll` on the phone) |
| `Fingersoft.AdSdk.AdManager`, `.AdManagerInstance`, `.Providers` | statics object + instance object, implemented from the game's own `Fingersoft.AdSdk.winmd`; all ad requests are swallowed, so no ads and no ad rewards |
| `Windows.System.Launcher` | left to the real runtime (Wine), not faked |
| `Windows.Foundation.PropertyValue` | left to the real runtime (Wine) - the host only uses its `CreateString` to seed typed settings |

Store behaviour is deliberate: pretending the Store works would put the game into a purchase
flow that can never complete. The coin pack patch (section 11) replaces the purchase path
entirely.

## 7. LocalSettings: containers, seeds, persistence

This was the single most expensive part of the port, because HCR's progress lives here and
nowhere else.

* The game uses cocos2d's `CCUserDefault`, which is implemented on top of
  `ApplicationData.Current.LocalSettings.Values` - a `PropertySet` of boxed values.
* HCR writes almost everything into a **child container named `UserDefaults`**
  (`LocalSettings.CreateContainer("UserDefaults", ...)`), not into the root map.
  Coins, upgrade levels, `car_selection`, `stage_selection`, `adfree`, `adFreeIAPPurchased`,
  achievements and highscores are all in that container.
* Every start the game also does `DeleteContainer("TileNotifications")` followed by
  `CreateContainer("TileNotifications")`, and writes into it. That container must stay isolated
  from `UserDefaults`; an early version shared one map and the tile task wiped the progress.
* Containers are real host objects with their own `Values` map, and each map persists to its own
  file (section 8).

Typed seeds. The game reads most settings with a bare `Lookup` and then casts; a missing key is
a fatal `Platform::OutOfBoundsException`, and a wrong type is a `TYPE_E_TYPEMISMATCH` raised
inside `vccorlib` (which kills the UI loop). So the host seeds every key the game touches, with
the right type:

| Type | Keys |
|---|---|
| Int32 (`X104_SEED_INT32`) | `stage`, `coins`, `highscore`, `level_engine`, `level_suspension`, `level_awd`, `level_traction`, `level_body`, `stage_selection`, `car_selection`, `display_mode`, `facebook_like_rewarded`, `video_ads_enabled`, `everyplay_enabled`, `windows_store_review_counter` |
| Boolean (`X104_SEED_BOOL`) | `adfree`, `adFreeIAPPurchased`, `auto_sign_in`, `render_dirt`, `scrolling_background`, `halloween_content`, `music`, `sound`, `moga_pro_guide`, `moga_pocket_guide`, `windows_store_review_should_ask` |
| String (`X104_SEED_STRING`) | `language` (`en` by default, or the value from `win32craft.ini`) |

Two of those deserve a note: `windows_store_review_should_ask` must be a Boolean (the game reads
it with `getBoolForKey`), and `language` must exist as a String because the game's fallback
branch would otherwise read `stage` as a string.

`coins` is an Int32 on the game side, so the largest safe value is 2 147 483 647. Do not use the
maximum: the game adds rewards on top, and the add overflows into a negative number (a negative
coin count is clamped to `0x7FFFFFFF` on load, but the in-session value is already wrong).
About 2 000 000 000 is the practical ceiling.

## 8. Data root and save layout

```
%APPDATA%\Win32Craft\HillClimbRacing\
  localsettings.dat                    root LocalSettings map (not used by HCR's progress)
  localsettings_UserDefaults.dat       all progress: coins, upgrades, cars, stages, adfree, ...
  localsettings_TileNotifications.dat  the tile task's scratch container
  fsusda.data                          the game's own save blob (empty on this build)
```

Format of the `localsettings*.dat` files is a log, one entry per line, UTF-8:

```
type<TAB>key<TAB>value
```

with `i` = Int32, `b` = Boolean, `s` = String, `r` = remove key, `c` = clear the map. Escapes are
`\\`, `\t`, `\r`, `\n`. **The last line for a key wins**, and the file is only ever appended to,
so a run killed in the middle of a save cannot truncate the profile; loading compacts the log
back to one line per key. This is why the coin tests below can be verified by simply tailing the
file.

`fsusda.data` is written through `Compressor`/`Decompressor` (pass-through) and
`GetFileAsync`, which creates the file when it is missing and returns a real `StorageFile`, so a
first run with an empty profile is not an error path. On this build the file stays empty; all
real state is in `UserDefaults`.

There is **no automatic migration code** in the host. The folder was renamed from the early
`Win32Craft\HCR` to `Win32Craft\HillClimbRacing` by hand on the test device, matching the
profile rename (`key` `hcr` -> `hillclimbracing`, `data_subdir` `Win32Craft\HCR` ->
`Win32Craft\HillClimbRacing`).

## 9. Language

`win32craft.ini` next to the exe:

```ini
; Win32Craft settings (read by win32craft.dll next to the exe)
; language: en, de, fr, it, es, ru, pt, ja, ko, zh, zh-TW
[platform]
language=ru
```

* No file, no section or no key means English.
* The value feeds the seeded `language` LocalSettings string, which the game reads with
  `getStringForKey`, and the host locale used for the WinRT culture answers.
* HCR has its own language picker in the options menu, which writes the same key; after the
  player changes the language there, the ini value is no longer what the game uses. That is
  expected - the ini exists mainly for games that have no picker of their own.
* Unrelated to the host: some button captions render incorrectly in Russian and Chinese on this
  build. The same happens on Windows 10 Mobile, so it is a game asset/font issue, not a porting
  bug; fix it with game files only.

## 10. Keyboard

HCR reads touch points only. With `X104_FEAT_KEYBOARD_PAD` the host maps:

* Right arrow, Up arrow -> GAS
* Left arrow, Down arrow -> BRAKE

and swallows the key so it does not also reach the game as an unrelated input. This mirrors what
the Windows 10 build does with a keyboard, and it is what makes the game playable over VNC
without a touchscreen.

Mouse input arrives as a normal `PointerPoint` with `PointerId = 0xFFFFFF01` and `type = 2`; the
game ignores everything except `PointerId` and `Position`.

## 11. Store, IAP and the free coin packs

### 11.1 What the game does (reverse-engineered addresses, image base 0x400000)

| Address | Role |
|---|---|
| `0x403540` | builds the product table (ids, prices, coin amounts) |
| `0x4A21D0` | loads the settings; the coins global is `dword_5A6630`; a negative value is clamped to `0x7FFFFFFF` |
| `sub_4A2634(mode)` | saves the stats (called with 1 after a grant) |
| `sub_409A24()` | returns the singleton "pending purchase" struct |
| `sub_4DFEA4(pending, amount, flag)` | `*(pending+8) += amount`; `if (flag) ++*(pending+12)` |
| `sub_409B9C` / `sub_409BF4` | getters for `+8` (amount, clears it) and `+12` (count) |
| `sub_40BC30` | grants a purchase: shows "You have been rewarded %s coins.", adds the coins, `sub_4A2634(1)`, "Purchase complete", then `sub_4DF9CC` |
| `sub_43E4CC` | shop poller: `if (*(scene+312) > 0x14 && !(*(scene+312) % 0x3C)) sub_40BC30()` |
| `sub_42BC38` | coin pack click handler: checks `*(*(app+0x80)+0x1C0)` (Store availability); on failure "Unable to connect to the Windows Store."; on success builds `iap/playstore/<id>` and calls `sub_409B38` |
| `sub_409B38` | purchase start: builds an `std::string` (via `sub_404438`) and calls `sub_4DF390(pending, string)` |
| `sub_4DF540` | purchase result handler: status 0 = success -> amount lookup -> `sub_4DFEA4(*a1, *v6, 1)` |
| `sub_4E1810(map, string*)` | returns `node+0x28` (the coin amount for that product id) |
| `sub_4DF390` | registers the pending purchase (`sub_4E9924`, `sub_4DED88`) |

So the normal flow is: tap pack -> `sub_42BC38` -> `sub_409B38` -> async Store operation ->
`sub_4DF540` -> pending purchase filled -> the shop poller (`sub_43E4CC`) picks it up within a
few frames -> `sub_40BC30` grants the coins and saves.

### 11.2 The patch

Two bytes and one 0x34-byte stub, all applied by AutoModificator config entries as **raw file
offsets** (`file_offset = VA - 0x400C00` inside `.text`):

| Patch | VA | File offset | Original | New |
|---|---|---|---|---|
| Store-availability branch -> unconditional | `0x42BC50` | `0x2B050` | `C3 B1` (`CBZ R3`) | `18 E0` (`B +0x30`) |
| `sub_409B38` entry -> cave | `0x409B38` | `0x8F38` | `2D E9 90 48` | `11 F1 12 BA` (`B.W 0x51AF60`) |
| Stub | `0x51AF60` | `0x11A360` | zeros | 0x34 bytes |

The stub (Thumb-2) does:

1. scan the product id string in `r0` to the first `_`;
2. parse the decimal digits that follow it (the ids are `adfree_150000coins`,
   `adfree_20000000coins`, ...), stopping at the first non-digit; no digits -> no grant;
3. `sub_409A24()` to get the pending purchase, then `sub_4DFEA4(pending, amount, 0)`;
4. return.

The third argument `0` is important: it leaves the "consumed count" alone, so the ad-free unlock
path is not triggered by a coin pack.

The cave is inside the last mapped page of `.text`, beyond `VirtualSize`
(`0x11AF60..0x11B000`), so no `.pdata` unwind entry exists for the stub - it only pushes
`{r4,r5,r6,lr}` and pops the same set, so it never unwinds through itself.

Because the third argument is 0, the patch never increments the pending purchase's consumed
count, so it cannot accidentally unlock ad-free: tapping a coin pack only adds coins, and
`adfree` stays 0. A product id without trailing digits does nothing at all, so the patch is
inert on any other purchase entry point.

### 11.3 AutoModificator config

`config.json` next to `Auto_Modificator_llvm_ARM32.exe`, one switch:

```json
{
    "enableAll": false,
    "enableRAM": false,
    "RAMWarningDisabled": true,
    "path": "Win32Craft.HCR-arm32.exe",
    "switches": [
        {
            "title": "GET COINS: free coin packs (no Store)",
            "patches": [ "0x2b050 >> C3 >> 18", "0x2b051 >> B1 >> E0", "..." ]
        }
    ]
}
```

54 one-byte entries in total (2 for the branch, 4 for the redirect, 48 for the stub). The switch
state logic of the tool requires every byte to equal the target (ON) or every byte to equal the
default (OFF), which is why the entries are per byte.

Apply with:

```
Auto_Modificator_llvm_ARM32.exe --config config.json --enable-all --yes
```

This was run inside Wine on the device against the unpatched exe and produced a byte-identical
result to the Python generator (`md5 8b27345e4346c19832e3eed09936017c`), so the config is safe
to ship. Keep the original exe as `Win32Craft.HCR-arm32.exe.orig` in the package, and note that
the bootstrap patch (`.d3imp`, section 2) must already be applied before the cheat config runs -
the config was generated against the bootstrap-patched exe.

### 11.4 Verified behaviour and how to re-verify

Verified on the device:

* open GET COINS, tap a pack -> the page closes immediately (the flow no longer waits for the
  Store);
* about 1-3 seconds later the coins are added by the shop poller and the profile is saved;
* the grant is visible as a new `i<TAB>coins<TAB><value>` line at the end of
  `localsettings_UserDefaults.dat`;
* tapping 150 000 and 20 000 000 both worked (999 924 999 -> 1 000 074 999 -> 1 020 074 999);
* no message box appears and the game keeps running.

Re-verification loop:

1. back up `localsettings_UserDefaults.dat`;
2. start the game, note the coin count and the last `coins` line;
3. tap GET COINS, tap a pack, wait ~4 s;
4. `tail` the profile file and compare, then restart the game and confirm the value survives.

The pre-existing `LoadListingInformationAsync -> E_NOTIMPL` first-chance exception in the log is
expected and does not block the cheat.

## 12. Traps that cost time

1. **`IRandomAccessStreamWithContentType` must be ABI-flattened.** The interface adds
   `get_ContentType` after `Close`, but the vtable also has to contain the full
   `IRandomAccessStream` range (slots 6..14) plus `Close` (15) and `get_ContentType` (16).
   A short vtable crashes the game inside its own save code.
2. **`HeapReAlloc(NULL, ...)` returns NULL.** The first buffer allocation must use `HeapAlloc`;
   every grow helper (`x104_platform_grow`) does that.
3. **`ICompressorFactory::CreateCompressorEx` takes four arguments**
   `(IOutputStream, CompressAlgorithm, UInt32 blockSize, ICompressor**)`. Dropping the unused
   ones shifts the output pointer and the game writes through garbage.
4. **The settings containers are per-map.** Sharing one map between `UserDefaults` and
   `TileNotifications` destroys the player's progress on every start (the tile task clears its
   container).
5. **A missing settings key is fatal, and a wrong type is fatal in a different way.** Seed every
   key the game reads, with the exact type (`Boolean` for `windows_store_review_should_ask`,
   `String` for `language`).
6. **Default feature level 9_1 renders nothing.** `--feature-level=10_0` is not optional on a
   modern GPU. A 9_1-only GPU needs the opposite trick (`X104_FEAT_FL9_LIE`: a hardware 9_1 device,
   a `GetFeatureLevel` lie and the ANGLE `_level_9_1` suffix); without the lie the game refuses the
   device, without the suffix patch the 9_3 bytecode kills the RT driver.
7. **On Surface RT the driver drops the 9_1 device after ~25 presents** (`0x887A0020`). The game
   does rebuild the device, so this is not the MCPE "unrecoverable" case: it loops. Log rotation
   keeps the previous run as `win32craft.log.prev` (the file is append-only and moves aside at start
   when it passes 1 MiB), which is how a run killed by the modal box is recovered.
8. **Activatable classes must be factory-based** (`X104_CLASS_DEF_NEW` with
   `X104ActivateHook`) - HCR activates `Fingersoft.AdSdk.*` and its background task through
   `RoGetActivationFactory`-style paths during `OnLaunched`, and a non-factory class fails the
   activation with a class-not-registered error.
9. **`GetFileAsync` on a missing file must succeed** and create the file. Returning
   `HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)` puts the game into a fatal error path on a fresh
   profile.
10. **The player's ad-free flag is not `adfree` alone**: the game also reads
    `adFreeIAPPurchased`. Seeding one without the other leaves the game showing ads logic that
    expects the other value.
11. **Log first.** On the device the host log is the only feedback channel; the host prints the
    profile key and data root in the first 10 ms, which is the fastest way to catch a wrong exe
    or a wrong folder.

## 13. Build and run

Host build (`work/hcrhost/build.ps1`), LLVM ARM32 toolchain with MSVC headers/libs:

```
clang-cl --target=thumbv7-pc-windows-msvc -vctoolsdir <VS2022 MSVC> -winsdkdir <SDK 10>
         -winsdkversion 10.0.22621.0 /c /O2 /GS- /Gy ...
lld-link /dll /machine:arm /entry:DllMain /def:win32craft_arm32.def ...
```

Run on the device (display `:1`, VNC port 5901):

```
export DISPLAY=:1 WINEPREFIX=/home/MaxRM/.wine11 WINEARCH=win32
cd /home/MaxRM/WinTest/HCR_1.27.0.32_ARM32_Win32Craft
wine Win32Craft.HCR-arm32.exe --feature-level=10_0
```

Options that matter for this port (all parsed by the host, `--help` is not needed):

| Option | Meaning |
|---|---|
| `--feature-level=<level>` | cap the level the host lets through (`10_0` on Wine/Adreno, unset on a 9_1 device) |
| `--fl-lie=0\|1` | retry the hardware at 9_1, report 9_3 to the game, patch ANGLE's HLSL suffix (HCR profile defaults to 1; the lie only engages when the 9_3 request fails) |
| `--angle-profile=9_3` | keep ANGLE's own `_level_9_3` suffix while still lying about the level (A/B test) |
| `--warp` | use the WARP rasteriser (works everywhere, 1-3 fps on Tegra 3) |
| `--window=WxH` | window size (`640x360` is usable for WARP tests on RT) |
| `--trace-game-state` | Direct3D tracer: per-second frame summaries, the last 8 draws of a frame, and the draws of the frame in which the device was lost |

`/home/MaxRM/start-hcr.sh` is the wrapper that sets all of the above (and
`WINEDLLOVERRIDES`); `start-mcpe11.sh` / `start-mcpe-hw.sh` are the same pattern for Minecraft.
Use `wineserver -k` (with `WINEPREFIX` exported) to stop the game cleanly; `pkill -f Win32Craft`
also kills the `su`/shell that invoked it.

## 14. Testing on the device

The VNC display is driven with xdotool through a small helper (`gui.sh`), and screenshots come
from ImageMagick `import` on the root window, so all coordinates below are **screen**
coordinates (subtract `(4,30)` for the game's client coordinates).

Navigation:

| Action | Screen coordinate |
|---|---|
| NEXT (vehicle -> stage -> upgrade) | `(694, 355)` |
| START | `(672, 355)` |
| VEHICLE / MORE | `(215, 355)` |
| GET COINS | `(450, 355)` |

GET COINS page:

| Pack | Coordinate |
|---|---|
| 20 000 000 | `(226, 130)` |
| 8 000 000 | `(456, 130)` |
| 4 000 000 | `(686, 130)` |
| 2 000 000 | `(226, 322)` |
| 750 000 | `(456, 322)` |
| 150 000 | `(686, 322)` |
| BACK | `(215, 433)` |

Top panel: gear `(849, 57)`, close `(641, 57)`, languages `(685, 57)`, sound `(729, 57)`,
music `(772, 57)`; language rows UK/DE/FR/ES at `y` 129/192/263/341; BACK `(215, 433)`.

The coin cheat can be tested without earning anything: open GET COINS, tap any pack, wait for
the profile line, then restart and confirm.

## 15. Open items

* Windows 11 ARM64 run (should be the same host DLL, different Wine / no Wine).
* Surface RT (Windows RT 8.1) hardware rendering: the 9_1 device is created and frames are
  rendered, but the already-identified driver drop after ~25 presents is still unexplained. The
  next step is `--trace-game-state` (the loss dump in section 5) and a comparison of the draws in
  the dying frame with WARP. Real WinRT is also a different host environment: the fakes take
  priority over any real implementation, and the `.d3imp` import redirection must not shadow
  APIs that RT actually exports.
* `tools/make_hcr_package.py` (packaging, MANIFEST) does not exist yet; the folder is currently
  assembled by hand. When it is written, use `tools/saves_folder.py` with
  `write_saves_helper(out, "Win32Craft\\HillClimbRacing", "Hill Climb Racing")` so the package
  gets the same `Open-Saves-Folder.bat` / `.lnk` as the other games.
* The game's own `fsusda.data` is empty on this build. If a later version starts using it, the
  compressor pass-through will produce a file the real game cannot read; that is the point to
  implement MS-XCA.
* Temporary diagnostics in the host (`x104_input_diag_*`, `x104_pass_stage`, "UI queue item #N",
  `x104_diag_raise_notimpl`) are still present and should be removed before release.
* Sound/music were not part of the verified matrix on the Wine device; treat them as untested
  here rather than as working.
