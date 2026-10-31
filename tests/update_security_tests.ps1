$ErrorActionPreference='Stop'
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\update-security.ps1')
$valid=[pscustomobject]@{Status='Valid';SignerCertificate=[pscustomobject]@{Thumbprint='ABC123'}}
Assert-UpdateSignatures -Installed $valid -Downloaded $valid
foreach($bad in @(
 [pscustomobject]@{Status='HashMismatch';SignerCertificate=$valid.SignerCertificate},
 [pscustomobject]@{Status='NotSigned';SignerCertificate=$null},
 [pscustomobject]@{Status='NotTrusted';SignerCertificate=$valid.SignerCertificate},
 [pscustomobject]@{Status='Valid';SignerCertificate=[pscustomobject]@{Thumbprint='DIFFERENT'}},
 [pscustomobject]@{Status='Valid';SignerCertificate=[pscustomobject]@{Thumbprint=''}}
)){
 $rejected=$false
 try{Assert-UpdateSignatures -Installed $valid -Downloaded $bad}catch{$rejected=$true}
 if(-not $rejected){throw 'Unsafe update signature accepted.'}
}
$rejected=$false
try{Assert-UpdateSignatures -Installed ([pscustomobject]@{Status='NotSigned';SignerCertificate=$null}) -Downloaded $valid}catch{$rejected=$true}
if(-not $rejected){throw 'Unsigned installed app accepted as update trust baseline.'}
Write-Output 'Updater rejects tampered, unsigned, untrusted, different-signer and empty-signer updates.'
