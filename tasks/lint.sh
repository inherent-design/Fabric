#!/bin/sh
# Run clang-tidy on Fabric source files.
# Env: LINT_FIX     - set to "1" to apply fixes (CAUTION: can break cross-file refs)
# Env: LINT_CHANGED - set to "1" to only lint git-dirty files (fast)
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

# Collect files to lint
if [ "${LINT_CHANGED:-}" = "1" ] || [ "${LINT_CHANGED:-}" = "true" ]; then
  echo "Linting changed files only"
  files=$(git diff --name-only --diff-filter=ACMR HEAD -- 'src/*.cc' 'src/*.hh' 'include/*.cc' 'include/*.hh' 2>/dev/null || true)
  staged=$(git diff --cached --name-only --diff-filter=ACMR -- 'src/*.cc' 'src/*.hh' 'include/*.cc' 'include/*.hh' 2>/dev/null || true)
  files=$(printf '%s\n%s' "$files" "$staged" | sort -u | grep -v 'Constants.g.hh' || true)
  if [ -z "$files" ]; then
    echo "No changed source files to lint"
    exit 0
  fi
else
  echo "Linting all source files"
  files=$(find src include -name '*.cc' -o -name '*.hh' | grep -v 'Constants.g.hh')
fi

echo "$files" | xargs clang-tidy \
  -p "${build_dir}" \
  $sysroot_flag \
  $fix_flag \
  --quiet 2>&1 | grep -v 'warnings generated'
