; Lumi Installer — Inno Setup Script
; Build with: ISCC.exe lumi-setup.iss

[Setup]
AppName=Lumi
AppVersion=Beta 1.0
AppPublisher=kotae in LUMOS STUDIOS
AppPublisherURL=https://github.com/kotae-dev/lumi
DefaultDirName={autopf}\Lumi
DefaultGroupName=Lumi
OutputDir=..\..\dist
OutputBaseFilename=lumi-setup-1.0.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
SetupIconFile=..\..\ui\Assets\lumi-icon.ico
UninstallDisplayIcon={app}\Lumi.exe
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\..\build\bin\Lumi.exe";      DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\build\bin\lumi_core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\build\bin\*.dll";         DestDir: "{app}"; Flags: ignoreversion recursesubdirs
Source: "..\..\build\bin\*.json";        DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\Lumi";              Filename: "{app}\Lumi.exe"
Name: "{commondesktop}\Lumi";      Filename: "{app}\Lumi.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\Lumi.exe"; Description: "Launch Lumi"; Flags: postinstall nowait skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\Lumi\logs"
