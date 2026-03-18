@echo off
setlocal
pushd %~dp0

echo Removing VB-CABLE driver package used by Apollo microphone passthrough...

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$drivers = pnputil /enum-drivers;" ^
  "$published = @();" ^
  "$current = @{};" ^
  "foreach ($line in $drivers) {" ^
  "  if ($line -match '^[ ]*Published Name[ ]*:[ ]*(.+)$') {" ^
  "    if ($current.ContainsKey('PublishedName') -and $current.ContainsKey('OriginalName') -and $current['OriginalName'] -like 'vbMmeCable*') { $published += $current['PublishedName'] };" ^
  "    $current = @{ PublishedName = $matches[1].Trim() };" ^
  "  } elseif ($line -match '^[ ]*Original Name[ ]*:[ ]*(.+)$') {" ^
  "    $current['OriginalName'] = $matches[1].Trim();" ^
  "  }" ^
  "}" ^
  "if ($current.ContainsKey('PublishedName') -and $current.ContainsKey('OriginalName') -and $current['OriginalName'] -like 'vbMmeCable*') { $published += $current['PublishedName'] };" ^
  "if ($published.Count -eq 0) { exit 0 };" ^
  "$exitCode = 0;" ^
  "foreach ($name in $published | Select-Object -Unique) {" ^
  "  pnputil /delete-driver $name /uninstall /force | Out-Host;" ^
  "  if ($LASTEXITCODE -ne 0) { $exitCode = $LASTEXITCODE }" ^
  "}" ^
  "exit $exitCode"
if errorlevel 1 (
  echo VB-CABLE uninstall reported an error.
  popd
  exit /b 1
)

echo VB-CABLE uninstall completed. A reboot may be required to fully remove the device.
popd
exit /b 0
