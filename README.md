# Fabric Engine

C++20 cross-platform runtime for building interactive spatial-temporal applications.

> **Work in Progress.** The engine is functional but not production-ready. APIs may change.

## Systems

- **Component**: type-safe component architecture with variant-based properties
- **Event**: thread-safe event handling with propagation and typed data
- **Lifecycle**: validated state machine transitions for component lifecycle
- **Plugin**: dependency-aware plugin system with resource management
- **ResourceHub**: centralized resource loading with dependency tracking
- **Spatial**: type-safe vector operations with coordinate space transformations (GLM bridge)
- **Temporal**: multi-timeline time processing with variable time flow
- **Command**: execute/undo/redo with composite commands and command history
- **Async**: Asio io_context scaffolding for coroutine-based async I/O
- **JsonTypes**: nlohmann/json serializers for core math types
- **ArgumentParser**: builder-pattern CLI argument parser with validation
- **SyntaxTree/Token**: AST and tokenizer for config and data parsing
- **WebView**: embedded browser with JavaScript bridge (optional, via `FABRIC_USE_WEBVIEW`)

## Building

### Prerequisites

- [mise](https://mise.jdx.dev/) (manages cmake + ninja)
- C++20 compatible compiler (Apple Clang, GCC 10+, Clang 13+, MSVC 19.29+)

### Build and Test

```bash
mise install            # Install tooling
mise run build          # Debug build
mise run test           # Unit tests
```

Or with CMake presets:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
```

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| SDL3 | 3.4.2 | Windowing, input, audio, timers |
| webview | 0.12.0 | Embedded browser, JS bridge |
| GoogleTest | 1.17.0 | Unit and E2E testing |
| GLM | 1.0.3 | Matrix math (inverse, decomposition) |
| mimalloc | 2.2.7 | Global memory allocator (Fabric exe) |
| Quill | 11.0.2 | Async structured logging |
| nlohmann/json | 3.12.0 | JSON serialization for core types |
| Tracy | 0.13.1 | Frame profiling (optional) |
| Standalone Asio | 1.36.0 | Async I/O, C++20 coroutines |
| fmtquill | 12.1.0 | Format library (bundled via Quill) |

All dependencies are fetched via CMake `FetchContent`. Each library has a dedicated module under `cmake/modules/`.

## Platform Support

| Platform | Minimum | Notes |
|----------|---------|-------|
| macOS | 14.0+ | Xcode CLT, Cocoa + WebKit frameworks |
| Linux | Recent kernel | webkit2gtk-4.1 (or 4.0 fallback) |
| Windows | 10+ | MSVC, Windows 10 SDK |

## Project Structure

```
fabric/
├── include/fabric/
│   ├── core/           # Component, Command, Event, Lifecycle, Plugin,
│   │                   # Resource, ResourceHub, Temporal, Spatial, Types,
│   │                   # Log, Async, JsonTypes, Constants.g
│   ├── parser/         # ArgumentParser, SyntaxTree, Token
│   ├── ui/             # WebView
│   └── utils/          # CoordinatedGraph, ThreadPoolExecutor,
│                       # TimeoutLock, Profiler, ErrorHandling, Testing, Utils
├── src/                # Implementation files (.cc)
├── tests/
│   ├── unit/           # Per-component unit tests (17 files, 164 tests)
│   └── e2e/            # End-to-end tests
├── cmake/modules/      # 7 FetchContent modules
├── tasks/              # POSIX shell scripts for mise
├── CMakeLists.txt      # Build config (FabricLib static library)
├── CMakePresets.json    # 7 presets (dev + CI)
└── mise.toml           # Task runner config
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Build Guide](docs/BUILD.md)
- [Testing Guide](docs/TESTING.md)
- [Contributing](CONTRIBUTING.md)

## License

[MIT License](LICENSE)
