# Minecraft 1.14.30.2 x86 - full porting record

This is the second 1.14 x86 Bedrock port (after 1.14.60.5) and the fourth x86 Bedrock port overall.
It reuses the shared `bootstrap_116` Win32 host and the proven 1.14.60.5 patching strategy. What is
version-specific is the PE layout, the import table, every RVA and the resulting patched image size
`0x02857000`. Do not copy addresses from 1.14.60.5: the binaries are different.

## 1. Where the pieces live

| Piece | Path |
|---|---|
| Host sources (one translation unit) | `win32_host.c` + `win32_host_116.inl` |
| x86 build script | `%TEMP%\w32c-114-build\build.ps1` (LLVM-MinGW clang + UCRT) |
| Mandatory forwarder normalization | `%TEMP%\w32c-114-build\fixfwd.py` |
| Exe patcher / packager | `tools/1.14.3002.0-patcher.py` |
| Host exports | `win32craft.def` |
| Retail source folder (outside git) | `<MCLauncher>\imported_versions\Microsoft.MinecraftUWP_1.14.3002.0_x86__8wekyb3d8bbwe` |
| Finished test package (outside git) | `<MCLauncher>\imported_versions\Minecraft_1.14.30.2_x86-Win32Craft` |
| Host log | `win32craft.log` next to the exe, only with `WIN32CRAFT_DEBUG=1` |

`win32_host_116.inl` is shared with 1.16.20.03, 1.16.40.02 and 1.14.60.5. The runtime picks the
profile from the patched image size; every address below is only valid for 1.14.30.2.

The port reuses the complete x86 WinRT host: package/application-data redirection, `CoreWindow`,
`CoreApplicationView`, the D3D11 HWND swap chain, the vccorlib compatibility shims, the stdio hooks
and the FMOD delay-import resolver. Only the profile and the exe patch sites are new.

## 2. Why 1.14.30.2 needed its own profile and patcher

| Property | Value |
|---|---|
| Input file | `Minecraft.Windows.exe` |
| Input size | 41,799,168 bytes |
| Input SHA-256 | `C306BA96A7FFC774EBD58F065DB0F076FBC2ACA6152A110BCA0B3B24B6ABC9F3` |
| Input `SizeOfImage` | `0x02856000` |
| Patched file | `Win32Craft.1.14.30.2.exe` |
| Patched size | 41,800,704 bytes |
| Patched SHA-256 | `1C342046485976F1AB33A481515F36E42B20B1930C7577383FD460D4B7CCE33B` |
| Patched `SizeOfImage` | `0x02857000` |

The 1.14.60.5 profile (`game_profile_114`) is selected by `SizeOfImage == 0x028B8000`. Feeding it a
1.14.30.2 image makes every signature check fail; the host logs the mismatch and the game does not
run. The patched 1.14.30.2 image is exactly one section-alignment page larger than the stock image,
so `0x02857000` is a stable discriminator.

`win32_host.c` routes that size into the shared bootstrap:

```c
if (image_size == 0x02db1000 || image_size == 0x02daf000 ||
    image_size == 0x028b8000 || image_size == 0x02857000)
    return bootstrap_116(image, instance);
```

and `bootstrap_116` selects the new profile:

```c
game_116 = image_size == 0x02daf000 ? &game_profile_116_04002
         : image_size == 0x028b8000 ? &game_profile_114
         : image_size == 0x02857000 ? &game_profile_114_3002
                                    : &game_profile_116_02003;
```

The patcher is a separate file because the RVAs, the import-table layout and the appended-section
layout all differ from 1.14.60.5. The code structure is intentionally the same, so a fix proven on
one 1.14 build can be carried to the other by re-locating the site.

## 3. What the patcher changes in the exe

### 3.1 PE header

| Field | Original | Patched | Why |
|---|---|---|---|
| `IMAGE_FILE_LARGE_ADDRESS_AWARE` | `0x0122` | `0x0122` | verified, not changed |
| OS version minor | `2` | `2` | verified, not changed |
| Subsystem version minor | `2` | `2` | verified, not changed |
| DLL characteristics | `0x9140` | `0x8100` | clears `APPCONTAINER` (`0x1000`) and `DYNAMIC_BASE` (`0x0040`) |

