# HUD / touch controls missing in the world (first device run, Surface 2, Win10 ARM32) - investigation

Date 2026-10-18. Exe analysed: `MCPE.Host.WindowsPhone.exe` (stock 0.10.4.40, RVAs = IDA VA - 0x400000), tool `arm32re.py`. Log: `arm32/device_logs/win32craft_0104_surface2_win10_first_run.log` (run 1 = lines 1-560).
New device evidence (coordinator): `--half-res` (resolution float NOT patched) shows the same missing HUD, so the resolution override (hypothesis 4, first half) is ruled out.

## 0. Verdict (read this first)

| | |
|---|---|
| Root cause | **UNKNOWN - not provable offline.** Every decision point of the game that I could find in the binary is consistent with "the HUD and the touch controls are created and drawn"; none of them depends on a value the host fakes in a way that would hide them. |
| What is certain | The exact chain that creates / draws the HUD and the touch buttons, every early-return condition of it, and that the hotbar and the touch buttons need the same three things: **level + player present, `options.hideGui == 0`, no `Screen` set** (section 2). The crosshair is NOT expected in touch mode (section 2.4). |
| What is probable | The game logic is fine and the draw calls are either not issued (a `Screen` is set / the player is not "loaded") or issued with a Direct3D state that makes them invisible (stale state from the world pass). |
| What I did | Implemented `--trace-game-state` (new, off by default): game-state snapshots (app unit) + a Direct3D draw / state tracer (panel unit). One device run with it should decide between all remaining candidates (section 5.2). Nothing the game does was changed. |
| Do first (no new build) | Send `options.txt` from the device (`%APPDATA%\MinecraftPE\0.10.4\games\com.mojang\minecraftpe\options.txt`): `gfx_hidegui:true` or `ctrl_usetouchjoypad:true` there would explain everything cheaply. |

## 1. Hypotheses of the brief, one by one

| # | hypothesis | status | evidence |
|---|---|---|---|
| 1a | `gfx_hidegui` (Options +0x244) hides the HUD | **cannot be excluded without the log / options.txt; no host influence** | Default 0 (Options defaults function `0xA0BD0`, store at `0xA0C1E`). Read from options.txt by `Options::init` `0xA230C` (`0xA2760`), toggled only from the settings screen (`0xA20DC`). Checked by `GameRenderer::render` at `0xBA0D4` and by `TouchscreenInput` at `0x9AD1E`. The host never writes options.txt. The new `gstate options` line prints `hideGui(+0x244)`. |
| 1b | `useTouchscreen` / `TouchInputHolder` vs `CustomInputHolder` | **refuted** | `useTouchscreen` (`0x5A1D0`) = `options[0x276] \|\| client[0x150]==0`. The platform slot-26 cell `0x33FB88` is left alone by the host = "return 1" -> `client[0x150]=0` -> true. Factory `0x56420` (`0x5643C`) creates `TouchInputHolder` (0x340 bytes, vtable `0x340140`, ctor `0x551DC`, which already applies the layout through `TouchInputHolder::vf1 0x58B90` at `0x5527A`). Options default of `+0x276` is 1 when `client[0x150]==0` (`0xA0D0A`). |
| 1c | `ctrl_usetouchjoypad` (+0x275) | default 0; only matters for the crosshair and a small debug rectangle | `TouchInputHolder::vf2` (`0x59170` -> `0x59184`) draws only a rectangle and only when +0x275 != 0. The real touch buttons are drawn by `TouchscreenInput::vf2` (`0x9B14C`), see 2.3. |
| 1d | `Gui::render` early returns | **checked, none depends on the host** | `0x6E2E8-0x6E2FC`: `client+0x68` (level) and `client+0x12C` (player, `vf38/vf42 0x56F40`) must be non-null. Both are set by `MinecraftClient::createLevel 0x593D0` (player `0x594FA`). |
| 2 | HUD drawn off screen / zero size by a wrong scale | **unlikely** | The menu is laid out with the same `setSize` (`0x59738`), the same globals (guiScale `0xDDE814`, 1/guiScale `0x340D40`) and the same host values (RenderSize 1280x720, scale 1.0, raw dpi 96 -> ppmm 3.779, `AppPlatform_Winrt::initInput 0x4B55C`, `AttachToPanel 0x1DA634`, `OnWindowSizeChanged 0x1DA32C`). The Gui (`Gui+0xA4 -> 0x6DE9C`) and the touch holder (`0x58B90`) are re-laid out by that same `setSize`. (The first SizeChanged happens before `initInput`, so platform scale fields are 0 and `setSize(0,0)` gives 240x140 until the ready tick - the same on a phone and re-done at AttachToPanel.) `gstate` prints all of it, incl. the touch button rectangles. |
| 3 | HUD textures / materials missing | **unlikely** | `gui/gui.png` (hotbar, touch buttons), `gui/icons.png`, `gui/touchgui*.png` are in `startup.images` (preloaded with the menu textures), exist in the package, are plain 8-bit RGBA PNG (the same format as the menu textures). All `ui_*` materials are in `ui.material` (loaded by `common.json`) and use the shaders the menu already uses (`uv.vertex`/`texture.fragment`, `position.vertex`/`current_color.fragment`, `color_uv`...). No `FAILED` / `X1507` line in the log. Not provable without pixels. |
| 4 | invisible because of render state | **OPEN - the main remaining candidate** | The UI materials are `DisableDepthTest` + `Blending` (`ui.material`), so depth is not the issue; what the world pass leaves in scissor / rasteriser / blend write-mask / render target is unknown. `--half-res` excludes the resolution override. The new D3D tracer answers it (section 5). |
| 5 | controls built only after first input | **refuted** | The touch input object is attached to the player by `MinecraftClient::vf40` (`0x58D8C`, `str r0,[player+0xD8C]` at `0x58DB4`) when `LocalPlayer::vf149` (`0xAA160`) turns the player's load state `+0xDB4` from 1 to 3 (section 3). Nothing there looks at input. |

