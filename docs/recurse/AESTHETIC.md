# Aesthetic & Visual Language

## Quanta Field Rendering
- **`DENSITY` Field:**
  - `> 0.5`: Render as a solid voxel.
  - `< 0.5`: Transparent/Void.
  - `Transition (CREATE/DESTROY)`: Quick scale-up/scale-down of voxels with particle effects.
- **`ESSENCE` Field (Color Mapping):**
  - Voxel color is a lerp of the 4 `ESSENCE` stats.
  - `ORDER` -> Cool Blues, Greys. Geometric, sharp-edged voxels.
  - `CHAOS` -> Reds, Purples. Jagged, unstable-looking voxels.
  - `LIFE` -> Greens, Browns. Organic, slightly rounded voxels.
  - `DECAY` -> Dark browns, sickly yellows. "Crumbling" voxel effect.

## VFX
- **Function Casting:** Player's hands emit colored light corresponding to the `BLOCKS` being used.
- **Transformation:** Target NPC dissolves into colored `ESSENCE` particles, which then reform into the new shape over ~1.5 seconds.
