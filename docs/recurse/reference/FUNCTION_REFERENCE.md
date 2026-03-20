# Function Reference Guide - RECURSE
*Complete Spell/Function Library with Properties and Interactions*

## Function Classification System

**Categories**: PRIMITIVE | COMPOSITE | META
**Types**: MATTER | ENERGY | SPACE | TIME | INFORMATION
**Power Levels**: 0.1-0.3 (Basic) | 0.4-0.6 (Intermediate) | 0.7-0.9 (Advanced) | 1.0+ (Reality-Breaking)

---

# PRIMITIVE FUNCTIONS (5 Core Functions)

## 1. CREATE_MATTER
*Generate new physical matter from energy*

**Properties:**
- **Category**: PRIMITIVE
- **Type**: MATTER
- **Power Level**: 0.3
- **Energy Cost**: 10-50 (based on material complexity)
- **Cooldown**: 2.0 seconds
- **Range**: 5 meters
- **Stability Requirement**: 0.2

**Parameters:**
- `material_type`: STONE | METAL | WOOD | CRYSTAL | ORGANIC
- `amount`: 0.1-10.0 units
- `shape`: SPHERE | CUBE | CYLINDER | CUSTOM

**Interactions:**
- **Player**: Creates objects for tools, weapons, barriers
- **NPCs**: Basic meta-capability NPCs can create simple tools
- **World**: Increases local matter density, may affect physics
- **Other Functions**: Output can be target for TRANSFORM_MATTER, KINETIC_PUSH

**Special Properties:**
- Created matter has 0.8 base stability
- Material complexity affects cost: STONE(1x) | WOOD(1.2x) | METAL(2x) | CRYSTAL(3x)
- Large amounts (>5 units) cause noticeable world disturbance

---

## 2. KINETIC_PUSH
*Apply directional force to objects*

**Properties:**
- **Category**: PRIMITIVE  
- **Type**: ENERGY
- **Power Level**: 0.2
- **Energy Cost**: 5-30 (based on force magnitude)
- **Cooldown**: 0.5 seconds
- **Range**: 8 meters
- **Stability Requirement**: 0.1

**Parameters:**
- `direction`: Vector3 direction of force
- `magnitude`: 1.0-20.0 force units
- `area_of_effect`: 0.5-3.0 meter radius
- `duration`: 0.1-1.0 seconds (sustained force)

**Interactions:**
- **Player**: Movement, combat, environmental manipulation
- **NPCs**: All NPCs affected by physics respond to force
- **World**: Can trigger chain reactions, affect unstable structures
- **Other Functions**: Combines with CREATE_MATTER for projectiles

**Special Properties:**
- Force effectiveness varies by target mass (F=ma)
- Can affect multiple objects simultaneously
- Sustained force costs energy over time
- Works on energy fields and spatial constructs

---

## 3. TRANSFORM_MATTER
*Change material properties of existing matter*

**Properties:**
- **Category**: PRIMITIVE
- **Type**: MATTER
- **Power Level**: 0.4
- **Energy Cost**: 15-60 (based on transformation difficulty)
- **Cooldown**: 3.0 seconds
- **Range**: Touch/Direct contact
- **Stability Requirement**: 0.4

**Parameters:**
- `target_material`: Desired material type
- `transformation_rate`: 0.1-1.0 (gradual vs instant)
- `affected_volume`: 0.1-5.0 cubic meters

**Transformation Difficulty Matrix:**
```
     → STONE WOOD METAL CRYSTAL ORGANIC
STONE    1x   2x   3x    5x     4x
WOOD     2x   1x   4x    6x     2x  
METAL    4x   3x   1x    3x     5x
CRYSTAL  6x   5x   4x    1x     7x
ORGANIC  3x   1.5x 4x    6x     1x
```

**Interactions:**
- **Player**: Environmental puzzle solving, weapon crafting
- **NPCs**: High meta-capability NPCs can use for tools/weapons
- **World**: Can destabilize structures, create new interaction possibilities
- **Other Functions**: Can transform function-created matter

**Special Properties:**
- Transformation reduces target stability by 0.1
- Some materials resist transformation (stability check)
- Organic matter transformations are unpredictable in low stability zones

---

## 4. CREATE_PORTAL
*Generate spatial connections between two points*

**Properties:**
- **Category**: PRIMITIVE
- **Type**: SPACE
- **Power Level**: 0.6
- **Energy Cost**: 40 + (distance × 2) + (duration × 10)
- **Cooldown**: 8.0 seconds
- **Range**: Line of sight up to 50 meters
- **Stability Requirement**: 0.5

