#include "win32_compat.h"

#define WIN32CRAFT_E_BOUNDS ((HRESULT)0x8000000BL)
#define WIN32CRAFT_RO_E_METADATA_NAME_NOT_FOUND ((HRESULT)0x8000000FL)
#define WIN32CRAFT_S_FALSE ((HRESULT)1L)
#define WIN32CRAFT_ERROR_DEVICE_NOT_CONNECTED 1167UL

DWORD WINAPI Win32CraftJsStartDebugging(
    void *runtime, void *callback, void *callback_state)
{
    (void)runtime;
    (void)callback;
    (void)callback_state;
    return 0;
}

HMODULE WINAPI Win32CraftLoadPackagedLibrary(LPCWSTR path, DWORD reserved)
{
    (void)reserved;
    return LoadLibraryW(path);
}

int _fltused;

typedef struct D3CraftMultiQi {
    const GUID *iid;
    void *interface_pointer;
    HRESULT result;
} D3CraftMultiQi;

static void *resolve_ole32(const char *name)
{
    static HMODULE module;
    if (!module) module = LoadLibraryW(L"ole32.dll");
    return module ? GetProcAddress(module, name) : NULL;
}

HRESULT WINAPI Win32CraftCoCreateFreeThreadedMarshaler(void *outer, void **value)
{
    typedef HRESULT (WINAPI *Fn)(void *, void **);
    Fn function = (Fn)resolve_ole32("CoCreateFreeThreadedMarshaler");
    if (!value) return E_POINTER;
    *value = NULL;
    return function ? function(outer, value) : E_NOTIMPL;
}

HRESULT WINAPI Win32CraftCoCreateGuid(GUID *guid)
{
    typedef HRESULT (WINAPI *Fn)(GUID *);
    Fn function = (Fn)resolve_ole32("CoCreateGuid");
    return !guid ? E_POINTER : function ? function(guid) : E_NOTIMPL;
}

void *WINAPI Win32CraftCoTaskMemAlloc(SIZE_T size)
{
    typedef void *(WINAPI *Fn)(SIZE_T);
    Fn function = (Fn)resolve_ole32("CoTaskMemAlloc");
    return function ? function(size) : NULL;
}

void WINAPI Win32CraftCoTaskMemFree(void *memory)
{
    typedef void (WINAPI *Fn)(void *);
    Fn function = (Fn)resolve_ole32("CoTaskMemFree");
    if (function) function(memory);
}

HRESULT WINAPI Win32CraftIIDFromString(const WCHAR *text, GUID *iid)
{
    typedef HRESULT (WINAPI *Fn)(const WCHAR *, GUID *);
    Fn function = (Fn)resolve_ole32("IIDFromString");
    return !iid ? E_POINTER : function ? function(text, iid) : E_NOTIMPL;
}

INT WINAPI Win32CraftStringFromGUID2(const GUID *guid, WCHAR *text, INT count)
{
    typedef INT (WINAPI *Fn)(const GUID *, WCHAR *, INT);
    Fn function = (Fn)resolve_ole32("StringFromGUID2");
    return !guid || !text || count <= 0 ? 0
        : function ? function(guid, text, count) : 0;
}

HRESULT WINAPI Win32CraftCoCreateInstanceFromApp(
    const GUID *class_id, void *outer, DWORD context,
#ifdef WIN32CRAFT_ARM32
    void *reserved,   /* the real prototype has 6 parameters (docs + exe call at fmod/exe 0x9561BA: r3 = 0 reserved, [sp] = count, [sp+4] = MULTI_QI*) */
#endif
    ULONG query_count, D3CraftMultiQi *queries)
{
#ifdef WIN32CRAFT_ARM32
    (void)reserved;
#endif
    typedef HRESULT (WINAPI *Fn)(const GUID *, void *, DWORD,
                                 const GUID *, void **);
    Fn function = (Fn)resolve_ole32("CoCreateInstance");
    HRESULT result = S_OK;
    ULONG index;
    if (!queries || !query_count) return E_INVALIDARG;
    for (index = 0; index < query_count; ++index) {
        queries[index].interface_pointer = NULL;
        queries[index].result = function && queries[index].iid
            ? function(class_id, outer, context, queries[index].iid,
                       &queries[index].interface_pointer)
            : REGDB_E_CLASSNOTREG;
        if (FAILED(queries[index].result)) result = queries[index].result;
    }
    return result;
}

HRESULT WINAPI Win32CraftCoGetContextToken(ULONG_PTR *token)
{
#ifdef WIN32CRAFT_ARM32
    {
        typedef HRESULT (WINAPI *NativeFn)(ULONG_PTR *);
        NativeFn native = (NativeFn)resolve_ole32("CoGetContextToken");
        if (native && token) return native(token);
    }
#endif

    if (!token) return E_POINTER;
    *token = 1;
    return S_OK;
}

ULONG WINAPI Win32CraftEventRegister(
    const GUID *provider, void *callback, void *context, ULONGLONG *handle)
{
    (void)provider; (void)callback; (void)context;
    if (handle) *handle = 1;
    return 0;
}

