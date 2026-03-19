# Launch Recurse directly into benchmark mode and capture a Tracy trace.
# Env: PROFILE_MODE    - "debug" (default) or "release"
#      CAPTURE_SECONDS  - seconds to capture (default: 30)
#      CAPTURE_PORT     - Tracy listen port (default: 8086)
$ErrorActionPreference = "Stop"

$mode = if ($env:PROFILE_MODE) { $env:PROFILE_MODE } else { "debug" }
$seconds = if ($env:CAPTURE_SECONDS) { $env:CAPTURE_SECONDS } else { "30" }
$port = if ($env:CAPTURE_PORT) { $env:CAPTURE_PORT } else { "8086" }

switch ($mode) {
    "debug"   { $preset = "dev-profile-debug" }
    "release" { $preset = "dev-profile-release" }
    default {
        Write-Error "Unknown PROFILE_MODE: $mode"
        exit 1
    }
}

$bin = "build\$preset\bin\Recurse.exe"

if (-not (Test-Path $bin)) {
    Write-Error "Binary not found: $bin. Run 'mise run profile' first."
    exit 1
}

$tracyCapture = Get-Command tracy-capture -ErrorAction SilentlyContinue
if (-not $tracyCapture) {
    Write-Error "tracy-capture not found. Install Tracy profiler tools."
    exit 1
}

New-Item -ItemType Directory -Force -Path tracy | Out-Null
$timestamp = Get-Date -Format "yyyy-MM-dd-HHmm"
$outfile = "tracy\$timestamp.tracy"

Write-Host "Starting Recurse ($preset) in benchmark mode..."
$proc = Start-Process -FilePath $bin -ArgumentList "--benchmark" -PassThru

Start-Sleep -Seconds 2

Write-Host "Capturing trace to $outfile (${seconds}s, port $port)..."
Write-Host "Press Ctrl+C to stop early."

& tracy-capture -o $outfile -s $seconds -p $port

Write-Host "Stopping Recurse..."
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

if (Test-Path $outfile) {
    $size = (Get-Item $outfile).Length / 1KB
    Write-Host "Trace saved: $outfile ($([math]::Round($size))KB)"
    Write-Host "View with:   tracy-profiler $outfile"
    Write-Host "Export CSV:  tracy-csvexport $outfile"
} else {
    Write-Error "No trace file produced."
    exit 1
}