**Parameters:**
- `exit_position`: Target location (must be valid space)
- `portal_size`: 0.5-2.0 meter diameter
- `duration`: 2.0-10.0 seconds
- `bidirectional`: true/false (can travel both ways)

**Interactions:**
- **Player**: Fast travel, tactical repositioning, escape
- **NPCs**: Advanced NPCs can use for flanking, escape
- **World**: Temporary spatial discontinuity, affects local space stability
- **Other Functions**: Can portal energy fields, matter projectiles

**Special Properties:**
- Both endpoints must be in valid, unoccupied space
- Portal stability decreases over time
- Objects maintain momentum when traveling through
- Large objects may destabilize portal early

---

## 5. DILATE_TIME
*Alter temporal flow rate in localized area*

**Properties:**
- **Category**: PRIMITIVE
- **Type**: TIME
- **Power Level**: 0.8
- **Energy Cost**: 60 + (|1-time_factor| × 30) + (radius × 20)
- **Cooldown**: 12.0 seconds
- **Range**: 10 meters
- **Stability Requirement**: 0.7

**Parameters:**
- `time_factor`: 0.1-3.0 (0.5 = half speed, 2.0 = double speed)
- `radius`: 1.0-5.0 meter effect area
- `duration`: 1.0-5.0 seconds
- `affects_caster`: true/false

**Interactions:**
- **Player**: Tactical advantage, puzzle solving, escape
- **NPCs**: Affects NPC decision making and reaction times
- **World**: Highly destabilizing, causes reality anomalies
- **Other Functions**: Affects speed of other functions within area

**Special Properties:**
- Extreme time factors (>2.5x or <0.2x) risk temporal paradoxes
- NPCs experience confusion when time returns to normal
- Reduces world stability by 0.2-0.5 in effect area
- FBI agents spawn if used repeatedly (>3 times in short period)

---

# COMPOSITE FUNCTIONS (7 Combinations)

## 6. MATTER_PROJECTILE
*CREATE_MATTER + KINETIC_PUSH chained execution*

**Properties:**
- **Category**: COMPOSITE
- **Type**: MATTER + ENERGY
- **Power Level**: 0.4
- **Energy Cost**: 25-70 (combined cost with 10% efficiency bonus)
- **Cooldown**: 2.5 seconds
- **Range**: 15 meters
- **Stability Requirement**: 0.3

**Composition**: CREATE_MATTER → KINETIC_PUSH
- Creates matter projectile at caster position
- Immediately applies kinetic force toward target
- Projectile exists for duration of flight + 3 seconds on impact

**Parameters:**
- `projectile_material`: Affects damage and physics
- `projectile_size`: 0.1-1.0 units
- `launch_force`: 5.0-15.0 force units
- `trajectory`: DIRECT | ARCING | HOMING

**Combat Effectiveness:**
- **Damage**: 15-45 (based on material and force)
- **Penetration**: Varies by material (METAL > STONE > WOOD)
- **Area Effect**: CRYSTAL projectiles shatter on impact

---

## 7. PHASE_DASH
*TRANSFORM_MATTER (self to ethereal) + KINETIC_PUSH + TRANSFORM_MATTER (restore)*

**Properties:**
- **Category**: COMPOSITE
- **Type**: MATTER + ENERGY + MATTER
- **Power Level**: 0.5
- **Energy Cost**: 45
- **Cooldown**: 6.0 seconds
- **Range**: 8 meters
- **Stability Requirement**: 0.4

**Composition**: TRANSFORM_MATTER(self→ETHEREAL) → KINETIC_PUSH(forward) → TRANSFORM_MATTER(ETHEREAL→original)
- Player becomes temporarily non-solid
- Propelled forward through obstacles
- Restored to solid form at destination

**Parameters:**
- `dash_distance`: 3.0-8.0 meters
- `dash_speed`: Fast movement multiplier
- `phase_duration`: 0.3-0.8 seconds

**Special Properties:**
- Can pass through walls and enemies during phase
- Vulnerable to energy attacks while ethereal
- Disorienting for NPCs (lose track of player temporarily)

---

## 8. BARRIER_WALL
*CREATE_MATTER + CREATE_MATTER + CREATE_MATTER (parallel execution)*

