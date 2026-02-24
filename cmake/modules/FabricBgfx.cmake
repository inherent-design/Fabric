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

FetchContent_MakeAvailable(bgfx)

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
