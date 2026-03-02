# Fabric Engine Architecture

## Overview

Fabric is a C++20 cross-platform runtime for building interactive spatial-temporal applications. Programming primitives (math, time, events, commands) compose into structural primitives (scenes, entities, graphs, simulations) that applications use to create games, editors, simulations, and multimedia tools. All public symbols reside in the `fabric::` namespace, with sub-namespaces for utilities (`fabric::Utils`, `fabric::Testing`) and subsystems (`fabric::log`, `fabric::async`).

Rendering uses Vulkan on all platforms. macOS translates Vulkan calls to Metal via MoltenVK. All shaders compile to SPIR-V.

## Layer Architecture

```
L0: Platform Abstraction
    SDL3             Windowing, input, timers, filesystem, native handles for bgfx
    bgfx             Rendering backend: Vulkan-only, SPIR-V shaders, MoltenVK on macOS
    WebView          Embedded browser, JS bridge (optional, FABRIC_USE_WEBVIEW)
    mimalloc         Memory allocator, available for explicit use (MI_OVERRIDE OFF by default)

L1: Infrastructure
    Log              Quill async SPSC logging, FABRIC_LOG_* macros, compile-time filtering
    Profiler         Tracy abstraction, zero-cost when FABRIC_ENABLE_PROFILING is OFF
    Async            Standalone Asio io_context, C++20 coroutine support, strands, timers
    ErrorHandling    FabricException, throwError(), ErrorCode enum, Result<T> template
    StateMachine     Generic state machine template with transition guards, entry/exit actions, observers

L2: Primitives
    Spatial          Type-safe vec/mat/quat/transform with compile-time coordinate space tags; GLM bridge
    Temporal         Multi-timeline time processing, snapshots, variable flow
    Event            Typed thread-safe publish/subscribe with propagation control
    Command          Undo/redo command pattern with composite commands and history
    Pipeline         Typed multi-stage data processing with fan-out and conditional stages
    Types            Core Variant, StringMap, Optional type aliases
    JsonTypes        nlohmann/json ADL serializers for Vector, Quaternion types

L2.5: Codec
    Codec            Encode/decode pipeline for binary, text, and structured data formats

L3: Structural
    ECS              Flecs v4.1.4: world wrapper, ChildOf hierarchy, CASCADE queries, LocalToWorld
    Component        Type-safe component architecture with variant properties and hierarchy
    Resource         Resource base with state machine, dependency tracking, priority loading
    ResourceHub      Centralized resource management, worker threads, memory budgets (de-singletoned)
    Lifecycle        StateMachine-backed component lifecycle transitions
    CoordinatedGraph Thread-safe DAG with intent-based locking, deadlock detection
    ImmutableDAG     Lock-free persistent DAG with structural sharing, snapshot isolation
    BufferPool       Fixed-size buffer pool with thread-safe allocation and RAII handles
    BVH              Bounding volume hierarchy, median-split, AABB and frustum queries

    Camera           Projection and view matrix (bx::mtxProj, homogeneousDepth, perspective/ortho)
    Rendering        AABB, Frustum, DrawCall, RenderList, FrustumCuller
    RenderCaps       Renderer capability detection and query

    Voxel World
        ChunkedGrid      Sparse 32^3 chunk-based 3D grid template
        FieldLayer       Typed field over ChunkedGrid (DensityField, EssenceField)
        VoxelMesher      Greedy meshing, per-vertex AO (4 levels), packed 8-byte VoxelVertex, palette
        VoxelRaycast     DDA Amanatides-Woo algorithm (castRay, castRayAll)
        VoxelRenderer    Chunk rendering with bgfx
        ChunkStreaming   Dynamic radius, speed-based prefetch, load/unload budgets
        ChunkMeshManager Dirty-chunk tracking via VoxelChanged event, re-mesh queue
        TerrainGenerator Procedural terrain via FastNoise2 (FBm, Cellular, DomainWarp)
        CaveCarver       Cellular automata cave generation
        EssencePalette   Continuous-to-discrete material quantization for greedy merging
        LSystemVegetation L-system procedural vegetation
        WFCGenerator     Wave function collapse structure generation
        StructuralIntegrity Voxel structural support analysis

    Character and Movement
        CharacterController  AABB ground collision, per-axis resolution, step-up
        FlightController     6DOF arcade flight with exponential drag
        MovementFSM          12-state machine via StateMachine<CharacterState>, transition guards
        TransitionController Ground-air momentum preservation, ground scan
        DashController       Burst movement with cooldown (ground dash, air boost)
        CameraController     First-person and third-person with DDA spring arm collision
        VoxelInteraction     Create/destroy matter via raycast-to-field, VoxelChanged events
        MeleeSystem          Axis-aligned hitbox, AABB intersection, cooldown, damage events

    Physics
        PhysicsWorld     Jolt v5.5.0: rigid body dynamics, chunk collision shapes, ghost prevention
        Ragdoll          Multi-joint capsule bodies, fixed constraints, activate/deactivate

    AI
        BehaviorAI       BehaviorTree.CPP 4.8.4: NPC behavior trees (patrol, chase, flee, attack)
        Pathfinding      A* on ChunkedGrid density, 6-connected, maxNodes budget, PathFollower

    Audio
        AudioSystem      miniaudio 0.11.22: spatial 3D, DDA occlusion, SPSC ring buffer, buses
        ReverbZone       Spatial reverb regions
        MaterialSounds   Material-based impact sounds

    Animation
        Animation        ozz-animation sampling, blending, LocalToModel
        AnimationEvents  Clip markers, time-wrap processing, typed callbacks
        SkinnedRenderer  GPU-skinned mesh rendering with bgfx
        IKSolver         Analytical two-bone inverse kinematics
        MeshLoader       fastgltf-based glTF 2.0 model loading

    Rendering Pipeline
        SkyRenderer      Procedural sky rendering
        WaterSimulation  Water physics simulation
        WaterRenderer    Water surface rendering with OIT
        OITCompositor    Order-independent transparency (weighted blended)
        PostProcess      Bloom pipeline (bright extract, blur, tonemap)
        ParticleSystem   GPU particle emission and rendering
        ShadowSystem     Shadow map generation with texel snapping
        VertexPool       Shared vertex buffer management
        DebugDraw        bgfx debug overlay rendering (lines, shapes)

    Input
        InputManager     SDL3 events to Fabric Event dispatch, key bindings
        InputRouter      SDL3-to-RmlUI event forwarding, InputMode FSM
        InputRecorder    Input recording and playback

    Simulation
        SimulationHarness Tick-based field processor, named rules, deterministic ordering, double-buffer

    Persistence
        SaveManager      Game state save and load
        DataLoader       TOML-based data file loading
        FileWatcher      Hot-reload via efsw file system watcher
        SceneSerializer  Scene graph serialization

    Application State
        AppModeManager   Application mode FSM (game, editor, menu)
        DebrisPool       Pooled debris entity management

L4: Framework
    App loop         SDL3 events, fixed timestep, bgfx render submission
    SceneView        bgfx view owner, cull and render orchestration via Flecs queries
    RmlUi            BgfxRenderInterface (8 methods, view 255 UI overlay), FreeType font engine
    Plugin           Dependency-aware plugin loading, pass by reference
    ArgumentParser   Builder-pattern CLI argument parser with validation
    SyntaxTree/Token AST and tokenizer for config and data file parsing

L5: Application
    Fabric           Main executable: camera WASD, time controls, ECS world, RmlUi context
```

