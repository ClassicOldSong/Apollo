# Windows Container Build Guide

This guide explains how to build Apollo Windows executables and installers using Docker Windows containers.

## Prerequisites

1. **Windows Host Machine**: Windows 10/11 Pro, Enterprise, or Windows Server
2. **Docker Desktop**: Installed and configured for Windows containers
3. **Hyper-V**: Enabled (usually automatic on supported Windows versions)

## Setup

### 1. Switch Docker to Windows Container Mode

Before building, ensure Docker Desktop is set to use Windows containers:

**Option A: Using Docker Desktop UI**
- Right-click the Docker Desktop icon in the system tray
- Select "Switch to Windows containers"

**Option B: Using PowerShell**
```powershell
& $Env:ProgramFiles\Docker\Docker\DockerCli.exe -SwitchDaemon
```

**Verify:**
```powershell
docker version
# Should show "OS/Arch: windows/amd64" for Server
```

### 2. Verify Windows Container Support

```powershell
docker run --rm mcr.microsoft.com/windows/servercore:ltsc2022 cmd /c echo "Windows containers work!"
```

## Building

### Method 1: Using PowerShell Helper Script (Recommended)

The easiest way to build is using the provided PowerShell script:

```powershell
# Basic build (Release mode)
.\docker-build-windows.ps1

# Debug build
.\docker-build-windows.ps1 -BuildType Debug

# Skip certain steps
.\docker-build-windows.ps1 -SkipConfigure  # Skip CMake configuration
.\docker-build-windows.ps1 -SkipBuild      # Skip compilation
.\docker-build-windows.ps1 -SkipPackage   # Skip packaging

# Interactive mode (get a shell in the container)
.\docker-build-windows.ps1 -Interactive
```

### Method 2: Using Docker Compose

```powershell
# Build the image
docker-compose -f docker-compose.windows.yml build

# Run the build
docker-compose -f docker-compose.windows.yml run --rm apollo-windows-build

# Interactive shell
docker-compose -f docker-compose.windows.yml run --rm apollo-windows-build powershell
```

### Method 3: Using Docker Directly

```powershell
# Build the image
docker build -f docker/Dockerfile.windows -t apollo-windows-builder .

# Run the build
docker run --rm `
    -v "${PWD}:C:\workspace" `
    -w C:\workspace `
    apollo-windows-builder

# Interactive shell
docker run --rm -it `
    -v "${PWD}:C:\workspace" `
    -w C:\workspace `
    apollo-windows-builder `
    powershell
```

## Build Output

After a successful build, you'll find:

- **Executable**: `build\sunshine.exe`
- **ZIP Package**: `build\cpack_artifacts\Apollo-*.zip`
- **NSIS Installer**: `build\cpack_artifacts\Apollo-*.exe`

## Troubleshooting

### Docker is in Linux Container Mode

**Error**: `The container operating system does not match the host operating system`

**Solution**: Switch to Windows containers (see Setup step 1)

### MSYS2 Installation Fails

**Error**: Chocolatey or MSYS2 installation fails

**Solution**: 
- Ensure you have internet connectivity
- Try rebuilding the image: `docker build --no-cache -f docker/Dockerfile.windows -t apollo-windows-builder .`

### Path Issues

**Error**: Build script can't find files

**Solution**: Ensure you're running from the apollo project root directory

### Build Fails with Permission Errors

**Error**: Access denied errors

**Solution**: 
- Run PowerShell as Administrator
- Check that Docker Desktop has proper permissions
- Ensure the workspace directory is accessible

### Container Runs but Build Fails

**Error**: CMake or ninja errors

**Solution**:
- Check that all submodules are initialized: `git submodule update --init --recursive`
- Try an interactive container to debug: `.\docker-build-windows.ps1 -Interactive`

## What's Inside the Container

The Windows container includes:

- **Base**: Windows Server Core LTSC 2022
- **Package Manager**: Chocolatey
- **Build System**: MSYS2 with MinGW-w64 UCRT toolchain
- **Dependencies**: All required MinGW packages (Boost, OpenSSL, NSIS, etc.)
- **Build Tools**: CMake, Ninja, Node.js

## Comparison with Native Build

| Aspect | Docker Container | Native MSYS2 |
|--------|------------------|--------------|
| Setup Time | One-time image build | Manual dependency installation |
| Isolation | Clean environment | System-wide installation |
| Reproducibility | High (same environment) | Depends on system state |
| Disk Space | ~10-15 GB (image) | ~5-10 GB (MSYS2) |
| Build Speed | Slightly slower (container overhead) | Faster (native) |

## Advanced Usage

### Custom Build Configuration

Edit `docker/windows-build.ps1` inside the container or pass environment variables:

```powershell
docker run --rm `
    -v "${PWD}:C:\workspace" `
    -w C:\workspace `
    -e CMAKE_BUILD_TYPE=Debug `
    -e BUILD_TESTS=ON `
    apollo-windows-builder
```

### Building Only Specific Components

The build script supports skipping steps:

```powershell
# Only configure, don't build
.\docker-build-windows.ps1 -SkipBuild -SkipPackage

# Only package existing build
.\docker-build-windows.ps1 -SkipConfigure -SkipBuild
```

## See Also

- [BUILD.md](../BUILD.md) - General build instructions
- [docs/building.md](../docs/building.md) - Comprehensive build guide
- [.github/workflows/windows-build.yml](../.github/workflows/windows-build.yml) - CI/CD workflow reference

