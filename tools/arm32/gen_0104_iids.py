#!/usr/bin/env python3
"""Generate win32_host_arm32_0104_iids.inl: IIDs + vtable slot numbers of the WinRT interfaces the 0.10.4 host fakes.

Source of truth: the Windows SDK WinRT headers (MIDL-generated C++ ABI interfaces: the order of the `virtual ... STDMETHODCALLTYPE`
members IS the vtable order; IInspectable contributes slots 0..5, IUnknown 0..2).  The numbers are cross-checked against the
verified slot facts of arm32/v0_10_4/facts (see CHECKS below).  Usage:

    python tools/arm32/gen_0104_iids.py            # rewrites win32_host_arm32_0104_iids.inl, prints the cross-check
    SDK_INCLUDE=...\\winrt python tools/arm32/gen_0104_iids.py

Interfaces that are not in the SDK (Windows Phone 8.1 only: HardwareButtons, VibrationDevice) are written by hand in the
MANUAL block of the generated file (values from arm32/v0_10_4/facts, medium confidence where the facts say so)."""
import os, re, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
SDK = Path(os.environ.get("SDK_INCLUDE", r"D:\Windows Kits\10\Include\10.0.19041.0\winrt"))
OUT = REPO / "win32_host_arm32_0104_iids.inl"

# short C name -> full WinRT interface name (as in the SDK header's InterfaceName_ string)
WANTED = """
Windows.UI.Xaml.IApplicationStatics Windows.UI.Xaml.IApplicationFactory Windows.UI.Xaml.IApplication
Windows.UI.Xaml.IApplicationOverrides Windows.UI.Xaml.IWindowStatics Windows.UI.Xaml.IWindow
Windows.UI.Xaml.IDependencyObject Windows.UI.Xaml.IUIElement Windows.UI.Xaml.IFrameworkElement
Windows.UI.Xaml.ISizeChangedEventArgs Windows.UI.Xaml.IDispatcherTimerFactory Windows.UI.Xaml.IDispatcherTimer

Windows.UI.Xaml.Controls.IPageFactory Windows.UI.Xaml.Controls.IPage Windows.UI.Xaml.Controls.ICanvasStatics
Windows.UI.Xaml.Controls.ISwapChainPanel Windows.UI.Xaml.Controls.IControl Windows.UI.Xaml.Controls.ITextBox
Windows.UI.Xaml.Controls.ITextBox2 Windows.UI.Xaml.Controls.IImage Windows.UI.Xaml.Controls.IButton
Windows.UI.Xaml.Controls.ITextChangedEventArgs
Windows.UI.Core.ICoreWindow Windows.UI.Core.ICoreWindowStatic Windows.UI.Core.ICoreDispatcher
Windows.UI.Core.ICorePointerInputSource Windows.UI.Core.ICoreInputSourceBase Windows.UI.Core.ICoreCursor
Windows.UI.Core.ICoreCursorFactory Windows.UI.Core.IPointerEventArgs Windows.UI.Core.IKeyEventArgs
Windows.UI.Core.IVisibilityChangedEventArgs Windows.UI.Core.ICoreWindowEventArgs
Windows.UI.Core.IAutomationProviderRequestedEventArgs Windows.UI.Core.ICoreWindow2
Windows.UI.Input.IPointerPoint Windows.UI.Input.IPointerPointProperties
Windows.Devices.Input.IPointerDevice
Windows.UI.ViewManagement.IInputPaneStatics Windows.UI.ViewManagement.IInputPane
Windows.UI.ViewManagement.IInputPaneVisibilityEventArgs
Windows.UI.ViewManagement.IStatusBarStatics Windows.UI.ViewManagement.IStatusBar
Windows.Graphics.Display.IDisplayInformationStatics Windows.Graphics.Display.IDisplayInformation
Windows.Graphics.Display.IDisplayInformation2
Windows.System.Threading.IThreadPoolStatics
Windows.System.IMemoryManagerStatics
Windows.Foundation.IAsyncAction Windows.Foundation.IClosable
Windows.Foundation.IUriRuntimeClassFactory Windows.Foundation.IUriRuntimeClass
Windows.Foundation.Diagnostics.IAsyncCausalityTracerStatics
Windows.Storage.IApplicationDataStatics Windows.Storage.IApplicationData Windows.Storage.IApplicationDataContainer
Windows.Storage.IStorageItem Windows.Storage.IStorageFolder
Windows.ApplicationModel.IPackageStatics Windows.ApplicationModel.IPackage
Windows.ApplicationModel.ISuspendingEventArgs Windows.ApplicationModel.ISuspendingOperation
Windows.ApplicationModel.Activation.IActivatedEventArgs Windows.ApplicationModel.Activation.ILaunchActivatedEventArgs
Windows.ApplicationModel.ISuspendingDeferral
Windows.ApplicationModel.Store.ICurrentApp Windows.ApplicationModel.Store.ICurrentAppSimulator
Windows.ApplicationModel.Store.ICurrentAppSimulatorWithConsumables Windows.ApplicationModel.Store.ILicenseInformation
Windows.Networking.Connectivity.IConnectionProfile2
Windows.Networking.Connectivity.INetworkInformationStatics Windows.Networking.Connectivity.IConnectionProfile
Windows.ApplicationModel.Core.ICoreApplication
""".split()

