param(
  [ValidateSet('Install','Uninstall')][string]$Action = 'Install',
  [string]$Executable = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build\EoriniZip.exe')
)
$ErrorActionPreference='Stop'
$base='Software\Classes'
$extensions=@('.zip','.7z','.rar','.tar','.gz','.bz2','.xz','.001')
function Verb([string]$path,[string]$label,[string]$mode) {
  $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey("$base\$path")
  try {
    $key.SetValue('', $label)
    $key.SetValue('Icon', '"' + $Executable + '",0')
    $key.SetValue('MultiSelectModel','Single')
    $command=$key.CreateSubKey('command')
    try { $command.SetValue('', '"' + $Executable + '" ' + $mode + ' "%1"') } finally { $command.Dispose() }
  } finally { $key.Dispose() }
}
$paths=@('*\shell\EoriniZip.Compress','Directory\shell\EoriniZip.Compress')
foreach($extension in $extensions) {
  $paths+="SystemFileAssociations\$extension\shell\EoriniZip.Open"
  $paths+="SystemFileAssociations\$extension\shell\EoriniZip.Extract"
}
if($Action -eq 'Uninstall') {
  foreach($path in $paths) { [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree("$base\$path",$false) }
  Write-Host 'EoriniZip context menu removed for the current user.'
  exit
}
$Executable=(Resolve-Path -LiteralPath $Executable).Path
Verb '*\shell\EoriniZip.Compress' '어린이zip으로 ZIP 압축' '--compress'
Verb 'Directory\shell\EoriniZip.Compress' '어린이zip으로 ZIP 압축' '--compress'
foreach($extension in $extensions) {
  Verb "SystemFileAssociations\$extension\shell\EoriniZip.Open" '어린이zip으로 열기' '--open'
  Verb "SystemFileAssociations\$extension\shell\EoriniZip.Extract" '어린이zip으로 새 폴더에 풀기' '--extract-folder'
}
Write-Host 'Installed for current user. On Windows 11, use Show more options.'
