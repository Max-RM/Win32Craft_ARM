# Minecraft 1.16.40.02 ARM32 port - scope, architecture and work breakdown

Status: planning document for the reverse-engineering (RE) round. Written before any area report exists. Read together with
`README_V116.md` (layout, how to run the toolkit for this exe), `../RE_GUIDE.md` (toolkit, ABI, output contract) and `../PORT_RULES.md` (decisions).
Everything marked "lead" below was observed with the toolkit on `Minecraft_1.16.40.02\Minecraft.Windows.exe` while writing this scope; a lead is NOT a
fact (needs >= 2 independent indicators in the area report before it becomes an `arm_rva`).

## 1. Goal

Run Minecraft Bedrock 1.16.40.02 (ARM32 UWP, build 2020-09-03) as a Win32 desktop process on Windows RT 8.1 / Windows 10 ARM32 at Direct3D feature
level 9_1, with the same `win32craft.dll` that already runs 1.2.8 ARM32. Finally ONE table-driven patcher must patch both builds (and be extensible to
other ARM32 builds below 1.16.40 - see 2.4).

## 2. Architecture decision (restated, do not re-litigate; flag contradictions in the area report)

### 2.1 One DLL, runtime dispatch
* ONE `win32craft.dll` (ARM) serves 1.2.8 and 1.16.40.02. `Win32Bootstrap()` identifies the game from the image it runs in: PE machine `0x01C4` and
  `SizeOfImage` (the patcher appends one import section, so patched = original + 0x1000: 1.2.8 -> `0x01731000` (existing `IMAGE_SIZE_128_ARM_PATCHED`);
  1.16.40.02 original `0x02BD9000` -> expected patched `0x02BDA000`, to be fixed by the patcher fact). Optionally the patcher also stores a small profile
  marker (magic, profile id, original SizeOfImage, TimeDateStamp) in the new section so unknown future builds can be recognised without recompiling the DLL.
* x86 behaviour and x86 code stay byte-identical (`tools/arm32/check_x86.py`). All ARM code stays under `#ifdef WIN32CRAFT_ARM32`.
* 1.2.8 ARM code is not touched by this port except where a shared helper has to become version-aware (see 2.5).

### 2.2 Files and names
| item | name |
|---|---|
| ARM 1.16 host code | `win32_host_arm32_116.inl`, included from `win32_host.c` under `#ifdef WIN32CRAFT_ARM32` (next to where `win32_host_116.inl` is included for x86) |
| generated constants | `win32_host_arm32_v116.h`, prefix `ARM116_`, macros `ARM116_CODE/ARM116_DATA` (see README_V116.md for the gen_constants command) |
| facts | `arm32/v1_16_40/facts/<area>.json/.md` (+ `.verdicts.json` from the verification round) |
| ARM bootstrap entry for 1.16 | `bootstrap_arm32_116(image, instance)` chosen by `Win32Bootstrap` (ARM variant) after `arm32_boot_identify_image` learns a second profile |
| patcher | `tools/arm32-patcher.py`, one script, profiles for 1.2.8 and 1.16.40.02 (see 2.4) |

### 2.3 Which control flow: follow the x86 1.16 host, not the 1.2.8 ARM host (default; RE may overturn it with evidence)
* 1.2.8 (ARM and x86): the host constructs `AppMain` itself through a game factory, creates the swap chain and calls `GameOwner::InitDevice/InitTargets`
  itself, injects `HIDItem`s, and runs its own frame loop.
* 1.16 (x86 host, `win32_host_116.inl::bootstrap_116`): the host does almost none of that. It creates the HWND, installs hooks, then calls the game's REAL `main`
  (`_main` -> `Platform::Details::GetCmdArguments` -> user main -> `CoreApplication::Run(IFrameworkViewSource)`). `CoreApplication` is a fake factory whose
  `Run` (`coreapp_run_116`) calls `IFrameworkViewSource::CreateView`, then drives `IFrameworkView::Initialize / SetWindow(fake CoreWindow) / Load / [Activated
  event] / Run` itself. The game builds its own `DX::DeviceResources`, calls `D3D11CreateDevice` (hooked: BGRA flag, and the host creates an HWND swap chain
  and drops it into the renderer's swap-chain slot so the game's own CoreWindow swap-chain code is skipped), registers its input handlers on the fake
  CoreWindow (PointerMoved/Pressed/Released/Entered/WheelChanged, KeyDown/Up, CharacterReceived, SizeChanged, Activated, VisibilityChanged,
  `MouseDevice.MouseMoved`) and the host feeds them with fake `PointerEventArgs`, `KeyEventArgs`, ... objects. The frame loop is the game's own
  `FrameworkView::Run`; the host hooks the single call site where it invokes the AppMain virtual slot `+0x68` (render=true) to run `Present` and refresh
  mouse capture after each frame.
