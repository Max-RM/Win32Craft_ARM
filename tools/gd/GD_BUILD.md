# Geometry Dash 2.0.1.0 — Win32Craft host build log

Living checkpoint for the GD port. Recon is in `games_recon/geometry_dash_gd-{shell,render,component,deps,platform}.md`
(all STATUS: DONE) and summarised in `games_recon/GAMES_PLAN.md`. Game folder: `Geometry_Dash_2.0.1.0/` (READ-ONLY,
never in git/zip). GD is WP8.0 **Silverlight + DrawingSurface + cocos2d-x/ANGLE** — a NEW host type, unrelated to the
x104/0.10.4 fake-object XAML host used by Minecraft PE and Redcon.

## Architecture (decoded)
Host replaces the Silverlight shell + DrawingSurfaceBackground compositor. It loads `GeometryJumpComponent.dll`, runs
the native `Direct3DInterop`, and drives one `Draw()` per frame (= one whole cocos2d game frame, on one thread that owns
the D3D11 immediate context). The game renders into the RTV the host passes to Draw; ANGLE uses the host's device (no
D3D11CreateDevice in the XAML path). Host also presents that texture with a 90° rotation (portrait-shaped RT).

Key contracts (gd-component F1/F4/F5/F8/F11, gd-shell F05/F19):
- Activation: `DllGetActivationFactory` → BroswerEventHelper (statics IID {6AF73B9D-6C05-39B0-90FC-1CA5238260B1},
  SetCallback=slot6) and Direct3DInterop (factory IID {E8D96887-B552-373B-AFBF-838B9195D4FD}, CreateInstance(int orientation)=slot6).
- Direct3DInterop default iface {D8485BA8-71AD-35C3-83DF-5B244DA34AA6}: slots 6 CreateContentProvider, 11 updateOrientation,
  12 OnBackKeyPress, 13/14 OnCocos2dKeyEvent, 15 OnCocos2dEditboxEvent, 18/19 get/set_WindowBounds (Size in s0/s1),
  20/21/22 SetCocos2d{Event,MessageBox,EditBox}Delegate.  Also QI {171B7EF7-A96F-4E37-B936-7A0B10F35163}
  = IDrawingSurfaceManipulationHandler (SetManipulationHost=slot6).
- Content provider → QI {262C1892-975A-45AA-8FE7-F25C2C4088E3} = IDrawingSurfaceBackgroundContentProviderNative,
  4 methods: slot3 Connect(host, device), 4 Disconnect(), 5 PrepareResources(LARGE_INTEGER* time, DrawingSurfaceSizeF* desiredSize),
  6 Draw(ID3D11Device*, ID3D11DeviceContext*, ID3D11RenderTargetView*).
- Host-supplied fakes: IDrawingSurfaceRuntimeHostNative (IUnknown + slot3 RequestAdditionalFrame(void); other slots E_NOTIMPL),
  DrawingSurfaceManipulationHost (12-slot IInspectable; add_ Pointer Moved/Pressed/Released at slots 6/8/10),
  PointerEventArgs (slot6 get_CurrentPoint; must QI IAgileObject {94EA2B94-E9CC-49E0-C0FF-EE64CA8F5B90}) +
  PointerPoint (slot7 get_Position as 2 floats, slot9 get_PointerId), ICallback {009BC214-16B9-37D2-8A4F-DD7628D0B7CB}
  (5 methods, all S_OK), 3 delegates (Event {0ED04E82-...}, MsgBox {4DBE8C04-...}, EditBox {3C7D6A8A-...}; Invoke=slot3).
- Delegates/callbacks are invoked on the Draw thread. Pointer positions are in RT PIXEL space, PORTRAIT frame, NO
  ResolutionScale in XAML mode (gd-component F8 corrects gd-shell F09). Orientation flag 0 = Landscape.