## Component Inventory

### Core (`include/fabric/core/`)

| Header | Purpose |
|--------|---------|
| `Animation.hh` | ozz-animation sampling, blending, and LocalToModel transform |
| `AnimationEvents.hh` | Clip markers with time-wrap processing and typed callbacks |
| `AppContext.hh` | Struct holding World, Timeline, EventDispatcher, ResourceHub references |
| `AppModeManager.hh` | Application mode FSM (game, editor, menu) with per-mode flag table |
| `Async.hh` | Standalone Asio io_context; `fabric::async::init()`, `poll()`, `run()`, `shutdown()`, `makeStrand()`, `makeTimer()`, `use_nothrow` |
| `AudioSystem.hh` | miniaudio spatial 3D audio, DDA occlusion through ChunkedGrid, SPSC ring buffer, sound categories and buses |
| `BehaviorAI.hh` | BehaviorTree.CPP NPC AI; patrol, chase, flee, attack behavior trees; perception queries; cached Flecs queries |
| `BgfxCallback.hh` | bgfx debug callback handler for logging and profiling |
| `BTDebugPanel.hh` | Behavior tree debug visualization panel (RmlUi) |
| `Camera.hh` | Projection and view matrix generation via bx::mtxProj with bgfx homogeneousDepth; perspective and ortho |
| `CameraController.hh` | First-person and third-person camera; DDA spring arm collision through ChunkedGrid; pitch clamp |
| `CaveCarver.hh` | Cellular automata cave generation on ChunkedGrid |
| `CharacterController.hh` | AABB ground collision with per-axis resolution, step-up, velocity integration |
| `CharacterTypes.hh` | Shared POD: CharacterState enum, Velocity, DashState, CharacterConfig |
| `ChunkedGrid.hh` | Sparse 32^3 chunk-based 3D grid template with cross-chunk neighbor access |
| `ChunkMeshManager.hh` | Dirty-chunk tracking via VoxelChanged event, budgeted re-mesh queue |
| `ChunkStreaming.hh` | Dynamic-radius chunk loading with speed-based prefetch, load/unload budgets |
| `Command.hh` | Execute/undo/redo command pattern with composite commands and history |
| `Component.hh` | Base component class with variant-based property storage, lifecycle methods, child management |
| `Constants.g.hh` | Generated constants (APP_NAME, APP_VERSION); output to build dir from `cmake/Constants.g.hh.in` |
| `ContentBrowser.hh` | Asset browser for runtime content inspection |
| `DashController.hh` | Burst movement with cooldown; ground dash and air boost variants |
| `DataLoader.hh` | TOML-based data file loading via toml++ |
| `DebrisPool.hh` | Pooled debris entity lifecycle management |
| `DebugDraw.hh` | bgfx debug overlay rendering (lines, boxes, spheres) via bgfx examples/common/debugdraw |
| `DevConsole.hh` | In-game developer console (RmlUi) |
| `ECS.hh` | Flecs v4.1.4 world wrapper; Position, Rotation, Scale, BoundingBox POD components; ChildOf hierarchy; CASCADE LocalToWorld |
| `EssencePalette.hh` | Continuous vec4 essence to discrete palette index quantization for greedy mesh merging |
| `Event.hh` | Thread-safe typed event handling with priority-sorted handlers, cancellation, propagation control, std::any payloads |
| `FieldLayer.hh` | Typed field over ChunkedGrid (DensityField, EssenceField type aliases); sample, fill, iterate |
| `FileWatcher.hh` | Hot-reload via efsw cross-platform file system watcher |
| `FlightController.hh` | 6DOF arcade flight with equal-priority axes and exponential drag |
| `IKSolver.hh` | Analytical two-bone inverse kinematics solver |
| `InputManager.hh` | SDL3 events to Fabric Event dispatch; WASD, mouse look, key bindings |
| `InputRecorder.hh` | Input event recording and deterministic playback |
| `InputRouter.hh` | SDL3-to-RmlUI event forwarding; InputMode FSM; ~45 key mappings |
| `JsonTypes.hh` | ADL-visible `to_json`/`from_json` for Vector2, Vector3, Vector4, Quaternion via nlohmann/json |
| `Lifecycle.hh` | StateMachine-backed component lifecycle (Created, Initialized, Rendered, Updating, Suspended, Destroyed) |
| `Log.hh` | Quill v11 wrapper; `fabric::log::init()`, `shutdown()`, `setLevel()`; FABRIC_LOG_{TRACE,DEBUG,INFO,WARN,ERROR,CRITICAL} macros |
| `LSystemVegetation.hh` | L-system procedural vegetation generation |
| `MaterialSounds.hh` | Material-based impact and interaction sound mapping |
| `MeleeSystem.hh` | Axis-aligned hitbox combat; AABB intersection, cooldown timer, damage events |
| `MeshLoader.hh` | fastgltf-based glTF 2.0 model loading (meshes, materials, textures) |
| `MovementFSM.hh` | 12-state movement FSM via StateMachine\<CharacterState\>; transition guards for each state pair |
| `OITCompositor.hh` | Order-independent transparency compositor (weighted blended); accumulation and composite passes |
| `ParticleSystem.hh` | GPU particle emission, simulation, and rendering |
| `Pathfinding.hh` | A* on ChunkedGrid density; 6-connected neighbors; maxNodes budget; PathFollower with seek/arrive steering |
| `PhysicsWorld.hh` | Jolt v5.5.0 rigid body dynamics; chunk collision shapes; debris constraints; ghost prevention |
| `Pipeline.hh` | Typed multi-stage data processing pipeline with fan-out, conditional stages, error handling |
| `Plugin.hh` | Dependency-aware plugin loading with resource management |
| `PostProcess.hh` | Bloom pipeline: bright extract, Gaussian blur, tonemap composite |
| `Ragdoll.hh` | Multi-joint capsule body ragdoll; fixed constraints; activate/deactivate on demand |
| `RenderCaps.hh` | Renderer capability detection and feature queries |
| `Rendering.hh` | AABB, Frustum, DrawCall, RenderList, FrustumCuller for scene rendering |
| `Resource.hh` | Resource base with state machine (Unloaded, Loading, Loaded, LoadingFailed, Unloading), dependency tracking |
| `ResourceHub.hh` | Centralized resource management; CoordinatedGraph-backed dependencies; worker threads; memory budgets; de-singletoned |
| `ReverbZone.hh` | Spatial reverb region definition and blending |
| `SaveManager.hh` | Game state serialization, save, and load |
| `SceneSerializer.hh` | Scene graph serialization and deserialization |
| `SceneView.hh` | bgfx view owner; Camera reference; ECS world; frustum cull and render list submission |
| `ShadowSystem.hh` | Shadow map generation with texel-snapping to prevent shimmer |
| `Simulation.hh` | Tick-based field processor with named rules, deterministic ordering, double-buffer mode |
| `SkinnedRenderer.hh` | GPU-skinned mesh rendering via bgfx; bone matrix upload; mesh buffer cache |
| `SkyRenderer.hh` | Procedural sky rendering (atmosphere, sun position) |
| `Spatial.hh` | Type-safe Vector2/3/4, Quaternion, Matrix4x4, Transform; compile-time coordinate space tags (Local, World, Screen, Parent); GLM bridge for `inverse()` |
| `StateMachine.hh` | Generic state machine template parameterized on enum; transition validation, guards, entry/exit actions, observers |
| `StructuralIntegrity.hh` | Voxel structural support analysis and collapse detection |
| `Temporal.hh` | Multi-timeline time processing with snapshots, variable time flow, region support |
| `TerrainGenerator.hh` | Procedural terrain via FastNoise2 (FBm, Cellular, DomainWarp) |
| `TransitionController.hh` | Ground-air transition with momentum preservation and ground scan |
| `Types.hh` | Core Variant (`nullptr_t, bool, int, float, double, string`), StringMap, Optional aliases |
| `VertexPool.hh` | Shared bgfx vertex buffer management and allocation |
| `VoxelInteraction.hh` | Create/destroy matter via raycast-to-field; fires VoxelChanged events |
| `VoxelMesher.hh` | Greedy meshing (Lysenko algorithm); per-vertex AO (3-neighbor, 4 levels); packed VoxelVertex (8 bytes); palette-based materials |
| `VoxelRaycast.hh` | DDA Amanatides-Woo voxel raycasting; castRay (first hit) and castRayAll (all intersections) |
| `VoxelRenderer.hh` | Chunk voxel mesh rendering with bgfx |
| `VoxelVertex.hh` | Packed 8-byte vertex layout: position (3x8), normal (3-bit), AO (2-bit), materialId (8-bit) |
| `WaterRenderer.hh` | Water surface rendering with OIT transparency |
| `WaterSimulation.hh` | Water physics simulation (flow, height field) |
| `WFCGenerator.hh` | Wave function collapse procedural structure generation |

