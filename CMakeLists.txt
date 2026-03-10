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
set(APP_NAME "Recurse")
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

# mimalloc - available for explicit use on hot paths; global override OFF by
# default because MI_OVERRIDE conflicts with dlopen'd drivers (MoltenVK/Metal).
option(FABRIC_USE_MIMALLOC "Link mimalloc as global allocator override" OFF)
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

# FastNoise2 - SIMD noise generation for terrain
include(FabricFastNoise2)

# fastgltf - high-performance glTF 2.0 parser
include(FabricFastgltf)

# ozz-animation - SoA SIMD skeletal animation runtime
include(FabricOzzAnimation)

# Jolt Physics - rigid body dynamics
include(FabricJolt)

# BehaviorTree.CPP v4 - behavior tree AI
include(FabricBehaviorTree)

# miniaudio - single-header audio library
include(FabricMiniaudio)

# toml++ - header-only TOML v1.0 parser for data files
include(FabricToml)

# efsw - cross-platform file system watcher for hot-reload
include(FabricEfsw)

# enkiTS - task scheduler for parallel dispatch
include(FabricEnkiTS)

# zstd - fast lossless compression for chunk persistence
include(FabricZstd)

# SDL3 - cross-platform windowing and input
include(FabricSDL3)

# WebView - embedded web UI
include(FabricWebView)

#------------------------------------------------------------------------------
# Source Files and Library Configuration
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Engine Source Files (fabric:: namespace)
#------------------------------------------------------------------------------
set(FABRIC_ENGINE_SOURCE_FILES
    src/core/AppModeManager.cc
    src/fabric/platform/Async.cc
    src/fabric/render/BgfxCallback.cc
    src/fabric/render/Camera.cc
    src/core/Component.cc
    src/fabric/platform/ConfigManager.cc
    src/core/ECS.cc
    src/core/Event.cc
    src/fabric/input/InputManager.cc
    src/fabric/input/InputRouter.cc
    src/core/Lifecycle.cc
    src/core/Log.cc
    src/core/LogConfig.cc
    src/core/Pipeline.cc
    src/fabric/platform/Plugin.cc
    src/fabric/render/RenderCaps.cc
    src/fabric/render/Rendering.cc
    src/core/Resource.cc
    src/core/ResourceHub.cc
    src/fabric/render/SceneView.cc
    src/core/Spatial.cc
    src/core/Temporal.cc
    src/fabric/input/InputSource.cc
    src/fabric/input/InputAxis.cc
    src/core/SystemPhase.cc
    src/core/SystemBase.cc
    src/core/SystemRegistry.cc
    src/fabric/input/InputContext.cc
    src/fabric/input/InputSystem.cc
    src/core/AssetRegistry.cc
    src/fabric/platform/FabricApp.cc
    src/fabric/platform/FileWatcher.cc
    src/core/FilteredConsoleSink.cc
    src/fabric/render/PostProcess.cc
    src/fabric/render/SkyRenderer.cc
    src/core/SimulationThreadPool.cc
    src/fabric/world/ChunkDirtyTracker.cc
    src/fabric/platform/JobScheduler.cc
)

