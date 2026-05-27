@echo off
REM ============================================================
REM  Windows Mouse Fix — Quick Test Runner
REM  Run this on any Windows machine to validate the physics
REM  engine WITHOUT needing the driver or Visual Studio.
REM
REM  Requirements: cl.exe (MSVC) on PATH
REM  Easiest way: open "x64 Native Tools Command Prompt for VS"
REM  then cd to this folder and run: run_tests.bat
REM ============================================================

setlocal

set SCRIPT_DIR=%~dp0
set ROOT=%SCRIPT_DIR%..
set OUT=%SCRIPT_DIR%out

echo.
echo === Windows Mouse Fix Test Runner ===
echo.

REM Create output dir
if not exist "%OUT%" mkdir "%OUT%"

REM Compile the test binary
echo Compiling tests...
cl.exe ^
    /nologo ^
    /std:c++17 ^
    /EHsc ^
    /W3 ^
    /O2 ^
    /DUNICODE ^
    /D_UNICODE ^
    /DWIN32_LEAN_AND_MEAN ^
    /DNOMINMAX ^
    /D_WIN32_WINNT=0x0A00 ^
    /I"%ROOT%\shared" ^
    /I"%ROOT%\app" ^
    "%SCRIPT_DIR%test_main.cpp" ^
    /Fe:"%OUT%\WmfTests.exe" ^
    /Fo:"%OUT%\\" ^
    user32.lib advapi32.lib shell32.lib

if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAIL] Compilation failed. Make sure you are running from
    echo        "x64 Native Tools Command Prompt for VS 2022"
    echo.
    exit /b 1
)

echo.
echo Running tests...
echo.
"%OUT%\WmfTests.exe"

if %ERRORLEVEL% equ 0 (
    echo.
    echo [PASS] All tests passed.
) else (
    echo.
    echo [FAIL] Some tests failed. See output above.
)

exit /b %ERRORLEVEL%
