@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64 >nul 2>&1

echo.
echo === Building WmfVirtualPad UMDF2 driver (ARM64) ===
echo.

set WDK_ROOT=C:\Program Files (x86)\Windows Kits\10
set WDK_VER=10.0.22621.0
set UMDF_VER=2.33

set WDK_INC=%WDK_ROOT%\Include\%WDK_VER%
set WDK_LIB=%WDK_ROOT%\Lib\%WDK_VER%
set WDF_INC=%WDK_ROOT%\Include\wdf\umdf\%UMDF_VER%
set WDF_LIB=%WDK_ROOT%\Lib\wdf\umdf\arm64\%UMDF_VER%

set OUTDIR=arm64\Release
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Target: ARM64
echo WDF lib: %WDF_LIB%
echo.

cl.exe /nologo /c ^
  /D_ARM64_ /DARM64 ^
  /DWIN32_LEAN_AND_MEAN=1 ^
  /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DWINNT=1 /DNTDDI_VERSION=0x0A00000C ^
  /D_WINDLL /D_UNICODE /DUNICODE ^
  /DUMDF_VERSION_MAJOR=2 /DUMDF_VERSION_MINOR=33 /DUMDF_USING_NTSTATUS ^
  /I"%WDF_INC%" ^
  /I"%WDK_INC%\shared" ^
  /I"%WDK_INC%\um" ^
  /I"%WDK_INC%\ucrt" ^
  /I"%WDK_INC%\km" ^
  /I"..\shared" ^
  /FI"%WDK_INC%\shared\warning.h" ^
  /W4 /WX- /Ox /Os /GF /Gm- /MT /GS /Gy /Zc:wchar_t- /Zc:forScope /Zc:inline /GR- ^
  /Fo"%OUTDIR%\WmfVirtualPad.obj" ^
  WmfVirtualPad.c

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo COMPILE FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Linking...

:: Find the MSVC lib path for ARM64
for /f "delims=" %%i in ('dir /b /ad "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"') do set MSVC_VER=%%i
set MSVC_LIB=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\%MSVC_VER%\lib\arm64

link.exe /nologo /DLL ^
  /OUT:"%OUTDIR%\WmfVirtualPad.dll" ^
  /MACHINE:ARM64 ^
  /SUBSYSTEM:WINDOWS ^
  /ENTRY:DriverEntry ^
  /NODEFAULTLIB ^
  "%OUTDIR%\WmfVirtualPad.obj" ^
  "%WDF_LIB%\WdfDriverStubUm.lib" ^
  "%WDK_LIB%\um\arm64\mincore.lib" ^
  "%WDK_LIB%\um\arm64\ntdllp.lib" ^
  "%MSVC_LIB%\libcmt.lib" ^
  "%MSVC_LIB%\libvcruntime.lib"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo LINK FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo BUILD SUCCEEDED
echo Output: %OUTDIR%\WmfVirtualPad.dll
dir "%OUTDIR%\WmfVirtualPad.*"
