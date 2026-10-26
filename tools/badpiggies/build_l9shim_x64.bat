@echo off
setlocal EnableExtensions

rem Builds l9shim_x64.dll (PC test build of the Bad Piggies FL 9_1 D3D11 shim) + the test harness l9shim_test_x64.exe.
rem Overridable: LLVM_BIN SDKROOT SDKVER MSVCINC

cd /d "%~dp0..\.."
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined SDKVER set "SDKVER=10.0.19041.0"
if not defined SDKROOT set "SDKROOT=D:\Windows Kits\10"
if not defined MSVCINC set "MSVCINC=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\include"
set "INC=%SDKROOT%\Include\%SDKVER%"
set "UM=%SDKROOT%\Lib\%SDKVER%\um\x64"
set "BUILD=build-badpiggies"

if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1

"%LLVM_BIN%\clang.exe" --target=x86_64-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wno-microsoft-cast -Wno-ignored-attributes -D_WIN32_WINNT=0x0603 -DWINVER=0x0603 -isystem "%MSVCINC%" -isystem "%INC%\um" -isystem "%INC%\shared" -isystem "%INC%\ucrt" -c tools\badpiggies\l9shim.c -o "%BUILD%\l9shim_x64.obj" || exit /b 1
"%LLVM_BIN%\lld-link.exe" /dll /machine:x64 /entry:DllMain /nodefaultlib /def:tools\badpiggies\l9shim.def /out:"%BUILD%\l9shim_x64.dll" "%BUILD%\l9shim_x64.obj" "%UM%\kernel32.lib" || exit /b 1
echo Built %BUILD%\l9shim_x64.dll

"%LLVM_BIN%\clang.exe" --target=x86_64-pc-windows-msvc -O1 -g0 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wno-microsoft-cast -Wno-ignored-attributes -D_WIN32_WINNT=0x0603 -DWINVER=0x0603 -isystem "%MSVCINC%" -isystem "%INC%\um" -isystem "%INC%\shared" -isystem "%INC%\ucrt" -c tools\badpiggies\l9shim_test.c -o "%BUILD%\l9shim_test.obj" || exit /b 1
"%LLVM_BIN%\lld-link.exe" /machine:x64 /subsystem:console /entry:hmain /nodefaultlib /out:"%BUILD%\l9shim_test_x64.exe" "%BUILD%\l9shim_test.obj" "%UM%\kernel32.lib" "%UM%\user32.lib" "%UM%\d3d11.lib" "%UM%\dxgi.lib" "%UM%\dxguid.lib" || exit /b 1
echo Built %BUILD%\l9shim_test_x64.exe
exit /b 0