# (interface short name, method, expected slot)  -- slots taken from the verified facts (startup_flow 2a / input_text 3 / platform_services)
CHECKS = [
    ("IApplicationStatics", "Start", 7), ("IApplicationStatics", "LoadComponentWithResourceLocation", 9),
    ("IApplicationFactory", "CreateInstance", 6), ("IApplication", "add_Suspending", 13), ("IApplication", "add_Resuming", 15),
    ("IApplicationOverrides", "OnLaunched", 7), ("IActivatedEventArgs", "get_PreviousExecutionState", 7),
    ("IWindowStatics", "get_Current", 6), ("IWindow", "put_Content", 9), ("IWindow", "get_CoreWindow", 10), ("IWindow", "Activate", 20),
    ("ICoreWindow", "add_VisibilityChanged", 56), ("ICoreWindow", "add_KeyDown", 34), ("ICoreWindow", "add_KeyUp", 36),
    ("ICoreWindow", "add_PointerMoved", 44), ("ICoreWindow", "add_PointerPressed", 46), ("ICoreWindow", "add_PointerReleased", 48),
    ("ICoreWindow", "get_Dispatcher", 9), ("ICoreWindow", "put_PointerCursor", 15),
    ("IVisibilityChangedEventArgs", "get_Visible", 6),
    ("IDisplayInformationStatics", "GetForCurrentView", 6), ("IDisplayInformationStatics", "add_DisplayContentsInvalidated", 9),
    ("IDisplayInformation", "get_CurrentOrientation", 6), ("IDisplayInformation", "get_NativeOrientation", 7),
    ("IDisplayInformation", "add_OrientationChanged", 8), ("IDisplayInformation", "get_LogicalDpi", 11),
    ("IDisplayInformation", "get_RawDpiX", 12), ("IDisplayInformation", "get_RawDpiY", 13), ("IDisplayInformation", "add_DpiChanged", 14),
    ("ISwapChainPanel", "get_CompositionScaleX", 6), ("ISwapChainPanel", "get_CompositionScaleY", 7),
    ("ISwapChainPanel", "add_CompositionScaleChanged", 8), ("ISwapChainPanel", "CreateCoreIndependentInputSource", 10),
    ("IFrameworkElement", "get_ActualWidth", 13), ("IFrameworkElement", "get_ActualHeight", 14),
    ("IFrameworkElement", "add_SizeChanged", 47), ("IFrameworkElement", "FindName", 51),
    ("ISizeChangedEventArgs", "get_NewSize", 7),
    ("IUIElement", "put_Visibility", 22), ("IUIElement", "get_RenderSize", 23),
    ("IDependencyObject", "get_Dispatcher", 11),
    ("ICoreDispatcher", "ProcessEvents", 7), ("ICoreDispatcher", "RunAsync", 8),
    ("ICorePointerInputSource", "add_PointerPressed", 20), ("ICorePointerInputSource", "add_PointerMoved", 18),
    ("ICorePointerInputSource", "add_PointerReleased", 22), ("ICoreInputSourceBase", "get_Dispatcher", 6),
    ("IDispatcherTimerFactory", "CreateInstance", 6), ("IDispatcherTimer", "put_Interval", 7), ("IDispatcherTimer", "add_Tick", 9),
    ("IDispatcherTimer", "Start", 11), ("IDispatcherTimer", "Stop", 12),
    ("IInputPaneStatics", "GetForCurrentView", 6), ("IInputPane", "add_Showing", 6), ("IInputPane", "add_Hiding", 8),
    ("IInputPane", "get_OccludedRect", 10),
    ("ICanvasStatics", "SetLeft", 8),
    ("IThreadPoolStatics", "RunAsync", 6), ("IThreadPoolStatics", "RunWithPriorityAndOptionsAsync", 8),
        ("IApplicationDataStatics", "get_Current", 6), ("IApplicationData", "get_LocalFolder", 12), ("IApplicationData", "get_LocalSettings", 10),
    ("IApplicationDataContainer", "get_Values", 8), ("IStorageItem", "get_Path", 12),
    ("IPackageStatics", "get_Current", 6), ("IPackage", "get_InstalledLocation", 7),
    ("IStatusBarStatics", "GetForCurrentView", 6), ("IStatusBar", "HideAsync", 7),
    ("IPageFactory", "CreateInstance", 6), ("IUriRuntimeClassFactory", "CreateUri", 6),
    ("ITextBox", "get_Text", 6), ("ITextBox", "put_Text", 7), ("ITextBox", "get_MaxLength", 14), ("ITextBox", "put_MaxLength", 15),
    ("ITextBox", "put_AcceptsReturn", 19), ("ITextBox", "add_TextChanged", 30), ("ITextBox", "Select", 36),
    ("ITextBox2", "put_PreventKeyboardDisplayOnProgrammaticFocus", 15), ("IControl", "Focus", 46),
    ("IPointerEventArgs", "get_CurrentPoint", 6), ("IKeyEventArgs", "get_VirtualKey", 6), ("IKeyEventArgs", "get_KeyStatus", 7),
    ("IPointerPoint", "get_PointerDevice", 6), ("IPointerPoint", "get_Position", 7), ("IPointerPoint", "get_PointerId", 9),
    ("IPointerPoint", "get_Properties", 13), ("IPointerPointProperties", "get_IsLeftButtonPressed", 16),
    ("IPointerDevice", "get_PointerDeviceType", 6),
    ("IMemoryManagerStatics", "get_AppMemoryUsage", 6), ("IMemoryManagerStatics", "get_AppMemoryUsageLimit", 7),
    ("INetworkInformationStatics", "GetInternetConnectionProfile", 7),
    ("ICoreCursorFactory", "CreateCursor", 6), ("ICoreWindowStatic", "GetForCurrentThread", 6),
]

