# input_text - Input (pointer/touch/mouse/keyboard/TextBox/back/cursor) - Minecraft PE 0.10.4.40 (WP8.1 ARM32)

Machine-readable facts: `input_text.json` (37 facts: 33 high, 4 medium, 0 low). All RVAs are even code addresses (Thumb bit stripped; IDA VA = 0x400000 + RVA).
Everything below was re-derived in this exe; nothing was taken from the friend's notes. Labels were stored with `arm32re.py name` (HIDController::ctor, HID:*, AppPlatform_Winrt::*, ...).

## 1. TL;DR

* **All input goes through one C++/CX class, `HIDController`** (vtable RVA 0xdb8780, ctor 0x1103fc, object created by the `AppPlatform_Winrt` constructor 0x494c0 and stored at `AppPlatform_Winrt+0x2c`).
  It registers: CoreWindow `KeyDown`/`KeyUp`, pointer `Pressed`/`Moved`/`Released` (either on a `CoreIndependentInputSource` of the SwapChainPanel or on the CoreWindow), XAML `TextBox.TextChanged`.
  MainPage additionally registers `HardwareButtons.BackPressed` and `InputPane.Showing/Hiding`.
* **Nothing is registered until `HIDController::Initialize` (0x111c1c) runs.** It is reached only through `MainPage::OnTick (0x1df8a4)` -> `AppMain::attachUi (0x1da634)` -> `AppPlatform_Winrt::initInput (0x4b55c)`; OnTick fires from a 50 ms `DispatcherTimer` and waits for `AppMain+0x3c` (IsAppInitialized, 0x1da310).
  A host that drives `AppMain` directly and skips MainPage therefore gets **zero input** (no pointer/key/TextChanged delegate exists).
* **The game is touch-only on this build.** `MinecraftClient::useTouchscreen` (0x5a1d0) is always true because `AppPlatform_Winrt` vtable cell 0x33fb88 (slot 26) is a "return 1" stub. The mouse+keyboard `CustomInputHolder` (MouseBuildInput/MouseTurnInput/KeyboardInput) exists but is unreachable.
  A real mouse still works: the pointer handlers emit a left-button press as a touch contact (+ a Mouse event). No wheel, no CharacterReceived.
* **Text input and the on-screen keyboard are driven entirely by a XAML `TextBox` + `IControl::Focus`**: show = `Focus(Pointer)` on the TextBox, hide = `Focus(Pointer)` on a second control (HID+0x30). There is no InputPane.TryShow/TryHide call. The game keeps its own intent flag (`AppPlatform+4`, `isKeyboardVisible` slot 34) and `HID+0x44`.

## 2. Registration inventory (who, what, on which object, handler RVA)

| # | Event | Object / vtable slot (byte offset) | Delegate type (ctor RVA) | Handler (RVA) | Registered by |
|---|---|---|---|---|---|
| 1 | KeyDown | ICoreWindow slot 34 (+0x88) | TypedEventHandler<CoreWindow,KeyEventArgs> (0x10f9cc) | HID::onKeyDown **0x1128b8** | HID::Initialize 0x111c1c |
| 2 | KeyUp | ICoreWindow slot 36 (+0x90) | same | HID::onKeyUp **0x112a30** | Initialize |
| 3 | PointerPressed | thread path: ICorePointerInputSource slot 20 (+0x50); CoreWindow path: ICoreWindow slot 46 (+0xb8) | TypedEventHandler<Object,PointerEventArgs> (0x10fc04) / <CoreWindow,PointerEventArgs> (0x10fae8) | HID::onPointerPressed **0x113010** (CoreWindow path via stub 0x113000) | lambda 0x110940 / Initialize |
| 4 | PointerMoved | slot 18 (+0x48) / ICoreWindow slot 44 (+0xb0) | same | **0x112b10** (stub 0x112b00) | same |
| 5 | PointerReleased | slot 22 (+0x58) / ICoreWindow slot 48 (+0xc0) | same | **0x1136d0** (stub 0x1136c0) | same |
| 6 | TextChanged | ITextBox slot 30 (+0x78) on HID+0x2c | TextChangedEventHandler (0x10fd20) | HID::onTextChanged **0x113d8c** | Initialize |
| 7 | BackPressed | IHardwareButtonsStatics slot 6 (via helper 0x1e1798) | EventHandler<BackPressedEventArgs> (0x1dca8c) | MainPage::OnBackPressed **0x1df4e8** | MainPage ctor 0x1dcf5c |
| 8 | InputPane.Hiding | IInputPane slot 8 (+0x20) | TypedEventHandler<InputPane,...> (0x1dc8bc) | MainPage::OnInputPaneHiding **0x1df700** | MainPage ctor |
| 9 | InputPane.Showing | IInputPane slot 6 (+0x18) | same | MainPage::OnInputPaneShowing **0x1df714** | MainPage ctor |
| 10 | DispatcherTimer.Tick (50 ms) | IDispatcherTimer slot 9 (+0x24) | EventHandler<Object> (0x1dc9a4) | MainPage::OnTick **0x1df8a4** | MainPage ctor |

