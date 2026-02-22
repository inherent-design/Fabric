#!/bin/sh
# Run clang-tidy on Fabric source files.
# Env: LINT_FIX - set to "1" to apply fixes (default: check-only)
# Requires: compile_commands.json in build dir (cmake generates this)
set -eu

build_dir="${BUILD_DIR:-build}"

if [ ! -f "${build_dir}/compile_commands.json" ]; then
  echo "No compile_commands.json found. Configuring build first."
  cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B "${build_dir}"
fi

fix_flag=""
case "${LINT_FIX:-}" in
  1|true) fix_flag="--fix" ;;
esac

# Resolve macOS SDK sysroot for Homebrew LLVM clang-tidy
sysroot_flag=""
if [ "$(uname)" = "Darwin" ] && command -v xcrun >/dev/null 2>&1; then
  sysroot_flag="--extra-arg=--sysroot=$(xcrun --show-sdk-path)"
fi

echo "Running clang-tidy"

# Lint fabric sources only (skip deps, generated, tests)
find src include -name '*.cc' -o -name '*.hh' | \
  grep -v 'Constants.g.hh' | \
  xargs clang-tidy \
    -p "${build_dir}" \
    $sysroot_flag \
    $fix_flag \
    --quiet
