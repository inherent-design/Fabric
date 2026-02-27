# FabricJolt.cmake - Fetch and configure Jolt Physics rigid body dynamics
include_guard()

# Jolt's Build/CMakeLists.txt gates test/sample targets behind
# CMAKE_CURRENT_SOURCE_DIR == CMAKE_SOURCE_DIR, so they are automatically
# excluded when consumed as a subdirectory dependency.
CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    GIT_TAG v5.5.0
    SOURCE_SUBDIR "Build"
    SYSTEM
    EXCLUDE_FROM_ALL
)