ULONG WINAPI Win32CraftEventSetInformation(
    ULONGLONG handle, INT type, const void *data, ULONG size)
{ (void)handle; (void)type; (void)data; (void)size; return 0; }
ULONG WINAPI Win32CraftEventUnregister(ULONGLONG handle)
{ (void)handle; return 0; }
ULONG WINAPI Win32CraftEventWriteTransfer(
    ULONGLONG handle, const void *descriptor, const GUID *activity,
    const GUID *related, ULONG count, const void *data)
{ (void)handle; (void)descriptor; (void)activity; (void)related;
  (void)count; (void)data; return 0; }

typedef struct Win32CraftHString {
    DWORD magic;
    DWORD flags;
    UINT32 length;
    const WCHAR *text;
} Win32CraftHString;

typedef struct Win32CraftHStringHeader {
    Win32CraftHString value;
} Win32CraftHStringHeader;

typedef struct Win32CraftHStringBuffer {
    Win32CraftHString value;
    DWORD reserved;
    WCHAR text[1];
} Win32CraftHStringBuffer;

static const WCHAR empty_string[] = L"";
#define WIN32CRAFT_HSTRING_MAGIC 0x48535452UL
#define WIN32CRAFT_HSTRING_OWNED 1UL
#ifdef WIN32CRAFT_ARM32
/* ---- ARM32: forward every HSTRING / Ro* / error-info export to the genuine combase.dll ----
 * (facts pe_imports.hstring_world + fmod.hstring_world, PORT_RULES #8).  On ARM the desktop vccorlib140.dll and
 * fmod_ARM.dll use REAL HSTRINGs, so the image-size gate of the x86 build (0x02db1000 = 1.16 only) does not apply:
 * the shim below is used only for exports combase really lacks (never the case on Windows 8.1 RT / Windows 10). */

/* Cross-unit hook (assigned by unit iat_hooks).  Return S_OK with a non-NULL *result to claim the class; any other
 * result makes RoGetActivationFactory fall back to the real combase one.  Lets fmod_ARM.dll's WinRT activation
 * (IAT cell RVA 0xB118C := RoGetActivationFactory below) reach the host fakes. */
typedef HRESULT (*Win32CraftActivationFn)(LPCWSTR class_name, const GUID *iid, void **result);
Win32CraftActivationFn Win32CraftActivationDispatcher;

/* The log is shared with win32_host.c (same file, same WIN32CRAFT_DEBUG gate); api_compat.c has no access to the
 * static log_line, so it appends to ".\win32craft.log" itself (FILE_APPEND_DATA = 4, share read|write = 3,
 * OPEN_ALWAYS = 4, FILE_ATTRIBUTE_NORMAL = 0x80). */
extern wchar_t Win32CraftLogPathW[];   /* win32_host.c: absolute path of the file its log_line appends to ("" before the first line) */
extern UINT Win32CraftLogPrefix(char *out, UINT capacity);   /* win32_host.c: "[  1234 ms T0a3c] " (shared clock + thread id) */
static void compat_log(const char *text)
{
    char line[640];
    char debug_value[8];
    DWORD length = 0, written = 0, debug_length;
    HANDLE file;
    if (!text) return;
    /* same switch as the ARM32 host log_line: on unless WIN32CRAFT_DEBUG=0 */
    debug_length = GetEnvironmentVariableA("WIN32CRAFT_DEBUG", debug_value, sizeof(debug_value));
    if (debug_length == 1 && debug_value[0] == '0') return;
    {
        UINT copied = 0;
        length = Win32CraftLogPrefix(line, sizeof(line));
        while (text[copied] && length < sizeof(line) - 3) line[length++] = text[copied++];
    }
    line[length++] = '\r';
    line[length++] = '\n';
    file = CreateFileW(Win32CraftLogPathW[0] ? Win32CraftLogPathW : L"win32craft.log", 4, 3, NULL, 4, 0x80, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        WriteFile(file, line, length, &written, NULL);
        CloseHandle(file);
    }
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
}

typedef struct Win32CraftNativeExport {
    const char *name;
    void *proc;
} Win32CraftNativeExport;

/* Every export of api_compat.c that has a genuine counterpart in combase.dll (RoGetBufferMarshaler lives in
 * wintypes.dll, the robuffer api-set host).  RoInitialize/RoUninitialize resolve combase themselves. */
static Win32CraftNativeExport native_exports[] = {
    {"WindowsCreateString", NULL}, {"WindowsCreateStringReference", NULL},
    {"WindowsDeleteString", NULL}, {"WindowsDuplicateString", NULL},
    {"WindowsGetStringLen", NULL}, {"WindowsGetStringRawBuffer", NULL},
    {"WindowsIsStringEmpty", NULL}, {"WindowsCompareStringOrdinal", NULL},
    {"WindowsConcatString", NULL}, {"WindowsSubstring", NULL},
    {"WindowsSubstringWithSpecifiedLength", NULL},
    {"WindowsStringHasEmbeddedNull", NULL}, {"WindowsReplaceString", NULL},
    {"WindowsTrimStringStart", NULL}, {"WindowsTrimStringEnd", NULL},
    {"WindowsPreallocateStringBuffer", NULL},
    {"WindowsPromoteStringBuffer", NULL}, {"WindowsDeleteStringBuffer", NULL},
    {"RoActivateInstance", NULL}, {"RoGetActivationFactory", NULL},
    {"RoGetApartmentIdentifier", NULL}, {"RoGetBufferMarshaler", NULL},
    {"RoGetParameterizedTypeInstanceIID", NULL},
    {"RoFreeParameterizedTypeExtra", NULL},
    {"RoRegisterActivationFactories", NULL},
    {"RoRevokeActivationFactories", NULL},
    {"RoRegisterForApartmentShutdown", NULL},
    {"RoUnregisterForApartmentShutdown", NULL},
    {"GetRestrictedErrorInfo", NULL}, {"SetRestrictedErrorInfo", NULL},
    {"RoOriginateError", NULL}, {"RoOriginateErrorW", NULL},
    {"RoTransformError", NULL}, {"RoCaptureErrorContext", NULL},
    {"RoFailFastWithErrorContext", NULL}, {"RoGetErrorReportingFlags", NULL},
    {"RoSetErrorReportingFlags", NULL}, {"RoReportFailedDelegate", NULL},
    {"RoReportUnhandledError", NULL}
};
#define NATIVE_EXPORT_COUNT ((UINT)ARRAYSIZE(native_exports))

