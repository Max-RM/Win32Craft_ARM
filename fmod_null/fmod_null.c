/*
 * fmod_null.c - SILENT drop-in replacement for fmod_ARM.dll (Minecraft 1.2.8 ARM32, FMOD low-level 1.09.04).
 *
 * The game (Minecraft.Windows.exe, ARM32) imports exactly 39 symbols from fmod_ARM.dll.  This DLL exports exactly
 * those 39 names (see fmod_null_arm32.def, which maps each C++-mangled name onto one of the C functions below) and
 * implements every one of them as "succeed, write plausible out-parameters, make no sound".
 *
 * Design constraints
 *   - freestanding: no CRT, no imports at all (not even kernel32), no globals needing constructors, no exceptions;
 *   - every object handed to the game ("System", "ChannelGroup", "Sound", "Channel") is a 16-byte record in a static
 *     pool, so the handle has a stable, non-NULL, process-lifetime address; pools are handed out round-robin with a
 *     lock-free atomic counter (the game calls createSound from worker threads), and a wrapped slot is simply reused
 *     because the records carry no state the caller can observe; nothing is ever freed, release() is a no-op;
 *   - the game uses FMOD through the C++ API of 1.09 (QAA = AAPCS-VFP, member function, `this` in r0, FMOD_RESULT in
 *     r0).  Only out-pointers are ever dereferenced and each is NULL-checked first;
 *   - none of the callbacks passed to setFileSystem is ever called.
 *
 * ABI summary (AAPCS-VFP, hard float; verified against the real call sites in SoundSystemFMOD, see
 * arm32/facts/fmod.json null_impl_table and fmod_null/README.md):
 *   r0 = this (or the first argument for the C export FMOD_System_Create), r1..r3 = next args, args 5.. on the stack
 *   at [sp], [sp+4], ...; floats in s0..; 64-bit integers occupy an even register pair (r2:r3) or an 8-aligned
 *   stack slot.  clang's C ABI for --target=thumbv7-pc-windows-msvc -mfloat-abi=hard is the same, so declaring the
 *   functions with the natural C prototypes reproduces the layout exactly.
 *
 * Build: build_fmod_null_arm32.bat   Test: python arm32/tests/fmod_null_test.py
 */

typedef unsigned int u32;
typedef unsigned long long u64;

#define FMOD_OK 0

/* 'NULL' little endian; lets the unit test tell the object kinds apart. */
#define NULL_OBJ_MAGIC 0x4C4C554Eu

#define KIND_SYSTEM  1u
#define KIND_GROUP   2u
#define KIND_SOUND   3u
#define KIND_CHANNEL 4u

/* FMOD 1.09 enum values that matter */
#define FMOD_VERSION_1_09_04    0x00010904u  /* the game requires >= this (SoundSystemFMOD::createSystem) */
#define FMOD_SOUND_TYPE_WAV     15           /* never 9 (MPEG) or 18 (AUDIOQUEUE): the game rejects those */
#define FMOD_SOUND_FORMAT_PCM16 2
#define FMOD_SPEAKERMODE_STEREO 3

#define POOL_GROUPS   256u    /* all pool sizes are powers of two (mask instead of modulo, no division helper) */
#define POOL_SOUNDS   4096u
#define POOL_CHANNELS 1024u

typedef struct NullObj {
    u32 magic;
    u32 kind;
    u32 serial;    /* hand-out counter value (diagnostics only) */
    u32 flags;
} NullObj;

static NullObj g_system;
static NullObj g_master_group;
static NullObj g_groups[POOL_GROUPS];
static NullObj g_sounds[POOL_SOUNDS];
static NullObj g_channels[POOL_CHANNELS];

static u32 g_group_ctr;
static u32 g_sound_ctr;
static u32 g_channel_ctr;

static NullObj *obj_fill(NullObj *o, u32 kind, u32 serial)
{
    o->magic = NULL_OBJ_MAGIC;
    o->kind = kind;
    o->serial = serial;
    return o;
}

static NullObj *obj_pick(NullObj *pool, u32 mask, u32 *counter, u32 kind)
{
    u32 n = __atomic_fetch_add(counter, 1u, __ATOMIC_RELAXED);
    return obj_fill(&pool[n & mask], kind, n);
}

/* out-pointer helpers: every write is guarded */
static void put_ptr(void **out, void *value) { if (out) *out = value; }
static void put_int(int *out, int value)     { if (out) *out = value; }

