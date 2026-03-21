#pragma once

#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/MatterState.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <concepts>

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

// -- MatterState overloads --------------------------------------------------

/// True when the MatterState cell is occupied (not Empty phase).
constexpr bool isOccupied(MatterState cell) {
    return cell.phase() != Phase::Empty;
}

/// True when the MatterState cell is empty.
constexpr bool isEmpty(MatterState cell) {
    return cell.phase() == Phase::Empty;
}

/// Returns the cell phase from MatterState directly. Registry parameter is
/// accepted for concept satisfaction but unused; MatterState carries its phase.
inline MoveType cellPhase(const MaterialRegistry& /*registry*/, MatterState cell) {
    switch (cell.phase()) {
        case Phase::Solid:
            return MoveType::Static;
        case Phase::Powder:
            return MoveType::Powder;
        case Phase::Liquid:
        case Phase::Gas:
            return MoveType::Liquid; // Gas placeholder until MoveType::Gas exists
        default:
            return MoveType::Static;
    }
}

/// Displacement check for MatterState cells. Uses displacementRank directly.
inline bool canDisplace(const MaterialRegistry& /*registry*/, MatterState mover, MatterState target) {
    if (isEmpty(target))
        return true;
    if (target.phase() == Phase::Solid)
        return false;
    return mover.displacementRank > target.displacementRank;
}

// -- Cell concepts ----------------------------------------------------------

/// A cell type that supports occupancy queries without external context.
template <typename T>
concept CellQuery = requires(T cell) {
    { isOccupied(cell) } -> std::convertible_to<bool>;
    { isEmpty(cell) } -> std::convertible_to<bool>;
};

/// A cell type that supports semantic queries requiring registry context.
template <typename T>
concept SemanticQuery = CellQuery<T> && requires(const MaterialRegistry& reg, T cell, T other) {
    { cellPhase(reg, cell) } -> std::convertible_to<MoveType>;
    { canDisplace(reg, cell, other) } -> std::convertible_to<bool>;
};

static_assert(CellQuery<VoxelCell>, "VoxelCell must satisfy CellQuery");
static_assert(CellQuery<MatterState>, "MatterState must satisfy CellQuery");
static_assert(SemanticQuery<VoxelCell>, "VoxelCell must satisfy SemanticQuery");
static_assert(SemanticQuery<MatterState>, "MatterState must satisfy SemanticQuery");

// ---------------------------------------------------------------------------

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
