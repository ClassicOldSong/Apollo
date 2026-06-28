#!/usr/bin/env pwsh
<#
.SYNOPSIS
Verifies the synthetic DualSense Edge evidence-summary cases.

.DESCRIPTION
Imports only function definitions from capture-dualsense-edge-evidence.ps1, then
checks that representative native SDL2 and sdl2-compat/SDL3 logs pass while
under-counted native SDL2 logs, mismatched or split raw-button/binding variants,
duplicated one-at-a-time Edge paddle/Fn states, and host-input stop patterns fail.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$captureScriptPath = Join-Path $PSScriptRoot 'capture-dualsense-edge-evidence.ps1'
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($captureScriptPath, [ref] $tokens, [ref] $errors)
if ($errors.Count -gt 0) {
  $errors | ForEach-Object { Write-Error $_.Message }
  exit 1
}

$ast.FindAll(
  { param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] },
  $true
) | ForEach-Object {
  Invoke-Expression $_.Extent.Text
}

function New-TestOutputDir {
  param(
    [Parameter(Mandatory = $true)][string] $Name
  )

  return Join-Path ([System.IO.Path]::GetTempPath()) ("apollo-edge-evidence-$Name-" + [guid]::NewGuid().ToString('N'))
}

function Write-TestEvidenceLog {
  param(
    [Parameter(Mandatory = $true)][string] $OutputDir,
    [Parameter(Mandatory = $true)][string] $DetectionLine,
    [Parameter(Mandatory = $true)][string] $BindingsLine,
    [Parameter(Mandatory = $true)][string] $MappingLine,
    [string[]] $ExtraLines = @()
  )

  $logDir = Join-Path $OutputDir 'logs'
  New-Item -ItemType Directory -Force -Path $logDir | Out-Null

  @(
    $DetectionLine,
    $MappingLine,
    $BindingsLine,
    'DualSense Edge PADDLE1 pressed (paddle/Fn flags: 0x00010000)',
    'DualSense Edge PADDLE2 pressed (paddle/Fn flags: 0x00020000)',
    'DualSense Edge PADDLE3 pressed (paddle/Fn flags: 0x00040000)',
    'DualSense Edge PADDLE4 pressed (paddle/Fn flags: 0x00080000)',
    'DualSense Edge PADDLE1 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE2 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE3 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE4 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge USB/IP backend is listening on 127.0.0.1:3240',
    'Gamepad 0 will be DualSense Edge controller (experimental USB/IP)',
    'Gamepad 0 reported DualSense Edge arrival type [02] and paddle/Fn buttons [000F0000]',
    'DualSense Edge USB/IP: client imported virtual controller',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00010000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00020000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00040000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00080000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]'
  ) + $ExtraLines | Set-Content -LiteralPath (Join-Path $logDir 'edge.log') -Encoding utf8
}

function Assert-Contains {
  param(
    [Parameter(Mandatory = $true)][string] $Content,
    [Parameter(Mandatory = $true)][string] $Needle,
    [Parameter(Mandatory = $true)][string] $CaseName
  )

  if (-not $Content.Contains($Needle)) {
    Write-Host $Content
    throw "$CaseName missing expected summary line: $Needle"
  }
}

function Assert-Matches {
  param(
    [Parameter(Mandatory = $true)][string] $Content,
    [Parameter(Mandatory = $true)][string] $Pattern,
    [Parameter(Mandatory = $true)][string] $CaseName
  )

  if ($Content -notmatch $Pattern) {
    Write-Host $Content
    throw "$CaseName missing expected pattern: $Pattern"
  }
}

