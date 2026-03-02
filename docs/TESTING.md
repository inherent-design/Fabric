# Testing

## Overview

Fabric uses GoogleTest 1.17.0 for all testing, organized into unit and E2E categories. As of Sprint 14 there are 1429+ tests across 111+ suites. Use `mise run test` and `mise run test:e2e` (or `mise run test:all`) to confirm current totals on your branch.

A custom `TestMain.cc` initializes Quill logging before test execution.

## Verification Commands

```bash
mise run build
mise run test
mise run test:e2e
mise run test:all
```

Use `mise run test:filter <Pattern>` for targeted checks while iterating.

```bash
mise run test:filter SpatialTest
```

These commands mirror `mise.toml` task definitions and are the source of truth for this repository.


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

### Unit Tests: `codec/`

| File | Component |
|------|-----------|
| `codec/CodecTest.cc` | Encode/decode framework |

### Unit Tests: `core/`

| File | Component |
|------|-----------|
| `core/AnimationEventsTest.cc` | Animation event dispatching |
| `core/AnimationSamplerTest.cc` | Keyframe sampling and interpolation |
| `core/AnimationTest.cc` | Skeletal animation playback |
| `core/AppModeManagerTest.cc` | App mode transitions, flag lookup |
| `core/AudioSystemTest.cc` | Audio playback and spatial sound |
| `core/BehaviorAITest.cc` | Behavior tree AI logic |
| `core/BTDebugPanelTest.cc` | Behavior tree debug panel |
| `core/BVHVisitorTest.cc` | BVH traversal visitors |
| `core/CameraControllerTest.cc` | Camera input and movement |
| `core/CameraTest.cc` | Projection, view matrix, bgfx compat |
| `core/CaveCarverTest.cc` | Procedural cave generation |
| `core/CCDTest.cc` | Continuous collision detection |
| `core/CharacterControllerTest.cc` | Character physics and movement |
| `core/ChunkedGridTest.cc` | Sparse 32^3 chunk storage, neighbors |
| `core/ChunkMeshManagerTest.cc` | Chunk mesh lifecycle |
| `core/ChunkStreamingTest.cc` | Async chunk load/unload |
| `core/CommandTest.cc` | Command pattern (execute, undo, redo) |
| `core/ComponentTest.cc` | Component properties and hierarchy |
| `core/ContentBrowserTest.cc` | Asset browser UI |
| `core/CoreApiTest.cc` | Cross-component API interactions |
| `core/DashControllerTest.cc` | Dash movement mechanics |
| `core/DataLoaderTest.cc` | Asset data loading pipeline |
| `core/DebrisPoolTest.cc` | Debris object pooling |
| `core/DebugDrawTest.cc` | Debug line/shape rendering |
| `core/DevConsoleTest.cc` | Developer console commands |
| `core/ECSRestTest.cc` | ECS REST API endpoints |
| `core/ECSTest.cc` | Flecs world, ChildOf, CASCADE, LocalToWorld |
| `core/EssencePaletteTest.cc` | Essence palette system |
| `core/EventTest.cc` | Event dispatching and propagation |
| `core/FabricRuntimeFlowTest.cc` | Runtime initialization flow |
| `core/FieldLayerTest.cc` | Typed field read/write/sample/fill |
| `core/FileWatcherTest.cc` | File change notification |
| `core/FlightControllerTest.cc` | Flight movement mode |
| `core/IKSolverTest.cc` | Inverse kinematics solver |
| `core/InputManagerTest.cc` | SDL3 event mapping, key bindings |
| `core/InputRecorderTest.cc` | Input recording and playback |
| `core/InputRouterTest.cc` | Input event routing and priorities |
| `core/JsonTypesTest.cc` | nlohmann/json serializers for Vector, Quaternion |
| `core/LifecycleTest.cc` | State machine transitions |
| `core/LSystemVegetationTest.cc` | L-system procedural vegetation |
| `core/MaterialSoundsTest.cc` | Material-based sound effects |
| `core/MeleeSystemTest.cc` | Melee combat mechanics |
| `core/MeshLoaderTest.cc` | Mesh file import |
| `core/MovementFSMTest.cc` | Movement state machine |
| `core/OITCompositorTest.cc` | Order-independent transparency compositing |
| `core/ParticleSystemTest.cc` | Particle emission and simulation |
| `core/PathfindingTest.cc` | Pathfinding algorithms |
| `core/PhysicsWorldTest.cc` | Physics world simulation |
| `core/PipelineTest.cc` | Multi-stage data processing |
| `core/PluginTest.cc` | Plugin loading and dependencies |
| `core/PostProcessTest.cc` | Post-processing effects |
| `core/RagdollTest.cc` | Ragdoll physics |
| `core/RenderCapsTest.cc` | Renderer capability queries |
| `core/RenderingTest.cc` | AABB, Frustum, DrawCall, RenderList |
| `core/ResourceHubTest.cc` | Resource management and caching |
| `core/ResourceTest.cc` | Resource lifecycle and dependencies |
| `core/ReverbZoneTest.cc` | Audio reverb zones |
| `core/SaveManagerTest.cc` | Save/load game state |
| `core/SceneSerializerTest.cc` | Scene serialization and deserialization |
| `core/SceneViewTest.cc` | Cull + render pipeline, Flecs queries |
| `core/ShadowSystemTest.cc` | Shadow mapping |
| `core/SimulationTest.cc` | Tick-based rules, deterministic ordering |
| `core/SkinnedRendererTest.cc` | Skinned mesh rendering |
| `core/SkyRendererTest.cc` | Sky and atmosphere rendering |
| `core/SpatialTest.cc` | Vector ops, coordinate transforms, GLM bridge |
| `core/Sprint7StubTest.cc` | Sprint 7 placeholder validations |
| `core/StateMachineTest.cc` | Generic state machine template |
| `core/StructuralIntegrityTest.cc` | Voxel structural integrity |
| `core/TemporalTest.cc` | Timeline and time processing |
| `core/TerrainGeneratorTest.cc` | Terrain generation pipeline |
| `core/TransitionControllerTest.cc` | State transition controller |
| `core/VertexPoolTest.cc` | Vertex buffer pooling |
| `core/VoxelInteractionTest.cc` | Voxel placement and removal |
| `core/VoxelMesherLODTest.cc` | LOD-aware voxel meshing |
| `core/VoxelMesherTest.cc` | Block meshing, hidden face culling |
| `core/VoxelRaycastTest.cc` | Voxel raycasting |
| `core/VoxelRendererTest.cc` | Voxel render pipeline |
| `core/VulkanRegressionTest.cc` | Vulkan/MoltenVK regression checks |
| `core/WaterMeshTest.cc` | Water surface mesh generation |
| `core/WaterRendererTest.cc` | Water render pipeline |
| `core/WaterSimulationTest.cc` | Water physics simulation |
| `core/WFCGeneratorTest.cc` | Wave function collapse generation |

