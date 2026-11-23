# Minecraft 1.16.40.02 x86 - full porting record

| | |
|---|---|
| Game | Minecraft Bedrock Edition (Mojang / Microsoft) |
| Version | 1.16.40.02, x86 UWP APPX, package `Microsoft.MinecraftUWP_1.16.4002.0_x86__8wekyb3d8bbwe` |
| Engine | native C++ Bedrock engine, Windows inbox Chakra JavaScript (`ChakraCore.dll` remains for cohtml), cohtml UI; no .NET game logic |
| Host | `win32craft.dll` (Win32Craft x86 host), profile `game_profile_116_04002` |
| Patcher | `tools/1.16.40.02-patcher.py` |
| Package | `dist-1.16.40.02\Minecraft_1.16.40.02_x86_Win32Craft` (gitignored), exe `Win32Craft.1.16.40.02.exe` |
| Verified on | Windows desktop x86: menu, world load, skin picker, resize, window title |
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
| Exe patcher / packager | `tools/1.16.40.02-patcher.py` |
| Host exports | `win32craft.def` |
| Retail source folder used for the package | `C:\Users\Max\Documents\!TMP\Microsoft.MinecraftUWP_1.16.4002.0_x86__8wekyb3d8bbwe` |
| Ready package | `dist-1.16.40.02\Minecraft_1.16.40.02_x86_Win32Craft` |
| Host log | `win32craft.log` next to the exe, only with `WIN32CRAFT_DEBUG=1` |
| Test build directory (not in git) | `C:\Users\Max\AppData\Local\Temp\w32c-build\` |

`win32_host_116.inl` is shared with 1.16.20.03. The runtime picks the profile from the patched
image size; every address below is only valid for 1.16.40.02.

## 2. Why 1.16.40.02 needed its own profile and patcher

The 1.16.20.03 host profile does not work on this build. The engine is recognisably the same,
but almost every address moved:

| Thing | 1.16.20.03 | 1.16.40.02 |
|---|---|---|
| Patched `SizeOfImage` (profile key) | `0x02DB1000` | `0x02DAF000` |
| ClientInstance vtable | `0x024E7FAC` | `0x024E5FAC` |
| Top UIScene vtable | `0x025226D4` | `0x025206D4` |
| DirectoryPack `getContents` slot | `0x02551B30` | `0x0254FB14` |
| ZipPack `getContents` slot | `0x02552348` | `0x02550348` |
| `FrameworkView::Run` frame call | `0x016E469E` | `0x016E299E` |
| `$is_publish` initializer | `0x0183C94A` | `0x0183AC8A` |
| Executable `_main` | `0x01C48DEB` | `0x01C4682B` |
| FMOD delay descriptor | `0x024B25A4` | `0x024B01CC` |
| IAT block | `0x01EA6xxx` | `0x01EA4xxx` |
| `IID_IDXGIFactory4` copy | `0x0204244C` | `0x02040358` |

Two retail x86 revisions of the same build are in circulation. They differ by one build-variant
byte at file offset `0x188CC5` (`mov bl, 0` vs `mov bl, 1`) and every patch site is identical.
The patcher accepts both:

| SHA-256 | Revision |
|---|---|
| `ebf9e37dd527dba7bfffea494c759cb099fdff29be125fc50f1d1b4ab959fe5d` | retail x86 revision A |
| `536d589921d0642ef59d6fd14aed1d32b6ee2351173ff3a63fd3b41b49d982c9` | retail x86 revision B (`Microsoft.MinecraftUWP_1.16.4002.0_x86`) |

The source used for the shipped package is revision B. Its stock `SizeOfImage` is
`0x02DAE000`; the patched image is `0x02DAF000`.

The patcher refuses an unknown input hash instead of applying bytes at the wrong offsets. The
host likewise validates every hook site signature and every profile vtable slot before it
patches anything, so a mismatched exe produces a log line instead of a crash deep in the WinRT
fakes.

## 3. What the patcher changes in the exe

### 3.1 PE header

| Field | Original | Patched | Why |
|---|---|---|---|
| `IMAGE_FILE_LARGE_ADDRESS_AWARE` | `0x0122` | `0x0122` | verified, not changed |
| OS version minor | `2` | `1` | image claims Windows 7 compatibility |
| Subsystem version minor | `2` | `1` | same |
| DLL characteristics | `0x9140` | `0x8100` | clears `APPCONTAINER` (`0x1000`) and `DYNAMIC_BASE` (`0x0040`) |

The package is an unpackaged Win32 process, so the loader must not treat the image as an
app-container image.

### 3.2 Code patch sites

All values are RVAs. The patcher prints the corresponding file offset (`file+0x...`) for each
site; in `.text` the file offset is RVA minus `0xC00` on this build.

| RVA | Original bytes | Patched bytes | Purpose |
|---|---|---|---|
| `0x016E225D` | `FF 50 08` | `90 90 90` | keep the temporary delegate alive while skipping the pointer-visualization block |
| `0x016E2260` | `E8 CB 34 00 00` | `E9 48 00 00 00` | skip `PointerVisualizationSettings` setup and continue at the CoreWindow event registration |
| `0x016D1302` | `FF 15 28 44 2A 02` | `33 C0 5D C2 08 00` | an optional startup notification may hold an empty `std::function`; return 0 from the two-argument wrapper |
| `0x016D25FB` | `FF 15 28 44 2A 02` | `E9 2A 00 00 00 90` | same notification from the loading-state transition: skip the empty invoke and continue to local cleanup |
| `0x016E3ABF` | `FF 15 E0 4B 2A 02` | `33 C0 90 90 90 90` | activation diagnostic stringifies a boxed WinRT enum; use a null HSTRING instead of the aborting `Enum::ToString` |
| `0x016E3EAE` | `FF 15 E0 4B 2A 02` | `33 C0 90 90 90 90` | same, window-state diagnostic |
| `0x0167906` | `75 85` | `EB 85` | force the "rating prompt already shown" branch; suppresses the "Did you like Minecraft?" Store rating prompt |
| `0x01C44BEE` | `50 57 68 00 00 40 00 E8 3F 1B 00 00` | `FF 15 <IAT> 90 90 90 90 90 90` | replace the game's `WinMain` call with an indirect call to `win32craft.dll!Win32Bootstrap` |

The three pointer-visualization patches are not cosmetic: `PointerVisualizationSettings` has no
desktop equivalent and its C++/CX projection needs an agile WinRT factory identity that an
unpackaged process does not have. The two callback patches avoid `std::_Xbad_function_call` /
`_invoke_watson`; the game treats the optional callback as a no-op and continues. The enum
diagnostics are optional logging; Wine's `Enum::ToString` aborts, a null HSTRING is a valid
empty diagnostic.

The rating gate at RVA `0x0167906` (file offset `0x166D06`, VA `0x567906`) is the
`RatingPromptController` path. The function reads the persisted option ID `0x174`
(`shown_ratings_prompt`); when it is false it reads the launch counter ID `0x175` and shows the
prompt after five launches. The prompt is the in-game `rating_prompt.rating_prompt_screen` flow
that talks to `Windows.Services.Store.StoreContext`. That UWP Store projection is unavailable in
the unpackaged host and crashed after save-on-exit. The `jne` short branch skips the entire
prompt body when the option is already set; patching it to an unconditional `jmp` makes every
run take that branch, so no Store object is ever created.

### 3.3 String and import surgery

The patcher replaces equal-length ASCII strings in place, padding with NUL:

| Original | Replacement | Count | Why |
|---|---|---|---|
| `vccorlib140_app.DLL` | `vccorlib140.DLL` | 1 | package-only CRT -> desktop CRT |
| `MSVCP140_APP.dll` | `MSVCP140.dll` | 1 | same |
| `CONCRT140_APP.dll` | `CONCRT140.dll` | 1 | same |
| `VCRUNTIME140_APP.dll` | `VCRUNTIME140.dll` | 1 | same |
| `fmod_X86.dll` | `fmod.dll` | 2 | the package ships the desktop-named copy |

The import directory is then rebuilt. The retail image has 32 descriptors and an import
directory size of `0x294`. Descriptor names are changed without moving the descriptor array:

* `dxgi.dll`, `d3d11.dll`, `xinputuap.dll` and the WinRT API-set DLLs are renamed to
  `win32craft.dll`. The host DLL exports the same functions (directly or as forwarders), so the
  host can install `D3D11CreateDevice`, `D3DCompile`, DXGI factory and WinRT compatibility
  shims before the game resolves them.
* `chakra.dll` stays unchanged by default, so the game uses the Windows inbox Chakra engine,
  matching the retail package. The diagnostic `--chakra-core` mode instead renames it to
  `ChakraCore.dll`, the DLL the retail package ships; that mode changes engine behavior and
  breaks experimental-gameplay scripts. `cohtml.UWP.dll` imports `ChakraCore.dll` in either
  mode, so the package keeps that DLL.
* A new `Win32Bootstrap` descriptor is appended. Its ILT/IAT live in the new `.d3imp` section,
  and the `WinMain` patch calls through that IAT.
* In `--chakra-core` mode only, a new `JsStartDebugging` descriptor points at the existing chakra
  IAT slot; its ILT entry is replaced with `JsAddRef`, so the loader never fails on a debugging
  export the package ChakraCore does not provide. The default system-chakra build keeps the real
  `JsStartDebugging` import from `chakra.dll`.
* Three delay-load helper imports (`ResolveDelayLoadedAPI`, `ResolveDelayLoadsFromDll`,
  `DelayLoadFailureHook`) are handled the same way: their ILT entries are replaced with
  `GetLastError`, and new descriptors point at the existing IAT slots. This keeps the image
  loadable on Windows 7, where those newer kernel32 exports do not exist.

The replacement names are shorter or equal in length, so no RVAs move.

### 3.4 The `.d3imp` section and the bootstrap call

A new section is appended after the last section:

* name `.d3imp`, characteristics `0xC0000040` (initialized data, read/write);
* it contains the relocated import descriptor array, the `win32craft.dll` name, hint/name
  entries, ILTs and the `Win32Bootstrap` IAT; in `--chakra-core` mode it also holds the
  `ChakraCore.dll` name and the `JsStartDebugging` placeholder descriptor;
* the default (system chakra) build appends 36 descriptors plus the terminator: the 32 retail
  descriptors, the bootstrap and the three delay-load helpers. `--chakra-core` adds one more;
* `SizeOfImage` grows from `0x02DAE000` to `0x02DAF000` (the host profile key);
* the bootstrap IAT VA is absolute (`0x400000 + new_rva + offset`), because the call site is a
  normal indirect `call dword ptr [abs]` in the patched image.

The `WinMain` call site is the last patch. The host's `Win32Bootstrap` creates the window and
then calls the executable's real `_main` at `main_entry` (`0x01C4682B`). The original
`WinMain` wrapper is not used.

### 3.5 Host DLL name

The default import name is `win32craft.dll`. `--host-dll NAME` changes it, but the name must be
ASCII, non-empty and fit the same string surgery; the package mode validates that the supplied
host is an x86 PE32 DLL.

## 4. Host profile `game_profile_116_04002`

The profile is selected in `bootstrap_116` from the PE `SizeOfImage`:

```c
game_116 = image_size == 0x02daf000 ? &game_profile_116_04002
                                    : &game_profile_116_02003;