function Test-LogSummary {
  param(
    [Parameter(Mandatory = $true)][string] $Name,
    [Parameter(Mandatory = $true)][string] $DetectionLine,
    [Parameter(Mandatory = $true)][string] $BindingsLine,
    [Parameter(Mandatory = $true)][string] $MappingLine,
    [Parameter(Mandatory = $true)][string] $ExpectedOverall,
    [string[]] $ExtraLines = @(),
    [string[]] $ExpectedSummaryLines = @()
  )

  $outputDir = New-TestOutputDir -Name $Name
  try {
    Write-TestEvidenceLog `
      -OutputDir $outputDir `
      -DetectionLine $DetectionLine `
      -BindingsLine $BindingsLine `
      -MappingLine $MappingLine `
      -ExtraLines $ExtraLines

    Write-LogEvidenceSummary -OutputDir $outputDir

    $summaryPath = Join-Path $outputDir 'log-evidence-summary.txt'
    $summary = Get-Content -LiteralPath $summaryPath -Raw
    $overall = Get-EvidenceSummaryOverall -Path $summaryPath
    if ($overall -ne $ExpectedOverall) {
      Write-Host $summary
      throw "$Name expected overall=$ExpectedOverall, got overall=$overall"
    }

    foreach ($line in $ExpectedSummaryLines) {
      Assert-Contains -Content $summary -Needle $line -CaseName $Name
    }
  } finally {
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Test-LogSummaryReadError {
  param(
    [Parameter(Mandatory = $true)][string] $Name,
    [Parameter(Mandatory = $true)][string] $DetectionLine,
    [Parameter(Mandatory = $true)][string] $BindingsLine,
    [Parameter(Mandatory = $true)][string] $MappingLine
  )

  $outputDir = New-TestOutputDir -Name $Name
  $stream = $null
  try {
    Write-TestEvidenceLog `
      -OutputDir $outputDir `
      -DetectionLine $DetectionLine `
      -BindingsLine $BindingsLine `
      -MappingLine $MappingLine

    $lockedLog = Join-Path $outputDir 'logs/locked.log'
    Set-Content -LiteralPath $lockedLog -Value 'locked evidence file' -Encoding utf8
    $stream = [System.IO.File]::Open(
      $lockedLog,
      [System.IO.FileMode]::Open,
      [System.IO.FileAccess]::ReadWrite,
      [System.IO.FileShare]::None
    )

    Write-LogEvidenceSummary -OutputDir $outputDir

    $summaryPath = Join-Path $outputDir 'log-evidence-summary.txt'
    $summary = Get-Content -LiteralPath $summaryPath -Raw
    $overall = Get-EvidenceSummaryOverall -Path $summaryPath
    if ($overall -ne 'FAIL') {
      Write-Host $summary
      throw "$Name expected overall=FAIL, got overall=$overall"
    }
    Assert-Contains -Content $summary -Needle 'locked.log: ERROR reading log for same-log checks' -CaseName $Name
  } finally {
    if ($null -ne $stream) {
      $stream.Dispose()
    }
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Write-TestSplitVariantEvidenceLog {
  param(
    [Parameter(Mandatory = $true)][string] $OutputDir,
    [Parameter(Mandatory = $true)][string] $DetectionLine,
    [Parameter(Mandatory = $true)][string] $BindingsLine,
    [Parameter(Mandatory = $true)][string] $MappingLine
  )

  $logDir = Join-Path $OutputDir 'logs'
  New-Item -ItemType Directory -Force -Path $logDir | Out-Null

  @(
    $DetectionLine
  ) | Set-Content -LiteralPath (Join-Path $logDir 'edge-detection.log') -Encoding utf8

  @(
    $MappingLine,
    $BindingsLine,
    'DualSense Edge PADDLE1 pressed (paddle/Fn flags: 0x00010000)',
    'DualSense Edge PADDLE2 pressed (paddle/Fn flags: 0x00020000)',
    'DualSense Edge PADDLE3 pressed (paddle/Fn flags: 0x00040000)',
    'DualSense Edge PADDLE4 pressed (paddle/Fn flags: 0x00080000)',
    'DualSense Edge PADDLE1 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE2 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE3 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE4 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge USB/IP backend is listening on 127.0.0.1:3240',
    'Gamepad 0 will be DualSense Edge controller (experimental USB/IP)',
    'Gamepad 0 reported DualSense Edge arrival type [02] and paddle/Fn buttons [000F0000]',
    'DualSense Edge USB/IP: client imported virtual controller',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00010000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00020000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00040000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00080000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]'
  ) | Set-Content -LiteralPath (Join-Path $logDir 'edge-arrival.log') -Encoding utf8
}

function Test-LogSummarySplitVariantEvidence {
  param(
    [Parameter(Mandatory = $true)][string] $Name,
    [Parameter(Mandatory = $true)][string] $DetectionLine,
    [Parameter(Mandatory = $true)][string] $BindingsLine,
    [Parameter(Mandatory = $true)][string] $MappingLine
  )

  $outputDir = New-TestOutputDir -Name $Name
  try {
    Write-TestSplitVariantEvidenceLog `
      -OutputDir $outputDir `
      -DetectionLine $DetectionLine `
      -BindingsLine $BindingsLine `
      -MappingLine $MappingLine

    Write-LogEvidenceSummary -OutputDir $outputDir

    $summaryPath = Join-Path $outputDir 'log-evidence-summary.txt'
    $summary = Get-Content -LiteralPath $summaryPath -Raw
    $overall = Get-EvidenceSummaryOverall -Path $summaryPath
    if ($overall -ne 'FAIL') {
      Write-Host $summary
      throw "$Name expected overall=FAIL, got overall=$overall"
    }
    Assert-Contains -Content $summary -Needle 'MISSING: Moonlight raw-button count matches selected Edge binding variant' -CaseName $Name
  } finally {
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Write-TestSplitApolloEvidenceLog {
  param(
    [Parameter(Mandatory = $true)][string] $OutputDir,
    [Parameter(Mandatory = $true)][string] $DetectionLine,
    [Parameter(Mandatory = $true)][string] $BindingsLine,
    [Parameter(Mandatory = $true)][string] $MappingLine
  )

  $logDir = Join-Path $OutputDir 'logs'
  New-Item -ItemType Directory -Force -Path $logDir | Out-Null

  @(
    $DetectionLine,
    $MappingLine,
    $BindingsLine,
    'DualSense Edge PADDLE1 pressed (paddle/Fn flags: 0x00010000)',
    'DualSense Edge PADDLE2 pressed (paddle/Fn flags: 0x00020000)',
    'DualSense Edge PADDLE3 pressed (paddle/Fn flags: 0x00040000)',
    'DualSense Edge PADDLE4 pressed (paddle/Fn flags: 0x00080000)',
    'DualSense Edge PADDLE1 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE2 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE3 released (paddle/Fn flags: 0x00000000)',
    'DualSense Edge PADDLE4 released (paddle/Fn flags: 0x00000000)'
  ) | Set-Content -LiteralPath (Join-Path $logDir 'moonlight.log') -Encoding utf8

  @(
    'DualSense Edge USB/IP backend is listening on 127.0.0.1:3240',
    'Gamepad 0 will be DualSense Edge controller (experimental USB/IP)',
    'Gamepad 0 reported DualSense Edge arrival type [02] and paddle/Fn buttons [000F0000]',
    'DualSense Edge USB/IP: client imported virtual controller'
  ) | Set-Content -LiteralPath (Join-Path $logDir 'apollo-setup.log') -Encoding utf8

  @(
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00010000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00020000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00040000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00080000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00000000]'
  ) | Set-Content -LiteralPath (Join-Path $logDir 'apollo-states.log') -Encoding utf8
}

function Test-LogSummarySplitApolloEvidence {
  param(
    [Parameter(Mandatory = $true)][string] $Name,
    [Parameter(Mandatory = $true)][string] $DetectionLine,
    [Parameter(Mandatory = $true)][string] $BindingsLine,
    [Parameter(Mandatory = $true)][string] $MappingLine
  )

  $outputDir = New-TestOutputDir -Name $Name
  try {
    Write-TestSplitApolloEvidenceLog `
      -OutputDir $outputDir `
      -DetectionLine $DetectionLine `
      -BindingsLine $BindingsLine `
      -MappingLine $MappingLine

    Write-LogEvidenceSummary -OutputDir $outputDir

    $summaryPath = Join-Path $outputDir 'log-evidence-summary.txt'
    $summary = Get-Content -LiteralPath $summaryPath -Raw
    $overall = Get-EvidenceSummaryOverall -Path $summaryPath
    if ($overall -ne 'FAIL') {
      Write-Host $summary
      throw "$Name expected overall=FAIL, got overall=$overall"
    }
    Assert-Contains -Content $summary -Needle 'MISSING: Apollo complete Edge host evidence comes from one copied log' -CaseName $Name
  } finally {
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Test-HostInputSummary {
  param(
    [Parameter(Mandatory = $true)][string] $Name,
    [Parameter(Mandatory = $true)][string[]] $Lines,
    [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]] $Patterns,
    [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]] $StopPatterns,
    [Parameter(Mandatory = $true)][string] $ExpectedOverall,
    [string[]] $ExpectedSummaryLines = @()
  )

  $outputDir = New-TestOutputDir -Name $Name
  try {
    $hostInputDir = Join-Path $outputDir 'host-input'
    New-Item -ItemType Directory -Force -Path $hostInputDir | Out-Null
    $Lines | Set-Content -LiteralPath (Join-Path $hostInputDir 'host-input.txt') -Encoding utf8

    Write-HostInputEvidenceSummary `
      -OutputDir $outputDir `
      -Patterns $Patterns `
      -StopPatterns $StopPatterns `
      -Required $true

    $summaryPath = Join-Path $outputDir 'host-input-evidence-summary.txt'
    $summary = Get-Content -LiteralPath $summaryPath -Raw
    $overall = Get-EvidenceSummaryOverall -Path $summaryPath
    if ($overall -ne $ExpectedOverall) {
      Write-Host $summary
      throw "$Name expected overall=$ExpectedOverall, got overall=$overall"
    }

    foreach ($line in $ExpectedSummaryLines) {
      Assert-Contains -Content $summary -Needle $line -CaseName $Name
    }
  } finally {
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Test-MissingHostInputSummary {
  param(
    [Parameter(Mandatory = $true)][string] $Name,
    [Parameter(Mandatory = $true)][bool] $Required,
    [Parameter(Mandatory = $true)][string] $ExpectedOverall,
    [string[]] $ExpectedSummaryLines = @()
  )

  $outputDir = New-TestOutputDir -Name $Name
  try {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

    Write-HostInputEvidenceSummary `
      -OutputDir $outputDir `
      -Patterns @() `
      -StopPatterns @() `
      -Required $Required

    $summaryPath = Join-Path $outputDir 'host-input-evidence-summary.txt'
    $summary = Get-Content -LiteralPath $summaryPath -Raw
    $overall = Get-EvidenceSummaryOverall -Path $summaryPath
    if ($overall -ne $ExpectedOverall) {
      Write-Host $summary
      throw "$Name expected overall=$ExpectedOverall, got overall=$overall"
    }

    foreach ($line in $ExpectedSummaryLines) {
      Assert-Contains -Content $summary -Needle $line -CaseName $Name
    }
  } finally {
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Test-HostInputSummaryReadError {
  param(
    [Parameter(Mandatory = $true)][string] $Name
  )

  $outputDir = New-TestOutputDir -Name $Name
  $stream = $null
  try {
    $hostInputDir = Join-Path $outputDir 'host-input'
    New-Item -ItemType Directory -Force -Path $hostInputDir | Out-Null
    Set-Content -LiteralPath (Join-Path $hostInputDir 'host-input.txt') -Encoding utf8 -Value 'PADDLE1 right rear 0x80'

    $lockedEvidence = Join-Path $hostInputDir 'locked-host-input.txt'
    Set-Content -LiteralPath $lockedEvidence -Value 'locked host input evidence' -Encoding utf8
    $stream = [System.IO.File]::Open(
      $lockedEvidence,
      [System.IO.FileMode]::Open,
      [System.IO.FileAccess]::ReadWrite,
      [System.IO.FileShare]::None
    )

    Write-HostInputEvidenceSummary `
      -OutputDir $outputDir `
      -Patterns @('PADDLE1') `
      -StopPatterns @() `
      -Required $true

    $summaryPath = Join-Path $outputDir 'host-input-evidence-summary.txt'
    $summary = Get-Content -LiteralPath $summaryPath -Raw
    $overall = Get-EvidenceSummaryOverall -Path $summaryPath
    if ($overall -ne 'FAIL') {
      Write-Host $summary
      throw "$Name expected overall=FAIL, got overall=$overall"
    }
    Assert-Contains -Content $summary -Needle 'locked-host-input.txt: ERROR reading host input evidence for pattern[0]' -CaseName $Name
  } finally {
    if ($null -ne $stream) {
      $stream.Dispose()
    }
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Test-CopiedEvidenceManifestHashes {
  $outputDir = New-TestOutputDir -Name 'manifest-hashes'
  try {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
    $logPath = Join-Path $outputDir 'apollo.log'
    $hostInputPath = Join-Path $outputDir 'host-input.txt'
    $usbipDriverLogPath = Join-Path $outputDir 'usbip-ude.txt'
    $logDirPath = Join-Path $outputDir 'apollo-log-dir'
    $hostInputDirPath = Join-Path $outputDir 'host-input-dir'
    $usbipDriverLogDirPath = Join-Path $outputDir 'usbip-driver-log-dir'
    Set-Content -LiteralPath $logPath -Encoding utf8 -Value 'apollo edge log'
    Set-Content -LiteralPath $hostInputPath -Encoding utf8 -Value 'host input evidence'
    Set-Content -LiteralPath $usbipDriverLogPath -Encoding utf8 -Value 'usbip2_ude attach trace'
    New-Item -ItemType Directory -Force -Path (Join-Path $logDirPath 'nested') | Out-Null
    New-Item -ItemType Directory -Force -Path $hostInputDirPath | Out-Null
    New-Item -ItemType Directory -Force -Path $usbipDriverLogDirPath | Out-Null
    Set-Content -LiteralPath (Join-Path $logDirPath 'apollo-dir.log') -Encoding utf8 -Value 'apollo directory log'
    Set-Content -LiteralPath (Join-Path $logDirPath 'nested/moonlight-dir.log') -Encoding utf8 -Value 'moonlight directory log'
    Set-Content -LiteralPath (Join-Path $hostInputDirPath 'host-dir.txt') -Encoding utf8 -Value 'host input directory evidence'
    Set-Content -LiteralPath (Join-Path $usbipDriverLogDirPath 'usbip-flt.txt') -Encoding utf8 -Value 'usbip2_filter attach trace'

    Copy-RequestedLogs -OutputDir $outputDir -Paths @($logPath, $logDirPath)
    Copy-RequestedHostInputEvidence -OutputDir $outputDir -Paths @($hostInputPath, $hostInputDirPath)
    Copy-RequestedUsbipDriverLogs -OutputDir $outputDir -Paths @($usbipDriverLogPath, $usbipDriverLogDirPath)

    $logManifest = Get-Content -LiteralPath (Join-Path $outputDir 'logs-manifest.txt') -Raw
    $hostInputManifest = Get-Content -LiteralPath (Join-Path $outputDir 'host-input-manifest.txt') -Raw
    $usbipDriverLogManifest = Get-Content -LiteralPath (Join-Path $outputDir 'usbip-driver-logs-manifest.txt') -Raw
    Assert-Matches `
      -Content $logManifest `
      -Pattern 'copied\[0\]\.sha256=[0-9a-f]{64}' `
      -CaseName 'manifest-hashes logs'
    Assert-Matches `
      -Content $hostInputManifest `
      -Pattern 'copied\[0\]\.sha256=[0-9a-f]{64}' `
      -CaseName 'manifest-hashes host-input'
    Assert-Matches `
      -Content $usbipDriverLogManifest `
      -Pattern 'copied\[0\]\.sha256=[0-9a-f]{64}' `
      -CaseName 'manifest-hashes usbip-driver-log'
    Assert-Contains `
      -Content $logManifest `
      -Needle 'copied[1].file_count=2' `
      -CaseName 'manifest-hashes log directory count'
    Assert-Matches `
      -Content $logManifest `
      -Pattern 'copied\[1\]\.file\[[0-9]+\]\.relative_path=.*moonlight-dir\.log' `
      -CaseName 'manifest-hashes log directory relative path'
    Assert-Matches `
      -Content $logManifest `
      -Pattern 'copied\[1\]\.file\[[0-9]+\]\.sha256=[0-9a-f]{64}' `
      -CaseName 'manifest-hashes log directory hash'
    Assert-Contains `
      -Content $hostInputManifest `
      -Needle 'copied[1].file_count=1' `
      -CaseName 'manifest-hashes host-input directory count'
    Assert-Matches `
      -Content $hostInputManifest `
      -Pattern 'copied\[1\]\.file\[0\]\.sha256=[0-9a-f]{64}' `
      -CaseName 'manifest-hashes host-input directory hash'
    Assert-Contains `
      -Content $usbipDriverLogManifest `
      -Needle 'copied[1].file_count=1' `
      -CaseName 'manifest-hashes usbip-driver-log directory count'
    Assert-Matches `
      -Content $usbipDriverLogManifest `
      -Pattern 'copied\[1\]\.file\[0\]\.sha256=[0-9a-f]{64}' `
      -CaseName 'manifest-hashes usbip-driver-log directory hash'
  } finally {
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Test-EvidenceBundleManifest {
  $outputDir = New-TestOutputDir -Name 'bundle-manifest'
  try {
    New-Item -ItemType Directory -Force -Path (Join-Path $outputDir 'logs') | Out-Null
    Set-Content -LiteralPath (Join-Path $outputDir 'environment.txt') -Encoding utf8 -Value 'is_admin=True'
    Set-Content -LiteralPath (Join-Path $outputDir 'logs/edge.log') -Encoding utf8 -Value 'edge log'
    Set-Content -LiteralPath (Join-Path $outputDir 'logs/.edge-hidden.log') -Encoding utf8 -Value 'hidden edge log'

    Push-Location -LiteralPath (Split-Path -Parent $outputDir)
    try {
      Write-EvidenceBundleManifest -OutputDir (Split-Path -Leaf $outputDir)
    } finally {
      Pop-Location
    }

    $manifest = Get-Content -LiteralPath (Join-Path $outputDir 'evidence-artifacts-manifest.txt') -Raw
    Assert-Contains `
      -Content $manifest `
      -Needle 'artifact_count=3' `
      -CaseName 'bundle-manifest count'
    Assert-Contains `
      -Content $manifest `
      -Needle 'excluded=evidence-artifacts-manifest.txt' `
      -CaseName 'bundle-manifest self exclusion'
    Assert-Matches `
      -Content $manifest `
      -Pattern 'artifact\[[0-9]+\]\.relative_path=logs[/\\]edge\.log' `
      -CaseName 'bundle-manifest nested path'
    Assert-Matches `
      -Content $manifest `
      -Pattern 'artifact\[[0-9]+\]\.relative_path=logs[/\\]\.edge-hidden\.log' `
      -CaseName 'bundle-manifest hidden nested path'
    Assert-Matches `
      -Content $manifest `
      -Pattern 'artifact\[[0-9]+\]\.sha256=[0-9a-f]{64}' `
      -CaseName 'bundle-manifest hash'
  } finally {
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Test-EvidenceArchive {
  $outputDir = New-TestOutputDir -Name 'archive'
  $archivePath = "$outputDir.zip"
  try {
    New-Item -ItemType Directory -Force -Path (Join-Path $outputDir 'logs') | Out-Null
    Set-Content -LiteralPath (Join-Path $outputDir 'environment.txt') -Encoding utf8 -Value 'is_admin=True'
    Set-Content -LiteralPath (Join-Path $outputDir 'logs/edge.log') -Encoding utf8 -Value 'edge log'

    Write-EvidenceBundleManifest -OutputDir $outputDir
    $writtenArchivePath = Write-EvidenceArchive -OutputDir $outputDir -ArchivePath $archivePath

    if ($writtenArchivePath -ne $archivePath) {
      throw "archive expected path $archivePath, got $writtenArchivePath"
    }
    if (-not (Test-Path -LiteralPath $archivePath)) {
      throw "archive was not created at $archivePath"
    }

    $summary = Get-Content -LiteralPath (Join-Path $outputDir 'archive-summary.txt') -Raw
    Assert-Contains -Content $summary -Needle 'overall=PASS' -CaseName 'archive summary pass'
    Assert-Matches -Content $summary -Pattern 'archive_sha256=[0-9a-f]{64}' -CaseName 'archive summary hash'

    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction Stop
    $archive = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
      $entryNames = @($archive.Entries | ForEach-Object { $_.FullName })
      if (-not ($entryNames -contains 'environment.txt')) {
        throw 'archive missing environment.txt'
      }
      if (-not ($entryNames -contains 'logs/edge.log')) {
        throw 'archive missing logs/edge.log'
      }
      if (-not ($entryNames -contains 'evidence-artifacts-manifest.txt')) {
        throw 'archive missing evidence-artifacts-manifest.txt'
      }
      if ($entryNames -contains 'archive-summary.txt') {
        throw 'archive should not contain archive-summary.txt because it records the final archive hash'
      }
    } finally {
      $archive.Dispose()
    }
  } finally {
    if (Test-Path -LiteralPath $archivePath) {
      Remove-Item -LiteralPath $archivePath -Force
    }
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

function Write-TestSystemEvidenceFiles {
  param(
    [Parameter(Mandatory = $true)][string] $OutputDir,
    [bool] $IncludeMi03 = $true,
    [bool] $IncludeAttachSuccess = $true,
    [bool] $IncludeUsbIpDriverEvidence = $true,
    [bool] $IncludeGameInputEvidence = $true,
    [bool] $IncludeUsbIpPortImport = $true,
    [bool] $IncludeUsbIpPortEdgeVidPid = $true,
    [bool] $SplitUsbIpPortEvidence = $false,
    [string] $ListenerAddress = '127.0.0.1'
  )

  New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

  Set-Content -LiteralPath (Join-Path $OutputDir 'environment.txt') -Encoding utf8 -Value @(
    'is_admin=True'
  )
  Set-Content -LiteralPath (Join-Path $OutputDir 'apollo-listener.txt') -Encoding utf8 -Value @(
    "LocalAddress : $ListenerAddress",
    'LocalPort    : 3240'
  )
  Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-command.txt') -Encoding utf8 -Value @(
    'Name   : usbip.exe',
    'Source : C:\Program Files\usbip-win2\usbip.exe'
  )
  if ($IncludeUsbIpDriverEvidence) {
    Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-services.txt') -Encoding utf8 -Value @(
      'Name        DisplayName                         Status  StartType',
      'usbipd-win2 USB/IP VHCI UDE Host Controller     Running Manual'
    )
    Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-driver-packages.txt') -Encoding utf8 -Value @(
      '### pnputil /enum-drivers filtered for USB/IP-related packages',
      'Driver Package Provider : usbip-win2',
      'Published Name          : oem42.inf',
      'Original Name           : usbip_vhci_ude.inf'
    )
    Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-signed-drivers.txt') -Encoding utf8 -Value @(
      'DeviceName         : USB/IP VHCI UDE Host Controller',
      'DriverProviderName : usbip-win2',
      'DriverVersion      : 1.0.0.0',
      'InfName            : usbip_vhci_ude.inf',
      'IsSigned           : True'
    )
  }
  if ($IncludeGameInputEvidence) {
    Set-Content -LiteralPath (Join-Path $OutputDir 'gameinput-service.txt') -Encoding utf8 -Value @(
      '### GameInput and Gaming Services context',
      'Name         DisplayName          Status  StartType',
      'GameInputSvc GameInput Service    Running Manual',
      'GamingServices Gaming Services    Running Manual'
    )
  }
  Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-list.txt') -Encoding utf8 -Value @(
    '1-1: Sony Corp. : DualSense Edge Wireless Controller (054c:0df2)'
  )
  if ($IncludeUsbIpPortImport) {
    $usbipPortVidPid = if ($IncludeUsbIpPortEdgeVidPid) { '054c:0df2' } else { '0000:0000' }
    if ($SplitUsbIpPortEvidence) {
      Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-port.txt') -Encoding utf8 -Value @(
        'Port 00: Imported from 127.0.0.1 busid 1-1 remote 0000:0000',
        'Port 01: Imported from 127.0.0.1 busid 9-9 remote 054c:0df2'
      )
    } else {
      Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-port.txt') -Encoding utf8 -Value @(
        "Port 00: Imported from 127.0.0.1 busid 1-1 remote $usbipPortVidPid"
      )
    }
  } else {
    Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-port.txt') -Encoding utf8 -Value @(
      'Port 00: Imported from 127.0.0.1 busid 9-9 remote 0000:0000'
    )
  }
  Set-Content -LiteralPath (Join-Path $OutputDir 'pnp-edge.txt') -Encoding utf8 -Value @(
    'InstanceId : USB\VID_054C&PID_0DF2\EDGE',
    'Status     : OK'
  )
  Set-Content -LiteralPath (Join-Path $OutputDir 'pnp-edge-hidclass.txt') -Encoding utf8 -Value @(
    'InstanceId : HID\VID_054C&PID_0DF2&MI_03\EDGE',
    'Status     : OK'
  )
  Set-Content -LiteralPath (Join-Path $OutputDir 'pnp-edge-health.txt') -Encoding utf8 -Value @(
    'device_count=2',
    'edge_ok_count=2',
    $(if ($IncludeMi03) { 'mi03_ok_count=1' } else { 'mi03_ok_count=0' })
  )
  Set-Content -LiteralPath (Join-Path $OutputDir 'pnp-edge-cim.txt') -Encoding utf8 -Value @(
    'PNPDeviceID : USB\VID_054C&PID_0DF2\EDGE',
    'Status      : OK'
  )

  if ($IncludeMi03) {
    Set-Content -LiteralPath (Join-Path $OutputDir 'pnp-edge-mi03.txt') -Encoding utf8 -Value @(
      'InstanceId : HID\VID_054C&PID_0DF2&MI_03\EDGE',
      'Status     : OK'
    )
  } else {
    Set-Content -LiteralPath (Join-Path $OutputDir 'pnp-edge-mi03.txt') -Encoding utf8 -Value @(
      'InstanceId : HID\VID_054C&PID_0DF2\EDGE',
      'Status     : OK'
    )
  }

  if ($IncludeAttachSuccess) {
    Set-Content -LiteralPath (Join-Path $OutputDir 'usbip-attach.txt') -Encoding utf8 -Value @(
      'exit_code=0'
    )
  }
}