- Render: all shaders precompiled (no HLSL compiler in-process); the game always asks ANGLE for FL 9_3 (compare-with-1/2/3
  never matches → default 3). On a real 9_1 device patch `GeometryJumpComponent.dll` file off 0x357FC `03 26`→`01 26`,
  or hook CreateWinrtEglWindow, so ANGLE takes its 9_1 paths (gd-render F4/F5). m0probe already proved the 18 used game
  shader containers CreateVertexShader/PixelShader OK at 9_1 on the Surface 2 Tegra (memory project_redcon_gd), so the
  level_9_1 shader swap (gd-render F7) is likely NOT needed on Surface 2; keep it ready for Surface RT.

## Dependency plan (gd-deps) — low-risk
Ship the game's native DLLs as-is + app-local VC110 trio (msvcr110/msvcp110/vccorlib110, NO import rename; from
D:\repo\armdeps\Microsoft.VCLibs.110.00_*.Appx or vcredist_arm_vs2012.exe) + 2 shims + clear one fmod APPCONTAINER byte.
- `PhoneAppModelHost.dll` shim: CaptureUiThreadContext → E_NOTIMPL (MUST fail), LoadPackagedLibrary → LoadLibraryExW,
  LaunchEmergencyDialer → E_NOTIMPL.  **BUILT** (tools/gd/phoneappmodelhost.c/.def).
