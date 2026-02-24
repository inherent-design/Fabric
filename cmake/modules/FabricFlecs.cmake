# FabricFlecs.cmake - Fetch and configure Flecs ECS framework
include_guard()

# Preserve BUILD_SHARED_LIBS state: Flecs may flip it
set(_FABRIC_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)

CPMAddPackage(
    NAME flecs
    GITHUB_REPOSITORY SanderMertens/flecs
    GIT_TAG v4.1.4
    OPTIONS
        "FLECS_STATIC ON"
        "FLECS_SHARED OFF"
        "FLECS_TESTS OFF"
        "FLECS_EXAMPLES OFF"
    SYSTEM
    EXCLUDE_FROM_ALL
)

# Restore BUILD_SHARED_LIBS
set(BUILD_SHARED_LIBS ${_FABRIC_SAVED_BUILD_SHARED_LIBS})
