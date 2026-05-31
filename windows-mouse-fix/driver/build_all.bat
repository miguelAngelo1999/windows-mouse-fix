@echo off
setlocal
set ROOT=c:\Users\virgoh\windows-mouse-fix\windows-mouse-fix
set DRIVER=%ROOT%\driver
set BUILD=%ROOT%\build
set SIGNTOOL="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86\signtool.exe"
set INF2CAT="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86\inf2cat.exe"

echo.
echo ========================================
echo  Building Windows Mouse Fix (all arches)
echo ========================================
echo.

:: ---- Build ARM64 driver ----
echo [1/5] Building ARM64 driver...
call "%DRIVER%\build_driver.bat"
if %ERRORLEVEL% NEQ 0 (echo ARM64 driver build FAILED & exit /b 1)

:: ---- Build x64 driver ----
echo [2/5] Building x64 driver...
call "%DRIVER%\build_driver_x64.bat"
if %ERRORLEVEL% NEQ 0 (echo x64 driver build FAILED & exit /b 1)

:: ---- Copy INF to both output dirs ----
echo [3/5] Copying INF to output dirs...
copy /y "%DRIVER%\WmfVirtualPad.inf" "%DRIVER%\ARM64\Release\WmfVirtualPad.inf"
copy /y "%DRIVER%\WmfVirtualPad.inf" "%DRIVER%\x64\Release\WmfVirtualPad.inf"

:: ---- Sign both DLLs ----
echo [4/5] Signing DLLs...
"%SIGNTOOL%" sign /fd sha256 /a /s Root /n WmfTestCert "%DRIVER%\ARM64\Release\WmfVirtualPad.dll"
"%SIGNTOOL%" sign /fd sha256 /a /s Root /n WmfTestCert "%DRIVER%\x64\Release\WmfVirtualPad.dll"

:: ---- Generate catalogs ----
echo [5/5] Generating catalogs...
%INF2CAT% /driver:"%DRIVER%\ARM64\Release" /os:10_RS5_ARM64
%INF2CAT% /driver:"%DRIVER%\x64\Release" /os:10_RS5_X64

:: ---- Sign catalogs ----
"%SIGNTOOL%" sign /fd sha256 /a /s Root /n WmfTestCert "%DRIVER%\ARM64\Release\wmfvirtualpad.cat"
"%SIGNTOOL%" sign /fd sha256 /a /s Root /n WmfTestCert "%DRIVER%\x64\Release\wmfvirtualpad.cat"

:: ---- Build x64 app ----
echo [6/6] Building x64 app...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=arm64 >nul 2>&1
cd /d "%BUILD%"
cl /nologo /std:c++17 /EHsc /W3 /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0A00 /I..\shared /I..\app /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt" ..\app\main.cpp ..\app\MouseHook.cpp ..\app\ScrollAnalyzer.cpp ..\app\ScrollPipeline.cpp ..\app\DragAnimator.cpp ..\app\DragCurve.cpp ..\app\ContactMapper.cpp ..\app\DriverClient.cpp ..\app\TouchInjector.cpp ..\app\TrayIcon.cpp ..\app\Settings.cpp /Fe:WindowsMouseFix.exe user32.lib advapi32.lib shell32.lib dwmapi.lib setupapi.lib
if %ERRORLEVEL% NEQ 0 (echo App build FAILED & exit /b 1)

echo.
echo ========================================
echo  ALL BUILDS SUCCEEDED
echo ========================================
echo ARM64 driver: %DRIVER%\ARM64\Release\WmfVirtualPad.dll
echo x64 driver:   %DRIVER%\x64\Release\WmfVirtualPad.dll
echo App (x64):    %BUILD%\WindowsMouseFix.exe
