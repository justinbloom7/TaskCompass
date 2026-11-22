# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$Path,
      [ValidateSet('check','create')][string]$Operation = 'check')
$ErrorActionPreference = 'Stop'
$evidencePath = $Path
$helper = Join-Path $PSScriptRoot '..\b78\private-directory.ps1'
if ($Operation -eq 'create') {
   # Only newly created, empty B7.9 output; never modifies a sealed snapshot ACL.
   & $helper -Path $Path -Operation create
}
. $helper -DefinitionsOnly
$sid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User
Assert-PrivateDirectory $evidencePath $sid $false ''
foreach ($file in Get-ChildItem -Force -LiteralPath $evidencePath) {
   if ($file.PSIsContainer -or ($file.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
      throw 'Private evidence contains nonregular file'
   }
   Assert-OwnerOnlyAcl (Get-Acl -LiteralPath $file.FullName) $sid $false
}
