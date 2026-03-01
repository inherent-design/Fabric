# Env: CODEQL_LANG (default: cpp), CODEQL_SUITE (default: code-scanning)
$ErrorActionPreference = 'Stop'

$lang = if ($env:CODEQL_LANG) { $env:CODEQL_LANG } else { 'cpp' }
$suite = if ($env:CODEQL_SUITE) { $env:CODEQL_SUITE } else { 'code-scanning' }
$buildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { 'build' }
$dbDir = 'build/codeql-db'

if (-not (Get-Command 'codeql' -ErrorAction SilentlyContinue)) {
    Write-Error "codeql not found. Run: mise install"
    exit 1
}

Write-Host "CodeQL $(codeql --version | Select-Object -First 1)"

# Clean previous database
if (Test-Path $dbDir) {
    Write-Host "Removing previous database"
    Remove-Item -Recurse -Force $dbDir
}

# Create database
$compiledLangs = @('cpp', 'java', 'csharp', 'go', 'swift')
Write-Host "Creating CodeQL database ($lang)"
if ($lang -in $compiledLangs) {
    codeql database create $dbDir `
        --language=$lang `
        --command="cmake --build $buildDir --clean-first -j" `
        --overwrite
} else {
    codeql database create $dbDir `
        --language=$lang `
        --overwrite
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Run analysis
$resultsFile = 'build/codeql-results.sarif'
Write-Host "Analyzing with suite: $suite"
codeql database analyze $dbDir `
    --format=sarifv2.1.0 `
    --output=$resultsFile `
    "$lang-$suite.qls"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`nResults: $resultsFile"

# Count findings from SARIF (PowerShell has native JSON support)
try {
    $sarif = Get-Content $resultsFile -Raw | ConvertFrom-Json
    $findings = @($sarif.runs | ForEach-Object { $_.results } | ForEach-Object { $_ })
    Write-Host "Findings: $($findings.Count)"
    if ($findings.Count -gt 0) {
        Write-Host "`nTop findings:"
        $findings | Select-Object -First 20 | ForEach-Object {
            $level = if ($_.level) { $_.level } else { 'warning' }
            Write-Host "  ${level}: $($_.message.text) [$($_.ruleId)]"
        }
    }
} catch {
    Write-Host "(Could not parse SARIF for summary)"
}
