@echo off
:: Windows Mouse Fix Uninstaller

net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo Uninstalling Windows Mouse Fix...

taskkill /F /IM WindowsMouseFix.exe >nul 2>&1
timeout /t 1 /nobreak >nul

reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "WindowsMouseFix" /f >nul 2>&1
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /f >nul 2>&1
del "%ProgramData%\Microsoft\Windows\Start Menu\Programs\Windows Mouse Fix.lnk" >nul 2>&1
rmdir /s /q "%ProgramFiles%\WindowsMouseFix" >nul 2>&1

echo Uninstalled successfully.
pause
