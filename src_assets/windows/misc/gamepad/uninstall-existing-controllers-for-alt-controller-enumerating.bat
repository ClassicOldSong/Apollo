@echo off
cls
pushd "%~dp0" 2> null
pushd "\\%~p0" 2> null
echo Purpose: This script will clear out the gamepad controllers so that the alternate controller enumerating can be more effective and be less biased by previous controllers.
echo The will get rid of the unconnected devices for:
echo "Xbox 360 Controller for Windows"
echo "HID-compliant game controller"
echo "USB Input Device"
echo .....
echo This should can be run once or as part of the commands when an application in Apollo is run.
echo 1. Make a Windows Restore Point if concerned about device removal. (When running for the first time)
echo 2. Be sure to run this as Administrator
echo 3. Run this when Apollo is not running. (Unless part of the sequence before the main Apollo application starts.)
echo 4. Run this when there are no controllers connected.
echo 5. Make sure the number of alternate controllers that you is filled in Apollo.
echo 6. Once clients start joining with controllers, the controller enumeration should be better. This should also get past the 5th+ controller odd numbering situation as long as it is run every time the Apollo application is started.
echo ....
echo Press CTRL-C to stop script
rem pause
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -listGhostDevicesOnly
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "Xbox 360 Controller for Windows" -listGhostDevicesOnly
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "HID-compliant game controller" -listGhostDevicesOnly
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "USB Input Device" -listGhostDevicesOnly
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "Xbox 360 Controller for Windows" -force
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "HID-compliant game controller" -force
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "USB Input Device" -force
echo ....
echo Done
rem pause
exit 0