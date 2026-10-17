; shellinc installer (Inno Setup 6).
; Built by the release workflow, which passes these defines:
;   AppVersion, NumericVersion, OutputName, RepoUrl
; Run it from the repository root after building build\shellinc.exe.

#ifndef AppVersion
#define AppVersion "0.0.0"
#endif
#ifndef NumericVersion
#define NumericVersion "0.0.0.0"
#endif
#ifndef OutputName
#define OutputName "shellinc-setup"
#endif
#ifndef RepoUrl
#define RepoUrl "https://github.com"
#endif

[Setup]
; AppId identifies this app to Windows. Never change it after the first release.
AppId={{6F2C1B7E-3A4D-4E8B-9C55-2D7A1F0B8E43}
AppName=shellinc
AppVersion={#AppVersion}
AppPublisherURL={#RepoUrl}
AppSupportURL={#RepoUrl}/issues
AppUpdatesURL={#RepoUrl}/releases
VersionInfoVersion={#NumericVersion}
DefaultDirName={autopf}\shellinc
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesEnvironment=yes
LicenseFile=..\LICENSE
OutputDir=output
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\shellinc.exe
SetupIconFile=..\assets\icon\shellinc.ico

[Tasks]
Name: "addtopath"; Description: "Add shellinc to my PATH (so I can type 'shellinc' in any terminal)"; GroupDescription: "Optional:"

[Files]
Source: "..\build\shellinc.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\shellinc"; Filename: "{app}\shellinc.exe"

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: addtopath; Check: NeedsAddPath('{app}')

[Run]
Filename: "{app}\shellinc.exe"; Description: "Start shellinc"; Flags: nowait postinstall skipifsilent

[Code]
{ True if Param is not yet an entry in the user's PATH. }
function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    Exit;
  end;
  Result := Pos(';' + Uppercase(ExpandConstant(Param)) + ';',
                ';' + Uppercase(OrigPath) + ';') = 0;
end;

{ Removes Path from the user's PATH, if it is there. }
procedure RemovePath(Path: string);
var
  Paths: string;
  P: Integer;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', Paths) then
    Exit;
  P := Pos(';' + Uppercase(Path) + ';', ';' + Uppercase(Paths) + ';');
  if P = 0 then
    Exit;
  if P > 1 then
    Delete(Paths, P - 1, Length(Path) + 1)
  else
    Delete(Paths, 1, Length(Path) + 1);
  RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', Paths);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemovePath(ExpandConstant('{app}'));
end;