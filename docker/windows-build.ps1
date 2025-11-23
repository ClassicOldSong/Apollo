# PowerShell build script for Apollo Windows build
# This script runs INSIDE the Windows container

param(
    [string]$BuildType = "Release",
    [switch]$SkipConfigure = $false,
    [switch]$SkipBuild = $false,
    [switch]$SkipPackage = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Apollo Windows Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Type: $BuildType" -ForegroundColor Green
Write-Host "Working Directory: $(Get-Location)" -ForegroundColor Green
Write-Host ""

# MSYS2 paths
$msys2Bash = "C:\tools\msys64\usr\bin\bash.exe"
$msys2Mingw = "C:\tools\msys64\mingw64\bin"

# Verify MSYS2 is installed
if (-not (Test-Path $msys2Bash)) {
    Write-Host "ERROR: MSYS2 not found at $msys2Bash" -ForegroundColor Red
    exit 1
}

# Add MSYS2 to PATH for this session
$env:PATH = "$msys2Mingw;$env:PATH"

# Convert Windows path to MSYS2 path format
$workspacePath = (Get-Location).Path -replace '\\', '/' -replace 'C:', '/c' -replace 'D:', '/d' -replace 'E:', '/e' -replace 'F:', '/f'

Write-Host "MSYS2 Workspace Path: $workspacePath" -ForegroundColor Yellow
Write-Host ""

# Configure CMake
if (-not $SkipConfigure) {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Configuring CMake..." -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    & $msys2Bash -lc "cd '$workspacePath' && cmake -B build -G Ninja -S . -DCMAKE_BUILD_TYPE=$BuildType -DBUILD_DOCS=OFF -DBUILD_TESTS=OFF"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "CMake configuration completed successfully!" -ForegroundColor Green
    Write-Host ""
} else {
    Write-Host "Skipping CMake configuration..." -ForegroundColor Yellow
    Write-Host ""
}

# Build
if (-not $SkipBuild) {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Building Apollo..." -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    & $msys2Bash -lc "cd '$workspacePath' && ninja -C build"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Build failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host ""
} else {
    Write-Host "Skipping build..." -ForegroundColor Yellow
    Write-Host ""
}

# Package
if (-not $SkipPackage) {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Packaging..." -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    # Create ZIP package
    Write-Host "Creating ZIP package..." -ForegroundColor Yellow
    & $msys2Bash -lc "cd '$workspacePath' && cpack -G ZIP --config ./build/CPackConfig.cmake"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "WARNING: ZIP packaging failed!" -ForegroundColor Yellow
    } else {
        Write-Host "ZIP package created successfully!" -ForegroundColor Green
    }
    
    # Create NSIS installer
    Write-Host "Creating NSIS installer..." -ForegroundColor Yellow
    & $msys2Bash -lc "cd '$workspacePath' && cpack -G NSIS --config ./build/CPackConfig.cmake"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "WARNING: NSIS installer creation failed!" -ForegroundColor Yellow
    } else {
        Write-Host "NSIS installer created successfully!" -ForegroundColor Green
    }
    
    Write-Host ""
} else {
    Write-Host "Skipping packaging..." -ForegroundColor Yellow
    Write-Host ""
}

# List artifacts
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Artifacts" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$artifactsDir = Join-Path (Get-Location) "build\cpack_artifacts"
if (Test-Path $artifactsDir) {
    Write-Host "Artifacts directory: $artifactsDir" -ForegroundColor Green
    Get-ChildItem $artifactsDir | ForEach-Object {
        Write-Host "  - $($_.Name) ($([math]::Round($_.Length / 1MB, 2)) MB)" -ForegroundColor White
    }
} else {
    Write-Host "Artifacts directory not found: $artifactsDir" -ForegroundColor Yellow
}

$exePath = Join-Path (Get-Location) "build\sunshine.exe"
if (Test-Path $exePath) {
    Write-Host ""
    Write-Host "Executable: $exePath" -ForegroundColor Green
    $exeInfo = Get-Item $exePath
    Write-Host "  Size: $([math]::Round($exeInfo.Length / 1MB, 2)) MB" -ForegroundColor White
    Write-Host "  Modified: $($exeInfo.LastWriteTime)" -ForegroundColor White
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
