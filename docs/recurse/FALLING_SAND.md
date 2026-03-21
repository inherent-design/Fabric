# Falling Sand Simulation

> **Implemented; not yet documented.** This system exists in the codebase but lacks a design document. Sections below are stubs to be filled as the design stabilizes.

## Overview

Cellular automaton driving powder, liquid, and gas movement. Each tick, the `FallingSandSystem` iterates active chunks and applies movement rules based on material phase: powders fall and pile, liquids spread laterally, gases rise.

## Key Code

- `include/recurse/simulation/FallingSandSystem.hh`
- `include/recurse/simulation/CellAccessors.hh` (`canDisplace`, `cellPhase`)
- Phase pipeline: runs in phases 1-4 of the per-tick simulation

## Simulation Rules

<!-- TODO: document displacement logic, settling detection, boundary propagation -->

## Performance

<!-- TODO: document chunk activation/deactivation, parallelFor worker strategy, settled chunk skipping -->