function Test-SystemSummary {
  param(
    [Parameter(Mandatory = $true)][string] $Name,
    [Parameter(Mandatory = $true)][string] $ExpectedOverall,
    [bool] $SkipAttach = $false,
    [bool] $IncludeMi03 = $true,
    [bool] $IncludeAttachSuccess = $true,
    [bool] $IncludeUsbIpDriverEvidence = $true,
    [bool] $IncludeGameInputEvidence = $true,
    [bool] $IncludeUsbIpPortImport = $true,
    [bool] $IncludeUsbIpPortEdgeVidPid = $true,
    [bool] $SplitUsbIpPortEvidence = $false,
    [string] $ListenerAddress = '127.0.0.1',
    [string[]] $ExpectedSummaryLines = @()
  )

  $outputDir = New-TestOutputDir -Name $Name
  try {
    Write-TestSystemEvidenceFiles `
      -OutputDir $outputDir `
      -IncludeMi03 $IncludeMi03 `
      -IncludeAttachSuccess $IncludeAttachSuccess `
      -IncludeUsbIpDriverEvidence $IncludeUsbIpDriverEvidence `
      -IncludeGameInputEvidence $IncludeGameInputEvidence `
      -IncludeUsbIpPortImport $IncludeUsbIpPortImport `
      -IncludeUsbIpPortEdgeVidPid $IncludeUsbIpPortEdgeVidPid `
      -SplitUsbIpPortEvidence $SplitUsbIpPortEvidence `
      -ListenerAddress $ListenerAddress

    Write-SystemEvidenceSummary `
      -OutputDir $outputDir `
      -HostAddress '127.0.0.1' `
      -Port 3240 `
      -BusId '1-1' `
      -SkipAttach $SkipAttach

    $summaryPath = Join-Path $outputDir 'system-evidence-summary.txt'
    $summary = Get-Content -LiteralPath $summaryPath -Raw
    $overall = Get-EvidenceSummaryOverall -Path $summaryPath
    if ($overall -ne $ExpectedOverall) {
      Write-Host $summary
      throw "$Name expected overall=$ExpectedOverall, got overall=$overall"
    }

    foreach ($line in $ExpectedSummaryLines) {
      Assert-Contains -Content $summary -Needle $line -CaseName $Name
    }
  } finally {
    if (Test-Path -LiteralPath $outputDir) {
      Remove-Item -LiteralPath $outputDir -Recurse -Force
    }
  }
}

