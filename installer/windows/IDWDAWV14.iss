#ifndef PackageRoot
  #error PackageRoot is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif

[Setup]
AppId={{89B0AF09-07A4-4BCC-9455-B9C5217DD78E}
AppName=IDW DAW V14 Beta
AppVersion=14.0.0
AppPublisher=In Da Wind Entertainment
DefaultDirName={localappdata}\Programs\IDW DAW V14 Beta
DefaultGroupName=IDW DAW V14 Beta
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=IDW-DAW-V14-Beta-Setup-Windows-x64
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
WizardStyle=modern
UninstallDisplayIcon={app}\IDW-DAW-V14-Beta.exe
CloseApplications=yes
RestartApplications=no

[Files]
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Icons]
Name: "{group}\IDW DAW V14 Beta"; Filename: "{app}\IDW-DAW-V14-Beta.exe"
Name: "{userdesktop}\IDW DAW V14 Beta"; Filename: "{app}\IDW-DAW-V14-Beta.exe"; Tasks: desktopicon

; No project, recording, or skin directory is installed into or deleted.
; Installation does not launch the DAW or change audio-device settings.
