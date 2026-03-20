# Procedural World Generation System - RECURSE
*Reality Composition Through Function-Based World Manipulation*

## Core Philosophy

The world is **malleable reality** that responds to player functions and environmental pressures. Rather than static levels, we generate **responsive environments** that evolve based on player actions, NPC behaviors, and temporal mechanics. The system prioritizes **systemic emergence** over scripted sequences.

---

# Function-Based Reality Manipulation

## Core Concept: Reality as Data Structure

Reality is represented as a **hierarchical node system** that can be modified through **function calls**. Every element in the world has **composition properties** that determine how it responds to manipulation.

```cpp
struct WorldNode {
    enum Type {
        MATTER,      // Physical objects, terrain
        ENERGY,      // Forces, fields, temporal flow
        SPACE,       // Dimensional structure, connectivity
        INFORMATION  // Memory, knowledge, causality
    };
    
    Type type;
    Vector3 position;
    float stability;        // Resistance to change (0.0 to 1.0)
    float malleability;     // Ease of transformation (0.0 to 1.0)
    float temporal_anchor;  // Connection to timeline (0.0 to 1.0)
    
    std::vector<WorldNode*> dependencies;
    std::map<string, float> properties;
};
```

## Function Framework

### Function Categories
1. **Matter Functions**: Direct physical manipulation
2. **Energy Functions**: Force and field manipulation  
3. **Space Functions**: Dimensional and connectivity manipulation
4. **Time Functions**: Temporal flow manipulation
5. **Information Functions**: Causality and memory manipulation

### Function Properties
```cpp
struct GameFunction {
    string name;
    FunctionType type;
    float cost;             // Energy/resource cost
    float cooldown;         // Time before reuse
    float range;            // Effective distance
    float power;            // Magnitude of effect
    float stability_requirement; // Minimum world stability needed
    
    std::vector<string> compatible_materials;
    std::vector<string> incompatible_materials;
    std::function<void(WorldNode&, float)> execute;
};
```

---

# Procedural Generation Architecture

## Multi-Scale Generation

### Macro Scale: World Structure
```cpp
class WorldGenerator {
private:
    struct WorldRegion {
        Vector3 center;
        float radius;
        BiomeType biome;
        float temporal_distortion; // How much time flows differently
        float reality_stability;   // How resistant to change
        MemoryTheme theme;         // What life period this represents
    };
    
public:
    void generateWorldStructure(int seed, LifePhase phase) {
        // Generate based on reverse chronological order
        switch (phase) {
            case RECENT_MEMORIES:
                generateUrbanEnvironment(seed, high_complexity);
                break;
            case ADOLESCENT_MEMORIES:
                generateSuburbanEnvironment(seed, medium_complexity);
                break;
            case CHILDHOOD_MEMORIES:
                generatePlaygroundEnvironment(seed, low_complexity);
                break;
        }
    }
};
```

### Meso Scale: Level Architecture  
```cpp
class LevelGenerator {
private:
    struct LevelModule {
        string name;
        Vector3 size;
        std::vector<ConnectionPoint> connections;
        std::vector<InteractionZone> interactive_areas;
        float difficulty_rating;
        NPCSpawnParameters npc_spawns;
    };
    
public:
    Level generateLevel(WorldRegion& region, float player_progress) {
        Level level;
        
        // Select appropriate modules for this memory phase
        auto modules = selectModulesForPhase(region.theme);
        
        // Arrange modules with procedural connectivity
        arrangeModules(level, modules, region.size);
        
        // Add interactive elements based on function compatibility
        addInteractiveElements(level, region.reality_stability);
        
        // Populate with NPCs based on memory theme
        populateWithNPCs(level, region.theme, player_progress);
        
        return level;
    }
};
```

### Micro Scale: Interactive Details
```cpp
class DetailGenerator {
public:
    void addInteractiveElements(Level& level, float reality_stability) {
        for (auto& zone : level.interaction_zones) {
            // Add elements based on how stable reality is here
            if (reality_stability > 0.8f) {
                // Stable reality: traditional physics objects
                addPhysicsObjects(zone, NORMAL_PHYSICS);
            } else if (reality_stability > 0.5f) {
                // Unstable reality: enhanced function responsiveness
                addEnhancedObjects(zone, AMPLIFIED_FUNCTIONS);
            } else {
                // Chaotic reality: unpredictable interactions
                addChaoticElements(zone, EMERGENT_BEHAVIORS);
            }
        }
    }
};
```