$sdl2Detection = 'DualSense Edge detected with 21 SDL joystick buttons (need 21 for SDL2 Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.10) (raw mapping signal: native SDL2 runtime)'
$sdl2Bindings = 'DualSense Edge arrival support: buttons=21 sdlType=3 arrivalType=0x02 supportedButtonFlags=0x000f0000 paddle/Fn=0x000f0000 capabilities=0x00000000 bindings=paddle1=b20,paddle2=b19,paddle3=b18,paddle4=b17'
$sdl2Mapping = 'Applied DualSense Edge paddle mappings (updated): paddle1:b20,paddle2:b19,paddle3:b18,paddle4:b17'
$sdl2PartialMapping = 'Applied DualSense Edge paddle mappings (updated): paddle2:b19,paddle4:b17'
$sdl2AlreadyPresentMapping = 'DualSense Edge paddle mappings already present'

$sdl2UndercountDetection = 'DualSense Edge detected with 20 SDL joystick buttons (need 21 for SDL2 Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.10) (raw mapping signal: native SDL2 runtime)'

$sdl3Detection = 'DualSense Edge detected with 17 SDL joystick buttons (need 17 for SDL3/sdl2-compat Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.70, SDL3 3.4.11) (raw mapping signal: SDL3_VERSION)'
$sdl3CompatNoHintDetection = 'DualSense Edge detected with 17 SDL joystick buttons (need 17 for SDL3/sdl2-compat Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.70) (raw mapping signal: sdl2-compat runtime version)'
$sdl3AmbiguousNoRuntimeSignalDetection = 'DualSense Edge detected with 17 SDL joystick buttons (need 17 for SDL3/sdl2-compat Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.10)'
$sdl3WrongSignalDetection = 'DualSense Edge detected with 17 SDL joystick buttons (need 17 for SDL3/sdl2-compat Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.70, SDL3 3.4.11) (raw mapping signal: native SDL2 runtime)'
$sdl3Bindings = 'DualSense Edge arrival support: buttons=17 sdlType=3 arrivalType=0x02 supportedButtonFlags=0x000f0000 paddle/Fn=0x000f0000 capabilities=0x00000000 bindings=paddle1=b16,paddle2=b15,paddle3=b14,paddle4=b13'
$sdl3Mapping = 'Applied DualSense Edge paddle mappings (updated): paddle1:b16,paddle2:b15,paddle3:b14,paddle4:b13'
$sdl3UndercountDetection = 'DualSense Edge detected with 16 SDL joystick buttons (need 17 for SDL3/sdl2-compat Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.70, SDL3 3.4.11) (raw mapping signal: SDL3_VERSION)'
$sdl3WrongBindings = 'DualSense Edge arrival support: buttons=17 sdlType=3 arrivalType=0x02 supportedButtonFlags=0x000f0000 paddle/Fn=0x000f0000 capabilities=0x00000000 bindings=paddle1=b16,paddle2=b15,paddle3=b14,paddle4=b12'

