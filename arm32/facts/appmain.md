# appmain (ARM32, Minecraft 1.2.8) - AppMain construction, activation, frame loop

Machine-readable details: `appmain.json` (34 facts, 31 high / 3 medium). RVAs are even code addresses (OR in 1 for a Thumb call).

## Call plan the host makes into the game
| step | x86 | ARM32 |
|---|---|---|
| create AppMain | `RVA_128_CREATE_APP_MAIN` 0x7f5f80, ECX=result EDX=ctx +1 stack arg | **0x00597b4c**, `r0=AppMain** result, r1=StartupCtx*, r2=CoreWindow** holder`, no stack args, returns r0=result |
| visibility | `0x7d07a0(AppMain*, BYTE)` | no function: inlined in `AppView::OnVisibilityChanged` 0x5956cc. Do `p=*(AppMain+0x1c); vt[27](p); vt[21](p,1)` (0x583cf8, 0x621620) |
| size | platform+0x330/+0x334 then vtbl[0x5c] (w,h,0) then vtbl[0x60] (w,h) | same offsets/slots (23, 24): `vt[23](game,w,h,0.0f /*s0*/)` = `MinecraftGame::setSize` 0x12f860; `vt[24](game,w,h)` = `windowSizeChanged` 0x12f8f0 |
| frame | `0x7cf360(AppMain*)` | **0x0057d3a8** `unsigned f(AppMain*)`, returns quit flag (`MinecraftGame+9`) |

## Layouts
* AppMain (size **0x28**): +0 vtable 0x13694a8, +4 restart flag, +8 StartupCtx*, +0xc Agile<CoreWindow> (16 B), **+0x1c AppPlatform_Winrt\*** (0x428 B), **+0x20 MinecraftGame\*** (0x3c0 B), +0x24 1-byte helper. No frame-enable byte (do not write +0x10, it is Agile storage).
* AppPlatform_Winrt: **+0x330 width, +0x334 height** (int; getters vf82/vf83 at 0x582660/0x582680), +0x33c/+0x340 DPI scale floats (LogicalDpi/96), +0x3b0 state, **+0x3b4 InputAdapter\*** (size 0x170; +4 HIDController\*), +9 visible byte.
* StartupCtx (0x20 B): +0 int mode, +4 byte, +8 std::string (size +0x18, cap +0x1c; set cap=0xF). AppMain keeps the pointer: keep it static.
* MinecraftGame: primary vtable 0x133bacc has 29 slots (0x74 B) so the x86 `IsBadReadPtr(vtable,0x64)` still holds; quit flag at +9.

## IsBadReadPtr sizes
* 0x24 on AppMain: ARM sizeof is 0x28; probe still fine (only +0x1c/+0x20 are read).
* 0x64 on vtable: still fine (vtable is 0x74 bytes).
* 0x1d0 on `controller+0x10` (the InputAdapter, enqueue_input target): ARM object is **0x170** bytes; change the probe to 0x170 (offset +0x10 unchanged).

## What the game does with the fake CoreWindow (activation argument)
It is stored in `Platform::Agile<CoreWindow>` (QI for IAgileObject must succeed, the default fake QI already does), forwarded to AppPlatform_Winrt/InputAdapter/HIDController and kept in a global Agile at 0x1576fcc. Vtable slots actually called (same ICoreWindow numbering as the x86 table): 0/1/2, 7 get_Bounds, 9 get_Dispatcher, 15/22/23 cursor and pointer capture, 16 get_PointerPosition, **20 GetAsyncKeyState polled every frame** (VK 1, 2, 0x12, 0x20), 28/34/36/44/46/48/52 add_* event registrations. Not called on this path: 18 Activate, 24, 54, 56 (AppView only). No vtable renumbering is needed.

## Notes / risks
* `AppMain::init` (inside the factory) already sizes the game via `AppMain::onWindowSizeChanged` (0x57df28) using fake Bounds x DisplayInformation LogicalDpi; provide LogicalDpi=96. The host's later explicit resize overrides it, as on x86.
* `AppMain::vf1` (0x57d224) restarts the game (recreates MinecraftGame); always re-read AppMain+0x20, never cache.
* Visibility mapping is the only medium-confidence item (derived from inlined ARM code, x86 body not available).