```

If the exe is not patched, `SizeOfImage` is `0x02DAE000`, the host falls into the 1.16.20.03
profile and every signature check fails. That is intentional: the error is visible in the log
instead of becoming a random crash.

Game structure addresses:

| Field | RVA | Meaning |
|---|---|---|
| `client_instance_vtable` | `0x024E5FAC` | `ClientInstance` vtable |
| `client_destroy` | `0x0010F440` | scalar deleting destructor |
| `client_scene_stack` | `0x0011A7F0` | `SceneStack` getter (vtable `+0x338`) |
| `client_top_scene` | `0x0011A740` | `getTopScene` |
| `ui_scene_vtable` | `0x025206D4` | top `UIScene` vtable |
| `gameplay_flag_menu` | `0x006A71F0` | `is_showing_menu` |
| `gameplay_flag_game` | `0x006A7230` | `should_steal_mouse` |
| `game_string_assign` | `0x000DFFC0` | game string assign |
| `game_string_destroy` | `0x000E0280` | game string destroy |
| `directory_get_contents_slot` | `0x0254FB14` | `DirectoryPack::getContents` vtable cell |
| `directory_get_contents` | `0x00F23800` | `DirectoryPack::getContents` |
| `zip_get_contents_slot` | `0x02550348` | `ZipPack::getContents` vtable cell |
| `zip_get_contents` | `0x00F64CF0` | `ZipPack::getContents` |
| `frame_site` | `0x016E299E` | `FrameworkView::Run` frame call site |
| `development_label_site` | `0x0183AC8A` | `$is_publish` initializer |
| `main_entry` | `0x01C4682B` | executable `_main` |
| `fmod_delay_descriptor` | `0x024B01CC` | FMOD delay-import descriptor |
| `dxgi_factory4_iid` | `0x02040358` | `IID_IDXGIFactory4` copy for the Windows 7 probe |

IAT slots patched by the host:

| Field | RVA | Hook |
|---|---|---|
| `iat_exception_trace` | `0x01EA4654` | `_CxxThrowException` trace |
| `iat_activation_redirect` | `0x01EA4BCC` | `GetActivationFactoryByPCWSTR` |
| `iat_vccorlib_ctor` | `0x01EA4BC4` | `Platform::Object` constructor |
| `iat_vccorlib_allocate2` | `0x01EA4C40` | `Platform::Heap::Allocate(size, align)` |
| `iat_vccorlib_allocate1` | `0x01EA4C6C` | `Platform::Heap::Allocate(size)` |
| `iat_vccorlib_ibox_array` | `0x01EA4C8C` | `GetIBoxArrayVtable` |
| `iat_vccorlib_ibox` | `0x01EA4BF4` | `GetIBoxVtable` |
| `iat_command_arguments` | `0x01EA4C60` | `GetCmdArguments` |
| `iat_d3d_compile` | `0x01EA401C` | `D3DCompile` |
| `iat_d3d11_create_device` | `0x01EA4B90` | `D3D11CreateDevice` |
| `iat_fopen` | `0x01EA4940` | `fopen` |
| `iat_wfopen` | `0x01EA4948` | `_wfopen` |
| `iat_wfopen_s` | `0x01EA499C` | `_wfopen_s` |
| `iat_vsscanf` | `0x01EA4978` | `__stdio_common_vsscanf` |
| `iat_invalid_parameter` | `0x01EA48D0` | `_invalid_parameter_noinfo` |
| `iat_invalid_parameter_noreturn` | `0x01EA48C8` | `_invalid_parameter_noinfo_noreturn` |
| `iat_terminate` | `0x01EA48C0` | `terminate` |
| `iat_std_terminate` | `0x01EA463C` | `__std_terminate` |
| `iat_report_unobserved` | `0x01EA4584` | `_ReportUnobservedException` |

The vccorlib and stdio hooks are the same compatibility layer used by the other 1.16 ports.
They are installed only after the profile is selected, so the 1.16.20.03 profile keeps its own
addresses.

## 5. Bootstrap and window

`bootstrap_116` does, in order:

1. select the profile and set `fmod_delay_descriptor_116`;
2. detect Windows 7 through `GetVersion` (the host creates a one-buffer DISCARD chain there);
3. install the frame hook, development label and language override;
4. hook the `ClientInstance` vtable (scene stack getter and destructor);
5. install the crash/exception trace, `RoInitialize`, package/application-data redirection,
   vccorlib compatibility, command-line and stdio hooks;
6. install the `D3DCompile` and `D3D11CreateDevice` compatibility hooks;
7. resolve the 48 FMOD delay imports;
8. create the `MCPE116Win32` window class and the game window;
9. call the executable's real `_main` at `main_entry`.

The window title is built by the host, not by an exe patch:

```c
lstrcpyW(title, L"Win32Craft ");
lstrcatW(title, game_116->version_wide);
```

`version_wide` is `L"1.16.40.02"`, so the actual title is `Win32Craft 1.16.40.02`. This was
verified with `Get-Process ... MainWindowTitle`; the earlier host built only the version string.
The class name is `MCPE116Win32`.

`install_frame_116` replaces the `FrameworkView::Run` call site. The expected signature is
`8B 01 8B 40 68 FF D0` (`mov eax,[ecx]; mov eax,[eax+0x68]; call eax`); the first five bytes
become `E8 <rel32>` and the last two become NOPs. The frame callback drives cursor state and,
if the game did not present this frame, presents the host swap chain once.

`install_development_label_116` changes the `$is_publish` initializer `C6 45 A4 01` to
`C6 45 A4 00`, which enables the development label. `install_language_override_116` hooks the
`DirectoryPack` and `ZipPack` `getContents` vtable slots and rewrites the `menu.copyright=`
line in `.lang` files plus the development control in `start_screen.json`. Both hooks check
the original slot value against the profile before patching.

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
`ResizeBuffers(0, client_width, client_height, DXGI_FORMAT_UNKNOWN, 0)`. Verified sequence:
`1264x681 -> 1920x1017 -> 1264x681`, no device loss.

`Present` and `Present1` are both hooked. The hooks force the host sync interval, strip
`DXGI_PRESENT_DO_NOT_WAIT`, forward to the real method and increment
`game_present_success_count`. `frame_116` compares the counter before and after its own work;
if the game already presented, the host does not present a second time. Presenting twice on a
DISCARD chain displays the discarded (often black) buffer and was visible as heavy flicker.
`Present1` is hooked through vtable slot 22 so both paths are covered.

On Windows 7 the host also:

* uses a one-buffer chain;
* downgrades the game's `IID_IDXGIFactory4` probe to `IID_IDXGIFactory` in the image, because
  the Windows 7 DXGI runtime does not provide `IDXGIFactory4`;
* installs the `D3D11CreateDevice` compatibility hook for the feature-level/flag differences.

## 7. Input and the skin picker

The game reads `CoreWindow` pointer and keyboard events. The host's frame callback
(`refresh_scene_mouse_116`) captures the live `ClientInstance` through the scene-stack getter
and reads the top scene. It only enters relative-mouse mode when the top scene is the gameplay
`UIScene` (`ui_scene_vtable`) and the menu flag is clear while the gameplay flag is set. In
menus it restores the UI cursor. Relative mode uses `ClipCursor` plus `SetCapture`, and the
default pointer cursor starts as the arrow, not NULL: the game saves the cursor before hiding
it and restores it when opening UI; starting with NULL made that restore request relative mode
again.

The game's HID layer also polls `ICoreWindow::GetAsyncKeyState`/`GetKeyState` every frame. The
host originally returned 0 for every key, so a held mouse button looked like a one-frame tap:
survival mining stopped after the first frame and the view could snap back. The shared host now
translates VK 1–6 (`VK_LBUTTON` … `VK_XBUTTON2`) to the real `GetAsyncKeyState` state and returns
1 while the button is physically down. Keyboard input still arrives through the `KeyDown`/`KeyUp`
events, so only the mouse-button range is translated. This is host-side and therefore applies to
both 1.16.20.03 and 1.16.40.02; the package must be built with the current `win32craft.dll`
(`FEC34FBC…` for the tested build), not the older `A5449BD5…` host.

The skin picker is a real `FileOpenPicker` path. The game appends to
`FileOpenPicker.FileTypeFilter`, which is a projected `IVector<HSTRING>`. The host stores the
extensions, implements `RemoveAtEnd` and `Clear`, and builds the Win32 `OPENFILENAME` filter and
`lpstrDefExt` from them. The verified dialog title/filter for a PNG skin is
`PNG image (*.png)`; selecting a real PNG imports it. The filter table also maps `.jpg`,
`.jpeg`, `.bmp`, `.gif`, `.tga`, `.mcworld`, `.mcpack`, `.mcaddon`, `.mctemplate` and `.zip`,
with a wildcard fallback.

## 8. FMOD delay imports and the remaining sound failure

The 1.16 exe uses a 48-entry delay-import descriptor. The profile selects it; the host does
not hardcode the descriptor's internal RVAs, because they moved between 1.16.20.03 and
1.16.40.02:

| Descriptor field | 1.16.40.02 | 1.16.20.03 |
|---|---|---|
| descriptor | `0x024B01CC` | `0x024B25A4` |
| DLL name | `0x01EA8230` (`fmod.dll`) | `0x01EAA230` |
| module handle | `0x02AD8800` | `0x02ADA400` |
| IAT | `0x02AD8730` | `0x02ADA320` |
| INT | `0x024B020C` | `0x024B25E4` |
| entries | 48 | 48 |

`resolve_fmod_delay_imports_116` validates the descriptor shape (attributes `1`, non-zero and
in-image name/module/IAT/INT fields, name exactly `fmod.dll`, count exactly 48), loads
`fmod.dll` with `LoadLibraryW`, resolves every name or ordinal, and writes the module handle
back into the descriptor. The common audio calls are routed through the host's diagnostic
bridge (`bridge_fmod_*`), including path translation for `ms-appx:///` sound paths and output
selection. All 48 imports resolve against the shipped DLL; the log line is:

```
Minecraft 1.16 desktop FMOD: 48 delay imports resolved
```

Sound still does not play on the test machine. The bridge log shows:

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
host suppresses its own AudioGraph completion for the 1.16 FMOD fallback, so the game continues
silently. The `FMOD bridge: missing export FMOD_ErrorString` line is harmless: that export is
only used to turn an error code into log text, and the bridge falls back to `"unknown"`.

The package's `fmod.dll` is a patched copy of the retail `fmod_X86.dll`, not the cracked
package's `fmod.dll`. Both are 1,338,880 bytes, but the retail one imports
`VCRUNTIME140_APP.dll`; the patcher rewrites that import before copying.

## 9. Data root

The host creates:

```
%APPDATA%\MinecraftPE\
  games\
    com.mojang\
      minecraftpe\
        options.txt
```

`options.txt` is seeded with the standard 1.16 defaults (`mp_username:Steve`,
`game_difficulty_new:1`, `game_language:en_US`, `gfx_renderdistance_new:96`, etc.). If the file
already exists and is non-empty, it is left alone; an existing empty file is regenerated. The
game's own saves, resource packs and skins live under the same `com.mojang` tree.

`WIN32CRAFT_DEBUG=1` enables `win32craft.log` next to the executable. Without it the host is
silent. The log is append-only and is the first place to look for a profile mismatch, a failed
hook signature, a missing DLL or the FMOD result.

