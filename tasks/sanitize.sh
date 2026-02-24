#!/bin/sh
# Build and test with AddressSanitizer + UndefinedBehaviorSanitizer.
# Requires: clang
set -eu

preset="${SANITIZE_PRESET:-ci-sanitize}"
build_dir="build/${preset}"

if ! command -v clang++ >/dev/null 2>&1; then
  echo "clang++ not found. Install via: brew install llvm" >&2
  exit 1
fi

echo "Configuring (${preset})"
cmake --preset "${preset}"

echo "Building"
cmake --build "${build_dir}" -j

echo "Running tests with sanitizers"
ctest --test-dir "${build_dir}" --output-on-failure
