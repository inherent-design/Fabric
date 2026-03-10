# FabricZstd.cmake - Fetch and configure zstd (Zstandard compression)
include_guard()

CPMAddPackage(
    NAME zstd
    GITHUB_REPOSITORY facebook/zstd
    GIT_TAG v1.5.7
    SOURCE_SUBDIR build/cmake
    OPTIONS
        "ZSTD_BUILD_PROGRAMS OFF"
        "ZSTD_BUILD_TESTS OFF"
        "ZSTD_BUILD_CONTRIB OFF"
        "ZSTD_BUILD_SHARED OFF"
        "ZSTD_BUILD_STATIC ON"
    SYSTEM
    EXCLUDE_FROM_ALL
)

if(zstd_ADDED)
    target_include_directories(libzstd_static SYSTEM INTERFACE
        "${zstd_SOURCE_DIR}/lib"
    )
endif()
