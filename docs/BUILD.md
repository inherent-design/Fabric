# Build Guide

## Prerequisites

- [mise](https://mise.jdx.dev/): manages cmake, ninja, and ccache automatically via `mise install`
- C++20 compiler:
  - Apple Clang (macOS, via Xcode Command Line Tools)
  - GCC 10+ (Linux)
  - Clang 13+ (Linux)
  - MSVC 19.29+ (Windows)
- Git (for fetching dependencies)

## Building

### Primary Method: mise

```bash
mise install           # Install cmake + ninja + ccache
mise run build         # Configure and build (Debug)
mise run build:release # Configure and build (Release)
mise run clean         # Remove build artifacts
```

### Alternative: CMake Presets

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
```

## mise Task Inventory

All tasks are defined in `mise.toml`. Each task has a `run` (POSIX shell) and `run_windows` (PowerShell) variant; mise routes to the correct one based on platform.

### Build

| Task | Alias | Description |
|------|-------|-------------|
| `build` | `b` | Configure and build (Debug, `dev-debug` preset) |
| `build:release` | `br` | Configure and build (Release, `dev-release` preset) |
| `clean` | | Remove build artifacts |

### Lint and Format

| Task | Alias | Description |
|------|-------|-------------|
| `format` | `fmt` | Check clang-format on source files |
| `format:fix` | | Auto-format source files in place |
| `lint` | | Run clang-tidy on all source files (slow) |
| `lint:changed` | | Run clang-tidy on git-dirty files only (fast) |
| `lint:fix` | `fix` | Run clang-tidy with auto-fix on changed files |
| `cppcheck` | | Run cppcheck static analysis |

### Test

| Task | Alias | Description |
|------|-------|-------------|
| `test` | `t` | Run unit tests (depends on build, with timeout) |
| `test:e2e` | | Run E2E tests |
| `test:all` | | Run unit and E2E tests |
| `test:filter` | `tf` | Run unit tests with gtest filter argument |

### Profiling (Tracy)

| Task | Alias | Description |
|------|-------|-------------|
| `profile` | `prof` | Build with Tracy profiler (Debug) |
| `profile:release` | `profr` | Build with Tracy profiler (RelWithDebInfo) |
| `profile:capture` | `cap` | Launch Recurse and capture a Tracy trace (30s) |
| `profile:capture-release` | `capr` | Launch Release build and capture a Tracy trace (30s) |
| `profile:view` | | Open the latest .tracy capture in Tracy Profiler |
| `profile:csv` | | Export the latest .tracy capture to CSV |

### Analysis

| Task | Alias | Description |
|------|-------|-------------|
| `sanitize` | | Build and test with ASan + UBSan (`ci-sanitize` preset) |
| `sanitize:tsan` | | Build and test with ThreadSanitizer (`ci-tsan` preset) |
| `coverage` | | Build with coverage, generate lcov report |
| `codeql` | | Run CodeQL security analysis locally |

## CMake Presets

`CMakePresets.json` defines 12 configure presets (1 hidden base, 11 visible), 8 build presets, and 5 test presets. All presets use Ninja as the generator. Build output goes to `build/<preset-name>/`.

### Configure Presets

| Preset | Type | Build Type | Notes |
|--------|------|------------|-------|
| `dev-debug` | Development | Debug | Tests enabled |
| `dev-release` | Development | Release | Tests disabled |
| `dev-profile-debug` | Development | Debug | Tracy ON, tests disabled |
| `dev-profile-release` | Development | RelWithDebInfo | Tracy ON, tests disabled |
| `dev-windows` | Development | Debug | Windows only, tests enabled |
| `ci-linux-gcc` | CI | Release | Linux only, GCC |
| `ci-linux-clang` | CI | Release | Linux only, Clang (used for clang-tidy) |
| `ci-macos` | CI | Release | macOS only, Apple Clang |
| `ci-windows` | CI | Release | Windows only, MSVC/Ninja |
| `ci-sanitize` | CI | Debug | ASan + UBSan, Clang, non-Windows |
| `ci-tsan` | CI | Debug | ThreadSanitizer, Clang, non-Windows |
| `ci-coverage` | CI | Debug | llvm-cov source-based coverage, Clang, non-Windows |

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `FABRIC_BUILD_TESTS` | `ON` | Build UnitTests and E2ETests executables |
| `FABRIC_USE_WEBVIEW` | `ON` | Enable WebView support; defines `FABRIC_USE_WEBVIEW` preprocessor symbol |
| `FABRIC_BUILD_UNIVERSAL` | `OFF` | Build universal binaries (arm64 + x86_64), macOS only |
| `FABRIC_USE_MIMALLOC` | `OFF` | Link mimalloc as global allocator override; OFF by default (MI_OVERRIDE conflicts with MoltenVK on macOS) |
| `FABRIC_ENABLE_PROFILING` | `OFF` | Enable Tracy profiler instrumentation; defines `FABRIC_PROFILING_ENABLED` |

## Dependency Management

All 20 dependencies are fetched automatically via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) v0.42.1. No manual installation is required. Each library has a dedicated CMake module in `cmake/modules/`.

### CPM_SOURCE_CACHE

Set `CPM_SOURCE_CACHE=~/.cache/CPM` (configured in `mise.toml`) to share downloaded sources across builds and avoid redundant fetches. On Windows, `mise.windows.toml` uses a Tera template (`{{ env.USERPROFILE }}/.cache/CPM`) because tilde expansion is not available.

### ccache

The build uses `ccache` when available for compiler output caching. `mise.toml` sets `CMAKE_C_COMPILER_LAUNCHER=ccache` and `CMAKE_CXX_COMPILER_LAUNCHER=ccache`.

### Dependency Table

| Dependency | Version | Type | Purpose |
|------------|---------|------|---------|
| [bgfx](https://github.com/bkaradzic/bgfx.cmake) | 1.139.9155-513 | Static | Vulkan rendering (SPIR-V shaders, MoltenVK on macOS) |
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.2 | Static | Windowing, input, timers, native handles for bgfx |
| [Flecs](https://github.com/SanderMertens/flecs) | 4.1.4 | Static | Entity Component System (archetype SoA, query caching) |
| [RmlUi](https://github.com/mikke89/RmlUi) | 6.2 | Static | In-game UI (HTML/CSS layout, bgfx RenderInterface) |
| [FreeType](https://github.com/freetype/freetype) | 2.14.1 | Static (fallback) | Font rendering (fetched from source when system package not found) |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | 5.5.0 | Static | Rigid body dynamics, collision shapes, ragdoll |
| [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP) | 4.8.4 | Static | XML behavior trees, action/condition nodes |
| [ozz-animation](https://github.com/guillaumeblanc/ozz-animation) | 0.16.0 | Static | SoA SIMD skeleton animation (sampling, blending, LocalToModel) |
| [fastgltf](https://github.com/spnda/fastgltf) | 0.9.0 | Static | glTF 2.0 parser, C++20 |
| [FastNoise2](https://github.com/Auburn/FastNoise2) | 1.1.1 | Static | SIMD-accelerated noise (FBm, Cellular, DomainWarp) |
| [efsw](https://github.com/SpartanJ/efsw) | 1.5.1 | Static | Cross-platform file system watcher for hot-reload |
| [webview](https://github.com/webview/webview) | 0.12.0 | Static | Embedded browser view, JS bridge |
| [Quill](https://github.com/odygrd/quill) | 11.0.2 | Static | Async structured logging (bundles fmtlib v12.1.0 as fmtquill) |
| [miniaudio](https://github.com/mackron/miniaudio) | 0.11.22 | Header only | Cross-platform audio, spatial 3D |
| [GLM](https://github.com/g-truc/glm) | 1.0.3 | Header only | OpenGL Mathematics (vectors, matrices, quaternions) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | Header only | JSON serialization for spatial types |
| [Standalone Asio](https://github.com/chriskohlhoff/asio) | 1.36.0 | Header only | Async I/O with C++20 coroutines (standalone, no Boost) |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | Header only | TOML v1.0 parser for configuration and data files |
| [Tracy](https://github.com/wolfpld/tracy) | 0.13.1 | Static (optional) | Frame profiler; only fetched when `FABRIC_ENABLE_PROFILING=ON` |
| [mimalloc](https://github.com/microsoft/mimalloc) | 2.2.7 | Static (optional) | Allocator override; only fetched when `FABRIC_USE_MIMALLOC=ON` |
| [GoogleTest](https://github.com/google/googletest) | 1.17.0 | Static (dev) | Testing framework (GTest + GMock) |

### Dependency Notes

- **mimalloc**: MI_OVERRIDE is OFF by default because the global malloc replacement conflicts with dlopen'd MoltenVK on macOS. Metal's internal allocators bypass mimalloc's malloc zone, causing crashes in `mi_free_size`. The override object (`MimallocOverride.cc`) links only into the Recurse executable, not into test targets.
- **Tracy**: Conditionally fetched. When `FABRIC_ENABLE_PROFILING` is `OFF` (the default), all `FABRIC_ZONE_*` and `FABRIC_FRAME_*` macros expand to nothing.
- **Asio**: Standalone mode (`ASIO_STANDALONE`) with C++20 coroutine support (`ASIO_HAS_CO_AWAIT`, `ASIO_HAS_STD_COROUTINE`).
- **GLM**: Configured with `GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_FORCE_SILENT_WARNINGS`.
- **FreeType**: Fetched from source only when the system package is not found. macOS and most Linux distributions use the system FreeType.
- **Jolt**: `CPP_RTTI_ENABLED=ON` is required. Fabric subclasses Jolt types (`BroadPhaseLayerInterface`, `ContactListener`); Linux ld rejects missing typeinfo without RTTI.

### CPM Patches

The `cmake/patches/` directory holds vendored patches applied via the CPM `PATCHES` argument during fetch.

| Patch File | Target | Description |
|------------|--------|-------------|
| `bgfx-vk-suboptimal.patch` | bgfx | Treats `VK_SUBOPTIMAL_KHR` as non-fatal; prevents crashes on window resize with MoltenVK |

Patches persist across builds (sources stay modified on disk) and reapply on version bumps. Existing CPM caches populated before `PATCHES` was added require manual cache clearing for first application.

To add a new patch: place the `.patch` file in `cmake/patches/`, then add `PATCHES "${CMAKE_CURRENT_LIST_DIR}/../patches/<file>.patch"` to the corresponding `CPMAddPackage()` call.

## Build Targets

| Target | Type | Links | Description |
|--------|------|-------|-------------|
| `FabricLib` | Static library | SDL3, bgfx/bx/bimg, webview, GLM, Quill, nlohmann/json, Asio, RmlUi, Flecs, FastNoise2, fastgltf, ozz-animation, Jolt, BehaviorTree.CPP, miniaudio, toml++, efsw, Tracy (optional) | All engine and game sources |
| `Recurse` | Executable | FabricLib, mimalloc (optional) | main() entry point via Recurse.cc |
| `UnitTests` | Executable | FabricLib, GTest, GMock, ozz_animation_offline | Unit test runner with custom TestMain |
| `E2ETests` | Executable | FabricLib, GTest, GMock, ozz_animation_offline | End-to-end test runner with custom TestMain |

### Shader Compilation

Eight CMake modules compile `.sc` shader sources to embedded `.bin.h` headers using bgfx `shaderc`. All shaders compile to SPIR-V only (platform argument: `linux`, profile: `spirv`). Output headers use the `BGFX_EMBEDDED_SHADER` macro.

| Module | Shaders | Purpose |
|--------|---------|---------|
| FabricRmlUi (shaders) | vs_rmlui, fs_rmlui | UI overlay rendering |
| FabricSkinnedShaders | vs_skinned, fs_skinned | GPU skeletal mesh skinning |
| FabricVoxelShaders | vs_voxel, fs_voxel | Voxel chunk terrain |
| FabricSkyShaders | vs_sky, fs_sky | Procedural sky |
| FabricPostShaders | vs_fullscreen, fs_bright, fs_blur, fs_tonemap | Bloom pipeline |
| FabricParticleShaders | vs_particle, fs_particle | Particle rendering |
| FabricWaterShaders | vs_water, fs_water | Water surface |
| FabricOITShaders | vs_oit_accum, fs_oit_accum, vs_oit_composite, fs_oit_composite | Order-independent transparency |

Shader source files live in `shaders/<category>/` with a `varying.def.sc` per category. FabricLib depends on all shader targets so compilation finishes before source files compile.

### Generated Files

`cmake/Constants.g.hh.in` is processed at configure time to produce `${CMAKE_BINARY_DIR}/include/fabric/core/Constants.g.hh`, containing `APP_NAME` and `APP_VERSION` from the CMake project definition. The build directory include path takes precedence over the source tree copy.

## Platform Requirements

### macOS

- macOS 15.0 or later
- Xcode Command Line Tools (`xcode-select --install`)
- Frameworks (included with macOS): Cocoa, WebKit, Metal, QuartzCore
- Vulkan via MoltenVK: `brew install molten-vk vulkan-loader`

When using Homebrew LLVM instead of Apple Clang, the build system automatically links against the bundled libc++ to resolve ABI differences.

### Linux

```bash
# Ubuntu/Debian
sudo apt install build-essential libwebkit2gtk-4.1-dev

# Fedora
sudo dnf install gcc-c++ webkit2gtk4.1-devel

# Arch
sudo pacman -S base-devel webkit2gtk
```

The build system tries `webkit2gtk-4.1` first, then falls back to `webkit2gtk-4.0`.

Install the Vulkan SDK and GPU-specific drivers for your distribution.

### Windows

- Windows 10 or later
- Visual Studio 2019+ with C++ Desktop Development workload
- Windows 10 SDK (10.0.19041.0 or later)
- LunarG Vulkan SDK

#### Windows Build

Requires: Visual Studio 2022 with C++ Desktop workload, Developer PowerShell for VS.

```powershell
# Set env before first use
$env:MISE_ENV = "windows"

# Build and test (same commands; mise routes to .ps1 via run_windows)
mise run build
mise run test
mise run test:all
```

`mise.windows.toml` overrides the base environment: `CC=cl`, `CXX=cl`, clears Homebrew paths/flags, and uses `{{ env.USERPROFILE }}/.cache/CPM` for the CPM cache. If CMake caches stale values after environment changes, clean the build directory with `mise run clean`.

## Platform Contracts

The Vulkan-only rendering backend imposes constraints that the engine must satisfy at runtime.

| Contract | Requirement | Consequence of Violation |
|----------|-------------|--------------------------|
| SDL window flags | `SDL_WINDOW_VULKAN` on SDL3 window creation | bgfx cannot create a Vulkan surface |
| HiDPI reset flag | `BGFX_RESET_HIDPI` on both `bgfx::init()` and every `bgfx::reset()` | Half-resolution rendering on Retina/HiDPI |
| Single-threaded init | `bgfx::renderFrame()` before `bgfx::init()` on macOS | Render thread deadlock with Metal/MoltenVK |
| MoltenVK runtime | macOS: `brew install molten-vk vulkan-loader`; BUILD_RPATH set to `/opt/homebrew/lib` | Vulkan loader not found at runtime |
| VK_SUBOPTIMAL_KHR | Patched via CPM PATCHES (`bgfx-vk-suboptimal.patch`) | Window resize or display switch crashes |