* The 1.16.40 ARM exe has the same shape (leads: `DX::DeviceResourcesWinRT` RTTI, `HIDControllerWinRT`, `TypedEventHandler<CoreWindow,PointerEventArgs>`,
  CoreApplication activation sub_909260, `main` wrapper with `GetCmdArguments` at 0x1454B80). Therefore the ARM 1.16 host reuses the x86 1.16 design with
  ARM-derived numbers. The 1.2.8 ARM building blocks (IAT/cell patching helpers, FL9_1 clamp, HSTRING-to-combase forwarding, activation dispatcher, touch via
  WM_TOUCH/MultitouchItem, loadcheck/app-local runtime, logging) are reused as they are.
* ARM-specific differences that always apply: genuine desktop `vccorlib140.dll`/combase (real HSTRINGs, so none of the Wine-era vccorlib shims), AAPCS
  calling (no thiscall/naked asm), Thumb bit on code pointers stored in data, no Windows 7 / Wine branches (`host_is_windows7` stays FALSE), IAT cells
  resolved BY NAME.

### 2.4 Patcher (built last): both versions now, more versions later
* One `tools/arm32-patcher.py`. A profile is selected by the input exe (SHA-256 first, then (machine, TimeDateStamp, SizeOfImage) as a hint). A profile is
  DATA: expected hash/size, DLL rename list, api-set redirect list, import-name redirects (e.g. `XINPUTUAP.dll`, `chakra.dll`), WinMain call-site RVA +
  original/patched bytes + trampoline RVA (or discovery rule), byte-patch list (`rva`, `expected`, `new`, `reason`), companion DLL patches (fmod_ARM.dll).
* Generic engine (shared by all profiles): read PE, rewrite the import directory into a new `.d3imp` section (adds `win32craft.dll!Win32Bootstrap`, can add
  more host imports), rename DLL names, clear APPCONTAINER, bump SizeOfImage / SizeOfInitializedData, add a Thumb `bl` trampoline in `.text` slack, verify
  every original byte before writing (`checked_write`), refuse to patch twice, print a verification report.
* "Any other version below that": a profile for an unknown ARM32 build is produced by RUNNING the same discovery rules instead of hard-coded RVAs: WinMain call
  site = the `bl` that follows `_get_narrow_winmain_command_line` in the CRT's `__scrt_common_main_seh`; DLL renames and api-set redirects are derived from the
  import names; image identification uses the marker. What stays version specific (and needs a new RE round per build) is everything the host calls INSIDE the
  game (RVAs, offsets). The patcher therefore only promises "patch + marker" for unknown builds, plus a clear message that the DLL has no profile for it.
  The area `winmain_patcher` must design the profile format and the discovery rules with this in mind; `pe_imports` supplies the redirect tables.

### 2.5 Shared-code integration notes (for the integration step after the RE round)
* `host_is_116` is consulted in shared code (win32_host.c lines ~3525-3560 CoreWindow/Dispatcher QI, ~5075 AudioGraph completion suppression, ~6144 message
  pump of 256 messages, ~9390 activation_116, ~12323 D3D11CreateDevice swap-chain injection, ~21273 paste). The ARM build currently forces `host_is_128 = TRUE`,
  the others FALSE (`install_arm32_iat_hooks`, `Win32Bootstrap`). A per-process game profile (`arm32_game_profile = {128, 116}`) is needed so the 1.16 path sets
  `host_is_116 = TRUE`, `host_is_128 = FALSE`. Those shared sites are listed in the area briefs that depend on them.
* On ARM the `*_116` functions in win32_host.c are inert stubs (`#ifdef WIN32CRAFT_ARM32` block ~line 5998) because `win32_host_116.inl` is not compiled there;
  the ARM 1.16 inl must replace those stubs (rename or `#undef`) for the 1.16 profile.

## 3. What the x86 1.16 host needs from the GAME BINARY (inventory; x86 values are semantics only)

