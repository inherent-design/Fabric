# Function-Based Meta-Manipulation System - RECURSE
*Reality Composition Through Procedural Generation Functions*

## Core Philosophy

Functions are **procedural reality generators** that allow the player to compose and manipulate the fundamental structure of the world. Rather than traditional "spells," functions are **computational instructions** that modify the underlying data structures of reality itself.

**Core Concept**: The player is a **procedural programmer of reality**, using function composition to solve problems and manipulate the world at multiple abstraction levels.

---

# Meta-System Architecture

## Function as First-Class Objects
```cpp
struct GameFunction {
    string name;
    FunctionCategory category;
    float cost;                    // Energy consumption
    float cooldown;               // Reuse timer
    float range;                  // Effective distance
    float power;                  // Base magnitude
    float stability_requirement;   // Min world stability needed
    
    // Composition properties
    std::vector<string> input_types;    // What this function can accept
    string output_type;                 // What this function produces
    bool is_chainable;                  // Can be composed with other functions
    
    // Execution context
    std::function<FunctionResult(ExecutionContext&)> execute;
    std::function<bool(ExecutionContext&)> can_execute;
    std::function<float(ExecutionContext&)> calculate_cost;
};
```

## Function Categories

### 1. Primitive Functions
**Low-level reality manipulation - direct data structure modification**

#### Matter Primitives
- **create_matter**: Generate new matter nodes
- **destroy_matter**: Remove matter nodes  
- **transform_matter**: Change material properties
- **move_matter**: Modify position/velocity

#### Energy Primitives
- **create_force**: Generate kinetic energy
- **create_field**: Generate energy fields
- **absorb_energy**: Consume existing energy
- **redirect_energy**: Change energy flow direction

#### Space Primitives
- **create_space**: Generate new spatial nodes
- **connect_space**: Create spatial relationships
- **fold_space**: Modify spatial connectivity
- **expand_space**: Alter spatial dimensions

#### Time Primitives
- **dilate_time**: Modify temporal flow rate
- **create_temporal_anchor**: Fix points in time
- **temporal_echo**: Create time-based duplicates
- **causal_link**: Create cause-effect relationships

### 2. Composite Functions
**Higher-level operations composed of primitives**

#### Construction Composites
- **build_structure**: Composed matter creation with spatial organization
- **create_barrier**: Matter + space + energy composition
- **manifest_tool**: Matter + energy + information composition

#### Movement Composites
- **teleport**: Space folding + matter relocation
- **flight**: Continuous force generation + momentum management
- **phase_shift**: Matter transformation + spatial manipulation

#### Combat Composites
- **projectile**: Matter creation + kinetic energy + trajectory calculation
- **area_denial**: Space modification + energy field + temporal persistence
- **defensive_matrix**: Barrier creation + energy redirection + spatial folding

### 3. Meta-Functions
**Functions that operate on other functions**

#### Function Manipulation
- **amplify**: Increase another function's power
- **chain**: Execute multiple functions in sequence
- **parallel**: Execute multiple functions simultaneously
- **conditional**: Execute function based on world state

#### Procedural Generation
- **generate_pattern**: Create repeating function applications
- **recursive_apply**: Apply function to its own output
- **iterate_until**: Repeat function until condition met
- **optimize_for**: Automatically adjust function parameters

---

# Function Execution Engine

## Execution Context
```cpp
struct ExecutionContext {
    Vector3 target_position;
    WorldNode* target_node;
    Player* caster;
    float available_energy;
    float world_stability;
    
    // Meta-context for function composition
    std::vector<FunctionResult> previous_results;
    std::map<string, float> parameters;
    ExecutionMode mode; // NORMAL, CHAINED, PARALLEL, RECURSIVE
};

enum ExecutionMode {
    NORMAL,      // Standard single function execution
    CHAINED,     // Sequential function composition
    PARALLEL,    // Simultaneous function execution
    RECURSIVE,   // Self-referential function application
    CONDITIONAL  // Context-dependent function execution
};
```

