; AirScreen installer (Inno Setup 6)
; iscc /DCrtDir=<VS>\VC\Redist\MSVC\<ver>\x64\Microsoft.VC143.CRT scripts\airscreen.iss
#define MyAppName "AirScreen"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "AirScreen"
#define MyAppURL "https://github.com/donerfolk/airscreen"
#define MyAppExeName "AirScreen.exe"
#ifndef BuildDir
  #define BuildDir "..\build\Release"
#endif
#ifndef CrtDir
  #error Pass /DCrtDir=<VS>\VC\Redist\MSVC\<ver>\x64\Microsoft.VC143.CRT
#endif

[Setup]
AppId={{8E3F0B6A-6C1A-4E2F-9B11-A1B5C8EE0001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\AirScreen
DefaultGroupName=AirScreen
OutputDir=..\out\installer
OutputBaseFilename=AirScreen-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
WizardStyle=modern
LicenseFile=..\LICENSE
SetupIconFile=..\src\app\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupLogging=yes
CloseApplications=yes

[Files]
; Only the DLLs AirScreen.exe imports, plus the app-local Visual C++ runtime
Source: "{#BuildDir}\AirScreen.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\avcodec-*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\avutil-*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\swresample-*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\libcrypto-*-x64.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#CrtDir}\msvcp140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#CrtDir}\vcruntime140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#CrtDir}\vcruntime140_1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\NOTICE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\AirScreen"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\AirScreen"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"
Name: "firewall"; Description: "Allow AirScreen through Windows Firewall"; GroupDescription: "Network:"

[Run]
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""AirScreen"" dir=in action=allow program=""{app}\{#MyAppExeName}"" enable=yes profile=any"; Flags: runhidden; Tasks: firewall
Filename: "{app}\{#MyAppExeName}"; Description: "Launch AirScreen"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "taskkill"; Parameters: "/f /im {#MyAppExeName}"; Flags: runhidden; RunOnceId: "KillApp"
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""AirScreen"""; Flags: runhidden; RunOnceId: "DelFirewall"