| # | requirement | x86 1.16.20.03 value / shape | ARM 1.16.40.02 lead (unverified) | area |
|---|---|---|---|---|
| 1 | image id | patched `SizeOfImage == 0x02DB1000` | machine 0x1C4, orig 0x2BD9000 | winmain_patcher |
| 2 | WinMain call site | 12 bytes at 0x01C471AF (`push,push,push 0x400000,call`) -> `call [Win32Bootstrap IAT]` | `bl 0x1454A28` at 0x1452EB0 inside sub_1452E08 (`__scrt_common_main_seh`, after `_get_narrow_winmain_command_line`) | winmain_patcher |
| 3 | original main | called by host: `image+0x01C48DEB` (cdecl, no args); it builds `Array<String^>` via `GetCmdArguments` | `main` wrapper sub_1454B80 (call to GetCmdArguments at 0x1454B9A) | winmain_patcher / appview |
| 4 | CoreApplication::Run replaced | activation redirect hands out fake factory; `Run` = vtable slot 13 (index 13 of `coreapp_vtable_116`) | CoreApplication activation helpers sub_909260 (caller `AppPlatform_Winrt::vf190`), sub_1075D08, sub_1075DA0; Run call chain to be traced | appview, winrt_activation |
| 5 | IFrameworkViewSource / IFrameworkView slots | `CreateView` = slot 6; view: Initialize 6, SetWindow 7, Load 8, Run 9 (after ICoreApplicationView Activated delivery) | to be read from the `MCPE_Host`/`App` view class | appview |
| 6 | frame call site | `0x016E469E`: `8b 01 8b 40 68 ff d0` = `mov eax,[ecx]; mov eax,[eax+0x68]; call eax` (AppMain vtable slot 26, thiscall, BOOL render) | not yet located; look for the indirect `blx` of AppMain/App vtable slot 26 in the FrameworkView::Run body; App RTTI vtables 0x22F8B24/0x22F8B60 | appview |
| 7 | D3D11CreateDevice IAT | 0x01EA6B90 (hook: add BGRA 0x20, swap chain injection) | IAT 0x15A6B44, thunk 0x12ECFEC, caller sub_1063830 (calls at 0x1063A6A and WARP retry 0x1063ABA; SDK=7, ppDevice=this+0xC0, pFeatureLevel=this+0xB8, adapter this+0xC4: same shape as 1.2.8 InitDevice) | gfx_compat |
| 8 | swap-chain slot | `(BYTE*)ppImmediateContext - 0x3C` = renderer+0x134 (context at renderer+0x170) | DX::DeviceResourcesWinRT swap-chain field; swap-chain creation sub_10661dc uses IDXGIFactory2 (IID at 0x1795BCC) | gfx_device |
| 9 | D3DCompile IAT / trace | 0x01EA601C | IAT 0x15A601C, sole caller sub_1062384 (call 0x1062556); D3DReflect IAT 0x15A6018 caller sub_105FEC4 (0x105FF02) | gfx_compat |
| 10 | IDXGIFactory4 probe (Win7 downgrade) | 16-byte IID at 0x0204244C | IID_IDXGIFactory4 at 0x1795988 (1 ref, sub_1060D70) -> not needed on ARM; document | gfx_compat |
| 11 | ClientInstance | vtable 0x024E7FAC, slot at +0x338 (= getLocalSceneStack, hooked, returns `*(client+0x340)`), slot 0 dtor 0x0010F440 hooked, `IsBadReadPtr(client,0x344)`, getTopScene fn 0x0011A740 | ClientInstance RTTI vtables 0x22DB910 (+0), 0x22DB8F8 (+4), 0x22DB8D4, 0x22DB8C0 (multiple inheritance); SceneStack vtable 0x232E7D0 | client_state |
| 12 | UIScene | vtable 0x025226D4; `is_showing_menu` fn 0x006A70D0 (+0xBC), `should_steal_mouse` fn 0x006A7110 (+0xC0) | UIScene vtable 0x232F278; strings is_showing_menu 0x16FCE48, should_steal_mouse 0x16FCE68, absorbs_input 0x16FCE58 (sub_1CD964) | client_state |
| 13 | language/start-screen override | vtable cells 0x02551B30 (DirectoryPack getContents, expects fn 0xF23070) and 0x02552348 (ZipPack, 0xF64550); string ctor 0xDFFC0, dtor 0xE0280; thiscall `(self,path*,out*,BOOL)` | pack access strategy RTTI (`DirectoryPackAccessStrategy`, `ZipPackAccessStrategy`) to be found; libc++ vs MSVC std::string layout on ARM | platform_fixes |
| 14 | fopen family hooks | IAT 0x01EA6940 fopen, 0x01EA6948 _wfopen, 0x01EA699C _wfopen_s, 0x01EA6978 __stdio_common_vsscanf (copyright/start_screen temp-file redirect + logging) | `api-ms-win-crt-stdio-l1-1-0` imports (37) - find cells by name | platform_fixes, pe_imports |
| 15 | termination/exception IAT | 0x01EA68C0 terminate, 0x01EA663C __std_terminate, 0x01EA68D0/0x01EA68C8 _invalid_parameter_noinfo(_noreturn), 0x01EA6654 _CxxThrowException trace, 0x01EA6584 `_ReportUnobservedException` (swallow PPL unobserved exceptions) | `_CxxThrowException` IAT 0x15A64A4, `_ReportUnobservedException` IAT 0x15A628C (MSVCP140_APP) | pe_imports |
| 16 | activation redirect cell | 0x01EA6BCC (vccorlib `GetActivationFactoryByPCWSTR`) | vccorlib import `GetActivationFactoryByPCWSTR` IAT 0x15A6C38 | pe_imports, winrt_activation |
| 17 | vccorlib Wine shims | 5 cells 0x01EA6BC4/6C40/6C6C/6C8C/6BF4 | not needed (genuine vccorlib140); confirm each | pe_imports |
| 18 | GetCmdArguments | IAT 0x01EA6C60 hooked | IAT 0x15A6BF4 (hook probably unnecessary on ARM) | pe_imports |
| 19 | FMOD | 48-entry delay descriptor 0x024B25A4 (name 0x1EAA230, module handle 0x2ADA400, IAT 0x2ADA320, INT 0x24B25E4) resolved eagerly against desktop `fmod.dll` | 48-entry DELAY import `fmod_ARM.dll` (cells 0x28D92xx); all 48 names exist in the shipped `fmod_ARM.dll` (SHA-256 5f92f5ac..., IDENTICAL to the 1.2.8 DLL, FMOD 1.09.04) | js_audio |
| 20 | chakra | patcher renames `chakra.dll` -> `ChakraCore.dll`, redirects `JsStartDebugging` to a host export | 38 JSRT imports (IAT 0x15A6AA8..0x15A6B3C), only user `ScriptApi::ChakraInterface` (vf1 sub_91EBF4: JsCreateRuntime 0x91EC28, JsStartDebugging 0x91EC9C; vf4 JsRunScript 0x91EE4C) | js_audio |
| 21 | byte patches in game code (x86 patcher) | 7 patches: PnP task completion @0x16EDD4F, skip startup metadata wait @0x16E30D7, skip PointerVisualizationSettings @0x16E3F5D/0x16E3F60, two absent-std::function callbacks @0x16D3002 and @0x16D42FB, two `Enum::ToString` diagnostics @0x16E57BF/0x16E5BAE | ARM equivalents unknown; all were Wine/Win7 workarounds: decide per patch whether needed on a genuine OS | platform_fixes |
| 22 | `$is_publish` development label | 4 bytes at 0x0183C94A `c6 45 a4 01` -> last byte 0 | string `$is_publish` at 0x17A58A4 (1 ref) | platform_fixes |
| 23 | fake CoreWindow ICoreWindow | 58-slot vtable; event adders keep the handler (AddRef, token 1); `PointerCursor` get/put (cursor NULL = relative mouse request); QI for ICoreWindow5 (4B4AE1E1-...), ICoreDispatcherWithTaskPriority (BAFAECAD-...), ICoreAcceleratorKeys (9FFDF7F5-...), InputPane2 (8A6B3F26-...) | all four IIDs found as data; ICoreWindow5/ICoreAcceleratorKeys in sub_AE1B60 (1114 B) and sub_AE3404, task priority in sub_1074BC8, InputPane2 in sub_90BA50/sub_90BC64 | input, appview |
| 24 | input event ABI | handler = `Invoke(this, sender, args)` at vtable slot 3; args objects with IPointerEventArgs / IPointerPoint / IPointerPointProperties / IPointerDevice, IKeyEventArgs (VirtualKey, KeyStatus 12 B), ICharacterReceivedEventArgs, IWindowSizeChangedEventArgs, IMouseEventArgs (MouseDelta) | HIDControllerWinRT (vtables 0x2359DA8/DC4/DE0/DF8) and `TypedEventHandler<CoreWindow,PointerEventArgs>` vtable 0x2359D48, KeyEventArgs sub_AE5ED4, CharacterReceived sub_AE5D38, MouseDevice sub_AE625C/AE620C | input |
| 25 | text input | InputPane TryShow sets `text_input_active`; CoreTextEditContext `TextUpdating` fed from WM_CHAR; clipboard paste via CoreText selection | CoreTextServicesManager sub_AE4C60, InputPane helpers 0x90C410 | input |
| 26 | WinRT classes faked | MouseCapabilities, PointerVisualizationSettings, InputPane (+InputPane2), SystemNavigationManager, PnpObject; plus shared fakes (ApplicationData, Package, CurrentApp, NetworkInformation, DisplayInformation, ApplicationView, CoreWindow, MouseDevice, Launcher, Gamepad, CoreTextServicesManager, AudioGraph(Settings), HardwareIdentification, DataReader, FileOpenPicker, ResourceContext/Manager, MemoryManager, AnalyticsInfo, GeographicRegion, ApplicationLanguages, ApiInformation, ThreadPool, CryptographicBuffer, Xaml Application/Window) | every one of the class-name strings exists in the ARM exe (UTF-16 strings with activation helper xrefs, e.g. MouseCapabilities sub_AE647C, PnpObject sub_1073D60/sub_1073E1C, PointerVisualizationSettings sub_1076C18, SystemNavigationManager sub_1076974, Gamepad sub_A698C8/A6994C/A699E4) | winrt_activation |
| 27 | Wine-only | Context1 `ClearView` slot 132 patch (Wine stub), `wine_get_version` probing, Win7 DXGI/Device2 downgrades | not applicable on ARM | (none; document as dropped) |

