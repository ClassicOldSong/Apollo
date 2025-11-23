# Apollo Local Build Instructions

This document provides instructions for building Apollo locally on your development machine. This is useful for catching compilation errors before pushing to remote repositories.

## Quick Start

The simplest way to build Apollo is using the provided build script:

```bash
cd apollo
./build.sh
```

This will:
1. Check for required dependencies
2. Automatically use Docker if dependencies are missing (optional)
3. Configure CMake
4. Build Apollo in Release mode

## Build Methods

You can build Apollo in two ways:

1. **Native Build** - Build directly on your machine (requires all dependencies)
2. **Docker Build** - Build in a Docker container (only requires Docker)

The build script automatically detects missing dependencies and can fall back to Docker if available. This means you don't need to install all build dependencies locally!

## Prerequisites

### Required Tools

- **CMake** >= 3.25.0
- **Ninja** build system
- **Node.js** and **npm** (for web UI)
- **C++ Compiler**:
  - macOS: Clang 15+ or Apple Clang 15+
  - Linux: GCC 13+ or Clang 17+

### macOS Dependencies

Install using Homebrew:

```bash
brew install cmake ninja node openssl@3 opus miniupnpc
```

If you encounter SSL header issues:

```bash
# For Intel Macs
ln -s /usr/local/opt/openssl/include/openssl /usr/local/include/openssl

# For Apple Silicon Macs
ln -s /opt/homebrew/opt/openssl/include/openssl /opt/homebrew/include/openssl
```

### Linux Dependencies

#### Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  nodejs \
  npm \
  libssl-dev \
  libopus-dev \
  libminiupnpc-dev \
  libcurl4-openssl-dev \
  libevdev-dev \
  libdrm-dev \
  libcap-dev \
  libwayland-dev \
  libx11-dev \
  libxcb1-dev \
  libxfixes-dev \
  libxrandr-dev \
  libxtst-dev
```

#### Fedora

```bash
sudo dnf install -y \
  gcc \
  gcc-c++ \
  cmake \
  ninja-build \
  nodejs \
  npm \
  openssl-devel \
  opus-devel \
  miniupnpc-devel \
  libcurl-devel \
  libevdev-devel \
  libdrm-devel \
  libcap-devel \
  wayland-devel \
  libX11-devel \
  libxcb-devel \
  libXfixes-devel \
  libXrandr-devel \
  libXtst-devel
```

#### Optional: CUDA (Linux only)

If you want CUDA support for NVENC encoding:

```bash
# Install CUDA Toolkit 12.9+ from NVIDIA
# Then ensure nvcc is in your PATH
nvcc --version
```

### Windows Dependencies

For Windows builds, you need MSYS2 with MinGW-w64 UCRT toolchain. See [Windows Container Build Guide](docker/README.windows.md) for containerized builds, or follow the native build instructions in [docs/building.md](docs/building.md).

## Docker Build (Recommended for Development)

Docker builds are the easiest way to build Apollo without installing all dependencies locally. You only need Docker installed.

### Prerequisites for Docker Build

- **Docker** (Docker Desktop for macOS/Windows, or Docker Engine for Linux)
- **docker-compose** or **docker compose** (usually included with Docker)

### Quick Docker Build

```bash
cd apollo

# Force Docker build (recommended)
USE_DOCKER=yes ./build.sh

# Or let it auto-detect (uses Docker if deps are missing)
./build.sh
```

The first build will take longer as it downloads and builds the Docker image. Subsequent builds will be faster.

### Docker Build Options

```bash
# Debug build in Docker
USE_DOCKER=yes CMAKE_BUILD_TYPE=Debug ./build.sh

# Build with tests in Docker
USE_DOCKER=yes BUILD_TESTS=ON ./build.sh test

# Clean and rebuild in Docker
USE_DOCKER=yes ./build.sh clean && USE_DOCKER=yes ./build.sh
```

### Docker Image Details

The Docker image (`apollo-dev:latest`) includes:
- All build dependencies (CMake, Ninja, GCC, etc.)
- All development libraries
- Node.js and npm for web UI
- Pre-configured build environment

Build artifacts are written to your local `build/` directory, so you can access them directly on your host machine.

### Manual Docker Usage

#### Option 1: Using docker-build.sh (Simplest)

```bash
cd apollo

# Simple Docker build
./docker-build.sh

# With options
CMAKE_BUILD_TYPE=Debug ./docker-build.sh
BUILD_TESTS=ON ./docker-build.sh test
```

#### Option 2: Using docker-compose

```bash
cd apollo

# Build the image
docker-compose -f docker-compose.dev.yml build

# Run a build
docker-compose -f docker-compose.dev.yml run --rm apollo-build ./build.sh

# Get an interactive shell in the container
docker-compose -f docker-compose.dev.yml run --rm apollo-build bash
```

#### Option 3: Using Docker directly

```bash
cd apollo

# Build the image
docker build -f docker/Dockerfile.dev -t apollo-dev .

