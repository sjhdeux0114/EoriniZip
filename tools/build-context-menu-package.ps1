param(
 [string]$Directory,
 [Parameter(Mandatory=$true)][string]$CertificateThumbprint,
 [string]$SdkBin
)
$ErrorActionPreference='Stop'
if(-not $Directory){$Directory=Join-Path (Split-Path -Parent $PSScriptRoot) 'dist\EoriniZip'}
if(-not $SdkBin){
 $sdkRoot=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
 $SdkBin=Get-ChildItem -LiteralPath $sdkRoot -Directory | Sort-Object Name -Descending |
  ForEach-Object {Join-Path $_.FullName 'x64'} |
  Where-Object {Test-Path -LiteralPath (Join-Path $_ 'makeappx.exe')} | Select-Object -First 1
}
if(-not $SdkBin){throw 'Windows SDK makeappx.exe and signtool.exe are required.'}
$Directory=(Resolve-Path -LiteralPath $Directory).Path
$manifestPath=Join-Path $Directory 'Package\AppxManifest.xml'
[xml]$manifest=Get-Content -LiteralPath $manifestPath
$certificate=Get-Item -LiteralPath ('Cert:\CurrentUser\My\'+$CertificateThumbprint)
if(-not $certificate.HasPrivateKey){throw 'The signing certificate must have a private key.'}
if($certificate.Subject -ne $manifest.Package.Identity.Publisher){throw 'Certificate subject must match the manifest Publisher.'}
$output=Join-Path $Directory 'Package\EoriniZip.ContextMenu.msix'
# Keep the output outside the package input tree, including on repeated builds.
$staging=Join-Path ([IO.Path]::GetTempPath()) ('EoriniZipIdentity-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging | Out-Null
try{
 Copy-Item -LiteralPath $manifestPath -Destination $staging
 Copy-Item -LiteralPath (Join-Path $Directory 'Package\Assets') -Destination $staging -Recurse
 & (Join-Path $SdkBin 'makeappx.exe') pack /d $staging /p $output /nv /o
 if($LASTEXITCODE -ne 0){throw 'Identity package creation failed.'}
 & (Join-Path $SdkBin 'signtool.exe') sign /fd SHA256 /sha1 $CertificateThumbprint $output
 if($LASTEXITCODE -ne 0){throw 'Identity package signing failed.'}
 Export-Certificate -Cert $certificate -FilePath (Join-Path (Split-Path $Directory) 'EoriniZip-test-signing.cer') | Out-Null
 Get-Item -LiteralPath $output | Select-Object FullName,Length
}finally{
 # This directory is created above with a fixed prefix and random identifier.
 if(([IO.Path]::GetFullPath($staging)).StartsWith([IO.Path]::GetTempPath(),[StringComparison]::OrdinalIgnoreCase)){
  Remove-Item -LiteralPath $staging -Recurse -Force
 }
}
