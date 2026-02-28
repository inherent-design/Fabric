# FabricEfsw.cmake - Cross-platform file system watcher
include_guard()

# Preserve BUILD_SHARED_LIBS state
set(_FABRIC_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)

CPMAddPackage(
    NAME efsw
    GITHUB_REPOSITORY SpartanJ/efsw
    GIT_TAG 1.5.1
    OPTIONS
        "BUILD_STATIC_LIBS ON"
        "BUILD_SHARED_LIBS OFF"
        "BUILD_TEST_APP OFF"
    SYSTEM
    EXCLUDE_FROM_ALL
)

# Restore BUILD_SHARED_LIBS
set(BUILD_SHARED_LIBS ${_FABRIC_SAVED_BUILD_SHARED_LIBS})
