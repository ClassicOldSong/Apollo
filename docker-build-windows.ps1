# PowerShell helper script to build Windows executables using Docker Windows container
# This script runs on the Windows host machine
# Usage: .\docker-build-windows.ps1

param(
    [string]$BuildType = "Release",
    [switch]$SkipConfigure = $false,
    [switch]$SkipBuild = $false,
    [switch]$SkipPackage = $false,
    [switch]$Interactive = $false
)

$ErrorActionPreference = "Stop"

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$PROJECT_ROOT = $SCRIPT_DIR
$IMAGE_NAME = "apollo-windows-builder"
$CONTAINER_NAME = "apollo-windows-build-$(Get-Random)"

function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[WARN] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Write-Header {
    param([string]$Message)
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host $Message -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

# Check Docker
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Error "Docker is not installed. Please install Docker Desktop for Windows."
    exit 1
}

try {
    docker info | Out-Null
} catch {
    Write-Error "Docker is not running. Please start Docker Desktop."
    exit 1
}

# Check if Docker is in Windows container mode
$dockerOs = docker version --format '{{.Server.Os}}' 2>$null
if ($dockerOs -eq "linux") {
    Write-Warn "Docker appears to be in Linux container mode."
    Write-Warn "Windows containers require switching to Windows container mode."
    Write-Warn "Right-click Docker Desktop icon -> 'Switch to Windows containers'"
    Write-Warn "Or run: & `$Env:ProgramFiles\Docker\Docker\DockerCli.exe -SwitchDaemon"
    $response = Read-Host "Continue anyway? (y/N)"
    if ($response -ne "y" -and $response -ne "Y") {
        exit 1
    }
}

Write-Header "Building Windows Container Image"
Write-Info "This may take several minutes on first run..."

Set-Location $PROJECT_ROOT
docker build -f docker/Dockerfile.windows -t $IMAGE_NAME .

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to build Docker image"
    exit 1
}

Write-Header "Running Windows Build"
Write-Info "Building Apollo in container..."

# Cleanup function
function Cleanup {
    Write-Info "Cleaning up container..."
    docker rm -f $CONTAINER_NAME 2>$null
}

# Register cleanup on exit
Register-EngineEvent PowerShell.Exiting -Action { Cleanup } | Out-Null

try {
    if ($Interactive) {
        Write-Info "Starting interactive container..."
        docker run --rm -it `
            --name $CONTAINER_NAME `
            -v "${PROJECT_ROOT}:C:\workspace" `
            -w C:\workspace `
            -e CMAKE_BUILD_TYPE=$BuildType `
            -e BUILD_TESTS=OFF `
            $IMAGE_NAME `
            powershell
    } else {
        # Build arguments for the build script
        $buildArgs = @()
        if ($SkipConfigure) { $buildArgs += "-SkipConfigure" }
        if ($SkipBuild) { $buildArgs += "-SkipBuild" }
        if ($SkipPackage) { $buildArgs += "-SkipPackage" }
        $buildArgs += "-BuildType", $BuildType
        
        $buildArgsString = $buildArgs -join " "
        
        docker run --rm `
            --name $CONTAINER_NAME `
            -v "${PROJECT_ROOT}:C:\workspace" `
            -w C:\workspace `
            -e CMAKE_BUILD_TYPE=$BuildType `
            -e BUILD_TESTS=OFF `
            $IMAGE_NAME `
            powershell -Command "& C:\build.ps1 $buildArgsString"
        
        if ($LASTEXITCODE -eq 0) {
            Write-Header "Build Successful!"
            Write-Info "Artifacts are in: $PROJECT_ROOT\build\cpack_artifacts\"
            Write-Info "Executable is in: $PROJECT_ROOT\build\sunshine.exe"
        } else {
            Write-Error "Build failed!"
            exit 1
        }
    }
} finally {
    Cleanup
}