## Function Composition System
```cpp
class FunctionComposer {
public:
    FunctionResult executeComposition(std::vector<GameFunction>& functions, 
                                    ExecutionContext& context) {
        switch (context.mode) {
            case CHAINED:
                return executeChained(functions, context);
            case PARALLEL:
                return executeParallel(functions, context);
            case RECURSIVE:
                return executeRecursive(functions, context);
            case CONDITIONAL:
                return executeConditional(functions, context);
            default:
                return executeSingle(functions[0], context);
        }
    }
    
private:
    FunctionResult executeChained(std::vector<GameFunction>& functions, 
                                ExecutionContext& context) {
        FunctionResult combined_result;
        
        for (auto& function : functions) {
            // Previous function results become input for next function
            if (!combined_result.outputs.empty()) {
                updateContextFromResult(context, combined_result);
            }
            
            auto result = function.execute(context);
            
            // Combine results
            combined_result.energy_cost += result.energy_cost;
            combined_result.world_changes.insert(combined_result.world_changes.end(),
                                               result.world_changes.begin(),
                                               result.world_changes.end());
            combined_result.outputs = result.outputs; // Latest output becomes final output
            
            // Check for chain breaking conditions
            if (!result.success || context.available_energy < 0) {
                combined_result.success = false;
                break;
            }
        }
        
        return combined_result;
    }
    
    FunctionResult executeParallel(std::vector<GameFunction>& functions,
                                 ExecutionContext& context) {
        FunctionResult combined_result;
        std::vector<FunctionResult> individual_results;
        
        // Execute all functions simultaneously
        for (auto& function : functions) {
            ExecutionContext function_context = context; // Copy context
            auto result = function.execute(function_context);
            individual_results.push_back(result);
        }
        
        // Combine results and check for conflicts
        combined_result = combineParallelResults(individual_results, context);
        
        return combined_result;
    }
    
    FunctionResult executeRecursive(std::vector<GameFunction>& functions,
                                  ExecutionContext& context) {
        if (functions.size() != 1) {
            // Recursive mode requires exactly one function
            return FunctionResult{.success = false};
        }
        
        auto& function = functions[0];
        FunctionResult combined_result;
        
        int iteration_count = 0;
        const int MAX_RECURSION = 10; // Prevent infinite loops
        
        while (iteration_count < MAX_RECURSION) {
            auto result = function.execute(context);
            
            // Combine results
            combined_result.energy_cost += result.energy_cost;
            combined_result.world_changes.insert(combined_result.world_changes.end(),
                                               result.world_changes.begin(),
                                               result.world_changes.end());
            
            // Check termination conditions
            if (!result.success || 
                context.available_energy < function.cost ||
                !shouldContinueRecursion(result, context)) {
                break;
            }
            
            // Update context with result for next iteration
            updateContextFromResult(context, result);
            iteration_count++;
        }
        
        return combined_result;
    }
};
```

## Dynamic Function Generation
```cpp
class FunctionGenerator {
public:
    GameFunction generateAdaptiveFunction(ExecutionContext& context, 
                                        ProblemType problem) {
        // Analyze the problem context
        auto analysis = analyzeProblem(context, problem);
        
        // Select appropriate primitive functions
        auto primitives = selectPrimitives(analysis);
        
        // Compose into new function
        GameFunction generated_function;
        generated_function.name = generateFunctionName(analysis);
        generated_function.category = COMPOSITE;
        generated_function.execute = createCompositeExecution(primitives);
        
        // Calculate properties based on constituent primitives
        generated_function.cost = calculateCompositeCost(primitives);
        generated_function.range = calculateCompositeRange(primitives);
        generated_function.power = calculateCompositePower(primitives);
        
        return generated_function;
    }
    
private:
    ProblemAnalysis analyzeProblem(ExecutionContext& context, ProblemType problem) {
        ProblemAnalysis analysis;
        
        switch (problem) {
            case NEED_MOVEMENT:
                analysis.required_categories = {ENERGY, SPACE};
                analysis.target_effect = "position_change";
                analysis.constraints = context.world_stability;
                break;
                
            case NEED_BARRIER:
                analysis.required_categories = {MATTER, SPACE};
                analysis.target_effect = "obstruction_creation";
                analysis.duration_requirement = calculateBarrierDuration(context);
                break;
                
            case NEED_ATTACK:
                analysis.required_categories = {MATTER, ENERGY};
                analysis.target_effect = "damage_application";
                analysis.range_requirement = calculateAttackRange(context);
                break;
        }
        
        return analysis;
    }
};
```

