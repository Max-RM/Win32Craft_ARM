@echo off
setlocal EnableExtensions

rem Builds gd_host.exe (ARM32 console, Geometry Dash M1 headless host probe).  Output: %DIST%\gd_host.exe
rem Imports KERNEL32 only (d3d11.dll + combase.dll + GeometryJumpComponent.dll are loaded at run time).
rem Same toolchain as build_m0probe_arm32.bat.  Overridable: LLVM_BIN SDKROOT SDKVER MSVCINC BUILD DIST

cd /d "%~dp0..\.."
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined SDKVER set "SDKVER=10.0.19041.0"
if not defined SDKROOT set "SDKROOT=D:\Windows Kits\10"
if not defined MSVCINC set "MSVCINC=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\include"
if not defined BUILD set "BUILD=build-gd\obj-arm32"
if not defined DIST set "DIST=dist-gd"
set "UM=%SDKROOT%\Lib\%SDKVER%\um\arm"
set "INC=%SDKROOT%\Include\%SDKVER%"

if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1
if exist "%DIST%\gd_host.exe" del /q "%DIST%\gd_host.exe"

"%LLVM_BIN%\clang.exe" --target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wno-microsoft-cast -Wno-ignored-attributes -D_WIN32_WINNT=0x0603 -DWINVER=0x0603 -isystem "%MSVCINC%" -isystem "%INC%\um" -isystem "%INC%\shared" -isystem "%INC%\ucrt" -c tools\gd\gd_host.c -o "%BUILD%\gd_host.obj" || exit /b 1
"%LLVM_BIN%\lld-link.exe" /machine:arm /subsystem:console,6.02 /entry:mainCRTStartup /nodefaultlib /brepro /out:"%DIST%\gd_host.exe" "%BUILD%\gd_host.obj" "%UM%\kernel32.lib" "%UM%\user32.lib" "%UM%\armrt.lib" || exit /b 1
echo Built %DIST%\gd_host.exe
"%LLVM_BIN%\llvm-readobj.exe" --coff-imports "%DIST%\gd_host.exe" | findstr /i /c:"Name:"
exit /b 0