#------------------------------------------------------------------------------
# Game Source Files (recurse:: namespace)
#------------------------------------------------------------------------------
set(RECURSE_GAME_SOURCE_FILES
    # world/
    src/recurse/world/VoxelRaycast.cc
    src/recurse/world/ChunkStreaming.cc
    src/recurse/world/EssencePalette.cc
    src/recurse/world/StructuralIntegrity.cc
    # character/
    src/recurse/character/CharacterController.cc
    src/recurse/character/FlightController.cc
    src/recurse/character/MovementFSM.cc
    src/recurse/character/TransitionController.cc
    src/recurse/character/DashController.cc
    src/recurse/character/CameraController.cc
    src/recurse/character/VoxelInteraction.cc
    src/recurse/character/MeleeSystem.cc
    # physics/
    src/recurse/physics/PhysicsWorld.cc
    src/recurse/physics/JoltCharacterController.cc
    src/recurse/physics/Ragdoll.cc
    src/recurse/physics/VoxelCollision.cc
    # ai/
    src/recurse/ai/BehaviorAI.cc
    src/recurse/ai/Pathfinding.cc
    src/recurse/ai/BTDebugPanel.cc
    # audio/
    src/recurse/audio/AudioSystem.cc
    src/recurse/audio/ReverbZone.cc
    src/recurse/audio/MaterialSounds.cc
    # animation/
    src/recurse/animation/Animation.cc
    src/recurse/animation/AnimationEvents.cc
    src/recurse/animation/SkinnedRenderer.cc
    src/recurse/animation/IKSolver.cc
    src/recurse/animation/MeshLoader.cc
    # world/ (smooth pipeline)
    src/recurse/world/ChunkDensityCache.cc
    src/recurse/world/SnapMCMesher.cc
    src/recurse/world/SnapMCTables.cc
    src/recurse/world/SurfaceNetsMesher.cc
    # render/
    src/recurse/render/OITCompositor.cc
    src/recurse/render/ParticleSystem.cc
    src/recurse/render/ShadowSystem.cc
    src/recurse/render/DebugDraw.cc
    src/recurse/render/VoxelRenderer.cc
    # persistence/
    src/recurse/persistence/WorldMetadata.cc
    src/recurse/persistence/FilesystemChunkStore.cc
    src/recurse/persistence/WorldRegistry.cc
    src/recurse/persistence/ChunkSaveService.cc
    src/recurse/persistence/SaveManager.cc
    src/recurse/persistence/DataLoader.cc
    src/recurse/persistence/SceneSerializer.cc
    src/recurse/persistence/InputRecorder.cc
    # ui/
    src/recurse/ui/ContentBrowser.cc
    src/recurse/ui/DevConsole.cc
    src/recurse/simulation/DebrisPool.cc

    # components/
    src/recurse/components/EssenceTypes.cc

    # world/ (VP0+ generators)
    src/recurse/world/WorldGenerator.cc
    src/recurse/world/TestWorldGenerator.cc

    # systems/ (SystemBase subclasses, phase-dispatched by SystemRegistry)
    src/recurse/systems/TerrainSystem.cc
    src/recurse/systems/VoxelSimulationSystem.cc
    src/recurse/systems/VoxelMeshingSystem.cc
    src/recurse/systems/ChunkPipelineSystem.cc
    src/recurse/systems/PhysicsGameSystem.cc
    src/recurse/systems/CharacterMovementSystem.cc
    src/recurse/systems/VoxelInteractionSystem.cc
    src/recurse/systems/AIGameSystem.cc
    src/recurse/systems/SaveGameSystem.cc
    src/recurse/systems/ParticleGameSystem.cc
    src/recurse/systems/CameraGameSystem.cc
    src/recurse/systems/AudioGameSystem.cc
    src/recurse/systems/ShadowRenderSystem.cc
    src/recurse/systems/VoxelRenderSystem.cc
    src/recurse/systems/OITRenderSystem.cc
    src/recurse/systems/DebugOverlaySystem.cc
    src/recurse/systems/MainMenuSystem.cc

    # LOD System (far-horizon terrain rendering)
    src/recurse/render/LODGrid.cc
    src/recurse/render/LODMeshManager.cc
    src/recurse/systems/LODSystem.cc

    # Panini projection post-processing
    src/fabric/render/PaniniPass.cc
)

# Simulation library components (recurse::simulation namespace)
set(FABRIC_SIMULATION_SOURCE_FILES
    src/recurse/simulation/ChunkRegistry.cc
    src/recurse/simulation/MaterialRegistry.cc
    src/recurse/simulation/SimulationGrid.cc
    src/recurse/simulation/ChunkActivityTracker.cc
    src/recurse/simulation/GhostCells.cc
    src/recurse/simulation/FallingSandSystem.cc
    src/recurse/simulation/VoxelSimulationSystem.cc
)

