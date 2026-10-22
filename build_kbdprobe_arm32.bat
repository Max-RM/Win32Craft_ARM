@echo off
setlocal EnableExtensions

rem Builds kbdprobe.exe (ARM32 GUI lab for the Windows touch keyboard / TabTip, freestanding, no CRT).  Output: %DIST%\kbdprobe.exe
rem Overridable: LLVM_BIN SDKROOT SDKVER MSVCINC BUILD DIST

cd /d "%~dp0"
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined SDKVER set "SDKVER=10.0.19041.0"
if not defined SDKROOT set "SDKROOT=D:\Windows Kits\10"
if not defined MSVCINC set "MSVCINC=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\include"
if not defined BUILD set "BUILD=build-arm32"
if not defined DIST set "DIST=dist-arm32"
set "UM=%SDKROOT%\Lib\%SDKVER%\um\arm"
set "INC=%SDKROOT%\Include\%SDKVER%"
set "LINK="

if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1
if exist "%DIST%\kbdprobe.exe" del /q "%DIST%\kbdprobe.exe"

"%LLVM_BIN%\clang.exe" --target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wno-microsoft-cast -D_WIN32_WINNT=0x0603 -DWINVER=0x0603 -DCOBJMACROS -isystem "%MSVCINC%" -isystem "%INC%\um" -isystem "%INC%\shared" -isystem "%INC%\ucrt" -c tools\kbdprobe\kbdprobe.c -o "%BUILD%\kbdprobe.obj" || exit /b 1
"%LLVM_BIN%\lld-link.exe" /machine:arm /subsystem:windows,6.02 /entry:probe_start /nodefaultlib /brepro /out:"%DIST%\kbdprobe.exe" "%BUILD%\kbdprobe.obj" "%UM%\kernel32.lib" "%UM%\user32.lib" "%UM%\gdi32.lib" "%UM%\ole32.lib" "%UM%\uuid.lib" "%UM%\armrt.lib" || exit /b 1
echo Built %DIST%\kbdprobe.exe
"%LLVM_BIN%\llvm-readobj.exe" --coff-imports "%DIST%\kbdprobe.exe" | findstr /i /c:"Name:"
exit /b 0
