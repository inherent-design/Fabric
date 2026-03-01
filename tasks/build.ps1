#Requires -Version 5.1
# Configure and build Fabric Engine using CMakePresets.
# Env: BUILD_PRESET - cmake preset name (default: dev-debug)
# See CMakePresets.json for available presets.
$ErrorActionPreference = 'Stop'

$preset = if ($env:BUILD_PRESET) { $env:BUILD_PRESET } else { 'dev-debug' }
$buildDir = "build/$preset"

if (-not (Test-Path "$buildDir/build.ninja")) {
    Write-Host "Configuring ($preset)"
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Building ($preset)"
cmake --build $buildDir -j
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
