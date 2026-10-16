# fmod: fmod_ARM.dll on desktop ARM32 (Minecraft 1.2.8 ARM32)

Machine-readable: `fmod.json` (21 facts: 16 high / 5 medium, `null_impl_table` = 39 functions, `options`, `port_instructions`).
DLL: `Minecraft_1.2.8.0_ARM32/fmod_ARM.dll`, 1118720 bytes, SHA-256
`5f92f5ac9317c7a5d61ab6b43a2e799ae90eb7eee5c992c85d0325ba1d09fc38`, FMOD low-level **1.09.04** (0x00010904 - exactly the
minimum the game demands), ImageBase 0x10000000, RVAs below are DLL RVAs unless prefixed "game".

## Recommendation (one line)
**Primary = option D**: patched `fmod_ARM.dll` (VCRUNTIME rename + clear APPCONTAINER + 6-byte patch that skips the
Package/ApplicationData step) - no host hook needed. **Fallback 1 = B'** (runtime IAT hook of the DLL's
`RoGetActivationFactory` cell 0xB118C onto the existing host fakes; needs combase HSTRING forwarding). **Fallback 2 = C**
(silent 39-function FMOD in win32craft.dll / stub fmod_ARM.dll; also the only option without the INFINITE-wait hang risk).

## 1. Anatomy and desktop status
27 import descriptors / 129 functions (directory file 0xF8D30; full table with every name-string file offset in JSON
`fmod.dll_import_descriptors.field_table`). Verified locally (Win11 apiset schema + export tables as a names-only proxy):
everything resolves **except VCRUNTIME140_APP.dll** (not in System32; exe would die at process start with 0xC0000135).
WinRT/MF/MMDevAPI imports: api-ms-win-core-winrt-l1-1-0 (RoGetActivationFactory, RoUninitialize, RoInitialize),
winrt-string-l1-1-0 (CompareStringOrdinal, CreateStringReference, GetStringRawBuffer), MMDevAPI ordinal 17 =
ActivateAudioInterfaceAsync (confirmed), MFPlat x9, MFReadWrite x1, `LoadPackagedLibrary` (only the plugin loader sub_79A20;
unused by the game). Windows RT 8.1 is NOT verifiable here; RISK list (knowledge-based): processthreads-l1-1-2,
file-l1-2-1, file-l2-1-1, libraryloader-l2-1-0, sysinfo-l1-2-1, plus UCRT (`api-ms-win-crt-*`) and VCRUNTIME. In-place
contingency: retarget those api-set names to `KERNEL32.dll`.

File offsets for a patcher (file offset = RVA - 0x1000 + 0x400 in .text):

| what | file offset | old | new |
|---|---|---|---|
| VCRUNTIME140_APP.dll name (21 B) | 0xF989A | `56435255 4E54494D 45313430 5F415050 2E646C6C 00` | `56435255 4E54494D 45313430 2E646C6C 00 00000000` |
| DllCharacteristics (u16) | 0x17E | `40 11` (0x1140) | `40 01` (0x0140, APPCONTAINER cleared) |
| sub_79300 skip Package/AppData | 0x78A50 (RVA 0x79650) | `BB65 4DF21871` | `0024 00F085B9` (`movs r4,#0; b.w 0x79960`) |
| (option B only) winrt descriptor name | 0xF929C (34 B field) | `api-ms-win-core-winrt-l1-1-0.dll` | `win32craft.dll` + NUL padding |

Other DLL-name strings (all patchable in place, only same/shorter): WS2_32 0xF91FA, winrt-string 0xF92BE,
MMDevAPI 0xF92E6, MFPlat 0xF93A2, MFReadWrite 0xF93D4, com 0xF963E, file-l1-2-1 0xF965E, handle 0xF967E,
errorhandling 0xF96A0, profile 0xF96CA, synch 0xF96EE, processthreads 0xF9710, libraryloader-l1-2-0 0xF973A,
libraryloader-l2-1-0 0xF9764, file-l2-1-1 0xF978E, string 0xF97AE, crt-* 0xF9AC8..0xF9B8E, sysinfo 0xF9C92, interlocked 0xF9CB6.
Checksum field is 0, no signature: nothing else to fix.

## 2. Init path and every WinRT class
`System::init` (export 0x6D8D8) -> `sub_945C4` -> `sub_89024` (refcounted OS init) -> **`sub_79300`**. Any nonzero result
returns immediately; WinRT failures give `0x1C = FMOD_ERR_INTERNAL`. `FMOD_System_Create` never touches WinRT.

`sub_79300` order (slots verified against the Windows SDK headers):

| step | class / interface (IID @ RVA) | calls | consumed value / failure |
|---|---|---|---|
| 1 | `Windows.Media.Devices.MediaDevice` / IMediaDeviceStatics {AA2D9A40-...} @0xDD7BC | slot 13 `add_DefaultAudioRenderDeviceChanged(handler,&token)` | token kept at .data 0x13F794+0x24; fail -> 0x1C |
| 2 | `Windows.Devices.Enumeration.DeviceInformation` / IDeviceInformationStatics {C17F100E-...} @0xDD870 | slot 13 `CreateWatcher(2=AudioRender)` and `(1=AudioCapture)`; watcher slots 6/8/10/12 add_Added/Updated/Removed/EnumerationCompleted, slot 17 Start, then **WaitForSingleObjectEx(event, INFINITE)** (handler Invoke = sub_7A018 -> SetEvent) | watchers kept at +0x18/+0x1C; fail -> 0x1C; **hang if never completed** |
| 3 | `Windows.ApplicationModel.Package` / IPackageStatics {4E534BDF-...} @0xDD704 | slot 6 get_Current; IPackage slot 7 get_InstalledLocation; QI IStorageItem {4207A996-...} @0xDD6F4; slot 12 get_Path | wcsdup -> +0 (InstalledLocation); fail -> 0x1C |
| 4 | `Windows.Storage.ApplicationData` / IApplicationDataStatics {5612147B-...} @0xDD860 | slot 6 get_Current; IApplicationData slot 14 TemporaryFolder, 12 LocalFolder, 13 RoamingFolder, each QI IStorageItem + get_Path | wcsdup -> +4 temp, +8 local, +0xC roaming; fail -> 0x1C |