Not registered anywhere: PointerWheelChanged, PointerEntered/Exited/CaptureLost, CharacterReceived, TouchHitTesting, UIElement routed events, gamepad. (`input_text.not_found_inputs`)

Which pointer path is used is decided by `HID::Initialize` argument r2: **non-NULL SwapChainPanel => thread path** (ThreadPool work item 0x110940: `ISwapChainPanel::CreateCoreIndependentInputSource(7)`, `ICorePointerInputSource` adds, then `ICoreDispatcher::ProcessEvents(2)` blocking that thread); **NULL => CoreWindow pointer events** (no thread). Both end in the same three handlers. MainPage passes MainPage+0x5c (SwapChainPanel), so the shipped behaviour is the thread path.

### Delegate firing convention (same for every delegate above)
`handler` is the pointer given to `add_*`. Fire with `((HRESULT (*)(void*,void*,void*))(*(void***)handler)[3])(handler, sender, args)`.
Slot 3 of vtable0 (0x115508 for the Object/PointerEventArgs handler) forwards r1/r2 into `functor->vf1` (0x112364) which calls `fn(HID, sender, args)`. Sender is unused by all input handlers. AddRef (vtable[1]) a handler before storing it.

## 3. Fake objects the host must provide (exact slots the game calls)

All objects: QueryInterface/AddRef/Release must work (the helper wrappers AddRef/Release every returned interface).

* **CoreWindow** (`Window.Current.CoreWindow`: IWindowStatics {93328409-4ea1-4afa-83dc-0c4e73e88bb1} slot 6 `get_Current`, IWindow {3276167d-c9f6-462d-9de2-ae4c1fd8c2e5} slot 10 `get_CoreWindow`; also `Windows.UI.Core.CoreWindow` statics {4d239005-3c2a-41b1-9022-536bb9cf93b1} slot 6 `GetForCurrentThread`).
  ICoreWindow {79b9d5f2-879e-4b89-b798-79e47598030c}: `get_Dispatcher` 9 (+0x24), `put_PointerCursor` 15 (+0x3c), `add_KeyDown` 34, `add_KeyUp` 36, `add_PointerMoved` 44, `add_PointerPressed` 46, `add_PointerReleased` 48, `add_VisibilityChanged` 56 (+0xe0, MainPage).
  **MUST answer QueryInterface(IAgileObject {94ea2b94-e9cc-49e0-c0ff-ee64ca8f5b90}) = S_OK**: HID holds it in `Platform::Agile<CoreWindow^>` (HID+0x14); without IAgileObject the Agile takes the cross-apartment marshalling path and every keyboard/cursor/text RunAsync wrapper breaks.
