# Testing

## Scope

Fabric uses GoogleTest for both unit and end-to-end coverage. The current tree contains:

- `tests/unit/` with 147 `.cc` files spread across `core`, `fx`, `persistence`, `platform`, `simulation`, `ui`, `utils`, and `world`
- `tests/e2e/FabricE2ETest.cc` for broad application validation
- `tests/fixtures/` for reusable test fixtures such as `BgfxNoopFixture.hh` and `SDLFixture.hh`

Both test executables share `tests/TestMain.cc`, which initializes Quill logging before GoogleTest runs. Do not add a second `main()` to test files.

## Common commands

```bash
mise run test
mise run test:e2e
mise run test:all
mise run test:filter RecurseBenchmarkStartupTest
```

Direct execution is still useful for quick local runs:

```bash
./build/dev-debug/bin/UnitTests
./build/dev-debug/bin/E2ETests
./build/dev-debug/bin/UnitTests --gtest_filter=ChunkActivityTrackerTest*
```

## Current test layout

| Location | Current focus |
|----------|---------------|
| `tests/unit/core/` | app bootstrap, input, rendering helpers, gameplay systems, config, UI-adjacent engine pieces |
| `tests/unit/fx/` | `Result`, `Error`, and typed error helpers |
| `tests/unit/persistence/` | chunk save service, codec, SQLite store, replay, registry, world session integration |
| `tests/unit/platform/` | benchmark startup, task groups, queueing helpers |
| `tests/unit/simulation/` | chunk state, activity, simulation grid, falling sand, finalization, parallel simulation |
| `tests/unit/ui/` | debug HUD, panels, RmlUi backend, toast and asset flow |
| `tests/unit/utils/` | async helpers, logging, graphs, text sanitization, BVH |
| `tests/unit/world/` | world generators and noise sampling |
| `tests/e2e/` | whole-app smoke coverage |

## Shared harness and fixtures

### `tests/TestMain.cc`

The shared main currently does one important thing beyond GoogleTest bootstrap: it initializes Quill logging so test output and engine code follow the same logging path as the app.

### `BgfxNoopFixture`

Use `BgfxNoopFixture` when a test needs bgfx state without requiring a real GPU backend. This keeps rendering-adjacent tests deterministic and CI-friendly.

### `SDLFixture`

Use `SDLFixture` when the test needs SDL video setup or display queries. Headless environments may still need `GTEST_SKIP()` for platform-dependent cases.

### ResourceHub guidance

For `ResourceHub` tests, create a local hub instance and disable worker threads for the test:

```cpp
ResourceHub hub;
hub.disableWorkerThreadsForTesting();
```

Or use:

```cpp
ResourceHub hub;
hub.reset();
```

Prefer focused tests, explicit lock timeouts, and direct resource objects when possible.

## Current validation priorities

The present test suite is strongest where the current implementation is most active:

- chunk and simulation correctness
- persistence and world-session behavior
- app startup and benchmark automation wiring
- UI and debug tooling behavior
- low-level engine primitives such as `Result`, async helpers, and logging

This matches the codebase's current center of gravity better than the older generic engine-only framing.

## Short-term testing direction

As the combined Goal #4 plus meshing checkpoints land, testing should focus on:

- Greedy-path regression safety first
- semantic-query adapter correctness without behavior drift
- invalidation and boundary-change behavior
- LOD policy alignment with the same semantic authority
- rollback-safe comparison coverage when SnapMC is used as an experimental reference path

Benchmark automation should continue to get targeted startup and control-path tests because it is part of the real workflow, not a one-off tool.

## Long-term testing direction

As the repository moves toward ops-as-values, type-state, and centralized execution, the suite should increasingly add:

- operation contract tests
- executor and session boundary tests
- engine and game boundary tests for multi-project readiness
- narrow regression tests around reusable `fabric::` surfaces

Until then, keep the tests grounded in the current implementation rather than documenting purely aspirational architecture.
