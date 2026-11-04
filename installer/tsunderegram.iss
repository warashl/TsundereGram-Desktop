; TsundereGram Desktop installer (Inno Setup 6).
#ifndef MyAppVersion
  #define MyAppVersion "7.2.9"
#endif
#ifndef SourceDir
  #define SourceDir "..\deploy\TsundereGram"
#endif
#ifndef OutputDir
  #define OutputDir "..\deploy"
#endif

[Setup]
AppId={{8E1C5B7A-3D24-4F6B-9A0E-5C7D2B1F4E93}
AppName=TsundereGram Desktop
AppVersion={#MyAppVersion}
AppVerName=TsundereGram Desktop {#MyAppVersion}
AppPublisher=TsundereGram
AppPublisherURL=https://warashl.github.io/
AppSupportURL=https://t.me/TsundereGram_Banners
DefaultDirName={userappdata}\TsundereGram Desktop
DisableProgramGroupPage=yes
DisableDirPage=no
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=TsundereGram-Desktop-Setup-x64
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\TsundereGram.exe
UninstallDisplayName=TsundereGram Desktop
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "uk"; MessagesFile: "compiler:Languages\Ukrainian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\TsundereGram"; Filename: "{app}\TsundereGram.exe"
Name: "{autodesktop}\TsundereGram"; Filename: "{app}\TsundereGram.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\TsundereGram.exe"; Description: "{cm:LaunchProgram,TsundereGram}"; Flags: nowait postinstall skipifsilent
