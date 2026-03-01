#Requires -Version 5.1
# Remove build artifacts.
$ErrorActionPreference = 'Stop'

$buildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { 'build' }

if (Test-Path $buildDir) {
    Write-Host "Removing $buildDir/"
    Remove-Item -Recurse -Force $buildDir
} else {
    Write-Host "Nothing to clean"
}
