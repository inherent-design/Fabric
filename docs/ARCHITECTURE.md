# Fabric Architecture

## Scope

Fabric currently ships as a reusable engine layer plus one game layer:

- `FabricLib`: the engine static library
- `RecurseGame`: the current game object library
- `Recurse`: the executable that links both

The repository is still centered on Recurse's voxel world, but the architecture is being tightened so the engine can support more than one game without importing Recurse-specific assumptions.

## Current implementation snapshot

### Layering and dependency direction

- `fabric::` is the engine layer
- `recurse::` is the game layer
- dependency direction is one way: `recurse::` depends on `fabric::`, never reverse
- rendering is Vulkan-only through bgfx, with MoltenVK handling the macOS translation layer

### Primary targets

| Target | Type | Current role |
|--------|------|--------------|
| `FabricLib` | static library | Engine runtime, platform, rendering, UI, resources, utilities |
| `RecurseGame` | object library | Game systems, voxel simulation, world generation, persistence, UI |
| `Recurse` | executable | App bootstrap and main loop entry point |
| `UnitTests` | executable | CPU-focused regression and component validation |
| `E2ETests` | executable | Broad application integration checks |

### Runtime phase model

`fabric::SystemPhase` defines seven phases: `PreUpdate`, `FixedUpdate`, `Update`, `PostUpdate`, `PreRender`, `Render`, and `PostRender`.

Recurse currently registers systems in four of those phases:

| Phase | Current systems |
|-------|-----------------|
| `FixedUpdate` | `TerrainSystem`, `VoxelSimulationSystem`, `PhysicsGameSystem`, `CharacterMovementSystem`, `BenchmarkAutomationSystem`, `AIGameSystem`, `ParticleGameSystem`, `ChunkPipelineSystem`, `VoxelInteractionSystem` |
| `Update` | `MainMenuSystem`, `AudioGameSystem`, `CameraGameSystem` |
| `PreRender` | `VoxelMeshingSystem`, `LODSystem`, `ShadowRenderSystem` |
| `Render` | `VoxelRenderSystem`, `OITRenderSystem`, `DebugOverlaySystem` |

`PreUpdate`, `PostUpdate`, and `PostRender` remain part of the engine contract even though Recurse does not currently register systems into them.

## Current subsystem layout

### Engine surface

`include/fabric/` is organized by subsystem:

- `core/`: application context, phases, lifecycle helpers, spatial types, state machines
- `ecs/`: Flecs integration and world-scoped helpers
- `fx/`: result types plus early world operation scaffolding
- `input/`: action and routing primitives
- `log/`: Quill integration and log configuration
- `platform/`: app bootstrap, async, config, window, job scheduling
- `render/`: bgfx integration, camera, draw helpers, scene submission, post-processing
- `resource/`: assets, handles, `ResourceHub`
- `ui/`: RmlUi bridge, panels, toast manager, WebView
- `utils/`: profiler, error handling, text sanitization, graph and BVH utilities
- `world/`: `ChunkCoord`, conversion helpers, `ChunkedGrid`

### Game surface

`include/recurse/` carries the current game domain:

- voxel simulation and materials
- terrain and chunk pipeline logic
- world persistence and registry management
- render systems and LOD support
- gameplay, character, audio, AI, and UI systems
- benchmark startup and automation helpers

## Current world pipeline

### Streaming and simulation

The near world is a chunked voxel simulation driven by `TerrainSystem`, `ChunkPipelineSystem`, and `VoxelSimulationSystem`. Chunk state, chunk activity, and simulation buffers live in the Recurse layer today. The fixed update pipeline freezes structural mutation before parallel work and then runs simulation, meshing preparation, and downstream consumers against that stabilized view.

### Meshing and rendering

`VoxelMeshingSystem` runs in `PreRender` and owns the near chunk meshing path. The current production contract is explicit in code and config:

- `config/recurse.toml` defaults `voxel_meshing.near_chunk_mesher = "greedy"`
- `VoxelMeshingSystem::NearChunkMesher` exposes `Greedy` and `SnapMC`
- Greedy is the primary shipped path
- SnapMC remains an optional experimental or rollback path behind the same boundary

Far distance terrain is handled by `LODSystem`, while `VoxelRenderSystem`, `OITRenderSystem`, and `DebugOverlaySystem` submit visible geometry, transparent composition, and UI overlays in `Render`.

### Persistence

Recurse persists world state through a SQLite-backed world stack that includes world registration, chunk storage, chunk save orchestration, codec support, replay, snapshots, and pruning. This is an active implementation area, not a speculative design note. Persistence is already part of the real runtime and test surface.

### Benchmark and profiling path

Benchmark automation is now a first-class part of the repository, not an afterthought. `BenchmarkAutomationSystem` participates in the frame pipeline, `BenchmarkStartup` handles startup selection, Quill provides structured logs, and Tracy instrumentation is available through profiling builds and capture tasks.

## Short-term planned changes

The active roadmap combines Goal #4 with a concrete meshing checkpoint sequence:

1. **Checkpoint 0**: instrument the current Greedy production path inside `generateMeshCPU()`
2. **Checkpoint 1**: remove the smooth-intermediate repack cost and emit the packed production vertex family directly
3. **Checkpoint 2**: add a read-only mesh semantic and query adapter with no behavior change
4. **Checkpoint 3**: replace blind neighbor dirtying with semantic boundary-change decisions
5. **Checkpoint 4**: move LOD semantic policy onto the same authority used by the Greedy path

Other expected short-term refactors:

- continue separating engine and game concerns so `fabric::` becomes more reusable
- keep the pluggable mesher seam stable enough for rollback and comparison work
- keep benchmark automation, profiling capture, and validation flows aligned with the shipped voxel-first path
- retire remaining engine-side ownership of game-specific concepts where practical

## Long-term architectural direction

The long-term direction is still:

- ops-as-values
- phantom type-state
- centralized execution
- RAII session ownership for world-scoped resources
- multi-project readiness with a clean engine and game boundary

Some scaffolding for that future is already present in `fabric::fx::WorldContext`, `fabric::fx::WorldOps`, `recurse::world::FunctionContracts`, and `recurse::simulation::VoxelSemanticView`. Those types indicate direction, but the migration is not complete and the current runtime still mixes mature production systems with forward-looking scaffolding.

## Architectural guidance for new work

- keep `README.md` short and hub-like
- keep deep implementation detail in `docs/*.md`
- preserve the Greedy-first production posture unless a task explicitly says otherwise
- do not move code into `fabric::` unless it genuinely belongs to a reusable engine surface
- treat `recurse::` as the place for game semantics, world rules, and content-facing decisions
