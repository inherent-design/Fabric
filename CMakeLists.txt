# Fabric Engine - CMake Build Configuration
cmake_minimum_required(VERSION 3.25)

# macOS deployment target must be set before project() for proper propagation
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS deployment version")
endif()

# Project definition
project(fabric
    VERSION 0.1.0
    DESCRIPTION "Fabric Engine"
    LANGUAGES CXX
)

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

#------------------------------------------------------------------------------
# Build Configuration
#------------------------------------------------------------------------------

# C++ standard settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# GNU extensions required: Quill's QUILL_LOG_* macros use ##__VA_ARGS__
# (GCC comma-eating extension) which requires -std=gnu++20 on GCC.
# Clang and MSVC handle this regardless of the extensions setting.
set(CMAKE_CXX_EXTENSIONS ON)

# Output directories configuration
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Enable position independent code for libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Platform-specific thread library linking
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#------------------------------------------------------------------------------
# Generate Constants Header
#------------------------------------------------------------------------------
set(APP_NAME "Fabric")
set(APP_VERSION "${PROJECT_VERSION}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Constants.g.hh.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/fabric/core/Constants.g.hh
    @ONLY
)

#------------------------------------------------------------------------------
# Dependencies (CPM.cmake - source-cached package manager)
#------------------------------------------------------------------------------
include(cmake/CPM.cmake)

# Google Test for testing
include(FabricGoogleTest)

# GLM (OpenGL Mathematics) - header-only math library
include(FabricGLM)

# mimalloc - global allocator replacement (gated for sanitizer compatibility)
option(FABRIC_USE_MIMALLOC "Link mimalloc as global allocator override" ON)
if(FABRIC_USE_MIMALLOC)
    include(FabricMimalloc)
endif()

# Quill - async structured logging
include(FabricQuill)

# nlohmann/json - JSON serialization
include(FabricNlohmannJson)

# Tracy Profiler (optional, controlled by FABRIC_ENABLE_PROFILING)
include(FabricTracy)

# Standalone Asio - async I/O with C++20 coroutines
include(FabricAsio)

# bgfx rendering backend (bx + bimg + bgfx via bgfx.cmake)
include(FabricBgfx)

# RmlUi - HTML/CSS UI (scaffold only, full integration is Sprint 3)
include(FabricRmlUi)

# Flecs ECS framework
include(FabricFlecs)

# SDL3 Configuration
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST OFF)

# Enable core SDL3 subsystems
foreach(SUBSYSTEM IN ITEMS AUDIO VIDEO RENDER EVENTS JOYSTICK HIDAPI SENSOR THREADS TIMERS)
    set(SDL_${SUBSYSTEM} ON)
endforeach()

CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.2
    SYSTEM
    EXCLUDE_FROM_ALL
)

# WebView
option(FABRIC_USE_WEBVIEW "Enable WebView support" ON)

# Set WebKitGTK API version for Linux (4.1 is available on Ubuntu 22.04+, Fedora 38+, Arch)
if(UNIX AND NOT APPLE)
    set(WEBVIEW_WEBKITGTK_API "4.1" CACHE STRING "WebKitGTK API version")
endif()

CPMAddPackage(
    NAME webview
    GITHUB_REPOSITORY webview/webview
    GIT_TAG 0.12.0
    SYSTEM
    EXCLUDE_FROM_ALL
)

#------------------------------------------------------------------------------
# Source Files and Library Configuration
#------------------------------------------------------------------------------
# Core library components
set(FABRIC_CORE_SOURCE_FILES
    src/core/Component.cc
    src/core/Event.cc
    src/core/Lifecycle.cc
    src/core/Log.cc
    src/core/Pipeline.cc
    src/core/Plugin.cc
    src/core/Resource.cc
    src/core/Spatial.cc
    src/core/Temporal.cc
    src/core/ResourceHub.cc
    src/core/Async.cc
    src/core/Rendering.cc
    src/core/Camera.cc
    src/core/InputManager.cc
    src/core/SceneView.cc
    src/core/ECS.cc
    src/core/Simulation.cc
    src/core/VoxelMesher.cc
)