## 2. What decides whether the HUD / touch controls are drawn (all RVAs checked in the disassembly)

### 2.1 Frame path
`Minecraft::update 0x1D8A54` -> `MinecraftClient::render 0x5A114` (needs `client+0x13C` renderEnabled) -> `GameRenderer::render 0xB9E1C`:
1. `0xB9E54/0xB9E5C`: `DR::SetViewportSize(client+0x20, client+0x24)` + `DR::BindRenderTarget`, clear colour + depth/stencil (`0xCE460 -> 0xCE344`: ClearDepthStencilView flags 3).
2. `0xBA094-0xBA0A6`: no level (`client+0x68`) -> menu path `0xBA108`. With a `Screen` set whose vf15 (`+0x3C`) returns 0 (ProgressScreen, ChestScreen, FurnaceScreen) the level is **not** rendered (`0xBA2BA`).
3. `0xBA0C8`: `renderLevel 0xBA51C` (viewport + render target again, sky, chunks, hand).
4. `0xBA0D4`: `options[0x244]` (hideGui). If 0: `setupGuiScreen 0xBAB1C` (ortho from `client+0x20/0x24 * 1/guiScale`) and `Gui::render 0x6E2C4(gui=client+0x130, a, hasScreen=(client+0x6C!=0), mouseX, mouseY)` at `0xBA0FE`.
5. `0xBA2C2-0xBA314` (only if `client+0x12C` player and **no Screen**): `client+0x124 (InputHolder)->vf2` and **`player+0xD8C -> vf2`** (the touch controls).
6. `0xBA316`: current Screen -> `vf1` (render).

### 2.2 Hotbar and the rest of the HUD (`Gui::render 0x6E2C4`)
Needs level `+0x68`, player `+0x12C`. Hearts / armour only in survival (`client+0x40 -> vf11`, `0x6E42C`). The hotbar `Gui::renderToolBar 0x6F410` is called at `0x6E4E4` **only when `client+0x6C == NULL`** (`0x6E4CA`). Slot count: `Gui+0xB0C` (ctor 4; `Gui::onConfigChanged 0x6DE9C` sets 6 for touch, more on wide screens, 9 without touch).

### 2.3 Touch buttons (d-pad, jump, pause, chat...)
`TouchscreenInput::vf2 0x9B14C` (binds `gui/gui.png`) -> `0x9ACFC`; guard `options[0x244]` (`0x9AD1E`); draws the button areas stored in the TouchscreenInput sub-object (`TouchInputHolder+0x10`, areas at +0xF4..+0x11C). The object reaches the game only through `player+0xD8C` (see 3).

### 2.4 Crosshair is NOT a symptom
`sub_6EE30` (called from `0x6E408` only without a Screen): with `useTouchscreen` and `ctrl_usetouchjoypad == 0` (the defaults) no crosshair is drawn at all (`0x6EEBA-0x6EECC`); only the bow indicator. So "no crosshair" is correct for the default touch setup; "no hotbar / no buttons" is the real symptom.