static volatile LONG native_state; /* 0 = not resolved, 1 = resolving, 2 = ready */
static void native_hstring_self_test(void);

static BOOL native_name_equals(const char *left, const char *right)
{
    while (*left && *left == *right) { ++left; ++right; }
    return *left == *right;
}

static BOOL native_name_has_prefix(const char *name, const char *prefix)
{
    while (*prefix && *name == *prefix) { ++name; ++prefix; }
    return *prefix == 0;
}

static void native_init_exports(void)
{
    MEMORY_BASIC_INFORMATION own, found;
    HMODULE combase, wintypes = NULL;
    BOOL wintypes_tried = FALSE;
    UINT index, resolved = 0, missing = 0, missing_length = 0;
    char missing_list[420];
    char line[640];

    missing_list[0] = 0;
    ZeroMemory(&own, sizeof(own));
    VirtualQuery((LPCVOID)native_init_exports, &own, sizeof(own));
    combase = LoadLibraryW(L"combase.dll");
    for (index = 0; index < NATIVE_EXPORT_COUNT; ++index) {
        void *proc = combase ? GetProcAddress(combase, native_exports[index].name) : NULL;
        if (!proc) {
            if (!wintypes_tried) {
                wintypes_tried = TRUE;
                wintypes = LoadLibraryW(L"wintypes.dll");
            }
            proc = wintypes ? GetProcAddress(wintypes, native_exports[index].name) : NULL;
        }
        if (proc) {
            /* never forward into ourselves (a forwarder loop would recurse forever) */
            ZeroMemory(&found, sizeof(found));
            if (VirtualQuery(proc, &found, sizeof(found)) &&
                own.AllocationBase && found.AllocationBase == own.AllocationBase) {
                wsprintfA(line, "api_compat ARM32: native %s resolves into win32craft.dll itself - ignored",
                          native_exports[index].name);
                compat_log(line);
                proc = NULL;
            }
        }
        native_exports[index].proc = proc;
        if (proc) {
            ++resolved;
        } else {
            ++missing;
            if (missing_length < sizeof(missing_list) - 48) {
                const char *text = native_exports[index].name;
                while (*text) missing_list[missing_length++] = *text++;
                missing_list[missing_length++] = ' ';
                missing_list[missing_length] = 0;
            }
        }
    }
    /* A HSTRING is either entirely native or entirely shim: without a native WindowsCreateString every
     * Windows* export must use the shim (their handles are not interchangeable). */
    if (!native_exports[0].proc) { /* [0] is WindowsCreateString */
        for (index = 0; index < NATIVE_EXPORT_COUNT; ++index)
            if (native_name_has_prefix(native_exports[index].name, "Windows"))
                native_exports[index].proc = NULL;
        compat_log("api_compat ARM32: combase has no WindowsCreateString - USING THE FAKE HSTRING SHIM (fmod/vccorlib will break)");
    }
    wsprintfA(line, "api_compat ARM32: combase.dll=%p wintypes.dll=%p native exports resolved %u/%u (%u missing): %s",
              combase, wintypes, resolved, (UINT)NATIVE_EXPORT_COUNT, missing, missing_list);
    compat_log(line);
}

static void native_ensure_ready(void)
{
    if (__sync_val_compare_and_swap(&native_state, 0, 1) == 0) {
        native_init_exports();
        __sync_synchronize();
        native_state = 2;
        native_hstring_self_test();
    } else {
        while (native_state != 2) Sleep(1);
    }
}

/* Same contract as the x86 version (named combase export or NULL), but ALWAYS native on ARM. */
static void *native_hstring_proc(const char *name)
{
    UINT index;
    if (native_state != 2) native_ensure_ready();
    for (index = 0; index < NATIVE_EXPORT_COUNT; ++index)
        if (native_exports[index].name == name) return native_exports[index].proc;
    for (index = 0; index < NATIVE_EXPORT_COUNT; ++index)
        if (native_name_equals(native_exports[index].name, name)) return native_exports[index].proc;
    return NULL;
}

