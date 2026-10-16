# Input injection and mouse-mode handling (Minecraft 1.2.8, ARM32/Thumb-2)

Machine-readable details: `input.json` (32 facts, 28 high / 4 medium). All RVAs are exe RVAs, even code addresses
(call them as `image + rva + 1` for Thumb). Every function below has been given a label with `arm32re.py name`.

## Object graph (offsets all unchanged from x86)
```
AppMain(MCPE_Host)  +0x1c -> AppPlatform_Winrt          (ctor 0x57ceec, factory 0x57fc30 -> ctor 0x581610, size 0x428)
AppPlatform_Winrt   +0x3b4 -> InputAdapter (0x170 bytes) (made by 0x58530c / 0x586b00, +0 CoreWindow ref, +8 mutex, +0x38 vector<unique_ptr<HIDItem>>)
InputAdapter        +4    -> HIDController               (ctor 0x623c74, >= 0x1a48 bytes)
HIDController       +0x10 -> InputAdapter (== "input handler" = EnqueueInput this)
HIDController  +0x28 int command (0/1/2)   +0xa0 byte "actual" relative flag
               +0xc9..+0x1c8 held-key table   +0x1f0 text sequence byte   +0xc4/+0xc6 shorts last x/y
```
Only change: the x86 `IsBadReadPtr(input_handler, 0x1d0)` must become `0x170` (ARM object is exactly 0x170 bytes).

## Function ABIs (all plain AAPCS, no naked bridges needed; stack args are only the 5th/6th mouse args)
| role | x86 RVA | ARM RVA | ARM signature |
|---|---|---|---|
| EnqueueInput | 0x7dd0c0 | **0x00587294** | `void(void* adapter r0, void** slot r1)`; **r1 is the address of a pointer slot** (unique_ptr by reference); callee moves it out and zeroes it |
| Mouse factory | 0x8d6500 | **0x0062a128** | `void**(void** out r0, const int* action r1, const int* pressed r2, const int* payload r3, const short* x [sp], const short* y [sp+4])`; item 0x14 bytes, type 0 |
| Key factory | 0x8d65e0 | **0x0062a254** | `void**(void** out r0, const int* key r1, const int* state r2)`; item 0x10 bytes, type 2 |
| Text factory | 0x8d6670 | **0x0062a2b4** | `void**(void** out r0, const MsvcSmallString* s r1, const BYTE* primary r2, const BYTE* seq r3)`; item 0x24 bytes, type 3 |
| MapVirtualKey | 0x8d1720 | **0x00626d10** | `unsigned(unsigned vk_minus_8 r0)`, `tbh` table, valid 0..0xd6 (VK 8..0xde) |

Width fix: the mouse factory reads action/pressed/payload with 32-bit loads and x/y with 16-bit loads, the key factory reads
key as 32-bit. Pass int/short locals, not BYTE pointers (x86 got away with it by little-endian over-read).
Std::string layout (16-byte SSO, size +0x10, cap +0x14) is identical, so `MsvcSmallString115` stays valid.
Wheel = mouse factory with action 4, pressed = signed byte delta, payload 0, x=y=0 (same as stock handler 0x625a50).
Stock handlers confirm everything: onKeyUp 0x625d64, keyDown 0x627188, queueTextItem 0x627108, onCharacterReceived 0x625e50,
pointer handlers 0x62499c / 0x626570 / 0x625a50.

## Mouse-mode hooks
The three x86 targets are `std::function<void()>::_Do_call` bodies of lambdas; `this` is the lambda object
(`+4` = adapter, adapter `+4` = controller, same as x86 `get_mouse_mode_controller`).

| hook | x86 RVA | ARM fn RVA | len | vtable slot-2 cell (in .data, RW) | semantics |
|---|---|---|---|---|---|
| toggle | 0x7e8260 | **0x0058e88c** | 44 | 0x01369c68 (= image+0x58e88c+1) | flip actual; command = actual ? 2 : 1 |
| absolute | 0x7e82c0 | **0x0058e8f0** | 18 | 0x01369c14 (= image+0x58e8f0+1) | command = 2 |
| relative | 0x7e8300 | **0x0058e93c** | 24 | 0x01369c30 (= image+0x58e93c+1) | if actual == 0, command = 1 |

Expected bytes (first 16; first 12 of toggle and relative are identical, byte 14 differs):
toggle `2d e9 00 48 eb 46 43 68 5a 68 92 f8 a0 30 33 b9`, absolute `2d e9 00 48 eb 46 43 68 02 22 5b 68 9a 62 bd e8`,
relative `2d e9 00 48 eb 46 43 68 5a 68 92 f8 a0 30 0b b9`.

Hook plan: **A (preferred)** replace the three vtable cells (writable .data, no code patching, safe where RWX is forbidden,
verify `*cell == image+fn_rva+1` first). **B (fallback)** 8-byte Thumb-2 trampoline at the entry: `DF F8 00 F0` (`ldr.w pc,[pc,#0]`) +
`hook|1` literal. All three entries are 4-aligned and at least 18 bytes, so both fit. Replacement signature `void f(void* lambda_self)`
(drop `thiscall`). The AppPlatform_Winrt front-ends are vtable slots 18 (relative, fn 0x582b00), 19 (absolute, 0x582b4c), 22 (toggle, 0x582b98).

## Not needed on ARM
`write_relative_jump` E9+NOP encoding and the 10-byte x86 expected arrays, the four naked asm bridges, thiscall/fastcall typedefs,
all 0.15.10/1.1.5 constants (APP_PLATFORM_HID_OFFSET 0x258, 0x278, 0xb8, 0xbf, 0x1e0, 0x44, key-handler fastcall path), the 0x1d0 probe.
`window_proc` itself has no binary-specific constants.

## Open items
Cross-area: ClientInstance keyboard offset 0x44 / ARM keyboard vtable for `text_input_active` (byte +5 bit 1 read confirmed on ARM);
whether the host dispatcher runs the lambdas synchronously; semantics of stock command consumers 0x627b74 / 0x627c88 (not needed).
