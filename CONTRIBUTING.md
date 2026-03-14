# Contributing to Fabric Engine

## Prerequisites

- [mise](https://mise.jdx.dev/) (manages cmake, ninja, and task running)
- C++20 compiler (Clang 13+, GCC 10+, MSVC 19.29+)
- Platform-specific Vulkan tooling (see [Build Guide](docs/BUILD.md))

### Platform Requirements

Vulkan is the sole rendering backend (SPIR-V shaders).

- **macOS**: Vulkan runs via MoltenVK. Install with `brew install molten-vk vulkan-loader`. The build sets `BUILD_RPATH` to `/opt/homebrew/lib` so the Vulkan loader is found at runtime.
- **Linux**: Install Vulkan SDK and drivers for your GPU.
- **Windows**: Install the LunarG Vulkan SDK.

## Getting Started

```bash
git clone <repository-url>
cd fabric
mise install
mise run build
mise run test
```

## Task Reference

All tasks are defined in `mise.toml` and run via `mise run <task>`.

| Task | Alias | Description |
|------|-------|-------------|
| `build` | `b` | Configure and build (Debug) |
| `build:release` | `br` | Configure and build (Release) |
| `clean` | *none* | Remove build artifacts |
| `run` | *none* | Build and run (Debug) |
| `run:release` | *none* | Build and run (Release) |
| `format` | *none* | Check clang-format |
| `format:fix` | *none* | Auto-format with clang-format |
| `lint` | *none* | Run clang-tidy on all source files (slow) |
| `lint:changed` | *none* | Run clang-tidy on git-dirty files only (fast) |
| `lint:fix` | `fix` | Run clang-tidy with auto-fix |
| `cppcheck` | *none* | Run cppcheck static analysis |
| `test` | `t` | Run unit tests |
| `test:e2e` | *none* | Run E2E tests |
| `test:all` | *none* | Run unit + E2E tests |
| `test:filter` | `tf` | Run unit tests with gtest filter |
| `profile` | *none* | Build with Tracy profiling (Debug) |
| `profile:release` | *none* | Build with Tracy profiling (Release) |
| `profile:capture` | *none* | Build + capture Tracy trace (Debug) |
| `profile:view` | *none* | Open Tracy profiler GUI |
| `profile:csv` | *none* | Export Tracy trace to CSV |
| `sanitize` | *none* | ASan + UBSan build and run |
| `sanitize:tsan` | *none* | ThreadSanitizer build and run |
| `coverage` | *none* | Code coverage with lcov report |
| `codeql` | *none* | CodeQL security analysis |

## Development Workflow

1. Create a feature branch from `main`
2. Build and test:
   ```bash
   mise run build && mise run test
   ```
3. Lint (fast mode for iteration, full before submitting):
   ```bash
   mise run lint:changed
   mise run lint
   ```
4. Submit a pull request

## Dependency Management

All third-party libraries are fetched via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) v0.42.1. Each library has a dedicated module in `cmake/modules/` (32 modules: 22 CPM dependencies + 10 shader/build modules).

`CPM_SOURCE_CACHE=~/.cache/CPM` is configured in `mise.toml` to share downloads across builds. The build uses `ccache` when available.

### CPM Patches

Some dependencies require vendored fixes. `cmake/patches/` holds patch files applied via the CPM `PATCHES` argument. For example, `bgfx-vk-suboptimal.patch` fixes a case where bgfx treats `VK_SUBOPTIMAL_KHR` as a fatal error on window resize with MoltenVK.

## Project Structure

