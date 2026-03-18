@echo off
setlocal
pushd %~dp0

set "VBCABLE_URL=https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip"
set "VBCABLE_SHA256=B950E39F01AF1D04EA623C8F6D8EB9B6EA5C477C637295FABF20631C85116BFB"
set "WORK_DIR=%TEMP%\apollo-vbcable"
set "ARCHIVE=%WORK_DIR%\VBCABLE_Driver_Pack45.zip"
set "EXTRACT_DIR=%WORK_DIR%\package"
set "SETUP_NAME=VBCABLE_Setup_x64.exe"

if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
  echo VB-CABLE automatic installation is not configured for ARM64 yet.
  echo Install VB-CABLE manually from https://vb-audio.com/Cable/
  popd
  exit /b 1
)

echo ============================================================
echo Apollo microphone passthrough for Windows uses VB-CABLE.
echo Origin: https://www.vb-cable.com/
echo VB-CABLE is a donationware, all participations are welcome.
echo ============================================================

mkdir "%WORK_DIR%" 2>nul
if exist "%EXTRACT_DIR%" rmdir /s /q "%EXTRACT_DIR%"

echo Downloading VB-CABLE package from %VBCABLE_URL%...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ProgressPreference='SilentlyContinue';" ^
  "Invoke-WebRequest -Uri '%VBCABLE_URL%' -OutFile '%ARCHIVE%';" ^
  "$hash=(Get-FileHash -Algorithm SHA256 '%ARCHIVE%').Hash.ToUpperInvariant();" ^
  "if ($hash -ne '%VBCABLE_SHA256%') { throw 'VB-CABLE package hash mismatch.' };" ^
  "Expand-Archive -Path '%ARCHIVE%' -DestinationPath '%EXTRACT_DIR%' -Force"
if errorlevel 1 (
  echo Failed to download or verify the official VB-CABLE package.
  popd
  exit /b 1
)

if not exist "%EXTRACT_DIR%\%SETUP_NAME%" (
  echo Couldn't find %SETUP_NAME% in the downloaded VB-CABLE package.
  popd
  exit /b 1
)

echo Launching the official VB-CABLE setup program...
echo If prompted by VB-Audio, allow the driver install to finish completely.
start /wait "" "%EXTRACT_DIR%\%SETUP_NAME%"
if errorlevel 1 (
  echo VB-CABLE installation failed.
  popd
  exit /b 1
)

echo VB-CABLE installation completed. A reboot may be required before "CABLE Input" and "CABLE Output" appear.
popd
exit /b 0
