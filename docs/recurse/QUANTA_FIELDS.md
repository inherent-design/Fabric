# Quanta Field Definitions

*The invisible grid-based data layer that drives all game logic.*

## Field 1: `DENSITY` (float, 0.0 to 1.0)
- **Purpose:** Defines solidity of a point in space.
- **`0.0`**: The VOID. Passable, no collision.
- **`1.0`**: Solid Matter. Collision, opaque.
- **Primary Functions:** `CREATE_MATTER`, `DESTROY_MATTER`.

## Field 2: `ESSENCE` (vec4, [O,C,L,D])
- **Purpose:** Defines the elemental "type" and behavior of matter or NPCs.
- **Example:** `[0.8, 0.1, 0.1, 0.0]` is primarily "Order".
- **Primary Functions:** `TRANSFUSE_ESSENCE`. Drives NPC transformations and visual appearance.

## Field 3 (Stretch Goal): `VELOCITY` (vec3)
- **Purpose:** Defines motion of quanta.
- **`[0,0,0]`**: Static.
- **`[x,y,z]`**: Moving.
- **Primary Functions:** `TIME_DILATION`, `ATTRACT_QUANTA`.
- **Note:** Defer this. Can be faked for the jam if needed.
