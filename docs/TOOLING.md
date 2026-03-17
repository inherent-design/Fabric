# Tooling

Reference for documentation generation tooling, comment conventions, and integration
strategy for the Fabric Engine codebase.

## Documentation Generation

Doxygen 1.16.1 is the selected documentation generator. It provides functional C++20
support for concepts (`\concept` command, improved in 1.16.0), requires clauses (trailing
and inline, fixed in 1.10+), template specializations, and constexpr. Standardese was
evaluated but rejected; the original author abandoned it in 2019 and community maintenance
is minimal. hdoc offers superior parse accuracy through its Clang-based frontend, but has
had no public release since February 2023 and lacks official macOS support. Doxygen's
built-in CMake integration, universal `///` comment compatibility, and active maintenance
make it the practical choice for a project at Fabric's scale.

## Comment Style Guide

Every public type, function, method, and constant gets a `///` doc comment. The first
sentence is a standalone summary; doc-gen tools extract it as the brief description.

### Basic format

```cpp
/// Summary sentence describing what the type or function does.
```

### With parameters

```cpp
/// Resolve a synchronous read operation against the session.
///
/// @tparam Op  Operation type satisfying SyncReadOp concept.
/// @param op   The operation to resolve.
/// @return     Result containing the operation's return value or error.
template <SyncReadOp Op>
auto resolve(const Op& op) -> OpResult<Op>;
```

### Concept documentation

```cpp
/// A synchronous read operation resolved inline by the executor.
///
/// Requires Returns and Errors type aliases and K_IS_SYNC == true.
template <typename Op>
concept SyncReadOp = /* ... */;
```

### constexpr data

```cpp
/// Maximum number of unique essence values per chunk palette.
static constexpr uint16_t K_DEFAULT_MAX_SIZE = 65535;
```

### Rules

- Use `///` exclusively; do not use `/** */` or `//!`.
- First sentence ends with a period and stands alone as the brief description.
- Separate the brief from the detailed description with a blank `///` line.
- Use `@tparam` for template parameters (including packs: `@tparam Es Error types.`),
  `@param` for function parameters, and `@return` for return values.
- Use `@note` sparingly for non-obvious behavior.
- Cover: what it does, input expectations, side effects, error conditions.
- Do not document what the code already says; `size()` does not need `/// Returns the size`.
- Private members and implementation details: document only when behavior is surprising.
- Match existing codebase style; do not add doc comments to unchanged code during a refactor.

## When to Comment (Non-Doc)

Default to no comments. Code should be self-explanatory through naming and structure.
Add a non-doc comment (`//`) when:

- The "why" is non-obvious: a workaround, API quirk, or spec requirement that the reader
  could not infer from the code alone.
- Behavior has surprising side effects that are not captured by the function signature or
  doc comment.
- A constant comes from an external specification; include the source reference.

These guidelines mirror the existing policy in CONTRIBUTING.md. Doc comments (`///`) and
inline comments (`//`) serve different purposes: doc comments describe the public contract,
inline comments explain implementation rationale.

## CMake Integration (Deferred)

CMake integration is deferred until the comment coverage reaches a useful threshold. When
ready, use CMake's built-in `FindDoxygen` module with `doxygen_add_docs()` or a custom
target. The Doxyfile template (or inline CMake variable configuration) is TBD. If richer
output is needed later, Sphinx combined with Breathe can consume Doxygen's XML output
without requiring any changes to source comments.

## Tool Versions

| Tool | Version | Status | C++20 Concepts | Notes |
|------|---------|--------|----------------|-------|
| Doxygen | 1.16.1 | Active (Jan 2026) | Yes | Selected. `\concept` command, built-in CMake support. |
| Standardese | N/A | Abandoned (2019) | Uncertain | Original author post-mortem published; community maintenance minimal. |
| hdoc | 1.4.1 | Stale (Feb 2023) | Likely (Clang AST) | No public release in 3+ years; Linux-only official support. |
| Poxy | 0.19.7 | Active | Yes (via Doxygen) | Upgrade option for modern HTML output; wraps Doxygen with m.css. |
| Sphinx + Breathe | N/A | Active | Yes (via Doxygen XML) | Upgrade path for narrative + API docs; higher setup cost. |
