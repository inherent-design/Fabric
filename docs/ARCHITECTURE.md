# Fabric Engine Architecture

## Overview

Fabric is a C++20 cross-platform runtime for building interactive spatial-temporal applications. Programming primitives (math, time, events, commands) compose into structural primitives (scenes, entities, graphs, simulations) that applications use to create games, editors, simulations, and multimedia tools. All public symbols reside in the `fabric::` namespace, with sub-namespaces for utilities (`fabric::Utils`, `fabric::Testing`) and subsystems (`fabric::log`, `fabric::async`).

## Layer Architecture

```
L0: Platform Abstraction
    SDL3             Windowing, input, audio, timers, filesystem, native handles
    bgfx             Cross-platform rendering: Vulkan/Metal/D3D12/GL (11 backends)
    WebView          Embedded browser, JS bridge (optional, FABRIC_USE_WEBVIEW)
    mimalloc         Global allocator override (Fabric executable only)

L1: Infrastructure
    Log              Quill-backed async SPSC logging, FABRIC_LOG_* macros
    Profiler         Tracy abstraction, zero-cost when FABRIC_ENABLE_PROFILING is OFF
    Async            Standalone Asio io_context scaffold, C++20 coroutine support
    ErrorHandling    FabricException, throwError() utilities
    StateMachine     Generic state machine template with validation and observers

L2: Primitives
    Spatial          Type-safe vec/mat/quat/transform with compile-time coordinate space tags; GLM bridge for Matrix4x4::inverse()
    Temporal         Multi-timeline time processing, snapshots, variable flow
    Event            Typed thread-safe publish/subscribe with propagation control
    Command          Undo/redo command pattern with composite commands and history
    Pipeline         Typed multi-stage data processing pipelines
    Types            Core Variant, StringMap, Optional type aliases
    JsonTypes        nlohmann/json ADL serializers for Vector, Quaternion types

L2.5: Codec
    Codec            Encode/decode pipeline for binary, text, and structured data formats

L3: Structural
    Component        Type-safe component architecture with variant properties and hierarchy
    Resource         Resource base with state machine, dependency tracking, priority loading
    ResourceHub      Centralized resource management, worker threads, memory budgets
    Lifecycle        Validated state machine transitions (Created, Initialized, Rendered, Updating, Suspended, Destroyed)
    CoordinatedGraph Thread-safe DAG with intent-based locking, deadlock detection, resource lock ordering
    ImmutableDAG     Lock-free persistent DAG with structural sharing and snapshot isolation
    BufferPool       Fixed-size buffer pool with thread-safe allocation and RAII handles

L4: Framework
    Plugin           Dependency-aware plugin loading with resource management
    ArgumentParser   Builder-pattern CLI argument parser with validation
    SyntaxTree       AST for config and data file parsing
    Token            Tokenizer with extensible type system

L5: Application
    Fabric           Main executable entry point
    FabricDemo       Interactive demo (future target)
```

## Component Inventory

### Core (`include/fabric/core/`)

| Header | Purpose |
|--------|---------|
| `Async.hh` | Standalone Asio io_context scaffold; provides `fabric::async::init()`, `poll()`, `run()`, `shutdown()`, `makeStrand()`, `makeTimer()`, and `use_nothrow` completion token for C++20 coroutines |
| `Command.hh` | Execute/undo/redo command pattern with composite commands and history |
| `Component.hh` | Base component class with variant-based property storage, lifecycle methods, child management |
| `Constants.g.hh` | Generated constants (APP_NAME, APP_VERSION); output to build dir from `cmake/Constants.g.hh.in` |
| `Event.hh` | Thread-safe typed event handling with priority-sorted handlers, cancellation semantics, propagation control, and std::any payloads |
| `JsonTypes.hh` | ADL-visible `to_json`/`from_json` for Vector2, Vector3, Vector4, Quaternion via nlohmann/json |
| `Lifecycle.hh` | State machine for component lifecycle (Created, Initialized, Rendered, Updating, Suspended, Destroyed) |
| `Log.hh` | Quill v11 wrapper; `fabric::log::init()`, `shutdown()`, `setLevel()`; FABRIC_LOG_{TRACE,DEBUG,INFO,WARN,ERROR,CRITICAL} macros with compile-time filtering |
| `Pipeline.hh` | Typed multi-stage data processing pipeline with fan-out, conditional stages, and error handling |
| `Plugin.hh` | Dependency-aware plugin loading with resource management |
| `StateMachine.hh` | Generic state machine template with transition validation, guards, entry/exit actions, and observers |
| `Resource.hh` | Resource base with state machine (Unloaded, Loading, Loaded, LoadingFailed, Unloading), dependency tracking, priority levels |
| `ResourceHub.hh` | Centralized resource management with CoordinatedGraph-backed dependency tracking, worker threads, memory budgets |
| `Spatial.hh` | Type-safe Vector2/3/4, Quaternion, Matrix4x4, Transform, SceneNode, Scene; compile-time coordinate space tags (Local, World, Screen, Parent); GLM bridge for `inverse()` |
| `Temporal.hh` | Multi-timeline time processing with snapshots, variable time flow, region support |
| `Types.hh` | Core Variant (`nullptr_t, bool, int, float, double, string`), StringMap, Optional aliases |

### Parser (`include/fabric/parser/`)

| Header | Purpose |
|--------|---------|
| `ArgumentParser.hh` | Builder-pattern CLI argument parser with validation |
| `SyntaxTree.hh` | AST for config and data file parsing |
| `Token.hh` | Tokenizer with extensible type system |

