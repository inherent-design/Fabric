# FabricBgfx.cmake - Fetch and configure bgfx rendering backend
include(FetchContent)

# bgfx.cmake bundles bx, bimg, bgfx as submodules
FetchContent_Declare(
    bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG        v1.139.9155-513
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)

# Suppress bgfx examples (they pull in extra dependencies)
set(BGFX_BUILD_EXAMPLES     OFF CACHE BOOL "Build bgfx examples" FORCE)
set(BGFX_BUILD_TOOLS        ON  CACHE BOOL "Build bgfx tools (shaderc, texturec, geometryc)" FORCE)
set(BGFX_INSTALL            OFF CACHE BOOL "Create installation target" FORCE)
set(BGFX_CUSTOM_TARGETS     OFF CACHE BOOL "Include custom targets" FORCE)

FetchContent_MakeAvailable(bgfx)
