#!/bin/sh
# Check or fix clang-format on Fabric source files.
# Env: FORMAT_FIX - set to "1" to apply fixes (default: check-only)
set -eu

fix_mode="${FORMAT_FIX:-}"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found. Install via: brew install llvm" >&2
  exit 1
fi

if [ "$fix_mode" = "1" ] || [ "$fix_mode" = "true" ]; then
  echo "Formatting source files"
  find src include -name '*.cc' -o -name '*.hh' | xargs clang-format -i
  echo "Done"
else
  echo "Checking format (dry-run)"
  find src include -name '*.cc' -o -name '*.hh' | xargs clang-format --dry-run --Werror
  echo "All files formatted correctly"
fi
