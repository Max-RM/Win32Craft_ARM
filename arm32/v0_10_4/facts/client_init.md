# client_init - MinecraftClient / App / AppPlatform init chain (MCPE 0.10.4.40, WP8.1 ARM32)

Exe: `MCPE.Host.WindowsPhone.exe` (ImageBase 0x400000, SizeOfImage 0xE6E000). All numbers are RVAs unless written `VA`
(VA = RVA + 0x400000; pointers stored in data are VAs with the Thumb bit set; the toolkit prints movw/movt targets as VAs).
Machine-readable twin: `client_init.json` (40 facts, expected bytes read from the exe). Scratch: `arm32/v0_10_4/analysis/`
(`client_vf7.txt` = annotated disassembly of `MinecraftClient::init`, `q.py/fieldcalls.py/...` = scan helpers).

## 0. Short answers

| question | answer |
|---|---|
| Who writes `client+0x118` (Textures*) and `+0x11C` (Font*)? | only `MinecraftClient::init` (vtable slot 7, **0x57B84**): Textures at `0x57F72..0x57F7E`, Font at `0x58234..0x5823C`. The ctor 0x54E90 zeroes both. |
| Who calls `init`? | `runInit` **0x471E8** (`client+0xC = holder; client->vtbl[7](); client+8 = 1; registerListener`). Its single caller is the init work item `lambda_c7b7::operator()` **0x1DA2D8**, a Windows ThreadPool job queued by `AppMain::createApp` **0x1D9DD4** (called from the AppMain ctor 0x1D9D14, called from the MainPage ctor 0x1DCF5C). After `runInit` the lambda sets `AppMain+0x3C = 1`. |
| In which state? | On a ThreadPool worker, right after the client/platform objects are created and **before** any XAML/swap-chain/size event. Needs: g_DeviceResources (device+context) alive, platform cell valid, LocalFolder path in `client+0x78/+0x90`, assets under `<installRoot>/data`. |
| Why did the friend get NULL `+0x118/+0x11C`? | `init` never ran: the friend skipped the AppMain init job and called `vtbl[32]` (0x58830) themselves. `StartMenuScreen` is created by the game INSIDE `init` (step 12 below), after Textures and Font exist. |
| When is `StartMenuScreen` created / rendered? | created at `0x5831A` (`ScreenChooser::setScreen(client+0xE8, 1)` = 0x91A3C -> 0x918D8 -> ctor 0x995EC -> `MinecraftClient::setScreen` 0x59638). Rendered by the first `Minecraft::update` after the ready flag: update -> vtbl[29] render -> `GameRenderer::render` 0xB9E1C -> `screen->vtbl[1](mouseX, mouseY, partial)`. |
| What drives frames? | NOT a timer tick: the ThreadPool render-loop body **0x1DA160** (started by `AppMain::StartRenderLoop` 0x1DA8A8, High priority, TimeSliced): `pump platform (0x50EAC); client->vtbl[19]() = Minecraft::update; DeviceResources::Present (0x1DBB10)` while `IAsyncInfo.Status == Started`. |
| What presents? | `DX::DeviceResources::Present` 0x1DBB10 (swap chain `Present(0,0)`); the client's end-of-frame platform hook (AppPlatform slot 25) is an empty stub in AppPlatform_Winrt. |
| Architecture (a) vs (b)? | Recommend the hybrid: do not run MainPage for the client side; replay AppMain (ctor 0x1D9D14 or the literal createApp steps) and call `runInit`, size, frame loop from the host. The game-side MainPage logic is NOT small (11 event registrations), but every handler only forwards to `AppMain`/`DeviceResources`, so the host replaces them by direct calls (section 5). |

## 1. Class chain and vtables

`AppPlatformListener -> App -> Minecraft -> MinecraftClient` (single inheritance on the primary vtable).

