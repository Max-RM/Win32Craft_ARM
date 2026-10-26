STATUS: DONE
## Geometry Dash 2.0.1.0 (WP8.0 Silverlight + cocos2d-x/ANGLE)

GD is a different kind of port from Minecraft PE and Redcon. It is **not** hosted by the x104 fake-object XAML host. The original app is a managed Silverlight shell (a `DrawingSurfaceBackground` compositor) around one native WinRT component, `GeometryJumpComponent.dll`. That component wraps cocos2d-x (`libcocos2d.dll`), ANGLE for WP8 (`libEGL_phone.dll` and `libGLESv2_phone.dll`), FMOD Ex (`fmodex_80_arm.dll`) and libcurl/OpenSSL. We replace the Silverlight shell and compositor with a small standalone native host, `tools/gd/gd_host.c`. The living build log is `tools/gd/GD_BUILD.md`. The decoded contracts are in `games_recon/geometry_dash_gd-{shell,component,render,deps,platform}.md` (cited below as gd-component F8 and so on). The go/no-go summary is in `games_recon/GAMES_PLAN.md`.

### 1. Status: milestone M1 done, the game is not playable yet

| Milestone | State |
|---|---|
| M0a import-chain check (`loadcheck`) | Done. 0 problems on the Surface 2 (Win10 15035). |
| M0b shaders at feature level 9_1 | Done. `tools/m0probe` created 30/30 game shader containers at 9_1 on the Surface 2 Tegra 4, including the 18 the game uses. |
| **M1 headless host** | **Done (2026-10-23).** Renders the real splash screen on the Surface 2. |
| M2 window, present, loop | Not started. |
| M3 input | Not started. |
| M4 on-demand WinRT | Only the boot-path part is done. |
| M5 audio | Shim written, never exercised. |
| M6 package and device matrix | Not started. |

What M1 proves, from the Surface 2 `gd_host.log`:
- Every step of the decoded DrawingSurface sequence returns `hr=0x0`: D3D11 device at FL 0x9100 (real Tegra 4), `SetCallback`, `CreateInstance(0)`, `set_WindowBounds(800,480)`, the 3 delegates, `CreateContentProvider`, `SetManipulationHost`, `Connect`, `PrepareResources` (desired size 800x480).
- `Draw #0` through `Draw #7` all return `hr=0x0`, and each calls `RequestAdditionalFrame`.
- `Draw #0` boots ANGLE on our device, runs `CCApplication::run` and loads the assets.
- `gd_frame.bmp` is the real GD splash: green logo, orange RobTop, blue brick background, loading bar. It is the raw 480x800 **portrait** render target, so the picture appears rotated 90 degrees clockwise. Rotate it 90 degrees counter-clockwise to view it upright. A PNG copy is `dist-gd/gd_frame.png`.

What is **not** done or not verified:
- **No window, no present, no input, no audio.** `gd_host.exe` is a console probe: it draws 8 frames, dumps frame 0, and exits. It is a diagnostic, not a playable game.
- Only the Surface 2 has been tested. Surface RT 8.1 and your POCO X3 Pro (Win11 ARM64) have never run it. The raw device log is not archived under `arm32/device_logs`.
- The ANGLE feature-level knob (section 5) is not applied. The splash rendered without it.
- The portrait-RT rotation direction and the pointer mapping are inferred from code (gd-component F9) and have not been seen running.
- Whether FMOD initialised during the 8 frames is unknown. The `PhoneAudioSes` shim does not log.

### 2. How to run it on an ARM Windows device