Items that were NOT in the x86 1.16 host but exist on 1.2.8 ARM and must be re-derived for 1.16.40: Xbox sign-in string patches and TCUI no-ops (1.16 x86 host did none: the exe has no ShowProfileCardUI delay imports and no
`ProcessPendingGameUI` string; xbox::services is statically linked), FL9_1 clamp/consequences, touch (MultitouchItem exists, vtable 0x2359E9C), pickFile, texture/backbuffer size limits, `ShaderCache` handling.

## 4. Verified-by-toolkit anchors (leads; env ARM32_EXE set; labels recorded with `arm32re.py name`)
* Image: SizeOfImage 0x2BD9000, entry 0x1452F11, 6 sections (.text 0x1000, .rdata 0x15A6000, .data 0x22D3000, .pdata 0x2960000, .rsrc, .reloc 0x2A73000),
  140033 functions, header slack 0xE0 bytes (room for a 7th section header), `.text` raw end 0x15A5600 vs virtual end 0x15A5504 (0xFC bytes trampoline slack), no `.tls`
  section (TLS directory inside .rdata), DllCharacteristics 0x9140 (APPCONTAINER set, same as 1.2.8), load config GuardFlags 0x100.
* Imports (777 import lines): 48 delay imports (all `fmod_ARM.dll`) + delay helper `ResolveDelayLoadedAPI` (api-ms-win-core-delayload-l1-1-1, IAT 0x15A65C0) and
  `DelayLoadFailureHook` (delayload-l1-1-0, 0x15A65B8); NO `XINPUTUAP.dll`, NO dxgi.dll, NO kernel32.dll; chakra.dll 38; vccorlib140_app 61; MSVCP140_APP 279;
  VCRUNTIME140_APP 20; CONCRT140_APP 5; ws2_32 32; IPHLPAPI 4; bcrypt 1; d3d11 1; D3DCOMPILER_47 2; WinRT api-sets: winrt-l1-1-0 (RoActivateInstance 0x15A6788,
  RoInitialize 0x15A678C, RoGetActivationFactory 0x15A6790), winrt-string-l1-1-0 (9), winrt-error-l1-1-0/-1-1, eventing-provider-l1-1-0, com-l1-1-0.