Test-LogSummary `
  -Name 'sdl2-pass' `
  -DetectionLine $sdl2Detection `
  -BindingsLine $sdl2Bindings `
  -MappingLine $sdl2Mapping `
  -ExpectedOverall 'PASS'

Test-LogSummary `
  -Name 'sdl2-partial-mapping-line-pass' `
  -DetectionLine $sdl2Detection `
  -BindingsLine $sdl2Bindings `
  -MappingLine $sdl2PartialMapping `
  -ExpectedOverall 'PASS'

Test-LogSummary `
  -Name 'sdl2-already-present-mapping-line-pass' `
  -DetectionLine $sdl2Detection `
  -BindingsLine $sdl2Bindings `
  -MappingLine $sdl2AlreadyPresentMapping `
  -ExpectedOverall 'PASS'

Test-LogSummary `
  -Name 'sdl3compat-pass' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'PASS'

Test-LogSummary `
  -Name 'sdl3compat-no-sdl3-hint-pass' `
  -DetectionLine $sdl3CompatNoHintDetection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'PASS'

Test-LogSummaryReadError `
  -Name 'log-read-error-fail' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping

Test-LogSummarySplitVariantEvidence `
  -Name 'split-variant-evidence-fail' `
  -DetectionLine $sdl2Detection `
  -BindingsLine $sdl2Bindings `
  -MappingLine $sdl2Mapping

Test-LogSummarySplitApolloEvidence `
  -Name 'split-apollo-evidence-fail' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping

