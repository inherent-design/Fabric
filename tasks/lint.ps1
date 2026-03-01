#Requires -Version 5.1
# Run clang-tidy on Fabric source files.
# Env: LINT_FIX     - set to "1" to apply fixes (CAUTION: can break cross-file refs)
# Env: LINT_CHANGED - set to "1" to only lint git-dirty files (fast)
# Requires: compile_commands.json in build dir (cmake generates this)
$ErrorActionPreference = 'Stop'

$buildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { 'build/dev-debug' }

if (-not (Test-Path "$buildDir/compile_commands.json")) {
    Write-Host "No compile_commands.json found. Running build first."
    & "$PSScriptRoot/build.ps1"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$fixFlag = @()
if ($env:LINT_FIX -eq '1' -or $env:LINT_FIX -eq 'true') {
    $fixFlag = @('--fix')
}

# Collect files to lint
if ($env:LINT_CHANGED -eq '1' -or $env:LINT_CHANGED -eq 'true') {
    Write-Host "Linting changed files only"
    $unstaged = git diff --name-only --diff-filter=ACMR HEAD -- 'src/*.cc' 'src/*.hh' 'include/*.cc' 'include/*.hh' 2>$null
    $staged = git diff --cached --name-only --diff-filter=ACMR -- 'src/*.cc' 'src/*.hh' 'include/*.cc' 'include/*.hh' 2>$null
    $files = @($unstaged) + @($staged) | Sort-Object -Unique | Where-Object { $_ -and $_ -notmatch 'Constants\.g\.hh' }
    if ($files.Count -eq 0) {
        Write-Host "No changed source files to lint"
        exit 0
    }
} else {
    Write-Host "Linting all source files"
    $files = Get-ChildItem -Recurse -Include '*.cc','*.hh' -Path 'src','include' |
        Where-Object { $_.Name -ne 'Constants.g.hh' } |
        Select-Object -ExpandProperty FullName
}

foreach ($f in $files) {
    clang-tidy -p $buildDir @fixFlag --quiet $f 2>&1 | Where-Object { $_ -notmatch 'warnings generated' }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
