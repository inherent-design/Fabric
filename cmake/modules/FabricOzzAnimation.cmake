# FabricOzzAnimation.cmake - Fetch and configure ozz-animation runtime library
include_guard()

# Disable tools, samples, tests, and howtos -- only runtime libraries needed
CPMAddPackage(
    NAME ozz-animation
    GITHUB_REPOSITORY guillaumeblanc/ozz-animation
    GIT_TAG 0.16.0
    OPTIONS
        "ozz_build_tools OFF"
        "ozz_build_fbx OFF"
        "ozz_build_gltf OFF"
        "ozz_build_samples OFF"
        "ozz_build_howtos OFF"
        "ozz_build_tests OFF"
        "ozz_build_data OFF"
        "ozz_build_postfix OFF"
    SYSTEM
    EXCLUDE_FROM_ALL
)
