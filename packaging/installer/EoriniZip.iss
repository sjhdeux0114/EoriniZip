#define AppVersion "0.5.6"
#define AppName "어린이zip"
#define ReleaseId "{D04F21CE-490B-40EF-91BB-B5EAEE88A5B1}"
#define TestId "{244F199B-6CB9-4FCF-B031-858B47F0276C}"
#ifndef PackageDirectory
#define PackageDirectory "..\..\dist\EoriniZip"
#endif
[Setup]
#ifdef TestInstall
AppId={{#TestId}
#else
AppId={{#ReleaseId}
#endif
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=EoriniZip
AppPublisherURL=https://github.com/sjhdeux0114/EoriniZip
DefaultDirName={localappdata}\Programs\EoriniZip
UsePreviousAppDir=no
DefaultGroupName={#AppName}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\..\dist
#ifdef TestInstall
OutputBaseFilename=EoriniZip-{#AppVersion}-installer-test
#else
OutputBaseFilename=EoriniZip-{#AppVersion}-windows-x64-setup
#endif
SetupIconFile=..\..\assets\EoriniZip.ico
UninstallDisplayIcon={app}\EoriniZip.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=yes
ChangesAssociations=yes
VersionInfoVersion={#AppVersion}.0
[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
[Tasks]
Name: "explorer"; Description: "탐색기 우클릭 메뉴 등록"
Name: "desktopicon"; Description: "바탕 화면 바로 가기 만들기"; Flags: unchecked
[Files]
Source: "{#PackageDirectory}\*"; Excludes: "EoriniZipShell_v051.dll,EoriniZipModernShell_v051.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageDirectory}\EoriniZipShell_v051.dll"; DestDir: "{app}"; Flags: ignoreversion; Check: NeedsShellUpdate('EoriniZipShell_v051.dll', '{#GetSHA256OfFile(PackageDirectory + "\EoriniZipShell_v051.dll")}')
Source: "{#PackageDirectory}\EoriniZipModernShell_v051.dll"; DestDir: "{app}"; Flags: ignoreversion; Check: NeedsShellUpdate('EoriniZipModernShell_v051.dll', '{#GetSHA256OfFile(PackageDirectory + "\EoriniZipModernShell_v051.dll")}')
[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\EoriniZip.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\EoriniZip.exe"; Tasks: desktopicon
[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\windows11-menu.ps1"" -Action Uninstall"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveModernContextMenu"; Check: FileExists(ExpandConstant('{app}\windows11-menu.ps1'))
[UninstallDelete]
; A matching, locked shell DLL may have been retained from the prior install.
Type: files; Name: "{app}\EoriniZipShell_v051.dll"
Type: files; Name: "{app}\EoriniZipModernShell_v051.dll"
[Code]
function HandlePreviousInstall(const AppId: String; Remove: Boolean): String;
var
  Key, Dir, Uninstaller, Name, Publisher, Version: String;
  InstalledVersion, SetupVersion: Int64;
  Code: Integer;
  Stream: TFileStream;
begin
  Result := '';
  Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\' + AppId + '_is1';
  if not RegKeyExists(HKCU, Key) then Exit;
  if IsAdmin then begin
    Result := '기존 사용자 설치를 업데이트하려면 설치 프로그램을 일반 권한으로 다시 실행하세요.';
    Exit;
  end;
  Result := '기존 어린이zip의 설치 정보를 확인할 수 없습니다. Windows 설치된 앱에서 기존 버전을 제거한 뒤 다시 실행하세요.';
  if not RegQueryStringValue(HKCU, Key, 'Publisher', Publisher) or
     (Publisher <> 'EoriniZip') or
     not RegQueryStringValue(HKCU, Key, 'InstallLocation', Dir) or (Dir = '') or
     not RegQueryStringValue(HKCU, Key, 'UninstallString', Uninstaller) then Exit;
  if RegQueryStringValue(HKCU, Key, 'DisplayVersion', Version) and
     StrToVersion(Version, InstalledVersion) and StrToVersion('{#AppVersion}', SetupVersion) then
    if InstalledVersion > SetupVersion then begin
      Result := '이미 더 최신 버전의 어린이zip이 설치되어 있습니다: ' + Version;
      Exit;
    end;
  Dir := RemoveBackslashUnlessRoot(Dir);
  Uninstaller := RemoveQuotes(Uninstaller);
  Name := Lowercase(ExtractFileName(Uninstaller));
  if (CompareText(ExtractFileDir(Uninstaller), Dir) <> 0) or
     (Length(Name) <> 12) or (Copy(Name, 1, 5) <> 'unins') or
     (StrToIntDef(Copy(Name, 6, 3), -1) < 0) or (Copy(Name, 9, 4) <> '.exe') then Exit;

  if not FileExists(Uninstaller) then begin
    // An installation that was manually deleted can leave an Apps entry behind.
    if not FileExists(AddBackslash(Dir) + 'EoriniZip.exe') and
       not FileExists(ChangeFileExt(Uninstaller, '.dat')) then
      if not Remove then Result := ''
      else if RegDeleteKeyIncludingSubkeys(HKCU, Key) then Result := '';
    Exit;
  end;
  if not FileExists(ChangeFileExt(Uninstaller, '.dat')) then Exit;
  if FileExists(AddBackslash(Dir) + 'EoriniZip.exe') then
    try
      Stream := TFileStream.Create(AddBackslash(Dir) + 'EoriniZip.exe', fmOpenRead or fmShareExclusive);
      Stream.Free;
    except
      Result := '실행 중인 어린이zip을 닫은 후 다시 시도하세요.' + #13#10 + Dir;
      Exit;
    end;
  if not Remove then begin
    Result := '';
    Exit;
  end;
  Log('Removing previous EoriniZip installation: ' + Dir);
  Result := '기존 어린이zip 제거를 완료하지 못했습니다.' + #13#10 + Dir;
  if not ExecAsOriginalUser(Uninstaller, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG',
       Dir, SW_HIDE, ewWaitUntilTerminated, Code) then Exit;
  if (Code <> 0) or RegKeyExists(HKCU, Key) or FileExists(AddBackslash(Dir) + 'EoriniZip.exe') then Exit;
  Result := '';
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := HandlePreviousInstall('{#TestId}', False);
#ifndef TestInstall
  if Result = '' then Result := HandlePreviousInstall('{#ReleaseId}', False);
#endif
  if Result = '' then Result := HandlePreviousInstall('{#TestId}', True);
#ifndef TestInstall
  if Result = '' then Result := HandlePreviousInstall('{#ReleaseId}', True);
#endif
end;

function NeedsShellUpdate(const Name, PackageHash: String): Boolean;
var
  Path: String;
begin
  Path := ExpandConstant('{app}\') + Name;
  Result := True;
  if FileExists(Path) then
    try
      Result := CompareText(GetSHA256OfFile(Path), PackageHash) <> 0;
      if not Result then Log('Keeping identical shell extension: ' + Path);
    except
      Log('Could not read installed shell extension: ' + Path);
    end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  Script, PackagePath, Parameters: String;
  WindowsVersion: TWindowsVersion;
begin
  GetWindowsVersionEx(WindowsVersion);
  if (CurStep <> ssPostInstall) or (not WizardIsTaskSelected('explorer')) or
     (WindowsVersion.Build < 22000) then
    Exit;
  Script := ExpandConstant('{app}\windows11-menu.ps1');
  PackagePath := ExpandConstant('{app}\Package\EoriniZip.ContextMenu.msix');
  if not FileExists(PackagePath) then begin
    Log('Windows 11 modern menu unavailable: signed identity MSIX is missing.');
    // if not WizardSilent then
    // MsgBox('Windows 11 첫 우클릭 메뉴용 서명 패키지가 포함되지 않았습니다. 어린이zip 메뉴는 [더 많은 옵션 표시]에서 사용할 수 있습니다.', mbInformation, MB_OK);
    Exit;
  end;
  Parameters := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + Script +
    '" -Action InstallSigned -Directory "' + ExpandConstant('{app}') + '"';
  if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    ResultCode := -1;
  if ResultCode <> 0 then begin
    Log('Windows 11 context menu registration failed: ' + IntToStr(ResultCode));
    // if not WizardSilent then
      // MsgBox('Windows 11 메뉴 등록에 실패했습니다. [더 많은 옵션 표시]의 메뉴를 사용하거나 설치 폴더의 windows11-menu.ps1로 등록 오류를 확인하세요.', mbInformation, MB_OK);
  end;
end;
[Registry]
Root: HKCU; Subkey: "Software\Classes\CLSID\{{5B30EB48-528D-446B-B18E-BB516C5B902F}\InprocServer32"; ValueType: string; ValueName: ""; ValueData: "{app}\EoriniZipShell_v051.dll"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\CLSID\{{5B30EB48-528D-446B-B18E-BB516C5B902F}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\*\shellex\ContextMenuHandlers\EoriniZip"; ValueType: string; ValueName: ""; ValueData: "{{5B30EB48-528D-446B-B18E-BB516C5B902F}"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\Directory\shellex\ContextMenuHandlers\EoriniZip"; ValueType: string; ValueName: ""; ValueData: "{{5B30EB48-528D-446B-B18E-BB516C5B902F}"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 압축 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "어린이zip으로 새 폴더에 풀기"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "알아서 풀기 (어린이zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