/* 64-bit out values are written as two 32-bit stores: the caller only guarantees 4-byte alignment on the stack */
static void put_u64(u64 *out, u32 lo, u32 hi)
{
    if (out) {
        u32 *p = (u32 *)out;
        p[0] = lo;
        p[1] = hi;
    }
}

/* ------------------------------------------------------------------ System */

/* FMOD_System_Create(FMOD_SYSTEM **system)   [C export, undecorated, ONE argument in r0] */
int fmod_FMOD_System_Create(void **system)
{
    put_ptr(system, obj_fill(&g_system, KIND_SYSTEM, 0));
    return FMOD_OK;
}

/* System::getVersion(unsigned *version) */
int fmod_System_getVersion(void *self, u32 *version)
{
    (void)self;
    if (version) *version = FMOD_VERSION_1_09_04;
    return FMOD_OK;
}

/* System::init(int maxchannels, unsigned flags, void *extradriverdata)   game: (0x20, 4, NULL) */
int fmod_System_init(void *self, int maxchannels, u32 flags, void *extradriverdata)
{
    (void)self; (void)maxchannels; (void)flags; (void)extradriverdata;
    return FMOD_OK;
}

/* System::set3DSettings(float dopplerscale, float distancefactor, float rolloffscale)   floats in s0..s2 */
int fmod_System_set3DSettings(void *self, float doppler, float distance, float rolloff)
{
    (void)self; (void)doppler; (void)distance; (void)rolloff;
    return FMOD_OK;
}

/* System::createChannelGroup(const char *name, ChannelGroup **group) */
int fmod_System_createChannelGroup(void *self, const char *name, void **group)
{
    (void)self; (void)name;
    put_ptr(group, obj_pick(g_groups, POOL_GROUPS - 1u, &g_group_ctr, KIND_GROUP));
    return FMOD_OK;
}

/* System::getMasterChannelGroup(ChannelGroup **group) */
int fmod_System_getMasterChannelGroup(void *self, void **group)
{
    (void)self;
    put_ptr(group, obj_fill(&g_master_group, KIND_GROUP, 0));
    return FMOD_OK;
}

/* ChannelGroup::addGroup(ChannelGroup *group, bool propagatedspclock, DSPConnection **connection)
 * game: (master, sound|music group, true, NULL); `propagate` is a bool in r2, connection in r3 */
int fmod_ChannelGroup_addGroup(void *self, void *group, int propagate, void **connection)
{
    (void)self; (void)group; (void)propagate;
    put_ptr(connection, 0);
    return FMOD_OK;
}

int fmod_System_close(void *self)         { (void)self; return FMOD_OK; }
int fmod_System_release(void *self)       { (void)self; return FMOD_OK; }   /* static handle, never freed */
int fmod_System_mixerResume(void *self)   { (void)self; return FMOD_OK; }
int fmod_System_mixerSuspend(void *self)  { (void)self; return FMOD_OK; }
int fmod_System_update(void *self)        { (void)self; return FMOD_OK; }

/* System::set3DNumListeners(int n) */
int fmod_System_set3DNumListeners(void *self, int n)
{
    (void)self; (void)n;
    return FMOD_OK;
}

/* System::setOutput(FMOD_OUTPUTTYPE) / setDriver(int) */
int fmod_System_setOutput(void *self, int output) { (void)self; (void)output; return FMOD_OK; }
int fmod_System_setDriver(void *self, int driver) { (void)self; (void)driver; return FMOD_OK; }

/* System::getNumDrivers(int *numdrivers): 0, so SoundSystemFMOD::selectDriver skips its getDriverInfo/setDriver loop */
int fmod_System_getNumDrivers(void *self, int *numdrivers)
{
    (void)self;
    put_int(numdrivers, 0);
    return FMOD_OK;
}

/* System::getDriverInfo(int id, char *name, int namelen, FMOD_GUID *guid, int *systemrate,
 *                       FMOD_SPEAKERMODE *speakermode, int *speakermodechannels)
 * r1=id r2=name r3=namelen, [sp]=guid [sp+4]=rate [sp+8]=speakermode [sp+12]=channels.
 * Not reached while getNumDrivers() == 0; written anyway as "one harmless driver" with every output guarded. */
