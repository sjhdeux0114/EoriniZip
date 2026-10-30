param(
 [ValidateSet('RegisterDevelopment','InstallSigned','Uninstall')][string]$Action='RegisterDevelopment',
 [string]$Directory=(Join-Path (Split-Path $PSScriptRoot) 'dist\EoriniZip'),
 [string]$SignedPackage
)
$ErrorActionPreference='Stop'
if($PSVersionTable.PSEdition -eq 'Core'){throw 'Run this script with Windows PowerShell 5.1 (powershell.exe).'}
if($Action -eq 'Uninstall'){
 Get-AppxPackage -Name EoriniZip.ContextMenu | Remove-AppxPackage
 return
}
$Directory=(Resolve-Path -LiteralPath $Directory).Path
if(-not (Test-Path -LiteralPath (Join-Path $Directory 'EoriniZipModernShell.dll'))){throw 'Modern shell DLL is missing.'}
if($Action -eq 'InstallSigned'){
 if(-not $SignedPackage){throw 'Provide the signed MSIX package path.'}
 Add-AppxPackage -Path $SignedPackage -ExternalLocation $Directory
}else{
 Add-AppxPackage -Register (Join-Path $Directory 'Package\AppxManifest.xml') -ExternalLocation $Directory
}
Get-AppxPackage -Name EoriniZip.ContextMenu | Select-Object Name,Version,Status,InstallLocation
