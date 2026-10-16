# gfx_device (ARM32, Minecraft 1.2.8) - D3D/DXGI device bootstrap

All RVAs are RVAs (ImageBase 0x400000). Code addresses are even; **call them with the Thumb bit set** (`image + rva + 1`).
Machine-readable version: `gfx_device.json` (30 facts, 24 high / 6 medium).

## Game functions the host calls (all plain AAPCS, no stack args, no floats)

| x86 symbol | x86 RVA | ARM RVA | ARM signature |
|---|---|---|---|
| operator new (`operator_new_rva`) | 0x010296d1 | **0x00b4929c** | `void* f(size_t r0)` (CRT operator new; matching delete 0xb495b0 / sized 0xb49864) |
| RVA_128_OWNER_CTOR | 0x0082cce0 | **0x005b7e7c** | `void* f(void* owner r0)` returns owner; memory pre-zeroed (0xd0) |
| RVA_128_INIT_DEVICE | 0x0082cff0 | **0x005bc258** | `void f(void* owner r0, void* renderer r1)` |
| RVA_128_INIT_TARGETS | 0x0082cd90 | **0x005bc0cc** | `void f(void* owner r0, Display* d r1, void* renderer r2)` |
| RVA_128_RELEASE_TARGETS | 0x0082c680 | **0x005bbb30** | `void f(void* renderer r0)` |
| RVA_128_DISCARD_WINDOW_VIEWS | 0x007d68e0 | **0x00582970** | AppPlatform_Winrt vtable slot 33 (+0x84, vtable 0x013695a4); `void f(this r0)` |
| RVA_128_OWNER_GLOBAL | 0x017c50f4 | **0x015b86ac** | plain `GameOwner*` cell in .data |
| D3D11CreateDevice IAT | 0x011d2a44 | **0x00c37a50** | thunk 0xa3b968; only called from InitDevice (0x5bc3a0, 0x5bc3ec) |

## Struct offsets (ARM) - all identical to the x86 1.2.8 table

owner: size **0xd0**, renderer@**0xac**, device@**0xc0**; (also featureLevel@0xb8 init 0x9100, flag@0xbc, adapter@0xc4, caps@0x90/0x91, size cache@0x94/0x98).
renderer (size 0x204): context@**0x110**, context2@**0x114**, swapchain@**0x104**, RTV@**0x118** (right-eye RTV 0x11c only if stereo), DSV@**0x120**, backbuffer@**0x124**, owner back-pointer@0x108, embedded display descriptor@0xe8 (so swapchain = 0xe8+0x1c), viewport@0x128, annotation@0x140.
display descriptor passed to InitTargets: `{u32 w@0, u32 h@4, ..., u8 stereo@0x19, IDXGISwapChain* @0x1c}`, sizeof 0x20 - **identical to the x86 struct** (all 32-bit members, no padding differences).
AppMain: platform@0x1c, game@0x20; platform width/height @0x330/0x334; game vtable slot 23 (+0x5c) `resize(game, int w, int h, float scale)` -> **scale is a float in s0**, slot 24 (+0x60) `windowSizeChanged(game,w,h)`.

## Key findings

* Owner creation must mirror the game's own `make_owner` (0x13f2a0): `new(0xd0)`, memset 0, ctor, then `+0xb8=0x9100`. Publishing the pointer in cell 0x15b86ac makes the game skip its own InitDevice/queryAdapterInfo (0x5b9f4c)/AppPlatform vf143 (same as x86). Do not call 0x5b9f4c yourself (it would flag tearing support that the host swap chain lacks).
* InitDevice calls D3D11CreateDevice itself (and retries with WARP itself); keep the IAT hook only for `flags |= 0x20`.
* InitTargets does not store the swap chain; host must store it at renderer+0x104 (as on x86). ReleaseTargets does not release renderer+0x124; host must (as on x86) before `ResizeBuffers`.
* Present: game `Display::present` (0x5bed3c, via AppPlatform_Winrt::vf32 0x582908 with display=renderer+0xe8) calls `Present(1,0)` (vtable slot 8) when `[*(u8**)(image+0x15b86b0)+0x30]==1` (default), else `Present1(0,flags,&params)` (slot 22). Hook both slots; mask ALLOW_TEARING; use PAGE_READWRITE for the vtable patch.
* patch_win7_discard_views / dxgi device3 IID / Device2 captured-base fallbacks: **not needed** (Windows RT 8.1 / Win10 ARM32 have D3D11.2). If ever wanted: DiscardWindowViews entry bytes `2d e9 18 48 0d f1 08 0b`, patch `70 47` (bx lr); IDXGIDevice3 IID cell 0x00dbc2c4.
* Calling-convention changes: remove every `__attribute__((thiscall))`/asm thunk (`call_game_resize` becomes a normal call with a `float` 4th argument).

## Open questions
See `open_questions` in the JSON (vsync selector semantics, owner+0xbc meaning, IDXGISwapChain1 pointer for Present1, scissor fix on ARM, AppMain layout owned by another area).

Names recorded in `tools/arm32/names.jsonl`: gfx:GameOwner::ctor/InitDevice/InitTargets/releaseDevice/dtor, gfx:Renderer::ReleaseTargets/createWindowSizeDependentResources/ctor/dtor, gfx:Display::present/resizeSwapChain, AppPlatform_Winrt::DiscardWindowViews/swapBuffers, AppMain::onWindowSizeChanged, g_owner (0x15b86ac), g_displaySettings (0x15b86b0), crt:operator_new/delete.
