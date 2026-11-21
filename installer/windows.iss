; Inno Setup script: Drop TNT VST3 installer for Windows (Ableton Live, FL Studio, Cubase, ...)
#define AppVer "3.3.0"

[Setup]
AppId={{6E1B7C52-3F7A-4D2B-9C1E-DF05E0A1F002}
AppName=Drop TNT
AppVersion={#AppVer}
AppPublisher=RiotAudio
DefaultDirName={autopf}\RiotAudio\Drop TNT
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=DropTNT-{#AppVer}-win-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\build-win\DropTNT_artefacts\Release\VST3\Drop TNT.vst3\*"; DestDir: "{commoncf64}\VST3\Drop TNT.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Drop TNT.vst3"

[InstallDelete]
; remove the old v1 plugin (Drop Fuse) so hosts don't show both
Type: filesandordirs; Name: "{commoncf64}\VST3\Drop Fuse.vst3"
