# Physics

> **Implemented; not yet documented.** This system exists in the codebase but lacks a design document. Sections below are stubs to be filled as the design stabilizes.

## Overview

Voxel-aware collision and physics. `PhysicsGameSystem` handles entity-vs-voxel collision, gravity, and batch collision dispatch via enkiTS workers.

## Key Code

- `include/recurse/physics/PhysicsGameSystem.hh`
- Collision batch dispatch uses `JobScheduler::parallelFor` with per-worker result collection

## Collision Model

<!-- TODO: document AABB-vs-voxel, sweep tests, terrain collision response -->

## Integration

<!-- TODO: document fixed timestep integration, gravity, entity movement resolution -->
