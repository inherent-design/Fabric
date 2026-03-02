# Build Guide

## Prerequisites

- [mise](https://mise.jdx.dev/): manages cmake and ninja automatically via `mise install`
- C++20 compiler:
  - Apple Clang (macOS, via Xcode Command Line Tools)
  - GCC 10+ (Linux)
  - Clang 13+ (Linux)
  - MSVC 19.29+ (Windows)
- Git (for fetching dependencies)

## Building

### Primary method: mise

```bash
mise install           # Install cmake + ninja
mise run build         # Configure and build (Debug)
mise run build:release # Configure and build (Release)
mise run clean         # Remove build artifacts
```

### Alternative: CMake presets

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
```

### CMake presets

The project includes `CMakePresets.json` with 11 configure presets (1 hidden base, 10 visible):

| Preset | Type | Notes |
|--------|------|-------|
| `dev-debug` | Development | Debug build, tests enabled |
| `dev-release` | Development | Release build, tests disabled |
| `dev-windows` | Development | Windows Debug build, tests enabled |
| `ci-linux-gcc` | CI | Linux only, GCC |
| `ci-linux-clang` | CI | Linux only, Clang (used for clang-tidy) |
| `ci-macos` | CI | macOS only, Apple Clang |
| `ci-windows` | CI | Windows only, MSVC/Ninja |
| `ci-sanitize` | CI | ASan + UBSan (non-Windows, Clang) |
| `ci-tsan` | CI | ThreadSanitizer (non-Windows, Clang) |
| `ci-coverage` | CI | Source-based coverage via llvm-cov (non-Windows, Clang) |

All presets use Ninja as the generator. Build output goes to `build/<preset-name>/`.

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `FABRIC_BUILD_TESTS` | `ON` | Build test executables (UnitTests, E2ETests) |
| `FABRIC_USE_WEBVIEW` | `ON` | Enable WebView support; defines `FABRIC_USE_WEBVIEW` preprocessor symbol |
| `FABRIC_BUILD_UNIVERSAL` | `OFF` | Build universal binaries (arm64 + x86_64), macOS only |
| `FABRIC_USE_MIMALLOC` | `OFF` | Link mimalloc as global allocator override into Fabric executable; OFF by default because MI_OVERRIDE conflicts with dlopen'd drivers (MoltenVK, Mesa) |
| `FABRIC_ENABLE_PROFILING` | `OFF` | Enable Tracy profiler instrumentation; defines `FABRIC_PROFILING_ENABLED` |

## Platform Requirements

### macOS

- macOS 15.0 or later
- Xcode Command Line Tools (`xcode-select --install`)
- Frameworks (included with macOS): Cocoa, WebKit
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

## Platform Contracts

The Vulkan-only rendering backend imposes several constraints that the engine must satisfy at runtime.

| Contract | Requirement | Consequence of violation |
|----------|-------------|------------------------|
| SDL window flags | `SDL_WINDOW_VULKAN` must be set on SDL3 window creation | bgfx fails to create a Vulkan surface |
| HiDPI reset flag | `BGFX_RESET_HIDPI` must be passed to both `bgfx::init()` and every `bgfx::reset()` call | Rendering at half resolution on Retina/HiDPI displays |
| Single-threaded init | `bgfx::renderFrame()` must be called before `bgfx::init()` on macOS | bgfx spawns a render thread that deadlocks with the Metal/MoltenVK driver |
| MoltenVK runtime | macOS requires `brew install molten-vk vulkan-loader`; `BUILD_RPATH` is set to `/opt/homebrew/lib` | Vulkan loader not found at runtime (`dlopen` fails) |
| VK_SUBOPTIMAL_KHR | bgfx treats suboptimal swapchains as fatal errors; patched via CPM PATCHES (`cmake/patches/bgfx-vk-suboptimal.patch`) | Window resize or display switch crashes the application |

## Testing

```bash
mise run test                    # Unit tests (with timeout)
mise run test:e2e                # E2E tests
mise run test:filter SpatialTest # Run specific test by name
```

Or run test binaries directly from the build directory:

```bash
./build/bin/UnitTests
./build/bin/E2ETests
./build/bin/UnitTests --gtest_filter=SpatialTest*
```

Both test executables use a custom `tests/TestMain.cc` that initializes and shuts down the Quill logging subsystem around the GoogleTest runner.

See [Testing Guide](TESTING.md) for test conventions and patterns.

## Analysis

```bash
mise run sanitize           # Build and test with ASan + UBSan
mise run sanitize:tsan      # Build and test with ThreadSanitizer
mise run coverage           # Build with coverage, generate lcov report
mise run codeql             # Run CodeQL security analysis locally
```

Sanitizer and coverage presets use Clang with the corresponding compiler flags. Coverage generates an lcov report at `build/ci-coverage/coverage.lcov`.

## Dependencies

All dependencies are fetched automatically via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) v0.42.1. No manual installation is required. Set `CPM_SOURCE_CACHE=~/.cache/CPM` (configured in `mise.toml`) to share downloaded sources across builds and avoid redundant fetches. The build also uses `ccache` when available for compiler output caching.

| Dependency | Version | Type | Purpose |
|------------|---------|------|---------|
| [bgfx](https://github.com/bkaradzic/bgfx.cmake) | 1.139.9155-513 | Static | Vulkan rendering (SPIR-V shaders, MoltenVK on macOS) |
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.2 | Static | Windowing, input, timers, native handles for bgfx |
| [Flecs](https://github.com/SanderMertens/flecs) | 4.1.4 | Static | Entity Component System (archetype SoA, query caching) |
| [RmlUi](https://github.com/mikke89/RmlUi) | 6.2 | Static | In-game UI (HTML/CSS layout, bgfx RenderInterface) |
| [FreeType](https://github.com/freetype/freetype) | 2.14.1 | Static | Font rendering (fetched by RmlUi if system package not found) |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | 5.5.0 | Static | Rigid body dynamics, collision shapes |
| [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP) | 4.8.4 | Static | XML behavior trees, action/condition nodes |
| [miniaudio](https://github.com/mackron/miniaudio) | 0.11.22 | Header only | Cross-platform audio, spatial 3D |
| [FastNoise2](https://github.com/Auburn/FastNoise2) | 1.1.1 | Static | SIMD-accelerated noise (FBm, Cellular, DomainWarp) |
| [fastgltf](https://github.com/spnda/fastgltf) | 0.9.0 | Static | High-performance glTF 2.0 parser, C++20 |
| [ozz-animation](https://github.com/guillaumeblanc/ozz-animation) | 0.16.0 | Static | SoA SIMD skeleton animation (sampling, blending) |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | Header only | TOML v1.0 parser for configuration files |
| [efsw](https://github.com/SpartanJ/efsw) | 1.5.1 | Static | Cross-platform file system watcher |
| [webview](https://github.com/webview/webview) | 0.12.0 | Static | Embedded browser view, JS bridge |
| [GoogleTest](https://github.com/google/googletest) | 1.17.0 | Static | Testing framework (GTest + GMock) |
| [GLM](https://github.com/g-truc/glm) | 1.0.3 | Header only | OpenGL Mathematics (vectors, matrices, quaternions) |
| [mimalloc](https://github.com/microsoft/mimalloc) | 2.2.7 | Static | Allocator override (only fetched when `FABRIC_USE_MIMALLOC=ON`) |
| [Quill](https://github.com/odygrd/quill) | 11.0.2 | Static | Async structured logging (bundles fmtlib v12.1.0 as fmtquill) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | Header only | JSON serialization and deserialization |
| [Tracy](https://github.com/wolfpld/tracy) | 0.13.1 | Static | Frame profiler (only fetched when `FABRIC_ENABLE_PROFILING=ON`) |
| [Asio](https://github.com/chriskohlhoff/asio) | 1.36.0 | Header only | Async I/O with C++20 coroutine support (standalone, no Boost) |

### Dependency notes

- **mimalloc** overrides the standard `malloc` interface at link time. The override object (`MimallocOverride.cc`) is linked only into the `Fabric` executable, not into test targets. The option is OFF by default because MI_OVERRIDE conflicts with dlopen'd drivers like MoltenVK.
- **Tracy** is conditionally fetched. When `FABRIC_ENABLE_PROFILING` is `OFF` (the default), no Tracy code is compiled or linked; all `FABRIC_ZONE_*` / `FABRIC_FRAME_*` / `FABRIC_ALLOC` macros expand to nothing.
- **Asio** is configured in standalone mode (`ASIO_STANDALONE`) with C++20 coroutine support (`ASIO_HAS_CO_AWAIT`, `ASIO_HAS_STD_COROUTINE`).
- **GLM** is configured with `GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`, and `GLM_FORCE_SILENT_WARNINGS`.
- **FreeType** is fetched from source only when the system package is not found. On macOS and most Linux distributions, the system FreeType is used.

### CPM PATCHES

Some upstream dependencies contain bugs that affect Fabric. The `cmake/patches/` directory holds vendored patches applied via the CPM `PATCHES` argument during fetch.

| Patch file | Target | Description |
|------------|--------|-------------|
| `bgfx-vk-suboptimal.patch` | bgfx | Treats `VK_SUBOPTIMAL_KHR` as non-fatal; prevents crashes on window resize with MoltenVK |

To add a new patch: place the `.patch` file in `cmake/patches/`, then add `PATCHES "${CMAKE_CURRENT_LIST_DIR}/../patches/<file>.patch"` to the corresponding `CPMAddPackage()` call.

## Build targets

| Target | Type | Links | Description |
|--------|------|-------|-------------|
| `FabricLib` | Static library | SDL3, webview, bgfx/bx/bimg, GLM, Quill, nlohmann/json, Asio, RmlUi, Flecs, FastNoise2, fastgltf, ozz-animation, Jolt, BehaviorTree.CPP, miniaudio, toml++, efsw, Tracy (optional) | All core, utils, parser, codec, and UI sources |
| `Fabric` | Executable | FabricLib, mimalloc (optional) | Main application entry point |
| `UnitTests` | Executable | FabricLib, GTest, GMock | Unit test runner with custom TestMain |
| `E2ETests` | Executable | FabricLib, GTest, GMock | End to end test runner with custom TestMain |

## Generated files

`cmake/Constants.g.hh.in` is processed at configure time to produce `${CMAKE_BINARY_DIR}/include/fabric/core/Constants.g.hh`, which contains `APP_NAME` and `APP_VERSION` from the CMake project definition.
