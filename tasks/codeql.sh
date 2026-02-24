#!/bin/sh
# Run CodeQL security analysis locally.
# Env: CODEQL_LANG  - language to analyze (default: cpp)
# Env: CODEQL_SUITE - query suite (default: code-scanning)
# Env: BUILD_DIR    - cmake build directory (default: build)
# Requires: codeql CLI (mise manages this)
set -eu

lang="${CODEQL_LANG:-cpp}"
suite="${CODEQL_SUITE:-code-scanning}"
build_dir="${BUILD_DIR:-build}"
db_dir="build/codeql-db"

if ! command -v codeql >/dev/null 2>&1; then
  echo "codeql not found. Run: mise install" >&2
  exit 1
fi

echo "CodeQL $(codeql --version | head -1)"

# Clean previous database
if [ -d "$db_dir" ]; then
  echo "Removing previous database"
  rm -rf "$db_dir"
fi

# Create database (for compiled languages, codeql needs to observe the build)
echo "Creating CodeQL database (${lang})"
if [ "$lang" = "cpp" ] || [ "$lang" = "java" ] || [ "$lang" = "csharp" ] || [ "$lang" = "go" ] || [ "$lang" = "swift" ]; then
  # Compiled language: codeql needs a build command
  # Ensure clean build so codeql sees all compilations
  codeql database create "$db_dir" \
    --language="$lang" \
    --command="cmake --build ${build_dir} --clean-first -j" \
    --overwrite
else
  # Interpreted language: no build command needed
  codeql database create "$db_dir" \
    --language="$lang" \
    --overwrite
fi

# Run analysis
echo "Analyzing with suite: ${suite}"
results_file="build/codeql-results.sarif"
codeql database analyze "$db_dir" \
  --format=sarifv2.1.0 \
  --output="$results_file" \
  "${lang}-${suite}.qls"

# Summary
echo ""
echo "Results: ${results_file}"

# Count findings from SARIF
if command -v jq >/dev/null 2>&1; then
  count=$(jq '[.runs[].results[]] | length' "$results_file" 2>/dev/null || echo "?")
  echo "Findings: ${count}"
  if [ "$count" != "0" ] && [ "$count" != "?" ]; then
    echo ""
    echo "Top findings:"
    jq -r '.runs[].results[] | "  \(.level // "warning"): \(.message.text) [\(.ruleId)]"' \
      "$results_file" 2>/dev/null | head -20
  fi
else
  echo "(install jq for finding summary)"
fi
