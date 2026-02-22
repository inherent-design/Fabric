#!/bin/sh
# Remove build artifacts.
set -eu

build_dir="${BUILD_DIR:-build}"

if [ -d "$build_dir" ]; then
  echo "Removing ${build_dir}/"
  rm -rf "$build_dir"
else
  echo "Nothing to clean"
fi