---

# Specific Function Implementations

## Matter Functions

### create_matter
```cpp
FunctionResult create_matter(ExecutionContext& context) {
    FunctionResult result;
    
    // Extract parameters
    MaterialType material = context.parameters["material_type"];
    float amount = context.parameters["amount"];
    Vector3 position = context.target_position;
    
    // Calculate cost based on material complexity
    float cost = amount * getMaterialComplexity(material) * MATTER_CREATION_COST;
    
    if (context.available_energy < cost) {
        result.success = false;
        result.failure_reason = "insufficient_energy";
        return result;
    }
    
    // Create new matter node
    WorldNode matter_node;
    matter_node.type = MATTER;
    matter_node.position = position;
    matter_node.properties["material_type"] = material;
    matter_node.properties["mass"] = amount * getMaterialDensity(material);
    matter_node.stability = 0.8f; // Created matter is fairly stable
    
    // Add to world
    world.addNode(matter_node);
    
    // Record result
    result.success = true;
    result.energy_cost = cost;
    result.world_changes.push_back({"node_created", matter_node.id});
    result.outputs["created_node_id"] = matter_node.id;
    
    return result;
}
```

### transform_matter
```cpp
FunctionResult transform_matter(ExecutionContext& context) {
    FunctionResult result;
    
    if (!context.target_node || context.target_node->type != MATTER) {
        result.success = false;
        result.failure_reason = "invalid_target";
        return result;
    }
    
    MaterialType current_material = context.target_node->properties["material_type"];
    MaterialType target_material = context.parameters["target_material"];
    
    // Calculate transformation difficulty
    float difficulty = calculateTransformationDifficulty(current_material, target_material);
    float cost = difficulty * TRANSFORMATION_BASE_COST;
    
    // Stability affects transformation success
    float success_chance = context.target_node->stability * context.world_stability;
    
    if (random(0.0f, 1.0f) > success_chance) {
        result.success = false;
        result.failure_reason = "transformation_failed";
        return result;
    }
    
    // Apply transformation
    context.target_node->properties["material_type"] = target_material;
    context.target_node->properties["density"] = getMaterialDensity(target_material);
    context.target_node->stability *= 0.9f; // Transformation reduces stability
    
    result.success = true;
    result.energy_cost = cost;
    result.world_changes.push_back({"node_transformed", context.target_node->id});
    
    return result;
}
```

## Energy Functions

### create_force
```cpp
FunctionResult create_force(ExecutionContext& context) {
    FunctionResult result;
    
    Vector3 force_direction = context.parameters["direction"];
    float force_magnitude = context.parameters["magnitude"];
    float duration = context.parameters["duration"];
    
    // Create force field node
    WorldNode force_node;
    force_node.type = ENERGY;
    force_node.position = context.target_position;
    force_node.properties["force_direction"] = force_direction;
    force_node.properties["force_magnitude"] = force_magnitude;
    force_node.properties["duration"] = duration;
    force_node.properties["remaining_time"] = duration;
    
    // Force field update function
    force_node.update_function = [](WorldNode& node, float dt) {
        node.properties["remaining_time"] -= dt;
        
        if (node.properties["remaining_time"] <= 0) {
            world.removeNode(node.id);
            return;
        }
        
        // Apply force to nearby objects
        auto nearby_objects = world.getObjectsInRadius(node.position, FORCE_RADIUS);
        
        Vector3 force_dir = node.properties["force_direction"];
        float force_mag = node.properties["force_magnitude"];
        
        for (auto& obj : nearby_objects) {
            if (obj.type == MATTER) {
                float mass = obj.properties["mass"];
                Vector3 acceleration = force_dir * (force_mag / mass);
                obj.velocity += acceleration * dt;
            }
        }
    };
    
    world.addNode(force_node);
    
    result.success = true;
    result.energy_cost = force_magnitude * duration * FORCE_CREATION_COST;
    result.world_changes.push_back({"force_field_created", force_node.id});
    
    return result;
}
```