- `PhoneAudioSes.dll` shim on mmdevapi: GetDefaultAudio{Render,Capture}Id(role) → MMDeviceEnumerator default endpoint id
  (CoTaskMemAlloc'd), ActivateAudioInterface(id,iid,ppv) → GetDevice→Activate.  **BUILT** (tools/gd/phoneaudioses.c/.def).
  Signatures inferred (gd-deps F5) — confirm on first audio run (M5).
- fmodex_80_arm.dll: clear IMAGE_DLLCHARACTERISTICS_APPCONTAINER (file byte 0x15F 0x11→0x01). Ship fmodex_80_arm.dll
  (not _80L). Omit libwebsockets.dll + fmodex_80L_arm.dll (unreferenced). libcurl/libeay32/ssleay32 as-is (network, optional).
- api-ms-win-core-* (16 contracts): served by OS on RT 8.1 (loadcheck 6.3.9600 proved 14; libraryloader/debug by inference)
  and Win10 (superset). No shim expected; if loadcheck flags one, `tools/make_arm32_apiset_shims.py --pe <gd dll> --out <dir>`.

Build shims: `tools/gd/build_gd_shims_arm32.bat` → dist-gd/PhoneAppModelHost.dll + PhoneAudioSes.dll (clang thumbv7,
freestanding, /nodefaultlib). Verified exports + imports (PhoneAppModelHost→KERNEL32; PhoneAudioSes→KERNEL32+ole32).

## Milestones
- M0a loadcheck (import chain): **DONE + shipped.** Both shims built; loadcheck.c module list extended with the GD DLLs +
  VC110 trio + shims (build_loadcheck_arm32.bat → dist-arm32/loadcheck.exe); tools/gd/make_gd_loadcheck_package.py assembles
  dist-gd-loadcheck/ (loadcheck.exe + 8 game DLLs with fmod APPCONTAINER byte 0x15F 0x11→0x01 cleared + VC110 trio from the
  VCLibs110 appx + 2 shims + run_loadcheck.bat). Sent dist-gd-loadcheck.zip to the user → run on Surface 2/RT, expect
  "RESULT: 0 problem(s)". **CONFIRMED on the Surface 2 (Win10 15035, E:\loadcheck.txt): 0 problems** - every GD DLL's
  imports resolve (all api-sets served by combase/KERNELBASE/KERNEL32 incl. libraryloader+debug which the recon only
  inferred; VC110 trio + both shims load). Whole loader story validated; no api-set shims needed.
- M0b shader/FL: mostly done via m0probe (30/30 at 9_1). TODO: the ANGLE FL-9_3→9_1 knob (hook CreateWinrtEglWindow or
  the 1-byte patch), applied from the host on 9_1 devices.
- M1 headless host skeleton: **host code BUILT + compiling (ARM32).** tools/gd/gd_host.c + build_gd_host_arm32.bat →
  dist-gd/gd_host.exe (console, imports KERNEL32 only; d3d11/combase/GeometryJumpComponent loaded at run time). It:
  RoInitialize(MTA); D3D11CreateDevice at the device's real max level (list 11_1..9_1, BGRA; not forced to 9_1) + a
  portrait 480x800 BGRA8 RT+RTV; LoadLibrary GeometryJumpComponent + DllGetActivationFactory; runs F11 steps 2-7
  (BroswerEventHelper.SetCallback(ICallback), Direct3DInterop factory→CreateInstance(0)→QI {D8485BA8}, set_WindowBounds
  {800,480} as HFA s0/s1, SetCocos2d{Event,MsgBox,EditBox}Delegate, CreateContentProvider→QI {262C1892} native,
  QI {171B7EF7} handler→SetManipulationHost, Connect(runtime-host,device)); then PrepareResources+Draw x8, dumps the
  first frame to gd_frame.bmp. Fake COM objects: ICallback(5 methods S_OK), 3 delegates, runtime-host (slot3
  RequestAdditionalFrame), manipulation-host (add_ slots 6/8/10, stores the component's pointer handlers for M3). Every
  slot the component calls on a fake is logged (trap unexpected). Reused the clang-ARM __builtin_va_* fix (GD_VA) - the
  recurring MSVC-va_start bug. **COUPLING FOUND:** cocos2d finds its 47 MB Assets/ via WinRT (Windows.ApplicationModel.Package
  InstalledLocation + Windows.Storage, gd-deps F10), so the first Draw cannot load assets / render until those shims exist
  → M1's first *rendering* run needs the minimal M4 asset-path shims (Package/Storage/DisplayProperties).
  **M1 diagnostic package SHIPPED:** tools/gd/make_gd_host_package.py → dist-gd-host/ (gd_host.exe + 8 game DLLs w/ fmod
  byte cleared + VC110 + 2 shims + the game's Assets/, 58 MB) → dist-gd-host.zip (46 MB) sent to the user. gd_host.c now
  FlushFileBuffers each log line so the log survives a hard crash in the first Draw. Awaiting the device gd_host.log: it
  will confirm the F11 host logic drives the component end-to-end (device created, factories, CreateInstance, delegates,
  CreateContentProvider, Connect, Draw) and show exactly where cocos2d stops (expected: asset loading via
  Windows.ApplicationModel.Package, which lacks package identity in a plain Win32 process → next build = the WinRT
  asset-path shim).

  **UPDATE (device): M1 host logic VALIDATED on the Surface 2 (gd_host.log).** The ENTIRE F11 sequence runs, every call
  hr=0x0: D3D11 device at FL 0x9100 (real Tegra 9_1), portrait RTV, SetCallback, CreateInstance(0), set_WindowBounds(800,480)
  (HFA correct), 3 delegates, CreateContentProvider + QI {262C1892} native, QI {171B7EF7} handler + SetManipulationHost
  (component registered its 3 pointer handlers on our fake), Connect, PrepareResources->desired=800x480. Then the log STOPS
  inside the first Draw (the post-Draw "Draw #0" line never appears) = hangs/crashes during ANGLE init + CCApplication::run()
  /asset load, as predicted. The hand-rolled COM host is correct; only the in-Draw WinRT activation remains.
  **Diagnostic v2 SHIPPED:** gd_host.c now IAT-patches RoGetActivationFactory in vccorlib110/libcocos2d/libEGL/libGLESv2/fmod
  (iat_patch) to LOG every WinRT class activated then forward to real combase, + a crash_filter (SetUnhandledExceptionFilter)
  logging code/addr/module+offset. dist-gd-host.zip (46 MB) re-sent. Next gd_host.log reveals the exact WinRT class + crash
  site to shim.
- M4 (partial): WinRT asset-path shims. **Blocker CONFIRMED by v2 log:** inside the first Draw the ONLY WinRT activation
  before the stop is RoGetActivationFactory("Windows.ApplicationModel.Package") (OS returned a factory hr=0, then
  IPackageStatics::get_Current died - no package identity; no crash line because C++/CX terminate -> fail-fast bypasses the
  filter). The IAT hook on RoGetActivationFactory works (vccorlib's GetActivationFactoryByPCWSTR calls it). **v3 BUILT+SHIPPED:**
  gd_host.c now fakes the two chains from gd-platform F2: IPackageStatics {4E534BDF} slot6 get_Current -> IPackage slot7
  get_InstalledLocation -> StorageItem (QI IStorageItem {4207A996}, slot12 get_Path) = the exe dir (holds Assets\Resources\);
  and IApplicationDataStatics {5612147B} slot6 get_Current -> IApplicationData slot12 get_LocalFolder -> StorageItem =
  %LOCALAPPDATA%\GeometryDash. hook_RoGAF returns these for those 2 class names, forwards+logs everything else. Paths set in
  main before the first Draw. dist-gd-host.zip (46 MB) re-sent. Next log: expect get_Path calls then either gd_frame.bmp or
  the next blocker (ANGLE init / another class / a render issue). Uri+Launcher (on-demand, openURL) and Accelerometer are
  NOT on the boot path (gd-platform F3/F4); DisplayProperties/CoreWindow NOT faked (XAML byte +0x17C off, F3).
  **v3 miss + v4 fix:** v3's pkgstat_QI returned E_NOINTERFACE because vccorlib's GetActivationFactoryByPCWSTR asks RoGAF
  for the GENERIC IActivationFactory {00000035-...} first, then QIs for the statics iid - it never passes IPackageStatics to
  RoGAF. v4: hook_RoGAF returns the fake statics object DIRECTLY for the 2 class names + QI answers IActivationFactory.

  ### *** M1 COMPLETE - GEOMETRY DASH RENDERS AT FL 9_1 (2026-10-23, gd_host.log + gd_frame.bmp) ***
  v4 device run: the full chain ran - Package.get_Current->get_InstalledLocation->get_Path(C:\...\dist-gd-host),
  ApplicationData->LocalFolder->get_Path (x4, save paths), ICallback.getUniqueUserID + isLowMemoryDevice, then
  Windows.System.Threading.ThreadPool (forwarded to OS, works), then **Draw #0 hr=0x0, wrote gd_frame.bmp (480x800),
  Draws #1-7 all hr=0x0, RequestAdditionalFrame=1 each**. gd_frame.bmp = the real GD splash screen (green GEOMETRY DASH
  logo, orange RobTop, blue brick bg, loading bar, achievements tip), rotated 90 deg (portrait RT; the game draws
  landscape-into-portrait and the host rotates on present - confirms gd-component F9). So the whole pipeline works:
  component boot -> ANGLE on our FL9_1 device -> cocos2d asset load -> rendered frames. The hand-rolled host is correct.
  Minor: the tiny logger does not support %08x width so iid printed as "08x" (cosmetic; fix l_fmtu/%x width if wanted).
- **M2 + M3 BUILT + SHIPPED (2026-10-23, windowed):** gd_host.c is now a real windowed app. Added: make_window + gd_wndproc
  (WM_CLOSE/LBUTTON*/MOUSEMOVE/KEYDOWN/CHAR); present_init (DXGI IDXGIFactory2::CreateSwapChainForHwnd, landscape 800x480
  B8G8R8A8, FLIP_SEQUENTIAL w/ DISCARD fallback) + present_frame (quad VS/PS = k_quad_vs/k_quad_ps from m0probe via
  tools/gd/gd_quad_shaders.h, samples the portrait 480x800 RT rotated 90deg CCW into the back buffer, Present(1,0)); the
  main loop = PeekMessage drain + PrepareResources + Draw(portrait rtv) + present_frame, continuous until WM_CLOSE or
  TerminateApp. M3 input: fake PointerEventArgs/PointerPoint (ring of 64 so queued events keep distinct coords; args QI
  answers IAgileObject; get_Position slot7, get_PointerId slot9, get_CurrentPoint slot6) + send_pointer maps window px ->
  portrait RT pixel (px=my*RTW/winH, py=(1-mx/winW)*RTH) and Invokes the component's stored g_ptr_pressed/moved/released;
  keyboard -> Direct3DInterop OnBackKeyPress(slot12)/OnCocos2dKeyEvent(slot13 Enter/Back, slot14 Text). Build links
  user32.lib now. dist-gd-host.zip (46 MB) sent. **UNVERIFIED on device (first windowed run):** the 90-CCW rotation
  direction + the pointer mapping are inferred - if the picture is sideways/mirrored or taps are off, flip the present verts'
  UVs and the send_pointer formula consistently (or use orientation flag 1 / updateOrientation). Still TODO after it runs
  right: M4 Uri/Launcher (on-demand), M5 audio (PhoneAudioSes real test at FMOD System::init), the ANGLE 9_3->9_1 knob if
  the menu/gameplay (beyond the splash) glitches on 9_1, M6 Surface 2 + RT 8.1. THEN refresh HANDOFF_FOR_FRIEND.md (GD
  section) + the bundle to match the finished, playable state.
- **M2/M3 VERIFIED on device (2026-10-23/24):** picture is upright (present verts' UVs flipped 180deg: window(u,v) samples
  RT uv (1-v,u); send_pointer maps px=(1-my/winH)*RTW, py=(mx/winW)*RTH); windowed run is stable 1200+ frames after
  switching the swap chain to DISCARD (blt) + unbinding the RTV/SRV before Present (FLIP invalidates the back-buffer RTV and
  crashed after a few flips); first touch no longer crashes after the manipulation-host add_Pointer* AddRef the component's
  delegates (dg_hold; they were use-after-free'd). Audio confirmed (FMOD via PhoneAudioSes). Input (mouse/touch + keyboard)
  works. REMAINING on device: the loading bar fills 0->~80% then freezes while the game keeps drawing + the menu theme loops.
- **M4 THE 80% LOADING-STALL FIX (2026-10-25, SHIPPED, pending device confirmation):** root cause found by static RE
  (gd-deps F12). GD loads assets with C++/CX PPL `task<byte>().then(lambda)`; the continuation captures the "UI thread" via
  _ContextCallback::_Capture -> CaptureUiThreadContext(&ctx) (GeometryJumpComponent sub_2ce94 / libcocos2d sub_8c628). The
  old shim returned E_NOTIMPL, so _Capture stored NULL and every continuation ran on the thread-pool thread that completed
  the antecedent -- cocos2d/GL completion work is render-thread-affine, so texture-load completions were lost and the counter
  froze at ~80% (the game kept drawing because the main thread never blocks). Fix: hook CaptureUiThreadContext (IAT patch in
  GeometryJumpComponent.dll + libcocos2d.dll) to return a real IContextCallback (g_ui_context) whose ContextCallback (slot 3)
  marshals the continuation onto the render thread, which drains a 64-slot queue twice per frame (disp_init/disp_drain,
  per-slot auto-reset events, inline if already on the render thread). Critical subtlety from the RE: _Capture passes a
  non-NULL ppContext but _IsCurrentOriginSTA (sub_2d8f8) passes NULL, and if the STA test returns S_OK, task::_Wait
  (sub_2d324) throws "Illegal to wait on a task in a Windows Runtime STA" -- so hook_CUTC returns S_OK+context ONLY for
  pp!=NULL and E_NOTIMPL for pp==NULL. Build clean (KERNEL32+USER32 only). gd_host.exe (24 KB) shipped standalone (drop into
  the existing dist-gd-host folder); dist-gd-host.zip (48.5 MB) refreshed. Diagnostics kept in this build: per-asset fopen
  trace + ctx capture/marshal logging, so if it still stalls the log localizes the remaining blocker.
- **M4 FIRST DEVICE RUN of the fix CRASHED in my own logger, not the fix (2026-10-25, E:\gd_host.log):** VEH fault
  0xc0000005 at gd_host.exe+0x2295 = lg()'s %s handler `ldrb r2,[r1]`. Cause: the tiny logger never supported the 'l'
  length modifier, so the fopen tracer's "[fopen #%ld] %s" made %ld consume nothing and %s deref the integer counter n
  (n=0 -> "(null)" safe; n=1 -> ldrb [0x1] AV). Crash preempts the fix -> hook_CUTC never ran (no "ctx #"/"CaptureUi" lines),
  so the fix was never tested. FIXED lg() to parse 'l' + width/'0' flags (also clears the cosmetic iid=08x).
- **M4 ADVERSARIAL REVIEW of the fix (5-agent Sonnet workflow, 2026-10-25):** verdict = the hook_CUTC/IContextCallback/
  dispatch CODE is ABI-correct + race-free (disassembly-verified), but the fix is LIKELY A NO-OP. gd-deps F12 UPDATE has
  the full argument; short version: RTTI task<E>=task<void> (create_async plumbing, not asset loading); _Capture only calls
  CaptureUiThreadContext when the antecedent's _M_fFromAsync byte is set (WinRT IAsyncAction), and GD's load tasks are not
  apartment-aware; the component's only PPL path (OnSuspending) is never called by the host; libcocos2d's addImageAsync
  loader has no .then and its StorageFile .then chain is dead code; GD's real background work is std::thread (a curl/
  CCHttpClient request-queue loop, entry sub_182ffc), NOT PPL. So the ~80% stall is most likely a std::thread worker or a
  main-thread poll on one, not a lost PPL continuation.
- **M4 DIAGNOSTIC DRAGNET build SHIPPED (2026-10-25, build stamp "Oct 25 2026 08:01:24", gd_host.exe md5 a1285aad...,
  dist-gd-host.zip refreshed + md5-verified):** keeps hook_CUTC (harmless; counters report if it ever fires) + adds: [tid]
  prefix on every log line (tell render thread 212 from worker/pool threads); GeometryJumpComponent.dll added to the
  fopen/RoGAF/ODS trace list (was the key untraced module); fopen cap 600->3000; counter summary at frame 300/900
  (fopen/cutc_cap/cutc_sta/ctx_calls); bounded-withdraw in ctx_ContextCallback (degrade to inline after 20s instead of a
  permanent hang); logger hardening (NUL-terminate OutputDebugStringA buffer, bounded TryEnter so a fault-while-holding
  can't silently hang). One run now settles the theory and localizes the real blocker.
- **M4 BIG REFRAME: the 80% "stall" is almost certainly NOT a stall (2026-10-25, 4-agent Sonnet static-RE workflow
  gd-loading-mechanism-map, full output in session tasks/w5r7m3akm.output).** GD's loading bar is a SYNCHRONOUS 14-step
  main-thread state machine (GeometryJumpComponent LoadingLayer::loadAssets sub_1057c8), each step paced by a self-re-arming
  CCSequence(CCDelayTime(0.01), CCCallFunc) ticked by CCScheduler inside Draw. It awaits NOTHING async -- no PPL, no
  std::thread, no curl, no file-completion feeds the counter (verified: no BL path from sub_1057c8 to task::_Wait /
  CaptureUiThreadContext / CCHttpClient::send / the thread launcher). Bar = 7%/step, max 91% (step 13 doesn't update it), so
  "~80%" = counter 11 (77%, next step = CCTextInputNode/FreeType TTF warm-up) or 12 (84%, next = CCCircleWave warm-up).
  libcocos2d addImageAsync is DEAD CODE for GD (no callers); GD loads every texture synchronously via CCTextureCache::addImage.
  KEY CLUE: menuLoop.mp3 is only PLAYED at step 13 (loading done -> GM::playMusic -> replaceScene(MenuLayer)) or in
  MenuLayer::init -- so if the user hears looping menu music, LOADING FINISHED and the scene switched; the frozen picture is
  the stale last loading frame. Leading hypothesis (3 of 4 agents, "likely"): the loader completes, replaceScene(MenuLayer)
  runs, but the first MENU frame kills the GL/device while gd_host keeps presenting the persistent portrait RT -> stale
  display. Candidate #2 (speculative but strong): D3D11 device removal / ANGLE context-loss at the first menu frame because
  ANGLE still believes FL 9_3 on a real 9_1 device (the long-deferred ANGLE 9_3->9_1 knob: GeometryJumpComponent file off
  0x357FC 03 26->01 26, or runtime-patch the FL at sub_36390 @0x363fc 'movs r6,#3'). m0probe proved the 18 used SHADERS
  create at 9_1, but a menu-only runtime path (format/feature) could still trip 9_1. The PPL fix (hook_CUTC) is confirmed a
  harmless no-op for this (kept; its counters will read cutc_cap=0/ctx_calls=0).
- **M4 DECISIVE PROBE build SHIPPED (2026-10-25, stamp "Oct 25 2026 09:00:09", gd_host.exe md5 00e85099...):** on top of the
  dragnet (tid prefix, component in fopen trace, counters) adds a per-60-frame health line: rt_checksum(portrait RT) with
  FROZEN/changing verdict + Present HRESULT (g_last_present_hr, now captured) + GetDeviceRemovedReason. Discriminates:
  (a) loader-finished-stale-display => fopen shows MENU assets after fonts/arial.ttf (GJ_logo_001/GJ_playBtn_001/...), rtsum
  FROZEN, removed!=0 or present!=0; (b) ticks-stopped => fopen stops at arial, rtsum may still change, no menu assets, no
  crash; (c) fault => !!! VEH/CRASH line. Next log pins it; then apply the matching fix (device-loss recovery and/or the FL
  9_3->9_1 knob -- per [[feedback_feature_level_auto]], as a runtime policy, NOT a permanent forced 9_1).
- **M4 ROOT CAUSE NAILED: it was NEVER a loading stall -- it was GPU device removal from an ANGLE feature-level mismatch
  (2026-10-25).** The 09:00:09 probe build's per-second health line read `present=0x887a0005 (DEVICE_REMOVED)` +
  `removed=0x887a0020 (DRIVER_INTERNAL_ERROR)` + `rtsum=0x0 FROZEN` from frame 60 on, while Draw kept returning 0x0 and the
  game logic loaded past the menu into gameplay assets -- i.e. the device dies early, ANGLE swallows it, and the host
  re-presents a dead frame (the "frozen loading screen"). `cutc_cap=0 ctx_calls=0` confirmed the PPL fix never fired (inert,
  as the review said). ANGLE debug.txt: `Image11::createStagingTexture failed` / `loadData: Could not map image` ×3 then
  `applyVertexBuffers: 0x887a0005` = driver died during a TEXTURE UPLOAD. Static RE: GeometryJumpComponent sub_36390 passes
  a feature-level index r6 (default `movs r6,#3`=FL9_3 @RVA 0x363fc) to libGLESv2_phone!CreateWinrtEglWindow (ordinal 181,
  @0x36496); our device is real 9_1, so ANGLE drove a 9_1 device as 9_3 -> DRIVER_INTERNAL_ERROR. (Texture size ruled out:
  all -hd sheets <=2048.)
- **M4 REAL FIX SHIPPED (2026-10-25, stamp "Oct 25 2026 11:37:17", gd_host.exe md5 f7d38cdc...):** runtime in-memory
  code_patch (verify-then-VirtualProtect-write-FlushInstructionCache; never edits the shipped DLL) that CLAMPS the requested
  ANGLE level to the device's actual max -- 9_1 device: 0x363fc 03->01 and 0x3641a 02->01 (r6->1); 9_2: r6->2; 9_3+: leave
  the game default. Per-device runtime policy (clamp to hardware), not a blanket forced 9_1, per the FL-auto feedback.
  CreateWinrtEglWindow is an ORDINAL import (181) so iat_patch can't hook it; the code_patch at the r6 source is the
  mechanism. Also added a per-frame device-removal pinpoint (first-removal frame + g_last_asset = last file opened) as a
  fallback if a different texture still trips it. Awaiting the device log: success = device alive, rtsum changing, real
  menu/gameplay; failure = the pinpoint names the triggering texture.
- M4 remaining WinRT shims (DisplayProperties, ApplicationData save, Launcher, Accelerometer, InputPane; gd-platform) on
  demand. M5 audio done (PhoneAudioSes). M6 package + Surface 2 then RT 8.1. THEN refresh HANDOFF_FOR_FRIEND.md (GD section)
  + the bundle to match the finished, playable state, and save memory.

## Rules (carry-over)
Never run ARM binaries on the x64 PC (build only; test on device). Game folder + *.appx/*.xap read-only, never in git/zip.
Do not force FL9_1 as a permanent default on capable GPUs (the ANGLE knob must be gated on the real device level).