```
fabric/                         # Engine (L0-L4)
  include/fabric/
    codec/                      # Encode/decode framework
    core/                       # ECS, Resource, Event, Command, Pipeline,
                                # Camera, Rendering, BVH, ChunkedGrid,
                                # Spatial, Temporal, Types, Log, Constants
    input/                      # Input routing and bindings
    parser/                     # ArgumentParser, SyntaxTree, Token
    platform/                   # FabricApp, SDL3 integration
    render/                     # Render systems, vertex pools, shaders
    ui/                         # RmlUi panels, BgfxRenderInterface, WebView
    utils/                      # ErrorHandling, Profiler, Testing, BufferPool
    world/                      # ChunkedGrid, ChunkCoordUtils
  src/
    core/                       # Engine implementation (.cc)
    input/
    platform/
    render/
    ui/
    world/

recurse/                        # Game (L5); depends on fabric, never reverse
  include/recurse/
    ai/                         # BehaviorTree debug panel
    animation/                  # Skeletal animation
    audio/                      # Miniaudio integration
    character/                  # CharacterController, MovementFSM
    components/                 # Game-specific ECS components
    gameplay/                   # Game rules
    persistence/                # Save/load, chunk storage
    physics/                    # Jolt integration, collision
    render/                     # Game-specific render systems
    simulation/                 # FallingSand, voxel simulation
    systems/                    # Per-frame game systems
    ui/                         # DevConsole, game UI panels
    world/                      # World generation, chunk pipeline
  src/recurse/
    ...                         # Mirrors include layout

shaders/
  oit/                          # Order-independent transparency
  panini/                       # Panini projection
  particle/                     # Particle system
  post/                         # Post-processing (bloom, tonemap)
  rmlui/                        # RmlUi render interface
  shared/                       # Shared varying defs, fullscreen VS
  skinned/                      # Skeletal animation
  sky/                          # Atmospheric rendering
  voxel/                        # Greedy mesh voxel
  voxel-lighting/               # Smooth mesh voxel (merged LOD+Smooth)

assets/
  ui/                           # RmlUi documents (.rml, .rcss)
  fonts/                        # Font files

tests/
  unit/                         # Unit tests (2089 tests, 135 files)
  e2e/                          # End-to-end tests

cmake/
  modules/                      # 32 CMake modules
  patches/                      # Vendored dependency patches

CMakeLists.txt                  # Single static library (FabricLib)
CMakePresets.json               # 13 configure presets (1 hidden base, 12 visible)
mise.toml                       # Task runner and tool management
```

## Design Principles

### Operations as values

State mutations are expressed as data structures, not direct function calls. Instead of calling a method on a resource from inside a worker thread closure, submit an operation struct to an executor that owns the resource.

```cpp
// Instead of this (side effect in closure, raw pointer capture):
scheduler.submit([store, cx, cy, cz]() {
    store->loadChunk(cx, cy, cz);  // store might be destroyed
});

// Do this (operation is data, executor owns lifetime):
pipeline.submit(LoadChunk{cx, cy, cz});
// Executor resolves the operation; owns the store.
```

This makes operations inspectable, batchable, cancellable, and replayable. Lifetime management is centralized in the executor, not distributed across closures.

### Compile-time correctness

Use C++20 features to push validation to compile time:

- **Concepts** for constraining template parameters at API boundaries
- **`constexpr`/`consteval`** for compile-time computation and validation
- **Phantom types** for encoding state in the type system (a `ChunkRef<Active>` exposes different methods than `ChunkRef<Loading>`)
- **`requires` clauses** as precondition documentation that the compiler enforces

```cpp
// State encoded in the type; invalid usage is a compile error
auto active = executor.resolve(Activate{coord});
const auto& buf = active.readBuffer();   // OK: Active chunks are readable
// loading.readBuffer();                 // COMPILE ERROR: Loading chunks are not
```

Use runtime checks only for inherently dynamic state (I/O results, user input, network).

### RAII session boundaries

Group related resources into a session object whose destructor guarantees complete cleanup. This prevents the class of bugs where teardown forgets to clean up some state.

```cpp
// WorldSession owns: store, save service, ECS entities, pending loads, streaming state
// Destroying the session drains futures, flushes persistence, clears everything.
std::unique_ptr<WorldSession> session_;
void loadWorld(...) { session_ = std::make_unique<WorldSession>(...); }
void unloadWorld() { session_.reset(); }  // complete by construction
```

### Error composition

Operations that can fail declare their error types. Composed operations merge error channels automatically. Do not create ad-hoc `FooResult` structs; use `fabric::fx::Result<A, Es...>` with tagged error types from `fabric/fx/Error.hh`.

