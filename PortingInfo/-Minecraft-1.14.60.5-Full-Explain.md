# Minecraft 1.14.60.5 x86 - full porting record

| | |
|---|---|
| Game | Minecraft Bedrock Edition (Mojang / Microsoft) |
| Version | 1.14.60.5, x86 UWP APPX, package `Microsoft.MinecraftUWP_1.14.6005.0_x86__8wekyb3d8bbwe` |
| Engine | native C++ Bedrock engine, ChakraCore JavaScript, cohtml UI; no .NET game logic |
| Host | `win32craft.dll` (Win32Craft x86 host), profile `game_profile_114` |
| Patcher | `tools/1.14.6005.0-patcher.py` |
| Package | `Win32Craft.1.14.60.5.exe` in the assembled folder (not in git); test copy in `%TEMP%\w32c-114-package` |
| Verified on | Windows desktop x86: menu, world load, mouse capture, survival block breaking |
| Not working yet | audio output on the test machine (FMOD output init) |

Everything below was established with the host log (`win32craft.log` next to the exe), IDA on
the retail exe, and the retail package's own PE import tables. The patcher and the host profile
are deliberately separate: the patcher knows the file layout, the host knows the runtime
addresses. Both must agree on the patched `SizeOfImage`.

## 1. Where the pieces live