* RTTI present: ClientInstance, SceneStack, UIScene, AbstractScene, MinecraftKeyboardManager, AppPlatform_Winrt (vtable 0x2346220 / +0x10 0x2346210), MinecraftGame
  (several vtables), App (0x22F8B24/0x22F8B60), DX::DeviceResources (0x23C796C) and DX::DeviceResourcesWinRT (0x2347058), GameRenderer, LevelRenderer, MCPE_Host::* ,
  HIDControllerWinRT, MultitouchItem, ScriptApi::ChakraInterface. NOT present: `AppMain` (1.2.8 name), `HIDController`, `InputAdapter`, `IFrameworkView` as plain names.
* Strings present: `_level_9_1` 0x1795C6C (xrefs sub_10674E8), `_level_9_3` 0x1795C60, `#define VERSION ` 0x17A3B08, `$is_publish`, `button.signin` 0x171EF14,
  `button.xbl_signin` 0x1725760. NOT present as exe strings: `menu.copyright`, `start_screen.json` (they live in resource files / are matched on the fly by the host).
* Labels already stored for this exe (names jsonl): sub_1063830, sub_1062384, sub_105FEC4, sub_1060D70, sub_10661DC, WinRT activation helpers for
  PointerVisualizationSettings/MouseCapabilities/InputPane/SystemNavigationManager/PnpObject, sub_1452E08, sub_1454A28, sub_1454B80, sub_AE1B60, sub_91EBF4.

