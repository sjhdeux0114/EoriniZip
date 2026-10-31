param(
 [ValidateSet('RegisterDevelopment','InstallSigned','Uninstall','Status')][string]$Action='Status',
 [string]$Directory,
 [string]$SignedPackage
)
$ErrorActionPreference='Stop'
if($PSVersionTable.PSEdition -eq 'Core'){throw 'Run this script with Windows PowerShell 5.1 (powershell.exe).'}
if($Action -eq 'Status'){
 Get-AppxPackage -ErrorAction Stop -Name EoriniZip.ContextMenu | Select-Object Name,Version,Status,InstallLocation
 return
}
if($Action -eq 'Uninstall'){
 Get-AppxPackage -ErrorAction Stop -Name EoriniZip.ContextMenu | Remove-AppxPackage -ErrorAction Stop
 return
}
if([Environment]::OSVersion.Version.Build -lt 22000){throw 'The modern context menu requires Windows 11 (build 22000 or later).'}
if(-not $Directory){
 $Directory=if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'EoriniZip.exe')){$PSScriptRoot}else{Join-Path (Split-Path $PSScriptRoot) 'dist\EoriniZip'}
}
$Directory=(Resolve-Path -LiteralPath $Directory).Path
if(-not (Test-Path -LiteralPath (Join-Path $Directory 'EoriniZip.exe'))){throw 'EoriniZip.exe is missing.'}
if(-not (Test-Path -LiteralPath (Join-Path $Directory 'EoriniZipModernShell_v051.dll'))){throw 'Modern shell DLL is missing.'}
if($Action -eq 'InstallSigned'){
 if(-not $SignedPackage){$SignedPackage=Join-Path $Directory 'Package\EoriniZip.ContextMenu.msix'}
 if(-not (Test-Path -LiteralPath $SignedPackage)){throw 'A signed context menu MSIX is required. The classic menu remains available under Show more options.'}
 Add-AppxPackage -ErrorAction Stop -Path $SignedPackage -ExternalLocation $Directory
}else{
 Add-AppxPackage -ErrorAction Stop -Register (Join-Path $Directory 'Package\AppxManifest.xml') -ExternalLocation $Directory
}
Get-AppxPackage -ErrorAction Stop -Name EoriniZip.ContextMenu | Select-Object Name,Version,Status,InstallLocation
Write-Host 'Context menu registered. If it is not visible, sign out and sign back in to reload Explorer.'