int fmod_System_getDriverInfo(void *self, int id, char *name, int namelen, void *guid,
                              int *systemrate, int *speakermode, int *speakermodechannels)
{
    static const char driver_name[] = "Null Output";
    (void)self; (void)id;
    if (name && namelen > 0) {
        int i = 0;
        while (i < namelen - 1 && driver_name[i]) {
            name[i] = driver_name[i];
            i++;
        }
        name[i] = 0;
    }
    if (guid) {                      /* FMOD_GUID = u32 + u16 + u16 + u8[8] = 16 bytes; all zero */
        unsigned char *g = (unsigned char *)guid;
        int i;
        for (i = 0; i < 16; i++) g[i] = 0;
    }
    put_int(systemrate, 48000);
    put_int(speakermode, FMOD_SPEAKERMODE_STEREO);
    put_int(speakermodechannels, 2);
    return FMOD_OK;
}

/* System::getSoftwareFormat(int *samplerate, FMOD_SPEAKERMODE *speakermode, int *numrawspeakers)
 * game: (&rate, NULL, NULL); the rate scales fade/delay lengths into DSP-clock units */
int fmod_System_getSoftwareFormat(void *self, int *samplerate, int *speakermode, int *numrawspeakers)
{
    (void)self;
    put_int(samplerate, 48000);
    put_int(speakermode, FMOD_SPEAKERMODE_STEREO);
    put_int(numrawspeakers, 2);
    return FMOD_OK;
}

/* System::setFileSystem(userOpen, userClose, userRead, userSeek, userAsyncRead, userAsyncCancel, int blockalign)
 * r1=open r2=close r3=read, [sp]=seek [sp+4]=asyncread(NULL) [sp+8]=asynccancel(NULL) [sp+12]=blockalign(-1).
 * The callbacks are accepted and ignored: nothing is ever opened, so they are never invoked. */
int fmod_System_setFileSystem(void *self, void *useropen, void *userclose, void *userread, void *userseek,
                              void *userasyncread, void *userasynccancel, int blockalign)
{
    (void)self; (void)useropen; (void)userclose; (void)userread; (void)userseek;
    (void)userasyncread; (void)userasynccancel; (void)blockalign;
    return FMOD_OK;
}

/* System::set3DListenerAttributes(int listener, const FMOD_VECTOR *pos, *vel, *forward, *up)
 * r1=listener r2=pos r3=vel, [sp]=forward [sp+4]=up */
int fmod_System_set3DListenerAttributes(void *self, int listener, const void *pos, const void *vel,
                                        const void *forward, const void *up)
{
    (void)self; (void)listener; (void)pos; (void)vel; (void)forward; (void)up;
    return FMOD_OK;
}

/* System::createSound / createStream(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo,
 *                                    Sound **sound)       r1=name r2=mode r3=exinfo, [sp]=Sound **sound */
int fmod_System_createSound(void *self, const char *name, u32 mode, void *exinfo, void **sound)
{
    (void)self; (void)name; (void)mode; (void)exinfo;
    put_ptr(sound, obj_pick(g_sounds, POOL_SOUNDS - 1u, &g_sound_ctr, KIND_SOUND));
    return FMOD_OK;
}

int fmod_System_createStream(void *self, const char *name, u32 mode, void *exinfo, void **sound)
{
    (void)self; (void)name; (void)mode; (void)exinfo;
    put_ptr(sound, obj_pick(g_sounds, POOL_SOUNDS - 1u, &g_sound_ctr, KIND_SOUND));
    return FMOD_OK;
}

/* System::playSound(Sound *sound, ChannelGroup *group, bool paused, Channel **channel)
 * r1=sound r2=group r3=paused, [sp]=Channel **channel  (game: stored into SoundSystemFMOD+0x84, then setVolume etc.) */
int fmod_System_playSound(void *self, void *sound, void *group, int paused, void **channel)
{
    (void)self; (void)sound; (void)group; (void)paused;
    put_ptr(channel, obj_pick(g_channels, POOL_CHANNELS - 1u, &g_channel_ctr, KIND_CHANNEL));
    return FMOD_OK;
}

/* ------------------------------------------------------------------- Sound */

int fmod_Sound_release(void *self) { (void)self; return FMOD_OK; }

/* Sound::setMode(FMOD_MODE) */
int fmod_Sound_setMode(void *self, u32 mode) { (void)self; (void)mode; return FMOD_OK; }

