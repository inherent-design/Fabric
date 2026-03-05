#include "recurse/components/EssenceTypes.hh"

#include <cstring>

namespace recurse {
namespace Archetypes {

// Archetype definitions from NPCs.md
const NPCArchetype StoneGolem = {
    "Stone Golem",
    {0.8f, 0.0f, 0.0f, 0.2f},  // Base essence
    {0.15f, 0.1f, 0.1f, 0.1f}, // Range
    0x808080,
    0x606060, // Gray stone colors
    150.0f,
    2.0f // Health, speed
};

const NPCArchetype Treant = {"Treant",
                             {0.2f, 0.0f, 0.8f, 0.0f},
                             {0.1f, 0.1f, 0.15f, 0.1f},
                             0x228B22,
                             0x006400, // Forest green colors
                             100.0f,
                             1.5f};

const NPCArchetype ChaosImp = {
    "Chaos Imp",
    {0.0f, 0.9f, 0.0f, 0.1f},
    {0.1f, 0.15f, 0.1f, 0.1f},
    0xFF4500,
    0x8B0000, // Red/orange demon colors
    50.0f,
    5.0f // Fast but weak
};

const NPCArchetype IronSentinel = {
    "Iron Sentinel",
    {0.9f, 0.0f, 0.0f, 0.1f},
    {0.15f, 0.1f, 0.1f, 0.1f},
    0x708090,
    0x2F4F4F, // Slate gray metal
    200.0f,
    0.0f // Stationary turret
};

const NPCArchetype Shambler = {"Shambler",
                               {0.0f, 0.5f, 0.0f, 0.5f},
                               {0.1f, 0.15f, 0.1f, 0.15f},
                               0x556B2F,
                               0x3C3C3C, // Rotting green/gray
                               75.0f,
                               1.0f};

const NPCArchetype HumanVillager = {"Human Villager",
                                    {0.1f, 0.0f, 0.7f, 0.0f},
                                    {0.1f, 0.1f, 0.15f, 0.1f},
                                    0xDEB887,
                                    0x8B4513, // Skin/cloth tones
                                    50.0f,
                                    3.0f};

const NPCArchetype FireElemental = {"Fire Elemental",
                                    {0.0f, 0.7f, 0.3f, 0.0f},
                                    {0.1f, 0.1f, 0.1f, 0.1f},
                                    0xFF6600,
                                    0xFFCC00, // Orange/yellow fire
                                    80.0f,
                                    4.0f};

const NPCArchetype CooledMagma = {
    "Cooled Magma",
    {0.3f, 0.0f, 0.0f, 0.0f},
    {0.1f, 0.1f, 0.1f, 0.1f},
    0x2F2F2F,
    0x1A1A1A, // Dark cooled rock
    0.0f,
    0.0f // Inactive terrain
};

const NPCArchetype* byName(const char* name) {
    if (name == nullptr)
        return nullptr;

    for (const auto* arch :
         {&StoneGolem, &Treant, &ChaosImp, &IronSentinel, &Shambler, &HumanVillager, &FireElemental, &CooledMagma}) {
        if (strcmp(arch->name, name) == 0) {
            return arch;
        }
    }
    return nullptr;
}

const NPCArchetype* byEssence(const Essence& e) {
    const NPCArchetype* best = nullptr;
    float bestDistance = 1000.0f; // Start with large distance, find minimum

    for (const auto* arch :
         {&StoneGolem, &Treant, &ChaosImp, &IronSentinel, &Shambler, &HumanVillager, &FireElemental, &CooledMagma}) {
        if (arch->matchesEssence(e)) {
            // Score by Euclidean distance to base (lower is better)
            float dOrder = e.order - arch->essenceBase.order;
            float dChaos = e.chaos - arch->essenceBase.chaos;
            float dLife = e.life - arch->essenceBase.life;
            float dDecay = e.decay - arch->essenceBase.decay;
            float distance = dOrder * dOrder + dChaos * dChaos + dLife * dLife + dDecay * dDecay;

            if (distance < bestDistance) {
                bestDistance = distance;
                best = arch;
            }
        }
    }
    return best;
}

} // namespace Archetypes
} // namespace recurse
