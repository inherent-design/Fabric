# World Generation

> **Implemented; not yet documented.** This system exists in the codebase but lacks a design document. Sections below are stubs to be filled as the design stabilizes.

## Overview

Procedural terrain generation using FastNoise2. `WorldGenerator` produces chunk-sized voxel buffers from noise fields, assigning materials based on density thresholds and biome rules.

## Key Code

- `include/recurse/world/WorldGenerator.hh`
- `include/recurse/world/MinecraftNoiseGenerator.hh`
- Noise library: FastNoise2 (`GenSingle2D`, `GenUniformGrid2D`)

## Terrain Pipeline

<!-- TODO: document noise stack, biome selection, material thresholds, LOD-consistent sampling (std::fma precision) -->

## Biomes

<!-- TODO: document biome types, transition rules, per-biome material palettes -->
