# FabricFlecs.cmake - Fetch and configure Flecs ECS framework
include_guard()

include(FetchContent)

FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG        v4.0.5
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)

# Static library only, no samples/tests
set(FLECS_STATIC ON CACHE BOOL "" FORCE)
set(FLECS_SHARED OFF CACHE BOOL "" FORCE)
set(FLECS_TESTS OFF CACHE BOOL "" FORCE)
set(FLECS_EXAMPLES OFF CACHE BOOL "" FORCE)

# Preserve BUILD_SHARED_LIBS state: Flecs may flip it
set(_FABRIC_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(flecs)

# Restore BUILD_SHARED_LIBS
set(BUILD_SHARED_LIBS ${_FABRIC_SAVED_BUILD_SHARED_LIBS})
