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
  [string[]]$HostInputEvidencePath = @(),
  [string[]]$HostInputEvidencePattern = @(),
  [string[]]$HostInputStopPattern = @(),
  [switch]$SkipAttach,
  [int]$PostAttachDelaySeconds = 3,
  [switch]$RequirePass,
  [switch]$RequireHostInputEvidence
)

$ErrorActionPreference = 'Continue'

function Write-TextFile {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][AllowEmptyString()][AllowEmptyCollection()][string[]]$Lines
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

function Get-PnpDevicePropertyData {
  param(
    [Parameter(Mandatory = $true)][string]$InstanceId,
    [Parameter(Mandatory = $true)][string]$KeyName
  )

  try {
    $property = Get-PnpDeviceProperty -InstanceId $InstanceId -KeyName $KeyName -ErrorAction Stop
    return $property.Data
  } catch {
    return $null
  }
}

function Add-CopiedPathHashes {
  param(
    [Parameter(Mandatory = $true)][System.Collections.Generic.List[string]]$Manifest,
    [Parameter(Mandatory = $true)][int]$Index,
    [Parameter(Mandatory = $true)][string]$Path
  )

  try {
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    if (-not $item.PSIsContainer) {
      $hash = Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256 -ErrorAction Stop
      $Manifest.Add("copied[$Index].sha256=$($hash.Hash.ToLowerInvariant())")
      return
    }

    $root = $item.FullName.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $files = @(Get-ChildItem -LiteralPath $item.FullName -Recurse -File -Force -ErrorAction Stop | Sort-Object FullName)
    $Manifest.Add("copied[$Index].file_count=$($files.Count)")
    for ($fileIndex = 0; $fileIndex -lt $files.Count; $fileIndex++) {
      $file = $files[$fileIndex]
      $relativePath = $file.FullName.Substring($root.Length).TrimStart([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
      $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256 -ErrorAction Stop
      $Manifest.Add("copied[$Index].file[$fileIndex].relative_path=$relativePath")
      $Manifest.Add("copied[$Index].file[$fileIndex].size_bytes=$($file.Length)")
      $Manifest.Add("copied[$Index].file[$fileIndex].sha256=$($hash.Hash.ToLowerInvariant())")
    }
  } catch {
    $Manifest.Add("copied[$Index].hash_error=$($_.Exception.Message)")
  }
}

function Write-EvidenceBundleManifest {
  param(
    [Parameter(Mandatory = $true)][string]$OutputDir
  )

  $manifestPath = Join-Path $OutputDir 'evidence-artifacts-manifest.txt'
  $manifest = New-Object System.Collections.Generic.List[string]

  try {
    $rootItem = Get-Item -LiteralPath $OutputDir -ErrorAction Stop
    $manifestFullPath = [System.IO.Path]::GetFullPath((Join-Path $rootItem.FullName 'evidence-artifacts-manifest.txt'))
    $root = $rootItem.FullName.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $files = @(
      Get-ChildItem -LiteralPath $rootItem.FullName -Recurse -File -Force -ErrorAction Stop |
        Where-Object { [System.IO.Path]::GetFullPath($_.FullName) -ne $manifestFullPath } |
        Sort-Object FullName
    )

    $manifest.Add("root=$($rootItem.FullName)")
    $manifest.Add("generated_utc=$((Get-Date).ToUniversalTime().ToString('o'))")
    $manifest.Add("artifact_count=$($files.Count)")
    $manifest.Add("excluded=evidence-artifacts-manifest.txt")

    for ($i = 0; $i -lt $files.Count; $i++) {
      $file = $files[$i]
      $relativePath = $file.FullName.Substring($root.Length).TrimStart([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
      $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256 -ErrorAction Stop
      $manifest.Add("artifact[$i].relative_path=$relativePath")
      $manifest.Add("artifact[$i].size_bytes=$($file.Length)")
      $manifest.Add("artifact[$i].sha256=$($hash.Hash.ToLowerInvariant())")
    }
  } catch {
    $manifest.Add("manifest_error=$($_.Exception.Message)")
  }

  Write-TextFile -Path $manifestPath -Lines $manifest.ToArray()
}

function Copy-RequestedLogs {
  param(
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Paths
  )

  $manifest = New-Object System.Collections.Generic.List[string]
  $logDir = Join-Path $OutputDir 'logs'
  if (Test-Path -LiteralPath $logDir) {
    Remove-Item -LiteralPath $logDir -Recurse -Force -ErrorAction Stop
    $manifest.Add("cleared_existing_logs_dir=$logDir")
  }

  if ($Paths.Count -eq 0) {
    $manifest.Add('No log paths provided. Pass -LogPath to copy Apollo/Moonlight logs into this evidence bundle.')
    Write-TextFile -Path (Join-Path $OutputDir 'logs-manifest.txt') -Lines $manifest.ToArray()
    return
  }

  New-Item -ItemType Directory -Force -Path $logDir | Out-Null

  for ($i = 0; $i -lt $Paths.Count; $i++) {
    $source = $Paths[$i]
    try {
      $item = Get-Item -LiteralPath $source -ErrorAction Stop
      $destination = Join-Path $logDir ("{0:D2}-{1}" -f $i, $item.Name)
      Copy-Item -LiteralPath $item.FullName -Destination $destination -Recurse -Force -ErrorAction Stop
      $manifest.Add("copied[$i]=$($item.FullName) -> $destination")
      $manifest.Add("copied[$i].type=$(if ($item.PSIsContainer) { 'directory' } else { 'file' })")
      $manifest.Add("copied[$i].last_write_utc=$($item.LastWriteTimeUtc.ToString('o'))")
      if (-not $item.PSIsContainer) {
        $manifest.Add("copied[$i].size_bytes=$($item.Length)")
      }
      Add-CopiedPathHashes -Manifest $manifest -Index $i -Path $destination
    } catch {
      $manifest.Add("failed[$i]=$source")
      $manifest.Add("error[$i]=$($_.Exception.Message)")
    }
  }

  Write-TextFile -Path (Join-Path $OutputDir 'logs-manifest.txt') -Lines $manifest.ToArray()
}

function Get-EvidenceSummaryOverall {
  param(
    [Parameter(Mandatory = $true)][string]$Path
  )

  if (-not (Test-Path -LiteralPath $Path)) {
    return 'MISSING'
  }

  $line = Get-Content -LiteralPath $Path -Tail 20 -ErrorAction SilentlyContinue |
    Where-Object { $_ -match '^overall=(PASS|FAIL|SKIPPED)$' } |
    Select-Object -Last 1

  if ($line -match '^overall=(PASS|FAIL|SKIPPED)$') {
    return $Matches[1]
  }

  return 'MISSING'
}

function Copy-RequestedHostInputEvidence {
  param(
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Paths
  )

  $manifest = New-Object System.Collections.Generic.List[string]
  $evidenceDir = Join-Path $OutputDir 'host-input'
  if (Test-Path -LiteralPath $evidenceDir) {
    Remove-Item -LiteralPath $evidenceDir -Recurse -Force -ErrorAction Stop
    $manifest.Add("cleared_existing_host_input_dir=$evidenceDir")
  }

  if ($Paths.Count -eq 0) {
    $manifest.Add('No host input evidence paths provided. Pass -HostInputEvidencePath with HID/gamepad inspector exports, logs, or screenshots.')
    Write-TextFile -Path (Join-Path $OutputDir 'host-input-manifest.txt') -Lines $manifest.ToArray()
    return
  }

  New-Item -ItemType Directory -Force -Path $evidenceDir | Out-Null

  for ($i = 0; $i -lt $Paths.Count; $i++) {
    $source = $Paths[$i]
    try {
      $item = Get-Item -LiteralPath $source -ErrorAction Stop
      $destination = Join-Path $evidenceDir ("{0:D2}-{1}" -f $i, $item.Name)
      Copy-Item -LiteralPath $item.FullName -Destination $destination -Recurse -Force -ErrorAction Stop
      $manifest.Add("copied[$i]=$($item.FullName) -> $destination")
      $manifest.Add("copied[$i].type=$(if ($item.PSIsContainer) { 'directory' } else { 'file' })")
      $manifest.Add("copied[$i].last_write_utc=$($item.LastWriteTimeUtc.ToString('o'))")
      if (-not $item.PSIsContainer) {
        $manifest.Add("copied[$i].size_bytes=$($item.Length)")
      }
      Add-CopiedPathHashes -Manifest $manifest -Index $i -Path $destination
    } catch {
      $manifest.Add("failed[$i]=$source")
      $manifest.Add("error[$i]=$($_.Exception.Message)")
    }
  }

  Write-TextFile -Path (Join-Path $OutputDir 'host-input-manifest.txt') -Lines $manifest.ToArray()
}

function Write-HostInputEvidenceSummary {
  param(
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Patterns,
    [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$StopPatterns,
    [Parameter(Mandatory = $true)][bool]$Required
  )

  $summary = New-Object System.Collections.Generic.List[string]
  $evidenceDir = Join-Path $OutputDir 'host-input'

  if (-not (Test-Path -LiteralPath $evidenceDir)) {
    $summary.Add('No copied host input evidence found. Pass -HostInputEvidencePath with HID/gamepad inspector exports, logs, or screenshots.')
    $summary.Add('')
    $summary.Add($(if ($Required) { 'overall=FAIL' } else { 'overall=SKIPPED' }))
    Write-TextFile -Path (Join-Path $OutputDir 'host-input-evidence-summary.txt') -Lines $summary.ToArray()
    return
  }

  $files = @(Get-ChildItem -LiteralPath $evidenceDir -Recurse -File -ErrorAction SilentlyContinue)
  if ($files.Count -eq 0) {
    $summary.Add('No copied host input evidence files found under the host-input directory.')
    $summary.Add('')
    $summary.Add($(if ($Required) { 'overall=FAIL' } else { 'overall=SKIPPED' }))
    Write-TextFile -Path (Join-Path $OutputDir 'host-input-evidence-summary.txt') -Lines $summary.ToArray()
    return
  }

  $hasFailure = $false
  $summary.Add("host_input_file_count=$($files.Count)")
  foreach ($file in $files) {
    $summary.Add("host_input_file=$($file.FullName) last_write_utc=$($file.LastWriteTimeUtc.ToString('o')) size_bytes=$($file.Length)")
  }
  $summary.Add('')

  if ($Patterns.Count -eq 0) {
    $summary.Add('No -HostInputEvidencePattern values provided. A reviewer must manually inspect the copied host-input evidence for four distinct Edge paddle/Fn inputs.')
  } else {
    for ($i = 0; $i -lt $Patterns.Count; $i++) {
      $pattern = $Patterns[$i]
      $matches = New-Object System.Collections.Generic.List[string]

      foreach ($file in $files) {
        try {
          Select-String -LiteralPath $file.FullName -Pattern $pattern -ErrorAction Stop |
            Select-Object -First 20 |
            ForEach-Object {
              $matches.Add(("{0}:{1}: {2}" -f $file.Name, $_.LineNumber, $_.Line.Trim()))
            }
        } catch {
          $summary.Add(("{0}: ERROR reading host input evidence for pattern[{1}]: {2}" -f $file.Name, $i, $_.Exception.Message))
        }
      }

      if ($matches.Count -gt 0) {
        $summary.Add("MATCH: host input pattern[$i] $pattern")
      } else {
        $hasFailure = $true
        $summary.Add("MISSING: host input pattern[$i] $pattern")
      }

      foreach ($match in $matches) {
        $summary.Add("  $match")
      }
    }
  }

  if ($StopPatterns.Count -eq 0) {
    $summary.Add('No -HostInputStopPattern values provided. A reviewer must manually inspect the copied host-input evidence for face-button remaps or duplicated Edge controls.')
  } else {
    for ($i = 0; $i -lt $StopPatterns.Count; $i++) {
      $pattern = $StopPatterns[$i]
      $matches = New-Object System.Collections.Generic.List[string]

      foreach ($file in $files) {
        try {
          Select-String -LiteralPath $file.FullName -Pattern $pattern -ErrorAction Stop |
            Select-Object -First 20 |
            ForEach-Object {
              $matches.Add(("{0}:{1}: {2}" -f $file.Name, $_.LineNumber, $_.Line.Trim()))
            }
        } catch {
          $summary.Add(("{0}: ERROR reading host input evidence for stop pattern[{1}]: {2}" -f $file.Name, $i, $_.Exception.Message))
        }
      }

      if ($matches.Count -gt 0) {
        $hasFailure = $true
        $summary.Add("STOP_MATCH: host input stop pattern[$i] $pattern")
      } else {
        $summary.Add("OK_NO_STOP_MATCH: host input stop pattern[$i] $pattern")
      }

      foreach ($match in $matches) {
        $summary.Add("  $match")
      }
    }
  }

  $summary.Add('')
  if ($hasFailure) {
    $summary.Add('overall=FAIL')
  } else {
    $summary.Add('overall=PASS')
  }

  Write-TextFile -Path (Join-Path $OutputDir 'host-input-evidence-summary.txt') -Lines $summary.ToArray()
}

function Write-LogEvidenceSummary {
  param(
    [Parameter(Mandatory = $true)][string]$OutputDir
  )

  $summary = New-Object System.Collections.Generic.List[string]
  $logDir = Join-Path $OutputDir 'logs'

  if (-not (Test-Path -LiteralPath $logDir)) {
    $summary.Add('No copied logs found. Pass Apollo and Moonlight logs with -LogPath to summarize software-side evidence.')
    $summary.Add('')
    $summary.Add('overall=FAIL')
    Write-TextFile -Path (Join-Path $OutputDir 'log-evidence-summary.txt') -Lines $summary.ToArray()
    return
  }

  $logs = @(Get-ChildItem -LiteralPath $logDir -Recurse -File -ErrorAction SilentlyContinue)
  if ($logs.Count -eq 0) {
    $summary.Add('No copied log files found under the logs directory.')
    $summary.Add('')
    $summary.Add('overall=FAIL')
    Write-TextFile -Path (Join-Path $OutputDir 'log-evidence-summary.txt') -Lines $summary.ToArray()
    return
  }

  $edgeRawButtonCountPattern =
    'DualSense Edge detected with (2[1-9]|[3-9][0-9]|[1-9][0-9]{2,}) SDL joystick buttons \(need 21 for SDL2 Edge raw mapping\).*VID/PID: 0x054c/0x0df2|' +
    'DualSense Edge detected with (1[7-9]|[2-9][0-9]|[1-9][0-9]{2,}) SDL joystick buttons \(need 17 for SDL3/sdl2-compat Edge raw mapping\).*VID/PID: 0x054c/0x0df2'
  $edgeMappingVariantPattern =
    '(?s)(?:(?=.*DualSense Edge detected with (2[1-9]|[3-9][0-9]|[1-9][0-9]{2,}) SDL joystick buttons \(need 21 for SDL2 Edge raw mapping\).*VID/PID: 0x054c/0x0df2)(?=.*DualSense Edge arrival support:.*bindings=paddle1=b20,paddle2=b19,paddle3=b18,paddle4=b17)|' +
    '(?=.*DualSense Edge detected with (1[7-9]|[2-9][0-9]|[1-9][0-9]{2,}) SDL joystick buttons \(need 17 for SDL3/sdl2-compat Edge raw mapping\).*VID/PID: 0x054c/0x0df2)(?=.*DualSense Edge arrival support:.*bindings=paddle1=b16,paddle2=b15,paddle3=b14,paddle4=b13))'
  $moonlightCompleteLogPattern =
    $edgeMappingVariantPattern +
    '(?=.*DualSense Edge detected with.*\(SDL [0-9]+\.[0-9]+\.[0-9]+(, SDL3 [0-9]+\.[0-9]+\.[0-9]+)?\))' +
    '(?=.*DualSense Edge arrival support:.*arrivalType=0x02.*paddle/Fn=0x000f0000.*bindings=(paddle1=b20,paddle2=b19,paddle3=b18,paddle4=b17|paddle1=b16,paddle2=b15,paddle3=b14,paddle4=b13))' +
    '(?=.*(Applied DualSense Edge paddle mappings|DualSense Edge paddle mappings already present))' +
    '(?=.*DualSense Edge PADDLE1 pressed \(paddle/Fn flags: 0x00010000\))' +
    '(?=.*DualSense Edge PADDLE2 pressed \(paddle/Fn flags: 0x00020000\))' +
    '(?=.*DualSense Edge PADDLE3 pressed \(paddle/Fn flags: 0x00040000\))' +
    '(?=.*DualSense Edge PADDLE4 pressed \(paddle/Fn flags: 0x00080000\))' +
    '(?=.*DualSense Edge PADDLE1 released \(paddle/Fn flags: 0x00000000\))' +
    '(?=.*DualSense Edge PADDLE2 released \(paddle/Fn flags: 0x00000000\))' +
    '(?=.*DualSense Edge PADDLE3 released \(paddle/Fn flags: 0x00000000\))' +
    '(?=.*DualSense Edge PADDLE4 released \(paddle/Fn flags: 0x00000000\))'
  $apolloCompleteLogPattern =
    '(?s)(?=.*DualSense Edge USB/IP backend is listening on 127\.0\.0\.1:3240)' +
    '(?=.*will be DualSense Edge controller \(experimental USB/IP\))' +
    '(?=.*reported DualSense Edge arrival type \[02\] and paddle/Fn buttons \[000F0000\])' +
    '(?=.*DualSense Edge USB/IP: client imported virtual controller)' +
    '(?=.*DualSense Edge paddle/Fn state changed \[00010000\])' +
    '(?=.*DualSense Edge paddle/Fn state changed \[00020000\])' +
    '(?=.*DualSense Edge paddle/Fn state changed \[00040000\])' +
    '(?=.*DualSense Edge paddle/Fn state changed \[00080000\])' +
    '(?=(.*DualSense Edge paddle/Fn state changed \[00000000\]){4})'

  $checks = @(
    @{ Name = 'Moonlight detected DualSense Edge VID/PID'; Required = $true; Stop = $false; Pattern = 'DualSense Edge detected with.*VID/PID: 0x054c/0x0df2' },
    @{ Name = 'Moonlight exposed enough Edge raw buttons'; Required = $true; Stop = $false; Pattern = $edgeRawButtonCountPattern },
    @{ Name = 'Moonlight raw-button count matches selected Edge binding variant'; Required = $true; Stop = $false; Pattern = $edgeMappingVariantPattern; SameLog = $true; MatchMessage = 'mapping variant is internally consistent' },
    @{ Name = 'Moonlight complete Edge software evidence comes from one copied log'; Required = $true; Stop = $false; Pattern = $moonlightCompleteLogPattern; SameLog = $true; MatchMessage = 'complete Moonlight Edge evidence found in one log' },
    @{ Name = 'Moonlight logged runtime SDL version'; Required = $true; Stop = $false; Pattern = 'DualSense Edge detected with.*\(SDL [0-9]+\.[0-9]+\.[0-9]+(, SDL3 [0-9]+\.[0-9]+\.[0-9]+)?\)' },
    @{ Name = 'Moonlight accepted or applied Edge paddle mapping'; Required = $true; Stop = $false; Pattern = 'Applied DualSense Edge paddle mappings|DualSense Edge paddle mappings already present' },
    @{ Name = 'Moonlight advertised PlayStation Edge arrival and full paddle/Fn support'; Required = $true; Stop = $false; Pattern = 'DualSense Edge arrival support:.*arrivalType=0x02.*paddle/Fn=0x000f0000.*bindings=(paddle1=b20,paddle2=b19,paddle3=b18,paddle4=b17|paddle1=b16,paddle2=b15,paddle3=b14,paddle4=b13)' },
    @{ Name = 'Moonlight PADDLE1 state observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge PADDLE1 pressed \(paddle/Fn flags: 0x00010000\)' },
    @{ Name = 'Moonlight PADDLE2 state observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge PADDLE2 pressed \(paddle/Fn flags: 0x00020000\)' },
    @{ Name = 'Moonlight PADDLE3 state observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge PADDLE3 pressed \(paddle/Fn flags: 0x00040000\)' },
    @{ Name = 'Moonlight PADDLE4 state observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge PADDLE4 pressed \(paddle/Fn flags: 0x00080000\)' },
    @{ Name = 'Moonlight PADDLE1 release observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge PADDLE1 released \(paddle/Fn flags: 0x00000000\)' },
    @{ Name = 'Moonlight PADDLE2 release observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge PADDLE2 released \(paddle/Fn flags: 0x00000000\)' },
    @{ Name = 'Moonlight PADDLE3 release observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge PADDLE3 released \(paddle/Fn flags: 0x00000000\)' },
    @{ Name = 'Moonlight PADDLE4 release observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge PADDLE4 released \(paddle/Fn flags: 0x00000000\)' },
    @{ Name = 'Apollo Edge USB/IP listener started'; Required = $true; Stop = $false; Pattern = 'DualSense Edge USB/IP backend is listening on 127\.0\.0\.1:3240' },
    @{ Name = 'Apollo accepted Edge allocation'; Required = $true; Stop = $false; Pattern = 'will be DualSense Edge controller \(experimental USB/IP\)' },
    @{ Name = 'Apollo received PlayStation Edge arrival and full paddle/Fn mask'; Required = $true; Stop = $false; Pattern = 'reported DualSense Edge arrival type \[02\] and paddle/Fn buttons \[000F0000\]' },
    @{ Name = 'Apollo USB/IP import observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge USB/IP: client imported virtual controller' },
    @{ Name = 'Apollo complete Edge host evidence comes from one copied log'; Required = $true; Stop = $false; Pattern = $apolloCompleteLogPattern; SameLog = $true; MatchMessage = 'complete Apollo Edge evidence found in one log' },
    @{ Name = 'Apollo PADDLE1 state observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge paddle/Fn state changed \[00010000\]' },
    @{ Name = 'Apollo PADDLE2 state observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge paddle/Fn state changed \[00020000\]' },
    @{ Name = 'Apollo PADDLE3 state observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge paddle/Fn state changed \[00040000\]' },
    @{ Name = 'Apollo PADDLE4 state observed'; Required = $true; Stop = $false; Pattern = 'DualSense Edge paddle/Fn state changed \[00080000\]' },
    @{ Name = 'Apollo Edge paddle/Fn state returned to neutral after each release'; Required = $true; Stop = $false; Pattern = 'DualSense Edge paddle/Fn state changed \[00000000\]'; Minimum = 4 },
    @{ Name = 'STOP: Moonlight combined Edge paddle/Fn state during one-at-a-time validation'; Required = $false; Stop = $true; Pattern = 'DualSense Edge PADDLE[1-4] (pressed|released) \(paddle/Fn flags: 0x000[35679a-fA-F]0000\)' },
    @{ Name = 'STOP: Apollo combined Edge paddle/Fn state during one-at-a-time validation'; Required = $false; Stop = $true; Pattern = 'DualSense Edge paddle/Fn state changed \[000[35679a-fA-F]0000\]' },
    @{ Name = 'STOP: Moonlight mapping did not expose expected paddle controller bindings'; Required = $false; Stop = $true; Pattern = 'DualSense Edge paddle mappings are present, but SDL did not expose|DualSense Edge paddle mapping update did not expose|DualSense Edge paddle mapping add did not expose' },
    @{ Name = 'STOP: Moonlight stale Edge raw alias suppressed'; Required = $false; Stop = $true; Pattern = 'as stale Edge raw alias' },
    @{ Name = 'STOP: Moonlight capability alias filtered'; Required = $false; Stop = $true; Pattern = 'not advertising it as a normal button' },
    @{ Name = 'STOP: Apollo refused Edge allocation'; Required = $false; Stop = $true; Pattern = 'refusing to allocate virtual Edge controller' }
  )

  $hasFailure = $false
  $summary.Add("log_count=$($logs.Count)")
  $logTexts = New-Object System.Collections.Generic.List[object]
  foreach ($log in $logs) {
    $summary.Add("log=$($log.FullName) last_write_utc=$($log.LastWriteTimeUtc.ToString('o')) size_bytes=$($log.Length)")
    try {
      $logTexts.Add([pscustomobject]@{
        Name = $log.Name
        Content = (Get-Content -LiteralPath $log.FullName -Raw -ErrorAction Stop)
      })
    } catch {
      $hasFailure = $true
      $summary.Add(("{0}: ERROR reading log for same-log checks: {1}" -f $log.Name, $_.Exception.Message))
    }
  }
  $summary.Add('')

  foreach ($check in $checks) {
    $matches = New-Object System.Collections.Generic.List[string]
    $minimum = 1
    if ($check.ContainsKey('Minimum')) {
      $minimum = [int]$check['Minimum']
    }

    if ($check.ContainsKey('SameLog') -and $check['SameLog']) {
      foreach ($logText in $logTexts) {
        if ([regex]::IsMatch($logText.Content, $check['Pattern'])) {
          $message = 'same-log evidence matched'
          if ($check.ContainsKey('MatchMessage')) {
            $message = $check['MatchMessage']
          }
          $matches.Add("$($logText.Name): $message")
        }
      }
    } else {
      foreach ($log in $logs) {
        try {
          Select-String -LiteralPath $log.FullName -Pattern $check['Pattern'] -ErrorAction Stop |
            Select-Object -First 20 |
            ForEach-Object {
              $matches.Add(("{0}:{1}: {2}" -f $log.Name, $_.LineNumber, $_.Line.Trim()))
            }
        } catch {
          $hasFailure = $true
          $summary.Add(("{0}: ERROR reading log for {1}: {2}" -f $log.Name, $check['Name'], $_.Exception.Message))
        }
      }
    }

    if ($check['Stop']) {
      if ($matches.Count -gt 0) {
        $hasFailure = $true
        $summary.Add("STOP_MATCH: $($check['Name'])")
      } else {
        $summary.Add("OK_NO_STOP_MATCH: $($check['Name'])")
      }
    } elseif ($check['Required']) {
      if ($matches.Count -ge $minimum) {
        $summary.Add("MATCH: $($check['Name'])")
      } else {
        $hasFailure = $true
        $summary.Add("MISSING: $($check['Name']) (matched $($matches.Count), required $minimum)")
      }
    } else {
      if ($matches.Count -gt 0) {
        $summary.Add("MATCH: $($check['Name'])")
      } else {
        $summary.Add("NO_MATCH: $($check['Name'])")
      }
    }

    foreach ($match in $matches) {
      $summary.Add("  $match")
    }
  }

  $summary.Add('')
  if ($hasFailure) {
    $summary.Add('overall=FAIL')
  } else {
    $summary.Add('overall=PASS')
  }

  Write-TextFile -Path (Join-Path $OutputDir 'log-evidence-summary.txt') -Lines $summary.ToArray()
}

function Write-SystemEvidenceSummary {
  param(
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [Parameter(Mandatory = $true)][string]$HostAddress,
    [Parameter(Mandatory = $true)][int]$Port,
    [Parameter(Mandatory = $true)][string]$BusId,
    [Parameter(Mandatory = $true)][bool]$SkipAttach
  )

  $summary = New-Object System.Collections.Generic.List[string]
  $usbipDriverPattern = 'usbip|vhci|ude|USB/IP|virtual host controller|vadim|cezanne'
  $usbipPortRequestedEdgePattern = '(?=.*' + [regex]::Escape($BusId) + ')(?=.*(?:054c:0df2|VID_054C&PID_0DF2))'
  $hasFailure = $false
  $checks = @(
    @{ Name = 'PowerShell is elevated'; Required = $true; Path = 'environment.txt'; Pattern = 'is_admin=True' },
    @{ Name = 'Apollo listener reports requested host'; Required = $true; Path = 'apollo-listener.txt'; Pattern = 'LocalAddress\s*:\s*' + [regex]::Escape($HostAddress) },
    @{ Name = 'Apollo listener reports requested port'; Required = $true; Path = 'apollo-listener.txt'; Pattern = 'LocalPort\s*:\s*' + [regex]::Escape([string]$Port) },
    @{ Name = 'usbip.exe command is resolvable'; Required = $true; Path = 'usbip-command.txt'; Pattern = 'usbip\.exe' },
    @{ Name = 'USB/IP service evidence was captured'; Required = $false; Path = 'usbip-services.txt'; Pattern = $usbipDriverPattern },
    @{ Name = 'USB/IP driver package evidence was captured'; Required = $false; Path = 'usbip-driver-packages.txt'; Pattern = $usbipDriverPattern },
    @{ Name = 'USB/IP signed driver evidence was captured'; Required = $false; Path = 'usbip-signed-drivers.txt'; Pattern = $usbipDriverPattern },
    @{ Name = 'usbip list shows requested busid'; Required = $true; Path = 'usbip-list.txt'; Pattern = [regex]::Escape($BusId) },
    @{ Name = 'usbip list shows DualSense Edge VID/PID'; Required = $true; Path = 'usbip-list.txt'; Pattern = '054c:0df2|VID_054C&PID_0DF2' },
    @{ Name = 'usbip attach command succeeded'; Required = (-not $SkipAttach); Path = 'usbip-attach.txt'; Pattern = 'exit_code=0' },
    @{ Name = 'usbip port shows requested busid'; Required = $true; Path = 'usbip-port.txt'; Pattern = [regex]::Escape($BusId) },
    @{ Name = 'usbip port shows virtual Edge VID/PID'; Required = $true; Path = 'usbip-port.txt'; Pattern = '054c:0df2|VID_054C&PID_0DF2' },
    @{ Name = 'usbip port shows requested virtual Edge import on one line'; Required = $true; Path = 'usbip-port.txt'; Pattern = $usbipPortRequestedEdgePattern },
    @{ Name = 'PnP shows DualSense Edge VID/PID'; Required = $true; Path = 'pnp-edge.txt'; Pattern = 'VID_054C&PID_0DF2' },
    @{ Name = 'PnP shows MI_03 HID child'; Required = $true; Path = 'pnp-edge-mi03.txt'; Pattern = 'VID_054C&PID_0DF2.*MI_03' },
    @{ Name = 'PnP shows at least one healthy Edge device'; Required = $true; Path = 'pnp-edge-health.txt'; Pattern = 'edge_ok_count=[1-9][0-9]*' },
    @{ Name = 'PnP shows healthy MI_03 HID child'; Required = $true; Path = 'pnp-edge-health.txt'; Pattern = 'mi03_ok_count=[1-9][0-9]*' },
    @{ Name = 'HIDClass shows Edge HID node'; Required = $true; Path = 'pnp-edge-hidclass.txt'; Pattern = 'VID_054C&PID_0DF2|DualSense Edge' },
    @{ Name = 'CIM shows Edge PnP entity'; Required = $true; Path = 'pnp-edge-cim.txt'; Pattern = 'VID_054C&PID_0DF2' }
  )

  $summary.Add("host=$HostAddress")
  $summary.Add("port=$Port")
  $summary.Add("busid=$BusId")
  $summary.Add("skip_attach=$SkipAttach")
  $summary.Add('')

  foreach ($check in $checks) {
    $path = Join-Path $OutputDir $check['Path']
    $matches = New-Object System.Collections.Generic.List[string]

    if (Test-Path -LiteralPath $path) {
      try {
        Select-String -LiteralPath $path -Pattern $check['Pattern'] -ErrorAction Stop |
          Select-Object -First 10 |
          ForEach-Object {
            $matches.Add(("{0}:{1}: {2}" -f $check['Path'], $_.LineNumber, $_.Line.Trim()))
          }
      } catch {
        $summary.Add(("{0}: ERROR reading capture for {1}: {2}" -f $check['Path'], $check['Name'], $_.Exception.Message))
      }
    }

    if ($matches.Count -gt 0) {
      $summary.Add("MATCH: $($check['Name'])")
    } elseif ($check['Required']) {
      $hasFailure = $true
      $summary.Add("MISSING: $($check['Name'])")
    } else {
      $summary.Add("SKIPPED_OR_MISSING: $($check['Name'])")
    }

    $summary.Add("  file=$path")
    foreach ($match in $matches) {
      $summary.Add("  $match")
    }
  }

  $summary.Add('')
  if ($hasFailure) {
    $summary.Add('overall=FAIL')
  } else {
    $summary.Add('overall=PASS')
  }

  Write-TextFile -Path (Join-Path $OutputDir 'system-evidence-summary.txt') -Lines $summary.ToArray()
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
  "host_input_evidence_path_count=$($HostInputEvidencePath.Count)",
  "host_input_evidence_pattern_count=$($HostInputEvidencePattern.Count)",
  "host_input_stop_pattern_count=$($HostInputStopPattern.Count)",
  "skip_attach=$SkipAttach",
  "require_pass=$RequirePass",
  "require_host_input_evidence=$RequireHostInputEvidence",
  "powershell=$($PSVersionTable.PSVersion)"
)

Copy-RequestedLogs -OutputDir $OutputDir -Paths $LogPath
Copy-RequestedHostInputEvidence -OutputDir $OutputDir -Paths $HostInputEvidencePath
Write-LogEvidenceSummary -OutputDir $OutputDir
Write-HostInputEvidenceSummary -OutputDir $OutputDir -Patterns $HostInputEvidencePattern -StopPatterns $HostInputStopPattern -Required ([bool]$RequireHostInputEvidence)

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

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'usbip-driver-packages.txt') -Script {
  $pattern = 'usbip|vhci|ude|USB/IP|virtual host controller|vadim|cezanne'
  "### pnputil /enum-drivers filtered for USB/IP-related packages"
  $output = @(& pnputil.exe /enum-drivers 2>&1)
  $matches = $output | Select-String -Pattern $pattern -Context 8,8
  if ($matches) {
    $matches
  } else {
    "No matching USB/IP driver package lines found with pattern: $pattern"
  }
}

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'usbip-signed-drivers.txt') -Script {
  Get-CimInstance Win32_PnPSignedDriver -ErrorAction Stop |
    Where-Object {
      $_.DeviceName -match 'usbip|vhci|ude|USB/IP|virtual host controller' -or
      $_.DriverProviderName -match 'usbip|vhci|ude|vadim|cezanne' -or
      $_.InfName -match 'usbip|vhci|ude'
    } |
    Sort-Object DeviceName,InfName |
    Select-Object DeviceName,Manufacturer,DriverProviderName,DriverVersion,DriverDate,InfName,IsSigned,Signer |
    Format-List
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

Invoke-PowerShellCapture -Path (Join-Path $OutputDir 'pnp-edge-health.txt') -Script {
  $records = New-Object System.Collections.Generic.List[object]

  Get-PnpDevice -PresentOnly -ErrorAction Stop |
    Where-Object { $_.InstanceId -match $EdgeDevicePattern } |
    ForEach-Object {
      $problemCode = Get-PnpDevicePropertyData -InstanceId $_.InstanceId -KeyName 'DEVPKEY_Device_ProblemCode'
      $service = Get-PnpDevicePropertyData -InstanceId $_.InstanceId -KeyName 'DEVPKEY_Device_Service'
      $driver = Get-PnpDevicePropertyData -InstanceId $_.InstanceId -KeyName 'DEVPKEY_Device_Driver'
      $parent = Get-PnpDevicePropertyData -InstanceId $_.InstanceId -KeyName 'DEVPKEY_Device_Parent'

      $problemCodeOk = $true
      if ($null -ne $problemCode) {
        try {
          $problemCodeOk = ([int]$problemCode -eq 0)
        } catch {
          $problemCodeOk = $false
        }
      }
      $isOk = ($_.Status -eq 'OK') -and $problemCodeOk

      $records.Add([PSCustomObject]@{
        Class = $_.Class
        FriendlyName = $_.FriendlyName
        InstanceId = $_.InstanceId
        Status = $_.Status
        ProblemCode = $problemCode
        Service = $service
        Driver = $driver
        Parent = $parent
        IsMi03 = ($_.InstanceId -match $EdgeHidInterfacePattern)
        IsOk = $isOk
      }) | Out-Null
    }

  $edgeOkCount = @($records | Where-Object { $_.IsOk }).Count
  $mi03OkCount = @($records | Where-Object { $_.IsMi03 -and $_.IsOk }).Count

  "device_count=$($records.Count)"
  "edge_ok_count=$edgeOkCount"
  "mi03_ok_count=$mi03OkCount"
  ''
  $records |
    Sort-Object Class,FriendlyName,InstanceId |
    Format-List Class,FriendlyName,InstanceId,Status,ProblemCode,Service,Driver,Parent,IsMi03,IsOk
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

Write-SystemEvidenceSummary -OutputDir $OutputDir -HostAddress $HostAddress -Port $Port -BusId $BusId -SkipAttach ([bool]$SkipAttach)
Write-EvidenceBundleManifest -OutputDir $OutputDir

$logEvidenceOverall = Get-EvidenceSummaryOverall -Path (Join-Path $OutputDir 'log-evidence-summary.txt')
$systemEvidenceOverall = Get-EvidenceSummaryOverall -Path (Join-Path $OutputDir 'system-evidence-summary.txt')
$hostInputEvidenceOverall = Get-EvidenceSummaryOverall -Path (Join-Path $OutputDir 'host-input-evidence-summary.txt')

Write-Host "Evidence written to $OutputDir"
Write-Host "Attach skipped: $SkipAttach"
Write-Host "Log evidence overall: $logEvidenceOverall"
Write-Host "System evidence overall: $systemEvidenceOverall"
Write-Host "Host input evidence overall: $hostInputEvidenceOverall"

if ($RequirePass -and ($logEvidenceOverall -ne 'PASS' -or $systemEvidenceOverall -ne 'PASS')) {
  Write-Error "DualSense Edge evidence validation failed. Expected log and system summaries to end with overall=PASS."
  exit 1
}

if ($RequireHostInputEvidence -and $hostInputEvidenceOverall -ne 'PASS') {
  Write-Error "DualSense Edge host input evidence validation failed. Expected host-input evidence summary to end with overall=PASS."
  exit 1
}
