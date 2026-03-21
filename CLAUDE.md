# Claude notes

## Build

Build and test via mise (preferred):

```bash
# Build
mise run build          # Debug build
mise run build:release  # Release build

# Lint & Format
mise run format         # Check clang-format
mise run format:fix     # Auto-format
mise run lint           # clang-tidy (all files, slow)
mise run lint:changed   # clang-tidy (git-dirty only, fast)
mise run lint:fix       # clang-tidy with auto-fix
mise run cppcheck       # cppcheck static analysis

# Test
mise run test           # Unit tests (with timeout)
mise run test:e2e       # E2E tests
mise run test:all       # Unit + E2E
mise run test:filter X  # Filter by test name

# Analysis
mise run sanitize       # ASan + UBSan
mise run sanitize:tsan  # ThreadSanitizer
mise run coverage       # Coverage + lcov report
mise run codeql         # CodeQL security analysis
```

Or with CMake presets:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
```

FabricLib is a static library that all targets link against. The `Recurse` executable additionally links mimalloc when `FABRIC_USE_MIMALLOC` is enabled. Test executables do not link mimalloc.

## Tests

Both `UnitTests` and `E2ETests` use a custom `tests/TestMain.cc` that initializes Quill logging before GoogleTest runs. Do not add a separate `main()` in test files.

When writing ResourceHub tests, create a local instance and disable worker threads to prevent hangs:

```cpp
ResourceHub hub;
hub.disableWorkerThreadsForTesting();
```

Or use `reset()` which disables workers, clears all resources, and resets the memory budget:

```cpp
ResourceHub hub;
hub.reset();
```

For ResourceHub tests, prefer:
1. Using direct Resource objects rather than going through the hub
2. Using explicit timeouts on all locks to prevent deadlocks
3. Wrapping operations in try/catch to ensure cleanup on failure
4. Testing one thing at a time

### bgfx test lifecycle

`BgfxNoopEnvironment` in `tests/fixtures/BgfxNoopFixture.hh` initializes bgfx with the Noop renderer exactly once per process via `::testing::AddGlobalTestEnvironment()` in `tests/TestMain.cc`. Test suites that need bgfx inherit from `BgfxNoopFixture`, which skips if init failed.

Do not call `bgfx::init()` or `bgfx::shutdown()` in test fixtures. bgfx is a process-global singleton; multiple init/shutdown cycles cause fatal assertions.

## Code style

- `.hh` for headers, `.cc` for source files
- Namespace: `fabric::` (with sub-namespaces `fabric::log`, `fabric::async`, etc.)
- PascalCase for classes, camelCase for methods, K_SCREAMING_SNAKE_CASE for constants
- Prefer `const` where possible
- `const_cast` is prohibited except in CoordinatedGraphCore.hh (deliberate lock pattern)
  and test code (string literal to char** conversion). If const_cast seems necessary,
  the API is missing a const overload; fix the API instead.
- Clean code, minimal comments (only for complex or interesting logic)
- No em/en-dashes, no emojis, no double-hyphens except title cases

### Static over dynamic

C++20 for clarity and safety; C99-style performance instincts for hot paths. Prefer compile-time resolution over runtime dispatch.

- **Ownership:** `std::unique_ptr` by default. `std::shared_ptr` only when shared ownership is genuinely required and provable; never as a lazy default.
- **Polymorphism:** Prefer templates, `if constexpr`, and CRTP over virtual dispatch. Use virtual only at system boundaries (SystemBase, RmlPanel) where the runtime set is small and fixed.
- **Allocation:** Value types and stack allocation first. Arena/pool allocation (BufferPool, VertexPool) for batch lifetimes. Heap allocation as last resort.
- **Containers:** `std::array` and fixed-size buffers over `std::vector` when size is known. Raw arrays are acceptable in performance-critical inner loops.
- **Indirection:** Minimize pointer chasing. Flat SOA layouts for cache-friendly iteration. Avoid deep `unique_ptr` nesting.
- **constexpr/consteval:** Push computation to compile time where possible (lookup tables, type traits, config constants).
- **C99 patterns welcome:** Plain structs, free functions, static arrays, bitwise packing, union type-punning (via `std::bit_cast`) are all acceptable when they produce cleaner or faster code than C++ alternatives.

## Logging

Quill v11.x async structured logging via `fabric/core/Log.hh`. Format strings use `{}` placeholders (libfmt style). Quill's backend thread handles all I/O; logging macros are non-blocking on the calling thread.

### Logger hierarchy

```
fabric::log::logger()         // Root logger (FABRIC_LOG_*)
fabric::log::renderLogger()   // FABRIC_LOG_RENDER_*
fabric::log::physicsLogger()  // FABRIC_LOG_PHYSICS_*
fabric::log::audioLogger()    // FABRIC_LOG_AUDIO_*
fabric::log::bgfxLogger()     // FABRIC_LOG_BGFX_*
```

Use subsystem loggers, not the root logger, when code belongs to a specific domain. This enables per-subsystem level filtering at runtime and in TOML config.

### Level selection

| Level | Use for | Compiled out in |
|-------|---------|-----------------|
| TRACE | Per-cell, per-vertex hot-loop data (never in committed code without guard) | Release, RelWithDebInfo |
| DEBUG | Per-frame stats, per-chunk operations, state transitions | Release |
| INFO | Startup, shutdown, config loaded, world created, system lifecycle | Never |
| WARN | Recoverable issues: fallback path taken, budget exceeded, config missing | Never |
| ERROR | Failed operations: file I/O, bgfx resource creation, assertion-like | Never |
| CRITICAL | Unrecoverable: corrupted state, safety violation | Never |

Compile-time ceiling set in CMakeLists.txt via `QUILL_COMPILE_ACTIVE_LOG_LEVEL`. Debug/RelWithDebInfo = DEBUG. Release = INFO.

### Patterns

**Lifecycle events (INFO):**
```cpp
FABRIC_LOG_INFO("ChunkPipelineSystem initialized: radius={}, budget={}", radius, budget);
FABRIC_LOG_INFO("WorldGenerator seed={} type={}", seed, name());
```

**State transitions (DEBUG):**
```cpp
FABRIC_LOG_DEBUG("chunk ({},{},{}) state {} -> {}", cx, cy, cz, fromStr, toStr);
```

**Per-frame stats (DEBUG, guarded):**
```cpp
FABRIC_LOG_DEBUG("meshing: {} dirty, {} meshed, {} skipped, {:.1f}ms", dirty, meshed, skipped, elapsed);
```

**Error with context (ERROR):**
```cpp
FABRIC_LOG_ERROR("failed to load chunk ({},{},{}): {}", cx, cy, cz, ec.message());
```

**Subsystem logger:**
```cpp
FABRIC_LOG_PHYSICS_WARN("collision budget exceeded: {} pending, limit {}", pending, limit);
FABRIC_LOG_RENDER_DEBUG("LOD upload: {} sections, {:.1f}ms", count, elapsed);
```

### Anti-patterns

- `FABRIC_LOG_INFO` in a per-frame loop (floods log; use DEBUG or TRACE with a frame-count guard)
- `std::cerr` or `printf` (bypasses Quill; loses structured output and async I/O)
- String concatenation in format args (`"prefix " + str`; use `{}` placeholders)
- Logging inside `parallelFor` workers without understanding Quill's thread safety (Quill is thread-safe, but high-frequency worker logging floods the queue; prefer per-worker counters collected after dispatch)
- Missing subsystem logger (rendering code using root logger instead of `renderLogger()`)
- Logging raw pointers or addresses without context (log chunk coords, not `slot*`)

### Configuration

`LogConfig` supports 4 layers: defaults < TOML `[logging]` < env vars < CLI flags.

```toml
[logging]
per_run_folders = true
logs_dir = "logs"