### Engine/game separation

`fabric::` is the engine; `recurse::` is the game. The dependency is strictly one-way: game code depends on engine code, never reverse. Engine code must not reference game-specific types, constants, or assumptions.

When adding engine features, ask: "Would a second game on Fabric need this?" If yes, it belongs in `fabric::`. If no, it belongs in `recurse::`.

## Code Style

### Files

- `.hh` for headers, `.cc` for source files
- `#pragma once` header guards

### Namespaces

Engine code lives under `fabric::` with sub-namespaces matching directory structure:

| Namespace | Domain |
|-----------|--------|
| `fabric` | Core engine types and systems |
| `fabric::async` | Coroutine and async primitives |
| `fabric::codec` | Encode/decode framework |
| `fabric::log` | Logging utilities |
| `fabric::world` | Chunk grid, coordinate utilities |
| `recurse` | Game-layer types |
| `recurse::physics` | Jolt physics integration |
| `recurse::simulation` | Voxel simulation (FallingSand, materials) |
| `recurse::systems` | Per-frame game systems |

Dependency is one-way: `recurse` depends on `fabric`, never reverse.

### Naming

| Element | Convention | Example |
|---------|------------|---------|
| Classes, structs, enums | `PascalCase` | `ChunkSlot`, `VoxelCell` |
| Functions, methods | `camelCase` | `advanceEpoch()`, `syncChunkBuffers()` |
| Variables, parameters | `camelCase` | `chunkSize`, `workerCount` |
| Constants | `K_SCREAMING_SNAKE` | `K_CHUNK_SIZE`, `K_DEFAULT_VIEW_ID` |
| Macros | `FABRIC_MACRO_NAME` | `FABRIC_LOG_INFO`, `FABRIC_ZONE_SCOPED` |

Prefer `constexpr` over macros.

### Error Handling

- `throwError()` from `fabric/utils/ErrorHandling.hh` for error conditions; not raw `throw`
- `fabric::fx::Result<A, Es...>` for typed error handling (tagged errors from `fabric/fx/Error.hh`)
- All exceptions subclass `FabricException`

### Logging

`FABRIC_LOG_*` macros from `fabric/core/Log.hh` (Quill v11, `{}` format placeholders):

```cpp
FABRIC_LOG_DEBUG("Chunk loaded at ({}, {}, {})", x, y, z);
FABRIC_LOG_ERROR("Failed to decode FCHK: {}", reason);
```

Levels: `DEBUG`, `INFO`, `WARN`, `ERROR`, `CRITICAL`.

### Profiling

`FABRIC_ZONE_SCOPED` / `FABRIC_ZONE_SCOPED_N("name")` for Tracy profiling zones. Add to any function that may appear in frame traces.

### Lifecycle

All `SystemBase` subclasses override `doInit()` / `doShutdown()`. The base class provides non-virtual idempotent `init()` / `shutdown()` wrappers.

### Comments

Default to no comments. Code should be self-explanatory through naming and structure. Comment when:

- The "why" is non-obvious (workaround, performance constraint, spec requirement)
- The behavior has surprising side effects
- A constant comes from an external spec (include the reference)

Write code with future doc-gen in mind: clear naming, consistent patterns, structured types.

## Writing and Prose

- No em-dashes, en-dashes, or double-hyphens in prose; use semicolons, commas, or colons
- No emojis
- No superlatives or marketing language
- Technical reference tone; precision over poetry
- Say it once, clearly; no long-then-short restatement

## Commits

Conventional-style prefixes: `feat:`, `fix:`, `chore:`, `docs:`, `test:`, `refactor:`, `perf:`, `build:`

Subject under 72 characters, imperative mood. Body explains why, not what. Bullet points for multi-line bodies.

## Documentation

- **README.md** is the hub: feature summary, quickstart, links to `docs/`
- **docs/*.md** are deep reference per topic (source of truth for that domain)
- **CONTRIBUTING.md** covers development workflow only
- When changing code that affects documented behavior, update `docs/` in the same change

## License

By contributing, you agree that your contributions will be licensed under the project's [MIT License](LICENSE).