Test-LogSummary `
  -Name 'sdl2-undercount-fail' `
  -DetectionLine $sdl2UndercountDetection `
  -BindingsLine $sdl2Bindings `
  -MappingLine $sdl2Mapping `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Moonlight exposed enough Edge raw buttons'
  )

Test-LogSummary `
  -Name 'sdl3compat-undercount-fail' `
  -DetectionLine $sdl3UndercountDetection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Moonlight exposed enough Edge raw buttons'
  )

Test-LogSummary `
  -Name 'sdl3compat-ambiguous-runtime-signal-fail' `
  -DetectionLine $sdl3AmbiguousNoRuntimeSignalDetection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Moonlight exposed enough Edge raw buttons'
  )

Test-LogSummary `
  -Name 'sdl3compat-wrong-raw-mapping-signal-fail' `
  -DetectionLine $sdl3WrongSignalDetection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Moonlight exposed enough Edge raw buttons'
  )

Test-LogSummary `
  -Name 'sdl3compat-wrong-bindings-fail' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl3WrongBindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Moonlight advertised PlayStation Edge arrival and full paddle/Fn support'
  )

Test-LogSummary `
  -Name 'sdl2-count-with-sdl3-bindings-fail' `
  -DetectionLine $sdl2Detection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Moonlight raw-button count matches selected Edge binding variant'
  )