# Utils library components
set(FABRIC_UTILS_SOURCE_FILES
    src/utils/BufferPool.cc
    src/utils/ErrorHandling.cc
    src/utils/ThreadPoolExecutor.cc
    src/utils/Utils.cc
)

# Parser library components
set(FABRIC_PARSER_SOURCE_FILES
    src/parser/ArgumentParser.cc
    src/parser/SyntaxTree.cc
)

# Codec library components
set(FABRIC_CODEC_SOURCE_FILES
    src/codec/Codec.cc
)

# UI library components
set(FABRIC_UI_SOURCE_FILES
    src/ui/WebView.cc
    src/ui/BgfxRenderInterface.cc
    src/ui/BgfxSystemInterface.cc
    src/ui/StbImage.cc
)

# Create internal static library (compile sources once, link into all targets)
add_library(FabricLib STATIC
    ${FABRIC_CORE_SOURCE_FILES}
    ${FABRIC_UTILS_SOURCE_FILES}
    ${FABRIC_PARSER_SOURCE_FILES}
    ${FABRIC_CODEC_SOURCE_FILES}
    ${FABRIC_UI_SOURCE_FILES}
)

# Build dir first so generated Constants.g.hh takes precedence over any stale source copy
target_include_directories(FabricLib
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${FABRIC_RMLUI_SHADER_OUT}
        ${bgfx_SOURCE_DIR}/bimg/3rdparty/stb
)

target_link_libraries(FabricLib
    PUBLIC
        Threads::Threads
        SDL3::SDL3-static
        webview::core
        bgfx
        bx
        bimg
        glm::glm
        quill::quill
        nlohmann_json::nlohmann_json
        asio::asio
        rmlui_core
        flecs::flecs_static
)

target_compile_definitions(FabricLib
    PUBLIC
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_SILENT_WARNINGS
    PRIVATE
        $<$<CONFIG:Debug>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_DEBUG>
        $<$<CONFIG:Release>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_INFO>
        $<$<CONFIG:RelWithDebInfo>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_DEBUG>
)

# Tracy profiling (no-op when FABRIC_ENABLE_PROFILING is OFF)
fabric_link_tracy(FabricLib)

# Shader compilation must finish before FabricLib sources compile
add_dependencies(FabricLib FabricRmlUiShaders)

# Per-target compile options (replaces global CMAKE_CXX_FLAGS_* variables)
target_compile_options(FabricLib PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        $<$<CONFIG:Debug>:/Zi /Od>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/Zi /O2>
    >
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
        -Wall -Wextra
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-g -O3>
    >
)

# WebView feature flag
if(FABRIC_USE_WEBVIEW)
    target_compile_definitions(FabricLib PUBLIC FABRIC_USE_WEBVIEW)
endif()

# Mark public API source files
set_source_files_properties(
    ${FABRIC_CORE_SOURCE_FILES}
    PROPERTIES
    COMPILE_DEFINITIONS "FABRIC_CORE_API"
)

# Homebrew LLVM ships a newer libc++ than the macOS system; link against it
# so symbols like __hash_memory resolve.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER MATCHES "/usr/bin")
    get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_llvm_root "${_compiler_dir}" DIRECTORY)
    set(_libcxx_path "${_llvm_root}/lib/c++")
    if(EXISTS "${_libcxx_path}/libc++.dylib")
        target_link_options(FabricLib PUBLIC "-L${_libcxx_path}" "-Wl,-rpath,${_libcxx_path}")
    endif()
endif()

# Create the Fabric executable
if(FABRIC_USE_MIMALLOC)
    add_executable(Fabric src/core/Fabric.cc src/core/MimallocOverride.cc)
    target_link_libraries(Fabric PRIVATE FabricLib mimalloc-static)
