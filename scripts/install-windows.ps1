# Run from the extracted release folder in an Administrator PowerShell.
$ErrorActionPreference = 'Stop'
if (Get-Process obs64 -ErrorAction SilentlyContinue) {
    throw 'Close OBS before installing the plugin.'
}
$source = Join-Path $PSScriptRoot 'recording-folder-dock'
if (!(Test-Path -LiteralPath (Join-Path $source 'bin\64bit\recording-folder-dock.dll'))) {
    throw 'Extract the release ZIP first, then run this script from its extracted folder.'
}
$destination = Join-Path $env:ProgramData 'obs-studio\plugins\recording-folder-dock'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -Path "$source\*" -Destination $destination -Recurse -Force
Write-Host 'Installed. Open OBS, then choose Docks > Recording Folder.'