/* Forward to the native export when it exists; otherwise fall through to the code that follows the macro. */
#define NATIVE_FORWARD(RETURN, EXPORT, PARAMS, ARGS) \
    do { \
        typedef RETURN (WINAPI *NativeForwardFn) PARAMS; \
        NativeForwardFn native_forward = (NativeForwardFn)native_hstring_proc(EXPORT); \
        if (native_forward) return native_forward ARGS; \
    } while (0)
#define NATIVE_FORWARD_VOID(EXPORT, PARAMS, ARGS) \
    do { \
        typedef void (WINAPI *NativeForwardFn) PARAMS; \
        NativeForwardFn native_forward = (NativeForwardFn)native_hstring_proc(EXPORT); \
        if (native_forward) { native_forward ARGS; return; } \
    } while (0)

static void compat_class_utf8(LPCWSTR class_name, char *text, int capacity)
{
    text[0] = 0;
    if (class_name)
        WideCharToMultiByte(CP_UTF8, 0, class_name, -1, text, capacity, NULL, NULL);
    text[capacity - 1] = 0;
}

/* RoGetActivationFactory: host fakes first (Win32CraftActivationDispatcher), then the real combase export.
 * use_dispatcher == FALSE is the combase-only entry (Win32CraftNativeRoGetActivationFactory). */
static HRESULT arm32_ro_get_activation_factory(
    HSTRING name, const GUID *iid, void **factory, BOOL use_dispatcher)
{
    typedef HRESULT (WINAPI *NativeFn)(HSTRING, const GUID *, void **);
    NativeFn native = (NativeFn)native_hstring_proc("RoGetActivationFactory");
    Win32CraftActivationFn dispatcher = use_dispatcher ? Win32CraftActivationDispatcher : NULL;
    LPCWSTR class_name = NULL;
    UINT32 length = 0;
    HRESULT result;
    char class_utf8[200];
    char line[420];

    if (name) class_name = WindowsGetStringRawBuffer(name, &length);
    compat_class_utf8(length ? class_name : NULL, class_utf8, sizeof(class_utf8));
    if (dispatcher && factory && class_name && length) {
        void *object = NULL;
        result = dispatcher(class_name, iid, &object);
        if (SUCCEEDED(result) && object) {
            *factory = object;
            wsprintfA(line, "RoGetActivationFactory(%s) IID %08lx: served by the host dispatcher -> %p",
                      class_utf8, iid ? (unsigned long)iid->Data1 : 0UL, object);
            compat_log(line);
            return result;
        }
        wsprintfA(line, "RoGetActivationFactory(%s): dispatcher returned 0x%08lx object=%p - trying combase",
                  class_utf8, (unsigned long)result, object);
        compat_log(line);
    }
    if (native) {
        result = native(name, iid, factory);
        wsprintfA(line, "RoGetActivationFactory(%s) IID %08lx: combase HRESULT 0x%08lx factory=%p",
                  class_utf8, iid ? (unsigned long)iid->Data1 : 0UL, (unsigned long)result,
                  factory ? *factory : NULL);
        compat_log(line);
        return result;
    }
    compat_log("RoGetActivationFactory: combase export missing and the dispatcher did not serve the class");
    if (factory) *factory = NULL;
    return REGDB_E_CLASSNOTREG;
}

/* Combase-only entry for host code (e.g. a filtered fmod_ARM.dll IAT hook that serves Package/ApplicationData from the
 * fakes and sends every other class here): never consults Win32CraftActivationDispatcher. */
HRESULT WINAPI Win32CraftNativeRoGetActivationFactory(HSTRING name, const GUID *iid, void **factory)
{
    return arm32_ro_get_activation_factory(name, iid, factory, FALSE);
}
#else
static void *native_hstring_proc(const char *name)
{
    static HMODULE combase;
    BYTE *image = (BYTE *)GetModuleHandleW(NULL);
    DWORD pe_offset;
    if (!image || IsBadReadPtr(image, 0x40)) return NULL;
    pe_offset = *(DWORD *)(image + 0x3c);
    if (pe_offset >= 0x1000 || IsBadReadPtr(image + pe_offset, 0x54) ||
        *(DWORD *)(image + pe_offset) != 0x00004550 ||
        *(DWORD *)(image + pe_offset + 0x50) != 0x02db1000)
        return NULL;
    if (!combase) combase = LoadLibraryW(L"combase.dll");
    return combase ? GetProcAddress(combase, name) : NULL;
}
#endif

static Win32CraftHString *hstring_value(HSTRING string)
{
    return (Win32CraftHString *)string;
}

HRESULT WINAPI WindowsCreateString(
    LPCWSTR source, UINT32 length, HSTRING *result)
{
    typedef HRESULT (WINAPI *NativeFn)(LPCWSTR, UINT32, HSTRING *);
    NativeFn native = (NativeFn)native_hstring_proc("WindowsCreateString");
    Win32CraftHString *string;
    WCHAR *text;
    SIZE_T bytes;
    HANDLE heap;

    if (native) return native(source, length, result);
    if (!result) return E_POINTER;
    *result = NULL;
    if (!source && length) return E_INVALIDARG;
    bytes = sizeof(*string) + ((SIZE_T)length + 1) * sizeof(WCHAR);
    heap = GetProcessHeap();
    string = (Win32CraftHString *)HeapAlloc(heap, 0, bytes);
    if (!string) return E_OUTOFMEMORY;
    text = (WCHAR *)(string + 1);
    if (length) CopyMemory(text, source, (SIZE_T)length * sizeof(WCHAR));
    text[length] = 0;
    string->magic = WIN32CRAFT_HSTRING_MAGIC;
    string->flags = WIN32CRAFT_HSTRING_OWNED;
    string->length = length;
    string->text = text;
    *result = (HSTRING)string;
    return S_OK;
}

