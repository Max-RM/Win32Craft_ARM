# touch_game (ARM32, Minecraft 1.2.8) - why the touch HUD never appears and how to get it

Machine-readable: `touch_game.json` (28 facts). Re-check with `python arm32/facts/touch_game_verify.py`.
All RVAs are exe RVAs; code pointers stored in data carry the Thumb bit (+1). Nothing here was run on a device.

## Answer in one paragraph
There is no setting and no "mouse/keyboard attached" detection. The game starts in the input mode given by
`AppPlatform::getDefaultInputMode` (Desktop DeviceFamily -> **Mouse**) and then switches **only because of the kind of input
items it receives**. `InputHandler::tick` (0x5abb44) asks every device mapper whether it saw input this frame; the last mapper
that says yes decides the mode. Our host only ever enqueues `MouseItem`s (touch arrives as Windows-emulated mouse messages),
so `MouseMapper` is the only active one, the mode stays Mouse, and the native touch controls (drawn by `TouchMapper::render`
only while mode == Touch) are never drawn. Fix = deliver touch contacts as `MultitouchItem`s (factory 0x62a174) and stop
the emulated mouse messages.

## InputMode
`0 Undefined, 1 Mouse, 2 Touch, 3 GamePad, 4 MotionController` (`InputMode_toString` 0x5ab74c). Per-client current mode =
first dword of the node at `InputHandler+0x11c` map (`getInputMode` 0x5ad050 returns `&node+0xc`). Only `InputHandler::tick`
(and `registerClient` 0x5ad0b8 at creation) writes it.

## Who decides (InputHandler::tick 0x5abb44)
mapper vector at `InputHandler+0x108`, order built by `MinecraftInputHandler::setupMappers` 0x38ba44:
`Gaze, Keyboard, Mouse, GamePad, Touch (only if !isHolographic), Motion, DeviceButton, Behavior`.
For each mapper: `vtable[3] tick()` -> bool, if true `vtable[4] getInputMode()` (Mouse 1, Touch 2, GamePad 3, Motion 4,
Keyboard/Gaze/DeviceButton/Behavior 0 = never switch). Last non-zero wins (Touch beats Mouse/GamePad in the same tick).
Switch happens if `new != current && InputHandler+0xf8 == 0` (lock = AppPlatform+0x210 = `oculus_launched` argument = 0 here),
logs "Switching input mode from X to Y", stores the mode and fires the listeners (`ClientInstance::onInputModeChanged`
0x1160cc: re-applies per-mode options and, for Touch, releases the mouse grab via AppPlatform vtable[19]).

| event | stock handler | item | mapper | result |
|---|---|---|---|---|
| pointer, device Mouse(2) | onPointerPressed/Moved/Released 0x624a64/0x625084/0x625398 | MouseItem type 0 | MouseMapper | Mouse |
| pointer, device Touch(0) | same handlers, touch branch | **MultitouchItem type 1** | TouchMapper | **Touch** |
| pointer, device Pen(1) | ignored | - | - | - |
| wheel / MouseDevice.MouseMoved / polled buttons | 0x625a50 / 0x62499c / 0x626570 | MouseItem | MouseMapper | Mouse |
| key / char | 0x625b10 0x625d64 0x625e50 | KeyCodeItem / TextItem | KeyboardMapper (mode 0) | no change |
| XInput | - | - | GamePadMapper | GamePad |

## Platform queries (value on our host)
| query | RVA / cell | host value |
|---|---|---|
| DeviceFamily -> deviceType (+0x3a8) | ctor 0x581610, getter vf116 0x583aa0 (cell 0x1369774) | "Windows.Desktop" -> 0 |
| getDefaultInputMode vf154 | 0x62212c (cell 0x136980c) | 1 (Mouse) |
| supportsTouch vf41 ($touch_supported) | 0x621390 (cell 0x1369648) | true |
| vf42/vf43 ($keyboard_and_mouse_supported / $gamepad_supported) | 0xad200 `return 1` | true (constant) |
| can_quit vf39 | 0x6212f4 | true |
| lock flag AppPlatform+0x210 | base ctor 0x61ffc0 | 0 |
No query of mouse/keyboard/touch hardware exists (no GetSystemMetrics, no *Capabilities classes). The fake MouseDevice is
irrelevant (its relative-move delegates are never fired on ARM) and `GetAsyncKeyState` of the fake CoreWindow returns 0.