# World library components (fabric::world namespace)
set(FABRIC_WORLD_SOURCE_FILES
    src/world/FlatGenerator.cc
    src/world/LayeredGenerator.cc
    src/world/SingleMaterialGenerator.cc
    src/world/MinecraftNoiseGenerator.cc
)

# Utils library components
set(FABRIC_UTILS_SOURCE_FILES
    src/utils/ErrorHandling.cc
    src/utils/ThreadPoolExecutor.cc
    src/utils/Utils.cc
)

# Parser library components
set(FABRIC_PARSER_SOURCE_FILES
    src/parser/ArgumentParser.cc
    src/parser/SyntaxTree.cc
)

# Platform library components
set(FABRIC_PLATFORM_SOURCE_FILES
    src/platform/PlatformInfo.cc
    src/platform/WindowDesc.cc
    src/platform/CursorManager.cc
)

# Codec library components (header-only; source list kept for future additions)
set(FABRIC_CODEC_SOURCE_FILES
)

# UI library components
set(FABRIC_UI_SOURCE_FILES
    src/ui/WebView.cc
    src/ui/BgfxRenderInterface.cc
    src/ui/BgfxSystemInterface.cc
    src/ui/StbImage.cc
    src/ui/RmlPanel.cc
    src/ui/DebugHUD.cc
    src/ui/ChunkDebugPanel.cc
    src/ui/LODStatsPanel.cc
    src/ui/ConcurrencyPanel.cc
    src/ui/WAILAPanel.cc
    src/ui/HotkeyPanel.cc
    src/ui/ToastManager.cc
)

# bgfx debugdraw (extracted from examples/common for debug overlays)
set(FABRIC_BGFX_DEBUGDRAW_SOURCE
    ${bgfx_SOURCE_DIR}/bgfx/examples/common/debugdraw/debugdraw.cpp
)

# Create internal static library (compile sources once, link into all targets)
add_library(FabricLib STATIC
    ${FABRIC_ENGINE_SOURCE_FILES}
    ${FABRIC_SIMULATION_SOURCE_FILES}
    ${FABRIC_WORLD_SOURCE_FILES}
    ${RECURSE_GAME_SOURCE_FILES}
    ${FABRIC_UTILS_SOURCE_FILES}
    ${FABRIC_PLATFORM_SOURCE_FILES}
    ${FABRIC_PARSER_SOURCE_FILES}
    ${FABRIC_CODEC_SOURCE_FILES}
    ${FABRIC_UI_SOURCE_FILES}
    ${FABRIC_BGFX_DEBUGDRAW_SOURCE}
)

# Build dir first so generated Constants.g.hh takes precedence over any stale source copy
target_include_directories(FabricLib
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${FABRIC_RMLUI_SHADER_OUT}
        ${bgfx_SOURCE_DIR}/bimg/3rdparty/stb
        ${bgfx_SOURCE_DIR}/bgfx/examples/common
        ${bgfx_SOURCE_DIR}/bgfx/examples/common/debugdraw
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
        FastNoise
        fastgltf::fastgltf
        ozz_animation
        ozz_base
        ozz_geometry
        Jolt
        behaviortree_cpp
        miniaudio
        tomlplusplus::tomlplusplus
        efsw
        enkiTS
        libzstd_static
)

target_compile_definitions(FabricLib
    PUBLIC
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_SILENT_WARNINGS
        $<$<CONFIG:Debug>:FABRIC_ECS_INSPECTOR>
    PRIVATE
        $<$<CONFIG:Debug>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_DEBUG>
        $<$<CONFIG:Release>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_INFO>
        $<$<CONFIG:RelWithDebInfo>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_DEBUG>
)

