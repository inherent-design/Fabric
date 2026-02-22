# Contributing to Fabric Engine

## Prerequisites

- [mise](https://mise.jdx.dev/) (manages cmake and ninja automatically)
- C++20 compatible compiler (Clang 13+, GCC 10+, MSVC 19.29+)
- Platform-specific tooling (see [Build Guide](docs/BUILD.md))

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
| `lint` | *none* | Run clang-tidy on source files |
| `lint:fix` | `fix` | Run clang-tidy with auto-fix |
| `test` | `t` | Run unit tests |
| `test:e2e` | *none* | Run E2E tests (slow) |
| `test:all` | *none* | Run unit + E2E tests |
| `test:filter` | `tf` | Run unit tests with gtest filter |

## Development Workflow

1. Create a feature branch from `main`
2. Make changes, build, and verify tests pass:
   ```bash
   mise run build && mise run test
   ```
3. Run linting:
   ```bash
   mise run lint
   ```
4. Submit a pull request with a clear description

## Project Structure

```
fabric/
├── include/fabric/
│   ├── core/           # Component, Command, Event, Lifecycle, Plugin,
│   │                   # Resource, ResourceHub, Temporal, Spatial, Types,
│   │                   # Log, Async, JsonTypes, Constants.g
│   ├── parser/         # ArgumentParser, SyntaxTree, Token
│   ├── ui/             # WebView
│   └── utils/          # CoordinatedGraph, ThreadPoolExecutor,
│                       # TimeoutLock, Profiler, ErrorHandling, Testing, Utils
├── src/                # Implementation files (.cc)
├── tests/
│   ├── unit/           # Per-component unit tests
│   └── e2e/            # End-to-end tests
├── cmake/modules/      # FetchContent modules (7 libraries)
├── tasks/              # POSIX shell scripts for mise
├── CMakeLists.txt      # Build config (FabricLib static library)
├── CMakePresets.json    # 7 presets (dev + CI)
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
