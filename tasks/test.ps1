# Env: TEST_SUITE (default: unit) — unit, e2e, all
# Env: TEST_FILTER — gtest --gtest_filter value
# Env: TEST_TIMEOUT (default: 120)
$ErrorActionPreference = 'Stop'

$buildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { 'build/dev-debug' }
$suite = if ($env:TEST_SUITE) { $env:TEST_SUITE } else { 'unit' }
$timeoutSec = if ($env:TEST_TIMEOUT) { [int]$env:TEST_TIMEOUT } else { 120 }

switch ($suite) {
    'unit' {
        $bin = "$buildDir/bin/UnitTests.exe"
        $runInBuildDir = $false
    }
    'e2e' {
        $bin = "$buildDir/bin/E2ETests.exe"
        $runInBuildDir = $true
    }
    'all' {
        foreach ($s in @('unit', 'e2e')) {
            $env:TEST_SUITE = $s
            & $PSCommandPath
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
        exit 0
    }
    default {
        Write-Error "Unknown suite: $suite. Valid: unit, e2e, all"
        exit 1
    }
}

if (-not (Test-Path $bin)) {
    Write-Error "Binary not found: $bin`nRun 'mise run build' first"
    exit 1
}

$filterFlag = @()
if ($env:TEST_FILTER) {
    $filterFlag = @("--gtest_filter=$($env:TEST_FILTER)")
}

Write-Host "Running $suite tests (timeout: ${timeoutSec}s)"

$oldDir = $null
if ($runInBuildDir) {
    $oldDir = Get-Location
    Set-Location $buildDir
    $bin = "bin/E2ETests.exe"
}

$proc = Start-Process -FilePath $bin -ArgumentList $filterFlag -NoNewWindow -PassThru
if (-not $proc.WaitForExit($timeoutSec * 1000)) {
    $proc.Kill()
    Write-Error "Test timed out after ${timeoutSec}s"
    exit 1
}

if ($oldDir) { Set-Location $oldDir }
exit $proc.ExitCode
