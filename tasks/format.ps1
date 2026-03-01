#Requires -Version 5.1
# Check or fix clang-format on Fabric source files.
# Env: FORMAT_FIX - set to "1" to apply fixes (default: check-only)
$ErrorActionPreference = 'Stop'

$fixMode = $env:FORMAT_FIX

if (-not (Get-Command 'clang-format' -ErrorAction SilentlyContinue)) {
    Write-Error "clang-format not found. Install LLVM or add to PATH."
    exit 1
}

$files = @()
$files += Get-ChildItem -Recurse -Include '*.cc','*.hh' -Path 'src','include' | Select-Object -ExpandProperty FullName

if ($files.Count -eq 0) {
    Write-Host "No source files found"
    exit 0
}

if ($fixMode -eq '1' -or $fixMode -eq 'true') {
    Write-Host "Formatting source files"
    foreach ($f in $files) {
        clang-format -i $f
    }
    Write-Host "Done"
} else {
    Write-Host "Checking format (dry-run)"
    foreach ($f in $files) {
        clang-format --dry-run --Werror $f
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    Write-Host "All files formatted correctly"
}
