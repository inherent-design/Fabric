# Fabric Engine

Fabric is a C++20 engine repository with two active layers:

- `fabric::`: the reusable engine runtime built as `FabricLib`
- `recurse::`: the current game built as `RecurseGame` and linked into the `Recurse` executable

This repository is currently centered on Recurse's voxel world, persistence, rendering, and profiling workflows, while the engine continues moving toward a cleaner multi-project boundary.

## Current repository state

### What ships today

- CMake + mise based build, test, lint, profiling, and analysis workflows
- Vulkan-only rendering through bgfx, with MoltenVK on macOS
- `FabricLib` static library plus `RecurseGame` object library
- `Recurse`, `UnitTests`, and `E2ETests` targets
- SQLite-backed per-world persistence with WAL, chunk snapshots, replay, and pruning
- Chunk streaming, simulation, near chunk meshing, far LOD rendering, and debug UI panels
- Benchmark automation startup plumbing and in-game benchmark entry points

### Voxel rendering posture

- Greedy meshing is the primary near-path production contract
- The intended shipped look is visibly voxel terrain with lighting, AO, fog, bloom, and composition doing the smoothing work
- SnapMC remains available only as an optional, experimental, or rollback mesher behind the pluggable mesher boundary

## Near-term roadmap

The current documentation and implementation wave is organized around the combined Goal #4 plus meshing sequence.

1. **Checkpoint 0**: measure current Greedy production-path subphases inside `generateMeshCPU()`
2. **Checkpoint 1**: remove the Greedy smooth-intermediate repack tax by emitting the packed production vertex family directly
3. **Checkpoint 2**: introduce a read-only mesh-facing semantic and query adapter without changing behavior
4. **Checkpoint 3**: migrate invalidation from blind neighbor dirtying toward semantic boundary-change decisions
5. **Checkpoint 4**: move LOD semantic policy onto the same Greedy-centered semantic authority

Other active short-term work:

- continue engine and game separation so `fabric::` is usable by more than Recurse
- keep public seams rollback-safe while Goal #4 scaffolding matures
- harden benchmark, profiling, and validation workflows around the current voxel-first production path

## Long-term direction

Fabric is moving toward:

- ops-as-values for world mutations and queries
- phantom type-state at API boundaries
- centralized execution instead of side effects hidden inside worker closures
- RAII session ownership for world-scoped resources
- engine and game separation suitable for multiple games on the same engine

Some scaffolding for that direction already exists, such as `fabric::fx::WorldContext`, `recurse::world::FunctionContracts`, and `recurse::simulation::VoxelSemanticView`, but those surfaces are not yet the dominant runtime model.

## Quickstart

```bash
mise install
mise run build
mise run test
mise run run
```

Useful follow-up commands:

- `mise run build:release`
- `mise run test:e2e`
- `mise run lint:changed`
- `mise run profile`
- `mise run profile:capture`

## Repository layout

| Path | Role |
|------|------|
| `include/fabric/` | Engine headers |
| `include/recurse/` | Game headers |
| `src/fabric/`, `src/core/`, `src/platform/`, `src/ui/`, `src/utils/` | Engine implementation |
| `src/recurse/` | Game implementation |
| `config/` | Engine and game TOML defaults |
| `shaders/` | bgfx shader sources compiled to SPIR-V |
| `tests/` | Unit, E2E, fixtures, shared test main |
| `tasks/` | Shell and PowerShell task entry points used by mise |
| `docs/` | Deep reference documentation |

## Documentation hub

- [Contributing guide](CONTRIBUTING.md)
- [Architecture reference](docs/ARCHITECTURE.md)
- [Build guide](docs/BUILD.md)
- [Testing guide](docs/TESTING.md)
- [Tooling and documentation conventions](docs/TOOLING.md)
- [API surface map](docs/api.md)

## License

[MIT License](LICENSE)
