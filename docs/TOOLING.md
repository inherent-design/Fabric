# Tooling

This document covers repository tooling with an emphasis on documentation conventions, source comments, and how curated docs relate to build, test, and profiling workflows.

## Repository documentation roles

The markdown set has distinct jobs:

- `README.md`: hub, status snapshot, quickstart, links
- `docs/*.md`: deep reference documents and current source of truth for their topic
- `CONTRIBUTING.md`: contributor workflow and repository policy
- `CLAUDE.md`: agent-facing implementation, style, and workflow guidance

Keep those roles distinct. Do not turn `README.md` into a full architecture manual, and do not let `CONTRIBUTING.md` drift into a deep subsystem reference.

## Current documentation posture

The repository currently relies on curated markdown first. That is the right tradeoff for the present state of the codebase because:

- current implementation and roadmap details change faster than generated API docs would stay useful
- the codebase includes both mature production systems and actively shifting architectural scaffolding
- short-term planning, especially the combined Goal #4 plus meshing checkpoint work, needs narrative context that generated symbol docs cannot provide
- the docs also need to preserve the current production stance: Greedy first, visibly voxel, with SnapMC documented only as optional and experimental behind the pluggable mesher boundary

## Prose and markdown conventions

Use these rules across repository docs:

- technical reference tone
- match identifiers exactly
- state what a thing does, valid values, and defaults in that order when relevant
- no em dashes, en dashes, or double hyphens in prose
- no emojis, superlatives, or marketing language
- keep `README.md` short; move depth into `docs/*.md`

Update docs in the same change when behavior, config, architecture guidance, or workflow changed.

## Source comment conventions

Doxygen remains the selected generator direction, but comment coverage is still intentionally selective.

Current rules:

- use `///` for doc comments
- keep the first sentence as a standalone summary
- use `@tparam`, `@param`, and `@return` when the public contract needs them
- prefer no non-doc comments unless the code needs a clear explanation of why
- do not add doc comments mechanically to unchanged code during unrelated work

Example:

```cpp
/// Resolve a synchronous read operation against the current world session.
///
/// @tparam Op Operation type satisfying the sync-read contract.
/// @param op Operation value to resolve.
/// @return Result containing the operation return type or error.
```

## Developer tooling touchpoints

Documentation should stay aligned with the real developer entry points:

- build and run via mise plus CMake presets
- test execution through `mise run test`, `test:e2e`, and `test:filter`
- formatting, clang-tidy, cppcheck, sanitizers, coverage, and CodeQL tasks from `mise.toml`
- Tracy profiling builds and capture workflows
- Quill logging configuration through TOML, env vars, and CLI flags

When any of those change, update the corresponding doc page in the same change.

## Generated documentation direction

Generated API docs are still deferred until the public comment surface is worth publishing. The expected path remains:

1. keep curated markdown accurate
2. improve public `///` coverage on reusable `fabric::` surfaces
3. add Doxygen generation once the output becomes more signal than noise
4. layer richer HTML on top later only if the repository needs it

Doxygen remains the preferred generator candidate because it is maintained, integrates well with CMake, and is good enough for the current codebase shape.

## Short-term tooling and docs work

The short-term focus is not new tooling for its own sake. It is keeping existing tooling and docs aligned with:

- the Greedy-first near-meshing production path
- the optional and experimental status of SnapMC behind the mesher boundary
- benchmark automation and profiling capture workflows
- the combined Goal #4 plus meshing checkpoint sequence
- engine and game boundary cleanup needed for multi-project readiness

Prefer updating existing markdown files over creating new ones unless a new document is genuinely required.

## Long-term direction

Longer term, the tooling story should support a cleaner engine API and more than one game on Fabric. That likely means:

- stronger public docs for `fabric::`
- clearer separation between engine docs and game docs
- generated API docs as a supplement, not a replacement, for curated architecture and workflow docs
- validation tooling that continues to treat profiling and benchmark automation as first-class workflows

Those docs also need to keep explaining the architectural direction without overstating completion: ops-as-values, type-state, and centralized execution are the direction of travel, while the current repository still contains a mix of mature production systems and evolving scaffolding.
