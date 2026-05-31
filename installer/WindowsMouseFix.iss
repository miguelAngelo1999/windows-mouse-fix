#define MyAppName "Windows Mouse Fix"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Windows Mouse Fix"
#define MyAppURL "https://github.com/miguelAngelo1999/windows-mouse-fix"
#define MyAppExeName "WindowsMouseFix.exe"

[Setup]
AppId={{B5A2C4D1-3E7F-4A8B-9C6D-1F2E3A4B5C6D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
PrivilegesRequired=admin
OutputDir=..\installer\out
OutputBaseFilename=WindowsMouseFix-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
InfoBeforeFile=..\installer\before_install.txt
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
RestartApplications=no
; Ask for reboot after install (handled in [Code] section)

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "autostart"; Description: "Start automatically when Windows starts"; GroupDescription: "Startup:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
; Main app
Source: "..\windows-mouse-fix\build\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Driver files
Source: "..\windows-mouse-fix\driver\ARM64\Release\WmfVirtualPad.dll"; DestDir: "{app}\driver"; Flags: ignoreversion
Source: "..\windows-mouse-fix\driver\ARM64\Release\WmfVirtualPad.inf"; DestDir: "{app}\driver"; Flags: ignoreversion
Source: "..\windows-mouse-fix\driver\ARM64\Release\wmfvirtualpad.cat"; DestDir: "{app}\driver"; Flags: ignoreversion
Source: "..\windows-mouse-fix\driver\WmfTestCert.cer"; DestDir: "{app}\driver"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Autostart — use HKLM so it works for all users (admin install)
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
; 1. Enable test signing
Filename: "{sys}\bcdedit.exe"; Parameters: "/set testsigning on"; Flags: runhidden; StatusMsg: "Enabling test signing mode..."

; 2. Add certificate to Trusted Root
Filename: "{sys}\certutil.exe"; Parameters: "-addstore Root ""{app}\driver\WmfTestCert.cer"""; Flags: runhidden; StatusMsg: "Installing driver certificate (Root)..."

; 3. Add certificate to TrustedPublisher
Filename: "{sys}\certutil.exe"; Parameters: "-addstore TrustedPublisher ""{app}\driver\WmfTestCert.cer"""; Flags: runhidden; StatusMsg: "Installing driver certificate (TrustedPublisher)..."

; 4. Install driver into driver store
Filename: "{sys}\pnputil.exe"; Parameters: "/add-driver ""{app}\driver\WmfVirtualPad.inf"" /install"; Flags: runhidden; StatusMsg: "Installing virtual touchpad driver..."

; 5. Create device node (may fail before reboot — that's OK)
Filename: "{sys}\pnputil.exe"; Parameters: "/add-device /instanceid ROOT\WmfVirtualPad\0000 /hardwareid ROOT\WmfVirtualPad"; Flags: runhidden; StatusMsg: "Creating virtual touchpad device..."

; 6. Launch app after install (optional)
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName} now"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Stop app first
Filename: "{sys}\taskkill.exe"; Parameters: "/f /im {#MyAppExeName}"; Flags: runhidden; RunOnceId: "StopApp"

; Remove device node
Filename: "{sys}\pnputil.exe"; Parameters: "/remove-device ROOT\WmfVirtualPad\0000"; Flags: runhidden; RunOnceId: "RemoveDevice"

; Remove driver from store
Filename: "{sys}\pnputil.exe"; Parameters: "/delete-driver wmfvirtualpad.inf /uninstall"; Flags: runhidden; RunOnceId: "RemoveDriver"

; Disable test signing
Filename: "{sys}\bcdedit.exe"; Parameters: "/set testsigning off"; Flags: runhidden; RunOnceId: "DisableTestSigning"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
// Show reboot prompt after installation
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssDone then begin
    if MsgBox('Installation complete!' + #13#10 + #13#10 +
              'A reboot is required for the driver to load.' + #13#10 +
              'Would you like to reboot now?',
              mbConfirmation, MB_YESNO) = IDYES then
    begin
      Exec(ExpandConstant('{sys}\shutdown.exe'), '/r /t 5 /c "Rebooting to load Windows Mouse Fix driver"', '', SW_HIDE, ewNoWait, ResultCode);
    end;
  end;
end;