**Package.** In the hand-off bundle it is `runnable/GeometryDash_M1_host.zip`. It is the same content as the repo's `dist-gd-host.zip` (48,478,324 bytes) and `dist-gd-host/` folder (57 MB, 328 asset files). The zip has no top-level folder, so extract it into a new empty folder. It contains:
- `gd_host.exe` (ARM32 console exe, 14 KB, imports KERNEL32 only).
- The 8 game DLLs, with the APPCONTAINER bit cleared in `fmodex_80_arm.dll`.
- The app-local VC110 trio `msvcr110.dll`, `msvcp110.dll` and `vccorlib110.dll`.
- The two shims `PhoneAppModelHost.dll` and `PhoneAudioSes.dll`.
- `Assets\Resources\` and `run_gd_host.bat`.

It contains extracted game DLLs and assets. Keep it private; it must never go in the public source zip or git.

**Steps.**
1. Extract the zip to a writable folder, for example `C:\gd\`, and keep the folder intact. The exe's folder is the game's "install location" and must contain `Assets\Resources\`.
2. Run `run_gd_host.bat`. It does `cd /d "%~dp0"`, runs `gd_host.exe`, and pauses.
3. Wait. The first `Draw` blocks for seconds while ANGLE starts and `CCApplication::run` loads the assets, and nothing prints in between. Do not kill it.
4. Collect the results from `%LOCALAPPDATA%` (`C:\Users\<you>\AppData\Local`):
   - `gd_host.log` is **appended** across runs and flushed per line, so it survives a hard crash. Delete it first for a clean run.
   - `gd_frame.bmp` is the first rendered frame.
   - `GeometryDash\` is the game's save folder.

**Success looks like** these lines in `gd_host.log`:
- `D3D11 device created, feature level 0x...`
- `Draw #0 hr=0x0`
- `wrote gd_frame.bmp (480x800)`
- `Draw #7 hr=0x0`
- `==== gd_host M1 done`

The host asks for the device's real maximum feature level (list 11_1 down to 9_1, BGRA, with a WARP fallback) and does not force 9_1. The Surface 2 gives `0x9100`. On the POCO X3 Pro expect a level of 9_3 or higher. That is unmeasured, and so is whether the ARM32 D3D11 driver path works there.

**If it fails:**
- The log stops at the last completed step.
- `!!! CRASH code=... in module+off` comes from the unhandled-exception filter. C++/CX fail-fast bypasses that filter, so a silent stop usually means a WinRT call threw.
- Every call the component makes into a host fake is logged, and unexpected ones say `UNEXPECTED`.
- For DLL load problems use `dist-gd-loadcheck.zip` (`run_loadcheck.bat`). It should print `RESULT: 0 problem(s)`.
- Never run any of the ARM binaries on the x64 PC.

### 3. How it works

**The DrawingSurface contract.** `gd_host.c` `mainCRTStartup` implements this order (gd-component F11):
1. `RoInitialize(MTA)`. Create the D3D11 device with BGRA support. Create a **portrait 480x800** BGRA8 render target with an RTV (`RENDER_TARGET|SHADER_RESOURCE`).
2. `LoadLibrary("GeometryJumpComponent.dll")`, then `DllGetActivationFactory`.
3. `BroswerEventHelper` statics {6AF73B9D-6C05-39B0-90FC-1CA5238260B1}, slot 6 `SetCallback(ICallback)`. The class name is misspelled that way in the game.
4. `Direct3DInterop` factory {E8D96887-B552-373B-AFBF-838B9195D4FD}, slot 6 `CreateInstance(0)` (0 = Landscape). Then QI {D8485BA8-71AD-35C3-83DF-5B244DA34AA6}:
   - slot 19 `set_WindowBounds` takes a `Size{800,480}` passed in VFP registers s0/s1.
   - slots 20/21/22 are `SetCocos2dEventDelegate`, `SetCocos2dMessageBoxDelegate` and `SetCocos2dEditBoxDelegate`. They must be set **before** the first `Draw`; the component copies them to the view only after `run()` returns.
5. Slot 6 `CreateContentProvider`, then QI `IDrawingSurfaceBackgroundContentProviderNative` {262C1892-975A-45AA-8FE7-F25C2C4088E3}. Separately QI {171B7EF7-A96F-4E37-B936-7A0B10F35163} and call `SetManipulationHost` (slot 6).
6. `Connect(runtimeHost, device)` is slot 3. Then per frame, on one thread, `PrepareResources` is slot 5 and `Draw(device, ctx, rtv)` is slot 6.

