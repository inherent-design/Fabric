# FabricBgfx.cmake - Fetch and configure bgfx rendering backend
include(FetchContent)

# bgfx requires Objective-C++ on Apple for Metal and Vulkan (MoltenVK) renderers
if(APPLE)
    enable_language(OBJCXX)
endif()

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
set(BGFX_AMALGAMATED       ON  CACHE BOOL "Amalgamate sources for faster builds" FORCE)

# bgfx bundles third-party code (glsl-optimizer) with undefined-behavior
# issues that crash shaderc when built with sanitizers.  Temporarily strip
# sanitizer/coverage flags so the entire bgfx subtree builds clean, then
# restore them for Fabric's own targets.
set(_bgfx_saved_cxx "${CMAKE_CXX_FLAGS}")
set(_bgfx_saved_c   "${CMAKE_C_FLAGS}")
set(_bgfx_saved_exe "${CMAKE_EXE_LINKER_FLAGS}")
string(REGEX REPLACE "-f(sanitize|no-omit-frame-pointer|profile-instr-generate|coverage-mapping)[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "-f(sanitize|no-omit-frame-pointer|profile-instr-generate|coverage-mapping)[^ ]*" "" CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}")
string(REGEX REPLACE "-f(sanitize|profile-instr-generate)[^ ]*" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

FetchContent_MakeAvailable(bgfx)

set(CMAKE_CXX_FLAGS "${_bgfx_saved_cxx}")
set(CMAKE_C_FLAGS   "${_bgfx_saved_c}")
set(CMAKE_EXE_LINKER_FLAGS "${_bgfx_saved_exe}")

# Xcode 26+ SDK requires ObjC++ for Foundation headers included transitively
# by bgfx Vulkan (via MoltenVK) and WebGPU renderers. Without this, pure C++
# compilation fails with "unknown type name 'NSString'" errors.
if(APPLE AND TARGET bgfx)
    set_source_files_properties(
        "${bgfx_SOURCE_DIR}/bgfx/src/renderer_vk.cpp"
        "${bgfx_SOURCE_DIR}/bgfx/src/renderer_webgpu.cpp"
        TARGET_DIRECTORY bgfx
        PROPERTIES LANGUAGE OBJCXX
    )
endif()

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