| class | vtable RVA | ctor | size |
|---|---|---|---|
| MinecraftClient | 0x33FFCC | 0x54E90 | 0x168 |
| Minecraft | 0xDC8704 | 0x1D7D44 | (base) |
| App | 0x33F824 | 0x46F40 (`this, argc, argv`) | (base) |
| AppPlatformListener | 0x33F940 | 0x47B14 | |
| AppPlatform / AppPlatform_Winrt | 0x33F890 / 0x33FB20 | 0x4726C / 0x494C0 | 0x88 |
| MCPE_Host::AppMain | 0xDC8830 | 0x1D9D14 | 0x40 |
| MCPE_Host::MainPage | 0xDC8AF8 | 0x1DCF5C | |
| Touch::StartMenuScreen | 0x34266C | 0x995EC | 0x388 |
| Gui / Font / LevelRenderer / SoundSystemRT | 0x340D60 / 0x340CE4 / 0x345118 / 0xDB8028 | 0x6C684 / 0x548A0 (factory) / 0xBD9D8 / 0x1062D8 | 0xC30 / 0x890 / 0x6A8 / 0xA2C(engine) |

### MinecraftClient vtable (cell = 0x33FFCC + 4*slot; value = 0x400000 + fn + 1)

| slot | fn RVA | role (verified from the body unless marked ?) |
|---|---|---|
| 0 | 0x55F78 | deleting dtor (full dtor 0x55838) |
| 1 | 0x58D70 | AppPlatformListener::onLowMemory (materials cache clear) |
| 2 | 0x58AF0 | onAppSuspended: `Textures::unloadAll(+0x118)` 0xDD6F0, materials release 0xC7D54, `+0x13C=0` |
| 3 | 0x58AA8 | onAppResumed: reload, `Textures::reload` 0xDD188, `+0x13C=1` |
| 4 | 0x58A50 | onAppFocusLost: if `!platform->isKeyboardVisible()` then `+0x13C=0`; `screen->vtbl[22]()` |
| 5 | 0x58A40 | onAppFocusGained: `+0x13C=1` |
| 7 | **0x57B84** | **App::init** |
| 8 | 0x47210 | `getPlatform()` = `**(this+0xC)` |
| 16 | 0x5A1D0 | useTouchscreen = `options[0x276] \|\| client[0x150]==0` |
| 19 | **0x1D8A54** | **Minecraft::update** (one loop iteration) |
| 20 | **0x59738** | **setSize(int w, int h, float guiScaleOverride /*s0*/)** |
| 21 / 22 | 0x47240 / 0x47260 | `quit()` sets `+9`; `wantToQuit()` returns `+9` |
| 24 | 0x575BC | handleBack(bool) |
| 26 / 27 | 0x593D0 / 0x595B4 | create-world / setLevel wrapper (Minecraft::vf27 0x1D8910 installs the world object into `+0x68`) |
| 28 | 0x59954 | preUpdate (`++(+0xF0)`, drain deque `+0xF8` via `screen->vtbl[9](0)`, `_Mtx_lock(+0x15C)`) |
| 29 | **0x5A114** | **render** (`_Mtx_unlock(+0x15C)`; GameRenderer; sound update) |
| 30 | 0x56BDC | postUpdate (`platform->slot25()` if `+0x13C`, then 0x11634C, world hook 0x592EC) |
| 31 | 0x59A8C | tick(i, n) (base `Minecraft::vf31` 0x1D8A00 + screen transitions) |
| 32 | 0x58830 | `(a, b)`: if `b`: `a ? 0x160-byte screen : StartMenuScreen`, then `Minecraft::vf32` 0x1D86A4 (leaveGame: destroys `+0x50/+0x68`, `+0xB0=0`) |
| 41 | 0x5A1F4 | vibrate(int ms) if `options[0x1E]` -> platform slot 29 |
| 6,9-15,17,18,23,25,33-40,42 | | not analysed (see section 9) |

## 2. Object layouts