else()
    add_executable(Fabric src/core/Fabric.cc)
    target_link_libraries(Fabric PRIVATE FabricLib)
endif()

#------------------------------------------------------------------------------
# Testing Configuration
#------------------------------------------------------------------------------
option(FABRIC_BUILD_TESTS "Build Fabric tests" ON)

if(FABRIC_BUILD_TESTS)
    enable_testing()

    # Create test executables with custom main (initializes Quill logging)
    add_executable(UnitTests tests/TestMain.cc)
    add_executable(E2ETests tests/TestMain.cc)

    # Link against FabricLib and GTest (FabricLib provides all transitive deps)
    foreach(TEST_TARGET UnitTests E2ETests)
        target_link_libraries(${TEST_TARGET} PRIVATE
            FabricLib
            GTest::gtest
            GTest::gmock
        )
    endforeach()

    # Add test directories
    add_subdirectory(tests)

    # Register tests with ctest via GoogleTest discovery
    include(GoogleTest)
    gtest_discover_tests(UnitTests)
    gtest_discover_tests(E2ETests)
endif()

#------------------------------------------------------------------------------
# Platform-Specific Configuration
#------------------------------------------------------------------------------
# macOS Configuration
if(APPLE)
    message(STATUS "Configuring for macOS")

    # Build universal binary (off by default; enable for release builds)
    option(FABRIC_BUILD_UNIVERSAL "Build universal binaries for macOS" OFF)
    if(FABRIC_BUILD_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    endif()

    # Frameworks required by Fabric's own code (SDL3 handles its own transitively)
    set(MACOS_FRAMEWORKS
        Cocoa
        WebKit
        Metal
        QuartzCore
    )

    foreach(FRAMEWORK ${MACOS_FRAMEWORKS})
        target_link_libraries(FabricLib PUBLIC "-framework ${FRAMEWORK}")
    endforeach()
endif()

# Linux Configuration
if(UNIX AND NOT APPLE)
    message(STATUS "Configuring for Linux")

    find_package(PkgConfig REQUIRED)

    # WebView dependencies (try 4.1 first, fall back to 4.0)
    pkg_check_modules(WEBKIT webkit2gtk-4.1)
    if(NOT WEBKIT_FOUND)
        pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.0)
    endif()
    target_include_directories(FabricLib PUBLIC ${WEBKIT_INCLUDE_DIRS})
    target_link_libraries(FabricLib PUBLIC ${WEBKIT_LIBRARIES})

    # SDL3 handles X11/Wayland dependencies transitively via SDL3::SDL3-static

    # Vulkan SDK for bgfx rendering backend
    find_package(Vulkan)
    if(Vulkan_FOUND)
        target_link_libraries(FabricLib PUBLIC Vulkan::Vulkan)
    endif()
endif()

# Windows Configuration
if(WIN32)
    message(STATUS "Configuring for Windows")

    # Target Windows 10 (required by SDL3 and WebView2)
    # NOMINMAX: prevent <windows.h> from defining min/max macros that break
    # std::numeric_limits<T>::max() in Quill and other template code.
    # WIN32_LEAN_AND_MEAN: exclude rarely-used Windows headers, speeds compilation.
    target_compile_definitions(FabricLib PUBLIC
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        NTDDI_VERSION=0x0A000007
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    # Configure as Windows GUI application
    set_target_properties(Fabric PROPERTIES WIN32_EXECUTABLE TRUE)

    # Windows-specific libraries (shlwapi and version required by webview 0.12)
    set(WINDOWS_LIBRARIES
        user32
        gdi32
        ole32
        shell32
        advapi32
        shlwapi
        version
        d3d12
        dxgi
        dxguid
    )

    target_link_libraries(FabricLib PUBLIC ${WINDOWS_LIBRARIES})
endif()

#------------------------------------------------------------------------------
# Installation Configuration
#------------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS Fabric
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hh"
)
