# FabricBgfx.cmake - Fetch and configure bgfx rendering backend
include_guard()

# bgfx requires Objective-C++ on Apple for Metal and Vulkan (MoltenVK) renderers
if(APPLE)
    enable_language(OBJCXX)
endif()

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

# bgfx.cmake bundles bx, bimg, bgfx as submodules
CPMAddPackage(
    NAME bgfx
    GITHUB_REPOSITORY bkaradzic/bgfx.cmake
    GIT_TAG v1.139.9155-513
    OPTIONS
        "BGFX_BUILD_EXAMPLES OFF"
        "BGFX_BUILD_TOOLS ON"
        "BGFX_INSTALL OFF"
        "BGFX_CUSTOM_TARGETS OFF"
        "BGFX_AMALGAMATED ON"
    PATCHES
        "${CMAKE_CURRENT_LIST_DIR}/../patches/bgfx-vk-suboptimal.patch"
    SYSTEM
    EXCLUDE_FROM_ALL
)

set(CMAKE_CXX_FLAGS "${_bgfx_saved_cxx}")
set(CMAKE_C_FLAGS   "${_bgfx_saved_c}")
set(CMAKE_EXE_LINKER_FLAGS "${_bgfx_saved_exe}")

# Vulkan-only: disable all non-Vulkan renderer backends in bgfx.
# Defining any BGFX_CONFIG_RENDERER_* causes bgfx/src/config.h to skip
# auto-detection; undefined renderers default to 0.
if(TARGET bgfx)
    target_compile_definitions(bgfx PRIVATE
        BGFX_CONFIG_RENDERER_VULKAN=1
        BGFX_CONFIG_RENDERER_DIRECT3D11=0
        BGFX_CONFIG_RENDERER_DIRECT3D12=0
        BGFX_CONFIG_RENDERER_METAL=0
        BGFX_CONFIG_RENDERER_OPENGL=0
        BGFX_CONFIG_RENDERER_OPENGLES=0
        BGFX_CONFIG_RENDERER_WEBGPU=0
    )
endif()

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

# Fabric targets Vulkan/SPIR-V exclusively; DXIL compilation is unnecessary.
# Disabling avoids a build failure on Windows SDK 10.0.19041.0 whose dxcapi.h
# lacks the IDxcCompiler3 API that bgfx's shaderc_dxil.cpp expects.
if(TARGET shaderc)
    target_compile_definitions(shaderc PRIVATE SHADERC_CONFIG_HAS_DXC=0)
endif()

# Homebrew LLVM rpath fix for shaderc
include(FabricHomebrew)
if(TARGET shaderc)
    fabric_fix_homebrew_llvm_rpath(shaderc)
endif()
