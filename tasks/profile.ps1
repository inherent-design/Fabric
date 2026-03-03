# Build Fabric with Tracy profiler instrumentation.
# Env: PROFILE_MODE - "debug" (default) or "release" (RelWithDebInfo + optimizations)
$ErrorActionPreference = "Stop"

$mode = if ($env:PROFILE_MODE) { $env:PROFILE_MODE } else { "debug" }

switch ($mode) {
    "debug" {
        $preset = "dev-profile-debug"
    }
    "release" {
        $preset = "dev-profile-release"
    }
    default {
        Write-Error "Unknown PROFILE_MODE: $mode (expected: debug, release)"
        exit 1
    }
}

$buildDir = "build\$preset"

if (-not (Test-Path "$buildDir\build.ninja")) {
    Write-Host "Configuring ($preset)"
    cmake --preset $preset
}

Write-Host "Building ($preset)"
cmake --build $buildDir -j

Write-Host ""
Write-Host "Profile build ready: $buildDir\bin\Recurse.exe"
Write-Host "Run with: mise run profile:capture"