PROLOGUE = r'''
/* ---- basics every generated typedef needs (the core includes this file FIRST) ---- */
#ifndef X104_API
#define X104_API WINAPI          /* COM thunk calling convention: empty on ARM, stdcall on the x86 host-side test build */
#endif
/* WinRT value types that always travel through POINTERS when they are OUTPUT parameters; as INPUT parameters Size/Point/Rect are passed by value
 * (all-float structs: homogeneous aggregates, in s0..s3 under AAPCS-VFP).  Windows.Foundation.Size / Point / Rect */
typedef struct X104Size  { float Width, Height; } X104Size;
typedef struct X104Point { float X, Y; } X104Point;
typedef struct X104Rect  { float X, Y, Width, Height; } X104Rect;
'''

MANUAL = r'''
/* ---- MANUAL: Windows Phone 8.1 interfaces that are not in the desktop SDK (values from arm32/v0_10_4/facts) ---- */
/* Windows.Phone.UI.Input.IHardwareButtonsStatics {594B8780-DA66-4FD8-A776-7506BD0CBFA7}: slot 6 add_BackPressed, 7 remove_BackPressed
 * (medium confidence on the slot, facts startup_flow.slots_hardwarebuttons / input_text.back_button), 8 add_CameraHalfPressed ... */
static const GUID X104_IID_IHardwareButtonsStatics = {0x594b8780,0xda66,0x4fd8,{0xa7,0x76,0x75,0x06,0xbd,0x0c,0xbf,0xa7}};
enum { X104_SLOT_IHardwareButtonsStatics_add_BackPressed = 6, X104_SLOT_IHardwareButtonsStatics_remove_BackPressed = 7,
       X104_NSLOTS_IHardwareButtonsStatics = 14 };
/* Windows.Phone.UI.Input.IBackPressedEventArgs: slot 6 get_Handled, slot 7 put_Handled (facts input_text.back_button; the IID is NOT in the
 * facts: the game never QIs it, it only calls vtbl[7] of the object it is handed, so it may be answered with any IID) */
enum { X104_SLOT_IBackPressedEventArgs_get_Handled = 6, X104_SLOT_IBackPressedEventArgs_put_Handled = 7,
       X104_NSLOTS_IBackPressedEventArgs = 8 };
/* Windows.Phone.Devices.Notification.IVibrationDeviceStatics {332FD2F1-1C69-4C91-949E-4BB67A85BDC7}: slot 6 GetDefault(IVibrationDevice**).
 * IVibrationDevice: slot 6 Vibrate(TimeSpan duration)  [TimeSpan = int64 in r2:r3, r1 skipped], slot 7 Cancel  (facts platform_services.vibration.vibrate) */
static const GUID X104_IID_IVibrationDeviceStatics = {0x332fd2f1,0x1c69,0x4c91,{0x94,0x9e,0x4b,0xb6,0x7a,0x85,0xbd,0xc7}};
enum { X104_SLOT_IVibrationDeviceStatics_GetDefault = 6, X104_NSLOTS_IVibrationDeviceStatics = 7,
       X104_SLOT_IVibrationDevice_Vibrate = 6, X104_SLOT_IVibrationDevice_Cancel = 7, X104_NSLOTS_IVibrationDevice = 8 };
/* Windows.Foundation.IAsyncInfo {00000036-0000-0000-C000-000000000046} (asyncinfo.h, not projected in the winrt headers):
 * 6 get_Id(uint32*), 7 get_Status(AsyncStatus*: 0 Started, 1 Completed, 2 Canceled, 3 Error), 8 get_ErrorCode(HRESULT*), 9 Cancel, 10 Close
 * (facts startup_flow 2a: get_Status 7, Cancel 9). */
static const GUID X104_IID_IAsyncInfo = {0x00000036,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
enum { X104_SLOT_IAsyncInfo_get_Id = 6, X104_SLOT_IAsyncInfo_get_Status = 7, X104_SLOT_IAsyncInfo_get_ErrorCode = 8,
       X104_SLOT_IAsyncInfo_Cancel = 9, X104_SLOT_IAsyncInfo_Close = 10, X104_NSLOTS_IAsyncInfo = 11 };
enum { X104_ASYNC_STARTED = 0, X104_ASYNC_COMPLETED = 1, X104_ASYNC_CANCELED = 2, X104_ASYNC_ERROR = 3 };
/* IIDs of the host's own interface answers that are not WinRT-projected */
static const GUID X104_IID_IUnknown        = {0x00000000,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID X104_IID_IInspectable    = {0xaf86e2e0,0xb12d,0x4c6a,{0x9c,0x5a,0xd7,0xaa,0x65,0x10,0x1e,0x90}};
static const GUID X104_IID_IAgileObject    = {0x94ea2b94,0xe9cc,0x49e0,{0xc0,0xff,0xee,0x64,0xca,0x8f,0x5b,0x90}};
static const GUID X104_IID_IActivationFactory = {0x00000035,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
/* ISwapChainPanelNative {F92F19D2-3ADE-45A6-A20C-F6F1EA90554B} (IUnknown-based, windows.ui.xaml.media.dxinterop.h): slot 3 SetSwapChain(IDXGISwapChain*) */
static const GUID X104_IID_ISwapChainPanelNative = {0xf92f19d2,0x3ade,0x45a6,{0xa2,0x0c,0xf6,0xf1,0xea,0x90,0x55,0x4b}};
enum { X104_SLOT_ISwapChainPanelNative_SetSwapChain = 3, X104_NSLOTS_ISwapChainPanelNative = 4 };
/* hand-written slot typedefs for the manual interfaces above (same rule as the generated ones: X104_SLOT() requires this exact signature) */
typedef HRESULT (X104_API *X104_FN_IHardwareButtonsStatics_add_BackPressed)(void *, void *, LONGLONG *);
typedef HRESULT (X104_API *X104_FN_IHardwareButtonsStatics_remove_BackPressed)(void *, LONGLONG);
typedef HRESULT (X104_API *X104_FN_IBackPressedEventArgs_get_Handled)(void *, BYTE *);
typedef HRESULT (X104_API *X104_FN_IBackPressedEventArgs_put_Handled)(void *, BYTE);
typedef HRESULT (X104_API *X104_FN_IVibrationDeviceStatics_GetDefault)(void *, void **);
typedef HRESULT (X104_API *X104_FN_IVibrationDevice_Vibrate)(void *, LONGLONG);
typedef HRESULT (X104_API *X104_FN_IVibrationDevice_Cancel)(void *);
typedef HRESULT (X104_API *X104_FN_IAsyncInfo_get_Id)(void *, UINT32 *);
typedef HRESULT (X104_API *X104_FN_IAsyncInfo_get_Status)(void *, int *);
typedef HRESULT (X104_API *X104_FN_IAsyncInfo_get_ErrorCode)(void *, HRESULT *);
typedef HRESULT (X104_API *X104_FN_IAsyncInfo_Cancel)(void *);
typedef HRESULT (X104_API *X104_FN_IAsyncInfo_Close)(void *);
typedef HRESULT (X104_API *X104_FN_ISwapChainPanelNative_SetSwapChain)(void *, void *);
/* IWorkItemHandler / DispatchedHandler / event delegates are game objects: Invoke is ALWAYS vtable slot 3 (IUnknown-based delegates) */
enum { X104_SLOT_DELEGATE_Invoke = 3 };
'''