---

# Function System Implementation

## Function Categories & Mechanics

### 1. Matter Functions

#### Create Matter
```cpp
void executeMatterCreation(Vector3 target_pos, MaterialType material, float amount) {
    WorldNode new_node;
    new_node.type = MATTER;
    new_node.position = target_pos;
    new_node.properties["density"] = getMmaterialDensity(material);
    new_node.properties["durability"] = getMaterialDurability(material);
    new_node.stability = 0.8f; // Created matter is fairly stable
    
    world.addNode(new_node);
    
    // Cost scales with amount and material complexity
    float cost = amount * getMaterialComplexity(material);
    player.consumeEnergy(cost);
}
```

#### Transform Matter
```cpp
void executeMatterTransformation(WorldNode& target, MaterialType new_material) {
    if (target.type != MATTER) return;
    
    float transformation_difficulty = calculateTransformationCost(
        target.properties["material_type"], new_material);
    
    if (player.energy < transformation_difficulty) return;
    
    // Transform properties
    target.properties["material_type"] = new_material;
    target.properties["density"] = getMaterialDensity(new_material);
    target.stability *= 0.9f; // Transformation reduces stability slightly
    
    player.consumeEnergy(transformation_difficulty);
}
```

### 2. Energy Functions

#### Kinetic Manipulation
```cpp
void executeKineticPush(Vector3 origin, Vector3 direction, float force) {
    auto affected_objects = getObjectsInCone(origin, direction, KINETIC_RANGE);
    
    for (auto& obj : affected_objects) {
        Vector3 push_vector = normalize(obj.position - origin) * force;
        
        // Force effectiveness depends on object properties
        float mass = obj.properties["mass"];
        float acceleration = force / mass;
        
        obj.velocity += push_vector * acceleration;
        
        // Affect stability based on force magnitude
        obj.stability -= force * 0.1f;
    }
    
    player.consumeEnergy(force * KINETIC_ENERGY_COST);
}
```

#### Energy Field Creation
```cpp
void createEnergyField(Vector3 center, FieldType field_type, float intensity) {
    WorldNode field_node;
    field_node.type = ENERGY;
    field_node.position = center;
    field_node.properties["field_type"] = field_type;
    field_node.properties["intensity"] = intensity;
    field_node.properties["radius"] = intensity * FIELD_RADIUS_MULTIPLIER;
    
    // Fields affect nearby objects continuously
    field_node.update_function = [](WorldNode& field, float dt) {
        auto nearby_objects = getObjectsInRadius(field.position, 
                                               field.properties["radius"]);
        
        for (auto& obj : nearby_objects) {
            applyFieldEffect(field, obj, dt);
        }
    };
    
    world.addNode(field_node);
    player.consumeEnergy(intensity * FIELD_CREATION_COST);
}
```

### 3. Space Functions

#### Portal Creation
```cpp
void createPortal(Vector3 entrance_pos, Vector3 exit_pos, float duration) {
    // Create entrance portal
    WorldNode entrance;
    entrance.type = SPACE;
    entrance.position = entrance_pos;
    entrance.properties["portal_type"] = "entrance";
    entrance.properties["exit_position"] = exit_pos;
    entrance.properties["duration"] = duration;
    entrance.stability = 0.3f; // Portals are inherently unstable
    
    // Create exit portal
    WorldNode exit;
    exit.type = SPACE;
    exit.position = exit_pos;
    exit.properties["portal_type"] = "exit";
    exit.properties["entrance_position"] = entrance_pos;
    exit.properties["duration"] = duration;
    exit.stability = 0.3f;
    
    // Link portals
    entrance.dependencies.push_back(&exit);
    exit.dependencies.push_back(&entrance);
    
    world.addNode(entrance);
    world.addNode(exit);
    
    float cost = length(exit_pos - entrance_pos) * PORTAL_DISTANCE_COST + 
                 duration * PORTAL_DURATION_COST;
    player.consumeEnergy(cost);
}
```

