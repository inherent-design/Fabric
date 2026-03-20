# NPC Transformation Rules

**Essence Stats:** [O: Order, C: Chaos, L: Life, D: Decay]

| ID | Name          | Base Essence | Base Form      | Transformation Trigger      | Transformed Form | Notes                               |
|----|---------------|--------------|----------------|-----------------------------|------------------|-------------------------------------|
| 1  | Stone Golem   | [0.8,0,0,0.2]| Rock monster   | `TRANSFUSE_ESSENCE(LIFE)`   | Treant           | Becomes allied to player.           |
| 2  | Treant        | [0.2,0,0.8,0]| Tree creature  | `TRANSFUSE_ESSENCE(ORDER)`  | Stone Golem      | Becomes neutral.                    |
| 3  | Chaos Imp     | [0,0.9,0,0.1]| Small demon    | `TRANSFUSE_ESSENCE(ORDER)`  | Iron Sentinel    | Becomes stationary turret.          |
| 4  | Iron Sentinel | [0.9,0,0,0.1]| Metal statue   | `TRANSFUSE_ESSENCE(CHAOS)`  | Chaos Imp        | Becomes mobile attacker.            |
| 5  | Shambler      | [0,0.5,0,0.5]| Zombie         | `TRANSFUSE_ESSENCE(LIFE)`   | Human Villager   | Becomes non-hostile, flees.         |
| 6  | Human Villager| [0.1,0,0.7,0]| Humanoid       | `TRANSFUSE_ESSENCE(DECAY)`  | Shambler         | Becomes hostile.                    |
| 7  | Fire Elemental| [0,0.7,0.3,0]| Fire creature  | `TIME_DILATION(DECELERATE)` | Cooled Magma     | Becomes temporary terrain.          |
| 8  | Cooled Magma  | [0.3,0,0,0]  | Hardened rock  | `TIME_DILATION(ACCELERATE)` | Fire Elemental   | Re-ignites, becomes hostile.        |
