#!/usr/bin/env pwsh
<#
.SYNOPSIS
Verifies the synthetic DualSense Edge evidence-summary cases.

.DESCRIPTION
Imports only function definitions from capture-dualsense-edge-evidence.ps1, then
checks that representative native SDL2 and sdl2-compat/SDL3 logs pass while
under-counted native SDL2 logs, duplicated one-at-a-time Edge paddle/Fn states,
and host-input stop patterns fail.
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

function Test-HostInputSummary {
  param(
    [Parameter(Mandatory = $true)][string] $Name,
    [Parameter(Mandatory = $true)][string[]] $Lines,
    [Parameter(Mandatory = $true)][string[]] $Patterns,
    [Parameter(Mandatory = $true)][string[]] $StopPatterns,
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

$sdl2Detection = 'DualSense Edge detected with 21 SDL joystick buttons (need 21 for SDL2 Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.10)'
$sdl2Bindings = 'DualSense Edge arrival support: buttons=21 sdlType=3 arrivalType=0x02 supportedButtonFlags=0x000f0000 paddle/Fn=0x000f0000 capabilities=0x00000000 bindings=paddle1=b20,paddle2=b19,paddle3=b18,paddle4=b17'
$sdl2Mapping = 'Applied DualSense Edge paddle mappings (updated): paddle1:b20,paddle2:b19,paddle3:b18,paddle4:b17'

$sdl2UndercountDetection = 'DualSense Edge detected with 20 SDL joystick buttons (need 21 for SDL2 Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.10)'

$sdl3Detection = 'DualSense Edge detected with 17 SDL joystick buttons (need 17 for SDL3/sdl2-compat Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.70, SDL3 3.4.11)'
$sdl3Bindings = 'DualSense Edge arrival support: buttons=17 sdlType=3 arrivalType=0x02 supportedButtonFlags=0x000f0000 paddle/Fn=0x000f0000 capabilities=0x00000000 bindings=paddle1=b16,paddle2=b15,paddle3=b14,paddle4=b13'
$sdl3Mapping = 'Applied DualSense Edge paddle mappings (updated): paddle1:b16,paddle2:b15,paddle3:b14,paddle4:b13'
$sdl3UndercountDetection = 'DualSense Edge detected with 16 SDL joystick buttons (need 17 for SDL3/sdl2-compat Edge raw mapping) (VID/PID: 0x054c/0x0df2) (SDL 2.32.70, SDL3 3.4.11)'
$sdl3WrongBindings = 'DualSense Edge arrival support: buttons=17 sdlType=3 arrivalType=0x02 supportedButtonFlags=0x000f0000 paddle/Fn=0x000f0000 capabilities=0x00000000 bindings=paddle1=b16,paddle2=b15,paddle3=b14,paddle4=b12'

Test-LogSummary `
  -Name 'sdl2-pass' `
  -DetectionLine $sdl2Detection `
  -BindingsLine $sdl2Bindings `
  -MappingLine $sdl2Mapping `
  -ExpectedOverall 'PASS'

Test-LogSummary `
  -Name 'sdl3compat-pass' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl3Bindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'PASS'

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
  -Name 'sdl3compat-wrong-bindings-fail' `
  -DetectionLine $sdl3Detection `
  -BindingsLine $sdl3WrongBindings `
  -MappingLine $sdl3Mapping `
  -ExpectedOverall 'FAIL' `
  -ExpectedSummaryLines @(
    'MISSING: Moonlight advertised PlayStation Edge arrival and full paddle/Fn support'
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
    'Ignoring DualSense Edge controller button a bound to raw paddle1:b16 as stale Edge raw alias',
    'DualSense Edge controller button b is bound to raw paddle2:b15; not advertising it as a normal button',
    'Gamepad 0 is using DualSense Edge mode, but the client did not report a PlayStation controller with all Edge paddle/Fn buttons (type [02], paddle/Fn [00070000]); refusing to allocate virtual Edge controller.'
  ) `
  -ExpectedSummaryLines @(
    'STOP_MATCH: STOP: Moonlight mapping did not expose expected paddle controller bindings',
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

Write-Host 'DualSense Edge evidence verification passed.'
