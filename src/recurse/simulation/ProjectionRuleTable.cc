#include "recurse/simulation/ProjectionRuleTable.hh"
#include "recurse/simulation/MaterialRegistry.hh"

#include <algorithm>

namespace recurse::simulation {

ProjectionRuleTable::ProjectionRuleTable() = default;

size_t ProjectionRuleTable::index(uint8_t essenceIdx, Phase phase) const {
    return static_cast<size_t>(essenceIdx) * K_PHASE_COUNT + static_cast<size_t>(static_cast<uint8_t>(phase));
}

const ProjectedMaterial& ProjectionRuleTable::lookup(uint8_t essenceIdx, Phase phase) const {
    return table_[index(essenceIdx, phase)];
}

void ProjectionRuleTable::setRule(uint8_t essenceIdx, Phase phase, const ProjectedMaterial& mat) {
    table_[index(essenceIdx, phase)] = mat;
}

void ProjectionRuleTable::populateFromRegistry(const MaterialRegistry& registry) {
    // During migration, MaterialId maps 1:1 to essenceIdx for registered materials.
    const MaterialId limit = std::min(registry.count(), static_cast<MaterialId>(K_MAX_ESSENCE));
    for (MaterialId id = 0; id < limit; ++id) {
        const auto& def = registry.get(id);

        // Derive phase from MoveType (mirrors CellAccessors.hh cellPhase logic)
        Phase phase = Phase::Empty;
        if (id == material_ids::AIR) {
            phase = Phase::Empty;
        } else {
            switch (def.moveType) {
                case MoveType::Static:
                    phase = Phase::Solid;
                    break;
                case MoveType::Powder:
                    phase = Phase::Powder;
                    break;
                case MoveType::Liquid:
                    phase = Phase::Liquid;
                    break;
                case MoveType::Gas:
                    phase = Phase::Gas;
                    break;
                default:
                    phase = Phase::Solid;
                    break;
            }
        }

        ProjectedMaterial projected;
        projected.baseColor = def.baseColor;
        projected.moveType = def.moveType;
        projected.density = def.density;
        projected.reductionTiebreak = def.density;
        // displayName left empty for v1; B8 (debug/WAILA) will wire names.
        projected.displayName = {};

        setRule(static_cast<uint8_t>(id), phase, projected);
    }

    // Projected appearances for transformation-produced materials.
    // These essenceIdx values are outside material_ids::COUNT (6) but referenced
    // by WorldRuleEngine rules as transformation products.

    // ICE (essenceIdx=6): produced by water freeze rule (R1)
    setRule(6, Phase::Solid,
            ProjectedMaterial{.displayName = "ice",
                              .baseColor = 0xFFD0E8FF,
                              .soundCategory = 0,
                              .reductionTiebreak = 110,
                              .moveType = MoveType::Static,
                              .density = 90});

    // GLASS (essenceIdx=10): produced by sand vitrify rule (R4)
    setRule(10, Phase::Solid,
            ProjectedMaterial{.displayName = "glass",
                              .baseColor = 0xFFA08040,
                              .soundCategory = 0,
                              .reductionTiebreak = 130,
                              .moveType = MoveType::Static,
                              .density = 140});

    // MAGMA (essenceIdx=11): produced by stone melt rule (R5)
    setRule(11, Phase::Liquid,
            ProjectedMaterial{.displayName = "magma",
                              .baseColor = 0xFFFF4400,
                              .soundCategory = 0,
                              .reductionTiebreak = 200,
                              .moveType = MoveType::Liquid,
                              .density = 190});
}

} // namespace recurse::simulation