| Piece | Path |
|---|---|
| Host sources (one translation unit) | `win32_host.c` + `win32_host_116.inl` |
| x86 build script | `build_windows_mingw.bat` (baseline GCC/lld-link build) |
| Exe patcher / packager | `tools/1.14.6005.0-patcher.py` |
| Host exports | `win32craft.def` |
| Retail exe used for analysis | `%TEMP%\w32c-114\Minecraft.Windows.exe` (stock SHA-256 below) |
| Ready test package | `%TEMP%\w32c-114-package` (not in git) |
| Host log | `win32craft.log` next to the exe, only with `WIN32CRAFT_DEBUG=1` |
| Test build directory (not in git) | `%TEMP%\w32c-114-build\` |

`win32_host_116.inl` is shared with 1.16.20.03 and 1.16.40.02. The runtime picks the profile
from the patched image size; every address below is only valid for 1.14.60.5.

The 1.14 port reuses the complete x86 WinRT host: package/application-data redirection,
`CoreWindow`, `CoreApplicationView`, the D3D11 HWND swap chain, the vccorlib compatibility
shims, the stdio hooks and the FMOD delay-import resolver. The version-specific parts are the
profile, the frame callback, the pointer-visualization fake and the world-entry exe patch.

## 2. Why 1.14.60.5 needed its own profile and patcher

The 1.16 profiles do not work on this build. The engine is recognisably the same, but almost
every address moved, and the executable layout is older:

| Thing | 1.16.40.02 | 1.14.60.5 |
|---|---|---|
| Stock `SizeOfImage` | `0x02DAE000` | `0x028B7000` |
| Patched `SizeOfImage` (profile key) | `0x02DAF000` | `0x028B8000` |
| ClientInstance vtable | `0x024E5FAC` | `0x02190DAC` |
| Top UIScene vtable | `0x025206D4` | `0x021CC388` |
| DirectoryPack `getContents` slot | `0x0254FB14` | `0x021F6120` |
| ZipPack `getContents` slot | `0x02550348` | `0x021F6A00` |
| `FrameworkView::Run` frame call | `0x016E299E` | `0x01672A4F` |
| `$is_publish` initializer | `0x0183AC8A` | `0x006280A1` |
| Executable `_main` | `0x01C4682B` | `0x019D2B8A` |
| FMOD delay descriptor | `0x024B01CC` | `0x0215C0F0` |
| FMOD delay imports | 48 | 42 |
| IAT block | `0x01EA4xxx` | `0x01C0Cxxx` |
| `IID_IDXGIFactory4` copy | `0x02040358` | `0x01D7ECA4` |
| ClientInstance scene-stack member | `0x340` | `0x310` |
| AppMain frame vtable slot | `0x68` | `0x18` |
| `$is_publish` stack displacement | `0xA4` | `0xE0` |

The patcher accepts exactly one input hash:

| SHA-256 | Revision |
|---|---|
| `5B022C1F60256CD366EAA5BB8DBA2ED9B35A053C7936A7D7C74C0280EF006F49` | retail x86 `Microsoft.MinecraftUWP_1.14.6005.0_x86` |

The stock exe is 42,193,408 bytes; the patched exe is 42,194,432 bytes. The patcher refuses an
unknown input hash instead of applying bytes at the wrong offsets. The host likewise validates
every hook site signature and every profile vtable slot before it patches anything, so a
mismatched exe produces a log line instead of a crash deep in the WinRT fakes.

## 3. What the patcher changes in the exe

### 3.1 PE header

| Field | Original | Patched | Why |
|---|---|---|---|
| `IMAGE_FILE_LARGE_ADDRESS_AWARE` | `0x0122` | `0x0122` | verified, not changed |
| OS version minor | `2` | `2` | verified, not changed |
| Subsystem version minor | `2` | `2` | verified, not changed |
| DLL characteristics | `0x9140` | `0x8100` | clears `APPCONTAINER` (`0x1000`) and `DYNAMIC_BASE` (`0x0040`) |

The package is an unpackaged Win32 process, so the loader must not treat the image as an
app-container image. `DYNAMIC_BASE` is also cleared on purpose: the `WinMain` patch uses an
absolute VA (`0x400000 + .d3imp RVA + offset`) for the `Win32Bootstrap` IAT, which is only
valid if the image loads at its preferred base `0x400000`.

### 3.2 Code patch sites

All values are RVAs. In `.text` the file offset is RVA minus `0xC00` on this build. The patcher
prints the corresponding file offset (`file+0x...`) for each site.

| RVA | Original bytes | Patched bytes | Purpose |
|---|---|---|---|
| `0x19D1A5F` | `50 57 68 00 00 40 00 E8 2D 10 00 00` | `FF 15 <IAT> 90 90 90 90 90 90` | replace the game's `WinMain` call with an indirect call to `win32craft.dll!Win32Bootstrap` |
| `0xEEFEF5` | `8B 4E 04 8B 01 8B 40 04 FF D0` | `B0 01 90 90 90 90 90 90 90 90` | skip the first Chakra script-interface predicate call in `sub_12EFEB0` |
| `0x9A6E26` | `8B 02 8B CA 6A 01 FF 10` | `E8 <rel32> 90 90 90` | unlink the old packet listener from the `NetworkHandler` dispatcher vector, then delete it |
| `0x9A6EC6` | `8B CA 8B 02 6A 01 FF 10` | `E8 <rel32> 90 90 90` | unlink the old telemetry listener from `[engine+0xB0]`, then delete it |
| `0x9A6F50` | `8B 02 8B CA 6A 01 FF 10` | `E8 <rel32> 90 90 90` | unlink the old client-level listener from its manager vector, then delete it |
| `0x9A6FC2` | `8B 01 6A 01 FF 10` | `E8 <rel32> 90` | unlink the old hit-detect listener from `[ClientInstance+0x62C]`, then delete it |

The four listener calls target stubs appended to `.d3imp`; see the client script listener
subsection below.

The 1.14 build did **not** need the 1.16.40.02 pointer-visualization, empty-callback and
`Enum::ToString` exe patches in the tested path. Its `PointerVisualizationSettings` access goes
through the vccorlib activation helper (`GetActivationFactoryByPCWSTR`), which the host
redirects; see section 4. The 1.14 startup and menu paths did not hit the empty `std::function`
or enum-diagnostic aborts.

#### The world-entry patch

Entering a world on the stock exe faults in `sub_12EFEB0` (IDA VA `0x12EFEB0`, RVA `0xEEFEB0`).
The function reads a script-interface pointer from `[esi+4]`, calls the first virtual method
(`vtable+4`) and tests the returned byte. At this point the interface is a `ChakraInterface`
object (IDA VA `0x01B2E740`, vtable `0x02186AF8`) whose backing script state is not ready; the
call faults and the loading screen never leaves.

The patch overwrites the 10-byte sequence

```
8B 4E 04        mov ecx, [esi+4]
8B 01           mov eax, [ecx]
8B 40 04        mov eax, [eax+4]
FF D0           call eax
```

with

```
B0 01           mov al, 1
90 * 9          nop
```

so the following `test al,al; je ...` takes the success path and the rest of the world-entry
function runs. The user confirmed that the world loads and survival play works. This is a
workaround, not a host-side fake: the skipped predicate is not reimplemented, and any script
feature that depends on it is bypassed. A proper fix would require a working Chakra interface
for the 1.14 script API, which is out of scope for this port.

#### The client script listener re-registration crash

`MinecraftClientScriptEngine::<init>` (VA `0xDA6DB0`) runs again for every world. It allocates
four new `Script*EventListener` objects, stores them in the engine at `+0x230` (packet),
`+0x234` (telemetry), `+0x23C` (client level) and `+0x240` (hit detect), and deletes the
previous instances. The stock code never unregisters the previous listener from the manager
that received it:

| Engine field | Listener | Manager |
|---|---|---|
| `+0x230` | `ScriptClientPacketEventListener` | `ClientInstance` vtbl `+0x3DC` -> `NetworkHandler`, then `+0x1B8` (dispatcher); listener vector at dispatcher `+0x0` |
| `+0x234` | `ScriptTelemetryEventListener` | vector object at `[engine+0xB0]` |
| `+0x23C` | `ScriptClientLevelEventListener` | manager object returned by vtbl `+0x70` on the `0xDA6DB0` argument |
| `+0x240` | `ScriptHitDetectEventListener` | vector object at `[ClientInstance+0x62C]` |

After the first world exit, every vector that survives contains `[stale, current]`. The packet
dispatcher was observed walking its vector, loading the first dword of the recycled stale block
as a vtable and calling through it. The fault was a second-chance AV/DEP with `EIP` inside heap
string data (`layer`, `quartz_block`); the stale 0x28-byte block had been reused for resource
names, not for code. The telemetry and hit-detect vectors also held stale pointers in the same
dump; the level manager is recreated per world and did not.

The fix replaces each deleting-destructor call with a stub in `.d3imp` that unlinks the old
pointer first:

* shared `remove_delete(esi = vector object, edx = listener)` helper: `pushad`, scan
  `[begin, end)` for `edx`, shift later entries left by four bytes, subtract four from `end`,
  then run the original deleting destructor (`mov eax,[edx]; mov ecx,edx; push 1; call [eax]`),
  `popad`, `ret`. All caller registers and the stack balance are preserved.
* packet stub (`0x9A6E26`): `push edx; mov ecx,[ebx+0x248]; mov eax,[ecx];
  call [eax+0x3DC]; mov esi,[eax+0x1B8]; pop edx; call helper; lea esi,[ebx+0x230]; ret`.
* telemetry stub (`0x9A6EC6`): `push edx; mov esi,[ebx+0xB0]; pop edx; call helper;
  lea esi,[ebx+0x234]; ret`.
* level stub (`0x9A6F50`): the manager vector is already in `esi`; `call helper; ret`.
* hit-detect stub (`0x9A6FC2`): `mov edx,ecx; mov eax,[ebx+0x248]; mov esi,[eax+0x62C];
  call helper; ret`.

`ebx` is the engine at all four sites. The stubs restore `esi`/`edi`/`edx` because the
registration code immediately after each site still uses them. The helper also tolerates a
vector that no longer contains the old pointer (the level manager case): it then falls through
to exactly the deleting destructor the stock code would have called. The server engine
allocations remain retained by the separate server-listener workaround; the client listener
objects themselves are still deleted, matching the stock code.

### 3.3 String and import surgery

The patcher replaces equal-length ASCII strings in place, padding with NUL:

| Original | Replacement | Count | Why |
|---|---|---|---|
| `vccorlib140_app.DLL` | `vccorlib140.DLL` | 1 | package-only CRT -> desktop CRT |
| `MSVCP140_APP.dll` | `MSVCP140.dll` | 1 | same |
| `CONCRT140_APP.dll` | `CONCRT140.dll` | 1 | same |
| `VCRUNTIME140_APP.dll` | `VCRUNTIME140.dll` | 1 | same |
| `fmod_X86.dll` | `fmod.dll` | 2 | the package ships the desktop-named copy |

The retail image has 31 import descriptors and an import directory size of `0x280`. Descriptor
names are changed without moving the descriptor array:

* `dxgi.dll`, `d3d11.dll`, `xinputuap.dll` and the WinRT API-set DLLs are renamed to
  `win32craft.dll`. The host DLL exports the same functions (directly or as forwarders), so the
  host can install `D3D11CreateDevice`, `D3DCompile`, DXGI factory and WinRT compatibility
  shims before the game resolves them.
* `chakra.dll` is renamed to `ChakraCore.dll`, which is the DLL the retail package actually
  ships.
* A new `Win32Bootstrap` descriptor is appended. Its ILT/IAT live in the new `.d3imp` section,
  and the `WinMain` patch calls through that IAT.
* A new `JsStartDebugging` descriptor points at the existing chakra IAT slot; its ILT entry is
  replaced with `JsAddRef`, so the loader never fails on a debugging export the game does not
  use.
* Three delay-load helper imports (`ResolveDelayLoadedAPI`, `ResolveDelayLoadsFromDll`,
  `DelayLoadFailureHook`) are handled the same way: their ILT entries are replaced with
  `GetLastError`, and new descriptors point at the existing IAT slots. This keeps the image
  loadable on Windows 7, where those newer kernel32 exports do not exist.

The replacement names are shorter or equal in length, so no RVAs move.

### 3.4 The `.d3imp` section and the bootstrap call

A new section is appended after the last section (`.reloc`):

* name `.d3imp`, RVA `0x028B7000`, raw offset `0x283D200`, characteristics `0xE0000040`
  (initialized data, read/write/execute, because the listener helper and stubs are code);
* it contains the relocated import descriptor array (37 descriptors, `0x2E4` bytes), the
  `win32craft.dll` name, the `ChakraCore.dll` name, hint/name entries, ILTs, the
  `Win32Bootstrap` IAT, and the client-listener `remove_delete` helper plus its four stubs;
* `SizeOfImage` grows from `0x028B7000` to `0x028B8000` (the host profile key);
* the bootstrap IAT VA is absolute (`0x400000 + new_rva + offset`), because the call site is a
  normal indirect `call dword ptr [abs]` in the patched image.

The `WinMain` call site is the last patch. The host's `Win32Bootstrap` creates the window and
then calls the executable's real `_main` at `main_entry` (`0x019D2B8A`). The original `WinMain`
wrapper is not used.

### 3.5 Host DLL name

The default import name is `win32craft.dll`. `--host-dll NAME` changes it, but the name must be
ASCII, non-empty and fit the same string surgery; the package mode validates that the supplied
host is an x86 PE32 DLL.

## 4. Host profile `game_profile_114`

The profile is selected in `bootstrap_116` from the PE `SizeOfImage`:

```c
game_116 = image_size == 0x02daf000 ? &game_profile_116_04002
         : image_size == 0x028b8000 ? &game_profile_114
                                    : &game_profile_116_02003;
