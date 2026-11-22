# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$Path,
      [ValidateSet('check','new')][string]$Operation = 'check')
$ErrorActionPreference = 'Stop'
$evidencePath = $Path
$evidenceOperation = $Operation
trap {
   # ASCII/UTF-8 machine diagnostic only; never emit arbitrary file contents/secrets.
   $detail = 'UNCLASSIFIED'
   $known = @('Directory owner/effective private rights differ',
              'Protected ACL required on snapshot or historical anchor',
              'Missing/non-directory/reparse-point private path',
              'Private evidence contains nonregular file')
   if ($known -contains $_.Exception.Message) { $detail = 'PRIVATE_POLICY_MISMATCH' }
   if ($_.Exception.Message -match '^B79_NEW_DIRECTORY_ERROR=(\d+)$') { $detail = 'WIN32_' + $Matches[1] }
   $message = 'B79_ACL_ERROR type=' + $_.Exception.GetType().FullName + ' hresult=' + $_.Exception.HResult + ' detail=' + $detail + "`n"
   $bytes = [System.Text.Encoding]::UTF8.GetBytes($message)
   [Console]::OpenStandardError().Write($bytes, 0, $bytes.Length)
   throw
}
$helper = Join-Path $PSScriptRoot '..\b78\private-directory.ps1'
. $helper -DefinitionsOnly
$sid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User
if ($evidenceOperation -eq 'new') {
   # CreateDirectoryW atomically refuses existing paths and applies the DACL at creation.
   # Never run Set-Acl on an existing failed directory or sealed evidence.
   if (-not ('TaskCompass.B79PrivateDirectory' -as [type])) {
      Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace TaskCompass {
   public static class B79PrivateDirectory {
      [StructLayout(LayoutKind.Sequential)]
      private struct SecurityAttributes {
         public int length;
         public IntPtr descriptor;
         [MarshalAs(UnmanagedType.Bool)] public bool inherit;
      }
      [DllImport("kernel32.dll", CharSet=CharSet.Unicode, ExactSpelling=true, SetLastError=true)]
      [return: MarshalAs(UnmanagedType.Bool)]
      private static extern bool CreateDirectoryW(string path, ref SecurityAttributes attributes);
      public static int createNew(string path, byte[] descriptor) {
         IntPtr memory = Marshal.AllocHGlobal(descriptor.Length);
         try {
            Marshal.Copy(descriptor, 0, memory, descriptor.Length);
            SecurityAttributes attributes = new SecurityAttributes();
            attributes.length = Marshal.SizeOf(typeof(SecurityAttributes));
            attributes.descriptor = memory;
            attributes.inherit = false;
            return CreateDirectoryW(path, ref attributes) ? 0 : Marshal.GetLastWin32Error();
         } finally { Marshal.FreeHGlobal(memory); }
      }
   }
}
'@
   }
   $acl = New-Object System.Security.AccessControl.DirectorySecurity
   $acl.SetOwner($sid)
   $acl.SetAccessRuleProtection($true, $false)
   $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($sid, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')))
   $code = [TaskCompass.B79PrivateDirectory]::createNew([System.IO.Path]::GetFullPath($evidencePath), $acl.GetSecurityDescriptorBinaryForm())
   if ($code -ne 0) { throw ('B79_NEW_DIRECTORY_ERROR=' + $code) }
}
Assert-PrivateDirectory $evidencePath $sid $false ''
foreach ($file in Get-ChildItem -Force -LiteralPath $evidencePath) {
   if ($file.PSIsContainer -or ($file.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
      throw 'Private evidence contains nonregular file'
   }
   Assert-OwnerOnlyAcl (Get-Acl -LiteralPath $file.FullName) $sid $false
}
