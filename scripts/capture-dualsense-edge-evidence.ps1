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
  [int]$Port = 3240,
  [string]$BusId = '1-1',
  [string]$OutputDir = (Join-Path $env:TEMP ("apollo-edge-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))),
  [string[]]$LogPath = @(),
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

function Copy-RequestedLogs {
  param(
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [Parameter(Mandatory = $true)][string[]]$Paths
  )

  $manifest = New-Object System.Collections.Generic.List[string]
  if ($Paths.Count -eq 0) {
    $manifest.Add('No log paths provided. Pass -LogPath to copy Apollo/Moonlight logs into this evidence bundle.')
    Write-TextFile -Path (Join-Path $OutputDir 'logs-manifest.txt') -Lines $manifest.ToArray()
    return
  }

  $logDir = Join-Path $OutputDir 'logs'
  New-Item -ItemType Directory -Force -Path $logDir | Out-Null

  for ($i = 0; $i -lt $Paths.Count; $i++) {
    $source = $Paths[$i]
    try {
      $item = Get-Item -LiteralPath $source -ErrorAction Stop
      $destination = Join-Path $logDir ("{0:D2}-{1}" -f $i, $item.Name)
      Copy-Item -LiteralPath $item.FullName -Destination $destination -Recurse -Force -ErrorAction Stop
      $manifest.Add("copied[$i]=$($item.FullName) -> $destination")
    } catch {
      $manifest.Add("failed[$i]=$source")
      $manifest.Add("error[$i]=$($_.Exception.Message)")
    }
  }

  Write-TextFile -Path (Join-Path $OutputDir 'logs-manifest.txt') -Lines $manifest.ToArray()
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$EdgeDevicePattern = 'VID_054C&PID_0DF2'
$EdgeHidInterfacePattern = 'VID_054C&PID_0DF2.*MI_03'

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-TextFile -Path (Join-Path $OutputDir 'environment.txt') -Lines @(
  "timestamp=$(Get-Date -Format o)",
  "computer=$env:COMPUTERNAME",
  "user=$env:USERNAME",
  "is_admin=$isAdmin",
  "host=$HostAddress",
  "port=$Port",
  "busid=$BusId",
  "log_path_count=$($LogPath.Count)",
  "skip_attach=$SkipAttach",
  "powershell=$($PSVersionTable.PSVersion)"
)

Copy-RequestedLogs -OutputDir $OutputDir -Paths $LogPath

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'usbip-command.txt') -Script {
  Get-Command usbip.exe -ErrorAction Stop | Format-List Name,Source,Version
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'usbip-services.txt') -Script {
  Get-Service -ErrorAction Stop |
    Where-Object {
      $_.Name -match 'usbip|vhci|ude' -or
      $_.DisplayName -match 'USB/IP|usbip|virtual host controller|UDE'
    } |
    Sort-Object Name |
    Format-Table Name,DisplayName,Status,StartType -AutoSize
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'apollo-listener.txt') -Script {
  Get-NetTCPConnection -LocalAddress $HostAddress -LocalPort $Port -State Listen -ErrorAction Stop |
    ForEach-Object {
      $process = Get-Process -Id $_.OwningProcess -ErrorAction SilentlyContinue
      $processName = ''
      $processPath = ''
      if ($process) {
        $processName = $process.ProcessName
        try {
          $processPath = $process.Path
        } catch {
          $processPath = ''
        }
      }

      [PSCustomObject]@{
        LocalAddress = $_.LocalAddress
        LocalPort = $_.LocalPort
        State = $_.State
        OwningProcess = $_.OwningProcess
        ProcessName = $processName
        Path = $processPath
      }
    } |
    Format-List
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'apollo-listener-netstat.txt') -Script {
  & netstat.exe -ano -p tcp |
    Select-String -Pattern (":$Port\s")
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
    Where-Object { $_.InstanceId -match $EdgeDevicePattern } |
    Sort-Object Class,FriendlyName,InstanceId |
    Format-List Class,FriendlyName,InstanceId,Status
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'pnp-edge-mi03.txt') -Script {
  Get-PnpDevice -PresentOnly -ErrorAction Stop |
    Where-Object { $_.InstanceId -match $EdgeHidInterfacePattern } |
    Sort-Object Class,FriendlyName,InstanceId |
    Format-List Class,FriendlyName,InstanceId,Status
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'pnp-edge-hidclass.txt') -Script {
  Get-PnpDevice -PresentOnly -Class HIDClass -ErrorAction Stop |
    Where-Object {
      $_.InstanceId -match $EdgeDevicePattern -or
      $_.FriendlyName -match 'DualSense Edge'
    } |
    Sort-Object Class,FriendlyName,InstanceId |
    Format-List Class,FriendlyName,InstanceId,Status
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'pnp-edge-properties.txt') -Script {
  Get-PnpDevice -PresentOnly -ErrorAction Stop |
    Where-Object { $_.InstanceId -match $EdgeDevicePattern } |
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

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'pnp-edge-cim.txt') -Script {
  Get-CimInstance Win32_PnPEntity -ErrorAction Stop |
    Where-Object { $_.PNPDeviceID -match $EdgeDevicePattern } |
    Sort-Object PNPClass,Name,PNPDeviceID |
    Select-Object Name,PNPClass,Status,Service,Manufacturer,PNPDeviceID |
    Format-List
}

Write-Host "Evidence written to $OutputDir"
Write-Host "Attach skipped: $SkipAttach"