```

If the exe is not patched, `SizeOfImage` is `0x028B7000`, the host falls into the 1.16.20.03
profile and every signature check fails. That is intentional: the error is visible in the log
instead of becoming a random crash.

Game structure addresses:

| Field | RVA | Meaning |
|---|---|---|
| `client_instance_vtable` | `0x02190DAC` | `ClientInstance` vtable |
| `client_destroy` | `0x00100910` | scalar deleting destructor |
| `client_scene_stack` | `0x0010B0D0` | `SceneStack` getter (vtable `+0x338`) |
| `client_top_scene` | `0x0010B020` | `getTopScene` |
| `ui_scene_vtable` | `0x021CC388` | top `UIScene` vtable |
| `gameplay_flag_menu` | `0x0066AD20` | `is_showing_menu` |
| `gameplay_flag_game` | `0x0066AD40` | `should_steal_mouse` |
| `game_string_assign` | `0x000D4820` | game string assign |
| `game_string_destroy` | `0x000D4AD0` | game string destroy |
| `directory_get_contents_slot` | `0x021F6120` | `DirectoryPack::getContents` vtable cell |
| `directory_get_contents` | `0x00EA4300` | `DirectoryPack::getContents` |
| `zip_get_contents_slot` | `0x021F6A00` | `ZipPack::getContents` vtable cell |
| `zip_get_contents` | `0x00EE6390` | `ZipPack::getContents` |
| `frame_site` | `0x01672A4F` | `FrameworkView::Run` frame call site |
| `development_label_site` | `0x006280A1` | `$is_publish` initializer |
| `main_entry` | `0x019D2B8A` | executable `_main` |
| `fmod_delay_descriptor` | `0x0215C0F0` | FMOD delay-import descriptor |
| `dxgi_factory4_iid` | `0x01D7ECA4` | `IID_IDXGIFactory4` copy for the Windows 7 probe |

Profile extras that differ from the 1.16 builds:

| Field | 1.14 value | Meaning |
|---|---|---|
| `client_scene_stack_slot` | `0x338` | ClientInstance vtable slot of the scene-stack getter |
| `client_scene_stack_member` | `0x310` | ClientInstance member returned by the getter |
| `frame_slot` | `0x18` | AppMain frame vtable slot |
| `frame_has_render` | `0` | the 1.14 frame call takes only `this`, no render flag |
| `development_label_disp` | `0xE0` | `$is_publish` stack displacement |
| `fmod_import_count` | `42` | FMOD delay imports |

`is_showing_menu` (`0x0066AD20`) and `should_steal_mouse` (`0x0066AD40`) are valid forwarders
to the `UIScene` inner object's virtual methods at `+0x24` / `+0x2C`. The host calls them with
the outer scene pointer; the forwarder adjusts to the inner object. That is why the scene
refresh does not need a separate inner-object pointer.

IAT slots patched by the host:

| Field | RVA | Hook |
|---|---|---|
| `iat_exception_trace` | `0x01C0C6A8` | `_CxxThrowException` trace |
| `iat_activation_redirect` | `0x01C0CCD0` | `GetActivationFactoryByPCWSTR` |
| `iat_vccorlib_ctor` | `0x01C0CC44` | `Platform::Object` constructor |
| `iat_vccorlib_allocate2` | `0x01C0CC20` | `Platform::Heap::Allocate(size, align)` |
| `iat_vccorlib_allocate1` | `0x01C0CC2C` | `Platform::Heap::Allocate(size)` |
| `iat_vccorlib_ibox_array` | `0x01C0CC54` | `GetIBoxArrayVtable` |
| `iat_vccorlib_ibox` | `0x01C0CCA8` | `GetIBoxVtable` |
| `iat_command_arguments` | `0x01C0CC94` | `GetCmdArguments` |
| `iat_d3d_compile` | `0x01C0C0B4` | `D3DCompile` |
| `iat_d3d11_create_device` | `0x01C0CBC0` | `D3D11CreateDevice` |
| `iat_fopen` | `0x01C0C974` | `fopen` |
| `iat_wfopen` | `0x01C0C9A0` | `_wfopen` |
| `iat_wfopen_s` | `0x01C0C96C` | `_wfopen_s` |
| `iat_vsscanf` | `0x01C0C954` | `__stdio_common_vsscanf` |
| `iat_invalid_parameter` | `0x01C0C904` | `_invalid_parameter_noinfo` |
| `iat_invalid_parameter_noreturn` | `0x01C0C938` | `_invalid_parameter_noinfo_noreturn` |
| `iat_terminate` | `0x01C0C93C` | `terminate` |
| `iat_std_terminate` | `0x01C0C670` | `__std_terminate` |
| `iat_report_unobserved` | `0x01C0C5C8` | `_ReportUnobservedException` |

The vccorlib and stdio hooks are the same compatibility layer used by the other 1.16 ports.
They are installed only after the profile is selected, so the 1.16 profiles keep their own
addresses.

### 4.1 Activation redirect and `PointerVisualizationSettings`

The activation IAT is the vccorlib helper `GetActivationFactoryByPCWSTR` at `0x01C0CCD0`, not
`RoGetActivationFactory` at `0x01C0C790`. The game calls the helper for projected WinRT types.
The helper lives in `vccorlib140.dll` and resolves `RoGetActivationFactory` itself, so patching
the exe's `RoGetActivationFactory` IAT does not intercept it; the helper's calls still go to the
real WinRT factory, which fails for `Windows.UI.Input.PointerVisualizationSettings` in an
unpackaged process.

The host's activation dispatcher recognises the class name and returns a fake factory. For
`PointerVisualizationSettings` the fake is:

* a factory object whose `Current` method returns a settings object;
* a 10-slot `IPointerVisualizationSettings` table: slots 6-9 are
  `put_IsContactFeedbackEnabled`, `get_IsContactFeedbackEnabled`,
  `put_IsBarrelButtonFeedbackEnabled`, `get_IsBarrelButtonFeedbackEnabled`.

1.14 calls slots 6 and 8 (the two setters), so the table must cover all four property methods,
not just two setters. The setters are no-ops and the getters return 0. This is why the 1.14
port does not need the 1.16.40.02 exe patches that skip the whole pointer-visualization block.

## 5. Bootstrap and window

`bootstrap_116` does, in order:

1. select the profile and set `fmod_delay_descriptor_116`;
2. detect Windows 7 through `GetVersion` (the host creates a one-buffer DISCARD chain there);
3. install the frame hook, development label and language override;
4. hook the `ClientInstance` vtable (scene stack getter and destructor);
5. install the crash/exception trace, `RoInitialize`, package/application-data redirection,
   vccorlib compatibility, command-line and stdio hooks;
6. install the `D3DCompile` and `D3D11CreateDevice` compatibility hooks;
7. resolve the 42 FMOD delay imports;
8. create the `MCPE116Win32` window class and the game window;
9. call the executable's real `_main` at `main_entry`.

The window title is built by the host, not by an exe patch:

```c
lstrcpyW(title, L"Win32Craft ");
lstrcatW(title, game_116->version_wide);
```

`version_wide` is `L"1.14.60.5"`, so the actual title is `Win32Craft 1.14.60.5`. The class name
is `MCPE116Win32`, the same class as the 1.16 ports.

`install_frame_116` replaces the `FrameworkView::Run` call site. The expected signature is
`8B 01 8B 40 18 FF D0` (`mov eax,[ecx]; mov eax,[eax+0x18]; call eax`); the first five bytes
become `E8 <rel32>` and the last two become NOPs. The target is `frame_114` because
`frame_has_render` is 0:

```c
static BYTE __attribute__((thiscall)) frame_114(void *app)
{
    typedef BYTE (__attribute__((thiscall)) *FrameFn)(void *);
    LONG presents_before = game_present_success_count;
    BYTE result = ((FrameFn)(*(void ***)app)[game_116->frame_slot / 4])(app);
    refresh_scene_mouse_116();
    if (game_swap_chain && game_present_success_count == presents_before)
        IDXGISwapChain_Present(game_swap_chain,
                               Win32CraftPresentSyncInterval(), 0);
    return result;
}
```

The 1.16 builds use `frame_116`, which passes the render flag to the frame method and uses slot
`0x68`. The 1.14 frame method takes only `this`; the host must not push a render argument.

`install_development_label_116` changes the `$is_publish` initializer `C6 45 E0 01` to
`C6 45 E0 00`, which enables the development label. `install_language_override_116` hooks the
`DirectoryPack` and `ZipPack` `getContents` vtable slots and rewrites the `menu.copyright=`
line in `.lang` files plus the development control in `start_screen.json`. Both hooks check
the original slot value against the profile before patching.

The `ClientInstance` hooks are also profile-driven:

* vtable slot `+0x338` must contain `0x0010B0D0`; the host replaces it with
  `client_scene_stack_116`, which records the live `ClientInstance` and returns
  `*(void **)(client + 0x310)`;
* vtable slot 0 must contain `0x00100910`; the host replaces it with `client_destroy_116`,
  which clears the cached client and cursor-request scene before calling the original
  destructor.

## 6. Rendering, Present and resize

The host creates the HWND swap chain and hands it to the game through the `D3D11CreateDevice`
compatibility hook. The chain is windowed, `DXGI_SWAP_EFFECT_DISCARD`, with
`BufferCount = host_is_windows7 ? 1 : 2`.

The game does resize its chain after a `SizeChanged`, but it passes
`DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` (`0x800`). That flag is only valid for flip-model chains;
on the host's DISCARD chain the game's own `ResizeBuffers` fails with
`DXGI_ERROR_INVALID_CALL`, and the game then re-acquires the old back buffer. The host:

* strips `0x800` from `ResizeBuffers` flags for its own chain (`resize_buffers_116_hook`);
* also resizes from `GetBuffer(0)` (`get_buffer_116_hook` -> `ensure_swap_chain_size_116`),
  after the game has released its views but before it receives the buffer, so the game's view
  creation picks up the new size.

`ensure_swap_chain_size_116` clears the render targets, flushes the context and calls
`ResizeBuffers(0, client_width, client_height, DXGI_FORMAT_UNKNOWN, 0)`.

`Present` and `Present1` are both hooked. The hooks force the host sync interval, strip
`DXGI_PRESENT_DO_NOT_WAIT`, forward to the real method and increment
`game_present_success_count`. `frame_114` compares the counter before and after its own work;
if the game already presented, the host does not present a second time. Presenting twice on a
DISCARD chain displays the discarded (often black) buffer and was visible as heavy flicker.
`Present1` is hooked through vtable slot 22 so both paths are covered.

On Windows 7 the host also:

* uses a one-buffer chain;
* downgrades the game's `IID_IDXGIFactory4` probe to `IID_IDXGIFactory` in the image, because
  the Windows 7 DXGI runtime does not provide `IDXGIFactory4`;
* installs the `D3D11CreateDevice` compatibility hook for the feature-level/flag differences.

The renderer is shared with the 1.16 ports. The 1.14 session used it for the menu and in-game
rendering; window resize was not re-measured on this build, but there is no 1.14-specific
renderer code.

## 7. Input, mouse capture and survival

The game reads `CoreWindow` pointer and keyboard events, and its HID layer polls
`CoreWindow.GetAsyncKeyState`/`GetKeyState` every frame. The host implements the fake
`CoreWindow` vtable and drives the HWND capture from two signals:

* the game's explicit `ICoreWindow::SetPointerCapture` (slot 23) and
  `ICoreWindow::ReleasePointerCapture` (slot 22) calls;
* the top scene's `is_showing_menu` / `should_steal_mouse` flags.

`put_PointerCursor` is also meaningful: `put_PointerCursor(NULL)` is a relative-mouse request,
while a non-NULL cursor is an absolute request. The host saves and releases the cursor object
with proper refcounts. The default cursor starts as the arrow, not NULL, because the game saves
the cursor before hiding it and restores it when opening UI; starting with NULL made that
restore request relative mode again.

### 7.1 Scene refresh is a tri-state probe

`refresh_scene_mouse_116` captures the live `ClientInstance` through the scene-stack getter,
reads the top scene, and compares its vtable with the profile's `ui_scene_vtable`. It only
probes the **exact** base `UIScene` vtable:

```c
if (scene && !IsBadReadPtr(scene, 0x1c) &&
    scene_vtable == scene_image_116 + game_116->ui_scene_vtable) {
    menu_flag = is_showing_menu(scene);
    game_flag = should_steal_mouse(scene);
    if (menu_flag)      gameplay = FALSE;
    else if (game_flag) gameplay = TRUE;
}
```

The result is tri-state:

* `menu=1` -> release the capture and show the UI cursor;
* `game=1` (menu clear) -> capture the mouse for gameplay;
* `menu=0, game=0`, or a foreign vtable -> neutral: keep whatever the game requested through
  `SetPointerCapture`/`ReleasePointerCapture`.

This is not cosmetic. Probing a transitioning derived screen is unsafe: a
`CubemapBackgroundScreen` caught mid-construction/destruction has a stale inner object at
`+0x18`, and calling `is_showing_menu` on it crashed at VA `0xA61F39` (the inner object's
`+0x44` was garbage). The base `UIScene` at `126C8010` / `24CED6B8` reports `menu=0 game=0`;
without the neutral state it would release the capture that `SetPointerCapture` had just
granted, and the mouse would never stay locked in gameplay.

### 7.2 Explicit capture calls win

`core_window_set_pointer_capture_116` and `core_window_release_pointer_capture_116` are the
1.14 gameplay path. They set `mouse_relative_requested_116` directly and log:

```
1.16 CoreWindow.SetPointerCapture
1.16 relative mouse requested
1.16 relative mouse captured
```

```
1.16 CoreWindow.ReleasePointerCapture
1.16 absolute mouse requested
1.16 relative mouse released
```

`put_PointerCursor` does the same through the cursor object:

```
1.16 relative mouse requested     (value == NULL)
1.16 absolute mouse requested     (value != NULL)
```

The `cursor_request_pending_116` guard is armed **only for relative requests**. A native
cursor request during a closing animation takes effect immediately and is kept until that
scene leaves the stack; otherwise the menu flag would undo the game's hide request every
frame. An absolute request must not arm the guard: it would stop the scene refresh from
re-entering capture after the transition.

`update_relative_mouse_116` applies the request: `ClipCursor` to the client rectangle,
`SetCapture(window)`, `ShowCursor(FALSE)` and a recenter to the client center. On release it
clears the clip, releases the capture, restores the arrow and logs
`1.16 relative mouse released`.

### 7.3 Survival: real mouse-button state

The fake `CoreWindow` key-state method is used by the game's HID update. The original host
returned 0 for every key. That made a held attack button look like a one-frame tap, so
survival block breaking never progressed (the game re-read the button as up on the next
frame).

The fix is in `win32_host.c`:

```c
static HRESULT WINAPI fake_core_window_key_state(
    FakeInspectable *object, INT key, INT *result)
{
    extern SHORT WINAPI GetAsyncKeyState(int);
    SHORT state = 0;
    if (!result) return E_POINTER;
    if (key >= 1 && key <= 6) {
        state = GetAsyncKeyState(key);
        *result = (state & 0x8000) ? 1 : 0;
    } else {
        *result = 0;
    }
    ...
}
```

Only the mouse buttons (VK 1-6) are translated; keyboard input already arrives through the
`KeyDown`/`KeyUp` events. `GetAsyncKeyState` must be declared manually because the MinGW
headers used for this build do not declare it. The log prints one line per key:

```
1.16 CoreWindow key state: vk=1 down=1
```

The user confirmed that survival block breaking works after this change.

## 8. FMOD delay imports and the remaining sound failure

The 1.14 exe uses a 42-entry delay-import descriptor at `0x0215C0F0`. The profile selects the
descriptor and the expected count; the host does not hardcode the descriptor's internal RVAs.
The resolver validates the descriptor shape (attributes `1`, non-zero and in-image
name/module/IAT/INT fields, name exactly `fmod.dll`, count exactly 42), loads `fmod.dll` with
`LoadLibraryW`, resolves every name or ordinal, and writes the module handle back into the
descriptor.

The common audio calls are routed through the host's diagnostic bridge (`bridge_fmod_*`),
including path translation for `ms-appx:///` sound paths and output selection. The bridge
selection is **name-based** (`fmod_bridge_target_116` matches C++ mangled prefixes such as
`?getVersion@System@`), so 1.14 and 1.16 share the same resolver even though the descriptor
addresses and counts differ. The log line is:

