# pe_imports: ARM32 import table, IAT hooks, patcher redirects (Minecraft 1.2.8, ARM32)

Machine-readable: `pe_imports.json` (41 facts: 37 high / 4 medium), full import table `imports_table.json`
(50 normal descriptors / 716 imports, 2 delay descriptors / 3 imports, with IAT cell RVA, thunk RVA, hint, per-DLL patcher action).
All RVAs are image-relative (ImageBase 0x400000). The exe is `Minecraft.Windows.exe`, SHA-256 `b01d5f12...fcfcf8`.

## Headline
* The ARM exe has **no KERNEL32 import**, **no dxgi.dll**, no LoadLibrary/GetProcAddress: everything comes through api-set DLLs.
  The x86 KERNEL32 compat-placeholder machinery (ResolveDelayLoadedAPI, DelayLoadFailureHook, CreateFile2) is not applicable.
* **Only XINPUTUAP.dll must be redirected** to win32craft.dll on ARM (UWP-only, absent on RT 8.1). The other six x86 API-set redirects
  (com-l1-1-0, winrt-l1-1-0, winrt-string-l1-1-0, winrt-error-l1-1-0/-1-1, eventing-provider-l1-1-0) are present in the ARM exe and are
  *optional parity*: all contracts resolve natively on Win10 (verified with the Win11 apiset schema + real export tables).
  If they are redirected (or if any HSTRING export is reachable) the HSTRING shims **must forward to combase on ARM**, because the
  desktop vccorlib140.dll and fmod_ARM.dll use real HSTRINGs (the fake shim returns "" for a real HSTRING).
* All 63 vccorlib / 258 MSVCP140 / 43 CONCRT140 / 19 VCRUNTIME140 imports exist in the desktop ARM redistributables (onecore\arm,
  14.44.35112): **0 missing**. The four renames of the x86 patcher work unchanged (each name occurs exactly once).
* fmod_ARM.dll is a normal import (not delay): `resolve_fmod_delay_imports_128` is dropped. It loads on desktop only after its
  `VCRUNTIME140_APP.dll` import is renamed; its WinRT init may fail without package identity (open risk).
* No `Win32CraftResolveDelayLoadedAPI`-style stubs are needed: the delay table holds only the three TCUI cells, which the host overwrites
  with no-ops before the lazy helper can run.

## Host-patched IAT cells (derived by NAME)
| x86 const | x86 RVA | ARM import | ARM IAT RVA | ARM thunk |
|---|---|---|---|---|
| RVA_128_CXX_THROW_IAT | 0x11d2690 | VCRUNTIME140_APP!_CxxThrowException | **0x00C374E4** | 0xB4A100 |
| RVA_128_CREATE_FILE2_IAT | 0x11d20d4 | api-ms-win-core-file-l1-2-0!CreateFile2 (direct use at 0x9895C4) | 0x00C3765C | none (hook not needed) |
| RVA_128_ACTIVATION_FACTORY_IAT | 0x11d2b54 | vccorlib!?GetActivationFactoryByPCWSTR@@YAJPAXAAVGuid@Platform@@PAPAX@Z | **0x00C37BF0** | 0xB49100 (wrapper 0xB48F88, 102 callers) |
| RVA_128_PLATFORM_OBJECT_CTOR_IAT | 0x11d2ae4 | vccorlib!??0Object@Platform@@Q$AAA@XZ | 0x00C37B90 | 0xB49028 |
| RVA_128_PLATFORM_ALLOCATE1_IAT | 0x11d2aa8 | vccorlib!?Allocate@Heap@Details@Platform@@SAPAXI@Z | 0x00C37B7C | 0xB48FF8 |
| RVA_128_PLATFORM_ALLOCATE2_IAT | 0x11d2aac | vccorlib!?Allocate@Heap@Details@Platform@@SAPAXII@Z | 0x00C37B80 | 0xB49004 |
| RVA_128_GET_IBOX_ARRAY_VTABLE_IAT | 0x11d2acc | vccorlib!?GetIBoxArrayVtable@Details@Platform@@YAPAXPAX@Z | 0x00C37B88 | 0xB49010 |
| RVA_128_GET_IBOX_VTABLE_IAT | 0x11d2b1c | vccorlib!?GetIBoxVtable@Details@Platform@@YAPAXPAX@Z | 0x00C37BC8 | 0xB49088 |
| RVA_128_D3D_COMPILE_IAT | 0x11d20b4 | D3DCOMPILER_47!D3DCompile (D3DPreprocess 0xC370B0 stays unhooked) | **0x00C370B4** | 0xA3B944 |
| RVA_128_D3D11_CREATE_DEVICE_IAT | 0x11d2a44 | d3d11!D3D11CreateDevice | **0x00C37A50** | 0xA3B968 |
| RVA_128_TCUI_SHOW_PROFILE_IAT | 0x1724400 | tcui-l1-1-0!ShowProfileCardUI (delay, .data) | **0x015121FC** | 0xB4A2DC |
| RVA_128_TCUI_PROCESS_PENDING_IAT | 0x17243fc | tcui-l1-1-0!ProcessPendingGameUI (delay) | **0x01512200** | 0xB4A2F8 |
| RVA_128_TCUI_SHOW_PROFILE_USER_IAT | 0x1724408 | tcui-l1-1-2!ShowProfileCardUIForUser (delay) | **0x01512208** | 0xB4A33C |