**Properties:**
- **Category**: COMPOSITE
- **Type**: MATTER (parallel)
- **Power Level**: 0.3
- **Energy Cost**: 35-80 (3× CREATE_MATTER with 15% bulk bonus)
- **Cooldown**: 4.0 seconds
- **Range**: 6 meters
- **Stability Requirement**: 0.3

**Composition**: CREATE_MATTER × 3 (parallel)
- Creates three connected matter blocks
- Forms wall-like structure automatically
- Optimized placement for maximum coverage

**Parameters:**
- `wall_material`: STONE | METAL | CRYSTAL
- `wall_height`: 1.5-3.0 meters
- `wall_length`: 3.0-6.0 meters
- `wall_thickness`: 0.3-0.8 meters

**Tactical Properties:**
- Blocks line of sight and movement
- Can be destroyed by sufficient damage
- CRYSTAL walls reflect energy attacks

---

## 9. PORTAL_STRIKE
*CREATE_PORTAL + MATTER_PROJECTILE (chained)*

**Properties:**
- **Category**: COMPOSITE
- **Type**: SPACE + MATTER + ENERGY
- **Power Level**: 0.7
- **Energy Cost**: 85-120
- **Cooldown**: 10.0 seconds
- **Range**: 25 meters (indirect)
- **Stability Requirement**: 0.6

**Composition**: CREATE_PORTAL → MATTER_PROJECTILE
- Opens portal near target
- Fires projectile through portal for flanking attack
- Portal closes after projectile passes through

**Parameters:**
- `portal_placement`: BEHIND | ABOVE | SIDE
- `projectile_type`: Standard MATTER_PROJECTILE parameters
- `attack_angle`: Approach vector for projectile

**Tactical Advantages:**
- Bypasses cover and shields
- Difficult for NPCs to predict/defend
- Can surprise even high-intelligence enemies

---

## 10. TEMPORAL_BUBBLE
*DILATE_TIME + CREATE_MATTER (barrier) parallel*

**Properties:**
- **Category**: COMPOSITE
- **Type**: TIME + MATTER
- **Power Level**: 0.8
- **Energy Cost**: 95-140
- **Cooldown**: 15.0 seconds
- **Range**: 4 meters
- **Stability Requirement**: 0.7

**Composition**: DILATE_TIME + CREATE_MATTER(barrier) (parallel)
- Creates slowed-time zone with physical barrier
- Barrier provides protection while time effect provides tactical advantage
- Both effects synchronized duration

**Parameters:**
- `time_factor`: 0.3-0.8 (slowed time)
- `barrier_strength`: LIGHT | MEDIUM | HEAVY
- `bubble_duration`: 3.0-8.0 seconds

**Strategic Uses:**
- Defensive position with time to plan
- Healing/recovery opportunity
- NPCs outside bubble move normally, creating tactical complexity

---

## 11. KINETIC_WAVE
*KINETIC_PUSH recursive application*

**Properties:**
- **Category**: COMPOSITE
- **Type**: ENERGY (recursive)
- **Power Level**: 0.5
- **Energy Cost**: 40-80 (scales with recursion depth)
- **Cooldown**: 7.0 seconds
- **Range**: 12 meters (expanding)
- **Stability Requirement**: 0.4

**Composition**: KINETIC_PUSH → KINETIC_PUSH → KINETIC_PUSH (recursive chain)
- Each iteration creates expanding force wave
- Force diminishes with distance but affects larger area
- Recursion continues until energy insufficient

**Parameters:**
- `initial_force`: 8.0-15.0 force units
- `wave_expansion`: 2.0-4.0 meters per iteration
- `force_falloff`: 0.7-0.9 (force retention per iteration)

**Area Effect:**
- Affects all objects in expanding radius
- Can knock down multiple NPCs
- Destabilizes environmental objects

---

## 12. MATTER_CASCADE
*TRANSFORM_MATTER recursive on adjacent objects*

**Properties:**
- **Category**: COMPOSITE
- **Type**: MATTER (recursive)
- **Power Level**: 0.6
- **Energy Cost**: 30 + (15 × transformed_objects)
- **Cooldown**: 8.0 seconds
- **Range**: Initial touch, then spreads
- **Stability Requirement**: 0.5

**Composition**: TRANSFORM_MATTER → TRANSFORM_MATTER → ... (recursive spread)
- Transforms initial target
- Spreads to adjacent similar materials
- Continues until no valid targets or energy depleted

**Parameters:**
- `target_material`: Desired transformation result
- `spread_criteria`: SAME_MATERIAL | ANY_MATTER | ORGANIC_ONLY
- `max_spread`: 3-10 objects maximum