### Unit Tests: `parser/`

| File | Component |
|------|-----------|
| `parser/ArgumentParserTest.cc` | CLI argument parsing |

### Unit Tests: `ui/`

| File | Component |
|------|-----------|
| `ui/DebugHUDPerfTest.cc` | Debug HUD performance |
| `ui/DebugHUDTest.cc` | Debug HUD display and toggles |
| `ui/RmlUiBackendTest.cc` | RmlUi backend integration |
| `ui/ToastManagerTest.cc` | Toast notification system |
| `ui/WebViewTest.cc` | WebView and JS bridge |

### Unit Tests: `utils/`

| File | Component |
|------|-----------|
| `utils/BufferPoolTest.cc` | Fixed-size pool, RAII handles |
| `utils/BVHTest.cc` | Bounding volume hierarchy, frustum queries |
| `utils/CoordinatedGraphTest.cc` | Graph operations and locking |
| `utils/ErrorHandlingTest.cc` | Error utilities |
| `utils/ImmutableDAGTest.cc` | Lock-free persistent DAG |
| `utils/LoggingTest.cc` | Quill logging macros (FABRIC_LOG_*) |
| `utils/UtilsTest.cc` | String utils, UUID generation |

### E2E Tests (`tests/e2e/`)

| File | Coverage |
|------|----------|
| `FabricE2ETest.cc` | Full application execution with CLI parameters |

## Vulkan Regression Tests

`VulkanRegressionTest.cc` validates Vulkan/MoltenVK correctness after the Sprint 14 renderer migration. These tests cover:

- **Shader profiles:** SPIRV is the only enabled profile; all others (DXBC, ESSL, Metal, etc.) are suppressed
- **View ID conflicts:** all bgfx view IDs are unique and ordered correctly (sky < geometry < transparent < particles < post-process < OIT < UI)
- **OIT compositor guards:** uninitialized compositor is not valid; composite view follows accumulation view; no overlap with post-process range
- **VoxelVertex format:** 8-byte packed layout, pack/unpack round-trip, stride matches layout, attributes use TexCoord0/1 (not Attrib::Position)
- **PostProcess guards:** disabled by default; render without init is a no-op
- **GPU timer sanitization:** clamps garbage timestamps from MoltenVK (negative values, overflow)
- **Runtime tests:** GPU-dependent checks (renderer type, homogeneous depth, OIT behavior) skip via `GTEST_SKIP` when no bgfx context is available

Run with:

```bash
mise run test:filter VulkanRegression
```

Most tests validate constraints statically without a live Vulkan device. Runtime tests that require a bgfx context are skipped in CI.

## Test Infrastructure

### Custom Test Main (`tests/TestMain.cc`)

The test binary uses a custom `main()` that:

1. Initializes Quill logging via `fabric::log::init()`
2. Runs all GoogleTest tests
3. Shuts down logging via `fabric::log::shutdown()`

ThreadPoolExecutor is paused during test execution to prevent background threads from interfering with deterministic test behavior.

### Disabled Tests

Some tests may be disabled (prefixed with `DISABLED_`). Use `--gtest_also_run_disabled_tests` to include them when needed.

## Naming Conventions

- Unit tests: `ComponentNameTest.cc`
- E2E tests: `FeatureE2ETest.cc`
- Regression tests: `SubsystemRegressionTest.cc`

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
