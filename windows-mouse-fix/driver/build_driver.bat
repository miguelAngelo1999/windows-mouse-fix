@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64 >nul 2>&1

cd /d c:\Users\virgoh\windows-mouse-fix\windows-mouse-fix\driver

echo.
echo === Building WmfVirtualPad UMDF2 driver (ARM64) ===
echo.

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

:: Create a minimal ucrt.lib by generating an import lib from ucrtbase.dll
echo Generating ucrt import library...
echo LIBRARY ucrtbase > "%OUTDIR%\ucrt.def"
echo EXPORTS >> "%OUTDIR%\ucrt.def"
echo   _initterm >> "%OUTDIR%\ucrt.def"
echo   _initterm_e >> "%OUTDIR%\ucrt.def"
echo   _cexit >> "%OUTDIR%\ucrt.def"
echo   _crt_atexit >> "%OUTDIR%\ucrt.def"
echo   _crt_at_quick_exit >> "%OUTDIR%\ucrt.def"
echo   _seh_filter_dll >> "%OUTDIR%\ucrt.def"
echo   _configure_narrow_argv >> "%OUTDIR%\ucrt.def"
echo   _initialize_narrow_environment >> "%OUTDIR%\ucrt.def"
echo   _initialize_onexit_table >> "%OUTDIR%\ucrt.def"
echo   _register_onexit_function >> "%OUTDIR%\ucrt.def"
echo   _execute_onexit_table >> "%OUTDIR%\ucrt.def"
echo   terminate >> "%OUTDIR%\ucrt.def"
echo   abort >> "%OUTDIR%\ucrt.def"
echo   malloc >> "%OUTDIR%\ucrt.def"
echo   free >> "%OUTDIR%\ucrt.def"
echo   calloc >> "%OUTDIR%\ucrt.def"
echo   memset >> "%OUTDIR%\ucrt.def"
echo   memcpy >> "%OUTDIR%\ucrt.def"
echo   memmove >> "%OUTDIR%\ucrt.def"
echo   __stdio_common_vsprintf >> "%OUTDIR%\ucrt.def"
echo   __stdio_common_vsprintf_s >> "%OUTDIR%\ucrt.def"

lib /nologo /DEF:"%OUTDIR%\ucrt.def" /OUT:"%OUTDIR%\ucrt.lib" /MACHINE:ARM64
if %ERRORLEVEL% NEQ 0 (echo FAILED to create ucrt.lib & exit /b 1)

echo Compiling WmfVirtualPad.c...
cl.exe /nologo /c /MD /D_ARM64_ /DARM64 /DWIN32_LEAN_AND_MEAN=1 /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DWINNT=1 /DNTDDI_VERSION=0x0A00000C /D_WINDLL /D_UNICODE /DUNICODE /DUMDF_VERSION_MAJOR=2 /DUMDF_VERSION_MINOR=33 /DUMDF_USING_NTSTATUS /I"%WDF_INC%" /I"%WDK_INC%\shared" /I"%WDK_INC%\um" /I"%WDK_INC%\ucrt" /I"%WDK_INC%\km" /I"..\shared" /FI"%WDK_INC%\shared\warning.h" /W3 /WX- /O2 /GS- /Gy /Fo"%OUTDIR%\WmfVirtualPad.obj" WmfVirtualPad.c
if %ERRORLEVEL% NEQ 0 (echo COMPILE FAILED & exit /b 1)

echo Linking...
link.exe /nologo /DLL /OUT:"%OUTDIR%\WmfVirtualPad.dll" /MACHINE:ARM64 /SUBSYSTEM:WINDOWS /NODEFAULTLIB "%OUTDIR%\WmfVirtualPad.obj" "%WDF_LIB%\WdfDriverStubUm.lib" "%WDK_LIB%\um\arm64\mincore.lib" "%WDK_LIB%\um\arm64\ntdllp.lib" "%MSVC_LIB%\msvcrt.lib" "%MSVC_LIB%\vcruntime.lib" "%OUTDIR%\ucrt.lib"
if %ERRORLEVEL% NEQ 0 (echo LINK FAILED & exit /b 1)

echo.
echo BUILD SUCCEEDED
dir "%OUTDIR%\WmfVirtualPad.dll"
