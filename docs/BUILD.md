# Build Guide

## Toolchain

Fabric uses CMake for configuration, Ninja for most local builds, CPM.cmake for dependency fetches, and mise as the task runner.

Required tools and platform pieces:

- [mise](https://mise.jdx.dev/) to provision or route toolchain commands
- a C++20 compiler: Apple Clang, GCC 10+, Clang 13+, or MSVC 19.29+
- Git for dependency fetches
- Vulkan runtime support
- on macOS, MoltenVK plus the Vulkan loader: `brew install molten-vk vulkan-loader`

## Quick start

```bash
mise install
mise run build
mise run test
```

Release build:

```bash
mise run build:release
```

## Common mise tasks

| Task | Alias | Purpose |
|------|-------|---------|
| `build` | `b`, `bd`, `build:debug` | Configure and build the debug preset |
| `build:release` | `br` | Configure and build the release preset |
| `run` | `r`, `rd`, `run:debug` | Run `Recurse` from the debug preset |
| `run:release` | `rr` | Run `Recurse` from the release preset |
| `format` | `fmt` | Check clang-format |
| `format:fix` | none | Auto-format tracked source files |
| `lint` | none | clang-tidy on the full source set |
| `lint:changed` | none | clang-tidy on changed files only |
| `test` | `t` | Unit tests |
| `test:e2e` | none | E2E tests |
| `test:all` | none | Unit plus E2E |
| `test:filter` | `tf` | Unit test filter runner |
| `profile` | `pd`, `p`, `profile:debug` | Tracy-enabled debug build |
| `profile:release` | `pr` | Tracy-enabled RelWithDebInfo build |
| `profile:capture` | `cap`, `profile:capture:debug` | Capture a debug Tracy trace |
| `profile:capture:release` | `capr` | Capture a release-style Tracy trace |
| `sanitize` | none | ASan plus UBSan build and test |
| `sanitize:tsan` | none | ThreadSanitizer build and test |
| `coverage` | none | Coverage build and report |
| `codeql` | none | Local CodeQL analysis |

## CMake presets

The main local presets are:

- `dev-debug`
- `dev-release`
- `dev-profile-debug`
- `dev-profile-release`

The repository also carries CI-oriented presets for platform validation, sanitizers, and coverage.

Direct CMake usage remains supported:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
```

Build outputs land under `build/<preset>/bin/`.

## Current target layout

| Target | Type | Current role |
|--------|------|--------------|
| `FabricLib` | static library | Engine code under `fabric::` |
| `RecurseGame` | object library | Game code under `recurse::` plus current voxel simulation sources |
| `Recurse` | executable | Main application entry point |
| `UnitTests` | executable | GoogleTest unit runner using `tests/TestMain.cc` |
| `E2ETests` | executable | GoogleTest end-to-end runner using `tests/TestMain.cc` |

`Recurse` optionally links mimalloc when `FABRIC_USE_MIMALLOC=ON`. Test targets do not link mimalloc.

## Dependency model

Dependencies are fetched through CPM.cmake modules in `cmake/modules/`. `mise.toml` sets `CPM_SOURCE_CACHE=~/.cache/CPM` so source downloads are reused between builds. The build uses `ccache` when it is available.

Important current build characteristics:

- bgfx, bx, and bimg are built from the repository's CMake modules
- shaders are compiled to SPIR-V and embedded into generated headers
- Vulkan is the only renderer backend the repository targets
- FreeType may come from the system package or a fallback source build
- Tracy and mimalloc are optional and controlled by CMake options and presets

## Platform notes

### macOS

- macOS uses Vulkan through MoltenVK
- Homebrew's `/opt/homebrew/lib` path is part of the runtime story for the Vulkan loader
- profiling and sanitizer presets are available, but keep an eye on platform-specific SDK mismatches

### Linux

- install Vulkan support for the active GPU and distribution
- WebKit development packages may still be required when WebView support is enabled

### Windows

- use a Visual Studio environment with the Desktop C++ workload
- set `MISE_ENV=windows` before the first mise-driven build if needed
- `mise.windows.toml` carries Windows-specific overrides such as the CPM cache path

## Current build posture

The build contract is intentionally conservative right now:

- `FabricLib` remains the reusable engine anchor
- Recurse remains the reference app and validation target
- the default configuration keeps Greedy meshing as the production near path
- profiling and benchmark capture stay first-class workflows, not side scripts

The combined Goal #4 plus meshing checkpoint wave is expected to change implementation internals, not the top-level build contract.

## Long-term direction

Over time the repository should support more application targets on top of the same engine core. The expected direction is:

- keep CMake plus mise as the primary build interface
- retain `FabricLib` as the reusable engine root
- allow additional game or tool targets without collapsing the `fabric::` and `recurse::` boundary
- keep profiling, analysis, and validation tasks as first-class build surfaces