[logging.fabric]
console_level = "info"
file_level = "debug"

[logging.render]
console_level = "warn"
file_level = "debug"

[logging.physics]
console_level = "info"
file_level = "debug"
```

Env vars: `FABRIC_LOG_LEVEL`, `FABRIC_LOG_RENDER`, `FABRIC_LOG_PHYSICS`, `FABRIC_LOG_CONSOLE_INCLUDE`, `FABRIC_LOG_CONSOLE_EXCLUDE`.

CLI: `--log.level=debug`, `--log.render=trace`, `--log.include=physics`.

## Error handling

- Use `throwError()` from `fabric/utils/ErrorHandling.hh`, not direct `throw` statements
- `throwError()` throws `FabricException` (subclass of `std::exception`)
- Catch and handle exceptions at appropriate boundaries
- Use `FABRIC_LOG_ERROR` for logging errors, not `std::cerr`
- `fabric::fx::Result<A, Es...>` for composable typed error handling: `Result::success(val)`, `Result::failure(err)`, `.isSuccess()`, `.isFailure()`, `.value()`, `.error<E>()`, `.map()`, `.flatMap()`, `.mapError()`, `.match()`
- `Result<A, Never>` for infallible operations (collapses to just `A`)
- `Result<void, Es...>` for effectful operations that can fail
- Tagged error types in `fabric/fx/Error.hh`: `IOError`, `StateError`, `NotFound`, `ConcurrencyError` (all satisfy `TaggedError` concept)
- Do not introduce ad-hoc `FooResult` structs; use `Result<A, Es...>` with appropriate error types

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

Tracy profiler via `fabric/utils/Profiler.hh`. All macros compile to nothing when `FABRIC_ENABLE_PROFILING` is OFF (the default). Enable with `cmake --preset dev-debug -DFABRIC_ENABLE_PROFILING=ON`.

### Zone instrumentation

Every system's `fixedUpdate()` and `render()` should have a named zone. Phase pipeline stages in VoxelSimulationSystem already do.

```cpp
#include "fabric/utils/Profiler.hh"

