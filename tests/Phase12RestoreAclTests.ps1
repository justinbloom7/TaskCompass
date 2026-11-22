# SPDX-License-Identifier: GPL-3.0-or-later
# Offline native Windows regression: only a new disposable temporary fixture.
$ErrorActionPreference = 'Stop'
$fixture = Join-Path ([System.IO.Path]::GetTempPath()) ('TaskCompass-B79-ACL-' + [guid]::NewGuid().ToString('N'))
$helper = Join-Path $PSScriptRoot '..\scripts\phase12\b79\private-evidence.ps1'
[System.IO.Directory]::CreateDirectory($fixture) | Out-Null
try {
   & $helper -Path $fixture -Operation create
   $file = Join-Path $fixture 'UTF8 fixture.txt'
   [System.IO.File]::WriteAllText($file, 'offline', (New-Object System.Text.UTF8Encoding($false)))
   & $helper -Path $fixture -Operation check
   $acl = Get-Acl -LiteralPath $file
   $other = New-Object System.Security.Principal.SecurityIdentifier('S-1-5-32-545')
   $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($other, 'Read', 'Allow')))
   Set-Acl -LiteralPath $file -AclObject $acl
   $rejected = $false
   try { & $helper -Path $fixture -Operation check } catch { $rejected = $true }
   if (!$rejected) { throw 'Extra fixture file access was accepted' }
   Write-Output 'PASS: new private restore evidence created/checked; extra file access rejected.'
} finally {
   # Only this uniquely created test fixture; never a snapshot or database.
   [System.IO.Directory]::Delete($fixture, $true)
}
