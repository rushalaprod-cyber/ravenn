; Installeur Windows de ravenn (Inno Setup 6)
; Compile avec : ISCC.exe /DBuildDir=<dossier des artefacts Release> ravenn.iss

#ifndef BuildDir
  #define BuildDir "..\..\build\ravenn_artefacts\Release"
#endif
#define AppVersion "1.8.0"

[Setup]
AppId={{7B1E5C2A-4D6F-4C1B-9A57-52A1E0D7F3C9}
AppName=ravenn
AppVersion={#AppVersion}
AppPublisher=ravenn
DefaultDirName={autopf}\ravenn
DefaultGroupName=ravenn
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputBaseFilename=ravenn-{#AppVersion}-windows-setup
OutputDir=output
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=ravenn

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "vst3"; Description: "Plugin VST3"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Application autonome"; Types: full

[Files]
Source: "{#BuildDir}\VST3\ravenn.vst3\*"; DestDir: "{commoncf64}\VST3\ravenn.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\ravenn.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\ravenn"; Filename: "{app}\ravenn.exe"; Components: standalone