HRESULT WINAPI WindowsCreateStringReference(
    LPCWSTR source, UINT32 length, void *header, HSTRING *result)
{
    typedef HRESULT (WINAPI *NativeFn)(LPCWSTR, UINT32, void *, HSTRING *);
    NativeFn native = (NativeFn)native_hstring_proc(
        "WindowsCreateStringReference");
    Win32CraftHStringHeader *storage = (Win32CraftHStringHeader *)header;
    if (native) return native(source, length, header, result);
    if (!header || !result) return E_POINTER;
    *result = NULL;
    if (!source && length) return E_INVALIDARG;
    storage->value.magic = WIN32CRAFT_HSTRING_MAGIC;
    storage->value.flags = 0;
    storage->value.length = length;
    storage->value.text = source;
    *result = (HSTRING)&storage->value;
    return S_OK;
}

HRESULT WINAPI WindowsDeleteString(HSTRING string)
{
    typedef HRESULT (WINAPI *NativeFn)(HSTRING);
    NativeFn native = (NativeFn)native_hstring_proc("WindowsDeleteString");
    Win32CraftHString *value = hstring_value(string);
    if (native) return native(string);
    if (value && value->magic == WIN32CRAFT_HSTRING_MAGIC &&
        (value->flags & WIN32CRAFT_HSTRING_OWNED))
        HeapFree(GetProcessHeap(), 0, value);
    return S_OK;
}

HRESULT WINAPI WindowsDuplicateString(HSTRING string, HSTRING *result)
{
    typedef HRESULT (WINAPI *NativeFn)(HSTRING, HSTRING *);
    NativeFn native = (NativeFn)native_hstring_proc("WindowsDuplicateString");
    Win32CraftHString *value = hstring_value(string);
    if (native) return native(string, result);
    if (!result) return E_POINTER;
    *result = NULL;
    if (!value || value->magic != WIN32CRAFT_HSTRING_MAGIC)
        return WindowsCreateString(NULL, 0, result);
    return WindowsCreateString(value->text, value->length, result);
}

UINT32 WINAPI WindowsGetStringLen(HSTRING string)
{
    typedef UINT32 (WINAPI *NativeFn)(HSTRING);
    NativeFn native = (NativeFn)native_hstring_proc("WindowsGetStringLen");
    Win32CraftHString *value = hstring_value(string);
    if (native) return native(string);
    return value && value->magic == WIN32CRAFT_HSTRING_MAGIC
        ? value->length : 0;
}

LPCWSTR WINAPI WindowsGetStringRawBuffer(HSTRING string, UINT32 *length)
{
    typedef LPCWSTR (WINAPI *NativeFn)(HSTRING, UINT32 *);
    NativeFn native = (NativeFn)native_hstring_proc(
        "WindowsGetStringRawBuffer");
    Win32CraftHString *value = hstring_value(string);
    if (native) return native(string, length);
    if (length) *length = value && value->magic == WIN32CRAFT_HSTRING_MAGIC
        ? value->length : 0;
    return value && value->magic == WIN32CRAFT_HSTRING_MAGIC
        ? value->text : empty_string;
}

BOOL WINAPI WindowsIsStringEmpty(HSTRING string)
{
    typedef BOOL (WINAPI *NativeFn)(HSTRING);
    NativeFn native = (NativeFn)native_hstring_proc("WindowsIsStringEmpty");
    if (native) return native(string);
    return WindowsGetStringLen(string) == 0;
}

INT WINAPI WindowsCompareStringOrdinal(HSTRING left, HSTRING right, INT *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsCompareStringOrdinal", (HSTRING, HSTRING, INT *), (left, right, result));
#endif

    typedef HRESULT (WINAPI *NativeFn)(HSTRING, HSTRING, INT *);
    NativeFn native = (NativeFn)native_hstring_proc(
        "WindowsCompareStringOrdinal");
    LPCWSTR a = WindowsGetStringRawBuffer(left, NULL);
    LPCWSTR b = WindowsGetStringRawBuffer(right, NULL);
    UINT32 alen = WindowsGetStringLen(left), blen = WindowsGetStringLen(right);
    UINT32 index, count = alen < blen ? alen : blen;
    if (native) return native(left, right, result);
    if (!result) return E_POINTER;
    for (index = 0; index < count && a[index] == b[index]; ++index) {}
    *result = index < count ? (a[index] < b[index] ? -1 : 1)
                            : (alen < blen ? -1 : alen > blen ? 1 : 0);
    return S_OK;
}

