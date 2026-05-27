# build_installer.ps1
# Builds a self-contained installer for Windows Mouse Fix.
# Creates a single .exe that extracts and installs everything.
# Uses IExpress (built into Windows) — no NSIS or WiX needed.
#
# Run from the repo root:
#   powershell -ExecutionPolicy Bypass -File installer\build_installer.ps1

param(
    [string]$Version = "1.0.0",
    [string]$BuildDir = ".\build",
    [string]$OutDir   = ".\installer\out"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

Write-Host "=== Windows Mouse Fix Installer Builder ===" -ForegroundColor Cyan
Write-Host "Version: $Version"
Write-Host "Build dir: $BuildDir"

# ---- Verify required files ----
$appExe = "$BuildDir\WindowsMouseFix.exe"
if (-not (Test-Path $appExe)) {
    Write-Error "WindowsMouseFix.exe not found at $appExe. Build the app first."
    exit 1
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# ---- Create the install script (runs on target machine) ----
$installScript = @'
@echo off
setlocal
set INSTDIR=%ProgramFiles%\WindowsMouseFix
set APPDATA_DIR=%APPDATA%\WindowsMouseFix

echo Installing Windows Mouse Fix...

:: Create install directory
mkdir "%INSTDIR%" 2>nul

:: Copy files
copy /Y "%~dp0WindowsMouseFix.exe" "%INSTDIR%\WindowsMouseFix.exe"
copy /Y "%~dp0README.md" "%INSTDIR%\README.md" 2>nul

:: Create Start Menu shortcut via PowerShell
powershell -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%ProgramData%\Microsoft\Windows\Start Menu\Programs\Windows Mouse Fix.lnk');$s.TargetPath='%INSTDIR%\WindowsMouseFix.exe';$s.Save()"

:: Set auto-start
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "WindowsMouseFix" /t REG_SZ /d "\"%INSTDIR%\WindowsMouseFix.exe\"" /f

:: Write uninstaller
copy /Y "%~dp0uninstall.bat" "%INSTDIR%\uninstall.bat" 2>nul

:: Add to Add/Remove Programs
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "DisplayName" /t REG_SZ /d "Windows Mouse Fix" /f
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "DisplayVersion" /t REG_SZ /d "1.0.0" /f
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "Publisher" /t REG_SZ /d "Windows Mouse Fix" /f
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "UninstallString" /t REG_SZ /d "\"%INSTDIR%\uninstall.bat\"" /f
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /v "InstallLocation" /t REG_SZ /d "%INSTDIR%" /f

:: Launch the app
start "" "%INSTDIR%\WindowsMouseFix.exe"

echo.
echo Installation complete!
echo Windows Mouse Fix is now running in your system tray.
echo.
echo NOTE: For full rubber-band and momentum scrolling, install the
echo       virtual touchpad driver. See README.md for instructions.
echo.
pause
'@

$uninstallScript = @'
@echo off
echo Uninstalling Windows Mouse Fix...
taskkill /F /IM WindowsMouseFix.exe 2>nul
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "WindowsMouseFix" /f 2>nul
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix" /f 2>nul
del "%ProgramData%\Microsoft\Windows\Start Menu\Programs\Windows Mouse Fix.lnk" 2>nul
rmdir /s /q "%ProgramFiles%\WindowsMouseFix" 2>nul
echo Uninstalled.
pause
'@

# ---- Stage files ----
$stageDir = "$OutDir\stage"
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Copy-Item -Force $appExe "$stageDir\WindowsMouseFix.exe"
Copy-Item -Force "$root\README.md" "$stageDir\README.md" -ErrorAction SilentlyContinue

$installScript  | Out-File "$stageDir\install.bat"  -Encoding ascii
$uninstallScript | Out-File "$stageDir\uninstall.bat" -Encoding ascii

# ---- Build self-extracting archive using IExpress ----
# IExpress is built into Windows (C:\Windows\System32\iexpress.exe)
# We create a .sed (Self Extraction Directive) file

$sedContent = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=1
HideExtractAnimation=0
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=%InstallPrompt%
DisplayLicense=%DisplayLicense%
FinishMessage=%FinishMessage%
TargetName=$OutDir\WindowsMouseFix-Setup-$Version.exe
FriendlyName=Windows Mouse Fix Setup
AppLaunched=install.bat
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
[Strings]
InstallPrompt=Install Windows Mouse Fix $Version?
DisplayLicense=
FinishMessage=Installation complete!
[SourceFiles]
SourceFiles0=$stageDir
[SourceFiles0]
%FILE0%=WindowsMouseFix.exe
%FILE1%=install.bat
%FILE2%=uninstall.bat
%FILE3%=README.md
"@

$sedPath = "$env:TEMP\wmf_installer.sed"
$sedContent | Out-File $sedPath -Encoding ascii

Write-Host "Building installer with IExpress..."
$iexpress = "$env:SystemRoot\System32\iexpress.exe"
$proc = Start-Process -FilePath $iexpress -ArgumentList "/N /Q $sedPath" -Wait -PassThru
Write-Host "IExpress exit code: $($proc.ExitCode)"

$installerPath = "$OutDir\WindowsMouseFix-Setup-$Version.exe"
if (Test-Path $installerPath) {
    $size = (Get-Item $installerPath).Length
    Write-Host ""
    Write-Host "SUCCESS: Installer built!" -ForegroundColor Green
    Write-Host "  Path: $installerPath"
    Write-Host "  Size: $([math]::Round($size/1KB, 1)) KB"
} else {
    Write-Warning "IExpress didn't produce output. Falling back to zip package..."

    # Fallback: create a zip with install.bat
    $zipPath = "$OutDir\WindowsMouseFix-$Version.zip"
    Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath -Force
    Write-Host "Created zip package: $zipPath"
    Write-Host "Extract and run install.bat as Administrator."
}
