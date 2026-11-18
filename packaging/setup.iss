[Setup]
AppId=OrbitTimer.37719248
AppName=ClenTimer
AppVersion=1.11
DefaultDirName={localappdata}\Programs\ClenTimer
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
OutputDir=..\dist
SetupIconFile=..\ClenTimer.ico
OutputBaseFilename=ClenTimer-1.11-x64-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\ClenTimer.exe
[Files]
Source: "..\stage\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs
[InstallDelete]
Type: files; Name: "{app}\OrbitTimer.exe"
Type: files; Name: "{userprograms}\Orbit Timer.lnk"
[Icons]
Name: "{autoprograms}\ClenTimer"; Filename: "{app}\ClenTimer.exe"
[Run]
Filename: "{app}\ClenTimer.exe"; Description: "Launch ClenTimer"; Flags: postinstall nowait skipifsilent
