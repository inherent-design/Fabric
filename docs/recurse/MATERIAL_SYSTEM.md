# Material System

> **Implemented; not yet documented.** This system exists in the codebase but lacks a design document. Sections below are stubs to be filled as the design stabilizes.

## Overview

Defines the material palette for the voxel world: stone, dirt, sand, water, and gravel. Each material maps to a `MaterialId` (defined in `VoxelMaterial.hh`), which the `MaterialRegistry` resolves to phase (solid, powder, liquid, gas), displacement rank, density, and rendering attributes.

## Key Code

- `include/recurse/simulation/MaterialRegistry.hh`
- `include/recurse/simulation/MaterialSemanticRegistry.hh`
- `include/recurse/simulation/CellAccessors.hh` (accessor quarantine)
- `include/recurse/simulation/VoxelConstants.hh`

## Design Decisions

<!-- TODO: document material palette choices, essence-to-material mapping, phase table layout -->

## Open Questions

- Final material count and palette for shipped game
- Essence blending rules (if any beyond 1:1 materialId mapping)
