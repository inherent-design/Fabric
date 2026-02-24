# Testing

## Overview

Fabric uses GoogleTest 1.17.0 for all testing. The test suite contains 364 tests across 38 suites, organized into unit and E2E categories. Tests run in approximately 1.7 seconds.

A custom `TestMain.cc` initializes Quill logging and pauses the ThreadPoolExecutor before test execution, preventing background thread interference.

## Test Suites

| Suite | Binary | Purpose |
|-------|--------|---------|
| UnitTests | `build/bin/UnitTests` | Per-component isolation tests |
| E2ETests | `build/bin/E2ETests` | Full application workflow tests |

## Running Tests

### Via mise (preferred)

```bash
mise run test                        # Unit tests (with timeout)
mise run test:e2e                    # E2E tests
mise run test:filter SpatialTest     # Run specific test by name
```

### Direct execution

```bash
./build/bin/UnitTests
./build/bin/E2ETests
./build/bin/UnitTests --gtest_filter=EventTest*
./build/bin/UnitTests --gtest_filter=ResourceHubTest.LoadResource
```

## Test Files

### Unit Tests (`tests/unit/`)

| File | Component |
|------|-----------|
| `core/CommandTest.cc` | Command pattern (execute, undo, redo) |
| `core/ComponentTest.cc` | Component properties and hierarchy |
| `core/CoreApiTest.cc` | Cross-component API interactions |
| `core/EventTest.cc` | Event dispatching and propagation |
| `core/JsonTypesTest.cc` | nlohmann/json serializers for Vector, Quaternion types |
| `core/LifecycleTest.cc` | State machine transitions |
| `core/PluginTest.cc` | Plugin loading and dependencies |
| `core/ResourceTest.cc` | Resource lifecycle and dependencies |
| `core/ResourceHubTest.cc` | Resource management and caching |
| `core/SpatialTest.cc` | Vector ops, coordinate transforms, GLM bridge |
| `core/TemporalTest.cc` | Timeline and time processing |
| `parser/ArgumentParserTest.cc` | CLI argument parsing |
| `ui/WebViewTest.cc` | WebView and JS bridge |
| `core/CameraTest.cc` | Projection, view matrix, bgfx compat |
| `core/InputManagerTest.cc` | SDL3 event mapping, key bindings |
| `core/SceneViewTest.cc` | Cull + render pipeline, Flecs queries |
| `core/RenderingTest.cc` | AABB, Frustum, DrawCall, RenderList |
| `core/ECSTest.cc` | Flecs world, ChildOf, CASCADE, LocalToWorld |
| `core/ChunkedGridTest.cc` | Sparse 32^3 chunk storage, neighbors |
| `core/FieldLayerTest.cc` | Typed field read/write/sample/fill |
| `core/BVHTest.cc` | Bounding volume hierarchy, frustum queries |
| `core/SimulationTest.cc` | Tick-based rules, deterministic ordering |
| `core/VoxelMesherTest.cc` | Block meshing, hidden face culling |
| `utils/BufferPoolTest.cc` | Fixed-size pool, RAII handles |
| `utils/CoordinatedGraphTest.cc` | Graph operations and locking |
| `utils/ImmutableDAGTest.cc` | Lock-free persistent DAG |
| `utils/ErrorHandlingTest.cc` | Error utilities |
| `utils/LoggingTest.cc` | Quill logging macros (FABRIC_LOG_*) |
| `utils/UtilsTest.cc` | String utils, UUID generation |

### E2E Tests (`tests/e2e/`)

| File | Coverage |
|------|----------|
| `FabricE2ETest.cc` | Full application execution with CLI parameters |

## Test Infrastructure

### Custom Test Main (`tests/TestMain.cc`)

The test binary uses a custom `main()` that:

1. Initializes Quill logging via `fabric::log::init()`
2. Runs all GoogleTest tests
3. Shuts down logging via `fabric::log::shutdown()`

ThreadPoolExecutor is paused during test execution to prevent background threads from interfering with deterministic test behavior.

### Disabled Tests

One test is currently disabled (prefixed with `DISABLED_`). Use `--gtest_also_run_disabled_tests` to include it.

## Naming Conventions

- Unit tests: `ComponentNameTest.cc`
- E2E tests: `FeatureE2ETest.cc`

## Writing Tests

### ResourceHub Tests

When testing code that interacts with ResourceHub, create a local instance and disable worker threads to prevent hangs:

```cpp
ResourceHub hub;
hub.disableWorkerThreadsForTesting();
ASSERT_EQ(hub.getWorkerThreadCount(), 0);
```

Restore in teardown:

```cpp
hub.restartWorkerThreadsAfterTesting();
```

### General Guidance

- Use explicit timeouts on all locks to prevent deadlocks.
- Handle exceptions with try/catch so tests can clean up resources.
- Avoid test dependencies on ResourceHub singleton state.
- Prefer testing with direct Resource objects rather than going through the hub when possible.
- Test one thing at a time; keep test functions focused.