**One `Draw()` is one whole cocos2d frame.** It drains queued input, renders, and ends with `RequestAdditionalFrame`. The very first `Draw` also boots the game. ANGLE reuses the **host's** D3D11 device. There is no `D3D11CreateDevice`, HWND or swap chain inside the component. ANGLE renders offscreen and copies into the host RTV on `eglSwapBuffers` (gd-render F9). The game draws landscape content into a portrait-shaped texture with its own 90-degree projection rotation, so the **host must rotate when presenting**.

**Hand-written COM fakes.** These are static C vtables in `gd_host.c`. AddRef and Release are no-ops, and every slot is logged:
- `ICallback` {009BC214-16B9-37D2-8A4F-DD7628D0B7CB}, 5 methods. It must always return `S_OK`: a failure HRESULT makes the component throw a C++/CX exception.
- The 3 delegates (event {0ED04E82-...}, message box {4DBE8C04-...}, edit box {3C7D6A8A-...}). `Invoke` is slot 3.
- `IDrawingSurfaceRuntimeHostNative`: only slot 3 `RequestAdditionalFrame` is used.
- `DrawingSurfaceManipulationHost`: a 12-slot IInspectable. `add_PointerMoved`, `add_PointerPressed` and `add_PointerReleased` are slots 6, 8 and 10. The component's three TypedEventHandlers are stored in `g_ptr_moved`, `g_ptr_pressed` and `g_ptr_released` for M3.

All fakes answer `IAgileObject`. If they did not, the component would capture COM contexts.

