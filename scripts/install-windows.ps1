# For installed OBS, run from an Administrator PowerShell.
# For portable OBS: .\install-windows.ps1 -Portable -ObsPath 'D:\OBS'
[CmdletBinding()]
param(
    [string]$ObsPath = (Join-Path $env:ProgramFiles 'obs-studio'),
    [switch]$Portable
)
$ErrorActionPreference = 'Stop'
if (Get-Process obs64 -ErrorAction SilentlyContinue) {
    throw 'Close OBS before installing the plugin.'
}
if ($Portable -and !$PSBoundParameters.ContainsKey('ObsPath')) {
    throw 'Portable installs require an explicit -ObsPath.'
}
$name = 'recording-folder-dock'
$source = Join-Path $PSScriptRoot $name
$sourceDll = Join-Path $source "$name.dll"
if (!(Test-Path -LiteralPath $sourceDll -PathType Leaf) -or
    !(Test-Path -LiteralPath (Join-Path $source 'data\locale\en-US.ini') -PathType Leaf)) {
    throw 'Extract the release ZIP first, then run this script from its extracted folder.'
}
$ObsPath = [IO.Path]::GetFullPath($ObsPath)
$obsExe = Join-Path $ObsPath 'bin\64bit\obs64.exe'
if (!(Test-Path -LiteralPath $obsExe -PathType Leaf)) {
    throw 'OBS was not found. Supply its installation root with -ObsPath.'
}
if ((Get-Item -LiteralPath $obsExe).VersionInfo.FileMajorPart -lt 33) {
    throw 'This package requires OBS 33 or newer. Use plugin v1.4.0 for OBS 31.1-32.'
}
$base = if ($Portable) { $ObsPath } else { Join-Path $env:ProgramData 'obs-studio' }
$destination = Join-Path $base "plugins\$name"
# Only this plugin's known legacy files are removed. Backups stay outside plugin discovery.
$legacy = @(
    (Join-Path $destination "bin\64bit\$name.dll"),
    (Join-Path $destination "bin\64bit\$name.pdb"),
    (Join-Path $ObsPath "obs-plugins\64bit\$name.dll"),
    (Join-Path $ObsPath "obs-plugins\64bit\$name.pdb"),
    (Join-Path $ObsPath "data\obs-plugins\$name")
)
function Assert-NoLinks([string]$path) {
    $cursor = [IO.Path]::GetFullPath($path)
    while ($cursor) {
        if ((Test-Path -LiteralPath $cursor) -and
            ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to modify a linked path: $cursor"
        }
        $parent = Split-Path -Path $cursor -Parent
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
}
function Assert-TreeNoLinks([string]$path) {
    Assert-NoLinks $path
    if ((Test-Path -LiteralPath $path -PathType Container) -and
        (Get-ChildItem -LiteralPath $path -Recurse -Force |
            Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint })) {
        throw "Refusing a path containing links: $path"
    }
}
Assert-TreeNoLinks $destination
foreach ($path in $legacy) { Assert-TreeNoLinks $path }
$backup = Join-Path $base "plugin-backups\$name\$([guid]::NewGuid().ToString('N'))"
Assert-NoLinks $backup
New-Item -ItemType Directory -Path $backup -Force | Out-Null
if (Test-Path -LiteralPath $destination) {
    Copy-Item -LiteralPath $destination -Destination (Join-Path $backup 'previous-install') -Recurse
}
for ($i = 0; $i -lt $legacy.Count; $i++) {
    if (Test-Path -LiteralPath $legacy[$i]) {
        Copy-Item -LiteralPath $legacy[$i] -Destination (Join-Path $backup "legacy-$i") -Recurse
    }
}
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $sourceDll -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $source 'data') -Destination $destination -Recurse -Force
if ((Get-FileHash -LiteralPath (Join-Path $destination "$name.dll")).Hash -ne
    (Get-FileHash -LiteralPath $sourceDll).Hash) { throw 'Installed DLL verification failed; legacy files were retained.' }
foreach ($path in $legacy) {
    # Absolute, explicitly named targets checked above; never remove their parent directories.
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
}
Write-Host "Installed to $destination"
Write-Host "Previous files backed up to $backup"
Write-Host 'Open OBS, then choose Docks > Recording Folder.'