The package is an unpackaged Win32 process, so the loader must not treat the image as an
app-container image. `DYNAMIC_BASE` is cleared on purpose: the `WinMain` patch uses an absolute VA
(`0x400000 + .d3imp RVA + offset`) for the `Win32Bootstrap` IAT entry, which is only valid if the
image loads at its preferred base `0x400000`.

### 3.2 Code patch sites

All values are RVAs. In `.text` the file offset is RVA minus `0xC00` on this build. The patcher
prints the corresponding file offset (`file+0x...`) for every site.

| RVA | Original bytes | Patched bytes | Purpose |
|---|---|---|---|
| `0x197353F` | `50 57 68 00 00 40 00 e8 2d 10 00 00` | `ff 15 <IAT> 90×6` | `WinMain` bootstrap call |
| `0xE956A5` | `8b 4e 04 8b 01 8b 40 04 ff d0` | `b0 01 90×8` | world-entry Chakra predicate skip |
| `0x94D956` | `8b 02 8b ca 6a 01 ff 10` | `e8 <stub> 90×3` | unlink old packet listener |
| `0x94D9F6` | `8b ca 8b 02 6a 01 ff 10` | `e8 <stub> 90×3` | unlink old telemetry listener |
| `0x94DA80` | `8b 02 8b ca 6a 01 ff 10` | `e8 <stub> 90×3` | unlink old level listener |
| `0x94DAF2` | `8b 01 6a 01 ff 10` | `e8 <stub> 90` | unlink old hit-detect listener |
| `0xF462F0` | `8b 02 8b ca eb 06 85 c9 74 06 8b 01 6a 01 ff 10` | `8b 4f 4c 8d 82 2c 02 00 00 50 e8 <rel32> 90` | unlink old server listener on re-init |
| `0xF4756A` | `74 06` | `eb 06` | retain server engine on teardown |
| `0xF7B63` | `75 6b` | `eb 6b` | suppress rating prompt |

The `WinMain` patch replaces `push eax; push edi; push 0x400000; call ...` with a call through the
absolute `Win32Bootstrap` IAT slot in `.d3imp`. The host receives the image base in `EAX` and the
`HINSTANCE` in `EDI`, the same convention the original call used.

The world-entry site is the first virtual call of the loader's Chakra script-interface predicate.
The stock code calls it before the interface is ready; the call faults and the loading screen never
leaves. The patch forces the predicate result to `1` (`mov al,1`) and skips the broken call. This is
an exe-side workaround, not a host-side fake.

The four client listener sites replace the deleting-destructor call for the four per-world
`Script*EventListener` objects. The engine stores them at:

| Engine field | Object | Manager vector |
|---|---|---|
| `+0x230` | `ScriptClientPacketEventListener` | `[[ClientInstance]+0x1b8]` |
| `+0x234` | `ScriptTelemetryEventListener` | `[engine+0xb0]` |
| `+0x23c` | `ScriptClientLevelEventListener` | manager returned by `vtbl+0x70` |
| `+0x240` | `ScriptHitDetectEventListener` | `[[ClientInstance]+0x62c]` |

The original code deletes the previous object without unregistering it from the manager vector.
After a world exit and re-entry the vector holds `[stale, current]`; the next dispatch dereferences
the recycled stale block as a vtable (observed as an AV in the packet-dispatcher thunk).

Each patch site calls a small stub in the appended section. The stub loads the old listener pointer
into `EDX` and the manager vector into `ESI`, then calls one shared helper. The helper:

1. saves all registers with `pushad`;
2. scans `[begin, end)` for the old pointer;
3. shifts every later entry left by one pointer and decrements `end`;
4. calls the original deleting destructor through vtable slot 0 with `this = EDX` and `delete = 1`;
5. returns with `popad; ret`.

The stubs preserve the original calling convention and restore the register state the surrounding
code expects (the packet and telemetry stubs put `ESI` back to `engine+0x230` / `engine+0x234`
before returning).

