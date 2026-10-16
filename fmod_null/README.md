# fmod_null - silent fmod_ARM.dll for Minecraft 1.2.8 ARM32

A 6 KB drop-in replacement for the game's `fmod_ARM.dll` (FMOD low-level 1.09.04). It exports exactly the 39 symbols
that `Minecraft.Windows.exe` (ARM32) imports from `fmod_ARM.dll`, answers every call with `FMOD_OK`, and never makes a
sound. It has **no imports at all** (no kernel32, no CRT, no WinRT, no VCRUNTIME140_APP), so it loads on any Windows
ARM32 desktop, and it removes the two ways the real FMOD can stop the game from starting: the unresolved
`VCRUNTIME140_APP.dll` import and the `INFINITE` wait for `DeviceWatcher` enumeration inside `System::init`
(see `arm32/facts/fmod.md`, option C).

## Use

1. Build (cmd.exe, repo root): `build_fmod_null_arm32.bat`  ->  `dist-fmod-null-arm32\fmod_ARM.dll`
   Overridable env vars: `LLVM_BIN`, `CC`, `LINKER` (alias `LINK`), `READOBJ`, `BUILD`, `DIST`, `RUN_TESTS=1`, `PYTHON`.
2. Install into the game folder **after** the patcher has run (the patcher/provisioning must not overwrite it):
   * rename the shipped DLL: `fmod_ARM.dll` -> `fmod_ARM.real.dll` (keep it; it is the only way back to sound);
   * copy `dist-fmod-null-arm32\fmod_ARM.dll` next to `Minecraft.Windows.exe`.
   No exe edit is needed: the exe already imports `fmod_ARM.dll` by name, and the loader matches the 39 names.
3. Go back to real audio: delete the stub and rename `fmod_ARM.real.dll` back to `fmod_ARM.dll`.
4. Test (no device needed): `python arm32/tests/fmod_null_test.py [path\to\fmod_ARM.dll]` (Unicorn; needs `unicorn`).

## What is silenced

Everything FMOD does. The game sees: `FMOD_System_Create` and `System::getVersion` succeed (version `0x00010904`, the
minimum the game accepts), `init`/`setFileSystem`/`set3DSettings`/channel groups/master group/`addGroup` succeed,
`getNumDrivers` is 0 (the driver-selection loop is skipped), `getSoftwareFormat` reports 48000 Hz stereo,
`createSound`/`createStream` hand out dummy `Sound` handles (type WAV, 0 sub-sounds; never MPEG/AUDIOQUEUE, which the
game would reject), `playSound` hands out a dummy `Channel` and every channel setter returns OK, `isPlaying` is always
false so the game reaps channels immediately, `getDSPClock` is 0. The `setFileSystem` callbacks are stored nowhere and
never called, so no sound file is ever opened or read. All out-pointers are NULL-checked before writing; no argument
other than an out-pointer is ever dereferenced.

Handles are 16-byte records in static pools (1 system, 1 master + 256 groups, 4096 sounds, 1024 channels) handed out
round-robin with a lock-free atomic counter (the game may call `createSound` from worker threads). `release` does
nothing; after 4096 loads a Sound handle is reused, which is unobservable because handles carry no state.

## Known limitations

* No audio, ever. This is the *fallback* (option C); the primary plan is the patched real DLL (option D in
  `fmod.md`). Use this stub if the real DLL fails to load, hangs in `System::init`, or while bringing up the port.
* Only the 39 imported functions exist. Any other FMOD export (a different game build, plugins) is missing and the
  loader would fail with an unresolved import - this DLL is specific to the 1.2.8 ARM32 exe.
* Code uses a NEON store in `getDriverInfo` (project flags `-mfpu=neon`); every ARM32 Windows device has NEON.
* Verified in an emulator (Unicorn, ARM/Thumb-2, AAPCS-VFP) and against the real call sites decoded with
  `tools/arm32/arm32re.py`; **not yet run on a real device**.
* Signature table the code follows (r0 = `this`; stack args at `[sp]`, `[sp+4]`...; floats in `s0..`; 64-bit values in
  `r2:r3`): `createSound`/`createStream` 5th arg `Sound**` on the stack, `playSound` 5th arg `Channel**` on the stack,
  `getFormat` 5th arg `bits*` on the stack, `getDriverInfo` args 4..7 on the stack, `setFileSystem` args 5..8 on the
  stack, `set3DListenerAttributes` forward/up on the stack, `addFadePoint(clock r2:r3, volume s0)`,
  `setDelay(start r2:r3, end [sp], stop [sp+8])`.
