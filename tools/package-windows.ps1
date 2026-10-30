param(
 [string]$BuildDirectory='build-validation',
 [string]$SevenZipDirectory='vendor\7zip',
 [string]$Destination='dist\EoriniZip'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=(Resolve-Path -LiteralPath (Join-Path $root $BuildDirectory)).Path
$engine=(Resolve-Path -LiteralPath (Join-Path $root $SevenZipDirectory)).Path
$target=[IO.Path]::GetFullPath((Join-Path $root $Destination))
if(-not $target.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Package output must stay inside this workspace.'}
New-Item -ItemType Directory -Force $target,(Join-Path $target '7zip'),(Join-Path $target 'licenses'),(Join-Path $target 'Package\Assets') | Out-Null
foreach($file in @('EoriniZip.exe','EoriniZipShell_v3.dll','EoriniZipModernShell.dll','eorinizip-unalz.exe','eorinizip-unegg.exe','eorinizip-zpaq.exe','eorinizip-easycrab.exe')){
 Copy-Item -LiteralPath (Join-Path $build $file) -Destination $target
}
foreach($file in @('7z.exe','7z.dll','License.txt','readme.txt')){
 Copy-Item -LiteralPath (Join-Path $engine $file) -Destination (Join-Path $target '7zip')
}
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.txt') -Destination $target
foreach($codec in @('unalz','unegg','easycrab','zpaq')){
 $folder=Join-Path $target ('licenses\'+$codec);New-Item -ItemType Directory -Force $folder | Out-Null
 foreach($file in @('LICENSE','COPYING','UPSTREAM_COMMIT')){
  $source=Join-Path $root ('third_party\'+$codec+'\'+$file)
  if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination $folder}
 }
}
Copy-Item -LiteralPath (Join-Path $root 'third_party\zpaq\libzpaq.cpp') -Destination (Join-Path $target 'licenses\zpaq')
Copy-Item -LiteralPath (Join-Path $root 'packaging\windows11\AppxManifest.xml') -Destination (Join-Path $target 'Package')
Add-Type -AssemblyName System.Drawing
$image=[Drawing.Image]::FromFile((Join-Path $root 'assets\logo.png'))
try{
 foreach($item in @(@('StoreLogo.png',50),@('Square44x44Logo.png',44),@('Square150x150Logo.png',150))){
  $bitmap=[Drawing.Bitmap]::new([int]$item[1],[int]$item[1]);$graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{$graphics.Clear([Drawing.Color]::Transparent);$graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;$graphics.DrawImage($image,0,0,[int]$item[1],[int]$item[1]);$bitmap.Save((Join-Path $target ('Package\Assets\'+$item[0])),[Drawing.Imaging.ImageFormat]::Png)}
  finally{$graphics.Dispose();$bitmap.Dispose()}
 }
}finally{$image.Dispose()}
Get-ChildItem -LiteralPath $target -File | Select-Object Name,Length

& (Join-Path $PSScriptRoot 'package-licenses.ps1') -Destination ($Destination+'/licenses/rust')
if($LASTEXITCODE -ne 0){throw 'Dependency license collection failed.'}
$sourceArchive=Join-Path $root 'vendor/7zip/7z2605-src.tar.xz'
if(-not (Test-Path -LiteralPath $sourceArchive)){
 Invoke-WebRequest 'https://github.com/ip7z/7zip/releases/download/26.05/7z2605-src.tar.xz' -OutFile $sourceArchive
}
Copy-Item -LiteralPath $sourceArchive -Destination (Join-Path $target 'licenses/7z2605-src.tar.xz')