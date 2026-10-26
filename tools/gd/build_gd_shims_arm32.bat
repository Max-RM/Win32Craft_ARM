@echo off
setlocal EnableExtensions

rem Builds the Geometry Dash dependency shim DLLs (ARM32 Thumb-2, freestanding, no CRT) into %DIST%:
rem   PhoneAppModelHost.dll  (CaptureUiThreadContext / LoadPackagedLibrary / LaunchEmergencyDialer)
rem   PhoneAudioSes.dll      (ActivateAudioInterface / GetDefaultAudioRenderId / GetDefaultAudioCaptureId, on mmdevapi)
rem Same toolchain as build_m0probe_arm32.bat / build_l9shim_arm32.bat.
rem Overridable: LLVM_BIN SDKROOT SDKVER MSVCINC BUILD DIST

cd /d "%~dp0..\.."
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined SDKVER set "SDKVER=10.0.19041.0"
if not defined SDKROOT set "SDKROOT=D:\Windows Kits\10"
if not defined MSVCINC set "MSVCINC=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\include"
if not defined BUILD set "BUILD=build-gd\obj-arm32"
if not defined DIST set "DIST=dist-gd"
set "UM=%SDKROOT%\Lib\%SDKVER%\um\arm"
set "INC=%SDKROOT%\Include\%SDKVER%"
set "CFLAGS=--target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wno-microsoft-cast -D_WIN32_WINNT=0x0603 -DWINVER=0x0603 -isystem "%MSVCINC%" -isystem "%INC%\um" -isystem "%INC%\shared" -isystem "%INC%\ucrt""

if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1

rem --- PhoneAppModelHost.dll ---
if exist "%DIST%\PhoneAppModelHost.dll" del /q "%DIST%\PhoneAppModelHost.dll"
"%LLVM_BIN%\clang.exe" %CFLAGS% -c tools\gd\phoneappmodelhost.c -o "%BUILD%\phoneappmodelhost.obj" || exit /b 1
"%LLVM_BIN%\lld-link.exe" /dll /machine:arm /subsystem:windows,6.02 /entry:DllMain /nodefaultlib /brepro /def:tools\gd\phoneappmodelhost.def /out:"%DIST%\PhoneAppModelHost.dll" "%BUILD%\phoneappmodelhost.obj" "%UM%\kernel32.lib" "%UM%\armrt.lib" || exit /b 1
echo Built %DIST%\PhoneAppModelHost.dll
"%LLVM_BIN%\llvm-readobj.exe" --coff-exports "%DIST%\PhoneAppModelHost.dll" | findstr /i /c:"Name:"

rem --- PhoneAudioSes.dll (built only if the source exists yet) ---
if exist tools\gd\phoneaudioses.c (
  if exist "%DIST%\PhoneAudioSes.dll" del /q "%DIST%\PhoneAudioSes.dll"
  "%LLVM_BIN%\clang.exe" %CFLAGS% -c tools\gd\phoneaudioses.c -o "%BUILD%\phoneaudioses.obj" || exit /b 1
  "%LLVM_BIN%\lld-link.exe" /dll /machine:arm /subsystem:windows,6.02 /entry:DllMain /nodefaultlib /brepro /def:tools\gd\phoneaudioses.def /out:"%DIST%\PhoneAudioSes.dll" "%BUILD%\phoneaudioses.obj" "%UM%\kernel32.lib" "%UM%\ole32.lib" "%UM%\armrt.lib" || exit /b 1
  echo Built %DIST%\PhoneAudioSes.dll
  "%LLVM_BIN%\llvm-readobj.exe" --coff-exports "%DIST%\PhoneAudioSes.dll" | findstr /i /c:"Name:"
)
exit /b 0