void MySystem::fixedUpdate(float dt) {
    FABRIC_ZONE_SCOPED_N("MySystem::fixedUpdate");
    // ...
}

void MySystem::render() {
    FABRIC_ZONE_SCOPED_N("MySystem::render");
    // ...
}
```

**Zone variants:**

| Macro | Use |
|-------|-----|
| `FABRIC_ZONE_SCOPED` | Auto-names from function signature; good for leaf functions |
| `FABRIC_ZONE_SCOPED_N("name")` | Explicit name; use for phases, loops, subsections |
| `FABRIC_ZONE_SCOPED_C(0xFF0000)` | Colored zone (RGB hex); use to visually group related zones |
| `FABRIC_ZONE_SCOPED_NC("name", color)` | Named + colored |
| `FABRIC_ZONE_TEXT(str, len)` | Attach dynamic text to current zone (chunk coords, counts) |
| `FABRIC_ZONE_VALUE(val)` | Attach numeric value to current zone (items processed, bytes) |

**Annotating zones with runtime data:**
```cpp
void VoxelMeshingSystem::processFrame() {
    FABRIC_ZONE_SCOPED_N("meshing");
    FABRIC_ZONE_VALUE(dirtyCount);  // Shows in Tracy tooltip
    // ...
}
```

### Frame marks

`FabricApp::run()` calls `FABRIC_FRAME_MARK` at the end of the main loop. This defines frame boundaries in Tracy. Do not add additional `FABRIC_FRAME_MARK` calls.

For sub-frame timing (fixedUpdate vs render), use named frame marks:
```cpp
FABRIC_FRAME_MARK_START("fixedUpdate");
// ... fixedUpdate work ...
FABRIC_FRAME_MARK_END("fixedUpdate");
```

### Plots

Track per-frame scalars in Tracy's plot view:

```cpp
FABRIC_PLOT("chunks/active", activeCount);
FABRIC_PLOT("meshing/dirty", dirtyCount);
FABRIC_PLOT("sim/epoch", epoch);
FABRIC_PLOT("collision/pending", pendingCount);
FABRIC_PLOT("memory/chunk_mb", chunkBytes / (1024.0 * 1024.0));
```

Plots are cheap (single atomic write). Use them for any per-frame counter that aids debugging.

### Thread naming

Name threads so Tracy's timeline view is readable:

```cpp
FABRIC_SET_THREAD_NAME("main");          // In FabricApp
FABRIC_SET_THREAD_NAME("quill_backend"); // In log::init
```

enkiTS worker threads are named by JobScheduler. Background threads (persistence, LOD gen) should be named at creation.

### Lock profiling

Replace `std::mutex` with `FABRIC_LOCKABLE` to see contention in Tracy:

```cpp
FABRIC_LOCKABLE(std::mutex, mutex_);              // Instead of: std::mutex mutex_;
FABRIC_LOCKABLE_N(std::mutex, mutex_, "ChunkReg"); // With description
```

The no-op fallback (`#else`) expands to a plain `std::mutex` declaration, so this is zero-cost when profiling is disabled.