The server side has the same bug. The `ServerInstance` owns a `ServerInstanceEventListener`
subobject at `engine+0x22c` and registers it with the listener manager at `[ServerInstance+0x4c]`.
Server re-initialization creates a new engine, stores it in `ServerInstance+0xfc`, deletes the old
engine and then registers the new listener. The old engine's listener is never unregistered, so the
dispatch vector keeps a pointer to freed memory.

The patch at `0xF462F0` replaces the old-engine deletion with:

```asm
mov  ecx, [edi+0x4c]        ; listener manager
lea  eax, [edx+0x22c]       ; old engine's listener subobject
push eax
call 0x5481F0               ; removeListener, same helper as the teardown path
nop
```

At this point `EDI` is the `ServerInstance` and `EDX` is the old engine; the `je` immediately before
the site still skips the sequence when no old engine exists. The engine allocation itself is
deliberately retained so a dispatch that is already in flight cannot run through freed memory. This
is a small per-world leak, matching the 1.16 listener workaround.

The teardown patch at `0xF4756A` changes `je +6` to `jmp +6`, so the engine allocation is always
retained there too. This is a second safety net for events queued before the listener was
unregistered.

The rating prompt is gated by the persisted `shown_ratings_prompt` option and a launch counter. When
it fires it reaches the UWP Store projection, which is unavailable in the unpackaged host, and
crashes after the save-on-exit path. The branch at `0xF7B63` returns false when the option is
already set; the patch forces that "already shown" path so the prompt is never built.

### 3.3 String and import surgery

The exe contains package-only CRT names and a package-only FMOD name. They are rewritten in place:

| Original | Replacement | Count |
|---|---|---|
| `vccorlib140_app.DLL` | `vccorlib140.DLL` | 1 |
| `MSVCP140_APP.dll` | `MSVCP140.dll` | 1 |
| `CONCRT140_APP.dll` | `CONCRT140.dll` | 1 |
| `VCRUNTIME140_APP.dll` | `VCRUNTIME140.dll` | 1 |
| `fmod_X86.dll` | `fmod.dll` | 2 |

The stock import table has 31 descriptors and `SizeOfImportDirectory = 0x280`. The patcher verifies
both values before touching anything, then:

* renames `dxgi.dll`, `d3d11.dll` and the WinRT API sets to `win32craft.dll`;
* renames `chakra.dll` to `ChakraCore.dll`;
* rewrites the `kernel32.dll` ILT entries for `ResolveDelayLoadedAPI`, `ResolveDelayLoadsFromDll`
  and `DelayLoadFailureHook` to the `GetLastError` hint, so the loader resolves them to
  `GetLastError`; their IAT slots are then re-exported by the host under the original names;
* rewrites the `chakra.dll` ILT entry for `JsStartDebugging` to the `JsAddRef` hint; its IAT slot is
  re-exported by the host as `JsStartDebugging`;
* appends six descriptors: `Win32Bootstrap`, `JsStartDebugging` and the three delay-load helpers.

The result is 36 real descriptors plus the null terminator = 37, `SizeOfImportDirectory = 0x2E4`.
The host must export these names; it does (130 exports, 61 forwarders).

### 3.4 The `.d3imp` section and the bootstrap call

The patcher appends one section at the end of the image:

| Property | Value |
|---|---|
| Name | `.d3imp` |
| RVA | `0x02856000` |
| Virtual size | `0x42D` |
| Raw offset | `0x027DCE00` |
| Raw size | `0x600` |
| Characteristics | `0xE0000040` (code + initialized data, read/write/execute) |

It contains the relocated import descriptors, the `win32craft.dll` name, the hint/name table, the
ILTs, the IAT slots and the listener stub code. The import directory is moved to
`0x02856000` / `0x2E4`, `SizeOfImage` becomes `0x02857000`, the section count becomes 7 and
`SizeOfInitializedData` grows by the raw size (`0x600`).

The `WinMain` patch points at the absolute VA
`0x400000 + 0x02856000 + bootstrap_iat_offset`. The patcher checks the resulting image size and
raises if it is not exactly `0x02857000`, because the host profile is keyed on that value.

### 3.5 Host DLL name

