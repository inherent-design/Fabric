# Claude notes

## Build

Build and test via mise (preferred):

```bash
mise run build          # Debug build
mise run build:release  # Release build
mise run test           # Unit tests (with timeout)
mise run test:e2e       # E2E tests
mise run test:filter X  # Filter by test name
mise run lint           # clang-tidy
mise run lint:fix       # clang-tidy with auto-fix
```

Or with CMake presets:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
```

FabricLib is a static library that all targets link against. The `Fabric` executable additionally links mimalloc for global allocator override. Test executables do not link mimalloc.

## Tests

Both `UnitTests` and `E2ETests` use a custom `tests/TestMain.cc` that initializes Quill logging before GoogleTest runs. Do not add a separate `main()` in test files.

When writing ResourceHub tests, disable worker threads to prevent hangs:

```cpp
ResourceHub::instance().disableWorkerThreadsForTesting();
```

Or use `reset()` which disables workers, clears all resources, and resets the memory budget:

```cpp
ResourceHub::instance().reset();
```

For ResourceHub tests, prefer:
1. Using direct Resource objects rather than going through the hub
2. Using explicit timeouts on all locks to prevent deadlocks
3. Wrapping operations in try/catch to ensure cleanup on failure
4. Testing one thing at a time

## Code style

- `.hh` for headers, `.cc` for source files
- Namespace: `fabric::` (with sub-namespaces `fabric::log`, `fabric::async`, etc.)
- PascalCase for classes, camelCase for methods
- Use `std::unique_ptr` or `std::shared_ptr` for ownership management
- Prefer `const` where possible
- Clean code, minimal comments (only for complex or interesting logic)
- No em/en-dashes, no emojis, no double-hyphens except title cases

## Logging

Use `FABRIC_LOG_*` macros from `fabric/core/Log.hh`. These wrap Quill v11.x with the root logger. Format strings use `{}` placeholders (libfmt style).

```cpp
#include "fabric/core/Log.hh"

FABRIC_LOG_INFO("Server started on port {}", port);
FABRIC_LOG_ERROR("Failed to load resource: {}", resource_id);
FABRIC_LOG_DEBUG("Processing {} items", count);
```

Available macros: `FABRIC_LOG_TRACE`, `FABRIC_LOG_DEBUG`, `FABRIC_LOG_INFO`, `FABRIC_LOG_WARN`, `FABRIC_LOG_ERROR`, `FABRIC_LOG_CRITICAL`.

In Release builds, `TRACE` and `DEBUG` are compiled out via `QUILL_COMPILE_ACTIVE_LOG_LEVEL`.

Initialize with `fabric::log::init()` and shut down with `fabric::log::shutdown()`. Test binaries handle this in `TestMain.cc`.

## Error handling

- Use `throwError()` from `fabric/utils/ErrorHandling.hh`, not direct `throw` statements
- `throwError()` throws `FabricException` (subclass of `std::exception`)
- Catch and handle exceptions at appropriate boundaries
- Use `FABRIC_LOG_ERROR` for logging errors, not `std::cerr`
- `ErrorCode` enum for hot-path error codes: `Ok`, `BufferOverrun`, `InvalidState`, `Timeout`, `ConnectionReset`, `PermissionDenied`, `NotFound`, `AlreadyExists`, `ResourceExhausted`, `Internal`
- `Result<T>` template for error-code returns without exceptions: `Result<T>::ok(val)`, `Result<T>::error(code, msg)`, `.isOk()`, `.value()`, `.valueOr(default)`
- `Result<void>` specialization for operations with no return value

## Async

The `fabric::async` namespace (`fabric/core/Async.hh`) provides a cooperative async subsystem built on standalone Asio with C++20 coroutines:

- `fabric::async::init()` / `fabric::async::shutdown()` for lifecycle
- `fabric::async::poll()` for non-blocking per-frame processing
- `fabric::async::run()` for blocking server-mode (calls `io_context::run()`)
- `fabric::async::makeStrand()` for per-connection/session serialization
- `fabric::async::makeTimer()` / `fabric::async::makeTimer(duration)` for steady timers
- `fabric::async::context()` for the underlying `asio::io_context`
- `fabric::async::use_nothrow` as a completion token that returns `tuple<error_code, T>`

## Profiling

When `FABRIC_ENABLE_PROFILING` is `ON`, Tracy macros are available via `fabric/utils/Profiler.hh`:

```cpp
#include "fabric/utils/Profiler.hh"

FABRIC_ZONE_SCOPED;            // Profile current scope
FABRIC_ZONE_SCOPED_N("name");  // Named zone
FABRIC_FRAME_MARK;             // Mark frame boundary
```

When profiling is disabled (the default), all macros expand to nothing.

## JSON

`fabric/core/JsonTypes.hh` provides ADL-visible `to_json`/`from_json` overloads for spatial types (Vector2, Vector3, Vector4, Quaternion), enabling direct conversion with nlohmann/json:

```cpp
nlohmann::json j = myVec3;
auto v = j.get<Vector3<float>>();
```

## StateMachine

`fabric/core/StateMachine.hh` is a generic state machine template parameterized on an enum type. Lifecycle.hh uses it internally. Supports transition validation, guards, entry/exit actions, and observers.

```cpp
#include "fabric/core/StateMachine.hh"

enum class MyState { Idle, Running, Done };
fabric::StateMachine<MyState> sm(MyState::Idle);
sm.addTransition(MyState::Idle, MyState::Running);
sm.transition(MyState::Running);
```

## Pipeline

`fabric/core/Pipeline.hh` provides typed multi-stage data processing. Stages transform data sequentially with support for fan-out, conditional stages, and error handling.

```cpp
#include "fabric/core/Pipeline.hh"

fabric::Pipeline<std::string> pipeline;
pipeline.addStage("trim", [](std::string& s) { /* trim whitespace */ });
pipeline.process(myString);
```

## Codec

`fabric/codec/Codec.hh` provides an encode/decode framework for binary, text, and structured data. Codecs register by name and can be chained.

## BufferPool

`fabric/utils/BufferPool.hh` is a fixed-size buffer pool with thread-safe allocation and RAII handles. Use for arena-style allocation patterns where all buffers are the same size.

## ImmutableDAG

`fabric/utils/ImmutableDAG.hh` is a lock-free persistent DAG with structural sharing. Each mutation returns a new version; old versions remain valid. Supports BFS, DFS, topological sort, and LCA queries.

## bgfx

bgfx v1.139.9155 is fetched via `cmake/modules/FabricBgfx.cmake`. CMake targets are `bgfx`, `bx`, `bimg` (no namespace prefix). On macOS, bgfx uses Metal by default. Shader tools (shaderc, texturec, geometryc) are built alongside.

## Breaking circular dependencies

- Forward declare in base class
- Use a separate helper class to implement functionality
- Use dependency injection