### MinecraftClient (0x168)
```
+0x00 vtbl           +0x04 listener platform (=g_platform)   +0x08 byte initDone   +0x09 byte quit
+0x0C AppPlatform**  (cell; getPlatform = **(+0xC))          +0x10 vector<string> args
+0x20 int width px   +0x24 int height px  (ctor: 1,1)        +0x28 = 0x1267
+0x3C byte paused? (read by update/tick, writer not found)   +0x40 camera/game-mode-like object
+0x4C RakNetInstance* (new 0x448, ctor 0x101B78)             +0x50 LocalPlayer-like (deleted by leaveGame)
+0x60 int clientId   +0x64 User* (0x30, name string first)   +0x44/+0x48 shared_ptr (0x90 object)
+0x68 world object (friend: "Game"; unique_ptr, NULL in menus, set by vf27, destroyed by vf25/vf32)
+0x6C Screen*  +0x70 control block   (shared_ptr<Screen>, NO stack)   +0x74 byte (UI-grab state)
+0x78 std::string localFolder (size @+0x88, cap @+0x8C)     +0x90 std::string localFolder copy
+0xA8 Timer* (new 0x24: [0]=20.0f tps,[4] ticks,[8] partialTicks,[0xC]=1.0f)
+0xAC FilePathManager* (0x38, ctor 0x1A8288(this,&+0x78,&+0x90))   +0xB0 byte hasWorld   +0xB1 byte platform->slot21()
+0xB4 std::string gameDir = localFolder + "/games/com.mojang/minecraftpe" (size @+0xC4)
+0xCC frame ctr  +0xD1 byte pending reset  +0xD8/+0xE0 double delta/last time
+0xE8 ScreenChooser* (4-byte {client})  +0xF0 int frames  +0xF8..+0x104 std::deque (count @+0x104)
+0x108 LevelRenderer* (0x6A8)  +0x10C GameRenderer* (0x130, ctor 0xB86E4)  +0x110 obj (0xCC4, ctor 0xA4114 ?)  +0x114 obj (0x38, ctor 0x52B30 ?)
+0x118 Textures* (0xB8, ctor 0xDBF98)      +0x11C Font* (0x890)      +0x120 obj (0x10, ctor 0x9A3A0)
+0x124 InputHolder* (TouchInputHolder 0x340 | CustomInputHolder)   +0x128/+0x12C helpers cleared by vf32
+0x130 Gui* (0xC30)   +0x134 Options* (0x2D0, ctor 0x550DC)   +0x138 SoundEngine* (0xA2C, ctor 0xEFA00)
+0x13C byte renderEnabled (1; 0 while suspended)   +0x148 byte inSetScreen  +0x149 byte pending  +0x14C Screen* pending
+0x150 byte (= !platform->slot26())   +0x154/+0x158 float*[2] {guiScale, 1/guiScale}   +0x15C _Mtx_t (recursive)
+0x160 byte   +0x164 int keyboardHeight
```
Confirmed by >=2 accessors: +0x0C, +0x20/+0x24, +0x68, +0x6C/+0x70, +0x118, +0x11C, +0x13C, +0x164 (init, setScreen, setSize, GameRenderer, update/tick).
The rest have one user plus the ctor zeroing.

### AppMain (0x40, operator new + zero by MainPage ctor 0x1DD4C8)
`+0x00 vtbl(0xDC8830; slots 0,1 = empty OnDeviceLost/Restored stubs)  +0x04 DX::DeviceResources* +0x08 ctrl block  +0x0C IAsyncAction* (render loop)  +0x10 critical_section  +0x30 float 0.0  +0x34 MinecraftClient*  +0x38 AppPlatform** cell holder (new 8 bytes, cell[0] = platform)  +0x3C byte ready (atomic)`.
`+0x34..+0x3C` is a 12-byte sub-struct `{client, holder, ready}`; `createApp` runs on `this+0x34`, and the init lambda captures that pointer.

