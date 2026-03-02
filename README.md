# Fabric Engine

C++20 cross-platform runtime for building interactive spatial-temporal applications. Time and space are first-class primitives; applications compose programming primitives (math, time, events, commands) into structural primitives (scenes, entities, graphs, simulations).

> **Work in Progress.** APIs may change between sprints.

## Systems

### L0: Platform Abstraction

- **SDL3**: windowing, input, timers, filesystem, native handles for bgfx
- **bgfx**: rendering backend (Vulkan-only, SPIR-V shaders, MoltenVK on macOS)
- **WebView**: embedded browser with JavaScript bridge (optional, via `FABRIC_USE_WEBVIEW`)
- **mimalloc**: memory allocator, available for explicit use (global override off by default)

### L1: Infrastructure

- **Log**: async structured logging via Quill (FABRIC_LOG_* macros, compile-time filtering)
- **Profiler**: Tracy abstraction (FABRIC_ZONE_SCOPED, zero-cost when disabled)
- **Async**: Standalone Asio io_context with C++20 coroutine support
- **ErrorHandling**: FabricException, ErrorCode enum, Result\<T\> for error-code returns
- **StateMachine**: generic state machine template with transition guards and observers

### L2: Primitives

- **Spatial**: type-safe vector, matrix, quaternion, and transform operations with coordinate space tags (GLM bridge for matrix inverse)
- **Temporal**: multi-timeline time processing with variable flow and snapshots
- **Event**: typed thread-safe publish/subscribe with propagation control
- **Command**: execute, undo, redo with composite commands and history
- **Pipeline**: typed multi-stage data processing with fan-out and conditional stages
- **Types**: core type definitions (Variant, StringMap, Optional)
- **JsonTypes**: nlohmann/json serializers for Vector, Quaternion, and spatial types
- **Codec**: encode/decode framework for binary, text, and structured data

### L3: Structural

- **ECS**: Flecs world wrapper with ChildOf hierarchy, CASCADE queries, and LocalToWorld transform composition
- **Component**: type-safe component architecture with variant-based properties
- **Resource / ResourceHub**: resource state machine with dependency tracking, worker threads, and memory budgets
- **Lifecycle**: StateMachine-backed component lifecycle transitions
- **Camera**: projection and view matrix generation (bx::mtxProj, homogeneousDepth, perspective and ortho)
- **Rendering**: AABB, Frustum, DrawCall, RenderList, FrustumCuller
- **SceneSerializer**: scene graph serialization
- **DebrisPool**: pooled debris entity management

#### Voxel World

- **ChunkedGrid**: sparse 32^3 chunk-based 3D grid template
- **FieldLayer**: typed field over ChunkedGrid (DensityField, EssenceField)
- **VoxelMesher**: greedy meshing with per-vertex AO, packed 8-byte VoxelVertex, palette-based materials
- **VoxelRaycast**: DDA Amanatides-Woo algorithm (castRay, castRayAll)
- **VoxelRenderer**: chunk rendering with bgfx
- **ChunkStreaming**: dynamic radius with speed-based prefetch, load/unload budgets
- **ChunkMeshManager**: dirty-chunk tracking via VoxelChanged event, re-mesh queue
- **TerrainGenerator**: procedural terrain via FastNoise2
- **CaveCarver**: cellular automata cave generation
- **EssencePalette**: continuous-to-discrete material quantization
- **LSystemVegetation**: L-system procedural vegetation
- **WFCGenerator**: wave function collapse structure generation

#### Character and Movement

- **CharacterController**: AABB ground collision with per-axis resolution and step-up
- **FlightController**: 6DOF arcade flight with exponential drag
- **MovementFSM**: 12-state machine via StateMachine\<CharacterState\> with transition guards
- **TransitionController**: ground-air momentum preservation with ground scan
- **DashController**: burst movement with cooldown (ground dash, air boost)
- **CameraController**: first-person and third-person with DDA spring arm collision
- **VoxelInteraction**: create/destroy matter via raycast-to-field, VoxelChanged events
- **MeleeSystem**: axis-aligned hitbox with AABB intersection, cooldown, and damage events

