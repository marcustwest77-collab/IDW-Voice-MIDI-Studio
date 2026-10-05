#ifndef BuildRoot
  #error BuildRoot must be supplied by ISCC using /DBuildRoot=...
#endif
#ifndef OutputDir
  #define OutputDir "release\\Installer"
#endif
#ifndef AudioLabRoot
  #error AudioLabRoot must be supplied by ISCC using /DAudioLabRoot=...
#endif

#define AppName "IDW Voice MIDI Studio"
#define AppVersion "10.5.0"
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
SetupIconFile=..\..\Resources\Brand\IDWVoiceMIDIStudio.ico
WizardImageFile=..\..\Resources\Brand\InstallerSidebar.bmp
WizardSmallImageFile=..\..\Resources\Brand\InstallerSmall.bmp
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}

[Files]
Source: "{#BuildRoot}\Standalone\IDW Voice MIDI Studio.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildRoot}\VST3\IDW Voice MIDI Studio.vst3\*"; DestDir: "{commoncf64}\VST3\IDW Voice MIDI Studio.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#AudioLabRoot}\*"; DestDir: "{app}\Audio-Lab"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\docs\USER_MANUAL.md"; DestDir: "{app}\Documentation"; Flags: ignoreversion

Source: "..\..\START-HERE.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\IDW Voice MIDI Studio"; Filename: "{app}\{#AppExeName}"
Name: "{group}\IDW Voice MIDI Studio Manual"; Filename: "{app}\START-HERE.html"
Name: "{group}\IDW Audio Lab"; Filename: "{app}\Audio-Lab\IDW-Audio-Lab.exe"
Name: "{autodesktop}\IDW Voice MIDI Studio"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch IDW Voice MIDI Studio"; Flags: nowait postinstall skipifsilent
