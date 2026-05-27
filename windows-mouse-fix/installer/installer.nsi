; installer.nsi
; NSIS installer script for Windows Mouse Fix
;
; Requirements:
;   - NSIS 3.x (https://nsis.sourceforge.io/)
;   - Signed driver files: WmfVirtualPad.dll, WmfVirtualPad.inf, WmfVirtualPad.cat
;   - Built app: WindowsMouseFix.exe
;
; Build: makensis installer.nsi

!define APP_NAME        "Windows Mouse Fix"
!define APP_VERSION     "1.0.0"
!define APP_PUBLISHER   "Windows Mouse Fix"
!define APP_URL         "https://github.com/your-repo/windows-mouse-fix"
!define APP_EXE         "WindowsMouseFix.exe"
!define DRIVER_INF      "WmfVirtualPad.inf"
!define DRIVER_DLL      "WmfVirtualPad.dll"
!define DRIVER_CAT      "WmfVirtualPad.cat"
!define INSTALL_DIR     "$PROGRAMFILES64\WindowsMouseFix"
!define REG_KEY         "Software\Microsoft\Windows\CurrentVersion\Uninstall\WindowsMouseFix"

; ---- General ----

Name            "${APP_NAME} ${APP_VERSION}"
OutFile         "WindowsMouseFix-Setup-${APP_VERSION}.exe"
InstallDir      "${INSTALL_DIR}"
InstallDirRegKey HKLM "${REG_KEY}" "InstallLocation"
RequestExecutionLevel admin
SetCompressor   lzma

; ---- Pages ----

Page license
Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

LicenseData "..\LICENSE"

; ---- Installer sections ----

Section "Windows Mouse Fix (required)" SecMain
    SectionIn RO

    SetOutPath "$INSTDIR"

    ; Copy application files
    File "..\build\Release\${APP_EXE}"
    File "..\driver\${DRIVER_INF}"
    File "..\driver\${DRIVER_DLL}"
    File "..\driver\${DRIVER_CAT}"
    File "..\README.md"

    ; Install the UMDF2 driver using pnputil
    DetailPrint "Installing virtual touchpad driver..."
    nsExec::ExecToLog 'pnputil /add-driver "$INSTDIR\${DRIVER_INF}" /install'
    Pop $0
    ${If} $0 != 0
        MessageBox MB_OK|MB_ICONEXCLAMATION \
            "Driver installation failed (error $0).$\n$\n\
             The app will still run but scrolling enhancement won't work.$\n\
             Try running the installer as Administrator."
    ${EndIf}

    ; Create Start Menu shortcut
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    CreateShortcut  "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"   "$INSTDIR\Uninstall.exe"

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Write registry entries for Add/Remove Programs
    WriteRegStr   HKLM "${REG_KEY}" "DisplayName"      "${APP_NAME}"
    WriteRegStr   HKLM "${REG_KEY}" "DisplayVersion"   "${APP_VERSION}"
    WriteRegStr   HKLM "${REG_KEY}" "Publisher"        "${APP_PUBLISHER}"
    WriteRegStr   HKLM "${REG_KEY}" "URLInfoAbout"     "${APP_URL}"
    WriteRegStr   HKLM "${REG_KEY}" "InstallLocation"  "$INSTDIR"
    WriteRegStr   HKLM "${REG_KEY}" "UninstallString"  "$INSTDIR\Uninstall.exe"
    WriteRegDWORD HKLM "${REG_KEY}" "NoModify"         1
    WriteRegDWORD HKLM "${REG_KEY}" "NoRepair"         1

    ; Launch the app after install
    Exec '"$INSTDIR\${APP_EXE}"'

SectionEnd

; ---- Uninstaller ----

Section "Uninstall"

    ; Stop the running app
    nsExec::ExecToLog 'taskkill /F /IM "${APP_EXE}"'

    ; Remove the driver
    DetailPrint "Removing virtual touchpad driver..."
    nsExec::ExecToLog 'pnputil /delete-driver "$INSTDIR\${DRIVER_INF}" /uninstall'

    ; Remove auto-start registry entry
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "WindowsMouseFix"

    ; Remove files
    Delete "$INSTDIR\${APP_EXE}"
    Delete "$INSTDIR\${DRIVER_INF}"
    Delete "$INSTDIR\${DRIVER_DLL}"
    Delete "$INSTDIR\${DRIVER_CAT}"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir  "$INSTDIR"

    ; Remove Start Menu shortcuts
    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"
    RMDir  "$SMPROGRAMS\${APP_NAME}"

    ; Remove Add/Remove Programs entry
    DeleteRegKey HKLM "${REG_KEY}"

    ; Remove app data (optional — ask user)
    MessageBox MB_YESNO "Remove settings and configuration files?" IDNO skip_appdata
        RMDir /r "$APPDATA\WindowsMouseFix"
    skip_appdata:

SectionEnd
