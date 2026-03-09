#pragma once

// Recurse Essence Types
// Semantic meaning for essence Vec4 components.
// Each NPC and voxel has an essence [Order, Chaos, Life, Decay].
// The dominant essence determines behavior, appearance, and transformation rules.

#include <array>
#include <cmath>
#include <cstdint>

namespace recurse {

/// The four essence types that define entity/material nature.
/// Values map to Vec4 components: [Order=0, Chaos=1, Life=2, Decay=3]
enum class EssenceType : uint8_t {
    Order = 0, // Structure, stability, stone/metal
    Chaos = 1, // Entropy, fire, demons
    Life = 2,  // Growth, healing, nature
    Decay = 3, // Death, corruption, undead
    None = 255 // No dominant essence (empty/balanced)
};

/// Essence vector: [Order, Chaos, Life, Decay]
/// Each component ranges 0.0-1.0, typically sums to ~1.0
struct Essence {
    float order;
    float chaos;
    float life;
    float decay;

    /// Create from raw array
    static Essence fromArray(const float* arr) { return {arr[0], arr[1], arr[2], arr[3]}; }

    /// Convert to raw array
    std::array<float, 4> toArray() const { return {order, chaos, life, decay}; }

    /// Get component by type
    float get(EssenceType type) const {
        switch (type) {
            case EssenceType::Order:
                return order;
            case EssenceType::Chaos:
                return chaos;
            case EssenceType::Life:
                return life;
            case EssenceType::Decay:
                return decay;
            default:
                return 0.0f;
        }
    }

    /// Set component by type
    void set(EssenceType type, float value) {
        switch (type) {
            case EssenceType::Order:
                order = value;
                break;
            case EssenceType::Chaos:
                chaos = value;
                break;
            case EssenceType::Life:
                life = value;
                break;
            case EssenceType::Decay:
                decay = value;
                break;
            default:
                break;
        }
    }

    /// Compute dominant essence type.
    /// Returns None if all values are below threshold.
    EssenceType dominant(float threshold = 0.3f) const {
        EssenceType best = EssenceType::None;
        float bestValue = threshold;

        if (order > bestValue) {
            best = EssenceType::Order;
            bestValue = order;
        }
        if (chaos > bestValue) {
            best = EssenceType::Chaos;
            bestValue = chaos;
        }
        if (life > bestValue) {
            best = EssenceType::Life;
            bestValue = life;
        }
        if (decay > bestValue) {
            best = EssenceType::Decay;
            bestValue = decay;
        }

        return best;
    }

    /// Check if this essence matches a pattern within tolerance
    bool matches(const Essence& pattern, float tolerance = 0.2f) const {
        return std::abs(order - pattern.order) <= tolerance && std::abs(chaos - pattern.chaos) <= tolerance &&
               std::abs(life - pattern.life) <= tolerance && std::abs(decay - pattern.decay) <= tolerance;
    }

    /// Blend this essence with another
    Essence blend(const Essence& other, float weight = 0.5f) const {
        return {order * (1 - weight) + other.order * weight, chaos * (1 - weight) + other.chaos * weight,
                life * (1 - weight) + other.life * weight, decay * (1 - weight) + other.decay * weight};
    }

    /// Create pure essence of a single type
    static Essence pure(EssenceType type) {
        Essence e{0, 0, 0, 0};
        e.set(type, 1.0f);
        return e;
    }

    // Common essence presets
    static Essence pureOrder() { return {1, 0, 0, 0}; }
    static Essence pureChaos() { return {0, 1, 0, 0}; }
    static Essence pureLife() { return {0, 0, 1, 0}; }
    static Essence pureDecay() { return {0, 0, 0, 1}; }
    static Essence neutral() { return {0.25f, 0.25f, 0.25f, 0.25f}; }
};

/// NPC archetype definitions with essence ranges
/// From godot-projects/recurse/docs/NPCs.md
struct NPCArchetype {
    const char* name;
    Essence essenceBase;  // Base essence for this archetype
    Essence essenceRange; // Allowed variance from base

    // Visual properties
    uint32_t colorPrimary;
    uint32_t colorSecondary;

    // Stats
    float baseHealth;
    float moveSpeed;

    /// Check if an essence matches this archetype
    bool matchesEssence(const Essence& e) const { return e.matches(essenceBase, essenceRange.order); }
};

/// Built-in NPC archetypes from game design
namespace Archetypes {
// Stone Golem: [0.8, 0, 0, 0.2] -> Transforms to Treant with LIFE
extern const NPCArchetype StoneGolem;

// Treant: [0.2, 0, 0.8, 0] -> Transforms to Stone Golem with ORDER
extern const NPCArchetype Treant;

// Chaos Imp: [0, 0.9, 0, 0.1] -> Transforms to Iron Sentinel with ORDER
extern const NPCArchetype ChaosImp;

// Iron Sentinel: [0.9, 0, 0, 0.1] -> Transforms to Chaos Imp with CHAOS
extern const NPCArchetype IronSentinel;

// Shambler: [0, 0.5, 0, 0.5] -> Transforms to Human Villager with LIFE
extern const NPCArchetype Shambler;

// Human Villager: [0.1, 0, 0.7, 0] -> Transforms to Shambler with DECAY
extern const NPCArchetype HumanVillager;

// Fire Elemental: [0, 0.7, 0.3, 0] -> Transforms to Cooled Magma with DECELERATE
extern const NPCArchetype FireElemental;

// Cooled Magma: [0.3, 0, 0, 0] -> Transforms to Fire Elemental with ACCELERATE
extern const NPCArchetype CooledMagma;

/// Get archetype by name
const NPCArchetype* byName(const char* name);

/// Find archetype that matches an essence
const NPCArchetype* byEssence(const Essence& e);
} // namespace Archetypes

} // namespace recurse
