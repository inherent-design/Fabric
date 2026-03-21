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

All developer workflows live in `mise.toml` and run with `mise run <task>`. See [docs/BUILD.md](docs/BUILD.md) for the full task table and build details.

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
- `Recurse`: application executable (links mimalloc when `FABRIC_USE_MIMALLOC` is enabled)
- `UnitTests` and `E2ETests` (do not link mimalloc; share `tests/TestMain.cc` which initializes Quill logging and bgfx Noop environment)

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

Greedy meshing is the primary production renderer; SnapMC is optional behind the pluggable mesher boundary. `VoxelCell` now uses essence-first `MatterState` storage (Wave 4 merged). New code touching cell data should use the accessors in `CellAccessors.hh`. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for architectural context.

## CI checks

Pull requests run these checks automatically:

- `clang-format` on `include/` and `src/`
- `cppcheck` static analysis (suppressions in `.cppcheck-suppressions`)
- ASan + UBSan sanitizer builds

All checks must pass before merge. Pre-existing suppressions are documented in `.cppcheck-suppressions` with comments explaining each entry.

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
