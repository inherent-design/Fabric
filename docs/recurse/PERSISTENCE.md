# Persistence

> **Implemented; not yet documented.** This system exists in the codebase but lacks a design document. Sections below are stubs to be filled as the design stabilizes.

## Overview

Chunk save/load system for world state. Modified chunks are serialized and written to disk; unmodified chunks are regenerated from seed on demand.

## Key Code

- `include/recurse/world/SaveService.hh`
- `include/recurse/world/WorldSession.hh` (RAII session boundary)

## Format

<!-- TODO: document serialization format, compression (zstd), chunk dirty tracking -->

## Lifecycle

<!-- TODO: document save triggers, flush on session teardown, async write strategy -->
