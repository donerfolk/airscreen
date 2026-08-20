; AirScreen installer (Inno Setup 6)
#define MyAppName "AirScreen"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "AirScreen"
#define MyAppExeName "AirScreen.exe"

[Setup]
AppId={{8E3F0B6A-6C1A-4E2F-9B11-A1B5C8EE0001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\AirScreen
DefaultGroupName=AirScreen
OutputDir=Output
OutputBaseFilename=AirScreen-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
LicenseFile=..\LICENSE
SetupIconFile=..\src\app\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupLogging=yes

[Files]
Source: "..\build\Release\AirScreen.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\AirScreen"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\AirScreen"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"
Name: "firewall"; Description: "Allow AirScreen through Windows Firewall"; GroupDescription: "Network:"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch AirScreen"; Flags: nowait postinstall skipifsilent
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""AirScreen"" dir=in action=allow program=""{app}\{#MyAppExeName}"" enable=yes profile=any"; Flags: runhidden; Tasks: firewall

[UninstallRun]
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""AirScreen"""; Flags: runhidden
