#ifndef PackageRoot
  #error PackageRoot is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif

[Setup]
AppId={{B22D4D30-92DD-49E4-A1ED-C579C8A2F136}
AppName=IDW Combined Studio Beta
AppVersion=14.0.0
AppPublisher=In Da Wind Entertainment
DefaultDirName={localappdata}\Programs\IDW Combined Studio Beta
DefaultGroupName=IDW Combined Studio Beta
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=IDW-Combined-Studio-Beta-Setup-Windows-x64
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
Name: "{group}\Voice MIDI, Tune and Song Studio"; Filename: "{app}\VoiceStudio\IDW Voice MIDI Studio.exe"
Name: "{group}\Combined Beta Guide"; Filename: "{app}\Documentation\COMBINED-BETA.md"
Name: "{group}\Remote Recording Test Guide"; Filename: "{app}\Documentation\REMOTE-RECORDING-TEST.md"
Name: "{group}\IDW Combined Studio Beta"; Filename: "{app}\IDW-DAW-V14-Beta.exe"
Name: "{userdesktop}\IDW Combined Studio Beta"; Filename: "{app}\IDW-DAW-V14-Beta.exe"; Tasks: desktopicon

; No project, recording, or skin directory is installed into or deleted.
; Installation does not launch the DAW or change audio-device settings.