**The WinRT trap, solved.** cocos2d finds `Assets\Resources\` through `Package.Current.InstalledLocation.Path` and its save root through `ApplicationData.Current.LocalFolder.Path`. Without package identity, both fail-fast in a plain Win32 process. The host patches the IAT of `RoGetActivationFactory` (`iat_patch`) in `vccorlib110`, `libcocos2d`, `libEGL_phone`, `libGLESv2_phone` and `fmodex_80_arm`. `hook_RoGAF` returns fake statics for `Windows.ApplicationModel.Package` and `Windows.Storage.ApplicationData`. The chain is `IPackageStatics` {4E534BDF-...} to `IPackage` to a `StorageItem` whose `IStorageItem.get_Path` is slot 12. The paths are the exe folder and `%LOCALAPPDATA%\GeometryDash`. Every other class is forwarded to combase and logged; for example `Windows.System.Threading.ThreadPool` works from the real OS. Lesson learned: vccorlib asks for the generic `IActivationFactory` {00000035-...} first and then QIs it for the statics interface. The fake has to return itself directly and answer both IIDs.

**Dependency shims.**
- `PhoneAppModelHost.dll` (`tools/gd/phoneappmodelhost.c`): `CaptureUiThreadContext` returns `E_NOTIMPL`. It **must** fail, or PPL continuations marshal to a non-existent UI context. `LoadPackagedLibrary` calls `LoadLibraryExW`.
- `PhoneAudioSes.dll` (`tools/gd/phoneaudioses.c`): maps the phone WASAPI-activation layer onto mmdevapi.
- VC110 runtime: app-local, taken from `D:\repo\armdeps\Microsoft.VCLibs.110.00_11.0.51106.1_arm__8wekyb3d8bbwe.Appx`.
- No api-set shims were needed (`loadcheck`).

### 4. Build (on the x64 PC, test on the device)

Defaults are in the `.bat` files and can be overridden with `LLVM_BIN`, `SDKROOT`, `SDKVER`, `MSVCINC`, `BUILD` and `DIST`. The defaults are our custom LLVM at `D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin`, the Windows SDK 10.0.19041 at `D:\Windows Kits\10`, and the MSVC headers of VS2022 BuildTools. Point them at your own installs. Any clang plus `lld-link` that targets `thumbv7-pc-windows-msvc` should work, but this has not been tried with a stock LLVM.

```
tools\gd\build_gd_shims_arm32.bat     -> dist-gd\PhoneAppModelHost.dll, dist-gd\PhoneAudioSes.dll
tools\gd\build_gd_host_arm32.bat      -> dist-gd\gd_host.exe
python -I tools\gd\make_gd_host_package.py   -> dist-gd-host\
```

The compiler flags are `--target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin`. The link flags are `/machine:arm /nodefaultlib /entry:mainCRTStartup` with `kernel32.lib` and `armrt.lib`.

`make_gd_host_package.py` needs these inputs:
- `Geometry_Dash_2.0.1.0\` in the repo root (the extracted xap, read-only).
- The VCLibs110 appx.
- The built `dist-gd\` files.

It copies the DLLs and assets and clears the fmod APPCONTAINER byte (file offset 0x15F, `0x11` to `0x01`). It does **not** create the zip. Compress `dist-gd-host\`'s contents yourself.

**Shortest dev loop.** `source.zip` has no game folders, so `make_gd_host_package.py` will not run without your own copy of the game. You do not need it. Extract `runnable/GeometryDash_M1_host.zip` once, rebuild only `gd_host.exe` with the second `.bat`, and copy it over the one in the extracted folder.

**Recurring bug.** MSVC `<stdarg.h>` `va_start` is wrong under clang-ARM and gives garbage varargs. Use `__builtin_va_*`, as `gd_host.c` does with `GD_VA`. The tiny logger supports `%s %S %d %u %x %p`, not width: `%08x` prints as `08x` (cosmetic).

### 5. How to continue

**M2: visible, upright, animated.** This is the next step.
- Create a Win32 window and an `IDXGISwapChain1` on the **same** device with `IDXGIFactory2::CreateSwapChainForHwnd`. A working freestanding pattern is T6 in `tools/m0probe/m0probe.c` (`make_window` and `CreateSwapChainForHwnd`, about line 1966).
- Write a present pass. `m0probe.c` has a ready textured-quad kit (`quad_init`, about line 1404). It uses fxc-compiled `vs_4_0_level_9_1` and `ps_4_0_level_9_1` blobs (`k_quad_vs` and `k_quad_ps`) with a POSITION+TEXCOORD0 layout. Use the portrait RT as the SRV and assign the quad's UVs rotated by 90 degrees **counter-clockwise** (orientation flag 0 produces a picture rotated clockwise). This should need no shader change.
- Set all pipeline state explicitly every frame. ANGLE shares the host immediate context and leaves its own state behind. Unbind the RTV before sampling the texture.
- Run `PeekMessage`, `PrepareResources`, `Draw` and present on **one** thread (the thread that owns the D3D11 context), at display rate. The `RequestAdditionalFrame` flag can be ignored.
- Keep the RT size constant. A size change triggers ANGLE's size-dependent reset. The game's logical frame is the texture size with width and height swapped.
- Delegates are invoked from the Draw thread. They must not block; post to the window thread. `Cocos2dEventDelegate.Invoke(2)` (TerminateApp) sets `g_want_quit` today. The game never exits any other way, so turn it into `WM_CLOSE`, then `Disconnect()` (slot 4).
- On minimise, `Disconnect` pauses the game, and `Connect` plus `Draw` resumes it.

**M3: input.** Fill in the stored handlers.
- Build a fake `PointerEventArgs`: it must QI `IAgileObject`, and its slot 6 `get_CurrentPoint` returns the point.
- Build a fake `PointerPoint`: slot 7 `get_Position` writes two floats (x, then y), and slot 9 `get_PointerId` returns a `UINT32`.
- Call the stored handler's `Invoke` (slot 3) as `(handler, &g_manip_host, args)` from the window thread. The component queues the event under a mutex and runs it inside the next `Draw`, so this is thread-safe.
- Coordinates are **RT pixels in the portrait frame**, with no ResolutionScale in XAML mode (gd-component F8, corrected by gd-platform F3). For orientation 1: `xp = Wp - gy*scale`, `yp = gx*scale`. Log the `CCPoint` the game receives to verify. If the picture or the touch is mirrored, try orientation 4 via `CreateInstance(1)` or `updateOrientation` (slot 11).
- Keyboard and back go through `Direct3DInterop`: slot 12 `OnBackKeyPress` (map Esc, Back and Alt+Left; never auto-close on them), slot 13 `OnCocos2dKeyEvent(key)` and slot 14 `OnCocos2dKeyEvent(key, text)`. The key enum is Text=0, Escape=1, Back=2, Enter=3. Send `WM_CHAR` as Text with UTF-16 and never send empty text.
- Slot 15 `OnCocos2dEditboxEvent` returns the result of the modal edit box, which needs a real dialog (gd-shell F14). Message boxes are fire-and-forget (gd-shell F13).
- `ShowKeyboard` and `HideKeyboard` (event 0 and 1) should drive the TabTip controller already in `win32_host.c`. See `arm32/OSK_NOTES.md`; the Windows 10 and Windows 11 logic differ. Keep all three delegates registered. Otherwise the game falls back to the phone-only `KeyboardInputBuffer`, which does not exist on desktop.

**M4: remaining WinRT and `ICallback`.**
- Still to do: `Windows.Foundation.Uri` plus `Launcher.LaunchUriAsync` for `openURL`, and optionally `Accelerometer`. Neither is on the boot path. Unknown classes are already forwarded and logged. Whether the OS `Launcher` works unpackaged is unmeasured; the 0.10.4 host has a `Launcher` fake (gd-platform F11) to copy.
- `DisplayProperties` and `CoreWindow` are **not needed**, because the XAML-mode byte `CCEGLView+0x17C` is set.
- `ICallback` is stubbed today: `isNetworkAvailable` returns TRUE, `isLowMemoryDevice` returns FALSE, `getUniqueUserID` returns 40 zeros (a stable 40-hex ID per device would be better), and `openEmail` and `rateApp` are ignored.

**M5: audio.** FMOD's WASAPI output module calls the `PhoneAudioSes` shim during `System::init`:
- `GetDefaultAudioRenderId(0)` and `GetDefaultAudioCaptureId(0)` return `CoTaskMemAlloc`'d UTF-16 endpoint ids.
- `ActivateAudioInterface(id, iid, &ppv)` is synchronous and asks for IAudioClient2 {726778CD-...} or IAudioClient {1CB9AD4C-...}.

These signatures are **inferred** from call sites (gd-deps F5). Confirm them on the first audio run and add logging to the shim. The fallback is a silent shim.

**M6: package and device matrix.** Test on the Surface 2, then Surface RT 8.1 (Tegra 3, FL 9_1, RT 8.1 never run), then the POCO X3 Pro. Run the M1 package on the POCO X3 Pro first and keep `gd_host.log` and `gd_frame.bmp`: they are the first data point for that device.

**ANGLE feature-level knob.** The game always tells ANGLE "9_3", whatever the real device level is. Its precompiled shaders are level 9_3. A real 9_1 GPU may need ANGLE on its 9_1 paths:
- Option A: patch `GeometryJumpComponent.dll` at file offset `0x357FC` from `03 26` to `01 26`. The package DLL is currently unpatched.
- Option B, preferred: hook `CreateWinrtEglWindow` and rewrite its 2nd argument. The component imports it from `libGLESv2_phone.dll` **by ordinal 181**. `iat_patch` currently skips ordinal imports, so it needs extending.
- Gate either option on the device's real `GetFeatureLevel() == 0x9100`. Do not force 9_1 on capable GPUs.
- If a real 9_1 GPU rejects the shaders, the fallback is 18 hand-written level_9_1 replacements swapped in by DXBC checksum (gd-render F7, with a proof of concept), or WARP at 9_3. The Surface 2 Tegra 4 needed neither for the splash.

**Open risks:**
- Performance on Tegra is unmeasured beyond the splash.
- Non-power-of-two textures with GL_REPEAT on 9_1 were not checked per texture.
- The portrait-RT direction and the pointer mapping are inferred.

**Rules.**
- Build on the x64 PC and test on the device. Never run ARM binaries on the x64 PC.
- Keep game folders, `*.appx`, `*.xap` and `*.pdf` out of git and out of the public zip.
