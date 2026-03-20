# NPC System Design - RECURSE
*Emergent Social Dynamics & Behavioral AI*

## Core Philosophy

NPCs are autonomous agents with agency, not scripted entities. They form dynamic relationships, make survival decisions, and create emergent narratives through behavioral interactions. The system prioritizes **behavioral authenticity** over narrative predictability.

---

# NPC Consciousness Model

## Core Stats System
Every NPC has four fundamental dimensions that drive all behavior:

```cpp
struct NPCCore {
    float fear_courage;      // -1.0 to 1.0 (coward to fearless)
    float empathy_apathy;    // -1.0 to 1.0 (cruel to compassionate)
    float health_damage;     // 0.0 to 1.0 (dying to peak condition)
    float meta_capability;   // 0.0 to 1.0 (NPC-like to player-like)
};
```

### Fear/Courage (-1.0 to 1.0)
- **< -0.5**: Panic-prone, flees from minor threats
- **-0.5 to 0.0**: Cautious, avoids confrontation  
- **0.0 to 0.5**: Measured, will fight if necessary
- **> 0.5**: Aggressive, seeks confrontation

### Empathy/Apathy (-1.0 to 1.0)
- **< -0.5**: Actively cruel, enjoys others' suffering
- **-0.5 to 0.0**: Selfish, ignores others' needs
- **0.0 to 0.5**: Helpful when convenient
- **> 0.5**: Altruistic, risks self for others

### Health/Damage (0.0 to 1.0)
- **< 0.3**: Critically injured, desperate behavior
- **0.3 to 0.7**: Wounded, cautious but functional
- **> 0.7**: Healthy, confident behavior

### Meta-Capability (0.0 to 1.0)
- **< 0.2**: Basic NPC, no function usage
- **0.2 to 0.5**: Limited function usage (basic matter manipulation)
- **0.5 to 0.8**: Advanced functions (time manipulation, barriers)
- **> 0.8**: Near-player level (reality composition, dimensional manipulation)

---

# Behavioral State Machine

## Primary States
```cpp
enum NPCState {
    IDLE,           // Default wandering/activity
    AWARE,          // Noticed player, assessing
    COOPERATIVE,    // Helping player/allies
    HOSTILE,        // Actively fighting
    FLEEING,        // Running from danger
    TRIBAL,         // Coordinating with faction
    MIMICKING,      // Pretending (lizard people)
    INVESTIGATING   // FBI agents examining anomalies
};
```

## Decision Framework
Each NPC continuously evaluates:

### Threat Assessment
```cpp
float assessPlayerThreat(Player& player) {
    float base_threat = 0.0f;
    
    // Recent player actions
    for (auto& memory : recent_memories) {
        if (memory.player_killed_npc) base_threat += 0.8f;
        if (memory.player_helped_npc) base_threat -= 0.3f;
        if (memory.player_used_high_function) base_threat += 0.5f;
    }
    
    // Player's current function usage
    base_threat += player.active_function_level * 0.4f;
    
    // Distance-based threat decay
    float distance = length(position - player.position);
    return base_threat * (1.0f / (1.0f + distance * 0.1f));
}
```

### Survival Calculus
```cpp
float calculateActionUtility(ActionType action) {
    switch (action) {
        case FIGHT:
            return (health_damage * fear_courage * combat_confidence) - threat_level;
        case FLEE:
            return (threat_level * (1.0f - fear_courage)) + survival_instinct;
        case COOPERATE:
            return (empathy_apathy * alliance_benefit) - personal_cost;
        case TRIBAL:
            return faction_loyalty * group_strength * shared_threat;
    }
}
```

---

# Faction System

## Faction Types

### CIVILIAN (Default)
- **Behavior**: Varied based on individual stats
- **Goals**: Personal survival, protecting family/friends
- **Triggers**: Respond to immediate threats
- **Special**: Can form spontaneous alliances

### FBI_AGENTS
- **Behavior**: Professional, coordinated, investigative
- **Goals**: Investigate reality anomalies, maintain order
- **Triggers**: Spawn when player uses high-level functions (>0.7 meta level)
- **Special**: Share intelligence network-wide, non-hostile unless provoked