Test-LogSummary `
  -Name 'sdl3-count-with-sdl2-bindings-fail' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl2Bindings `
  -MappingLine $sdl2Mapping `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Moonlight raw-button count matches selected Edge binding variant'
  )

Test-LogSummary `
  -Name 'combined-state-fail' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'FAIL' `
  -ExtraLines @(
    'DualSense Edge PADDLE1 pressed (paddle/Fn flags: 0x00030000)',
    'Gamepad 0 DualSense Edge paddle/Fn state changed [00090000]'
  ) `
  -ExpectedSummaryLines @(
    'STOP_MATCH: STOP: Moonlight combined Edge paddle/Fn state during one-at-a-time validation',
    'STOP_MATCH: STOP: Apollo combined Edge paddle/Fn state during one-at-a-time validation'
  )

Test-LogSummary `
  -Name 'stop-condition-fail' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'FAIL' `
  -ExtraLines @(
    'DualSense Edge paddle mapping update did not expose the expected paddle controller bindings (actual: paddle1=b16,paddle2=b15,paddle3=b14,paddle4=b12)',
    'Ignoring DualSense Edge PADDLE1 event without verified paddle bindings',
    'Ignoring DualSense Edge controller button a bound to raw paddle1:b16 as stale Edge raw alias',
    'DualSense Edge controller button b is bound to raw paddle2:b15; not advertising it as a normal button',
    'Gamepad 0 is using DualSense Edge mode, but the client did not report a PlayStation controller with all Edge paddle/Fn buttons (type [02], paddle/Fn [00070000]); refusing to allocate virtual Edge controller.'
  ) `
  -ExpectedSummaryLines @(
    'STOP_MATCH: STOP: Moonlight mapping did not expose expected paddle controller bindings',
    'STOP_MATCH: STOP: Moonlight unverified Edge paddle event ignored',
    'STOP_MATCH: STOP: Moonlight stale Edge raw alias suppressed',
    'STOP_MATCH: STOP: Moonlight capability alias filtered',
    'STOP_MATCH: STOP: Apollo refused Edge allocation'
  )

Test-HostInputSummary `
  -Name 'host-input-stop-pass' `
  -Lines @(
    'PADDLE1 right rear 0x80',
    'PADDLE2 left rear 0x40',
    'PADDLE3 right Fn 0x20',
    'PADDLE4 left Fn 0x10'
  ) `
  -Patterns @('PADDLE1', 'PADDLE2', 'PADDLE3', 'PADDLE4') `
  -StopPatterns @('same event.*face', 'same event.*left.*right') `
  -ExpectedOverall 'PASS' `
  -ExpectedSummaryLines @(
    'OK_NO_STOP_MATCH: host input stop pattern[0] same event.*face'
  )

Test-HostInputSummary `
  -Name 'host-input-missing-pattern-fail' `
  -Lines @(
    'PADDLE1 right rear 0x80',
    'PADDLE2 left rear 0x40'
  ) `
  -Patterns @('PADDLE1', 'PADDLE2', 'PADDLE3', 'PADDLE4') `
  -StopPatterns @() `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: host input pattern[2] PADDLE3',
    'MISSING: host input pattern[3] PADDLE4'
  )