#### Physics

- **PhysicsWorld**: Jolt rigid body dynamics with chunk collision and ghost prevention
- **Ragdoll**: multi-joint capsule bodies with fixed constraints, activate/deactivate

#### AI

- **BehaviorAI**: BehaviorTree.CPP NPC behavior (patrol, chase, flee, attack) with perception queries
- **Pathfinding**: A* on ChunkedGrid density, 6-connected, maxNodes budget, PathFollower seek/arrive

#### Audio

- **AudioSystem**: miniaudio spatial 3D with DDA occlusion, SPSC ring buffer, sound categories and buses
- **ReverbZone**: spatial reverb regions
- **MaterialSounds**: material-based impact sounds
- **AnimationEvents**: clip markers with time-wrap processing and typed callbacks

#### Animation

- **Animation**: ozz-animation sampling, blending, and LocalToModel
- **SkinnedRenderer**: GPU-skinned mesh rendering with bgfx
- **IKSolver**: analytical two-bone inverse kinematics
- **MeshLoader**: fastgltf-based glTF 2.0 model loading

#### Rendering Pipeline

- **SkyRenderer**: procedural sky rendering
- **WaterSimulation**: water physics simulation
- **WaterRenderer**: water surface rendering with OIT
- **OITCompositor**: order-independent transparency (weighted blended)
- **PostProcess**: bloom (bright extract, blur, tonemap)
- **ParticleSystem**: GPU particle rendering
- **ShadowSystem**: shadow map generation with texel snapping
- **VertexPool**: shared vertex buffer management
- **DebugDraw**: bgfx debug overlay rendering

#### Input and UI

- **InputRouter**: SDL3-to-RmlUI event forwarding with InputMode FSM
- **InputManager**: SDL3 events to Fabric Event dispatch, key bindings
- **InputRecorder**: input recording and playback
- **DebugHUD**: RmlUi-based debug overlay
- **BTDebugPanel**: behavior tree debug visualization
- **DevConsole**: in-game developer console
- **ContentBrowser**: asset browser
- **ToastManager**: notification toast display

#### Simulation

- **SimulationHarness**: tick-based field processor with named rules, deterministic ordering, double-buffer mode

#### Persistence

- **SaveManager**: game state save/load
- **DataLoader**: TOML-based data file loading
- **FileWatcher**: hot-reload via efsw file system watcher

#### Application

- **AppModeManager**: application mode state machine (game, editor, menu)
- **SceneView**: bgfx view owner, cull and render orchestration via Flecs queries

### L4: Framework

- **App loop**: SDL3 events, fixed timestep, bgfx render submission
- **Plugin**: dependency-aware plugin loading
- **ArgumentParser**: builder-pattern CLI argument parser with validation
- **SyntaxTree / Token**: AST and tokenizer for config and data parsing

## Building

### Prerequisites

