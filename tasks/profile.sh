#!/bin/sh
# Build Fabric with Tracy profiler instrumentation.
# Env: PROFILE_MODE - "debug" (default) or "release" (RelWithDebInfo + optimizations)
set -eu

mode="${PROFILE_MODE:-debug}"

case "$mode" in
    debug)
        preset="dev-profile-debug"
        ;;
    release)
        preset="dev-profile-release"
        ;;
    *)
        echo "Unknown PROFILE_MODE: $mode (expected: debug, release)" >&2
        exit 1
        ;;
esac

build_dir="build/${preset}"

if [ ! -f "${build_dir}/build.ninja" ]; then
    echo "Configuring (${preset})"
    cmake --preset "${preset}"
fi

echo "Building (${preset})"
cmake --build "${build_dir}" -j

echo ""
echo "Profile build ready: ${build_dir}/bin/Recurse"
echo "Run with: mise run profile:capture"
