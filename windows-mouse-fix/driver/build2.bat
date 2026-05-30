@echo off
setlocal

set WDK_ROOT=C:\Program Files (x86)\Windows Kits\10
set WDK_VER=10.0.22621.0
set UMDF_VER=2.33
set WDK_INC=%WDK_ROOT%\Include\%WDK_VER%
set WDF_INC=%WDK_ROOT%\Include\wdf\umdf\%UMDF_VER%
set WDF_LIB=%WDK_ROOT%\Lib\wdf\umdf\arm64\%UMDF_VER%
set WDK_LIB=%WDK_ROOT%\Lib\%WDK_VER%

for /f "delims=" %%i in ('dir /b /ad "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"') do set MSVC_VER=%%i
set MSVC_LIB=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\%MSVC_VER%\lib\arm64

set OUTDIR=arm64\Release
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\%MSVC_VER%\bin\HostARM64\arm64;%PATH%
set PATH=C:\Program Files (x86)\Windows Kits\10\bin\%WDK_VER%\arm64;%PATH%

set MSVC_INC=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\%MSVC_VER%\include

echo Compiling WmfVirtualPad.c...
cl.exe /nologo /c /MD /D_ARM64_ /DARM64 /DWIN32_LEAN_AND_MEAN=1 /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DWINNT=1 /DNTDDI_VERSION=0x0A00000C /D_WINDLL /D_UNICODE /DUNICODE /DUMDF_VERSION_MAJOR=2 /DUMDF_VERSION_MINOR=33 /DUMDF_USING_NTSTATUS /I"%WDF_INC%" /I"%WDK_INC%\shared" /I"%WDK_INC%\um" /I"%WDK_INC%\ucrt" /I"%WDK_INC%\km" /I"%MSVC_INC%" /I"..\shared" /FI"%WDK_INC%\shared\warning.h" /W3 /WX- /O2 /GS- /Gy /Fo"%OUTDIR%\WmfVirtualPad.obj" WmfVirtualPad.c
if %ERRORLEVEL% NEQ 0 (echo COMPILE FAILED & exit /b 1)

echo Linking...
link.exe /nologo /DLL /OUT:"%OUTDIR%\WmfVirtualPad.dll" /MACHINE:ARM64 /SUBSYSTEM:WINDOWS /NODEFAULTLIB "%OUTDIR%\WmfVirtualPad.obj" "%WDF_LIB%\WdfDriverStubUm.lib" "%WDK_LIB%\um\arm64\mincore.lib" "%WDK_LIB%\um\arm64\ntdllp.lib" "%MSVC_LIB%\msvcrt.lib" "%MSVC_LIB%\vcruntime.lib" "%OUTDIR%\ucrt.lib"
if %ERRORLEVEL% NEQ 0 (echo LINK FAILED & exit /b 1)

echo BUILD SUCCEEDED
dir "%OUTDIR%\WmfVirtualPad.dll"