* **CoreDispatcher** {60db2fa8-b705-4fde-a7d6-ebbb1891d39e}: `ProcessEvents` 7 (+0x1c; arg 2 on the input thread), `RunAsync` 8 (+0x20: `(priority=0, DispatchedHandler*, IAsyncAction** out)`). Queue the handler and Invoke it (vtable[3]) on the UI thread.
* **SwapChainPanel** (ISwapChainPanel {c589644f-eba8-427a-b75a-9f1f93a11ae9}): `CreateCoreIndependentInputSource` 10 (+0x28, arg 7); MainPage additionally uses get_CompositionScaleX/Y 6/7, add_CompositionScaleChanged 8, IFrameworkElement/IUIElement (other areas).
* **CoreIndependentInputSource**: ICoreInputSourceBase {9f488807-4580-4be8-be68-92a9311713bb} `get_Dispatcher` 6 (+0x18); ICorePointerInputSource {bbf1bb18-e47a-48eb-8807-f8f8d3ea4551} `add_PointerMoved` 18 (+0x48), `add_PointerPressed` 20 (+0x50), `add_PointerReleased` 22 (+0x58). (Only these.)
* **PointerEventArgs** (raw pointer is IPointerEventArgs): slot 6 `get_CurrentPoint(IPointerPoint**)`.
  **PointerPoint** (IPointerPoint): 6 `get_PointerDevice`, 7 `get_Position(float X,float Y)`, 9 `get_PointerId(uint32)`, 13 `get_Properties`. **PointerDevice**: 6 `get_PointerDeviceType(int)` (0 touch, 1 pen, 2 mouse). **PointerPointProperties**: 16 `get_IsLeftButtonPressed(byte)`. Nothing else is read. The same PointerPoint object can be returned for every `get_CurrentPoint` call (called ~6x per event).
* **KeyEventArgs**: slot 6 `get_VirtualKey(int32)` (Win32 VK codes), slot 7 `get_KeyStatus(struct*)` - only `RepeatCount` (first dword) is read; write <= 12 bytes (KeyUp has a 16-byte stack buffer). RepeatCount > 1 is dropped.
* **TextBox** (HID+0x2c; QI-able for): ITextBox {e48f5a8b-1dff-4352-a1f4-e516514ec882}: `get_Text` 6, `put_Text` 7, `get_MaxLength` 14, `put_MaxLength` 15, `put_AcceptsReturn` 19, `add_TextChanged` 30, `Select` 36; ITextBox2 {f7168c00-1432-462a-9405-38f385bfc37c}: `put_PreventKeyboardDisplayOnProgrammaticFocus` 15; IControl {a8912263-2951-4f58-a9c5-5a134eaa7f07}: `Focus` 46 (+0xb8, `(FocusState, bool* out)`); IUIElement {676d0be9-b65c-41c6-ba40-58cf87f201c1} (MainPage only).
* **Focus control** (HID+0x30): IControl `Focus` 46 (+ IUIElement for MainPage). Any Control works; its only job is to take focus away from the TextBox.
* **InputPane** (`Windows.UI.ViewManagement.InputPane` statics {95f4af3a-ef47-424a-9741-fd2815eba2bd} slot 6 `GetForCurrentView`; IInputPane {640ada70-06f3-4c87-a678-9829c9127c28}): `add_Showing` 6, `add_Hiding` 8, `get_OccludedRect` 10 (Rect floats X,Y,W,H; Height read).
* **HardwareButtons** statics {594b8780-da66-4fd8-a776-7506bd0cbfa7} slot 6 `add_BackPressed`; **BackPressedEventArgs**: `put_Handled` slot 7 (+0x1c) (get_Handled slot 6).
* **CoreCursor** factory {f6359621-a79d-4ed3-8c32-a9ef9d6b76a4} slot 6 `CreateCursor(type, id, out)`; any COM object is acceptable as the cursor.
* **ThreadPool** statics {b6bf67dd-84bd-44f8-ac1c-93ebcb9dba91} slot 8 RunAsync(handler, priority, options) - used for the input work item (thread path) and the render loop.

## 4. What the handlers do (pointer + keyboard -> game)