## 10. Package mode and the `STATUS_DLL_NOT_FOUND` trap

Build a package from an extracted folder or directly from the `.appx`:

```
python tools\1.16.40.02-patcher.py SOURCE OUTPUT --package --host path\to\win32craft.dll
```

`SOURCE` may be the extracted game folder or an `.appx`; `OUTPUT` must not be the source and
must not be inside it. `--force` replaces an existing output. `--exe-name` changes the patched
executable name; the default is `Win32Craft.1.16.40.02.exe`. The packager:

1. copies or extracts the game;
2. removes `AppxSignature.p7x`, every other `*.p7x`, `AppxMetadata\` and the IDA `*.i64`
   databases (package identity files and analysis databases are meaningless in an unpackaged
   folder);
3. patches `Minecraft.Windows.exe` to `Win32Craft.1.16.40.02.exe` and deletes the stock exe;
4. rewrites the `*_APP` CRT imports in `ChakraCore.dll`, `cohtml.UWP.dll`,
   `HttpServer.UWP.dll`, `MediaDecoders.UWP.dll`, `RenoirCore.UWP.dll` and `fmod_X86.dll`;
5. copies the patched `fmod_X86.dll` to `fmod.dll`;
6. copies the host DLL to `win32craft.dll`;
7. writes `.Win32Craft` with the exact ASCII text `Minecraft Bedrock Edition` (no trailing
   newline), so repack tools can identify the folder without guessing from the exe name;
8. writes `PATCH_NOTES.md` with the input/output hashes and sizes.

The CRT step is the one that cost the most time. The retail game DLLs import
`vccorlib140_app.DLL`, `VCRUNTIME140_APP.dll` and `MSVCP140_APP.dll`. Those names are only
resolvable inside the package graph; an unpackaged process fails in the loader with
`STATUS_DLL_NOT_FOUND` (`0xC0000135`, process exit `-1073741515`) before a single line of host
code runs. Windows then shows the missing-DLL dialog, and because each DLL has its own missing
import, the dialog can repeat cyclically. The host log stays empty, which makes it look like
the exe patch failed.

The cracked run folder that was used during development already had desktop CRT names in its
DLLs, so copying the retail `!TMP` folder initially reproduced the failure even though the exe
was patched correctly. `patch_crt_imports` rewrites the import descriptor name strings in
place (same length, NUL padded). This is also why `fmod_X86.dll` must be patched before it is
copied to `fmod.dll`: the retail file itself imports `VCRUNTIME140_APP.dll`, and the loader
would fail when the game first loads FMOD.

`AppxManifest.xml`, `AppxBlockMap.xml` and `[Content_Types].xml` remain in the folder but are
inert for the unpackaged process.

## 11. Build and run

The repository baseline is `build_windows_mingw.bat` (32-bit MinGW-w64 GCC + `lld-link`). The
verified test20 host was built with the LLVM-MinGW clang driver and `ld.lld`, with the extra
`ctorstub.obj` / `interlocked.obj` and the compiler runtime libraries:

```
C:\llvm-mingw\bin\i686-w64-mingw32-clang.exe -c -O2 -fno-stack-protector -fno-builtin ^
  -Wno-incompatible-pointer-types -Wno-attributes win32_host.c ^
  -o C:\Users\Max\AppData\Local\Temp\w32c-build\win32_host_test20.obj