**Environmental Impact:**
- Can transform entire structures
- Creates dramatic environmental changes
- NPCs react strongly to large-scale reality alterations

---

# META-FUNCTIONS (3 Advanced)

## 13. FUNCTION_AMPLIFY
*Increase power of next function used*

**Properties:**
- **Category**: META
- **Type**: INFORMATION
- **Power Level**: 0.7
- **Energy Cost**: 50 + (amplification_factor × 20)
- **Cooldown**: 10.0 seconds
- **Range**: Self-buff
- **Stability Requirement**: 0.6

**Effect:**
- Next function used gains amplification multiplier
- Affects power, range, and duration
- Does not affect energy cost (risk/reward balance)

**Parameters:**
- `amplification_factor`: 1.5-3.0× multiplier
- `duration`: 8.0-15.0 seconds (buff window)
- `function_types`: ALL | MATTER_ONLY | ENERGY_ONLY | etc.

**Strategic Use:**
- Prepare powerful combo attacks
- Overcome high-stability areas
- Risk management (high cost, high reward)

---

## 14. ADAPTIVE_FUNCTION
*AI-generated function based on context*

**Properties:**
- **Category**: META
- **Type**: INFORMATION
- **Power Level**: 0.9
- **Energy Cost**: 70-120 (varies by generated function)
- **Cooldown**: 20.0 seconds
- **Range**: Contextual
- **Stability Requirement**: 0.8

**AI Generation Process:**
1. Analyze current situation (enemies, environment, player state)
2. Identify optimal function combination
3. Generate custom composite function
4. Execute with contextual parameters

**Context Analysis:**
- **Combat**: Generates offensive/defensive combinations
- **Puzzle**: Creates environmental manipulation solutions
- **Exploration**: Produces movement/access functions
- **Social**: Develops NPC interaction possibilities

**Risk Factors:**
- Unpredictable results in low-stability areas
- May generate functions beyond player's normal capability
- FBI agents always triggered by use

---

## 15. REALITY_COMPOSE
*Direct manipulation of world data structures*

**Properties:**
- **Category**: META
- **Type**: INFORMATION
- **Power Level**: 1.0+ (Reality-Breaking)
- **Energy Cost**: 150-300
- **Cooldown**: 30.0 seconds
- **Range**: 20 meters
- **Stability Requirement**: 0.9

**Capabilities:**
- Directly edit world node properties
- Create impossible geometries
- Violate conservation laws temporarily
- Generate paradoxical objects

**Parameters:**
- `edit_type`: PROPERTY_CHANGE | STRUCTURE_MODIFY | PHYSICS_VIOLATE
- `scope`: SINGLE_OBJECT | LOCAL_AREA | REALITY_PATCH
- `violation_intensity`: 0.1-1.0 (how much physics is broken)

**Consequences:**
- Massive world stability reduction (-0.3 to -0.8)
- Guaranteed FBI agent response
- May spawn Terries if used multiple times
- Reality may "snap back" violently when effect ends

**Endgame Function:**
- Primary tool for final level challenges
- Required for changing fate/breaking the loop
- Ultimate expression of player agency vs predetermined destiny

---

# Function Interaction Matrix

## Synergistic Combinations
- **MATTER + ENERGY**: Enhanced combat effectiveness
- **SPACE + TIME**: Complex tactical maneuvers  
- **MATTER + SPACE**: Environmental restructuring
- **ENERGY + TIME**: Temporal combat techniques
- **Any + META**: Amplified effects and capabilities

## Conflicting Combinations
- **CREATE_MATTER + TRANSFORM_MATTER** (same target): Reality confusion
- **Multiple TIME functions**: Temporal paradox risk
- **PORTAL + DILATE_TIME**: Spatial-temporal instability
- **High-power functions in low stability**: Chaotic results

## Learning Progression
1. **Basic Functions** (1-5): Learn individual capabilities
2. **Simple Composites** (6-9): Understand chaining
3. **Complex Composites** (10-12): Master parallel and recursive execution
4. **Meta Functions** (13-15): Reality manipulation mastery

## NPC Response Scaling
- **0.1-0.3 Power**: Minimal NPC reaction
- **0.4-0.6 Power**: Increased caution, some flee
- **0.7-0.8 Power**: FBI investigation triggered
- **0.9+ Power**: Terry spawn, reality enforcement response

This function system provides deep mechanical complexity while maintaining intuitive learning progression and meaningful player choice in problem-solving approaches.