- [mise](https://mise.jdx.dev/) (manages cmake + ninja)
- C++20 compiler (Apple Clang, GCC 10+, Clang 13+, MSVC 19.29+)
- Vulkan SDK or MoltenVK (macOS: `brew install molten-vk vulkan-loader`)

### Build and Test

```bash
mise install            # Install tooling
mise run build          # Debug build
mise run build:release  # Release build
mise run test           # Unit tests
mise run test:e2e       # E2E tests
mise run test:all       # Unit + E2E
```

Or with CMake presets:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
```

## Dependencies

All dependencies are fetched via CPM.cmake v0.42.1. Each library has a dedicated module under `cmake/modules/`.

| Dependency | Version | Type | Purpose |
|------------|---------|------|---------|
| SDL3 | 3.4.2 | static | Windowing, input, timers, native handles |
| bgfx (bx, bimg) | 1.139.9155-513 | static | Rendering backend (Vulkan-only, SPIR-V) |
| Flecs | 4.1.4 | static | Entity component system (archetype SoA) |
| RmlUi | 6.2 | static | In-game HTML/CSS UI layout engine |
| FreeType | 2.14.1 | static (fallback) | Font rendering (required by RmlUi) |
| Jolt Physics | 5.5.0 | static | Rigid body dynamics, collision, ragdoll |
| BehaviorTree.CPP | 4.8.4 | static | AI behavior trees |
| ozz-animation | 0.16.0 | static | SoA SIMD skeletal animation runtime |
| fastgltf | 0.9.0 | static | glTF 2.0 model parser |
| FastNoise2 | 1.1.1 | static | SIMD noise generation for terrain |
| efsw | 1.5.1 | static | File system watcher for hot-reload |
| GLM | 1.0.3 | header-only | Matrix and vector math |
| Standalone Asio | 1.36.0 | header-only | Async I/O, C++20 coroutines |
| nlohmann/json | 3.12.0 | header-only | JSON serialization for spatial types |
| toml++ | 3.4.0 | header-only | TOML v1.0 parser for data files |
| miniaudio | 0.11.22 | header-only | Cross-platform spatial audio |
| Quill | 11.0.2 | static | Async structured logging (FABRIC_LOG_*) |
| Tracy | 0.13.1 | static (optional) | Frame profiling (FABRIC_ENABLE_PROFILING) |
| mimalloc | 2.2.7 | static (optional) | Memory allocator (MI_OVERRIDE off) |
| webview | 0.12.0 | static | Embedded browser, JS bridge |
| GoogleTest | 1.17.0 | static (dev) | Unit and E2E testing |

## Platform Support

| Platform | Minimum | Notes |
|----------|---------|-------|
| macOS | 15.0+ | Xcode CLT, Vulkan via MoltenVK (`brew install molten-vk vulkan-loader`) |
| Linux | Recent kernel | Vulkan SDK, webkit2gtk-4.1 (or 4.0 fallback) |
| Windows | 10+ | MSVC 2022, Windows 10 SDK, Vulkan drivers |

Rendering uses Vulkan on all platforms. macOS requires MoltenVK to translate Vulkan calls to Metal.

## Project Structure

```
fabric/
├── include/fabric/
│   ├── core/           # ECS, Component, Event, Lifecycle, Resource, Temporal,
│   │                   # Spatial, Camera, Rendering, Physics, Audio, AI, Animation,
│   │                   # Voxel, Water, Particles, Input, StateMachine, Log, Async
│   ├── parser/         # ArgumentParser, SyntaxTree, Token
│   ├── codec/          # Codec
│   ├── ui/             # WebView, BgfxRenderInterface, BgfxSystemInterface
│   └── utils/          # BVH, BufferPool, CoordinatedGraph, ImmutableDAG,
│                       # ErrorHandling, Profiler, ThreadPoolExecutor, TimeoutLock
├── src/
│   ├── core/           # 65 source files
│   ├── parser/         # 2 source files
│   ├── codec/          # 1 source file
│   ├── ui/             # 5 source files
│   └── utils/          # 4 source files
├── shaders/
│   ├── rmlui/          # UI overlay shaders
│   ├── voxel/          # Chunk terrain shaders
│   ├── skinned/        # GPU skinning shaders
│   ├── sky/            # Procedural sky shaders
│   ├── post/           # Bloom (bright, blur, tonemap)
│   ├── particle/       # Particle rendering shaders
│   ├── water/          # Water surface shaders
│   └── oit/            # Order-independent transparency shaders
├── cmake/
│   ├── CPM.cmake       # CPM.cmake v0.42.1
│   ├── modules/        # 25 dependency and shader modules
│   └── patches/        # Vendored dependency patches
├── tests/
│   ├── unit/           # Per-component unit tests
│   ├── e2e/            # End-to-end tests
│   └── TestMain.cc     # Shared test main (Quill init)
├── tasks/              # POSIX shell scripts for mise
├── assets/             # Runtime assets (UI templates, styles)
├── docs/               # Architecture, build, testing guides
├── CMakeLists.txt      # Build config (FabricLib static library)
├── CMakePresets.json    # 11 presets (1 hidden base, 10 visible)
└── mise.toml           # Task runner config
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Build Guide](docs/BUILD.md)
- [Testing Guide](docs/TESTING.md)
- [Contributing](CONTRIBUTING.md)

## License

[MIT License](LICENSE)
