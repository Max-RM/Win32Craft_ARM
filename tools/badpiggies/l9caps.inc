/* l9caps.inc - correct Unity's in-memory graphics-capability record for feature level 9_1 (Bad Piggies Rebooted, UnityPlayer.dll 2018.4.36f1).
 *
 * Why: Unity's GfxDeviceD3D11 caps init (UnityPlayer sub_1007434) maps the real feature level through a table (.rdata 0x026767b8) that has NO 9_x row,
 * so a 9_1 device is treated as FL 12_1: it advertises 32-bit index buffers, 8 render targets, instancing, compute, 16384 textures.  Unity then issues
 * calls a 9_1 GPU rejects - most dangerously a 32-bit index-buffer bind, which can REMOVE the device (games_recon/bad_piggies_hw-unitygfx.md F4/F5, -hw-critic.md F6).
 * The shim's creation-time sanitisers cannot catch the context-side index bind, so the caps record itself must be corrected.
 *
 * How: this runs ONLY in the shim's 9_x wrap path (so FL10+ GPUs are untouched - the project rule "never force a feature level").  It is armed after the
 * device is wrapped (l9caps_arm) and fires once from the shim's CheckFeatureSupport hook on Unity's last caps-init query (D3D11_FEATURE 0xf), by which point
 * sub_1007434 has filled the caps object; it then overwrites the fields with 9_1-correct values.  Guarded by two anchor instructions in sub_1007434, so it
 * is a silent no-op on any other UnityPlayer build.  Every offset was confirmed by disassembling UnityPlayer.dll (sub_1007434, 2026-10-22). */

#define L9_UNITY_CAPS_GLOBAL_RVA   0x0298ed40u   /* movw/movt 0x1298ed40 in sub_733428: base+this holds the caps-object pointer */
#define L9_UNITY_ANCHOR1_RVA       0x0100776cu   /* strb.w sb,[r6,#0x104]  -> 86 f8 04 91 */
#define L9_UNITY_ANCHOR2_RVA       0x0100763au   /* str.w  r3,[r6,#0xbc]   -> c6 f8 bc 30 */

static ULONG_PTR g_unity_base;
static int g_caps_armed, g_caps_done, g_fl_queried;
static int g_caps_logonly = 0;   /* 0 = apply the 9_1 correction, gated on the caps object's content looking like Unity's GfxCaps (needed so Unity does not
                                    attempt 32-bit indices / MRT / instancing the 9_1 device cannot do).  1 = validate + log only.  The error-shader crash
                                    seen earlier was NOT this write: it was the "unity default resources" file shipped with a %20 in its name (now decoded). */

static int l9_bytes_eq(ULONG_PTR addr, const u8 *want, int n)
{
    const u8 *p = (const u8 *)addr;
    int i;
    if (IsBadReadPtr(p, n)) return 0;
    for (i = 0; i < n; ++i) if (p[i] != want[i]) return 0;
    return 1;
}

/* arm the caps fix-up once the shim has wrapped a 9_x device (called from wrap_device).  Verifies this is the known UnityPlayer build. */
static void l9caps_arm(void)
{
    static const u8 a1[4] = { 0x86, 0xf8, 0x04, 0x91 };
    static const u8 a2[4] = { 0xc6, 0xf8, 0xbc, 0x30 };
    HMODULE m = GetModuleHandleW(L"UnityPlayer.dll");
    if (!m) { lgf("l9caps: UnityPlayer.dll not loaded - caps fix-up disabled (not the Bad Piggies host?)"); return; }
    g_unity_base = (ULONG_PTR)m;
    if (!l9_bytes_eq(g_unity_base + L9_UNITY_ANCHOR1_RVA, a1, 4) || !l9_bytes_eq(g_unity_base + L9_UNITY_ANCHOR2_RVA, a2, 4)) {
        lgf("l9caps: UnityPlayer.dll caps-init anchors do not match this build - caps fix-up disabled (a no-op, nothing patched)");
        g_unity_base = 0;
        return;
    }
    g_caps_armed = 1;
    lgf("l9caps: armed (UnityPlayer base %p; the 9_1 caps correction fires on Unity's last caps-init query)", (void *)g_unity_base);
}