## User setting?
No. Settings > Controls > Touch (hidden when `$is_holographic` or not `$touch_supported`) only tunes touch play: sensitivity,
invert Y, auto jump, vibration, toggle crouch, "Split Controls" (= option `ctrl_usetouchjoypad`, lang `options.usetouchpad`).
The per-mode options are InputMode{Bool,Float}Option (ids 0x8c..0x91; key format `<key>_<ModeName>`). `button.simulate_touch`
is just a key bound to AppPlatform::toggleMouseMode.

## Item path (verified)
`processHIDItem` 0x582ff8 (drains the InputAdapter queue every frame) dispatches on the item type byte: 0 Mouse -> Pointer::feed
(global Mouse object 0x1978880), **1 Multitouch -> Multitouch::feed 0x5b00ec** (12 pointer slots 0x19788a8 + i*0x28), 2 key, 3 text, 4 caret.

`make_MultitouchItem` 0x62a174 (item 0x10 bytes, vtable 0x136ddf8, type byte 1):
`void** f(void** out r0, const int* buttonId r1, const int* data r2, const short* x r3, const short* y [sp], const BYTE* slot [sp+4])`
-> `+8 x, +0xa y, +0xc buttonId, +0xd data, +0xe slot`; hand to EnqueueInput 0x587294 with the address of a pointer slot (same as the mouse item).
Events: down `(1,1)`, move `(0,0)`, up `(1,0)`; slot = lowest free index 0..11 per contact (stock `allocPointerSlot` 0x627af0);
x,y in swap-chain-buffer pixels like the mouse path (stock multiplies DIPs by adapter+0xa0/0xa4 = 1.0f).

## Where the touch HUD comes from
Not from JSON (the pack has no touch HUD). `TouchMapper` (vtable 0xdbba20) owns TouchGlyphButton/TextButton/TurnInteract/GuiPassthrough
controls (areas `binding.area.*`); `renderInputMapperControls` 0x3783ac (called from the UI renderer 0x4743f8 @0x4749aa when a player
exists, the top scene is not a menu and the HUD is not hidden) calls `mapper->vtable[6]` only for mappers whose `getInputMode()` equals
the current mode, i.e. only in Touch mode. Menus also use `$touch` ( = mode==2, UI variable setter 0x329368).

## Host plan (details in json `plan_*` and `port_instructions`)
1. `queue_touch_event(buttonId,data,x,y,slot)` next to `queue_mouse_event` using the factory above.
2. `window_proc`: WM_POINTERDOWN/UPDATE/UP/CAPTURECHANGED for PT_TOUCH -> events, `return 0` (no DefWindowProc, so no mouse emulation);
   other pointer types -> DefWindowProc. Fallbacks: WM_TOUCH, or drop mouse messages with `GetMessageExtraInfo() & 0xFFFFFF00 == 0xFF515700`.
3. Back to Mouse needs nothing: any real MouseItem flips it (stale touch release flags are cleared only by mapper reset on scene/focus changes).
4. Optional: patch cell 0x136980c (getDefaultInputMode) to return 2 on touch-only machines.
5. Diagnostics: log mapper list (`InputHandler+0x108`), mode changes (`getInputMode` recipe), touch events, absolute-mouse request on entering Touch.

## Open
Identity of the object at `ClientInstance+0x34` (its vtable[10] = isHolographic decides whether the TouchMapper exists; expected false);
exact per-mode options.txt key names; WM_POINTER on RT 8.1 not executed; Option B (invoking the stored CoreWindow pointer delegates with fake
PointerEventArgs/PointerPoint/PointerDevice objects) only specified.
