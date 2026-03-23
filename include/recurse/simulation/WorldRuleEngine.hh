#pragma once
#include "recurse/simulation/VoxelMaterial.hh"
#include <cstdint>
#include <span>
#include <vector>

namespace recurse::simulation {

/// Flat 14-byte rule for world simulation behaviors.
/// Locked design: Decision 2 (2026-03-22). No nested types.
/// Tag is plain uint8_t for debugging (0=Thermal, 1=Contact, 2=Gravity).
struct WorldRule {
    uint8_t essenceIdxA; // self (255 = wildcard)
    uint8_t essenceIdxB; // neighbor (255 = self-transform)
    Phase requiredPhaseA{Phase::Empty};
    uint8_t temperatureMin{0};
    uint8_t temperatureMax{255};
    uint8_t resultEssenceA{255}; // 255 = unchanged
    uint8_t resultEssenceB{255}; // 255 = unchanged
    Phase resultPhaseA{Phase::Empty};
    Phase resultPhaseB{Phase::Empty};
    uint8_t resultTempA{0};   // 0 = unchanged
    uint8_t resultTempB{0};   // 0 = unchanged
    uint8_t probability{255}; // 0-255, 255 = always fire
    uint8_t priority{128};    // higher = evaluated first
    uint8_t tag{0};           // system origin for debugging
};
static_assert(sizeof(WorldRule) == 14, "WorldRule must be exactly 14 bytes");

/// Configurable max transformations per chunk per tick.
inline constexpr int K_MAX_TRANSFORMS_PER_CHUNK = 64;

class WorldRuleEngine {
  public:
    WorldRuleEngine();

    /// Add a rule to the engine. Rules are kept sorted by priority (descending).
    void addRule(WorldRule rule);

    /// Query matching rules for a cell pair at a given temperature.
    /// Returns a span of matching rules sorted by priority (highest first).
    /// selfEssence: essenceIdx of the cell being evaluated
    /// neighborEssence: essenceIdx of the adjacent cell (255 for self-transform queries)
    /// selfPhase: phase of the cell being evaluated
    /// temperature: temperature byte of the cell
    std::span<const WorldRule> query(uint8_t selfEssence, uint8_t neighborEssence, Phase selfPhase,
                                     uint8_t temperature) const;

    /// Number of registered rules.
    size_t ruleCount() const;

    /// Access to all rules (for testing/debugging).
    std::span<const WorldRule> allRules() const;

  private:
    std::vector<WorldRule> rules_;

    /// Scratch buffer for query results (avoids allocation per query).
    /// Mutable because query() is const but needs to fill this.
    mutable std::vector<WorldRule> queryResults_;

    void sortByPriority();
    bool matches(const WorldRule& rule, uint8_t selfEssence, uint8_t neighborEssence, Phase selfPhase,
                 uint8_t temperature) const;
};

} // namespace recurse::simulation
