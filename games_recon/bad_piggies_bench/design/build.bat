@echo off
set VC=D:\Program Files\vs22buildtools\VC\Tools\MSVC\14.44.35207
set SDK=D:\Windows Kits\10
set SV=10.0.19041.0
set INCLUDE=%VC%\include;%SDK%\Include\%SV%\ucrt;%SDK%\Include\%SV%\um;%SDK%\Include\%SV%\shared
set LIB=%VC%\lib\x64;%SDK%\Lib\%SV%\ucrt\x64;%SDK%\Lib\%SV%\um\x64
cd /d "%~dp0"
"%VC%\bin\Hostx64\x64\cl.exe" /nologo /W3 /O1 /D_CRT_SECURE_NO_WARNINGS l9bench.c /Fe:l9bench.exe d3d11.lib dxgi.lib dxguid.lib user32.lib