### Codec (`include/fabric/codec/`)

| Header | Purpose |
|--------|---------|
| `Codec.hh` | Encode/decode pipeline for binary, text, and structured data; codec registry and chaining |

### Parser (`include/fabric/parser/`)

| Header | Purpose |
|--------|---------|
| `ArgumentParser.hh` | Builder-pattern CLI argument parser with named arguments, flags, and validation |
| `SyntaxTree.hh` | AST for config and data file parsing |
| `Token.hh` | Tokenizer with extensible type system for the parser |

### UI (`include/fabric/ui/`)

| Header | Purpose |
|--------|---------|
| `BgfxRenderInterface.hh` | RmlUi RenderInterface for bgfx (8 methods, view 255, premultiplied alpha, SPIR-V shaders) |
| `BgfxSystemInterface.hh` | RmlUi SystemInterface (logging bridge, elapsed time) |
| `DebugHUD.hh` | RmlUi-based debug overlay (FPS, memory, ECS stats) |
| `ToastManager.hh` | Notification toast display and lifecycle |
| `WebView.hh` | Embedded browser with JavaScript bridge (optional, via `FABRIC_USE_WEBVIEW`) |

### Utils (`include/fabric/utils/`)

| Header | Purpose |
|--------|---------|
| `BufferPool.hh` | Fixed-size buffer pool with thread-safe allocation, RAII handles, configurable block sizes |
| `BVH.hh` | Bounding volume hierarchy; median-split construction; AABB and frustum queries |
| `CoordinatedGraph.hh` | Thread-safe DAG with intent-based locking (Read, NodeModify, GraphStructure); deadlock detection; BFS/DFS/topological sort |
| `ErrorHandling.hh` | FabricException class, `throwError()`, `ErrorCode` enum (Ok, BufferOverrun, InvalidState, Timeout, etc.), `Result<T>` template |
| `ImmutableDAG.hh` | Lock-free persistent DAG with structural sharing, snapshot isolation, BFS/DFS/topological sort, LCA queries |
| `Profiler.hh` | Tracy v0.13.1 abstraction; FABRIC_ZONE_*, FABRIC_FRAME_*, FABRIC_ALLOC/FREE, FABRIC_LOCKABLE; compiles to nothing when off |
| `Testing.hh` | MockComponent, test utilities, helpers for concurrent test scenarios |
| `ThreadPoolExecutor.hh` | Thread pool with task submission, timeout support, testing mode (synchronous execution) |
| `TimeoutLock.hh` | Timeout-protected lock acquisition for shared_mutex and mutex types |
| `Utils.hh` | `generateUniqueId()` with thread-safe random hex generation |

