@echo off
setlocal EnableExtensions

rem Builds m0probe_x64.exe: the PC proof build of tools\m0probe\m0probe.c (same single source as the ARM32 build, freestanding, no CRT).
rem Output: %BUILD%\m0probe_x64.exe   (never ship this one; the tablets get dist-m0probe\m0probe.exe)
rem Overridable: VC SDK SV BUILD

cd /d "%~dp0..\.."
if not defined VC set "VC=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207"
if not defined SDK set "SDK=D:\Windows Kits\10"
if not defined SV set "SV=10.0.19041.0"
if not defined BUILD set "BUILD=build-x64probe"
set "INCLUDE=%VC%\include;%SDK%\Include\%SV%\ucrt;%SDK%\Include\%SV%\um;%SDK%\Include\%SV%\shared"
set "LIB=%VC%\lib\x64;%SDK%\Lib\%SV%\um\x64"

if not exist "%BUILD%" mkdir "%BUILD%" || exit /b 1
if exist "%BUILD%\m0probe_x64.exe" del /q "%BUILD%\m0probe_x64.exe"

"%VC%\bin\Hostx64\x64\cl.exe" /nologo /W3 /O1 /GS- /Zl /Gy /D_WIN32_WINNT=0x0603 /DWINVER=0x0603 tools\m0probe\m0probe.c /Fo"%BUILD%\\" /Fe"%BUILD%\m0probe_x64.exe" /link /NODEFAULTLIB /ENTRY:mainCRTStartup /SUBSYSTEM:CONSOLE,6.02 /INCREMENTAL:NO kernel32.lib user32.lib dxguid.lib || exit /b 1
echo Built %BUILD%\m0probe_x64.exe
exit /b 0