## 5. Areas (11) - owner = the area's RE agent; every area writes `facts/<area>.json` + `.md`
| key | title | x86 source regions (file : symbols/lines) | main deliverables | needs |
|---|---|---|---|---|
| gfx_device | Graphics device / swap chain / present / resize | win32_host.c: `win32_d3d11_create_device` 12283-12403 (swap-chain injection), `install_swap_chain_present_hook`/`win32_present` 12852-13040, `initialize_game_d3d`, `resize_game_window`, `Win32CraftPresentSyncInterval` 3400; 116.inl `frame_116`, `install_frame_116`, `window_proc_116` WM_SIZE | DeviceResourcesWinRT layout (device, context, swap chain, RTV, DSV, size fields), swap-chain creation path and how to pre-seed/skip it, Present vtable slots, resize path, device-lost path | gfx_compat, appview |
| gfx_compat | Graphics compat + FL9_1 | win32_host.c: `install_d3d11_device_compat` 12532-12600, `win32_d3d11_create_device`, `install_d3d_compile_trace`/`win32_d3d_compile` 11980-12090, 12192-12280, `patch_win7_*` 12407-12470, 116.inl `bootstrap_116` (D3D parts, IID 0x0204244C), `install_clear_view_116`; ARM facts `gfx_compat.md`, `fl91.md` | feature-level list/flags/args at the call site, clamp plan to {0x9100}, D3DCompile/D3DReflect ABI + defines/profile selection + shader cache, IID cells, device-lost, FL9_1 consequences | gfx_device |
| appview | App/view creation, activation, frame loop, visibility, resize | 116.inl `coreapp_run_116`, `coreapp_view_*_116`, `activation_args_*_116`, `frame_116`/`install_frame_116`, `bootstrap_116`, `command_arguments_116`; win32_host.c 6065-6160 (`fake_core_window_activate_116`, activated/visibility args, `fake_core_dispatcher_process_events`), 6243-6330 `drain_dispatcher_queue`, `win32_core_window_vtable` 6427; ARM facts `appmain.md` | ordered startup sequence from `main` to first frame, IFrameworkView/Source slot numbers, Activated/VisibilityChanged/SizeChanged semantics, frame call site + slot 26, quit path, suspend/resume | winrt_activation, input, gfx_device |
| input | Pointer / keyboard / character / mouse modes / text | 116.inl lines 1-60, 109-205 (relative mouse), 329-465 (event arg objects), 753-1011 (`invoke_*_116`, `window_proc_116`); win32_host.c 5941-6060, 6308-6330 (MouseDevice), 6427 (ICoreWindow vtable), 6647-6830 (CoreText paste/TextUpdating), 17588-17700 (`set_game_relative_actual`, `update_mouse_capture`); ARM facts `input.md` | how HIDControllerWinRT consumes each event, exact interface slots read from each args object, mouse modes (PointerCursor NULL / MouseMoved deltas), key/char/text, gamepad, wheel, handler lifetime | appview, client_state, touch |
| touch | Touch input and HUD | ARM facts `touch_game.md`, `touch_host.md`, `touch_impl.json`; win32_host.c "ARM32 touch input" block (grep `arm32_touch`, ~19940-21230) | whether MultitouchItem/InputMode logic exists and how touch pointers reach it in 1.16.40 (PointerDeviceType touch vs HID item), mapper order, default input mode, touch HUD rendering condition | input |
| client_state | Client/scene state for mouse capture and text focus | 116.inl 49-107 (`client_scene_stack_116`, `client_destroy_116`, `refresh_scene_mouse_116`); win32_host.c 6035-6045 cursor request; ARM facts `client_state.md` | ClientInstance vtable/slot/field layout, local scene stack, top scene, UIScene flags, keyboard-manager text-focus bit, hook plan by data cell | input |
| platform_fixes | Platform fixes, game-code hooks, Xbox/TCUI, one-off byte patches | 116.inl 1064-1181 (dev label, language/start-screen overrides), win32_host.c `Win32CraftPrepareCopyrightLang*` 10800+, `win32_115_fopen*` 11290-11440, `lang_apply_line_override` 10298, `start_screen_force_development_control` 10956; tools/1.16.20.03-patcher.py all `checked_write` code patches; ARM facts `platform_fixes.md`, `chat_xbox.md` | pickFile hook, DirectoryPack/ZipPack getContents cells, string layout, `$is_publish` site, per-patch verdict on the 7 x86 byte patches, Xbox sign-in neutralisation, TCUI, chat compat | pe_imports |
| pe_imports | PE imports, IAT hooks by name, api-sets, delay imports | 116.inl `bootstrap_116` IAT list, tools/1.16.20.03-patcher.py (`WIN32CRAFT_API_SETS`, renames, compat placeholders), win32_host.c `install_activation_redirect` 9713, `install_vccorlib_compat` 10152, `install_exception_trace` 11808/12174; ARM facts `pe_imports.md`, `imports_table.json` | full import table with per-DLL patcher action, IAT cells by name, api-set contract list vs RT 8.1 (5 unproven), delay descriptors, which x86 hooks drop | winmain_patcher, js_audio |
| winmain_patcher | WinMain/entry call site and table-driven patcher plan | tools/1.16.20.03-patcher.py `patch()`, tools/1.2.8-arm32-patcher.py (whole), win32_host.c `Win32Bootstrap` dispatch ~22343-22370, `arm32_boot_identify_image` 21966; ARM facts `winmain.md`, `winmain_verify.py` | bytes at the call site, trampoline slot, new-section layout, expected patched SizeOfImage, profile format + discovery rules, byte-patch table, verification harness | pe_imports, platform_fixes, js_audio |
| winrt_activation | WinRT activation surface | win32_host.c `win32_get_activation_factory` 9383-9710, `activation_116` (116.inl 619-668), fake object tables (grep `*_factory`), `fake_query_interface` 3500-3600 | complete list of classes/IIDs activated before the first frame (by call-site tracing), which need fakes on desktop/RT 8.1, vtable slots called on each, GetForCurrentView dependencies | appview, input |
| js_audio | chakra.dll (JSRT) and FMOD plan | tools/1.16.20.03-patcher.py (chakra descriptor handling, `fmod_X86.dll` rename), 116.inl `resolve_fmod_delay_imports_116` (win32_host.c 13964-14060), win32_host.c AudioGraph fakes + 5075; ARM facts `fmod.md`, `fmod.json`, `fmod_null/` | JSRT call inventory (which of 38 are really reached, ABI, what scripts run), shim design for RT 8.1/Win10, FMOD delay-load flow + patched DLL reuse, AudioGraph fallback | pe_imports, winmain_patcher |

