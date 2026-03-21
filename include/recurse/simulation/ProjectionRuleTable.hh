#pragma once

#include "recurse/simulation/MatterState.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <array>
#include <cstdint>
#include <string_view>

namespace recurse::simulation {

class MaterialRegistry; // forward declare

/// Projected material properties derived from semantic cell state.
/// Pure downstream cache; never world authority.
/// Note: string_view for displayName means pointed-to strings must outlive the table.
/// For defaults populated from string literals, this is safe.
struct ProjectedMaterial {
    std::string_view displayName{};
    uint32_t baseColor{0};
    uint8_t soundCategory{0};
    uint8_t reductionTiebreak{0};
    MoveType moveType{MoveType::Static};
    uint8_t density{0};
};

static_assert(std::is_trivially_copyable_v<ProjectedMaterial>, "ProjectedMaterial must be trivially copyable");

/// Runtime-configurable projection from semantic cell state to material properties.
/// Maps (essenceIdx, phase) -> ProjectedMaterial.
///
/// Primary lookup keys on essenceIdx * K_PHASE_COUNT + phase.
/// displacementRank-dependent overrides are not part of v1.
class ProjectionRuleTable {
  public:
    static constexpr size_t K_PHASE_COUNT = 8;
    static constexpr size_t K_MAX_ESSENCE = 256;
    static constexpr size_t K_TABLE_SIZE = K_MAX_ESSENCE * K_PHASE_COUNT;

    ProjectionRuleTable();

    /// Look up projected material for the given semantic state.
    const ProjectedMaterial& lookup(uint8_t essenceIdx, Phase phase) const;

    /// Override a single rule entry.
    void setRule(uint8_t essenceIdx, Phase phase, const ProjectedMaterial& mat);

    /// Bulk-populate from existing MaterialRegistry.
    /// Maps each registered MaterialId to its corresponding essenceIdx slot.
    /// This preserves identical behavior during migration.
    void populateFromRegistry(const MaterialRegistry& registry);

  private:
    size_t index(uint8_t essenceIdx, Phase phase) const;
    std::array<ProjectedMaterial, K_TABLE_SIZE> table_{};
};

} // namespace recurse::simulation