```
Minecraft desktop FMOD: 42 delay imports resolved
```

Sound still does not play on the test machine. The bridge log shows the same failure as the
1.16 ports:

```
FMOD bridge: System_Create result=0 system=...
FMOD bridge: getVersion result=0 version=0x10904
FMOD bridge: setOutput(WASAPI) -> 0
FMOD bridge: pre-init drivers output=WASAPI result=0 count=1
FMOD bridge: setDriver(0) -> 0
FMOD bridge: init output=WASAPI channels=32 flags=0x4 result=28
FMOD bridge: setOutput(DSOUND) -> 54
FMOD bridge: setOutput(WINMM) -> 54
```

`28` is `FMOD_ERR_OUTPUT_INIT`, `54` is `FMOD_ERR_OUTPUT_CREATEBUFFER`. The shipped DLL is the
UWP FMOD 1.09.04 build (`getVersion` `0x10904`), so this looks like a package-identity /
audio-graph limitation of that build rather than a missing import or a wrong descriptor. The
game continues silently.

The package's `fmod.dll` is a patched copy of the retail `fmod_X86.dll`, not a different DLL.
Both are 1,338,880 bytes, but the retail one imports `VCRUNTIME140_APP.dll`; the patcher
rewrites that import before copying.

## 9. Data root