## 6. Cross-area dependencies (who must agree on what)
1. gfx_compat <-> gfx_device: the hooked `D3D11CreateDevice` is where the swap chain is injected (x86); both must agree on the DeviceResourcesWinRT field that receives
   the swap chain and on whether the game's own swap-chain creation (sub_10661DC) is skipped by pre-seeding or must be replaced by a code hook.
2. appview <-> winrt_activation: `CoreApplication`, `CoreApplicationView`, `CoreWindow`, `CoreDispatcher`, `ApplicationView`, `DisplayInformation` faking and their slot numbers.
3. appview <-> input: which handlers the game registers during `SetWindow`/`Load` and when (handlers must exist before the first event is delivered); window_proc ownership.
4. input <-> client_state: relative mouse is driven by `PointerCursor == NULL` plus the top-scene probe; text focus by the keyboard-manager bit / InputPane TryShow.
5. input <-> touch: Windows pointer messages vs WM_TOUCH; touch pointers must not be duplicated as mouse; PointerDeviceType in fake PointerDevice.
6. pe_imports <-> winmain_patcher <-> js_audio: the same import rewrite pass renames DLLs, adds `win32craft.dll` exports, redirects `chakra.dll`, and handles the fmod delay descriptor; the new section must hold all added import descriptors.
7. platform_fixes <-> winrt_activation: several x86 byte patches skip optional WinRT work (PnP, PointerVisualizationSettings, enum diagnostics); the decision "fake vs real vs skip" must be consistent.
8. platform_fixes <-> winmain_patcher: every code byte patch ends up in the patcher table (rva, expected, new, reason).
9. gfx_compat <-> platform_fixes: shader cache location and `.material`/HLSL resource access go through the same file/zip hooks.

## 7. 1.16-specific risks (to be addressed by the areas; each needs evidence, not guesses)
1. **chakra.dll**: 38 JSRT imports; absent on RT 8.1 (JSRT lives in jscript9.dll there, and `JsSetPromiseContinuationCallback` / `JsStartDebugging` are Win10-only). Loader fails
   at process start unless every import resolves. Options: app-local shim DLL (renamed in the import table so it can never be shadowed by a different system `chakra.dll`), forwarding to
   jscript9/chakra when present, else stub returning errors; need to know whether script engine init is on the startup path and whether failure is fatal.
2. **New api-sets on RT 8.1**: `processtopology-obsolete-l1-1-0` (SetThreadAffinityMask), `synch-ansi-l1-1-0` (OpenMutexA), `synch-l1-2-1` (WaitForMultipleObjects...), `windowserrorreporting-l1-1-0`
   (WerRegisterFile), `wow64-l1-1-0` (IsWow64Process); 41 of 46 are proven by loadcheck. Also check `api-ms-win-core-datetime-l1-1-1` and `sysinfo-l1-2-0` (only the HANDOFF list of 46 contracts is authoritative). The app-local runtime
   generator (`tools/make_arm32_apiset_shims.py`) must learn the new names; `loadcheck.exe` is the verification tool.