### UI (`include/fabric/ui/`)

| Header | Purpose |
|--------|---------|
| `WebView.hh` | Embedded browser with JavaScript bridge (optional, via `FABRIC_USE_WEBVIEW`) |

### Codec (`include/fabric/codec/`)

| Header | Purpose |
|--------|---------|
| `Codec.hh` | Encode/decode pipeline for binary, text, and structured data formats with codec registry and chaining |

### Utils (`include/fabric/utils/`)

| Header | Purpose |
|--------|---------|
| `BufferPool.hh` | Fixed-size buffer pool with thread-safe allocation, RAII handles, and configurable block sizes |
| `CoordinatedGraph.hh` | Thread-safe DAG with intent-based locking (Read, NodeModify, GraphStructure), node-level concurrency, deadlock detection, resource lock ordering, BFS/DFS/topological sort |
| `ErrorHandling.hh` | FabricException class, `throwError()` utility, `ErrorCode` enum, and `Result<T>` template for hot-path error reporting |
| `ImmutableDAG.hh` | Lock-free persistent DAG with structural sharing, snapshot isolation, BFS/DFS/topological sort, and LCA queries |
| `Profiler.hh` | Tracy v0.13.1 abstraction; FABRIC_ZONE_*, FABRIC_FRAME_*, FABRIC_ALLOC/FREE, FABRIC_LOCKABLE macros; compiles to nothing when `FABRIC_ENABLE_PROFILING` is OFF |
| `Testing.hh` | MockComponent, test utilities, helpers for concurrent test scenarios |
| `ThreadPoolExecutor.hh` | Thread pool with task submission, timeout support, testing mode (synchronous execution) |
| `TimeoutLock.hh` | Timeout-protected lock acquisition for shared_mutex and mutex types |
| `Utils.hh` | `generateUniqueId()` with thread-safe random hex generation |

### Source-only Files

| File | Purpose |
|------|---------|
| `src/core/MimallocOverride.cc` | Forces linker to pull mimalloc malloc/free/new/delete overrides; compiled into Fabric executable only, not test targets |
| `src/core/Fabric.cc` | Main executable entry point |

## Dependencies

All dependencies are fetched via CMake `FetchContent`. Each has a dedicated module in `cmake/modules/`.

| Dependency | Version | Module | Purpose |
|------------|---------|--------|---------|
| SDL3 | 3.4.2 | (inline) | Windowing, input, audio, timers, filesystem, native handles |
| webview | 0.12.0 | (inline) | Embedded browser, JS bridge |
| GoogleTest | 1.17.0 | FabricGoogleTest | Unit and E2E testing |
| GLM | 1.0.3 | FabricGLM | Math bridge for Matrix4x4::inverse(); header-only |
| mimalloc | 2.2.7 | FabricMimalloc | Global allocator override (Fabric exe only, not test targets) |
| Quill | 11.0.2 | FabricQuill | Async SPSC structured logging with compile-time level filtering |
| nlohmann/json | 3.12.0 | FabricNlohmannJson | JSON serialization for spatial types |
| Tracy | 0.13.1 | FabricTracy | Frame/zone/lock/memory profiler; opt-in via `FABRIC_ENABLE_PROFILING` |
| Standalone Asio | 1.36.0 | FabricAsio | Async I/O with C++20 coroutines; io_context per-frame poll |
| bgfx | 1.139.9155 | FabricBgfx | Cross-platform rendering: Vulkan/Metal/D3D12/GL (11 backends) |

## Build System

CMake 3.25+ with target-based configuration throughout. FabricLib is a static library compiled once from all core, utils, parser, codec, and UI sources; all targets link against it. See [BUILD.md](BUILD.md) for build commands, presets, and tooling details.

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `FABRIC_BUILD_TESTS` | `ON` | Build UnitTests and E2ETests executables |
| `FABRIC_USE_WEBVIEW` | `ON` | Enable WebView support and link webview::core |
| `FABRIC_BUILD_UNIVERSAL` | `OFF` | Build universal (arm64+x86_64) binaries on macOS |
| `FABRIC_ENABLE_PROFILING` | `OFF` | Enable Tracy profiler instrumentation |

## Testing

GoogleTest 1.17.0 for all tests. Two test executables: UnitTests (per-component, `tests/unit/`) and E2ETests (application-level, `tests/e2e/`). Both use a custom `tests/TestMain.cc` that initializes Quill logging before test execution. 241 tests across 17 suites. See [TESTING.md](TESTING.md) for test conventions and running instructions.

## Platform Support

| Platform | Minimum | Compiler | Notes |
|----------|---------|----------|-------|
| macOS | 14.0+ | Apple Clang | Cocoa + WebKit frameworks |
| Linux | Recent kernel | GCC 10+ or Clang 13+ | webkit2gtk-4.1 (falls back to 4.0) |
| Windows | 10+ | MSVC 19.29+ | Windows 10 SDK, WebView2, shlwapi, version libs |

## Known Issues

1. **Temporal dead scaffolding**: approximately 70% of Temporal.hh is unused scaffolding. Core timeline and snapshot functionality works; the rest needs cleanup.
2. **Singletons**: Timeline, ResourceHub, and PluginManager are singletons. Planned migration to pass-by-reference through an application context object.
3. **mimalloc macOS zone interposition**: mimalloc's malloc zone replacement on macOS can conflict with AddressSanitizer and some debugging tools. The override is excluded from test targets to avoid interference.