The host creates:

```
%APPDATA%\MinecraftPE\
  games\
    com.mojang\
      minecraftpe\
        options.txt
```

`options.txt` is seeded with the standard x86 defaults (`mp_username:Steve`,
`game_difficulty_new:1`, `game_language:en_US`, `gfx_renderdistance_new:96`, etc.). If the
file already exists and is non-empty, it is left alone; an existing empty file is regenerated.
The 1.14 game rewrites `old_game_version_major/minor/patch/revision` to its own version and
stores the selected skin in `game_skintypefull`. The game's own saves, resource packs and skins
live under the same `com.mojang` tree.

`WIN32CRAFT_DEBUG=1` enables `win32craft.log` next to the executable. Without it the host is
silent. The log is append-only and is the first place to look for a profile mismatch, a failed
hook signature, a missing DLL or the FMOD result. Many log lines still say `1.16` because the
code is shared with the 1.16 ports; on 1.14 read them as "the shared x86 host", not as a
version check.

## 10. Package mode and the `STATUS_DLL_NOT_FOUND` trap

Build a package from an extracted folder or directly from the `.appx`:

```
python tools\1.14.6005.0-patcher.py SOURCE OUTPUT --package --host path\to\win32craft.dll
```

`SOURCE` may be the extracted game folder or an `.appx`; `OUTPUT` must not be the source and
must not be inside it. `--force` replaces an existing output. `--exe-name` changes the patched
executable name; the default is `Win32Craft.1.14.60.5.exe`. The packager:

1. copies or extracts the game;
2. removes `AppxSignature.p7x`, every other `*.p7x`, `AppxMetadata\` and the IDA `*.i64`
   databases (package identity files and analysis databases are meaningless in an unpackaged
   folder);
3. removes a leftover `Minecraft.Windows.exe.orig` stock backup, if the imported folder has
   one;
4. patches `Minecraft.Windows.exe` to `Win32Craft.1.14.60.5.exe` and deletes the stock exe;
5. rewrites the `*_APP` CRT imports in `ChakraCore.dll`, `cohtml.UWP.dll`,
   `HttpServer.UWP.dll`, `MediaDecoders.UWP.dll`, `RenoirCore.UWP.dll` and `fmod_X86.dll`;
6. copies the patched `fmod_X86.dll` to `fmod.dll`;
7. copies the host DLL to `win32craft.dll`;
8. writes `.Win32Craft` with the exact ASCII text `Minecraft Bedrock Edition` (no trailing
   newline), so repack tools can identify the folder without guessing from the exe name;
9. writes `PATCH_NOTES.md` with the input/output hashes and sizes.

The CRT step is the one that costs the most time. The retail game DLLs import
`vccorlib140_app.DLL`, `VCRUNTIME140_APP.dll` and `MSVCP140_APP.dll`. Those names are only
resolvable inside the package graph; an unpackaged process fails in the loader with
`STATUS_DLL_NOT_FOUND` (`0xC0000135`, process exit `-1073741515`) before a single line of host
code runs. Windows then shows the missing-DLL dialog, and because each DLL has its own missing
import, the dialog can repeat cyclically. The host log stays empty, which makes it look like
the exe patch failed.

The 1.14 test package was assembled from an imported folder; do not assume its DLLs are already
desktop-clean just because the game starts. Run the CRT rewrite on every package, including
`fmod_X86.dll` before copying it to `fmod.dll`.

`AppxManifest.xml`, `AppxBlockMap.xml` and `[Content_Types].xml` remain in the folder but are
inert for the unpackaged process.

## 11. Build and run

The repository baseline is `build_windows_mingw.bat` (32-bit MinGW-w64 GCC + `lld-link`). The
verified 1.14 host was built with the LLVM-MinGW clang driver and `ld.lld`, with the extra
`ctorstub.obj` / `interlocked.obj` and the compiler runtime libraries:

```
C:\llvm-mingw\bin\i686-w64-mingw32-clang.exe -c -O2 -fno-stack-protector -fno-builtin ^
  -Wno-incompatible-pointer-types -Wno-attributes win32_host.c ^
  -o C:\Users\Max\AppData\Local\Temp\w32c-114-build\win32_host.obj