#### Dimensional Pocket
```cpp
void createDimensionalPocket(Vector3 anchor_pos, Vector3 pocket_size) {
    WorldNode pocket;
    pocket.type = SPACE;
    pocket.position = anchor_pos;
    pocket.properties["pocket_size"] = pocket_size;
    pocket.properties["capacity"] = pocket_size.x * pocket_size.y * pocket_size.z;
    pocket.stability = 0.5f;
    
    // Create internal space
    Level pocket_level;
    pocket_level.bounds = BoundingBox(Vector3::zero, pocket_size);
    pocket_level.physics_properties.gravity = Vector3(0, -9.8f, 0);
    
    pocket.internal_space = pocket_level;
    
    world.addNode(pocket);
    player.consumeEnergy(pocket.properties["capacity"] * POCKET_SPACE_COST);
}
```

### 4. Time Functions

#### Temporal Dilation
```cpp
void createTemporalDilation(Vector3 center, float radius, float time_factor, float duration) {
    WorldNode time_field;
    time_field.type = ENERGY;
    time_field.position = center;
    time_field.properties["time_factor"] = time_factor; // 0.5 = half speed, 2.0 = double speed
    time_field.properties["radius"] = radius;
    time_field.properties["duration"] = duration;
    
    time_field.update_function = [](WorldNode& field, float dt) {
        auto affected_objects = getObjectsInRadius(field.position, 
                                                 field.properties["radius"]);
        
        float time_factor = field.properties["time_factor"];
        for (auto& obj : affected_objects) {
            obj.time_scale = time_factor;
            
            // NPCs experience time dilation
            if (obj.npc_component) {
                obj.npc_component->time_perception *= time_factor;
            }
        }
    };
    
    world.addNode(time_field);
    
    float cost = abs(1.0f - time_factor) * radius * duration * TIME_MANIPULATION_COST;
    player.consumeEnergy(cost);
}
```

### 5. Information Functions

#### Memory Materialization
```cpp
void materializeMemory(Vector3 position, MemoryFragment memory) {
    WorldNode memory_node;
    memory_node.type = INFORMATION;
    memory_node.position = position;
    memory_node.properties["memory_type"] = memory.type;
    memory_node.properties["emotional_intensity"] = memory.emotional_weight;
    memory_node.stability = memory.clarity; // Clear memories are more stable
    
    // Create physical manifestation
    MaterialType manifestation_material = getMemoryMaterial(memory.emotional_tone);
    WorldNode physical_form;
    physical_form.type = MATTER;
    physical_form.position = position;
    physical_form.properties["material_type"] = manifestation_material;
    physical_form.properties["memory_link"] = memory.id;
    
    // Link information and physical nodes
    memory_node.dependencies.push_back(&physical_form);
    physical_form.dependencies.push_back(&memory_node);
    
    world.addNode(memory_node);
    world.addNode(physical_form);
    
    player.consumeEnergy(memory.emotional_weight * MEMORY_MATERIALIZATION_COST);
}
```

---

# World Responsiveness System

## Environmental Reactions
```cpp
class EnvironmentalReaction {
public:
    void onFunctionUsed(GameFunction& function, Vector3 location, float power) {
        // World stability affects function effectiveness
        float local_stability = world.getStabilityAt(location);
        float modified_power = power * calculateStabilityModifier(local_stability);
        
        // Update world stability based on function usage
        updateWorldStability(location, function.type, modified_power);
        
        // Trigger environmental chain reactions
        propagateEffects(location, function.type, modified_power);
        
        // Notify NPCs of reality manipulation
        notifyNPCsOfAnomaly(location, modified_power);
    }
    
private:
    void updateWorldStability(Vector3 location, FunctionType type, float power) {
        float stability_change = -power * STABILITY_DEGRADATION_RATE;
        
        // Some functions are more destabilizing than others
        switch (type) {
            case TIME_MANIPULATION:
                stability_change *= 2.0f; // Time functions are very destabilizing
                break;
            case SPACE_MANIPULATION:
                stability_change *= 1.5f; // Space functions moderately destabilizing
                break;
            case MATTER_MANIPULATION:
                stability_change *= 1.0f; // Matter functions baseline destabilizing
                break;
        }
        
        world.modifyStabilityAt(location, stability_change, STABILITY_EFFECT_RADIUS);
    }
    
    void propagateEffects(Vector3 epicenter, FunctionType type, float power) {
        auto nearby_nodes = world.getNodesInRadius(epicenter, power * EFFECT_RADIUS);
        
        for (auto& node : nearby_nodes) {
            float distance = length(node.position - epicenter);
            float effect_strength = power * (1.0f / (1.0f + distance * DISTANCE_FALLOFF));
            
            // Different node types react differently
            switch (node.type) {
                case MATTER:
                    applyMatterEffects(node, type, effect_strength);
                    break;
                case ENERGY:
                    applyEnergyEffects(node, type, effect_strength);
                    break;
                case SPACE:
                    applySpaceEffects(node, type, effect_strength);
                    break;
            }
        }
    }
};
```