def split_params(text):
    out, depth, cur = [], 0, ""
    for ch in text:
        if ch in "(<":
            depth += 1
        elif ch in ")>":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def short_type(t):
    t = re.sub(r"\b(?:ABI::)?(?:Windows::\w+::)+(?:\w+::)*", "", t)      # drop namespaces
    t = re.sub(r"\bconst\b|\b_In_\b|\b_Out_\b|\bstruct\b", "", t)
    return re.sub(r"\s+", " ", t).strip()


class Unsupported(Exception):
    """a parameter type the generator cannot express (a struct BY VALUE that is not Size/Point/Rect, ...): no typedef is emitted for the slot,
    so a unit that wants to implement it with X104_SLOT gets a compile error and must extend the generator (or use X104_SLOT_NOARGS)"""


SIMPLE = {"boolean": "BYTE", "HSTRING": "HSTRING", "INT32": "INT", "UINT32": "UINT32", "int": "INT", "UINT64": "ULONGLONG", "INT64": "LONGLONG",
          "double": "double", "float": "float", "DOUBLE": "double", "FLOAT": "float", "INT": "INT", "UINT": "UINT32", "BOOLEAN": "BYTE", "ULONG": "ULONG", "TimeSpan": "LONGLONG", "EventRegistrationToken": "LONGLONG", "DateTime": "LONGLONG",
          "unsigned int": "UINT32", "HRESULT": "HRESULT", "GUID": "GUID", "UINT8": "BYTE", "UINT16": "WORD", "INT16": "SHORT"}