The import descriptors and the bootstrap call use the bare name `win32craft.dll`. The name is
ASCII, has no path and must match the file in the package. Changing it requires rebuilding both the
exe patch and the package.

## 4. Host profile `game_profile_114_3002`

The profile is a full `GameProfile116` re-based to 1.14.30.2:

| Field | Value | Meaning |
|---|---|---|
| `version_ascii` / `version_wide` | `1.14.30.2` | window title and log |
| `client_instance_vtable` | `0x02131A30` | `ClientInstance` vtable |
| `client_destroy` | `0x000A9D50` | scalar deleting destructor |
| `client_scene_stack` | `0x000B4540` | `SceneStack` getter (vtable `+0x338`) |
| `client_top_scene` | `0x000B4490` | `getTopScene` |
| `ui_scene_vtable` | `0x0216CF48` | top `UIScene` vtable |
| `gameplay_flag_menu` | `0x00612410` | `is_showing_menu` |
| `gameplay_flag_game` | `0x00612430` | `should_steal_mouse` |
| `game_string_assign` | `0x0007DEC0` | game string assign |
| `game_string_destroy` | `0x0007E170` | game string destroy |
| `directory_get_contents_slot` | `0x02196C78` | `DirectoryPack` getContents vtable cell |
| `directory_get_contents` | `0x000E49AB0` | `DirectoryPack::getContents` |
| `zip_get_contents_slot` | `0x02197568` | `ZipPack` getContents vtable cell |
| `zip_get_contents` | `0x000E8BB30` | `ZipPack::getContents` |
| `frame_site` | `0x016145BF` | `FrameworkView::Run` frame call site |
| `development_label_site` | `0x005CF8F1` | `$is_publish` initializer |
| `main_entry` | `0x0197466A` | executable's `main` |
| `fmod_delay_descriptor` | `0x020FB140` | FMOD delay-import descriptor |
| `iat_exception_trace` | `0x01BAC6A8` | `_CxxThrowException` IAT |
| `iat_activation_redirect` | `0x01BACCCC` | `GetActivationFactoryByPCWSTR` IAT |
| `iat_vccorlib_ctor` | `0x01BACC40` | `Platform::Object` ctor IAT |
| `iat_vccorlib_allocate2` | `0x01BACC1C` | `Platform::Heap::Allocate(size,align)` IAT |
| `iat_vccorlib_allocate1` | `0x01BACC28` | `Platform::Heap::Allocate(size)` IAT |
| `iat_vccorlib_ibox_array` | `0x01BACC50` | `GetIBoxArrayVtable` IAT |
| `iat_vccorlib_ibox` | `0x01BACCA4` | `GetIBoxVtable` IAT |
| `iat_command_arguments` | `0x01BACC90` | `GetCmdArguments` IAT |
| `iat_d3d_compile` | `0x01BAC0B4` | `D3DCompile` IAT |
| `iat_d3d11_create_device` | `0x01BACBBC` | `D3D11CreateDevice` IAT |
| `iat_fopen` | `0x01BAC974` | `fopen` IAT |
| `iat_wfopen` | `0x01BAC9A0` | `_wfopen` IAT |
| `iat_wfopen_s` | `0x01BAC96C` | `_wfopen_s` IAT |
| `iat_vsscanf` | `0x01BAC954` | `__stdio_common_vsscanf` IAT |
| `iat_invalid_parameter` | `0x01BAC904` | `_invalid_parameter_noinfo` IAT |
| `iat_invalid_parameter_noreturn` | `0x01BAC938` | `_invalid_parameter_noinfo_noreturn` IAT |
| `iat_terminate` | `0x01BAC93C` | `terminate` IAT |
| `iat_std_terminate` | `0x01BAC670` | `__std_terminate` IAT |
| `iat_report_unobserved` | `0x01BAC5C8` | `_ReportUnobservedException` IAT |
| `dxgi_factory4_iid` | `0x01D1EA28` | `IID_IDXGIFactory4` |
| `client_scene_stack_slot` | `0x338` | `ClientInstance` scene-stack vtable slot |
| `client_scene_stack_member` | `0x310` | `ClientInstance` scene-stack member |
| `frame_slot` | `0x18` | `AppMain` frame vtable slot |
| `frame_has_render` | `0` | frame slot has no render flag |
| `development_label_disp` | `0xE0` | `$is_publish` stack displacement |
| `fmod_import_count` | `42` | FMOD delay-import count |