3. **WinRT classes missing on RT 8.1 / desktop processes**: `SystemNavigationManager`, `Gamepad` (Windows.Gaming.Input), `AudioGraph`, `CoreTextServicesManager`, `PointerVisualizationSettings`
   are Win10-only; every `*.GetForCurrentView` static (`InputPane`, `MouseDevice`, `DisplayInformation`, `ApplicationView`, `SystemNavigationManager`, `CoreTextServicesManager`) fails without a CoreWindow view.
   About 100 class names (many generic) fall through to the real runtime; `WinRT fallback` log lines are the evidence trail. WebAuthenticationCoreManager (xbox sign-in) has 7 refs.
4. **FMOD**: `fmod_ARM.dll` is byte-identical to the 1.2.8 one (1.09.04, patchable the 1.2.8 way) but is a DELAY import with 48 entries here (1.2.8: normal import); the delay-load helper path
   (`ResolveDelayLoadedAPI`, `DelayLoadFailureHook`) and the DLL name used for LoadLibrary must be handled; the FMOD WinRT/package-identity step and INFINITE device-watcher waits are known hazards (silent fallback `fmod_null` exists).
   The game also touches `Windows.Media.Audio.AudioGraph` (sub_8D082C/8D08B0): x86 suppresses completion callbacks to force the FMOD path.
5. **FL9_1 specifics**: user devices top out at 9_1. `D3D11CreateDevice` list/flags are in a std::vector built in sub_1063830 (clamp to {0x9100}); the game derives `VERSION`, shader targets (`vs/ps_4_0_level_9_1`),
   vertex formats and sampler filters from the selected level; 52/52 vertex+fragment HLSL files compile at 9_1 but geometry shaders (14, VR-only) and instancing/texture-array paths must stay off; no MSAA, 16-bit index meshes,
   textures/backbuffer <= 2048; ID3D11Device2/3 and DXGI 1.3+ interface probes (QI results must tolerate failure); `CheckFeatureSupport` options at 9_1; swap-chain buffer count/effect for HWND vs CoreWindow;
   the runtime shader cache (`ShaderCache` directory/key) must be wiped or keyed per feature level; the hook must stay installed for the process lifetime because device loss re-runs device creation (1.2.8 zeroed `owner+0xB8`); device-lost / `DXGI_ERROR_DEVICE_REMOVED` handling and Present return handling.
6. **Genuine desktop runtime behaviours the x86 host patched around under Wine**: `_ReportUnobservedException` (PPL terminate), `Enum::ToString` diagnostics, absent `std::function` callbacks, PnP metadata wait - each must be
   re-evaluated on a real OS before a patch is carried over (do not port Wine workarounds blindly, but do not drop one without evidence either).
7. **Thumb/ARM code patching**: only data cells (vtable/IAT) are patched at runtime; the single code edit (WinMain `bl`) is offline. If the frame call site needs a code hook it must be an offline `bl` to a trampoline
   (as the WinMain one) or be replaced by a vtable cell patch; `.text` slack is only 0xFC bytes, so trampolines must be shared.
8. **Memory/time**: image is 44 MB / 140k functions; startup loads ~19 `.material` JSON files and compiles shaders at runtime (check how `D3DCOMPILER_47.dll` is provided on RT 8.1, same question as for 1.2.8).
9. **Input**: 1.16 reads pointer positions, button states, wheel, key status and device type from fake WinRT objects whose exact interface slots must be read from ARM code; a wrong slot returns garbage silently.
   Touch: Windows RT 8.1 delivers no WM_POINTER to the window (touch arrives as WM_TOUCH or emulated mouse), so the 1.2.8 device finding applies.

## 8. Conventions for all area agents
* `ARM32_EXE=D:\repo\Win32Craft-main\Minecraft_1.16.40.02\Minecraft.Windows.exe` before ANY toolkit command (otherwise 1.2.8 is analysed); label everything with `arm32re.py name <rva> <label>`.
* Write only `arm32/v1_16_40/facts/<area>.json|.md`. Fact ids `area.short_name`; `x86_symbol` may be a 1.16 x86 constant (win32_host_116.inl) or a 1.2.8 ARM fact id (analogy), say which.
* A fact needs >= 2 independent indicators for `high`; "not found" is an acceptable answer. Never reuse a 1.2.8 ARM number without re-deriving it here.
* Record expected bytes (first 8-12 bytes) for every function/cell/site the host will hook or patch; code pointers stored in data have bit 0 set (Thumb), report RVAs even.
* Keep tool output small (`-n`, `head`, `Select-Object -First`).