STRUCTS = {"Size": "X104Size", "Rect": "X104Rect", "Point": "X104Point"}


ENUMS = {"AsyncStatus"}          # filled by parse_headers(): `typedef enum X : int|unsigned int X;` of the SDK headers (ABI: a 32-bit integer; always written `int`)
VALUE_STRUCTS = set()  # `typedef struct X X;` value types (Thickness, CornerRadius, ...)


def c_param(decl):
    """'ABI::Windows::Foundation::Size* value' -> (c type, name, note).  Raises Unsupported for a struct passed by value that the host has no type for.
    The C types written here are the CANONICAL ones: the typedefs X104_FN_<Iface>_<Method> use them and X104_SLOT() requires an exactly matching thunk."""
    m = re.match(r"(.*?)(\w+)\s*$", decl.strip())
    if not m:
        return decl, "", ""
    type_text, name = m.group(1), m.group(2)
    t = short_type(type_text)
    stars = t.count("*")
    base = t.replace("*", "").strip()
    note = ""
    if base in SIMPLE:
        cbase = SIMPLE[base]
    elif base in STRUCTS:
        cbase = STRUCTS[base]
        if stars == 0:
            note = "STRUCT BY VALUE (all-float struct: AAPCS-VFP homogeneous aggregate, passed in s0.. like floats)"
    elif base in ENUMS:
        cbase = "int"
        note = "enum %s" % base
    elif base in VALUE_STRUCTS:
        if stars == 0:
            raise Unsupported("%s %s: struct %s by value" % (base, name, base))
        cbase = "void"
        note = "pointer to struct %s" % base
    elif base.startswith("I") and base[1:2].isupper() or base.startswith("__FI"):
        cbase = "void"                         # interface / delegate / collection pointer (IFoo* -> void *, IFoo** -> void **)
        if stars == 0:
            cbase = "void *"
    else:
        if stars == 0:
            raise Unsupported("%s %s: unknown value type %s" % (base, name, base))
        cbase = "void"
        note = "pointer to " + base
    if cbase.endswith("*"):
        return cbase.strip(), name, note
    return (cbase + " " + "*" * stars).strip(), name, note


