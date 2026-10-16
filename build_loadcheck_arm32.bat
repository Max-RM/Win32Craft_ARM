@echo off
setlocal EnableExtensions

rem Builds loadcheck.exe (ARM32 console diagnostic, imports only KERNEL32/USER32).  Output: %DIST%\loadcheck.exe
rem Overridable: LLVM_BIN SDKROOT SDKVER BUILD DIST

cd /d "%~dp0"
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined SDKVER set "SDKVER=10.0.19041.0"
if not defined SDKROOT set "SDKROOT=D:\Windows Kits\10"
if not defined BUILD set "BUILD=build-arm32"
if not defined DIST set "DIST=dist-arm32"
set "UM=%SDKROOT%\Lib\%SDKVER%\um\arm"
set "LINK="

if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1
if exist "%DIST%\loadcheck.exe" del /q "%DIST%\loadcheck.exe"

"%LLVM_BIN%\clang.exe" --target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -c tools\loadcheck\loadcheck.c -o "%BUILD%\loadcheck.obj" || exit /b 1
"%LLVM_BIN%\lld-link.exe" /machine:arm /subsystem:console,6.02 /entry:mainCRTStartup /nodefaultlib /brepro /out:"%DIST%\loadcheck.exe" "%BUILD%\loadcheck.obj" "%UM%\kernel32.lib" "%UM%\user32.lib" "%UM%\armrt.lib" || exit /b 1
echo Built %DIST%\loadcheck.exe
"%LLVM_BIN%\llvm-readobj.exe" --coff-imports "%DIST%\loadcheck.exe" | findstr /i /c:"Name:"
exit /b 0