/* Sound::set3DMinMaxDistance(float min, float max)   s0, s1 */
int fmod_Sound_set3DMinMaxDistance(void *self, float min, float max)
{
    (void)self; (void)min; (void)max;
    return FMOD_OK;
}

/* Sound::getFormat(FMOD_SOUND_TYPE *type, FMOD_SOUND_FORMAT *format, int *channels, int *bits)
 * r1=type r2=format r3=channels, [sp]=bits.  game (soundFormatAccepted): (&type, NULL, NULL, NULL) and it releases
 * the sound when type is 9 (MPEG) or 0x12 (AUDIOQUEUE) - so report WAV. */
int fmod_Sound_getFormat(void *self, int *type, int *format, int *channels, int *bits)
{
    (void)self;
    put_int(type, FMOD_SOUND_TYPE_WAV);
    put_int(format, FMOD_SOUND_FORMAT_PCM16);
    put_int(channels, 2);
    put_int(bits, 16);
    return FMOD_OK;
}

/* Sound::getNumSubSounds(int *n): 0, so the game never iterates getSubSound */
int fmod_Sound_getNumSubSounds(void *self, int *numsubsounds)
{
    (void)self;
    put_int(numsubsounds, 0);
    return FMOD_OK;
}

/* Sound::getSubSound(int index, Sound **subsound): not reached, but returns a valid sound anyway */
int fmod_Sound_getSubSound(void *self, int index, void **subsound)
{
    (void)self; (void)index;
    put_ptr(subsound, obj_pick(g_sounds, POOL_SOUNDS - 1u, &g_sound_ctr, KIND_SOUND));
    return FMOD_OK;
}

/* ------------------------------------------------- Channel / ChannelGroup */

int fmod_ChannelControl_stop(void *self) { (void)self; return FMOD_OK; }

/* setVolume(float) / setPitch(float)   s0 */
int fmod_ChannelControl_setVolume(void *self, float volume) { (void)self; (void)volume; return FMOD_OK; }
int fmod_ChannelControl_setPitch(void *self, float pitch)   { (void)self; (void)pitch;  return FMOD_OK; }

/* setPaused(bool) / setMute(bool)   bool in r1 */
int fmod_ChannelControl_setPaused(void *self, int paused) { (void)self; (void)paused; return FMOD_OK; }
int fmod_ChannelControl_setMute(void *self, int mute)     { (void)self; (void)mute;   return FMOD_OK; }

/* set3DAttributes(const FMOD_VECTOR *pos, const FMOD_VECTOR *vel, const FMOD_VECTOR *alt_pan_pos) */
int fmod_ChannelControl_set3DAttributes(void *self, const void *pos, const void *vel, const void *altpanpos)
{
    (void)self; (void)pos; (void)vel; (void)altpanpos;
    return FMOD_OK;
}

/* getDSPClock(unsigned long long *dspclock, unsigned long long *parentclock)
 * game: (NULL, &local): both pointers optional */
int fmod_ChannelControl_getDSPClock(void *self, u64 *dspclock, u64 *parentclock)
{
    (void)self;
    put_u64(dspclock, 0, 0);
    put_u64(parentclock, 0, 0);
    return FMOD_OK;
}

/* addFadePoint(unsigned long long dspclock, float volume)   clock in r2:r3 (r1 skipped), volume in s0 */
int fmod_ChannelControl_addFadePoint(void *self, u64 dspclock, float volume)
{
    (void)self; (void)dspclock; (void)volume;
    return FMOD_OK;
}

/* setDelay(unsigned long long dspclock_start, unsigned long long dspclock_end, bool stopchannels)
 * start in r2:r3, end in [sp]/[sp+4], stopchannels in [sp+8] */
int fmod_ChannelControl_setDelay(void *self, u64 start, u64 end, int stopchannels)
{
    (void)self; (void)start; (void)end; (void)stopchannels;
    return FMOD_OK;
}

/* isPlaying(bool *isplaying): ONE byte; always "finished" so the game reaps the channel at once */
int fmod_ChannelControl_isPlaying(void *self, unsigned char *isplaying)
{
    (void)self;
    if (isplaying) *isplaying = 0;
    return FMOD_OK;
}

/* ---------------------------------------------------------------- DllMain */

int DllMain(void *instance, u32 reason, void *reserved)
{
    (void)instance; (void)reason; (void)reserved;
    return 1;
}
