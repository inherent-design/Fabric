# API Surface Map

This document is a current-state API map, not a promise that every included header is frozen. Its job is to show which headers are the practical engine-facing surface today, which surfaces are still evolving, and which Recurse headers are game-specific rather than reusable engine API.

## Scope and status

- `fabric::` is the intended reusable engine surface
- `recurse::` is the current game layer and should not be treated as stable engine API
- some engine headers are mature and widely used today
- some headers are forward-looking scaffolding for the longer-term ops-as-values and type-state direction
- the long-term API goal is multi-project readiness, not a Fabric API that only works for Recurse

## Minimal bootstrap for an app

A new app built on the current engine shape typically starts from these headers:

| Header | Role |
|--------|------|
| `fabric/platform/FabricApp.hh` | App entry point and main loop owner |
| `fabric/platform/FabricAppDesc.hh` | System registration and lifecycle callbacks |
| `fabric/core/SystemBase.hh` | CRTP base for systems |
| `fabric/core/SystemPhase.hh` | Phase selection for system registration |
| `fabric/core/AppContext.hh` | Per-frame context passed to systems |
| `fabric/log/Log.hh` | Structured logging macros |

## Current engine-facing surface by subsystem

| Subsystem | Notable headers | Current status |
|-----------|-----------------|----------------|
| `fabric/core` | `AppContext.hh`, `AppModeManager.hh`, `Spatial.hh`, `StateMachine.hh`, `SystemBase.hh`, `SystemPhase.hh`, `RuntimeState.hh`, `JsonTypes.hh` | Core bootstrap and utility surface. `RuntimeState.hh` is more transitional than the others. |
| `fabric/ecs` | `ECS.hh`, `Component.hh`, `WorldScoped.hh` | Reusable, but still influenced by current world-lifecycle patterns. |
| `fabric/fx` | `Error.hh`, `Never.hh`, `OneOf.hh`, `Result.hh`, `WorldContext.hh`, `WorldOps.hh`, `SpatialDataOp.hh`, `WorldQueryProvider.hh` | Important evolving surface. This is where future executor-style work is incubating. |
| `fabric/input` | `InputAction.hh`, `InputAxis.hh`, `InputContext.hh`, `InputManager.hh`, `InputRouter.hh`, `InputSource.hh`, `InputSystem.hh` | Mature, app-facing input surface. |
| `fabric/log` | `Log.hh`, `LogConfig.hh`, `FilteredConsoleSink.hh` | Stable in daily use, though sink internals are less reusable than the macros and config surface. |
| `fabric/platform` | `FabricApp.hh`, `FabricAppDesc.hh`, `ConfigManager.hh`, `Async.hh`, `JobScheduler.hh`, `ScopedTaskGroup.hh`, `WriterQueue.hh`, `WindowDesc.hh`, `PlatformInfo.hh`, `CursorManager.hh`, `DefaultConfig.hh` | Practical bootstrap and runtime surface. Some helpers remain implementation-heavy. |
| `fabric/render` | `Camera.hh`, `DrawCall.hh`, `Geometry.hh`, `SceneView.hh`, `PostProcess.hh`, `PaniniPass.hh`, `RenderCaps.hh`, `BgfxHandle.hh`, `HandleMap.hh`, `BgfxCallback.hh`, `FullscreenQuad.hh` | Real engine surface, but still tightly shaped by current bgfx and Recurse usage. |
| `fabric/resource` | `Resource.hh`, `ResourceHub.hh`, `AssetLoader.hh`, `AssetRegistry.hh`, `Handle.hh` | Reusable subsystem with active test coverage. |
| `fabric/ui` | `RmlPanel.hh`, `ToastManager.hh`, `WebView.hh`, `BgfxRenderInterface.hh`, `BgfxSystemInterface.hh`, `ConcurrencyPanel.hh`, `HotkeyPanel.hh` | Mixed surface: some headers are reusable panel abstractions, others are backend plumbing. |
| `fabric/utils` | `ErrorHandling.hh`, `Profiler.hh`, `Testing.hh`, `TextSanitize.hh`, `BVH.hh`, `CoordinatedGraph*.hh`, `Utils.hh` | Utility surface with varying stability. `Profiler.hh` and `ErrorHandling.hh` are the most generally reusable. |
| `fabric/world` | `ChunkCoord.hh`, `ChunkCoordUtils.hh`, `ChunkedGrid.hh` | Core spatial world helpers. `ChunkedGrid.hh` is useful today but still carries assumptions from the current voxel-heavy usage. |

## Evolving surfaces to treat carefully

The following surfaces are real and used, but they are still expected to move as the architecture matures:

- `fabric::fx::WorldContext` and related op types
- `fabric::core::RuntimeState`
- `fabric::ecs::WorldScoped`
- `fabric::world::ChunkedGrid`
- any surface whose current shape is heavily influenced by Recurse's voxel world

## Recurse-specific and experimental surfaces

These are important to the repository, but they are not generic engine API.

### Game-layer headers

`include/recurse/` contains the current game domain: simulation, persistence, world generation, gameplay, render systems, audio, AI, UI, and benchmark startup support.

Examples of intentionally game-specific surfaces:

- `recurse/BenchmarkStartup.hh`
- `recurse/persistence/*`
- `recurse/systems/*`
- `recurse/world/*`
- `recurse/simulation/*`

### Goal #4 and meshing rollout surfaces

A few Recurse headers are especially relevant to the active roadmap:

- `recurse/world/MesherInterface.hh`
- `recurse/world/FunctionContracts.hh`
- `recurse/simulation/VoxelSemanticView.hh`

These are evolving rollout surfaces for the combined Goal #4 plus meshing checkpoint sequence. They should be treated as important, but not frozen.

## Current production posture

API decisions should preserve the current shipped stance:

- Greedy meshing is the primary near-path production contract
- SnapMC is optional and experimental behind the pluggable mesher boundary
- the visual direction remains visibly voxel, not a forced smooth-surface conversion

## Near-term API direction

In the short term, expect the API surface to shift in ways that support:

1. Greedy-path instrumentation and output cleanup
2. semantic-query adapter introduction without behavior drift
3. semantic boundary-change invalidation
4. LOD policy alignment with the same semantic authority
5. continued cleanup of engine versus game ownership

The goal is not to churn public seams for style reasons. It is to make current production paths clearer and safer while the engine boundary improves.

## Long-term direction

Over the longer term, the repository aims for:

- a cleaner `fabric::` API that another game can consume directly
- ops-as-values and centralized execution for world access
- type-state at important boundaries
- a stronger distinction between reusable engine API and Recurse-only gameplay API
- generated API docs later, once the curated surface is stable enough to publish

That work is part of the larger multi-project readiness push: `fabric::` should become a clean engine boundary that additional games can adopt without inheriting Recurse-specific world or meshing assumptions.
