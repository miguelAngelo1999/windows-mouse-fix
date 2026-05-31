@echo off
:: Driver installation helper script
:: Called by the Inno Setup installer

set DRIVER_DIR=%~dp0

echo Enabling test signing...
bcdedit /set testsigning on

echo Installing certificate...
certutil -addstore Root "%DRIVER_DIR%WmfTestCert.cer"
certutil -addstore TrustedPublisher "%DRIVER_DIR%WmfTestCert.cer"

echo Installing driver...
pnputil /add-driver "%DRIVER_DIR%WmfVirtualPad.inf" /install

echo Creating device node...
pnputil /add-device /instanceid ROOT\WmfVirtualPad\0000 /hardwareid ROOT\WmfVirtualPad

echo Done.
