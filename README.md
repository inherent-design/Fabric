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

## mise task surface

`mise.toml` is the primary developer entry point. `docs/BUILD.md` remains the deep reference; this section is the current hub-level task map.

### Build and run

| Task | Alias | Current behavior |
|------|-------|------------------|
| `clean` | none | Remove build artifacts |
| `build` | `b`, `bd`, `build:debug` | Configure and build the default debug preset |
| `build:release` | `br` | Configure and build the release preset |
| `run` | `r`, `rd`, `run:debug` | Build if needed, then run the debug `Recurse` executable |
| `run:release` | `rr` | Build if needed, then run the release `Recurse` executable |

### Format and static analysis

| Task | Alias | Current behavior |
|------|-------|------------------|
| `format` | `fmt` | Check clang-format on tracked source files |
| `format:fix` | none | Auto-format tracked source files |
| `lint` | none | Run clang-tidy on the full source set |
| `lint:changed` | none | Run clang-tidy on git-dirty files only for faster iteration |
| `lint:fix` | `fix` | Run clang-tidy auto-fix on changed files |
| `cppcheck` | none | Run cppcheck static analysis |

### Test and validation

| Task | Alias | Current behavior |
|------|-------|------------------|
| `test` | `t` | Build if needed, then run unit tests |
| `test:e2e` | none | Build if needed, then run end-to-end tests |
| `test:all` | none | Build if needed, then run unit plus end-to-end tests |
| `test:filter` | `tf` | Build if needed, then run unit tests with a gtest filter argument |

### Profiling and capture workflows

| Task | Alias | Current behavior |
|------|-------|------------------|
| `profile` | `pd`, `p`, `profile:debug` | Build a Tracy-enabled debug configuration |
| `profile:release` | `pr` | Build a Tracy-enabled `RelWithDebInfo` configuration |
| `profile:capture` | `cap`, `profile:capture:debug` | Build the profiling target if needed, launch the debug app, and capture a 30 second Tracy trace |
| `profile:capture:release` | `capr` | Build the profiling target if needed, launch the release-style app, and capture a 30 second Tracy trace |
| `profile:view` | none | Open the latest `.tracy` capture in Tracy Profiler |
| `profile:csv` | none | Export the latest `.tracy` capture to CSV |

Current expectation: use the capture tasks when validating benchmark and profiling flows, since benchmark automation startup plumbing and in-game benchmark entry points are part of the repository's real workflow.

### Sanitizers and broader analysis

| Task | Alias | Current behavior |
|------|-------|------------------|
| `sanitize` | none | Build and test with ASan plus UBSan |
| `sanitize:tsan` | none | Build and test with ThreadSanitizer |
| `coverage` | none | Build with coverage instrumentation and generate a report |
| `codeql` | none | Run local CodeQL analysis after a build |

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