Pointer coordinates: `x = (int16)(Position.X * HID[0x50])`, `y = (int16)(Position.Y * HID[0x54])`. HID+0x50/+0x54 are loaded once in Initialize: thread path = `DX::DeviceResources::get()+0x60/+0x64` (composition scale, written by `SetCompositionScale` 0x1dbbcc); CoreWindow path = `DisplayInformation::LogicalDpi / 96.0`. Use scale 1.0 and pixel positions (or overwrite the two floats after Initialize).

| Handler | Mouse (type 2) | Touch / pen |
|---|---|---|
| Pressed 0x113010 | allocate slot; if left && !HID[0x45]: HID[0x45]=1 + hide cursor (`put_PointerCursor(NULL)` via RunAsync); HID[0x46]=left; queue MOUSE{btn=(left?1:2), down=1}; if left also TOUCH{btn=1, down=1, slot} | MOUSE{btn=1,down=1} + TOUCH{btn=1,down=1,slot} |
| Moved 0x112b10 | MOUSE{btn=0,down=0} + TOUCH{btn=0,down=0,slot} (hover moves included) | same |
| Released 0x1136d0 | if HID[0x45] && !left: HID[0x45]=0 + restore Arrow cursor; MOUSE{btn=(HID46?1:2), down=0}; if HID46 also TOUCH{btn=1,down=0,slot}; erase map entry | MOUSE{btn=1,down=0} + TOUCH{btn=1,down=0,slot}; erase map entry |

Queue (`AppPlatform_Winrt::queueInputEvent` 0x4b290, critical_section at platform+0x50, deque at +0x70) is drained on the render thread by `pumpInput` 0x50eac (called from `AppMain::updateFrame` 0x1da160) via `pollInputEvent` 0x4b300:

| type | size | layout | game call |
|---|---|---|---|
| 0 MOUSE | 8 | s16 x@2, s16 y@4, s8 btn@6, s8 down@7 | `Mouse::feed(btn,down,x,y)` 0x115f84 |
| 1 TOUCH | 10 | same + s8 slot@8 (0..11) | `Multitouch::feed(btn,down,x,y,slot)` 0x116444 |
| 2 KEY | 8 | u8 key@1, s32 down@4 | `Keyboard::feed(key,down)` 0x4e568 |
| 3 TEXT | 0x20 | std::string@4 (MSVC120 layout), u8 flag@0x1c | `Keyboard::feedText(str,flag)` 0x4e598 |
| 4 BACK | 1 | - | `MinecraftClient::handleBack(false)` (vtable slot 24, 0x575bc) unless the current screen is `Touch::StartMenuScreen` |

If the host injects events itself it must use msvcr120_app `operator new` (events are freed with `operator delete`); preferred is to invoke the delegates with fake args instead.
PointerId -> slot map: `std::map` at HID+0x48 (`allocPointerSlot` 0x1111fc, smallest free slot); 12 slots max. **Always send Released for every Pressed** (cancelled touches too) or slots leak.
Cursor: only `CoreWindow.PointerCursor` is touched (NULL on LMB down, Arrow `CoreCursor(0,0)` on release); no pointer capture.

Keyboard (CoreWindow KeyDown/KeyUp), ignored completely while `HID[0x44]==1`:
* VK_SHIFT (0x10) toggles a static byte (RVA 0xde2fef). Mapped keys (identity for Back 8, Enter 0xD, Esc 0x1B, Space 0x20, arrows 0x25-0x28, F1-F12 0x70-0x7B; PageUp 0x21<->PageDown 0x22 swapped; 0xA0 -> 0x10) become KEY events (RepeatCount <= 1 only).
* Every other VK becomes a TEXT event with `(char)vk`, lowercased unless the shift byte is set (so only letters/digits are meaningful; OEM punctuation would be garbage).

## 5. Text input and the on-screen keyboard (the TabTip chain)

