#!/bin/sh
# Run Fabric test suites.
# Env: TEST_SUITE   - which suite: unit, e2e, all (default: unit)
# Env: TEST_FILTER  - gtest --gtest_filter value (default: disabled)
# Env: TEST_TIMEOUT - per-suite timeout in seconds (default: 120)
set -eu

build_dir="${BUILD_DIR:-build/dev-debug}"
suite="${TEST_SUITE:-unit}"
timeout_sec="${TEST_TIMEOUT:-120}"
run_in_build_dir=0

case "$suite" in
  unit) bin="${build_dir}/bin/UnitTests" ;;
  e2e)
    bin="${build_dir}/bin/E2ETests"
    run_in_build_dir=1
    ;;
  all)
    for s in unit e2e; do
      TEST_SUITE="$s" sh "$(dirname "$0")/test.sh"
    done
    exit 0
    ;;
  *)
    echo "Unknown suite: ${suite}" >&2
    echo "Valid: unit, e2e, all" >&2
    exit 1
    ;;
esac

if [ ! -x "$bin" ]; then
  echo "Binary not found: ${bin}" >&2
  echo "Run 'mise run build' first" >&2
  exit 1
fi

filter_flag=""
if [ -n "${TEST_FILTER:-}" ]; then
  filter_flag="--gtest_filter=${TEST_FILTER}"
fi

echo "Running ${suite} tests (timeout: ${timeout_sec}s)"

cmd="$bin"
if [ "$run_in_build_dir" = "1" ]; then
  cmd="./bin/E2ETests"
  old_pwd=$(pwd)
  cd "$build_dir"
fi

if command -v timeout >/dev/null 2>&1; then
  timeout "${timeout_sec}" $cmd $filter_flag
elif command -v gtimeout >/dev/null 2>&1; then
  gtimeout "${timeout_sec}" $cmd $filter_flag
else
  $cmd $filter_flag
fi

if [ "$run_in_build_dir" = "1" ]; then
  cd "$old_pwd"
fi