HRESULT WINAPI WindowsConcatString(HSTRING left, HSTRING right, HSTRING *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsConcatString", (HSTRING, HSTRING, HSTRING *), (left, right, result));
#endif

    typedef HRESULT (WINAPI *NativeFn)(HSTRING, HSTRING, HSTRING *);
    NativeFn native = (NativeFn)native_hstring_proc("WindowsConcatString");
    UINT32 alen = WindowsGetStringLen(left), blen = WindowsGetStringLen(right);
    LPCWSTR a = WindowsGetStringRawBuffer(left, NULL);
    LPCWSTR b = WindowsGetStringRawBuffer(right, NULL);
    WCHAR *text;
    HANDLE heap;
    HRESULT status;
    if (native) return native(left, right, result);
    if (!result) return E_POINTER;
    heap = GetProcessHeap();
    text = (WCHAR *)HeapAlloc(heap, 0, ((SIZE_T)alen + blen + 1) * sizeof(WCHAR));
    if (!text) return E_OUTOFMEMORY;
    CopyMemory(text, a, (SIZE_T)alen * sizeof(WCHAR));
    CopyMemory(text + alen, b, (SIZE_T)blen * sizeof(WCHAR));
    text[alen + blen] = 0;
    status = WindowsCreateString(text, alen + blen, result);
    HeapFree(heap, 0, text);
    return status;
}

HRESULT WINAPI WindowsSubstringWithSpecifiedLength(
    HSTRING string, UINT32 start, UINT32 length, HSTRING *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsSubstringWithSpecifiedLength", (HSTRING, UINT32, UINT32, HSTRING *), (string, start, length, result));
#endif

    UINT32 total = WindowsGetStringLen(string);
    LPCWSTR text = WindowsGetStringRawBuffer(string, NULL);
    if (start > total || length > total - start) return WIN32CRAFT_E_BOUNDS;
    return WindowsCreateString(text + start, length, result);
}

HRESULT WINAPI WindowsSubstring(HSTRING string, UINT32 start, HSTRING *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsSubstring", (HSTRING, UINT32, HSTRING *), (string, start, result));
#endif

    UINT32 total = WindowsGetStringLen(string);
    if (start > total) return WIN32CRAFT_E_BOUNDS;
    return WindowsSubstringWithSpecifiedLength(string, start, total - start, result);
}

HRESULT WINAPI WindowsStringHasEmbeddedNull(HSTRING string, BOOL *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsStringHasEmbeddedNull", (HSTRING, BOOL *), (string, result));
#endif

    LPCWSTR text = WindowsGetStringRawBuffer(string, NULL);
    UINT32 index, length = WindowsGetStringLen(string);
    if (!result) return E_POINTER;
    *result = FALSE;
    for (index = 0; index < length; ++index)
        if (!text[index]) { *result = TRUE; break; }
    return S_OK;
}

HRESULT WINAPI WindowsReplaceString(
    HSTRING string, HSTRING search, HSTRING replacement, HSTRING *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsReplaceString", (HSTRING, HSTRING, HSTRING, HSTRING *), (string, search, replacement, result));
#endif

    (void)search;
    (void)replacement;
    return WindowsDuplicateString(string, result);
}

HRESULT WINAPI WindowsTrimStringStart(
    HSTRING string, HSTRING trim, HSTRING *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsTrimStringStart", (HSTRING, HSTRING, HSTRING *), (string, trim, result));
#endif

    (void)trim;
    return WindowsDuplicateString(string, result);
}

HRESULT WINAPI WindowsTrimStringEnd(
    HSTRING string, HSTRING trim, HSTRING *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsTrimStringEnd", (HSTRING, HSTRING, HSTRING *), (string, trim, result));
#endif

    (void)trim;
    return WindowsDuplicateString(string, result);
}

HRESULT WINAPI WindowsPreallocateStringBuffer(
    UINT32 length, WCHAR **buffer, void **handle)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsPreallocateStringBuffer", (UINT32, WCHAR **, void **), (length, buffer, handle));
    /* the fallback below builds shim-layout handles: never mix them with native ones */
    if (native_hstring_proc("WindowsCreateString")) return E_NOTIMPL;
#endif

    Win32CraftHStringBuffer *memory;
    SIZE_T bytes;
    if (!buffer || !handle) return E_POINTER;
    *buffer = NULL;
    *handle = NULL;
    bytes = sizeof(Win32CraftHString) + sizeof(DWORD) +
            ((SIZE_T)length + 1) * sizeof(WCHAR);
    memory = (Win32CraftHStringBuffer *)HeapAlloc(
        GetProcessHeap(), 0, bytes);
    if (!memory) return E_OUTOFMEMORY;
    memory->value.magic = WIN32CRAFT_HSTRING_MAGIC;
    memory->value.flags = WIN32CRAFT_HSTRING_OWNED;
    memory->value.length = length;
    memory->value.text = memory->text;
    memory->reserved = 0;
    memory->text[length] = 0;
    *buffer = memory->text;
    *handle = memory;
    return S_OK;
}

HRESULT WINAPI WindowsPromoteStringBuffer(void *handle, HSTRING *result)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsPromoteStringBuffer", (void *, HSTRING *), (handle, result));
    /* the fallback below builds shim-layout handles: never mix them with native ones */
    if (native_hstring_proc("WindowsCreateString")) return E_NOTIMPL;
#endif

    if (!handle || !result) return E_POINTER;
    *result = (HSTRING)handle;
    return S_OK;
}

HRESULT WINAPI WindowsDeleteStringBuffer(void *handle)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "WindowsDeleteStringBuffer", (void *), (handle));
    /* the fallback below builds shim-layout handles: never mix them with native ones */
    if (native_hstring_proc("WindowsCreateString")) return E_NOTIMPL;