### TERRIES (Hostile FBI Variants)
- **Behavior**: Aggressive hunters, coordinated pack tactics
- **Goals**: Eliminate player as reality threat
- **Triggers**: Player kills FBI agents or uses reality-breaking functions
- **Special**: Predictive movement, pincer attacks, high meta-capability

### LIZARD_MEN
- **Behavior**: Infiltrators, patient ambush predators
- **Goals**: Blend in until optimal strike opportunity
- **Triggers**: Random spawns, replace civilians dynamically
- **Special**: Perfect mimicry until revealed, then superior combat stats

### HOMELESS_TRIBE
- **Behavior**: Cooperative survival, resource sharing
- **Goals**: Group survival, mutual aid
- **Triggers**: Spawn in groups, high empathy stats
- **Special**: Excellent scavenging, will trade with player

### KAREN_COLLECTIVE
- **Behavior**: Middle-class enforcers, territorial
- **Goals**: Maintain social order, eliminate "undesirables"
- **Triggers**: Player disrupts "normal" areas
- **Special**: High persistence, calls for backup, bureaucratic warfare

---

# Behavioral Emergence Systems

## Panic Propagation
```cpp
void updatePanicSpread(std::vector<NPC>& nearby_npcs) {
    for (auto& panicked_npc : npcs_in_state(FLEEING)) {
        for (auto& other_npc : nearby_npcs) {
            float distance = length(panicked_npc.pos - other_npc.pos);
            if (distance < PANIC_SPREAD_RADIUS) {
                float influence = other_npc.empathy_apathy * PANIC_SUSCEPTIBILITY;
                influence *= (1.0f - other_npc.fear_courage); // Fearful NPCs more susceptible
                
                if (influence > PANIC_THRESHOLD) {
                    other_npc.addEmotionalState(FEAR, influence * PANIC_STRENGTH);
                }
            }
        }
    }
}
```

## Spontaneous Alliance Formation
```cpp
void evaluateAllianceFormation(NPC& npc1, NPC& npc2) {
    // Shared threat assessment
    float threat_similarity = 1.0f - abs(npc1.threat_level - npc2.threat_level);
    
    // Empathy compatibility
    float empathy_match = min(npc1.empathy_apathy, npc2.empathy_apathy);
    
    // Proximity and timing
    float proximity_factor = 1.0f / (1.0f + distance(npc1.pos, npc2.pos));
    
    float alliance_probability = threat_similarity * empathy_match * proximity_factor;
    
    if (alliance_probability > ALLIANCE_THRESHOLD) {
        formTemporaryAlliance(npc1, npc2);
    }
}
```

## Collective Intelligence
High meta-capability NPCs (>0.6) exhibit group problem-solving:

```cpp
void updateCollectiveIntelligence(std::vector<NPC*>& smart_npcs, Player& player) {
    if (smart_npcs.size() < 3) return;
    
    // Coordinate complex strategies
    Vector3 predicted_player_pos = predictPlayerMovement(player);
    
    // Assign specialized roles
    for (int i = 0; i < smart_npcs.size(); ++i) {
        switch (i % 4) {
            case 0: // Herder - push player toward trap
                smart_npcs[i]->setRole(HERD_TARGET, predicted_player_pos);
                break;
            case 1: // Blocker - cut off escape routes
                smart_npcs[i]->setRole(BLOCK_ESCAPE, calculateEscapeRoutes(player));
                break;
            case 2: // Supporter - heal/buff allies
                smart_npcs[i]->setRole(SUPPORT_ALLIES, getAllyCenter(smart_npcs));
                break;
            case 3: // Disruptor - counter player functions
                smart_npcs[i]->setRole(COUNTER_FUNCTIONS, player.position);
                break;
        }
    }
}
```

---

# Memory & Learning System

## Memory Types
```cpp
struct NPCMemory {
    enum Type {
        PLAYER_ENCOUNTER,
        WITNESSED_VIOLENCE,
        RECEIVED_HELP,
        ALLY_DEATH,
        FUNCTION_USAGE,
        ENVIRONMENTAL_CHANGE
    };
    
    Type type;
    Vector3 location;
    float timestamp;
    float emotional_intensity;
    float recency_weight;  // Decays over time
};
```