The scene probe uses `client_instance_vtable` and `ui_scene_vtable` as anchors. The frame hook uses
`frame_site`; `main_entry` is used by the shared bootstrap. The IAT addresses are all inside the
`0x01BACxxx` block; if the exe is rebuilt, they move.

## 5. Bootstrap and window

`Win32Bootstrap` reads the PE `SizeOfImage` from the image base and dispatches to `bootstrap_116`.
The bootstrap installs the activation redirects, the vccorlib shims, the stdio hooks, the D3D11
swap chain, the input hooks and the FMOD resolver, then calls the game's `main_entry`.

The window title is built from `version_ascii`; the observed title is `Win32Craft 1.14.30.2`.

With `WIN32CRAFT_DEBUG=1`, the first log lines are:

```
Minecraft 1.14.30.2 development label enabled
Win32Craft 1.14.30.2 bootstrap entered
```

The scene probe logs one line per state change:

```
1.16 top scene: <ptr> vtable=<vt> expected=0256CF48 menu=<0|1> game=<0|1> -> <action>
```

The log is written next to the exe. It is buffered, so a tail on a live process can lag a few lines
behind the actual state; wait a second before concluding that a line is missing.

## 6. Rendering, Present and resize

The 1.14.30.2 port uses the shared 1.16 renderer: a single present per frame, `ALLOW_TEARING`
stripped from `ResizeBuffers`, and the host's HWND swap chain. The version-specific IAT addresses
are in the profile (`D3D11CreateDevice`, `D3DCompile`, `IID_IDXGIFactory4`).

There is no 1.14.30.2-specific rendering patch. If the window is black, check the profile selection
and the `D3D11CreateDevice` hook first; a wrong profile usually fails earlier with a signature
mismatch in the log.

Window resize was not re-measured in this session; it is inherited from the shared host.

## 7. Input, mouse capture and survival

The scene probe treats the base `UIScene` flags as a tri-state:

* `menu=1` -> release the pointer and show the arrow;
* `game=1` -> capture the pointer and use relative mouse;
* `0/0` -> keep the current state.

The world-list overlay uses a different vtable (`0x0256943C` in the test session), so it reports
`neutral` and keeps the menu state. That is expected, not a failure.

The game's explicit `CoreWindow.SetPointerCapture` / `ReleasePointerCapture` calls win over the
probe. `put_PointerCursor(NULL)` is treated as a relative-mouse request; a non-NULL cursor restores
the arrow.

Survival block breaking uses the shared `GetAsyncKeyState` / `GetKeyState` VK 1-6 real mouse-button
state. It was not re-measured for 1.14.30.2 because the test world was Creative; the code path is
identical to 1.14.60.5.

## 8. World re-entry

World entry alone was already working with the Chakra predicate skip. The failure that needed the
listener patches appears only after a world exit and a second entry: the stale listener vector is
dispatched and the process faults in the packet-dispatcher thunk.

The fix has two halves:

1. Client: the four `Script*EventListener` deleting-destructor calls are replaced by stubs that
   unlink the old pointer from the manager vector before deleting it.
2. Server: the old-engine deletion on re-init is replaced by `removeListener`; the engine
   allocation is retained on both re-init and teardown so in-flight events cannot run through freed
   memory.

Verification performed on the finished package:

1. Launch and wait for the menu.
2. `Play` -> select the `1.14.30.2` world -> load.
3. `Escape` -> `Save & Quit`.
4. `Play` -> select the world -> load again.

Result: two world loads from the world list, one `Save & Quit` cycle, no exception, no AV. The
probe logged `game=1` on the second load; on the first load it briefly reported `neutral` during
the loading screen, but the world rendered and the process stayed alive. The log contained no
`unhandled exception` or `fault` lines.

If re-entry crashes, check in this order: the four listener sites, the server re-init patch, the
teardown patch, the executable bit on `.d3imp`, and the relative call displacements.