### Memory profiling

Track allocations for specific subsystems:

```cpp
auto* buf = new ChunkBuffers::Buffer();
FABRIC_ALLOC(buf, sizeof(ChunkBuffers::Buffer));
// ...
FABRIC_FREE(buf);
delete buf;
```

Named pools for aggregate tracking:
```cpp
FABRIC_ALLOC_N(ptr, size, "ChunkBuffers");
FABRIC_FREE_N(ptr, "ChunkBuffers");
```

### What to instrument (audit checklist)

| Location | Zone | Plots |
|----------|------|-------|
| `FabricApp::run()` main loop | `FABRIC_ZONE_SCOPED_N("main_loop")` | frame time |
| Each `SystemBase::fixedUpdate/render` | `FABRIC_ZONE_SCOPED_N("SystemName::method")` | -- |
| VoxelSimulationSystem phases 0-5 | Already instrumented | epoch, active chunks |
| VoxelMeshingSystem per-frame | Zone + `ZONE_VALUE(dirtyCount)` | dirty/meshed/skipped counts |
| PhysicsGameSystem batch collision | Zone + `ZONE_VALUE(batchSize)` | pending collision count |
| LODSystem fixedUpdate + render | Zone per phase | sections built/uploaded |
| ChunkPipelineSystem streaming | Zone | load/unload counts |
| WorldGenerator::generate | Zone + `ZONE_VALUE(1)` per chunk | -- |
| JobScheduler::parallelFor | Zone wrapping dispatch | job count |
| Background jobs (LOD fill, persistence) | `FABRIC_ZONE_SCOPED_N("bg_lod_fill")` | -- |
| ChunkBuffers::materialize | Zone (128KB+ allocation) | materialized count |

### Anti-patterns

