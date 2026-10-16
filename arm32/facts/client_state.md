# client_state (ARM32, Minecraft 1.2.8) - ClientInstance / scene / keyboard state

All RVAs are image-relative. Code pointers stored in data carry the Thumb bit (+1). Machine-readable version: `client_state.json` (25 facts).

## Constants
| What | x86 | ARM32 | Same? |
|---|---|---|---|
| ClientInstance vtable (primary) | 0x01549354 | **0x0133a758** | no |
| ClientInstance second vtable ([client+4]) | primary+0x20 | **0x0133a778** (= +0x20) | relation yes |
| ClientInstance::vf3 (hooked slot 3) | 0x0015a080 | **0x0011a280** (cell 0x0133a764 holds image+0x11a281) | no |
| LevelRenderer vtable ([lr+0]) | 0x01570574 | **0x013636c8** (primary; NOT 0x1363694) | no |
| MinecraftKeyboardManager vtable | 0x01569eac | **0x0135cfcc** | no |
| UIScene vtable | 0x015684b8 | **0x0135b5d8** | no |
| getTopScene (comment only) | 0x0015c740 | 0x0011be24 | no |
| keyboard show/hide (comment only) | 0x4e06d0/0x4e07d0 | 0x00395504 / 0x00395620 | no |

Region delta check: 0x1569eac-0x135cfcc = 0x15684b8-0x135b5d8 = 0x1570574-0x1363694 = 0x20cee0, which independently corroborates the keyboard and UIScene vtables. Caution: for LevelRenderer the delta lands on the offset-4 subobject vtable (ARM orders the pair secondary-then-primary); the ARM ctor writes 0x13636c8 at [0], so use that.

## Offsets (all coincide with x86)
* ClientInstance: +0x04 second vtable, +0x18 owner MinecraftGame*, +0x28 unique_ptr<LevelRenderer> (NULL when no world; created by 0x11b974 from InGamePlayScreen::vf1, cleared by 0x118b58), +0x44 unique_ptr<MinecraftKeyboardManager> (0x68-byte object), +0x2c0 unique_ptr<SceneStack>. **sizeof = 0x378** (deleting dtor `operator_delete_sized(p,0x378)`; make_shared block 0x388 = 0x10 + 0x378), so `IsBadReadPtr(client,0x378)` is exact.
* MinecraftGame: +0x6c unique_ptr<SceneStack> (size 0xc0).
* SceneStack: std::vector<shared_ptr<AbstractScene>> begin@+0, end@+4, element stride 8 ({ptr,ctrl}); top = `*(end-8)`. Same layout (4-byte pointers).
* MinecraftKeyboardManager: byte +5, bit 1 (mask 2) = text focus. show (slot 1) sets it (also when no on-screen keyboard), hide (slot 3) clears it, slot 5 reads it.

## UIScene
The compared class is `UIScene` itself (JSON-driven scene; the HUD is a UIScene built from `hud.hud_screen` with a HudScreenController; there is no HudScreen scene class). Exact-vtable compare = type guard, since slots 41/42 are only implemented by UIScene (AbstractScene has _purecall there; no subclass inherits them).
* slot 41 (+0xa4) `UIScene::is_showing_menu` 0x0036505c: `uint32 f(UIScene* r0)` -> flag bit 4.
* slot 42 (+0xa8) `UIScene::should_steal_mouse` 0x003650f4: same signature -> flag bit 6. (slot 40 is_modal, bit 5, unused.)
* JSON parser 0x17e66c fills the byte: b0 screen_not_flushable, b1 always_accepts_input, b2 render_game_behind, b3 absorbs_input, b4 is_showing_menu, b5 is_modal, b6 should_steal_mouse, b7 low_frequency_rendering.

## Render hook ABI (important)
`ClientInstance::vf3` = `push {r0-r3}` prologue, reads **r2 and r3** (two ints passed to every scene's vtable slot 12 and compared for equality), **r1 is never read**, no stack args, returns void. The x86 hook forwards exactly 2 stack args; the ARM hook must be
`void hook(void* self, uintptr_t r1, uintptr_t r2, uintptr_t r3)` and call the original through the Thumb pointer `image+0x11a280+1` with all four registers. No thiscall attribute. Slot validation: `*slot == image+0x11a280+1`, slot = `(void**)(image+0x133a758)+3`; cell is in `.data`.

Expected bytes: vf3 `0fb42de9f0490df1140b`; show `2de9f0490df1140b87b0`; hide `2de918480df1080b0446`; UIScene slots 41/42 `2de990480df1080b9cf4`.

## Not found / open
* Virtual call site of slot 3 not located; x86-vs-ARM arg-count difference (2 stack args vs r1(unused)+r2+r3) unexplained but harmless when all four registers are forwarded.
* The hud_screen.json default `should_steal_mouse=true` is an asset, not in the exe (flag plumbing verified).
* No byte patches, IAT cells or x86 machine code exist in this area; the only patch is the single vtable cell write.