## 9. FMOD delay imports and the remaining sound failure

The profile selects the 1.14.30.2 descriptor at `0x020FB140` with 42 delay imports. The host
resolves all 42 names against the shipped `fmod_X86.dll` (and the `fmod.dll` copy) and routes the
common calls through the shared diagnostic bridge.

The shipped DLL is the UWP FMOD 1.09.04 build. In 1.14.60.5 the same DLL lets `System_Create`,
`getVersion` and `setOutput(WASAPI)` succeed, but `init` returns 28 (`FMOD_ERR_OUTPUT_INIT`);
DirectSound and WinMM fail with 54 (`FMOD_ERR_OUTPUT_CREATEBUFFER`). Audio was not re-measured for
1.14.30.2, but the build is the same, so expect a silent game.

## 10. Data root

The host redirects the package/application-data APIs to the desktop filesystem. The 1.14 data root
is `%APPDATA%\MinecraftPE\games\com.mojang`; worlds and options live there. The host writes
`win32craft.log` next to the exe only when `WIN32CRAFT_DEBUG=1` is set.

## 11. Package mode and the `STATUS_DLL_NOT_FOUND` trap

Build a ready-to-run folder with:

```
python tools\1.14.3002.0-patcher.py SOURCE OUTPUT --package --host PATH [--force] [--exe-name NAME]
```

`SOURCE` is an extracted game folder or an `.appx`. `--host` is required and must be an x86 PE32
DLL. `--force` deletes an existing `OUTPUT`; never point it at the source.

The packager:

1. copies or extracts the game;
2. removes `AppxSignature.p7x`, `AppxMetadata/`, every `*.i64` and a leftover
   `Minecraft.Windows.exe.orig`;
3. patches the exe to `Win32Craft.1.14.30.2.exe` and deletes the stock `Minecraft.Windows.exe`;
4. redirects the `*_APP` CRT imports in `ChakraCore.dll`, `cohtml.UWP.dll`, `HttpServer.UWP.dll`,
   `MediaDecoders.UWP.dll`, `RenoirCore.UWP.dll` and `fmod_X86.dll`;
5. copies `fmod_X86.dll` to `fmod.dll`;
6. copies the host to `win32craft.dll`;
7. calls `prepare_mcbe_package` (removes any remaining `*.p7x` and writes the
   `.Win32Craft` marker with `Minecraft Bedrock Edition`);
8. writes `PATCH_NOTES.md` with the current input/output/host hashes and sizes.

The trap: the retail game DLLs import the package-only `vccorlib140_app`, `msvcp140_app`,
`concrt140_app` and `vcruntime140_app` names. Those DLLs exist only inside the package graph. An
unpackaged process fails with `STATUS_DLL_NOT_FOUND` before the host can log anything. The packager
must rewrite them to the desktop names; do not skip this step when assembling a folder by hand.

Do not remove `.i64` or `.p7x` files from the source folder. They are removed only from the
finished package.

## 12. Build and run

The verified host was built with LLVM-MinGW clang 19.1.5 and UCRT, using the recipe in
`%TEMP%\w32c-114-build\build.ps1`:

```
C:\llvm-mingw\bin\i686-w64-mingw32-clang.exe -c -O2 -fno-stack-protector -fno-builtin ^
  -Wno-incompatible-pointer-types -Wno-attributes win32_host.c ^
  -o %TEMP%\w32c-114-build\win32_host.obj

... (early2016 + api_compat objects, same flags)

ld.lld -flavor link /dll /machine:x86 /entry:DllMain@12 /safeseh:no ^
  /def:...\win32craft.def ...\dllcrt2.o ...\crtbegin.o ^
  win32_host.obj win32_host_early2016.obj api_compat.obj ... ^
  /out:...\win32craft.dll
```

`fixfwd.py` is mandatory after linking: it normalizes the 61 forwarders so the export surface
matches the validated host.

Current host:

| Property | Value |
|---|---|
| Size | 247,296 bytes |
| SHA-256 | `11CD5977C693414E9E384BABAA8D45E67B1CB3F77639B0064456FE801E316712` |
| Exports | 130 |
| Forwarders | 61 |
| Imports | `api-ms-win-crt-runtime/stdio/private/string/heap`, `COMDLG32`, `SHELL32`, `USER32`, `KERNEL32` |

