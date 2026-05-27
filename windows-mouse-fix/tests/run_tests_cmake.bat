@echo off
REM ============================================================
REM  Windows Mouse Fix — CMake Test Runner
REM  Use this if you have CMake + Visual Studio installed.
REM  No driver needed — tests only cover pure-logic components.
REM ============================================================

setlocal

set ROOT=%~dp0..
set BUILD=%ROOT%\build_tests

echo.
echo === Windows Mouse Fix — CMake Test Build ===
echo.

REM Check cmake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] cmake not found on PATH.
    echo         Install from https://cmake.org/download/
    exit /b 1
)

REM Configure
echo Configuring...
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

REM Build tests only
echo.
echo Building WmfTests...
cmake --build "%BUILD%" --config Release --target WmfTests
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b 1
)

REM Run
echo.
echo Running tests...
echo.
"%BUILD%\Release\WmfTests.exe"

if %ERRORLEVEL% equ 0 (
    echo.
    echo [PASS] All tests passed.
) else (
    echo.
    echo [FAIL] Some tests failed.
)

exit /b %ERRORLEVEL%
