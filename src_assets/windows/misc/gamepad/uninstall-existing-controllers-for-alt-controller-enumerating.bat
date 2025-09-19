@echo off
cls
echo Purpose: This script will clear out the gamepad controllers so that the alternate controller enumerating can be more effective and be less biased by previous controllers.
echo The will get rid of the unconnected devices for:
echo "Xbox 360 Controller for Windows"
echo "HID-compliant game controller"
echo .....
echo This should only be run sparingly or when needed when there are unused controller altering the Windows controller enumeration.
echo 1. Make a Windows Restore Point if concerned about device removal.
echo 2. Be sure to run this as Administrator
echo 3. Run this when Apollo is not running.
echo 4. Run this when there are no controllers connected.
echo 5. This script will prompt to hit any key to run or CTRL-C to stop.
echo 6. After it is completed, the system must be restarted in order to clear out the older devices.
echo 7. When the system is rebooted, set the number of preallocated controller to the number that you want in Apollo.
echo 8. Once clients start joining with controllers, the controller enumeration should be better. This may also get past the 5th+ controller odd numbering situation at least initially.
echo ....
echo Press CTRL-C to stop script or
pause
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -listGhostDevicesOnly
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "Xbox 360 Controller for Windows" -listGhostDevicesOnly
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "HID-compliant game controller" -listGhostDevicesOnly
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "Xbox 360 Controller for Windows" -force
powershell.exe -ExecutionPolicy Bypass -File ".\removeGhosts.ps1" -narrowbyfriendlyname "HID-compliant game controller" -force
echo ....
echo Done
echo 6. After it is completed, the system must be restarted in order to clear out the older devices.
echo 7. When the system is rebooted, set the number of preallocated controller to the number that you want in Apollo.
echo 8. Once clients start joining with controllers, the controller enumeration should be better. This may also get past the 5th+ controller odd numbering situation at least initially.