### AppPlatform_Winrt (0x88)
`+0x04 byte keyboardVisible (base)  +0x08 listener std::map  +0x10 MinecraftClient*  +0x14 std::string installRoot  +0x2C HIDController*  +0x30 int width (def 0x780)  +0x34 int height (def 0x438)  +0x38 float pixels/mm  +0x3C float scaleX  +0x40 float scaleY  +0x48/+0x4C u64 vibration deadline  +0x50 critical_section  +0x70.. input deque (+0x7C head, +0x80 count)`.

## 3. What the game really does (ordered)

A. `MainPage` ctor 0x1DCF5C (XAML path): InitializeComponent, `make_shared<DX::DeviceResources>` 0x1DCE2C (this runs `CreateDeviceResources` = D3D11CreateDevice and sets `g_DeviceResources`), `new AppMain` + ctor 0x1D9D14, event registrations (SizeChanged 0x1DF808, Showing/Hiding 0x1DF714/0x1DF700, BackPressed 0x1DF4E8, ...), 50 ms DispatcherTimer (see `frame_render.frame.timer_tick`).

B. AppMain ctor 0x1D9D14 -> `createApp(this+0x34)` **0x1D9DD4**:
 1. cell = `new(8)` (0x1D9C98); platform = `new(0x88)` zeroed + `AppPlatform_Winrt::ctor` 0x494C0 (WinRT: `Package.Current.InstalledLocation.Path` -> `+0x14`; HID object); `*cell = platform`.
 2. client = factory 0x1D9CC0 (`new(0x168)`, memset, ctor 0x54E90(client, 0, 0)) -> `this+0`.
 3. `platform+0x10 = client`.
 4. WinRT `ApplicationData.Current.LocalFolder.Path` -> narrow -> `client+0x78` and `client+0x90` (std::string::assign 0xD730).
 5. init job: `lambda_c7b7` (0x1D9B68, vtbl 0xDC8848) -> `ThreadPool.RunAsync` (0x1DA4A4).
 6. back in the AppMain ctor: `DeviceResources::setOwner(dr, AppMain)` 0x1DBBC0.

C. Worker thread: handler `Invoke` -> `0x1DA2D8` -> **`runInit(client, holder)` 0x471E8**: `client+0xC = holder`; `client->vtbl[7]()`; `client+8 = 1`; `registerListener(client, 0.0f)` 0x47BF4; then (lambda) `AppMain+0x3C = 1` with `dmb`.

D. **`MinecraftClient::init` 0x57B84** (all in `client_vf7.txt`):
 1. `createDirectories(localFolder, {"/games","/com.mojang","/minecraftpe"})` 0x1A90BC (`_access/_mkdir`); `client+0xB4 = gameDir`.
 2. atlas metadata `images/terrain.meta` -> global shared_ptr RVA 0xDDDBF8, `images/items.meta` -> 0xDDDC00.
 3. Options: `0xA0B78(options, client, gameDir)`, `0xA230C` (defaults call platform slot 40 and read DR+0x35).
 4. `Minecraft::init2` 0x1D84B0(client, &options+0x25C): `<gameDir>/clientId.txt`, User, **`this->vtbl[20](+0x20,+0x24,0.0f)`**, network-side objects, static item/tile registration.
 5. I18n `0xF3CB4(platform, "en_US")` (locale is hard-coded; file `lang/en_US.lang` via `platform->readAssetFile`).
 6. FilePathManager (+0xAC); **Textures `new(0xB8)` + `0xDBF98(tex, client+0x134, platform)` -> `+0x118`**; atlases 0..4; texture lists `startup.images`, `background.images`, `ingame.images` (0xDCD38).
 7. `MaterialManager::load(0xDE0AC0, "materials/common.json")` 0xCDFCC (D3DCompile of the ~45 shaders happens here/lazily); `0x563A8(client, options[0x1A])` -> `materials/fancy.json` or `materials/sad.json`.
 8. Gui (+0x130), ScreenChooser (+0xE8), LevelRenderer (+0x108), GameRenderer (+0x10C), obj +0x110, **Font `new(0x890)` via 0x548A0 with `"font/default8.png"` -> `+0x11C`**.
 9. gamepad query `0x1D83E0(client)->vtbl[2]`: no pad (or `options[0x290]==0`) -> **`ScreenChooser::setScreen(+0xE8, 1)` => StartMenuScreen**.
 10. SoundEngine (+0x138, XAudio2_8 via `SoundSystemRT`), `client+0x150 = !platform->slot26()`, InputHolder factory 0x56420 (+0x124).