# Run build
docker run --rm -v "$PWD:/workspace" -w /workspace apollo-dev ./build.sh
```

## Native Build

If you prefer to build natively on your machine, you'll need to install all dependencies listed below.

## Build Script Usage

The `build.sh` script provides several commands:

### Basic Build

```bash
./build.sh              # Build in Release mode (default)
./build.sh build        # Same as above
```

### Configure Only

```bash
./build.sh configure    # Only run CMake configuration, don't build
```

### Clean Build

```bash
./build.sh clean        # Remove build directory
```

### Build with Tests

```bash
./build.sh test        # Build with tests enabled and run them
```

### Build Options

You can control the build using environment variables:

```bash
# Debug build
CMAKE_BUILD_TYPE=Debug ./build.sh

# Build with tests
BUILD_TESTS=ON ./build.sh

# Enable warnings as errors (stricter checking)
BUILD_WERROR=ON ./build.sh

# Force native build (fail if deps missing)
USE_DOCKER=no ./build.sh

# Force Docker build
USE_DOCKER=yes ./build.sh

# Combine options
CMAKE_BUILD_TYPE=Debug BUILD_TESTS=ON BUILD_WERROR=ON ./build.sh
```

## Manual Build (Without Script)

If you prefer to build manually:

### 1. Configure CMake

```bash
cd apollo
mkdir -p build
cd build

cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

### 2. Build

```bash
ninja
```

### 3. Run Tests (Optional)

```bash
# Configure with tests enabled
cmake .. -DBUILD_TESTS=ON

# Build
ninja

# Run tests
./tests/apollo_tests  # or sunshine_tests depending on build
```

## Build Types

- **Release** (default): Optimized build for production
- **Debug**: Includes debug symbols, no optimizations
- **RelWithDebInfo**: Optimized with debug symbols

## Common Issues

### CMake Version Too Old

If you get a CMake version error:

```bash
# macOS
brew upgrade cmake

# Linux - download from cmake.org or use package manager
```

### Missing Dependencies

The build script will check for dependencies and provide installation instructions if any are missing.

### Compilation Errors

1. Check that all dependencies are installed
2. Ensure you're using a supported compiler version
3. Try a clean build: `./build.sh clean && ./build.sh`
4. Check the error messages for specific missing headers or libraries

### CUDA Issues (Linux)

If CUDA is not found but you want to build without it:

```bash
# The build script will automatically disable CUDA if nvcc is not found
# Or manually disable:
cmake .. -DSUNSHINE_ENABLE_CUDA=OFF
```

## Development Workflow

### Using Docker (Recommended)

1. **Make code changes**
2. **Build in Docker**: `USE_DOCKER=yes ./build.sh`
3. **Fix any compilation errors**
4. **Run tests** (if applicable): `USE_DOCKER=yes BUILD_TESTS=ON ./build.sh test`
5. **Commit and push** once build succeeds

### Using Native Build

1. **Make code changes**
2. **Build locally**: `./build.sh` (or `USE_DOCKER=no ./build.sh`)
3. **Fix any compilation errors**
4. **Run tests** (if applicable): `BUILD_TESTS=ON ./build.sh test`
5. **Commit and push** once build succeeds

**Tip**: Docker builds are recommended because they:
- Don't require installing dependencies locally
- Provide consistent build environment across machines
- Are easier to set up for new developers

## Integration with IDEs

### VS Code / Cursor

The build script generates `compile_commands.json` which enables:
- IntelliSense/autocomplete
- Error highlighting
- Go to definition

After building, your IDE should automatically pick up the compile commands.

### CLion

CLion can import the CMake project directly:
1. Open CLion
2. File → Open → Select `apollo/CMakeLists.txt`
3. CLion will configure and build automatically

## Continuous Integration

For CI/CD pipelines, you can use the build script:

```bash
# In CI script
cd apollo
chmod +x build.sh
BUILD_WERROR=ON ./build.sh
```

## Windows Container Build

For Windows users, you can build Apollo using Docker Windows containers. This provides a clean, reproducible build environment without installing MSYS2 directly on your system.

### Quick Start

```powershell
# Build Windows executable and installer
.\docker-build-windows.ps1
```

### Prerequisites

- Windows 10/11 Pro, Enterprise, or Windows Server
- Docker Desktop configured for Windows containers
- See [Windows Container Build Guide](docker/README.windows.md) for detailed instructions

### Usage

```powershell
# Release build (default)
.\docker-build-windows.ps1

# Debug build
.\docker-build-windows.ps1 -BuildType Debug

# Interactive container shell
.\docker-build-windows.ps1 -Interactive
```

The build artifacts (`.exe` and installer) will be in `build\cpack_artifacts\`.

## Additional Resources

- [Full Building Documentation](docs/building.md) - Comprehensive build guide
- [Windows Container Build Guide](docker/README.windows.md) - Windows Docker build instructions
- [Contributing Guide](docs/contributing.md) - Development guidelines
- [Troubleshooting](docs/troubleshooting.md) - Common issues and solutions

