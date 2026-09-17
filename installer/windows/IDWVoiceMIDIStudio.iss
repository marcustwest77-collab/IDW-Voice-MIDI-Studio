#ifndef BuildRoot
  #error BuildRoot must be supplied by ISCC using /DBuildRoot=...
#endif
#ifndef OutputDir
  #define OutputDir "release\\Installer"
#endif

#define AppName "IDW Voice MIDI Studio"
#define AppVersion "0.8.1"
#define Publisher "In Da Wind Entertainment"
#define AppExeName "IDW Voice MIDI Studio.exe"

[Setup]
AppId={{A16EAC95-6B53-4C36-B8B2-0F4F3DCFD8A1}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
DefaultDirName={autopf}\IDW Voice MIDI Studio
DefaultGroupName=IDW Voice MIDI Studio
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=IDW-Voice-MIDI-Studio-Setup-Windows-x64
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#AppName}

[Files]
Source: "{#BuildRoot}\Standalone\IDW Voice MIDI Studio.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildRoot}\VST3\IDW Voice MIDI Studio.vst3\*"; DestDir: "{commoncf64}\VST3\IDW Voice MIDI Studio.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\IDW Voice MIDI Studio"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\IDW Voice MIDI Studio"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch IDW Voice MIDI Studio"; Flags: nowait postinstall skipifsilent