TCUI block `slots = image + 0x015121FC`: `[0]` ShowProfileCard, `[1]` ProcessPending (**swapped vs x86**), `[2]` zero terminator (skip), `[3]` ForUser.
No-ops return S_OK / E_NOTIMPL / E_NOTIMPL (wrapper treats only S_OK/E_PENDING as "wait"). Cells hold Thumb pointers; written pointers need bit0 set.
Calls are all AAPCS: remove WINAPI/stdcall from every hook typedef. Hook parameter lists are identical to x86 (see `arm_signature` per fact).

Vccorlib hooks (Object ctor / Allocate x2 / IBox x2) were written for Wine's aborting stubs; the desktop ARM vccorlib140.dll implements all of them
(real bodies), so they are optional on ARM (defensive zeroing only). `install_create_file2_compat` is dropped (native on 8.1/10).
Win7-only patches in Bootstrap (`patch_win7_dxgi_device3_query_to_base`, `patch_win7_discard_views`, 11_1 feature level filter) are inert/unneeded
(`host_is_windows7` never true on ARM). Remaining Bootstrap callees (crash trace, zip overrides, client-state hook, mouse hooks, platform fixes,
D3D/AppMain init) belong to other areas.

## Patcher plan (ARM equivalent of tools/1.2.8-patcher.py)
* Assertions: 50 descriptors, import size 0x3fc (x86: 27 / 0x230). SHA-256 of the ARM exe as above.
* Header: Characteristics already 0x0122 (LAA) -> no change; OS/subsystem 6.2 -> no change; DllCharacteristics 0x9140 -> **0x8140** (clear only APPCONTAINER;
  keep DYNAMIC_BASE and the .reloc table; any new absolute address in code needs a reloc entry or must be PIC).
* Renames (count 1 each): vccorlib140_app.DLL->vccorlib140.DLL, MSVCP140_APP.dll->MSVCP140.dll, CONCRT140_APP.dll->CONCRT140.dll, VCRUNTIME140_APP.dll->VCRUNTIME140.dll.
  No fmod rename (fmod_ARM.dll stays). No KERNEL32 placeholders.
* Redirect (repoint descriptor name RVA to "win32craft.dll" in the new section): XINPUTUAP.dll (required; its 14-byte name slot is too small to overwrite in place);
  optionally the six API sets. Never redirect CRT/kernel api sets, WS2_32, IPHLPAPI, D3DCOMPILER_47, d3d11, delayload, file-l1-2-0.
* New section `.d3imp`: RVA 0x01730000, file offset 0x1680A00, header slot at 0x350; new import dir = (50+1+1)*20 bytes; adds `win32craft.dll!Win32Bootstrap`.
  New SizeOfImage = **0x01731000** (ARM equivalent of IMAGE_SIZE_128_PATCHED 0x01923000; Bootstrap keys the build on it).
* Call site: replace `bl sub_b4a424` at **0xB49ACA** (file 0xB48ECA, bytes `00f0abfc`) in `__scrt_common_main_seh` (details in the winmain area).
* IAT directory (0xC37000/0xBF8) and every cell RVA stay untouched.
* Also patch fmod_ARM.dll: `VCRUNTIME140_APP.dll` -> `VCRUNTIME140.dll` (file offset 0xF989A).

