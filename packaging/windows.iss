; Windows installer (Inno Setup). Called as: ISCC /DName=CatCrusher "/DProduct=Cat Crusher" /DVersion=0.1.0 packaging/windows.iss
; Files are staged in ..\stage by the release workflow. Works offline.
[Setup]
AppName={#Product}
AppVersion={#Version}
AppPublisher=Studio 87
AppPublisherURL=https://github.com/SpeakingColors
DefaultDirName={autopf}\Studio 87\{#Product}
DefaultGroupName=Studio 87
OutputDir=..\Output
OutputBaseFilename={#Name}-Setup
LicenseFile=..\LICENSE
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
UninstallDisplayName={#Product}

[Files]
Source: "..\stage\vst3\*"; DestDir: "{commoncf64}\VST3\{#Product}.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\stage\{#Product}.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\stage\{#Name}-manual.pdf"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#Product}"; Filename: "{app}\{#Product}.exe"
Name: "{group}\{#Product} manual"; Filename: "{app}\{#Name}-manual.pdf"