1. Game TextBox widget gains focus (`TextBox::vf19` 0x68d98) -> `AppPlatform_Winrt::showKeyboard(const std::string& text, int maxLen, bool limit)` (vtable slot 31, 0x4fac4) -> `HID::ShowKeyboard` (0x114220) -> `CoreWindow.Dispatcher.RunAsync(lambda 0x110be8)`; and base `AppPlatform::showKeyboard` (0x47a00) sets **platform byte +4 = 1**.
2. UI-thread lambda 0x110be8: `HID+0x40 = text`; `TextBox.MaxLength = (limit && n>0) ? n+1 : 50000`; `TextBox.Text = text`; `TextBox.Select(len,len)`; `IControl::Focus(FocusState=3 Pointer)` on the TextBox; `HID[0x44] = 1`. At Initialize the game already set `PreventKeyboardDisplayOnProgrammaticFocus=false` and `AcceptsReturn=true` and subscribed `TextChanged`.
3. **Host: this `Focus(Pointer)` on the TextBox is the "show TabTip" moment.** After the keyboard appears, set the fake InputPane `OccludedRect` and invoke the Showing delegate (0x1df714 reads `get_OccludedRect().Height` and calls `AppMain::SetKeyboardHeight` -> `MinecraftClient::setKeyboardHeight` which moves the UI).
4. Typing: the host edits the fake TextBox text (WM_CHAR append, Backspace trim, Enter -> `L'\r'`) and Invokes the TextChanged delegates. `onTextChanged` (0x113d8c) ignores the call if `HID[0x44]==0`; otherwise it re-reads `ITextBox::get_Text`, diffs against `HID+0x40` (common prefix; each old char after the prefix = one Backspace key event, each new char = text/Enter key event) and re-mirrors the text. `updateTextBoxText` (slot 33, 0x50f2c) sets `HID[0x58]=1` so the echo of the game's own `put_Text` is ignored.
5. Hide: `TextBox::vf20` -> `AppPlatform_Winrt::hideKeyboard` (slot 32, 0x4edac): `platform+4 = 0`, then lambda 0x1108d4: `HID[0x44]=0; Focus(Pointer)` on the control at HID+0x30. **There is no InputPane hide call** - the keyboard is supposed to close only because the XAML TextBox lost focus. On the host this Focus on the second control is the "hide TabTip" moment, followed by the InputPane Hiding delegate (height 0).
6. The game's own idea of visibility never waits for InputPane events: `isKeyboardVisible` (slot 34, 0x47740) returns `platform+4`.

Suggested TabTip handling (for the show-but-won't-hide / won't-type issue): make the host level-triggered. Each UI tick compare the desired state (`AppPlatform_Winrt+4`, backed by `HID+0x44`) with the real TabTip state and issue show/hide only on a real mismatch, with a debounce (~150-300 ms) and never from inside the game thread; derive the OccludedRect from the actual TabTip window/`IFrameworkInputPane::Location` and raise Showing/Hiding only after the change took effect. Because the game never calls an explicit hide, a host that only reacts to "InputPane.TryHide" calls (which never come) will leave the keyboard up.

## 6. Back button
`HardwareButtons.BackPressed` -> `MainPage::OnBackPressed` (0x1df4e8) -> `AppMain::HandleBack` (0x1da248): not on the start menu => queue BACK event, `args.put_Handled(true)`; on `Touch::StartMenuScreen` => `put_Handled(false)` (OS default: leave app). Host: map Esc / Alt+Left / Browser-Back to Invoke(handler, NULL, fakeBackArgs{put_Handled}); if Handled stays false do the default.

