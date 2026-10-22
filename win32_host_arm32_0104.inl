/* win32_host_arm32_0104.inl - umbrella of the Minecraft PE 0.10.4.40 (Windows Phone 8.1 XAML) ARM32 host.
 *
 * Included by win32_host.c under WIN32CRAFT_ARM32, directly in front of Win32Bootstrap (every static helper it uses - log_line, the
 * arm32_iat_ and arm32_patch_ helpers, the TabTip controller, the arm32_boot_* helpers - is defined above that point).
 * Design, rules, threading model, cross-unit API: arm32/v0_10_4/HOST_DESIGN.md.  Unit briefs: arm32/v0_10_4/units/<unit>.md.
 *
 * File ownership (each file has exactly ONE owner; nobody edits another unit's file):
 *   win32_host_arm32_0104_iids.inl     GENERATED (tools/arm32/gen_0104_iids.py)
 *   win32_host_arm32_0104_core.inl     designer: toolkit, global state, cross-unit prototypes
 *   win32_host_arm32_0104_async.inl    async unit    (ThreadPool, IAsyncAction/IAsyncInfo, CoreDispatcher, DispatcherTimer, UI loop)
 *   win32_host_arm32_0104_app.inl      app unit      (Application, Page, Window, Uri, StatusBar, Canvas, HardwareButtons, DisplayInformation, lifecycle)
 *   win32_host_arm32_0104_panel.inl    panel unit    (SwapChainPanel, D3D11CreateDevice hook, shader profile patch, presentation, WM_SIZE)
 *   win32_host_arm32_0104_platform.inl platform unit (Package, ApplicationData, MemoryManager, Vibration, Network, Store, tracer, paths, cells)
 *   win32_host_arm32_0104_input.inl    input unit    (CoreWindow, pointer/key/text, XAML elements, InputPane, TabTip, window messages)
 *   win32_host_arm32_0104_selftest.inl designer: on-device self test of the fakes (--selftest-fakes); core + unit contract functions only
 *   win32_host_arm32_0104_boot.inl     designer: identification, options, IAT hooks, VC120 diagnostics, window, x104_bootstrap
 * Order matters: the core declares the cross-unit prototypes, so the units may call each other regardless of their order.  The boot is included LAST:
 * no unit may reference anything defined in it (shared constants and helpers live in the core; try_0104.py lints this). */
#ifndef WIN32_HOST_ARM32_0104_INL
#define WIN32_HOST_ARM32_0104_INL

#include "win32_host_arm32_0104_core.inl"
#include "win32_host_arm32_0104_async.inl"
#include "win32_host_arm32_0104_app.inl"
#include "win32_host_arm32_0104_panel.inl"
#include "win32_host_arm32_0104_platform.inl"
#include "win32_host_arm32_0104_input.inl"
#include "win32_host_arm32_0104_selftest.inl"
#include "win32_host_arm32_0104_boot.inl"

#endif /* WIN32_HOST_ARM32_0104_INL */