#endif

    if (handle) HeapFree(GetProcessHeap(), 0, handle);
    return S_OK;
}

#ifdef WIN32CRAFT_ARM32
/* One-shot HSTRING sanity check run the first time any native export is needed: the log tells the user (and us)
 * whether real HSTRINGs round-trip through this DLL on the device. */
static void native_hstring_self_test(void)
{
    HSTRING text = NULL, copy = NULL, reference = NULL, joined = NULL;
    UINT32 header[8];   /* HSTRING_HEADER is 20 bytes on 32-bit; 32 is plenty */
    UINT32 length = 0, joined_length = 0;
    INT compare = 99;
    LPCWSTR raw, joined_raw;
    HRESULT status[5];
    BOOL ok;
    char line[400];

    ZeroMemory(header, sizeof(header));
    status[0] = WindowsCreateString(L"Win32Craft", 10, &text);
    raw = text ? WindowsGetStringRawBuffer(text, &length) : NULL;
    status[1] = WindowsDuplicateString(text, &copy);
    status[2] = WindowsCompareStringOrdinal(text, copy, &compare);
    status[3] = WindowsCreateStringReference(L"Craft", 5, header, &reference);
    status[4] = WindowsConcatString(text, reference, &joined);
    joined_raw = joined ? WindowsGetStringRawBuffer(joined, &joined_length) : NULL;
    ok = status[0] == S_OK && status[1] == S_OK && status[2] == S_OK && status[3] == S_OK && status[4] == S_OK &&
         text && raw && length == 10 && raw[0] == L'W' && raw[9] == L't' && raw[10] == 0 &&
         WindowsGetStringLen(text) == 10 && !WindowsIsStringEmpty(text) && WindowsIsStringEmpty(NULL) &&
         compare == 0 && joined_raw && joined_length == 15 && joined_raw[10] == L'C' && joined_raw[15] == 0;
    wsprintfA(line, "HSTRING self-test %s (native=%s): create=0x%08lx dup=0x%08lx cmp=0x%08lx/%d ref=0x%08lx concat=0x%08lx text=%p len=%u joined=%p len=%u",
              ok ? "PASSED" : "FAILED", native_hstring_proc("WindowsCreateString") ? "combase" : "SHIM",
              (unsigned long)status[0], (unsigned long)status[1], (unsigned long)status[2], compare,
              (unsigned long)status[3], (unsigned long)status[4], text, (unsigned)length, joined,
              (unsigned)joined_length);
    compat_log(line);
    WindowsDeleteString(joined);
    WindowsDeleteString(reference);
    WindowsDeleteString(copy);
    WindowsDeleteString(text);
}

#endif
HRESULT WINAPI RoInitialize(UINT type)
{
    typedef HRESULT (WINAPI *Fn)(UINT);
    static Fn function;
    static BOOL resolved;
    if (!resolved) {
        HMODULE module;
        resolved = TRUE;
        module = LoadLibraryW(L"combase.dll");
        if (module) function = (Fn)GetProcAddress(module, "RoInitialize");
    }
    return function ? function(type) : S_OK;
}

void WINAPI RoUninitialize(void)
{
    typedef void (WINAPI *Fn)(void);
    static Fn function;
    static BOOL resolved;
    if (!resolved) {
        HMODULE module;
        resolved = TRUE;
        module = LoadLibraryW(L"combase.dll");
        if (module) function = (Fn)GetProcAddress(module, "RoUninitialize");
    }
    if (function) function();
}
HRESULT WINAPI RoActivateInstance(HSTRING name, void **instance)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoActivateInstance", (HSTRING, void **), (name, instance));
#endif
 (void)name; if (instance) *instance = NULL; return REGDB_E_CLASSNOTREG; }
HRESULT WINAPI RoGetActivationFactory(HSTRING name, const GUID *iid, void **factory)
{
#ifdef WIN32CRAFT_ARM32
    return arm32_ro_get_activation_factory(name, iid, factory, TRUE);
#endif
 (void)name; (void)iid; if (factory) *factory = NULL; return REGDB_E_CLASSNOTREG; }
HRESULT WINAPI RoGetApartmentIdentifier(ULONGLONG *id)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoGetApartmentIdentifier", (ULONGLONG *), (id));
#endif
 if (!id) return E_POINTER; *id = 1; return S_OK; }
HRESULT WINAPI RoGetBufferMarshaler(void **value)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoGetBufferMarshaler", (void **), (value));
#endif
 if (value) *value = NULL; return E_NOTIMPL; }
HRESULT WINAPI RoGetParameterizedTypeInstanceIID(
    UINT32 count, const WCHAR **names, const void *locator, GUID *iid, void **extra)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoGetParameterizedTypeInstanceIID", (UINT32, const WCHAR **, const void *, GUID *, void **), (count, names, locator, iid, extra));
#endif
 (void)count; (void)names; (void)locator; (void)iid; if (extra) *extra = NULL;
  return WIN32CRAFT_RO_E_METADATA_NAME_NOT_FOUND; }
void WINAPI RoFreeParameterizedTypeExtra(void *extra) {
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD_VOID("RoFreeParameterizedTypeExtra", (void *), (extra));
#endif
 (void)extra; }