## Information Sharing
```cpp
void shareMemories(NPC& source, NPC& target) {
    if (source.faction != target.faction && !source.isAlliedWith(target)) return;
    
    // Share high-intensity memories
    for (auto& memory : source.memories) {
        if (memory.emotional_intensity > SHARE_THRESHOLD) {
            target.addSecondhandMemory(memory, source.trust_level);
        }
    }
    
    // Update threat assessments based on shared intelligence
    target.updateThreatAssessment(source.player_threat_level);
}
```

---

# Meta-Capability System

## Function Usage by NPCs
High meta-capability NPCs can use simplified versions of player functions:

### Basic Functions (0.2-0.5 meta-capability)
- **Create Matter**: Small objects, basic weapons
- **Kinetic Push**: Knockback effects, environmental interaction
- **Barrier Creation**: Temporary cover, path blocking

### Advanced Functions (0.5-0.8 meta-capability)
- **Time Dilation**: Slow local time for tactical advantage
- **Portal Creation**: Short-range teleportation for escape/flanking
- **Matter Transformation**: Change environmental materials
- **Healing Generation**: Create health items for allies

### Master Functions (0.8+ meta-capability)
- **Reality Composition**: Limited reality manipulation
- **Dimensional Manipulation**: Create pocket spaces
- **Probability Alteration**: Influence random events
- **Causal Interference**: Minor time paradox creation

## Function Usage AI
```cpp
void evaluateFunctionUsage(NPC& npc, GameState& world) {
    if (npc.meta_capability < 0.2f) return;
    
    // Assess current situation needs
    float threat_level = npc.assessCurrentThreat();
    float ally_need = npc.assessAllyNeeds();
    float tactical_advantage = npc.assessTacticalOpportunity();
    
    // Select optimal function based on situation
    if (threat_level > 0.8f && npc.canEscape()) {
        npc.useFunction("create_escape_portal");
    } else if (ally_need > 0.6f && npc.meta_capability > 0.5f) {
        npc.useFunction("generate_healing_item", npc.getAllyCenter());
    } else if (tactical_advantage > 0.7f) {
        npc.useFunction("create_matter_barrier", npc.getOptimalBarrierPosition());
    }
}
```

---

# Performance Optimization

## Level of Detail (LOD) System
```cpp
void updateNPCLOD(NPC& npc, Player& player) {
    float distance = length(npc.position - player.position);
    
    if (distance < CLOSE_RANGE) {
        npc.ai_update_rate = 60;  // Full AI updates
        npc.behavior_complexity = FULL;
    } else if (distance < MEDIUM_RANGE) {
        npc.ai_update_rate = 30;  // Half rate updates
        npc.behavior_complexity = SIMPLIFIED;
    } else {
        npc.ai_update_rate = 10;  // Minimal updates
        npc.behavior_complexity = BASIC;
    }
}
```

## Behavioral Culling
- NPCs beyond render distance use simplified state machines
- Memory systems limited to essential recent events
- Faction coordination reduced to key representatives
- Function usage limited to visible NPCs

---

# Integration with Game Systems

## Player Function Detection
```cpp
void onPlayerFunctionUsed(FunctionType function, float power_level) {
    // Notify nearby NPCs
    for (auto& npc : getNPCsInRange(player.position, FUNCTION_DETECTION_RANGE)) {
        npc.addMemory(FUNCTION_USAGE, player.position, power_level);
        
        // High-level functions trigger FBI response
        if (power_level > 0.7f && npc.faction != FBI_AGENTS) {
            spawnFBIResponse(player.position);
        }
    }
}
```

## Environmental Reaction
```cpp
void onEnvironmentalChange(Vector3 location, ChangeType change) {
    for (auto& npc : getNPCsInRange(location, ENVIRONMENT_AWARENESS_RANGE)) {
        npc.addMemory(ENVIRONMENTAL_CHANGE, location, getChangeIntensity(change));
        
        // Some NPCs investigate changes
        if (npc.meta_capability > 0.4f && npc.getCurrentState() == IDLE) {
            npc.setState(INVESTIGATING);
            npc.setInvestigationTarget(location);
        }
    }
}
```

This NPC system creates a living, breathing world where agents have genuine agency and create emergent narratives through their interactions with each other and the player. The behavior emerges from simple rules but creates complex, unpredictable social dynamics.