### Source-only Files

| File | Purpose |
|------|---------|
| `src/core/Fabric.cc` | Main executable entry point: SDL3 init, bgfx init, ECS world, main loop |
| `src/core/MimallocOverride.cc` | Forces linker to pull mimalloc malloc/free/new/delete overrides; compiled into Fabric executable only |

## Dependencies

All dependencies are fetched via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) v0.42.1. Each library has a dedicated module in `cmake/modules/`. Set `CPM_SOURCE_CACHE=~/.cache/CPM` (configured in `mise.toml`) to share sources across builds.

| Dependency | Version | Module | Type | Purpose |
|------------|---------|--------|------|---------|
| [bgfx](https://github.com/bkaradzic/bgfx.cmake) | 1.139.9155-513 | FabricBgfx | Static | Vulkan rendering (SPIR-V shaders, MoltenVK on macOS) |
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.2 | (inline) | Static | Windowing, input, timers, native handles for bgfx |
| [Flecs](https://github.com/SanderMertens/flecs) | 4.1.4 | FabricFlecs | Static | Entity Component System (archetype SoA, query caching) |
| [RmlUi](https://github.com/mikke89/RmlUi) | 6.2 | FabricRmlUi | Static | In-game UI (HTML/CSS layout, bgfx RenderInterface) |
| [FreeType](https://github.com/freetype/freetype) | 2.14.1 | FabricRmlUi | Static (fallback) | Font rendering (fetched from source when system package not found) |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | 5.5.0 | FabricJolt | Static | Rigid body dynamics, collision shapes, ragdoll |
| [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP) | 4.8.4 | FabricBehaviorTree | Static | XML behavior trees, action/condition nodes |
| [ozz-animation](https://github.com/guillaumeblanc/ozz-animation) | 0.16.0 | FabricOzzAnimation | Static | SoA SIMD skeleton animation (sampling, blending, LocalToModel) |
| [fastgltf](https://github.com/spnda/fastgltf) | 0.9.0 | FabricFastgltf | Static | High-performance glTF 2.0 parser, C++20, zero-copy |
| [FastNoise2](https://github.com/Auburn/FastNoise2) | 1.1.1 | FabricFastNoise2 | Static | SIMD-accelerated noise (FBm, Cellular, DomainWarp) |
| [efsw](https://github.com/SpartanJ/efsw) | 1.5.1 | FabricEfsw | Static | Cross-platform file system watcher for hot-reload |
| [miniaudio](https://github.com/mackron/miniaudio) | 0.11.22 | FabricMiniaudio | Header only | Cross-platform audio, spatial 3D |
| [Quill](https://github.com/odygrd/quill) | 11.0.2 | FabricQuill | Static | Async structured logging (bundles fmtlib v12.1.0 as fmtquill) |
| [GLM](https://github.com/g-truc/glm) | 1.0.3 | FabricGLM | Header only | OpenGL Mathematics (vectors, matrices, quaternions) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | FabricNlohmannJson | Header only | JSON serialization for spatial types |
| [Standalone Asio](https://github.com/chriskohlhoff/asio) | 1.36.0 | FabricAsio | Header only | Async I/O with C++20 coroutines (standalone, no Boost) |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | FabricToml | Header only | TOML v1.0 parser for configuration and data files |
| [Tracy](https://github.com/wolfpld/tracy) | 0.13.1 | FabricTracy | Static (optional) | Frame profiler; only fetched when `FABRIC_ENABLE_PROFILING=ON` |
| [mimalloc](https://github.com/microsoft/mimalloc) | 2.2.7 | FabricMimalloc | Static (optional) | Allocator override; only fetched when `FABRIC_USE_MIMALLOC=ON` |
| [webview](https://github.com/webview/webview) | 0.12.0 | (inline) | Static | Embedded browser view, JS bridge |
| [GoogleTest](https://github.com/google/googletest) | 1.17.0 | FabricGoogleTest | Static (dev) | Testing framework (GTest + GMock) |

### Dependency Notes

- **mimalloc**: MI_OVERRIDE is OFF by default because the global malloc replacement conflicts with dlopen'd MoltenVK on macOS. Metal's internal allocators bypass mimalloc's malloc zone, causing crashes in mi_free_size.
- **Tracy**: conditionally fetched. When profiling is off, all FABRIC_ZONE_* and FABRIC_FRAME_* macros expand to nothing.
- **Asio**: standalone mode (ASIO_STANDALONE) with C++20 coroutine support (ASIO_HAS_CO_AWAIT, ASIO_HAS_STD_COROUTINE).
- **GLM**: configured with GLM_FORCE_RADIANS, GLM_FORCE_DEPTH_ZERO_TO_ONE, GLM_FORCE_SILENT_WARNINGS.
- **FreeType**: fetched from source only when the system package is not found. macOS and most Linux distributions use the system FreeType.
- **Jolt**: CPP_RTTI_ENABLED=ON is required. Fabric subclasses Jolt types (BroadPhaseLayerInterface, ContactListener); Linux linker rejects missing typeinfo without RTTI.

### CPM PATCHES

The `cmake/patches/` directory holds vendored patches applied via the CPM `PATCHES` argument during fetch.

| Patch | Target | Description |
|-------|--------|-------------|
| `bgfx-vk-suboptimal.patch` | bgfx | Treats VK_SUBOPTIMAL_KHR as non-fatal; prevents infinite swapchain recreation on MoltenVK |

Patches persist across builds (sources stay modified on disk) and re-apply on version bumps. Existing CPM caches populated before PATCHES was added require manual cache clearing for first application.

## Build System

CMake 3.25+ with target-based configuration throughout. Ninja is the default generator. FabricLib is a static library compiled once from all core, utils, parser, codec, and UI sources. All targets link against it.

### Build Targets

| Target | Type | Links | Description |
|--------|------|-------|-------------|
| `FabricLib` | Static library | SDL3, bgfx/bx/bimg, webview, GLM, Quill, nlohmann/json, Asio, RmlUi, Flecs, FastNoise2, fastgltf, ozz-animation, Jolt, BehaviorTree.CPP, miniaudio, toml++, efsw, Tracy (optional) | All core, utils, parser, codec, and UI sources |
| `Fabric` | Executable | FabricLib, mimalloc (optional) | Main application entry point |
| `UnitTests` | Executable | FabricLib, GTest, GMock, ozz_animation_offline | Unit test runner with custom TestMain |
| `E2ETests` | Executable | FabricLib, GTest, GMock, ozz_animation_offline | End-to-end test runner with custom TestMain |

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `FABRIC_BUILD_TESTS` | `ON` | Build UnitTests and E2ETests executables |
| `FABRIC_USE_WEBVIEW` | `ON` | Enable WebView support; defines `FABRIC_USE_WEBVIEW` preprocessor symbol |
| `FABRIC_BUILD_UNIVERSAL` | `OFF` | Build universal binaries (arm64 + x86_64), macOS only |
| `FABRIC_USE_MIMALLOC` | `OFF` | Link mimalloc as global allocator override; OFF by default (MI_OVERRIDE conflicts with MoltenVK) |
| `FABRIC_ENABLE_PROFILING` | `OFF` | Enable Tracy profiler instrumentation; defines `FABRIC_PROFILING_ENABLED` |

### Shader Compilation

Seven CMake modules compile `.sc` shader sources to embedded `.bin.h` headers using bgfx shaderc. All shaders compile to SPIR-V only (platform argument: `linux`, profile: `spirv`). Output headers use the `BGFX_EMBEDDED_SHADER` macro.

| Module | Shaders | Purpose |
|--------|---------|---------|
| FabricRmlUi (shaders) | vs_rmlui, fs_rmlui | UI overlay rendering |
| FabricSkinnedShaders | vs_skinned, fs_skinned | GPU skeletal mesh skinning |
| FabricVoxelShaders | vs_voxel, fs_voxel | Voxel chunk terrain |
| FabricSkyShaders | vs_sky, fs_sky | Procedural sky |
| FabricPostShaders | vs_fullscreen, fs_bright, fs_blur, fs_tonemap | Bloom pipeline |
| FabricParticleShaders | vs_particle, fs_particle | Particle rendering |
| FabricWaterShaders | vs_water, fs_water | Water surface |
| FabricOITShaders | vs_oit_accum, fs_oit_accum, vs_oit_composite, fs_oit_composite | Order-independent transparency |

Shader source files live in `shaders/<category>/` with a `varying.def.sc` per category. FabricLib depends on all shader targets so compilation finishes before source files compile.

### Generated Files

`cmake/Constants.g.hh.in` is processed at configure time to produce `${CMAKE_BINARY_DIR}/include/fabric/core/Constants.g.hh`, containing `APP_NAME` and `APP_VERSION` from the CMake project definition. The build directory include path takes precedence over the source tree copy.

## Testing

GoogleTest 1.17.0 for all tests. 1429+ tests across 111+ suites, organized into unit and E2E categories. Both test executables use a custom `tests/TestMain.cc` that initializes Quill logging before test execution. See [TESTING.md](TESTING.md) for test conventions, suite inventory, and patterns.

## Platform Support

| Platform | Minimum | Compiler | Notes |
|----------|---------|----------|-------|
| macOS | 15.0+ | Apple Clang | Xcode CLT; Cocoa, WebKit, Metal, QuartzCore frameworks; Vulkan via MoltenVK |
| Linux | Recent kernel | GCC 10+ or Clang 13+ | webkit2gtk-4.1 (falls back to 4.0); Vulkan SDK and GPU drivers |
| Windows | 10+ | MSVC 19.29+ | Windows 10 SDK; D3D12, DXGI, shlwapi, version libs; Vulkan drivers |

### Platform Contracts

The Vulkan-only backend imposes constraints that must be satisfied at runtime.

| Contract | Requirement | Consequence of violation |
|----------|-------------|------------------------|
| SDL window flags | `SDL_WINDOW_VULKAN` on SDL3 window creation | bgfx cannot create a Vulkan surface |
| HiDPI reset flag | `BGFX_RESET_HIDPI` on both `bgfx::init()` and every `bgfx::reset()` | Half-resolution rendering on Retina/HiDPI |
| Single-threaded init | `bgfx::renderFrame()` before `bgfx::init()` on macOS | Render thread deadlock with Metal/MoltenVK |
| MoltenVK runtime | macOS: `brew install molten-vk vulkan-loader`; BUILD_RPATH set to `/opt/homebrew/lib` | Vulkan loader not found at runtime |
| VK_SUBOPTIMAL_KHR | Patched via CPM PATCHES (`bgfx-vk-suboptimal.patch`) | Window resize or display switch crashes |

## Known Issues

1. **Temporal scaffolding**: approximately 10-15% remaining unused scaffolding. Core timeline and snapshot functionality works. Generic Interpolator template and makeTimeBehavior factory are functional but underused.
2. **OIT double-init**: OITCompositor initialized twice at startup (duplicate bgfx uniforms). Likely a spurious SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED on the first frame.
3. **Missing .rml assets**: DebugHUD, BTDebugPanel, DevConsole fail to load .rml files at runtime. Build runs from `build/dev-debug/bin/` but assets are in source tree.
4. **Mesh buffer cache key**: SkinnedRenderer uses raw mesh pointer as cache key. Vulnerable to use-after-free if MeshData relocates. Needs a stable mesh ID.
5. **miniaudio resource manager UAF**: v0.11.22 has heap-use-after-free when loading nonexistent files in headless mode. Workaround: std::filesystem::exists() pre-check in executePlay.
6. **No runtime configuration**: Window size, clear color, reset flags, view IDs, initial AppMode, and physics timestep are all hardcoded in Fabric.cc. toml++ is a dependency but unused for engine config. Runtime config is designed for Sprint 15.