## Reality Stability Zones
```cpp
enum StabilityLevel {
    CHAOS,      // 0.0-0.2: Reality highly malleable, unpredictable effects
    UNSTABLE,   // 0.2-0.5: Enhanced function effectiveness, some randomness  
    NORMAL,     // 0.5-0.8: Standard physics, predictable function behavior
    RIGID       // 0.8-1.0: Resistant to change, functions less effective
};

class StabilityZone {
private:
    Vector3 center;
    float radius;
    StabilityLevel level;
    float current_stability;
    
public:
    void updateZone(float deltaTime) {
        // Stability gradually recovers over time
        current_stability += STABILITY_RECOVERY_RATE * deltaTime;
        current_stability = clamp(current_stability, 0.0f, 1.0f);
        
        // Update level based on current stability
        updateStabilityLevel();
        
        // Apply zone effects to contained objects
        applyZoneEffects();
    }
    
private:
    void applyZoneEffects() {
        auto contained_objects = world.getObjectsInRadius(center, radius);
        
        for (auto& obj : contained_objects) {
            switch (level) {
                case CHAOS:
                    applyChaosEffects(obj);
                    break;
                case UNSTABLE:
                    applyUnstableEffects(obj);
                    break;
                case NORMAL:
                    applyNormalPhysics(obj);
                    break;
                case RIGID:
                    applyRigidConstraints(obj);
                    break;
            }
        }
    }
    
    void applyChaosEffects(WorldNode& obj) {
        // Random property fluctuations
        for (auto& [key, value] : obj.properties) {
            value += (random(-1.0f, 1.0f) * CHAOS_FLUCTUATION_STRENGTH);
        }
        
        // Spontaneous transformations
        if (random(0.0f, 1.0f) < CHAOS_TRANSFORMATION_CHANCE) {
            spontaneousTransformation(obj);
        }
        
        // Enhanced function responsiveness
        obj.function_responsiveness = 2.0f;
    }
};
```

---

# Integration with Game Systems

## Player Function Casting
```cpp
class PlayerFunctionSystem {
private:
    std::vector<GameFunction> available_functions;
    float current_energy;
    std::map<string, float> cooldowns;
    
public:
    bool castFunction(string function_name, Vector3 target_position) {
        auto function = getFunctionByName(function_name);
        if (!function) return false;
        
        // Check requirements
        if (current_energy < function->cost) return false;
        if (cooldowns[function_name] > 0) return false;
        
        // Check world stability requirements
        float local_stability = world.getStabilityAt(target_position);
        if (local_stability < function->stability_requirement) {
            // Function might fail or have unpredictable effects
            if (random(0.0f, 1.0f) > local_stability) {
                executeUnpredictableFunction(*function, target_position);
                return true;
            }
        }
        
        // Execute function normally
        function->execute(world.getNodeAt(target_position), function->power);
        
        // Apply costs and cooldowns
        current_energy -= function->cost;
        cooldowns[function_name] = function->cooldown;
        
        // Notify environmental system
        environment.onFunctionUsed(*function, target_position, function->power);
        
        return true;
    }
};
```

## NPC Function Detection & Response
```cpp
void onFunctionDetected(NPCBehavior& npc, GameFunction& function, Vector3 location, float power) {
    // Add to NPC's memory
    NPCMemory memory;
    memory.type = FUNCTION_USAGE;
    memory.location = location;
    memory.emotional_intensity = power;
    memory.timestamp = current_time;
    npc.addMemory(memory);
    
    // Update threat assessment
    float threat_increase = power * FUNCTION_THREAT_MULTIPLIER;
    if (function.type == TIME_MANIPULATION || function.type == SPACE_MANIPULATION) {
        threat_increase *= 2.0f; // Reality-altering functions more threatening
    }
    npc.modifyPlayerThreat(threat_increase);
    
    // Trigger faction responses
    if (power > HIGH_LEVEL_FUNCTION_THRESHOLD) {
        triggerFactionResponse(npc.faction, location, power);
    }
}
```

This procedural world system creates a living, responsive environment where player actions have lasting consequences and reality itself becomes a malleable medium for creative problem-solving and emergent storytelling.