@echo off
setlocal EnableExtensions

rem Builds m0probe.exe (ARM32 console diagnostic, freestanding, no CRT).  Output: %DIST%\m0probe.exe
rem Same toolchain as build_kbdprobe_arm32.bat / build_loadcheck_arm32.bat (clang thumbv7-pc-windows-msvc, SDK libs for ARM, armrt.lib).
rem Imports: KERNEL32 + USER32 only (d3d11, dxgi, d3dcompiler_47, version, advapi32 are loaded at run time).
rem Overridable: LLVM_BIN SDKROOT SDKVER MSVCINC BUILD DIST
rem Then run  python tools\m0probe\make_m0probe_package.py  to add the shader data, run_probe.bat and README.txt to %DIST%.

cd /d "%~dp0..\.."
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined SDKVER set "SDKVER=10.0.19041.0"
if not defined SDKROOT set "SDKROOT=D:\Windows Kits\10"
if not defined MSVCINC set "MSVCINC=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\include"
if not defined BUILD set "BUILD=build-x64probe\obj-arm32"
if not defined DIST set "DIST=dist-m0probe"
set "UM=%SDKROOT%\Lib\%SDKVER%\um\arm"
set "INC=%SDKROOT%\Include\%SDKVER%"
set "LINK="

if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1
if exist "%DIST%\m0probe.exe" del /q "%DIST%\m0probe.exe"

"%LLVM_BIN%\clang.exe" --target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wno-microsoft-cast -D_WIN32_WINNT=0x0603 -DWINVER=0x0603 -isystem "%MSVCINC%" -isystem "%INC%\um" -isystem "%INC%\shared" -isystem "%INC%\ucrt" -c tools\m0probe\m0probe.c -o "%BUILD%\m0probe.obj" || exit /b 1
"%LLVM_BIN%\lld-link.exe" /machine:arm /subsystem:console,6.02 /entry:mainCRTStartup /nodefaultlib /brepro /out:"%DIST%\m0probe.exe" "%BUILD%\m0probe.obj" "%UM%\kernel32.lib" "%UM%\user32.lib" "%UM%\dxguid.lib" "%UM%\armrt.lib" || exit /b 1
echo Built %DIST%\m0probe.exe
"%LLVM_BIN%\llvm-readobj.exe" --coff-imports "%DIST%\m0probe.exe" | findstr /i /c:"Name:"
exit /b 0
