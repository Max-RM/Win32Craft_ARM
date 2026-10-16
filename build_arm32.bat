@echo off
setlocal EnableExtensions

rem Native Windows build of win32craft.dll for ARM32 (Thumb-2, hard-float) with the Windows RT LLVM toolchain.
rem Run from cmd.exe.  Output: %DIST%\win32craft.dll
rem
rem Overridable environment variables (defaults point at the original developer machine):
rem   LLVM_BIN   clang/lld-link directory     D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin
rem   VCLIB      MSVC ARM lib directory       D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\arm
rem   SDKVER     Windows SDK version          10.0.19041.0
rem   SDKROOT    Windows Kits\10 directory    D:\Windows Kits\10
rem   BUILD DIST object / output directories

cd /d "%~dp0"
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined VCLIB set "VCLIB=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207\lib\arm"
if not defined SDKVER set "SDKVER=10.0.19041.0"
if not defined SDKROOT set "SDKROOT=D:\Windows Kits\10"
if not defined BUILD set "BUILD=build-arm32"
if not defined DIST set "DIST=dist-arm32"
if not defined CC set "CC=%LLVM_BIN%\clang.exe"
if not defined LINKER set "LINKER=%LLVM_BIN%\lld-link.exe"
set "UM=%SDKROOT%\Lib\%SDKVER%\um\arm"
set "UCRT=%SDKROOT%\Lib\%SDKVER%\ucrt\arm"
rem lld-link treats a LINK environment variable as extra command-line arguments
set "LINK="

for %%F in ("%CC%" "%LINKER%" "%UM%\kernel32.lib" "%UM%\armrt.lib" "%VCLIB%\libcmt.lib") do (
  if not exist %%F (
    echo ERROR: %%F not found - set LLVM_BIN / VCLIB / SDKROOT / SDKVER.
    exit /b 1
  )
)

rem Never delete whole directories: BUILD/DIST are overridable.
if exist "%BUILD%\win32_host.obj" del /q "%BUILD%\win32_host.obj"
if exist "%BUILD%\api_compat.obj" del /q "%BUILD%\api_compat.obj"
if exist "%DIST%\win32craft.dll" del /q "%DIST%\win32craft.dll"
if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1

set "CFLAGS=--target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -fno-stack-protector -fno-builtin -ffunction-sections -I. -Wno-incompatible-pointer-types -Wno-microsoft-cast -Wno-ignored-attributes -Wno-comment"

"%CC%" %CFLAGS% -c win32_host.c -o "%BUILD%\win32_host.obj" || exit /b 1
"%CC%" %CFLAGS% -c api_compat.c -o "%BUILD%\api_compat.obj" || exit /b 1

rem armrt.lib supplies the __rt_* division helpers, libcmt.lib the GS support those helpers reference.
"%LINKER%" /dll /machine:arm /entry:DllMain /def:win32craft_arm32.def ^
  /out:"%DIST%\win32craft.dll" /implib:"%BUILD%\win32craft.lib" ^
  "%BUILD%\win32_host.obj" "%BUILD%\api_compat.obj" ^
  "%UM%\kernel32.lib" "%UM%\user32.lib" "%UM%\shell32.lib" "%UM%\comdlg32.lib" ^
  "%UM%\armrt.lib" "%VCLIB%\libcmt.lib" "%VCLIB%\legacy_stdio_definitions.lib" "%VCLIB%\legacy_stdio_wide_specifiers.lib" "%UCRT%\ucrt.lib" "%VCLIB%\vcruntime.lib" ^
  /nodefaultlib /subsystem:windows,6.02 /nxcompat /brepro /stack:0x200000,0x1000 /opt:ref /opt:icf || exit /b 1

echo.
echo Build completed: %CD%\%DIST%\win32craft.dll
"%LLVM_BIN%\llvm-readobj.exe" --file-headers "%DIST%\win32craft.dll" | findstr /i /c:"Machine" /c:"Characteristics"
exit /b 0
