# RECURSE Function System

## Core Functions (Player Unlocks)
*Immutable abilities that manipulate Quanta Fields.*

| Name             | Unlock | Core Quanta Effect                               | Description                                       |
|------------------|--------|--------------------------------------------------|---------------------------------------------------|
| **CREATE_MATTER**  | Lvl 1  | `DENSITY` -> `1.0`                               | Solidifies a region of the VOID.                  |
| **DESTROY_MATTER** | Lvl 1  | `DENSITY` -> `0.0`                               | Dissolves a region of matter back to the VOID.    |
| **TRANSFUSE_ESSENCE**| Lvl 2  | `ESSENCE` -> `[new_vec4]`                        | Overwrites the elemental 'Essence' of a target.   |
| **TIME_DILATION**  | Lvl 2  | `VELOCITY` -> `* 0.1` or `* 10.0`                | Speeds up or slows down processes in a region.    |
| **ATTRACT_QUANTA** | Lvl 3  | `VELOCITY` -> `vector_towards_point`             | Pulls quanta towards a focal point.               |


## Implementation Blocks (Consumables)
*Composable modifiers that alter a Function's behavior.*

**Block Types:** `TARGET`, `SHAPE`, `EFFECT`, `FLOW`

| Name                  | Type     | Compatible With | Description                                           |
|-----------------------|----------|-----------------|-------------------------------------------------------|
| **TARGET_POINT**        | `TARGET` | ALL             | Affects a single point in space.                      |
| **TARGET_SELF**         | `TARGET` | ALL             | Affects the player's immediate location.              |
| **TARGET_NPC**          | `TARGET` | ALL             | Homes in on the nearest valid NPC.                    |
| **SHAPE_BEAM**          | `SHAPE`  | ALL             | Projects the effect in a straight line.               |
| **SHAPE_SPHERE**        | `SHAPE`  | ALL             | Affects a spherical area of effect.                   |
| **SHAPE_CUBE**          | `SHAPE`  | CREATE/DESTROY  | Affects a cubic area of effect.                       |
| **EFFECT_ESSENCE_ORDER**| `EFFECT` | TRANSFUSE       | Sets target Essence to [1,0,0,0].                     |
| **EFFECT_ESSENCE_CHAOS**| `EFFECT` | TRANSFUSE       | Sets target Essence to [0,1,0,0].                     |
| **EFFECT_ESSENCE_LIFE** | `EFFECT` | TRANSFUSE       | Sets target Essence to [0,0,1,0].                     |
| **EFFECT_ACCELERATE**   | `EFFECT` | TIME_DILATION   | Multiplies time factor.                               |
| **EFFECT_DECELERATE**   | `EFFECT` | TIME_DILATION   | Divides time factor.                                  |
| **FLOW_PERSIST**        | `FLOW`   | ALL             | Effect lingers for 3 seconds instead of being instant. |
