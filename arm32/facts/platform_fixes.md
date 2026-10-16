# platform_fixes (1.2.8 -> ARM32/Thumb-2)

Scope: custom-skin picker hook, ZIP/directory pack getAsset override (lang + start_screen.json), Xbox sign-in string patches, TCUI IAT no-ops, and sweep of other `host_is_128` game-code mutations. All RVAs are ARM image RVAs (even code address; stored pointers = ImageBase 0x400000 + RVA + 1 in the file, relocated at load).

## What the 1.2.8 x86 host really does in this area
1. `install_win32_platform_fixes_128`: ONE hook only (pickFile). No fullscreen/keyboard/help-URL hooks on 1.2.8.
2. `install_128_zip_resource_overrides`: swaps the same getAsset pointer in two vtable cells; wrapper rewrites `.lang` (menu.copyright / development_version) and `start_screen.json` in the returned std::string.
3. `patch_xbox_actions_noop` (128 branch): two 1-byte string patches ('b' -> '!').
4. `install_xbox_tcui_noops_128`: three IAT cells.
5. Nothing else on the 1.2.8 path mutates/calls game code outside gfx/appmain/input/client_state/imports/winmain (verified by grep).

## ARM map
| x86 item | ARM | notes |
|---|---|---|
| RVA_128_PICK_FILE 0x7d72e0 | **0x005830F8** AppPlatform_Winrt::pickFile (vtable 0x013695A4 slot 35) | patch **vtable cell 0x01369630** (preferred) or function entry; bytes `2de98048 0df1040b 8db0 6f46` |
| platform+0x3b4 adapter, adapter+0xa8 callback, callback vtable[2] | **unchanged** (0x3b4, 0xa8, slot 2) | confirmed in pickFile, ctor, cancel/success lambdas |
| RVA_128_ZIP_GET_CONTENTS 0x90bf40 | **0x0064E6B8** ZipPackAccessStrategy::vf11 = getAsset(path,out,trusted) | AAPCS r0..r3 |
| ZIP_GET_CONTENTS_SLOT 0x157bad4 | **0x0136E598** (ZipPackAccessStrategy[11]) | file value 0x00A4E6B9 |
| ZIP_OWNING_GET_CONTENTS_SLOT 0x157ba84 | **0x0136E5E8** (ZipPackAccessStrategyOwningFileAcccess[11]) | same pointer; order of vtables reversed vs x86 |
| (new) directory strategy | **0x0136DF2C** (DirectoryPackAccessStrategy[11]) -> 0x006314BC | ARM package ships loose files, no zips: this is the likely live path; same signature |
| RVA_128_STRING_ASSIGN 0x85f20 | **0x00084048** std::string::assign(const char*,size_t) | returns this; 21860 callers |
| std::string layout (16 inline, len +0x10, cap +0x14, 0x18 bytes) | **unchanged** | cap<16 => inline |
| Xbox string patch 0x12b64b4 / 0x12bfbcc ('b'->'!') | **0x00D99E28** "button.signin", **0x00DA345C** "button.xbl_signin" | expected 0x62; use PAGE_READWRITE |
| TCUI cells 0x17243fc/400/408 (order ProcessPending, ShowProfile, null, ForUser) | **0x015121FC ShowProfileCardUI, 0x01512200 ProcessPendingGameUI, 0x01512204 null (leave), 0x01512208 ShowProfileCardUIForUser** | order differs: index by name |

## No longer needed / unused on ARM
- `write_relative_jump` (E9 rel32) for this area: use data-cell patching. `thiscall` typedefs -> plain AAPCS C.
- 0.15.10/1.1.5 only: platform+0x1b0, picker/keyboard/fullscreen command hooks, `return_handled` Xbox code patches, `patch_chat_textbox_compat`, copyright/start-screen temp-file + fopen hooks and `Win32CraftPrepare*` exports.
- Windows-7-only: `patch_win7_discard_views`, DXGI Device3 IID downgrade.
- `RVA_128_DEBUG_*`, `RVA_128_FONT_*`, `RVA_128_COLOR_WHITE`, `RVA_128_DEVELOPMENT_VERSION_GETTER`: only `#define`d, never referenced. `RVA_APP_PLATFORM_GLOBAL` / `RVA_DEVICE_RESOURCES_GLOBAL`: 0.15.10-only code.
- Text rewriters and the options.txt / custom.png logic are portable C; ARM data files (en_US.lang, start_screen.json) and exe option keys match what the host expects.

## Caveats
- Prefer PAGE_READWRITE for every patch (all targets are data); avoid PAGE_EXECUTE_READWRITE.
- Read the original getAsset pointer from the cell (carries the Thumb bit); keep separate originals for zip (0x64E6B8) and directory (0x6314BC).
- See `open_questions` in the JSON (UI JSON loader slot not traced; which strategy serves vanilla at runtime).
