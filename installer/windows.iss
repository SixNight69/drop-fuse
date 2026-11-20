; Inno Setup script: Drop Fuse VST3 installer for Windows (Ableton Live, FL Studio, Cubase, ...)
#define AppVer "1.0.0"

[Setup]
AppId={{6E1B7C52-3F7A-4D2B-9C1E-DF05E0A1F001}
AppName=Drop Fuse
AppVersion={#AppVer}
AppPublisher=RiotAudio
DefaultDirName={autopf}\RiotAudio\Drop Fuse
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=DropFuse-{#AppVer}-win-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\build-win\DropFuse_artefacts\Release\VST3\Drop Fuse.vst3\*"; DestDir: "{commoncf64}\VST3\Drop Fuse.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Drop Fuse.vst3"
