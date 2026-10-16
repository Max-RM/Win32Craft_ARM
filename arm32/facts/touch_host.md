# Touch input on a touch-only device - host side (Investigator B)

Scope: why the Surface RT (Windows RT 8.1, no keyboard/mouse) never shows the game's on-screen touch controls, what the
host feeds the game today, and a concrete host-side design (Windows touch -> game MultitouchItem, input-source switching,
on-screen keyboard). Machine-readable companion: `touch_host.json` (RE_GUIDE output contract). Everything marked
**[RE]** was read from the ARM exe with `arm32re.py`, **[SRC]** from `win32_host.c`, **[WIN]** is Windows API knowledge that
cannot be verified on this x64 machine (to be proven by the device log), **[?]** is an open point.

## 0. Answers in one page

1. **There is no game setting that turns the touch controls on.** The only touch-related persisted option in the ARM exe is
   `ctrl_usetouchjoypad` (UI label `options.usetouchpad`, option registration `sub_3f5d70`, string 0xDACBEC) - a layout
   preference. `ctrl_usetouchscreen` does not exist in this build (already noted in `create_user_data`). The HUD style is chosen
   at run time from the *kind of input the game receives* (see 1.4): `#touch_being_used` == `ScreenController+0x24c == 2`.
2. **The game never asks Windows which devices are attached.** The exe contains no `TouchCapabilities`, `KeyboardCapabilities`,
   `MouseCapabilities`, `PointerDevice`, `UserInteractionMode`, `UIViewSettings` (ASCII and UTF-16 searched). The only device
   queries are `Windows.System.Profile.AnalyticsInfo.DeviceFamily` (-> AppPlatform_Winrt+0x3a8 device class) and the PnP
   container/model/manufacturer telemetry strings. So the host cannot "report a mouse as attached"; it makes the game *behave
   as if* a mouse and keyboard were in use.
3. **What does make the game believe that** (strongest first):
   a. *Every* finger event arrives in the host as a legacy mouse message (Windows' touch->mouse emulation, because
      `window_proc` passes all `WM_POINTER*` to `DefWindowProcW`) and is injected as `MouseItem` (HID item type 0) [SRC window_proc,
      queue_mouse_event]. The game therefore sees an absolute mouse moving and a left button pressed.
   b. Inside a world the host asserts a **captured relative mouse**: `refresh_relative_mouse_from_client_128` ->
      `host_relative_requested` -> `update_mouse_capture` does `SetCapture`, `ShowCursor(FALSE)`, `ClipCursor`,
      `set_game_relative_actual(TRUE)` (writes `HIDController+0xa0 = 1`) and turns every `WM_MOUSEMOVE` into relative deltas with
      `SetCursorPos(center)` [SRC 17429-17464, 19481-19500]. The game's own touch branches are gated on that flag [RE 0x624c60,
      0x6255ca].
   c. The host **never produces a `MultitouchItem` (HID item type 1)**. In the game its factory (0x0062A174) has exactly six
      callers, all inside the three WinRT pointer handlers that the game registers on `CoreWindow` (0x624A64 pressed, 0x625084
      moved, 0x625398 released; registration `sub_62420c`). The host stores those handlers (`core_window_pointer_*_handler_116`) but on ARM
      nothing ever calls them (the `invoke_pointer_handler_116` code lives in `win32_host_116.inl`, not compiled for ARM).
   d. Not a cause: `Windows.Devices.Input.MouseDevice` fake (UWP has it on every device), `DeviceFamily = "Windows.Desktop"`
      (same string a real Surface reports; AppPlatform class 0), `GetAsyncKeyState` fake returns 0.
4. **Fix = give the game real touch input**: handle `WM_POINTERDOWN/UPDATE/UP/CAPTURECHANGED` for `PT_TOUCH` pointers in
   `window_proc` (return 0, no `DefWindowProc`), convert them to `MultitouchItem`s via the game's own factory and queue, stop the relative-mouse capture
   while the last input came from touch, show the Windows touch keyboard for text boxes. Mouse and keyboard keep working
   unchanged and the active source follows "last used wins". Section 4 names every function.
