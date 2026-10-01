param([switch]$Test)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    $cmake = $cmake.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 C++ Build Tools and CMake.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (!(Test-Path -LiteralPath $cmake)) { throw 'Install the C++ CMake tools component in Visual Studio.' }
}
# Some launchers expose both Path and PATH; MSBuild rejects that environment.
$buildSearchPath = $env:Path
Remove-Item Env:PATH -ErrorAction SilentlyContinue
Remove-Item Env:Path -ErrorAction SilentlyContinue
$env:Path = $buildSearchPath
Push-Location $projectRoot
try {
    $version = (Get-Content buildspec.json -Raw | ConvertFrom-Json).version
    $buildDir = "build_x64_v$version"
    $package = Join-Path $projectRoot "dist\recording-folder-dock-$version-windows-x64"
    if (Test-Path -LiteralPath "$package.zip") { throw "Package already exists: $package.zip. Bump the version to create a new release." }
    $testing = if ($Test) { 'ON' } else { 'OFF' }
    & $cmake --preset windows-x64 -B $buildDir "-DBUILD_TESTING=$testing"
    if ($LASTEXITCODE) { throw 'Configuration failed.' }
    & $cmake --build $buildDir --config RelWithDebInfo
    if ($LASTEXITCODE) { throw 'Build failed.' }
    if ($Test) {
        $obsBin = Join-Path $projectRoot '.deps\obs-studio-31.1.1\build_x64\rundir\Release\bin\64bit'
        $qtRoot = Join-Path $projectRoot '.deps\obs-deps-qt6-2025-07-11-x64'
        $depsBin = Join-Path $projectRoot '.deps\obs-deps-2025-07-11-x64\bin'
        $env:Path = "$obsBin;$qtRoot\bin;$depsBin;$env:Path"
        $previousQtPlugins = $env:QT_PLUGIN_PATH
        $env:QT_PLUGIN_PATH = Join-Path $qtRoot 'plugins'
        try {
            & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir $buildDir -C RelWithDebInfo --output-on-failure
            if ($LASTEXITCODE) { throw 'Tests failed.' }
        } finally {
            $env:QT_PLUGIN_PATH = $previousQtPlugins
        }
    }
    & $cmake --install $buildDir --config RelWithDebInfo --prefix $package
    if ($LASTEXITCODE) { throw 'Packaging failed.' }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'install-windows.ps1') -Destination $package
    Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md'), (Join-Path $projectRoot 'TESTING.md'), (Join-Path $projectRoot 'LICENSE') -Destination $package
    Compress-Archive -Path "$package\*" -DestinationPath "$package.zip"
    Write-Host "Package ready: $package.zip"
} finally {
    $env:Path = $buildSearchPath
    Pop-Location
}
