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
; Support both x64 and ARM64
ArchitecturesAllowed=x64compatible arm64
ArchitecturesInstallIn64BitMode=x64compatible arm64
InfoBeforeFile=..\installer\before_install.txt
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "autostart"; Description: "Start automatically when Windows starts"; GroupDescription: "Startup:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
; Main app (x64 — runs on both x64 and ARM64 via emulation, or natively on x64)
Source: "..\windows-mouse-fix\build\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; ARM64 driver files
Source: "..\windows-mouse-fix\driver\ARM64\Release\WmfVirtualPad.dll"; DestDir: "{app}\driver\arm64"; Flags: ignoreversion
Source: "..\windows-mouse-fix\driver\ARM64\Release\WmfVirtualPad.inf"; DestDir: "{app}\driver\arm64"; Flags: ignoreversion
Source: "..\windows-mouse-fix\driver\ARM64\Release\wmfvirtualpad.cat"; DestDir: "{app}\driver\arm64"; Flags: ignoreversion

; x64 driver files
Source: "..\windows-mouse-fix\driver\x64\Release\WmfVirtualPad.dll"; DestDir: "{app}\driver\x64"; Flags: ignoreversion
Source: "..\windows-mouse-fix\driver\x64\Release\WmfVirtualPad.inf"; DestDir: "{app}\driver\x64"; Flags: ignoreversion
Source: "..\windows-mouse-fix\driver\x64\Release\wmfvirtualpad.cat"; DestDir: "{app}\driver\x64"; Flags: ignoreversion

; Certificate (shared)
Source: "..\windows-mouse-fix\driver\WmfTestCert.cer"; DestDir: "{app}\driver"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
; 1. Enable test signing
Filename: "{sys}\bcdedit.exe"; Parameters: "/set testsigning on"; Flags: runhidden; StatusMsg: "Enabling test signing mode..."

; 2. Install certificate
Filename: "{sys}\certutil.exe"; Parameters: "-addstore Root ""{app}\driver\WmfTestCert.cer"""; Flags: runhidden; StatusMsg: "Installing driver certificate..."
Filename: "{sys}\certutil.exe"; Parameters: "-addstore TrustedPublisher ""{app}\driver\WmfTestCert.cer"""; Flags: runhidden; StatusMsg: "Installing driver certificate..."

; 3. Install correct driver for this architecture (handled in [Code])

; 4. Launch app
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName} now"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\taskkill.exe"; Parameters: "/f /im {#MyAppExeName}"; Flags: runhidden; RunOnceId: "StopApp"
Filename: "{sys}\pnputil.exe"; Parameters: "/remove-device ROOT\WmfVirtualPad\0000"; Flags: runhidden; RunOnceId: "RemoveDevice"
Filename: "{sys}\pnputil.exe"; Parameters: "/delete-driver wmfvirtualpad.inf /uninstall"; Flags: runhidden; RunOnceId: "RemoveDriver"
Filename: "{sys}\bcdedit.exe"; Parameters: "/set testsigning off"; Flags: runhidden; RunOnceId: "DisableTestSigning"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function IsARM64(): Boolean;
var
  Arch: String;
begin
  // Check processor architecture
  Arch := GetEnv('PROCESSOR_ARCHITECTURE');
  Result := (Uppercase(Arch) = 'ARM64');
end;

procedure InstallDriver();
var
  DriverDir: String;
  ResultCode: Integer;
begin
  if IsARM64() then
    DriverDir := ExpandConstant('{app}\driver\arm64')
  else
    DriverDir := ExpandConstant('{app}\driver\x64');

  // Install driver into store
  Exec(ExpandConstant('{sys}\pnputil.exe'),
    '/add-driver "' + DriverDir + '\WmfVirtualPad.inf" /install',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

  // Create device node
  Exec(ExpandConstant('{sys}\pnputil.exe'),
    '/add-device /instanceid ROOT\WmfVirtualPad\0000 /hardwareid ROOT\WmfVirtualPad',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    WizardForm.StatusLabel.Caption := 'Installing virtual touchpad driver...';
    InstallDriver();
  end;

  if CurStep = ssDone then
  begin
    if MsgBox('Installation complete!' + #13#10 + #13#10 +
              'A reboot is required for the driver to load.' + #13#10 +
              'Would you like to reboot now?',
              mbConfirmation, MB_YESNO) = IDYES then
    begin
      Exec(ExpandConstant('{sys}\shutdown.exe'),
        '/r /t 5 /c "Rebooting to load Windows Mouse Fix driver"',
        '', SW_HIDE, ewNoWait, ResultCode);
    end;
  end;
end;
