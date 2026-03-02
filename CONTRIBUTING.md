# Contributing to Fabric Engine

## Prerequisites

- [mise](https://mise.jdx.dev/) (manages cmake and ninja automatically)
- C++20 compatible compiler (Clang 13+, GCC 10+, MSVC 19.29+)
- Platform-specific tooling (see [Build Guide](docs/BUILD.md))

### Platform Requirements

All platforms use Vulkan as the sole rendering backend (SPIR-V shaders).

- **macOS**: Vulkan runs via MoltenVK. Install with `brew install molten-vk vulkan-loader`. The build sets `BUILD_RPATH` to `/opt/homebrew/lib` so the Vulkan loader is found at runtime.
- **Linux**: Install Vulkan SDK and drivers for your GPU.
- **Windows**: Install the LunarG Vulkan SDK.

## Getting Started

```bash
git clone <repository-url>
cd fabric
mise install
mise run build
mise run test
```

## Task Reference

All tasks are defined in `mise.toml` and run via `mise run <task>`.

| Task | Alias | Description |
|------|-------|-------------|
| `build` | `b` | Configure and build (Debug) |
| `build:release` | `br` | Configure and build (Release) |
| `clean` | *none* | Remove build artifacts |
| `format` | *none* | Check clang-format |
| `format:fix` | *none* | Auto-format with clang-format |
| `lint` | *none* | Run clang-tidy on all source files (slow) |
| `lint:changed` | *none* | Run clang-tidy on git-dirty files only (fast) |
| `lint:fix` | `fix` | Run clang-tidy with auto-fix |
| `cppcheck` | *none* | Run cppcheck static analysis |
| `test` | `t` | Run unit tests (with timeout) |
| `test:e2e` | *none* | Run E2E tests |
| `test:all` | *none* | Run unit + E2E tests |
| `test:filter` | `tf` | Run unit tests with gtest filter |
| `sanitize` | *none* | ASan + UBSan build and run |
| `sanitize:tsan` | *none* | ThreadSanitizer build and run |
| `coverage` | *none* | Code coverage with lcov report |
| `codeql` | *none* | CodeQL security analysis |

## Development Workflow

1. Create a feature branch from `main`
2. Make changes, build, and verify tests pass:
   ```bash
   mise run build && mise run test
   ```
3. Run linting (fast mode for iteration, full before submitting):
   ```bash
   mise run lint:changed   # only git-dirty files
   mise run lint           # all files (pre-submit)
   ```
4. Submit a pull request with a clear description

## Dependency Management

All third-party libraries are fetched via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) v0.42.1. Each library has a dedicated module in `cmake/modules/` using `CPMAddPackage()`. There are 25 cmake modules in total: 18 CPM dependency modules and 7 shader compilation modules.

Set `CPM_SOURCE_CACHE=~/.cache/CPM` (configured in `mise.toml`) to share downloaded sources across builds and avoid redundant downloads. The build also uses `ccache` when available for compiler output caching.

### CPM PATCHES

Some dependencies require vendored fixes. The `cmake/patches/` directory holds patch files applied via the CPM `PATCHES` argument. For example, `bgfx-vk-suboptimal.patch` fixes a case where bgfx treats `VK_SUBOPTIMAL_KHR` as a fatal error, causing crashes on window resize with MoltenVK.

## Project Structure

```
fabric/
├── include/fabric/
│   ├── core/           # Component, Command, Event, Lifecycle, Plugin,
│   │                   # Resource, ResourceHub, Temporal, Spatial, Types,
│   │                   # Log, Async, JsonTypes, Constants, StateMachine,
│   │                   # Pipeline, Camera, Rendering, BVH, ChunkedGrid
│   ├── codec/          # Codec encode/decode framework
│   ├── parser/         # ArgumentParser, SyntaxTree, Token
│   ├── ui/             # WebView, BgfxRenderInterface (RmlUi)
│   └── utils/          # CoordinatedGraph, ImmutableDAG, BufferPool,
│                       # ThreadPoolExecutor, TimeoutLock, Profiler,
│                       # ErrorHandling, Testing, Utils
├── src/
│   ├── core/           # Implementation files (.cc)
│   └── ui/             # UI implementation
├── shaders/
│   ├── oit/            # Order-independent transparency
│   ├── particle/       # Particle system
│   ├── post/           # Post-processing
│   ├── rmlui/          # RmlUi render interface
│   ├── skinned/        # Skeletal animation
│   ├── sky/            # Atmospheric rendering
│   ├── voxel/          # Voxel mesh
│   └── water/          # Water surface
├── assets/
│   ├── ui/             # RmlUi documents (.rml, .rcss)
│   └── fonts/          # Font files
├── tests/
│   ├── unit/           # Per-component unit tests (1429 tests, 111+ suites)
│   └── e2e/            # End-to-end tests
├── cmake/
│   ├── modules/        # 25 CMake modules (18 CPM deps + 7 shader compilation)
│   └── patches/        # Vendored dependency patches
├── tasks/              # POSIX shell scripts for mise
├── CMakeLists.txt      # Build config (FabricLib static library)
├── CMakePresets.json    # 11 configure presets (1 hidden base, 10 visible)
└── mise.toml           # Task runner config
```

## Code Style

### File Conventions

- `.hh` for headers, `.cc` for source files
- Namespace: `fabric::` (sub-namespaces: `fabric::Utils`, `fabric::log`, `fabric::async`, `fabric::Testing`)

### Naming

- **Classes, structs, enums:** `PascalCase`
- **Functions and methods:** `camelCase`
- **Variables and parameters:** `camelCase`
- **Constants:** `kConstantName`
- **Macros:** `FABRIC_MACRO_NAME` (prefer constexpr over macros)

### Error Handling and Logging

- Use `throwError()` for error conditions, not raw throws
- Use `FABRIC_LOG_*` macros for logging (`FABRIC_LOG_DEBUG`, `FABRIC_LOG_INFO`, `FABRIC_LOG_WARN`, `FABRIC_LOG_ERROR`, `FABRIC_LOG_CRITICAL`)
- Include `fabric/core/Log.hh` for logging

### Writing and Comments

- Clean code, minimal comments. Only comment when the "why" is non-obvious.
- No em-dashes, en-dashes, or double-hyphens in prose. Use semicolons, commas, or colons.
- No emojis.
- No superlatives or marketing language in documentation.

## Commits

Use conventional-style prefixes:

```
feat: new feature
fix: bug fix
chore: maintenance, dependency updates
docs: documentation changes
test: adding or updating tests
refactor: code restructuring without behavior change
perf: performance improvements
build: build system changes
```

Subject line under 72 characters, imperative mood. Body explains why, not what. Bullet points preferred for multi-line bodies.

## Documentation

- **README.md** is the hub document: feature summary, quickstart, links to `docs/`
- **docs/*.md** files are deep reference per topic (source of truth for that domain)
- **CONTRIBUTING.md** covers development workflow only
- When changing code that affects documented behavior, update the corresponding `docs/` file in the same change

## License

By contributing, you agree that your contributions will be licensed under the project's [MIT License](LICENSE).
