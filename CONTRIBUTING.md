# Contributing to Fabric

## Prerequisites

- [mise](https://mise.jdx.dev/) for task execution and tool provisioning
- A C++20 compiler: Apple Clang, GCC 10+, Clang 13+, or MSVC 19.29+
- Platform Vulkan support as described in [docs/BUILD.md](docs/BUILD.md)

### Platform requirements

Rendering is Vulkan-only.

- **macOS**: install MoltenVK and the Vulkan loader with `brew install molten-vk vulkan-loader`
- **Linux**: install the Vulkan SDK or distribution Vulkan packages plus GPU drivers
- **Windows**: install the LunarG Vulkan SDK

## Getting started

```bash
git clone <repository-url>
cd fabric
mise install
mise run build
mise run test
```

## Task reference

All developer workflows live in `mise.toml` and run with `mise run <task>`.

| Task | Alias | Description |
|------|-------|-------------|
| `build` | `b`, `bd`, `build:debug` | Configure and build the default debug preset |
| `build:release` | `br` | Release build |
| `run` | `r`, `rd`, `run:debug` | Build and run the debug app target |
| `run:release` | `rr` | Run the release app target |
| `format` | `fmt` | Check clang-format |
| `format:fix` | none | Auto-format tracked source files |
| `lint` | none | clang-tidy on the full source set |
| `lint:changed` | none | clang-tidy on git-dirty files only |
| `lint:fix` | `fix` | clang-tidy auto-fix on changed files |
| `cppcheck` | none | cppcheck static analysis |
| `test` | `t` | Unit tests |
| `test:e2e` | none | End-to-end tests |
| `test:all` | none | Unit plus E2E |
| `test:filter` | `tf` | Unit tests with a gtest filter |
| `profile` | `pd`, `p`, `profile:debug` | Tracy-enabled debug build |
| `profile:release` | `pr` | Tracy-enabled release-style build |
| `profile:capture` | `cap`, `profile:capture:debug` | Build and capture a Tracy trace |
| `profile:capture:release` | `capr` | Capture a release-style Tracy trace |
| `sanitize` | none | ASan plus UBSan build and run |
| `sanitize:tsan` | none | ThreadSanitizer build and run |
| `coverage` | none | Coverage report generation |
| `codeql` | none | Local CodeQL analysis |

## Development workflow

1. Branch from `dev`, or work on `dev` when that is the agreed integration branch.
2. Build and test early.
   ```bash
   mise run build
   mise run test
   ```
3. Use focused validation while iterating, then broader checks before review.
   ```bash
   mise run lint:changed
   mise run lint
   ```
4. Update documentation in the same change when behavior, configuration, architecture guidance, or contributor workflow changed.
5. Submit the pull request.

## Build and dependency notes

The build uses CMake plus CPM.cmake modules under `cmake/modules/`. `mise.toml` sets `CPM_SOURCE_CACHE=~/.cache/CPM` so repeated builds reuse downloads. Use package managers or CMake dependency plumbing for dependency changes. Do not hand-edit vendored metadata or lock-equivalent files.

The current targets are:

- `FabricLib`: reusable engine static library
- `RecurseGame`: game object library
- `Recurse`: application executable
- `UnitTests` and `E2ETests`

## Repository boundaries

- `fabric::` is the engine layer
- `recurse::` is the current game layer
- dependency direction stays one way: game depends on engine, never reverse

Ask this before moving code into `fabric::`: would a second game on Fabric need this exact abstraction?

## Documentation conventions

- `README.md` is the hub and should stay short and scannable
- `docs/*.md` are deep references and the source of truth for their topic
- `CONTRIBUTING.md` stays workflow-focused
- `CLAUDE.md` carries agent and implementation guidance that should match the real codebase
- update docs in the same change when code or config behavior changed

Follow the prose rules from [docs/TOOLING.md](docs/TOOLING.md):

- technical reference tone
- no em dashes, en dashes, or double hyphens in prose
- no marketing language or fluff
- prefer exact identifiers and defaults

## Current project stance

Contributors should preserve the current production posture:

- Greedy meshing is the primary near-path production renderer
- SnapMC is optional and experimental behind the pluggable mesher boundary
- the shipped visual target is visibly voxel terrain, not smooth-surface replacement

The active short-term sequence combines Goal #4 with the meshing checkpoints:

1. instrument the Greedy production path
2. remove the smooth-intermediate repack cost
3. add a read-only mesh semantic and query adapter
4. replace blind neighbor invalidation with semantic boundary decisions
5. move LOD policy onto the same semantic authority

Other near-term work continues to tighten engine and game separation, benchmark automation, and multi-project readiness without destabilizing the Greedy-first shipped path.

## Long-term direction

The long arc of the codebase is unchanged:

- ops-as-values instead of hidden side effects in worker closures
- phantom type-state at API boundaries
- centralized execution for world access
- RAII session ownership for world-scoped state
- a clean `fabric::` and `recurse::` boundary that supports more than one game

Some scaffolding already exists in `fabric::fx::WorldContext`, `recurse::world::FunctionContracts`, and `recurse::simulation::VoxelSemanticView`, but contributors should treat those as evolving surfaces, not as proof that the migration is complete.

## Code and commit expectations

- `.hh` headers and `.cc` sources
- `PascalCase` for types, `camelCase` for functions, `K_SCREAMING_SNAKE_CASE` for constants
- prefer `throwError()` over raw `throw`
- prefer `fabric::fx::Result<A, Es...>` over ad-hoc result structs
- use Quill logging macros, not `printf` or `std::cerr`
- add Tracy zones where frame analysis will need them

Use conventional prefixes for commits: `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `perf:`, `build:`.

Keep subjects under 72 characters and explain why in the body when a body is needed.

## License

By contributing, you agree that your contributions will be licensed under the project's [MIT License](LICENSE).
