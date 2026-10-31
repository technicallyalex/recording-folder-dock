# Test the actual release installer in a workspace fixture, never in the real OBS folders.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = (Get-Content (Join-Path $root 'buildspec.json') -Raw | ConvertFrom-Json).version
$fixture = Join-Path $root "dist\installer-test-$([guid]::NewGuid().ToString('N'))"
$extracted = Join-Path $fixture 'package'
Expand-Archive -LiteralPath (Join-Path $root "dist\recording-folder-dock-$version-windows-x64.zip") -DestinationPath $extracted
$installer = Join-Path $extracted 'install-windows.ps1'
$sourceDll = Join-Path $extracted 'recording-folder-dock\recording-folder-dock.dll'
$sourceLocale = Join-Path $extracted 'recording-folder-dock\data\locale\en-US.ini'
$expected = (Get-FileHash -LiteralPath $sourceDll).Hash
$originalProgramData = $env:ProgramData
function Check([bool]$condition, [string]$message) { if (!$condition) { throw $message } }
function Invoke-Installer([string]$obsRoot, [bool]$portable = $false, [bool]$running = $false, [int]$major = 33) {
    & {
        param($file, $obsRoot, $portable, $running, $major)
        function Get-Process {
            [CmdletBinding()] param([string]$Name)
            if ($Name -eq 'obs64' -and $running) { [pscustomobject]@{ ProcessName='obs64' } }
        }
        function Get-Item {
            [CmdletBinding()] param([string]$LiteralPath, [switch]$Force)
            if ($LiteralPath -eq (Join-Path $obsRoot 'bin\64bit\obs64.exe')) {
                [pscustomobject]@{ VersionInfo=[pscustomobject]@{ FileMajorPart=$major }; Attributes=[IO.FileAttributes]::Normal }
            } else { Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath -Force:$Force }
        }
        & $file -ObsPath $obsRoot -Portable:$portable
    } $installer $obsRoot $portable $running $major
}
function Expect-Rejection([scriptblock]$action, [string]$pattern) {
    $blocked=$false
    try { & $action } catch { if ($_.Exception.Message -notmatch $pattern) { throw }; $blocked=$true }
    Check $blocked "Expected rejection: $pattern"
}
try {
    $env:ProgramData = Join-Path $fixture 'ProgramData'
    $obsRoot = Join-Path $fixture 'OBS with spaces'
    New-Item -ItemType Directory -Path (Join-Path $obsRoot 'bin\64bit') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $obsRoot 'bin\64bit\obs64.exe') -Value 'fixture'
    Expect-Rejection { Invoke-Installer $obsRoot $false $true } 'Close OBS'
    Expect-Rejection { Invoke-Installer $obsRoot $false $false 32 } 'OBS 33'
    Expect-Rejection { Invoke-Installer (Join-Path $fixture 'missing') } 'OBS was not found'
    Check (!(Test-Path -LiteralPath $env:ProgramData)) 'Rejected install wrote to ProgramData'
    $destination = Join-Path $env:ProgramData 'obs-studio\plugins\recording-folder-dock'
    $oldDll = Join-Path $destination 'bin\64bit\recording-folder-dock.dll'
    $legacyDll = Join-Path $obsRoot 'obs-plugins\64bit\recording-folder-dock.dll'
    $otherDll = Join-Path $obsRoot 'obs-plugins\64bit\other-plugin.dll'
    $legacyData = Join-Path $obsRoot 'data\obs-plugins\recording-folder-dock'
    foreach ($path in @($oldDll,$legacyDll,$otherDll)) {
        New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
        Set-Content -LiteralPath $path -Value 'old plugin'
    }
    New-Item -ItemType Directory -Path $legacyData -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $legacyData 'old.txt') -Value 'old data'
    Invoke-Installer $obsRoot
    $installedDll=Join-Path $destination 'recording-folder-dock.dll'
    Check ((Get-FileHash -LiteralPath $installedDll).Hash -eq $expected) 'DLL copy mismatch'
    Check ((Get-FileHash -LiteralPath (Join-Path $destination 'data\locale\en-US.ini')).Hash -eq (Get-FileHash -LiteralPath $sourceLocale).Hash) 'Data copy mismatch'
    Check (!(Test-Path -LiteralPath $oldDll) -and !(Test-Path -LiteralPath $legacyDll) -and !(Test-Path -LiteralPath $legacyData)) 'Legacy files remain'
    Check (Test-Path -LiteralPath $otherDll) 'Installer touched another plugin'
    $backups=@(Get-ChildItem -LiteralPath (Join-Path $env:ProgramData 'obs-studio\plugin-backups\recording-folder-dock') -Directory)
    Check ($backups.Count -eq 1) 'Backup missing'
    Check (Test-Path -LiteralPath (Join-Path $backups[0].FullName 'legacy-2')) 'Legacy DLL was not backed up'
    Invoke-Installer $obsRoot
    Check ((Get-FileHash -LiteralPath $installedDll).Hash -eq $expected) 'Repeat install failed'
    Invoke-Installer $obsRoot $true
    Check ((Get-FileHash -LiteralPath (Join-Path $obsRoot 'plugins\recording-folder-dock\recording-folder-dock.dll')).Hash -eq $expected) 'Portable install failed'
    # A junction must not redirect migration into another tree.
    $linkedObs = Join-Path $fixture 'linked-OBS'
    New-Item -ItemType Directory -Path (Join-Path $linkedObs 'bin\64bit') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $linkedObs 'bin\64bit\obs64.exe') -Value 'fixture'
    $unrelated = Join-Path $fixture 'unrelated'
    New-Item -ItemType Directory -Path $unrelated -Force | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $linkedObs 'plugins') -Target $unrelated | Out-Null
    Expect-Rejection { Invoke-Installer $linkedObs $true } 'linked path'
    Check (@(Get-ChildItem -LiteralPath $unrelated).Count -eq 0) 'Linked path was modified'
    # Malformed packages must not alter an installed copy.
    Move-Item -LiteralPath $sourceDll -Destination ($sourceDll+'.missing')
    Expect-Rejection { Invoke-Installer $obsRoot } 'Extract the release ZIP'
    Check ((Get-FileHash -LiteralPath $installedDll).Hash -eq $expected) 'Rejected install changed existing DLL'
    Write-Output 'PASS: modern layout, data, migration backups, repeat install, portable install, unrelated plugin preservation, and rejection guards.'
} finally { $env:ProgramData=$originalProgramData }
