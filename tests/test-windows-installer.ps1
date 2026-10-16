# Exercise the packaged installer in an isolated workspace directory.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$version = (Get-Content (Join-Path $projectRoot 'buildspec.json') -Raw | ConvertFrom-Json).version
$package = Join-Path $projectRoot "dist\recording-folder-dock-$version-windows-x64.zip"
$fixture = Join-Path $projectRoot "dist\installer-test-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $fixture | Out-Null
$resolvedRoot = (Resolve-Path -LiteralPath $projectRoot).Path
$resolvedFixture = (Resolve-Path -LiteralPath $fixture).Path
if (!$resolvedFixture.StartsWith($resolvedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Installer test must stay inside the workspace.'
}
$extracted = Join-Path $fixture 'package'
Expand-Archive -LiteralPath $package -DestinationPath $extracted
$installer = Join-Path $extracted 'install-windows.ps1'
$sourceDll = Join-Path $extracted 'recording-folder-dock\bin\64bit\recording-folder-dock.dll'
$testProgramData = Join-Path $fixture 'ProgramData'
$originalProgramData = $env:ProgramData
function Invoke-TestInstaller([bool]$simulateRunning) {
    & {
        param($file, $running)
        function Get-Process {
            [CmdletBinding()]
            param([string]$Name)
            if ($Name -eq 'obs64' -and $running) { [pscustomobject]@{ ProcessName = 'obs64' } }
        }
        & $file
    } $installer $simulateRunning
}
try {
    $env:ProgramData = $testProgramData
    $blocked = $false
    try { Invoke-TestInstaller $true } catch {
        if ($_.Exception.Message -notmatch 'Close OBS') { throw }
        $blocked = $true
    }
    if (!$blocked -or (Test-Path -LiteralPath $testProgramData)) { throw 'Running-OBS guard failed.' }
    Invoke-TestInstaller $false
    $installedDll = Join-Path $testProgramData 'obs-studio\plugins\recording-folder-dock\bin\64bit\recording-folder-dock.dll'
    $expected = (Get-FileHash -LiteralPath $sourceDll -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash -ne $expected) {
        throw 'Installed DLL does not match the package.'
    }
    Move-Item -LiteralPath $sourceDll -Destination ($sourceDll + '.missing')
    $blocked = $false
    try { Invoke-TestInstaller $false } catch {
        if ($_.Exception.Message -notmatch 'Extract the release ZIP') { throw }
        $blocked = $true
    }
    if (!$blocked) { throw 'Missing-package guard failed.' }
    if ((Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash -ne $expected) {
        throw 'Rejected install changed the installed DLL.'
    }
    Write-Output 'PASS: installer copy, running-OBS guard, and missing-package guard.'
} finally {
    $env:ProgramData = $originalProgramData
}
