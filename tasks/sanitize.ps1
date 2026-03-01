#Requires -Version 5.1
# Build and test with AddressSanitizer.
# NOTE: MSVC ASan support is limited compared to Clang. UBSan not available on MSVC.
# Requires: MSVC with ASan support (VS 2019 16.9+) or clang-cl
$ErrorActionPreference = 'Stop'

$preset = if ($env:SANITIZE_PRESET) { $env:SANITIZE_PRESET } else { 'ci-sanitize' }
$buildDir = "build/$preset"

if (-not (Get-Command 'cl' -ErrorAction SilentlyContinue)) {
    Write-Error "cl.exe not found. Run from Developer PowerShell for VS."
    exit 1
}

Write-Host "Configuring ($preset)"
cmake --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Building"
cmake --build $buildDir -j
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running tests with sanitizers"
ctest --test-dir $buildDir --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