5. **Confidence**: root cause (no type-1 items, mouse emulation, relative capture) high [RE+SRC]. That feeding type-1 items flips
   `InputMode` to Touch is the way the UWP build works (type-1 items have no other producer) but the exact setter of the mode
   field was **not** traced here (Investigator A's area); it is the main residual risk, see 6.

## 1. What the game receives today

### 1.1 Host input path (win32_host.c, ARM build)

| piece | where | behaviour |
|---|---|---|
| `window_proc` | 19580 | `WM_KEY*`/`WM_CHAR` -> `handle_win32_key`/`handle_win32_character`; `WM_MOUSEMOVE` -> `queue_pointer_position`; `WM_L/R/MBUTTON*` -> `SetFocus`, `SetCapture`, `queue_pointer_position` + `queue_mouse_event(x,y,0,button,pressed)`; `WM_MOUSEWHEEL` -> wheel item; **no `WM_POINTER*`, `WM_TOUCH`, `WM_GESTURE` case: default branch -> `DefWindowProcW`** |
| `queue_pointer_position` | 19481 | if `mouse_capture_active`: x,y minus client centre -> relative item (payload 1) + `center_mouse_cursor`; else absolute item (payload 0) |
| `queue_mouse_event` (ARM) | 18900 | `arm32_input_target` (AppMain+0x1c -> platform+0x3b4 -> adapter+4 -> HIDController, probes), absolute coords scaled client->swap-chain buffer by `arm32_scale_pointer_to_buffer`, `call_game_make_mouse_event` (0x0062A128) + `arm32_enqueue_input` (0x00587294). First 12 events logged |
| `queue_keyboard_event` / `queue_character_event` (ARM) | 19115 / 19157 | key factory 0x0062A254 (VK 8..0xDE only, so `VK_PACKET` 0xE7 is dropped), text factory 0x0062A2B4; character events only while `text_input_active` |
| `get_hid_controller` | 16331 | AppMain -> platform+0x3b4 (InputAdapter, 0x170 bytes) -> +4 HIDController |
| mouse-mode hooks | 16431-16597 | the three `std::function` lambda cells (relative/absolute/toggle) record the game's request in `host_relative_requested` |
| `game_requests_relative_mouse` (ARM) | 16604 | returns `host_relative_requested`; consumes the one-frame command at HIDController+0x28 |
| `refresh_relative_mouse_from_client_128` (ARM) | 17159 | derives `gameplay` = HUD scene with `!is_showing_menu && should_steal_mouse && !text_input_active` and assigns `host_relative_requested`; sets `text_input_active` from `MinecraftKeyboardManager` byte+5 bit 1 |
| `update_mouse_capture` | 17429 | (see 0.3b) |
| fake CoreWindow | 6253 | 58-slot vtable. Pointer/key/char event adders (slots 28/34/36/44/46/48/52) store the handler (`fake_core_window_event_add_116`) and return token 1; slot 20/21 `GetAsyncKeyState` returns 0; `get_PointerPosition` returns (0,0) |
| fake MouseDevice | 6117, 6302 | `GetForCurrentView` + `add_MouseMoved` accept and (non-1.16) ignore the handler |
| fake AnalyticsInfo | 8559 | `DeviceFamily = "Windows.Desktop"` -> AppPlatform_Winrt+0x3a8 = 0 [RE 0x5816e8-0x5817b8: Xbox -> 2, Mobile -> 1, else 0] |
| fake InputPane | 9012-9127 | `TryShow`/`TryHide` both `arm32_input_pane_try` -> returns TRUE, nothing shown; `OccludedRect` = 0 |
| main loop | 20355 | `PeekMessageW` drain + `TranslateMessage`/`DispatchMessageW`, then `drain_dispatcher_queue`, `refresh_relative_mouse_from_client_128`, `update_mouse_capture`, one game frame |
| window | 20556-20587 | class `MCPE128Win32`, `CS_HREDRAW|CS_VREDRAW|CS_OWNDC`, `WS_OVERLAPPEDWINDOW|WS_VISIBLE` 1280x720 client request, no DPI-awareness call, no `RegisterTouchWindow`, no touch feedback settings |
| user32 imports of win32craft.dll | `llvm-readobj --coff-imports` | only basic functions (AdjustWindowRectEx ... wsprintfA); no pointer/touch/metric APIs yet |

### 1.2 What Windows delivers for a finger today [WIN, not yet confirmed by a device log]

`DefWindowProc` receives the `WM_POINTERENTER/DOWN/UPDATE/UP/LEAVE` stream and derives the legacy messages for the **primary**
contact only: `WM_MOUSEMOVE` (the cursor jumps to the finger), `WM_LBUTTONDOWN` (`MK_LBUTTON`), drag `WM_MOUSEMOVE`, `WM_LBUTTONUP`.
Their `GetMessageExtraInfo()` carries the pen/touch signature: `(extra & 0xFFFFFF00) == 0xFF515700` (`MI_WP_SIGNATURE`) and bit
0x80 set for touch. Additional contacts produce **no** mouse messages. Unhandled gestures are also translated by `DefWindowProc`: pan and pinch
may come out as `WM_MOUSEWHEEL` (pinch with `MK_CONTROL`), press-and-hold as `WM_RBUTTONDOWN/UP` + `WM_CONTEXTMENU`, and the system draws touch
feedback (contact dot, press-and-hold ring). Whether the emulated `WM_LBUTTONDOWN` is delivered at touch-down or at lift for a quick tap
differs between Windows builds; the planned log line (section 4.9) records it.

The game therefore gets: `MouseItem(action 0, absolute)` moves, `MouseItem(action 1, pressed 1/0)` clicks, maybe wheel items (action 4) and
right clicks (action 2) from gestures, and - inside a world - relative deltas with HIDController+0xa0 = 1. One finger only, never multi-touch,
never a type-1 item. This matches the user's report: menus work (they are mouse driven), nothing touch specific appears.

### 1.3 Mouse/keyboard-presence summary
See 0.3. In code terms the "mouse is present" signal is: item types 0/2/3 only, plus `HIDController+0xa0` set by `set_game_relative_actual(TRUE)`.

### 1.4 Game-side facts the host design depends on (all [RE], details in touch_host.json)

* HID item types (byte at item+4): 0 Mouse (0x14 bytes), **1 Multitouch (0x10 bytes)**, 2 KeyCode (0x10), 3 Text (0x24), 4 Caret.
  Consumer `AppPlatform_Winrt::processHIDItem` (0x00582FF8) pops one item (`sub_587314`) and dispatches on the type byte.
* **MultitouchItem factory `input:make_MultitouchItem` 0x0062A174** (plain AAPCS):
  `void** f(void** out r0, const int* buttonId r1, const int* data r2, const short* x r3, const short* y [sp], const BYTE* pointerId [sp+4])`.
  It `operator new(0x10)`, vtable at +0, **type byte 1 at +4**, x short +8, y short +0xa, buttonId byte +0xc, data byte +0xd, pointerId byte +0xe.
  Expected bytes `2de9f04b0df1180b81461020`.
* `processHIDItem` type 1 calls `Multitouch::feed` (0x005B00EC) with (buttonId = item+0xc, data = item+0xd, x = +8, y = +0xa, pointerId = +0xe, all
  signed loads). `Multitouch::feed` clamps pointerId to <= 11 (>= 12 -> 11) but a **negative id fail-fasts** (`sub_b49964`+trap at 0x5B01AE), so valid ids are
  **0..11**. Semantics: buttonId 0 = move; buttonId 1 with data 1 = press (sets the down/pressed arrays), data 0 = release.
  It also forwards the sample to the per-pointer Mouse instance array at 0x019788A8 (stride 0x28) via `sub_5af968`.
* Stock handlers and the values they emit for **non-mouse** pointers (PointerDevice.PointerDeviceType != 2; 0 touch, 1 pen, 2 mouse):
  pressed 0x624A64 -> `(1,1)`, moved 0x625084 -> `(0,0)`, released 0x625398 -> `(1,0)`; x,y = Position(DIP) * InputAdapter scale (+0xa0/+0xa4) via `sub_627f28`.
  (Mouse pointers under `HIDController+0xa0 != 0` take a different branch; the pressed handler there also emits a `(1,1)` Multitouch item at 0x624cec - see open questions.)
* Pointer handlers are registered once by `sub_62420c` through CoreWindow vtable offsets 0xB8 (add_PointerPressed), 0xB0 (add_PointerMoved), 0xC0 (add_PointerReleased).
* UI: `#touch_being_used` binding is `ScreenController+0x24c == 2` (lambda 0x001FB460); the field is written by `sub_1fad80` (setter, modes 3/4 special-cased). UI variables
  `$touch_supported` / `$keyboard_and_mouse_supported` are read from a struct (sub_287fbc+0x1ba / +0x18c, bytes at struct+0x50 / +0x4c) [?: producer not traced].
* AppPlatform_Winrt+0x3a8 device class: 0 desktop, 1 mobile (`Windows.Mobile`), 2 Xbox. vf37 (0x5826B0) = (class == 1), vf40 (0x5827B4) = (class != 2),
  vf116 (0x583AA0) returns the class, vf147 (0x583B50) = native machine ARM && class 0. Changing `DeviceFamily` to `Windows.Mobile` is **not** recommended as the primary fix
  (phone semantics everywhere, unknown side effects) but it is a cheap experiment lever (`fake_analytics_device_family`, 8559).

## 2. Windows-side design (Windows RT 8.1 = 6.3.9600 and Windows 10 ARM32)

### 2.1 API availability (SDK 10.0.19041 ARM `user32.lib` contains post-8.1 symbols, so link-time presence proves nothing)

| API | since | use | how to bind |
|---|---|---|---|
| `WM_POINTERDOWN/UPDATE/UP/CAPTURECHANGED/ENTER/LEAVE/ACTIVATE` (0x246/245/247/24C/249/24A/24B) | Win 8 (6.2) | primary path | message numbers only |
| `GetPointerType(UINT32, POINTER_INPUT_TYPE*)` | Win 8 | touch vs mouse/pen/touchpad (PT_TOUCH 2, PT_PEN 3, PT_MOUSE 4, PT_TOUCHPAD 5 = 8.1+) | **GetProcAddress** |
| `GetPointerInfo`, `GetPointerTouchInfo`, `GetPointerFrameTouchInfo` | Win 8 | optional extras (contact rect, frame) - not needed | GetProcAddress (only if wanted) |
| `EnableMouseInPointer`/`IsMouseInPointerEnabled` | Win 8 | **do not call `EnableMouseInPointer`** (would turn the mouse into pointer messages and kill the legacy mouse path). `IsMouseInPointerEnabled` only for a log line | GetProcAddress |
| `RegisterTouchWindow`, `GetTouchInputInfo`, `CloseTouchInputHandle`, `WM_TOUCH` 0x240 | Win 7 | fallback 1 | GetProcAddress |
| `SetWindowFeedbackSetting` | Win 7 | switch off contact dot / tap / press-and-hold feedback | GetProcAddress |
| `GetSystemMetrics(SM_DIGITIZER 94, SM_MAXIMUMTOUCHES 95, SM_MOUSEPRESENT 19)`, `GetMessageExtraInfo`, `ScreenToClient`, `GetFocus`, `FindWindowW`, `IsWindowVisible`, `GetWindowRect` | old | always available | static import is fine |
| `GetRawInputDeviceList`, `GetRawInputDeviceInfoW` | XP/Vista | device inventory for the log / initial guess | GetProcAddress (cheap insurance) |
| `GetDpiForWindow`, `GetDpiForSystem`, `SetProcessDpiAwarenessContext`, `GetSystemMetricsForDpi`, `GetPointerInputTransform` | Win 10 1607+ | **must not be imported statically** (they are in the SDK lib, absent on 8.1 -> the DLL would fail to load with "entry point not found") | avoid |

Rule for the implementer: declare every new user32/shell32 function by hand like the existing `extern ... AdjustWindowRectEx` (the host does not include SDK headers; `win32_compat.h`
has only `LOWORD/HIWORD`), and resolve everything that is not in the "old" row with `GetProcAddress(GetModuleHandleW(L"user32.dll"), ...)` into static function pointers inside `arm32_input_init`.
Struct layouts needed on ARM32: `TOUCHINPUT` 40 bytes {LONG x,y; HANDLE hSource; DWORD dwID,dwFlags,dwMask,dwTime; ULONG_PTR dwExtraInfo; DWORD cxContact,cyContact}; `RAWINPUTDEVICELIST` 8 bytes {HANDLE hDevice; DWORD dwType}.
The primary path needs **no** big pointer struct: pointer id, flags and screen position are in `wParam`/`lParam`.

### 2.2 Primary mechanism: `WM_POINTER*`, touch pointers only

Handling (all on the UI thread inside `window_proc`; function `arm32_touch_pointer_message` in 4.2):

* `id = LOWORD(wParam)` (GET_POINTERID_WPARAM), `flags = HIWORD(wParam)` (POINTER_MESSAGE_FLAG_NEW 0x1, INRANGE 0x2, INCONTACT 0x4, FIRSTBUTTON 0x10, PRIMARY 0x2000, CONFIDENCE 0x4000, CANCELED 0x8000).
  `lParam` = **screen** coordinates in pixels (`(short)LOWORD`, `(short)HIWORD`) for DOWN/UPDATE/UP. **For `WM_POINTERCAPTURECHANGED` lParam is the HWND that took capture, not a position.**
* Position -> client: `POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)}; ScreenToClient(window, &p);` then clamp to `[0, client_w-1] x [0, client_h-1]` (touch contacts are implicitly captured to the window where they started, so
  updates keep coming outside the client area; the clamp keeps a joystick drag alive) and apply the existing `arm32_scale_pointer_to_buffer` (client px -> swap-chain buffer px, needed when the 2048 clamp makes the buffer smaller). Do **not** use HIMETRIC
  (`ptHimetricLocation`, 0.01 mm) or `GetPointerInfo().ptPixelLocation`: lParam + `ScreenToClient` is the same coordinate space as the existing mouse path, and stays correct under the
  DPI virtualisation Windows applies to this DPI-unaware process (Surface 2 / Pro at 150 %). (`WM_TOUCH` would need `/100`: `TOUCH_COORD_TO_PIXEL`.)
* Dispatch:
  * `WM_POINTERDOWN`: `GetPointerType(id,&type)`; if it fails or `type != PT_TOUCH` -> `DefWindowProcW` (mouse, pen, touchpad keep using the legacy messages). Touch: allocate the **lowest free slot 0..11**
    (the game's valid pointer ids), remember `{id, slot, x, y, dirty=0}`, `arm32_input_note_source(TOUCH)`, `if (GetFocus() != window) SetFocus(window)`, release `initial_menu_cursor_guard` like the first mouse click does,
    queue `(buttonId 1, data 1, x, y, slot)`, return 0. If all 12 slots are used: log once, ignore, return 0.
  * `WM_POINTERUPDATE`: look the id up in the slot table first; if present, store position, set `dirty` (sent once per frame by `arm32_touch_flush_moves`, section 4.4) and return 0. If absent: no `GetPointerType`
    call unless `flags & INCONTACT` (mouse hover updates therefore cost nothing); `INCONTACT` with an unknown touch id (missed DOWN) is treated as a DOWN.
  * `WM_POINTERUP`: slot lookup (no type call needed); flush a pending move, queue `(1, 0, x, y, slot)`, free the slot, return 0. `POINTER_MESSAGE_FLAG_CANCELED` is handled the same way (release at the last position).
  * `WM_POINTERCAPTURECHANGED`: if `id` is an active slot, synthesise the release at the last known position, free the slot, return 0 (log "capture lost"). Otherwise `DefWindowProcW`.
  * `WM_POINTERENTER/LEAVE/ACTIVATE`, `WM_POINTERROUTEDTO/AWAY/RELEASED`, `WM_NCPOINTER*`: unchanged (`DefWindowProcW`); `WM_POINTERACTIVATE` must reach `DefWindowProc` so a touch activates the window.
* **DefWindowProc rule [WIN]**: returning 0 from `WM_POINTERDOWN/UPDATE/UP` for a touch contact without calling `DefWindowProc` prevents the system from synthesising the legacy mouse
  messages, gestures (`WM_GESTURE`, the wheel/right-click conversions) and touch feedback for that contact. Mouse and pen pointers are passed on so the legacy path is untouched.
  As belt and braces the legacy handlers also test the extra info signature (2.4) and drop touch-sourced mouse messages while touch handling is active; the log counts how many arrived
  (`legacy_from_touch`), which tells us on the first run whether the DefWindowProc assumption holds on RT 8.1.
* Disable the visual feedback once after window creation: `SetWindowFeedbackSetting(hwnd, FEEDBACK_TOUCH_CONTACTVISUALIZATION 1 / TOUCH_TAP 7 / TOUCH_DOUBLETAP 8 / TOUCH_PRESSANDHOLD 9 / TOUCH_RIGHTTAP 10 / GESTURE_PRESSANDTAP 11, 0, sizeof(BOOL), &FALSE)`
  and `SetPropW(hwnd, L"MicrosoftTabletPenServiceProperty", (HANDLE)(TABLET_DISABLE_PRESSANDHOLD 0x1 | PENTAPFEEDBACK 0x8 | PENBARRELFEEDBACK 0x10 | FLICKS 0x10000 | FLICKFALLBACKKEYS 0x100000))`
  (declared in tpcshrd.h; `SetPropW` needs a hand-written extern). Failures are only logged.
* `EnableMouseInPointer` implications: process-wide; with TRUE the mouse also produces `WM_POINTER*` (PT_MOUSE) and **no** `WM_MOUSE*`/`WM_MOUSEWHEEL`; the whole existing mouse/wheel/relative-delta path
  would have to be rebuilt. Rejected; the design keeps it FALSE (the init log prints `IsMouseInPointerEnabled`).

### 2.3 Fallbacks (selectable with `--touch-api`, section 4.7, so the user can switch without a rebuild)

1. **`wmtouch`** (`WM_TOUCH`, also what `DefWindowProc` produces from pointer messages for a registered window): `RegisterTouchWindow(hwnd, 0)`; in `window_proc`
   `case 0x240`: `n = LOWORD(wParam)`, `GetTouchInputInfo((HANDLE)lParam, n, TOUCHINPUT[], sizeof)`, per input: `x,y = TOUCH_COORD_TO_PIXEL(ti.x/y)` (screen) -> `ScreenToClient`, `dwFlags & TOUCHEVENTF_DOWN 2 / MOVE 1 / UP 4`,
   `dwID` = contact id (same slot table), ignore `TOUCHEVENTF_PALM 0x80`, then `CloseTouchInputHandle((HANDLE)lParam)` and return 0. Use when `GetPointerType` is missing (Windows 7 test machines) or pointer handling misbehaves on a device.
2. **`legacy`** (single touch, always available): in the mouse cases of `window_proc` test
   `ex = (DWORD)GetMessageExtraInfo(); touch = ((ex & 0xFFFFFF00) == 0xFF515700) && (ex & 0x80)` (`MI_WP_SIGNATURE`, pen = signature without 0x80). A touch-sourced
   `WM_LBUTTONDOWN/MOUSEMOVE(MK_LBUTTON)/LBUTTONUP` is turned into slot 0 `(1,1)/(0,0)/(1,0)` Multitouch items instead of Mouse items; `WM_RBUTTON*`/`WM_MOUSEWHEEL` with the touch signature are dropped. No multi-touch (two thumbs: joystick + jump
   impossible), but it already switches the HUD.

Why pointer as primary: explicit touch/mouse/pen separation (`GetPointerType`) without heuristics, per-contact ids, explicit cancel/capture notifications, screen pixel coordinates, no handle to close, and it is what the UWP game itself receives.

### 2.4 Presence detection and dynamic switching ("last input wins")

* Startup (`arm32_input_init`, only informational + initial guess): log `SM_DIGITIZER` (bits NID_INTEGRATED_TOUCH 1, EXTERNAL_TOUCH 2, INTEGRATED_PEN 4, EXTERNAL_PEN 8, MULTI_INPUT 0x40, READY 0x80),
  `SM_MAXIMUMTOUCHES`, `SM_MOUSEPRESENT`, `SM_CMOUSEBUTTONS`, `SM_CONVERTIBLESLATEMODE 0x2003`, `SM_REMOTESESSION`, and the raw-input inventory (`GetRawInputDeviceList`: RIM_TYPEMOUSE 0 / RIM_TYPEKEYBOARD 1 with
  `GetRawInputDeviceInfoW(RIDI_DEVICENAME 0x20000007)`; names containing `Root#`/`RDP_` are virtual and not counted). **`SM_MOUSEPRESENT` is not reliable on touch tablets (Windows keeps a logical mouse), so it is logged but never used to decide.**
  Initial source: `TOUCH` when `SM_MAXIMUMTOUCHES > 0` and no real mouse/keyboard was found, else `MOUSEKB`.
* Runtime (authoritative): `arm32_input_note_source(src, why)` is called from
  * touch down (pointer/WM_TOUCH/legacy-touch) -> `TOUCH`;
  * mouse: `WM_LBUTTON/RBUTTON/MBUTTONDOWN` and `WM_MOUSEWHEEL` without the touch signature, and `WM_MOUSEMOVE` without the signature **only if** the cursor position differs from the last mouse position
    and no touch happened in the last 300 ms (Windows re-posts synthetic `WM_MOUSEMOVE` on window changes even without a mouse) -> `MOUSEKB`;
  * `WM_KEYDOWN`/`WM_SYSKEYDOWN` -> `MOUSEKB`, **except** while `text_input_active` and the touch keyboard is visible (its injected keys must not flip the HUD).
* Effects of `TOUCH` (`arm32_input_touch_active()` true): `game_requests_relative_mouse()` returns FALSE (so `update_mouse_capture` never captures/clips/hides, `HIDController+0xa0` stays 0), mouse items are not generated for touch, and the OSK is allowed.
  Effects of switching to `MOUSEKB`: `arm32_touch_release_all("source -> mouse/keyboard")` (queues the releases so no joystick stays stuck) and the old behaviour resumes.
* Policy `--input=touch` pins `TOUCH` (mouse/keyboard still deliver events, capture stays off), `--input=mouse` disables all of the above (exactly today's behaviour: pointer messages go to `DefWindowProc`), `--input=auto` (default) = state machine.
  Hot-plugging a keyboard cover therefore needs no device notification: the first key press flips to `MOUSEKB`, the next touch flips back.
* Release-all triggers: `WM_KILLFOCUS`, `WM_CANCELMODE`, `WM_ACTIVATEAPP(FALSE)`, `WM_SIZE(SIZE_MINIMIZED)`, `WM_POINTERCAPTURECHANGED` (per contact), source switch, shutdown.

## 3. Text input on a touch-only device

Facts: the game asks for the keyboard through `AppPlatform_Winrt` show/hide (vf11 0x5829AC / vf12 0x582A5C) which post the lambdas 0x586CC4 (`InputPane.TryShow`, IInputPane2 slot 6) and 0x586F8C (`TryHide`, slot 7; also run **once
unconditionally at startup**), and reads `OccludedRect` (IInputPane slot 10, function 0x57E2E8) when a text box gets focus [RE + SRC 9012-9127]. The host fake answers TRUE and shows nothing, so on a touch-only device a focused text box can never be typed into.
Text then arrives the normal way: `WM_KEYDOWN` (real VKs) and `WM_CHAR` (from `TranslateMessage`, which also converts `VK_PACKET` 0xE7 key messages into `WM_CHAR`), forwarded only while `text_input_active`
(MinecraftKeyboardManager byte+5 bit 1). `VK_PACKET` itself is ignored by `queue_keyboard_event` (VK > 0xDE), so no change is needed there.

Recommendation: **hook the existing fake instead of patching the game** - split `arm32_input_pane_try` (shared by the TryShow/TryHide vtable slots) into `arm32_input_pane_try_show` and `arm32_input_pane_try_hide`, return TRUE as before and call
`arm32_osk_show("InputPane.TryShow")` / `arm32_osk_hide("InputPane.TryHide")`. Both already run on the host thread from `drain_dispatcher_queue`.

APIs that exist on RT 8.1 and Windows 10 (all [WIN]; the class name and path are the main things the first device log must confirm):
* Window of the touch keyboard: `FindWindowW(L"IPTip_Main_Window", NULL)`; visible = `IsWindowVisible`; rectangle = `GetWindowRect`.
* Show: if not visible, start `TabTip.exe` = `SHGetFolderPathW(CSIDL_PROGRAM_FILES_COMMON 0x2B)` + `\microsoft shared\ink\TabTip.exe` with `ShellExecuteExW`/`CreateProcessW` (non-blocking, throttle to one attempt per second). If the
  process already runs but the window is hidden (or on Windows 10 where starting it again does nothing): COM `CoCreateInstance(CLSID_UIHostNoLaunch {4CE576FA-83DC-4F88-951C-9D0782B4E376}, NULL, CLSCTX_INPROC_HANDLER|CLSCTX_LOCAL_SERVER, IID_ITipInvocation {37C994E7-432B-4834-A2F7-DCE1F13B834B})`
  and `ITipInvocation::Toggle(GetDesktopWindow())` (vtable index 3, toggles, so call it only when `FindWindow` says hidden). The host's main thread is an MTA after `RoInitialize(RO_INIT_MULTITHREADED)`, which is fine for this out-of-proc server.
* Hide: `PostMessageW(tip_window, WM_SYSCOMMAND, SC_CLOSE, 0)` when visible (the process stays alive for the next show). Never kill TabTip.exe.
* The touch keyboard is non-activating, so the game keeps keyboard focus; the host must not call `SetFocus` on keyboard taps. `WM_KILLFOCUS`/`release_all_win32_keys` already cover a focus loss.
* Eligibility (`--osk=auto|on|off`): auto = `arm32_input_touch_active()` (no hardware keyboard has been used since the last touch) so a user with a Type Cover still types on the cover; on = always; off = never (diagnostics).
* `OccludedRect`: `arm32_input_pane_occluded_rect` returns the intersection of the keyboard window rectangle with the client area, in the same units as `CoreWindow.Bounds` (swap-chain buffer pixels): `ScreenToClient` the rect corners, then the same client->buffer factor as `arm32_scale_pointer_to_buffer`; zeros when hidden.
  The keyboard window appears a few hundred ms after `TabTip.exe` starts, so the first query right after `TryShow` may still be zero. Phase 2 (only if the chat box ends up behind the keyboard on device): store the handlers given to `add_Showing`/`add_Hiding`
  (slots 6/8, tokens already faked) and invoke them (`handler->vtable[3](handler, &arm32_input_pane, &args)`, args = fake `IInputPaneVisibilityEventArgs`: 6 get_OccludedRect(Rect*), 7 put_EnsuredFocusedElementInView(bool), 8 get_EnsuredFocusedElementInView(bool*)) from
  a small poll in the main loop (`arm32_osk_poll`, ~100 ms) when the visibility changes. The game does register such a handler (TypedEventHandler<InputPane, InputPaneVisibilityEventArgs>, `sub_597258`).
* Fallbacks: Windows' own auto-invoke of the touch keyboard works only for controls that expose text patterns, not for this custom window, so it cannot replace the explicit request. `osk.exe` (legacy OSK) is a last resort only.

## 4. Proposed architecture (names are final; everything inside `#ifdef WIN32CRAFT_ARM32`)

### 4.1 Files and placement
Only `win32_host.c` changes (plus 2 package files). No new exports in `win32craft_arm32.def`. Order matters because the host has no headers:

| block | place | content |
|---|---|---|
| A. policy state + parser | in the existing ARM option block next to `arm32_feature_level_cap` (line ~2872) and inside the ARM `Win32CraftParseHostOptions` (2941) | `Arm32InputPolicy`, `arm32_input_policy`, `Arm32TouchApi`, `arm32_touch_api`, `Arm32OskPolicy`, `arm32_osk_policy`, `arm32_input_verbose`; parse `--input`, `--touch-api`, `--osk`, `--input-log`, env `WIN32CRAFT_INPUT`, `WIN32CRAFT_TOUCH_API`, `WIN32CRAFT_OSK`, `WIN32CRAFT_INPUT_LOG` |
| B. forward declarations | ARM block near the other forward declarations (line ~316) | `static BOOL arm32_input_touch_active(void);` `static void arm32_osk_show(const char *why);` `static void arm32_osk_hide(const char *why);` `static BOOL arm32_osk_rect_in_buffer(float rect[4]);` |
| C. OSK | right after the fake InputPane (after line 9127) | `arm32_osk_window`, `arm32_osk_visible`, `arm32_osk_show`, `arm32_osk_hide`, `arm32_osk_rect_in_buffer`, `arm32_input_pane_try_show`, `arm32_input_pane_try_hide` (replace `arm32_input_pane_try` in the two vtable slots 6/7 of `arm32_input_pane2_vtable`), updated `arm32_input_pane_occluded_rect` |
| D. touch item builder | immediately after the ARM `queue_mouse_event` (after line 18953) | `call_game_make_multitouch_event`, `queue_touch_event` |
| E. touch/input core | new ARM section directly before `window_proc` (after `queue_pointer_position`, line ~19511, next to the existing WM_GETMINMAXINFO helper) | constants, `Arm32TouchSlot`, `arm32_touch_slots[12]`, API pointers, `arm32_input_init`, `arm32_input_note_source`, `arm32_input_touch_active`, slot helpers, `arm32_touch_pointer_message`, `arm32_touch_wm_touch`, `arm32_legacy_mouse_filter`, `arm32_touch_flush_moves`, `arm32_touch_release_all`, `arm32_input_log_stats` |
| F. window_proc cases | inside `window_proc` | see 4.3 |
| G. wiring | `Win32Bootstrap` after `game_window = window` (20579), `arm32_boot_run_loop` (20355), `arm32_boot_log_startup_summary` (20439), `game_requests_relative_mouse` ARM body (16604) | see 4.5 |
| H. package | `tools/make_arm32_device_package.py` (loop at line 163 that writes `start_fl91.bat` ...), `ARM32.md` | `start_touch.bat` (`--input=touch`), `start_mouse.bat` (`--input=mouse`); option table + troubleshooting rows |
| I. facts | `arm32/facts/touch_host.json` (this analysis) | run `python tools/arm32/gen_constants.py` -> `ARM_TOUCH_HOST_MAKE_MULTITOUCH_ITEM` etc.; `verify_constants.py` re-checks the bytes |

### 4.2 Functions

```c
typedef enum { ARM32_INPUT_AUTO = 0, ARM32_INPUT_TOUCH = 1, ARM32_INPUT_MOUSE = 2 } Arm32InputPolicy;
typedef enum { ARM32_SRC_NONE = 0, ARM32_SRC_MOUSEKB = 1, ARM32_SRC_TOUCH = 2 } Arm32InputSource;
typedef enum { ARM32_TOUCHAPI_AUTO = 0, ARM32_TOUCHAPI_POINTER, ARM32_TOUCHAPI_WMTOUCH, ARM32_TOUCHAPI_LEGACY } Arm32TouchApi;
typedef enum { ARM32_OSK_AUTO = 0, ARM32_OSK_ON, ARM32_OSK_OFF } Arm32OskPolicy;
typedef struct Arm32TouchSlot { UINT32 id; BOOL active; BOOL dirty; int x, y; DWORD down_tick; } Arm32TouchSlot;   /* x,y: client pixels */
#define ARM32_MAX_TOUCH_SLOTS 12            /* ARM_TOUCH_HOST_MULTITOUCH_MAX_POINTERS: game accepts pointer ids 0..11 */

static void  arm32_input_init(HWND window);                       /* once, right after CreateWindowExW */
static BOOL  arm32_input_touch_active(void);                      /* policy==TOUCH || (policy==AUTO && source==TOUCH) */
static void  arm32_input_note_source(Arm32InputSource src, const char *why);   /* AUTO only; logs transitions; TOUCH->release capture, MOUSEKB->release touches */
static int   arm32_touch_find_slot(UINT32 id);                    /* -1 if none */
static int   arm32_touch_alloc_slot(UINT32 id);                   /* lowest free slot, -1 if full */
static BOOL  arm32_touch_pointer_message(HWND w, UINT msg, WPARAM wp, LPARAM lp);   /* TRUE = consumed (window_proc returns 0) */
static BOOL  arm32_touch_wm_touch(HWND w, WPARAM wp, LPARAM lp);                    /* fallback 1 */
static BOOL  arm32_legacy_mouse_filter(HWND w, UINT msg, LPARAM lp);                /* fallback 2 + drop of touch-sourced mouse msgs; TRUE = consumed */
static void  arm32_touch_down(int slot, int cx, int cy);          /* clamp + queue (1,1) */
static void  arm32_touch_move(int slot, int cx, int cy);          /* store only (dirty) */
static void  arm32_touch_up(int slot, int cx, int cy, const char *why);          /* flush + queue (1,0) + free */
static void  arm32_touch_flush_moves(void);                       /* at most one (0,0) per dirty slot; called once per loop pass */
static void  arm32_touch_release_all(const char *why);
static BOOL  queue_touch_event(int slot, BYTE button_id, BYTE data, int client_x, int client_y);   /* placed after queue_mouse_event */
static void  arm32_input_log_stats(const char *when);             /* counters, see 4.8 */
```

`queue_touch_event` (block D) mirrors `queue_mouse_event`: `arm32_input_target(&input_handler)`; `arm32_scale_pointer_to_buffer(&x,&y)`; clamp to `0..32767`; locals
`int b=button_id, d=data; short sx=(short)x, sy=(short)y; BYTE pid=(BYTE)slot;` then
`((void**(*)(void**,const int*,const int*,const short*,const short*,const BYTE*))ARM32_THUMB_FUNCTION(game_image + ARM_TOUCH_HOST_MAKE_MULTITOUCH_ITEM))(&event,&b,&d,&sx,&sy,&pid)`;
validate `event && !IsBadReadPtr(event, 0x10) && ((BYTE*)event)[4] == 1` (log "multitouch event factory returned an invalid event" otherwise), then `arm32_enqueue_input(input_handler, event)`. Log the first 40 events (`touch event: slot= btn= data= x= y= client=x,y`) and every event when `--input-log`.
Verify the factory bytes once in `install_arm32_input_hooks` (add `{"MultitouchItem factory", ARM_TOUCH_HOST_MAKE_MULTITOUCH_ITEM, ..._EXPECTED}` to its `game_functions[]` table).

### 4.3 `window_proc` changes (ARM-only `#ifdef` blocks; x86 text unchanged)

```c
#ifdef WIN32CRAFT_ARM32
    case 0x0245: case 0x0246: case 0x0247: case 0x024C:          /* WM_POINTERUPDATE/DOWN/UP/CAPTURECHANGED */
        if (arm32_touch_pointer_message(window, message, wparam, lparam)) return 0;
        return DefWindowProcW(window, message, wparam, lparam);
    case 0x0240:                                                  /* WM_TOUCH (fallback 1) */
        if (arm32_touch_wm_touch(window, wparam, lparam)) return 0;
        return DefWindowProcW(window, message, wparam, lparam);
#endif
```
and, at the top of the existing cases `WM_MOUSEMOVE`, `WM_LBUTTONDOWN/UP`, `WM_RBUTTONDOWN/UP`, `WM_MBUTTONDOWN/UP`, `WM_MOUSEWHEEL`, `WM_KEYDOWN/SYSKEYDOWN`:
`#ifdef WIN32CRAFT_ARM32  if (arm32_legacy_mouse_filter(window, message, lparam)) return 0;  #endif` (for keys: `arm32_input_note_source(ARM32_SRC_MOUSEKB, "key")` guarded by the text-input exception).
Also: `WM_KILLFOCUS`, `WM_ACTIVATEAPP(wparam==0)`, `WM_SIZE(SIZE_MINIMIZED)` -> `arm32_touch_release_all(...)`; new `case 0x001F /*WM_CANCELMODE*/` -> release all, `return DefWindowProcW`.
The exit-confirmation heuristic in `WM_LBUTTONUP` (main menu after ESC) is mouse-only; touch-only users cannot press ESC, so no touch equivalent is needed (leave a comment).

### 4.4 Wiring
* `Win32Bootstrap` (ARM, after `game_window = window;` ~20579): `arm32_input_init(window);` (never fails the boot; every API failure only logs).
* `arm32_boot_run_loop` (~20376): after the `PeekMessage` drain and before `drain_dispatcher_queue()`: `arm32_touch_flush_moves();` and `arm32_osk_poll()` if phase 2 is built.
* `game_requests_relative_mouse` ARM body (~16638): final line `return host_relative_requested;` -> `return host_relative_requested && !arm32_input_touch_active();`
  (single gating point; `update_mouse_capture` then releases capture and `set_game_relative_actual(FALSE)` by itself).
* `arm32_boot_log_startup_summary` (~20439): add `input policy=... touch-api=... osk=...` to the identifying line.
* `install_arm32_input_hooks`: add the factory to the sanity-check table (4.2).

### 4.5 Options (command line beats environment beats default, same style as `--feature-level`)
| option | env | values | default |
|---|---|---|---|
| `--input=auto\|touch\|mouse` (also `--input auto`) | `WIN32CRAFT_INPUT` | auto: last used wins; touch: pinned touch; mouse: today's behaviour (no pointer interception) | auto |
| `--touch-api=auto\|pointer\|wmtouch\|legacy` | `WIN32CRAFT_TOUCH_API` | auto = pointer if `GetPointerType` resolves else wmtouch | auto |
| `--osk=auto\|on\|off` | `WIN32CRAFT_OSK` | see section 3 | auto |
| `--input-log` | `WIN32CRAFT_INPUT_LOG=1` | log every touch event / source decision (default: first 40 + all transitions) | off |
Parse helper: one `arm32_parse_choice(const wchar_t *text, const wchar_t *const *names, UINT count, int *out)` reusing `host_argument_equals`; unknown values are logged and ignored (like `--feature-level`). The effective values and their source are logged
by `Win32CraftParseHostOptions` ("ARM32 effective input policy: ..., source: ...").

### 4.6 Logging (grep prefixes `ARM32 input:`, `ARM32 touch:`, `ARM32 osk:`)
* init: `ARM32 input: policy=auto touch-api=pointer osk=auto digitizer=0x000000c1 maxTouches=5 mousePresent=1 mouseButtons=3 slate=? remote=0 rawMice=N rawKeyboards=M (virtual skipped=K) mouseInPointer=0 api: GetPointerType=yes RegisterTouchWindow=yes SetWindowFeedbackSetting=yes ... feedbackOff=ok/fail(err) initialSource=TOUCH`
  plus one line per raw device (`ARM32 input: raw keyboard \\?\HID#...`).
* events: `ARM32 touch: down id=%u slot=%d client=(%d,%d) buffer=(%d,%d) flags=0x%04x`, `up`, `capture-changed`, `cancel`, `slots full`, `queue_touch_event: invalid event`.
* decisions: `ARM32 input: source MOUSEKB -> TOUCH (pointer down id=3)` / `TOUCH -> MOUSEKB (key VK=0x41)` / `(mouse button)` / `(mouse move 14px)`; `relative mouse suppressed (touch)`.
* diagnostics: first 8 legacy mouse messages with their extra info: `ARM32 input: legacy mouse msg=0x0201 extra=0xFF515780 touch=1 action=dropped|used`; stats every 600 frames and at exit: `touch stats: down=N up=N moves=N flushed=N full=N cancel=N legacy_from_touch=N legacy_mouse=N wm_pointer_mouse=N wm_pointer_pen=N source_switches=N`.
* after the first touch down (once): the HID state the game will see: `controller+0xa0`, `+0x25`, `+0x28`, `input_events_enabled`, `text_input_active`.
* osk: `ARM32 osk: show requested (reason) visible=%d window=%p policy=auto touchActive=%d`, `launched TabTip.exe path=... result=%d err=%lu`, `window found rect=...` / `window NOT found after 1500 ms (class name differs?)`, `hide requested`, `occluded rect (..)`.

### 4.7 Test plan for the Surface RT (user runs, sends `win32craft.log`, plus a one-line note of what happened on screen)
1. New package, plain start (no options). Expect the `ARM32 input:` init lines. Report: do `digitizer`, `maxTouches` look right, what `rawMice/rawKeyboards` say, `initialSource`.
2. Main menu: tap a few buttons. Log: `touch: down/up` pairs, `legacy_from_touch` stays 0 (if it grows the DefWindowProc suppression assumption is wrong: then try `--touch-api=legacy` or tell us the counters).
3. Create/enter a world with no cover attached. Expected: touch HUD (joystick/jump/sneak) visible, `relative mouse suppressed (touch)` and **no** `relative mouse captured` line. If the HUD is still the mouse HUD while `touch: down` lines and `touch event:` lines are present, the remaining fault is game-side (Investigator A); send the log.
4. Multi-touch: hold the joystick with one finger and tap jump with another; log shows two slots (0 and 1) active at once.
5. Hot-plug: attach the Type Cover, type a letter or move the trackpad -> `source TOUCH -> MOUSEKB`; touch the screen -> `MOUSEKB -> TOUCH`.
6. Chat/text: tap a text box -> `osk: show requested` then `window found`, keyboard appears, typed text shows up in the game; close -> `osk: hide`.
7. Fallbacks, in this order if 2/3 fail: `start_mouse.bat` (`--input=mouse`: must behave exactly like the old build), `--touch-api=wmtouch`, `--touch-api=legacy`, `--osk=on`.
8. Experiment lever if the HUD still does not switch although type-1 items are queued: the `DeviceFamily` string (`Windows.Mobile`) - only on request, not part of the first patch.

## 5. Build/verification checklist for the implementer
`cmd /c .\build_arm32.bat`; `python tools/arm32/check_x86.py` (all new code and the new `case` labels must be inside `#ifdef WIN32CRAFT_ARM32`); `python tools/arm32/gen_constants.py` after copying `touch_host.json` into place;
`python tools/arm32/verify_constants.py`; `python tools/make_arm32_device_package.py` last. Use the Edit/Write tools (backslashes in raw strings such as `L"\\microsoft shared\\ink\\TabTip.exe"`).

## 6. Risks and open questions
1. **What sets InputMode = Touch (2) in the game** was not traced (field `ScreenController+0x24c`, setter 0x1FAD80, caller lambda 0x1FAF42, producer of the mode change event). Expectation: type-1 items from a non-mouse source are what the UWP build produces, and the stock mouse branch of
   the pressed handler (0x624CEC) also emits `(1,1)` Multitouch items, so type-1 items alone may not be the trigger; if the HUD does not switch after step 3 the missing piece is game-side (e.g. a `ClientInputHandler` rule that watches Multitouch state arrays at 0x019159BC, or a
   check of HIDController+0xa0/0x25). Investigator A's result decides; the host design above is independent of it.
2. Semantics of the `(buttonId,data)` pairs `(1,1)` press, `(0,0)` move, `(1,0)` release come from the three stock handlers and `Multitouch::feed`; coordinate unit = buffer pixels (same as MouseItem) because the stock path multiplies DIPs by the adapter scale (`sub_627f28`). Medium-high confidence, proven by the first device log.
3. DefWindowProc suppression of emulated mouse messages for handled touch pointers is documented Windows behaviour but unverified on RT 8.1; the `legacy_from_touch` counter and the extra-info filter make a wrong assumption harmless (messages are dropped when touch handling is active).
4. `IPTip_Main_Window` / `TabTip.exe` path on Windows RT 8.1 and the TIP COM interface are known from desktop Windows 8-10; first log decides. If the window is not found the game stays usable (typing via a hardware keyboard).
5. The 1280x720 resizable framed window on a 1366x768 screen (title bar + taskbar) is awkward for touch; a borderless/maximized start for touch-only devices is a separate UX item (the x86 `win32_set_fullscreen_mode` is not used on ARM).
6. Mixed use (cover attached, user touches and types alternately) flips the HUD each time by design; if that is annoying a hysteresis (e.g. 1.5 s) can be added to `arm32_input_note_source` without changing the architecture.
7. The mouse pointer branch of the pressed handler emitting `(1,1)` Multitouch items when `HIDController+0xa0 != 0` means the stock game produces both item kinds for mouse clicks while captured; the host's mouse path (Mouse items only) has always differed from that and is not touched by this design.
