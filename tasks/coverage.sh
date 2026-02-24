#!/bin/sh
# Build with coverage instrumentation and generate lcov report.
# Requires: clang, llvm-profdata, llvm-cov
set -eu

build_dir="build/ci-coverage"

for tool in clang++ llvm-profdata llvm-cov; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "$tool not found. Install via: brew install llvm" >&2
    exit 1
  fi
done

echo "Configuring (ci-coverage)"
cmake --preset ci-coverage

echo "Building"
cmake --build "${build_dir}" -j

echo "Running tests"
LLVM_PROFILE_FILE="${build_dir}/fabric-%p.profraw" \
  ctest --test-dir "${build_dir}" --output-on-failure

echo "Merging profiles"
llvm-profdata merge -sparse "${build_dir}"/fabric-*.profraw -o "${build_dir}/coverage.profdata"

echo "Generating lcov report"
llvm-cov export \
  "${build_dir}/bin/UnitTests" \
  -instr-profile="${build_dir}/coverage.profdata" \
  -format=lcov \
  -ignore-filename-regex='build/|_deps/|tests/' \
  > "${build_dir}/coverage.lcov"

echo "Coverage report: ${build_dir}/coverage.lcov"

# Show summary
llvm-cov report \
  "${build_dir}/bin/UnitTests" \
  -instr-profile="${build_dir}/coverage.profdata" \
  -ignore-filename-regex='build/|_deps/|tests/'