Copy it to `dist-mingw-windows\win32craft.dll`, then rebuild the package with the command above.
The host hash changes on every relink; `PATCH_NOTES.md` is the authoritative current hash.

Run `Win32Craft.1.14.30.2.exe` from the package. Set `WIN32CRAFT_DEBUG=1` to get the log.

## 13. Verification checklist

Static:

* input SHA-256 `C306...` and output SHA-256 `1C342...`;
* output size 41,800,704 bytes, `SizeOfImage = 0x02857000`;
* import directory `0x02856000`, size `0x2E4` (37 descriptors including the null);
* `.d3imp` RVA `0x02856000`, virtual size `0x42D`, raw offset `0x027DCE00`, raw size `0x600`,
  characteristics `0xE0000040`;
* no stock `Minecraft.Windows.exe`, no `*.p7x`, no `*.i64`, no `AppxMetadata`;
* `.Win32Craft` contains exactly `Minecraft Bedrock Edition`;
* no real `*_APP` DLL imports. A grep for `_APP` also matches the function name `_set_app_type` in
  `api-ms-win-crt-runtime`; that is not a DLL import;
* host: 130 exports, 61 forwarders, same names and forwarders as the validated host, same import
  DLL list.

Runtime:

* launch, window title `Win32Craft 1.14.30.2`;
* log shows `bootstrap entered` and `development label enabled`;
* main menu renders; `Play` opens the world list;
* world loads; probe logs `game=1`;
* `Save & Quit` returns to the world list;
* world loads again; repeat; no exception and no AV;
* process still alive after each step.

## 14. Traps that cost time

1. **The RakNet crash is environmental.** 1.14.30.2 faulted at RVA `0xE16BB9` (`mov eax,[ecx]`,
   `ecx=[edi+0x98]=0`). The code path is: `0xE17BB0` creates a `RakPeer`, calls the locator startup
   helper `0xE1A100` (which calls `RakPeer::Startup`), and on failure deletes the peer and nulls the
   pointer; the caller `0xE16AE0` uses it unguarded. The same failure happened machine-wide: stock
   UWP builds, the Dev Win32 1.16.12.2 client and BDS 1.16.40.2 all failed at
   `RakNetServerLocator::activate` (`Assertion failed: This should always work.`,
   `raknetserverlocator.cpp @ 524`); BDS crashed in `announceServer`. A reboot fixed it. Do not add
   a null-guard patch to the port for this; it is not a 1.14.30.2 bug.
2. **Wrong profile.** An unpatched image (`0x02856000`) falls into the 1.16.20.03 profile and every
   signature check fails. A patched image without the new dispatch also fails. Check the
   `SizeOfImage` branch first.
3. **`DYNAMIC_BASE` must stay cleared.** The bootstrap IAT VA is absolute and only valid at
   `0x400000`. Do not enable ASLR on this image.
4. **`_set_app_type` is a false positive.** It is a function name, not a DLL import. Inspect the DLL
   name, not the imported symbol.
5. **The listener patches are not optional.** Without them the first world entry works, but the
   first exit/re-entry crashes in the packet dispatcher. Leaking the engine alone is not enough;
   the stale pointer must be unlinked.
6. **The rating prompt crash happens after save-on-exit**, not on startup. A clean exit needs the
   `0xF7B63` branch patch.
7. **Never copy a working exe from another version.** Always patch the original
   `Minecraft.Windows.exe`; the addresses are version-specific. Do not modify the source folder.
8. **Relinking changes the host hash.** Regenerate `PATCH_NOTES.md` after every host rebuild.

## 15. Open items

* Audio output: same UWP FMOD 1.09.04 `FMOD_ERR_OUTPUT_INIT` limitation as 1.14.60.5; not fixed.
* Survival block breaking, window resize and the skin picker: inherited from the shared host but
  not re-measured on 1.14.30.2.
* 1.16.40.02 ARM32 remains parked.
* The x64 0.12.0 port remains unavailable/unpublished.