C:\llvm-mingw\bin\i686-w64-mingw32-clang.exe -c -O2 -fno-stack-protector -fno-builtin ^
  -Wno-incompatible-pointer-types -Wno-attributes ^
  -DWin32Bootstrap=Win32BootstrapEarly2016 -DDllMain=DllMainEarly2016 ^
  win32_host_early2016.c -o ...\win32_host_early2016.obj

C:\llvm-mingw\bin\i686-w64-mingw32-clang.exe -c -O2 ... api_compat.c -o ...\api_compat.obj

ld.lld -flavor link /dll /machine:x86 /entry:DllMain@12 /safeseh:no ^
  /def:...\win32craft.def ...\dllcrt2.o ...\crtbegin.o ^
  win32_host.obj win32_host_early2016.obj api_compat.obj ^
  ...\ctorstub.obj ...\interlocked.obj ^
  libmingw32.a libclang_rt.builtins-i386.a libunwind.a libmoldname.a ^
  libmingwex.a libmsvcrt.a libadvapi32.a libcomdlg32.a libshell32.a ^
  libuser32.a libkernel32.a ...\crtend.o

python C:\Users\Max\AppData\Local\Temp\w32c-114-build\fixfwd.py raw.dll win32craft.dll
```

`fixfwd.py` is mandatory after every link with this toolchain: `lld-link` spells x86 PE
forwarder module names as `_KERNEL32.`; the script shifts the string left and NUL-terminates it.
The tested 1.14 host has 130 exports and 61 forwarders. Without the fix the host's API-set
exports are unresolved.

The exact object paths and runtime libraries live in
`C:\Users\Max\AppData\Local\Temp\w32c-114-build\`; that directory is not part of the
repository. The source changes are in `win32_host.c`, `win32_host_116.inl` and
`tools\1.14.6005.0-patcher.py`.

Run:

```
set WIN32CRAFT_DEBUG=1
Win32Craft.1.14.60.5.exe
```

The window title should be `Win32Craft 1.14.60.5`. The first log lines should include:

```
Win32Craft 1.14.60.5 bootstrap entered
Minecraft 1.14.60.5 development label enabled
1.1.5 stdio hook installed: 1.16 ClientInstance scene stack original=0050B0D0
Minecraft desktop FMOD: 42 delay imports resolved
1.16 HWND ready; entering original application startup
```

## 12. Verification checklist

1. `Get-FileHash` on the package:
   * `Win32Craft.1.14.60.5.exe` = `90E9DE4E659EDDE22273AC03DC834E16E5D1FC78D2E02649987BFD135190611E`
     (42,194,944 bytes, `SizeOfImage 0x028B8000`); the stock `Minecraft.Windows.exe` is
     absent and no backup exe remains;
   * `win32craft.dll` = `FEC34FBC8AAD975E6B206F7AB2A69B38E0B04851F35A17ADFC29136F75FE4267`
     (246,784 bytes);
   * `fmod.dll` = `15CF102EFEC2FAF846C13201DD707C23A05BB58869E6DA71D03D3D3B869E4174`
     (1,338,880 bytes);
   * `.Win32Craft` = `Minecraft Bedrock Edition` (no trailing newline);
   * no `*.p7x` or `*.i64` files remain.
2. Copy the package to a clean directory (do not run it from the build output) and start
   `Win32Craft.1.14.60.5.exe` with `WIN32CRAFT_DEBUG=1`.
3. Confirm the window title is `Win32Craft 1.14.60.5`.
4. Confirm the log has the bootstrap line, the development-label line and the 42-import FMOD
   line.
5. Open the main menu, load a world, confirm the mouse is captured in gameplay, hold the
   attack button to break a survival block, then press Esc and confirm the cursor is released.
6. Exit the world to the main menu, load a second world (or the same world again), and confirm
   the game does not crash in the packet dispatcher. Repeat a third entry to exercise the
   listener vectors twice.
7. Quit the game normally: confirm a clean exit with no crash dialog and no Store rating
   prompt.
8. Sound is expected to be absent on this machine with the current UWP FMOD build; check the
   `init result=28` line before treating it as a new regression.

Validation note (2026-11-23): the rebuilt package completed world entry, exit, re-entry (twice
in a row), and a normal quit without a crash dialog or the Store rating prompt. The log ended
with `1.16 original startup returned: 00000000`.

## 13. Traps that cost time

1. **Patch `GetActivationFactoryByPCWSTR`, not `RoGetActivationFactory`.** The 1.14 game calls
   the vccorlib helper at IAT `0x01C0CCD0` for projected types. The helper resolves
   `RoGetActivationFactory` inside `vccorlib140.dll`, so patching the exe's
   `RoGetActivationFactory` IAT (`0x01C0C790`) does not intercept it, and
   `PointerVisualizationSettings` still fails in an unpackaged process.
2. **The `IPointerVisualizationSettings` table needs 10 slots.** 1.14 calls slots 6 and 8
   (the two setters). A table with only two entries reads past the end. Slots 6-9 must be
   `put/get` for contact feedback and barrel-button feedback.
3. **The world-entry Chakra predicate is not optional in the stock exe.** `sub_12EFEB0` calls
   the first virtual method of a `ChakraInterface` before its script state is ready and faults.
   Patch RVA `0xEEFEF5` to `mov al,1` + NOPs. Do not try to fake the interface from the host
   without understanding the script API; the call is skipped, not reimplemented.
4. **Probe only the exact `UIScene` vtable.** Derived screens can be caught mid-transition
   with a stale inner object at `+0x18`. Calling `is_showing_menu` on a transitioning
   `CubemapBackgroundScreen` crashed at VA `0xA61F39`.
5. **Scene flags are tri-state.** A neutral scene (`menu=0, game=0`) or a foreign vtable must
   keep the current capture state. Otherwise the refresh releases the capture that
   `SetPointerCapture` just granted, every frame.
6. **The pending-cursor guard is only for relative requests.** `put_PointerCursor(NULL)` during
   a closing animation takes effect immediately and is kept until that scene leaves the stack.
   An absolute request must not arm the guard, or the scene refresh cannot re-enter capture.
7. **`put_PointerCursor(NULL)` means relative, non-NULL means absolute.** The game saves the
   cursor object before hiding it and restores it when opening UI; starting the fake with NULL
   makes that restore request relative mode again.
8. **Mouse buttons must come from `GetAsyncKeyState`.** Returning 0 for VK 1-6 makes a held
   attack look like a one-frame tap and survival block breaking never progresses. Keyboard
   input stays event-driven.
9. **Clear `DYNAMIC_BASE`.** The bootstrap patch uses an absolute VA for the host IAT. If the
   image relocates, the call site points at the wrong address. Also clear `APPCONTAINER`: the
   process is unpackaged.
10. **`SizeOfImage` is the profile key.** Adding `.d3imp` changes it from `0x028B7000` to
    `0x028B8000`. Running the host against the stock exe silently selects the 1.16.20.03
    profile and fails the signature checks.
11. **The FMOD descriptor is version-specific.** 1.14 has 42 imports at `0x0215C0F0`; 1.16 has
    48 at different addresses. Select both from the profile and validate the descriptor shape,
    name and count before resolving. The bridge itself is name-based and can be shared.
12. **Patch `fmod_X86.dll` before copying it to `fmod.dll`.** The retail FMOD DLL itself has a
    `VCRUNTIME140_APP.dll` import. Copying first and patching only the copy is fine only if the
    copy is patched; copying the retail file unpatched produces a loader failure when FMOD is
    first used.
13. **`*_APP` CRT imports fail before `Win32Bootstrap`.** The retail DLLs are package-graph
    binaries. An unpackaged loader cannot resolve `vccorlib140_app.DLL`,
    `VCRUNTIME140_APP.dll` or `MSVCP140_APP.dll`; the process exits with `0xC0000135` and the
    host log is empty. Patch the import descriptor names in every game DLL, not only the exe.
14. **The 1.14 package folder may contain a stock backup exe.** Imported folders sometimes keep
    `Minecraft.Windows.exe.orig` next to the game. The packager removes it; if you assemble a
    folder by hand, do not ship two 42 MB executables.
15. **Every world re-initializes the client script engine.** `MinecraftClientScriptEngine`
    (`0xDA6DB0`) replaces four listeners on every world load and frees the previous instances.
    The stock code does not unregister them from their manager vectors, so the second world
    leaves stale pointers that the dispatcher later dereferences as vtables. Remove the old
    pointer from its manager vector before the deleting destructor; the four call sites are
    `0xDA6E26`, `0xDA6EC6`, `0xDA6F50` and `0xDA6FC2`, patched to stubs in `.d3imp`. A crash
    dump of the unfixed exe shows `EIP` inside recycled heap string data, not module code; do
    not look for a missing or corrupted function in `.text`.

## 14. Open items

* Audio output. The next useful experiment is a desktop FMOD build with the same 1.09.04 ABI
  (or a package-identity shim for the UWP build), then re-check `System::init`. The current
  failure is `FMOD_ERR_OUTPUT_INIT` / `FMOD_ERR_OUTPUT_CREATEBUFFER`, not a missing import.
* Window resize and the skin picker are inherited from the shared 1.16 host. They were not
  re-measured in the 1.14 session; a clean 1.14 resize and PNG skin import would close that
  gap.
* The world-entry fix is a workaround. A proper `ChakraInterface` readiness fake would be
  better if 1.14 script features (add-ons, UI scripts) need to work.
* ARM32 1.14 is a separate port; this document is x86-only.
* The 1.16.20.03 and 1.16.40.02 packages share the modified scene refresh and mouse-key fake.
  They must be rebuilt and retested with the current host before declaring them unaffected.