## 3. Finding: the controls are bound to the player only when the world is "loaded"
* `LocalPlayer` ctor `0xA7D00`: `player+0xD8C = 0` (`0xA7D54`), `player+0xDB4 = 1` (`0xA7D66`), `player+0xDB8 = client`.
* `Level::tick (vtable 13)` -> for each player `vtable[149]` = `LocalPlayer::vf149 0xAA160` (call site `0x17CA12`, argument = tick stamp `level+0xBC4`). It calls `Player::vf149 0x14943C`, which walks 57 chunk offsets (table at RVA `0xDC0788`) around the player and returns a counter (+1 per existing chunk with state byte `chunk+0x54 >= 2`, +1 more for `>= 4`). While `player+0xDB4 == 1` it stores `counter/..` as progress (`+0xE0C`) and, when **counter > 0x68 (104)** (`0xAA184`), calls `client->vf40(player)` = `MinecraftClient::vf40 0x58D8C` (which sets `player+0xD8C = inputHolder->vf5()` = the TouchscreenInput, binds the Gui `0x6DF98`, informs the LevelRenderer) and sets `player+0xDB4 = 3` (`0xAA1A6`).
* After "Create world" the game puts up a `ProgressScreen` (`0x78C6E`, class vtable `0x341CDC`, vf15 = false) whose tick (`0x8F93C`) closes it (`setScreen(NULL)`) when `player+0xDB4 == 3` (`0x8F97A`), or shows "Could not connect to server" for 4.
* State 3 is also required by `LevelRenderer` chunk rendering (`sub_C4660`, `0xC468A`), `GameRenderer::tick` (`0xBAD12`) and the player's movement (`LocalPlayer::vf13 0xA8E84`).
* Consequence: if the terrain is really visible, the player is in state 3 and the touch input object IS attached, the ProgressScreen is gone, and the game wants to draw the HUD. If the player were still in state 1 the user would see the ProgressScreen (dirt, green bar) and no terrain. The report "terrain + held block visible, nothing else" therefore points to the Direct3D side (4) or to a Screen with `renderGameBehind == true` that draws nothing (ChatScreen, PauseScreen, OptionsScreen, ControllerOptionsScreen, DeathScreen, InBedScreen - all draw visible controls, so unlikely).

## 4. What the host feeds (checked against the log)
`DisplayInformation` landscape / 96 dpi (RawDpiX/Y 96 -> `platform+0x38` 3.779), CompositionScale 1.0 -> `platform+0x3C/+0x40`, panel RenderSize = client size, `useTouchscreen` cell untouched, MemoryManager tier BEST. `Options::init` runs on the init thread BEFORE `initInput`, so `gfx_pixeldensity` (`+0x28C`) is the clamp minimum 3.0 on the phone too (not host related). None of these can switch the HUD off; all are printed by the new `gstate` lines.

## 5. The diagnostic (`--trace-game-state`) and how to use it
Run once: `start_gamestate.bat` (added to `tools/make_0104_package.py`; it appears in the next package) or, with the current package, a copy of `start.bat` that ends in `--trace-game-state`, i.e. `Win32Craft.0.10.4-arm32.exe --trace-game-state`. Play -> create world -> wait in the world >= 60 s (touch around, try the bottom-left / bottom-right corners and the top centre) -> close. Send `win32craft.log`.

