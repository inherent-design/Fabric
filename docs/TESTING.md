# Testing

## Overview

Fabric uses GoogleTest 1.17.0 for all testing, organized into unit and E2E categories. The current count is 1890 tests across 116 test files and 117 suites (1887 pass, 3 skip). Use `mise run test:all` to confirm current totals on your branch.

Both test executables use a custom `tests/TestMain.cc` that initializes Quill logging before GoogleTest runs and shuts it down afterward.

## Running Tests

### Via mise (Preferred)

```bash
mise run build                      # Build first (test tasks depend on build)
mise run test                       # Unit tests (with timeout)
mise run test:e2e                   # E2E tests
mise run test:all                   # Unit + E2E tests
mise run test:filter SpatialTest    # Run specific test by name
```

### Direct Execution

```bash
./build/dev-debug/bin/UnitTests
./build/dev-debug/bin/E2ETests
./build/dev-debug/bin/UnitTests --gtest_filter=EventTest*
./build/dev-debug/bin/UnitTests --gtest_filter=ResourceHubTest.LoadResource
```

## Test Binaries

| Binary | Path | Purpose |
|--------|------|---------|
| UnitTests | `build/<preset>/bin/UnitTests` | Per-component isolation tests |
| E2ETests | `build/<preset>/bin/E2ETests` | Full application workflow tests |

## Test Infrastructure

### Custom Test Main (`tests/TestMain.cc`)

The test binary uses a custom `main()` that:

1. Initializes Quill logging via `fabric::log::init()`
2. Runs all GoogleTest tests
3. Shuts down logging via `fabric::log::shutdown()`

`ThreadPoolExecutor` is paused during test execution to prevent background threads from interfering with deterministic test behavior.

### BgfxNoopFixture

Tests that require bgfx state (uniform creation, texture handles, view configuration) use `BgfxNoopFixture`. This fixture initializes bgfx with `RendererType::Noop`, which provides the full bgfx API surface without requiring a GPU. Tests that need a live Vulkan device skip via `GTEST_SKIP()`.

### SDLFixture

Tests that depend on SDL display queries (window creation, display enumeration) use `SDLFixture`. This fixture calls `SDL_Init(SDL_INIT_VIDEO)` in `SetUp()` and `SDL_Quit()` in `TearDown()`. Display-dependent tests skip in headless CI environments.

### Disabled Tests

Some tests are disabled (prefixed with `DISABLED_`). Use `--gtest_also_run_disabled_tests` to include them. Current disabled tests: 3 Sprint 7 stubs and 1 property system test.

## Test Organization

### Unit Tests

Unit tests live in `tests/unit/` and mirror the source directory structure:

| Directory | File Count | Coverage Area |
|-----------|------------|---------------|
| `tests/unit/codec/` | 1 | Encode/decode framework |
| `tests/unit/core/` | 92 | Core engine and game systems |
| `tests/unit/parser/` | 1 | CLI argument parsing |
| `tests/unit/ui/` | 5 | DebugHUD, RmlUi backend, Toast, WebView |
| `tests/unit/utils/` | 7 | BVH, BufferPool, CoordinatedGraph, ErrorHandling, ImmutableDAG, Logging, Utils |

### E2E Tests

E2E tests live in `tests/e2e/`. The sole test file (`FabricE2ETest.cc`) validates full application execution with CLI parameters. E2E tests must run from the build directory for correct asset path resolution.

## Naming Conventions

| Category | Pattern | Example |
|----------|---------|---------|
| Unit tests | `ComponentNameTest.cc` | `SpatialTest.cc` |
| E2E tests | `FeatureE2ETest.cc` | `FabricE2ETest.cc` |
| Regression tests | `SubsystemRegressionTest.cc` | `VulkanRegressionTest.cc` |

## Sanitizer Integration

CI runs three sanitizer configurations, each using a dedicated CMake preset. All sanitizer presets use Clang and are non-Windows only.

| Sanitizer | Preset | mise Task | Flags |
|-----------|--------|-----------|-------|
| ASan + UBSan | `ci-sanitize` | `mise run sanitize` | `-fsanitize=address,undefined -fno-omit-frame-pointer` |
| TSan | `ci-tsan` | `mise run sanitize:tsan` | `-fsanitize=thread` |
| Coverage | `ci-coverage` | `mise run coverage` | `-fprofile-instr-generate -fcoverage-mapping` |

### Sanitizer Environment Variables

Test presets set sanitizer-specific environment variables:

| Preset | Variable | Value |
|--------|----------|-------|
| `ci-sanitize` | `ASAN_OPTIONS` | `detect_leaks=1:halt_on_error=1` |
| `ci-sanitize` | `UBSAN_OPTIONS` | `print_stacktrace=1:halt_on_error=1` |
| `ci-tsan` | `TSAN_OPTIONS` | `halt_on_error=1:second_deadlock_stack=1` |
| `ci-coverage` | `LLVM_PROFILE_FILE` | `fabric-%p.profraw` |

All sanitizer presets set `FABRIC_USE_MIMALLOC=OFF` to avoid allocator interference.

## Coverage

```bash
mise run coverage    # Build with coverage, generate lcov report
```

Coverage uses Clang source-based instrumentation (`-fprofile-instr-generate -fcoverage-mapping`) via the `ci-coverage` preset. The coverage workflow generates an lcov report at `build/ci-coverage/coverage.lcov`. The CI coverage workflow is manual-only (`workflow_dispatch`) until Codecov integration is configured.

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
- Avoid test dependencies on ResourceHub singleton state (ResourceHub is de-singletoned; always create local instances).
- Prefer testing with direct Resource objects rather than going through the hub when possible.
- Test one thing at a time; keep test functions focused.
- Tests that require bgfx should use BgfxNoopFixture; tests that require SDL should use SDLFixture.
- GPU-dependent tests that need a live Vulkan device should skip via `GTEST_SKIP()` when no bgfx context is available. These tests run locally but skip in CI.

## Vulkan Regression Tests

`VulkanRegressionTest.cc` validates Vulkan/MoltenVK correctness. These tests cover:

- Shader profiles: SPIR-V is the only enabled profile; all others are suppressed
- View ID conflicts: all bgfx view IDs are unique and ordered correctly
- OIT compositor guards: uninitialized compositor is not valid; composite view follows accumulation view
- VoxelVertex format: 8-byte packed layout, pack/unpack round-trip, stride matches layout
- PostProcess guards: disabled by default; render without init is a no-op
- GPU timer sanitization: clamps garbage timestamps from MoltenVK (negative values, overflow)
- Runtime tests: GPU-dependent checks skip via `GTEST_SKIP` when no bgfx context is available

Run with:

```bash
mise run test:filter VulkanRegression
```

Most tests validate constraints statically without a live Vulkan device. Runtime tests that require a bgfx context skip in CI.