ld.lld -flavor link /dll /machine:x86 /entry:DllMain@12 /safeseh:no ^
  /def:...\win32craft.def ...\dllcrt2.o ...\crtbegin.o ^
  win32_host_test20.obj win32_host_early2016.obj api_compat.obj ^
  ctorstub.obj interlocked.obj ^
  libmingw32.a libclang_rt.builtins-i386.a libunwind.a libmoldname.a ^
  libmingwex.a libmsvcrt.a libadvapi32.a libcomdlg32.a libshell32.a ^
  libuser32.a libkernel32.a ...\crtend.o

python C:\Users\Max\AppData\Local\Temp\w32c-build\fixfwd.py raw.dll win32craft.dll
```

`fixfwd.py` is mandatory after every link with this toolchain: `lld-link` spells x86 PE
forwarder module names as `_KERNEL32.`; the script shifts the string left and NUL-terminates it.
The test20 build had 61 forwarders. Without the fix the host's API-set exports are unresolved.

The exact object paths and runtime libraries live in
`C:\Users\Max\AppData\Local\Temp\w32c-build\`; that directory is not part of the repository.
The source changes are in `win32_host.c`, `win32_host_116.inl` and
`tools\1.16.40.02-patcher.py`.

Run:

```
set WIN32CRAFT_DEBUG=1
Win32Craft.1.16.40.02.exe
```

The window title should be `Win32Craft 1.16.40.02`. The first log lines should include:

```
Win32Craft 1.16.40.02 bootstrap entered
Minecraft 1.16 development label enabled
1.16 ClientInstance scene stack original=...
Minecraft 1.16 desktop FMOD: 48 delay imports resolved
1.16 HWND ready; entering original application startup
```

## 12. Verification checklist

1. `Get-FileHash` on the package:
   * `Win32Craft.1.16.40.02.exe` = `5A28A168C8BDBE2766D9D0EDAE0350C0C1AEE0CCDFC48D5E33D438742B19DD41`
     (47,334,400 bytes, `SizeOfImage 0x02DAF000`); the stock `Minecraft.Windows.exe` is absent;
   * `win32craft.dll` = `FEC34FBC8AAD975E6B206F7AB2A69B38E0B04851F35A17ADFC29136F75FE4267`
     (246,784 bytes);
   * `fmod.dll` = `15CF102EFEC2FAF846C13201DD707C23A05BB58869E6DA71D03D3D3B869E4174`
     (1,338,880 bytes);
   * `.Win32Craft` = `Minecraft Bedrock Edition` (no trailing newline);
   * no `*.p7x` or `*.i64` files remain.
2. Copy the package to a clean directory (do not run it from the build output) and start
   `Win32Craft.1.16.40.02.exe` with `WIN32CRAFT_DEBUG=1`.
3. Confirm the window title is `Win32Craft 1.16.40.02`, not just `1.16.40.02`.
4. Confirm the log has the bootstrap line and the 48-import FMOD line.
5. Open the main menu, load a world, resize the window through the verified sequence, and
   import a PNG skin.
6. Hold the attack button in survival and creative: block breaking must keep progressing
   (`GetAsyncKeyState` polling supplies the held state).
7. Exit to the main menu, load a world again, then quit normally: confirm no crash and no
   Store rating prompt.
8. Sound is expected to be absent on this machine with the current UWP FMOD build; check the
   `init result=28` line before treating it as a new regression.

## 13. Traps that cost time

1. **`*_APP` CRT imports fail before `Win32Bootstrap`.** The retail DLLs are package-graph
   binaries. An unpackaged loader cannot resolve `vccorlib140_app.DLL`,
   `VCRUNTIME140_APP.dll` or `MSVCP140_APP.dll`; the process exits with `0xC0000135` and the
   host log is empty. Patch the import descriptor names in every game DLL, not only the exe.
2. **Patch `fmod_X86.dll` before copying it to `fmod.dll`.** The retail FMOD DLL itself has a
   `VCRUNTIME140_APP.dll` import. Copying first and patching only the copy is fine only if the
   copy is patched; copying the retail file unpatched produces a loader failure when FMOD is
   first used.
3. **`ALLOW_TEARING` on a DISCARD chain is invalid.** The game's own resize fails with
   `DXGI_ERROR_INVALID_CALL`. Strip `0x800` in the `ResizeBuffers` hook and resize again from
   `GetBuffer(0)` after the game releases its views.
4. **Do not present twice per frame.** `Present` and `Present1` must both be hooked and counted;
   a second present on a DISCARD chain shows the discarded buffer and looks like heavy flicker.
5. **`PointerVisualizationSettings` is not optional in the UWP projection but is useless on the
   desktop.** Skip the block, but keep the temporary delegate alive because the function's
   common epilogue releases it unconditionally.
6. **An empty `std::function` callback aborts through `_invoke_watson`.** The optional startup
   and loading-state notifications can be empty in an unpackaged host. Return 0 from the
   wrapper or skip the invoke; do not call the empty function.
7. **`Enum::ToString` can abort.** The activation/window-state diagnostics stringify boxed
   WinRT enums. Replace the call with a null HSTRING and keep the surrounding COM releases.
8. **Delay-load helper imports are not available on Windows 7.** `ResolveDelayLoadedAPI`,
   `ResolveDelayLoadsFromDll` and `DelayLoadFailureHook` are newer kernel32 exports. Point
   their ILT entries at `GetLastError` and add descriptors that reuse the existing IAT slots.
9. **The FMOD descriptor is version-specific.** Do not reuse the 1.16.20.03 RVAs. Select the
   descriptor from the profile and validate its shape, name and 48 entries before resolving.
10. **Two retail revisions exist.** They differ by one byte and patch identically. Accept both
    hashes in the patcher; do not make the host depend on one full-file hash.
11. **The window title belongs to the host.** Patching the game's title string is fragile; the
    host already knows the selected version and builds `Win32Craft ` + version.
12. **`SizeOfImage` is the profile key.** Adding `.d3imp` changes it from `0x02DAE000` to
    `0x02DAF000`. Running the host against the stock exe silently selects the 1.16.20.03
    profile and fails the signature checks.
13. **The skin filter is a projected vector.** Store the extensions and implement
    `RemoveAtEnd`/`Clear`; otherwise the dialog has no filter and the default extension is
    missing.
14. **Do not use the cracked run folder as the package source.** Its DLLs already have desktop
    CRT names and hide the `*_APP` problem. Use the retail `!TMP` folder or the original
    `.appx`.

## 14. Open items

* Audio output. The next useful experiment is a desktop FMOD build with the same 1.09.04 ABI
  (or a package-identity shim for the UWP build), then re-check `System::init`. The current
  failure is `FMOD_ERR_OUTPUT_INIT` / `FMOD_ERR_OUTPUT_CREATEBUFFER`, not a missing import.
* ARM32 1.16.40.02 is a separate port under `arm32/v1_16_40/`; this document is x86-only.
* The 1.16.20.03 x86 package uses the same conventions (`Win32Craft.1.16.20.03.exe`,
  `.Win32Craft`, no `*.p7x`/`*.i64`) and the same shared host; see the package's
  `PATCH_NOTES.md` and `tools\1.16.20.03-patcher.py`.
