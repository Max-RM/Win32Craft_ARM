# chat_xbox (1.2.8 -> ARM32/Thumb-2)

Scope: chat text box compat, Xbox sign-in neutralisation, TCUI no-ops, exception trace, and a full catalogue of every x86 byte/pointer patch on the 1.2.8 path.
Details and evidence: `chat_xbox.json` (29 facts: 19 high, 9 medium, 1 low). All RVAs are ARM image RVAs (even code addresses).

## Xbox sign-in neutralisation (patch_xbox_actions_noop, 1.2.8 branch)
x86 changes the first character 'b' -> '!' of two shared event-name strings. ARM has **exactly one copy of each string** (whole-image scan, ASCII and UTF-16):

| string | ARM RVA | bytes (expected) | registrations (all 5 are string_to_event_id + registerEventHandler) |
|---|---|---|---|
| `button.signin` (x86 0x012b64b4) | **0x00D99E28** | `62 75 74 74 6f 6e 2e 73 ...` | ItemDetailScreenController fn 0x2044EC (ref 0x2047C2), StoreBaseScreenController fn 0x2A4A44 (ref 0x2A4B24) |
| `button.xbl_signin` (x86 0x012bfbcc) | **0x00DA345C** | `62 75 74 74 6f 6e 2e 78 62 6c ...` | StartMenuScreenController fn 0x299E08 (0x29A038), XblFirstLaunchScreenController fn 0x2D2F4C (0x2D2F84), XblOptionalSigninScreenController fn 0x2D63D4 (0x2D646C) |

Patch = write `0x21` over the first byte (verify `0x62` first). The strings are in read-only `.rdata`: use `PAGE_READWRITE` (not EXECUTE), restore afterwards. Lengths (13 / 17) are hard-coded in the registration code, so the one-byte change is safe. The event-id interning function is 0x1BAB8C (map at controller+0x80), so JSON `button.signin` no longer matches the registered id. No other string must change (checked: `menu_xbox_signin_button` is a name-returning binding, `xbox_signin` a layout name, `button.save_to_xbl` is not patched on x86 1.2.8 either). 11 of the 16 callers of `ScreenController::xblSignIn` (0x21B848) stay live, same as on x86 (optional hardening noted in JSON).

## TCUI no-ops (install_xbox_tcui_noops_128)
Cells live in .data (delay IAT). **ARM order differs from x86**: `0x015121FC ShowProfileCardUI`, `0x01512200 ProcessPendingGameUI`, `0x01512204 null (leave)`, `0x01512208 ShowProfileCardUIForUser`. Assign by name. Only game caller of all three: wrapper **0x00A3E014** (calls at 0xA3E0BE / 0xA3E0AA / 0xA3E0D4). It skips ProcessPendingGameUI+wait unless the Show* HRESULT is >=0 or E_PENDING, so returning E_NOTIMPL keeps the x86 behaviour. The caller chain above the wrapper (0xA3C580 <- 0xA3CC30) has no inbound references in the image, so the no-ops are defensive. Real ARM argument order is (HSTRING, callback, context) (x86 names swapped; ignored anyway).

## patch_chat_textbox_compat
1.1.5-only on x86 (returns immediately for 1.2.8): three `test esi,esi / jz` null guards (20/28/24 bytes at 0x7b1021/0x7b10df/0x7b112c) around C++/CX vtable calls on a XAML TextBox interface. **Not needed on ARM**: the 1.2.8 ARM exe has no XAML at all (no `Windows.UI.Xaml*`/`TextBox`/`ITextBox` strings, ASCII or UTF-16; all 46 `Windows.UI*` strings are Core*/ViewManagement/Text.Core). Text input comes from CoreTextServicesManager (GetForCurrentView helper 0x628248, used by HIDController ctor 0x623C74) plus engine text items - the host keeps providing the fake factory as on x86. InputPane show/hide (vf11 0x5829AC / vf12 0x582A5C, lambdas 0x586CC4 / 0x586F8C, GetForCurrentView 0x57F824) is unchanged vs x86 1.2.8 (no hook); open question in JSON.

## install_exception_trace
IAT cell **0x00C374E4** (`VCRUNTIME140_APP!_CxxThrowException`, one thunk 0xB4A100, ~2900 callers). Same hook logic; log RVAs with Thumb bit cleared. `install_crash_trace`/VEH use x86 CONTEXT registers and an EBP chain walk: rewrite for ARM32 CONTEXT (Pc/Sp/Lr/R0-R12).

## Catalogue of byte/pointer patches reachable on 1.2.8 (source lines)
| x86 (line) | what | ARM replacement |
|---|---|---|
| 12334-12339 patches_128 | 2 string bytes 'b'->'!' | 0xD99E28 / 0xDA345C, 0x62->0x21 (data) |
| 12356-12421 patch_chat_textbox_compat | 1.1.5 only; no-op on 1.2.8 | none needed |
| 13792-13806 install_xbox_tcui_noops_128 | 3 delay-IAT cells | 0x015121FC / 0x01512200 / 0x01512208 |
| 9605-9621 install_exception_trace | IAT 0x11d2690 | 0xC374E4 |
| 12806-12873 install_mouse_mode_hooks | 3x write_relative_jump | cells 0x01369C30 / 0x01369C14 / 0x01369C68 (fns 0x58E93C / 0x58E8F0 / 0x58E88C); fallback trampoline `DF F8 00 F0 <fn\|1>` |
| 13941-13955 install_win32_platform_fixes_128 | 1x write_relative_jump (pickFile) | cell 0x01369630 (fn 0x5830F8); fallback trampoline |
| 8563-8589 patch_128_zip_slot x2 | vtable cells | 0x0136E5E8 / 0x0136E598 (+ dir strategy 0x0136DF2C) |
| 12934-12944 install_client_state_hook_128 | ClientInstance vtable slot 3 | cell 0x0133A764 (orig fn 0x11A280) |
| 376 / 8195 / 8289 / 9702 / 9968 | CreateFile2, activation, vccorlib x5, D3DCompile, D3D11CreateDevice IAT cells | 0xC3765C / 0xC37BF0 / 0xC37B90,7C,80,88,C8 / 0xC370B4 / 0xC37A50 (pe_imports owns) |
| 9916-9958 patch_win7_discard_views, 9870-9914 DXGI Device3 IID | Windows 7 only | never run on ARM; not needed |
| 10024 / 1312 | host-owned DXGI/D3D11 vtable hooks | gfx areas; use PAGE_READWRITE |
| 10778 write_relative_jump | E9 rel32 + 5 NOPs | removed; Thumb trampoline `DF F8 00 F0` + literal (keystone/capstone verified); `bx lr`=`70 47`, `movs r0,#8;bx lr`=`08 20 70 47` |
| 10885 resolve_fmod_delay_imports_128 | fmod delay IAT | fmod_ARM.dll is a normal import; not needed |

Everything else in the source that patches code (0.15.10 / 1.1.5 / trace installers) is unreachable for 1.2.8 (list in `chat_xbox.catalog_not_on_128_path`).

## Names recorded (arm32re.py name)
Registration fns 0x2044EC / 0x2A4A44 / 0x299E08 / 0x2D2F4C / 0x2D63D4, lambda bodies 0x2A4D48 / 0x204A2C / 0x29A410, xblSignIn 0x21B848, profile-card wrapper 0xA3E014 (+0xA3C580, 0xA3CC30), string_to_event_id 0x1BAB8C, InputPane/CoreText helpers 0x57F824 / 0x586F8C / 0x628248, save_to_xbl 0x1CD44C / 0x1CD518.
