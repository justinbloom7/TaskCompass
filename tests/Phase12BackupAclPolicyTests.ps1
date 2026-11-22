# SPDX-License-Identifier: GPL-3.0-or-later
# Native Windows offline policy regression: in-memory ACLs and mocked metadata only.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\scripts\phase12\b78\private-directory.ps1') -DefinitionsOnly
$sid = New-Object System.Security.Principal.SecurityIdentifier('S-1-5-21-111-222-333-1001')
$other = New-Object System.Security.Principal.SecurityIdentifier('S-1-5-21-111-222-333-1002')
function New-TestAcl([bool]$protected) {
   $acl = New-Object System.Security.AccessControl.DirectorySecurity
   $acl.SetOwner($sid)
   $acl.SetAccessRuleProtection($protected, $true)
   $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($sid, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')))
   return $acl
}
function Expect-Rejection([scriptblock]$action) {
   $rejected = $false
   try { & $action } catch { $rejected = $true }
   if (!$rejected) { throw 'Unsafe policy case was accepted' }
}
$inherited = New-TestAcl $false
$protected = New-TestAcl $true
Assert-OwnerOnlyAcl $inherited $sid $false
Expect-Rejection { Assert-OwnerOnlyAcl $inherited $sid $true }
Assert-OwnerOnlyAcl $protected $sid $true
$badOwner = New-TestAcl $false; $badOwner.SetOwner($other)
Expect-Rejection { Assert-OwnerOnlyAcl $badOwner $sid $false }
$extra = New-TestAcl $false
$extra.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($other, 'Read', 'Allow')))
Expect-Rejection { Assert-OwnerOnlyAcl $extra $sid $false }
$denied = New-TestAcl $false
$denied.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($sid, 'Write', 'Deny')))
Expect-Rejection { Assert-OwnerOnlyAcl $denied $sid $false }
$limited = New-Object System.Security.AccessControl.DirectorySecurity; $limited.SetOwner($sid)
$limited.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($sid, 'Read', 'Allow')))
Expect-Rejection { Assert-OwnerOnlyAcl $limited $sid $false }
$script:anchor = 'C:\fixture\schema1'
$script:leaf = $script:anchor + '\b77-fixture\final'
$script:metadata = @{
   'C:\fixture\schema1' = $protected
   'C:\fixture\schema1\b77-fixture' = $inherited
   'C:\fixture\schema1\b77-fixture\final' = $inherited
}
$script:attributes = [System.IO.FileAttributes]::Directory
function Get-Item { param([switch]$Force, [string]$LiteralPath)
   if (!$script:metadata.ContainsKey($LiteralPath)) { throw 'Unknown fixture directory' }
   return [pscustomobject]@{ PSIsContainer = $true; Attributes = $script:attributes }
}
function Get-Acl { param([string]$LiteralPath) return $script:metadata[$LiteralPath] }
Assert-PrivateDirectory $script:leaf $sid $true $script:anchor
Expect-Rejection { Assert-PrivateDirectory $script:leaf $sid $false '' }
Expect-Rejection { Assert-PrivateDirectory 'C:\fixture\unrelated' $sid $true $script:anchor }
$script:attributes = [System.IO.FileAttributes]::ReparsePoint
Expect-Rejection { Assert-PrivateDirectory $script:leaf $sid $true $script:anchor }
$script:attributes = [System.IO.FileAttributes]::Directory
$script:metadata[$script:anchor] = $inherited
Expect-Rejection { Assert-PrivateDirectory $script:leaf $sid $true $script:anchor }
$script:metadata[$script:anchor] = $protected
$script:metadata['C:\fixture\schema1\b77-fixture'] = $extra
Expect-Rejection { Assert-PrivateDirectory $script:leaf $sid $true $script:anchor }
Write-Output 'PASS: historical owner-only inheritance accepted only through private protected anchor; fresh snapshots/extra grants/owners/rights refuse.'