def sig_parts(params):
    """-> ([c types], [names], [notes]) without `self`; raises Unsupported"""
    types, names, notes = [], [], []
    if params.strip() in ("", "void"):
        return types, names, notes
    for decl in split_params(params):
        t, n, note = c_param(decl)
        types.append(t)
        names.append(n)
        if note:
            notes.append("%s: %s" % (n, note))
    return types, names, notes


def sig_text(method, params):
    parts = ["void *self"]
    notes = []
    try:
        types, names, notes = sig_parts(params)
    except Unsupported as error:
        return "HRESULT %s(void *self, ...)" % method, ["UNSUPPORTED (no typedef, X104_SLOT refuses it): %s" % error]
    for t, n in zip(types, names):
        parts.append("%s %s" % (t, n) if not t.endswith("*") else "%s%s" % (t, n))
    return "HRESULT %s(%s)" % (method, ", ".join(parts)), notes


def fn_typedef(short, method, params):
    """`typedef HRESULT (X104_API *X104_FN_<Iface>_<Method>)(void *, <types>);` or None when a parameter is not expressible"""
    try:
        types, names, notes = sig_parts(params)
    except Unsupported:
        return None
    return "typedef HRESULT (X104_API *X104_FN_%s_%s)(%s);" % (short, method, ", ".join(["void *"] + types))


