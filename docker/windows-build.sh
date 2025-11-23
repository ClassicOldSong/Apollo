#!/bin/bash
# Helper script to build Windows executables using Docker Windows container
# This script runs on the Windows host machine

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="apollo-windows-builder"
CONTAINER_NAME="apollo-windows-build-$$"

# Colors for output (if supported)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

function print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

function print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

function print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

function print_header() {
    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}========================================${NC}"
}

# Check if running on Windows
if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "win32" && "$OSTYPE" != "cygwin" ]]; then
    print_error "This script is designed for Windows. For Linux/macOS, use GitHub Actions or cross-compilation."
    exit 1
fi

# Check Docker
if ! command -v docker &> /dev/null; then
    print_error "Docker is not installed. Please install Docker Desktop for Windows."
    exit 1
fi

if ! docker info &> /dev/null; then
    print_error "Docker is not running. Please start Docker Desktop."
    exit 1
fi

# Check if Docker is in Windows container mode
# Note: This check might not work perfectly, but it's a good attempt
if docker version --format '{{.Server.Os}}' 2>/dev/null | grep -qi "linux"; then
    print_warn "Docker appears to be in Linux container mode."
    print_warn "Windows containers require switching to Windows container mode."
    print_warn "Right-click Docker Desktop icon -> 'Switch to Windows containers'"
    print_warn "Or run: & \$Env:ProgramFiles\\Docker\\Docker\\DockerCli.exe -SwitchDaemon"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

print_header "Building Windows Container Image"
print_info "This may take several minutes on first run..."

cd "$PROJECT_ROOT"
docker build -f docker/Dockerfile.windows -t "$IMAGE_NAME" .

if [ $? -ne 0 ]; then
    print_error "Failed to build Docker image"
    exit 1
fi

print_header "Running Windows Build"
print_info "Building Apollo in container..."

# Cleanup function
cleanup() {
    print_info "Cleaning up container..."
    docker rm -f "$CONTAINER_NAME" 2>/dev/null || true
}
trap cleanup EXIT

# Run build
docker run --rm \
    --name "$CONTAINER_NAME" \
    -v "${PROJECT_ROOT}:C:\workspace" \
    -w C:\workspace \
    -e CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -e BUILD_TESTS="${BUILD_TESTS:-OFF}" \
    "$IMAGE_NAME" \
    powershell -File C:\build.ps1

if [ $? -eq 0 ]; then
    print_header "Build Successful!"
    print_info "Artifacts are in: $PROJECT_ROOT/build/cpack_artifacts/"
    print_info "Executable is in: $PROJECT_ROOT/build/sunshine.exe"
else
    print_error "Build failed!"
    exit 1
fi

