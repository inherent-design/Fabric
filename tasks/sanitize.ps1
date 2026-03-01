# MSVC ASan only — no UBSan. Use clang-cl for full sanitizer coverage.
# Env: SANITIZE_PRESET (default: ci-sanitize)
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
