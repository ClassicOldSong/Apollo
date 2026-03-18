@echo off
setlocal
pushd %~dp0

if not exist "VirtualAudioDriver.inf" (
  echo Apollo Virtual Microphone driver package is not bundled in this build yet.
  popd
  exit /b 0
)

set "CTL=%~dp0..\..\tools\apollovmicctl.exe"
if not exist "%CTL%" (
  echo Apollo virtual microphone installer helper not found: %CTL%
  popd
  exit /b 1
)

echo Installing Apollo Virtual Microphone driver...
"%CTL%" install "%~dp0VirtualAudioDriver.inf"

popd
exit /b %errorlevel%