/* overwrite the caps object with 9_1-correct values.  `caps` is Unity's GfxCaps (returned by sub_733428).  Idempotent. */
static void l9caps_apply(void *caps)
{
    static const struct { u32 off, val; } dwords[] = {
        { 0x0bc, 0x000000E3u },   /* shaderCaps mask = "target 3.0" (keep the 0x21/0xE3 shader programs; 0x1 would make Unity drop them) */
        { 0x0dc, 2048 },          /* maxTextureSize */
        { 0x0e4, 2048 },          /* maxRenderTextureSize */
        { 0x0e0, 512 },           /* maxCubemapSize */
        { 0x0e8, 256 },           /* max 3D texture size */
        { 0x0ec, 1 },             /* SupportedRenderTargetCount (one at 9_1) */
        { 0x0f0, 0 },             /* SupportedRandomWriteTargetCount (no UAVs) */
        { 0x378, 1 }, { 0x37c, 1 },/* NPOTSupport = Restricted (conditional: 1 mip, no WRAP) */
        { 0x394, 0 },             /* idx>=2 feature word */
        { 0x3f4, 0 },             /* caps-table row index -> tier 0 */
    };
    static const u32 bytes[] = {
        0x104,   /* Supports32bitsIndexBuffer -> 0  (THE critical one: a 32-bit index bind can remove a 9_1 device) */
        0x0ff,   /* SupportsSeparatedRenderTargetsBlend */
        0x107,   /* SupportsMultisampledTextures */
        0x36c,   /* SupportsTextureWrapMirrorOnce */
        0x375, 0x376,  /* Supports3DTextures */
        0x386,   /* Supports3DRenderTextures */
        0x3d4, 0x3d5,  /* SupportsAsyncGPUReadback */
        0x3f8,   /* idx>=1 feature flag */
        0x3d9,   /* idx>=2 feature flag */
        0x101    /* OutputMergerLogicOp (Unsupported below 11_1) */
    };
    u8 *c = (u8 *)caps;
    unsigned i;
    if (!c || IsBadReadPtr(c, 0x400) || IsBadWritePtr(c, 0x400)) { lgf("l9caps: caps object %p not writable - fix-up skipped", (void *)c); return; }
    for (i = 0; i < sizeof dwords / sizeof dwords[0]; ++i) *(u32 *)(c + dwords[i].off) = dwords[i].val;
    for (i = 0; i < sizeof bytes / sizeof bytes[0]; ++i) c[bytes[i]] = 0;
    lgf("l9caps: caps object %p corrected to 9_1 (shaderCaps 0xE3, maxTex 2048, 1 render target, no 32-bit indices / MRT / instancing / 3D / mirror-once / readback)", (void *)c);
}

/* called from the shim's CheckFeatureSupport hook.  feature 0xf (D3D11_FEATURE_D3D11_OPTIONS3) is the LAST device query sub_1007434 makes on the
 * Surface 2 / RT 8.1 (OS build >= 1000), i.e. the caps object is fully filled by then. */
static void l9caps_on_check_feature(UINT feature)
{
    u8 *c;
    u32 rowIdx, shaderCaps, maxTex;
    int looks_real;
    if (!g_caps_armed || g_caps_done || !g_unity_base) return;
    if (feature != 0xf) return;                 /* D3D11_FEATURE 0xf = Unity's last caps-init query (sub_1007434 is done filling the caps object) */
    c = *(u8 **)(g_unity_base + L9_UNITY_CAPS_GLOBAL_RVA);
    if (!c) { lgf("l9caps: the caps global is still NULL at the OPTIONS3 query - not applied yet"); return; }
    if (IsBadReadPtr(c, 0x400) || IsBadWritePtr(c, 0x400)) { lgf("l9caps: caps object %p is not readable/writable - NOT applied", (void *)c); g_caps_done = 1; return; }
    rowIdx = *(u32 *)(c + 0x3f4); shaderCaps = *(u32 *)(c + 0xbc); maxTex = *(u32 *)(c + 0xdc);
    /* log the unpatched values (a device log then shows the fix was needed and the object is really Unity's) */
    lgf("l9caps: caps object %p BEFORE: rowIdx(+0x3f4)=%u shaderCaps(+0xbc)=0x%x maxTex(+0xdc)=%u RTcount(+0xec)=%u idx32(+0x104)=%u NPOT(+0x378)=%u",
        (void *)c, rowIdx, shaderCaps, maxTex, *(u32 *)(c + 0xec), (unsigned)c[0x104], *(u32 *)(c + 0x378));
    /* content gate: at FL 9_1 with the unpatched caps table Unity stored the 12_1 row - rowIdx small, a non-zero/non-0xffffffff shader mask, idx32 0/1,
     * a plausible texture limit.  If it does not look like that, the global is not what we think: do NOT write (protects against a wrong pointer). */
    looks_real = rowIdx <= 8 && shaderCaps != 0 && shaderCaps != 0xffffffffu && c[0x104] <= 1 && maxTex && maxTex <= 0x10000u;
    if (!looks_real) { lgf("l9caps: the caps object does not look like Unity's GfxCaps (unexpected values) - NOT applied, to be safe"); g_caps_done = 1; return; }
    if (g_caps_logonly) { lgf("l9caps: LOG-ONLY - not writing the 9_1 correction"); g_caps_done = 1; return; }
    l9caps_apply(c);
    g_caps_done = 1;
}