## Space Functions

### create_portal
```cpp
FunctionResult create_portal(ExecutionContext& context) {
    FunctionResult result;
    
    Vector3 entrance_pos = context.target_position;
    Vector3 exit_pos = context.parameters["exit_position"];
    float duration = context.parameters["duration"];
    
    float distance = length(exit_pos - entrance_pos);
    float cost = distance * PORTAL_DISTANCE_COST + duration * PORTAL_DURATION_COST;
    
    if (context.available_energy < cost) {
        result.success = false;
        result.failure_reason = "insufficient_energy";
        return result;
    }
    
    // Check if exit position is valid
    if (!world.isPositionValid(exit_pos)) {
        result.success = false;
        result.failure_reason = "invalid_exit_position";
        return result;
    }
    
    // Create portal pair
    auto portal_pair = createPortalPair(entrance_pos, exit_pos, duration);
    
    world.addNode(portal_pair.entrance);
    world.addNode(portal_pair.exit);
    
    result.success = true;
    result.energy_cost = cost;
    result.world_changes.push_back({"portal_created", portal_pair.entrance.id});
    result.world_changes.push_back({"portal_created", portal_pair.exit.id});
    result.outputs["entrance_id"] = portal_pair.entrance.id;
    result.outputs["exit_id"] = portal_pair.exit.id;
    
    return result;
}
```

## Time Functions

### dilate_time
```cpp
FunctionResult dilate_time(ExecutionContext& context) {
    FunctionResult result;
    
    Vector3 center = context.target_position;
    float radius = context.parameters["radius"];
    float time_factor = context.parameters["time_factor"]; // 0.5 = half speed, 2.0 = double speed
    float duration = context.parameters["duration"];
    
    // Time manipulation is very expensive and destabilizing
    float cost = abs(1.0f - time_factor) * radius * duration * TIME_MANIPULATION_COST;
    float stability_cost = abs(1.0f - time_factor) * TIME_STABILITY_COST;
    
    if (context.world_stability < 0.3f) {
        result.success = false;
        result.failure_reason = "insufficient_stability";
        return result;
    }
    
    // Create temporal field
    WorldNode time_field;
    time_field.type = ENERGY;
    time_field.position = center;
    time_field.properties["time_factor"] = time_factor;
    time_field.properties["radius"] = radius;
    time_field.properties["duration"] = duration;
    time_field.properties["remaining_time"] = duration;
    time_field.stability = 0.2f; // Time fields are very unstable
    
    time_field.update_function = [](WorldNode& field, float dt) {
        field.properties["remaining_time"] -= dt;
        
        if (field.properties["remaining_time"] <= 0) {
            world.removeNode(field.id);
            return;
        }
        
        // Apply time dilation to objects in range
        auto affected_objects = world.getObjectsInRadius(field.position, 
                                                       field.properties["radius"]);
        
        float time_factor = field.properties["time_factor"];
        for (auto& obj : affected_objects) {
            obj.time_scale = time_factor;
            
            // NPCs experience altered time perception
            if (obj.npc_component) {
                obj.npc_component->time_perception *= time_factor;
            }
        }
    };
    
    world.addNode(time_field);
    world.modifyStabilityAt(center, -stability_cost, radius);
    
    result.success = true;
    result.energy_cost = cost;
    result.world_changes.push_back({"temporal_field_created", time_field.id});
    result.world_changes.push_back({"stability_reduced", stability_cost});
    
    return result;
}
```

---

# Advanced Function Composition

## Function Chaining Examples

