@echo off
:: Run this script as Administrator AFTER disabling Secure Boot in Parallels
:: From Mac host: prlctl set "<VM_NAME>" --efi-secure-boot off
::
:: This script:
:: 1. Enables test signing
:: 2. Adds the test cert to trusted stores
:: 3. Installs the driver
:: 4. Reboots to activate test signing

echo ============================================
echo  WmfVirtualPad Driver Installation Script
echo ============================================
echo.

cd /d c:\Users\virgoh\windows-mouse-fix\windows-mouse-fix\driver

echo [1/4] Enabling test signing...
bcdedit /set testsigning on
if %ERRORLEVEL% NEQ 0 (
    echo FAILED - Is Secure Boot still enabled?
    echo Run from Mac host: prlctl set "YOUR_VM_NAME" --efi-secure-boot off
    pause
    exit /b 1
)
echo OK

echo.
echo [2/4] Adding test certificate to trusted stores...
certutil -addstore Root WmfTestCert.cer
certutil -addstore TrustedPublisher WmfTestCert.cer
echo OK

echo.
echo [3/4] Installing driver...
pnputil /add-driver arm64\Release\WmfVirtualPad.inf /install
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Driver install failed. Will retry after reboot.
    echo After reboot, run this script again.
)

echo.
echo [4/4] Rebooting to activate test signing...
echo Press any key to reboot, or Ctrl+C to cancel.
pause
shutdown /r /t 5 /c "Rebooting to activate test signing for WmfVirtualPad driver"