E. First frame (UI thread side): `SizeChanged` handler -> `DeviceResources::SetSwapChainPanel` and `AppMain::OnWindowSizeChanged` 0x1DA32C / `AttachToPanel` 0x1DA634 -> **`client->vtbl[20](int((W+.5)*scaleX), int((H+.5)*scaleY), 0.0f)`** + `platform setScreenWidth/Height` (0x4FA40 -> +0x30, 0x4FA20 -> +0x34) + `AppPlatform_Winrt::attach` 0x4B55C (HID, DPI -> `+0x38/+0x3C/+0x40`); then `StartRenderLoop` 0x1DA8A8 once `IsAppInitialized` (0x1DA310) is true.

## 4. Per-frame path

```
loop body 0x1DA160   (while IAsyncInfo.Status == Started(0); scoped_lock AppMain+0x10; if AppMain+0x3C)
  AppPlatform_Winrt::pump 0x50EAC(platform)      ; drain input deque (0x4B300) until empty; cancel vibration if GetTickCount64 > +0x48
  client->vtbl[19](client)  = Minecraft::update 0x1D8A54
      vtbl[28] preUpdate                          ; ++frame, deque, _Mtx_lock(+0x15C)
      Timer::advanceTime 0x65F10(timer=*(+0xA8))  ; skipped (keeps partialTicks) if +0x3C && +0x68
      RakNet pump (+0x4C/+0x50)
      for i < timer.ticks: vtbl[31](i, ticks-1); ++(+0xCC)
      delta time (0x11B07C ms double) -> +0xD8, +0xE0
      vtbl[29] render 0x5A114                     ; _Mtx_unlock; GameRenderer::render(+0x10C, partial)
           GameRenderer::render 0xB9E1C           ; DR RT bind, world if +0x68, then client+0x6C -> vtbl[1](mouseX, mouseY, partial)
      0x1198B8 ; vtbl[30] postUpdate (platform slot 25 = stub)
  DX::DeviceResources::Present 0x1DBB10(AppMain+4) ; swapchain->Present(0,0), handles 0x887A0005/7 -> HandleDeviceLost
```
Never call `vtbl[28]` and `vtbl[29]` separately (mutex pair).

## 5. Host-callable steps (hybrid)

| # | step | function (RVA) / ABI |
|---|---|---|
| 0 | fake WinRT activation (`vccorlib120_app!GetActivationFactoryByPCWSTR`, thunk 0x2001E4) for ApplicationData, Package, ThreadPool, MemoryManager (+ Vibration/Network/CurrentAppSimulator on demand) | surface in `client_init.platform.winrt_surface` |
| 1 | create DeviceResources | `make_shared<DR>` 0x1DCE2C(&out) (see frame_render.*) -> `g_DeviceResources` (0xE2AFC4) |
| 2 | `AppMain` | `operator new(0x40)`+zero; `AppMain::AppMain` 0x1D9D14(this /*r0*/, shared_ptr<DR>* /*r1*/) |
| 2' | (no ThreadPool) literal createApp | cell=new(8); platform=new(0x88) zeroed + 0x494C0(platform); `*cell=platform`; client=new(0x168) zeroed + 0x54E90(client,0,0); `platform+0x10=client`; `client+0x78/+0x90 = path` |
| 3 | run init | `runInit` 0x471E8(client /*r0*/, AppPlatform** cell /*r1*/); then set ready (AppMain+0x3C or your own flag) |
| 4 | size | `client->vtbl[20](client, W, H, 0.0f /*s0*/)`; `platform+0x30=W, +0x34=H, +0x38=pixels/mm (~3.78), +0x3C=+0x40=1.0f` |
| 5 | swap chain | frame_render.swapchain.* |
| 6 | frame | `pump 0x50EAC(platform)`; `client->vtbl[19](client)`; `Present 0x1DBB10(dr)`; stop when `client->vtbl[22](client)` is true |
| 7 | input | `queueInputEvent` 0x4B290 or sinks (0x115F84, 0x116444, 0x4E568, 0x4E598) |
| 8 | lifecycle | focus lost: 0x473D4 then 0x474A4; resume: 0x4743C then 0x4736C (all `(AppPlatform_Winrt*)`) |