Test-HostInputSummary `
  -Name 'host-input-stop-fail' `
  -Lines @(
    'PADDLE1 right rear 0x80',
    'PADDLE2 left rear 0x40',
    'PADDLE3 right Fn 0x20',
    'PADDLE4 left Fn 0x10',
    'same event right Fn and left Fn duplicated'
  ) `
  -Patterns @('PADDLE1', 'PADDLE2', 'PADDLE3', 'PADDLE4') `
  -StopPatterns @('same event.*left.*right|same event.*right.*left') `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'STOP_MATCH: host input stop pattern[0] same event.*left.*right|same event.*right.*left'
  )

Test-MissingHostInputSummary `
  -Name 'host-input-required-missing-fail' `
  -Required $true `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'No copied host input evidence found. Pass -HostInputEvidencePath with HID/gamepad inspector exports, logs, or screenshots.'
  )

Test-MissingHostInputSummary `
  -Name 'host-input-optional-missing-skipped' `
  -Required $false `
  -ExpectedOverall 'SKIPPED' `
  -ExpectedSummaryLines @(
    'No copied host input evidence found. Pass -HostInputEvidencePath with HID/gamepad inspector exports, logs, or screenshots.'
  )

Test-HostInputSummaryReadError `
  -Name 'host-input-read-error-fail'

Test-CopiedEvidenceManifestHashes
Test-EvidenceBundleManifest
Test-EvidenceArchive

Test-SystemSummary `
  -Name 'system-pass' `
  -ExpectedOverall 'PASS' `
  -ExpectedSummaryLines @(
    'MATCH: Apollo listener reports requested host',
    'MATCH: Apollo listener reports requested port',
    'MATCH: USB/IP service evidence was captured',
    'MATCH: USB/IP driver package evidence was captured',
    'MATCH: USB/IP signed driver evidence was captured',
    'MATCH: GameInput/Gaming Services context was captured',
    'MATCH: usbip port shows requested busid',
    'MATCH: usbip port shows virtual Edge VID/PID',
    'MATCH: usbip port shows requested virtual Edge import on one line',
    'MATCH: PnP shows MI_03 HID child',
    'MATCH: PnP shows healthy MI_03 HID child'
  )

Test-SystemSummary `
  -Name 'system-missing-mi03-fail' `
  -IncludeMi03 $false `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: PnP shows MI_03 HID child',
    'MISSING: PnP shows healthy MI_03 HID child'
  )

Test-SystemSummary `
  -Name 'system-wrong-listener-host-fail' `
  -ListenerAddress '0.0.0.0' `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Apollo listener reports requested host'
  )

Test-SystemSummary `
  -Name 'system-missing-usbip-port-import-fail' `
  -IncludeUsbIpPortImport $false `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: usbip port shows requested busid',
    'MISSING: usbip port shows virtual Edge VID/PID',
    'MISSING: usbip port shows requested virtual Edge import on one line'
  )

Test-SystemSummary `
  -Name 'system-usbip-port-busid-without-edge-fail' `
  -IncludeUsbIpPortEdgeVidPid $false `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MATCH: usbip port shows requested busid',
    'MISSING: usbip port shows virtual Edge VID/PID',
    'MISSING: usbip port shows requested virtual Edge import on one line'
  )

Test-SystemSummary `
  -Name 'system-usbip-port-split-import-evidence-fail' `
  -SplitUsbIpPortEvidence $true `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MATCH: usbip port shows requested busid',
    'MATCH: usbip port shows virtual Edge VID/PID',
    'MISSING: usbip port shows requested virtual Edge import on one line'
  )

Test-SystemSummary `
  -Name 'system-missing-usbip-driver-evidence-pass' `
  -IncludeUsbIpDriverEvidence $false `
  -ExpectedOverall 'PASS' `
  -ExpectedSummaryLines @(
    'SKIPPED_OR_MISSING: USB/IP service evidence was captured',
    'SKIPPED_OR_MISSING: USB/IP driver package evidence was captured',
    'SKIPPED_OR_MISSING: USB/IP signed driver evidence was captured'
  )

Test-SystemSummary `
  -Name 'system-missing-gameinput-context-pass' `
  -IncludeGameInputEvidence $false `
  -ExpectedOverall 'PASS' `
  -ExpectedSummaryLines @(
    'SKIPPED_OR_MISSING: GameInput/Gaming Services context was captured'
  )

Test-SystemSummary `
  -Name 'system-skip-attach-pass' `
  -SkipAttach $true `
  -IncludeAttachSuccess $false `
  -ExpectedOverall 'PASS' `
  -ExpectedSummaryLines @(
    'SKIPPED_OR_MISSING: usbip attach command succeeded'
  )

Write-Host 'DualSense Edge evidence verification passed.'
