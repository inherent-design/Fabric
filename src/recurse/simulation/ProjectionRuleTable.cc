#include "recurse/simulation/ProjectionRuleTable.hh"
#include "recurse/simulation/MaterialRegistry.hh"

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
    for (MaterialId id = 0; id < registry.count(); ++id) {
        if (id >= K_MAX_ESSENCE) {
            continue;
        }
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
}

} // namespace recurse::simulation
