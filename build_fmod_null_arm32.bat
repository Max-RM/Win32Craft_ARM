@echo off
setlocal EnableExtensions

rem Builds the SILENT fmod_ARM.dll replacement (ARM32, Thumb-2, hard-float, no CRT, no imports).
rem Output: %DIST%\fmod_ARM.dll   (default dist-fmod-null-arm32\fmod_ARM.dll)
rem Run from cmd.exe.  Overridable environment variables:
rem   LLVM_BIN  directory holding clang.exe / lld-link.exe / llvm-readobj.exe
rem   CC LINKER READOBJ  full tool paths (override the LLVM_BIN lookup); LINK is accepted as an alias of LINKER
rem   (lld-link itself reads the LINK variable as extra arguments, so the script clears it before linking)
rem   BUILD DIST       object and output directories
rem   RUN_TESTS=1      also run arm32\tests\fmod_null_test.py afterwards (PYTHON overrides the interpreter)

cd /d "%~dp0"
if not defined LLVM_BIN set "LLVM_BIN=D:\repo\llvm-rt\scratch\RT23P\build-rt21\bin"
if not defined CC if exist "%LLVM_BIN%\clang.exe" set "CC=%LLVM_BIN%\clang.exe"
if not defined CC set "CC=clang"
if not defined LINKER if defined LINK set "LINKER=%LINK%"
if not defined LINKER if exist "%LLVM_BIN%\lld-link.exe" set "LINKER=%LLVM_BIN%\lld-link.exe"
if not defined LINKER set "LINKER=lld-link"
set "LINK="
if not defined READOBJ if exist "%LLVM_BIN%\llvm-readobj.exe" set "READOBJ=%LLVM_BIN%\llvm-readobj.exe"
if not defined READOBJ set "READOBJ=llvm-readobj"
if not defined BUILD set "BUILD=build-fmod-null-arm32"
if not defined DIST set "DIST=dist-fmod-null-arm32"
if not defined PYTHON set "PYTHON=python"

call :have "%CC%" || goto :no_cc
call :have "%LINKER%" || goto :no_link

rem Never delete whole directories: BUILD/DIST are overridable and DIST may point at a game folder.
if exist "%BUILD%\fmod_null.obj" del /q "%BUILD%\fmod_null.obj"
if exist "%DIST%\fmod_ARM.dll" del /q "%DIST%\fmod_ARM.dll"
if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1

rem Project ARM32 flags (arm32\PORT_RULES.md) plus -ffreestanding so clang never emits memset/memcpy calls.
set "CFLAGS=--target=thumbv7-pc-windows-msvc -march=armv7-a -mfpu=neon -mfloat-abi=hard -O2 -fno-stack-protector -fno-builtin -ffreestanding -Wall -Wextra"

"%CC%" %CFLAGS% -c fmod_null\fmod_null.c -o "%BUILD%\fmod_null.obj" || exit /b 1

rem No /defaultlib and no import libraries: the DLL has an empty import table.
rem DYNAMIC_BASE (the lld default, mandatory on ARM) keeps the base relocations.
"%LINKER%" /dll /machine:arm /nodefaultlib /noimplib /entry:DllMain ^
  /def:fmod_null\fmod_null_arm32.def /out:"%DIST%\fmod_ARM.dll" ^
  /subsystem:windows,6.02 /dynamicbase /nxcompat /opt:ref /opt:noicf ^
  "%BUILD%\fmod_null.obj" || exit /b 1

echo.
echo Built "%DIST%\fmod_ARM.dll"
call :have "%READOBJ%" || goto :skip_readobj
"%READOBJ%" --file-headers "%DIST%\fmod_ARM.dll" | findstr /i /c:"Machine" /c:"Characteristics"
echo --- exports ---
"%READOBJ%" --coff-exports "%DIST%\fmod_ARM.dll"
echo --- imports (expect none) ---
"%READOBJ%" --coff-imports "%DIST%\fmod_ARM.dll"
:skip_readobj

if not "%RUN_TESTS%"=="1" goto :done
"%PYTHON%" arm32\tests\fmod_null_test.py "%DIST%\fmod_ARM.dll" || exit /b 1

:done
endlocal
exit /b 0

:no_cc
echo ERROR: clang was not found. Set LLVM_BIN or CC.
exit /b 1

:no_link
echo ERROR: lld-link was not found. Set LLVM_BIN or LINKER.
exit /b 1

rem :have <tool> - succeeds if the tool is an existing file or is found in PATH
:have
if exist "%~1" exit /b 0
where "%~1" >nul 2>nul
exit /b %errorlevel%
