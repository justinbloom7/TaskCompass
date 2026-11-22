# SPDX-License-Identifier: GPL-3.0-or-later
# Historical reads accept a verified private inheritance chain; new captures stay protected.
param([string]$Path,
      [ValidateSet('history', 'check', 'create')][string]$Operation = 'check',
      [string]$Anchor,
      [switch]$DefinitionsOnly)
$ErrorActionPreference = 'Stop'

function Assert-OwnerOnlyAcl($acl, $sid, [bool]$requireProtected) {
   $rules = $acl.GetAccessRules($true, $true, [System.Security.Principal.SecurityIdentifier])
   if ($acl.GetOwner([System.Security.Principal.SecurityIdentifier]).Value -ne $sid.Value -or
       $rules.Count -ne 1 -or $rules[0].IdentityReference.Value -ne $sid.Value -or
       $rules[0].FileSystemRights -ne [System.Security.AccessControl.FileSystemRights]::FullControl -or
       $rules[0].AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow -or
       ($rules[0].PropagationFlags -band [System.Security.AccessControl.PropagationFlags]::InheritOnly)) {
      throw 'Directory owner/effective private rights differ'
   }
   if ($requireProtected -and !$acl.AreAccessRulesProtected) {
      throw 'Protected ACL required on snapshot or historical anchor'
   }
}

function Assert-PrivateDirectory([string]$path, $sid, [bool]$history, [string]$anchor) {
   $current = [System.IO.Path]::GetFullPath($path).TrimEnd('\')
   if ($history) {
      $anchor = [System.IO.Path]::GetFullPath($anchor).TrimEnd('\')
      if (!$current.Equals($anchor, [System.StringComparison]::OrdinalIgnoreCase) -and
          !$current.StartsWith($anchor + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
         throw 'Historical QA path is outside approved capture anchor'
      }
   }
   while ($true) {
      $item = Get-Item -Force -LiteralPath $current
      if (!$item.PSIsContainer -or ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
         throw 'Missing/non-directory/reparse-point private path'
      }
      $atAnchor = $history -and $current.Equals($anchor, [System.StringComparison]::OrdinalIgnoreCase)
      Assert-OwnerOnlyAcl (Get-Acl -LiteralPath $current) $sid (!$history -or $atAnchor)
      if (!$history -or $atAnchor) { return }
      $parent = [System.IO.Path]::GetDirectoryName($current)
      if (!$parent -or $parent -eq $current) { throw 'Historical anchor not reached' }
      $current = $parent
   }
}

if ($DefinitionsOnly) { return }
if (!$Path -or ($Operation -eq 'history' -and !$Anchor)) { throw 'Private path/anchor required' }
$sid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User
if ($Operation -eq 'create') {
   # Caller only uses create for a newly made, empty snapshot; never historical evidence.
   $item = Get-Item -Force -LiteralPath $Path
   if (!$item.PSIsContainer -or ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -or
       @(Get-ChildItem -Force -LiteralPath $Path).Count -ne 0) { throw 'New snapshot must be an empty regular directory' }
   $acl = New-Object System.Security.AccessControl.DirectorySecurity
   $acl.SetOwner($sid)
   $acl.SetAccessRuleProtection($true, $false)
   $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($sid, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')))
   Set-Acl -LiteralPath $Path -AclObject $acl
}
Assert-PrivateDirectory $Path $sid ($Operation -eq 'history') $Anchor
