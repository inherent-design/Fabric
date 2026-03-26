#pragma once
#include "recurse/simulation/VoxelMaterial.hh"
#include <cstdint>
#include <span>
#include <vector>

namespace recurse::simulation {

/// Tag bits for WorldRule::tag. Low 3 bits encode system origin; bit 7 encodes result type.
namespace RuleTag {
inline constexpr uint8_t kThermal = 0;
inline constexpr uint8_t kContact = 1;
inline constexpr uint8_t kGravity = 2;
inline constexpr uint8_t kSystemMask = 0x07;
inline constexpr uint8_t kSwap = 0x80;
} // namespace RuleTag

/// Flat 14-byte rule for world simulation behaviors.
/// Locked design: Decision 2 (2026-03-22). No nested types.
/// Tag encodes system origin (low 3 bits: 0=Thermal, 1=Contact, 2=Gravity)
/// and result type (bit 7 set = SWAP, clear = WRITE).
///
/// Sentinel conventions for result fields:
///   resultEssenceA/B = 255: leave essence unchanged
///   resultPhaseA/B = Phase::Unchanged: leave phase unchanged
///   resultTempA/B = 0: leave temperature unchanged (temp cannot be set to literal zero)
struct WorldRule {
    uint8_t essenceIdxA; // self (255 = wildcard)
    uint8_t essenceIdxB; // neighbor (255 = self-transform)
    Phase requiredPhaseA{Phase::Unchanged};
    uint8_t temperatureMin{0};
    uint8_t temperatureMax{255};
    uint8_t resultEssenceA{255}; // 255 = unchanged
    uint8_t resultEssenceB{255}; // 255 = unchanged
    Phase resultPhaseA{Phase::Unchanged};
    Phase resultPhaseB{Phase::Unchanged};
    uint8_t resultTempA{0};   // 0 = unchanged
    uint8_t resultTempB{0};   // 0 = unchanged
    uint8_t probability{255}; // 0-255, 255 = always fire
    uint8_t priority{128};    // higher = evaluated first
    uint8_t tag{0};           // system origin + result type flags

    bool isSwap() const { return (tag & RuleTag::kSwap) != 0; }
    uint8_t systemTag() const { return tag & RuleTag::kSystemMask; }
};
static_assert(sizeof(WorldRule) == 14, "WorldRule must be exactly 14 bytes");

/// Configurable max transformations per chunk per tick.
inline constexpr int K_MAX_TRANSFORMS_PER_CHUNK = 64;

class WorldRuleEngine {
  public:
    WorldRuleEngine();

    /// Add a rule to the engine. Rules are kept sorted by priority (descending).
    void addRule(WorldRule rule);

    /// Query matching WRITE rules for a cell pair at a given temperature.
    /// Appends matching rules (sorted by priority, highest first) to caller-owned buffer.
    /// Skips SWAP-tagged rules; use queryGravity for movement.
    void query(uint8_t selfEssence, uint8_t neighborEssence, Phase selfPhase, uint8_t temperature,
               std::vector<WorldRule>& results) const;

    /// Query matching gravity SWAP rules for a cell by self-phase.
    /// Returns rules sorted by priority (highest first). The caller checks
    /// neighbor emptiness and displacement rank at evaluation time.
    void queryGravity(Phase selfPhase, std::vector<WorldRule>& results) const;

    /// Number of registered rules.
    size_t ruleCount() const;

    /// Access to all rules (for testing/debugging).
    std::span<const WorldRule> allRules() const;

  private:
    std::vector<WorldRule> rules_;

    void sortByPriority();
    bool matches(const WorldRule& rule, uint8_t selfEssence, uint8_t neighborEssence, Phase selfPhase,
                 uint8_t temperature) const;
};

} // namespace recurse::simulation
