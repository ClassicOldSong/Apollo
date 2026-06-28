#Requires -Version 5.1
<#
.SYNOPSIS
Captures Windows-side DualSense Edge USB/IP validation evidence.

.DESCRIPTION
Run this from an elevated PowerShell after Apollo has logged that the
DualSense Edge USB/IP backend is listening. The script captures usbip-win2
list/attach output and Windows PnP device IDs for the virtual Sony
VID_054C&PID_0DF2 device, including the expected MI_03 HID child.
#>

[CmdletBinding()]
param(
  [string]$HostAddress = '127.0.0.1',
  [string]$BusId = '1-1',
  [string]$OutputDir = (Join-Path $env:TEMP ("apollo-edge-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))),
  [switch]$SkipAttach,
  [int]$PostAttachDelaySeconds = 3
)

$ErrorActionPreference = 'Continue'

function Write-TextFile {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][string[]]$Lines
  )

  $Lines | Out-File -FilePath $Path -Encoding utf8 -Width 4096
}

function Invoke-NativeCapture {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][string]$Command,
    [Parameter(Mandatory = $true)][string[]]$Arguments
  )

  try {
    & $Command @Arguments *> $Path
    "exit_code=$LASTEXITCODE" | Add-Content -Path $Path -Encoding utf8
  } catch {
    $_ | Out-File -FilePath $Path -Encoding utf8 -Width 4096
  }
}

function Invoke-PowerShellCapture {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][scriptblock]$Script
  )

  try {
    & $Script *>&1 | Out-File -FilePath $Path -Encoding utf8 -Width 4096
  } catch {
    $_ | Out-File -FilePath $Path -Encoding utf8 -Width 4096
  }
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-TextFile -Path (Join-Path $OutputDir 'environment.txt') -Lines @(
  "timestamp=$(Get-Date -Format o)",
  "computer=$env:COMPUTERNAME",
  "user=$env:USERNAME",
  "is_admin=$isAdmin",
  "host=$HostAddress",
  "busid=$BusId",
  "skip_attach=$SkipAttach",
  "powershell=$($PSVersionTable.PSVersion)"
)

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'usbip-command.txt') -Script {
  Get-Command usbip.exe -ErrorAction Stop | Format-List Name,Source,Version
}

Invoke-NativeCapture `
  -Path (Join-Path $OutputDir 'usbip-list.txt') `
  -Command 'usbip.exe' `
  -Arguments @('list', '-r', $HostAddress)

if (-not $SkipAttach) {
  Invoke-NativeCapture `
    -Path (Join-Path $OutputDir 'usbip-attach.txt') `
    -Command 'usbip.exe' `
    -Arguments @('attach', '-r', $HostAddress, '-b', $BusId)

  if ($PostAttachDelaySeconds -gt 0) {
    Start-Sleep -Seconds $PostAttachDelaySeconds
  }
}

Invoke-NativeCapture `
  -Path (Join-Path $OutputDir 'usbip-port.txt') `
  -Command 'usbip.exe' `
  -Arguments @('port')

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'pnp-edge.txt') -Script {
  Get-PnpDevice -PresentOnly -ErrorAction Stop |
    Where-Object { $_.InstanceId -match 'VID_054C&PID_0DF2' } |
    Sort-Object Class,FriendlyName,InstanceId |
    Format-List Class,FriendlyName,InstanceId,Status
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'pnp-edge-mi03.txt') -Script {
  Get-PnpDevice -PresentOnly -ErrorAction Stop |
    Where-Object {
      $_.InstanceId -match 'VID_054C&PID_0DF2' -and
      $_.InstanceId -match 'MI_03'
    } |
    Sort-Object Class,FriendlyName,InstanceId |
    Format-List Class,FriendlyName,InstanceId,Status
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'pnp-edge-properties.txt') -Script {
  Get-PnpDevice -PresentOnly -ErrorAction Stop |
    Where-Object { $_.InstanceId -match 'VID_054C&PID_0DF2' } |
    ForEach-Object {
      "### $($_.InstanceId)"
      Get-PnpDeviceProperty -InstanceId $_.InstanceId -ErrorAction Continue |
        Where-Object {
          $_.KeyName -match 'DEVPKEY_Device_(Class|ClassGuid|Driver|FriendlyName|HardwareIds|InstanceId|Manufacturer|Parent|ProblemCode|Service|Status)'
        } |
        Format-Table KeyName,Data -AutoSize
      ''
    }
}

Write-Host "Evidence written to $OutputDir"
Write-Host "Attach skipped: $SkipAttach"
