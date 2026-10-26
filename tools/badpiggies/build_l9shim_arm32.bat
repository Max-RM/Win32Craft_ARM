@echo off
setlocal EnableExtensions

rem Builds l9shim.dll (ARM32 Thumb-2, the Bad Piggies feature-level-9_1 D3D11 shim).  Output: %DIST%\l9shim.dll
rem Overridable: LLVM_BIN SDKROOT SDKVER MSVCINC BUILD DIST

cd /d "%~dp0..\.."
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined SDKVER set "SDKVER=10.0.19041.0"
if not defined SDKROOT set "SDKROOT=D:\Windows Kits\10"
if not defined MSVCINC set "MSVCINC=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\include"
if not defined BUILD set "BUILD=build-badpiggies\obj-arm32"
if not defined DIST set "DIST=dist-badpiggies"
set "UM=%SDKROOT%\Lib\%SDKVER%\um\arm"
set "INC=%SDKROOT%\Include\%SDKVER%"

if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1
if exist "%DIST%\l9shim.dll" del /q "%DIST%\l9shim.dll"

"%LLVM_BIN%\clang.exe" --target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wno-microsoft-cast -Wno-ignored-attributes -D_WIN32_WINNT=0x0603 -DWINVER=0x0603 -isystem "%MSVCINC%" -isystem "%INC%\um" -isystem "%INC%\shared" -isystem "%INC%\ucrt" -c tools\badpiggies\l9shim.c -o "%BUILD%\l9shim.obj" || exit /b 1
"%LLVM_BIN%\lld-link.exe" /dll /machine:arm /subsystem:windows,6.02 /entry:DllMain /nodefaultlib /brepro /def:tools\badpiggies\l9shim.def /out:"%DIST%\l9shim.dll" "%BUILD%\l9shim.obj" "%UM%\kernel32.lib" "%UM%\armrt.lib" || exit /b 1
echo Built %DIST%\l9shim.dll
"%LLVM_BIN%\llvm-readobj.exe" --coff-exports "%DIST%\l9shim.dll" | findstr /i /c:"Name:"
"%LLVM_BIN%\llvm-readobj.exe" --coff-imports "%DIST%\l9shim.dll" | findstr /i /c:"Name:"
exit /b 0
