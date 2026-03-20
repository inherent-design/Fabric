# AI Behavior States

*These enums define the high-level state for all NPCs.*

## States
- **`IDLE`**: No target, stationary or short patrol path. Default state in Rest/End levels.
- **`PATROL`**: Moving along a predefined path.
- **`ATTACK`**: Moving towards and using abilities on a hostile target.
- **`FLEE`**: Moving away from a perceived threat.
- **`TRANSFORMING`**: Temporary state, plays VFX, cannot act.
- **`INTERACT`**: Special state for Rest level; faces player, can be targeted by non-combat functions.

## Core Logic Triggers
- `IF [target in range] AND [faction == HOSTILE] -> ATTACK`
- `IF [health < 25%] AND [faction != CHAOS] -> FLEE`
- `IF [hit by TRANSFUSE] -> TRANSFORMING`
- `IF [no target] AND [in combat level] -> PATROL`
