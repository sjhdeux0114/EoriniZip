param(
  [ValidateSet('Install','Uninstall')][string]$Action = 'Install',
  [string]$Executable = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build\EoriniZip.exe')
)
$ErrorActionPreference='Stop'
$base='Software\Classes'
$clsid='{5B30EB48-528D-446B-B18E-BB516C5B902F}'
$extensions=@('.zip','.7z','.rar','.tar','.gz','.bz2','.xz','.001','.alz','.egg','.zpaq','.ezc')
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
$legacy=@('*\shell\EoriniZip.Compress','Directory\shell\EoriniZip.Compress')
$handlers=@('*\shellex\ContextMenuHandlers\EoriniZip','Directory\shellex\ContextMenuHandlers\EoriniZip')
$paths=$legacy+$handlers+@("CLSID\$clsid")
foreach($extension in $extensions) {
  $paths+="SystemFileAssociations\$extension\shell\EoriniZip.Open"
  $paths+="SystemFileAssociations\$extension\shell\EoriniZip.Extract"
  $paths+="SystemFileAssociations\$extension\shell\EoriniZip.ExtractHere"
  $paths+="SystemFileAssociations\$extension\shell\EoriniZip.ExtractSmart"
}
if($Action -eq 'Uninstall') {
  foreach($path in $paths) { [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree("$base\$path",$false) }
  Write-Host 'EoriniZip context menu removed for the current user.'
} else {
  $Executable=(Resolve-Path -LiteralPath $Executable).Path
  $dll=(Resolve-Path -LiteralPath (Join-Path (Split-Path $Executable) 'EoriniZipShell_v051.dll')).Path
  $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey("$base\CLSID\$clsid\InprocServer32")
  try { $key.SetValue('',$dll);$key.SetValue('ThreadingModel','Apartment') } finally {$key.Dispose()}
  foreach($path in $handlers){
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey("$base\$path")
    try{$key.SetValue('',$clsid)}finally{$key.Dispose()}
  }
  foreach($path in $legacy){[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree("$base\$path",$false)}
  foreach($extension in $extensions) {
    Verb "SystemFileAssociations\$extension\shell\EoriniZip.Open" '어린이zip으로 압축 풀기' '--extract-here'
    Verb "SystemFileAssociations\$extension\shell\EoriniZip.Extract" '어린이zip으로 새 폴더에 풀기' '--extract-folder'
    Verb "SystemFileAssociations\$extension\shell\EoriniZip.ExtractHere" '여기에 풀기 (어린이zip)' '--extract-here'
    Verb "SystemFileAssociations\$extension\shell\EoriniZip.ExtractSmart" '알아서 풀기 (어린이zip)' '--extract-smart'
  }
  Write-Host 'Installed native dynamic menus for current user. On Windows 11, use Show more options.'
}
if (-not ('EoriniZipShellNotification' -as [type])) {
  Add-Type 'using System; using System.Runtime.InteropServices; public static class EoriniZipShellNotification { [DllImport("shell32.dll")] public static extern void SHChangeNotify(uint e, uint f, IntPtr a, IntPtr b); }'
}
[EoriniZipShellNotification]::SHChangeNotify(0x08000000,0,[IntPtr]::Zero,[IntPtr]::Zero)