### 5.1 Log lines (all in `win32craft.log`; `m` = thousandths; `00000000 (null)`; `FFFFFFFF` / `-1` / `(unreadable)` = unreadable)
App unit (UI thread, read-only, every read behind `IsBadReadPtr`; 1 line set per snapshot):
```
X104 gstate: --trace-game-state active: the game client exists (...)          once
X104 gstate #N [periodic|change] t=S.mmm ready=1 client=.. init=1 renderEnabled=1 paused=0 grabbed=0 size=WxH platformSlot26Flag(+0x150)=0 frames(+0xF0)=.. kbdH=.. timer: tps=20000m ticks=.. partial=..m
X104 gstate  screen(+0x6C)=PTR <RTTI class> | pending(+0x149)=.->PTR inSetScreen(+0x148)=. | level(+0x68)=PTR gameMode(+0x40)=PTR <class> | inputHolder(+0x124)=PTR <class> | gui(+0x130)=PTR <class> slots(+0xB0C)=N guiOwner(+0xAC4)=client|OTHER
X104 gstate  options=PTR hideGui(+0x244)=. useTouchscreen(+0x276)=. useTouchJoypad(+0x275)=. leftHanded(+0x1D)=. fancyGraphics(+0x1A)=. vibrate(+0x1E)=. animTex(+0x278)=. fancySkies(+0x277)=. serverVisible(+0x274)=. pixelDensity(+0x28C)=..m difficulty(+0x240)=. | guiScale=..m invGuiScale=..m
X104 gstate  platform=PTR width(+0x30)=.. height(+0x34)=.. ppmm(+0x38)=..m scaleX(+0x3C)=..m scaleY(+0x40)=..m | DR=PTR viewport(+0x18)=x,y w x h z=a..b rtSize(+0x3C) outSize(+0x44) logical(+0x4C) ovr(+0x34) prof(+0x35) dpi(+0x5C) comp(+0x60) rtv dsv
X104 gstate  player(+0x12C)=PTR <class> loadState(+0xDB4)=N (1 loading, 3 ready) progress(+0xE0C)=..m input(+0xD8C)=PTR <class> health(+0x138)=. clientId(+0xCC8)=. pos(+0x10)=x,y,z mc(+0xDB8)=client|OTHER
X104 gstate  touch: TouchscreenInput=PTR flags: hidden(+0x11) f(+0xDD) f(+0xDE) f(+0xDF) options(+0xD8) mc(+0xF0) | areas(+0xF4..+0x11C; x0,y0,x1,y1 in thousandths) PTR:x0,y0,x1,y1 ... (11 entries)
```
Schedule: 1 s for the first 60 s after the client exists, then 10 s; plus a `[change]` snapshot (<= 80, >= 150 ms apart) whenever screen / player / loadState / input object / holder / hideGui / client size / level changes (so "Create" and "world appears" are always captured).
Panel unit (render thread at Present; wraps 13 cells of the `ID3D11DeviceContext` vtable - PSSetShaderResources, PSSetShader, DrawIndexed, Draw, Map, OMSetRenderTargets, OMSetBlendState, OMSetDepthStencilState, RSSetState, RSSetViewports, RSSetScissorRects, ClearRenderTargetView, ClearDepthStencilView - and forwards every call unchanged):
```
X104 d3dtrace: ID3D11DeviceContext vtable PTR (...): 13 of 13 cells wrapped          at device creation (a second line for the ID3D11DeviceContext2 vtable when it differs)
X104 d3dtrace frame #N [periodic|U-appeared|U-vanished] t=.. draws=D indexed=I W=.. B=.. U=.. u=.. clearRT=.. clearDS=.. sets: rtv=.. vp=.. blend=.. depth=.. raster=.. scissor=.. maps=M (discard .., no-overwrite .., FAILED ..)
X104 d3dtrace   passes: C1 D1 W120 B8 W3 U52          call-order run-length list. C = ClearRenderTargetView, D = ClearDepthStencilView, draws: W = depth on/no blend, B = depth on + blend, U = depth OFF + blend (= the game's ui_* materials), u = depth off/no blend
X104 d3dtrace   end state: rtv=PTR (game DR.rtv=.. same|DIFFERENT) dsv=PTR (...) viewport=WxH scissor=WxH blend=. depth=. cull=. scissorOn=.
X104 d3dtrace   draw[-k] I|V CLASS count=.. start=.. vp=WxH scissor=WxH(on=.) blend=.(src.. dst.. mask..) depth=.(write.. func.. stencil.) cull=. ps=PTR tex0=PTR rtv=PTR dsv=PTR     last 8 draws of the frame
```
Schedule: 1 s for 90 s, then 10 s; plus one extra frame record when "this frame has U draws" flips (<= 60). `X104 d3dtrace: Map FAILED hr=... resource=.. subresource=.. type=.. flags=..` (<= 40) reports every failed `ID3D11DeviceContext::Map` (constant / vertex buffer updates; the game ignores the result, a failed update leaves stale matrices / colours in the buffer).