### Movement Chain: Dash + Phase
```cpp
// Chain: create_force (for momentum) -> transform_matter (player becomes ethereal) -> restore_matter
std::vector<GameFunction> dash_phase_chain = {
    functions["create_force"],
    functions["transform_matter"], 
    functions["restore_matter"]
};

ExecutionContext context;
context.mode = CHAINED;
context.parameters["force_direction"] = player.forward_vector;
context.parameters["force_magnitude"] = 50.0f;
context.parameters["target_material"] = ETHEREAL;
context.parameters["restore_delay"] = 0.5f;

auto result = composer.executeComposition(dash_phase_chain, context);
```

### Combat Chain: Matter + Energy + Space
```cpp
// Chain: create_matter (projectile) -> create_force (launch) -> create_portal (redirect)
std::vector<GameFunction> redirected_projectile = {
    functions["create_matter"],
    functions["create_force"],
    functions["create_portal"]
};

ExecutionContext context;
context.mode = CHAINED;
context.parameters["material_type"] = DENSE_METAL;
context.parameters["amount"] = 1.0f;
context.parameters["force_direction"] = calculateTrajectory(target);
context.parameters["exit_position"] = flanking_position;

auto result = composer.executeComposition(redirected_projectile, context);
```

## Recursive Function Examples

### Fractal Barrier Creation
```cpp
// Recursive: create_barrier that spawns smaller barriers at its corners
GameFunction fractal_barrier = functions["create_barrier"];
fractal_barrier.execute = [](ExecutionContext& context) {
    // Create main barrier
    auto result = create_barrier_base(context);
    
    // If we have energy and haven't reached max recursion depth
    int current_depth = context.parameters["recursion_depth"];
    if (current_depth < 3 && context.available_energy > MIN_RECURSION_ENERGY) {
        // Create smaller barriers at corners
        Vector3 barrier_pos = context.target_position;
        float barrier_size = context.parameters["size"] * 0.6f; // Smaller each level
        
        std::vector<Vector3> corner_positions = calculateCornerPositions(barrier_pos, barrier_size);
        
        for (auto& corner_pos : corner_positions) {
            ExecutionContext recursive_context = context;
            recursive_context.target_position = corner_pos;
            recursive_context.parameters["size"] = barrier_size;
            recursive_context.parameters["recursion_depth"] = current_depth + 1;
            
            // Recursive call
            fractal_barrier.execute(recursive_context);
        }
    }
    
    return result;
};
```

---

# Integration with Game Systems

## Player Function Learning
```cpp
class FunctionDiscovery {
public:
    void onSuccessfulFunctionUse(GameFunction& function, FunctionResult& result) {
        // Player learns about function effectiveness
        function_knowledge[function.name].success_count++;
        function_knowledge[function.name].total_energy_spent += result.energy_cost;
        
        // Discover new composition possibilities
        if (last_used_function != "") {
            auto combination_key = last_used_function + "+" + function.name;
            combination_knowledge[combination_key].discovery_count++;
            
            // Unlock new composite functions when combinations are used enough
            if (combination_knowledge[combination_key].discovery_count >= UNLOCK_THRESHOLD) {
                unlockCompositeFunction(last_used_function, function.name);
            }
        }
        
        last_used_function = function.name;
    }
    
private:
    void unlockCompositeFunction(string func1, string func2) {
        GameFunction composite;
        composite.name = func1 + "_" + func2 + "_composite";
        composite.category = COMPOSITE;
        
        // Create execution that chains the two functions
        composite.execute = [func1, func2](ExecutionContext& context) {
            ExecutionContext chain_context = context;
            chain_context.mode = CHAINED;
            
            std::vector<GameFunction> chain = {
                functions[func1],
                functions[func2]
            };
            
            return composer.executeComposition(chain, chain_context);
        };
        
        // Add to available functions
        available_functions.push_back(composite);
        
        // Notify player of discovery
        ui.showFunctionDiscovered(composite.name);
    }
};
```

This function meta-system creates a deep, explorable space where players can discover emergent combinations and develop their own problem-solving approaches through systematic experimentation with reality manipulation.