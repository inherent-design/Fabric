#pragma once

#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/VoxelMaterial.hh"

namespace recurse::simulation {

/// True when the cell contains a non-AIR material.
constexpr bool isOccupied(VoxelCell cell) {
    return cell.materialId != material_ids::AIR;
}

/// True when the cell is AIR (empty).
constexpr bool isEmpty(VoxelCell cell) {
    return cell.materialId == material_ids::AIR;
}

/// Returns the cell move phase from the material registry.
inline MoveType cellPhase(const MaterialRegistry& registry, VoxelCell cell) {
    return registry.get(cell.materialId).moveType;
}

/// True when mover can displace target (empty, non-static, or lower density).
inline bool canDisplace(const MaterialRegistry& registry, VoxelCell mover, VoxelCell target) {
    if (isEmpty(target))
        return true;
    const auto& targetDef = registry.get(target.materialId);
    if (targetDef.moveType == MoveType::Static)
        return false;
    const auto& moverDef = registry.get(mover.materialId);
    return moverDef.density > targetDef.density;
}

/// Extract the raw material id from a cell. Quarantines direct field access
/// so the LOD and debug paths route through a single point that changes
/// when Wave 4 swaps the cell layout.
constexpr MaterialId cellMaterialId(VoxelCell cell) {
    return cell.materialId;
}

/// Semantic priority for LOD material reduction. Higher values win tiebreaks
/// during 2x2x2 downsampling. The body stays as-is; quarantined here so
/// consumers do not switch on raw materialId constants directly.
inline int materialSemanticPriority(uint16_t materialId) {
    switch (materialId) {
        case material_ids::SAND:
        case material_ids::GRAVEL:
            return 4;
        case material_ids::WATER:
            return 3;
        case material_ids::DIRT:
            return 2;
        case material_ids::STONE:
            return 1;
        default:
            return 0;
    }
}

/// Merge key for greedy meshing. Adjacent faces with equal keys are merged
/// into a single quad. Initially maps to materialId; Wave 4 swaps to a
/// visual-equivalence hash derived from MatterState fields.
using MergeKey = uint16_t;

/// Sentinel value indicating an empty (unmergeable) face slot in the mask.
inline constexpr MergeKey K_MERGE_KEY_EMPTY = static_cast<MergeKey>(material_ids::AIR);

/// Extract the merge key from a cell for the greedy mesher mask array.
inline MergeKey mergeKey(VoxelCell cell) {
    return cell.materialId;
}

/// True when two adjacent face slots can be merged into a single greedy quad.
inline bool canMergeQuads(MergeKey a, MergeKey b) {
    return a == b;
}

} // namespace recurse::simulation