## 7. Ordered host steps (delivery plan)
1. Provide activation factories/fakes listed in section 3 (CoreWindow incl. IAgileObject, dispatcher, SwapChainPanel + input source, TextBox, focus control, InputPane, HardwareButtons, CoreCursor, ThreadPool, DispatcherTimer).
2. Let the game's own `MainPage` code run (preferred) so `OnTick -> attachUi -> initInput -> HID::Initialize` executes once `AppMain+0x3c` is set; or call 0x1da634/0x4b55c/0x111c1c yourself. Before that, make sure `DX::DeviceResources` exists (`get()` throws otherwise) and its composition scale (+0x60/+0x64) is what you want for pointer scaling.
3. Capture every delegate passed to `add_*` (pointer x3, key x2, TextChanged, BackPressed, InputPane x2).
4. Win32 -> WinRT mapping: WM_POINTER/mouse/touch -> fake PointerEventArgs (device type 0/2, PointerId, DIP position, IsLeftButtonPressed) -> Invoke Pressed/Moved/Released; WM_KEYDOWN/UP (VK direct, RepeatCount from lParam) -> fake KeyEventArgs; WM_CHAR/Backspace -> fake TextBox while HID[0x44]==1; mouse cursor requests -> Win32 SetCursor/ShowCursor.
5. Run the fake dispatcher queue on the Win32 UI thread; do not block the game thread inside RunAsync.
6. Nothing to do for `pumpInput`: it runs in the game's own frame update.

## 8. Not found / uncertain
* Default of `Options+0x276` (USE_TOUCHSCREEN) - only matters if someone patches the touch-only decision (cell 0x33fb88 -> stub 0x29ba0 returning 0).
* Who arms the `AppPlatform_Winrt+0x48/+0x4c` GetTickCount64 deadline that makes `pumpInput` call `StatusBar` (sub_4aeb8 / vtable slot 7) - probably a phone status-bar hide; a no-op fake is expected to suffice.
* Rest of `AppMain::attachUi` (0x1da634) (sizes/positions of the XAML elements) was not decoded; only the HID hand-off is certain. `sub_1dfab8` applied to the TextBox/focus elements in the MainPage ctor (probably opacity/size) was not decoded.
* Text diff internals (max-length branch, exact Enter/LF handling in 0x111894/0x1119e0) read at control-flow level only (medium).
* Behaviour of `Platform::Agile` with a non-agile fake was not run; recommendation (answer IAgileObject) follows from the code structure.
* No evidence of any desktop/keyboard-only code path being selected at runtime other than the useTouchscreen cell; no gamepad support at all.

## 9. RVA index (labelled in tools/arm32/names)
HID class 0xdb8780 (vtables) / ctor 0x1103fc / Initialize 0x111c1c / onPointerPressed 0x113010 / Moved 0x112b10 / Released 0x1136d0 / onKeyDown 0x1128b8 / onKeyUp 0x112a30 / onTextChanged 0x113d8c / mapVirtualKey 0x1127cc / allocPointerSlot 0x1111fc / text diff 0x1115b0 (0x1119e0 backspaces, 0x111894 chars) / ShowKeyboard 0x114220 / HideKeyboard 0x111b20 / UpdateText 0x114410 / lambdas: input thread 0x110940, show 0x110be8, put-text 0x11079c, arrow cursor 0x110868, hide-focus 0x1108d4, no-cursor 0x112250 / Agile ctor 0x114194, resolve 0x1112f4 / CoreWindow statics 0x111420 / CoreCursor helper 0x1102e8 /
AppPlatform_Winrt ctor 0x494c0, vtable 0x33fb20 (slot26 cell 0x33fb88, slots 31/32/33/34 = 0x4fac4/0x4edac/0x50f2c/0x47740), queue 0x4b290, poll 0x4b300, pumpInput 0x50eac, initInput 0x4b55c /
MainPage ctor 0x1dcf5c, OnTick 0x1df8a4, OnBackPressed 0x1df4e8, InputPane 0x1df700/0x1df714, AppMain::attachUi 0x1da634, HandleBack 0x1da248, SetKeyboardHeight 0x1da5b4, updateFrame 0x1da160 /
Game: InputHolder factory 0x56420, useTouchscreen 0x5a1d0, client init 0x57b84, Mouse::feed 0x115f84, Multitouch::feed 0x116444, Keyboard::feed 0x4e568, Keyboard::feedText 0x4e598, setKeyboardHeight 0x59578, handleBack 0x575bc.
