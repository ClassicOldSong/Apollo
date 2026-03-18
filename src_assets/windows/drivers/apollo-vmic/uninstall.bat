@echo off
setlocal
pushd %~dp0

set "CTL=%~dp0..\..\tools\apollovmicctl.exe"
if not exist "%CTL%" (
  echo Apollo virtual microphone installer helper not found: %CTL%
  popd
  exit /b 1
)

echo Removing Apollo Virtual Microphone driver...
"%CTL%" uninstall

popd
exit /b %errorlevel%
