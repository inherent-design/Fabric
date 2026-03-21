# Meshing

> **Implemented; not yet documented.** This system exists in the codebase but lacks a design document. Sections below are stubs to be filled as the design stabilizes.

## Overview

Converts voxel data into renderable triangle meshes. The primary path is greedy meshing (`GreedyMeshBuilder`), which merges coplanar faces of the same material into larger quads to reduce draw calls.

## Key Code

- `include/recurse/rendering/GreedyMeshBuilder.hh`
- `include/recurse/rendering/VoxelMeshingSystem.hh`
- `include/recurse/simulation/CellAccessors.hh` (`mergeKey`, `canMergeQuads`)

## Greedy Meshing

<!-- TODO: document face merging strategy, merge key derivation, vertex packing -->

## Experimental: SnapMC

<!-- TODO: document SnapMC status, pluggable mesher boundary, when/if to enable -->