* The four strings are read **only** by `sub_790BC` (FMOD_OS_File_Open) to expand the prefixes `ms-appx://`,
  `ms-appdata:///temp|local|roaming`. The game installs its own `setFileSystem` callbacks, so they are not needed for output.
* Later (device list / output start) FMOD uses the REAL MediaDevice + DeviceInformation again (`sub_7B364` render list,
  `sub_7BAA0`: MFStartup, MFLockSharedWorkQueue("Audio"), `ActivateAudioInterfaceAsync(IAudioClient2 {726778CD-...})`).
  Needed for basic stereo output; none of it needs package identity. Not executed here - unverified on device.
* RoInitialize(1=MTA)/RoUninitialize are only used by FMOD's thread trampoline `sub_79D24`.
* **New finding vs pe_imports**: steps 1-2 (two blocking device-enumeration waits) run BEFORE the failing step 3, so even
  option A can hang rather than fail if DeviceWatcher EnumerationCompleted never fires.

## 3. Game side (ARM exe)
* `SoundEngine::ctor` (0x53CDA4) always builds `SoundSystemFMOD` (vtable 0x136AF94). There is **no** AudioGraph-vs-FMOD
  selection: RTTI has only SoundSystemBase/SoundSystemFMOD. AudioGraph/AudioGraphSettings (IAudioGraphSettingsFactory,
  category 9 = Speech) belong to the AppPlatform_Winrt text-to-speech adapter (sub_59871C); its continuation (sub_5988CC)
  takes a benign branch when `CreateAudioGraphResult.Status != 0`, same semantics as the x86 host fake (DeviceNotAvailable=1).
* `vf1` 0x5A393C: `FMOD_System_Create(&this+0x74)` + `getVersion >= 0x00010904`, returns bool; false -> engine skips init.
  `vf2` 0x5A398C: `System::init(0x20, 4, NULL)`; on error calls the no-op logger sub_5A31BC (formats then discards) and returns.
  Success tail: setFileSystem (4 callbacks), set3DSettings, 2x createChannelGroup, getMasterChannelGroup, 2x addGroup, `+0x70=1`.
* **Init failure = graceful silence**, never crash/assert: loadSound/play/update/volume paths all guard on `+0x74` (System*)
  and `+0x70` (flag). Only quirk: vf5 (suspend/resume) rewrites `+0x70`, harmless (real FMOD returns UNINITIALIZED errors).

## 4. Options
| | what | result | effort | risk |
|---|---|---|---|---|
| A | rename + flags only | init -> 0x1C, silent game | 0.5 h | hang risk (steps 1-2) |
| B | + descriptor #1 -> win32craft.dll exports | audio if fakes+HSTRING right | 3-4 h | medium |
| **D** | A + skip steps 3-4 (6 bytes) | **audio**, path globals NULL (unused) | 1 h | low-medium |
| B' | A + host IAT hook of cell 0xB118C | audio + real paths | 2-3 h | low-medium |
| C | silent FMOD (39 fns) | no audio, no hang, no fmod load dep | 3-5 h | low |

B/B' details: host fakes already match FMOD's slots (factory slot 6 -> package_object/appdata_object; package slot 7;
default QI returns self for IStorageItem; folder slot 12 `fake_folder_path`; appdata slots 12/13/14 all `fake_appdata_local_folder`).
Hard requirement: `fake_folder_path` hands an HSTRING to native FMOD, so `api_compat.c native_hstring_proc` must forward to
combase on the ARM image size 0x01731000 (PORT_RULES #8). FMOD never calls WindowsDeleteString.

C: `null_impl_table` in the JSON lists all 39 mangled exports, AAPCS signatures and required out-values. Key points:
NULL-tolerant outputs (getSoftwareFormat, getDSPClock, getFormat, addGroup), `getFormat` type must not be 9 (MPEG) or 0x12
(AUDIOQUEUE) or the game releases the sound (sub_5A3210), `getNumDrivers`=0, `getVersion`>=0x10904, isPlaying -> 0 (1-byte bool),
64-bit args in r2:r3 (skip r1), stack args for createSound/playSound/setFileSystem/set3DListenerAttributes/getDriverInfo.
Exe wiring for C: descriptor #1 at file 0x1325DBC, Name field at 0x1325DC8 (0x013283EA -> "fmod_ARM.dll" at 0x13277EA, 13-byte
slot, cannot hold "win32craft.dll").

## Open questions
See JSON `open_questions` (DeviceWatcher completion on desktop/RT, APPCONTAINER DLL load, RT 8.1 contracts, audible output
unverified, delegate bodies not decoded, ms-appx names reaching FMOD's file layer).