Always on (no option, like the existing CreateBuffer / CreateTexture2D wrappers): the panel unit now also logs every FAILED result of `ID3D11Device::CreateShaderResourceView / CreateRenderTargetView / CreateDepthStencilView / CreateInputLayout / CreateVertexShader / CreatePixelShader / CreateBlendState / CreateDepthStencilState / CreateRasterizerState / CreateSamplerState` (the game ignores these HRESULTs and runs at 9_1 here instead of the 9_3 a phone gives it), at most 20 lines per kind, with the descriptor fields: `X104 panel: CreateRasterizerState FAILED hr=0x80070057 E_INVALIDARG fill=3 cull=2 ... depthClip=0 ...`. A state that never came into existence is a candidate for "drawn but invisible" that this run's log could not show before; install line: `X104 panel: d3d11 device vtable PTR (ID3D11Device): 10 of 10 Create* failure-logging wrappers installed`.

### 5.2 Decision table for the next log (compare a MENU frame with a WORLD frame)
| what the log shows in the world | meaning | next step |
|---|---|---|
| `hideGui(+0x244)=1` | options.txt / toggle | delete options.txt in the data root; if it comes back, find what writes it |
| `screen(+0x6C)=... ProgressScreen`, `loadState=1` for a long time | the player never becomes "loaded": the HUD is simply not created yet (then the terrain must not be visible - check the screenshot) | the chunk-ready counter (`Player::vf149 0x14943C`) never exceeds 104: look at chunk generation threads (the game uses `std::thread` `_Thrd_create` and PPL tasks on msvcr120_app's ConcRT scheduler; the ThreadPool/ThreadPoolTimer activations come from msvcr120_app and go to the REAL runtime) |
| `screen(+0x6C)=` another class (Chat/Pause/Death/...) | a Screen is open | identify who opened it |
| `screen=(null)`, `loadState=3`, `input(+0xD8C)=TouchscreenInput`, hideGui 0, **and `U>0` every world frame** | the game DRAWS the HUD; Direct3D shows nothing | read `end state` and the `draw[-k]` lines: `rtv DIFFERENT` (drawn into another target), `scissorOn=1` with a small rect, `blend ... mask0` (colour write off), viewport not full size, `dsv`/`rtv` unbound, texture pointer `tex0=00000000`. A stale state is then fixed in the panel unit by wrapping the same cell (the tracer already shows the pattern). |
| `screen=(null)`, loadState=3, input attached, **no U draws** in world frames but U in menu frames | the game does not issue the draws although it should | check the `touch:` areas (null / degenerate rectangles = layout bug), `gui slots`, `gameMode` class; then a deeper look at `Gui::render 0x6E2C4` branch by branch |
| `screen=(null)`, `loadState=3`, `input(+0xD8C)=00000000` | `MinecraftClient::vf40` did not attach the input | trace `0x58D8C`: `player->vf169 (0x2A4)` must be true (LocalPlayer returns 1) |
| no `d3dtrace` lines at all | context vtable not patched | the `d3dtrace:` install lines say why (vtable unreadable / cells could not be patched) |
| `X104 panel: Create...State/...View FAILED` lines, or `Map FAILED` | a state / view / buffer update the game ignored | that object is missing or stale in the HUD draws: fix by making the descriptor valid for 9_1 (host wrapper may sanitise the descriptor) or by reporting the feature level 9_3 only if the GPU really supports it (`start_fl93.bat`) |

## 6. Files changed (all ARM-only; x86 output byte-identical)
* `win32_host_arm32_0104_app.inl` - new section 8b (`x104_app_gs_*`): option parse (own copy of the command line scan, so core/boot stay untouched), RTTI class-name reader (vtable[-1] -> locator +12 -> descriptor +8; verified against the exe: the pointers are relocated), snapshot + schedule + change detection, call from `x104_app_ui_tick`. `x104_app_install` logs `--trace-game-state=0|1`.
* `win32_host_arm32_0104_panel.inl` - new section 2b (`x104_panel_tr_*`): 13 context-vtable wrappers, per-frame record, log; new section 2c (`x104_panel_cf_*`): the always-on Create* failure wrappers (10 `ID3D11Device` cells, installed from `x104_panel_post_create` for ID3D11Device and ID3D11Device2); hooks: `x104_panel_present_stub` (frame end), `x104_panel_post_create` (+`context` field in `x104_panel_created_t`, set from the D3D11CreateDevice out parameter), `x104_panel_install` (option parse + log). Disabled = no cell is patched and the only cost is one `if` in the Present stub.
* `tools/make_0104_package.py`: one launcher entry `start_gamestate.bat` (package NOT built). `HOST_DESIGN.md` option list and `INTEGRATION_NOTES.md` (one bullet) mention the option.
* Why no core / boot edit: the option is parsed by the two units themselves (`host_next_argument`, `host_argument_equals` of win32_host.c).
* Tests: `tools/arm32/test_0104_app.c` (+ `host_*` argument stand-ins, `test_game_state_tracer`: fake game graph with fake RTTI, 177 checks, garbage / unreadable pointers, line length < 1000) and the new `tools/arm32/test_0104_panel_tr.c` / `.py` (106 checks: fake `ID3D11DeviceContext` vtable + state objects, forwarding of every argument, W/B/U/u classification, run list, GetDesc fault, Map counting / failure, frame log, U flip; fake `ID3D11Device` for the 10 Create* wrappers: silent on success, one log line with the descriptor fields on failure, unreadable descriptor). All 27 wrapped vtable slot numbers were checked against the SDK header `d3d11.h` (script in the session, 0 mismatches) and the descriptor layouts against the SDK structs.

## 7. Verified on the PC
`python tools\arm32\try_0104.py` (tree), `python tools\arm32\try_0104.py --selftest --integration` (130 + 148 checks, 43 fake selftest checks), `--unit app`, `--unit panel`, `cmd /c build_arm32.bat` (dist-arm32\win32craft.dll), `python tools\arm32\check_x86.py` (win32_host.c and api_compat.c x86 IDENTICAL), `python tools\arm32\test_0104_app.py`, `python tools\arm32\test_0104_panel_tr.py`. Not verifiable offline: the real d3d11 vtable layout on the Surface 2 (cells wrapped are checked at run time, `12 of 12 cells wrapped`), and the ARM passing of the `float depth` argument of `ClearDepthStencilView` (plain C prototype, compiled by the same ARM compiler that builds the game calls' counterpart).

## 7. RESOLUTION (2026-10-18, confirmed on the Surface 2: the HUD shows)
Root cause chain found with the device logs (arm32/device_logs/win32craft_0104_gamestate_run3..7*.log):
1. The Direct3D runtime rewrites the draw/clear cells of its heap context vtable (DrawIndexed 12, Draw 13, DrawIndexedInstanced 20, DrawInstanced 21, ClearRenderTargetView 50, ClearDepthStencilView 53) back to its own functions; the tracer therefore re-puts its wrappers after every state call (`x104_panel_tr_heal`) - only then the draws became visible to us.
2. World frames: `D1 C1 D1 C1 W.. B3 W.. U1 D1 W1 U12` - the game DOES issue the HUD (12 UI draws after the 3D pass) with the same render state as the visible menu draws; the HUD textures (touchgui.png, terrain atlas) are valid with alpha; the back-buffer dump had no HUD in it.
3. The last UI draw: valid 20-byte-per-vertex data (x,y in GUI pixels, z 0, packed colour/uv), correct GUI ortho matrix (0.00625, -0.0111, -1, 1) in VS constant buffer slot 3, correct indices - but the vertex buffer was bound with STRIDE 0 (`IASetVertexBuffers pStrides[0] == 0`). Stride 0 = every vertex reads element 0 -> degenerate quads -> nothing is drawn. All TEXTURED dynamic-VB HUD draws had stride 0; static world meshes (24/20/12) and the colour-only UI draw (16) were fine.
4. FIX (panel unit, `x104_panel_tr_fix_stride`, ON by default, `--no-vb-stride-fix` disables): `CreateInputLayout` records the stride each layout needs (max offset+size of slot 0, `x104_panel_ls_*`); right before every draw, if slot 0 is bound with stride 0 and the bound layout needs N bytes, the buffer is re-bound with stride N (log: `X104 panel: vertex-buffer STRIDE FIX #n`). The HUD appears.
5. NOT known: why the game passes stride 0 for those buffers (suspects: a vertex-format/feature-level dependent size the host influences - we report FL 9_3 on a 9_1 device - or a game-side bug that the phone runtime tolerates). `--trace-game-state` now logs the game callers (`callers(rva): vb= ib= draw=`) of every draw and a `STRIDE 0 from game rva` line; analysing those functions could give the real root cause, the fixer is a safe workaround meanwhile (a non-zero stride from the game is never touched).
Diagnostics kept (all behind `--trace-game-state` / start_gamestate.bat): game-state snapshots, draw tracer with cell healing, frame/texture BMP dumps, buffer/constant dumps, input layout list, always-on failed-Create* logging.
