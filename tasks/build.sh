#!/bin/sh
# Configure and build Fabric Engine using CMakePresets.
# Env: BUILD_PRESET - cmake preset name (default: dev-debug)
# See CMakePresets.json for available presets.
set -eu

preset="${BUILD_PRESET:-dev-debug}"
build_dir="build/${preset}"

if [ ! -f "${build_dir}/build.ninja" ]; then
  echo "Configuring (${preset})"
  cmake --preset "${preset}"
fi

echo "Building (${preset})"
cmake --build "${build_dir}" -j
