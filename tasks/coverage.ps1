# Requires: OpenCppCoverage (choco install opencppcoverage)
$ErrorActionPreference = 'Stop'

$buildDir = 'build/ci-coverage'

if (-not (Get-Command 'OpenCppCoverage' -ErrorAction SilentlyContinue)) {
    Write-Error "OpenCppCoverage not found. Install via: choco install opencppcoverage"
    exit 1
}

Write-Host "Configuring (ci-coverage)"
cmake --preset ci-coverage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Building"
cmake --build $buildDir -j
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running tests with coverage"
OpenCppCoverage `
    --sources "src" --sources "include" `
    --excluded_sources "build" --excluded_sources "_deps" --excluded_sources "tests" `
    --export_type "cobertura:$buildDir/coverage.xml" `
    -- "$buildDir/bin/UnitTests.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Coverage report: $buildDir/coverage.xml"
