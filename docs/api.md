# Fabric Engine API Reference

Public header classification for game developers building on Fabric Engine.
Headers are organized by stability tier and subsystem.

## Stability Tiers

**Stable**: API contract maintained across versions. Safe to depend on.

**Unstable**: API may change. Functional but subject to revision.

**Internal**: Implementation detail. Do not include directly.

## Minimal Bootstrap

A new game needs these 6 headers to register systems and run the main loop:

| Header | Purpose |
|--------|---------|
| `fabric/platform/FabricApp.hh` | Application entry point and main loop |
| `fabric/platform/FabricAppDesc.hh` | System registration and lifecycle callbacks |
| `fabric/core/SystemBase.hh` | CRTP base class for game systems |
| `fabric/core/SystemPhase.hh` | Phase enum for system registration order |
| `fabric/core/AppContext.hh` | Context struct passed to init, update, render |
| `fabric/log/Log.hh` | Structured logging macros |

The transitive closure of these headers pulls in no game-specific types.
`AppContext.hh` transitively includes `Event.hh`, `Temporal.hh`, and `ECS.hh`.

## Public Stable Headers

### core/

| Header | Purpose |
|--------|---------|
| `fabric/core/AppContext.hh` | Per-frame context: ECS world, dispatcher, system registry, render state |
| `fabric/core/AppModeManager.hh` | Application mode state machine (Game, Paused, Menu, Editor, Console) |
| `fabric/core/CompilerHints.hh` | Branch prediction hints and force-inline macros |
| `fabric/core/Event.hh` | Type-erased event dispatcher with string-keyed listeners |
| `fabric/core/Spatial.hh` | Tagged vector and matrix types with coordinate space safety |
| `fabric/core/StateMachine.hh` | Generic state machine with transition validation |
| `fabric/core/SystemBase.hh` | CRTP base class for systems with phase-dispatched lifecycle |
| `fabric/core/SystemPhase.hh` | Phase enum (Init, Input, Update, Physics, Render, Shutdown) |
| `fabric/core/SystemRegistry.hh` | System storage and phase-ordered dispatch |
| `fabric/core/Temporal.hh` | Timeline management: time regions, entity state snapshots, temporal operations |
| `fabric/core/WorldLifecycle.hh` | World begin/end participant registration |

### ecs/

| Header | Purpose |
|--------|---------|
| `fabric/ecs/ECS.hh` | Flecs ECS world wrapper and query utilities |

### fx/

| Header | Purpose |
|--------|---------|
| `fabric/fx/Error.hh` | Error type for Result monad |
| `fabric/fx/Never.hh` | Bottom type for infallible Result |
| `fabric/fx/OneOf.hh` | Tagged union (variant wrapper) |
| `fabric/fx/Result.hh` | Result monad with error propagation |

### input/

| Header | Purpose |
|--------|---------|
| `fabric/input/InputManager.hh` | Action-based input mapping and state queries |
| `fabric/input/InputRouter.hh` | Key callback registration and console toggle routing |

### log/

| Header | Purpose |
|--------|---------|
| `fabric/log/Log.hh` | Structured async logging via Quill backend |

### platform/

| Header | Purpose |
|--------|---------|
| `fabric/platform/ConfigManager.hh` | TOML configuration file loading and access |
| `fabric/platform/FabricApp.hh` | Application entry point; owns the main loop |
| `fabric/platform/FabricAppDesc.hh` | Descriptor for system registration, window config, lifecycle hooks |
| `fabric/platform/JobScheduler.hh` | enkiTS-backed parallel task dispatch |
| `fabric/platform/PlatformInfo.hh` | OS, CPU, and GPU capability queries |
| `fabric/platform/ScopedTaskGroup.hh` | RAII task group with automatic join on scope exit |
| `fabric/platform/WriterQueue.hh` | Serial write executor: dedicated consumer thread with FIFO task queue |

### render/

