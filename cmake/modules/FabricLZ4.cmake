# FabricLZ4.cmake - Fetch and configure LZ4 (fast compression for hot-path I/O)
include_guard()

CPMAddPackage(
    NAME lz4
    GITHUB_REPOSITORY lz4/lz4
    GIT_TAG v1.10.0
    SOURCE_SUBDIR build/cmake
    OPTIONS
        "LZ4_BUILD_CLI OFF"
        "LZ4_BUILD_LEGACY_LZ4C OFF"
        "BUILD_SHARED_LIBS OFF"
        "BUILD_STATIC_LIBS ON"
    SYSTEM
    EXCLUDE_FROM_ALL
)
