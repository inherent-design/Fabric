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

# Homebrew LLVM ships a newer libc++ than the macOS system; shaderc needs
# the same rpath fix as FabricLib so __hash_memory resolves at link time.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        AND NOT CMAKE_CXX_COMPILER MATCHES "/usr/bin"
        AND TARGET shaderc)
    get_filename_component(_bgfx_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_bgfx_llvm_root "${_bgfx_compiler_dir}" DIRECTORY)
    set(_bgfx_libcxx "${_bgfx_llvm_root}/lib/c++")
    if(EXISTS "${_bgfx_libcxx}/libc++.dylib")
        target_link_options(shaderc PRIVATE "-L${_bgfx_libcxx}" "-Wl,-rpath,${_bgfx_libcxx}")
    endif()
endif()