| Header | Purpose |
|--------|---------|
| `fabric/render/BgfxHandle.hh` | RAII wrappers for bgfx resource handles |
| `fabric/render/Camera.hh` | Camera with world-space double-precision position and camera-relative rendering |
| `fabric/render/FullscreenQuad.hh` | Lazy-init singleton fullscreen triangle vertex buffer |
| `fabric/render/Geometry.hh` | Vertex layout declarations and mesh data structures |
| `fabric/render/Rendering.hh` | Render state management and frame submission |
| `fabric/render/SceneView.hh` | View and projection matrix management, projection mode cycling |
| `fabric/render/ShaderProgram.hh` | Utility for creating bgfx programs from embedded shaders |
| `fabric/render/SpvOnly.hh` | SPIRV-only shader suppression block for non-SPIR-V renderers |
| `fabric/render/ViewLayout.hh` | Constexpr view ID ranges with static_assert overlap validation |

### ui/

| Header | Purpose |
|--------|---------|
| `fabric/ui/ConcurrencyPanel.hh` | RmlUi panel displaying worker thread and job queue stats |
| `fabric/ui/HotkeyPanel.hh` | RmlUi panel showing keybindings per application mode |

### utils/

| Header | Purpose |
|--------|---------|
| `fabric/utils/BVH.hh` | Bounding volume hierarchy for spatial queries |
| `fabric/utils/ErrorHandling.hh` | Exception types and assertion macros |
| `fabric/utils/Profiler.hh` | Tracy profiler integration macros (no-op when profiling disabled) |

### world/

| Header | Purpose |
|--------|---------|
| `fabric/world/ChunkCoord.hh` | Integer chunk coordinate type and hashing |
| `fabric/world/ChunkCoordUtils.hh` | Coordinate conversion between world, chunk, and local space |

## Public Unstable Headers

These headers are functional and used by Recurse, but their API is subject to change.

| Header | Reason | Purpose |
|--------|--------|---------|
| `fabric/core/RuntimeState.hh` | Contains voxel-specific fields (visibleChunks, totalChunks) | Aggregate of per-frame engine runtime statistics |
| `fabric/ecs/WorldScoped.hh` | API under revision | ECS component lifetime scoped to world begin/end |
| `fabric/fx/SpatialDataOp.hh` | Concept interface still evolving | Concept for spatial data operations via WorldContext |
| `fabric/fx/WorldContext.hh` | Executor template interface still evolving | Type-erased world operation executor |
| `fabric/world/ChunkedGrid.hh` | Hardcoded K_CHUNK_SIZE=32; will be parameterized via template | Sparse infinite grid with chunk-based spatial partitioning |

## Internal Headers

Implementation details not intended for direct inclusion by game code.
These are used by engine internals or exposed only transitively.

| Subsystem | Count | Headers |
|-----------|-------|---------|
| core | 4 | Constants.g.hh, JsonTypes.hh, Lifecycle.hh, Types.hh |
| ecs | 1 | Component.hh |
| fx | 2 | WorldOps.hh, WorldQueryProvider.hh |
| input | 5 | InputAction.hh, InputAxis.hh, InputContext.hh, InputSource.hh, InputSystem.hh |
| log | 2 | FilteredConsoleSink.hh, LogConfig.hh |
| platform | 4 | Async.hh, CursorManager.hh, DefaultConfig.hh, WindowDesc.hh |
| render | 7 | BgfxCallback.hh, DrawCall.hh, HandleMap.hh, PaniniPass.hh, PostProcess.hh, RenderCaps.hh, SkyRenderer.hh |
| resource | 5 | AssetLoader.hh, AssetRegistry.hh, Handle.hh, Resource.hh, ResourceHub.hh |
| ui | 7 | BgfxRenderInterface.hh, BgfxSystemInterface.hh, RmlPanel.hh, ToastManager.hh, WebView.hh, font/FontEngineInterfaceHarfBuzz.hh, font/LanguageData.hh |
| utils | 6 | CoordinatedGraph.hh, CoordinatedGraphCore.hh, CoordinatedGraphLocking.hh, CoordinatedGraphTypes.hh, Testing.hh, Utils.hh |

## Game-Layer Panels (Recurse)

Four UI panels with voxel-specific data structures ship with Recurse in the `recurse::` namespace.
They are not part of the engine API surface.

| Header | Purpose |
|--------|---------|
| `recurse/ui/ChunkDebugPanel.hh` | Chunk mesh statistics overlay |
| `recurse/ui/DebugHUD.hh` | FPS, draw calls, memory, and chunk stats HUD |
| `recurse/ui/LODStatsPanel.hh` | Far-horizon LOD section statistics |
| `recurse/ui/WAILAPanel.hh` | Voxel crosshair inspector (What Am I Looking At) |