## win32craft.dll exports on ARM (undecorated)
Required: `Win32Bootstrap`, `XInputGetState`. Optional parity (all present in the existing .def, 0 missing): `IIDFromString CoGetContextToken CoCreateGuid
CoCreateFreeThreadedMarshaler StringFromGUID2 CoCreateInstanceFromApp CoTaskMemFree CoTaskMemAlloc EventWriteTransfer EventUnregister EventRegister
EventSetInformation RoFailFastWithErrorContext SetRestrictedErrorInfo RoReportUnhandledError RoInitialize WindowsCreateString WindowsDeleteString
WindowsIsStringEmpty WindowsCompareStringOrdinal WindowsDuplicateString WindowsCreateStringReference WindowsGetStringRawBuffer WindowsConcatString`.
Drop the `_Name@N` decoration; D3D11CreateDevice/dxgi/CreateFile2/ResolveDelay*/KERNEL32 forwarders are not needed. 64-bit REGHANDLE params use r0:r1 pairs automatically.

## Desktop CRT compare (onecore\arm\Microsoft.VC143.CRT, 14.44.35112)
| ARM import DLL | imports | desktop exports | missing |
|---|---|---|---|
| vccorlib140_app.DLL -> vccorlib140.dll | 63 | 256 | none |
| MSVCP140_APP.dll -> msvcp140.dll | 258 | 1493 | none |
| CONCRT140_APP.dll -> concrt140.dll | 43 | 291 | none |
| VCRUNTIME140_APP.dll -> vcruntime140.dll | 19 | 71 | none |

The desktop DLLs themselves import only low-version contracts + api-ms-win-crt-* (UCRT needed on RT 8.1).

## fmod_ARM.dll
Loads on desktop after the VCRUNTIME140_APP rename (all 11 symbols, incl. `__std_type_info_destroy_list`, exist in desktop vcruntime140). Other deps (WS2_32, MFPlat,
MFReadWrite, MMDevAPI, api-ms-win-core-*, crt) exist. **Risk:** output init (fmod 0x79300) activates Windows.ApplicationModel.Package and Windows.Storage.ApplicationData
through the real RoGetActivationFactory; without package identity they fail (0x80073D54) and the function returns FMOD_ERR_INTERNAL. LoadPackagedLibrary is only the plugin loader (unused).

## Delay-load plumbing
Delay dir 0x131F20C (size 0x60): tcui-l1-1-0 (IAT 0x15121FC, 2 cells) and tcui-l1-1-2 (IAT 0x1512208, 1 cell), attributes=1. Cells start at lazy stubs
(sub_b4a2cc/sub_b4a2e8/sub_b4a32c) -> helpers sub_b4a2a4/sub_b4a304 -> `delay:__delayLoadHelper2` sub_b4a3f0 -> ResolveDelayLoadedAPI(0x400000, desc, 0, &DelayLoadFailureHook, cell, 0).
Pre-patched cells never reach it. No Win32Craft delay stubs required.

## Contingency for Windows RT 8.1 (cannot be verified on this machine)
If the loader reports a missing contract, add that DLL to the redirect set. Existing .def covers winrt-error, eventing, com, delayload (2), CreateFile2 and most kernel forwarders; **not covered**:
K32GetProcessMemoryInfo, GlobalMemoryStatusEx, GetLocalTime, GetNativeSystemInfo, MultiByteToWideChar, WideCharToMultiByte, GetDiskFreeSpaceExW, LocalFree, CloseHandle,
QueryPerformanceFrequency, SystemTimeToFileTime, FileTimeToSystemTime, DecodePointer. UCRT (api-ms-win-crt-*, 159 functions) cannot be shimmed.

## Activation classes (activation_class_set)
67 classes go through the single funnel 0xB48F88; 27 are faked by the x86 host, 40 fall through to the real runtime (list in JSON). Wide-string dispatch ports unchanged.

## Open questions
See JSON `open_questions` (fmod WinRT init, RT 8.1 contract availability, allocator zeroing, redirect-vs-parity A/B test, un-faked classes, APPCONTAINER DLL load).
