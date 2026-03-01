#Requires -Version 5.1
# Run cppcheck static analysis on Fabric source files.
# Suppressions: .cppcheck-suppressions (shared with CI)
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot

if (-not (Get-Command 'cppcheck' -ErrorAction SilentlyContinue)) {
    Write-Error "cppcheck not found. Install via scoop/choco or add to PATH."
    exit 1
}

Write-Host "Running cppcheck ($(cppcheck --version))"
cppcheck `
    --enable=warning,performance,portability `
    --error-exitcode=1 `
    --suppressions-list="$projectRoot/.cppcheck-suppressions" `
    -I include/ `
    src/ include/
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
