#!/bin/sh
# Run cppcheck static analysis on Fabric source files.
# Suppressions: .cppcheck-suppressions (shared with CI)
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if ! command -v cppcheck >/dev/null 2>&1; then
  echo "cppcheck not found. Install via: brew install cppcheck" >&2
  exit 1
fi

echo "Running cppcheck ($(cppcheck --version))"
cppcheck \
  --enable=warning,performance,portability \
  --error-exitcode=1 \
  --suppressions-list="${PROJECT_ROOT}/.cppcheck-suppressions" \
  -I include/ \
  src/ include/
