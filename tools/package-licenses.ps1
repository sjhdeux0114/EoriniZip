param([string]$Destination='dist/dependency-licenses')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(Test-Path "$root/.tmp/cargo/bin/cargo.exe"){$env:CARGO_HOME="$root/.tmp/cargo";$env:RUSTUP_HOME="$root/.tmp/rustup";$cargo="$root/.tmp/cargo/bin/cargo.exe"}else{$cargo='cargo'}
$all=@{}
foreach($codec in @('unalz','unegg','easycrab')){
 $json=& $cargo metadata --locked --format-version 1 --filter-platform x86_64-pc-windows-msvc --manifest-path "$root/third_party/$codec/Cargo.toml"
 if($LASTEXITCODE -ne 0){throw "Cargo metadata failed: $codec"}
 $meta=$json | ConvertFrom-Json
 foreach($pkg in $meta.packages){if($pkg.source){$all[$pkg.name+'-'+$pkg.version]=$pkg}}
}
$out=Join-Path $root $Destination
New-Item -ItemType Directory -Force $out | Out-Null
foreach($pair in $all.GetEnumerator()){
 $dir=Split-Path $pair.Value.manifest_path
 $files=Get-ChildItem -LiteralPath $dir -File | Where-Object {$_.Name -match '^(LICENSE|LICENCE|COPYING|NOTICE)'}
 if(!$files -and (Test-Path (Join-Path $dir 'LICENSES'))){$files=Get-ChildItem (Join-Path $dir 'LICENSES') -File}
 if(!$files){$fallback=Join-Path $root ('third_party/notices/'+$pair.Key);if(Test-Path -LiteralPath $fallback){$files=Get-ChildItem -LiteralPath $fallback -File}}
 if(!$files){throw ('Missing license files: '+$pair.Key)}
 $dest=Join-Path $out $pair.Key
 New-Item -ItemType Directory -Force $dest | Out-Null
 foreach($file in $files){Copy-Item -LiteralPath $file.FullName -Destination $dest}
 [IO.File]::WriteAllText((Join-Path $dest 'package.txt'),($pair.Value.name+"`n"+$pair.Value.version+"`nLicense: "+$pair.Value.license+"`n"+$pair.Value.repository))
}
Write-Output ('Collected dependency notices: '+$all.Count)