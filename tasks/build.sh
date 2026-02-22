#!/bin/sh
# Configure and build Fabric Engine.
# Env: BUILD_TYPE  - cmake build type (default: Debug)
# Env: BUILD_DIR   - build output directory (default: build)
# Env: EXTRA_FLAGS - extra cmake flags
set -eu

build_type="${BUILD_TYPE:-Debug}"
build_dir="${BUILD_DIR:-build}"

if [ ! -f "${build_dir}/build.ninja" ]; then
  echo "Configuring (${build_type})"
  cmake -G Ninja \
    -DCMAKE_BUILD_TYPE="${build_type}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ${EXTRA_FLAGS:-} \
    -B "${build_dir}"
fi

echo "Building (${build_type})"
cmake --build "${build_dir}"