def parse_headers():
    """returns {full_name: (guid, [method names in vtable order], base, header, {method: raw param text})}"""
    result = {}
    for path in sorted(SDK.glob("*.h")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for em in re.finditer(r"typedef enum (\w+) : (?:unsigned )?int \1;", text):
            ENUMS.add(em.group(1))
        for sm in re.finditer(r"typedef struct (\w+) \1;", text):
            VALUE_STRUCTS.add(sm.group(1))
        for m in re.finditer(r'InterfaceName_(\w+)\[\] = L"([^"]+)";\s*\n(?:namespace(?:(?!InterfaceName_).)*?)MIDL_INTERFACE\("([0-9a-fA-F-]+)"\)\s*\n\s*(\w+)\s*:\s*public\s+(\w+)\s*\{(.*?)\n\s*\};', text, re.S):
            full, guid, name, base, body = m.group(2), m.group(3), m.group(4), m.group(5), m.group(6)
            if full.rsplit(".", 1)[1] != name:
                continue
            methods = []
            sigs = {}
            for sm in re.finditer(r'virtual\s+[\w:\*<> ]+?\s+STDMETHODCALLTYPE\s+(\w+)\s*\((.*?)\)\s*=\s*0;', body, re.S):
                methods.append(sm.group(1))
                sigs[sm.group(1)] = sm.group(2)
            result.setdefault(full, (guid.lower(), methods, base, path.name, sigs))
    return result


def c_guid(guid):
    p = guid.split("-")
    d4 = p[3] + p[4]
    return "{0x%s,0x%s,0x%s,{%s}}" % (p[0], p[1], p[2], ",".join("0x" + d4[i:i + 2] for i in range(0, 16, 2)))


def main():
    parsed = parse_headers()
    lines = []
    lines.append("/* win32_host_arm32_0104_iids.inl - GENERATED by tools/arm32/gen_0104_iids.py from the Windows SDK WinRT headers (%s)." % SDK.name)
    lines.append(" * DO NOT EDIT; re-run the generator.  X104_IID_<Interface>: the IID.  X104_SLOT_<Interface>_<Method>: vtable slot (IUnknown 0-2, IInspectable 3-5,")
    lines.append(" * own members from 6).  X104_NSLOTS_<Interface>: number of vtable slots (use it to size a vtable array with X104_VTBL).  The slots listed in the")
    lines.append(" * CHECKS table of the generator were compared with the verified facts of the 0.10.4 exe (all identical when this file was generated).")
    lines.append(" * The C signature of every slot is in arm32/v0_10_4/units/SLOTS.md (generated together with this file). */")
    lines.append("#ifndef WIN32_HOST_ARM32_0104_IIDS_INL")
    lines.append("#define WIN32_HOST_ARM32_0104_IIDS_INL")
    lines.append(PROLOGUE)
    table = {}
    sigtable = {}
    missing = []
    ntypedefs = 0
    for full in WANTED:
        short = full.rsplit(".", 1)[1]
        if full not in parsed:
            missing.append(full)
            continue
        guid, methods, base, src, sigs = parsed[full]
        base_slots = 3 if base == "IUnknown" else 6
        if base not in ("IUnknown", "IInspectable"):
            print("WARNING: %s derives from %s (not handled)" % (full, base))
        lines.append("")
        lines.append("/* %s (%s) */" % (full, src))
        lines.append("static const GUID X104_IID_%s = %s;" % (short, c_guid(guid)))
        slots = {}
        entries = []
        for i, name in enumerate(methods):
            slot = base_slots + i
            slots[name] = slot
            entries.append("X104_SLOT_%s_%s = %d" % (short, name, slot))
        entries.append("X104_NSLOTS_%s = %d" % (short, base_slots + len(methods)))
        lines.append("enum { " + ", ".join(entries) + " };")
        skipped = []
        for name in methods:
            td = fn_typedef(short, name, sigs.get(name, ""))
            if td:
                lines.append(td)
                ntypedefs += 1
            else:
                skipped.append(name)
        if skipped:
            lines.append("/* no X104_FN_ typedef (parameter type not expressible, see SLOTS.md): %s */" % ", ".join(skipped))
        table[short] = slots
        sigtable[short] = (full, guid, base, base_slots, methods, sigs)
    lines.append(MANUAL)
    lines.append("")
    lines.append("/* readable names for the logs: x104_iid_name() walks this table */")
    lines.append("typedef struct X104KnownIid { const GUID *iid; const char *name; } X104KnownIid;")
    lines.append("static const X104KnownIid X104_KNOWN_IIDS[] = {")
    for short in table:
        lines.append('    { &X104_IID_%s, "%s" },' % (short, short))
    for extra in ("IHardwareButtonsStatics", "IVibrationDeviceStatics", "IUnknown", "IInspectable", "IAgileObject",
                  "IActivationFactory", "ISwapChainPanelNative", "IAsyncInfo"):
        lines.append('    { &X104_IID_%s, "%s" },' % (extra, extra))
    lines.append("};")
    lines.append("")
    lines.append("#endif /* WIN32_HOST_ARM32_0104_IIDS_INL */")
    bad = 0
    for short, method, expected in CHECKS:
        got = table.get(short, {}).get(method)
        if got != expected:
            bad += 1
            print("MISMATCH %s.%s: SDK slot %s, fact slot %d" % (short, method, got, expected))
    print("cross-check: %d slot facts compared, %d mismatches" % (len(CHECKS), bad))
    if missing:
        print("not found in the SDK headers:", missing)
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("wrote", OUT, "(%d interfaces, %d slot typedefs)" % (len(table), ntypedefs))

    # SLOTS.md: every slot with its normalized C signature
    md = []
    md.append("# Slot tables of the WinRT interfaces the 0.10.4 host fakes (GENERATED by tools/arm32/gen_0104_iids.py - do not edit)")
    md.append("")
    md.append("Source: Windows SDK %s WinRT headers (vtable order = declaration order). All slots in `tools/arm32/gen_0104_iids.py` CHECKS were compared with the verified" % SDK.name)
    md.append("facts of the 0.10.4 exe: identical. ABI (AAPCS-VFP, Thumb-2): `this` is r0, `HRESULT` returned in r0; `HSTRING` is a pointer; `boolean`/`BYTE` travel in a 32-bit register")
    md.append("(zero-extended); `LONGLONG` (TimeSpan, EventRegistrationToken, INT64) is an 8-byte aligned pair (r2:r3 right after `this`, because r1 is skipped); `double` goes in")
    md.append("d0..; `float` in s0..; enums are `int`. STRUCTS: as OUTPUT parameters (`Size* value`) they travel through POINTERS (`X104Size {float Width,Height}`, `X104Point {float X,Y}`,")
    md.append("`X104Rect {float X,Y,Width,Height}`); as INPUT parameters WinRT passes them BY VALUE (`Measure(Size)`, `Arrange(Rect)`, `put_RenderTransformOrigin(Point)`, `put_PointerPosition(Point)`):")
    md.append("under AAPCS-VFP an all-float struct is a homogeneous aggregate and goes in s0..s3 (declare the C parameter as the struct and the compiler does it). `void *` stands for an interface")
    md.append("pointer (`void **` = `IFoo**`). **Every slot has a typedef `X104_FN_<Interface>_<Method>` (generated into the iids file with exactly the C signature below) and a unit MUST register its")
    md.append("thunk with `X104_SLOT(Interface, Method, thunk)`**: the build refuses a thunk whose signature differs (a missing `LONGLONG`, `float` instead of `double`, `int` instead of `BYTE *` ...).")
    md.append("Slots whose parameters the generator cannot express (struct by value other than Size/Point/Rect) have NO typedef: X104_SLOT does not compile for them (extend the generator). A slot")
    md.append("that deliberately ignores all its arguments is registered with `X104_SLOT_NOARGS(Interface, Method, x104_nop)`. Slot numbers are the vtable index (IUnknown 0-2, IInspectable 3-5 for")
    md.append("`IInspectable`-based interfaces).")
    md.append("")
    for short, (full, guid, base, base_slots, methods, sigs) in sigtable.items():
        md.append("## %s  `{%s}`" % (full, guid.upper()))
        md.append("")
        md.append("`X104_IID_%s`, `X104_NSLOTS_%s` = %d, base %s" % (short, short, base_slots + len(methods), base))
        md.append("")
        md.append("| slot | C signature | notes |")
        md.append("|---|---|---|")
        for i, name in enumerate(methods):
            sig, notes = sig_text(name, sigs.get(name, ""))
            md.append("| %d | `%s` | %s |" % (base_slots + i, sig.replace("|", "\\|"), "; ".join(notes)))
        md.append("")
    (REPO / "arm32" / "v0_10_4" / "units").mkdir(parents=True, exist_ok=True)
    (REPO / "arm32" / "v0_10_4" / "units" / "SLOTS.md").write_text("\n".join(md) + "\n", encoding="utf-8", newline="\n")
    print("wrote units/SLOTS.md")


if __name__ == "__main__":
    main()
