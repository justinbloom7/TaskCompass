# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only diagnosis of the recorded Windows B7.7 QA directories. No ACL changes.
$ErrorActionPreference = 'Stop'
$qaDirectory = 'C:\Users\justin\TaskCompassBackups\Phase1.2-schema1-windows-20261117-064917\b77-windows-20261118-204828-79876'
$currentSid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User
Write-Output 'Read-only ACL inspection; no database access or permission changes.'
foreach ($label in @('.', 'hierarchy', 'ownership-moves', 'before-restart', 'after-restart', 'final')) {
   $directory = if ($label -eq '.') { $qaDirectory } else { Join-Path $qaDirectory $label }
   $item = Get-Item -Force -LiteralPath $directory
   if (!$item.PSIsContainer -or ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
      throw 'Unexpected non-directory/reparse-point QA path; stop for review'
   }
   $acl = Get-Acl -LiteralPath $directory
   $rules = $acl.GetAccessRules($true, $true, [System.Security.Principal.SecurityIdentifier])
   $ownerMatches = $acl.GetOwner([System.Security.Principal.SecurityIdentifier]).Value -eq $currentSid.Value
   Write-Output ("Directory: {0}" -f $directory)
   Write-Output ("OwnerMatchesCurrentUser={0}; InheritanceDisabled={1}; RuleCount={2}" -f $ownerMatches, $acl.AreAccessRulesProtected, $rules.Count)
   foreach ($rule in $rules) {
      Write-Output ("  CurrentUser={0}; Rights={1}; Type={2}; Inherited={3}; InheritanceFlags={4}; PropagationFlags={5}" -f
         ($rule.IdentityReference.Value -eq $currentSid.Value), $rule.FileSystemRights,
         $rule.AccessControlType, $rule.IsInherited, $rule.InheritanceFlags, $rule.PropagationFlags)
   }
}
