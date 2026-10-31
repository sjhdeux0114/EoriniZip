function Assert-UpdateSignatures {
 param([Parameter(Mandatory=$true)]$Installed,[Parameter(Mandatory=$true)]$Downloaded)
 if($Installed.Status -ne 'Valid' -or -not $Installed.SignerCertificate -or -not $Installed.SignerCertificate.Thumbprint){throw 'The installed application does not have a valid trusted signature.'}
 if($Downloaded.Status -ne 'Valid' -or -not $Downloaded.SignerCertificate -or -not $Downloaded.SignerCertificate.Thumbprint){throw 'The downloaded installer signature is invalid or untrusted.'}
 if(-not [string]::Equals($Installed.SignerCertificate.Thumbprint,$Downloaded.SignerCertificate.Thumbprint,[StringComparison]::OrdinalIgnoreCase)){throw 'The installer was signed by a different signer. The update was not started.'}
}