HRESULT WINAPI RoRegisterActivationFactories(
    HSTRING *classes, void **callbacks, UINT32 count, void **cookie)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoRegisterActivationFactories", (HSTRING *, void **, UINT32, void **), (classes, callbacks, count, cookie));
#endif
 (void)classes; (void)callbacks; (void)count; if (cookie) *cookie = NULL;
  return E_NOTIMPL; }
void WINAPI RoRevokeActivationFactories(void *cookie) {
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD_VOID("RoRevokeActivationFactories", (void *), (cookie));
#endif
 (void)cookie; }
HRESULT WINAPI RoRegisterForApartmentShutdown(void *callback, ULONGLONG *id, void **cookie)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoRegisterForApartmentShutdown", (void *, ULONGLONG *, void **), (callback, id, cookie));
#endif
 (void)callback; if (id) *id = 1; if (cookie) *cookie = NULL; return E_NOTIMPL; }
HRESULT WINAPI RoUnregisterForApartmentShutdown(void *cookie)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoUnregisterForApartmentShutdown", (void *), (cookie));
#endif
 (void)cookie; return S_OK; }

HRESULT WINAPI GetRestrictedErrorInfo(void **info)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "GetRestrictedErrorInfo", (void **), (info));
#endif
 if (!info) return E_POINTER; *info = NULL; return WIN32CRAFT_S_FALSE; }
HRESULT WINAPI SetRestrictedErrorInfo(void *info) {
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "SetRestrictedErrorInfo", (void *), (info));
#endif
 (void)info; return S_OK; }
BOOL WINAPI RoOriginateError(HRESULT error, HSTRING message)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(BOOL, "RoOriginateError", (HRESULT, HSTRING), (error, message));
#endif
 (void)error; (void)message; return FALSE; }
BOOL WINAPI RoOriginateErrorW(HRESULT error, UINT32 length, LPCWSTR message)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(BOOL, "RoOriginateErrorW", (HRESULT, UINT32, LPCWSTR), (error, length, message));
#endif
 (void)error; (void)length; (void)message; return FALSE; }
BOOL WINAPI RoTransformError(HRESULT old_error, HRESULT new_error, HSTRING message)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(BOOL, "RoTransformError", (HRESULT, HRESULT, HSTRING), (old_error, new_error, message));
#endif
 (void)old_error; (void)new_error; (void)message; return FALSE; }
HRESULT WINAPI RoCaptureErrorContext(HRESULT error) {
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoCaptureErrorContext", (HRESULT), (error));
#endif
 (void)error; return S_OK; }
void WINAPI RoFailFastWithErrorContext(HRESULT error) {
#ifdef WIN32CRAFT_ARM32
    {
        char line[96];
        wsprintfA(line, "RoFailFastWithErrorContext(0x%08lx) forwarded to combase", (unsigned long)error);
        compat_log(line);
    }
    NATIVE_FORWARD_VOID("RoFailFastWithErrorContext", (HRESULT), (error));
#endif
 (void)error; }
HRESULT WINAPI RoGetErrorReportingFlags(UINT32 *flags)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoGetErrorReportingFlags", (UINT32 *), (flags));
#endif
 if (!flags) return E_POINTER; *flags = 0; return S_OK; }
HRESULT WINAPI RoSetErrorReportingFlags(UINT32 flags) {
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoSetErrorReportingFlags", (UINT32), (flags));
#endif
 (void)flags; return S_OK; }
HRESULT WINAPI RoReportFailedDelegate(void *delegate, void *error)
{
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoReportFailedDelegate", (void *, void *), (delegate, error));
#endif
 (void)delegate; (void)error; return S_OK; }
HRESULT WINAPI RoReportUnhandledError(void *error) {
#ifdef WIN32CRAFT_ARM32
    NATIVE_FORWARD(HRESULT, "RoReportUnhandledError", (void *), (error));
#endif
 (void)error; return S_OK; }

void WINAPI ProcessPendingGameUI(void) {}
HRESULT WINAPI ShowGameInviteUI(void *a, void *b, void *c, void *d, void *e, void *f)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return E_NOTIMPL; }
HRESULT WINAPI ShowProfileCardUI(void *a, void *b, void *c, void *d)
{ (void)a; (void)b; (void)c; (void)d; return E_NOTIMPL; }
HRESULT WINAPI ShowTitleAchievementsUI(void *a, void *b, void *c, void *d)
{ (void)a; (void)b; (void)c; (void)d; return E_NOTIMPL; }

DWORD WINAPI Win32CraftXInputGetState(DWORD user_index, void *state)
{
    typedef DWORD (WINAPI *XInputGetStateFn)(DWORD, void *);
    static XInputGetStateFn function;
    static BOOL resolved;
    static const WCHAR *const names[] = {
        L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"
    };
    UINT index;
    if (!resolved) {
        resolved = TRUE;
        for (index = 0; index < ARRAYSIZE(names) && !function; ++index) {
            HMODULE module = LoadLibraryW(names[index]);
            if (module)
                function = (XInputGetStateFn)GetProcAddress(
                    module, "XInputGetState");
        }
    }
    return function ? function(user_index, state)
                    : WIN32CRAFT_ERROR_DEVICE_NOT_CONNECTED;
}
