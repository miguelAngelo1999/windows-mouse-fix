@echo off
:: Windows Mouse Fix Installer
:: Run this as Administrator, or it will self-elevate.

:: Self-elevate if not admin
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

setlocal
set INSTDIR=%ProgramFiles%\WindowsMouseFix
set SCRIPT_DIR=%~dp0

echo.
echo ============================================
echo   Windows Mouse Fix v1.0.0 Installer
echo ============================================
echo.
echo Install directory: %INSTDIR%
echo.
set /p CONFIRM=Press Enter to install, or Ctrl+C to cancel...

:: Stop any running instance
taskkill /F /IM WindowsMouseFix.exe >nul 2>&1

:: Create install directory
mkdir "%INSTDIR%" >nul 2>&1

:: Copy files
echo Copying files...
copy /Y "%SCRIPT_DIR%WindowsMouseFix.exe" "%INSTDIR%\WindowsMouseFix.exe" >nul
copy /Y "%SCRIPT_DIR%README.md"           "%INSTDIR%\README.md"           >nul 2>&1
copy /Y "%SCRIPT_DIR%Uninstall.bat"       "%INSTDIR%\Uninstall.bat"       >nul 2>&1

:: Create Start Menu shortcut
echo Creating shortcuts...
powershell -NoProfile -Command ^
  "$s=(New-Object -COM WScript.Shell).CreateShortcut('%ProgramData%\Microsoft\Windows\Start Menu\Programs\Windows Mouse Fix.lnk');" ^
  "$s.TargetPath='%INSTDIR%\WindowsMouseFix.exe';" ^
  "$s.Description='Windows Mouse Fix - Smooth scrolling for any mouse';" ^
  "$s.Save()"

:: Set auto-start
echo Configuring auto-start...
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" ^
  /v "WindowsMouseFix" /t REG_SZ ^
  /d "\"%INSTDIR%\WindowsMouseFix.exe\"" /f >nul

:: Add to Add/Remove Programs
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "DisplayName"     /t REG_SZ  /d "Windows Mouse Fix"                    /f >nul
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "DisplayVersion"  /t REG_SZ  /d "1.0.0"                                 /f >nul
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "Publisher"       /t REG_SZ  /d "Windows Mouse Fix"                    /f >nul
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "InstallLocation" /t REG_SZ  /d "%INSTDIR%"                             /f >nul
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "UninstallString" /t REG_SZ  /d "\"%INSTDIR%\Uninstall.bat\""           /f >nul
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "NoModify"        /t REG_DWORD /d 1                                     /f >nul

:: Launch the app
echo Launching Windows Mouse Fix...
start "" "%INSTDIR%\WindowsMouseFix.exe"

echo.
echo ============================================
echo   Installation complete!
echo ============================================
echo.
echo Windows Mouse Fix is now running in your system tray.
echo Look for the icon in the bottom-right corner of your taskbar.
echo.
echo CURRENT MODE: Basic (smooth scrolling, no rubber-band)
echo.
echo For FULL rubber-band + momentum scrolling, install the
echo virtual touchpad driver:
echo   1. Install WDK from: https://aka.ms/wdk
echo   2. Enable test-signing: bcdedit /set testsigning on (reboot)
echo   3. Run: pnputil /add-driver "%INSTDIR%\WmfVirtualPad.inf" /install
echo.
echo See README.md for full instructions.
echo.
pause
