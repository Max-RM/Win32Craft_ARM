/* Host-side (x86, runs on the PC) test of the --trace-game-state Direct3D context tracer in win32_host_arm32_0104_panel.inl (section 2b): the REAL panel unit against the
 * pristine stubs of the other units.  A fake ID3D11DeviceContext (a vtable of recording functions) and fake state objects (GetDesc at slot 7) are driven through the patched
 * vtable exactly like the game does; the test checks that every call is forwarded unchanged and that the per-frame record / log lines are right.
 * Build + run:  python tools/arm32/test_0104_panel_tr.py
 * (the ARM register passing of the float argument of ClearDepthStencilView is NOT covered here; it is a plain C prototype that the ARM compiler maps like the game's call.) */
#include "win32_compat.h"

extern void *WINAPI GetStdHandle(DWORD);
extern char *strstr(const char *, const char *);
extern int strcmp(const char *, const char *);
extern int WINAPI wvsprintfA(LPSTR, LPCSTR, __builtin_va_list);
extern void WINAPI RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR *);

static int g_failures;
static int g_checks;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; test_print("CHECK FAILED line %d: %s\n", __LINE__, #cond); } } while (0)

static void test_print(const char *format, ...)
{
    char line[1100];
    DWORD written;
    __builtin_va_list arguments;
    __builtin_va_start(arguments, format);
    wvsprintfA(line, format, arguments);
    __builtin_va_end(arguments);
    WriteFile(GetStdHandle((DWORD)-11), line, (DWORD)strlen(line), &written, NULL);
}

/* ---- stand-ins for win32_host.c ---- */
static char g_log[200000];
static int g_log_len;
static int g_verbose = 0;
static void log_line(const char *text)
{
    int length = (int)strlen(text);
    if (g_log_len + length + 2 < (int)sizeof(g_log)) {
        memcpy(g_log + g_log_len, text, (size_t)length);
        g_log_len += length;
        g_log[g_log_len++] = '\n';
        g_log[g_log_len] = 0;
    }
    if (g_verbose) test_print("  [log] %s\n", text);
}
static int log_has(const char *needle) { return strstr(g_log, needle) != NULL; }
static void log_hresult(const char *operation, HRESULT result) { test_print("  [hr] %s 0x%08lx\n", operation, (unsigned long)result); }
static wchar_t host_ascii_lower_w(wchar_t value) { return (value >= L'A' && value <= L'Z') ? (wchar_t)(value + (L'a' - L'A')) : value; }
static BOOL host_argument_equals(const wchar_t *left, const wchar_t *right)
{
    while (*left && *right) {
        if (host_ascii_lower_w(*left) != host_ascii_lower_w(*right)) return FALSE;
        ++left;
        ++right;
    }
    return *left == 0 && *right == 0;
}
static const wchar_t *host_next_argument(const wchar_t *cursor, wchar_t *argument, UINT capacity)
{
    UINT length = 0;
    BOOL quoted = FALSE;
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    while (*cursor) {
        if (*cursor == L'"') { quoted = !quoted; ++cursor; continue; }
        if (!quoted && (*cursor == L' ' || *cursor == L'\t')) break;
        if (length + 1 < capacity) argument[length++] = *cursor;
        ++cursor;
    }
    argument[length] = 0;
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    return cursor;
}
static const wchar_t *arm32_iat_argument_value(const wchar_t *argument, const wchar_t *prefix)
{
    while (*prefix) {
        if (host_ascii_lower_w(*argument) != host_ascii_lower_w(*prefix)) return NULL;
        ++argument;
        ++prefix;
    }
    return argument;
}
extern HRESULT WINAPI WindowsDeleteString(HSTRING);
extern BOOL WINAPI WindowsIsStringEmpty(HSTRING);
#define ARM32_SRC_MOUSEKB 1
#define ARM32_SRC_TOUCH 2
static BOOL arm32_check_bytes(const BYTE *address, const BYTE *expected, SIZE_T size, const char *what) { (void)what; return memcmp(address, expected, size) == 0; }
/* a cell patch that really writes the cell (the test vtables are plain heap memory) */
static void arm32_iat_describe_address(const void *address, char *out, SIZE_T out_size)
{
    (void)address;
    if (out_size) { SIZE_T i; const char *text = "test address"; for (i = 0; i + 1 < out_size && text[i]; ++i) out[i] = text[i]; out[i] = 0; }
}

static BOOL arm32_patch_cell(void **cell, const void *expected, void *replacement, void **original_out, const char *what)
{
    (void)what;
    if (expected && *cell != expected) return FALSE;
    if (original_out) *original_out = *cell;
    *cell = replacement;
    return TRUE;
}
static BOOL arm32_patch_byte(BYTE *address, BYTE expected, BYTE replacement, const char *what) { (void)what; if (*address != expected) return FALSE; *address = replacement; return TRUE; }
static void *test_calloc(size_t count, size_t size) { return HeapAlloc(GetProcessHeap(), 8 /* HEAP_ZERO_MEMORY */, count * size); }
static BOOL arm32_iat_hook_import(BYTE *image, BOOL by_name, const char *name, DWORD rva, void *hook, void **original, const char *what) { (void)image; (void)by_name; (void)name; (void)rva; (void)hook; (void)original; (void)what; return TRUE; }
static void **arm32_iat_find_cell(BYTE *image, BOOL delay, const char *name) { (void)image; (void)delay; (void)name; return NULL; }
static UINT arm32_feature_level_cap = 0x9100u;
static void arm32_osk_show(const char *why) { (void)why; }
static void arm32_osk_hide(const char *why) { (void)why; }
static void arm32_osk_poll(void) { }
static BOOL arm32_osk_rect_buffer(float rect[4]) { (void)rect; return FALSE; }
static void arm32_osk_note_pointer_up(void) { }
static void arm32_input_note_source(int source, const char *why) { (void)source; (void)why; }
static BOOL arm32_input_touch_active(void) { return FALSE; }
static BOOL text_input_active;
static volatile LONG arm32_boot_frames_started, arm32_boot_frames_done, arm32_boot_progress, arm32_boot_loop_passes;
static DWORD arm32_boot_start_tick;
static void arm32_boot_note_frame(LONG frame, DWORD loop_start, DWORD frame_ms) { (void)frame; (void)loop_start; (void)frame_ms; }

#include "win32_host_arm32_0104_core.inl"

#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_async.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_app.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_input.inl"
#include "arm32/v0_10_4/stubs/win32_host_arm32_0104_platform.inl"
#include "win32_host_arm32_0104_panel.inl"

/* ---- the fake context: a vtable of 54 recording functions ---- */
static int g_calls[X104_PANEL_TR_SLOTS];
static UINT g_last_a, g_last_b, g_last_c;
static void *g_last_p;
static float g_last_depth;
static BYTE g_last_stencil;

static void WINAPI fake_di(void *self, UINT count, UINT start, INT base) { (void)self; (void)base; ++g_calls[12]; g_last_a = count; g_last_b = start; g_last_c = (UINT)base; }
static void WINAPI fake_draw(void *self, UINT count, UINT start) { (void)self; ++g_calls[13]; g_last_a = count; g_last_b = start; }
static void WINAPI fake_omrt(void *self, UINT count, void *const *views, void *depth) { (void)self; ++g_calls[33]; g_last_a = count; g_last_p = depth; (void)views; }
static void WINAPI fake_bs(void *self, void *state, const float *factor, UINT mask) { (void)self; (void)factor; ++g_calls[35]; g_last_p = state; g_last_a = mask; }
static void WINAPI fake_dss(void *self, void *state, UINT reference) { (void)self; ++g_calls[36]; g_last_p = state; g_last_a = reference; }
static void WINAPI fake_rs(void *self, void *state) { (void)self; ++g_calls[43]; g_last_p = state; }
static void WINAPI fake_vp(void *self, UINT count, const void *viewports) { (void)self; ++g_calls[44]; g_last_a = count; g_last_p = (void *)viewports; }
static void WINAPI fake_sc(void *self, UINT count, const void *rects) { (void)self; ++g_calls[45]; g_last_a = count; g_last_p = (void *)rects; }
static void WINAPI fake_ps(void *self, void *shader, void *const *instances, UINT count) { (void)self; (void)instances; ++g_calls[9]; g_last_p = shader; g_last_a = count; }
static void WINAPI fake_srv(void *self, UINT start, UINT count, void *const *views) { (void)self; (void)views; ++g_calls[8]; g_last_a = start; g_last_b = count; }
static void WINAPI fake_clr(void *self, void *view, const float *color) { (void)self; (void)color; ++g_calls[50]; g_last_p = view; }
static void WINAPI fake_cld(void *self, void *view, UINT flags, float depth, BYTE stencil) { (void)self; ++g_calls[53]; g_last_p = view; g_last_a = flags; g_last_depth = depth; g_last_stencil = stencil; }
static HRESULT g_map_hr = S_OK;
static UINT g_map_type, g_map_flags, g_map_sub;
static HRESULT WINAPI fake_map(void *self, void *resource, UINT sub, UINT type, UINT flags, void *mapped) { (void)self; (void)resource; (void)mapped; ++g_calls[14]; g_map_sub = sub; g_map_type = type; g_map_flags = flags; return g_map_hr; }
static void WINAPI fake_other(void *self) { (void)self; }
static void WINAPI fake_il(void *self, void *layout) { (void)self; (void)layout; }
static UINT g_vb_stride_seen, g_vb_binds;
static void WINAPI fake_vb(void *self, UINT start, UINT count, void *const *buffers, const UINT *strides, const UINT *offsets) { (void)self; (void)start; (void)count; (void)buffers; (void)offsets; ++g_vb_binds; g_vb_stride_seen = strides ? strides[0] : 0xDEADu; }

/* a state object: vtable slot 7 = GetDesc */
typedef struct FakeState { void **vtbl; BYTE desc[300]; UINT desc_size; BOOL fault; } FakeState;
static FakeState *g_cur_state;
static void WINAPI fake_getdesc(void *self, void *desc)
{
    FakeState *state = (FakeState *)self;
    if (state->fault) RaiseException(0xC0000005u, 0, 0, NULL);
    memcpy(desc, state->desc, state->desc_size);
}
static void *g_state_vtbl[8] = { (void *)fake_other, (void *)fake_other, (void *)fake_other, (void *)fake_other, (void *)fake_other, (void *)fake_other, (void *)fake_other, (void *)fake_getdesc };

static FakeState *make_blend_state(BOOL enable, UINT src, UINT dst)
{
    x104_panel_tr_blend_t d;
    FakeState *state = (FakeState *)test_calloc(1, sizeof(FakeState));
    memset(&d, 0, sizeof(d));
    d.rt[0].BlendEnable = enable; d.rt[0].SrcBlend = src; d.rt[0].DestBlend = dst; d.rt[0].WriteMask = 15;
    state->vtbl = g_state_vtbl; memcpy(state->desc, &d, sizeof(d)); state->desc_size = sizeof(d);
    return state;
}
static FakeState *make_depth_state(BOOL enable, UINT write, UINT func)
{
    x104_panel_tr_depth_t d;
    FakeState *state = (FakeState *)test_calloc(1, sizeof(FakeState));
    memset(&d, 0, sizeof(d));
    d.DepthEnable = enable; d.DepthWriteMask = write; d.DepthFunc = func;
    state->vtbl = g_state_vtbl; memcpy(state->desc, &d, sizeof(d)); state->desc_size = sizeof(d);
    return state;
}
static FakeState *make_raster_state(UINT cull, BOOL scissor)
{
    x104_panel_tr_raster_t d;
    FakeState *state = (FakeState *)test_calloc(1, sizeof(FakeState));
    memset(&d, 0, sizeof(d));
    d.FillMode = 3; d.CullMode = cull; d.ScissorEnable = scissor;
    state->vtbl = g_state_vtbl; memcpy(state->desc, &d, sizeof(d)); state->desc_size = sizeof(d);
    return state;
}

static HRESULT g_cf_hr = S_OK;
static int g_cf_calls;
static void *g_cf_a, *g_cf_c;
static const void *g_cf_b;
static SIZE_T g_cf_len;
static HRESULT WINAPI fake_cf_view(void *self, void *resource, const void *desc, void **out) { (void)self; ++g_cf_calls; g_cf_a = resource; g_cf_b = desc; g_cf_c = (void *)out; return g_cf_hr; }
static HRESULT WINAPI fake_cf_state(void *self, const void *desc, void **out) { (void)self; ++g_cf_calls; g_cf_b = desc; g_cf_c = (void *)out; return g_cf_hr; }
static HRESULT WINAPI fake_cf_shader(void *self, const void *code, SIZE_T length, void *linkage, void **out) { (void)self; (void)linkage; ++g_cf_calls; g_cf_b = code; g_cf_len = length; g_cf_c = (void *)out; return g_cf_hr; }
static HRESULT WINAPI fake_cf_layout(void *self, const void *elements, UINT count, const void *code, SIZE_T length, void **out) { (void)self; (void)elements; (void)count; ++g_cf_calls; g_cf_b = code; g_cf_len = length; g_cf_c = (void *)out; return g_cf_hr; }
typedef struct FakeContext { void **vtbl; } FakeContext;
static void *g_ctx_vtbl[X104_PANEL_TR_SLOTS + 1];

int main(void)
{
    FakeContext context;
    void **table = g_ctx_vtbl + 1;                         /* a COM vtable pointer points at slot 0 */
    int slot;
    FakeState *blend_off, *blend_on, *depth_on, *depth_off, *raster, *bad;
    x104_panel_tr_viewport_t viewport = { 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    x104_panel_tr_rect_t scissor = { 10, 20, 110, 220 };
    void *views[1] = { (void *)0x1000 };
    void (WINAPI **vt)();
    void *saved_cells[X104_PANEL_TR_SLOTS];

    test_print("== panel tracer test ==\n");
    x104.image = (BYTE *)test_calloc(1, 0x1000);
    for (slot = 0; slot < X104_PANEL_TR_SLOTS; ++slot) table[slot] = (void *)fake_other;
    table[18] = (void *)fake_vb; table[17] = (void *)fake_il; table[8] = (void *)fake_srv; table[9] = (void *)fake_ps; table[12] = (void *)fake_di; table[13] = (void *)fake_draw; table[14] = (void *)fake_map; table[33] = (void *)fake_omrt; table[35] = (void *)fake_bs;
    table[36] = (void *)fake_dss; table[43] = (void *)fake_rs; table[44] = (void *)fake_vp; table[45] = (void *)fake_sc; table[50] = (void *)fake_clr; table[53] = (void *)fake_cld;
    memcpy(saved_cells, table, sizeof(saved_cells));
    context.vtbl = table;
    vt = (void (WINAPI **)())table;

    CHECK(x104_panel_tr.enabled == FALSE);
    x104_panel_tr_parse();
    CHECK(x104_panel_tr.enabled == FALSE);                 /* the test process has no --trace-game-state */

    x104_panel_tr_install_vtable(&context, "test context");
    CHECK(x104_panel_tr.installed && x104_panel_tr.vt_count == 1);
    CHECK(log_has("21 of 21 cells wrapped"));
    for (slot = 0; slot < X104_PANEL_TR_SLOTS; ++slot) {
        BOOL wrapped = (slot == 7 || slot == 11 || slot == 16 || slot == 17 || slot == 18 || slot == 19 || slot == 8 || slot == 9 || slot == 12 || slot == 13 || slot == 20 || slot == 21 || slot == 14 || slot == 33 || slot == 35 || slot == 36 || slot == 43 || slot == 44 || slot == 45 || slot == 50 || slot == 53);
        CHECK(wrapped ? (table[slot] != saved_cells[slot]) : (table[slot] == saved_cells[slot]));
    }
    x104_panel_tr_install_vtable(&context, "again");        /* idempotent */
    CHECK(x104_panel_tr.vt_count == 1 && log_has("already traced"));

    /* stride fixer: a layout of {POSITION fmt6 off0, COLOR fmt28 off12, TEXCOORD fmt37 off16} needs 20 bytes; the game binds its buffer with stride 0 */
    {
        DWORD elements[21] = { 0, 0, 6, 0, 0, 0, 0,   0, 0, 28, 0, 12, 0, 0,   0, 0, 37, 0, 16, 0, 0 };
        void *layout = (void *)0x1234;
        void *buffer = (void *)0x4321;
        UINT zero = 0, stride_fixed = 16, offset0 = 0;
        void *buffers[1];
        buffers[0] = buffer;
        x104_panel_tr_parse();
        CHECK(x104_panel_tr.fix_vb == TRUE);
        x104_panel_ls_add(layout, elements, 3);
        CHECK(x104_panel_ls_find(layout) == 20u);
        x104_panel_ls_add((void *)0x5555, elements, 0);          /* nothing */
        CHECK(x104_panel_ls_find((void *)0x5555) == 0u);
        ((void (WINAPI *)(void *, UINT, UINT, void *const *, const UINT *, const UINT *))table[18])(&context, 0, 1, buffers, &zero, &offset0);
        ((void (WINAPI *)(void *, void *))table[17])(&context, layout);
        g_vb_binds = 0;
        ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 6, 0, 0);
        CHECK(g_vb_binds == 1 && g_vb_stride_seen == 20u);       /* re-bound with the layout's stride right before the draw */
        CHECK(x104_panel_tr.stride_fixes == 1);
        ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 6, 0, 0);
        CHECK(g_vb_binds == 1);                                   /* already fixed: no second re-bind */
        buffers[0] = buffer;
        ((void (WINAPI *)(void *, UINT, UINT, void *const *, const UINT *, const UINT *))table[18])(&context, 0, 1, buffers, &stride_fixed, &offset0);
        ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 6, 0, 0);
        CHECK(g_vb_binds == 2 && g_vb_stride_seen == 16u);        /* a non-zero stride from the game is never touched (the bind itself passed through) */
        CHECK(x104_panel_tr.stride_fixes == 1);
        /* leave the frame record clean for the checks below */
        x104_panel_tr.draws = x104_panel_tr.indexed = x104_panel_tr.cls_w = x104_panel_tr.cls_b = x104_panel_tr.cls_u_ui = x104_panel_tr.cls_u_flat = 0;
        x104_panel_tr.ring_pos = 0;
        x104_panel_tr.nruns = x104_panel_tr.run_overflow = 0;
        g_calls[12] = 0;
        x104_panel_tr.vb0 = NULL; x104_panel_tr.layout = NULL; x104_panel_tr.vstride = 0; x104_panel_tr.vb_count = 0;
    }

    blend_off = make_blend_state(FALSE, 1, 0);
    blend_on = make_blend_state(TRUE, 5, 6);
    depth_on = make_depth_state(TRUE, 1, 2);
    depth_off = make_depth_state(FALSE, 0, 8);
    raster = make_raster_state(3, TRUE);
    bad = make_blend_state(TRUE, 1, 1);
    bad->fault = TRUE;

    /* one fake frame: clears, a world pass, a blended world draw, a UI pass, plus a faulting GetDesc */
    ((void (WINAPI *)(void *, void *, const float *))table[50])(&context, (void *)0x1111, NULL);
    CHECK(g_calls[50] == 1 && g_last_p == (void *)0x1111);
    ((void (WINAPI *)(void *, void *, UINT, float, BYTE))table[53])(&context, (void *)0x2222, 3, 1.0f, 7);
    CHECK(g_calls[53] == 1 && g_last_p == (void *)0x2222 && g_last_a == 3 && g_last_depth == 1.0f && g_last_stencil == 7);
    ((void (WINAPI *)(void *, UINT, void *const *, void *))table[33])(&context, 1, views, (void *)0x3333);
    CHECK(g_calls[33] == 1 && g_last_p == (void *)0x3333);
    ((void (WINAPI *)(void *, UINT, const x104_panel_tr_viewport_t *))table[44])(&context, 1, &viewport);
    CHECK(g_calls[44] == 1 && g_last_p == (void *)&viewport);
    ((void (WINAPI *)(void *, UINT, const x104_panel_tr_rect_t *))table[45])(&context, 1, &scissor);
    ((void (WINAPI *)(void *, void *))table[43])(&context, raster);
    ((void (WINAPI *)(void *, void *, UINT))table[36])(&context, depth_on, 0);
    ((void (WINAPI *)(void *, void *, const float *, UINT))table[35])(&context, blend_off, NULL, 0xFFFFFFFFu);
    CHECK(g_calls[35] == 1 && g_last_p == blend_off && g_last_a == 0xFFFFFFFFu);
    ((void (WINAPI *)(void *, void *, void *const *, UINT))table[9])(&context, (void *)0x4444, NULL, 0);
    ((void (WINAPI *)(void *, UINT, UINT, void *const *))table[8])(&context, 0, 1, views);
    ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 36, 12, -5);          /* W */
    CHECK(g_calls[12] == 1 && g_last_a == 36 && g_last_b == 12 && g_last_c == (UINT)-5);
    ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 6, 0, 0);             /* W */
    ((void (WINAPI *)(void *, void *, const float *, UINT))table[35])(&context, blend_on, NULL, 0xFFFFFFFFu);
    ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 12, 0, 0);            /* B */
    ((void (WINAPI *)(void *, void *, UINT))table[36])(&context, depth_off, 0);
    ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 6, 0, 0);             /* U */
    ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 6, 6, 0);             /* U */
    ((void (WINAPI *)(void *, UINT, UINT))table[13])(&context, 3, 0);                     /* U via Draw */
    ((void (WINAPI *)(void *, void *, const float *, UINT))table[35])(&context, bad, NULL, 0); /* GetDesc faults: logged-free fallback, the call is still forwarded */
    CHECK(g_calls[35] == 3 && g_last_p == bad);
    ((void (WINAPI *)(void *, void *, const float *, UINT))table[35])(&context, NULL, NULL, 0);   /* NULL = default (blend off) */
    ((void (WINAPI *)(void *, void *, UINT))table[36])(&context, NULL, 0);                         /* NULL = default (depth on) */
    ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 9, 0, 0);             /* W */

    CHECK(x104_panel_tr.draws == 7 && x104_panel_tr.indexed == 6);
    CHECK(x104_panel_tr.cls_w == 3 && x104_panel_tr.cls_b == 1 && x104_panel_tr.cls_u_ui == 3 && x104_panel_tr.cls_u_flat == 0);
    CHECK(x104_panel_tr.clears_rt == 1 && x104_panel_tr.clears_ds == 1);
    CHECK(x104_panel_tr.vp_w == 1280 && x104_panel_tr.vp_h == 720 && x104_panel_tr.sc_w == 100 && x104_panel_tr.sc_h == 200);
    CHECK(x104_panel_tr.rs_cull == 3 && x104_panel_tr.rs_sc == 1);
    CHECK(x104_panel_tr.ps == (void *)0x4444 && x104_panel_tr.srv0 == (void *)0x1000);
    CHECK(x104_panel_tr.nruns == 6);
    CHECK(x104_panel_tr.run_cls[0] == 'C' && x104_panel_tr.run_cls[1] == 'D' && x104_panel_tr.run_cls[2] == 'W' && x104_panel_tr.run_n[2] == 2 && x104_panel_tr.run_cls[3] == 'B' &&
          x104_panel_tr.run_cls[4] == 'U' && x104_panel_tr.run_n[4] == 3 && x104_panel_tr.run_cls[5] == 'W');

    /* Map: forwarded, counted by type, failures logged (once the budget allows) */
    {
        HRESULT mhr;
        mhr = ((HRESULT (WINAPI *)(void *, void *, UINT, UINT, UINT, void *))table[14])(&context, (void *)0x5555, 2, 4, 0, NULL);
        CHECK(mhr == S_OK && g_calls[14] == 1 && g_map_sub == 2 && g_map_type == 4 && g_map_flags == 0);
        mhr = ((HRESULT (WINAPI *)(void *, void *, UINT, UINT, UINT, void *))table[14])(&context, (void *)0x5555, 0, 5, 0x100, NULL);
        CHECK(mhr == S_OK && g_map_type == 5 && g_map_flags == 0x100);
        g_map_hr = (HRESULT)0x80070057u;
        mhr = ((HRESULT (WINAPI *)(void *, void *, UINT, UINT, UINT, void *))table[14])(&context, (void *)0x6666, 0, 5, 0, NULL);
        g_map_hr = S_OK;
        CHECK(mhr == (HRESULT)0x80070057u);
        CHECK(x104_panel_tr.maps == 3 && x104_panel_tr.maps_discard == 1 && x104_panel_tr.maps_nooverwrite == 2 && x104_panel_tr.maps_failed == 1);
        CHECK(log_has("Map FAILED hr=0x80070057") && log_has("resource=00006666") || log_has("Map FAILED hr=0x80070057"));
    }

    /* the frame log */
    g_log_len = 0; g_log[0] = 0;
    x104_panel_tr_log_frame("test", GetTickCount());
    CHECK(log_has("X104 d3dtrace frame #0 [test]") && log_has("draws=7 indexed=6 W=3 B=1 U=3 u=0 clearRT=1 clearDS=1"));
    CHECK(log_has("maps=3 (discard 1, no-overwrite 2, FAILED 1)"));
    CHECK(log_has("passes: C1 D1 W2 B1 U3 W1 "));
    CHECK(log_has("draw[-7]") && log_has("draw[-1]"));
    CHECK(log_has("blend=1(src5 dst6 maskf)"));
    CHECK(log_has("depth=0(write0 func8 stencil0)") && log_has("depth=1(write1 func2 stencil0)"));

    /* frame_end: first call arms the schedule and resets the record; a U flip logs */
    g_log_len = 0; g_log[0] = 0;
    x104_panel_tr.frames = 0;
    x104_panel_tr.enabled = TRUE;                       /* frame_end only logs / dumps while --trace-game-state is on (the fixer hooks run without it) */
    x104_panel_tr_frame_end();
    CHECK(x104_panel_tr.draws == 0 && x104_panel_tr.nruns == 0 && x104_panel_tr.ring_pos == 0);
    CHECK(log_has("[periodic]"));                          /* the first frame logs at once */
    {
        int index;
        for (index = 0; index < 5; ++index) {              /* frames without U */
            ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 9, 0, 0);
            x104_panel_tr_frame_end();
        }
        g_log_len = 0; g_log[0] = 0;
        ((void (WINAPI *)(void *, void *, UINT))table[36])(&context, depth_off, 0);
        ((void (WINAPI *)(void *, void *, const float *, UINT))table[35])(&context, blend_on, NULL, 0);
        ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 6, 0, 0);
        x104_panel_tr_frame_end();
        CHECK(log_has("[U-appeared]"));
        g_log_len = 0; g_log[0] = 0;
        ((void (WINAPI *)(void *, void *, UINT))table[36])(&context, depth_on, 0);
        ((void (WINAPI *)(void *, void *, const float *, UINT))table[35])(&context, blend_off, NULL, 0);
        ((void (WINAPI *)(void *, UINT, UINT, INT))table[12])(&context, 6, 0, 0);
        x104_panel_tr_frame_end();
        CHECK(log_has("[U-vanished]"));
    }
    (void)vt;
    CHECK(log_has("end state: rtv="));

    /* ---- the always-on Create* failure wrappers ---- */
    {
        static void *device_table_storage[X104_PANEL_CF_SLOTS + 1];
        void **device_table = device_table_storage + 1;
        FakeContext device;
        int calls_before;
        HRESULT hr;
        UINT desc_words[16];
        x104_panel_tr_blend_t blend_desc;
        x104_panel_tr_depth_t depth_desc;
        x104_panel_tr_raster_t raster_desc;
        x104_panel_cf_sampler_t sampler_desc;
        for (slot = 0; slot < X104_PANEL_CF_SLOTS; ++slot) device_table[slot] = (void *)fake_other;
        device_table[7] = (void *)fake_cf_view; device_table[9] = (void *)fake_cf_view; device_table[10] = (void *)fake_cf_view;
        device_table[11] = (void *)fake_cf_layout; device_table[12] = (void *)fake_cf_shader; device_table[15] = (void *)fake_cf_shader;
        device_table[20] = (void *)fake_cf_state; device_table[21] = (void *)fake_cf_state; device_table[22] = (void *)fake_cf_state; device_table[23] = (void *)fake_cf_state;
        device.vtbl = device_table;
        x104_panel_cf_install(&device, "fake device");
        CHECK(x104_panel_cf.count == 1 && log_has("10 of 10 Create* failure-logging wrappers installed"));
        x104_panel_cf_install(&device, "fake device again");
        CHECK(x104_panel_cf.count == 1);
        CHECK(device_table[7] != (void *)fake_cf_view && device_table[23] != (void *)fake_cf_state && device_table[8] == (void *)fake_other);

        memset(desc_words, 0, sizeof(desc_words)); desc_words[0] = 28; desc_words[1] = 4;
        g_cf_hr = S_OK; g_log_len = 0; g_log[0] = 0; calls_before = g_cf_calls;
        hr = ((HRESULT (WINAPI *)(void *, void *, const void *, void **))device_table[7])(&device, (void *)0x77, desc_words, (void **)0x88);
        CHECK(hr == S_OK && g_cf_calls == calls_before + 1 && g_cf_a == 0x77 && g_cf_b == desc_words && g_cf_c == (void *)0x88);
        CHECK(g_log_len == 0);                                                     /* success: silent */
        g_cf_hr = (HRESULT)0x80070057u;
        hr = ((HRESULT (WINAPI *)(void *, void *, const void *, void **))device_table[7])(&device, (void *)0x77, desc_words, (void **)0x88);
        CHECK(hr == (HRESULT)0x80070057u && log_has("CreateShaderResourceView FAILED hr=0x80070057") && log_has("format=28 dimension=4"));
        hr = ((HRESULT (WINAPI *)(void *, void *, const void *, void **))device_table[9])(&device, (void *)0x77, desc_words, (void **)0x88);
        CHECK(log_has("CreateRenderTargetView FAILED"));
        hr = ((HRESULT (WINAPI *)(void *, void *, const void *, void **))device_table[10])(&device, (void *)0x77, desc_words, (void **)0x88);
        CHECK(log_has("CreateDepthStencilView FAILED"));
        hr = ((HRESULT (WINAPI *)(void *, const void *, UINT, const void *, SIZE_T, void **))device_table[11])(&device, desc_words, 3, desc_words, 99, (void **)0x99);
        CHECK(hr == (HRESULT)0x80070057u && log_has("CreateInputLayout FAILED") && log_has("elements=3") && log_has("(99 bytes)") && g_cf_len == 99);
        hr = ((HRESULT (WINAPI *)(void *, const void *, SIZE_T, void *, void **))device_table[12])(&device, desc_words, 123, NULL, (void **)0x99);
        CHECK(log_has("CreateVertexShader FAILED") && log_has("(123 bytes)") && g_cf_len == 123);
        hr = ((HRESULT (WINAPI *)(void *, const void *, SIZE_T, void *, void **))device_table[15])(&device, desc_words, 321, NULL, (void **)0x99);
        CHECK(log_has("CreatePixelShader FAILED") && log_has("(321 bytes)"));
        memset(&blend_desc, 0, sizeof(blend_desc)); blend_desc.rt[0].BlendEnable = 1; blend_desc.rt[0].SrcBlend = 5; blend_desc.rt[0].DestBlend = 6; blend_desc.rt[0].WriteMask = 15;
        hr = ((HRESULT (WINAPI *)(void *, const void *, void **))device_table[20])(&device, &blend_desc, (void **)0x99);
        CHECK(log_has("CreateBlendState FAILED") && log_has("rt0: blend=1 src=5 dst=6") && g_cf_b == &blend_desc);
        memset(&depth_desc, 0, sizeof(depth_desc)); depth_desc.DepthEnable = 1; depth_desc.DepthWriteMask = 1; depth_desc.DepthFunc = 4;
        hr = ((HRESULT (WINAPI *)(void *, const void *, void **))device_table[21])(&device, &depth_desc, (void **)0x99);
        CHECK(log_has("CreateDepthStencilState FAILED") && log_has("depth=1 write=1 func=4"));
        memset(&raster_desc, 0, sizeof(raster_desc)); raster_desc.FillMode = 3; raster_desc.CullMode = 2; raster_desc.DepthClipEnable = 0; raster_desc.SlopeScaledDepthBias = 1.5f; raster_desc.ScissorEnable = 1;
        hr = ((HRESULT (WINAPI *)(void *, const void *, void **))device_table[22])(&device, &raster_desc, (void **)0x99);
        CHECK(log_has("CreateRasterizerState FAILED") && log_has("fill=3 cull=2") && log_has("slopeBias=1500m depthClip=0 scissor=1"));
        memset(&sampler_desc, 0, sizeof(sampler_desc)); sampler_desc.Filter = 21; sampler_desc.AddressU = 3; sampler_desc.MaxAnisotropy = 16; sampler_desc.MaxLOD = 3.402823466e+38f;
        hr = ((HRESULT (WINAPI *)(void *, const void *, void **))device_table[23])(&device, &sampler_desc, (void **)0x99);
        CHECK(log_has("CreateSamplerState FAILED") && log_has("filter=21 address=3/0/0") && log_has("maxAniso=16"));
        hr = ((HRESULT (WINAPI *)(void *, const void *, void **))device_table[22])(&device, (const void *)0x10, (void **)0x99);      /* unreadable desc: no fault */
        CHECK(hr == (HRESULT)0x80070057u && log_has("(desc 00000010 unreadable)"));
        g_cf_hr = S_OK;
        (void)desc_words;
    }

    test_print("== panel tracer test: %d checks, %d failures %s ==\n", g_checks, g_failures, g_failures ? "FAILED" : "OK");
    if (g_failures) test_print("---- log ----\n%s\n", g_log);
    return g_failures ? 1 : 0;
}
