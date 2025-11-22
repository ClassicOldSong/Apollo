# Azure Pipelines CMake Fix

## Problem
The error `cmake: command not found` occurs because CMake is not installed in the MSYS2 environment before the build step.

## Solution
Add a step to install CMake (and other dependencies) using `pacman` in MSYS2 before running the cmake command.

## Example Azure Pipelines YAML Fix

Add this step **before** your cmake command:

```yaml
- task: Bash@3
  displayName: 'Install CMake and dependencies'
  inputs:
    targetType: 'inline'
    script: |
      pacman -Syu --noconfirm
      pacman -S --noconfirm \
        git \
        mingw-w64-x86_64-cmake \
        mingw-w64-x86_64-ninja \
        mingw-w64-x86_64-toolchain \
        mingw-w64-x86_64-boost \
        mingw-w64-x86_64-cppwinrt \
        mingw-w64-x86_64-curl-winssl \
        mingw-w64-x86_64-MinHook \
        mingw-w64-x86_64-miniupnpc \
        mingw-w64-x86_64-nodejs \
        mingw-w64-x86_64-nsis \
        mingw-w64-x86_64-onevpl \
        mingw-w64-x86_64-openssl \
        mingw-w64-x86_64-opus \
        mingw-w64-x86_64-nlohmann_json
```

Or if you're using the `msys2/setup-msys2` action (if available in Azure Pipelines), use:

```yaml
- task: UseMSYS2@0  # or equivalent
  inputs:
    msystem: 'MINGW64'
    update: true
    install: >-
      git
      mingw-w64-x86_64-cmake
      mingw-w64-x86_64-ninja
      mingw-w64-x86_64-toolchain
      mingw-w64-x86_64-boost
      mingw-w64-x86_64-cppwinrt
      mingw-w64-x86_64-curl-winssl
      mingw-w64-x86_64-MinHook
      mingw-w64-x86_64-miniupnpc
      mingw-w64-x86_64-nodejs
      mingw-w64-x86_64-nsis
      mingw-w64-x86_64-onevpl
      mingw-w64-x86_64-openssl
      mingw-w64-x86_64-opus
      mingw-w64-x86_64-nlohmann_json
```

## Minimal Fix (CMake only)

If you only need to fix the immediate CMake error:

```yaml
- task: Bash@3
  displayName: 'Install CMake'
  inputs:
    targetType: 'inline'
    script: |
      pacman -S --noconfirm mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

## Note
- For `MINGW64` environment, use `mingw-w64-x86_64-*` packages
- For `UCRT64` environment, use `mingw-w64-ucrt-x86_64-*` packages
- The `--noconfirm` flag prevents interactive prompts during installation