- `FABRIC_ZONE_SCOPED` inside a tight inner loop (per-voxel, per-vertex). Tracy overhead is ~50ns/zone; 32K zones per chunk is 1.6ms overhead. Zone the outer loop, use `ZONE_VALUE` for iteration count.
- Missing thread name on background threads (shows as "Thread N" in Tracy; unreadable).
- Forgetting `FABRIC_ZONE_TEXT` for chunk coordinates (a 2ms zone without coords doesn't tell you WHICH chunk was slow).
- Not using plots for per-frame counters (plots are near-free and make trends visible across thousands of frames).

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

## bgfx

All dependencies are managed via CPM.cmake v0.42.1 (`cmake/CPM.cmake`). Each library has a dedicated module in `cmake/modules/Fabric*.cmake` using `CPMAddPackage()`. Set `CPM_SOURCE_CACHE=~/.cache/CPM` (configured in `mise.toml`) to share sources across builds.

bgfx v1.139.9155 is fetched via `cmake/modules/FabricBgfx.cmake`. CMake targets are `bgfx`, `bx`, `bimg` (no namespace prefix). Rendering uses Vulkan on all platforms; macOS translates via MoltenVK. All shaders compile to SPIR-V. Shader tools (shaderc, texturec, geometryc) are built alongside.

## Concurrency patterns

### enkiTS / JobScheduler

`fabric::JobScheduler` wraps enkiTS (work-stealing, persistent threads). Key patterns:

- **threadnum indexing**: `workerCount()` returns N created threads. enkiTS `threadnum` ranges 0..N where 0 is the calling thread. Per-worker resources must allocate `workerCount()+1` slots and index by `workerIdx` from `parallelFor(count, fn(jobIdx, workerIdx))`.
- **Per-worker collection**: use per-workerIdx vectors during `parallelFor`, collect results on main thread after completion. Validated in FallingSandSystem (settled chunk lists) and PhysicsGameSystem (batch collision).
- **Stack budget**: enkiTS uses 64KB fiber stacks by default. Keep stack allocations under ~40KB in any function dispatched via `parallelFor` or `submitBackground`. Move large arrays (noise buffers, etc.) to heap.
- **Background job pointer safety**: create/lookup objects on main thread, pass pointer to `submitBackground` lambda. The owning container must not erase entries with in-flight background jobs.

### Phase pipeline contracts

The per-tick phase pipeline separates structural modification from data access:

- **Phase 0 (main thread)**: structural modification (add/remove chunks, transition states, `buildDispatchList`, `resolveBufferPointers`). Pre-materialize all needed chunks here so parallel phases never insert into the registry.
- **Phases 1-8**: topology frozen. `find()` is safe (readers only). Workers access pre-resolved pointers; no map lookup in hot loops.
- **State gating**: simulation dispatches only Active chunks. Meshing observes BoundaryDirty. Generation targets Generating chunks. The `ChunkSlotState` enum defines which phases process which chunks.

### SimulationGrid buffer operations

- **`syncChunkBuffers(cx,cy,cz)`**: for newly generated chunks. Copies write buffer to all other buffers (single chunk). Use after `WorldGenerator::generate()`.
- **`advanceEpoch()`**: post-simulation epoch swap. Copies write buffer to the buffer that will serve as next epoch's write target. For K=2 this is the current read buffer; for K=3 it would be the free buffer.
- **Index math**: `readIdx = epoch % K_COUNT`, `writeIdx = (epoch+1) % K_COUNT`, `copyDst = (epoch+2) % K_COUNT`. Works for any K_COUNT >= 2.
- **Triple-buffer note**: increasing K_COUNT to 3 does NOT eliminate the advanceEpoch copy. FallingSandSystem writes partial cells; untouched cells must carry forward. Triple-buffer changes the copy target (free buffer instead of read buffer), enabling concurrent reads during the copy.

### WorldGenerator precision

`GenSingle2D` and `GenUniformGrid2D` (FastNoise2) differ by 1 ULP at FMA boundaries due to different coordinate computation paths. At density thresholds (`> 0.0f`, `> 3.0f`) this can flip material selection, producing LOD seams. Use `std::fma()` for coordinate replication in `sampleMaterial()` to match batch behavior.

## Cell accessors

`recurse::simulation::CellAccessors.hh` provides free-function accessors that quarantine direct `VoxelCell` field access. All simulation and consumer code should use these instead of reading `cell.materialId`, `registry.get(id).moveType`, or `registry.get(id).density` directly.

| Accessor | Signature | Replaces |
|----------|-----------|----------|
| `isOccupied` | `(VoxelCell cell) -> bool` | `cell.materialId != material_ids::AIR` |
| `isEmpty` | `(VoxelCell cell) -> bool` | `cell.materialId == material_ids::AIR` |
| `cellPhase` | `(const MaterialRegistry&, VoxelCell) -> MoveType` | `registry.get(id).moveType` |
| `canDisplace` | `(const MaterialRegistry&, VoxelCell, VoxelCell) -> bool` | `registry.get(src).density > registry.get(tgt).density` |

These accessors exist to contain the blast radius of the MatterState migration. When Wave 4 swaps the cell layout, only the accessor bodies change, not the consumer call sites.

### Anti-patterns

- Direct `cell.materialId` comparison outside CellAccessors.hh
- `registry.get(id).moveType` or `.density` in simulation code (use `cellPhase`/`canDisplace`)
- Adding new cell field reads without a corresponding accessor

## VoxelStats

`recurse::VoxelStats` (in `include/recurse/world/VoxelStats.hh`) holds game-specific runtime counters that were previously in `fabric::RuntimeState`. Engine code must not reference `VoxelStats`; it lives in `recurse::` because the counters are voxel-game-specific.

Fields: `visibleChunks`, `totalChunks`, `meshQueueSize`.

## Development workflow

### Commits

Conventional-style prefixes: `feat:`, `fix:`, `chore:`, `docs:`, `test:`, `refactor:`, `perf:`, `build:`.

Subject under 72 characters, imperative mood. Body explains why, not what. Bullet points for multi-line bodies.

Scopes are optional and used when targeting a specific subsystem:
```
fix(physics): correct collision batch allocation for enkiTS thread count
refactor(simulation): parametrize chunk buffer count for triple-buffer readiness
perf(meshing): parallel chunk meshing via JobScheduler
```

### Making changes

1. Make changes on `dev` branch (or feature branch off `dev`)
2. Build: `mise run build`
3. Test: `mise run test`
4. Lint (git-dirty files only): `mise run lint:changed`
5. If header/source files moved: check `.cppcheck-suppressions` paths
6. If code affects documented behavior: update `docs/` in the same commit

### cppcheck suppressions

`.cppcheck-suppressions` lists known pre-existing findings. When cppcheck flags a new finding, either fix it or add a suppression with a comment explaining why. The script (`tasks/cppcheck.sh`) uses `--error-exitcode=1`, so any unsuppressed finding fails CI.

### Documentation

**Where things live:**
- `README.md` is the hub (feature summary, quickstart, links to `docs/`). Keep scannable; no deep reference content.
- `docs/*.md` are deep reference per topic. Source of truth for that domain.
- `CONTRIBUTING.md` covers development workflow only. Not user-facing.
- `CLAUDE.md` (this file) covers agent/LLM instructions: build, style, patterns, anti-patterns.

**When to update docs:**
- Header changes that alter public API: update corresponding `docs/` file in the same change.
- New systems or subsystems: add a section to `CLAUDE.md` if it has patterns agents need to know.
- Config changes (TOML keys, env vars, CLI flags): update both `docs/` and relevant CLAUDE.md sections.
- Roadmap or architecture shifts: update the existing markdown set in place instead of scattering new one-off planning docs.

**Writing style:**
- Match code identifiers exactly (camelCase methods, PascalCase types, `K_CONSTANT`)
- State what it does, valid values, and default, in that order
- No em-dashes, en-dashes, or double-hyphens in prose
- No emojis, no superlatives, no flattery, no preamble
- Technical reference tone; precision over poetry

## Current repository stance

- Greedy meshing is the primary near-path production path. Preserve it first.
- SnapMC is optional and experimental behind the pluggable mesher boundary.
- The active short-term program is the **strong-hybrid MatterState migration** (strangler-fig pattern), which wraps legacy material-first assumptions behind narrow accessors, then swaps the cell layout behind those accessors.
- Wave 1 (accessor quarantine and boundary cleanup) is complete and in review as PR #81.
- Waves 2 through 6 continue through mesh/LOD abstraction, type definitions, the live cell layout swap, consumer migration, and GPU pipeline updates.
- Meshing optimization (greedy instrumentation, packed-vertex cleanup, semantic adapter) is sequenced within this migration, not as a separate track.
- Other near-term work should keep improving engine and game separation plus multi-project readiness without destabilizing the shipped voxel-first path.

## Programming model

Fabric is migrating toward an effect-like programming model. The target architecture has three pillars: ops-as-values, phantom type-state, and centralized execution. These are not yet implemented end-to-end, though `fabric::fx::WorldContext`, `recurse::world::FunctionContracts`, and `recurse::simulation::VoxelSemanticView` already act as scaffolding toward that direction. This section describes the target so new code aligns with it and avoids introducing patterns that conflict.

Reference: `~/.atlas/integrator/reports/fabric/effect-cpp-proposal-2026-03-02.md`

### Ops-as-values

State mutations are expressed as data, not direct function calls. Instead of calling `store->loadChunk(cx, cy, cz)` inside a lambda on a worker thread, submit a `LoadChunk{cx, cy, cz}` value to an executor. The executor owns the resource, controls lifetime, and resolves operations in batch.

```cpp
// Target pattern: operation is a struct, not a side effect
struct LoadChunk {
    ChunkCoord coord;
    using RequiresState = Unknown;
    using Returns = ChunkRef<Loading>;
    using Errors = TypeList<IOError, NotFound>;
};

// Executor resolves the operation; owns the store lifetime
auto result = ctx.resolve(LoadChunk{coord});
```

Benefits: operations are inspectable (debug/logging), batchable (one transaction for N ops), cancellable (drop from queue), reorderable (priority sort), replayable (deterministic testing). Lifetime management is centralized in the executor, not distributed across N closures capturing raw pointers.

Reads also go through the executor for model uniformity, but are resolved synchronously (no queue overhead):

```cpp
// Reads: synchronous resolution, zero-copy
auto buf = ctx.resolve(ReadBuffer{coord});
// Returns Result<const VoxelBuffer*, NotFound>
```

### Phantom type-state

Chunk lifecycle states are encoded in the type system at API boundaries. Internal storage is type-erased (variant or enum). The type constrains what callers can do with a handle.

```cpp
// ChunkRef<Active> exposes readBuffer(), palette()
// ChunkRef<Loading> exposes progress()
// Compile error if you call readBuffer() on a Loading chunk

// State transitions return the new type
auto active = ctx.resolve(Activate{coord});
// active : Result<ChunkRef<Active>, ActivateError>
```

Operations carry their state requirements as type parameters. The executor validates that the current runtime state matches the required state before resolution.

### Centralized execution

Every interaction with world state (reads and mutations) goes through a context/executor. This provides a single enforcement point for:

- Lifetime safety (executor owns resources; callers never hold raw pointers to stores)
- Concurrency contracts (executor decides what runs on which thread)
- State validation (executor checks preconditions before resolving ops)
- Observability (executor logs all operations for debugging)

The executor resolves reads synchronously (direct return, no allocation) and mutations asynchronously (queued, batched, deferred). The operation is always a value; the resolution strategy is an implementation detail.

### Result

`fabric::fx::Result<A, Es...>` handles composable typed errors. `A` is the success type; `Es...` are the possible error types (tagged, `std::variant`-backed). Operations compose via `flatMap`, which merges error channels at compile time.

```cpp
auto r = ctx.resolve(LoadChunk{coord})
    .flatMap([](auto ref) { return decode(ref); })
    .flatMap([](auto data) { return decompress(data); });
// r : Result<VoxelData, IOError, NotFound, DecodeError, ZstdError>
```

`Never` sentinel indicates infallible operations (`Result<A, Never>` collapses to just `A`). Error channel merging is automatic and deduplicated at the type level. Four specializations: `Result<A, Es...>` (primary), `Result<A, Never>` (infallible), `Result<void, Es...>` (effectful), `Result<void, Never>` (void infallible).

### RAII session boundaries

Per-world state is owned by a session object whose destructor guarantees complete cleanup. `WorldSession` owns the chunk store, save service, transaction store, ECS entities, streaming state, and pending async work. Destroying the session drains all futures, flushes persistence, and clears all associated state.

```cpp
std::unique_ptr<WorldSession> session_;
void loadWorld(...) { session_ = std::make_unique<WorldSession>(...); }
void unloadWorld() { session_.reset(); } // complete teardown by construction
```

This pattern applies to any resource group with a shared lifecycle (audio sessions, network connections, editor undo stacks).

### Compile-time enforcement

Prefer `constexpr`, `consteval`, concepts, and template constraints over runtime checks. Push invariant validation to compile time where the state is statically known. Use runtime guards with logging only where state is inherently dynamic (chunk lifecycle transitions driven by I/O completion).

The Zig comptime mental model applies: if the information exists at compile time, validate it there. C++20 provides concepts, `requires` clauses, `consteval`, and `if constexpr` for this.

## Multi-project readiness

Fabric is the engine; Recurse is one game. Someone's (a Pokemon platform) will be a second game on Fabric. All engine code must be game-agnostic.

Current violations to address:
- `K_CHUNK_SIZE` defined in engine headers but semantically game-specific
- Voxel-specific types in `fabric::` that belong in `recurse::`
- `fabric/core/` contains 27 headers spanning unrelated domains (ECS, cameras, audio, physics)

Resolved violations:
- `ChunkCoord` 4-way duplication unified to single `fabric::ChunkCoord{x,y,z}` in Wave 1b (2026-03-13)
- `visibleChunks`/`totalChunks`/`meshQueueSize` moved from `fabric::RuntimeState` to `recurse::VoxelStats` in Wave 1 (2026-03-20)
- `ChunkedGrid` default template parameter `= 32` removed; all 43 instantiation sites now explicit (2026-03-20)

The `fabric::`/`recurse::` boundary must be a clean API surface that a second game can depend on without importing Recurse-specific types. Near-term work should keep removing game-specific assumptions from `fabric::` while preserving the current Greedy-first production path in `recurse::`.

## Dead code

Removed in Wave 1a/1c (2026-03-13): Plugin.hh/.cc, FileWatcher, BufferPool.hh, ArgumentParser.hh, SyntaxTree.hh, Token.hh, SimulationThreadPool, ThreadPoolExecutor, ImmutableDAG, TimeoutLock, Codec.hh, Command.hh, Pipeline.hh, ChunkDirtyTracker, FilesystemChunkStore, and 14 associated test files. 3,828 lines total.

No known dead code modules remain. If new dead code is discovered, list here with callers and size.

## Breaking circular dependencies

- Forward declare in base class
- Use a separate helper class to implement functionality
- Use dependency injection