All functions are plain AAPCS-VFP calls; add the Thumb bit to the function address; verify `expected_bytes` from the json first.

KB/mouse hint: patch AppPlatform_Winrt slot 26 (cell 0x33FB88) to return 0 before init => `client+0x150 = 1` => `useTouchscreen()` false (unless `options[0x276]`) => `CustomInputHolder` + `KeyboardInput` instead of `TouchInputHolder`.

## 6. Platform requirements (AppPlatform_Winrt vtable 0x33FB20)

Slot / fn / meaning / WinRT dependency (full list with all 43 slots in the json):

| slot | fn | meaning | WinRT |
|---|---|---|---|
| 1 | 0x4EB90 | `getImagePath(out, name, inAssets)` -> `<installRoot>/data/images/` + name | none |
| 2 / 3 | 0x4F028 / 0x47818 | `loadPNG` (ifstream + libpng 1.5.13) / `loadTGA` | none |
| 14 | 0x4F780 | `readAssetFile(out, name)` = fopen(`<installRoot>/data/`+name) (base 0x4792C) | none (installRoot from Package) |
| 16 / 17 / 18 | 0x4ED00 / 0x4ECE0 / 0x4ECC0 | width, height, pixels-per-mm (`+0x30/+0x34/+0x38`) | none (values set by size handlers) |
| 19 | 0x4EF14 | isNetworkEnabled | NetworkInformation |
| 21 | 0x29BA0 | returns false -> `client+0xB1` | none |
| 25 | 0x0BF60 | end-of-frame hook, empty | none |
| 26 | 0x17FD40 | returns true (`client+0x150 = !slot26`) | none |
| 29 | 0x50FF0 | vibrate(ms) | VibrationDevice |
| 30 / 42 | 0x47690 / 0x47644 | getPlatformStringVar / device name (placeholders) | none |
| 31 / 32 / 33 / 34 | 0x4FAC4 / 0x4EDAC / 0x50F2C / 0x47740 | showKeyboard / hideKeyboard / updateTextBoxText / isKeyboardVisible | HID -> XAML TextBox + dispatcher |
| 40 | 0x4EAF8 | getFreeMemory (u64) = MemoryManager limit - usage; Options picks the memory tier from it (>0x1EA00000, >0xE600000) | MemoryManager |

Not found in this build: clipboard, locale query (hard-coded `en_US`), device-id (uses `clientId.txt`), launcher/URI.
Files: assets are plain files under `<installRoot>/data`; game data under `client+0xB4` through msvcr120 `fopen/_mkdir` (no WinRT storage API).

## 7. Window size / GUI scale / flags (host-readable)

| item | where |
|---|---|
| window size in client | `+0x20` width, `+0x24` height (physical px), written by setSize |
| guiScale (float) | RVA 0xDDE814 (1,2,3,4 or 6; override via `s0`) |
| invGuiScale (float) | RVA 0x340D40 = 1/guiScale (initial 1/3). Mouse and `Screen::init/setSize` use `int(px * inv)` |
| scale rule (s0 == 0) | w<400:1, >=400:2, >=800:3, >=1000: 4 (6 if pixels/mm > 15) |
| renderEnabled | `client+0x13C` (0 while suspended) |
| quit | `wantToQuit()` = `[client+9]` (vtbl[22]) |
| paused | `client+0x3C` (reader only) |
| keyboard height | `client+0x164` (set via 0x59578) |

