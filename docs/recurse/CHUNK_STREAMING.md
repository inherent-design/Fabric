# Chunk Streaming

> **Implemented; not yet documented.** This system exists in the codebase but lacks a design document. Sections below are stubs to be filled as the design stabilizes.

## Overview

Manages chunk load/unload around the player. `ChunkPipelineSystem` tracks a streaming radius, transitions chunks through lifecycle states (Generating, Active, Draining), and coordinates with world generation and persistence.

## Key Code

- `include/recurse/systems/ChunkPipelineSystem.hh`
- `include/recurse/simulation/ChunkRegistry.hh` (`ChunkSlotState` enum)
- `include/recurse/simulation/SimulationGrid.hh` (buffer management)

## Lifecycle States

<!-- TODO: document ChunkSlotState transitions, phase gating, buffer materialization -->

## Budget

<!-- TODO: document per-frame chunk budget, LOD distance tiers, memory limits -->