# Tracy profiling (no-op when FABRIC_ENABLE_PROFILING is OFF)
fabric_link_tracy(FabricLib)

# Skinned mesh shaders (vs_skinned, fs_skinned)
include(FabricSkinnedShaders)

# Smooth voxel shaders (vs_smooth, fs_smooth) - Journey-style 3-layer composition
include(FabricSmoothShaders)

# Sky shaders (vs_sky, fs_sky)
include(FabricSkyShaders)

# Post-process shaders (vs_fullscreen, fs_bright, fs_blur, fs_tonemap)
include(FabricPostShaders)

# Particle shaders (vs_particle, fs_particle)
include(FabricParticleShaders)

# OIT shaders (vs_oit_accum, fs_oit_accum, vs_oit_composite, fs_oit_composite)
include(FabricOITShaders)

# Panini projection shaders (vs_panini, fs_panini)
include(FabricPaniniShaders)

# Add shader output to include path
target_include_directories(FabricLib PRIVATE ${FABRIC_SKINNED_SHADER_OUT} ${FABRIC_SMOOTH_SHADER_OUT} ${FABRIC_SKY_SHADER_OUT} ${FABRIC_POST_SHADER_OUT} ${FABRIC_PARTICLE_SHADER_OUT} ${FABRIC_OIT_SHADER_OUT} ${FABRIC_PANINI_SHADER_OUT})

# Shader compilation must finish before FabricLib sources compile
add_dependencies(FabricLib FabricRmlUiShaders FabricSkinnedShaders FabricSmoothShaders FabricSkyShaders FabricPostShaders FabricParticleShaders FabricOITShaders FabricPaniniShaders)

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
    ${FABRIC_ENGINE_SOURCE_FILES}
    ${RECURSE_GAME_SOURCE_FILES}
    PROPERTIES
    COMPILE_DEFINITIONS "FABRIC_CORE_API"
)

# Homebrew LLVM rpath fix (shared with FabricBgfx shaderc)
include(FabricHomebrew)
fabric_fix_homebrew_llvm_rpath(FabricLib)

# Create the Recurse executable (canonical entry point, FabricAppDesc pattern).
# Uses FabricApp::run() with lifecycle callbacks; all game logic in Recurse.cc.
if(FABRIC_USE_MIMALLOC)
    add_executable(Recurse src/recurse/Recurse.cc src/core/MimallocOverride.cc)
    target_link_libraries(Recurse PRIVATE FabricLib mimalloc-static)
else()
    add_executable(Recurse src/recurse/Recurse.cc)
    target_link_libraries(Recurse PRIVATE FabricLib)
endif()

if(APPLE)
    set_target_properties(Recurse PROPERTIES
        BUILD_RPATH "/opt/homebrew/lib"
    )
endif()

add_custom_command(TARGET Recurse POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Recurse>/assets
    COMMENT "Copying assets to build output directory")

add_custom_command(TARGET Recurse POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/config/fabric.toml $<TARGET_FILE_DIR:Recurse>/fabric.toml
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/config/recurse.toml $<TARGET_FILE_DIR:Recurse>/recurse.toml
    COMMENT "Copying config files to build output directory")

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
    # ozz_animation_offline is needed for building test skeletons and animations
    foreach(TEST_TARGET UnitTests E2ETests)
        target_link_libraries(${TEST_TARGET} PRIVATE
            FabricLib
            GTest::gtest
            GTest::gmock
            ozz_animation_offline
        )
    endforeach()

    # Test fixture include path (tests/fixtures/)
    target_include_directories(UnitTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

    # Add test directories
    add_subdirectory(tests)

    # Register tests with ctest via GoogleTest discovery
    include(GoogleTest)
    gtest_discover_tests(UnitTests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
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
    set_target_properties(Recurse PROPERTIES WIN32_EXECUTABLE TRUE)

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

install(TARGETS Recurse
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hh"
)