## 8. Verdict on the friend's claims for this area

* "+0x118 Textures, +0x11C Font are NULL because normal init is not run" - CONFIRMED (cause: the init job `0x1DA2D8 -> 0x471E8` never executed).
* "`AppMain::tick` `sub_5DA5B4` updates the client" - WRONG: 0x1DA5B4 is `onKeyboardHeightChanged(px)` (InputPane handlers). The frame driver is 0x1DA160 which calls `client->vtbl[19]`.
* "AppMain+0x34 = MinecraftClient*, +0x3C = ready" - CONFIRMED.
* "client+0x6C/+0x70 shared_ptr<Screen>" - CONFIRMED; "+0x68 Game*" - it is the world object (set by vf27, deleted by vf32), identity (Level?) not proven.
* "StartMenuScreen via vtbl[32] sub_458830" - exists, but the game's own first screen is ScreenChooser id 1 inside `init`.
* "setSize sub_459738 -> vtbl[8] -> *(client+0xC)" - setSize only reads the platform through `vtbl[8]` for model/ppmm checks; it mainly sets `+0x20/+0x24` and the GUI scale globals.

## 9. Not found / uncertain

* writer of `client+0x3C` (pause flag); identity of `+0x68` (Level?), `+0x110`, `+0x114`, `+0x120/+0x128/+0x12C`, `+0x44/+0x48`.
* client vtable slots 6,9-15,17,18,23,25,33-40,42 (roles not decoded).
* item/tile registration calls at the end of `Minecraft::init2` (0x19B528, 0x1CC2C0, 0x15A800, 0x163384, 0x1BACA4).
* callers of `loadPNG/loadTGA` (platform slots 2/3).
* first start of the render loop (confirmed only the callers of `StartRenderLoop`: 0x1DF4C4, 0x1DF8A4, 0x1DF9AC, 0x1E017C; the 50 ms timer theory is in frame_render).
* device-loss: AppMain vtable slots 0/1 (OnDeviceLost/Restored) are empty stubs, so the game does not reload textures after `HandleDeviceLost`.
* gamepad branch at the end of `init` (element size 0x24, `options[0x290]`) not decoded.

## 10. Hazards for the port (cross-area)

* D3D feature level gate: `shader:getShaderModelSuffix` 0xD73FC throws for FL 9_1/9_2; the game only asks for {11_1,11_0,10_1,10_0,9_3}. See `frame_render.shader.*` before running `init` on a 9_1 device.
* TabTip: `hideKeyboard`/`showKeyboard` only reach the OS via HID/XAML TextBox; the game never re-checks the real keyboard. `platform+4` and `client+0x164` must be maintained by the host (see json `client_init.keyboard`).
* `AppMain` ready flag gates the render loop, so set it only after `runInit` returns.

## 11. Labels added (tools/arm32/names for this exe)
`MinecraftClient::ctor 0x54E90`, `Minecraft::ctor 0x1D7D44`, `App::ctor 0x46F40`, `App::getPlatform 0x47210`, `App::quit/wantToQuit 0x47240/0x47260`, client listener callbacks `0x58A40..0x58D70`, `preUpdate 0x59954`, `postUpdate 0x56BDC`, `tick 0x59A8C`, `Minecraft::leaveGame/setGame 0x1D86A4/0x1D8910`, `ScreenChooser::setScreen/createScreen 0x91A3C/0x918D8`, `Touch::StartMenuScreen::ctor 0x995EC`, `Screen::init/setSize 0x90624/0x915E8`, `Textures::ctor 0xDBF98`, factories `0x548A0/0x54AC0/0x549A8/0x5495C/0x54B78`.
