Win32Craft source package
=========================

Win32Craft runs UWP / WinRT Minecraft Bedrock / PE builds as desktop Win32 programs: a host DLL (win32craft.dll, win32_host.c) plus a patched game exe.
This tree contains the x86 host (Windows 7 / Wine, versions 0.13 - 1.16) and the ARM32 ports (Windows RT 8.1 / Windows 10 ARM32): Minecraft 1.2.8 (CoreWindow app) and
Minecraft PE 0.10.4.40 (Windows Phone 8.1 XAML app). ONE win32craft.dll serves both ARM32 versions (chosen at runtime by PE machine + patched SizeOfImage).

Start here
  HANDOFF_ARM32.md              what is done, decisions, device results, open issues, how to continue
  ARM32.md                      user-facing notes and options of the ARM32 host
  arm32/PORT_RULES.md, arm32/RE_GUIDE.md     rules and the reverse-engineering toolkit guide
  arm32/OSK_NOTES.md            touch keyboard (TabTip) device facts + the controller
  arm32/v0_10_4/                0.10.4.40: RECON_SUMMARY.md, HOST_DESIGN.md, INTEGRATION_NOTES.md, HUD_INVESTIGATION.md, PATCHER_NOTES.md, facts/, units/
  arm32/v1_16_40/               1.16.40.02 groundwork (PARKED)
  arm32/device_logs/            logs from real devices (Surface RT 8.1, Surface 2 Windows 10)
  whatsapp/README_WHATSAPP.md, tools/whatsapp/   next app (Sup, a Windows Phone 8.1 XAML app): recon + the package patcher (the app itself is third-party and not included)
  wp81/WP81_APP_METHOD.md, tools/wp81/wp81scan.py   the method for abandoned Windows Phone 8.1 apps (recon tool + step-by-step guide)

Build (Windows, LLVM for Windows RT + VS2022 Build Tools + Windows SDK 10.0.19041; paths are overridable environment variables, see the .bat headers)
  build_arm32.bat                         -> dist-arm32\win32craft.dll (ARM32, Thumb-2)
  python tools\arm32-patcher.py ...        table-driven exe patcher (profiles v0_10_4, v1_2_8; v1_16_40 placeholder)
  python tools\make_arm32_device_package.py   1.2.8 device folder + zip      (needs the original Minecraft_1.2.8.0_ARM32 folder next to the sources)
  python tools\make_0104_package.py --build   0.10.4.40 device folder + zip  (needs the original 0.10.4.40 folder: see DEFAULT_GAME in the script or WIN32CRAFT_GAME_DIR_0104)
  python tools\arm32\check_x86.py            proves the x86 build is unchanged by the ARM32 code
  python tools\arm32\try_0104.py --selftest --integration   0.10.4 host unit + whole-tree tests on the PC
The original game folders are NOT part of this archive (copyrighted).  The generated device packages (zips) are delivered separately.

Heredoc note for tool-driven editing: shell heredocs collapse backslashes - write scripts that contain C strings with backslashes with a file editor.
