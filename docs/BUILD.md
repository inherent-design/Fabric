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

The project includes `CMakePresets.json` with 7 presets (1 hidden base, 6 visible):

| Preset | Type | Notes |
|--------|------|-------|
| `dev-debug` | Development | Debug build, tests enabled |
| `dev-release` | Development | Release build, tests disabled |
| `ci-linux-gcc` | CI | Linux only, GCC |
| `ci-linux-clang` | CI | Linux only, Clang |
| `ci-macos` | CI | macOS only, Apple Clang |
| `ci-windows` | CI | Windows only, MSVC/Ninja |

All presets use Ninja as the generator. Build output goes to `build/<preset-name>/`.

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `FABRIC_BUILD_TESTS` | `ON` | Build test executables (UnitTests, E2ETests) |
| `FABRIC_USE_WEBVIEW` | `ON` | Enable WebView support; defines `FABRIC_USE_WEBVIEW` preprocessor symbol |
| `FABRIC_BUILD_UNIVERSAL` | `OFF` | Build universal binaries (arm64 + x86_64), macOS only |
| `FABRIC_ENABLE_PROFILING` | `OFF` | Enable Tracy profiler instrumentation; defines `FABRIC_PROFILING_ENABLED` |

## Platform requirements

### macOS

- macOS 14.0 or later
- Xcode Command Line Tools (`xcode-select --install`)
- Frameworks (included with macOS): Cocoa, WebKit

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

### Windows

- Windows 10 or later
- Visual Studio 2019+ with C++ Desktop Development workload
- Windows 10 SDK (10.0.19041.0 or later)

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

## Dependencies

All dependencies are fetched automatically via CMake `FetchContent`. No manual installation is required.

| Dependency | Version | Type | Purpose |
|------------|---------|------|---------|
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.2 | Static | Windowing, input, audio, timers |
| [webview](https://github.com/webview/webview) | 0.12.0 | Static | Embedded browser view, JS bridge |
| [GoogleTest](https://github.com/google/googletest) | 1.17.0 | Static | Testing framework (GTest + GMock) |
| [GLM](https://github.com/g-truc/glm) | 1.0.3 | Header only | OpenGL Mathematics (vectors, matrices, quaternions) |
| [mimalloc](https://github.com/microsoft/mimalloc) | 2.2.7 | Static | Global allocator replacement via `malloc` override |
| [Quill](https://github.com/odygrd/quill) | 11.0.2 | Static | Async structured logging backend |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | Header only | JSON serialization and deserialization |
| [Tracy](https://github.com/wolfpld/tracy) | 0.13.1 | Static | Frame profiler; only fetched when `FABRIC_ENABLE_PROFILING=ON` |
| [Asio](https://github.com/chriskohlhoff/asio) | 1.36.0 | Header only | Async I/O with C++20 coroutine support (standalone, no Boost) |

### Dependency notes

- **mimalloc** overrides the standard `malloc` interface at link time. The override object (`MimallocOverride.cc`) is linked only into the `Fabric` executable, not into test targets.
- **Tracy** is conditionally fetched. When `FABRIC_ENABLE_PROFILING` is `OFF` (the default), no Tracy code is compiled or linked; all `FABRIC_ZONE_*` / `FABRIC_FRAME_*` / `FABRIC_ALLOC` macros expand to nothing.
- **Asio** is configured in standalone mode (`ASIO_STANDALONE`) with C++20 coroutine support (`ASIO_HAS_CO_AWAIT`, `ASIO_HAS_STD_COROUTINE`).
- **GLM** is configured with `GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`, and `GLM_FORCE_SILENT_WARNINGS`.

## Build targets

| Target | Type | Links | Description |
|--------|------|-------|-------------|
| `FabricLib` | Static library | SDL3, webview, GLM, Quill, nlohmann/json, Asio, Tracy (optional) | All core, utils, parser, and UI sources |
| `Fabric` | Executable | FabricLib, mimalloc | Main application entry point |
| `UnitTests` | Executable | FabricLib, GTest, GMock | Unit test runner with custom TestMain |
| `E2ETests` | Executable | FabricLib, GTest, GMock | End to end test runner with custom TestMain |

## Generated files

`cmake/Constants.g.hh.in` is processed at configure time to produce `${CMAKE_BINARY_DIR}/include/fabric/core/Constants.g.hh`, which contains `APP_NAME` and `APP_VERSION` from the CMake project definition.
