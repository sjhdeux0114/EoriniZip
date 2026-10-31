param([version]$CurrentVersion='0.5.1',[switch]$Quiet,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'update-security.ps1')
try{
 try{$release=Invoke-RestMethod -Uri 'https://api.github.com/repos/sjhdeux0114/EoriniZip/releases/latest' -TimeoutSec 30 -Headers @{'User-Agent'='EoriniZip-update-check';'Accept'='application/vnd.github+json'}}catch{
  if($_.Exception.Response -and [int]$_.Exception.Response.StatusCode -eq 404){
   if($CheckOnly){[pscustomobject]@{Current=$CurrentVersion.ToString();Latest=$null;UpdateAvailable=$false;Status='No public stable release'} | ConvertTo-Json -Compress}elseif(-not $Quiet){Add-Type -AssemblyName PresentationFramework;[System.Windows.MessageBox]::Show('공개 안정 릴리즈가 아직 없습니다. 개발 릴리즈는 GitHub 릴리즈 페이지에서 확인하세요.','어린이zip 업데이트') | Out-Null}
   exit 0
  }
  throw
 }
 if($release.tag_name -notmatch '^v?(\d+\.\d+(?:\.\d+){0,2})$'){throw 'The release version is not recognized.'}
 $next=[version]$Matches[1]
 if($CheckOnly){[pscustomobject]@{Current=$CurrentVersion.ToString();Latest=$next.ToString();UpdateAvailable=($next -gt $CurrentVersion)} | ConvertTo-Json -Compress;exit 0}
 Add-Type -AssemblyName PresentationFramework
 if($next -le $CurrentVersion){if(-not $Quiet){[System.Windows.MessageBox]::Show('현재 최신 버전입니다.','어린이zip 업데이트') | Out-Null};exit 0}
 if([System.Windows.MessageBox]::Show(('버전 '+$next+'이 있습니다. 설치 파일을 내려받아 서명을 확인한 후 실행할까요?'),'어린이zip 업데이트','YesNo','Question') -ne 'Yes'){exit 0}
 $name='EoriniZip-'+$next.ToString()+'-windows-x64-setup.exe'
 $asset=@($release.assets | Where-Object {$_.name -eq $name})
 if($asset.Count -ne 1){throw 'The Windows x64 installer asset is missing.'}
 $uri=[uri]$asset[0].browser_download_url
 if($uri.Scheme -ne 'https' -or $uri.Host -ne 'github.com' -or -not $uri.AbsolutePath.StartsWith('/sjhdeux0114/EoriniZip/releases/download/',[StringComparison]::Ordinal)){throw 'Unexpected installer download location.'}
 $installed=Get-AuthenticodeSignature -LiteralPath (Join-Path $PSScriptRoot 'EoriniZip.exe')
 if($installed.Status -ne 'Valid' -or -not $installed.SignerCertificate){throw 'The installed app must have a valid trusted signature before automatic updating is enabled. Use the GitHub release page for manual updates.'}
 $folder=Join-Path ([IO.Path]::GetTempPath()) ('EoriniZip-update-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $folder | Out-Null
 $installer=Join-Path $folder $name
 Invoke-WebRequest -UseBasicParsing -Uri $uri.AbsoluteUri -TimeoutSec 600 -OutFile $installer
 $signature=Get-AuthenticodeSignature -LiteralPath $installer
 try{Assert-UpdateSignatures -Installed $installed -Downloaded $signature}catch{Remove-Item -LiteralPath $installer -Force;throw}
 Start-Process -FilePath $installer
}catch{
 if($CheckOnly){Write-Error $_}elseif(-not $Quiet -or $folder){Add-Type -AssemblyName PresentationFramework;[System.Windows.MessageBox]::Show($_.Exception.Message,'어린이zip 업데이트') | Out-Null}
 exit 1
}
