# platform_services - MCPE 0.10.4.40 (WP8.1, ARM32) platform layer, what the Win32 host must provide

Machine-readable: `platform_services.json` (27 facts: 23 high / 4 medium). All numbers are RVAs of `MCPE.Host.WindowsPhone.exe`
(IDA VA = 0x400000 + RVA; pointers stored in vtables are VA|1, subtract 0x400000 and the Thumb bit to get the RVA used here).
Everything was re-derived with `arm32re.py` (`ARM32_EXE` set to the 0.10.4 exe); friend notes were only used as pointers. Labels added to the toolkit name file for ~50 functions.

## Headline
1. **One hook point for every WinRT static**: the exe has no `RoActivateInstance`/`RoGetActivationFactory` import; all 36 activations go through
   `vccorlib120_app!GetActivationFactoryByPCWSTR` (IAT cell **0x20B5FC**, wrapper `sub_2001e4` -> thunk 0x200308). Args: `(const wchar_t* className, GUID* iid, void** out)`.
2. **Only 22 of the 36 activations are platform services** (the other 14 are XAML/UI/input, other areas). The platform services are tiny: Package, ApplicationData,
   MemoryManager, VibrationDevice, NetworkInformation, CurrentAppSimulator, ThreadPool, AsyncCausalityTracer, Windows.Web.Http (dead-ish). **No StorageFile/StorageFolder async API is used anywhere**: all file I/O is CRT (`fopen`/`_mkdir`/`rename`...) and Win32 (`CreateFile2`, `FindFirstFileExW`... in LevelDB's `WinRTEnv`).
3. **Both game paths come from IStorageItem.Path**: assets = `Package.Current.InstalledLocation.Path` (AppPlatform+0x14, used as `<path>/data/...`), writable root = `ApplicationData.Current.LocalFolder.Path` (client+0x78/+0x90, then `/games/com.mojang/minecraftpe`, `/minecraftWorlds`, `/options.txt`, `/clientId.txt`). The game narrows these by **truncating each UTF-16 unit to a byte** (sub_480d8): ASCII paths only.
4. **ThreadPool is the key the friend missed.** `AppMain::createApp` (0x1d9dd4) starts the client init as a ThreadPool work item (`ThreadPool.RunAsync`, sub_1da4a4 at 0x1d9fa4). That job runs `MinecraftClient::vf7` (0x57b84), which creates the texture manager (+0x118), the font (+0x11C), shaders, options, sound and the first screen. Without a fake ThreadPool that actually invokes the delegate, those fields stay NULL (the friend's crash).
   The render loop is a second work item (`RunWithPriorityAndOptionsAsync`, High/TimeSliced) that loops on `IAsyncInfo.get_Status == Started`.
5. **Audio is XAudio2 2.8 only** (`XAudio2_8.dll!#1` = XAudio2Create, cell 0x20B05C), sounds are PCM tables embedded in the exe, no files, no FMOD. Native on RT 8.1 -> REAL-ON-RT. If XAudio2Create fails the first `play` crashes (no null check) - patch two vtable cells in that case.
6. **Network = RakNet over plain Winsock UDP** (19 ordinal imports of WS2_32). REAL-ON-RT.
7. **Windows.Web.Http is effectively dead code**: `IHttpClient` (job+0xd8) is never assigned anywhere; only reachable from Realms ("MCO") screens. Do not fake it.
8. **Phone-only / package-identity classes must be faked or bypassed**: Package, ApplicationData, MemoryManager, VibrationDevice, CurrentAppSimulator (and ThreadPool/AsyncCausalityTracer/NetworkInformation are safest faked). Three of them can be bypassed by writing a no-op into one vtable cell instead of faking WinRT (vibrate 0x33FB94, buy 0x33FB7C, network 0x33FB6C).
9. **Imports**: everything is native on RT 8.1 except two unproven api-set contracts (`errorhandling-l1-1-2`, `localization-l1-2-1`) and the VC120 `_app` runtimes, which are not part of Windows (see section PE imports; the Store VCLibs.120 ARM appx found on this PC provides all 322 imported symbols, 0 missing).

## Boot-ordered sequence (what must exist when)
| step | code | platform service touched | host duty |
|---|---|---|---|
| 0 | CRT/vccorlib startup (`sub_1ffecc`, `sub_200878`, RoInitialize) | VC120 `_app` DLLs, combase | other areas (loader/winmain) |
| 1 | `AppMain::createApp` 0x1d9dd4: `new(0x88)` + `AppPlatform_Winrt::ctor` 0x494c0 | `Package.Current.InstalledLocation.Path` -> platform+0x14 | fake Package chain (ASCII exe dir) |
| 2 | `sub_1d9cc0` new MinecraftClient(0x168) + ctor 0x54e90 | none | - |
| 3 | createApp 0x1d9e7c..0x1d9f72 | `ApplicationData.Current.LocalFolder.Path` -> client+0x78 and +0x90 | fake ApplicationData (slot 12) + writable ASCII dir |
| 4 | `sub_1d9b68` + `sub_1da4a4` | `ThreadPool.RunAsync(initJob)` | fake: run `handler->vtbl[3](handler, asyncAction)` on a new MTA thread |
| 4a (job) | `MinecraftClient::vf7` 0x57b84: `_mkdir` tree under LocalFolder, `sub_a230c` Options::init (options.txt + MemoryManager tier at 0xa25ec/0xa260c/0xa263a), images via AppPlatform vf1/vf2/vf3, shaders/fonts via vf14 (`fopen`), `sub_efa00` SoundEngine ctor at 0x5833c (XAudio2Create + CreateMasteringVoice) | MemoryManager (via platform vf40), assets (CRT), XAudio2 | fake MemoryManager (or patch cell 0x33FBC0), correct data path, XAudio2_8 native |
| 5 | `AppMain::StartRenderLoop` 0x1da8a8 -> `sub_114078` | `ThreadPool.RunWithPriorityAndOptionsAsync(handler, 1, 1)` | fake with IAsyncAction answering IAsyncInfo (Status=0 until Cancel) |
| 6 | per frame `pumpInput` 0x50eac | `VibrationDevice.Cancel` only after a vibrate | none if vibrate is bypassed |
| later | worlds: LevelDB `WinRTEnv` + `sub_1069d0` directory listing; multiplayer RakPeer; Realms REST job | Win32/Winsock/(Http dead) | native |

## Per-service facts

### Package (assets) - FAKE-NEEDED, before menu
* `sub_4e840` (0x4e840): class `Windows.ApplicationModel.Package` (rdata 0x20FE50), IID `IPackageStatics {4E534BDF-2960-4878-97A4-9624DEB72F2D}`, slot 6 `get_Current` (helper `sub_4e7e4`).
* `sub_4e8e0`: `IPackage` slot 7 (0x1c) `get_InstalledLocation` -> `IStorageFolder`; caller QI's `IStorageItem {4207A996-CA2F-42F7-BDE8-8B10457A7F30}` (IID bytes rdata 0x20FBC8, runs `[obj]->vtbl[0]`) and `sub_4ea38` reads slot 12 (0x30) `get_Path` (real HSTRING dup).
* only caller: AppPlatform_Winrt::ctor (0x49564). Resulting std::string (narrowed, `sub_480d8`) lives at platform+0x14.
* Asset access (all CRT, no WinRT): `readAssetFile` vf14 0x4f780 = `[+0x14] + "/data/" + name` -> base 0x4792c `fopen("rb")/fread`; `getImagePath` vf1 0x4eb90 (`"/data/images/"`); `loadPNG` vf2 0x4f028 (libpng 1.5.13 via `std::ifstream`/`_Fiopen`); `loadTGA` vf3 0x47818 (`sub_125bc4`, `fopen`, error string "can't fopen"); `savePNG` vf12 0x4f800 (`name+".png"`, `sub_51084`, `fopen/fwrite`). Package folder layout confirmed: `data/{fonts,images,lang,materials,shaders}`.

### ApplicationData / storage - FAKE-NEEDED, before menu
* `sub_1dab28`: `Windows.Storage.ApplicationData` (rdata 0x2DE6A0), `IApplicationDataStatics {5612147B-E843-45E3-94D8-06169E3C8E17}` slot 6 `get_Current`.
* `sub_1dad20`: `IApplicationData` slot **12** (offset 0x30) `get_LocalFolder` (verified; same slot index as the old x86 fake). Slots 8 (0x20, `sub_1e6278`) and 10 (0x28, `sub_1e621c`) are used only by three MainPage handlers (`sub_1e3a30`, `sub_1e3c34`, `sub_1e3da0`, calling `MainPage::vf11/vf10`): not on the boot path; return `S_OK` + NULL there.
* The path is stored at client+0x78 and client+0x90. Layout beneath it (strings in rdata): `/games` 0x211EDC, `/com.mojang` 0x211EE4, `/minecraftpe` 0x211EF0 (client+0xb4 = root + that), `/options.txt` 0x227CBC, `/clientId.txt` 0x2DDC68, `/minecraftWorlds` 0x2D25B8.
* File APIs actually used (all REAL-ON-RT): CRT `fopen fread fwrite fseek ftell remove rename _mkdir _access _stat64i32`; Win32 (api-ms-win-core-file-l1-2-1 / l2-1-1) `CreateFile2 ReadFile WriteFile FlushFileBuffers SetFilePointerEx LockFileEx UnlockFileEx GetFileInformationByHandleEx GetFileAttributesExW CreateDirectoryW DeleteFileW RemoveDirectoryW MoveFileExW FindFirstFileExW FindNextFileW FindClose FormatMessageW` (LevelDB `WinRTEnv` vtable 0xDCA228, `sub_1f500`, `sub_20174`, `sub_1069d0`, `sub_106704/106804/106850`, `sub_10676c`). LevelDB converts file names narrow -> HSTRING -> wide with `WindowsCreateString/GetStringRawBuffer` (real combase; do not leave the old fake HSTRING shim in front of them).

### MemoryManager - FAKE-NEEDED, before menu
* `sub_4e69c` slot 6 (0x18) `get_AppMemoryUsage(UINT64*)`, `sub_4e740` slot 7 (0x1c) `get_AppMemoryUsageLimit(UINT64*)`; IID `{5C6C279C-D7CA-4779-9188-4057219CE64C}`, class rdata 0x210098. Only user: `AppPlatform_Winrt::vf40` (0x4eaf8, cell 0x33FBC0) = limit-usage (uint64 in r0:r1).
* Consumers via global `AppPlatform*` (RVA **0xDDD7E0**, set by `sub_4726c`): `sub_a040c` (picks one of three global vectors at 0xDDED14/0xDDED20/0xDDED2C by free memory > 0x1EA00000 / > 0xE600000) called three times by `Options::init` (sub_a230c, invoked at 0x57e40 inside the init job), and `sub_1a673c` (vtable +0xa0). Fake: limit-usage must exceed 0x1EA00000 (e.g. limit 1 GiB, usage = working set).

### VibrationDevice - FAKE-NEEDED (or bypass), gameplay
* `vf29` vibrate(int ms) 0x50ff0 (cell 0x33FB94): `GetDefault` (`sub_4aeb8`, `IVibrationDeviceStatics {332FD2F1-1C69-4C91-949E-4BB67A85BDC7}` slot 6), `Cancel` slot 7, `Vibrate(TimeSpan)` slot 6 of the device (64-bit TimeSpan in r2:r3, r1 skipped), deadline `GetTickCount64()+ms` at platform+0x48; `pumpInput` (0x50eac, every frame) cancels when the deadline passed. Bypass: put the base no-op (image+0xBF61, Thumb bit set) in cell 0x33FB94.

### NetworkInformation - STUB-OK / probably REAL, can wait
* `vf19` isNetworkEnabled(bool wifiOnly) 0x4ef14 (cell 0x33FB6C): `GetInternetConnectionProfile` (`sub_4b108`, `INetworkInformationStatics {5074F851-950D-4165-9C15-365619481EEA}` slot 7), profile slot 7 `GetNetworkConnectivityLevel == 3`, QI `{E2045145-4C9F-400C-9150-7EC7D6E2888A}` (rdata 0x20FBE8, assumed IConnectionProfile2) slot 7 = IsWlan. Result `(level==3) && (!wifiOnly || isWlan)`. Used by UI screens only.

### CurrentAppSimulator (Store) - STUB-OK, can wait
* `vf23` 0x4e350 (cell 0x33FB7C): `LicenseInformation.ProductLicenses.Lookup(L"TODO").IsActive`, if false runs `RequestProductPurchaseAsync` via a PPL task (`sub_4e93c` IID `{F17F9DB1-74CD-4787-9787-19866E9A5559}`, `sub_4b44c` IID `{4E51F0AB-20E7-4412-9B85-59BB78388667}`). Product id "TODO" (unfinished build). User-triggered only; write a no-op into the cell. Nothing else checks a licence (vf11/26/28 are constants).

### ThreadPool / WorkItemHandler - FAKE-NEEDED, before menu
* `RunAsync` (`sub_1da4a4`, `IThreadPoolStatics {B6BF67DD-84BD-44F8-AC1C-93EBCB9DBA91}` slot 6) - one caller, the init job (above). Delegate: CX object, first vptr 0xDB892C; COM `IWorkItemHandler`: `[0]=QI [1]=AddRef [2]=Release [3]=Invoke(IAsyncAction*)`; Invoke (0x11556c) ignores its argument and calls the captured lambda.
* `RunWithPriorityAndOptionsAsync` (`sub_114078`, slot 8, ABI `(this, handler, priority, options, IAsyncAction** [sp])`): render loop (priority 1 High, options 1 TimeSliced), CoreIndependentInputSource worker (MainPage ctor 0x1dd40e) and `HID::Initialize` (0x111e68).
* Render loop body 0x1da160 (never returns by itself): QI the passed IAsyncAction for `IAsyncInfo {00000036-0000-0000-C000-000000000046}` (bytes rdata 0x20FBD8), `get_Status` (slot 7) must be 0 (Started) to continue; `AppMain::StopRenderLoop` 0x1da9d4 calls `Cancel` (slot 9). Minimal fake IAsyncAction: QI(IUnknown/IInspectable/IAsyncInfo/IAsyncAction), `get_Status` 0 -> 2 after Cancel, `Cancel`, `Close`, `get_Id`, `get_ErrorCode`.
* Start the init job after createApp returned (the real pool also starts immediately and concurrently; the old x86 host slept 10 ms). The worker threads need an initialised apartment (MTA) because CX delegates/HSTRINGs/real activation factories are used.

### AsyncCausalityTracer - STUB-OK, can wait
* `IAsyncCausalityTracerStatics {50850B26-267E-451B-A890-AB6A370245EE}` (class rdata 0x20FDE8): five wrappers called by PPL task code (Store/Http/task<void>): TraceOperationCreation `sub_4b7cc` (slot 6), Completion `sub_4b708` (7), Relation `sub_4b898` (8), SynchronousWorkStart `sub_4ba04` (9), SynchronousWorkCompletion `sub_4b95c` (10). Statics are re-activated on every call. Fake: all slots S_OK.

### Windows.Web.Http (Realms/Mojang REST) - STUB-OK, can wait
* `WinRTRestRequestJob` (vtable 0xDB86EC, ctor `sub_10dafc`, send `vf2` 0x10e9d4) uses `HttpRequestMessage` (default-activated, `sub_108698`), `HttpMethod` statics (4 verbs: `sub_10c5e8/10c6e4/10c83c/10c938`, `IHttpMethodStatics {64D171F0-...}`), `Uri` (`sub_1088a0`, `IUriRuntimeClassFactory {44A9796F-...}`), `HttpStringContent` (`sub_108790`, `{46649D5B-...}`), a "Cookie" header, `IHttpClient::SendRequestAsync` (slot 14) on the object at job+0xd8.
* URLs: `https://authserver.mojang.com` (rdata 0x23CAAC) and `https://peoapi.minecraft.net` (0x23CACC), registered by `sub_f91fc` (called from `Minecraft::init-after-platform` 0x1d84b0). Jobs are created by `sub_f9c30`, called from `ManageMCOServerScreen` etc. (Realms UI), never at boot.
* Job+0xd8 (IHttpClient) is zero-initialised by the ctor and never written elsewhere: the WP code itself would null-deref. No update check, no skin download, no device-id/locale HTTP traffic exists.

### Audio (XAudio2) - REAL-ON-RT, before menu (created in the init job)
* Import `XAudio2_8.dll!#1` (ordinal 1 = `XAudio2Create`, verified on this PC's XAudio2_8.dll exports), cell 0x20B05C; callers `sub_1062d8` (SoundSystemRT ctor) and `SoundSystemRT::vf10` (0x106630).
* `SoundSystemRT` vtable 0xDB8028 (base `SoundSystem` 0xDB7FDC): `XAudio2Create(&this[1], 0, 1)`; `IXAudio2` slot 7 `CreateMasteringVoice(&this[2], 2, 44100, 0, NULL, NULL, 6)`; play (`vf9` 0x106688): WAVEFORMATEX PCM from `PCMSound {+0 data, +4 bytes, +8 channels, +0xc bytesPerSample, +0x10 rate, +0x14 frames}`, `CreateSourceVoice` slot 5 (callback `AudioCallbackHandler` vtable 0xDB8008, OnBufferEnd 0x10659c), `SetVolume` voice slot 12, `SubmitSourceBuffer` slot 21 + `Start` slot 19.
* Sound bank = ~60 named PCM objects built by static initialisers from blobs in the exe's .data (e.g. `sub_5108`), registered by `sub_f03e8`; **no sound files in the package**.
* Risk: `XAudio2Create` failure leaves `this+4 == NULL`; `play` then dereferences it. Fallback: write image+0xBF61 (the shared no-op, Thumb bit set) into cells 0xDB804C (vf9) and 0xDB8050 (vf10).

### Networking (RakNet/Winsock) - REAL-ON-RT, gameplay
UDP only. RakPeer vtable 0x2F6F40; `WSAStartup` in ctor `sub_32db4` (0x3311a), `WSACleanup` in `sub_3348c`; socket `sub_3f780`; bind/options `sub_3f834` (SO_RCVBUF 0x40000, SO_SNDBUF 0x4000, SO_BROADCAST 1, optname 0x80); sendto `sub_3fa1c`; recvfrom `sub_3d368`; interface list `sub_3fb88` (`WSAIoctl 0x4004747F` = SIO_GET_INTERFACE_LIST); names `sub_3fdcc`/`sub_31a60`; threads via `CreateThread` in `RakPeer::vf1`.
IAT cells (ordinal -> name): 0x20B00C #116 WSACleanup, 0x20B010 #8 htonl, 0x20B014 WSAIoctl, 0x20B018 #57 gethostname, 0x20B01C #17 recvfrom, 0x20B020 #111 WSAGetLastError, 0x20B024 #6 getsockname, 0x20B028 #21 setsockopt, 0x20B02C #20 sendto, 0x20B030 #2 bind, 0x20B034 #23 socket, 0x20B038 #52 gethostbyname, 0x20B03C #7 getsockopt, 0x20B040 #3 closesocket, 0x20B044 #12 inet_ntoa, 0x20B048 #11 inet_addr, 0x20B04C #9 htons, 0x20B050 #15 ntohs, 0x20B054 #115 WSAStartup.

### Time / locale / device id - REAL-ON-RT / nothing to do
`GetLocalTime`, `GetSystemTimeAsFileTime`, `GetTickCount64`, `QueryPerformanceCounter/Frequency`, `_time64` are native. **No locale or language API**: language is hard-coded `"en_US"` (rdata 0x211F24, use 0x57e78) and `lang/en_US.lang` (sub_f3cb4). **No device-id API**: `/clientId.txt` is generated and stored in LocalFolder; AppPlatform vf35 returns four empty strings, vf42 "UNKNOWN", vf30 "<getPlatformStringVar NotImplemented>", vf39 "".

## AppPlatform_Winrt vtable (primary, RVA 0x33FB20 + 4*n; value = fn VA|1)
| n | fn RVA | meaning (inferred) | n | fn RVA | meaning (inferred) |
|---|---|---|---|---|---|
| 0 | 0x4ac14 | dtor | 21,22,27 | 0x29ba0 | ret 0 |
| 1 | 0x4eb90 | getImagePath(name,bool) | 23 | 0x4e350 | buy / licence ("TODO") |
| 2 | 0x4f028 | loadPNG | 24,25,36-38 | 0xbf60 | no-op |
| 3 | 0x47818 | loadTGA (base) | 26,28,11 | 0x17fd40 | ret 1 |
| 4,5,6,13,15,20 | 0xbf60 | no-op | 29 | 0x50ff0 | vibrate(ms) |
| 7 | 0x29ba0 | ret 0 | 30 | 0x47690 | getPlatformStringVar (NotImplemented text) |
| 8, 41 | 0x476f0 | returns zeroed 12-byte vector | 31 | 0x4fac4 | text/keyboard (HID, input area) |
| 9 | 0x4eb18 | getDateString(int) "N s (UTC)" | 32 | 0x4edac | keyboard hide-ish via HID dispatcher (input area) |
| 10 | 0x4e4d4 | counter at 0xDDD7E4 | 33 | 0x50f2c | set text (HID, input area) |
| 12 | 0x4f800 | savePNG | 34 | 0x47740 | returns byte platform+4 |
| 14 | 0x4f780 | readAssetFile | 35 | 0x47618 | returns 4 empty strings |
| 16,17,18 | 0x4ed00,0x4ece0,0x4ecc0 | screen width/height (+0x30/+0x34), float (+0x38) | 39 | 0x475f0 | returns "" |
| 19 | 0x4ef14 | isNetworkEnabled(bool) | 40 | 0x4eaf8 | getFreeMemory (u64) |
| | | | 42 | 0x47644 | returns "UNKNOWN" |
Base `AppPlatform` vtable 0x33F890 (slots 1,2,25 pure), `AppPlatformListener` 0x33F940 (7 slots, all no-op).

## PE imports vs Windows RT 8.1 desktop
* Source of truth: `python tools/arm32/apiset_scan.py <exe>` (16 api-set contracts) cross-checked with `arm32/device_logs/loadcheck_rt81_6.3.9600_OK.txt` (device run on 6.3.9600).
* **Native (proven)**: com-l1-1-1, file-l1-2-1, file-l2-1-1, handle-l1-1-0, processthreads-l1-1-2, profile-l1-1-0, string-l1-1-0, synch-l1-2-0, sysinfo-l1-2-1, util-l1-1-0, winrt-l1-1-0, winrt-error-l1-1-0/-1-1, winrt-string-l1-1-0.
* **Unproven**: `api-ms-win-core-errorhandling-l1-1-2` (RaiseException IAT 0x20B080, GetLastError 0x20B084; only -l1-1-0/-1-1 were seen on the device) - most likely missing (WP8.1-era contract): ship a forwarder shim and run loadcheck first; `api-ms-win-core-localization-l1-2-1` (FormatMessageW 0x20B0DC) - probably present (the Store VCLibs.120 DLLs, which run on RT, import it).
* **Not Windows components**: `msvcr120_app.dll` (149 imports), `msvcp120_app.dll` (123), `vccorlib120_app.DLL` (50). The Store appx `D:\apps\arms\Microsoft.VCLibs.120.00_12.0.21005.1_arm__8wekyb3d8bbwe.appx` contains all three and exports every imported symbol (0 missing, checked); DllCharacteristics 0x1140 -> make app-local copies with 0x0140 (APPCONTAINER cleared). Their own imports are 8.1 contracts.
* **System DLLs, expected present on RT 8.1**: `d3d11.dll`, `dxgi.dll!CreateDXGIFactory1`, `D3DCOMPILER_47.dll` (D3DCompile, D3DReflect; already used by the working 1.2.8 port), `WS2_32.dll` (19 ordinals), `XAudio2_8.dll!#1` (not yet exercised on the device - verify with `loadcheck.exe`).
* Phone-only runtime names in the PE: none beyond the `*_app` VC120 pair; the "Phone" part is entirely WinRT classes activated at runtime (list above), plus `Windows.Phone.Devices.Notification.VibrationDevice`.

## Prioritised list
**Needed before the main menu shows**
1. GetActivationFactory hook (IAT 0x20B5FC) + ASCII exe dir via fake Package -> IStorageItem.Path.
2. Fake ApplicationData.Current.LocalFolder (slot 12) -> writable ASCII dir (created beforehand).
3. Fake ThreadPool.RunAsync running the init-job delegate (Invoke slot 3) on an MTA thread; then RunWithPriorityAndOptionsAsync with an IAsyncAction/IAsyncInfo fake for the render loop.
4. Fake MemoryManager (or patch cell 0x33FBC0) for Options::init.
5. XAudio2_8.dll present (else patch SoundSystemRT cells 0xDB804C/0xDB8050 to no-ops); CRT/Win32 file I/O native.
6. VC120 `_app` DLL copies + (if missing) the errorhandling-l1-1-2 shim.

**Needed for gameplay**: vibrate bypass/fake (cell 0x33FB94), world storage (LevelDB + directory listing: native once LocalFolder is right), sound playback, RakNet UDP, clientId/options persistence, savePNG screenshots (vf12).

**Can wait**: NetworkInformation (cell 0x33FB6C), Store/buy (0x33FB7C), AsyncCausalityTracer, Windows.Web.Http/Realms, MainPage suspend/resume ApplicationData slots 8/10, vf31-34 text/keyboard (input area).

## Not found / uncertain
* Creation site of `IHttpClient` (job+0xd8) not found (never written); Realms request path likely broken/unreachable even on WP.
* Callers of platform vf19 (isNetworkEnabled), vf23 (buy), vf9/vf10 (SoundSystem) not individually traced (called through client-held platform pointers).
* Semantic names of platform slots other than those with clear bodies are inferred; slot -> function mapping is exact.
* IID at rdata 0x20FBE8 identity (IConnectionProfile2) is medium confidence; `IApplicationData` slots 8/10 names unknown (only slot 12 verified).
* Real behaviour of ThreadPool / AsyncCausalityTracer / NetworkInformation activation for an unpackaged RT 8.1 process is unverified; fakes recommended.
* `errorhandling-l1-1-2` availability on RT 8.1 needs the device's loadcheck.txt.
* Non-ASCII install / profile paths break the game's own narrowing (sub_480d8): untested GetShortPathNameW workaround.

## Friend notes cross-check (platform area only)
* Their IAT cells D3D/activation `0x20B5FC`, `0x20B1A4`, `0x20B1AC` match this exe (activation cell confirmed independently).
* "AppMain+0x3C setter not found" - it is set by the init-job lambda `0x1da2d8` (after `MinecraftClient::vf7` returns) which only runs through `ThreadPool.RunAsync`; "client+0x118/+0x11C NULL" = the init job never ran. Their host never fakes ThreadPool, so this is the missing piece.
* Their `Phone-only APIs assumed same set as prior recon` is not accurate for this exe: ThreadPool, ApplicationData, Package and MemoryManager are needed and their slot usage is listed above.

## Method
`arm32re.py`: `imports`, `impcallers`, `callers 2001e4`, `str/strx -w` for every `Windows.*` string, `rtti` (AppPlatform, SoundSystemRT, RakPeer, WinRTRestRequestJob, WinRTEnv), `vtable`, `func`; private helpers in the session scratchpad (`ps_iid.py` decodes the stack GUIDs, `ps_scan.py` regex-scans functions, `ps_bytes.py` expected bytes, `ps_vclibs*.py` compares exports of the Store VCLibs.120 DLLs with this exe's imports). `llvm-readobj --coff-exports` on this PC's XAudio2_8.dll and ws2_32.dll resolved the ordinals. No file under `D:\repo\win32craft\0.10.4.40` was modified.
