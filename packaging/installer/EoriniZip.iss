#define AppVersion "0.5.2"
#define AppName "?¥Î¶∞?¥zip"
#ifndef PackageDirectory
#define PackageDirectory "..\..\dist\EoriniZip"
#endif
[Setup]
#ifdef TestInstall
AppId={{244F199B-6CB9-4FCF-B031-858B47F0276C}
#else
AppId={{D04F21CE-490B-40EF-91BB-B5EAEE88A5B1}
#endif
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=EoriniZip
AppPublisherURL=https://github.com/sjhdeux0114/EoriniZip
DefaultDirName={localappdata}\Programs\EoriniZip
DefaultGroupName={#AppName}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\..\dist
#ifdef TestInstall
OutputBaseFilename=EoriniZip-0.5.2-installer-test
#else
OutputBaseFilename=EoriniZip-0.5.2-windows-x64-setup
#endif
SetupIconFile=..\..\assets\EoriniZip.ico
UninstallDisplayIcon={app}\EoriniZip.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
ChangesAssociations=yes
VersionInfoVersion=0.5.2.0
[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
[Tasks]
Name: "explorer"; Description: "?êÏÉâÍ∏??∞ÌÅ¥Î¶?Î©îÎâ¥ ?±Î°ù"
Name: "desktopicon"; Description: "Î∞îÌÉï ?îÎ©¥ Î∞îÎ°ú Í∞ÄÍ∏?ÎßåÎì§Í∏?; Flags: unchecked
[Files]
Source: "{#PackageDirectory}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\EoriniZip.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\EoriniZip.exe"; Tasks: desktopicon
[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\windows11-menu.ps1"" -Action Uninstall"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveModernContextMenu"; Check: FileExists(ExpandConstant('{app}\windows11-menu.ps1'))
[Code]
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
    if not WizardSilent then
      MsgBox('Windows 11 Ï≤??∞ÌÅ¥Î¶?Î©îÎâ¥???úÎ™Ö ?®ÌÇ§ÏßÄÍ∞Ä ?¨Ìï®?òÏ? ?äÏïò?µÎãà?? ?¥Î¶∞?¥zip Î©îÎâ¥??[??ÎßéÏ? ?µÏÖò ?úÏãú]?êÏÑú ?¨Ïö©?????àÏäµ?àÎã§.', mbInformation, MB_OK);
    Exit;
  end;
  Parameters := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + Script +
    '" -Action InstallSigned -Directory "' + ExpandConstant('{app}') + '"';
  if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    ResultCode := -1;
  if ResultCode <> 0 then begin
    Log('Windows 11 context menu registration failed: ' + IntToStr(ResultCode));
    if not WizardSilent then
      MsgBox('Windows 11 Î©îÎâ¥ ?±Î°ù???§Ìå®?àÏäµ?àÎã§. [??ÎßéÏ? ?µÏÖò ?úÏãú]??Î©îÎâ¥Î•??¨Ïö©?òÍ±∞???§Ïπò ?¥Îçî??windows11-menu.ps1Î°??±Î°ù ?§Î•òÎ•??ïÏù∏?òÏÑ∏??', mbInformation, MB_OK);
  end;
end;
[Registry]
Root: HKCU; Subkey: "Software\Classes\CLSID\{{5B30EB48-528D-446B-B18E-BB516C5B902F}\InprocServer32"; ValueType: string; ValueName: ""; ValueData: "{app}\EoriniZipShell_v051.dll"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\CLSID\{{5B30EB48-528D-446B-B18E-BB516C5B902F}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\*\shellex\ContextMenuHandlers\EoriniZip"; ValueType: string; ValueName: ""; ValueData: "{{5B30EB48-528D-446B-B18E-BB516C5B902F}"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\Directory\shellex\ContextMenuHandlers\EoriniZip"; ValueType: string; ValueName: ""; ValueData: "{{5B30EB48-528D-446B-B18E-BB516C5B902F}"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zip\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.7z\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.rar\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.tar\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.gz\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.bz2\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.xz\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.cab\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.iso\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wim\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.001\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.alz\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.egg\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.zpaq\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Open"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ?ïÏ∂ï ?ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Open"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-here ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Extract"; ValueType: string; ValueName: ""; ValueData: "?¥Î¶∞?¥zip?ºÎ°ú ???¥Îçî???ÄÍ∏?; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Extract"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-folder ""%1"""; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: ""; ValueData: "?åÏïÑ???ÄÍ∏?(?¥Î¶∞?¥zip)"; Flags: uninsdeletekey; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.ExtractSmart"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\EoriniZip.exe"",0"; Tasks: explorer
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ezc\shell\EoriniZip.ExtractSmart\command"; ValueType: string; ValueName: ""; ValueData: """{app}\EoriniZip.exe"" --extract-smart ""%1"""; Tasks: explorer
