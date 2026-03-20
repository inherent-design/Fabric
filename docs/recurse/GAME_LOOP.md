# Game Loop: C-C-R-E

*One continuous world. Levels are defined by spawning specific NPCs and environmental setups in the VOID.*

- **MENU:** Player is in the VOID. A single "Shatter" function starts the game.

- **LEVEL 1: Combat (Intro)**
  - **Objective:** Survive 2 waves of Chaos Imps.
  - **Environment:** A few large, pre-existing rock platforms.
  - **Unlocks:** `CREATE_MATTER`, `DESTROY_MATTER`. Player learns to make/destroy cover.

- **LEVEL 2: Combat (Transformation)**
  - **Objective:** Transform 2 Stone Golems into Treants.
  - **Environment:** A mix of rock platforms and small patches of "Life" terrain.
  - **Unlocks:** `TRANSFUSE_ESSENCE`, `TIME_DILATION`. Player learns to change NPC allegiance and manipulate time.

- **LEVEL 3: Rest (Puzzle)**
  - **Objective:** A friendly Treant NPC needs to cross a chasm.
  - **Environment:** Two platforms separated by a wide gap.
  - **Solution:** Player must use `CREATE_MATTER` to build a bridge. No enemies.

- **LEVEL 4: Ending (Composition)**
  - **Objective:** "Re-compose" the shattered snow globe core.
  - **Environment:** A central platform with 3 orbiting "dead" nodes.
  - **Solution:** Player must hit each node with a specific function combo (e.g., Node 1 needs `CREATE_MATTER` + `SHAPE_SPHERE`, Node 2 needs
`TRANSFUSE_ESSENCE(LIFE)`).
  - **End:** Hitting all 3 nodes correctly triggers the end credit sequence.
