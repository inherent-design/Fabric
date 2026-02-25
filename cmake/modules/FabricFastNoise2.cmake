# FabricFastNoise2.cmake - Fetch and configure FastNoise2 (SIMD noise generation)

set(FASTNOISE2_TESTS OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_TOOLS OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_UTILITY OFF CACHE BOOL "" FORCE)

CPMAddPackage(
    NAME FastNoise2
    GITHUB_REPOSITORY Auburn/FastNoise2
    GIT_TAG v1.1.1
    SYSTEM
    EXCLUDE_FROM_ALL
)
